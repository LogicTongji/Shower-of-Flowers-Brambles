#include "gui_window_session.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

std::filesystem::path ResolveFixtureRoot(int argc, char** argv)
{
    const std::filesystem::path input = argc >= 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    const std::filesystem::path projectFixture =
        input / "overlay" / "tests" / "fixtures" / "gui_interpreter";
    return std::filesystem::is_directory(projectFixture)
        ? projectFixture
        : input;
}

class ProbePlugin final : public IGuiPlugin
{
public:
    std::string_view WindowName() const override
    {
        return "probe_window";
    }

    std::string_view WindowTitle() const override
    {
        return "Session Probe";
    }

    uint32_t TickIntervalMilliseconds() const override
    {
        return 25;
    }

    bool Initialize(
        const GuiPluginInitContext& context,
        std::string& error
    ) override
    {
        if (!context.windowRuntime.IsBound()
            || context.windowRuntime.Name() != WindowName())
        {
            error = "Probe plugin received an unbound runtime";
            return false;
        }
        initialized = true;
        return true;
    }

    void Shutdown() override
    {
        initialized = false;
        ++shutdownCount;
    }

    void RegisterCustomWidgets(
        gui::GuiCustomWidgetRegistry&
    ) override
    {
    }

    std::shared_ptr<GuiDataRegistry> BuildDataRegistry() const override
    {
        ++buildCount;
        auto data = std::make_shared<GuiDataRegistry>();
        data->Set("state.visible", visible);
        data->Set("title", "Session Probe");
        data->Set("progress", progress);
        data->Set("probe.drag.value", 5.0);

        GuiListModel list;
        for (uint64_t itemId = 1; itemId <= 8; ++itemId)
        {
            GuiListItem item;
            item.id = 100 + itemId;
            item.text = "Item " + std::to_string(itemId);
            item.fields["region"] =
                std::string("region_") + std::to_string(itemId);
            list.items.push_back(std::move(item));
        }
        data->SetList("probe_list", std::move(list));

        GuiListModel polarList;
        for (uint64_t itemId = 1; itemId <= 6; ++itemId)
        {
            polarList.items.push_back({itemId, {}});
        }
        data->SetList("probe_polar_list", std::move(polarList));
        return data;
    }

    bool Tick(uint64_t) override
    {
        progress = 0.75;
        ++tickCount;
        return true;
    }

    bool HandleAction(const GuiActionContext& context) override
    {
        lastAction = context;
        ++actionCount;
        return false;
    }

    mutable int buildCount = 0;
    int tickCount = 0;
    int actionCount = 0;
    int shutdownCount = 0;
    bool initialized = false;
    bool visible = true;
    double progress = 0.25;
    GuiActionContext lastAction;
};

const gui::GuiResolvedWidget* FindFirstListItem(
    const std::vector<gui::GuiResolvedWidget>& widgets
)
{
    const auto found = std::find_if(
        widgets.begin(),
        widgets.end(),
        [](const gui::GuiResolvedWidget& widget)
        {
            return widget.listName == "probe_list"
                && widget.listIndex == 0;
        }
    );
    return found == widgets.end() ? nullptr : &*found;
}

}

int main(int argc, char** argv)
{
    const std::filesystem::path fixtureRoot =
        ResolveFixtureRoot(argc, argv);

    gui::GuiInterpreter interpreter;
    GuiBehaviorRegistry behaviors;
    std::string error;
    if (!interpreter.LoadDirectory(fixtureRoot, error)
        || !behaviors.LoadDirectory(fixtureRoot, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    ProbePlugin plugin;
    GuiPluginLaunch launch;
    launch.id = "probe_plugin";
    launch.visibleWhen = "state.visible";
    launch.plugin = &plugin;
    GuiWindowSessionController session(
        fixtureRoot,
        launch,
        interpreter,
        &behaviors
    );

    int dataChangedCount = 0;
    int eventResolveCount = 0;
    std::vector<bool> visibilityChanges;
    session.SetDataChangedCallback(
        [&dataChangedCount]()
        {
            ++dataChangedCount;
        }
    );
    session.SetVisibilityChangedCallback(
        [&visibilityChanges](bool visible)
        {
            visibilityChanges.push_back(visible);
        }
    );
    session.SetEventResolver(
        [&eventResolveCount](std::vector<GuiActionEvent>&)
        {
            ++eventResolveCount;
        }
    );

    if (!session.Bind(error)
        || !session.Initialize(nullptr, error)
        || !plugin.initialized
        || !session.IsOpen()
        || !session.IsVisible()
        || session.PluginId() != "probe_plugin"
        || session.WindowName() != "probe_window"
        || dataChangedCount != 1
        || visibilityChanges != std::vector<bool>{true})
    {
        std::cerr << "Session lifecycle initialization failed: "
                  << error << '\n';
        return 1;
    }

    const GuiListModel* list = session.FindListModel("probe_list");
    const GuiListRuntimeLayout listLayout =
        session.BuildListRuntimeLayout("probe_list");
    if (!list
        || list->size() != 8
        || listLayout.items.size() != 8
        || listLayout.maximumScroll <= 0
        || session.ListNames().size() != 2
        || !session.ListTemplateNames().count("probe_list_item"))
    {
        std::cerr << "Session list binding failed\n";
        return 1;
    }

    std::vector<gui::GuiResolvedWidget> widgets =
        session.ResolveInteractiveWidgets();
    const gui::GuiResolvedWidget* firstItem =
        FindFirstListItem(widgets);
    if (!firstItem)
    {
        std::cerr << "Resolved list item is missing\n";
        return 1;
    }
    const int clickX = firstItem->rect.x + 2;
    const int clickY = firstItem->rect.y + 2;
    session.DispatchPress(widgets, clickX, clickY);
    if (session.DispatchRelease(widgets, clickX, clickY) != 1)
    {
        std::cerr << "Session click dispatch failed\n";
        return 1;
    }

    const GuiListRuntimeState* listState =
        session.ListRuntimeStore().Find("probe_list");
    if (!listState
        || listState->selectedItemId != 101
        || session.DataRegistry()->ResolveNumber("selected.id") != 101
        || plugin.actionCount != 1
        || plugin.lastAction.action != "activate_item"
        || plugin.lastAction.functionName != "ProbeGui.ActivateItem"
        || plugin.lastAction.listName != "probe_list"
        || plugin.lastAction.listIndex != 0
        || !plugin.lastAction.hasListItemId
        || plugin.lastAction.listItemId != 101
        || plugin.lastAction.parameters.at("region") != "region_1"
        || eventResolveCount != 1)
    {
        std::cerr << "Session list action context failed\n";
        return 1;
    }

    if (!session.ScrollListAt(
            listLayout.viewport.x + 1,
            listLayout.viewport.y + 1,
            1
        )
        || session.ListRuntimeStore()
                .Find("probe_list")->scrollOffset <= 0)
    {
        std::cerr << "Session list scrolling failed\n";
        return 1;
    }

    session.SetVisibilityMode(GuiWindowVisibilityMode::Hidden);
    session.SetVisibilityMode(GuiWindowVisibilityMode::Automatic);
    if (visibilityChanges != std::vector<bool>({true, false, true}))
    {
        std::cerr << "Session visibility override failed\n";
        return 1;
    }

    const int previousBuildCount = plugin.buildCount;
    if (!session.Tick(1000)
        || plugin.tickCount != 1
        || plugin.buildCount <= previousBuildCount
        || session.DataRegistry()->ResolveNumber("progress") != 0.75
        || session.DataRegistry()->ResolveNumber("selected.id") != 101)
    {
        std::cerr << "Session tick refresh failed\n";
        return 1;
    }

    session.CloseWindow();
    if (session.IsOpen())
    {
        std::cerr << "Session close failed\n";
        return 1;
    }
    session.Shutdown();
    if (plugin.initialized || plugin.shutdownCount != 1)
    {
        std::cerr << "Session shutdown failed\n";
        return 1;
    }

    std::cout
        << "Session data refreshes: " << dataChangedCount << '\n'
        << "Session actions: " << plugin.actionCount << '\n'
        << "Session visibility transitions: "
        << visibilityChanges.size() << '\n';
    return 0;
}
