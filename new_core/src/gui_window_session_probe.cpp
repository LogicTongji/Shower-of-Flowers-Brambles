#include "gui_window_session.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
        input / "new_core" / "tests" / "fixtures" / "gui_interpreter";
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
        data->Set("state.sessionid", sessionId);
        data->Set("state.persistencekey", persistenceKey);
        data->Set(
            "state.persistenceavailable",
            authoritativePersistence
        );
        data->Set("selected.id", selectedId);
        data->Set("title", "Session Probe");
        data->Set("progress", progress);
        data->Set("probe.drag.value", 5.0);
		data->Set("state.rotation", 75.0);
		data->Set("state.scale_x", 2.0);
		data->Set("state.scale_y", 0.75);
		data->Set("state.effect_time", 250.0);
		data->Set("probe.filter", "probe");
		data->Set("probe.static.title", "Dynamic Override");
		GuiListModel dynamicStaticList;
		dynamicStaticList.revision = 99;
		dynamicStaticList.items.push_back({99, "Dynamic Override"});
		data->SetList("probe_static_list", std::move(dynamicStaticList));

        GuiListModel list;
        for (uint64_t itemId = 1; itemId <= 8; ++itemId)
        {
            GuiListItem item;
            item.id = 100 + itemId;
            item.text = "Item " + std::to_string(itemId);
            item.fields["region"] =
                std::string("region_") + std::to_string(itemId);
            item.fields["sprite"] = "GFX_probe_panel";
			item.fields["tag"] = "probe";
            list.items.push_back(std::move(item));
            data->Set(
                "items." + std::to_string(100 + itemId) + ".label",
                "Resolved " + std::to_string(100 + itemId)
            );
            data->Set(
                "items." + std::to_string(100 + itemId) + ".visible",
                true
            );
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
    bool authoritativePersistence = false;
    double selectedId = 0.0;
    std::string sessionId = "probe_session_1";
    std::string persistenceKey = "probe_save_1";
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
                && widget.listIndex == 0
                && widget.definition
                && widget.definition->name == "probe_list_item";
        }
    );
    return found == widgets.end() ? nullptr : &*found;
}

const gui::GuiResolvedWidget* FindListWidget(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    std::string_view name,
    int listIndex
)
{
    const auto found = std::find_if(
        widgets.begin(),
        widgets.end(),
        [name, listIndex](const gui::GuiResolvedWidget& widget)
        {
            return widget.listIndex == listIndex
                && widget.definition
                && widget.definition->name == name;
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
    const std::filesystem::path persistenceRoot =
        std::filesystem::temp_directory_path()
        / ("gui_window_session_probe_" + std::to_string(
            std::chrono::steady_clock::now()
                .time_since_epoch().count()
        ));
    session.SetPersistenceStore(
        std::make_shared<GuiPersistenceStore>(persistenceRoot)
    );

    int dataChangedCount = 0;
    int sessionChangedCount = 0;
    int eventResolveCount = 0;
    std::vector<bool> visibilityChanges;
    session.SetDataChangedCallback(
        [&dataChangedCount]()
        {
            ++dataChangedCount;
        }
    );
    session.SetSessionChangedCallback(
        [&sessionChangedCount](std::string_view, std::string_view)
        {
            ++sessionChangedCount;
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
	const GuiListModel* staticList = session.DataRegistry()->FindList(
		"probe_static_list"
	);
    const GuiListRuntimeLayout listLayout =
        session.BuildListRuntimeLayout("probe_list");
    if (!list
        || list->size() != 8
        || listLayout.items.size() != 8
        || listLayout.maximumScroll <= 0
		|| !staticList
		|| staticList->revision != 7
		|| staticList->items.size() != 1
		|| session.DataRegistry()->ResolveText("probe.static.title")
			!= "Static Probe"
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
    const gui::GuiResolvedWidget* firstIcon = FindListWidget(
        widgets,
        "probe_list_icon",
        0
    );
    const gui::GuiResolvedWidget* firstLabel = FindListWidget(
        widgets,
        "probe_list_label",
        0
    );
    const gui::GuiResolvedWidget* firstNested = FindListWidget(
        widgets,
        "probe_list_nested",
        0
    );
	const auto animated = std::find_if(
		widgets.begin(),
		widgets.end(),
		[](const gui::GuiResolvedWidget& widget)
		{
			return widget.definition
				&& widget.definition->name == "probe_animated_icon";
		}
	);
    gui::GuiTextCommand labelText;
    gui::GuiTextCommand tooltipText;
    if (!firstItem
        || !firstIcon
        || !firstLabel
        || !firstNested
		|| animated == widgets.end()
		|| std::abs(animated->transform.rotationDegrees - 75.0f) > 0.0001f
		|| std::abs(animated->transform.scaleX - 2.0f) > 0.0001f
		|| std::abs(animated->transform.scaleY - 0.75f) > 0.0001f
		|| !animated->transform.flipX
		|| session.ResolveWidgetEffect(*animated) != "GFX_probe_pulse"
        || firstLabel->rect.x != firstItem->rect.x + 28
        || firstNested->rect.y != firstItem->rect.y + 4
        || !firstLabel->hasClipRect
        || firstLabel->clipRect.x != listLayout.viewport.x
        || firstLabel->clipRect.y != listLayout.viewport.y
        || firstLabel->clipRect.width != listLayout.viewport.width
        || firstLabel->clipRect.height != listLayout.viewport.height
        || std::abs(firstItem->opacity - 0.2f) > 0.0001f
        || std::abs(firstLabel->opacity - 0.1f) > 0.0001f
        || session.ResolveWidgetSprite(*firstIcon) != "GFX_probe_panel"
        || !session.ResolveWidgetText(*firstLabel, labelText)
        || labelText.text != "Resolved 101"
        || !session.ResolveWidgetTooltip(*firstItem, tooltipText)
        || tooltipText.text != "region_1"
        || tooltipText.rect.width != 144
        || tooltipText.rect.height != 48
        || tooltipText.font != "probe_font"
        || tooltipText.fontSize != 13
        || tooltipText.lineSpacing != 2
        || tooltipText.color[0] != 0.90f
        || !tooltipText.wrap)
    {
        std::cerr << "Resolved list template scene failed\n";
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

    const int nestedX = firstNested->rect.x + 2;
    const int nestedY = firstNested->rect.y + 2;
    session.DispatchPress(widgets, nestedX, nestedY);
    if (session.DispatchRelease(widgets, nestedX, nestedY) != 1
        || plugin.actionCount != 2
        || plugin.lastAction.widgetName != "probe_list_nested"
        || plugin.lastAction.listItemId != 101)
    {
        std::cerr << "Nested list control dispatch failed\n";
        return 1;
    }

    const gui::GuiResolvedWidget* clippedItem = FindListWidget(
        widgets,
        "probe_list_item",
        4
    );
    if (!clippedItem)
    {
        std::cerr << "Partially clipped list item is missing\n";
        return 1;
    }
    const gui::GuiResolvedWidget* clippedHit = gui::HitTestGuiWidgets(
        widgets,
        clippedItem->rect.x + 2,
        listLayout.viewport.y + listLayout.viewport.height + 1
    );
    if (clippedHit && clippedHit->listIndex == 4)
    {
        std::cerr << "List viewport clipping failed\n";
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
    const int closedTickCount = plugin.tickCount;
    if (session.IsOpen()
        || session.IsVisible()
        || !session.Tick(2000)
        || plugin.tickCount != closedTickCount + 1
        || session.IsOpen()
        || session.IsVisible())
    {
        std::cerr << "Session close failed\n";
        return 1;
    }
    session.OpenWindow();
    if (!session.IsOpen()
        || !session.IsVisible()
        || session.DataRegistry()->ResolveNumber("selected.id") != 101)
    {
        std::cerr << "Session reopen persistence failed\n";
        return 1;
    }

    plugin.sessionId = "probe_session_2";
    plugin.persistenceKey = "probe_save_2";
    session.RefreshData();
    const GuiListRuntimeState* resetListState =
        session.ListRuntimeStore().Find("probe_list");
    if (session.SessionId() != "probe_session_2"
        || sessionChangedCount != 1
        || session.DataRegistry()->ResolveNumber("selected.id") != 0
        || (resetListState && resetListState->selectedItemId != 0))
    {
        std::cerr << "Session generation reset failed\n";
        return 1;
    }

    plugin.sessionId = "probe_session_3";
    plugin.persistenceKey = "probe_save_1";
    session.RefreshData();
    const GuiListRuntimeState* restoredListState =
        session.ListRuntimeStore().Find("probe_list");
    if (session.DataRegistry()->ResolveNumber("selected.id") != 101
        || !restoredListState
        || restoredListState->selectedItemId != 101
        || restoredListState->scrollOffset <= 0
        || !session.PersistenceError().empty())
    {
        std::cerr << "Session save-profile restore failed: "
                  << session.PersistenceError() << '\n';
        return 1;
    }

    plugin.authoritativePersistence = true;
    plugin.selectedId = 404;
    plugin.sessionId = "probe_session_4";
    session.RefreshData();
    if (session.DataRegistry()->ResolveNumber("selected.id") != 404)
    {
        std::cerr << "Authoritative persistence override failed\n";
        return 1;
    }

    plugin.authoritativePersistence = false;
    plugin.selectedId = 505;
    plugin.sessionId = "probe_session_5";
    session.RefreshData();
    if (session.DataRegistry()->ResolveNumber("selected.id") != 505)
    {
        std::cerr << "Authoritative persistence cleanup failed\n";
        return 1;
    }
    session.Shutdown();
    if (plugin.initialized || plugin.shutdownCount != 1)
    {
        std::cerr << "Session shutdown failed\n";
        return 1;
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(persistenceRoot, cleanupError);

    std::cout
        << "Session data refreshes: " << dataChangedCount << '\n'
        << "Session actions: " << plugin.actionCount << '\n'
        << "Session visibility transitions: "
        << visibilityChanges.size() << '\n';
    return 0;
}
