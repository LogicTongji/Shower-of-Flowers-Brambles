#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_list_model.h"

namespace gui
{

enum class ValueKind
{
	Scalar,
	Block
};

struct GuiObject;

struct GuiValue
{
	ValueKind kind = ValueKind::Scalar;
	std::string scalar;
	std::shared_ptr<GuiObject> block;
};

struct GuiField
{
	std::string name;
	GuiValue value;
};

struct GuiObject
{
	std::vector<GuiField> fields;
};

struct GuiDocument
{
	std::filesystem::path path;
	GuiObject root;
};

struct SpriteResource
{
	std::string name;
	std::filesystem::path textureFile;
	std::string effectFile;
	std::string loadType;
	int frameCount = 1;
	bool noRefCount = false;
};

struct ProgressBarResource
{
	std::string name;
	std::string textureFile1;
	std::string textureFile2;
	std::string effectFile;
	float color[3] = {1.0f, 1.0f, 1.0f};
	float secondColor[3] = {1.0f, 1.0f, 1.0f};
	int width = 0;
	int height = 0;
	bool horizontal = true;
};

struct GuiRgbaColor
{
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
};

struct IndexedMapColorStop
{
	float minimum = 0.0f;
	GuiRgbaColor color;
};

struct IndexedMapSourceItem
{
	uint16_t id = 0;
	std::string name;
};

struct IndexedMapResource
{
	std::string name;
	std::filesystem::path textureFile;
	std::filesystem::path indexFile;
	std::filesystem::path sourceDefinitionFile;
	std::filesystem::path sourceProvinceFile;
	std::filesystem::path sourceGroupFile;
	std::vector<IndexedMapSourceItem> sourceItems;
	GuiRgbaColor sourceFillColor{0.855f, 0.855f, 0.824f, 1.0f};
	GuiRgbaColor sourceBoundaryColor{0.235f, 0.235f, 0.235f, 1.0f};
	GuiRgbaColor boundaryColor{0.02f, 0.06f, 0.07f, 0.92f};
	GuiRgbaColor hoverColor{0.90f, 0.94f, 1.0f, 0.47f};
	std::vector<IndexedMapColorStop> colorStops;
	int cropPadding = 16;
	int boundaryWidth = 1;
	bool flipVertical = true;
	bool drawBoundaries = true;
};

enum class WidgetType
{
	Window,
	Image,
	Text,
	Button,
	ListBox,
	ProgressBar,
	ScrollBar,
	ColorBox,
	IndexedMap,
	MarkerLayer,
	Custom,
	Unknown
};

struct GuiRect
{
	int x = 0;
	int y = 0;
	int width = 0;
    int height = 0;
};

struct GuiActionBinding
{
    std::string onClick;
    std::string onPress;
    std::string onRelease;
    std::string onHoverEnter;
    std::string onHoverLeave;
    std::string onDragStart;
    std::string onDrag;
    std::string onDragEnd;
};

struct GuiNineSliceInsets
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    bool Enabled() const
    {
        return left > 0
            || top > 0
            || right > 0
            || bottom > 0;
    }
};

struct WidgetDefinition
{
	WidgetType type = WidgetType::Unknown;
	std::string name;
	std::string parent;
	std::string spriteName;
	std::string spriteSource;
	std::string spriteValuePrefix;
	std::string frameSpriteName;
	std::string pressedSpriteName;
	std::string pressedSpriteSource;
	std::string text;
	std::string textSource;
	std::string localizationKey;
	std::string font;
	std::string customType;
	std::string templateName;
	std::string scrollBarName;
	std::string disabledByListName;
	std::string disabledMatchField;
	std::string disabledFilterField;
	std::string disabledFilterValueSource;
    std::string sliderName;
	std::string trackName;
	std::string progressResourceName;
	std::string indexedMapResourceName;
	std::string dataSource;
	std::string mapWidgetName;
	std::string portraitSource;
	std::string regionSource;
	std::string markerXSource;
	std::string markerYSource;
	std::string descriptionSource;
	std::string nameSource;
	int progressColorIndex = 0;
	std::string orientation;
	std::string layoutMode;
    std::string positionType;
    std::string scaleMode;
    GuiNineSliceInsets nineSlice;
    std::string alignment;
    std::string renderMode;
    std::string valueSource;
    std::string tooltip;
	std::string tooltipPlacement;
	std::string markerActionSpriteName;
	std::string markerActionName;
	std::string markerActionLocalizationKey;
	std::string markerStackSource;
	std::string markerStackOrderSource;
	std::string markerStackDirection;
	std::string dragAxis;
	std::string dragTrackName;
	std::string dragValueSource;
    std::string visibleWhen;
    std::string enabledWhen;
    GuiActionBinding actions;
    GuiRect rect;
	GuiRect markerRect;
	GuiRect portraitRect;
	GuiRect tooltipRect;
	GuiRect markerActionRect;
	GuiRgbaColor lineColor{0.20f, 0.65f, 1.0f, 0.90f};
	GuiRgbaColor tooltipColor{0.0f, 0.0f, 0.0f, 0.75f};
    int spacing = 0;
    int columnSpacing = 0;
	int polarCenterX = -1;
	int polarCenterY = -1;
	int polarRingCount = 0;
	int polarInnerRadius = 0;
	int polarOuterRadius = 0;
	int polarRingSpacing = 0;
	std::vector<int> polarRingItemCounts;
    int zOrder = 0;
    int frameZOrder = 1000000;
    int fontSize = 0;
	int lineSpacing = 0;
	int lineWidth = 1;
	int tooltipPadding = 10;
	int markerActionFontSize = 0;
	int markerStackSpacing = 0;
	int dragSteps = 0;
	float opacity = 1.0f;
		// 控件自身透明度。
	// 0.0 = 完全透明
	// 1.0 = 完全不透明
	//
	// 注意：
	// 这里存储的是 definition 自身的 opacity，
	// 真正渲染时使用的是 GuiResolvedWidget::opacity。
	float value = 0.0f;
	float polarStartAngle = 180.0f;
	float polarEndAngle = 360.0f;
	float dragMinimum = 0.0f;
	float dragMaximum = 1.0f;
	float dragStep = 0.0f;
	float textColor[3] = {1.0f, 1.0f, 1.0f};
	bool fillFromEnd = false;
	bool drawBackground = true;
    bool visible = true;
    bool enabled = true;
    bool localized = false;
    bool wrap = false;
    bool draggable = false;
    bool localizeTooltip = false;
	bool avoidTooltipOverlap = false;
	bool dragInverted = false;
    bool clipChildren = false;
    bool moveable = false;
    int dragHeight = 0;
    std::vector<WidgetDefinition> children;
};

struct WindowDefinition : WidgetDefinition
{
};

struct GuiLayoutContext
{
    std::function<bool(std::string_view)> conditionEvaluator;
    std::function<std::string(std::string_view)> textResolver;
    std::function<const ::GuiListModel*(std::string_view)> listResolver;
    std::function<double(std::string_view)> valueResolver;
    std::function<std::string(std::string_view)> localizationResolver;
};

struct GuiResolvedWidget
{
    const WidgetDefinition* definition = nullptr;
    GuiRect rect;
    bool visible = true;
    bool enabled = true;
	// 最终有效透明度。
	//
	// root:
	//     definition.opacity
	//
	// child:
	//     parent.opacity * definition.opacity
	float opacity = 1.0f;
    int depth = 0;
	int zOrder = 0;
	std::size_t order = 0;
	GuiRect clipRect;
	bool hasClipRect = false;
	std::string listName;
	int listIndex = -1;
	uint64_t listItemId = 0;
};

struct GuiListItemLayout
{
    const WidgetDefinition* definition = nullptr;
    std::size_t index = 0;
    GuiRect rect;
    bool visible = true;
    bool enabled = true;
    int zOrder = 0;
};

struct GuiListBinding
{
    bool valid = false;
    std::string listName;
    std::string templateName;
    std::string scrollbarName;
    std::string sliderName;
    std::string trackName;
	std::string disabledByListName;
	std::string disabledMatchField;
	std::string disabledFilterField;
    std::string disabledFilterValueSource;
	std::string layoutMode;
    GuiRect viewport;
    GuiRect scrollbar;
    GuiRect item;
    int spacing = 0;
    int columnSpacing = 0;
};

enum class GuiTextAlignment
{
    Left,
    Center,
    Right
};

struct GuiTextCommand
{
    const WidgetDefinition* definition = nullptr;
    GuiRect rect;
    std::string text;
    std::string font;
    GuiTextAlignment alignment = GuiTextAlignment::Left;
    int fontSize = 0;
    float color[3] = {1.0f, 1.0f, 1.0f};
    int zOrder = 0;
    int lineSpacing = 0;
    bool wrap = false;
};

class GuiInterpreter
{
public:
	bool LoadDirectory(
		const std::filesystem::path& root,
		std::string& error
	);

	bool LoadFile(
		const std::filesystem::path& path,
		std::string& error
	);

	const SpriteResource* FindSprite(
		const std::string& name
	) const;

	const ProgressBarResource* FindProgressBar(
		const std::string& name
	) const;

	const IndexedMapResource* FindIndexedMap(
		const std::string& name
	) const;

	std::filesystem::path ResolveTexture(
		const std::string& resourceName,
		const std::filesystem::path& projectRoot
	) const;

	std::filesystem::path ResolveIndexedMapTexture(
		const std::string& resourceName,
		const std::filesystem::path& projectRoot
	) const;

	std::filesystem::path ResolveIndexedMapIndex(
		const std::string& resourceName,
		const std::filesystem::path& projectRoot
	) const;

	const std::vector<GuiDocument>& Documents() const
	{
		return documents_;
	}

	const std::vector<std::string>& LoadDiagnostics() const
	{
		return loadDiagnostics_;
	}

	const std::unordered_map<std::string, SpriteResource>& Sprites() const
	{
		return sprites_;
	}

	const std::unordered_map<std::string, ProgressBarResource>& ProgressBars() const
	{
		return progressBars_;
	}

	const std::unordered_map<std::string, IndexedMapResource>& IndexedMaps() const
	{
		return indexedMaps_;
	}

	const std::vector<WindowDefinition>& Windows() const
	{
		return windows_;
	}

	const WindowDefinition* FindWindow(
		const std::string& name
	) const;

	std::vector<GuiResolvedWidget> ResolveWindowLayout(
		const std::string& name,
		const GuiLayoutContext& context = {}
	) const;

	std::vector<GuiListItemLayout> InstantiateListItems(
		const std::string& windowName,
		const std::string& listName,
		std::size_t itemCount,
		const GuiLayoutContext& context = {}
	) const;

	std::vector<GuiResolvedWidget> InstantiateListWidgets(
		const std::string& windowName,
		const std::string& listName,
		std::size_t itemCount,
		int scrollOffset = 0,
		const GuiLayoutContext& context = {}
	) const;

	bool ResolveListBinding(
		const std::string& windowName,
		const std::string& listName,
		GuiListBinding& output,
		const GuiLayoutContext& context = {}
	) const;

	std::vector<GuiTextCommand> BuildTextCommands(
		const std::string& windowName,
		const GuiLayoutContext& context = {}
	) const;

	std::vector<GuiTextCommand> BuildListTextCommands(
		const std::string& windowName,
		const std::string& listName,
		const std::vector<std::string>& texts,
		const GuiLayoutContext& context = {}
	) const;

	std::vector<GuiTextCommand> BuildListTextCommands(
		const std::string& windowName,
		const std::string& listName,
		const GuiLayoutContext& context
	) const;

private:
	void RegisterResources(const GuiObject& object);
	void RegisterLayouts(const GuiObject& object);

	std::vector<GuiDocument> documents_;
	std::vector<std::string> loadDiagnostics_;
	std::unordered_map<std::string, SpriteResource> sprites_;
	std::unordered_map<std::string, ProgressBarResource> progressBars_;
	std::unordered_map<std::string, IndexedMapResource> indexedMaps_;
	std::vector<WindowDefinition> windows_;
};

const GuiResolvedWidget* HitTestGuiWidgets(
	const std::vector<GuiResolvedWidget>& widgets,
		int mouseX,
		int mouseY
);

int HitTestGuiListItems(
	const std::vector<GuiListItemLayout>& items,
	const GuiRect& viewport,
	int scrollOffset,
	int mouseX,
	int mouseY
);

}
