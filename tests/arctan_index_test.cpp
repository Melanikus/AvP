// The test for 3dc/arctan_index.h: ArcTan's place in its table, at the edges of what an int holds. Written 2026-11-08 by Melanikus for the
// AvP port. tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game.

#include <limits.h>
#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "arctan_index.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// Reads a file of the game as text, as save_checks_test does. /FC in this test's settings makes __FILE__ the whole path of this file, so
// the repository's top is found from it.
static std::string ReadSourceFile(char const *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

// The place ArcTan's 32-bit sum used to give, for the same two sizes, where it fits: to show the 64-bit one gives the same answers
static int OldPlace(int shorter, int longer)
{
	return (shorter << 8) / longer;
}

int main()
{
	printf("Sizes\n");
	Check(ArcTan_Size(5) == 5 && ArcTan_Size(-5) == 5 && ArcTan_Size(0) == 0, "a side's size is its length, either way");
	Check(ArcTan_Size(INT_MIN) == 2147483648LL, "and the most negative int's size is 2147483648, which no int holds");

	printf("Places in the table\n");
	bool same = true;

	for (int longer = 1; longer < 2000; longer += 7)
	{
		for (int shorter = 0; shorter < longer; shorter += 3)
		{
			same = same && ArcTan_TableIndex(shorter, longer) == OldPlace(shorter, longer);
		}
	}

	Check(same, "for sides that fitted before, every place is the one the 32-bit sum gave");

	// A side just over 2^23 times 256 doesn't fit in 32 bits: the old sum went negative, and read before the table
	int const big = (1 << 23) + 1;
	Check(ArcTan_TableIndex(big, big + 1) == 255, "a side over 8.4 km, just shorter than the other, is the table's last place, 255");
	Check(ArcTan_TableIndex(2147483647LL, 2147483648LL) == 255, "and the largest sizes there are give 255 too");
	Check(ArcTan_TableIndex(1, 2147483648LL) == 0, "a short side against the longest gives 0");

	bool inTable = true;
	long long const edges[] = { 0, 1, 255, 256, 8388607, 8388608, 8388609, 16777216, 2147483646LL, 2147483647LL, 2147483648LL };

	for (long long const a : edges)
	{
		for (long long const b : edges)
		{
			int const place = ArcTan_TableIndex(a, b);
			inTable = inTable && place >= 0 && place <= 255;
		}
	}

	Check(inTable, "for every pair of edge sizes, the place is in the table");
	Check(ArcTan_TableIndex(5, 5) == 0 && ArcTan_TableIndex(6, 5) == 0 && ArcTan_TableIndex(-1, 5) == 0, "sides of one size, the wrong way round, or a negative size give 0");

	printf("ArcTan uses it\n");
	std::string const maths = ReadSourceFile("3dc/maths.cpp");
	Check(Has(maths, "abs_height_x = ArcTan_Size(height_x);") && Has(maths, "abs_width_z = ArcTan_Size(width_z);"), "ArcTan takes its sides' sizes from here");
	Check(Has(maths, "angle = ArcTan_TableIndex(abs_width_z, abs_height_x);"), "and its place in the table");
	Check(Has(maths, "long long abs_height_x, abs_width_z, temp;"), "and keeps the sizes in 64 bits while it swaps them");

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all arctan index checks passed\n");
	return 0;
}
