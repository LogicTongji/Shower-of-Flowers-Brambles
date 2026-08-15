#include "gui_inprocess_application.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#include "gui_lua_bridge.h"
#include "gui_window_session.h"

int main(int argc, char** argv)
{
    const std::filesystem::path root = argc >= 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();

    GuiInProcessApplication application;
    std::string error;
    if (!application.Initialize(root, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    if (application.Launches().size() != 1
        || application.Launches().front().id != "china_anti_jap")
    {
        std::cerr << "Live startup plugin selection failed\n";
        return 1;
    }

    GuiDataBridgeUpdate snapshot;
    snapshot.revision = 1;
    snapshot.fullSnapshot = true;
    snapshot.values["state.visible"] = true;
    snapshot.values["state.active"] = true;
    std::string bridgeError;
    if (!GetGuiLuaBridgeService().PublishUpdate(
            "china_anti_jap",
            std::move(snapshot),
            bridgeError
        ))
    {
        std::cerr << bridgeError << '\n';
        return 1;
    }

    GuiWindowSessionController session(
        application.Root(),
        application.Launches().front(),
        application.Interpreter(),
        application.Behaviors()
    );
    if (!session.Bind(error)
        || !session.Initialize(nullptr, error)
        || !session.IsVisible())
    {
        std::cerr << "Live session initialization failed: "
                  << error << '\n';
        return 1;
    }

    session.Shutdown();
    application.Shutdown();
    std::cout << "In-process live plugin bootstrap: passed\n";
    return 0;
}
