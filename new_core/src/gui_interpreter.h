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
	int line = 0;
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
	std::string frameLayout = "horizontal";
	std::string animationMode = "loop";
	int frameCount = 1;
	int animationStartFrame = 1;
	int animationEndFrame = 0;
	int animationFrameTimeMilliseconds = 0;
	int animationOffsetMilliseconds = 0;
	bool autoAnimate = false;
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

struct PositionResource
{
	std::string name;
	int x = 0;
	int y = 0;
};

struct GuiRgbaColor
{
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
};

struct GuiEffectResource
{
	std::string name;
	std::string effect = "tint";
	GuiRgbaColor color;
	float minimum = 0.5f;
	float maximum = 1.0f;
	float speed = 1.0f;
	float phaseDegrees = 0.0f;
	bool enabled = true;
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
	GuiRgbaColor sourceFillColor;
	GuiRgbaColor sourceBoundaryColor;
	GuiRgbaColor boundaryColor;
	GuiRgbaColor hoverColor{1.0f, 1.0f, 1.0f, 0.0f};
	std::vector<IndexedMapColorStop> colorStops;
	int cropPadding = 0;
	int boundaryWidth = 0;
	bool flipVertical = false;
	bool drawBoundaries = false;
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

struct GuiTransform2D
{
	float rotationDegrees = 0.0f;
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	float pivotX = 0.5f;
	float pivotY = 0.5f;
	bool flipX = false;
	bool flipY = false;
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
	std::string frameSource;
	std::string rotationSource;
	std::string transformScaleSource;
	std::string transformScaleXSource;
	std::string transformScaleYSource;
	std::string animationMode;
	std::string animationTimeSource;
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
	std::string itemFilterField;
	std::string itemFilterValueSource;
    std::string sliderName;
	std::string trackName;
	std::string progressResourceName;
	std::string indexedMapResourceName;
	std::string effectResourceName;
	std::string effectSource;
	std::string effectTimeSource;
	std::string dataSource;
	std::string catalogSource;
	std::string mapWidgetName;
	std::string portraitSource;
	std::string regionSource;
	std::string markerXSource;
	std::string markerYSource;
	std::string descriptionSource;
	std::string nameSource;
	int progressColorIndex = 0;
	int frame = 1;
	int animationStartFrame = 0;
	int animationEndFrame = 0;
	int animationFrameTimeMilliseconds = 0;
	int animationOffsetMilliseconds = 0;
	std::string orientation;
	std::string layoutMode;
    std::string positionType;
    std::string scaleMode;
    GuiNineSliceInsets nineSlice;
	GuiNineSliceInsets tooltipNineSlice;
    std::string alignment;
    std::string renderMode;
    std::string valueSource;
    std::string tooltip;
	std::string tooltipSource;
	std::string tooltipLocalizationKey;
	std::string tooltipSpriteName;
	std::string tooltipFont;
	std::string tooltipScaleMode;
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
	GuiRgbaColor lineColor;
	GuiRgbaColor tooltipColor;
	GuiTransform2D transform;
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
    int frameZOrder = 0;
    int fontSize = 0;
	int lineSpacing = 0;
	int lineWidth = 0;
	int tooltipPadding = 0;
	int tooltipSearchStep = 0;
	int tooltipFontSize = 0;
	int tooltipLineSpacing = 0;
	int tooltipDelayMilliseconds = 0;
	int minimumThumbSize = 0;
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
	float disabledBrightness = 1.0f;
	float disabledOpacity = 1.0f;
	float textColor[3] = {1.0f, 1.0f, 1.0f};
	float tooltipTextColor[3] = {1.0f, 1.0f, 1.0f};
	bool fillFromEnd = false;
	bool drawBackground = true;
	bool animate = false;
	bool animateSpecified = false;
    bool visible = true;
    bool enabled = true;
    bool localized = false;
    bool wrap = false;
    bool draggable = false;
    bool localizeTooltip = false;
	bool tooltipWrap = true;
	bool avoidTooltipOverlap = false;
	bool dragInverted = false;
    bool clipChildren = false;
    bool moveable = false;
	bool fullScreen = false;
	bool fullScreenSpecified = false;
    int dragHeight = 0;
	std::vector<WidgetDefinition> children;
};

int ResolveSpriteFrameIndex(
	const SpriteResource& resource,
	const WidgetDefinition& widget,
	uint64_t animationTimeMilliseconds,
	int sourcedFrame = 0,
	bool hasSourcedFrame = false
);

GuiRect ResolveSpriteFrameSourceRect(
	const SpriteResource& resource,
	int textureWidth,
	int textureHeight,
	int frame
);

struct StaticDataValueDefinition
{
	std::string name;
	GuiDataValue value;
};

struct StaticDataListDefinition
{
	std::string name;
	GuiListModel model;
};

struct WindowDefinition : WidgetDefinition
{
	std::vector<StaticDataValueDefinition> staticValues;
	std::vector<StaticDataListDefinition> staticLists;
};

struct GuiLayoutContext
{
    std::function<bool(std::string_view)> conditionEvaluator;
    std::function<std::string(std::string_view)> textResolver;
    std::function<const ::GuiListModel*(std::string_view)> listResolver;
    std::function<double(std::string_view)> valueResolver;
    std::function<std::string(std::string_view)> localizationResolver;
	GuiRect rootClientRect;
	bool hasRootClientRect = false;
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
	GuiTransform2D transform;
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
	std::string itemFilterField;
	std::string itemFilterValueSource;
	std::string layoutMode;
    GuiRect viewport;
    GuiRect scrollbar;
    GuiRect item;
    int spacing = 0;
    int columnSpacing = 0;
	int minimumThumbSize = 0;
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
	void SetStrictLegacyFiles(bool enabled)
	{
		strictLegacyFiles_ = enabled;
	}

	bool StrictLegacyFiles() const
	{
		return strictLegacyFiles_;
	}

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

	const PositionResource* FindPosition(
		const std::string& name
	) const;

	std::filesystem::path ResolveTexture(
		const std::string& resourceName,
		const std::filesystem::path& projectRoot
	) const;

	std::filesystem::path ResolveAssetPath(
		const std::filesystem::path& asset,
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

	const std::unordered_map<std::string, GuiEffectResource>& Effects() const
	{
		return effects_;
	}

	const std::unordered_map<std::string, PositionResource>& Positions() const
	{
		return positions_;
	}

	const std::vector<WindowDefinition>& Windows() const
	{
		return windows_;
	}

	const WindowDefinition* FindWindow(
		const std::string& name
	) const;

	const GuiEffectResource* FindEffect(
		const std::string& name
	) const;

	GuiRect ResolveRootRect(
		const std::string& name,
		const GuiLayoutContext& context = {}
	) const;

	GuiRect ResolveChildRect(
		const GuiRect& parent,
		const WidgetDefinition& child
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
	void RegisterLayouts(
		const GuiObject& object,
		const std::filesystem::path& path
	);

	std::vector<GuiDocument> documents_;
	std::vector<std::string> loadDiagnostics_;
	std::unordered_map<std::string, SpriteResource> sprites_;
	std::unordered_map<std::string, ProgressBarResource> progressBars_;
	std::unordered_map<std::string, IndexedMapResource> indexedMaps_;
	std::unordered_map<std::string, GuiEffectResource> effects_;
	std::unordered_map<std::string, PositionResource> positions_;
	std::vector<WindowDefinition> windows_;
	bool strictLegacyFiles_ = false;
};

GuiRgbaColor SampleGuiEffect(
	const GuiEffectResource& resource,
	uint64_t elapsedMilliseconds
);

const GuiResolvedWidget* HitTestGuiWidgets(
	const std::vector<GuiResolvedWidget>& widgets,
		int mouseX,
		int mouseY
);

const GuiResolvedWidget* HitTestGuiHoverWidgets(
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
