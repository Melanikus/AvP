// The test of the log line that says how far the aliens' ghosts jump when their news comes (3dc/alien_ghost_jumps.h). Written
// 2026-11-08 by Melanikus for the AvP port (#120).
//
// WHAT IT TESTS
//
// - The header on its own: which surface a down is on, at the 45-degree edges; how far a jump is, including the farthest two places a
//   message can carry; when two downs are more than 45 degrees apart; the counting; when a line is due, also as the clock wraps round;
//   and the line's words, exactly.
// - By reading the game's source (pldnet.cpp): the jump is counted for a ghost that already exists, before the news moves it, and never
//   for a far one; the news's down is taken from the same part of its turn as the game's own code for a crawling ghost; the line is
//   written when it's due and again as a network game ends.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/alien_ghost_jumps_test.args, and the program gives back a non-zero result if any check
// failed. tests/takeouts/alien_ghost_jumps_test.json proves each check can fail.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "alien_ghost_jumps.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok" : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A source file, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const char *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();

	std::string out;

	for (char c : text.str())
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// Does first come before second in text, with both there?
static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(second, a);
	return a != std::string::npos && b != std::string::npos;
}

// The part of text from start up to end, or nothing if either isn't there
static std::string Between(const std::string &text, const char *start, const char *end)
{
	size_t const a = text.find(start);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(end, a);
	return (a == std::string::npos || b == std::string::npos) ? std::string() : text.substr(a, b - a);
}

static void TestTheRules()
{
	printf("Surfaces, jumps and downs:\n");

	Check(AlienGhost_SurfaceOf(65536) == kAlienGhostOnFloor, "a down pointing straight down is a floor's");
	Check(AlienGhost_SurfaceOf(46342) == kAlienGhostOnFloor, "and so is one just inside 45 degrees of it");
	Check(AlienGhost_SurfaceOf(46341) == kAlienGhostOnWall, "but one at 45 degrees is a wall's");
	Check(AlienGhost_SurfaceOf(0) == kAlienGhostOnWall, "as is one pointing sideways");
	Check(AlienGhost_SurfaceOf(-46341) == kAlienGhostOnWall, "and one at 45 degrees from straight up");
	Check(AlienGhost_SurfaceOf(-46342) == kAlienGhostOnCeiling, "while one just inside that is a ceiling's");
	Check(AlienGhost_SurfaceOf(-65536) == kAlienGhostOnCeiling, "as is one pointing straight up");

	Check(AlienGhost_JumpMm(3, 4, 0) == 5, "a jump of 3 and 4 mm on two axes is 5 mm");
	Check(AlienGhost_JumpMm(-3, -4, 0) == 5, "and the same backwards");
	Check(AlienGhost_JumpMm(0, 0, 0) == 0, "no jump is 0");
	Check(AlienGhost_JumpMm(8200000, 8200000, 8200000) == 14202817,
		"the farthest two places a message can carry, 8.2 km apart on each axis, are 14,202,817 mm apart, with nothing overflowing");

	int const down[3] = { 0, 65536, 0 };
	int const up[3] = { 0, -65536, 0 };
	int const tipped44[3] = { 0, 47143, 45525 };   // 44 degrees from straight down: cos and sin of 44 degrees, times 65536
	int const tipped46[3] = { 0, 45525, 47143 };   // 46 degrees
	Check(!AlienGhost_DownIsOff(down, down), "a down isn't off from itself");
	Check(!AlienGhost_DownIsOff(down, tipped44), "nor from one 44 degrees away");
	Check(AlienGhost_DownIsOff(down, tipped46), "but it is from one 46 degrees away");
	Check(AlienGhost_DownIsOff(down, up), "and from one pointing the other way");
}

static void TestTheCounting()
{
	printf("Counting, and the line:\n");

	AlienGhostJumps jumps;
	Check(AlienGhostJumps_Line(jumps, 5000).empty(), "with nothing counted there's no line");
	Check(!AlienGhostJumps_LineIsDue(jumps, 50000), "and none is due");

	AlienGhostJumps_Note(&jumps, kAlienGhostSurfaces, 100, true, true, 1000);
	Check(AlienGhostJumps_Line(jumps, 5000).empty(), "a surface past the end of the list isn't counted");

	AlienGhostJumps_Note(&jumps, kAlienGhostOnFloor, 30, false, false, 1000);
	AlienGhostJumps_Note(&jumps, kAlienGhostOnFloor, 41, false, false, 1500);
	AlienGhostJumps_Note(&jumps, kAlienGhostOnWall, 900, true, true, 2000);
	AlienGhostJumps_Note(&jumps, kAlienGhostOnCeiling, 300, true, false, 3000);
	AlienGhostJumps_Note(&jumps, kAlienGhostOnFloor, 20, false, true, 3500);

	Check(!AlienGhostJumps_LineIsDue(jumps, 10999), "a line isn't due until 10 s after the first news");
	Check(AlienGhostJumps_LineIsDue(jumps, 11000), "and is then");

	std::string const line = AlienGhostJumps_Line(jumps, 11600);
	std::string const expected = "net: alien ghosts in the last 11 s: on floors 3 news, a jump of 30 mm on average and 41 mm at most; on walls 1 news, 900 mm and 900 mm; "
		"on ceilings 1 news, 300 mm and 300 mm; of 2 news while crawling, 1 found the ghost's own down over 45 degrees off";
	Check(line == expected, "the line gives each surface's news, average and largest jump, and counts the downs that were off only among the crawling");

	if (line != expected)
	{
		printf("        it said: %s\n", line.c_str());
	}

	AlienGhostJumps walls;
	AlienGhostJumps_Note(&walls, kAlienGhostOnWall, 61, false, false, 0);
	AlienGhostJumps_Note(&walls, kAlienGhostOnWall, 62, false, false, 0);
	Check(AlienGhostJumps_Line(walls, 10000) == "net: alien ghosts in the last 10 s: on walls 2 news, a jump of 62 mm on average and 62 mm at most",
		"only the surfaces with news are named, the first in full words, and the average of 61 and 62 rounds to 62; with no crawling there's no last part");

	AlienGhostJumps late;
	AlienGhostJumps_Note(&late, kAlienGhostOnFloor, 1, false, false, 0xFFFFF000u);
	Check(!AlienGhostJumps_LineIsDue(late, 4294967000u), "a count started just before the clock wraps round isn't due 3.8 s later");
	Check(AlienGhostJumps_LineIsDue(late, 0x00001800u), "but is 10.2 s later, after the wrap");
}

static void TestTheGameUsesIt()
{
	printf("Where the game uses it, by reading:\n");

	std::string const net = ReadSource("3dc/avp/win95/pldnet.cpp");
	std::string const receive = Between(net, "static void ProcessNetMsg_AlienAIState(NETMESSAGE_ALIENAISTATE *messagePtr, NetID senderId)\n{", "\n}\n");
	std::string const note = Between(net, "static void NoteAlienGhostJump(", "\n}\n");
	std::string const write = Between(net, "static void WriteAlienGhostJumps(void)\n{", "\n}\n");
	std::string const endGame = Between(net, "void EndAVPNetGame(void)", "RemovePlayerFromGame(AvPNetID);");

	Check(ComesBefore(receive, "sbPtr = CreateNetGhost(", "NoteAlienGhostJump(sbPtr, &position, &orientation, !messagePtr->standard_gravity);") &&
		ComesBefore(receive, "NoteAlienGhostJump(sbPtr, &position, &orientation, !messagePtr->standard_gravity);", "UpdateAlienAIGhost(sbPtr, &position, &orientation,"),
		"the jump is counted for a ghost that's already there, before the news moves it, and says whether the alien is crawling");
	Check(Has(note, "if (!ghostData || !dynPtr || ghostData->onlyValidFar)"), "a far ghost's jump isn't counted");
	Check(Has(note, "int const newsDown[3] = { turn.mat12, turn.mat22, turn.mat32 };") && Has(receive, "sbPtr->DynPtr->GravityDirection.vx = mat.mat12;"),
		"the news's down is taken from the same part of its turn as the game's own down for a crawling ghost");
	Check(Has(note, "AlienGhost_SurfaceOf(newsDown[1])"), "and its surface from how much of that down points down");
	Check(Has(note, "newsPlace->vx - dynPtr->Position.vx, newsPlace->vy - dynPtr->Position.vy, newsPlace->vz - dynPtr->Position.vz"),
		"the jump is from where the ghost is now to where the news puts it");
	Check(ComesBefore(note, "if (AlienGhostJumps_LineIsDue(alienGhostJumps, now))", "AlienGhostJumps_Note(&alienGhostJumps,"),
		"a line that's due is written before the next news is counted, so the news starts the next count");
	Check(Has(write, "LogString(line);") && Has(write, "alienGhostJumps = AlienGhostJumps();"), "writing the line starts the count again");
	Check(Has(endGame, "WriteAlienGhostJumps();"), "and the end of a network game writes what's left");
}

int main()
{
	TestTheRules();
	TestTheCounting();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
