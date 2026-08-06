# macOS (and Windows) port of *In Pursuit of Greed*

> **The tree moved, 2026-08-05.** `src/` and `platform/` are no longer under `source_macos/` — they
> are in **`source_shared/`**, along with `cmake/deps.cmake` and a new `cmake/engine.cmake`, and are
> compiled unchanged by both ports. `source_macos/` keeps only the `.app` bundle; `source_win64/` is
> the new Windows x64 build. Every file path in the sections below that reads `source_macos/src/...`
> or `platform/...` now lives under `source_shared/`. See **Windows port** at the end.

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
| 6a — Retina presentation | **done** | `HIGH_PIXEL_DENSITY` + `PIXELART`; measured 3024×1898 backing store |
| 6b — `SETUP.CFG` v2 | **done** | Appended fields; old configs migrate instead of resetting |
| 6c — Display options | **done** | Screen size row replaced by RENDERER / FULLSCREEN |
| 6d — Span tag widened | **done** | 64-bit tag, `MAXSPANS` 4096→262144, overflow guards now compiled in |
| 6e — HD renderer | **done** | Native aspect + capped native resolution; HD is the default |

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
  cutscene and whenever the menu was open. First fixed by calling `UpdateSound()` from
  `Sys_PumpFrame()`; that turned out to be too narrow — see the 2026-08-05 briefing entry — and the
  call now lives in `Sys_Frame()`, which every pump goes through.

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

**The same held key ran the whole mission briefing, and the whole end-game sequence.** Every briefing
page, end-game text page and credits screen ended in `for(;;){Wait(10); if (newascii) break;}`. Nothing
there consumes the key, so the auto-repeat kept re-latching `newascii` and one held key ran the pages
together. Measured: one press consumed all six pages of the map-0 briefing, and a 13-second hold ran
four of `EndGame1`'s five screens back to back. The briefings had it worse, because they clear
`newascii` *before* the page fades in rather than after, so the repeat had the whole ~1.8s fade — plus
the tail of the press that turned the previous page — to latch in as well.

All 26 loops (11 in `MissionBriefing`, 5 each in `EndGame1/2/3`) now call `BriefingWaitKey()`, which
waits for every ASCII-producing key to be seen released before it accepts a press. Modifiers are
skipped (no `ASCIINames` entry, so they cannot set `newascii`) or the screen would hang for anyone
resting a finger on shift. Esc is exempt from the debounce: `MissionBriefing` follows the wait with
`if (lastascii==27) goto end`, so debouncing it would cost a second press to leave, and it cannot run
pages together because it exits the function outright. The end-game callers keep their `newascii=false`
immediately before the call, which is what stops a stale Esc reaching the exemption there.

**Cmd-Q only worked while you were actually playing.** `quitgame` was only ever tested by `PlayLoop`'s
`while (!quitgame)` and by `intro()` after each portrait, because in DOS nothing could set it
asynchronously — it came from the menu's own Quit item, reached from a loop that was about to exit
anyway. The port gave it two asynchronous sources (Cmd-Q and the window's close box, both in
`handle_event`), and every other blocking screen ignored it: intro movies, briefings, end-game and
credits, the menu, the help pages, the save-name field, the pause screen. Measured: SIGTERM — which SDL
turns into `SDL_EVENT_QUIT`, the same flag Cmd-Q sets — left the process running through a mission
briefing until it was SIGKILLed 25s later.

Two mechanisms, because the loops divide into two kinds:

- *Waiting on time.* `Wait()` now abandons the delay when `quitgame` is set. Nearly every pause in
  those screens is a `Wait`, so this alone collapses fades, caption ramps, FLI frame pacing and the
  two-second holds. Callers see the delay as having elapsed, which is safe — the fades set their final
  palette after their loop, so none is left half applied.
- *Waiting on input.* Those never terminate on their own, and with `Wait` short-circuited they would
  spin instead of hang, which is no better. Each one tests `quitgame` in its own condition:
  `BriefingWaitKey`, `ShowMenu`, `ShowHelp`, `ShowQuit`, `ShowSaveDir`'s name field, `ShowPause`,
  `MenuAnimate`, and `playfli`'s frame loop.

`CheckDemoExit()` reports `quitgame` too, which covers every step of `MainIntro` at once — it already
asks after each screen and each movie. `MissionBriefing` folds it into the `if (lastascii==27) goto
end` it already had at each page. And `startup()`'s `-nointro` path now guards its `maingame()` call:
`newplayer` runs the briefing, `InitData` clears `quitgame`, so a quit taken during that briefing was
otherwise swallowed and the game started anyway.

Not touched: `NetWaitStart`'s multiplayer sync loop, which has its own 10-minute timeout and reports
cancellation through `MS_Error`. Quitting mid-sync would want a different answer than an error box, and
netmode is not testable here.

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
- **2026-08-05** — Carried the menu's keypress debounce into `MissionBriefing` (above), and made a
  press *during* a fade snap it to the end instead of being swallowed: `if (newascii) i=steps;` in
  `BriefingFadeInPage`, and the same jump in the three `fontbasecolor` caption ramps and the
  mission-results stats fade. Jumping the counter rather than breaking means the final step still runs,
  so the snapped page is left in exactly the state a completed fade leaves it in — checked by dumping
  both to PPM, they are byte-identical. The press is not spent on the snap; `BriefingWaitKey` still
  wants one of its own to turn the page, matching the screen's own "PRESS SPACE BAR TO CONTINUE".
  Verified with the same `Sys_Frame` input-scripting harness as the menu fix, driving `MissionBriefing(0)`
  straight from `startup()` (the menu route is slow and `-nointro` returns early). Measured, with the
  old loop restored behind an env var for the before/after: one 4s hold spent all six pages before,
  exactly one after; a tap 600ms into a fade ended it at 653ms against an uninterrupted 1792ms; a
  caption ramp ran 2 of 9 steps when held through, 9 of 9 when not.
- **2026-08-05** — Same debounce carried into `EndGame1/2/3` (15 more loops), which had the identical
  pattern. Verified the same way, driving `EndGame1()` from `startup()`: a 13s hold ran screens 1–4
  back to back before the fix and **zero** after, with discrete presses then advancing exactly one
  screen each. Two notes from writing that harness. `EndGame1` never sets `font` — it inherits it from
  the play loop, so calling it cold segfaults in `FN_PrintCentered`; that is fine in the real flow but
  worth knowing. And unlike the briefings, these screens clear `newascii` *after* `VI_FadeIn` rather
  than before, so a press during their fade was always discarded — left as is, since changing it would
  mean making `VI_FadeIn` interruptible and that is shared with the intro and menus.
- **2026-08-05** — User reports Cmd-Q does nothing. `quitgame` now honoured everywhere (above).
  Verified with SIGTERM, which SDL delivers as `SDL_EVENT_QUIT` and so exercises the identical flag
  without needing synthetic keystrokes — macOS blocks those, so this is the only way to test the quit
  path from a script. Signalled at four points in the normal run (intro movies at 5s, intro screens at
  20s, portrait/menu cycle at 45s, in-game via `-nointro` at 12s) and, through a temporary hook that
  called them directly, in the briefing, the menu, the help page and `EndGame1` during both its movies
  and its credits. Every case exits **0.2–0.7s** after the signal with the screen's function returning
  normally, against the briefing before the fix surviving to a SIGKILL 25s later.
- **2026-08-05** — User reports the music stopping in `MissionBriefing`. The FLI/menu starvation fix
  above was put in `Sys_PumpFrame()`, but the briefings never call it: every pause in them is a
  `Wait()`, and `Wait()` spins on `Sys_Frame()` alone. The stream holds ~250ms, so the soundtrack died
  a few hundred ms into the first page and stayed dead until `TimeUpdate()` picked it up in the play
  loop — pressing space to turn a page is simply where the silence gets noticed. Moved the
  `UpdateSound()` call down into `Sys_Frame()`, after the tick loop so it runs at 35 Hz rather than at
  the speed of an unthrottled spin, and dropped it from `Sys_PumpFrame()`. The intro text screens (they
  call `UpdateSound()` by hand, which is the tell), the credits and `StopMusic`'s own fade-out all had
  the same hole. Measured rather than argued: a temporary probe logging `SDL_GetAudioStreamQueued` every
  500ms during the map 0 briefing, driven by a temporary `-brieftest` parm calling `newplayer(0,0,2)`
  from `startup()`. Before, with the fix disabled by env var: 57344 bytes at 348ms and **0 for every
  sample after**, out to 10s. After: 36864–73728 bytes, never dry, across the same run.
- **2026-08-06** — User asks whether the music is supposed to loop. It is, and it did not: the track
  went silent for good when it ended, until the next `PlaySong`. `Sys_MusicUpdate` passed `loop=1` to
  `xmp_play_buffer` with a comment reading "the soundtrack repeats until the level changes", but
  libxmp's `loop` is a **maximum loop count**, not a flag (`player.c`: `if (ret < 0 || (loop > 0 &&
  fi.loop_count >= loop))`). One pass and it returned `-1`, clearing `music_running`. `loop=0`
  disables the cutoff. One character, but it took a long detour to be sure it was the *only* change
  needed, and the detour is the part worth keeping:
  - The modules are compilations — `SONG0.S3M` is five tracks in one file and `selectsong()` starts
    maps 0–4 at orders 0/20/37/54/73 — and the order lists carry `0xFF` end markers (at 19/36/53/72/89
    in `SONG0`). libxmp reads `0xFF` as end-of-song and splits the list into separate *sequences*;
    DSIK did not. `AUDIO.LIB`'s `player.asm` was extracted from the OMF library and disassembled to
    settle it: `29e: movb 0x40(%edi,%eax),%al / 2a2: cmpw 0x28(%edi),%ax / 2a6: jae 0x275` — any order
    `>= NumPatterns` is *skipped*, and playback stops only on running off the end with
    `ReStart >= OrderLen`, which the S3M importer guarantees by forcing `ReStart = MAXORDERS-1`
    (`IMPORT.C:941`). The game then restarted it from order 0 (`MODPLAY.C:397-400`), since `pattern`
    is a one-shot poke applied *after* `dPlayMusic` already started at 0.
  - **None of which matters**, because the markers are never reached. Every section ends with a `Bxx`
    position jump back to its own first order — `B00`→0, `B14`→20, `B25`→37, `B36`→54, `B49`→73, each
    on row 63 of that section's last pattern. The music data loops itself, DSIK followed those jumps
    (`PB_JUMP`, which `PlaySong` uses for the initial seek), and libxmp follows them too. An
    intermediate fix that rewrote the order list to emulate DSIK's skip was written, measured to be a
    behavioural no-op, and removed — it also crashed, because `m->scan_cnt[]` is allocated per order
    index and sized to that index's pattern, so resliding the list corrupts it.
  - Verified with a headless harness linking the pinned libxmp, printing `xmp_frame_info.pos`
    transitions for each `(module, pattern)` pair `selectsong` can produce. With `loop=1`:
    `73…84 |WRAP-> 73 <play_buffer returned end-of-replay>`. With `loop=0`: the same twelve orders
    round and round. Every section of every module self-loops; `ENDING.MOD` honours its restart byte
    of 41.
- **2026-08-05** — User reports the HUD does not work. It never had: the status bar, ammo, shield,
  inventory and goal readouts are all gated on `currentViewSize`, and size 0 draws none of them. Three
  things kept it there. `SC.screensize` was force-set to 0 in `InitSound` when the screen-size row was
  taken out of the options menu; nothing applied `SC.screensize` to `currentViewSize` anyway
  (`InitData`'s `for (i=0;i<currentViewSize;i++)` cannot loop — the four `ChangeViewSize(false)` calls
  above it have just driven that to 0, and it is the DOS source's, where the options slider was the
  only thing that ever set the view size); and in HD `ChangeViewSize` returned before clearing
  `resizeScreen`, so F9/F10 did nothing and jammed each other after one press. Sizes 0..3 are all
  320x200 in `viewSizes[]` and differ only in HUD coverage, so HD can offer those four and refuse 4+,
  which is what `MaxViewSize()` now says. Default is 3, the full overlay HUD over a full-screen view.
  Verified by looking: PPM at tick 150 of `-nointro`, no HUD before and the full status bar after.
- **2026-08-05** — *Requested:* the options menu's camera delay row is now HUD density, and the delay
  is pinned to its minimum (a live rear-view camera). The row drives `SC.screensize` through
  `MaxViewSize()`, not the DOS 0..9, and reads the natural way round rather than the DOS slider's
  inverted one — full bar is the full HUD, so left/right match the volume rows above. The original
  `menuscrsli` artwork is still in `GREED.BLO` and is reused, with the word "SCREEN" baked into its
  title strip painted out (rows 32..39, x 45..85, measured by dumping `screen` from a temporary hook
  rather than guessed) and "HUD" printed in its place at `fontbasecolor` 178, which lands the glyph
  ramp in the artwork's own magenta. The 0 and 10 range markers either side are baked and left alone;
  they were already only roughly the range in the DOS build and are wrong by more in HD, where the
  range is 0..3.
- **2026-08-05** — *Requested:* A.S.S. cam default moved C -> V, motion sensor S -> C. The sensor was
  the DOS build's hardcoded `SC_S`, which in this port's WASD defaults is `bt_south` — every step
  backwards toggled it. Note what this exposed: there is **no key config screen anywhere in this
  port** (SETUP.EXE was where the DOS game did that), so `SC.ckeys` read back from a saved SETUP.CFG
  pins a binding with no way to reach the new default. SETUP.CFG is now version 3 and `InitSound`
  drops the bindings from any older file, keeping the rest of it and writing the defaults back on the
  next save. Confirmed by hexdump of the written file: version 3, `ckeys[6]` 0x2e -> 0x2f, camdelay
  35 -> 0.
- **2026-08-06** — *Requested:* the help page (F1) is drawn rather than loaded. It was the `INFO1`
  lump, one baked image with every key name painted into it, and it had been wrong since the day the
  controls were modernised — arrows for movement, Z jump, X use item, A for the cam, Space for doors,
  and "RUN SETUP IN DOS TO ALTER THE ABOVE KEYS", which there is no longer any such thing as. The
  bindable rows now read out of `scanbuttons[]`, so they cannot drift again; the fixed rows are
  spelled from the same `SC_` constants `PlayerCommand` tests. `HelpKeyName` names a scan code, using
  `ASCIINames` for anything that produces a character.
  Backdrop is `BRIEF3`, the weapons still life from the first briefing — the same artwork INFO1 had
  dimmed behind its text. Two things worth knowing for anything else that draws over a `loadscreen`:
  a screen brings its own palette, and BRIEF3's differs from the game's in 748 of 768 bytes, so menu
  font colours mean nothing over it. The page dims that palette to 24% and builds three seven-entry
  text ramps in indices 3..31, which a histogram of the lump shows it does not use. And `colors[]` is
  a global the fades read: ShowHelp now puts the game palette back into it on the way out, which INFO1
  never needed because its palette *was* the game's, byte for byte.
  The wait loop also pumps frames now instead of `Wait(10)`; `screen` was the visible framebuffer in
  DOS, so nothing here ever had to present, and the page was drawn by the single blit inside
  `VI_SetPalette`. Verified by looking at the rendered page.
- **2026-08-06** — *Requested:* the mission-briefing fade-*outs* are skippable now, finishing the
  2026-08-05 entry above. That entry made every ramp on a briefing page snap on a keypress and left
  one beat that could not: the fade to black between pages, which is the engine's `VI_FadeOut`, whose
  loop tests no input at all. So the player pressed space to turn the page and then sat through 64
  ticks with no way out, seven times a briefing. `BriefingFadeOutPage()` is the mirror image of
  `BriefingFadeInPage` — same 64 steps, same `if (newascii) i=steps;` counter-jump — and replaces the
  seven `VI_FadeOut` calls inside `MissionBriefing`. `BriefingFadeStep` gained a target-palette
  argument so the ramp to black can share it. `VI_FadeOut` itself is untouched: the intro, the menus
  and the end-game screens share it and must not become interruptible.
  The one thing that is *not* symmetric with the fade-in: this fade begins with a press already
  latched, the one `BriefingWaitKey` just took to turn the page, so it clears `newascii` on entry or
  it would snap on its own first step. Nothing downstream wants that press — the Esc test runs before
  the fade and the next page clears `newascii` anyway.
  Measured, not argued: scripting presses from inside `Sys_Frame` again and printing the tick at each
  end of the fade, an untouched fade ran 152→222 and a tap at 177 ended it at 178 — one step later,
  because the jump lets the final step run. The baseline number is the one that matters most; it is
  what proves the entry-clear works, since without it every fade would have ended on step 1. Then
  confirmed by hand in the real game. The end-game fades were considered and deliberately left alone.
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

## Phase 6 — Retina presentation, fullscreen, display options

### 6a — Retina + fullscreen by default (**done**, 2026-08-04)

The window was created without `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so SDL pinned the macOS backing store
to 1× and the compositor bilinearly upscaled that to the panel — the `SDL_SCALEMODE_NEAREST` "chunky
pixels" intent was being undone by an OS blur applied afterwards. `Info.plist` already had
`NSHighResolutionCapable`, which is necessary but not sufficient.

- `sys_video.c` `VI_Init`: adds `SDL_WINDOW_HIGH_PIXEL_DENSITY`, and `SDL_WINDOW_FULLSCREEN` when
  `SC.fullscreen`. Measured on a 14" MBP: window points 1512×949, **pixels 3024×1898**, density 2.00.
  Before the change points and pixels were equal.
- `SDL_SyncWindow` right after creation. macOS animates into fullscreen asynchronously and nothing
  pumps events between `VI_Init` and the first frame, so without it the window reported its
  pre-transition size and `SDL_GetWindowFlags` still said windowed.
- Scale mode is now `SDL_SCALEMODE_PIXELART` (SDL ≥ 3.4). At a Retina backing store the 4:3 box is
  almost never an integer multiple of 320×200 — 3024×1898 gives x 7.9, y 9.49 — and `NEAREST` then
  makes some source pixels one device pixel wider than their neighbours. `PIXELART` keeps hard edges
  while sizing every source pixel equally.
- **The letterbox calculation now exists once.** `Sys_GetPresentRect` in `sys_video.c` is
  unit-agnostic; `VI_BlitView` passes device pixels (`SDL_GetWindowSizeInPixels`, correct because no
  logical presentation is set) and `Sys_MapWindowToGame` passes window points (`SDL_GetWindowSize`,
  correct because SDL reports mouse coordinates in points). `sys_input.c` used to carry a second copy
  complete with its own `4.0f`/`3.0f`/`320.0f`/`200.0f` literals. With high-DPI on, those two unit
  systems finally differ, so the duplication had to go before it could desync.
- Cmd-F / F11 goes through `VI_SetFullscreen` and updates `SC.fullscreen`, so the shortcut and the
  menu row cannot disagree and the choice survives into the next run.

### 6b — `SETUP.CFG` version 2 (**done**, 2026-08-04)

`SoundCard` gained `fullscreen` and `hdmode`, appended at the end. `LoadSetup` demanded
`hdr.structsize == sizeof(SoundCard)` exactly, which would have rejected every existing file and reset
volumes *and* key bindings for the sake of two ints. It now accepts `structsize <= sizeof(SoundCard)`
for `version <= SETUP_VERSION` and reads only the bytes the file has; `InitSound` assigns the tail's
defaults **before** calling it. Verified with a synthetic v1 file: `musicvol`/`sfxvol` survived, the
new tail took its defaults, and bad magic / future version / oversized struct all still fall back.

`SC.screensize` is forced to 0 on load — see 6c.

### 6c — Display options on the options menu (**done**, 2026-08-04)

Menu screens are single pre-rendered 258×174 bitmaps; `cursors[][]` holds only highlight rectangles,
so a new row needs either free space in the artwork or drawn text. There is no free space: the blank
strip below the last options row is 8px, and the apparent gap in the left column is the widget
preview's bevel.

The **screen size** slider was removed and its space reused. It was a 1995 way to buy frame rate by
rendering less of the view, and it was already broken below full size — `RF_BlitView` (`D_video.c`)
copies a fixed 320×200 and ignores `windowWidth`/`windowHeight`, unlike the DOS original in
`RA_DRAW.ASM`. Its row plus the blank strip below is repainted by `MenuShowOptions` and relabelled as
three drawn rows:

| index | row | control |
|---|---|---|
| 7 | `RENDERER` — ORIGINAL / HD | ←/→, or Enter/click to toggle |
| 8 | `FULLSCREEN` — ON / OFF | as above; applies live |
| 9 | `CAMERA DELAY` | unchanged slider, moved down |
| 10 | back | was 9 |

- Labels use `font1` with `fontbasecolor=228`, which maps its 1..6 glyph gradient onto palette indices
  229..234 — the exact amber ramp the baked labels above are drawn in.
- Rows 7 and 8 have no widget bitmap and therefore no clickable arrows, so `Execute` toggles them; the
  current value is also drawn in the preview box at (35,29), matching how the violence and animation
  rows show their state there as artwork.
- `MenuShowCursor` now draws contents **before** the highlight rectangle, and `MenuShowOptions` ends
  with `MenuDrawCursorBox`. With the old order the panel repaint landed on top of the highlight and
  erased it for exactly the three new rows.

`SC.hdmode` is persisted and settable but **not yet wired to anything** — see 6d.

### 6d — Span tag widened (**done**, 2026-08-04)

This was the blocker for any taller view. A span tag packed depth and the index into `spans[]` into
one integer so that sorting the tags sorts the spans back to front (descending — see `Partition`).
It was 32 bits: 20 of z above **12** of index. Every bit was spoken for, so `MAXSPANS` could not be
raised past 4096 — about 20 spans per scanline at 200 rows, which does not survive 900.

- New `spantag_t` (`unsigned long long`) with a 20-bit index field: `ZSHIFT 20`, `ZTOFRAC 12`,
  `ZMASK (0xfffffULL<<20)`, `SPANMASK 0xfffffULL`. The z field keeps its width, contents and position
  *relative to the index*, so the sort order is unchanged. `MAXSPANS` 4096 → 262144, `MAXPEND`
  3072 → 32768.
- **The cast at each composition site is load-bearing.** `pointz` is a 32-bit signed `fixed_t` and
  `pointz<<12` overflows it; all nine sites are now `((spantag_t)pointz<<ZTOFRAC)&ZMASK`, and the six
  `unsigned span;` locals became `spantag_t`. Nothing warns on the narrowing, so these were found by
  mapping every declaration to its function rather than by the compiler.
- Recovery masks before shifting: `(tag&ZMASK)>>ZTOFRAC`. The old layout let the top 8 bits of the
  index fall into the low bits of the recovered depth, so a floor's texture stepping depended on how
  many spans happened to precede it in the list. Worst-case depth error is 255/65536 either way — the
  old one just jittered with list position instead of being a clean floor.
- **All nine `MAXSPANS exceeded` / `MAXPEND` guards are now unconditional.** They were `#ifdef
  VALIDATE`, and `VALIDATE` is defined nowhere in this tree, so an overflow wrote past `spans[]` in
  silence. Verified by building with `MAXSPANS 512` against a frame needing 772: it now stops with
  `MAXSPANS exceeded, FlatSpan (512>=512)`.
- Cost: `__common` 1.4 MB → 16.8 MB, all zerofill (`spans[]` 12 MB at 48 bytes/entry, `spantags[]`
  2 MB, `spansx[]` 1 MB). Address space, not resident memory. Sized generously on purpose — the real
  peak is unknown until the renderer runs taller, and guessing low is now a loud failure.

Regression checked against a pre-change build at a fixed spawn (deterministic: two runs byte-identical).
Same `numspans`/`transparentposts` (772/235); **155 of 64000 pixels differ (0.24%)**, 147 of them on one
ceiling scanline, from the depth-recovery change above. Frames are visually indistinguishable.

### 6e — HD renderer (**done**, 2026-08-04)

`SC.hdmode` now runs the 3D view at the display's own pixel count and aspect.

**The aspect, without splitting the projection.** `TransformVertex` derives `px`, `floory` *and*
`ceilingy` from a single `SCALE`, so giving the axes independent scales would have meant classifying a
dozen `FIXEDDIV(SCALE,z)` / `FIXEDMUL(z,ISCALE)` call sites as horizontal or vertical — the most
error-prone thing in the whole change. It is unnecessary. Keep one uniform projection, take its scale
from the *height*, and make the buffer `1.2 * aspect` as wide as it is tall; presenting that stretched
to fill the window reproduces exactly the 1.2 vertical stretch a 320x200 image already gets in a 4:3
box. Vertical FOV then stays at its original 64 degrees at every aspect and only the horizontal opens
up — 90 degrees at 4:3, 100 at 16:10, 106 at 16:9. **320x200 is precisely the 4:3 case of that rule**
(0.8*200 == 320/2 == 160), so `SetViewSize` needed one new `proj` variable, not a fork.

**Compositing.** All 2D chrome — status bar, weapon, HUD, map modes, messages — was drawn into
`viewbuffer`, which in HD is the wrong size for 320x200 artwork. `hudylookup`/`hudWidth`/`hudHeight`
now name the chrome's target: the view buffer in original mode, `screen` in HD. Because they *alias*
the originals in original mode, the retargeting across `Display.c`, `D_font.c`, `D_video.c` and
`Raven.c` is provably a no-op there, which the golden test confirmed. In HD the chrome is uploaded as
a second 320x200 texture over the view, keyed on palette index 0 — already the engine's "nothing
drawn" value, since every `VI_DrawMaskedPic*` skips it.

**Telling a 3D frame from a menu.** `RF_BlitView` sets `hdviewfresh`; `VI_BlitView` clears it after
presenting the two layers. Without a fresh frame it is a menu, fade or cutscene showing `screen`
directly, which takes the ordinary 4:3 path. This avoids threading a mode flag through every menu,
fade and cutscene entry point. After presenting, `VI_BlitView` composes the view down into `screen`
wherever the chrome left a hole, so the snapshot those callers take is the last visible frame.

**Traps hit on the way:**
- `newplayer` walks `ChangeViewSize` up and down four times to force the tables to rebuild, which put
  the view straight back to 320x200. `ChangeViewSize` is now a no-op in HD, where the render
  resolution replaces `viewSizes[]` entirely.
- `RearView` renders a 64x64 square camera and restored the view with `viewSizes[currentViewSize]`,
  which is not the HD size. It now saves/clears `hdmode` for the square projection and restores via
  `mainViewWidth/Height`.
- `Playfli` decoded FLI frames through `viewylookup` while the whole-frame reader and the flip both
  treat `viewbuffer` as packed 320x200. Given an explicit `SCREENWIDTH` stride.
- Composing into `screen` inside `RF_BlitView` — the obvious place — makes the overlay fully opaque
  and hides the view. It has to happen after the overlay has been read out.

**Sky.** The `windowWidth - 257` backdrop phase term compensates for the left/right branch sign flip
at the view midpoint, where `backtangents[TANANGLES/2]` is the projection scale. It is now
`2*viewproj - 1`, which is congruent mod 256 to the old form for *every* original view size (proj is
width/2 there) and correct in HD. The two halves meet with the same constant 1-pixel step at any
projection scale — checked analytically and by eye on map 15.

**Measured**, 3024x1898 window on an M-series Mac, at spawn on map 0:

| mode | view | pixels | vsynced | uncapped |
|---|---|---|---|---|
| original | 320x200 | 64,000 | 119 fps | 341 fps |
| HD | 1574x824 | 1,296,976 | 119 fps | 278 fps |

20x the pixels for ~18% of frame time, so rasterisation is not the bottleneck at this scale. `numspans`
went 774 -> 3538, comfortably inside the widened limit. One scene on one level; a crowded firefight
will be heavier.

**Verified**: original mode byte-identical to a pre-change build after *each* of 2a, 2c, 2d, 2e and 2f
(fixed spawn, settled snapshot, three runs agreeing). HD checked by eye against the original — wider
horizontally, wall band at the same vertical proportion (28% vs 29% of frame height). Both modes ran
12s clean with no crash reports.

**Look up/down was clamped to the original 200-row scale** (reported by the user). `MAXSCROLL` is a
flat 60 *view rows* — 30% of a 200-row view, but under 7% of an HD one, so looking up and down barely
moved. It is now `VIEWSCROLL(h) = h*60/200`, exactly 60 at 200 rows, and `SCROLLRATE` and the mouse-look
delta scale the same way so the feel is unchanged. Two things this had to avoid breaking:

- `player.scrollmin` doubles as a **vertical aim angle** — seven `(-player.scrollmin)&ANGLES` sites feed
  autoaim and shot pitch. Left alone, a wider row range would have swung shots wildly. They now go
  through `ScrollAngle()`, which normalises back to the 200-row scale. Measured identical at both
  resolutions: full look-up gives aim 59 and full look-down 965 in original *and* HD.
- `yslope[]` is indexed `[y + MAXSCROLL]` and sized `MAX_VIEW_HEIGHT + MAXSCROLL2`; both now track the
  scaled range (`MAXSCROLL2` is `2*VIEWSCROLL(MAX_VIEW_HEIGHT)`).

Verified: original mode byte-identical again afterwards; HD scrolls ±29.9% of the view against the
original's ±29.5%, checked by forcing full up/level/down and looking at all three.

**Long thin trapezoids fanning across the view** (reported by the user, on sloped ground).
`RenderPolygon` walks the polygon edges with `rightstep=(deltax<<FRACBITS)/deltay`. Once the
difference of projected x passes 32767 that shifts past `INT_MAX`, the step comes back wrapped
negative, and the edge strides the wrong way laying trapezoids across the screen.

Measured at the same spot: original mode's largest `|deltax|` is **10400** and never overflows; HD
reaches **42835** and overflows 21 times in one frame. The HD projection scale is ~4x the original's
160, which pushes near-plane-clipped vertices past a limit the 1995 fixed-point code was comfortably
inside at 320x200. Fixed by bounding each projected x to 16000 either side of centre in
`ZClipPolygon`, which caps every difference at 32000. The bound is an order of magnitude outside any
real view, so edges crossing the visible columns are untouched; only edges already sweeping absurdly
far off-screen are pinned, and those were producing garbage. Verified: overflow count is 0 on every
map tested, original mode's numbers are unchanged, and original mode is still byte-identical.

Also scaled, in the same pass: `ZClipPolygon`'s `vertexy > 640` reject. That constant is in the DOS
original verbatim and is a screen-row bound on a magnitude (`clipty*SCALE/z`) that grows with the
projection, so it starts discarding ordinary ceiling polygons at an HD scale — 940 per frame on map
20, versus none in original mode. Now scaled with the projection and left at exactly 640 for every
original view size. **This one is unproven against a visible symptom**: on the frames compared, the
rejections made no difference to the image. It is fixed as a resolution assumption, not as a
demonstrated repro.

**Two more constants tuned for a projection scale of 160**, found while chasing reported texture
warping:

- `DrawWall`/`DrawSteps` cull a column with `if (scale<1000)`, where `scale` is the texel step and so
  proportional to 1/projection. That culls anything nearer than 2.44 world units at 320x200 but
  **10.1** at an HD scale — four times too eager, dropping columns off near walls. Now `viewminscale`,
  which is exactly 1000 when the projection is 160.
- `ISCALE` is `FRACUNIT/proj`, an integer division whose rounding error grows with the projection:
  0.146% at 160, **0.450%** at 659. It is a texture step, so the error accumulates down a post. HD now
  divides directly (`TEXELSTEP`), exact to 1/65536; original mode keeps the historical expression and
  is byte-identical. Changes 4.2-9.2% of HD pixels.

**The remaining `#ifdef VALIDATE` overflow guards are now compiled in** (`vertexlist` in R_render.c and
R_conten.c, `entries` in R_render.c), matching what was done for the span guards. Measured peaks in HD
are nowhere near their limits -- vertexlist 102/1536, entries 89/1024, spans 7638/262144, transparent
posts 2361/32768 -- so none of these is a live problem, but an overflow will now name itself instead
of corrupting the heap.

### Still open: per-column angle quantisation (the zigzag)

`pixelangle[x]` is an **integer** index into a TANANGLES-based table:
`rint(atan((CENTERX-(x+1))/proj) * 2*TANANGLES/PI)`. The +/-0.5 rounding is fixed, but the angle step
between adjacent columns shrinks as the projection grows:

| | projection | angle units per column | rounding as a fraction of one column |
|---|---|---|---|
| original 320x200 | 160 | 32.6 | 1.5% |
| HD 1574x824 | 659 | 7.9 | **6.3%** |

So each HD column's ray is off by up to 6.3% of the spacing to its neighbour rather than 1.5% -- 4.1x
worse, exactly the projection ratio. `pointz` jitters column to column, the wall top row jitters with
it, and horizontal texture features zigzag. It is worst where depth changes fastest per column: near,
oblique walls, which is where it was reported.

Fixing it means finer angular resolution -- raising `TANANGLES` (tables are parameterised by it;
`sines[TANANGLES*5]` and friends grow to ~1.2 MB at 4x) or making `pixelangle` fixed-point and
interpolating `cosines[]` in the inner loop. Either changes original mode's output too, so the
byte-identity check that has guarded every other change here would have to be given up or re-based.
**Not attempted.**

**The freeze on sloped ground** (reported by the user; reproduced under lldb). Scripted the reported
route -- turn 180, walk down the slope on the first level, turn back -- with a watchdog `alarm()` per
frame. It fired: **`RenderPolygon` entered 2 times, its outer loop ran 1,443,511,558 iterations.** A
genuine infinite loop, in a `SlopeSpan` polygon, exactly where it was reported.

`RenderPolygon` walks the polygon with `rightvertex` counting up and `leftvertex` counting down, both
wrapping, and ends on `while (rightvertex!=leftvertex && mr_y!=scrollmax)`. The two walkers are only
compared once per iteration, so they can pass each other without ever being equal on the same pass and
then walk the polygon forever. Captured state at the freeze:

```
mr_y=867  stopy=470  scrollmin=0  scrollmax=880
vertexy = [470, 867, -24264, 23649, 600]
```

Those vertices thousands of rows outside an 880-row view come from near-plane clipping at a 4x
projection scale. At 320x200 the projected values stay small and the walk always converges, which is
why the original never hit this.

Two fixes, both in `RenderPolygon`:
- The `skipleftvertex` path had no `leftvertex==rightvertex` guard while `skiprightvertex` did -- an
  asymmetry present in the DOS original too (`source_dos/CODE/R_PLANE.C`). A run of vertices sharing a
  y spins there forever. Mirrored the right-hand guard.
- Bounded the trapezoid walk at `2*numvertex + (scrollmax-scrollmin) + 8`. Every legitimate iteration
  either consumes a vertex or advances `mr_y` down the view, so that count is already unreachable; it
  exists only so a malformed polygon cannot hang the game.

Verified: the scripted repro now runs to completion (`z 183..222`, full 360 degree sweep at the bottom
of the slope), and original mode is still byte-identical.

**Noted, not fixed:** the brute-force position sweep used to find this also produced a segfault in
`MapRow` with `span_p->picture == NULL` for an `sp_flat` span -- `mr_picture = lumpmain[flatlump+flatpic]`
reads the lump cache directly and is NULL when that flat was never cached. It was only reachable by
teleporting the player into map regions normal play does not enter, so it is likely a harness artifact
rather than a live bug; it has not been reproduced from legitimate movement.

**Known limits:**
- The 320x200 chrome is stretched to the window, so at a wide aspect the weapon and status bar are
  proportionally wider than the artwork intends.
- `Display.c` writes index 0 to *erase* HUD elements (an indicator light going out). In HD those read
  as transparent rather than black. Only reachable at HUD levels >= 1.
- A live window resize keeps the HD resolution chosen for the old size until the next menu exit.

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

---

# Windows port

There is one, and it builds from these same sources — see **[`windows_plan.md`](windows_plan.md)**.

The short version: `src/` and `platform/` moved to `source_shared/` and are compiled unchanged by
both targets. The engine needed **no** Windows-specific changes at all, and the SDL3 platform layer
needed nine `#ifdef` sites in 1,937 lines. Everything genuinely macOS-specific turned out to be in
the build — `MACOSX_BUNDLE`, `Info.plist`, `.icns`, `codesign`, `Contents/Resources` — which is what
`source_macos/CMakeLists.txt` now holds and nothing else.

Two findings from that port are worth reading back into this one:

- **`SDL_GetPrefPath` was already doing the portable thing.** The read-only-install problem
  `sys_files.c` exists to solve is the same problem on both platforms, and the macOS solution needed
  no adaptation.
- **A path can be relative on one platform and absolute on the other.** The engine's forty
  `A:\GREED\MOVIES\*.FLI` strings are relative on macOS — which is the only reason this port's
  resolver ever saw them and normalised them. On Windows they parse as valid absolute paths and
  bypassed the fix entirely. Worth remembering before adding any other short-circuit to
  `sys_files.c`.
