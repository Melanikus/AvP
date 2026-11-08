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
#include <stdio.h>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
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

int main()
{
	puts("1. the lines");
	TestTheLines();
	puts("2. the view");
	TestTheView();
	puts("3. the limits");
	TestTheLimits();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
