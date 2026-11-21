
#ifndef _mempool_h_
#define _mempool_h_ 1

#define NEW_DEALLOCATION_ORDER 1

#define USE_LEVEL_MEMORY_POOL 1

#if USE_LEVEL_MEMORY_POOL
void* PoolAllocateMem(/*unsigned int*/size_t amount);
void ClearMemoryPool();

/* HOW MUCH OF THE LEVEL'S MEMORY IS USED (Frost 2026-11-21, the owner's track import: "we had 40mb of space in the RIF (before hitting engine crash
   limit) to put in anim"). A level's data comes out of 40 blocks of 1 MB, its own first; Rebellion's 1999 game writes past a block for a request bigger
   than one, and past its list for a 41st. Read only: nothing here changes what PoolAllocateMem gives. The level editor opens a level through the same code,
   so it can say what the level takes, and the track import can keep its tracks to what is left. tests/mempool_use_test.cpp. */
struct LevelMemoryPoolUse
{
	int blocksTaken;        /* of blocksMost, since the level started */
	int blocksMost;         /* 40 */
	size_t blockBytes;      /* 1 MB */
	size_t leftInBlock;     /* what the block in use can still give */
	size_t bytesHandedOut;  /* every PoolAllocateMem since the level started */
	size_t extraBytes;      /* of those, given outside the 40 blocks: a request bigger than a block, or the blocks all taken */
};
void LevelMemoryPool_Use(struct LevelMemoryPoolUse *use);
/* The words of the log line, in at most size bytes (cut, never past them): "level memory: N of 40 blocks taken (1 MB each), X MB left in the last; Y MB
   handed out, Z MB of it outside the 40 blocks" */
void LevelMemoryPool_Words(const struct LevelMemoryPoolUse *use, char *words, size_t size);
/* Writes that line to the log: once a level has loaded (winmain.cpp, after StaticLevel_BuildThisLevel) */
void LevelMemoryPool_Say(void);
#else
#define PoolAllocateMem AllocateMem
#define ClearMemoryPool() ((void)0)
#endif

#endif
