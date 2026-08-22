#include "gui_diagnostics.h"

#include <windows.h>

#include <fstream>
#include <mutex>
#include <string>

namespace
{

std::mutex DiagnosticsMutex;
std::filesystem::path DiagnosticsRoot;

std::filesystem::path ResolveRoot()
{
    if (!DiagnosticsRoot.empty())
    {
        return DiagnosticsRoot;
    }
    wchar_t root[32768]{};
    DWORD length = GetEnvironmentVariableW(
        L"NEW_CORE_ROOT",
        root,
        static_cast<DWORD>(std::size(root))
    );
    if (length == 0 || length >= std::size(root))
    {
        length = GetEnvironmentVariableW(
            L"SCRIPTED_GUI_ROOT",
            root,
            static_cast<DWORD>(std::size(root))
        );
    }
    if (length > 0 && length < std::size(root))
    {
        return std::filesystem::path(root);
    }
    std::error_code error;
    return std::filesystem::current_path(error);
}

std::filesystem::path LogPath()
{
    return ResolveRoot() / "new_core" / "new_core_runtime.log";
}

}

void SetGuiDiagnosticsRoot(const std::filesystem::path& root)
{
    std::lock_guard<std::mutex> lock(DiagnosticsMutex);
    DiagnosticsRoot = root;
}

void ResetGuiDiagnostics()
{
    std::lock_guard<std::mutex> lock(DiagnosticsMutex);
    std::ofstream output(LogPath(), std::ios::trunc);
}

void WriteGuiDiagnostic(std::string_view message)
{
    std::lock_guard<std::mutex> lock(DiagnosticsMutex);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::ofstream output(LogPath(), std::ios::app);
    if (!output)
    {
        return;
    }
    output
        << '['
        << time.wHour << ':'
        << time.wMinute << ':'
        << time.wSecond << '.'
        << time.wMilliseconds
        << "][thread=" << GetCurrentThreadId() << "] "
        << message << '\n';
}
