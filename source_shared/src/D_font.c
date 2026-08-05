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

#include <STDARG.H>
#include <STDIO.H>
#include <STRING.H>
#include "d_global.h"
#include "d_video.h"
#include "d_misc.h"
#include "d_font.h"
#include "d_ints.h"
#include "r_public.h"
#include "protos.h"


/**** VARIABLES ****/

#define MSGQUESIZE 3

font_t  *font;
int     fontbasecolor;
int     fontspacing=1;
char    str[MAXPRINTF];  // general purpose string buffer
char    msgstr[MSGQUESIZE][MAXPRINTF];
int     printx, printy;  // the printing position (top left corner)
longint msgtime;


/**** FUNCTIONS ****/

void FN_RawPrint4(char *str)
/* Draws a string of characters to the buffer */
{
 byte b;
 byte *dest, *source;
 int  width, height, y, oldpx, yh;
 char ch;

 oldpx=printx;
 dest=hudylookup[printy]+printx;
 height=font->height;
 while ((ch=*str++)!=0)
  {
   width=font->width[ch];
   source=((byte*)font) + font->charofs[ch];
   while (width--)
    {
     for (y=0,yh=0;y<height;y++,yh+=hudWidth)
      {
       b=*source++;
       if (b) dest[yh]=fontbasecolor+b;
	else dest[yh]=0;
       }
     dest++;
     printx++;
     }
   dest+=fontspacing;
   printx+=fontspacing;
   }
 }


void FN_RawPrint2(char *str)
/* Draws a string of characters to the buffer */
{
 byte b;
 byte *dest, *source;
 int  width, height, y, oldpx, yh;
 char ch;

 oldpx=printx;
 dest=hudylookup[printy]+printx;
 height=font->height;
 while ((ch=*str++)!=0)
  {
   width=font->width[ch];
   source=((byte*)font) + font->charofs[ch];
   while (width--)
    {
     for (y=0,yh=0;y<height;y++,yh+=hudWidth)
      {
       b=*source++;
       if (b) dest[yh]=fontbasecolor+b;
       }
     dest++;
     printx++;
     }
   dest+=fontspacing;
   printx+=fontspacing;
   }
 }


void FN_RawPrint(char *str)
/* Draws a string of characters to the screen */
{
 byte b;
 byte *dest, *source;
 int  width, height, y, oldpx, yh;
 char ch;

 oldpx=printx;
 dest=ylookup[printy]+printx;
 height=font->height;
 while ((ch=*str++) != 0)
  {
   width=font->width[ch];
   source=((byte *)font) + font->charofs[ch];
   while (width--)
    {
     for (y=0,yh=0;y<height;y++,yh+=320)
      {
       b=*source++;
       if (b) dest[yh]=fontbasecolor+b;
	else dest[yh]=0;
       }
     dest++;
     printx++;
     }
   dest+=fontspacing;
   printx+=fontspacing;
   }
 }


void FN_RawPrint3(char *str)
/* Draws a string of characters to the screen */
{
 byte b;
 byte *dest, *source;
 int  width, height, y, oldpx, yh;
 char ch;

 oldpx=printx;
 dest=ylookup[printy]+printx;
 height=font->height;
 while ((ch=*str++) != 0)
  {
   width=font->width[ch];
   source=((byte *)font) + font->charofs[ch];
   while (width--)
    {
     for (y=0,yh=0;y<height;y++,yh+=320)
      {
       b=*source++;
       if (b) dest[yh]=fontbasecolor+b;
       }
     dest++;
     printx++;
     }
   dest+=fontspacing;
   printx+=fontspacing;
   }
 }


int FN_RawWidth(char *str)
/* Returns the width of a string
   Does NOT handle newlines     */
{
 int width;

 width=0;
 while (*str)
  {
   width+=font->width[*str++];
   width+=fontspacing;
   }
 return width;
 }


/* Copy one '\n'-delimited line out of s into line[], returning the position
   of the terminator so the caller can step past it.

   The originals split the string in place -- write '\0' over the newline,
   print, then put the character back.  Watcom put string literals in writable
   memory so that worked; on macOS they are in read-only __TEXT and the store
   is a segfault.  Every caller of FN_Print and FN_PrintCentered passes a
   literal, so both copy instead of writing to the caller's buffer. */
static char *fn_copyline(char *s, char *line)
{
 char *se=s;
 unsigned n;

 while (*se && *se!='\n') se++;
 n=(unsigned)(se-s);
 if (n>=MAXPRINTF) n=MAXPRINTF-1;
 memcpy(line,s,n);
 line[n]='\0';
 return se;
 }


void FN_Print(char  *s)
/* Prints a string in the current window, with newlines
   going down a line and back to 0 */
{
 char     line[MAXPRINTF], *se;
 unsigned h;

 h=font->height;
 while (*s)
  {
   se=fn_copyline(s,line);
   FN_RawPrint(line);
   s=se;
   if (*s)
    {
     s++;
     printx=0;
     printy+=h;
     }
   }
 }


void FN_PrintCentered(char  *s)
/* Prints a multi line string with each line centered */
{
 char     line[MAXPRINTF], *se;
 unsigned w, h;

 h=font->height;
 while (*s)
  {
   se=fn_copyline(s,line);   /* not in place -- see fn_copyline */
   w=FN_RawWidth(line);
   printx=(320-w)/2;
   FN_RawPrint3(line);
   s=se;
   if (*s)
    {
     s++;
     printx=0;
     printy+=h;
     }
   }
 }


void FN_Printf(char *fmt, ...)
/* Prints a printf style formatted string at the current print position
    using the current print routines  */
{
 va_list argptr;
 int     cnt;

 va_start(argptr,fmt);
 cnt=vsprintf(str,fmt,argptr);
 va_end(argptr);
#ifdef PARMCHECK
 if (cnt>=MAXPRINTF) MS_Error("FN_Printf: String too long: %s",fmt);
#endif
 FN_Print(str);
 }


void FN_CenterPrintf(char *fmt, ...)
/* As FN_Printf, but centers each line of text in the window bounds */
{
 va_list argptr;
 int     cnt;

 va_start(argptr,fmt);
 cnt=vsprintf(str,fmt,argptr);
 va_end(argptr);
#ifdef PARMCHECK
 if (cnt>=MAXPRINTF) MS_Error("FN_CPrintf: String too long: %s",fmt);
#endif
 FN_PrintCentered(str);
 }


void FN_BlockCenterPrintf(char *fmt, ...)
/* As FN_CenterPrintf, but also enters the entire set of lines vertically in
   the window bounds */
{
 va_list argptr;
 int     cnt;
 char    *s;
 int     height;

 va_start(argptr,fmt);
 cnt=vsprintf(str,fmt,argptr);
 va_end(argptr);
#ifdef PARMCHECK
 if (cnt>=MAXPRINTF) MS_Error("FN_CCPrintf: String too long: %s",fmt);
#endif
 height=1;
 s=str;
 while (*s) if (*s++=='\n') height++;
 height*=font->height;
 printy=0+(200-height)/2;
 FN_PrintCentered(str);
 }


void rewritemsg(void)
/* write the current msg to the view buffer */
{
 int i;

 fontbasecolor=73;
 font=font1;
 for(i=0;i<MSGQUESIZE;i++)
  if (msgstr[i][0])
   {
    printx=2;
    printy=1+i*6;
    FN_RawPrint2(msgstr[i]);
    }
 if (timecount>msgtime)
  {
   for(i=1;i<MSGQUESIZE;i++)
    strcpy(msgstr[i-1],msgstr[i]);
   msgstr[MSGQUESIZE-1][0]=0;
   msgtime=timecount+MSGTIME;
   }
 }


void writemsg(char *s)
/* update current msg */
{
 int i;

 if (msgstr[MSGQUESIZE-1][0]!=0)
  for(i=1;i<MSGQUESIZE;i++)
   strcpy(msgstr[i-1],msgstr[i]);
 strcpy(msgstr[MSGQUESIZE-1],s);
 msgtime=timecount+MSGTIME; // 10 secs
 }

