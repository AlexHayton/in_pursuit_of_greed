/* File path resolution.

   The game was written to run from its own directory: it opens assets by bare
   name ("GREED.BLO", "SONG0.S3M", "MOVIES/TEXT.FLI") and writes settings and
   savegames right next to them.  Neither target allows that -- an .app bundle's
   Resources directory is read-only once signed, and so is a Program Files
   install on Windows.

   So reads and writes resolve differently:

     reads   -- prefs dir, then $GREED_DATA, then cwd, then the directory the
                executable came from.  Prefs comes first so a SETUP.CFG the
                player has modified wins over the pristine one shipped
                alongside the game.
     writes  -- always the prefs dir: ~/Library/Application Support/Greed/ on
                macOS, %APPDATA%\redshadow\Greed\ on Windows.  Both come from
                SDL_GetPrefPath, so this needs no per-platform code.

   sys_compat.h macros fopen/open/_open onto these, so the ~20 call sites in
   D_disk.c, Menu.c, Utils.c, Event.c and Modplay.c are untouched. */

#include "sys_compat.h"

/* Reach the real libc entry points, not our own macros. */
#undef fopen
#undef open
#undef _open

#include <stdarg.h>

/* d_global.h before sys_sdl.h -- see the long note in sys_sdl.h. */
#include "d_global.h"
#include "sys_sdl.h"

/* prefs + several GREED_DATA entries + cwd + the executable's directory. */
#define MAX_SEARCH 8

/* GREED_DATA holds a list of directories.  It cannot be colon-separated on
   Windows, where "C:\Greed" contains one. */
#ifdef _WIN32
#define GREED_DATA_SEP ";"
#else
#define GREED_DATA_SEP ":"
#endif

static char  search[MAX_SEARCH][1024];   /* read search path, in order */
static int   num_search;
static char  prefdir[1024];              /* writable directory */
static int   paths_ready;


static int is_sep(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}


static void add_search(const char *dir)
{
    size_t len;

    if (!dir || !*dir || num_search >= MAX_SEARCH)
        return;

    len = strlen(dir);
    if (len >= sizeof(search[0]) - 1)
        return;

    SDL_strlcpy(search[num_search], dir, sizeof(search[0]));
    /* Normalise to a trailing separator so callers can just concatenate.
       SDL_GetPrefPath and SDL_GetBasePath already return one, but it is a
       backslash on Windows. */
    if (!is_sep(search[num_search][len - 1]))
        SDL_strlcat(search[num_search], "/", sizeof(search[0]));
    num_search++;
}


void Sys_InitPaths(void)
{
    const char *base;
    char       *pref;
    const char *env;

    if (paths_ready)
        return;

    /* SDL_GetPrefPath allocates; SDL_GetBasePath does not (it returns an
       internally cached string that must not be freed). */
    pref = SDL_GetPrefPath("redshadow", "Greed");
    if (pref) {
        SDL_strlcpy(prefdir, pref, sizeof(prefdir));
        SDL_free(pref);
    } else {
        /* Without a prefs dir, fall back to the working directory so the game
           still runs -- saving will just land wherever it was launched. */
        SDL_strlcpy(prefdir, "./", sizeof(prefdir));
    }

    add_search(prefdir);

    /* GREED_DATA may be a list (: on POSIX, ; on Windows), so the CD-ROM tree
       and the installed tree can both be searched.  They hold different things:
       greed_cdrom/ is the un-installed master (FIRST.EXE plus the compressed
       GREED.SHR payload) and supplies only MOVIES/, while greed_final/ is the
       installed game with GREED.BLO, SETUP.CFG and the 18 music modules. */
    env = SDL_getenv("GREED_DATA");
    if (env) {
        char  list[2048];
        char *tok, *save;

        SDL_strlcpy(list, env, sizeof(list));
        for (tok = strtok_r(list, GREED_DATA_SEP, &save); tok;
             tok = strtok_r(NULL, GREED_DATA_SEP, &save))
            add_search(tok);
    }

    add_search("./");

    base = SDL_GetBasePath();
    if (base) {
#ifdef __APPLE__
        char resources[1024];
        /* Inside a bundle the executable is Contents/MacOS/, so the data sits
           one level up in Contents/Resources/. */
        SDL_snprintf(resources, sizeof(resources), "%s../Resources", base);
        add_search(resources);
#else
        /* Everywhere else the data sits next to the executable. */
        add_search(base);
#endif
    }

    paths_ready = 1;
}


/* The FLI cutscenes are the one thing the engine does not open by bare name.
   Forty sprintf sites in Intro.c, Event.c and Utils.c build DOS CD-ROM paths:

       sprintf(name,"%c:\\GREED\\MOVIES\\PRISON1.FLI",cdr_drivenum+'A');

   and cdr_drivenum is never assigned anywhere in the tree, so it is a zeroed
   global and every one of them comes out as "A:\...".  Rewriting all forty
   would mean touching four engine files; normalising here instead keeps the
   port diff where the rest of the path handling already lives.

   Turn "A:\GREED\MOVIES\TEXT.FLI" into "MOVIES/TEXT.FLI": drop the drive
   letter, flip the separators, and drop the leading GREED/ the DOS installer
   created, since MOVIES/ sits at the root of our search directories.

   Still needed on Windows.  The drive letter is bogus there too -- it is
   cdr_drivenum + 'A' with cdr_drivenum zero, i.e. the floppy drive -- and the
   GREED\ prefix still has to go.  Only the separator flip is a no-op, since
   Windows accepts both.

   Guarded on the path actually looking like DOS, so a genuine absolute path is
   never mangled. */
static int looks_like_dos_path(const char *name)
{
    return name && ((name[0] && name[1] == ':') || strchr(name, '\\') != NULL);
}


static void normalize_dos_path(const char *name, char *out, size_t outlen)
{
    size_t i = 0;

    if (name[0] && name[1] == ':')      /* drive letter */
        name += 2;

    while (*name == '\\' || *name == '/')
        name++;

    if (SDL_strncasecmp(name, "GREED\\", 6) == 0 ||
        SDL_strncasecmp(name, "GREED/", 6) == 0)
        name += 6;

    while (*name && i + 1 < outlen) {
        out[i++] = (*name == '\\') ? '/' : *name;
        name++;
    }
    out[i] = '\0';
}


/* Writes always go to the prefs directory. */
static void resolve_write(const char *name, char *out, size_t outlen)
{
    char fixed[1024];

    Sys_InitPaths();

    if (looks_like_dos_path(name)) {
        normalize_dos_path(name, fixed, sizeof(fixed));
        name = fixed;
    }

    SDL_snprintf(out, outlen, "%s%s", prefdir, name);
}


/* Reads take the first search directory where the file exists.  If it exists
   nowhere, return the first candidate so the caller reports a sensible path
   in its error message. */
static void resolve_read(const char *name, char *out, size_t outlen)
{
    char fixed[1024];
    int  i;

    Sys_InitPaths();

    if (looks_like_dos_path(name)) {
        normalize_dos_path(name, fixed, sizeof(fixed));
        name = fixed;
    }

    for (i = 0; i < num_search; i++) {
        SDL_snprintf(out, outlen, "%s%s", search[i], name);
        /* SDL_GetPathInfo rather than access(): portable, and it keeps this
           file's only libc dependency out of the Windows build, where the
           spelling would have to be _access with a bare mode number. */
        if (SDL_GetPathInfo(out, NULL))
            return;
    }

    SDL_snprintf(out, outlen, "%s%s", num_search ? search[0] : "", name);
}


/* An absolute path, or one the caller already built from a resolved path, is
   passed through untouched. */
static int is_absolute(const char *name)
{
    if (!name || !name[0])
        return 0;

#ifdef _WIN32
    /* "C:\...", "C:/..." and UNC "\\server\share" all count.  Missing the
       drive-letter case would send an already-resolved path back through the
       search list and prefix it a second time. */
    if (name[1] == ':' && (name[2] == '\\' || name[2] == '/'))
        return 1;
    if (name[0] == '\\' && name[1] == '\\')
        return 1;
#endif

    return is_sep(name[0]);
}


/* Reads get the stricter test, and the difference only matters on Windows.

   The engine's forty FLI paths -- "A:\GREED\MOVIES\PRISON1.FLI", built from an
   unassigned cdr_drivenum -- are not absolute on macOS, so they fall through
   to normalize_dos_path and get found.  On Windows they parse as a perfectly
   well-formed absolute path on drive A:, short-circuit here, and every cutscene
   fails with "File Not Found".  Requiring the file to actually exist separates
   a real caller-built path from a bogus one without having to guess which
   drive letters are meaningful. */
static int is_absolute_and_exists(const char *name)
{
    return is_absolute(name) && SDL_GetPathInfo(name, NULL);
}


FILE *Sys_fopen(const char *name, const char *mode)
{
    char path[1024];
    int  writing;

    writing = (strchr(mode, 'w') != NULL) ||
              (strchr(mode, 'a') != NULL) ||
              (strchr(mode, '+') != NULL);

    /* A write target legitimately does not exist yet, so it only gets the
       cheap test; nothing in the engine writes to a DOS-built path. */
    if (writing ? is_absolute(name) : is_absolute_and_exists(name))
        return fopen(name, mode);

    if (writing)
        resolve_write(name, path, sizeof(path));
    else
        resolve_read(name, path, sizeof(path));

    return fopen(path, mode);
}


/* libxmp opens module files itself, so it needs a real path rather than a
   name our fopen shim would resolve. */
void Sys_ResolveAsset(const char *name, char *out, unsigned long outlen)
{
    resolve_read(name, out, (size_t)outlen);
}


int Sys_open(const char *name, int flags, ...)
{
    char    path[1024];
    int     writing;
    /* int, not mode_t: the UCRT has no mode_t, and _open's third argument is
       an int there.  It is what va_arg pulls out on either platform anyway --
       POSIX promotes mode_t through the ellipsis. */
    int     mode = 0;

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    writing = (flags & (O_WRONLY | O_RDWR | O_CREAT)) != 0;

    if (writing ? is_absolute(name) : is_absolute_and_exists(name))
        return open(name, flags, mode);

    if (writing)
        resolve_write(name, path, sizeof(path));
    else
        resolve_read(name, path, sizeof(path));

    return open(path, flags, mode);
}
