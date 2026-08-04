/* SDL3 video backend: the GDI half of the old D_video.c.

   The engine renders into a 320x200 8-bit paletted buffer called `screen` and
   calls VI_BlitView() to show it.  Here that buffer is expanded through a
   palette lookup into an ARGB texture and presented.

   Two details that are easy to get wrong:

   1. `screen` is TOP-DOWN, exactly as VGA mode 13h was, and this file
      presents it row for row.  There is no flip here and there must not be
      one: the DOS RF_BlitView (RA_DRAW.ASM) is a plain `rep movsd` into VGA
      memory.  The Win32 C rewrite's viewylookup[199-i] copy looks like an
      engine invariant but was only ever compensation for GDI's bottom-up
      CreateDIBSection; it has been removed from D_video.c, where there is a
      longer note.  Having both that reversal and a flip here cancelled out
      for the 3D view -- so gameplay looked correct -- while leaving the menus,
      the FLI cutscenes and the status bar upside down.

   2. 320x200 was displayed as 4:3 on a CRT, i.e. with non-square pixels.
      Presenting it at its literal 1.6:1 aspect makes everything look
      squashed, so the destination rect is computed for 4:3.  That is also why
      SDL_SetRenderLogicalPresentation isn't used -- its letterbox mode would
      preserve the texture's own aspect, which is precisely the wrong one. */

#include "sys_compat.h"

/* Engine headers FIRST: d_global.h declares `typedef enum{false,true} bool`,
   which stdbool.h (pulled in by SDL) would otherwise have macro-shadowed. */
#include "d_global.h"
#include "sys_sdl.h"

#include "d_disk.h"
#include "d_misc.h"
#include "d_video.h"
#include "r_public.h"
#include "r_refdef.h"

#define VGA_W 320
#define VGA_H 200

/* 200 * 1.2 == 240; a 4:3 box for the 320-wide image. */
#define ASPECT_W 4.0f
#define ASPECT_H 3.0f

static SDL_Window   *window;
static SDL_Renderer *renderer;
static SDL_Texture  *texture;

static Uint32 lut[256];        /* palette index -> ARGB8888 */
static byte   curpal[768];     /* current palette, 6-bit VGA levels */

static Uint32 *convbuf;        /* 320x200 ARGB scratch for the row flip */


SDL_Window *Sys_GetWindow(void)
{
    return window;
}


static void rebuild_lut(void)
{
    int i;

    for (i = 0; i < 256; i++) {
        /* VGA DAC levels are 0..63; scale to 0..255.  Using <<2 alone would
           cap at 252 and never reach full white, so replicate the top bits. */
        Uint32 r = (Uint32)curpal[i * 3 + 0] & 0x3f;
        Uint32 g = (Uint32)curpal[i * 3 + 1] & 0x3f;
        Uint32 b = (Uint32)curpal[i * 3 + 2] & 0x3f;

        r = (r << 2) | (r >> 4);
        g = (g << 2) | (g >> 4);
        b = (b << 2) | (b >> 4);

        lut[i] = 0xff000000u | (r << 16) | (g << 8) | b;
    }
}


void VI_Init(int specialbuffer)
{
    int y;

    (void)specialbuffer;

    window = SDL_CreateWindow("In Pursuit of Greed",
                              VGA_W * 4, (int)(VGA_H * 1.2f) * 4,
                              SDL_WINDOW_RESIZABLE);
    if (!window)
        MS_Error("VI_Init: SDL_CreateWindow failed: %s", SDL_GetError());

    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
        MS_Error("VI_Init: SDL_CreateRenderer failed: %s", SDL_GetError());

    SDL_SetRenderVSync(renderer, 1);

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                VGA_W, VGA_H);
    if (!texture)
        MS_Error("VI_Init: SDL_CreateTexture failed: %s", SDL_GetError());

    /* Chunky 1995 pixels, not a blur. */
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    convbuf = (Uint32 *)malloc(VGA_W * VGA_H * sizeof(Uint32));
    screen  = (byte *)malloc(VGA_W * VGA_H);
    if (!screen || !convbuf)
        MS_Error("VI_Init: Out of memory for screen");
    memset(screen, 0, VGA_W * VGA_H);

    for (y = 0; y < SCREENHEIGHT; y++)
        ylookup[y] = screen + y * SCREENWIDTH;

    transparency = CA_CacheLump(CA_GetNamedNum("TRANSPARENCY"));

    for (y = 0; y < 255; y++)
        translookup[y] = transparency + 256 * y;

    rebuild_lut();
}


void VI_BlitView(void)
{
    int         x, y;
    SDL_FRect   dst;
    int         winw, winh;
    float       scale;

    if (!renderer || !texture)
        return;

    /* Expand indexed -> ARGB, row for row.  `screen` is top-down, exactly as
       VGA mode 13h was -- see the long note in D_video.c before reintroducing
       any flip here. */
    for (y = 0; y < VGA_H; y++) {
        const byte *src = screen + y * VGA_W;
        Uint32     *dstrow = convbuf + y * VGA_W;

        for (x = 0; x < VGA_W; x++)
            dstrow[x] = lut[src[x]];
    }

    SDL_UpdateTexture(texture, NULL, convbuf, VGA_W * (int)sizeof(Uint32));

    SDL_GetWindowSizeInPixels(window, &winw, &winh);

    /* Largest 4:3 box that fits, centred. */
    scale = (float)winw / ASPECT_W;
    if ((float)winh / ASPECT_H < scale)
        scale = (float)winh / ASPECT_H;

    dst.w = ASPECT_W * scale;
    dst.h = ASPECT_H * scale;
    dst.x = ((float)winw - dst.w) * 0.5f;
    dst.y = ((float)winh - dst.h) * 0.5f;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_RenderPresent(renderer);
}


void VI_SetPalette(byte *palette)
{
    memcpy(curpal, palette, 768);
    rebuild_lut();
    VI_BlitView();
}


void VI_GetPalette(byte *palette)
{
    /* The Win32 port left this empty, which is why VI_FadeIn/VI_FadeOut in
       D_video.c faded from whatever happened to be on the stack.  Returning
       the real palette makes the menu and intro fades work. */
    memcpy(palette, curpal, 768);
}


void VI_FillPalette(int red, int green, int blue)
{
    int i;

    /* Also an empty stub in the Win32 port.  VI_FadeOut calls it to land on a
       flat colour at the end of a fade. */
    for (i = 0; i < 256; i++) {
        curpal[i * 3 + 0] = (byte)red;
        curpal[i * 3 + 1] = (byte)green;
        curpal[i * 3 + 2] = (byte)blue;
    }
    rebuild_lut();
    VI_BlitView();
}


void VI_ResetPalette(void)
{
    /* Existed to re-realize the GDI palette after a WM_PAINT.  Nothing to do
       when we own the pixels. */
}


void Sys_VideoShutdown(void)
{
    if (texture)  { SDL_DestroyTexture(texture);   texture = NULL; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
    if (window)   { SDL_DestroyWindow(window);     window = NULL; }
    if (convbuf)  { free(convbuf); convbuf = NULL; }
}


void Sys_ErrorBox(const char *message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                             "In Pursuit of Greed", message, window);
}
