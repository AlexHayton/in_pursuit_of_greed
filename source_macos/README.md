# In Pursuit of Greed — macOS port

A native Apple Silicon build of the 1995 Softdisk/Channel 7 FPS, built from the
game's own C sources rather than emulated.

The base is `source_win32/Win32/`, which had already replaced John Bianca's
hand-written x86 renderer with portable C. What this tree adds is an SDL3
platform layer, working audio, and the 64-bit correctness work the original
sources never needed.

## Building

Needs CMake 3.20+ and the Xcode command line tools. Nothing else — SDL3 and
libxmp are fetched at pinned tags and built from source on first configure,
which takes a few minutes and is then cached.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

The result is `build/Greed.app`, a self-contained bundle: both dependencies are
linked statically, so `otool -L` shows nothing but system frameworks and the
app runs on a clean macOS install.

To upgrade a dependency, bump its tag in `../source_shared/cmake/deps.cmake` and reconfigure.

## Running

```sh
open build/Greed.app
```

The bundle carries its own copy of the game data, so it runs from anywhere.
To play against a data directory outside the bundle — useful while working on
the port — set `GREED_DATA`, which takes a colon-separated search list:

```sh
GREED_DATA=../greed_cdrom:../greed_final ./build/Greed.app/Contents/MacOS/Greed -nointro
```

Assets resolve in this order: the prefs directory, each `GREED_DATA` entry, the
current directory, then `Contents/Resources`. Anything the game *writes* —
`SETUP.CFG`, saved games, recorded demos — always goes to
`~/Library/Application Support/Greed/`, because a signed bundle's `Resources`
is read-only.

Useful flags, all inherited from the original: `-nointro`, `-ticker` (frame
statistics), `-debugmode`, `-nospawn`, `-record` / `-playback`.

### About the game data

`greed_final/` is the installed game and is what the bundle copies from.
`greed_cdrom/` is the *un-installed* CD master — `FIRST.EXE` plus a compressed
`GREED.SHR` — so it has no `GREED.BLO` and no music. The one thing it uniquely
supplies is `MOVIES/`, the FLI intro, which the bundle picks up from there.

## Controls

Modernised defaults. Every one is still rebindable from the in-game config
menu, and the original arrow-key bindings still work alongside them.

| | |
|---|---|
| W / S | forward / back |
| A / D | strafe |
| ← / → | turn |
| mouse | look |
| left mouse / Ctrl | fire |
| right mouse / E | use |
| Shift | run |
| Space | jump |
| Q | use item |
| C | ass-cam |
| Page Up / Page Down / Home | look up / down / centre |
| Insert / Delete | cycle inventory |
| Esc | menu (also releases the mouse) |
| ⌘F or F11 | fullscreen |
| ⌘Q | quit |

The originals, for reference: arrows moved, `,`/`.` strafed, Space used, Z
jumped, X used an item, A was the ass-cam.

## Notes on the port

- **The framebuffer is bottom-up.** `CreateDIBSection` with a positive
  `biHeight` gave Win32 a bottom-up surface, and `RF_BlitView` was written
  against that. The contract is preserved and the flip happens once, at
  present time, rather than by touching every drawing path.
- **4:3 correction.** 320×200 was displayed as 4:3, so the destination rect is
  computed for non-square pixels rather than letting SDL preserve the texture's
  own 1.6:1 aspect.
- **Timing runs on the main thread.** The DOS and Win32 builds fired the 35 Hz
  tick from an interrupt / `timeSetEvent` thread that then mutated game state
  concurrently with the renderer. `Sys_Frame()` drives ticks from the main loop
  instead, capped at 8 catch-up ticks, so the race is gone and the game speed
  is independent of display refresh.

  The catch: the original's loops could sit and watch `timecount`, `keyboard[]`
  or `newascii` change underneath them, because something else was mutating
  those asynchronously. Here, a loop that doesn't call into the platform layer
  never advances at all. Any such loop must call `Sys_PumpFrame()` — if you add
  one that waits on tick-driven state and it hangs, that's why.
- **LP64.** The dominant hazard in the port was pointers and pointer-sized
  arithmetic passing through `int`, and 32-bit fixed-point types silently
  widening to 64. See `macos_plan.md` for the full list of what that broke.
- **Audio.** `Modplay.c` was entirely stubbed out in the Win32 port. It is
  reimplemented against the original DOS logic in `source_dos/CODE/MODPLAY.C`:
  one `SDL_AudioStream` per voice for effects, libxmp for the `.MOD`/`.S3M`
  soundtrack.
- **Networking is out of scope.** `Net.c` compiles but is disabled; it speaks
  to an IPX/serial DOS driver.

## Licences

The game itself is Softdisk Publishing's; this port only replaces its platform
layer. The two build dependencies ship their notices in
`Greed.app/Contents/Resources/licenses/`:

- **SDL3** — zlib licence.
- **libxmp** — MIT, as of 4.5.0. It was LGPL-2.1-or-later before that, which is
  why `../source_shared/cmake/deps.cmake` warns against pinning it back below 4.5.0 without also
  switching it to a shared library.

The build is ad-hoc code-signed, which is enough for Gatekeeper to run it
locally. If macOS still refuses, right-click the app and choose Open.
