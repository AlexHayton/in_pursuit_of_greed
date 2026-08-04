/* SDL3 input backend.

   The engine indexes a keyboard[] array by IBM PC set-1 scancodes (the
   SC_* constants in d_ints.h).  SDL reports USB HID scancodes, so this file
   owns the translation table between them.

   It also implements the mouse entry points that the Win32 port left as empty
   stubs -- which is why that build had no mouse support at all, in the menus
   or in play. */

#include "sys_compat.h"

/* Engine headers FIRST -- see the note in sys_video.c. */
#include "d_global.h"
#include "sys_sdl.h"

#include "d_ints.h"
#include "d_video.h"
#include "protos.h"
#include "r_refdef.h"

extern SoundCard SC;

SDL_Window *Sys_GetWindow(void);

/* SDL scancode -> DOS set-1 scancode.  Anything not listed stays 0, which the
   engine treats as "no such key". */
static byte sdl_to_dos[SDL_SCANCODE_COUNT];

static int  mouselook_on;
static int  accum_dx, accum_dy;   /* relative motion since last read */
static int  warp_pending;


static void build_keymap(void)
{
    memset(sdl_to_dos, 0, sizeof(sdl_to_dos));

    sdl_to_dos[SDL_SCANCODE_ESCAPE]       = SC_ESCAPE;
    sdl_to_dos[SDL_SCANCODE_RETURN]       = SC_ENTER;
    sdl_to_dos[SDL_SCANCODE_KP_ENTER]     = SC_ENTER;
    sdl_to_dos[SDL_SCANCODE_SPACE]        = SC_SPACE;
    sdl_to_dos[SDL_SCANCODE_BACKSPACE]    = SC_BACKSPACE;
    sdl_to_dos[SDL_SCANCODE_TAB]          = SC_TAB;
    sdl_to_dos[SDL_SCANCODE_LALT]         = SC_ALT;
    sdl_to_dos[SDL_SCANCODE_RALT]         = SC_ALT;
    sdl_to_dos[SDL_SCANCODE_LCTRL]        = SC_CONTROL;
    sdl_to_dos[SDL_SCANCODE_RCTRL]        = SC_CONTROL;
    sdl_to_dos[SDL_SCANCODE_CAPSLOCK]     = SC_CAPSLOCK;
    sdl_to_dos[SDL_SCANCODE_NUMLOCKCLEAR] = SC_NUMLOCK;
    sdl_to_dos[SDL_SCANCODE_SCROLLLOCK]   = SC_SCROLLLOCK;
    sdl_to_dos[SDL_SCANCODE_LSHIFT]       = SC_LSHIFT;
    sdl_to_dos[SDL_SCANCODE_RSHIFT]       = SC_RSHIFT;

    sdl_to_dos[SDL_SCANCODE_UP]           = SC_UPARROW;
    sdl_to_dos[SDL_SCANCODE_DOWN]         = SC_DOWNARROW;
    sdl_to_dos[SDL_SCANCODE_LEFT]         = SC_LEFTARROW;
    sdl_to_dos[SDL_SCANCODE_RIGHT]        = SC_RIGHTARROW;
    sdl_to_dos[SDL_SCANCODE_INSERT]       = SC_INSERT;
    sdl_to_dos[SDL_SCANCODE_DELETE]       = SC_DELETE;
    sdl_to_dos[SDL_SCANCODE_HOME]         = SC_HOME;
    sdl_to_dos[SDL_SCANCODE_END]          = SC_END;
    sdl_to_dos[SDL_SCANCODE_PAGEUP]       = SC_PGUP;
    sdl_to_dos[SDL_SCANCODE_PAGEDOWN]     = SC_PGDN;
    sdl_to_dos[SDL_SCANCODE_GRAVE]        = SC_TILDA;
    sdl_to_dos[SDL_SCANCODE_COMMA]        = SC_COMMA;
    sdl_to_dos[SDL_SCANCODE_PERIOD]       = SC_PERIOD;
    sdl_to_dos[SDL_SCANCODE_MINUS]        = SC_MINUS;
    sdl_to_dos[SDL_SCANCODE_EQUALS]       = SC_PLUS;

    sdl_to_dos[SDL_SCANCODE_F1]  = SC_F1;
    sdl_to_dos[SDL_SCANCODE_F2]  = SC_F2;
    sdl_to_dos[SDL_SCANCODE_F3]  = SC_F3;
    sdl_to_dos[SDL_SCANCODE_F4]  = SC_F4;
    sdl_to_dos[SDL_SCANCODE_F5]  = SC_F5;
    sdl_to_dos[SDL_SCANCODE_F6]  = SC_F6;
    sdl_to_dos[SDL_SCANCODE_F7]  = SC_F7;
    sdl_to_dos[SDL_SCANCODE_F8]  = SC_F8;
    sdl_to_dos[SDL_SCANCODE_F9]  = SC_F9;
    sdl_to_dos[SDL_SCANCODE_F10] = SC_F10;

    sdl_to_dos[SDL_SCANCODE_1] = SC_1;
    sdl_to_dos[SDL_SCANCODE_2] = SC_2;
    sdl_to_dos[SDL_SCANCODE_3] = SC_3;
    sdl_to_dos[SDL_SCANCODE_4] = SC_4;
    sdl_to_dos[SDL_SCANCODE_5] = SC_5;
    sdl_to_dos[SDL_SCANCODE_6] = SC_6;
    sdl_to_dos[SDL_SCANCODE_7] = SC_7;
    sdl_to_dos[SDL_SCANCODE_8] = SC_8;
    sdl_to_dos[SDL_SCANCODE_9] = SC_9;
    sdl_to_dos[SDL_SCANCODE_0] = SC_0;

    sdl_to_dos[SDL_SCANCODE_A] = SC_A;
    sdl_to_dos[SDL_SCANCODE_B] = SC_B;
    sdl_to_dos[SDL_SCANCODE_C] = SC_C;
    sdl_to_dos[SDL_SCANCODE_D] = SC_D;
    sdl_to_dos[SDL_SCANCODE_E] = SC_E;
    sdl_to_dos[SDL_SCANCODE_F] = SC_F;
    sdl_to_dos[SDL_SCANCODE_G] = SC_G;
    sdl_to_dos[SDL_SCANCODE_H] = SC_H;
    sdl_to_dos[SDL_SCANCODE_I] = SC_I;
    sdl_to_dos[SDL_SCANCODE_J] = SC_J;
    sdl_to_dos[SDL_SCANCODE_K] = SC_K;
    sdl_to_dos[SDL_SCANCODE_L] = SC_L;
    sdl_to_dos[SDL_SCANCODE_M] = SC_M;
    sdl_to_dos[SDL_SCANCODE_N] = SC_N;
    sdl_to_dos[SDL_SCANCODE_O] = SC_O;
    sdl_to_dos[SDL_SCANCODE_P] = SC_P;
    sdl_to_dos[SDL_SCANCODE_Q] = SC_Q;
    sdl_to_dos[SDL_SCANCODE_R] = SC_R;
    sdl_to_dos[SDL_SCANCODE_S] = SC_S;
    sdl_to_dos[SDL_SCANCODE_T] = SC_T;
    sdl_to_dos[SDL_SCANCODE_U] = SC_U;
    sdl_to_dos[SDL_SCANCODE_V] = SC_V;
    sdl_to_dos[SDL_SCANCODE_W] = SC_W;
    sdl_to_dos[SDL_SCANCODE_X] = SC_X;
    sdl_to_dos[SDL_SCANCODE_Y] = SC_Y;
    sdl_to_dos[SDL_SCANCODE_Z] = SC_Z;
}


void Sys_InputInit(void)
{
    build_keymap();
    /* The cheat-code parser and the savegame-name entry field both read
       lastascii/newascii, which SDL_EVENT_TEXT_INPUT feeds. */
    SDL_StartTextInput(Sys_GetWindow());
}


/* Called once per Sys_Frame, before the engine reads keyboard[]. */
void Sys_RefreshKeys(void)
{
    /* sdl_bool, not bool: this array comes from SDL and is one byte per key,
       whereas the engine's bool is int-sized.  See sys_sdl.h. */
    const sdl_bool *state;
    int         numkeys, i;
    float       mx, my;
    SDL_MouseButtonFlags buttons;

    state = SDL_GetKeyboardState(&numkeys);
    if (numkeys > SDL_SCANCODE_COUNT)
        numkeys = SDL_SCANCODE_COUNT;

    memset(keyboard, 0, sizeof(bool) * NUMCODES);

    for (i = 0; i < numkeys; i++) {
        byte dos = sdl_to_dos[i];
        if (dos && state[i])
            keyboard[dos] = true;
    }

    buttons = SDL_GetMouseState(&mx, &my);
    b1 = (buttons & SDL_BUTTON_LMASK) ? 1 : 0;
    b2 = (buttons & SDL_BUTTON_RMASK) ? 1 : 0;
}


void Sys_HandleTextInput(const char *text)
{
    if (text && text[0]) {
        lastascii = text[0];
        newascii  = true;
    }
}


/* SDL_EVENT_TEXT_INPUT only reports printable characters, but the engine
   expects the DOS keyboard ISR's behaviour, where every key with an entry in
   ASCIINames produced one -- including Esc as 27, Enter as 13 and Backspace
   as 8.  Without this, PlayFLI's `while (... && !newascii)` never sees Esc and
   the intro cannot be skipped, and the menus' `case 27:` never fires.

   Both handlers stay live: this one covers the control keys, and text input
   arrives afterwards for printable keys, so a non-US layout still types the
   character actually on the keycap. */
void Sys_HandleKeyDown(int sdl_scancode, int shift, int caps)
{
    byte dos, c;

    if (sdl_scancode < 0 || sdl_scancode >= SDL_SCANCODE_COUNT)
        return;

    dos = sdl_to_dos[sdl_scancode];
    if (!dos)
        return;

    if (shift) {
        c = ShiftNames[dos];
        if (caps && c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    } else {
        c = ASCIINames[dos];
        if (caps && c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
    }

    if (c) {
        lastascii = (char)c;
        newascii  = true;
    }
}


void Sys_HandleMouseMotion(float dx, float dy)
{
    if (mouselook_on) {
        accum_dx += (int)dx;
        accum_dy += (int)dy;
    }
}


void Sys_GetMouseLook(int *dx, int *dy)
{
    if (dx) *dx = accum_dx;
    if (dy) *dy = accum_dy;
    accum_dx = 0;
    accum_dy = 0;
}


void Sys_SetMouseLook(int enabled)
{
    if (mouselook_on == (enabled != 0))
        return;

    mouselook_on = (enabled != 0);
    SDL_SetWindowRelativeMouseMode(Sys_GetWindow(), mouselook_on ? true : false);

    /* Drop whatever accumulated across the transition, or the view snaps. */
    accum_dx = 0;
    accum_dy = 0;
}


/* ---- The mouse entry points D_ints.c used to stub out ------------------- */

void M_Init(void)
{
    mouseinstalled = true;
}


void M_Shutdown(void)
{
    Sys_SetMouseLook(0);
}


void UpdateMouse(void)
{
    /* Button state is refreshed in Sys_RefreshKeys; nothing periodic needed. */
}


void ResetMouse(void)
{
    accum_dx = 0;
    accum_dy = 0;
}


/* Menu code asks for a click in 320x200 screen coordinates.  Map the window
   position back through the same 4:3 letterbox the renderer presents into. */
int MouseGetClick(short *x, short *y)
{
    float mx, my;
    int   winw, winh;
    float scale, boxw, boxh, boxx, boxy;
    SDL_MouseButtonFlags buttons;

    buttons = SDL_GetMouseState(&mx, &my);
    if (!(buttons & SDL_BUTTON_LMASK))
        return 0;

    SDL_GetWindowSize(Sys_GetWindow(), &winw, &winh);

    scale = (float)winw / 4.0f;
    if ((float)winh / 3.0f < scale)
        scale = (float)winh / 3.0f;

    boxw = 4.0f * scale;
    boxh = 3.0f * scale;
    boxx = ((float)winw - boxw) * 0.5f;
    boxy = ((float)winh - boxh) * 0.5f;

    if (mx < boxx || mx >= boxx + boxw || my < boxy || my >= boxy + boxh)
        return 0;

    if (x) *x = (short)((mx - boxx) / boxw * 320.0f);
    if (y) *y = (short)((my - boxy) / boxh * 200.0f);

    return 1;
}


void MouseHide(void)
{
    SDL_HideCursor();
}


void MouseShow(void)
{
    SDL_ShowCursor();
}
