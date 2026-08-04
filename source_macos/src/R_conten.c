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

#include <STDLIB.H>
#include "d_global.h"
#include "d_disk.h"
#include "r_refdef.h"
#include "d_misc.h"
#include "d_ints.h"


/**** VARIABLES ****/

scaleobj_t  firstscaleobj, lastscaleobj; // just placeholders for links
scaleobj_t  scaleobjlist[MAXSPRITES], *freescaleobj_p;
doorobj_t   doorlist[MAXDOORS];
int         numdoors;
elevobj_t   firstelevobj, lastelevobj;
elevobj_t   elevlist[MAXELEVATORS], *freeelevobj_p;
int         numelev;
int         doorxl, doorxh;
spawnarea_t spawnareas[MAXSPAWNAREAS];
int         numspawnareas, rtimecount;


/**** FUNCTIONS ****/

void DrawDoor(void);

vertex_t *TransformPoint(fixed_t x, fixed_t y)
/* returns vertex pointer of transformed vertex */
{
 fixed_t  trx, try;
 fixed_t  scale;
 vertex_t *point;

 point=vertexlist_p++;
#ifdef VALIDATE
 if (point>=&vertexlist[MAXVISVERTEXES]) MS_Error("TransformPoint: Vertexlist overflow");
#endif
 trx=x-viewx;
 try=y-viewy;
 point->tx=FIXEDMUL(try, viewcos)+FIXEDMUL(trx, viewsin);
 point->tz=FIXEDMUL(trx, viewcos)-FIXEDMUL(try, viewsin);
 if (point->tz>=MINZ)
  {
   scale=FIXEDDIV(SCALE,point->tz);
   point->px=CENTERX+(FIXEDMUL(point->tx, scale)>>FRACBITS);
   }
 return point;
 }


bool ClipDoor(void)
/* Sets p1->px and p2->px correctly for Z values < MINZ
   Returns false if entire door is too close or far away */
{
 fixed_t frac, clip;

 if ((p1->tz>MAXZ && p2->tz>MAXZ) ||       // entire face is too far away
     (p1->tz<=0 && p2->tz<=0)) return false; // totally behind the projection plane
 if (p1->tz<MINZ)
  {
   if (p1->tz==0) clip=p1->tx;
   else
    {
     if (p2->tz==p1->tz) return false;
     frac=FIXEDDIV(p2->tz, (p2->tz-p1->tz));
     clip=p2->tx+FIXEDMUL((p1->tx-p2->tx), frac);
     }
   p1->px=clip<0?0:windowWidth;
   }
 else if (p2->tz<MINZ)
  {
   if (p2->tz==0) clip=p2->tx;
   else
    {
     if (p2->tz==p1->tz) return false;
     frac=FIXEDDIV(p1->tz, (p1->tz-p2->tz));
     clip=p1->tx+FIXEDMUL((p2->tx-p1->tx), frac);
     }
   p2->px=clip<0?0:windowWidth;
   }
 return true;
 }


void RenderDoor(void)
/*  Posts one pixel wide span events for each visible post of the door a
    tilex / tiley / xclipl / xcliph
    sets doorxl, doorxh based on the position of the door.  One of the t
    in the tile bounds, the other will be off the edge of the view.  The
    restrict the flowing into other tiles bounds. */
{
 doorobj_t *door_p, *last_p;
 fixed_t   tx, ty;
 byte      **postindex;    // start of the 64 entry texture table for t
 fixed_t   pointz;         // transformed distance to wall post
 fixed_t   anglecos;
 fixed_t   ceilingheight;  // top of the wall
 fixed_t   floorh;         // bottom of the wall
 int       angle;          // the ray angle that strikes the current post
 int       texture;        // 0-63 post number
 int       x, x1, x2;      // collumn and ranges
 span_t    *span_p;
 unsigned  span;
 fixed_t   distance, absdistance, position;
 int       baseangle;
 fixed_t   textureadjust;  // the amount the texture p1ane is shifted
 spanobj_t spantype;
 short     *wall;
 vertex_t  *p3;

 // scan the doorlist for matching tilex/tiley
 // this only happens a couple times / frame max, so it's not a big deal
 last_p=&doorlist[numdoors];
 for (door_p=doorlist;;door_p++)
  {
   if (door_p->transparent)
    MS_Error("Door transparent");
   if (door_p->tilex==tilex && door_p->tiley==tiley) break;
   }
 // transform both endpoints of the door
 // p1 is the anchored point, p2 is the moveable point
 tx=tilex<<(TILESHIFT+FRACBITS);
 ty=tiley<<(TILESHIFT+FRACBITS);
 position=door_p->position;
 switch (door_p->orientation)
  {
   case dr_horizontal:
    ty+=FRACUNIT *27;
    p1=TransformPoint(tx+position, ty);
    p2=TransformPoint(tx, ty);
    textureadjust=viewx+TILEGLOBAL - (tx+position);
    baseangle=TANANGLES*2;
    distance=viewy-ty;
    if (!player.northmap[mapspot]) player.northmap[mapspot]=DOOR_COLOR;
    break;
   case dr_vertical:
    tx+=FRACUNIT *27;
    p1=TransformPoint(tx, ty+position);
    p2=TransformPoint(tx, ty);
    textureadjust=viewy+TILEGLOBAL - (ty+position);
    baseangle=TANANGLES;
    distance=tx-viewx;
    if (!player.westmap[mapspot]) player.westmap[mapspot]=DOOR_COLOR;
    break;
   case dr_horizontal2:
    tx+=TILEGLOBAL;
    ty+=FRACUNIT*27;
    p1=TransformPoint(tx-position, ty);
    p2=TransformPoint(tx, ty);
    textureadjust=viewx+TILEGLOBAL - (tx-position);
    baseangle=TANANGLES*2;
    distance=viewy-ty;
    if (!player.northmap[mapspot]) player.northmap[mapspot]=DOOR_COLOR;
    break;
   case dr_vertical2:
    tx+=FRACUNIT*27;
    ty+=TILEGLOBAL;
    p1=TransformPoint(tx, ty-position);
    p2=TransformPoint(tx, ty);
    textureadjust=viewy+TILEGLOBAL - (ty-position);
    baseangle=TANANGLES;
    distance=tx-viewx;
    if (!player.westmap[mapspot]) player.westmap[mapspot]=DOOR_COLOR;
    break;
   }

 if (p1->px>p2->px)
  {
   p3=p1;
   p1=p2;
   p2=p3;
   }
 if (!door_p->position || !ClipDoor()) goto part2;
 x1=p1->px;
 x2=p2->px;

 // calculate the textures to post into the span list
 if (x1<xclipl) x1=xclipl;
 if (x2>xcliph+1) x2=xcliph+1;
 if (x1>=x2) goto part2; // totally clipped off side
 // set up for loop
 if (door_p->transparent)
  {
   spantype=sp_maskeddoor;
   doortile = false;
   }
  else spantype=sp_door;
 walltype=door_p->pic;
 walltype=walltranslation[walltype];     // global animation
 walltype--;                             // make 0 based
 wall=lumpmain[walllump+walltype];
 ceilingheight=vertex[0]->ceilingheight;
 floorh=-vertex[0]->floorheight;
 postindex=wallposts+(walltype<<6);      // 64 pointers to texture starts
 baseangle+=viewfineangle;
 absdistance=distance<0?-distance : distance;
 // step through the individual posts
 for (x=x1; x<x2; x++)
  {
   angle=baseangle+pixelangle[x];
   angle&=TANANGLES *2-1;
   // the z distance of the post hit = walldistance*cos(screenangle
   anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
   if (anglecos<8000)
    continue;
   pointz=FIXEDDIV(absdistance, anglecos);
   pointz=FIXEDMUL(pointz, pixelcosine[x]);
   if (pointz>MAXZ || pointz<MINZ) continue;

   // calculate the texture post along the wall that was hit
   texture=(textureadjust+FIXEDMUL(distance, tangents[angle]))>>FRACBITS;
   texture&=63;
   sp_source=postindex[texture];

   // post the span in the draw list
   span=(pointz<<ZTOFRAC)&ZMASK;
   spansx[numspans]=x;
   span|=numspans;
   spantags[numspans]=span;
   span_p=&spans[numspans];
   span_p->spantype=spantype;
   span_p->picture=sp_source;
   span_p->y=ceilingheight;
   span_p->yh=floorh;
   span_p->structure=door_p;
   span_p->light=maplight;
   span_p->shadow=wallshadow;

   numspans++;
#ifdef VALIDATE
   if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, RenderDoor (%i>=%i)",numspans,MAXSPANS);
#endif
   }

part2:
 tx=tilex<<(TILESHIFT+FRACBITS);
 ty=tiley<<(TILESHIFT+FRACBITS);
 position=door_p->position;
 switch (door_p->orientation)
  {
   case dr_horizontal:
    ty+=FRACUNIT*37;
    p1=TransformPoint(tx+position, ty);
    p2=TransformPoint(tx, ty);
    textureadjust=viewx+TILEGLOBAL - (tx+position);
    baseangle=TANANGLES*2;
    distance=viewy-ty;
    if (!player.northmap[mapspot]) player.northmap[mapspot]=DOOR_COLOR;
    break;
   case dr_vertical:
    tx+=FRACUNIT*37;
    p1=TransformPoint(tx, ty+position);
    p2=TransformPoint(tx, ty);
    textureadjust=viewy+TILEGLOBAL - (ty+position);
    baseangle=TANANGLES;
    distance=tx-viewx;
    if (!player.westmap[mapspot]) player.westmap[mapspot]=DOOR_COLOR;
    break;
   case dr_horizontal2:
    tx+=TILEGLOBAL;
    ty+=FRACUNIT*37;
    p1=TransformPoint(tx-position, ty);
    p2=TransformPoint(tx, ty);
    textureadjust=viewx+TILEGLOBAL - (tx-position);
    baseangle=TANANGLES*2;
    distance=viewy-ty;
    if (!player.northmap[mapspot]) player.northmap[mapspot]=DOOR_COLOR;
    break;
   case dr_vertical2:
    tx+=FRACUNIT*37;
    ty+=TILEGLOBAL;
    p1=TransformPoint(tx, ty-position);
    p2=TransformPoint(tx, ty);
    textureadjust=viewy+TILEGLOBAL - (ty-position);
    baseangle=TANANGLES;
    distance=tx-viewx;
    if (!player.westmap[mapspot]) player.westmap[mapspot]=DOOR_COLOR;
    break;
   }

 if (p1->px>p2->px)
  {
   p3=p1;
   p1=p2;
   p2=p3;
   }
 if (!door_p->position || !ClipDoor()) goto part3;
 x1=p1->px;
 x2=p2->px;
 if (x1<xclipl) x1=xclipl;
 if (x2>xcliph+1) x2=xcliph+1;
 if (x1>=x2) goto part3;
 // set up for loop
 if (door_p->transparent)
  {
   spantype=sp_maskeddoor;
   doortile = false;
   }
  else spantype=sp_door;
 walltype=door_p->pic;
 walltype=walltranslation[walltype];     // global animation
 walltype--;                             // make 0 based
 wall=lumpmain[walllump+walltype];
 ceilingheight=vertex[0]->ceilingheight;
 floorh=-vertex[0]->floorheight;
 postindex=wallposts+(walltype<<6);      // 64 pointers to texture starts
 baseangle+=viewfineangle;
 absdistance=distance<0?-distance : distance;
 // step through the individual posts
 for (x=x1; x<x2; x++)
  {
   angle=baseangle+pixelangle[x];
   angle&=TANANGLES *2-1;
   // the z distance of the post hit = walldistance*cos(screenangle
   anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
   if (anglecos<8000)
    continue;
   pointz=FIXEDDIV(absdistance, anglecos);
   pointz=FIXEDMUL(pointz, pixelcosine[x]);
   if (pointz>MAXZ) return;
   if (pointz<MINZ) continue;

   // calculate the texture post along the wall that was hit
   texture=(textureadjust+FIXEDMUL(distance, tangents[angle]))>>FRACBITS;
   texture&=63;
   sp_source=postindex[texture];

   // post the span in the draw list
   span=(pointz<<ZTOFRAC)&ZMASK;
   spansx[numspans]=x;
   span|=numspans;
   spantags[numspans]=span;
   span_p=&spans[numspans];
   span_p->spantype=spantype;
   span_p->picture=sp_source;
   span_p->y=ceilingheight;
   span_p->yh=floorh;
   span_p->structure=door_p;
   span_p->light=maplight;
   span_p->shadow=wallshadow;

   numspans++;
#ifdef VALIDATE
   if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, RenderDoor (%i>=%i)",numspans,MAXSPANS);
#endif
   }


part3:
 tx=tilex<<(TILESHIFT+FRACBITS);
 ty=tiley<<(TILESHIFT+FRACBITS);
 switch (door_p->orientation)
  {
   case dr_horizontal:
    ty+=FRACUNIT*32;
    tx+=position;
    p1=TransformPoint(tx, ty+(5<<FRACBITS));
    p2=TransformPoint(tx, ty-(5<<FRACBITS));
    textureadjust=viewy+TILEGLOBAL - ty;
    baseangle=TANANGLES;
    distance=tx-viewx;
    break;
   case dr_vertical:
    tx+=FRACUNIT*32;
    ty+=position;
    p1=TransformPoint(tx+(5<<FRACBITS), ty);
    p2=TransformPoint(tx-(5<<FRACBITS), ty);
    textureadjust=viewx+TILEGLOBAL - tx;
    baseangle=TANANGLES*2;
    distance=viewy-ty;
    break;
   case dr_horizontal2:
    ty+=FRACUNIT*32;
    tx+=FRACUNIT*64-position;
    p1=TransformPoint(tx, ty+(5<<FRACBITS));
    p2=TransformPoint(tx, ty-(5<<FRACBITS));
    textureadjust=viewy+TILEGLOBAL - ty;
    baseangle=TANANGLES;
    distance=tx-viewx;
    break;
   case dr_vertical2:
    tx+=FRACUNIT*32;
    ty+=FRACUNIT*64-position;
    p1=TransformPoint(tx+(5<<FRACBITS), ty);
    p2=TransformPoint(tx-(5<<FRACBITS), ty);
    textureadjust=viewx+TILEGLOBAL - tx;
    baseangle=TANANGLES*2;
    distance=viewy-ty;
    break;
   }
 if (p1->px>p2->px)
  {
   p3=p1;
   p1=p2;
   p2=p3;
   }
 if (!door_p->position || !ClipDoor()) return;
 x1=p1->px;
 x2=p2->px;

 // calculate the textures to post into the span list
 if (x1<xclipl) x1=xclipl;
 if (x2>xcliph+1) x2=xcliph+1;
 if (x1>=x2) return;  // totally clipped off side
 // set up for loop
 walltype=2;
 wall=lumpmain[walllump+walltype];
 postindex=wallposts+(walltype<<6);      // 64 pointers to texture starts
 baseangle+=viewfineangle;
 absdistance=distance<0?-distance : distance;
 // step through the individual posts
 for (x=x1; x<x2; x++)
  {
   angle=baseangle+pixelangle[x];
   angle&=TANANGLES *2-1;
   // the z distance of the post hit = walldistance*cos(screenangle
   anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
   if (anglecos<8000)
    continue;
   pointz=FIXEDDIV(absdistance, anglecos);
   pointz=FIXEDMUL(pointz, pixelcosine[x]);
   if (pointz>MAXZ) return;
   if (pointz<MINZ) continue;

   // calculate the texture post along the wall that was hit
   texture=(textureadjust+FIXEDMUL(distance, tangents[angle]))>>FRACBITS;
   texture&=63;
   sp_source=postindex[texture];

   // post the span in the draw list
   span=(pointz<<ZTOFRAC)&ZMASK;
   spansx[numspans]=x;
   span|=numspans;
   spantags[numspans]=span;
   span_p=&spans[numspans];
   span_p->spantype=spantype;
   span_p->picture=sp_source;
   span_p->y=ceilingheight;
   span_p->yh=floorh;
   span_p->structure=door_p;
   span_p->light=maplight;
   span_p->shadow=wallshadow;

   numspans++;
#ifdef VALIDATE
   if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, RenderDoor (%i>=%i)",numspans,MAXSPANS);
#endif
   }

 }


void RenderSprites()
/*  For each sprite, if the sprite's bounding rect touches a tile with a
    vertex, transform and clip the projected view rect.  If still visible
    a span into the span list */
{
 scaleobj_t *sprite;
 fixed_t    deltax, deltay, pointx, pointz, gxt, gyt;
 int        picnum;
 unsigned   span;
 span_t     *span_p;
 byte       animationGraphic, animationMax, animationDelay;
 int        mapx, mapy, mapspot;

 for (sprite=firstscaleobj.next; sprite!=&lastscaleobj;sprite=sprite->next)
  {
   // calculate which image to display
   picnum=sprite->basepic;
   if (sprite->rotate)
    {   // this is only aproximate, but ok for 8
     if (sprite->rotate == rt_eight)
      picnum+=((viewangle - sprite->angle+WEST+DEGREE45_2)>>7)&7;
     else picnum+=((viewangle - sprite->angle+WEST+DEGREE45)>>8)&3;
     }

   if ((sprite->animation) && (rtimecount>=(int)sprite->animationTime))
    {
     animationGraphic = (sprite->animation & ANIM_CG_MASK) >> 1;
     animationMax = (sprite->animation & ANIM_MG_MASK) >> 5;
     animationDelay = (sprite->animation & ANIM_DELAY_MASK) >> 9;
     if (animationGraphic < animationMax-1) animationGraphic++;
      else if (sprite->animation & ANIM_LOOP_MASK) animationGraphic=0;
      else if (sprite->animation & ANIM_SELFDEST)
       {
	sprite=sprite->prev; /* some sprites exist only to animate and die (like some people ;p) */
	RF_RemoveSprite(sprite->next);
	continue;
	}
     picnum+=animationGraphic;
     sprite->animation = (sprite->animation & ANIM_LOOP_MASK) +
      (animationGraphic << 1) + (animationMax << 5) + (animationDelay << 9) +
      (sprite->animation & ANIM_SELFDEST);
     sprite->animationTime = timecount + animationDelay;
     }
   else if (sprite->animation) picnum+=(sprite->animation & ANIM_CG_MASK)>>1;

   deltax=sprite->x - viewx;
   if (deltax<-MAXZ || deltax>MAXZ) continue;
   deltay=sprite->y - viewy;
   if (deltay<-MAXZ || deltay>MAXZ) continue;

   // transform the point

   gxt=FIXEDMUL(deltax,viewcos);
   gyt=FIXEDMUL(deltay,viewsin);

   pointz=gxt-gyt;

   if (pointz>MAXZ || pointz<FRACUNIT*8) continue;

   // transform the point
   pointx=FIXEDMUL(deltax, viewsin) + FIXEDMUL(deltay, viewcos);
   // post the span event
   span=(pointz<<ZTOFRAC)&ZMASK;
   span|=numspans;
   spantags[numspans]=span;
   span_p=&spans[numspans];
   span_p->spantype=sp_shape;
   span_p->picture=lumpmain[picnum];
   span_p->x2=pointx;
   span_p->y=sprite->z-viewz;
   span_p->structure=sprite;
   mapy=sprite->y>>FRACTILESHIFT;
   mapx=sprite->x>>FRACTILESHIFT;
   mapspot=mapy*MAPCOLS+mapx;
   if (sprite->specialtype==st_noclip) span_p->shadow=(st_noclip<<8);
    else span_p->shadow=(sprite->specialtype<<8)+mapeffects[mapspot];
   if (sprite->specialtype==st_noclip) span_p->light=-1000;
    else span_p->light=(maplights[mapspot]<<2) + reallight[mapspot];
   numspans++;
#ifdef VALIDATE
   if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, RenderSprites (%i>=%i)",numspans,MAXSPANS);
#endif
   }
 }
