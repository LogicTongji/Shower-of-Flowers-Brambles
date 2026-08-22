#pragma once

#include <d3d9.h>
#include <windows.h>

#include <cstddef>
#include <cstdint>

#if defined(SCRIPTED_GUI_OVERLAY_EXPORTS)
#define SCRIPTED_GUI_API
#else
#define SCRIPTED_GUI_API __declspec(dllimport)
#endif

extern "C"
{

struct ScriptedGuiLuaState;

using ScriptedGuiLuaCFunction = int (__cdecl *)(
    ScriptedGuiLuaState*
);

struct ScriptedGuiLua51ApiV1
{
    uint32_t size;
    uint32_t version;
    int (__cdecl *getTop)(ScriptedGuiLuaState*);
    void (__cdecl *setTop)(ScriptedGuiLuaState*, int);
    int (__cdecl *type)(ScriptedGuiLuaState*, int);
    int (__cdecl *toBoolean)(ScriptedGuiLuaState*, int);
    double (__cdecl *toNumber)(ScriptedGuiLuaState*, int);
    const char* (__cdecl *toLString)(
        ScriptedGuiLuaState*,
        int,
        std::size_t*
    );
    void* (__cdecl *toUserdata)(ScriptedGuiLuaState*, int);
    void (__cdecl *pushNil)(ScriptedGuiLuaState*);
    void (__cdecl *pushBoolean)(ScriptedGuiLuaState*, int);
    void (__cdecl *pushNumber)(ScriptedGuiLuaState*, double);
    void (__cdecl *pushLString)(
        ScriptedGuiLuaState*,
        const char*,
        std::size_t
    );
    void (__cdecl *pushLightUserdata)(
        ScriptedGuiLuaState*,
        void*
    );
    void (__cdecl *pushCClosure)(
        ScriptedGuiLuaState*,
        ScriptedGuiLuaCFunction,
        int
    );
    void (__cdecl *createTable)(ScriptedGuiLuaState*, int, int);
    void (__cdecl *getField)(
        ScriptedGuiLuaState*,
        int,
        const char*
    );
    void (__cdecl *setField)(
        ScriptedGuiLuaState*,
        int,
        const char*
    );
    std::size_t (__cdecl *objLen)(ScriptedGuiLuaState*, int);
    void (__cdecl *rawGetI)(ScriptedGuiLuaState*, int, int);
    int (__cdecl *next)(ScriptedGuiLuaState*, int);
};

constexpr uint32_t SCRIPTED_GUI_LUA51_API_VERSION = 1;
constexpr uint32_t NEW_CORE_ABI_VERSION = 1;

SCRIPTED_GUI_API uint32_t WINAPI NewCore_GetAbiVersion();
SCRIPTED_GUI_API DWORD WINAPI NewCore_GetModuleIds(
    char* output,
    DWORD capacity
);
SCRIPTED_GUI_API DWORD WINAPI NewCore_GetHookStatuses(
    char* output,
    DWORD capacity
);
SCRIPTED_GUI_API DWORD WINAPI NewCore_GetLastError(
    char* output,
    DWORD capacity
);

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_SetRootW(
    const wchar_t* root
);

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_AttachDevice(
    IDirect3DDevice9* device
);

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_AttachLua51(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1* api
);

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_IsLuaAttached();

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_InstallHooks();
SCRIPTED_GUI_API void WINAPI ScriptedGui_UninstallHooks();
SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_AreHooksInstalled();

SCRIPTED_GUI_API void WINAPI ScriptedGui_OnEndScene(
    IDirect3DDevice9* device
);

SCRIPTED_GUI_API void WINAPI ScriptedGui_OnBeforeReset();

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_OnAfterReset(
    IDirect3DDevice9* device,
    HRESULT resetResult
);

SCRIPTED_GUI_API BOOL WINAPI ScriptedGui_HandleWindowMessage(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
);

SCRIPTED_GUI_API void WINAPI ScriptedGui_Shutdown();

SCRIPTED_GUI_API DWORD WINAPI ScriptedGui_GetLastError(
    char* output,
    DWORD capacity
);

}

#undef SCRIPTED_GUI_API
