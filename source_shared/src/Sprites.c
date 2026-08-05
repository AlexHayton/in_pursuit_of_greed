/**************************************************************************/
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
#include <MATH.H>
#include <TIME.H>
#include "d_disk.h"
#include "d_global.h"
#include "r_refdef.h"
#include "d_ints.h"
#include "protos.h"
#include "d_misc.h"


/**** VARIABLES ****/

scaleobj_t *msprite, probe;
bool    spritehit, playerhit;
fixed_t    hitx, hity, targx, targy, targz;
int        spriteloc; // where did it hit on a sprite


/**** FUNCTIONS ****/

bool SP_TryDoor (fixed_t xcenter, fixed_t ycenter)
{
 int       xl, yl, xh, yh, x, y;
 doorobj_t *door_p, *last_p;

 if (msprite==&probe) return true;
// These values will probably have to be tweaked for doors that are along
// the vertical opposite axis (northwall)
 xl = (int)((xcenter-msprite->movesize) >> FRACTILESHIFT);
 yl = (int)((ycenter-msprite->movesize/* - (TILEUNIT >> 1)*/) >> FRACTILESHIFT);
 xh = (int)((xcenter+msprite->movesize) >> FRACTILESHIFT);
 yh = (int)((ycenter+msprite->movesize/* - (TILEUNIT >> 1)*/) >> FRACTILESHIFT);
// check for doors on the north wall
 for (y=yl+1;y<=yh;y++)
  for (x=xl;x<=xh;x++)
   {
    if (mapflags[y*MAPSIZE+x] & FL_DOOR) // if tile has a door
     {
      last_p=&doorlist[numdoors];
      for (door_p=doorlist;door_p!=last_p;door_p++)
       if (door_p->tilex==x && door_p->tiley==y && (door_p->orientation==dr_horizontal || door_p->orientation==dr_horizontal2))
	{
	 if (door_p->doorOpen && !door_p->doorClosing) return true; // can move, door is open
	 else if (!door_p->doorOpen && door_p->doorBumpable && !door_p->doorOpening)
	  {
	   door_p->doorOpening=true;
	   door_p->doorClosing=false;
	   SoundEffect(SN_DOOR,15,door_p->tilex<<FRACTILESHIFT,door_p->tiley<<FRACTILESHIFT);
	   door_p->doorTimer+=20;
	   if (door_p->orientation==dr_horizontal) SP_TryDoor(xcenter+64*FRACUNIT,ycenter);
	    else SP_TryDoor(xcenter-64*FRACUNIT,ycenter);
	   return false;
	   }
	 else if (!door_p->doorOpen && door_p->doorBumpable && door_p->doorClosing)
	  {
	   door_p->doorClosing=false;
	   door_p->doorOpening=true;
	   SoundEffect(SN_DOOR,15,door_p->tilex<<FRACTILESHIFT,door_p->tiley<<FRACTILESHIFT);
	   door_p->doorTimer+=20;
	   if (door_p->orientation==dr_horizontal) SP_TryDoor(xcenter+64*FRACUNIT,ycenter);
	    else SP_TryDoor(xcenter-64*FRACUNIT,ycenter);
	   return false;
	   }
	 else return false;
	 }
      }
    }
// check for doors on the west wall
 xl = (int)((xcenter-msprite->movesize/* - (TILEUNIT >> 1)*/) >> FRACTILESHIFT);
 yl = (int)((ycenter-msprite->movesize) >> FRACTILESHIFT);
 xh = (int)((xcenter+msprite->movesize/* - (TILEUNIT >> 1)*/) >> FRACTILESHIFT);
 yh = (int)((ycenter+msprite->movesize) >> FRACTILESHIFT);
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
	   SoundEffect(SN_DOOR,15,door_p->tilex<<FRACTILESHIFT,door_p->tiley<<FRACTILESHIFT);
	   door_p->doorTimer+=20;
	   if (door_p->orientation==dr_vertical) SP_TryDoor(xcenter,ycenter+64*FRACUNIT);
	    else SP_TryDoor(xcenter,ycenter-64*FRACUNIT);
	   return false;
	   }
	 else if (!door_p->doorOpen && door_p->doorBumpable && door_p->doorClosing)
	  {
	   door_p->doorClosing=false;
	   door_p->doorOpening=true;
	   SoundEffect(SN_DOOR,15,door_p->tilex<<FRACTILESHIFT,door_p->tiley<<FRACTILESHIFT);
	   door_p->doorTimer+=20;
	   if (door_p->orientation==dr_vertical) SP_TryDoor(xcenter,ycenter+64*FRACUNIT);
	    else SP_TryDoor(xcenter,ycenter-64*FRACUNIT);
	   return false;
	   }
	 else return false;
	 }
      }
    }
 return true;
 }


int SP_TryMove(fixed_t xcenter, fixed_t ycenter)
{
 int     xl, yl, xh, yh, x, y, mapspot;

 xl = (int)((xcenter-msprite->movesize) >> FRACTILESHIFT);
 yl = (int)((ycenter-msprite->movesize) >> FRACTILESHIFT);
 xh = (int)((xcenter+msprite->movesize) >> FRACTILESHIFT);
 yh = (int)((ycenter+msprite->movesize) >> FRACTILESHIFT);
 for (y=yl;y<=yh;y++)
  for (x=xl;x<=xh;x++)
   {
    mapspot=MAPCOLS*y+x;
    if ((y>yl && northwall[mapspot] && !(northflags[mapspot] & F_NOCLIP) &&
	!(northflags[mapspot] & F_NOBULLETCLIP)) ||
	(x>xl && westwall[mapspot] && !(westflags[mapspot] & F_NOCLIP) &&
	!(westflags[mapspot] & F_NOBULLETCLIP))) return 2; // wall hit
    if (msprite!=&probe)
     {
      if (msprite->z<RF_GetFloorZ((x<<FRACTILESHIFT)+(32<<FRACBITS),(y<<FRACTILESHIFT)+(32<<FRACBITS))) return 2;    // below floor
      if (msprite->z>RF_GetCeilingZ((x<<FRACTILESHIFT)+(32<<FRACBITS),(y<<FRACTILESHIFT)+(32<<FRACBITS))) return 2; // below ceiling
      }

    if (mapsprites[mapspot]==64) return 2; // instawall

    if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128 && mapspot!=msprite->startspot)
     {
      spritehit=true;
      spriteloc=mapspot;
      return 1;
      }
    if (mapspot==player.mapspot && mapspot!=msprite->startspot && msprite->spawnid!=playernum) // can't shot yourself
     {
      playerhit=true;
      return 1;
      }
    }
 return 0;
 }


byte SP_ClipMove(fixed_t xmove,fixed_t ymove,fixed_t zmove)
{
 int     result;
 fixed_t dx, dy;

 dx=msprite->x+xmove;
 dy=msprite->y+ymove;
 spritehit=false;
 result=SP_TryMove(dx,dy);
 if (result)
  {
   hitx=dx;
   hity=dy;
   }
 if (result!=2 && !SP_TryDoor(dx,dy)) result=2;  // door hit or wall hit
 if (result!=2)
  {
   msprite->x+=xmove;
   msprite->y+=ymove;
   msprite->z+=zmove;
   }
 return result;
 }


byte SP_Thrust()
{
 msprite->angle&=ANGLES;
 msprite->angle2&=ANGLES;
 return SP_ClipMove(costable[msprite->angle],-sintable[msprite->angle],sintable[msprite->angle2]);
 }


bool SP_TryMove2(int angle,fixed_t xcenter, fixed_t ycenter,int smapspot)
{
 int     xl, yl, xh, yh, x, y, mapspot;
 fixed_t sz, sz2, floorz, ceilingz;

 if (angle<NORTH || angle>SOUTH)
  {
   xl=xcenter>>FRACTILESHIFT;
   xh=(xcenter+msprite->movesize) >> FRACTILESHIFT;
   }
 else if (angle>NORTH && angle<SOUTH)
  {
   xh=xcenter>>FRACTILESHIFT;
   xl=(xcenter-msprite->movesize) >> FRACTILESHIFT;
   }
 else
  {
   xl=(xcenter-msprite->movesize) >> FRACTILESHIFT;
   xh=(xcenter+msprite->movesize) >> FRACTILESHIFT;
   }

 if (angle>WEST)
  {
   yl=ycenter>>FRACTILESHIFT;
   yh=(ycenter+msprite->movesize) >> FRACTILESHIFT;
   }
 else if (angle<WEST && angle!=EAST)
  {
   yl=(ycenter-msprite->movesize) >> FRACTILESHIFT;
   yh=ycenter>>FRACTILESHIFT;
   }
 else
  {
   yl=(ycenter-msprite->movesize) >> FRACTILESHIFT;
   yh=(ycenter+msprite->movesize) >> FRACTILESHIFT;
   }
 sz = msprite->z - msprite->zadj + (20<<FRACBITS);
 sz2 = msprite->z - msprite->zadj;
 for (y=yl;y<=yh;y++)
  for (x=xl;x<=xh;x++)
   {
    mapspot=MAPCOLS*y+x;
    if (mapspot==player.mapspot ||
	(y>yl && northwall[mapspot] && !(northflags[mapspot] & F_NOCLIP)) ||
	(x>xl && westwall[mapspot] && !(westflags[mapspot] & F_NOCLIP)))
     return false; // wall hit
    floorz=RF_GetFloorZ((x<<FRACTILESHIFT)+(32<<FRACBITS),(y<<FRACTILESHIFT)+(32<<FRACBITS));
    if (floorz>sz)
     return false;
    if (msprite->nofalling && floorz+(5<<FRACBITS)<sz2)
     return false;
    ceilingz=RF_GetCeilingZ((x<<FRACTILESHIFT)+(32<<FRACBITS),(y<<FRACTILESHIFT)+(32<<FRACBITS));
    if (ceilingz<msprite->z+msprite->height)
     return false;
    if (ceilingz-floorz<msprite->height)
     return false;
    if (mapspot!=smapspot && mapsprites[mapspot])
     return false;
    }
 return true;
 }


byte SP_ClipMove2(fixed_t xmove, fixed_t ymove)
{
 fixed_t dx, dy;
 int     smapspot, angle2, ms;

 if (msprite->type==S_CLONE) ms=SM_CLONE;
  else ms=1;
 dx=msprite->x+xmove;
 dy=msprite->y+ymove;
 smapspot=(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT);
 if (SP_TryMove2(msprite->angle,dx,dy,smapspot) && SP_TryDoor(dx,dy))
  {
   if (floorpic[(dy>>FRACTILESHIFT)*MAPCOLS+(dx>>FRACTILESHIFT)]==0) return 0;
   mapsprites[smapspot]=0;
   msprite->x+=xmove;
   msprite->y+=ymove;
   mapsprites[(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT)]=ms;
   return 1;
   }
// the move goes into a wall, so try and move along one axis
 if (xmove>0)
  {
   angle2=EAST;
   dx=msprite->x+msprite->moveSpeed;
   }
 else
  {
   angle2=WEST;
   dx=msprite->x-msprite->moveSpeed;
   }
 if (SP_TryMove2(angle2,dx,msprite->y,smapspot) && SP_TryDoor(dx,msprite->y))
  {
   if (floorpic[(msprite->y>>FRACTILESHIFT)*MAPCOLS+(dx>>FRACTILESHIFT)]==0) return 0;
   mapsprites[smapspot]=0;
   msprite->x+=xmove;
   mapsprites[(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT)]=ms;
   return 2;
   }
 if (ymove>0)
  {
   angle2=SOUTH;
   dy=msprite->y+msprite->moveSpeed;
   }
 else
  {
   angle2=NORTH;
   dy=msprite->y-msprite->moveSpeed;
   }
 if (SP_TryMove2(angle2,msprite->x,dy,smapspot) && SP_TryDoor(msprite->x,dy))
  {
   if (floorpic[(dy>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT)]==0) return 0;
   mapsprites[smapspot]=0;
   msprite->y+=ymove;
   mapsprites[(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT)]=ms;
   return 3;
   }
 return 0;
 }


byte SP_Thrust2()
{
 fixed_t xmove, ymove;

 msprite->angle&=ANGLES;
 xmove=FIXEDMUL(msprite->moveSpeed,costable[msprite->angle]);
 ymove=-FIXEDMUL(msprite->moveSpeed,sintable[msprite->angle]);
 return SP_ClipMove2(xmove,ymove);
 }


void ActivationSound(scaleobj_t *sp)
{
 switch (sp->type)
  {
   case S_MONSTER1:
    SoundEffect(SN_MON1_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER2:
    SoundEffect(SN_MON2_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER3:
    SoundEffect(SN_MON3_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER4:
    SoundEffect(SN_MON4_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER5:
    SoundEffect(SN_MON5_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER6:
    SoundEffect(SN_MON6_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER7:
    SoundEffect(SN_MON7_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER8:
    SoundEffect(SN_MON8_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER9:
    SoundEffect(SN_MON9_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER10:
    SoundEffect(SN_MON10_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER11:
    SoundEffect(SN_MON11_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER12:
    SoundEffect(SN_MON12_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER13:
    SoundEffect(SN_MON13_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER14:
    SoundEffect(SN_MON14_WAKE,7,sp->x,sp->y);
    break;
   case S_MONSTER15:
    SoundEffect(SN_MON15_WAKE,7,sp->x,sp->y);
    break;
   }
 }


void ActivateSprites(int sx,int sy)
/* proximity activation (recursive chain reaction) */
{
 scaleobj_t *sp;
 int        x, y;

 for(sp=firstscaleobj.next;sp!=&lastscaleobj;sp=sp->next)
  if (sp->active==false && sp->moveSpeed)
   {
    x=sp->x>>FRACTILESHIFT;
    y=sp->y>>FRACTILESHIFT;
    if (abs(x-sx)<5 && abs(y-sy)<5)
     {
      sp->active=true;
      sp->actiontime=timecount+40;
      ActivationSound(sp);
      ActivationSound(sp);
      ActivateSprites(x,y);
      }
    }
 }


void ShowWallPuff()
{
 int i;

 switch (msprite->type)
  {
   case S_BULLET3:
   case S_BULLET12:
   case S_BULLET17:
   case S_MONSTERBULLET2:
   case S_MONSTERBULLET4:
   case S_MONSTERBULLET6:
   case S_MONSTERBULLET8:
   case S_GRENADEBULLET:
    i=S_SMALLEXPLODE;
    break;
   case S_BULLET4:
   case S_MONSTERBULLET5:
   case S_MONSTERBULLET11:
    i=S_PLASMAWALLPUFF;
    break;
   case S_HANDBULLET:
   case S_BLOODSPLAT:
   case S_BULLET7:
   case S_MONSTERBULLET7:
   case S_MONSTERBULLET10:
   case S_MONSTERBULLET12:
   case S_MONSTERBULLET15:
   case S_SOULBULLET:
    return;
   case S_BULLET9:
    i=S_ARROWPUFF;
    break;
   case S_BULLET10:
   case S_BULLET18:
    i=S_GREENPUFF;
    break;
   case S_MINEBULLET:
    i=S_MINEPUFF;
    break;
   default:
    i=S_WALLPUFF;
   }
 SpawnSprite(i,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
 ActivateSprites(msprite->x>>FRACTILESHIFT,msprite->y>>FRACTILESHIFT);
 }


void HitSprite(scaleobj_t *sp)
{
 switch (sp->type)
  {
   case S_CLONE:
    if (!sp->active)
     {
      ActivateSprites((int)(sp->x>>FRACTILESHIFT),(int)(sp->y>>FRACTILESHIFT));
      sp->active=true;
      }
    sp->modetime=timecount+8;
    sp->basepic=sp->startpic+32;
    SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
    if (msprite->type!=S_BULLET17)
     {
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      }
    break;
   case S_MONSTER1:
   case S_MONSTER2:
   case S_MONSTER7:
   case S_MONSTER9:
   case S_MONSTER10:
   case S_MONSTER12:
   case S_MONSTER13:
   case S_MONSTER14:
   case S_MONSTER15:
    if (!sp->active)
     {
      ActivateSprites((int)(sp->x>>FRACTILESHIFT),(int)(sp->y>>FRACTILESHIFT));
      sp->active=true;
      }
    sp->modetime=timecount+8;
    sp->basepic=sp->startpic+40;
    SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
    if (msprite->type!=S_BULLET17)
     {
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
      }
    break;
   case S_MONSTER3:
   case S_MONSTER4:
   case S_MONSTER5:
   case S_MONSTER6:
   case S_MONSTER8:
   case S_MONSTER11:
    ShowWallPuff();
    break;
   }
 }


bool Int0(void)
/* control of bullets, explosions, and other objects (not monsters!!) */
{
 scaleobj_t *hsprite, *sp;
 int        counter, mapspot, result, angle, angleinc, i, oldfall;
 int        oldangle, oldmovespeed;
 bool    killed, blood;

 counter=0;
 killed=false;

 if (msprite->type==S_GRENADE) msprite->angle2-=4;
  else if ((msprite->type==S_BLOODSPLAT || msprite->type==S_METALPARTS) && (msprite->angle2<NORTH || msprite->angle2>SOUTH))
   msprite->angle2-=32;

 if (msprite->maxmove)
  {
   --msprite->maxmove;
   if (msprite->maxmove<=0)
    {
     ShowWallPuff();
     return true;
     }
   }

 if (msprite->type==S_MONSTERBULLET4 || msprite->type==S_MONSTERBULLET6 || msprite->type==S_BULLET17)
  SpawnSprite(S_WALLPUFF,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
 blood=false;
 while (counter++<msprite->moveSpeed)
  {
   result=SP_Thrust();
   if (msprite->type==S_BULLET3)
    msprite->z=RF_GetFloorZ(msprite->x,msprite->y)+(20<<FRACBITS);

   if (result!=0)
    {
     if (msprite->type==S_BLOODSPLAT)
      {
       if (result==2) return true;
       spritehit=false;
       playerhit=false;
       }
     else if (msprite->type==S_METALPARTS && result==2)
      {
       playerhit=false;
       return true;
       }
     if (spritehit)
      {
       if (mapsprites[spriteloc]==SM_NETPLAYER)
	{
	 for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
	  if (hsprite!=msprite)
	   {
	    mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	    if (mapspot==spriteloc)
	     {
	      if (msprite->z<hsprite->z || msprite->z>hsprite->z+hsprite->height || hsprite->type!=S_NETPLAYER) break;
	       {
		SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
	     //	if (msprite->type!=S_BULLET17)
	     //	 {
		  SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
		  SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
		  SpawnSprite(S_BLOODSPLAT,msprite->x,msprite->y,msprite->z,msprite->zadj,0,0,false,0);
		  blood=true;
		  killed=true;
	     //	  }
		}
	      }
	    }
	  }
       else
	for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
	 if (hsprite!=msprite)
	  {
	   mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
	   if (mapspot==spriteloc)
	    {
	     if (msprite->z<hsprite->z || msprite->z>hsprite->z+hsprite->height) continue;
	     if (hsprite->hitpoints)
	      {
	       if (hsprite->type!=S_MONSTER5)
		hsprite->actiontime+=15;
	       else
		hsprite->actiontime+=5;
	       hsprite->hitpoints-=msprite->damage;
	       if (msprite->spawnid==255) hsprite->enraged++;

	       if (msprite->type==S_SOULBULLET && msprite->spawnid==playernum)
		{
		 heal(msprite->damage/2);
		 medpaks(msprite->damage/2);
		 }

	       killed=true;
	       if (hsprite->hitpoints<=0)
		{
		 if (msprite->spawnid==playernum || msprite->spawnid-200==playernum)
		  {
		   ++player.bodycount;
		   addscore(hsprite->score);
		   }
		 mapsprites[spriteloc]=0;
		 blood=true;
		 HitSprite(hsprite);
		 KillSprite(hsprite,msprite->type);
		 }
	       else if (msprite->damage)
		{
		 oldangle=hsprite->angle;
		 oldmovespeed=hsprite->moveSpeed;
		 hsprite->angle=msprite->angle;
		 hsprite->moveSpeed=(msprite->damage>>2)<<FRACBITS;
		 sp=msprite;
		 msprite=hsprite;
		 oldfall=msprite->nofalling;
		 msprite->nofalling=0;
		 SP_Thrust2();
		 msprite->nofalling=oldfall;
		 msprite=sp;
		 hsprite->angle=oldangle;
		 hsprite->moveSpeed=oldmovespeed;
		 blood=true;
		 HitSprite(hsprite);
		 }
	       break;
	       }
	     }
	   }
       }
     else if (playerhit && msprite->z>player.z-player.height && msprite->z<player.z)
      {
       if (player.angst!=0) // don't keep hitting
	{
	 hurt(msprite->damage);
	 if (player.angst==0 && netmode) NetDeath(msprite->spawnid);
	 }
       Thrust(msprite->angle,msprite->damage<<(FRACBITS-3));
       playerhit=false;
       killed=true;
       if (msprite->damage>50)
	{
	 player.angle+=-15+(MS_RndT()&31);
	 player.angle&=ANGLES;
	 }
       }
     if (result==2)
      killed=true;
     if (killed)
      {
       if (!blood)
	ShowWallPuff();
       break;
       }
     }
   }
 if (killed && msprite->type==S_GRENADE)
  {
   angleinc=ANGLES/12;
   angle=0;
   for(i=0,angle=0;i<12;i++,angle+=angleinc)
    {
     sp=SpawnSprite(S_GRENADEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,0,true,msprite->spawnid);
     sp->maxmove=3;
     sp->startspot=-1;
     }
   angleinc=ANGLES/8;
   angle=0;
   for(i=0,angle=0;i<8;i++,angle+=angleinc)
    {
     sp=SpawnSprite(S_GRENADEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,64,true,msprite->spawnid);
     sp->maxmove=2;
     sp->startspot=-1;
     }
   angleinc=ANGLES/8;
   angle=0;
   for(i=0,angle=0;i<8;i++,angle+=angleinc)
    {
     sp=SpawnSprite(S_GRENADEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,-64,true,msprite->spawnid);
     sp->maxmove=2;
     sp->startspot=-1;
     }
   sp=SpawnSprite(S_EXPLODE,msprite->x,msprite->y,msprite->z,0,0,0,true,255);
   SoundEffect(SN_EXPLODE1+(MS_RndT()&1),15,msprite->x,msprite->y);
   }
 else if (killed && msprite->type==S_BULLET17)
  {
   angleinc=ANGLES/8;
   angle=0;
   for(i=0,angle=0;i<8;i++,angle+=angleinc)
    {
     sp=SpawnSprite(S_GRENADEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,0,true,msprite->spawnid);
     sp->maxmove=3;
     sp->startspot=-1;
     }
   angleinc=ANGLES/6;
   angle=0;
   for(i=0,angle=0;i<6;i++,angle+=angleinc)
    {
     sp=SpawnSprite(S_GRENADEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,64,true,msprite->spawnid);
     sp->maxmove=2;
     sp->startspot=-1;
     }
   angleinc=ANGLES/6;
   angle=0;
   for(i=0,angle=0;i<6;i++,angle+=angleinc)
    {
     sp=SpawnSprite(S_GRENADEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,-64,true,msprite->spawnid);
     sp->maxmove=2;
     sp->startspot=-1;
     }
   sp=SpawnSprite(S_EXPLODE,msprite->x,msprite->y,msprite->z,0,0,0,true,255);
   SoundEffect(SN_EXPLODE1+(MS_RndT()&1),15,msprite->x,msprite->y);
   }
 return killed;
 }

/***************************************************************************/

int ScanX(int limit1, int x1, int y1, int x2, int y2,int *tx,int *ty)
/* check for the player along the x axis */
{
 int mapspot, wall, x, limit, flags;

 mapspot=y1*MAPCOLS+x1+1;
 x=x1;
 limit=limit1;
 while (1)
  {
   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (x==x2 && y1==y2))
    {
     *tx=x+1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x+1;
     *ty=y1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;
   wall=westwall[mapspot];
   flags=westflags[mapspot];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;
   ++mapspot;
   ++x;
   --limit;
   if (!limit) break;
   }
 limit=limit1;
 mapspot=y1*MAPCOLS+x1-1;
 x=x1;
 while (1)
  {
   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (x==x2 && y1==y2))
    {
     *tx=x-1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x-1;
     *ty=y1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) return 1;
   wall=westwall[mapspot+1];
   flags=westflags[mapspot+1];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) return 0;
   --mapspot;
   --x;
   --limit;
   if (!limit) break;
   }
 return 0;
 }


int ScanY(int limit1, int x1, int y1, int x2, int y2,int *tx,int *ty)
/* check for the player along the y axis */
{
 int mapspot, wall, y, limit, flags;

 limit=limit1;
 mapspot=y1*MAPCOLS+x1+MAPCOLS;
 y=y1;
 while (1)
  {
   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (y==y2 && x1==x2))
    {
     *ty=y+1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x1;
     *ty=y+1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;
   wall=northwall[mapspot];
   flags=northflags[mapspot];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;
   mapspot+=MAPCOLS;
   ++y;
   --limit;
   if (!limit) break;
   }
 limit=limit1;
 mapspot=y1*MAPCOLS+x1-MAPCOLS;
 y=y1;
 while (1)
  {
   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (y==y2 && x1==x2))
    {
     *ty=y-1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x1;
     *ty=y-1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;
   wall=northwall[mapspot+MAPCOLS];
   flags=northflags[mapspot+MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;
   mapspot-=MAPCOLS;
   --y;
   --limit;
   if (!limit) break;
   }
 return 0;
 }


int ScanAngle(int limit1, int x1, int y1, int x2, int y2,int *tx,int *ty)
/* scan for the player along a 45 degree angle
   this is not very accurate!! approximate only */
{
 int mapspot, wall, x, y, limit, flags;

 limit=limit1;
 mapspot=y1*MAPCOLS+x1+MAPCOLS+1;
 y=y1;
 x=x1;
 while (1)
  {
   wall=northwall[mapspot-1];
   flags=northflags[mapspot-1];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;

   wall=westwall[mapspot-MAPCOLS];
   flags=westflags[mapspot-MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) return 0;

   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (y==y2 && x==x2))
    {
     *tx=x+1;
     *ty=y+1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x+1;
     *ty=y+1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;

   mapspot+=MAPCOLS+1;
   ++y;
   ++x;
   --limit;
   if (!limit) break;
   }

 limit=limit1;
 mapspot=y1*MAPCOLS+x1+MAPCOLS-1;
 y=y1;
 x=x1;
 while (1)
  {
   wall=northwall[mapspot+1];
   flags=northflags[mapspot+1];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;

   wall=westwall[mapspot-MAPCOLS];
   flags=westflags[mapspot-MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) return 0;

   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (y==y2 && x==x2))
    {
     *tx=x-1;
     *ty=y+1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x-1;
     *ty=y+1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;

   mapspot+=MAPCOLS-1;
   ++y;
   --x;
   --limit;
   if (!limit) break;
   }

 limit=limit1;
 mapspot=y1*MAPCOLS+x1-MAPCOLS+1;
 y=y1;
 x=x1;
 while (1)
  {
   wall=northwall[mapspot-1+MAPCOLS];
   flags=northflags[mapspot-1+MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;

   wall=westwall[mapspot-MAPCOLS];
   flags=westflags[mapspot-MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) return 0;

   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (y==y2 && x==x2))
    {
     *tx=x+1;
     *ty=y-1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x+1;
     *ty=y-1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;

   mapspot-=MAPCOLS+1;
   --y;
   ++x;
   --limit;
   if (!limit) break;
   }

 limit=limit1;
 mapspot=y1*MAPCOLS+x1-MAPCOLS-1;
 y=y1;
 x=x1;
 while (1)
  {
   wall=northwall[mapspot+1+MAPCOLS];
   flags=northflags[mapspot+1+MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;

   wall=westwall[mapspot-MAPCOLS];
   flags=westflags[mapspot-MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) return 0;

   if (mapsprites[mapspot]==SM_NETPLAYER || mapsprites[mapspot]==SM_CLONE || (y==y2 && x==x2))
    {
     *tx=x-1;
     *ty=y-1;
     return 2;
     }
   if (msprite->enraged>=6-player.difficulty && mapsprites[mapspot]==1)
    {
     *tx=x-1;
     *ty=y-1;
     return 2;
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;

   mapspot-=MAPCOLS-1;
   --y;
   --x;
   --limit;
   if (!limit) break;
   }

 return 0;
 }

/***************************************************************************/

int GetFireAngle(fixed_t sz,int x1,int y1,fixed_t px,fixed_t py,fixed_t pz)
{
 scaleobj_t *hsprite;
 int        x, y, z, d, spriteloc, mapspot;
 bool    found;

 sz+=msprite->z;
 if (x1!=px>>FRACTILESHIFT || y1!=py>>FRACTILESHIFT)
  {
   spriteloc=y1*MAPCOLS+x1;
   found=false;
   for(hsprite=firstscaleobj.next;hsprite!=&lastscaleobj;hsprite=hsprite->next)
    if (hsprite->hitpoints)
     {
      mapspot=(hsprite->y>>FRACTILESHIFT)*MAPCOLS+(hsprite->x>>FRACTILESHIFT);
      if (mapspot==spriteloc)
       {
	found=true;
	break;
	}
      }
   if (found)
    {
     px=hsprite->x;
     py=hsprite->y;
     pz=hsprite->z+(32<<FRACBITS);
     }
   }
 else pz+=20<<FRACBITS;
 if (sz>pz)
  {
   z=(sz-pz)>>(FRACBITS+2);
   if (z>=MAXAUTO) return 0;
   x=(msprite->x-px)>>(FRACBITS+2);
   y=(msprite->y-py)>>(FRACBITS+2);
   d=(int)sqrt(x*x + y*y);
   if (d>=MAXAUTO || autoangle2[d][z]==-1) return 0;
   return -autoangle2[d][z];
   }
 else if (sz<pz)
  {
   z=(pz-sz)>>(FRACBITS+2);
   if (z>=MAXAUTO) return 0;
   x=(msprite->x-px)>>(FRACBITS+2);
   y=(msprite->y-py)>>(FRACBITS+2);
   d=(int)sqrt(x*x + y*y);
   if (d>=MAXAUTO || autoangle2[d][z]==-1) return 0;
   return autoangle2[d][z];
   }
 else return 0;
 }

/***************************************************************************/

void Int5(void)  // priests / viscount lords
{
 int     angle, sx, sy, px, py, tx, ty, pangle;
 fixed_t floorz, oldspeed, fheight;

 sx=msprite->x>>FRACTILESHIFT;
 sy=msprite->y>>FRACTILESHIFT;
 if (netmode) NetGetClosestPlayer(sx,sy);
 else
  {
   if (specialeffect==SE_INVISIBILITY)
    {
     targx=0;
     targy=0;
     targz=0;
     }
   else
    {
     targx=player.x;
     targy=player.y;
     targz=player.z;
     }
   }
 px=targx>>FRACTILESHIFT;
 py=targy>>FRACTILESHIFT;

 oldspeed=msprite->moveSpeed;
 if (abs(px-sx)<6 && abs(py-sy)<6) msprite->moveSpeed=msprite->moveSpeed*2;

 if (timecount>msprite->movetime)
  {
   if (px>sx) angle=EAST;
    else if (px<sx) angle=WEST;
    else angle=-1;
   if (py<sy)
    {
     if (angle==EAST) angle+=DEGREE45;
      else if (angle==WEST) angle-=DEGREE45;
      else angle=NORTH;
     }
   else if (py>sy)
    {
     if (angle==EAST) angle-=DEGREE45;
      else if (angle==WEST) angle+=DEGREE45;
      else angle=SOUTH;
     }
   angle=angle - DEGREE45 + MS_RndT();
   msprite->angle=angle&ANGLES;
   msprite->movetime=timecount+140; // 350
   }

 if (timecount>msprite->firetime && timecount>msprite->scantime)
  {
   tx=px;
   ty=py;
   if (ScanX(10,sx,sy,px,py,&tx,&ty)>1 || ScanY(10,sx,sy,px,py,&tx,&ty)>1
    || ScanAngle(10,sx,sy,px,py,&tx,&ty)>1)
    {
     if (tx>sx) angle=EAST;
      else if (tx<sx) angle=WEST;
      else angle=-1;
     if (ty<sy)
      {
       if (angle==EAST) angle+=DEGREE45;
	else if (angle==WEST) angle-=DEGREE45;
	else angle=NORTH;
       }
     else if (ty>sy)
      {
       if (angle==EAST) angle-=DEGREE45;
	else if (angle==WEST) angle+=DEGREE45;
	else angle=SOUTH;
       }
     msprite->angle=angle&ANGLES;
     msprite->basepic=msprite->startpic+24;
     msprite->movemode=4;
     msprite->firetime=timecount+(40+5*player.difficulty);
     msprite->actiontime=timecount+30;
     msprite->modetime=timecount+15;
     }
   msprite->scantime=timecount+30;
   }

 if (timecount>msprite->modetime)
  {
   msprite->modetime=timecount+10;
   switch (msprite->movemode)
    {
     case 0: // left
     case 1: // mid
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	++msprite->movemode;
	msprite->basepic=msprite->startpic+msprite->movemode*8;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 2: // right
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->basepic=msprite->startpic + 8; // midstep
	++msprite->movemode;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 3: // mid #2
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 4: // fire #1
      tx=px;
      ty=py;
      if (ScanX(10,sx,sy,px,py,&tx,&ty)>1 || ScanY(10,sx,sy,px,py,&tx,&ty)>1
       || ScanAngle(10,sx,sy,px,py,&tx,&ty)>1)
       {
	if (tx>sx) angle=EAST;
	 else if (tx<sx) angle=WEST;
	 else angle=-1;
	if (ty<sy)
	 {
	  if (angle==EAST) angle+=DEGREE45;
	   else if (angle==WEST) angle-=DEGREE45;
	   else angle=NORTH;
	  }
	else if (ty>sy)
	 {
	  if (angle==EAST) angle-=DEGREE45;
	   else if (angle==WEST) angle+=DEGREE45;
	   else angle=SOUTH;
	  }
	msprite->angle=angle&ANGLES;
	msprite->movemode=5;
	msprite->basepic=msprite->startpic+32;
	if (msprite->type==S_MONSTER7) fheight=15<<FRACBITS;
	 else fheight=40<<FRACBITS;
	pangle=GetFireAngle(fheight,tx,ty,targx,targy,targz)-15+(MS_RndT()&31);
	SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31),pangle,true,255);
	msprite->modetime+=8;
	}
      else
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	}
      break;
     case 5: // fire #2
      msprite->movemode=0;
      msprite->basepic=msprite->startpic;
      break;
     }
   }

 if (timecount>msprite->actiontime && SP_Thrust2()!=1)
  {
   angle=msprite->angle+DEGREE45;
   msprite->angle=angle&ANGLES;
   }
 floorz=RF_GetFloorZ(msprite->x,msprite->y);
 if (floorz+msprite->zadj<msprite->z)
  msprite->z-=FRACUNIT<<4;
 if (floorz+msprite->zadj>msprite->z)
  msprite->z=floorz+msprite->zadj;
 msprite->moveSpeed=oldspeed;
 if (MS_RndT()>=255)
  ActivationSound(msprite);
 }

/***************************************************************************/

bool Int6() // intelligence for mines
{
 scaleobj_t *sp;
 int        i, j, angle,angleinc, x, y, sx, sy;
 bool    activate;

 if (timecount>msprite->actiontime) // now active
  {
   if (msprite->type==S_TIMEMINE)
    {
     angleinc=ANGLES/20;
     angle=0;
     for(i=0,angle=0;i<20;i++,angle+=angleinc)
      sp=SpawnSprite(S_MINEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,0,true,msprite->spawnid);
     sp=SpawnSprite(S_EXPLODE,msprite->x,msprite->y,msprite->z,0,0,0,true,255);
     SoundEffect(SN_EXPLODE1+(MS_RndT()&1),15,msprite->x,msprite->y);
     return true;
     }
   else if (msprite->type==S_PROXMINE)
    {
     if (MS_RndT()&1) msprite->angle+=8;
      else msprite->angle-=8;
     msprite->angle&=ANGLES;

     x=msprite->x>>FRACTILESHIFT;
     y=msprite->y>>FRACTILESHIFT;
     activate=false;
     if (abs(x-(player.x>>FRACTILESHIFT))<2 && abs(y-(player.y>>FRACTILESHIFT))<2) activate=true;
     if (!activate)
      for (sp=firstscaleobj.next;sp!=&lastscaleobj;sp=sp->next)
       if (sp->hitpoints)
	{
	 sx=sp->x>>FRACTILESHIFT;
	 sy=sp->y>>FRACTILESHIFT;
	 if (abs(x-sx)<2 && abs(y-sy)<2)
	  {
	   activate=true;
	   break;
	   }
	 }
     for(i=-1;i<2;i++)
      for(j=-1;j<2;j++)
       if (mapsprites[(i+y)*MAPCOLS+j+x]==SM_NETPLAYER) activate=true;
     if (activate)
      {
       angleinc=ANGLES/16;
       angle=0;
       for(i=0,angle=0;i<16;i++,angle+=angleinc)
	sp=SpawnSprite(S_MINEBULLET,msprite->x,msprite->y,msprite->z,20<<FRACBITS,angle,0,true,msprite->spawnid);
       sp=SpawnSprite(S_EXPLODE,msprite->x,msprite->y,msprite->z,0,0,0,true,255);
       SoundEffect(SN_EXPLODE1+(MS_RndT()&1),15,msprite->x,msprite->y);
       return true;
       }
     }
   else if (msprite->type==S_INSTAWALL)
    {
     mapsprites[(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT)]=0;
     return true;
     }
   }
 return false;
 }

/***************************************************************************/

int CloneScanX(int x,int y,int *x2)
/* check for the target along the x axis */
{
 int mapspot, wall, x1, limit, flags;

 mapspot=y*MAPCOLS+x+1;
 x1=x;
 limit=10;
 while (1)
  {
   if (mapsprites[mapspot]==SM_CLONE || mapsprites[mapspot]==1)
    {
     *x2=x1+1;
     return 1+(MS_RndT()&1);
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;
   wall=westwall[mapspot];
   flags=westflags[mapspot];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;
   ++mapspot;
   ++x1;
   --limit;
   if (!limit) break;
   }
 limit=10;
 mapspot=y*MAPCOLS+x-1;
 x1=x;
 while (1)
  {
   if (mapsprites[mapspot]==SM_CLONE || mapsprites[mapspot]==1)
    {
     *x2=x1-1;
     return 1+(MS_RndT()&1);
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) return 1;
   wall=westwall[mapspot+1];
   flags=westflags[mapspot+1];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) return 0;
   --mapspot;
   --x1;
   --limit;
   if (!limit) break;
   }
 return 0;
 }


int CloneScanY(int x,int y,int *y2)
/* check for the player along the y axis */
{
 int mapspot, wall, y1, limit, flags;

 limit=10;
 mapspot=y*MAPCOLS+x+MAPCOLS;
 y1=y;
 while (1)
  {
   if (mapsprites[mapspot]==SM_CLONE || mapsprites[mapspot]==1)
    {
     *y2=y1+1;
     return 1+(MS_RndT()&1);
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;
   wall=northwall[mapspot];
   flags=northflags[mapspot];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;
   mapspot+=MAPCOLS;
   ++y1;
   --limit;
   if (!limit) break;
   }
 limit=10;
 mapspot=y*MAPCOLS+x-MAPCOLS;
 y1=y;
 while (1)
  {
   if (mapsprites[mapspot]==SM_CLONE || mapsprites[mapspot]==1)
    {
     *y2=y1-1;
     return 1+(MS_RndT()&1);
     }
   if (mapsprites[mapspot]>0 && mapsprites[mapspot]<128) break;
   wall=northwall[mapspot+MAPCOLS];
   flags=northflags[mapspot+MAPCOLS];
   if (wall && !(flags & F_NOCLIP) && !(flags & F_NOBULLETCLIP)) break;
   mapspot-=MAPCOLS;
   --y1;
   --limit;
   if (!limit) break;
   }
 return 0;
 }


void Int7(void)  // clone ai
{
 int     angle, sx, sy, px, py, pangle, r;
 fixed_t floorz, fheight;

 sx=msprite->x>>FRACTILESHIFT;
 sy=msprite->y>>FRACTILESHIFT;

 if (timecount>msprite->movetime)
  {
   angle=msprite->angle - DEGREE45;
   r=MS_RndT()%3;

   if (r==1)
    angle+=DEGREE45;
   else if (r==2)
    angle+=NORTH;

   msprite->angle=angle&ANGLES;
   msprite->movetime=timecount+250;
   }

 if (timecount>msprite->firetime && timecount>msprite->scantime)
  {
   px=sx;
   py=sy;
   if (CloneScanX(sx,sy,&px)>1)
    {
     if (px>sx) angle=EAST;
      else if (px<sx) angle=WEST;
     msprite->angle=angle&ANGLES;
     msprite->movemode=4;
     msprite->basepic=msprite->startpic+24;
     fheight=40<<FRACBITS;
     pangle=GetFireAngle(fheight,px,py,0,0,0);
     SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle,pangle,true,255);
     msprite->modetime=timecount+8;
     msprite->actiontime=timecount+30;
     msprite->firetime=timecount+30;
     }
   else if (CloneScanY(sx,sy,&py)>1)
    {
     if (py>sy) angle=SOUTH;
      else if (py<sy) angle=NORTH;
     msprite->angle=angle&ANGLES;
     msprite->movemode=4;
     msprite->basepic=msprite->startpic+24;
     fheight=40<<FRACBITS;
     pangle=GetFireAngle(fheight,px,py,0,0,0);
     SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle,pangle,true,255);
     msprite->modetime=timecount+8;
     msprite->actiontime=timecount+30;
     msprite->firetime=timecount+30;
     }
   msprite->scantime=timecount+20;
   }

 if (timecount>msprite->modetime)
  {
   msprite->modetime=timecount+10;
   switch (msprite->movemode)
    {
     case 0: // left
     case 1: // mid
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	++msprite->movemode;
	msprite->basepic=msprite->startpic+msprite->movemode*8;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 2: // right
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->basepic=msprite->startpic + 8;
	++msprite->movemode;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 3: // mid #2
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 4: // fire
      msprite->movemode=0;
      msprite->basepic=msprite->startpic;
      break;
     }
   }
 if (timecount>msprite->actiontime && SP_Thrust2()!=1)
  {
   angle=msprite->angle+DEGREE45;
   msprite->angle=angle&ANGLES;
   }
 floorz=RF_GetFloorZ(msprite->x,msprite->y);
 if (floorz+msprite->zadj<msprite->z) msprite->z-=FRACUNIT<<4;
 if (floorz+msprite->zadj>msprite->z) msprite->z=floorz+msprite->zadj;
 }

/***************************************************************************/

void Int8(void)  // prisoners
{
 int     angle, sx, sy, px, py, tx, ty, pangle;
 fixed_t floorz, oldspeed, fheight;

 sx=msprite->x>>FRACTILESHIFT;
 sy=msprite->y>>FRACTILESHIFT;
 if (netmode) NetGetClosestPlayer(sx,sy);
 else
  {
   if (specialeffect==SE_INVISIBILITY)
    {
     targx=0;
     targy=0;
     targz=0;
     }
   else
    {
     targx=player.x;
     targy=player.y;
     targz=player.z;
     }
   }
 px=targx>>FRACTILESHIFT;
 py=targy>>FRACTILESHIFT;

 oldspeed=msprite->moveSpeed;
 if (abs(px-sx)<6 && abs(py-sy)<6) msprite->moveSpeed=msprite->moveSpeed*2;

 if (timecount>msprite->movetime)
  {
   if (px>sx) angle=EAST;
    else if (px<sx) angle=WEST;
    else angle=-1;
   if (py<sy)
    {
     if (angle==EAST) angle+=DEGREE45;
      else if (angle==WEST) angle-=DEGREE45;
      else angle=NORTH;
     }
   else if (py>sy)
    {
     if (angle==EAST) angle-=DEGREE45;
      else if (angle==WEST) angle+=DEGREE45;
      else angle=SOUTH;
     }
   angle=angle - DEGREE45 + MS_RndT();
   msprite->angle=angle&ANGLES;
   msprite->movetime=timecount+350;
   }

 if (timecount>msprite->firetime && timecount>msprite->scantime)
  {
   tx=px;
   ty=py;
   if (ScanX(7,sx,sy,px,py,&tx,&ty)>1 || ScanY(7,sx,sy,px,py,&tx,&ty)>1
    || ScanAngle(7,sx,sy,px,py,&tx,&ty)>1)
    {
     if (tx>sx) angle=EAST;
      else if (tx<sx) angle=WEST;
      else angle=-1;
     if (ty<sy)
      {
       if (angle==EAST) angle+=DEGREE45;
	else if (angle==WEST) angle-=DEGREE45;
	else angle=NORTH;
       }
     else if (ty>sy)
      {
       if (angle==EAST) angle-=DEGREE45;
	else if (angle==WEST) angle+=DEGREE45;
	else angle=SOUTH;
       }
     msprite->angle=angle&ANGLES;

     if (abs(tx-sx)>2 || abs(ty-sy)>2)
      {
       msprite->scantime=timecount+45;
       goto endscan;
       }

     msprite->basepic=msprite->startpic+24;
     msprite->movemode=4;
     msprite->firetime=timecount+(80+5*player.difficulty);
     msprite->actiontime=timecount+30;
     msprite->modetime=timecount+15;
     }
   msprite->scantime=timecount+45;
   }

endscan:
 if (timecount>msprite->modetime)
  {
   msprite->modetime=timecount+10;
   switch (msprite->movemode)
    {
     case 0: // left
     case 1: // mid
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	++msprite->movemode;
	msprite->basepic=msprite->startpic+msprite->movemode*8;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 2: // right
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->basepic=msprite->startpic + 8; // midstep
	++msprite->movemode;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 3: // mid #2
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 4: // fire #1
      tx=px;
      ty=py;
      if (ScanX(7,sx,sy,px,py,&tx,&ty)>1 || ScanY(7,sx,sy,px,py,&tx,&ty)>1
       || ScanAngle(7,sx,sy,px,py,&tx,&ty)>1)
       {
	if (tx>sx) angle=EAST;
	 else if (tx<sx) angle=WEST;
	 else angle=-1;
	if (ty<sy)
	 {
	  if (angle==EAST) angle+=DEGREE45;
	   else if (angle==WEST) angle-=DEGREE45;
	   else angle=NORTH;
	  }
	else if (ty>sy)
	 {
	  if (angle==EAST) angle-=DEGREE45;
	   else if (angle==WEST) angle+=DEGREE45;
	   else angle=SOUTH;
	  }

	msprite->angle=angle&ANGLES;

	if (abs(tx-sx)>2 || abs(ty-sy)>2)
	 {
	  msprite->movemode=0;
	  msprite->basepic=msprite->startpic;
	  break;
	  }

	msprite->movemode=5;
	msprite->basepic=msprite->startpic+32;
	fheight=40<<FRACBITS;
	pangle=GetFireAngle(fheight,tx,ty,targx,targy,targz);
	SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle,pangle,true,255);
	msprite->modetime+=8;
	}
      else
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	}
      break;
     case 5: // fire #2
      msprite->movemode=0;
      msprite->basepic=msprite->startpic;
      break;
     }
   }

 if (timecount>msprite->actiontime && SP_Thrust2()!=1)
  {
   angle=msprite->angle+DEGREE45;
   msprite->angle=angle&ANGLES;
   }
 floorz=RF_GetFloorZ(msprite->x,msprite->y);
 if (floorz+msprite->zadj<msprite->z)
  msprite->z-=FRACUNIT<<4;
 if (floorz+msprite->zadj>msprite->z)
  msprite->z=floorz+msprite->zadj;
 msprite->moveSpeed=oldspeed;
 if (MS_RndT()>=255)
  ActivationSound(msprite);
 }

/***************************************************************************/

void Int9(void)  // big guards only
{
 int     i, angleinc, angle, sx, sy, px, py, tx, ty, pangle;
 fixed_t floorz, oldspeed, fheight;

 if (msprite->hitpoints<1000)
  msprite->hitpoints+=10;
 msprite->enraged=0;
 sx=msprite->x>>FRACTILESHIFT;
 sy=msprite->y>>FRACTILESHIFT;
 if (netmode) NetGetClosestPlayer(sx,sy);
 else
  {
   if (specialeffect==SE_INVISIBILITY)
    {
     targx=0;
     targy=0;
     targz=0;
     }
   else
    {
     targx=player.x;
     targy=player.y;
     targz=player.z;
     }
   }
 px=targx>>FRACTILESHIFT;
 py=targy>>FRACTILESHIFT;

 oldspeed=msprite->moveSpeed;
 if (abs(px-sx)<6 && abs(py-sy)<6) msprite->moveSpeed=msprite->moveSpeed*2;

 if (timecount>msprite->movetime)
  {
   if (px>sx) angle=EAST;
    else if (px<sx) angle=WEST;
    else angle=-1;
   if (py<sy)
    {
     if (angle==EAST) angle+=DEGREE45;
      else if (angle==WEST) angle-=DEGREE45;
      else angle=NORTH;
     }
   else if (py>sy)
    {
     if (angle==EAST) angle-=DEGREE45;
      else if (angle==WEST) angle+=DEGREE45;
      else angle=SOUTH;
     }
   angle=angle - DEGREE45 + MS_RndT();
   msprite->angle=angle&ANGLES;
   msprite->movetime=timecount+200;
   }

 if (timecount>msprite->firetime && timecount>msprite->scantime)
  {
   SoundEffect(SN_MON11_WAKE,7,msprite->x,msprite->y);
   tx=px;
   ty=py;
   if (ScanX(8,sx,sy,px,py,&tx,&ty)>1 || ScanY(8,sx,sy,px,py,&tx,&ty)>1
    || ScanAngle(8,sx,sy,px,py,&tx,&ty)>1)
    {
     if (tx>sx) angle=EAST;
      else if (tx<sx) angle=WEST;
      else angle=-1;
     if (ty<sy)
      {
       if (angle==EAST) angle+=DEGREE45;
	else if (angle==WEST) angle-=DEGREE45;
	else angle=NORTH;
       }
     else if (ty>sy)
      {
       if (angle==EAST) angle-=DEGREE45;
	else if (angle==WEST) angle+=DEGREE45;
	else angle=SOUTH;
       }
     msprite->angle=angle&ANGLES;
     msprite->basepic=msprite->startpic+24;
     msprite->movemode=4;
     msprite->firetime=timecount+(120+5*player.difficulty);
     msprite->actiontime=timecount+30;
     msprite->modetime=timecount+20;
     }
   msprite->scantime=timecount+45;
   }

 if (timecount>msprite->modetime)
  {
   msprite->modetime=timecount+20;
   switch (msprite->movemode)
    {
     case 0: // left
     case 1: // mid
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	++msprite->movemode;
	msprite->basepic=msprite->startpic+msprite->movemode*8;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      if (MS_RndT()<32)
       {
	angle=0;
	angleinc=ANGLES/16;
	for (i=0;i<16;i++,angle+=angleinc)
	 SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,32<<FRACBITS,angle,0,true,255);
	SoundEffect(SN_MON11_FIRE,7,msprite->x,msprite->y);
	}
      break;
     case 2: // right
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->basepic=msprite->startpic + 8; // midstep
	++msprite->movemode;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      if (MS_RndT()<32)
       {
	angle=0;
	angleinc=ANGLES/16;
	for (i=0;i<16;i++,angle+=angleinc)
	 SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,32<<FRACBITS,angle,0,true,255);
	SoundEffect(SN_MON11_FIRE,7,msprite->x,msprite->y);
	}
      break;
     case 3: // mid #2
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      if (MS_RndT()<32)
       {
	angle=0;
	angleinc=ANGLES/16;
	for (i=0;i<16;i++,angle+=angleinc)
	 SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,32<<FRACBITS,angle,0,true,255);
	SoundEffect(SN_MON11_FIRE,7,msprite->x,msprite->y);
	}
      break;
     case 4: // firing bullets
     case 5:
     case 6:
     case 7:
      tx=px;
      ty=py;
      if (ScanX(8,sx,sy,px,py,&tx,&ty)>1 || ScanY(8,sx,sy,px,py,&tx,&ty)>1
       || ScanAngle(8,sx,sy,px,py,&tx,&ty)>1)
       {
	if (tx>sx) angle=EAST;
	 else if (tx<sx) angle=WEST;
	 else angle=-1;
	if (ty<sy)
	 {
	  if (angle==EAST) angle+=DEGREE45;
	   else if (angle==WEST) angle-=DEGREE45;
	   else angle=NORTH;
	  }
	else if (ty>sy)
	 {
	  if (angle==EAST) angle-=DEGREE45;
	   else if (angle==WEST) angle+=DEGREE45;
	   else angle=SOUTH;
	  }
	msprite->angle=angle&ANGLES;
	++msprite->movemode;
	msprite->basepic=msprite->startpic+32;
	fheight=70<<FRACBITS;
	if (msprite->movemode==5 && MS_RndT()<32)
	 {
	  SpawnSprite(S_GRENADE,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31),0,true,255);
	  SpawnSprite(S_GRENADE,msprite->x,msprite->y,msprite->z,fheight,msprite->angle+15+(MS_RndT()&31),0,true,255);
	  SoundEffect(SN_GRENADE,0,msprite->x,msprite->y);
	  msprite->movemode=0;
	  msprite->basepic=msprite->startpic;
	  msprite->firetime=timecount+(120+5*player.difficulty);
	  msprite->actiontime=timecount+30;
	  }
	else
	 {
	  pangle=GetFireAngle(fheight,tx,ty,targx,targy,targz)-15+(MS_RndT()&31);
	  SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31),pangle,true,255);
	  SoundEffect(SN_MON11_FIRE,7,msprite->x,msprite->y);
	  msprite->modetime=timecount+15;
	  }
	}
      else
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	msprite->firetime=timecount+(120+5*player.difficulty);
	msprite->actiontime=timecount+30;
	}
      break;
     case 8: // fire #2
      msprite->movemode=0;
      msprite->basepic=msprite->startpic;
      msprite->firetime=timecount+(120+5*player.difficulty);
      msprite->actiontime=timecount+30;
      break;
     }
   }

 if (timecount>msprite->actiontime && SP_Thrust2()!=1)
  {
   angle=msprite->angle+DEGREE45;
   msprite->angle=angle&ANGLES;
   }
 floorz=RF_GetFloorZ(msprite->x,msprite->y);
 if (floorz+msprite->zadj<msprite->z)
  msprite->z-=FRACUNIT<<4;
 if (floorz+msprite->zadj>msprite->z)
  msprite->z=floorz+msprite->zadj;
 msprite->moveSpeed=oldspeed;
 if (MS_RndT()>=255)
  ActivationSound(msprite);
 }

/***************************************************************************/

void Int10(void)
{
 int     angle, sx, sy, px, py, tx, ty, pangle;
 fixed_t floorz, oldspeed, fheight;

 if (msprite->type==S_MONSTER5 && msprite->hitpoints<5000)
  msprite->hitpoints+=4;
 else if (msprite->type==S_MONSTER13 && msprite->hitpoints<300)
  {
   msprite->hitpoints+=25;
   msprite->enraged=0;
   }
 else if (msprite->type==S_MONSTER15 && msprite->hitpoints<2000)
  {
   msprite->hitpoints+=6;
   msprite->enraged=0;
   }
 else if (msprite->type==S_MONSTER14 && msprite->hitpoints<350)
  msprite->hitpoints+=1;
 sx=msprite->x>>FRACTILESHIFT;
 sy=msprite->y>>FRACTILESHIFT;
 if (netmode) NetGetClosestPlayer(sx,sy);
 else
  {
   if (specialeffect==SE_INVISIBILITY)
    {
     targx=0;
     targy=0;
     targz=0;
     }
   else
    {
     targx=player.x;
     targy=player.y;
     targz=player.z;
     }
   }
 px=targx>>FRACTILESHIFT;
 py=targy>>FRACTILESHIFT;

 oldspeed=msprite->moveSpeed;
 if (abs(px-sx)<6 && abs(py-sy)<6) msprite->moveSpeed=msprite->moveSpeed*2;

 if (timecount>msprite->movetime)
  {
   if (px>sx) angle=EAST;
    else if (px<sx) angle=WEST;
    else angle=-1;
   if (py<sy)
    {
     if (angle==EAST) angle+=DEGREE45;
      else if (angle==WEST) angle-=DEGREE45;
      else angle=NORTH;
     }
   else if (py>sy)
    {
     if (angle==EAST) angle-=DEGREE45;
      else if (angle==WEST) angle+=DEGREE45;
      else angle=SOUTH;
     }
   angle=angle - DEGREE45 + MS_RndT();
   msprite->angle=angle&ANGLES;
   msprite->movetime=timecount+350;
   }

 if (timecount>msprite->firetime && timecount>msprite->scantime)
  {
   tx=px;
   ty=py;
   if (ScanX(10,sx,sy,px,py,&tx,&ty)>1 || ScanY(10,sx,sy,px,py,&tx,&ty)>1
    || ScanAngle(10,sx,sy,px,py,&tx,&ty)>1)
    {
     if (tx>sx) angle=EAST;
      else if (tx<sx) angle=WEST;
      else angle=-1;
     if (ty<sy)
      {
       if (angle==EAST) angle+=DEGREE45;
	else if (angle==WEST) angle-=DEGREE45;
	else angle=NORTH;
       }
     else if (ty>sy)
      {
       if (angle==EAST) angle-=DEGREE45;
	else if (angle==WEST) angle+=DEGREE45;
	else angle=SOUTH;
       }
     msprite->angle=angle&ANGLES;

     if (msprite->type==S_MONSTER7 && (abs(tx-sx)>2 || abs(ty-sy)>2))
      {
       msprite->scantime=timecount+30;
       goto endscan;
       }

     if (msprite->type==S_MONSTER15 && (abs(tx-sx)>4 || abs(ty-sy)>4))
      {
       msprite->scantime=timecount+30;
       goto endscan;
       }

     msprite->basepic=msprite->startpic+32;
     msprite->movemode=6;
     if (msprite->type==S_MONSTER5)
      msprite->firetime=timecount+(10+3*player.difficulty);
     else
      msprite->firetime=timecount+(40+5*player.difficulty);

     msprite->actiontime=timecount+30;
     msprite->modetime=timecount+15;
     if (msprite->type==S_MONSTER3)
      fheight=3<<FRACBITS;
     else if (msprite->type==S_MONSTER6)
      fheight=100<<FRACBITS;
     else
      fheight=40<<FRACBITS;

     pangle=GetFireAngle(fheight,tx,ty,targx,targy,targz)-15+(MS_RndT()&31);
     if (msprite->type==S_MONSTER13 || msprite->type==S_MONSTER6 || msprite->type==S_MONSTER15 || msprite->type==S_MONSTER5)
      {
       SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31)+16,pangle,true,255);
       SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31)-16,pangle,true,255);
       SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31),pangle,true,255);
       }
     else if (msprite->type==S_MONSTER4)
      {
       SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31)+8,pangle,true,255);
       SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31)-8,pangle,true,255);
       }
     else
      SpawnSprite(msprite->bullet,msprite->x,msprite->y,msprite->z,fheight,msprite->angle-15+(MS_RndT()&31),pangle,true,255);
     }
   msprite->scantime=timecount+30;
   }
endscan:
 if (timecount>msprite->modetime)
  {
   msprite->modetime=timecount+8;
   switch (msprite->movemode)
    {
     case 0: // 1
     case 1: // 2
     case 2: // 3
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	++msprite->movemode;
	msprite->basepic=msprite->startpic+msprite->movemode*8;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 3: // 2
     case 4: // 1
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->movemode++;
	msprite->basepic=msprite->startpic+(6-msprite->movemode)*8;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 5:
      if (msprite->lastx!=msprite->x || msprite->lasty!=msprite->y)
       {
	msprite->movemode=0;
	msprite->basepic=msprite->startpic;
	msprite->lasty=msprite->y;
	msprite->lastx=msprite->x;
	}
      break;
     case 6: // fire
      msprite->movemode=0;
      msprite->basepic=msprite->startpic;
      break;
     }
  }

 if (timecount>msprite->actiontime && SP_Thrust2()!=1)
  {
   angle=msprite->angle+DEGREE45;
   msprite->angle=angle&ANGLES;
   }
 floorz=RF_GetFloorZ(msprite->x,msprite->y) + msprite->zadj;
 if (floorz<msprite->z)
  msprite->z-=FRACUNIT<<4;
 if (floorz>msprite->z)
  msprite->z=floorz;
 msprite->moveSpeed=oldspeed;
 if (MS_RndT()>=255)
  ActivationSound(msprite);
 }


/***************************************************************************/

void MoveSprites(void)
{
 int     mapspot, i, j, c, px, py, sx, sy;
 bool killed;
 fixed_t floor;

 if (!netmode)
  {
   targx=player.x;
   targy=player.y;
   px=targx>>FRACTILESHIFT;
   py=targy>>FRACTILESHIFT;
   }

 for (msprite=firstscaleobj.next;msprite!=&lastscaleobj;msprite=msprite->next)
  {
   if (msprite->active)
    {
     if (msprite->moveSpeed)
      switch (msprite->intelligence)
       {
	case 0:
	 killed=Int0();
	 if (killed)
	  {
	   if (msprite->type==S_BLOODSPLAT)
	    {
	     msprite->intelligence=128;
	     break;
	     }
	   else if (msprite->type==S_METALPARTS && !spritehit)
	    {
	     killed=false;
	     continue;
	     }
	   msprite=msprite->prev;
	   RF_RemoveSprite(msprite->next);
	   killed=false;
	   continue;
	   }
	 break;
	case 5:
	 Int5();
	 break;
	case 6:
	 killed=Int6();
	 if (killed)
	  {
	   msprite=msprite->prev;
	   RF_RemoveSprite(msprite->next);
	   killed=false;
	   continue;
	   }
	 break;
	case 7:
	 Int7();
	 break;
	case 8:
	 Int8();
	 break;
	case 9:
	 Int9();
	 break;
	case 10:
	 Int10();
	 break;
	case 128:
	 floor=RF_GetFloorZ(msprite->x,msprite->y);
	 if (msprite->z>floor+FRACUNIT)
	  msprite->z-=FRACUNIT*2;
	 if (msprite->z<floor)
	  msprite->z=floor;
	 break;
	}
     if (!killed && msprite->heat)
      {
       mapspot=(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT);
       if (msprite->heat>256)
	{
	 c=msprite->heat>>1;
	 for(i=-1;i<2;i++)
	  for(j=-1;j<2;j++)
	   reallight[mapspot+(i*MAPCOLS)+j]-=c;
	 reallight[mapspot]-=msprite->heat>>2;
	 }
       else reallight[mapspot]-=msprite->heat;
       }
     killed=false;
     }
   else if (msprite->intelligence!=255)
    {

     if (msprite->moveSpeed)
      {

       sx=msprite->x>>FRACTILESHIFT;
       sy=msprite->y>>FRACTILESHIFT;
       if (netmode)
	{
	 NetGetClosestPlayer(sx,sy);
	 px=targx>>FRACTILESHIFT;
	 py=targy>>FRACTILESHIFT;
	 }

       if ((abs(px-sx)<6 && abs(py-sy)<6))
	{
	 msprite->active=true;
	 ActivateSprites(sx,sy);
	 }
       }

     floor=RF_GetFloorZ(msprite->x,msprite->y)+msprite->zadj;
     if (msprite->z>floor) msprite->z-=FRACUNIT<<4;
     if (msprite->z<floor) msprite->z=floor;
     if (msprite->heat)
      {
       mapspot=(msprite->y>>FRACTILESHIFT)*MAPCOLS+(msprite->x>>FRACTILESHIFT);
       if (msprite->heat>256)
	{
	 c=msprite->heat>>1;
	 for(i=-1;i<2;i++)
	  for(j=-1;j<2;j++)
	   reallight[mapspot+(i*MAPCOLS)+j]-=c;
	 reallight[mapspot]-=msprite->heat>>2;
	 msprite->heat-=64;
	 }
       else reallight[mapspot]-=msprite->heat;
       }
     }
   }
 }
