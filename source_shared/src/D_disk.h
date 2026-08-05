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

#ifndef DISK_H
#define DISK_H

#include <STDIO.H>

/**** TYPES ****/

#pragma pack(push,packing,1)

typedef struct            // must be noaligned, or the first
{                         // short will be padded to 4 bytes
 short numlumps;
 int   infotableofs;
 int   infotablesize;
 } fileinfo_t;

typedef struct
{
 int      filepos;
 unsigned size;
 short    nameofs;
 short    compress;
 } lumpinfo_t;

#pragma pack(pop,packing)

/**** VARIABLES ****/

extern fileinfo_t fileinfo;     // the file header
extern lumpinfo_t *infotable;   // pointers into the cache file
extern void       **lumpmain;   // pointers to the lumps in main memory
extern int        cachehandle;  // handle of current file



/**** FUNCTIONS ****/

void CA_ReadFile(char *name,void *buffer,unsigned length);
void *CA_LoadFile(char *name);
void CA_InitFile(char *filename);
int  CA_CheckNamedNum(char *name);
int  CA_GetNamedNum(char *name);
void *CA_CacheLump(int lump);
void CA_ReadLump(int lump,void *dest);
void CA_FreeLump(unsigned lump);
void CA_WriteLump(unsigned lump);
void CA_OpenDebug(void);
void CA_CloseDebug(void);

#endif


