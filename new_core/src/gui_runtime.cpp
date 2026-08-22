#include "gui_runtime.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "gui_data.h"

namespace
{

std::string WidgetKey(const GuiResolvedWidget* widget)
{
    if (!widget || !widget->definition)
    {
        return {};
    }

    if (!widget->listName.empty())
    {
        return widget->listName
            + "#"
            + std::to_string(widget->listIndex)
            + "#"
            + widget->definition->name;
    }

    return widget->definition->name;
}

std::string Trim(std::string value)
{
    const auto notSpace = [](unsigned char character)
    {
        return !std::isspace(character);
    };
    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), notSpace)
    );
    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            notSpace
        ).base(),
        value.end()
    );
    return value;
}

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

bool IsTruthy(const std::string& value)
{
    const std::string normalized = Lower(Trim(value));
    return normalized == "yes"
        || normalized == "true"
        || normalized == "1"
        || normalized == "on";
}

std::vector<std::string> SplitExpression(
    const std::string& expression,
    const std::string& separator
)
{
    std::vector<std::string> parts;
    size_t start = 0;
    size_t position = expression.find(separator);
    while (position != std::string::npos)
    {
        parts.push_back(expression.substr(start, position - start));
        start = position + separator.size();
        position = expression.find(separator, start);
    }
    parts.push_back(expression.substr(start));
    return parts;
}

std::string ReplaceItemId(
	std::string value,
	uint64_t itemId
)
{
	constexpr std::string_view placeholder = "{id}";
	const std::string replacement = std::to_string(itemId);
	std::size_t position = value.find(placeholder);
	while (position != std::string::npos)
	{
		value.replace(position, placeholder.size(), replacement);
		position = value.find(
			placeholder,
			position + replacement.size()
		);
	}
	return value;
}

}

void GuiConditionEnvironment::Set(
    std::string name,
    std::string value
)
{
    values_[Lower(Trim(std::move(name)))] = Trim(std::move(value));
}

void GuiConditionEnvironment::SetBool(
    std::string name,
    bool value
)
{
    Set(std::move(name), value ? "true" : "false");
}

bool GuiConditionEnvironment::Evaluate(
    std::string_view expression
) const
{
    std::string value = Trim(std::string(expression));
    if (value.empty())
    {
        return true;
    }

    const std::vector<std::string> orParts = SplitExpression(value, "||");
    if (orParts.size() > 1)
    {
        for (const std::string& part : orParts)
        {
            if (Evaluate(part))
            {
                return true;
            }
        }
        return false;
    }

    const std::vector<std::string> andParts = SplitExpression(value, "&&");
    if (andParts.size() > 1)
    {
        for (const std::string& part : andParts)
        {
            if (!Evaluate(part))
            {
                return false;
            }
        }
        return true;
    }

    if (value.front() == '!')
    {
        return !Evaluate(value.substr(1));
    }

    const size_t equalPosition = value.find("==");
    const size_t notEqualPosition = value.find("!=");
    const size_t operatorPosition = equalPosition != std::string::npos
        ? equalPosition
        : notEqualPosition;
    if (operatorPosition != std::string::npos)
    {
        const bool notEqual = notEqualPosition != std::string::npos
            && notEqualPosition == operatorPosition;
        const std::string left = Lower(Trim(
            value.substr(0, operatorPosition)
        ));
        std::string right = Trim(value.substr(
            operatorPosition + 2
        ));
        if (right.size() >= 2
            && ((right.front() == '"' && right.back() == '"')
                || (right.front() == '\'' && right.back() == '\'')))
        {
            right = right.substr(1, right.size() - 2);
        }

        const auto iterator = values_.find(left);
        const std::string actual = iterator == values_.end()
            ? left
            : Lower(Trim(iterator->second));
        const bool equal = actual == Lower(right);
        return notEqual ? !equal : equal;
    }

    std::string key = Lower(Trim(value));
    if (key.size() >= 2
        && key.front() == '('
        && key.back() == ')')
    {
        return Evaluate(key.substr(1, key.size() - 2));
    }

    const auto iterator = values_.find(key);
    if (iterator != values_.end())
    {
        return IsTruthy(iterator->second);
    }

    return IsTruthy(key);
}

GuiListRuntimeState& GuiListRuntimeStore::Get(
    std::string_view listName
)
{
    return states_[std::string(listName)];
}

const GuiListRuntimeState* GuiListRuntimeStore::Find(
    std::string_view listName
) const
{
    const auto iterator = states_.find(std::string(listName));
    return iterator == states_.end()
        ? nullptr
        : &iterator->second;
}

void GuiListRuntimeStore::ScrollBy(
    std::string_view listName,
    int delta,
    int maximumScroll
)
{
    GuiListRuntimeState& state = Get(listName);
    state.scrollOffset = std::clamp(
        state.scrollOffset + delta,
        0,
        std::max(0, maximumScroll)
    );
}

void GuiListRuntimeStore::Clear()
{
    states_.clear();
}

namespace
{

bool ContainsCustomType(
    const gui::WidgetDefinition& widget,
    std::string_view customType
)
{
    if (widget.type == gui::WidgetType::Custom
        && (widget.customType == customType
            || (widget.customType.empty()
                && widget.name == customType)))
    {
        return true;
    }

    for (const gui::WidgetDefinition& child : widget.children)
    {
        if (ContainsCustomType(child, customType))
        {
            return true;
        }
    }

    return false;
}

std::string FindFirstWidgetNameRecursive(
    const gui::WidgetDefinition& widget,
    gui::WidgetType type
)
{
    if (widget.type == type && !widget.name.empty())
    {
        return widget.name;
    }

    for (const gui::WidgetDefinition& child : widget.children)
    {
        const std::string name = FindFirstWidgetNameRecursive(
            child,
            type
        );
        if (!name.empty())
        {
            return name;
        }
    }

    return {};
}

}

bool GuiWindowRuntime::Bind(
    const gui::GuiInterpreter& interpreter,
    std::string_view windowName
)
{
    if (!interpreter.FindWindow(std::string(windowName)))
    {
        interpreter_ = nullptr;
        windowName_.clear();
        return false;
    }

    interpreter_ = &interpreter;
    windowName_ = windowName;
    return true;
}

bool GuiWindowRuntime::BindFirstWindowWithCustomType(
    const gui::GuiInterpreter& interpreter,
    std::string_view customType
)
{
    for (const gui::WindowDefinition& window : interpreter.Windows())
    {
        if (ContainsCustomType(window, customType))
        {
            return Bind(interpreter, window.name);
        }
    }

    interpreter_ = nullptr;
    windowName_.clear();
    return false;
}

const gui::WindowDefinition* GuiWindowRuntime::Definition() const
{
    return IsBound()
        ? interpreter_->FindWindow(windowName_)
        : nullptr;
}

std::string GuiWindowRuntime::FindFirstWidgetName(
    gui::WidgetType type
) const
{
    const gui::WindowDefinition* window = Definition();
    return window
        ? FindFirstWidgetNameRecursive(*window, type)
        : std::string{};
}

std::vector<gui::GuiResolvedWidget> GuiWindowRuntime::ResolveLayout(
    const gui::GuiLayoutContext& context
) const
{
    return IsBound()
        ? interpreter_->ResolveWindowLayout(windowName_, context)
        : std::vector<gui::GuiResolvedWidget>{};
}

std::vector<gui::GuiResolvedWidget>
GuiWindowRuntime::InstantiateListWidgets(
    std::string_view listName,
    std::size_t itemCount,
    int scrollOffset,
    const gui::GuiLayoutContext& context
) const
{
    return IsBound()
        ? interpreter_->InstantiateListWidgets(
            windowName_,
            std::string(listName),
            itemCount,
            scrollOffset,
            context
        )
        : std::vector<gui::GuiResolvedWidget>{};
}

std::vector<gui::GuiTextCommand> GuiWindowRuntime::BuildTextCommands(
    const gui::GuiLayoutContext& context
) const
{
    return IsBound()
        ? interpreter_->BuildTextCommands(windowName_, context)
        : std::vector<gui::GuiTextCommand>{};
}

std::vector<gui::GuiTextCommand>
GuiWindowRuntime::BuildListTextCommands(
    std::string_view listName,
    const gui::GuiLayoutContext& context
) const
{
    return IsBound()
        ? interpreter_->BuildListTextCommands(
            windowName_,
            std::string(listName),
            context
        )
        : std::vector<gui::GuiTextCommand>{};
}

bool GuiWindowRuntime::ResolveListBinding(
    std::string_view listName,
    gui::GuiListBinding& output,
    const gui::GuiLayoutContext& context
) const
{
    return IsBound()
        && interpreter_->ResolveListBinding(
            windowName_,
            std::string(listName),
            output,
            context
        );
}

GuiListRuntimeLayout GuiWindowRuntime::BuildListRuntimeLayout(
    std::string_view listName,
    const GuiListModel& model,
    const GuiListRuntimeState& runtime,
    const GuiRuntimeInputState& inputState,
    const gui::GuiLayoutContext& context
) const
{
    return IsBound()
        ? BuildGuiListRuntimeLayout(
            *interpreter_,
            windowName_,
            listName,
            model,
            runtime,
            inputState,
            context
        )
        : GuiListRuntimeLayout{};
}

GuiListRuntimeLayout BuildGuiListRuntimeLayout(
    const gui::GuiInterpreter& interpreter,
    std::string_view windowName,
    std::string_view listName,
    const GuiListModel& model,
    const GuiListRuntimeState& runtime,
    const GuiRuntimeInputState& inputState,
    const gui::GuiLayoutContext& context
)
{
    GuiListRuntimeLayout output;
    const std::string window(windowName);
    const std::string list(listName);
    gui::GuiListBinding binding;
    if (!interpreter.ResolveListBinding(
        window,
        list,
        binding,
        context
    ))
    {
        return output;
    }

    output.viewport = binding.viewport;
    output.scrollbar = binding.scrollbar;
    output.scrollbarTrackSprite = binding.trackName;
    output.scrollbarThumbSprite = binding.sliderName;
    output.minimumScrollbarThumbSize = binding.minimumThumbSize;

    const int itemWidth = std::max(0, binding.item.width);
    const int itemHeight = std::max(0, binding.item.height);
    const int columnGap = std::max(0, binding.columnSpacing);
    const int rowGap = std::max(0, binding.spacing);
    const int columnStep = itemWidth + columnGap;
    const int rowStep = std::max(1, itemHeight + rowGap);
    const int columns = std::max(
        1,
        columnStep > 0
            ? (binding.viewport.width + columnGap) / columnStep
            : 1
    );
	std::vector<std::size_t> visibleItemIndices;
	visibleItemIndices.reserve(model.items.size());
	const std::string itemFilterValue =
		!binding.itemFilterValueSource.empty() && context.textResolver
			? context.textResolver(binding.itemFilterValueSource)
			: std::string{};
	for (std::size_t index = 0; index < model.items.size(); ++index)
	{
		const GuiListItem& item = model.items[index];
		bool visible = true;
		if (const GuiDataValue* value = item.Find("visible"))
		{
			visible = GuiDataValueToBool(*value);
		}
		if (visible)
		{
			if (const GuiDataValue* condition = item.Find("visiblewhen"))
			{
				visible = !context.conditionEvaluator
					|| context.conditionEvaluator(
						GuiDataValueToText(*condition)
					);
			}
		}
		if (visible
			&& !binding.itemFilterField.empty()
			&& !binding.itemFilterValueSource.empty())
		{
			const GuiDataValue* field = item.Find(binding.itemFilterField);
			visible = field
				&& GuiDataValueToText(*field) == itemFilterValue;
		}
		if (visible)
		{
			visibleItemIndices.push_back(index);
		}
	}

    const int rowCount = visibleItemIndices.empty()
        ? 0
        : static_cast<int>(
			(visibleItemIndices.size()
				+ static_cast<size_t>(columns) - 1)
                / static_cast<size_t>(columns)
        );

    output.rowStep = rowStep;
	const std::string layoutMode = Lower(binding.layoutMode);
	const bool freeLayout = layoutMode == "polar"
		|| layoutMode == "radial"
		|| layoutMode == "semicircle";
    output.contentHeight = freeLayout
		? binding.viewport.height
		: rowCount > 0
			? rowCount * rowStep - rowGap
			: 0;
    output.maximumScroll = std::max(
        0,
        output.contentHeight - binding.viewport.height
    );
    output.scrollOffset = std::clamp(
        runtime.scrollOffset,
        0,
        output.maximumScroll
    );

	std::unordered_set<uint64_t> disabledItemIds;
	const GuiListModel* disabledItems = nullptr;
	if (!binding.disabledByListName.empty() && context.listResolver)
	{
		disabledItems = context.listResolver(binding.disabledByListName);
		if (disabledItems)
		{
			for (const GuiListItem& item : disabledItems->items)
			{
				disabledItemIds.insert(item.id);
			}
		}
	}

    const std::vector<gui::GuiListItemLayout> items =
        interpreter.InstantiateListItems(
            window,
            list,
			visibleItemIndices.size(),
            context
        );
    output.items.reserve(items.size());
    for (const gui::GuiListItemLayout& item : items)
    {
		if (!item.definition || item.index >= visibleItemIndices.size())
        {
            continue;
        }

		const std::size_t modelIndex = visibleItemIndices[item.index];
		const GuiListItem& modelItem = model.items[modelIndex];
		bool enabled = item.enabled;
		if (const GuiDataValue* value = modelItem.Find("enabled"))
		{
			enabled = enabled && GuiDataValueToBool(*value);
		}
		if (const GuiDataValue* condition = modelItem.Find("enabledwhen"))
		{
			enabled = enabled
				&& (!context.conditionEvaluator
					|| context.conditionEvaluator(
						GuiDataValueToText(*condition)
					));
		}
		enabled = enabled
			&& disabledItemIds.find(modelItem.id)
				== disabledItemIds.end();
		if (enabled
			&& disabledItems
			&& !binding.disabledMatchField.empty()
			&& !binding.disabledFilterField.empty()
			&& !binding.disabledFilterValueSource.empty())
		{
			const GuiDataValue* itemMatch = modelItem.Find(
				binding.disabledMatchField
			);
			const std::string filterValue = context.textResolver
				? context.textResolver(
					binding.disabledFilterValueSource
				)
				: std::string{};
			if (itemMatch && !filterValue.empty())
			{
				const std::string matchValue = GuiDataValueToText(
					*itemMatch
				);
				for (const GuiListItem& disabled : disabledItems->items)
				{
					const GuiDataValue* disabledMatch = disabled.Find(
						binding.disabledMatchField
					);
					const GuiDataValue* disabledFilter = disabled.Find(
						binding.disabledFilterField
					);
					if (disabledMatch
						&& disabledFilter
						&& GuiDataValueToText(*disabledMatch)
							== matchValue
						&& GuiDataValueToText(*disabledFilter)
							== filterValue)
					{
						enabled = false;
						break;
					}
				}
			}
		}

		auto resolveItemSprite = [&modelItem, &context](
			const std::string& source,
			const std::string& fallback,
			const std::string& valuePrefix
		)
		{
			constexpr std::string_view prefix = "item.";
			if (source.rfind(prefix, 0) == 0)
			{
				const GuiDataValue* value = modelItem.Find(
					source.substr(prefix.size())
				);
				return value
					? GuiDataValueToText(*value)
					: std::string{};
			}
			if (!source.empty())
			{
				std::string value = ReplaceItemId(source, modelItem.id);
				if (context.textResolver)
				{
					const std::string resolved = context.textResolver(value);
					if (!resolved.empty())
					{
						value = resolved;
					}
				}
				return valuePrefix.empty()
					? value
					: valuePrefix + value;
			}
			return fallback;
		};
		const std::string normalSprite = resolveItemSprite(
			item.definition->spriteSource,
			item.definition->spriteName,
			item.definition->spriteValuePrefix
		);
		std::string pressedSprite = resolveItemSprite(
			item.definition->pressedSpriteSource,
			item.definition->pressedSpriteName,
			item.definition->spriteValuePrefix
		);
		if (pressedSprite.empty())
		{
			pressedSprite = normalSprite;
		}

        output.items.push_back({
            item.definition,
			modelIndex,
            modelItem.id,
            item.rect,
            item.visible,
            enabled,
            modelItem.id == runtime.selectedItemId
                || (inputState.pressed != nullptr
                    && inputState.pressedSnapshot.listName == list
                    && inputState.pressedSnapshot.listIndex
						== static_cast<int>(modelIndex)),
			item.zOrder,
			normalSprite,
			pressedSprite
        });
    }

    return output;
}

namespace
{

void AddAction(
    std::vector<GuiActionEvent>& output,
    const GuiResolvedWidget* widget,
    GuiActionPhase phase,
    const std::string& action
)
{
    if (widget && !action.empty())
    {
        output.push_back({widget, phase, action});
    }
}

std::string FormatNumber(double value)
{
	std::ostringstream stream;
	stream << std::fixed << std::setprecision(6) << value;
	std::string output = stream.str();
	while (!output.empty() && output.back() == '0')
	{
		output.pop_back();
	}
	if (!output.empty() && output.back() == '.')
	{
		output.pop_back();
	}
	return output.empty() ? "0" : output;
}

void AttachDragParameters(
	GuiActionEvent& event,
	const GuiResolvedWidget& widget,
	const std::vector<GuiResolvedWidget>& widgets,
	const GuiRuntimeInputState& state,
	int mouseX,
	int mouseY
)
{
	const gui::WidgetDefinition& definition = *widget.definition;
	gui::GuiRect track = widget.rect;
	if (!definition.dragTrackName.empty())
	{
		for (const GuiResolvedWidget& candidate : widgets)
		{
			if (candidate.definition
				&& candidate.definition->name == definition.dragTrackName)
			{
				track = candidate.rect;
				break;
			}
		}
	}
	const std::string axis = Lower(definition.dragAxis);
	const bool vertical = axis == "vertical" || axis == "y";
	const double mousePosition = vertical
		? mouseY - state.dragGrabOffsetY
		: mouseX - state.dragGrabOffsetX;
	const double trackStart = vertical ? track.y : track.x;
	const double trackLength = std::max(
		1,
		vertical
			? track.height - widget.rect.height
			: track.width - widget.rect.width
	);
	double normalized = std::clamp(
		(mousePosition - trackStart) / trackLength,
		0.0,
		1.0
	);
	if (definition.dragInverted)
	{
		normalized = 1.0 - normalized;
	}
	double minimum = definition.dragMinimum;
	double maximum = definition.dragMaximum;
	if (maximum < minimum)
	{
		std::swap(minimum, maximum);
	}
	double value = minimum + normalized * (maximum - minimum);
	if (definition.dragStep > 0.0f)
	{
		value = minimum + std::round(
			(value - minimum) / definition.dragStep
		) * definition.dragStep;
		value = std::clamp(value, minimum, maximum);
		normalized = maximum > minimum
			? (value - minimum) / (maximum - minimum)
			: 0.0;
	}

	event.parameters["normalized"] = FormatNumber(normalized);
	event.parameters["value"] = FormatNumber(value);
	event.parameters["dragx"] = std::to_string(mouseX);
	event.parameters["dragy"] = std::to_string(mouseY);
	event.parameters["deltax"] = std::to_string(
		mouseX - state.dragStartMouseX
	);
	event.parameters["deltay"] = std::to_string(
		mouseY - state.dragStartMouseY
	);
	event.parameters["stepdeltax"] = std::to_string(
		mouseX - state.dragLastMouseX
	);
	event.parameters["stepdeltay"] = std::to_string(
		mouseY - state.dragLastMouseY
	);
	if (!definition.dragValueSource.empty())
	{
		event.parameters["target"] = definition.dragValueSource;
	}
	if (definition.dragSteps > 0)
	{
		const int stepIndex = std::clamp(
			static_cast<int>(std::lround(
				normalized * (definition.dragSteps - 1)
			)) + 1,
			1,
			definition.dragSteps
		);
		event.parameters["stepindex"] = std::to_string(stepIndex);
	}
}

}

std::vector<GuiActionEvent> GuiEventRouter::ProcessMove(
    const std::vector<GuiResolvedWidget>& widgets,
    GuiRuntimeInputState& state,
    int mouseX,
    int mouseY
) const
{
    std::vector<GuiActionEvent> output;
    const GuiResolvedWidget* target = gui::HitTestGuiHoverWidgets(
        widgets,
        mouseX,
        mouseY
    );

    const std::string targetKey = WidgetKey(target);
    if (targetKey == state.hoveredKey)
    {
        return output;
    }

    if (!state.hoveredKey.empty())
    {
        AddAction(
            output,
            &state.hoveredSnapshot,
            GuiActionPhase::HoverLeave,
            state.hoveredSnapshot.definition
                ? state.hoveredSnapshot.definition
                    ->actions.onHoverLeave
                : std::string{}
        );
    }

    if (target && target->definition)
    {
        AddAction(
            output,
            target,
            GuiActionPhase::HoverEnter,
            target->definition->actions.onHoverEnter
        );
    }

    state.hovered = target;
    state.hoveredKey = targetKey;
    if (target)
    {
        state.hoveredSnapshot = *target;
    }
    else
    {
        state.hoveredSnapshot = {};
    }
    return output;
}

std::vector<GuiActionEvent> GuiEventRouter::ProcessDragMove(
	const std::vector<GuiResolvedWidget>& widgets,
	GuiRuntimeInputState& state,
	int mouseX,
	int mouseY
) const
{
	std::vector<GuiActionEvent> output;
	if (state.pressedKey.empty()
		|| !state.pressedSnapshot.definition
		|| !state.pressedSnapshot.definition->draggable)
	{
		return output;
	}

	if (!state.dragging)
	{
		state.dragging = true;
		GuiActionEvent start{
			&state.pressedSnapshot,
			GuiActionPhase::DragStart,
			state.pressedSnapshot.definition->actions.onDragStart
		};
		AttachDragParameters(
			start,
			state.pressedSnapshot,
			widgets,
			state,
			mouseX,
			mouseY
		);
		if (!start.action.empty())
		{
			output.push_back(std::move(start));
		}
	}

	GuiActionEvent drag{
		&state.pressedSnapshot,
		GuiActionPhase::Drag,
		state.pressedSnapshot.definition->actions.onDrag
	};
	AttachDragParameters(
		drag,
		state.pressedSnapshot,
		widgets,
		state,
		mouseX,
		mouseY
	);
	if (!drag.action.empty())
	{
		output.push_back(std::move(drag));
	}
	state.dragLastMouseX = mouseX;
	state.dragLastMouseY = mouseY;
	return output;
}

std::vector<GuiActionEvent> GuiEventRouter::ProcessPress(
    const std::vector<GuiResolvedWidget>& widgets,
    GuiRuntimeInputState& state,
    int mouseX,
    int mouseY
) const
{
    std::vector<GuiActionEvent> output;
    state.pressed = gui::HitTestGuiWidgets(
        widgets,
        mouseX,
        mouseY
    );
    state.pressedKey = WidgetKey(state.pressed);
    if (state.pressed)
    {
        state.pressedSnapshot = *state.pressed;
		state.dragStartMouseX = mouseX;
		state.dragStartMouseY = mouseY;
		state.dragLastMouseX = mouseX;
		state.dragLastMouseY = mouseY;
		state.dragGrabOffsetX = mouseX - state.pressed->rect.x;
		state.dragGrabOffsetY = mouseY - state.pressed->rect.y;
		state.dragging = false;
    }
    else
    {
        state.pressedSnapshot = {};
    }

    if (state.pressed && state.pressed->definition)
    {
        AddAction(
            output,
            state.pressed,
            GuiActionPhase::Press,
            state.pressed->definition->actions.onPress
        );
		if (state.pressed->definition->draggable)
		{
			GuiActionEvent drag{
				state.pressed,
				GuiActionPhase::Drag,
				state.pressed->definition->actions.onDrag
			};
			AttachDragParameters(
				drag,
				*state.pressed,
				widgets,
				state,
				mouseX,
				mouseY
			);
			if (!drag.action.empty())
			{
				output.push_back(std::move(drag));
			}
		}
    }

    return output;
}

std::vector<GuiActionEvent> GuiEventRouter::ProcessRelease(
    const std::vector<GuiResolvedWidget>& widgets,
    GuiRuntimeInputState& state,
    int mouseX,
    int mouseY
) const
{
    std::vector<GuiActionEvent> output;
    const GuiResolvedWidget* target = gui::HitTestGuiWidgets(
        widgets,
        mouseX,
        mouseY
    );
    const GuiResolvedWidget* pressed = state.pressedKey.empty()
        ? nullptr
        : &state.pressedSnapshot;
    const std::string targetKey = WidgetKey(target);

    if (pressed && pressed->definition)
    {
		if (state.dragging && pressed->definition->draggable)
		{
			GuiActionEvent dragEnd{
				pressed,
				GuiActionPhase::DragEnd,
				pressed->definition->actions.onDragEnd
			};
			AttachDragParameters(
				dragEnd,
				*pressed,
				widgets,
				state,
				mouseX,
				mouseY
			);
			if (!dragEnd.action.empty())
			{
				output.push_back(std::move(dragEnd));
			}
		}
        AddAction(
            output,
            pressed,
            GuiActionPhase::Release,
            pressed->definition->actions.onRelease
        );

        if (!state.dragging && state.pressedKey == targetKey)
        {
            AddAction(
                output,
                pressed,
                GuiActionPhase::Click,
                pressed->definition->actions.onClick
            );
        }
    }

    state.pressed = nullptr;
    state.pressedKey.clear();
	state.dragging = false;
    return output;
}

int GetClickedListIndex(
    const std::vector<GuiActionEvent>& events,
    std::string_view listName
)
{
    for (const GuiActionEvent& event : events)
    {
        if (event.phase != GuiActionPhase::Click
            || !event.widget
            || event.widget->listName != listName)
        {
            continue;
        }

        return event.widget->listIndex;
    }

    return -1;
}
