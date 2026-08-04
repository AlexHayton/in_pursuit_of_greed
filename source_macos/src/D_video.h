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
extern byte *ylookup[SCREENHEIGHT];
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
void VI_DrawMaskedPic(int x,int y,pic_t  *pic);
void VI_DrawMaskedPicToBuffer(int x,int y,pic_t *pic);
void VI_DrawMaskedPicToBuffer2(int x,int y,pic_t *pic);
void VI_DrawTransPicToBuffer(int x,int y,pic_t *pic);
void VI_BlitView();
void VI_ResetPalette();

#endif

