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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\overlay\build_windows.ps1 -Configuration Debug
```

For an optimized build:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\overlay\build_windows.ps1 -Configuration Release
```

The script configures the `windows-x86` preset, builds all cross-platform targets, and runs the offline probe suite. Use `-SkipTests` only when a build-only iteration is needed.

VS Code also exposes the same commands through `Terminal > Run Build Task`.

## In-process DLL

The Win32 build now produces:

```text
overlay\build-win\Debug\scripted_gui_overlay.dll
overlay\build-win\Debug\scripted_gui_injector.exe
```

The DLL contains only the platform-independent Scripted GUI runtime and the
Win32/Direct3D 9 backend. It does not link SDL or any macOS host code. Startup
plugins are always created with the live `lua` bridge; manifest `file` and
`sequence` providers remain development-only executables.

The DLL installs its D3D9 hooks from a worker scheduled by `DllMain`. The
installer patches the shared `IDirect3DDevice9` vtable for `EndScene` and
`Reset`, then subclasses the real device focus window when the first frame is
seen. Heavy GUI initialization remains lazy and runs from the render callback,
not under the loader lock.

The hook layer and external diagnostics use these stable exports:

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

`ScriptedGui_AttachLua51` must run on the thread that owns the supplied Lua
state. Its versioned function table is declared in
`src/scripted_gui_overlay_api.h`; this keeps the Scripted GUI core independent
of a particular Lua import library or hard-coded game address. The remaining
HOI3-specific integration step is to locate the game's live `lua_State` and
Lua 5.1 C API addresses, then call this export once for that state. The data
conversion, update queue, action queue and global-table registration are
already implemented in the DLL.

For the HOI3 4.02/TFH binaries, the game imports Lua 5.1 through `lua51.dll`.
The DLL therefore also installs a narrow IAT hook on the main executable's
`lua_pcall` import. The hook preserves the Lua stack, registers
`ScriptedGuiNative` the first time each active Lua context is observed, then
immediately delegates to the original `lua_pcall`. Lua API addresses are
resolved by exported names from `lua51.dll`; no fixed executable addresses are
required for this build.

For deterministic development startup, the x86 injector creates the selected
game executable suspended, supplies `SCRIPTED_GUI_ROOT`, loads the overlay DLL
with a remote `LoadLibraryW`, and only then resumes the game thread:

```powershell
.\overlay\build-win\Debug\scripted_gui_injector.exe `
  "D:\80th_special_version\hoi3\hoi3_tfh.exe" `
  ".\overlay\build-win\Debug\scripted_gui_overlay.dll" `
  "."
```

Any arguments after the project root are forwarded to HOI3, including its
`-mod` option. This launcher is a development utility and is not linked into
the injected DLL. A two-argument `-mod <descriptor>` input is normalized to
HOI3's `-mod=<descriptor>` form. Relative descriptors intentionally remain
relative to the HOI3 installation directory because the legacy mod loader
expects paths such as `mod/scripted_gui_development.mod`.

CMake also generates `overlay\build-win\hoi3_scripted_gui_dev.mod`. It points
directly at the current repository and uses a separate HOI3 user directory, so
the repository can be tested without copying scripts or assets into the game
installation:

```powershell
.\overlay\build-win\Debug\scripted_gui_injector.exe `
  "D:\80th_special_version\hoi3\hoi3_tfh.exe" `
  ".\overlay\build-win\Debug\scripted_gui_overlay.dll" `
  "." `
  "-mod" `
  ".\overlay\build-win\hoi3_scripted_gui_dev.mod"
```

The D3D9 backend currently renders declarative images, text, buttons, lists,
scrollbars, progress bars, indexed maps and marker layers. Marker layers keep
the same region anchoring, stacking, tooltip, drag and marker-action behavior
as the offline macOS prototype.

`ScriptedGui_SetRootW` can explicitly select the mod root. Otherwise the DLL
checks `SCRIPTED_GUI_ROOT`, its own directory and parents, then the process
working directory and parents. `DllMain` only stores its module handle,
disables thread notifications and schedules the hook worker. It performs no
file parsing, plugin creation, texture loading, or rendering directly.
