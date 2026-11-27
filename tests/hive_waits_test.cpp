// A test of how long the hive and its generators wait (3dc/avp/hive_waits.h). Written 2026-11-27 by Melanikus (Drake) for the AvP port.
//
// WHAT IT TESTS
//
// hive_waits.h works out a wait "an eighth either way" in 64 bits, where 1999's sums ran over a whole number for any wait over 4,681 s (the header
// says why it matters). This test gives it the waits the game's own levels ask for, the longest a level can, and the ones that ran over, with random
// numbers chosen here, and checks each answer against what it should be. For every wait 1999's sums held, it checks that the answer is theirs to the
// last 65536th of a second, and that the random number is asked for exactly when they asked for it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file with the settings in tests/hive_waits_test.args. It needs nothing but the header. The program prints a line for
// each check, and gives back a non-zero result if any check failed.

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "hive_waits.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	checks++;
	printf("%s: %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static int64_t const OneSecond = 65536;   // ONE_FIXED
static int const ShortestGeneratorWait = 3 * 65536;    // GENERATORTIME_MIN (bh_generator.h)
static int const LongestGeneratorWait = 120 * 65536;   // GENERATORTIME_MAX

// The random number the wait is given, chosen here, and how many times it was asked for
static int randomGives = 0;
static int timesAsked = 0;

static int ChosenRandom(void)
{
	timesAsked++;
	return randomGives;
}

static int64_t WaitWith(int64_t baseTime, int random)
{
	randomGives = random;
	return HiveWaits_AnEighthEitherWay(baseTime, ChosenRandom);
}

// 1999's sums, as bh_generator.cpp had them, given a random number of 0 or more. Only for waits they held: past 4,681 s they run over, which C++
// leaves undefined, so they aren't asked to.
static int Sums1999(int baseTime, int random)
{
	return ((baseTime*7)/8) + ((baseTime/4) ? random%(baseTime/4) : 0);
}

static void TestTheSameAs1999()
{
	puts("1. every wait 1999's sums held comes out as it did");

	int const longestTheyHeld = 4681 * 65536;   // seven times this is 2,147,418,112, just under 2^31
	int const randoms[] = { 0, 1, 12345, 65535, 1 << 30, INT_MAX };
	int differ = 0, tried = 0;

	for (int64_t base = -60 * OneSecond; base <= longestTheyHeld; base += (base >= -8 && base < 300) ? 1 : 9973)
	{
		for (int r : randoms)
		{
			tried++;
			differ += (WaitWith(base, r) != Sums1999((int)base, r)) ? 1 : 0;
		}
	}

	for (int base : { 60 * 65536, 120 * 65536, 180 * 65536, 1000 * 65536, longestTheyHeld })
	{
		int const quarter = base / 4;

		for (int r : { 0, quarter - 1, quarter, INT_MAX })
		{
			tried++;
			differ += (WaitWith(base, r) != Sums1999(base, r)) ? 1 : 0;
		}
	}

	Check(tried > 150000 && differ == 0, "from -60 s to 4,681 s, with no random part, the most and others between, every wait is 1999's to the last 65536th");
	Check(WaitWith(60 * OneSecond, 0) == 3440640, "the game's own 60 s hive waits 52.5 s at the least, as it did");

	timesAsked = 0;
	(void)WaitWith(3, 0);
	Check(timesAsked == 0, "a wait whose quarter is 0 doesn't ask for a random number, as 1999's didn't");
	(void)WaitWith(4, 0);
	Check(timesAsked == 1, "a wait whose quarter isn't 0 asks for one, once");
}

static void TestTheLevelsLongWaits()
{
	puts("2. the waits the game's own levels ask for, past 4,681 s");

	int64_t const base = 32000 * OneSecond;
	int64_t const quarter = base / 4;

	Check(WaitWith(base, 0) == 28000 * OneSecond, "a level's 32,000 s hive waits 28,000 s at the least, as its makers meant (1999's sums gave 3,424 s)");
	Check(WaitWith(base, (int)(quarter - 1)) == 36000 * OneSecond - 1, "and just under 36,000 s at the most");
	Check(HiveWaits_Kept(WaitWith(base, 0), 0, INT_MAX) == 28000 * 65536, "kept to what the game holds, 28,000 s is still 28,000 s");
	Check(HiveWaits_Kept(WaitWith(base, (int)(quarter - 1)), 0, INT_MAX) == INT_MAX,
		"and the most is kept to the most the game's whole number holds, just under 32,768 s, not wrapped round");

	for (int seconds : { 15001, 20000, 24000, 30000 })
	{
		int64_t const at = seconds * OneSecond;
		Check(WaitWith(at, 0) == (at * 7) / 8, "the game's other long waits give seven eighths of themselves at the least");
	}

	int64_t const longest = 32767 * OneSecond;   // the longest a level can ask for now (GlobalGenerator_LongestHiveSeconds, avp_chunk.hpp)
	Check(WaitWith(longest, 0) == (longest * 7) / 8 && WaitWith(longest, 0) > 28000 * OneSecond, "the longest a level can ask for, 32,767 s, gives just over 28,671 s");
}

static void TestTheGenerators()
{
	puts("3. the generators' waits");

	// A generator's own rate is NPCs a minute times 100 (bh_generator.cpp): 1 is one NPC every 100 minutes
	int64_t const everyHundredMinutes = (60 * 65536 * 100) / 1;
	Check(HiveWaits_Kept(WaitWith(everyHundredMinutes, 0), ShortestGeneratorWait, LongestGeneratorWait) == LongestGeneratorWait,
		"a generator making one NPC every 100 minutes waits the longest, 120 s (1999's sums ran over and gave the shortest, 3 s)");

	int64_t const twice = (60 * 65536 * 100) / 2;
	Check(HiveWaits_Kept(WaitWith(twice, 0), ShortestGeneratorWait, LongestGeneratorWait) == LongestGeneratorWait, "and one making two, too");

	Check(HiveWaits_SharedGeneratorTime(4, 13) == (60 * 65536 / 4) * 13, "13 generators sharing 4 NPCs a minute share the wait as before");
	Check(HiveWaits_SharedGeneratorTime(0, 5) == HiveWaits_SharedGeneratorTime(1, 5), "fewer than 1 NPC a minute is taken as 1");
	Check(HiveWaits_SharedGeneratorTime(-7, 5) == HiveWaits_SharedGeneratorTime(1, 5), "and so is a negative number");
	Check(HiveWaits_SharedGeneratorTime(1, 1000) == (int64_t)3932160 * 1000, "1,000 generators sharing one NPC a minute: a wait no whole number holds, worked out whole");

	Check(HiveWaits_Kept(WaitWith(HiveWaits_SharedGeneratorTime(1, 79), 0), ShortestGeneratorWait, LongestGeneratorWait) == LongestGeneratorWait,
		"79 generators sharing one NPC a minute wait the longest, 120 s (1999's sums ran over and gave the shortest, 3 s)");
	Check(HiveWaits_Kept(WaitWith(HiveWaits_SharedGeneratorTime(1, 1000), 0), ShortestGeneratorWait, LongestGeneratorWait) == LongestGeneratorWait,
		"and so do 1,000");
	Check(HiveWaits_Kept(WaitWith(HiveWaits_SharedGeneratorTime(60, 27), 0), ShortestGeneratorWait, LongestGeneratorWait) == (27 * 65536 * 7) / 8,
		"27 generators sharing 60 a minute, as one of the game's own levels has, wait 23.6 s at the least, as before");
}

static void TestKeeping()
{
	puts("4. keeping a wait between two numbers");

	Check(HiveWaits_Kept(2, 3, 120) == 3, "a wait below the shortest is the shortest");
	Check(HiveWaits_Kept(121, 3, 120) == 120, "a wait above the longest is the longest");
	Check(HiveWaits_Kept(50, 3, 120) == 50, "a wait between them is itself");
	Check(HiveWaits_Kept((int64_t)INT_MAX * 9, 0, INT_MAX) == INT_MAX, "a wait far past what a whole number holds is the most one does");
	Check(HiveWaits_Kept(-(int64_t)INT_MAX * 9, 0, INT_MAX) == 0, "and one far below 0 is 0");
}

int main()
{
	// Print each line straight away. If the code under test crashes, the lines so far still show how far it got.
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheSameAs1999();
	TestTheLevelsLongWaits();
	TestTheGenerators();
	TestKeeping();

	printf("%d checks, %d failed\n", checks, checksFailed);
	printf("%s\n", checksFailed ? "The hive waits test FAILED" : "The hive waits test passed");
	return checksFailed ? 1 : 0;
}
