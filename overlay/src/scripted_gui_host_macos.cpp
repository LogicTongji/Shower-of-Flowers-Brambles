#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gui_builtin_plugins_macos.h"
#include "gui_data_bridge.h"
#include "gui_file_data_provider.h"
#include "gui_host_macos.h"
#include "gui_memory_data_bridge.h"
#include "gui_lua_bridge.h"
#include "gui_plugin_manifest_macos.h"
#include "gui_plugin_registry_macos.h"
#include "gui_sequence_data_provider.h"

namespace
{

struct CommandLineOptions
{
    std::filesystem::path root = std::filesystem::current_path();
    std::filesystem::path dataPath;
    std::vector<std::string> pluginIds;
    bool listPlugins = false;
    bool printResourceStats = false;
    bool showHelp = false;
};

void PrintUsage(std::string_view executable)
{
    std::cout
        << "Usage: " << executable << " [root] [data] [plugin_id]\n"
        << "       " << executable
        << " [--root path] [--data path] [--plugin id ...]"
        << " [--resource-stats]\n"
        << "       " << executable << " --list-plugins\n";
}

void PrintPlugins(const GuiMacPluginRegistry& registry)
{
    std::cout << "Available scripted GUI plugins:\n";
    for (const GuiMacPluginDescriptor& descriptor : registry.Descriptors())
    {
        std::cout
            << "  " << descriptor.id
            << " - " << descriptor.displayName
            << " [" << descriptor.factoryType << "]"
            << (descriptor.startup ? " startup" : " manual")
            << '\n';
    }
}

bool ReadOptionValue(
    int argc,
    char** argv,
    int& index,
    std::string_view option,
    std::string& value
)
{
    if (index + 1 >= argc)
    {
        std::cerr << "Missing value for " << option << "\n";
        return false;
    }
    value = argv[++index];
    return true;
}

bool ParseCommandLine(
    int argc,
    char** argv,
    CommandLineOptions& options
)
{
    std::vector<std::string> positional;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
        }
        else if (argument == "--list-plugins")
        {
            options.listPlugins = true;
        }
        else if (argument == "--resource-stats")
        {
            options.printResourceStats = true;
        }
        else if (argument == "--root")
        {
            std::string value;
            if (!ReadOptionValue(argc, argv, index, argument, value))
            {
                return false;
            }
            options.root = value;
        }
        else if (argument == "--data")
        {
            std::string value;
            if (!ReadOptionValue(argc, argv, index, argument, value))
            {
                return false;
            }
            options.dataPath = value;
        }
        else if (argument == "--plugin")
        {
            std::string pluginId;
            if (!ReadOptionValue(
                    argc,
                    argv,
                    index,
                    argument,
                    pluginId
                ))
            {
                return false;
            }
            options.pluginIds.push_back(std::move(pluginId));
        }
        else if (!argument.empty() && argument.front() == '-')
        {
            std::cerr << "Unknown option: " << argument << "\n";
            return false;
        }
        else
        {
            positional.push_back(argument);
        }
    }

    if (positional.size() > 3)
    {
        std::cerr << "Too many positional arguments\n";
        return false;
    }

    if (!positional.empty())
    {
        options.root = positional[0];
    }
    if (positional.size() >= 2)
    {
        options.dataPath = positional[1];
    }
    if (positional.size() >= 3)
    {
        options.pluginIds.push_back(positional[2]);
    }
    return true;
}

}

int main(int argc, char** argv)
{
    CommandLineOptions options;
    if (!ParseCommandLine(argc, argv, options))
    {
        PrintUsage(argv[0]);
        return 1;
    }

    GuiDataBridgeChannelRegistry dataBridgeChannels;
    if (!RegisterBuiltinGuiDataBridgeChannels(dataBridgeChannels)
        || !RegisterGuiLuaDataBridgeChannel(
            dataBridgeChannels,
            GetGuiLuaBridgeService()
        ))
    {
        std::cerr << "Failed to register GUI data bridge channels\n";
        return 1;
    }

    GuiDataProviderRegistry dataProviders;
    if (!RegisterBuiltinGuiDataProviders(dataProviders)
        || !RegisterGuiSequenceDataProvider(dataProviders)
        || !RegisterGuiBridgeDataProvider(
            dataProviders,
            dataBridgeChannels
        ))
    {
        std::cerr << "Failed to register built-in GUI data providers\n";
        return 1;
    }

    GuiMacPluginRegistry registry;
    if (!RegisterBuiltinGuiMacPluginFactories(registry))
    {
        std::cerr << "Failed to register built-in GUI plugins\n";
        return 1;
    }

    const std::filesystem::path manifestRoot =
        options.root / "interface" / "gui_plugins";
    if (!std::filesystem::is_directory(manifestRoot))
    {
        std::cerr << "GUI plugin manifest directory not found: "
                  << manifestRoot << "\n";
        return 1;
    }

    std::size_t loadedCount = 0;
    std::string manifestError;
    if (!LoadGuiMacPluginManifestDirectory(
            manifestRoot,
            registry,
            loadedCount,
            manifestError
        ))
    {
        std::cerr << "Failed to load GUI plugin manifests: "
                  << manifestError << "\n";
        return 1;
    }

    if (options.showHelp)
    {
        PrintUsage(argv[0]);
        PrintPlugins(registry);
        return 0;
    }

    if (options.listPlugins)
    {
        PrintPlugins(registry);
        return 0;
    }

    std::vector<std::string> selectedPluginIds = options.pluginIds;
    if (selectedPluginIds.empty())
    {
        for (const GuiMacPluginDescriptor& descriptor
            : registry.Descriptors())
        {
            if (descriptor.startup)
            {
                selectedPluginIds.push_back(descriptor.id);
            }
        }
    }
    if (selectedPluginIds.empty())
    {
        std::cerr << "No startup GUI plugins are configured\n";
        return 1;
    }

    GuiMacPluginCreateContext createContext;
    createContext.root = options.root;
    createContext.dataProviders = &dataProviders;
    if (!options.dataPath.empty())
    {
        createContext.options["data"] = options.dataPath.string();
    }

    std::vector<std::unique_ptr<IGuiMacPlugin>> plugins;
    std::vector<GuiMacPluginLaunch> launches;
    plugins.reserve(selectedPluginIds.size());
    launches.reserve(selectedPluginIds.size());
    for (const std::string& pluginId : selectedPluginIds)
    {
        const GuiMacPluginDescriptor* descriptor = registry.Find(pluginId);
        std::unique_ptr<IGuiMacPlugin> plugin = registry.Create(
            pluginId,
            createContext
        );
        if (!descriptor || !plugin)
        {
            std::cerr
                << "Unknown or unavailable GUI plugin: "
                << pluginId << "\n";
            PrintPlugins(registry);
            return 1;
        }

        IGuiMacPlugin* pluginPointer = plugin.get();
        plugins.push_back(std::move(plugin));
        launches.push_back({
            descriptor->id,
            descriptor->visibleWhen,
            pluginPointer
        });
    }

    GuiMacHostOptions hostOptions;
    hostOptions.printResourceStats = options.printResourceStats;
    return RunGuiMacHostApplication(
        options.root,
        launches,
        hostOptions
    );
}
