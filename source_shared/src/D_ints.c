/***************************************************************************/
/*                                                                         */
/*                                                                         */
/* Raven 3D Engine                                                         */
/* Copyright (C) 1996 by Softdisk Publishing                               */
/*                                                                         */
/* Original Design:                                                        */
/*  John Carmack of id Software                                            */
/*                                                                         */
/* Enhancements by:                                                        */
/*  Robert Morgan of Channel 7............................Main Engine Code */
/*  Todd Lewis of Softdisk Publishing......Tools,Utilities,Special Effects */
/*  John Bianca of Softdisk Publishing..............Low-level Optimization */
/*  Carlos Hasan..........................................Music/Sound Code */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

#include <DOS.H>
#include <STRING.H>
#include <CONIO.H>
#include <STDIO.H>
#include <IO.H>
#include <STDLIB.H>
#include "d_global.h"
#include "d_video.h"
#include "d_ints.h"
#include "d_misc.h"
#include "timer.h"
#include "protos.h"
#include "r_refdef.h"
#include "d_disk.h"

/**** CONSTANTS ****/

#define TIMERINT       8
#define KEYBOARDINT    9
#define VBLCOUNTER     16000
#define MOUSEINT       0x33
#define MOUSESENSE     SC.mousesensitivity
#define JOYPORT        0x201
#define MOUSESIZE      16


/**** VARIABLES ****/

void    (*oldkeyboardisr)();
void    (*timerhook)();                 // called every other frame (player)
bool timeractive;
longint timecount;                     // current time index
bool keyboard[NUMCODES];             // keyboard flags
bool keypause, capslock, newascii;   /* keypause: was "pause", renamed to avoid POSIX pause(3) */
bool mouseinstalled, joyinstalled;
int     in_button[NUMBUTTONS];          // frames the button has been down
byte    lastscan;
char    lastascii;

/* mouse data */
short mdx, mdy, b1, b2;
int   hiding=1, busy=1;           /* internal flags */
int   mousex=160, mousey=100;
byte  back[MOUSESIZE*MOUSESIZE];  /* background for mouse */
byte  fore[MOUSESIZE*MOUSESIZE];  /* mouse foreground */


/* joystick data */
int   jx, jy, jdx, jdy, j1, j2;
word  jcenx, jceny, xsense, ysense;

/* config data */
extern SoundCard SC;


byte ASCIINames[] = // Unshifted ASCII for scan codes
 {
  0  ,27 ,'1','2','3','4','5','6','7','8','9','0','-','=',8  ,9  ,
  'q','w','e','r','t','y','u','i','o','p','[',']',13 ,0  ,'a','s',
  'd','f','g','h','j','k','l',';',39 ,'`',0  ,92 ,'z','x','c','v',
  'b','n','m',',','.','/',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,'-',0  ,0  ,0  ,'+',0  ,
  0  ,0  ,0  ,127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  };

byte ShiftNames[] = // Shifted ASCII for scan codes
 {
  0  ,27 ,'!','@','#','$','%','^','&','*','(',')','_','+',8  ,9  ,
  'Q','W','E','R','T','Y','U','I','O','P','{','}',13 ,0  ,'A','S',
  'D','F','G','H','J','K','L',':',34 ,'~',0  ,'|','Z','X','C','V',
  'B','N','M','<','>','?',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,'-',0  ,0  ,0  ,'+',0  ,
  0  ,0  ,0  ,127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  };

byte SpecialNames[] = // ASCII for 0xe0 prefixed codes
 {
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,13 ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,'/',0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  };

/* Modernised defaults for the macOS port.  The originals are in the trailing
   comments; every one of these is still rebindable from the in-game key
   config menu, and the arrow keys deliberately keep working alongside WASD.

   bt_use moves from Space to E: Space becomes jump (the modern convention),
   and door/switch use needs to stay reachable from the keyboard rather than
   relying on the right mouse button alone. */
int scanbuttons[NUMBUTTONS] =
{
 SC_W,             // bt_north        (was SC_UPARROW)
 SC_RIGHTARROW,    // bt_east         -- turn right; mouse look also turns
 SC_S,             // bt_south        (was SC_DOWNARROW)
 SC_LEFTARROW,     // bt_west         -- turn left
 SC_CONTROL,       // bt_fire         -- also left mouse button
 SC_ALT,           // bt_straf
 SC_E,             // bt_use          (was SC_SPACE); also right mouse button
 SC_LSHIFT,        // bt_run
 SC_SPACE,         // bt_jump         (was SC_Z)
 SC_Q,             // bt_useitem      (was SC_X)
 SC_V,             // bt_asscam       (was SC_A, which is now strafe-left)
 SC_PGUP,          // bt_lookup
 SC_PGDN,          // bt_lookdown
 SC_HOME,          // bt_centerview
 SC_A,             // bt_slideleft    (was SC_COMMA)
 SC_D,             // bt_slideright   (was SC_PERIOD)
 SC_LBRACKET,      // bt_invleft      (was SC_INSERT)
 SC_RBRACKET,      // bt_invright     (was SC_DELETE)
 };


/**** FUNCTIONS ****/


void INT_KeyboardISR()
/* keyboard interrupt
    processes make/break codes
    sets key flags accordingly */
{
 /*static bool special;
 byte           k, c, al;

// Get the scan code
 k=inbyte(0x60);

 if (k==0xE0) special=true;
 else if (k==0xE1) keypause^=true;
 else
  {
   if (special && (k==0x2A || k==0xAA || k==0xAA || k==0x36))
    {
     special=false;
     goto end;
     }
   if (k & 0x80) // Break code
    {
     k&=0x7F;
     keyboard[k]=false;
     }
   else // Make code
    {
     lastscan=k;
     keyboard[k]=true;
     if (special) c=SpecialNames[k];
     else
      {
       if (k==SC_CAPSLOCK) capslock^=true;
       if (keyboard[SC_LSHIFT] || keyboard[SC_RSHIFT])
	{
	 c=ShiftNames[k];
	 if (capslock && c>='A' && c<='Z') c+='a'-'A';
	 }
       else
	{
	 c=ASCIINames[k];
	 if (capslock && c>='a' && c<='z') c-='a'-'A';
	 }
       }
     if (c)         // return a new ascii character
      {
       lastascii=c;
       newascii=true;
       }
     }
   special=false;
   }
end:
// acknowledge the interrupt
 al=inbyte(0x61);
 al|=0x80;
 outbyte(0x61,al);
 al&=0x7F;
 outbyte(0x61,al);
 outbyte(0x20,0x20);*/
 }


void INT_ReadControls(void)
/* read in input controls */
{
 int i;

 /* keyboard[] is refreshed by Sys_Frame() from SDL's keyboard state, so
    unlike the Win32 version this no longer polls the OS itself -- it just
    folds the key flags into the button flags. */

 memset(in_button,0,sizeof(in_button));
 for(i=0;i<NUMBUTTONS;i++)
  if (keyboard[scanbuttons[i]])
   in_button[i]=1;

 if (mouseinstalled)
  {
   /* Mouse buttons are bindable through SETUP.CFG, exactly as the DOS
      version did it (see INT_ReadControls in source_dos/CODE/D_INTS.C).
      The value 2 rather than 1 is the original's marker for "came from the
      mouse", which ControlMovement treats as a held button. */
   if (b1) in_button[SC.leftbutton]=2;
   if (b2) in_button[SC.rightbutton]=2;
   }
 }

/*============================================================================*/

void INT_TimerISR(void)
/* process each timer tick */
{
	timecount+=2;
	if ( timerhook )
		timerhook();
}


void INT_TimerHook(void(* hook)(void))
{
 timerhook=hook;
 }


/*============================================================================*/

/* UpdateMouse, MouseGetClick, ResetMouse, M_Init, M_Shutdown, MouseHide and
   MouseShow are implemented in platform/sys_input.c.  The Win32 port left
   every one of them an empty stub, which is why the menus had no mouse
   support at all. */


/***************************************************************************/

void INT_Setup(void)
{
	memset(keyboard,0,sizeof(keyboard));
	M_Init();
	dStartTimer(INT_TimerISR,1000/35);
	timeractive = true;
}


void INT_ShutdownKeyboard(void)
{
	INT_TimerHook(NULL);
}


void INT_Shutdown(void)
{
 if ( timeractive )
	 dStopTimer();
 if ( mouseinstalled )
	 M_Shutdown();
}