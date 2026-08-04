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
void Sys_VideoShutdown(void);
void Sys_SoundShutdown(void);

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
        /* Cmd-Q, since macOS users will reach for it before Alt-Q. */
        if (ev->key.key == SDLK_Q && (ev->key.mod & SDL_KMOD_GUI))
            quitgame = true;
        /* Cmd-F / F11 for fullscreen. */
        if ((ev->key.key == SDLK_F && (ev->key.mod & SDL_KMOD_GUI)) ||
            ev->key.key == SDLK_F11) {
            SDL_Window *w = SDL_GetWindowFromID(ev->key.windowID);
            bool full = (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(w, !full);
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

    case SDL_EVENT_MOUSE_MOTION:
        Sys_HandleMouseMotion(ev->motion.xrel, ev->motion.yrel);
        break;

    default:
        break;
    }
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
    /* The music stream is topped up by polling, from UpdateSound -> and that
       is only reached from TimeUpdate() in the play loop.  Without this call
       the soundtrack starves and cuts out for the whole of the FLI cutscenes
       and any time the menu is open. */
    UpdateSound();
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

    printf("\nIn Pursuit of Greed (macOS port)\n\n");

    startup();

    Sys_SoundShutdown();
    Sys_VideoShutdown();
    SDL_Quit();

    return 0;
}
