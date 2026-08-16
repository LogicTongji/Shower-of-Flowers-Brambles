#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gui_action_bridge.h"
#include "gui_data.h"

class GuiDeclarativeDataStore
{
public:
    GuiDeclarativeDataStore();

    bool LoadFile(
        const std::filesystem::path& path,
        std::string& error
    );

    bool LoadFiles(
        const std::vector<std::filesystem::path>& paths,
        std::string& error
    );

    void Clear();

    std::shared_ptr<GuiDataRegistry> Registry() const;
    void SetRegistry(std::shared_ptr<GuiDataRegistry> registry);

    bool ApplyAction(const GuiActionContext& context);

    bool SetFromText(
        std::string_view name,
        std::string_view value,
        std::string_view type = {}
    );

private:
    std::shared_ptr<GuiDataRegistry> registry_;
};
