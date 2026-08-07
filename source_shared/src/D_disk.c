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

#include <MALLOC.H>
#include <IO.H>
#include <FCNTL.H>
#include <STRING.H>
#include <SYS/STAT.H>
#include "d_global.h"
#include "d_disk.h"
#include "d_misc.h"
#include "protos.h"

/**** VARIABLES ****/

fileinfo_t fileinfo;     // the file header
lumpinfo_t *infotable;   // pointers into the cache file
void       **lumpmain;   // pointers to the lumps in main memory
int        cachehandle;  // handle of current file

extern bool waiting;

/* The optional 4x art pack: GREED_HD.001.BLO and friends.

   Each part is a lump-number-parallel overlay of GREED.BLO with the same entry
   count, where size 0 means "not overridden, read the original".  Addressing by
   number rather than name is forced: the wall, flat and sprite lumps carry no
   names at all -- only the 244 markers and menu items do -- so there is nothing
   to match.

   The pack is split across several files purely because GitHub refuses a blob
   over 100 MB and warns over 50, and the whole pack is 209 MB.  Nothing else
   depends on the split: a part is just a partial pack, exactly the shape
   `pack.py --only wall` already produced for testing one class at a time, so
   any subset of parts loads and the lumps the missing ones would have supplied
   come from the original art.  A single undivided GREED_HD.BLO still works and
   is still loaded if present.

   The merge happens at the directory level rather than by threading an archive
   id through CA_CacheLump/CA_ReadLump/CA_FreeLump, because the whole engine
   indexes lumps through one global number space and that would touch ~40 call
   sites.  Here, only the two reads below change.

   hdinfo[] holds the merged directory so the two art sets can be swapped back
   and forth at runtime without re-reading anything. */
#define HD_MAXPARTS  64          /* lumpsrc is a byte, so the ceiling is 255 */

static int        hdhandle[HD_MAXPARTS];
static int        hdparts;       /* how many parts are open */
static lumpinfo_t *hdinfo;       /* the merged directory, or NULL */
static lumpinfo_t *origtable;    /* GREED.BLO's, kept for switching back */
static byte       *hdpart;       /* per lump: which part holds it, 1-based */
static byte       *lumpsrc;      /* per lump: 0 = read GREED.BLO, else part */
static int        hdtexshift, hdflatshift, hdspriteshift, hdhudscale;

int  hdart;                      /* 1 when the HD art is the active set */
int  hdartavail;                 /* 1 when a pack was found at startup */

/**** FUNCTIONS ****/

void CA_ReadFile(char *name, void *buffer, unsigned length)
/* generic read file */
{
 int handle;

 if ((handle=open(name,O_RDONLY | O_BINARY))==-1) MS_Error("CA_ReadFile: Open failed on %s!",name);
 if (!read(handle,buffer,length))
  {
   close(handle);
   MS_Error("CA_LoadFile: Read failed on %s!",name);
   }
 close(handle);
}


void *CA_LoadFile(char *name)
/* generic load file */
{
 int      handle;
 unsigned length;
 void     *buffer;

 if ((handle=open(name,O_RDONLY | O_BINARY))==-1) MS_Error("CA_LoadFile: Open failed on %s!",name);
 length=filelength(handle);
 if (!(buffer=malloc(length))) MS_Error("CA_LoadFile: Malloc failed for %s!",name);
 if (!read(handle,buffer,length))
  {
   close(handle);
   MS_Error("CA_LoadFile: Read failed on %s!",name);
   }
 close(handle);
 return buffer;
 }


void CA_InitFile(char *filename)
/* initialize link file */
{
 unsigned size;
 long i;

 if (cachehandle) // already open, must shut down
  {
   close(cachehandle);
   free(infotable);
   for(i=0;i<fileinfo.numlumps;i++)  // dump the lumps
    if (lumpmain[i]) free(lumpmain[i]);
   free(lumpmain);
   }
 // load the header
 if ((cachehandle=open(filename,O_RDONLY | O_BINARY))==-1)
  MS_Error("CA_InitFile: Can't open %s!",filename);
 read(cachehandle,(void *)&fileinfo, sizeof(fileinfo));
 // load the info list
 size=fileinfo.infotablesize;
 infotable=malloc(size);
 lseek(cachehandle,fileinfo.infotableofs,SEEK_SET);
 read(cachehandle,(void *)infotable, size);
 /* lumpmain is void**, so this must size by pointer, not by int.  On LP64 the
    original sizeof(int) under-allocated by half and overflowed the heap. */
 size=fileinfo.numlumps*sizeof(void *);
 lumpmain=malloc(size);
 memset(lumpmain,0,size);
 }


int CA_CheckNamedNum(char *name)
/* returns number of lump if found
   returns -1 if name not found */
{
 int i, ofs;

 for(i=0;i<fileinfo.numlumps;i++)
  {
   ofs=infotable[i].nameofs;
   if (!ofs) continue;
   if (stricmp(name,((char *)infotable)+ofs)==0) return i;
   }
 return -1;
 }


int CA_GetNamedNum(char *name)
/* searches for lump with name
   returns -1 if not found */
{
 int i;

 i=CA_CheckNamedNum(name);
 if (i!=-1) return i;
 MS_Error("CA_GetNamedNum: %s not found!",name);
 return -1;
 }


/* The sidecar's header: fileinfo_t's three fields, then what the art set needs
   the renderer to know.  Kept a superset so the first three parse identically. */
#pragma pack(push,packing,1)
typedef struct
 {
  short numlumps;
  int   infotableofs;
  int   infotablesize;
  short version;
  /* Per class, so a partial pack -- built one class at a time -- describes only
     what it actually contains.  A flats-only sidecar leaves texshift at 6. */
  short texshift;      /* log2 of the wall width:      8 for a 4x pack */
  short flatshift;     /* log2 of the flat dimension:  8 for a 4x pack */
  short spriteshift;   /* extra sprite texel density:  2 for a 4x pack */
  short hudscale;      /* chrome upscale for 320x200 art: 4 for a 4x pack */
  } hdheader_t;
#pragma pack(pop,packing)

#define HD_VERSION 1


static int hd_max(int a,int b) { return a>b ? a : b; }


int CA_OverlayFile(char *filename)
/* Merge one part of the art pack over the directory.  Returns 0 if the file is
   not there, which is how CA_OverlayArt knows it has run off the end of the
   sequence -- absence is not an error, the pack is optional and the game runs
   on the original art without it. */
{
 hdheader_t hd;
 lumpinfo_t *part;
 unsigned   size;
 int        h, i, n;

 if (hdparts>=HD_MAXPARTS) return 0;
 if ((h=open(filename,O_RDONLY | O_BINARY))==-1) return 0;

 if (read(h,(void *)&hd,sizeof(hd))!=(int)sizeof(hd) ||
     hd.version!=HD_VERSION || hd.numlumps!=fileinfo.numlumps)
  {
   printf("HD art:\t%s is not a usable pack, ignoring\n",filename);
   close(h);
   return 0;
   }

 if (!hdinfo)                     /* first part: set up the merge targets */
  {
   hdinfo=calloc(fileinfo.numlumps,sizeof(lumpinfo_t));
   origtable=malloc((size_t)fileinfo.numlumps*sizeof(lumpinfo_t));
   memcpy(origtable,infotable,(size_t)fileinfo.numlumps*sizeof(lumpinfo_t));
   hdpart=calloc(fileinfo.numlumps,1);
   lumpsrc=calloc(fileinfo.numlumps,1);
   hdtexshift=TILESHIFT;
   hdflatshift=TILESHIFT;
   hdspriteshift=0;
   hdhudscale=1;
   }

 size=hd.infotablesize;
 part=malloc(size);
 lseek(h,hd.infotableofs,SEEK_SET);
 if (read(h,(void *)part,size)!=(int)size)
  MS_Error("CA_OverlayFile: short read on %s directory",filename);

 hdhandle[hdparts++]=h;
 for (i=0,n=0;i<fileinfo.numlumps;i++)
  {
   if (!part[i].size) continue;   /* this part does not carry that lump */
   hdinfo[i]=part[i];
   /* A part carries no name blob -- it is addressed purely by lump number --
      so every one of its nameofs fields is 0.  Names have to keep coming from
      GREED.BLO's infotable, or CA_CheckNamedNum stops finding any lump that is
      both named and overridden.  door_1 is exactly that: a wall lump with a
      name, and losing it kills RF_PreloadGraphics. */
   hdinfo[i].nameofs=origtable[i].nameofs;
   hdinfo[i].compress=0;
   hdpart[i]=(byte)hdparts;       /* 1-based; which file the bytes are in */
   n++;
   }
 free(part);

 /* Highest wins.  Every part of a split pack carries the same values, but a
    hand-built partial pack describes only what it holds -- a flats-only one
    leaves texshift at 6 -- and the defaults are the minima, so max merges both
    cases correctly. */
 hdtexshift=hd_max(hdtexshift,hd.texshift);
 hdflatshift=hd_max(hdflatshift,hd.flatshift);
 hdspriteshift=hd_max(hdspriteshift,hd.spriteshift);
 hdhudscale=hd_max(hdhudscale,hd.hudscale ? hd.hudscale : 1);
 hdartavail=1;

 printf("HD art:\t%s, %d lumps\n",filename,n);
 return 1;
 }


void CA_OverlayArt(char *stem)
/* Load the art pack, however it happens to be split.

   An undivided <stem>.BLO first, then <stem>.001.BLO upwards until one is
   missing.  Both are tried so that a pack built before the split, or one
   reassembled by hand, still works; parts merge over the single file if
   somebody has both. */
{
 char name[64];
 int  i, total;

 snprintf(name,sizeof(name),"%s.BLO",stem);
 CA_OverlayFile(name);

 for (i=1;i<=HD_MAXPARTS;i++)
  {
   snprintf(name,sizeof(name),"%s.%03d.BLO",stem,i);
   if (!CA_OverlayFile(name)) break;
   }

 if (!hdartavail) return;

 for (i=0,total=0;i<fileinfo.numlumps;i++)
  if (hdinfo[i].size) total++;
 printf("HD art:\t%d parts, %d lumps, wall %d flat %d sprite %d hud %d\n",
	 hdparts,total,1<<hdtexshift,1<<hdflatshift,
	 1<<hdspriteshift,hdhudscale);
 }


void CA_SetArtMode(int hd)
/* Point the lump directory at the HD pack or back at the original.

   Every lump whose contents change has to be dropped here, because the caller
   re-caches them afterwards; anything still resident would keep the other art
   set's data at a size the renderer no longer expects. */
{
 int i;

 if (!hdartavail) return;
 if (hd) hd=1;
 if (hd==hdart) return;

 for (i=0;i<fileinfo.numlumps;i++)
  if (hdinfo[i].size)
   {
    CA_FreeLump(i);
    infotable[i]=hd ? hdinfo[i] : origtable[i];
    /* The part number, not a flag: hdinfo[i].filepos is an offset into the
       file that actually holds lump i, so the handle has to follow the
       directory entry.  Writing 1 here sent every read to the first part at
       another part's offset, which read plausible garbage rather than
       failing. */
    lumpsrc[i]=hd ? hdpart[i] : 0;
    }

 hdart=hd;
 texshift       = hd ? hdtexshift : TILESHIFT;
 texscaleshift  = texshift-TILESHIFT;
 texmask        = (1<<texshift)-1;
 flatshift      = hd ? hdflatshift : TILESHIFT;
 flatscaleshift = flatshift-TILESHIFT;
 flatmask       = (1<<flatshift)-1;
 spriteshift    = hd ? hdspriteshift : 0;
 hudscale       = hd ? hdhudscale : 1;
 /* The backdrop lumps are pics, so the sky is upscaled exactly when the chrome
    is; deriving it avoids a header field that could disagree with reality. */
 skyshift       = 0;
 while ((1<<skyshift)<hudscale) skyshift++;
 skymask        = (256<<skyshift)-1;
 }


void *CA_CacheLump(int lump)
/* returns pointer to lump
   caches lump in memory */
{
#ifdef PARMCHECK
 if (lump>=fileinfo.numlumps) MS_Error("CA_LumpPointer: %i>%i max lumps!",lump,fileinfo.numlumps);
#endif
 if (!lumpmain[lump])
  {
   // load the lump off disk
   if (!(lumpmain[lump]=malloc(infotable[lump].size)))
    MS_Error("CA_LumpPointer: malloc failure of lump %d, with size %d",
	      lump,infotable[lump].size);
   int h=(lumpsrc && lumpsrc[lump]) ? hdhandle[lumpsrc[lump]-1] : cachehandle;
   lseek(h,infotable[lump].filepos,SEEK_SET);
   if (waiting) UpdateWait();
   /* Checked, unlike the original: a 4x lump is up to 345 KB rather than 12 KB,
      and a short read there yields silently corrupt art rather than an obvious
      one-lump glitch. */
   if (read(h,lumpmain[lump],infotable[lump].size)!=(int)infotable[lump].size)
    MS_Error("CA_CacheLump: short read on lump %d (%d bytes)",
	      lump,infotable[lump].size);
   if (waiting) UpdateWait();
   }
 return lumpmain[lump];
 }


void CA_ReadLump(int lump, void *dest)
/* reads a lump into a buffer */
{
#ifdef PARMCHECK
 if (lump>=fileinfo.numlumps) MS_Error("CA_ReadLump: %i>%i max lumps!",lump,fileinfo.numlumps);
#endif
 int h=(lumpsrc && lumpsrc[lump]) ? hdhandle[lumpsrc[lump]-1] : cachehandle;

 lseek(h, infotable[lump].filepos, SEEK_SET);
 if (read(h,dest,infotable[lump].size)!=(int)infotable[lump].size)
  MS_Error("CA_ReadLump: short read on lump %d (%d bytes)",
	    lump,infotable[lump].size);
 }


void CA_FreeLump(unsigned lump)
/* frees a cached lump */
{
#ifdef PARMCHECK
 if (lump>=fileinfo.numlumps) MS_Error("CA_FreeLump: %i>%i max lumps!",lump,fileinfo.numlumps);
#endif
 if (!lumpmain[lump]) return;
 free(lumpmain[lump]);
 lumpmain[lump]=NULL;
 }


void CA_WriteLump(unsigned lump)
/* writes a lump to the link file */
{
#ifdef PARMCHECK
 if (lump>=fileinfo.numlumps) MS_Error("CA_WriteLump: %i>%i max lumps!",lump,fileinfo.numlumps);
 if (!lumpmain[lump]) MS_Error("CA_WriteLump: %i not cached in!",lump);
#endif
 lseek(cachehandle,infotable[lump].filepos, SEEK_SET);
 write(cachehandle,lumpmain[lump],infotable[lump].size);
 }

