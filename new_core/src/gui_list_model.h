#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_data_value.h"

struct GuiListItem
{
    uint64_t id = 0;
    std::string text;
    std::unordered_map<std::string, GuiDataValue> fields;

    const GuiDataValue* Find(std::string_view name) const
    {
        std::string key(name);
        std::transform(
            key.begin(),
            key.end(),
            key.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            }
        );
        const auto iterator = fields.find(key);
        return iterator == fields.end() ? nullptr : &iterator->second;
    }

    bool operator==(
        const GuiListItem& other
    ) const
    {
        return id == other.id
            && text == other.text
            && fields == other.fields;
    }

    bool operator!=(
        const GuiListItem& other
    ) const
    {
        return !(*this == other);
    }
};

struct GuiListModel
{
    std::vector<GuiListItem> items;
    uint64_t revision = 0;

    size_t size() const
    {
        return items.size();
    }

    bool empty() const
    {
        return items.empty();
    }

    bool operator==(
        const GuiListModel& other
    ) const
    {
        return items == other.items;
    }

    bool operator!=(
        const GuiListModel& other
    ) const
    {
        return !(*this == other);
    }
};
