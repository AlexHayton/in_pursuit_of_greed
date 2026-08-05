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

#ifndef PROTOS_H
#define PROTOS_H

#include "d_font.h"
#include "d_video.h"
#include "r_public.h"

/* Raven */

 // generated sprites
#define S_START           513
#define S_BULLET1         513
#define S_BULLET2         514
#define S_BULLET3         515
#define S_BULLET4         516
#define S_BULLET7         517
#define S_BULLET9         518
#define S_BULLET10        519
#define S_BULLET11        520
#define S_BULLET12        521
#define S_BULLET16        522
#define S_BULLET17        523
#define S_BULLET18        524
#define S_EXPLODE         525
#define S_EXPLODE2        526
#define S_MINEBULLET      527
#define S_MINEPUFF        528
#define S_HANDBULLET      529
#define S_SOULBULLET      530
#define S_WALLPUFF        531
#define S_BLOODSPLAT      532
#define S_GREENBLOOD      533
#define S_PLASMAWALLPUFF  534
#define S_GREENPUFF       535
#define S_ARROWPUFF       536
#define S_GENERATOR       537
#define S_WARP            538
#define S_MONSTERBULLET1  539
#define S_MONSTERBULLET2  540
#define S_MONSTERBULLET3  541
#define S_MONSTERBULLET4  542
#define S_MONSTERBULLET5  543
#define S_MONSTERBULLET6  544
#define S_MONSTERBULLET7  545
#define S_MONSTERBULLET8  546
#define S_MONSTERBULLET9  547
#define S_MONSTERBULLET10 548
#define S_MONSTERBULLET11 549
#define S_MONSTERBULLET12 550
#define S_MONSTERBULLET13 551
#define S_MONSTERBULLET14 552
#define S_MONSTERBULLET15 553
#define S_END             553

#define S_SMALLEXPLODE   600
#define S_BONUSITEM      601
#define S_INSTAWALL      602
#define S_DECOY          603
#define S_GRENADE        604
#define S_CLONE          605
#define S_GRENADEBULLET  606
#define S_METALPARTS     607

 // mapsprites
#define S_PLAYER           1
#define S_NETPLAYER2       2
#define S_NETPLAYER3       3
#define S_NETPLAYER4       4
#define S_NETPLAYER5       5
#define S_NETPLAYER6       6
#define S_NETPLAYER7       7
#define S_NETPLAYER8       8
#define S_SOLID           10
#define S_WARP1           11
#define S_WARP2           12
#define S_WARP3           13
#define S_ELEVATOR        15
#define S_PAUSEDELEVATOR  16

#define S_SWAPSWITCH      19
#define S_ELEVATORHIGH    20
#define S_ELEVATORLOW     21
#define S_ELEVATOR3M      22
#define S_ELEVATOR6M      23
#define S_ELEVATOR15M     24
#define S_TRIGGER1        27
#define S_TRIGGERD1       28
#define S_TRIGGER2        30
#define S_TRIGGERD2       31

#define S_MONSTER13       34
#define S_MONSTER13_NS    35
#define S_MONSTER14       36
#define S_MONSTER14_NS    37
#define S_MONSTER15       38
#define S_MONSTER15_NS    39

#define S_STRIGGER        49
#define S_SDOOR           50
#define S_PRIMARY1        51
#define S_PRIMARY2        52
#define S_SECONDARY1      53
#define S_SECONDARY2      54
#define S_SECONDARY3      55
#define S_SECONDARY4      56
#define S_SECONDARY5      57
#define S_SECONDARY6      58
#define S_SECONDARY7      59
#define S_VDOOR1          60
#define S_HDOOR1          61
#define S_VDOOR2          62
#define S_HDOOR2          63
#define S_VDOOR3          64
#define S_HDOOR3          65
#define S_VDOOR4          66
#define S_HDOOR4          67
#define S_VDOOR5          68
#define S_HDOOR5          69
#define S_VDOOR6          70
#define S_HDOOR6          71
#define S_VDOOR7          72
#define S_HDOOR7          73

#define S_MONSTER1        75
#define S_MONSTER1_NS     76
#define S_MONSTER2        77
#define S_MONSTER2_NS     78
#define S_MONSTER3        79
#define S_MONSTER3_NS     80
#define S_MONSTER4        81
#define S_MONSTER4_NS     82
#define S_MONSTER6        84
#define S_MONSTER6_NS     85
#define S_MONSTER7        86
#define S_MONSTER7_NS     87
#define S_MONSTER8        88
#define S_MONSTER8_NS     89
#define S_MONSTER9        90
#define S_MONSTER9_NS     91
#define S_MONSTER10       92
#define S_MONSTER10_NS    93
#define S_MONSTER11       94
#define S_MONSTER11_NS    95
#define S_MONSTER12       96
#define S_MONSTER12_NS    97
#define S_MONSTER5        98
#define S_MONSTER5_NS     99

#define S_GENERATOR1     100
#define S_GENERATOR2     101
#define S_SPAWN8_NS      102
#define S_SPAWN9_NS      103

#define S_WEAPON0        105
#define S_WEAPON1        106
#define S_WEAPON2        107
#define S_WEAPON3        108
#define S_WEAPON4        109
#define S_WEAPON5        110
#define S_WEAPON6        111
#define S_WEAPON7        112
#define S_WEAPON8        113
#define S_WEAPON9        114
#define S_WEAPON10       115
#define S_WEAPON11       116
#define S_WEAPON12       117
#define S_WEAPON13       118
#define S_WEAPON14       119
#define S_WEAPON15       120
#define S_WEAPON16       121
#define S_WEAPON17       122
#define S_WEAPON18       123

#define S_GOODIEBOX      133
#define S_MEDBOX         134
#define S_AMMOBOX        135
#define S_PROXMINE       136
#define S_TIMEMINE       137
#define S_EXIT           138
#define S_HOLE           139
#define S_ITEM1          140
#define S_ITEM2          141
#define S_ITEM3          142
#define S_ITEM4          143
#define S_ITEM5          144
#define S_ITEM6          145
#define S_ITEM7          146
#define S_ITEM8          147
#define S_ITEM9          148
#define S_ITEM10         149
#define S_ITEM11         150
#define S_ITEM12         151
#define S_ITEM13         152
#define S_ITEM14         153
#define S_ITEM15         154
#define S_ITEM16         155
#define S_ITEM17         156
#define S_ITEM18         157
#define S_ITEM19         158
#define S_ITEM20         159
#define S_ITEM21         160
#define S_ITEM22         161
#define S_ITEM23         162
#define S_ITEM24         163
#define S_ITEM25         164
#define S_ITEM26         165
#define S_ITEM27         166
#define S_ITEM28         167
#define S_ITEM29         168
#define S_ITEM30         169
#define S_ITEM31         170
#define S_ITEM32         171
#define S_ITEM33         172
#define S_ITEM34         173

#define S_SPAWN1         200
#define S_SPAWN2         201
#define S_SPAWN3         202
#define S_SPAWN4         203
#define S_SPAWN5         204
#define S_SPAWN6         205
#define S_SPAWN7         206
#define S_SPAWN8         207
#define S_SPAWN9         208
#define S_SPAWN10        209
#define S_SPAWN11        210
#define S_SPAWN12        211
#define S_SPAWN13        212
#define S_SPAWN14        213
#define S_SPAWN15        214

#define S_DEADMONSTER15  241
#define S_DEADMONSTER14  242
#define S_DEADMONSTER13  243
#define S_DEADMONSTER12  244
#define S_DEADMONSTER11  245
#define S_DEADMONSTER10  246
#define S_DEADMONSTER9   247
#define S_DEADMONSTER8   248
#define S_DEADMONSTER7   249
#define S_DEADMONSTER6   250
#define S_DEADMONSTER5   251
#define S_DEADMONSTER4   252
#define S_DEADMONSTER3   253
#define S_DEADMONSTER2   254
#define S_DEADMONSTER1   255

//------------------------------

#define S_NETPLAYER      399
#define S_MEDPAK1        400
#define S_MEDPAK2        401
#define S_MEDPAK3        402
#define S_MEDPAK4        403
#define S_ENERGY         404
#define S_BALLISTIC      405
#define S_PLASMA         406
#define S_SHIELD1        407
#define S_SHIELD2        408
#define S_SHIELD3        409
#define S_SHIELD4        410

#define S_IGRENADE       411
#define S_IREVERSO       412
#define S_IPROXMINE      413
#define S_ITIMEMINE      414
#define S_IDECOY         415
#define S_IINSTAWALL     416
#define S_ICLONE         417
#define S_IHOLO          418
#define S_IINVIS         419
#define S_IJAMMER        420
#define S_ISTEALER       421

#define S_GENSTART       400
#define S_GENEND         421


 // stored back in mapsprites
#define SM_ELEVATOR      127 // for special elevators (to make solid)
#define SM_WARP1         128 // permanent
#define SM_WARP2         129 // permanent
#define SM_WARP3         130 // permanent
#define SM_MEDPAK1       131
#define SM_MEDPAK2       132
#define SM_MEDPAK3       133
#define SM_MEDPAK4       134
#define SM_ENERGY        135
#define SM_BALLISTIC     136
#define SM_PLASMA        137
#define SM_SHIELD1       138
#define SM_SHIELD2       139
#define SM_SHIELD3       140
#define SM_SHIELD4       141
#define SM_IGRENADE      142
#define SM_IREVERSO      143
#define SM_IPROXMINE     144
#define SM_ITIMEMINE     145
#define SM_IDECOY        146
#define SM_IINSTAWALL    147
#define SM_ICLONE        148
#define SM_IHOLO         149
#define SM_IINVIS        150
#define SM_IJAMMER       151
#define SM_ISTEALER      152
#define SM_SWITCHDOWN    153
#define SM_SWITCHDOWN2   154
#define SM_SWITCHUP      155
#define SM_SWAPSWITCH    156
#define SM_BONUSITEM     158
#define SM_PRIMARY1      159
#define SM_PRIMARY2      160
#define SM_SECONDARY1    161
#define SM_SECONDARY2    162
#define SM_SECONDARY3    163
#define SM_SECONDARY4    164
#define SM_SECONDARY5    165
#define SM_SECONDARY6    166
#define SM_SECONDARY7    167
#define SM_STRIGGER      168
#define SM_EXIT          169
#define SM_AMMOBOX       170
#define SM_MEDBOX        171
#define SM_GOODIEBOX     172

#define SM_WEAPON0       200
#define SM_WEAPON1       201
#define SM_WEAPON2       202
#define SM_WEAPON3       203
#define SM_WEAPON4       204
#define SM_WEAPON5       205
#define SM_WEAPON6       206
#define SM_WEAPON7       207
#define SM_WEAPON8       208
#define SM_WEAPON9       209
#define SM_WEAPON10      210
#define SM_WEAPON11      211
#define SM_WEAPON12      212
#define SM_WEAPON13      213
#define SM_WEAPON14      214
#define SM_WEAPON15      215
#define SM_WEAPON16      216
#define SM_WEAPON17      217
#define SM_WEAPON18      218

#define SM_NETPLAYER     100
#define SM_CLONE          99

 // sound effects
#define SN_MON8_WAKE       0
#define SN_MON8_FIRE       1
#define SN_MON8_DIE        2
#define SN_MON9_WAKE       3
#define SN_MON9_FIRE       4
#define SN_MON9_DIE        5
#define SN_MON10_WAKE      6
#define SN_MON10_FIRE      7
#define SN_MON10_DIE       8
#define SN_MON11_WAKE      9
#define SN_MON11_FIRE      10
#define SN_MON11_DIE       11
#define SN_DOOR            12
#define SN_BULLET1         13
#define SN_BULLET3         14
#define SN_BULLET4         15
#define SN_BULLET5         16
#define SN_BULLET8         17
#define SN_BULLET9         18
#define SN_BULLET10        19
#define SN_BULLET12        20
#define SN_BULLET13        21
#define SN_EXPLODE1        22
#define SN_EXPLODE2        23
#define SN_PICKUP0         24
#define SN_PICKUP1         25
#define SN_PICKUP2         26
#define SN_PICKUP3         27
#define SN_PICKUP4         28
#define SN_HIT0            29
#define SN_HIT1            30
#define SN_HIT2            31
#define SN_HIT3            32
#define SN_HIT4            33
#define SN_DEATH0          34
#define SN_DEATH1          35
#define SN_DEATH2          36
#define SN_DEATH3          37
#define SN_DEATH4          38
#define SN_WEAPPICKUP0     39
#define SN_WEAPPICKUP1     40
#define SN_WEAPPICKUP2     41
#define SN_WEAPPICKUP3     42
#define SN_WEAPPICKUP4     43
#define SN_GRENADE         44
#define SN_TRIGGER         45
#define SN_NEXUS           46
#define SN_EVENTALARM      47
#define SN_ELEVATORSTART   48
#define SN_ELEVATORSTOP    49
#define SN_WALLSWITCH      50
#define SN_BULLET18        51
#define SN_MON1_WAKE       52
#define SN_MON1_FIRE       53
#define SN_MON1_DIE        54
#define SN_MON2_WAKE       55
#define SN_MON2_FIRE       56
#define SN_MON2_DIE        57
#define SN_MON3_WAKE       58
#define SN_MON3_FIRE       59
#define SN_MON3_DIE        60
#define SN_MON4_WAKE       61
#define SN_MON4_FIRE       62
#define SN_MON4_DIE        63
#define SN_MON5_WAKE       64
#define SN_MON5_FIRE       65
#define SN_MON5_DIE        66
#define SN_MON6_WAKE       67
#define SN_MON6_FIRE       68
#define SN_MON6_DIE        69
#define SN_MON7_WAKE       70
#define SN_MON7_FIRE       71
#define SN_MON7_DIE        72
#define SN_MON12_WAKE      73
#define SN_MON12_FIRE      74
#define SN_MON12_DIE       75
#define SN_MON13_WAKE      76
#define SN_MON13_FIRE      77
#define SN_MON13_DIE       78
#define SN_MON14_WAKE      79
#define SN_MON14_FIRE      80
#define SN_MON14_DIE       81
#define SN_MON15_WAKE      82
#define SN_MON15_FIRE      83
#define SN_MON15_DIE       84
#define SN_WARP            85
#define SN_SOULSTEALER     86
#define SN_TEMPLEEGG       87
#define SN_KAALEGG         88
#define SN_BULLET17        89


/* player special effects */
#define SE_REVERSOPILL     1
#define SE_DECOY           2
#define SE_CLONE           3
#define SE_INVISIBILITY    4
#define SE_WARPJAMMER      5

#define KBDELAY            20
#define MAXPLAYERS         8
#define MAXRANDOMITEMS     48
#define MAXINVENTORY       13

#define BOBFACTOR         12    /* adjustment to amount of head bobbing */
#define WEAPFACTOR        0.001 /* adjustment to amount of weapon bobbing */
#define RECBUFSIZE        16000
#define MAXPROBE          960
#define MOVEDELAY         7
#define MAXSPAWN          50
#define AMBIENTLIGHT      2048
#define MAXBOBS           30
#define MINDOORSIZE       8
#define SCROLLRATE        2
#define HEALTIME          300
#define MAXSTARTLOCATIONS 8
#define MAXCHARTYPES      6
#define MAXVIEWSIZE       10

typedef struct SoundCard_s
 {
  byte    ID;
  byte    Modes;
  word    Port;
  byte    IrqLine;
  byte    DmaChannel;
  word    SampleRate;
  char    DriverName[16];
  char    inversepan;
  char    ckeys[14];
  char    effecttracks;
  int     musicvol;
  int     sfxvol;
  int     ambientlight;
  int     camdelay;
  int     screensize;
  char    animation;
  char    violence;
  char    joystick;
  char    mouse;
  int     chartype;
  int     socket;
  int     numplayers;
  char    dialnum[13];
  int     com;
  int     serplayers;
  char    netname[13];
  word    jcenx, jceny, xsense, ysense;
  int     rightbutton, leftbutton;
  int     joybut1, joybut2;
  int     netmap, netdifficulty;
  int     mousesensitivity;
  int     turnspeed, turnaccel;
  int     vrhelmet;
  int     vrangle;
  int     vrdist;
  /* macOS port additions.  These must stay at the end: LoadSetup accepts a
     SETUP.CFG written by an older build by reading only the bytes that file
     actually has, which only works while the older layout is a prefix. */
  int     fullscreen;             /* window starts fullscreen */
  int     hdmode;                 /* 0 = original 320x200, 1 = HD */
  } SoundCard;

typedef struct
 {
  fixed_t x, y, z;            // location
  int     mapspot;
  int     angle;              // facing angle
  fixed_t height;             // height of character (30 units default)
  int     currentweapon;
  int     shield, angst;
  int     maxshield, maxangst;
  int     map, mission, chartype, levelscore, difficulty;
  int     weapons[5];
  int     ammo[3];
  longint bodycount, timecount, score;
  byte    northmap[MAPCOLS*MAPROWS];
  byte    westmap[MAPCOLS*MAPROWS];
  byte    northwall[MAPCOLS*MAPROWS], westwall[MAPCOLS*MAPROWS];
  byte    savesprites[MAPCOLS*MAPROWS];
  int     scrollmin, scrollmax; // looking up and down
  int     primaries[2];
  int     secondaries[7];
  int     frags[MAXPLAYERS];
  int     runmod, walkmod;
  int     inventory[MAXINVENTORY];
  int     status; // net only : firing or getting hit
  int     holopic, holoscale;
  fixed_t jumpmod;
  byte    events[256];
  } player_t;

typedef struct
 {
  int     chargerate;         // how fast it charges
  int     charge;             // current charge level
  longint chargetime;         // timecount + chargetime
  int     ammotype;
  int     ammorate;
  } weapon_t;

typedef struct
 {
  longint    time;
  int        score;
  char       *name;
  int        tilex, tiley, mapspot, num;
  scaleobj_t *sprite;
  } bonus_t;


extern bool  godmode, hurtborder, recording, playback, quitgame,
		activatemenu, gameloaded, nospawn, heatmode, nointro,
		motionmode, togglegoalitem, svgamode, ticker, gamepause,
		togglegoalitem, exitexists, warpjammer, midgetmode, netwarpjammer;
extern player_t player;
extern weapon_t weapons[19];
extern font_t   *font1, *font2, *font3;
extern byte     currentViewSize, resizeScreen, *vrscreen;
extern pic_t    *weaponpic[7], *statusbar[4];
extern longint  frames, keyboardDelay, wallanimationtime, spritemovetime, SwitchTime,
		weapdelay, secretdelay, RearViewTime, RearViewDelay, netsendtime, 
		specialeffecttime;
extern byte     *backdrop;
extern byte     *backdroplookup[256];
extern int      turnrate, MapZoom, mapmode, viewSizes[20], viewLoc[20], goalitem,
		changelight, lighting, specialeffect, oldgoalitem,
		headmove[MAXBOBS], weapmove[MAXBOBS], pmaxshield[MAXCHARTYPES],
		pmaxangst[MAXCHARTYPES], pwalkmod[MAXCHARTYPES],
		prunmod[MAXCHARTYPES], slumps[S_END-S_START+1],
		playerturnspeed, turnunit, exitx, exity;
extern fixed_t  moverate, fallrate, strafrate, pheights[MAXCHARTYPES],
		pjumpmod[MAXCHARTYPES];
extern bonus_t  BonusItem;
extern char     *randnames[MAXRANDOMITEMS], *slumpnames[S_END-S_START+1],
		pickupamounts[], *pickupammomsg[], *pickupmsg[],
		*missioninfo[30][3];
extern byte     colors[768], weaponstate[19][5];

void    maingame(void);
bool	Thrust(int angle,fixed_t speed);
void    TimeUpdate(void);
bool    TryDoor(fixed_t xcenter,fixed_t ycenter);
void    CheckItems(int centerx,int centery,bool useit,int chartype);
void    CheckHere(int useit,fixed_t centerx,fixed_t centery,int angle);
void    loadscreen(char *s);


/* Utils */

extern int     primaries[4];
extern int     secondaries[14];
extern int     bonustime;
extern int     pcount[2], scount[7];
extern int     statusbarloc[20];
extern bool    gameloading;
extern longint levelscore, inventorytime, nethurtsoundtime;
extern char    *charnames[MAXCHARTYPES];

scaleobj_t *SpawnSprite(int value, fixed_t x, fixed_t y, fixed_t z,fixed_t zadj,int angle,int angle2,bool active,int spawnid);
void       DemandLoadMonster(int lump,int num);
void       KillSprite(scaleobj_t *sp,int weapon);
void       LoadNewMap(int lump);
void       loadweapon(int n);
void       ChangeViewSize(byte MakeLarger);
void       heal(int n);
void       medpaks(int n);
void       hurt(int n);
void       newplayer(int map,int chartype,int difficulty);
void       SaveGame(int n);
void       LoadGame(int n);
void       ResetScalePostWidth (int NewWindowWidth);
void       CheckMapFlats(void);
void       addscore(int n);
void       newmap(int map,int activate);
void       respawnplayer(void);
void       MissionBriefing(int map);

/* Sprites */

extern int        spriteloc;
extern scaleobj_t *msprite, probe;
extern fixed_t    targx, targy, targz;

byte SP_Thrust();
void MoveSprites(void);
void MoveSprite(scaleobj_t *s);
void HitSprite(scaleobj_t *sp);
void ActivationSound(scaleobj_t *sp);


/* Display */

extern int     oldshots, oldgoalitem, inventorycursor, oldinventory, totaleffecttime;
extern pic_t   *heart[10];
extern bool firegrenade;

void resetdisplay(void);
void updatedisplay(void);
void displaymapmode(void);
void displayswingmapmode(void);
void displayheatmode(void);
void displayheatmapmode(void);
void displaymotionmode(void);
void displaymotionmapmode(void);
void inventoryright(void);
void inventoryleft(void);
void useinventory(void);


/* Net */

#define DATALENGTH 128

typedef struct greedcom_s
{
 unsigned int id;               /* was long: this struct is the DOS IPX driver's
				  ABI, so every field must keep its 32-bit
				  DOS width even though Net is disabled.
				  Unsigned so the 0xC7C7C7C7 magic compares. */
 short  intnum;                 // greed executes an int to send commands
 short  maxusage;
 short  nettype;
// communication between greed and the driver
 short  command;                // CMD_SEND or CMD_GET
 short  remotenode;             // dest for send, set by get (-1 = no packet)
 short  datalength;
// info common to all nodes
 short  numnodes;               // console is allways node 0
// info specific to this node
 short  consoleplayer;          // 0-3 = player number
 short  numplayers;             // 1-4
// packet data to be sent
 char   data[DATALENGTH];
 } greedcom_t;

typedef struct
 {
  int     id;
  int     playerid;
  fixed_t x, y, z;
  int     angle;
  int     angst;
  int     chartype;
  int     status;
  int     holopic, holoscale;
  fixed_t height;
  int     specialeffect;
  } pevent_t;

extern int        netmode, playernum, netpaused, netwarpjamtime;
extern greedcom_t *greedcom;
extern scaleobj_t *playersprites[MAXPLAYERS];
extern pevent_t   playerdata[MAXPLAYERS];
extern char       netnames[MAXPLAYERS][13];

void NetInit(void *addr);
void NetGetData(void);
void NetSendPlayerData(void);
void NetSendSpawnData(void);
void NetSendItemData(void);
void NetWaitStart(void);
void NetSendSpawn(int value, fixed_t x, fixed_t y, fixed_t z,fixed_t zadj,int angle,int angle2,bool active,int spawnid);
void NetQuitGame(void);
void NetOpenDoor(fixed_t x, fixed_t y);
void NetDeath(int bulletid);
void NetNewPlayerData(void);
void NetItemPickup(int x,int y);
void NetBonusItem(void);
void NetGetClosestPlayer(int sx,int sy);
void NetPause(void);
void NetUnPause(void);
void NetCheckHere(fixed_t centerx,fixed_t centery,int angle);
void NetSoundEffect(int n,int variation,fixed_t x,fixed_t y);
void NetWarpJammer(void);
void NetEvent(int n);
void NetSendMessage(char *s);

/* Modplay */

extern bool MusicPresent, MusicPlaying, MusicSwapChannels;
extern int     MusicError;

void LoadSoundEffects(void);
void FreeSoundEffects(void);
void InitSound(void);
void PlaySong(char *sname,int pattern);
void SoundEffect(int n,int variation,fixed_t x,fixed_t y);
void StaticSoundEffect(int n,fixed_t x,fixed_t y);
void UpdateSound(void);
void SetVolumes(int music,int fx);
void StopMusic(void);
int LoadSetup(SoundCard *SC, char *Filename);
int SaveSetup(SoundCard *SC, char *Filename);

/* Menu */

void ShowMenu(int n);
void ShowPause(void);
bool ShowQuit(void (*kbdfunction)(void));
void StartWait(void);
void UpdateWait(void);
void EndWait(void);
void ShowHelp(void);

/* PlayFLI */

bool playfli(char *fname,longint offset);
void Wait(longint time);

/* Event */

extern byte triggers[MAPROWS][MAPCOLS], switches[MAPROWS][MAPCOLS];
extern int numprocesses;

void LoadScript(int map,bool newgame);
void Event(int eval,bool netsend);
void Process(void);


#endif


