#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "gui_lua51_hook.h"
#include "gui_lua_bridge.h"
#include "gui_lua_native_binding.h"

namespace
{

using LuaNewStateFunction = ScriptedGuiLuaState* (__cdecl *)();
using LuaOpenLibrariesFunction = void (__cdecl *)(ScriptedGuiLuaState*);
using LuaLoadStringFunction = int (__cdecl *)(
    ScriptedGuiLuaState*,
    const char*
);
using LuaPCallFunction = int (__cdecl *)(
    ScriptedGuiLuaState*,
    int,
    int,
    int
);
using LuaCloseFunction = void (__cdecl *)(ScriptedGuiLuaState*);

template <typename Function>
Function Resolve(HMODULE module, const char* name)
{
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

bool RunBooleanChunk(
    ScriptedGuiLuaState* state,
    const ScriptedGuiLua51ApiV1& api,
    LuaLoadStringFunction loadString,
    LuaPCallFunction pcall,
    const char* source
)
{
    api.setTop(state, 0);
    if (loadString(state, source) != 0
        || pcall(state, 0, 1, 0) != 0)
    {
        return false;
    }
    return api.type(state, -1) == 1
        && api.toBoolean(state, -1) != 0;
}

}

int wmain(int argumentCount, wchar_t** arguments)
{
    if (argumentCount != 2)
    {
        std::cerr << "usage: gui_lua51_native_probe <lua5.1.dll>\n";
        return 2;
    }
    const std::filesystem::path luaPath = arguments[1];
    SetDllDirectoryW(luaPath.parent_path().wstring().c_str());
    HMODULE module = LoadLibraryW(luaPath.wstring().c_str());
    if (!module)
    {
        std::cerr << "failed to load Lua DLL\n";
        return 3;
    }

    const LuaNewStateFunction newState =
        Resolve<LuaNewStateFunction>(module, "luaL_newstate");
    const LuaOpenLibrariesFunction openLibraries =
        Resolve<LuaOpenLibrariesFunction>(module, "luaL_openlibs");
    const LuaLoadStringFunction loadString =
        Resolve<LuaLoadStringFunction>(module, "luaL_loadstring");
    const LuaPCallFunction pcall =
        Resolve<LuaPCallFunction>(module, "lua_pcall");
    const LuaCloseFunction closeState =
        Resolve<LuaCloseFunction>(module, "lua_close");
    if (!newState || !openLibraries || !loadString || !pcall || !closeState)
    {
        std::cerr << "required Lua exports are missing\n";
        FreeLibrary(module);
        return 4;
    }

    ScriptedGuiLua51ApiV1 api;
    std::string error;
    if (!ResolveGuiLua51Api(api, error))
    {
        std::cerr << error << '\n';
        FreeLibrary(module);
        return 5;
    }
    ScriptedGuiLuaState* state = newState();
    if (!state)
    {
        std::cerr << "luaL_newstate failed\n";
        FreeLibrary(module);
        return 6;
    }
    openLibraries(state);

    GuiLuaBridgeService service;
    std::unique_ptr<IGuiDataBridgeChannel> channel =
        service.CreateChannel("probe", {});
    const std::filesystem::path root;
    if (!channel
        || !channel->Open(GuiDataProviderInitContext{root}, error)
        || !GetGuiLuaNativeBinding().Install(
            state,
            api,
            service,
            error
        ))
    {
        std::cerr << error << '\n';
        GetGuiLuaNativeBinding().DetachAll();
        closeState(state);
        FreeLibrary(module);
        return 7;
    }

    const char* publishSource =
        "return ScriptedGuiNative.PublishUpdate('probe',{"
        "revision=1,baseRevision=0,fullSnapshot=true,"
        "values={['state.visible']=true,"
        "['regions.7.controlledPercentage']=91.5},"
        "removedValues={},"
        "lists={assigned_leader_list={revision=4,items={{"
        "id=11,text='leader',regionid=7,x=0.25,y=0.5}}}},"
        "removedLists={}})";
    GuiDataBridgeUpdate update;
    if (!RunBooleanChunk(
            state,
            api,
            loadString,
            pcall,
            publishSource
        )
        || channel->Poll(update, error) != GuiDataBridgePollResult::Update)
    {
        std::cerr << "Lua update publication failed: " << error << '\n';
        GetGuiLuaNativeBinding().DetachAll();
        closeState(state);
        FreeLibrary(module);
        return 8;
    }
    const auto visible = update.values.find("state.visible");
    const auto percentage = update.values.find(
        "regions.7.controlledpercentage"
    );
    const auto leaders = update.lists.find("assigned_leader_list");
    if (update.revision != 1
        || !update.fullSnapshot
        || visible == update.values.end()
        || !GuiDataValueToBool(visible->second)
        || percentage == update.values.end()
        || GuiDataValueToNumber(percentage->second) != 91.5
        || leaders == update.lists.end()
        || leaders->second.revision != 4
        || leaders->second.items.size() != 1
        || leaders->second.items.front().id != 11
        || !leaders->second.items.front().Find("regionid")
        || GuiDataValueToNumber(
            *leaders->second.items.front().Find("regionid")
        ) != 7.0)
    {
        std::cerr << "decoded Lua update did not match\n";
        GetGuiLuaNativeBinding().DetachAll();
        closeState(state);
        FreeLibrary(module);
        return 9;
    }

    GuiActionContext action;
    action.action = "move_leader";
    action.functionName = "MoveLeader";
    action.parameters["regionid"] = "7";
    if (channel->SendAction(action, error)
            != GuiDataBridgeSendResult::Accepted
        || !RunBooleanChunk(
            state,
            api,
            loadString,
            pcall,
            "local a=ScriptedGuiNative.TryPopAction('probe');"
            "return a and a.functionName=='MoveLeader' "
            "and a.regionid=='7' and a.parameters.regionid=='7'"
        ))
    {
        std::cerr << "Lua action delivery failed: " << error << '\n';
        GetGuiLuaNativeBinding().DetachAll();
        closeState(state);
        FreeLibrary(module);
        return 10;
    }

    channel->Close();
    GetGuiLuaNativeBinding().DetachAll();
    closeState(state);
    FreeLibrary(module);
    std::cout << "Lua 5.1 native bridge probe passed\n";
    return 0;
}
