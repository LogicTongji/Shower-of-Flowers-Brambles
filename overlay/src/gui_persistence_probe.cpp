#include "gui_persistence.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("scripted_gui_persistence_probe_" + std::to_string(
            std::chrono::steady_clock::now()
                .time_since_epoch().count()
        ));
    GuiPersistenceStore store(root);
    GuiPersistentState source;
    source.values["boolean"] = true;
    source.values["integer"] = int64_t{42};
    source.values["number"] = 0.625;
    source.values["text"] = std::string("persisted");
    GuiListModel list;
    list.revision = 7;
    GuiListItem item;
    item.id = 91;
    item.text = "item";
    item.fields["region"] = std::string("hubei_region");
    item.fields["percentage"] = 80.5;
    list.items.push_back(std::move(item));
    source.lists["regions"] = std::move(list);
    source.listRuntime["regions"] = {18, 91};

    std::string error;
    GuiPersistentState loaded;
    if (!store.Save("save_a", "war_map", source, error)
        || !store.Load("save_a", "war_map", loaded, error)
        || loaded.values != source.values
        || loaded.lists != source.lists
        || loaded.listRuntime != source.listRuntime
        || loaded.lists.at("regions").revision != 7)
    {
        std::cerr << "Persistence round trip failed: " << error << '\n';
        return 1;
    }

    GuiPersistentState isolated;
    if (!store.Load("save_b", "war_map", isolated, error)
        || !isolated.values.empty()
        || !isolated.lists.empty())
    {
        std::cerr << "Persistence profile isolation failed\n";
        return 1;
    }

    source.values["integer"] = int64_t{99};
    if (!store.Save("save_a", "war_map", source, error)
        || !store.Load("save_a", "war_map", loaded, error)
        || std::get<int64_t>(loaded.values.at("integer")) != 99)
    {
        std::cerr << "Persistence atomic replacement failed: "
                  << error << '\n';
        return 1;
    }

    {
        std::ofstream corrupt(
            store.ResolvePath("save_a", "war_map"),
            std::ios::binary | std::ios::trunc
        );
        corrupt << "invalid";
    }
    if (store.Load("save_a", "war_map", loaded, error)
        || !loaded.values.empty()
        || error.empty())
    {
        std::cerr << "Persistence corruption isolation failed\n";
        return 1;
    }

    if (!store.Remove("save_a", "war_map", error))
    {
        std::cerr << "Persistence cleanup failed: " << error << '\n';
        return 1;
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::cout << "Persistence round trip and isolation passed\n";
    return 0;
}
