#include "gui_data_provider.h"

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

std::string GuiDataProviderCreateContext::Option(
    std::string_view name
) const
{
    const auto found = options.find(NormalizeName(name));
    return found == options.end() ? std::string{} : found->second;
}

bool GuiDataProviderRegistry::RegisterFactory(
    std::string type,
    GuiDataProviderFactory factory
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

std::unique_ptr<IGuiDataProvider> GuiDataProviderRegistry::Create(
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

bool GuiDataProviderRegistry::HasFactory(std::string_view type) const
{
    return factories_.find(NormalizeName(type)) != factories_.end();
}
