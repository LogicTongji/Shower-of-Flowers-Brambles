#pragma once

#include <unordered_set>
#include <vector>

#include "gui_interpreter.h"

enum class GuiRenderCommandType
{
    IndexedMap,
    MarkerLayer,
    Custom,
    Image,
    Button,
    ColorBox,
    ProgressBar,
    List,
    Text,
    WindowFrame
};

struct GuiRenderCommand
{
    GuiRenderCommandType type = GuiRenderCommandType::Image;
    const gui::GuiResolvedWidget* widget = nullptr;
    int zOrder = 0;
    std::size_t order = 0;
};

std::vector<GuiRenderCommand> BuildGuiRenderQueue(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    const std::unordered_set<std::string>& listTemplateNames
);
