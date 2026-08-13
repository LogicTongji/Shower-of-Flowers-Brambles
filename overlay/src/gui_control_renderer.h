#pragma once

#include <SDL.h>

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

struct GuiListRuntimeLayout;

struct GuiButtonVisual
{
	SDL_Texture* normal = nullptr;
	SDL_Texture* pressed = nullptr;
	SDL_Color fallbackFill{15, 45, 50, SDL_ALPHA_OPAQUE};
	SDL_Color fallbackBorder{80, 190, 190, SDL_ALPHA_OPAQUE};
};

struct GuiButtonDrawCommand
{
	SDL_Rect rect{0, 0, 0, 0};
	GuiButtonVisual visual;
	bool pressed = false;
	bool enabled = true;
};

enum class GuiImageScaleMode
{
    Stretch,
    PreserveAspect,
    Center
};

SDL_Rect CalculateGuiImageRect(
    const SDL_Rect& target,
    int sourceWidth,
    int sourceHeight,
    GuiImageScaleMode mode
);

void DrawGuiImage(
	SDL_Renderer* renderer,
	SDL_Texture* texture,
	const SDL_Rect& target,
	GuiImageScaleMode mode = GuiImageScaleMode::Stretch
);

struct GuiColorBoxDrawCommand
{
    SDL_Rect rect{0, 0, 0, 0};
    SDL_Color color{0, 0, 0, SDL_ALPHA_OPAQUE};
};

void DrawGuiColorBoxes(
    SDL_Renderer* renderer,
    const std::vector<GuiColorBoxDrawCommand>& commands
);

void DrawGuiButton(
	SDL_Renderer* renderer,
	const SDL_Rect& rect,
	const GuiButtonVisual& visual,
	bool pressed,
	bool enabled = true
);

void DrawGuiButtons(
	SDL_Renderer* renderer,
	const std::vector<GuiButtonDrawCommand>& commands
);

enum class GuiInputAction
{
	Move,
	Press,
	Release
};

enum class GuiButtonEvent
{
	None,
	HoverEnter,
	HoverLeave,
	Pressed,
	Clicked
};

struct GuiButtonInputState
{
	bool hovered = false;
	bool pressed = false;
};

GuiButtonEvent ProcessGuiButtonInput(
	const SDL_Rect& rect,
	GuiButtonInputState& state,
	GuiInputAction action,
	int mouseX,
	int mouseY
);

struct GuiScrollbarVisual
{
	SDL_Texture* track = nullptr;
	SDL_Texture* thumb = nullptr;
	SDL_Color fallbackTrack{35, 75, 78, SDL_ALPHA_OPAQUE};
	SDL_Color fallbackThumb{100, 205, 200, SDL_ALPHA_OPAQUE};
	int minimumThumbHeight = 24;
};

struct GuiListWidgetDrawData
{
	SDL_Rect viewport{0, 0, 0, 0};
	std::vector<GuiButtonDrawCommand> itemCommands;
	struct ItemImage
	{
		SDL_Texture* texture = nullptr;
		SDL_Rect rect{0, 0, 0, 0};
		GuiImageScaleMode scaleMode = GuiImageScaleMode::Stretch;
	};
	std::vector<ItemImage> itemImages;
	int contentHeight = 0;
	int scrollOffset = 0;
	SDL_Rect scrollbarRect{0, 0, 0, 0};
	GuiScrollbarVisual scrollbarVisual;
	SDL_Texture* labels = nullptr;
	SDL_Rect labelsSource{0, 0, 0, 0};
	SDL_Rect labelsDestination{0, 0, 0, 0};
};

using GuiTextureResolver = std::function<
	SDL_Texture*(std::string_view)
>;

struct GuiListWidgetResources
{
	GuiTextureResolver textureResolver;
	SDL_Texture* labels = nullptr;
	std::function<void(
		std::size_t,
		const SDL_Rect&,
		std::vector<GuiListWidgetDrawData::ItemImage>&
	)> appendItemImages;
};

struct GuiProgressVisual
{
    SDL_Texture* background = nullptr;
    SDL_Texture* fill = nullptr;
    SDL_Color fallbackBackground{35, 45, 48, SDL_ALPHA_OPAQUE};
    SDL_Color fallbackFill{90, 190, 100, SDL_ALPHA_OPAQUE};
    bool horizontal = true;
    bool fillFromEnd = false;
    bool drawBackground = true;
};

struct GuiProgressDrawCommand
{
	SDL_Rect rect{0, 0, 0, 0};
	float value = 0.0f;
	GuiProgressVisual visual;
};

void DrawGuiProgressBar(
    SDL_Renderer* renderer,
    const SDL_Rect& rect,
    float value,
    const GuiProgressVisual& visual
);

void DrawGuiProgressBars(
	SDL_Renderer* renderer,
	const std::vector<GuiProgressDrawCommand>& commands
);

SDL_Rect CalculateGuiScrollbarThumbRect(
	const SDL_Rect& trackRect,
	int contentHeight,
	int viewportHeight,
	int scrollOffset,
	int minimumThumbHeight
);

void DrawGuiScrollbar(
	SDL_Renderer* renderer,
	const SDL_Rect& trackRect,
	int contentHeight,
	int viewportHeight,
	int scrollOffset,
    const GuiScrollbarVisual& visual
);

void DrawGuiList(
	SDL_Renderer* renderer,
	const SDL_Rect& viewport,
	const std::vector<GuiButtonDrawCommand>& itemCommands,
	int contentHeight,
	int scrollOffset,
	const SDL_Rect& scrollbarRect,
	const GuiScrollbarVisual& scrollbarVisual
);

void DrawGuiListWidget(
	SDL_Renderer* renderer,
	const GuiListWidgetDrawData& data
);

void DrawGuiListWidget(
	SDL_Renderer* renderer,
	const GuiListRuntimeLayout& layout,
	const GuiListWidgetResources& resources
);

struct GuiListLayout
{
	SDL_Rect viewport{0, 0, 0, 0};
	int columns = 1;
	int itemWidth = 0;
	int itemHeight = 0;
	int columnGap = 0;
	int rowGap = 0;
};

int GetGuiListRowStep(const GuiListLayout& layout);

int GetGuiListContentHeight(
	const GuiListLayout& layout,
	size_t itemCount
);

int GetGuiListMaximumScroll(
	const GuiListLayout& layout,
	size_t itemCount
);

SDL_Rect GetGuiListItemRect(
	const GuiListLayout& layout,
	size_t itemIndex,
	int scrollOffset
);

int HitTestGuiListItem(
	const GuiListLayout& layout,
	size_t itemCount,
	int scrollOffset,
	int mouseX,
	int mouseY
);

int ClampGuiListScroll(
	const GuiListLayout& layout,
	size_t itemCount,
	int scrollOffset
);
