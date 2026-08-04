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
#pragma pack(pop,packing)

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
