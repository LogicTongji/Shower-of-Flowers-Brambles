#include "declarative_gui_plugin_macos.h"

#include <iostream>
#include <utility>

#include "gui_plugin_registry_macos.h"

struct DeclarativeGuiMacPlugin::Impl
{
    DeclarativeGuiMacPluginConfig config;
    bool providerInitialized = false;
};

DeclarativeGuiMacPlugin::DeclarativeGuiMacPlugin(
    DeclarativeGuiMacPluginConfig config
)
    : impl_(std::make_unique<Impl>())
{
    impl_->config = std::move(config);
    if (impl_->config.windowTitle.empty())
    {
        impl_->config.windowTitle = impl_->config.windowName;
    }
}

DeclarativeGuiMacPlugin::~DeclarativeGuiMacPlugin() = default;

std::string_view DeclarativeGuiMacPlugin::WindowName() const
{
    return impl_->config.windowName;
}

std::string_view DeclarativeGuiMacPlugin::WindowTitle() const
{
    return impl_->config.windowTitle;
}

uint32_t DeclarativeGuiMacPlugin::TickIntervalMilliseconds() const
{
    return impl_->config.tickIntervalMilliseconds;
}

bool DeclarativeGuiMacPlugin::Initialize(
    const GuiMacPluginInitContext& context,
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

void DeclarativeGuiMacPlugin::Shutdown()
{
    if (impl_->providerInitialized && impl_->config.dataProvider)
    {
        impl_->config.dataProvider->Shutdown();
    }
    impl_->providerInitialized = false;
}

void DeclarativeGuiMacPlugin::RegisterCustomWidgets(
    gui::GuiCustomWidgetRegistry&
)
{
}

std::shared_ptr<GuiDataRegistry>
DeclarativeGuiMacPlugin::BuildDataRegistry() const
{
    return impl_->config.dataProvider
        ? impl_->config.dataProvider->Registry()
        : nullptr;
}

bool DeclarativeGuiMacPlugin::Tick(uint64_t nowMilliseconds)
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

bool DeclarativeGuiMacPlugin::HandleAction(
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

std::unique_ptr<IGuiMacPlugin> CreateDeclarativeGuiMacPlugin(
    const GuiMacPluginCreateContext& context
)
{
    if (!context.dataProviders)
    {
        return nullptr;
    }

    DeclarativeGuiMacPluginConfig config;
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
    return std::make_unique<DeclarativeGuiMacPlugin>(
        std::move(config)
    );
}
