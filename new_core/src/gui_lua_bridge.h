#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "gui_data_bridge.h"

struct GuiLuaBridgeConfig
{
    std::size_t maxPendingUpdates = 256;
    std::size_t maxPendingActions = 256;
};

struct GuiLuaBridgeStats
{
    std::size_t pendingUpdates = 0;
    std::size_t pendingActions = 0;
    bool consumerOpen = false;
};

enum class GuiGameplayLifecycleState
{
    Unknown,
    Frontend,
    Gameplay
};

struct GuiGameplayLifecycleSnapshot
{
    GuiGameplayLifecycleState state =
        GuiGameplayLifecycleState::Unknown;
    uint64_t generation = 0;
    std::string playerTag;
};

class GuiLuaBridgeService
{
public:
    GuiLuaBridgeService();
    ~GuiLuaBridgeService();

    GuiLuaBridgeService(const GuiLuaBridgeService&) = delete;
    GuiLuaBridgeService& operator=(const GuiLuaBridgeService&) = delete;

    bool PublishUpdate(
        std::string_view channelName,
        GuiDataBridgeUpdate update,
        std::string& error
    );

    bool TryPopAction(
        std::string_view channelName,
        GuiActionContext& context
    );

    GuiLuaBridgeStats Stats(std::string_view channelName) const;

    bool ReportGameplayPlayerTag(std::string_view playerTag);
    GuiGameplayLifecycleSnapshot GameplayLifecycle() const;
    void ResetGameplayLifecycle();

    void Reset(std::string_view channelName);
    void ResetAll();

    std::unique_ptr<IGuiDataBridgeChannel> CreateChannel(
        std::string channelName,
        GuiLuaBridgeConfig config
    );

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

GuiLuaBridgeService& GetGuiLuaBridgeService();

bool RegisterGuiLuaDataBridgeChannel(
    GuiDataBridgeChannelRegistry& registry,
    GuiLuaBridgeService& service
);
