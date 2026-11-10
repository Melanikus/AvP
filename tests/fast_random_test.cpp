// Tests for the game's random numbers (issue #121): the fresh fast_random.cpp has to give exactly the numbers the game's old ones gave,
// bit for bit, because a network game needs every machine to draw the same numbers from the same seed. The numbers it's held to are
// in fast_random_test_numbers.h, printed by today's own code in a run on GitHub, not worked out by hand: see that file for which run.
// Written 2026-11-08 by Melanikus for the AvP port.
#include "fast_random.h"
#include "fast_random_test_numbers.h"

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here may have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// Draws count numbers from a generator and says whether every one is the one it should be, and if not, which was the first that wasn't
static bool Matches(int (*draw)(void), const int32_t *expected, int count, std::string *firstWrong)
{
	for (int i = 0; i < count; i++)
	{
		int const got = draw();

		if (got != expected[i])
		{
			*firstWrong = "number " + std::to_string(i + 1) + " is " + std::to_string(got) + ", where it was " + std::to_string(expected[i]);
			return false;
		}
	}

	return true;
}

static void TestBeforeStarting()
{
	// These two run first of all, before anything here has started either generator
	std::string wrong;
	Check(Matches(SeededFastRandom, kSeededFromSeed[0].numbers, 5, &wrong), "asked before it has a seed, the seeded generator starts itself from 0 " + wrong);
	Check(Matches(FastRandom, kMainFromClock[0].numbers, 5, &wrong), "asked before the game starts it, the main generator starts itself from 0 " + wrong);
}

static void TestTheSeeded()
{
	for (const SeededNumbers &row : kSeededFromSeed)
	{
		std::string wrong;
		SetSeededFastRandom(row.seed);
		Check(Matches(SeededFastRandom, row.numbers, kSeededCount, &wrong), "from seed " + std::to_string(row.seed) + ", the seeded generator gives the numbers it always has " + wrong);

		SetSeededFastRandom(row.seed);
		Check(Matches(SeededFastRandom, row.numbers, kSeededCount, &wrong), "and the same seed again gives them again");
	}
}

static void TestTheMain()
{
	for (const MainNumbers &row : kMainFromClock)
	{
		std::string wrong;
		FastRandom_StartFrom(row.clock);
		Check(Matches(SeededFastRandom, row.seededAfter, kSeededAfterCount, &wrong), "started from the clock at " + std::to_string(row.clock) + ", the seeded generator it starts gives the numbers it always has " + wrong);
		Check(Matches(FastRandom, row.numbers, kMainCount, &wrong), "and so does the main one " + wrong);
	}

	// Every number is a whole number from 0 to 2,147,483,647
	bool inRange = true;
	FastRandom_StartFrom(987654321u);

	for (int i = 0; i < 100000; i++)
	{
		int const n = FastRandom();
		inRange = inRange && n >= 0;
	}

	Check(inRange, "100,000 numbers in a row are never below 0");
}

static void TestJoiningTheGame()
{
	std::string const maths = ReadSourceFile("3dc/maths.cpp");
	std::string const fresh = ReadSourceFile("3dc/fast_random.cpp");
	std::string const header = ReadSourceFile("3dc/fast_random.h");

	Check(!maths.empty() && !fresh.empty() && !header.empty(), "the game's files are there to read");
	Check(Has(maths, "void SetFastRandom(void)\n{\n\tFastRandom_StartFrom(timeGetTime());\n}"), "the game starts the random numbers from the clock, through the fresh code");
	Check(!Has(maths, "DEG_3") && !Has(maths, "front_ptr") && !Has(maths, "Most Excellent") && !Has(maths, "int FastRandom(void)"), "and the old code, with its 31 numbers nothing read, is gone from maths.cpp");
	Check(Has(ReadSourceFile("3dc/avp/win95/winmain.cpp"), "\tSetFastRandom();\n"), "WinMain starts them");
	Check(Has(header, "Credit: the method is the University of") && Has(header, "Jamie Lokier") && Has(fresh, "Credit: the method is"), "the fresh files credit Berkeley's method and Jamie Lokier's earlier version");
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "3dc\\fast_random.cpp") && Has(ReadSourceFile("win32/VS2026/AvP.vcxproj.filters"), "3dc\\fast_random.cpp"), "the game builds the fresh code");
}

int main()
{
	puts("1. before anything is started");
	TestBeforeStarting();
	puts("2. the seeded generator");
	TestTheSeeded();
	puts("3. the main generator");
	TestTheMain();
	puts("4. joining the game");
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
