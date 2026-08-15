#include "gui_plugin_registry.h"

#include <algorithm>
#include <cctype>
#include <utility>

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

}

std::string GuiPluginCreateContext::Option(std::string_view name) const
{
    const auto found = options.find(NormalizeName(name));
    if (found == options.end())
    {
        return {};
    }
    return found->second;
}

bool GuiPluginRegistry::Register(GuiPluginDescriptor descriptor)
{
    descriptor.id = NormalizeName(descriptor.id);
    descriptor.factoryType = NormalizeName(descriptor.factoryType);
    if (descriptor.id.empty()
        || descriptor.factoryType.empty()
        || !HasFactory(descriptor.factoryType))
    {
        return false;
    }

    if (descriptorIndex_.find(descriptor.id) != descriptorIndex_.end())
    {
        return false;
    }

    if (descriptor.displayName.empty())
    {
        descriptor.displayName = descriptor.id;
    }

    descriptorIndex_[descriptor.id] = descriptors_.size();
    descriptors_.push_back(std::move(descriptor));
    return true;
}

bool GuiPluginRegistry::RegisterFactory(
    std::string factoryType,
    GuiPluginFactory factory
)
{
    factoryType = NormalizeName(factoryType);
    if (factoryType.empty()
        || !factory
        || factories_.find(factoryType) != factories_.end())
    {
        return false;
    }

    factories_.emplace(
        std::move(factoryType),
        std::move(factory)
    );
    return true;
}

std::unique_ptr<IGuiPlugin> GuiPluginRegistry::Create(
    std::string_view id,
    const GuiPluginCreateContext& context
) const
{
    const GuiPluginDescriptor* descriptor = Find(id);
    if (!descriptor)
    {
        return nullptr;
    }

    const auto factory = factories_.find(descriptor->factoryType);
    if (factory == factories_.end())
    {
        return nullptr;
    }

    GuiPluginCreateContext mergedContext = context;
    std::unordered_map<std::string, std::string> runtimeOptions;
    for (const auto& option : context.options)
    {
        runtimeOptions[NormalizeName(option.first)] = option.second;
    }
    mergedContext.options.clear();
    for (const auto& option : descriptor->defaultOptions)
    {
        mergedContext.options[NormalizeName(option.first)] = option.second;
    }
    for (auto& option : runtimeOptions)
    {
        mergedContext.options[std::move(option.first)] =
            std::move(option.second);
    }
    return factory->second(mergedContext);
}

const GuiPluginDescriptor* GuiPluginRegistry::Find(
    std::string_view id
) const
{
    const auto found = descriptorIndex_.find(NormalizeName(id));
    if (found == descriptorIndex_.end())
    {
        return nullptr;
    }
    return &descriptors_[found->second];
}

bool GuiPluginRegistry::HasFactory(
    std::string_view factoryType
) const
{
    return factories_.find(NormalizeName(factoryType))
        != factories_.end();
}

bool GuiPluginRegistry::CanCreate(std::string_view id) const
{
    const GuiPluginDescriptor* descriptor = Find(id);
    return descriptor && HasFactory(descriptor->factoryType);
}

const std::vector<GuiPluginDescriptor>&
GuiPluginRegistry::Descriptors() const
{
    return descriptors_;
}

std::string_view GuiPluginRegistry::DefaultPluginId() const
{
    if (descriptors_.empty())
    {
        return {};
    }
    return descriptors_.front().id;
}
