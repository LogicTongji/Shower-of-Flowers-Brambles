#include "new_core_launcher_core.h"

#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>
#include <vector>

namespace new_core
{
namespace
{

constexpr wchar_t LauncherSection[] = L"launcher";

std::wstring QuoteArgument(std::wstring_view value)
{
    if (value.find_first_of(L" \t\n\v\"")
        == std::wstring_view::npos)
    {
        return std::wstring(value);
    }
    std::wstring output = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t character : value)
    {
        if (character == L'\\')
        {
            ++backslashes;
            continue;
        }
        if (character == L'\"')
        {
            output.append(backslashes * 2 + 1, L'\\');
            output.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        output.append(backslashes, L'\\');
        backslashes = 0;
        output.push_back(character);
    }
    output.append(backslashes * 2, L'\\');
    output.push_back(L'\"');
    return output;
}

std::wstring ReadIniString(
    const std::filesystem::path& path,
    const wchar_t* key,
    std::wstring_view fallback
)
{
    std::array<wchar_t, 32768> value{};
    GetPrivateProfileStringW(
        LauncherSection,
        key,
        std::wstring(fallback).c_str(),
        value.data(),
        static_cast<DWORD>(value.size()),
        path.wstring().c_str()
    );
    return value.data();
}

bool WriteIniString(
    const std::filesystem::path& path,
    const wchar_t* key,
    std::wstring_view value
)
{
    const std::wstring owned(value);
    return WritePrivateProfileStringW(
        LauncherSection,
        key,
        owned.c_str(),
        path.wstring().c_str()
    ) != FALSE;
}

std::filesystem::path FindProjectRoot(
    std::filesystem::path candidate
)
{
    for (int depth = 0; depth < 8 && !candidate.empty(); ++depth)
    {
        if (std::filesystem::is_directory(
                candidate / "interface" / "gui_plugins"
            )
            && std::filesystem::is_directory(
                candidate / "new_core"
            ))
        {
            return std::filesystem::absolute(candidate)
                .lexically_normal();
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
    return {};
}

std::filesystem::path EnvironmentPath(const wchar_t* name)
{
    std::array<wchar_t, 32768> value{};
    const DWORD length = GetEnvironmentVariableW(
        name,
        value.data(),
        static_cast<DWORD>(value.size())
    );
    return length > 0 && length < value.size()
        ? std::filesystem::path(value.data())
        : std::filesystem::path{};
}

std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty())
    {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (length <= 0)
    {
        return L"New Core returned an unreadable message";
    }
    std::wstring output(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        length
    );
    return output;
}

bool IsGameAlreadyRunning(const std::filesystem::path& executable)
{
    const std::wstring target = executable.filename().wstring();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, target.c_str()) == 0)
            {
                found = true;
                break;
            }
        }
        while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

bool HasNewCoreExports(const std::filesystem::path& library)
{
    HMODULE module = LoadLibraryExW(
        library.wstring().c_str(),
        nullptr,
        DONT_RESOLVE_DLL_REFERENCES
    );
    if (!module)
    {
        return false;
    }
    const bool valid = GetProcAddress(
            module,
            "NewCore_GetAbiVersion"
        )
        && GetProcAddress(module, "NewCore_GetModuleIds")
        && GetProcAddress(module, "NewCore_GetHookStatuses");
    FreeLibrary(module);
    return valid;
}

bool InjectLibrary(
    HANDLE process,
    const std::filesystem::path& library,
    std::wstring& error
)
{
    const std::wstring path = std::filesystem::absolute(library)
        .lexically_normal().wstring();
    const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remotePath = VirtualAllocEx(
        process,
        nullptr,
        bytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (!remotePath)
    {
        error = L"VirtualAllocEx 失败："
            + std::to_wstring(GetLastError());
        return false;
    }

    bool success = false;
    SIZE_T written = 0;
    if (WriteProcessMemory(
            process,
            remotePath,
            path.c_str(),
            bytes,
            &written
        )
        && written == bytes)
    {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        const auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(kernel, "LoadLibraryW")
        );
        HANDLE thread = loadLibrary
            ? CreateRemoteThread(
                process,
                nullptr,
                0,
                loadLibrary,
                remotePath,
                0,
                nullptr
            )
            : nullptr;
        if (thread)
        {
            if (WaitForSingleObject(thread, 30000) == WAIT_OBJECT_0)
            {
                DWORD moduleHandle = 0;
                success = GetExitCodeThread(thread, &moduleHandle)
                    && moduleHandle != 0;
            }
            CloseHandle(thread);
        }
    }
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    if (!success)
    {
        error = L"远程 LoadLibraryW 失败："
            + std::to_wstring(GetLastError());
    }
    return success;
}

class EnvironmentOverride
{
public:
    EnvironmentOverride(const wchar_t* name, std::wstring_view value)
        : name_(name)
    {
        std::array<wchar_t, 32768> previous{};
        const DWORD length = GetEnvironmentVariableW(
            name,
            previous.data(),
            static_cast<DWORD>(previous.size())
        );
        existed_ = length > 0 && length < previous.size();
        if (existed_)
        {
            previous_ = previous.data();
        }
        const std::wstring owned(value);
        applied_ = SetEnvironmentVariableW(name, owned.c_str()) != FALSE;
    }

    ~EnvironmentOverride()
    {
        if (!applied_)
        {
            return;
        }
        SetEnvironmentVariableW(
            name_.c_str(),
            existed_ ? previous_.c_str() : nullptr
        );
    }

    bool Applied() const
    {
        return applied_;
    }

private:
    std::wstring name_;
    std::wstring previous_;
    bool existed_ = false;
    bool applied_ = false;
};

LaunchResult LaunchOriginal(const LauncherConfig& config)
{
    LaunchResult result;
    std::wstring command = QuoteArgument(
        config.originalLauncher.wstring()
    );
    std::vector<wchar_t> writable(
        command.begin(),
        command.end()
    );
    writable.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring directory =
        config.originalLauncher.parent_path().wstring();
    if (!CreateProcessW(
            config.originalLauncher.wstring().c_str(),
            writable.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            directory.c_str(),
            &startup,
            &process
        ))
    {
        result.message = L"无法启动原版启动器："
            + std::to_wstring(GetLastError());
        return result;
    }
    result.success = true;
    result.processStarted = true;
    result.processId = process.dwProcessId;
    result.message = L"原版启动器已启动。";
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

LaunchResult LaunchInjected(const LauncherConfig& config)
{
    LaunchResult result;
    if (config.preventDuplicateGame
        && IsGameAlreadyRunning(config.gameExecutable))
    {
        result.message = L"检测到同名 HOI3 进程；为避免重复注入，本次启动已取消。";
        return result;
    }

    const std::wstring handshakeName =
        L"Local\\HOI3NewCore_"
        + std::to_wstring(GetCurrentProcessId())
        + L"_" + std::to_wstring(GetTickCount64());
    HandshakeMapping handshake;
    std::wstring error;
    if (!CreateHandshakeMapping(handshakeName, handshake, error))
    {
        result.message = L"无法创建 New Core 握手：" + error;
        return result;
    }

    PROCESS_INFORMATION process{};
    {
        EnvironmentOverride root(
            L"NEW_CORE_ROOT",
            config.projectRoot.wstring()
        );
        EnvironmentOverride legacyRoot(
            L"SCRIPTED_GUI_ROOT",
            config.projectRoot.wstring()
        );
        EnvironmentOverride handshakeEnvironment(
            HandshakeEnvironmentName,
            handshakeName
        );
        if (!root.Applied()
            || !legacyRoot.Applied()
            || !handshakeEnvironment.Applied())
        {
            result.message = L"无法设置 New Core 启动环境。";
            CloseHandshakeMapping(handshake);
            return result;
        }

        std::wstring command = BuildInjectedGameCommandLine(config);
        std::vector<wchar_t> writable(command.begin(), command.end());
        writable.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        const std::wstring directory =
            config.gameExecutable.parent_path().wstring();
        if (!CreateProcessW(
                config.gameExecutable.wstring().c_str(),
                writable.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_SUSPENDED,
                nullptr,
                directory.c_str(),
                &startup,
                &process
            ))
        {
            result.message = L"无法创建 HOI3 进程："
                + std::to_wstring(GetLastError());
            CloseHandshakeMapping(handshake);
            return result;
        }
    }

    result.processStarted = true;
    result.processId = process.dwProcessId;
    PublishHandshake(
        handshake,
        HandshakeState::ProcessCreated,
        "HOI3 process created",
        {},
        {},
        process.dwProcessId
    );

    if (!InjectLibrary(process.hProcess, config.coreLibrary, error))
    {
        PublishHandshake(
            handshake,
            HandshakeState::Failed,
            "remote LoadLibraryW failed"
        );
        TerminateProcess(process.hProcess, 1);
        result.message = error;
        result.handshakeState = HandshakeState::Failed;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandshakeMapping(handshake);
        return result;
    }
    result.libraryInjected = true;

    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
    {
        TerminateProcess(process.hProcess, 1);
        result.message = L"无法恢复 HOI3 主线程。";
        result.handshakeState = HandshakeState::Failed;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandshakeMapping(handshake);
        return result;
    }
    CloseHandle(process.hThread);

    const uint64_t deadline = GetTickCount64()
        + config.handshakeTimeoutMilliseconds;
    bool terminalStateReached = false;
    while (GetTickCount64() < deadline)
    {
        result.handshakeState = ReadHandshakeState(handshake);
        if (result.handshakeState == HandshakeState::Ready
            || result.handshakeState == HandshakeState::Failed)
        {
            terminalStateReached = true;
            break;
        }
        if (WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0)
        {
            result.message = L"HOI3 在 New Core 就绪前退出。";
            result.handshakeState = HandshakeState::Failed;
            terminalStateReached = true;
            break;
        }
    }

    if (handshake.block)
    {
        MemoryBarrier();
        result.modules = handshake.block->modules;
        result.hooks = handshake.block->hooks;
        if (result.message.empty())
        {
            result.message = Utf8ToWide(handshake.block->message);
        }
    }
    result.coreReady = result.handshakeState == HandshakeState::Ready;
    result.success = result.processStarted
        && result.libraryInjected
        && result.coreReady;
    if (!terminalStateReached)
    {
        result.message = L"New Core 就绪等待超时；HOI3 进程仍在运行。";
    }
    CloseHandle(process.hProcess);
    CloseHandshakeMapping(handshake);
    return result;
}

}

LauncherConfig MakeDefaultLauncherConfig(
    const std::filesystem::path& launcherDirectory
)
{
    LauncherConfig config;
    config.projectRoot = FindProjectRoot(launcherDirectory);
    if (config.projectRoot.empty())
    {
        config.projectRoot = EnvironmentPath(L"NEW_CORE_ROOT");
    }

    std::filesystem::path gameRoot = EnvironmentPath(L"HOI3_ROOT");
    if (gameRoot.empty() && !config.projectRoot.empty())
    {
        gameRoot = config.projectRoot.parent_path() / "hoi3";
    }
    config.gameExecutable = gameRoot / "hoi3_tfh.exe";
    config.originalLauncher = gameRoot / "launcher.exe";

    const std::filesystem::path primaryLibrary =
        launcherDirectory / "hoi3_new_core.dll";
    config.coreLibrary = primaryLibrary;

    const std::filesystem::path developmentDescriptor =
        gameRoot / "tfh" / "mod" / "hoi3_scripted_gui_dev.mod";
    const std::filesystem::path productionDescriptor =
        gameRoot / "tfh" / "mod" / "Shower of Flowers5.mod";
    config.modDescriptor =
        std::filesystem::is_regular_file(developmentDescriptor)
        ? developmentDescriptor
        : productionDescriptor;
    return config;
}

bool LoadLauncherConfig(
    const std::filesystem::path& path,
    LauncherConfig& config,
    std::wstring& error
)
{
    if (!std::filesystem::is_regular_file(path))
    {
        error.clear();
        return true;
    }
    const std::wstring mode = ReadIniString(path, L"mode", L"inject");
    config.mode = _wcsicmp(mode.c_str(), L"original") == 0
        ? LauncherMode::OriginalLauncher
        : LauncherMode::InjectedGame;
    config.gameExecutable = ReadIniString(
        path,
        L"game_executable",
        config.gameExecutable.wstring()
    );
    config.originalLauncher = ReadIniString(
        path,
        L"original_launcher",
        config.originalLauncher.wstring()
    );
    config.coreLibrary = ReadIniString(
        path,
        L"core_library",
        config.coreLibrary.wstring()
    );
    config.projectRoot = ReadIniString(
        path,
        L"project_root",
        config.projectRoot.wstring()
    );
    config.modDescriptor = ReadIniString(
        path,
        L"mod_descriptor",
        config.modDescriptor.wstring()
    );
    config.extraArguments = ReadIniString(
        path,
        L"extra_arguments",
        config.extraArguments
    );
    config.handshakeTimeoutMilliseconds = static_cast<uint32_t>(_wtoi(
        ReadIniString(path, L"handshake_timeout_ms", L"125000").c_str()
    ));
    config.handshakeTimeoutMilliseconds = std::clamp<uint32_t>(
        config.handshakeTimeoutMilliseconds,
        5000,
        600000
    );
    config.preventDuplicateGame = _wtoi(
        ReadIniString(path, L"prevent_duplicate_game", L"1").c_str()
    ) != 0;
    error.clear();
    return true;
}

bool SaveLauncherConfig(
    const std::filesystem::path& path,
    const LauncherConfig& config,
    std::wstring& error
)
{
    std::error_code directoryError;
    std::filesystem::create_directories(
        path.parent_path(),
        directoryError
    );
    const bool success = WriteIniString(
            path,
            L"mode",
            config.mode == LauncherMode::InjectedGame
                ? L"inject" : L"original"
        )
        && WriteIniString(
            path,
            L"game_executable",
            config.gameExecutable.wstring()
        )
        && WriteIniString(
            path,
            L"original_launcher",
            config.originalLauncher.wstring()
        )
        && WriteIniString(
            path,
            L"core_library",
            config.coreLibrary.wstring()
        )
        && WriteIniString(
            path,
            L"project_root",
            config.projectRoot.wstring()
        )
        && WriteIniString(
            path,
            L"mod_descriptor",
            config.modDescriptor.wstring()
        )
        && WriteIniString(
            path,
            L"extra_arguments",
            config.extraArguments
        )
        && WriteIniString(
            path,
            L"handshake_timeout_ms",
            std::to_wstring(config.handshakeTimeoutMilliseconds)
        )
        && WriteIniString(
            path,
            L"prevent_duplicate_game",
            config.preventDuplicateGame ? L"1" : L"0"
        );
    if (!success)
    {
        error = L"无法写入启动器配置："
            + std::to_wstring(GetLastError());
        return false;
    }
    error.clear();
    return true;
}

bool ValidateLauncherConfig(
    const LauncherConfig& config,
    std::wstring& error
)
{
    if (config.mode == LauncherMode::OriginalLauncher)
    {
        if (!std::filesystem::is_regular_file(config.originalLauncher))
        {
            error = L"原版启动器路径无效。";
            return false;
        }
        error.clear();
        return true;
    }
    if (!std::filesystem::is_regular_file(config.gameExecutable))
    {
        error = L"HOI3 可执行文件路径无效。";
        return false;
    }
    if (!std::filesystem::is_regular_file(config.coreLibrary))
    {
        error = L"New Core DLL 路径无效。";
        return false;
    }
    if (!std::filesystem::is_directory(config.projectRoot))
    {
        error = L"Mod/New Core 根目录无效。";
        return false;
    }
    if (!std::filesystem::is_directory(
            config.projectRoot / "interface" / "gui_plugins"
        )
        || !std::filesystem::is_directory(
            config.projectRoot / "new_core"
        ))
    {
        error = L"项目根目录缺少 interface/gui_plugins 或 new_core。";
        return false;
    }
    if (!config.modDescriptor.empty()
        && !std::filesystem::is_regular_file(config.modDescriptor))
    {
        error = L"Mod 描述文件路径无效。";
        return false;
    }
    std::wstring architectureError;
    if (!IsPe32I386(config.gameExecutable, architectureError))
    {
        error = L"HOI3 必须是 32 位 x86 程序：" + architectureError;
        return false;
    }
    if (!IsPe32I386(config.coreLibrary, architectureError))
    {
        error = L"New Core DLL 必须是 32 位 x86：" + architectureError;
        return false;
    }
    if (!HasNewCoreExports(config.coreLibrary))
    {
        error = L"所选 DLL 不包含 New Core ABI 导出。";
        return false;
    }
    error.clear();
    return true;
}

bool IsPe32I386(
    const std::filesystem::path& path,
    std::wstring& error
)
{
    std::ifstream input(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    input.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!input || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
    {
        error = L"DOS/PE 文件头无效。";
        return false;
    }
    input.seekg(dos.e_lfanew, std::ios::beg);
    DWORD signature = 0;
    IMAGE_FILE_HEADER file{};
    input.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    input.read(reinterpret_cast<char*>(&file), sizeof(file));
    if (!input
        || signature != IMAGE_NT_SIGNATURE
        || file.Machine != IMAGE_FILE_MACHINE_I386)
    {
        error = L"不是 PE32 i386 文件。";
        return false;
    }
    error.clear();
    return true;
}

std::wstring BuildInjectedGameCommandLine(
    const LauncherConfig& config
)
{
    std::wstring command = QuoteArgument(
        config.gameExecutable.wstring()
    );
    if (!config.modDescriptor.empty())
    {
        const std::wstring modArgument = L"-mod=mod/"
            + config.modDescriptor.filename().wstring();
        command.push_back(L' ');
        command += QuoteArgument(modArgument);
    }
    if (!config.extraArguments.empty())
    {
        command.push_back(L' ');
        command += config.extraArguments;
    }
    return command;
}

LaunchResult LaunchFromConfig(const LauncherConfig& config)
{
    std::wstring error;
    if (!ValidateLauncherConfig(config, error))
    {
        LaunchResult result;
        result.message = std::move(error);
        return result;
    }
    return config.mode == LauncherMode::OriginalLauncher
        ? LaunchOriginal(config)
        : LaunchInjected(config);
}

}
