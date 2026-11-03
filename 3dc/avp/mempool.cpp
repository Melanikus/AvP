#include "ourasert.h"
#include "mem3dc.h"
#include "mempool.h"
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
static bool LoggedLargeRequest = false;
static bool LoggedPoolFull = false;

static char* AllocateExtra(size_t amount)
{
	union ExtraAllocationHeader *header;

	if (amount > ((size_t)-1) - sizeof(union ExtraAllocationHeader)) {
		return 0;
	}

	header = (union ExtraAllocationHeader*)AllocateMem(sizeof(union ExtraAllocationHeader) + amount);
	if (!header) {
		return 0;
	}

	header->next = ExtraAllocations;
	ExtraAllocations = header;

	return (char*)(header + 1);
}

void* PoolAllocateMem(size_t amount)
{
	char *retval;

	GLOBALASSERT(amount <= MEMORY_BLOCK_SIZE)

	if (amount > MEMORY_BLOCK_SIZE)
	{
		// it can never fit a block (MemoryLeft -= amount would wrap): give it an allocation of its own, and leave
		// the current block as it is
		if (!LoggedLargeRequest)
		{
			LogErrorString("PoolAllocateMem - a request of " + Util::IntToString((int)amount) + " bytes is larger than a pool block; allocating it separately");
			LoggedLargeRequest = true;
		}

		retval = AllocateExtra(amount);
		if (!retval) {
			LogErrorString("PoolAllocateMem - out of memory");
		}
		return (void*)retval;
	}

	if (amount > MemoryLeft)
	{
		char *block;

		if (CurrentMemoryBlock + 1 < MAX_NUM_MEMORY_BLOCK)
		{
			block = (char*)AllocateMem(MEMORY_BLOCK_SIZE);
			if (!block)
			{
				// keep the pool as it was; the caller gets NULL
				LogErrorString("PoolAllocateMem - out of memory");
				return 0;
			}
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
			if (!block)
			{
				LogErrorString("PoolAllocateMem - out of memory");
				return 0;
			}
		}

		MemoryLeft = MEMORY_BLOCK_SIZE;
		MemoryPoolPtr = block;
	}

	retval = MemoryPoolPtr;
	MemoryLeft -= amount;
	MemoryPoolPtr += amount;

	return (void*)retval;
}


void ClearMemoryPool()
{
	for (int i = 0; i <= CurrentMemoryBlock; i++)
	{
		DeallocateMem(MemoryBlocks[i]);
		MemoryBlocks[i] = 0;
	}
	CurrentMemoryBlock =- 1;
	MemoryPoolPtr = 0;
	MemoryLeft = 0;

	while (ExtraAllocations)
	{
		union ExtraAllocationHeader *next = ExtraAllocations->next;
		DeallocateMem(ExtraAllocations);
		ExtraAllocations = next;
	}
	LoggedLargeRequest = false;
	LoggedPoolFull = false;
}

#endif

