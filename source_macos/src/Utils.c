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
#include <IO.H>
#include <FCNTL.H>
#include <TIME.H>
#include <sys/stat.h>
#include "d_disk.h"
#include "d_global.h"
#include "r_refdef.h"
#include "d_font.h"
#include "protos.h"
#include "d_ints.h"
#include "d_misc.h"

#ifdef GAME1
 #define SAVENAME "SAVE1.%i"
#elif defined(GAME2)
 #define SAVENAME "SAVE2.%i"
#elif defined(GAME3)
 #define SAVENAME "SAVE3.%i"
#else
 #define SAVENAME "SAVEGAME.%i"
#endif


/**** VARIABLES ****/

int primaries[4], secondaries[14], pcount[2], scount[7], bonustime;
extern int cdr_drivenum;

longint levelscore;

bool gameloading, eventloading;

int startlocations[MAXSTARTLOCATIONS][2];

extern bool redo;
extern int fxtimecount;
extern SoundCard SC;


/**** FUNCTIONS ****/

void KillSprite(scaleobj_t *sp, int weapon)
{
 scaleobj_t *s;
 int        i;
 fixed_t    x, y, z;

 if (sp->deathevent)
  Event(sp->deathevent,false);
 switch (sp->type)
  {
   case S_CLONE:
    if (sp->startpic==CA_GetNamedNum(charnames[0]))
     {
      s=SpawnSprite(S_TIMEMINE,sp->x,sp->y,0,0,0,0,false,playernum);
      s->basepic=sp->startpic+40;
      s->scale=1;
      sp->animation=0 + (0<<1) + (1<<5) + (0<<9) + ANIM_SELFDEST;
      }
    else
      sp->animation=0 + (0<<1) + (8<<5) + ((4+(MS_RndT()&3))<<9);
    sp->basepic=sp->startpic+40;
    sp->rotate=rt_one;
    sp->heat=0;
    sp->active=false;
    sp->moveSpeed=0;
    sp->hitpoints=0;
    break;
   case S_MONSTER1:
   case S_MONSTER2:
   case S_MONSTER5:
   case S_MONSTER7:
   case S_MONSTER8:
   case S_MONSTER9:
   case S_MONSTER10:
   case S_MONSTER12:
   case S_MONSTER13:
   case S_MONSTER14:
   case S_MONSTER15:
    sp->basepic=sp->startpic+48;
    sp->animation=0 + (0<<1) + (8<<5) + ((2+(MS_RndT()&3))<<9);
    switch (sp->type)
     {
      case S_MONSTER1:
       SoundEffect(SN_MON1_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER2:
       SoundEffect(SN_MON2_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER5:
       SoundEffect(SN_MON5_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER7:
       SoundEffect(SN_MON7_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER8:
       SoundEffect(SN_MON8_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER9:
       SoundEffect(SN_MON9_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER10:
       SoundEffect(SN_MON10_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER12:
       SoundEffect(SN_MON12_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER13:
       SoundEffect(SN_MON13_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER14:
       SoundEffect(SN_MON14_DIE,7,sp->x,sp->y);
       break;
      case S_MONSTER15:
       SoundEffect(SN_MON15_DIE,7,sp->x,sp->y);
       break;
      }
    sp->rotate=rt_one;
    sp->heat=0;
    sp->active=false;
    sp->moveSpeed=0;
    sp->hitpoints=0;
    break;
   case S_MONSTER3:
    SpawnSprite(S_EXPLODE,sp->x,sp->y,sp->z,0,0,0,false,0);
    SoundEffect(SN_MON3_DIE,7,sp->x,sp->y);
    RF_RemoveSprite(sp);
    break;
   case S_MONSTER4:
    SpawnSprite(S_EXPLODE,sp->x,sp->y,sp->z,0,0,0,false,0);
    SoundEffect(SN_MON4_DIE,7,sp->x,sp->y);
    RF_RemoveSprite(sp);
    break;
   case S_MONSTER6:
   case S_MONSTER11:
    for(i=0;i<30;i++)
     SpawnSprite(S_METALPARTS,sp->x,sp->y,sp->z+64*FRACUNIT,0,0,0,false,0);
    for(i=0;i<10;i++)
     {
      x=sp->x + ((-64+(MS_RndT()&127))<<FRACBITS);
      y=sp->y + ((-64+(MS_RndT()&127))<<FRACBITS);
      z=sp->z + ((MS_RndT()&127)<<FRACBITS);
      SpawnSprite(S_EXPLODE+(MS_RndT()&1),x,y,z,0,0,0,false,0);
      }
    SoundEffect(SN_EXPLODE1+(clock()&1),15,x,y);
    SoundEffect(SN_MON11_DIE,7,sp->x,sp->y);
    SoundEffect(SN_MON11_DIE,7,sp->x,sp->y);
    RF_RemoveSprite(sp);
    break;
   default:
    MS_Error("Illegal KillSprite: type %i",sp->type);
   }
 }


void ActivateSpritesFromMap(void)
{
 int x, y;

 gameloading=true;
 for(y=0;y<MAPROWS;y++)
  for(x=0;x<MAPCOLS;x++)
   if (mapsprites[y*MAPCOLS+x])
    SpawnSprite((int)mapsprites[y*MAPCOLS+x],(fixed_t)(x*MAPSIZE+32)<<FRACBITS,
     (fixed_t)(y*MAPCOLS+32)<<FRACBITS,0,0,0,0,false,0);
 gameloading=false;
 }


void ActivateSlopes(void)
{
 int i, j, mapspot;

 for(i=0;i<MAPCOLS;i++)
  for(j=0;j<MAPROWS;j++)
   {
    mapspot=i*MAPCOLS+j;
    switch (mapslopes[mapspot])
     {
      case 49:
       mapflags[mapspot]|=POLY_SLOPE<<FLS_CEILING;
       break;
      case 50:
       mapflags[mapspot]|=POLY_URTOLL<<FLS_CEILING;
       break;
      case 51:
       mapflags[mapspot]|=POLY_ULTOLR<<FLS_CEILING;
       break;

      case 52:
       mapflags[mapspot]|=POLY_SLOPE;
       break;
      case 53:
       mapflags[mapspot]|=POLY_SLOPE;
       mapflags[mapspot]|=POLY_URTOLL<<FLS_CEILING;
       break;
      case 54:
       mapflags[mapspot]|=POLY_SLOPE;
       mapflags[mapspot]|=POLY_ULTOLR<<FLS_CEILING;
       break;

      case 55:
       mapflags[mapspot]|=POLY_URTOLL;
       mapflags[mapspot]|=POLY_SLOPE<<FLS_CEILING;
       break;
      case 56:
       mapflags[mapspot]|=POLY_URTOLL;
       break;
      case 57:
       mapflags[mapspot]|=POLY_URTOLL;
       mapflags[mapspot]|=POLY_ULTOLR<<FLS_CEILING;
       break;

      case 58:
       mapflags[mapspot]|=POLY_ULTOLR;
       mapflags[mapspot]|=POLY_SLOPE<<FLS_CEILING;
       break;
      case 59:
       mapflags[mapspot]|=POLY_ULTOLR;
       mapflags[mapspot]|=POLY_URTOLL<<FLS_CEILING;
       break;
      case 60:
       mapflags[mapspot]|=POLY_ULTOLR;
       break;

      case 61:
       mapflags[mapspot]|=POLY_SLOPE;
       mapflags[mapspot]|=POLY_SLOPE<<FLS_CEILING;
       break;
      case 62:
       mapflags[mapspot]|=POLY_URTOLL;
       mapflags[mapspot]|=POLY_URTOLL<<FLS_CEILING;
       break;
      case 63:
       mapflags[mapspot]|=POLY_ULTOLR;
       mapflags[mapspot]|=POLY_ULTOLR<<FLS_CEILING;
       break;
      }
    }
 }


void LoadTextures(void)
{
 char textures[256];
 int  i, x, size, numsprites, startsprites;
 byte *base, *wall;

 startsprites=CA_GetNamedNum("startdemand");
 numsprites=CA_GetNamedNum("enddemand")-startsprites;
 for (i=1; i<numsprites; i++)
  CA_FreeLump(startsprites+i);
 UpdateWait();
 DemandLoadMonster(CA_GetNamedNum(charnames[player.chartype]),48);
 UpdateWait();
 if (debugmode)
  {
   for (i=0;i<numwalls-1;i++)
    {
     wall=lumpmain[walllump+i+1];
     base=wall+65*2;
     size=*wall*4;
     for (x=0;x<64;x++)
      wallposts[i*64+x]=base+size*x;
     }
   return;
   }
 UpdateWait();
 for(i=1;i<numwalls-7;i++) CA_FreeLump(walllump+i);
 UpdateWait();
 if (wallposts)
  free(wallposts);
 memset(textures,0,sizeof(textures));
 UpdateWait();
 for(i=0;i<MAPCOLS*MAPROWS;i++)
  {
   textures[northwall[i]]=1;
   textures[westwall[i]]=1;
   textures[floordef[i]]=1;
   textures[ceilingdef[i]]=1;
   }
 UpdateWait();
 textures[3]=1;    // for sides of doors

 if (textures[228] || textures[229] || textures[230])
  {
   textures[228]=1;  // animation textures
   textures[229]=1;
   textures[230]=1;
   }
 if (textures[172] || textures[173])
  {
   textures[172]=1;  // switch textures
   textures[173]=1;
   }
 if (textures[127] || textures[128])
  {
   textures[127]=1;
   textures[128]=1;
   }
 if (textures[75] || textures[76])
  {
   textures[75]=1;
   textures[76]=1;
   }
 if (textures[140] || textures[141])
  {
   textures[140]=1;
   textures[141]=1;
   }
 if (textures[234] || textures[235])
  {
   textures[234]=1;
   textures[235]=1;
   }

 UpdateWait();
 for(i=1;i<numwalls;i++)
  if (textures[i])
   {
    CA_CacheLump(walllump+i);
    UpdateWait();
    }
 /* wallposts is byte**, so this must size by pointer.  The original *4 was the
    32-bit pointer size; on LP64 it allocated half of what the loop below
    writes, overflowing the heap and handing back NULL texture columns --
    which crashed ScalePost the moment a wall came into view. */
 wallposts=malloc((size_t)(numwalls+1)*64*sizeof(byte *));
 UpdateWait();

	for ( i = 0 ; i < numwalls - 1 ; i++ )
	{
		wall = lumpmain[walllump + i + 1];
		if ( wall )
		{
			base = wall + 65 * 2;
			size = *wall * 4;
			for ( x = 0 ; x < 64 ; x++ )
				wallposts[i * 64 + x] = base + size * x;
		}
	}

 UpdateWait();
 for(i=1;i<numflats;i++) CA_FreeLump(flatlump+i);
 UpdateWait();
 memset(textures,0,sizeof(textures));
 UpdateWait();
 for(i=0;i<MAPCOLS*MAPROWS;i++)
  {
   textures[floorpic[i]]=1;
   textures[ceilingpic[i]]=1;
   }
 UpdateWait();
 if (textures[57] || textures[58] || textures[59])
  {
   textures[57]=1;  // animation textures
   textures[58]=1;
   textures[59]=1;
   }
 if (textures[217] || textures[218] || textures[219])
  {
   textures[217]=1;  // animation textures
   textures[218]=1;
   textures[219]=1;
   }
 textures[133]=1;
 textures[134]=1;
 textures[135]=1;
 for(i=1;i<numflats;i++)
//  if (textures[i])
   {
    CA_CacheLump(flatlump+i);
    UpdateWait();
    }
 }


/* Count floor/ceiling tiles the renderer cannot draw a flat for.

   Only 1..numflats-1 are real textures: 0 is the `startflats` marker and
   numflats is `endflats`, both zero length.  Flat 0 turns out to be simply how
   the maps encode a solid tile -- 3213 of 4096 on the first level -- so this is
   normal data, not corruption, and the count is only interesting to a developer
   watching it change.  What matters is that RenderTileEnds no longer walks into
   lumpmain[flatlump+0] and dereferences NULL when a wide field of view reaches
   one of those tiles.

   Anything at or past numflats would be genuinely out of range; it is folded
   into the same count because the handling is identical. */
void CheckMapFlats(void)
{
 int i, badfloor=0, badceiling=0, firstbad=-1;

 for(i=0;i<MAPROWS*MAPCOLS;i++)
  {
   if (floorpic[i]<1 || floorpic[i]>=numflats)
    { badfloor++; if (firstbad<0) firstbad=i; }
   if (ceilingpic[i]<1 || ceilingpic[i]>=numflats)
    { badceiling++; if (firstbad<0) firstbad=i; }
   }
 if (debugmode && (badfloor || badceiling))
  printf("Map:\t%d floor and %d ceiling tiles carry no flat "
         "(first at mapspot %d); those surfaces are skipped\n",
         badfloor,badceiling,firstbad);
 }


void LoadNewMap(int lump)
{
 int  i, j, f;
 char *fname;

 StartWait();
 for(i=0;i<S_END-S_START+1;i++)
  slumps[i]=CA_GetNamedNum(slumpnames[i]);
 UpdateWait();
 goalitem=-1;
 oldgoalitem=-1;
 togglegoalitem=true;
 RF_ClearWorld();
 UpdateWait();
 if (!MS_CheckParm("file"))
  {
   lseek(cachehandle,infotable[lump].filepos,SEEK_SET);
   UpdateWait();
   read(cachehandle,northwall,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,northflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,westwall,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,westflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,floorpic,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,floorflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,ceilingpic,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,ceilingflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,floorheight,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,ceilingheight,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,floordef,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,floordefflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,ceilingdef,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,ceilingdefflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,maplights,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,mapeffects,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,mapsprites,MAPROWS*MAPCOLS);
   UpdateWait();
   read(cachehandle,mapslopes,MAPROWS*MAPCOLS);
   UpdateWait();
   }
 else
  {
   fname=infotable[lump].nameofs + (char *)infotable;
   if ((f=_open(fname,O_RDONLY|O_BINARY,S_IREAD))==-1) MS_Error("LoadNewMap: Can't open %s!",fname);
   UpdateWait();
   read(f,northwall,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,northflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,westwall,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,westflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,floorpic,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,floorflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,ceilingpic,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,ceilingflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,floorheight,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,ceilingheight,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,floordef,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,floordefflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,ceilingdef,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,ceilingdefflags,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,maplights,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,mapeffects,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,mapsprites,MAPROWS*MAPCOLS);
   UpdateWait();
   read(f,mapslopes,MAPROWS*MAPCOLS);
   UpdateWait();
   close(f);
   }
 memset(mapflags,0,sizeof(mapflags));
 UpdateWait();
 for(i=0;i<MAPCOLS;i++)
  for(j=0;j<MAPROWS;j++)
   {
    if (floordef[i*64+j]==0) floordef[i*64+j]=56;
    if (ceilingdef[i*64+j]==0) ceilingdef[i*64+j]=56;
    }
 UpdateWait();
 ActivateSlopes();
 UpdateWait();
 LoadTextures();
 CheckMapFlats();
 }


void loadweapon(int n)
{
 static weaponlump=0, numlumps=0;
 int i;

 if (weaponlump)
  for (i=0;i<numlumps;i++)
   CA_FreeLump(weaponlump+i);
 weapons[n].charge=100;
 weapons[n].chargetime=timecount+weapons[n].chargerate;
 switch (n)
  {
   case 1:
    i=CA_GetNamedNum("gun2");
    weaponlump=i;
    numlumps=3;
    if (netmode) NetGetData();
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    break;
   case 2:
    i=CA_GetNamedNum("gun3");
    weaponlump=i;
    numlumps=4;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    weaponpic[3]=CA_CacheLump(i+3);
    if (netmode) NetGetData();
    break;
   case 3:
    i=CA_GetNamedNum("gun4");
    weaponlump=i;
    numlumps=4;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    weaponpic[3]=CA_CacheLump(i+3);
    if (netmode) NetGetData();
    break;
   case 4:
    i=CA_GetNamedNum("gun5");
    weaponlump=i;
    numlumps=4;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    weaponpic[3]=CA_CacheLump(i+3);
    if (netmode) NetGetData();
    break;
   case 7:
    i=CA_GetNamedNum("gunsquar");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 8:
    i=CA_GetNamedNum("gunknife");
    weaponlump=i;
    numlumps=4;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    weaponpic[3]=CA_CacheLump(i+3);
    if (netmode) NetGetData();
    break;
   case 9:
    i=CA_GetNamedNum("guncross");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 10:
    i=CA_GetNamedNum("gunspec7");
    weaponlump=i;
    numlumps=4;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    weaponpic[3]=CA_CacheLump(i+3);
    if (netmode) NetGetData();
    break;
   case 11:
    i=CA_GetNamedNum("gunmoo");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 12:
    i=CA_GetNamedNum("gunprong");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 13:
    i=CA_GetNamedNum("catlprod");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 14:
    i=CA_GetNamedNum("s7weapon");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 15:
    i=CA_GetNamedNum("domknife");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 16:
    i=CA_GetNamedNum("redgun");
    weaponlump=i;
    numlumps=2;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    break;
   case 17:
    i=CA_GetNamedNum("bluegun");
    weaponlump=i;
    numlumps=3;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    break;
   case 18:
    i=CA_GetNamedNum("greengun");
    weaponlump=i;
    numlumps=5;
    weaponpic[0]=CA_CacheLump(i);
    if (netmode) NetGetData();
    weaponpic[1]=CA_CacheLump(i+1);
    if (netmode) NetGetData();
    weaponpic[2]=CA_CacheLump(i+2);
    if (netmode) NetGetData();
    weaponpic[3]=CA_CacheLump(i+3);
    if (netmode) NetGetData();
    weaponpic[4]=CA_CacheLump(i+4);
    if (netmode) NetGetData();
    break;
   }
 }


void ResetScalePostWidth (int NewWindowWidth)
/* this must be updated if the scalepost or scalemaskedpost are changed
   the increment is size of each replicated asm block
   the offset is the location of the line to draw the pixel

   *note: runtime change of code!! */
{
 }


void ChangeViewSize(byte MakeLarger)
{
 int lastviewsize;

 /* In HD the view size is the render resolution, chosen by VI_ApplyRenderMode
    from the display, and the viewSizes[] table does not apply.  newplayer walks
    this function up and down four times to force the tables to rebuild, which
    would otherwise drop the view straight back to 320x200. */
 if (hdmode) return;

 if (SC.vrhelmet==1)
  {
   if (MakeLarger && viewSizes[(currentViewSize+1)*2]!=320)
    return;
   else if (!MakeLarger && viewSizes[(currentViewSize-1)*2]!=320)
    return;
   }
 lastviewsize=currentViewSize;
 resizeScreen=0;
 if (MakeLarger)
  {
   if (currentViewSize<MAXVIEWSIZE-1) currentViewSize++;
    else return;
   }
 else
  {
   if (currentViewSize>0) currentViewSize--;
    else return;
   }
 if (viewSizes[currentViewSize*2]!=viewSizes[lastviewsize*2]
  || viewSizes[currentViewSize*2+1]!=viewSizes[lastviewsize*2+1]
  )
  {
   windowWidth=viewSizes[currentViewSize*2];
   windowHeight=viewSizes[currentViewSize*2+1];
   windowLeft=viewLoc[currentViewSize*2];
   windowTop=viewLoc[currentViewSize*2+1];
   windowSize=windowHeight*windowWidth;
   viewLocation=(intptr_t)screen+windowTop*320+windowLeft;
   mainViewWidth=windowWidth;
   mainViewHeight=windowHeight;
   SetViewSize(windowWidth,windowHeight);
   ResetScalePostWidth(windowWidth);
   InitWalls();
   }
 resetdisplay();
 if (currentViewSize>=5)
  {
   memset(screen,0,64000);
   VI_DrawPic(4,149,statusbar[2]);
   }
 if (currentViewSize>=4) VI_DrawMaskedPic(0,0,statusbar[3]);
 player.scrollmin=scrollmin;
 player.scrollmax=scrollmax;
 }


void SaveGame(int n)
{
 scaleobj_t  *sprite_p;
 FILE        *f;
 char        fname[20];
 doorobj_t   *door_p, *last_p;
 int         i, mapspot;
 spawnarea_t *sa;
 elevobj_t   *elev_p;

 StartWait();
 memset(player.savesprites,0,sizeof(player.savesprites));
 memcpy(player.westwall,westwall,sizeof(westwall));
 memcpy(player.northwall,northwall,sizeof(northwall));

 UpdateWait();
 /* sprites */
 for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
  {
   mapspot=(sprite_p->y>>FRACTILESHIFT)*MAPCOLS+(sprite_p->x>>FRACTILESHIFT);
   switch (sprite_p->type)
    {
     case S_MONSTER1:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER1_NS;
	 else player.savesprites[mapspot]=S_MONSTER1;
	}
       else player.savesprites[mapspot]=S_DEADMONSTER1;
      break;
     case S_MONSTER2:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER2_NS;
	 else player.savesprites[mapspot]=S_MONSTER2;
	}
      break;
     case S_MONSTER3:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER3_NS;
	 else player.savesprites[mapspot]=S_MONSTER3;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER3;
      break;
     case S_MONSTER4:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER4_NS;
	 else player.savesprites[mapspot]=S_MONSTER4;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER4;
      break;
     case S_MONSTER5:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints) player.savesprites[mapspot]=S_MONSTER5;
       else player.savesprites[mapspot]=S_DEADMONSTER5;
      break;
     case S_MONSTER6:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER6_NS;
	 else player.savesprites[mapspot]=S_MONSTER6;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER6;
      break;
     case S_MONSTER7:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER7_NS;
	 else player.savesprites[mapspot]=S_MONSTER7;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER7;
      break;
     case S_MONSTER8:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER8_NS;
	 else player.savesprites[mapspot]=S_MONSTER8;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER8;
      break;
     case S_MONSTER9:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER9_NS;
	 else player.savesprites[mapspot]=S_MONSTER9;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER9;
      break;
     case S_MONSTER10:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER10_NS;
	 else player.savesprites[mapspot]=S_MONSTER10;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER10;
      break;
     case S_MONSTER11:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER11_NS;
	 else player.savesprites[mapspot]=S_MONSTER11;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER11;
      break;
     case S_MONSTER12:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER12_NS;
	 else player.savesprites[mapspot]=S_MONSTER12;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER12;
      break;
     case S_MONSTER13:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER13_NS;
	 else player.savesprites[mapspot]=S_MONSTER13;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER13;
      break;
     case S_MONSTER14:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER14_NS;
	 else player.savesprites[mapspot]=S_MONSTER14;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER14;
      break;
     case S_MONSTER15:
      if (sprite_p->deathevent)
       break;
      if (sprite_p->hitpoints)
       {
	if (sprite_p->nofalling) player.savesprites[mapspot]=S_MONSTER15_NS;
	 else player.savesprites[mapspot]=S_MONSTER15;
	}
      else player.savesprites[mapspot]=S_DEADMONSTER15;
      break;
     case S_DEADMONSTER1:
     case S_DEADMONSTER2:
     case S_DEADMONSTER3:
     case S_DEADMONSTER4:
     case S_DEADMONSTER5:
     case S_DEADMONSTER6:
     case S_DEADMONSTER7:
     case S_DEADMONSTER8:
     case S_DEADMONSTER9:
     case S_DEADMONSTER10:
     case S_DEADMONSTER11:
     case S_DEADMONSTER12:
     case S_DEADMONSTER13:
     case S_DEADMONSTER14:
     case S_DEADMONSTER15:
     case S_AMMOBOX:
     case S_MEDBOX:
     case S_GOODIEBOX:
     case S_PROXMINE:
     case S_TIMEMINE:
     case S_PRIMARY1:
     case S_PRIMARY2:
     case S_SECONDARY1:
     case S_SECONDARY2:
     case S_SECONDARY3:
     case S_SECONDARY4:
     case S_SECONDARY5:
     case S_SECONDARY6:
     case S_SECONDARY7:
     case S_WEAPON0:
     case S_WEAPON1:
     case S_WEAPON2:
     case S_WEAPON3:
     case S_WEAPON4:
     case S_WEAPON5:
     case S_WEAPON6:
     case S_WEAPON7:
     case S_WEAPON8:
     case S_WEAPON9:
     case S_WEAPON10:
     case S_WEAPON11:
     case S_WEAPON12:
     case S_WEAPON13:
     case S_WEAPON14:
     case S_WEAPON15:
     case S_WEAPON16:
     case S_WEAPON17:
     case S_WEAPON18:
     case S_ITEM1:
     case S_ITEM2:
     case S_ITEM3:
     case S_ITEM4:
     case S_ITEM5:
     case S_ITEM6:
     case S_ITEM7:
     case S_ITEM8:
     case S_ITEM9:
     case S_ITEM10:
     case S_ITEM11:
     case S_ITEM12:
     case S_ITEM13:
     case S_ITEM14:
     case S_ITEM15:
     case S_ITEM16:
     case S_ITEM17:
     case S_ITEM18:
     case S_ITEM19:
     case S_ITEM20:
     case S_ITEM21:
     case S_ITEM22:
     case S_ITEM23:
     case S_ITEM24:
     case S_ITEM25:
      player.savesprites[mapspot]=sprite_p->type;
      break;
     }
   }
 UpdateWait();

 /* map triggers */
 for(i=0;i<MAPCOLS*MAPROWS;i++)  // remember warps
  switch (mapsprites[i])
   {
    case SM_WARP1:
    case SM_WARP2:
    case SM_WARP3:
     player.savesprites[i]=mapsprites[i];
     break;
    case SM_SWITCHDOWN:
     player.savesprites[i]=S_TRIGGER1;
     break;
    case SM_SWITCHDOWN2:
     player.savesprites[i]=S_TRIGGER2;
     break;
    case SM_SWAPSWITCH:
     player.savesprites[i]=S_SWAPSWITCH;
     break;
    case SM_STRIGGER:
     player.savesprites[i]=S_STRIGGER;
     break;
    case SM_EXIT:
     player.savesprites[i]=S_EXIT;
     break;
//    case SM_HOLE:
//     player.savesprites[i]=S_HOLE;
//     break;
    }
 UpdateWait();

 /* doors */
 last_p=&doorlist[numdoors];
 for (door_p=doorlist;door_p!=last_p;door_p++)
  if (door_p->pic==CA_GetNamedNum("door_1")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR1;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR1;
    }
  else if (door_p->pic==CA_GetNamedNum("door_2")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR2;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR2;
    }
  else if (door_p->pic==CA_GetNamedNum("door_3")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR3;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR3;
    }
  else if (door_p->pic==CA_GetNamedNum("door_4")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR4;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR4;
    }
  else if (door_p->pic==CA_GetNamedNum("door_5")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR5;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR5;
    }
  else if (door_p->pic==CA_GetNamedNum("door_6")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR6;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR6;
    }
  else if (door_p->pic==CA_GetNamedNum("door_7")-walllump)
   {
    if (door_p->orientation==dr_vertical || door_p->orientation==dr_vertical2)
     player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_VDOOR7;
    else player.savesprites[door_p->tiley*MAPCOLS+door_p->tilex]=S_HDOOR7;
    }
 UpdateWait();

 /* spawning areas / generators */
 sa=spawnareas;
 for(i=0;i<numspawnareas;i++,sa++)
  switch (sa->type)
   {
    case 0:
     player.savesprites[sa->mapspot]=S_GENERATOR1;
     break;
    case 1:
     player.savesprites[sa->mapspot]=S_GENERATOR2;
     break;
    case 10:
     player.savesprites[sa->mapspot]=S_SPAWN1;
     break;
    case 11:
     player.savesprites[sa->mapspot]=S_SPAWN2;
     break;
    case 12:
     player.savesprites[sa->mapspot]=S_SPAWN3;
     break;
    case 13:
     player.savesprites[sa->mapspot]=S_SPAWN4;
     break;
    case 14:
     player.savesprites[sa->mapspot]=S_SPAWN5;
     break;
    case 15:
     player.savesprites[sa->mapspot]=S_SPAWN6;
     break;
    case 16:
     player.savesprites[sa->mapspot]=S_SPAWN7;
     break;
    case 17:
     player.savesprites[sa->mapspot]=S_SPAWN8;
     break;
    case 18:
     player.savesprites[sa->mapspot]=S_SPAWN9;
     break;
    case 19:
     player.savesprites[sa->mapspot]=S_SPAWN10;
     break;
    case 20:
     player.savesprites[sa->mapspot]=S_SPAWN11;
     break;
    case 21:
     player.savesprites[sa->mapspot]=S_SPAWN12;
     break;
    case 22:
     player.savesprites[sa->mapspot]=S_SPAWN13;
     break;
    case 23:
     player.savesprites[sa->mapspot]=S_SPAWN14;
     break;
    case 24:
     player.savesprites[sa->mapspot]=S_SPAWN15;
     break;
    case 100:
     player.savesprites[sa->mapspot]=S_SPAWN8_NS;
     break;
    case 101:
     player.savesprites[sa->mapspot]=S_SPAWN9_NS;
     break;
    }
 UpdateWait();

 /* elevators */
 for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
  switch (elev_p->type)
   {
    case E_NORMAL:
     if (!elev_p->nosave)
      {
       if (elev_p->elevTimer==0x70000000)
	player.savesprites[elev_p->mapspot]=S_PAUSEDELEVATOR;
       else
	player.savesprites[elev_p->mapspot]=S_ELEVATOR;
       }
     break;
    case E_TIMED:
     switch (elev_p->elevTimer)
      {
       case 12600:
	player.savesprites[elev_p->mapspot]=S_ELEVATOR3M;
	break;
       case 25200:
	player.savesprites[elev_p->mapspot]=S_ELEVATOR6M;
	break;
       case 63000:
	player.savesprites[elev_p->mapspot]=S_ELEVATOR15M;
	break;
       }
     break;
    case E_SWITCHDOWN:
     player.savesprites[elev_p->mapspot]=S_TRIGGERD1;
     break;
    case E_SWITCHDOWN2:
     player.savesprites[elev_p->mapspot]=S_TRIGGERD2;
     break;
    case E_SECRET:
     player.savesprites[elev_p->mapspot]=S_SDOOR;
     break;
    case E_SWAP:
     if ((elev_p->position==elev_p->floor && !elev_p->elevUp) || elev_p->elevDown) player.savesprites[elev_p->mapspot]=S_ELEVATORLOW;
      else if ((elev_p->position==elev_p->ceiling && !elev_p->elevDown) || elev_p->elevUp) player.savesprites[elev_p->mapspot]=S_ELEVATORHIGH;
     break;
    }

 UpdateWait();

 sprintf(fname,SAVENAME,n);
 f=fopen(fname,"w+b");
 if (f==NULL) MS_Error("SaveGame: File Open Error: %s",fname);
 UpdateWait();
 if (!fwrite(&player,sizeof(player),1,f)) MS_Error("SaveGame: File Write Error:%s",fname);
 UpdateWait();
 fclose(f);
 EndWait();
 }


void resetengine(void)
{
 turnrate=0;
 moverate=0;
 fallrate=0;
 strafrate=0;
 exitexists=false;
 BonusItem.time=2100;
 BonusItem.score=0;
 timecount=0;
 frames=0;
 player.timecount=0;
 weapdelay=0;
 secretdelay=0;
 frames=0;
 keyboardDelay=0;
 spritemovetime=0;
 wallanimationtime=0;
 msgtime=0;
 RearViewTime=0;
 RearViewDelay=0;
 netsendtime=0;
 SwitchTime=0;
 inventorytime=0;
 nethurtsoundtime=0;
 midgetmode=0;
 fxtimecount=0;
 ResetMouse();
 }


void selectsong(int songmap)
{
 char fname[20];
 int  pattern;

#ifdef DEMO
 songmap%=5;
#endif
 switch (songmap)
  {
   case 0:
    pattern=0;
    strcpy(fname,"SONG0.S3M");
    break;
   case 1:
    pattern=20;
    strcpy(fname,"SONG0.S3M");
    break;
   case 2:
    pattern=37;
    strcpy(fname,"SONG0.S3M");
    break;
   case 3:
    pattern=54;
    strcpy(fname,"SONG0.S3M");
    break;
   case 4:
    pattern=73;
    strcpy(fname,"SONG0.S3M");
    break;

   case 5:
    pattern=0;
    strcpy(fname,"SONG2.S3M");
    break;
   case 6:
    pattern=26;
    strcpy(fname,"SONG2.S3M");
    break;
   case 7:
    pattern=46;
    strcpy(fname,"SONG2.S3M");
    break;
   case 8:
    pattern=64;
    strcpy(fname,"SONG2.S3M");
    break;
   case 9:
    pattern=83;
    strcpy(fname,"SONG2.S3M");
    break;

   case 10:
    pattern=0;
    strcpy(fname,"SONG3.S3M");
    break;
   case 11:
    pattern=39;
    strcpy(fname,"SONG3.S3M");
    break;
   case 12:
    pattern=58;
    strcpy(fname,"SONG3.S3M");
    break;
   case 13:
    pattern=78;
    strcpy(fname,"SONG3.S3M");
    break;
   case 14:
    pattern=94;
    strcpy(fname,"SONG3.S3M");
    break;

   case 15:
    pattern=0;
    strcpy(fname,"SONG1.S3M");
    break;
   case 16:
    pattern=24;
    strcpy(fname,"SONG1.S3M");
    break;
   case 17:
    pattern=45;
    strcpy(fname,"SONG1.S3M");
    break;

   case 18:
    pattern=0;
    strcpy(fname,"SONG4.S3M");
    break;
   case 19:
    pattern=10;
    strcpy(fname,"SONG4.S3M");
    break;
   case 20:
    pattern=21;
    strcpy(fname,"SONG4.S3M");
    break;
   case 21:
    pattern=0;
    strcpy(fname,"SONG8.MOD");
    break;

   case 22:
    if (netmode)
     {
      pattern=0;
      strcpy(fname,"SONG14.MOD");
      }
    else
     {
      pattern=0;
      strcpy(fname,"ENDING.MOD");
      }
    break;

   case 23:
    pattern=0;
    strcpy(fname,"SONG5.MOD");
    break;
   case 24:
    pattern=0;
    strcpy(fname,"SONG6.MOD");
    break;
   case 25:
    pattern=0;
    strcpy(fname,"SONG7.MOD");
    break;
   case 26:
    pattern=33;
    strcpy(fname,"SONG4.S3M");
    break;
   case 27:
    pattern=0;
    strcpy(fname,"SONG9.MOD");
    break;
   case 28:
    pattern=0;
    strcpy(fname,"SONG10.MOD");
    break;
   case 29:
    pattern=0;
    strcpy(fname,"SONG11.MOD");
    break;
   case 30:
    pattern=0;
    strcpy(fname,"SONG12.MOD");
    break;
   case 31:
    pattern=0;
    strcpy(fname,"SONG13.MOD");
    break;

   case 99:
    pattern=0;
    strcpy(fname,"PROBE.MOD");
    break;

   default:
    pattern=0;
    strcpy(fname,"SONG0.S3M");
    break;

   }
 PlaySong(fname,pattern);
 }


static void BriefingWaitKey(void)
/* One press, one page.  Used by the mission briefings and the end-game text
   and credits screens, which all wait for the player the same way.

   Every one of those pages used to end in `for(;;){Wait(10); if (newascii)
   break;}`.  Nothing there consumes the key, so the auto-repeat kept re-latching
   newascii and a single held key ran the pages together -- measurably, one press
   spent all six pages of the map 0 briefing.  The briefings had it worse than
   the end-game screens, because they clear newascii *before* the page fades in
   rather than after, so the repeat had the whole fade to latch in as well.

   Same debounce as MenuCommand's Enter and Esc: nothing counts until the keys
   that can produce a character have been seen released.  Modifiers are skipped
   because they have no ASCIINames entry and so cannot set newascii; waiting on
   them would hang the screen for anyone resting a finger on shift.

   Esc is exempt.  MissionBriefing follows this with `if (lastascii==27) goto
   end`, so debouncing Esc would cost a second press to leave a briefing the
   player has already said they want out of -- and it cannot run pages together,
   because it leaves MissionBriefing outright.  Same `newascii && lastascii==27`
   test the engine's other skippable screens use.  The end-game callers clear
   newascii immediately before calling, so a stale Esc cannot reach this.

   quitgame likewise returns straight away, and every caller tests it after.
   This is one of the few loops in the engine that waits on nothing but input,
   so it cannot be left to Wait's own quitgame check -- with the delay abandoned
   it would simply spin here forever instead. */
{
 int i;

 if (quitgame || (newascii && lastascii==27)) return;

 for (;;)
  {
   for (i=0;i<NUMCODES;i++)
    if (keyboard[i] && ASCIINames[i]) break;
   if (i==NUMCODES) break;
   if (quitgame || (newascii && lastascii==27)) return;
   Wait(1);
   }

 newascii=false;
 while (!newascii && !quitgame) Wait(1);
 }


void EndGame1(void)
{
 char name[64];

 selectsong(22);

#ifdef CDROMGREEDDIR
 sprintf(name,"%c:\\GREED\\MOVIES\\PRISON1.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\TEMPLE1.FLI",cdr_drivenum+'A');
 playfli(name,0);
#else
 sprintf(name,"%c:\\MOVIES\\PRISON1.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\TEMPLE1.FLI",cdr_drivenum+'A');
 playfli(name,0);
#endif

 VI_FillPalette(0,0,0);

 loadscreen("REDCHARS");
 VI_FadeIn(0,256,colors,48);
 Wait(140);
 for(fontbasecolor=64;fontbasecolor<73;++fontbasecolor)
  {
   printy=80;
   FN_PrintCentered(
    "BY SUCCESSFULLY BRAVING THE DESARIAN\n"
    "PENAL COLONY YOU EMERGE VICTORIOUS\n"
    "WITH THE BRASS RING OF BYZANT IN HAND.\n"
    "...BUT IT'S NOT OVER YET, HUNTER.\n"
    "IT'S ON TO PHASE TWO OF THE HUNT, THE\n"
    "CITY TEMPLE OF RISTANAK.  ARE YOU\n"
    "PREPARED TO FACE THE Y'RKTARELIAN\n"
    "PRIESTHOOD AND THEIR PAGAN GOD?\n"
    "NOT BLOODY LIKELY...\n"
    "\n\n\n\n\nTO BE CONTINUED...\n");
   }
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("SOFTLOGO");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS1");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS2");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

#ifndef ASSASSINATOR
 loadscreen("CREDITS3");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);
#endif

 redo=true;
 }


void EndGame2(void)
{
 char name[64];

 selectsong(22);

#ifdef CDROMGREEDDIR
 sprintf(name,"%c:\\GREED\\MOVIES\\TEMPLE2.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS1.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS2.FLI",cdr_drivenum+'A');
 playfli(name,0);
#else
 sprintf(name,"%c:\\MOVIES\\TEMPLE2.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS1.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS2.FLI",cdr_drivenum+'A');
 playfli(name,0);
#endif


 VI_FillPalette(0,0,0);

 loadscreen("REDCHARS");
 VI_FadeIn(0,256,colors,48);
 Wait(140);
 for(fontbasecolor=64;fontbasecolor<73;++fontbasecolor)
  {
   printy=80;
   FN_PrintCentered(
    "WITH Y'RKTAREL DEAD AND THE PRIESTHOOD\n"
    "IN RUINS CONGRATULATE YOURSELF, HUNTER.\n"
    "YOU'VE ANNHILIATED YET ANOTHER CULTURE\n"
    "ALL FOR THE SAKE OF THE HUNT.\n"
    "...BUT DON'T RELAX YET, FOR IT'S ON TO\n"
    "PHASE THREE OF THE HUNT.  THIS TIME\n"
    "YOU'LL BATTLE AN ENTIRE ARMY AS YOU FACE\n"
    "OFF WITH LORD KAAL IN HIS SPACEBORN\n"
    "MOUNTAIN CITADEL.\n"
    "DO YOU HAVE WHAT IT TAKES TO SLAY LORD\n"
    "KAAL AND WREST FROM HIM THE IMPERIAL SIGIL?\n"
    "\n\n\n\n\nTO BE CONTINUED...\n");
   }
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("SOFTLOGO");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS1");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS2");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS3");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 redo=true;
 }


void EndGame3(void)
{
 char name[64];

#ifdef CDROMGREEDDIR
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS3.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS4.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS5.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS6.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBS6B.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS7.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS8.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS9.FLI",cdr_drivenum+'A');
 playfli(name,0);
#else
 sprintf(name,"%c:\\MOVIES\\JUMPBAS3.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS4.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS5.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS6.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBS6B.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS7.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS8.FLI",cdr_drivenum+'A');
 playfli(name,0);
 sprintf(name,"%c:\\MOVIES\\JUMPBAS9.FLI",cdr_drivenum+'A');
 playfli(name,0);
#endif

 VI_FillPalette(0,0,0);

 loadscreen("REDCHARS");
 VI_FadeIn(0,256,colors,48);
 Wait(140);
 for(fontbasecolor=64;fontbasecolor<73;++fontbasecolor)
  {
   printy=80;
#ifdef GAME3
   FN_PrintCentered(
    "WELL, YOU SUCCESSFULLY PULLED DOWN THE LAST\n"
    "VESTIGES OF MILITARY AUTHORITY FOR THE SECTOR.\n"
    "YOU COULD HAVE RICHES, FAME AND POWER,\n"
    "AND YOUR CHOICE OF PLEASURE PLANETS.\n"
    "UNFORTUNATELY, YOU'RE STUCK ON A SHIP THAT'S\n"
    "DRIFTING THROUGH HYPERSPACE.  IN SHORT\n"
    "YOU'RE LOST.  LUCKY FOR THE PASSENGERS\n"
    "THAT YOU'RE A HEROIC HUNTER THAT CAN SAVE\n"
    "THEM FROM THEIR FATE IN THE CLUTCHES\n"
    "OF THE MAZDEEN EMPEROR.  OR CAN YOU?\n"
    "\n\n\n\n\nTO BE CONTINUED...\n");
#else
   FN_PrintCentered(
    "WELL, YOU SUCCESSFULLY BRAVED A BLOODY RIOT, FACED\n"
    "A GOD AND SURVIVED, AND PULLED DOWN THE LAST\n"
    "VESTIGES OF MILITARY AUTHORITY FOR THE SECTOR.\n"
    "YOU COULD HAVE RICHES, FAME AND POWER,\n"
    "AND YOUR CHOICE OF PLEASURE PLANETS.\n"
    "UNFORTUNATELY, YOU'RE STUCK ON A SHIP THAT'S\n"
    "DRIFTING THROUGH HYPERSPACE.  IN SHORT\n"
    "YOU'RE LOST.  LUCKY FOR THE PASSENGERS\n"
    "THAT YOU'RE A HEROIC HUNTER THAT CAN SAVE\n"
    "THEM FROM THEIR FATE IN THE CLUTCHES\n"
    "OF THE MAZDEEN EMPEROR.  OR CAN YOU?\n"
    "\n\n\n\n\nTO BE CONTINUED...\n");
#endif
   }
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("SOFTLOGO");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS1");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS2");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 loadscreen("CREDITS3");
 VI_FadeIn(0,256,colors,48);
 newascii=false;
 BriefingWaitKey();
 if (quitgame) return;
 VI_FadeOut(0,256,0,0,0,48);
 memset(screen,0,64000);

 redo=true;
 }


void newmap(int map,int activate)
{
 int lump, i, n, songmap;

 if (activate)
  {
   memset(player.westmap,0,sizeof(player.westmap));
   memset(player.northmap,0,sizeof(player.northmap));
   memset(player.events,0,sizeof(player.events));
   player.x=-1;
   }
 player.map=map;
 songmap=map;
 if ((map!=8 && map!=16) || netmode)
  selectsong(songmap);
 else
  StopMusic();
 if (activate==1)
  MissionBriefing(map);
 resetengine();
 lump=CA_GetNamedNum("map") + map + 1;
#ifdef DEMO
 if (map==3 && !netmode) ;
 else
  {
   LoadNewMap(lump);
   if (activate)
    {
     LoadScript(lump,true);
     ActivateSpritesFromMap();
     }
   else
    LoadScript(lump,false);
   }
#else

#ifdef GAME1
 if (map==8 && !netmode)
  EndGame1();
#elif defined(GAME2)
 if (map==16 && !netmode)
  EndGame2();
#else
 if (map==22 && !netmode)
  EndGame3();
#endif

 else
  {
   LoadNewMap(lump);
   if (activate)
    {
     LoadScript(lump,true);
     ActivateSpritesFromMap();
     }
   else
    LoadScript(lump,false);
   }
#endif
 EndWait();
 for(i=0;i<5;i++)
  if (player.weapons[i]!=-1)
   {
    n=player.weapons[i];
    weapons[n].charge=100;
    weapons[n].chargetime=timecount+weapons[n].chargerate;
    }
 }


void LoadGame(int n)
{
 char fname[20];
 int  handle, i, oldscore;

 sprintf(fname,SAVENAME,n);
 if ((handle=open(fname,O_RDONLY | O_BINARY))==-1) return;
 if (!read(handle,&player,sizeof(player)))
  {
   close(handle);
   MS_Error("LoadGame: Error loading %s!",fname);
   }
 close(handle);
 oldscore=player.levelscore;

 resetengine();
 gameloaded=true;
 player.scrollmax=windowHeight+player.scrollmin;
 timecount=player.timecount;
 keyboardDelay=0;
 BonusItem.time=timecount+2100;
 wallanimationtime=player.timecount;
 spritemovetime=player.timecount;

 newmap(player.map,0);
 memcpy(mapsprites,player.savesprites,sizeof(mapsprites));
 ActivateSpritesFromMap();
 timecount=player.timecount;
 loadweapon(player.weapons[player.currentweapon]);
 player.levelscore=oldscore;
 memcpy(westwall,player.westwall,sizeof(westwall));
 memcpy(northwall,player.northwall,sizeof(northwall));
 eventloading=true;
 for (i=1;i<256;i++)
  if (player.events[i])
   Event(i,true);
 eventloading=false;
 }


void heal(int n)
{
 player.shield+=n;
 if (player.shield>player.maxshield) player.shield=player.maxshield;
 hurtborder=true;
 }


void medpaks(int n)
{
 if (player.angst<=0) return;
 player.angst+=n;
 if (player.angst>player.maxangst) player.angst=player.maxangst;
 hurtborder=true;
 }


void hurt(int n)
{
 if (godmode || player.angst==0) return;

 if (specialeffect==SE_INVISIBILITY)
  n/=3;

 if (specialeffect==SE_REVERSOPILL)
  {
   medpaks(n/2);
   heal(n/2);
   return;
   }
 player.status=1;
 if (n>player.shield)
  {
   n-=player.shield;
   player.shield=0;
   player.angst-=n;
   if (player.angst<0) player.angst=0;
   }
 else player.shield-=n;
 hurtborder=true;
 if (player.angst==0)
  {
   SoundEffect(SN_DEATH0+player.chartype,15,player.x,player.y);
   if (netmode) NetSoundEffect(SN_DEATH0+player.chartype,15,player.x,player.y);
   SoundEffect(SN_DEATH0+player.chartype,15,player.x,player.y);
   if (netmode) NetSoundEffect(SN_DEATH0+player.chartype,15,player.x,player.y);
   }
 else
  {
   SoundEffect(SN_HIT0+player.chartype,15,player.x,player.y);
   if (netmode && timecount>nethurtsoundtime)
    {
     NetSoundEffect(SN_HIT0+player.chartype,15,player.x,player.y);
     nethurtsoundtime=timecount+35;
     }
   }
 }


void newplayer(int map,int chartype,int difficulty)
{
 int parm;

 parm=MS_CheckParm("char");
 if (parm && parm<my_argc-1)
  {
   chartype=atoi(my_argv[parm+1]);
   if (chartype<0 || chartype>=MAXCHARTYPES)
    MS_Error("Invalid Character Selection (%i)",chartype);
   }

 gameloaded=true;
 memset(&player,0,sizeof(player));
 player.scrollmin=0;
 player.scrollmax=windowHeight;
 player.x=-1;
 player.map=map;
 player.height=pheights[chartype];
 player.maxangst=pmaxangst[chartype];
 player.maxshield=pmaxshield[chartype];
 player.walkmod=pwalkmod[chartype];
 player.runmod=prunmod[chartype];
 player.jumpmod=pjumpmod[chartype];
 player.shield=player.maxshield;
 player.angst=player.maxangst;
 player.levelscore=levelscore;
 player.chartype=chartype;
 player.difficulty=difficulty;
 resetengine();
 switch (chartype)
  {
   case 0: // psyborg
    player.weapons[0]=7;
    player.weapons[1]=1;
    break;
   case 1: // lizard
    player.weapons[0]=8;
    player.weapons[1]=9;
    break;
   case 2: // mooman
    player.weapons[0]=13;
    player.weapons[1]=11;
    break;
   case 3: // specimen 7
    player.weapons[0]=14;
    player.weapons[1]=10;
    break;
   case 4: // trix
    player.weapons[0]=15;
    player.weapons[1]=12;
    break;
   case 5:
    player.weapons[0]=8;
    player.weapons[1]=9;
   }
 player.weapons[2]=-1;
 player.weapons[3]=-1;
 player.weapons[4]=-1;
 player.ammo[0]=100;
 player.ammo[1]=100;
 player.ammo[2]=100;
 player.inventory[7]=2;
 player.inventory[5]=2;
 player.inventory[4]=2;
 player.inventory[2]=4;
 newmap(player.map,1);
 timecount=0;
 loadweapon(player.weapons[0]);
 }


void addscore(int n)
{
 player.score+=n;
 if (player.score>4000000000) player.score=0;
 player.levelscore-=n;
 if (player.levelscore<0) player.levelscore=0;
 }


void ControlMovement(void);


void respawnplayer(void)
{
 int mapspot;
 int x, y, n;

 do
  {
   n=(clock()+MS_RndT())%MAXSTARTLOCATIONS;
   x=startlocations[n][0];
   y=startlocations[n][1];
   mapspot=y*MAPCOLS+x;
   } while (mapsprites[mapspot]>0);
 player.x=(x<<FRACTILESHIFT) + (32<<FRACBITS);
 player.y=(y<<FRACTILESHIFT) + (32<<FRACBITS);
 player.z=RF_GetFloorZ(player.x,player.y)+player.height;
 player.angle=NORTH;
 NetNewPlayerData();
 }


void PlayerCommand(void);


static void BriefingFadeStep(byte *basep,int i,int steps)
/* One step of a basep->colors palette ramp.  Same interpolation VI_FadeIn
   does, as plain integer arithmetic rather than its signed-char error
   accumulator.  VI_SetPalette presents as a side effect. */
{
 byte work[768];
 int  j;

 for (j=0;j<768;j++)
  work[j]=(byte)((int)basep[j]+(((int)colors[j]-(int)basep[j])*i)/steps);
 VI_SetPalette(work);
 }


static void BriefingFadeInPage(int texty,char *text)
/* Bring a briefing page up with its caption arriving at the same time.

   The original played this as three separate beats: fade the picture up over
   64 ticks, hold for two seconds, then ramp the caption in through
   fontbasecolor 0..8.  Here the caption ramp is spread across the same
   interval as the palette fade, so the words appear with the picture.

   Redrawing the text every step is safe: FN_RawPrint3 writes fontbasecolor+b
   straight into the pixel rather than blending, so each pass simply restates
   the same glyphs a shade brighter.

   The palette interpolation is the same start->target ramp VI_FadeIn does,
   written as plain integer arithmetic rather than its signed-char error
   accumulator. */
{
 byte basep[768];
 int  i, steps=64;

 VI_GetPalette(basep);
 for (i=1;i<=steps;i++)
  {
   /* A key pressed during the ramp snaps it to the end.  Jumping i rather than
      breaking means the last step still runs, so the page is left in exactly
      the state a completed fade leaves it in.  The press is not spent here --
      BriefingWaitKey still wants one of its own to turn the page. */
   if (newascii) i=steps;
   fontbasecolor=(i*9)/steps;
   if (fontbasecolor>8) fontbasecolor=8;
   printy=texty;
   FN_PrintCentered(text);
   BriefingFadeStep(basep,i,steps);
   Wait(1);
   }
 VI_SetPalette(colors);
 }


void MissionBriefing(int map)
{
 int pprimaries, psecondaries, i, tprimaries, tsecondaries, oldtimecount;
 char str[255], name[64];
 byte *scr;
 byte basep[768];

 if ( netmode || nointro ) 
	 return;

 scr=(byte *)malloc(64000);
 if (scr==NULL)
  MS_Error("Error allocating MissonBriefing buffer");
 memcpy(scr,viewbuffer,64000);

 oldtimecount=timecount;

 INT_TimerHook(NULL);
 font=font1;

 if (map==0)
  {
   VI_FillPalette(0,0,0);

	loadscreen("BRIEF3");
	VI_BlitView();
   newascii=false;
   BriefingFadeInPage(149,
      "WELCOME ABOARD HUNTER.\n"
      "DUE TO INCREASED FUNDING FROM THE AVC YOU'LL BE EQUIPPED WITH THE\n"
      "LATEST IN HUNTER HARDWARE.  ALONG WITH YOUR EXISTING AUTO MAPPER,\n"
      "HEAT AND MOTION SENSORS HAVE BEEN ADDED TO YOUR VISUAL ARRAY AS\n"
      "WELL AS AN AFT SENSORY SYSTEM, OR A.S.S. CAM, FOR CONTINUOUS\n"
      "REAR VIEW.");

   /* The Win32 port had a palette-ramp debug fill here -- 200 rows, row i
      painted with colour index i -- which is not in the DOS source and wiped
      the briefing screen the player is meant to be reading. */

   BriefingWaitKey();
   if (quitgame || lastascii==27) goto end;

   loadscreen("BRIEF3");
   newascii=false;
   for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
    {
     if (newascii) fontbasecolor=8;   /* snap the ramp -- see BriefingFadeInPage */
     printy=149;
     FN_PrintCentered(
      "A MENUING SYSTEM HAS ALSO BEEN INSTALLED ALLOWING YOU TO\n"
      "FINE TUNE YOUR HARDWARE SETTINGS.  STAY ALERT THOUGH, YOUR MENU\n"
      "OVERLAY CANCELS INPUT FROM YOUR VISUAL ARRAY SO DON'T EXPECT TO\n"
      "SEE THINGS COMING WHILE YOU'RE ADJUSTING YOUR SETTINGS.");
     Wait(3);
     }
   BriefingWaitKey();
   if (quitgame || lastascii==27) goto end;
   VI_FadeOut(0,256,0,0,0,64);


   loadscreen("BRIEF1");
   newascii=false;
   BriefingFadeInPage(139,
      "BUILT FROM A HOLLOWED ASTEROID, THE DESARIAN PENAL COLONY\n"
      "HOUSES THE DREGS OF IMPERIAL SOCIETY.  A RIOT IS IN PROGRESS\n"
      "WHICH SHOULD MAKE ITEM RETRIEVAL INTERESTING.\n"
      "THE PRIMARY ITEM TO BE LOCATED HERE IS THE BYZANTIUM BRASS RING,\n"
      "AN ANCIENT ARTIFACT NOW USED AS THE POWER CORE FOR THE COMPLEX.\n"
      "SUCH AN ENIGMATIC ENERGY SOURCE IS OF OBVIOUS INTEREST TO A.V.C.\n"
      "RESEARCH, SO ACQUIRING IT UNDAMAGED IS ESSENTIAL.\n"
      "YOUR ENTRY POINT WILL BE AT THE BASE OF THE COMPLEX.\n");
   BriefingWaitKey();
   if (quitgame || lastascii==27) goto end;
   VI_FadeOut(0,256,0,0,0,64);

   loadscreen("BRIEF2");
   newascii=false;
   BriefingFadeInPage(139,
      "EACH SUBLEVEL WILL HAVE A MANDATORY PRIMARY OBJECTIVE, AS WELL\n"
      "AS OPTIONAL SECONDARY OBJECTIVES, ALL OF WHICH HELP YOU TO\n"
      "ACHIEVE A STATED POINT TOTAL NEEDED TO ADVANCE TO THE NEXT LEVEL.\n"
      "POINTS ARE ALSO AWARDED FOR KILLS AS WELL AS ACQUIRING RANDOMLY\n"
      "PLACED OBJECTS TAKEN FROM THE SHIP'S INVENTORY. EXPECT\n"
      "NON-COOPERATIVES (NOPS) FROM OTHER PARTS OF THE COLONY TO BE\n"
      "BROUGHT IN AT REGULAR INTERVALS TO REPLACE CASUALTIES OF THE HUNT.\n");
   BriefingWaitKey();
   if (quitgame || lastascii==27) goto end;

   loadscreen("BRIEF2");
   newascii=false;
   for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
    {
     if (newascii) fontbasecolor=8;   /* snap the ramp -- see BriefingFadeInPage */
     printy=139;
     FN_PrintCentered(
      "THIS MISSION WILL BEGIN IN THE INMATE PROCESSING AREA, WHERE\n"
      "YOU ARE TO SEARCH FOR AN EXPERIMENTAL EXPLOSIVE HIDDEN\n"
      "IN THE SUBLEVEL.\n"
      "SECONDARY GOALS ARE PHOSPHER PELLETS AND DELOUSING KITS.\n");
     Wait(3);
     }
   BriefingWaitKey();
   if (quitgame || lastascii==27) goto end;

   loadscreen("BRIEF2");
   newascii=false;
   for(fontbasecolor=0;fontbasecolor<9;++fontbasecolor)
    {
     if (newascii) fontbasecolor=8;   /* snap the ramp -- see BriefingFadeInPage */
     printy=139;
     FN_PrintCentered(
      "YOU WILL BE MONITORED.  POINTS WILL BE AWARDED FOR PRIMARY,\n"
      "SECONDARY, AND RANDOM ITEMS, AS WELL AS FOR KILLING NOPS.\n"
      "WHEN YOU'VE ACQUIRED THE PRIMARY ITEM AND YOUR POINT TOTAL\n"
      "MEETS OR EXCEEDS 50000 WE'LL OPEN A TRANSLATION NEXUS.  WATCH\n"
      "FOR THE FLASHING EXIT SIGN.  ENTER THE NEXUS AND WE'LL\n"
      "TRANSLATE YOU TO THE NEXT AREA OF THE BASE.\n \nGOOD LUCK.");
     Wait(3);
     }
   BriefingWaitKey();
   if (quitgame || lastascii==27) goto end;
   }
#ifdef GAME1
 else if (map<8)
#elif defined(GAME2)
 else if (map<16)
#else
 else if (map<22)
#endif
  {
   if (map==8)
    {
     player.levelscore=levelscore;
     player.weapons[2]=-1;
     player.weapons[3]=-1;
     player.weapons[4]=-1;
     player.currentweapon=0;
     loadweapon(player.weapons[0]);
     memset(player.inventory,0,sizeof(player.inventory));
     player.inventory[7]=2;
     player.inventory[5]=2;
     player.inventory[4]=2;
     player.inventory[2]=4;
     player.ammo[0]=100;
     player.ammo[1]=100;
     player.ammo[2]=100;
     player.angst=player.maxangst;
     player.shield=200;
     selectsong(99);

#ifdef CDROMGREEDDIR

 #ifndef GAME2
      sprintf(name,"%c:\\GREED\\MOVIES\\PRISON1.FLI",cdr_drivenum+'A');
      playfli(name,0);
 #endif
      sprintf(name,"%c:\\GREED\\MOVIES\\TEMPLE1.FLI",cdr_drivenum+'A');
      playfli(name,0);

#else

 #ifndef GAME2
      sprintf(name,"%c:\\MOVIES\\PRISON1.FLI",cdr_drivenum+'A');
      playfli(name,0);
 #endif
      sprintf(name,"%c:\\MOVIES\\TEMPLE1.FLI",cdr_drivenum+'A');
      playfli(name,0);


#endif

     selectsong(map);

     VI_FillPalette(0,0,0);
     loadscreen("BRIEF4");
     newascii=false;
     BriefingFadeInPage(139,
       "THIS IS THE CITY-TEMPLE OF RISTANAK, ANCIENT HOME TO THE\n"
       "PRIESTHOOD OF YRKTAREL.  THE PRIESTHOOD HAS WORSHIPPED THEIR\n"
       "PAGAN DEITY FOR CENTURIES IN PEACE... UNTIL NOW.\n");
     BriefingWaitKey();
     if (quitgame || lastascii==27) goto end;
     VI_FadeOut(0,256,0,0,0,64);

     loadscreen("BRIEF5");
     newascii=false;
     BriefingFadeInPage(139,
       "THE PRIMARY OBJECTIVE FOR THE TEMPLE IS THE ENCODED\n"
       "PERSONALITY MATRIX OF THE DEMON-SAINT B'RNOURD.  THIS IS,\n"
       "OF COURSE, AN ITEM WHOSE POSSESSION, IF KNOWN, WOULD BRING\n"
       "INSTANT DESTRUCTION.  THE IMPERIAL COUNCIL WOULD ORDER THE\n"
       "SECTOR STERILIZED IF IT KNEW OF ITS EXISTENCE.\n"
       "THE A.V.C. BELIEVES THE ENCODE TO CONTAIN FORGOTTEN\n"
       "TECHNOLOGIES WHICH WOULD BE PRICELESS ON THE BLACK MARKET.\n"
       "IT IS YOUR MISSION TO ACQUIRE IT.\n");
     BriefingWaitKey();
     if (quitgame || lastascii==27) goto end;
     VI_FadeOut(0,256,0,0,0,64);

     }
   else if (map==16)
    {
     player.levelscore=levelscore;
     player.weapons[2]=-1;
     player.weapons[3]=-1;
     player.weapons[4]=-1;
     player.currentweapon=0;
     loadweapon(player.weapons[0]);
     memset(player.inventory,0,sizeof(player.inventory));
     player.inventory[7]=2;
     player.inventory[5]=2;
     player.inventory[4]=2;
     player.inventory[2]=4;
     player.ammo[0]=100;
     player.ammo[1]=100;
     player.ammo[2]=100;
     player.angst=player.maxangst;
     player.shield=200;
     selectsong(99);
#ifdef CDROMGREEDDIR

 #ifndef GAME3
      sprintf(name,"%c:\\GREED\\MOVIES\\TEMPLE2.FLI",cdr_drivenum+'A');
      playfli(name,0);
 #endif
      sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS1.FLI",cdr_drivenum+'A');
      playfli(name,0);
      sprintf(name,"%c:\\GREED\\MOVIES\\JUMPBAS2.FLI",cdr_drivenum+'A');
      playfli(name,0);
#else

 #ifndef GAME3
      sprintf(name,"%c:\\MOVIES\\TEMPLE2.FLI",cdr_drivenum+'A');
      playfli(name,0);
 #endif
      sprintf(name,"%c:\\MOVIES\\JUMPBAS1.FLI",cdr_drivenum+'A');
      playfli(name,0);
      sprintf(name,"%c:\\MOVIES\\JUMPBAS2.FLI",cdr_drivenum+'A');
      playfli(name,0);
#endif


     selectsong(map);

     VI_FillPalette(0,0,0);

     loadscreen("BRIEF6");
     newascii=false;
     BriefingFadeInPage(139,
       "DURING THE INSURRECTION AT ALPHA PRAM,  THE FOURTH PLANET IN\n"
       "THE SYSTEM, WHICH WAS BASE TO THE ELITE GALACTIC CORPS, WAS\n"
       "DESTROYED BY A BOVINARIAN VIOLATOR SHIP.  THE SHIELDING\n"
       "SURROUNDING THE MOUNTAIN WHERE THE CORPS WAS BASED WAS SO\n"
       "STRONG, HOWEVER, THAT THE MOUNTAIN SURVIVED.  THE BASE WAS\n"
       "THEN MOUNTED TO A TROJAN GATE JUMP POINT AND TO THIS DAY IT\n"
       "REMAINS AS A WAY POINT BETWEEN THE RIM WORLDS AND THE CORE\n"
       "QUARTER, AS WELL AS HOUSING MILITARY MIGHT IN THIS SECTOR.\n");
     BriefingWaitKey();
     if (quitgame || lastascii==27) goto end;
     VI_FadeOut(0,256,0,0,0,64);

     loadscreen("BRIEF7");
     newascii=false;
     BriefingFadeInPage(139,
       "THE PRIMARY OBJECTIVE FOR THIS WORLD IS THE IMPERIAL SIGIL.\n"
       "IT IS THE SYMBOL OF POWER WHICH MAINTAINS THE CHANCELLOR\n"
       "IN HIS POSITION OF DOMINANCE WITHIN THE SECTOR.  YOU HAVE BUT\n"
       "TO TAKE THE SIGIL FROM THE CHANCELLOR HIMSELF.  UNFORTUNATELY\n"
       "FOR YOU, THE DESPOTIC CHANCELLOR HAD HIS FLESH REPLACED\n"
       "BY A CYBERNETIC SYMBIOTE IN ORDER TO INSURE HIS IMMORTALITY\n"
       "AND SUBSEQUENT ETERNAL RULE OF THE CORPS.  OVER 30 ATTEMPTS\n"
       "HAVE BEEN MADE TO WREST THE SIGIL FROM THE CHANCELLOR'S GRASP.\n"
       "THEY ALL FAILED.\n");
     BriefingWaitKey();
     if (quitgame || lastascii==27) goto end;
     VI_FadeOut(0,256,0,0,0,64);

     }

   VI_FillPalette(0,0,0);
   if (map<8)
    loadscreen("TRANS");
   else if (map<16)
    loadscreen("TRANS2");
   else
    loadscreen("TRANS3");
   newascii=false;
   pprimaries=player.primaries[0]+player.primaries[1];
   tprimaries=pcount[0] + pcount[1];
   psecondaries=0;
   tsecondaries=0;
   for(i=0;i<7;i++)
    {
     psecondaries+=player.secondaries[i];
     tsecondaries+=scount[i];
     }
   fontbasecolor=8;
   printx=20;
   printy=30;
   sprintf(str,"MISSION SUCCESSFUL!");
   FN_RawPrint3(str);
   printx=25;
   printy=40;
   sprintf(str,"PRIMARY GOALS STOLEN: %i of %i",pprimaries,tprimaries);
   FN_RawPrint3(str);
   printx=25;
   printy=50;
   sprintf(str,"SECONDARY GOALS STOLEN: %i of %i",psecondaries,tsecondaries);
   FN_RawPrint3(str);
   printx=25;
   printy=65;
   sprintf(str,"POINT TOTAL: %i",player.score);
   FN_RawPrint3(str);
   printx=25;
   printy=75;
   sprintf(str,"TOTAL KILLS: %i",player.bodycount);
   FN_RawPrint3(str);
   /* The stats above are drawn while the palette is still black, so they come
      up with the picture instead of popping in after it, and the mission
      summary ramps across the same fade. */
   VI_GetPalette(basep);
   for (i=1;i<=64;i++)
    {
     if (newascii) i=64;   /* snap the ramp -- see BriefingFadeInPage */
     fontbasecolor=(i*9)/64;
     if (fontbasecolor>8) fontbasecolor=8;
     printy=85;
     FN_PrintCentered(missioninfo[map][0]);
     FN_PrintCentered(missioninfo[map][1]);
     FN_PrintCentered(missioninfo[map][2]);
     BriefingFadeStep(basep,i,64);
     Wait(1);
     }
   VI_SetPalette(colors);
   BriefingWaitKey();
   if (quitgame) goto end;
   VI_FadeOut(0,256,0,0,0,64);
   }

end:
 memcpy(viewbuffer,scr,64000);
 free(scr);
 memset(screen,0,64000);
 VI_SetPalette(CA_CacheLump(CA_GetNamedNum("palette")));
 timecount=oldtimecount;
 }
