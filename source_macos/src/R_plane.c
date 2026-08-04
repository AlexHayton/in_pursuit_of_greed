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
#include "d_disk.h"
#include "r_refdef.h"


/**** VARIABLES ****/

int     mr_y, mr_x1, mr_x2; // used by mapplane to calculate texture end
intptr_t mr_shadow;         // special lighting effect (mode code or colormap ptr)
int     mr_light;
fixed_t mr_deltaheight;
int     flatpic;
bool transparent, ceilingbit;

 // vertexes for drawable polygon
int numvertex;
int vertexy[5];
int vertexx[5];
int spantype;


 // vertexes in need of Z clipping
clippoint_t vertexpt[5];

 // coefficients of the plane equation for sloping polygons
fixed_t planeA, planeB, planeC, planeD;

#define COPYFLOOR(s,d)  \
 vertexpt[d].tx = vertex[s]->tx; \
 vertexpt[d].ty = vertex[s]->floorheight; \
 vertexpt[d].tz = vertex[s]->tz; \
 vertexpt[d].px = vertex[s]->px; \
 vertexpt[d].py = vertex[s]->floory;

#define COPYCEILING(s,d)        \
 vertexpt[d].tx = vertex[s]->tx; \
 vertexpt[d].ty = vertex[s]->ceilingheight; \
 vertexpt[d].tz = vertex[s]->tz; \
 vertexpt[d].px = vertex[s]->px; \
 vertexpt[d].py = vertex[s]->ceilingy;


/**** FUNCTIONS ****/

void FlatSpan(void)
/* used for flat floors and ceilings, coordinates must be pre clipped
   mr_deltaheight is planeheight - viewheight, with height values increased
   mr_picture and mr_deltaheight are set once per polygon */
{
 fixed_t  pointz;    // row's distance to view plane
 span_t   *span_p;
 unsigned span;

 pointz=FIXEDDIV(mr_deltaheight,yslope[mr_y+MAXSCROLL]);
 if (pointz>MAXZ) return;
 // post the span in the draw list
 span=(pointz<<ZTOFRAC)&ZMASK;
 spansx[numspans]=mr_x1;
 span|=numspans;
 spantags[numspans]=span;
 span_p=&spans[numspans];
 span_p->spantype=spantype;
 span_p->picture=mr_picture;
 span_p->x2=mr_x2;
 span_p->y=mr_y;
 span_p->shadow=mr_shadow;
 span_p->light=mr_light;
 numspans++;
#ifdef VALIDATE
 if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, FlatSpan (%i>=%i)",numspans,MAXSPANS);
#endif
 }


void SlopeSpan(void)
/* used for sloping floors and ceilings
   planeA, planeB, planeC, planeD must be precalculated
   mr_picture is set once per polygon */
{
 fixed_t  pointz, pointz2;        // row's distance to view plane
 fixed_t  partial, denom;
 span_t   *span_p;
 unsigned span;

 // calculate the Z values for each end of the span
 partial=FIXEDMUL(planeB,yslope[mr_y+MAXSCROLL])+planeC;
 denom=FIXEDMUL(planeA,xslope[mr_x1])+partial;
 if (denom<8000) return;
 pointz=FIXEDDIV(planeD,denom);
 if (pointz>MAXZ) return;
 denom=FIXEDMUL(planeA,xslope[mr_x2])+partial;
 if (denom<8000) return;
 pointz2=FIXEDDIV(planeD,denom);
 if (pointz2>MAXZ) return;
//  post the span in the draw list
 span=(pointz<<ZTOFRAC)&ZMASK;
 spansx[numspans]=mr_x1;
 span|=numspans;
 spantags[numspans]=span;
 span_p=&spans[numspans];
 span_p->spantype=spantype;
 span_p->picture=mr_picture;
 span_p->x2=mr_x2;
 span_p->y=mr_y;
 span_p->yh=pointz2;
 span_p->shadow=mr_shadow;
 span_p->light=mr_light;
 numspans++;
#ifdef VALIDATE
 if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, SlopeSpan (%i>=%i)",numspans,MAXSPANS);
#endif
 }


void RenderPolygon(void (*spanfunction)(void))
/* Vertex list must be precliped, convex, and in clockwise order
   Backfaces (not in clockwise order) generate no pixels
   The polygon is divided into trapezoids (from 1 to numvertex-1 can be
   which have a constant slope on both sides
   mr_x1                       screen coordinates of the span to draw, use by map
   mr_x2                       plane to calculate textures at the endpoints
   mr_y                        along with mr_deltaheight
   mr_dest                     pointer inside viewbuffer where span starts
   mr_count                    length of span to draw (mr_x2 - mr_x1)
   spanfunction is a pointer to a function that will handle determining
   in the calculated span (FlatSpan or SlopeSpan) */
{
 int     stopy;
 fixed_t leftfrac, rightfrac;
 fixed_t leftstep, rightstep;
 int     leftvertex, rightvertex;
 int     deltax, deltay;
 int     oldx;

 // find topmost vertex
 rightvertex=0;                  // topmost so far
 for (leftvertex=1; leftvertex<numvertex; leftvertex++)
  if (vertexy[leftvertex]<vertexy[rightvertex]) rightvertex=leftvertex;
 // ride down the left and right edges
 mr_y=vertexy[rightvertex];
 leftvertex=rightvertex;
 if (mr_y>=scrollmax) return;   // totally off bottom
 do
  {
   if (mr_y==vertexy[rightvertex])
    {
skiprightvertex:
     oldx=vertexx[rightvertex];
     if (++rightvertex==numvertex) rightvertex=0;
     deltay=vertexy[rightvertex]-mr_y;
     if (!deltay)
      {
       if (leftvertex==rightvertex) return; // the last edge is exactly horizontal
       goto skiprightvertex;
       }
     deltax=vertexx[rightvertex]-oldx;
     rightfrac=(oldx<<FRACBITS);     // fix roundoff
     rightstep=(deltax<<FRACBITS)/deltay;
     }
   if (mr_y==vertexy[leftvertex])
    {
skipleftvertex:
     oldx=vertexx[leftvertex];
     if (--leftvertex==-1) leftvertex=numvertex-1;
     deltay=vertexy[leftvertex]-mr_y;
     if (!deltay) goto skipleftvertex;
     deltax=vertexx[leftvertex]-oldx;
     leftfrac=(oldx<<FRACBITS);      // fix roundoff
     leftstep=(deltax<<FRACBITS)/deltay;
     }
   if (vertexy[rightvertex]<vertexy[leftvertex]) stopy=vertexy[rightvertex];
    else stopy=vertexy[leftvertex];
   // draw a trapezoid
   if (stopy<=scrollmin)
    {
     leftfrac+=leftstep * (stopy-mr_y);
     rightfrac+=rightstep * (stopy-mr_y);
     mr_y=stopy;
     continue;
     }
   if (mr_y<scrollmin)
    {
     leftfrac+=leftstep * (scrollmin-mr_y);
     rightfrac+=rightstep * (scrollmin-mr_y);
     mr_y=scrollmin;
     }
   if (stopy>scrollmax) stopy=scrollmax;
   for (; mr_y<stopy; mr_y++)
    {
     mr_x1=leftfrac>>FRACBITS;
     mr_x2=rightfrac>>FRACBITS;
     if (mr_x1<xclipl) mr_x1=xclipl;
     if (mr_x2>xcliph) mr_x2=xcliph;
     if (mr_x1<xcliph && mr_x2>mr_x1) spanfunction(); // different functions for flat and slope
     leftfrac+=leftstep;
     rightfrac+=rightstep;
     }
   } while (rightvertex!=leftvertex && mr_y!=scrollmax);
 }


void CalcPlaneEquation(void)
/* Calculates planeA, planeB, planeC, planeD
   planeD is actually -planeD
   for vertexpt[0-2] */
{
 fixed_t x1, y1, z1;
 fixed_t x2, y2, z2;

 // calculate two vectors going away from the middle vertex
 x1=vertexpt[0].tx-vertexpt[1].tx;
 y1=vertexpt[0].ty-vertexpt[1].ty;
 z1=vertexpt[0].tz-vertexpt[1].tz;
 x2=vertexpt[2].tx-vertexpt[1].tx;
 y2=vertexpt[2].ty-vertexpt[1].ty;
 z2=vertexpt[2].tz-vertexpt[1].tz;
 // the A, B, C coefficients are the cross product of v1 and v2
 // shift over to save some precision bits
 planeA=(FIXEDMUL(y1, z2)-FIXEDMUL(z1, y2))>>8;
 planeB=(FIXEDMUL(z1, x2)-FIXEDMUL(x1, z2))>>8;
 planeC=(FIXEDMUL(x1, y2)-FIXEDMUL(y1, x2))>>8;
 // calculate D based on A,B,C and one of the vertex points
 planeD=FIXEDMUL(planeA,vertexpt[0].tx) + FIXEDMUL(planeB,vertexpt[0].ty) +
  FIXEDMUL(planeC,vertexpt[0].tz);
 }


bool ZClipPolygon(int numvertexpts, fixed_t minz)
{
 int         v;
 fixed_t     scale;
 fixed_t     frac, cliptx, clipty;
 clippoint_t *p1, *p2;

 numvertex=0;
 if (minz<MINZ) minz=MINZ; // less than this will cause problems
 p1=&vertexpt[0];
 for (v=1; v<=numvertexpts; v++)
  {
   p2=p1;                   // p2 is old point
   if (v!=numvertexpts) p1=&vertexpt[v]; // p1 is new point
    else p1=&vertexpt[0];
   if ((p1->tz<minz) ^ (p2->tz<minz))
    {
     scale=FIXEDDIV(SCALE,minz);
     frac=FIXEDDIV((p1->tz-minz),(p1->tz-p2->tz));
     cliptx=p1->tx+FIXEDMUL((p2->tx-p1->tx),frac);
     clipty=p1->ty+FIXEDMUL((p2->ty-p1->ty),frac);
     vertexx[numvertex]=CENTERX+(FIXEDMUL(cliptx,scale)>>FRACBITS);
     vertexy[numvertex]=CENTERY-(FIXEDMUL(clipty,scale)>>FRACBITS);
     if (ceilingbit && vertexy[numvertex]>640) return false;
     numvertex++;
     }
   if (p1->tz>=minz)
    {
     vertexx[numvertex]=p1->px;
     vertexy[numvertex]=p1->py;
     if (ceilingbit && vertexy[numvertex]>640) return false;
     numvertex++;
     }
   }
 if (!numvertex) return false;
 return true;
 }


void RenderTileEnds(void)
/* draw floor and ceiling for tile */
{
 int flags, polytype;

 xcliph++;
 flags=mapflags[mapspot];
 // draw the floor
 flatpic=floorpic[mapspot];
 mr_shadow=mapeffects[mapspot];

 if (mr_shadow==1) mr_shadow=(intptr_t)(colormaps+(wallglow<<8));
 else if (mr_shadow==2) mr_shadow=(intptr_t)(colormaps+(wallflicker1<<8));
 else if (mr_shadow==3) mr_shadow=(intptr_t)(colormaps+(wallflicker2<<8));
 else if (mr_shadow==4) mr_shadow=(intptr_t)(colormaps+(wallflicker3<<8));
 else if (mr_shadow>=5 && mr_shadow<=8)
  {
   if (wallcycle==mr_shadow-5) mr_shadow=(intptr_t)colormaps;
    else mr_shadow=0;
   }
 mr_light=maplight;
 flatpic=flattranslation[flatpic];
 mr_picture=lumpmain[flatlump+flatpic];
 polytype=(flags&FL_FLOOR)>>FLS_FLOOR;
 ceilingbit=false;
 switch (polytype)
  {
   case POLY_FLAT:
    spantype=sp_flat;
    mr_deltaheight=vertex[0]->floorheight;
    if (mr_deltaheight<0)
     {
      COPYFLOOR(0, 0);
      COPYFLOOR(1, 1);
      COPYFLOOR(2, 2);
      COPYFLOOR(3, 3);
      if (ZClipPolygon(4, -mr_deltaheight)) RenderPolygon(FlatSpan);
      }
    break;
   case POLY_SLOPE:
    spantype=sp_slope;
    COPYFLOOR(0, 0);
    COPYFLOOR(1, 1);
    COPYFLOOR(2, 2);
    COPYFLOOR(3, 3);
    CalcPlaneEquation();
    if (ZClipPolygon(4,(fixed_t)MINZ)) RenderPolygon(SlopeSpan);
    break;
   case POLY_ULTOLR:
    spantype=sp_slope;
    COPYFLOOR(0, 0);
    COPYFLOOR(1, 1);
    COPYFLOOR(2, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3,(fixed_t)MINZ)) RenderPolygon(SlopeSpan);
    COPYFLOOR(2, 0);
    COPYFLOOR(3, 1);
    COPYFLOOR(0, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3,(fixed_t)MINZ)) RenderPolygon(SlopeSpan);
    break;
   case POLY_URTOLL:
    spantype=sp_slope;
    COPYFLOOR(0, 0);
    COPYFLOOR(1, 1);
    COPYFLOOR(3, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3,(fixed_t)MINZ)) RenderPolygon(SlopeSpan);
    COPYFLOOR(1, 0);
    COPYFLOOR(2, 1);
    COPYFLOOR(3, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3,(fixed_t)MINZ)) RenderPolygon(SlopeSpan);
    break;
   }
 // draw the ceiling
 ceilingbit=true;
 flatpic=ceilingpic[mapspot];
 if (ceilingflags[mapspot] & F_TRANSPARENT) transparent=true;
  else transparent=false;
 flatpic=flattranslation[flatpic];
 mr_picture=lumpmain[flatlump+flatpic];
 polytype=(flags&FL_CEILING)>>FLS_CEILING;
 switch (polytype)
  {
   case POLY_FLAT:
    if (flatpic==63) spantype=sp_sky;
     else if (transparent) spantype=sp_flatsky;
     else spantype=sp_flat;
    mr_deltaheight=vertex[0]->ceilingheight;
    if (mr_deltaheight>0)
     {
      COPYCEILING(3, 0);
      COPYCEILING(2, 1);
      COPYCEILING(1, 2);
      COPYCEILING(0, 3);
      if (ZClipPolygon(4, mr_deltaheight)) RenderPolygon(FlatSpan);
      }
    break;
   case POLY_SLOPE:
    if (flatpic==63) spantype=sp_sky;
     else if (transparent) spantype=sp_slopesky;
     else spantype=sp_slope;
    COPYCEILING(3, 0);
    COPYCEILING(2, 1);
    COPYCEILING(1, 2);
    COPYCEILING(0, 3);
    CalcPlaneEquation();
    if (ZClipPolygon(4, MINZ)) RenderPolygon(SlopeSpan);
    break;
   case POLY_ULTOLR:
    if (flatpic==63) spantype=sp_sky;
     else if (transparent) spantype=sp_slopesky;
     else spantype=sp_slope;
    COPYCEILING(3, 0);
    COPYCEILING(2, 1);
    COPYCEILING(1, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3, MINZ)) RenderPolygon(SlopeSpan);
    COPYCEILING(3, 0);
    COPYCEILING(1, 1);
    COPYCEILING(0, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3, MINZ)) RenderPolygon(SlopeSpan);
    break;
   case POLY_URTOLL:
    if (flatpic==63) spantype=sp_sky;
     else if (transparent) spantype=sp_slopesky;
     else spantype=sp_slope;
    COPYCEILING(3, 0);
    COPYCEILING(2, 1);
    COPYCEILING(0, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3, MINZ)) RenderPolygon(SlopeSpan);
    COPYCEILING(2, 0);
    COPYCEILING(1, 1);
    COPYCEILING(0, 2);
    CalcPlaneEquation();
    if (ZClipPolygon(3, MINZ)) RenderPolygon(SlopeSpan);
    break;
   }
 }
