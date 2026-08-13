#include "gui_control_renderer.h"

#include <algorithm>

#include "gui_runtime.h"

namespace
{

void FillRect(
	SDL_Renderer* renderer,
	const SDL_Rect& rect,
	const SDL_Color& color
)
{
	SDL_SetRenderDrawColor(
		renderer,
		color.r,
		color.g,
		color.b,
		color.a
	);
SDL_RenderFillRect(renderer, &rect);
}

void DrawTextureOrFill(
	SDL_Renderer* renderer,
	SDL_Texture* texture,
	const SDL_Rect& rect,
	const SDL_Color& fallback
)
{
	if (texture)
	{
		SDL_RenderCopy(renderer, texture, nullptr, &rect);
		return;
	}

	FillRect(renderer, rect, fallback);
}

}

SDL_Rect CalculateGuiImageRect(
    const SDL_Rect& target,
    int sourceWidth,
    int sourceHeight,
    GuiImageScaleMode mode
)
{
    if (mode == GuiImageScaleMode::Stretch
        || sourceWidth <= 0
        || sourceHeight <= 0)
    {
        return target;
    }

    if (mode == GuiImageScaleMode::Center)
    {
        return {
            target.x + (target.w - sourceWidth) / 2,
            target.y + (target.h - sourceHeight) / 2,
            sourceWidth,
            sourceHeight
        };
    }

    const double scale = std::min(
        static_cast<double>(target.w)
            / static_cast<double>(sourceWidth),
        static_cast<double>(target.h)
            / static_cast<double>(sourceHeight)
    );
    const int width = std::max(
        1,
        static_cast<int>(sourceWidth * scale)
    );
    const int height = std::max(
        1,
        static_cast<int>(sourceHeight * scale)
    );

    return {
        target.x + (target.w - width) / 2,
        target.y + (target.h - height) / 2,
        width,
        height
    };
}

void DrawGuiImage(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const SDL_Rect& target,
    GuiImageScaleMode mode
)
{
    if (!texture)
    {
        return;
    }

    int sourceWidth = 0;
    int sourceHeight = 0;
    SDL_QueryTexture(
        texture,
        nullptr,
        nullptr,
        &sourceWidth,
        &sourceHeight
    );
    const SDL_Rect destination = CalculateGuiImageRect(
        target,
        sourceWidth,
        sourceHeight,
        mode
    );
    SDL_RenderCopy(renderer, texture, nullptr, &destination);
}

void DrawGuiColorBoxes(
    SDL_Renderer* renderer,
    const std::vector<GuiColorBoxDrawCommand>& commands
)
{
    for (const GuiColorBoxDrawCommand& command : commands)
    {
        if (command.rect.w <= 0 || command.rect.h <= 0)
        {
            continue;
        }

        SDL_SetRenderDrawColor(
            renderer,
            command.color.r,
            command.color.g,
            command.color.b,
            command.color.a
        );
        SDL_RenderFillRect(renderer, &command.rect);
    }
}

void DrawGuiProgressBar(
    SDL_Renderer* renderer,
    const SDL_Rect& rect,
    float value,
    const GuiProgressVisual& visual
)
{
    const float clampedValue = std::clamp(value, 0.0f, 1.0f);
	if (visual.drawBackground)
	{
		DrawTextureOrFill(
			renderer,
			visual.background,
			rect,
			visual.fallbackBackground
		);
	}

    SDL_Rect fillRect = rect;
	if (visual.horizontal)
	{
		fillRect.w = static_cast<int>(
			static_cast<float>(rect.w) * clampedValue
		);
		if (visual.fillFromEnd)
		{
			fillRect.x = rect.x + rect.w - fillRect.w;
		}
	}
    else
    {
        fillRect.h = static_cast<int>(
            static_cast<float>(rect.h) * clampedValue
        );
        fillRect.y = rect.y + rect.h - fillRect.h;
    }

    if (fillRect.w <= 0 || fillRect.h <= 0)
    {
        return;
    }

    SDL_RenderSetClipRect(renderer, &fillRect);
    DrawTextureOrFill(
        renderer,
        visual.fill,
        rect,
        visual.fallbackFill
    );
    SDL_RenderSetClipRect(renderer, nullptr);
}

void DrawGuiProgressBars(
	SDL_Renderer* renderer,
	const std::vector<GuiProgressDrawCommand>& commands
)
{
	for (const GuiProgressDrawCommand& command : commands)
	{
		DrawGuiProgressBar(
			renderer,
			command.rect,
			command.value,
			command.visual
		);
	}
}

void DrawGuiButton(
	SDL_Renderer* renderer,
	const SDL_Rect& rect,
	const GuiButtonVisual& visual,
	bool pressed,
	bool enabled
)
{
	SDL_Texture* texture = pressed
		? visual.pressed
		: visual.normal;

	DrawTextureOrFill(
		renderer,
		texture,
		rect,
		visual.fallbackFill
	);

	if (!texture)
	{
		SDL_SetRenderDrawColor(
			renderer,
			visual.fallbackBorder.r,
			visual.fallbackBorder.g,
			visual.fallbackBorder.b,
			visual.fallbackBorder.a
		);
		SDL_RenderDrawRect(renderer, &rect);
	}

	if (!enabled)
	{
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 72, 72, 72, 165);
		SDL_RenderFillRect(renderer, &rect);
	}
}

void DrawGuiButtons(
	SDL_Renderer* renderer,
	const std::vector<GuiButtonDrawCommand>& commands
)
{
	for (const GuiButtonDrawCommand& command : commands)
	{
		DrawGuiButton(
			renderer,
			command.rect,
			command.visual,
			command.pressed,
			command.enabled
		);
	}
}

GuiButtonEvent ProcessGuiButtonInput(
	const SDL_Rect& rect,
	GuiButtonInputState& state,
	GuiInputAction action,
	int mouseX,
	int mouseY
)
{
	const SDL_Point point{mouseX, mouseY};
	const bool inside = SDL_PointInRect(&point, &rect);

	if (action == GuiInputAction::Move)
	{
		const bool wasHovered = state.hovered;
		state.hovered = inside;

		if (!wasHovered && state.hovered)
		{
			return GuiButtonEvent::HoverEnter;
		}

		if (wasHovered && !state.hovered)
		{
			return GuiButtonEvent::HoverLeave;
		}

		return GuiButtonEvent::None;
	}

	if (action == GuiInputAction::Press)
	{
		if (!inside)
		{
			return GuiButtonEvent::None;
		}

		state.hovered = true;
		state.pressed = true;
		return GuiButtonEvent::Pressed;
	}

	const bool wasPressed = state.pressed;
	state.pressed = false;
	state.hovered = inside;

	if (wasPressed && inside)
	{
		return GuiButtonEvent::Clicked;
	}

	return GuiButtonEvent::None;
}

SDL_Rect CalculateGuiScrollbarThumbRect(
	const SDL_Rect& trackRect,
	int contentHeight,
	int viewportHeight,
	int scrollOffset,
	int minimumThumbHeight
)
{
	if (trackRect.h <= 0)
	{
		return {trackRect.x, trackRect.y, trackRect.w, 0};
	}

	const int safeContentHeight = std::max(
		viewportHeight,
		contentHeight
	);
	const int safeViewportHeight = std::max(1, viewportHeight);
	const int thumbHeight = std::min(
		trackRect.h,
		std::max(
			minimumThumbHeight,
			trackRect.h * safeViewportHeight
				/ std::max(1, safeContentHeight)
		)
	);
	const int maximumScroll = std::max(
		0,
		contentHeight - viewportHeight
	);
	const int thumbTravel = std::max(
		0,
		trackRect.h - thumbHeight
	);
	const int clampedOffset = std::clamp(
		scrollOffset,
		0,
		maximumScroll
	);
	const int thumbTop = maximumScroll > 0
		? thumbTravel * clampedOffset / maximumScroll
		: 0;

	return {
		trackRect.x,
		trackRect.y + thumbTop,
		trackRect.w,
		thumbHeight
	};
}

void DrawGuiScrollbar(
	SDL_Renderer* renderer,
	const SDL_Rect& trackRect,
	int contentHeight,
	int viewportHeight,
	int scrollOffset,
	const GuiScrollbarVisual& visual
)
{
	DrawTextureOrFill(
		renderer,
		visual.track,
		trackRect,
		visual.fallbackTrack
	);

	const SDL_Rect thumbRect = CalculateGuiScrollbarThumbRect(
		trackRect,
		contentHeight,
		viewportHeight,
		scrollOffset,
		visual.minimumThumbHeight
	);

	DrawTextureOrFill(
		renderer,
		visual.thumb,
		thumbRect,
		visual.fallbackThumb
	);
}

void DrawGuiList(
	SDL_Renderer* renderer,
	const SDL_Rect& viewport,
	const std::vector<GuiButtonDrawCommand>& itemCommands,
	int contentHeight,
	int scrollOffset,
	const SDL_Rect& scrollbarRect,
	const GuiScrollbarVisual& scrollbarVisual
)
{
	SDL_RenderSetClipRect(renderer, &viewport);
	DrawGuiButtons(renderer, itemCommands);
	SDL_RenderSetClipRect(renderer, nullptr);

	if (scrollbarRect.w <= 0
		|| scrollbarRect.h <= 0
		|| contentHeight <= viewport.h)
	{
		return;
	}

	DrawGuiScrollbar(
		renderer,
		scrollbarRect,
		contentHeight,
		viewport.h,
		scrollOffset,
		scrollbarVisual
	);
}

void DrawGuiListWidget(
	SDL_Renderer* renderer,
	const GuiListWidgetDrawData& data
)
{
	if (data.viewport.w <= 0 || data.viewport.h <= 0)
	{
		return;
	}

	SDL_RenderSetClipRect(renderer, &data.viewport);
	DrawGuiButtons(renderer, data.itemCommands);
	for (const GuiListWidgetDrawData::ItemImage& image
		: data.itemImages)
	{
		DrawGuiImage(
			renderer,
			image.texture,
			image.rect,
			image.scaleMode
		);
	}

	if (data.labels
		&& data.labelsSource.w > 0
		&& data.labelsSource.h > 0
		&& data.labelsDestination.w > 0
		&& data.labelsDestination.h > 0)
	{
		SDL_RenderCopy(
			renderer,
			data.labels,
			&data.labelsSource,
			&data.labelsDestination
		);
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	for (const GuiButtonDrawCommand& command : data.itemCommands)
	{
		if (command.enabled)
		{
			continue;
		}
		SDL_SetRenderDrawColor(renderer, 72, 72, 72, 150);
		SDL_RenderFillRect(renderer, &command.rect);
	}

	SDL_RenderSetClipRect(renderer, nullptr);

	if (data.scrollbarRect.w <= 0
		|| data.scrollbarRect.h <= 0
		|| data.contentHeight <= data.viewport.h)
	{
		return;
	}

	DrawGuiScrollbar(
		renderer,
		data.scrollbarRect,
		data.contentHeight,
		data.viewport.h,
		data.scrollOffset,
		data.scrollbarVisual
	);
}

void DrawGuiListWidget(
	SDL_Renderer* renderer,
	const GuiListRuntimeLayout& layout,
	const GuiListWidgetResources& resources
)
{
	GuiListWidgetDrawData data;
	data.viewport = {
		layout.viewport.x,
		layout.viewport.y,
		layout.viewport.width,
		layout.viewport.height
	};
	data.contentHeight = layout.contentHeight;
	data.scrollOffset = layout.scrollOffset;
	data.scrollbarRect = {
		layout.scrollbar.x,
		layout.scrollbar.y,
		layout.scrollbar.width,
		layout.scrollbar.height
	};

	const auto resolveTexture =
		[&resources](std::string_view name) -> SDL_Texture*
	{
		if (name.empty() || !resources.textureResolver)
		{
			return nullptr;
		}
		return resources.textureResolver(name);
	};

	data.itemCommands.reserve(layout.items.size());
	for (const GuiListItemRuntimeLayout& item : layout.items)
	{
		if (!item.visible)
		{
			continue;
		}

		const gui::GuiRect& rect = item.rect;
		data.itemCommands.push_back({
			SDL_Rect{
				rect.x,
				rect.y - layout.scrollOffset,
				rect.width,
				rect.height
			},
			GuiButtonVisual{
				resolveTexture(item.normalSpriteName),
				resolveTexture(item.pressedSpriteName)
			},
			item.pressed && item.enabled,
			item.enabled
		});
		if (resources.appendItemImages)
		{
			resources.appendItemImages(
				item.itemIndex,
				data.itemCommands.back().rect,
				data.itemImages
			);
		}
	}

	data.scrollbarVisual = {
		resolveTexture(layout.scrollbarTrackSprite),
		resolveTexture(layout.scrollbarThumbSprite),
		SDL_Color{35, 75, 78, SDL_ALPHA_OPAQUE},
		SDL_Color{100, 205, 200, SDL_ALPHA_OPAQUE},
		24
	};
	data.labels = resources.labels;
	if (data.labels)
	{
		data.labelsSource = {
			0,
			layout.scrollOffset,
			data.viewport.w,
			data.viewport.h
		};
		data.labelsDestination = data.viewport;
	}

	DrawGuiListWidget(renderer, data);
}

int GetGuiListRowStep(const GuiListLayout& layout)
{
	return std::max(1, layout.itemHeight + layout.rowGap);
}

int GetGuiListContentHeight(
	const GuiListLayout& layout,
	size_t itemCount
)
{
	const int columns = std::max(1, layout.columns);
	const int rowCount = static_cast<int>(
		(itemCount + static_cast<size_t>(columns) - 1)
		/ static_cast<size_t>(columns)
	);

	if (rowCount <= 0)
	{
		return 0;
	}

	return rowCount * GetGuiListRowStep(layout)
		- layout.rowGap;
}

int GetGuiListMaximumScroll(
	const GuiListLayout& layout,
	size_t itemCount
)
{
	return std::max(
		0,
		GetGuiListContentHeight(layout, itemCount)
			- layout.viewport.h
	);
}

SDL_Rect GetGuiListItemRect(
	const GuiListLayout& layout,
	size_t itemIndex,
	int scrollOffset
)
{
	const int columns = std::max(1, layout.columns);
	const size_t row = itemIndex
		/ static_cast<size_t>(columns);
	const size_t column = itemIndex
		% static_cast<size_t>(columns);

	return {
		layout.viewport.x
			+ static_cast<int>(column)
				* (layout.itemWidth + layout.columnGap),
		layout.viewport.y
			+ static_cast<int>(row)
				* GetGuiListRowStep(layout)
			- scrollOffset,
		layout.itemWidth,
		layout.itemHeight
	};
}

int HitTestGuiListItem(
	const GuiListLayout& layout,
	size_t itemCount,
	int scrollOffset,
	int mouseX,
	int mouseY
)
{
	const SDL_Point point{mouseX, mouseY};
	if (!SDL_PointInRect(&point, &layout.viewport))
	{
		return -1;
	}

	for (size_t index = 0; index < itemCount; ++index)
	{
		const SDL_Rect itemRect = GetGuiListItemRect(
			layout,
			index,
			scrollOffset
		);

		if (SDL_PointInRect(&point, &itemRect))
		{
			return static_cast<int>(index);
		}
	}

	return -1;
}

int ClampGuiListScroll(
	const GuiListLayout& layout,
	size_t itemCount,
	int scrollOffset
)
{
	return std::clamp(
		scrollOffset,
		0,
		GetGuiListMaximumScroll(layout, itemCount)
	);
}
