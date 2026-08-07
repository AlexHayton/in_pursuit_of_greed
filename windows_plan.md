# Windows x64 port of *In Pursuit of Greed*

## Status

*Updated as work proceeds. Last updated: 2026-08-05.*

All four planned phases are implemented. `source_win64/build/Greed.exe` builds from cold and runs: the
intro FLIs play and the 3D view renders, both confirmed by looking at framebuffer dumps rather than by
argument. What remains is verification, not construction — see **Open / next**, which is explicit
about what has been *watched* and what merely no longer fails.

| Phase | State | Notes |
|---|---|---|
| 0 — Hoist shared code into `source_shared/` | **done** | 50 files `git mv`'d; history preserved |
| 1 — Windows branches in the shared platform layer | **done** | 9 conditional sites in 1,937 lines |
| 2 — `source_win64` build (CMake + `build.ps1`) | **done** | Static CRT; no VC++ redist needed |
| 3 — Build, run, fix what surfaces | **done** | Two bugs, both platform-difference shaped |

```powershell
cd source_win64
.\build.ps1                    # Release -> build\Greed.exe
.\build.ps1 -Config Debug      # Debug   -> build-debug\Greed.exe

$root = "N:\CodeProjects\in_pursuit_of_greed"
$env:GREED_DATA = "$root\greed_final;$root\greed_cdrom"    # note ';' not ':'
.\build\Greed.exe -window
```

`build.ps1` locates VS 2022's bundled CMake, Ninja and compiler itself, so nothing needs to be on
`PATH` and no developer prompt is required.

---

## How much was actually shared

This is the question the port existed to answer, and it was measured rather than assumed — first by
grepping the tree for OS APIs before any work started, then confirmed by what actually had to change.

| Component | Lines | Windows-specific changes |
|---|---|---|
| `src/` — 24 `.c` + 10 `.h` | 24,221 | **none** |
| `platform/sys_video.c` | 494 | **none** |
| `platform/sys_input.c` | 336 | **none** |
| `platform/sys_sound.c` | 316 | **none** |
| `platform/sys_sdl.h`, `sys_greed.h` | 116 | **none** |
| `platform/sys_main.c` | 260 | 2 sites — Cmd-Q / Cmd-F |
| `platform/sys_files.c` | 329 | 4 sites — separator, base dir, abspath |
| `platform/sys_compat.h` | 86 | 3 sites — libc headers, `strtok_r` |

**Nine `#ifdef` sites in the whole platform layer, and none in the engine.**

The reason is that the macOS port was written against SDL3 rather than against macOS. A grep of the
whole tree for `__APPLE__`, `TARGET_OS`, `Cocoa`, `Foundation`, `mach_` and `dispatch_` returned
nothing before the port began. `SDL_GetPrefPath`, `SDL_GetTicks`, `SDL_AudioStream`,
`SDL_GetKeyboardState` and `SDL_SetWindowRelativeMouseMode` are the same calls on both platforms.

`SDL_GetPrefPath("redshadow", "Greed")` needed **no change at all** — it yields
`%APPDATA%\redshadow\Greed\` here and `~/Library/Application Support/Greed/` there, so the entire
read-only-install problem that `sys_files.c` exists to solve was already solved portably.

Everything genuinely macOS-specific turned out to be in the *build*, not the code: `MACOSX_BUNDLE`,
`Info.plist`, `.icns`, `codesign`, and the `Contents/Resources` layout.

### The engine needed nothing

Worth stating plainly, because it was the main risk going in. `source_win32/Win32/` had already
rewritten John Bianca's hand-written x86 assembler renderer in portable C, and the macOS port removed
the last Windows dependencies. A grep for `windows.h`, `WINAPI`, `HWND`, `HPALETTE`, `WinMain`,
`__asm`, `timeSetEvent`, `GetAsyncKeyState` and `PostQuitMessage` across `src/` finds hits in exactly
two places, both *comments* — `D_misc.c:121` and `Timer.c:28` — explaining what was removed and why.

Two hazards specific to 1995 C on Windows were checked for and are absent: no identifier collides
with `windows.h`'s `near`, `far`, `min`, `max`, `IN` or `OUT` (the grep hits are all string literals
in `Constant.c`'s mission briefings), and there is only one implicit-int declaration left in the whole
tree, `Utils.c:515`.

---

## Bugs found and fixed

Only two, and both have the same shape: **a platform difference that turned a macOS no-op into
load-bearing code.**

### `mode_t` does not exist in the UCRT

`Sys_open` declared its varargs mode as `mode_t`. Now `int`, which is what `va_arg` extracts on both
platforms anyway — POSIX promotes `mode_t` through the ellipsis, and MSVC's `_open` takes an `int`
third argument. A compile error, found on the first build.

### Every FLI cutscene failed: `File Not Found: A:\MOVIES\TEXT.FLI`

This one is worth remembering, because it is the exact inverse of the bug the plan predicted.

The engine builds forty DOS CD-ROM paths from `cdr_drivenum`, which is never assigned anywhere in the
tree, so every one comes out as `A:\GREED\MOVIES\*.FLI`. `sys_files.c` normalises those away rather
than editing four engine files — that fix predates this port and works fine on macOS.

But `Sys_fopen` short-circuits on absolute paths, and Phase 1 had just taught `is_absolute()` about
drive letters. That change was *correct in itself*: without it, an already-resolved `C:\...` path
would be sent back through the search list and prefixed a second time. The problem is that
`A:\MOVIES\TEXT.FLI` **is** a perfectly well-formed absolute path on drive A:, so it now
short-circuited straight past the normaliser that existed to fix it.

On macOS the same string is *relative* — there are no drive letters — which is the only reason the
resolver ever saw it. The macOS behaviour depended on an accident.

Fixed by requiring an absolute path to **exist** before it short-circuits a read
(`is_absolute_and_exists`). That separates a real caller-built path from a bogus one without having
to guess which drive letters are meaningful. Writes keep the cheap test, since a write target
legitimately does not exist yet, and nothing in the engine writes to a DOS-built path.

### Predicted and did not happen

Both hazards this plan called out in advance turned out to be non-events, which is worth recording so
they are not re-litigated:

- **The `open` macro collision.** `sys_compat.h` `#define`s `open` → `Sys_open`. Any libc header that
  declares `open` must be included *above* that macro or its declaration is rewritten into a
  conflicting prototype — silent on POSIX, where `<fcntl.h>`'s signature happens to match, and a hard
  error on Windows, where the UCRT adds `__declspec(dllimport)`. Phase 1 was written with the headers
  above the macro from the start, so it never fired. The ordering is now load-bearing and commented
  as such.
- **Text-mode `fopen`.** `Event.c:409` opens the binary `GREED.BLO` with `"rt"`, and `Menu.c:365-371`
  writes `savedir` through a `"w"` stream. Windows restores the CRLF/`^Z` translation that macOS
  silently didn't have — but these lines are byte-identical to `source_dos/CODE/{EVENT,MENU}.C` and
  were written for exactly those semantics. `savedir` is `char[10][21]` of space-padded printable
  names, so it carries no `0x1A` to terminate a read early. Left alone deliberately.

---

## Added, not a bug fix

**`-window`.** The built-in default is fullscreen (`Modplay.c:154` sets `SC.fullscreen=1`), and SDL
minimises a fullscreen window that never gains focus. A game launched from a script was therefore
invisible and impossible to observe. `-window` overrides `SC.fullscreen` for one run *without*
writing the choice back to `SETUP.CFG`.

**`GREED_SHOT` / `GREED_SHOT_AT`.** Framebuffer dumps at chosen ticks, marked `TEMPORARY` alongside
the existing `GREED_REPRO` harness in `Raven.c`. See *Verification* for why this is necessary rather
than convenient.

---

## Log

- **2026-08-05** — Plan approved. Decisions: hoist to `source_shared/`, clang-cl, bare `Greed.exe`,
  Mac available to verify the refactor.
- **2026-08-05** — Phase 0 hoist. 50 files `git mv`'d; git detected every one as a rename, so history
  follows. `source_macos/CMakeLists.txt` shrank 118 → 72 lines.
- **2026-08-05** — **clang-cl is not installed.** The VS 2022 `VC\Tools\Llvm` tree holds only
  redistributable DLLs; the "C++ Clang tools for Windows" component was never selected. Rather than
  trigger a ~1 GB install unasked, `engine.cmake` grew a three-way compiler branch and `build.ps1`
  auto-detects. MSVC `cl` then compiled the entire 1995 tree with one suppression.
- **2026-08-05** — First run reached `VI_Init` and printed `Video: HD, view 1664x780`. Two bugs
  (above). Both fixed same day.
- **2026-08-05** — Desktop screen-grabs returned the wallpaper three times before the cause was
  understood (Windows foreground restrictions, below). Switched to in-process framebuffer dumps and
  got usable images immediately.
- **2026-08-05** — Intro FLI and in-game 3D view both confirmed correct by looking at the dumps.
- **2026-08-05** — `source_shared` fix, so it lands on this port too: the music went silent a few
  hundred ms into every mission briefing. The libxmp stream is topped up by polling from
  `UpdateSound()`, which was reached only from `Sys_PumpFrame()` and the play loop's `TimeUpdate()` —
  and the briefings call neither, since every pause in them is a `Wait()` and `Wait()` spins on
  `Sys_Frame()`. Moved the `UpdateSound()` call into `Sys_Frame()` itself (after the tick loop, so it
  runs at 35 Hz) and dropped it from `Sys_PumpFrame()`. Nothing platform-specific about the cause or
  the fix; measured on macOS with a queue-depth probe, and untested on Windows only because the
  soundtrack has not been listened to on this port yet.
- **2026-08-06** — `source_shared` fix, so it lands on this port too: the music never looped. When a
  track ended the audio stayed silent until the next `PlaySong`. `Sys_MusicUpdate` passed `loop=1` to
  `xmp_play_buffer`, but libxmp's `loop` is a *maximum loop count*, not a flag, so it returned `-1`
  after a single pass and cleared `music_running`. Now `loop=0`, which disables the cutoff. The
  soundtrack modules loop themselves via `Bxx` position jumps written into the music (each section of
  the compilation modules jumps back to its own first order), exactly as DSIK followed them in DOS, so
  nothing else was needed — see `macos_plan.md` for the DSIK disassembly that ruled out the order-list
  emulation this first appeared to require. Verified on macOS with a headless libxmp harness tracing
  order positions; not yet listened to on this port.
- **2026-08-05** — More `source_shared` changes, all of which land here too. The HUD was never drawn:
  `currentViewSize` was pinned at 0, which draws no status bar, and in HD `ChangeViewSize` returned
  early so F9/F10 could not change it either (and jammed after one press). `MaxViewSize()` now caps HD
  at view size 3 — sizes 0..3 are all a full 320x200 view in `viewSizes[]` and differ only in HUD
  coverage — `InitData` applies the saved `SC.screensize`, and the default is 3. The options menu's
  camera delay row became "HUD DENSITY" driving that value, with the delay locked to its minimum.
  Key defaults changed too: A.S.S. cam C -> V, motion sensor S -> C (S is `bt_south` here, so walking
  backwards was toggling the sensor). SETUP.CFG is version 3 as a result and bindings from older files
  are dropped, since neither port has a key config screen to undo them with. All verified on macOS by
  framebuffer dump; untested on Windows, though nothing in it is platform-specific.
- **2026-08-06** — `source_shared` again: the help page is drawn in code instead of being the baked
  `INFO1` lump, so its key names come from `scanbuttons[]` and match whatever the port's defaults are.
  Backdrop is the `BRIEF3` artwork with its palette dimmed to 24% and text ramps built in the indices
  it leaves unused. Its wait loop pumps frames now rather than sitting in `Wait(10)` — worth noting
  here because an unrepainted window is more visible on Windows, where the compositor is happy to
  hand back a stale surface. Verified on macOS only.
- **2026-08-06** — `source_shared`, so it lands here too: the mission briefings' fade-outs can be cut
  short with a keypress now. Every other ramp on those pages already snapped; the fade to black
  between pages was the engine's `VI_FadeOut`, which tests no input, so the press that turned the page
  was followed by 64 unskippable ticks. A briefing-local `BriefingFadeOutPage()` replaces the seven
  `VI_FadeOut` calls inside `MissionBriefing`; `VI_FadeOut` itself is unchanged, which matters here
  because the intro and menus on this port lean on it too. Verified on macOS only — but note the
  briefings are still on this port's untested list below, and `-nointro` cannot reach them.
- **2026-08-06** — `source_shared`, so it lands here too: inventory cycling moved from Insert/Delete
  to `[` and `]`. Neither bracket existed in the port — no `SC_` constant, no `build_keymap()` row —
  so `SC_LBRACKET` 0x1a and `SC_RBRACKET` 0x1b were added alongside the `SDL_SCANCODE_LEFTBRACKET`/
  `RIGHTBRACKET` mappings, and `bt_invleft`/`bt_invright` repointed at them. The `ASCIINames` tables
  already held `[` and `]` at those codes, so the drawn help page picks the names up on its own.
  **SETUP.CFG is version 4 as a result**, and bindings from older files are dropped — same reasoning
  as the version 3 bump above, and the same reason it is not optional: without it a saved `ckeys`
  re-pins Insert/Delete and neither port has a key config screen to undo that. Verified on macOS by
  hexdumping the config across the upgrade (v3 `52 53` -> v4 `1a 1b`); untested on Windows, though
  nothing in it is platform-specific — the scancodes are the same IBM set-1 values on both.

---

## Current state

Verified today by running the tree, not by reading it:

- Cold configure and build of both `build/` and `build-debug/` succeed. SDL3 and libxmp are fetched
  and built from source; 436 build steps.
- `dumpbin /dependents build\Greed.exe` lists **only** system DLLs — `KERNEL32`, `USER32`, `GDI32`,
  `WINMM`, `IMM32`, `ole32`, `OLEAUT32`, `VERSION`, `ADVAPI32`, `SETUPAPI`, `SHELL32`. No `SDL3.dll`,
  no `VCRUNTIME140.dll`, no `MSVCP140.dll`. 3.5 MB, self-contained.
- The Softdisk intro FLI plays and animates through its palette fade, right way up, at 4:3.
- The 3D view renders textured walls, floors, ceilings and the weapon sprite correctly, in HD mode.
- `%APPDATA%\redshadow\Greed\` — like its macOS counterpart, **has never been created**, which is
  direct evidence that no save, no `SaveSetup` and no rebind has been written. The whole writable
  branch of `sys_files.c` is still unexercised at runtime on either platform.

Two warnings, both pre-existing and benign: `Event.c:276` and `R_walls.c:165`, C4700 uninitialised
local.

---

## Open / next

Roughly in the order the work is worth doing.

**1. Play it, end to end, once.** Every remaining doubt in this file is a thing that only playing
settles, and the macOS experience is unambiguous on this point: all three bug classes that port hit
were invisible until the code path in front of them worked, and each surfaced only when a human drove
the game. Specifically still unwatched on Windows: the menus, character select, the mission briefing,
audio of any kind, mouse look, and the ESC quit prompt.

**2. Save/load round-trip** to `%APPDATA%\redshadow\Greed\`. The largest untested surface on *either*
platform — `SAVEGAME.DIR`/`SAVEGAME.%i` writing, the prefs-dir-first read order, and a rebind
persisting through `SETUP.CFG`'s magic+version header. Testing it here validates both ports at once.

**3. Rebuild the macOS tree after the hoist.** The move was mechanical and the shared CMake is
exercised by the Windows build, which catches path mistakes — but nothing has been compiled on the
Mac since. This is the gate the plan originally put *before* Windows work; it slipped behind it
because the hoist and the Windows build were done in one sitting.

**4. Install clang-cl and switch to it.** `winget install LLVM.LLVM`, or the VS Installer component.
`build.ps1` picks it up automatically. This restores the real `-Werror=int-conversion` /
`-Werror=pointer-to-int-cast` guards instead of MSVC's approximate `/we` equivalents — the guards
that caught six real pointer-truncation bugs during the macOS port.

**5. HiDPI.** `SDL_WINDOW_HIGH_PIXEL_DENSITY` is set, and HD mode derived a 1440×900 view inside a
1296×999 window, which suggests DPI awareness is working. It has not been checked on a *scaled*
display. If it turns out blurry, the fix is an application manifest or the SDL DPI hint.

**6. Icon, GUI subsystem, packaging.** All deliberately out of scope. The exe is console-subsystem on
purpose (below).

---

# Context

`source_macos/` was a finished, playable SDL3 port of the 1995 Raven engine, built from
`source_win32/Win32/`, having fixed roughly fifteen classes of real bug — LP64 truncation, `bool`
shadowing, unpumped spin loops, read-only string literals, the bottom-up framebuffer myth. Those are
catalogued in `macos_plan.md`, which remains the reference for *why* the engine looks the way it does.

The 2002-era `source_win32/` tree cannot be revived on a modern machine: it needs 32-bit MSVC, x86
inline assembler and GDI palette APIs that no longer function. But the macOS port had already thrown
all of that away and replaced it with SDL3 — so the question was not "can this be ported to Windows"
but "how much of the macOS port is actually macOS".

**Goal:** a native x64 `Greed.exe` that plays the game, built from the same sources as the macOS app,
with no duplicated engine code.

**Decisions taken (from user):** hoist shared code into `source_shared/`; target clang-cl; produce a
bare executable with no installer or packaging; the Mac remains available to verify the refactor.

---

# Approach

## Layout

```
source_shared/
  src/                      24 .c + 10 .h — the engine, shared verbatim
  platform/
    sys_video.c  sys_input.c  sys_sound.c        shared verbatim
    sys_sdl.h    sys_greed.h                     shared verbatim
    sys_main.c   sys_files.c   sys_compat.h      shared, 9 #ifdef sites total
    compat/posix/  io.h conio.h malloc.h process.h tchar.h dos.h
    compat/win/    dos.h only
  cmake/
    deps.cmake              SDL3 + libxmp at pinned tags
    engine.cmake            greed_add_engine_target(): sources, flags, warnings

source_macos/               CMakeLists.txt (72 lines) + bundle/ + cmake/copy_data.cmake
source_win64/               CMakeLists.txt (27 lines) + build.ps1 + README.md
```

`source_win32/` and `source_dos/` are untouched. Per `CLAUDE.md`, `source_dos/` remains the authority
on intended behaviour.

**Why hoist rather than copy.** The alternative — copying `src/` and `platform/` into `source_win64/`
the way `source_macos/` was once copied out of `source_win32/` — would mean applying every future
engine fix twice, forever, and would leave `macos_plan.md`'s bug catalogue describing only one of two
trees. The hoist was done with `git mv` so all 50 files were detected as renames and their history
follows them.

## The compat shims had to split

`compat/` faked six headers for macOS: `io.h`, `conio.h`, `malloc.h`, `process.h`, `tchar.h`,
`dos.h`. On Windows the UCRT ships real versions of five of them, and shadowing those would hide the
genuine `filelength()`, `_open()` and `kbhit()` that the engine wants — worse, `compat/io.h` includes
`<unistd.h>`, which does not exist there.

Enumerating every system header the engine includes settles it: `CONIO.H`, `CTYPE.H`, `DOS.H`,
`FCNTL.H`, `IO.H`, `MALLOC.H`, `MATH.H`, `PROCESS.H`, `STDARG.H`, `STDIO.H`, `STDLIB.H`, `STRING.H`,
`SYS/STAT.H`, `TCHAR.H`, `TIME.H`. Windows supplies all but one. So `compat/win/` holds **only
`dos.h`**, and `engine.cmake` selects the directory. (The capitalisation resolves on both because
both filesystems are case-insensitive.)

## Toolchain

**Intended: clang-cl. Actual: MSVC `cl` 14.44.35207.**

clang-cl was chosen because it is the same compiler family as the macOS build, so `-std=gnu90` and
the four `-Werror=*-cast` guards transfer verbatim while still producing a normal MSVC-ABI exe with
PDBs. It ships with VS 2022 — but only if the "C++ Clang tools for Windows" component was selected,
and it was not. `VC\Tools\Llvm` contains only redistributable DLLs.

Rather than trigger a large install without asking, `engine.cmake` grew a three-way branch and
`build.ps1` prefers clang-cl, falling back to `cl`. Both work; the fallback is what runs today.

MSVC compiled the 1995 sources with a **single** suppression, `/wd4431`, for `Utils.c:515`
(`static weaponlump=0, numlumps=0;`). Two things matter for anyone maintaining this:

- **Do not add `/std:c11` or `/std:c17`.** MSVC's default C mode is the permissive
  C89-with-extensions that these sources need; the newer modes reject implicit int outright.
- The pointer-truncation guards are spelled `/we4311 /we4312 /we4302 /we4047 /we4133 /we4024 /we4029`
  under `cl`. These are an approximation of clang's, not an equivalent — which is why installing
  clang-cl is on the Open list.

`_CRT_NONSTDC_NO_WARNINGS` and `_CRT_SECURE_NO_WARNINGS` are defined because the engine uses
`open`, `filelength`, `stricmp` and `S_IREAD` throughout — all real in the UCRT under their non-stdc
names, all deprecated in favour of underscored spellings, and all wanted as-is.

### `build.ps1`

`cmake`, `ninja` and the compiler all ship with VS 2022 but none are on `PATH`, so the script imports
the developer environment via `Enter-VsDevShell` and uses full paths. Two Windows-specific snags,
both fixed and commented:

- `-Debug` cannot be a parameter name — PowerShell reserves it as a common parameter and refuses to
  load a script that redefines it. The switch is `-Config Release|Debug`.
- `VsDevCmd.bat` shells out to a bare `vswhere`, so without the installer directory on `PATH` it
  prints "not recognized" before carrying on regardless. The script prepends it.

Single-config Ninja rather than the multi-config VS generator, so `build/` means Release and
`build-debug/` means Debug exactly as on macOS, and `cmake --build build` means the same thing on
both platforms.

## Dependencies

`source_shared/cmake/deps.cmake` is shared unchanged apart from two `MSVC`-guarded additions, both of
which must be set **before** `FetchContent_MakeAvailable` or the subprojects are already configured:

- `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` — static CRT, so `Greed.exe`
  runs on a machine with no Visual C++ redistributable. This is the equivalent of the fully-static
  `Greed.app`.
- `SDL_FORCE_STATIC_VCRT ON` — SDL's own switch for the same thing. Without it SDL builds `/MD` and
  the link fails on mismatched runtimes.

SDL3 `release-3.4.14` and libxmp `libxmp-4.7.2`, both built from source at pinned tags, both linked
statically, target names `SDL3::SDL3-static` and `libxmp::xmp_static` — identical to macOS, no
Windows-specific handling needed. SDL3 carries `user32`/`gdi32`/`winmm`/`imm32`/`ole32`/`setupapi`/
`shell32` as interface link libraries exactly the way it carries the macOS frameworks, so static
linking needed no manual library list. The `CMAKE_POLICY_VERSION_MINIMUM 3.5` workaround for libxmp's
old `cmake_minimum_required` is still required.

---

# Phases

## Phase 0 — Hoist

Pure mechanical move, no logic changes.

1. `git mv` `src/`, `platform/` and `cmake/deps.cmake` from `source_macos/` into `source_shared/`.
2. Split `compat/` into `compat/posix/` (all six existing shims, unchanged) and `compat/win/`
   (`dos.h` only).
3. Extract the target definition from `source_macos/CMakeLists.txt` into
   `source_shared/cmake/engine.cmake` as `greed_add_engine_target(<name>)`, carrying the source glob,
   include dirs with the compat directory selected by platform, `C_STANDARD 90` / `C_EXTENSIONS ON`,
   the force-included `sys_compat.h`, and the full warning set including the pointer-truncation
   guards.
4. Reduce `source_macos/CMakeLists.txt` to two `include()`s, one `greed_add_engine_target()` call,
   and the bundle steps it already had.

## Phase 1 — Windows branches in the shared platform layer

### `sys_compat.h` — 3 sites

The libc includes split by platform, and they sit **above** the `#define open Sys_open` lines. That
ordering is the whole ballgame: the UCRT declares `open` with `__declspec(dllimport)` and a differing
signature, which the macro would rewrite into a conflicting redeclaration of `Sys_open`. On POSIX the
same arrangement is harmless by luck, because `<fcntl.h>`'s signature matches exactly.

`stricmp`/`strcmpi`/`strnicmp` are *not* defined on Windows — they are real there, and defining over
them collides. `strtok_r` maps to `strtok_s`, which takes the same three arguments in the same order.

### `sys_files.c` — 4 sites

- `GREED_DATA` separator: `;` on Windows, because `C:\Greed` contains a colon.
- `is_sep()` accepts `\` as well as `/`, since `SDL_GetPrefPath` and `SDL_GetBasePath` return
  backslash-terminated paths.
- `is_absolute()` accepts `X:\`, `X:/` and UNC `\\server\share`. Paired with
  `is_absolute_and_exists()` for reads — see *Bugs found*.
- The base-directory search entry: macOS appends `../Resources` because the binary lives in
  `Contents/MacOS/`; everywhere else the data sits next to the executable.

`access(out, R_OK)` was replaced with `SDL_GetPathInfo(out, NULL)` — portable, and it removes this
file's only libc dependency rather than adding an `#ifdef` for `_access` with a bare mode number.

`dos_to_posix` was renamed `normalize_dos_path`. It is still needed on Windows: the drive letter is
just as bogus there (`cdr_drivenum + 'A'` with `cdr_drivenum` zero, i.e. the floppy drive) and the
installer's `GREED\` prefix still has to go. Only the separator flip is a no-op.

### `sys_main.c` — 2 sites

Cmd-Q and Cmd-F are guarded behind `SDL_PLATFORM_APPLE`. `SDL_KMOD_GUI` is the *Windows key* here, and
`Win+Q` is the OS search shortcut — binding quit to it would be a nasty surprise. `Alt-F4` already
arrives as `SDL_EVENT_QUIT` and is handled. F11 fullscreen stays on both.

The startup banner now uses `SDL_GetPlatform()` instead of a hardcoded `"macOS"`.

### `src/` — no changes

Two things deliberately *not* done, both recorded above under *Predicted and did not happen*: no
`fopen` mode changes, and no touching of `%lu`/`%ld` format specifiers. Windows x64 is **LLP64** —
`long` is 32-bit here and 64-bit on macOS — so the specifiers that were bugs there are trivially
correct here. The narrowed types (`longint`, `fixed_t`, the `sp_*` steppers) are correct under both
models.

## Phase 2 — The `source_win64` build

27 lines of CMake. It includes the two shared cmake files, calls `greed_add_engine_target(greed)` and
sets `OUTPUT_NAME`.

It deliberately does **not** set `WIN32_EXECUTABLE`. `sys_main.c` uses `SDL_MAIN_HANDLED` with a plain
`main()`, and a console subsystem keeps `printf` and `MS_Error` output visible — which is how
essentially every bug in `macos_plan.md` was actually found. Worth flipping to a GUI subsystem once
the port has been played through.

No data-copy step, no `.rc`, no icon, no installer, per the bare-exe decision. Data comes from
`GREED_DATA`, which `sys_files.c` already reads as a search list.

## Phase 3 — Build, run, fix

See *Bugs found and fixed*.

---

# Verification

**Neither platform allows an ordinary screenshot, for different reasons.** macOS blocks
`screencapture` by permission. Windows refuses to let a process launched from a script raise its own
window to the foreground (`SetForegroundWindow` returns false), so a desktop grab captures whatever
was already on top — which cost three attempts and three pictures of the desktop wallpaper before the
cause was understood. Compounding it, the default is fullscreen and SDL minimises a fullscreen window
that never gains focus, so there was nothing to capture in the first place.

The reliable answer on both is to read the presented framebuffer from inside the process:

```powershell
$env:GREED_SHOT = "C:\temp\shot"        # writes shot<tick>.ppm
$env:GREED_SHOT_AT = "60,150,300"       # default 70,210,420
.\build\Greed.exe -window -nointro
```

`VI_DumpPresented` (already present from the macOS work) calls `SDL_RenderReadPixels` and writes a
P6 PPM, so it captures exactly what a player would see including the HD path. Pillow is not installed
here; `scratchpad/ppm2png.py` converts with `zlib` + `struct` alone.

The checklist:

1. **Both configs build clean from cold**, with the pointer-truncation guards active and no new
   warnings beyond the two known-benign C4700s. ✅
2. **Self-contained exe** — `dumpbin /dependents` lists only system DLLs. ✅
3. **Intro FLIs** play, animate through the palette fade, right way up, at 4:3. ✅ *(watched, ticks
   40/120/260)*
4. **3D view** renders textured walls, floors, ceilings and sprites. ✅ *(watched, ticks 60/150/300,
   `-nointro`)*
5. **Framebuffer orientation** — menus, status bar and cutscenes right way up, not just the 3D view.
   Those two cancelled out on macOS and hid the bug for a whole port. ⬜ *cutscenes confirmed; menus
   and status bar not yet*
6. **Input** — WASD, mouse look, both buttons, F11, Esc opening the menu without freezing, and one
   press advancing exactly one menu level. ⬜
7. **Save/load round-trip** to `%APPDATA%\redshadow\Greed\`, plus a rebind persisting. ⬜
8. **Audio** — SFX panning and pitch variation, music on level load and change. ⬜
9. **Timing** — `-ticker` on a high-refresh monitor, confirming the 35 Hz tick is independent of
   frame rate. ⬜

Note `-nointro` proves nothing about `MissionBriefing`, which opens with
`if (netmode || nointro) return;`. On macOS that is exactly why a crash there survived a whole
session of headless testing.

A DOSBox run of `greed_final/GREED.EXE` remains the reference for palette and geometry correctness.

---

# Out of scope

Networked multiplayer (`Net.c` compiles but stays disabled — it is IPX/serial), the DOS-only tools,
an icon, an installer, code signing, a GUI subsystem, and an ARM64 Windows slice. The last would be a
small change now that both dependencies build from source, but it is untried.

---

# Upscaled art: 4× textures (Real-ESRGAN), 2× cutscenes (NVIDIA VSR)

Everything here is under `source_shared/`, so it applies to both ports. The design rationale,
the two traps and the incidental fixes are written up in **[`macos_plan.md`](macos_plan.md)**; this
section covers running the pipeline on Windows and what is left to do.

The cutscene half is Windows-only: it needs an NVIDIA GPU, so macOS either takes the movies built
here or rebuilds them with `make_fli.py --backend esrgan`.

## Running it

```powershell
cd tools\hdtex
.\setup.ps1                                   # venv + torch + weights + NVEncC
.\.venv\Scripts\python roundtrip.py           # prove the codecs first
.\.venv\Scripts\python nvvsr.py --selftest    # prove the VSR colour path
.\.venv\Scripts\python make_hd.py --all       # extract -> upscale -> pack
.\.venv\Scripts\python make_fli.py            # cutscenes, ~20 min
```

`setup.ps1` pins the interpreter to a version torch actually ships wheels for. `py -3` picks the
newest *registered* CPython, which on this machine is a 3.15 alpha with no torch wheel on any index —
the failure looks like "no matching distribution for torch" and is easy to misread as a bad CUDA
index. 3.13 + cu124 is what works here.

`realesrgan` and `basicsr` are deliberately not dependencies: `basicsr` imports
`torchvision.transforms.functional_tensor`, removed in torchvision 0.17, and neither package is
maintained. `rrdbnet.py` vendors the ~120 lines of model definition and loads the official `.pth`
directly.

Iterate on one class with `--only flat`, swap models with `--model x4plus_anime`, and look at the
result with `contact_sheet.py <class>` before packing. For the cutscenes the equivalent is
`fli_compare.py --only CITYBURN`, which puts every backend side by side on the same frames after
requantization — the only fair comparison, since the palette is fixed by the format and how
gracefully an upscaler survives being squeezed back into it is most of what matters.

## Verifying

`tools\hdtex\shots.ps1 <tag>` captures framebuffer dumps in **both** render modes via `GREED_SHOT`
(neither platform allows an ordinary screenshot — see `CLAUDE.md`). Compare hashes against a `base`
tag captured from the pre-change build:

```powershell
.\shots.ps1 base        # then make changes, rebuild
.\shots.ps1 stage3
```

Original mode on original art must stay byte-identical through every stage that claims to be a
no-op. It did for the plumbing, the loader hardening and the archive overlay.

`shots.ps1` passes `-nointro`, which returns out of `MissionBriefing` before a single FLI plays, so
it proves nothing whatever about the cutscenes. **`introshot.ps1 <tag>`** is the one for those: same
`GREED_SHOT` mechanism, no `-nointro`. `DemoIntro` hands straight to `DemoIntroFlis` when
`MOVIES/TEXT.FLI` exists (`Intro.c:477`), and TEXT.FLI is 390 frames at speed 5 — 1950 ticks — so
the default `-At 300,700,1100,1500` all land inside it. This is what showed that ESRGAN had erased
the starfield: a contact sheet suggested it, the framebuffer dump proved it.

**`GREED_KEYS=<tick>:<scancode>[:<hold>],...`** injects scancodes into `keyboard[]`, because the
menus are keyboard-driven and a script-launched window cannot take focus — so dialogs like
`ShowQuit` were simply unreachable in an automated run, which is why the trail bug survived. It
holds the key down over a window of ticks rather than pulsing it, since `MenuCommand` edge-detects
Enter and rate-limits the arrows. `quitshot.ps1 <tag>` uses it to reach the QUIT dialog:
`-nointro` starts *in* a game, not at the menu, so the sequence is Esc (`0x01`) to open the menu,
DownArrow (`0x50`) onto QUIT, Enter (`0x1c`).

Note `hudscale` follows whether the HD pack loaded, **not** `GREED_MODE` — that only sets the 3D
view resolution. To test the hudscale-1 path, move `greed_final/GREED_HD.*.BLO` aside; a
`GREED_MODE=original` run with the pack present is still hudscale 4 and proves nothing about it.

Note `build.ps1` writes progress to stderr, so `2>&1` turns a successful build into a PowerShell
`NativeCommandError` and a false failure. Don't redirect it. (`Join-String` is also absent — this is
Windows PowerShell 5.1, not 7.)

## Status

| class | count | pipeline | engine |
|---|---|---|---|
| walls | 262 | OK | OK 256x512 / 256x256 |
| flats | 232 | OK | OK 256x256 |
| sprites | 1508 | OK | OK widened layout, `spriteshift` |
| pics | 136 | OK ESRGAN + EPX on edges | OK `hudscale`, 1280x800 chrome |
| fonts | 3 | OK EPX only | OK widened `charofs`, `hudscale` |
| cutscenes | 39 / 5722 frames | OK NGX VSR 2x | OK `VI_BlitLogical`, 640x400 |

All six classes ship. The sidecar is 209 MB across five `GREED_HD.NNN.BLO` parts and the movies
283 MB; both are committed, so a clone is playable without a GPU. Live switching works in both
directions (`GREED_FLIP=400,800`), and original render mode on original art is
byte-identical to the pre-change build throughout.

## The backdrop and the cutscenes

Both are 4x too, and both needed their own treatment.

**The sky** is a 256-square buffer assembled from two 256x128 lumps and *sampled*
rather than drawn -- wrapped column, scaled row -- so it scales like a flat:
`skyshift` 0 or 2, `SKYSIZE` 256 or 1024. The subtlety is `backtangents`, which
maps a view column to a sky column. Shifting its result left by 2 would sample
only every fourth column, i.e. the same 256 columns replicated with no detail
gained; it is instead computed with two more bits kept
(`>>(FRACBITS-skyshift)`), which is where the horizontal resolution actually
comes from. `BACKDROPHEIGHT` scales for the vertical.

Two things worth knowing. The sky is only visible on 13 of the 32 maps -- the
first eight have none, so `GREED_MAP=27` (pillars, 2544 sky tiles) is the one to
test on. And `LoadBackdrop` replaces an `lseek`/`read` straight off
`cachehandle` at `infotable[lump].filepos+8`; that skipped the pic header, which
is the only reason it worked, and it reads the *wrong file entirely* once a
sidecar is loaded, because those lumps then live on the other handle.

**The cutscenes** are 39 FLI files, 5722 frames, which cannot be upscaled at run
time -- so `tools/hdtex/fli.py` decodes and re-encodes them offline and
`make_fli.py` puts 640x400 copies in `greed_final/MOVIES/`. The decoder in
`Playfli.c` was hardcoded to 320x200 in four places (`chunkbuf` at 64000, an
explicit 320 row stride, and the COPY/BLACK chunk sizes) despite reading
`header.width`/`height`; it now uses them, and `VI_BlitLogical` takes source
dimensions so an original and an upscaled movie both fill the screen -- it
point-samples in both directions, doubling 640x400 into HD chrome and reducing
it into the original renderer's 320x200.

The upscaler is **NVIDIA NGX DLVSR**, the model behind RTX Video Super
Resolution, not the Real-ESRGAN the texture pack uses. There is no Python
binding for it; `tools/hdtex/nvvsr.py` drives it through rigaya's NVEncC, which
wraps it as `--vpp-resize algo=ngx-vsr` and ships `nvngx_vsr.dll` inside its own
archive. The other candidate -- the D3D11 video-processor extension that
Chromium and mpv use for the browser feature -- is documented for 360p-1440p
input and would have refused 320x200; the offline NGX API takes it happily.

Two things about that path are worth recording because neither is obvious and
both were measured rather than assumed. **ngx-vsr always emits limited-range
YUV, whatever `--colorrange` says** -- the flag only tags metadata. Fed
full-range samples it clamps 0 to 16 and 255 to 235 while leaving mid-tones
alone, crushing every pure black and white in a cutscene; the first sheets came
out visibly dark. Feeding limited-range 10-bit 4:4:4 makes the round trip
exactly lossless. And once that was fixed, **ngx-vsr differs from plain
`lanczos4` by a mean absolute difference of only about 1.4** -- most of what had
looked like the model working was the clamp. `nvvsr.py --selftest` now checks
the colour round trip, a grey ramp for the clamp, and the filter chain NVEncC
reports, because NVEncC will silently substitute an ordinary resize for a scale
factor the network does not support.

Encoder notes are in `tools/hdtex/README.md`. Three matter: BRUN and LC use
*opposite* sign conventions for run-vs-literal and inverting either produces a
frame that looks almost right; delta compression loses to whole-frame RLE on
high-motion content, so the writer tries both and keeps the smaller; and because
the FLI header can only be written at close, an interrupted conversion leaves a
file with a zeroed signature -- which is why `--resume` validates frame counts
rather than testing for existence.

The shipped set is **283 MB for all 39 against 43 MB of originals** (5722 frames,
every one decoded back and validated at 640x400), because the upscaled frames are
far higher entropy than the dithered art the format was tuned for. The 4x set
that used to sit beside it is gone; 640x400 is the only size now.

VSR costs about twice what ESRGAN did at the same 2x -- 283 MB against 156 MB,
and roughly 2x per movie across the board. That is the same fact as the quality
difference seen from the other side: ESRGAN smooths, so its output RLEs well,
while VSR preserves the source's dither and banding, which is precisely what the
FLI encoder cannot compress. Going in, the smaller set looked like an argument
*for* VSR. It turned out to be the argument against, and it loses:

**ESRGAN deletes fine detail and invents geometry.** On TEXT.FLI -- the intro,
the one `DemoIntro` probes for -- it erases the entire starfield. Not thinned:
gone, a pure black background, confirmed in framebuffer dumps from the running
game, not just in contact sheets. On CITYBURN it restructures the skyline at a
tight crop, merging towers and losing a thin one into a dark mass. ngx-vsr keeps
both, at about 17% more lit pixels on the same frame. Hence the cutscenes moved
to VSR and the texture pack did not.

What a smaller codec is worth, measured over a stratified sample of the old 4x
set rather than estimated. The ratios still hold; the absolute figures were
taken at 1280x800 and are kept for the shape of the result:

| | size | vs FLI |
|---|---|---|
| upscaled FLI at 4x (former) | 593 MB | - |
| + deflate-9 whole frames | ~391 MB | 1.5x |
| + zstd-19 whole frames | ~348 MB | 1.7x |
| per-frame best of FLI/deflate | ~330 MB | 1.8x |
| H.264 CRF 18 | ~64 MB | 9.5x |

The interesting result is that general-purpose compression is a poor deal here.
It only manages 1.5-1.7x, and on the *low*-motion movies (RUBBLE, JETTISON,
INSHIP04) it is worse than FLI, because compressing whole frames throws away the
inter-frame delta that FLI's LC chunk exploits. A hybrid picking the smaller per
frame reaches maybe 1.8x -- for the cost of vendoring a decompressor and adding
a chunk type. Halving the dimension beat all of it, for free, which is what 2x
is now doing.

H.264 is the only option that changes the picture, and it is not a drop-in: the
cutscene path is 8-bit paletted end to end, so it needs a decoder (libavcodec,
or Media Foundation plus VideoToolbox) and a present path that bypasses `screen`
and the palette LUT. That last part is also the upside -- it would make the
cutscenes truecolour rather than 256 indices.

## Things that bit, and why

**The sidecar is split into five files, and a part number is not a flag.** GitHub refuses a blob
over 100 MB and warns over 50, against a 209 MB pack, so `pack.py` writes `GREED_HD.001.BLO` …
`.005.BLO` at ~43 MB each and `CA_OverlayArt` merges whatever it finds. No format change was
needed: a part is just a partial pack over the full lump number space, the shape `--only wall`
already produced, so any subset loads and the rest falls back to the original art.

What bit was `CA_SetArtMode`. `lumpsrc[]` now records *which file* holds each lump, but that
function still did `lumpsrc[i]=(byte)hd` when switching art sets — rewriting every part number to
1. Reads then went to part 1 at offsets belonging to part 3, which is exactly the failure mode that
does not announce itself: the seek succeeds, the read succeeds, and the bytes are plausible
garbage. It presented as a hang during `RF_PreloadGraphics` with no error at all. `hdpart[]` holds
the mapping and `lumpsrc[i] = hd ? hdpart[i] : 0`.

Verified byte-identical: framebuffer dumps at ticks 120 and 300 hash the same for a five-part pack
and an undivided one, and the runtime art swap (`GREED_FLIP=200,400`) still works both ways.

**`64000` was an alias for "the whole screen", and at 4x it is 1/16th of it.**
The engine spells the framebuffer size as a literal 64000 in about fifty places.
`SCREENBYTES` replaced them, but a family of save/restore pairs was missed --
`ShowQuit`, `ShowPause`, `MenuAnimate`, `StartWait`/`EndWait`, `RunBrief` and
`MissionBriefing` all stash the chrome with `memcpy(viewbuffer,screen,64000)`
and put it back afterwards. At hudscale 4 that preserved the top 50 rows and
abandoned the other 750, so anything drawn over the menu left its art behind.

Worse, the sliding dialogs restore the rows they vacate as they move, and that
had been *switched off* in HD rather than scaled:

```c
if (y>=0 && hudscale==1) memcpy(ylookup[y],viewbuffer+320*y,640);
```

Two rows at a 320 pitch -- right only at hudscale 1. With it disabled the QUIT
and PAUSE boxes smeared a trail down the screen as they dropped. `VI_DrawMaskedPic2`
had the same shape of workaround for its masked pixels, `source2=(hudscale==1) ?
... : NULL`, justified with a comment claiming `screen` already held the right
background -- true only if nothing had overdrawn it, which is exactly what the
sliding box does. Both now scale by `screenpitch`/`hudscale` and behave the same
at hudscale 1, where the expressions reduce to the originals. `RestoreChromeRows`
in `Menu.c` is the shared helper.

The lesson is the general one for this port: when an HD path was "disabled
rather than scaled", the guard is the bug, not the fix.

**Sprites, pics and fonts all overflowed their 1995 containers at 4x.** Sprite
`collumnofs` is int16 with only 256 entries against a needed 704, and per-column
`top`/`bottom` are bytes against a needed 780; font `charofs` is int16 against
33506. `hdformats.py` emits widened layouts and `R_refdef.h`/`D_font.h` carry
`SP_*` / `FN_CHAROFS` accessors that select on `spriteshift`/`hudscale`. The
decode was open-coded in fifteen places; folding it into accessors first, and
proving that a no-op, is what made the format change safe.

**Palette indices in the status bar carry meaning, not colour.** The art paints
index 254 across the meter regions and `Display.c` rewrites 254 -- or a 113..168
value it wrote previously -- with a gradient from shield/health every frame.
Upscaling destroyed all 524 marker pixels and invented ~80 spurious ones, so the
meters would have stopped filling while unrelated pixels flashed. Marker pixels
are now replicated 4x4 as *indices* and those ranges are excluded from the
quantizer elsewhere; counts come out at exactly 16x the original.

**Index 0 means transparent in every `VI_DrawMaskedPic` path.** Excluding it from
the quantizer stops new holes appearing but also destroys the real ones -- the
weapon rendered inside a black rectangle. Masked pics now get the same
fill-upscale-remask treatment as sprites.

**Real-ESRGAN cannot upscale a glyph, and fonts were a third instance of the
palette-indices-carry-meaning trap.** Text is 6 pixels tall at 320x200, below
anything the model preserves, and it has no notion of a letterform: it blurs
strokes together and hallucinates into the gaps. Two places were affected.

`_do_font` assumed every font lump held ramp offsets that `FN_RawPrint` adds
`fontbasecolor` to. That is true of `font1` alone. Every call site selecting
`font2` or `font3` sets `fontbasecolor=0` (26 of them in `Display.c`), so those
bytes *are* palette indices -- the lumps hold five and nine distinct values.
Stretching them to 0..255, running a GAN over them and compressing back produced
every intermediate value: 35% of `font2`'s upscaled pixels and 48% of `font3`'s
were indices appearing nowhere in the source, landing in unrelated parts of the
VGA palette, so green HUD glyphs came out fringed with red, white and grey.
`font1` had no stray values but its 8 levels have no headroom for the model's
contrast expansion -- level 1 came out 3x over-represented and 2..6 about 4x
under, collapsing the shading to a core plus a dark outline.

And the **help screen is a pic, not a font**: `ShowHelp` (`Menu.c:1229`) calls
`loadscreen("INFO1")` and draws lump 2322 as one full-screen bitmap, no font code
involved. That text is baked into the art, and the 4x version read `VERPON
SELECTION`, `LEPTSHIFT`, `DUMP`, `SPADE BAR`. Text on the plain black background
survived only by accident -- `info1` contains index 0, so it is flagged `masked`
and the transparency re-cut happened to restore a sharp silhouette there. Over
the gun artwork nothing saved it.

`tools/hdtex/pixelart.py` adds EPX/Scale2x, which *copies* each output pixel from
one of five input neighbours, so the output value set is a subset of the input's
by construction: no quantizer, no interpolation, no stray indices, and index 0
survives exactly. Fonts are EPX end to end and no longer load a model at all --
per glyph, because `extract._font_atlas` packs them with no gutter and EPX's 3x3
window would otherwise pull a neighbouring letter into a glyph's edge columns.
Pics keep the model for artwork and take EPX only where the source has a hard
edge (Oklab neighbour delta over `--sharp-threshold`, default 0.30, dilated one
pixel). The threshold is not delicate: on `info1` coverage is 26.8% at 0.25 and
26.6% at 0.35, so anything on that plateau gives the same mask. Object
silhouettes go sharp too, which is wanted -- the model's ringing there was the
other thing that looked wrong at 4x.

**Chrome coordinates come in two units and mixing them is silent.** Callers pass
320x200 logical and the blit primitives scale on the way in; but the automap, the
radar, the rear-view inset and the weapon position compute directly against
`hudWidth`/`hudHeight`, which are chrome pixels. Scaling those a second time put
writes thousands of pixels past the end of a row. The rule that emerged: if a
statement mentions `hudWidth`/`hudHeight`, it is already in pixels and must not
go through `VI_HudFill`/`VI_HudPut`.

**Switching art sets must restore the working set, not a list of classes.**
`CA_SetArtMode` frees every overridden lump; re-caching walls and flats by hand
left 1508 sprite lumps NULL and `DrawSprite` dereferenced one on the next frame.
`RF_SetArtMode` now snapshots `lumpmain` residency first and restores exactly
that, re-applying the 255->0 sprite patch on the way. Which monsters are resident
depends on the level, so nothing else could have reconstructed it.

Debugging note: the crash was found by linking with `/MAP:greed.map` (inject via
`CMAKE_EXE_LINKER_FLAGS` in the build cache -- write it back **without** a BOM or
CMake rejects the file) and resolving the Windows Event Log fault offset against
it. Far quicker than bisecting by inspection.

## The status bar was unreachable, and the parts behind it were half converted

Reported as "the HUD doesn't work". It did not: at no view size and by no key
was a status bar reachable, and three of the pieces that draw into it would not
have rendered correctly if it had been.

**`SC.screensize` was pinned to 0 and `ChangeViewSize` refused every size in
HD.** Two independent stops on the same setting. The pin (`Modplay.c`) was there
because the options menu lost its screen size slider in 6c, so a stale
`SETUP.CFG` value could have stranded the player with no way back. The guard
(`Utils.c`) was there because in HD the render resolution comes from the display
and `viewSizes[]` cannot drive it.

Both were too broad. `viewSizes[]` says sizes 0..3 are *all* 320x200: they do not
resize the view at all, they only vary how much status bar chrome is drawn over
it, which composites at any resolution. Only 4 and up shrink the view. So HD
refuses `>= 4` now, `MAXHDVIEWSIZE`, and the setting is clamped rather than
pinned -- F9/F10 in play are the control, so a bad value is recoverable and only
the range needs enforcing. This is the same lesson as the sky: when an HD path
was disabled rather than scaled, the guard is the bug.

**`newplayer`'s view size restore cannot restore anything.** The DOS original
ends its table-rebuild walk with

```c
for (i=0;i<currentViewSize;i++) ChangeViewSize(true);
```

whose bound is the counter's own target, so every step raises the bound too and
it climbs to the smallest view the engine allows. Harmless for thirty years
because `currentViewSize` was always 0 here; live the moment a non-zero size
became reachable. It now walks to `SC.screensize`, which also makes a saved size
take effect at level start rather than only after a visit to the menu. The
`Menu.c` apply loop got the same treatment: it tests the size `ChangeViewSize`
may decline to change, so it has to stop on "no movement" rather than spin.

**The `...1` display family had its writes converted and its reads left behind.**
`displaystats1` writes through `VI_HudPut` (logical) but tested the marker with
raw `*(hudylookup[i]+j)` (pixels), so at hudscale 4 it sampled (j,i) of a
1280x800 buffer -- up in the 3D view, never 254 -- and no meter ever filled.
`VI_HUDPEEK` is the read that pairs with `VI_HudPut`. Exact, because the art
pipeline replicates marker indices 4x4 as indices; see the note on palette
indices carrying meaning. The `...2` family (view sizes 5..9) had been converted
correctly, which is why this survived: nothing reached the `...1` family.

`displaysettings1`'s three indicator lights were raw `memset`s, all three landing
on row 199 of the 1280x800 buffer rather than on the bar. Their "off" branch is
now gone rather than converted: at these view sizes the status bar lump is
redrawn every frame, so the unlit light is already what the art shows, and in HD
erasing to index 0 -- the chrome layer's transparent key -- punched a hole
through the bar to the view behind it. That was the "writes index 0 to erase HUD
elements, only reachable at HUD levels >= 1" limitation noted in `macos_plan.md`;
it is closed, not merely unreachable.

`displaynetbonusitem1`'s 30x30 portrait copy is the last one, now
`VI_DrawPicToBuffer` -- `VI_DrawPic` to the chrome target, which is not `screen`
outside HD.

**Toggling the automap segfaulted, at every view size.** `displayarrow` had been
converted to `VI_HudPut`, but its only caller passes `hudWidth/2+1`,
`hudHeight/2+1` -- already pixels. At hudscale 4 the write landed at four times
the centre of the buffer, past the end of `hudylookup[]`: a wild pointer, not a
stray pixel. Pixels are the right unit here because the map around the arrow is
drawn in pixels too, four per tile, so a logical arrow would be twelve pixels
across a four-pixel grid.

**`displayheatmode` and `displaymotionmode` had the border logical and the
contents in pixels.** The 66x66 border was drawn four times life size around a
64x64 patch of cells stranded in its top-left corner. `displaymotionmode` had it
both ways inside one function -- sprite dots through `VI_HudPut`, the player dot
raw. Contents follow the border: a cell is a logical pixel, which keeps the
sensor the same fraction of the screen it was at 320x200.

Verified by reading the framebuffer with `GREED_SHOT` and driving the keys with
`GREED_KEYS` (`0x43` F9, `0x32` M, `0x23` H): the bar appears at each of levels
1..3 and each panel draws; the health sweep animates between frames, which only
the `==254` read can produce; the map and heat lights probe green and red when
lit and return to the art's unlit grey when not, with no hole; the automap draws
walls and arrow instead of crashing. Then with `GREED_HD.*.BLO` moved aside --
the only way to test hudscale 1, since `GREED_MODE=original` with the pack
present is still hudscale 4 -- levels 4 and 5+ and the `...2` family are correct
too.

**Known limit, unchanged:** the automap and the heat/motion *map* overlays are
drawn at chrome-pixel density, so in HD they are a quarter of their original size
relative to the screen and show four times as many tiles. That is a deliberate
consequence of the pixel-unit rule above, not a regression; making them logical
would enlarge them and narrow what they cover.
