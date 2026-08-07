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

#ifndef __FONT__
#define __FONT__

/**** CONSTANTS ****/

#define MAXPRINTF 256
#define MSGTIME   350


/**** TYPES ****/

#pragma pack(push,packing,1)
typedef struct
{
 short height;
 char  width[256];
 short charofs[256];
 } font_t;

/* The widened font a 4x art pack uses.  charofs is int16 in the original and
   font1/font2 reach 33506 and 37250 bytes at 4x, just past its 32767 limit, so
   the offset table has to grow.  height and width[] keep their types and
   offsets, so only charofs needs a branch.

   hudscale selects between them: it is 4 only when the pack actually carries
   chrome art, and pack.py refuses to claim that unless both the pics and the
   fonts are present. */
typedef struct
{
 short height;
 char  width[256];
 int   charofs[256];
 } fonthd_t;
#pragma pack(pop,packing)

#define FN_CHAROFS(f,c) (hudscale>1 ? ((fonthd_t *)(f))->charofs[c]           \
				    : (int)((font_t *)(f))->charofs[c])

/* The font's line height in 320x200 LOGICAL units.

   font->height is in chrome pixels, so with a 4x pack it is four times what
   every caller written against the original expects.  printx/printy are logical
   and the print routines scale them, so anything that steps printy by a line,
   or centres text against the logical 200, has to use this instead -- otherwise
   lines advance four times too far and walk off the end of ylookup. */
#define FN_LINEHEIGHT   (font->height/hudscale)

/**** VARIABLES ****/

extern font_t  *font;
extern int     fontbasecolor;
extern int     fontspacing;
extern int     printx,printy;
extern longint msgtime;


/**** FUNCTIONS ****/

void FN_RawPrint(char *str);
void FN_RawPrint2(char *str);
void FN_RawPrint3(char *str);
void FN_RawPrint4(char *str);
int  FN_RawWidth(char *str);
void FN_Printf(char *fmt,...);
void FN_PrintCentered(char  *s);
void FN_CenterPrintf(char *fmt,...);
void FN_BlockCenterPrintf(char *fmt,...);
void rewritemsg(void);
void writemsg(char *s);

#endif
