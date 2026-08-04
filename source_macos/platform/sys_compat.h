/* Force-included into every translation unit (see CMakeLists.txt).

   Supplies the MSVC/Watcom runtime pieces the 1996 sources assume, and routes
   file access through the macOS path resolver in sys_files.c.  Keeping this in
   one force-included header is what lets src/ stay a near-verbatim copy of
   source_win32/Win32/. */

#ifndef GREED_SYS_COMPAT_H
#define GREED_SYS_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

/* ---- MSVC spellings ---------------------------------------------------- */

#define stricmp   strcasecmp
#define strcmpi   strcasecmp
#define strnicmp  strncasecmp

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef _S_IREAD
#define _S_IREAD  S_IRUSR
#endif
#ifndef _S_IWRITE
#define _S_IWRITE S_IWUSR
#endif

/* ---- File access ------------------------------------------------------- */
/* The game opens read-only assets by bare filename ("GREED.BLO", "SONG0.S3M")
   and writes saves and settings to the same directory.  Inside a signed .app
   the Resources directory is read-only, so reads and writes must resolve to
   different places.  Sys_fopen/Sys_open do that; see sys_files.c.

   These are macros so that the ~20 existing call sites in D_disk.c, Menu.c,
   Utils.c, Event.c and Modplay.c need no edits.  Note _open is mapped too:
   Modplay.c and Utils.c use the underscored MSVC spelling. */

FILE *Sys_fopen(const char *name, const char *mode);
int   Sys_open(const char *name, int flags, ...);

/* sys_files.c itself must reach the real fopen/open, so it #undefs these
   after including this header. */
#define fopen Sys_fopen
#define open  Sys_open
#define _open Sys_open

#include "sys_greed.h"

#endif /* GREED_SYS_COMPAT_H */
