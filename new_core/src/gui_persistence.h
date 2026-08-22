#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "gui_data_value.h"
#include "gui_list_model.h"

struct GuiPersistentState
{
    struct ListRuntimeState
    {
        int scrollOffset = 0;
        uint64_t selectedItemId = 0;

        bool operator==(const ListRuntimeState& other) const
        {
            return scrollOffset == other.scrollOffset
                && selectedItemId == other.selectedItemId;
        }
    };

    std::unordered_map<std::string, GuiDataValue> values;
    std::unordered_map<std::string, GuiListModel> lists;
    std::unordered_map<std::string, ListRuntimeState> listRuntime;
};

class GuiPersistenceStore
{
public:
    explicit GuiPersistenceStore(std::filesystem::path root);

    bool Load(
        std::string_view profileKey,
        std::string_view pluginId,
        GuiPersistentState& state,
        std::string& error
    ) const;

    bool Save(
        std::string_view profileKey,
        std::string_view pluginId,
        const GuiPersistentState& state,
        std::string& error
    ) const;

    bool Remove(
        std::string_view profileKey,
        std::string_view pluginId,
        std::string& error
    ) const;

    std::filesystem::path ResolvePath(
        std::string_view profileKey,
        std::string_view pluginId
    ) const;

    const std::filesystem::path& Root() const;

private:
    std::filesystem::path root_;
};
