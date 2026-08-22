#include "gui_application_bus.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace
{

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

std::string Parameter(
    const GuiActionContext& context,
    std::string_view name
)
{
    const auto found = context.parameters.find(Lower(std::string(name)));
    return found == context.parameters.end()
        ? std::string{}
        : found->second;
}

std::string TargetWindow(const GuiActionContext& context)
{
    std::string target = Parameter(context, "window");
    if (target.empty())
    {
        target = Parameter(context, "plugin");
    }
    if (target.empty())
    {
        target = Parameter(context, "target_window");
    }
    return target;
}

void RemoveRoutingParameters(GuiActionContext& context)
{
    context.parameters.erase("window");
    context.parameters.erase("plugin");
    context.parameters.erase("target_window");
    context.parameters.erase("operation");
}

}

void GuiApplicationActionBus::SetEndpoints(
    std::vector<IGuiApplicationEndpoint*> endpoints
)
{
    endpoints_.clear();
    for (IGuiApplicationEndpoint* endpoint : endpoints)
    {
        if (endpoint)
        {
            endpoints_.push_back(endpoint);
        }
    }
}

IGuiApplicationEndpoint* GuiApplicationActionBus::FindEndpoint(
    std::string_view target,
    std::string_view sourcePluginId
) const
{
    const std::string normalizedTarget = Lower(
        target.empty()
            ? std::string(sourcePluginId)
            : std::string(target)
    );
    for (IGuiApplicationEndpoint* endpoint : endpoints_)
    {
        if (Lower(std::string(endpoint->PluginId())) == normalizedTarget
            || Lower(std::string(endpoint->WindowName()))
                == normalizedTarget)
        {
            return endpoint;
        }
    }
    return nullptr;
}

bool GuiApplicationActionBus::Dispatch(
    std::string_view sourcePluginId,
    const GuiActionContext& context
) const
{
    const std::string operation = Lower(context.fallbackOperation);
    IGuiApplicationEndpoint* endpoint = FindEndpoint(
        TargetWindow(context),
        sourcePluginId
    );
    if (!endpoint)
    {
        return false;
    }

    if (operation == "open_window")
    {
        endpoint->OpenWindow();
        endpoint->SetVisibilityMode(GuiWindowVisibilityMode::Automatic);
        return true;
    }
    if (operation == "show_window")
    {
        endpoint->OpenWindow();
        endpoint->SetVisibilityMode(GuiWindowVisibilityMode::Shown);
        return true;
    }
    if (operation == "hide_window")
    {
        endpoint->SetVisibilityMode(GuiWindowVisibilityMode::Hidden);
        return true;
    }
    if (operation == "toggle_window")
    {
        if (!endpoint->IsOpen())
        {
            endpoint->OpenWindow();
            endpoint->SetVisibilityMode(GuiWindowVisibilityMode::Shown);
            return true;
        }
        endpoint->SetVisibilityMode(
            endpoint->IsVisible()
                ? GuiWindowVisibilityMode::Hidden
                : GuiWindowVisibilityMode::Shown
        );
        return true;
    }
    if (operation == "reset_window_visibility")
    {
        endpoint->SetVisibilityMode(GuiWindowVisibilityMode::Automatic);
        return true;
    }
    if (operation == "close_window")
    {
        endpoint->CloseWindow();
        return true;
    }

    GuiActionContext forwarded = context;
    RemoveRoutingParameters(forwarded);
    if (operation == "dispatch_action"
        || operation == "send_action")
    {
        forwarded.fallbackOperation = Parameter(context, "operation");
        return !forwarded.fallbackOperation.empty()
            && endpoint->DispatchPluginAction(forwarded);
    }
    if (operation == "set_window_value")
    {
        forwarded.fallbackOperation = "set_value";
    }
    else if (operation == "toggle_window_value")
    {
        forwarded.fallbackOperation = "toggle_value";
    }
    else if (operation == "add_window_value")
    {
        forwarded.fallbackOperation = "add_value";
    }
    else if (operation == "reload_window_data")
    {
        forwarded.fallbackOperation = "reload_data";
    }
    else
    {
        return false;
    }

    const std::string key = Parameter(context, "key");
    if (!key.empty())
    {
        forwarded.parameters["target"] = key;
    }
    forwarded.parameters.erase("key");
    return endpoint->DispatchPluginAction(forwarded);
}
