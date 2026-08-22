#include "gui_plugin_manifest.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "gui_interpreter.h"
#include "gui_plugin_registry.h"

namespace
{

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

bool EqualsIgnoreCase(
    std::string_view first,
    std::string_view second
)
{
    if (first.size() != second.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(first[index]))
            != std::tolower(static_cast<unsigned char>(second[index])))
        {
            return false;
        }
    }
    return true;
}

const gui::GuiValue* FindValue(
    const gui::GuiObject& object,
    std::string_view name
)
{
    for (const gui::GuiField& field : object.fields)
    {
        if (EqualsIgnoreCase(field.name, name))
        {
            return &field.value;
        }
    }
    return nullptr;
}

std::string FindScalar(
    const gui::GuiObject& object,
    std::string_view name
)
{
    const gui::GuiValue* value = FindValue(object, name);
    return value && value->kind == gui::ValueKind::Scalar
        ? value->scalar
        : std::string{};
}

const gui::GuiObject* FindBlock(
    const gui::GuiObject& object,
    std::string_view name
)
{
    const gui::GuiValue* value = FindValue(object, name);
    return value
        && value->kind == gui::ValueKind::Block
        && value->block
        ? value->block.get()
        : nullptr;
}

bool IsFalse(std::string value)
{
    value = Lower(std::move(value));
    return value == "no"
        || value == "false"
        || value == "0";
}

bool IsTrue(std::string value)
{
    return !value.empty() && !IsFalse(std::move(value));
}

int ParseInteger(std::string_view value, int fallback = 0)
{
    if (value.empty())
    {
        return fallback;
    }
    std::string text(value);
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0')
    {
        return fallback;
    }
    return static_cast<int>(std::clamp(
        parsed,
        static_cast<long>(std::numeric_limits<int>::min()),
        static_cast<long>(std::numeric_limits<int>::max())
    ));
}

double ParsePositiveDouble(
    std::string_view value,
    double fallback = 1.0
)
{
    if (value.empty())
    {
        return fallback;
    }
    std::string text(value);
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    return end != text.c_str()
            && *end == '\0'
            && std::isfinite(parsed)
            && parsed > 0.0
        ? parsed
        : fallback;
}

void CollectPluginBlocks(
    const gui::GuiObject& object,
    std::vector<const gui::GuiObject*>& output
)
{
    for (const gui::GuiField& field : object.fields)
    {
        if (field.value.kind != gui::ValueKind::Block
            || !field.value.block)
        {
            continue;
        }

        if (EqualsIgnoreCase(field.name, "guiPlugin"))
        {
            output.push_back(field.value.block.get());
        }
        else
        {
            CollectPluginBlocks(*field.value.block, output);
        }
    }
}

void CopyOption(
    const gui::GuiObject& object,
    std::string_view fieldName,
    std::string_view optionName,
    GuiPluginDescriptor& descriptor
)
{
    const std::string value = FindScalar(object, fieldName);
    if (!value.empty())
    {
        descriptor.defaultOptions[std::string(optionName)] = value;
    }
}

bool BuildDescriptor(
    const gui::GuiObject& object,
    const std::filesystem::path& sourcePath,
    GuiPluginDescriptor& descriptor,
    std::string& error
)
{
    if (IsFalse(FindScalar(object, "enabled")))
    {
        return false;
    }

    descriptor.id = FindScalar(object, "id");
    descriptor.displayName = FindScalar(object, "displayName");
    descriptor.factoryType = FindScalar(object, "factory");
    if (descriptor.factoryType.empty())
    {
        descriptor.factoryType = FindScalar(object, "type");
    }
    descriptor.sourcePath = sourcePath;
    descriptor.visibleWhen = FindScalar(object, "visibleWhen");
    const std::string startup = FindScalar(object, "startup");
    descriptor.startup = startup.empty() || !IsFalse(startup);
    std::string windowZOrder = FindScalar(object, "windowZOrder");
    if (windowZOrder.empty())
    {
        windowZOrder = FindScalar(object, "zOrder");
    }
    descriptor.windowZOrder = ParseInteger(windowZOrder);
    descriptor.modal = IsTrue(FindScalar(object, "modal"));
    descriptor.maxViewportWidthRatio = ParsePositiveDouble(
        FindScalar(object, "maxViewportWidthRatio")
    );
    descriptor.maxViewportHeightRatio = ParsePositiveDouble(
        FindScalar(object, "maxViewportHeightRatio")
    );
    descriptor.cascadeOffsetX = ParseInteger(
        FindScalar(object, "cascadeOffsetX")
    );
    descriptor.cascadeOffsetY = ParseInteger(
        FindScalar(object, "cascadeOffsetY")
    );

    if (descriptor.id.empty())
    {
        error = "plugin_id_missing: " + sourcePath.string();
        return false;
    }
    if (descriptor.factoryType.empty())
    {
        error = "plugin_factory_missing: "
            + descriptor.id + ": " + sourcePath.string();
        return false;
    }

    const gui::GuiObject* options = FindBlock(object, "options");
    if (options)
    {
        for (const gui::GuiField& field : options->fields)
        {
            if (!field.name.empty()
                && field.value.kind == gui::ValueKind::Scalar)
            {
                descriptor.defaultOptions[Lower(field.name)] =
                    field.value.scalar;
            }
        }
    }

    CopyOption(object, "window", "window", descriptor);
    CopyOption(object, "title", "title", descriptor);
    CopyOption(object, "data", "data", descriptor);
    CopyOption(object, "dataPath", "data_path", descriptor);
    CopyOption(object, "dataProvider", "data_provider", descriptor);
    CopyOption(object, "provider", "provider", descriptor);
    CopyOption(object, "channel", "channel", descriptor);
    CopyOption(
        object,
        "maxUpdatesPerTick",
        "max_updates_per_tick",
        descriptor
    );
    CopyOption(object, "tickInterval", "tick_interval", descriptor);
    return true;
}

}

bool LoadGuiPluginManifestDirectory(
    const std::filesystem::path& root,
    GuiPluginRegistry& registry,
    std::size_t& loadedCount,
    std::string& error,
    std::vector<std::string>* diagnostics
)
{
    loadedCount = 0;
    if (!std::filesystem::is_directory(root))
    {
        error = "plugin_manifest_directory_not_found: "
            + root.string();
        return false;
    }

    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry
        : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const std::string extension = Lower(
            entry.path().extension().string()
        );
        if (extension == ".txt" || extension == ".plugin")
        {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    for (const std::filesystem::path& path : paths)
    {
        gui::GuiInterpreter parser;
        std::string parseError;
        if (!parser.LoadFile(path, parseError))
        {
            const std::string issue =
                "plugin_manifest_parse_failed: " + parseError;
            if (diagnostics)
            {
                diagnostics->push_back(issue);
            }
            continue;
        }

        for (const gui::GuiDocument& document : parser.Documents())
        {
            std::vector<const gui::GuiObject*> blocks;
            CollectPluginBlocks(document.root, blocks);
            for (const gui::GuiObject* block : blocks)
            {
                GuiPluginDescriptor descriptor;
                std::string descriptorError;
                if (!BuildDescriptor(
                        *block,
                        document.path,
                        descriptor,
                        descriptorError
                    ))
                {
                    if (!descriptorError.empty())
                    {
                        if (diagnostics)
                        {
                            diagnostics->push_back(descriptorError);
                        }
                    }
                    continue;
                }

                if (!registry.HasFactory(descriptor.factoryType))
                {
                    const std::string issue =
                        "plugin_factory_not_registered: "
                        + descriptor.factoryType
                        + ": " + document.path.string();
                    if (diagnostics)
                    {
                        diagnostics->push_back(issue);
                    }
                    continue;
                }
                if (!registry.Register(std::move(descriptor)))
                {
                    const std::string issue =
                        "plugin_registration_failed: "
                        + document.path.string();
                    if (diagnostics)
                    {
                        diagnostics->push_back(issue);
                    }
                    continue;
                }
                ++loadedCount;
            }
        }
    }

    if (loadedCount == 0)
    {
        error = "no_plugin_manifests_found: " + root.string();
        return false;
    }
    error.clear();
    return true;
}
