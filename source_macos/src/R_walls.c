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

#include <MATH.H>
#include <STRING.H>
#include "d_global.h"
#include "r_refdef.h"
#include "d_disk.h"
#include "d_video.h"
#include "d_misc.h"


/**** VARIABLES ****/

fixed_t tangents[TANANGLES *2];
fixed_t sines[TANANGLES *5];
fixed_t *cosines;                       // point 1/4 phase into sines
int     pixelangle[MAX_VIEW_WIDTH+1];
fixed_t pixelcosine[MAX_VIEW_WIDTH+1];
fixed_t wallz[MAX_VIEW_WIDTH];  // pointx
byte    *tpwalls_dest[MAXPEND];
byte    *tpwalls_colormap[MAXPEND];
int     tpwalls_count[MAXPEND];
int     transparentposts;
int     wallpixelangle[MAX_VIEW_WIDTH+1];
fixed_t wallpixelcosine[MAX_VIEW_WIDTH+1];
int     campixelangle[MAX_VIEW_WIDTH+1];
fixed_t campixelcosine[MAX_VIEW_WIDTH+1];


/**** FUNCTIONS ****/

void InitWalls(void)
{
 int     intval, i;
 // calculate the angle deltas for each view post
 // VIEWWIDTH view posts covers TANANGLES angles
 // traces go through the RIGHT EDGE of the pixel to follow the direction
 for (i=0;i<windowWidth+1; i++)
  {
   intval=rint(atan(((double)CENTERX-((double)i+1.0))/(double)CENTERX)/(double)PI*(double)TANANGLES*(double)2);
   pixelangle[i]=intval;
   pixelcosine[i]=cosines[intval&(TANANGLES * 4 - 1)];
   }
 memcpy(wallpixelangle,pixelangle,sizeof(pixelangle));
 memcpy(wallpixelcosine,pixelcosine,sizeof(pixelcosine));
 }


void DrawWall(int x1,int x2)
/* Draws the wall on side from p1->px to p2->px-1 with wall picture wall
   p1/p2 are projected and Z clipped, but unclipped to the view window */
{
 int      baseangle;
 byte     **postindex;    // start of the 64 entry texture table for t
 fixed_t  distance;       // horizontal / vertical dist to wall segmen
 fixed_t  pointz;         // transformed distance to wall post
 fixed_t  anglecos;
 fixed_t  textureadjust;  // the amount the texture p1ane is shifted
 fixed_t  ceiling;        // top of the wall
 fixed_t  floor;          // bottom of the wall
 fixed_t  top, bottom;    // precise y coordinates for post
 fixed_t  scale;
 int      topy, bottomy;  // pixel y coordinates for post
 fixed_t  fracadjust;     // the amount to prestep for the top pixel
 int      angle;          // the ray angle that strikes the current po
 int      texture;        // 0-63 post number
 int      x;      // collumn and ranges
 int      light;
 short    *wall;
 unsigned span;
 span_t   *span_p;
 int      rotateright, rotateleft, transparent, rotateup, rotatedown, invisible;

 walltype=walltranslation[walltype];     // global animation
 wall=lumpmain[walllump+walltype];       // to get wall height
 postindex=wallposts+((walltype-1)<<6);  // 64 pointers to texture start
 baseangle=viewfineangle;
 transparent=wallflags & F_TRANSPARENT;
 floor=floorheight[mapspot];
 ceiling=ceilingheight[mapspot];
 switch (side)
  {
   case 0:                         // south facing wall
    distance=viewy-(tiley<<FRACTILESHIFT);
    textureadjust=viewx;
    baseangle+=TANANGLES *2;
    if (transparent) player.northmap[mapspot]=TRANS_COLOR;
     else player.northmap[mapspot]=WALL_COLOR;
    if (mapflags[mapspot]&(FL_CEILING+FL_FLOOR))
     {
      if (floorheight[mapspot+1]<floor) floor=floorheight[mapspot+1];
      if (ceilingheight[mapspot+1]>ceiling) ceiling=ceilingheight[mapspot+1];
      }
    break;
   case 1:                         // west facing wall
    distance=((tilex+1)<<FRACTILESHIFT)-viewx;
    textureadjust=viewy;
    baseangle+=TANANGLES;
    if (transparent) player.westmap[mapspot+1]=TRANS_COLOR;
     else player.westmap[mapspot+1]=WALL_COLOR;
    if (mapflags[mapspot]&(FL_CEILING+FL_FLOOR))
     {
      if (floorheight[mapspot+MAPCOLS+1]<floor) floor=floorheight[mapspot+MAPCOLS+1];
      if (ceilingheight[mapspot+MAPCOLS+1]>ceiling) ceiling=ceilingheight[mapspot+MAPCOLS+1];
      }
    break;
   case 2:                         // north facing wall
    distance=((tiley+1)<<FRACTILESHIFT)-viewy;
    textureadjust=-viewx;
    baseangle+=TANANGLES *2;
    if (transparent) player.northmap[mapspot+MAPCOLS]=TRANS_COLOR;
     else player.northmap[mapspot+MAPCOLS]=WALL_COLOR;
    if (mapflags[mapspot]&(FL_CEILING+FL_FLOOR))
     {
      if (floorheight[mapspot+MAPCOLS+1]<floor) floor=floorheight[mapspot+MAPCOLS+1];
      if (ceilingheight[mapspot+MAPCOLS+1]>ceiling) ceiling=ceilingheight[mapspot+MAPCOLS+1];
      }
    break;
   case 3:                         // east facing wall
    distance=viewx-(tilex<<FRACTILESHIFT);
    textureadjust=-viewy;
    baseangle+=TANANGLES;
    if (transparent) player.westmap[mapspot]=TRANS_COLOR;
     else player.westmap[mapspot]=WALL_COLOR;
    if (mapflags[mapspot]&(FL_CEILING+FL_FLOOR))
     {
      if (floorheight[mapspot+MAPCOLS]<floor) floor=floorheight[mapspot+MAPCOLS];
      if (ceilingheight[mapspot+MAPCOLS]>ceiling) ceiling=ceilingheight[mapspot+MAPCOLS];
      }
   }
 // the floor and ceiling height is the max of the points
 ceiling=(ceiling<<FRACBITS)-viewz;
 floor=-((floor<<FRACBITS)-viewz);   // distance below vi
 sp_loopvalue=(*wall * 4)<<FRACBITS;

  /* special effects */
 if (wallshadow==1) sp_colormap=colormaps+(wallglow<<8);
  else if (wallshadow==2) sp_colormap=colormaps+(wallflicker1<<8);
  else if (wallshadow==3) sp_colormap=colormaps+(wallflicker2<<8);
  else if (wallshadow==4) sp_colormap=colormaps+(wallflicker3<<8);
  else if (wallshadow>=5 && wallshadow<=8)
   {
    if (wallcycle==wallshadow-5) sp_colormap=colormaps;
    else
     {
      light=(pointz>>FRACBITS)+maplight;
      if (light>MAXZLIGHT) light=MAXZLIGHT;
       else if (light<0) light=0;
      sp_colormap=zcolormap[light];
      }
    }

 rotateleft=wallflags & F_LEFT;
 rotateright=wallflags & F_RIGHT;
 rotateup=wallflags & F_UP;
 rotatedown=wallflags & F_DOWN;
 invisible=wallflags & F_DAMAGE;

 // step through the individual posts
 for (x=x1; x<=x2; x++)
  {
    // first do the z clipping
   angle=baseangle+pixelangle[x];
   angle&=TANANGLES *2-1;
   anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
   pointz=FIXEDDIV(distance, anglecos);
   pointz=FIXEDMUL(pointz, pixelcosine[x]);
   if (pointz>MAXZ)
    return;
   if (pointz<MINZ)
    continue;

   /* wall special effects */
   if (wallshadow==0)
    {
     light=(pointz>>FRACBITS)+maplight;
     if (light>MAXZLIGHT) light=MAXZLIGHT;
      else if (light<0) light=0;
     sp_colormap=zcolormap[light];
     }
   else if (wallshadow==9)
    {
     light=(pointz>>FRACBITS)+maplight+wallflicker4;
     if (light>MAXZLIGHT) light=MAXZLIGHT;
      else if (light<0) light=0;
     sp_colormap=zcolormap[light];
     }

   // calculate the texture post along the wall that was hit
   texture=(textureadjust+FIXEDMUL(distance,tangents[angle]))>>FRACBITS;

   if (rotateright) texture-=wallrotate;
    else if (rotateleft) texture+=wallrotate;
    else if (x==x1 && x!=0) texture=0; // fix the incorrect looping problem
   texture&=63;

   sp_source=postindex[texture];
   if (!transparent)
    wallz[x]=pointz;

   // calculate the size and scale of the post
   sp_fracstep=FIXEDMUL(pointz,ISCALE);
   scale=sp_fracstep;
   if (scale<1000) continue;
   top=FIXEDDIV(ceiling,scale)+FRACUNIT;
   topy=CENTERY - (top>>FRACBITS);
   fracadjust=top & (FRACUNIT-1);
   sp_frac=FIXEDMUL(fracadjust,sp_fracstep);

   if (rotatedown)
    sp_frac+=FRACUNIT*(63-wallrotate);
   else if (rotateup)
    sp_frac+=FRACUNIT*wallrotate;

   if (topy<scrollmin)
    {
     sp_frac+=(scrollmin-topy)*scale;
     while (sp_frac>=sp_loopvalue) sp_frac-=sp_loopvalue;
     topy=scrollmin;
     }
   bottom=FIXEDDIV(floor,scale)+FRACUNIT*2;
   bottomy=bottom>=((CENTERY+scrollmin)<<FRACBITS) ?
    scrollmax-1: CENTERY+(bottom>>FRACBITS);
   if (bottomy<scrollmin || topy>=scrollmax || topy==bottomy) continue;
   sp_count=bottomy-topy+1;

   sp_dest=viewylookup[bottomy-scrollmin]+x;
   if (transparent)
    {
     span=(pointz<<ZTOFRAC)&ZMASK;
     spansx[numspans]=x;
     span|=numspans;
     spantags[numspans]=span;
     span_p=&spans[numspans];
     if (invisible)
      span_p->spantype=sp_inviswall;
     else
      span_p->spantype=sp_transparentwall;
     span_p->picture=sp_source;
     span_p->y=sp_frac;           // store info in span structure
     span_p->yh=sp_fracstep;
     span_p->x2=transparentposts; // post index
     span_p->light=(*wall*4);
     numspans++;
     tpwalls_dest[transparentposts]=sp_dest;
     tpwalls_colormap[transparentposts]=sp_colormap;
     tpwalls_count[transparentposts]=sp_count;
     transparentposts++;
#ifdef VALIDATE
     if (transparentposts>=MAXPEND) MS_Error("Too many Pending Posts! (%i>=%i)",transparentposts,MAXPEND);
     if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, Walls (%i>=%i)",numspans,MAXSPANS);
#endif
     }
    else ScalePost();
   }
 }


void DrawSteps(int x1, int x2)
{
 int      baseangle;
 byte     **postindex1,**postindex2; // start of the 64 entry texture table for t
 fixed_t  distance;       // horizontal / vertical dist to wall segmen
 fixed_t  pointz;         // transformed distance to wall post
 fixed_t  anglecos;
 fixed_t  textureadjust;  // the amount the texture p1ane is shifted
 fixed_t  ceiling1, ceiling2; // top of the wall
 fixed_t  floor1,floor2;     // bottom of the wall
 fixed_t  top, bottom;    // precise y coordinates for post
 fixed_t  scale;
 fixed_t  cclip1;
 int      topy, bottomy;  // pixel y coordinates for post
 fixed_t  fracadjust;     // the amount to prestep for the top pixel
 int      angle;          // the ray angle that strikes the current po
 int      texture, texture2; // 0-63 post number
 int      x;      // collumn and ranges
 int      light;
 short    *wall1, *wall2;
 unsigned span;
 span_t   *span_p;
 int      walltype1, walltype2, c, rotateright1, rotateright2;
 int      rotateleft1, rotateleft2, tm;
 int      rotateup1, rotateup2, rotatedown1, rotatedown2;
 bool  floor, ceiling;

 floor=false;
 ceiling=false;
 if (mapflags[mapspot]&FL_FLOOR) goto ceilingstep;
 baseangle=viewfineangle;
 switch (side)
  {
   case 0:                         // south facing wall
    distance=viewy-(tiley<<FRACTILESHIFT);
    textureadjust=viewx;
    baseangle+=TANANGLES *2;
    tm=mapspot-MAPCOLS;
    break;
   case 1:                         // west facing wall
    distance=((tilex+1)<<FRACTILESHIFT)-viewx;
    textureadjust=viewy;
    baseangle+=TANANGLES;
    tm=mapspot+1;
    break;
   case 2:                         // north facing wall
    distance=((tiley+1)<<FRACTILESHIFT)-viewy;
    textureadjust=-viewx;
    baseangle+=TANANGLES *2;
    tm=mapspot+MAPCOLS;
    break;
   case 3:                         // east facing wall
    distance=viewx-(tilex<<FRACTILESHIFT);
    textureadjust=-viewy;
    baseangle+=TANANGLES;
    tm=mapspot-1;
    break;
   }
 ceiling1=floorheight[tm];
 floor1=floorheight[mapspot];

 if (ceiling1<=floor1) goto ceilingstep;
 if (ceiling1>=ceilingheight[mapspot]) walltype=1; // clip beyond this tile

 floor=true;
 walltype1=floordef[tm];
 rotateright1=floordefflags[tm] & F_RIGHT;
 rotateleft1=floordefflags[tm] & F_LEFT;
 rotateup1=floordefflags[tm] & F_UP;
 rotatedown1=floordefflags[tm] & F_DOWN;
 cclip1=ceiling1;
 ceiling1=(ceiling1<<FRACBITS)-viewz;
 floor1=-((floor1<<FRACBITS)-viewz);       // distance below vi
 walltype1=walltranslation[walltype1];     // global animation
 wall1=lumpmain[walllump+walltype1];       // to get wall height
 postindex1=wallposts+((walltype1-1)<<6);  // 64 pointers to texture start

ceilingstep:

 if (mapflags[mapspot]&FL_CEILING)
  {
   if (!floor) return;
   goto skipceilingcalc;
   }
 switch (side)
  {
   case 0:                         // south facing wall
    tm=mapspot-MAPSIZE;
    break;
   case 1:                         // west facing wall
    tm=mapspot+1;
    break;
   case 2:                         // north facing wall
    tm=mapspot+MAPSIZE;
    break;
   case 3:                         // east facing wall
    tm=mapspot-1;
    break;
   }
 floor2=ceilingheight[tm];
 ceiling2=ceilingheight[mapspot];

 if (ceiling2<=floor2)
  {
   if (!floor) return;
   goto skipceilingcalc;
   }

 if (floor2<=floorheight[mapspot]) walltype=1; // clip beyond this tile
 if (floor && cclip1>=floor2) walltype=1;

 ceiling=true;
 ceiling2=(ceiling2<<FRACBITS)-viewz;
 floor2=-((floor2<<FRACBITS)-viewz);   // distance below vi
 walltype2=ceilingdef[tm];
 walltype2=walltranslation[walltype2];     // global animation
 wall2=lumpmain[walllump+walltype2];       // to get wall height
 postindex2=wallposts+((walltype2-1)<<6);  // 64 pointers to texture start
 rotateleft2=ceilingdefflags[tm] & F_LEFT;
 rotateright2=ceilingdefflags[tm] & F_RIGHT;
 rotateup2=ceilingdefflags[tm] & F_UP;
 rotatedown2=ceilingdefflags[tm] & F_DOWN;

skipceilingcalc:

 if (wallshadow==1) sp_colormap=colormaps+(wallglow<<8);
  else if (wallshadow==2) sp_colormap=colormaps+(wallflicker1<<8);
  else if (wallshadow==3) sp_colormap=colormaps+(wallflicker2<<8);
  else if (wallshadow==4) sp_colormap=colormaps+(wallflicker3<<8);

 // step through the individual posts
 for (x=x1; x<=x2; x++)
  {
    // first do the z clipping
   angle=baseangle+pixelangle[x];
   angle&=TANANGLES *2-1;
   anglecos=cosines[(angle-TANANGLES)&(TANANGLES *4-1)];
   pointz=FIXEDDIV(distance, anglecos);
   pointz=FIXEDMUL(pointz, pixelcosine[x]);
   if (pointz>MAXZ || pointz<MINZ) continue;

   /* wall special effects */
   if (wallshadow==0)
    {
     light=(pointz>>FRACBITS)+maplight;
     if (light>MAXZLIGHT) light=MAXZLIGHT;
      else if (light<0) light=0;
     sp_colormap=zcolormap[light];
     }
   else if (wallshadow>=5 && wallshadow<=8)
    {
     if (wallcycle==wallshadow-5) sp_colormap=colormaps;
     else
      {
       light=(pointz>>FRACBITS)+maplight;
       if (light>MAXZLIGHT) light=MAXZLIGHT;
	else if (light<0) light=0;
       sp_colormap=zcolormap[light];
       }
     }
   else if (wallshadow==9)
    {
     light=(pointz>>FRACBITS)+maplight+wallflicker4;
     if (light>MAXZLIGHT) light=MAXZLIGHT;
      else if (light<0) light=0;
     sp_colormap=zcolormap[light];
     }

   texture=(textureadjust+FIXEDMUL(distance,tangents[angle]))>>FRACBITS;

   scale=FIXEDMUL(pointz,ISCALE);

   if (scale<1000) continue;

   sp_fracstep=scale;

  /*=======================================*/
   if (floor)
    {
     texture2=texture;
     if (rotateright1) texture2-=wallrotate;
      else if (rotateleft1) texture2+=wallrotate;
      else if (x==x1 && x!=0) texture2=0; // fix the incorrect looping problem
     texture2&=63;
     sp_source=postindex1[texture2];
     top=FIXEDDIV(ceiling1,scale);
     topy=CENTERY - (top>>FRACBITS);
     fracadjust=top & (FRACUNIT-1);
     sp_frac=FIXEDMUL(fracadjust,sp_fracstep);

     if (topy<scrollmin)
      {
       sp_frac+=(scrollmin-topy)*scale;
       sp_loopvalue=(*wall1 * 4)<<FRACBITS;
       while (sp_frac>=sp_loopvalue) sp_frac-=sp_loopvalue;
       topy=scrollmin;
       }
     if (rotatedown1)
      sp_frac+=FRACUNIT*(63-wallrotate);
     else if (rotateup1)
      sp_frac+=FRACUNIT*wallrotate;

     bottom=FIXEDDIV(floor1,scale)+FRACUNIT;
     bottomy=bottom>=((CENTERY+scrollmin)<<FRACBITS) ?
      scrollmax-1: CENTERY+(bottom>>FRACBITS);
     if ((bottomy<scrollmin)||(topy>=scrollmax)) goto contceiling;
     sp_count=bottomy-topy+1;
     sp_dest=viewylookup[bottomy-scrollmin]+x;
     span=(pointz<<ZTOFRAC)&ZMASK;
     spansx[numspans]=x;
     span|=numspans;
     spantags[numspans]=span;
     span_p=&spans[numspans];
     span_p->spantype=sp_step;
     span_p->picture=sp_source;
     span_p->y=sp_frac;           // store info in span structure
     span_p->yh=sp_fracstep;
     span_p->x2=transparentposts; // post index
     span_p->light=(*wall1 * 4);
     numspans++;
     tpwalls_dest[transparentposts]=sp_dest;
     tpwalls_colormap[transparentposts]=sp_colormap;
     tpwalls_count[transparentposts]=sp_count;
     transparentposts++;
  #ifdef VALIDATE
     if (transparentposts>=MAXPEND) MS_Error("Too many Pending Posts! (%i>=%i)",transparentposts,MAXPEND);
     if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, FloorDefs (%i>=%i)",numspans,MAXSPANS);
  #endif
     }

contceiling:
  /*=======================================*/
   if (ceiling)
    {
     texture2=texture;
     if (rotateright2) texture2-=wallrotate;
      else if (rotateleft2) texture2+=wallrotate;
      else if (x==x1 && x!=0) texture2=0; // fix the incorrect looping problem
     texture2&=63;
     sp_source=postindex2[texture2];
     top=FIXEDDIV(ceiling2,scale)+FRACUNIT;
     topy=CENTERY - (top>>FRACBITS);
     fracadjust=top & (FRACUNIT-1);
     sp_frac=FIXEDMUL(fracadjust,sp_fracstep);

     if (topy<scrollmin)
      {
       sp_frac+=(scrollmin-topy)*scale;
       sp_loopvalue=(*wall2 * 4)<<FRACBITS;
       while (sp_frac>=sp_loopvalue) sp_frac-=sp_loopvalue;
       topy=scrollmin;
       }
     if (rotatedown2)
      sp_frac+=FRACUNIT*(63-wallrotate);
     else if (rotateup2)
      sp_frac+=FRACUNIT*wallrotate;

     bottom=FIXEDDIV(floor2,scale)+FRACUNIT;
     bottomy=bottom>=((CENTERY+scrollmin)<<FRACBITS) ?
      scrollmax-1: CENTERY+(bottom>>FRACBITS);
     if (bottomy<scrollmin|| topy>=scrollmax) continue;
     sp_count=bottomy-topy+1;
     sp_dest=viewylookup[bottomy-scrollmin]+x;
     span=(pointz<<ZTOFRAC)&ZMASK;
     spansx[numspans]=x;
     span|=numspans;
     spantags[numspans]=span;
     span_p=&spans[numspans];
     span_p->spantype=sp_step;
     span_p->picture=sp_source;
     span_p->y=sp_frac;           // store info in span structure
     span_p->yh=sp_fracstep;
     span_p->x2=transparentposts; // post index
     span_p->light=(*wall2 * 4);  // loop value
     numspans++;
     tpwalls_dest[transparentposts]=sp_dest;
     tpwalls_colormap[transparentposts]=sp_colormap;
     tpwalls_count[transparentposts]=sp_count;
     transparentposts++;
  #ifdef VALIDATE
     if (transparentposts>=MAXPEND) MS_Error("Too many Pending Posts! (%i>=%i)",transparentposts,MAXPEND);
     if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, CeilingDefs (%i>=%i)",numspans,MAXSPANS);
  #endif
     }
   }

 if (floor)
  {
   if (walltype) c=WALL_COLOR;
    else c=STEP_COLOR;
   switch(side)
    {
     case 0:
      player.northmap[mapspot]=c;
      break;
     case 1:
      player.westmap[mapspot+1]=c;
      break;
     case 2:
      player.northmap[mapspot+MAPCOLS]=c;
      break;
     case 3:
      player.westmap[mapspot]=c;
     }
   }
 }
