#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "gui_render_queue.h"

int main()
{
    gui::WidgetDefinition frame;
    frame.type = gui::WidgetType::Window;
    frame.frameSpriteName = "frame";
    frame.frameZOrder = 100;

    gui::WidgetDefinition image;
    image.type = gui::WidgetType::Image;

    gui::WidgetDefinition text;
    text.type = gui::WidgetType::Text;

    gui::WidgetDefinition map;
    map.type = gui::WidgetType::IndexedMap;

    gui::WidgetDefinition list;
    list.type = gui::WidgetType::ListBox;

    gui::WidgetDefinition templateButton;
    templateButton.type = gui::WidgetType::Button;
    templateButton.name = "item_template";

    std::vector<gui::GuiResolvedWidget> widgets(6);
    widgets[0] = {&frame, {}, true, true, 0, 0, 0};
    widgets[1] = {&image, {}, true, true, 0, 5, 4};
    widgets[2] = {&text, {}, true, true, 0, -2, 5};
    widgets[3] = {&map, {}, true, true, 0, 5, 2};
    widgets[4] = {&list, {}, true, true, 0, 3, 3};
    widgets[5] = {&templateButton, {}, true, true, 0, 1, 1};

    const std::vector<GuiRenderCommand> queue = BuildGuiRenderQueue(
        widgets,
        std::unordered_set<std::string>{"item_template"}
    );
    if (queue.size() != 5
        || queue[0].type != GuiRenderCommandType::Text
        || queue[1].type != GuiRenderCommandType::List
        || queue[2].type != GuiRenderCommandType::IndexedMap
        || queue[3].type != GuiRenderCommandType::Image
        || queue[4].type != GuiRenderCommandType::WindowFrame)
    {
        std::cerr << "Global render queue ordering failed\n";
        return 1;
    }

    std::cout << "Render queue commands: " << queue.size() << '\n';
    for (const GuiRenderCommand& command : queue)
    {
        std::cout << command.zOrder << ':' << command.order << '\n';
    }
    return 0;
}
