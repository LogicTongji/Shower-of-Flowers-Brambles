#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_data_provider.h"
#include "gui_declarative_data.h"

struct GuiSequenceDataProviderConfig
{
    std::filesystem::path path;
    std::filesystem::path basePath;
    uint64_t frameIntervalMilliseconds = 1200;
    bool loop = true;
};

class GuiSequenceDataProvider final : public IGuiDataProvider
{
public:
    explicit GuiSequenceDataProvider(
        GuiSequenceDataProviderConfig config
    );

    std::string_view Type() const override;

    bool Initialize(
        const GuiDataProviderInitContext& context,
        std::string& error
    ) override;

    void Shutdown() override;

    std::shared_ptr<GuiDataRegistry> Registry() const override;

    GuiDataProviderUpdateResult Tick(
        uint64_t nowMilliseconds,
        std::string& error
    ) override;

    GuiDataProviderActionResult HandleAction(
        const GuiActionContext& context,
        std::string& error
    ) override;

private:
    bool CollectFrames(std::string& error);
    bool LoadCurrentFrame(std::string& error);
    void RestorePersistentValues();

    GuiSequenceDataProviderConfig config_;
    GuiDeclarativeDataStore dataStore_;
    std::filesystem::path resolvedPath_;
    std::filesystem::path resolvedBasePath_;
    std::vector<std::filesystem::path> frames_;
    std::unordered_map<std::string, GuiDataValue> persistentValues_;
    std::unordered_map<std::string, GuiListModel> persistentLists_;
    std::size_t currentFrame_ = 0;
    uint64_t nextAdvanceMilliseconds_ = 0;
    bool initialized_ = false;
};

bool RegisterGuiSequenceDataProvider(
    GuiDataProviderRegistry& registry
);
