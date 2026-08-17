#include "gui_lua_bridge.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

std::string NormalizeChannelName(std::string_view name)
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

std::string NormalizePlayerTag(std::string_view name)
{
    std::string normalized = NormalizeChannelName(name);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::toupper(character));
        }
    );
    return normalized;
}

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

struct GuiLuaBridgeEndpoint
{
    explicit GuiLuaBridgeEndpoint(GuiLuaBridgeConfig initialConfig)
        : config(initialConfig)
    {
        config.maxPendingUpdates = std::max<std::size_t>(
            1,
            config.maxPendingUpdates
        );
        config.maxPendingActions = std::max<std::size_t>(
            1,
            config.maxPendingActions
        );
    }

    GuiLuaBridgeConfig config;
    mutable std::mutex mutex;
    std::deque<GuiDataBridgeUpdate> updates;
    std::deque<GuiActionContext> actions;
    bool consumerOpen = false;
};

class GuiLuaDataBridgeChannel final : public IGuiDataBridgeChannel
{
public:
    GuiLuaDataBridgeChannel(
        std::string name,
        std::shared_ptr<GuiLuaBridgeEndpoint> endpoint
    )
        : name_(std::move(name)),
          endpoint_(std::move(endpoint))
    {
    }

    ~GuiLuaDataBridgeChannel() override
    {
        Close();
    }

    std::string_view Type() const override
    {
        return "lua";
    }

    bool Open(
        const GuiDataProviderInitContext&,
        std::string& error
    ) override
    {
        std::lock_guard<std::mutex> lock(endpoint_->mutex);
        error.clear();
        if (open_)
        {
            return true;
        }
        if (endpoint_->consumerOpen)
        {
            error = "lua_bridge_channel_already_open: " + name_;
            return false;
        }
        endpoint_->consumerOpen = true;
        open_ = true;
        return true;
    }

    void Close() override
    {
        std::lock_guard<std::mutex> lock(endpoint_->mutex);
        if (!open_)
        {
            return;
        }
        endpoint_->consumerOpen = false;
        endpoint_->actions.clear();
        endpoint_->updates.clear();
        open_ = false;
    }

    GuiDataBridgePollResult Poll(
        GuiDataBridgeUpdate& update,
        std::string& error
    ) override
    {
        std::lock_guard<std::mutex> lock(endpoint_->mutex);
        error.clear();
        if (!open_)
        {
            error = "lua_bridge_channel_not_open: " + name_;
            return GuiDataBridgePollResult::Failed;
        }
        if (endpoint_->updates.empty())
        {
            return GuiDataBridgePollResult::Empty;
        }
        update = std::move(endpoint_->updates.front());
        endpoint_->updates.pop_front();
        return GuiDataBridgePollResult::Update;
    }

    GuiDataBridgeSendResult SendAction(
        const GuiActionContext& context,
        std::string& error
    ) override
    {
        std::lock_guard<std::mutex> lock(endpoint_->mutex);
        error.clear();
        if (!open_)
        {
            error = "lua_bridge_channel_not_open: " + name_;
            return GuiDataBridgeSendResult::Failed;
        }
        if (endpoint_->actions.size()
            >= endpoint_->config.maxPendingActions)
        {
            error = "lua_bridge_action_queue_full: " + name_;
            return GuiDataBridgeSendResult::Failed;
        }
        endpoint_->actions.push_back(context);
        return GuiDataBridgeSendResult::Accepted;
    }

private:
    std::string name_;
    std::shared_ptr<GuiLuaBridgeEndpoint> endpoint_;
    bool open_ = false;
};

}

struct GuiLuaBridgeService::Impl
{
    std::shared_ptr<GuiLuaBridgeEndpoint> GetOrCreate(
        const std::string& name,
        GuiLuaBridgeConfig config = {}
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = endpoints.find(name);
        if (found != endpoints.end())
        {
            return found->second;
        }
        auto endpoint = std::make_shared<GuiLuaBridgeEndpoint>(config);
        endpoints.emplace(name, endpoint);
        return endpoint;
    }

    std::shared_ptr<GuiLuaBridgeEndpoint> Find(
        const std::string& name
    ) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = endpoints.find(name);
        return found == endpoints.end() ? nullptr : found->second;
    }

    mutable std::mutex mutex;
    std::unordered_map<
        std::string,
        std::shared_ptr<GuiLuaBridgeEndpoint>
    > endpoints;
    GuiGameplayLifecycleState lifecycleState =
        GuiGameplayLifecycleState::Unknown;
    uint64_t lifecycleGeneration = 0;
    std::string lifecyclePlayerTag;
};

GuiLuaBridgeService::GuiLuaBridgeService()
    : impl_(std::make_unique<Impl>())
{
}

GuiLuaBridgeService::~GuiLuaBridgeService() = default;

bool GuiLuaBridgeService::PublishUpdate(
    std::string_view channelName,
    GuiDataBridgeUpdate update,
    std::string& error
)
{
    const std::string name = NormalizeChannelName(channelName);
    if (name.empty())
    {
        error = "lua_bridge_channel_name_missing";
        return false;
    }
    const std::shared_ptr<GuiLuaBridgeEndpoint> endpoint =
        impl_->GetOrCreate(name);
    std::lock_guard<std::mutex> lock(endpoint->mutex);
    if (endpoint->updates.size()
        >= endpoint->config.maxPendingUpdates)
    {
        error = "lua_bridge_update_queue_full: " + name;
        return false;
    }
    endpoint->updates.push_back(std::move(update));
    error.clear();
    return true;
}

bool GuiLuaBridgeService::TryPopAction(
    std::string_view channelName,
    GuiActionContext& context
)
{
    const std::string name = NormalizeChannelName(channelName);
    const std::shared_ptr<GuiLuaBridgeEndpoint> endpoint =
        impl_->Find(name);
    if (!endpoint)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(endpoint->mutex);
    if (endpoint->actions.empty())
    {
        return false;
    }
    context = std::move(endpoint->actions.front());
    endpoint->actions.pop_front();
    return true;
}

GuiLuaBridgeStats GuiLuaBridgeService::Stats(
    std::string_view channelName
) const
{
    const std::string name = NormalizeChannelName(channelName);
    const std::shared_ptr<GuiLuaBridgeEndpoint> endpoint =
        impl_->Find(name);
    if (!endpoint)
    {
        return {};
    }
    std::lock_guard<std::mutex> lock(endpoint->mutex);
    return {
        endpoint->updates.size(),
        endpoint->actions.size(),
        endpoint->consumerOpen
    };
}

bool GuiLuaBridgeService::ReportGameplayPlayerTag(
    std::string_view playerTag
)
{
    const std::string normalizedTag = NormalizePlayerTag(playerTag);
    const GuiGameplayLifecycleState nextState =
        normalizedTag.empty() || normalizedTag == "---"
            ? GuiGameplayLifecycleState::Frontend
            : GuiGameplayLifecycleState::Gameplay;
    std::vector<std::shared_ptr<GuiLuaBridgeEndpoint>> endpoints;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->lifecycleState == nextState
            && impl_->lifecyclePlayerTag == normalizedTag)
        {
            return false;
        }
        impl_->lifecycleState = nextState;
        impl_->lifecyclePlayerTag = normalizedTag;
        ++impl_->lifecycleGeneration;
        if (nextState == GuiGameplayLifecycleState::Frontend)
        {
            endpoints.reserve(impl_->endpoints.size());
            for (const auto& entry : impl_->endpoints)
            {
                endpoints.push_back(entry.second);
            }
        }
    }
    for (const auto& endpoint : endpoints)
    {
        std::lock_guard<std::mutex> lock(endpoint->mutex);
        endpoint->updates.clear();
        endpoint->actions.clear();
    }
    return true;
}

GuiGameplayLifecycleSnapshot
GuiLuaBridgeService::GameplayLifecycle() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return {
        impl_->lifecycleState,
        impl_->lifecycleGeneration,
        impl_->lifecyclePlayerTag
    };
}

void GuiLuaBridgeService::ResetGameplayLifecycle()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->lifecycleState = GuiGameplayLifecycleState::Unknown;
    impl_->lifecyclePlayerTag.clear();
    ++impl_->lifecycleGeneration;
}

void GuiLuaBridgeService::Reset(std::string_view channelName)
{
    const std::string name = NormalizeChannelName(channelName);
    const std::shared_ptr<GuiLuaBridgeEndpoint> endpoint =
        impl_->Find(name);
    if (!endpoint)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(endpoint->mutex);
    endpoint->updates.clear();
    endpoint->actions.clear();
}

void GuiLuaBridgeService::ResetAll()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->endpoints.clear();
}

std::unique_ptr<IGuiDataBridgeChannel>
GuiLuaBridgeService::CreateChannel(
    std::string channelName,
    GuiLuaBridgeConfig config
)
{
    channelName = NormalizeChannelName(channelName);
    if (channelName.empty())
    {
        return nullptr;
    }
    const std::shared_ptr<GuiLuaBridgeEndpoint> endpoint =
        impl_->GetOrCreate(channelName, config);
    {
        std::lock_guard<std::mutex> lock(endpoint->mutex);
        if (!endpoint->consumerOpen)
        {
            endpoint->config.maxPendingUpdates =
                std::max<std::size_t>(1, config.maxPendingUpdates);
            endpoint->config.maxPendingActions =
                std::max<std::size_t>(1, config.maxPendingActions);
        }
    }
    return std::make_unique<GuiLuaDataBridgeChannel>(
        channelName,
        endpoint
    );
}

GuiLuaBridgeService& GetGuiLuaBridgeService()
{
    static GuiLuaBridgeService service;
    return service;
}

bool RegisterGuiLuaDataBridgeChannel(
    GuiDataBridgeChannelRegistry& registry,
    GuiLuaBridgeService& service
)
{
    return registry.RegisterFactory(
        "lua",
        [&service](const GuiDataProviderCreateContext& context)
            -> std::unique_ptr<IGuiDataBridgeChannel>
        {
            std::string channelName = context.Option("bridge_name");
            if (channelName.empty())
            {
                channelName = context.Option("endpoint");
            }
            if (channelName.empty())
            {
                channelName = "default";
            }

            GuiLuaBridgeConfig config;
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
            return service.CreateChannel(
                std::move(channelName),
                config
            );
        }
    );
}
