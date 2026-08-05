/* Shim for the Watcom/MSVC <dos.h> the original sources include.
   Nothing in the surviving Win32 port actually uses real-mode DOS services --
   the interrupt code in D_ints.c was already commented out -- so this only
   needs to exist. */
#ifndef GREED_COMPAT_DOS_H
#define GREED_COMPAT_DOS_H
#endif
