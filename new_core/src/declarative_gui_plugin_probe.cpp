#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "declarative_gui_plugin.h"
#include "gui_file_data_provider.h"
#include "gui_plugin_registry.h"

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    const fs::path root = argc >= 2
        ? fs::path(argv[1])
        : fs::current_path();
    const fs::path source = root
        / "new_core"
        / "tests"
        / "fixtures"
        / "declarative_gui_data.txt";
    const fs::path temporary = fs::temp_directory_path()
        / ("declarative_gui_plugin_probe_"
            + std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()
            )
            + ".txt");

    std::error_code fileError;
    fs::copy_file(
        source,
        temporary,
        fs::copy_options::overwrite_existing,
        fileError
    );
    if (fileError)
    {
        std::cerr << "Failed to create hot-reload fixture\n";
        return 1;
    }

    GuiDataProviderRegistry dataProviders;
    if (!RegisterBuiltinGuiDataProviders(dataProviders))
    {
        std::cerr << "Failed to register file data provider\n";
        fs::remove(temporary, fileError);
        return 1;
    }
    GuiPluginCreateContext createContext;
    createContext.root = root;
    createContext.dataProviders = &dataProviders;
    createContext.options["window"] = "probe_window";
    createContext.options["title"] = "Probe";
    createContext.options["data_provider"] = "file";
    createContext.options["data"] = temporary.string();
    createContext.options["tick_interval"] = "25";
    std::unique_ptr<IGuiPlugin> plugin =
        CreateDeclarativeGuiPlugin(createContext);
    if (!plugin)
    {
        std::cerr << "Failed to create declarative GUI plugin\n";
        fs::remove(temporary, fileError);
        return 1;
    }
    if (CreateDeclarativeGuiPlugin(
            GuiPluginCreateContext{}
        ))
    {
        std::cerr << "Declarative GUI factory accepted no provider\n";
        fs::remove(temporary, fileError);
        return 1;
    }

    gui::GuiInterpreter interpreter;
    GuiWindowRuntime windowRuntime;
    std::string error;
    if (!plugin->Initialize(
            GuiPluginInitContext{
                root,
                nullptr,
                interpreter,
                windowRuntime
            },
            error
        ))
    {
        std::cerr << error << '\n';
        fs::remove(temporary, fileError);
        return 1;
    }
    if (plugin->BuildDataRegistry()->ResolveNumber("counter") != 2.0)
    {
        std::cerr << "Initial declarative plugin data is invalid\n";
        fs::remove(temporary, fileError);
        return 1;
    }

    const fs::file_time_type previousWriteTime = fs::last_write_time(
        temporary,
        fileError
    );
    {
        std::ofstream output(temporary, std::ios::trunc);
        output
            << "guiData = {\n"
            << "  state.visible = yes\n"
            << "  counter = 9\n"
            << "}\n";
    }
    fs::last_write_time(
        temporary,
        previousWriteTime + std::chrono::seconds(2),
        fileError
    );
    if (fileError
        || !plugin->Tick(1000)
        || plugin->BuildDataRegistry()->ResolveNumber("counter") != 9.0)
    {
        std::cerr << "Declarative plugin hot reload failed\n";
        fs::remove(temporary, fileError);
        return 1;
    }

    GuiActionContext action;
    action.fallbackOperation = "add_value";
    action.parameters["target"] = "counter";
    action.parameters["amount"] = "4";
    if (!plugin->HandleAction(action)
        || plugin->BuildDataRegistry()->ResolveNumber("counter") != 13.0)
    {
        std::cerr << "Declarative plugin action handling failed\n";
        fs::remove(temporary, fileError);
        return 1;
    }

    std::cout
        << "Declarative plugin tick interval: "
        << plugin->TickIntervalMilliseconds() << '\n'
        << "Hot-reloaded counter: "
        << plugin->BuildDataRegistry()->ResolveNumber("counter") << '\n';

    plugin->Shutdown();
    fs::remove(temporary, fileError);
    return 0;
}
