#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_plugin.h"

class GuiDataProviderRegistry;

struct GuiPluginCreateContext
{
    std::filesystem::path root;
    std::unordered_map<std::string, std::string> options;
    const GuiDataProviderRegistry* dataProviders = nullptr;

    std::string Option(std::string_view name) const;
};

using GuiPluginFactory = std::function<
    std::unique_ptr<IGuiPlugin>(const GuiPluginCreateContext&)
>;

struct GuiPluginDescriptor
{
    std::string id;
    std::string displayName;
    std::string factoryType;
    std::filesystem::path sourcePath;
    std::unordered_map<std::string, std::string> defaultOptions;
    std::string visibleWhen;
    bool startup = true;
    int windowZOrder = 0;
    bool modal = false;
    double maxViewportWidthRatio = 1.0;
    double maxViewportHeightRatio = 1.0;
    int cascadeOffsetX = 0;
    int cascadeOffsetY = 0;
};

class GuiPluginRegistry
{
public:
    bool RegisterFactory(
        std::string factoryType,
        GuiPluginFactory factory
    );

    bool Register(GuiPluginDescriptor descriptor);

    std::unique_ptr<IGuiPlugin> Create(
        std::string_view id,
        const GuiPluginCreateContext& context
    ) const;

    const GuiPluginDescriptor* Find(std::string_view id) const;
    bool HasFactory(std::string_view factoryType) const;
    bool CanCreate(std::string_view id) const;

    const std::vector<GuiPluginDescriptor>& Descriptors() const;
    std::string_view DefaultPluginId() const;

private:
    std::vector<GuiPluginDescriptor> descriptors_;
    std::unordered_map<std::string, std::size_t> descriptorIndex_;
    std::unordered_map<std::string, GuiPluginFactory> factories_;
};
