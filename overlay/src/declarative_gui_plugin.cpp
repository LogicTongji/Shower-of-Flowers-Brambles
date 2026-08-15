#include "declarative_gui_plugin.h"

#include <iostream>
#include <utility>

#include "gui_plugin_registry.h"

struct DeclarativeGuiPlugin::Impl
{
    DeclarativeGuiPluginConfig config;
    bool providerInitialized = false;
};

DeclarativeGuiPlugin::DeclarativeGuiPlugin(
    DeclarativeGuiPluginConfig config
)
    : impl_(std::make_unique<Impl>())
{
    impl_->config = std::move(config);
    if (impl_->config.windowTitle.empty())
    {
        impl_->config.windowTitle = impl_->config.windowName;
    }
}

DeclarativeGuiPlugin::~DeclarativeGuiPlugin() = default;

std::string_view DeclarativeGuiPlugin::WindowName() const
{
    return impl_->config.windowName;
}

std::string_view DeclarativeGuiPlugin::WindowTitle() const
{
    return impl_->config.windowTitle;
}

uint32_t DeclarativeGuiPlugin::TickIntervalMilliseconds() const
{
    return impl_->config.tickIntervalMilliseconds;
}

bool DeclarativeGuiPlugin::Initialize(
    const GuiPluginInitContext& context,
    std::string& error
)
{
    if (impl_->config.windowName.empty())
    {
        error = "Declarative GUI window name is empty";
        return false;
    }
    if (!impl_->config.dataProvider)
    {
        error = "Declarative GUI data provider is missing";
        return false;
    }
    if (!impl_->config.dataProvider->Initialize(
            GuiDataProviderInitContext{context.root},
            error
        ))
    {
        impl_->config.dataProvider->Shutdown();
        return false;
    }
    impl_->providerInitialized = true;
    return true;
}

void DeclarativeGuiPlugin::Shutdown()
{
    if (impl_->providerInitialized && impl_->config.dataProvider)
    {
        impl_->config.dataProvider->Shutdown();
    }
    impl_->providerInitialized = false;
}

void DeclarativeGuiPlugin::RegisterCustomWidgets(
    gui::GuiCustomWidgetRegistry&
)
{
}

std::shared_ptr<GuiDataRegistry>
DeclarativeGuiPlugin::BuildDataRegistry() const
{
    return impl_->config.dataProvider
        ? impl_->config.dataProvider->Registry()
        : nullptr;
}

bool DeclarativeGuiPlugin::Tick(uint64_t nowMilliseconds)
{
    if (!impl_->providerInitialized || !impl_->config.dataProvider)
    {
        return false;
    }

    std::string error;
    const GuiDataProviderUpdateResult result =
        impl_->config.dataProvider->Tick(
            nowMilliseconds,
            error
        );
    if (result == GuiDataProviderUpdateResult::Failed)
    {
        std::cerr << "GUI data provider tick warning ["
                  << impl_->config.dataProvider->Type()
                  << "]: " << error << '\n';
    }
    return result == GuiDataProviderUpdateResult::Changed;
}

bool DeclarativeGuiPlugin::HandleAction(
    const GuiActionContext& context
)
{
    if (!impl_->providerInitialized || !impl_->config.dataProvider)
    {
        return false;
    }

    std::string error;
    const GuiDataProviderActionResult result =
        impl_->config.dataProvider->HandleAction(
            context,
            error
        );
    if (result == GuiDataProviderActionResult::Failed)
    {
        std::cerr << "GUI data provider action warning ["
                  << impl_->config.dataProvider->Type()
                  << "]: " << error << '\n';
    }
    return result == GuiDataProviderActionResult::Handled;
}

std::unique_ptr<IGuiPlugin> CreateDeclarativeGuiPlugin(
    const GuiPluginCreateContext& context
)
{
    if (!context.dataProviders)
    {
        return nullptr;
    }

    DeclarativeGuiPluginConfig config;
    config.windowName = context.Option("window");
    config.windowTitle = context.Option("title");

    std::string providerType = context.Option("data_provider");
    if (providerType.empty())
    {
        providerType = context.Option("provider");
    }
    if (providerType.empty())
    {
        providerType = "file";
    }

    GuiDataProviderCreateContext providerContext;
    providerContext.options = context.options;
    config.dataProvider = context.dataProviders->Create(
        providerType,
        providerContext
    );
    if (!config.dataProvider)
    {
        return nullptr;
    }

    const std::string tickInterval = context.Option("tick_interval");
    if (!tickInterval.empty())
    {
        try
        {
            config.tickIntervalMilliseconds =
                static_cast<uint32_t>(std::stoul(tickInterval));
        }
        catch (...)
        {
            return nullptr;
        }
    }
    if (config.windowName.empty())
    {
        return nullptr;
    }
    return std::make_unique<DeclarativeGuiPlugin>(
        std::move(config)
    );
}
