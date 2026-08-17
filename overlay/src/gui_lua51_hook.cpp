#include "gui_lua51_hook.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "gui_lua_bridge.h"
#include "gui_lua_native_binding.h"
#include "gui_diagnostics.h"
#ifdef SCRIPTED_GUI_OVERLAY_EXPORTS
#include "gui_d3d9_hook.h"
#endif

namespace
{

using LuaPCallFunction = int (__cdecl *)(
    ScriptedGuiLuaState*,
    int,
    int,
    int
);
using LuaLLoadBufferFunction = int (__cdecl *)(
    ScriptedGuiLuaState*,
    const char*,
    std::size_t,
    const char*
);
using LuaCloseFunction = void (__cdecl *)(ScriptedGuiLuaState*);
using LuaNewStateFunction = ScriptedGuiLuaState* (__cdecl *)();

std::mutex HookMutex;
std::mutex AttachMutex;
std::mutex DiagnosticMutex;
void** LuaPCallSlot = nullptr;
LuaPCallFunction OriginalLuaPCall = nullptr;
void** LuaLLoadBufferSlot = nullptr;
LuaLLoadBufferFunction OriginalLuaLLoadBuffer = nullptr;
void** LuaCloseSlot = nullptr;
LuaCloseFunction OriginalLuaClose = nullptr;
void** LuaNewStateSlot = nullptr;
LuaNewStateFunction OriginalLuaNewState = nullptr;
std::unordered_set<ScriptedGuiLuaState*> ObservedStates;
std::unordered_set<std::string> ObservedChunks;
std::size_t LoggedLuaErrors = 0;
constexpr std::size_t MaximumLoggedChunks = 128;
constexpr std::size_t MaximumLoggedLuaErrors = 64;
constexpr uint64_t StatePruneInterval = 4096;
constexpr uint64_t MaximumStateIdleMilliseconds = 5 * 60 * 1000;
std::atomic<uint64_t> LuaCallCount{0};

template <typename Function>
bool ResolveFunction(
    HMODULE module,
    const char* name,
    Function& output,
    std::string& error
)
{
    output = reinterpret_cast<Function>(GetProcAddress(module, name));
    if (!output)
    {
        error = std::string("lua51_export_missing: ") + name;
        return false;
    }
    return true;
}

bool IsReadableImage(const uint8_t* base)
{
    if (!base)
    {
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
    {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew
    );
    return nt->Signature == IMAGE_NT_SIGNATURE
        && nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
}

void** FindImportSlot(
    HMODULE image,
    const char* importedModule,
    const char* functionName
)
{
    auto* base = reinterpret_cast<uint8_t*>(image);
    if (!IsReadableImage(base))
    {
        return nullptr;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew
    );
    const IMAGE_DATA_DIRECTORY& directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0 || directory.Size == 0)
    {
        return nullptr;
    }
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + directory.VirtualAddress
    );
    for (; descriptor->Name != 0; ++descriptor)
    {
        const char* moduleName = reinterpret_cast<const char*>(
            base + descriptor->Name
        );
        if (_stricmp(moduleName, importedModule) != 0)
        {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + (descriptor->OriginalFirstThunk != 0
                ? descriptor->OriginalFirstThunk
                : descriptor->FirstThunk)
        );
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + descriptor->FirstThunk
        );
        for (; names->u1.AddressOfData != 0; ++names, ++addresses)
        {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
            {
                continue;
            }
            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                base + names->u1.AddressOfData
            );
            if (std::strcmp(
                    reinterpret_cast<const char*>(import->Name),
                    functionName
                ) == 0)
            {
                return reinterpret_cast<void**>(&addresses->u1.Function);
            }
        }
    }
    return nullptr;
}

bool ExchangeImportSlot(
    void** slot,
    void* replacement,
    void** previous,
    std::string& error
)
{
    DWORD oldProtection = 0;
    if (!slot
        || !VirtualProtect(
            slot,
            sizeof(void*),
            PAGE_READWRITE,
            &oldProtection
        ))
    {
        error = "lua51_iat_virtual_protect_failed";
        return false;
    }
    void* oldValue = InterlockedExchangePointer(slot, replacement);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    if (previous)
    {
        *previous = oldValue;
    }
    error.clear();
    return true;
}

void PruneInactiveLuaStates()
{
    std::lock_guard<std::mutex> lock(AttachMutex);
    const std::vector<ScriptedGuiLuaState*> removed =
        GetGuiLuaNativeBinding().PruneInactiveStates(
            MaximumStateIdleMilliseconds
        );
    for (ScriptedGuiLuaState* state : removed)
    {
        ObservedStates.erase(state);
    }
}

ScriptedGuiLuaState* __cdecl HookedLuaNewState()
{
    LuaNewStateFunction original = OriginalLuaNewState;
    ScriptedGuiLuaState* state = original ? original() : nullptr;
    if (state)
    {
        try
        {
            std::lock_guard<std::mutex> lock(AttachMutex);
            ObservedStates.erase(state);
            GetGuiLuaNativeBinding().DetachState(state);
        }
        catch (...)
        {
        }
    }
    return state;
}

int __cdecl HookedLuaPCall(
    ScriptedGuiLuaState* state,
    int argumentCount,
    int resultCount,
    int errorFunction
)
{
#ifdef SCRIPTED_GUI_OVERLAY_EXPORTS
    MaintainGuiD3D9Hooks();
#endif
    try
    {
        std::string ignoredError;
        AttachGuiLua51State(state, ignoredError);
    }
    catch (...)
    {
    }
    if (LuaCallCount.fetch_add(1, std::memory_order_relaxed) %
            StatePruneInterval
        == StatePruneInterval - 1)
    {
        try
        {
            PruneInactiveLuaStates();
        }
        catch (...)
        {
        }
    }
    LuaPCallFunction original = OriginalLuaPCall;
    const int result = original
        ? original(state, argumentCount, resultCount, errorFunction)
        : -1;
    if (result != 0)
    {
        ScriptedGuiLua51ApiV1 api;
        std::string ignoredError;
        if (ResolveGuiLua51Api(api, ignoredError))
        {
            std::size_t length = 0;
            const char* message = api.toLString(state, -1, &length);
            std::lock_guard<std::mutex> lock(DiagnosticMutex);
            if (LoggedLuaErrors < MaximumLoggedLuaErrors)
            {
                ++LoggedLuaErrors;
                WriteGuiDiagnostic(
                    "Lua pcall failed: "
                    + std::string(
                        message ? message : "unknown Lua error",
                        message ? length : 17
                    )
                );
            }
        }
    }
    return result;
}

void __cdecl HookedLuaClose(ScriptedGuiLuaState* state)
{
    try
    {
        std::lock_guard<std::mutex> lock(AttachMutex);
        ObservedStates.erase(state);
        GetGuiLuaNativeBinding().DetachState(state);
    }
    catch (...)
    {
    }
    LuaCloseFunction original = OriginalLuaClose;
    if (original)
    {
        original(state);
    }
}

int __cdecl HookedLuaLLoadBuffer(
    ScriptedGuiLuaState* state,
    const char* buffer,
    std::size_t size,
    const char* name
)
{
    std::string chunkName = name ? name : "<unnamed>";
    bool relevantBuffer = false;
    if (buffer && size > 0)
    {
        const std::string_view source(buffer, size);
        relevantBuffer = source.find("war_map_adapter")
                != std::string_view::npos
            || source.find("gui_data_bridge")
                != std::string_view::npos
            || source.find("UpdateChinaWarMapOncePerDay")
                != std::string_view::npos;
    }
    {
        std::lock_guard<std::mutex> lock(DiagnosticMutex);
        const bool first = ObservedChunks.insert(chunkName).second;
        if ((first && ObservedChunks.size() <= MaximumLoggedChunks)
            || relevantBuffer)
        {
            if (chunkName.size() > 240)
            {
                chunkName.resize(240);
            }
            WriteGuiDiagnostic(
                std::string("Lua chunk loaded: ")
                + chunkName
                + (relevantBuffer ? " [Scripted GUI bootstrap found]" : "")
            );
        }
    }
    const LuaLLoadBufferFunction original = OriginalLuaLLoadBuffer;
    return original ? original(state, buffer, size, name) : -1;
}

}

bool ResolveGuiLua51Api(
    ScriptedGuiLua51ApiV1& api,
    std::string& error
)
{
    api = {};
    HMODULE module = GetModuleHandleW(L"lua51.dll");
    if (!module)
    {
        module = GetModuleHandleW(L"lua5.1.dll");
    }
    if (!module)
    {
        error = "lua51_module_not_loaded";
        return false;
    }
    api.size = sizeof(api);
    api.version = SCRIPTED_GUI_LUA51_API_VERSION;
    return ResolveFunction(module, "lua_gettop", api.getTop, error)
        && ResolveFunction(module, "lua_settop", api.setTop, error)
        && ResolveFunction(module, "lua_type", api.type, error)
        && ResolveFunction(module, "lua_toboolean", api.toBoolean, error)
        && ResolveFunction(module, "lua_tonumber", api.toNumber, error)
        && ResolveFunction(module, "lua_tolstring", api.toLString, error)
        && ResolveFunction(module, "lua_touserdata", api.toUserdata, error)
        && ResolveFunction(module, "lua_pushnil", api.pushNil, error)
        && ResolveFunction(module, "lua_pushboolean", api.pushBoolean, error)
        && ResolveFunction(module, "lua_pushnumber", api.pushNumber, error)
        && ResolveFunction(module, "lua_pushlstring", api.pushLString, error)
        && ResolveFunction(
            module,
            "lua_pushlightuserdata",
            api.pushLightUserdata,
            error
        )
        && ResolveFunction(
            module,
            "lua_pushcclosure",
            api.pushCClosure,
            error
        )
        && ResolveFunction(module, "lua_createtable", api.createTable, error)
        && ResolveFunction(module, "lua_getfield", api.getField, error)
        && ResolveFunction(module, "lua_setfield", api.setField, error)
        && ResolveFunction(module, "lua_objlen", api.objLen, error)
        && ResolveFunction(module, "lua_rawgeti", api.rawGetI, error)
        && ResolveFunction(module, "lua_next", api.next, error);
}

bool ProbeGuiLua51LifecycleState(
    ScriptedGuiLuaState* state,
    GuiLua51LifecycleObservation& observation,
    std::string& error
)
{
    observation = {};
    if (!state)
    {
        error = "lua51_lifecycle_state_missing";
        return false;
    }
    ScriptedGuiLua51ApiV1 api;
    if (!ResolveGuiLua51Api(api, error))
    {
        return false;
    }
    HMODULE module = GetModuleHandleW(L"lua51.dll");
    if (!module)
    {
        module = GetModuleHandleW(L"lua5.1.dll");
    }
    LuaLLoadBufferFunction loadBuffer = nullptr;
    LuaPCallFunction pcall = nullptr;
    if (!module
        || !ResolveFunction(module, "luaL_loadbuffer", loadBuffer, error)
        || !ResolveFunction(module, "lua_pcall", pcall, error))
    {
        return false;
    }

    static constexpr char Source[] =
        "local playerOk,player=pcall(function() "
        "return CCurrentGameState.GetPlayer() end);"
        "local playerTag='';"
        "if playerOk and player~=nil then "
        "local textOk,text=pcall(tostring,player);"
        "if textOk and text~=nil then playerTag=text end end;"
        "return playerOk,playerTag";
    const int originalTop = api.getTop(state);
    if (loadBuffer(
            state,
            Source,
            sizeof(Source) - 1,
            "@scripted_gui_lifecycle_probe"
        ) != 0)
    {
        std::size_t length = 0;
        const char* message = api.toLString(state, -1, &length);
        error = "lua51_lifecycle_load_failed: "
            + std::string(
                message ? message : "unknown Lua error",
                message ? length : 17
            );
        api.setTop(state, originalTop);
        return false;
    }
    if (pcall(state, 0, 2, 0) != 0)
    {
        std::size_t length = 0;
        const char* message = api.toLString(state, -1, &length);
        error = "lua51_lifecycle_call_failed: "
            + std::string(
                message ? message : "unknown Lua error",
                message ? length : 17
            );
        api.setTop(state, originalTop);
        return false;
    }

    observation.playerQuerySucceeded = api.toBoolean(state, -2) != 0;
    if (api.type(state, -1) == 4)
    {
        std::size_t length = 0;
        const char* text = api.toLString(state, -1, &length);
        if (text)
        {
            observation.playerTag.assign(text, length);
        }
    }
    api.setTop(state, originalTop);
    error.clear();
    return true;
}

bool AttachGuiLua51State(
    ScriptedGuiLuaState* state,
    std::string& error
)
{
    if (!state)
    {
        error = "lua51_state_missing";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(AttachMutex);
        if (ObservedStates.find(state) != ObservedStates.end())
        {
            if (GetGuiLuaNativeBinding().TouchState(state))
            {
                error.clear();
                return true;
            }
            ObservedStates.erase(state);
        }
    }

    ScriptedGuiLua51ApiV1 api;
    if (!ResolveGuiLua51Api(api, error))
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(AttachMutex);
    if (ObservedStates.find(state) != ObservedStates.end())
    {
        if (GetGuiLuaNativeBinding().TouchState(state))
        {
            error.clear();
            return true;
        }
        ObservedStates.erase(state);
    }
    if (!GetGuiLuaNativeBinding().Install(
            state,
            api,
            GetGuiLuaBridgeService(),
            error
        ))
    {
        return false;
    }
    ObservedStates.insert(state);
    GetGuiLuaNativeBinding().TouchState(state);
    WriteGuiDiagnostic("Lua 5.1 state attached to ScriptedGuiNative");
    return true;
}

bool InstallGuiLua51Hooks(std::string& error)
{
    std::lock_guard<std::mutex> lock(HookMutex);
    if (LuaPCallSlot
        && OriginalLuaPCall
        && LuaLLoadBufferSlot
        && OriginalLuaLLoadBuffer
        && LuaNewStateSlot
        && OriginalLuaNewState)
    {
        error.clear();
        return true;
    }
    ScriptedGuiLua51ApiV1 api;
    if (!ResolveGuiLua51Api(api, error))
    {
        return false;
    }
    void** slot = FindImportSlot(
        GetModuleHandleW(nullptr),
        "lua51.dll",
        "lua_pcall"
    );
    if (!slot)
    {
        error = "lua51_pcall_import_not_found";
        return false;
    }
    LuaPCallFunction original = reinterpret_cast<LuaPCallFunction>(*slot);
    if (!original)
    {
        error = "lua51_pcall_import_is_null";
        return false;
    }
    void** loadBufferSlot = FindImportSlot(
        GetModuleHandleW(nullptr),
        "lua51.dll",
        "luaL_loadbuffer"
    );
    if (!loadBufferSlot || !*loadBufferSlot)
    {
        error = "lua51_loadbuffer_import_not_found";
        return false;
    }
    void** newStateSlot = FindImportSlot(
        GetModuleHandleW(nullptr),
        "lua51.dll",
        "luaL_newstate"
    );
    if (!newStateSlot || !*newStateSlot)
    {
        error = "lua51_newstate_import_not_found";
        return false;
    }
    void** closeSlot = FindImportSlot(
        GetModuleHandleW(nullptr),
        "lua51.dll",
        "lua_close"
    );

    LuaPCallSlot = slot;
    OriginalLuaPCall = original;
    LuaLLoadBufferSlot = loadBufferSlot;
    OriginalLuaLLoadBuffer = reinterpret_cast<LuaLLoadBufferFunction>(
        *loadBufferSlot
    );
    LuaNewStateSlot = newStateSlot;
    OriginalLuaNewState = reinterpret_cast<LuaNewStateFunction>(
        *newStateSlot
    );
    LuaCloseSlot = closeSlot;
    OriginalLuaClose = closeSlot
        ? reinterpret_cast<LuaCloseFunction>(*closeSlot)
        : nullptr;
    void* previous = nullptr;
    if (!ExchangeImportSlot(
            slot,
            reinterpret_cast<void*>(&HookedLuaPCall),
            &previous,
            error
        ))
    {
        LuaPCallSlot = nullptr;
        OriginalLuaPCall = nullptr;
        LuaLLoadBufferSlot = nullptr;
        OriginalLuaLLoadBuffer = nullptr;
        LuaCloseSlot = nullptr;
        OriginalLuaClose = nullptr;
        LuaNewStateSlot = nullptr;
        OriginalLuaNewState = nullptr;
        return false;
    }
    if (previous != reinterpret_cast<void*>(original))
    {
        OriginalLuaPCall = reinterpret_cast<LuaPCallFunction>(previous);
    }
    previous = nullptr;
    if (!ExchangeImportSlot(
            loadBufferSlot,
            reinterpret_cast<void*>(&HookedLuaLLoadBuffer),
            &previous,
            error
        ))
    {
        std::string ignoredError;
        ExchangeImportSlot(
            LuaPCallSlot,
            reinterpret_cast<void*>(OriginalLuaPCall),
            nullptr,
            ignoredError
        );
        LuaPCallSlot = nullptr;
        OriginalLuaPCall = nullptr;
        LuaLLoadBufferSlot = nullptr;
        OriginalLuaLLoadBuffer = nullptr;
        LuaCloseSlot = nullptr;
        OriginalLuaClose = nullptr;
        LuaNewStateSlot = nullptr;
        OriginalLuaNewState = nullptr;
        return false;
    }
    if (previous != reinterpret_cast<void*>(OriginalLuaLLoadBuffer))
    {
        OriginalLuaLLoadBuffer =
            reinterpret_cast<LuaLLoadBufferFunction>(previous);
    }
    previous = nullptr;
    if (!ExchangeImportSlot(
            newStateSlot,
            reinterpret_cast<void*>(&HookedLuaNewState),
            &previous,
            error
        ))
    {
        std::string ignoredError;
        ExchangeImportSlot(
            LuaLLoadBufferSlot,
            reinterpret_cast<void*>(OriginalLuaLLoadBuffer),
            nullptr,
            ignoredError
        );
        ExchangeImportSlot(
            LuaPCallSlot,
            reinterpret_cast<void*>(OriginalLuaPCall),
            nullptr,
            ignoredError
        );
        LuaPCallSlot = nullptr;
        OriginalLuaPCall = nullptr;
        LuaLLoadBufferSlot = nullptr;
        OriginalLuaLLoadBuffer = nullptr;
        LuaCloseSlot = nullptr;
        OriginalLuaClose = nullptr;
        LuaNewStateSlot = nullptr;
        OriginalLuaNewState = nullptr;
        return false;
    }
    if (previous != reinterpret_cast<void*>(OriginalLuaNewState))
    {
        OriginalLuaNewState = reinterpret_cast<LuaNewStateFunction>(
            previous
        );
    }
    if (closeSlot && OriginalLuaClose)
    {
        previous = nullptr;
        if (!ExchangeImportSlot(
                closeSlot,
                reinterpret_cast<void*>(&HookedLuaClose),
                &previous,
                error
            ))
        {
            std::string ignoredError;
            ExchangeImportSlot(
                LuaNewStateSlot,
                reinterpret_cast<void*>(OriginalLuaNewState),
                nullptr,
                ignoredError
            );
            ExchangeImportSlot(
                LuaLLoadBufferSlot,
                reinterpret_cast<void*>(OriginalLuaLLoadBuffer),
                nullptr,
                ignoredError
            );
            ExchangeImportSlot(
                LuaPCallSlot,
                reinterpret_cast<void*>(OriginalLuaPCall),
                nullptr,
                ignoredError
            );
            LuaPCallSlot = nullptr;
            OriginalLuaPCall = nullptr;
            LuaLLoadBufferSlot = nullptr;
            OriginalLuaLLoadBuffer = nullptr;
            LuaCloseSlot = nullptr;
            OriginalLuaClose = nullptr;
            LuaNewStateSlot = nullptr;
            OriginalLuaNewState = nullptr;
            return false;
        }
        if (previous != reinterpret_cast<void*>(OriginalLuaClose))
        {
            OriginalLuaClose = reinterpret_cast<LuaCloseFunction>(
                previous
            );
        }
    }
    GetGuiLuaBridgeService().ReportGameplayPlayerTag("---");
    WriteGuiDiagnostic(
        "Lua lifecycle initialized from publisher activity: state=frontend"
    );
    error.clear();
    return true;
}

void UninstallGuiLua51Hooks()
{
    std::lock_guard<std::mutex> lock(HookMutex);
    if (LuaPCallSlot && OriginalLuaPCall)
    {
        std::string ignoredError;
        ExchangeImportSlot(
            LuaPCallSlot,
            reinterpret_cast<void*>(OriginalLuaPCall),
            nullptr,
            ignoredError
        );
    }
    if (LuaLLoadBufferSlot && OriginalLuaLLoadBuffer)
    {
        std::string ignoredError;
        ExchangeImportSlot(
            LuaLLoadBufferSlot,
            reinterpret_cast<void*>(OriginalLuaLLoadBuffer),
            nullptr,
            ignoredError
        );
    }
    if (LuaCloseSlot && OriginalLuaClose)
    {
        std::string ignoredError;
        ExchangeImportSlot(
            LuaCloseSlot,
            reinterpret_cast<void*>(OriginalLuaClose),
            nullptr,
            ignoredError
        );
    }
    if (LuaNewStateSlot && OriginalLuaNewState)
    {
        std::string ignoredError;
        ExchangeImportSlot(
            LuaNewStateSlot,
            reinterpret_cast<void*>(OriginalLuaNewState),
            nullptr,
            ignoredError
        );
    }
    LuaPCallSlot = nullptr;
    OriginalLuaPCall = nullptr;
    LuaLLoadBufferSlot = nullptr;
    OriginalLuaLLoadBuffer = nullptr;
    LuaCloseSlot = nullptr;
    OriginalLuaClose = nullptr;
    LuaNewStateSlot = nullptr;
    OriginalLuaNewState = nullptr;
    std::lock_guard<std::mutex> attachLock(AttachMutex);
    ObservedStates.clear();
    GetGuiLuaNativeBinding().DetachAll();
    GetGuiLuaBridgeService().ResetGameplayLifecycle();
    std::lock_guard<std::mutex> diagnosticLock(DiagnosticMutex);
    ObservedChunks.clear();
    LoggedLuaErrors = 0;
    LuaCallCount.store(0, std::memory_order_relaxed);
}

bool AreGuiLua51HooksInstalled()
{
    std::lock_guard<std::mutex> lock(HookMutex);
    return LuaPCallSlot != nullptr
        && OriginalLuaPCall != nullptr
        && LuaLLoadBufferSlot != nullptr
        && OriginalLuaLLoadBuffer != nullptr
        && LuaNewStateSlot != nullptr
        && OriginalLuaNewState != nullptr;
}
