#include "gui_marker_layer_macos.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "gui_control_renderer.h"

namespace
{

uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(std::lround(
        std::clamp(value, 0.0f, 1.0f) * 255.0f
    ));
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

std::string SourceField(std::string source, std::string fallback)
{
    constexpr std::string_view prefix = "item.";
    if (source.rfind(prefix, 0) == 0)
    {
        source.erase(0, prefix.size());
    }
    return source.empty() ? std::move(fallback) : source;
}

std::string ItemText(
    const GuiListItem& item,
    const std::string& source,
    const std::string& fallback
)
{
    const GuiDataValue* value = item.Find(
        SourceField(source, fallback)
    );
    return value ? GuiDataValueToText(*value) : std::string{};
}

double ItemNumber(
    const GuiListItem& item,
    const std::string& source,
    const std::string& fallback,
    double defaultValue
)
{
    const GuiDataValue* value = item.Find(
        SourceField(source, fallback)
    );
    return value ? GuiDataValueToNumber(*value) : defaultValue;
}

std::string FormatNumber(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
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

struct GuiMarkerLayerMacRuntime::Impl
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
        int pressedIndex = -1;
        int dragOffsetX = 0;
        int dragOffsetY = 0;
        bool dragging = false;
        bool moved = false;
		uint64_t actionPressedId = 0;
    };

    struct TooltipTexture
    {
        SDL_Texture* texture = nullptr;
        std::vector<uint8_t> pixels;
        std::string text;
        int width = 0;
        int height = 0;
    };

    struct MarkerView
    {
        const GuiListItem* item = nullptr;
        std::size_t itemIndex = 0;
        gui::GuiRect markerRect;
        gui::GuiRect mapRect;
		gui::GuiRect containerRect;
        int anchorX = 0;
        int anchorY = 0;
        uint16_t regionId = 0;
		std::string stackKey;
		double stackOrder = 0.0;
		int stackIndex = 0;
		int stackOffsetX = 0;
		int stackOffsetY = 0;
        std::string portrait;
        std::string name;
        std::string description;
    };

    SDL_Renderer* renderer = nullptr;
    const GuiMacFontSet* fonts = nullptr;
    const GuiLocalizationRegistry* localization = nullptr;
    GuiMarkerTextureResolver textureResolver;
    std::shared_ptr<const GuiDataRegistry> data;
    std::unordered_map<std::string, MarkerState> markers;
    std::unordered_map<const gui::WidgetDefinition*, LayerState> layers;
    std::unordered_map<std::string, TooltipTexture> tooltips;
	std::unordered_map<
		const gui::WidgetDefinition*,
		TooltipTexture
	> actionLabels;

    static std::string MarkerKey(
        const gui::WidgetDefinition& definition,
        uint64_t itemId
    )
    {
        return definition.name + "#" + std::to_string(itemId);
    }

    const gui::GuiResolvedWidget* FindMapWidget(
        const gui::WidgetDefinition& definition,
        const std::vector<gui::GuiResolvedWidget>& widgets
    ) const
    {
        for (const gui::GuiResolvedWidget& widget : widgets)
        {
            if (widget.definition
                && widget.definition->type
                    == gui::WidgetType::IndexedMap
                && (definition.mapWidgetName.empty()
                    || widget.definition->name
                        == definition.mapWidgetName))
            {
                return &widget;
            }
        }
        return nullptr;
    }

    std::vector<MarkerView> BuildViews(
        const gui::GuiResolvedWidget& layer,
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const GuiIndexedMapMacRuntime& indexedMaps
    )
    {
        std::vector<MarkerView> output;
        if (!layer.definition || !data)
        {
            return output;
        }
        const gui::WidgetDefinition& definition = *layer.definition;
        const GuiListModel* model = data->FindList(definition.dataSource);
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

        const int markerWidth = definition.markerRect.width > 0
            ? definition.markerRect.width
            : 68;
        const int markerHeight = definition.markerRect.height > 0
            ? definition.markerRect.height
            : 84;
        std::unordered_set<std::string> active;
        std::unordered_set<uint64_t> activeIds;
        output.reserve(model->items.size());
        for (std::size_t index = 0; index < model->items.size(); ++index)
        {
            const GuiListItem& item = model->items[index];
            const int region = static_cast<int>(std::lround(ItemNumber(
                item,
                definition.regionSource,
                "regionid",
                0.0
            )));
            if (region <= 0 || region > 65535)
            {
                continue;
            }
            int anchorX = 0;
            int anchorY = 0;
            if (!indexedMaps.ResolveItemAnchor(
                    *mapWidget,
                    static_cast<uint16_t>(region),
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
                definition.markerXSource,
                "x",
                -1.0
            );
            const double sourceY = ItemNumber(
                item,
                definition.markerYSource,
                "y",
                -1.0
            );
            const bool sourcePosition = sourceX >= 0.0
                && sourceX <= 1.0
                && sourceY >= 0.0
                && sourceY <= 1.0;
            if (sourcePosition
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
            view.regionId = static_cast<uint16_t>(region);
			view.stackKey = ItemText(
				item,
				definition.markerStackSource,
				""
			);
			if (view.stackKey.empty())
			{
				view.stackKey = key;
			}
			view.stackOrder = ItemNumber(
				item,
				definition.markerStackOrderSource,
				"assignmentorder",
				static_cast<double>(index)
			);
            view.portrait = ItemText(
                item,
                definition.portraitSource,
                "portrait"
            );
            view.name = ItemText(
                item,
                definition.nameSource,
                "namekey"
            );
            view.description = ItemText(
                item,
                definition.descriptionSource,
                "descriptionkey"
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
				view.stackIndex = static_cast<int>(rank);
				view.stackOffsetX = stepX * view.stackIndex;
				view.stackOffsetY = stepY * view.stackIndex;
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
        for (auto iterator = tooltips.begin(); iterator != tooltips.end();)
        {
            if (iterator->first.rfind(definition.name + "#", 0) != 0
                || active.find(iterator->first) != active.end())
            {
                ++iterator;
            }
            else
            {
                SDL_DestroyTexture(iterator->second.texture);
                iterator = tooltips.erase(iterator);
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
            layerState.pressedIndex = -1;
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
        for (const auto& field : view.item->fields)
        {
            event.parameters[field.first] = GuiDataValueToText(
                field.second
            );
        }
        event.parameters["regionid"] = std::to_string(view.regionId);
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
        event.parameters["markerx"] = std::to_string(view.markerRect.x);
        event.parameters["markery"] = std::to_string(view.markerRect.y);
    }

    TooltipTexture& ResolveTooltip(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
        int width,
        int height
    )
    {
        const std::string key = MarkerKey(definition, view.item->id);
        TooltipTexture& tooltip = tooltips[key];
        const std::string text = view.name.empty()
            ? view.description
            : view.name + "\n" + view.description;
        if (tooltip.texture
            && tooltip.text == text
            && tooltip.width == width
            && tooltip.height == height)
        {
            return tooltip;
        }
        SDL_DestroyTexture(tooltip.texture);
        tooltip.texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height
        );
        if (tooltip.texture)
        {
            SDL_SetTextureBlendMode(
                tooltip.texture,
                SDL_BLENDMODE_BLEND
            );
            gui::GuiTextCommand command;
            command.rect = {0, 0, width, height};
            command.text = text;
            command.font = definition.font;
            command.fontSize = definition.fontSize > 0
                ? definition.fontSize
                : 14;
            command.color[0] = definition.textColor[0];
            command.color[1] = definition.textColor[1];
            command.color[2] = definition.textColor[2];
            command.lineSpacing = definition.lineSpacing;
            command.wrap = true;
            UpdateGuiTextCommandTexture(
                tooltip.texture,
                tooltip.pixels,
                command,
                *fonts
            );
        }
        tooltip.text = text;
        tooltip.width = width;
        tooltip.height = height;
        return tooltip;
    }

    void DrawLine(
        const gui::WidgetDefinition& definition,
        const MarkerView& view
    ) const
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(
            renderer,
            ToByte(definition.lineColor.r),
            ToByte(definition.lineColor.g),
            ToByte(definition.lineColor.b),
            ToByte(definition.lineColor.a)
        );
        const int startX = view.markerRect.x + view.markerRect.width / 2;
        const int startY = view.markerRect.y + view.markerRect.height;
        const int width = std::max(1, definition.lineWidth);
        const int firstOffset = -(width - 1) / 2;
        for (int offset = 0; offset < width; ++offset)
        {
            SDL_RenderDrawLine(
                renderer,
                startX + firstOffset + offset,
                startY,
                view.anchorX + firstOffset + offset,
                view.anchorY
            );
        }
    }

    void DrawMarker(
        const gui::WidgetDefinition& definition,
        const MarkerView& view
    ) const
    {
        SDL_Texture* portrait = textureResolver
            ? textureResolver(view.portrait)
            : nullptr;
        const gui::GuiRect& portraitRect = definition.portraitRect;
        DrawGuiImage(
            renderer,
            portrait,
            SDL_Rect{
                view.markerRect.x + portraitRect.x,
                view.markerRect.y + portraitRect.y,
                portraitRect.width > 0
                    ? portraitRect.width
                    : view.markerRect.width,
                portraitRect.height > 0
                    ? portraitRect.height
                    : view.markerRect.height
            },
            GuiImageScaleMode::Stretch
        );
        SDL_Texture* frame = textureResolver
            ? textureResolver(definition.frameSpriteName)
            : nullptr;
        DrawGuiImage(
            renderer,
            frame,
            SDL_Rect{
                view.markerRect.x,
                view.markerRect.y,
                view.markerRect.width,
                view.markerRect.height
            },
            GuiImageScaleMode::Stretch
        );
    }

    gui::GuiRect ResolveTooltipRect(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
		const std::vector<MarkerView>& views
    ) const
    {
        const int width = definition.tooltipRect.width > 0
            ? definition.tooltipRect.width
            : 280;
        const int height = definition.tooltipRect.height > 0
            ? definition.tooltipRect.height
            : 140;
		const bool onRight = definition.tooltipPlacement == "right";
		auto makeRect = [&](bool right, int y)
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
					view.containerRect.x
						+ view.containerRect.width - width
				)
			);
			rect.y = std::clamp(
				rect.y,
				view.containerRect.y,
				std::max(
					view.containerRect.y,
					view.containerRect.y
						+ view.containerRect.height - height
				)
			);
			return rect;
		};
		auto overlapsMarker = [&](const gui::GuiRect& rect)
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
		const int initialY = view.markerRect.y;
		auto findClearRect = [&](bool right, gui::GuiRect& output)
		{
			for (int distance = 0;
				distance <= view.containerRect.height;
				distance += 12)
			{
				const int candidates[] = {
					initialY - distance,
					initialY + distance
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
		return makeRect(onRight, initialY);
	}

    void DrawTooltip(
        const gui::WidgetDefinition& definition,
        const MarkerView& view,
		const std::vector<MarkerView>& views
    )
    {
		const gui::GuiRect rect = ResolveTooltipRect(
			definition,
			view,
			views
		);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(
            renderer,
            ToByte(definition.tooltipColor.r),
            ToByte(definition.tooltipColor.g),
            ToByte(definition.tooltipColor.b),
            ToByte(definition.tooltipColor.a)
        );
        const SDL_Rect background{
            rect.x,
            rect.y,
            rect.width,
            rect.height
        };
        SDL_RenderFillRect(renderer, &background);
        const int padding = std::max(0, definition.tooltipPadding);
        const int textWidth = std::max(1, rect.width - padding * 2);
        const int textHeight = std::max(1, rect.height - padding * 2);
        TooltipTexture& tooltip = ResolveTooltip(
            definition,
            view,
            textWidth,
            textHeight
        );
        if (tooltip.texture)
        {
            const SDL_Rect destination{
                rect.x + padding,
                rect.y + padding,
                textWidth,
                textHeight
            };
            SDL_RenderCopy(
                renderer,
                tooltip.texture,
                nullptr,
                &destination
            );
        }
    }

	void DrawMarkerAction(
		const gui::WidgetDefinition& definition,
		const MarkerView& view
	)
	{
		if (definition.markerActionSpriteName.empty()
			|| definition.markerActionRect.width <= 0
			|| definition.markerActionRect.height <= 0)
		{
			return;
		}
		SDL_Texture* texture = textureResolver
			? textureResolver(definition.markerActionSpriteName)
			: nullptr;
		const gui::GuiRect rect = ActionRect(definition, view);
		DrawGuiImage(
			renderer,
			texture,
			SDL_Rect{rect.x, rect.y, rect.width, rect.height},
			GuiImageScaleMode::Stretch
		);
		if (definition.markerActionLocalizationKey.empty()
			|| !fonts)
		{
			return;
		}
		std::string text = definition.markerActionLocalizationKey;
		if (localization)
		{
			text = localization->Resolve(text);
		}
		TooltipTexture& label = actionLabels[&definition];
		if (!label.texture
			|| label.text != text
			|| label.width != rect.width
			|| label.height != rect.height)
		{
			SDL_DestroyTexture(label.texture);
			label.texture = SDL_CreateTexture(
				renderer,
				SDL_PIXELFORMAT_RGBA32,
				SDL_TEXTUREACCESS_STREAMING,
				rect.width,
				rect.height
			);
			if (label.texture)
			{
				SDL_SetTextureBlendMode(
					label.texture,
					SDL_BLENDMODE_BLEND
				);
				gui::GuiTextCommand command;
				command.rect = {0, 0, rect.width, rect.height};
				command.text = text;
				command.font = definition.font;
				command.fontSize = definition.markerActionFontSize > 0
					? definition.markerActionFontSize : 11;
				command.alignment = gui::GuiTextAlignment::Center;
				command.color[0] = definition.textColor[0];
				command.color[1] = definition.textColor[1];
				command.color[2] = definition.textColor[2];
				UpdateGuiTextCommandTexture(
					label.texture,
					label.pixels,
					command,
					*fonts
				);
			}
			label.text = text;
			label.width = rect.width;
			label.height = rect.height;
		}
		if (label.texture)
		{
			const SDL_Rect destination = {
				rect.x,
				rect.y,
				rect.width,
				rect.height
			};
			SDL_RenderCopy(
				renderer,
				label.texture,
				nullptr,
				&destination
			);
		}
	}

    const gui::GuiResolvedWidget* FindLayer(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const gui::WidgetDefinition* definition = nullptr
    ) const
    {
        for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
        {
            if (iterator->visible
                && iterator->definition
                && iterator->definition->type
                    == gui::WidgetType::MarkerLayer
                && (!definition || iterator->definition == definition))
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
};

GuiMarkerLayerMacRuntime::GuiMarkerLayerMacRuntime()
    : impl_(std::make_unique<Impl>())
{
}

GuiMarkerLayerMacRuntime::~GuiMarkerLayerMacRuntime()
{
    Shutdown();
}

void GuiMarkerLayerMacRuntime::Initialize(
    SDL_Renderer* renderer,
    const GuiMacFontSet& fonts,
    const GuiLocalizationRegistry& localization,
    GuiMarkerTextureResolver textureResolver
)
{
    Shutdown();
    impl_->renderer = renderer;
    impl_->fonts = &fonts;
    impl_->localization = &localization;
    impl_->textureResolver = std::move(textureResolver);
}

void GuiMarkerLayerMacRuntime::Shutdown()
{
    for (auto& tooltip : impl_->tooltips)
    {
        SDL_DestroyTexture(tooltip.second.texture);
    }
    impl_->tooltips.clear();
	for (auto& label : impl_->actionLabels)
	{
		SDL_DestroyTexture(label.second.texture);
	}
	impl_->actionLabels.clear();
    impl_->markers.clear();
    impl_->layers.clear();
    impl_->data.reset();
    impl_->textureResolver = {};
    impl_->renderer = nullptr;
    impl_->fonts = nullptr;
    impl_->localization = nullptr;
}

void GuiMarkerLayerMacRuntime::SetData(
    std::shared_ptr<const GuiDataRegistry> data
)
{
    impl_->data = std::move(data);
}

bool GuiMarkerLayerMacRuntime::DrawWidget(
    const gui::GuiResolvedWidget& layer,
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapMacRuntime& indexedMaps
)
{
    if (!impl_->renderer
        || !layer.visible
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
        impl_->DrawLine(*layer.definition, view);
    }
    for (const Impl::MarkerView& view : views)
    {
        impl_->DrawMarker(*layer.definition, view);
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
        impl_->DrawTooltip(*layer.definition, *tooltip, views);
    }
	if (const Impl::MarkerView* selected = impl_->FindById(
			views,
			state.selectedId
		))
	{
		impl_->DrawMarkerAction(*layer.definition, *selected);
	}
    return true;
}

GuiMarkerLayerInputResult GuiMarkerLayerMacRuntime::HandleMove(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapMacRuntime& indexedMaps,
    int mouseX,
    int mouseY
)
{
    GuiMarkerLayerInputResult result;
    for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
    {
        if (!iterator->visible
            || !iterator->definition
            || iterator->definition->type
                != gui::WidgetType::MarkerLayer)
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
						impl_->ActionRect(
							*iterator->definition,
							*selected
						),
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
					- dragged->markerRect.width
					- groupOffsetX
            );
            const int nextY = std::clamp(
				mouseY - state.dragOffsetY - dragged->stackOffsetY,
                dragged->mapRect.y,
                dragged->mapRect.y + dragged->mapRect.height
					- dragged->markerRect.height
					- groupOffsetY
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
				|| nextX + dragged->stackOffsetX
					!= dragged->markerRect.x
				|| nextY + dragged->stackOffsetY
					!= dragged->markerRect.y;
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

GuiMarkerLayerInputResult GuiMarkerLayerMacRuntime::HandlePress(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapMacRuntime& indexedMaps,
    int mouseX,
    int mouseY
)
{
    GuiMarkerLayerInputResult result;
    for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
    {
        if (!iterator->visible
            || !iterator->definition
            || iterator->definition->type
                != gui::WidgetType::MarkerLayer)
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
						impl_->ActionRect(
							*iterator->definition,
							*selected
						),
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
        state.pressedIndex = static_cast<int>(pressed->itemIndex);
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

GuiMarkerLayerInputResult GuiMarkerLayerMacRuntime::HandleRelease(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const GuiIndexedMapMacRuntime& indexedMaps,
    int mouseX,
    int mouseY
)
{
    GuiMarkerLayerInputResult result;
    for (auto iterator = widgets.rbegin(); iterator != widgets.rend(); ++iterator)
    {
        if (!iterator->visible
            || !iterator->definition
            || iterator->definition->type
                != gui::WidgetType::MarkerLayer)
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
					impl_->ActionRect(
						*iterator->definition,
						*selected
					),
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
        state.pressedIndex = -1;
        state.dragging = false;
        state.moved = false;
        return result;
    }
    return result;
}

GuiMarkerLayerMacResourceStats
GuiMarkerLayerMacRuntime::ResourceStats() const
{
    GuiMarkerLayerMacResourceStats stats;
    for (const auto& tooltip : impl_->tooltips)
    {
        stats.cpuBytes += tooltip.second.pixels.capacity();
        if (tooltip.second.texture)
        {
            ++stats.textureCount;
            stats.textureBytes += TextureBytes(tooltip.second.texture);
        }
    }
	for (const auto& label : impl_->actionLabels)
	{
		stats.cpuBytes += label.second.pixels.capacity();
		if (label.second.texture)
		{
			++stats.textureCount;
			stats.textureBytes += TextureBytes(label.second.texture);
		}
	}
    stats.cpuBytes += impl_->markers.size()
        * sizeof(Impl::MarkerState);
    stats.cpuBytes += impl_->layers.size()
        * sizeof(Impl::LayerState);
    return stats;
}
