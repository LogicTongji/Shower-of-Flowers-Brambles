#pragma once

#include <d3d9.h>
#include <windows.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core_hook_registry.h"
#include "core_lifecycle.h"
#include "core_module_registry.h"
#include "scripted_gui_overlay_api.h"

class ScriptGuiCoreModule;

namespace core
{

class Runtime
{
public:
    bool Initialize(HMODULE moduleHandle, std::string& error);

    bool InstallHooks(std::string& error);
    void UninstallHooks();
    bool AreHooksInstalled() const;
    void Pump();

    bool SetScriptGuiRoot(const wchar_t* root);
    bool AttachScriptGuiDevice(IDirect3DDevice9* device);
    bool AttachScriptGuiLua51(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1* api
    );
    bool IsScriptGuiLuaAttached() const;
    void OnEndScene(IDirect3DDevice9* device);
    void OnBeforeReset();
    bool OnAfterReset(
        IDirect3DDevice9* device,
        HRESULT resetResult
    );
    bool HandleWindowMessage(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    bool NotifySaveLoaded(
        std::string_view saveKey,
        LifecycleEventSource source =
            LifecycleEventSource::External
    );

    LifecycleSnapshot Lifecycle() const;
    std::vector<std::string> ModuleIds() const;
    std::vector<HookStatus> HookStatuses() const;
    std::string LastError() const;

    void Shutdown();

private:
    bool InitializeUnlocked(HMODULE moduleHandle, std::string& error);
    void PumpUnlocked(uint64_t nowMilliseconds);
    void ProbeLifecycleUnlocked(uint64_t nowMilliseconds);
    void DispatchLifecycleUnlocked();
    void SetErrorUnlocked(std::string error);

    mutable std::mutex mutex_;
    HMODULE moduleHandle_ = nullptr;
    HookRegistry hooks_;
    LifecycleService lifecycle_;
    ModuleRegistry modules_;
    ScriptGuiCoreModule* scriptGui_ = nullptr;
    uint64_t nextLifecycleProbeMilliseconds_ = 0;
    bool lifecycleUnsupportedLogged_ = false;
    bool initialized_ = false;
    bool terminated_ = false;
    std::string lastError_;
};

Runtime& GetRuntime();

}
