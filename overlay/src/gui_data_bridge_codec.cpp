#include "gui_data_bridge_codec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace
{

constexpr uint8_t kFrameMagic[] = {'G', 'D', 'B', 'R'};

enum class WireValueType : uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    Number = 3,
    String = 4
};

class ByteWriter
{
public:
    ByteWriter(
        std::vector<uint8_t>& output,
        std::size_t maxBytes
    )
        : output_(output), maxBytes_(maxBytes)
    {
    }

    bool U8(uint8_t value, std::string& error)
    {
        if (!Reserve(1, error))
        {
            return false;
        }
        output_.push_back(value);
        return true;
    }

    bool U32(uint32_t value, std::string& error)
    {
        if (!Reserve(4, error))
        {
            return false;
        }
        for (int shift = 0; shift < 32; shift += 8)
        {
            output_.push_back(static_cast<uint8_t>(value >> shift));
        }
        return true;
    }

    bool U64(uint64_t value, std::string& error)
    {
        if (!Reserve(8, error))
        {
            return false;
        }
        for (int shift = 0; shift < 64; shift += 8)
        {
            output_.push_back(static_cast<uint8_t>(value >> shift));
        }
        return true;
    }

    bool I32(int32_t value, std::string& error)
    {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return U32(bits, error);
    }

    bool I64(int64_t value, std::string& error)
    {
        uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return U64(bits, error);
    }

    bool Number(double value, std::string& error)
    {
        uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return U64(bits, error);
    }

    bool String(
        const std::string& value,
        const GuiDataBridgeCodecLimits& limits,
        std::string& error
    )
    {
        if (value.size() > limits.maxStringBytes
            || value.size() > std::numeric_limits<uint32_t>::max())
        {
            error = "bridge_codec_string_too_large";
            return false;
        }
        if (!U32(static_cast<uint32_t>(value.size()), error)
            || !Reserve(value.size(), error))
        {
            return false;
        }
        output_.insert(output_.end(), value.begin(), value.end());
        return true;
    }

private:
    bool Reserve(std::size_t bytes, std::string& error) const
    {
        if (bytes > maxBytes_
            || output_.size() > maxBytes_ - bytes)
        {
            error = "bridge_codec_payload_too_large";
            return false;
        }
        return true;
    }

    std::vector<uint8_t>& output_;
    std::size_t maxBytes_ = 0;
};

class ByteReader
{
public:
    ByteReader(const uint8_t* data, std::size_t size)
        : data_(data), size_(size)
    {
    }

    bool U8(uint8_t& value, std::string& error)
    {
        if (!Require(1, error))
        {
            return false;
        }
        value = data_[position_++];
        return true;
    }

    bool U32(uint32_t& value, std::string& error)
    {
        if (!Require(4, error))
        {
            return false;
        }
        value = 0;
        for (int shift = 0; shift < 32; shift += 8)
        {
            value |= static_cast<uint32_t>(data_[position_++]) << shift;
        }
        return true;
    }

    bool U64(uint64_t& value, std::string& error)
    {
        if (!Require(8, error))
        {
            return false;
        }
        value = 0;
        for (int shift = 0; shift < 64; shift += 8)
        {
            value |= static_cast<uint64_t>(data_[position_++]) << shift;
        }
        return true;
    }

    bool I32(int32_t& value, std::string& error)
    {
        uint32_t bits = 0;
        if (!U32(bits, error))
        {
            return false;
        }
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool I64(int64_t& value, std::string& error)
    {
        uint64_t bits = 0;
        if (!U64(bits, error))
        {
            return false;
        }
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool Number(double& value, std::string& error)
    {
        uint64_t bits = 0;
        if (!U64(bits, error))
        {
            return false;
        }
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool String(
        std::string& value,
        const GuiDataBridgeCodecLimits& limits,
        std::string& error
    )
    {
        uint32_t length = 0;
        if (!U32(length, error))
        {
            return false;
        }
        if (length > limits.maxStringBytes)
        {
            error = "bridge_codec_string_too_large";
            return false;
        }
        if (!Require(length, error))
        {
            return false;
        }
        value.assign(
            reinterpret_cast<const char*>(data_ + position_),
            length
        );
        position_ += length;
        return true;
    }

    bool AtEnd() const
    {
        return position_ == size_;
    }

private:
    bool Require(std::size_t bytes, std::string& error) const
    {
        if (bytes > size_ || position_ > size_ - bytes)
        {
            error = "bridge_codec_payload_truncated";
            return false;
        }
        return true;
    }

    const uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t position_ = 0;
};

uint32_t Crc32(const uint8_t* data, std::size_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t index = 0; index < size; ++index)
    {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit)
        {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void AppendU16(std::vector<uint8_t>& output, uint16_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
}

void AppendU32(std::vector<uint8_t>& output, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        output.push_back(static_cast<uint8_t>(value >> shift));
    }
}

uint16_t ReadU16(const uint8_t* data)
{
    return static_cast<uint16_t>(
        static_cast<uint32_t>(data[0])
        | static_cast<uint32_t>(data[1]) << 8
    );
}

uint32_t ReadU32(const uint8_t* data)
{
    uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8)
    {
        value |= static_cast<uint32_t>(data[shift / 8]) << shift;
    }
    return value;
}

bool BuildFrame(
    GuiDataBridgeMessageType type,
    const std::vector<uint8_t>& payload,
    std::vector<uint8_t>& frame,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    if (limits.maxFrameBytes < kGuiDataBridgeFrameHeaderBytes
        || payload.size()
            > limits.maxFrameBytes - kGuiDataBridgeFrameHeaderBytes
        || payload.size() > std::numeric_limits<uint32_t>::max())
    {
        error = "bridge_codec_frame_too_large";
        return false;
    }

    std::vector<uint8_t> next;
    next.reserve(kGuiDataBridgeFrameHeaderBytes + payload.size());
    next.insert(next.end(), std::begin(kFrameMagic), std::end(kFrameMagic));
    AppendU16(next, kGuiDataBridgeProtocolVersion);
    AppendU16(next, static_cast<uint16_t>(type));
    AppendU32(next, static_cast<uint32_t>(payload.size()));
    AppendU32(next, Crc32(payload.data(), payload.size()));
    next.insert(next.end(), payload.begin(), payload.end());
    frame = std::move(next);
    error.clear();
    return true;
}

bool ParseFrame(
    const std::vector<uint8_t>& frame,
    GuiDataBridgeFrameInfo& info,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    if (frame.size() < kGuiDataBridgeFrameHeaderBytes)
    {
        error = "bridge_codec_frame_truncated";
        return false;
    }
    if (frame.size() > limits.maxFrameBytes)
    {
        error = "bridge_codec_frame_too_large";
        return false;
    }
    if (!std::equal(
            std::begin(kFrameMagic),
            std::end(kFrameMagic),
            frame.begin()
        ))
    {
        error = "bridge_codec_magic_invalid";
        return false;
    }

    GuiDataBridgeFrameInfo next;
    next.version = ReadU16(frame.data() + 4);
    const uint16_t rawType = ReadU16(frame.data() + 6);
    next.payloadBytes = ReadU32(frame.data() + 8);
    next.checksum = ReadU32(frame.data() + 12);
    if (next.version != kGuiDataBridgeProtocolVersion)
    {
        error = "bridge_codec_version_unsupported: "
            + std::to_string(next.version);
        return false;
    }
    if (rawType != static_cast<uint16_t>(
            GuiDataBridgeMessageType::DataUpdate
        )
        && rawType != static_cast<uint16_t>(
            GuiDataBridgeMessageType::Action
        ))
    {
        error = "bridge_codec_message_type_invalid";
        return false;
    }
    next.type = static_cast<GuiDataBridgeMessageType>(rawType);

    const std::size_t actualPayloadBytes =
        frame.size() - kGuiDataBridgeFrameHeaderBytes;
    if (next.payloadBytes != actualPayloadBytes)
    {
        error = "bridge_codec_payload_size_mismatch";
        return false;
    }
    const uint8_t* payload = frame.data()
        + kGuiDataBridgeFrameHeaderBytes;
    if (next.checksum != Crc32(payload, actualPayloadBytes))
    {
        error = "bridge_codec_checksum_mismatch";
        return false;
    }

    info = next;
    error.clear();
    return true;
}

template <typename Map>
std::vector<const typename Map::value_type*> SortedEntries(
    const Map& map
)
{
    std::vector<const typename Map::value_type*> entries;
    entries.reserve(map.size());
    for (const auto& entry : map)
    {
        entries.push_back(&entry);
    }
    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto* first, const auto* second)
        {
            return first->first < second->first;
        }
    );
    return entries;
}

bool CheckCount(
    std::size_t count,
    std::size_t maximum,
    std::string_view field,
    std::string& error
)
{
    if (count > maximum
        || count > std::numeric_limits<uint32_t>::max())
    {
        error = "bridge_codec_count_limit: " + std::string(field);
        return false;
    }
    return true;
}

bool WriteName(
    ByteWriter& writer,
    const std::string& name,
    const GuiDataBridgeCodecLimits& limits,
    std::string& error
)
{
    if (name.empty())
    {
        error = "bridge_codec_name_empty";
        return false;
    }
    return writer.String(name, limits, error);
}

bool WriteDataValue(
    ByteWriter& writer,
    const GuiDataValue& value,
    const GuiDataBridgeCodecLimits& limits,
    std::string& error
)
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return writer.U8(
            static_cast<uint8_t>(WireValueType::Null),
            error
        );
    }
    if (const bool* boolean = std::get_if<bool>(&value))
    {
        return writer.U8(
                static_cast<uint8_t>(WireValueType::Boolean),
                error
            )
            && writer.U8(*boolean ? 1 : 0, error);
    }
    if (const int64_t* integer = std::get_if<int64_t>(&value))
    {
        return writer.U8(
                static_cast<uint8_t>(WireValueType::Integer),
                error
            )
            && writer.I64(*integer, error);
    }
    if (const double* number = std::get_if<double>(&value))
    {
        if (!std::isfinite(*number))
        {
            error = "bridge_codec_number_not_finite";
            return false;
        }
        return writer.U8(
                static_cast<uint8_t>(WireValueType::Number),
                error
            )
            && writer.Number(*number, error);
    }
    const std::string* string = std::get_if<std::string>(&value);
    return string
        && writer.U8(
            static_cast<uint8_t>(WireValueType::String),
            error
        )
        && writer.String(*string, limits, error);
}

bool ReadDataValue(
    ByteReader& reader,
    GuiDataValue& value,
    const GuiDataBridgeCodecLimits& limits,
    std::string& error
)
{
    uint8_t rawType = 0;
    if (!reader.U8(rawType, error))
    {
        return false;
    }
    switch (static_cast<WireValueType>(rawType))
    {
    case WireValueType::Null:
        value = std::monostate{};
        return true;
    case WireValueType::Boolean:
    {
        uint8_t boolean = 0;
        if (!reader.U8(boolean, error) || boolean > 1)
        {
            if (error.empty())
            {
                error = "bridge_codec_boolean_invalid";
            }
            return false;
        }
        value = boolean != 0;
        return true;
    }
    case WireValueType::Integer:
    {
        int64_t integer = 0;
        if (!reader.I64(integer, error))
        {
            return false;
        }
        value = integer;
        return true;
    }
    case WireValueType::Number:
    {
        double number = 0.0;
        if (!reader.Number(number, error))
        {
            return false;
        }
        if (!std::isfinite(number))
        {
            error = "bridge_codec_number_not_finite";
            return false;
        }
        value = number;
        return true;
    }
    case WireValueType::String:
    {
        std::string string;
        if (!reader.String(string, limits, error))
        {
            return false;
        }
        value = std::move(string);
        return true;
    }
    }
    error = "bridge_codec_value_type_invalid";
    return false;
}

bool WriteRemovedNames(
    ByteWriter& writer,
    const std::vector<std::string>& names,
    std::size_t maximum,
    std::string_view field,
    const GuiDataBridgeCodecLimits& limits,
    std::string& error
)
{
    if (!CheckCount(names.size(), maximum, field, error))
    {
        return false;
    }
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
    {
        error = "bridge_codec_duplicate_name: " + std::string(field);
        return false;
    }
    if (!writer.U32(static_cast<uint32_t>(sorted.size()), error))
    {
        return false;
    }
    for (const std::string& name : sorted)
    {
        if (!WriteName(writer, name, limits, error))
        {
            return false;
        }
    }
    return true;
}

bool ReadCount(
    ByteReader& reader,
    std::size_t maximum,
    std::string_view field,
    uint32_t& count,
    std::string& error
)
{
    if (!reader.U32(count, error))
    {
        return false;
    }
    if (count > maximum)
    {
        error = "bridge_codec_count_limit: " + std::string(field);
        return false;
    }
    return true;
}

bool ReadName(
    ByteReader& reader,
    std::string& name,
    const GuiDataBridgeCodecLimits& limits,
    std::string& error
)
{
    if (!reader.String(name, limits, error))
    {
        return false;
    }
    if (name.empty())
    {
        error = "bridge_codec_name_empty";
        return false;
    }
    return true;
}

bool ReadRemovedNames(
    ByteReader& reader,
    std::size_t maximum,
    std::string_view field,
    std::vector<std::string>& names,
    const GuiDataBridgeCodecLimits& limits,
    std::string& error
)
{
    uint32_t count = 0;
    if (!ReadCount(reader, maximum, field, count, error))
    {
        return false;
    }
    std::unordered_set<std::string> unique;
    names.reserve(count);
    for (uint32_t index = 0; index < count; ++index)
    {
        std::string name;
        if (!ReadName(reader, name, limits, error)
            || !unique.insert(name).second)
        {
            if (error.empty())
            {
                error = "bridge_codec_duplicate_name: "
                    + std::string(field);
            }
            return false;
        }
        names.push_back(std::move(name));
    }
    return true;
}

bool ValidateUpdateRevision(
    const GuiDataBridgeUpdate& update,
    std::string& error
)
{
    if (update.revision == 0)
    {
        error = "bridge_codec_revision_missing";
        return false;
    }
    if (update.fullSnapshot)
    {
        if (update.baseRevision != 0)
        {
            error = "bridge_codec_snapshot_base_invalid";
            return false;
        }
        return true;
    }
    if (update.baseRevision == 0
        || update.revision <= update.baseRevision)
    {
        error = "bridge_codec_delta_revision_invalid";
        return false;
    }
    return true;
}

bool IntFitsI32(int value)
{
    const int64_t converted = value;
    return converted >= std::numeric_limits<int32_t>::min()
        && converted <= std::numeric_limits<int32_t>::max();
}

}

bool InspectGuiDataBridgeFrame(
    const std::vector<uint8_t>& frame,
    GuiDataBridgeFrameInfo& info,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    return ParseFrame(frame, info, error, limits);
}

bool EncodeGuiDataBridgeUpdate(
    const GuiDataBridgeUpdate& update,
    std::vector<uint8_t>& frame,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    error.clear();
    if (!ValidateUpdateRevision(update, error)
        || !CheckCount(
            update.values.size(),
            limits.maxValues,
            "values",
            error
        )
        || !CheckCount(
            update.lists.size(),
            limits.maxLists,
            "lists",
            error
        ))
    {
        return false;
    }

    const std::size_t maxPayload =
        limits.maxFrameBytes >= kGuiDataBridgeFrameHeaderBytes
        ? limits.maxFrameBytes - kGuiDataBridgeFrameHeaderBytes
        : 0;
    std::vector<uint8_t> payload;
    ByteWriter writer(payload, maxPayload);
    if (!writer.U64(update.revision, error)
        || !writer.U64(update.baseRevision, error)
        || !writer.U8(update.fullSnapshot ? 1 : 0, error)
        || !writer.U32(
            static_cast<uint32_t>(update.values.size()),
            error
        ))
    {
        return false;
    }

    for (const auto* entry : SortedEntries(update.values))
    {
        if (!WriteName(writer, entry->first, limits, error)
            || !WriteDataValue(
                writer,
                entry->second,
                limits,
                error
            ))
        {
            return false;
        }
    }
    if (!WriteRemovedNames(
            writer,
            update.removedValues,
            limits.maxValues,
            "removed_values",
            limits,
            error
        )
        || !writer.U32(
            static_cast<uint32_t>(update.lists.size()),
            error
        ))
    {
        return false;
    }

    std::size_t totalItems = 0;
    for (const auto* entry : SortedEntries(update.lists))
    {
        const GuiListModel& list = entry->second;
        if (totalItems > limits.maxListItems
            || list.items.size() > limits.maxListItems - totalItems
            || !CheckCount(
                list.items.size(),
                limits.maxListItems,
                "list_items",
                error
            )
            || !WriteName(writer, entry->first, limits, error)
            || !writer.U64(list.revision, error)
            || !writer.U32(
                static_cast<uint32_t>(list.items.size()),
                error
            ))
        {
            if (error.empty())
            {
                error = "bridge_codec_count_limit: list_items";
            }
            return false;
        }
        totalItems += list.items.size();

        std::unordered_set<uint64_t> itemIds;
        itemIds.reserve(list.items.size());
        std::size_t totalFields = 0;
        for (const GuiListItem& item : list.items)
        {
            if (item.id == 0 || !itemIds.insert(item.id).second)
            {
                error = "bridge_codec_list_item_id_invalid";
                return false;
            }
            if (!writer.U64(item.id, error)
                || !writer.String(item.text, limits, error)
                || totalFields > limits.maxListItemFields
                || item.fields.size()
                    > limits.maxListItemFields - totalFields
                || !CheckCount(
                    item.fields.size(),
                    limits.maxListItemFields,
                    "list_item_fields",
                    error
                )
                || !writer.U32(
                    static_cast<uint32_t>(item.fields.size()),
                    error
                ))
            {
                return false;
            }
            totalFields += item.fields.size();
            for (const auto* field : SortedEntries(item.fields))
            {
                if (!WriteName(writer, field->first, limits, error)
                    || !WriteDataValue(
                        writer,
                        field->second,
                        limits,
                        error
                    ))
                {
                    return false;
                }
            }
        }
    }
    if (!WriteRemovedNames(
            writer,
            update.removedLists,
            limits.maxLists,
            "removed_lists",
            limits,
            error
        ))
    {
        return false;
    }
    return BuildFrame(
        GuiDataBridgeMessageType::DataUpdate,
        payload,
        frame,
        error,
        limits
    );
}

bool DecodeGuiDataBridgeUpdate(
    const std::vector<uint8_t>& frame,
    GuiDataBridgeUpdate& update,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    GuiDataBridgeFrameInfo info;
    if (!ParseFrame(frame, info, error, limits))
    {
        return false;
    }
    if (info.type != GuiDataBridgeMessageType::DataUpdate)
    {
        error = "bridge_codec_message_type_mismatch";
        return false;
    }

    ByteReader reader(
        frame.data() + kGuiDataBridgeFrameHeaderBytes,
        info.payloadBytes
    );
    GuiDataBridgeUpdate next;
    uint8_t fullSnapshot = 0;
    if (!reader.U64(next.revision, error)
        || !reader.U64(next.baseRevision, error)
        || !reader.U8(fullSnapshot, error))
    {
        return false;
    }
    if (fullSnapshot > 1)
    {
        error = "bridge_codec_snapshot_flag_invalid";
        return false;
    }
    next.fullSnapshot = fullSnapshot != 0;
    if (!ValidateUpdateRevision(next, error))
    {
        return false;
    }

    uint32_t valueCount = 0;
    if (!ReadCount(
            reader,
            limits.maxValues,
            "values",
            valueCount,
            error
        ))
    {
        return false;
    }
    next.values.reserve(valueCount);
    for (uint32_t index = 0; index < valueCount; ++index)
    {
        std::string name;
        GuiDataValue value;
        if (!ReadName(reader, name, limits, error)
            || !ReadDataValue(reader, value, limits, error))
        {
            return false;
        }
        if (!next.values.emplace(std::move(name), std::move(value)).second)
        {
            error = "bridge_codec_duplicate_name: values";
            return false;
        }
    }
    if (!ReadRemovedNames(
            reader,
            limits.maxValues,
            "removed_values",
            next.removedValues,
            limits,
            error
        ))
    {
        return false;
    }

    uint32_t listCount = 0;
    if (!ReadCount(
            reader,
            limits.maxLists,
            "lists",
            listCount,
            error
        ))
    {
        return false;
    }
    next.lists.reserve(listCount);
    std::size_t totalItems = 0;
    for (uint32_t listIndex = 0; listIndex < listCount; ++listIndex)
    {
        std::string name;
        GuiListModel list;
        uint32_t itemCount = 0;
        if (!ReadName(reader, name, limits, error)
            || !reader.U64(list.revision, error)
            || !ReadCount(
                reader,
                limits.maxListItems,
                "list_items",
                itemCount,
                error
            ))
        {
            return false;
        }
        if (totalItems > limits.maxListItems
            || itemCount > limits.maxListItems - totalItems)
        {
            error = "bridge_codec_count_limit: list_items";
            return false;
        }
        totalItems += itemCount;
        list.items.reserve(itemCount);
        std::unordered_set<uint64_t> itemIds;
        itemIds.reserve(itemCount);
        std::size_t totalFields = 0;
        for (uint32_t itemIndex = 0;
            itemIndex < itemCount;
            ++itemIndex)
        {
            GuiListItem item;
            if (!reader.U64(item.id, error)
                || !reader.String(item.text, limits, error))
            {
                return false;
            }
            if (item.id == 0 || !itemIds.insert(item.id).second)
            {
                error = "bridge_codec_list_item_id_invalid";
                return false;
            }
            uint32_t fieldCount = 0;
            if (!ReadCount(
                    reader,
                    limits.maxListItemFields,
                    "list_item_fields",
                    fieldCount,
                    error
                )
                || totalFields > limits.maxListItemFields
                || fieldCount
                    > limits.maxListItemFields - totalFields)
            {
                if (error.empty())
                {
                    error = "bridge_codec_count_limit: list_item_fields";
                }
                return false;
            }
            totalFields += fieldCount;
            item.fields.reserve(fieldCount);
            for (uint32_t fieldIndex = 0;
                fieldIndex < fieldCount;
                ++fieldIndex)
            {
                std::string fieldName;
                GuiDataValue fieldValue;
                if (!ReadName(reader, fieldName, limits, error)
                    || !ReadDataValue(
                        reader,
                        fieldValue,
                        limits,
                        error
                    ))
                {
                    return false;
                }
                if (!item.fields.emplace(
                        std::move(fieldName),
                        std::move(fieldValue)
                    ).second)
                {
                    error = "bridge_codec_duplicate_name: "
                        "list_item_fields";
                    return false;
                }
            }
            list.items.push_back(std::move(item));
        }
        if (!next.lists.emplace(std::move(name), std::move(list)).second)
        {
            error = "bridge_codec_duplicate_name: lists";
            return false;
        }
    }
    if (!ReadRemovedNames(
            reader,
            limits.maxLists,
            "removed_lists",
            next.removedLists,
            limits,
            error
        ))
    {
        return false;
    }
    if (!reader.AtEnd())
    {
        error = "bridge_codec_payload_trailing_bytes";
        return false;
    }

    update = std::move(next);
    error.clear();
    return true;
}

bool EncodeGuiDataBridgeAction(
    const GuiActionContext& context,
    std::vector<uint8_t>& frame,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    error.clear();
    if (context.action.empty()
        && context.functionName.empty()
        && context.fallbackOperation.empty())
    {
        error = "bridge_codec_action_operation_missing";
        return false;
    }
    if (!CheckCount(
            context.parameters.size(),
            limits.maxActionParameters,
            "action_parameters",
            error
        )
        || !IntFitsI32(context.listIndex)
        || !IntFitsI32(context.mouseX)
        || !IntFitsI32(context.mouseY))
    {
        if (error.empty())
        {
            error = "bridge_codec_action_integer_out_of_range";
        }
        return false;
    }

    const std::size_t maxPayload =
        limits.maxFrameBytes >= kGuiDataBridgeFrameHeaderBytes
        ? limits.maxFrameBytes - kGuiDataBridgeFrameHeaderBytes
        : 0;
    std::vector<uint8_t> payload;
    ByteWriter writer(payload, maxPayload);
    const std::string* strings[] = {
        &context.action,
        &context.functionName,
        &context.fallbackOperation,
        &context.phase,
        &context.windowName,
        &context.widgetName,
        &context.listName
    };
    for (const std::string* string : strings)
    {
        if (!writer.String(*string, limits, error))
        {
            return false;
        }
    }
    if (!writer.I32(static_cast<int32_t>(context.listIndex), error)
        || !writer.U64(context.listItemId, error)
        || !writer.U8(context.hasListItemId ? 1 : 0, error)
        || !writer.I32(static_cast<int32_t>(context.mouseX), error)
        || !writer.I32(static_cast<int32_t>(context.mouseY), error)
        || !writer.U32(
            static_cast<uint32_t>(context.parameters.size()),
            error
        ))
    {
        return false;
    }
    for (const auto* entry : SortedEntries(context.parameters))
    {
        if (!WriteName(writer, entry->first, limits, error)
            || !writer.String(entry->second, limits, error))
        {
            return false;
        }
    }
    return BuildFrame(
        GuiDataBridgeMessageType::Action,
        payload,
        frame,
        error,
        limits
    );
}

bool DecodeGuiDataBridgeAction(
    const std::vector<uint8_t>& frame,
    GuiActionContext& context,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits
)
{
    GuiDataBridgeFrameInfo info;
    if (!ParseFrame(frame, info, error, limits))
    {
        return false;
    }
    if (info.type != GuiDataBridgeMessageType::Action)
    {
        error = "bridge_codec_message_type_mismatch";
        return false;
    }

    ByteReader reader(
        frame.data() + kGuiDataBridgeFrameHeaderBytes,
        info.payloadBytes
    );
    GuiActionContext next;
    std::string* strings[] = {
        &next.action,
        &next.functionName,
        &next.fallbackOperation,
        &next.phase,
        &next.windowName,
        &next.widgetName,
        &next.listName
    };
    for (std::string* string : strings)
    {
        if (!reader.String(*string, limits, error))
        {
            return false;
        }
    }
    int32_t listIndex = -1;
    int32_t mouseX = 0;
    int32_t mouseY = 0;
    uint8_t hasListItemId = 0;
    if (!reader.I32(listIndex, error)
        || !reader.U64(next.listItemId, error)
        || !reader.U8(hasListItemId, error)
        || !reader.I32(mouseX, error)
        || !reader.I32(mouseY, error))
    {
        return false;
    }
    if (hasListItemId > 1)
    {
        error = "bridge_codec_action_item_flag_invalid";
        return false;
    }
    next.listIndex = listIndex;
    next.hasListItemId = hasListItemId != 0;
    next.mouseX = mouseX;
    next.mouseY = mouseY;

    uint32_t parameterCount = 0;
    if (!ReadCount(
            reader,
            limits.maxActionParameters,
            "action_parameters",
            parameterCount,
            error
        ))
    {
        return false;
    }
    next.parameters.reserve(parameterCount);
    for (uint32_t index = 0; index < parameterCount; ++index)
    {
        std::string name;
        std::string value;
        if (!ReadName(reader, name, limits, error)
            || !reader.String(value, limits, error))
        {
            return false;
        }
        if (!next.parameters.emplace(
                std::move(name),
                std::move(value)
            ).second)
        {
            error = "bridge_codec_duplicate_name: action_parameters";
            return false;
        }
    }
    if (!reader.AtEnd())
    {
        error = "bridge_codec_payload_trailing_bytes";
        return false;
    }
    if (next.action.empty()
        && next.functionName.empty()
        && next.fallbackOperation.empty())
    {
        error = "bridge_codec_action_operation_missing";
        return false;
    }

    context = std::move(next);
    error.clear();
    return true;
}
