/* Shim for <conio.h>.  MS_Error() drains the keystroke buffer with
   while (kbhit()) getch(); -- under SDL there is no such buffer, so these
   report "no key waiting" and the loop falls straight through. */
#ifndef GREED_COMPAT_CONIO_H
#define GREED_COMPAT_CONIO_H

static int kbhit(void) { return 0; }
static int getch(void) { return 0; }

#endif
