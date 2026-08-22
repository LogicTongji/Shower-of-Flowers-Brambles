#include "gui_interpreter.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "gui_behavior.h"
#include "gui_data.h"
#include "gui_runtime.h"

namespace
{

std::filesystem::path ResolveFixtureRoot(int argc, char** argv)
{
    const std::filesystem::path input = argc >= 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    const std::filesystem::path projectFixture =
        input / "new_core" / "tests" / "fixtures" / "gui_interpreter";
    return std::filesystem::is_directory(projectFixture)
        ? projectFixture
        : input;
}

int CountWidgets(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    gui::WidgetType type,
    bool visible
)
{
    return static_cast<int>(std::count_if(
        widgets.begin(),
        widgets.end(),
        [type, visible](const gui::GuiResolvedWidget& widget)
        {
            return widget.definition
                && widget.definition->type == type
                && widget.visible == visible;
        }
    ));
}

const gui::WidgetDefinition* FindWidgetDefinition(
    const gui::WidgetDefinition& root,
    std::string_view name
)
{
    if (root.name == name)
    {
        return &root;
    }
    for (const gui::WidgetDefinition& child : root.children)
    {
        if (const gui::WidgetDefinition* found =
                FindWidgetDefinition(child, name))
        {
            return found;
        }
    }
    return nullptr;
}

}

int main(int argc, char** argv)
{
    const std::filesystem::path fixtureRoot =
        ResolveFixtureRoot(argc, argv);

    gui::GuiInterpreter interpreter;
    std::string error;
    if (!interpreter.LoadDirectory(fixtureRoot, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    const gui::SpriteResource* sprite =
        interpreter.FindSprite("GFX_probe_panel");
	const gui::SpriteResource* animatedSprite =
		interpreter.FindSprite("GFX_probe_animated");
    const gui::ProgressBarResource* progress =
        interpreter.FindProgressBar("probe_progress_resource");
    const gui::IndexedMapResource* indexedMap =
        interpreter.FindIndexedMap("GFX_probe_indexed_map");
	const gui::GuiEffectResource* pulseEffect =
		interpreter.FindEffect("GFX_probe_pulse");
	const gui::PositionResource* diagnosticPosition =
		interpreter.FindPosition("diagnostic_anchor");
    const gui::WindowDefinition* window =
        interpreter.FindWindow("probe_window");
	const gui::WindowDefinition* diagnosticWindow =
		interpreter.FindWindow("diagnostic_window");
	const gui::WidgetDefinition* diagnosticAnchorChild = diagnosticWindow
		? FindWidgetDefinition(*diagnosticWindow, "diagnostic_anchor_child")
		: nullptr;
	const gui::WidgetDefinition* animatedIcon = window
		? FindWidgetDefinition(*window, "probe_animated_icon")
		: nullptr;
    if (!sprite || !animatedSprite || !progress || !indexedMap
		|| !pulseEffect
		|| !diagnosticPosition || !window
		|| !diagnosticWindow
		|| !animatedIcon
		|| animatedSprite->frameCount != 6
		|| animatedSprite->frameLayout != "horizontal"
		|| !animatedSprite->autoAnimate
		|| animatedSprite->animationMode != "pingpong"
		|| animatedSprite->animationFrameTimeMilliseconds != 100
		|| animatedSprite->animationStartFrame != 2
		|| animatedSprite->animationEndFrame != 5
		|| animatedIcon->frame != 3
		|| animatedIcon->frameSource != "state.sprite_frame"
		|| !animatedIcon->animateSpecified
		|| !animatedIcon->animate
		|| animatedIcon->animationMode != "once"
		|| animatedIcon->animationFrameTimeMilliseconds != 50
		|| animatedIcon->animationStartFrame != 1
		|| animatedIcon->animationEndFrame != 4
		|| animatedIcon->animationOffsetMilliseconds != 25
		|| animatedIcon->animationTimeSource != "state.animation_time"
		|| animatedIcon->effectResourceName != "GFX_probe_pulse"
		|| animatedIcon->effectTimeSource != "state.effect_time"
		|| animatedIcon->transform.rotationDegrees != 15.0f
		|| animatedIcon->rotationSource != "state.rotation"
		|| animatedIcon->transform.pivotX != 0.25f
		|| animatedIcon->transform.pivotY != 0.75f
		|| animatedIcon->transform.scaleX != 1.5f
		|| animatedIcon->transform.scaleY != 0.5f
		|| animatedIcon->transformScaleXSource != "state.scale_x"
		|| animatedIcon->transformScaleYSource != "state.scale_y"
		|| !animatedIcon->transform.flipX
		|| pulseEffect->effect != "brightness_pulse"
		|| pulseEffect->minimum != 0.25f
		|| pulseEffect->maximum != 0.75f
		|| pulseEffect->phaseDegrees != -90.0f
		|| progress->textureFile1 != "probe\\progress_green.png"
		|| progress->textureFile2 != "probe\\progress_red.png"
		|| diagnosticPosition->x != 12
		|| diagnosticPosition->y != 24
		|| !diagnosticAnchorChild
		|| diagnosticAnchorChild->positionType != "diagnostic_anchor"
		|| !diagnosticWindow->fullScreenSpecified
		|| !diagnosticWindow->fullScreen
		|| diagnosticWindow->orientation != "UPPER_LEFT"
		|| window->frameZOrder != 250
		|| window->staticValues.size() != 1
		|| window->staticValues[0].name != "probe.static.title"
		|| window->staticLists.size() != 1
		|| window->staticLists[0].name != "probe_static_list"
		|| window->staticLists[0].model.revision != 7
		|| window->staticLists[0].model.items.size() != 1
		|| window->staticLists[0].model.items[0].id != 41)
    {
		std::cerr << "Interpreter resource registration failed"
			<< ": animatedSprite=" << (animatedSprite != nullptr)
			<< ", animatedIcon=" << (animatedIcon != nullptr)
			<< ", frames=" << (animatedSprite
				? animatedSprite->frameCount : -1)
			<< ", layout=" << (animatedSprite
				? animatedSprite->frameLayout : std::string{})
			<< ", auto=" << (animatedSprite
				? animatedSprite->autoAnimate : false)
			<< ", resourceMode=" << (animatedSprite
				? animatedSprite->animationMode : std::string{})
			<< ", resourceTime=" << (animatedSprite
				? animatedSprite->animationFrameTimeMilliseconds : -1)
			<< ", widgetFrame=" << (animatedIcon
				? animatedIcon->frame : -1)
			<< ", widgetMode=" << (animatedIcon
				? animatedIcon->animationMode : std::string{})
			<< ", progress1=" << (progress
				? progress->textureFile1 : std::string{})
			<< '\n';
        return 1;
    }

	gui::WidgetDefinition inheritedAnimation;
	if (gui::ResolveSpriteFrameIndex(
			*animatedSprite,
			inheritedAnimation,
			0
		) != 2
		|| gui::ResolveSpriteFrameIndex(
			*animatedSprite,
			inheritedAnimation,
			300
		) != 5
		|| gui::ResolveSpriteFrameIndex(
			*animatedSprite,
			inheritedAnimation,
			400
		) != 4
		|| gui::ResolveSpriteFrameIndex(
			*animatedSprite,
			*animatedIcon,
			500
		) != 4
		|| gui::ResolveSpriteFrameIndex(
			*animatedSprite,
			*animatedIcon,
			0,
			6,
			true
		) != 6)
	{
		std::cerr << "Sprite frame animation resolution failed\n";
		return 1;
	}
	const gui::GuiRgbaColor sampledEffect = gui::SampleGuiEffect(
		*pulseEffect,
		0
	);
	if (std::abs(sampledEffect.r - 0.2f) > 0.0001f
		|| std::abs(sampledEffect.g - 0.15f) > 0.0001f
		|| std::abs(sampledEffect.b - 0.1f) > 0.0001f
		|| std::abs(sampledEffect.a - 0.5f) > 0.0001f)
	{
		std::cerr << "Built-in effect sampling failed\n";
		return 1;
	}
	const gui::GuiRect horizontalFrame =
		gui::ResolveSpriteFrameSourceRect(
			*animatedSprite,
			600,
			20,
			3
		);
	gui::SpriteResource verticalSprite = *animatedSprite;
	verticalSprite.frameLayout = "vertical";
	const gui::GuiRect verticalFrame =
		gui::ResolveSpriteFrameSourceRect(
			verticalSprite,
			20,
			600,
			4
		);
	if (horizontalFrame.x != 200
		|| horizontalFrame.y != 0
		|| horizontalFrame.width != 100
		|| horizontalFrame.height != 20
		|| verticalFrame.x != 0
		|| verticalFrame.y != 300
		|| verticalFrame.width != 20
		|| verticalFrame.height != 100)
	{
		std::cerr << "Sprite frame source rectangle failed\n";
		return 1;
	}

    const gui::WidgetDefinition* panel = FindWidgetDefinition(
        *window,
        "probe_panel"
    );
    const gui::WidgetDefinition* scrollbar = FindWidgetDefinition(
        *window,
        "probe_scrollbar"
    );
    const gui::WidgetDefinition* listItem = FindWidgetDefinition(
        *window,
        "probe_list_item"
    );
	const gui::WidgetDefinition* listDefinition = FindWidgetDefinition(
		*window,
		"probe_list"
	);
    const bool scaleDiagnostic = std::any_of(
        interpreter.LoadDiagnostics().begin(),
        interpreter.LoadDiagnostics().end(),
        [](const std::string& diagnostic)
        {
            return diagnostic.find("probe_invalid_scale")
                    != std::string::npos
                && diagnostic.find("unsupported scaleMode")
                    != std::string::npos;
        }
    );
	const bool layoutFieldDiagnostic = std::any_of(
		interpreter.LoadDiagnostics().begin(),
		interpreter.LoadDiagnostics().end(),
		[](const std::string& diagnostic)
		{
			return diagnostic.find("strict_diagnostics.sgui")
					!= std::string::npos
				&& diagnostic.find("unknown field 'visiblWhen'")
					!= std::string::npos
				&& diagnostic.find("did you mean 'visiblewhen'")
					!= std::string::npos;
		}
	);
	const bool resourceFieldDiagnostic = std::any_of(
		interpreter.LoadDiagnostics().begin(),
		interpreter.LoadDiagnostics().end(),
		[](const std::string& diagnostic)
		{
			return diagnostic.find("strict_diagnostics.sgfx")
					!= std::string::npos
				&& diagnostic.find("unknown field 'textureFil'")
					!= std::string::npos
				&& diagnostic.find("did you mean 'texturefile'")
					!= std::string::npos;
		}
	);
	const bool compatibilityFieldMisdiagnosed = std::any_of(
		interpreter.LoadDiagnostics().begin(),
		interpreter.LoadDiagnostics().end(),
		[](const std::string& diagnostic)
		{
			return diagnostic.find("strict_diagnostics.sgui")
					!= std::string::npos
				&& (diagnostic.find("unknown field 'fullScreen'")
						!= std::string::npos
					|| diagnostic.find("unknown field 'orientation'")
						!= std::string::npos
					|| diagnostic.find("unknown field 'positionType'")
						!= std::string::npos);
		}
	);
	const auto hasStrictDiagnostic = [&interpreter](
		std::string_view first,
		std::string_view second)
	{
		return std::any_of(
			interpreter.LoadDiagnostics().begin(),
			interpreter.LoadDiagnostics().end(),
			[first, second](const std::string& diagnostic)
			{
				return diagnostic.find(first) != std::string::npos
					&& diagnostic.find(second) != std::string::npos;
			}
		);
	};
	const bool rangeDiagnostic =
		hasStrictDiagnostic("opacity", "outside")
		&& hasStrictDiagnostic("noOfFrames", "outside");
	const bool typeDiagnostic =
		hasStrictDiagnostic("norefcount", "requires boolean");
	const bool animationSchemaDiagnostic =
		hasStrictDiagnostic("frameLayout", "unsupported")
		&& hasStrictDiagnostic("mode", "unsupported")
		&& hasStrictDiagnostic("fps", "outside")
		&& hasStrictDiagnostic("frameTim", "unknown field");
	const bool effectSchemaDiagnostic =
		hasStrictDiagnostic("external_shader", "unsupported")
		&& hasStrictDiagnostic("effectFil", "unknown field")
		&& hasStrictDiagnostic("speed", "outside")
		&& hasStrictDiagnostic("GFX_invalid_effect", "minimum must not");
	const bool requiredDiagnostic =
		hasStrictDiagnostic("diagnostic_list", "itemTemplate");
	const bool mutualDiagnostic =
		hasStrictDiagnostic("diagnostic_text", "mutually exclusive");
	const bool applicabilityDiagnostic =
		hasStrictDiagnostic("diagnostic_anchor_child", "not applicable");
	const bool legacyDiagnosedByDefault = std::any_of(
		interpreter.LoadDiagnostics().begin(),
		interpreter.LoadDiagnostics().end(),
		[](const std::string& diagnostic)
		{
			return diagnostic.find("legacyTypo") != std::string::npos;
		}
	);
    if (!panel
        || panel->scaleMode != "contain"
        || panel->opacity != 0.5f
        || panel->nineSlice.left != 4
        || panel->nineSlice.top != 5
        || panel->nineSlice.right != 6
        || panel->nineSlice.bottom != 7
		|| panel->frameZOrder != -25
		|| panel->lineWidth != 4
		|| panel->tooltipPadding != 9
		|| panel->tooltipSearchStep != 7
		|| panel->lineColor.r != 0.20f
		|| panel->tooltipColor.a != 0.75f
        || !scrollbar
        || !scrollbar->nineSlice.Enabled()
		|| scrollbar->minimumThumbSize != 13
        || !listItem
        || !listItem->nineSlice.Enabled()
		|| listItem->disabledBrightness != 0.45f
		|| listItem->disabledOpacity != 0.55f
		|| listItem->tooltipSource != "item.region"
		|| listItem->tooltipRect.x != 8
		|| listItem->tooltipRect.y != 10
		|| listItem->tooltipRect.width != 144
		|| listItem->tooltipRect.height != 48
		|| listItem->tooltipSpriteName != "GFX_probe_panel"
		|| listItem->tooltipFont != "probe_font"
		|| listItem->tooltipFontSize != 13
		|| listItem->tooltipLineSpacing != 2
		|| listItem->tooltipDelayMilliseconds != 25
		|| listItem->tooltipScaleMode != "stretch"
		|| listItem->tooltipPlacement != "cursor"
		|| !listItem->tooltipNineSlice.Enabled()
		|| listItem->tooltipTextColor[0] != 0.90f
		|| !listItem->tooltipWrap
		|| !listDefinition
		|| listDefinition->itemFilterField != "tag"
		|| listDefinition->itemFilterValueSource != "probe.filter"
		|| !scaleDiagnostic
		|| !layoutFieldDiagnostic
		|| !resourceFieldDiagnostic
		|| compatibilityFieldMisdiagnosed
		|| !rangeDiagnostic
		|| !typeDiagnostic
		|| !animationSchemaDiagnostic
		|| !effectSchemaDiagnostic
		|| !requiredDiagnostic
		|| !mutualDiagnostic
		|| !applicabilityDiagnostic
		|| legacyDiagnosedByDefault)
    {
        std::cerr << "Image transform parsing or validation failed\n";
        return 1;
    }

	gui::GuiLayoutContext fullScreenContext;
	fullScreenContext.rootClientRect = {0, 0, 640, 360};
	fullScreenContext.hasRootClientRect = true;
	const std::vector<gui::GuiResolvedWidget> diagnosticLayout =
		interpreter.ResolveWindowLayout(
			"diagnostic_window",
			fullScreenContext
		);
	const auto diagnosticRoot = std::find_if(
		diagnosticLayout.begin(),
		diagnosticLayout.end(),
		[diagnosticWindow](const gui::GuiResolvedWidget& widget)
		{
			return widget.definition == diagnosticWindow;
		}
	);
	const auto anchoredChild = std::find_if(
		diagnosticLayout.begin(),
		diagnosticLayout.end(),
		[](const gui::GuiResolvedWidget& widget)
		{
			return widget.definition
				&& widget.definition->name == "diagnostic_anchor_child";
		}
	);
	if (diagnosticRoot == diagnosticLayout.end()
		|| diagnosticRoot->rect.x != 0
		|| diagnosticRoot->rect.y != 0
		|| diagnosticRoot->rect.width != 640
		|| diagnosticRoot->rect.height != 360
		|| anchoredChild == diagnosticLayout.end()
		|| anchoredChild->rect.x != 615
		|| anchoredChild->rect.y != 28
		|| anchoredChild->rect.width != 10
		|| anchoredChild->rect.height != 12)
	{
		std::cerr << "Full-screen or anchored layout resolution failed\n";
		return 1;
	}

	gui::GuiInterpreter strictLegacyInterpreter;
	strictLegacyInterpreter.SetStrictLegacyFiles(true);
	std::string strictLegacyError;
	if (!strictLegacyInterpreter.LoadFile(
			fixtureRoot / "strict_legacy.gui",
			strictLegacyError
		)
		|| !std::any_of(
			strictLegacyInterpreter.LoadDiagnostics().begin(),
			strictLegacyInterpreter.LoadDiagnostics().end(),
			[](const std::string& diagnostic)
			{
				return diagnostic.find("legacyTypo") != std::string::npos;
			}
		))
	{
		std::cerr << "Optional strict legacy validation failed\n";
		return 1;
	}

    if (indexedMap->sourceDefinitionFile != "map\\definition.csv"
        || indexedMap->sourceProvinceFile != "map\\provinces.bmp"
        || indexedMap->sourceGroupFile != "map\\groups.txt"
        || indexedMap->sourceItems.size() != 2
        || indexedMap->sourceItems[0].id != 7
        || indexedMap->sourceItems[0].name != "first_group"
        || indexedMap->cropPadding != 8
		|| indexedMap->boundaryWidth != 3
		|| !indexedMap->drawBoundaries
		|| indexedMap->sourceFillColor.g != 0.50f
		|| indexedMap->sourceBoundaryColor.b != 0.30f
		|| indexedMap->boundaryColor.a != 0.70f
		|| indexedMap->hoverColor.r != 0.80f
        || indexedMap->flipVertical)
    {
        std::cerr << "Indexed-map build configuration failed\n";
        return 1;
    }

    GuiDataRegistry data;
    data.Set("state.visible", true);
    data.Set("title", "Probe Title");
    data.Set("progress", 0.5);
    GuiListModel list;
    list.items.push_back({11, "First"});
    list.items.push_back({22, "Second"});
    data.SetList("probe_list", list);
	data.Set("items.11.label", "Resolved 11");
	data.Set("items.22.label", "Resolved 22");
	data.Set("items.11.visible", true);
	data.Set("items.22.visible", true);
	GuiListModel polarList;
	for (uint64_t itemId = 1; itemId <= 6; ++itemId)
	{
		polarList.items.push_back({itemId, {}});
	}
	data.SetList("probe_polar_list", polarList);
	data.Set("probe.drag.value", 5.0);
    const gui::GuiLayoutContext context = data.MakeLayoutContext();

    const std::vector<gui::GuiResolvedWidget> visibleLayout =
        interpreter.ResolveWindowLayout("probe_window", context);
    if (CountWidgets(
            visibleLayout,
            gui::WidgetType::IndexedMap,
            true
        ) != 1
        || CountWidgets(
            visibleLayout,
            gui::WidgetType::ProgressBar,
            true
        ) != 1)
    {
        std::cerr << "Visible layout resolution failed\n";
        return 1;
    }

    const std::vector<gui::GuiTextCommand> textCommands =
        interpreter.BuildTextCommands("probe_window", context);
    const std::vector<gui::GuiTextCommand> listTextCommands =
        interpreter.BuildListTextCommands(
            "probe_window",
            "probe_list",
            context
        );
    if (textCommands.size() != 1
        || textCommands.front().text != "Probe Title"
		|| textCommands.front().color[0] != 0.10f
		|| textCommands.front().color[1] != 0.20f
		|| textCommands.front().color[2] != 0.30f
        || listTextCommands.size() != 2)
    {
        std::cerr << "Declarative text resolution failed\n";
        return 1;
    }

    gui::GuiListBinding binding;
    const std::vector<gui::GuiListItemLayout> listItems =
        interpreter.InstantiateListItems(
            "probe_window",
            "probe_list",
            3,
            context
        );
    if (listItems.size() != 3
        || !interpreter.ResolveListBinding(
            "probe_window",
            "probe_list",
            binding,
            context
        )
        || !binding.valid
        || binding.templateName != "probe_list_item"
        || binding.scrollbarName != "probe_scrollbar"
		|| binding.minimumThumbSize != 13)
    {
        std::cerr << "Declarative list binding failed\n";
        return 1;
    }

	const std::vector<gui::GuiListItemLayout> polarItems =
		interpreter.InstantiateListItems(
			"probe_window",
			"probe_polar_list",
			polarList.size(),
			context
		);
	if (polarItems.size() != polarList.size()
		|| polarItems.front().rect.x >= polarItems.back().rect.x
		|| polarItems.front().rect.y != polarItems.back().rect.y)
	{
		std::cerr << "Polar list layout failed\n";
		return 1;
	}

	const std::vector<gui::GuiResolvedWidget> dragLayout =
		interpreter.ResolveWindowLayout("probe_window", context);
	const auto dragWidget = std::find_if(
		dragLayout.begin(),
		dragLayout.end(),
		[](const gui::GuiResolvedWidget& widget)
		{
			return widget.definition
				&& widget.definition->name == "probe_drag_cursor";
		}
	);
	if (dragWidget == dragLayout.end()
		|| dragWidget->rect.x != 105)
	{
		std::cerr << "Drag-bound layout failed\n";
		return 1;
	}

	GuiRuntimeInputState dragInput;
	GuiEventRouter eventRouter;
	const std::vector<GuiActionEvent> pressEvents =
		eventRouter.ProcessPress(
			dragLayout,
			dragInput,
			dragWidget->rect.x + 5,
			dragWidget->rect.y + 5
		);
	const std::vector<GuiActionEvent> dragEvents =
		eventRouter.ProcessDragMove(
			dragLayout,
			dragInput,
			160,
			dragWidget->rect.y + 5
		);
	const std::vector<GuiActionEvent> releaseEvents =
		eventRouter.ProcessRelease(
			dragLayout,
			dragInput,
			160,
			dragWidget->rect.y + 5
		);
	if (pressEvents.size() != 2
		|| pressEvents[0].phase != GuiActionPhase::Press
		|| pressEvents[1].phase != GuiActionPhase::Drag
		|| dragEvents.size() != 2
		|| dragEvents[0].phase != GuiActionPhase::DragStart
		|| dragEvents[1].phase != GuiActionPhase::Drag
		|| dragEvents[1].parameters.at("stepindex") != "9"
		|| releaseEvents.size() != 1
		|| releaseEvents[0].phase != GuiActionPhase::DragEnd)
	{
		std::cerr << "Continuous drag routing failed\n";
		return 1;
	}

	gui::WidgetDefinition coveredButton;
	coveredButton.type = gui::WidgetType::Button;
	coveredButton.name = "covered_button";
	gui::WidgetDefinition markerLayer;
	markerLayer.type = gui::WidgetType::MarkerLayer;
	markerLayer.name = "marker_layer";
	markerLayer.draggable = true;
	const std::vector<gui::GuiResolvedWidget> markerLayout = {
		{&coveredButton, {10, 10, 80, 40}, true, true},
		{&markerLayer, {0, 0, 200, 200}, true, true}
	};
	const gui::GuiResolvedWidget* markerHit =
		gui::HitTestGuiWidgets(markerLayout, 20, 20);
	if (!markerHit || markerHit->definition != &coveredButton)
	{
		std::cerr << "Marker layer blocked generic input routing\n";
		return 1;
	}

    gui::WidgetDefinition transparentButton;
    transparentButton.type = gui::WidgetType::Button;
    transparentButton.name = "transparent_button";
    std::vector<gui::GuiResolvedWidget> transparentLayout = {
        {&coveredButton, {10, 10, 80, 40}, true, true},
        {&transparentButton, {10, 10, 80, 40}, true, true}
    };
    transparentLayout.back().opacity = 0.0f;
    const gui::GuiResolvedWidget* transparentHit =
        gui::HitTestGuiWidgets(transparentLayout, 20, 20);
    if (!transparentHit || transparentHit->definition != &coveredButton)
    {
        std::cerr << "Transparent widget blocked input routing\n";
        return 1;
    }

    gui::WidgetDefinition actionableText;
    actionableText.type = gui::WidgetType::Text;
    actionableText.name = "actionable_text";
    actionableText.actions.onClick = "probe_action";
    const std::vector<gui::GuiResolvedWidget> actionableLayout = {
        {&actionableText, {10, 10, 80, 40}, true, true}
    };
    const gui::GuiResolvedWidget* actionableHit =
        gui::HitTestGuiWidgets(actionableLayout, 20, 20);
    if (!actionableHit
        || actionableHit->definition != &actionableText)
    {
        std::cerr << "Actionable passive widget missed input routing\n";
        return 1;
    }

	gui::WidgetDefinition structuralWindow;
	structuralWindow.type = gui::WidgetType::Window;
	structuralWindow.name = "structural_window";
	const std::vector<gui::GuiResolvedWidget> structuralLayout = {
		{&structuralWindow, {0, 0, 200, 200}, true, true}
	};
	if (gui::HitTestGuiWidgets(structuralLayout, 20, 20))
	{
		std::cerr << "Structural root window blocked input routing\n";
		return 1;
	}
	structuralWindow.moveable = true;
	structuralWindow.dragHeight = 32;
	if (gui::HitTestGuiWidgets(structuralLayout, 20, 20)
		!= &structuralLayout.front())
	{
		std::cerr << "Moveable window missed input routing\n";
		return 1;
	}

	gui::WidgetDefinition tooltipOnlyText;
	tooltipOnlyText.type = gui::WidgetType::Text;
	tooltipOnlyText.name = "tooltip_only_text";
	tooltipOnlyText.tooltip = "Passive tooltip";
	const std::vector<gui::GuiResolvedWidget> tooltipOnlyLayout = {
		{&tooltipOnlyText, {10, 10, 80, 40}, true, true}
	};
	if (gui::HitTestGuiWidgets(tooltipOnlyLayout, 20, 20)
		|| gui::HitTestGuiHoverWidgets(tooltipOnlyLayout, 20, 20)
			!= &tooltipOnlyLayout.front())
	{
		std::cerr << "Tooltip-only widget input separation failed\n";
		return 1;
	}

	gui::WidgetDefinition transformedButton;
	transformedButton.type = gui::WidgetType::Button;
	transformedButton.name = "transformed_button";
	gui::GuiResolvedWidget transformedWidget;
	transformedWidget.definition = &transformedButton;
	transformedWidget.rect = {40, 40, 100, 20};
	transformedWidget.visible = true;
	transformedWidget.enabled = true;
	transformedWidget.transform.rotationDegrees = 90.0f;
	const std::vector<gui::GuiResolvedWidget> transformedLayout = {
		transformedWidget
	};
	if (gui::HitTestGuiWidgets(transformedLayout, 90, 10)
		!= &transformedLayout.front()
		|| gui::HitTestGuiWidgets(transformedLayout, 50, 50))
	{
		std::cerr << "Transformed widget hit testing failed\n";
		return 1;
	}

    GuiWindowRuntime runtime;
    if (!runtime.Bind(interpreter, "probe_window")
        || runtime.FindFirstWidgetName(gui::WidgetType::ListBox)
            != "probe_list")
    {
        std::cerr << "Window runtime binding failed\n";
        return 1;
    }

    GuiBehaviorRegistry behaviors;
    if (!behaviors.LoadDirectory(fixtureRoot, error)
        || !behaviors.Find("activate_item"))
    {
        std::cerr << "Behavior registration failed: " << error << '\n';
        return 1;
    }

    data.Set("state.visible", false);
    const std::vector<gui::GuiResolvedWidget> hiddenLayout =
        interpreter.ResolveWindowLayout(
            "probe_window",
            data.MakeLayoutContext()
        );
    if (CountWidgets(
            hiddenLayout,
            gui::WidgetType::IndexedMap,
            false
        ) != 1
        || CountWidgets(
            hiddenLayout,
            gui::WidgetType::ProgressBar,
            false
        ) != 1)
    {
        std::cerr << "Conditional visibility resolution failed\n";
        return 1;
    }

    std::cout
        << "Documents: " << interpreter.Documents().size() << '\n'
        << "Sprites: " << interpreter.Sprites().size() << '\n'
        << "Progress bars: " << interpreter.ProgressBars().size() << '\n'
        << "Indexed maps: " << interpreter.IndexedMaps().size() << '\n'
        << "Windows: " << interpreter.Windows().size() << '\n'
        << "List items: " << listItems.size() << '\n'
		<< "Polar items: " << polarItems.size() << '\n'
		<< "Continuous drag: passed\n"
		<< "Marker input pass-through: passed\n"
        << "Behaviors: " << behaviors.Size() << '\n';
    return 0;
}
