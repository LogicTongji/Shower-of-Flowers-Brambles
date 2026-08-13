#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "gui_action_bridge.h"
#include "gui_data.h"

struct GuiDataProviderInitContext
{
    const std::filesystem::path& root;
};

struct GuiDataProviderCreateContext
{
    std::unordered_map<std::string, std::string> options;

    std::string Option(std::string_view name) const;
};

enum class GuiDataProviderUpdateResult
{
    Unchanged,
    Changed,
    Failed
};

enum class GuiDataProviderActionResult
{
    Unhandled,
    Handled,
    Failed
};

class IGuiDataProvider
{
public:
    virtual ~IGuiDataProvider() = default;

    virtual std::string_view Type() const = 0;

    virtual bool Initialize(
        const GuiDataProviderInitContext& context,
        std::string& error
    ) = 0;

    virtual void Shutdown() = 0;

    virtual std::shared_ptr<GuiDataRegistry> Registry() const = 0;

    virtual GuiDataProviderUpdateResult Tick(
        uint64_t nowMilliseconds,
        std::string& error
    ) = 0;

    virtual GuiDataProviderActionResult HandleAction(
        const GuiActionContext& context,
        std::string& error
    ) = 0;
};

using GuiDataProviderFactory = std::function<
    std::unique_ptr<IGuiDataProvider>(
        const GuiDataProviderCreateContext&
    )
>;

class GuiDataProviderRegistry
{
public:
    bool RegisterFactory(
        std::string type,
        GuiDataProviderFactory factory
    );

    std::unique_ptr<IGuiDataProvider> Create(
        std::string_view type,
        const GuiDataProviderCreateContext& context
    ) const;

    bool HasFactory(std::string_view type) const;

private:
    std::unordered_map<std::string, GuiDataProviderFactory> factories_;
};
