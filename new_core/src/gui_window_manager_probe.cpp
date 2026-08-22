#include "gui_window_manager.h"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    GuiWindowManager manager;
    if (!manager.Register({"first", 0, false})
        || !manager.Register({"second", 0, false})
        || !manager.Register({"system", 10, false})
        || manager.Register({"FIRST", 0, false}))
    {
        std::cerr << "Window registration failed\n";
        return 1;
    }

    manager.SetState("first", true, true);
    manager.SetState("second", true, true);
    manager.SetState("system", true, true);
    if (manager.RenderOrder()
        != std::vector<std::string>({"first", "second", "system"}))
    {
        std::cerr << "Initial render order failed\n";
        return 1;
    }

    manager.Focus("first");
    if (manager.RenderOrder()
            != std::vector<std::string>({"second", "first", "system"})
        || manager.InputOrder()
            != std::vector<std::string>({"system", "first", "second"}))
    {
        std::cerr << "Focus ordering failed\n";
        return 1;
    }

    if (!manager.Register({"modal", -100, true}))
    {
        std::cerr << "Modal registration failed\n";
        return 1;
    }
    manager.SetState("modal", true, true);
    if (!manager.HasActiveModal()
        || manager.RenderOrder().back() != "modal"
        || manager.InputOrder() != std::vector<std::string>({"modal"}))
    {
        std::cerr << "Modal isolation failed\n";
        return 1;
    }

    manager.SetState("modal", true, false);
    if (manager.HasActiveModal()
        || manager.InputOrder().front() != "system"
        || !manager.Unregister("modal")
        || manager.Size() != 3)
    {
        std::cerr << "Window state cleanup failed\n";
        return 1;
    }

    std::cout << "Window manager ordering passed\n";
    return 0;
}
