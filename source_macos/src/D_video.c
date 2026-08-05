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

#include <STRING.H>
#include <STDLIB.H>
#include <CONIO.H>
#include "d_global.h"
#include "d_ints.h"
#include "d_video.h"
#include "d_misc.h"
#include "d_disk.h"
#include "r_public.h"
#include "r_refdef.h"

/**** VARIABLES ****/

/* screen is the 320x200 8-bit framebuffer, TOP-DOWN, exactly as VGA mode 13h
   was: ylookup[y] = screen + y*320, and everything -- menus, status bar, FLI
   frames, the blitted 3D view -- is written top-down.

   Do not reintroduce a flip here.  The Win32 port's RF_BlitView copied
   viewylookup[199-i] into ylookup[i], which reads like an engine invariant but
   is not: it was compensation for GDI, whose CreateDIBSection with a positive
   biHeight gives a bottom-up surface.  The DOS original (RA_DRAW.ASM,
   RF_BlitView) is a plain `rep movsd` straight into VGA memory with no
   reversal at all.  Keeping the Win32 reversal *and* flipping again at present
   time cancelled out for the 3D view while leaving everything else upside
   down. */
byte *		screen;
byte *		ylookup[SCREENHEIGHT];
byte *		transparency;
byte *		translookup[255];

extern SoundCard SC;


/**** FUNCTIONS ****/

/* VI_Init, VI_BlitView, VI_SetPalette, VI_ResetPalette, VI_GetPalette and
   VI_FillPalette live in platform/sys_video.c -- they were the GDI half of
   this file.  Everything below is portable and unchanged. */

void VI_FadeOut(int start,int end,int red,int green,int blue,int steps)
{
 byte        basep[768];
 signed char px[768], pdx[768], dx[768];
 int         i, j;

 VI_GetPalette(basep);
 memset(dx,0,768);
 for(j=start;j<end;j++)
  {
   pdx[j*3]=(basep[j*3]-red)%steps;
   px[j*3]=(basep[j*3]-red)/steps;
   pdx[j*3+1]=(basep[j*3+1]-green)%steps;
   px[j*3+1]=(basep[j*3+1]-green)/steps;
   pdx[j*3+2]=(basep[j*3+2]-blue)%steps;
   px[j*3+2]=(basep[j*3+2]-blue)/steps;
   }
 start*=3;
 end*=3;
 for (i=0;i<steps;i++)
  {
   for (j=start;j<end;j++)
    {
     basep[j]-=px[j];
     dx[j]+=pdx[j];
     if (dx[j]>=steps)
      {
       dx[j]-=steps;
       --basep[j];
       }
     else if (dx[j]<=-steps)
      {
       dx[j]+=steps;
       ++basep[j];
       }
     }
   Wait(1);
   VI_SetPalette(basep);
   }
 VI_FillPalette(red,green,blue);
 }


void VI_FadeIn(int start,int end,byte *palette,int steps)
{
 byte        basep[768], work[768];
 signed char px[768], pdx[768], dx[768];
 int         i, j;

 VI_GetPalette(basep);
 memset(dx,0,768);
 memset(work,0,768);
 start*=3;
 end*=3;
 for(j=start;j<end;j++)
  {
   pdx[j]=(palette[j]-basep[j])%steps;
   px[j]=(palette[j]-basep[j])/steps;
   }
 for (i=0;i<steps;i++)
  {
   for (j=start;j<end;j++)
    {
     work[j]+=px[j];
     dx[j]+=pdx[j];
     if (dx[j]>=steps)
      {
       dx[j]-=steps;
       ++work[j];
       }
     else if (dx[j]<=-steps)
      {
       dx[j]+=steps;
       --work[j];
       }
     }
   Wait(1);
   VI_SetPalette(work);
   }
 VI_SetPalette(palette);
 }


void VI_DrawPic(int x,int y,pic_t * pic)
{
	byte *	dest;
	byte *	source;
	int		width;
	int		height;

	width = pic->width;
	height = pic->height;
	source = &pic->data;
	dest = ylookup[y] + x;

	while ( height-- )
	{
		memcpy(dest,source,width);
		dest += SCREENWIDTH;
		source += width;
	}
}


void VI_DrawMaskedPic(int x, int y, pic_t  *pic)
/* Draws a formatted image to the screen, masked with zero */
{
 byte *dest, *source;
 int  width, height, xcor;

 x -= pic->orgx;
 y -= pic->orgy;
 height=pic->height;
 source=&pic->data;
 while (y<0)
  {
   source+=pic->width;
   height--;
   y++;
   }
 while (height--)
  {
   if (y<200)
    {
     dest=ylookup[y]+x;
     xcor=x;
     width=pic->width;
     while (width--)
      {
       if (xcor>=0 && xcor<=319 && *source) *dest=*source;
       xcor++;
       source++;
       dest++;
       }
     }
   y++;
   }
 }


void VI_DrawTransPicToBuffer(int x,int y,pic_t *pic)
/* Draws a transpartent, masked pic to the view buffer */
{
 byte *dest,*source;
 int  width,height;

 x -= pic->orgx;
 y -= pic->orgy;
 height=pic->height;
 source=&pic->data;
 while (y<0)
  {
   source+=pic->width;
   height--;
   y++;
   }
 while (height-->0)
  {
   if (y<hudHeight)
    {
     dest=hudylookup[y]+x;
     width=pic->width;
     while (width--)
      {
       if (*source) *dest=*(translookup[*source-1]+*dest);
       source++;
       dest++;
       }
     }
   y++;
   }
 }


void VI_DrawMaskedPicToBuffer2(int x,int y,pic_t *pic)
/* Draws a masked pic to the view buffer */
{
 byte *dest, *source, *colormap;
 int  width, height, maplight;

// x-=pic->orgx;
// y-=pic->orgy;
 height=pic->height;
 source=&pic->data;

 wallshadow=mapeffects[player.mapspot];
 if (wallshadow==0)
  {
   maplight=((int)maplights[player.mapspot]<<3)+reallight[player.mapspot];
   if (maplight<0) colormap=zcolormap[0];
    else if (maplight>MAXZLIGHT) colormap=zcolormap[MAXZLIGHT];
    else colormap=zcolormap[maplight];
   }
 else if (wallshadow==1) colormap=colormaps+(wallglow<<8);
 else if (wallshadow==2) colormap=colormaps+(wallflicker1<<8);
 else if (wallshadow==3) colormap=colormaps+(wallflicker2<<8);
 else if (wallshadow==4) colormap=colormaps+(wallflicker3<<8);
 else if (wallshadow>=5 && wallshadow<=8)
  {
   if (wallcycle==wallshadow-5) colormap=colormaps;
   else
    {
     maplight=((int)maplights[player.mapspot]<<3)+reallight[player.mapspot];
     if (maplight<0) colormap=zcolormap[0];
      else if (maplight>MAXZLIGHT) colormap=zcolormap[MAXZLIGHT];
      else colormap=zcolormap[maplight];
     }
   }
 else if (wallshadow==9)
  {
   maplight=((int)maplights[player.mapspot]<<3)+reallight[player.mapspot]+wallflicker4;
   if (maplight<0) colormap=zcolormap[0];
    else if (maplight>MAXZLIGHT) colormap=zcolormap[MAXZLIGHT];
    else colormap=zcolormap[maplight];
   }
 if (height+y>hudHeight)
  height=hudHeight-y;
 while (height-->0)
  {
   dest=hudylookup[y]+x;
   width=pic->width;
   while (width--)
    {
     if (*source) *dest=*(colormap + *(source));
     source++;
     dest++;
     }
   y++;
   }
 }


void RF_BlitView(void)
/* Copy the rendered 3D view into the framebuffer.  Row i to row i -- the DOS
   version is a straight linear copy (RA_DRAW.ASM); the Win32 C rewrite's
   i/199-i reversal was a GDI bottom-up workaround, see the note above.

   This used to copy a fixed SCREENHEIGHT rows of SCREENWIDTH and ignore the
   view size entirely.  At any view smaller than full screen that is wrong in
   both directions: viewylookup rows are windowWidth apart, not 320, and rows
   past windowHeight still hold pointers from the previous SetViewSize.  The
   DOS original (RA_DRAW.ASM) takes the fast path only when the view is full
   width and otherwise steps row by row into windowTop/windowLeft, which is
   what this now does.

   In HD nothing is copied: the view buffer is presented directly as its own
   texture and `screen` carries only the 2D overlay. */
{
	int i;

	if (hdmode)
	{
		/* Nothing to copy: the view buffer goes to the screen as its own
		   texture and `screen` carries only the 2D chrome.  Just record that a
		   frame is ready, so VI_BlitView can tell the play loop from a menu or
		   a fade presenting `screen` directly. */
		hdviewfresh = 1;
		return;
	}

	if (windowWidth==SCREENWIDTH && windowHeight==SCREENHEIGHT)
	{
		for ( i = 0 ; i < SCREENHEIGHT ; i++ )
			memcpy(ylookup[i],viewylookup[i],SCREENWIDTH);
		return;
	}

	for ( i = 0 ; i < windowHeight ; i++ )
		memcpy(ylookup[windowTop+i]+windowLeft,viewylookup[i],windowWidth);
}


void VI_DrawMaskedPicToBuffer(int x,int y,pic_t *pic)
/* Draws a masked pic to the view buffer */
{
	byte *	dest;
	byte *	source;
	int		width,height,xcor;

	x -= pic->orgx;
	y -= pic->orgy;
	height = pic->height;
	source = &pic->data;
	while (y<0) 
	{
		source += pic->width;
		height--;
	}
	while (height--)
	{
		if (y<hudHeight)
		{
			/* Through hudylookup, not viewbuffer + y*MAX_VIEW_WIDTH as this
			   once was: that constant is the size the scratch array is declared
			   at, not the pitch of the rows being filled, and this artwork does
			   not live in the view buffer at all in HD. */
			dest = hudylookup[y] + x;
			xcor = x;
			width = pic->width;
			while (width--)
			{
				if ( ( xcor >= 0 ) && ( xcor < hudWidth ) )
				{
					if (*source) 
						*dest = *source;
				}
				xcor++;
				source++;
				dest++;
			}
		}
		y++;
	}
 }
