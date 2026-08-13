#include "gui_indexed_map_macos.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <utility>

#include "gui_texture_loader_macos.h"
#include "gui_indexed_map_core.h"

namespace
{

struct IndexedMapRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(std::lround(
        std::clamp(value, 0.0f, 1.0f) * 255.0f
    ));
}

IndexedMapColor ToIndexedMapColor(const gui::GuiRgbaColor& color)
{
    return {
        ToByte(color.r),
        ToByte(color.g),
        ToByte(color.b),
        ToByte(color.a)
    };
}

IndexedMapColorRamp ToColorRamp(
    const gui::IndexedMapResource& resource
)
{
    IndexedMapColorRamp output;
    output.reserve(resource.colorStops.size());
    for (const gui::IndexedMapColorStop& stop : resource.colorStops)
    {
        output.push_back({stop.minimum, ToIndexedMapColor(stop.color)});
    }
    return output;
}

IndexedMapRect CalculateMapRect(
    const gui::GuiRect& viewport,
    int mapWidth,
    int mapHeight
)
{
    if (viewport.width <= 0
        || viewport.height <= 0
        || mapWidth <= 0
        || mapHeight <= 0)
    {
        return {};
    }

    const double scale = std::max(
        0.01,
        std::min(
            static_cast<double>(viewport.width) / mapWidth,
            static_cast<double>(viewport.height) / mapHeight
        )
    );
    const int width = std::max(
        1,
        static_cast<int>(mapWidth * scale)
    );
    const int height = std::max(
        1,
        static_cast<int>(mapHeight * scale)
    );
    return {
        viewport.x + (viewport.width - width) / 2,
        viewport.y + (viewport.height - height) / 2,
        width,
        height
    };
}

bool PointInside(
    const IndexedMapRect& rect,
    int mouseX,
    int mouseY
)
{
    return rect.width > 0
        && rect.height > 0
        && mouseX >= rect.x
        && mouseY >= rect.y
        && mouseX < rect.x + rect.width
        && mouseY < rect.y + rect.height;
}

uint16_t PickItem(
    const IndexedMapData& map,
    const IndexedMapRect& rect,
    int mouseX,
    int mouseY
)
{
    if (!PointInside(rect, mouseX, mouseY))
    {
        return 0;
    }

    const int textureX = (mouseX - rect.x)
        * static_cast<int>(map.width) / rect.width;
    const int textureY = (mouseY - rect.y)
        * static_cast<int>(map.height) / rect.height;
    if (textureX < 0
        || textureY < 0
        || textureX >= static_cast<int>(map.width)
        || textureY >= static_cast<int>(map.height))
    {
        return 0;
    }
    return map.itemIds[
        static_cast<size_t>(textureY) * map.width
        + static_cast<size_t>(textureX)
    ];
}

void UpdateTexture(
    SDL_Texture* texture,
    const std::vector<RgbaPixel>& pixels,
    int width
)
{
    if (!texture || pixels.empty() || width <= 0)
    {
        return;
    }
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        width * static_cast<int>(sizeof(RgbaPixel))
    );
}

void UpdateTextureRegion(
    SDL_Texture* texture,
    const std::vector<RgbaPixel>& pixels,
    int width,
    const IndexedMapBounds& bounds
)
{
    if (!texture || !bounds.valid || pixels.empty() || width <= 0)
    {
        return;
    }

    const SDL_Rect dirtyRect{
        static_cast<int>(bounds.minX),
        static_cast<int>(bounds.minY),
        static_cast<int>(bounds.maxX - bounds.minX + 1),
        static_cast<int>(bounds.maxY - bounds.minY + 1)
    };
    const size_t firstPixel =
        static_cast<size_t>(bounds.minY) * width + bounds.minX;
    SDL_UpdateTexture(
        texture,
        &dirtyRect,
        pixels.data() + firstPixel,
        width * static_cast<int>(sizeof(RgbaPixel))
    );
}

std::string ResolveIndexedSource(
    std::string pattern,
    uint16_t itemId
)
{
    const std::string replacement = std::to_string(itemId);
    size_t position = pattern.find("{id}");
    while (position != std::string::npos)
    {
        pattern.replace(position, 4, replacement);
        position = pattern.find("{id}", position + replacement.size());
    }
    return pattern;
}

uint64_t TextureBytes(SDL_Texture* texture)
{
    if (!texture)
    {
        return 0;
    }
    Uint32 format = SDL_PIXELFORMAT_UNKNOWN;
    int width = 0;
    int height = 0;
    if (SDL_QueryTexture(
            texture,
            &format,
            nullptr,
            &width,
            &height
        ) != 0)
    {
        return 0;
    }
    return static_cast<uint64_t>(std::max(0, width))
        * static_cast<uint64_t>(std::max(0, height))
        * static_cast<uint64_t>(
            std::max(1, static_cast<int>(SDL_BYTESPERPIXEL(format)))
        );
}

}

struct GuiIndexedMapMacRuntime::Impl
{
    struct Instance
    {
        const gui::WidgetDefinition* definition = nullptr;
        const gui::IndexedMapResource* resource = nullptr;
        SDL_Texture* baseTexture = nullptr;
        SDL_Texture* overlayTexture = nullptr;
        SDL_Texture* boundaryTexture = nullptr;
        SDL_Texture* hoverTexture = nullptr;
        int width = 0;
        int height = 0;
        IndexedMapData map;
        IndexedMapPixelIndex pixelIndex;
        IndexedMapColorRamp colorRamp;
        IndexedMapColor hoverColor;
        std::vector<RgbaPixel> overlayPixels;
        std::vector<RgbaPixel> hoverPixels;
        std::vector<float> previousValues;
        std::vector<uint16_t> changedItemIds;
        uint16_t hoveredItemId = 0;
        uint16_t pressedItemId = 0;
        uint16_t releasedItemId = 0;
        bool valuesInitialized = false;
        bool pressed = false;

        void Shutdown()
        {
            SDL_DestroyTexture(hoverTexture);
            SDL_DestroyTexture(boundaryTexture);
            SDL_DestroyTexture(overlayTexture);
            SDL_DestroyTexture(baseTexture);
            hoverTexture = nullptr;
            boundaryTexture = nullptr;
            overlayTexture = nullptr;
            baseTexture = nullptr;
            map = {};
            pixelIndex = {};
            overlayPixels.clear();
            hoverPixels.clear();
            previousValues.clear();
            changedItemIds.clear();
        }
    };

    SDL_Renderer* renderer = nullptr;
    std::vector<Instance> instances;
    std::unordered_map<const gui::WidgetDefinition*, size_t> byDefinition;

    SDL_Texture* CreateStreamingTexture(int width, int height) const
    {
        SDL_Texture* texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height
        );
        if (texture)
        {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        }
        return texture;
    }

    Instance* Find(const gui::WidgetDefinition* definition)
    {
        const auto iterator = byDefinition.find(definition);
        return iterator == byDefinition.end()
            ? nullptr
            : &instances[iterator->second];
    }

    const Instance* Find(const gui::WidgetDefinition* definition) const
    {
        const auto iterator = byDefinition.find(definition);
        return iterator == byDefinition.end()
            ? nullptr
            : &instances[iterator->second];
    }

    const gui::GuiResolvedWidget* HitWidget(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    ) const
    {
        for (auto iterator = widgets.rbegin();
             iterator != widgets.rend();
             ++iterator)
        {
            if (!iterator->definition
                || iterator->definition->type
                    != gui::WidgetType::IndexedMap
                || !iterator->visible
                || !iterator->enabled)
            {
                continue;
            }
            const Instance* instance = Find(iterator->definition);
            if (!instance)
            {
                continue;
            }
            const IndexedMapRect rect = CalculateMapRect(
                iterator->rect,
                instance->width,
                instance->height
            );
            if (PointInside(rect, mouseX, mouseY))
            {
                return &*iterator;
            }
        }
        return nullptr;
    }

    uint16_t Pick(
        const gui::GuiResolvedWidget& widget,
        int mouseX,
        int mouseY
    ) const
    {
        const Instance* instance = Find(widget.definition);
        if (!instance)
        {
            return 0;
        }
        return PickItem(
            instance->map,
            CalculateMapRect(
                widget.rect,
                instance->width,
                instance->height
            ),
            mouseX,
            mouseY
        );
    }

    void UpdateHighlight(Instance& instance, uint16_t itemId)
    {
        if (itemId == instance.hoveredItemId)
        {
            return;
        }
        const uint16_t previousItemId = instance.hoveredItemId;
        if (!UpdateIndexedMapHighlight(
                instance.pixelIndex,
                previousItemId,
                itemId,
                instance.hoverColor,
                instance.hoverPixels
            ))
        {
            return;
        }
        instance.hoveredItemId = itemId;
        if (previousItemId < instance.pixelIndex.boundsByItem.size())
        {
            UpdateTextureRegion(
                instance.hoverTexture,
                instance.hoverPixels,
                instance.width,
                instance.pixelIndex.boundsByItem[previousItemId]
            );
        }
        if (itemId < instance.pixelIndex.boundsByItem.size())
        {
            UpdateTextureRegion(
                instance.hoverTexture,
                instance.hoverPixels,
                instance.width,
                instance.pixelIndex.boundsByItem[itemId]
            );
        }
    }

    void Shutdown()
    {
        for (Instance& instance : instances)
        {
            instance.Shutdown();
        }
        instances.clear();
        byDefinition.clear();
        renderer = nullptr;
    }
};

GuiIndexedMapMacRuntime::GuiIndexedMapMacRuntime()
    : impl_(std::make_unique<Impl>())
{
}

GuiIndexedMapMacRuntime::~GuiIndexedMapMacRuntime()
{
    Shutdown();
}

bool GuiIndexedMapMacRuntime::Initialize(
    const std::filesystem::path& root,
    SDL_Renderer* renderer,
    const gui::GuiInterpreter& interpreter,
    const gui::WindowDefinition& window,
    std::string& error
)
{
    Shutdown();
    impl_->renderer = renderer;

    std::vector<const gui::WidgetDefinition*> definitions;
    std::function<void(const gui::WidgetDefinition&)> collect =
        [&](const gui::WidgetDefinition& parent)
    {
        for (const gui::WidgetDefinition& child : parent.children)
        {
            if (child.type == gui::WidgetType::IndexedMap)
            {
                definitions.push_back(&child);
            }
            collect(child);
        }
    };
    collect(window);

    impl_->instances.reserve(definitions.size());
    for (const gui::WidgetDefinition* definition : definitions)
    {
        const gui::IndexedMapResource* resource =
            interpreter.FindIndexedMap(
                definition->indexedMapResourceName
            );
        if (!resource)
        {
            error = "Indexed map resource not found: "
                + definition->indexedMapResourceName;
            Shutdown();
            return false;
        }

        Impl::Instance instance;
        instance.definition = definition;
        instance.resource = resource;
        instance.colorRamp = ToColorRamp(*resource);
        instance.hoverColor = ToIndexedMapColor(resource->hoverColor);
        instance.baseTexture = LoadGuiMacTexture(
            renderer,
            interpreter.ResolveIndexedMapTexture(resource->name, root),
            &instance.width,
            &instance.height
        );
        if (!instance.baseTexture)
        {
            error = "Failed to load indexed map texture: "
                + resource->textureFile.string();
            instance.Shutdown();
            Shutdown();
            return false;
        }

        if (!LoadIndexedMapData(
                interpreter.ResolveIndexedMapIndex(resource->name, root),
                instance.map
            )
            || static_cast<int>(instance.map.width) != instance.width
            || static_cast<int>(instance.map.height) != instance.height
            || !BuildIndexedMapPixelIndex(
                instance.map,
                instance.pixelIndex
            ))
        {
            error = "Failed to load or validate indexed map IDs: "
                + resource->indexFile.string();
            instance.Shutdown();
            Shutdown();
            return false;
        }

        instance.overlayTexture = impl_->CreateStreamingTexture(
            instance.width,
            instance.height
        );
        instance.hoverTexture = impl_->CreateStreamingTexture(
            instance.width,
            instance.height
        );
        if (resource->drawBoundaries)
        {
            instance.boundaryTexture = impl_->CreateStreamingTexture(
                instance.width,
                instance.height
            );
        }
        if (!instance.overlayTexture
            || !instance.hoverTexture
            || (resource->drawBoundaries
                && !instance.boundaryTexture))
        {
            error = "Failed to create indexed map textures";
            instance.Shutdown();
            Shutdown();
            return false;
        }

        instance.hoverPixels.assign(
            instance.map.itemIds.size(),
            RgbaPixel{0, 0, 0, 0}
        );
        UpdateTexture(
            instance.hoverTexture,
            instance.hoverPixels,
            instance.width
        );

        if (resource->drawBoundaries)
        {
            std::vector<RgbaPixel> boundaryPixels;
            BuildIndexedMapBoundaryOverlay(
                instance.map,
                ToIndexedMapColor(resource->boundaryColor),
                resource->boundaryWidth,
                boundaryPixels
            );
            UpdateTexture(
                instance.boundaryTexture,
                boundaryPixels,
                instance.width
            );
        }

        const size_t index = impl_->instances.size();
        impl_->instances.push_back(std::move(instance));
        impl_->byDefinition[definition] = index;
    }
    return true;
}

void GuiIndexedMapMacRuntime::Shutdown()
{
    impl_->Shutdown();
}

void GuiIndexedMapMacRuntime::Refresh(
    const gui::GuiLayoutContext& context
)
{
    for (Impl::Instance& instance : impl_->instances)
    {
        std::vector<float> values(
            instance.pixelIndex.spansByItem.size(),
            0.0f
        );
        if (context.valueResolver
            && !instance.definition->valueSource.empty())
        {
            for (size_t itemId = 1; itemId < values.size(); ++itemId)
            {
                values[itemId] = static_cast<float>(
                    context.valueResolver(
                        ResolveIndexedSource(
                            instance.definition->valueSource,
                            static_cast<uint16_t>(itemId)
                        )
                    )
                );
            }
        }

        if (!instance.valuesInitialized)
        {
            BuildIndexedMapOverlay(
                instance.map,
                values,
                instance.colorRamp,
                instance.overlayPixels
            );
            UpdateTexture(
                instance.overlayTexture,
                instance.overlayPixels,
                instance.width
            );
            instance.valuesInitialized = true;
        }
        else if (UpdateChangedIndexedMapOverlay(
            instance.pixelIndex,
            instance.previousValues,
            values,
            instance.colorRamp,
            instance.overlayPixels,
            &instance.changedItemIds
        ))
        {
            for (const uint16_t itemId : instance.changedItemIds)
            {
                if (itemId < instance.pixelIndex.boundsByItem.size())
                {
                    UpdateTextureRegion(
                        instance.overlayTexture,
                        instance.overlayPixels,
                        instance.width,
                        instance.pixelIndex.boundsByItem[itemId]
                    );
                }
            }
        }
        instance.previousValues = std::move(values);
    }
}

void GuiIndexedMapMacRuntime::Draw(
    const std::vector<gui::GuiResolvedWidget>& widgets
) const
{
    for (const gui::GuiResolvedWidget& widget : widgets)
    {
        DrawWidget(widget);
    }
}

bool GuiIndexedMapMacRuntime::DrawWidget(
    const gui::GuiResolvedWidget& widget
) const
{
    if (!widget.definition
        || widget.definition->type != gui::WidgetType::IndexedMap
        || !widget.visible)
    {
        return false;
    }
    const Impl::Instance* instance = impl_->Find(widget.definition);
    if (!instance || !instance->baseTexture)
    {
        return false;
    }
    const IndexedMapRect mapRect = CalculateMapRect(
        widget.rect,
        instance->width,
        instance->height
    );
    const SDL_Rect destination{
        mapRect.x,
        mapRect.y,
        mapRect.width,
        mapRect.height
    };
    SDL_RenderCopy(
        impl_->renderer,
        instance->baseTexture,
        nullptr,
        &destination
    );
    SDL_RenderCopy(
        impl_->renderer,
        instance->overlayTexture,
        nullptr,
        &destination
    );
    if (instance->boundaryTexture)
    {
        SDL_RenderCopy(
            impl_->renderer,
            instance->boundaryTexture,
            nullptr,
            &destination
        );
    }
    SDL_RenderCopy(
        impl_->renderer,
        instance->hoverTexture,
        nullptr,
        &destination
    );
    return true;
}

bool GuiIndexedMapMacRuntime::ResolveDrawRect(
    const gui::GuiResolvedWidget& widget,
    gui::GuiRect& rect
) const
{
    rect = {};
    if (!widget.definition
        || widget.definition->type != gui::WidgetType::IndexedMap)
    {
        return false;
    }
    const Impl::Instance* instance = impl_->Find(widget.definition);
    if (!instance)
    {
        return false;
    }
    const IndexedMapRect mapRect = CalculateMapRect(
        widget.rect,
        instance->width,
        instance->height
    );
    if (mapRect.width <= 0 || mapRect.height <= 0)
    {
        return false;
    }
    rect = {
        mapRect.x,
        mapRect.y,
        mapRect.width,
        mapRect.height
    };
    return true;
}

bool GuiIndexedMapMacRuntime::ResolveItemAnchor(
    const gui::GuiResolvedWidget& widget,
    uint16_t itemId,
    int& x,
    int& y
) const
{
    const Impl::Instance* instance = widget.definition
        ? impl_->Find(widget.definition)
        : nullptr;
    gui::GuiRect drawRect;
    if (!instance
        || itemId == 0
        || itemId >= instance->pixelIndex.anchorsByItem.size()
        || !ResolveDrawRect(widget, drawRect))
    {
        return false;
    }
    const IndexedMapAnchor& anchor =
        instance->pixelIndex.anchorsByItem[itemId];
    if (!anchor.valid || instance->width <= 0 || instance->height <= 0)
    {
        return false;
    }
    x = drawRect.x + static_cast<int>(
        anchor.x * drawRect.width / instance->width
    );
    y = drawRect.y + static_cast<int>(
        anchor.y * drawRect.height / instance->height
    );
    return true;
}

void GuiIndexedMapMacRuntime::HandleMove(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    const gui::GuiResolvedWidget* target = impl_->HitWidget(
        widgets,
        mouseX,
        mouseY
    );
    for (Impl::Instance& instance : impl_->instances)
    {
        const uint16_t itemId = target
            && target->definition == instance.definition
            ? impl_->Pick(*target, mouseX, mouseY)
            : 0;
        impl_->UpdateHighlight(instance, itemId);
    }
}

void GuiIndexedMapMacRuntime::HandlePress(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    const gui::GuiResolvedWidget* target = impl_->HitWidget(
        widgets,
        mouseX,
        mouseY
    );
    for (Impl::Instance& instance : impl_->instances)
    {
        instance.pressed = false;
        instance.pressedItemId = 0;
        instance.releasedItemId = 0;
    }
    if (!target)
    {
        return;
    }
    Impl::Instance* instance = impl_->Find(target->definition);
    if (!instance)
    {
        return;
    }
    instance->pressedItemId = impl_->Pick(
        *target,
        mouseX,
        mouseY
    );
    instance->pressed = instance->pressedItemId != 0;
}

void GuiIndexedMapMacRuntime::HandleRelease(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    const gui::GuiResolvedWidget* target = impl_->HitWidget(
        widgets,
        mouseX,
        mouseY
    );
    for (Impl::Instance& instance : impl_->instances)
    {
        instance.releasedItemId = 0;
        if (instance.pressed
            && target
            && target->definition == instance.definition)
        {
            const uint16_t itemId = impl_->Pick(
                *target,
                mouseX,
                mouseY
            );
            if (itemId == instance.pressedItemId)
            {
                instance.releasedItemId = itemId;
            }
        }
        instance.pressed = false;
    }
}

void GuiIndexedMapMacRuntime::AttachItemIds(
    std::vector<GuiActionEvent>& events
) const
{
    for (GuiActionEvent& event : events)
    {
        if (!event.widget
            || !event.widget->definition
            || event.widget->definition->type
                != gui::WidgetType::IndexedMap)
        {
            continue;
        }
        const Impl::Instance* instance = impl_->Find(
            event.widget->definition
        );
        if (!instance)
        {
            continue;
        }

        uint16_t itemId = 0;
        if (event.phase == GuiActionPhase::Press)
        {
            itemId = instance->pressedItemId;
        }
        else if (event.phase == GuiActionPhase::Release
            || event.phase == GuiActionPhase::Click)
        {
            itemId = instance->releasedItemId;
        }
        else if (event.phase == GuiActionPhase::HoverEnter)
        {
            itemId = instance->hoveredItemId;
        }

        if (itemId != 0)
        {
            event.itemId = itemId;
            event.hasItemId = true;
        }
    }
}

GuiIndexedMapMacResourceStats
GuiIndexedMapMacRuntime::ResourceStats() const
{
    GuiIndexedMapMacResourceStats stats;
    for (const Impl::Instance& instance : impl_->instances)
    {
        SDL_Texture* textures[] = {
            instance.baseTexture,
            instance.overlayTexture,
            instance.boundaryTexture,
            instance.hoverTexture
        };
        for (SDL_Texture* texture : textures)
        {
            if (texture)
            {
                ++stats.textureCount;
                stats.textureBytes += TextureBytes(texture);
            }
        }
        stats.cpuBytes += instance.map.itemIds.capacity()
            * sizeof(uint16_t);
        stats.cpuBytes += instance.overlayPixels.capacity()
            * sizeof(RgbaPixel);
        stats.cpuBytes += instance.hoverPixels.capacity()
            * sizeof(RgbaPixel);
        stats.cpuBytes += instance.previousValues.capacity()
            * sizeof(float);
        stats.cpuBytes += instance.changedItemIds.capacity()
            * sizeof(uint16_t);
        stats.cpuBytes += instance.pixelIndex.boundsByItem.capacity()
            * sizeof(IndexedMapBounds);
        stats.cpuBytes += instance.pixelIndex.anchorsByItem.capacity()
            * sizeof(IndexedMapAnchor);
        for (const std::vector<IndexedMapSpan>& spans
            : instance.pixelIndex.spansByItem)
        {
            stats.cpuBytes += spans.capacity() * sizeof(IndexedMapSpan);
        }
    }
    return stats;
}
