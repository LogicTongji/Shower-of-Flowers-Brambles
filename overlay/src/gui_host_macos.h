#pragma once

#include <filesystem>
#include <vector>

#include "gui_plugin.h"

struct GuiMacHostOptions
{
    bool printResourceStats = false;
};

int RunGuiMacHostApplication(
    const std::filesystem::path& root,
    const std::vector<GuiPluginLaunch>& launches,
    const GuiMacHostOptions& options = {}
);

int RunGuiMacHost(
    const std::filesystem::path& root,
    IGuiPlugin& plugin
);
