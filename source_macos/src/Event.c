
#include <STDLIB.H>
#include <STRING.H>
#include <STDIO.H>
#include <DOS.H>
#include <FCNTL.H>
#include <IO.H>
#include <TIME.H>
#include "d_global.h"
#include "r_public.h"
#include "r_refdef.h"
#include "protos.h"
#include "d_disk.h"
#include "d_ints.h"
#include "d_misc.h"

/**** CONSTANTS ****/

#define MAXZONES         255
#define ACTIVATIONTYPE   0
#define MAPZONETYPE      1
#define SPAWNTYPE        2
#define TRIGGERTYPE      3
#define SOUNDTYPE        4
#define FLITYPE          5
#define AREATRIGGERTYPE  6

#define MAXPROCESS       128

#define NORTHWALL        0
#define NORTHFLAGS       1
#define WESTWALL         2
#define WESTFLAGS        3
#define FLOOR            4
#define FLOORFLAGS       5
#define CEILING          6
#define CEILINGFLAGS     7
#define FLOORHEIGHT      8
#define CEILINGHEIGHT    9
#define FLOORDEF         10
#define FLOORDEFFLAGS    11
#define CEILINGDEF       12
#define CEILINGDEFFLAGS  13
#define LIGHTS           14
#define EFFECTS          15
#define SPRITES          16
#define SLOPES           17

#define CHECKERROR(n)                                                               \
	 if (result!=n)                                                             \
	  MS_Error("Must have %i parms on line %i. %i parms found.",n,line,result);

/**** TYPES ****/

typedef struct
 {
  int x1, y1, x2, y2;
  int eval, endeval;
  int rate, layer, newvalue, stype, removeable;
  int type;
  } zone_t;


/**** VARIABLES ****/

byte   triggers[MAPROWS][MAPCOLS], switches[MAPROWS][MAPCOLS];
int    processes[MAXPROCESS];
int    numprocesses, numzones, fliplayed;
extern int cdr_drivenum;
zone_t zones[MAXZONES];
char  *fliname[]=
 {
  "JETTISON.FLI",
  "TMPL_DIE.FLI",
  "KAAL_DIE.FLI",
  };
extern bool eventloading;
extern SoundCard SC;


/**** FUNCTIONS ****/

void AddProcess(int n)
{
 int i;

 if (numprocesses==MAXPROCESS)
  MS_Error("AddProcess: Too many active processes!");
 i=0;
 while (i<MAXPROCESS && processes[i]!=0) i++;
 if (i==MAXPROCESS)
  MS_Error("AddProcess: Process array overflow");
 processes[i]=1000+n;
 numprocesses++;
 }


void Event(int eval,bool netsend)
{
 int        i, j;
 elevobj_t  *elev_p;
 scaleobj_t *sp;
 int        x, y;
 fixed_t    x1, y1;
 byte      *vrscr;
#ifndef DEMO
 char       name[64];
#endif

// FILE *debug;


 if (netsend && netmode)
  NetEvent(eval);
 if (eval<256)
  {
   player.events[eval]=1;
   if (eval>200 && (eval&1)==0)
    player.events[eval-1]=0;
   }
 for (i=0;i<MAPCOLS;i++)
  for (j=0;j<MAPROWS;j++)
   if (triggers[j][i]==eval)
    triggers[j][i]=0;
 for (i=0;i<MAPCOLS;i++)
  for (j=0;j<MAPROWS;j++)
   if (switches[j][i]==eval)
    switches[j][i]=0;

 for(elev_p=firstelevobj.next;elev_p!=&lastelevobj;elev_p=elev_p->next)
  if (elev_p->eval==eval && eval>0)
   {
    if (elev_p->position==elev_p->floor)
     {
      elev_p->elevUp=true;
      elev_p->eval=0;
      elev_p->elevTimer=timecount;
      SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
      }
    else if (elev_p->position==elev_p->ceiling)
     {
      elev_p->elevDown=true;
      elev_p->eval=0;
      elev_p->elevTimer=timecount;
      SoundEffect(SN_ELEVATORSTART,15,(elev_p->mapspot&63)<<FRACTILESHIFT,(elev_p->mapspot>>6)<<FRACTILESHIFT);
      }
    }

// if (eval==50)
//  {
//   debug=fopen("debug.txt","wt");
//   for (i=0;i<numzones;i++)
//    fprintf(debug,"zone:%i eval:%i type:%i\n",i,zones[i].eval,zones[i].type);
//   fclose(debug);
//   }

 for(i=0;i<numzones;i++)
  if (zones[i].eval==eval)
   {
    if (zones[i].type==ACTIVATIONTYPE)
     {
      for(sp=firstscaleobj.next;sp!=&lastscaleobj;sp=sp->next)
       if (sp->active==false && sp->moveSpeed)
	{
	 x=sp->x>>FRACTILESHIFT;
	 y=sp->y>>FRACTILESHIFT;
	 if (x>=zones[i].x1 && x<=zones[i].x2 && y>=zones[i].y1 && y<=zones[i].y2)
	  {
	   sp->active=true;
	   sp->actiontime=timecount+40;
	   ActivationSound(sp);
	   }
	 }
       if (zones[i].removeable)
	{
	 zones[i].type=-1;
	 zones[i].eval=0;
	 }
       }
     else if (zones[i].type==MAPZONETYPE)
      {
       AddProcess(i);
       if (zones[i].removeable)
	{
	 zones[i].type=-1;
	 zones[i].eval=0;
	 }
       }
     else if (zones[i].type==SPAWNTYPE && eval>0)
      {
       gameloading=true;
       if (!eventloading || zones[i].endeval)
	{
	 x1=(zones[i].x1*MAPSIZE+32)<<FRACBITS;
	 y1=(zones[i].y1*MAPSIZE+32)<<FRACBITS;
	 SpawnSprite(S_WARP,x1,y1,RF_GetFloorZ(x1,y1)+10*FRACUNIT,0,0,0,true,0);
	 sp=SpawnSprite(zones[i].stype,x1,y1,RF_GetFloorZ(x1,y1)+10*FRACUNIT,0,0,0,true,0);
	 sp->deathevent=zones[i].endeval;
	 }
       gameloading=false;
       if (zones[i].removeable)
	{
	 zones[i].type=-1;
	 zones[i].eval=0;
	 }
       }
     else if (zones[i].type==TRIGGERTYPE && eval>0)
      {
       triggers[zones[i].x1][zones[i].y1]=zones[i].endeval;
       if (zones[i].removeable)
	{
	 zones[i].type=-1;
	 zones[i].eval=0;
	 }
       }
     else if (zones[i].type==SOUNDTYPE && eval>0)
      {
       SoundEffect(zones[i].endeval,0,(zones[i].x1*MAPSIZE+32)<<FRACBITS,(zones[i].y1*MAPSIZE+32)<<FRACBITS);
       if (zones[i].removeable)
	{
	 zones[i].type=-1;
	 zones[i].eval=0;
	 }
       }
     else if (zones[i].type==AREATRIGGERTYPE)
      {
       for (y=zones[i].y1;y<zones[i].y2;y++)
	for (x=zones[i].x1;x<zones[i].x2;x++)
	 triggers[x][y]=zones[i].endeval;
       if (zones[i].removeable)
	{
	 zones[i].type=-1;
	 zones[i].eval=0;
	 }
       }
     else if (zones[i].type==FLITYPE && eval>0 && !netmode)
      {

#ifdef DEMO
       playfli("GREED.BLO",infotable[CA_GetNamedNum(fliname[zones[i].endeval])].filepos);
#else

#ifdef CDROMGREEDDIR
       sprintf(name,"%c:\\GREED\\MOVIES\\%s",cdr_drivenum+'A',fliname[zones[i].endeval]);
#else
       sprintf(name,"%c:\\MOVIES\\%s",cdr_drivenum+'A',fliname[zones[i].endeval]);
#endif
       playfli(name,0);
//       playfli("E:\\PSYBORG\\MAKEDATA\\FLI\\JETTISON.FLI",0);

#endif

       font=font1;
       fontbasecolor=8;
       printx=160;
       printy=185;
       switch (zones[i].endeval)
	{
	 case 0:
	  FN_PrintCentered("Do not taunt happy fun airlock.");
	  break;
	 case 1:
	  FN_PrintCentered("Original recipe or extra crispy?");
	  break;
	 case 2:
	  FN_PrintCentered("Thank you for recycling!");
	  break;
	 }
       fliplayed=1;
       zones[i].eval=0;
       zones[i].type=-1;
       player.angst=0;

       if (SC.vrhelmet==1)
	{
	 screen=vrscr;
	 for (j=0;j<SCREENHEIGHT;j++)
	  ylookup[j]=screen+j*SCREENWIDTH;
	 }

       }
     }
 }


void Process(void)
{
 int     count, index, x, y, mapspot;
 zone_t  *z;
 byte    *layer;
 bool changed;

 count=numprocesses;
 index=-1;
 do
  {
   index++;
   count--;
   while (index<MAXPROCESS && processes[index]==0) index++;
   if (index==MAXPROCESS)
    MS_Error("Processes: can't find next process!");
   z=&zones[processes[index]-1000];
   switch (z->layer)
    {
     case NORTHWALL:
      layer=northwall;
      break;
     case WESTWALL:
      layer=westwall;
      break;
     case LIGHTS:
      layer=maplights;
      break;
     case FLOORHEIGHT:
      layer=floorheight;
      break;
     case CEILINGHEIGHT:
      layer=ceilingheight;
      break;
     case EFFECTS:
      layer=mapeffects;
      break;
     case CEILING:
      layer=ceilingpic;
      break;
     case FLOOR:
      layer=floorpic;
      break;
     default:
      MS_Error("Layer %i is not implemented",z->layer);
     }
   changed=false;
   for (y=z->y1;y<=z->y2;y++)
    for (x=z->x1;x<=z->x2;x++)
     {
      mapspot=y*MAPCOLS+x;
      if (layer[mapspot]<z->newvalue)
       {
	if ((int)layer[mapspot]+z->rate>z->newvalue)
	 layer[mapspot]=z->newvalue;
	else
	 layer[mapspot]+=z->rate;
	changed=true;
	}
      else if (layer[mapspot]>z->newvalue)
       {
	if ((int)layer[mapspot]-z->rate<z->newvalue)
	 layer[mapspot]=z->newvalue;
	else
	 layer[mapspot]-=z->rate;
	if (layer[mapspot]<z->newvalue)
	 layer[mapspot]=z->newvalue;
	changed=true;
	}
      }
   if (!changed)
    {
     --numprocesses;
     processes[index]=0;
     if (z->endeval)
      Event(z->endeval,false);
     }
   } while (count>0);
 }


void LoadScript(int lump,bool newgame)
{
 char       s[100], *fname, token[100];
 int        i, j, x, y, eval, line, etype, upper, lower, speed, result, endeval;
 int        num, val, psprite, total, ceval, def1, def2, x1, y1, x2, y2, removeable;
 FILE       *f;
 elevobj_t  *elevator_p;
 zone_t     *z;
 int        numloadsprites, loadsprites[16], loadspritesn[16], eventlump;

 memset(triggers,0,sizeof(triggers));
 memset(switches,0,sizeof(switches));
 memset(zones,0,sizeof(zones));
 memset(processes,0,sizeof(processes));
 numprocesses=0;
 numzones=0;
 fliplayed=0;
 numloadsprites=0;

 memset(secondaries,-1,sizeof(secondaries));
 memset(primaries,-1,sizeof(primaries));
 memset(pcount,0,sizeof(pcount));
 memset(scount,0,sizeof(scount));
 bonustime=3150;
 levelscore=100000;
 player.levelscore=100000;
 eventlump=CA_GetNamedNum("BACKDROP");

 if (MS_CheckParm("file"))
  {
   fname=infotable[lump].nameofs + (char *)infotable;
   strcpy(s,fname);
   i=0;
   while (i<20 && s[i]!='.' && s[i]!=0) i++;
   strcpy(&s[i],".SUX");
   f=fopen(s,"rt");
   if (f==NULL) MS_Error("LoadScript: Error opening %s",s);
   }
 else
  {
   close(cachehandle);
   lump=lump-CA_GetNamedNum("MAP")+CA_GetNamedNum("SUX");
   f=fopen("GREED.BLO","rt");
   if (f==NULL) MS_Error("LoadScript: Error reopening GREED.BLO");
   fseek(f,infotable[lump].filepos,SEEK_SET);
   }

 line=1;
 while (1)
  {
   UpdateWait();
   fscanf(f," %s ",token);
   if (stricmp(token,"END")==0)
    break;
   else if (stricmp(token,"TRIGGER")==0)
    {
     result=fscanf(f,"%i %i %i \n",&x,&y,&eval);
     CHECKERROR(3);
     triggers[x][y]=eval;
     }
   else if (stricmp(token,"AREATRIGGER")==0)
    {
     result=fscanf(f,"%i %i %i %i %i \n",&x1,&y1,&x2,&y2,&eval);
     CHECKERROR(5);
     for (i=y1;i<=y2;i++)
      for (j=x1;j<=x2;j++)
       triggers[j][i]=eval;
     }
   else if (stricmp(token,"WALLSWITCH")==0)
    {
     result=fscanf(f,"%i %i %i \n",&x,&y,&eval);
     CHECKERROR(3);
     switches[x][y]=eval;
     }
   else if (stricmp(token,"ELEVATOR")==0)
    {
     result=fscanf(f,"%i %i %i %i %i %i %i %i \n",&x,&y,&eval,&endeval,&etype,&upper,&lower,&speed);
     CHECKERROR(8);
     elevator_p=RF_GetElevator();
     elevator_p->floor=lower;
     elevator_p->mapspot=y*MAPCOLS+x;
     elevator_p->ceiling=upper;
     if (etype==0)
      elevator_p->position=lower;
     else
      elevator_p->position=upper;
     elevator_p->type=E_TRIGGERED;
     elevator_p->elevTimer=0x70000000;
     elevator_p->speed=speed;
     elevator_p->eval=eval;
     elevator_p->endeval=endeval;
     elevator_p->elevTimer=player.timecount;
     elevator_p->nosave=1;
     floorheight[elevator_p->mapspot]=elevator_p->position;
     }
   else if (stricmp(token,"SPAWNELEVATOR")==0)
    {
     result=fscanf(f,"%i %i %i %i %i %i %i \n",&x,&y,&eval,&etype,&upper,&lower,&speed);
     CHECKERROR(7);
     elevator_p=RF_GetElevator();
     elevator_p->floor=lower;
     elevator_p->mapspot=y*MAPCOLS+x;
     elevator_p->ceiling=upper;
     if (etype==0)
      elevator_p->position=lower;
     else
      elevator_p->position=upper;
     elevator_p->type=E_NORMAL;
     elevator_p->elevTimer=0x70000000;
     elevator_p->speed=speed;
     elevator_p->eval=eval;
     elevator_p->endeval=endeval;
     elevator_p->elevTimer=player.timecount;
     elevator_p->nosave=1;
     floorheight[elevator_p->mapspot]=elevator_p->position;
     }
   else if (stricmp(token,"ACTIVATIONZONE")==0)
    {
     z=&zones[numzones];
     ++numzones;
     if (numzones==MAXZONES)
      MS_Error("Out of mapzones");
     result=fscanf(f,"%i %i %i %i %i %i \n",&z->x1,&z->y1,&z->x2,&z->y2,&z->eval,&z->removeable);
     CHECKERROR(6);
     z->type=ACTIVATIONTYPE;
     }
   else if (stricmp(token,"MAPZONE")==0)
    {
     z=&zones[numzones];
     ++numzones;
     if (numzones==MAXZONES)
      MS_Error("Out of mapzones");
     result=fscanf(f,"%i %i %i %i %i %i %i %i %i %i \n",&z->x1,&z->y1,&z->x2,&z->y2,&z->eval,&z->endeval,&z->layer,&z->newvalue,&z->rate,&z->removeable);
     CHECKERROR(10);
     z->type=MAPZONETYPE;
     }
   else if (stricmp(token,"BONUSTIME")==0)
    {
     result=fscanf(f,"%i \n",&bonustime);
     CHECKERROR(1);
     bonustime*=70;
     }
   else if (stricmp(token,"PRIMARY")==0)
    {
     result=fscanf(f,"%i %i %i %i \n",&num,&val,&total,&psprite);
     CHECKERROR(4);
     primaries[num*2]=psprite;
     primaries[num*2+1]=val;
     pcount[num]=total;
     }
   else if (stricmp(token,"SECONDARY")==0)
    {
     result=fscanf(f,"%i %i %i %i \n",&num,&val,&total,&psprite);
     CHECKERROR(4);
     secondaries[num*2]=psprite;
     secondaries[num*2+1]=val;
     scount[num]=total;
     }
   else if (stricmp(token,"LEVELSCORE")==0)
    {
     result=fscanf(f,"%i \n",&levelscore);
     player.levelscore=levelscore;
     CHECKERROR(1);
     }
   else if (stricmp(token,"SPRITE")==0)
    {
     result=fscanf(f,"%i %i %i %i %i %i \n",&x,&y,&num,&ceval,&def1,&def2);
     CHECKERROR(6);
     if (newgame && player.difficulty>=5-def2 && player.difficulty<=5-def1)
      {
       gameloading=true;
       mapsprites[y*MAPCOLS+x]=num;
       gameloading=false;
       }
     }
   else if (stricmp(token,"SPAWN")==0)
    {
     result=fscanf(f,"%i %i %i %i %i %i %i %i \n",&x,&y,&eval,&num,&ceval,&def1,&def2,&removeable);
     CHECKERROR(8);
     if (player.difficulty>=5-def2 && player.difficulty<=5-def1 &&
      (newgame || (!newgame && (!removeable || !player.events[eval]))))
      {
       z=&zones[numzones];
       ++numzones;
       if (numzones==MAXZONES)
	MS_Error("Out of mapzones");
       z->x1=x;
       z->y1=y;
       z->eval=eval;
       z->endeval=ceval;
       z->stype=num;
       z->type=SPAWNTYPE;
       z->removeable=removeable;
       }
     }
   else if (stricmp(token,"SPAWNTRIGGER")==0)
    {
     result=fscanf(f,"%i %i %i %i %i\n",&x,&y,&eval,&ceval,&removeable);
     CHECKERROR(5);
     z=&zones[numzones];
     ++numzones;
     if (numzones==MAXZONES)
      MS_Error("Out of mapzones");
     z->x1=x;
     z->y1=y;
     z->eval=eval;
     z->endeval=ceval;
     z->type=TRIGGERTYPE;
     z->removeable=removeable;
     }
   else if (stricmp(token,"SPAWNSOUND")==0)
    {
     result=fscanf(f,"%i %i %i %i %i\n",&x,&y,&eval,&ceval,&removeable);
     CHECKERROR(5);
     z=&zones[numzones];
     ++numzones;
     if (numzones==MAXZONES)
      MS_Error("Out of mapzones");
     z->x1=x;
     z->y1=y;
     z->eval=eval;
     z->endeval=ceval;
     z->type=SOUNDTYPE;
     z->removeable=removeable;
     }
   else if (stricmp(token,"SPAWNFLI")==0)
    {
     result=fscanf(f,"%i %i \n",&eval,&ceval);
     CHECKERROR(2);
     z=&zones[numzones];
     ++numzones;
     if (numzones==MAXZONES)
      MS_Error("Out of mapzones");
     z->eval=eval;
     z->endeval=ceval;
     z->type=FLITYPE;
     }
   else if (stricmp(token,"FORCELOAD")==0)
    {
     result=fscanf(f,"%s %i \n",s,&x);
     CHECKERROR(2);
     loadsprites[numloadsprites]=CA_GetNamedNum(s);
     loadspritesn[numloadsprites]=x;
     numloadsprites++;
     }
   else if (stricmp(token,"SPAWNAREATRIGGER")==0)
    {
     z=&zones[numzones];
     ++numzones;
     if (numzones==MAXZONES)
      MS_Error("Out of mapzones");
     result=fscanf(f,"%i %i %i %i %i %i %i \n",&z->x1,&z->y1,&z->x2,&z->y2,&z->eval,&z->endeval,&z->removeable);
     CHECKERROR(7);
     z->type=AREATRIGGERTYPE;
     }
   else if (stricmp(token,"BACKDROP")==0)
    {
     result=fscanf(f,"%s \n",s);
     CHECKERROR(1);
     eventlump=CA_GetNamedNum(s);
     }
   else while (fgetc(f)!='\n') ;
   line++;
   }
 fclose(f);
 if (!MS_CheckParm("file"))
  {
   if ((cachehandle=open("GREED.BLO",O_RDONLY | O_BINARY))==-1)
    MS_Error("LoadScript: Can't open GREED.BLO!");
   }
 for (x=0;x<numloadsprites;x++)
  {
   UpdateWait();
   DemandLoadMonster(loadsprites[x],loadspritesn[x]);
   UpdateWait();
   }
 lseek(cachehandle,infotable[eventlump].filepos+8,SEEK_SET);
 read(cachehandle,backdrop,256*128);
 lseek(cachehandle,infotable[eventlump+1].filepos+8,SEEK_SET);
 read(cachehandle,backdrop+256*128,256*128);
 Event(0,false);
 }
