#include <filesystem>
#include <iostream>
#include <string>

#include "gui_action_bridge.h"
#include "gui_behavior.h"
#include "gui_declarative_data.h"

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    const fs::path root = argc >= 2
        ? fs::path(argv[1])
        : fs::current_path();
    const fs::path fixtureRoot =
        root / "new_core" / "tests" / "fixtures";

    GuiDeclarativeDataStore dataStore;
    std::string error;
    if (!dataStore.LoadFile(
            fixtureRoot / "declarative_gui_data.txt",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }

    const std::shared_ptr<GuiDataRegistry> data = dataStore.Registry();
    const GuiListModel* list = data->FindList("task_list");
    if (!data->ResolveBool("state.visible")
        || data->ResolveNumber("counter") != 2.0
        || data->ResolveNumber("progress") != 0.25
        || data->ResolveText("formatted_code") != "007"
        || !list
        || list->revision != 4
        || list->items.size() != 2
        || list->items[1].id != 22
        || list->items[1].text != "第二项")
    {
        std::cerr << "Declarative GUI data parsing failed\n";
        return 1;
    }

    GuiBehaviorRegistry behaviors;
    if (!behaviors.LoadFile(
            fixtureRoot / "declarative_gui_behavior.txt",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }

    GuiLuaActionBridge actionBridge;
    actionBridge.SetBehaviorRegistry(&behaviors);
    actionBridge.SetConditionEvaluator(
        [&dataStore](std::string_view expression)
        {
            return dataStore.Registry()->EvaluateCondition(expression);
        }
    );
    actionBridge.SetFallbackInvoker(
        [&dataStore](const GuiActionContext& context)
        {
            return dataStore.ApplyAction(context);
        }
    );

    const GuiActionEvent toggleEvent{
        nullptr,
        GuiActionPhase::Click,
        "toggle_visibility"
    };
    const GuiActionEvent increaseEvent{
        nullptr,
        GuiActionPhase::Click,
        "increase_counter"
    };
    const GuiActionEvent renameEvent{
        nullptr,
        GuiActionPhase::Click,
        "rename_title"
    };
    if (!actionBridge.Dispatch("probe", toggleEvent)
        || !actionBridge.Dispatch("probe", increaseEvent)
        || !actionBridge.Dispatch("probe", renameEvent)
        || dataStore.Registry()->ResolveBool("state.visible")
        || dataStore.Registry()->ResolveNumber("counter") != 5.0
        || dataStore.Registry()->ResolveText("title") != "动作已执行")
    {
        std::cerr << "Declarative GUI behavior dispatch failed\n";
        return 1;
    }

    std::string directFunction;
    actionBridge.SetInvoker(
        [&directFunction](
            std::string_view functionName,
            const GuiActionContext&
        )
        {
            directFunction = std::string(functionName);
            return true;
        }
    );
    const GuiActionEvent directLuaEvent{
        nullptr,
        GuiActionPhase::Click,
        "ProbeGui.DirectAction"
    };
    if (!actionBridge.Dispatch("probe", directLuaEvent)
        || directFunction != "ProbeGui.DirectAction")
    {
        std::cerr << "Direct Lua action dispatch failed\n";
        return 1;
    }

    GuiActionContext itemAction;
    itemAction.fallbackOperation = "set_value";
    itemAction.parameters["target"] = "selected_item";
    itemAction.parameters["value"] = "$list_item_id";
    itemAction.parameters["type"] = "integer";
    itemAction.hasListItemId = true;
    itemAction.listItemId = 22;
    if (!dataStore.ApplyAction(itemAction)
        || dataStore.Registry()->ResolveNumber("selected_item") != 22.0)
    {
        std::cerr << "Declarative GUI event parameter binding failed\n";
        return 1;
    }

	GuiActionContext dragAction;
	dragAction.fallbackOperation = "set_drag_value";
	dragAction.parameters["target"] = "drag.value";
	dragAction.parameters["value"] = "6.375";
	dragAction.parameters["steptarget"] = "drag.step";
	dragAction.parameters["stepindex"] = "7";
	dragAction.parameters["normalizedtarget"] = "drag.normalized";
	dragAction.parameters["normalized"] = "0.671875";
	if (!dataStore.ApplyAction(dragAction)
		|| dataStore.Registry()->ResolveNumber("drag.value") != 6.375
		|| dataStore.Registry()->ResolveNumber("drag.step") != 7.0
		|| dataStore.Registry()->ResolveNumber("drag.normalized")
			!= 0.671875)
	{
		std::cerr << "Declarative drag value binding failed\n";
		return 1;
	}

    std::cout
        << "Declarative values: visible="
        << dataStore.Registry()->ResolveBool("state.visible")
        << " counter="
        << dataStore.Registry()->ResolveNumber("counter")
        << " title="
        << dataStore.Registry()->ResolveText("title") << '\n'
        << "Declarative list items: " << list->items.size() << '\n'
        << "Bound list item: "
        << dataStore.Registry()->ResolveNumber("selected_item") << '\n'
		<< "Drag value: "
		<< dataStore.Registry()->ResolveNumber("drag.value")
		<< " step="
		<< dataStore.Registry()->ResolveNumber("drag.step") << '\n';
    return 0;
}
