#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_host_macos.h"

class GuiDataProviderRegistry;

struct GuiMacPluginCreateContext
{
    std::filesystem::path root;
    std::unordered_map<std::string, std::string> options;
    const GuiDataProviderRegistry* dataProviders = nullptr;

    std::string Option(std::string_view name) const;
};

using GuiMacPluginFactory = std::function<
    std::unique_ptr<IGuiMacPlugin>(const GuiMacPluginCreateContext&)
>;

struct GuiMacPluginDescriptor
{
    std::string id;
    std::string displayName;
    std::string factoryType;
    std::filesystem::path sourcePath;
    std::unordered_map<std::string, std::string> defaultOptions;
    std::string visibleWhen;
    bool startup = true;
};

class GuiMacPluginRegistry
{
public:
    bool RegisterFactory(
        std::string factoryType,
        GuiMacPluginFactory factory
    );

    bool Register(GuiMacPluginDescriptor descriptor);

    std::unique_ptr<IGuiMacPlugin> Create(
        std::string_view id,
        const GuiMacPluginCreateContext& context
    ) const;

    const GuiMacPluginDescriptor* Find(std::string_view id) const;
    bool HasFactory(std::string_view factoryType) const;
    bool CanCreate(std::string_view id) const;

    const std::vector<GuiMacPluginDescriptor>& Descriptors() const;
    std::string_view DefaultPluginId() const;

private:
    std::vector<GuiMacPluginDescriptor> descriptors_;
    std::unordered_map<std::string, std::size_t> descriptorIndex_;
    std::unordered_map<std::string, GuiMacPluginFactory> factories_;
};
