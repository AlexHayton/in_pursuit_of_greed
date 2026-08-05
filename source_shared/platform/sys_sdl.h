/* Include SDL3 through this header, never <SDL3/SDL.h> directly.

   The engine defines its own boolean in d_global.h:

       typedef enum{false,true} bool;

   which is int-sized -- 4 bytes.  SDL's headers pull in <stdbool.h>, and
   because this tree is compiled as C90 (where `bool` is not yet a keyword)
   that macro-defines `bool` to `_Bool`, which is 1 byte.

   From that point on, in that translation unit, every engine header declares
   its booleans one-byte-wide while the engine's own .c files define them
   four-byte-wide.  For scalars it usually survives by luck on a little-endian
   machine.  For arrays it does not:

       extern bool keyboard[NUMCODES];

   read through a 1-byte stride here and a 4-byte stride in D_ints.c refer to
   completely different memory.  That silently killed all keyboard input --
   the platform layer set keyboard[SC_W] at byte offset 0x11 while the engine
   read it at 0x44.

   Including SDL through here restores the engine's meaning of bool, true and
   false immediately afterwards, so the include order of engine headers stops
   mattering.  SDL's own prototypes are already parsed by then and keep using
   the real _Bool; sdl_bool below names that type for the few places that need
   to hold something SDL handed back. */

#ifndef SYS_SDL_H
#define SYS_SDL_H

#include <SDL3/SDL.h>

/* Capture SDL's boolean before taking the name back. */
typedef bool sdl_bool;

#undef bool
#undef true
#undef false

/* d_global.h must have been included before this point, or `bool` is now
   undefined rather than restored.  This also fails loudly if the engine's
   bool ever stops being int-sized. */
typedef char sys_sdl_bool_size_check[(sizeof(bool) == sizeof(int)) ? 1 : -1];

#endif /* SYS_SDL_H */
