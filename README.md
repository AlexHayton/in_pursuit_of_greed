# In Pursuit of Greed source code

## Source

- Original release (files have been removed): https://redshadowsoftware.com/greed
- Internet Archive: https://archive.org/details/GreedSource

## Layout

| | |
|---|---|
| `source_dos/` | The original 1995 Watcom/DOS sources. The authority on intended behaviour. |
| `source_win32/` | An unfinished MSVC port. Notable for rewriting the x86 asm renderer in C. |
| `source_macos/` | **A native macOS port that builds and runs.** See below. |
| `raven_engine/` | Earlier prototype of the engine. |
| `greed_final/` | The installed game data. |
| `greed_cdrom/` | The un-installed CD master — installer plus a compressed payload. Supplies `MOVIES/`. |

## macOS port

`source_macos/` is a native Apple Silicon build, compiled from the game's own C rather than emulated.
It plays: 3D view, sprites, menus, the FLI cutscenes, sound effects and the tracker soundtrack.

```sh
cd source_macos
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
open build/Greed.app
```

Nothing needs to be installed beyond CMake and the Xcode command line tools. SDL3 and libxmp are
fetched at pinned tags and linked statically, so `Greed.app` is self-contained — `otool -L` shows only
system frameworks — and carries its own copy of the game data.

It starts from `source_win32/Win32/`, which had already replaced John Bianca's hand-written x86
renderer with portable C. On top of that this tree adds an SDL3 platform layer (video, input, audio,
timing, file paths), a reimplementation of the sound code that the Win32 port had stubbed out
entirely, mouse look and WASD, and the 64-bit correctness work the original sources never needed.

Substantive changes to engine behaviour, as opposed to platform plumbing:

- **Modernised controls.** WASD, mouse look, Space to jump. The original bindings still work alongside
  them and everything remains rebindable in the config menu.
- **The mission briefing fades its caption in with the picture** rather than fading the picture,
  holding two seconds, then ramping the text.
- **The 35 Hz tick runs on the main thread.** DOS and Win32 fired it from an interrupt / timer thread
  that mutated game state while the renderer read it; that race is gone.

`source_macos/README.md` covers the controls, the data search path and the port's design decisions.
`macos_plan.md` is the working log — what broke, why, and what is still unverified.
