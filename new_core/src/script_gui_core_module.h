#pragma once

#include <d3d9.h>
#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>

#include "core_module.h"
#include "scripted_gui_overlay_api.h"

class GuiD3D9Host;

class ScriptGuiCoreModule final : public core::IModule
{
public:
    explicit ScriptGuiCoreModule(HMODULE moduleHandle);
    ~ScriptGuiCoreModule() override;

    std::string_view Id() const override;
    int Priority() const override;
    bool Initialize(
        core::Services& services,
        std::string& error
    ) override;
    void OnLifecycleEvent(
        const core::LifecycleEvent& event
    ) override;
    void Tick(uint64_t nowMilliseconds) override;
    void Shutdown() override;

    bool SetRoot(const wchar_t* root);
    bool AttachDevice(IDirect3DDevice9* device);
    bool AttachLua51(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1* api
    );
    bool IsLuaAttached() const;

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

    const std::string& LastError() const;

private:
    bool EnsureAttached(IDirect3DDevice9* device);
    std::filesystem::path ResolveRoot() const;

    HMODULE moduleHandle_ = nullptr;
    core::DiagnosticSink diagnostic_;
    std::unique_ptr<GuiD3D9Host> host_;
    std::filesystem::path configuredRoot_;
    std::string lastError_;
    std::string lastLoggedError_;
    bool initialized_ = false;
};
