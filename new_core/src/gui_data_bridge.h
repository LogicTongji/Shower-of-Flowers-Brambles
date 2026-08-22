#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_data_provider.h"

struct GuiDataBridgeUpdate
{
    uint64_t revision = 0;
    uint64_t baseRevision = 0;
    bool fullSnapshot = false;
    std::unordered_map<std::string, GuiDataValue> values;
    std::vector<std::string> removedValues;
    std::unordered_map<std::string, GuiListModel> lists;
    std::vector<std::string> removedLists;
};

enum class GuiDataBridgePollResult
{
    Empty,
    Update,
    Failed
};

enum class GuiDataBridgeSendResult
{
    Accepted,
    Rejected,
    Failed
};

class IGuiDataBridgeChannel
{
public:
    virtual ~IGuiDataBridgeChannel() = default;

    virtual std::string_view Type() const = 0;

    virtual bool Open(
        const GuiDataProviderInitContext& context,
        std::string& error
    ) = 0;

    virtual void Close() = 0;

    virtual GuiDataBridgePollResult Poll(
        GuiDataBridgeUpdate& update,
        std::string& error
    ) = 0;

    virtual GuiDataBridgeSendResult SendAction(
        const GuiActionContext& context,
        std::string& error
    ) = 0;
};

using GuiDataBridgeChannelFactory = std::function<
    std::unique_ptr<IGuiDataBridgeChannel>(
        const GuiDataProviderCreateContext&
    )
>;

class GuiDataBridgeChannelRegistry
{
public:
    bool RegisterFactory(
        std::string type,
        GuiDataBridgeChannelFactory factory
    );

    std::unique_ptr<IGuiDataBridgeChannel> Create(
        std::string_view type,
        const GuiDataProviderCreateContext& context
    ) const;

    bool HasFactory(std::string_view type) const;

private:
    std::unordered_map<
        std::string,
        GuiDataBridgeChannelFactory
    > factories_;
};

struct GuiBridgeDataProviderConfig
{
    std::unique_ptr<IGuiDataBridgeChannel> channel;
	std::filesystem::path baseDataPath;
    std::size_t maxUpdatesPerTick = 64;
};

class GuiBridgeDataProvider final : public IGuiDataProvider
{
public:
    explicit GuiBridgeDataProvider(
        GuiBridgeDataProviderConfig config
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

    uint64_t AppliedRevision() const;

private:
    GuiDataProviderUpdateResult DrainUpdates(std::string& error);

    GuiBridgeDataProviderConfig config_;
    std::shared_ptr<GuiDataRegistry> registry_;
	std::shared_ptr<GuiDataRegistry> baseRegistry_;
    uint64_t appliedRevision_ = 0;
    bool hasSnapshot_ = false;
    bool initialized_ = false;
};

bool RegisterGuiBridgeDataProvider(
    GuiDataProviderRegistry& providers,
    const GuiDataBridgeChannelRegistry& channels
);
