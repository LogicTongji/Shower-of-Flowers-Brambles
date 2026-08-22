#pragma once

#include <cstddef>
#include <deque>
#include <mutex>

#include "gui_data_bridge.h"

struct GuiMemoryDataBridgeConfig
{
    std::size_t maxPendingUpdates = 256;
    std::size_t maxPendingActions = 256;
};

class GuiMemoryDataBridgeChannel final
    : public IGuiDataBridgeChannel
{
public:
    explicit GuiMemoryDataBridgeChannel(
        GuiMemoryDataBridgeConfig config = {}
    );

    std::string_view Type() const override;

    bool Open(
        const GuiDataProviderInitContext& context,
        std::string& error
    ) override;

    void Close() override;

    GuiDataBridgePollResult Poll(
        GuiDataBridgeUpdate& update,
        std::string& error
    ) override;

    GuiDataBridgeSendResult SendAction(
        const GuiActionContext& context,
        std::string& error
    ) override;

    bool PublishUpdate(GuiDataBridgeUpdate update);

    bool TryPopAction(GuiActionContext& context);

    std::size_t PendingUpdateCount() const;
    std::size_t PendingActionCount() const;

private:
    GuiMemoryDataBridgeConfig config_;
    mutable std::mutex mutex_;
    std::deque<GuiDataBridgeUpdate> updates_;
    std::deque<GuiActionContext> actions_;
    bool open_ = false;
};

bool RegisterBuiltinGuiDataBridgeChannels(
    GuiDataBridgeChannelRegistry& registry
);
