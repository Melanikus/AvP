// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   a level that asks for more memory than a pool block holds gets it separately, instead of writing past
//   the block; running out of memory stops the game in AllocMem, so nothing here has to handle it
// Modified 2026-11-04 by Melanikus for the AvP port:
//   the level's lists on the graphics card go when the shapes do
//   what the shoot tool has shot and stopped goes with the shapes
#include "ourasert.h"
#include "mem3dc.h"
#include "mempool.h"
#include "static_level.h"

void ShadowShoot_LevelEnded();   // see shadow_shoot.cpp: a polygon's number only means something in its own level

#include "logString.h"
#include "utilities.h"

#if USE_LEVEL_MEMORY_POOL

#define MAX_NUM_MEMORY_BLOCK 40
#define MEMORY_BLOCK_SIZE (1024*1024)

static char* MemoryBlocks[MAX_NUM_MEMORY_BLOCK];
static int CurrentMemoryBlock =-1;

static char* MemoryPoolPtr = 0;
static size_t MemoryLeft = 0;

/*
	Allocations made outside the MAX_NUM_MEMORY_BLOCK pool blocks: a request larger than MEMORY_BLOCK_SIZE, and the
	extra blocks used once all the pool blocks are taken (the GLOBALASSERTs below are empty, so these cases used to run
	past the block or past MemoryBlocks[]). Each starts with this header, which links it into a list that
	ClearMemoryPool frees with the blocks.
*/
union ExtraAllocationHeader
{
	union ExtraAllocationHeader *next;
	double align;
	char pad[16];
};

static union ExtraAllocationHeader *ExtraAllocations = 0;

// What the level has been given, for LevelMemoryPool_Use: every byte handed out, and those given outside the blocks (Frost 2026-11-21)
static size_t BytesHandedOut = 0;
static size_t ExtraBytes = 0;
static bool InExtraBlock = false;   // the 40 are all taken: the block in use is an extra one
static bool LoggedLargeRequest = false;
static bool LoggedPoolFull = false;

static char* AllocateExtra(size_t amount)
{
	union ExtraAllocationHeader *header;

	// AllocMem stops the game if the memory cannot be found, so the answer is never empty. A request this close to the
	// largest possible size would wrap round to a small one when the header is added, so ask for the largest size
	// instead: AllocMem is certain to refuse that, and the game stops with its message
	if (amount > ((size_t)-1) - sizeof(union ExtraAllocationHeader)) {
		header = (union ExtraAllocationHeader*)AllocateMem((size_t)-1);
	} else {
		header = (union ExtraAllocationHeader*)AllocateMem(sizeof(union ExtraAllocationHeader) + amount);
	}

	header->next = ExtraAllocations;
	ExtraAllocations = header;

	return (char*)(header + 1);
}

void* PoolAllocateMem(size_t amount)
{
	char *retval;

	GLOBALASSERT(amount <= MEMORY_BLOCK_SIZE)

	BytesHandedOut += amount;

	if (amount > MEMORY_BLOCK_SIZE)
	{
		// it can never fit a block (MemoryLeft -= amount would wrap): give it an allocation of its own, and leave
		// the current block as it is
		if (!LoggedLargeRequest)
		{
			LogErrorString("PoolAllocateMem - a request of " + Util::IntToString((int)amount) + " bytes is larger than a pool block; allocating it separately");
			LoggedLargeRequest = true;
		}

		ExtraBytes += amount;
		return (void*)AllocateExtra(amount);
	}

	if (amount > MemoryLeft)
	{
		char *block;

		if (CurrentMemoryBlock + 1 < MAX_NUM_MEMORY_BLOCK)
		{
			block = (char*)AllocateMem(MEMORY_BLOCK_SIZE);
			CurrentMemoryBlock++;
			GLOBALASSERT(CurrentMemoryBlock < MAX_NUM_MEMORY_BLOCK);
			MemoryBlocks[CurrentMemoryBlock] = block;
			GLOBALASSERT(MemoryBlocks[CurrentMemoryBlock]);
		}
		else
		{
			// all the pool blocks are in use: carry on in an extra block
			if (!LoggedPoolFull)
			{
				LogErrorString("PoolAllocateMem - all pool blocks are in use; allocating extra blocks");
				LoggedPoolFull = true;
			}

			block = AllocateExtra(MEMORY_BLOCK_SIZE);
			InExtraBlock = true;
		}

		MemoryLeft = MEMORY_BLOCK_SIZE;
		MemoryPoolPtr = block;
	}

	if (InExtraBlock)
	{
		ExtraBytes += amount;
	}

	retval = MemoryPoolPtr;
	MemoryLeft -= amount;
	MemoryPoolPtr += amount;

	return (void*)retval;
}


void ClearMemoryPool()
{
	// Everything that leaves a level comes through here, and this is what frees the shapes those lists were made from.
	StaticLevel_Release();
	ShadowShoot_LevelEnded();

	for (int i = 0; i <= CurrentMemoryBlock; i++)
	{
		DeallocateMem(MemoryBlocks[i]);
		MemoryBlocks[i] = 0;
	}
	CurrentMemoryBlock =- 1;
	MemoryPoolPtr = 0;
	MemoryLeft = 0;

	BytesHandedOut = 0;
	ExtraBytes = 0;
	InExtraBlock = false;

	while (ExtraAllocations)
	{
		union ExtraAllocationHeader *next = ExtraAllocations->next;
		DeallocateMem(ExtraAllocations);
		ExtraAllocations = next;
	}
	LoggedLargeRequest = false;
	LoggedPoolFull = false;
}

// How much of the level's memory is used (mempool.h). Read only. Added 2026-11-21 by Melanikus (Frost) for the AvP port, the owner's track import.
void LevelMemoryPool_Use(struct LevelMemoryPoolUse *use)
{
	if (!use)
	{
		return;
	}

	use->blocksTaken = CurrentMemoryBlock + 1;
	use->blocksMost = MAX_NUM_MEMORY_BLOCK;
	use->blockBytes = MEMORY_BLOCK_SIZE;
	// with the 40 taken, the block in use is an extra one outside them: none of the 40 has anything left
	use->leftInBlock = InExtraBlock ? 0 : MemoryLeft;
	use->bytesHandedOut = BytesHandedOut;
	use->extraBytes = ExtraBytes;
}

void LevelMemoryPool_Words(const struct LevelMemoryPoolUse *use, char *words, size_t size)
{
	if (!words || size == 0)
	{
		return;
	}

	words[0] = '\0';

	if (!use)
	{
		return;
	}

	double const mb = 1024.0 * 1024.0;

	// _snprintf cuts at size - 1 and may leave no zero: the last byte is always set
	_snprintf(words, size - 1, "level memory: %d of %d blocks taken (1 MB each), %.2f MB left in the last; %.2f MB handed out, %.2f MB of it outside the %d blocks",
		use->blocksTaken, use->blocksMost, use->leftInBlock / mb, use->bytesHandedOut / mb, use->extraBytes / mb, use->blocksMost);
	words[size - 1] = '\0';
}

void LevelMemoryPool_Say(void)
{
	struct LevelMemoryPoolUse use;
	char words[256];

	LevelMemoryPool_Use(&use);
	LevelMemoryPool_Words(&use, words, sizeof(words));
	LogString(words);
}

#endif

