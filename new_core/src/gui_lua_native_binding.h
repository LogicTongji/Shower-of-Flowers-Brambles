#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gui_lua_bridge.h"
#include "scripted_gui_overlay_api.h"

class GuiLuaNativeBinding
{
public:
    GuiLuaNativeBinding();
    ~GuiLuaNativeBinding();

    GuiLuaNativeBinding(const GuiLuaNativeBinding&) = delete;
    GuiLuaNativeBinding& operator=(const GuiLuaNativeBinding&) = delete;

    bool Install(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api,
        GuiLuaBridgeService& service,
        std::string& error
    );

    bool DetachState(ScriptedGuiLuaState* state);
    bool TouchState(ScriptedGuiLuaState* state);
    std::vector<ScriptedGuiLuaState*> PruneInactiveStates(
        uint64_t maximumIdleMilliseconds
    );
    void ResetChannelOwnership();
    void DetachAll();
    bool IsInstalled() const;
    bool IsStateInstalled(ScriptedGuiLuaState* state) const;
    std::size_t StateCount() const;

private:
    static int __cdecl TryAcquireChannelThunk(
        ScriptedGuiLuaState* state
    );
    static int __cdecl ReleaseChannelThunk(
        ScriptedGuiLuaState* state
    );
    static int __cdecl PublishUpdateThunk(ScriptedGuiLuaState* state);
    static int __cdecl TryPopActionThunk(ScriptedGuiLuaState* state);

    int TryAcquireChannel(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api,
        GuiLuaBridgeService& service
    );
    int ReleaseChannel(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api,
        GuiLuaBridgeService& service
    );
    int PublishUpdate(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api,
        GuiLuaBridgeService& service
    );
    int TryPopAction(
        ScriptedGuiLuaState* state,
        const ScriptedGuiLua51ApiV1& api,
        GuiLuaBridgeService& service
    );

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

GuiLuaNativeBinding& GetGuiLuaNativeBinding();
