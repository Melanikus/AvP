
#ifndef _mempool_h_
#define _mempool_h_ 1

#define NEW_DEALLOCATION_ORDER 1

// The level memory pool hands out a level's objects from big blocks of its own. The game always has it, as in 1999. Modified 2026-11-10 by
// Melanikus for the AvP port (SAVE-HOSTILE-1): a sanitizer build alone turns it off, by building with USE_LEVEL_MEMORY_POOL set to 0, so
// that each object is an allocation of its own and the sanitizer sees a write past one. A player's build never sets it (AvP.vcxproj
// doesn't name it, and save_test_rules_test checks that), so it's always 1 there.
#ifndef USE_LEVEL_MEMORY_POOL
#define USE_LEVEL_MEMORY_POOL 1
#endif

#if USE_LEVEL_MEMORY_POOL
void* PoolAllocateMem(/*unsigned int*/size_t amount);
void ClearMemoryPool();
#else
#define PoolAllocateMem AllocateMem
#define ClearMemoryPool() ((void)0)
#endif

#endif
