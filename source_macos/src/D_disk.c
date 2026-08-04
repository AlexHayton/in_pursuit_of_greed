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
   lseek(cachehandle,infotable[lump].filepos,SEEK_SET);
   if (waiting) UpdateWait();
   read(cachehandle,lumpmain[lump],infotable[lump].size);
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
 lseek(cachehandle, infotable[lump].filepos, SEEK_SET);
 read(cachehandle,dest,infotable[lump].size);
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

