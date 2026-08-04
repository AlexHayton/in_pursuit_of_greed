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

#include <STDIO.H>
#include <MATH.H>
#include <STRING.H>
#include <STDLIB.H>
#include "d_global.h"
#include "r_refdef.h"
#include "d_misc.h"
#include "d_ints.h"

/**** VARIABLES ****/

#define MAXENTRIES 1024

byte    westwall[MAPROWS*MAPCOLS];
byte    westflags[MAPROWS*MAPCOLS];
byte    northwall[MAPROWS*MAPCOLS];
byte    northflags[MAPROWS*MAPCOLS];
byte    floorpic[MAPROWS*MAPCOLS];
byte    floorflags[MAPROWS*MAPCOLS];
byte    ceilingpic[MAPROWS*MAPCOLS];
byte    ceilingflags[MAPROWS*MAPCOLS];
byte    floorheight[MAPROWS*MAPCOLS];
byte    ceilingheight[MAPROWS*MAPCOLS];
byte    floordef[MAPROWS*MAPCOLS];
byte    floordefflags[MAPROWS*MAPCOLS];
byte    ceilingdef[MAPROWS*MAPCOLS];
byte    ceilingdefflags[MAPROWS*MAPCOLS];
byte    maplights[MAPROWS*MAPCOLS];
byte    mapsprites[MAPROWS*MAPCOLS];
byte    mapslopes[MAPROWS*MAPCOLS];
byte    mapeffects[MAPROWS*MAPCOLS];
byte    mapflags[MAPROWS*MAPCOLS];
int     reallight[MAPROWS*MAPCOLS];
int     actionflag;
int     wallglow, wallglowindex;
int     wallrotate;
int     maplight;
int     wallflicker1, wallflicker2, wallflicker3, wallflicker4, wallflags, wallcycle;

// each visible vertex is used up to four times, so to prevent recalculation
// the vertex info is reused if it has been calculated previously that f
// The calculated flag is also used to determine if a moving sprite is i
// is at least partially visable.
//
// frameon is incremented at the start of each frame, so it is 1 on the
// framevalid[][] holds the frameon number for which vertex[][] is valid
//      set to 0 at initialization, so no points are valid
// cornervertex[][] is a pointer into vertexlist[]
// vertexlist[] holds the currently valid transformed vertexes
// vertexlist_p is set to vertexlist[0] at the start of each frame, and
//      after transforming a new vertex

int      frameon;
int      framevalid[MAPROWS*MAPCOLS];
int      framech[MAPROWS*MAPCOLS];
int      framefl[MAPROWS*MAPCOLS];
vertex_t *cornervertex[MAPROWS*MAPCOLS];
vertex_t vertexlist[MAXVISVERTEXES], *vertexlist_p;
fixed_t  costable[ANGLES+1];
fixed_t  sintable[ANGLES+1];
pixel_t  viewbuffer[MAX_VIEW_WIDTH*MAX_VIEW_HEIGHT];
pixel_t  *viewylookup[MAX_VIEW_HEIGHT];
fixed_t  yslope[MAX_VIEW_HEIGHT+MAXSCROLL2], xslope[MAX_VIEW_WIDTH+1];
byte     **wallposts;
byte     *colormaps;
int      numcolormaps;
byte     *zcolormap[(MAXZ>>FRACBITS)+1];
fixed_t  viewx, viewy, viewz;
fixed_t  viewcos, viewsin;
fixed_t  xscale, yscale;         // SCALE/viewcos , SCALE/viewsin
int      viewangle, viewfineangle;
int      viewtilex, viewtiley;
vertex_t *vertex[4];             // points to the for corner vertexes in vert
vertex_t *p1, *p2;
int      side;                   // wall number 0-3
int      walltype;               // wall number (picture) of p1-p2 edge
int      wallshadow;             // degree of shadow for a tile
int      xclipl, xcliph;         // clip window for current tile
int      tilex, tiley;           // coordinates of the tile being rendered
int      mapspot;                // tiley*MAPSIZE+tilex
int      *flattranslation;       // global animation tables
int      *walltranslation;
int      spritelump, walllump, flatlump;
int      numsprites, numwalls, numflats;
bool  doortile;               // true if the tile being renderd has a door
int      adjacentx[4]= {  0, 1, 0, -1 };
int      adjacenty[4]= { -1, 0, 1,  0 };
entry_t  entries[MAXENTRIES], *entry_p;
int      entrymap[MAPCOLS*MAPROWS], entrycount[MAPCOLS*MAPROWS];
int      entrycounter;
int      fxtimecount;
extern int rtimecount;

vertex_t *TransformVertex(int tilex, int tiley)
/* Returns a pointer to the vertex for a given coordinate
   tx,tz will be the transformed coordinates
   px, floorheight, ceilingheight will be valid if tz >= MINZ */
{
 fixed_t  trx, try, scale;
 vertex_t *point;
 int      mapspot2, fl, ch;

 mapspot2=tiley*MAPROWS+tilex;
 if (mapspot!=mapspot2)
  {
   if (mapflags[mapspot]&FL_FLOOR) fl=(floorheight[mapspot2]<<FRACBITS)-viewz;
    else fl=((floorheight[mapspot])<<FRACBITS)-viewz;
   if (mapflags[mapspot]&FL_CEILING) ch=(ceilingheight[mapspot2]<<FRACBITS)-viewz;
    else ch=((ceilingheight[mapspot])<<FRACBITS)-viewz;
   }
 else
  {
   fl=((floorheight[mapspot2])<<FRACBITS)-viewz;
   ch=((ceilingheight[mapspot2])<<FRACBITS)-viewz;
   }
 if (framevalid[mapspot2]==frameon && framefl[mapspot2]==fl && framech[mapspot2]==ch)
  return cornervertex[mapspot2];
 point=vertexlist_p++;
#ifdef VALIDATE
 if (point==&vertexlist[MAXVISVERTEXES]) MS_Error("Vertexlist overflow (%i>=%i)",vertexlist_p-vertexlist,MAXVISVERTEXES);
#endif
 point->floorheight=fl;
 point->ceilingheight=ch;
 trx=(tilex<<(FRACBITS+TILESHIFT))-viewx;
 try=(tiley<<(FRACBITS+TILESHIFT))-viewy;
 point->tx=FIXEDMUL(trx,viewsin)+FIXEDMUL(try,viewcos);
 point->tz=FIXEDMUL(trx,viewcos)-FIXEDMUL(try,viewsin);
 if (point->tz>=MINZ)
  {
   scale=FIXEDDIV(SCALE,point->tz);
   point->px=CENTERX+(FIXEDMUL(point->tx,scale)>>FRACBITS);
   point->floory=CENTERY-(FIXEDMUL(point->floorheight,scale)>>FRACBITS);
   point->ceilingy=CENTERY-(FIXEDMUL(point->ceilingheight,scale)>>FRACBITS);
   }
 framevalid[mapspot2]=frameon;
 cornervertex[mapspot2]=point;
 framefl[mapspot2]=fl;
 framech[mapspot2]=ch;
 return point;
 }


bool ClipEdge(void)
/* Sets p1->px and p2->px correctly for Z values < MINZ
   Returns false if entire edge is too close or far away */
{
 fixed_t leftfrac, rightfrac, clipz, dx, dz;

 if (p1->tz>MAXZ && p2->tz>MAXZ) return false; // entire face is too far away
 if (p1->tz<=0 && p2->tz<=0) return false; // totally behind the projection plane
 if (p1->tz<MINZ || p2->tz<MINZ)
  {
   dx = p2->tx - p1->tx;
   dz = p2->tz - p1->tz;
   if (p1->tz<MINZ)
    {
     if (labs(dx+dz)<1024) return false;
     leftfrac = FIXEDDIV(-p1->tx - p1->tz , dx+dz);
     }
   if (p2->tz<MINZ)
    {
     if (labs(dz-dx)<1024) return false;
     rightfrac = FIXEDDIV(p1->tx - p1->tz , dz-dx);
     if (p1->tz<MINZ && rightfrac<leftfrac) return false;  // back face
     clipz = p1->tz + FIXEDMUL(dz,rightfrac);
     if (clipz<0) return false;
     p2->px = windowWidth;
     }
   }
 if (p1->tz<MINZ)
  {
   clipz = p1->tz + FIXEDMUL(dz,leftfrac);
   if (clipz<0) return false;
   p1->px = 0;
   }
 if (p1->px==p2->px) return false;
 return true;
 }


void RenderTileWalls(entry_t *e)
{
 int xl, xh, tx, ty, x1, x2;

 tilex=e->tilex;
 tiley=e->tiley;
 xclipl=e->xmin;
 xcliph=e->xmax;
//#ifdef VALIDATE
// if ((tilex<0)||(tilex>=MAPCOLS)||(tiley<0)||(tiley>=MAPROWS)||(xclipl<0)||
//  (xclipl>=windowWidth)||(xcliph<0)||(xcliph>=windowWidth)||(xclipl>xcliph))
//  MS_Error("Invalid RenderTile (%i, %i, %i, %i)\n", e->tilex, e->tiley,
//  e->xmin, e->xmax);
//#endif
 mapspot=tiley*MAPCOLS+tilex;
 maplight=((int)maplights[mapspot]<<3)+reallight[mapspot];
 wallshadow=mapeffects[mapspot];
 // validate or transform the four corner vertexes
 vertex[0]=TransformVertex(tilex,tiley);
 vertex[1]=TransformVertex(tilex+1,tiley);
 vertex[2]=TransformVertex(tilex+1,tiley+1);
 vertex[3]=TransformVertex(tilex,tiley+1);
 // handle a door if present
 if (mapflags[mapspot]&FL_DOOR)
  {
   doortile=true;
   RenderDoor();      // sets doorxl / doorxh
   }
 else doortile=false;
 // draw or flow through the walls
 for (side=0; side<4; side++)
  {
   p1=vertex[side];
   p2=vertex[(side+1)&3];
   if (!ClipEdge()) continue;
   if (p1->px>=p2->px) continue;
   switch (side)
    {
     case 0: // north
      walltype=northwall[mapspot];
      wallflags=northflags[mapspot];
      break;
     case 1: // east
      walltype=westwall[mapspot+1];
      wallflags=westflags[mapspot+1];
      break;
     case 2: // south
      walltype=northwall[mapspot+MAPCOLS];
      wallflags=northflags[mapspot+MAPCOLS];
      break;
     case 3: // west
      walltype=westwall[mapspot];
      wallflags=westflags[mapspot];
     }
   x1=p1->px<xclipl?xclipl : p1->px;
   x2=p2->px-1>xcliph?xcliph : p2->px-1;
   if (x1<=x2)
    {      // totally clipped off side
     if (walltype) 
		 DrawWall(x1,x2);
     DrawSteps(x1,x2);
     }
   if (walltype==0 || (wallflags&F_TRANSPARENT))
    {
     // restrict outward flow by the door, if present
     xl=p1->px;
     xh=p2->px-1;
     // restrict by clipping window
     if (xl<xclipl) xl=xclipl;
     if (xh>xcliph) xh=xcliph;
     // flow into the adjacent tile if there is at least a one pix
     if (xh>=xl)
      {
       tx=tilex+adjacentx[side];
       ty=tiley+adjacenty[side];
       if (tx<0 || tx>=MAPCOLS-1 || ty<0 || ty>=MAPROWS-1) continue;
       entry_p->tilex=tx;
       entry_p->tiley=ty;
       entry_p->xmin=xl;
       entry_p->xmax=xh;
       entry_p->mapspot=(ty<<6)+tx;
       ++entrycounter;
       entry_p->counter=entrycounter;
       entrycount[entry_p->mapspot]=entrycounter;
       ++entry_p;
#ifdef VALIDATE
       if (entry_p>=&entries[MAXENTRIES]) MS_Error("Entry Array OverFlow (%i>=%i)",entry_p-entries,MAXENTRIES);
#endif
       }
     }
   }
 }


void SetupFrame(void)
{
 int i;

 memset(viewbuffer,0,windowSize);

 /* Clears the wallz array, so posts that fade out into the distance won't block sprites */
 for(i=0;i<windowWidth;i++)
  wallz[i]=MAXZ+1;

 // reset span counters
 numspans=0;
 transparentposts=0;
 ++frameon;
 vertexlist_p=vertexlist;    // put the first transformed vertex

 // special effects
 if (rtimecount>fxtimecount)
  {
   if (++wallglowindex==32) wallglowindex=0;
   if (wallglowindex<16) wallglow=wallglowindex<<1;
    else wallglow=(32-wallglowindex)<<1;
   if (wallrotate==63) wallrotate=0;
    else wallrotate++;
   wallflicker1=(MS_RndT()&63);
   wallflicker2=(MS_RndT()&63);
   wallflicker3=(MS_RndT()&63);
   if (frameon&1) wallflicker4=(MS_RndT()%63)-32;
   wallcycle++;
   wallcycle&=3;
   fxtimecount=timecount+5;
   }

 viewtilex=viewx>>TILEFRACSHIFT;
 viewtiley=viewy>>TILEFRACSHIFT;
 viewfineangle=viewangle<<FINESHIFT;
 viewcos=costable[viewangle];
 viewsin=sintable[viewangle];
 xscale=FIXEDDIV(viewsin,SCALE);
 yscale=FIXEDDIV(viewcos,SCALE);
 }


void FlowView()
{
 entry_t *process_p, *nextprocess_p;

 process_p=entries;
 process_p->tilex=viewtilex;
 process_p->tiley=viewtiley;
 process_p->mapspot=(viewtiley<<6)+viewtilex;
 process_p->xmin=0;
 process_p->xmax=windowWidth-1;
 entry_p=process_p+1;
 memset(entrycount,0,MAPCOLS*MAPROWS*4);
 entrycounter=1;
 while (process_p<entry_p)
  {
   if (process_p->mapspot==-1) // entry has been merged
    {
     process_p++;
     continue;
     }

    /* check for mergeable entries */
   if (entrycount[process_p->mapspot]>process_p->counter) // mergeable tile
    for (nextprocess_p=process_p+1;nextprocess_p<entry_p;nextprocess_p++) // scan for mergeable entries
     if (nextprocess_p->mapspot==process_p->mapspot)
      {
       if (nextprocess_p->xmin==process_p->xmax+1) process_p->xmax=nextprocess_p->xmax;
       else if (nextprocess_p->xmax==process_p->xmin-1) process_p->xmin=nextprocess_p->xmin;
       else // bad merge!
	MS_Error("Bad tile event combination:\n"
		 " nextprocess_p=%p process_p=%p\n"
		 " nextprocess_p->xmin=%d  nextprocess_p->xmax=%d\n"
		 " process_p->xmin=%d  process_p->xmax=%d\n",
		 (void *)nextprocess_p,(void *)process_p,
		 nextprocess_p->xmin,nextprocess_p->xmax,
		 process_p->xmin,process_p->xmax);
       entrycount[nextprocess_p->mapspot]=0;
       nextprocess_p->mapspot=-1;
       }

     /* check for a dublicate entry */
   if (entrymap[process_p->mapspot]==frameon) goto end;

   entrymap[process_p->mapspot]=frameon;
   RenderTileWalls(process_p);
   RenderTileEnds();
end:
   process_p++;
   }
 }
