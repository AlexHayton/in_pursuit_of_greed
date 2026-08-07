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
#include <MATH.H>
#include <TIME.H>
#include "d_global.h"
#include "d_disk.h"
#include "d_misc.h"
#include "d_video.h"
#include "d_ints.h"
#include "r_refdef.h"
#include "d_font.h"
#include "protos.h"

/**** VARIABLES ****/

#define PLAYERMOVESPEED   FRACUNIT*2.5
#define MOVEUNIT          FRACUNIT
#define FALLUNIT          FRACUNIT
#define MAXAMMO           300
#define DEFAULTVRDIST     157286
#define DEFAULTVRANGLE    4

player_t player;
byte     resizeScreen, biggerScreen, warpActive, currentViewSize=0;
longint  keyboardDelay, frames, weapdelay, spritemovetime, secretdelay,
	 RearViewTime, RearViewDelay, inventorytime;
font_t   *font1, *font2, *font3;
pic_t    *weaponpic[7], *statusbar[4];
byte     *backdrop;
byte     *backdroplookup[256<<2];   /* sized for the largest skyshift */
bool  changingweapons, weaponlowering, quitgame, togglemapmode,
	 toggleheatmode, heatmode, godmode, togglemotionmode, motionmode,
	 hurtborder, recording, playback, activatemenu, specialcode,
	 debugmode, gameloaded, nospawn, doorsound, deadrestart, ticker,
	 togglegoalitem, waterdrop, gamepause, ExitLevel, exitexists, warpjammer,
	 paused, QuickExit, autorun, ToggleRearView, RearViewOn, checktrigger,
	 activatehelp, useitem, toggleautorun, goiright, goileft, activatebrief,
	 midgetmode, autotarget=1, toggleautotarget, adjustvrangle;
float	 adjustvrdist;
int      weapmode, newweapon, weaponychange, headbob, weapbob, moveforward,
	 changelight, lighting, wbobcount, turnrate, mapmode, secretindex,
	 scrollview, doorx, doory, MapZoom, Warping, goalitem, specialeffect,
	 falldamage, netmode, wallanimcount, netmsgindex, netmsgstatus,
	 playerturnspeed=8, turnunit=2, exitx, exity, songnum, enemyviewmode;
fixed_t  moverate, strafrate, fallrate, WarpX, WarpY;
longint  wallanimationtime, recordindex, netsendtime, specialeffecttime,
	 SwitchTime, nethurtsoundtime;
byte     *demobuffer;
byte     demokb[NUMCODES];
char     secretbuf[30];
char     netmsg[30];
byte     rearbuf[64*64];
bonus_t  BonusItem;
bool  SaveTheScreen, redo, newsong;

extern entry_t   entries[1024], *entry_p;
extern int       frameon, rtimecount;
extern SoundCard SC;
extern int       fliplayed;
extern void      (*timerhook2)();

/**** FUNCTIONS ****/

void selectsong(int num);

void CheckElevators(void)
{
 elevobj_t *elev_p;
 longint   time;
 fixed_t   floorz, newfloorz;

 floorz=RF_GetFloorZ(player.x,player.y)+player.height;
 time=timecount;
 for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
  while (time>=elev_p->elevTimer)
   {
    if (elev_p->elevUp && (elev_p->position+=elev_p->speed)>=elev_p->ceiling)
     {
      SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
      elev_p->position=elev_p->ceiling;
      if (elev_p->type==E_NORMAL) elev_p->elevDown=true;
       else if (elev_p->type!=E_SWAP && elev_p->type!=E_SECRET)
	{
	 if (elev_p->endeval)
	  Event(elev_p->endeval,false);
	 floorheight[elev_p->mapspot]=elev_p->position;
	 if (mapsprites[elev_p->mapspot]==SM_ELEVATOR)
	  mapsprites[elev_p->mapspot]=0;
	 elev_p=elev_p->prev;
	 RF_RemoveElevator(elev_p->next);
	 break;
	 }
      elev_p->elevUp=false;
      elev_p->elevTimer+=280;
      }
    else if (elev_p->elevDown && (elev_p->position-=elev_p->speed)<=elev_p->floor)
     {
      SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
      elev_p->position=elev_p->floor;
      if (elev_p->type==E_NORMAL || elev_p->type==E_SECRET) elev_p->elevUp=true;
       else if (elev_p->type!=E_SWAP)
	{
	 if (elev_p->endeval)
	  Event(elev_p->endeval,false);
	 floorheight[elev_p->mapspot]=elev_p->position;
	 if (mapsprites[elev_p->mapspot]==SM_ELEVATOR)
	  mapsprites[elev_p->mapspot]=0;
	 elev_p=elev_p->prev;
	 RF_RemoveElevator(elev_p->next);
	 break;
	 }
      elev_p->elevDown=false;
      elev_p->elevTimer+=280;
      }
    if (elev_p->type==E_SECRET && elev_p->elevUp)
     {
      if (player.mapspot==elev_p->mapspot || mapsprites[elev_p->mapspot]) elev_p->position=elev_p->floor;
      }
    if (mapsprites[elev_p->mapspot]==SM_ELEVATOR) mapsprites[elev_p->mapspot]=0;
    floorheight[elev_p->mapspot]=elev_p->position;
    elev_p->elevTimer+=MOVEDELAY;
    }
 newfloorz=RF_GetFloorZ(player.x,player.y)+player.height;
 if (newfloorz!=floorz)
  {
   if (player.z>newfloorz)
    {
     fallrate+=FALLUNIT;
     player.z-=fallrate;
     if (player.z<newfloorz) player.z=newfloorz;
     }
   else if (player.z<newfloorz)
    {
     player.z=newfloorz;
     fallrate=0;
     }
   }
 }


int ViewScrollRate(void)
/* SCROLLRATE is a number of view rows per tick.  Scaled with the view so that
   looking up and down covers the same ground per keypress at any render
   resolution; exactly SCROLLRATE at the original 200 rows. */
{
 int r;

 r=(SCROLLRATE*windowHeight)/INIT_VIEW_HEIGHT;
 return r<1 ? 1 : r;
 }


int ScrollAngle(void)
/* player.scrollmin is a view-row offset, but the aiming code reads it directly
   as an angle -- so the row range has to be normalised back to the 200-row
   scale it was written against, or vertical autoaim and shot pitch would change
   with the render resolution.  Identity at 200 rows. */
{
 return (((-player.scrollmin)*INIT_VIEW_HEIGHT)/windowHeight)&ANGLES;
 }


int GetTargetAngle(int n, fixed_t pz)
{
 scaleobj_t *hsprite;
 int        counter, mapspot, result, x, y, z, d, accuracy;
 bool    found;
 fixed_t    sz;

 if (!autotarget)
  return ScrollAngle();
 accuracy=16; //16 + 2*player.difficulty;
 counter=0;
 found=false;
 msprite=&probe;
 probe.x=player.x;
 probe.y=player.y;
 probe.z=player.z;
 probe.angle=player.angle+n;
 probe.zadj=player.height;
 probe.startspot=(player.y>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT);
 while (counter++<MAXPROBE)
  {
   result=SP_Thrust();
   if (result==1)
    {
     for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
      if (hsprite->hitpoints)
       {
	mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	if (mapspot==spriteloc)
	 {
	  found=true;
	  counter=MAXPROBE;
	  break;
	  }
	}
     }
   }
 if (!found)
  {
   counter=0;
   msprite=&probe;
   probe.x=player.x;
   probe.y=player.y;
   probe.z=player.z;
   probe.angle=player.angle+n+accuracy;
   probe.zadj=player.height;
   probe.startspot=(player.y>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT);
   while (counter++<MAXPROBE)
    {
     result=SP_Thrust();
     if (result==1)
      {
       for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
	if (hsprite->hitpoints)
	 {
	  mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	  if (mapspot==spriteloc)
	   {
	    found=true;
	    player.angle+=accuracy;
	    counter=MAXPROBE;
	    break;
	    }
	  }
       }
     }
   }
 if (!found)
  {
   counter=0;
   msprite=&probe;
   probe.x=player.x;
   probe.y=player.y;
   probe.z=player.z;
   probe.angle=player.angle+n-accuracy;
   probe.zadj=player.height;
   probe.startspot=(player.y>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT);
   while (counter++<MAXPROBE)
    {
     result=SP_Thrust();
     if (result==1)
      {
       for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
	if (hsprite->hitpoints)
	 {
	  mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	  if (mapspot==spriteloc)
	   {
	    found=true;
	    player.angle-=accuracy;
	    counter=MAXPROBE;
	    break;
	    }
	  }
       }
     }
   }
 if (!found)
  {
   counter=0;
   msprite=&probe;
   probe.x=player.x;
   probe.y=player.y;
   probe.z=player.z;
   probe.angle=player.angle+n+accuracy/2;
   probe.zadj=player.height;
   probe.startspot=(player.y>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT);
   while (counter++<MAXPROBE)
    {
     result=SP_Thrust();
     if (result==1)
      {
       for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
	if (hsprite->hitpoints)
	 {
	  mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	  if (mapspot==spriteloc)
	   {
	    found=true;
	    player.angle+=accuracy/2;
	    counter=MAXPROBE;
	    break;
	    }
	  }
       }
     }
   }
 if (!found)
  {
   counter=0;
   msprite=&probe;
   probe.x=player.x;
   probe.y=player.y;
   probe.z=player.z;
   probe.angle=player.angle+n-accuracy/2;
   probe.zadj=player.height;
   probe.startspot=(player.y>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT);
   while (counter++<MAXPROBE)
    {
     result=SP_Thrust();
     if (result==1)
      {
       for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
	if (hsprite->hitpoints)
	 {
	  mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	  if (mapspot==spriteloc)
	   {
	    found=true;
	    player.angle-=accuracy/2;
	    counter=MAXPROBE;
	    break;
	    }
	  }
       }
     }
   }
 if (found)
  {
   pz+=player.z;
   sz=hsprite->z+(hsprite->height/2);
   if (sz>pz)
    {
     z=(sz-pz)>>(FRACBITS+2);
     if (z>=MAXAUTO) return ScrollAngle();
     x=(hsprite->x-player.x)>>(FRACBITS+2);
     y=(hsprite->y-player.y)>>(FRACBITS+2);
     d=(int)sqrt(x*x + y*y);
     if (d>=MAXAUTO || autoangle2[d][z]==-1) return ScrollAngle();
     return autoangle2[d][z];
     }
   else if (sz<pz)
    {
     z=(pz-sz)>>(FRACBITS+2);
     if (z>=MAXAUTO) return ScrollAngle();
     x=(hsprite->x-player.x)>>(FRACBITS+2);
     y=(hsprite->y-player.y)>>(FRACBITS+2);
     d=(int)sqrt(x*x + y*y);
     if (d>=MAXAUTO || autoangle2[d][z]==-1) return ScrollAngle();
     return -autoangle2[d][z];
     }
   else return ScrollAngle();
   }
 else return ScrollAngle();
 }


void fireweapon(void)
{
// scaleobj_t *hsprite;
// int        px, py, sx, sy, xmove, ymove, spriteloc1, spriteloc2, mapspot;
 int        i, n, angle2, ammo, angle, angleinc, oldangle;
 fixed_t    z, xmove2, ymove2;

 player.status=2;
 n=player.weapons[player.currentweapon];
 ammo=weapons[n].ammorate;
 if (player.ammo[weapons[n].ammotype]<ammo) return;
 oldangle=player.angle;
 weapons[n].charge=0;
 player.ammo[weapons[n].ammotype]-=ammo;
 if (n!=4 && n!=18 && weapmode!=1) weapmode=1;
 switch (n)
  {
   case 13: // mooman #1
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_HANDBULLET,player.x,player.y,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SoundEffect(SN_BULLET13,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET13,0,player.x,player.y);
    break;
   case 8: // lizard #1
   case 14: // specimen #1
   case 15: // trix #1
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_HANDBULLET,player.x,player.y,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SoundEffect(SN_BULLET8,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET8,0,player.x,player.y);
    break;
   case 1: // psyborg #2
    z=player.height-(52<<FRACBITS);
    angle2=GetTargetAngle(0,z);
    SpawnSprite(S_BULLET1,player.x,player.y,player.z,z,player.angle,angle2,true,playernum);
    SoundEffect(SN_BULLET1,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET1,0,player.x,player.y);
    break;
   case 2:
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET2,player.x,player.y,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SoundEffect(SN_BULLET5,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET5,0,player.x,player.y);
    break;
   case 3:
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET3,player.x,player.y,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SoundEffect(SN_BULLET3,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET3,0,player.x,player.y);
    break;
   case 4:
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET4,player.x,player.y,player.z,z,player.angle-48,GetTargetAngle(-16,z),true,playernum);
    SpawnSprite(S_BULLET4,player.x,player.y,player.z,z,player.angle-24,GetTargetAngle(-32,z),true,playernum);
    SpawnSprite(S_BULLET4,player.x,player.y,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SpawnSprite(S_BULLET4,player.x,player.y,player.z,z,player.angle+24,GetTargetAngle(+16,z),true,playernum);
    SpawnSprite(S_BULLET4,player.x,player.y,player.z,z,player.angle+48,GetTargetAngle(+32,z),true,playernum);
    break;
   case 5:

    break;
   case 6:
    break;
   case 7: // psyborg #1
    z=player.height-(52<<FRACBITS);
    angle2=GetTargetAngle(0,z);
    SpawnSprite(S_BULLET7,player.x,player.y,player.z,z,player.angle,angle2,true,playernum);
    SoundEffect(SN_BULLET13,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET13,0,player.x,player.y);
    break;
   case 9: // lizard #2
    z=player.height-(52<<FRACBITS);
    angle2=GetTargetAngle(0,z);
    SpawnSprite(S_BULLET9,player.x,player.y,player.z,z,player.angle,angle2,true,playernum);
    SoundEffect(SN_BULLET9,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET1,0,player.x,player.y);
    break;
   case 10: // specimen #2
    z=player.height-(52<<FRACBITS);
    angle2=GetTargetAngle(0,z);
    SpawnSprite(S_BULLET10,player.x,player.y,player.z,z,player.angle,angle2,true,playernum);
    SoundEffect(SN_BULLET10,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET10,0,player.x,player.y);
    break;
   case 11: // mooman #2
    z=player.height-(52<<FRACBITS);
    angle2=GetTargetAngle(0,z);
    SpawnSprite(S_BULLET11,player.x,player.y,player.z,z,player.angle,angle2,true,playernum);
    SoundEffect(SN_BULLET1,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET1,0,player.x,player.y);
    break;
   case 12: // dominatrix #2
    angle=(player.angle-NORTH)&ANGLES;
    xmove2=FIXEDMUL(FRACUNIT*4,costable[angle]);
    ymove2=-FIXEDMUL(FRACUNIT*4,sintable[angle]);
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET12,player.x+xmove2,player.y+ymove2,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    angle=(player.angle+NORTH)&ANGLES;
    xmove2=FIXEDMUL(FRACUNIT*4,costable[angle]);
    ymove2=-FIXEDMUL(FRACUNIT*4,sintable[angle]);
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET12,player.x+xmove2,player.y+ymove2,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SoundEffect(SN_BULLET12,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET12,0,player.x,player.y);
    break;

   case 16: // red gun
    angle=(player.angle-NORTH)&ANGLES;
    xmove2=FIXEDMUL(FRACUNIT*4,costable[angle]);
    ymove2=-FIXEDMUL(FRACUNIT*4,sintable[angle]);
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET16,player.x+xmove2,player.y+ymove2,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    angle=(player.angle+NORTH)&ANGLES;
    xmove2=FIXEDMUL(FRACUNIT*4,costable[angle]);
    ymove2=-FIXEDMUL(FRACUNIT*4,sintable[angle]);
    z=player.height-(fixed_t)(50<<FRACBITS);
    SpawnSprite(S_BULLET16,player.x+xmove2,player.y+ymove2,player.z,z,player.angle,GetTargetAngle(0,z),true,playernum);
    SoundEffect(SN_BULLET12,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET12,0,player.x,player.y);
    break;

   case 17: // blue gun
    z=player.height-(64<<FRACBITS);
    angle2=GetTargetAngle(0,z);
    SpawnSprite(S_BULLET17,player.x,player.y,player.z,z,player.angle,angle2,true,playernum);
    SoundEffect(SN_BULLET17,0,player.x,player.y);
    if (netmode) NetSoundEffect(SN_BULLET1,0,player.x,player.y);
    break;

   case 18: // green gun
    angleinc=ANGLES/12;
    angle=0;
    for(i=0;i<12;i++,angle+=angleinc)
     {
      z=player.height-(52<<FRACBITS);
      angle2=GetTargetAngle(0,z);
      SpawnSprite(S_BULLET18,player.x,player.y,player.z,z,angle,angle2,true,playernum);
      }
    if (netmode)
     NetSendSpawn(S_BULLET18,player.x,player.y,player.z,z,angle,angle2,true,playernum);
    break;
   }
 player.angle=oldangle;
 }


bool FindWarpDestination(int *x, int *y,byte warpValue)
{
 int search, nosearch;

 nosearch=*y*MAPSIZE+*x;
 if (!warpActive)
  {
   for(search=0;search<MAPROWS*MAPCOLS;search++)
    if (mapsprites[search]==warpValue && search!=nosearch)
     {
      *x=search & (MAPSIZE-1);
      *y=search>>TILESHIFT;
      turnrate=0;
      moverate=0;
      fallrate=0;
      strafrate=0;
      ResetMouse();
      warpActive=warpValue;
      return true;
      }
   }
 return false;
 }


void CheckItems(int centerx,int centery,bool useit,int chartype)
{
 scaleobj_t *sprite;
 int        mapspot, value, value2, index, ammo, cmapspot;
 int        x, y, i, j;
 elevobj_t  *elev_p;
 bool    sound;

 mapspot=centery*MAPCOLS+centerx;
 value=mapsprites[mapspot];
 switch (value)
  {
   case SM_MEDPAK1:
   case SM_MEDPAK2:
   case SM_MEDPAK3:
   case SM_MEDPAK4:
    value2=value-SM_MEDPAK1+S_MEDPAK1;
    for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==value2)
       {
	if (useit && netmode) NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  if (player.angst==player.maxangst)
	   {
	    player.inventory[0]+=5-(value-SM_MEDPAK1);
	    if (player.inventory[0]>20) player.inventory[0]=20;
	    oldinventory=-2;
	    inventoryleft();
	    inventoryright();
	    writemsg("Stored MedTube!");
	    }
	  else
	   {
	    medpaks((5-(value-SM_MEDPAK1))*50);
	    writemsg("Used MedTube!");
	    }
	  }
	SpawnSprite(S_GENERATOR,sprite->x,sprite->y,0,0,0,0,false,0);
	RF_RemoveSprite(sprite);
	return;
	}
    break;

   case SM_SHIELD1:
   case SM_SHIELD2:
   case SM_SHIELD3:
   case SM_SHIELD4:
    value2=value-SM_MEDPAK1+S_MEDPAK1;
    for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==value2)
       {
	if (useit && netmode) NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  if (player.shield==player.maxshield)
	   {
	    player.inventory[1]+=1+(value-SM_SHIELD1);
	    if (player.inventory[1]>20) player.inventory[1]=20;
	    oldinventory=-2;
	    inventoryleft();
	    inventoryright();
	    writemsg("Stored Shield Charge!");
	    }
	  else
	   {
	    heal((1+(value-SM_SHIELD1))*50);
	    writemsg("Used Shield Charge!");
	    }
	  }
	SpawnSprite(S_GENERATOR,sprite->x,sprite->y,0,0,0,0,false,0);
	RF_RemoveSprite(sprite);
	return;
	}
    break;

   case SM_ENERGY:
   case SM_BALLISTIC:
   case SM_PLASMA:
    value2=value-SM_ENERGY+S_ENERGY;
    for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==value2)
       {
	if (useit && netmode)
	 NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  hurtborder=true;
	  player.ammo[value-SM_ENERGY]+=75;
	  if (player.ammo[value-SM_ENERGY]>MAXAMMO)
	   player.ammo[value-SM_ENERGY]=MAXAMMO;
	  oldshots=-1;
	  writemsg(pickupammomsg[value-SM_ENERGY]);
	  }
	SpawnSprite(S_GENERATOR,sprite->x,sprite->y,0,0,0,0,false,0);
	RF_RemoveSprite(sprite);
	return;
	}
    break;

   case SM_AMMOBOX:
    value2=weapons[player.weapons[player.currentweapon]].ammotype;
    if (useit && (player.ammo[value2]>=MAXAMMO ||
     (weapons[player.currentweapon].ammorate==0 &&
      player.ammo[0]>=MAXAMMO && player.ammo[1]>=MAXAMMO &&
      player.ammo[2]>=MAXAMMO)))
     return;
    for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==S_AMMOBOX)
       {
	if (useit && netmode)
	 NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  hurtborder=true;
	  if (weapons[player.currentweapon].ammorate==0)
	   {
	    player.ammo[0]+=45;
	    if (player.ammo[0]>MAXAMMO)
	     player.ammo[0]=MAXAMMO;
	    player.ammo[1]+=45;
	    if (player.ammo[1]>MAXAMMO)
	     player.ammo[1]=MAXAMMO;
	    player.ammo[2]+=45;
	    if (player.ammo[2]>MAXAMMO)
	     player.ammo[2]=MAXAMMO;
	    }
	  else
	   {
	    player.ammo[value2]+=125;
	    if (player.ammo[value2]>MAXAMMO)
	     player.ammo[value2]=MAXAMMO;
	    }
	  oldshots=-1;
	  writemsg(pickupmsg[11]);
	  }
	if (sprite->deathevent)
	 Event(sprite->deathevent,false);
	RF_RemoveSprite(sprite);
	return;
	}
    break;
   case SM_MEDBOX:
    if (useit && player.angst==player.maxangst && player.shield==player.maxshield)
     return;
    for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==S_MEDBOX)
       {
	if (useit && netmode)
	 NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  heal(250);
	  medpaks(250);
	  hurtborder=true;
	  writemsg(pickupmsg[12]);
	  }
	if (sprite->deathevent)
	 Event(sprite->deathevent,false);
	RF_RemoveSprite(sprite);
	return;
	}
    break;
   case SM_GOODIEBOX:
    for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==S_GOODIEBOX)
       {
	if (useit && netmode)
	 NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  for (i=0;i<2;i++)
	   {
	    if (netmode)
	     do
	      {
	       value2=(clock()+MS_RndT())%11;
	       } while (value2!=8);
	    else
	     do
	      {
	       value2=(clock()+MS_RndT())%11;
	       } while (value2!=6 && value2!=10 && value2!=12);
	    player.inventory[value2+2]+=pickupamounts[value2];
	    if (value2==2)
	     {
	      if (player.inventory[2]>15)
	       player.inventory[2]=15;
	      }
	    else if (player.inventory[value2+2]>10)
	     player.inventory[value2+2]=10;
	    }
	  oldinventory=-2;
	  inventoryleft();
	  inventoryright();
	  hurtborder=true;
	  writemsg(pickupmsg[13]);
	  }
	if (sprite->deathevent)
	 Event(sprite->deathevent,false);
	RF_RemoveSprite(sprite);
	return;
	}
    break;

   case SM_IGRENADE:
   case SM_IREVERSO:
   case SM_IPROXMINE:
   case SM_ITIMEMINE:
   case SM_IDECOY:
   case SM_IINSTAWALL:
   case SM_ICLONE:
   case SM_IHOLO:
   case SM_IINVIS:
   case SM_IJAMMER:
   case SM_ISTEALER:
    value2=value-SM_IGRENADE+S_IGRENADE;
    for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==value2)
       {
	if (useit && netmode)
	 NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  hurtborder=true;
	  player.inventory[value-SM_IGRENADE+2]+=pickupamounts[value-SM_IGRENADE];
	  if (value==SM_IGRENADE)
	   {
	    if (player.inventory[2]>15)
	     player.inventory[2]=15;
	    }
	  else if (player.inventory[value-SM_IGRENADE+2]>10)
	   player.inventory[value-SM_IGRENADE+2]=10;
	  writemsg(pickupmsg[value-SM_IGRENADE]);
	  oldinventory=-2;
	  inventoryleft();
	  inventoryright();
	  }
	SpawnSprite(S_GENERATOR,sprite->x,sprite->y,0,0,0,0,false,0);
	RF_RemoveSprite(sprite);
	return;
	}
    break;
/*   case SM_HOLE:
    if (useit && player.inventory[10]>=10) return;
    for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==S_HOLE)
       {
	if (useit && netmode) NetItemPickup(centerx,centery);
	mapsprites[mapspot]=0;
	SoundEffect(SN_PICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  hurtborder=true;
	  ++player.inventory[10];
	  writemsg("Portable Hole picked up!");
	  oldinventory=-2;
	  inventoryleft();
	  inventoryright();
	  }
	RF_RemoveSprite(sprite);
	return;
	}
    break; */

   case SM_BONUSITEM:
    if (useit)
     {
      if (netmode) NetItemPickup(centerx,centery);
      addscore(BonusItem.score);
      heal(150);
      medpaks(150);
      hurtborder=true;
      writemsg("Bonus Item!");
      }
    BonusItem.score=0;
    BonusItem.mapspot=-1;
    RF_RemoveSprite(BonusItem.sprite);
    mapsprites[mapspot]=0;
    SoundEffect(SN_WEAPPICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
    BonusItem.name=NULL;
    break;

   case SM_PRIMARY1:
   case SM_PRIMARY2:
    value2=mapsprites[mapspot]-SM_PRIMARY1 + S_PRIMARY1;
    for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
      if (sprite->type==value2)
       {
	if (useit && netmode) NetItemPickup(centerx,centery);
	RF_RemoveSprite(sprite);
	mapsprites[mapspot]=0;
	SoundEffect(SN_WEAPPICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	if (useit)
	 {
	  heal(150);
	  medpaks(150);
	  hurtborder=true;
	  addscore(primaries[(value2-S_PRIMARY1)*2 + 1]);
	  writemsg("Primary goal item!");
	  player.primaries[value2-S_PRIMARY1]++;
	  }
	return;
	}
    break;
   case SM_SECONDARY1:
   case SM_SECONDARY2:
   case SM_SECONDARY3:
   case SM_SECONDARY4:
   case SM_SECONDARY5:
   case SM_SECONDARY6:
   case SM_SECONDARY7:
    value2=mapsprites[mapspot]-SM_SECONDARY1 + S_SECONDARY1;
    for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;sprite=sprite->next)
     if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery && sprite->type==value2)
      {
       if (useit && netmode)
	NetItemPickup(centerx,centery);
       RF_RemoveSprite(sprite);
       mapsprites[mapspot]=0;
       SoundEffect(SN_WEAPPICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
       if (useit)
	{
	 heal(150);
	 medpaks(150);
	 hurtborder=true;
	 addscore(secondaries[(value2-S_SECONDARY1)*2 + 1]);
	 writemsg("Secondary goal item!");
	 player.secondaries[value2-S_SECONDARY1]++;
	 }
       return;
       }
    break;

   case SM_SWITCHDOWN:
    sound=false;
    for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
     if (elev_p->type==E_SWITCHDOWN && !elev_p->elevDown)
      {
       elev_p->elevDown=true;
       elev_p->elevTimer=timecount;
       sound=true;
       SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
       }
    if (useit && netmode) NetItemPickup(centerx,centery);
    mapsprites[mapspot]=0;
    if (useit && sound)
     {
      SoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
      if (netmode) NetSoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
      SoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
      if (netmode) NetSoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
      }
    break;
   case SM_SWITCHDOWN2:
    if (useit && netmode) NetItemPickup(centerx,centery);
    sound=false;
    for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
     if (elev_p->type==E_SWITCHDOWN2 && !elev_p->elevDown)
      {
       elev_p->elevDown=true;
       elev_p->elevTimer=timecount;
       SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
       sound=true;
       }
    if (useit && netmode) NetItemPickup(centerx,centery);
    mapsprites[mapspot]=0;
    if (sound) SoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
    break;
   case SM_SWITCHUP:
    if (useit && netmode) NetItemPickup(centerx,centery);
    sound=false;
    for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
     if (elev_p->type==E_SWITCHUP && !elev_p->elevUp)
      {
       elev_p->elevUp=true;
       elev_p->elevTimer=timecount;
       sound=true;
       SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
       }
    if (useit && netmode) NetItemPickup(centerx,centery);
    mapsprites[mapspot]=0;
    if (sound) SoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
    break;

   case SM_EXIT:
    ExitLevel=true;
    break;

   case SM_WEAPON0:
   case SM_WEAPON1:
   case SM_WEAPON2:
   case SM_WEAPON3:
   case SM_WEAPON4:
   case SM_WEAPON5:
   case SM_WEAPON6:
   case SM_WEAPON7:
   case SM_WEAPON8:
   case SM_WEAPON9:
   case SM_WEAPON10:
   case SM_WEAPON11:
   case SM_WEAPON12:
   case SM_WEAPON13:
   case SM_WEAPON14:
   case SM_WEAPON15:
   case SM_WEAPON16:
   case SM_WEAPON17:
   case SM_WEAPON18:
    value2=value-SM_WEAPON0;
    ammo=weapons[value2].ammotype;
    index=ammo+2;

    if (player.weapons[index]==value2 && !netmode)
     {
      player.ammo[ammo]+=100;
      if (player.ammo[ammo]>MAXAMMO) player.ammo[ammo]=MAXAMMO;
      writemsg("Found more ammo.");
      }
    else if (player.weapons[index]!=-1 && !netmode)
     {
      for(i=-MAPCOLS;i<=MAPCOLS;i+=MAPCOLS)
       for(j=-1;j<=1;j++)
	{
	 cmapspot=mapspot+i+j;
	 if (cmapspot!=mapspot && floorpic[cmapspot] && mapsprites[cmapspot]==0)
	  {
	   x=(cmapspot&63)*MAPSIZE+32;
	   y=(cmapspot/64)*MAPSIZE+32;
	   SpawnSprite(player.weapons[index]+S_WEAPON0,x<<FRACBITS,y<<FRACBITS,0,0,0,0,0,0);
	   i=MAPCOLS*2;
	   j=1;
	   break;
	   }
	 }
      if (player.currentweapon==index)
       {
	weaponlowering=false;
	newweapon=index;
	loadweapon(value2);
	weaponychange=weaponpic[0]->height/hudscale-20;
	changingweapons=true;
	}
      else
       {
	changingweapons=true;
	weaponlowering=true;
	newweapon=index;
	}
      writemsg("Exchanged weapons!");
      }
    else if (!netmode)
     {
      writemsg("Acquired new weapon!");
      changingweapons=true;
      weaponlowering=true;
      newweapon=index;
      }
    if (netmode)
     {
      if (player.weapons[index]==value2)
       return;
      writemsg("Acquired new weapon!");
      player.weapons[index]=value-SM_WEAPON0;
      player.ammo[ammo]+=100;
      if (player.ammo[ammo]>MAXAMMO) player.ammo[ammo]=MAXAMMO;
      SoundEffect(SN_WEAPPICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
      NetSoundEffect(SN_WEAPPICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
      changingweapons=true;
      weaponlowering=true;
      newweapon=index;
      }
    else
     {
      value2=value-SM_WEAPON0+S_WEAPON0;
      for (sprite=firstscaleobj.next;sprite!=&lastscaleobj;sprite=sprite->next)
       if (sprite->x>>FRACTILESHIFT==centerx && sprite->y>>FRACTILESHIFT==centery)
	if (sprite->type==value2)
	 {
	  player.weapons[index]=value-SM_WEAPON0;
	  value2=weapons[player.weapons[index]].ammotype;
	  hurtborder=true;
	  RF_RemoveSprite(sprite);
	  mapsprites[mapspot]=0;
	  SoundEffect(SN_WEAPPICKUP0+chartype,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
	  return;
	  }
      }
    break;
   }
 }


void CheckWarps(fixed_t centerx,fixed_t centery)
{
 int x, y, mapspot;

 x=centerx>>FRACTILESHIFT;
 y=centery>>FRACTILESHIFT;
 mapspot=y*MAPCOLS+x;
 if (mapsprites[mapspot]>=128 && mapsprites[mapspot]<=130)
  {
   if (Warping) return;
   if (FindWarpDestination(&x, &y, mapsprites[mapspot]))
    {
     WarpX=(x*MAPSIZE+32)<<FRACBITS;
     WarpY=(y*MAPSIZE+32)<<FRACBITS;
     Warping=1;
     }
   }
 else
  {
   warpActive=0;
   if (mapsprites[mapspot]>130)
    CheckItems(x,y,true,player.chartype);
   }
 if (triggers[x][y])
  {
   SoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
   if (netmode) NetSoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
   SoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
   if (netmode) NetSoundEffect(SN_TRIGGER,0,centerx<<FRACTILESHIFT,centery<<FRACTILESHIFT);
   Event(triggers[x][y],true);
   }
 }


void CheckDoors(fixed_t centerx, fixed_t centery)
{
 int       x, y, mapspot;
 doorobj_t *door_p, *last_p;

 x=(int)((centerx)>>FRACTILESHIFT);
 y=(int)((centery)>>FRACTILESHIFT);
 last_p=&doorlist[numdoors];
 for (door_p=doorlist; door_p != last_p; door_p++)
  while (timecount>=door_p->doorTimer)
   {
    mapspot=door_p->tiley*MAPCOLS + door_p->tilex;
    if ((door_p->tilex==x && door_p->tiley==y) || mapsprites[mapspot])
     {
      if (door_p->doorOpen && !door_p->doorClosing) door_p->doorBlocked=true;
      }
    else door_p->doorBlocked=false;

    if (door_p->doorOpening)
     {
      if ((door_p->doorSize-=4)<=MINDOORSIZE)
       {
	door_p->doorSize=MINDOORSIZE;
	door_p->doorOpening=false;
	door_p->doorOpen=true;
	door_p->doorTimer+=270; // 3 seconds
	}
      else door_p->doorTimer+=MOVEDELAY;
      }
    else if (door_p->doorClosing)
     {
      if ((door_p->doorSize+=4)>=64)
       {
	door_p->doorSize=64;
	door_p->doorClosing=false;
	door_p->doorTimer+=MOVEDELAY;
	}
      else door_p->doorTimer+=MOVEDELAY;
      }
    else if (door_p->doorOpen && timecount>door_p->doorTimer && !door_p->doorBlocked)
     {
      door_p->doorClosing=true;
      door_p->doorOpen=false;
      SoundEffect(SN_DOOR,15,door_p->tilex<<FRACTILESHIFT,door_p->tiley<<FRACTILESHIFT);
      door_p->doorTimer+=MOVEDELAY;
      }
    else door_p->doorTimer+=MOVEDELAY;

    door_p->position=door_p->doorSize<<FRACBITS;
    }
 }


bool CheckForSwitch(int x,int y,int angle,bool doubleswitch)
{
 int mapspot;

 mapspot=y*MAPCOLS+x;
 if (timecount<SwitchTime) return false;
 if (angle>=SOUTH+DEGREE45 || angle<DEGREE45)
  {
   if (westwall[mapspot+1]==127) return true;
    else if (westwall[mapspot+1]==128 && doubleswitch) return true;
    else if (westwall[mapspot+1]==172) return true;
    else if (westwall[mapspot+1]==173 && doubleswitch) return true;
    else if (westwall[mapspot+1]==75) return true;
    else if (westwall[mapspot+1]==76 && doubleswitch) return true;
    else if (westwall[mapspot+1]==140) return true;
    else if (westwall[mapspot+1]==141 && doubleswitch) return true;
    else if (westwall[mapspot+1]==234) return true;
    else if (westwall[mapspot+1]==235 && doubleswitch) return true;
   }
 else if (angle>=DEGREE45 && angle<NORTH+DEGREE45)
  {
   if (northwall[mapspot]==127) return true;
    else if (northwall[mapspot]==128 && doubleswitch) return true;
    else if (northwall[mapspot]==172) return true;
    else if (northwall[mapspot]==173 && doubleswitch) return true;
    else if (northwall[mapspot]==75) return true;
    else if (northwall[mapspot]==76 && doubleswitch) return true;
    else if (northwall[mapspot]==140) return true;
    else if (northwall[mapspot]==141 && doubleswitch) return true;
    else if (northwall[mapspot]==234) return true;
    else if (northwall[mapspot]==235 && doubleswitch) return true;
   }
 else if (angle>=NORTH+DEGREE45 && angle<WEST+DEGREE45)
  {
   if (westwall[mapspot]==127) return true;
    else if (westwall[mapspot]==128 && doubleswitch) return true;
    else if (westwall[mapspot]==172) return true;
    else if (westwall[mapspot]==173 && doubleswitch) return true;
    else if (westwall[mapspot]==75) return true;
    else if (westwall[mapspot]==76 && doubleswitch) return true;
    else if (westwall[mapspot]==140) return true;
    else if (westwall[mapspot]==141 && doubleswitch) return true;
    else if (westwall[mapspot]==234) return true;
    else if (westwall[mapspot]==235 && doubleswitch) return true;
   }
 else if (angle>=WEST+DEGREE45)
  {
   if (northwall[mapspot+MAPCOLS]==127) return true;
    else if (northwall[mapspot+MAPCOLS]==128 && doubleswitch) return true;
    else if (northwall[mapspot+MAPCOLS]==172) return true;
    else if (northwall[mapspot+MAPCOLS]==173 && doubleswitch) return true;
    else if (northwall[mapspot+MAPCOLS]==75) return true;
    else if (northwall[mapspot+MAPCOLS]==76 && doubleswitch) return true;
    else if (northwall[mapspot+MAPCOLS]==140) return true;
    else if (northwall[mapspot+MAPCOLS]==141 && doubleswitch) return true;
    else if (northwall[mapspot+MAPCOLS]==234) return true;
    else if (northwall[mapspot+MAPCOLS]==235 && doubleswitch) return true;
   }
 return false;
 }


void SwitchWall(int x,int y,int angle,bool doubleswitch)
{
 int mapspot;

 SoundEffect(SN_WALLSWITCH,0,x<<FRACTILESHIFT,y<<FRACTILESHIFT);
 mapspot=y*MAPCOLS+x;
 if (angle>=SOUTH+DEGREE45 || angle<DEGREE45)
  {
   if (westwall[mapspot+1]==127) westwall[mapspot+1]=128;
    else if (westwall[mapspot+1]==128 && doubleswitch) westwall[mapspot+1]=127;
    else if (westwall[mapspot+1]==172) westwall[mapspot+1]=173;
    else if (westwall[mapspot+1]==173 && doubleswitch) westwall[mapspot+1]=172;
    else if (westwall[mapspot+1]==75) westwall[mapspot+1]=76;
    else if (westwall[mapspot+1]==76 && doubleswitch) westwall[mapspot+1]=75;
    else if (westwall[mapspot+1]==140) westwall[mapspot+1]=141;
    else if (westwall[mapspot+1]==141 && doubleswitch) westwall[mapspot+1]=140;
    else if (westwall[mapspot+1]==234) westwall[mapspot+1]=235;
    else if (westwall[mapspot+1]==235 && doubleswitch) westwall[mapspot+1]=234;
   }
 else if (angle>=DEGREE45 && angle<NORTH+DEGREE45)
  {
   if (northwall[mapspot]==127) northwall[mapspot]=128;
    else if (northwall[mapspot]==128 && doubleswitch) northwall[mapspot]=127;
    else if (northwall[mapspot]==172) northwall[mapspot]=173;
    else if (northwall[mapspot]==173 && doubleswitch) northwall[mapspot]=172;
    else if (northwall[mapspot]==75) northwall[mapspot]=76;
    else if (northwall[mapspot]==76 && doubleswitch) northwall[mapspot]=75;
    else if (northwall[mapspot]==140) northwall[mapspot]=141;
    else if (northwall[mapspot]==141 && doubleswitch) northwall[mapspot]=140;
    else if (northwall[mapspot]==234) northwall[mapspot]=235;
    else if (northwall[mapspot]==235 && doubleswitch) northwall[mapspot]=234;
   }
 else if (angle>=NORTH+DEGREE45 && angle<WEST+DEGREE45)
  {
   if (westwall[mapspot]==127) westwall[mapspot]=128;
    else if (westwall[mapspot]==128 && doubleswitch) westwall[mapspot]=127;
    else if (westwall[mapspot]==172) westwall[mapspot]=173;
    else if (westwall[mapspot]==173 && doubleswitch) westwall[mapspot]=172;
    else if (westwall[mapspot]==75) westwall[mapspot]=76;
    else if (westwall[mapspot]==76 && doubleswitch) westwall[mapspot]=75;
    else if (westwall[mapspot]==140) westwall[mapspot]=141;
    else if (westwall[mapspot]==141 && doubleswitch) westwall[mapspot]=140;
    else if (westwall[mapspot]==234) westwall[mapspot]=235;
    else if (westwall[mapspot]==235 && doubleswitch) westwall[mapspot]=234;
   }
 else if (angle>=WEST+DEGREE45)
  {
   if (northwall[mapspot+MAPCOLS]==127) northwall[mapspot+MAPCOLS]=128;
    else if (northwall[mapspot+MAPCOLS]==128 && doubleswitch) northwall[mapspot+MAPCOLS]=127;
    else if (northwall[mapspot+MAPCOLS]==172) northwall[mapspot+MAPCOLS]=173;
    else if (northwall[mapspot+MAPCOLS]==173 && doubleswitch) northwall[mapspot+MAPCOLS]=172;
    else if (northwall[mapspot+MAPCOLS]==75) northwall[mapspot+MAPCOLS]=76;
    else if (northwall[mapspot+MAPCOLS]==76 && doubleswitch) northwall[mapspot+MAPCOLS]=75;
    else if (northwall[mapspot+MAPCOLS]==140) northwall[mapspot+MAPCOLS]=141;
    else if (northwall[mapspot+MAPCOLS]==141 && doubleswitch) northwall[mapspot+MAPCOLS]=140;
    else if (northwall[mapspot+MAPCOLS]==234) northwall[mapspot+MAPCOLS]=235;
    else if (northwall[mapspot+MAPCOLS]==235 && doubleswitch) northwall[mapspot+MAPCOLS]=234;
   }
 }


void CheckHere(int useit,fixed_t centerx,fixed_t centery,int angle)
/* check for door at centerx, centery */
{
 int        mapspot, x, y, x1, y1;
 elevobj_t  *elev_p;
 bool    switchit;

 TryDoor(centerx,centery);
 x=centerx>>FRACTILESHIFT;
 y=centery>>FRACTILESHIFT;
 mapspot=y*MAPCOLS+x;
 switchit=false;

 if (switches[x][y])
  {
   if (useit)
    {
     if (!CheckForSwitch(x,y,angle,true))
      goto skipit;
     if (netmode)
      NetCheckHere(centerx,centery,angle);
     }
   SwitchWall(x,y,angle,true);
   Event(switches[x][y],false);
   }
skipit:
 switch (mapsprites[mapspot])
  {
   case SM_SWAPSWITCH:
    if (useit && !CheckForSwitch(x,y,angle,true)) break;
    if (useit && netmode) NetCheckHere(centerx,centery,angle);
    for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
     if (elev_p->type==E_SWAP)
      {
       if (elev_p->position==elev_p->ceiling)
	{
	 elev_p->elevDown=true;
	 elev_p->elevTimer=timecount;
	 switchit=true;
	 }
       else if (elev_p->position==elev_p->floor)
	{
	 elev_p->elevUp=true;
	 elev_p->elevTimer=timecount;
	 switchit=true;
	 }
       }
    if (switchit)
     {
      SwitchWall(x,y,angle,true);
      SwitchTime=timecount+210;
      }
    break;

   case SM_STRIGGER:
    for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
     if (elev_p->type==E_SECRET && elev_p->elevDown==false && elev_p->elevUp==false)
      {
       x1=elev_p->mapspot%MAPCOLS;
       y1=elev_p->mapspot/MAPCOLS;
       if (abs(x1-x)<2 && abs(y1-y)<2)
	{
	 switchit=true;
	 elev_p->elevDown=true;
	 elev_p->elevTimer=timecount;
	 CheckHere(false,x1<<FRACTILESHIFT,y1<<FRACTILESHIFT,angle);
	 }
       }
    if (switchit && useit && netmode)
     NetCheckHere(centerx,centery,angle);
    break;
   }
 }


void chargeweapons(void)
{
 int     i, n;
 longint time;

 time=timecount;
 for(i=0;i<5;i++)
  {
   n=player.weapons[i];
   while (n!=-1 && weapons[n].charge<100 && time>=weapons[n].chargetime)
    {
     if (weapons[n].charge==0)
      weapons[n].chargetime=timecount;
     weapons[n].charge+=20;
     weapons[n].chargetime+=weapons[n].chargerate;
     }
   }
 }


/* timer access! ********************************************/
void ControlStub1(void)
{
 }

bool TryDoor(fixed_t xcenter, fixed_t ycenter)
{
 int       xl, yl, xh, yh, x, y;
 doorobj_t *door_p, *last_p;

 xl = (int)((xcenter-PLAYERSIZE) >> FRACTILESHIFT);
 yl = (int)((ycenter-PLAYERSIZE - (TILEUNIT>>1)) >> FRACTILESHIFT);
 xh = (int)((xcenter+PLAYERSIZE) >> FRACTILESHIFT);
 yh = (int)((ycenter+PLAYERSIZE - (TILEUNIT>>1)) >> FRACTILESHIFT);
// check for doors on the north wall
 for (y=yl+1;y<=yh;y++)
  for (x=xl;x<=xh;x++)
   {
    if (mapflags[y*MAPSIZE+x] & FL_DOOR) // if tile has a door
     {
      last_p=&doorlist[numdoors];
      for (door_p=doorlist; door_p != last_p; door_p++)
       if (door_p->tilex==x && door_p->tiley==y && (door_p->orientation==dr_horizontal || door_p->orientation==dr_horizontal2))
	{
	 if (door_p->doorOpen && !door_p->doorClosing) return true; // can move, door is open
	 else if (!door_p->doorOpen && door_p->doorBumpable && !door_p->doorOpening)
	  {
	   door_p->doorClosing=false;
	   door_p->doorOpening=true;
	   doorsound=true;
	   doorx=door_p->tilex<<FRACTILESHIFT;
	   doory=door_p->tiley<<FRACTILESHIFT;
	   if (door_p->orientation==dr_horizontal) TryDoor(xcenter+64*FRACUNIT,ycenter);
	    else TryDoor(xcenter-64*FRACUNIT,ycenter);
	   if (netmode) NetOpenDoor(xcenter,ycenter);
	   return false;
	   }
	 else if (!door_p->doorOpen && door_p->doorBumpable && door_p->doorClosing)
	  {
	   door_p->doorClosing=false;
	   door_p->doorOpening=true;
	   doorsound=true;
	   doorx=door_p->tilex<<FRACTILESHIFT;
	   doory=door_p->tiley<<FRACTILESHIFT;
	   if (door_p->orientation==dr_horizontal) TryDoor(xcenter+64*FRACUNIT,ycenter);
	    else TryDoor(xcenter-64*FRACUNIT,ycenter);
	   if (netmode) NetOpenDoor(xcenter,ycenter);
	   return false;
	   }
	 else return false;
	 }
      }
    }
// check for doors on the west wall
 xl = (int)((xcenter-PLAYERSIZE - (TILEUNIT>>1)) >> FRACTILESHIFT);
 yl = (int)((ycenter-PLAYERSIZE) >> FRACTILESHIFT);
 xh = (int)((xcenter+PLAYERSIZE - (TILEUNIT>>1)) >> FRACTILESHIFT);
 yh = (int)((ycenter+PLAYERSIZE) >> FRACTILESHIFT);
 for (y=yl;y<=yh;y++)
  for (x=xl+1;x<=xh;x++)
   {
    if (mapflags[y*MAPSIZE+x] & FL_DOOR) // if tile has a door
     {
      last_p=&doorlist[numdoors];
      for (door_p=doorlist; door_p != last_p; door_p++)
       if (door_p->tilex==x && door_p->tiley==y && (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2))
	{
	 if (door_p->doorOpen && !door_p->doorClosing) return true; // can move, door is open
	 else if (!door_p->doorOpen && door_p->doorBumpable && !door_p->doorOpening)
	  {
	   door_p->doorOpening=true;
	   door_p->doorClosing=false;
	   doorsound=true;
	   doorx=door_p->tilex<<FRACTILESHIFT;
	   doory=door_p->tiley<<FRACTILESHIFT;
	   if (door_p->orientation==dr_vertical) TryDoor(xcenter,ycenter+64*FRACUNIT);
	    else TryDoor(xcenter,ycenter-64*FRACUNIT);
	   if (netmode) NetOpenDoor(xcenter,ycenter);
	   return false;
	   }
	 else if (!door_p->doorOpen && door_p->doorBumpable && door_p->doorClosing)
	  {
	   door_p->doorClosing=false;
	   door_p->doorOpening=true;
	   doorsound=true;
	   doorx=door_p->tilex<<FRACTILESHIFT;
	   doory=door_p->tiley<<FRACTILESHIFT;
	   if (door_p->orientation==dr_vertical) TryDoor(xcenter,ycenter+64*FRACUNIT);
	    else TryDoor(xcenter,ycenter-64*FRACUNIT);
	   if (netmode) NetOpenDoor(xcenter,ycenter);
	   return false;
	   }
	 else return false;
	 }
      }
    }
 return true;
 }


bool TryMove(int angle,fixed_t xcenter, fixed_t ycenter)
{
 int       xl, yl, xh, yh, x, y, mapspot;
 fixed_t   pz;

 if (angle<NORTH || angle>SOUTH)
  {
   xl=xcenter>>FRACTILESHIFT;
   xh=(xcenter+PLAYERSIZE)>>FRACTILESHIFT;
   }
 else if (angle>NORTH && angle<SOUTH)
  {
   xh=xcenter>>FRACTILESHIFT;
   xl=(xcenter-PLAYERSIZE)>>FRACTILESHIFT;
   }
 else
  {
   xh=(xcenter+PLAYERSIZE)>>FRACTILESHIFT;
   xl=(xcenter-PLAYERSIZE)>>FRACTILESHIFT;
   }
 if (angle>WEST)
  {
   yl=ycenter>>FRACTILESHIFT;
   yh=(ycenter+PLAYERSIZE)>>FRACTILESHIFT;
   }
 else if (angle<WEST && angle!=EAST)
  {
   yl=(ycenter-PLAYERSIZE)>>FRACTILESHIFT;
   yh=ycenter>>FRACTILESHIFT;
   }
 else
  {
   yl=(ycenter-PLAYERSIZE)>>FRACTILESHIFT;
   yh=(ycenter+PLAYERSIZE)>>FRACTILESHIFT;
   }
 pz=player.z - player.height + (26<<FRACBITS);
// check for solid walls
 for (y=yl;y<=yh;y++)
  for (x=xl;x<=xh;x++)
   {
    mapspot=MAPCOLS*y+x;
    if ((y>yl && northwall[mapspot] && !(northflags[mapspot]&F_NOCLIP)) ||
	(x>xl && westwall[mapspot] && !(westflags[mapspot]&F_NOCLIP))) return false;
    if (mapspot!=player.mapspot)
     {
      if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) return false;
      if (RF_GetFloorZ((x<<FRACTILESHIFT)+(32<<FRACBITS),(y<<FRACTILESHIFT)+(32<<FRACBITS))>pz) return false;
      if (RF_GetCeilingZ((x<<FRACTILESHIFT)+(32<<FRACBITS),(y<<FRACTILESHIFT)+(32<<FRACBITS))<player.z+(10<<FRACBITS)) return false;
      }
    }
 return true;
 }


bool ClipMove(int angle,fixed_t xmove, fixed_t ymove)
{
 fixed_t dx, dy;
 int     angle2;

 dx=player.x+xmove;
 dy=player.y+ymove;
 if (TryMove(angle,dx,dy) && TryDoor(dx,dy))
  {
   if (floorpic[(dy>>FRACTILESHIFT)*MAPCOLS+(dx>>FRACTILESHIFT)]==0) return false;
   player.x+=xmove;
   player.y+=ymove;
   return true;
   }
// the move goes into a wall, so try and move along one axis
 if (xmove>0) angle2=EAST;
  else angle2=WEST;
 if (TryMove(angle2,dx,player.y) && TryDoor(dx,player.y))
  {
   if (floorpic[(player.y>>FRACTILESHIFT)*MAPCOLS+(dx>>FRACTILESHIFT)]==0)
    return false;
   player.x+=xmove;
   return true;
   }
 if (ymove>0) angle2=SOUTH;
  else angle2=NORTH;
 if (TryMove(angle2,player.x,dy) && TryDoor(player.x,dy))
  {
   if (floorpic[(dy>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT)]==0)
    return false;
   player.y+=ymove;
   return true;
   }
 return false;
 }


bool Thrust(int angle,fixed_t speed)
{
 fixed_t xmove, ymove;
 int     result;

 angle&=ANGLES;
 xmove=FIXEDMUL(speed,costable[angle]);
 ymove=-FIXEDMUL(speed,sintable[angle]);
 result=ClipMove(angle,xmove,ymove);
 player.mapspot=(player.y>>FRACTILESHIFT)*MAPCOLS+(player.x>>FRACTILESHIFT);
 return result;
 }


void ControlMovement (void)
{
 fixed_t    modifiedSpeed;
 int        modifiedTurn, modifiedMoveUnit, modifiedturnunit, n;
 fixed_t    floorz, fz, xl, yl, xh, yh, maxz;
 int        maxx, maxy, mapspot;

 if (Warping)
  {
   floorz=RF_GetFloorZ(player.x,player.y)+player.height;
   if (player.z>floorz)
    {
     fallrate+=MOVEUNIT;
     player.z-=fallrate;
     if (player.z<floorz) player.z=floorz;
     }
   else if (player.z<floorz)
    {
     player.z=floorz;
     fallrate=0;
     }
   return;
   }

//#ifndef DEMO
// if (keyboard[SC_G] && timecount>keyboardDelay && !netmsgstatus)
//  {
//   SaveTheScreen=true;
//   keyboardDelay=timecount+KBDELAY;
//   }
//#endif

 if (keyboard[SC_ESCAPE] && timecount>keyboardDelay && !netmsgstatus)
  activatemenu=true;

 if (keyboard[SC_F5] && keyboard[SC_LSHIFT] && timecount>keyboardDelay)
  {
   adjustvrangle=-SC.vrangle+DEFAULTVRANGLE;
   adjustvrdist=-1;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_F4] && keyboard[SC_LSHIFT] && timecount>keyboardDelay)
  {
   adjustvrdist=72090;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_F3] && keyboard[SC_LSHIFT] && timecount>keyboardDelay)
  {
   adjustvrdist=59578;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_F2] && keyboard[SC_LSHIFT] && timecount>keyboardDelay)
  {
   adjustvrangle=1;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_F1] && keyboard[SC_LSHIFT] && timecount>keyboardDelay)
  {
   adjustvrangle=-1;
   keyboardDelay=timecount+KBDELAY;
   }

 if (keyboard[SC_F1] && timecount>keyboardDelay)
  {
   activatehelp=true;
   keyboardDelay=timecount+KBDELAY;
   }

 if ((keyboard[SC_F4] || (keyboard[SC_ALT] && keyboard[SC_Q]))
  && timecount>keyboardDelay) QuickExit=true;

 if (keyboard[SC_F5] && timecount>keyboardDelay && !netmode)
  activatebrief=true;

 if (keyboard[SC_P] && timecount>keyboardDelay && !netmsgstatus)
  paused=true;

  /* change screen size */
 if (keyboard[SC_F9] && !resizeScreen && timecount>keyboardDelay)
  {
   resizeScreen=1;
   biggerScreen=1;
   keyboardDelay=timecount+KBDELAY;
   /* Was a bare <9.  MaxViewSize() instead, because ChangeViewSize can refuse
      the step -- in HD everything past MAXHDVIEWSIZE shrinks the view and
      cannot be applied -- and the setting must not drift past what is actually
      on screen, because SaveSetup persists it and newplayer restores the view
      from it. */
   if (SC.screensize<MaxViewSize()) SC.screensize++;
   return;
   }
 if (keyboard[SC_F10] && !resizeScreen && timecount>keyboardDelay)
  {
   resizeScreen=1;
   biggerScreen=0;
   keyboardDelay=timecount+KBDELAY;
   if (SC.screensize) SC.screensize--;
   return;
   }

 if (keyboard[SC_MINUS] && timecount>keyboardDelay && !netmsgstatus)
  {
   switch (MapZoom)
    {
     case 8:
      MapZoom=4;
      break;
     case 16:
      MapZoom=8;
      break;
     }
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_PLUS] && timecount>keyboardDelay && !netmsgstatus)
  {
   switch (MapZoom)
    {
     case 4:
      MapZoom=8;
      break;
     case 8:
      MapZoom=16;
      break;
     }
   keyboardDelay=timecount+KBDELAY;
   }

 /* SCROLLRATE is in view rows, so it has to scale with the view or looking up
    and down crawls at an HD height.  ViewScrollRate is SCROLLRATE at 200. */
 if (in_button[bt_lookup] && !netmsgstatus) scrollview-=ViewScrollRate();
 if (in_button[bt_lookdown] && !netmsgstatus) scrollview+=ViewScrollRate();
 if (in_button[bt_centerview] && !netmsgstatus) scrollview=255;

 if (scrollview==255)
  {
   if (player.scrollmin<0)
    {
     player.scrollmin+=ViewScrollRate();
     player.scrollmax+=ViewScrollRate();
     }
   else if (player.scrollmin>0)
    {
     player.scrollmin-=ViewScrollRate();
     player.scrollmax-=ViewScrollRate();
     }
   else scrollview=0;
   }

  /* display mode toggles */
 if (keyboard[SC_M] && timecount>keyboardDelay && !netmsgstatus)
  {
   togglemapmode=true;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_H] && timecount>keyboardDelay && !netmsgstatus)
  {
   toggleheatmode=true;
   keyboardDelay=timecount+KBDELAY;
   }
 /* C, not the DOS original's S: S is bt_south in this port's WASD defaults, so
    every step backwards toggled the motion sensor.  The A.S.S. cam moved off C
    to V to make room. */
 if (keyboard[SC_C] && timecount>keyboardDelay && !netmsgstatus)
  {
   togglemotionmode=true;
   keyboardDelay=timecount+KBDELAY;
   }
 if (in_button[bt_asscam] && timecount>keyboardDelay && !netmsgstatus)
  {
   ToggleRearView=true;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_TAB] && timecount>keyboardDelay && !netmsgstatus)
  {
   togglegoalitem=true;
   keyboardDelay=timecount+KBDELAY;
   }


 if (keyboard[SC_CAPSLOCK] && timecount>keyboardDelay)
  {
   toggleautorun=true;
   keyboardDelay=timecount+KBDELAY;
   }
 if (keyboard[SC_NUMLOCK] && timecount>keyboardDelay)
  {
   toggleautotarget=true;
   keyboardDelay=timecount+KBDELAY;
   }


  /* secrets */
 if (newascii)
  {
   secretbuf[secretindex++]=lastascii;
   if (secretindex>=19) specialcode=true;
   if (netmsgstatus==1) // getting message
    {
     switch (lastascii)
      {
       case 27:
	netmsgstatus=0;
	break;
       case 8:
	netmsg[netmsgindex]=' ';
	if (netmsgindex>0)
	  netmsgindex--;
	netmsg[netmsgindex]='_';
	break;
       case 13:
	netmsgstatus=2; // sending
	netmsg[netmsgindex]=' ';
	break;
       default:
	netmsg[netmsgindex]=lastascii;
	if (netmsgindex<29)
	 netmsgindex++;
	netmsg[netmsgindex]='_';
	break;
       }
     }
   newascii=false;
   secretdelay=timecount+KBDELAY*5;
   }
 if (timecount>secretdelay)
  {
   specialcode=true;
   secretdelay=timecount+KBDELAY*5;
   }

 if (keyboard[SC_F6] && netmode && netmsgstatus==0 && timecount>keyboardDelay)
  {
   memset(netmsg,0,sizeof(netmsg));
   netmsgstatus=1;
   netmsgindex=0;
   netmsg[0]='_';
   keyboardDelay=timecount+KBDELAY;
   }

 if (keyboard[SC_F7])
  newsong=true;

 if (in_button[bt_invright] && timecount>keyboardDelay)
  {
   goiright=true;
   keyboardDelay=timecount+KBDELAY;
   inventorytime=timecount+(3*70);
   }
 if (in_button[bt_invleft]  && timecount>keyboardDelay)
  {
   goileft=true;
   keyboardDelay=timecount+KBDELAY;
   inventorytime=timecount+(3*70);
   }

  /* he's dead jim! */
 if (player.angst==0)
  {

   if (floorpic[player.mapspot]>=57 && floorpic[player.mapspot]<=59)
    {
     if (player.z>RF_GetFloorZ(player.x,player.y)+(40<<FRACBITS))
      player.z-=FRACUNIT;
     else if (player.z<RF_GetFloorZ(player.x,player.y)+(40<<FRACBITS))
      player.z=RF_GetFloorZ(player.x,player.y)+(40<<FRACBITS);
     }
   else
    {
     if (player.z>RF_GetFloorZ(player.x,player.y)+(12<<FRACBITS))
      player.z-=FRACUNIT;
     else if (player.z<RF_GetFloorZ(player.x,player.y)+(12<<FRACBITS))
      player.z=RF_GetFloorZ(player.x,player.y)+(12<<FRACBITS);
     }
   if (keyboard[SC_SPACE]) deadrestart=true;
   return;
   }

 if (in_button[bt_useitem] && timecount>keyboardDelay)
  {
   useitem=true;
   keyboardDelay=timecount+KBDELAY;
   inventorytime=timecount+(3*70);
   }

  /* change weapon */
 if (keyboard[SC_1] && !changingweapons && player.currentweapon!=0 && !netmsgstatus)
  {
   changingweapons=true;
   weaponlowering=true;
   newweapon=0;
   }
 else if (keyboard[SC_2] && !changingweapons && player.currentweapon!=1 && player.weapons[1]!=-1 && !netmsgstatus)
  {
   changingweapons=true;
   weaponlowering=true;
   newweapon=1;
   }
 else if (keyboard[SC_3] && !changingweapons && player.currentweapon!=2 && player.weapons[2]!=-1 && !netmsgstatus)
  {
   changingweapons=true;
   weaponlowering=true;
   newweapon=2;
   }
 else if (keyboard[SC_4] && !changingweapons && player.currentweapon!=3 && player.weapons[3]!=-1 && !netmsgstatus)
  {
   changingweapons=true;
   weaponlowering=true;
   newweapon=3;
   }
 else if (keyboard[SC_5] && !changingweapons && player.currentweapon!=4 && player.weapons[4]!=-1 && !netmsgstatus)
  {
   changingweapons=true;
   weaponlowering=true;
   newweapon=4;
   }

 if (in_button[bt_jump] && timecount>keyboardDelay && fallrate==0 && !netmsgstatus)
  {
   fallrate-=FALLUNIT*9+player.jumpmod;
   keyboardDelay=timecount+KBDELAY;
   }

  /* check run/slow keys */
 if (in_button[bt_run] || autorun)
  {
   modifiedSpeed=(int)((PLAYERMOVESPEED)*6 + player.runmod);
   modifiedTurn=(int)(playerturnspeed*2.5);
   modifiedMoveUnit=MOVEUNIT*2;
   modifiedturnunit=turnunit;
   }
 else
  {
   modifiedSpeed=(int)((PLAYERMOVESPEED)*3.5 + player.walkmod);
   modifiedTurn=playerturnspeed;
   modifiedMoveUnit=MOVEUNIT;
   modifiedturnunit=turnunit*2;
   }

 floorz=RF_GetFloorZ(player.x,player.y)+player.height;
 if (floorpic[player.mapspot]>=57 && floorpic[player.mapspot]<=59)
  {
   if (player.z==floorz)
    modifiedSpeed>>=1;
   }

  /* check strafe */
 if ((in_button[bt_straf] || in_button[bt_slideleft] || in_button[bt_slideright]) && !netmsgstatus)
  {
   if (in_button[bt_west] || in_button[bt_slideleft])
    {
     strafrate-=modifiedMoveUnit;
     if (strafrate<-modifiedSpeed) strafrate+=modifiedMoveUnit;
     }
   if (in_button[bt_east] || in_button[bt_slideright])
    {
     strafrate+=modifiedMoveUnit;
     if (strafrate>modifiedSpeed) strafrate-=modifiedMoveUnit;
     }
   else if (!in_button[bt_west] && !in_button[bt_slideleft])
    {
     if (strafrate<0) strafrate+=MOVEUNIT;
     else if (strafrate>0) strafrate-=MOVEUNIT;
     }
   }
 else
  {
   if (strafrate<0) strafrate+=MOVEUNIT;
    else if (strafrate>0) strafrate-=MOVEUNIT;

   /* not strafing */
   if (in_button[bt_east])
    {
     turnrate-=modifiedturnunit;
     if (turnrate<-modifiedTurn)
      turnrate=-modifiedTurn;
     player.angle+=turnrate;
     }
   else if (in_button[bt_west])
    {
     turnrate+=modifiedturnunit;
     if (turnrate>modifiedTurn)
      turnrate=modifiedTurn;
     player.angle+=turnrate;
     }
   else
    {
     if (turnrate<0)
      {
       turnrate+=modifiedturnunit;
       if (turnrate>0)
	turnrate=0;
       }
     else if (turnrate>0)
      {
       turnrate-=modifiedturnunit;
       if (turnrate<0)
	turnrate=0;
       }
     player.angle+=turnrate;
     }
   player.angle&=ANGLES;
   }

 /* Mouse look.  The DOS build fed mouse motion through in_button[bt_east] and
    friends (see INT_ReadControls in source_dos/CODE/D_INTS.C), quantising it
    into whole turn-key presses -- which feels like the view is ratcheting.
    Applying the delta straight to player.angle instead gives smooth aiming.
    Vertical goes through scrollview, the same path bt_lookup/bt_lookdown use,
    so ChangeScroll's existing clamping still applies. */
 {
  int mlx, mly, sens;

  Sys_GetMouseLook(&mlx,&mly);
  if (mlx || mly)
   {
    sens=SC.mousesensitivity;
    if (sens<1) sens=1;

    /* mousesensitivity defaults to 32; scale so that is a comfortable feel. */
    player.angle-=(mlx*sens)/24;
    player.angle&=ANGLES;

    /* 255 is bt_centerview's sentinel, not a delta -- don't disturb it, and
       don't let an accumulated delta land on it by accident. */
    if (scrollview!=255)
     {
      /* Also view rows: the same wrist movement must cover the same
	 fraction of the view at any render resolution. */
      scrollview-=(mly*sens*windowHeight)/(96*INIT_VIEW_HEIGHT);
      if (scrollview==255) scrollview=254;
      }
    }
  }

 if (strafrate<0)
  {
   if (!Thrust(player.angle+NORTH,-strafrate))
    {
     moverate=0;
     strafrate=0;
     }
   }
 else if (strafrate>0)
  {
   if (!Thrust(player.angle+SOUTH,strafrate))
    {
     moverate=0;
     strafrate=0;
     }
   }

  /* forward/backwards move */
 if (in_button[bt_north]) moveforward=1;
  else if (in_button[bt_south]) moveforward=-1;
  else moveforward=0;

  /* compute move vectors */
 if (moveforward==1)
  {
   if (moverate<modifiedSpeed) moverate+=modifiedMoveUnit;
   if (moverate>modifiedSpeed) moverate-=modifiedMoveUnit;
   }
 else if (moveforward==-1)
  {
   if (moverate>-modifiedSpeed) moverate-=modifiedMoveUnit;
   if (moverate<-modifiedSpeed) moverate+=modifiedMoveUnit;
   }
 else if (moverate!=0)
  {
   if (moverate<0) moverate+=MOVEUNIT;
    else moverate-=MOVEUNIT;
   }

  /* move along move vector & compute head bobbing */
 if (moverate<0)
  {
   if (headbob==MAXBOBS-1) headbob=0;
    else ++headbob;
   if (wbobcount==4)
    {
     wbobcount=0;
     if (weapbob==MAXBOBS-1) weapbob=0;
      else ++weapbob;
     }
   else ++wbobcount;
   if (!Thrust(player.angle+WEST,-moverate)) moverate=0;
   }
 else if (moverate>0)
  {
   if (headbob==MAXBOBS-1) headbob=0;
    else ++headbob;
   if (wbobcount==4)
    {
     wbobcount=0;
     if (weapbob==MAXBOBS-1) weapbob=0;
      else ++weapbob;
     }
   else ++wbobcount;
   if (!Thrust(player.angle,moverate)) moverate=0;
   }
 else if (timecount&8)
  {
   if (weapmove[weapbob]!=0)
    {
     if (abs(weapmove[weapbob-1])<abs(weapmove[weapbob])) --weapbob;
     else
      {
       ++weapbob;
       if (weapbob==MAXBOBS) weapbob=0;
       }
     }
   if (headmove[headbob]!=0)
    {
     if (abs(headmove[headbob-1])<abs(headmove[headbob])) --headbob;
     else
      {
       ++headbob;
       if (headbob==MAXBOBS) headbob=0;
       }
     }
   }

  /* try to open a door in front of player */
 if (in_button[bt_use] && timecount>keyboardDelay && !netmsgstatus)
  {
   checktrigger=true;
   keyboardDelay=timecount+KBDELAY*2;
   }

  /* fire a weapon */
 if (in_button[bt_fire] && weapons[player.weapons[player.currentweapon]].charge==100 && !changingweapons)
  {
   n=player.weapons[player.currentweapon];
   if (n==18 || n==4)
    {
     if (player.ammo[weapons[n].ammotype]>=weapons[n].ammorate && weapmode==0) weapmode=1;
     }
   else RF_SetActionHook(fireweapon);
   }

 /* compute falling or stepping up higher */
 xl=(player.x-(FRACUNIT*8))>>FRACTILESHIFT;
 xh=(player.x+(FRACUNIT*8))>>FRACTILESHIFT;
 yl=(player.y-(FRACUNIT*8))>>FRACTILESHIFT;
 yh=(player.y+(FRACUNIT*8))>>FRACTILESHIFT;
 floorz=player.z-player.height;
 maxz=0;
 for(;xl<=xh;xl++)
  for(;yl<=yh;yl++)
   {
    fz=RF_GetFloorZ((xl*64+32)<<FRACBITS,(yl*64+32)<<FRACBITS);
    if (fz>maxz && fz<floorz+(20<<FRACBITS))
     {
      maxz=fz;
      maxx=xl;
      maxy=yl;
      }
    }
 if (maxz==0)
  {
   maxz=RF_GetFloorZ(player.x,player.y);
   maxx=player.x>>FRACTILESHIFT;
   maxy=player.y>>FRACTILESHIFT;
   }
 floorz=maxz + player.height;

 if (abs(player.z-floorz)<=10<<FRACBITS)
  {
   mapspot=maxy*MAPCOLS + maxx;
   if (floorflags[mapspot]&F_RIGHT)
    Thrust(EAST,FRACUNIT*4);
   if (floorflags[mapspot]&F_LEFT)
    Thrust(WEST,FRACUNIT*4);
   if (floorflags[mapspot]&F_UP)
    Thrust(NORTH,FRACUNIT*4);
   if (floorflags[mapspot]&F_DOWN)
    Thrust(SOUTH,FRACUNIT*4);
   }

// floorz=RF_GetFloorZ(player.x,player.y)+player.height;

 player.z-=fallrate;
 if (player.z>floorz) fallrate+=FALLUNIT;
 else if (player.z<floorz)
  {
   if (fallrate>=12*FRACUNIT) falldamage=(fallrate>>FRACBITS)/7;
   player.z+=FRACUNIT<<2;
   if (player.z>floorz) player.z=floorz;
   fallrate=0;
   }
 floorz=RF_GetCeilingZ(player.x,player.y);
 if (player.z+(10<<FRACBITS)>floorz)
  {
   player.z=floorz-(10<<FRACBITS);
   fallrate=FALLUNIT;
   }
 }


void PlayerCommand(void)
/* called by an interrupt */
{
 INT_ReadControls();
 ControlMovement();
 }


void newlights(void)
{
 if (lighting+changelight>4096) lighting=4096;
  else lighting+=changelight;
 if (lighting<=0) lighting=1;
 RF_SetLights((fixed_t)lighting<<FRACBITS);
 changelight=0;
 }


void ChangeScroll(void)
{
 if (scrollview==255) return;
 if (player.scrollmin+scrollview<=-viewscroll || player.scrollmin+scrollview>=viewscroll)
  {
   scrollview=0;
   return;
   }
 player.scrollmin+=scrollview;
 player.scrollmax+=scrollview;
 scrollview=0;
 }


void Special_Code(char *s)
/* secrets */
{
 scaleobj_t *hsprite_p, *sprite_p;
 int        i;

 if (netmode && !MS_CheckParm("ravenger"))
  {
   specialcode=false;
   memset(secretbuf,0,sizeof(secretbuf));
   secretindex=0;
   return;
   }
 if (stricmp(s,"belfast")==0)
  {
   for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
    if (sprite_p->hitpoints)
     {
      mapsprites[(sprite_p->y>>FRACTILESHIFT)*MAPCOLS+(sprite_p->x>>FRACTILESHIFT)]=0;
      hsprite_p=sprite_p;
      sprite_p=sprite_p->prev;
      KillSprite(hsprite_p,S_BULLET3);
      player.bodycount++;
      }
   writemsg("DeathKiss");
   }
 else if (stricmp(s,"allahmode")==0)
  {
   if (godmode)
    {
     godmode=false;
     writemsg("GodMode Off");
     }
   else
    {
     godmode=true;
     writemsg("GodMode On");
     }
   }
 else if (stricmp(s,"channel7")==0)
  {
   writemsg("Rob Lays Eggs");
   }
 else if (stricmp(s,"lizardman")==0)
  {
   writemsg("Jeremy Lays Eggs");
   }
 else if (stricmp(s,"dominatrix")==0)
  {
   writemsg("On your knees worm!");
   }
 else if (stricmp(s,"cyborg")==0)
  {
   writemsg("Psyborgs Rule!");
   }
 else if (stricmp(s,"mooman")==0)
  {
   writemsg("Brady is better than you, and that ain't saying much!");
   }
 else if (stricmp(s,"raven")==0)
  {
   player.angst=player.maxangst;
   player.shield=player.maxshield;
   writemsg("Ambrosia");
   }
 else if (stricmp(s,"omni")==0)
  {
   for(i=0;i<MAPCOLS*MAPROWS;i++)
    if (northwall[i] & 255) player.northmap[i]=WALL_COLOR;
   for(i=0;i<MAPCOLS*MAPROWS;i++)
    if (westwall[i] & 255) player.westmap[i]=WALL_COLOR;
   writemsg("Omniscience");
   }
 else if (stricmp(s,"kmfdm")==0)
  {
   player.ammo[0]=999;
   player.ammo[1]=999;
   player.ammo[2]=999;
   writemsg("Backpack of Holding");
   oldshots=-1;
   }
 else if (stricmp(s,"beavis")==0)
  {
   player.levelscore=100;
   player.primaries[0]=pcount[0];
   player.primaries[1]=pcount[1];
   for (i=0;i<7;i++)
    player.secondaries[i]=scount[i];
   writemsg("Time Warp");
   }
 else if (stricmp(s,"gulliver")==0)
  {
   if (midgetmode)
    {
     for (sprite_p=firstscaleobj.next;sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
      sprite_p->scale--;
     midgetmode=false;
     writemsg("Midget Mode Off");
     }
   else
    {
     for (sprite_p=firstscaleobj.next;sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
      sprite_p->scale++;
     midgetmode=true;
     writemsg("Midget Mode On");
     }
   }
 else if (stricmp(s,"gimme")==0)
  {
   player.inventory[0]=20;
   player.inventory[1]=20;
   player.inventory[2]=15;
   player.inventory[3]=10;
   player.inventory[4]=10;
   player.inventory[5]=10;
   player.inventory[6]=10;
   player.inventory[7]=10;
   player.inventory[8]=10;
#ifndef DEMO
   player.inventory[9]=10;
   player.inventory[10]=10;
   player.inventory[11]=10;
   player.inventory[12]=10;
#endif
   writemsg("Bag of Holding");
   }
 else if (stricmp(s,"taco")==0)
  {
   enemyviewmode^=1;
   writemsg("Enemy view toggled");
   }
#if !defined(DEMO) && !defined(GAME1) && !defined(GAME2) && !defined(GAME3)
 if (s[0]=='g' && s[1]=='o')
  {
   if (stricmp(s,"go1")==0) newmap(0,2);
    else if (stricmp(s,"go2")==0) newmap(1,2);
    else if (stricmp(s,"go3")==0) newmap(2,2);
    else if (stricmp(s,"go4")==0) newmap(3,2);
    else if (stricmp(s,"go5")==0) newmap(4,2);
    else if (stricmp(s,"go6")==0) newmap(5,2);
    else if (stricmp(s,"go7")==0) newmap(6,2);
    else if (stricmp(s,"go8")==0) newmap(7,2);
    else if (stricmp(s,"go9")==0) newmap(8,2);
    else if (stricmp(s,"go10")==0) newmap(9,2);
    else if (stricmp(s,"go11")==0) newmap(10,2);
    else if (stricmp(s,"go12")==0) newmap(11,2);
    else if (stricmp(s,"go13")==0) newmap(12,2);
    else if (stricmp(s,"go14")==0) newmap(13,2);
    else if (stricmp(s,"go15")==0) newmap(14,2);
    else if (stricmp(s,"go16")==0) newmap(15,2);
    else if (stricmp(s,"go17")==0) newmap(16,2);
    else if (stricmp(s,"go18")==0) newmap(17,2);
    else if (stricmp(s,"go19")==0) newmap(18,2);
    else if (stricmp(s,"go20")==0) newmap(19,2);
    else if (stricmp(s,"go21")==0) newmap(20,2);
    else if (stricmp(s,"go22")==0) newmap(21,2);
    else if (stricmp(s,"go23")==0) newmap(22,2);
    else if (stricmp(s,"go24")==0) newmap(23,2);
    else if (stricmp(s,"go25")==0) newmap(24,2);
    else if (stricmp(s,"go26")==0) newmap(25,2);
    else if (stricmp(s,"go27")==0) newmap(26,2);
    else if (stricmp(s,"go28")==0) newmap(27,2);
    else if (stricmp(s,"go29")==0) newmap(28,2);
    else if (stricmp(s,"go30")==0) newmap(29,2);
    else if (stricmp(s,"go31")==0) newmap(30,2);
    else if (stricmp(s,"go32")==0) newmap(31,2);
   INT_TimerHook(PlayerCommand);
   }
#endif
 else if (s[0]=='b' && s[1]=='l')
  {
   if (stricmp(s,"blammo1")==0) player.weapons[2]=2;
    else if (stricmp(s,"blammo2")==0) player.weapons[2]=3;
    else if (stricmp(s,"blammo3")==0) player.weapons[2]=4;
    else if (stricmp(s,"blammo4")==0) player.weapons[2]=16;
    else if (stricmp(s,"blammo5")==0) player.weapons[2]=17;
    else if (stricmp(s,"blammo6")==0) player.weapons[2]=18;
   if (player.weapons[2]>=0)
    {
     loadweapon(player.weapons[2]);
     player.currentweapon=2;
     weapmode=0;
     }
   }
 specialcode=false;
 memset(secretbuf,0,sizeof(secretbuf));
 secretindex=0;
 }


void CheckSpawnAreas(void)
{
 spawnarea_t *sa;
 int         i, count, type, stype;
 scaleobj_t  *sprite_p;

 if (specialeffect==SE_WARPJAMMER) return;
 if (netwarpjammer && netwarpjamtime>(int)timecount) return;

 sa=spawnareas;
 for(i=0;i<numspawnareas;i++,sa++)
  if (timecount>=sa->time)
   {
    if (mapsprites[sa->mapspot]==0 && sa->mapspot!=player.mapspot)
     {
      switch (sa->type)
       {
	case 0:
	 if (!netmode)
	  {
	   #ifdef DEMO
	    type=(clock()+MS_RndT())%110;
	   #else
	    type=(clock()+MS_RndT())%114;
	   #endif
	   if (type<30) stype=S_ENERGY;
	    else if (type<60) stype=S_BALLISTIC;
	    else if (type<90) stype=S_PLASMA;
	   #ifdef DEMO
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IINSTAWALL;
	    else stype=S_ICLONE;
	   #else
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IINSTAWALL;
	    else if (type<110) stype=S_ICLONE;
	    else if (type<112) stype=S_IJAMMER;
	    else stype=S_ISTEALER;
	   #endif
	   sa->time=timecount+(clock()&255) + 3500 - (350*(player.difficulty+1));
	   }
	 else
	  {
	   #ifdef DEMO
	    type=(clock()+MS_RndT())%110;
	   #else
	    type=(clock()+MS_RndT())%146;
	   #endif
	   if (type<30) stype=S_ENERGY;
	    else if (type<60) stype=S_BALLISTIC;
	    else if (type<90) stype=S_PLASMA;
	   #ifdef DEMO
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IDECOY;
	    else stype=S_IINSTAWALL;
	   #else
	    else if (type<98) stype=S_IGRENADE;
	    else if (type<102) stype=S_IREVERSO;
	    else if (type<112) stype=S_IPROXMINE;
	    else if (type<116) stype=S_ITIMEMINE;
	    else if (type<120) stype=S_IDECOY;
	    else if (type<134) stype=S_IINSTAWALL;
	    else if (type<138) stype=S_IINVIS;
	    else if (type<142) stype=S_ISTEALER;
	    else stype=S_IHOLO;
	   #endif
	   sa->time=timecount + (clock()&255) + (9-greedcom->numplayers)*437;
	   }
	 break;
	case 1:
	 if (!netmode)
	  {
	   #ifdef DEMO
	    type=(clock()+MS_RndT())%110;
	   #else
	    type=(clock()+MS_RndT())%114;
	   #endif
	   if (type<15) stype=S_MEDPAK1;
	    else if (type<22) stype=S_MEDPAK2;
	    else if (type<30) stype=S_MEDPAK3;
	    else if (type<45) stype=S_MEDPAK4;
	    else if (type<60) stype=S_SHIELD4;
	    else if (type<67) stype=S_SHIELD3;
	    else if (type<75) stype=S_SHIELD2;
	    else if (type<90) stype=S_SHIELD1;
	   #ifdef DEMO
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IINSTAWALL;
	    else stype=S_ICLONE;
	   #else
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IINSTAWALL;
	    else if (type<110) stype=S_ICLONE;
	    else if (type<112) stype=S_IJAMMER;
	    else stype=S_ISTEALER;
	   #endif
	   sa->time=timecount+(clock()&255) + 3500 - (350*(player.difficulty+1));
	   }
	 else
	  {
	   #ifdef DEMO
	    type=(clock()+MS_RndT())%110;
	   #else
	    type=(clock()+MS_RndT())%116;
	   #endif
	   if (type<15) stype=S_MEDPAK1;
	    else if (type<22) stype=S_MEDPAK2;
	    else if (type<30) stype=S_MEDPAK3;
	    else if (type<45) stype=S_MEDPAK4;
	    else if (type<60) stype=S_SHIELD4;
	    else if (type<67) stype=S_SHIELD3;
	    else if (type<75) stype=S_SHIELD2;
	    else if (type<90) stype=S_SHIELD1;
	   #ifdef DEMO
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IDECOY;
	    else stype=S_IINSTAWALL;
	   #else
	    else if (type<96) stype=S_IGRENADE;
	    else if (type<98) stype=S_IREVERSO;
	    else if (type<102) stype=S_IPROXMINE;
	    else if (type<106) stype=S_ITIMEMINE;
	    else if (type<108) stype=S_IDECOY;
	    else if (type<110) stype=S_IINSTAWALL;
	    else if (type<110) stype=S_IINVIS;
	    else if (type<112) stype=S_IJAMMER;
	    else if (type<114) stype=S_ISTEALER;
	    else stype=S_IHOLO;
	   #endif
	   sa->time=timecount + (clock()&255) + (9-greedcom->numplayers)*437;
	   }
	 break;
	case 10:
	 stype=S_MONSTER1;
	 sa->time=timecount+(clock()&255) + (2100*(player.difficulty+1));
	 break;
	case 11:
	 stype=S_MONSTER2;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 12:
	 stype=S_MONSTER3;
	 sa->time=timecount+(clock()&255) + (2100*(player.difficulty+1));
	 break;
	case 13:
	 stype=S_MONSTER4;
	 sa->time=timecount+(clock()&255) + (10500*(player.difficulty+1));
	 break;
	case 14:
	 stype=S_MONSTER5;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 15:
	 stype=S_MONSTER6;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 16:
	 stype=S_MONSTER7;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 17:
	 stype=S_MONSTER8;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 18:
	 stype=S_MONSTER9;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 19:
	 stype=S_MONSTER10;
	 sa->time=timecount+(clock()&255) + (1200*(player.difficulty+1));
	 break;
	case 20:
	 stype=S_MONSTER11;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 21:
	 stype=S_MONSTER12;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 22:
	 stype=S_MONSTER13;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 23:
	 stype=S_MONSTER14;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 24:
	 stype=S_MONSTER15;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;

	case 100:
	 stype=S_MONSTER8_NS;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	case 101:
	 stype=S_MONSTER9_NS;
	 sa->time=timecount+(clock()&255) + (4200*(player.difficulty+1));
	 break;
	}

      if (sa->type>=10)
       {
	count=0;
	for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
	 if (sprite_p->type==stype && sprite_p->hitpoints) ++count;
	}
      else count=0;

      if (count<MAXSPAWN)
       {
	for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
	 if ((sprite_p->type==S_GENERATOR || (sprite_p->type>=S_GENSTART && sprite_p->type<=S_GENEND))
	  && sprite_p->x==sa->mapx && sprite_p->y==sa->mapy)
	  {
	   sprite_p=sprite_p->prev;
	   RF_RemoveSprite(sprite_p->next);
	   }
	if (sa->type>=10)
	 for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
	  if (sprite_p->type==stype && sprite_p->hitpoints==0)
	   {
	    RF_RemoveSprite(sprite_p);
	    break;
	    }
	if (!netmode || (netmode && playernum==0))
	 {
	  SpawnSprite(stype,sa->mapx,sa->mapy,0,0,0,0,true,0);
	  SpawnSprite(S_WARP,sa->mapx,sa->mapy,0,0,0,0,true,0);
	  if (netmode && sa->type>=10)
	   NetSendSpawn(stype,sa->mapx,sa->mapy,0,0,0,0,true,0);
	  }
	}
      }
    else sa->time=timecount+(clock()&255) + 7000;
    }
 }


void CheckBonusItem(void)
{
 scaleobj_t *sprite;

 if (timecount>BonusItem.time)
  {
   if (netmode && playernum!=0) return; // player 0 spawns the bonuses

   if (BonusItem.score>0)
    {
     for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;sprite=sprite->next)
      if (sprite->type==S_BONUSITEM)
       {
	RF_RemoveSprite(sprite);
	mapsprites[BonusItem.mapspot]=0;
	break;
	}
      SpawnSprite(S_WARP,(BonusItem.tilex*MAPSIZE+32)<<FRACBITS,(BonusItem.tiley*MAPSIZE+32)<<FRACBITS,0,0,0,0,false,0);
     }
   do
    {
     BonusItem.tilex=(clock()+MS_RndT())&63;
     BonusItem.tiley=(clock()+MS_RndT())&63;
     BonusItem.mapspot=BonusItem.tiley*MAPCOLS + BonusItem.tilex;
     } while (floorpic[BonusItem.mapspot]==0 || mapsprites[BonusItem.mapspot] || mapeffects[BonusItem.mapspot]&FL_FLOOR
	|| floorheight[BonusItem.mapspot]==ceilingheight[BonusItem.mapspot]);
   BonusItem.score=2000 + (clock()&7)*300;
   BonusItem.time=timecount + bonustime + (clock()&1023);
   BonusItem.num=clock()%MAXRANDOMITEMS;
   BonusItem.name=randnames[BonusItem.num];
   BonusItem.sprite=SpawnSprite(S_BONUSITEM,(BonusItem.tilex*MAPSIZE+32)<<FRACBITS,(BonusItem.tiley*MAPSIZE+32)<<FRACBITS,0,0,0,0,false,0);
   SpawnSprite(S_WARP,(BonusItem.tilex*MAPSIZE+32)<<FRACBITS,(BonusItem.tiley*MAPSIZE+32)<<FRACBITS,0,0,0,0,false,0);
   BonusItem.sprite->basepic+=BonusItem.num;
   oldgoalitem=-1;
   if (netmode) NetBonusItem();
   goalitem=0;
   }
 }


void TimeUpdate(void)
{
	longint	time;

	/* Where the Win32 message pump lived.  Sys_Frame also advances the 35 Hz
	   clock that timecount below depends on. */
	Sys_Frame();

 chargeweapons();
 UpdateMouse();
 if (netmode) NetGetData();
 if (netmode)
  {
   NetGetData();
   if (timecount>netsendtime)
    {
     if (player.angst) NetSendPlayerData();
     netsendtime=timecount + 3 + greedcom->numplayers;
     }
   NetGetData();
   }
 time=timecount;
 UpdateSound();
 while (time>=spritemovetime)
  {
   if (netmode) NetGetData();
   if (numprocesses)
    {
     Process();
     if (netmode) NetGetData();
     }

//   if (recording || playback) rndofs=0;

   memset(reallight,0,MAPROWS*MAPCOLS*4);
   MoveSprites();

   if (netmode)
    {
     NetGetData();
     spritemovetime+=2;
     }

   spritemovetime+=8;
   }
 UpdateMouse();
 if (netmode) NetGetData();
 if (numspawnareas) CheckSpawnAreas();
 if (netmode) NetGetData();
 CheckElevators();
 if (netmode) NetGetData();
 CheckBonusItem();
 if (netmode) NetGetData();
 if (doorsound)
  {
   doorsound=false;
   SoundEffect(SN_DOOR,15,doorx,doory);
   }
 if (netmode) NetGetData();
 UpdateMouse();
 }


extern pevent_t playerdata[MAXPLAYERS];


void RearView(void)
{
 int scrollmin1, scrollmax1, view, savedhd;
 intptr_t location;

 view=currentViewSize*2;
 location=viewLocation;
 /* The camera is a 64x64 square shown as a square, so it wants the plain
    square projection whatever the main view is using. */
 savedhd=hdmode;
 hdmode=0;
 windowWidth=64;
 windowHeight=64;
 windowLeft=0;
 windowTop=0;
 windowSize=4096;
 viewLocation=(intptr_t)screen;
 scrollmin1=player.scrollmin;
 scrollmax1=player.scrollmax;
 SetViewSize(windowWidth,windowHeight);
 ResetScalePostWidth(windowWidth);
 scrollmin=0;
 scrollmax=64;
 memcpy(pixelangle,campixelangle,sizeof(pixelangle));
 memcpy(pixelcosine,campixelcosine,sizeof(pixelcosine));
 if (enemyviewmode && goalitem>0)
  RF_RenderView(playerdata[goalitem-1].x,playerdata[goalitem-1].y,playerdata[goalitem-1].z,playerdata[goalitem-1].angle);
 else
  RF_RenderView(player.x,player.y,player.z,player.angle+WEST);
 memcpy(rearbuf,viewbuffer,sizeof(rearbuf));
 windowLeft=viewLoc[view];
 windowTop=viewLoc[view+1];
 viewLocation=location;
 hdmode=savedhd;
 /* Back to the real view size -- in HD that is not an entry in viewSizes[]. */
 SetViewSize(mainViewWidth,mainViewHeight);
 ResetScalePostWidth(windowWidth);
 memcpy(pixelangle,wallpixelangle,sizeof(pixelangle));
 memcpy(pixelcosine,wallpixelcosine,sizeof(pixelcosine));
 player.scrollmin=scrollmin1;
 player.scrollmax=scrollmax1;
 }


void NewGoalItem(void)
{
 togglegoalitem=false;
 goalitem++;

 if (!netmode)
  {
   if (goalitem==0 && BonusItem.score==0) goalitem++;
   while (goalitem>=1 && goalitem<=2 && primaries[(goalitem-1)*2]==-1) goalitem++;
   while (goalitem>=3 && goalitem<=9 && secondaries[(goalitem-3)*2]==-1) goalitem++;
   if (goalitem>=10)
    {
     goalitem=0;
     if (goalitem==0 && BonusItem.score==0) goalitem++;
     while (goalitem>=1 && goalitem<=2 && primaries[(goalitem-1)*2]==-1) goalitem++;
     while (goalitem>=3 && goalitem<=9 && secondaries[(goalitem-3)*2]==-1) goalitem++;
     if (goalitem==10) goalitem=-1;
     }
   }
 else
  {
   if (goalitem==0 && BonusItem.score==0) goalitem++;
   if (goalitem>greedcom->numplayers)
    {
     goalitem=0;
     if (goalitem==0 && BonusItem.score==0) goalitem++;
     if (goalitem>greedcom->numplayers)
      goalitem=-1;
     }
   }
 }


void GrabTheScreen(void)
{
 FILE *f;
 char name[15];
 byte palette[768];
 static int count=0;

 if (MS_CheckParm("GRAB"))
  {
   sprintf(name,"grab%i.raw",count);
   ++count;
   f=fopen(name,"wb");
   if (f==NULL)
    MS_Error("Error opening the screen grab file!");
   fwrite((char *)0xa0000,64000,1,f);
   VI_GetPalette(palette);
   fwrite(palette,768,1,f);
   fclose(f);
   }
 SaveTheScreen=false;
 }


void startover(int restartvalue)
{
 int  i;

 if (netmode) NetGetData();
 resetdisplay();
 if (restartvalue!=1)
  {
   player.score=0;
   player.levelscore=levelscore;
   player.weapons[2]=-1;
   player.weapons[3]=-1;
   player.weapons[4]=-1;
   player.currentweapon=0;
   loadweapon(player.weapons[0]);

   if (!netmode)
    {
     memset(player.inventory,0,sizeof(player.inventory));
     player.inventory[7]=2;
     player.inventory[5]=2;
     player.inventory[4]=2;
     player.inventory[2]=4;
     }
   else
    {
     if (player.inventory[7]<2)
      player.inventory[7]=2;
     if (player.inventory[5]<2)
      player.inventory[5]=2;
     if (player.inventory[4]<2)
      player.inventory[4]=2;
     if (player.inventory[2]<4)
      player.inventory[2]=4;
     }
   player.bodycount=0;
   player.ammo[0]=100;
   player.ammo[1]=100;
   player.ammo[2]=100;
   player.angst=player.maxangst;
   player.shield=200;
   }
 player.holopic=0;

 if (!netmode)
  {
   newmap(player.map,restartvalue);
   INT_TimerHook(PlayerCommand);
   }
 else
  respawnplayer();

 if (netmode) NetGetData();

 exitexists=false;
 specialeffecttime=0;
 ExitLevel=false;
 if (currentViewSize>=5)
  VI_DrawPic(4,149,statusbar[2]);
 if (currentViewSize>=4)
  VI_DrawMaskedPic(0,0,statusbar[3]);
 if (netmode) NetGetData();
 turnrate=0;
 moverate=0;
 fallrate=0;
 strafrate=0;
 deadrestart=false;
 player.primaries[0]=0;
 player.primaries[1]=0;
 for(i=0;i<7;i++)
  player.secondaries[i]=0;
 }


void EndLevel(void)
{
 VI_FadeOut(0,256,0,0,0,64);
 memset(screen,0,SCREENBYTES);
 VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));
 ++player.map;
 startover(1);
 }


void WarpAnim(void)
{
 if (Warping==1)
  {
   CA_ReadLump(CA_GetNamedNum("WARPLIGHTS"),colormaps);
   Warping=2;
   }
 else if (Warping==2)
  {
   if (lighting>=128) changelight=-128;
   else
    {
     Warping=3;
     player.x=WarpX;
     player.y=WarpY;
     }
   }
 else if (Warping==3)
  {
   if (lighting<SC.ambientlight) changelight=128;
   else
    {
     CA_ReadLump(CA_GetNamedNum("LIGHTS"),colormaps);
     Warping=0;
     }
   }
 }


void DrawHolo(void)
{
 int i, j, count, bottom, top, x, y;
 byte       *collumn;
 scalepic_t *spic;

 spic=lumpmain[player.holopic]; // draw the pic for it
 x=5*hudscale;
 for (i=0;i<SP_HUDLEN(SP_WIDTH(spic));i++,x++)
  {
   int src=SP_HUDSRC(i);
   if (!SP_COLOFS(spic,src))
    continue;
   collumn=(byte *)spic+SP_COLOFS(spic,src);
   top=SP_BOTTOM(collumn);
   bottom=SP_TOP(collumn);
   count=SP_HUDLEN(bottom-top+1);
   collumn+=SP_COLHDR;
   y=hudHeight-SP_HUDLEN(top)-count-5*hudscale;
   for (j=0;j<count;j++,y++)
    {
     byte v=collumn[SP_HUDSRC(j)];
     if (y>=0 && v) VI_HudPut(hudylookup,x,y,v);
     }
   }
 }


void RunMenu(void)
{
 player.timecount=timecount;
 Sys_SetMouseLook(0);       /* release the pointer so the menu is clickable */
 ShowMenu(0);
 Sys_SetMouseLook(1);
 if (!netmode) timecount=player.timecount;
 activatemenu=false;
 INT_TimerHook(PlayerCommand);
 keyboardDelay=timecount+KBDELAY;
 }


void RunHelp(void)
{
 player.timecount=timecount;
 INT_TimerHook(NULL);
 ShowHelp();
 INT_TimerHook(PlayerCommand);
 activatehelp=false;
 timecount=player.timecount;
 keyboardDelay=timecount+KBDELAY;
 }


void RunQuickExit(void)
{
 QuickExit=false;
 MouseShow();
 if (ShowQuit(PlayerCommand)) quitgame=true;
 MouseHide();
 keyboardDelay=timecount+KBDELAY;
 }


void RunPause(void)
{
 if (paused)
  {
   gamepause=true;
   if (netmode) NetPause();
   }
 player.timecount=timecount;
 ShowPause();
 timecount=player.timecount;
 if (paused && netmode) NetUnPause();
 paused=false;
 gamepause=false;
 INT_TimerHook(PlayerCommand);
 keyboardDelay=timecount+KBDELAY;
 }


void PrepareNexus(void)
{
 int i, j, mapspot, x, y;

 for(i=-MAPCOLS;i<=MAPCOLS;i+=MAPCOLS)
  for(j=-1;j<=1;j++)
   {
    mapspot=player.mapspot+i+j;
    if (mapspot!=player.mapspot && floorpic[mapspot] && mapsprites[mapspot]==0)
     {
      x=((player.mapspot+i+j)&63)*MAPSIZE+32;
      y=((player.mapspot+i+j)/64)*MAPSIZE+32;
      SpawnSprite(S_EXIT,x<<FRACBITS,y<<FRACBITS,0,0,0,0,0,0);
      SoundEffect(SN_NEXUS,0,x<<FRACBITS,y<<FRACBITS);
      SoundEffect(SN_NEXUS,0,x<<FRACBITS,y<<FRACBITS);
      exitexists=true;
      writemsg("Translation Nexus Created!");
      return;
      }
    }
 }


void RunBrief(void)
{
 /* SCREENBYTES, not 64000: the chrome is hudscale*320 x hudscale*200, so a
    fixed 64000 saved and restored only the top 50 rows of an HD screen and
    left the briefing's art everywhere below them. */
 memcpy(viewbuffer,screen,SCREENBYTES);
 MissionBriefing(player.map);
 INT_TimerHook(PlayerCommand);
 memcpy(screen,viewbuffer,SCREENBYTES);
 activatebrief=false;
 }


void JamWarps(void)
{
 scaleobj_t  *sp, *t;
 int         mapspot, i;
 spawnarea_t *sa;

 if (specialeffect!=SE_WARPJAMMER)
  {
   specialeffect=SE_WARPJAMMER;
   specialeffecttime=timecount+70*60;
   totaleffecttime=70*60;
   --player.inventory[11];
   for (sp=firstscaleobj.next; sp!=&lastscaleobj;)
    if (sp->type==S_GENERATOR || (sp->type>=S_GENSTART && sp->type<=S_GENEND))
     {
      mapspot=(sp->y>>FRACTILESHIFT)*MAPCOLS + (sp->x>>FRACTILESHIFT);
      mapsprites[mapspot]=0;
      t=sp;
      sp=sp->next;
      RF_RemoveSprite(t);
      }
    else
     sp=sp->next;
   sa=spawnareas;
   for(i=0;i<numspawnareas;i++,sa++)
    sa->time=timecount;
   writemsg("Used Warp Jammer");
   }
 warpjammer=false;
 }


void SelectNewSong(void)
{
 songnum++;
 songnum%=32;
 selectsong(songnum);
 newsong=false;
 }


void UpdateView(fixed_t px,fixed_t py,fixed_t pz,int angle,int update)
{
 int          weaponx, weapony, i, x;
 pic_t        *pic;
 static pic_t *wpic;
 char         dbg[80];
 static int   weapbob1, wx, wy;

 angle&=ANGLES;

 if (update)
  weapbob1=weapbob;

 if (update)
  rtimecount=timecount;

 /* The original never cleared the view buffer: floors, ceilings and walls
    always covered every pixel, so there was nothing to clear.  That stopped
    being true in HD, where a polygon can legitimately be skipped -- a ceiling
    whose projection is out of range, a tile carrying no flat, a malformed
    polygon that trips RenderPolygon's guard.  Uncovered pixels then show
    whatever the previous frame left there, which reads as torn rectangles and
    diagonal seams of an older room rather than as a hole.

    Only in HD, and only the extent actually in use; at 1.3 Mpixel this is
    tens of microseconds against a frame that costs milliseconds. */
 /* Safe point for a deferred resize: nothing of this frame is drawn yet. */
 if (hdresizepending)
  {
   hdresizepending=0;
   RF_SetArtMode(SC.hdmode);
   VI_ApplyRenderMode();
   }

 /* TEMPORARY -- test hook, like GREED_SHOT and GREED_MODE.  GREED_FLIP=<ticks>
    flips the renderer mode at each listed tick, which is the only way to
    exercise the live art swap without driving the options menu by hand.  That
    swap is the risky path: it frees every overridable lump while weaponpic[],
    statusbar[], heart[] and the fonts still point into them. */
 {
  const char *flip=getenv("GREED_FLIP");
  if (flip)
   {
    static longint lastflip;
    longint now=timecount/2;
    char list[128], *tok;
    if (now!=lastflip)
     {
      strncpy(list,flip,sizeof(list)-1);
      list[sizeof(list)-1]=0;
      for (tok=strtok(list,","); tok; tok=strtok(NULL,","))
       {
	longint want=(longint)atoi(tok);
	if (lastflip<want && now>=want)
	 {
	  SC.hdmode=!SC.hdmode;
	  printf("FLIP to %s at tick %ld\n",SC.hdmode?"hd":"original",(long)now);
	  RF_SetArtMode(SC.hdmode);
	  VI_ApplyRenderMode();
	  }
       }
      lastflip=now;
      }
    }
  }

 if (hdmode)
  memset(viewbuffer,0,(size_t)windowWidth*windowHeight);

 RF_RenderView(px,py,pz,angle);

 /* In HD the 2D chrome is a separate 320x200 layer composited over the view,
    so it starts each frame empty.  Index 0 is the engine's existing "nothing
    drawn" value -- every VI_DrawMaskedPic* already skips it. */
 if (hdmode) memset(screen,0,SCREENBYTES);

 if (update==1) TimeUpdate();

 if (player.holopic) DrawHolo();

 if (netmode) NetGetData();

 if (timecount<RearViewTime)
  {
   /* rearbuf is a raw 64x64 camera render, so this whole inset is in CHROME
      PIXELS -- x comes straight off hudWidth -- and must not go through the
      logical helpers, which would scale it by hudscale a second time and write
      thousands of pixels past the end of the row. */
   x=hudWidth-66;
   for(i=1;i<64;i++)
    {
     memcpy(hudylookup[i+1]+x,rearbuf+(i<<6),64);
     *(hudylookup[i+1]+x-1)=30;
     *(hudylookup[i+1]+x+64)=30;
     }
   memset(hudylookup[65]+x-1,30,66);
   memset(hudylookup[1]+x-1,30,66);
   }

/* update sprite movement */
 if (update==1) TimeUpdate();

/* draw the weapon pic */

 if (player.angst)  // only if alive
  {
   if (update)
    wpic=weaponpic[weapmode];

   /* All of this is in 320x200 logical units, because that is what
      VI_DrawMaskedPicToBuffer* expect -- they scale by hudscale on the way in.
      hudWidth/hudHeight and the pic's own dimensions are in chrome pixels, so
      both have to come back down first; without that the weapon is positioned
      hudscale times too far right and low, i.e. off the screen entirely. */
   weaponx=(((hudWidth-wpic->width)/hudscale)>>1) + (weapmove[weapbob1]>>1);
   weapony=(hudHeight-wpic->height)/hudscale + (weapmove[weapbob1/2]>>3);

     if (currentViewSize>=6) weapony+=25;
      else if (currentViewSize==5) weapony+=15;
     if (changingweapons && weaponlowering)
      {
       weaponychange+=15;
       weapony+=weaponychange;
       if (weapony>=hudHeight/hudscale-20)
	{
	 weaponlowering=false;
	 player.currentweapon=newweapon;
	 loadweapon(player.weapons[newweapon]);
	 weapmode=0;
	 wpic=weaponpic[weapmode];
	 weaponychange=weaponpic[weapmode]->height/hudscale-20;
	 weapony=hudHeight/hudscale-21;
	 weaponx=(((hudWidth-wpic->width)/hudscale)>>1) + (weapmove[weapbob1]>>1);
	 }
       }
     else if (changingweapons)
      {
       weaponychange-=10;
       if (weaponychange<=0) changingweapons=false;
	else weapony+=weaponychange;
       }

   if (update)
    {
     wx=weaponx;
     wy=weapony;
     }
   else
    {
     weaponx=wx;
     weapony=wy;
     }

   if (netmode) NetGetData();
   if (weapmode==0) VI_DrawMaskedPicToBuffer2(weaponx,weapony,wpic);
    else VI_DrawMaskedPicToBuffer(weaponx,weapony,wpic);
   }

/* update sprite movement */
 if (update==1) TimeUpdate();

/* update displays */

 if (mapmode==1) displaymapmode();
  else if (mapmode==2) displayswingmapmode();
  else if (mapmode) MS_Error("PlayLoop: mapmode %i",mapmode);
 if (heatmode)
  {
   if (mapmode) displayheatmapmode();
    else displayheatmode();
   }
 if (motionmode)
  {
   if (mapmode) displaymotionmapmode();
    else displaymotionmode();
   }

 if (netmode) NetGetData();

 if (currentViewSize>0 && currentViewSize<=4)
  {
   if (currentViewSize==4) pic=statusbar[2];
    else pic=statusbar[currentViewSize-1];
   VI_DrawMaskedPicToBuffer(statusbarloc[currentViewSize*2],statusbarloc[currentViewSize*2+1],pic);
   }

 if (netmode) NetGetData();

 updatedisplay();

 if (netmode) NetGetData();

/* display the message string */
 rewritemsg();

/* finally draw it */

 if (netmode) NetGetData();

 if (ticker)
  {
   sprintf(dbg,"sp:%-4i tp:%-4i ver:%-4i e:%-4i mu:%-2i t:%3i:%2i",
    /* the two pointer differences are ptrdiff_t, which is 64-bit on LP64;
       without these casts %i misreads the varargs and shifts everything
       after it. */
    numspans,transparentposts,(int)(vertexlist_p-vertexlist),
    (int)(entry_p-entries),greedcom->maxusage,timecount/4200,(timecount/70)%60);
//     sprintf(dbg,"x: %i  y: %i",(player.x>>FRACBITS)&63,(player.y>>FRACBITS)&63);
   fontbasecolor=73;
   font=font1;
   printx=2;
   printy=19;
   FN_RawPrint2(dbg);
   }
 if (netmsgstatus==1)
  {
   fontbasecolor=73;
   font=font1;
   printx=2;
   printy=19;
   sprintf(dbg,"Message: %s",netmsg);
   FN_RawPrint2(dbg);
   }
 else if (netmsgstatus==2)
  {
   NetSendMessage(netmsg);
   netmsgstatus=0;
   }

/* update sprite movement */
 if (update==1)
  TimeUpdate();
 }

void PlayLoop (void)
{
	int i;

 if (netmode) NetWaitStart();
  else timecount=0;

 while (!quitgame)
  {
   if (fliplayed)
    {
     if (deadrestart)
      {
		memset(screen,0,SCREENBYTES);

		VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));

       startover(2);
       }
     continue;
     }
   if (netmode) NetGetData();
   if (toggleautorun)
    {
     autorun^=1;
     if (autorun) writemsg("Auto-Run On");
      else writemsg("Auto-Run Off");
     toggleautorun=false;
     }
   if (toggleautotarget)
    {
     autotarget^=1;
     if (autotarget)
      writemsg("Auto-Target On");
     else
      writemsg("Auto-Target Off");
     toggleautotarget=false;
     }
   if (goiright)
    {
     inventoryright();
     goiright=false;
     }
   if (goileft)
    {
     inventoryleft();
     goileft=false;
     }
   if (useitem)
    {
     useinventory();
     useitem=false;
     }
   if (checktrigger)
    {
     checktrigger=false;
     CheckHere(true,player.x,player.y,player.angle);
     if (fliplayed)
      continue;
     }
   if (warpjammer) JamWarps();
   if (netmode) NetGetData();
   if (falldamage)
    { // just makes a grunt sound
     SoundEffect(SN_HIT0+player.chartype,15,player.x,player.y);
     if (netmode) NetSoundEffect(SN_HIT0+player.chartype,15,player.x,player.y);
     falldamage=0;
     }

   if (player.levelscore==0 && !exitexists && !netmode &&
    ((primaries[0]!=-1 && player.primaries[0]==pcount[0]) || primaries[0]==-1) &&
    ((primaries[2]!=-1 && player.primaries[1]==pcount[1]) || primaries[2]==-1))
    PrepareNexus();

   if (ExitLevel)
    {
     EndLevel();
     if (player.map>=22)
      {
       quitgame=true;
       return;
       }
     }

   if (netmode) NetGetData();

   if (timecount>specialeffecttime)
    {
     specialeffect=0;
     specialeffecttime=0x7FFFFFFF;
     }
   if (firegrenade)
    {
     SpawnSprite(S_GRENADE,player.x,player.y,player.z,player.height-(50<<FRACBITS),player.angle,ScrollAngle(),true,playernum);
     SoundEffect(SN_GRENADE,0,player.x,player.y);
     if (netmode) NetSoundEffect(SN_GRENADE,0,player.x,player.y);
     --player.inventory[2];
     firegrenade=false;
     }

   if (netmode) NetGetData();

   if (Warping) WarpAnim();

   if (netmode) NetGetData();

/* check special code flag */
   if (specialcode) Special_Code(secretbuf);

/* update sprite movement */
   TimeUpdate();

/* update wallanimation */
   if (timecount>=wallanimationtime)
    {
     wallanimcount++;
     switch (wallanimcount%3)
      {
       case 0:
	flattranslation[57]=58;
	flattranslation[58]=59;
	flattranslation[59]=57;
	flattranslation[217]=218;
	flattranslation[218]=219;
	flattranslation[219]=217;
	walltranslation[228]=229;
	walltranslation[229]=230;
	walltranslation[230]=228;
	break;
       case 1:
	flattranslation[57]=59;
	flattranslation[58]=57;
	flattranslation[59]=58;
	flattranslation[217]=219;
	flattranslation[218]=217;
	flattranslation[219]=218;
	walltranslation[228]=230;
	walltranslation[229]=228;
	walltranslation[230]=229;
	break;
       case 2:
	flattranslation[57]=57;
	flattranslation[58]=58;
	flattranslation[59]=59;
	flattranslation[217]=217;
	flattranslation[218]=218;
	flattranslation[219]=219;
	walltranslation[228]=228;
	walltranslation[229]=229;
	walltranslation[230]=230;
	break;
       }
     wallanimationtime=timecount+12;
     if (netmode)
      NetGetData();
     if (floorflags[player.mapspot]&F_DAMAGE && player.z==RF_GetFloorZ(player.x,player.y)+player.height)
      hurt(30);
     }

   CheckWarps(player.x,player.y);
   if (fliplayed)
    continue;

   CheckDoors(player.x,player.y);
   if (netmode) NetGetData();
   if (deadrestart) startover(2);
   if (resizeScreen) ChangeViewSize(biggerScreen);
   if (netmode) NetGetData();
   if (scrollview) ChangeScroll();

/* update sprite movement */
   TimeUpdate();

/* check display toggle flags */
   if (toggleheatmode)
    {
     if (heatmode) heatmode=false;
     else
      {
       heatmode=true;
       if (mapmode==2) mapmode=1;
       }
     toggleheatmode=false;
     }
   if (togglemotionmode)
    {
     if (motionmode) motionmode=false;
     else
      {
       motionmode=true;
       if (mapmode==2) mapmode=1;
       }
     togglemotionmode=false;
     }
   if (togglemapmode)
    {
     switch (mapmode)
      {
       case 0:
	mapmode=1;
	break;
       case 1:
	if (heatmode || motionmode) mapmode=0;
	 else mapmode=2;
	break;
       case 2:
	mapmode=0;
       }
     togglemapmode=false;
     }
   if (togglegoalitem) NewGoalItem();
   if (ToggleRearView)
    {
     RearViewOn^=1;
     ToggleRearView=false;
     RearViewDelay=timecount;
     }

   if (netmode) NetGetData();

/* render the view */
   if (RearViewOn && timecount>=RearViewDelay)
    {
     RearViewTime=timecount+140;
     RearView();
     RearViewDelay=timecount+SC.camdelay;
     if (SC.camdelay==70)
      RearViewOn=false;
     }

   if (netmode) NetGetData();

   scrollmin=player.scrollmin;
   scrollmax=player.scrollmax;
   
   /*for ( i = 0 ; i < 200 ; i++ )
	   memset(ylookup[i],i,320);
   VI_BlitView();*/

   UpdateView(player.x,player.y,player.z,player.angle,1);

   RF_BlitView();
   VI_BlitView();
   if (getenv("GREED_REPRO"))
    {
     extern void VI_DumpPresented(char*);
     static int a0, done; static longint lastt; char path[300];
     if (!a0) a0=player.angle|0x10000;
     if (timecount!=lastt)
      {
       lastt=timecount;
       if (timecount<140) {}
       else if (timecount<170) player.angle=((a0&0xffff)+512)&ANGLES;
       else if (timecount<535) Thrust(player.angle,40<<10);
       else if (timecount<565) player.angle=(a0&0xffff)&ANGLES;
       else if (!done)
        { sprintf(path,"%s",getenv("GREED_REPRO"));
          printf("REPROSHOT view=%dx%d proj=%d\n",windowWidth,windowHeight,viewproj);
          VI_DumpPresented(path); done=1; exit(0); }
       }
     }
   if (newsong) SelectNewSong();

   if (activatemenu) RunMenu();

   if (activatehelp) RunHelp();

   if (activatebrief) RunBrief();

   TimeUpdate();

   if (QuickExit) RunQuickExit();

   if (netmode) NetGetData();

   if (paused || netpaused) RunPause();

   ++frames;

/* update sprite movement */
   TimeUpdate();

/* update lights if necessary */
   if (changelight!=0) newlights();

/* update weapon to be displayed */
   while (weapmode && timecount>=weapdelay)
    {
     if (player.weapons[player.currentweapon]==4 || player.weapons[player.currentweapon]==18)
      {
       if (weapmode==1)
	{
	 if (player.weapons[player.currentweapon]==4)
	  {
	   SoundEffect(SN_BULLET4,0,player.x,player.y);
	   if (netmode) NetSoundEffect(SN_BULLET4,0,player.x,player.y);
	   }
	 if (player.weapons[player.currentweapon]==18)
	  {
	   SoundEffect(SN_BULLET18,0,player.x,player.y);
	   if (netmode)
	    NetSoundEffect(SN_BULLET18,0,player.x,player.y);
	   }
	 }
       weapmode=weaponstate[player.weapons[player.currentweapon]][weapmode];
       if (weapmode==0)
	fireweapon();
       }
     else
      weapmode=weaponstate[player.weapons[player.currentweapon]][weapmode];
     weapdelay=timecount+8;
     }

   if (netmode) NetGetData();
   }
 }


void ActionHook (void)
{
 actionflag = 0;
 }


void InitData(void)
{
 int i;

 quitgame=false;
 mapmode=0;
 heatmode=false;
 motionmode=false;
 turnrate=0;
 moverate=0;
 fallrate=0;
 strafrate=0;
 MapZoom=8;
 memset(secretbuf,0,20);
 secretindex=0;
//   demobuffer=CA_CacheLump(CA_GetNamedNum("demo"));
 if (playback)
  {
   demobuffer=CA_LoadFile("demo1");
   recordindex=0;
   }
 if (recording)
  {
   demobuffer=CA_CacheLump(CA_GetNamedNum("demo"));
   memset(demobuffer,0,RECBUFSIZE);
   recordindex=0;
   }
 probe.moveSpeed=MAXPROBE;
 probe.movesize=16<<FRACBITS; // half a tile
 probe.spawnid=playernum;
 /* Walk the view size to both ends to force the projection tables to rebuild,
    then put it back where the player had it.

    The DOS original ends this with

      for (i=0;i<currentViewSize;i++) ChangeViewSize(true);

    which cannot restore anything: the bound is the counter's own target, so
    every step raises it too and the loop climbs to the smallest view the engine
    allows.  It was harmless only because SC.screensize was pinned to 0 and
    currentViewSize was therefore always 0 here -- with the slider gone from
    this port's options menu nothing applied SC.screensize at all, so the game
    was stuck at size 0 and drew no HUD but the inventory popup.  Now that the
    status bar is reachable again this runs with a real value, so walk to a
    fixed target -- SC.screensize, which is also what makes a saved size take
    effect at level start rather than only after a visit to the menu.  The
    step-check ends the loop if ChangeViewSize refuses, which it does past
    MAXHDVIEWSIZE in HD. */
 for (i=0;i<4;i++) ChangeViewSize(true);
 for (i=0;i<4;i++) ChangeViewSize(false);
 if (SC.screensize>MaxViewSize()) SC.screensize=MaxViewSize();
 while (currentViewSize<SC.screensize)
  {
   int prev=currentViewSize;
   ChangeViewSize(true);
   if (currentViewSize==prev) break;
   }
 resetdisplay();
 }


void SaveDemo(void)
{
 FILE *f;

 f=fopen("demo1","w");
 fwrite(demobuffer,RECBUFSIZE,1,f);
 fclose(f);
 }


void maingame(void)
{
 VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));
 InitData();
 INT_TimerHook(PlayerCommand);   // the players actions are sampled by an interrupt
 newlights();
 Sys_SetMouseLook(1);            // capture the pointer for mouse aiming
 PlayLoop();
 Sys_SetMouseLook(0);
 player.timecount=timecount;
 if (netmode) 
	 NetQuitGame();
 if (recording) 
	 SaveDemo();
 SaveSetup(&SC,"SETUP.CFG");
 playback=false;
 }

