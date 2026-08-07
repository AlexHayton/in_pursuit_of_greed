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
#include <STRING.H>
#include <STDLIB.H>
#include "d_global.h"
#include "d_video.h"
#include "r_public.h"
#include "d_misc.h"
#include "d_ints.h"

#define getbyte() chunkbuf[bufptr++]
#define getword() chunkbuf[bufptr] + (chunkbuf[bufptr+1]<<8); bufptr+=2

/**** TYPES ****/

typedef signed char shortint;

 /* FLI file header */
#pragma pack(push,packing,1)
typedef struct
 {
  longint size;
  word    signature;
  word    nframes;
  word    width;
  word    height;
  word    depth;
  word    flags;
  word    speed;
  longint next;
  longint frit;
  byte    padding[102];
  } fliheader;

 /* individual frame header */
typedef struct
 {
  longint size;
  word    signature;
  word    nchunks;
  byte    padding[8];
  } frameheader;

 /* frame chunk type */
typedef struct
 {
  longint size;
  word    type;
  } chunktype;


/**** VARIABLES ****/

#pragma pack(pop,packing)

static fliheader header;
static int       currentfliframe, bufptr;
static byte      *chunkbuf;
static byte      flipal[256][3];


/**** FUNCTIONS ****/

void fli_readcolors(void)
/* read a color chunk */
{
 int     i, j, total;
 word    packets;
 byte    change, skip;
 byte    *k;

 packets=getword();
 for (i=0;i<packets;i++)
  {
   skip=getbyte();     // colors to skip
   change=getbyte();   // num colors to change
   if (change==0)
    total=256;         // hack for 256
   k=flipal[skip];
   for (j=0;j<total;j++)
    {
     *k++=getbyte();   // r
     *k++=getbyte();   // g
     *k++=getbyte();   // b
     }
   }
 VI_SetPalette((char*)flipal);
 }


void fli_brun(void)
/* read beginning runlength compressed frame */
{
 int      i, j, y, y2, p;
 shortint count;
 byte     data, packets;
 byte     *line;

 line=(byte *)viewbuffer;
 for (y=0,y2=header.height;y<y2;y++)
  {
   packets=getbyte();
   for (p=0;p<packets;p++)
    {
     count=getbyte();
     if (count<0)               // uncompressed
      for (i=0,j=-count;i<j;i++,line++) 
       *line=getbyte();
     else                       // compressed
      {
       data=getbyte();          // byte to repeat
       for (i=0;i<count;i++)
	*line++=data;
       }
     }
   }
 }


void fli_linecompression(void)
/* normal line runlength compression type chunk */
{
 int      i, j, p;
 word     y, y2;
 shortint count;
 byte     data, packets;
 byte     *line;

 y=getword();                // start y
 y2=getword();               // number of lines to change
 for (y2+=y;y<y2;y++)
  {
   /* The movie's own width, not viewylookup and not a 320 constant: the FLI
      decoder treats viewbuffer as a packed frame of header.width x
      header.height -- the whole-frame reader above walks it linearly and the
      blit below scales it to the chrome.  viewylookup rows are windowWidth
      apart, which is a different number again. */
   line=viewbuffer+y*header.width;
   packets=getbyte();
   for (p=0;p<packets;p++)
    {
     line+=getbyte();
     count=getbyte();
     if (count<0)            // uncompressed
      {
       data=getbyte();
       for (i=0,j=-count;i<j;i++,line++)
	*line=data;
       }                     // compressed
     else for (i=0;i<count;i++,line++)
      *line=getbyte();
     }
   }
 }


void fli_readframe(FILE *f)
/* process each frame, chunk by chunk */
{
 chunktype   chunk;
 frameheader frame;
 int         i;

 if (!fread(&frame,sizeof(frame),1,f) || frame.signature!=0xF1FA)
  MS_Error("FLI_ReadFrame: Error Reading Frame!");
 if (frame.size==0)
  return;
 for(i=0;i<frame.nchunks;i++)
  {
   if (!fread(&chunk,sizeof(chunk),1,f))
    MS_Error("FLI_ReadFram: Error Reading Chunk Header!");
   if (!fread(chunkbuf,chunk.size-6,1,f))
    MS_Error("FLI_ReadFram: Error with Chunk Read!");
   bufptr=0;
   switch (chunk.type)
    {
     case 12:  // fli line compression
      fli_linecompression();
      break;
     case 15:  // fli line compression first time (only once at beginning)
      fli_brun();
      break;
     case 16:  // copy chunk
      memcpy(viewbuffer,chunkbuf,(size_t)header.width*header.height);
      break;
     case 11:  //  new palette
      fli_readcolors();
      break;
     case 13:  //  clear (only 1 usually at beginning)
      memset(viewbuffer,0,(size_t)header.width*header.height);
      break;
     }
   }
 }


bool CheckTime(int n1, int n2)
/* check timer update (70/sec) 
    this is for loop optimization in watcom c */
{
 if (n1<n2) 
  return false;
 return true;
 }


bool playfli(char *fname,longint offset)
/* play FLI out of BLO file
    load FLI header
     set timer
     read frame
     copy frame to screen
     reset timer
     dump out if keypressed or mousereleased */
{
 FILE    *f;
 longint delay;

 newascii=false;
 memset(screen,0,SCREENBYTES);
 VI_FillPalette(0,0,0);
 f=fopen(fname,"rb");
 if (f==NULL) 
  MS_Error("PlayFLI: File Not Found: %s",fname);
 if (fseek(f,offset,0) || !fread(&header,sizeof(fliheader),1,f))
  MS_Error("PlayFLI: File Read Error: %s",fname);
 /* Sized from the header rather than a fixed 64000: a 4x cutscene is 1280x800,
    and a COPY chunk carries a whole uncompressed frame.  The header has to be
    read first, which is why this moved below the fread. */
 if ((longint)header.width*header.height > (longint)MAX_VIEW_WIDTH*MAX_VIEW_HEIGHT)
  MS_Error("PlayFLI: %s is %dx%d, larger than the view buffer",
	    fname,header.width,header.height);
 chunkbuf=(byte *)malloc((size_t)header.width*header.height*2);
 if (chunkbuf==NULL) 
  MS_Error("PlayFLI: Out of Memory with ChunkBuf!");
 currentfliframe=0;
 delay=timecount;
 while (currentfliframe++<header.nframes && !newascii && !quitgame) // newascii=user break
  {
   delay+=header.speed;                   // set timer
   fli_readframe(f);
   /* Was `while (!CheckTime(timecount,delay)) ;` -- an empty spin, which only
      ever terminated because a timer interrupt advanced timecount behind the
      loop's back.  With a main-thread tick that is an unconditional hang, and
      it also has to pump for newascii above to ever break the outer loop. */
   while (!CheckTime(timecount,delay))
    {
     Sys_PumpFrame();
     if (quitgame) break;
     }
   /* Scale into the chrome rather than copying at the wrong pitch: a movie is
      whatever size its own header says.  The shipped set is 640x400, upscaled
      offline by tools/hdtex, so this point-doubles it into the 1280x800 HD
      chrome and point-samples it back down to 320x200 in the original one. */
   VI_BlitLogical(viewbuffer,header.width,header.height);
   }
 fclose(f);
 free(chunkbuf);
 if (currentfliframe<header.nframes) // user break
  {
   memset(screen,0,SCREENBYTES);
   return false;
   }
 else return true;
 }
