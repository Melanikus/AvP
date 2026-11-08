// PROBE, never merged (issue #121): prints the numbers today's two random number generators give, so the fresh ones can be pinned to
// them exactly. It builds today's 3dc/maths.cpp word for word, included below, with one thing changed: its one call of the clock
// (timeGetTime, in SetFastRandom) gets a number this probe chooses, so that each run gives the same numbers. Written 2026-11-08 by
// Melanikus for the AvP port.
//
// Windows' own headers are read first, so their declarations are the real ones. Only then is the name timeGetTime made to mean the
// probe's clock, so maths.cpp's call reaches it. maths.cpp calls much of the rest of the game, which isn't here, so the linker is told
// to carry on (/FORCE:UNRESOLVED), as limits_test and far_locations_test do. Nothing called here needs any of it.

#include "os_header.h"

#include <stdint.h>
#include <stdio.h>

static uint32_t probeClock = 0;

static uint32_t ProbeClock(void)
{
	return probeClock;
}

#define timeGetTime ProbeClock
#include "maths.cpp"
#undef timeGetTime

// One line a number list, ready to copy into a test: RANDOM-PROBE <name> <count>: n, n, n, ...
static void PrintList(const char *name, const int32_t *numbers, int count)
{
	printf("RANDOM-PROBE %s %d:", name, count);

	for (int i = 0; i < count; i++)
	{
		printf(" %ld%s", (long)numbers[i], i + 1 < count ? "," : "");
	}

	printf("\n");
}

int main()
{
	// The seeded generator, from a seed given straight to it, as the network code does
	static const int32_t seeds[] = { 0, 1, 2, 12345, -1, 2147483647, (int32_t)0x80000000u, 1103515145 };
	static const int kSeededCount = 40;

	for (int32_t const seed : seeds)
	{
		int32_t numbers[kSeededCount];
		SetSeededFastRandom(seed);

		for (int i = 0; i < kSeededCount; i++)
		{
			numbers[i] = SeededFastRandom();
		}

		char name[64];
		snprintf(name, sizeof(name), "seeded-from-%ld", (long)seed);
		PrintList(name, numbers, kSeededCount);
	}

	// The main generator, started by SetFastRandom from the clock, and then the seeded one, which SetFastRandom starts from the main
	static const uint32_t clocks[] = { 0u, 1u, 12345u, 600000u, 2147483648u, 4294967295u };
	static const int kMainCount = 80;
	static const int kSeededAfterCount = 20;

	for (uint32_t const clock : clocks)
	{
		probeClock = clock;
		SetFastRandom();

		int32_t seededAfter[kSeededAfterCount];

		for (int i = 0; i < kSeededAfterCount; i++)
		{
			seededAfter[i] = SeededFastRandom();
		}

		int32_t numbers[kMainCount];

		for (int i = 0; i < kMainCount; i++)
		{
			numbers[i] = FastRandom();
		}

		char name[64];
		snprintf(name, sizeof(name), "main-from-clock-%lu", (unsigned long)clock);
		PrintList(name, numbers, kMainCount);
		snprintf(name, sizeof(name), "seeded-after-clock-%lu", (unsigned long)clock);
		PrintList(name, seededAfter, kSeededAfterCount);
	}

	printf("RANDOM-PROBE done\n");
	return 0;
}
