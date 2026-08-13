#include "gui_sequence_data_provider.h"

#include <algorithm>
#include <cctype>
#include <limits>
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

bool ParseBoolean(std::string value, bool defaultValue)
{
    if (value.empty())
    {
        return defaultValue;
    }
    value = Lower(std::move(value));
    return value != "no"
        && value != "false"
        && value != "off"
        && value != "0";
}

bool ParseUnsigned(
    const std::string& value,
    uint64_t& output
)
{
    if (value.empty())
    {
        return false;
    }
    try
    {
        std::size_t parsed = 0;
        const unsigned long long number = std::stoull(value, &parsed);
        if (parsed != value.size())
        {
            return false;
        }
        output = static_cast<uint64_t>(number);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::string FindParameter(
    const GuiActionContext& context,
    std::string_view name
)
{
    const auto found = context.parameters.find(
        Lower(std::string(name))
    );
    return found == context.parameters.end()
        ? std::string{}
        : found->second;
}

bool IsDataFile(const fs::path& path)
{
    const std::string extension = Lower(path.extension().string());
    return extension == ".txt"
        || extension == ".data"
        || extension == ".gui";
}

fs::path ResolvePath(
    const fs::path& root,
    const fs::path& path
)
{
    if (path.empty() || path.is_absolute())
    {
        return path;
    }
    return root / path;
}

}

GuiSequenceDataProvider::GuiSequenceDataProvider(
    GuiSequenceDataProviderConfig config
)
    : config_(std::move(config))
{
}

std::string_view GuiSequenceDataProvider::Type() const
{
    return "sequence";
}

bool GuiSequenceDataProvider::Initialize(
    const GuiDataProviderInitContext& context,
    std::string& error
)
{
    Shutdown();
    resolvedPath_ = ResolvePath(context.root, config_.path);
    resolvedBasePath_ = ResolvePath(context.root, config_.basePath);
    if (!CollectFrames(error) || !LoadCurrentFrame(error))
    {
        Shutdown();
        return false;
    }
    initialized_ = true;
    return true;
}

void GuiSequenceDataProvider::Shutdown()
{
    dataStore_.Clear();
    resolvedPath_.clear();
    resolvedBasePath_.clear();
    frames_.clear();
    persistentValues_.clear();
    persistentLists_.clear();
    currentFrame_ = 0;
    nextAdvanceMilliseconds_ = 0;
    initialized_ = false;
}

std::shared_ptr<GuiDataRegistry>
GuiSequenceDataProvider::Registry() const
{
    return dataStore_.Registry();
}

GuiDataProviderUpdateResult GuiSequenceDataProvider::Tick(
    uint64_t nowMilliseconds,
    std::string& error
)
{
    error.clear();
    if (!initialized_
        || frames_.size() <= 1
        || config_.frameIntervalMilliseconds == 0)
    {
        return GuiDataProviderUpdateResult::Unchanged;
    }

    if (nextAdvanceMilliseconds_ == 0)
    {
        nextAdvanceMilliseconds_ = nowMilliseconds
            + config_.frameIntervalMilliseconds;
        return GuiDataProviderUpdateResult::Unchanged;
    }
    if (nowMilliseconds < nextAdvanceMilliseconds_)
    {
        return GuiDataProviderUpdateResult::Unchanged;
    }

    std::size_t nextFrame = currentFrame_ + 1;
    if (nextFrame >= frames_.size())
    {
        if (!config_.loop)
        {
            nextAdvanceMilliseconds_ =
                std::numeric_limits<uint64_t>::max();
            return GuiDataProviderUpdateResult::Unchanged;
        }
        nextFrame = 0;
    }

    const std::size_t previousFrame = currentFrame_;
    currentFrame_ = nextFrame;
    nextAdvanceMilliseconds_ = nowMilliseconds
        + config_.frameIntervalMilliseconds;
    if (!LoadCurrentFrame(error))
    {
        currentFrame_ = previousFrame;
        return GuiDataProviderUpdateResult::Failed;
    }
    return GuiDataProviderUpdateResult::Changed;
}

GuiDataProviderActionResult GuiSequenceDataProvider::HandleAction(
    const GuiActionContext& context,
    std::string& error
)
{
    error.clear();
    if (Lower(context.fallbackOperation) == "reload_data")
    {
        return LoadCurrentFrame(error)
            ? GuiDataProviderActionResult::Handled
            : GuiDataProviderActionResult::Failed;
    }
    if (!dataStore_.ApplyAction(context))
    {
        return GuiDataProviderActionResult::Unhandled;
    }

    if (ParseBoolean(FindParameter(context, "persist"), false))
    {
        const std::string target = FindParameter(context, "target");
        const GuiDataValue* value = dataStore_.Registry()->Find(target);
        if (!target.empty() && value)
        {
            persistentValues_[Lower(target)] = *value;
        }
        const GuiListModel* list = dataStore_.Registry()->FindList(target);
        if (!target.empty() && list)
        {
            persistentLists_[Lower(target)] = *list;
        }
		for (const auto& parameter : context.parameters)
		{
			constexpr std::string_view prefix = "set.";
			if (parameter.first.rfind(prefix, 0) != 0)
			{
				continue;
			}
			const std::string name = parameter.first.substr(prefix.size());
			if (const GuiDataValue* changed =
				dataStore_.Registry()->Find(name))
			{
				persistentValues_[Lower(name)] = *changed;
			}
		}
    }
    return GuiDataProviderActionResult::Handled;
}

bool GuiSequenceDataProvider::CollectFrames(std::string& error)
{
    error.clear();
    frames_.clear();
    std::error_code pathError;
    if (resolvedPath_.empty())
    {
        error = "sequence_data_provider_path_missing";
        return false;
    }
    if (fs::is_regular_file(resolvedPath_, pathError))
    {
        if (!IsDataFile(resolvedPath_))
        {
            error = "sequence_data_provider_file_type_invalid: "
                + resolvedPath_.string();
            return false;
        }
        frames_.push_back(resolvedPath_);
    }
    else if (!pathError && fs::is_directory(resolvedPath_, pathError))
    {
        for (const fs::directory_entry& entry
            : fs::directory_iterator(resolvedPath_))
        {
            if (entry.is_regular_file() && IsDataFile(entry.path()))
            {
                frames_.push_back(entry.path());
            }
        }
        std::sort(frames_.begin(), frames_.end());
    }
    else
    {
        error = "sequence_data_provider_path_not_found: "
            + resolvedPath_.string();
        return false;
    }

    if (pathError)
    {
        error = "sequence_data_provider_path_failed: "
            + resolvedPath_.string();
        return false;
    }
    if (frames_.empty())
    {
        error = "sequence_data_provider_frames_empty: "
            + resolvedPath_.string();
        return false;
    }
    if (!resolvedBasePath_.empty()
        && !fs::is_regular_file(resolvedBasePath_))
    {
        error = "sequence_data_provider_base_not_found: "
            + resolvedBasePath_.string();
        return false;
    }
    return true;
}

bool GuiSequenceDataProvider::LoadCurrentFrame(std::string& error)
{
    std::vector<fs::path> paths;
    if (!resolvedBasePath_.empty())
    {
        paths.push_back(resolvedBasePath_);
    }
    paths.push_back(frames_[currentFrame_]);
    if (!dataStore_.LoadFiles(paths, error))
    {
        return false;
    }
    RestorePersistentValues();
    return true;
}

void GuiSequenceDataProvider::RestorePersistentValues()
{
    const std::shared_ptr<GuiDataRegistry> registry = dataStore_.Registry();
    for (const auto& entry : persistentValues_)
    {
        registry->Set(entry.first, entry.second);
    }
    for (const auto& entry : persistentLists_)
    {
        registry->SetList(entry.first, entry.second);
    }
}

bool RegisterGuiSequenceDataProvider(
    GuiDataProviderRegistry& registry
)
{
    return registry.RegisterFactory(
        "sequence",
        [](const GuiDataProviderCreateContext& context)
        {
            GuiSequenceDataProviderConfig config;
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

            std::string basePath = context.Option("base_path");
            if (basePath.empty())
            {
                basePath = context.Option("base_data");
            }
            if (basePath.empty())
            {
                basePath = context.Option("common_data");
            }
            config.basePath = std::move(basePath);

            std::string interval = context.Option("frame_interval");
            if (interval.empty())
            {
                interval = context.Option("frame_interval_ms");
            }
            if (interval.empty())
            {
                interval = context.Option("advance_interval");
            }
            uint64_t parsedInterval = 0;
            if (ParseUnsigned(interval, parsedInterval))
            {
                config.frameIntervalMilliseconds = parsedInterval;
            }
            config.loop = ParseBoolean(
                context.Option("loop"),
                config.loop
            );
            return std::make_unique<GuiSequenceDataProvider>(
                std::move(config)
            );
        }
    );
}
