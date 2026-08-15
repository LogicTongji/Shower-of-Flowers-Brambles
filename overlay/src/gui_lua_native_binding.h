#pragma once

#include <memory>
#include <string>

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

    void DetachAll();
    bool IsInstalled() const;
    bool IsStateInstalled(ScriptedGuiLuaState* state) const;

private:
    static int __cdecl PublishUpdateThunk(ScriptedGuiLuaState* state);
    static int __cdecl TryPopActionThunk(ScriptedGuiLuaState* state);

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
