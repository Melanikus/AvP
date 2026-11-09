// The test of the shapes Hudson on foot's ribbon is made of. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 3dc/avp/hudson_play_ribbon.h: where a band and a block put their corners, that a hop leaves a gap, and that nothing can go past the limit on corners.
//
// WHAT IT DOESN'T TEST
//
// Whether they look right or are in the right place in a level. Only a run shows that.

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "hudson_play_ribbon.h"

static int checksFailed = 0;

static void Check(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

struct Point
{
	int x, y, z;
};

static HudsonPlayRibbonColour const kColour = { 200, 100, 50 };

static void TestABand()
{
	printf("A band\n");

	std::vector<HudsonPlayRibbonCorner> corners;
	std::vector<Point> const way = { { 0, 1000, 0 }, { 1000, 800, 0 } };

	HudsonPlayRibbon_AddBand(way, 0, 60, kColour, corners);
	Check(corners.size() == 6, "two points make one piece, which is two triangles");

	int lowestX = 1000000, highestX = -1000000, lowestZ = 1000000, highestZ = -1000000;

	for (HudsonPlayRibbonCorner const &corner : corners)
	{
		lowestX = corner.x < lowestX ? corner.x : lowestX;
		highestX = corner.x > highestX ? corner.x : highestX;
		lowestZ = corner.z < lowestZ ? corner.z : lowestZ;
		highestZ = corner.z > highestZ ? corner.z : highestZ;
	}

	Check(lowestZ == -kHudsonPlayRibbonHalfWidth && highestZ == kHudsonPlayRibbonHalfWidth, "it's as wide as it should be, to each side of the way");
	Check(lowestX == -kHudsonPlayRibbonHalfWidth && highestX == 1000 + kHudsonPlayRibbonHalfWidth, "and it runs a little past each end");

	bool const startsLow = corners[0].y == 1000 - 60 && corners[5].y == 1000 - 60;
	bool const endsHigh = corners[1].y == 800 - 60 && corners[2].y == 800 - 60;

	Check(startsLow && endsHigh, "it lies just above the floor at each end, so it goes up a stair with him");

	HudsonPlayRibbonColour const white = { 255, 255, 255 };
	std::vector<HudsonPlayRibbonCorner> striped;
	std::vector<Point> const three = { { 0, 0, 0 }, { 1000, 0, 0 }, { 2000, 0, 0 } };

	HudsonPlayRibbon_AddBand(three, 0, 0, white, striped);
	Check(striped.size() == 12 && striped[0].red != striped[6].red, "the pieces take turns being darker, which is what shows it going up a stair");

	std::vector<HudsonPlayRibbonCorner> fromTheMiddle;

	HudsonPlayRibbon_AddBand(three, 1, 0, white, fromTheMiddle);
	Check(fromTheMiddle.size() == 6 && fromTheMiddle[0].x < 2000 && fromTheMiddle[0].x >= 1000 - kHudsonPlayRibbonHalfWidth, "it only starts from the point it's asked to");

	std::vector<HudsonPlayRibbonCorner> nearEnd;

	HudsonPlayRibbon_AddBand(three, 0, 0, white, nearEnd);
	Check(nearEnd[0].x >= 1000 - kHudsonPlayRibbonHalfWidth, "the far end goes in first, because that's where he is now, on a trail");
}

static void TestGaps()
{
	printf("Where there's nothing to draw\n");

	std::vector<HudsonPlayRibbonCorner> corners;

	std::vector<Point> const onePoint = { { 5, 5, 5 } };
	HudsonPlayRibbon_AddBand(onePoint, 0, 0, kColour, corners);
	Check(corners.empty(), "one point is no band");

	std::vector<Point> const noPoints;
	HudsonPlayRibbon_AddBand(noPoints, 0, 0, kColour, corners);
	Check(corners.empty(), "and neither is none");

	std::vector<Point> const startPastTheEnd = { { 0, 0, 0 }, { 500, 0, 0 } };
	HudsonPlayRibbon_AddBand(startPastTheEnd, 7, 0, kColour, corners);
	Check(corners.empty(), "starting past the end draws nothing, and reads nothing it shouldn't");

	std::vector<Point> const straightUp = { { 0, 0, 0 }, { 0, -3000, 0 } };
	HudsonPlayRibbon_AddBand(straightUp, 0, 0, kColour, corners);
	Check(corners.empty(), "one point straight above another has no way to run");

	std::vector<Point> const aHop = { { 0, 0, 0 }, { kHudsonPlayRibbonLongestPiece + 1, 0, 0 } };
	HudsonPlayRibbon_AddBand(aHop, 0, 0, kColour, corners);
	Check(corners.empty(), "two points far apart are a hop, and no band joins them");

	std::vector<Point> const justShort = { { 0, 0, 0 }, { kHudsonPlayRibbonLongestPiece, 0, 0 } };
	HudsonPlayRibbon_AddBand(justShort, 0, 0, kColour, corners);
	Check(corners.size() == 6, "and exactly that far is still a walk");
}

static void TestALimit()
{
	printf("The limit\n");

	std::vector<Point> way;

	for (int i = 0; i < 20000; i++)
	{
		Point const point = { i * 500, 0, 0 };

		way.push_back(point);
	}

	std::vector<HudsonPlayRibbonCorner> corners;

	HudsonPlayRibbon_AddBand(way, 0, 0, kColour, corners);
	Check(corners.size() <= kHudsonPlayRibbonMostCorners && corners.size() + 6 > kHudsonPlayRibbonMostCorners, "a band too long stops at the limit, as near to it as it can");
	Check(corners[0].x >= 19999 * 500 - 500 - kHudsonPlayRibbonHalfWidth, "and what's left out is its beginning");

	HudsonPlayRibbon_AddBlock(0, 0, 0, 100, 100, kColour, corners);
	Check(corners.size() <= kHudsonPlayRibbonMostCorners, "a block can't go past it either");
}

static void TestABlock()
{
	printf("A block\n");

	std::vector<HudsonPlayRibbonCorner> corners;

	HudsonPlayRibbon_AddBlock(1000, 500, 2000, 100, 300, kColour, corners);
	Check(corners.size() == 30, "a top and four sides are ten triangles");

	bool insideIt = true;
	bool sawTop = false, sawFeet = false;

	for (HudsonPlayRibbonCorner const &corner : corners)
	{
		insideIt = insideIt && abs(corner.x - 1000) == 100 && abs(corner.z - 2000) == 100 && (corner.y == 500 || corner.y == 200);
		sawTop = sawTop || corner.y == 200;
		sawFeet = sawFeet || corner.y == 500;
	}

	Check(insideIt && sawTop && sawFeet, "every corner is at its edge, at its feet or at its top, and its top is higher up than its feet");
	Check(corners[0].y == 200 && corners[3].y == 200 && corners[0].red == kColour.red, "its top has the whole colour");
	Check(corners[6].red == (uint8_t)(kColour.red * kHudsonPlayRibbonSideOneWay), "one pair of sides is darker");
	Check(corners[18].red == (uint8_t)(kColour.red * kHudsonPlayRibbonSideOtherWay), "and the other pair darker still, so its corners can be seen");
}

int main()
{
	TestABand();
	TestGaps();
	TestALimit();
	TestABlock();

	if (checksFailed)
	{
		printf("%d SOME CHECKS FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
