#include "gui_custom_widget.h"

#include <unordered_set>
#include <utility>

namespace gui
{

namespace
{

std::string_view ResolveHandlerType(
    const GuiResolvedWidget& widget
)
{
    if (!widget.definition)
    {
        return {};
    }
    if (!widget.definition->customType.empty())
    {
        return widget.definition->customType;
    }
    if (widget.definition->type == WidgetType::MarkerLayer)
    {
        return "marker_layer";
    }
    if (widget.definition->type == WidgetType::Custom)
    {
        return widget.definition->name;
    }
    return {};
}

bool IsTeardownEvent(GuiCustomInputEventType type)
{
    return type == GuiCustomInputEventType::PointerUp
        || type == GuiCustomInputEventType::PointerLeave
        || type == GuiCustomInputEventType::FocusLost
        || type == GuiCustomInputEventType::Cancel;
}

}

bool GuiCustomWidgetRegistry::Register(
    std::string type,
    GuiCustomWidgetHandler handler
)
{
    if (type.empty() || !handler.draw)
    {
        return false;
    }

    handlers_[std::move(type)] = std::move(handler);
    return true;
}

const GuiCustomWidgetHandler* GuiCustomWidgetRegistry::Find(
    std::string_view type
) const
{
    const auto iterator = handlers_.find(std::string(type));
    return iterator == handlers_.end()
        ? nullptr
        : &iterator->second;
}

bool GuiCustomWidgetRegistry::Draw(
    const std::vector<GuiResolvedWidget>& widgets,
    const GuiCustomWidgetContext& context
) const
{
    bool drawn = false;
    for (const GuiResolvedWidget& widget : widgets)
    {
        drawn = DrawWidget(widget, context) || drawn;
    }

    return drawn;
}

bool GuiCustomWidgetRegistry::DrawWidget(
    const GuiResolvedWidget& widget,
    const GuiCustomWidgetContext& context
) const
{
    if (!widget.definition || !widget.visible)
    {
        return false;
    }

    const std::string_view type = ResolveHandlerType(widget);
    if (type.empty())
    {
        return false;
    }
    const GuiCustomWidgetHandler* handler = Find(type);
    if (!handler)
    {
        return false;
    }

    handler->draw(widget, context);
    return true;
}

bool GuiCustomWidgetRegistry::HandleInput(
    const GuiResolvedWidget& widget,
    const GuiCustomWidgetContext& context,
    GuiCustomInputPhase phase,
    int mouseX,
    int mouseY
) const
{
    GuiCustomInputEvent event;
    event.mouseX = mouseX;
    event.mouseY = mouseY;
    event.button = phase == GuiCustomInputPhase::Move
        ? GuiCustomPointerButton::None
        : GuiCustomPointerButton::Left;
    event.type = phase == GuiCustomInputPhase::Move
        ? GuiCustomInputEventType::PointerMove
        : (phase == GuiCustomInputPhase::Press
            ? GuiCustomInputEventType::PointerDown
            : GuiCustomInputEventType::PointerUp);
    return HandleEvent(widget, context, event);
}

bool GuiCustomWidgetRegistry::HandleInput(
    const std::vector<GuiResolvedWidget>& widgets,
    const GuiCustomWidgetContext& context,
    GuiCustomInputPhase phase,
    int mouseX,
    int mouseY
) const
{
    GuiCustomInputEvent event;
    event.mouseX = mouseX;
    event.mouseY = mouseY;
    event.button = phase == GuiCustomInputPhase::Move
        ? GuiCustomPointerButton::None
        : GuiCustomPointerButton::Left;
    event.type = phase == GuiCustomInputPhase::Move
        ? GuiCustomInputEventType::PointerMove
        : (phase == GuiCustomInputPhase::Press
            ? GuiCustomInputEventType::PointerDown
            : GuiCustomInputEventType::PointerUp);
    return HandleEvent(widgets, context, event);
}

bool GuiCustomWidgetRegistry::HandleEvent(
    const GuiResolvedWidget& widget,
    const GuiCustomWidgetContext& context,
    const GuiCustomInputEvent& event
) const
{
    if (!widget.definition
        || ((!widget.visible || !widget.enabled)
            && !IsTeardownEvent(event.type)))
    {
        return false;
    }

    const GuiCustomWidgetHandler* handler = Find(
        ResolveHandlerType(widget)
    );
    if (!handler)
    {
        return false;
    }
    if (handler->event)
    {
        return handler->event(widget, context, event);
    }
    if (!handler->input)
    {
        return false;
    }

    GuiCustomInputPhase phase;
    switch (event.type)
    {
    case GuiCustomInputEventType::PointerMove:
        phase = GuiCustomInputPhase::Move;
        break;
    case GuiCustomInputEventType::PointerDown:
        if (event.button != GuiCustomPointerButton::Left)
        {
            return false;
        }
        phase = GuiCustomInputPhase::Press;
        break;
    case GuiCustomInputEventType::PointerUp:
        if (event.button != GuiCustomPointerButton::Left)
        {
            return false;
        }
        phase = GuiCustomInputPhase::Release;
        break;
    default:
        return false;
    }
    return handler->input(
        widget,
        context,
        phase,
        event.mouseX,
        event.mouseY
    );
}

bool GuiCustomWidgetRegistry::HandleEvent(
    const std::vector<GuiResolvedWidget>& widgets,
    const GuiCustomWidgetContext& context,
    const GuiCustomInputEvent& event
) const
{
    bool handled = false;
    for (auto iterator = widgets.rbegin();
        iterator != widgets.rend();
        ++iterator)
    {
        handled = HandleEvent(*iterator, context, event) || handled;
    }
    return handled;
}

bool GuiCustomWidgetRegistry::HandleGlobalEvent(
    const std::vector<GuiResolvedWidget>& widgets,
    const GuiCustomWidgetContext& context,
    const GuiCustomInputEvent& event,
    GuiResolvedWidget* handledWidget
) const
{
    std::unordered_set<const GuiCustomWidgetHandler*> dispatched;
    for (auto iterator = widgets.rbegin();
        iterator != widgets.rend();
        ++iterator)
    {
        if (!iterator->definition
            || ((!iterator->visible || !iterator->enabled)
                && !IsTeardownEvent(event.type)))
        {
            continue;
        }
        const GuiCustomWidgetHandler* handler = Find(
            ResolveHandlerType(*iterator)
        );
        if (!handler
            || !handler->globalInput
            || !dispatched.insert(handler).second)
        {
            continue;
        }
        if (HandleEvent(*iterator, context, event))
        {
            if (handledWidget)
            {
                *handledWidget = *iterator;
            }
            return true;
        }
    }
    return false;
}

bool GuiCustomWidgetRegistry::CanHandle(
    const GuiResolvedWidget& widget
) const
{
    return widget.definition
        && Find(ResolveHandlerType(widget));
}

}
