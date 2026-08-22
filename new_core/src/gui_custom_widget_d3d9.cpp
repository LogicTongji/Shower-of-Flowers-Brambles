#include "gui_custom_widget_d3d9.h"

#include <utility>

#include "gui_marker_layer_d3d9.h"

struct GuiCustomWidgetD3D9Runtime::Impl
{
    GuiMarkerLayerD3D9Runtime markerLayer;
    bool initialized = false;
};

GuiCustomWidgetD3D9Runtime::GuiCustomWidgetD3D9Runtime()
    : impl_(std::make_unique<Impl>())
{
}

GuiCustomWidgetD3D9Runtime::~GuiCustomWidgetD3D9Runtime()
{
    Shutdown();
}

bool GuiCustomWidgetD3D9Runtime::Initialize(
    IDirect3DDevice9* device,
    GuiTextRendererD3D9& textRenderer,
    const GuiLocalizationRegistry& localization,
    GuiD3D9CustomTextureResolver textureResolver,
    gui::GuiCustomWidgetRegistry& registry,
    const GuiIndexedMapD3D9Runtime& indexedMaps,
    std::string& error
)
{
    Shutdown();
    impl_->markerLayer.Initialize(
        device,
        textRenderer,
        localization,
        std::move(textureResolver)
    );
    if (!impl_->markerLayer.RegisterCustomWidget(
            registry,
            indexedMaps
        ))
    {
        impl_->markerLayer.Shutdown();
        error = "builtin_custom_widget_registration_failed";
        return false;
    }
    impl_->initialized = true;
    return true;
}

void GuiCustomWidgetD3D9Runtime::Shutdown()
{
    if (!impl_)
    {
        return;
    }
    impl_->markerLayer.Shutdown();
    impl_->initialized = false;
}

void GuiCustomWidgetD3D9Runtime::SetData(
    std::shared_ptr<const GuiDataRegistry> data
)
{
    if (impl_->initialized)
    {
        impl_->markerLayer.SetData(std::move(data));
    }
}
