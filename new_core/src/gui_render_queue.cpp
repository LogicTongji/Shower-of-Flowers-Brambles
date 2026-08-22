#include "gui_render_queue.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>

namespace
{

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

bool ResolveCommandType(
    const gui::GuiResolvedWidget& widget,
    const std::unordered_set<const gui::WidgetDefinition*>&
        listTemplateDefinitions,
    GuiRenderCommandType& type
)
{
    if (!widget.definition || !widget.visible)
    {
        return false;
    }
    if (widget.listIndex < 0
        && listTemplateDefinitions.find(widget.definition)
            != listTemplateDefinitions.end())
    {
        return false;
    }

    switch (widget.definition->type)
    {
    case gui::WidgetType::Window:
        if (widget.definition->frameSpriteName.empty())
        {
            return false;
        }
        type = GuiRenderCommandType::WindowFrame;
        return true;
    case gui::WidgetType::Image:
        type = GuiRenderCommandType::Image;
        return true;
    case gui::WidgetType::Text:
        if (Lower(widget.definition->renderMode) == "custom")
        {
            return false;
        }
        type = GuiRenderCommandType::Text;
        return true;
    case gui::WidgetType::Button:
        type = GuiRenderCommandType::Button;
        return true;
    case gui::WidgetType::ListBox:
        return false;
    case gui::WidgetType::ProgressBar:
        type = GuiRenderCommandType::ProgressBar;
        return true;
    case gui::WidgetType::ColorBox:
        type = GuiRenderCommandType::ColorBox;
        return true;
    case gui::WidgetType::IndexedMap:
        type = GuiRenderCommandType::IndexedMap;
        return true;
    case gui::WidgetType::MarkerLayer:
        type = GuiRenderCommandType::Custom;
        return true;
    case gui::WidgetType::Custom:
        type = GuiRenderCommandType::Custom;
        return true;
    case gui::WidgetType::ScrollBar:
        type = GuiRenderCommandType::ScrollBar;
        return true;
    case gui::WidgetType::Unknown:
        return false;
    }
    return false;
}

bool HasTextDescendant(const gui::WidgetDefinition& definition)
{
    for (const gui::WidgetDefinition& child : definition.children)
    {
        if (child.type == gui::WidgetType::Text
            || HasTextDescendant(child))
        {
            return true;
        }
    }
    return false;
}

bool NeedsButtonText(
    const gui::GuiResolvedWidget& widget,
    const std::unordered_set<std::string>& listTemplateNames
)
{
    if (!widget.definition
        || widget.definition->type != gui::WidgetType::Button
        || HasTextDescendant(*widget.definition))
    {
        return false;
    }
    const gui::WidgetDefinition& definition = *widget.definition;
    const bool listTemplateRoot = widget.listIndex >= 0
        && listTemplateNames.find(definition.name)
            != listTemplateNames.end();
    return listTemplateRoot
        || !definition.font.empty()
        || !definition.text.empty()
        || !definition.textSource.empty()
        || !definition.localizationKey.empty();
}

}

std::vector<GuiRenderCommand> BuildGuiRenderQueue(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const std::unordered_set<std::string>& listTemplateNames
)
{
    std::vector<GuiRenderCommand> commands;
    commands.reserve(widgets.size() * 2);

    std::unordered_set<const gui::WidgetDefinition*>
        listTemplateDefinitions;
    std::function<void(const gui::WidgetDefinition&)> collectTemplate =
        [&](const gui::WidgetDefinition& definition)
    {
        listTemplateDefinitions.insert(&definition);
        for (const gui::WidgetDefinition& child : definition.children)
        {
            collectTemplate(child);
        }
    };
    for (const gui::GuiResolvedWidget& widget : widgets)
    {
        if (widget.definition
            && widget.listIndex < 0
            && listTemplateNames.find(widget.definition->name)
                != listTemplateNames.end())
        {
            collectTemplate(*widget.definition);
        }
    }

    for (const gui::GuiResolvedWidget& widget : widgets)
    {
        GuiRenderCommandType type;
        if (!ResolveCommandType(
                widget,
                listTemplateDefinitions,
                type
            ))
        {
            continue;
        }

        int zOrder = widget.zOrder;
        std::size_t order = widget.order;
        if (type == GuiRenderCommandType::WindowFrame)
        {
            const int offset = widget.definition->frameZOrder;
            if (offset > 0
                && zOrder > std::numeric_limits<int>::max() - offset)
            {
                zOrder = std::numeric_limits<int>::max();
            }
            else if (offset < 0
                && zOrder < std::numeric_limits<int>::min() - offset)
            {
                zOrder = std::numeric_limits<int>::min();
            }
            else
            {
                zOrder += offset;
            }
            order = std::numeric_limits<std::size_t>::max();
        }

        commands.push_back({type, &widget, zOrder, order});
        if (NeedsButtonText(widget, listTemplateNames))
        {
            commands.push_back({
                GuiRenderCommandType::Text,
                &widget,
                zOrder,
                order
            });
        }
    }

    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const GuiRenderCommand& first,
           const GuiRenderCommand& second)
        {
            if (first.zOrder != second.zOrder)
            {
                return first.zOrder < second.zOrder;
            }
            return first.order < second.order;
        }
    );
    return commands;
}
