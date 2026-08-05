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
#include "d_disk.h"
#include "d_global.h"
#include "d_video.h"
#include "d_ints.h"
#include "r_refdef.h"
#include "protos.h"

/**** CONSTANTS ****/


char *primnames[]=
 {
  "Explosives",
  "Head of Warden",
  "Phlegmatic Eel",
  "Byzantium Brass Ring",
  "Sacrificial Dagger",
  "Book of Chants",
  "Holy Incantation Brazier",
  "error 7",
  "Personality Encode",
  "Space Generator",
  "Imperial Sigil",
  "Psiflex Data Module",
  "Fissure Prism",
  "Mummification Glyph",
  "Soylent Brown Narcotic",
  "Viral Stabilization Pods",
  "Idol of the Felasha Pont",
  "Skull Scepter",
  "Sacred Cow of Tooms",
  "Gene Coding Cube",
  "Desecrate Summoning Circle",
  "Power Coupler",
  };


char *secnames[]=
 {
  "Inmate Uniforms",
  "Delousing Kits",
  "Truth Serum Vials",
  "Hypodermic Needles",
  "Lubricants",
  "Emergency Lantern",
  "Water Tanks",
  "Oxygen Tanks",
  "Exo-suit",
  "Phosfor Pellets",
  "Plasma Conduit Coupling",
  "Madree 3 Cypher Kit",
  "Security Key",
  "Denatured Bio-Proteins",
  "Neural Reanimation Focus",
  "Shunt Matrix",
  "Plasmoid CryoVice",
  "Rad-Shield Goggles",
  "Prayer Scroll",
  "Silver Beetle",
  "Finger Bones",
  "Pain Ankh",
  "War Slug Larvae",
  "War Slug Food",
  "Ritual Candles",
  "Idth Force Key 1",
  "Idth Force Key 2",
  "Idth Force Key 3",
  "Idth Force Key 4",
  "error 29",
  "error 30",
  "error 31",
  "error 32",
  "Tribolek Game Cubes",
  "Atomic Space Heater",
  "Reactor Coolant Container",
  "Power Flow Calibrator",
  "Verimax Insulated Gloves",
  "Gold Ingots",
  "Soul Orb of Eyul",
  "Prayer Scroll"
  };


char *ammonames[]=
 {
  "ENERGY",
  "BULLET",
  "PLASMA",
  };


char *inventorynames[]=
 {
  "Medical Tube  ",
  "Shield Charge ",
  "Grenade       ",
  "Reverso Pill  ",
  "Proximity Mine",
  "Time Bomb     ",
  "Decoy         ",
  "InstaWall     ",
  "Clone         ",
  "HoloSuit      ",
//"Portable Hole ",
  "Invisibility  ",
  "Warp Jammer   ",
  "Soul Stealer  "
  };


/**** VARIABLES ****/

byte    oldcharges[7];       /* delta values (only changes if different) */
longint oldbodycount, oldscore, oldlevelscore;
int     oldshield, oldangst, oldshots, oldgoalitem, oldangle, statcursor=269,
	oldangstpic, inventorycursor=-1, oldinventory, totaleffecttime,
	inventorylump, primarylump, secondarylump, fraglump;
pic_t   *heart[10];
bool firegrenade;

extern int fragcount[MAXPLAYERS];

/**** FUNCTIONS ****/

/*========================================================================*/

void resetdisplay(void)
/* clears delta values */
{
 int i;

 memset(oldcharges,-1,7);
 oldbodycount=-1;
 oldshield=-1;
 oldangst=-1;
 oldscore=-1;
 oldshots=-1;
 oldlevelscore=-1;
 oldangle=-1;
 oldangstpic=-1;
 oldinventory=-2;
 oldgoalitem=-1;
 inventorylump=CA_GetNamedNum("medtube");
 primarylump=CA_GetNamedNum("primary");
 secondarylump=CA_GetNamedNum("secondary");
 if (netmode)
  {
   fraglump=CA_GetNamedNum("profiles");
   for (i=0;i<MAXCHARTYPES;i++)
    CA_CacheLump(fraglump+i);
   }
 }


void inventoryright(void)
{
 if (inventorycursor==-1) inventorycursor=0;
  else inventorycursor++;
 while (inventorycursor<MAXINVENTORY && player.inventory[inventorycursor]==0) inventorycursor++;
 if (inventorycursor>=MAXINVENTORY)
  {
   inventorycursor=0;
   while (inventorycursor<MAXINVENTORY && player.inventory[inventorycursor]==0) inventorycursor++;
   if (inventorycursor==MAXINVENTORY) inventorycursor=-1; // nothing found
   }
 }


void inventoryleft(void)
{
 inventorycursor--;
 while (inventorycursor>=0 && player.inventory[inventorycursor]==0) inventorycursor--;
 if (inventorycursor<=-1)
  {
   inventorycursor=MAXINVENTORY-1;
   while (inventorycursor>=0 && player.inventory[inventorycursor]==0) inventorycursor--;
   }
 }


void useinventory(void)
{
 int         mindist, pic, x, y, dist, angle, angleinc, i, scale;
 scaleobj_t  *sp;

 if (inventorycursor==-1 || player.inventory[inventorycursor]==0) return;

 switch (inventorycursor)
  {
   case 0: // medtube
    if (player.angst<player.maxangst)
     {
      medpaks(50);
      --player.inventory[inventorycursor];
      writemsg("Used Medtube");
      }
    break;
   case 1: // shield
    if (player.shield<player.maxshield)
     {
      heal(50);
      --player.inventory[inventorycursor];
      writemsg("Used Shield Charge");
      }
    break;
   case 2: // grenade
    firegrenade=true;
    break;
   case 3: // reversopill
    if (specialeffect!=SE_REVERSOPILL)
     {
      specialeffect=SE_REVERSOPILL;
      specialeffecttime=timecount+70*15;
      totaleffecttime=70*15;
      --player.inventory[inventorycursor];
      writemsg("Used Reverso Pill");
      }
     break;
   case 4:
    SpawnSprite(S_PROXMINE,player.x,player.y,0,0,0,0,false,playernum);
    --player.inventory[inventorycursor];
    writemsg("Dropped Proximity Mine");
    break;
   case 5:
    SpawnSprite(S_TIMEMINE,player.x,player.y,0,0,0,0,false,playernum);
    --player.inventory[inventorycursor];
    writemsg("Dropped Time Bomb");
    break;
   case 6:
    SpawnSprite(S_DECOY,player.x,player.y,0,0,0,0,false,player.chartype);
    --player.inventory[inventorycursor];
    writemsg("Activated Decoy");
    break;
   case 7:
    SpawnSprite(S_INSTAWALL,player.x,player.y,0,0,0,0,false,playernum);
    --player.inventory[inventorycursor];
    writemsg("Activated InstaWall");
    break;
   case 8: // clone
    SpawnSprite(S_CLONE,player.x,player.y,0,0,0,0,false,player.chartype);
    --player.inventory[inventorycursor];
    writemsg("Clone Activated");
    break;
   case 9: // holosuit
    mindist=0x7FFFFFFF;
    for (sp=firstscaleobj.next;sp!=&lastscaleobj;sp=sp->next)
     if (!sp->active && !sp->hitpoints)
      {
       x=(player.x-sp->x)>>FRACTILESHIFT;
       y=(player.y-sp->y)>>FRACTILESHIFT;
       dist=x*x+y*y;
       if (dist<mindist)
	{
	 pic=sp->basepic;
	 scale=sp->scale;
	 mindist=dist;
	 }
       }
     if (mindist!=0x7FFFFFFF)
      {
       --player.inventory[inventorycursor];
       player.holopic=pic;
       player.holoscale=scale;
       writemsg("HoloSuit Active");
       }
     else writemsg("Unsuccessful Holo");
    break;
/*   case 10: // portable hole
    if (mapsprites[player.mapspot]!=0)
     {
      SpawnSprite(S_HOLE,player.x,player.y,0,0,0,0,false,player.chartype);
      --player.inventory[inventorycursor];
      writemsg("Dropped Portable Hole");
      }
    else
     writemsg("Portable Hole Blocked");
    break; */
   case 10: // invisibility
    if (specialeffect!=SE_INVISIBILITY)
     {
      specialeffect=SE_INVISIBILITY;
      specialeffecttime=timecount+70*30;
      totaleffecttime=70*30;
      --player.inventory[inventorycursor];
      writemsg("Activated Invisibility Shield");
      }
     break;
   case 11: // warp jammer
    warpjammer=true;
    break;
   case 12: // soul stealer
    angleinc=ANGLES/16;
    angle=0;
    for(i=0,angle=0;i<16;i++,angle+=angleinc)
     SpawnSprite(S_SOULBULLET,player.x,player.y,player.z,player.height-(52<<FRACBITS),angle,0,true,playernum);
    SoundEffect(SN_SOULSTEALER,0,player.x,player.y);
    SoundEffect(SN_SOULSTEALER,0,player.x,player.y);
    if (netmode)
     {
      NetSendSpawn(S_SOULBULLET,player.x,player.y,player.z,player.height-(52<<FRACBITS),angle,0,true,playernum);
      NetSoundEffect(SN_SOULSTEALER,0,player.x,player.y);
      NetSoundEffect(SN_SOULSTEALER,0,player.x,player.y);
      }
    --player.inventory[inventorycursor];
    writemsg("Used Soul Stealer");
    break;
   }
 oldinventory=-2;
 inventoryleft();
 inventoryright();
 }


/*=======================================================================*/

void displaystats1(int ofs)
{
 int  d, i, j, c;
 char str1[20];

 d=(player.shield*28)/player.maxshield;
 for(i=152+ofs;i<=159+ofs;i++)
  {
   c=(d*(i-(152+ofs)))/8 + 140;
   for(j=272;j<=300;j++)
    if (*(hudylookup[i]+j)==254) *(hudylookup[i]+j)=c;
   }
 for(i=193+ofs;i<=199+ofs;i++)
  {
   c=(d*((199+ofs)-i))/7 + 140;
   for(j=272;j<=300;j++)
    if (*(hudylookup[i]+j)==254) *(hudylookup[i]+j)=c;
   }
 for(j=307;j<=315;j++)
  {
   c=(d*(315-j))/9 + 140;
   for(i=165+ofs;i<=187+ofs;i++)
    if (*(hudylookup[i]+j)==254) *(hudylookup[i]+j)=c;
   }
 for(j=257;j<=265;j++)
  {
   c=(d*(j-257))/9 + 140;
   for(i=165+ofs;i<=187+ofs;i++)
    if (*(hudylookup[i]+j)==254) *(hudylookup[i]+j)=c;
   }
 d=(player.angst*10)/player.maxangst;
 if (d>=10) d=9;
 VI_DrawMaskedPicToBuffer(269,161+ofs,heart[d]);
 statcursor+=2;
 if (statcursor>=323) statcursor=269;
 for(j=269;j<=305;j++)
  {
   if (j>statcursor) c=113;
   else
    {
     c=139 - (statcursor-j);
     if (c<120) c=113;
     }
   for(i=161+ofs;i<=185+ofs;i++)
    if (*(hudylookup[i]+j)==254) *(hudylookup[i]+j)=c;
   }
 font=font2;
 fontbasecolor=0;
 sprintf(str1,"%3i",player.angst);
 printx=280;
 printy=188+ofs;
 FN_RawPrint4(str1);
 }


void displaycompass1(int ofs)
{
 int     c, x, y, i;
 fixed_t x1, y1;

 x=237;
 x1=x<<FRACBITS;
 y=161+ofs;
 y1=y<<FRACBITS;
 c=139;
 for(i=0;i<10;i++,c--)
  {
   *(hudylookup[y]+x)=c;
   x1+=costable[player.angle];
   y1-=FIXEDMUL(sintable[player.angle],54394);
   x=x1>>FRACBITS;
   y=y1>>FRACBITS;
   }
 }


void displaysettings1(int ofs)
{
 if (heatmode) memset(hudylookup[199+ofs]+236,102,6);
  else memset(hudylookup[199+ofs]+236,0,6);
 if (motionmode) memset(hudylookup[199+ofs]+246,156,6);
  else memset(hudylookup[199+ofs]+246,0,6);
 if (mapmode) memset(hudylookup[199+ofs]+226,133,6);
  else memset(hudylookup[199+ofs]+226,0,6);
 }


void displayinventory1(int ofs)
{
 int        n, ammo, lump, maxshots, i, j, count, top, bottom, x, y;
 char       str1[20];
 scalepic_t *pic;
 byte       *collumn;

 font=font2;
 fontbasecolor=0;
 n=player.weapons[player.currentweapon];
 if (weapons[n].ammorate>0)
  {
   ammo=player.ammo[weapons[n].ammotype];
   maxshots=weapons[n].ammorate;
   }
 else
  {
   ammo=999;
   maxshots=999;
   }
 printx=186;
 printy=183+ofs;
 sprintf(str1,"%3i",ammo);
 FN_RawPrint2(str1);
 printx=203;
 printy=183+ofs;
 sprintf(str1,"%3i",maxshots);
 FN_RawPrint2(str1);
 printx=187;
 printy=191+ofs;
 FN_RawPrint2(ammonames[weapons[n].ammotype]);

 if (inventorycursor==-1 || player.inventory[inventorycursor]==0) inventoryright();
 if (inventorycursor!=-1)
  {
   lump=inventorylump + inventorycursor;
   printx=149;         // name
   printy=163+ofs;
   FN_RawPrint2(inventorynames[inventorycursor]);
   font=font3;         // number of items
   printx=150;
   printy=172+ofs;
   sprintf(str1,"%2i",player.inventory[inventorycursor]);
   FN_RawPrint2(str1);
   pic=lumpmain[lump]; // draw the pic for it
   x=128-(pic->width>>1);
   for (i=0;i<pic->width;i++,x++)
    if (pic->collumnofs[i])
     {
      collumn=(byte *)pic+pic->collumnofs[i];
      top=*(collumn+1);
      bottom=*(collumn);
      count=bottom-top+1;
      collumn+=2;
      y=(188+ofs)-top-count;
      for (j=0;j<count;j++,collumn++,y++)
       if (*collumn)
        *(hudylookup[y]+x)=*collumn;
      }
   }
 else
  {
   printx=149;   // name
   printy=163+ofs;
   sprintf(str1,"%14s"," ");
   FN_RawPrint2(str1);
   font=font3;   // number of items
   printx=150;
   printy=172+ofs;
   FN_RawPrint2("  ");
   }

 }


void displaynetbonusitem1(int ofs)
{
 char       *type, *name;
 char       str1[40];
 int        i, j, count, top, bottom, x, y, lump, score;
 scalepic_t *pic;
 pic_t      *p;
 byte       *collumn, *b;
 longint    time;

 font=font2;
 fontbasecolor=0;
 if (goalitem==-1)
  {
   if (BonusItem.score) togglegoalitem=true;
   return;
   }
 if (goalitem==0)
  {
   if (BonusItem.score==0) // bonus item go bye-bye
    {
     togglegoalitem=true;
     return;
     }
   lump=BonusItem.sprite->basepic;
   score=BonusItem.score;
   type="Bonus  ";
   time=(BonusItem.time-timecount)/70;
   if (time>10000) time=0;
   sprintf(str1,"%3i s ",time);
   name=BonusItem.name;
   pic=lumpmain[lump];
   x=34-(pic->width>>1);
   for (i=0;i<pic->width;i++,x++)
    if (pic->collumnofs[i])
     {
      collumn=(byte *)pic+pic->collumnofs[i];
      top=*(collumn+1);
      bottom=*(collumn);
      count=bottom-top+1;
      collumn+=2;
      y=(188+ofs)-top-count;
       for (j=0;j<count;j++,collumn++,y++)
	if (*collumn)
	 *(hudylookup[y]+x)=*collumn;
      }
   }
 else
  {
   lump=fraglump + playerdata[goalitem-1].chartype;
   p=(pic_t *)lumpmain[lump];
   type="Player ";
   name=netnames[goalitem-1];
   sprintf(str1,"%-5i",fragcount[goalitem-1]);
   b=&(p->data);
   for (y=0;y<30;y++)
    for (x=0;x<30;x++,b++)
     *(hudylookup[y+158+ofs]+19+x)=*b;
   }
 printx=53;
 printy=172+ofs;
 FN_RawPrint2(str1);
 printx=53;
 printy=182+ofs;
 FN_RawPrint2(type);
 printx=22;
 printy=192+ofs;
 sprintf(str1,"%-32s",name);
 FN_RawPrint2(str1);
 }


void displaybonusitem1(int ofs)
{
 char       *type, *name;
 char       str1[40];
 int        i, j, count, top, bottom, x, y, lump, score, found, total;
 scalepic_t *pic;
 byte       *collumn;
 longint    time;

 font=font2;
 fontbasecolor=0;
 if (goalitem==-1)
  {
   if (BonusItem.score) togglegoalitem=true;
   return;
   }
 if (goalitem==0)
  {
   if (BonusItem.score==0) // bonus item go bye-bye
    {
     togglegoalitem=true;
     return;
     }
   lump=BonusItem.sprite->basepic;
   score=BonusItem.score;
   type="Bonus  ";
   time=(BonusItem.time-timecount)/70;
   if (time>10000) time=0;
   sprintf(str1,"%3i s",time);
   name=BonusItem.name;
   }
 else if (goalitem>=1 && goalitem<=2)
  {
   lump=primarylump + primaries[(goalitem-1)*2];
   score=primaries[(goalitem-1)*2+1];
   type="Primary";
   found=player.primaries[goalitem-1];
   total=pcount[goalitem-1];
   sprintf(str1,"%2i/%2i",found,total);
   name=primnames[primaries[(goalitem-1)*2]];
   }
 else if (goalitem>=3)
  {
   lump=secondarylump + secondaries[(goalitem-3)*2];
   score=secondaries[(goalitem-3)*2+1];
   type="Second ";
   found=player.secondaries[goalitem-3];
   total=scount[goalitem-3];
   sprintf(str1,"%2i/%2i",found,total);
   name=secnames[secondaries[(goalitem-3)*2]];
   }

 pic=lumpmain[lump];
 x=34-(pic->width>>1);
 for (i=0;i<pic->width;i++,x++)
  if (pic->collumnofs[i])
   {
    collumn=(byte *)pic+pic->collumnofs[i];
    top=*(collumn+1);
    bottom=*(collumn);
    count=bottom-top+1;
    collumn+=2;
    y=(188+ofs)-top-count;
    for (j=0;j<count;j++,collumn++,y++)
     if (*collumn)
      *(hudylookup[y]+x)=*collumn;
    }

 printx=53;
 printy=162+ofs;
 FN_RawPrint2(str1);
 sprintf(str1,"%5i",score);
 printx=53;
 printy=172+ofs;
 FN_RawPrint2(str1);
 printx=53;
 printy=182+ofs;
 FN_RawPrint2(type);
 printx=22;
 printy=192+ofs;
 sprintf(str1,"%-32s",name);
 FN_RawPrint2(str1);
 }


void displayrightstats1(int ofs)
{
 int  n, shots;
 char str1[10];

 n=player.weapons[player.currentweapon];
 if (weapons[n].ammorate>0)
  shots=player.ammo[weapons[n].ammotype]/weapons[n].ammorate;
 else shots=999;
 font=font3;
 printx=225;
 printy=178+ofs;
 fontbasecolor=0;
 sprintf(str1,"%3i",shots);
 FN_RawPrint2(str1);
 displaycompass1(ofs);
 displaysettings1(ofs);
 displaystats1(ofs);
 }

/*==========================================================================*/

void displaystats2(void)
{
 int  d, i, j, c, p;
 char str1[20];

 if (player.shield!=oldshield)
  {
   oldshield=player.shield;
   d=(player.shield*28)/player.maxshield;
   for(i=152;i<=159;i++)
    {
     c=(d*(i-(152)))/8 + 140;
     for(j=272;j<=300;j++)
      {
       p=*(ylookup[i]+j);
       if (p==254 || (p>=140 && p<=168)) *(ylookup[i]+j)=c;
       }
     }
   for(i=193;i<=199;i++)
    {
     c=(d*((199)-i))/7 + 140;
     for(j=272;j<=300;j++)
      {
       p=*(ylookup[i]+j);
       if (p==254 || (p>=140 && p<=168)) *(ylookup[i]+j)=c;
       }
     }
   for(j=307;j<=315;j++)
    {
     c=(d*(315-j))/9 + 140;
     for(i=165;i<=187;i++)
      {
       p=*(ylookup[i]+j);
       if (p==254 || (p>=140 && p<=168)) *(ylookup[i]+j)=c;
       }
     }
   for(j=257;j<=265;j++)
    {
     c=(d*(j-257))/9 + 140;
     for(i=165;i<=187;i++)
      {
       p=*(ylookup[i]+j);
       if (p==254 || (p>=140 && p<=168)) *(ylookup[i]+j)=c;
       }
     }
   }
 if (player.angst!=oldangst)
  {
   d=(player.angst*10)/player.maxangst;
   if (d>=10) d=9;
   if (oldangstpic!=d)
    {
     VI_DrawPic(269,161,heart[d]);
     oldangstpic=d;
     }
   oldangst=player.angst;
   font=font2;
   fontbasecolor=0;
   sprintf(str1,"%3i",player.angst);
   printx=280;
   printy=188;
   FN_RawPrint(str1);
   }
 statcursor+=2;
 if (statcursor>=323) statcursor=269;
 for(j=269;j<=305;j++)
  {
   if (j>statcursor) c=113;
   else
    {
     c=139 - (statcursor-j);
     if (c<120) c=113;
     }
   for(i=161;i<=185;i++)
    {
     p=*(ylookup[i]+j);
     if (p==254 || (p>=113 && p<=139)) *(ylookup[i]+j)=c;
     }
   }
 }


void displaycompass2(void)
{
 int     c, x, y, i;
 fixed_t x1, y1;

 if (player.angle==oldangle) return;
 if (oldangle!=-1)
  {
   x=237;
   x1=x<<FRACBITS;
   y=161;
   y1=y<<FRACBITS;
   for(i=0;i<10;i++)
    {
     *(ylookup[y]+x)=0;
     x1+=costable[oldangle];
     y1-=FIXEDMUL(sintable[oldangle],54394);
     x=x1>>FRACBITS;
     y=y1>>FRACBITS;
     }
   }
 oldangle=player.angle;
 x=237;
 x1=x<<FRACBITS;
 y=161;
 y1=y<<FRACBITS;
 c=139;
 for(i=0;i<10;i++,c--)
  {
   *(ylookup[y]+x)=c;
   x1+=costable[oldangle];
   y1-=FIXEDMUL(sintable[oldangle],54394);
   x=x1>>FRACBITS;
   y=y1>>FRACBITS;
   }
 }


void displaysettings2(void)
{
 if (heatmode) memset(ylookup[199]+236,102,6);
  else memset(ylookup[199]+236,0,6);
 if (motionmode) memset(ylookup[199]+246,156,6);
  else memset(ylookup[199]+246,0,6);
 if (mapmode) memset(ylookup[199]+226,133,6);
  else memset(ylookup[199]+226,0,6);
 }


void displayinventory2(void)
{
 int        n, ammo, lump, i, j, count, top, bottom, x, y, maxshots;
 char       str1[20];
 scalepic_t *pic;
 byte       *collumn;

 font=font2;
 fontbasecolor=0;
 n=player.weapons[player.currentweapon];
 if (weapons[n].ammorate>0)
  {
   ammo=player.ammo[weapons[n].ammotype];
   maxshots=weapons[n].ammorate;
   }
 else
  {
   maxshots=999;
   ammo=999;
   }
 printx=186;
 printy=183;
 FN_Printf("%3i",ammo);
 printx=203;
 printy=183;
 FN_Printf("%3i",maxshots);
 printx=187;
 printy=191;
 FN_RawPrint(ammonames[weapons[n].ammotype]);

 if (inventorycursor==-1 || player.inventory[inventorycursor]==0) inventoryright();
 if (inventorycursor!=oldinventory)
  {
   oldinventory=inventorycursor;
   for (i=162;i<=184;i++) memset(ylookup[i]+113,0,30);
   if (inventorycursor!=-1)
    {
     lump=inventorylump + inventorycursor;
     printx=149;         // name
     printy=163;
     FN_RawPrint(inventorynames[inventorycursor]);
     pic=lumpmain[lump]; // draw the pic for it
     x=128-(pic->width>>1);
     for (i=0;i<pic->width;i++,x++)
      if (pic->collumnofs[i])
       {
	collumn=(byte *)pic+pic->collumnofs[i];
	top=*(collumn+1);
	bottom=*(collumn);
	count=bottom-top+1;
	collumn+=2;
	y=188-top-count;
	for (j=0;j<count;j++,collumn++,y++)
	 if (*collumn)
          *(ylookup[y]+x)=*collumn;
	}
     font=font3;         // number of items
     printx=150;
     printy=172;
     sprintf(str1,"%2i",player.inventory[inventorycursor]);
     FN_RawPrint(str1);
     }
   else
    {
     printx=149;   // name
     printy=163;
     sprintf(str1,"%14s"," ");
     FN_RawPrint(str1);
     font=font3;   // number of items
     printx=150;
     printy=172;
     FN_RawPrint("  ");
     }
   }
 }


void displaynetbonusitem2(void)
{
 char       *type, *name;
 char       str1[40];
 int        i, j, count, top, bottom, x, y, lump, score;
 scalepic_t *pic;
 pic_t      *p;
 byte       *collumn, *b;
 longint    time;

 if (goalitem==-1)
  {
   if (BonusItem.score) togglegoalitem=true;
   for(i=158;i<188;i++)
    memset(ylookup[i]+19,0,30);
   font=font2;
   fontbasecolor=0;
   printx=53;
   printy=162;
   FN_RawPrint("     ");
   printx=53;
   printy=172;
   FN_RawPrint("     ");
   printx=53;
   printy=182;
   FN_RawPrint("       ");
   printx=22;
   printy=192;
   sprintf(str1,"%-32s"," ");
   FN_RawPrint(str1);
   return;
   }
 if (goalitem==0 && BonusItem.score==0)
  {
   togglegoalitem=true;
   return;
   }
 if (oldgoalitem==goalitem) return;
 font=font2;
 fontbasecolor=0;
 if (goalitem==0)
  {
   lump=BonusItem.sprite->basepic;
   score=BonusItem.score;
   type="Bonus  ";
   time=(BonusItem.time-timecount)/70;
   if (time>10000) time=0;
   sprintf(str1,"%3i s",time);
   name=BonusItem.name;
   pic=lumpmain[lump];
   x=34-(pic->width>>1);
   for(i=158;i<188;i++)
    memset(ylookup[i]+19,0,30);
   for (i=0;i<pic->width;i++,x++)
    if (pic->collumnofs[i])
     {
      collumn=(byte *)pic+pic->collumnofs[i];
      top=*(collumn+1);
      bottom=*(collumn);
      count=bottom-top+1;
      collumn+=2;
      y=188-top-count;
      for (j=0;j<count;j++,collumn++,y++)
       if (*collumn)
	*(ylookup[y]+x)=*collumn;
      }
   }
 else
  {
   lump=fraglump + playerdata[goalitem-1].chartype;
   p=(pic_t *)lumpmain[lump];
   type="Player";
   name=netnames[goalitem-1];
   sprintf(str1,"%-5i",fragcount[goalitem-1]);

   b=&(p->data);
   for (y=0;y<30;y++)
    for (x=0;x<30;x++,b++)
     *(ylookup[y+158]+19+x)=*b;
   }
 oldgoalitem=goalitem;
 for(i=162;i<168;i++)
  memset(ylookup[i]+53,0,23);
 for(i=172;i<178;i++)
  memset(ylookup[i]+53,0,30);
 for(i=182;i<188;i++)
  memset(ylookup[i]+53,0,39);
 for(i=192;i<198;i++)
  memset(ylookup[i]+22,0,159);
 printx=53;
 printy=162;
 FN_RawPrint("     ");
 printx=53;
 printy=172;
 FN_RawPrint(str1);
 printx=53;
 printy=182;
 FN_RawPrint(type);
 printx=22;
 printy=192;
 sprintf(str1,"%-32s",name);
 FN_RawPrint(str1);
 }


void displaybonusitem2(void)
{
 char       *type, *name;
 char       str1[40];
 int        i, j, count, top, bottom, x, y, lump, score, found, total;
 scalepic_t *pic;
 byte       *collumn;
 longint    time;

 if (goalitem==-1)
  {
   if (BonusItem.score) togglegoalitem=true;
   for(i=158;i<188;i++)
    memset(ylookup[i]+19,0,30);
   font=font2;
   fontbasecolor=0;
   printx=53;
   printy=162;
   FN_RawPrint("     ");
   printx=53;
   printy=172;
   FN_RawPrint("     ");
   printx=53;
   printy=182;
   FN_RawPrint("       ");
   printx=22;
   printy=192;
   sprintf(str1,"%-32s"," ");
   FN_RawPrint(str1);
   return;
   }
 if (goalitem==0 && BonusItem.score==0)
  {
   togglegoalitem=true;
   return;
   }
 if (oldgoalitem==goalitem && goalitem!=0) return;
 font=font2;
 fontbasecolor=0;
 if (goalitem==0)
  {
   lump=BonusItem.sprite->basepic;
   score=BonusItem.score;
   type="Bonus  ";
   time=(BonusItem.time-timecount)/70;
   if (time>10000) time=0;
   sprintf(str1,"%3i s",time);
   name=BonusItem.name;
   if (oldgoalitem==goalitem)
    {
     printx=53;
     printy=162;
     FN_RawPrint(str1);
     return;
     }
   }
 else if (goalitem>=1 && goalitem<=2)
  {
   lump=primarylump + primaries[(goalitem-1)*2];
   score=primaries[(goalitem-1)*2+1];
   type="Primary";
   found=player.primaries[goalitem-1];
   total=pcount[goalitem-1];
   sprintf(str1,"%2i/%2i",found,total);
   name=primnames[primaries[(goalitem-1)*2]];
   }
 else if (goalitem>=3)
  {
   lump=secondarylump + secondaries[(goalitem-3)*2];
   score=secondaries[(goalitem-3)*2+1];
   type="Second ";
   found=player.secondaries[goalitem-3];
   total=scount[goalitem-3];
   sprintf(str1,"%2i/%2i",found,total);
   name=secnames[secondaries[(goalitem-3)*2]];
   }

 oldgoalitem=goalitem;

 for(i=158;i<188;i++)
  memset(ylookup[i]+19,0,30);
 pic=lumpmain[lump];
 x=34-(pic->width>>1);
 for (i=0;i<pic->width;i++,x++)
  if (pic->collumnofs[i])
   {
    collumn=(byte *)pic+pic->collumnofs[i];
    top=*(collumn+1);
    bottom=*(collumn);
    count=bottom-top+1;
    collumn+=2;
    y=188-top-count;
    for (j=0;j<count;j++,collumn++,y++)
     if (*collumn)
      *(ylookup[y]+x)=*collumn;
    }

 for(i=162;i<168;i++)
  memset(ylookup[i]+53,0,23);
 for(i=172;i<178;i++)
  memset(ylookup[i]+53,0,30);
 for(i=182;i<188;i++)
  memset(ylookup[i]+53,0,39);
 for(i=192;i<198;i++)
  memset(ylookup[i]+22,0,159);

 printx=53;
 printy=162;
 FN_RawPrint(str1);
 printx=53;
 printy=172;
 FN_Printf("%5i",score);
 printx=53;
 printy=182;
 FN_RawPrint(type);
 printx=22;
 printy=192;
 sprintf(str1,"%-32s",name);
 FN_RawPrint(str1);
 }


void displayrightstats2(void)
{
 int n, shots;

 n=player.weapons[player.currentweapon];
 if (weapons[n].ammorate>0)
  shots=player.ammo[weapons[n].ammotype]/weapons[n].ammorate;
 else shots=999;
 if (shots!=oldshots)
  {
   font=font3;
   printx=225;
   printy=178;
   fontbasecolor=0;
   FN_Printf("%3i",shots);
   oldshots=shots;
   }
 displaycompass2();
 displaysettings2();
 displaystats2();
 }


void displaybodycount2(void)
{
 char str1[20];
 int  i, j, d, d1, c;

 if (player.bodycount!=oldbodycount)
  {
   font=font2;
   printx=178;
   printy=3;
   fontbasecolor=0;
   sprintf(str1,"%8u",player.bodycount);   /* longint is 32-bit; %lu would misread varargs */
   FN_RawPrint(str1);
   oldbodycount=player.bodycount;
   }
 if (player.score!=oldscore)
  {
   font=font2;
   printx=28;
   printy=3;
   fontbasecolor=0;
   sprintf(str1,"%9u",player.score);
   FN_RawPrint(str1);
   oldscore=player.score;
   }
 if (player.levelscore!=(int)oldlevelscore)
  {
   font=font2;
   printx=104;
   printy=3;
   fontbasecolor=0;
   sprintf(str1,"%9d",player.levelscore);   /* levelscore is int, not longint */
   FN_RawPrint(str1);
   oldlevelscore=player.levelscore;
   }
 if (specialeffecttime!=0x7FFFFFFF)
  {
   d1=(specialeffecttime-timecount)>>2;
   if (d1>97) d=97;
    else if (d1<0) d=0;
    else d=d1;

   for(j=0;j<d;j++)
    {
     c=(j*26)/d1;
     for(i=2;i<=8;i++)
      *(ylookup[i]+j+221)=c+140;
     }
   if (d<97)
    for(j=d;j<=97;j++)
     for(i=2;i<=8;i++)
      *(ylookup[i]+j+221)=0;
   }
 }

/*=====================================================================*/

void displayinventoryitem(void)
{
 int        lump, i, j, count, top, bottom, x, y;
 char       str1[20];
 scalepic_t *pic;
 byte       *collumn;

 if (inventorycursor<0) return;
 for(i=0;i<27;i++)
  {
   memset(hudylookup[i]+hudWidth-54,0,54);
   *(hudylookup[i]+hudWidth-55)=30;
   }
 memset(hudylookup[i]+hudWidth-55,30,55);
 lump=inventorylump + inventorycursor;
 pic=lumpmain[lump]; // draw the pic for it
 x=hudWidth-31;
 for (i=0;i<pic->width;i++,x++)
  if (pic->collumnofs[i])
   {
    collumn=(byte *)pic+pic->collumnofs[i];
    top=*(collumn+1);
    bottom=*(collumn);
    count=bottom-top+1;
    collumn+=2;
    y=28-top-count;
    for (j=0;j<count;j++,collumn++,y++)
     if (*collumn)
      *(hudylookup[y]+x)=*collumn;
    }
 fontbasecolor=0;
 font=font3;         // number of items
 printx=hudWidth-53;
 printy=6;
 sprintf(str1,"%2i",player.inventory[inventorycursor]);
 FN_RawPrint4(str1);
 }

/*=====================================================================*/

void updatedisplay(void)
{
 switch (currentViewSize)
  {
   case 0:
    if (timecount<inventorytime) displayinventoryitem();
    break;
   case 3:
    displayinventory1(0);   // level 3 to view
   case 2:  // no break!
    if (!netmode)
     displaybonusitem1(0);   // level 2 to view
    else
     displaynetbonusitem1(0);
   case 1:  // no break!
    displayrightstats1(0);  // level 1 to view
    if (currentViewSize<3 && timecount<inventorytime) displayinventoryitem();
    break;
   case 4:                 // level 4 to view + top to screen
    displayinventory1(-11);
    if (!netmode)
     displaybonusitem1(-11);
    else
     displaynetbonusitem1(-11);
    displayrightstats1(-11);
    displaybodycount2();
    break;
   case 5:                 // smaller screen sizes
   case 6:
   case 7:
   case 8:
   case 9:
    displayinventory2();
    if (!netmode)
     displaybonusitem2();
    else
     displaynetbonusitem2();
    displayrightstats2();
    displaybodycount2();
    break;
   }
 }


/*=====================================================================*/

void displayarrow(int x,int y)
{
 int angle;

 *(hudylookup[y]+x)=26;
 angle=(((player.angle+DEGREE45_2)&ANGLES)*8)/ANGLES;
 switch (angle)
  {
   case 0:
    *(hudylookup[y]+x+1)=40;
    *(hudylookup[y]+x-1)=20;
    break;
   case 1:
    *(hudylookup[y-1]+x+1)=40;
    *(hudylookup[y+1]+x-1)=20;
    break;
   case 2:
    *(hudylookup[y-1]+x)=40;
    *(hudylookup[y+1]+x)=20;
    break;
   case 3:
    *(hudylookup[y-1]+x-1)=40;
    *(hudylookup[y+1]+x+1)=20;
    break;
   case 4:
    *(hudylookup[y]+x-1)=40;
    *(hudylookup[y]+x+1)=20;
    break;
   case 5:
    *(hudylookup[y+1]+x-1)=40;
    *(hudylookup[y-1]+x+1)=20;
    break;
   case 6:
    *(hudylookup[y+1]+x)=40;
    *(hudylookup[y-1]+x)=20;
    break;
   case 7:
    *(hudylookup[y+1]+x+1)=40;
    *(hudylookup[y-1]+x-1)=20;
    break;
   }
 }


void displaymapmode(void)
/* head up display map */
{
 int i, j, ofsx, ofsy, x, y, px, py, mapy, mapx, c, a, miny, maxy, minx, maxx;
 int mapspot;
 int b;

 y=hudHeight/4;
 miny=-(y/2);
 maxy=y/2;
 x=hudWidth/4;
 minx=-(x/2);
 maxx=x/2;
 ofsx=1-((player.x>>(FRACBITS+4))&3);
 ofsy=1-((player.y>>(FRACBITS+4))&3);
 px=player.x>>FRACTILESHIFT;
 py=player.y>>FRACTILESHIFT;
 y=ofsy;
 for(i=miny;i<=maxy;i++,y+=4)  // display north maps
  {
   mapy=py+i;
   if (mapy<0 || mapy>=MAPROWS) continue;
   mapx=px+minx;
   mapspot=mapy*MAPCOLS+mapx;
   x=ofsx;
   for(j=minx;j<=maxx;j++,mapspot++,mapx++)
    if (mapx>=0 && mapx<MAPCOLS && player.northmap[mapspot])
     {
      c=player.northmap[mapspot];
      if (c==DOOR_COLOR)
       {
	for(a=0;a<5;a++,x++)
	 if (x>=0 && x<hudWidth && y+2<hudHeight && y+2>=0) *(hudylookup[y+2]+x)=c;
	}
      else
       {
	for(a=0;a<5;a++,x++)
	 if (x>=0 && x<hudWidth && y<hudHeight && y>=0) *(hudylookup[y]+x)=c;
	}
      x--;
      }
    else x+=4;
   }
 x=ofsx;
 for(j=minx;j<=maxx;j++,x+=4) // display west maps
  {
   mapx=px+j;
   if (mapx<0 || mapx>=MAPCOLS) continue;
   mapy=py+miny;
   mapspot=mapy*MAPCOLS+mapx;
   y=ofsy;
   for(i=miny;i<=maxy;i++,mapspot+=MAPCOLS,mapy++)
    if (mapy>=0 && mapy<MAPROWS && player.westmap[mapspot])
     {
      c=player.westmap[mapspot];
      if (c==DOOR_COLOR)
       {
	for(a=0;a<5;a++,y++)
	 if (y>=0 && y<hudHeight && x+2<hudWidth && x+2>=0) *(hudylookup[y]+x+2)=c;
	}
      else
       {
	for(a=0;a<5;a++,y++)
	 if (y>=0 && y<hudHeight && x<hudWidth && x>=0) *(hudylookup[y]+x)=c;
	}
      y--;
      }
   else y+=4;
   }
 displayarrow(hudWidth/2+1,hudHeight/2+1);
 if (BonusItem.score)
  {
   y=ofsy + 4*(BonusItem.tiley - py);
   y+=hudHeight/2;
   x=ofsx + 4*(BonusItem.tilex - px);
   x+=hudWidth/2;
   c=44;
   for(a=0;a<4;a++)
    for(b=0;b<4;b++)
     if (x+b<hudWidth && x+b>=0 && y+a<hudHeight && y+a>=0) *(hudylookup[y+a]+b+x)=c;
   }
 if (exitexists)
  {
   y=ofsy + 4*(exity - py);
   y+=hudHeight/2;
   x=ofsx + 4*(exitx - px);
   x+=hudWidth/2;
   for(a=0;a<4;a++)
    for(b=0;b<4;b++)
     if (x+b<hudWidth && x+b>=0 && y+a<hudHeight && y+a>=0) *(hudylookup[y+a]+b+x)=187;
   }
 }


void displayswingmapmode(void)
/* rotating map display */
{
 int     i, j, ofsx, ofsy, x, y, px, py, c, a, x1, y1, y2, x2;
 fixed_t xfrac, yfrac, xfrac2, yfrac2, xfracstep, yfracstep, xfracstep2, yfracstep2;
 int     mapspot;

 switch (MapZoom)
  {
   case 8:
    ofsx=1-((player.x>>(FRACBITS+3))&7); /* compute player tile offset */
    ofsy=1-((player.y>>(FRACBITS+3))&7);
    break;
   case 4:
    ofsx=1-((player.x>>(FRACBITS+4))&3); /* compute player tile offset */
    ofsy=1-((player.y>>(FRACBITS+4))&3);
    break;
   case 16:
    ofsx=1-((player.x>>(FRACBITS+2))&15); /* compute player tile offset */
    ofsy=1-((player.y>>(FRACBITS+2))&15);
    break;
   }

 px=(player.x>>FRACTILESHIFT)*MapZoom-ofsx; /* compute player position */
 py=(player.y>>FRACTILESHIFT)*MapZoom-ofsy;
 /* compute incremental values for both diagonal axis */
 xfracstep=cosines[((player.angle+SOUTH)&ANGLES)<<FINESHIFT];
 yfracstep=sines[((player.angle+SOUTH)&ANGLES)<<FINESHIFT];
 xfracstep2=cosines[player.angle<<FINESHIFT];
 yfracstep2=sines[player.angle<<FINESHIFT];
 xfrac2=((hudWidth/2)<<FRACBITS) - (py*xfracstep2 + px*xfracstep);
 yfrac2=((hudHeight/2)<<FRACBITS) - (py*yfracstep2 + px*yfracstep);
 y=yfrac2>>FRACBITS;
 x=xfrac2>>FRACBITS;
 mapspot=0;
 /* don't ask me to explain this!!!
     basically you start at upper left corner, adding one axis increment
      on the y axis and then updating the one for the x axis as it draws */
 if (BonusItem.score)
  {
   yfrac2+=yfracstep2*MapZoom*BonusItem.tiley + (yfracstep2*MapZoom/2); //+ ((MapZoom>>1)*yfracstep2);
   xfrac2+=xfracstep2*MapZoom*BonusItem.tiley + (xfracstep2*MapZoom/2); //+ ((MapZoom>>1)*yfracstep2);

   xfrac=xfrac2 + xfracstep*MapZoom*BonusItem.tilex + (xfracstep*MapZoom/2); //+ ((MapZoom>>1)*yfracstep);
   yfrac=yfrac2 + yfracstep*MapZoom*BonusItem.tilex + (yfracstep*MapZoom/2); //+ ((MapZoom>>1)*yfracstep);

   x1=(xfrac>>FRACBITS);
   y1=(yfrac>>FRACBITS);

   if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=44;
   if (y1+1>=0 && x1>=0 && x1<hudWidth && y1+1<hudHeight) *(hudylookup[y1+1]+x1)=44;
   x1++;
   if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=44;
   y1++;
   if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=44;

   xfrac2=((hudWidth/2)<<FRACBITS) - (py*xfracstep2 + px*xfracstep);
   yfrac2=((hudHeight/2)<<FRACBITS) - (py*yfracstep2 + px*yfracstep);
   y=yfrac2>>FRACBITS;
   x=xfrac2>>FRACBITS;
   }
 if (exitexists)
  {
   yfrac2+=yfracstep2*MapZoom*exity + (yfracstep2*MapZoom/2); //+ ((MapZoom>>1)*yfracstep2);
   xfrac2+=xfracstep2*MapZoom*exity + (xfracstep2*MapZoom/2); //+ ((MapZoom>>1)*yfracstep2);

   xfrac=xfrac2 + xfracstep*MapZoom*exitx + (xfracstep*MapZoom/2); //+ ((MapZoom>>1)*yfracstep);
   yfrac=yfrac2 + yfracstep*MapZoom*exitx + (yfracstep*MapZoom/2); //+ ((MapZoom>>1)*yfracstep);

   x1=(xfrac>>FRACBITS);
   y1=(yfrac>>FRACBITS);

   if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=187;
   if (y1+1>=0 && x1>=0 && x1<hudWidth && y1+1<hudHeight) *(hudylookup[y1+1]+x1)=187;
   x1++;
   if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=187;
   y1++;
   if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=187;

   xfrac2=((hudWidth/2)<<FRACBITS) - (py*xfracstep2 + px*xfracstep);
   yfrac2=((hudHeight/2)<<FRACBITS) - (py*yfracstep2 + px*yfracstep);
   y=yfrac2>>FRACBITS;
   x=xfrac2>>FRACBITS;
   }

 for(i=0;i<MAPCOLS;i++)
  {
   xfrac=xfrac2;
   yfrac=yfrac2;
   x1=x;
   y1=y;
   for(j=0;j<MAPROWS;j++,mapspot++)
    if (player.northmap[mapspot])
     {
      c=player.northmap[mapspot];
      if (c==DOOR_COLOR)
       {
	for(a=0;a<=MapZoom;a++)
	 {
	  y2=y1+(((MapZoom>>1)*yfracstep2)>>FRACBITS);
	  x2=x1+(((MapZoom>>1)*xfracstep2)>>FRACBITS);
	  if (y2>=0 && x2>=0 && x2<hudWidth && y2<hudHeight) *(hudylookup[y2]+x2)=c;
	  xfrac+=xfracstep;
	  x1=xfrac>>FRACBITS;
	  yfrac+=yfracstep;
	  y1=yfrac>>FRACBITS;
	  }
	}
      else
       {
	for(a=0;a<=MapZoom;a++)
	 {
	  if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=c;
	  xfrac+=xfracstep;
	  x1=xfrac>>FRACBITS;
	  yfrac+=yfracstep;
	  y1=yfrac>>FRACBITS;
	  }
	}
      xfrac-=xfracstep;
      x1=xfrac>>FRACBITS;
      yfrac-=yfracstep;
      y1=yfrac>>FRACBITS;
      }
    else
     {
      xfrac+=xfracstep*MapZoom;
      x1=xfrac>>FRACBITS;
      yfrac+=yfracstep*MapZoom;
      y1=yfrac>>FRACBITS;
      }
   yfrac2+=yfracstep2*MapZoom;
   y=yfrac2>>FRACBITS;
   xfrac2+=xfracstep2*MapZoom;
   x=xfrac2>>FRACBITS;
   }
 xfrac=((hudWidth/2)<<FRACBITS) - (py*xfracstep2 + px*xfracstep);
 yfrac=((hudHeight/2)<<FRACBITS) - (py*yfracstep2 + px*yfracstep);
 y=yfrac>>FRACBITS;
 x=xfrac>>FRACBITS;
 for(i=0;i<MAPCOLS;i++)
  {
   xfrac2=xfrac;
   yfrac2=yfrac;
   x1=x;
   y1=y;
   mapspot=i;
   for(j=0;j<MAPROWS;j++,mapspot+=MAPCOLS)
    if (player.westmap[mapspot])
     {
      c=player.westmap[mapspot];
      if (c==DOOR_COLOR)
       {
	for(a=0;a<=MapZoom;a++)
	 {
	  y2=y1+(((MapZoom>>1)*yfracstep)>>FRACBITS);
	  x2=x1+(((MapZoom>>1)*xfracstep)>>FRACBITS);
	  if (y2>=0 && x2>=0 && x2<hudWidth && y2<hudHeight) *(hudylookup[y2]+x2)=c;
	  xfrac2+=xfracstep2;
	  x1=xfrac2>>FRACBITS;
	  yfrac2+=yfracstep2;
	  y1=yfrac2>>FRACBITS;
	  }
	}
      else
       {
	for(a=0;a<=MapZoom;a++)
	 {
	  if (y1>=0 && x1>=0 && x1<hudWidth && y1<hudHeight) *(hudylookup[y1]+x1)=c;
	  xfrac2+=xfracstep2;
	  x1=xfrac2>>FRACBITS;
	  yfrac2+=yfracstep2;
	  y1=yfrac2>>FRACBITS;
	  }
	}
      xfrac2-=xfracstep2;
      x1=xfrac2>>FRACBITS;
      yfrac2-=yfracstep2;
      y1=yfrac2>>FRACBITS;
      }
    else
     {
      xfrac2+=xfracstep2*MapZoom;
      x1=xfrac2>>FRACBITS;
      yfrac2+=yfracstep2*MapZoom;
      y1=yfrac2>>FRACBITS;
      }
   yfrac+=yfracstep*MapZoom;
   y=yfrac>>FRACBITS;
   xfrac+=xfracstep*MapZoom;
   x=xfrac>>FRACBITS;
   }
 *(hudylookup[hudHeight/2+1]+hudWidth/2+1)=40;
 }


void displayheatmode(void)
/* display overhead heat sensor */
{
 int i, j, c;

 for(i=0;i<64;i++)
  for(j=0;j<64;j++)
   if (reallight[i*64+j])
    {
     c=-reallight[i*64+j]/48;
     if (c>15) c=15;
      else if (c<0) c=0;
     *(hudylookup[i+21]+j+3)=88-c;
     }
 memset(hudylookup[20]+2,73,66);
 memset(hudylookup[85]+2,73,66);
 for(i=21;i<85;i++)
  {
   *(hudylookup[i]+2)=73;
   *(hudylookup[i]+67)=73;
   }
 *(hudylookup[(player.y>>FRACTILESHIFT)+21]+(player.x>>FRACTILESHIFT)+3)=40;
 if (BonusItem.score)
  *(hudylookup[BonusItem.tiley + 21] + BonusItem.tilex + 3)=44;
 if (exitexists)
  *(hudylookup[exity + 21] + exitx + 3)=187;
 }


void displayheatmapmode(void)
/* display heat traces on the overhead map */
{
 int i, j, ofsx, ofsy, x, y, px, py, mapy, mapx, c, a, miny, maxy, minx, maxx;
 int mapspot, b;

 y=hudHeight/4;
 miny=-(y/2);
 maxy=y/2;
 x=hudWidth/4;
 minx=-(x/2);
 maxx=x/2;
 ofsx=1-((player.x>>(FRACBITS+4))&3);
 ofsy=1-((player.y>>(FRACBITS+4))&3);
 px=player.x>>FRACTILESHIFT;
 py=player.y>>FRACTILESHIFT;
 y=ofsy;
 for(i=miny;i<=maxy;i++,y+=4)
  {
   mapy=py+i;
   if (mapy<0 || mapy>=MAPROWS) continue;
   mapx=px+minx;
   mapspot=mapy*MAPCOLS+mapx;
   x=ofsx;
   for(j=minx;j<=maxx;j++,mapspot++,mapx++,x+=4)
    if (mapx>=0 && mapx<MAPCOLS && reallight[mapspot])
     {
      c=-reallight[mapspot]/48;
      if (c>15) c=15;
       else if (c<0) c=0;
      c=88-c;
      for(a=0;a<4;a++)
       if (y+a<hudHeight && y+a>=0)
	for(b=0;b<4;b++)
	 if (x+b<hudWidth && x+b>=0) *(hudylookup[y+a]+b+x)=c;
      }
    }
 }


void displaymotionmode(void)
/* display sensors on overhead display */
{
 scaleobj_t *sp;
 int        sx, sy, i;

 for(sp=firstscaleobj.next;sp!=&lastscaleobj;sp=sp->next)
  if (sp->active && sp->hitpoints)
   {
    sx=sp->x>>FRACTILESHIFT;
    sy=sp->y>>FRACTILESHIFT;
    *(hudylookup[sy+21]+sx+3)=152;
    }
 memset(hudylookup[20]+2,73,66);
 memset(hudylookup[85]+2,73,66);
 for(i=21;i<85;i++)
  {
   *(hudylookup[i]+2)=73;
   *(hudylookup[i]+67)=73;
   }
 *(hudylookup[(player.y>>FRACTILESHIFT)+21]+(player.x>>FRACTILESHIFT)+3)=40;
 if (BonusItem.score)
  *(hudylookup[BonusItem.tiley + 21] + BonusItem.tilex + 3)=44;
 if (exitexists)
  *(hudylookup[exity + 21] + exitx + 3)=187;
 }


void displaymotionmapmode(void)
/* display sensors on overhead map */
{
 int        ofsx, ofsy, x, y, a, b, px, py;
 scaleobj_t *sp;

 ofsx=1-((player.x>>(FRACBITS+4))&3)+hudWidth/2;
 ofsy=1-((player.y>>(FRACBITS+4))&3)+hudHeight/2;
 px=player.x>>FRACTILESHIFT;
 py=player.y>>FRACTILESHIFT;
 for(sp=firstscaleobj.next;sp!=&lastscaleobj;sp=sp->next)
  if (sp->active && sp->hitpoints)
   {
    x=((((sp->x)>>FRACTILESHIFT)-px)<<2)+ofsx;
    y=((((sp->y)>>FRACTILESHIFT)-py)<<2)+ofsy;
    for(a=0;a<4;a++)
     if (y+a<hudHeight && y+a>=0)
      for(b=0;b<4;b++)
       if (x+b<hudWidth && x+b>=0) *(hudylookup[y+a]+b+x)=152;
    }
 }

