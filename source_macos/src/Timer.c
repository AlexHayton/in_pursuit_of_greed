/****************************************************************************
*
*                   Digital Sound Interface Kit (DSIK)
*                            Version 2.00
*
*                           by Carlos Hasan
*
* Filename:     timer.c
* Version:      Revision 1.1
*
* Language:     WATCOM C
* Environment:  IBM PC (DOS/4GW)
*
* Description:  Timer interrupt services.
*
* Revision History:
* ----------------
*
* Revision 1.1  94/11/16  10:48:42  chv
* Added VGA vertical retrace synchronization code
*
* Revision 1.0  94/10/28  22:45:47  chv
* Initial revision
*
****************************************************************************/

/* macOS port: the DOS build hooked IRQ0 and the Win32 port used a
   timeSetEvent multimedia timer.  Both ran the game tick -- and so
   PlayerCommand -> ControlMovement -- on a different thread from the
   renderer, racing on global game state.  The SDL port advances the clock
   from Sys_Frame() on the main thread instead (platform/sys_main.c), so
   these are deliberately no-ops rather than starting a timer. */

#include "timer.h"

void dStopTimer(void)
{
}


void dStartTimer(TimerProc timer,int rate)
{
	(void)timer;
	(void)rate;
}

