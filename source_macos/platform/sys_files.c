/* File path resolution for the macOS port.

   The game was written to run from its own directory: it opens assets by bare
   name ("GREED.BLO", "SONG0.S3M", "MOVIES/TEXT.FLI") and writes settings and
   savegames right next to them.  That cannot work from an .app bundle, whose
   Resources directory is read-only once signed.

   So reads and writes resolve differently:

     reads   -- prefs dir, then $GREED_DATA, then cwd, then the bundle's
                Resources.  Prefs comes first so a SETUP.CFG the player has
                modified wins over the pristine one shipped in the bundle.
     writes  -- always the prefs dir, ~/Library/Application Support/Greed/.

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

/* prefs + several GREED_DATA entries + cwd + bundle Resources. */
#define MAX_SEARCH 8

static char  search[MAX_SEARCH][1024];   /* read search path, in order */
static int   num_search;
static char  prefdir[1024];              /* writable directory */
static int   paths_ready;


static void add_search(const char *dir)
{
    size_t len;

    if (!dir || !*dir || num_search >= MAX_SEARCH)
        return;

    len = strlen(dir);
    if (len >= sizeof(search[0]) - 1)
        return;

    SDL_strlcpy(search[num_search], dir, sizeof(search[0]));
    /* Normalise to a trailing slash so callers can just concatenate. */
    if (search[num_search][len - 1] != '/')
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

    /* GREED_DATA may be a colon-separated list, so the CD-ROM tree and the
       installed tree can both be searched.  They hold different things:
       greed_cdrom/ is the un-installed master (FIRST.EXE plus the compressed
       GREED.SHR payload) and supplies only MOVIES/, while greed_final/ is the
       installed game with GREED.BLO, SETUP.CFG and the 18 music modules. */
    env = SDL_getenv("GREED_DATA");
    if (env) {
        char  list[2048];
        char *tok, *save;

        SDL_strlcpy(list, env, sizeof(list));
        for (tok = strtok_r(list, ":", &save); tok; tok = strtok_r(NULL, ":", &save))
            add_search(tok);
    }

    add_search("./");

    base = SDL_GetBasePath();
    if (base) {
        char resources[1024];
        /* Inside a bundle the executable is Contents/MacOS/, so the data sits
           one level up in Contents/Resources/. */
        SDL_snprintf(resources, sizeof(resources), "%s../Resources", base);
        add_search(resources);
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

   Guarded on the path actually looking like DOS, so a genuine absolute POSIX
   path is never mangled. */
static int looks_like_dos_path(const char *name)
{
    return name && ((name[0] && name[1] == ':') || strchr(name, '\\') != NULL);
}


static void dos_to_posix(const char *name, char *out, size_t outlen)
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
        dos_to_posix(name, fixed, sizeof(fixed));
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
        dos_to_posix(name, fixed, sizeof(fixed));
        name = fixed;
    }

    for (i = 0; i < num_search; i++) {
        SDL_snprintf(out, outlen, "%s%s", search[i], name);
        if (access(out, R_OK) == 0)
            return;
    }

    SDL_snprintf(out, outlen, "%s%s", num_search ? search[0] : "", name);
}


/* An absolute path, or one the caller already built from a resolved path, is
   passed through untouched. */
static int is_absolute(const char *name)
{
    return name && name[0] == '/';
}


FILE *Sys_fopen(const char *name, const char *mode)
{
    char path[1024];
    int  writing;

    if (is_absolute(name))
        return fopen(name, mode);

    writing = (strchr(mode, 'w') != NULL) ||
              (strchr(mode, 'a') != NULL) ||
              (strchr(mode, '+') != NULL);

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
    mode_t  mode = 0;

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }

    if (is_absolute(name))
        return open(name, flags, mode);

    writing = (flags & (O_WRONLY | O_RDWR | O_CREAT)) != 0;

    if (writing)
        resolve_write(name, path, sizeof(path));
    else
        resolve_read(name, path, sizeof(path));

    return open(path, flags, mode);
}
