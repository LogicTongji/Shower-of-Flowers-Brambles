#include "gui_data_bridge.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>
#include <utility>

#include "gui_declarative_data.h"

namespace
{

std::string NormalizeName(std::string_view name)
{
    const auto begin = std::find_if_not(
        name.begin(),
        name.end(),
        [](unsigned char character)
        {
            return std::isspace(character) != 0;
        }
    );
    const auto end = std::find_if_not(
        name.rbegin(),
        name.rend(),
        [](unsigned char character)
        {
            return std::isspace(character) != 0;
        }
    ).base();
    if (begin >= end)
    {
        return {};
    }

    std::string normalized(begin, end);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return normalized;
}

bool ValidateValue(const GuiDataValue& value)
{
    const double* number = std::get_if<double>(&value);
    return !number || std::isfinite(*number);
}

bool ValidateList(const GuiListModel& list)
{
    std::unordered_set<uint64_t> ids;
    ids.reserve(list.items.size());
    for (const GuiListItem& item : list.items)
    {
        if (item.id == 0 || !ids.insert(item.id).second)
        {
            return false;
        }
    }
    return true;
}

bool ValidateUpdate(
    const GuiDataBridgeUpdate& update,
    std::string& error
)
{
    for (const auto& entry : update.values)
    {
        if (NormalizeName(entry.first).empty()
            || !ValidateValue(entry.second))
        {
            error = "bridge_update_value_invalid: " + entry.first;
            return false;
        }
    }
    for (const std::string& name : update.removedValues)
    {
        if (NormalizeName(name).empty())
        {
            error = "bridge_update_removed_value_invalid";
            return false;
        }
    }
    for (const auto& entry : update.lists)
    {
        if (NormalizeName(entry.first).empty()
            || !ValidateList(entry.second))
        {
            error = "bridge_update_list_invalid: " + entry.first;
            return false;
        }
    }
    for (const std::string& name : update.removedLists)
    {
        if (NormalizeName(name).empty())
        {
            error = "bridge_update_removed_list_invalid";
            return false;
        }
    }
    return true;
}

enum class ApplyResult
{
    Ignored,
    Changed,
    Failed
};

ApplyResult ApplyUpdate(
    const GuiDataBridgeUpdate& update,
    std::shared_ptr<GuiDataRegistry>& registry,
	const GuiDataRegistry* baseRegistry,
    uint64_t& appliedRevision,
    bool& hasSnapshot,
    std::string& error
)
{
    if (update.revision == 0)
    {
        error = "bridge_update_revision_missing";
        return ApplyResult::Failed;
    }
    if (hasSnapshot && update.revision <= appliedRevision)
    {
        return ApplyResult::Ignored;
    }
    if (update.fullSnapshot)
    {
        if (update.baseRevision != 0)
        {
            error = "bridge_snapshot_base_revision_must_be_zero";
            return ApplyResult::Failed;
        }
    }
    else
    {
        if (!hasSnapshot)
        {
            error = "bridge_delta_requires_snapshot";
            return ApplyResult::Failed;
        }
        if (update.baseRevision != appliedRevision
            || update.revision <= update.baseRevision)
        {
            error = "bridge_revision_gap: expected_base="
                + std::to_string(appliedRevision)
                + " actual_base="
                + std::to_string(update.baseRevision);
            return ApplyResult::Failed;
        }
    }
    if (!ValidateUpdate(update, error))
    {
        return ApplyResult::Failed;
    }

	auto nextRegistry = update.fullSnapshot
		? baseRegistry
			? std::make_shared<GuiDataRegistry>(*baseRegistry)
			: std::make_shared<GuiDataRegistry>()
        : std::make_shared<GuiDataRegistry>(*registry);
    for (const std::string& name : update.removedValues)
    {
        nextRegistry->Remove(name);
    }
    for (const auto& entry : update.values)
    {
        nextRegistry->Set(entry.first, entry.second);
    }
    for (const std::string& name : update.removedLists)
    {
        nextRegistry->RemoveList(name);
    }
    for (const auto& entry : update.lists)
    {
        nextRegistry->SetList(entry.first, entry.second);
    }

    registry = std::move(nextRegistry);
    appliedRevision = update.revision;
    hasSnapshot = true;
    return ApplyResult::Changed;
}

}

bool GuiDataBridgeChannelRegistry::RegisterFactory(
    std::string type,
    GuiDataBridgeChannelFactory factory
)
{
    type = NormalizeName(type);
    if (type.empty()
        || !factory
        || factories_.find(type) != factories_.end())
    {
        return false;
    }
    factories_.emplace(std::move(type), std::move(factory));
    return true;
}

std::unique_ptr<IGuiDataBridgeChannel>
GuiDataBridgeChannelRegistry::Create(
    std::string_view type,
    const GuiDataProviderCreateContext& context
) const
{
    const auto found = factories_.find(NormalizeName(type));
    if (found == factories_.end())
    {
        return nullptr;
    }

    GuiDataProviderCreateContext normalizedContext;
    for (const auto& option : context.options)
    {
        normalizedContext.options[NormalizeName(option.first)] =
            option.second;
    }
    return found->second(normalizedContext);
}

bool GuiDataBridgeChannelRegistry::HasFactory(
    std::string_view type
) const
{
    return factories_.find(NormalizeName(type)) != factories_.end();
}

GuiBridgeDataProvider::GuiBridgeDataProvider(
    GuiBridgeDataProviderConfig config
)
    : config_(std::move(config)),
      registry_(std::make_shared<GuiDataRegistry>())
{
    config_.maxUpdatesPerTick = std::max<std::size_t>(
        1,
        config_.maxUpdatesPerTick
    );
}

std::string_view GuiBridgeDataProvider::Type() const
{
    return "bridge";
}

bool GuiBridgeDataProvider::Initialize(
    const GuiDataProviderInitContext& context,
    std::string& error
)
{
    Shutdown();
    if (!config_.channel)
    {
        error = "bridge_channel_missing";
        return false;
    }
	if (!config_.baseDataPath.empty())
	{
		const std::filesystem::path basePath =
			config_.baseDataPath.is_absolute()
				? config_.baseDataPath
				: context.root / config_.baseDataPath;
		GuiDeclarativeDataStore baseData;
		if (!baseData.LoadFile(basePath, error))
		{
			return false;
		}
		baseRegistry_ = std::make_shared<GuiDataRegistry>(
			*baseData.Registry()
		);
		registry_ = std::make_shared<GuiDataRegistry>(*baseRegistry_);
	}
    if (!config_.channel->Open(context, error))
    {
        config_.channel->Close();
        return false;
    }
    initialized_ = true;
    if (DrainUpdates(error) == GuiDataProviderUpdateResult::Failed)
    {
        Shutdown();
        return false;
    }
    return true;
}

void GuiBridgeDataProvider::Shutdown()
{
    if (config_.channel)
    {
        config_.channel->Close();
    }
    registry_ = std::make_shared<GuiDataRegistry>();
	baseRegistry_.reset();
    appliedRevision_ = 0;
    hasSnapshot_ = false;
    initialized_ = false;
}

std::shared_ptr<GuiDataRegistry>
GuiBridgeDataProvider::Registry() const
{
    return registry_;
}

GuiDataProviderUpdateResult GuiBridgeDataProvider::Tick(
    uint64_t,
    std::string& error
)
{
    if (!initialized_)
    {
        error = "bridge_provider_not_initialized";
        return GuiDataProviderUpdateResult::Failed;
    }
    return DrainUpdates(error);
}

GuiDataProviderActionResult GuiBridgeDataProvider::HandleAction(
    const GuiActionContext& context,
    std::string& error
)
{
    if (!initialized_ || !config_.channel)
    {
        error = "bridge_provider_not_initialized";
        return GuiDataProviderActionResult::Failed;
    }

    const GuiDataBridgeSendResult result =
        config_.channel->SendAction(context, error);
    if (result == GuiDataBridgeSendResult::Accepted)
    {
        return GuiDataProviderActionResult::Handled;
    }
    if (result == GuiDataBridgeSendResult::Rejected)
    {
        return GuiDataProviderActionResult::Unhandled;
    }
    return GuiDataProviderActionResult::Failed;
}

uint64_t GuiBridgeDataProvider::AppliedRevision() const
{
    return appliedRevision_;
}

GuiDataProviderUpdateResult GuiBridgeDataProvider::DrainUpdates(
    std::string& error
)
{
    error.clear();
    std::shared_ptr<GuiDataRegistry> nextRegistry = registry_;
    uint64_t nextRevision = appliedRevision_;
    bool nextHasSnapshot = hasSnapshot_;
    bool changed = false;

    for (std::size_t index = 0;
        index < config_.maxUpdatesPerTick;
        ++index)
    {
        GuiDataBridgeUpdate update;
        const GuiDataBridgePollResult pollResult =
            config_.channel->Poll(update, error);
        if (pollResult == GuiDataBridgePollResult::Failed)
        {
            return GuiDataProviderUpdateResult::Failed;
        }
        if (pollResult == GuiDataBridgePollResult::Empty)
        {
            break;
        }

        const ApplyResult applyResult = ApplyUpdate(
            update,
            nextRegistry,
			baseRegistry_.get(),
            nextRevision,
            nextHasSnapshot,
            error
        );
        if (applyResult == ApplyResult::Failed)
        {
            return GuiDataProviderUpdateResult::Failed;
        }
        changed = applyResult == ApplyResult::Changed || changed;
    }

    if (!changed)
    {
        return GuiDataProviderUpdateResult::Unchanged;
    }
    registry_ = std::move(nextRegistry);
    appliedRevision_ = nextRevision;
    hasSnapshot_ = nextHasSnapshot;
    return GuiDataProviderUpdateResult::Changed;
}

bool RegisterGuiBridgeDataProvider(
    GuiDataProviderRegistry& providers,
    const GuiDataBridgeChannelRegistry& channels
)
{
    return providers.RegisterFactory(
        "bridge",
        [&channels](const GuiDataProviderCreateContext& context)
            -> std::unique_ptr<IGuiDataProvider>
        {
            std::string channelType = context.Option("channel");
            if (channelType.empty())
            {
                channelType = "memory";
            }

            GuiBridgeDataProviderConfig config;
			std::string baseData = context.Option("base_data");
			if (baseData.empty())
			{
				baseData = context.Option("base_path");
			}
			config.baseDataPath = std::move(baseData);
            config.channel = channels.Create(channelType, context);
            if (!config.channel)
            {
                return std::unique_ptr<IGuiDataProvider>{};
            }

            std::string maxUpdates = context.Option(
                "max_updates_per_tick"
            );
            if (maxUpdates.empty())
            {
                maxUpdates = context.Option("max_updates");
            }
            if (!maxUpdates.empty())
            {
                try
                {
                    config.maxUpdatesPerTick = static_cast<std::size_t>(
                        std::stoull(maxUpdates)
                    );
                }
                catch (...)
                {
                    return std::unique_ptr<IGuiDataProvider>{};
                }
                if (config.maxUpdatesPerTick == 0)
                {
                    return std::unique_ptr<IGuiDataProvider>{};
                }
            }
            return std::make_unique<GuiBridgeDataProvider>(
                std::move(config)
            );
        }
    );
}
