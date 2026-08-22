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

    gui::WidgetDefinition scrollbar;
    scrollbar.type = gui::WidgetType::ScrollBar;

    gui::WidgetDefinition templateButton;
    templateButton.type = gui::WidgetType::Button;
    templateButton.name = "item_template";
    templateButton.font = "probe_font";

    gui::WidgetDefinition nestedButton;
    nestedButton.type = gui::WidgetType::Button;
    nestedButton.name = "nested_action";

    gui::WidgetDefinition custom;
    custom.type = gui::WidgetType::Custom;
    custom.customType = "probe_custom";

    gui::WidgetDefinition marker;
    marker.type = gui::WidgetType::MarkerLayer;

    std::vector<gui::GuiResolvedWidget> widgets(11);
    widgets[0] = {&frame, {}, true, true, 1.0f, 0, 0, 0};
    widgets[1] = {&image, {}, true, true, 1.0f, 0, 5, 4};
    widgets[2] = {&text, {}, true, true, 1.0f, 0, -2, 5};
    widgets[3] = {&map, {}, true, true, 1.0f, 0, 5, 2};
    widgets[4] = {&list, {}, true, true, 1.0f, 0, 3, 3};
    widgets[5] = {&templateButton, {}, true, true, 1.0f, 0, 1, 1};
    widgets[6] = {&scrollbar, {}, true, true, 1.0f, 0, 2, 6};
    widgets[7] = {&templateButton, {}, true, true, 1.0f, 0, 1, 7};
    widgets[7].listName = "probe_list";
    widgets[7].listIndex = 0;
    widgets[7].listItemId = 101;
    widgets[8] = {&nestedButton, {}, true, true, 1.0f, 0, 1, 8};
    widgets[8].listName = "probe_list";
    widgets[8].listIndex = 0;
    widgets[8].listItemId = 101;
    widgets[9] = {&custom, {}, true, true, 1.0f, 0, 4, 9};
    widgets[10] = {&marker, {}, true, true, 1.0f, 0, 4, 10};

    const std::vector<GuiRenderCommand> queue = BuildGuiRenderQueue(
        widgets,
        std::unordered_set<std::string>{"item_template"}
    );
    if (queue.size() != 10
        || queue[0].type != GuiRenderCommandType::Text
        || queue[1].type != GuiRenderCommandType::Button
        || queue[1].widget->listIndex != 0
        || queue[2].type != GuiRenderCommandType::Text
        || queue[2].widget->listIndex != 0
        || queue[3].type != GuiRenderCommandType::Button
        || queue[3].widget->definition != &nestedButton
        || queue[4].type != GuiRenderCommandType::ScrollBar
        || queue[5].type != GuiRenderCommandType::Custom
        || queue[5].widget->definition != &custom
        || queue[6].type != GuiRenderCommandType::Custom
        || queue[6].widget->definition != &marker
        || queue[7].type != GuiRenderCommandType::IndexedMap
        || queue[8].type != GuiRenderCommandType::Image
        || queue[9].type != GuiRenderCommandType::WindowFrame)
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
