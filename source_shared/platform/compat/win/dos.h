/* Shim for the Watcom/MSVC <dos.h> the original sources include.

   This is the only shim Windows needs: the UCRT ships real versions of every
   other header the engine asks for (CONIO.H, IO.H, MALLOC.H, PROCESS.H,
   TCHAR.H, SYS/STAT.H), and shadowing those would hide the genuine
   filelength(), _open() and kbhit() we want to use.  Hence compat/win/ rather
   than the compat/posix/ set.

   Nothing in the surviving port uses real-mode DOS services -- the interrupt
   code in D_ints.c was already commented out -- so this only needs to exist. */
#ifndef GREED_COMPAT_DOS_H
#define GREED_COMPAT_DOS_H
#endif
