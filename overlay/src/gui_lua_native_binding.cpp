#include "gui_lua_native_binding.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "gui_data.h"
#include "gui_diagnostics.h"

namespace
{

constexpr int LuaGlobalsIndex = -10002;
constexpr int LuaUpvalueOne = LuaGlobalsIndex - 1;
constexpr int LuaTypeNil = 0;
constexpr int LuaTypeBoolean = 1;
constexpr int LuaTypeNumber = 3;
constexpr int LuaTypeString = 4;
constexpr int LuaTypeTable = 5;
constexpr int LuaTypeFunction = 6;
constexpr std::size_t MaximumValues = 65536;
constexpr std::size_t MaximumLists = 1024;
constexpr std::size_t MaximumItemsPerList = 65536;
constexpr std::size_t MaximumFieldsPerItem = 1024;
constexpr auto PublisherLease = std::chrono::seconds(10);

std::atomic<GuiLuaNativeBinding*> ActiveBinding{nullptr};

class LuaStackGuard
{
public:
    LuaStackGuard(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api
    )
        : state_(state),
          api_(api),
          top_(api.getTop(state))
    {
    }

    ~LuaStackGuard()
    {
        api_.setTop(state_, top_);
    }

private:
    ScriptedGuiLuaState* state_;
    const ScriptedGuiLua51ApiV1& api_;
    int top_;
};

int AbsoluteIndex(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int index
)
{
    return index > 0 || index <= LuaGlobalsIndex
        ? index
        : api.getTop(state) + index + 1;
}

void Pop(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int count = 1
)
{
    api.setTop(state, -count - 1);
}

std::string NormalizeName(std::string value)
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

bool ReadString(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int index,
    std::string& output
)
{
    if (api.type(state, index) != LuaTypeString)
    {
        return false;
    }
    std::size_t length = 0;
    const char* value = api.toLString(state, index, &length);
    if (!value)
    {
        return false;
    }
    output.assign(value, length);
    return true;
}

bool ReadUnsigned(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int index,
    uint64_t& output
)
{
    if (api.type(state, index) != LuaTypeNumber)
    {
        return false;
    }
    const double value = api.toNumber(state, index);
    if (!std::isfinite(value)
        || value < 0.0
        || std::floor(value) != value
        || value > static_cast<double>(
            std::numeric_limits<uint64_t>::max()
        ))
    {
        return false;
    }
    output = static_cast<uint64_t>(value);
    return true;
}

bool ReadScalar(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int index,
    GuiDataValue& output
)
{
    const int type = api.type(state, index);
    if (type == LuaTypeBoolean)
    {
        output = api.toBoolean(state, index) != 0;
        return true;
    }
    if (type == LuaTypeNumber)
    {
        const double value = api.toNumber(state, index);
        if (!std::isfinite(value))
        {
            return false;
        }
        if (std::floor(value) == value
            && value >= static_cast<double>(
                std::numeric_limits<int64_t>::min()
            )
            && value <= static_cast<double>(
                std::numeric_limits<int64_t>::max()
            ))
        {
            output = static_cast<int64_t>(value);
        }
        else
        {
            output = value;
        }
        return true;
    }
    if (type == LuaTypeString)
    {
        std::string value;
        if (!ReadString(state, api, index, value))
        {
            return false;
        }
        output = std::move(value);
        return true;
    }
    return false;
}

bool ReadNamedUnsigned(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int tableIndex,
    const char* name,
    uint64_t& output,
    bool required
)
{
    tableIndex = AbsoluteIndex(state, api, tableIndex);
    api.getField(state, tableIndex, name);
    const bool missing = api.type(state, -1) == LuaTypeNil;
    const bool valid = missing
        ? !required
        : ReadUnsigned(state, api, -1, output);
    Pop(state, api);
    return valid;
}

bool ReadNamedBoolean(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int tableIndex,
    const char* name,
    bool& output
)
{
    tableIndex = AbsoluteIndex(state, api, tableIndex);
    api.getField(state, tableIndex, name);
    const int type = api.type(state, -1);
    if (type != LuaTypeBoolean && type != LuaTypeNumber)
    {
        Pop(state, api);
        return false;
    }
    output = api.toBoolean(state, -1) != 0;
    Pop(state, api);
    return true;
}

bool DecodeValues(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int updateIndex,
    GuiDataBridgeUpdate& update,
    std::string& error
)
{
    updateIndex = AbsoluteIndex(state, api, updateIndex);
    LuaStackGuard guard(state, api);
    api.getField(state, updateIndex, "values");
    if (api.type(state, -1) == LuaTypeNil)
    {
        return true;
    }
    if (api.type(state, -1) != LuaTypeTable)
    {
        error = "lua_update_values_not_table";
        return false;
    }
    const int valuesIndex = AbsoluteIndex(state, api, -1);
    api.pushNil(state);
    while (api.next(state, valuesIndex) != 0)
    {
        std::string name;
        GuiDataValue value;
        if (!ReadString(state, api, -2, name)
            || name.empty()
            || !ReadScalar(state, api, -1, value))
        {
            error = "lua_update_value_invalid";
            return false;
        }
        update.values[NormalizeName(std::move(name))] = std::move(value);
        if (update.values.size() > MaximumValues)
        {
            error = "lua_update_value_limit_exceeded";
            return false;
        }
        Pop(state, api);
    }
    return true;
}

bool DecodeRemovedNames(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int updateIndex,
    const char* fieldName,
    std::vector<std::string>& output,
    std::string& error
)
{
    updateIndex = AbsoluteIndex(state, api, updateIndex);
    LuaStackGuard guard(state, api);
    api.getField(state, updateIndex, fieldName);
    if (api.type(state, -1) == LuaTypeNil)
    {
        return true;
    }
    if (api.type(state, -1) != LuaTypeTable)
    {
        error = std::string("lua_update_") + fieldName + "_not_table";
        return false;
    }
    const int namesIndex = AbsoluteIndex(state, api, -1);
    const std::size_t count = api.objLen(state, namesIndex);
    if (count > MaximumValues)
    {
        error = std::string("lua_update_") + fieldName
            + "_limit_exceeded";
        return false;
    }
    output.reserve(count);
    for (std::size_t index = 1; index <= count; ++index)
    {
        api.rawGetI(state, namesIndex, static_cast<int>(index));
        std::string name;
        if (!ReadString(state, api, -1, name) || name.empty())
        {
            error = std::string("lua_update_") + fieldName
                + "_entry_invalid";
            return false;
        }
        output.push_back(NormalizeName(std::move(name)));
        Pop(state, api);
    }
    return true;
}

bool DecodeListItem(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int itemIndex,
    GuiListItem& item,
    std::string& error
)
{
    itemIndex = AbsoluteIndex(state, api, itemIndex);
    if (api.type(state, itemIndex) != LuaTypeTable
        || !ReadNamedUnsigned(
            state,
            api,
            itemIndex,
            "id",
            item.id,
            true
        )
        || item.id == 0)
    {
        error = "lua_list_item_id_invalid";
        return false;
    }

    api.pushNil(state);
    while (api.next(state, itemIndex) != 0)
    {
        std::string name;
        if (!ReadString(state, api, -2, name))
        {
            error = "lua_list_item_field_name_invalid";
            return false;
        }
        name = NormalizeName(std::move(name));
        if (name != "id")
        {
            GuiDataValue value;
            if (!ReadScalar(state, api, -1, value))
            {
                error = "lua_list_item_field_invalid: " + name;
                return false;
            }
            if (name == "text")
            {
                item.text = GuiDataValueToText(value);
            }
            item.fields[std::move(name)] = std::move(value);
            if (item.fields.size() > MaximumFieldsPerItem)
            {
                error = "lua_list_item_field_limit_exceeded";
                return false;
            }
        }
        Pop(state, api);
    }
    return true;
}

bool DecodeList(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int listIndex,
    GuiListModel& list,
    std::string& error
)
{
    listIndex = AbsoluteIndex(state, api, listIndex);
    if (api.type(state, listIndex) != LuaTypeTable
        || !ReadNamedUnsigned(
            state,
            api,
            listIndex,
            "revision",
            list.revision,
            false
        ))
    {
        error = "lua_list_invalid";
        return false;
    }
    LuaStackGuard guard(state, api);
    api.getField(state, listIndex, "items");
    if (api.type(state, -1) == LuaTypeNil)
    {
        return true;
    }
    if (api.type(state, -1) != LuaTypeTable)
    {
        error = "lua_list_items_not_table";
        return false;
    }
    const int itemsIndex = AbsoluteIndex(state, api, -1);
    const std::size_t count = api.objLen(state, itemsIndex);
    if (count > MaximumItemsPerList)
    {
        error = "lua_list_item_limit_exceeded";
        return false;
    }
    list.items.reserve(count);
    std::unordered_set<uint64_t> ids;
    for (std::size_t index = 1; index <= count; ++index)
    {
        api.rawGetI(state, itemsIndex, static_cast<int>(index));
        GuiListItem item;
        if (!DecodeListItem(state, api, -1, item, error)
            || !ids.insert(item.id).second)
        {
            if (error.empty())
            {
                error = "lua_list_item_id_duplicate";
            }
            return false;
        }
        list.items.push_back(std::move(item));
        Pop(state, api);
    }
    return true;
}

bool DecodeLists(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int updateIndex,
    GuiDataBridgeUpdate& update,
    std::string& error
)
{
    updateIndex = AbsoluteIndex(state, api, updateIndex);
    LuaStackGuard guard(state, api);
    api.getField(state, updateIndex, "lists");
    if (api.type(state, -1) == LuaTypeNil)
    {
        return true;
    }
    if (api.type(state, -1) != LuaTypeTable)
    {
        error = "lua_update_lists_not_table";
        return false;
    }
    const int listsIndex = AbsoluteIndex(state, api, -1);
    api.pushNil(state);
    while (api.next(state, listsIndex) != 0)
    {
        std::string name;
        GuiListModel list;
        if (!ReadString(state, api, -2, name)
            || name.empty()
            || !DecodeList(state, api, -1, list, error))
        {
            if (error.empty())
            {
                error = "lua_update_list_invalid";
            }
            return false;
        }
        update.lists[NormalizeName(std::move(name))] = std::move(list);
        if (update.lists.size() > MaximumLists)
        {
            error = "lua_update_list_limit_exceeded";
            return false;
        }
        Pop(state, api);
    }
    return true;
}

bool DecodeUpdate(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int updateIndex,
    GuiDataBridgeUpdate& update,
    std::string& error
)
{
    if (api.type(state, updateIndex) != LuaTypeTable
        || !ReadNamedUnsigned(
            state,
            api,
            updateIndex,
            "revision",
            update.revision,
            true
        )
        || update.revision == 0
        || !ReadNamedUnsigned(
            state,
            api,
            updateIndex,
            "baseRevision",
            update.baseRevision,
            false
        )
        || !ReadNamedBoolean(
            state,
            api,
            updateIndex,
            "fullSnapshot",
            update.fullSnapshot
        ))
    {
        error = "lua_update_header_invalid";
        return false;
    }
    return DecodeValues(state, api, updateIndex, update, error)
        && DecodeRemovedNames(
            state,
            api,
            updateIndex,
            "removedValues",
            update.removedValues,
            error
        )
        && DecodeLists(state, api, updateIndex, update, error)
        && DecodeRemovedNames(
            state,
            api,
            updateIndex,
            "removedLists",
            update.removedLists,
            error
        );
}

void PushString(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    std::string_view value
)
{
    api.pushLString(state, value.data(), value.size());
}

void SetStringField(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int tableIndex,
    const char* name,
    std::string_view value
)
{
    tableIndex = AbsoluteIndex(state, api, tableIndex);
    PushString(state, api, value);
    api.setField(state, tableIndex, name);
}

void SetNumberField(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    int tableIndex,
    const char* name,
    double value
)
{
    tableIndex = AbsoluteIndex(state, api, tableIndex);
    api.pushNumber(state, value);
    api.setField(state, tableIndex, name);
}

void PushAction(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    const GuiActionContext& context
)
{
    api.createTable(
        state,
        0,
        static_cast<int>(context.parameters.size() + 14)
    );
    const int actionIndex = AbsoluteIndex(state, api, -1);
    SetStringField(state, api, actionIndex, "action", context.action);
    SetStringField(
        state,
        api,
        actionIndex,
        "functionName",
        context.functionName
    );
    SetStringField(
        state,
        api,
        actionIndex,
        "fallbackOperation",
        context.fallbackOperation
    );
    SetStringField(state, api, actionIndex, "phase", context.phase);
    SetStringField(
        state,
        api,
        actionIndex,
        "windowName",
        context.windowName
    );
    SetStringField(
        state,
        api,
        actionIndex,
        "widgetName",
        context.widgetName
    );
    SetStringField(state, api, actionIndex, "listName", context.listName);
    SetNumberField(state, api, actionIndex, "listIndex", context.listIndex);
    if (context.hasListItemId)
    {
        SetNumberField(
            state,
            api,
            actionIndex,
            "listItemId",
            static_cast<double>(context.listItemId)
        );
    }
    SetNumberField(state, api, actionIndex, "mouseX", context.mouseX);
    SetNumberField(state, api, actionIndex, "mouseY", context.mouseY);

    api.createTable(
        state,
        0,
        static_cast<int>(context.parameters.size())
    );
    const int parametersIndex = AbsoluteIndex(state, api, -1);
    for (const auto& parameter : context.parameters)
    {
        SetStringField(
            state,
            api,
            parametersIndex,
            parameter.first.c_str(),
            parameter.second
        );
        SetStringField(
            state,
            api,
            actionIndex,
            parameter.first.c_str(),
            parameter.second
        );
    }
    api.setField(state, actionIndex, "parameters");
}

bool IsComplete(const ScriptedGuiLua51ApiV1& api)
{
    return api.size >= sizeof(ScriptedGuiLua51ApiV1)
        && api.version == SCRIPTED_GUI_LUA51_API_VERSION
        && api.getTop
        && api.setTop
        && api.type
        && api.toBoolean
        && api.toNumber
        && api.toLString
        && api.toUserdata
        && api.pushNil
        && api.pushBoolean
        && api.pushNumber
        && api.pushLString
        && api.pushLightUserdata
        && api.pushCClosure
        && api.createTable
        && api.getField
        && api.setField
        && api.objLen
        && api.rawGetI
        && api.next;
}

bool HasNativeTable(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api
)
{
    LuaStackGuard guard(state, api);
    api.getField(state, LuaGlobalsIndex, "ScriptedGuiNative");
    if (api.type(state, -1) != LuaTypeTable)
    {
        return false;
    }
    const int tableIndex = AbsoluteIndex(state, api, -1);
    api.getField(state, tableIndex, "TryAcquireChannel");
    const bool hasAcquire = api.type(state, -1) == LuaTypeFunction;
    Pop(state, api);
    api.getField(state, tableIndex, "ReleaseChannel");
    const bool hasRelease = api.type(state, -1) == LuaTypeFunction;
    Pop(state, api);
    api.getField(state, tableIndex, "PublishUpdate");
    const bool hasPublish = api.type(state, -1) == LuaTypeFunction;
    Pop(state, api);
    api.getField(state, tableIndex, "TryPopAction");
    const bool hasActions = api.type(state, -1) == LuaTypeFunction;
    return hasAcquire && hasRelease && hasPublish && hasActions;
}

}

struct GuiLuaNativeBinding::Impl
{
    struct StateBinding
    {
        ScriptedGuiLuaState* state = nullptr;
        ScriptedGuiLua51ApiV1 api{};
        GuiLuaBridgeService* service = nullptr;
        uint64_t ordinal = 0;
        std::chrono::steady_clock::time_point lastSeen;
    };

    struct ChannelPublisher
    {
        StateBinding* binding = nullptr;
        uint64_t priority = 0;
        std::chrono::steady_clock::time_point lastHeartbeat;
        std::chrono::steady_clock::time_point lastPublish;
        uint64_t localRevision = 0;
        uint64_t globalRevision = 0;
        bool hasSnapshot = false;
    };

    enum class PublishOwnership
    {
        Rejected,
        Owned,
        Claimed,
        TakenOver
    };

    struct PublishOutcome
    {
        bool accepted = false;
        bool firstRejection = false;
        uint64_t stateOrdinal = 0;
        uint64_t ownerOrdinal = 0;
        PublishOwnership ownership = PublishOwnership::Rejected;
    };

    struct AcquireOutcome
    {
        bool acquired = false;
        bool firstRejection = false;
        uint64_t stateOrdinal = 0;
        uint64_t ownerOrdinal = 0;
        uint64_t ownerPriority = 0;
        PublishOwnership ownership = PublishOwnership::Rejected;
    };

    struct DetachOutcome
    {
        bool detached = false;
        uint64_t stateOrdinal = 0;
        std::size_t releasedChannels = 0;
        GuiLuaBridgeService* service = nullptr;
        bool releasedLastServicePublisher = false;
    };

    StateBinding* Find(ScriptedGuiLuaState* state)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = states.find(state);
        return found == states.end() ? nullptr : found->second.get();
    }

    const StateBinding* Find(ScriptedGuiLuaState* state) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = states.find(state);
        return found == states.end() ? nullptr : found->second.get();
    }

    StateBinding* Add(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api,
        GuiLuaBridgeService& service
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        sharedApi = api;
        const auto existing = states.find(state);
        if (existing != states.end())
        {
            return existing->second.get();
        }
        auto binding = std::make_unique<StateBinding>();
        binding->state = state;
        binding->api = api;
        binding->service = &service;
        binding->ordinal = nextStateOrdinal++;
        binding->lastSeen = std::chrono::steady_clock::now();
        StateBinding* pointer = binding.get();
        states.emplace(state, std::move(binding));
        return pointer;
    }

    DetachOutcome Remove(ScriptedGuiLuaState* state)
    {
        std::lock_guard<std::mutex> lock(mutex);
        DetachOutcome outcome;
        const auto found = states.find(state);
        if (found == states.end())
        {
            return outcome;
        }

        StateBinding* binding = found->second.get();
        outcome.detached = true;
        outcome.stateOrdinal = binding->ordinal;
        outcome.service = binding->service;
        for (auto publisher = channelPublishers.begin();
            publisher != channelPublishers.end();)
        {
            if (publisher->second.binding == binding)
            {
                publisher = channelPublishers.erase(publisher);
                ++outcome.releasedChannels;
            }
            else
            {
                ++publisher;
            }
        }
        if (outcome.releasedChannels > 0)
        {
            outcome.releasedLastServicePublisher = std::none_of(
                channelPublishers.begin(),
                channelPublishers.end(),
                [binding](const auto& publisher)
                {
                    return publisher.second.binding->service
                        == binding->service;
                }
            );
        }
        states.erase(found);
        rejectedPublishers.clear();
        if (states.empty())
        {
            sharedApi = {};
        }
        return outcome;
    }

    std::size_t Count() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return states.size();
    }

    bool Touch(ScriptedGuiLuaState* state)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = states.find(state);
        if (found == states.end())
        {
            return false;
        }
        found->second->lastSeen = std::chrono::steady_clock::now();
        return true;
    }

    std::vector<ScriptedGuiLuaState*> Prune(
        uint64_t maximumIdleMilliseconds
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<ScriptedGuiLuaState*> removed;
        const auto now = std::chrono::steady_clock::now();
        const auto maximumIdle = std::chrono::milliseconds(
            std::max<uint64_t>(1, maximumIdleMilliseconds)
        );
        for (auto state = states.begin(); state != states.end();)
        {
            if (now - state->second->lastSeen < maximumIdle)
            {
                ++state;
                continue;
            }

            StateBinding* binding = state->second.get();
            for (auto publisher = channelPublishers.begin();
                publisher != channelPublishers.end();)
            {
                if (publisher->second.binding == binding)
                {
                    publisher = channelPublishers.erase(publisher);
                }
                else
                {
                    ++publisher;
                }
            }
            removed.push_back(state->first);
            state = states.erase(state);
        }
        if (!removed.empty())
        {
            rejectedPublishers.clear();
        }
        if (states.empty())
        {
            sharedApi = {};
        }
        return removed;
    }

    StateBinding* ResolveCallbackBinding(
        ScriptedGuiLuaState* state
    )
    {
        ScriptedGuiLua51ApiV1 api;
        {
            std::lock_guard<std::mutex> lock(mutex);
            api = sharedApi;
        }
        if (!api.toUserdata)
        {
            return nullptr;
        }
        auto* pointer = static_cast<StateBinding*>(
            api.toUserdata(state, LuaUpvalueOne)
        );
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& entry : states)
        {
            if (entry.second.get() == pointer)
            {
                return pointer;
            }
        }
        return nullptr;
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        states.clear();
        channelPublishers.clear();
        channelRevisions.clear();
        rejectedPublishers.clear();
        attemptedChannels.clear();
        publishedChannels.clear();
        nextStateOrdinal = 1;
        sharedApi = {};
    }

    void ResetChannelOwnership()
    {
        std::lock_guard<std::mutex> lock(mutex);
        channelPublishers.clear();
        rejectedPublishers.clear();
        attemptedChannels.clear();
        publishedChannels.clear();
    }

    bool Empty() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return states.empty();
    }

    bool MarkChannelPublished(const std::string& channel)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return publishedChannels.insert(channel).second;
    }

    bool MarkChannelAttempted(const std::string& channel)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return attemptedChannels.insert(channel).second;
    }

    AcquireOutcome TryAcquireAuthorized(
        ScriptedGuiLuaState* state,
        const std::string& channel,
        uint64_t priority,
        GuiLuaBridgeService& service
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        AcquireOutcome outcome;
        const auto stateIterator = states.find(state);
        if (stateIterator == states.end()
            || stateIterator->second->service != &service)
        {
            return outcome;
        }

        StateBinding* binding = stateIterator->second.get();
        outcome.stateOrdinal = binding->ordinal;
        const auto now = std::chrono::steady_clock::now();
        auto publisher = channelPublishers.find(channel);
        if (publisher == channelPublishers.end())
        {
            channelPublishers[channel] = {
                binding,
                priority,
                now,
                {},
                0,
                channelRevisions[channel],
                false
            };
            outcome.acquired = true;
            outcome.ownership = PublishOwnership::Claimed;
            return outcome;
        }

        if (publisher->second.binding == binding)
        {
            publisher->second.priority = priority;
            publisher->second.lastHeartbeat = now;
            outcome.acquired = true;
            outcome.ownerPriority = priority;
            outcome.ownership = PublishOwnership::Owned;
            return outcome;
        }

        outcome.ownerOrdinal = publisher->second.binding->ordinal;
        outcome.ownerPriority = publisher->second.priority;
        const bool stale = now - publisher->second.lastHeartbeat
            >= PublisherLease;
        if (priority > publisher->second.priority
            || (stale && priority == publisher->second.priority))
        {
            publisher->second = {
                binding,
                priority,
                now,
                {},
                0,
                channelRevisions[channel],
                false
            };
            outcome.acquired = true;
            outcome.ownership = PublishOwnership::TakenOver;
            rejectedPublishers.clear();
            return outcome;
        }

        outcome.firstRejection = rejectedPublishers.insert(
            "acquire#" + channel + "#"
                + std::to_string(binding->ordinal)
        ).second;
        return outcome;
    }

    bool ReleaseAuthorized(
        ScriptedGuiLuaState* state,
        const std::string& channel,
        GuiLuaBridgeService& service,
        uint64_t& stateOrdinal
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto stateIterator = states.find(state);
        const auto publisher = channelPublishers.find(channel);
        if (stateIterator == states.end()
            || stateIterator->second->service != &service
            || publisher == channelPublishers.end()
            || publisher->second.binding != stateIterator->second.get())
        {
            return false;
        }
        stateOrdinal = stateIterator->second->ordinal;
        channelPublishers.erase(publisher);
        rejectedPublishers.clear();
        return true;
    }

    PublishOutcome PublishAuthorized(
        ScriptedGuiLuaState* state,
        const std::string& channel,
        GuiDataBridgeUpdate update,
        GuiLuaBridgeService& service,
        std::string& error
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        PublishOutcome outcome;
        const auto stateIterator = states.find(state);
        if (stateIterator == states.end()
            || stateIterator->second->service != &service)
        {
            error = "lua_channel_publisher_state_unknown";
            return outcome;
        }

        StateBinding* binding = stateIterator->second.get();
        outcome.stateOrdinal = binding->ordinal;
        const auto now = std::chrono::steady_clock::now();
        auto publisher = channelPublishers.find(channel);
        const bool unowned = publisher == channelPublishers.end();
        const bool owned = !unowned
            && publisher->second.binding == binding;
        const bool stale = !unowned
            && now - publisher->second.lastHeartbeat >= PublisherLease;
        const bool canClaim = unowned && update.fullSnapshot;
        const bool canTakeOver = !unowned
            && !owned
            && stale
            && update.fullSnapshot;
        if (!owned && !canClaim && !canTakeOver)
        {
            outcome.ownerOrdinal = unowned
                ? 0
                : publisher->second.binding->ordinal;
            outcome.firstRejection = rejectedPublishers.insert(
                channel + "#" + std::to_string(binding->ordinal)
            ).second;
            error = unowned
                ? "lua_channel_first_snapshot_must_be_full"
                : "lua_channel_publisher_not_owner";
            return outcome;
        }

        ChannelPublisher candidate;
        ChannelPublisher* activePublisher = nullptr;
        if (owned)
        {
            activePublisher = &publisher->second;
        }
        else
        {
            candidate.binding = binding;
            candidate.priority = 0;
            candidate.lastHeartbeat = now;
            candidate.globalRevision = channelRevisions[channel];
            activePublisher = &candidate;
        }

        const uint64_t localRevision = update.revision;
        const uint64_t nextGlobalRevision = channelRevisions[channel] + 1;
        if (nextGlobalRevision == 0)
        {
            error = "lua_channel_revision_overflow";
            return outcome;
        }
        if (!update.fullSnapshot)
        {
            if (!activePublisher->hasSnapshot)
            {
                error = "lua_channel_delta_requires_owned_snapshot";
                return outcome;
            }
            if (update.baseRevision != activePublisher->localRevision
                || update.revision <= update.baseRevision)
            {
                error = "lua_channel_local_revision_gap";
                return outcome;
            }
            update.baseRevision = activePublisher->globalRevision;
        }
        else
        {
            update.baseRevision = 0;
        }
        update.revision = nextGlobalRevision;

        outcome.accepted = service.PublishUpdate(
            channel,
            std::move(update),
            error
        );
        if (!outcome.accepted)
        {
            return outcome;
        }

        activePublisher->binding = binding;
        activePublisher->lastHeartbeat = now;
        activePublisher->lastPublish = now;
        activePublisher->localRevision = localRevision;
        activePublisher->globalRevision = nextGlobalRevision;
        activePublisher->hasSnapshot = true;
        channelRevisions[channel] = nextGlobalRevision;

        if (canClaim)
        {
            channelPublishers[channel] = candidate;
            outcome.ownership = PublishOwnership::Claimed;
            rejectedPublishers.clear();
        }
        else if (canTakeOver)
        {
            outcome.ownerOrdinal = publisher->second.binding->ordinal;
            publisher->second = candidate;
            outcome.ownership = PublishOwnership::TakenOver;
            rejectedPublishers.clear();
        }
        else
        {
            outcome.ownership = PublishOwnership::Owned;
        }
        return outcome;
    }

    bool TryPopAuthorizedAction(
        ScriptedGuiLuaState* state,
        const std::string& channel,
        GuiLuaBridgeService& service,
        GuiActionContext& context
    )
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto stateIterator = states.find(state);
        const auto publisher = channelPublishers.find(channel);
        return stateIterator != states.end()
            && stateIterator->second->service == &service
            && publisher != channelPublishers.end()
            && publisher->second.binding == stateIterator->second.get()
            && service.TryPopAction(channel, context);
    }

    mutable std::mutex mutex;
    ScriptedGuiLua51ApiV1 sharedApi{};
    std::unordered_map<
        ScriptedGuiLuaState*,
        std::unique_ptr<StateBinding>
    > states;
    std::unordered_map<std::string, ChannelPublisher>
        channelPublishers;
    std::unordered_map<std::string, uint64_t> channelRevisions;
    std::unordered_set<std::string> rejectedPublishers;
    std::unordered_set<std::string> attemptedChannels;
    std::unordered_set<std::string> publishedChannels;
    uint64_t nextStateOrdinal = 1;
};

GuiLuaNativeBinding::GuiLuaNativeBinding()
    : impl_(std::make_unique<Impl>())
{
}

GuiLuaNativeBinding::~GuiLuaNativeBinding()
{
    DetachAll();
}

bool GuiLuaNativeBinding::Install(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    GuiLuaBridgeService& service,
    std::string& error
)
{
    if (!state || !IsComplete(api))
    {
        error = "lua51_native_api_invalid";
        return false;
    }
    if (impl_->Find(state))
    {
        error.clear();
        return true;
    }
    const bool replacingStaleTable = HasNativeTable(state, api);
    Impl::StateBinding* bindingPointer = impl_->Add(
        state,
        api,
        service
    );
    ActiveBinding.store(this, std::memory_order_release);

    LuaStackGuard guard(state, api);
    api.createTable(state, 0, 4);
    const int nativeTable = AbsoluteIndex(state, api, -1);
    api.pushLightUserdata(state, bindingPointer);
    api.pushCClosure(state, &TryAcquireChannelThunk, 1);
    api.setField(state, nativeTable, "TryAcquireChannel");
    api.pushLightUserdata(state, bindingPointer);
    api.pushCClosure(state, &ReleaseChannelThunk, 1);
    api.setField(state, nativeTable, "ReleaseChannel");
    api.pushLightUserdata(state, bindingPointer);
    api.pushCClosure(state, &PublishUpdateThunk, 1);
    api.setField(state, nativeTable, "PublishUpdate");
    api.pushLightUserdata(state, bindingPointer);
    api.pushCClosure(state, &TryPopActionThunk, 1);
    api.setField(state, nativeTable, "TryPopAction");
    api.setField(state, LuaGlobalsIndex, "ScriptedGuiNative");
    if (replacingStaleTable)
    {
        WriteGuiDiagnostic(
            "Replaced stale ScriptedGuiNative Lua table"
        );
    }
    error.clear();
    return true;
}

bool GuiLuaNativeBinding::DetachState(ScriptedGuiLuaState* state)
{
    const Impl::DetachOutcome outcome = impl_->Remove(state);
    if (!outcome.detached)
    {
        return false;
    }
    WriteGuiDiagnostic(
        "Lua 5.1 state detached from ScriptedGuiNative: state="
        + std::to_string(outcome.stateOrdinal)
        + ", releasedChannels="
        + std::to_string(outcome.releasedChannels)
    );
    if (outcome.releasedLastServicePublisher && outcome.service)
    {
        outcome.service->ReportGameplayPlayerTag("---");
        WriteGuiDiagnostic(
            "Lua lifecycle changed by publisher detach: state=frontend"
        );
    }
    if (impl_->Empty())
    {
        GuiLuaNativeBinding* expected = this;
        ActiveBinding.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel
        );
    }
    return true;
}

bool GuiLuaNativeBinding::TouchState(ScriptedGuiLuaState* state)
{
    return impl_->Touch(state);
}

std::vector<ScriptedGuiLuaState*>
GuiLuaNativeBinding::PruneInactiveStates(
    uint64_t maximumIdleMilliseconds
)
{
    std::vector<ScriptedGuiLuaState*> removed = impl_->Prune(
        maximumIdleMilliseconds
    );
    if (!removed.empty())
    {
        WriteGuiDiagnostic(
            "Pruned inactive Lua 5.1 states: count="
            + std::to_string(removed.size())
        );
    }
    if (impl_->Empty())
    {
        GuiLuaNativeBinding* expected = this;
        ActiveBinding.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel
        );
    }
    return removed;
}

void GuiLuaNativeBinding::DetachAll()
{
    GuiLuaNativeBinding* expected = this;
    ActiveBinding.compare_exchange_strong(
        expected,
        nullptr,
        std::memory_order_acq_rel
    );
    impl_->Clear();
}

void GuiLuaNativeBinding::ResetChannelOwnership()
{
    impl_->ResetChannelOwnership();
    WriteGuiDiagnostic("Lua GUI channel ownership reset");
}

bool GuiLuaNativeBinding::IsInstalled() const
{
    return !impl_->Empty();
}

bool GuiLuaNativeBinding::IsStateInstalled(
    ScriptedGuiLuaState* state
) const
{
    return impl_->Find(state) != nullptr;
}

std::size_t GuiLuaNativeBinding::StateCount() const
{
    return impl_->Count();
}

int __cdecl GuiLuaNativeBinding::TryAcquireChannelThunk(
    ScriptedGuiLuaState* state
)
{
    GuiLuaNativeBinding* owner = ActiveBinding.load(
        std::memory_order_acquire
    );
    Impl::StateBinding* binding = owner
        ? owner->impl_->ResolveCallbackBinding(state)
        : nullptr;
    if (!binding || !binding->service)
    {
        return 0;
    }
    try
    {
        return owner->TryAcquireChannel(
            state,
            binding->api,
            *binding->service
        );
    }
    catch (const std::exception& exception)
    {
        try
        {
            WriteGuiDiagnostic(
                std::string("Lua TryAcquireChannel exception: ")
                + exception.what()
            );
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
        try
        {
            WriteGuiDiagnostic(
                "Lua TryAcquireChannel unknown exception"
            );
        }
        catch (...)
        {
        }
    }
    binding->api.pushBoolean(state, 0);
    PushString(state, binding->api, "exception");
    return 2;
}

int __cdecl GuiLuaNativeBinding::ReleaseChannelThunk(
    ScriptedGuiLuaState* state
)
{
    GuiLuaNativeBinding* owner = ActiveBinding.load(
        std::memory_order_acquire
    );
    Impl::StateBinding* binding = owner
        ? owner->impl_->ResolveCallbackBinding(state)
        : nullptr;
    if (!binding || !binding->service)
    {
        return 0;
    }
    try
    {
        return owner->ReleaseChannel(
            state,
            binding->api,
            *binding->service
        );
    }
    catch (const std::exception& exception)
    {
        try
        {
            WriteGuiDiagnostic(
                std::string("Lua ReleaseChannel exception: ")
                + exception.what()
            );
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
        try
        {
            WriteGuiDiagnostic("Lua ReleaseChannel unknown exception");
        }
        catch (...)
        {
        }
    }
    binding->api.pushBoolean(state, 0);
    return 1;
}

int __cdecl GuiLuaNativeBinding::PublishUpdateThunk(
    ScriptedGuiLuaState* state
)
{
    GuiLuaNativeBinding* owner = ActiveBinding.load(
        std::memory_order_acquire
    );
    Impl::StateBinding* binding = owner
        ? owner->impl_->ResolveCallbackBinding(state)
        : nullptr;
    if (!binding
        || !binding->service)
    {
        return 0;
    }
    try
    {
        return owner->PublishUpdate(
            state,
            binding->api,
            *binding->service
        );
    }
    catch (const std::exception& exception)
    {
        try
        {
            WriteGuiDiagnostic(
                std::string("Lua PublishUpdate exception: ")
                + exception.what()
            );
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
        try
        {
            WriteGuiDiagnostic("Lua PublishUpdate unknown exception");
        }
        catch (...)
        {
        }
    }
    binding->api.pushBoolean(state, 0);
    return 1;
}

int __cdecl GuiLuaNativeBinding::TryPopActionThunk(
    ScriptedGuiLuaState* state
)
{
    GuiLuaNativeBinding* owner = ActiveBinding.load(
        std::memory_order_acquire
    );
    Impl::StateBinding* binding = owner
        ? owner->impl_->ResolveCallbackBinding(state)
        : nullptr;
    if (!binding
        || !binding->service)
    {
        return 0;
    }
    try
    {
        return owner->TryPopAction(
            state,
            binding->api,
            *binding->service
        );
    }
    catch (const std::exception& exception)
    {
        try
        {
            WriteGuiDiagnostic(
                std::string("Lua TryPopAction exception: ")
                + exception.what()
            );
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
        try
        {
            WriteGuiDiagnostic("Lua TryPopAction unknown exception");
        }
        catch (...)
        {
        }
    }
    binding->api.pushNil(state);
    return 1;
}

int GuiLuaNativeBinding::TryAcquireChannel(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    GuiLuaBridgeService& service
)
{
    std::string channel;
    uint64_t priority = 0;
    if (api.getTop(state) < 1
        || !ReadString(state, api, 1, channel)
        || channel.empty()
        || (api.getTop(state) >= 2
            && !ReadUnsigned(state, api, 2, priority)))
    {
        api.pushBoolean(state, 0);
        PushString(state, api, "invalid");
        return 2;
    }

    channel = NormalizeName(std::move(channel));
    const Impl::AcquireOutcome outcome = impl_->TryAcquireAuthorized(
        state,
        channel,
        priority,
        service
    );
    std::string_view status = "rejected";
    if (outcome.ownership == Impl::PublishOwnership::Claimed)
    {
        status = "claimed";
        WriteGuiDiagnostic(
            "Lua GUI channel publisher reserved: channel="
            + channel
            + ", state=" + std::to_string(outcome.stateOrdinal)
            + ", priority=" + std::to_string(priority)
        );
    }
    else if (outcome.ownership == Impl::PublishOwnership::TakenOver)
    {
        status = "taken_over";
        WriteGuiDiagnostic(
            "Lua GUI channel publisher preempted: channel="
            + channel
            + ", previousState="
            + std::to_string(outcome.ownerOrdinal)
            + ", previousPriority="
            + std::to_string(outcome.ownerPriority)
            + ", state=" + std::to_string(outcome.stateOrdinal)
            + ", priority=" + std::to_string(priority)
        );
    }
    else if (outcome.ownership == Impl::PublishOwnership::Owned)
    {
        status = "owned";
    }

    api.pushBoolean(state, outcome.acquired ? 1 : 0);
    PushString(state, api, status);
    api.pushNumber(
        state,
        static_cast<double>(outcome.stateOrdinal)
    );
    return 3;
}

int GuiLuaNativeBinding::ReleaseChannel(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    GuiLuaBridgeService& service
)
{
    std::string channel;
    uint64_t stateOrdinal = 0;
    const bool released = api.getTop(state) >= 1
        && ReadString(state, api, 1, channel)
        && !channel.empty()
        && impl_->ReleaseAuthorized(
            state,
            NormalizeName(channel),
            service,
            stateOrdinal
        );
    if (released)
    {
        WriteGuiDiagnostic(
            "Lua GUI channel publisher released: channel="
            + NormalizeName(std::move(channel))
            + ", state=" + std::to_string(stateOrdinal)
        );
    }
    api.pushBoolean(state, released ? 1 : 0);
    return 1;
}

int GuiLuaNativeBinding::PublishUpdate(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    GuiLuaBridgeService& service
)
{
    bool accepted = false;
    if (api.getTop(state) >= 2)
    {
        std::string channel;
        GuiDataBridgeUpdate update;
        std::string error;
        if (ReadString(state, api, 1, channel) && !channel.empty())
        {
            channel = NormalizeName(std::move(channel));
            std::string lifecyclePlayerTag;
            const bool firstAttempt =
                impl_->MarkChannelAttempted(channel);
            if (firstAttempt)
            {
                WriteGuiDiagnostic(
                    "First Lua GUI snapshot decode started: channel="
                    + channel
                );
            }
            if (DecodeUpdate(state, api, 2, update, error))
            {
                const auto viewer = update.values.find("state.viewertag");
                if (viewer != update.values.end())
                {
                    lifecyclePlayerTag = GuiDataValueToText(
                        viewer->second
                    );
                }
                if (firstAttempt)
                {
                    const auto valueText = [&update](
                        const char* name
                    )
                    {
                        const auto found = update.values.find(name);
                        return found == update.values.end()
                            ? std::string("<missing>")
                            : GuiDataValueToText(found->second);
                    };
                    WriteGuiDiagnostic(
                        "First Lua GUI snapshot state: channel="
                        + channel
                        + ", visible="
                        + valueText("state.visible")
                        + ", active="
                        + valueText("state.active")
                        + ", viewer="
                        + valueText("state.viewertag")
                        + ", values="
                        + std::to_string(update.values.size())
                        + ", lists="
                        + std::to_string(update.lists.size())
                    );
                    std::size_t regionValueCount = 0;
                    std::size_t nonzeroRegionCount = 0;
                    double maximumRegionValue = 0.0;
                    std::string maximumRegionName;
                    constexpr std::string_view regionPrefix = "regions.";
                    constexpr std::string_view regionSuffix =
                        ".controlledpercentage";
                    for (const auto& [name, value] : update.values)
                    {
                        if (name.size()
                                <= regionPrefix.size()
                                    + regionSuffix.size()
                            || name.compare(
                                0,
                                regionPrefix.size(),
                                regionPrefix
                            ) != 0
                            || name.compare(
                                name.size() - regionSuffix.size(),
                                regionSuffix.size(),
                                regionSuffix
                            ) != 0)
                        {
                            continue;
                        }
                        ++regionValueCount;
                        const double percentage =
                            GuiDataValueToNumber(value);
                        if (percentage > 0.0)
                        {
                            ++nonzeroRegionCount;
                        }
                        if (percentage > maximumRegionValue)
                        {
                            maximumRegionValue = percentage;
                            maximumRegionName = name;
                        }
                    }
                    WriteGuiDiagnostic(
                        "First Lua GUI Region values: channel="
                        + channel
                        + ", count="
                        + std::to_string(regionValueCount)
                        + ", nonzero="
                        + std::to_string(nonzeroRegionCount)
                        + ", maximum="
                        + std::to_string(maximumRegionValue)
                        + ", maximumKey="
                        + (maximumRegionName.empty()
                            ? std::string("<none>")
                            : maximumRegionName)
                    );
                }
                const uint64_t revision = update.revision;
                const Impl::PublishOutcome outcome =
                    impl_->PublishAuthorized(
                        state,
                        channel,
                        std::move(update),
                        service,
                        error
                    );
                accepted = outcome.accepted;
                if (outcome.ownership
                    == Impl::PublishOwnership::Claimed)
                {
                    WriteGuiDiagnostic(
                        "Lua GUI channel publisher claimed: channel="
                        + channel
                        + ", state="
                        + std::to_string(outcome.stateOrdinal)
                    );
                }
                else if (outcome.ownership
                    == Impl::PublishOwnership::TakenOver)
                {
                    WriteGuiDiagnostic(
                        "Lua GUI channel publisher changed: channel="
                        + channel
                        + ", previousState="
                        + std::to_string(outcome.ownerOrdinal)
                        + ", state="
                        + std::to_string(outcome.stateOrdinal)
                    );
                }
                else if (!accepted && outcome.firstRejection)
                {
                    WriteGuiDiagnostic(
                        "Lua GUI channel publisher rejected: channel="
                        + channel
                        + ", state="
                        + std::to_string(outcome.stateOrdinal)
                        + ", ownerState="
                        + std::to_string(outcome.ownerOrdinal)
                        + ", error="
                        + error
                    );
                }
                if (accepted
                    && !lifecyclePlayerTag.empty()
                    && lifecyclePlayerTag != "---"
                    && service.ReportGameplayPlayerTag(
                        lifecyclePlayerTag
                    ))
                {
                    const GuiGameplayLifecycleSnapshot lifecycle =
                        service.GameplayLifecycle();
                    WriteGuiDiagnostic(
                        "Lua lifecycle changed by snapshot: player="
                        + lifecycle.playerTag
                        + ", state=gameplay, generation="
                        + std::to_string(lifecycle.generation)
                    );
                }
                if (accepted && impl_->MarkChannelPublished(channel))
                {
                    WriteGuiDiagnostic(
                        "First Lua GUI snapshot received: channel="
                        + channel
                        + ", revision="
                        + std::to_string(revision)
                    );
                }
            }
            else
            {
                WriteGuiDiagnostic(
                    "Lua GUI snapshot decode rejected: channel="
                    + channel
                    + ", error="
                    + error
                );
            }
        }
    }
    api.pushBoolean(state, accepted ? 1 : 0);
    return 1;
}

int GuiLuaNativeBinding::TryPopAction(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    GuiLuaBridgeService& service
)
{
    std::string channel;
    GuiActionContext context;
    if (api.getTop(state) < 1
        || !ReadString(state, api, 1, channel)
        || channel.empty()
        || !impl_->TryPopAuthorizedAction(
            state,
            NormalizeName(std::move(channel)),
            service,
            context
        ))
    {
        api.pushNil(state);
        return 1;
    }
    PushAction(state, api, context);
    return 1;
}

GuiLuaNativeBinding& GetGuiLuaNativeBinding()
{
    static GuiLuaNativeBinding binding;
    return binding;
}
