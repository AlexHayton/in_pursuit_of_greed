/* Copyright 1996 by Robert Morgan of Channel 7
   Sound Interface */

#include <STDIO.H>
#include <STDLIB.H>
#include <STRING.H>
#include <IO.H>
#include <fcntl.h>
#include <sys/stat.h>
#include "d_global.h"
#include "d_misc.h"
#include "d_disk.h"
#include "d_video.h"
#include "protos.h"
#include "r_refdef.h"
#include "d_ints.h"


/**** CONSTANTS ****/

#define MAXEFFECTS    8
#define NUMTRACKS     effecttracks
#define MAXCACHESIZE  (MAXEFFECTS*2+4)
#define MAXSOUNDS     90
#define MAXSOUNDDIST  384
#define MSD           (MAXSOUNDDIST<<FRACBITS)
#define DEFAULTVRDIST  157286
#define DEFAULTVRANGLE 4

/**** VARIABLES ****/

bool   MusicPresent, MusicPlaying, MusicSwapChannels;
SoundCard SC;
int       MusicError, EffectChan, CurrentChan, FXLump;
int       MusicVol, effecttracks;

/* Per-effect-voice bookkeeping, mirroring the DOS build's SampleX/SampleY/
   SampleVol arrays: where in the world each playing effect is, so UpdateSound
   can re-attenuate it as the player moves. */
static int SampleX[MAXEFFECTS], SampleY[MAXEFFECTS];
static int SampleVol[MAXEFFECTS];
static int DVolume[MAXSOUNDDIST];   /* 0..63 by squared distance */


/**** FUNCTIONS ****/

/* SETUP.CFG was a raw dump of the SoundCard struct.  That struct's layout is
   compiler- and word-size-dependent, so the DOS/Win32 file and this build's
   are not interchangeable -- and silently reading one as the other would load
   garbage into the key bindings and volume settings.

   A magic + version + size header makes the mismatch explicit: anything that
   doesn't match is rejected and the built-in defaults are used instead. */

#define SETUP_MAGIC   0x44454247UL   /* 'GBED' little-endian */
#define SETUP_VERSION 1

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int structsize;
} setuphdr_t;


int LoadSetup(SoundCard *SC, char *Filename)
{
    int        Handle;
    setuphdr_t hdr;

    if ((Handle = _open(Filename,O_RDONLY|O_BINARY)) < 0)
        return 1;
    if (read(Handle,&hdr,sizeof(hdr)) != (int)sizeof(hdr)) {
        close(Handle);
        return 1;
    }
    if (hdr.magic != SETUP_MAGIC ||
        hdr.version != SETUP_VERSION ||
        hdr.structsize != (unsigned int)sizeof(SoundCard)) {
        close(Handle);
        return 1;
    }
    if (read(Handle,SC,sizeof(SoundCard)) != (int)sizeof(SoundCard)) {
        close(Handle);
        return 1;
    }
    close(Handle);
    return 0;
}


int SaveSetup(SoundCard *SC, char *Filename)
{
    int        Handle;
    setuphdr_t hdr;

    hdr.magic      = SETUP_MAGIC;
    hdr.version    = SETUP_VERSION;
    hdr.structsize = (unsigned int)sizeof(SoundCard);

    if ((Handle = _open(Filename,O_CREAT|O_WRONLY|O_BINARY,_S_IREAD | _S_IWRITE )) < 0)
        return 1;
    if (write(Handle,&hdr,sizeof(hdr)) != (int)sizeof(hdr)) {
        close(Handle);
        return 1;
    }
    if (write(Handle,SC,sizeof(SoundCard)) != (int)sizeof(SoundCard)) {
        close(Handle);
        return 1;
    }
    close(Handle);
    return 0;
}


void StopMusic(void)
{
 int i;

 if (MusicError)
  return;
 if (MusicPlaying)
  {
   if (!netmode)
    for(i=MusicVol;i>0;i-=3)              /* fade out */
     {
      if (i<0) i=0;
      Sys_MusicSetVolume((float)i/255.0f);
      Wait(1);
      }
   Sys_MusicStop();
   MusicPlaying=false;
   }
 if (netmode)
  NetGetData();
 }


void InitSound(void)
{
 bool autodetect, noconfig;

 MusicPresent=false;
 autodetect=false;
 noconfig=false;
 if (LoadSetup(&SC,"SETUP.CFG"))     /* load config file */
  {
   noconfig=true;
   printf("Sound:\tSETUP.CFG not found\n");
   printf("\tAuto-Detection=");
   autodetect=false;

   SC.ambientlight=2048;      // load all defaults
   SC.violence=true;
   SC.animation=true;
   SC.musicvol=100;
   SC.sfxvol=128;
   SC.ckeys[0]=scanbuttons[bt_run];
   SC.ckeys[1]=scanbuttons[bt_jump];
   SC.ckeys[2]=scanbuttons[bt_straf];
   SC.ckeys[3]=scanbuttons[bt_fire];
   SC.ckeys[4]=scanbuttons[bt_use];
   SC.ckeys[5]=scanbuttons[bt_useitem];
   SC.ckeys[6]=scanbuttons[bt_asscam];
   SC.ckeys[7]=scanbuttons[bt_lookup];
   SC.ckeys[8]=scanbuttons[bt_lookdown];
   SC.ckeys[9]=scanbuttons[bt_centerview];
   SC.ckeys[10]=scanbuttons[bt_slideleft];
   SC.ckeys[11]=scanbuttons[bt_slideright];
   SC.ckeys[12]=scanbuttons[bt_invleft];
   SC.ckeys[13]=scanbuttons[bt_invright];
   SC.inversepan=false;
   SC.screensize=0;
   SC.camdelay=35;
   SC.effecttracks=4;
   SC.mouse=1;
   SC.joystick=0;

   SC.chartype=0;
   SC.socket=1234;
   SC.numplayers=2;
   SC.serplayers=1;
   SC.com=1;
   SC.rightbutton=bt_north;
   SC.leftbutton=bt_fire;
   SC.joybut1=bt_fire;
   SC.joybut2=bt_straf;
   strncpy(SC.dialnum,"           ",12);
   strncpy(SC.netname,"           ",12);
   SC.netmap=22;
   SC.netdifficulty=2;
   SC.mousesensitivity=32;
   SC.turnspeed=8;
   SC.turnaccel=2;

   SC.vrhelmet=0;
   SC.vrangle=DEFAULTVRANGLE;
   SC.vrdist=DEFAULTVRDIST;

   lighting=1;
   changelight=SC.ambientlight;

   if (autodetect)
    {
     printf("Failed\n");
     MusicError=1;
     return;
     }
   else printf("Success\n");
   }

 MusicSwapChannels=SC.inversepan;

 scanbuttons[bt_run]=SC.ckeys[0];
 scanbuttons[bt_jump]=SC.ckeys[1];
 scanbuttons[bt_straf]=SC.ckeys[2];
 scanbuttons[bt_fire]=SC.ckeys[3];
 scanbuttons[bt_use]=SC.ckeys[4];
 scanbuttons[bt_useitem]=SC.ckeys[5];
 scanbuttons[bt_asscam]=SC.ckeys[6];
 scanbuttons[bt_lookup]=SC.ckeys[7];
 scanbuttons[bt_lookdown]=SC.ckeys[8];
 scanbuttons[bt_centerview]=SC.ckeys[9];
 scanbuttons[bt_slideleft]=SC.ckeys[10];
 scanbuttons[bt_slideright]=SC.ckeys[11];
 scanbuttons[bt_invleft]=SC.ckeys[12];
 scanbuttons[bt_invright]=SC.ckeys[13];

 lighting=1;
 changelight=SC.ambientlight;
 playerturnspeed=SC.turnspeed;
 turnunit=SC.turnaccel;

 effecttracks=SC.effecttracks;
 if (effecttracks<1) effecttracks=1;
 if (effecttracks>MAXEFFECTS) effecttracks=MAXEFFECTS;

 /* Sound effects are lumps FXLump..FXLump+n of GREED.BLO, sitting just after
    the "SOUNDEFFECTS" marker lump.  Each is a plain RIFF/WAVE. */
 FXLump=CA_GetNamedNum("SOUNDEFFECTS")+1;

 /* Attenuation table, indexed by SQUARED tile distance -- straight from the
    DOS InitSound.  Linear from 63 at the listener to 0 at MAXSOUNDDIST. */
 {
  fixed_t s, sfrac;
  int     i;

  s=63<<FRACBITS;
  sfrac=FIXEDDIV(s,MAXSOUNDDIST<<FRACBITS);
  for(i=0;i<MAXSOUNDDIST;i++,s-=sfrac)
   DVolume[i]=s>>FRACBITS;

  for(i=0;i<MAXEFFECTS;i++)
   SampleVol[i]=0;
 }

 if (!Sys_SoundInit(effecttracks))
  {
   MusicError=3;
   return;
   }

 MusicPresent=true;
 MusicError=0;
 CurrentChan=0;
 EffectChan=0;
 SetVolumes(SC.musicvol,SC.sfxvol);
 }


void PlaySong(char *sname,int pattern)
{
 char path[1024];

 if (MusicError) return;
 StopMusic();                     /* stop other music first */

 /* libxmp opens the file itself, so it needs a resolved path rather than the
    bare name the rest of the engine passes around. */
 Sys_ResolveAsset(sname,path,sizeof(path));

 if (!Sys_MusicPlay(path,pattern))
  {
   /* A missing or unreadable module is not worth aborting the game over --
      the DOS build called MS_Error here, but that was when the soundtrack was
      guaranteed to be sitting next to the executable. */
   fprintf(stderr,"PlaySong: could not play %s\n",sname);
   return;
   }

 if (netmode)
  NetGetData();

 Sys_MusicSetVolume((float)SC.musicvol/255.0f);
 MusicVol=SC.musicvol;
 MusicPlaying=true;
 }


void SoundEffect(int n,int variation,fixed_t x,fixed_t y)
{
 int     x1, y1, z, d1, vol;
 fixed_t d, viewsin, viewcos;
 byte   *wav;
 float   ratio, pan;

 if (MusicError || SC.sfxvol==0) return;

 x1=(x-player.x)>>FRACTILESHIFT;        /* don't play if too far */
 y1=(y-player.y)>>FRACTILESHIFT;
 z=x1*x1 + y1*y1;
 if (z>=MAXSOUNDDIST || z<0) return;

 SampleX[CurrentChan]=x>>FRACTILESHIFT;
 SampleY[CurrentChan]=y>>FRACTILESHIFT;

 /* Pan from the listener's view vector, exactly as the DOS version did:
    project the offset onto the player's facing to get -0x40..+0x40. */
 viewcos=costable[player.angle];
 viewsin=sintable[player.angle];
 d=-FIXEDMUL(y1<<FRACTILESHIFT,viewcos)-FIXEDMUL(x1<<FRACTILESHIFT,viewsin);
 d=FIXEDDIV(d,MSD)*0x40;
 if (MusicSwapChannels) d1=(d>>FRACBITS) + 0x40;
  else d1=-(d>>FRACBITS) + 0x40;
 if (d1<0) d1=0;
  else if (d1>0x80) d1=0x80;

 vol=DVolume[z];
 SampleVol[CurrentChan]=vol;

 /* Pitch variation, so repeated shots don't sound mechanical.  midgetmode
    doubles the rate for the shrunk-player cheat. */
 ratio=(float)((MS_RndT()&variation)+100-(variation>>1))/100.0f;
 if (midgetmode) ratio*=2.0f;

 pan=(float)d1/128.0f;

 wav=CA_CacheLump(FXLump+n);
 Sys_PlayVoice(CurrentChan,
	       wav,infotable[FXLump+n].size,
	       ((float)vol/63.0f)*((float)SC.sfxvol/255.0f),
	       pan,ratio);

 ++CurrentChan;
 if (CurrentChan>=effecttracks) CurrentChan=0;   /* go to next channel */
 }


void StaticSoundEffect(int n,fixed_t x,fixed_t y)
{
 /* Deliberately empty: the entire body was already commented out in the
    shipped DOS source (source_dos/CODE/MODPLAY.C), so ambient looping sounds
    never played in the real game either. */
 (void)n; (void)x; (void)y;
 }


void UpdateSound(void)
{
 int     x1, y1, z, i, px, py, vol;

 if (MusicError) return;

 Sys_MusicUpdate();

 /* Re-attenuate each playing effect as the player moves.  Pan is fixed for
    the life of an effect -- see the note in platform/sys_sound.c. */
 px=player.x>>FRACTILESHIFT;
 py=player.y>>FRACTILESHIFT;

 for(i=0;i<effecttracks;i++)
  {
   x1=SampleX[i]-px;
   y1=SampleY[i]-py;
   z=x1*x1 + y1*y1;
   if (z>=MAXSOUNDDIST || z<0)          /* out of earshot */
    {
     if (SampleVol[i]!=0)
      {
       Sys_SetVoiceGain(i,0.0f);
       SampleVol[i]=0;
       }
     continue;
     }
   vol=DVolume[z];
   if (SampleVol[i]!=vol)
    {
     Sys_SetVoiceGain(i,((float)vol/63.0f)*((float)SC.sfxvol/255.0f));
     SampleVol[i]=vol;
     }
   }
 }


void SetVolumes(int music,int fx)
{
 if (MusicError)
  return;
 if (music>255) music=255;
 Sys_MusicSetVolume((float)music/255.0f);
 MusicVol=music;
 if (fx>255) fx=255;
 SC.musicvol=music;
 SC.sfxvol=fx;
 }
