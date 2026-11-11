// The test for 3dc/avp/alien_ways_view.h: the ALIEN WAYS developer view's lines, an alien's way drawn as coloured lines with a cross at
// each point (issue #109). Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game. Coordinates are
// the game's: millimetres, and y goes down. The way here goes along the floor, up a wall and across a ceiling:
//
//         p2 (1000, -2000, 0), ceiling
//          |
//          |  the wall
//          |
//   p0 ----- p1 (1000, 0, 0), wall
//   (0, 0, 0), floor

#include "alien_ways_view.h"

#include <climits>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

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

// How many times a piece of text is in the text (added 2026-11-11 by Melanikus (Third), as alien_ways_build_test has it)
static size_t CountOf(std::string const &text, char const *wanted)
{
	size_t count = 0;

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + 1))
	{
		count++;
	}

	return count;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static const float kSame[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };   // a camera at the level's 0, looking down the level's z

static const AlienWaysPoint kWay[3] =
{
	{ 0, 0, 0, kAlienWaysFloor },
	{ 1000, 0, 0, kAlienWaysWall },
	{ 1000, -2000, 0, kAlienWaysCeiling }
};

static bool At(const AlienWaysViewEnd &end, float x, float y, float z)
{
	return end.x == x && end.y == y && end.z == z;
}

// How many of the ends are this colour
static int CountColour(const std::vector<AlienWaysViewEnd> &ends, uint32_t colour)
{
	int count = 0;

	for (const AlienWaysViewEnd &end : ends)
	{
		count += end.colour == colour ? 1 : 0;
	}

	return count;
}

static void TestTheLines()
{
	std::vector<AlienWaysViewEnd> ends;
	size_t const added = AlienWaysView_AddWay(kSame, kWay, 3, 2, &ends);

	// Two stretches and three crosses of three lines: 11 lines, 22 ends
	Check(added == 22 && ends.size() == 22, "a way of three points is two stretches and three crosses: 22 line ends");

	// In order: the first stretch, the first point's cross, the second stretch, the second point's cross, the last point's cross
	Check(At(ends[0], 0, 0, 0) && At(ends[1], 1000, 0, 0) && ends[0].colour == kAlienWaysViewFloor, "the first stretch runs along the floor, in the floor's gold");
	Check(At(ends[2], -150, 0, 0) && At(ends[3], 150, 0, 0), "a point's cross reaches 15 cm each way");
	Check(ends[2].colour == kAlienWaysViewFloor && ends[6].colour == kAlienWaysViewFloor, "in its point's colour");
	Check(At(ends[8], 1000, 0, 0) && At(ends[9], 1000, 2000, 0), "the second stretch goes up the wall, with up turned over for the card, as the game's polygons are");
	Check(ends[8].colour == kAlienWaysViewHere && ends[9].colour == kAlienWaysViewHere, "and it's the stretch the alien is on, into the point it's heading for, so it's white");
	Check(ends[10].colour == kAlienWaysViewWall, "the wall point's cross is the wall's blue");
	Check(At(ends[16], 700, 2000, 0) && At(ends[17], 1300, 2000, 0) && ends[16].colour == kAlienWaysViewHere, "the point it's heading for has a white cross twice the size");
	Check(CountColour(ends, kAlienWaysViewHere) == 2 + 6, "and nothing else is white");

	// Heading for the very first point: it hasn't started along the way yet, so no stretch is white, only that point
	ends.clear();
	AlienWaysView_AddWay(kSame, kWay, 3, 0, &ends);
	Check(ends[0].colour == kAlienWaysViewFloor && ends[8].colour == kAlienWaysViewWall, "heading for the first point, no stretch is white");
	Check(At(ends[2], -300, 0, 0) && ends[2].colour == kAlienWaysViewHere, "and the first point's cross is the white one");

	// A heading outside the way marks nothing white
	for (int heading : { -1, 3, INT_MAX, INT_MIN })
	{
		ends.clear();
		AlienWaysView_AddWay(kSame, kWay, 3, heading, &ends);
		Check(ends.size() == 22 && CountColour(ends, kAlienWaysViewHere) == 0, "heading " + std::to_string(heading) + ", outside the way: the way is drawn, and nothing is white");
	}

	// Every stretch's colour
	Check(AlienWaysView_ColourOf(kAlienWaysFloor) == 0xffd9b44au && AlienWaysView_ColourOf(kAlienWaysWall) == 0xff78a2d0u && AlienWaysView_ColourOf(kAlienWaysCeiling) == 0xffb48ad0u, "floor gold, wall blue, ceiling purple: the offline page's colours");
	Check(AlienWaysView_ColourOf(static_cast<AlienWaysKind>(7)) == kAlienWaysViewUnknown, "and grey for a kind that isn't one of the three");

	// A kept way (added 2026-11-11 by Melanikus (Third) for the AvP port, #109): the last way of an alien that isn't following it now. The
	// same lines as a way with no point it's heading for, each colour at two fifths, so it stays on screen but plainly isn't followed.
	std::vector<AlienWaysViewEnd> plain;
	AlienWaysView_AddWay(kSame, kWay, 3, -1, &plain);
	ends.clear();
	size_t const kept = AlienWaysView_AddKeptWay(kSame, kWay, 3, &ends);
	bool sameLines = kept == plain.size() && ends.size() == plain.size();
	bool allDimmed = sameLines;

	for (size_t e = 0; sameLines && e < ends.size(); e++)
	{
		sameLines = ends[e].x == plain[e].x && ends[e].y == plain[e].y && ends[e].z == plain[e].z;
		allDimmed = allDimmed && ends[e].colour == AlienWaysView_Dimmed(plain[e].colour);
	}

	Check(kept == 22 && sameLines, "a kept way is the same 22 line ends as the way, in the same places");
	Check(allDimmed && CountColour(ends, kAlienWaysViewHere) == 0, "every one dimmed, and nothing white, as no point is being headed for");
	Check(AlienWaysView_Dimmed(0xffd9b44au) == 0xff56481du && AlienWaysView_Dimmed(0x80ffffffu) == 0x80666666u,
		"dimmed is red, green and blue at two fifths, the alpha kept");
	Check(AlienWaysView_AddKeptWay(kSame, kWay, 0, &ends) == 0 && AlienWaysView_AddKeptWay(kSame, kWay, 3, 0) == 0,
		"a kept way of no points, or with nowhere to put its lines, adds nothing");
}

static void TestTheView()
{
	// A camera moved: the game's matrix puts the level's 0 at (10, 20, 30) in its view, and the card's y is that turned over
	float const moved[12] = { 1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30 };
	std::vector<AlienWaysViewEnd> ends;
	AlienWaysView_AddWay(moved, kWay, 1, -1, &ends);
	Check(At(ends[0], -140, -20, 30) && At(ends[1], 160, -20, 30), "a point goes through the game's own view sums, then up is turned over");

	// A camera turned a quarter round, so the level's x is the view's z
	float const turned[12] = { 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0 };
	ends.clear();
	AlienWaysView_AddWay(turned, kWay, 2, -1, &ends);
	Check(At(ends[0], 0, 0, 0) && At(ends[1], 0, 0, 1000), "turned a quarter round, the first stretch runs away from the camera");
}

static void TestTheLimits()
{
	std::vector<AlienWaysViewEnd> ends;

	Check(AlienWaysView_AddWay(kSame, kWay, 0, 0, &ends) == 0 && ends.empty(), "a way of no points draws nothing");
	Check(AlienWaysView_AddWay(kSame, kWay, -5, 0, &ends) == 0, "nor does a count below nothing");
	Check(AlienWaysView_AddWay(kSame, 0, 3, 0, &ends) == 0, "nor no points at all");
	Check(AlienWaysView_AddWay(0, kWay, 3, 0, &ends) == 0, "nor no view");
	Check(AlienWaysView_AddWay(kSame, kWay, 3, 0, 0) == 0, "nor nowhere to put the lines");
	Check(AlienWaysView_AddWay(kSame, kWay, 1, 0, &ends) == 6, "one point is only its cross");

	// A way longer than any the map gives: only the first kAlienWaysMostWayPoints points are drawn
	std::vector<AlienWaysPoint> longWay(kAlienWaysMostWayPoints + 50);

	for (size_t i = 0; i < longWay.size(); i++)
	{
		longWay[i].x = (int)i * 100;
		longWay[i].y = 0;
		longWay[i].z = 0;
		longWay[i].kind = kAlienWaysFloor;
	}

	ends.clear();
	size_t const added = AlienWaysView_AddWay(kSame, longWay.data(), (int)longWay.size(), -1, &ends);
	Check(added == ((kAlienWaysMostWayPoints - 1) + kAlienWaysMostWayPoints * 3) * 2, "a way of 306 points draws only its first 256: 255 stretches and 256 crosses");

	// A frame's list stops at its ceiling, and only whole lines go in
	ends.assign(kAlienWaysViewMostEnds - 5, AlienWaysViewEnd());
	size_t const atTheTop = AlienWaysView_AddWay(kSame, kWay, 3, 2, &ends);
	Check(atTheTop == 4 && ends.size() == kAlienWaysViewMostEnds - 1, "five ends from the ceiling, two whole lines go in, and nothing past it");
	Check(AlienWaysView_AddWay(kSame, kWay, 3, 2, &ends) == 0, "and with no room for a whole line, nothing more is added");
}

// The game's own files, read as text: the view is drawn where and when it should be, and never in a network game
static void TestJoiningTheGame()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const ways = ReadSourceFile("3dc/avp/alien_ways.h");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");

	Check(!render.empty() && !ways.empty() && !winmain.empty(), "the game's files are there to read");

	// The view has its own developer switch, -alienwaysview, which needs -alienways, and nothing else turns it on, so no player ever sees the
	// lines once the ways are on for everyone (modified 2026-11-11 by Melanikus (Third) for the AvP port, #109, PO's decision R-LAVA-P2)
	Check(Has(winmain, "if (devSwitch(\"-alienwaysview\"))\n\t\t{\n\t\t\tif (AlienWaysSwitchedOn)\n\t\t\t{\n\t\t\t\tAlienWaysViewSwitchedOn = 1;"),
		"the view's own switch is a developer's, and it needs -alienways");
	Check(CountOf(winmain, "AlienWaysViewSwitchedOn = 1;") == 1 && !Has(render, "AlienWaysViewSwitchedOn = 1;"), "and nothing else turns the view on");
	Check(Has(ReadSourceFile("3dc/avp/alien_ways.cpp"), "\nint AlienWaysViewSwitchedOn = 0;\n"), "and it starts off");

	std::string const draw = Between(render, "static void DrawAlienWays()\n{", "\n}\n");
	Check(Has(draw, "if (!AlienWaysViewSwitchedOn || AvP.Network != I_No_Network)\n\t{\n\t\treturn;"), "the view is only drawn with its own developer switch, and never in a network game");
	Check(Has(draw, "size_t const count = AlienWays_WaysToShow(shown, kMostShown);"), "it draws the ways the alien ways code gives it, on the game's thread");
	Check(Has(draw, "AlienWaysView_AddWay(ViewMatrix, shown[i].points, shown[i].count, shown[i].heading, &ends);"), "through this frame's view, with the point each alien is heading for");
	Check(Has(draw, "device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);") && Has(draw, "device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);"), "tested against the level's depth, so a wall hides a way behind it, and leaving the depth as it was");
	Check(Has(draw, "asItWas->Apply();") && Has(draw, "D3DPT_LINELIST"), "drawn as lines, with the card put back as it was");

	Check(Has(render, "if (toyFinalFlush && AlienWaysViewSwitchedOn)\n\t{\n\t\tDrawAlienWays();"), "in the frame's last drawing, when the whole level is in the depth buffer, with its own switch");

	std::string const network = Between(render, "static void KeepDeveloperToolsOutOfNetworkGames()\n{", "\n}\n");
	Check(Has(network, "LogString(\"alien ways view: this is a network game, so the aliens' ways aren't drawn\");"), "a network game says once in the log that the ways aren't drawn");

	Check(Has(ways, "struct AlienWaysShown\n{\n\tconst AlienWaysPoint *points;\n\tint count;\n\tint heading;\n};") && Has(ways, "size_t AlienWays_WaysToShow(AlienWaysShown *out, size_t most);"), "the alien ways code gives the ways in the shape the view reads");
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj"), "3dc\\avp\\alien_ways_view.cpp"), "and the game builds the view");
}

int main()
{
	puts("1. the lines");
	TestTheLines();
	puts("2. the view");
	TestTheView();
	puts("3. the limits");
	TestTheLimits();
	puts("4. joining the game");
	TestJoiningTheGame();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
