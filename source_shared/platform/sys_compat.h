/* Force-included into every translation unit (see cmake/engine.cmake).

   Supplies the MSVC/Watcom runtime pieces the 1996 sources assume, and routes
   file access through the path resolver in sys_files.c.  Keeping this in one
   force-included header is what lets src/ stay a near-verbatim copy of
   source_win32/Win32/. */

#ifndef GREED_SYS_COMPAT_H
#define GREED_SYS_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- Platform libc ------------------------------------------------------ */
/* These must come BEFORE the fopen/open macros at the bottom of this header.
   Both CRTs declare open() in one of them, and if the macro is already in
   effect that declaration gets rewritten into a redeclaration of Sys_open.  On
   POSIX that is harmless by luck -- <fcntl.h>'s `int open(const char *, int,
   ...)` matches Sys_open's prototype exactly -- but the UCRT declares open()
   with __declspec(dllimport) and a differing signature, so getting the order
   wrong is a hard compile error on Windows rather than a silent nothing. */

#ifdef _WIN32
  #include <io.h>          /* _open, open, filelength      */
  #include <fcntl.h>       /* O_BINARY, O_RDONLY, ...      */
  #include <sys/stat.h>    /* _S_IREAD, _S_IWRITE, S_IREAD */
#else
  #include <strings.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/stat.h>
#endif

/* ---- MSVC spellings ---------------------------------------------------- */

/* stricmp, strnicmp, filelength, open and S_IREAD are all real in the UCRT
   under their non-stdc names, so on Windows the engine's spellings need no
   help -- and defining over them would collide.  engine.cmake defines
   _CRT_NONSTDC_NO_WARNINGS to stop the deprecation noise. */
#ifndef _WIN32
#define stricmp   strcasecmp
#define strcmpi   strcasecmp
#define strnicmp  strncasecmp
#endif

/* MSVC's strtok_s takes the same three arguments in the same order as POSIX
   strtok_r, so this is a rename rather than a shim. */
#ifdef _WIN32
#define strtok_r  strtok_s
#endif

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
   and writes saves and settings to the same directory.  That cannot work as-is
   on either target: the Resources directory inside a signed .app is read-only,
   and so is a Program Files install.  Sys_fopen/Sys_open split reads from
   writes; see sys_files.c.

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
