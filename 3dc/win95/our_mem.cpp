// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   when the computer has no memory left to give, the game now stops with a clear message, instead of carrying
//   on with nothing and crashing somewhere else
#include "3dc.h"

#include <malloc.h>
#include <process.h>
#include <stdio.h>

#define UseLocalAssert  FALSE

#include "ourasert.h"
#include "utilities.h"

// the number the game ends with when this happens, so a log or a launcher can tell why it stopped
#define OUT_OF_MEMORY_EXIT_CODE 0x4D454D

#if debug
int alloc_cnt = 0;
int deall_cnt = 0;
#endif

void *AllocMem(size_t __size);
void DeallocMem(void *__ptr);

/*
	The computer had no memory left to give. Hundreds of places in the game ask for memory through AllocMem, and most
	of them use the answer without checking it. Handing back "no memory" would make them write to the wrong place, or
	crash somewhere that has nothing to do with the real problem. So the game stops here, in one place, and says why.
*/
static void StopGameOutOfMemory(size_t size)
{
	static int alreadyStopping = 0;

	// closing the game down can ask for memory too. If that also fails we come back here, and normal closing down
	// must not be started twice, so this time we leave at once
	if (alreadyStopping)
	{
		_exit(OUT_OF_MEMORY_EXIT_CODE);
	}
	alreadyStopping = 1;

	// the words are written here, not taken from the language file, because that may not be loaded yet
	char message[128];
	_snprintf(message, sizeof(message) - 1, "The game has run out of memory and has to stop.\n\nIt needed %u more bytes.", (unsigned int)size);
	message[sizeof(message) - 1] = '\0';

	avp_MessageBox(message, MB_OK+MB_SYSTEMMODAL);
	avp_exit(OUT_OF_MEMORY_EXIT_CODE);
}

/* Note: Never use AllocMem directly !   */
/* Instead use AllocateMem() which is a  */
/* macro defined in mem3dc.h that allows */
/* for debugging info.                   */

void *AllocMem(size_t __size)
{
	GLOBALASSERT(__size>0);
	#if debug
	alloc_cnt++;
	#endif

	void *block = malloc(__size);
	if (!block)
	{
		StopGameOutOfMemory(__size);
	}

	return block;
}

/* Note: Never use DeallocMem directly !  */
/* Instead use DeallocateMem() which is a */
/* macro defined in mem3dc.h that allows  */
/* for debugging info.                    */

void DeallocMem(void *__ptr)
{
	#if debug
	deall_cnt++;
	#endif

	if(__ptr)
		free(__ptr);

	#if debug
	else {
		OutputDebugString("ERROR - freeing null ptr\n");
		textprint("ERROR - freeing null ptr\n");
		WaitForReturn();

	}
	#endif
}
