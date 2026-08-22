#include "gui_file_data_provider.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace
{

std::string Lower(std::string value)
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

bool ParseWatchOption(std::string value)
{
    value = Lower(std::move(value));
    return value != "no"
        && value != "false"
        && value != "off"
        && value != "0";
}

}

GuiFileDataProvider::GuiFileDataProvider(
    GuiFileDataProviderConfig config
)
    : config_(std::move(config))
{
}

std::string_view GuiFileDataProvider::Type() const
{
    return "file";
}

bool GuiFileDataProvider::Initialize(
    const GuiDataProviderInitContext& context,
    std::string& error
)
{
    Shutdown();
    resolvedPath_ = config_.path.empty()
        ? fs::path{}
        : config_.path.is_absolute()
            ? config_.path
            : context.root / config_.path;
    initialized_ = true;
    if (resolvedPath_.empty())
    {
        return true;
    }
    if (!Reload(error))
    {
        initialized_ = false;
        return false;
    }
    return true;
}

void GuiFileDataProvider::Shutdown()
{
    dataStore_.Clear();
    resolvedPath_.clear();
    hasWriteTime_ = false;
    statFailed_ = false;
    initialized_ = false;
}

std::shared_ptr<GuiDataRegistry> GuiFileDataProvider::Registry() const
{
    return dataStore_.Registry();
}

GuiDataProviderUpdateResult GuiFileDataProvider::Tick(
    uint64_t,
    std::string& error
)
{
    error.clear();
    if (!initialized_ || !config_.watch || resolvedPath_.empty())
    {
        return GuiDataProviderUpdateResult::Unchanged;
    }

    std::error_code timeError;
    const fs::file_time_type writeTime = fs::last_write_time(
        resolvedPath_,
        timeError
    );
    if (timeError)
    {
        if (statFailed_)
        {
            return GuiDataProviderUpdateResult::Unchanged;
        }
        statFailed_ = true;
        error = "file_data_provider_stat_failed: "
            + resolvedPath_.string();
        return GuiDataProviderUpdateResult::Failed;
    }

    const bool recoveredFromStatFailure = statFailed_;
    statFailed_ = false;
    if (!recoveredFromStatFailure
        && hasWriteTime_
        && writeTime == lastWriteTime_)
    {
        return GuiDataProviderUpdateResult::Unchanged;
    }

    lastWriteTime_ = writeTime;
    hasWriteTime_ = true;
    if (!dataStore_.LoadFile(resolvedPath_, error))
    {
        return GuiDataProviderUpdateResult::Failed;
    }
    return GuiDataProviderUpdateResult::Changed;
}

GuiDataProviderActionResult GuiFileDataProvider::HandleAction(
    const GuiActionContext& context,
    std::string& error
)
{
    error.clear();
    if (Lower(context.fallbackOperation) == "reload_data")
    {
        if (!Reload(error))
        {
            return GuiDataProviderActionResult::Failed;
        }
        return GuiDataProviderActionResult::Handled;
    }
    return dataStore_.ApplyAction(context)
        ? GuiDataProviderActionResult::Handled
        : GuiDataProviderActionResult::Unhandled;
}

bool GuiFileDataProvider::Reload(std::string& error)
{
    error.clear();
    if (resolvedPath_.empty())
    {
        dataStore_.Clear();
        hasWriteTime_ = false;
        statFailed_ = false;
        return true;
    }
    if (!dataStore_.LoadFile(resolvedPath_, error))
    {
        return false;
    }
    RefreshWriteTime();
    return true;
}

void GuiFileDataProvider::RefreshWriteTime()
{
    std::error_code timeError;
    lastWriteTime_ = fs::last_write_time(resolvedPath_, timeError);
    hasWriteTime_ = !timeError;
    statFailed_ = false;
}

bool RegisterBuiltinGuiDataProviders(
    GuiDataProviderRegistry& registry
)
{
    return registry.RegisterFactory(
        "file",
        [](const GuiDataProviderCreateContext& context)
        {
            GuiFileDataProviderConfig config;
            std::string path = context.Option("path");
            if (path.empty())
            {
                path = context.Option("data_path");
            }
            if (path.empty())
            {
                path = context.Option("data");
            }
            config.path = std::move(path);

            const std::string watch = context.Option("watch");
            if (!watch.empty())
            {
                config.watch = ParseWatchOption(watch);
            }
            return std::make_unique<GuiFileDataProvider>(
                std::move(config)
            );
        }
    );
}
