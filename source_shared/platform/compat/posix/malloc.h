/* Shim for <malloc.h>.  macOS keeps the allocator declarations in <stdlib.h>
   (and <malloc/malloc.h>); there is no top-level malloc.h in the SDK. */
#ifndef GREED_COMPAT_MALLOC_H
#define GREED_COMPAT_MALLOC_H
#include <stdlib.h>
#endif
