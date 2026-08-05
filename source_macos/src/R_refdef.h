/***************************************************************************/
/*                                                                         */
/*                                                                         */
/* Raven 3D Engine                                                         */
/* Copyright (C) 1995 by Softdisk Publishing                               */
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

#ifndef R_REFDEF_H
#define R_REFDEF_H

#include "r_public.h"

#define rint(x) (int)(x+0.5)

/**** CONSTANTS ****/

#define TANANGLES      8192  // one quadrant
#define FINESHIFT      5
#define MAXVISVERTEXES 1536  // max tile corners visible at once

// for spans
/* A span tag packs the span's depth and its index in spans[] into one integer,
   so that sorting the tags sorts the spans back to front (descending: see
   Partition in R_spans.c).  Depth is the high-order field and must stay there.

   This was 32 bits -- 20 of z above 12 of index -- with every bit spoken for,
   so MAXSPANS could not be raised past 4096 without re-encoding.  4096 is
   about 20 spans per scanline at 200 rows and does not survive a taller view.
   The tag is now 64 bits with a 20-bit index field; the z field keeps its
   width, its contents and its position relative to the index, so the sort
   order is exactly what it was.

   ZTOFRAC lands pointz's bits 8..27 in the z field.  The low 8 bits of the
   16.16 depth are dropped, as they always were -- the tag only has to order
   spans and pick a colormap row, not reproduce the depth exactly.

   MAXSPANS is deliberately generous: the peak is not known until the renderer
   actually runs at a taller view, and the arrays below are zerofill, so the
   cost is address space rather than resident memory (spans[] 12 MB, spantags[]
   2 MB, spansx[] 1 MB -- __common goes from 1.4 MB to 16.8 MB).  Guessing low
   is safe now that the overflow checks below are compiled in unconditionally:
   it fails with a message instead of walking off the end of spans[]. */
#define MAXSPANS       262144
#define ZSHIFT         20
#define ZTOFRAC        12                     // shift the Z into frac position
#define ZMASK          (0xfffffULL<<ZSHIFT)   // 20 bits
#define SPANMASK       0xfffffULL             // 20 bits
#define MAXPEND        32768
#define MAXAUTO        (16*16)

 /* flags */
#define F_RIGHT          (1<<0)
#define F_LEFT           (1<<1)
#define F_UP             (1<<2)
#define F_DOWN           (1<<3)
#define F_TRANSPARENT    (1<<4)
#define F_NOCLIP         (1<<5)
#define F_NOBULLETCLIP   (1<<6)
#define F_DAMAGE         (1<<7)


/**** TYPES ****/

typedef struct
 {
  fixed_t floorheight, ceilingheight;
  fixed_t tx, tz;               // transformed x / distance
  int px;                       // projected x if tz > 0
  int floory, ceilingy;
  } vertex_t;

typedef struct
 {
  int tilex, tiley, xmin, xmax, mapspot, counter;
  } entry_t;

typedef enum
 {
  sp_flat, sp_slope, sp_door, sp_shape, sp_maskeddoor,
  sp_transparentwall, sp_step, sp_sky, sp_slopesky, sp_flatsky,
  sp_inviswall
  } spanobj_t;

typedef struct
 {
  spanobj_t spantype;
  byte *picture;
  void *structure;         // either doorobj or scaleobj
  fixed_t x2, y, yh;
  int light;
  /* shadow doubles as a small mode code (1..9, from mapeffects) AND as a
     colormap POINTER: R_plane.c stores colormaps+offset here and R_spans.c
     casts it back to byte*.  As an int that silently truncated the pointer on
     LP64.  intptr_t keeps both uses working. */
  intptr_t shadow;
  } span_t;

/* The packed depth+index sort key; see the MAXSPANS note above.  Must be wider
   than 32 bits, and must be unsigned so the sort compares the z field as a
   magnitude. */
typedef unsigned long long spantag_t;

typedef struct
 {
  short leftoffset, width;
  short collumnofs[256];   // only uses [width] entries
  } scalepic_t;

typedef struct
 {
  fixed_t tx, ty, tz;
  int px, py;
  } clippoint_t;


/**** VARIABLES ****/

extern void        (*actionhook)(void);
extern vertex_t    vertexlist[MAXVISVERTEXES], *vertexlist_p;
extern fixed_t     yslope[MAX_VIEW_HEIGHT+MAXSCROLL2], xslope[MAX_VIEW_WIDTH+1];
extern byte        **wallposts;
extern byte        *colormaps;
extern int         numcolormaps;
extern byte        *zcolormap[(MAXZ>>FRACBITS)+1];
extern fixed_t     viewx, viewy, viewz;
extern fixed_t     viewcos, viewsin;
extern fixed_t     xscale, yscale;
extern int         viewangle, viewfineangle;
extern int         viewtilex, viewtiley;
extern int         side;
extern int         walltype;
extern int         wallshadow;
extern vertex_t    *vertex[4];        // points to the for corner vertexes in vert
extern vertex_t    *p1, *p2;
extern int         xclipl, xcliph;    // clip window for current tile
extern int         tilex, tiley;      // coordinates of the tile being rendered
extern int         mapspot;           // tiley*MAPSIZE+tilex
extern bool     doortile;          // true if the tile being renderd has a door
extern fixed_t     tangents[TANANGLES *2];
extern fixed_t     sines[TANANGLES *5];
extern int         backtangents[TANANGLES*2];
extern fixed_t     *cosines;          // point 1/4 phase into sines
extern int         pixelangle[MAX_VIEW_WIDTH+1];     // +1 because span ends go one past
extern fixed_t     pixelcosine[MAX_VIEW_WIDTH+1];
extern int         wallpixelangle[MAX_VIEW_WIDTH+1];
extern fixed_t     wallpixelcosine[MAX_VIEW_WIDTH+1];
extern int         campixelangle[MAX_VIEW_WIDTH+1];
extern fixed_t     campixelcosine[MAX_VIEW_WIDTH+1];
extern fixed_t     wallz[MAX_VIEW_WIDTH];
extern byte        *mr_picture;       // pointer to a raw 64*64 pixel picture
extern fixed_t     mf_deltaheight;
extern scaleobj_t  firstscaleobj, lastscaleobj;
extern scaleobj_t  scaleobjlist[MAXSPRITES], *freescaleobj_p;
extern doorobj_t   doorlist[MAXDOORS];
extern int         numdoors;
extern elevobj_t   firstelevobj, lastelevobj;
extern elevobj_t   elevlist[MAXELEVATORS], *freeelevobj_p;
extern spawnarea_t spawnareas[MAXSPAWNAREAS];
extern int         numspawnareas;
extern int         doorxl, doorxh;
extern byte        *sp_dest;          // the bottom most pixel to be drawn (in vie
extern byte        *sp_source;        // the first pixel in the vertical post (may
extern byte        *sp_colormap;      // pointer to a 256 byte color number to pal
extern int         sp_frac;           // fixed point location past sp_source
extern int         sp_fracstep;       // fixed point step value
extern int         sp_count;          // the number of pixels to draw
extern int         sp_loopvalue;      /* was long -- 32-bit, like fixed_t */
extern byte        *mr_dest;          // the left most pixel to be drawn (in viewb
extern byte        *mr_picture;       // pointer to a raw 64*64 pixel picture
extern byte        *mr_colormap;      // pointer to a 256 byte color number to pal
extern int         mr_xfrac;          // starting texture coordinate
extern int         mr_yfrac;          // starting texture coordinate
extern int         mr_xstep;          // fixed point step value
extern int         mr_ystep;          // fixed point step value
extern int         mr_count;          // the number of pixels to draw
extern intptr_t    mr_shadow;   /* mode code or colormap pointer; see span_t */
extern spantag_t   spantags[MAXSPANS];
extern spantag_t   *starttaglist_p, *endtaglist_p;
extern span_t      spans[MAXSPANS], *spans_p;
extern int         spansx[MAXSPANS];
extern int         numspans;
extern int         wallglow;          // wallshadow = 1
extern int         wallglowindex;     // counter for wall glow
extern int         wallrotate;
extern int         maplight;
extern byte        *tpwalls_dest[MAXPEND];  // transparentposts
extern byte        *tpwalls_colormap[MAXPEND];
extern int         tpwalls_count[MAXPEND];
extern int         transparentposts;
extern int         autoangle[MAXAUTO*2+1][MAXAUTO*2+1];
extern int         autoangle2[MAXAUTO][MAXAUTO];
extern int         wallflicker1, wallflicker2, wallflicker3, wallcycle, wallflicker4;
extern int         wallflags;
extern fixed_t     afrac, hfrac;

/**** FUNCTIONS ****/

int      ProjectOffset(fixed_t v, fixed_t scale);
void     SetupFrame(void);
vertex_t *TransformVertex(int tilex, int tiley);
void     FlowView();
void     InitWalls(void);
void     RenderTileWalls(entry_t *e);
void     DrawWall(int x1,int x2);
void     DrawSteps(int x1, int x2);
void     InitPlane(void);
void     ClearMapCache(void);
void     RenderTileEnds(void);
void     FindBackVertex(void);
void     RenderDoor(void);
void     RenderSprites();
void     ScalePost(void);
void     ScaleMaskedPost(void);
void     MapRow(void);
void     DrawSpans(void);
void     MS_Error(char *error, ...);

#endif

