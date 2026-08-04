/* Shim for the MSVC/Watcom <io.h>: low-level file handles.

   POSIX supplies open/read/write/close/lseek already; the only genuinely
   missing piece is filelength(), which MSVC provides and POSIX does not. */
#ifndef GREED_COMPAT_IO_H
#define GREED_COMPAT_IO_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* MSVC's O_BINARY distinguishes text from binary mode; on a Unix there is no
   such distinction and the flag must be a no-op. */
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef _S_IREAD
#define _S_IREAD  S_IRUSR
#endif
#ifndef _S_IWRITE
#define _S_IWRITE S_IWUSR
#endif

static long filelength(int handle)
{
    struct stat st;
    if (fstat(handle, &st) != 0)
        return -1L;
    return (long)st.st_size;
}

#endif
