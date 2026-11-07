// The test for 3dc/win95/circle_tables.h and 3dc/win95/frame_time.h, and for two places in the game that use them.
//
// WHAT IT TESTS
//
// The sine and cosine tables have one entry for each of the 4096 steps round a circle. A step number outside 0 to 4095
// must wrap round the circle and read the right entry, not memory past the end of a table. It tries the real tables,
// with the numbers the game can really ask for. It also checks a frame never counts as nothing, since the game divides
// by that. The last part reads the game's own files, because the code that uses these can't be built on its own here.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs /FC, so that
// its source files are found.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "tables.h"
#include "circle_tables.h"
#include "frame_time.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, found from where this test's own source is.
static std::string ReadSourceFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream text;

	text << file.rdbuf();
	return text.str();
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static void TestTheStepsOnTheCircle()
{
	// These read the table as they always have
	Check(GetSin(0) == 0, "the sine of no turn is 0");
	Check(GetCos(0) == 65536, "the cosine of no turn is a whole one");
	Check(GetSin(1024) == 65535, "the sine of a quarter turn is the largest in the table, one short of a whole one");
	Check(GetSin(4095) == sine[4095], "the last step reads the last entry of the sine table");
	Check(GetCos(4095) == cosine[4095], "the last step reads the last entry of the cosine table");
	Check(GetSin(2048) == sine[2048], "a step in the middle reads its own entry");
}

static void TestTheStepsPastTheEnd()
{
	// The number NPCOrientateToVector asks for when an NPC's turn this frame rounds down to nothing: 4096 take away 0
	int const turnThatRoundedToNothing = 4096 - 0;

	Check(GetSin(turnThatRoundedToNothing) == 0, "a whole turn reads no turn in the sine table, for an NPC's turn that rounded to nothing");
	Check(GetCos(turnThatRoundedToNothing) == 65536, "a whole turn reads no turn in the cosine table, for an NPC's turn that rounded to nothing");

	// The number pmove.cpp comes to: a turn wrapped to 4095 and then one more for a turn right
	int const playersTurn = (-1 & 4095) + 1;

	Check(GetSin(playersTurn) == 0, "a player's turn of step 4095 and one more reads no turn in the sine table");
	Check(GetCos(playersTurn) == 65536, "a player's turn of step 4095 and one more reads no turn in the cosine table");

	Check(GetSin(4097) == sine[1], "one step past a whole turn reads step 1");
	Check(GetCos(8192) == cosine[0], "two whole turns read no turn");
	Check(GetSin(-1) == sine[4095], "one step back reads the last entry");
	Check(GetCos(-4096) == cosine[0], "a whole turn back reads no turn");
}

static void TestTheFrameTime()
{
	Check(FrameTime_NeverNothing(0) == 1, "a frame that came to nothing counts as the shortest there is");
	Check(FrameTime_NeverNothing(-5) == 1, "a frame below nothing counts as the shortest there is");
	Check(FrameTime_NeverNothing(1) == 1, "the shortest frame stays as it is");
	Check(FrameTime_NeverNothing(65) == 65, "a frame of a millisecond stays as it is");
	Check(FrameTime_NeverNothing(16384) == 16384, "the longest frame stays as it is");
}

static void TestTheRealGame()
{
	std::string const platform = ReadSourceFile("3dc/win95/platform.h");
	std::string const frames = ReadSourceFile("3dc/win95/io.cpp");
	std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");

	Check(!platform.empty() && !frames.empty() && !marine.empty(), "SOURCE: the game's files could be read");
	Check(Has(platform, "#include \"circle_tables.h\""), "SOURCE: the game's reads of the tables come from the header tested here");
	Check(!Has(platform, "sine[a]") && !Has(platform, "cosine[a]"), "SOURCE: the game has no other way of reading the tables with no wrap");
	Check(Has(frames, "NormalFrameTime = FrameTime_NeverNothing(NormalFrameTime);"), "SOURCE: a frame's length goes through the rule tested here");
	Check(Has(marine, "mod = marineStatusPointer->Target ? SpeedRangeMods(&relPos,&relPos2) : 0;"), "SOURCE: a marine firing with no target doesn't work out its aim from numbers nothing set");
	Check(Has(marine, "if (magrange==0) return(0);"), "SOURCE: a target standing exactly on the marine doesn't divide by nothing");
}

int main()
{
	printf("What the sine and cosine tables give back, and how short a frame can be:\n");

	TestTheStepsOnTheCircle();
	TestTheStepsPastTheEnd();
	TestTheFrameTime();
	TestTheRealGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
