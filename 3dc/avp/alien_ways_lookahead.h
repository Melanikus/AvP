// Where an alien on its way aims: a point a little further along the way, so that it curves round the way's bends instead of running
// from one corner to the next (#109). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY. The guide (alien_ways_guide.cpp) steers an alien straight at the next point of its way. So it runs to that point, turns there, and
// runs to the one after: corner to corner. This gives a point to aim at instead. Start at the alien's nearest place on the stretch of the
// way it's on, then go a set distance further along the way's own stretches (about 1.5 m). On a gentle bend, that point slides round the
// bend ahead of the alien, so the alien curves round it.
//
//      way:   A ------------------ B
//                                   \            the alien (o) aims at x, 1.5 m along the way from its nearest place on it,
//             o--------------------> x            so it starts turning before it gets to B
//                                     \
//                                      C
//
// WHERE THE AIM STOPS SHORT. It never goes:
// - past the way's last point, the goal;
// - past a step onto another kind of surface (floor, wall or ceiling) that the alien hasn't made yet. The guide counts a step as made only
//   once the alien is on the new surface (OnTheKindNeeded), and until then the alien has to go to the corner where the two surfaces meet,
//   so that the game's own climbing turns it onto the new one. A point past the step is on the other surface, and aiming at it would
//   point into, or out of, the surface the alien is clinging to;
// - round bends that, added together, turn the way more than kAlienWaysAimMostTurnDegrees. The alien goes in a straight line to the
//   aim, and a straight line to a point round a bend cuts the corner. An outside corner can't be cut. Third and Primary found an alien
//   stuck on Ops01 at an outside corner between two walls (polygons 451 to 565), where the next point was round the corner. On a gentle
//   bend the line cuts in only a little. With one bend of just under 30 degrees half-way along the 1.5 m, the line passes 0.19 m inside
//   the corner point (tests/alien_ways_lookahead_test.cpp measures it).
//
// It knows nothing of the game. alien_ways_guide.cpp is meant to call it where it now aims at points[heading], with a setting for how far
// ahead to look, so a film can compare the two. With that setting at 0 it aims at points[heading], exactly as before.
//
// NOT DONE YET (debt, for after the film):
// - The numbers (1.5 m ahead, and 30 degrees of bend) are a first guess. The film decides them.
// - The alien's width. The amount a gentle bend is cut by doesn't depend on how big the alien is, or on how near a wall it is. A pillar's
//   corner on the floor is cut by the same amount as a corner in the open.
// - Only the way's own points are known: nothing here checks that the straight line to the aim stays on a surface, or clear of one.
// - The guide's "reached" and "stuck" rules still look at points[heading], not at the aim. That's the guide's part, for when it's wired.

#ifndef ALIEN_WAYS_LOOKAHEAD_H
#define ALIEN_WAYS_LOOKAHEAD_H

#include <math.h>

#include "alien_ways_map.h"

// How far ahead along its way an alien looks for the point it aims at, in mm, as a start
int const kAlienWaysAimAheadMm = 1500;

// The farthest it may look, whatever it's asked, in mm. A way is at most kAlienWaysMostWayPoints points, and this keeps the aim near the
// alien even on a way with very long stretches.
int const kAlienWaysAimMostAheadMm = 10000;

// The most the way may turn, all its bends together, between the alien's nearest place on it and the aim, in degrees. A bend that takes it
// past this stops the aim at that bend.
double const kAlienWaysAimMostTurnDegrees = 30.0;

namespace AlienWaysAim
{
	// A place, or a direction, in mm, worked out in doubles so that nothing overflows even at the map's farthest corners (64 km out)
	struct Place
	{
		double x, y, z;
	};

	inline Place PlaceOf(AlienWaysPoint const &point)
	{
		Place const place = { static_cast<double>(point.x), static_cast<double>(point.y), static_cast<double>(point.z) };
		return place;
	}

	inline Place PlaceOf(AlienWaysXYZ const &xyz)
	{
		Place const place = { static_cast<double>(xyz.x), static_cast<double>(xyz.y), static_cast<double>(xyz.z) };
		return place;
	}

	inline double Length(Place const &a, Place const &b)
	{
		double const dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
		return sqrt(dx * dx + dy * dy + dz * dz);
	}

	// The point part of the way from a to b: 0 is a, 1 is b
	inline Place PartWay(Place const &a, Place const &b, double part)
	{
		Place const place = { a.x + (b.x - a.x) * part, a.y + (b.y - a.y) * part, a.z + (b.z - a.z) * part };
		return place;
	}

	// The nearest place to p on the stretch from a to b. A stretch of no length is just a.
	inline Place NearestOn(Place const &a, Place const &b, Place const &p)
	{
		double const ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
		double const uu = ux * ux + uy * uy + uz * uz;

		if (uu <= 0.0)
		{
			return a;
		}

		double part = ((p.x - a.x) * ux + (p.y - a.y) * uy + (p.z - a.z) * uz) / uu;
		part = part < 0.0 ? 0.0 : (part > 1.0 ? 1.0 : part);
		return PartWay(a, b, part);
	}

	// How much, in degrees, the direction from a to b turns to become the direction from b to c. A stretch of no length has no direction,
	// so it turns nothing.
	inline double TurnAt(Place const &a, Place const &b, Place const &c)
	{
		double const inLength = Length(a, b), outLength = Length(b, c);

		if (inLength <= 0.0 || outLength <= 0.0)
		{
			return 0.0;
		}

		double cosine = ((b.x - a.x) * (c.x - b.x) + (b.y - a.y) * (c.y - b.y) + (b.z - a.z) * (c.z - b.z)) / (inLength * outLength);
		cosine = cosine < -1.0 ? -1.0 : (cosine > 1.0 ? 1.0 : cosine);
		return acos(cosine) * (180.0 / 3.14159265358979323846);
	}

	inline AlienWaysXYZ Rounded(Place const &place)
	{
		AlienWaysXYZ const xyz = { static_cast<int>(lround(place.x)), static_cast<int>(lround(place.y)), static_cast<int>(lround(place.z)) };
		return xyz;
	}
}

// Puts in *aim the point an alien at `at` should steer at. Its way is count points, and it's heading for points[heading], on the stretch
// from points[heading - 1] to points[heading], as the guide keeps it. aheadMm is how far along the way to look, at most
// kAlienWaysAimMostAheadMm. 0 or less gives points[heading] itself, the guide's aim before this.
//
// Gives back false, and leaves *aim as it was, for a way it can't use: no points, heading not one of them past the first (so a way of
// fewer than 2 points is refused too), or nowhere to put the answer. Then the guide aims as it did before.
//
// The aim is always on the way's own stretches, so it's never farther out than the way's points are, and it's worked out in doubles.
inline bool AlienWays_AimAlong(const AlienWaysPoint *points, int count, int heading, AlienWaysXYZ at, int aheadMm, AlienWaysXYZ *aim)
{
	using namespace AlienWaysAim;

	if (!points || !aim || heading < 1 || heading >= count)
	{
		return false;
	}

	if (aheadMm <= 0)
	{
		*aim = AlienWaysXYZ{ points[heading].x, points[heading].y, points[heading].z };
		return true;
	}

	// How much farther along the way the aim still has to go
	double left = aheadMm < kAlienWaysAimMostAheadMm ? static_cast<double>(aheadMm) : static_cast<double>(kAlienWaysAimMostAheadMm);

	// How much the way has turned so far, from the alien's nearest place on it
	double turned = 0.0;

	// From the alien's nearest place on its stretch, along each stretch in turn. point is the end of the stretch being walked along.
	Place from = NearestOn(PlaceOf(points[heading - 1]), PlaceOf(points[heading]), PlaceOf(at));

	// Where the way last came from, for the direction it comes into a point in. A stretch of no length has no direction, so this stays at
	// the last point the way really moved from: a bend just after two points in the same place is still seen.
	Place cameFrom = PlaceOf(points[heading - 1]);

	for (int point = heading; point < count; point++)
	{
		Place const to = PlaceOf(points[point]);
		double const length = Length(from, to);

		// The aim is on this stretch
		if (length >= left)
		{
			*aim = Rounded(PartWay(from, to, left / length));
			return true;
		}

		left -= length;

		// The goal: there's no stretch after it
		if (point + 1 >= count)
		{
			break;
		}

		// The aim waits at this point of the way if it's a step onto another kind of surface, which the alien hasn't made yet, or a bend
		// that takes the way past the most it may turn
		Place const next = PlaceOf(points[point + 1]);
		bool const step = points[point].kind != points[point - 1].kind;
		turned += TurnAt(cameFrom, to, next);

		if (step || turned > kAlienWaysAimMostTurnDegrees)
		{
			*aim = AlienWaysXYZ{ points[point].x, points[point].y, points[point].z };
			return true;
		}

		if (Length(to, next) > 0.0)
		{
			cameFrom = to;
		}

		from = to;
	}

	// The way ran out before the aim did: the aim is its last point, the goal
	*aim = AlienWaysXYZ{ points[count - 1].x, points[count - 1].y, points[count - 1].z };
	return true;
}

#endif
