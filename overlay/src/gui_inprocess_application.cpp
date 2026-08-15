#include "gui_inprocess_application.h"

#include <utility>

#include "gui_builtin_plugins.h"
#include "gui_lua_bridge.h"
#include "gui_plugin_manifest.h"

GuiInProcessApplication::~GuiInProcessApplication()
{
    Shutdown();
}

bool GuiInProcessApplication::Initialize(
    const std::filesystem::path& root,
    std::string& error
)
{
    if (initialized_)
    {
        return true;
    }

    root_ = std::filesystem::absolute(root).lexically_normal();
    if (!std::filesystem::is_directory(root_ / "interface"))
    {
        error = "Scripted GUI interface directory not found: "
            + (root_ / "interface").string();
        return false;
    }
    if (!interpreter_.LoadDirectory(root_ / "interface", error))
    {
        error = "Failed to load GUI definitions: " + error;
        return false;
    }

    const std::filesystem::path behaviorRoot =
        std::filesystem::is_directory(root_ / "script_gui")
        ? root_ / "script_gui"
        : root_ / "scripted_guis";
    if (std::filesystem::is_directory(behaviorRoot))
    {
        if (!behaviors_.LoadDirectory(behaviorRoot, error))
        {
            error = "Failed to load GUI behaviors: " + error;
            return false;
        }
        behaviorsLoaded_ = true;
    }

    GuiLuaBridgeService& luaBridge = GetGuiLuaBridgeService();
    luaBridge.ResetAll();
    if (!RegisterGuiLuaDataBridgeChannel(
            bridgeChannels_,
            luaBridge
        )
        || !RegisterGuiBridgeDataProvider(
            dataProviders_,
            bridgeChannels_
        )
        || !RegisterBuiltinGuiPluginFactories(pluginRegistry_))
    {
        error = "Failed to register in-process GUI services";
        return false;
    }

    const std::filesystem::path manifestRoot =
        root_ / "interface" / "gui_plugins";
    std::size_t loadedCount = 0;
    if (!LoadGuiPluginManifestDirectory(
            manifestRoot,
            pluginRegistry_,
            loadedCount,
            error
        ))
    {
        error = "Failed to load GUI plugin manifests: " + error;
        return false;
    }

    for (const GuiPluginDescriptor& descriptor
        : pluginRegistry_.Descriptors())
    {
        if (!descriptor.startup)
        {
            continue;
        }

        GuiPluginCreateContext createContext;
        createContext.root = root_;
        createContext.dataProviders = &dataProviders_;
        createContext.options["data_provider"] = "bridge";
        createContext.options["channel"] = "lua";
        createContext.options["bridge_name"] = descriptor.id;

        std::unique_ptr<IGuiPlugin> plugin =
            pluginRegistry_.Create(descriptor.id, createContext);
        if (!plugin)
        {
            error = "Failed to create live GUI plugin: "
                + descriptor.id;
            Shutdown();
            return false;
        }

        IGuiPlugin* pluginPointer = plugin.get();
        plugins_.push_back(std::move(plugin));
        launches_.push_back({
            descriptor.id,
            descriptor.visibleWhen,
            pluginPointer
        });
    }

    if (launches_.empty())
    {
        error = "No startup GUI plugins are configured";
        Shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void GuiInProcessApplication::Shutdown()
{
    launches_.clear();
    plugins_.clear();
    GetGuiLuaBridgeService().ResetAll();
    initialized_ = false;
}

bool GuiInProcessApplication::IsInitialized() const
{
    return initialized_;
}

const std::filesystem::path& GuiInProcessApplication::Root() const
{
    return root_;
}

const gui::GuiInterpreter&
GuiInProcessApplication::Interpreter() const
{
    return interpreter_;
}

const GuiBehaviorRegistry* GuiInProcessApplication::Behaviors() const
{
    return behaviorsLoaded_ ? &behaviors_ : nullptr;
}

const std::vector<GuiPluginLaunch>&
GuiInProcessApplication::Launches() const
{
    return launches_;
}
