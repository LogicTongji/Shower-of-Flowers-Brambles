#include "leader_capture_core_module.h"

#include <string>
#include <utility>

#include "leader_capture_engine.h"

std::string_view LeaderCaptureCoreModule::Id() const
{
    return "leader_capture";
}

int LeaderCaptureCoreModule::Priority() const
{
    return 200;
}

bool LeaderCaptureCoreModule::Initialize(
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
    core::HookDefinition hook;
    hook.id = "hoi3.leader_capture";
    hook.priority = 300;
    hook.install = [this](std::string& hookError)
    {
        const bool installed = leader_capture::InstallHooks(
            hookError
        );
        if (installed)
        {
            if (diagnostic_)
            {
                diagnostic_("Leader Capture hooks installed");
            }
            lastHookError_.clear();
        }
        else if (diagnostic_ && hookError != lastHookError_)
        {
            diagnostic_(
                "Leader Capture hook installation failed: "
                + hookError
            );
            lastHookError_ = hookError;
        }
        return installed;
    };
    hook.uninstall = [this]
    {
        leader_capture::SetGameplayActive(false);
        leader_capture::UninstallHooks();
        if (diagnostic_)
        {
            diagnostic_("Leader Capture hooks uninstalled");
        }
    };
    hook.isInstalled = []
    {
        return leader_capture::AreHooksInstalled();
    };

    if (!services.hooks.Register(std::move(hook), error))
    {
        return false;
    }

    leader_capture::SetGameplayActive(false);
    initialized_ = true;
    error.clear();
    return true;
}

void LeaderCaptureCoreModule::OnLifecycleEvent(
    const core::LifecycleEvent& event
)
{
    const bool resetRequired = event.playerChanged
        || event.exitedGameplay
        || event.reason == core::LifecycleEventReason::SaveLoaded
        || event.reason
            == core::LifecycleEventReason::RuntimeStopping;
    if (resetRequired)
    {
        leader_capture::SetGameplayActive(false);
        leader_capture::ResetSessionState();
    }

    const bool gameplay = event.current.runtimeActive
        && event.current.phase == core::GamePhase::Gameplay;
    leader_capture::SetGameplayActive(gameplay);
}

void LeaderCaptureCoreModule::Tick(uint64_t nowMilliseconds)
{
    leader_capture::Tick(nowMilliseconds);
}

void LeaderCaptureCoreModule::Shutdown()
{
    leader_capture::SetGameplayActive(false);
    leader_capture::ResetSessionState();
    lastHookError_.clear();
    initialized_ = false;
}
