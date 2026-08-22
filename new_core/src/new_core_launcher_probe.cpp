#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "new_core_handshake.h"
#include "new_core_launcher_core.h"

namespace
{

class EnvironmentRestore
{
public:
    explicit EnvironmentRestore(const wchar_t* name)
        : name_(name)
    {
        wchar_t value[512]{};
        const DWORD length = GetEnvironmentVariableW(
            name,
            value,
            static_cast<DWORD>(std::size(value))
        );
        existed_ = length > 0 && length < std::size(value);
        if (existed_)
        {
            value_ = value;
        }
    }

    ~EnvironmentRestore()
    {
        SetEnvironmentVariableW(
            name_.c_str(),
            existed_ ? value_.c_str() : nullptr
        );
    }

private:
    std::wstring name_;
    std::wstring value_;
    bool existed_ = false;
};

}

int wmain(int argumentCount, wchar_t** arguments)
{
    if (argumentCount == 2
        && _wcsicmp(arguments[1], L"--publish-handshake") == 0)
    {
        new_core::HandshakeMapping runtimeMapping;
        std::string error;
        if (!new_core::OpenHandshakeMappingFromEnvironment(
                runtimeMapping,
                error
            ))
        {
            return 20;
        }
        new_core::PublishHandshake(
            runtimeMapping,
            new_core::HandshakeState::Ready,
            "probe ready",
            "script_gui,leader_capture",
            "windows.d3d9=installed,hoi3.leader_capture=installed",
            GetCurrentProcessId(),
            1
        );
        new_core::CloseHandshakeMapping(runtimeMapping);
        return 0;
    }
    if (argumentCount != 5)
    {
        std::wcerr << L"launcher probe requires four paths\n";
        return 2;
    }
    const std::filesystem::path root = arguments[1];
    const std::filesystem::path launcher = arguments[2];
    const std::filesystem::path library = arguments[3];
    const std::filesystem::path ini = arguments[4];

    std::wstring error;
    if (!new_core::IsPe32I386(launcher, error)
        || !new_core::IsPe32I386(library, error))
    {
        std::wcerr << L"PE32 validation failed: " << error << L'\n';
        return 3;
    }

    new_core::LauncherConfig config;
    config.mode = new_core::LauncherMode::InjectedGame;
    config.gameExecutable = launcher;
    config.originalLauncher = launcher;
    config.coreLibrary = library;
    config.projectRoot = root;
    config.modDescriptor.clear();
    config.extraArguments = L"-debug -probe";
    config.handshakeTimeoutMilliseconds = 54321;
    config.preventDuplicateGame = false;
    if (!new_core::ValidateLauncherConfig(config, error))
    {
        std::wcerr << L"Launcher config validation failed: "
                   << error << L'\n';
        return 4;
    }

    std::error_code removeError;
    std::filesystem::remove(ini, removeError);
    if (!new_core::SaveLauncherConfig(ini, config, error))
    {
        std::wcerr << L"Launcher config save failed: " << error << L'\n';
        return 5;
    }
    new_core::LauncherConfig loaded;
    if (!new_core::LoadLauncherConfig(ini, loaded, error)
        || loaded.mode != config.mode
        || loaded.gameExecutable != config.gameExecutable
        || loaded.coreLibrary != config.coreLibrary
        || loaded.projectRoot != config.projectRoot
        || loaded.extraArguments != config.extraArguments
        || loaded.handshakeTimeoutMilliseconds
            != config.handshakeTimeoutMilliseconds
        || loaded.preventDuplicateGame)
    {
        std::wcerr << L"Launcher config roundtrip failed\n";
        return 6;
    }
    std::filesystem::remove(ini, removeError);

    const std::wstring command =
        new_core::BuildInjectedGameCommandLine(config);
    if (command.find(L"-debug -probe") == std::wstring::npos)
    {
        std::wcerr << L"Launcher command construction failed\n";
        return 7;
    }

    const std::wstring mappingName = L"Local\\NewCoreLauncherProbe_"
        + std::to_wstring(GetCurrentProcessId());
    new_core::HandshakeMapping launcherMapping;
    if (!new_core::CreateHandshakeMapping(
            mappingName,
            launcherMapping,
            error
        ))
    {
        std::wcerr << L"Handshake creation failed: " << error << L'\n';
        return 8;
    }
    EnvironmentRestore restore(new_core::HandshakeEnvironmentName);
    SetEnvironmentVariableW(
        new_core::HandshakeEnvironmentName,
        mappingName.c_str()
    );
    std::wstring childCommand = L"\""
        + std::filesystem::absolute(arguments[0]).wstring()
        + L"\" --publish-handshake";
    std::vector<wchar_t> writable(
        childCommand.begin(),
        childCommand.end()
    );
    writable.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(
            std::filesystem::absolute(arguments[0]).wstring().c_str(),
            writable.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &child
        ))
    {
        std::cerr << "Handshake publisher process failed\n";
        new_core::CloseHandshakeMapping(launcherMapping);
        return 9;
    }
    WaitForSingleObject(child.hProcess, 5000);
    DWORD childExitCode = 1;
    GetExitCodeProcess(child.hProcess, &childExitCode);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    const bool handshakePassed =
        childExitCode == 0
        &&
        new_core::ReadHandshakeState(launcherMapping)
            == new_core::HandshakeState::Ready
        && launcherMapping.block
        && std::string(launcherMapping.block->message) == "probe ready"
        && launcherMapping.block->processId != 0
        && launcherMapping.block->abiVersion == 1;
    new_core::CloseHandshakeMapping(launcherMapping);
    if (!handshakePassed)
    {
        std::cerr << "Handshake roundtrip failed\n";
        return 10;
    }

    std::cout << "New Core launcher probe: passed\n";
    return 0;
}
