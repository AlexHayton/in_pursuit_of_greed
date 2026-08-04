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

#include <STDLIB.H>
#include <MATH.H>
#include <STRING.H>
#include "d_global.h"
#include "d_disk.h"
#include "r_refdef.h"
#include "protos.h"
#include "d_ints.h"

/**** VARIABLES ****/

int     windowHeight = INIT_VIEW_HEIGHT;
int     windowWidth = INIT_VIEW_WIDTH;
int     windowLeft = 0;
int     windowTop = 0;
int     windowSize = INIT_VIEW_HEIGHT*INIT_VIEW_WIDTH;
/* A DOS leftover -- it once pointed at VGA memory at 0xA0000.  Nothing
   dereferences it any more, but it is still assigned (intptr_t)screen, so it
   has to be pointer-width. */
intptr_t viewLocation=0xA0000;
fixed_t CENTERX=INIT_VIEW_WIDTH/2;
fixed_t CENTERY=INIT_VIEW_HEIGHT/2;
fixed_t SCALE;
fixed_t ISCALE;
int     backtangents[TANANGLES*2];
int     autoangle2[MAXAUTO][MAXAUTO];
int     scrollmin, scrollmax, bloodcount, metalcount;
void    (*actionhook)(void);

extern SoundCard SC;

/**** FUNCTIONS ****/

/* The x86 originals kept the full 64-bit intermediate in edx:eax -- imul then
   shrd for the multiply, cdq/shld/sal then idiv for the divide.  Doing the
   intermediate in int64_t reproduces that exactly, without the asm. */

fixed_t FIXEDMUL(fixed_t num1,fixed_t num2)
{
	return (fixed_t)(((int64_t)num1 * (int64_t)num2) >> FRACBITS);
}


fixed_t FIXEDDIV(fixed_t num1,fixed_t num2)
{
	return (fixed_t)(((int64_t)num1 << FRACBITS) / (int64_t)num2);
}


void RF_PreloadGraphics(void)
{
 int i;
 int doorlump;

 // find the number of lumps of each type
 spritelump=CA_GetNamedNum("startsprites");
 numsprites=CA_GetNamedNum("endsprites")-spritelump;
 walllump=CA_GetNamedNum("startwalls");
 numwalls=CA_GetNamedNum("endwalls")-walllump;
 flatlump=CA_GetNamedNum("startflats");
 numflats=CA_GetNamedNum("endflats")-flatlump;
 doorlump=CA_GetNamedNum("door_1");
 printf(".");
 // load the lumps
 for (i=1; i<numsprites; i++)
  {
   DemandLoadMonster(spritelump+i,1);
//   CA_CacheLump(spritelump+i);
   if (i%50==0)
    {
     printf(".");
     if (newascii && lastascii==27) return;
     }

   }
 printf(".");
 if (!debugmode)
  for(i=doorlump;i<numwalls+walllump;i++) CA_CacheLump(i);
 else
  {
   CA_CacheLump(walllump+1);
   CA_CacheLump(flatlump+1);
   }
 printf(".");
 }


void RF_InitTargets(void)
{
 double  at, atf;
 int     j, angle, x1, y1, i;
 fixed_t x, y;

 memset(autoangle2,-1,sizeof(autoangle2));
 i=0;
 do
  {
   at=atan((double)i/(double)MAXAUTO);
   atf=at*(double)ANGLES/(2*PI);
   angle=rint(atf);
   for(j=0;j<MAXAUTO*2;j++)
    {
     y=FIXEDMUL(sintable[angle],j<<FRACBITS);
     x=FIXEDMUL(costable[angle],j<<FRACBITS);
     x1=x>>FRACBITS;
     y1=y>>FRACBITS;
     if (x1>=MAXAUTO || y1>=MAXAUTO || autoangle2[x1][y1]!=-1) continue;
     autoangle2[x1][y1]=angle;
     }
   i++;
   } while (angle<DEGREE45+DEGREE45_2);

 for(i=MAXAUTO-1;i>0;i--)
  for(j=0;j<MAXAUTO;j++)
   if (autoangle2[j][i]==-1) autoangle2[j][i]=autoangle2[j][i-1];
 for(i=MAXAUTO-1;i>0;i--)
  for(j=0;j<MAXAUTO;j++)
   if (autoangle2[j][i]==-1) autoangle2[j][i]=autoangle2[j][i-1];
 }


void InitTables(void)
/* Builds tangent tables for -90 degrees to +90 degrees
   and pixel angle table */
{
 double  tang, value, ivalue;
 int     intval, i;

 // tangent values for wall tracing
 for (i=0; i<TANANGLES/2; i++)
  {
   tang=(i+0.5)*PI/(TANANGLES*2);
//   tang=i*PI/(TANANGLES*2);
   value=tan(tang);
   ivalue=1/value;
   value=rint(value*FRACUNIT);
   ivalue=rint(ivalue*FRACUNIT);
   tangents[TANANGLES + i]=(int)(-value);
   tangents[TANANGLES + TANANGLES - 1 - i]=(int)(-ivalue);
   tangents[i]=(int)(ivalue);
   tangents[TANANGLES - 1 - i]=(int)(value);
   }
 // high precision sin / cos for distance calculations
 for (i=0; i<TANANGLES; i++)
  {
   tang=(i+0.5)*PI/(TANANGLES*2);
//   tang=i*PI/(TANANGLES*2);
   value=sin(tang);
   intval=rint(value*FRACUNIT);
   sines[i]=intval;
   sines[TANANGLES*4 + i]=intval;
   sines[TANANGLES*2 - 1 - i]=intval;
   sines[TANANGLES*2 + i]=-intval;
   sines[TANANGLES*4 - 1 - i]=-intval;
   }
 cosines=&sines[TANANGLES];
 for(i=0;i<TANANGLES*2;i++)
  backtangents[i]=((windowWidth/2)*tangents[i])>>FRACBITS;
 }


void InitReverseCam(void)
{
 int i, intval;

 for (i=0;i<65; i++)
  {
   intval=rint(atan(((double)32-((double)i+1.0))/(double)32)/(double)PI*(double)TANANGLES*(double)2);
   pixelangle[i]=intval;
   pixelcosine[i]=cosines[intval&(TANANGLES * 4 - 1)];
   }
 memcpy(campixelangle,pixelangle,sizeof(pixelangle));
 memcpy(campixelcosine,pixelcosine,sizeof(pixelcosine));
 }


void RF_Startup(void)
{
 int    i;
 double angle;
 int    lightlump;

 memset(framevalid, 0, sizeof(framevalid));
 printf(".");
 frameon=0;
 // trig tables
 for (i=0; i<=ANGLES; i++)
  {
   angle=(double)(i * PI * 2)/(double)(ANGLES + 1);
   sintable[i]=rint(sin(angle)*FRACUNIT);
   costable[i]=rint(cos(angle)*FRACUNIT);
   }
 printf(".");
 SetViewSize(windowWidth,windowHeight);
 // set up lights
 // Allocates a page aligned buffer and load in the light tables
 lightlump=CA_GetNamedNum("lights");
 numcolormaps=infotable[lightlump].size/256;
 colormaps=malloc((size_t)256*(numcolormaps+1));
 /* Round up to a 256-byte boundary; the +1 colormap in the malloc above is
    the slack that makes this safe.  Must round through uintptr_t -- as an int
    this truncated the heap pointer on LP64. */
 colormaps=(byte *)(((uintptr_t)colormaps+255)&~(uintptr_t)0xff);
 CA_ReadLump(lightlump, colormaps);
 RF_SetLights((fixed_t)MAXZ);
 RF_ClearWorld();
 printf(".");
 // initialize the translation to no animation
 /* These two are int*, so 4 bytes per element is still right on LP64 -- but
    say so explicitly rather than leaving a bare 4 next to the pointer-array
    allocations that were not. */
 flattranslation=malloc((size_t)(numflats+1)*sizeof(int));
 walltranslation=malloc((size_t)(numwalls+1)*sizeof(int));
 if (!debugmode)
  {
   for(i=0;i<=numflats;i++) flattranslation[i]=i;
   for(i=0;i<=numwalls;i++) walltranslation[i]=i;
   }
 else
  {
   for(i=1;i<=numflats;i++) flattranslation[i]=1;
   for(i=1;i<=numwalls;i++) walltranslation[i]=1;
   flattranslation[0]=0;
   walltranslation[0]=0;
   }
 actionhook=NULL;
 actionflag=0;
 RF_InitTargets();
 InitTables();
 printf(".");
 InitReverseCam();
 InitWalls();
 printf(".");
 }


void RF_ClearWorld(void)
{
 int i;

 firstscaleobj.prev=NULL;
 firstscaleobj.next=&lastscaleobj;
 lastscaleobj.prev=&firstscaleobj;
 lastscaleobj.next=NULL;
 freescaleobj_p=scaleobjlist;
 memset(scaleobjlist,0,sizeof(scaleobjlist));
 for(i=0;i<MAXSPRITES-1;i++) scaleobjlist[i].next=&scaleobjlist[i+1];
 firstelevobj.prev=NULL;
 firstelevobj.next=&lastelevobj;
 lastelevobj.prev=&firstelevobj;
 lastelevobj.next=NULL;
 freeelevobj_p=elevlist;
 memset(elevlist,0,sizeof(elevlist));
 for(i=0;i<MAXELEVATORS-1;i++) elevlist[i].next=&elevlist[i+1];
 numdoors=0;
 numspawnareas=0;
 bloodcount=0;
 metalcount=0;
 }


doorobj_t *RF_GetDoor(int tilex, int tiley)
{
 doorobj_t *door;

 if (numdoors==MAXDOORS) MS_Error("RF_GetDoor: Too many doors placed! (%i,%i)",numdoors,MAXDOORS);
 door=&doorlist[numdoors];
 numdoors++;
 door->tilex=tilex;
 door->tiley=tiley;
 mapflags[tiley*MAPROWS+tilex] |= FL_DOOR;
 return door;
 }


scaleobj_t *RF_GetSprite(void)
/* returns a new sprite */
{
 scaleobj_t *new;

 if (!freescaleobj_p) MS_Error("RF_GetSprite: Out of spots in scaleobjlist!");
 new=freescaleobj_p;
 freescaleobj_p=freescaleobj_p->next;
 memset(new,0,sizeof(scaleobj_t));
 new->next=(scaleobj_t *)&lastscaleobj;
 new->prev=lastscaleobj.prev;
 lastscaleobj.prev=new;
 new->prev->next=new;
 return new;
 }


elevobj_t *RF_GetElevator(void)
/* returns a elevator structure */
{
 elevobj_t *new;

 if (!freeelevobj_p) MS_Error("RF_GetElevator: Too many elevators placed!");
 new=freeelevobj_p;
 freeelevobj_p=freeelevobj_p->next;
 memset(new,0,sizeof(elevobj_t));
 new->next=(elevobj_t *)&lastelevobj;
 new->prev=lastelevobj.prev;
 lastelevobj.prev=new;
 new->prev->next=new;
 return new;
 }


spawnarea_t *RF_GetSpawnArea(void)
{
 if (numspawnareas==MAXSPAWNAREAS) MS_Error("RF_GetSpawnArea: Too many Spawn Areas placed! (%i,%i)",numspawnareas,MAXSPAWNAREAS);
 ++numspawnareas;
 return &spawnareas[numspawnareas-1];
 }


void Event(int e,bool send);


void RF_RemoveSprite(scaleobj_t *spr)
/* removes sprite from doublely linked list of sprites */
{
 spr->next->prev=spr->prev;
 spr->prev->next=spr->next;
 spr->next=freescaleobj_p;
 freescaleobj_p=spr;
 }


void RF_RemoveElevator(elevobj_t *e)
{
 e->next->prev=e->prev;
 e->prev->next=e->next;
 e->next=freeelevobj_p;
 freeelevobj_p=e;
 }


fixed_t RF_GetFloorZ(fixed_t x, fixed_t y)
{
 fixed_t h1, h2, h3, h4;
 int tilex, tiley, mapspot;
 int polytype;
 fixed_t fx, fy;
 fixed_t top, bottom, water;

 tilex=x>>(FRACBITS+TILESHIFT);
 tiley=y>>(FRACBITS+TILESHIFT);
 mapspot=tiley *MAPSIZE+tilex;
 polytype=(mapflags[mapspot]&FL_FLOOR)>>FLS_FLOOR;
 if (floorpic[mapspot]>=57 && floorpic[mapspot]<=59)
  water=-(20<<FRACBITS);
 else
  water=0;
 if (polytype==POLY_FLAT)
  return (floorheight[mapspot]<<FRACBITS) + water;
 h1=floorheight[mapspot]<<FRACBITS;
 h2=floorheight[mapspot+1]<<FRACBITS;
 h3=floorheight[mapspot+MAPSIZE]<<FRACBITS;
 h4=floorheight[mapspot+MAPSIZE+1]<<FRACBITS;
 fx=(x&(TILEUNIT-1))>>6; // range from 0 to fracunit-1
 fy=(y&(TILEUNIT-1))>>6;
 if (polytype==POLY_SLOPE)
  {
   if (h1==h2) return h1+FIXEDMUL(h3-h1, fy) + water;
    else return h1+FIXEDMUL(h2-h1, fx) + water;
   }
 // triangulated slopes
 // set the outside corner of the triangle that the point is NOT in s
 // plane with the other three
 if (polytype==POLY_ULTOLR)
  {
   if (fx>fy) h3=h1-(h2-h1);
    else h2=h1+(h1-h3);
   }
 else
  {
   if (fx<FRACUNIT-fy) h4=h2+(h2-h1);
    else h1=h2-(h4-h2);
   }
 top=h1+FIXEDMUL(h2-h1, fx);
 bottom=h3+FIXEDMUL(h4-h3, fx);
 return top+FIXEDMUL(bottom-top, fy) + water;
 }


fixed_t RF_GetCeilingZ(fixed_t x, fixed_t y)
/* find how high the ceiling is at x,y */
{
 fixed_t h1, h2, h3, h4;
 int     tilex, tiley, mapspot;
 int     polytype;
 fixed_t fx, fy;
 fixed_t top, bottom;

 tilex=x>>(FRACBITS+TILESHIFT);
 tiley=y>>(FRACBITS+TILESHIFT);
 mapspot=tiley *MAPSIZE+tilex;
 polytype=(mapflags[mapspot]&FL_CEILING)>>FLS_CEILING;
 // flat
 if (polytype==POLY_FLAT) return ceilingheight[mapspot]<<FRACBITS;
 // constant slopes
 if (polytype==POLY_SLOPE)
  {
   h1=ceilingheight[mapspot]<<FRACBITS;
   h2=ceilingheight[mapspot+1]<<FRACBITS;
   if (h1==h2)
    {
     h3=ceilingheight[mapspot+MAPSIZE]<<FRACBITS;
     fy=(y&(TILEUNIT-1))>>6;
     return h1+FIXEDMUL(h3-h1, fy); // north/south slope
     }
   else
    {
     fx=(x&(TILEUNIT-1))>>6;
     return h1+FIXEDMUL(h2-h1, fx); // east/west slope
     }
   }
 // triangulated slopes
 // set the outside corner of the triangle that the point is NOT in s
 // plane with the other three
 h1=ceilingheight[mapspot]<<FRACBITS;
 h2=ceilingheight[mapspot+1]<<FRACBITS;
 h3=ceilingheight[mapspot+MAPSIZE]<<FRACBITS;
 h4=ceilingheight[mapspot+MAPSIZE+1]<<FRACBITS;
 fx=(x&(TILEUNIT-1))>>6; // range from 0 to fracunit-1
 fy=(y&(TILEUNIT-1))>>6;
 if (polytype==POLY_ULTOLR)
  {
   if (fx>fy) h3=h1-(h2-h1);
    else h2=h1+(h1-h3);
   }
 else
  {
   if (fx<FRACUNIT-fy) h4=h2+(h2-h1);
    else h1=h2-(h4-h2);
   }
 top=h1+FIXEDMUL(h2-h1, fx);
 bottom=h3+FIXEDMUL(h4-h3, fx);
 return top+FIXEDMUL(bottom-top, fy);
 }


void RF_SetActionHook(void (*hook)(void))
{
 actionhook=hook;
 actionflag=1;
 }

void r_publicstub2(void)
{
 }


void RF_SetLights(fixed_t blackz)
/* resets the color maps to new lighting values */
{
 // linear diminishing, table is actually logrithmic
 int     i, table;

 blackz>>=FRACBITS;
 for (i=0;i<=MAXZ>>FRACBITS;i++)
  {
   table=numcolormaps * i/blackz;
   if (table>=numcolormaps) table=numcolormaps-1;
   zcolormap[i]=colormaps+table*256;
   }
 }


void RF_CheckActionFlag(void)
{
 if (SC.vrhelmet==0)
  TimeUpdate();
 if (!actionflag) return;
 actionhook();
 actionflag=0;
 }


void RF_RenderView(fixed_t x, fixed_t y, fixed_t z, int angle)
{
//#ifdef VALIDATE
// if (x<=0 || x>=((MAPSIZE-1)<<(FRACBITS+TILESHIFT)) || y<=0 ||
//  y>=((MAPSIZE-1)<<(FRACBITS+TILESHIFT)))
//  MS_Error("Invalid RF_RenderView (%p, %p, %p, %i)\n", x, y, z, angle);
//#endif

// viewx=(x&~0xfff) + 0x800;
// viewy=(y&~0xfff) + 0x800;
// viewz=(z&~0xfff) + 0x800;

 viewx=x;
 viewy=y;
 viewz=z;
 viewangle=angle&ANGLES;
 RF_CheckActionFlag();
 SetupFrame();
 RF_CheckActionFlag();
 FlowView();
 RF_CheckActionFlag();
 RenderSprites();
 DrawSpans();
 RF_CheckActionFlag();
 }


void SetViewSize(int width, int height)
{
 int i;

 if (width>MAX_VIEW_WIDTH) width = MAX_VIEW_WIDTH;
 if (height>MAX_VIEW_HEIGHT) height = MAX_VIEW_HEIGHT;
 windowHeight = height;
 windowWidth = width;
 windowSize = width*height;
 scrollmax=windowHeight+scrollmin;
 CENTERX=width/2;
 CENTERY=height/2;
 SCALE=(width/2)<<FRACBITS;
 ISCALE=FRACUNIT/(width/2);

 for (i=0;i<height;i++)
  viewylookup[i] = viewbuffer + i * width;

// slopes for rows and collumns of screen pixels
// slightly biased to account for the truncation in coordinates
 for(i=0;i<=width;i++)
  xslope[i]=rint((float)(i+1-CENTERX)/CENTERX*FRACUNIT);
 for(i=-MAXSCROLL;i<height+MAXSCROLL;i++)
  yslope[i+MAXSCROLL] = rint(-(float)(i-0.5-CENTERY)/CENTERX*FRACUNIT);
 for(i=0;i<TANANGLES*2;i++)
  backtangents[i]=((width/2)*tangents[i])>>FRACBITS;
 hfrac=FIXEDDIV(BACKDROPHEIGHT<<FRACBITS,(windowHeight/2)<<FRACBITS);
 afrac=FIXEDDIV(TANANGLES<<FRACBITS,width<<FRACBITS);
 }
