#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class GuiPluginRegistry;

bool LoadGuiPluginManifestDirectory(
    const std::filesystem::path& root,
    GuiPluginRegistry& registry,
    std::size_t& loadedCount,
    std::string& error,
    std::vector<std::string>* diagnostics = nullptr
);
