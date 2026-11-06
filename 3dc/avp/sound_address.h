// Written 2026-11-06 by Melanikus for the AvP port.
//
// A sound can hold the address of a number in whatever started it, and write there when it ends. If the owner has gone by then, that address is freed memory, and the write damages whatever uses that memory next. So memory about to be freed asks each sound whether its address is inside. This is the question, by itself so that a test can ask it.
#pragma once

#include <stddef.h>

inline bool SoundAddressIsInside(const int *address, const void *start, size_t bytes)
{
	if (address == NULL)
	{
		return false;
	}

	char const *const place = (char const *)address;
	char const *const first = (char const *)start;

	// Numbers rather than the end's address, so that a block at the very top of memory can't wrap round
	return place >= first && (size_t)(place - first) < bytes;
}
