# Windows x64 port of *In Pursuit of Greed*

A native 64-bit Windows build of the 1995 Raven engine, from the same sources as the macOS app.

Almost nothing here is Windows-specific. The engine and the SDL3 platform layer live in
[`../source_shared`](../source_shared) and are shared verbatim with [`../source_macos`](../source_macos);
this directory holds a 27-line `CMakeLists.txt` and a build script.

- [`../windows_plan.md`](../windows_plan.md) — how this port was done, what was shared, what broke.
- [`../macos_plan.md`](../macos_plan.md) — the older and fuller record, and still the reference for
  *why* the engine looks the way it does.

## Build

```powershell
.\build.ps1                    # Release -> build\Greed.exe
.\build.ps1 -Config Debug      # Debug   -> build-debug\Greed.exe
.\build.ps1 -Clean             # wipe the tree first (refetches SDL3 + libxmp)
```

Needs only **Visual Studio 2022** — the script locates VS's bundled CMake, Ninja and compiler itself,
so nothing has to be on `PATH` and no developer prompt is needed. The first configure builds SDL3 and
libxmp from source (a few minutes), cached thereafter.

`-Config` rather than `-Debug`: PowerShell reserves the latter as a common parameter and refuses to
load a script that redefines it.

### Compiler

`build.ps1` prefers **clang-cl** and falls back to **MSVC `cl`**, which is what is used today because
the optional "C++ Clang tools for Windows" VS component is not installed here. Both work.

clang-cl is still worth installing (VS Installer, or `winget install LLVM.LLVM`): it is the same
compiler family the macOS port is built with, so `cmake/engine.cmake` can use the identical
`-Werror=int-conversion` / `-Werror=pointer-to-int-cast` guards rather than MSVC's approximate `/we`
equivalents. Those guards caught six real pointer-truncation bugs during the macOS port. The script
picks it up automatically once present.

Note Windows x64 is **LLP64** — `long` is 32-bit here, 64-bit on macOS. The engine's narrowed types
(`longint`, `fixed_t`, the `sp_*` steppers) are correct under both, and the `%lu`/`%ld` specifiers
that had to be fixed for macOS are trivially correct here. Don't "re-fix" them.

## Run

The game data is not copied next to the exe. Point `GREED_DATA` at the release directories — a
**semicolon**-separated list on Windows, because a colon appears in every drive letter:

```powershell
$root = "N:\CodeProjects\in_pursuit_of_greed"
$env:GREED_DATA = "$root\greed_final;$root\greed_cdrom"
.\build\Greed.exe
```

Both directories are needed and hold different things: `greed_final\` is the installed game
(`GREED.BLO`, `SETUP.CFG`, the 18 music modules); `greed_cdrom\` is the un-installed CD master and
supplies only `MOVIES\` for the FLI cutscenes.

Settings and savegames are written to `%APPDATA%\redshadow\Greed\`, never next to the exe.

### Options

| Flag | Effect |
|---|---|
| `-window` | Start windowed. The built-in default is fullscreen, and a fullscreen window that never gains focus gets minimised by SDL. |
| `-nointro` | Skip the FLI intro and the menu; drops straight into a game. Note it also skips `MissionBriefing` entirely, so it proves nothing about that path. |
| `-ticker` | Overlay the 35 Hz tick counters. |

`F11` toggles fullscreen. `Alt-F4` quits. (`Cmd-Q`/`Cmd-F` are macOS-only — the equivalent modifier
here is the Windows key, and binding quit to `Win+Q` would collide with the OS.)

## Verifying a change visually

Windows will not let a process launched from a script raise its own window to the foreground, so a
desktop screen-grab captures whatever was already on top. Read the framebuffer from inside the process
instead:

```powershell
$env:GREED_SHOT = "C:\temp\shot"        # writes shot<tick>.ppm
$env:GREED_SHOT_AT = "60,150,300"       # ticks to dump at (default 70,210,420)
.\build\Greed.exe -window -nointro
```

This is the same technique the macOS port used, where `screencapture` is blocked by permissions.
`Raven.c` also carries a `GREED_REPRO` harness that drives the player along a fixed path and dumps one
frame.
