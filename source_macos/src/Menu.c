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
   134, 101, 127, 15,
   134, 115, 127, 15,
   142, 142, 62, 15,
   0, 0, 0, 0,
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
 { 7, 7, 12, 12, 10, 8 };


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


void MenuShowOptions(void)
{
 MouseHide();
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
   case 7: // screen size
    VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menuscrsli")));
    ShowMenuSliders(10-SC.screensize,10);
    break;
   case 8: // asscam
    VI_DrawPic(35,29,CA_CacheLump(CA_GetNamedNum("menucamsli")));
    ShowMenuSliders(SC.camdelay,70);
    break;
   }
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
     case 7: // screensize
      if (SC.screensize<9)
       {
	SC.screensize++;
	ShowMenuSliders(10-SC.screensize,10);
	timedelay=timecount+KBDELAY2;
	goleft=false;
	}
      break;
     case 8: // camera delay
      if (SC.camdelay)
       {
	SC.camdelay--;
	ShowMenuSliders(SC.camdelay,70);
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
     case 7: // screensize
      if (SC.screensize)
       {
	SC.screensize--;
	ShowMenuSliders(10-SC.screensize,10);
	timedelay=timecount+KBDELAY2;
	goright=false;
	}
      break;
     case 8: // camera delay
      if (SC.camdelay<70)
       {
	SC.camdelay++;
	ShowMenuSliders(SC.camdelay,70);
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
 x=cursors[menulevel][menucurloc].x;
 y=cursors[menulevel][menucurloc].y;
 w=cursors[menulevel][menucurloc].w;
 h=cursors[menulevel][menucurloc].h;
 memset(ylookup[y]+x,133,w);
 memset(ylookup[y+h-1]+x,133,w);
 for(i=y;i<y+h;i++)
  {
   *(ylookup[i]+x)=133;
   *(ylookup[i]+x+w-1)=133;
   }
 MouseShow();
 if (menulevel==2 || menulevel==3) ShowSaveDir();
 if (menulevel==4) MenuShowOptions();

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
   while (!newascii)   // wait for a new key
    {
     Sys_PumpFrame();   /* newascii is set from the event pump */
     MenuShowCursor(menucursor+1);
     }
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
      case 7: // screen size
      case 8: // camera delay
       break;
      case 9:
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
     case 7:
      if (y>=49 && y<=64 && x>=42 && x<=90)
       {
	SC.screensize=9-(((x-40)*10)/49);
	if (SC.screensize>9) SC.screensize=9;
	else if (SC.screensize<0) SC.screensize=0;
	ShowMenuSliders(10-SC.screensize,10);
	}
      break;
     case 8:
      if (y>=49 && y<=64 && x>=42 && x<=90)
       {
	SC.camdelay=((x-40)*70)/49;
	if (SC.camdelay>70) SC.camdelay=70;
	ShowMenuSliders(SC.camdelay,70);
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
   } while (!quitmenu);
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
 SaveSetup(&SC,"SETUP.CFG");
 turnrate=0;
 moverate=0;
 fallrate=0;
 strafrate=0;
 ResetMouse();
 }

/**************************************************************************/

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
 loadscreen("INFO1");
#endif
 VI_SetPalette(colors);
 newascii=false;
 for(;;)
  {
   Wait(10);
   if (netmode) TimeUpdate();
   if (newascii) break;
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
   if (newascii) break;
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
   if (newascii) break;
   }
 memset(screen,0,64000);
 VI_FillPalette(0,0,0);
#endif

 VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));
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
 while (!CheckPause())
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