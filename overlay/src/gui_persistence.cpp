#include "gui_persistence.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

constexpr std::array<char, 8> Magic{
    'S', 'G', 'U', 'I', 'S', 'T', 'A', 'T'
};
constexpr uint32_t FormatVersion = 1;
constexpr uint32_t MaximumCollectionSize = 100000;
constexpr uint32_t MaximumStringSize = 4 * 1024 * 1024;

enum class ValueType : uint8_t
{
    Empty = 0,
    Boolean = 1,
    Integer = 2,
    Number = 3,
    Text = 4
};

template <typename Value>
bool WriteScalar(std::ostream& output, const Value& value)
{
    static_assert(std::is_trivially_copyable_v<Value>);
    output.write(
        reinterpret_cast<const char*>(&value),
        sizeof(Value)
    );
    return output.good();
}

template <typename Value>
bool ReadScalar(std::istream& input, Value& value)
{
    static_assert(std::is_trivially_copyable_v<Value>);
    input.read(reinterpret_cast<char*>(&value), sizeof(Value));
    return input.good();
}

bool WriteString(std::ostream& output, std::string_view value)
{
    if (value.size() > MaximumStringSize)
    {
        return false;
    }
    const uint32_t size = static_cast<uint32_t>(value.size());
    if (!WriteScalar(output, size))
    {
        return false;
    }
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    return output.good();
}

bool ReadString(std::istream& input, std::string& value)
{
    uint32_t size = 0;
    if (!ReadScalar(input, size) || size > MaximumStringSize)
    {
        return false;
    }
    value.resize(size);
    if (size != 0)
    {
        input.read(value.data(), static_cast<std::streamsize>(size));
    }
    return input.good();
}

bool WriteValue(std::ostream& output, const GuiDataValue& value)
{
    const ValueType type = std::visit(
        [](const auto& current) -> ValueType
        {
            using Current = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<Current, std::monostate>)
            {
                return ValueType::Empty;
            }
            else if constexpr (std::is_same_v<Current, bool>)
            {
                return ValueType::Boolean;
            }
            else if constexpr (std::is_same_v<Current, int64_t>)
            {
                return ValueType::Integer;
            }
            else if constexpr (std::is_same_v<Current, double>)
            {
                return ValueType::Number;
            }
            else
            {
                return ValueType::Text;
            }
        },
        value
    );
    if (!WriteScalar(output, type))
    {
        return false;
    }
    switch (type)
    {
    case ValueType::Empty:
        return true;
    case ValueType::Boolean:
    {
        const uint8_t boolean = std::get<bool>(value) ? 1 : 0;
        return WriteScalar(output, boolean);
    }
    case ValueType::Integer:
        return WriteScalar(output, std::get<int64_t>(value));
    case ValueType::Number:
        return WriteScalar(output, std::get<double>(value));
    case ValueType::Text:
        return WriteString(output, std::get<std::string>(value));
    }
    return false;
}

bool ReadValue(std::istream& input, GuiDataValue& value)
{
    ValueType type = ValueType::Empty;
    if (!ReadScalar(input, type))
    {
        return false;
    }
    switch (type)
    {
    case ValueType::Empty:
        value = std::monostate{};
        return true;
    case ValueType::Boolean:
    {
        uint8_t boolean = 0;
        if (!ReadScalar(input, boolean) || boolean > 1)
        {
            return false;
        }
        value = boolean != 0;
        return true;
    }
    case ValueType::Integer:
    {
        int64_t integer = 0;
        if (!ReadScalar(input, integer))
        {
            return false;
        }
        value = integer;
        return true;
    }
    case ValueType::Number:
    {
        double number = 0.0;
        if (!ReadScalar(input, number))
        {
            return false;
        }
        value = number;
        return true;
    }
    case ValueType::Text:
    {
        std::string text;
        if (!ReadString(input, text))
        {
            return false;
        }
        value = std::move(text);
        return true;
    }
    }
    return false;
}

template <typename Map>
std::vector<typename Map::const_iterator> SortedEntries(const Map& map)
{
    std::vector<typename Map::const_iterator> entries;
    entries.reserve(map.size());
    for (auto iterator = map.begin(); iterator != map.end(); ++iterator)
    {
        entries.push_back(iterator);
    }
    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto& first, const auto& second)
        {
            return first->first < second->first;
        }
    );
    return entries;
}

bool WriteState(
    std::ostream& output,
    std::string_view profileKey,
    std::string_view pluginId,
    const GuiPersistentState& state
)
{
    output.write(Magic.data(), Magic.size());
    if (!WriteScalar(output, FormatVersion)
        || !WriteString(output, profileKey)
        || !WriteString(output, pluginId)
        || state.values.size() > MaximumCollectionSize
        || state.lists.size() > MaximumCollectionSize)
    {
        return false;
    }

    const uint32_t valueCount = static_cast<uint32_t>(
        state.values.size()
    );
    if (!WriteScalar(output, valueCount))
    {
        return false;
    }
    for (const auto iterator : SortedEntries(state.values))
    {
        if (!WriteString(output, iterator->first)
            || !WriteValue(output, iterator->second))
        {
            return false;
        }
    }

    const uint32_t listCount = static_cast<uint32_t>(state.lists.size());
    if (!WriteScalar(output, listCount))
    {
        return false;
    }
    for (const auto listIterator : SortedEntries(state.lists))
    {
        const GuiListModel& list = listIterator->second;
        if (!WriteString(output, listIterator->first)
            || !WriteScalar(output, list.revision)
            || list.items.size() > MaximumCollectionSize)
        {
            return false;
        }
        const uint32_t itemCount = static_cast<uint32_t>(
            list.items.size()
        );
        if (!WriteScalar(output, itemCount))
        {
            return false;
        }
        for (const GuiListItem& item : list.items)
        {
            if (!WriteScalar(output, item.id)
                || !WriteString(output, item.text)
                || item.fields.size() > MaximumCollectionSize)
            {
                return false;
            }
            const uint32_t fieldCount = static_cast<uint32_t>(
                item.fields.size()
            );
            if (!WriteScalar(output, fieldCount))
            {
                return false;
            }
            for (const auto fieldIterator : SortedEntries(item.fields))
            {
                if (!WriteString(output, fieldIterator->first)
                    || !WriteValue(output, fieldIterator->second))
                {
                    return false;
                }
            }
        }
    }
    if (state.listRuntime.size() > MaximumCollectionSize)
    {
        return false;
    }
    const uint32_t runtimeCount = static_cast<uint32_t>(
        state.listRuntime.size()
    );
    if (!WriteScalar(output, runtimeCount))
    {
        return false;
    }
    for (const auto runtimeIterator : SortedEntries(state.listRuntime))
    {
        const int32_t scrollOffset = static_cast<int32_t>(std::clamp(
            runtimeIterator->second.scrollOffset,
            static_cast<int>(std::numeric_limits<int32_t>::min()),
            static_cast<int>(std::numeric_limits<int32_t>::max())
        ));
        if (!WriteString(output, runtimeIterator->first)
            || !WriteScalar(output, scrollOffset)
            || !WriteScalar(
                output,
                runtimeIterator->second.selectedItemId
            ))
        {
            return false;
        }
    }
    return output.good();
}

bool ReadState(
    std::istream& input,
    std::string_view expectedProfileKey,
    std::string_view expectedPluginId,
    GuiPersistentState& state
)
{
    std::array<char, Magic.size()> magic{};
    input.read(magic.data(), magic.size());
    uint32_t version = 0;
    std::string profileKey;
    std::string pluginId;
    if (!input.good()
        || magic != Magic
        || !ReadScalar(input, version)
        || version != FormatVersion
        || !ReadString(input, profileKey)
        || !ReadString(input, pluginId)
        || profileKey != expectedProfileKey
        || pluginId != expectedPluginId)
    {
        return false;
    }

    GuiPersistentState loaded;
    uint32_t valueCount = 0;
    if (!ReadScalar(input, valueCount)
        || valueCount > MaximumCollectionSize)
    {
        return false;
    }
    for (uint32_t index = 0; index < valueCount; ++index)
    {
        std::string name;
        GuiDataValue value;
        if (!ReadString(input, name)
            || name.empty()
            || !ReadValue(input, value))
        {
            return false;
        }
        loaded.values[std::move(name)] = std::move(value);
    }

    uint32_t listCount = 0;
    if (!ReadScalar(input, listCount)
        || listCount > MaximumCollectionSize)
    {
        return false;
    }
    for (uint32_t listIndex = 0; listIndex < listCount; ++listIndex)
    {
        std::string listName;
        GuiListModel list;
        uint32_t itemCount = 0;
        if (!ReadString(input, listName)
            || listName.empty()
            || !ReadScalar(input, list.revision)
            || !ReadScalar(input, itemCount)
            || itemCount > MaximumCollectionSize)
        {
            return false;
        }
        list.items.reserve(itemCount);
        for (uint32_t itemIndex = 0; itemIndex < itemCount; ++itemIndex)
        {
            GuiListItem item;
            uint32_t fieldCount = 0;
            if (!ReadScalar(input, item.id)
                || !ReadString(input, item.text)
                || !ReadScalar(input, fieldCount)
                || fieldCount > MaximumCollectionSize)
            {
                return false;
            }
            for (uint32_t fieldIndex = 0;
                fieldIndex < fieldCount;
                ++fieldIndex)
            {
                std::string name;
                GuiDataValue value;
                if (!ReadString(input, name)
                    || name.empty()
                    || !ReadValue(input, value))
                {
                    return false;
                }
                item.fields[std::move(name)] = std::move(value);
            }
            list.items.push_back(std::move(item));
        }
        loaded.lists[std::move(listName)] = std::move(list);
    }
    uint32_t runtimeCount = 0;
    if (!ReadScalar(input, runtimeCount)
        || runtimeCount > MaximumCollectionSize)
    {
        return false;
    }
    for (uint32_t runtimeIndex = 0;
        runtimeIndex < runtimeCount;
        ++runtimeIndex)
    {
        std::string listName;
        int32_t scrollOffset = 0;
        GuiPersistentState::ListRuntimeState runtime;
        if (!ReadString(input, listName)
            || listName.empty()
            || !ReadScalar(input, scrollOffset)
            || !ReadScalar(input, runtime.selectedItemId))
        {
            return false;
        }
        runtime.scrollOffset = scrollOffset;
        loaded.listRuntime[std::move(listName)] = runtime;
    }
    if (input.peek() != std::char_traits<char>::eof())
    {
        return false;
    }
    state = std::move(loaded);
    return true;
}

uint64_t HashKey(std::string_view value)
{
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char character : value)
    {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string SafePrefix(std::string_view value)
{
    std::string result;
    result.reserve(std::min<std::size_t>(value.size(), 32));
    for (unsigned char character : value)
    {
        if (result.size() == 32)
        {
            break;
        }
        result.push_back(std::isalnum(character) != 0
            ? static_cast<char>(std::tolower(character))
            : '_');
    }
    return result.empty() ? std::string("gui") : result;
}

}

GuiPersistenceStore::GuiPersistenceStore(std::filesystem::path root)
    : root_(std::move(root))
{
}

bool GuiPersistenceStore::Load(
    std::string_view profileKey,
    std::string_view pluginId,
    GuiPersistentState& state,
    std::string& error
) const
{
    state = {};
    if (profileKey.empty() || pluginId.empty())
    {
        error = "persistence_identity_missing";
        return false;
    }
    const std::filesystem::path path = ResolvePath(profileKey, pluginId);
    if (!std::filesystem::exists(path))
    {
        error.clear();
        return true;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input || !ReadState(input, profileKey, pluginId, state))
    {
        state = {};
        error = "persistence_file_invalid:" + path.string();
        return false;
    }
    error.clear();
    return true;
}

bool GuiPersistenceStore::Save(
    std::string_view profileKey,
    std::string_view pluginId,
    const GuiPersistentState& state,
    std::string& error
) const
{
    if (profileKey.empty() || pluginId.empty())
    {
        error = "persistence_identity_missing";
        return false;
    }
    std::error_code fileError;
    std::filesystem::create_directories(root_, fileError);
    if (fileError)
    {
        error = "persistence_directory_create_failed:"
            + fileError.message();
        return false;
    }

    const std::filesystem::path path = ResolvePath(profileKey, pluginId);
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(
            temporary,
            std::ios::binary | std::ios::trunc
        );
        if (!output || !WriteState(output, profileKey, pluginId, state))
        {
            error = "persistence_write_failed:" + temporary.string();
            return false;
        }
        output.flush();
        if (!output.good())
        {
            error = "persistence_flush_failed:" + temporary.string();
            return false;
        }
    }

    const std::filesystem::path backup = path.string() + ".bak";
    std::filesystem::remove(backup, fileError);
    fileError.clear();
    if (std::filesystem::exists(path))
    {
        std::filesystem::rename(path, backup, fileError);
        if (fileError)
        {
            const std::string message = fileError.message();
            std::error_code cleanupError;
            std::filesystem::remove(temporary, cleanupError);
            error = "persistence_backup_failed:" + message;
            return false;
        }
    }
    std::filesystem::rename(temporary, path, fileError);
    if (fileError)
    {
        std::error_code restoreError;
        if (std::filesystem::exists(backup))
        {
            std::filesystem::rename(backup, path, restoreError);
        }
        std::filesystem::remove(temporary, restoreError);
        error = "persistence_replace_failed:" + fileError.message();
        return false;
    }
    std::filesystem::remove(backup, fileError);
    error.clear();
    return true;
}

bool GuiPersistenceStore::Remove(
    std::string_view profileKey,
    std::string_view pluginId,
    std::string& error
) const
{
    if (profileKey.empty() || pluginId.empty())
    {
        error = "persistence_identity_missing";
        return false;
    }
    std::error_code fileError;
    std::filesystem::remove(ResolvePath(profileKey, pluginId), fileError);
    if (fileError)
    {
        error = "persistence_remove_failed:" + fileError.message();
        return false;
    }
    error.clear();
    return true;
}

std::filesystem::path GuiPersistenceStore::ResolvePath(
    std::string_view profileKey,
    std::string_view pluginId
) const
{
    std::string identity(profileKey);
    identity.push_back('\n');
    identity.append(pluginId);
    std::ostringstream fileName;
    fileName << SafePrefix(pluginId) << '_'
             << std::hex << std::setw(16) << std::setfill('0')
             << HashKey(identity) << ".sgstate";
    return root_ / fileName.str();
}

const std::filesystem::path& GuiPersistenceStore::Root() const
{
    return root_;
}
