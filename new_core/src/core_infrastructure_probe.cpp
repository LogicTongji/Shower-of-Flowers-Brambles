#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core_hook_registry.h"
#include "core_lifecycle.h"
#include "core_module_registry.h"

namespace
{

class ProbeModule final : public core::IModule
{
public:
    ProbeModule(
        std::string id,
        int priority,
        std::vector<std::string>& log
    )
        : id_(std::move(id)),
          priority_(priority),
          log_(log)
    {
    }

    std::string_view Id() const override
    {
        return id_;
    }

    int Priority() const override
    {
        return priority_;
    }

    bool Initialize(core::Services&, std::string& error) override
    {
        log_.push_back("init:" + id_);
        error.clear();
        return true;
    }

    void OnLifecycleEvent(
        const core::LifecycleEvent& event
    ) override
    {
        log_.push_back(
            "life:" + id_ + ":"
            + std::to_string(static_cast<int>(event.reason))
        );
        if (event.enteredGameplay)
        {
            enteredGameplay_ = true;
        }
        if (event.exitedGameplay)
        {
            exitedGameplay_ = true;
        }
        if (event.reason == core::LifecycleEventReason::SaveLoaded)
        {
            saveLoaded_ = true;
        }
    }

    void Tick(uint64_t) override
    {
        log_.push_back("tick:" + id_);
    }

    void Shutdown() override
    {
        log_.push_back("stop:" + id_);
    }

    bool enteredGameplay_ = false;
    bool exitedGameplay_ = false;
    bool saveLoaded_ = false;

private:
    std::string id_;
    int priority_ = 0;
    std::vector<std::string>& log_;
};

bool CheckHookRegistry()
{
    core::HookRegistry hooks;
    std::vector<std::string> log;
    bool firstInstalled = false;
    bool secondInstalled = false;
    bool secondReady = false;

    auto makeHook = [&](std::string id, int priority, bool& installed)
    {
        core::HookDefinition hook;
        hook.id = std::move(id);
        hook.priority = priority;
        hook.install = [&log, &installed, &secondReady, priority](
            std::string& error
        )
        {
            log.push_back("install:" + std::to_string(priority));
            if (priority == 20 && !secondReady)
            {
                error = "not_ready";
                return false;
            }
            installed = true;
            error.clear();
            return true;
        };
        hook.uninstall = [&log, &installed, priority]
        {
            log.push_back("uninstall:" + std::to_string(priority));
            installed = false;
        };
        hook.isInstalled = [&installed]
        {
            return installed;
        };
        hook.maintain = [&log, priority]
        {
            log.push_back("maintain:" + std::to_string(priority));
        };
        return hook;
    };

    std::string error;
    if (!hooks.Register(
            makeHook("second", 20, secondInstalled),
            error
        )
        || !hooks.Register(
            makeHook("first", 10, firstInstalled),
            error
        ))
    {
        return false;
    }
    core::HookDefinition duplicate = makeHook(
        "first",
        30,
        firstInstalled
    );
    if (hooks.Register(std::move(duplicate), error))
    {
        return false;
    }
    if (hooks.InstallAll(error)
        || !firstInstalled
        || secondInstalled
        || log.size() != 2
        || log[0] != "install:10"
        || log[1] != "install:20")
    {
        return false;
    }
    secondReady = true;
    if (!hooks.InstallAll(error) || !hooks.AreAllInstalled())
    {
        return false;
    }
    hooks.Maintain();
    hooks.UninstallAll();
    return !firstInstalled
        && !secondInstalled
        && log.size() >= 7
        && log[log.size() - 2] == "uninstall:20"
        && log.back() == "uninstall:10";
}

bool CheckLifecycleAndModules()
{
    core::HookRegistry hooks;
    core::LifecycleService lifecycle;
    core::ModuleRegistry modules;
    std::vector<std::string> log;
    auto late = std::make_unique<ProbeModule>("late", 20, log);
    auto early = std::make_unique<ProbeModule>("early", 10, log);
    ProbeModule* latePointer = late.get();
    ProbeModule* earlyPointer = early.get();
    std::string error;
    if (!modules.Register(std::move(late), error)
        || !modules.Register(std::move(early), error))
    {
        return false;
    }
    core::Services services{
        hooks,
        lifecycle,
        [](std::string_view)
        {
        }
    };
    if (!modules.InitializeAll(services, error)
        || log.size() != 2
        || log[0] != "init:early"
        || log[1] != "init:late")
    {
        return false;
    }

    lifecycle.Start();
    lifecycle.Observe(
        core::GamePhase::Gameplay,
        "chi",
        core::LifecycleEventSource::NativeProbe
    );
    lifecycle.NotifySaveLoaded(
        "probe_save",
        core::LifecycleEventSource::External
    );
    lifecycle.Observe(
        core::GamePhase::Frontend,
        "---",
        core::LifecycleEventSource::NativeProbe
    );
    for (const core::LifecycleEvent& event : lifecycle.DrainEvents())
    {
        modules.DispatchLifecycle(event);
    }
    modules.Tick(100);

    const core::LifecycleSnapshot snapshot = lifecycle.Snapshot();
    if (!earlyPointer->enteredGameplay_
        || !latePointer->enteredGameplay_
        || !earlyPointer->exitedGameplay_
        || !latePointer->exitedGameplay_
        || !earlyPointer->saveLoaded_
        || !latePointer->saveLoaded_
        || snapshot.phase != core::GamePhase::Frontend
        || snapshot.saveGeneration != 1)
    {
        return false;
    }

    modules.ShutdownAll();
    return log.size() >= 2
        && log[log.size() - 2] == "stop:late"
        && log.back() == "stop:early";
}

}

int main()
{
    if (!CheckHookRegistry())
    {
        std::cerr << "Core hook registry probe failed\n";
        return 1;
    }
    if (!CheckLifecycleAndModules())
    {
        std::cerr << "Core lifecycle/module probe failed\n";
        return 1;
    }
    std::cout << "Core infrastructure probe: passed\n";
    return 0;
}
