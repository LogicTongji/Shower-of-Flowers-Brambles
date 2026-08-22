#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "core_hook_registry.h"
#include "core_lifecycle.h"
#include "leader_capture_core_module.h"
#include "leader_capture_engine.h"

namespace
{

bool Dispatch(
    core::LifecycleService& lifecycle,
    LeaderCaptureCoreModule& module
)
{
    for (const core::LifecycleEvent& event : lifecycle.DrainEvents())
    {
        module.OnLifecycleEvent(event);
    }
    return true;
}

}

int main()
{
    core::HookRegistry hooks;
    core::LifecycleService lifecycle;
    std::vector<std::string> diagnostics;
    core::Services services{
        hooks,
        lifecycle,
        [&diagnostics](std::string_view message)
        {
            diagnostics.emplace_back(message);
        }
    };

    LeaderCaptureCoreModule module;
    std::string error;
    if (module.Id() != "leader_capture"
        || module.Priority() != 200
        || !module.Initialize(services, error))
    {
        std::cerr << "Leader Capture module initialization failed: "
                  << error << '\n';
        return 1;
    }

    const std::vector<core::HookStatus> statuses = hooks.Status();
    if (statuses.size() != 1
        || statuses.front().id != "hoi3.leader_capture"
        || statuses.front().priority != 300
        || statuses.front().installed)
    {
        std::cerr << "Leader Capture hook registration mismatch\n";
        return 2;
    }

    lifecycle.Start();
    Dispatch(lifecycle, module);
    if (leader_capture::IsGameplayActive())
    {
        std::cerr << "Leader Capture activated before gameplay\n";
        return 3;
    }

    lifecycle.Observe(
        core::GamePhase::Gameplay,
        "CHI",
        core::LifecycleEventSource::NativeProbe
    );
    Dispatch(lifecycle, module);
    if (!leader_capture::IsGameplayActive())
    {
        std::cerr << "Leader Capture did not enter gameplay\n";
        return 4;
    }

    module.Tick(100);
    lifecycle.NotifySaveLoaded(
        "leader_capture_probe_save",
        core::LifecycleEventSource::External
    );
    Dispatch(lifecycle, module);
    if (!leader_capture::IsGameplayActive())
    {
        std::cerr << "Leader Capture did not resume after save load\n";
        return 5;
    }

    lifecycle.Observe(
        core::GamePhase::Frontend,
        "---",
        core::LifecycleEventSource::NativeProbe
    );
    Dispatch(lifecycle, module);
    if (leader_capture::IsGameplayActive())
    {
        std::cerr << "Leader Capture remained active in frontend\n";
        return 6;
    }

    module.Shutdown();
    std::cout << "Leader Capture core module probe: passed\n";
    return 0;
}
