#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "gui_action_bridge.h"
#include "gui_custom_widget.h"
#include "gui_data.h"
#include "gui_runtime.h"

struct GuiPluginInitContext
{
    const std::filesystem::path& root;
    void* graphicsContext = nullptr;
    const gui::GuiInterpreter& interpreter;
    const GuiWindowRuntime& windowRuntime;
};

struct GuiPluginResourceStats
{
    std::size_t textureCount = 0;
    uint64_t textureBytes = 0;
    uint64_t cpuBytes = 0;
};

class IGuiPlugin
{
public:
    virtual ~IGuiPlugin() = default;

    virtual std::string_view WindowName() const = 0;
    virtual std::string_view WindowTitle() const = 0;

    virtual uint32_t TickIntervalMilliseconds() const
    {
        return 120;
    }

    virtual bool Initialize(
        const GuiPluginInitContext& context,
        std::string& error
    ) = 0;

    virtual void Shutdown() = 0;

    virtual void RegisterCustomWidgets(
        gui::GuiCustomWidgetRegistry& registry
    ) = 0;

    virtual std::shared_ptr<GuiDataRegistry> BuildDataRegistry() const = 0;

    virtual bool Tick(uint64_t nowMilliseconds) = 0;

    virtual bool HandleAction(
        const GuiActionContext& context
    ) = 0;

    virtual void* CustomWidgetContext()
    {
        return nullptr;
    }

    virtual GuiPluginResourceStats ResourceStats() const
    {
        return {};
    }
};

struct GuiPluginLaunch
{
    std::string id;
    std::string visibleWhen;
    IGuiPlugin* plugin = nullptr;
    bool openInitially = true;
    int windowZOrder = 0;
    bool modal = false;
    double maxViewportWidthRatio = 1.0;
    double maxViewportHeightRatio = 1.0;
    int cascadeOffsetX = 0;
    int cascadeOffsetY = 0;
};
