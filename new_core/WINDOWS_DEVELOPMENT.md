# Windows development

The Hearts of Iron III process is 32-bit, so every in-process overlay component must be built for `Win32`/x86.

## Prerequisites

- Visual Studio 2022 with Desktop development with C++
- MSVC x86/x64 build tools
- Windows 10 or 11 SDK
- CMake tools for Windows

## Build and test

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\new_core\build_windows.ps1 -Configuration Debug
```

For an optimized build:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\new_core\build_windows.ps1 -Configuration Release
```

The script configures the `windows-x86` preset, builds all cross-platform targets, and runs the offline probe suite. Use `-SkipTests` only when a build-only iteration is needed.

The portable probes cover Lua channel ownership, independent action/data
scheduling, state detachment, global window focus/modal ordering, session
generation resets, persistence round trips, save-profile isolation, and
corrupt-state rejection. The native Lua 5.1 probe also verifies that fallback
publishers persist into the player country's save variables and that rolling
back the persisted revision creates a fresh GUI session. The D3D9 smoke probe
returns `77` when no usable windowed device is available, which CTest records
as skipped.

Presentation persistence defaults to
`%LOCALAPPDATA%\HOI3 Scripted GUI\state`. Redirect it for a portable or test
run with:

```powershell
$env:NEW_CORE_STATE_ROOT = "$PWD\new_core\build-win\probe-state"
```

VS Code also exposes the same commands through `Terminal > Run Build Task`.

## In-process DLL

The Win32 build now produces:

```text
new_core\build-win\Debug\hoi3_new_core.dll
new_core\build-win\Debug\hoi3_new_core_launcher.exe
new_core\build-win\Debug\scripted_gui_injector.exe
```

The DLL contains the New Core module runtime, Leader Capture, the
backend-neutral Scripted GUI runtime, and the Win32/Direct3D 9 backend. The
in-process host registers `file`, `sequence`, and `bridge` providers and creates
every enabled manifest plugin. `startup` controls only whether its window opens
initially. Manifest options prefixed with `inprocess_` override the corresponding
provider option in the DLL, allowing one GUI to use sequence snapshots offline
and a named Lua bridge in HOI3.

The DLL installs its D3D9 hooks from a worker scheduled by `DllMain`. The
installer patches the shared `IDirect3DDevice9` vtable for `EndScene` and
`Reset`, then subclasses the real device focus window when the first frame is
seen. Heavy GUI initialization remains lazy and runs from the render callback,
not under the loader lock.

The hook layer and external diagnostics use these stable exports:

- `NewCore_GetAbiVersion()` for launcher/core protocol compatibility.
- `NewCore_GetModuleIds()` for the registered module list.
- `NewCore_GetHookStatuses()` for unified Hook installation status.
- `NewCore_GetLastError()` for the latest core-level failure.
- `ScriptedGui_OnEndScene(device)` before the original D3D9 `EndScene`.
- `ScriptedGui_OnBeforeReset()` before the original `Reset`.
- `ScriptedGui_OnAfterReset(device, result)` after `Reset` succeeds.
- `ScriptedGui_HandleWindowMessage(...)` from the hooked game `WndProc`.
- `ScriptedGui_Shutdown()` before hooks are removed or the DLL is unloaded.
- `ScriptedGui_InstallHooks()` and `ScriptedGui_UninstallHooks()` for explicit
  lifecycle control when an injector does not want automatic installation.
- `ScriptedGui_AttachLua51(state, api)` to register the native
  `ScriptedGuiNative.PublishUpdate` and `ScriptedGuiNative.TryPopAction`
  functions in a Lua 5.1 state.
- `ScriptedGui_IsLuaAttached()` to verify that native Lua registration has
  completed.

For normal development and product startup, prefer
`hoi3_new_core_launcher.exe`; the command-line injector remains a narrow
diagnostic utility. See `LAUNCHER.md`.

`ScriptedGui_AttachLua51` must run on the thread that owns the supplied Lua
state. Its versioned function table is declared in
`src/scripted_gui_overlay_api.h`; this keeps the Scripted GUI core independent
of hard-coded game addresses. The automatic Lua 5.1 import hooks attach every
observed state, prune inactive generations, and detach a state immediately
when an imported `lua_close` is available.

For the HOI3 4.02/TFH binaries, the game imports Lua 5.1 through `lua51.dll`.
The DLL therefore installs narrow IAT hooks on the main executable's
`luaL_newstate` and `lua_pcall` imports, with an optional `lua_close` hook. The
hook preserves the Lua stack, registers
`ScriptedGuiNative` the first time each active Lua context is observed, then
immediately delegates to the original `lua_pcall`. Lua API addresses are
resolved by exported names from `lua51.dll`; no fixed executable addresses are
required for this build.

For deterministic development startup, the x86 injector creates the selected
game executable suspended, supplies `SCRIPTED_GUI_ROOT`, loads the core DLL
with a remote `LoadLibraryW`, and only then resumes the game thread:

```powershell
.\new_core\build-win\Debug\scripted_gui_injector.exe `
  "D:\80th_special_version\hoi3\hoi3_tfh.exe" `
  ".\new_core\build-win\Debug\hoi3_new_core.dll" `
  "."
```

Any arguments after the project root are forwarded to HOI3, including its
`-mod` option. This launcher is a development utility and is not linked into
the injected DLL. A two-argument `-mod <descriptor>` input is normalized to
HOI3's `-mod=<descriptor>` form. Relative descriptors intentionally remain
relative to the HOI3 installation directory because the legacy mod loader
expects paths such as `mod/scripted_gui_development.mod`.

CMake also generates `new_core\build-win\hoi3_scripted_gui_dev.mod`. It points
directly at the current repository and uses a separate HOI3 user directory, so
the repository can be tested without copying scripts or assets into the game
installation:

```powershell
.\new_core\build-win\Debug\scripted_gui_injector.exe `
  "D:\80th_special_version\hoi3\hoi3_tfh.exe" `
  ".\new_core\build-win\Debug\hoi3_new_core.dll" `
  "." `
  "-mod" `
  ".\new_core\build-win\hoi3_scripted_gui_dev.mod"
```

The D3D9 backend currently renders declarative images, text, buttons, lists,
scrollbars, progress bars, indexed maps and marker layers. Marker layers
support region anchoring, stacking, tooltip, drag and marker actions.

`ScriptedGui_SetRootW` can explicitly select the mod root. Otherwise the DLL
checks `SCRIPTED_GUI_ROOT`, its own directory and parents, then the process
working directory and parents. `DllMain` only stores its module handle,
disables thread notifications and schedules the hook worker. It performs no
file parsing, plugin creation, texture loading, or rendering directly.
