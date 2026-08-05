/* The platform layer's interface to the engine.

   Deliberately small: the SDL backends reimplement the existing VI_*, INT_*
   and M_* entry points (declared in D_video.h / D_ints.h) rather than
   introducing new ones, so this covers only the genuinely new calls the
   engine makes into the platform. */

#ifndef GREED_SYS_GREED_H
#define GREED_SYS_GREED_H

/* ---- sys_main.c -------------------------------------------------------- */

/* Pumps SDL events and advances the 35 Hz game clock, running INT_TimerISR()
   once per elapsed tick.  Replaces the Win32 PeekMessage/DispatchMessage
   pump, and is called from exactly the two places that pump lived: Wait()
   in Intro.c and TimeUpdate() in Raven.c.

   Note this deliberately runs the tick on the main thread.  The Win32 port
   fired INT_TimerISR from a timeSetEvent callback on a separate thread, which
   then mutated game state from PlayerCommand -> ControlMovement while the
   render loop read it.  Driving ticks from here removes that race. */
void Sys_Frame(void);

/* Sys_Frame plus a present, for engine loops that spin waiting on state the
   timer hook mutates -- the menu loops in Menu.c.  Those relied on the tick
   arriving asynchronously; with a main-thread tick, a loop that doesn't call
   in here never advances and the game appears to freeze.  VSync inside the
   present keeps such a loop at refresh rate instead of spinning hot. */
void Sys_PumpFrame(void);

/* Modal error box, used by MS_Error before it exits. */
void Sys_ErrorBox(const char *message);

/* ---- sys_input.c ------------------------------------------------------- */

/* Mouse-look deltas accumulated since the last read, consumed by
   ControlMovement().  Reading clears them. */
void Sys_GetMouseLook(int *dx, int *dy);

/* True while the game should have the pointer captured for mouse look. */
void Sys_SetMouseLook(int enabled);

/* ---- sys_files.c ------------------------------------------------------- */

/* Must be called once at startup, before any asset is opened. */
void Sys_InitPaths(void);

/* Resolves a read-only asset name to a full path (for libxmp, which opens
   files itself rather than going through our fopen shim). */
void Sys_ResolveAsset(const char *name, char *out, unsigned long outlen);

/* ---- sys_sound.c ------------------------------------------------------- */

int  Sys_SoundInit(int voices);
void Sys_SoundShutdown(void);

/* wav points at a RIFF/WAVE lump from GREED.BLO.  pan is 0 (left) to 1
   (right); ratio is a playback-rate multiplier for pitch variation. */
void Sys_PlayVoice(int v, const void *wav, int wavlen,
                   float gain, float pan, float ratio);
void Sys_SetVoiceGain(int v, float gain);
void Sys_StopVoice(int v);

int  Sys_MusicPlay(const char *path, int pattern);
void Sys_MusicStop(void);
int  Sys_MusicPlaying(void);
void Sys_MusicSetVolume(float v);
void Sys_MusicUpdate(void);

#endif /* GREED_SYS_GREED_H */
