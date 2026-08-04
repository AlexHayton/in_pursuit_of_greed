# Guidelines

Work happens in `source_macos/`. Keep `macos_plan.md` updated as you go.

## Build

```sh
cd source_macos
cmake --build build -j8            # Release, the one the user runs
cmake --build build-debug -j8      # -g -O0, for lldb
```

Rebuild the directory the user is actually running. Watch the shell's cwd — a `cd` in an earlier
command persists, and `cmake --build build` from the repo root silently targets nothing.

## Authority

`source_dos/` decides intended behaviour, **not** `source_win32/`. The Win32 tree is unfinished and
contains workarounds for GDI and leftover debug code that read like engine invariants. Diff against
the DOS source before preserving anything odd.

## Bug classes seen in this port

- **LP64.** Pointers through `int`, `sizeof(int)` sizing pointer arrays, `long` where the engine means
  32 bits. `fixed_t` and `longint` must stay 32-bit.
- **`bool` shadowing.** The engine's `bool` is a 4-byte enum; SDL's headers macro it to 1-byte `_Bool`.
  Include SDL only via `platform/sys_sdl.h`, which restores it. A compile-time assert guards this.
- **Read-only string literals.** Watcom's were writable. Never mutate a `char *` argument in place.
- **Spin loops.** The tick is main-thread now, so any loop waiting on `timecount`, `keyboard[]` or
  `newascii` must call `Sys_PumpFrame()` or it hangs forever.
- **Framebuffer is top-down.** No flips anywhere. Both `RF_BlitView` and the present path copy row for
  row.

## Verifying

- ASan is broken on this macOS — it deadlocks in its own initialiser. Use `build-debug` + lldb, or the
  crash report in `~/Library/Logs/DiagnosticReports/`.
- `-nointro` skips `MissionBriefing` entirely (`if (netmode || nointro) return;`). Headless tests using
  it prove nothing about that code.
- `screencapture` is blocked by permissions. To check anything visual, dump `screen` through `curpal`
  to a PPM temporarily and read the image — then remove the dump.
- Don't claim a visual or behavioural fix works without looking. "Correct by construction" has been
  wrong here more than once.

## Style

Match the surrounding 1995 code: same indentation, same brace placement, no reformatting. Comment *why*
a change was needed — usually which platform assumption broke — not what the code does.
