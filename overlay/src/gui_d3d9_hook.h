#pragma once

#include <string>

bool InstallGuiD3D9Hooks(std::string& error);
void UninstallGuiD3D9Hooks();
bool AreGuiD3D9HooksInstalled();
void MaintainGuiD3D9Hooks();
