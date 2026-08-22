#pragma once

#include <d3d9.h>

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "gui_data.h"
#include "gui_custom_widget.h"
#include "gui_indexed_map_d3d9.h"
#include "gui_localization.h"
#include "gui_runtime.h"

class GuiTextRendererD3D9;

using GuiMarkerD3D9TextureResolver = std::function<IDirect3DTexture9*(
    std::string_view
)>;

struct GuiMarkerLayerD3D9InputResult
{
    bool consumed = false;
    std::vector<GuiActionEvent> events;
};

class GuiMarkerLayerD3D9Runtime
{
public:
    GuiMarkerLayerD3D9Runtime();
    ~GuiMarkerLayerD3D9Runtime();

    GuiMarkerLayerD3D9Runtime(
        const GuiMarkerLayerD3D9Runtime&
    ) = delete;
    GuiMarkerLayerD3D9Runtime& operator=(
        const GuiMarkerLayerD3D9Runtime&
    ) = delete;

    void Initialize(
        IDirect3DDevice9* device,
        GuiTextRendererD3D9& textRenderer,
        const GuiLocalizationRegistry& localization,
        GuiMarkerD3D9TextureResolver textureResolver
    );

    void Shutdown();
    void SetData(std::shared_ptr<const GuiDataRegistry> data);

    bool RegisterCustomWidget(
        gui::GuiCustomWidgetRegistry& registry,
        const GuiIndexedMapD3D9Runtime& indexedMaps
    );

    bool DrawWidget(
        const gui::GuiResolvedWidget& layer,
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapD3D9Runtime& indexedMaps
    );

    GuiMarkerLayerD3D9InputResult HandleMove(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapD3D9Runtime& indexedMaps,
        int mouseX,
        int mouseY
    );

    GuiMarkerLayerD3D9InputResult HandlePress(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapD3D9Runtime& indexedMaps,
        int mouseX,
        int mouseY
    );

    GuiMarkerLayerD3D9InputResult HandleRelease(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapD3D9Runtime& indexedMaps,
        int mouseX,
        int mouseY
    );

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
