#include "gui_memory_data_bridge.h"

#include <algorithm>
#include <utility>

namespace
{

bool ParseCapacity(
    const GuiDataProviderCreateContext& context,
    std::string_view name,
    std::size_t& value
)
{
    const std::string text = context.Option(name);
    if (text.empty())
    {
        return true;
    }
    try
    {
        value = static_cast<std::size_t>(std::stoull(text));
    }
    catch (...)
    {
        return false;
    }
    return value > 0;
}

}

GuiMemoryDataBridgeChannel::GuiMemoryDataBridgeChannel(
    GuiMemoryDataBridgeConfig config
)
    : config_(config)
{
    config_.maxPendingUpdates = std::max<std::size_t>(
        1,
        config_.maxPendingUpdates
    );
    config_.maxPendingActions = std::max<std::size_t>(
        1,
        config_.maxPendingActions
    );
}

std::string_view GuiMemoryDataBridgeChannel::Type() const
{
    return "memory";
}

bool GuiMemoryDataBridgeChannel::Open(
    const GuiDataProviderInitContext&,
    std::string& error
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    error.clear();
    open_ = true;
    return true;
}

void GuiMemoryDataBridgeChannel::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    open_ = false;
    updates_.clear();
    actions_.clear();
}

GuiDataBridgePollResult GuiMemoryDataBridgeChannel::Poll(
    GuiDataBridgeUpdate& update,
    std::string& error
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    error.clear();
    if (!open_)
    {
        error = "memory_bridge_channel_not_open";
        return GuiDataBridgePollResult::Failed;
    }
    if (updates_.empty())
    {
        return GuiDataBridgePollResult::Empty;
    }
    update = std::move(updates_.front());
    updates_.pop_front();
    return GuiDataBridgePollResult::Update;
}

GuiDataBridgeSendResult GuiMemoryDataBridgeChannel::SendAction(
    const GuiActionContext& context,
    std::string& error
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    error.clear();
    if (!open_)
    {
        error = "memory_bridge_channel_not_open";
        return GuiDataBridgeSendResult::Failed;
    }
    if (actions_.size() >= config_.maxPendingActions)
    {
        error = "memory_bridge_action_queue_full";
        return GuiDataBridgeSendResult::Failed;
    }
    actions_.push_back(context);
    return GuiDataBridgeSendResult::Accepted;
}

bool GuiMemoryDataBridgeChannel::PublishUpdate(
    GuiDataBridgeUpdate update
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_ || updates_.size() >= config_.maxPendingUpdates)
    {
        return false;
    }
    updates_.push_back(std::move(update));
    return true;
}

bool GuiMemoryDataBridgeChannel::TryPopAction(
    GuiActionContext& context
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (actions_.empty())
    {
        return false;
    }
    context = std::move(actions_.front());
    actions_.pop_front();
    return true;
}

std::size_t GuiMemoryDataBridgeChannel::PendingUpdateCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return updates_.size();
}

std::size_t GuiMemoryDataBridgeChannel::PendingActionCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actions_.size();
}

bool RegisterBuiltinGuiDataBridgeChannels(
    GuiDataBridgeChannelRegistry& registry
)
{
    return registry.RegisterFactory(
        "memory",
        [](const GuiDataProviderCreateContext& context)
            -> std::unique_ptr<IGuiDataBridgeChannel>
        {
            GuiMemoryDataBridgeConfig config;
            if (!ParseCapacity(
                    context,
                    "max_pending_updates",
                    config.maxPendingUpdates
                )
                || !ParseCapacity(
                    context,
                    "max_pending_actions",
                    config.maxPendingActions
                ))
            {
                return nullptr;
            }
            return std::make_unique<GuiMemoryDataBridgeChannel>(config);
        }
    );
}
