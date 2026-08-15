#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "gui_host_d3d9.h"
#include "gui_d3d9_hook.h"
#include "gui_diagnostics.h"
#include "gui_lua_bridge.h"
#include "gui_lua51_hook.h"
#include "gui_lua_native_binding.h"
#include "scripted_gui_overlay_api.h"

namespace
{

HMODULE ModuleHandle = nullptr;

struct OverlayRuntime
{
    std::mutex mutex;
    std::unique_ptr<GuiD3D9Host> host;
    std::filesystem::path configuredRoot;
    std::string lastError;
    std::string lastLoggedError;
};

OverlayRuntime& Runtime()
{
    static OverlayRuntime runtime;
    return runtime;
}

bool IsProjectRoot(const std::filesystem::path& path)
{
    return std::filesystem::is_directory(path / "interface" / "gui_plugins")
        && (std::filesystem::is_directory(path / "script_gui")
            || std::filesystem::is_directory(path / "scripted_guis"));
}

void AppendCandidateAndParents(
    std::vector<std::filesystem::path>& output,
    std::filesystem::path candidate
)
{
    candidate = candidate.lexically_normal();
    for (int depth = 0; depth < 8 && !candidate.empty(); ++depth)
    {
        output.push_back(candidate);
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
}

std::filesystem::path ResolveRoot(
    const OverlayRuntime& runtime
)
{
    if (!runtime.configuredRoot.empty()
        && IsProjectRoot(runtime.configuredRoot))
    {
        return runtime.configuredRoot;
    }

    std::vector<std::filesystem::path> candidates;
    wchar_t environmentRoot[32768]{};
    const DWORD environmentLength = GetEnvironmentVariableW(
        L"SCRIPTED_GUI_ROOT",
        environmentRoot,
        static_cast<DWORD>(std::size(environmentRoot))
    );
    if (environmentLength > 0
        && environmentLength < std::size(environmentRoot))
    {
        AppendCandidateAndParents(candidates, environmentRoot);
    }

    wchar_t modulePath[32768]{};
    const DWORD moduleLength = GetModuleFileNameW(
        ModuleHandle,
        modulePath,
        static_cast<DWORD>(std::size(modulePath))
    );
    if (moduleLength > 0 && moduleLength < std::size(modulePath))
    {
        AppendCandidateAndParents(
            candidates,
            std::filesystem::path(modulePath).parent_path()
        );
    }

    std::error_code currentError;
    const std::filesystem::path current =
        std::filesystem::current_path(currentError);
    if (!currentError)
    {
        AppendCandidateAndParents(candidates, current);
    }

    for (const std::filesystem::path& candidate : candidates)
    {
        if (IsProjectRoot(candidate))
        {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
    }
    return {};
}

bool EnsureAttached(
    OverlayRuntime& runtime,
    IDirect3DDevice9* device
)
{
    if (runtime.host && runtime.host->IsInitialized())
    {
        if (runtime.host->UsesDevice(device))
        {
            return true;
        }
        WriteGuiDiagnostic(
            "D3D9 presentation device changed; rebuilding GUI host"
        );
        runtime.host->Shutdown();
        runtime.host.reset();
    }
    const std::filesystem::path root = ResolveRoot(runtime);
    if (root.empty())
    {
        runtime.lastError =
            "Unable to locate the Scripted GUI project root";
        if (runtime.lastLoggedError != runtime.lastError)
        {
            WriteGuiDiagnostic(runtime.lastError);
            runtime.lastLoggedError = runtime.lastError;
        }
        return false;
    }
    auto host = std::make_unique<GuiD3D9Host>();
    if (!host->Initialize(root, device, runtime.lastError))
    {
        if (runtime.lastLoggedError != runtime.lastError)
        {
            WriteGuiDiagnostic(
                "D3D9 host initialization failed: " + runtime.lastError
            );
            runtime.lastLoggedError = runtime.lastError;
        }
        return false;
    }
    runtime.host = std::move(host);
    SetGuiDiagnosticsRoot(root);
    WriteGuiDiagnostic("D3D9 Scripted GUI host initialized");
    runtime.lastError.clear();
    runtime.lastLoggedError.clear();
    return true;
}

DWORD WINAPI InstallHooksWorker(LPVOID)
{
    HMODULE selfReference = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(InstallHooksWorker),
        &selfReference
    );
    DWORD result = 1;
    ResetGuiDiagnostics();
    WriteGuiDiagnostic("Scripted GUI hook worker started");
    for (int attempt = 0; attempt < 240; ++attempt)
    {
        std::string d3dError;
        std::string luaError;
        const bool d3dReady = AreGuiD3D9HooksInstalled()
            || InstallGuiD3D9Hooks(d3dError);
        const bool luaReady = AreGuiLua51HooksInstalled()
            || InstallGuiLua51Hooks(luaError);
        if (d3dReady && luaReady)
        {
            WriteGuiDiagnostic("D3D9 and Lua 5.1 hooks installed");
            result = 0;
            break;
        }
        Sleep(500);
    }
    if (selfReference)
    {
        FreeLibraryAndExitThread(selfReference, result);
    }
    return result;
}

}

extern "C" BOOL WINAPI
ScriptedGui_SetRootW(const wchar_t* root)
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (runtime.host && runtime.host->IsInitialized())
    {
        runtime.lastError =
            "Scripted GUI root cannot change after initialization";
        return FALSE;
    }
    runtime.configuredRoot = root
        ? std::filesystem::path(root)
        : std::filesystem::path{};
    if (!runtime.configuredRoot.empty())
    {
        SetGuiDiagnosticsRoot(runtime.configuredRoot);
    }
    return TRUE;
}

extern "C" BOOL WINAPI
ScriptedGui_AttachDevice(IDirect3DDevice9* device)
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    return EnsureAttached(runtime, device) ? TRUE : FALSE;
}

extern "C" BOOL WINAPI ScriptedGui_AttachLua51(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1* api
)
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (!api)
    {
        runtime.lastError = "Lua 5.1 API table is missing";
        return FALSE;
    }
    if (!GetGuiLuaNativeBinding().Install(
            state,
            *api,
            GetGuiLuaBridgeService(),
            runtime.lastError
        ))
    {
        return FALSE;
    }
    runtime.lastError.clear();
    WriteGuiDiagnostic("Lua 5.1 native bridge attached explicitly");
    return TRUE;
}

extern "C" BOOL WINAPI ScriptedGui_IsLuaAttached()
{
    return GetGuiLuaNativeBinding().IsInstalled() ? TRUE : FALSE;
}

extern "C" BOOL WINAPI ScriptedGui_InstallHooks()
{
    std::string d3dError;
    std::string luaError;
    const bool d3dReady = AreGuiD3D9HooksInstalled()
        || InstallGuiD3D9Hooks(d3dError);
    const bool luaReady = AreGuiLua51HooksInstalled()
        || InstallGuiLua51Hooks(luaError);
    if (d3dReady && luaReady)
    {
        return TRUE;
    }
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    runtime.lastError = !d3dReady
        ? std::move(d3dError)
        : std::move(luaError);
    return FALSE;
}

extern "C" void WINAPI ScriptedGui_UninstallHooks()
{
    UninstallGuiD3D9Hooks();
    UninstallGuiLua51Hooks();
}

extern "C" BOOL WINAPI ScriptedGui_AreHooksInstalled()
{
    return AreGuiD3D9HooksInstalled()
        && AreGuiLua51HooksInstalled()
        ? TRUE
        : FALSE;
}

extern "C" void WINAPI
ScriptedGui_OnEndScene(IDirect3DDevice9* device)
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (EnsureAttached(runtime, device))
    {
        runtime.host->TickAndRender(device);
    }
}

extern "C" void WINAPI
ScriptedGui_OnBeforeReset()
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (runtime.host)
    {
        runtime.host->BeforeDeviceReset();
    }
}

extern "C" BOOL WINAPI
ScriptedGui_OnAfterReset(IDirect3DDevice9* device, HRESULT resetResult)
{
    if (FAILED(resetResult))
    {
        return FALSE;
    }
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    return runtime.host
        && runtime.host->AfterDeviceReset(device, runtime.lastError)
        ? TRUE
        : FALSE;
}

extern "C" BOOL WINAPI
ScriptedGui_HandleWindowMessage(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    return runtime.host
        && runtime.host->HandleWindowMessage(
            window,
            message,
            wParam,
            lParam
        )
        ? TRUE
        : FALSE;
}

extern "C" void WINAPI
ScriptedGui_Shutdown()
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (runtime.host)
    {
        runtime.host->Shutdown();
        runtime.host.reset();
    }
}

extern "C" DWORD WINAPI
ScriptedGui_GetLastError(char* output, DWORD capacity)
{
    OverlayRuntime& runtime = Runtime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    const DWORD required = static_cast<DWORD>(
        runtime.lastError.size() + 1
    );
    if (output && capacity > 0)
    {
        const std::size_t count = std::min<std::size_t>(
            runtime.lastError.size(),
            capacity - 1
        );
        std::memcpy(output, runtime.lastError.data(), count);
        output[count] = '\0';
    }
    return required;
}

BOOL APIENTRY DllMain(
    HMODULE module,
    DWORD reason,
    LPVOID reserved
)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        ModuleHandle = module;
        DisableThreadLibraryCalls(module);
        HANDLE worker = CreateThread(
            nullptr,
            0,
            InstallHooksWorker,
            nullptr,
            0,
            nullptr
        );
        if (worker)
        {
            CloseHandle(worker);
        }
    }
    else if (reason == DLL_PROCESS_DETACH && reserved == nullptr)
    {
        UninstallGuiD3D9Hooks();
        UninstallGuiLua51Hooks();
    }
    return TRUE;
}
