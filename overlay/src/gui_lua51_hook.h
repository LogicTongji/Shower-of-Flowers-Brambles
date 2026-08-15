#pragma once

#include <string>

#include "scripted_gui_overlay_api.h"

bool ResolveGuiLua51Api(
    ScriptedGuiLua51ApiV1& api,
    std::string& error
);

bool InstallGuiLua51Hooks(std::string& error);
void UninstallGuiLua51Hooks();
bool AreGuiLua51HooksInstalled();

bool AttachGuiLua51State(
    ScriptedGuiLuaState* state,
    std::string& error
);
