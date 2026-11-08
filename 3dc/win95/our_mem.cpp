// AvP port by Barry Duncan.
// Modified 2026-11-06 by Melanikus for the AvP port:
//   before any memory is freed, a sound still holding an address in it forgets it. See sound_address.h.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   when the computer has no memory left to give, the game now stops with a clear message, instead of carrying
//   on with nothing and crashing somewhere else; a block can now change size, and a list can be asked for by
//   count, with the same promise; the same goes for a plain new that fails, on any thread
#include "3dc.h"

#include <malloc.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <exception>
#include <new>

#define UseLocalAssert  FALSE

#include "ourasert.h"
#include "utilities.h"
#include "logString.h"

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
	// size is 0 when the game can't know it: a plain new that failed doesn't say how much it asked for
	char message[128];
	if (size)
	{
		_snprintf(message, sizeof(message) - 1, "The game has run out of memory and has to stop.\n\nIt needed %u more bytes.",
			(unsigned int)size);
	}
	else
	{
		_snprintf(message, sizeof(message) - 1, "The game has run out of memory and has to stop.");
	}
	message[sizeof(message) - 1] = '\0';

	avp_MessageBox(message, MB_OK+MB_SYSTEMMODAL);
	avp_exit(OUT_OF_MEMORY_EXIT_CODE);
}

// In psnd.cpp. A sound may be holding an address inside memory that's about to go.
extern void Sound_ForgetAddressesInside(const void *start, size_t bytes);

// The thread the game started on: the only one that may shut Direct3D down.
static DWORD mainThreadId = 0;

/*
	Most of the game asks for memory with a plain "new". When that fails it throws an exception, and nothing in the game
	catches one, so the C++ runtime ends the game by calling abort(), with no message at all. abort() gives this function
	a chance first. If the reason is a failed new, the game stops the way AllocMem stops it. Anything else carries on
	to abort as it always did.

	Only the main thread can do that properly. Any other thread (music, video, the libraries' own) must not shut Direct3D
	down while the main thread is using it, and a message box from one could wait behind a full screen for ever, so it
	leaves a line in the log and ends the game at once.
*/
static void __cdecl OnAbort(int)
{
	std::exception_ptr failure = std::current_exception();
	if (!failure)
	{
		return;
	}

	try
	{
		std::rethrow_exception(failure);
	}
	catch (const std::bad_alloc &)
	{
		if (GetCurrentThreadId() == mainThreadId)
		{
			StopGameOutOfMemory(0);
		}

		LogErrorStringC("The game ran out of memory on a background thread and has to stop");
		_exit(OUT_OF_MEMORY_EXIT_CODE);
	}
	catch (...)
	{
	}
}

/*
	Called once, as the game starts. Measured with this compiler: std::set_new_handler would not do, because it also fires
	for new(std::nothrow), which the audio and video code relies on to get 0 back, and std::set_terminate only covers the
	thread that calls it. A SIGABRT handler covers every thread with this one call.
*/
void StopGameIfNewFails(void)
{
	mainThreadId = GetCurrentThreadId();
	signal(SIGABRT, OnAbort);
}

// In psnd.cpp. A sound may be holding an address inside memory that's about to go.
extern void Sound_ForgetAddressesInside(const void *start, size_t bytes);

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

/*
	Changes the size of a block from AllocMem and keeps what is in it. When realloc fails it answers 0 and leaves the old
	block where it was, so the usual p = realloc(p, n) loses the block as well. There is nothing to rescue here: the game
	stops, as it does in AllocMem, so the answer never needs checking.
*/
void *ReallocMem(void *__ptr, size_t __size)
{
	GLOBALASSERT(__size>0);

	void *block = realloc(__ptr, __size);
	if (!block)
	{
		StopGameOutOfMemory(__size);
	}

	return block;
}

/*
	Asks for room for a list of __count things of __size bytes each. The count often comes from a file, and a big enough
	count times the size is more than a number can hold: it wraps round to a small number, and the list comes out far
	too short for its count. A list that size could never fit in memory anyway, so it stops the game like any other
	request that cannot be met. A negative count from a file arrives here as a huge one, and stops the same way.
*/
void *AllocArrayMem(size_t __count, size_t __size)
{
	if (__size != 0 && __count > (size_t)-1 / __size)
	{
		StopGameOutOfMemory((size_t)-1);
	}

	return AllocMem(__count * __size);
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
	{
		Sound_ForgetAddressesInside(__ptr, _msize(__ptr));
		free(__ptr);
	}

	#if debug
	else {
		OutputDebugString("ERROR - freeing null ptr\n");
		textprint("ERROR - freeing null ptr\n");
		WaitForReturn();

	}
	#endif
}
