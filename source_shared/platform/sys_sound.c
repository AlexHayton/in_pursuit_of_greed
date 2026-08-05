/* SDL3 audio backend.

   The Win32 port stubbed out every sound function, so this restores audio the
   game has not had since DOS.  It replaces DSIK (Carlos Hasan's Digital Sound
   Interface Kit), which the original used for both sample playback and module
   music.

   SDL3 does most of the work.  Rather than hand-rolling a mixer, each effect
   voice is its own SDL_AudioStream bound to the playback device: SDL3 mixes
   all bound streams together and resamples each from its source rate to the
   device rate.  That maps almost one-to-one onto DSIK's voice API --
   SDL_SetAudioStreamGain for dSetVoiceVolume, and
   SDL_SetAudioStreamFrequencyRatio for dSetVoiceFreq's pitch variation.

   Sound effects live in GREED.BLO as plain RIFF/WAVE, 8-bit unsigned mono at
   11025 Hz.

   One deliberate fidelity compromise: DSIK could change a voice's stereo
   balance while it was still playing, and UpdateSound did that every frame as
   the player turned.  SDL3 has gain per stream but no pan, so panning is
   applied by expanding the mono sample to stereo with per-channel gains at
   the moment the effect starts.  Volume still tracks the player continuously;
   pan is fixed for the life of one effect.  Since effects are well under a
   second, this is inaudible in practice. */

#include "sys_compat.h"

/* Engine headers FIRST -- see the note in sys_video.c. */
#include "d_global.h"
#include "sys_sdl.h"

#ifdef HAVE_LIBXMP
#include <xmp.h>
#endif

#include "d_global.h"

#define MAX_VOICES    8
#define DEV_RATE      44100
#define MUSIC_CHUNK   8192            /* frames generated per top-up */
#define MUSIC_LOW     (DEV_RATE / 4)  /* keep ~250ms queued */

static SDL_AudioDeviceID device;
static SDL_AudioStream  *voice[MAX_VOICES];
static int               num_voices;
static int               audio_ready;

static SDL_AudioStream  *music_stream;
static float             music_gain = 1.0f;

#ifdef HAVE_LIBXMP
static xmp_context       xmp_ctx;
static int               module_loaded;
static int               music_running;
#endif


int Sys_SoundInit(int voices)
{
    SDL_AudioSpec devspec, srcspec;
    int i;

    if (audio_ready)
        return 1;

    if (voices < 1)          voices = 1;
    if (voices > MAX_VOICES) voices = MAX_VOICES;
    num_voices = voices;

    SDL_zero(devspec);
    devspec.freq     = DEV_RATE;
    devspec.format   = SDL_AUDIO_F32;
    devspec.channels = 2;

    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &devspec);
    if (!device) {
        fprintf(stderr, "Sound: could not open audio device: %s\n", SDL_GetError());
        return 0;
    }

    /* Effect sources: the WAV data as it sits in GREED.BLO, widened to stereo
       so we can bake in the pan.  SDL3 resamples 11025 -> device rate. */
    SDL_zero(srcspec);
    srcspec.freq     = 11025;
    srcspec.format   = SDL_AUDIO_U8;
    srcspec.channels = 2;

    for (i = 0; i < num_voices; i++) {
        voice[i] = SDL_CreateAudioStream(&srcspec, &devspec);
        if (!voice[i]) {
            fprintf(stderr, "Sound: SDL_CreateAudioStream failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_BindAudioStream(device, voice[i]);
    }

#ifdef HAVE_LIBXMP
    {
        SDL_AudioSpec mspec;
        SDL_zero(mspec);
        mspec.freq     = DEV_RATE;
        mspec.format   = SDL_AUDIO_S16;   /* libxmp's default output format */
        mspec.channels = 2;

        music_stream = SDL_CreateAudioStream(&mspec, &devspec);
        if (music_stream)
            SDL_BindAudioStream(device, music_stream);

        xmp_ctx = xmp_create_context();
    }
#endif

    SDL_ResumeAudioDevice(device);
    audio_ready = 1;
    return 1;
}


void Sys_SoundShutdown(void)
{
    int i;

    if (!audio_ready)
        return;

#ifdef HAVE_LIBXMP
    Sys_MusicStop();
    if (xmp_ctx) {
        xmp_free_context(xmp_ctx);
        xmp_ctx = NULL;
    }
#endif

    for (i = 0; i < num_voices; i++) {
        if (voice[i]) {
            SDL_DestroyAudioStream(voice[i]);
            voice[i] = NULL;
        }
    }
    if (music_stream) {
        SDL_DestroyAudioStream(music_stream);
        music_stream = NULL;
    }

    SDL_CloseAudioDevice(device);
    device = 0;
    audio_ready = 0;
}


/* ---- Sound effects ------------------------------------------------------ */

/* wav/wavlen: a RIFF/WAVE lump straight out of GREED.BLO.
   gain:  0..1
   pan:   0 = hard left, 0.5 = centre, 1 = hard right
   ratio: playback rate multiplier (1.0 = as recorded) */
void Sys_PlayVoice(int v, const void *wav, int wavlen,
                   float gain, float pan, float ratio)
{
    SDL_AudioSpec  spec;
    Uint8         *buf = NULL;
    Uint32         len = 0;
    SDL_IOStream  *io;
    Uint8         *stereo;
    Uint32         i;
    float          lg, rg;

    if (!audio_ready || v < 0 || v >= num_voices || !voice[v])
        return;

    io = SDL_IOFromConstMem(wav, (size_t)wavlen);
    if (!io)
        return;

    /* closeio = true: SDL closes the IOStream for us either way. */
    if (!SDL_LoadWAV_IO(io, true, &spec, &buf, &len))
        return;

    /* Drop whatever this voice was playing, exactly as dStopVoice did. */
    SDL_ClearAudioStream(voice[v]);

    /* Equal-gain pan.  The samples are 8-bit unsigned, so silence is 128 and
       scaling has to happen about that midpoint, not about zero. */
    lg = 1.0f - pan;
    rg = pan;

    stereo = (Uint8 *)malloc((size_t)len * 2);
    if (!stereo) {
        SDL_free(buf);
        return;
    }

    for (i = 0; i < len; i++) {
        int s = (int)buf[i] - 128;
        int l = 128 + (int)(s * lg);
        int r = 128 + (int)(s * rg);
        stereo[i * 2 + 0] = (Uint8)(l < 0 ? 0 : (l > 255 ? 255 : l));
        stereo[i * 2 + 1] = (Uint8)(r < 0 ? 0 : (r > 255 ? 255 : r));
    }

    SDL_SetAudioStreamGain(voice[v], gain);
    SDL_SetAudioStreamFrequencyRatio(voice[v], ratio);
    SDL_PutAudioStreamData(voice[v], stereo, (int)len * 2);

    free(stereo);
    SDL_free(buf);
}


void Sys_SetVoiceGain(int v, float gain)
{
    if (audio_ready && v >= 0 && v < num_voices && voice[v])
        SDL_SetAudioStreamGain(voice[v], gain);
}


void Sys_StopVoice(int v)
{
    if (audio_ready && v >= 0 && v < num_voices && voice[v])
        SDL_ClearAudioStream(voice[v]);
}


/* ---- Music -------------------------------------------------------------- */

int Sys_MusicPlay(const char *path, int pattern)
{
#ifdef HAVE_LIBXMP
    if (!audio_ready || !xmp_ctx || !music_stream)
        return 0;

    Sys_MusicStop();

    if (xmp_load_module(xmp_ctx, (char *)path) != 0)
        return 0;
    module_loaded = 1;

    if (xmp_start_player(xmp_ctx, DEV_RATE, 0) != 0) {
        xmp_release_module(xmp_ctx);
        module_loaded = 0;
        return 0;
    }

    /* PlaySong's second argument jumps to an order position, which the DOS
       code did by poking the DSIK music struct directly. */
    if (pattern)
        xmp_set_position(xmp_ctx, pattern);

    music_running = 1;
    Sys_MusicUpdate();
    return 1;
#else
    (void)path; (void)pattern;
    return 0;
#endif
}


void Sys_MusicStop(void)
{
#ifdef HAVE_LIBXMP
    if (!xmp_ctx)
        return;

    if (music_running) {
        xmp_end_player(xmp_ctx);
        music_running = 0;
    }
    if (module_loaded) {
        xmp_release_module(xmp_ctx);
        module_loaded = 0;
    }
    if (music_stream)
        SDL_ClearAudioStream(music_stream);
#endif
}


int Sys_MusicPlaying(void)
{
#ifdef HAVE_LIBXMP
    return music_running;
#else
    return 0;
#endif
}


void Sys_MusicSetVolume(float v)
{
    music_gain = v;
    if (music_stream)
        SDL_SetAudioStreamGain(music_stream, music_gain);
}


/* Called from UpdateSound each frame; tops the music buffer back up. */
void Sys_MusicUpdate(void)
{
#ifdef HAVE_LIBXMP
    static short buf[MUSIC_CHUNK * 2];

    if (!music_running || !music_stream)
        return;

    while (SDL_GetAudioStreamQueued(music_stream) <
           (int)(MUSIC_LOW * 2 * sizeof(short))) {
        /* loop=1: the soundtrack repeats until the level changes. */
        if (xmp_play_buffer(xmp_ctx, buf, (int)sizeof(buf), 1) != 0) {
            music_running = 0;
            break;
        }
        SDL_PutAudioStreamData(music_stream, buf, (int)sizeof(buf));
    }
#endif
}
