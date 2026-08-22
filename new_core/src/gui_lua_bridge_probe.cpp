#include <iostream>
#include <memory>
#include <string>

#include "gui_data_bridge.h"
#include "gui_data_provider.h"
#include "gui_lua_bridge.h"

int main()
{
    GuiLuaBridgeService service;
    const GuiGameplayLifecycleSnapshot initialLifecycle =
        service.GameplayLifecycle();
    if (initialLifecycle.state
            != GuiGameplayLifecycleState::Unknown
        || !service.ReportGameplayPlayerTag("---"))
    {
        std::cerr << "Lua bridge frontend lifecycle failed\n";
        return 1;
    }
    const GuiGameplayLifecycleSnapshot frontendLifecycle =
        service.GameplayLifecycle();
    if (frontendLifecycle.state
            != GuiGameplayLifecycleState::Frontend
        || !service.ReportGameplayPlayerTag("chi"))
    {
        std::cerr << "Lua bridge gameplay lifecycle failed\n";
        return 1;
    }
    const GuiGameplayLifecycleSnapshot gameplayLifecycle =
        service.GameplayLifecycle();
    if (gameplayLifecycle.state
            != GuiGameplayLifecycleState::Gameplay
        || gameplayLifecycle.playerTag != "CHI"
        || gameplayLifecycle.generation
            <= frontendLifecycle.generation)
    {
        std::cerr << "Lua bridge lifecycle snapshot failed\n";
        return 1;
    }
    GuiDataBridgeChannelRegistry channels;
    GuiDataProviderRegistry providers;
    if (!RegisterGuiLuaDataBridgeChannel(channels, service)
        || !RegisterGuiBridgeDataProvider(providers, channels))
    {
        std::cerr << "Lua bridge registration failed\n";
        return 1;
    }

    GuiDataProviderCreateContext createContext;
    createContext.options["channel"] = "lua";
    createContext.options["bridge_name"] = "probe";
    std::unique_ptr<IGuiDataProvider> provider = providers.Create(
        "bridge",
        createContext
    );

    GuiDataBridgeUpdate snapshot;
    snapshot.revision = 1;
    snapshot.fullSnapshot = true;
    snapshot.values["state.visible"] = true;
    snapshot.values["counter"] = int64_t{2};
    std::string error;
    if (!service.PublishUpdate("PROBE", std::move(snapshot), error)
        || !provider
        || !provider->Initialize(
            GuiDataProviderInitContext{std::filesystem::current_path()},
            error
        )
        || provider->Registry()->ResolveNumber("counter") != 2.0)
    {
        std::cerr << "Lua snapshot publication failed: " << error << '\n';
        return 1;
    }

    GuiActionContext action;
    action.action = "activate_item";
    action.functionName = "ProbeGui.ActivateItem";
    action.phase = "click";
    action.windowName = "probe_window";
    action.widgetName = "probe_button";
    action.listItemId = 17;
    action.hasListItemId = true;
    if (provider->HandleAction(action, error)
            != GuiDataProviderActionResult::Handled)
    {
        std::cerr << "Lua action publication failed: " << error << '\n';
        return 1;
    }

    GuiActionContext received;
    if (!service.TryPopAction("probe", received)
        || received.functionName != "ProbeGui.ActivateItem"
        || !received.hasListItemId
        || received.listItemId != 17)
    {
        std::cerr << "Lua action payload was not preserved\n";
        return 1;
    }

    GuiDataBridgeUpdate delta;
    delta.revision = 2;
    delta.baseRevision = 1;
    delta.values["counter"] = int64_t{5};
    if (!service.PublishUpdate("probe", std::move(delta), error)
        || provider->Tick(1, error)
            != GuiDataProviderUpdateResult::Changed
        || provider->Registry()->ResolveNumber("counter") != 5.0)
    {
        std::cerr << "Lua delta publication failed: " << error << '\n';
        return 1;
    }

    const GuiLuaBridgeStats stats = service.Stats("probe");
    if (!stats.consumerOpen
        || stats.pendingUpdates != 0
        || stats.pendingActions != 0)
    {
        std::cerr << "Lua bridge statistics failed\n";
        return 1;
    }

    std::cout
        << "Lua bridge counter: "
        << provider->Registry()->ResolveNumber("counter") << '\n'
        << "Lua action: " << received.functionName << '\n';
    provider->Shutdown();
    return 0;
}
