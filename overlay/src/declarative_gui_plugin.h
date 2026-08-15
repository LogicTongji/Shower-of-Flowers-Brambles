#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gui_data_provider.h"
#include "gui_plugin.h"

struct GuiPluginCreateContext;

struct DeclarativeGuiPluginConfig
{
    std::string windowName;
    std::string windowTitle;
    std::unique_ptr<IGuiDataProvider> dataProvider;
    uint32_t tickIntervalMilliseconds = 200;
};

class DeclarativeGuiPlugin final : public IGuiPlugin
{
public:
    explicit DeclarativeGuiPlugin(
        DeclarativeGuiPluginConfig config
    );

    ~DeclarativeGuiPlugin() override;

    std::string_view WindowName() const override;
    std::string_view WindowTitle() const override;
    uint32_t TickIntervalMilliseconds() const override;

    bool Initialize(
        const GuiPluginInitContext& context,
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

std::unique_ptr<IGuiPlugin> CreateDeclarativeGuiPlugin(
    const GuiPluginCreateContext& context
);
