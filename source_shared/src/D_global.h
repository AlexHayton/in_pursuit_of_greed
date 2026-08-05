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

#ifndef GLOBAL_H
#define GLOBAL_H

//#define PARMCHECK
//#define VALIDATE

typedef unsigned char byte;
typedef unsigned short word;
/* Must stay exactly 32 bits.  longint appears inside #pragma pack(1) structs
   that are read straight off disk (Playfli.c's fliheader/frameheader) and in
   player_t, and timecount relies on 32-bit wraparound.  On LP64 an
   "unsigned long" would silently be 8 bytes and corrupt all three. */
typedef unsigned int longint;
typedef enum{false,true} bool;

#endif



