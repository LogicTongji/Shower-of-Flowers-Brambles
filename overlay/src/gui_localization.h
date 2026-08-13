#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

class GuiLocalizationRegistry
{
public:
    bool LoadDirectory(
        const std::filesystem::path& root,
        std::string& error,
        std::size_t languageColumn = 1
    );

    bool LoadFile(
        const std::filesystem::path& path,
        std::string& error,
        std::size_t languageColumn = 1
    );

    std::string Resolve(std::string_view key) const;
    bool Contains(std::string_view key) const;
    void Clear();
    std::size_t Size() const;

private:
    std::unordered_map<std::string, std::string> values_;
};
