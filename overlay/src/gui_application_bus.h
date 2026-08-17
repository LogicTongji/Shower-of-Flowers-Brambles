#pragma once

#include <string_view>
#include <vector>

#include "gui_action_bridge.h"

enum class GuiWindowVisibilityMode
{
    Automatic,
    Shown,
    Hidden
};

class IGuiApplicationEndpoint
{
public:
    virtual ~IGuiApplicationEndpoint() = default;

    virtual std::string_view PluginId() const = 0;
    virtual std::string_view WindowName() const = 0;
    virtual bool IsOpen() const = 0;
    virtual bool IsVisible() const = 0;

    virtual void OpenWindow() = 0;

    virtual void SetVisibilityMode(
        GuiWindowVisibilityMode mode
    ) = 0;

    virtual void CloseWindow() = 0;

    virtual bool DispatchPluginAction(
        const GuiActionContext& context
    ) = 0;
};

class GuiApplicationActionBus
{
public:
    void SetEndpoints(
        std::vector<IGuiApplicationEndpoint*> endpoints
    );

    bool Dispatch(
        std::string_view sourcePluginId,
        const GuiActionContext& context
    ) const;

private:
    IGuiApplicationEndpoint* FindEndpoint(
        std::string_view target,
        std::string_view sourcePluginId
    ) const;

    std::vector<IGuiApplicationEndpoint*> endpoints_;
};
