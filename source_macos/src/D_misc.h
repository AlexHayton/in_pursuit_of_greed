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

#ifndef MISC_H
#define MISC_H

 /**** VARIABLES ****/

extern int  my_argc;
extern char **my_argv;
extern int  rndofs;
extern byte rndtable[512];


 /**** FUNCTIONS ****/

#define MS_RndT() (byte)rndtable[(++rndofs)&511]
int  MS_CheckParm (char *check);
void MS_Error (char *error, ...);
void MS_ExitClean (void);

#endif
