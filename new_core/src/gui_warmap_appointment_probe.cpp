#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gui_action_bridge.h"
#include "gui_behavior.h"
#include "gui_declarative_data.h"
#include "gui_interpreter.h"
#include "gui_runtime.h"

namespace
{

const gui::WidgetDefinition* FindWidget(
    const gui::WidgetDefinition& root,
    std::string_view name
)
{
    if (root.name == name)
    {
        return &root;
    }
    for (const gui::WidgetDefinition& child : root.children)
    {
        if (const gui::WidgetDefinition* found = FindWidget(child, name))
        {
            return found;
        }
    }
    return nullptr;
}

const gui::GuiResolvedWidget* FindResolved(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    std::string_view name
)
{
    for (const gui::GuiResolvedWidget& widget : widgets)
    {
        if (widget.definition && widget.definition->name == name)
        {
            return &widget;
        }
    }
    return nullptr;
}

bool HasLiteralText(const gui::WidgetDefinition& root)
{
    if (root.type == gui::WidgetType::Text && !root.text.empty())
    {
        return true;
    }
    for (const gui::WidgetDefinition& child : root.children)
    {
        if (HasLiteralText(child))
        {
            return true;
        }
    }
    return false;
}

int FindItemIndex(const GuiListModel& model, uint64_t itemId)
{
    for (std::size_t index = 0; index < model.items.size(); ++index)
    {
        if (model.items[index].id == itemId)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

GuiActionEvent MakeListEvent(
    const gui::GuiResolvedWidget* widget,
    uint64_t itemId,
    std::string action
)
{
    GuiActionEvent event;
    event.widget = widget;
    event.phase = GuiActionPhase::Click;
    event.action = std::move(action);
    event.itemId = itemId;
    event.hasItemId = true;
    return event;
}

bool IsWidgetVisible(
    const gui::GuiInterpreter& interpreter,
    const GuiDataRegistry& data,
    std::string_view name
)
{
    const std::vector<gui::GuiResolvedWidget> widgets =
        interpreter.ResolveWindowLayout(
            "china_anti_jap",
            data.MakeLayoutContext()
        );
    const gui::GuiResolvedWidget* widget = FindResolved(widgets, name);
    return widget && widget->visible;
}

GuiListRuntimeLayout BuildCandidateLayout(
    const gui::GuiInterpreter& interpreter,
    const GuiDataRegistry& data
)
{
    const GuiListModel* candidates = data.FindList(
        "leader_candidate_list"
    );
    return candidates
        ? BuildGuiListRuntimeLayout(
            interpreter,
            "china_anti_jap",
            "leader_candidate_list",
            *candidates,
            {},
            {},
            data.MakeLayoutContext()
        )
        : GuiListRuntimeLayout{};
}

void ApplyWindowStaticData(
	const gui::WindowDefinition& window,
	GuiDataRegistry& registry
)
{
	for (const gui::StaticDataValueDefinition& value : window.staticValues)
	{
		registry.Set(value.name, value.value);
	}
	for (const gui::StaticDataListDefinition& list : window.staticLists)
	{
		registry.SetList(list.name, list.model);
	}
}

}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    const fs::path root = argc > 1
        ? fs::path(argv[1])
        : fs::current_path();
    std::string error;

    gui::GuiInterpreter interpreter;
    if (!interpreter.LoadFile(
            root / "interface" / "china_anti_jap.sgfx",
            error
        )
        || !interpreter.LoadFile(
            root / "interface" / "china_anti_jap.sgui",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }

    const gui::WindowDefinition* window = interpreter.FindWindow(
        "china_anti_jap"
    );
    const gui::WidgetDefinition* map = window
        ? FindWidget(*window, "china_region_map")
        : nullptr;
    const gui::WidgetDefinition* marker = window
        ? FindWidget(*window, "china_war_leader_markers")
        : nullptr;
    const gui::WidgetDefinition* combatButton = window
        ? FindWidget(*window, "combat_region_button")
        : nullptr;
    const gui::WidgetDefinition* candidateButton = window
        ? FindWidget(*window, "leader_candidate_button")
        : nullptr;
    const gui::WidgetDefinition* candidates = window
        ? FindWidget(*window, "leader_candidate_list")
        : nullptr;
    const gui::WidgetDefinition* banner = window
        ? FindWidget(*window, "china_war_banner_title")
        : nullptr;
    const gui::WidgetDefinition* legendTitle = window
        ? FindWidget(*window, "legend_title")
        : nullptr;

    if (!window
        || !map
        || !marker
        || marker->type != gui::WidgetType::MarkerLayer
		|| marker->catalogSource != "leader_candidate_list"
        || marker->markerRect.width != 68
        || marker->markerRect.height != 84
        || marker->portraitRect.width != 64
        || marker->portraitRect.height != 80
        || marker->tooltipPlacement != "right"
        || marker->markerActionName != "step_down_war_map_leader"
        || marker->markerActionRect.width != 80
        || marker->markerActionRect.height != 15
        || marker->markerActionLocalizationKey != "WARMAP_STEP_DOWN"
		|| marker->markerStackSource != "item.regionid"
		|| marker->markerStackOrderSource != "item.assignmentorder"
		|| marker->markerStackDirection != "vertical"
		|| marker->markerStackSpacing != 4
		|| !marker->avoidTooltipOverlap
        || !marker->draggable
        || !combatButton
        || !candidateButton
		|| candidateButton->spriteSource
			!= "leaderbuttons.{state.viewertag}"
        || !candidates
        || candidates->scrollBarName != "leader_candidate_scrollbar"
		|| candidates->itemFilterField != "tag"
		|| candidates->itemFilterValueSource != "state.viewertag"
        || candidates->disabledByListName != "assigned_leader_list"
		|| candidates->disabledMatchField != "leadertype"
		|| candidates->disabledFilterField != "regionid"
		|| candidates->disabledFilterValueSource != "selectedregion.id"
        || !banner
        || banner->rect.y != 12
        || banner->rect.height != 76
        || banner->zOrder <= window->frameZOrder
        || !legendTitle
        || legendTitle->localizationKey != "WARMAP_LEGEND"
        || !legendTitle->wrap
        || FindWidget(*window, "legend_title_top")
        || FindWidget(*window, "legend_title_bottom")
        || HasLiteralText(*window)
        || !interpreter.FindSprite("GFX_warmap_leader_li_zongren")
		|| !interpreter.FindSprite("GFX_warmap_officer_ju_zheng")
        || !interpreter.FindSprite("GFX_warmap_leader_panel")
        || !interpreter.FindSprite("GFX_warmap_leader_button_chi")
        || !interpreter.FindSprite("GFX_warmap_leader_button_chc")
        || !interpreter.FindSprite("GFX_warmap_leader_button_jap")
        || !interpreter.FindSprite("GFX_warmap_step_down"))
    {
        std::cerr << "warmap appointment GUI parsing failed\n";
        return 1;
    }

    GuiDeclarativeDataStore dataStore;
    if (!dataStore.LoadFiles(
            {
                root / "script_gui" / "data"
                    / "china_anti_jap_common.txt",
                root / "snapshots" / "china_war_gui"
                    / "01_static.txt"
            },
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }
	ApplyWindowStaticData(*window, *dataStore.Registry());

    const std::vector<std::pair<int, double>> staticPercentages = {
        {1, 15.0}, {2, 65.0}, {9, 95.0}, {10, 5.0},
        {14, 95.0}, {16, 95.0}, {17, 15.0}, {18, 15.0},
        {19, 95.0}, {20, 35.0}, {21, 15.0}, {22, 95.0},
        {23, 10.0}, {24, 85.0}, {25, 95.0}, {26, 95.0},
        {27, 95.0}, {28, 95.0}, {29, 95.0}, {30, 95.0},
        {31, 95.0}, {32, 95.0}, {33, 95.0}, {34, 95.0},
        {35, 95.0}, {40, 95.0}
    };
    for (const auto& expected : staticPercentages)
    {
        const std::string path = "regions."
            + std::to_string(expected.first)
            + ".controlledPercentage";
        if (std::abs(
                dataStore.Registry()->ResolveNumber(path)
                - expected.second
            ) > 0.0001)
        {
            std::cerr << "static preview percentage mismatch: "
                      << path << '\n';
            return 1;
        }
    }

    const GuiListModel* combatRegions =
        dataStore.Registry()->FindList("combat_region_list");
    const GuiListModel* candidateList =
        dataStore.Registry()->FindList("leader_candidate_list");
    if (!dataStore.Registry()->ResolveBool("state.visible")
        || !dataStore.Registry()->ResolveBool("state.active")
        || !combatRegions
        || combatRegions->items.size() != 9
        || !candidateList
        || candidateList->items.size() != 2
        || candidateList->items[0].Find("portrait") == nullptr
        || GuiDataValueToText(
            *candidateList->items[0].Find("leaderid")
        ) != "li_zongren"
		|| candidateList->items[1].Find("portrait") == nullptr
		|| GuiDataValueToText(
			*candidateList->items[1].Find("leaderid")
		) != "ju_zheng"
		|| candidateList->items[1].Find("leadertype") == nullptr
		|| GuiDataValueToText(
			*candidateList->items[1].Find("leadertype")
		) != "administrative"
		|| GuiDataValueToText(
			*candidateList->items[1].Find("portrait")
		) != "GFX_warmap_officer_ju_zheng"
		|| !candidateList->items[0].Find("tag")
		|| GuiDataValueToText(*candidateList->items[0].Find("tag"))
			!= "CHI"
		|| !candidateList->items[1].Find("tag")
		|| GuiDataValueToText(*candidateList->items[1].Find("tag"))
			!= "CHI")
    {
        std::cerr << "static preview data mismatch\n";
        return 1;
    }
	GuiListModel expandedCandidates = *candidateList;
	GuiListItem secondMilitary = candidateList->items.front();
	secondMilitary.id = 3;
	secondMilitary.fields["leaderid"] = std::string("military_probe");
	expandedCandidates.items.push_back(std::move(secondMilitary));
	++expandedCandidates.revision;
	dataStore.Registry()->SetList(
		"leader_candidate_list",
		std::move(expandedCandidates)
	);
	dataStore.Registry()->Set("state.viewertag", "CHC");
	if (!BuildCandidateLayout(interpreter, *dataStore.Registry()).items.empty())
	{
		std::cerr << "candidate tag filtering failed\n";
		return 1;
	}
	dataStore.Registry()->Set("state.viewertag", "CHI");

    GuiBehaviorRegistry behaviors;
    if (!behaviors.LoadFile(
            root / "script_gui" / "china_anti_jap_warmap.txt",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }
    GuiLuaActionBridge bridge;
    bridge.SetBehaviorRegistry(&behaviors);
    bridge.SetConditionEvaluator(
        [&dataStore](std::string_view expression)
        {
            return dataStore.Registry()->EvaluateCondition(expression);
        }
    );
    bridge.SetFallbackInvoker(
        [&dataStore](const GuiActionContext& context)
        {
            return dataStore.ApplyAction(context);
        }
    );

    const int hubeiIndex = FindItemIndex(*combatRegions, 24);
    const int henanIndex = FindItemIndex(*combatRegions, 23);
    if (hubeiIndex < 0 || henanIndex < 0)
    {
        std::cerr << "combat region test data missing\n";
        return 1;
    }

    gui::GuiResolvedWidget mapWidget;
    mapWidget.definition = map;
    gui::GuiResolvedWidget combatWidget;
    combatWidget.definition = combatButton;
    combatWidget.listName = "combat_region_list";
    gui::GuiResolvedWidget candidateWidget;
    candidateWidget.definition = candidateButton;
    candidateWidget.listName = "leader_candidate_list";
    candidateWidget.listIndex = 0;
    gui::GuiResolvedWidget markerWidget;
    markerWidget.definition = marker;
    markerWidget.listName = "assigned_leader_list";
    markerWidget.listIndex = 0;

    const GuiActionEvent mapSelection = MakeListEvent(
        &mapWidget,
        24,
        "select_war_map_region"
    );
    if (!bridge.Dispatch("china_anti_jap", mapSelection)
        || dataStore.Registry()->ResolveText("selectedregion.source")
            != "map"
        || IsWidgetVisible(
            interpreter,
            *dataStore.Registry(),
            "leader_candidate_list"
        ))
    {
        std::cerr << "CHI map selection policy failed\n";
        return 1;
    }

    const GuiActionEvent assign = MakeListEvent(
        &candidateWidget,
        1,
        "assign_war_map_leader"
    );
    if (bridge.Dispatch("china_anti_jap", assign))
    {
        std::cerr << "CHI map assignment was not rejected\n";
        return 1;
    }

    const GuiActionEvent combatSelection = MakeListEvent(
        &combatWidget,
        24,
        "select_combat_region"
    );
    combatWidget.listIndex = hubeiIndex;
    if (!bridge.Dispatch("china_anti_jap", combatSelection)
        || dataStore.Registry()->ResolveText("selectedregion.source")
            != "combat"
        || !IsWidgetVisible(
            interpreter,
            *dataStore.Registry(),
            "leader_candidate_list"
        ))
    {
        std::cerr << "CHI combat selection policy failed\n";
        return 1;
    }

    GuiListRuntimeLayout candidateLayout = BuildCandidateLayout(
        interpreter,
        *dataStore.Registry()
    );
	if (candidateLayout.items.size() != 3
        || !candidateLayout.items[0].enabled
		|| !candidateLayout.items[1].enabled
		|| !candidateLayout.items[2].enabled
        || candidateLayout.items[0].normalSpriteName
            != "GFX_warmap_leader_button_chi")
    {
        std::cerr << "eligible candidate rendering state failed\n";
        return 1;
    }

    if (!bridge.Dispatch("china_anti_jap", assign))
    {
        std::cerr << "leader assignment fallback failed\n";
        return 1;
    }
    const GuiListModel* assigned = dataStore.Registry()->FindList(
        "assigned_leader_list"
    );
    const GuiDataValue* region = assigned && !assigned->items.empty()
        ? assigned->items[0].Find("regionid")
        : nullptr;
    if (!region || GuiDataValueToNumber(*region) != 24.0)
    {
        std::cerr << "assigned leader region mismatch\n";
        return 1;
    }

    candidateLayout = BuildCandidateLayout(
        interpreter,
        *dataStore.Registry()
    );
	if (candidateLayout.items.size() != 3
		|| candidateLayout.items[0].enabled
		|| !candidateLayout.items[1].enabled
		|| candidateLayout.items[2].enabled)
    {
		std::cerr << "military slot candidate state failed\n";
        return 1;
    }

	candidateWidget.listIndex = 1;
	const GuiActionEvent assignAdministrative = MakeListEvent(
		&candidateWidget,
		2,
		"assign_war_map_leader"
	);
	if (!bridge.Dispatch("china_anti_jap", assignAdministrative))
	{
		std::cerr << "administrative slot assignment failed\n";
		return 1;
	}
	candidateWidget.listIndex = 2;
	const GuiActionEvent assignSecondMilitary = MakeListEvent(
		&candidateWidget,
		3,
		"assign_war_map_leader"
	);
	if (bridge.Dispatch("china_anti_jap", assignSecondMilitary))
	{
		std::cerr << "duplicate military slot was not rejected\n";
		return 1;
	}
	assigned = dataStore.Registry()->FindList("assigned_leader_list");
	if (!assigned
		|| assigned->items.size() != 2
		|| GuiDataValueToText(
			*assigned->items[0].Find("leadertype")
		) != "military"
		|| GuiDataValueToText(
			*assigned->items[1].Find("leadertype")
		) != "administrative"
		|| GuiDataValueToNumber(
			*assigned->items[0].Find("assignmentorder")
		) != 1.0
		|| GuiDataValueToNumber(
			*assigned->items[1].Find("assignmentorder")
		) != 2.0)
	{
		std::cerr << "dual leader slot data failed\n";
		return 1;
	}
	candidateLayout = BuildCandidateLayout(
		interpreter,
		*dataStore.Registry()
	);
	if (candidateLayout.items.size() != 3
		|| candidateLayout.items[0].enabled
		|| candidateLayout.items[1].enabled
		|| candidateLayout.items[2].enabled)
	{
		std::cerr << "occupied dual slot state failed\n";
		return 1;
	}

    const GuiActionEvent secondSelection = MakeListEvent(
        &combatWidget,
        23,
        "select_combat_region"
    );
    combatWidget.listIndex = henanIndex;
	candidateWidget.listIndex = 1;
    if (!bridge.Dispatch("china_anti_jap", secondSelection)
		|| bridge.Dispatch("china_anti_jap", assignAdministrative))
    {
        std::cerr << "duplicate leader assignment was not rejected\n";
        return 1;
    }
    assigned = dataStore.Registry()->FindList("assigned_leader_list");
    region = assigned && !assigned->items.empty()
        ? assigned->items[0].Find("regionid")
        : nullptr;
    if (!region || GuiDataValueToNumber(*region) != 24.0)
    {
        std::cerr << "duplicate assignment changed leader region\n";
        return 1;
    }

    GuiActionEvent moved;
    moved.widget = &markerWidget;
    moved.phase = GuiActionPhase::DragEnd;
    moved.action = "move_war_map_leader";
    moved.itemId = 1;
    moved.hasItemId = true;
    moved.sourceListName = "assigned_leader_list";
    moved.sourceListIndex = 0;
    moved.parameters["normalizedx"] = "0.4";
    moved.parameters["normalizedy"] = "0.55";
	moved.parameters["regionid"] = "24";
    if (!bridge.Dispatch("china_anti_jap", moved))
    {
        std::cerr << "leader drag fallback failed\n";
        return 1;
    }
    assigned = dataStore.Registry()->FindList("assigned_leader_list");
	const GuiDataValue* x = assigned->items[0].Find("x");
	const GuiDataValue* y = assigned->items[0].Find("y");
	const GuiDataValue* administrativeX = assigned->items[1].Find("x");
	const GuiDataValue* administrativeY = assigned->items[1].Find("y");
    if (!x
        || !y
		|| !administrativeX
		|| !administrativeY
        || std::abs(GuiDataValueToNumber(*x) - 0.4) > 0.0001
		|| std::abs(GuiDataValueToNumber(*y) - 0.55) > 0.0001
		|| std::abs(GuiDataValueToNumber(*administrativeX) - 0.4)
			> 0.0001
		|| std::abs(GuiDataValueToNumber(*administrativeY) - 0.55)
			> 0.0001)
    {
        std::cerr << "leader drag coordinates mismatch\n";
        return 1;
    }

    gui::GuiLayoutContext context =
        dataStore.Registry()->MakeLayoutContext();
	context.localizationResolver = [](std::string_view key)
	{
		if (key == "WARMAP_LEADER_LIST_LI_ZONGREN")
		{
			return std::string("军事主官　李宗仁");
		}
		if (key == "WARMAP_LEADER_LIST_JU_ZHENG")
		{
			return std::string("行政主官　居正");
		}
		return std::string(key);
	};
    const std::vector<gui::GuiTextCommand> listText =
        interpreter.BuildListTextCommands(
            "china_anti_jap",
            "leader_candidate_list",
            context
        );
	if (listText.size() != 3
		|| listText[0].text != "军事主官　李宗仁"
		|| listText[1].text != "行政主官　居正")
    {
        std::cerr << "localized rich list text binding failed\n";
        return 1;
    }

    const GuiActionEvent stepDown = MakeListEvent(
        &markerWidget,
        1,
        "step_down_war_map_leader"
    );
    if (!bridge.Dispatch("china_anti_jap", stepDown))
    {
        std::cerr << "leader step-down fallback failed\n";
        return 1;
    }
	assigned = dataStore.Registry()->FindList("assigned_leader_list");
    candidateLayout = BuildCandidateLayout(
        interpreter,
        *dataStore.Registry()
    );
	if (!assigned
		|| assigned->items.size() != 1
		|| candidateLayout.items.size() != 3
		|| !candidateLayout.items[0].enabled
		|| candidateLayout.items[1].enabled
		|| !candidateLayout.items[2].enabled)
    {
		std::cerr << "military step-down slot state failed\n";
        return 1;
    }
	markerWidget.listIndex = 0;
	const GuiActionEvent stepDownAdministrative = MakeListEvent(
		&markerWidget,
		2,
		"step_down_war_map_leader"
	);
	if (!bridge.Dispatch("china_anti_jap", stepDownAdministrative))
	{
		std::cerr << "administrative step-down fallback failed\n";
		return 1;
	}
	assigned = dataStore.Registry()->FindList("assigned_leader_list");
	candidateLayout = BuildCandidateLayout(
		interpreter,
		*dataStore.Registry()
	);
	if (!assigned
		|| !assigned->items.empty()
		|| candidateLayout.items.size() != 3
		|| !candidateLayout.items[0].enabled
		|| !candidateLayout.items[1].enabled
		|| !candidateLayout.items[2].enabled)
	{
		std::cerr << "administrative step-down slot state failed\n";
		return 1;
	}

    std::cout << "CHI map assignment rejected: yes\n";
    std::cout << "CHI combat assignment accepted: yes\n";
    std::cout << "Unique assignment enforced: yes\n";
	std::cout << "Per-Region military/administrative slots: yes\n";
	std::cout << "Occupied slot candidates disabled: yes\n";
    std::cout << "Leader step-down restored candidate: yes\n";
    std::cout << "GUI literal text count: 0\n";
    return 0;
}
