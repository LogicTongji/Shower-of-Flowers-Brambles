#include "gui_indexed_map_d3d9.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>

#include "gui_indexed_map_core.h"
#include "gui_texture_loader_d3d9.h"

namespace
{

uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(std::lround(
        std::clamp(value, 0.0f, 1.0f) * 255.0f
    ));
}

IndexedMapColor ToIndexedMapColor(
    const gui::GuiRgbaColor& color
)
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
        output.push_back({
            stop.minimum,
            ToIndexedMapColor(stop.color)
        });
    }
    return output;
}

gui::GuiRect CalculateMapRect(
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

bool PointInside(const gui::GuiRect& rect, int x, int y)
{
    return rect.width > 0
        && rect.height > 0
        && x >= rect.x
        && y >= rect.y
        && x < rect.x + rect.width
        && y < rect.y + rect.height;
}

uint16_t PickItem(
    const IndexedMapData& map,
    const gui::GuiRect& rect,
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
        static_cast<std::size_t>(textureY) * map.width
        + static_cast<std::size_t>(textureX)
    ];
}

std::string ResolveIndexedSource(
    std::string pattern,
    uint16_t itemId
)
{
    const std::string replacement = std::to_string(itemId);
    std::size_t position = pattern.find("{id}");
    while (position != std::string::npos)
    {
        pattern.replace(position, 4, replacement);
        position = pattern.find(
            "{id}",
            position + replacement.size()
        );
    }
    return pattern;
}

IDirect3DTexture9* CreatePixelTexture(
    IDirect3DDevice9* device,
    int width,
    int height
)
{
    IDirect3DTexture9* texture = nullptr;
    if (!device
        || width <= 0
        || height <= 0
        || FAILED(device->CreateTexture(
            static_cast<UINT>(width),
            static_cast<UINT>(height),
            1,
            0,
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            &texture,
            nullptr
        )))
    {
        return nullptr;
    }
    return texture;
}

bool UpdatePixelTexture(
    IDirect3DTexture9* texture,
    const std::vector<RgbaPixel>& pixels,
    int width,
    const IndexedMapBounds* bounds = nullptr
)
{
    if (!texture || pixels.empty() || width <= 0)
    {
        return false;
    }

    RECT dirty{};
    const RECT* dirtyPointer = nullptr;
    int startX = 0;
    int startY = 0;
    int updateWidth = width;
    int updateHeight = static_cast<int>(pixels.size()) / width;
    if (bounds)
    {
        if (!bounds->valid)
        {
            return false;
        }
        startX = static_cast<int>(bounds->minX);
        startY = static_cast<int>(bounds->minY);
        updateWidth = static_cast<int>(
            bounds->maxX - bounds->minX + 1
        );
        updateHeight = static_cast<int>(
            bounds->maxY - bounds->minY + 1
        );
        dirty = {
            startX,
            startY,
            startX + updateWidth,
            startY + updateHeight
        };
        dirtyPointer = &dirty;
    }

    D3DLOCKED_RECT locked{};
    if (FAILED(texture->LockRect(0, &locked, dirtyPointer, 0)))
    {
        return false;
    }
    for (int row = 0; row < updateHeight; ++row)
    {
        auto* destination = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(locked.pBits)
            + static_cast<std::size_t>(row) * locked.Pitch
        );
        const std::size_t sourceOffset =
            static_cast<std::size_t>(startY + row) * width + startX;
        for (int column = 0; column < updateWidth; ++column)
        {
            const RgbaPixel& pixel = pixels[
                sourceOffset + static_cast<std::size_t>(column)
            ];
            destination[column] = D3DCOLOR_ARGB(
                pixel.a,
                pixel.r,
                pixel.g,
                pixel.b
            );
        }
    }
    texture->UnlockRect(0);
    return true;
}

}

struct GuiIndexedMapD3D9Runtime::Impl
{
    struct Instance
    {
        const gui::WidgetDefinition* definition = nullptr;
        const gui::IndexedMapResource* resource = nullptr;
        GuiD3D9Texture baseTexture;
        IDirect3DTexture9* overlayTexture = nullptr;
        IDirect3DTexture9* boundaryTexture = nullptr;
        IDirect3DTexture9* hoverTexture = nullptr;
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

        Instance() = default;
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&& other) noexcept
        {
            MoveFrom(std::move(other));
        }

        Instance& operator=(Instance&& other) noexcept
        {
            if (this != &other)
            {
                Shutdown();
                MoveFrom(std::move(other));
            }
            return *this;
        }

        ~Instance()
        {
            Shutdown();
        }

        void Shutdown()
        {
            if (hoverTexture)
            {
                hoverTexture->Release();
            }
            if (boundaryTexture)
            {
                boundaryTexture->Release();
            }
            if (overlayTexture)
            {
                overlayTexture->Release();
            }
            hoverTexture = nullptr;
            boundaryTexture = nullptr;
            overlayTexture = nullptr;
            baseTexture.Reset();
        }

    private:
        void MoveFrom(Instance&& other)
        {
            definition = other.definition;
            resource = other.resource;
            baseTexture = std::move(other.baseTexture);
            overlayTexture = other.overlayTexture;
            boundaryTexture = other.boundaryTexture;
            hoverTexture = other.hoverTexture;
            map = std::move(other.map);
            pixelIndex = std::move(other.pixelIndex);
            colorRamp = std::move(other.colorRamp);
            hoverColor = other.hoverColor;
            overlayPixels = std::move(other.overlayPixels);
            hoverPixels = std::move(other.hoverPixels);
            previousValues = std::move(other.previousValues);
            changedItemIds = std::move(other.changedItemIds);
            hoveredItemId = other.hoveredItemId;
            pressedItemId = other.pressedItemId;
            releasedItemId = other.releasedItemId;
            valuesInitialized = other.valuesInitialized;
            pressed = other.pressed;
            other.overlayTexture = nullptr;
            other.boundaryTexture = nullptr;
            other.hoverTexture = nullptr;
        }
    };

    IDirect3DDevice9* device = nullptr;
    std::vector<Instance> instances;
    std::unordered_map<const gui::WidgetDefinition*, std::size_t>
        byDefinition;

    Instance* Find(const gui::WidgetDefinition* definition)
    {
        const auto found = byDefinition.find(definition);
        return found == byDefinition.end()
            ? nullptr
            : &instances[found->second];
    }

    const Instance* Find(
        const gui::WidgetDefinition* definition
    ) const
    {
        const auto found = byDefinition.find(definition);
        return found == byDefinition.end()
            ? nullptr
            : &instances[found->second];
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
            if (instance
                && PointInside(
                    CalculateMapRect(
                        iterator->rect,
                        instance->baseTexture.width,
                        instance->baseTexture.height
                    ),
                    mouseX,
                    mouseY
                ))
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
        return instance
            ? PickItem(
                instance->map,
                CalculateMapRect(
                    widget.rect,
                    instance->baseTexture.width,
                    instance->baseTexture.height
                ),
                mouseX,
                mouseY
            )
            : 0;
    }
};

GuiIndexedMapD3D9Runtime::GuiIndexedMapD3D9Runtime()
    : impl_(std::make_unique<Impl>())
{
}

GuiIndexedMapD3D9Runtime::~GuiIndexedMapD3D9Runtime()
{
    Shutdown();
}

bool GuiIndexedMapD3D9Runtime::Initialize(
    const std::filesystem::path& root,
    IDirect3DDevice9* device,
    const gui::GuiInterpreter& interpreter,
    const gui::WindowDefinition& window,
    std::string& error
)
{
    Shutdown();
    impl_->device = device;
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
        if (!LoadGuiD3D9Texture(
                device,
                interpreter.ResolveIndexedMapTexture(
                    resource->name,
                    root
                ),
                instance.baseTexture,
                error
            )
            || !LoadIndexedMapData(
                interpreter.ResolveIndexedMapIndex(
                    resource->name,
                    root
                ),
                instance.map
            )
            || instance.baseTexture.width
                != static_cast<int>(instance.map.width)
            || instance.baseTexture.height
                != static_cast<int>(instance.map.height)
            || !BuildIndexedMapPixelIndex(
                instance.map,
                instance.pixelIndex
            ))
        {
            if (error.empty())
            {
                error = "Failed to load indexed map: "
                    + resource->name;
            }
            Shutdown();
            return false;
        }

        instance.overlayTexture = CreatePixelTexture(
            device,
            instance.baseTexture.width,
            instance.baseTexture.height
        );
        instance.hoverTexture = CreatePixelTexture(
            device,
            instance.baseTexture.width,
            instance.baseTexture.height
        );
        if (resource->drawBoundaries)
        {
            instance.boundaryTexture = CreatePixelTexture(
                device,
                instance.baseTexture.width,
                instance.baseTexture.height
            );
        }
        if (!instance.overlayTexture
            || !instance.hoverTexture
            || (resource->drawBoundaries
                && !instance.boundaryTexture))
        {
            error = "Failed to create D3D9 indexed-map layers";
            Shutdown();
            return false;
        }

        instance.hoverPixels.assign(
            instance.map.itemIds.size(),
            RgbaPixel{}
        );
        UpdatePixelTexture(
            instance.hoverTexture,
            instance.hoverPixels,
            instance.baseTexture.width
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
            UpdatePixelTexture(
                instance.boundaryTexture,
                boundaryPixels,
                instance.baseTexture.width
            );
        }

        const std::size_t index = impl_->instances.size();
        impl_->instances.push_back(std::move(instance));
        impl_->byDefinition[definition] = index;
    }
    return true;
}

void GuiIndexedMapD3D9Runtime::Shutdown()
{
    impl_->byDefinition.clear();
    impl_->instances.clear();
    impl_->device = nullptr;
}

void GuiIndexedMapD3D9Runtime::Refresh(
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
            for (std::size_t itemId = 1;
                itemId < values.size();
                ++itemId)
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
            UpdatePixelTexture(
                instance.overlayTexture,
                instance.overlayPixels,
                instance.baseTexture.width
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
                    UpdatePixelTexture(
                        instance.overlayTexture,
                        instance.overlayPixels,
                        instance.baseTexture.width,
                        &instance.pixelIndex.boundsByItem[itemId]
                    );
                }
            }
        }
        instance.previousValues = std::move(values);
    }
}

bool GuiIndexedMapD3D9Runtime::ResolveDrawLayers(
    const gui::GuiResolvedWidget& widget,
    GuiIndexedMapD3D9DrawLayers& output
) const
{
    output = {};
    const Impl::Instance* instance = widget.definition
        ? impl_->Find(widget.definition)
        : nullptr;
    if (!instance || !widget.visible)
    {
        return false;
    }
    output.rect = CalculateMapRect(
        widget.rect,
        instance->baseTexture.width,
        instance->baseTexture.height
    );
    output.base = instance->baseTexture.texture;
    output.overlay = instance->overlayTexture;
    output.boundary = instance->boundaryTexture;
    output.hover = instance->hoverTexture;
    return output.rect.width > 0 && output.rect.height > 0;
}

bool GuiIndexedMapD3D9Runtime::ResolveDrawRect(
    const gui::GuiResolvedWidget& widget,
    gui::GuiRect& rect
) const
{
    rect = {};
    const Impl::Instance* instance = widget.definition
        ? impl_->Find(widget.definition)
        : nullptr;
    if (!instance
        || !widget.visible
        || widget.definition->type != gui::WidgetType::IndexedMap)
    {
        return false;
    }
    rect = CalculateMapRect(
        widget.rect,
        instance->baseTexture.width,
        instance->baseTexture.height
    );
    return rect.width > 0 && rect.height > 0;
}

bool GuiIndexedMapD3D9Runtime::ResolveItemAnchor(
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
    if (!anchor.valid
        || instance->baseTexture.width <= 0
        || instance->baseTexture.height <= 0)
    {
        return false;
    }
    x = drawRect.x + static_cast<int>(
        anchor.x * drawRect.width / instance->baseTexture.width
    );
    y = drawRect.y + static_cast<int>(
        anchor.y * drawRect.height / instance->baseTexture.height
    );
    return true;
}

void GuiIndexedMapD3D9Runtime::HandleMove(
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
        if (itemId == instance.hoveredItemId)
        {
            continue;
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
            continue;
        }
        instance.hoveredItemId = itemId;
        if (previousItemId < instance.pixelIndex.boundsByItem.size())
        {
            UpdatePixelTexture(
                instance.hoverTexture,
                instance.hoverPixels,
                instance.baseTexture.width,
                &instance.pixelIndex.boundsByItem[previousItemId]
            );
        }
        if (itemId < instance.pixelIndex.boundsByItem.size())
        {
            UpdatePixelTexture(
                instance.hoverTexture,
                instance.hoverPixels,
                instance.baseTexture.width,
                &instance.pixelIndex.boundsByItem[itemId]
            );
        }
    }
}

void GuiIndexedMapD3D9Runtime::HandlePress(
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

void GuiIndexedMapD3D9Runtime::HandleRelease(
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

void GuiIndexedMapD3D9Runtime::AttachItemIds(
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
        else if (event.phase == GuiActionPhase::Press
            || event.phase == GuiActionPhase::Release
            || event.phase == GuiActionPhase::Click)
        {
            event.action.clear();
        }
    }
}
