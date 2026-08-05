/* Entry point and frame pump.

   The 35 Hz game clock deliberately runs on the main thread here.  Both
   earlier versions ran it elsewhere: DOS hooked IRQ0, and the Win32 port used
   a timeSetEvent callback.  In each case the tick called PlayerCommand ->
   ControlMovement, mutating player position, sprite lists and the timer hook
   itself while the render loop was reading them.  Driving it from Sys_Frame
   costs nothing and removes the whole class of race. */

#include "sys_compat.h"

/* Engine headers FIRST -- see the note in sys_video.c. */
#include "d_global.h"
#define SDL_MAIN_HANDLED
#include "sys_sdl.h"
#include <SDL3/SDL_main.h>

#include "d_ints.h"
#include "d_video.h"
#include "d_misc.h"
#include "protos.h"
#include "r_refdef.h"

/* The engine ticks at 35 Hz and INT_TimerISR adds 2 per tick (timecount is in
   half-tick units, as it was in the DOS build). */
#define TICK_HZ      35
#define TICK_MS      (1000.0 / TICK_HZ)

/* If we've been stalled -- a long level load, a drag of the window -- don't
   try to catch up on every missed tick at once, or the player teleports. */
#define MAX_CATCHUP  8

void Sys_InputInit(void);
void Sys_RefreshKeys(void);
void Sys_HandleTextInput(const char *text);
void Sys_HandleKeyDown(int sdl_scancode, int shift, int caps);
void Sys_HandleMouseMotion(float dx, float dy);
void Sys_HandleMouseButtonDown(float x, float y);
void Sys_VideoShutdown(void);
void Sys_SoundShutdown(void);

extern SoundCard SC;

extern int  my_argc;
extern char **my_argv;

void startup(void);

static Uint64 last_ms;
static int    clock_started;


static void handle_event(const SDL_Event *ev)
{
    switch (ev->type) {
    case SDL_EVENT_QUIT:
        quitgame = true;
        break;

    case SDL_EVENT_KEY_DOWN:
        /* Cmd-Q and Cmd-F, since macOS users will reach for those before Alt-Q
           and F11.  Not on Windows: SDL_KMOD_GUI is the Windows key there, and
           Win+Q is the OS search shortcut -- binding quit to it would be a
           surprise.  Alt-F4 already arrives as SDL_EVENT_QUIT. */
#ifdef SDL_PLATFORM_APPLE
        if (ev->key.key == SDLK_Q && (ev->key.mod & SDL_KMOD_GUI))
            quitgame = true;
#endif
        /* Fullscreen goes through SC and VI_SetFullscreen so the shortcut and
           the Display menu row cannot end up disagreeing, and so the choice
           survives into the next run. */
        if (ev->key.key == SDLK_F11
#ifdef SDL_PLATFORM_APPLE
            || (ev->key.key == SDLK_F && (ev->key.mod & SDL_KMOD_GUI))
#endif
           ) {
            SC.fullscreen = !VI_GetFullscreen();
            VI_SetFullscreen(SC.fullscreen);
        }
        /* Feed lastascii/newascii the way the DOS keyboard ISR did, so Esc
           and Enter produce characters and can skip the intro. */
        if (!(ev->key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL)))
            Sys_HandleKeyDown((int)ev->key.scancode,
                              (ev->key.mod & SDL_KMOD_SHIFT) != 0,
                              (ev->key.mod & SDL_KMOD_CAPS) != 0);
        break;

    case SDL_EVENT_TEXT_INPUT:
        Sys_HandleTextInput(ev->text.text);
        break;

    /* The HD buffer's shape is derived from the window's pixel size, so it has
       to be re-derived when that changes -- a fullscreen transition settling,
       or the user resizing.  Cheap and idempotent; it rebuilds the projection
       tables and the two textures. */
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        /* Only flag it.  Events are pumped from TimeUpdate, which runs in the
           middle of PlayLoop -- rebuilding here would destroy and recreate both
           textures and re-point viewylookup between the render and the present,
           so the frame would be composed from two different view sizes. */
        if (hdmode)
            hdresizepending = 1;
        break;

    case SDL_EVENT_MOUSE_MOTION:
        Sys_HandleMouseMotion(ev->motion.xrel, ev->motion.yrel);
        break;

    /* The menus need the press itself, not the button's held state -- see the
       comment on MouseGetClick. */
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button == SDL_BUTTON_LEFT)
            Sys_HandleMouseButtonDown(ev->button.x, ev->button.y);
        break;

    default:
        break;
    }
}


/* TEMPORARY -- headless verification hook.

   Windows will not let a process launched from a script raise its own window
   to the foreground, so a desktop screen-grab of the game captures whatever
   was already on top instead.  Reading the presented framebuffer from inside
   the process sidesteps that entirely, the same way the PPM dumps did on
   macOS, where screencapture is blocked by permissions.

   GREED_SHOT=<prefix> writes <prefix>NNN.ppm at each tick listed in
   GREED_SHOT_AT (comma-separated, default "70,210,420").  Remove once the port
   has been played through by hand. */
static void maybe_shot(void)
{
    extern void VI_DumpPresented(char *);
    const char *prefix = getenv("GREED_SHOT");
    const char *at;
    char        list[256], path[512];
    char       *tok, *save;
    static longint last;
    longint     now;

    if (!prefix)
        return;

    now = timecount / 2;              /* timecount runs in half-ticks */
    if (now == last)
        return;

    at = getenv("GREED_SHOT_AT");
    SDL_strlcpy(list, at ? at : "70,210,420", sizeof(list));

    for (tok = strtok_r(list, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        longint want = (longint)atoi(tok);
        if (last < want && now >= want) {
            SDL_snprintf(path, sizeof(path), "%s%ld.ppm", prefix, (long)want);
            VI_DumpPresented(path);
            printf("SHOT %s at tick %ld\n", path, (long)now);
        }
    }

    last = now;
}


void Sys_Frame(void)
{
    SDL_Event ev;
    Uint64    now;
    int       ticks;

    while (SDL_PollEvent(&ev))
        handle_event(&ev);

    Sys_RefreshKeys();

    now = SDL_GetTicks();          /* Uint64 in SDL3 */
    if (!clock_started) {
        last_ms = now;
        clock_started = 1;
        return;
    }

    ticks = (int)((double)(now - last_ms) / TICK_MS);
    if (ticks <= 0)
        return;

    if (ticks > MAX_CATCHUP) {
        ticks = MAX_CATCHUP;
        last_ms = now;
    } else {
        /* Advance by whole ticks only, so the remainder carries forward and
           the clock doesn't drift slow. */
        last_ms += (Uint64)(ticks * TICK_MS);
    }

    while (ticks-- > 0)
        INT_TimerISR();

    /* Top the music stream up here rather than only in Sys_PumpFrame.  The
       stream holds a quarter second, so anything that pumps frames without
       refilling it goes silent almost immediately -- which is what the mission
       briefings did: every one of their pauses is a Wait(), Wait() spins on
       Sys_Frame(), and nothing on that path reached UpdateSound.  The
       soundtrack died a few hundred ms into the first page and only came back
       when the play loop's TimeUpdate started calling UpdateSound again.  The
       intro text screens, the credits and StopMusic's own fade-out had the
       same hole.

       After the tick loop, so it runs at the 35 Hz tick rather than at the
       speed of a spin loop that isn't throttled by anything. */
    UpdateSound();

    maybe_shot();
}


/* For engine loops that spin waiting on state the timer hook mutates.

   In DOS and Win32 those loops worked because the 35 Hz tick arrived
   asynchronously -- a real interrupt, then a timeSetEvent thread -- so a loop
   could sit there reading keyboard[] and timecount and watch them change
   underneath it.  This port drives the tick from the main thread instead
   (deliberately: see the Phase 2 note about the race that model had), which
   means any loop that does not call in here never advances at all.

   ShowMenu's `do { ... } while (!quitmenu)` is the example that bit: quitmenu
   is set by MenuCommand, MenuCommand runs only as the timer hook, and the loop
   pumped only `if (netmode)`.  In single player it spun forever.

   Presenting here as well, because these loops draw into `screen` and in DOS
   that *was* the visible framebuffer.  VSync throttles the loop as a side
   effect, so it idles at the refresh rate rather than burning a core. */
void Sys_PumpFrame(void)
{
    Sys_Frame();
    VI_BlitView();
}


int main(int argc, char *argv[])
{
    my_argc = argc;
    my_argv = argv;

    setbuf(stdout, NULL);

    /* Plain main() rather than SDL3's SDL_AppInit callback model, so the
       engine's existing startup() -> maingame() flow survives unchanged. */
    SDL_SetMainReady();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    Sys_InitPaths();
    Sys_InputInit();

    printf("\nIn Pursuit of Greed (%s port)\n\n", SDL_GetPlatform());

    startup();

    Sys_SoundShutdown();
    Sys_VideoShutdown();
    SDL_Quit();

    return 0;
}
