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
 spantag_t span;

 pointz=FIXEDDIV(mr_deltaheight,yslope[mr_y+viewscroll]);
 if (pointz>MAXZ) return;
 // post the span in the draw list
 span=((spantag_t)pointz<<ZTOFRAC)&ZMASK;
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
 if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, FlatSpan (%i>=%i)",numspans,MAXSPANS);
 }


void SlopeSpan(void)
/* used for sloping floors and ceilings
   planeA, planeB, planeC, planeD must be precalculated
   mr_picture is set once per polygon */
{
 fixed_t  pointz, pointz2;        // row's distance to view plane
 fixed_t  partial, denom;
 span_t   *span_p;
 spantag_t span;

 // calculate the Z values for each end of the span
 partial=FIXEDMUL(planeB,yslope[mr_y+viewscroll])+planeC;
 denom=FIXEDMUL(planeA,xslope[mr_x1])+partial;
 if (denom<8000) return;
 pointz=FIXEDDIV(planeD,denom);
 if (pointz>MAXZ) return;
 denom=FIXEDMUL(planeA,xslope[mr_x2])+partial;
 if (denom<8000) return;
 pointz2=FIXEDDIV(planeD,denom);
 if (pointz2>MAXZ) return;
//  post the span in the draw list
 span=((spantag_t)pointz<<ZTOFRAC)&ZMASK;
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
 if (numspans>=MAXSPANS) MS_Error("MAXSPANS exceeded, SlopeSpan (%i>=%i)",numspans,MAXSPANS);
 }


/* Edge step for the trapezoid walk.

   The engine spells this (deltax<<FRACBITS)/deltay, which overflows fixed_t
   once |deltax| passes 32767: the step comes back wrapped and the edge strides
   the wrong way, laying long thin trapezoids across the view.  320x200 never
   got near it -- the largest difference in practice is about 10400 -- but the
   HD projection is ~4x that and near-plane-clipped vertices go well past.

   Doing the divide in 64 bits and saturating keeps the true slope for every
   edge that can be represented, which is every edge that actually crosses the
   view.  Only an edge sweeping tens of thousands of columns in a row or two
   saturates, and it leaves the view within a fraction of a row either way.

   An earlier attempt clamped the projected x instead.  That bounded the
   subtraction but moved the vertex, which changes the edge's slope -- and
   slope floors are clipped at MINZ, so they are exactly the polygons with
   extreme projections.  Their edges walked wrong and the floor disappeared.
   Bound the step, never the geometry. */
static fixed_t EdgeStep(int deltax, int deltay)
{
 int64_t st;

 st=(((int64_t)deltax)<<FRACBITS)/deltay;
 if (st> 0x3FFFFFFFLL) st= 0x3FFFFFFFLL;
 if (st<-0x3FFFFFFFLL) st=-0x3FFFFFFFLL;
 return (fixed_t)st;
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
 int     guard;

 // find topmost vertex
 rightvertex=0;                  // topmost so far
 for (leftvertex=1; leftvertex<numvertex; leftvertex++)
  if (vertexy[leftvertex]<vertexy[rightvertex]) rightvertex=leftvertex;
 // ride down the left and right edges
 mr_y=vertexy[rightvertex];
 leftvertex=rightvertex;
 if (mr_y>=scrollmax) return;   // totally off bottom

 /* The loop below ends when the two edge walkers meet or mr_y reaches the
    bottom of the view.  rightvertex counts up and leftvertex counts down, both
    wrapping, and they are only compared once per iteration -- so they can pass
    each other without ever being equal on the same pass, and then walk the
    polygon forever.  Every legitimate iteration either consumes a vertex or
    advances mr_y down the view, so this many is already unreachable; it only
    has to stop a malformed polygon from hanging the game.

    320x200 never hit it.  HD does: near-plane clipping at a 4x projection
    scale produces vertices thousands of rows outside the view (measured
    -24264 and 23649 on a five-vertex slope polygon), and the walk over those
    does not converge. */
 guard=2*numvertex + (scrollmax-scrollmin) + 8;
 if (getenv("NOGUARD")) guard=1<<28;
 do
  {
   if (--guard<0) return;
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
     rightstep=EdgeStep(deltax,deltay);
     }
   if (mr_y==vertexy[leftvertex])
    {
skipleftvertex:
     oldx=vertexx[leftvertex];
     if (--leftvertex==-1) leftvertex=numvertex-1;
     deltay=vertexy[leftvertex]-mr_y;
     if (!deltay)
      {
       /* The right-hand walker above has this guard and the left one never
	  did, in the DOS original too.  Without it a run of vertices sharing
	  a y spins here forever. */
       if (leftvertex==rightvertex) return;
       goto skipleftvertex;
       }
     deltax=vertexx[leftvertex]-oldx;
     leftfrac=(oldx<<FRACBITS);      // fix roundoff
     leftstep=EdgeStep(deltax,deltay);
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
     vertexx[numvertex]=CENTERX+ProjectOffset(cliptx,scale);
     vertexy[numvertex]=CENTERY-ProjectOffset(clipty,scale);
     if (ceilingbit && vertexy[numvertex]>viewclipy) return false;
     numvertex++;
     }
   if (p1->tz>=minz)
    {
     vertexx[numvertex]=p1->px;
     vertexy[numvertex]=p1->py;
     if (ceilingbit && vertexy[numvertex]>viewclipy) return false;
     numvertex++;
     }
   }
 if (!numvertex) return false;
 return true;
 }


/* Flats 1..numflats-1 are the real 4096-byte textures.  Index 0 is the
   `startflats` marker and numflats is `endflats`, both zero length, and
   floorpic[]/ceilingpic[] are bytes so they can also hold values past the end
   of flattranslation[].

   A tile carrying flat 0 means "no surface here".  The engine draws it anyway
   and only gets away with it because at 320x200 such tiles are never in view;
   a wider field of view reaches them, lumpmain[flatlump+0] is NULL, and MapRow
   dereferences it.  Returning NULL here makes the caller skip the surface --
   a hole in geometry that was never meant to be visible, rather than a crash.

   Loud under -debugmode so a real content or renderer fault still surfaces
   during development instead of being silently papered over. */
static byte *FlatPicture(int pic)
{
 if (pic<1 || pic>=numflats)
  {
   if (debugmode)
    MS_Error("Flat index %d out of range 1..%d at mapspot %d",pic,numflats-1,mapspot);
   return NULL;
   }
 return lumpmain[flatlump+flattranslation[pic]];
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
 mr_picture=FlatPicture(flatpic);
 if (mr_picture) flatpic=flattranslation[flatpic];
 /* -1 matches no case below, so an absent floor simply is not drawn. */
 polytype=mr_picture ? (int)((flags&FL_FLOOR)>>FLS_FLOOR) : -1;
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
 mr_picture=FlatPicture(flatpic);
 if (mr_picture) flatpic=flattranslation[flatpic];
 polytype=mr_picture ? (int)((flags&FL_CEILING)>>FLS_CEILING) : -1;
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
