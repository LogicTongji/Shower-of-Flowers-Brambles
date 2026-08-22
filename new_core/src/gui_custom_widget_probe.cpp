#include <algorithm>
#include <array>
#include <iostream>
#include <utility>
#include <vector>

#include "gui_custom_widget.h"

int main()
{
    gui::GuiCustomWidgetRegistry registry;
    int graphicsToken = 1;
    int hostToken = 2;
    int drawCount = 0;
    int inputCount = 0;
    bool contextMatched = false;
    bool inputMatched = false;

    if (!registry.Register(
            "probe_custom",
            {
                [&](const gui::GuiResolvedWidget& widget,
                    const gui::GuiCustomWidgetContext& context)
                {
                    ++drawCount;
                    contextMatched = widget.rect.x == 12
                        && widget.rect.y == 34
                        && context.graphicsContext == &graphicsToken
                        && context.hostContext == &hostToken;
                },
                [&](const gui::GuiResolvedWidget& widget,
                    const gui::GuiCustomWidgetContext& context,
                    gui::GuiCustomInputPhase phase,
                    int mouseX,
                    int mouseY)
                {
                    ++inputCount;
                    inputMatched = widget.rect.width == 56
                        && context.graphicsContext == &graphicsToken
                        && context.hostContext == &hostToken
                        && phase == gui::GuiCustomInputPhase::Press
                        && mouseX == 23
                        && mouseY == 45;
                    return true;
                }
            }
        ))
    {
        std::cerr << "Custom widget registration failed\n";
        return 1;
    }

    gui::WidgetDefinition definition;
    definition.type = gui::WidgetType::Custom;
    definition.name = "probe_widget";
    definition.customType = "probe_custom";

    gui::GuiResolvedWidget widget;
    widget.definition = &definition;
    widget.rect = {12, 34, 56, 78};
    widget.visible = true;

    const gui::GuiCustomWidgetContext context{
        &graphicsToken,
        &hostToken
    };
    if (!registry.DrawWidget(widget, context)
        || drawCount != 1
        || !contextMatched)
    {
        std::cerr << "Custom widget draw dispatch failed\n";
        return 1;
    }

    widget.visible = false;
    if (registry.DrawWidget(widget, context) || drawCount != 1)
    {
        std::cerr << "Hidden custom widget was drawn\n";
        return 1;
    }

    widget.visible = true;
    widget.enabled = true;
    if (!registry.HandleInput(
            widget,
            context,
            gui::GuiCustomInputPhase::Press,
            23,
            45
        )
        || inputCount != 1
        || !inputMatched)
    {
        std::cerr << "Custom widget input dispatch failed\n";
        return 1;
    }

    widget.enabled = false;
    if (registry.HandleInput(
            widget,
            context,
            gui::GuiCustomInputPhase::Press,
            23,
            45
        )
        || inputCount != 1)
    {
        std::cerr << "Disabled custom widget received input\n";
        return 1;
    }

    std::vector<gui::GuiCustomInputEventType> receivedTypes;
    bool eventContextMatched = true;
    gui::GuiCustomWidgetHandler eventHandler;
    eventHandler.draw = [](
        const gui::GuiResolvedWidget&,
        const gui::GuiCustomWidgetContext&)
    {
    };
    eventHandler.event = [
        &receivedTypes,
        &eventContextMatched,
        &graphicsToken,
        &hostToken
    ](
        const gui::GuiResolvedWidget& resolved,
        const gui::GuiCustomWidgetContext& eventContext,
        const gui::GuiCustomInputEvent& event)
    {
        receivedTypes.push_back(event.type);
        eventContextMatched = eventContextMatched
            && resolved.rect.height == 78
            && eventContext.graphicsContext == &graphicsToken
            && eventContext.hostContext == &hostToken;
        return true;
    };
    if (!registry.Register("event_custom", std::move(eventHandler)))
    {
        std::cerr << "Unified custom event registration failed\n";
        return 1;
    }

    definition.customType = "event_custom";
    widget.visible = true;
    widget.enabled = true;
    const std::array<gui::GuiCustomInputEventType, 12> eventTypes{
        gui::GuiCustomInputEventType::PointerMove,
        gui::GuiCustomInputEventType::PointerEnter,
        gui::GuiCustomInputEventType::PointerLeave,
        gui::GuiCustomInputEventType::PointerDown,
        gui::GuiCustomInputEventType::PointerUp,
        gui::GuiCustomInputEventType::PointerWheel,
        gui::GuiCustomInputEventType::KeyDown,
        gui::GuiCustomInputEventType::KeyUp,
        gui::GuiCustomInputEventType::TextInput,
        gui::GuiCustomInputEventType::FocusGained,
        gui::GuiCustomInputEventType::FocusLost,
        gui::GuiCustomInputEventType::Cancel
    };
    for (const gui::GuiCustomInputEventType type : eventTypes)
    {
        gui::GuiCustomInputEvent event;
        event.type = type;
        event.button = gui::GuiCustomPointerButton::Right;
        event.mouseX = 23;
        event.mouseY = 45;
        event.wheelDelta = 2;
        event.keyCode = 65;
        event.character = 'A';
        event.modifiers = gui::GuiCustomModifierShift;
        event.repeatCount = 1;
        if (!registry.HandleEvent(widget, context, event))
        {
            std::cerr << "Unified custom event dispatch failed\n";
            return 1;
        }
    }
    if (receivedTypes.size() != eventTypes.size()
        || !std::equal(
            receivedTypes.begin(),
            receivedTypes.end(),
            eventTypes.begin()
        )
        || !eventContextMatched)
    {
        std::cerr << "Unified custom event sequence failed\n";
        return 1;
    }

    widget.enabled = false;
    gui::GuiCustomInputEvent disabledPress;
    disabledPress.type = gui::GuiCustomInputEventType::PointerDown;
    if (registry.HandleEvent(widget, context, disabledPress))
    {
        std::cerr << "Disabled custom widget received pointer down\n";
        return 1;
    }
    gui::GuiCustomInputEvent cancel;
    cancel.type = gui::GuiCustomInputEventType::Cancel;
    if (!registry.HandleEvent(widget, context, cancel))
    {
        std::cerr << "Disabled custom widget missed cancel\n";
        return 1;
    }

    int markerDrawCount = 0;
    int markerEventCount = 0;
    gui::GuiCustomWidgetHandler markerHandler;
    markerHandler.draw = [&markerDrawCount](
        const gui::GuiResolvedWidget&,
        const gui::GuiCustomWidgetContext& markerContext)
    {
        if (markerContext.sceneWidgets)
        {
            ++markerDrawCount;
        }
    };
    markerHandler.event = [&markerEventCount](
        const gui::GuiResolvedWidget&,
        const gui::GuiCustomWidgetContext& markerContext,
        const gui::GuiCustomInputEvent& event)
    {
        if (markerContext.sceneWidgets
            && event.type ==
                gui::GuiCustomInputEventType::PointerMove)
        {
            ++markerEventCount;
            return true;
        }
        return false;
    };
    markerHandler.globalInput = true;
    if (!registry.Register("marker_layer", std::move(markerHandler)))
    {
        std::cerr << "Marker custom widget registration failed\n";
        return 1;
    }

    gui::WidgetDefinition markerDefinition;
    markerDefinition.type = gui::WidgetType::MarkerLayer;
    markerDefinition.name = "probe_marker_layer";
    gui::GuiResolvedWidget markerWidget;
    markerWidget.definition = &markerDefinition;
    markerWidget.visible = true;
    markerWidget.enabled = true;
    std::vector<gui::GuiResolvedWidget> markerScene{
        markerWidget,
        markerWidget
    };
    const gui::GuiCustomWidgetContext markerContext{
        &graphicsToken,
        &hostToken,
        &markerScene,
        nullptr
    };
    if (!registry.DrawWidget(markerWidget, markerContext)
        || markerDrawCount != 1
        || !registry.CanHandle(markerWidget))
    {
        std::cerr << "Marker draw did not use Custom registry\n";
        return 1;
    }
    gui::GuiCustomInputEvent markerMove;
    markerMove.type = gui::GuiCustomInputEventType::PointerMove;
    if (!registry.HandleGlobalEvent(
            markerScene,
            markerContext,
            markerMove
        )
        || markerEventCount != 1)
    {
        std::cerr << "Marker global input did not use Custom registry\n";
        return 1;
    }

    std::cout << "Custom widget unified event dispatch: passed\n";
    return 0;
}
