#include <iostream>
#include <string>
#include <string_view>

#include "gui_application_bus.h"

namespace
{

class ProbeEndpoint final : public IGuiApplicationEndpoint
{
public:
    ProbeEndpoint(std::string id, std::string window)
        : id_(std::move(id)), window_(std::move(window))
    {
    }

    std::string_view PluginId() const override
    {
        return id_;
    }

    std::string_view WindowName() const override
    {
        return window_;
    }

    bool IsVisible() const override
    {
        return visible_;
    }

    bool IsOpen() const override
    {
        return open_;
    }

    void OpenWindow() override
    {
        open_ = true;
        ++openCount_;
    }

    void SetVisibilityMode(GuiWindowVisibilityMode mode) override
    {
        visibilityMode_ = mode;
        if (mode == GuiWindowVisibilityMode::Shown)
        {
            visible_ = true;
        }
        else if (mode == GuiWindowVisibilityMode::Hidden)
        {
            visible_ = false;
        }
        else
        {
            visible_ = automaticVisibility_;
        }
    }

    void CloseWindow() override
    {
        open_ = false;
        visible_ = false;
        closed_ = true;
    }

    bool DispatchPluginAction(const GuiActionContext& context) override
    {
        lastAction_ = context;
        return true;
    }

    bool closed_ = false;
    bool open_ = true;
    int openCount_ = 0;
    bool visible_ = true;
    bool automaticVisibility_ = true;
    GuiWindowVisibilityMode visibilityMode_ =
        GuiWindowVisibilityMode::Automatic;
    GuiActionContext lastAction_;

private:
    std::string id_;
    std::string window_;
};

GuiActionContext MakeAction(
    std::string operation,
    std::string target
)
{
    GuiActionContext context;
    context.fallbackOperation = std::move(operation);
    context.parameters["window"] = std::move(target);
    return context;
}

}

int main()
{
    ProbeEndpoint first("first", "first_window");
    ProbeEndpoint second("second", "second_window");
    GuiApplicationActionBus bus;
    bus.SetEndpoints({&first, &second});

    if (!bus.Dispatch("first", MakeAction("hide_window", "second"))
        || second.visible_)
    {
        std::cerr << "Window visibility routing failed\n";
        return 1;
    }
    if (!bus.Dispatch("first", MakeAction("toggle_window", "second"))
        || !second.visible_)
    {
        std::cerr << "Window visibility toggle failed\n";
        return 1;
    }

    GuiActionContext setValue = MakeAction(
        "set_window_value",
        "second_window"
    );
    setValue.parameters["key"] = "shared.value";
    setValue.parameters["value"] = "42";
    setValue.parameters["type"] = "integer";
    if (!bus.Dispatch("first", setValue)
        || second.lastAction_.fallbackOperation != "set_value"
        || second.lastAction_.parameters["target"] != "shared.value"
        || second.lastAction_.parameters["value"] != "42")
    {
        std::cerr << "Cross-window data routing failed\n";
        return 1;
    }

    GuiActionContext send = MakeAction("send_action", "second");
    send.parameters["operation"] = "add_value";
    send.parameters["target"] = "shared.value";
    send.parameters["amount"] = "8";
    if (!bus.Dispatch("first", send)
        || second.lastAction_.fallbackOperation != "add_value"
        || second.lastAction_.parameters["amount"] != "8")
    {
        std::cerr << "Cross-window action forwarding failed\n";
        return 1;
    }

    if (!bus.Dispatch("first", MakeAction("close_window", "second"))
        || !second.closed_
        || second.IsOpen())
    {
        std::cerr << "Window close routing failed\n";
        return 1;
    }

    if (!bus.Dispatch("first", MakeAction("show_window", "second"))
        || !second.IsOpen()
        || !second.IsVisible()
        || second.openCount_ != 1)
    {
        std::cerr << "Closed window reopen routing failed\n";
        return 1;
    }

    std::cout
        << "Application bus visibility: " << second.visible_ << '\n'
        << "Application bus action: "
        << second.lastAction_.fallbackOperation << '\n'
        << "Application bus closed: " << second.closed_ << '\n';
    return 0;
}
