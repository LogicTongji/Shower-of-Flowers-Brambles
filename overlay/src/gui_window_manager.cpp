#include "gui_window_manager.h"

#include <algorithm>
#include <cctype>
#include <utility>

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

}

bool GuiWindowManager::Register(GuiManagedWindowConfig config)
{
    config.id = Lower(std::move(config.id));
    if (config.id.empty() || windows_.find(config.id) != windows_.end())
    {
        return false;
    }

    WindowState state;
    state.config = std::move(config);
    state.registrationOrder = ++registrationSerial_;
    state.focusOrder = ++focusSerial_;
    windows_.emplace(state.config.id, std::move(state));
    return true;
}

bool GuiWindowManager::Unregister(std::string_view id)
{
    return windows_.erase(Lower(std::string(id))) != 0;
}

void GuiWindowManager::Clear()
{
    windows_.clear();
    registrationSerial_ = 0;
    focusSerial_ = 0;
}

bool GuiWindowManager::SetState(
    std::string_view id,
    bool open,
    bool visible
)
{
    const auto found = windows_.find(Lower(std::string(id)));
    if (found == windows_.end())
    {
        return false;
    }
    found->second.open = open;
    found->second.visible = visible;
    return true;
}

bool GuiWindowManager::Focus(std::string_view id)
{
    const auto found = windows_.find(Lower(std::string(id)));
    if (found == windows_.end()
        || !found->second.open
        || !found->second.visible)
    {
        return false;
    }
    found->second.focusOrder = ++focusSerial_;
    return true;
}

std::vector<const GuiWindowManager::WindowState*>
GuiWindowManager::OrderedActiveWindows() const
{
    std::vector<const WindowState*> ordered;
    ordered.reserve(windows_.size());
    for (const auto& entry : windows_)
    {
        if (entry.second.open && entry.second.visible)
        {
            ordered.push_back(&entry.second);
        }
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const WindowState* first, const WindowState* second)
        {
            if (first->config.modal != second->config.modal)
            {
                return !first->config.modal;
            }
            if (first->config.zOrder != second->config.zOrder)
            {
                return first->config.zOrder < second->config.zOrder;
            }
            if (first->focusOrder != second->focusOrder)
            {
                return first->focusOrder < second->focusOrder;
            }
            return first->registrationOrder < second->registrationOrder;
        }
    );
    return ordered;
}

std::vector<std::string> GuiWindowManager::RenderOrder() const
{
    const std::vector<const WindowState*> ordered =
        OrderedActiveWindows();
    std::vector<std::string> result;
    result.reserve(ordered.size());
    for (const WindowState* window : ordered)
    {
        result.push_back(window->config.id);
    }
    return result;
}

std::vector<std::string> GuiWindowManager::InputOrder() const
{
    const std::vector<const WindowState*> ordered =
        OrderedActiveWindows();
    if (ordered.empty())
    {
        return {};
    }
    if (ordered.back()->config.modal)
    {
        return {ordered.back()->config.id};
    }

    std::vector<std::string> result;
    result.reserve(ordered.size());
    for (auto iterator = ordered.rbegin();
        iterator != ordered.rend();
        ++iterator)
    {
        result.push_back((*iterator)->config.id);
    }
    return result;
}

bool GuiWindowManager::HasActiveModal() const
{
    const std::vector<const WindowState*> ordered =
        OrderedActiveWindows();
    return !ordered.empty() && ordered.back()->config.modal;
}

std::size_t GuiWindowManager::Size() const
{
    return windows_.size();
}
