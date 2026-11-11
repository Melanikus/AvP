// A test of the number in a new name for something made while the game runs (sb_name_numbers.h, #139). Written 2026-11-08 by Melanikus
// for the AvP port.
//
// WHAT IT TESTS
//
// The first part runs SBNameNumber_Next itself: it counts up, goes back to 1 after the most a name holds, starts again from 1 after a
// number that's out of range, passes over numbers something alive still has, and always ends. The second part reads the game's own
// AssignNewSBName and the check it hands that function (strategy_def.cpp), because those need the whole game to run. /FC in this test's
// settings makes __FILE__ the whole path of this file, which is how the source is found.
#include "sb_name_numbers.h"

#include <fstream>
#include <set>
#include <sstream>
#include <stdio.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static void TheCount()
{
	puts("The count:");
	auto const nothingInUse = [](unsigned int) { return false; };

	Check(SBNameNumber_Next(0, nothingInUse, 1000) == 1, "the first name's number is 1");
	Check(SBNameNumber_Next(5, nothingInUse, 1000) == 6, "after 5 comes 6");
	Check(SBNameNumber_Next(kSBNameMostNumber - 1, nothingInUse, 1000) == kSBNameMostNumber, "the most a name holds is used");
	Check(SBNameNumber_Next(kSBNameMostNumber, nothingInUse, 1000) == 1, "after the most, it goes back to 1, not to 0");
	Check(SBNameNumber_Next(0xffffffffu, nothingInUse, 1000) == 1 && SBNameNumber_Next(kSBNameMostNumber + 7, nothingInUse, 1000) == 1,
		"a number out of range, as a saved game's file could say, starts again from 1");
}

static void PassingOver()
{
	puts("Passing over numbers something alive has:");
	std::set<unsigned int> alive = { 1, 2, 4 };
	auto const inUse = [&alive](unsigned int number) { return alive.count(number) != 0; };

	Check(SBNameNumber_Next(kSBNameMostNumber, inUse, 1000) == 3, "after going round, 1 and 2 are still alive, so the next is 3");
	Check(SBNameNumber_Next(3, inUse, 1000) == 5, "and after 3, 4 is passed over too");
	Check(SBNameNumber_Next(9, inUse, 1000) == 10, "a number nothing alive has is used as it comes");

	// Every number tried is in use, which can't happen with no more than mostAlive things alive: it still ends, after mostAlive + 1 tries
	unsigned int tried = 0;
	auto const allInUse = [&tried](unsigned int) { tried++; return true; };
	unsigned int const given = SBNameNumber_Next(100, allInUse, 5);
	Check(tried == 6 && given == 106, "with everything in use it still ends, after one more try than there can be things alive");
}

static std::string ReadSourceFile(const char *name)
{
	std::string const path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return std::string();
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	std::string plain;

	for (char const letter : all.str())
	{
		if (letter != '\r')
		{
			plain += letter;
		}
	}

	return plain;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Has(const std::string &text, const char *part)
{
	return text.find(part) != std::string::npos;
}

// Whether first is there, and before second
static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);
	return a != std::string::npos && b != std::string::npos && a < b;
}

static void TheGamesSide()
{
	puts("The game's side, read from its files:");
	std::string const blocks = ReadSourceFile("3dc/avp/strategy_def.cpp");
	Check(!blocks.empty(), "SOURCE: strategy_def.cpp can be read");

	std::string const naming = Between(blocks, "void AssignNewSBName(STRATEGYBLOCK *sbPtr)\n{", "\n}\n");
	Check(Has(naming, "IncrementalSBname = SBNameNumber_Next(IncrementalSBname, [sbPtr, tag](unsigned int number) { return NameNumberIsInUse(sbPtr, number, tag); }, maxstblocks);"),
		"SOURCE: a new name's number comes from SBNameNumber_Next, passing over numbers in use, with no more tries than things can be alive");
	Check(!Has(naming, "IncrementalSBname++"), "SOURCE: and nowhere else");
	Check(ComesBefore(naming, "sbPtr->SBname[SB_NAME_LENGTH-1]=1;", "char const tag = sbPtr->SBname[SB_NAME_LENGTH-1];")
		&& ComesBefore(naming, "char const tag = sbPtr->SBname[SB_NAME_LENGTH-1];", "IncrementalSBname = SBNameNumber_Next("),
		"SOURCE: the check looks for the tag the name is given");
	Check(Has(naming, "\tsbPtr->SBname[4] = (char)(IncrementalSBname & 0xff);\n\tsbPtr->SBname[5] = (char)((IncrementalSBname >> 8) & 0xff);\n\tsbPtr->SBname[6] = (char)((IncrementalSBname >> 16) & 0xff);")
		&& !Has(naming, "*((int *)&sbPtr->SBname[4])"), "SOURCE: and the number goes into the three bytes before it, leaving the tag in place");

	std::string const inUse = Between(blocks, "static bool NameNumberIsInUse(STRATEGYBLOCK const *self, unsigned int number, char tag)\n{", "\n}\n");
	Check(Has(inUse, "for (int i = 0; i < NumActiveStBlocks; i++)") && Has(inUse, "ActiveStBlockList[i]"), "SOURCE: the check looks at everything alive");
	Check(Has(inUse, "other != self"), "SOURCE: but not at the thing being named, which is already on the list");
	Check(Has(inUse, "memcmp(&other->SBname[4], id, sizeof(id)) == 0"), "SOURCE: and compares the four bytes the network uses as an id, the tag among them");
}

int main()
{
	TheCount();
	PassingOver();
	TheGamesSide();

	if (checksFailed)
	{
		printf("The name numbers test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	printf("The name numbers test passed\n");
	return 0;
}
