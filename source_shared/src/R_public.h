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

#ifndef R_PUBLIC_H
#define R_PUBLIC_H


/**** CONSTANTS ****/

/* The ceiling the view scratch arrays are sized to, NOT the view size.  The
   engine's rasteriser is resolution-independent -- SetViewSize recomputes every
   derived table -- but viewbuffer, viewylookup, xslope/yslope and the per-column
   angle tables are static arrays, so this is the hard limit.

   320x200 until the HD mode; 1920x1200 covers the ~1.3 Mpixel HD cap at any
   display aspect.  Cost is viewbuffer at 2.3 MB, zerofill.

   Nothing may use these as a stride: the row pitch of viewbuffer is
   windowWidth, which is the *current* view width and usually smaller. */
#define MAX_VIEW_WIDTH		1920
#define MAX_VIEW_HEIGHT		1200
#define INIT_VIEW_WIDTH		320
#define INIT_VIEW_HEIGHT	200
#define VIEW_LEFT			0
#define VIEW_TOP			0

/* 16.16 fixed point.  Must stay exactly 32 bits: the engine was built for
   Watcom/MSVC 32-bit, where FIXEDMUL was "imul; shrd" and so truncated to 32
   bits, and several routines depend on that wraparound (R_walls.c's texture
   steppers, the angle math in Raven.c).  On LP64 a "long" is 8 bytes and those
   results silently stop wrapping. */
typedef int fixed_t;

#define WALL_CONTACT    1
#define DOOR_CONTACT    2
#define FRACBITS        16
#define FRACUNIT        (1<<FRACBITS)
#define TILEUNIT        (64*FRACUNIT)
#define HALFTILEUNIT    (32*FRACUNIT)
#define PI              3.14159265
#define MAPSIZE         64              // there must not be any 65th vertexes
#define MAPROWS         64
#define MAPCOLS         64
#define TILESHIFT       6
#define TILESIZE        (1<<TILESHIFT)  // pixels to tile
#define TILEFRACSHIFT   (TILESHIFT+FRACBITS)
#define TILEGLOBAL      (1<<TILEFRACSHIFT)
#define ANGLES          1023
#define WEST            512
#define EAST            0
#define NORTH           256
#define SOUTH           768
#define DEGREE45        128
#define DEGREE45_2      64
#define MINZ            (FRACUNIT/2)

			//first value is the maximum # of tiles to render outwards
#define MAXZ            ((32<<(FRACBITS+TILESHIFT)) - 1)
#define MAXZLIGHT       (MAXZ>>FRACBITS)
#define MAXDOORS        32
#define MAXSPRITES      700
#define MAXELEVATORS    128
#define MAXSPAWNAREAS   96
#define ANIM_LOOP_MASK  1
#define ANIM_CG_MASK    30
#define ANIM_MG_MASK    480
#define ANIM_SELFDEST   32768
#define ANIM_DELAY_MASK 32256


#define MINDIST         (FRACUNIT*12)
#define PLAYERSIZE      (16<<FRACBITS)
#define FRACTILESHIFT   (FRACBITS+TILESHIFT)

#define BACKDROPHEIGHT  100
/* How far looking up and down may scroll the view, in view rows.  60 rows of a
   200-row view is 30% of it; as a flat count it becomes a few percent of an HD
   view, which makes looking up and down nearly useless.  VIEWSCROLL keeps the
   fraction instead, and is exactly 60 at 200 rows. */
#define MAXSCROLL       60
#define VIEWSCROLL(h)   (((h)*MAXSCROLL)/INIT_VIEW_HEIGHT)
/* Slack the yslope/mapcache arrays need above and below the view. */
#define MAXSCROLL2      (2*VIEWSCROLL(MAX_VIEW_HEIGHT))

// flags in mapflags
#define FL_DOOR         128
#define FL_FLOOR        7
#define FL_CEILING      56
#define FL_AUX          64      // not used right now
#define FLS_FLOOR       0
#define FLS_CEILING     3
#define POLY_FLAT       0
#define POLY_SLOPE      1
#define POLY_ULTOLR     2
#define POLY_URTOLL     3
// additional POLY_??? can be defined from 4-7


#define STEP_COLOR      132
#define WALL_COLOR      127
#define TRANS_COLOR     79
#define DOOR_COLOR      73

/**** TYPES ****/

typedef byte pixel_t;

typedef enum
 {
  rt_one, rt_four, rt_eight
  } rotate_t;

typedef enum
 {
  st_none, st_noclip, st_transparent, st_maxlight
  } special_t;

typedef enum
 {
  dr_horizontal, dr_vertical, dr_horizontal2, dr_vertical2
  } orientation_t;

typedef enum
 {
  E_NORMAL, E_SWITCHDOWN, E_SWITCHDOWN2, E_TIMED, E_SWITCHUP, E_SWAP, E_SECRET,
  E_TRIGGERED, E_PRESSUREHIGH, E_PRESSURELOW
  } elevtype;


typedef struct scaleobj_s
 {
  // list links, don't touch
  struct scaleobj_s *prev, *next;
  // modify this part whenever you want
  int       animation;
  longint   animationTime;  // must accept all possible tick values
  fixed_t   moveSpeed, zadj; // zadj = height above floor
  fixed_t   x, y, z;        // global position of the BOTTOM of the shape
  fixed_t   lastx, lasty;
  int       basepic;        // lumpnum is spritelump+basepic+rotation
  rotate_t  rotate;
  int       angle;          // 0 - ANGLES
  int       angle2;
  int       movesize;       // how big he is
  bool		active, nofalling;
  int       intelligence, bullet, enraged;
  longint   movetime, modetime, actiontime, scantime, firetime;
  int       heat, startpic, movemode;
  int       startspot;      // so it doesn't self destruct owner
  int       damage, hitpoints;
  int       type;           // id
  int       spawnid;        // who created it
  int       score;
  int       maxmove;
  int       regen;
  int       deathevent;
  fixed_t   height;         // vertical height
  special_t specialtype;    // transparent, no clipping
  int       scale;
  } scaleobj_t;

typedef struct doorobj_s
 {
  int           tilex, tiley;
  // modify this part whenever you want
  bool       doorOpen;
  bool       doorOpening;
  bool       doorClosing;
  bool       doorBlocked;
  bool       doorBumpable;
  int           doorSize;
  longint       doorTimer;
  byte          doorLocks;
  orientation_t orientation;      // probably only want to set this once
  bool       transparent;      // set true if the pic has any masked areas
  int           pic;                                                                                // lumpnum is doorlump+pic
  int           height;                                                                     // should generally be set to the floor height
  fixed_t       position;         // range from 0 (open) - FRACUNIT*64 (closed
  } doorobj_t;

typedef struct elevobj_s          // elevator structure
 {
  struct elevobj_s *prev, *next;

  bool  elevUp;                // going up?
  bool  elevDown;
  int      position;              // height
  longint  elevTimer;             // time for each movement
  int      floor;                 // set to floorheight[mapspot]
  int      ceiling;               // set to ceilingheight[mapspot]-64
  int      mapspot, speed, eval, endeval, nosave;
  elevtype type;
  } elevobj_t;

typedef struct spawnarea_s
 {
  int     mapspot;
  fixed_t mapx, mapy;
  int     type;
  longint time;
  } spawnarea_t;


/**** VARIABLES ****/

extern int     actionflag;          // if set non 0, the action hook is called
extern pixel_t viewbuffer[MAX_VIEW_WIDTH*MAX_VIEW_HEIGHT];
extern pixel_t *viewylookup[MAX_VIEW_HEIGHT];
extern int     spritelump, walllump, flatlump;
extern int     numsprites, numwalls, numflats;
extern int     *flattranslation, *walltranslation;
extern fixed_t costable[ANGLES+1], sintable[ANGLES+1];
extern byte    westwall[MAPROWS*MAPCOLS];
extern byte    westflags[MAPROWS*MAPCOLS];
extern byte    northwall[MAPROWS*MAPCOLS];
extern byte    northflags[MAPROWS*MAPCOLS];
extern byte    floorpic[MAPROWS*MAPCOLS];
extern byte    floorflags[MAPROWS*MAPCOLS];
extern byte    ceilingpic[MAPROWS*MAPCOLS];
extern byte    ceilingflags[MAPROWS*MAPCOLS];
extern byte    floorheight[MAPROWS*MAPCOLS];
extern byte    ceilingheight[MAPROWS*MAPCOLS];
extern byte    floordef[MAPROWS*MAPCOLS];
extern byte    floordefflags[MAPROWS*MAPCOLS];
extern byte    ceilingdef[MAPROWS*MAPCOLS];
extern byte    ceilingdefflags[MAPROWS*MAPCOLS];
extern byte    maplights[MAPROWS*MAPCOLS];
extern byte    mapsprites[MAPROWS*MAPCOLS];
extern byte    mapslopes[MAPROWS*MAPCOLS];
extern byte    mapeffects[MAPROWS*MAPCOLS];
extern byte    mapflags[MAPROWS*MAPCOLS];
extern int     reallight[MAPROWS*MAPCOLS];
extern int     windowHeight, windowWidth, windowSize,
	       windowLeft, windowTop, scrollmin, scrollmax;
extern intptr_t viewLocation;  /* pointer-width; see R_public.c */
extern fixed_t mapcache_height[MAX_VIEW_HEIGHT+MAXSCROLL2];
extern int     frameon, framevalid[MAPROWS*MAPCOLS];
extern fixed_t CENTERY, CENTERX, SCALE, ISCALE;
extern int     hdmode;         /* active render mode; see R_public.c */
extern int     viewproj;       /* projection scale in pixels, set by SetViewSize */
extern int     viewscroll;     /* look up/down range in view rows */
extern int     viewclipy;      /* ceiling-polygon reject bound; see R_public.c */
extern int     viewminscale;   /* near cutoff on the texel step; see R_public.c */

/* Texture resolution.

   The engine was written with one texel per world unit, so TILESHIFT 6 meant a
   64-unit tile and a 64-texel wall were the same number and the two were used
   interchangeably -- `texture&=63` indexes a wall column with a world
   coordinate.  A 4x texture pack breaks that identity: the tile is still 64
   units (TILESHIFT does NOT change) but the wall is 256 texels across, so
   world coordinates need scaling up before they index texture space.

   texshift       log2 of the wall width: 6 originally, 8 for a 4x pack
   texscaleshift  texshift-TILESHIFT, i.e. log2 texels per world unit: 0 or 2
   texmask        (1<<texshift)-1, the column wrap mask
   flatshift/flatscaleshift/flatmask   the same three for flats
   spriteshift    extra sprite texel density beyond scaleobj_t.scale: 0 or 2
   hudscale       chrome upscale factor for 320x200-authored art: 1 or 4

   Walls and flats are kept separate even though a finished pack upscales both,
   because the pack is built and tested one class at a time -- a sidecar holding
   only 256px flats must not tell the renderer that walls are 256px too.

   These are runtime, not compile-time, so one binary runs either art set --
   the HD pack is optional and the game must still work without it.  They are
   set once from the sidecar's header (CA_OverlayArt) and never change after,
   because the cached lump pointers in weaponpic[]/statusbar[]/font would
   dangle if the art were swapped mid-game. */
extern int     texshift, texscaleshift, texmask;
extern int     flatshift, flatscaleshift, flatmask;
extern int     spriteshift, hudscale;
/* The backdrop/sky.  It is a 256x256 buffer assembled from two 256x128 lumps
   and sampled by (row, column) with the column wrapping, so it scales the same
   way the flats do: 0 or 2, giving 256 or 1024 on a side.  Kept separate from
   texshift/flatshift because the sky is built and packed with the pics. */
extern int     skyshift, skymask;
#define SKYSIZE  (256<<skyshift)
/* Defined in D_disk.c, which owns the sidecar.  hdart is 1 when the 4x pack is
   the active art set; hdartavail is 1 when a pack was found at all. */
extern int     hdart, hdartavail;

/* Texels per screen row for a post at depth z.

   The engine spells this FIXEDMUL(z,ISCALE) with ISCALE = FRACUNIT/proj, an
   integer division whose rounding error grows with the projection: 0.15% at
   the original 160, but 0.45% at an HD scale of 659.  That error is a texture
   step, so it accumulates down the post and varies from column to column with
   z -- which reads as a zigzag along horizontal features on near walls.
   Dividing directly is exact to 1/65536.

   Also taken when a 4x art pack is active, even in original render mode: the
   same -0.146% is 0.19 texel over a 128-row post but 0.75 over a 512-row one.
   Original mode on original art keeps the historical expression, so its output
   is unchanged. */
#define TEXELSTEP(z)   ((hdmode||hdart) ? (fixed_t)((z)/viewproj) : FIXEDMUL((z),ISCALE))
/* Target for the 2D chrome; aliases the view buffer or `screen`.  See
   R_public.c.  Anything drawing 320x200-authored artwork uses these, never
   viewylookup/windowWidth directly. */
extern byte    **hudylookup;
extern int     hudWidth, hudHeight;
extern int     mainViewWidth, mainViewHeight;
extern int     hdviewfresh;
extern int     hdresizepending;
extern bool debugmode;


/**** FUNCTIONS *****/

void         RF_PreloadGraphics(void);
fixed_t      FIXEDMUL(fixed_t,fixed_t);
fixed_t      FIXEDDIV(fixed_t,fixed_t);
void         RF_Startup(void);
void         RF_ClearWorld(void);
doorobj_t   *RF_GetDoor(int tilex, int tiley);
scaleobj_t  *RF_GetSprite(void);
elevobj_t   *RF_GetElevator(void);
spawnarea_t *RF_GetSpawnArea(void);
void         RF_RemoveSprite(scaleobj_t *spr);
void         RF_RemoveElevator(elevobj_t *e);
fixed_t      RF_GetFloorZ(fixed_t x, fixed_t y);
fixed_t      RF_GetCeilingZ(fixed_t x, fixed_t y);
void         RF_SetLights(fixed_t intensity);
void         RF_SetActionHook(void (*hook)(void));
void         RF_CheckActionFlag(void);
void         RF_RenderView(fixed_t x,fixed_t y,fixed_t z,int angle);
void         RF_BlitView(void);
void         SetViewSize(int width, int height);

#include "protos.h"

#endif


