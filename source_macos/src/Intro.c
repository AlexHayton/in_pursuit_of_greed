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
#include <CONIO.H>
#include <MALLOC.H>
#include <TIME.H>
#include <IO.H>
#include <TCHAR.H>
#include "d_global.h"
#include "d_disk.h"
#include "d_misc.h"
#include "d_video.h"
#include "d_ints.h"
#include "r_refdef.h"
#include "r_public.h"
#include "d_font.h"
#include "protos.h"

/**** CONSTANTS ****/

#define VERSION     "1.000.041"

char *charinfo[5][27]=  // character profiles
{"CYBORG",
".",
"NAME: TOBIAS LOCKE",
".",
"RACE: HOMO SAPIEN",
".",
"AGE: 27",
".",
"HEIGHT: 6'1''",
".",
"WEIGHT: 450 LBS",
"(INCLUDING POWERED LIMBS)",
".",
"BIO: CYBERNETICALLY ENHANCED HUMAN",
".",
"BACKGROUND: BORN INTO THE LOTHLOS",
"CASTE OF HUNTERS, TOBIAS WAS",
"REBUILT AT THE AGE OF 22 FOLLOWING",
"HIS INITIATION RITE INTO THE ELITE",
"LOTH MAL ESCH, OR \"SCAVENGER",
"BROOD.\"",
"     HE WAS SOON RECRUITED BY THE",
"GREEN QUARTER HUNT SQUAD AND",
"ASSIGNED TO THE SCAVENGER VESSEL,",
"RED HUNTER, FOR COMPETITION IN THE",
"GAME.",
".",

"LIZARD MAN",
".",
"NAME: XITH",
".",
"RACE: SAPIOSAURUS ROBUSTUS",
".",
"AGE: 43",
".",
"HEIGHT: 4'11''",
".",
"WEIGHT: 100 LBS",
".",
"BIO: ZOLLEESIAN LIZARD MAN",
".",
"BACKGROUND: XITH WAS BORN BETWEEN",
"LIZARD CLANS AND FROM BIRTH HAS",
"BEEN AN OUTCAST AMONG HIS TRIBES.",
"HE ENTERED THE GAME HOPING TO USE",
"THE LIGHTNING QUICK SPEED INHERENT",
"IN HIS RACE TO VINDICATE HIMSELF.",
"     XITH WAS TRADED TO GREEN",
"QUARTER AND SUBSEQUENTLY PLACED",
"ABOARD THE RED HUNTER.",
".",
".",
".",
".",

"MOOMAN",
".",
"NAME: ALDUS KADEN",
".",
"RACE: BRAHMAN ERECTUS",
".",
"AGE: 35",
".",
"HEIGHT: 7'0''",
".",
"WEIGHT: 375 LBS",
".",
"BIO: BORN ON ELTHO III",
".",
"BACKGROUND: THE BOVINARIAN OR",
"\"MOOMEN\" AS RIMWARD WORLDS CALL",
"THEM, HAVE BEEN FASCINATED BY THE",
"GAME FROM THE BEGINNING.  HAVING",
"PARTICIPATED IN THE INSURRECTION",
"AT ALPHA PRAM AND THEN IN THE",
"FAILED COUP AT SARTUS I, ALDUS",
"KADEN WENT INTO THE ONLY",
"PROFESSION LEFT TO HIM.",
"     THE GREEN QUARTER TOOK HIM",
"IMMEDIATELY AND AFTER ONLY THREE",
"HUNTS HE WAS TRANSFERED TO THE",
"RED HUNTER UNDER TOBIAS'S COMMAND.",

"MUTANT",
".",
"NAME: SPECIMEN 7",
".",
"RACE: HOMO DEGENEROUS",
".",
"AGE: ?",
".",
"HEIGHT: 5'10'' (WHEN STANDING)",
".",
"WEIGHT: 250 LBS",
".",
"BIO: HUMAN VARIANT ENGINEERED BY",
"JANEX CORP. GEN-TECH DIVISION.",
".",
"BACKGROUND: AN ERROR IN OXYGEN",
"FEEDS TO THE BIRTHING TANKS",
"RESULTED IN ABNORMAL INTELLIGENCE",
"LEVELS IN HIS STRAIN.  ESCAPING THE",
"STERILIZATION THAT FOLLOWED",
"SPECIMEN 7 IS THE ONLY REMAINING",
"MEMBER OF HIS RACE.",
"     HE JOINED GREEN QUARTER HUNT",
"SQUAD WHEN THEY REALIZED HIS",
"POTENTIAL AS A HUNTER.",
".",
".",

"DOMINATRIX",
".",
"NAME: THEOLA NOM",
".",
"RACE: HOMO MAJESTRIX",
".",
"AGE: 22",
".",
"HEIGHT: 5'11''",
".",
"WEIGHT: 140 LBS",
".",
"BIO: HUMAN VARIANT ENGINEERED BY",
"JANEX CORP. GEN-TECH DIVISION.",
".",
"BACKGROUND: INITIALLY ENGINEERED",
"BY JANEX FOR SALE AS A 'HOME",
"ENTERTAINMENT SYSTEM', THEOLA",
"FIRST USED HER PREVIOUSLY LATENT",
"PSYCHIC ABILITIES ON HER FIRST",
"OWNER.  THEY NEVER FOUND THE BODY.",
"     SINCE THEN, SHE HAS BEEN A",
"FUGITIVE FROM CORE LAW.  RECENTLY",
"SHE HAS SOUGHT REFUGE WITHIN THE",
"RED HUNTER HUNT SQUAD.",
".",
".",
};


/**** VARIABLES ****/

byte    colors[768];
bool nointro, nextchar;
extern  SoundCard SC;
extern  bool redo;
int     cdr_drivenum;


/**** FUNCTIONS ****/

void DoIntroMenu(void);

void loadscreen(char *s)
{
 byte *l;
 int  i;

 i=CA_GetNamedNum(s);
 l=CA_CacheLump(i);
 VI_DrawPic(0,0,(pic_t*)l);
 CA_FreeLump(i);
 l=CA_CacheLump(i+1);
 memcpy(colors,l,768);
 CA_FreeLump(i+1);
 }


bool CheckTime(int n1, int n2);

void Wait(longint time)
{
	longint t;

	t = timecount + time;
	/* Sys_Frame both pumps SDL events and advances timecount, so this is the
	   spin that actually lets the clock move.  The Win32 original pumped the
	   message queue here while a separate timer thread drove timecount.

	   It is also where quitgame gets set, by Cmd-Q or the window's close box.
	   Abandoning the delay there is what makes quitting prompt from the intro,
	   the briefings and the credits: nearly every pause in those is a Wait, and
	   sitting out a two second hold or a 64 step fade at every one of them adds
	   up to seconds before anybody looks at the flag.  Callers see the delay as
	   having elapsed, which is safe -- the fades set their final palette after
	   their loop, so none is left half applied. */
	while ( !CheckTime(timecount,t) )
	{
		Sys_Frame();
		if (quitgame) return;
	}
}


void ShowPortrait(int n)
{
 char str1[128];
 int  i;

 sprintf(str1,"CHAR%i",n);
 loadscreen(str1);
 VI_FadeIn(0,256,colors,48);
 font=font1;
 for(i=0;i<27;i++)
  {
   if (quitgame) return;
   UpdateSound();
   for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
    if (charinfo[n-1][i][0]!='.')
     {
      printx=144;
      printy=19+6*i;
      sprintf(str1,"%s",charinfo[n-1][i]);
      FN_RawPrint3(str1);
      Wait(2);
      }
   if (activatemenu)
    {
     DoIntroMenu();
     if (quitgame || gameloaded) return;
     }
   }
 for(i=0;i<50;i++)
  {
   if (quitgame) return;
   UpdateSound();
   Wait(35);
   if (activatemenu)
    {
     DoIntroMenu();
     if (quitgame || gameloaded) return;
     }
   if (nextchar)
    {
     nextchar=false;
     break;
     }
   }
 VI_FadeOut(0,256,0,0,0,48);
 }


void IntroCommand(void)
{
 if ((keyboard[SC_ESCAPE] || keyboard[SC_ENTER]) && timecount>keyboardDelay)
  {
   activatemenu=true;
   keyboardDelay=timecount+20;
   }
 if (keyboard[SC_SPACE] && timecount>keyboardDelay)
  {
   nextchar=true;
   keyboardDelay=timecount+20;
   }
 }

void DoIntroMenu(void)
{
 byte *temp, oldcolors[768];

 memcpy(oldcolors,colors,768);
 temp=(byte *)malloc(64000);
 if (temp==NULL) MS_Error("DoIntroMenu: No memory for temp screen");
 memcpy(temp,screen,64000);
 memset(screen,0,64000);
 VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));
 player.timecount=timecount;
 ShowMenu(0);
 if (!quitgame && !gameloaded)
  {
   memset(screen,0,64000);
   memcpy(colors,oldcolors,768);
   VI_SetPalette(colors);
   memcpy(screen,temp,64000);
   }
 free(temp);
 timecount=player.timecount;
 activatemenu=false;
// lock_region((void near *)IntroCommand,(char *)IntroStub - (char near *)IntroCommand);
 INT_TimerHook(IntroCommand);
 keyboardDelay=timecount+KBDELAY;
 }


bool CheckDemoExit(void)
{
 /* Cmd-Q and the close box set quitgame from inside the event pump.  Reporting
    it here covers every step of MainIntro at once -- it already asks after each
    screen and each movie. */
 if (quitgame) return true;
 if (activatemenu)
  {
   activatemenu=false;
   nextchar=false;
//   unlock_region((void near *)IntroCommand,(char *)IntroStub - (char near *)IntroCommand);
//   INT_TimerHook(NULL);
   return true;
   }
 return false;
 }


void DemoIntroFlis(char *path);


void MainIntro(void)
{
 char  path[64];

#ifdef CDROMGREEDDIR
 sprintf(path,"%c:\\GREED\\MOVIES\\",cdr_drivenum+'A');
#else
 sprintf(path,"%c:\\MOVIES\\",cdr_drivenum+'A');
#endif
 DemoIntroFlis(path);
 }


void DemoIntroFlis(char *path)
{
 char name[64];

 fontbasecolor=0;
 font=font1;

 sprintf(name,"%sTEXT.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sWARP01.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sWARP02.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sINSHIP01.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sARBITER.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sINSHIP02.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;
 Wait(140);

 sprintf(name,"%sCHAR1.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sCHAR2.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sCHAR3.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sCHAR4.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sCHAR5.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sINSHIP03.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sDROPPOD.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sSHP1.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sCITYBURN.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sRUBBLE.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sTHF1.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sTHF2.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sTHF3.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sTHF4.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sINSHIP04.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

 sprintf(name,"%sWARP05.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;

#ifndef ASSASSINATOR
 sprintf(name,"%sLOGOFLY.FLI",path);
 playfli(name,0);
 if (CheckDemoExit()) return;
#endif

 Wait(210);
 }


void DemoIntro(void)
{
 int   i;
 FILE *f;

 fontbasecolor=0;
 font=font1;

 f=fopen("MOVIES/TEXT.FLI","rb");
 if (f!=NULL)
  {
   fclose(f);
   DemoIntroFlis("MOVIES/");
   return;
   }

 VI_FillPalette(0,0,0);
 loadscreen("SOFTLOGO");
 VI_FadeIn(0,256,colors,48);
 Wait(210);
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;

 loadscreen("C7LOGO");
 VI_FadeIn(0,256,colors,48);
 Wait(210);
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;

 loadscreen("LOGO");
 VI_FadeIn(0,256,colors,48);
 Wait(210);
 VI_FillPalette(63,63,63);
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;

 loadscreen("INTRO00");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf(
    "IT IS THE YEAR 15392 DURING THE THIRD AGE OF MAN.\n"
    "SCAVENGER HUNTS MEAN BIG MONEY FOR THE CRIMINAL\n"
    "ELITE.  COVERTLY RECRUITED BY AN ENIGMATIC FACTION\n"
    "OF THE A.V.C. YOU ARE A MEMBER OF THE RED HUNTER\n"
    "ELITE ACQUISITION SQUAD.");
   Wait(5);
   }
 for(i=0;i<70;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;


 loadscreen("INTRO01");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf(
    "USING YOUR NEEDLING SHIP, THE RED HUNTER, IT IS YOUR\n"
    "JOB TO WARP THROUGH TO UNSUSPECTING FRINGE WORLDS...");
   Wait(5);
   }
 for(i=0;i<42;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;


 loadscreen("INTRO02");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf(
    "...PLUMMET FROM ORBIT IN YOUR DROP SHIP, SUPPRESSING\n"
    "NONCOOPERATIVE ENTITIES (NOPS) AS BEST YOU CAN IN\n"
    "ORDER TO ACQUIRE YOUR PRIMARY TARGET ITEMS.");
   Wait(5);
   }
 for(i=0;i<42;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;


 loadscreen("INTRO03");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf(
    "THE SIZE AND SPEED OF THE DROP SHIP SHOULD BE SUFFICIENT\n"
    "TO DRIVE NONCOOPERATIVES FROM THE DROP ZONE UPON IMPACT.");
   Wait(5);
   }
 for(i=0;i<42;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FillPalette(0,0,0);
 if (CheckDemoExit()) return;


 loadscreen("INTRO04");
 VI_SetPalette(colors);
 for(i=0;i<21;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FadeOut(0,256,64,64,64,48);
 if (CheckDemoExit()) return;


 loadscreen("INTRO05");
 VI_FadeIn(0,256,colors,48);
 for(i=0;i<21;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;


 loadscreen("INTRO06");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf("IN SHORT, IF IT'S MOVING AROUND, KILL IT...");
   Wait(5);
   }
 for(i=0;i<28;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;


 loadscreen("INTRO07");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf("...AND IF IT'S NOT NAILED DOWN, STEAL IT.");
   Wait(5);
   }
 for(i=0;i<28;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;

 loadscreen("INTRO08");
 VI_FadeIn(0,256,colors,48);
 for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
  {
   printy=160;
   FN_CenterPrintf(
    "YOU'RE A SCAVENGER.  YOU'VE GOT A GUN.\n"
    "LET'S GET SOME.");
   Wait(5);
   }
 for(i=0;i<28;i++)
  {
   Wait(10);
   if (CheckDemoExit()) return;
   }
 if (CheckDemoExit()) return;
 VI_FadeOut(0,256,0,0,0,48);
 if (CheckDemoExit()) return;
 }


void intro(void)
{
 int i;

 INT_TimerHook(IntroCommand);

 PlaySong("INTRO.S3M",0);

 VI_FillPalette(0,0,0);

 quitgame=0;
 gameloaded=0;

 if (!redo)
  MainIntro();

 redo=0;

// lock_region((void near *)IntroCommand,(char *)IntroStub - (char near *)IntroCommand);
// INT_TimerHook(IntroCommand);
 VI_FillPalette(0,0,0);
 activatemenu=false;
 for(;;)
  {
   for(i=1;i<6;i++)
    {
     ShowPortrait(i);
     if (quitgame || gameloaded)
      {
       INT_TimerHook(NULL);
       return;
       }
     }
   }
 }


void LoadMiscData(void)
{
 int i;

 printf(".");
 font1=CA_CacheLump(CA_GetNamedNum("FONT1"));
 printf(".");
 font2=CA_CacheLump(CA_GetNamedNum("FONT2"));
 printf(".");
 font3=CA_CacheLump(CA_GetNamedNum("FONT3"));
 statusbar[0]=CA_CacheLump(CA_GetNamedNum("STATBAR1"));
 statusbar[1]=CA_CacheLump(CA_GetNamedNum("STATBAR2"));
 statusbar[2]=CA_CacheLump(CA_GetNamedNum("STATBAR3"));
 statusbar[3]=CA_CacheLump(CA_GetNamedNum("STATBAR4"));
 printf(".");
 for(i=0;i<10;i++)
  heart[i]=CA_CacheLump(CA_GetNamedNum("HEART")+i);
 printf(".");
 backdrop=malloc(256*256);
 if (backdrop==NULL)
  MS_Error("Out of memory for BackDrop");
 printf(".");
 for(i=0;i<256;i++)
  backdroplookup[i]=(byte*)backdrop+256*i;
 printf(".");
 }


void checkexit()
{
 if (newascii && lastascii==27) MS_Error("Cancel Startup");
 newascii=false;
 }


void LoadData(void)
{
 bool		netplay = false;
 int		parm;
 longint	netaddr;

 if (MS_CheckParm("nointro")) 
	 nointro=true;
 if (MS_CheckParm("record")) 
	 recording=true;
 if (MS_CheckParm("playback")) 
	 playback=true;
 if (MS_CheckParm("debugmode")) 
	 debugmode=true;
 parm=MS_CheckParm("net");
 if (parm && parm<my_argc-1)
  {
   netaddr=atoi(my_argv[parm+1]);
   netplay=true;
   netmode=1;
   }

 if (MS_CheckParm("nospawn")) nospawn=true;
 if (MS_CheckParm("ticker")) ticker=true;
 CA_InitFile("GREED.BLO");
 if (netplay)
  {
   NetInit((void *)(intptr_t)netaddr);
   nointro=true;
   }
 InitSound();
 INT_Setup();
 checkexit();
 RF_PreloadGraphics();
 checkexit();
 RF_Startup();
 checkexit();
 LoadMiscData();
 checkexit();
 VI_Init(0);
 }


void startup()
{
	/* The Win32 port hardcoded nointro=true here, so the intro never ran.
	   LoadData already parses -nointro; honour it.  Restored otherwise to the
	   DOS main() verbatim, including the netmode newplayer branch the Win32
	   port dropped -- and including its double maingame() call in the nointro
	   path, which is original behaviour, not a porting slip. */
	LoadData();

restart:

	if (nointro)
	{
		redo = false;
		VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));
		if (netmode)
			newplayer(SC.netmap,0,SC.netdifficulty);
		else
			newplayer(0,0,2);
		/* newplayer runs the mission briefing, which the player can quit out
		   of.  InitData clears quitgame, so without this the quit is swallowed
		   and the game starts anyway. */
		if (!quitgame)
			maingame();
	}
	else
		intro();

	if (!quitgame)
	{
		maingame();
		if ( redo )
			goto restart;
	}
	else
		StopMusic();

	INT_Shutdown();
}
