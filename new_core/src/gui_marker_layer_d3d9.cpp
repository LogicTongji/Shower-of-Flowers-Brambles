#include "gui_marker_layer_d3d9.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "gui_text_renderer_d3d9.h"

namespace
{

struct MarkerVertex
{
    float x;
    float y;
    float z;
    float rhw;
    D3DCOLOR color;
    float u;
    float v;
};

constexpr DWORD MarkerVertexFormat =
    D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(std::lround(
        std::clamp(value, 0.0f, 1.0f) * 255.0f
    ));
}

D3DCOLOR ToD3DColor(const gui::GuiRgbaColor& color,float opacity = 1.0f)
{
    return D3DCOLOR_ARGB(
        ToByte(color.a
            * std::clamp(
                opacity,
                0.0f,
                1.0f
            )
        ),
        ToByte(color.r),
        ToByte(color.g),
        ToByte(color.b)
    );
}
D3DCOLOR WhiteWithOpacity(float opacity)
{
    return D3DCOLOR_ARGB(
        ToByte(
            std::clamp(
                opacity,
                0.0f,
                1.0f
            )
        ),
        255,
        255,
        255
    );
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

bool RectanglesOverlap(
    const gui::GuiRect& first,
    const gui::GuiRect& second
)
{
    return first.x < second.x + second.width
        && first.x + first.width > second.x
        && first.y < second.y + second.height
        && first.y + first.height > second.y;
}

std::string SourceField(std::string source)
{
    constexpr std::string_view prefix = "item.";
    if (source.rfind(prefix, 0) == 0)
    {
        source.erase(0, prefix.size());
    }
    return source;
}

const GuiDataValue* ItemValue(
    const GuiListItem& item,
    const GuiListItem* catalogItem,
    const std::string& source
)
{
    const std::string field = SourceField(source);
    const GuiDataValue* value = field.empty()
        ? nullptr
        : item.Find(field);
	if (!value && catalogItem && !field.empty())
	{
		value = catalogItem->Find(field);
	}
	return value;
}

std::string ItemText(
    const GuiListItem& item,
    const GuiListItem* catalogItem,
    const std::string& source
)
{
    const GuiDataValue* value = ItemValue(item, catalogItem, source);
    return value ? GuiDataValueToText(*value) : std::string{};
}

double ItemNumber(
    const GuiListItem& item,
    const GuiListItem* catalogItem,
    const std::string& source,
    double defaultValue
)
{
    const GuiDataValue* value = ItemValue(item, catalogItem, source);
    return value ? GuiDataValueToNumber(*value) : defaultValue;
}

std::string FormatNumber(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

void DrawQuad(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255)
)
{
    if (!device || rect.width <= 0 || rect.height <= 0)
    {
        return;
    }
    const float left = static_cast<float>(rect.x) - 0.5f;
    const float top = static_cast<float>(rect.y) - 0.5f;
    const float right = static_cast<float>(rect.x + rect.width) - 0.5f;
    const float bottom = static_cast<float>(rect.y + rect.height) - 0.5f;
    const MarkerVertex vertices[4] = {
        {left, top, 0.0f, 1.0f, color, 0.0f, 0.0f},
        {right, top, 0.0f, 1.0f, color, 1.0f, 0.0f},
        {left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f},
        {right, bottom, 0.0f, 1.0f, color, 1.0f, 1.0f}
    };
    device->SetFVF(MarkerVertexFormat);
    device->SetTexture(0, texture);
    if (!texture)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    }
    device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP,
        2,
        vertices,
        sizeof(MarkerVertex)
    );
    if (!texture)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    }
}

void DrawTextureQuad(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255)
)
{
    if (texture)
    {
        DrawQuad(device, rect, texture, color);
    }
}

void DrawThickLine(
    IDirect3DDevice9* device,
    float startX,
    float startY,
    float endX,
    float endY,
    float width,
    D3DCOLOR color
)
{
    const float deltaX = endX - startX;
    const float deltaY = endY - startY;
    const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    if (!device || length <= 0.001f)
    {
        return;
    }
    const float halfWidth = std::max(1.0f, width) * 0.5f;
    const float offsetX = -deltaY / length * halfWidth;
    const float offsetY = deltaX / length * halfWidth;
    const MarkerVertex vertices[4] = {
        {startX + offsetX - 0.5f, startY + offsetY - 0.5f,
            0.0f, 1.0f, color, 0.0f, 0.0f},
        {startX - offsetX - 0.5f, startY - offsetY - 0.5f,
            0.0f, 1.0f, color, 0.0f, 0.0f},
        {endX + offsetX - 0.5f, endY + offsetY - 0.5f,
            0.0f, 1.0f, color, 0.0f, 0.0f},
        {endX - offsetX - 0.5f, endY - offsetY - 0.5f,
            0.0f, 1.0f, color, 0.0f, 0.0f}
    };
    device->SetFVF(MarkerVertexFormat);
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP,
        2,
        vertices,
        sizeof(MarkerVertex)
    );
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}

}

struct GuiMarkerLayerD3D9Runtime::Impl
{
    struct MarkerState
    {
        double normalizedX = 0.0;
        double normalizedY = 0.0;
        double sourceX = -1.0;
        double sourceY = -1.0;
        bool initialized = false;
    };

    struct LayerState
    {
        uint64_t hoveredId = 0;
        uint64_t selectedId = 0;
        uint64_t pressedId = 0;
        int dragOffsetX = 0;
        int dragOffsetY = 0;
        bool dragging = false;
        bool moved = false;
        uint64_t actionPressedId = 0;
    };

    struct MarkerView
    {
        const GuiListItem* item = nullptr;
		const GuiListItem* catalogItem = nullptr;
        std::size_t itemIndex = 0;
        gui::GuiRect markerRect;
        gui::GuiRect mapRect;
        gui::GuiRect containerRect;
        int anchorX = 0;
        int anchorY = 0;
        std::string stackKey;
        double stackOrder = 0.0;
        int stackOffsetX = 0;
        int stackOffsetY = 0;
        std::string portrait;
        std::string name;
        std::string description;
    };

    IDirect3DDevice9* device = nullptr;
    GuiTextRendererD3D9* textRenderer = nullptr;
    const GuiLocalizationRegistry* localization = nullptr;
    GuiMarkerD3D9TextureResolver textureResolver;
    std::shared_ptr<const GuiDataRegistry> data;
    std::unordered_map<std::string, MarkerState> markers;
    std::unordered_map<const gui::WidgetDefinition*, LayerState> layers;

    static std::string MarkerKey(
        const gui::WidgetDefinition& definition,
        uint64_t itemId
    )
    {
        return definition.name + "#" + std::to_string(itemId);
    }

    static std::string TextSlot(
        const gui::WidgetDefinition& definition,
        uint64_t itemId,
        std::string_view suffix
    )
    {
        return "marker:"
            + std::to_string(reinterpret_cast<std::uintptr_t>(&definition))
            + ":" + std::to_string(itemId)
            + ":" + std::string(suffix);
    }

    const gui::GuiResolvedWidget* FindMapWidget(
        const gui::WidgetDefinition& definition,
        const std::vector<gui::GuiResolvedWidget>& widgets
    ) const
    {
        for (const gui::GuiResolvedWidget& widget : widgets)
        {
            if (widget.definition
                && widget.definition->type == gui::WidgetType::IndexedMap
                && (definition.mapWidgetName.empty()
                    || widget.definition->name == definition.mapWidgetName))
            {
                return &widget;
            }
        }
        return nullptr;
    }

    std::vector<MarkerView> BuildViews(
        const gui::GuiResolvedWidget& layer,
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapD3D9Runtime& indexedMaps
    )
    {
        std::vector<MarkerView> output;
        if (!layer.definition || !data)
        {
            return output;
        }
        const gui::WidgetDefinition& definition = *layer.definition;
        const GuiListModel* model = data->FindList(definition.dataSource);
		const GuiListModel* catalog = definition.catalogSource.empty()
			? nullptr
			: data->FindList(definition.catalogSource);
		std::unordered_map<uint64_t, const GuiListItem*> catalogItems;
		if (catalog)
		{
			catalogItems.reserve(catalog->items.size());
			for (const GuiListItem& item : catalog->items)
			{
				catalogItems[item.id] = &item;
			}
		}
        const gui::GuiResolvedWidget* mapWidget = FindMapWidget(
            definition,
            widgets
        );
        gui::GuiRect mapRect;
        if (!model
            || !mapWidget
            || !indexedMaps.ResolveDrawRect(*mapWidget, mapRect))
        {
            return output;
        }

        if (definition.markerRect.width <= 0
            || definition.markerRect.height <= 0
            || definition.regionSource.empty())
        {
            return output;
        }
        const int markerWidth = definition.markerRect.width;
        const int markerHeight = definition.markerRect.height;
        std::unordered_set<std::string> active;
        std::unordered_set<uint64_t> activeIds;
        output.reserve(model->items.size());
        for (std::size_t index = 0; index < model->items.size(); ++index)
        {
            const GuiListItem& item = model->items[index];
			const auto catalogFound = catalogItems.find(item.id);
			const GuiListItem* catalogItem = catalogFound == catalogItems.end()
				? nullptr
				: catalogFound->second;
            const int anchorItemId = static_cast<int>(std::lround(ItemNumber(
                item,
				catalogItem,
                definition.regionSource,
                0.0
            )));
            if (anchorItemId <= 0 || anchorItemId > 65535)
            {
                continue;
            }
            int anchorX = 0;
            int anchorY = 0;
            if (!indexedMaps.ResolveItemAnchor(
                    *mapWidget,
                    static_cast<uint16_t>(anchorItemId),
                    anchorX,
                    anchorY
                ))
            {
                continue;
            }

            const std::string key = MarkerKey(definition, item.id);
            active.insert(key);
            activeIds.insert(item.id);
            MarkerState& state = markers[key];
            const double sourceX = ItemNumber(
                item,
				catalogItem,
                definition.markerXSource,
                -1.0
            );
            const double sourceY = ItemNumber(
                item,
				catalogItem,
                definition.markerYSource,
                -1.0
            );
            const bool hasSourcePosition = sourceX >= 0.0
                && sourceX <= 1.0
                && sourceY >= 0.0
                && sourceY <= 1.0;
            if (hasSourcePosition
                && (!state.initialized
                    || sourceX != state.sourceX
                    || sourceY != state.sourceY))
            {
                state.normalizedX = sourceX;
                state.normalizedY = sourceY;
                state.sourceX = sourceX;
                state.sourceY = sourceY;
                state.initialized = true;
            }
            else if (!state.initialized)
            {
                state.normalizedX = static_cast<double>(
                    anchorX - mapRect.x - markerWidth / 2
                ) / std::max(1, mapRect.width);
                state.normalizedY = static_cast<double>(
                    anchorY - mapRect.y - markerHeight - 8
                ) / std::max(1, mapRect.height);
                state.initialized = true;
            }
            state.normalizedX = std::clamp(
                state.normalizedX,
                0.0,
                std::max(
                    0.0,
                    1.0 - static_cast<double>(markerWidth)
                        / std::max(1, mapRect.width)
                )
            );
            state.normalizedY = std::clamp(
                state.normalizedY,
                0.0,
                std::max(
                    0.0,
                    1.0 - static_cast<double>(markerHeight)
                        / std::max(1, mapRect.height)
                )
            );

            MarkerView view;
            view.item = &item;
			view.catalogItem = catalogItem;
            view.itemIndex = index;
            view.mapRect = mapRect;
            view.containerRect = layer.rect;
            view.markerRect = {
                mapRect.x + static_cast<int>(
                    std::lround(state.normalizedX * mapRect.width)
                ),
                mapRect.y + static_cast<int>(
                    std::lround(state.normalizedY * mapRect.height)
                ),
                markerWidth,
                markerHeight
            };
            view.anchorX = anchorX;
            view.anchorY = anchorY;
            view.stackKey = ItemText(
                item,
				catalogItem,
                definition.markerStackSource
            );
            if (view.stackKey.empty())
            {
                view.stackKey = key;
            }
            view.stackOrder = ItemNumber(
                item,
				catalogItem,
                definition.markerStackOrderSource,
                static_cast<double>(index)
            );
            view.portrait = ItemText(
                item,
				catalogItem,
                definition.portraitSource
            );
            view.name = ItemText(
                item,
				catalogItem,
                definition.nameSource
            );
            view.description = ItemText(
                item,
				catalogItem,
                definition.descriptionSource
            );
            if (definition.localizeTooltip && localization)
            {
                view.name = localization->Resolve(view.name);
                view.description = localization->Resolve(view.description);
            }
            output.push_back(std::move(view));
        }

        std::unordered_map<std::string, std::vector<std::size_t>> groups;
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            groups[output[index].stackKey].push_back(index);
        }
        const bool horizontal =
            definition.markerStackDirection == "horizontal";
        for (auto& group : groups)
        {
            std::vector<std::size_t>& indices = group.second;
            std::stable_sort(
                indices.begin(),
                indices.end(),
                [&output](std::size_t first, std::size_t second)
                {
                    return output[first].stackOrder
                        < output[second].stackOrder;
                }
            );
            if (indices.empty())
            {
                continue;
            }
            MarkerView& first = output[indices.front()];
            MarkerState& base = markers[MarkerKey(
                definition,
                first.item->id
            )];
            const int stepX = horizontal
                ? markerWidth + definition.markerStackSpacing
                : 0;
            const int stepY = horizontal
                ? 0
                : markerHeight + definition.markerStackSpacing;
            const int lastOffsetX = stepX
                * static_cast<int>(indices.size() - 1);
            const int lastOffsetY = stepY
                * static_cast<int>(indices.size() - 1);
            base.normalizedX = std::clamp(
                base.normalizedX,
                0.0,
                std::max(
                    0.0,
                    1.0 - static_cast<double>(
                        markerWidth + lastOffsetX
                    ) / std::max(1, mapRect.width)
                )
            );
            base.normalizedY = std::clamp(
                base.normalizedY,
                0.0,
                std::max(
                    0.0,
                    1.0 - static_cast<double>(
                        markerHeight + lastOffsetY
                    ) / std::max(1, mapRect.height)
                )
            );
            const int baseX = mapRect.x + static_cast<int>(
                std::lround(base.normalizedX * mapRect.width)
            );
            const int baseY = mapRect.y + static_cast<int>(
                std::lround(base.normalizedY * mapRect.height)
            );
            for (std::size_t rank = 0; rank < indices.size(); ++rank)
            {
                MarkerView& view = output[indices[rank]];
                MarkerState& state = markers[MarkerKey(
                    definition,
                    view.item->id
                )];
                state.normalizedX = base.normalizedX;
                state.normalizedY = base.normalizedY;
                view.stackOffsetX = stepX * static_cast<int>(rank);
                view.stackOffsetY = stepY * static_cast<int>(rank);
                view.markerRect.x = baseX + view.stackOffsetX;
                view.markerRect.y = baseY + view.stackOffsetY;
            }
        }
        std::stable_sort(
            output.begin(),
            output.end(),
            [](const MarkerView& first, const MarkerView& second)
            {
                return first.stackOrder < second.stackOrder;
            }
        );

        for (auto iterator = markers.begin(); iterator != markers.end();)
        {
            if (iterator->first.rfind(definition.name + "#", 0) != 0
                || active.find(iterator->first) != active.end())
            {
                ++iterator;
            }
            else
            {
                iterator = markers.erase(iterator);
            }
        }
        LayerState& layerState = layers[&definition];
        if (activeIds.find(layerState.hoveredId) == activeIds.end())
        {
            layerState.hoveredId = 0;
        }
        if (activeIds.find(layerState.selectedId) == activeIds.end())
        {
            layerState.selectedId = 0;
        }
        if (activeIds.find(layerState.pressedId) == activeIds.end())
        {
            layerState.pressedId = 0;
            layerState.dragging = false;
            layerState.moved = false;
        }
        if (activeIds.find(layerState.actionPressedId) == activeIds.end())
        {
            layerState.actionPressedId = 0;
        }
        return output;
    }

    const MarkerView* Pick(
        const std::vector<MarkerView>& views,
        int mouseX,
        int mouseY
    ) const
    {
        for (auto iterator = views.rbegin(); iterator != views.rend(); ++iterator)
        {
            if (PointInside(iterator->markerRect, mouseX, mouseY))
            {
                return &*iterator;
            }
        }
        return nullptr;
    }

    const MarkerView* FindById(
        const std::vector<MarkerView>& views,
        uint64_t itemId
    ) const
    {
        const auto found = std::find_if(
            views.begin(),
            views.end(),
            [itemId](const MarkerView& view)
            {
                return view.item && view.item->id == itemId;
            }
        );
        return found == views.end() ? nullptr : &*found;
    }

    gui::GuiRect ActionRect(
        const gui::WidgetDefinition& definition,
        const MarkerView& view
    ) const
    {
        return {
            view.markerRect.x + definition.markerActionRect.x,
            view.markerRect.y + definition.markerActionRect.y,
            definition.markerActionRect.width,
            definition.markerActionRect.height
        };
    }

    GuiActionEvent Event(
        const gui::GuiResolvedWidget& layer,
        const MarkerView& view,
        GuiActionPhase phase,
        const std::string& action
    ) const
    {
        GuiActionEvent event;
        event.widget = &layer;
        event.phase = phase;
        event.action = action;
        event.itemId = view.item->id;
        event.hasItemId = true;
        event.sourceWidgetName = layer.definition->name;
        event.sourceListName = layer.definition->dataSource;
        event.sourceListIndex = static_cast<int>(view.itemIndex);
		if (view.catalogItem)
		{
			for (const auto& field : view.catalogItem->fields)
			{
				event.parameters[field.first] = GuiDataValueToText(
					field.second
				);
			}
		}
        for (const auto& field : view.item->fields)
        {
            event.parameters[field.first] = GuiDataValueToText(field.second);
        }
        return event;
    }

    void AddPositionParameters(
        GuiActionEvent& event,
        const gui::WidgetDefinition& definition,
        const MarkerView& view
    ) const
    {
        const auto state = markers.find(MarkerKey(
            definition,
            view.item->id
        ));
        if (state == markers.end())
        {
            return;
        }
        event.parameters["normalizedx"] = FormatNumber(
            state->second.normalizedX
        );
        event.parameters["normalizedy"] = FormatNumber(
            state->second.normalizedY
        );
        event.parameters["markerx"] = std::to_string(
            view.mapRect.x + static_cast<int>(std::lround(
                state->second.normalizedX * view.mapRect.width
            )) + view.stackOffsetX
        );
        event.parameters["markery"] = std::to_string(
            view.mapRect.y + static_cast<int>(std::lround(
                state->second.normalizedY * view.mapRect.height
            )) + view.stackOffsetY
        );
    }

    gui::GuiRect ResolveTooltipRect(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
        const std::vector<MarkerView>& views
    ) const
    {
        const int width = definition.tooltipRect.width;
        const int height = definition.tooltipRect.height;
        if (width <= 0 || height <= 0)
        {
            return {};
        }
        const bool onRight = definition.tooltipPlacement == "right";
        const auto makeRect = [&](bool right, int y)
        {
            gui::GuiRect rect{
                right
                    ? view.markerRect.x + view.markerRect.width
                    : view.markerRect.x - width,
                y,
                width,
                height
            };
            rect.x = std::clamp(
                rect.x,
                view.containerRect.x,
                std::max(
                    view.containerRect.x,
                    view.containerRect.x + view.containerRect.width - width
                )
            );
            rect.y = std::clamp(
                rect.y,
                view.containerRect.y,
                std::max(
                    view.containerRect.y,
                    view.containerRect.y + view.containerRect.height - height
                )
            );
            return rect;
        };
        const auto overlapsMarker = [&](const gui::GuiRect& rect)
        {
            if (!definition.avoidTooltipOverlap)
            {
                return false;
            }
            for (const MarkerView& marker : views)
            {
                if (marker.item->id != view.item->id
                    && RectanglesOverlap(rect, marker.markerRect))
                {
                    return true;
                }
            }
            return false;
        };
        const auto findClearRect = [&](bool right, gui::GuiRect& output)
        {
			const int searchStep = std::max(
				1,
				definition.tooltipSearchStep
			);
            for (int distance = 0;
                distance <= view.containerRect.height;
				distance += searchStep)
            {
                const int candidates[] = {
                    view.markerRect.y - distance,
                    view.markerRect.y + distance
                };
                for (int candidateY : candidates)
                {
                    gui::GuiRect candidate = makeRect(right, candidateY);
                    if (!overlapsMarker(candidate))
                    {
                        output = candidate;
                        return true;
                    }
                }
            }
            return false;
        };
        gui::GuiRect clear;
        if (findClearRect(onRight, clear)
            || findClearRect(!onRight, clear))
        {
            return clear;
        }
        return makeRect(onRight, view.markerRect.y);
    }

    void DrawLine(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
        float opacity
    ) const
    {
		if (definition.lineWidth <= 0)
		{
			return;
		}
        DrawThickLine(
            device,
            static_cast<float>(
                view.markerRect.x + view.markerRect.width / 2
            ),
            static_cast<float>(
                view.markerRect.y + view.markerRect.height
            ),
            static_cast<float>(view.anchorX),
            static_cast<float>(view.anchorY),
			static_cast<float>(definition.lineWidth),
            ToD3DColor(definition.lineColor,opacity)
        );
    }

    void DrawMarker(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
        float opacity
    ) const
    {
        const gui::GuiRect& portraitRect = definition.portraitRect;
        DrawTextureQuad(
            device,
            {
                view.markerRect.x + portraitRect.x,
                view.markerRect.y + portraitRect.y,
                portraitRect.width > 0
                    ? portraitRect.width
                    : view.markerRect.width,
                portraitRect.height > 0
                    ? portraitRect.height
                    : view.markerRect.height
            },
            textureResolver ? textureResolver(view.portrait) : nullptr,
            WhiteWithOpacity(opacity)
        );
        DrawTextureQuad(
            device,
            view.markerRect,
            textureResolver
                ? textureResolver(definition.frameSpriteName)
                : nullptr,
            WhiteWithOpacity(opacity)
        );
    }

    void DrawTooltip(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
        const std::vector<MarkerView>& views,
        float opacity
    )
    {
        const gui::GuiRect rect = ResolveTooltipRect(
            definition,
            view,
            views
        );
        if (rect.width <= 0 || rect.height <= 0)
        {
            return;
        }
        DrawQuad(device, rect, nullptr, ToD3DColor(definition.tooltipColor, opacity));
        const int padding = std::max(0, definition.tooltipPadding);
        gui::GuiTextCommand command;
        command.rect = {
            rect.x + padding,
            rect.y + padding,
            std::max(1, rect.width - padding * 2),
            std::max(1, rect.height - padding * 2)
        };
        command.text = view.name.empty()
            ? view.description
            : view.name + "\n" + view.description;
        command.font = definition.font;
        command.fontSize = definition.fontSize;
        command.color[0] = definition.textColor[0];
        command.color[1] = definition.textColor[1];
        command.color[2] = definition.textColor[2];
        command.lineSpacing = definition.lineSpacing;
        command.wrap = true;
        DrawTextureQuad(
            device,
            command.rect,
            textRenderer
                ? textRenderer->Resolve(
                    TextSlot(definition, view.item->id, "tooltip"),
                    command
                )
                : nullptr,
            WhiteWithOpacity(opacity)
        );
    }

    void DrawMarkerAction(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
        float opacity
    )
    {
        if (definition.markerActionSpriteName.empty()
            || definition.markerActionRect.width <= 0
            || definition.markerActionRect.height <= 0)
        {
            return;
        }
        const gui::GuiRect rect = ActionRect(definition, view);
        DrawTextureQuad(
            device,
            rect,
            textureResolver
                ? textureResolver(definition.markerActionSpriteName)
                : nullptr,
            WhiteWithOpacity(opacity)
        );
        if (definition.markerActionLocalizationKey.empty()
            || !textRenderer)
        {
            return;
        }
        gui::GuiTextCommand command;
        command.rect = rect;
        command.text = localization
            ? localization->Resolve(
                definition.markerActionLocalizationKey
            )
            : definition.markerActionLocalizationKey;
        command.font = definition.font;
        command.fontSize = definition.markerActionFontSize > 0
            ? definition.markerActionFontSize
			: definition.fontSize;
        command.alignment = gui::GuiTextAlignment::Center;
        command.color[0] = definition.textColor[0];
        command.color[1] = definition.textColor[1];
        command.color[2] = definition.textColor[2];
        DrawTextureQuad(
            device,
            rect,
            textRenderer->Resolve(
                TextSlot(definition, view.item->id, "action"),
                command
            ),
            WhiteWithOpacity(opacity)
        );
    }
};

GuiMarkerLayerD3D9Runtime::GuiMarkerLayerD3D9Runtime()
    : impl_(std::make_unique<Impl>())
{
}

GuiMarkerLayerD3D9Runtime::~GuiMarkerLayerD3D9Runtime()
{
    Shutdown();
}

void GuiMarkerLayerD3D9Runtime::Initialize(
    IDirect3DDevice9* device,
    GuiTextRendererD3D9& textRenderer,
    const GuiLocalizationRegistry& localization,
    GuiMarkerD3D9TextureResolver textureResolver
)
{
    Shutdown();
    impl_->device = device;
    impl_->textRenderer = &textRenderer;
    impl_->localization = &localization;
    impl_->textureResolver = std::move(textureResolver);
}

void GuiMarkerLayerD3D9Runtime::Shutdown()
{
    impl_->markers.clear();
    impl_->layers.clear();
    impl_->data.reset();
    impl_->textureResolver = {};
    impl_->device = nullptr;
    impl_->textRenderer = nullptr;
    impl_->localization = nullptr;
}

void GuiMarkerLayerD3D9Runtime::SetData(
    std::shared_ptr<const GuiDataRegistry> data
)
{
    impl_->data = std::move(data);
}

bool GuiMarkerLayerD3D9Runtime::RegisterCustomWidget(
    gui::GuiCustomWidgetRegistry& registry,
    const GuiIndexedMapD3D9Runtime& indexedMaps
)
{
    gui::GuiCustomWidgetHandler handler;
    handler.draw = [this, &indexedMaps](
        const gui::GuiResolvedWidget& layer,
        const gui::GuiCustomWidgetContext& context)
    {
        if (!context.sceneWidgets)
        {
            return;
        }
        DrawWidget(layer, *context.sceneWidgets, indexedMaps);
    };
    handler.event = [this, &indexedMaps](
        const gui::GuiResolvedWidget&,
        const gui::GuiCustomWidgetContext& context,
        const gui::GuiCustomInputEvent& event)
    {
        if (!context.sceneWidgets)
        {
            return false;
        }
        GuiMarkerLayerD3D9InputResult result;
        switch (event.type)
        {
        case gui::GuiCustomInputEventType::PointerMove:
        case gui::GuiCustomInputEventType::PointerLeave:
        case gui::GuiCustomInputEventType::Cancel:
            result = HandleMove(
                *context.sceneWidgets,
                indexedMaps,
                event.type == gui::GuiCustomInputEventType::PointerMove
                    ? event.mouseX : -1,
                event.type == gui::GuiCustomInputEventType::PointerMove
                    ? event.mouseY : -1
            );
            break;
        case gui::GuiCustomInputEventType::PointerDown:
            if (event.button != gui::GuiCustomPointerButton::Left)
            {
                return false;
            }
            result = HandlePress(
                *context.sceneWidgets,
                indexedMaps,
                event.mouseX,
                event.mouseY
            );
            break;
        case gui::GuiCustomInputEventType::PointerUp:
            if (event.button != gui::GuiCustomPointerButton::Left)
            {
                return false;
            }
            result = HandleRelease(
                *context.sceneWidgets,
                indexedMaps,
                event.mouseX,
                event.mouseY
            );
            break;
        default:
            return false;
        }
        if (context.emittedEvents && !result.events.empty())
        {
            context.emittedEvents->insert(
                context.emittedEvents->end(),
                std::make_move_iterator(result.events.begin()),
                std::make_move_iterator(result.events.end())
            );
        }
        return result.consumed;
    };
    handler.globalInput = true;
    return registry.Register("marker_layer", std::move(handler));
}

bool GuiMarkerLayerD3D9Runtime::DrawWidget(
    const gui::GuiResolvedWidget& layer,
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapD3D9Runtime& indexedMaps
)
{
    if (!impl_->device
        || !layer.visible
        || layer.opacity <= 0.0f
        || !layer.definition
        || layer.definition->type != gui::WidgetType::MarkerLayer)
    {
        return false;
    }
    const std::vector<Impl::MarkerView> views = impl_->BuildViews(
        layer,
        widgets,
        indexedMaps
    );
    for (const Impl::MarkerView& view : views)
    {
        impl_->DrawLine(*layer.definition, view,layer.opacity);
    }
    for (const Impl::MarkerView& view : views)
    {
        impl_->DrawMarker(*layer.definition, view,layer.opacity);
    }
    const Impl::LayerState& state = impl_->layers[layer.definition];
    const uint64_t tooltipId = state.hoveredId != 0
        ? state.hoveredId
        : state.selectedId;
    if (const Impl::MarkerView* tooltip = impl_->FindById(
            views,
            tooltipId
        ))
    {
        impl_->DrawTooltip(*layer.definition, *tooltip, views,layer.opacity);
    }
    if (const Impl::MarkerView* selected = impl_->FindById(
            views,
            state.selectedId
        ))
    {
        impl_->DrawMarkerAction(*layer.definition, *selected,layer.opacity);
    }
    return true;
}

GuiMarkerLayerD3D9InputResult GuiMarkerLayerD3D9Runtime::HandleMove(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapD3D9Runtime& indexedMaps,
    int mouseX,
    int mouseY
)
{
    GuiMarkerLayerD3D9InputResult result;
    for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
    {
        if (!iterator->visible
            || !iterator->enabled
            || iterator->opacity <= 0.0f
            || !iterator->definition
            || iterator->definition->type != gui::WidgetType::MarkerLayer)
        {
            continue;
        }
        const std::vector<Impl::MarkerView> views = impl_->BuildViews(
            *iterator,
            widgets,
            indexedMaps
        );
        Impl::LayerState& state = impl_->layers[iterator->definition];
        if (state.selectedId != 0)
        {
            if (const Impl::MarkerView* selected = impl_->FindById(
                    views,
                    state.selectedId
                ))
            {
                if (PointInside(
                        impl_->ActionRect(*iterator->definition, *selected),
                        mouseX,
                        mouseY
                    ))
                {
                    result.consumed = true;
                    return result;
                }
            }
        }
        if (state.dragging && state.pressedId != 0)
        {
            const Impl::MarkerView* dragged = impl_->FindById(
                views,
                state.pressedId
            );
            if (!dragged)
            {
                state = {};
                continue;
            }
            int groupOffsetX = 0;
            int groupOffsetY = 0;
            for (const Impl::MarkerView& grouped : views)
            {
                if (grouped.stackKey == dragged->stackKey)
                {
                    groupOffsetX = std::max(
                        groupOffsetX,
                        grouped.stackOffsetX
                    );
                    groupOffsetY = std::max(
                        groupOffsetY,
                        grouped.stackOffsetY
                    );
                }
            }
            const int nextX = std::clamp(
                mouseX - state.dragOffsetX - dragged->stackOffsetX,
                dragged->mapRect.x,
                dragged->mapRect.x + dragged->mapRect.width
                    - dragged->markerRect.width - groupOffsetX
            );
            const int nextY = std::clamp(
                mouseY - state.dragOffsetY - dragged->stackOffsetY,
                dragged->mapRect.y,
                dragged->mapRect.y + dragged->mapRect.height
                    - dragged->markerRect.height - groupOffsetY
            );
            const double normalizedX = static_cast<double>(
                nextX - dragged->mapRect.x
            ) / std::max(1, dragged->mapRect.width);
            const double normalizedY = static_cast<double>(
                nextY - dragged->mapRect.y
            ) / std::max(1, dragged->mapRect.height);
            for (const Impl::MarkerView& grouped : views)
            {
                if (grouped.stackKey == dragged->stackKey)
                {
                    Impl::MarkerState& groupedState = impl_->markers[
                        Impl::MarkerKey(
                            *iterator->definition,
                            grouped.item->id
                        )
                    ];
                    groupedState.normalizedX = normalizedX;
                    groupedState.normalizedY = normalizedY;
                }
            }
            state.moved = state.moved
                || nextX + dragged->stackOffsetX != dragged->markerRect.x
                || nextY + dragged->stackOffsetY != dragged->markerRect.y;
            result.consumed = true;
            if (!iterator->definition->actions.onDrag.empty())
            {
                GuiActionEvent event = impl_->Event(
                    *iterator,
                    *dragged,
                    GuiActionPhase::Drag,
                    iterator->definition->actions.onDrag
                );
                impl_->AddPositionParameters(
                    event,
                    *iterator->definition,
                    *dragged
                );
                result.events.push_back(std::move(event));
            }
            return result;
        }

        const Impl::MarkerView* hovered = impl_->Pick(
            views,
            mouseX,
            mouseY
        );
        const uint64_t nextId = hovered ? hovered->item->id : 0;
        if (nextId != state.hoveredId)
        {
            if (const Impl::MarkerView* previous = impl_->FindById(
                    views,
                    state.hoveredId
                ))
            {
                const std::string& action =
                    iterator->definition->actions.onHoverLeave;
                if (!action.empty())
                {
                    result.events.push_back(impl_->Event(
                        *iterator,
                        *previous,
                        GuiActionPhase::HoverLeave,
                        action
                    ));
                }
            }
            state.hoveredId = nextId;
            if (hovered)
            {
                const std::string& action =
                    iterator->definition->actions.onHoverEnter;
                if (!action.empty())
                {
                    result.events.push_back(impl_->Event(
                        *iterator,
                        *hovered,
                        GuiActionPhase::HoverEnter,
                        action
                    ));
                }
            }
        }
        if (hovered)
        {
            result.consumed = true;
            return result;
        }
    }
    return result;
}

GuiMarkerLayerD3D9InputResult GuiMarkerLayerD3D9Runtime::HandlePress(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapD3D9Runtime& indexedMaps,
    int mouseX,
    int mouseY
)
{
    GuiMarkerLayerD3D9InputResult result;
    for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
    {
        if (!iterator->visible
            || !iterator->enabled
            || iterator->opacity <= 0.0f
            || !iterator->definition
            || iterator->definition->type != gui::WidgetType::MarkerLayer)
        {
            continue;
        }
        const std::vector<Impl::MarkerView> views = impl_->BuildViews(
            *iterator,
            widgets,
            indexedMaps
        );
        Impl::LayerState& state = impl_->layers[iterator->definition];
        if (state.selectedId != 0
            && !iterator->definition->markerActionName.empty())
        {
            if (const Impl::MarkerView* selected = impl_->FindById(
                    views,
                    state.selectedId
                ))
            {
                if (PointInside(
                        impl_->ActionRect(*iterator->definition, *selected),
                        mouseX,
                        mouseY
                    ))
                {
                    state.actionPressedId = selected->item->id;
                    result.consumed = true;
                    return result;
                }
            }
        }
        const Impl::MarkerView* pressed = impl_->Pick(
            views,
            mouseX,
            mouseY
        );
        if (!pressed)
        {
            continue;
        }
        state.pressedId = pressed->item->id;
        state.dragOffsetX = mouseX - pressed->markerRect.x;
        state.dragOffsetY = mouseY - pressed->markerRect.y;
        state.dragging = iterator->definition->draggable;
        state.moved = false;
        result.consumed = true;
        if (!iterator->definition->actions.onPress.empty())
        {
            result.events.push_back(impl_->Event(
                *iterator,
                *pressed,
                GuiActionPhase::Press,
                iterator->definition->actions.onPress
            ));
        }
        if (state.dragging
            && !iterator->definition->actions.onDragStart.empty())
        {
            result.events.push_back(impl_->Event(
                *iterator,
                *pressed,
                GuiActionPhase::DragStart,
                iterator->definition->actions.onDragStart
            ));
        }
        return result;
    }
    return result;
}

GuiMarkerLayerD3D9InputResult GuiMarkerLayerD3D9Runtime::HandleRelease(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapD3D9Runtime& indexedMaps,
    int mouseX,
    int mouseY
)
{
    GuiMarkerLayerD3D9InputResult result;
    for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
    {
        if (!iterator->visible
            || iterator->opacity <= 0.0f
            || !iterator->definition
            || iterator->definition->type != gui::WidgetType::MarkerLayer)
        {
            continue;
        }
        Impl::LayerState& state = impl_->layers[iterator->definition];
        if (state.actionPressedId != 0)
        {
            const std::vector<Impl::MarkerView> views = impl_->BuildViews(
                *iterator,
                widgets,
                indexedMaps
            );
            const Impl::MarkerView* selected = impl_->FindById(
                views,
                state.actionPressedId
            );
            result.consumed = true;
            if (selected
                && PointInside(
                    impl_->ActionRect(*iterator->definition, *selected),
                    mouseX,
                    mouseY
                )
                && !iterator->definition->markerActionName.empty())
            {
                result.events.push_back(impl_->Event(
                    *iterator,
                    *selected,
                    GuiActionPhase::Click,
                    iterator->definition->markerActionName
                ));
            }
            state.actionPressedId = 0;
            return result;
        }
        if (state.pressedId == 0)
        {
            continue;
        }
        const std::vector<Impl::MarkerView> views = impl_->BuildViews(
            *iterator,
            widgets,
            indexedMaps
        );
        const Impl::MarkerView* pressed = impl_->FindById(
            views,
            state.pressedId
        );
        if (!pressed)
        {
            state = {};
            continue;
        }
        result.consumed = true;
        if (!iterator->definition->actions.onRelease.empty())
        {
            result.events.push_back(impl_->Event(
                *iterator,
                *pressed,
                GuiActionPhase::Release,
                iterator->definition->actions.onRelease
            ));
        }
        if (state.dragging && state.moved)
        {
            if (!iterator->definition->actions.onDragEnd.empty())
            {
                GuiActionEvent event = impl_->Event(
                    *iterator,
                    *pressed,
                    GuiActionPhase::DragEnd,
                    iterator->definition->actions.onDragEnd
                );
                impl_->AddPositionParameters(
                    event,
                    *iterator->definition,
                    *pressed
                );
                result.events.push_back(std::move(event));
            }
        }
        else if (PointInside(pressed->markerRect, mouseX, mouseY))
        {
            state.selectedId = state.selectedId == pressed->item->id
                ? 0
                : pressed->item->id;
            if (!iterator->definition->actions.onClick.empty())
            {
                result.events.push_back(impl_->Event(
                    *iterator,
                    *pressed,
                    GuiActionPhase::Click,
                    iterator->definition->actions.onClick
                ));
            }
        }
        state.pressedId = 0;
        state.dragging = false;
        state.moved = false;
        return result;
    }
    return result;
}
