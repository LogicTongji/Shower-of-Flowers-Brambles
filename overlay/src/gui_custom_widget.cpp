#include "gui_custom_widget.h"

#include <utility>

namespace gui
{

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
    if (!widget.definition
        || widget.definition->type != WidgetType::Custom
        || !widget.visible)
    {
        return false;
    }

    const std::string& type = widget.definition->customType.empty()
        ? widget.definition->name
        : widget.definition->customType;
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
    if (!widget.definition
        || widget.definition->type != WidgetType::Custom
        || !widget.visible
        || !widget.enabled)
    {
        return false;
    }

    const std::string& type = widget.definition->customType.empty()
        ? widget.definition->name
        : widget.definition->customType;
    const GuiCustomWidgetHandler* handler = Find(type);
    return handler && handler->input
        ? handler->input(
            widget,
            context,
            phase,
            mouseX,
            mouseY
        )
        : false;
}

bool GuiCustomWidgetRegistry::HandleInput(
    const std::vector<GuiResolvedWidget>& widgets,
    const GuiCustomWidgetContext& context,
    GuiCustomInputPhase phase,
    int mouseX,
    int mouseY
) const
{
    bool handled = false;
    for (const GuiResolvedWidget& widget : widgets)
    {
        handled = HandleInput(
            widget,
            context,
            phase,
            mouseX,
            mouseY
        ) || handled;
    }

    return handled;
}

}
