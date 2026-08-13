#include "gui_builtin_plugins_macos.h"

#include "declarative_gui_plugin_macos.h"
#include "gui_plugin_registry_macos.h"

bool RegisterBuiltinGuiMacPluginFactories(
    GuiMacPluginRegistry& registry
)
{
    return registry.RegisterFactory(
        "declarative_gui",
        CreateDeclarativeGuiMacPlugin
    );
}
