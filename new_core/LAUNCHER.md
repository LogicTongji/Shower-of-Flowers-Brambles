# HOI3 New Core Launcher

`hoi3_new_core_launcher.exe` is the product entry point for New Core. It keeps
process startup separate from injected module business logic.

## Modes

- **Injected mode** starts `hoi3_tfh.exe` suspended, injects
  `hoi3_new_core.dll`, resumes the game, and waits for the DLL handshake.
- **Original launcher mode** starts the configured Paradox launcher without
  loading New Core.

The launcher never implements Script GUI, leader capture, or future gameplay
mechanisms. Those remain registered `core::IModule` implementations inside the
DLL.

## First development run

Build the Win32 Debug targets:

```powershell
& .\new_core\build_windows.ps1 -Configuration Debug
```

Start:

```powershell
& .\new_core\build-win\Debug\hoi3_new_core_launcher.exe
```

In this repository layout the launcher normally detects:

- game: `D:\80th_special_version\hoi3\hoi3_tfh.exe`
- original launcher: `D:\80th_special_version\hoi3\launcher.exe`
- DLL: `new_core\build-win\Debug\hoi3_new_core.dll`
- project root: the repository root
- development descriptor: `tfh\mod\hoi3_scripted_gui_dev.mod`

Review all paths before the first launch. Settings are saved beside the
launcher as `hoi3_new_core_launcher.ini`.

## Injection handshake

The launcher creates a versioned named shared-memory block and passes its name
through `NEW_CORE_HANDSHAKE_NAME`. The DLL reports these states:

1. `dll_worker_started`
2. `runtime_initialized`
3. `hooks_installing`
4. `ready` or `failed`

The final payload includes ABI version, registered module IDs, Hook IDs, and
installation status. Therefore remote `LoadLibraryW` success alone is not
treated as a successful New Core launch.

## Safety rules

- The launcher validates that both game and DLL are PE32 i386 files.
- Duplicate `hoi3_tfh.exe` processes are blocked by default.
- A failed remote DLL load terminates the still-suspended child.
- Hook initialization failure leaves the resumed game process running and
  reports the failure for diagnosis.
- Do not use the legacy development injector at the same time as this launcher.

## Configuration

`hoi3_new_core_launcher.ini` uses the `[launcher]` section:

```ini
[launcher]
mode=inject
game_executable=D:\path\to\hoi3_tfh.exe
original_launcher=D:\path\to\launcher.exe
core_library=D:\path\to\hoi3_new_core.dll
project_root=D:\path\to\mod-root
mod_descriptor=D:\path\to\tfh\mod\example.mod
extra_arguments=
handshake_timeout_ms=125000
prevent_duplicate_game=1
```
