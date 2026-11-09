// How Hudson feels his way as he walks. Written 2026-11-05 by Melanikus for the AvP port. hudson_nav_feel.h says what it is.

#include "hudson_nav_feel.h"
#include "hudson_nav_plan.h"

#include <math.h>
#include <stdlib.h>

static double const kDegreesToRadians = 3.14159265358979 / 180.0;
static int const kPlacesOnHisBox = 5;
static double const kAcrossHim[kPlacesOnHisBox] = { 0.0, -0.5, 0.5, -1.0, 1.0 };

// Turns a way round, by this many degrees.
static void Turn(double x, double z, int degrees, double *turnedX, double *turnedZ)
{
	double const c = cos(degrees * kDegreesToRadians);
	double const s = sin(degrees * kDegreesToRadians);

	*turnedX = x * c - z * s;
	*turnedZ = x * s + z * c;
}

// As the rays want a direction: one whole length is 65536.
static int InWhole(double along)
{
	return (int)floor(along * kHudsonNavOneWhole + 0.5);
}

// How far his box reaches from his middle, along a way or across it. It's a square that doesn't turn, so that's further on a slant.
static double ReachOfHisBox(double wayX, double wayZ)
{
	return kHudsonNavMarineRadius * (fabs(wayX) + fabs(wayZ));
}

// A place on the front of his box, going this way, from his middle. across is how far across him it is, from -1 at one edge to 1 at the other. So at the edges on a slant, it's a corner that's level with his middle.
static void PlaceOnHisBox(double wayX, double wayZ, double across, double *placeX, double *placeZ)
{
	double const reach = ReachOfHisBox(wayX, wayZ);
	double const sideX = -wayZ * across * reach;
	double const sideZ = wayX * across * reach;
	double forward = 1.0e9;

	// As far forward from that as it's still inside the square
	if (wayX != 0.0)
	{
		double const toEdge = (((wayX > 0.0) ? kHudsonNavMarineRadius : -kHudsonNavMarineRadius) - sideX) / wayX;

		forward = (toEdge < forward) ? toEdge : forward;
	}

	if (wayZ != 0.0)
	{
		double const toEdge = (((wayZ > 0.0) ? kHudsonNavMarineRadius : -kHudsonNavMarineRadius) - sideZ) / wayZ;

		forward = (toEdge < forward) ? toEdge : forward;
	}

	*placeX = sideX + wayX * forward;
	*placeZ = sideZ + wayZ * forward;
}

int HudsonNav_FreeAhead(HudsonNavRays &rays, int x, int y, int z, double wayX, double wayZ)
{
	double const reach = ReachOfHisBox(wayX, wayZ);
	int const directionX = InWhole(wayX);
	int const directionZ = InWhole(wayZ);
	double frontX[kPlacesOnHisBox], frontZ[kPlacesOnHisBox];
	int free = kHudsonNavFeelAhead;

	// A little inside his box, so that a wall he's already touching is in front of the ray, and not behind where it starts
	for (int i = 0; i < kPlacesOnHisBox; i++)
	{
		PlaceOnHisBox(wayX, wayZ, kAcrossHim[i], &frontX[i], &frontZ[i]);
		frontX[i] -= wayX * kHudsonNavRayOverlap;
		frontZ[i] -= wayZ * kHudsonNavRayOverlap;
	}

	// At his head, as far as he feels
	int const headReach = kHudsonNavFeelAhead + 2 * kHudsonNavRayOverlap;

	for (int i = 0; i < kPlacesOnHisBox; i++)
	{
		HudsonNavRayAnswer const atHead = rays.Cast(x + (int)frontX[i], y - kHudsonNavHeadHeight, z + (int)frontZ[i], directionX, 0, directionZ, headReach);

		if (atHead.hit && atHead.distance - kHudsonNavRayOverlap < free && !atHead.door && atHead.normalY > -kHudsonNavWalkable)
		{
			free = (atHead.distance > kHudsonNavRayOverlap) ? atHead.distance - kHudsonNavRayOverlap : 0;
		}
	}

	// Then a foot at a time, as far as that. What he's standing on is what's under the front of his box, because that's what the game lifts him onto.
	int const footReach = kHudsonNavFootStep + 2 * kHudsonNavRayOverlap;
	int floorHeight = y;
	int walked = 0;

	while (walked + kHudsonNavFootStep <= free)
	{
		for (int i = 0; i < kPlacesOnHisBox; i++)
		{
			HudsonNavRayAnswer const atKnee = rays.Cast(x + (int)(frontX[i] + wayX * walked), floorHeight - kHudsonNavKneeHeight, z + (int)(frontZ[i] + wayZ * walked), directionX, 0, directionZ, footReach);

			if (atKnee.hit && atKnee.distance < footReach && !atKnee.door)
			{
				int const clear = walked + atKnee.distance - kHudsonNavRayOverlap;

				return (clear > free) ? free : ((clear > 0) ? clear : 0);
			}
		}

		walked += kHudsonNavFootStep;

		int const startHeight = floorHeight - kHudsonNavHeadHeight;
		HudsonNavRayAnswer const down = rays.Cast(x + (int)(wayX * (walked + reach)), startHeight, z + (int)(wayZ * (walked + reach)), 0, kHudsonNavOneWhole, 0, kHudsonNavHeadHeight + kHudsonNavStepUp);

		// An edge, as far as he should go without the map having said so
		if (!down.hit)
		{
			return walked - kHudsonNavFootStep;
		}

		if (down.door)
		{
			continue;
		}

		if (down.normalY > -kHudsonNavWalkable)
		{
			return walked - kHudsonNavFootStep;
		}

		int const nextFloor = startHeight + down.distance;
		int const rise = floorHeight - nextFloor;

		// Higher than a step, unless it's a slope, as in the map. A slope can rise up to twice as far as the foot is long.
		if (rise > kHudsonNavStepUp && (down.normalY <= -kHudsonNavFlatFloor || rise > 2 * kHudsonNavFootStep))
		{
			return walked - kHudsonNavFootStep;
		}

		floorHeight = nextFloor;
	}

	return free;
}

// How far to the nearest wall this way, at his knee, up to as far as he looks.
static int WallThisWay(HudsonNavRays &rays, int x, int y, int z, double wayX, double wayZ)
{
	HudsonNavRayAnswer const across = rays.Cast(x, y - kHudsonNavKneeHeight, z, InWhole(wayX), 0, InWhole(wayZ), kHudsonNavFeelSides);

	if (across.hit && across.distance < kHudsonNavFeelSides && !across.door && across.normalY > -kHudsonNavWalkable)
	{
		return across.distance;
	}

	return kHudsonNavFeelSides;
}

HudsonNavFeel HudsonNav_FeelTheWay(HudsonNavRays &rays, int x, int y, int z, double wantX, double wantZ)
{
	// Nearest first, and this side and that in turn, so that the same place always gives the same answer
	int const turns[] = { 0, 20, -20, 40, -40, 60, -60, 90, -90 };
	HudsonNavFeel best = { false, wantX, wantZ, -1, 0 };

	for (size_t i = 0; i < sizeof(turns) / sizeof(turns[0]); i++)
	{
		double wayX = 0.0, wayZ = 0.0;

		Turn(wantX, wantZ, turns[i], &wayX, &wayZ);

		int const free = HudsonNav_FreeAhead(rays, x, y, z, wayX, wayZ);

		if (free > best.freeAhead)
		{
			best.x = wayX;
			best.z = wayZ;
			best.freeAhead = free;
			best.turned = turns[i];
		}

		if (free >= kHudsonNavFeelAhead)
		{
			break;
		}
	}

	if (best.freeAhead < kHudsonNavFeelEnough)
	{
		HudsonNavFeel const none = { false, wantX, wantZ, best.freeAhead, 0 };

		return none;
	}

	best.canGo = true;

	// Near a wall on one side, with more room on the other, he turns a little towards the room. Not if that would take him where he can't go.
	double rightX = 0.0, rightZ = 0.0;

	Turn(best.x, best.z, 90, &rightX, &rightZ);

	int const towardsRight = WallThisWay(rays, x, y, z, rightX, rightZ);
	int const towardsLeft = WallThisWay(rays, x, y, z, -rightX, -rightZ);
	int const nearest = (towardsLeft < towardsRight) ? towardsLeft : towardsRight;

	if (nearest < kHudsonNavNearAWall && towardsLeft != towardsRight)
	{
		double const lean = (double)(towardsRight - towardsLeft) / (double)(towardsRight + towardsLeft);
		int const degrees = (int)(lean * kHudsonNavMostTurnFromAWall);
		double wayX = 0.0, wayZ = 0.0;

		Turn(best.x, best.z, degrees, &wayX, &wayZ);

		if (degrees != 0 && HudsonNav_FreeAhead(rays, x, y, z, wayX, wayZ) >= kHudsonNavFeelEnough)
		{
			best.x = wayX;
			best.z = wayZ;
		}
	}

	return best;
}

HudsonNavFeel HudsonNav_MostRoom(HudsonNavRays &rays, int x, int y, int z, double wantX, double wantZ)
{
	HudsonNavFeel best = { false, 1.0, 0.0, -1, 0 };
	double bestAlong = 2.0;

	for (int degrees = 0; degrees < 360; degrees += 45)
	{
		double wayX = 0.0, wayZ = 0.0;

		Turn(1.0, 0.0, degrees, &wayX, &wayZ);

		int const free = HudsonNav_FreeAhead(rays, x, y, z, wayX, wayZ);
		double const along = fabs(wayX * wantX + wayZ * wantZ);

		if (free > best.freeAhead || (free == best.freeAhead && along < bestAlong))
		{
			best.x = wayX;
			best.z = wayZ;
			best.freeAhead = free;
			bestAlong = along;
		}
	}

	best.canGo = best.freeAhead >= kHudsonNavFeelEnough;

	return best;
}
