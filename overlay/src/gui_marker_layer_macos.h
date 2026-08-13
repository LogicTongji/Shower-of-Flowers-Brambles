#pragma once

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "gui_data.h"
#include "gui_indexed_map_macos.h"
#include "gui_localization.h"
#include "gui_runtime.h"
#include "gui_text_renderer_macos.h"

using GuiMarkerTextureResolver = std::function<SDL_Texture*(
    std::string_view
)>;

struct GuiMarkerLayerInputResult
{
    bool consumed = false;
    std::vector<GuiActionEvent> events;
};

struct GuiMarkerLayerMacResourceStats
{
    std::size_t textureCount = 0;
    uint64_t textureBytes = 0;
    uint64_t cpuBytes = 0;
};

class GuiMarkerLayerMacRuntime
{
public:
    GuiMarkerLayerMacRuntime();
    ~GuiMarkerLayerMacRuntime();

    GuiMarkerLayerMacRuntime(const GuiMarkerLayerMacRuntime&) = delete;
    GuiMarkerLayerMacRuntime& operator=(
        const GuiMarkerLayerMacRuntime&
    ) = delete;

    void Initialize(
        SDL_Renderer* renderer,
        const GuiMacFontSet& fonts,
        const GuiLocalizationRegistry& localization,
        GuiMarkerTextureResolver textureResolver
    );

    void Shutdown();

    void SetData(std::shared_ptr<const GuiDataRegistry> data);

    bool DrawWidget(
        const gui::GuiResolvedWidget& layer,
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapMacRuntime& indexedMaps
    );

    GuiMarkerLayerInputResult HandleMove(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapMacRuntime& indexedMaps,
        int mouseX,
        int mouseY
    );

    GuiMarkerLayerInputResult HandlePress(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapMacRuntime& indexedMaps,
        int mouseX,
        int mouseY
    );

    GuiMarkerLayerInputResult HandleRelease(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapMacRuntime& indexedMaps,
        int mouseX,
        int mouseY
    );

    GuiMarkerLayerMacResourceStats ResourceStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
