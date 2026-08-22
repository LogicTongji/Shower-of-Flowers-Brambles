#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_interpreter.h"
#include "gui_runtime.h"

namespace gui
{

enum class GuiCustomInputPhase
{
    Move,
    Press,
    Release
};

enum class GuiCustomInputEventType
{
    PointerMove,
    PointerEnter,
    PointerLeave,
    PointerDown,
    PointerUp,
    PointerWheel,
    KeyDown,
    KeyUp,
    TextInput,
    FocusGained,
    FocusLost,
    Cancel
};

enum class GuiCustomPointerButton
{
    None,
    Left,
    Right,
    Middle,
    X1,
    X2
};

enum GuiCustomInputModifier : uint32_t
{
    GuiCustomModifierNone = 0,
    GuiCustomModifierShift = 1u << 0,
    GuiCustomModifierControl = 1u << 1,
    GuiCustomModifierAlt = 1u << 2,
    GuiCustomModifierLeftButton = 1u << 3,
    GuiCustomModifierRightButton = 1u << 4,
    GuiCustomModifierMiddleButton = 1u << 5,
    GuiCustomModifierX1Button = 1u << 6,
    GuiCustomModifierX2Button = 1u << 7
};

struct GuiCustomInputEvent
{
    GuiCustomInputEventType type =
        GuiCustomInputEventType::PointerMove;
    GuiCustomPointerButton button = GuiCustomPointerButton::None;
    int mouseX = -1;
    int mouseY = -1;
    int wheelDelta = 0;
    uint32_t keyCode = 0;
    uint32_t character = 0;
    uint32_t modifiers = GuiCustomModifierNone;
    uint16_t repeatCount = 0;
    bool repeated = false;
    bool horizontalWheel = false;
};

struct GuiCustomWidgetContext
{
    void* graphicsContext = nullptr;
    void* hostContext = nullptr;
    const std::vector<GuiResolvedWidget>* sceneWidgets = nullptr;
    std::vector<GuiActionEvent>* emittedEvents = nullptr;
};

using GuiCustomDrawCallback = std::function<void(
    const GuiResolvedWidget&,
    const GuiCustomWidgetContext&
)>;

using GuiCustomInputCallback = std::function<bool(
    const GuiResolvedWidget&,
    const GuiCustomWidgetContext&,
    GuiCustomInputPhase,
    int,
    int
)>;

using GuiCustomEventCallback = std::function<bool(
    const GuiResolvedWidget&,
    const GuiCustomWidgetContext&,
    const GuiCustomInputEvent&
)>;

struct GuiCustomWidgetHandler
{
    GuiCustomDrawCallback draw;
    GuiCustomInputCallback input;
    GuiCustomEventCallback event;
    bool globalInput = false;
};

class GuiCustomWidgetRegistry
{
public:
    bool Register(
        std::string type,
        GuiCustomWidgetHandler handler
    );

    bool Draw(
        const std::vector<GuiResolvedWidget>& widgets,
        const GuiCustomWidgetContext& context
    ) const;

    bool DrawWidget(
        const GuiResolvedWidget& widget,
        const GuiCustomWidgetContext& context
    ) const;

    bool HandleInput(
        const GuiResolvedWidget& widget,
        const GuiCustomWidgetContext& context,
        GuiCustomInputPhase phase,
        int mouseX,
        int mouseY
    ) const;

    bool HandleEvent(
        const GuiResolvedWidget& widget,
        const GuiCustomWidgetContext& context,
        const GuiCustomInputEvent& event
    ) const;

    bool HandleEvent(
        const std::vector<GuiResolvedWidget>& widgets,
        const GuiCustomWidgetContext& context,
        const GuiCustomInputEvent& event
    ) const;

    bool HandleGlobalEvent(
        const std::vector<GuiResolvedWidget>& widgets,
        const GuiCustomWidgetContext& context,
        const GuiCustomInputEvent& event,
        GuiResolvedWidget* handledWidget = nullptr
    ) const;

    bool CanHandle(const GuiResolvedWidget& widget) const;

    bool HandleInput(
        const std::vector<GuiResolvedWidget>& widgets,
        const GuiCustomWidgetContext& context,
        GuiCustomInputPhase phase,
        int mouseX,
        int mouseY
    ) const;

    const GuiCustomWidgetHandler* Find(
        std::string_view type
    ) const;

private:
    std::unordered_map<std::string, GuiCustomWidgetHandler> handlers_;
};

}
