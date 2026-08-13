#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gui_data_provider.h"
#include "gui_host_macos.h"

struct GuiMacPluginCreateContext;

struct DeclarativeGuiMacPluginConfig
{
    std::string windowName;
    std::string windowTitle;
    std::unique_ptr<IGuiDataProvider> dataProvider;
    uint32_t tickIntervalMilliseconds = 200;
};

class DeclarativeGuiMacPlugin final : public IGuiMacPlugin
{
public:
    explicit DeclarativeGuiMacPlugin(
        DeclarativeGuiMacPluginConfig config
    );

    ~DeclarativeGuiMacPlugin() override;

    std::string_view WindowName() const override;
    std::string_view WindowTitle() const override;
    uint32_t TickIntervalMilliseconds() const override;

    bool Initialize(
        const GuiMacPluginInitContext& context,
        std::string& error
    ) override;

    void Shutdown() override;

    void RegisterCustomWidgets(
        gui::GuiCustomWidgetRegistry& registry
    ) override;

    std::shared_ptr<GuiDataRegistry> BuildDataRegistry() const override;

    bool Tick(uint64_t nowMilliseconds) override;

    bool HandleAction(
        const GuiActionContext& context
    ) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<IGuiMacPlugin> CreateDeclarativeGuiMacPlugin(
    const GuiMacPluginCreateContext& context
);
