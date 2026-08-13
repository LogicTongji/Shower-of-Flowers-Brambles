#pragma once

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gui_action_bridge.h"
#include "gui_custom_widget.h"
#include "gui_data.h"
#include "gui_runtime.h"

struct GuiMacPluginInitContext
{
    const std::filesystem::path& root;
    SDL_Renderer* renderer = nullptr;
    const gui::GuiInterpreter& interpreter;
    const GuiWindowRuntime& windowRuntime;
};

struct GuiMacPluginResourceStats
{
    std::size_t textureCount = 0;
    uint64_t textureBytes = 0;
    uint64_t cpuBytes = 0;
};

class IGuiMacPlugin
{
public:
    virtual ~IGuiMacPlugin() = default;

    virtual std::string_view WindowName() const = 0;
    virtual std::string_view WindowTitle() const = 0;

    virtual uint32_t TickIntervalMilliseconds() const
    {
        return 120;
    }

    virtual bool Initialize(
        const GuiMacPluginInitContext& context,
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

    virtual GuiMacPluginResourceStats ResourceStats() const
    {
        return {};
    }
};

struct GuiMacPluginLaunch
{
    std::string id;
    std::string visibleWhen;
    IGuiMacPlugin* plugin = nullptr;
};

struct GuiMacHostOptions
{
    bool printResourceStats = false;
};

int RunGuiMacHostApplication(
    const std::filesystem::path& root,
    const std::vector<GuiMacPluginLaunch>& launches,
    const GuiMacHostOptions& options = {}
);

int RunGuiMacHost(
    const std::filesystem::path& root,
    IGuiMacPlugin& plugin
);
