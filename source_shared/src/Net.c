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

#include <STDIO.H>
#include <STRING.H>
#include <DOS.H>
#include <CONIO.H>
#include <STDLIB.H>
#include "d_global.h"
#include "d_ints.h"
#include "d_misc.h"
#include "r_public.h"
#include "protos.h"
#include "d_disk.h"
#include "r_refdef.h"

/**** CONSTANTS ****/

#define CMD_SEND          1
#define CMD_GET           2
#define GREEDCOM_ID       0xC7C7C7C7L
#define NETIPX            1
#define NETSERIAL         2
#define NETMODEM          3

#define INITEVENTID       1
#define PLAYEREVENTID     2
#define SPAWNEVENTID      3
#define QUITEVENTID       4
#define DOOREVENTID       5
#define FRAGEVENTID       6
#define NEWPLAYEREVENTID  7
#define ITEMEVENTID       8
#define BONUSEVENTID      9
#define PAUSEEVENTID      10
#define UNPAUSEEVENTID    11
#define TRIGGEREVENTID    12
#define SOUNDEVENTID      13
#define JAMMEREVENTID     14
#define EVENTEVENTID      15
#define MESSAGEEVENTID    16

#define QUESIZE				4095

#define PEL_WRITE_ADR       0x3c8
#define PEL_DATA            0x3c9
#define I_ColorBlack(r,g,b) outp(PEL_WRITE_ADR,0); \
			    outp(PEL_DATA,r);      \
			    outp(PEL_DATA,g);      \
			    outp(PEL_DATA,b)

#define NETINT() // send data on network


/**** TYPES ****/

typedef struct
 {
  int     id;
  int     playerid;
  int     found;
  int     start;
  char    netname[13];
  int     map;
  int     difficulty;
  } ievent_t; // initialization

typedef struct
 {
  int     id;
  int     playerid;
  int     value;
  fixed_t x;
  fixed_t y;
  fixed_t z;
  fixed_t zadj;
  int     angle;
  int     angle2;
  int     active;
  int     spawnid;
  } sevent_t; // spawn

typedef struct
 {
  int     id;
  int     playerid;
  } qevent_t; // quit game

typedef struct
 {
  int     id;
  int     playerid;
  fixed_t x, y;
  int     angle;
  } devent_t; // door open (by player) / trigger flipped

typedef struct
 {
  int     id;
  int     playerid;
  int     bulletid;
  } fevent_t; // frag

typedef struct
 {
  int     id;
  int     playerid;
  int     time, score;
  int     tilex, tiley;
  int     num;
  } bevent_t; // bonus item spawn

typedef struct
 {
  int     id;
  int     playerid;
  int     tilex, tiley, mapspot;
  int     type, chartype;
  } ipevent_t; // item pickup

typedef struct
 {
  int     id;
  int     playerid;
  fixed_t x, y;
  int     effect, variation;
  } eevent_t; // sound effect

typedef struct
 {
  int     id;
  int     playerid;
  char    message[30];
  } mevent_t;


enum {UART_8250, UART_16550} uart_type;

typedef struct
{
 long head, tail;    // bytes are put on head and pulled from tail
 byte data[QUESIZE+1];
 } que_t;

typedef struct
{
 que_t in;
 que_t out;
 short uart, uarttype, irqintnum;
 short intseg, intofs;
 short rsent, rreceived;
 short psent, preceived;
 } ques_t;


/**** VARIABLES ****/

pevent_t      *pevent;
sevent_t      *sevent;
ievent_t      *ievent;
qevent_t      *qevent;
devent_t      *devent;
fevent_t      *fevent;
bevent_t      *bevent;
ipevent_t     *ipevent;
eevent_t      *eevent;
mevent_t      *mevent;
greedcom_t    *greedcom;
char          msg[60];
scaleobj_t    *playersprites[MAXPLAYERS], *sprite_p, *sprite2_p, *temp_p;
pevent_t      playerdata[MAXPLAYERS];
char          netnames[MAXPLAYERS][13];
int           playernum=1, netpaused, netwarpjamtime;
bool          netwarpjammer;
int           playermapspot[MAXPLAYERS], pmapspot, oldsprites[MAXPLAYERS];
ques_t        *que;
int           uart, irqintnum, maxcount;
int           pseg, pofs, rseg, rofs;
int           fragcount[MAXPLAYERS];

extern SoundCard SC;


/**** FUNCTIONS ****/

void SpawnNewPlayer()
{
 sprite_p=RF_GetSprite();
 playersprites[pevent->playerid]=sprite_p;
 sprite_p->rotate=rt_eight;
 sprite_p->basepic=CA_GetNamedNum(charnames[pevent->chartype]);
 DemandLoadMonster(sprite_p->basepic,48);
 sprite_p->scale=1;
 sprite_p->startpic=sprite_p->basepic;
 sprite_p->intelligence=255;
 sprite_p->x=pevent->x;
 sprite_p->y=pevent->y;
 sprite_p->z=pevent->z;
 sprite_p->angle=pevent->angle;
 sprite_p->type=S_NETPLAYER;
 sprite_p->hitpoints=500;
 sprite_p->height=60<<FRACBITS;
 pmapspot=(sprite_p->y>>FRACTILESHIFT)*MAPCOLS+(sprite_p->x>>FRACTILESHIFT);
 playermapspot[pevent->playerid]=pmapspot;
 if (mapsprites[pmapspot]==0)
  mapsprites[pmapspot]=SM_NETPLAYER;
 }


void NetInit(void *addr)
{
 printf("Multiplayer:\n");
 greedcom=(greedcom_t *)addr;
 if (greedcom->id!=GREEDCOM_ID) MS_Error("Invalid ComData Address! ID=0x%X",greedcom->id);
 printf("\tPlayers=%i\n",greedcom->numplayers);
 playernum=greedcom->consoleplayer;
 printf("\tYou are player #%i\n",playernum+1);
 printf("\tNetMode=");
 if (greedcom->nettype==NETIPX) printf("IPX Net");
  else if (greedcom->nettype==NETSERIAL) printf("Serial");
  else if (greedcom->nettype==NETMODEM) printf("Modem");
  else MS_Error("Unknown net type!");
 printf("\n");
 pevent=(pevent_t *)greedcom->data;
 sevent=(sevent_t *)greedcom->data;
 ievent=(ievent_t *)greedcom->data;
 qevent=(qevent_t *)greedcom->data;
 devent=(devent_t *)greedcom->data;
 fevent=(fevent_t *)greedcom->data;
 bevent=(bevent_t *)greedcom->data;
 ipevent=(ipevent_t *)greedcom->data;
 eevent=(eevent_t *)greedcom->data;
 mevent=(mevent_t *)greedcom->data;
 }


void PlayerCommand(void);


void NetWaitStart(void)
{
 int     players[MAXPLAYERS], found, playersfound[MAXPLAYERS], ready, i;
 longint timeout, waittime;

 INT_TimerHook(NULL);
 StartWait();
 memset(players,0,sizeof(players));
 memset(playersfound,0,sizeof(playersfound));
 strncpy(netnames[playernum],SC.netname,12);
 if (netnames[playernum][10]==' ')
  {
   i=10;
   while (i>0 && netnames[playernum][i]==' ')
    {
     netnames[playernum][i]=0;
     i--;
     }
   }
 players[playernum]=1; // found ourself
 found=1;
 ready=0;
 timeout=timecount+42000; // 10 minute wait
 newascii=false;
 waittime=timecount+70;
 while (1)
  {
   UpdateWait();
   if (timecount>timeout)
    MS_Error("Multiplayer synch time-out error");
   if (newascii && lastascii==27)
    MS_Error("Multiplay Cancelled");
   newascii=false;
   if (timecount>waittime)
    {
     found=0;
     for(i=0;i<greedcom->numplayers;i++)
      if (players[i]) found++;                   // count how many we've found
     playersfound[playernum]=found;
     greedcom->command=CMD_SEND;
     greedcom->remotenode=MAXPLAYERS;           // broadcast
     greedcom->datalength=sizeof(ievent_t);
     ievent->id=INITEVENTID;
     ievent->playerid=playernum;                 // tell who we are
     ievent->found=found;                        // tell how many we've found
     ievent->start=ready;                        // player 0 tells when to start
     ievent->map=player.map;
     ievent->difficulty=player.difficulty;
     strncpy(ievent->netname,netnames[playernum],12);
     NETINT();
     waittime=timecount + 140;
     }
   if (ievent->start==greedcom->numplayers)
    break;
   greedcom->command=CMD_GET;
   NETINT();
   if (greedcom->remotenode==-1)
    continue; // no broadcasts
    if (ievent->id!=INITEVENTID)
    continue; // wrong packet id!
   if (greedcom->nettype!=NETIPX && ievent->start==greedcom->numplayers)
    break; // we can start now

   if (ievent->playerid==0)
    player.difficulty=ievent->difficulty;

   if (ievent->map!=player.map)
    MS_Error("Player #%i is playing a different map!",ievent->playerid);

   players[ievent->playerid]=1;                  // found new player
   playersfound[ievent->playerid]=ievent->found; // how many they've found
   strncpy(netnames[ievent->playerid],ievent->netname,12);
   if (playernum==0) // player 0 checks for readiness
    {
     ready=0;
     for(i=0;i<greedcom->numplayers;i++)
      if (playersfound[i]==greedcom->numplayers)
       ready++; // check everybody
     }
   }
 timecount=0;
 greedcom->maxusage=0;
 EndWait();
 NetNewPlayerData();
 INT_TimerHook(PlayerCommand);
 if (greedcom->nettype!=NETIPX && greedcom->consoleplayer==0)
  timecount=19;
 NetGetData();
 }


void NetGetData(void)
{
 static int i, angle, angleinc;

 do
  {
 /*  if (greedcom->nettype==NETIPX && greedcom->maxusage==25)
    MS_Error("Network Overload\n"
	     "Possible solutions:\n"
	     " 1. increase the speed of this machine\n"
	     " 2. remove other non-game machines from the net\n"
	     " 3. reduce number of players in game\n"); */
   greedcom->command=CMD_GET;
   NETINT();
   if (greedcom->remotenode==-1) return;
   if (pevent->playerid==playernum) continue;
   switch (pevent->id)
    {
     case PLAYEREVENTID:
      sprite_p=playersprites[pevent->playerid];
      if (sprite_p==NULL)
       continue;
      sprite_p->x=pevent->x;
      sprite_p->y=pevent->y;
      sprite_p->z=pevent->z-pevent->height;
      sprite_p->angle=pevent->angle;
      sprite_p->hitpoints=500;
      pmapspot=playermapspot[pevent->playerid];
      if (mapsprites[pmapspot]==SM_NETPLAYER)
       mapsprites[pmapspot]=0;
      pmapspot=(sprite_p->y>>FRACTILESHIFT)*MAPCOLS+(sprite_p->x>>FRACTILESHIFT);
      if (mapsprites[pmapspot]==0)
       mapsprites[pmapspot]=SM_NETPLAYER;
      playermapspot[pevent->playerid]=pmapspot;

      if (pevent->status==1)  // hit
       {
	sprite_p->basepic=sprite_p->startpic+32;
	sprite_p->modetime=timecount+12;
	}
      else if (pevent->status==2) // firing
       {
	sprite_p->basepic=sprite_p->startpic+24;
	sprite_p->modetime=timecount+12;
	playerdata[pevent->playerid].x=-1; // so it draws the walk frame
	}

      if (timecount>sprite_p->modetime)
       {
	if (playerdata[pevent->playerid].x!=pevent->x || playerdata[pevent->playerid].y!=pevent->y)
	 {
	  if (sprite_p->movemode==3)
	   {
	    sprite_p->movemode=0;
	    sprite_p->basepic=sprite_p->startpic;
	    }
	  else if (sprite_p->movemode==2)
	   {
	    sprite_p->basepic=sprite_p->startpic + 8; // midstep
	    ++sprite_p->movemode;
	    }
	  else
	   {
	    ++sprite_p->movemode;
	    sprite_p->basepic=sprite_p->startpic+sprite_p->movemode*8;
	    }
	  }
	sprite_p->modetime=timecount+12;
	}

      memcpy(&playerdata[pevent->playerid],pevent,sizeof(pevent_t));

      if (pevent->holopic)
       {
//	if (MS_RndT()>253)
//	 {
//	  sprite_p->basepic=sprite_p->startpic;
//	  sprite_p->scale=1;
//	  sprite_p->rotate=rt_eight;
//	  }
//	else
//	 {
	  sprite_p->basepic=pevent->holopic;
	  sprite_p->scale=pevent->holoscale;
	  sprite_p->rotate=rt_one;
//	  }
	}

      if (pevent->specialeffect==SE_INVISIBILITY)
       sprite_p->specialtype=st_transparent;
      else
       sprite_p->specialtype=0;

      break;
     case SPAWNEVENTID:
      gameloading=true;
      if (sevent->value==S_BULLET18)
       {
	angleinc=ANGLES/12;
	angle=0;
	for(i=0,angle=0;i<12;i++,angle+=angleinc)
	 SpawnSprite(sevent->value,sevent->x,sevent->y,sevent->z,sevent->zadj,angle,sevent->angle2,sevent->active,sevent->spawnid);
	}
      else if (sevent->value==S_SOULBULLET)
       {
	angleinc=ANGLES/16;
	angle=0;
	for(i=0,angle=0;i<16;i++,angle+=angleinc)
	 SpawnSprite(sevent->value,sevent->x,sevent->y,sevent->z,sevent->zadj,angle,sevent->angle2,sevent->active,sevent->spawnid);
	}
      else SpawnSprite(sevent->value,sevent->x,sevent->y,sevent->z,sevent->zadj,sevent->angle,sevent->angle2,sevent->active,sevent->spawnid);
      gameloading=false;
      break;
     case DOOREVENTID:
      TryDoor(devent->x,devent->y);
      break;
     case ITEMEVENTID:
      mapsprites[ipevent->mapspot]=ipevent->type;
      CheckItems(ipevent->tilex,ipevent->tiley,false,ipevent->chartype);
      mapsprites[ipevent->mapspot]=0;
      break;
     case SOUNDEVENTID:
      SoundEffect(eevent->effect,eevent->variation,eevent->x,eevent->y);
      break;
     case BONUSEVENTID:
      if (BonusItem.score>0)
       {
	for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
	 if (sprite_p->type==S_BONUSITEM)
	  {
	   RF_RemoveSprite(sprite_p);
	   mapsprites[BonusItem.mapspot]=0;
	   break;
	   }
	 SpawnSprite(S_WARP,(BonusItem.tilex*MAPSIZE+32)<<FRACBITS,(BonusItem.tiley*MAPSIZE+32)<<FRACBITS,0,0,0,0,false,0);
	}
      do
       {
	BonusItem.tilex=bevent->tilex;
	BonusItem.tiley=bevent->tiley;
	BonusItem.mapspot=BonusItem.tiley*MAPCOLS + BonusItem.tilex;
	} while (floorpic[BonusItem.mapspot]==0 || mapsprites[BonusItem.mapspot]>0 || mapeffects[BonusItem.mapspot]&FL_FLOOR);
      BonusItem.score=bevent->score;
      BonusItem.time=bevent->time;
      BonusItem.num=bevent->num;
      BonusItem.name=randnames[BonusItem.num];
      BonusItem.sprite=SpawnSprite(S_BONUSITEM,(BonusItem.tilex*MAPSIZE+32)<<FRACBITS,(BonusItem.tiley*MAPSIZE+32)<<FRACBITS,0,0,0,0,false,0);
      SpawnSprite(S_WARP,(BonusItem.tilex*MAPSIZE+32)<<FRACBITS,(BonusItem.tiley*MAPSIZE+32)<<FRACBITS,0,0,0,0,false,0);
      BonusItem.sprite->basepic+=BonusItem.num;
      oldgoalitem=-1;
      writemsg("Bonus item located!");
      break;
     case TRIGGEREVENTID:
      CheckHere(false,devent->x,devent->y,devent->angle);
      break;
     case FRAGEVENTID:
      if (fevent->bulletid<MAXPLAYERS)
       {
	oldgoalitem=-1;
	fragcount[fevent->bulletid]++;
	goalitem=fevent->bulletid+1;
	}
      if (playernum==fevent->bulletid)
       {
	player.frags[fevent->playerid]++;
	addscore(5000);
	sprintf(msg,"Fragged %s! %i of %i",netnames[fevent->playerid],player.frags[fevent->playerid],fragcount[playernum]);
	writemsg(msg);
	}
      else if (fevent->bulletid<MAXPLAYERS)
       {
	sprintf(msg,"%s fragged %s.",netnames[fevent->bulletid],netnames[fevent->playerid]);
	writemsg(msg);
	}
      else
       {
	sprintf(msg,"%s was killed.",netnames[fevent->playerid]);
	writemsg(msg);
	}

      sprite_p=playersprites[fevent->playerid];
      if (sprite_p->startpic==CA_GetNamedNum(charnames[0]))
       {
	gameloading=true;  // do not transmit
	sprite2_p=SpawnSprite(S_TIMEMINE,sprite_p->x,sprite_p->y,0,0,0,0,false,playernum);
	gameloading=false;
	sprite2_p->basepic=sprite_p->startpic+40;
	sprite_p->basepic=sprite_p->startpic+40;
	sprite2_p->scale=1;
	sprite_p->animation=0 + (0<<1) + (0<<5) + (0<<9) + ANIM_SELFDEST;
	}
      else
       {
	sprite_p->basepic=sprite_p->startpic + 40;
	sprite_p->animation = 0 + (0<<1) + (8<<5) + ((6+(MS_RndT()&3))<<9);
	}
      sprite_p->rotate=rt_one;
      sprite_p->heat=0;
      sprite_p->active=false;
      sprite_p->moveSpeed=0;
      sprite_p->hitpoints=0;
      sprite_p->intelligence=0;
      mapsprites[playermapspot[fevent->playerid]]=0;
      playersprites[fevent->playerid]=NULL;
      break;
     case NEWPLAYEREVENTID:
      SpawnNewPlayer(); // new player
      break;
     case PAUSEEVENTID:
      netpaused=true;
      break;
     case UNPAUSEEVENTID:
      netpaused=false;
      break;
     case JAMMEREVENTID:
      for (sprite_p=firstscaleobj.next; sprite_p!=&lastscaleobj;sprite_p=sprite_p->next)
       if (sprite_p->type==S_GENERATOR || (sprite_p->type>=S_GENSTART && sprite_p->type<=S_GENEND))
	{
	 mapsprites[(sprite_p->y>>FRACTILESHIFT)*MAPCOLS + (sprite_p->x>>FRACTILESHIFT)]=0;
	 temp_p=sprite_p;
	 sprite_p=sprite_p->next;
	 RF_RemoveSprite(temp_p);
	 }
       else
	sprite_p=sprite_p->next;
      sprintf(msg,"%s activated Warp Jammer!",netnames[qevent->playerid]);
      writemsg(msg);
      netwarpjammer=true;
      netwarpjamtime=timecount+70*60;
      break;
     case EVENTEVENTID:
      Event(fevent->bulletid,false);
      break;
     case QUITEVENTID:
      sprintf(msg,"%s has left the game!",netnames[qevent->playerid]);
      writemsg(msg);
      break;
     case MESSAGEEVENTID:
      sprintf(msg,"%s: %s",netnames[mevent->playerid],mevent->message);
      writemsg(msg);
      SoundEffect(SN_NEXUS,0,player.x,player.y);
      break;
     }
  } while (1);
 }


void NetSendPlayerData(void)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS; // broadcast
 greedcom->datalength=sizeof(pevent_t);
 pevent->playerid=playernum;
 pevent->id=PLAYEREVENTID;
 pevent->x=player.x;
 pevent->y=player.y;
 pevent->z=player.z;
 pevent->angle=player.angle;
 pevent->angst=player.angst;
 pevent->height=player.height;
 pevent->chartype=player.chartype;
 pevent->status=player.status;
 pevent->holopic=player.holopic;
 pevent->holoscale=player.holoscale;
 pevent->specialeffect=specialeffect;
 player.status=0;
 memcpy(&playerdata[playernum],pevent,sizeof(pevent_t));
 NETINT();
 }


void NetNewPlayerData(void)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS; // broadcast
 greedcom->datalength=sizeof(pevent_t);
 pevent->playerid=playernum;
 pevent->id=NEWPLAYEREVENTID;
 pevent->x=player.x;
 pevent->y=player.y;
 pevent->z=player.z;
 pevent->angle=player.angle;
 pevent->angst=player.angst;
 pevent->height=player.height;
 pevent->chartype=player.chartype;
 pevent->status=player.status;
 pevent->holopic=player.holopic;
 pevent->specialeffect=specialeffect;
 player.status=0;
 memcpy(&playerdata[playernum],pevent,sizeof(pevent_t));
 NETINT();
 }


void NetSendSpawn(int value, fixed_t x, fixed_t y, fixed_t z,fixed_t zadj,int angle,int angle2,bool active,int spawnid)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS; // broadcast
 greedcom->datalength=sizeof(sevent_t);
 sevent->id=SPAWNEVENTID;
 sevent->value=value;
 sevent->x=x;
 sevent->y=y;
 sevent->z=z;
 sevent->zadj=zadj;
 sevent->angle=angle;
 sevent->angle2=angle2;
 sevent->active=active;
 sevent->spawnid=spawnid;
 sevent->playerid=playernum;
 NETINT();
 }


void NetQuitGame(void)
{
 NetDeath(255);
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(qevent_t);
 qevent->id=QUITEVENTID;
 qevent->playerid=playernum;
 NETINT();
 }


void NetOpenDoor(fixed_t x,fixed_t y)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(devent_t);
 devent->id=DOOREVENTID;
 devent->playerid=playernum;
 devent->x=x;
 devent->y=y;
 NETINT();
 }
void netstub(void)
{
 }


void NetDeath(int bulletid)
{
 char str1[50];

 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(fevent_t);
 fevent->id=FRAGEVENTID;
 fevent->playerid=playernum;
 fevent->bulletid=bulletid;
 if (fevent->bulletid<MAXPLAYERS)
  {
   oldgoalitem=-1;
   fragcount[bulletid]++;
   goalitem=bulletid+1;
   sprintf(str1,"You were fragged by %s!",netnames[fevent->bulletid]);
   writemsg(str1);
   }
 NETINT();
 }


void NetItemPickup(int x,int y)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(ipevent_t);
 ipevent->id=ITEMEVENTID;
 ipevent->playerid=playernum;
 ipevent->tilex=x;
 ipevent->tiley=y;
 ipevent->mapspot=y*MAPCOLS+x;
 ipevent->type=mapsprites[ipevent->mapspot];
 ipevent->chartype=player.chartype;
 NETINT();
 }


void NetBonusItem(void)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(bevent_t);
 bevent->id=BONUSEVENTID;
 bevent->playerid=playernum;
 bevent->time=BonusItem.time;
 bevent->score=BonusItem.score;
 bevent->tilex=BonusItem.tilex;
 bevent->tiley=BonusItem.tiley;
 bevent->num=BonusItem.num;
 NETINT();
 }


void NetGetClosestPlayer(int sx,int sy)
{
 longint mindist, minplayer, dist;
 int     i, px, py;

 minplayer=-1;
 mindist=0x7FFFFFFFL;
 for(i=0;i<greedcom->numplayers;i++)
  if (playerdata[i].angst && playerdata[i].specialeffect!=SE_INVISIBILITY)
   // gotta be alive && visible
   {
    px=playerdata[i].x>>FRACTILESHIFT;
    py=playerdata[i].y>>FRACTILESHIFT;
    dist=(px-sx)*(px-sx) + (py-sy)*(py-sy);
    if (dist<mindist)
     {
      mindist=dist;
      minplayer=i;
      }
    }
 targx=playerdata[minplayer].x;
 targy=playerdata[minplayer].y;
 targz=playerdata[minplayer].z;
 }


void NetPause(void)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(qevent_t);
 qevent->id=PAUSEEVENTID;
 qevent->playerid=playernum;
 NETINT();
 }


void NetUnPause(void)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(qevent_t);
 qevent->id=UNPAUSEEVENTID;
 qevent->playerid=playernum;
 NETINT();
 }


void NetCheckHere(fixed_t centerx,fixed_t centery,int angle)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(devent_t);
 devent->id=TRIGGEREVENTID;
 devent->playerid=playernum;
 devent->x=centerx;
 devent->y=centery;
 devent->angle=angle;
 NETINT();
 }


void NetSoundEffect(int n,int variation,fixed_t x,fixed_t y)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(eevent_t);
 eevent->id=SOUNDEVENTID;
 eevent->playerid=playernum;
 eevent->x=x;
 eevent->y=y;
 eevent->variation=variation;
 eevent->effect=n;
 NETINT();
 }


void NetWarpJam(void)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(qevent_t);
 qevent->id=JAMMEREVENTID;
 qevent->playerid=playernum;
 NETINT();
 }


void NetEvent(int n)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(fevent_t);
 fevent->id=EVENTEVENTID;
 fevent->playerid=playernum;
 fevent->bulletid=n;
 NETINT();
 }


void NetSendMessage(char *s)
{
 greedcom->command=CMD_SEND;
 greedcom->remotenode=MAXPLAYERS;
 greedcom->datalength=sizeof(mevent_t);
 mevent->id=MESSAGEEVENTID;
 mevent->playerid=playernum;
 strcpy(mevent->message,s);
 NETINT();
 }
