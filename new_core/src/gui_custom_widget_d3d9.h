#pragma once

#include <d3d9.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "gui_custom_widget.h"
#include "gui_data.h"

class GuiIndexedMapD3D9Runtime;
class GuiLocalizationRegistry;
class GuiTextRendererD3D9;

using GuiD3D9CustomTextureResolver = std::function<IDirect3DTexture9*(
    std::string_view
)>;

class GuiCustomWidgetD3D9Runtime
{
public:
    GuiCustomWidgetD3D9Runtime();
    ~GuiCustomWidgetD3D9Runtime();

    GuiCustomWidgetD3D9Runtime(
        const GuiCustomWidgetD3D9Runtime&
    ) = delete;
    GuiCustomWidgetD3D9Runtime& operator=(
        const GuiCustomWidgetD3D9Runtime&
    ) = delete;

    bool Initialize(
        IDirect3DDevice9* device,
        GuiTextRendererD3D9& textRenderer,
        const GuiLocalizationRegistry& localization,
        GuiD3D9CustomTextureResolver textureResolver,
        gui::GuiCustomWidgetRegistry& registry,
        const GuiIndexedMapD3D9Runtime& indexedMaps,
        std::string& error
    );

    void Shutdown();
    void SetData(std::shared_ptr<const GuiDataRegistry> data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
