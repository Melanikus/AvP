// The test for 3dc/shadow_gap.h, which says how far behind a polygon its shadow starts. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// That a shadow's start stays far enough behind a polygon's flat surface at every corner, including the far corner of a long wall with a lamp beside it, where the old
// rule left it under a unit away. And that the old rule really does fail on that wall, so that this test would notice if the sum went back.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game files and none of the game.

#include <math.h>
#include <stdio.h>

#include "shadow_gap.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static unsigned int randomState = 1999;

static float RandomBetween(float lowest, float highest)
{
	randomState = randomState * 1103515245u + 12345u;

	float const fraction = (float)((randomState >> 8) & 0xFFFFFF) / 16777216.0f;

	return lowest + (highest - lowest) * fraction;
}

// What a gap along a corner's line comes to, as a distance straight behind the flat surface.
static float DistanceBehind(float gap, float length, float planeDistance)
{
	return gap * planeDistance / length;
}

static void CheckTheSum()
{
	Check(ShadowGap_ForCorner(8.0f, 1000.0f, 1000.0f, 20000.0f) == 8.0f, "a corner square on to the light goes just the gap");
	Check(ShadowGap_ForCorner(8.0f, 1000.0f, 500.0f, 20000.0f) == 16.0f, "at a slant it takes further to get the same distance behind");
	Check(ShadowGap_ForCorner(2.0f, 1000.0f, 1000.0f, 20000.0f) == 2.0f, "a small gap stays small");
	Check(ShadowGap_ForCorner(8.0f, 1000.0f, 999.0f, 20000.0f) == 8.25f, "it goes up to the next quarter of a unit, and never down");
	Check(ShadowGap_ForCorner(8.0f, 100000.0f, 1.0f, 20000.0f) == 20000.0f, "a corner almost exactly edge on is held to the limit");
	Check(ShadowGap_ForCorner(8.0f, 100000.0f, 1e-30f, 20000.0f) == 20000.0f, "even when the sum comes to more than a number can hold");

	// flat surfaces of two polygons lying flat together differ in the last digits
	Check(ShadowGap_ForCorner(8.0f, 1234.5f, 777.0001f, 20000.0f) == ShadowGap_ForCorner(8.0f, 1234.5f, 777.0002f, 20000.0f), "two surfaces that differ in the last digits give exactly the same answer");
}

// A lamp 80 from a wall, near one end of it. The scene's cut to whole numbers can move a surface by up to 1.73.
static void CheckALampBesideALongWall()
{
	float const cut = 1.73f;
	float const startGap = 8.0f;
	float const planeDistance = 80.0f;
	float const nearCorner = sqrtf(planeDistance * planeDistance + 100.0f * 100.0f);
	float const farCorner = sqrtf(planeDistance * planeDistance + 12000.0f * 12000.0f);

	// how it was: one gap for the whole polygon, from its first corner, at most 200
	float const oldFromNear = startGap * nearCorner / planeDistance;
	float const oldFromFar = (startGap * farCorner / planeDistance < 200.0f) ? startGap * farCorner / planeDistance : 200.0f;

	Check(DistanceBehind(oldFromNear, farCorner, planeDistance) < 1.0f, "the old rule, first corner near the lamp: under a unit behind the far corner");
	Check(DistanceBehind(oldFromFar, farCorner, planeDistance) < cut, "the old rule, first corner far from the lamp: the limit of 200 leaves it inside the cut");

	float const gapAtFar = ShadowGap_ForCorner(startGap, farCorner, planeDistance, kShadowGapWidest);
	float const gapAtNear = ShadowGap_ForCorner(startGap, nearCorner, planeDistance, kShadowGapWidest);

	Check(DistanceBehind(gapAtFar, farCorner, planeDistance) >= startGap, "now: the whole gap behind the far corner");
	Check(DistanceBehind(gapAtNear, nearCorner, planeDistance) >= startGap, "and behind the near corner");
	Check(gapAtNear < 20.0f, "without the near corner's shadow starting any further back than it has to");
}

// A corner's start is always at least the gap behind the surface, and at most a quarter of a unit's worth more, wherever the light is, unless the limit holds it back.
static void CheckEverywhere()
{
	int tooClose = 0, tooFar = 0, atTheLimit = 0;

	for (int i = 0; i < 200000; i++)
	{
		float const length = RandomBetween(1.0f, 100000.0f);
		float const planeDistance = RandomBetween(0.001f, length);
		float const startGap = (i & 1) ? 8.0f : 2.0f;
		float const gap = ShadowGap_ForCorner(startGap, length, planeDistance, kShadowGapWidest);

		if (gap == kShadowGapWidest)
		{
			atTheLimit++;
			continue;
		}

		float const behind = DistanceBehind(gap, length, planeDistance);

		// a little room for a float's own rounding in this check's sums
		tooClose += (behind < startGap * 0.9999f) ? 1 : 0;
		tooFar += (gap > startGap * length / planeDistance + 0.26f) ? 1 : 0;
	}

	Check(tooClose == 0, "over 200,000 corners, a start is never less than the gap behind the surface");
	Check(tooFar == 0, "and never more than a quarter of a unit further along than it needs");
	Check(atTheLimit > 0 && atTheLimit < 2000, "and the limit only comes into it at the very lowest angles");
}

int main()
{
	CheckTheSum();
	CheckALampBesideALongWall();
	CheckEverywhere();

	printf("%d failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
