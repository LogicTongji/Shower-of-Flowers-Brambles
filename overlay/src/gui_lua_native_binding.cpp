#include "gui_lua_native_binding.h"

#include <algorithm>
#include <atomic>
#include <cctype>
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
    api.getField(state, tableIndex, "PublishUpdate");
    const bool hasPublish = api.type(state, -1) == LuaTypeFunction;
    Pop(state, api);
    api.getField(state, tableIndex, "TryPopAction");
    const bool hasActions = api.type(state, -1) == LuaTypeFunction;
    return hasPublish && hasActions;
}

}

struct GuiLuaNativeBinding::Impl
{
    struct StateBinding
    {
        ScriptedGuiLuaState* state = nullptr;
        ScriptedGuiLua51ApiV1 api{};
        GuiLuaBridgeService* service = nullptr;
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
        StateBinding* pointer = binding.get();
        states.emplace(state, std::move(binding));
        return pointer;
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
        attemptedChannels.clear();
        publishedChannels.clear();
        sharedApi = {};
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

    mutable std::mutex mutex;
    ScriptedGuiLua51ApiV1 sharedApi{};
    std::unordered_map<
        ScriptedGuiLuaState*,
        std::unique_ptr<StateBinding>
    > states;
    std::unordered_set<std::string> attemptedChannels;
    std::unordered_set<std::string> publishedChannels;
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
    if (HasNativeTable(state, api))
    {
        error.clear();
        return true;
    }
    Impl::StateBinding* bindingPointer = impl_->Add(
        state,
        api,
        service
    );
    ActiveBinding.store(this, std::memory_order_release);

    LuaStackGuard guard(state, api);
    api.createTable(state, 0, 2);
    const int nativeTable = AbsoluteIndex(state, api, -1);
    api.pushLightUserdata(state, bindingPointer);
    api.pushCClosure(state, &PublishUpdateThunk, 1);
    api.setField(state, nativeTable, "PublishUpdate");
    api.pushLightUserdata(state, bindingPointer);
    api.pushCClosure(state, &TryPopActionThunk, 1);
    api.setField(state, nativeTable, "TryPopAction");
    api.setField(state, LuaGlobalsIndex, "ScriptedGuiNative");
    error.clear();
    return true;
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
                accepted = service.PublishUpdate(
                    channel,
                    std::move(update),
                    error
                );
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
        || !service.TryPopAction(channel, context))
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
