#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_interpreter.h"

namespace gui
{

enum class GuiCustomInputPhase
{
    Move,
    Press,
    Release
};

struct GuiCustomWidgetContext
{
    void* graphicsContext = nullptr;
    void* hostContext = nullptr;
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

struct GuiCustomWidgetHandler
{
    GuiCustomDrawCallback draw;
    GuiCustomInputCallback input;
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
