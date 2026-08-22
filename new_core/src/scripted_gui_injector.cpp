#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

std::wstring QuoteArgument(std::wstring_view value)
{
    if (value.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
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

std::wstring BuildCommandLine(
    const std::filesystem::path& executable,
    int argumentCount,
    wchar_t** arguments
)
{
    std::wstring command = QuoteArgument(executable.wstring());
    for (int index = 4; index < argumentCount; ++index)
    {
        std::wstring value = arguments[index];
        if (_wcsicmp(value.c_str(), L"-mod") == 0
            && index + 1 < argumentCount)
        {
            value = L"-mod=" + std::wstring(arguments[++index]);
        }
        command.push_back(L' ');
        command += QuoteArgument(value);
    }
    return command;
}

bool InjectLibrary(
    HANDLE process,
    const std::filesystem::path& library,
    std::wstring& error
)
{
    const std::wstring path = library.wstring();
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
        error = L"VirtualAllocEx failed";
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
        error = L"remote LoadLibraryW failed";
    }
    return success;
}

}

int wmain(int argumentCount, wchar_t** arguments)
{
    if (argumentCount < 4)
    {
        std::wcerr
            << L"usage: scripted_gui_injector <game.exe> <overlay.dll> "
               L"<project-root> [game arguments...]\n";
        return 2;
    }
    const std::filesystem::path executable =
        std::filesystem::absolute(arguments[1]).lexically_normal();
    const std::filesystem::path library =
        std::filesystem::absolute(arguments[2]).lexically_normal();
    const std::filesystem::path projectRoot =
        std::filesystem::absolute(arguments[3]).lexically_normal();
    if (!std::filesystem::is_regular_file(executable)
        || !std::filesystem::is_regular_file(library)
        || !std::filesystem::is_directory(projectRoot))
    {
        std::wcerr << L"one or more input paths are invalid\n";
        return 3;
    }

    wchar_t previousRoot[32768]{};
    const DWORD previousLength = GetEnvironmentVariableW(
        L"SCRIPTED_GUI_ROOT",
        previousRoot,
        static_cast<DWORD>(std::size(previousRoot))
    );
    if (!SetEnvironmentVariableW(
            L"SCRIPTED_GUI_ROOT",
            projectRoot.wstring().c_str()
        ))
    {
        std::wcerr << L"failed to set SCRIPTED_GUI_ROOT\n";
        return 4;
    }

    std::wstring commandLine = BuildCommandLine(
        executable,
        argumentCount,
        arguments
    );
    std::wcout << L"HOI3 command line: " << commandLine << L'\n';
    std::vector<wchar_t> writableCommand(
        commandLine.begin(),
        commandLine.end()
    );
    writableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring workingDirectory =
        executable.parent_path().wstring();
    const BOOL created = CreateProcessW(
        executable.wstring().c_str(),
        writableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        workingDirectory.c_str(),
        &startup,
        &process
    );
    if (previousLength > 0 && previousLength < std::size(previousRoot))
    {
        SetEnvironmentVariableW(L"SCRIPTED_GUI_ROOT", previousRoot);
    }
    else
    {
        SetEnvironmentVariableW(L"SCRIPTED_GUI_ROOT", nullptr);
    }
    if (!created)
    {
        std::wcerr << L"CreateProcessW failed: " << GetLastError() << L'\n';
        return 5;
    }

    std::wstring error;
    if (!InjectLibrary(process.hProcess, library, error))
    {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::wcerr << error << L": " << GetLastError() << L'\n';
        return 6;
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
    {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::wcerr << L"ResumeThread failed\n";
        return 7;
    }
    std::wcout << L"HOI3 started with Scripted GUI, pid="
        << process.dwProcessId << L'\n';
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
