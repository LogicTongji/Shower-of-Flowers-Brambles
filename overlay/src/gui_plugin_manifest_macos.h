#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

class GuiMacPluginRegistry;

bool LoadGuiMacPluginManifestDirectory(
    const std::filesystem::path& root,
    GuiMacPluginRegistry& registry,
    std::size_t& loadedCount,
    std::string& error
);
