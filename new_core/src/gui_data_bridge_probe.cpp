#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "gui_data_bridge.h"
#include "gui_memory_data_bridge.h"

namespace
{

GuiDataBridgeUpdate MakeInitialSnapshot()
{
    GuiDataBridgeUpdate snapshot;
    snapshot.revision = 1;
    snapshot.fullSnapshot = true;
    snapshot.values["state.visible"] = true;
    snapshot.values["title"] = std::string("bridge probe");
    snapshot.values["progress"] = 0.25;

    GuiListModel tasks;
    tasks.revision = 1;
    tasks.items.push_back({101, "first item"});
    tasks.items.push_back({102, "second item"});
    snapshot.lists["tasks"] = std::move(tasks);
    return snapshot;
}

}

int main()
{
    GuiDataBridgeChannelRegistry builtinChannels;
    if (!RegisterBuiltinGuiDataBridgeChannels(builtinChannels)
        || !builtinChannels.HasFactory(" MEMORY "))
    {
        std::cerr << "Built-in bridge channel registration failed\n";
        return 1;
    }

    GuiMemoryDataBridgeChannel* memoryChannel = nullptr;
    GuiDataBridgeChannelRegistry channels;
    if (!channels.RegisterFactory(
            "probe_memory",
            [&memoryChannel](const GuiDataProviderCreateContext&)
                -> std::unique_ptr<IGuiDataBridgeChannel>
            {
                GuiMemoryDataBridgeConfig config;
                config.maxPendingUpdates = 8;
                config.maxPendingActions = 1;
                auto channel =
                    std::make_unique<GuiMemoryDataBridgeChannel>(config);
                memoryChannel = channel.get();
                return channel;
            }
        ))
    {
        std::cerr << "Probe bridge channel registration failed\n";
        return 1;
    }

    GuiDataProviderRegistry providers;
    if (!RegisterGuiBridgeDataProvider(providers, channels))
    {
        std::cerr << "Bridge data provider registration failed\n";
        return 1;
    }

    GuiDataProviderCreateContext createContext;
    createContext.options["CHANNEL"] = "probe_memory";
    createContext.options["MAX_UPDATES_PER_TICK"] = "8";
	createContext.options["BASE_DATA"] =
		"new_core/tests/fixtures/gui_data_bridge/base.txt";
    std::unique_ptr<IGuiDataProvider> provider = providers.Create(
        " BRIDGE ",
        createContext
    );
    auto* bridge = dynamic_cast<GuiBridgeDataProvider*>(provider.get());
    if (!bridge || !memoryChannel)
    {
        std::cerr << "Bridge data provider creation failed\n";
        return 1;
    }

    std::string error;
    const std::filesystem::path root =
        std::filesystem::current_path();
    if (!provider->Initialize(
            GuiDataProviderInitContext{root},
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }

    if (!memoryChannel->PublishUpdate(MakeInitialSnapshot())
        || provider->Tick(100, error)
            != GuiDataProviderUpdateResult::Changed)
    {
        std::cerr << "Bridge full snapshot failed: " << error << '\n';
        return 1;
    }
    const GuiListModel* tasks = provider->Registry()->FindList(
        "tasks"
    );
	const GuiListModel* baseCatalog = provider->Registry()->FindList(
		"base_catalog"
	);
    if (bridge->AppliedRevision() != 1
        || !provider->Registry()->ResolveBool("state.visible")
        || provider->Registry()->ResolveText("title") != "bridge probe"
        || provider->Registry()->ResolveNumber("progress") != 0.25
        || !tasks
        || tasks->items.size() != 2
		|| tasks->items[1].id != 102
		|| provider->Registry()->ResolveText("bridge.base.label")
			!= "base data"
		|| !baseCatalog
		|| baseCatalog->revision != 3
		|| baseCatalog->items.size() != 1
		|| baseCatalog->items[0].id != 7)
    {
        std::cerr << "Bridge snapshot contents are invalid\n";
        return 1;
    }

    GuiDataBridgeUpdate stale;
    stale.revision = 1;
    stale.baseRevision = 0;
    stale.fullSnapshot = true;
    stale.values["progress"] = 0.1;

    GuiDataBridgeUpdate delta;
    delta.revision = 2;
    delta.baseRevision = 1;
    delta.values["progress"] = 0.75;
    delta.values["selected_item"] = int64_t{102};
    delta.removedValues.push_back("title");
    if (!memoryChannel->PublishUpdate(std::move(stale))
        || !memoryChannel->PublishUpdate(std::move(delta))
        || provider->Tick(200, error)
            != GuiDataProviderUpdateResult::Changed
        || bridge->AppliedRevision() != 2
        || provider->Registry()->ResolveNumber("progress") != 0.75
        || provider->Registry()->ResolveNumber("selected_item")
            != 102.0
        || provider->Registry()->Find("title"))
    {
        std::cerr << "Bridge delta application failed: "
                  << error << '\n';
        return 1;
    }

    GuiDataBridgeUpdate validBeforeGap;
    validBeforeGap.revision = 3;
    validBeforeGap.baseRevision = 2;
    validBeforeGap.values["progress"] = 0.9;

    GuiDataBridgeUpdate revisionGap;
    revisionGap.revision = 4;
    revisionGap.baseRevision = 99;
    revisionGap.values["progress"] = 1.0;
    if (!memoryChannel->PublishUpdate(std::move(validBeforeGap))
        || !memoryChannel->PublishUpdate(std::move(revisionGap))
        || provider->Tick(300, error)
            != GuiDataProviderUpdateResult::Failed
        || error.find("bridge_revision_gap") == std::string::npos
        || bridge->AppliedRevision() != 2
        || provider->Registry()->ResolveNumber("progress") != 0.75)
    {
        std::cerr << "Bridge atomic gap rejection failed: "
                  << error << '\n';
        return 1;
    }

    GuiDataBridgeUpdate recovery;
    recovery.revision = 5;
    recovery.fullSnapshot = true;
    recovery.values["state.visible"] = false;
    recovery.values["progress"] = 0.4;
    if (!memoryChannel->PublishUpdate(std::move(recovery))
        || provider->Tick(400, error)
            != GuiDataProviderUpdateResult::Changed
        || bridge->AppliedRevision() != 5
        || provider->Registry()->ResolveBool("state.visible")
        || provider->Registry()->ResolveNumber("progress") != 0.4
		|| provider->Registry()->FindList("tasks")
		|| provider->Registry()->ResolveText("bridge.base.label")
			!= "base data"
		|| !provider->Registry()->FindList("base_catalog"))
    {
        std::cerr << "Bridge full snapshot recovery failed: "
                  << error << '\n';
        return 1;
    }

    GuiActionContext action;
    action.action = "activate_item";
    action.fallbackOperation = "send_action";
    action.windowName = "probe_window";
    action.widgetName = "activate_button";
    action.listName = "probe_list";
    action.listIndex = 2;
    action.listItemId = 102;
    action.hasListItemId = true;
    action.mouseX = 320;
    action.mouseY = 480;
    action.parameters["mode"] = "probe_mode";
    if (provider->HandleAction(action, error)
            != GuiDataProviderActionResult::Handled
        || memoryChannel->PendingActionCount() != 1
        || provider->HandleAction(action, error)
            != GuiDataProviderActionResult::Failed
        || error != "memory_bridge_action_queue_full")
    {
        std::cerr << "Bridge action queue handling failed: "
                  << error << '\n';
        return 1;
    }

    GuiActionContext received;
    if (!memoryChannel->TryPopAction(received)
        || received.action != action.action
        || received.windowName != action.windowName
        || received.widgetName != action.widgetName
        || !received.hasListItemId
        || received.listItemId != 102
        || received.mouseX != 320
        || received.mouseY != 480
        || received.parameters["mode"] != "probe_mode")
    {
        std::cerr << "Bridge action payload was not preserved\n";
        return 1;
    }

    std::cout
        << "Bridge revision: " << bridge->AppliedRevision() << '\n'
        << "Bridge progress: "
        << provider->Registry()->ResolveNumber("progress") << '\n'
        << "Bridge action: " << received.action
        << " item=" << received.listItemId
        << " mouse=" << received.mouseX
        << ',' << received.mouseY << '\n';

    provider->Shutdown();
    return 0;
}
