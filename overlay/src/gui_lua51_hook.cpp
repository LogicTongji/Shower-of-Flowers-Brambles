#include "gui_lua51_hook.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

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

std::mutex HookMutex;
std::mutex AttachMutex;
std::mutex DiagnosticMutex;
void** LuaPCallSlot = nullptr;
LuaPCallFunction OriginalLuaPCall = nullptr;
void** LuaLLoadBufferSlot = nullptr;
LuaLLoadBufferFunction OriginalLuaLLoadBuffer = nullptr;
std::unordered_set<ScriptedGuiLuaState*> ObservedStates;
std::unordered_set<std::string> ObservedChunks;
std::size_t LoggedLuaErrors = 0;
constexpr std::size_t MaximumLoggedChunks = 128;
constexpr std::size_t MaximumLoggedLuaErrors = 64;

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
            error.clear();
            return true;
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
        error.clear();
        return true;
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
    WriteGuiDiagnostic("Lua 5.1 state attached to ScriptedGuiNative");
    return true;
}

bool InstallGuiLua51Hooks(std::string& error)
{
    std::lock_guard<std::mutex> lock(HookMutex);
    if (LuaPCallSlot
        && OriginalLuaPCall
        && LuaLLoadBufferSlot
        && OriginalLuaLLoadBuffer)
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

    LuaPCallSlot = slot;
    OriginalLuaPCall = original;
    LuaLLoadBufferSlot = loadBufferSlot;
    OriginalLuaLLoadBuffer = reinterpret_cast<LuaLLoadBufferFunction>(
        *loadBufferSlot
    );
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
        return false;
    }
    if (previous != reinterpret_cast<void*>(OriginalLuaLLoadBuffer))
    {
        OriginalLuaLLoadBuffer =
            reinterpret_cast<LuaLLoadBufferFunction>(previous);
    }
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
    LuaPCallSlot = nullptr;
    OriginalLuaPCall = nullptr;
    LuaLLoadBufferSlot = nullptr;
    OriginalLuaLLoadBuffer = nullptr;
    std::lock_guard<std::mutex> attachLock(AttachMutex);
    ObservedStates.clear();
    std::lock_guard<std::mutex> diagnosticLock(DiagnosticMutex);
    ObservedChunks.clear();
    LoggedLuaErrors = 0;
}

bool AreGuiLua51HooksInstalled()
{
    std::lock_guard<std::mutex> lock(HookMutex);
    return LuaPCallSlot != nullptr
        && OriginalLuaPCall != nullptr
        && LuaLLoadBufferSlot != nullptr
        && OriginalLuaLLoadBuffer != nullptr;
}
