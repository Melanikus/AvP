/* mem3dc.h */
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   Removed the old memory tracker, which was never switched on. Only the two names below are left.
#ifndef MEM3DC_H_INCLUDED
#define MEM3DC_H_INCLUDED

#ifdef __cplusplus

	extern "C" {

#endif

#include "system.h"
#include <stddef.h>

/* platform specific memory allocation and deallocation declarations */
extern void *AllocMem(size_t __size);
extern void DeallocMem(void *__ptr);

// The game asks for memory through these two names. Long ago they could pass through a tracker
// that wrote down every request, so that memory never given back could be listed. That tracker
// was never turned on in this port and has been deleted, so they simply call the two functions
// above. It's still in the git history if anyone wants it.
#define AllocateMem(x) AllocMem(x)
#define DeallocateMem(x) DeallocMem(x)

#ifdef __cplusplus

	};

#endif


#endif
