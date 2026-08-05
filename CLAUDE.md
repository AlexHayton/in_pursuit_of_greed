# Guidelines

The engine and the SDL3 platform layer live in `source_shared/` and are built by **both** ports.
`source_macos/` and `source_win64/` hold only what is genuinely theirs — the `.app` bundle on one
side, a CMakeLists and a build script on the other.

Keep the plan file for whichever target you touched updated as you go: `macos_plan.md` or
`windows_plan.md`. A change under `source_shared/` may belong in both. `macos_plan.md` is the older
and fuller of the two and remains the reference for *why* the engine looks the way it does.

A change under `source_shared/` affects both platforms. Anything that needs an `#ifdef` there is
worth a second look — it usually belongs in the per-port CMakeLists instead.

## Build

```sh
cd source_macos
cmake --build build -j8            # Release, the one the user runs
cmake --build build-debug -j8      # -g -O0, for lldb
```

```powershell
cd source_win64
.\build.ps1                        # Release -> build\Greed.exe
.\build.ps1 -Config Debug          # Debug   -> build-debug\Greed.exe
```

`build.ps1` finds VS 2022's bundled CMake, Ninja and compiler itself — nothing needs to be on `PATH`.
It prefers clang-cl and falls back to MSVC `cl`, which is what is installed today.

Rebuild the directory the user is actually running. Watch the shell's cwd — a `cd` in an earlier
command persists, and `cmake --build build` from the repo root silently targets nothing.

## Authority

`source_dos/` decides intended behaviour, **not** `source_win32/`. The Win32 tree is unfinished and
contains workarounds for GDI and leftover debug code that read like engine invariants. Diff against
the DOS source before preserving anything odd.

## Bug classes seen in this port

- **64-bit word size.** Pointers through `int`, `sizeof(int)` sizing pointer arrays, `long` where the
  engine means 32 bits. `fixed_t` and `longint` must stay 32-bit. Note the two targets disagree:
  macOS is LP64 (`long` is 64-bit), Windows x64 is LLP64 (`long` is 32-bit). A `%ld` that is wrong on
  one is right on the other — check both before changing a format specifier.
- **`bool` shadowing.** The engine's `bool` is a 4-byte enum; SDL's headers macro it to 1-byte `_Bool`.
  Include SDL only via `platform/sys_sdl.h`, which restores it. A compile-time assert guards this.
- **Read-only string literals.** Watcom's were writable. Never mutate a `char *` argument in place.
- **Spin loops.** The tick is main-thread now, so any loop waiting on `timecount`, `keyboard[]` or
  `newascii` must call `Sys_PumpFrame()` or it hangs forever.
- **Framebuffer is top-down.** No flips anywhere. Both `RF_BlitView` and the present path copy row for
  row.
- **`fopen`/`open` macro collisions.** `sys_compat.h` macros `open` onto `Sys_open`. Any libc header
  that declares `open` must be included *above* that macro or its declaration is rewritten into a
  conflicting prototype. This is silent on POSIX and a hard error on Windows.
- **Paths that only look absolute.** The engine builds `A:\GREED\MOVIES\*.FLI` from an unassigned
  `cdr_drivenum`. That parses as a valid absolute path on Windows and as a relative one on macOS, so
  a resolver short-circuit that is harmless on one platform breaks every cutscene on the other.

## Verifying

- ASan is broken on this macOS — it deadlocks in its own initialiser. Use `build-debug` + lldb, or the
  crash report in `~/Library/Logs/DiagnosticReports/`.
- `-nointro` skips `MissionBriefing` entirely (`if (netmode || nointro) return;`). Headless tests using
  it prove nothing about that code.
- **Neither platform lets you take an ordinary screenshot.** macOS blocks `screencapture` by
  permission; Windows refuses to let a script-launched process raise its own window to the
  foreground, so a desktop grab returns whatever was already on top. Read the framebuffer from inside
  the process instead: set `GREED_SHOT=<prefix>` (and optionally `GREED_SHOT_AT=<ticks>`) to write
  PPMs, then convert and look at them. `-window` helps too — the default is fullscreen, and SDL
  minimises a fullscreen window that never gains focus.
- Don't claim a visual or behavioural fix works without looking. "Correct by construction" has been
  wrong here more than once.

## Style

Match the surrounding 1995 code: same indentation, same brace placement, no reformatting. Comment *why*
a change was needed — usually which platform assumption broke — not what the code does.
