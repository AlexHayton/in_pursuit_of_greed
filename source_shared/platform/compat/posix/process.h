/* Shim for <process.h>, MSVC's home for the spawn and exec families.
   D_misc.c includes it, but the surviving code never spawns a child process. */
#ifndef GREED_COMPAT_PROCESS_H
#define GREED_COMPAT_PROCESS_H
#include <stdlib.h>
#endif
