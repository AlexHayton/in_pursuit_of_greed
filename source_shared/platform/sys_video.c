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
      preserve the texture's own aspect, which is precisely the wrong one.

   3. The window is created with SDL_WINDOW_HIGH_PIXEL_DENSITY.  Without it SDL
      pins the macOS backing store to 1x and the compositor bilinearly upscales
      that to the Retina panel -- so the chunky-pixel scale mode below was being
      undone by an OS blur applied afterwards.  With it we own every device
      pixel.  The cost is that window points and window pixels are no longer
      the same number: SDL delivers mouse coordinates in points, the renderer
      draws in pixels, and Sys_GetPresentRect below is unit-agnostic precisely
      so both can share one shape without sharing one unit. */

#include "sys_compat.h"

/* Engine headers FIRST: d_global.h declares `typedef enum{false,true} bool`,
   which stdbool.h (pulled in by SDL) would otherwise have macro-shadowed. */
#include "d_global.h"
#include "sys_sdl.h"

#include "d_disk.h"
#include "d_misc.h"
#include "d_video.h"
#include "protos.h"
#include "r_public.h"
#include "r_refdef.h"

extern SoundCard SC;

#define VGA_W 320
#define VGA_H 200

/* 200 * 1.2 == 240; a 4:3 box for the 320-wide image. */
#define ASPECT_W 4.0f
#define ASPECT_H 3.0f

/* HD renders the 3D view at the display's own pixel count and aspect, capped so
   a 1995 software rasteriser can still hit a frame.  1.3 Mpixel is ~20x the
   320x200 workload.  See VI_ApplyRenderMode for how the buffer shape follows
   from the display aspect. */
#define HD_MAX_PIXELS 1300000

static SDL_Window   *window;
static SDL_Renderer *renderer;
static SDL_Texture  *texture;    /* the 3D view: 320x200, or hdw x hdh in HD */
static SDL_Texture  *overlay;    /* HD only: the 320x200 chrome, keyed on 0 */

static int hdw, hdh;             /* HD view buffer size */

static Uint32 lut[256];        /* palette index -> ARGB8888 */
static byte   curpal[768];     /* current palette, 6-bit VGA levels */


static Uint32 *convbuf;        /* ARGB scratch, big enough for the 3D texture */
static int     convbuf_px;


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


void VI_SetHudScale(int scale)
/* Re-lay the chrome buffer for a new hudscale.

   `screen` is allocated once at the maximum size, so this only re-points
   ylookup and updates the pitch.  Called from VI_Init and again whenever the
   art set changes, before anything draws. */
{
    int y;

    if (scale < 1) scale = 1;
    if (scale > MAX_HUDSCALE) scale = MAX_HUDSCALE;
    hudscale = scale;
    screenpitch = SCREENWIDTH * scale;

    for (y = 0; y < SCREENHEIGHT * scale; y++)
        ylookup[y] = screen + y * screenpitch;
    /* Rows past the current height would otherwise keep pointing into the
       middle of the buffer at the old pitch. */
    for (; y < HUD_MAXHEIGHT; y++)
        ylookup[y] = screen;
}


void VI_Init(int specialbuffer)
{
    int             y;
    SDL_WindowFlags flags;

    (void)specialbuffer;

    /* InitSound has already read SETUP.CFG by the time LoadData gets here, so
       SC.fullscreen is the value from the last run.  -window overrides it for
       this run only, without writing the choice back: the default is
       fullscreen (Modplay.c's built-in defaults set SC.fullscreen=1), and a
       fullscreen window that never gains focus gets minimised by SDL, which
       makes the game impossible to observe when it is launched from a script
       rather than by hand. */
    if (MS_CheckParm("window"))
        SC.fullscreen = 0;

    flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (SC.fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow("In Pursuit of Greed",
                              VGA_W * 4, (int)(VGA_H * 1.2f) * 4,
                              flags);
    if (!window)
        MS_Error("VI_Init: SDL_CreateWindow failed: %s", SDL_GetError());

    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
        MS_Error("VI_Init: SDL_CreateRenderer failed: %s", SDL_GetError());

    SDL_SetRenderVSync(renderer, 1);

    /* Sized for the largest chrome scale up front rather than reallocated on a
       mode change: 1 MB, and it removes any chance of a stale ylookup row
       surviving a switch. */
    screen  = (byte *)malloc(HUD_MAXWIDTH * HUD_MAXHEIGHT);
    if (!screen)
        MS_Error("VI_Init: Out of memory for screen");
    memset(screen, 0, HUD_MAXWIDTH * HUD_MAXHEIGHT);

    VI_SetHudScale(hudscale);

    transparency = CA_CacheLump(CA_GetNamedNum("TRANSPARENCY"));

    for (y = 0; y < 255; y++)
        translookup[y] = transparency + 256 * y;

    rebuild_lut();

    VI_ApplyRenderMode();
}


/* Rebuild the renderer for SC.hdmode.  Safe to call again at any time; the
   menu calls it on exit when the setting has changed. */
void VI_ApplyRenderMode(void)
{
    int   pw, ph, px;
    int   oldheight;
    float aspect;

    oldheight = windowHeight;
    hdmode = SC.hdmode ? 1 : 0;

    if (hdmode) {
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        if (pw < 1 || ph < 1) { pw = VGA_W; ph = VGA_H; }
        aspect = (float)pw / (float)ph;

        /* The buffer is 1.2*aspect as wide as it is tall.  Presenting that
           stretched to fill the window reproduces exactly the 1.2 vertical
           stretch a 320x200 image already gets in a 4:3 box, which is what
           keeps the vertical field of view at its original 64 degrees while the
           horizontal opens up with the display.  See SetViewSize. */
        hdh = (int)(SDL_sqrtf((float)HD_MAX_PIXELS / (1.2f * aspect)));
        if (hdh > ph) hdh = ph;                 /* never exceed the panel */
        if (hdh > MAX_VIEW_HEIGHT) hdh = MAX_VIEW_HEIGHT;
        hdw = (int)(1.2f * aspect * (float)hdh);
        if (hdw > MAX_VIEW_WIDTH) {
            /* Wider than the scratch arrays allow; give up width-driven sizing
               and keep the shape by shrinking the height to match. */
            hdw = MAX_VIEW_WIDTH;
            hdh = (int)((float)hdw / (1.2f * aspect));
        }
        hdw &= ~1;
        hdh &= ~1;
        if (hdw < VGA_W) hdw = VGA_W;
        if (hdh < VGA_H) hdh = VGA_H;

        if (getenv("HDSMALL")) { hdw=VGA_W; hdh=VGA_H; }
        mainViewWidth = hdw;
        mainViewHeight = hdh;
    } else {
        hdw = VGA_W;
        hdh = VGA_H;
        mainViewWidth = viewSizes[currentViewSize * 2];
        mainViewHeight = viewSizes[currentViewSize * 2 + 1];
    }

    SetViewSize(mainViewWidth, mainViewHeight);
    ResetScalePostWidth(windowWidth);
    InitWalls();

    /* Carry the player's view scroll across the resize.  scrollmin is a look
       up/down offset in view rows and scrollmax is the bottom of the view, both
       measured in the OLD view's rows -- and PlayLoop copies them into the
       renderer's clip bounds every frame.  Left behind, they clip a tall HD
       view to the old 200-row one and most of the screen is never drawn at all;
       going the other way they point past the rows SetViewSize just filled in
       viewylookup.  Only ChangeViewSize used to resize the view, and it kept
       these in step; it is a no-op in HD, so this has to. */
    if (oldheight > 0 && oldheight != windowHeight)
        player.scrollmin = (player.scrollmin * windowHeight) / oldheight;
    if (player.scrollmin >  viewscroll) player.scrollmin =  viewscroll;
    if (player.scrollmin < -viewscroll) player.scrollmin = -viewscroll;
    player.scrollmax = windowHeight + player.scrollmin;
    scrollmin = player.scrollmin;
    scrollmax = player.scrollmax;

    if (texture) { SDL_DestroyTexture(texture); texture = NULL; }
    if (overlay) { SDL_DestroyTexture(overlay); overlay = NULL; }

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                hdw, hdh);
    if (!texture)
        MS_Error("VI_ApplyRenderMode: SDL_CreateTexture failed: %s", SDL_GetError());

    /* Chunky 1995 pixels, not a blur -- but not NEAREST either.  At a Retina
       backing store the destination is almost never an integer multiple of the
       source (320x200 into 2530x1898 is x 7.9, y 9.5), and NEAREST then makes
       some source pixels one device pixel wider than their neighbours.
       PIXELART keeps the hard edges while sizing every source pixel the same. */
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

    if (hdmode) {
        /* The 2D chrome stays 320x200 artwork and is drawn over the view.
           Index 0 is already the engine's "nothing here" value: every
           VI_DrawMaskedPic* skips it, so a cleared `screen` plus masked draws
           means 0 marks exactly the untouched pixels. */
        overlay = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    SCREENWIDTH * hudscale,
                                    SCREENHEIGHT * hudscale);
        if (!overlay)
            MS_Error("VI_ApplyRenderMode: overlay texture failed: %s", SDL_GetError());
        SDL_SetTextureScaleMode(overlay, SDL_SCALEMODE_PIXELART);
        SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
    }

    px = hdw * hdh;
    if (px < VGA_W * VGA_H) px = VGA_W * VGA_H;
    /* The chrome is expanded through convbuf too, and at hudscale 4 it is
       1280x800 -- bigger than the view on a small display. */
    if (px < SCREENWIDTH * hudscale * SCREENHEIGHT * hudscale)
        px = SCREENWIDTH * hudscale * SCREENHEIGHT * hudscale;
    if (px > convbuf_px) {
        free(convbuf);
        convbuf = (Uint32 *)malloc((size_t)px * sizeof(Uint32));
        if (!convbuf)
            MS_Error("VI_ApplyRenderMode: out of memory for %d pixels", px);
        convbuf_px = px;
    }

    printf("Video:\t%s, view %dx%d\n", hdmode ? "HD" : "original", hdw, hdh);
}


/* The largest centred 4:3 box inside a winw x winh surface.

   Unit-agnostic on purpose.  The renderer passes device pixels, the mouse code
   passes window points, and with SDL_WINDOW_HIGH_PIXEL_DENSITY those differ by
   the display scale.  What must not differ is the shape: this used to be
   copy-pasted into sys_input.c, and two copies of a letterbox calculation are
   two chances for a click to land somewhere other than what it looked like it
   hit. */
void Sys_GetPresentRect(float winw, float winh, SDL_FRect *dst)
{
    float scale;

    scale = winw / ASPECT_W;
    if (winh / ASPECT_H < scale)
        scale = winh / ASPECT_H;

    dst->w = ASPECT_W * scale;
    dst->h = ASPECT_H * scale;
    dst->x = (winw - dst->w) * 0.5f;
    dst->y = (winh - dst->h) * 0.5f;
}


/* Window position -> 320x200 screen coordinates, for the menus.  Returns 0 for
   a point in the letterbox bars, which are not part of any menu. */
int Sys_MapWindowToGame(float mx, float my, short *x, short *y)
{
    SDL_FRect box;
    int       winw, winh;

    /* Points, not pixels: SDL_EVENT_MOUSE_BUTTON_DOWN and SDL_GetMouseState
       both report in window points. */
    SDL_GetWindowSize(window, &winw, &winh);
    Sys_GetPresentRect((float)winw, (float)winh, &box);

    if (mx < box.x || mx >= box.x + box.w || my < box.y || my >= box.y + box.h)
        return 0;

    if (x) *x = (short)((mx - box.x) / box.w * (float)VGA_W);
    if (y) *y = (short)((my - box.y) / box.h * (float)VGA_H);

    return 1;
}


void VI_SetFullscreen(int on)
{
    if (!window)
        return;
    /* 1/0 rather than true/false: sys_sdl.h has taken those names back for the
       engine's 4-byte bool by this point, and this argument is SDL's _Bool. */
    SDL_SetWindowFullscreen(window, on ? 1 : 0);
}


int VI_GetFullscreen(void)
{
    if (!window)
        return 0;
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}


/* Expand an indexed buffer into convbuf.  `screen` and viewbuffer are both
   top-down, exactly as VGA mode 13h was -- see the long note in D_video.c
   before reintroducing any flip here. */
static void expand(const byte *src, int w, int h)
{
    int i, n = w * h;

    for (i = 0; i < n; i++)
        convbuf[i] = lut[src[i]];
}


void VI_BlitView(void)
{
    SDL_FRect   dst;
    int         winw, winh, i, n, x, y;

    if (!renderer || !texture)
        return;

    /* Pixels, not points: with no logical presentation set, SDL3 renderer
       coordinates are device pixels. */
    SDL_GetWindowSizeInPixels(window, &winw, &winh);

    /* HD, and RF_BlitView has a fresh 3D frame waiting: present the view at its
       own resolution filling the window, then the 320x200 chrome over it.

       Without a fresh frame this is a menu, a fade or a cutscene presenting
       `screen` directly, which wants the ordinary 4:3 path below.  RF_BlitView
       leaves `screen` holding a composed 320x200 copy of the last frame for
       exactly those callers. */
    if (hdmode && hdviewfresh) {
        hdviewfresh = 0;

        expand(viewbuffer, hdw, hdh);
        SDL_UpdateTexture(texture, NULL, convbuf, hdw * (int)sizeof(Uint32));

        n = screenpitch * SCREENHEIGHT * hudscale;
        for (i = 0; i < n; i++)
            convbuf[i] = screen[i] ? lut[screen[i]] : 0;
        SDL_UpdateTexture(overlay, NULL, convbuf,
                          screenpitch * (int)sizeof(Uint32));

        dst.x = 0.0f;
        dst.y = 0.0f;
        dst.w = (float)winw;
        dst.h = (float)winh;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, &dst);
        SDL_RenderTexture(renderer, overlay, NULL, &dst);
        SDL_RenderPresent(renderer);

        /* Now, and only now that the overlay has been read out of it, compose
           a 320x200 copy of what is on screen back into `screen`: the view
           nearest-downscaled in wherever the chrome left a hole.  Menus, fades,
           the pause box and the quit box all snapshot `screen` and draw over
           it, and are entitled to find the last visible frame there. */
        {
            int cw = SCREENWIDTH * hudscale, ch = SCREENHEIGHT * hudscale;

            for (y = 0; y < ch; y++) {
                byte *dest = screen + y * screenpitch;
                byte *src  = viewbuffer + ((y * hdh) / ch) * hdw;

                for (x = 0; x < cw; x++)
                    if (!dest[x])
                        dest[x] = src[(x * hdw) / cw];
            }
        }
        return;
    }

    expand(screen, screenpitch, SCREENHEIGHT * hudscale);

    /* The 320x200 texture only exists at that size in original mode; in HD the
       main texture is the view buffer's size, so borrow the overlay. */
    if (hdmode) {
        SDL_UpdateTexture(overlay, NULL, convbuf,
                          screenpitch * (int)sizeof(Uint32));
        Sys_GetPresentRect((float)winw, (float)winh, &dst);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_NONE);
        SDL_RenderTexture(renderer, overlay, NULL, &dst);
        SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
        SDL_RenderPresent(renderer);
        return;
    }

    SDL_UpdateTexture(texture, NULL, convbuf,
                      screenpitch * (int)sizeof(Uint32));
    Sys_GetPresentRect((float)winw, (float)winh, &dst);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_RenderPresent(renderer);
}


/* TEMPORARY */
void VI_DumpPresented(char *path)
{
    SDL_Surface *s,*c; FILE *f; int y;
    s=SDL_RenderReadPixels(renderer,NULL); if(!s) return;
    f=fopen(path,"wb");
    if(f){ c=SDL_ConvertSurface(s,SDL_PIXELFORMAT_RGB24);
      if(c){ fprintf(f,"P6\n%d %d\n255\n",c->w,c->h);
        for(y=0;y<c->h;y++) fwrite((byte*)c->pixels+(size_t)y*c->pitch,3,c->w,f);
        SDL_DestroySurface(c);} fclose(f); }
    SDL_DestroySurface(s);
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
    if (overlay)  { SDL_DestroyTexture(overlay);   overlay = NULL; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
    if (window)   { SDL_DestroyWindow(window);     window = NULL; }
    if (convbuf)  { free(convbuf); convbuf = NULL; }
}


void Sys_ErrorBox(const char *message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                             "In Pursuit of Greed", message, window);
}
