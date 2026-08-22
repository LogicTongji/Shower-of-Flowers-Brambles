#include "core_runtime.h"

#include <utility>

#include "gui_diagnostics.h"
#include "hoi3_lifecycle.h"
#include "leader_capture_core_module.h"
#include "script_gui_core_module.h"

namespace core
{
namespace
{

constexpr uint64_t LifecycleProbeIntervalMilliseconds = 100;

std::string PhaseName(GamePhase phase)
{
    switch (phase)
    {
    case GamePhase::Frontend:
        return "frontend";
    case GamePhase::Gameplay:
        return "gameplay";
    case GamePhase::Unknown:
    default:
        return "unknown";
    }
}

}

bool Runtime::Initialize(HMODULE moduleHandle, std::string& error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return InitializeUnlocked(moduleHandle, error);
}

bool Runtime::InstallHooks(std::string& error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!InitializeUnlocked(moduleHandle_, error))
    {
        return false;
    }
    if (!hooks_.InstallAll(error))
    {
        SetErrorUnlocked(error);
        return false;
    }
    lastError_.clear();
    return true;
}

void Runtime::UninstallHooks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    hooks_.UninstallAll();
}

bool Runtime::AreHooksInstalled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_ && hooks_.AreAllInstalled();
}

void Runtime::Pump()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || terminated_)
    {
        return;
    }
    PumpUnlocked(GetTickCount64());
}

bool Runtime::SetScriptGuiRoot(const wchar_t* root)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string error;
    if (!InitializeUnlocked(moduleHandle_, error))
    {
        return false;
    }
    if (!scriptGui_->SetRoot(root))
    {
        SetErrorUnlocked(scriptGui_->LastError());
        return false;
    }
    lastError_.clear();
    return true;
}

bool Runtime::AttachScriptGuiDevice(IDirect3DDevice9* device)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string error;
    if (!InitializeUnlocked(moduleHandle_, error))
    {
        return false;
    }
    if (!scriptGui_->AttachDevice(device))
    {
        SetErrorUnlocked(scriptGui_->LastError());
        return false;
    }
    lastError_.clear();
    return true;
}

bool Runtime::AttachScriptGuiLua51(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1* api
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string error;
    if (!InitializeUnlocked(moduleHandle_, error))
    {
        return false;
    }
    if (!scriptGui_->AttachLua51(state, api))
    {
        SetErrorUnlocked(scriptGui_->LastError());
        return false;
    }
    lastError_.clear();
    return true;
}

bool Runtime::IsScriptGuiLuaAttached() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_
        && scriptGui_
        && scriptGui_->IsLuaAttached();
}

void Runtime::OnEndScene(IDirect3DDevice9* device)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string error;
    if (!InitializeUnlocked(moduleHandle_, error))
    {
        return;
    }
    PumpUnlocked(GetTickCount64());
    scriptGui_->OnEndScene(device);
    if (!scriptGui_->LastError().empty())
    {
        SetErrorUnlocked(scriptGui_->LastError());
    }
}

void Runtime::OnBeforeReset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_ && scriptGui_)
    {
        scriptGui_->OnBeforeReset();
    }
}

bool Runtime::OnAfterReset(
    IDirect3DDevice9* device,
    HRESULT resetResult
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !scriptGui_)
    {
        return false;
    }
    const bool result = scriptGui_->OnAfterReset(
        device,
        resetResult
    );
    if (!result)
    {
        SetErrorUnlocked(scriptGui_->LastError());
    }
    return result;
}

bool Runtime::HandleWindowMessage(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_
        && scriptGui_
        && scriptGui_->HandleWindowMessage(
            window,
            message,
            wParam,
            lParam
        );
}

bool Runtime::NotifySaveLoaded(
    std::string_view saveKey,
    LifecycleEventSource source
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_
        || !lifecycle_.NotifySaveLoaded(saveKey, source))
    {
        return false;
    }
    DispatchLifecycleUnlocked();
    return true;
}

LifecycleSnapshot Runtime::Lifecycle() const
{
    return lifecycle_.Snapshot();
}

std::vector<std::string> Runtime::ModuleIds() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return modules_.ModuleIds();
}

std::vector<HookStatus> Runtime::HookStatuses() const
{
    return hooks_.Status();
}

std::string Runtime::LastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void Runtime::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_)
    {
        return;
    }
    lifecycle_.Stop();
    DispatchLifecycleUnlocked();
    modules_.ShutdownAll();
    hooks_.UninstallAll();
    initialized_ = false;
    terminated_ = true;
}

bool Runtime::InitializeUnlocked(
    HMODULE moduleHandle,
    std::string& error
)
{
    if (initialized_)
    {
        error.clear();
        return true;
    }
    if (terminated_)
    {
        error = "core_runtime_terminated";
        SetErrorUnlocked(error);
        return false;
    }
    if (moduleHandle)
    {
        moduleHandle_ = moduleHandle;
    }
    if (!moduleHandle_)
    {
        error = "core_module_handle_missing";
        SetErrorUnlocked(error);
        return false;
    }

    auto scriptGui = std::make_unique<ScriptGuiCoreModule>(
        moduleHandle_
    );
    scriptGui_ = scriptGui.get();
    if (!modules_.Register(std::move(scriptGui), error))
    {
        scriptGui_ = nullptr;
        SetErrorUnlocked(error);
        return false;
    }

    auto leaderCapture =
        std::make_unique<LeaderCaptureCoreModule>();
    if (!modules_.Register(std::move(leaderCapture), error))
    {
        scriptGui_ = nullptr;
        SetErrorUnlocked(error);
        return false;
    }

    Services services{
        hooks_,
        lifecycle_,
        [](std::string_view message)
        {
            WriteGuiDiagnostic(std::string(message));
        }
    };
    if (!modules_.InitializeAll(services, error))
    {
        scriptGui_ = nullptr;
        SetErrorUnlocked(error);
        return false;
    }
    lifecycle_.Start();
    initialized_ = true;
    DispatchLifecycleUnlocked();
    WriteGuiDiagnostic("New Core module runtime initialized");
    error.clear();
    lastError_.clear();
    return true;
}

void Runtime::PumpUnlocked(uint64_t nowMilliseconds)
{
    hooks_.Maintain();
    ProbeLifecycleUnlocked(nowMilliseconds);
    DispatchLifecycleUnlocked();
    modules_.Tick(nowMilliseconds);
}

void Runtime::ProbeLifecycleUnlocked(uint64_t nowMilliseconds)
{
    if (nowMilliseconds < nextLifecycleProbeMilliseconds_)
    {
        return;
    }
    nextLifecycleProbeMilliseconds_ = nowMilliseconds
        + LifecycleProbeIntervalMilliseconds;

    const Hoi3LifecycleProbeResult probe = ProbeHoi3Lifecycle();
    if (probe.status
        == Hoi3LifecycleProbeStatus::UnsupportedExecutable)
    {
        if (!lifecycleUnsupportedLogged_)
        {
            lifecycleUnsupportedLogged_ = true;
            WriteGuiDiagnostic(
                "New Core lifecycle probe unavailable: "
                "unsupported executable"
            );
        }
        return;
    }
    if (probe.status == Hoi3LifecycleProbeStatus::Unavailable)
    {
        return;
    }

    const GamePhase phase = probe.status
            == Hoi3LifecycleProbeStatus::Gameplay
        ? GamePhase::Gameplay
        : GamePhase::Frontend;
    if (!lifecycle_.Observe(
            phase,
            probe.playerTag,
            LifecycleEventSource::NativeProbe
        ))
    {
        return;
    }
    const LifecycleSnapshot snapshot = lifecycle_.Snapshot();
    WriteGuiDiagnostic(
        "New Core lifecycle changed: player="
        + snapshot.playerTag
        + ", state=" + PhaseName(snapshot.phase)
        + ", generation="
        + std::to_string(snapshot.generation)
    );
}

void Runtime::DispatchLifecycleUnlocked()
{
    for (const LifecycleEvent& event : lifecycle_.DrainEvents())
    {
        modules_.DispatchLifecycle(event);
    }
}

void Runtime::SetErrorUnlocked(std::string error)
{
    lastError_ = std::move(error);
}

Runtime& GetRuntime()
{
    static Runtime runtime;
    return runtime;
}

}
