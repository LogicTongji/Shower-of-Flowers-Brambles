#include "gui_plugin_manifest.h"

#include <algorithm>
#include <cctype>
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
    std::string& error
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
            error = "plugin_manifest_parse_failed: " + parseError;
            return false;
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
                        error = descriptorError;
                        return false;
                    }
                    continue;
                }

                if (!registry.HasFactory(descriptor.factoryType))
                {
                    error = "plugin_factory_not_registered: "
                        + descriptor.factoryType
                        + ": " + document.path.string();
                    return false;
                }
                if (!registry.Register(std::move(descriptor)))
                {
                    error = "plugin_registration_failed: "
                        + document.path.string();
                    return false;
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
    return true;
}
