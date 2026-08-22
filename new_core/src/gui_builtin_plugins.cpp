#include "gui_builtin_plugins.h"

#include "declarative_gui_plugin.h"
#include "gui_plugin_registry.h"

bool RegisterBuiltinGuiPluginFactories(
    GuiPluginRegistry& registry
)
{
    return registry.RegisterFactory(
        "declarative_gui",
        CreateDeclarativeGuiPlugin
    );
}
