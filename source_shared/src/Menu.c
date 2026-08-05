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
#include <STDIO.H>
#include <STDLIB.H>
#include <STRING.H>
#include <CTYPE.H>
#include "d_global.h"
#include "d_disk.h"
#include "d_misc.h"
#include "d_video.h"
#include "d_ints.h"
#include "d_font.h"
#include "r_refdef.h"
#include "protos.h"

/**** TYPES ****/

typedef struct
 {
  int x, y;
  int w, h;
  } cursor_t;


/**** CONSTANTS ****/

#define KBDELAY2     10
#define MENUS        6
#define MAXSAVEGAMES 10

cursor_t cursors[MENUS][15]=
 {
  // main menu
   41, 114, 55, 20,   // new
   41, 138, 55, 20,   // quit
   134, 42, 127, 19,  // load
   134, 62, 127, 19,  // save
   134, 82, 127, 19,  // options
   134, 102, 127, 19, // info
   142, 142, 62, 15,  // quit/resume
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,

  // char menu
   41, 138, 55, 20,   // quit
   134, 32, 127, 18,  // cyborg
   134, 51, 127, 18,  // lizard
   134, 70, 127, 18,  // moo
   134, 89, 127, 18,  // specimen
   134, 108, 127, 18, // dominatrix
   142, 142, 62, 15,  // back
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,

  // load
   41, 138, 55, 20,   // quit
   137, 34, 7, 5,
   137, 44, 7, 5,
   137, 54, 7, 5,
   137, 64, 7, 5,
   137, 74, 7, 5,
   137, 84, 7, 5,
   137, 94, 7, 5,
   137, 104, 7, 5,
   137, 114, 7, 5,
   137, 124, 7, 5,
   142, 142, 62, 15,  // back
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,

  // save
   41, 138, 55, 20,   // quit
   137, 34, 7, 5,
   137, 44, 7, 5,
   137, 54, 7, 5,
   137, 64, 7, 5,
   137, 74, 7, 5,
   137, 84, 7, 5,
   137, 94, 7, 5,
   137, 104, 7, 5,
   137, 114, 7, 5,
   137, 124, 7, 5,
   142, 142, 62, 15,  // back
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,

  // options
   41, 114, 55, 20,
   41, 138, 55, 20,
   134, 32, 127, 15,
   134, 46, 127, 15,
   134, 60, 127, 15,
   134, 74, 127, 15,
   134, 88, 127, 15,
   /* Rows 7-9 replace what used to be one baked "screen size" row at 134,101
      and one "camera delay" row at 134,115.  Their space, plus the blank panel
      strip below the old last row, is repainted by MenuShowOptions and
      relabelled here -- which is where the renderer and fullscreen rows fit
      without needing a whole extra menu screen there is no artwork for.

      The setting behind the old screen-size row is back, as row 9: it turned
      out to be what decides how much of the HUD is drawn, so with no row for
      it the game was pinned at size 0 and had no HUD at all.  The camera delay
      it displaced is locked to its minimum instead; see InitSound. */
   134, 103, 127, 11,   // renderer: original / HD
   134, 114, 127, 11,   // fullscreen
   134, 125, 127, 10,   // HUD density (the engine's view size)
   142, 142, 62, 15,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,

  // monster menu
   41, 138, 55, 20,   // quit
   134, 30, 127, 15,  // difficulty level 0
   134, 45, 127, 15,
   134, 60, 127, 15,
   134, 75, 127, 15,
   134, 90, 127, 15,
   134, 117, 127, 15,
   142, 142, 62, 15,  // back
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,


  };


int menumax[MENUS]=               // max cursor
 { 7, 7, 12, 12, 11, 8 };


/**** VARIABLES ****/

pic_t   *menuscreen;
int     menulevel, menucursor, menucurloc, menumaincursor, identity, waitanim,
	saveposition;
longint timedelay;
bool quitmenu, menuexecute, downlevel, goright, goleft, waiting;
char    savedir[MAXSAVEGAMES][21];
pic_t   *waitpics[4];
extern  SoundCard SC;


/**** FUNCTIONS ****/

void VI_DrawMaskedPic2(int x, int y, pic_t  *pic)
/* Draws a formatted image to the screen, masked with zero */
{
 byte *dest, *source, *source2;
 int  width, height, xcor;

 x -= pic->orgx;
 y -= pic->orgy;
 height=pic->height;
 source=&pic->data;
 while (y<0)
  {
   source+=pic->width;
   height--;
   y++;
   }
 while (height--)
  {
   if (y<200)
    {
     dest=ylookup[y]+x;
     source2=y*320+x+viewbuffer;
     xcor=x;
     width=pic->width;
     while (width--)
      {
       if (xcor>=0 && xcor<=319 && *source) *dest=*source;
	else *dest=*source2;
       xcor++;
       source++;
       source2++;
       dest++;
       }
     }
   y++;
   }
 }


void MenuCommand(void);


bool ShowQuit(void (*kbdfunction)(void))
{
 longint animtime, droptime;
 int     anim, y, i, lump;
 short   mx, my;
 pic_t   *pics[3];
 char    c;
 bool result;
 byte    *scr;

 INT_TimerHook(NULL);
 scr=(byte *)malloc(64000);
 if (scr==NULL) MS_Error("Error allocating ShowQuit buffer");
 MouseHide();
 memcpy(scr,viewbuffer,64000);
 memcpy(viewbuffer,screen,64000);
 MouseShow();
 if (netmode) TimeUpdate();
 lump=CA_GetNamedNum("quit");
 for(i=0;i<3;i++) pics[i]=CA_CacheLump(lump+i);
 timedelay=timecount+KBDELAY2;
 Wait(KBDELAY2);
 if (netmode) TimeUpdate();
 newascii=false;
 anim=0;
 MouseHide();
 if (!SC.animation || netmode)
  {
   y=68;
   MouseShow();
   }
  else y=-66;
 droptime=timecount;
 animtime=timecount;
 while (1)
  {
   /* Already quitting -- answer the dialog's own question yes, so the caller
      unwinds the same way it would have if the player had clicked it. */
   if (quitgame)
    {
     c='y';
     break;
     }
   if (y>=67 && MouseGetClick(&mx,&my) && my>=110 && my<=117)
    {
     if (mx>=130 && mx<=153)
      {
       c='y';
       break;
       }
     else if (mx>=162 && mx<=186)
      {
       c='n';
       break;
       }
     }

   /* Pump unconditionally: newascii and timecount below only advance from
      the timer tick, which this port drives from the main thread. */
   Sys_PumpFrame();
   if (netmode) TimeUpdate();

   if (newascii && y>=67)
    {
     c=lastascii;
     break;
     }
   if (timecount>=droptime && y<67)
    {
     if (y>=0) memcpy(ylookup[y],viewbuffer+320*y,640);
     y+=2;
     droptime=timecount+1;
     VI_DrawMaskedPic2(111,y,pics[anim]);
     if (y>=67) MouseShow();
     }
   if (timecount>=animtime)
    {
     anim++;
     anim%=3;
     animtime+=10;
     MouseHide();
     VI_DrawMaskedPic2(111,y,pics[anim]);
     MouseShow();
     }
   }
 if (c=='y' || c=='Y') result=true;
  else result=false;
 droptime=timecount;
 animtime=timecount;
 if (!SC.animation || netmode) y=200;
 MouseHide();
 while (y<199)
  {
   Sys_PumpFrame();     /* y only advances when timecount does */
   if (quitgame) break;
   if (timecount>=droptime)
    {
     if (y>=0) memcpy(ylookup[y],viewbuffer+320*y,640);
     y+=2;
     droptime=timecount+1;
     VI_DrawMaskedPic2(111,y,pics[anim]);
     }
   if (timecount>=animtime)
    {
     anim++;
     anim%=3;
     animtime+=10;
     VI_DrawMaskedPic2(111,y,pics[anim]);
     }
   }
 memcpy(screen,viewbuffer,64000);
 memcpy(viewbuffer,scr,64000);
 for(i=0;i<3;i++) CA_FreeLump(lump+i);
 MouseShow();
 free(scr);
 timedelay=timecount+KBDELAY2;
 turnrate=0;
 moverate=0;
 fallrate=0;
 strafrate=0;
 ResetMouse();
 INT_TimerHook(kbdfunction);
 if (netmode) TimeUpdate();
 return result;
 }


/******************************************************************************/


void ShowMenuSliders(int value,int range)
{
 int a, c, d, i;

 MouseHide();
 d=(value*49)/range;
 for(a=0;a<d;a++)
  {
   c=(a*32)/d + 140;
   for(i=49;i<65;i++)
    *(ylookup[i]+a+42)=c;
   }
 if (d<49)
  for(a=d;a<49;a++)
   {
    for(i=49;i<65;i++)
     *(ylookup[i]+a+42)=0;
    }
 MouseShow();
 }


void SaveDirectory()
{
 FILE *f;

#ifdef GAME1
 f=fopen("SAVE1.DIR","w");
#elif defined(GAME2)
 f=fopen("SAVE2.DIR","w");
#elif defined(GAME3)
 f=fopen("SAVE3.DIR","w");
#else
 f=fopen("SAVEGAME.DIR","w");
#endif

 if (f==NULL)
  MS_Error("SaveDirectory: Error creating SAVEGAME.DIR");
 if (!fwrite(savedir,sizeof(savedir),1,f))
  MS_Error("SaveDirectory: Error saving SAVEGAME.DIR");
 fclose(f);
 }


void InitSaveDir()
{
 int  i;

 for(i=0;i<MAXSAVEGAMES;i++)
  {
   memset(savedir[i],(int)' ',20);
   savedir[i][20]=0;
   }
 SaveDirectory();
 }


void ShowSaveDir(void)
{
 FILE *f;
 int  i, j;

#ifdef GAME1
 f=fopen("SAVE1.DIR","r");
#elif defined(GAME2)
 f=fopen("SAVE2.DIR","r");
#elif defined(GAME3)
 f=fopen("SAVE3.DIR","r");
#else
 f=fopen("SAVEGAME.DIR","r");
#endif

 if (f==NULL) InitSaveDir();
 else
  {
   if (!fread(savedir,sizeof(savedir),1,f))
    MS_Error("ShowSaveDir: Savegame directory read failure!");
   fclose(f);
   }
 fontbasecolor=93;
 font=font1;
 MouseHide();
 for(i=0;i<MAXSAVEGAMES;i++)
  {
   printx=148;
   printy=34+i*10;
   for(j=0;j<6;j++)
    memset(ylookup[printy+j]+printx,0,110);
   FN_Printf(savedir[i]);
   }
 MouseShow();
 }


/* The bottom three option rows have no baked artwork -- see the note in
   cursors[] -- so their labels are drawn.  The panel interior is palette index
   1; repainting with that rather than 0 keeps them sitting on the same
   background as the rows above instead of in a black box. */
#define OPTPANEL_X    132
#define OPTPANEL_W    130
#define OPTPANEL_Y    103
#define OPTPANEL_H    32

static void MenuDrawOptionRow(int y, char *label, char *value)
{
 /* font1's glyphs are a 1..6 vertical gradient added to fontbasecolor, so 228
    lands them on 229..234 -- the same amber ramp the baked labels above are
    drawn in. */
 fontbasecolor=228;
 font=font1;
 printx=140;
 printy=y;
 FN_RawPrint3(label);
 printx=212;
 printy=y;
 FN_RawPrint3(value);
 }


/* The value also goes in the preview box at (35,29), where the slider rows put
   their widget bitmap.  That is the established shape of this screen: the
   violence and animation rows show their state there too, as artwork. */
static void MenuShowOptionValue(char *value)
{
 int i;

 for(i=29;i<80;i++)
  memset(ylookup[i]+35,0,64);
 fontbasecolor=228;
 font=font1;
 printx=35+(64-FN_RawWidth(value))/2;
 printy=52;
 FN_RawPrint3(value);
 }


/* Drawn last, by everything that repaints under it -- see MenuShowCursor. */
void MenuDrawCursorBox(void)
{
 int x, y, w, h, i;

 if (menucurloc==-1) return;
 x=cursors[menulevel][menucurloc].x;
 y=cursors[menulevel][menucurloc].y;
 w=cursors[menulevel][menucurloc].w;
 h=cursors[menulevel][menucurloc].h;
 if (!w || !h) return;
 memset(ylookup[y]+x,133,w);
 memset(ylookup[y+h-1]+x,133,w);
 for(i=y;i<y+h;i++)
  {
   *(ylookup[i]+x)=133;
   *(ylookup[i]+x+w-1)=133;
   }
 }


void MenuShowOptions(void)
{
 int i;

 MouseHide();

 for(i=OPTPANEL_Y;i<OPTPANEL_Y+OPTPANEL_H;i++)
  memset(ylookup[i]+OPTPANEL_X,1,OPTPANEL_W);
 MenuDrawOptionRow(105,"RENDERER",SC.hdmode ? "HD" : "ORIGINAL");
 MenuDrawOptionRow(116,"FULLSCREEN",SC.fullscreen ? "ON" : "OFF");
 /* "HUD DENSITY", not "SCREEN SIZE": the setting behind it is still the engine's
    view size (SC.screensize / currentViewSize) and is spelled that way in the
    code, but sizes 0..3 leave the view full screen and differ only in how much
    of the HUD is drawn, which is what the player is actually choosing here. */
 MenuDrawOptionRow(127,"HUD DENSITY","");

 switch (menucursor)
  {
   case 2: // music vol
    VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menumussli")));
    ShowMenuSliders(SC.musicvol,256);
    break;
   case 3: // sound vol
    VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menusousli")));
    ShowMenuSliders(SC.sfxvol,256);
    break;
   case 4: // violence
    if (SC.violence)
     VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuvioon")));
    else
     VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuviooff")));
    break;
   case 5: // animation
    if (SC.animation)
     VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuanion")));
    else
     VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuanioff")));
    break;
   case 6: // ambient light
    VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuambsli")));
    ShowMenuSliders(SC.ambientlight,4096);
    break;
   case 7: // renderer
    MenuShowOptionValue(SC.hdmode ? "HD" : "ORIGINAL");
    break;
   case 8: // fullscreen
    MenuShowOptionValue(SC.fullscreen ? "ON" : "OFF");
    break;
   /* HUD density, on the row the camera delay used to have.  The delay is
      pinned to its minimum now (see InitSound), and this is the setting that
      actually needed to come back: it is the engine's view size, and with no
      row for it the game was stuck at size 0 -- which draws no HUD at all.
      "menuscrsli" is the original artwork for this slider, still in GREED.BLO
      from when the row existed.

      Not drawn inverted the way the DOS screen-size slider was: read as HUD
      density a full bar is the full HUD, which also puts left and right the
      same way round as the volume rows above.  The range is MaxViewSize()
      rather than a fixed 9 because HD stops at 3. */
   case 9: // HUD density (view size)
    VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuscrsli")));
    /* That bitmap has "SCREEN" baked into its title strip, between the 0 and
       10 range markers.  Paint the word out and print the row's real name in
       its place -- 45..85 by rows 32..39, measured off the drawn pic. */
    for(i=32;i<40;i++)
     memset(ylookup[i]+45,0,41);
    fontbasecolor=178;      /* lands the glyph ramp in the artwork's magenta */
    font=font1;
    printx=45+(41-FN_RawWidth("HUD"))/2;
    printy=33;
    FN_RawPrint3("HUD");
    ShowMenuSliders(SC.screensize,MaxViewSize());
    break;
   }
 MenuDrawCursorBox();
 MouseShow();
 }


void MenuLeft(void)
{
 if (menulevel==4)
  {
   MouseHide();
   switch (menucursor)
    {
     case 2:
      if (SC.musicvol)
       {
	SC.musicvol-=4;
	if (SC.musicvol<0) SC.musicvol=0;
	SetVolumes(SC.musicvol,SC.sfxvol);
	ShowMenuSliders(SC.musicvol,255);
	}
      break;
     case 3:
      if (SC.sfxvol)
       {
	SC.sfxvol-=4;
	if (SC.sfxvol<0) SC.sfxvol=0;
	SetVolumes(SC.musicvol,SC.sfxvol);
	ShowMenuSliders(SC.sfxvol,255);
	}
      break;
     case 4: // violence
      SC.violence=true;
      MenuShowOptions();
      break;
     case 5: // animation
      SC.animation=true;
      MenuShowOptions();
      break;
     case 6: // ambient
      if (SC.ambientlight)
       {
	SC.ambientlight-=64;
	if (SC.ambientlight<0) SC.ambientlight=0;
	ShowMenuSliders(SC.ambientlight,4096);
	changelight=SC.ambientlight;
	lighting=1;
	}
      break;
     case 7: // renderer
      SC.hdmode=0;
      MenuShowOptions();
      break;
     case 8: // fullscreen
      SC.fullscreen=1;
      VI_SetFullscreen(SC.fullscreen);
      MenuShowOptions();
      break;
     case 9: // HUD density
      if (SC.screensize)
       {
	SC.screensize--;
	ShowMenuSliders(SC.screensize,MaxViewSize());
	}
      break;
     }
   MouseShow();
   }
 }


void MenuRight(void)
{
 if (menulevel==4)
  {
   MouseHide();
   switch (menucursor)
    {
     case 2:
      if (SC.musicvol<255)
       {
	SC.musicvol+=4;
	if (SC.musicvol>255) SC.musicvol=255;
	SetVolumes(SC.musicvol,SC.sfxvol);
	ShowMenuSliders(SC.musicvol,255);
	}
      break;
     case 3:
      if (SC.sfxvol<255)
       {
	SC.sfxvol+=4;
	if (SC.sfxvol>255) SC.sfxvol=255;
	SetVolumes(SC.musicvol,SC.sfxvol);
	ShowMenuSliders(SC.sfxvol,255);
	}
      break;
     case 4: // violence
      SC.violence=false;
      MenuShowOptions();
      break;
     case 5: // animation
      SC.animation=false;
      MenuShowOptions();
      break;
     case 6: // ambient
      if (SC.ambientlight<4096)
       {
	SC.ambientlight+=64;
	if (SC.ambientlight>4096) SC.ambientlight=4096;
	ShowMenuSliders(SC.ambientlight,4096);
	changelight=SC.ambientlight;
	lighting=1;
	}
      break;
     case 7: // renderer
      SC.hdmode=1;
      MenuShowOptions();
      break;
     case 8: // fullscreen
      SC.fullscreen=0;
      VI_SetFullscreen(SC.fullscreen);
      MenuShowOptions();
      break;
     case 9: // HUD density
      if (SC.screensize<MaxViewSize())
       {
	SC.screensize++;
	ShowMenuSliders(SC.screensize,MaxViewSize());
	}
      break;
     }
   MouseShow();
   }
 }


/* Enter and Esc act once per press.  KBDELAY2 alone only throttles the repeat
   to 5 ticks (~143ms), so holding Enter to pick a character executed a second
   time on the difficulty menu that press had just opened, and the game started
   at whatever difficulty the cursor defaulted to.  The arrow keys below keep
   repeating on purpose. */
static bool enterheld, escheld;

void MenuCommand(void)
{

 if (!keyboard[SC_ESCAPE]) escheld=false;
 else if (!escheld && timecount>timedelay)
  {
   escheld=true;
   downlevel=true;
   timedelay=timecount+KBDELAY2;
   }

 if (keyboard[SC_UPARROW] && timecount>timedelay)
  {
   --menucursor;
   if (menucursor<0) menucursor=menumax[menulevel]-1;
   timedelay=timecount+KBDELAY2;
   }
 else if (keyboard[SC_DOWNARROW] && timecount>timedelay)
  {
   ++menucursor;
   if (menucursor==menumax[menulevel]) menucursor=0;
   timedelay=timecount+KBDELAY2;
   }

 if (keyboard[SC_RIGHTARROW] && timecount>timedelay) goright=true;
  else if (keyboard[SC_LEFTARROW] && timecount>timedelay) goleft=true;

 if (!keyboard[SC_ENTER]) enterheld=false;
 else if (!enterheld && timecount>timedelay)
  {
   enterheld=true;
   menuexecute=true;
   timedelay=timecount+KBDELAY2;
   }
 }
void MenuStub(void)
{
 }


void MenuShowCursor(int menucursor)
{
 int x, y, w, h, i;

 if (menucursor==-1 || menucursor==menucurloc) return;
 MouseHide();
 VI_DrawMaskedPic(20,15,CA_CacheLump(CA_GetNamedNum("menumain")+menulevel));
 menucurloc=menucursor;
 /* Contents first, highlight second.  MenuShowOptions repaints the bottom of
    the options panel to redraw the three rows that have no baked artwork, and
    with the old order that repaint landed on top of the highlight rectangle
    and erased it for exactly those rows. */
 if (menulevel==2 || menulevel==3) ShowSaveDir();
 if (menulevel==4) MenuShowOptions();
 MenuDrawCursorBox();
 MouseShow();
 }


void ShowMenuLevel(int level)
{
 if (menulevel==0)
  menumaincursor=menucursor;
 menulevel=level;
 MouseHide();
 VI_DrawMaskedPic(20,15,CA_CacheLump(CA_GetNamedNum("menumain")+level));
 MouseShow();
 if (menulevel==0)
  menucursor=menumaincursor;
 else if (menulevel==1)
  menucursor=1;
 else if (menulevel==2 || menulevel==3)
  {
   if (saveposition>0)
    menucursor=saveposition;
   else
    menucursor=1;
   }
 else
  menucursor=2;
 menucurloc=-1;
 MenuShowCursor(menucursor);
 }


void GetSavedName(int menucursor)
{
 bool done;
 int     cursor, i;

 MouseHide();
 cursor=20;
 while (savedir[menucursor][cursor-1]==' ' && cursor>0) --cursor;
 if (cursor==20) cursor=19;
 savedir[menucursor][cursor]='_';
 done=false;
 INT_TimerHook(NULL);
 lastascii=0;
 newascii=false;
 while (!done)
  {
   printx=148;
   printy=34+menucursor*10;
   for(i=0;i<6;i++)
    memset(ylookup[printy+i]+printx,0,100);
   FN_Printf(savedir[menucursor]);
   while (!newascii && !quitgame)   // wait for a new key
    {
     Sys_PumpFrame();   /* newascii is set from the event pump */
     MenuShowCursor(menucursor+1);
     }
   if (quitgame) return;
   switch (lastascii)
    {
     case 27:
      done=true;
      break;
     case 13:
      done=true;
      break;
     case 8:
      if (cursor>0)
       {
	savedir[menucursor][cursor-1]='_';
	memset(&savedir[menucursor][cursor],(int)' ',20-cursor);
	--cursor;
	}
      break;
     default:
      if (isalnum(lastascii) || lastascii==' ' || lastascii=='.' ||
       lastascii=='-' || lastascii=='_' || lastascii=='!' || lastascii==',' ||
       lastascii=='?' || lastascii=='"')
       {
	savedir[menucursor][cursor]=lastascii;
	if (cursor<19) ++cursor;
	savedir[menucursor][cursor]='_';
	break;
	}
     }
   newascii=false;
   }
 savedir[menucursor][cursor]=' ';
 if (lastascii==27) ShowSaveDir();
 else
  {
   downlevel=true;
   SaveDirectory();
   SaveGame(menucursor);
   }
 timedelay=timecount+KBDELAY2;
 INT_TimerHook(MenuCommand);
 MouseShow();
 }


void Execute(int level,int cursor)
{
 switch (level)
  {
   case 0: // main menu
    switch (cursor)
     {
      case 0: // new game
       if (!netmode) ShowMenuLevel(1);
       break;
      case 1: // quit
       if (ShowQuit(MenuCommand))
	{
	 quitgame=true;
	 quitmenu=true;
	 }
       break;
      case 2: // load
       if (!netmode) ShowMenuLevel(2);
       break;
      case 3: // save
       if (!netmode && gameloaded) ShowMenuLevel(3);
       break;
      case 4: // volume menu
       ShowMenuLevel(4);
       break;
      case 5: // info
       INT_TimerHook(NULL);
       MouseHide();
       ShowHelp();
       MouseShow();
       INT_TimerHook(MenuCommand);
       break;
      case 6: // resume
       quitmenu=true;
       break;
      }
    break;
   case 1: // char selection
    switch (cursor)
     {
      case 0: // quit
       if (ShowQuit(MenuCommand))
	{
	 quitgame=true;
	 quitmenu=true;
	 }
       break;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
       identity=cursor-1;
       ShowMenuLevel(5);
       break;
      case 6: // resume
       downlevel=true;
       break;
      }
    break;
   case 2: // load menu
    switch (cursor)
     {
      case 0: // quit
       if (ShowQuit(MenuCommand))
	{
	 quitgame=true;
	 quitmenu=true;
	 }
       break;
      case 11: // back
       downlevel=true;
       break;
      default:
       MouseHide();
       LoadGame(menucursor-1);
       quitmenu=true;
       MouseShow();
       saveposition=cursor;
       break;
      }
    break;
   case 3: // save menu
    switch (cursor)
     {
      case 0: // quit
       if (ShowQuit(MenuCommand))
	{
	 quitgame=true;
	 quitmenu=true;
	 }
       break;
      case 11: // back
       downlevel=true;
       break;
      default:
       GetSavedName(menucursor-1);
       saveposition=cursor;
       break;
      }
    break;
   case 4: // option menu
    switch (cursor)
     {
      case 0:
       ShowMenuLevel(1);
       break;
      case 1:
       if (ShowQuit(MenuCommand))
	{
	 quitgame=true;
	 quitmenu=true;
	 }
       break;
      case 2: // music vol
      case 3: // sound vol
      case 4: // violence
      case 5: // animations
      case 6: // ambient light
      case 9: // HUD density
       break;
      /* The rows above are driven by the left/right arrows drawn into their
	 widget bitmaps.  These two have no bitmap and so no arrows to click,
	 so Enter -- and a click on the row -- toggles them instead. */
      case 7: // renderer
       SC.hdmode=!SC.hdmode;
       MenuShowOptions();
       break;
      case 8: // fullscreen
       SC.fullscreen=!SC.fullscreen;
       VI_SetFullscreen(SC.fullscreen);
       MenuShowOptions();
       break;
      case 10:
       downlevel=true;
       break;
      }
    break;
   case 5: // difficulty selection
    switch (cursor)
     {
      case 0: // quit
       if (ShowQuit(MenuCommand))
	{
	 quitgame=true;
	 quitmenu=true;
	 }
       break;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
       timecount=0;
       frames=0;
       MouseHide();
#ifdef GAME1
       newplayer(0,identity,6-cursor);
#elif defined(GAME2)
       newplayer(8,identity,6-cursor);
#elif defined(GAME3)
       newplayer(16,identity,6-cursor);
#else
       newplayer(0,identity,6-cursor);
#endif
       MouseShow();
       quitmenu=true;
       break;
      case 7:
       ShowMenuLevel(1);
       break;
      }
    break;
   }
 }


void MenuAnimate(void)
{
 int     lump;
 pic_t   *frames[8];
 int     i, frame;
 longint waittime;

 if (netmode) return;
 memcpy(viewbuffer,screen,64000);
 lump=CA_GetNamedNum("menuanim");
 for(i=0;i<8;i++)
  frames[i]=CA_CacheLump(lump+i);
 frame=-1;
 waittime=timecount;
 while (1)
  {
   Sys_PumpFrame();     /* timecount only advances from the tick */
   if (quitgame) break;
   if (timecount>=waittime)
    {
     ++frame;
     if (frame==8) break;
     VI_DrawMaskedPic2(20,15,frames[frame]);
     waittime+=7;
     }
   }
 for(i=0;i<8;i++)
  CA_FreeLump(lump+i);
 }


void CheckMouse(void)
{
 int      i, clicked;
 short    x, y;
 cursor_t *c;

 clicked=MouseGetClick(&x,&y);
 if (clicked)
  for(i=0;i<menumax[menulevel];i++)
   {
    c=&cursors[menulevel][i];
    if (x<c->x+c->w && x>c->x &&
	y<c->y+c->h && y>c->y)
     {
      menucursor=i;
      MenuShowCursor(menucursor);
      menuexecute=true;
      return;
      }
    }

 /* The sliders are dragged, so they need the held button state MouseGetClick
    no longer reports.  The arrows below stay on the press, or one click on
    them toggles the setting every frame. */
 if (menulevel==4 && (clicked || MouseGetDrag(&x,&y)))
  {
   switch (menucursor)
    {
     case 2:
      if (y>=49 && y<=64 && x>=42 && x<=90)
       {
	SC.musicvol=((x-40)*256)/49;
	if (SC.musicvol>255) SC.musicvol=255;
	SetVolumes(SC.musicvol,SC.sfxvol);
	ShowMenuSliders(SC.musicvol,255);
	}
      break;
     case 3:
      if (y>=49 && y<=64 && x>=42 && x<=90)
       {
	SC.sfxvol=((x-40)*256)/49;
	if (SC.sfxvol>255) SC.sfxvol=255;
	SetVolumes(SC.musicvol,SC.sfxvol);
	ShowMenuSliders(SC.sfxvol,255);
	}
      break;
     case 4:
     case 5:
      if (clicked && y>=62 && y<=70)
       {
	if (x>=50 && x<=61) goleft=true;
	else if (x>=72 && x<=83) goright=true;
	}
      break;
     case 6:
      if (y>=49 && y<=64 && x>=42 && x<=90)
       {
	SC.ambientlight=((x-40)*4096)/49;
	if (SC.ambientlight>4096) SC.ambientlight=4096;
	ShowMenuSliders(SC.ambientlight,4096);
	changelight=SC.ambientlight;
	lighting=1;
	}
      break;
     /* 7 and 8 have no slider and no arrows; clicking the row runs Execute,
	which toggles them. */
     case 9: // HUD density
      if (y>=49 && y<=64 && x>=42 && x<=90)
       {
	SC.screensize=((x-40)*(MaxViewSize()+1))/49;
	if (SC.screensize<0) SC.screensize=0;
	else if (SC.screensize>MaxViewSize()) SC.screensize=MaxViewSize();
	ShowMenuSliders(SC.screensize,MaxViewSize());
	}
      break;
     }
   }
 }


void ShowMenu(int n)
{
 byte *scr;

 timedelay=timecount+KBDELAY2;
 /* Assume held until seen released, so the keypress that opened the menu
    cannot also pick something inside it. */
 enterheld=true;
 escheld=true;
 INT_TimerHook(MenuCommand);

 scr=(byte *)malloc(64000);
 if (scr==NULL) MS_Error("ShowMenu: Out of Memory!");
 memcpy(scr,screen,64000);
 if (SC.animation) MenuAnimate();
 MouseShow();
 ShowMenuLevel(n);
 quitmenu=false;
 do
  {
   MenuShowCursor(menucursor);
   CheckMouse();
   if (menuexecute)
    {
     Execute(menulevel,menucursor);
     menuexecute=false;
     }
   if (downlevel)
    {
     if (menulevel==0) quitmenu=true;
      else ShowMenuLevel(0);
     downlevel=false;
     }
   if (goright)
    {
     MenuRight();
     goright=false;
     }
   if (goleft)
    {
     MenuLeft();
     goleft=false;
     }
   /* quitmenu, menuexecute, downlevel, goright and goleft are all set by
      MenuCommand, which runs only as the timer hook.  Without this the loop
      never sees any of them change and the game locks up on Esc. */
   Sys_PumpFrame();
   if (netmode) TimeUpdate();
   } while (!quitmenu && !quitgame);
 MouseHide();
 memcpy(screen,scr,64000);
 free(scr);
 if (gameloaded)
  {
   if (SC.vrhelmet==0)
    {
     while (currentViewSize<SC.screensize)
      ChangeViewSize(true);
     while (currentViewSize>SC.screensize)
      ChangeViewSize(false);
     }
   }
 /* Applied on the way out rather than live, the way the view size always has
    been: it rebuilds the projection tables and both textures.  Re-applied
    whenever HD is active, not only on a change, because the fullscreen row
    above may just have altered the window the HD resolution is derived from. */
 if (SC.hdmode!=hdmode || hdmode)
  VI_ApplyRenderMode();
 SaveSetup(&SC,"SETUP.CFG");
 turnrate=0;
 moverate=0;
 fallrate=0;
 strafrate=0;
 ResetMouse();
 }

/**************************************************************************/

/* The help page.

   It used to be the `INFO1` lump: one baked 320x200 image with every key name
   painted into it.  That was already wrong on arrival here -- this port moved
   movement to WASD, use to E, jump to Space and so on -- and the picture told
   the player to "RUN SETUP IN DOS TO ALTER THE ABOVE KEYS", which there is no
   longer any such thing as.  Drawing the page instead means the bindable rows
   read out of scanbuttons[], so they cannot go stale again.

   The backdrop is `BRIEF3`, the weapons still life from the first mission
   briefing.  That is the same artwork INFO1 had dimmed behind its text, which
   is why it looks right: this dims its palette rather than its pixels, to the
   same end.

   Colours are indices into BRIEF3's OWN palette, not the game's -- loadscreen
   brings a screen's palette with it, and BRIEF3's differs from the main one in
   748 of its 768 bytes.  The picture leaves 3..31 unused (measured, not
   assumed), so three seven-entry ramps are built there for the text.  The font
   adds a 1..6 gradient to fontbasecolor, hence the seven. */

#define HELPDIM       24          /* percent of full brightness for the art --
                                     BRIEF3 is lit for a full screen of its own
                                     and swallows text at anything brighter */
#define HELPKEYCOL     3          /* amber: the key itself */
#define HELPTEXTCOL   11          /* white: what it does */
#define HELPHEADCOL   19          /* green: section headings */

#define HELPLEFT       8
#define HELPRIGHT    166
#define HELPACTION    50          /* action column, relative to the key */
#define HELPROW        8          /* font1 is 6 tall, so this leaves 2 clear */


static char *HelpKeyName(int sc)
/* Printable name for a scan code.  ASCIINames covers everything that produces
   a character; the rest have to be spelled out. */
{
 static char one[2];
 char        c;

 switch (sc)
  {
   case SC_CONTROL:    return "CTRL";
   case SC_ALT:        return "ALT";
   case SC_LSHIFT:     return "SHIFT";
   case SC_RSHIFT:     return "R SHIFT";
   case SC_SPACE:      return "SPACE";
   case SC_ENTER:      return "ENTER";
   case SC_ESCAPE:     return "ESC";
   case SC_BACKSPACE:  return "BKSP";
   case SC_TAB:        return "TAB";
   case SC_CAPSLOCK:   return "CAPS LOCK";
   case SC_NUMLOCK:    return "NUM LOCK";
   case SC_UPARROW:    return "UP ARROW";
   case SC_DOWNARROW:  return "DN ARROW";
   case SC_LEFTARROW:  return "LF ARROW";
   case SC_RIGHTARROW: return "RT ARROW";
   case SC_INSERT:     return "INSERT";
   case SC_DELETE:     return "DELETE";
   case SC_HOME:       return "HOME";
   case SC_END:        return "END";
   case SC_PGUP:       return "PAGE UP";
   case SC_PGDN:       return "PAGE DN";
   case SC_MINUS:      return "-";
   case SC_PLUS:       return "+";
   }
 if (sc>0 && sc<NUMCODES && ASCIINames[sc]>' ' && ASCIINames[sc]<127)
  {
   c=(char)ASCIINames[sc];
   if (c>='a' && c<='z') c-='a'-'A';    /* the fonts are upper case only */
   one[0]=c;
   one[1]=0;
   return one;
   }
 return "--";
 }


static void HelpRow(int x,int y,char *key,char *action)
{
 font=font1;
 fontbasecolor=HELPKEYCOL;
 printx=x;
 printy=y;
 FN_RawPrint3(key);
 fontbasecolor=HELPTEXTCOL;
 printx=x+HELPACTION;
 printy=y;
 FN_RawPrint3(action);
 }


static void HelpHeading(int x,int y,char *s)
{
 font=font1;
 fontbasecolor=HELPHEADCOL;
 printx=x;
 printy=y;
 FN_RawPrint3(s);
 }


static void HelpRamp(int base,int r1,int g1,int b1,int r2,int g2,int b2)
/* Seven entries from (r1,g1,b1) to (r2,g2,b2), in VGA 0..63 levels. */
{
 int i;

 for (i=0;i<=6;i++)
  {
   colors[(base+i)*3+0]=(byte)(r1+((r2-r1)*i)/6);
   colors[(base+i)*3+1]=(byte)(g1+((g2-g1)*i)/6);
   colors[(base+i)*3+2]=(byte)(b1+((b2-b1)*i)/6);
   }
 }


static void DrawHelpPage(void)
{
 int i, y;

 loadscreen("BRIEF3");

 /* Dim the artwork, then cut the text ramps back in at full strength.  Order
    matters: the dim would take the ramps down with everything else. */
 for (i=0;i<768;i++)
  colors[i]=(byte)(((int)colors[i]*HELPDIM)/100);
 HelpRamp(HELPKEYCOL,  26,16, 0, 63,48,10);      /* amber */
 HelpRamp(HELPTEXTCOL, 24,24,24, 63,63,63);      /* white */
 HelpRamp(HELPHEADCOL,  0,24, 0, 24,63,24);      /* green */

 /* BRIEF3 carries a baked "PRESS SPACE BAR TO CONTINUE" across the bottom,
    which is not what this screen wants said. */
 for (i=182;i<200;i++)
  memset(ylookup[i],0,320);

 font=font1;
 fontbasecolor=HELPKEYCOL;
 printy=6;
 FN_PrintCentered("GREED COMMAND KEYS");

 y=22;
 HelpHeading(HELPLEFT,y,"MOVEMENT");
 y+=10;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_north]),      "MOVE FORWARD"); y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_south]),      "MOVE BACK");    y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_west]),       "TURN LEFT");    y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_east]),       "TURN RIGHT");   y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_slideleft]),  "STRAFE LEFT");  y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_slideright]), "STRAFE RIGHT"); y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_straf]),      "TURN = STRAFE");y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_run]),        "RUN");          y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_jump]),       "JUMP");         y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_lookup]),     "LOOK UP");      y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_lookdown]),   "LOOK DOWN");    y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_centerview]), "CENTER VIEW");  y+=HELPROW;

 y+=3;
 HelpHeading(HELPLEFT,y,"ACTION");
 y+=10;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_fire]),       "FIRE");         y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_use]),        "USE / OPEN");   y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_useitem]),    "USE ITEM");     y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_invleft]),    "PREV ITEM");    y+=HELPROW;
 HelpRow(HELPLEFT,y,HelpKeyName(scanbuttons[bt_invright]),   "NEXT ITEM");

 /* The right hand column is the keys PlayerCommand tests directly rather than
    through scanbuttons[], so these are spelled from the same SC_ constants it
    uses -- change one and this goes with it. */
 y=22;
 HelpHeading(HELPRIGHT,y,"DISPLAY");
 y+=10;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_M),                      "MAP MODES");    y+=HELPROW;
 HelpRow(HELPRIGHT,y,"+ -",                                  "MAP ZOOM");     y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_H),                      "HEAT SENSOR");  y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_C),                      "MOTION SENSOR");y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(scanbuttons[bt_asscam]),    "A.S.S. CAM");   y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_TAB),                    "TARGET LIST");  y+=HELPROW;
 HelpRow(HELPRIGHT,y,"F9 F10",                               "HUD DENSITY");

 y+=11;
 HelpHeading(HELPRIGHT,y,"GAME");
 y+=10;
 HelpRow(HELPRIGHT,y,"1 - 5",                                "WEAPONS");      y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_CAPSLOCK),               "AUTO-RUN");     y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_NUMLOCK),                "AUTO-TARGET");  y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_P),                      "PAUSE");        y+=HELPROW;
 HelpRow(HELPRIGHT,y,HelpKeyName(SC_ESCAPE),                 "MENU");         y+=HELPROW;
 HelpRow(HELPRIGHT,y,"F1",                                   "HELP");         y+=HELPROW;
 HelpRow(HELPRIGHT,y,"F5",                                   "BRIEFING");     y+=HELPROW;
 HelpRow(HELPRIGHT,y,"F6",                                   "SEND MESSAGE");

 font=font1;
 fontbasecolor=HELPKEYCOL;
 printy=188;
 FN_PrintCentered("PRESS ANY KEY TO CONTINUE");
 }


void ShowHelp(void)
{
 byte *s;
#ifdef ASSASSINATOR
 FILE *f;
#endif

 s=(byte *)malloc(64000);
 if (s==NULL) MS_Error("Error Allocating in ShowHelp");
 memcpy(s,screen,64000);
 VI_FillPalette(0,0,0);
 memset(screen,0,64000);

#ifdef ASSASSINATOR
 f=fopen("help.dat","rb");
 if (f==NULL)
  MS_Error("Error Loading Help.Dat file");
 fread(screen,64000,1,f);
 fread(colors,768,1,f);
 fclose(f);
#else
 DrawHelpPage();
#endif
 VI_SetPalette(colors);
 newascii=false;
 for(;;)
  {
   /* Sys_PumpFrame, not the bare Wait(10) this was: `screen` was the visible
      framebuffer in DOS, so nothing here ever had to present.  Now the page is
      shown only by the single VI_BlitView inside VI_SetPalette above, and
      anything that disturbs the window while help is up leaves it unrepainted.
      Every other screen that sits waiting for a key pumps for the same reason. */
   Sys_PumpFrame();
   if (netmode) TimeUpdate();
   if (newascii || quitgame) break;
   }
 VI_FillPalette(0,0,0);
 memset(screen,0,64000);

#ifdef DEMO
 loadscreen("INFO2");
 VI_SetPalette(colors);
 newascii=false;
 for(;;)
  {
   Wait(10);
   if (netmode) TimeUpdate();
   if (newascii || quitgame) break;
   }
 VI_FillPalette(0,0,0);
 memset(screen,0,64000);

 loadscreen("INFO3");
 VI_SetPalette(colors);
 newascii=false;
 for(;;)
  {
   Wait(10);
   if (netmode) TimeUpdate();
   if (newascii || quitgame) break;
   }
 memset(screen,0,64000);
 VI_FillPalette(0,0,0);
#endif

 /* Put the game palette back in colors[] and not just on the screen.  It is a
    global that VI_FadeIn and the briefing fades read, and DrawHelpPage has left
    a dimmed copy of BRIEF3's in it.  This did not matter when the page was
    INFO1, whose palette is the game's byte for byte. */
 memcpy(colors,CA_CacheLump(CA_GetNamedNum("palette")),768);
 VI_SetPalette(colors);
 memcpy(screen,s,64000);
 free(s);
 }

/**************************************************************************/

bool CheckPause()
{
 if (netmode)
  {
   NetGetData();
   if (!gamepause) return !netpaused;
    else return newascii;
   }
 return newascii;
 }


void ShowPause(void)
{
 longint animtime, droptime;
 int     anim, y, i;
 int     lump;
 pic_t   *pics[4];

 INT_TimerHook(NULL);
 memcpy(viewbuffer,screen,64000);
 lump=CA_GetNamedNum("pause");
 for(i=0;i<4;i++) pics[i]=CA_CacheLump(lump+i);
 timedelay=timecount+KBDELAY2;
 Wait(KBDELAY2);
 anim=0;
 if (!SC.animation) y=72;
  else y=-56;
 droptime=timecount;
 animtime=timecount;
 newascii=false;
 while (!CheckPause() && !quitgame)
  {
   Sys_PumpFrame();     /* CheckPause reads keyboard[]; timecount ticks here */
   if (timecount>=droptime && y<72)
    {
     if (y>=0) memcpy(ylookup[y],viewbuffer+320*y,640);
     y+=2;
     droptime=timecount+1;
     }
   if (timecount>=animtime)
    {
     anim++;
     anim&=3;
     animtime+=10;
     }
   VI_DrawMaskedPic2(106,y,pics[anim]);
   }
 if (!SC.animation) y=200;
 droptime=timecount;
 animtime=timecount;
 while (y<199)
  {
   Sys_PumpFrame();     /* y only advances when timecount does */
   if (quitgame) break;
   if (timecount>=droptime)
    {
     if (y>=0) memcpy(ylookup[y],viewbuffer+320*y,640);
     y+=2;
     droptime=timecount+1;
     }
   if (timecount>=animtime)
    {
     anim++;
     anim&=3;
     animtime+=10;
     }
   VI_DrawMaskedPic2(106,y,pics[anim]);
   }
 memcpy(screen,viewbuffer,64000);
 for (i=0;i<4;i++)
  CA_FreeLump(lump+i);
 }

/*****************************************************************************/

void StartWait(void)
{
 int i, lump;

 memcpy(viewbuffer,screen,64000);
 lump=CA_GetNamedNum("wait");
 for(i=0;i<4;i++) waitpics[i]=CA_CacheLump(lump+i);
 waitanim=0;
 VI_DrawMaskedPic2(106,72,waitpics[0]);
 timedelay=timecount+10;
 waiting=true;
 }


void UpdateWait(void)
{
 if (timecount>timedelay)
  {
   ++waitanim;
   waitanim&=3;
   VI_DrawMaskedPic2(106,72,waitpics[waitanim]);
   timedelay=timecount+10;
   }
 }


void EndWait(void)
{
 int lump, i;

 lump=CA_GetNamedNum("wait");
 for(i=0;i<4;i++) CA_FreeLump(lump+i);
 memcpy(screen,viewbuffer,64000);
 waiting=false;
 }