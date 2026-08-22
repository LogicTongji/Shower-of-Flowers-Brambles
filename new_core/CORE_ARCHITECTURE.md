# New Core architecture

`new_core` is the shared in-process extension runtime for HOI3. Script GUI and
Leader Capture are registered modules, not owners of the process bootstrap,
hooks, or game lifecycle.

## Core infrastructure

### Module registry

Files:

- `src/core_module.h`
- `src/core_module_registry.h/.cpp`

Every injected mechanism implements `core::IModule` and provides a stable ID,
startup priority, initialization, lifecycle handling, ticking, and shutdown.
Modules are initialized in ascending priority order and shut down in reverse
order. Duplicate IDs are rejected. Exceptions in lifecycle and tick callbacks
are isolated so one module cannot stop dispatch to the remaining modules.

Modules register before the first hook installation. This keeps startup
deterministic and prevents runtime code patches from appearing before all
modules have completed configuration validation.

```cpp
class ExampleModule final : public core::IModule
{
public:
    std::string_view Id() const override { return "example"; }
    int Priority() const override { return 200; }

    bool Initialize(
        core::Services& services,
        std::string& error
    ) override;

    void OnLifecycleEvent(
        const core::LifecycleEvent& event
    ) override;

    void Tick(uint64_t nowMilliseconds) override;
    void Shutdown() override;
};
```

### Hook registry

Files:

- `src/core_hook_registry.h/.cpp`

Each module registers one or more `core::HookDefinition` objects. A definition
contains a unique ID, priority, install/uninstall/status callbacks, and an
optional maintenance callback. Installation follows priority order;
uninstallation uses reverse order. Successfully installed hooks remain active
when another hook is temporarily unavailable, allowing the startup worker to
retry without repeatedly patching working hooks.

The Script GUI module currently registers:

- `windows.d3d9`
- `windows.lua51`

The Leader Capture module registers one transactional patch group:

- `hoi3.leader_capture`

The D3D9 maintenance path is invoked through the generic core pump. Lua no
longer directly knows how to repair D3D9 hooks.

### Lifecycle service

Files:

- `src/core_lifecycle.h/.cpp`
- `src/hoi3_lifecycle.h/.cpp`

`core::LifecycleService` publishes immutable before/after snapshots. Current
phases are `Unknown`, `Frontend`, and `Gameplay`. A transition records whether
gameplay was entered or exited and whether the player country changed.

`NotifySaveLoaded` is already part of the core API and increments both the
general lifecycle generation and save generation. Native save-load detection
is not connected yet; a future save hook or Lua bridge can call this method
without changing module code.

The native HOI3 player-tag probe is polled by `core::Runtime`, not by the D3D9
GUI host. Consequently non-GUI modules can subscribe to the same frontend and
gameplay boundaries.

## Runtime and modules

Files:

- `src/core_runtime.h/.cpp`
- `src/script_gui_core_module.h/.cpp`
- `src/leader_capture_core_module.h/.cpp`
- `src/scripted_gui_overlay_dll.cpp`

`core::Runtime` owns the hook registry, lifecycle service, and module registry.
It serializes exported calls, pumps hook maintenance, polls lifecycle state,
dispatches queued lifecycle events, and ticks modules.

`ScriptGuiCoreModule` owns the D3D9 host and adapts core lifecycle events to the
existing Lua GUI bridge. Existing `ScriptedGui_*` exports and the current DLL
filename remain unchanged for injector compatibility; the DLL entry is now a
thin adapter around `core::Runtime`.

`LeaderCaptureCoreModule` is the second module. Its capture and transfer engine
is implemented by `src/leader_capture_engine.h/.cpp`. It has no private
`DllMain`, injector, or worker thread. Hook installation belongs to
`HookRegistry`, its 100 ms controller polling belongs to the shared module Tick,
and gameplay exit, player changes, and save-load notifications clear all native
session pointers.

The former standalone project, launcher, build outputs, and compatibility DLL
have been removed. A later compatibility pass should move the fixed executable
RVAs and signatures into a named version profile without changing the module
contract.

## Product launcher and handshake

Files:

- `src/new_core_launcher.cpp`
- `src/new_core_launcher_core.h/.cpp`
- `src/new_core_handshake.h/.cpp`
- `src/new_core_launcher_probe.cpp`

`hoi3_new_core_launcher.exe` provides original-launcher and injected-game
modes. Its launch core validates PE32 i386 compatibility, creates HOI3,
injects `hoi3_new_core.dll`, and waits for a versioned shared-memory handshake.
The DLL reports runtime initialization, module IDs, Hook installation status,
and terminal readiness or failure. The launcher contains no gameplay-module or
GUI-plugin business logic.

The stable generic exports are `NewCore_GetAbiVersion`,
`NewCore_GetModuleIds`, `NewCore_GetHookStatuses`, and
`NewCore_GetLastError`. Existing `ScriptedGui_*` exports remain available for
the Script GUI module and compatibility tooling.
