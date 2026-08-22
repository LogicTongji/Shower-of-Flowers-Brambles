#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core_runtime.h"
#include "gui_diagnostics.h"
#include "new_core_handshake.h"
#include "scripted_gui_overlay_api.h"

namespace
{

HMODULE ModuleHandle = nullptr;

DWORD WriteStringResult(
    const std::string& value,
    char* output,
    DWORD capacity
)
{
    const DWORD required = static_cast<DWORD>(value.size() + 1);
    if (output && capacity > 0)
    {
        const std::size_t count = std::min<std::size_t>(
            value.size(),
            capacity - 1
        );
        std::memcpy(output, value.data(), count);
        output[count] = '\0';
    }
    return required;
}

std::string JoinModuleIds(core::Runtime& runtime)
{
    const std::vector<std::string> modules = runtime.ModuleIds();
    std::string output;
    for (const std::string& module : modules)
    {
        if (!output.empty())
        {
            output += ',';
        }
        output += module;
    }
    return output;
}

std::string JoinHookStatuses(core::Runtime& runtime)
{
    const std::vector<core::HookStatus> hooks =
        runtime.HookStatuses();
    std::string output;
    for (const core::HookStatus& hook : hooks)
    {
        if (!output.empty())
        {
            output += ',';
        }
        output += hook.id;
        output += '=';
        output += hook.installed ? "installed" : "pending";
    }
    return output;
}

bool EnsureRuntimeInitialized(core::Runtime& runtime)
{
    std::string error;
    return runtime.Initialize(ModuleHandle, error);
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
    WriteGuiDiagnostic("New Core hook worker started");

    new_core::HandshakeMapping handshake;
    std::string handshakeError;
    const bool handshakeRequested =
        new_core::OpenHandshakeMappingFromEnvironment(
            handshake,
            handshakeError
        );
    if (handshakeRequested)
    {
        new_core::PublishHandshake(
            handshake,
            new_core::HandshakeState::DllWorkerStarted,
            "New Core DLL worker started",
            {},
            {},
            GetCurrentProcessId(),
            NEW_CORE_ABI_VERSION
        );
    }

    core::Runtime& runtime = core::GetRuntime();
    std::string error;
    if (!runtime.Initialize(ModuleHandle, error))
    {
        WriteGuiDiagnostic(
            "New Core initialization failed: " + error
        );
        if (handshakeRequested)
        {
            new_core::PublishHandshake(
                handshake,
                new_core::HandshakeState::Failed,
                error
            );
        }
    }
    else
    {
        const std::string modules = JoinModuleIds(runtime);
        if (handshakeRequested)
        {
            new_core::PublishHandshake(
                handshake,
                new_core::HandshakeState::RuntimeInitialized,
                "New Core runtime initialized",
                modules,
                JoinHookStatuses(runtime),
                GetCurrentProcessId(),
                NEW_CORE_ABI_VERSION
            );
            new_core::PublishHandshake(
                handshake,
                new_core::HandshakeState::HooksInstalling,
                "Installing registered hooks",
                modules,
                JoinHookStatuses(runtime)
            );
        }
        for (int attempt = 0; attempt < 240; ++attempt)
        {
            if (runtime.InstallHooks(error))
            {
                WriteGuiDiagnostic(
                    "All registered New Core hooks installed"
                );
                result = 0;
                if (handshakeRequested)
                {
                    new_core::PublishHandshake(
                        handshake,
                        new_core::HandshakeState::Ready,
                        "New Core is ready",
                        modules,
                        JoinHookStatuses(runtime),
                        GetCurrentProcessId(),
                        NEW_CORE_ABI_VERSION
                    );
                }
                break;
            }
            Sleep(500);
        }
        if (result != 0 && !error.empty())
        {
            WriteGuiDiagnostic(
                "New Core hook installation timed out: " + error
            );
            if (handshakeRequested)
            {
                new_core::PublishHandshake(
                    handshake,
                    new_core::HandshakeState::Failed,
                    error,
                    modules,
                    JoinHookStatuses(runtime)
                );
            }
        }
    }

    new_core::CloseHandshakeMapping(handshake);

    if (selfReference)
    {
        FreeLibraryAndExitThread(selfReference, result);
    }
    return result;
}

}

extern "C" uint32_t WINAPI NewCore_GetAbiVersion()
{
    return NEW_CORE_ABI_VERSION;
}

extern "C" DWORD WINAPI NewCore_GetModuleIds(
    char* output,
    DWORD capacity
)
{
    core::Runtime& runtime = core::GetRuntime();
    EnsureRuntimeInitialized(runtime);
    return WriteStringResult(
        JoinModuleIds(runtime),
        output,
        capacity
    );
}

extern "C" DWORD WINAPI NewCore_GetHookStatuses(
    char* output,
    DWORD capacity
)
{
    core::Runtime& runtime = core::GetRuntime();
    EnsureRuntimeInitialized(runtime);
    return WriteStringResult(
        JoinHookStatuses(runtime),
        output,
        capacity
    );
}

extern "C" DWORD WINAPI NewCore_GetLastError(
    char* output,
    DWORD capacity
)
{
    return WriteStringResult(
        core::GetRuntime().LastError(),
        output,
        capacity
    );
}

extern "C" BOOL WINAPI ScriptedGui_SetRootW(const wchar_t* root)
{
    core::Runtime& runtime = core::GetRuntime();
    return EnsureRuntimeInitialized(runtime)
        && runtime.SetScriptGuiRoot(root)
        ? TRUE
        : FALSE;
}

extern "C" BOOL WINAPI
ScriptedGui_AttachDevice(IDirect3DDevice9* device)
{
    core::Runtime& runtime = core::GetRuntime();
    return EnsureRuntimeInitialized(runtime)
        && runtime.AttachScriptGuiDevice(device)
        ? TRUE
        : FALSE;
}

extern "C" BOOL WINAPI ScriptedGui_AttachLua51(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1* api
)
{
    core::Runtime& runtime = core::GetRuntime();
    return EnsureRuntimeInitialized(runtime)
        && runtime.AttachScriptGuiLua51(state, api)
        ? TRUE
        : FALSE;
}

extern "C" BOOL WINAPI ScriptedGui_IsLuaAttached()
{
    return core::GetRuntime().IsScriptGuiLuaAttached()
        ? TRUE
        : FALSE;
}

extern "C" BOOL WINAPI ScriptedGui_InstallHooks()
{
    std::string error;
    core::Runtime& runtime = core::GetRuntime();
    return runtime.Initialize(ModuleHandle, error)
        && runtime.InstallHooks(error)
        ? TRUE
        : FALSE;
}

extern "C" void WINAPI ScriptedGui_UninstallHooks()
{
    core::GetRuntime().UninstallHooks();
}

extern "C" BOOL WINAPI ScriptedGui_AreHooksInstalled()
{
    return core::GetRuntime().AreHooksInstalled()
        ? TRUE
        : FALSE;
}

extern "C" void WINAPI
ScriptedGui_OnEndScene(IDirect3DDevice9* device)
{
    core::Runtime& runtime = core::GetRuntime();
    if (EnsureRuntimeInitialized(runtime))
    {
        runtime.OnEndScene(device);
    }
}

extern "C" void WINAPI ScriptedGui_OnBeforeReset()
{
    core::GetRuntime().OnBeforeReset();
}

extern "C" BOOL WINAPI ScriptedGui_OnAfterReset(
    IDirect3DDevice9* device,
    HRESULT resetResult
)
{
    return core::GetRuntime().OnAfterReset(device, resetResult)
        ? TRUE
        : FALSE;
}

extern "C" BOOL WINAPI ScriptedGui_HandleWindowMessage(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    return core::GetRuntime().HandleWindowMessage(
            window,
            message,
            wParam,
            lParam
        )
        ? TRUE
        : FALSE;
}

extern "C" void WINAPI ScriptedGui_Shutdown()
{
    core::GetRuntime().Shutdown();
}

extern "C" DWORD WINAPI ScriptedGui_GetLastError(
    char* output,
    DWORD capacity
)
{
    return NewCore_GetLastError(output, capacity);
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
        core::GetRuntime().UninstallHooks();
    }
    return TRUE;
}
