#include "gui_localization.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>
#include <vector>

namespace
{

std::string Normalize(std::string value)
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

std::vector<std::string> ParseRow(std::string line)
{
    if (line.size() >= 3
        && static_cast<unsigned char>(line[0]) == 0xef
        && static_cast<unsigned char>(line[1]) == 0xbb
        && static_cast<unsigned char>(line[2]) == 0xbf)
    {
        line.erase(0, 3);
    }
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t index = 0; index < line.size(); ++index)
    {
        const char character = line[index];
        if (character == '"')
        {
            if (quoted
                && index + 1 < line.size()
                && line[index + 1] == '"')
            {
                field.push_back('"');
                ++index;
            }
            else
            {
                quoted = !quoted;
            }
        }
        else if (character == ';' && !quoted)
        {
            fields.push_back(std::move(field));
            field.clear();
        }
        else
        {
            field.push_back(character);
        }
    }
    fields.push_back(std::move(field));
    return fields;
}

std::string SelectTranslation(
    const std::vector<std::string>& fields,
    std::size_t languageColumn
)
{
    if (languageColumn < fields.size()
        && !fields[languageColumn].empty())
    {
        return fields[languageColumn];
    }
    for (std::size_t index = 1; index < fields.size(); ++index)
    {
        if (Normalize(fields[index]) != "x"
            && !fields[index].empty())
        {
            return fields[index];
        }
    }
    return {};
}

}

bool GuiLocalizationRegistry::LoadDirectory(
    const std::filesystem::path& root,
    std::string& error,
    std::size_t languageColumn
)
{
    error.clear();
    if (!std::filesystem::exists(root)
        || !std::filesystem::is_directory(root))
    {
        error = "localization_directory_not_found: " + root.string();
        return false;
    }
    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& entry
        : std::filesystem::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file()
            && Normalize(entry.path().extension().string()) == ".csv")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& path : files)
    {
        if (!LoadFile(path, error, languageColumn))
        {
            return false;
        }
    }
    return true;
}

bool GuiLocalizationRegistry::LoadFile(
    const std::filesystem::path& path,
    std::string& error,
    std::size_t languageColumn
)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "localization_file_not_found: " + path.string();
        return false;
    }
    std::string line;
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = ParseRow(line);
        if (fields.empty()
            || fields.front().empty()
            || fields.front().front() == '#'
            || Normalize(fields.front()) == "xxxx")
        {
            continue;
        }
        const std::string translation = SelectTranslation(
            fields,
            languageColumn
        );
        if (!translation.empty())
        {
            values_[Normalize(fields.front())] = translation;
        }
    }
    error.clear();
    return true;
}

std::string GuiLocalizationRegistry::Resolve(std::string_view key) const
{
    const auto iterator = values_.find(Normalize(std::string(key)));
    return iterator == values_.end()
        ? std::string(key)
        : iterator->second;
}

bool GuiLocalizationRegistry::Contains(std::string_view key) const
{
    return values_.find(Normalize(std::string(key))) != values_.end();
}

void GuiLocalizationRegistry::Clear()
{
    values_.clear();
}

std::size_t GuiLocalizationRegistry::Size() const
{
    return values_.size();
}
