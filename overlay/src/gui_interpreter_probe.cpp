#include "gui_interpreter.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "gui_behavior.h"
#include "gui_data.h"
#include "gui_runtime.h"

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

int CountWidgets(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    gui::WidgetType type,
    bool visible
)
{
    return static_cast<int>(std::count_if(
        widgets.begin(),
        widgets.end(),
        [type, visible](const gui::GuiResolvedWidget& widget)
        {
            return widget.definition
                && widget.definition->type == type
                && widget.visible == visible;
        }
    ));
}

}

int main(int argc, char** argv)
{
    const std::filesystem::path fixtureRoot =
        ResolveFixtureRoot(argc, argv);

    gui::GuiInterpreter interpreter;
    std::string error;
    if (!interpreter.LoadDirectory(fixtureRoot, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    const gui::SpriteResource* sprite =
        interpreter.FindSprite("GFX_probe_panel");
    const gui::ProgressBarResource* progress =
        interpreter.FindProgressBar("probe_progress_resource");
    const gui::IndexedMapResource* indexedMap =
        interpreter.FindIndexedMap("GFX_probe_indexed_map");
    const gui::WindowDefinition* window =
        interpreter.FindWindow("probe_window");
    if (!sprite || !progress || !indexedMap || !window
        || window->frameZOrder != 250)
    {
        std::cerr << "Interpreter resource registration failed\n";
        return 1;
    }

    if (indexedMap->sourceDefinitionFile != "map\\definition.csv"
        || indexedMap->sourceProvinceFile != "map\\provinces.bmp"
        || indexedMap->sourceGroupFile != "map\\groups.txt"
        || indexedMap->sourceItems.size() != 2
        || indexedMap->sourceItems[0].id != 7
        || indexedMap->sourceItems[0].name != "first_group"
        || indexedMap->cropPadding != 8
        || indexedMap->flipVertical)
    {
        std::cerr << "Indexed-map build configuration failed\n";
        return 1;
    }

    GuiDataRegistry data;
    data.Set("state.visible", true);
    data.Set("title", "Probe Title");
    data.Set("progress", 0.5);
    GuiListModel list;
    list.items.push_back({11, "First"});
    list.items.push_back({22, "Second"});
    data.SetList("probe_list", list);
	GuiListModel polarList;
	for (uint64_t itemId = 1; itemId <= 6; ++itemId)
	{
		polarList.items.push_back({itemId, {}});
	}
	data.SetList("probe_polar_list", polarList);
	data.Set("probe.drag.value", 5.0);
    const gui::GuiLayoutContext context = data.MakeLayoutContext();

    const std::vector<gui::GuiResolvedWidget> visibleLayout =
        interpreter.ResolveWindowLayout("probe_window", context);
    if (CountWidgets(
            visibleLayout,
            gui::WidgetType::IndexedMap,
            true
        ) != 1
        || CountWidgets(
            visibleLayout,
            gui::WidgetType::ProgressBar,
            true
        ) != 1)
    {
        std::cerr << "Visible layout resolution failed\n";
        return 1;
    }

    const std::vector<gui::GuiTextCommand> textCommands =
        interpreter.BuildTextCommands("probe_window", context);
    const std::vector<gui::GuiTextCommand> listTextCommands =
        interpreter.BuildListTextCommands(
            "probe_window",
            "probe_list",
            context
        );
    if (textCommands.size() != 1
        || textCommands.front().text != "Probe Title"
        || listTextCommands.size() != 2)
    {
        std::cerr << "Declarative text resolution failed\n";
        return 1;
    }

    gui::GuiListBinding binding;
    const std::vector<gui::GuiListItemLayout> listItems =
        interpreter.InstantiateListItems(
            "probe_window",
            "probe_list",
            3,
            context
        );
    if (listItems.size() != 3
        || !interpreter.ResolveListBinding(
            "probe_window",
            "probe_list",
            binding,
            context
        )
        || !binding.valid
        || binding.templateName != "probe_list_item"
        || binding.scrollbarName != "probe_scrollbar")
    {
        std::cerr << "Declarative list binding failed\n";
        return 1;
    }

	const std::vector<gui::GuiListItemLayout> polarItems =
		interpreter.InstantiateListItems(
			"probe_window",
			"probe_polar_list",
			polarList.size(),
			context
		);
	if (polarItems.size() != polarList.size()
		|| polarItems.front().rect.x >= polarItems.back().rect.x
		|| polarItems.front().rect.y != polarItems.back().rect.y)
	{
		std::cerr << "Polar list layout failed\n";
		return 1;
	}

	const std::vector<gui::GuiResolvedWidget> dragLayout =
		interpreter.ResolveWindowLayout("probe_window", context);
	const auto dragWidget = std::find_if(
		dragLayout.begin(),
		dragLayout.end(),
		[](const gui::GuiResolvedWidget& widget)
		{
			return widget.definition
				&& widget.definition->name == "probe_drag_cursor";
		}
	);
	if (dragWidget == dragLayout.end()
		|| dragWidget->rect.x != 105)
	{
		std::cerr << "Drag-bound layout failed\n";
		return 1;
	}

	GuiRuntimeInputState dragInput;
	GuiEventRouter eventRouter;
	const std::vector<GuiActionEvent> pressEvents =
		eventRouter.ProcessPress(
			dragLayout,
			dragInput,
			dragWidget->rect.x + 5,
			dragWidget->rect.y + 5
		);
	const std::vector<GuiActionEvent> dragEvents =
		eventRouter.ProcessDragMove(
			dragLayout,
			dragInput,
			160,
			dragWidget->rect.y + 5
		);
	const std::vector<GuiActionEvent> releaseEvents =
		eventRouter.ProcessRelease(
			dragLayout,
			dragInput,
			160,
			dragWidget->rect.y + 5
		);
	if (pressEvents.size() != 2
		|| pressEvents[0].phase != GuiActionPhase::Press
		|| pressEvents[1].phase != GuiActionPhase::Drag
		|| dragEvents.size() != 2
		|| dragEvents[0].phase != GuiActionPhase::DragStart
		|| dragEvents[1].phase != GuiActionPhase::Drag
		|| dragEvents[1].parameters.at("stepindex") != "9"
		|| releaseEvents.size() != 1
		|| releaseEvents[0].phase != GuiActionPhase::DragEnd)
	{
		std::cerr << "Continuous drag routing failed\n";
		return 1;
	}

	gui::WidgetDefinition coveredButton;
	coveredButton.type = gui::WidgetType::Button;
	coveredButton.name = "covered_button";
	gui::WidgetDefinition markerLayer;
	markerLayer.type = gui::WidgetType::MarkerLayer;
	markerLayer.name = "marker_layer";
	markerLayer.draggable = true;
	const std::vector<gui::GuiResolvedWidget> markerLayout = {
		{&coveredButton, {10, 10, 80, 40}, true, true},
		{&markerLayer, {0, 0, 200, 200}, true, true}
	};
	const gui::GuiResolvedWidget* markerHit =
		gui::HitTestGuiWidgets(markerLayout, 20, 20);
	if (!markerHit || markerHit->definition != &coveredButton)
	{
		std::cerr << "Marker layer blocked generic input routing\n";
		return 1;
	}

    GuiWindowRuntime runtime;
    if (!runtime.Bind(interpreter, "probe_window")
        || runtime.FindFirstWidgetName(gui::WidgetType::ListBox)
            != "probe_list")
    {
        std::cerr << "Window runtime binding failed\n";
        return 1;
    }

    GuiBehaviorRegistry behaviors;
    if (!behaviors.LoadDirectory(fixtureRoot, error)
        || !behaviors.Find("activate_item"))
    {
        std::cerr << "Behavior registration failed: " << error << '\n';
        return 1;
    }

    data.Set("state.visible", false);
    const std::vector<gui::GuiResolvedWidget> hiddenLayout =
        interpreter.ResolveWindowLayout(
            "probe_window",
            data.MakeLayoutContext()
        );
    if (CountWidgets(
            hiddenLayout,
            gui::WidgetType::IndexedMap,
            false
        ) != 1
        || CountWidgets(
            hiddenLayout,
            gui::WidgetType::ProgressBar,
            false
        ) != 1)
    {
        std::cerr << "Conditional visibility resolution failed\n";
        return 1;
    }

    std::cout
        << "Documents: " << interpreter.Documents().size() << '\n'
        << "Sprites: " << interpreter.Sprites().size() << '\n'
        << "Progress bars: " << interpreter.ProgressBars().size() << '\n'
        << "Indexed maps: " << interpreter.IndexedMaps().size() << '\n'
        << "Windows: " << interpreter.Windows().size() << '\n'
        << "List items: " << listItems.size() << '\n'
		<< "Polar items: " << polarItems.size() << '\n'
		<< "Continuous drag: passed\n"
		<< "Marker input pass-through: passed\n"
        << "Behaviors: " << behaviors.Size() << '\n';
    return 0;
}
