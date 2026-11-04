// Memory that ends at a wall, for the tests. Written 2026-11-04 by Melanikus for the AvP port.
//
// A read past the end of a block of memory nearly always lands in whatever comes next, which proves nothing either way, so a test can't see
// it. So here, what's being read ends exactly where readable memory does: the next page is marked so that touching it stops the program.
// Reading even one byte past the end crashes the test, every time, and that counts as a failure. Nothing here is ever run as code.

#ifndef MEMORY_WALL_H
#define MEMORY_WALL_H

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// Gives back where to put this many bytes so that they end at the wall. There's only one wall, so each call replaces what the last one held.
static char * MemoryEndingAtAWall(size_t bytes)
{
	static char * block = 0;

	SYSTEM_INFO system;
	GetSystemInfo(&system);

	size_t const page = system.dwPageSize;
	size_t const readable = 256 * page;

	if (!block)
	{
		DWORD before = 0;
		block = (char *)VirtualAlloc(NULL, readable + page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if (!block || !VirtualProtect(block + readable, page, PAGE_NOACCESS, &before))
		{
			puts("Couldn't make memory that ends at a wall");
			exit(1);
		}
	}

	if (bytes > readable)
	{
		puts("Too many bytes for the memory that ends at a wall");
		exit(1);
	}

	return block + readable - bytes;
}

#endif
