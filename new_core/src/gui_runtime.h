#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_interpreter.h"

using gui::GuiResolvedWidget;

class GuiConditionEnvironment
{
public:
    void Set(
        std::string name,
        std::string value
    );

    void SetBool(
        std::string name,
        bool value
    );

    bool Evaluate(
        std::string_view expression
    ) const;

private:
    std::unordered_map<std::string, std::string> values_;
};

enum class GuiActionPhase
{
    HoverEnter,
    HoverLeave,
    Press,
    Release,
    Click,
    DragStart,
    Drag,
    DragEnd
};

struct GuiActionEvent
{
    const GuiResolvedWidget* widget = nullptr;
    GuiActionPhase phase = GuiActionPhase::Click;
    std::string action;
    uint64_t itemId = 0;
    bool hasItemId = false;
    std::unordered_map<std::string, std::string> parameters;
    std::string sourceWidgetName;
    std::string sourceListName;
    int sourceListIndex = -1;
};

struct GuiRuntimeInputState
{
    const GuiResolvedWidget* hovered = nullptr;
    const GuiResolvedWidget* pressed = nullptr;
    GuiResolvedWidget hoveredSnapshot;
    GuiResolvedWidget pressedSnapshot;
    std::string hoveredKey;
    std::string pressedKey;
	bool dragging = false;
	int dragStartMouseX = 0;
	int dragStartMouseY = 0;
	int dragLastMouseX = 0;
	int dragLastMouseY = 0;
	int dragGrabOffsetX = 0;
	int dragGrabOffsetY = 0;
};

struct GuiListRuntimeState
{
    int scrollOffset = 0;
    uint64_t selectedItemId = 0;
};

struct GuiListItemRuntimeLayout
{
    const gui::WidgetDefinition* definition = nullptr;
    std::size_t itemIndex = 0;
    uint64_t itemId = 0;
    gui::GuiRect rect;
    bool visible = true;
    bool enabled = true;
    bool pressed = false;
	int zOrder = 0;
	std::string normalSpriteName;
	std::string pressedSpriteName;
};

struct GuiListRuntimeLayout
{
    gui::GuiRect viewport;
    std::vector<GuiListItemRuntimeLayout> items;
    gui::GuiRect scrollbar;
    std::string scrollbarTrackSprite;
    std::string scrollbarThumbSprite;
    int minimumScrollbarThumbSize = 0;
    int contentHeight = 0;
    int maximumScroll = 0;
    int rowStep = 1;
    int scrollOffset = 0;
};

GuiListRuntimeLayout BuildGuiListRuntimeLayout(
    const gui::GuiInterpreter& interpreter,
    std::string_view windowName,
    std::string_view listName,
    const GuiListModel& model,
    const GuiListRuntimeState& runtime,
    const GuiRuntimeInputState& inputState,
    const gui::GuiLayoutContext& context = {}
);

class GuiListRuntimeStore
{
public:
    GuiListRuntimeState& Get(
        std::string_view listName
    );

    const GuiListRuntimeState* Find(
        std::string_view listName
    ) const;

    void ScrollBy(
        std::string_view listName,
        int delta,
        int maximumScroll
    );

    void Clear();

private:
    std::unordered_map<
        std::string,
        GuiListRuntimeState
    > states_;
};

class GuiWindowRuntime
{
public:
    bool Bind(
        const gui::GuiInterpreter& interpreter,
        std::string_view windowName
    );

    bool BindFirstWindowWithCustomType(
        const gui::GuiInterpreter& interpreter,
        std::string_view customType
    );

    bool IsBound() const
    {
        return interpreter_ != nullptr && !windowName_.empty();
    }

    const std::string& Name() const
    {
        return windowName_;
    }

    const gui::GuiInterpreter& Interpreter() const
    {
        return *interpreter_;
    }

    const gui::WindowDefinition* Definition() const;

    std::string FindFirstWidgetName(
        gui::WidgetType type
    ) const;

    std::vector<gui::GuiResolvedWidget> ResolveLayout(
        const gui::GuiLayoutContext& context = {}
    ) const;

    std::vector<gui::GuiResolvedWidget> InstantiateListWidgets(
        std::string_view listName,
        std::size_t itemCount,
        int scrollOffset,
        const gui::GuiLayoutContext& context = {}
    ) const;

    std::vector<gui::GuiTextCommand> BuildTextCommands(
        const gui::GuiLayoutContext& context = {}
    ) const;

    std::vector<gui::GuiTextCommand> BuildListTextCommands(
        std::string_view listName,
        const gui::GuiLayoutContext& context = {}
    ) const;

    bool ResolveListBinding(
        std::string_view listName,
        gui::GuiListBinding& output,
        const gui::GuiLayoutContext& context = {}
    ) const;

    GuiListRuntimeLayout BuildListRuntimeLayout(
        std::string_view listName,
        const GuiListModel& model,
        const GuiListRuntimeState& runtime,
        const GuiRuntimeInputState& inputState,
        const gui::GuiLayoutContext& context = {}
    ) const;

private:
    const gui::GuiInterpreter* interpreter_ = nullptr;
    std::string windowName_;
};

class GuiEventRouter
{
public:
    std::vector<GuiActionEvent> ProcessMove(
        const std::vector<GuiResolvedWidget>& widgets,
        GuiRuntimeInputState& state,
        int mouseX,
        int mouseY
    ) const;

	std::vector<GuiActionEvent> ProcessDragMove(
		const std::vector<GuiResolvedWidget>& widgets,
		GuiRuntimeInputState& state,
		int mouseX,
		int mouseY
	) const;

    std::vector<GuiActionEvent> ProcessPress(
        const std::vector<GuiResolvedWidget>& widgets,
        GuiRuntimeInputState& state,
        int mouseX,
        int mouseY
    ) const;

    std::vector<GuiActionEvent> ProcessRelease(
        const std::vector<GuiResolvedWidget>& widgets,
        GuiRuntimeInputState& state,
        int mouseX,
        int mouseY
    ) const;
};

int GetClickedListIndex(
    const std::vector<GuiActionEvent>& events,
    std::string_view listName
);
