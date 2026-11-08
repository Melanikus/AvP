// The test of where an alien on its way aims (3dc/avp/alien_ways_lookahead.h, #109). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// AlienWays_AimAlong, on made-up ways (places in mm; y goes down, so a wall going up has smaller y as it goes):
// - on a straight stretch it aims 1.5 m on from the alien's nearest place on it, and with 0 ahead it aims at the next point, as before;
// - it never aims past the goal, past a step onto another kind of surface, or round bends adding up to more than 30 degrees, and a bend
//   just after two points in the same place still counts;
// - it does go round a gentle bend, and on up a wall once the step onto it is made;
// - an alien off either end of its stretch looks from that end, and it never looks more than 10 m ahead;
// - it refuses a way it can't use, and leaves the answer alone then;
// - it's right at the map's farthest corners, 64 km out.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/alien_ways_lookahead_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/alien_ways_lookahead_test.json proves each check can fail.

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <string>

#include "alien_ways_lookahead.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static AlienWaysPoint P(int x, int y, int z, AlienWaysKind kind)
{
	AlienWaysPoint const point = { x, y, z, kind };
	return point;
}

static AlienWaysXYZ At(int x, int y, int z)
{
	AlienWaysXYZ const xyz = { x, y, z };
	return xyz;
}

// The aim for an alien at `at` heading for points[heading], 1.5 m ahead unless said otherwise; a far-off answer if it refused
static AlienWaysXYZ Aim(const AlienWaysPoint *points, int count, int heading, AlienWaysXYZ at, int ahead = kAlienWaysAimAheadMm)
{
	AlienWaysXYZ aim = At(-1, -1, -1);

	if (!AlienWays_AimAlong(points, count, heading, at, ahead, &aim))
	{
		return At(INT_MIN, INT_MIN, INT_MIN);
	}

	return aim;
}

static bool Is(AlienWaysXYZ got, int x, int y, int z)
{
	bool const same = got.x == x && got.y == y && got.z == z;

	if (!same)
	{
		printf("        it aimed at (%d, %d, %d), not (%d, %d, %d)\n", got.x, got.y, got.z, x, y, z);
	}

	return same;
}

static const AlienWaysKind F = kAlienWaysFloor;
static const AlienWaysKind W = kAlienWaysWall;

static void TestStraight()
{
	puts("On a straight stretch:");

	AlienWaysPoint const way[] = { P(0, 0, 0, F), P(10000, 0, 0, F) };

	Check(Is(Aim(way, 2, 1, At(2000, -500, 300)), 3500, 0, 0), "it aims 1.5 m on from the alien's nearest place on its stretch");
	Check(Is(Aim(way, 2, 1, At(2000, -500, 300), 0), 10000, 0, 0) && Is(Aim(way, 2, 1, At(2000, -500, 300), -5), 10000, 0, 0),
		"with 0 ahead, or less, it aims at the point it's heading for, as the guide did before");
	Check(Is(Aim(way, 2, 1, At(-3000, 0, 0)), 1500, 0, 0), "an alien behind the stretch's start looks on from the start");

	AlienWaysPoint const longWay[] = { P(0, 0, 0, F), P(60000000, 0, 0, F) };
	Check(Is(Aim(longWay, 2, 1, At(0, 0, 0), INT_MAX), 10000, 0, 0), "and it never looks more than 10 m ahead, whatever it's asked");
}

static void TestTheGoal()
{
	puts("The goal:");

	AlienWaysPoint const way[] = { P(0, 0, 0, F), P(2000, 0, 0, F) };

	Check(Is(Aim(way, 2, 1, At(1000, 0, 0)), 2000, 0, 0), "it never aims past the way's last point");
	Check(Is(Aim(way, 2, 1, At(5000, 0, 0)), 2000, 0, 0), "and an alien past the end of its stretch looks from the end, which is the goal");
}

static void TestSteps()
{
	puts("Steps onto another kind of surface:");

	// The kinds change at point 1 with the way going straight on, so only the step can stop the aim there
	AlienWaysPoint const straightOn[] = { P(0, 0, 0, F), P(2000, 0, 0, W), P(6000, 0, 0, W) };
	Check(Is(Aim(straightOn, 3, 1, At(1000, 0, 0)), 2000, 0, 0), "it waits at a step it hasn't made, even with the way going straight on");

	AlienWaysPoint const later[] = { P(0, 0, 0, F), P(1000, 0, 0, F), P(1800, 0, 0, W), P(5000, 0, 0, W) };
	Check(Is(Aim(later, 4, 1, At(500, 0, 0)), 1800, 0, 0), "and at one after another point on the same surface");

	AlienWaysPoint const ontoAWall[] = { P(0, 0, 0, F), P(2000, 0, 0, W), P(2000, -6000, 0, W) };
	Check(Is(Aim(ontoAWall, 3, 1, At(1000, 0, 0)), 2000, 0, 0), "so an alien on the floor aims at the foot of the wall it's going up");
	Check(Is(Aim(ontoAWall, 3, 2, At(2000, -1000, 0)), 2000, -2500, 0), "and once the step is made, and it's on the wall, it looks on up the wall");
}

static void TestBends()
{
	puts("Bends:");

	double const degree = 3.14159265358979323846 / 180.0;

	// A gentle bend of 20 degrees, 0.5 m on from the alien: the aim goes round it, 1 m along the stretch after the bend
	AlienWaysPoint const gentle[] = { P(0, 0, 0, F), P(1000, 0, 0, F), P(1000 + static_cast<int>(lround(3000 * cos(20 * degree))), 0,
		static_cast<int>(lround(3000 * sin(20 * degree))), F) };
	double const bentX = gentle[2].x - 1000.0, bentZ = gentle[2].z, bentLength = sqrt(bentX * bentX + bentZ * bentZ);
	Check(Is(Aim(gentle, 3, 1, At(500, 0, 0)), static_cast<int>(lround(1000 + bentX * 1000 / bentLength)), 0, static_cast<int>(lround(bentZ * 1000 / bentLength))),
		"it goes round a gentle bend (20 degrees), and on along the way");

	// An outside corner between two walls, as on Ops01: a quarter turn
	AlienWaysPoint const corner[] = { P(0, -1500, 0, W), P(1000, -1500, 0, W), P(1000, -1500, 3000, W) };
	Check(Is(Aim(corner, 3, 1, At(500, -1500, 0)), 1000, -1500, 0), "it waits at a sharp corner (90 degrees), as at Ops01's corner between two walls");

	// Two bends of 20 degrees each: the first is let through, and together they're past 30
	AlienWaysPoint twoBends[4];
	twoBends[0] = P(0, 0, 0, F);
	twoBends[1] = P(500, 0, 0, F);
	twoBends[2] = P(500 + static_cast<int>(lround(500 * cos(20 * degree))), 0, static_cast<int>(lround(500 * sin(20 * degree))), F);
	twoBends[3] = P(twoBends[2].x + static_cast<int>(lround(3000 * cos(40 * degree))), 0, twoBends[2].z + static_cast<int>(lround(3000 * sin(40 * degree))), F);
	Check(Is(Aim(twoBends, 4, 1, At(0, 0, 0)), twoBends[2].x, 0, twoBends[2].z), "the bends are added up: two of 20 degrees stop it at the second");

	// One bend half-way along the 1.5 m: just under 30 degrees is let through, and 31 isn't
	AlienWaysPoint const justUnder[] = { P(0, 0, 0, F), P(750, 0, 0, F), P(750 + 866026, 0, 500000, F) };
	AlienWaysXYZ const under = Aim(justUnder, 3, 1, At(0, 0, 0));
	double const cut = fabs(750.0 * under.z) / sqrt(static_cast<double>(under.x) * under.x + static_cast<double>(under.z) * under.z);
	Check(under.x > 750 && under.z > 0 && cut > 185.0 && cut < 200.0,
		"a bend of just under 30 degrees half-way is let through, and the line to the aim passes 0.19 m inside the corner (" + std::to_string(cut) + " mm)");

	AlienWaysPoint const over[] = { P(0, 0, 0, F), P(750, 0, 0, F), P(750 + static_cast<int>(lround(3000 * cos(31 * degree))), 0,
		static_cast<int>(lround(3000 * sin(31 * degree))), F) };
	Check(Is(Aim(over, 3, 1, At(0, 0, 0)), 750, 0, 0), "and one of 31 degrees stops it at the bend");
}

static void TestNoLength()
{
	puts("Stretches of no length:");

	AlienWaysPoint const twice[] = { P(0, 0, 0, F), P(1000, 0, 0, F), P(1000, 0, 0, F), P(5000, 0, 0, F) };
	Check(Is(Aim(twice, 4, 1, At(500, 0, 0)), 2000, 0, 0), "two points in the same place are passed over, with no bend");

	AlienWaysPoint const thenACorner[] = { P(0, 0, 0, F), P(1000, 0, 0, F), P(1000, 0, 0, F), P(1000, 0, 3000, F) };
	Check(Is(Aim(thenACorner, 4, 1, At(500, 0, 0)), 1000, 0, 0), "and a sharp corner just after them is still waited at");

	AlienWaysPoint const startsOnOne[] = { P(1000, 0, 0, F), P(1000, 0, 0, F), P(5000, 0, 0, F) };
	Check(Is(Aim(startsOnOne, 3, 1, At(0, 0, 0)), 2500, 0, 0), "an alien whose own stretch has no length looks on from that place");
}

static void TestRefused()
{
	puts("Ways it can't use:");

	AlienWaysPoint const way[] = { P(0, 0, 0, F), P(2000, 0, 0, F) };
	AlienWaysXYZ aim = At(7, 8, 9);
	bool const noPoints = AlienWays_AimAlong(0, 2, 1, At(0, 0, 0), 1500, &aim);
	bool const onePoint = AlienWays_AimAlong(way, 1, 1, At(0, 0, 0), 1500, &aim);
	bool const headingNone = AlienWays_AimAlong(way, 2, 0, At(0, 0, 0), 1500, &aim);
	bool const headingPast = AlienWays_AimAlong(way, 2, 2, At(0, 0, 0), 1500, &aim);
	bool const nowhere = AlienWays_AimAlong(way, 2, 1, At(0, 0, 0), 1500, 0);

	Check(!noPoints && !onePoint && !headingNone && !headingPast && !nowhere && Is(aim, 7, 8, 9),
		"no points, one point, heading for the first point or past the last, or nowhere to put the answer: refused, and the answer is left alone");
}

static void TestFarOut()
{
	puts("At the map's farthest corners:");

	AlienWaysPoint const way[] = { P(-64000000, 0, 0, F), P(64000000, 0, 0, F) };
	Check(Is(Aim(way, 2, 1, At(-63000000, 5, 5)), -62998500, 0, 0), "a way from 64 km out on one side to 64 km on the other is worked out right");
}

int main()
{
	TestStraight();
	TestTheGoal();
	TestSteps();
	TestBends();
	TestNoLength();
	TestRefused();
	TestFarOut();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
