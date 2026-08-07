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

#ifndef D_VIDEO_H
#define D_VIDEO_H

/**** CONSTANTS ****/

#define SCREEN            0xa0000
#define SCREENWIDTH       320
#define SCREENHEIGHT      200

/* The 2D chrome -- status bar, weapon, HUD, menus, briefing screens -- is
   authored at 320x200 and every caller still passes coordinates in that space.
   With a 4x art pack the buffer behind it is hudscale times larger on each
   axis, and the blit primitives scale on the way in.  This is the ceiling the
   buffer and ylookup[] are sized to, not the current factor; hudscale is 1
   without an HD art pack and the whole thing reduces to the original. */
#define MAX_HUDSCALE      4
#define HUD_MAXWIDTH      (SCREENWIDTH*MAX_HUDSCALE)
#define HUD_MAXHEIGHT     (SCREENHEIGHT*MAX_HUDSCALE)


/**** VARIABLES ****/

#pragma pack(push,packing,1)
typedef struct
{
 short width, height;
 short orgx, orgy;
 byte  data;
 } pic_t;
#pragma pack(pop,packing)

extern byte *screen;
extern byte *ylookup[HUD_MAXHEIGHT];
/* Bytes in the live chrome buffer.  The engine spelled this 64000 in about
   fifty places; that is only right at hudscale 1. */
#define SCREENBYTES  (screenpitch*SCREENHEIGHT*hudscale)
/* Row pitch of `screen`, in bytes: SCREENWIDTH*hudscale.  Never use
   SCREENWIDTH as a stride -- it is the *logical* width. */
extern int  screenpitch;
extern byte *transparency;
extern byte *translookup[255];


/**** FUNCTIONS ****/

void VI_Init();
void VI_SetPalette(byte *palette);
void VI_GetPalette(byte *palette);
void VI_FillPalette(int red,int green,int blue);
void VI_FadeOut (int start,int end,int red,int green,int blue,int steps);
void VI_FadeIn(int start,int end,byte *pallete,int steps);
void VI_DrawPic(int x,int y,pic_t  *pic);
void VI_DrawPicToBuffer(int x,int y,pic_t *pic);
void VI_DrawMaskedPic(int x,int y,pic_t  *pic);
void VI_DrawMaskedPicToBuffer(int x,int y,pic_t *pic);
void VI_DrawMaskedPicToBuffer2(int x,int y,pic_t *pic);
void VI_DrawTransPicToBuffer(int x,int y,pic_t *pic);
void VI_BlitView();
void VI_ResetPalette();
/* Implemented by the platform layer (platform/sys_video.c). */
void VI_SetFullscreen(int on);
int  VI_GetFullscreen(void);
void VI_ApplyRenderMode(void);
/* Re-lay the chrome buffer for a new hudscale; see sys_video.c. */
void VI_SetHudScale(int scale);

/* Procedural chrome drawing in 320x200 logical coordinates; see D_video.c. */
void VI_HudFill(byte **lookup,int x,int y,int w,int h,int c);
void VI_HudPut(byte **lookup,int x,int y,int c);
/* Copy a packed sw x sh image into the chrome, point-scaled to fill it.  For
   content sized by its own format rather than by hudscale -- the FLI
   cutscenes, which are 320x200 originally and 1280x800 upscaled. */
void VI_BlitLogical(byte *src,int sw,int sh);
/* Read the representative pixel of a logical cell.  Valid because the art
   pipeline replicates marker indices 4x4, so the cell is uniform. */
#define VI_HUDPEEK(lookup,x,y) (*((lookup)[(y)*hudscale]+(x)*hudscale))

#endif

