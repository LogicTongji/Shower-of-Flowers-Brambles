#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "gui_data_provider.h"
#include "gui_declarative_data.h"

struct GuiFileDataProviderConfig
{
    std::filesystem::path path;
    bool watch = true;
};

class GuiFileDataProvider final : public IGuiDataProvider
{
public:
    explicit GuiFileDataProvider(GuiFileDataProviderConfig config);

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
    bool Reload(std::string& error);
    void RefreshWriteTime();

    GuiFileDataProviderConfig config_;
    GuiDeclarativeDataStore dataStore_;
    std::filesystem::path resolvedPath_;
    std::filesystem::file_time_type lastWriteTime_{};
    bool hasWriteTime_ = false;
    bool statFailed_ = false;
    bool initialized_ = false;
};

bool RegisterBuiltinGuiDataProviders(
    GuiDataProviderRegistry& registry
);
