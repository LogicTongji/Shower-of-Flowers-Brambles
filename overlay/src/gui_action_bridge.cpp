#include "gui_action_bridge.h"

#include <utility>

namespace
{

std::string PhaseName(GuiActionPhase phase)
{
    switch (phase)
    {
    case GuiActionPhase::HoverEnter:
        return "hover_enter";
    case GuiActionPhase::HoverLeave:
        return "hover_leave";
    case GuiActionPhase::Press:
        return "press";
    case GuiActionPhase::Release:
        return "release";
    case GuiActionPhase::Click:
        return "click";
    case GuiActionPhase::DragStart:
        return "drag_start";
    case GuiActionPhase::Drag:
        return "drag";
    case GuiActionPhase::DragEnd:
        return "drag_end";
    }

    return "unknown";
}

}

void GuiLuaActionBridge::SetInvoker(
    GuiLuaActionInvoker invoker
)
{
    invoker_ = std::move(invoker);
}

void GuiLuaActionBridge::SetFallbackInvoker(
    GuiFallbackActionInvoker invoker
)
{
    fallbackInvoker_ = std::move(invoker);
}

void GuiLuaActionBridge::SetBehaviorRegistry(
    const GuiBehaviorRegistry* registry
)
{
    behaviorRegistry_ = registry;
}

void GuiLuaActionBridge::SetConditionEvaluator(
    GuiActionConditionEvaluator evaluator
)
{
    conditionEvaluator_ = std::move(evaluator);
}

void GuiLuaActionBridge::SetListItemIdResolver(
    GuiListItemIdResolver resolver
)
{
    listItemIdResolver_ = std::move(resolver);
}

bool GuiLuaActionBridge::Dispatch(
    std::string_view windowName,
    const GuiActionEvent& event,
    int mouseX,
    int mouseY
) const
{
    if (event.action.empty())
    {
        return false;
    }

    const GuiBehaviorDefinition* behavior = behaviorRegistry_
        ? behaviorRegistry_->Find(event.action)
        : nullptr;
    if (behavior && !behavior->AcceptsPhase(
        PhaseName(event.phase)
    ))
    {
        return false;
    }
    if (behavior
        && !behavior->enabledWhen.empty()
        && conditionEvaluator_
        && !conditionEvaluator_(behavior->enabledWhen))
    {
        return false;
    }

    GuiActionContext context;
    context.action = event.action;
    context.phase = PhaseName(event.phase);
    context.functionName = behavior
        ? behavior->functionName
        : event.action;
    context.fallbackOperation = behavior
        ? behavior->fallbackOperation
        : std::string{};
    if (behavior)
    {
        context.parameters = behavior->parameters;
    }
    for (const auto& parameter : event.parameters)
    {
        context.parameters[parameter.first] = parameter.second;
    }
    context.windowName = windowName;
    context.mouseX = mouseX;
    context.mouseY = mouseY;
    if (event.hasItemId)
    {
        context.listItemId = event.itemId;
        context.hasListItemId = true;
    }
    if (event.widget && event.widget->definition)
    {
        context.widgetName = event.sourceWidgetName.empty()
            ? event.widget->definition->name
            : event.sourceWidgetName;
        context.listName = event.sourceListName.empty()
            ? event.widget->listName
            : event.sourceListName;
        context.listIndex = event.sourceListIndex >= 0
            ? event.sourceListIndex
            : event.widget->listIndex;
        if (!context.hasListItemId
            && listItemIdResolver_
            && context.listIndex >= 0
            && listItemIdResolver_(
                context.listName,
                context.listIndex,
                context.listItemId
            ))
        {
            context.hasListItemId = true;
        }
    }

    if (invoker_
        && invoker_(context.functionName, context))
    {
        return true;
    }

    return fallbackInvoker_ && fallbackInvoker_(context);
}

std::size_t GuiLuaActionBridge::DispatchEvents(
    std::string_view windowName,
    const std::vector<GuiActionEvent>& events,
    int mouseX,
    int mouseY
) const
{
    std::size_t dispatched = 0;
    for (const GuiActionEvent& event : events)
    {
        if (Dispatch(windowName, event, mouseX, mouseY))
        {
            ++dispatched;
        }
    }
    return dispatched;
}
