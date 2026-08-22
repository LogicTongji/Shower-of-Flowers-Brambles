#include "gui_inprocess_application.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "gui_builtin_plugins.h"
#include "gui_file_data_provider.h"
#include "gui_lua_bridge.h"
#include "gui_plugin_manifest.h"
#include "gui_sequence_data_provider.h"

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

bool EnvironmentFlagEnabled(const char* name)
{
    const char* value = std::getenv(name);
    if (!value)
    {
        return false;
    }
    const std::string normalized = Lower(value);
    return normalized == "1"
        || normalized == "yes"
        || normalized == "true"
        || normalized == "on";
}

std::filesystem::path ResolvePersistenceRoot(
    const std::filesystem::path& fallbackRoot
)
{
#if defined(_WIN32)
    if (const wchar_t* configured = _wgetenv(L"NEW_CORE_STATE_ROOT"))
    {
        if (*configured != L'\0')
        {
            return std::filesystem::path(configured) / "script_gui";
        }
    }
    if (const wchar_t* configured = _wgetenv(L"SCRIPTED_GUI_STATE_ROOT"))
    {
        if (*configured != L'\0')
        {
            return std::filesystem::path(configured);
        }
    }
    if (const wchar_t* localAppData = _wgetenv(L"LOCALAPPDATA"))
    {
        if (*localAppData != L'\0')
        {
            return std::filesystem::path(localAppData)
                / "HOI3 Scripted GUI" / "state";
        }
    }
#else
    if (const char* configured = std::getenv("SCRIPTED_GUI_STATE_ROOT"))
    {
        if (*configured != '\0')
        {
            return std::filesystem::path(configured);
        }
    }
#endif
    return fallbackRoot / "new_core" / "state" / "script_gui";
}

void AddIssue(
    std::vector<GuiConfigurationIssue>& issues,
    std::string pluginId,
    std::string stage,
    std::string message
)
{
    issues.push_back({
        std::move(pluginId),
        std::move(stage),
        std::move(message)
    });
}

void CollectWidgets(
    const gui::WidgetDefinition& widget,
    std::vector<const gui::WidgetDefinition*>& widgets,
    std::unordered_map<std::string, std::size_t>& names
)
{
    widgets.push_back(&widget);
    if (!widget.name.empty())
    {
        ++names[Lower(widget.name)];
    }
    for (const gui::WidgetDefinition& child : widget.children)
    {
        CollectWidgets(child, widgets, names);
    }
}

bool ValidatePluginLayout(
    const std::filesystem::path& root,
    const GuiPluginDescriptor& descriptor,
    const gui::GuiInterpreter& interpreter,
    std::vector<GuiConfigurationIssue>& issues
)
{
    const auto windowOption = descriptor.defaultOptions.find("window");
    const std::string windowName = windowOption
        == descriptor.defaultOptions.end()
        ? std::string{}
        : windowOption->second;
    if (windowName.empty())
    {
        AddIssue(
            issues,
            descriptor.id,
            "manifest",
            "plugin_window_missing"
        );
        return false;
    }

    const gui::WindowDefinition* window =
        interpreter.FindWindow(windowName);
    if (!window)
    {
        AddIssue(
            issues,
            descriptor.id,
            "layout",
            "plugin_window_not_found:" + windowName
        );
        return false;
    }

    bool valid = true;
    std::vector<const gui::WidgetDefinition*> widgets;
    std::unordered_map<std::string, std::size_t> names;
    CollectWidgets(*window, widgets, names);
    for (const auto& [name, count] : names)
    {
        if (count > 1)
        {
            AddIssue(
                issues,
                descriptor.id,
                "layout",
                "widget_name_duplicate:" + name
            );
            valid = false;
        }
    }

    const auto requireWidget = [&names, &issues, &descriptor, &valid](
        std::string_view kind,
        const std::string& name
    )
    {
        if (!name.empty()
            && names.find(Lower(name)) == names.end())
        {
            AddIssue(
                issues,
                descriptor.id,
                "layout",
                std::string(kind) + "_not_found:" + name
            );
            valid = false;
        }
    };
    const auto requireSprite = [
        &root,
        &interpreter,
        &issues,
        &descriptor,
        &valid
    ](const std::string& name)
    {
        if (name.empty())
        {
            return;
        }
        const gui::SpriteResource* sprite = interpreter.FindSprite(name);
        const std::filesystem::path path =
            interpreter.ResolveTexture(name, root);
        if (!sprite || path.empty()
            || !std::filesystem::is_regular_file(path))
        {
            AddIssue(
                issues,
                descriptor.id,
                "resource",
                "sprite_not_found:" + name
            );
            valid = false;
        }
    };

    for (const gui::WidgetDefinition* widget : widgets)
    {
        if (!widget)
        {
            continue;
        }
        if (widget->type == gui::WidgetType::Unknown)
        {
            AddIssue(
                issues,
                descriptor.id,
                "layout",
                "widget_type_unknown:" + widget->name
            );
            valid = false;
        }
        requireSprite(widget->spriteName);
        requireSprite(widget->pressedSpriteName);
        requireSprite(widget->frameSpriteName);
        requireSprite(widget->markerActionSpriteName);
        if (widget->type == gui::WidgetType::ScrollBar)
        {
            requireSprite(widget->sliderName);
            requireSprite(widget->trackName);
        }
        requireWidget("item_template", widget->templateName);
        requireWidget("scrollbar", widget->scrollBarName);
        requireWidget("map_widget", widget->mapWidgetName);
        requireWidget("drag_track", widget->dragTrackName);

        if (!widget->progressResourceName.empty()
            && !interpreter.FindProgressBar(
                widget->progressResourceName
            ))
        {
            AddIssue(
                issues,
                descriptor.id,
                "resource",
                "progress_resource_not_found:"
                    + widget->progressResourceName
            );
            valid = false;
        }
        if (!widget->indexedMapResourceName.empty()
            && !interpreter.FindIndexedMap(
                widget->indexedMapResourceName
            ))
        {
            AddIssue(
                issues,
                descriptor.id,
                "resource",
                "indexed_map_resource_not_found:"
                    + widget->indexedMapResourceName
            );
            valid = false;
        }
    }
    return valid;
}

}

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

    issues_.clear();
    root_ = std::filesystem::absolute(root).lexically_normal();
    persistenceStore_ = std::make_shared<GuiPersistenceStore>(
        ResolvePersistenceRoot(root_)
    );
    if (!std::filesystem::is_directory(root_ / "interface"))
    {
        error = "Scripted GUI interface directory not found: "
            + (root_ / "interface").string();
        return false;
    }
	interpreter_.SetStrictLegacyFiles(
		EnvironmentFlagEnabled("SCRIPTED_GUI_STRICT_LEGACY")
	);
    if (!interpreter_.LoadDirectory(root_ / "interface", error))
    {
        error = "Failed to load GUI definitions: " + error;
        return false;
    }
    for (const std::string& diagnostic
        : interpreter_.LoadDiagnostics())
    {
        AddIssue(issues_, {}, "interface_parse", diagnostic);
    }

    const std::filesystem::path behaviorRoot =
        std::filesystem::is_directory(root_ / "script_gui")
        ? root_ / "script_gui"
        : root_ / "scripted_guis";
    if (std::filesystem::is_directory(behaviorRoot))
    {
        std::vector<std::string> behaviorDiagnostics;
        if (!behaviors_.LoadDirectory(
                behaviorRoot,
                error,
                &behaviorDiagnostics
            ))
        {
            error = "Failed to load GUI behaviors: " + error;
            return false;
        }
        for (std::string& diagnostic : behaviorDiagnostics)
        {
            AddIssue(
                issues_,
                {},
                "behavior_parse",
                std::move(diagnostic)
            );
        }
        behaviorsLoaded_ = true;
    }

    GuiLuaBridgeService& luaBridge = GetGuiLuaBridgeService();
    luaBridge.ResetAll();
    if (!RegisterBuiltinGuiDataProviders(dataProviders_)
        || !RegisterGuiSequenceDataProvider(dataProviders_)
        || !RegisterGuiLuaDataBridgeChannel(
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
    std::vector<std::string> manifestDiagnostics;
    if (!LoadGuiPluginManifestDirectory(
            manifestRoot,
            pluginRegistry_,
            loadedCount,
            error,
            &manifestDiagnostics
        ))
    {
        error = "Failed to load GUI plugin manifests: " + error;
        return false;
    }
    for (std::string& diagnostic : manifestDiagnostics)
    {
        AddIssue(
            issues_,
            {},
            "manifest_parse",
            std::move(diagnostic)
        );
    }

    for (const GuiPluginDescriptor& descriptor
        : pluginRegistry_.Descriptors())
    {
        if (!ValidatePluginLayout(
                root_,
                descriptor,
                interpreter_,
                issues_
            ))
        {
            continue;
        }
        GuiPluginCreateContext createContext;
        createContext.root = root_;
        createContext.dataProviders = &dataProviders_;
        constexpr std::string_view inProcessPrefix = "inprocess_";
        for (const auto& option : descriptor.defaultOptions)
        {
            if (option.first.rfind(inProcessPrefix, 0) == 0
                && option.first.size() > inProcessPrefix.size())
            {
                createContext.options[option.first.substr(
                    inProcessPrefix.size()
                )] = option.second;
            }
        }
        const auto provider = createContext.options.find(
            "data_provider"
        );
        if (provider != createContext.options.end()
            && provider->second == "bridge"
            && createContext.options.find("bridge_name")
                == createContext.options.end())
        {
            createContext.options["bridge_name"] = descriptor.id;
        }

        std::unique_ptr<IGuiPlugin> plugin =
            pluginRegistry_.Create(descriptor.id, createContext);
        if (!plugin)
        {
            AddIssue(
                issues_,
                descriptor.id,
                "plugin_create",
                "failed_to_create_live_gui_plugin"
            );
            continue;
        }

        IGuiPlugin* pluginPointer = plugin.get();
        plugins_.push_back(std::move(plugin));
        launches_.push_back({
            descriptor.id,
            descriptor.visibleWhen,
            pluginPointer,
            descriptor.startup,
            descriptor.windowZOrder,
            descriptor.modal,
            descriptor.maxViewportWidthRatio,
            descriptor.maxViewportHeightRatio,
            descriptor.cascadeOffsetX,
            descriptor.cascadeOffsetY
        });
    }

    if (launches_.empty())
    {
        error = "No valid GUI plugins are configured";
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
    persistenceStore_.reset();
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

const std::vector<GuiConfigurationIssue>&
GuiInProcessApplication::Issues() const
{
    return issues_;
}

const std::shared_ptr<GuiPersistenceStore>&
GuiInProcessApplication::PersistenceStore() const
{
    return persistenceStore_;
}
