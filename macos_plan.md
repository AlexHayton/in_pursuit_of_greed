# macOS port of *In Pursuit of Greed*

## Status

*Updated as work proceeds. Last updated: 2026-08-04.*

All five planned phases are implemented. The port builds clean and plays: the intro FLIs, the menus,
character select, the mission briefing, the 3D view, sprites, sound effects and the tracker soundtrack
all run, each having been fixed in response to something the user or a framebuffer dump actually showed.
What remains is verification and polish, not construction — see **Open / next**, which is honest about
which of those have been *watched* and which merely no longer fail.

| Phase | State | Notes |
|---|---|---|
| 0 — Scaffold tree + CMake build | **done** | `cmake -B build && cmake --build build` works from cold |
| 1 — Compile existing C on clang/arm64 | **done** | 34 engine sources build; LP64 hazards fixed |
| 2 — SDL3 video, input, timing | **done** | Game boots to a playable, rendering level |
| 3 — Audio (SFX + libxmp music) | **done** | Confirmed audible by the user |
| 4 — Modernised controls | **done, untuned** | WASD + mouse look work now the `bool` fix has revived the keyboard; sensitivity has had no tuning pass |
| 5 — `Greed.app` bundle | **done** | Fully static, icon, licences; runs from any cwd |

Build and run, for the record — there is **no bare `build/greed`**; `MACOSX_BUNDLE` means the binary
lives inside the app:

```sh
cd source_macos
cmake --build build -j8
open build/Greed.app                                    # or, to see stdout:
GREED_DATA=../greed_final:../greed_cdrom ./build/Greed.app/Contents/MacOS/Greed
```

### Bugs found and fixed

The LP64 pointer-truncation class the plan predicted was real, and bigger than expected:

| Site | Bug |
|---|---|
| `D_disk.c:98` | `lumpmain` (`void**`) allocated with `sizeof(int)` — half size |
| `Utils.c:346` | `wallposts` (`byte**`) allocated with `*4` — half size. **Caused the first crash**: NULL texture columns segfaulted `ScalePost` as soon as a wall was drawn |
| `R_refdef.h` `span_t.shadow`, `mr_shadow` | Stored a *colormap pointer* in an `int`; truncated on LP64 |
| `R_public.c:215` | `colormaps` page-aligned through `(int)` — truncated the heap pointer |
| `R_public.c:35` | `viewLocation` held `(int)screen`; a DOS leftover, never dereferenced |
| `D_global.h:29` | `longint` was `unsigned long` (8 bytes); must be 32-bit for packed disk structs and `timecount` wraparound |
| `Display.c:1148+` | `%lu` on a now-32-bit `longint` — misreads varargs on arm64 |
| `R_public.h:32` | `fixed_t` was `long`, so 64-bit. The engine's 16.16 math was written for 32-bit `imul`/`shrd`, which truncates; widened, several steppers stopped wrapping. Now `int` |
| `R_refdef.h:149` | `sp_frac`/`sp_fracstep`/`sp_loopvalue` were `long`, same problem — now `int` |
| `Raven.c:3209` | The `-ticker` overlay passed two `ptrdiff_t` pointer differences to `%i`. 64-bit on LP64, so it misread the varargs and shifted every field after it |
| `Protos.h:678` | `greedcom_s.id` was `long`, widening the DOS IPX driver's ABI struct by 4 bytes and breaking every field after it. Also made unsigned so its `0xC7C7C7C7` magic can compare equal |

**All keyboard input was dead, and it took the intro down with it.** The engine's `bool` is
`typedef enum{false,true} bool` — int-sized, 4 bytes. SDL's headers pull in `<stdbool.h>`, and because
this tree compiles as C90 (where `bool` is not yet a keyword) that macro-defines `bool` to `_Bool`,
1 byte. Every engine header included *after* SDL therefore declared its booleans one byte wide while the
engine's own `.c` files defined them four bytes wide. Scalars survived on luck; the array did not:

```c
extern bool keyboard[NUMCODES];
```

`sys_input.c` set `keyboard[SC_W]` at byte offset 0x11 while `D_ints.c` read it at 0x44. Nothing lined up
and no key ever registered.

This also explains the intro appearing to hang: `intro()` ends in `for(;;)` cycling the five character
portraits, and its only exits are `quitgame`/`gameloaded`, both reachable only through `IntroCommand`
reading `keyboard[SC_ESCAPE]`. With input dead the loop is infinite *by construction* — it wasn't stuck,
it was waiting for a keypress it could never see.

Note this had been half-diagnosed earlier: reordering `d_global.h` before SDL fixed the *compile error*
from the same collision, which made the problem look solved while the shadowing was still live for every
header after it. Fixed properly with `platform/sys_sdl.h` — all platform files include SDL through it,
and it restores `bool`/`true`/`false` afterwards, exposes SDL's own type as `sdl_bool` for the one place
that genuinely needs it (`SDL_GetKeyboardState`), and carries a compile-time assertion that the engine's
`bool` is int-sized so this cannot regress silently.

**The bottom-up framebuffer "contract" did not exist.** This plan asserted that `screen` is bottom-up
because the Win32 port used `CreateDIBSection` with a positive `biHeight`, and told the port to preserve
that by flipping rows at present time. That was wrong, and checking the DOS source settles it: the
original `RF_BlitView` (`RA_DRAW.ASM`) is a plain `rep movsd` straight into VGA memory with **no
reversal**, and `ylookup[y] = screen + y*SCREENWIDTH` in both DOS and the port. The `viewylookup[199-i]`
copy in the Win32 C rewrite is GDI compensation, not engine semantics.

Keeping both that reversal *and* a flip in `VI_BlitView` cancelled out for the 3D view — which is why
gameplay looked perfect and hid the bug for the whole port — while everything written straight into
`screen` came out upside down: the menus, the FLI cutscenes, the status bar. Fixed by removing both
flips, so the whole pipeline is top-down like mode 13h. Verified by dumping the live framebuffer to a
PPM: the intro's Softdisk card and an in-game frame both render correctly.

**Every engine spin-loop had to be taught to pump — the real cost of the main-thread tick.** Phase 2
moved the 35 Hz tick onto the main thread to kill the DOS/Win32 timer-thread race. That was right, but
it has a consequence the plan never spelled out: in the original, a loop could sit and watch `timecount`,
`keyboard[]` or `newascii` change *underneath it*, because an interrupt (later a `timeSetEvent` thread)
was mutating them asynchronously. On a main-thread tick, any loop that doesn't call into the platform
layer never advances at all — it hangs, hard.

Pressing Esc froze the game for exactly this reason. `ShowMenu`'s `do { ... } while (!quitmenu)` sets
`quitmenu` from `MenuCommand`, which runs *only* as the timer hook, and the loop pumped only
`if (netmode)`. In single player it spun forever.

Added `Sys_PumpFrame()` (pump + present; VSync keeps it at refresh rate rather than burning a core) and
swept every `.c` file for loops that wait on tick-driven state without pumping. Seven were real:

| Site | Loop |
|---|---|
| `Menu.c` `ShowMenu` | `while (!quitmenu)` — **the Esc freeze** |
| `Menu.c` `ShowQuit` | `while (1)` waiting on `newascii` for y/n |
| `Menu.c` ×2 | `while (y<199)` dialog slide animations |
| `Menu.c` | `while (!newascii)` savegame-name entry |
| `Menu.c` | `while (1)` menu animation on `timecount` |
| `Menu.c` | `while (!CheckPause())` pause overlay |
| `Playfli.c:249` | `while (!CheckTime(timecount,delay)) ;` — an **empty** spin |

That last one matters beyond the freeze: it means the FLI player could never have worked, so the "intro
runs clean for 25s" result logged earlier was this hang, not success. The candidates in `Raven.c` that
the sweep flagged are all catch-up loops whose bodies advance their own deadline, inside a loop that
already pumps — those are fine.

**The FLI cutscenes never loaded.** Forty `sprintf` sites in `Intro.c`, `Event.c` and `Utils.c` build
DOS CD-ROM paths — `sprintf(name,"%c:\\GREED\\MOVIES\\PRISON1.FLI",cdr_drivenum+'A')` — and
`cdr_drivenum` is *never assigned anywhere in the tree*, so it is a zeroed global and every one of them
came out as `A:\...`. `PlayFLI` fails soft, printing `File Not Found` and carrying on, which is why this
did not surface until the intro was actually exercised. Normalised in `sys_files.c` (drop the drive
letter, flip separators, drop the installer's leading `GREED/`) rather than editing four engine files;
the guard means a genuine POSIX absolute path is untouched.

**The intro couldn't be skipped, and music died during it.** Two more consequences of platform
assumptions:

- `lastascii`/`newascii` were fed only from `SDL_EVENT_TEXT_INPUT`, which reports *printable* characters
  only. The DOS keyboard ISR set them from `ASCIINames[]` for every mapped key, so Esc produced 27,
  Enter 13, Backspace 8. `PlayFLI`'s skip test is `while (... && !newascii)` and the menus check
  `case 27:` — none of which could ever fire. Now driven from key-down through the engine's own
  `ASCIINames`/`ShiftNames` tables, with text input still layered on top so a non-US keyboard types the
  character actually on the keycap.
- The libxmp music stream is topped up by polling, from `UpdateSound()`, which is only reached from
  `TimeUpdate()` in the play loop. So the soundtrack starved and cut out for the whole of every FLI
  cutscene and whenever the menu was open. `Sys_PumpFrame()` now calls `UpdateSound()`.

Not a bug: **the menus have no click sounds because the original had none** — `SoundEffect` appears zero
times in both `source_dos/CODE/MENU.C` and this tree's `Menu.c`. FLI files carry no audio track either;
the intro's sound is `INTRO.S3M` playing underneath, which is what the starvation fix restores.

**String literals are read-only on macOS.** `FN_Print` and `FN_PrintCentered` split multi-line text *in
place* — write `\0` over the newline, print, put the character back. Watcom put literals in writable
memory so that worked in 1995; here every caller passes a literal and the store faults
(`EXC_BAD_ACCESS ... byte write Permission fault`). This is what crashed on selecting a character and
difficulty, via `newplayer` → `newmap` → `MissionBriefing`. Both functions now copy each line into a
local buffer. Audited the rest of the class (`strtok`, `strupr`, stores through `char *` parameters) —
no others.

Note why this survived so long: `MissionBriefing` opens with `if (netmode || nointro) return;`, so
**every headless test using `-nointro` skipped the crashing code entirely.** `-char 0..5` all came back
clean and wrongly suggested the path was fine.

**A Win32 debug leftover wiped the briefing screen.** `for (i=0;i<200;i++) memset(ylookup[i],i,320);`
painted row *i* with colour index *i* — a palette ramp, not in the DOS source. Removed; diffing the
whole function against `source_dos/CODE/UTILS.C` showed the only other divergence is the added
`VI_BlitView()` calls, which are needed because DOS wrote straight to VGA.

**ASan is unusable on this machine.** It deadlocks inside its own initialiser on macOS 26 — infinite
recursion through `AsanInitInternal` → `malloc` → `AsanInitFromRtl` — and never reaches `main`. The
verification section's sanitiser step is therefore not available; `build-debug` plus macOS crash
reports is the fallback.

**One press advanced two menu levels.** Choosing a character went straight into the game at whatever
difficulty the cursor happened to sit on. Both input paths were level-triggered, and `ShowMenu`'s one
`do { ... } while (!quitmenu)` loop spent a single press twice — the character boxes and the difficulty
boxes overlap on screen, so the same coordinates hit both.

- *Mouse.* DOS's `MouseGetClick` called INT 33h **AX=06h**, "get button release information", which
  returns the presses since the previous call *and resets the counter as it does so* — edge-triggered
  and self-consuming in one call. That is the entire reason `CheckMouse` carries no debounce of its
  own. The port replaced it with `SDL_GetMouseState`, which reports the button down on every frame it
  is held. Restored the edge behaviour by latching `SDL_EVENT_MOUSE_BUTTON_DOWN` in `sys_input.c` and
  having `MouseGetClick` consume the latch. The option-menu sliders are *dragged*, not clicked, so they
  moved to a new `MouseGetDrag()` holding the old held-state body.
- *Keyboard.* `MenuCommand` polled `keyboard[SC_ENTER]` gated only by `timedelay=timecount+KBDELAY2`,
  i.e. a 5-tick (~143 ms) auto-repeat. This one is verbatim DOS, not a port regression, but it produces
  the same symptom: a 4-second hold measurably ran new game → cyborg → difficulty in one go. Enter and
  Esc now need a release before they fire again; the arrow keys keep repeating on purpose.

Also fixed, unrelated to word size:
- `D_global.h` include guard typo (`GLOBAl_H` vs `GLOBAL_H`) meant it never guarded.
- `MS_Error` printed and *returned*, letting callers run on the null pointer that caused the error. Now exits.
- `VI_GetPalette`/`VI_FillPalette` were empty in the Win32 port, so all fades ran off uninitialised stack.
- `SETUP.CFG` was a raw struct dump; now carries a magic+version+size header so a DOS-era or stale file is rejected rather than loading garbage bindings.
- `pause` renamed to `keypause` (collided with POSIX `pause(3)`); it was dead code.

### Log

- **2026-08-03** — Plan approved. Base chosen: `source_win32/Win32/` (already-portable C; only 6 files
  touch Windows APIs). Build: CMake + `FetchContent`, SDL3 static, libxmp dynamic.
- **2026-08-04** — Builds and runs. Confirmed by user: the game starts and renders. Fixed the
  `wallposts` crash. Added WASD + mouse look.
- **2026-08-04** — `GREED_DATA` now accepts a colon-separated search list. Note `greed_cdrom/` is the
  **un-installed CD master** (`FIRST.EXE` + compressed `GREED.SHR`); it has no `GREED.BLO` or music, and
  supplies only `MOVIES/`. The installed data lives in `greed_final/`.
- **2026-08-04** — Audio confirmed working by the user. Narrowed `fixed_t` and the `sp_*` steppers to
  32 bits, which turned up two more varargs bugs of the same family (`-ticker`, `greedcom_s`).
- **2026-08-04** — **The LGPL premise in this plan was wrong.** libxmp relicensed to **MIT at 4.5.0**;
  at the pinned 4.7.2 there is no §6 relinking obligation, so the reason for linking it dynamically
  evaporated. It is now static like SDL3, and `Greed.app` has no `Contents/Frameworks` and no rpath at
  all — `otool -L` shows only system frameworks. That also retires the `$<TARGET_SONAME_FILE_NAME:>`
  workaround and the SONAME-mismatch failure mode it existed to fix. `cmake/deps.cmake` records the
  version floor so a future downgrade doesn't silently reintroduce the obligation.
- **2026-08-04** — Phase 5 finished: icon derived from `greed_cdrom/LOGO.PCX` (committed as
  `bundle/Greed.icns`, regenerable and byte-reproducible via `bundle/make_icon.py`, which is
  deliberately *not* wired into the build so that building needs only CMake and a compiler); SDL3 and
  libxmp licence texts copied into `Contents/Resources/licenses/`; `README.md` written.
- **2026-08-04** — Verified the bundle runs from an unrelated working directory with `GREED_DATA`
  unset, which exercises the `Contents/Resources` fallback in the path resolver.
- **2026-08-04** — Running *without* `-nointro` for the first time exposed the `cdr_drivenum` DOS-path
  bug (above). Fixed; the intro now runs clean.
- **2026-08-04** — User reports keyboard dead and the intro hanging. Both turned out to be the same
  `bool` size-shadowing bug (above), which had been misdiagnosed earlier as an include-order compile
  error. Fixed at the root with `platform/sys_sdl.h` plus a compile-time guard. **Lesson for the rest
  of this port: a fix that makes an error message go away is not the same as a fix for the cause.**
- **2026-08-04** — With the keyboard alive, Esc froze the game. Root cause was the main-thread tick
  (above): engine loops that waited on tick-driven state never advanced. Added `Sys_PumpFrame()` and
  swept all sources; seven loops needed it, including the FLI player's empty spin. Worth remembering
  that this class is invisible until the input path that reaches the loop actually works — which is why
  it surfaced only now, two bugs deep.
- **2026-08-04** — User screenshot showed the menu rotated. Traced to the bottom-up premise being false
  (above); removed both flips. Verified by dumping the live framebuffer to PPM rather than by argument,
  after being wrong once already this session about a "proven by construction" fix.
- **2026-08-04** — Intro unskippable and silent: `newascii` never saw Esc, and the music poll never ran
  outside the play loop. Both fixed. Menu click sounds turn out never to have existed.
- **2026-08-04** — Crash on starting a new game from the menu: writes to read-only string literals in
  the font code (above), plus the Win32 palette-ramp leftover behind it.
- **2026-08-04** — *Requested change, not a bug fix:* the mission briefing now fades its caption in
  across the same interval as the picture. The original played it as three beats — fade the picture
  over 64 ticks, hold 2s, then ramp the caption through `fontbasecolor` 0..8. `BriefingFadeInPage()`
  spreads the caption ramp over the palette fade; applied to all 7 caption pages and to the
  mission-results screen, whose stats now draw before the fade instead of popping in after it.
  Redrawing text each step is safe because `FN_RawPrint3` writes `fontbasecolor+b` into the pixel
  rather than blending.
- **2026-08-04** — User reports that picking a character immediately picks a difficulty. Fixed both
  input paths (above). Verified by scripting input from inside `Sys_Frame` behind an env var and
  dumping the framebuffer to PPM at each step, since macOS blocks `osascript` from sending keystrokes
  to the window. The same harness run against the *old* keyboard logic reproduced the report exactly —
  one 4s hold, three `Execute` calls — which is what makes the after-state worth believing. Note
  `-nointro` cannot test any of this: it skips the menu entirely and calls `newplayer(0,0,2)` direct,
  so the test opens the in-game Esc menu instead, which is the same `ShowMenu`.
- **2026-08-04** — Added a top-level `.gitignore` (`.DS_Store`, `build/`, `build-*/`, `*.dSYM/`, editor
  dirs), so `git status` shows only real sources. Note what this exposes: **none of `source_macos/` is
  committed yet** — the whole port is still untracked working-tree state on top of the 2022 import.

### Current state

Verified today, by running the tree rather than by reading it:

- A touch-everything rebuild of all 34 engine sources plus the platform layer compiles with **three
  warnings and no errors** (listed under *Known-benign warnings* below).
- `Greed.app/Contents/MacOS/Greed` launches, loads its lumps and stays up; `Contents/Resources/` carries
  `GREED.BLO`, `SETUP.CFG`, all 18 music modules, `MOVIES/`, the icon and both licence texts.
- `~/Library/Application Support/Greed/` **does not exist**, which is direct evidence that no save,
  no `SaveSetup` and no rebind has ever been written — i.e. the whole writable-path branch of
  `sys_files.c` is still unexercised at runtime.

### Open / next

Roughly in the order the work is worth doing.

**1. Play it properly, once, end to end.** Every remaining doubt in this file is a thing that only
playing settles. The three bug classes this port actually hit — `bool` shadowing, unpumped spin loops,
read-only literals — were all invisible until the code path in front of them worked, and each surfaced
only when a human drove the game. Specifically still unwatched:

- The intro FLIs at the right *speed*, and skippable. Two separate bugs blocked them (the DOS
  drive-letter path, then `Playfli.c`'s empty spin); both are fixed but nobody has sat through one.
- Save/load round-trip to `~/Library/Application Support/Greed/` — see above, provably never run. This
  is the largest untested surface left: `SAVEGAME.DIR`/`SAVEGAME.%i` writing, the prefs-dir-first read
  order, and a rebind persisting through `SETUP.CFG`'s new magic+version header.
- Cheat codes (`Special_Code`), the ESC quit prompt, and the `bt_*` bindings one by one.
- `-ticker`, to confirm the 35 Hz tick really is independent of a 120 Hz ProMotion display.

**2. Tune mouse-look feel.** The divisors in `ControlMovement` were picked by reading the DOS code, not
by playing. Sensitivity scaling and invert-Y both want a pass with hands on.

**3. Commit the port.** `git ls-files source_macos` is empty: `src/`, `platform/`, `cmake/`, `bundle/`,
`CMakeLists.txt` and both READMEs are untracked, as are `CLAUDE.md` and this plan. Until that lands
there is no bisectable history for any of the fixes catalogued above, and no way to diff a regression
against a known-good tree. Worth doing before the next behavioural change, not after.

**Known-benign warnings** — three, all looked at, none chased down:

| Site | Warning |
|---|---|
| `Playfli.c:105` | `char *` passed as `byte *` (`-Wpointer-sign`) |
| `Utils.c:1166` | 4 unhandled enum values in a `switch` (`-Wswitch`) |
| link step | `reducing alignment of section __DATA,__common from 0x8000 to 0x4000` — clang over-aligns a large common symbol past the segment maximum; the linker's fallback is correct for every use here |

**Not attempted:**

- A universal (arm64 + x86_64) build. It should be a one-line `CMAKE_OSX_ARCHITECTURES` change
  precisely because both deps build from source, but it is untried, and a cold two-slice configure
  rebuilds SDL3 and libxmp twice.
- Distribution to another machine: the bundle is ad-hoc signed, so it is only known to run on the
  machine that built it. Notarisation is out of scope, but the right-click → Open path is untested by
  anyone else.

**Housekeeping:** `source_macos/build-asan/` is dead weight — ASan deadlocks in its own initialiser on
this macOS (see above) and the tree can never be run. Delete it and keep `build` + `build-debug`.

---

## Context

This repo is an archive of the 1995 Softdisk/Channel 7 FPS *In Pursuit of Greed* (Raven 3D engine): the
original DOS/Watcom source (`source_dos/`), an unfinished Win32 C port (`source_win32/`), shipped game
data (`greed_final/`, `greed_cdrom/`), and the earlier `raven_engine/` prototype. Neither buildable tree
runs on a modern machine — the DOS build needs Watcom + DOS/4GW + real-mode interrupts, and the Win32
build needs MSVC, 32-bit x86 inline asm, and GDI palette APIs that no longer function.

Goal: a native, double-clickable Apple Silicon macOS app that plays the game, built from the existing C
source rather than emulated.

**`source_win32/Win32/` is the right base.** It has already done the hard part — John Bianca's
hand-written x86 renderer (`RA_DRAW.ASM`, `BLITBUF.ASM`, `MOUSE.ASM`) was rewritten in portable C
(`R_render.c`), and DOS real-mode I/O was removed. Its OS-specific surface is only six files:

| File | Dependency to replace |
|---|---|
| `Intro.c` | `WinMain`, `WndProc`, window creation |
| `D_video.c` | GDI DIB section + `HPALETTE` for the 320×200 8-bit framebuffer |
| `D_ints.c` | `GetAsyncKeyState` / `MapVirtualKey` polling |
| `Timer.c` | `timeSetEvent` multimedia timer (35 Hz) |
| `R_public.c` | `_asm` `FIXEDMUL` / `FIXEDDIV` |
| `D_misc.c` | `PostQuitMessage` in `MS_Error` / `MS_ExitClean` |

Everything else — renderer, sprites, AI, menus, FLI player, netcode — is portable C.

**Decisions taken** (from user): sound effects **and** music; **modernised** controls (mouse look + WASD
layered over the original bindings); ship a **`Greed.app`** bundle; **CMake + `FetchContent`** with
pinned, statically-built dependencies; target **SDL3**.

## Approach

Create a new top-level `source_macos/` rather than editing `source_win32/` in place — the Win32 tree is a
historical artefact worth preserving, and a clean copy makes the port diff reviewable.

```
source_macos/
  CMakeLists.txt        # FetchContent for SDL3 + libxmp; targets: greed, Greed.app
  cmake/deps.cmake      # pinned dependency tags
  README.md             # build + run, controls, licence notes
  src/                  # copied from source_win32/Win32/ then modified
  platform/             # new, macOS-only
    sys_main.c          # main(), event pump, tick driver, Sys_Frame()
    sys_video.c         # SDL_Window/Renderer/Texture, palette expansion, present
    sys_input.c         # SDL scancode -> DOS set-1 table, mouse look
    sys_sound.c         # one SDL_AudioStream per voice + libxmp music
    sys_files.c         # asset search path + writable prefs path
    sys_compat.h        # DOS/MSVC libc shims; force-included
  bundle/Info.plist.in  # configured by CMake
```

**Do not vendor the game data.** The bundle target copies `GREED.BLO`, the 18 `*.MOD`/`*.S3M` tracks and
`SETUP.CFG` from `greed_final/`, and `MOVIES/` from `greed_cdrom/` if present, into
`Greed.app/Contents/Resources/`.

### Dependencies

`cmake/deps.cmake` declares both dependencies at pinned git tags and builds them from source:

- **SDL3** — `SDL_STATIC=ON`, `SDL_SHARED=OFF`, link `SDL3::SDL3-static`. Its CMake config carries the
  required macOS framework list (Cocoa, CoreAudio, CoreVideo, Metal, …) as interface dependencies, so
  static linking Just Works. SDL3 is zlib-licensed — static linking carries no obligations.
- **libxmp** — pinned tag, built through its bundled `CMakeLists.txt`. I'm confident SDL3's static target
  name; I'm *less* certain of libxmp's exact target (`libxmp_static` vs `libxmp`), so verify against the
  pinned tree at implementation time, with Homebrew's libxmp as the fallback.

~~**Licensing caveat, worth deciding up front:** libxmp is **LGPL-2.1-or-later**. Static linking triggers
the relinking obligation (LGPL §6)… So the plan is: **SDL3 static, libxmp dynamic-into-the-bundle.**~~

**Superseded — the premise was wrong.** libxmp relicensed to **MIT at 4.5.0**; the pinned 4.7.2 carries
an MIT notice in `docs/COPYING`. There is no §6 obligation, so libxmp is linked **statically** like SDL3
and `Greed.app` needs no `Contents/Frameworks`, no `install_name_tool` and no rpath. Both licences do
require their notices to travel with the binary, so the bundle copies them into
`Contents/Resources/licenses/`. `cmake/deps.cmake` records the 4.5.0 floor so that pinning libxmp
backwards doesn't silently reintroduce the obligation.

Prerequisite: `brew install cmake` (not currently installed). `pkg-config`, `git` and clang are already
present. First configure will compile SDL3 from source (a few minutes), cached thereafter.

**Conan was evaluated and rejected.** It has the better upgrade story in principle — real version
resolution and a `conan.lock` — but the dependencies aren't there: SDL3 is on ConanCenter (`sdl/3.4.8`),
while **libxmp is not**, and nor is libopenmpt. The only tracker library available is
`libmodplug/0.8.9.0`, which is old and weaker on S3M playback — and 12 of this game's 18 tracks are S3M.
Conan would therefore require authoring and maintaining a local recipe for libxmp (~80 lines, per the
`libmodplug` recipe's shape), which is precisely the work FetchContent avoids by consuming libxmp's own
`CMakeLists.txt`. Revisit if libxmp ever lands on ConanCenter.

**End users need nothing installed.** Both dependencies are resolved at build time on the build machine
only; SDL3 is linked statically and libxmp ships inside the bundle (see Phase 5), so `Greed.app` runs on
a clean macOS system with no Homebrew, no CMake and no libxmp.

---

## Phase 1 — Compile the existing C on clang/arm64

Copy `source_win32/Win32/*.c` and `*.h` into `source_macos/src/`, then:

**Build flags.** `C_STANDARD 90` with GNU extensions (the source uses implicit `int` and K&R-isms that
C99+ rejects, e.g. `static weaponlump=0` at `Utils.c:508`), `-fno-strict-aliasing`, `-Wno-parentheses`,
and a force-included `platform/sys_compat.h`. A `Debug` config adds `-O0 -g -fsanitize=address`.

**`sys_compat.h`** supplies what MSVC/DOS provided:
- `stricmp`→`strcasecmp` (74 call sites), `_open`→`open`, `O_BINARY`→`0`, `filelength()` via `fstat`,
  `_S_IREAD`/`_S_IWRITE`, `kbhit()`/`getch()` stubs.
- Redirect file access through the path resolver: `#define fopen Sys_fopen`, `#define open Sys_open`.
  This keeps the port diff tiny versus editing ~20 call sites.
- Drop `#include <Windows.h>` from `D_global.h:22`; the DOS-era headers (`DOS.H`, `CONIO.H`, `IO.H`,
  `MALLOC.H`, `PROCESS.H`) are matched by empty shim headers on an include path so the ~40 `#include`
  lines can stay untouched.

**64-bit correctness** — the real portability work, and where silent corruption would come from:
- `D_global.h:29` — `typedef unsigned long longint` must become `unsigned int`. `longint` appears in
  `#pragma pack(1)` structs read straight off disk (`Playfli.c` `fliheader`/`frameheader`) and in
  `player_t` (`Protos.h:538`); an 8-byte `long` silently breaks both, plus `timecount` wraparound.
- `D_disk.c:98` — `size=fileinfo.numlumps*sizeof(int)` allocates a `void**` array (`lumpmain`) at half
  the needed size on LP64. Must be `sizeof(void*)`. This is a guaranteed heap overflow on first run.
- Audit `Utils.c`, `Spawn.c`, `Sprites.c` for `int`/pointer round-trips; promote
  `-Wint-conversion -Wpointer-to-int-cast -Wincompatible-pointer-types` to errors and fix what surfaces.

**`R_public.c:49-76`** — replace the two `_asm` bodies with portable 64-bit fixed-point:
```c
fixed_t FIXEDMUL(fixed_t a, fixed_t b) { return (fixed_t)(((int64_t)a * b) >> FRACBITS); }
fixed_t FIXEDDIV(fixed_t a, fixed_t b) { return (fixed_t)(((int64_t)a << FRACBITS) / b); }
```
These match `shrd`/`idiv` semantics exactly for the ranges the engine uses.

**`D_misc.c:109-131`** — `MS_Error` currently prints and *returns*, letting callers continue on garbage.
Make it print to stderr, show an `SDL_ShowSimpleMessageBox`, and `exit(1)`. `MS_ExitClean` → `exit(0)`.

**Exit criterion:** links to a binary that reaches `CA_InitFile("GREED.BLO")` and loads lumps without
crashing (verify by temporarily stubbing `VI_Init`).

## Phase 2 — SDL3 video, input, timing

Use `SDL_MAIN_HANDLED` + `SDL_SetMainReady()` with a plain `main()`, so the existing `startup()` flow in
`Intro.c` survives intact rather than being restructured into SDL3's `SDL_AppInit` callback model.
Include `<SDL3/SDL.h>`.

**`sys_video.c`** replaces the GDI half of `D_video.c`, keeping every function signature identical so no
caller changes:
- `VI_Init` — `malloc(64000)` for `screen`; create window (default 1280×960, resizable) +
  `SDL_CreateRenderer(window, NULL)` + a streaming `SDL_PIXELFORMAT_ARGB8888` 320×200 texture with
  `SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST)`. Keep the existing `ylookup` / `transparency` /
  `translookup` setup verbatim.
- **The framebuffer is bottom-up.** `CreateDIBSection` with positive `biHeight` gave a bottom-up
  surface, which is why `RF_BlitView` (`D_video.c:414`) copies `viewylookup[199-i]` into `ylookup[i]`.
  Preserve that contract: leave `RF_BlitView` and all `VI_DrawPic`/`ylookup` writers alone, and have
  `VI_BlitView` walk `screen` rows in reverse when expanding through the palette into the texture.
  Getting this backwards flips the whole game vertically.
- `VI_BlitView` — expand 64000 indexed bytes to ARGB via a 256-entry `Uint32` LUT, `SDL_UpdateTexture`,
  then `SDL_RenderTexture` (SDL2's `SDL_RenderCopy`, renamed) into a 4:3 letterboxed `SDL_FRect`.
  320×200 was displayed as 4:3 on a CRT, so square pixels would look wrong — compute the dest rect
  manually rather than using `SDL_SetRenderLogicalPresentation`, whose letterbox mode would preserve the
  texture's own 1.6:1 aspect.
- `VI_SetPalette` — rebuild the LUT from the 6-bit VGA palette (`<<2`) and re-present. `VI_ResetPalette`
  becomes a no-op. Implement `VI_GetPalette` and `VI_FillPalette`, which the Win32 port left **empty** —
  that is why `VI_FadeOut`/`VI_FadeIn` (`D_video.c:103-183`) currently fade from uninitialised stack.
  Keep a `byte curpal[768]` and back both with it; this fixes the menu and intro fades for free.

**`sys_input.c`** replaces `INT_ReadControls` (`D_ints.c:199`):
- A static `SDL_SCANCODE_* → DOS set-1 scancode` table covering the ~100 codes in `D_ints.h:24-100`,
  driving the existing `keyboard[NUMCODES]` array. Note SDL3's `SDL_GetKeyboardState` returns
  `const bool *`, not `Uint8 *`.
- Set `lastascii`/`newascii` from `SDL_EVENT_TEXT_INPUT` (SDL3 requires an explicit
  `SDL_StartTextInput(window)`) so the menus and the cheat-code parser (`Special_Code`) keep working.
- Keep the `scanbuttons[]` → `in_button[]` mapping untouched, so `SETUP.CFG` rebinding still applies.
- Implement `M_Init`/`UpdateMouse`/`MouseGetClick`/`MouseHide`/`MouseShow`, which are all empty stubs
  today — this restores mouse support in the menus.

**Timing — drive ticks from the main thread.** `Timer.c`'s `timeSetEvent` fired `INT_TimerISR` on a
separate thread, which then called `PlayerCommand` → `ControlMovement` and mutated game state
concurrently. Don't reproduce that race. Instead:
- `Sys_Frame()` pumps `SDL_PollEvent`, refreshes input state, and computes elapsed 35 Hz ticks from
  `SDL_GetTicks()` (`Uint64` in SDL3), calling `INT_TimerISR()` (which does `timecount+=2` and invokes
  `timerhook`) once per elapsed tick, capped at ~8 per call to survive a stall.
- Call `Sys_Frame()` exactly where the Windows message pump lived: `Wait()` (`Intro.c:216`) and
  `TimeUpdate()` (`Raven.c:2623`). `dStartTimer`/`dStopTimer` become no-ops.
- `SDL_EVENT_QUIT` and ⌘Q set `quitgame = true`, matching the existing `WM_CLOSE` handler.

**`sys_files.c`** — read-only assets resolve against, in order: `$GREED_DATA`, the current directory,
then `SDL_GetBasePath()/../Resources`. Writable files (`SETUP.CFG`, `SAVEGAME.DIR`, `SAVEGAME.%i`,
`demo1`) resolve to `SDL_GetPrefPath("redshadow","Greed")` → `~/Library/Application Support/Greed/`,
seeded from Resources on first run. A code-signed `.app` has a read-only Resources dir, so without this,
saving fails. (Check the SDL3 headers for ownership: `SDL_GetPrefPath`'s result must be `SDL_free`d;
`SDL_GetBasePath`'s must not.)

Also fix `DemoIntro` (`Intro.c:460`): `"MOVIES\\TEXT.FLI"` → `"MOVIES/TEXT.FLI"`. `startup()`
(`Intro.c:765`) hardcodes `nointro = true`; drive it from `MS_CheckParm("nointro")` so the FLI intro
plays when `MOVIES/` is present.

**Exit criterion:** the game boots to a playable level in a window, keyboard-controlled.

## Phase 3 — Audio

The Win32 port stubbed out *all* of `Modplay.c` (`PlaySong`, `SoundEffect`, `StaticSoundEffect`,
`UpdateSound` are empty). Reimplement against the original DOS logic in
`source_dos/CODE/MODPLAY.C`, which is intact and readable.

**SDL3 removes most of the work here.** SDL2 would have needed a hand-written mixer with manual
resampling; SDL3's `SDL_AudioStream` does mixing and rate conversion itself:

- **Sound effects.** Lumps `1..225` of `GREED.BLO` (after the `soundeffects` marker at lump 0, per
  `MODPLAY.C:207`) are plain **RIFF/WAVE, 8-bit unsigned mono, 11025 Hz** — verified by dumping the
  file. Decode with `SDL_LoadWAV_IO` over an `SDL_IOFromConstMem` of the cached lump.
- **Voices.** Allocate one `SDL_AudioStream` per effect track (`effecttracks`, default 4, configurable
  via `SETUP.CFG`) with an input spec of `SDL_AUDIO_U8` / 11025 Hz, each `SDL_BindAudioStream`ed to
  `SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK`. SDL3 mixes all bound streams and resamples to the device rate.
  Then:
  - `SDL_SetAudioStreamGain` — volume, from `MODPLAY.C`'s `DVolume[]` squared-distance table.
  - `SDL_SetAudioStreamFrequencyRatio` — the ±`variation` pitch randomisation and `midgetmode`'s 2×,
    replacing `dSetVoiceFreq`.
  - Panning has no direct equivalent to `dSetVoiceBalance`. Simplest faithful approach: give the stream
    a **stereo** input spec and duplicate the mono sample into L/R with the per-channel gains derived
    from `d1` (0..0x80). Port the view-vector pan math from `MODPLAY.C:309-346` as-is.
- **Music.** `libxmp` plays both `.MOD` and `.S3M`. `PlaySong` calls `xmp_load_module`; a dedicated
  `SDL_AudioStream` is fed from `xmp_play_buffer` and gained to `SC.musicvol`. Keep `StopMusic`'s
  fade-out loop. Guard with `#ifdef HAVE_LIBXMP` so the build degrades to silent music if libxmp is
  unavailable.
- Replace `LoadSetup`/`SaveSetup`'s raw `SoundCard` struct dump (`Modplay.c:40-65`) — that struct is
  layout-sensitive and now differs from the DOS/Win32 one. Add `#pragma pack(1)` plus a magic+version
  header, and reject a `SETUP.CFG` whose size or magic doesn't match, falling back to the built-in
  defaults already coded at `Modplay.c:106-154`.

## Phase 4 — Modernised controls

Layer on top of the original bindings; never remove them.

- **Mouse look.** `SDL_SetWindowRelativeMouseMode(window, true)` during play (SDL3 makes this
  window-scoped). In `sys_input.c`, accumulate raw deltas from `SDL_EVENT_MOUSE_MOTION`; in
  `ControlMovement` (`Raven.c:1544`), apply horizontal delta directly to `player.angle` (masked with
  `ANGLES`, as the existing turn code at `Raven.c:1903-1934` does) rather than through the DOS path,
  which quantised mouse motion into discrete `in_button[bt_east]=2` steps and feels terrible.
  Vertical delta feeds `scrollview`, the same variable `bt_lookup`/`bt_lookdown` use, clamped by the
  existing `ChangeScroll` (`Raven.c:2120`). Scale both by `SC.mousesensitivity`; add an invert-Y flag.
- **WASD.** Remap `scanbuttons[]` defaults: W/S → `bt_north`/`bt_south`, A/D → `bt_slideleft`/
  `bt_slideright`, left mouse → `bt_fire`, right mouse → `bt_use`, Shift → `bt_run`, Space → `bt_jump`,
  E → `bt_useitem`. Because this goes through `scanbuttons[]`, the in-game key-config menu still works
  and the choices persist to `SETUP.CFG`.
- ESC releases mouse capture as well as opening the menu; ⌘F / F11 toggles fullscreen.
- Note the conflict: A is `bt_asscam` by default and Z is `bt_jump`. Both stay reachable from the
  config menu; document the new defaults in the README.

## Phase 5 — `Greed.app` bundle

Drive this from CMake (`MACOSX_BUNDLE` plus a configured `Info.plist.in`):
```
Greed.app/Contents/
  Info.plist            # CFBundleExecutable=greed, LSMinimumSystemVersion=11.0,
                        # NSHighResolutionCapable=true, CFBundleIconFile
  MacOS/Greed           # SDL3 and libxmp both linked statically
  Resources/            # Greed.icns, GREED.BLO, SETUP.CFG, *.MOD, *.S3M, MOVIES/,
                        # licenses/{SDL3-LICENSE,libxmp-COPYING}.txt
```
- No `Frameworks/`, no `install_name_tool`, no rpath — see the licensing note above. `otool -L` on the
  built binary lists only system frameworks, `libSystem` and `libobjc`.
- `codesign --force --sign -` (ad-hoc) so Gatekeeper lets a locally built app run; README should explain
  the right-click → Open path for an unsigned build.
- Build `-arch arm64` by default; a universal build is straightforward here *because* the deps are built
  from source — set `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` and FetchContent builds both slices.
- Icon from `greed_cdrom/LOGO.PCX`. `sips` can't read PCX, so `bundle/make_icon.py` uses Pillow +
  `iconutil`: aspect-correct 320×200 → 320×240, crop the wordmark, pad to square on the logo's own
  black starfield, and upscale with **NEAREST** (a smooth filter turns 1995 pixel art to mush). The
  resulting `bundle/Greed.icns` is committed and the script is not wired into CMake, so building the
  port still needs nothing but CMake and a compiler.
- Include SDL3's and libxmp's licence texts in `Resources/licenses/`, copied from the exact fetched
  trees that were linked against.

---

## Verification

1. **Configure and build clean.** `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`
   from a cold cache, with zero errors and the LP64 warnings promoted to errors.
2. **Sanitiser run.** A Debug build with ASan through the first level. This is the fastest way to catch
   the remaining 32→64-bit pointer/struct bugs; the `lumpmain` under-allocation would fire immediately.
3. **Boot to gameplay.** `GREED_DATA=../greed_final ./build/Greed.app/Contents/MacOS/Greed -nointro`
   → status bar, walls, sprites
   and textured floors/ceilings render. Compare against a DOSBox run of `greed_final/GREED.EXE` for
   palette and geometry correctness — the vertical-flip contract and the 6-bit→8-bit palette scaling are
   the two things most likely to be visibly wrong.
4. **Fades and menus.** ESC opens the menu; verify the fade in/out is smooth (this exercises the
   `VI_GetPalette`/`VI_FillPalette` implementations the Win32 port left empty), and that save/load
   writes to `~/Library/Application Support/Greed/`.
5. **Audio.** Fire each weapon and confirm SFX pan left/right as you turn and pitch-vary between shots;
   confirm music starts on level load and cross-fades on level change. `SONG0.S3M`..`SONG14.MOD` are all
   exercised by `Utils.c:1258-1406`.
6. **Controls.** Mouse look is smooth (not stepped), WASD moves, both mouse buttons act, ESC releases
   capture, fullscreen toggles without corrupting the framebuffer.
7. **Timing.** Confirm the game runs at the intended 35 Hz tick regardless of render frame rate — no
   speed-up on a 120 Hz ProMotion display. Verify with `-ticker`, which the engine already supports
   (`Intro.c:745`).
8. **Bundle.** `open build/Greed.app` from a *different* directory with `GREED_DATA` unset, proving the
   `Contents/Resources` fallback in the path resolver works. ~~and after `brew uninstall libxmp`,
   proving the bundled dylib is loaded~~ — moot now libxmp is static; the equivalent check is that
   `otool -L` lists only system frameworks, `libSystem` and `libobjc`. **Done** (2026-08-04).
9. **Intro (optional).** Copy `greed_cdrom/MOVIES` into Resources and launch without `-nointro`; the FLI
   sequence should play and be skippable.

## Out of scope

Networked multiplayer (`Net.c` is IPX/serial-based and would need a full rewrite — it should be compiled
but left disabled), the DOS-only tools (`MEDIT`, `SGRAB`, `SETUP`, `IDLINK`), and the `raven_engine/`
prototype.

**A Rust rewrite was considered and deferred.** The engine's shape — 168 `extern` globals, arrays of
interior pointers into the framebuffer (`ylookup`, `viewylookup`, `zcolormap`, `translookup`), an
intrusive doubly-linked sprite list mutated during iteration, and renderer hot loops driven by global raw
pointers (`R_spans.c:MapRow`) — makes it a 3–5× rewrite rather than a port, with no test suite to
validate against. Doing the C port first yields the working reference build that a later Rust rewrite
would need as its oracle.
