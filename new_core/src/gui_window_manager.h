#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct GuiManagedWindowConfig
{
    std::string id;
    int zOrder = 0;
    bool modal = false;
};

class GuiWindowManager
{
public:
    bool Register(GuiManagedWindowConfig config);
    bool Unregister(std::string_view id);
    void Clear();

    bool SetState(
        std::string_view id,
        bool open,
        bool visible
    );
    bool Focus(std::string_view id);

    std::vector<std::string> RenderOrder() const;
    std::vector<std::string> InputOrder() const;
    bool HasActiveModal() const;
    std::size_t Size() const;

private:
    struct WindowState
    {
        GuiManagedWindowConfig config;
        uint64_t registrationOrder = 0;
        uint64_t focusOrder = 0;
        bool open = false;
        bool visible = false;
    };

    std::vector<const WindowState*> OrderedActiveWindows() const;

    std::unordered_map<std::string, WindowState> windows_;
    uint64_t registrationSerial_ = 0;
    uint64_t focusSerial_ = 0;
};
