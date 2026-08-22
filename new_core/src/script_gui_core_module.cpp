#include "script_gui_core_module.h"

#include <array>
#include <utility>
#include <vector>

#include "gui_d3d9_hook.h"
#include "gui_diagnostics.h"
#include "gui_host_d3d9.h"
#include "gui_lua51_hook.h"
#include "gui_lua_bridge.h"
#include "gui_lua_native_binding.h"

namespace
{

bool IsScriptGuiRoot(const std::filesystem::path& path)
{
    return std::filesystem::is_directory(
            path / "interface" / "gui_plugins"
        )
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

void AppendEnvironmentRoot(
    std::vector<std::filesystem::path>& output,
    const wchar_t* name
)
{
    std::array<wchar_t, 32768> value{};
    const DWORD length = GetEnvironmentVariableW(
        name,
        value.data(),
        static_cast<DWORD>(value.size())
    );
    if (length > 0 && length < value.size())
    {
        AppendCandidateAndParents(output, value.data());
    }
}

}

ScriptGuiCoreModule::ScriptGuiCoreModule(HMODULE moduleHandle)
    : moduleHandle_(moduleHandle)
{
}

ScriptGuiCoreModule::~ScriptGuiCoreModule()
{
    Shutdown();
}

std::string_view ScriptGuiCoreModule::Id() const
{
    return "script_gui";
}

int ScriptGuiCoreModule::Priority() const
{
    return 100;
}

bool ScriptGuiCoreModule::Initialize(
    core::Services& services,
    std::string& error
)
{
    if (initialized_)
    {
        error.clear();
        return true;
    }
    diagnostic_ = services.diagnostic;
    core::HookDefinition d3dHook;
    d3dHook.id = "windows.d3d9";
    d3dHook.priority = 100;
    d3dHook.install = [](std::string& hookError)
    {
        return InstallGuiD3D9Hooks(hookError);
    };
    d3dHook.uninstall = []
    {
        UninstallGuiD3D9Hooks();
    };
    d3dHook.isInstalled = []
    {
        return AreGuiD3D9HooksInstalled();
    };
    d3dHook.maintain = []
    {
        MaintainGuiD3D9Hooks();
    };
    if (!services.hooks.Register(std::move(d3dHook), error))
    {
        return false;
    }

    core::HookDefinition luaHook;
    luaHook.id = "windows.lua51";
    luaHook.priority = 200;
    luaHook.install = [](std::string& hookError)
    {
        return InstallGuiLua51Hooks(hookError);
    };
    luaHook.uninstall = []
    {
        UninstallGuiLua51Hooks();
    };
    luaHook.isInstalled = []
    {
        return AreGuiLua51HooksInstalled();
    };
    if (!services.hooks.Register(std::move(luaHook), error))
    {
        return false;
    }

    initialized_ = true;
    error.clear();
    return true;
}

void ScriptGuiCoreModule::OnLifecycleEvent(
    const core::LifecycleEvent& event
)
{
    GuiLuaBridgeService& service = GetGuiLuaBridgeService();
    if (event.reason
        == core::LifecycleEventReason::RuntimeStarted)
    {
        service.ResetGameplayLifecycle();
        return;
    }
    if (event.reason
        == core::LifecycleEventReason::RuntimeStopping)
    {
        GetGuiLuaNativeBinding().ResetChannelOwnership();
        service.ResetGameplayLifecycle();
        return;
    }
    if (event.reason == core::LifecycleEventReason::SaveLoaded)
    {
        GetGuiLuaNativeBinding().ResetChannelOwnership();
        service.ResetGameplayLifecycle();
    }

    if (event.current.phase == core::GamePhase::Frontend)
    {
        const GuiGameplayLifecycleSnapshot previous =
            service.GameplayLifecycle();
        if (previous.state != GuiGameplayLifecycleState::Frontend)
        {
            GetGuiLuaNativeBinding().ResetChannelOwnership();
        }
        service.ReportGameplayPlayerTag("---");
    }
    else if (event.current.phase == core::GamePhase::Gameplay)
    {
        service.ReportGameplayPlayerTag(event.current.playerTag);
    }
}

void ScriptGuiCoreModule::Tick(uint64_t)
{
}

void ScriptGuiCoreModule::Shutdown()
{
    if (host_)
    {
        host_->Shutdown();
        host_.reset();
    }
    initialized_ = false;
}

bool ScriptGuiCoreModule::SetRoot(const wchar_t* root)
{
    if (host_ && host_->IsInitialized())
    {
        lastError_ =
            "Scripted GUI root cannot change after initialization";
        return false;
    }
    configuredRoot_ = root
        ? std::filesystem::path(root)
        : std::filesystem::path{};
    if (!configuredRoot_.empty())
    {
        SetGuiDiagnosticsRoot(configuredRoot_);
    }
    return true;
}

bool ScriptGuiCoreModule::AttachDevice(IDirect3DDevice9* device)
{
    return EnsureAttached(device);
}

bool ScriptGuiCoreModule::AttachLua51(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1* api
)
{
    if (!api)
    {
        lastError_ = "Lua 5.1 API table is missing";
        return false;
    }
    if (!GetGuiLuaNativeBinding().Install(
            state,
            *api,
            GetGuiLuaBridgeService(),
            lastError_
        ))
    {
        return false;
    }
    lastError_.clear();
    WriteGuiDiagnostic("Lua 5.1 native bridge attached explicitly");
    return true;
}

bool ScriptGuiCoreModule::IsLuaAttached() const
{
    return GetGuiLuaNativeBinding().IsInstalled();
}

void ScriptGuiCoreModule::OnEndScene(IDirect3DDevice9* device)
{
    if (EnsureAttached(device))
    {
        host_->TickAndRender(device);
    }
}

void ScriptGuiCoreModule::OnBeforeReset()
{
    if (host_)
    {
        host_->BeforeDeviceReset();
    }
}

bool ScriptGuiCoreModule::OnAfterReset(
    IDirect3DDevice9* device,
    HRESULT resetResult
)
{
    return SUCCEEDED(resetResult)
        && host_
        && host_->AfterDeviceReset(device, lastError_);
}

bool ScriptGuiCoreModule::HandleWindowMessage(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    return host_
        && host_->HandleWindowMessage(
            window,
            message,
            wParam,
            lParam
        );
}

const std::string& ScriptGuiCoreModule::LastError() const
{
    return lastError_;
}

bool ScriptGuiCoreModule::EnsureAttached(IDirect3DDevice9* device)
{
    if (host_ && host_->IsInitialized())
    {
        if (host_->UsesDevice(device))
        {
            return true;
        }
        WriteGuiDiagnostic(
            "D3D9 presentation device changed; rebuilding GUI host"
        );
        host_->Shutdown();
        host_.reset();
    }
    const std::filesystem::path root = ResolveRoot();
    if (root.empty())
    {
        lastError_ = "Unable to locate the Scripted GUI project root";
        if (lastLoggedError_ != lastError_)
        {
            WriteGuiDiagnostic(lastError_);
            lastLoggedError_ = lastError_;
        }
        return false;
    }
    auto host = std::make_unique<GuiD3D9Host>();
    if (!host->Initialize(root, device, lastError_))
    {
        if (lastLoggedError_ != lastError_)
        {
            WriteGuiDiagnostic(
                "D3D9 host initialization failed: " + lastError_
            );
            lastLoggedError_ = lastError_;
        }
        return false;
    }
    host_ = std::move(host);
    SetGuiDiagnosticsRoot(root);
    WriteGuiDiagnostic("D3D9 Scripted GUI module initialized");
    lastError_.clear();
    lastLoggedError_.clear();
    return true;
}

std::filesystem::path ScriptGuiCoreModule::ResolveRoot() const
{
    if (!configuredRoot_.empty()
        && IsScriptGuiRoot(configuredRoot_))
    {
        return configuredRoot_;
    }

    std::vector<std::filesystem::path> candidates;
    AppendEnvironmentRoot(candidates, L"NEW_CORE_ROOT");
    AppendEnvironmentRoot(candidates, L"SCRIPTED_GUI_ROOT");

    std::array<wchar_t, 32768> modulePath{};
    const DWORD moduleLength = GetModuleFileNameW(
        moduleHandle_,
        modulePath.data(),
        static_cast<DWORD>(modulePath.size())
    );
    if (moduleLength > 0 && moduleLength < modulePath.size())
    {
        AppendCandidateAndParents(
            candidates,
            std::filesystem::path(modulePath.data()).parent_path()
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
        if (IsScriptGuiRoot(candidate))
        {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
    }
    return {};
}
