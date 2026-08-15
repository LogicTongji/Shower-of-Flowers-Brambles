#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

class GuiPluginRegistry;

bool LoadGuiPluginManifestDirectory(
    const std::filesystem::path& root,
    GuiPluginRegistry& registry,
    std::size_t& loadedCount,
    std::string& error
);
