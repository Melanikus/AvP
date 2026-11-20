// hudson_supply.h: where the lane probe makes its next alien around Hudson (supplyat=around, hudson_lanes.h), with no game in it, so a test can run it. Written
// 2026-11-20 by Melanikus (Frost) for the AvP port (Bishop for the owner, on his second stretch: the first supplyat=around took any room 15 to 25 m away in a straight line,
// so it made its aliens a storey above him and few reached him).
//
// THE RULE. Of the rooms' middles (where a marine can stand: HudsonNav_MiddleOfRoom), the one where all of these hold:
//   - out of his view (not in the game's own list of the rooms he can see, ModuleCurrVisArray),
//   - kHudsonSupplyNear to kHudsonSupplyFar from him,
//   - on his level: within kHudsonSupplyLevel of his height,
//   - by his route ahead: within kHudsonSupplyRoute across of one of the places he walks to next,
// and of those, the one nearest kHudsonSupplyMiddle from him, so that the same walk makes the same aliens. An alien keeps its slot while it is alive, however far
// off (Bishop 11-20, the owner's "swarm of aliens": a far one freeing its slot let 14 be made in a run); only a dead one frees it. At most kHudsonSupplyMostTimes
// times the cap are made in a run (the owner: "Let's limit to 4": 4 alive, 8 made).
// y goes DOWN in this game, as everywhere in it.

#ifndef HUDSON_SUPPLY_H
#define HUDSON_SUPPLY_H

#include <math.h>
#include <vector>

static const double kHudsonSupplyNear = 15000;     // mm
static const double kHudsonSupplyFar = 25000;
static const double kHudsonSupplyMiddle = 20000;
static const double kHudsonSupplyLevel = 2000;     // mm up or down from his height
static const double kHudsonSupplyRoute = 6000;     // mm across from a place ahead
static const int kHudsonSupplyMostTimes = 2;
// supplyin=: in the named rooms only (the corridors of his route), so no route test, and further off: they are already on his way
static const double kHudsonSupplyInNear = 10000;   // mm
static const double kHudsonSupplyInFar = 40000;

struct HudsonSupplyRoom
{
	int x, y, z;          // the room's middle, where a marine can stand
	bool inHisView;       // the room is in the game's list of the rooms he can see
};

struct HudsonSupplyPlace
{
	int x, y, z;          // a place he walks to next
};

// The index of the room to make the next alien in, or -1 if none fits. hx, hy, hz: where he is.
inline int HudsonSupply_Pick(std::vector<HudsonSupplyRoom> const &rooms, int hx, int hy, int hz, std::vector<HudsonSupplyPlace> const &ahead)
{
	int best = -1;
	double bestOff = 1e300;

	for (size_t i = 0; i < rooms.size(); i++)
	{
		HudsonSupplyRoom const &r = rooms[i];

		if (r.inHisView || fabs((double)r.y - hy) > kHudsonSupplyLevel)
		{
			continue;
		}

		double const dx = (double)r.x - hx, dy = (double)r.y - hy, dz = (double)r.z - hz;
		double const d = sqrt(dx * dx + dy * dy + dz * dz);

		if (d < kHudsonSupplyNear || d > kHudsonSupplyFar || fabs(d - kHudsonSupplyMiddle) >= bestOff)
		{
			continue;
		}

		bool byTheRoute = false;

		for (HudsonSupplyPlace const &p : ahead)
		{
			double const ax = (double)r.x - p.x, az = (double)r.z - p.z;

			byTheRoute = byTheRoute || (ax * ax + az * az <= kHudsonSupplyRoute * kHudsonSupplyRoute && fabs((double)r.y - p.y) <= kHudsonSupplyLevel);
		}

		if (byTheRoute)
		{
			best = (int)i;
			bestOff = fabs(d - kHudsonSupplyMiddle);
		}
	}

	return best;
}

// supplyin=: of the named rooms' middles (the caller gives only those), the one out of his view, on his level, kHudsonSupplyInNear to kHudsonSupplyInFar from him,
// nearest kHudsonSupplyMiddle; -1 if none fits
inline int HudsonSupply_PickIn(std::vector<HudsonSupplyRoom> const &rooms, int hx, int hy, int hz)
{
	int best = -1;
	double bestOff = 1e300;

	for (size_t i = 0; i < rooms.size(); i++)
	{
		HudsonSupplyRoom const &r = rooms[i];
		double const dx = (double)r.x - hx, dy = (double)r.y - hy, dz = (double)r.z - hz;
		double const d = sqrt(dx * dx + dy * dy + dz * dz);

		if (r.inHisView || fabs(dy) > kHudsonSupplyLevel || d < kHudsonSupplyInNear || d > kHudsonSupplyInFar || fabs(d - kHudsonSupplyMiddle) >= bestOff)
		{
			continue;
		}

		best = (int)i;
		bestOff = fabs(d - kHudsonSupplyMiddle);
	}

	return best;
}

// Whether one of the supply's aliens keeps its slot: while it is alive, however far from him (the distance stays in the call: a far one no longer frees it)
inline bool HudsonSupply_HoldsASlot(bool alive, double distanceFromHim)
{
	(void)distanceFromHim;
	return alive;
}

// The most made in a run, for a cap of this many
inline int HudsonSupply_MostMade(int cap)
{
	return cap * kHudsonSupplyMostTimes;
}

#endif
