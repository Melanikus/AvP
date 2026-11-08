// More made-up worlds for 3dc/avp/alien_ways_map.h, the map of the surfaces an alien can cling to and the search across it that can make the
// floor dear ("floor is lava", issue #109). Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/alien_ways_map_test.cpp checks the map itself on a corridor: every limit, every refusal, every kind. This test asks one thing of each
// of nine small worlds, each shaped like a place real levels have: a doorway, a gantry, a vent, a lift, stairs, a pillar, a long wall, an
// outside corner, and a ceiling between two walls. Each world's way is worked out by hand in the comments, at the floor's usual price (1) and
// at floor is lava (10), and where it helps, at the price where the way changes.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game.
//
// HOW TO READ THE SUMS. Coordinates are the game's: millimetres, and y goes down, so a ceiling 3 m up is at y = -3000. A way costs its length
// on walls and ceilings, plus its length on floors times the floor's price. The search crosses from one polygon to the next at the point of the
// edge they share that's nearest to where it came onto the first one. So a way hugs the walls it follows, and comes down off a wall at the near
// end of the goal's own edge. That's how the map is meant to work (alien_ways_map.h), and the sums below follow it.
//
// WHERE ONLY THE LENGTH IS CHECKED. In three worlds, at the usual price, two different ways are exactly as long: along the foot of a wall, or
// along the floor right beside it. Which one the search takes then depends on which polygon it happens to look at first, so those checks look
// only at the way's length. Everywhere else the whole way is checked, point for point. Each of those ways was also worked out with the ties
// between polygons broken the other way round, and came out the same, so a check doesn't depend on that order. (That was done with a copy of
// the search in JavaScript, which isn't in the repository: these sums, and this test's run, are what count.)

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "alien_ways_map.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Near(double a, double b, double within)
{
	return fabs(a - b) <= within;
}

float const kUp = 1.0f, kUpright = 0.0f, kDown = -1.0f;
static const AlienWaysKind kFloor = kAlienWaysFloor, kWall = kAlienWaysWall, kCeiling = kAlienWaysCeiling;

static AlienWaysPolygon Polygon(std::vector<AlienWaysXYZ> const &corners, float up)
{
	AlienWaysPolygon p;
	memset(&p, 0, sizeof(p));
	p.corners = static_cast<int>(corners.size());

	for (size_t i = 0; i < corners.size() && i < static_cast<size_t>(kAlienWaysMostCorners); i++)
	{
		p.x[i] = corners[i].x;
		p.y[i] = corners[i].y;
		p.z[i] = corners[i].z;
	}

	p.up = up;
	return p;
}

// A flat rectangle at height y, from x0 to x1 and from z0 to z1, facing up (a floor) or down (a ceiling)
static AlienWaysPolygon Flat(int x0, int x1, int y, int z0, int z1, float up)
{
	return Polygon({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, up);
}

// An upright wall standing at x, running from z0 to z1, from its bottom at height yBottom up to its top at yTop (the smaller number, as y goes down)
static AlienWaysPolygon WallAtX(int x, int z0, int z1, int yBottom, int yTop)
{
	return Polygon({ { x, yBottom, z0 }, { x, yTop, z0 }, { x, yTop, z1 }, { x, yBottom, z1 } }, kUpright);
}

// The same, standing at z and running from x0 to x1
static AlienWaysPolygon WallAtZ(int z, int x0, int x1, int yBottom, int yTop)
{
	return Polygon({ { x0, yBottom, z }, { x0, yTop, z }, { x1, yTop, z }, { x1, yBottom, z } }, kUpright);
}

static const char *NameOf(AlienWaysKind kind)
{
	return kind == kAlienWaysFloor ? "floor" : kind == kAlienWaysWall ? "wall" : "ceiling";
}

static bool Same(AlienWaysXYZ a, AlienWaysXYZ b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

// Whether polygons a and b of the map touch, along the stretch from p to q (either way round). A stretch from a point to itself means they
// touch only at that point, at a corner.
static bool TouchAlong(AlienWaysMap const &map, int a, int b, AlienWaysXYZ p, AlienWaysXYZ q)
{
	AlienWaysXYZ from = { 0, 0, 0 }, to = { 0, 0, 0 };

	if (!map.Joined(a, b, &from, &to))
	{
		return false;
	}

	return (Same(from, p) && Same(to, q)) || (Same(from, q) && Same(to, p));
}

struct Metres
{
	double floor, wall, ceiling;
};

// How many metres of a way are on each kind of polygon. A point's kind is what the way is on from it to the next point.
static Metres MetresOf(std::vector<AlienWaysPoint> const &way)
{
	Metres m = { 0.0, 0.0, 0.0 };

	for (size_t k = 1; k < way.size(); k++)
	{
		double const dx = static_cast<double>(way[k].x) - way[k - 1].x;
		double const dy = static_cast<double>(way[k].y) - way[k - 1].y;
		double const dz = static_cast<double>(way[k].z) - way[k - 1].z;
		double const metres = sqrt(dx * dx + dy * dy + dz * dz) / 1000.0;

		if (way[k - 1].kind == kAlienWaysFloor)
		{
			m.floor += metres;
		}
		else if (way[k - 1].kind == kAlienWaysWall)
		{
			m.wall += metres;
		}
		else
		{
			m.ceiling += metres;
		}
	}

	return m;
}

static double LengthOf(std::vector<AlienWaysPoint> const &way)
{
	Metres const m = MetresOf(way);
	return m.floor + m.wall + m.ceiling;
}

// The floor, wall and ceiling metres of a way, each within a centimetre
static bool MetresAre(std::vector<AlienWaysPoint> const &way, double onFloors, double onWalls, double onCeilings)
{
	Metres const m = MetresOf(way);
	return Near(m.floor, onFloors, 0.01) && Near(m.wall, onWalls, 0.01) && Near(m.ceiling, onCeilings, 0.01);
}

// One point a way should have, and the kind of polygon it should be on from there
struct Expected
{
	int x, y, z;
	AlienWaysKind kind;
};

// Whether the way is exactly these points, each on the kind of polygon given. If it isn't, this prints the way that was found, for whoever has to
// work out why.
static void CheckWay(std::vector<AlienWaysPoint> const &way, std::vector<Expected> const &expected, std::string const &what)
{
	bool same = way.size() == expected.size();

	for (size_t k = 0; same && k < way.size(); k++)
	{
		same = way[k].x == expected[k].x && way[k].y == expected[k].y && way[k].z == expected[k].z && way[k].kind == expected[k].kind;
	}

	Check(same, what);

	if (!same)
	{
		printf("        the way found has %u points:\n", static_cast<unsigned>(way.size()));

		for (size_t k = 0; k < way.size(); k++)
		{
			printf("          (%d, %d, %d) %s\n", way[k].x, way[k].y, way[k].z, NameOf(way[k].kind));
		}
	}
}

// Builds a world and checks that every polygon of it is on the map, numbered in the order it was given
static void Builds(AlienWaysMap &map, std::vector<AlienWaysPolygon> const &world, std::string const &what)
{
	const char *const why = map.Build(world);
	Check(why == nullptr && map.Ready() && map.Polygons() == world.size() && map.PolygonsLeftOut() == 0,
		what + " builds, with all " + std::to_string(world.size()) + " of its polygons" + (why ? std::string(": refused, \"") + why + "\"" : std::string()));
}

// Runs a search to its end, all at once. A search that wouldn't start is a failed check, with its reason.
static AlienWaysSearch SearchAll(AlienWaysMap &map, int from, AlienWaysXYZ start, int to, AlienWaysXYZ goal, int price)
{
	const char *const why = map.StartSearch(from, start, to, goal, price);

	if (why)
	{
		Check(false, std::string("the search starts: refused, \"") + why + "\"");
		return kAlienWaysNoWay;
	}

	AlienWaysSearch state = kAlienWaysSearching;

	for (int calls = 0; calls < 1000 && state == kAlienWaysSearching; calls++)
	{
		state = map.Work(100000);
	}

	return state;
}

// 1. A DOORWAY JOINED ONLY BY ITS FLOOR. Two rooms, each 2 m wide (x) and 4 m long (z), with floors of 1 m tiles, a wall each side and a ceiling
// 3 m up. Between them is a doorway 0.8 m wide and 0.5 m deep, the thickness of the wall. Each room's end wall stops either side of the doorway,
// so nothing of one room touches anything of the other: the doorway's floor is the only way across. Seen from above:
//
//            x = 0     600     1400   2000
//   z = 8500 +--------------------------+
//            |          room B          |    tiles 8 to 11, walls 12 (x = 0) and 13 (x = 2000), ceiling 14
//   z = 4500 +=========+      +=========+    B's end wall, in two parts: 17 and 18
//                      |  7   |              the doorway's floor
//   z = 4000 +=========+      +=========+    A's end wall, in two parts: 15 and 16
//            |          room A          |    tiles 0 to 3, walls 4 (x = 0) and 5 (x = 2000), ceiling 6
//   z = 0    +--------------------------+
static void Doorway()
{
	printf("A doorway joined only by its floor\n");
	std::vector<AlienWaysPolygon> world;

	for (int k = 0; k < 4; k++)
	{
		world.push_back(Flat(0, 2000, 0, k * 1000, (k + 1) * 1000, kUp));
	}

	world.push_back(WallAtX(0, 0, 4000, 0, -3000));
	world.push_back(WallAtX(2000, 0, 4000, 0, -3000));
	world.push_back(Flat(0, 2000, -3000, 0, 4000, kDown));
	world.push_back(Flat(600, 1400, 0, 4000, 4500, kUp));

	for (int k = 0; k < 4; k++)
	{
		world.push_back(Flat(0, 2000, 0, 4500 + k * 1000, 5500 + k * 1000, kUp));
	}

	world.push_back(WallAtX(0, 4500, 8500, 0, -3000));
	world.push_back(WallAtX(2000, 4500, 8500, 0, -3000));
	world.push_back(Flat(0, 2000, -3000, 4500, 8500, kDown));
	world.push_back(WallAtZ(4000, 0, 600, 0, -3000));
	world.push_back(WallAtZ(4000, 1400, 2000, 0, -3000));
	world.push_back(WallAtZ(4500, 0, 600, 0, -3000));
	world.push_back(WallAtZ(4500, 1400, 2000, 0, -3000));

	int const doorway = 7, lastTileOfA = 3, firstTileOfB = 8, goalTile = 11, endWallOfA = 15, endWallOfB = 17;
	AlienWaysMap map;
	Builds(map, world, "the two rooms and the doorway");

	Check(TouchAlong(map, doorway, lastTileOfA, { 600, 0, 4000 }, { 1400, 0, 4000 }) && TouchAlong(map, doorway, firstTileOfB, { 600, 0, 4500 }, { 1400, 0, 4500 }),
		"the doorway's floor touches each room's floor along its own width");
	Check(TouchAlong(map, doorway, endWallOfA, { 600, 0, 4000 }, { 600, 0, 4000 }) && TouchAlong(map, doorway, endWallOfB, { 600, 0, 4500 }, { 600, 0, 4500 }),
		"and each end wall only at the doorway's corner");

	int const roomA[] = { 0, 1, 2, 3, 4, 5, 6, 15, 16 }, roomB[] = { 8, 9, 10, 11, 12, 13, 14, 17, 18 };
	bool apart = true;

	for (int a : roomA)
	{
		for (int b : roomB)
		{
			AlienWaysXYZ p, q;
			apart = apart && !map.Joined(a, b, &p, &q);
		}
	}

	Check(apart, "nothing of room A touches anything of room B");

	AlienWaysXYZ const start = { 700, 0, 500 }, goal = { 700, 0, 8000 };

	// The floor cheap: straight along x = 700, 7.5 m, all floor, through the doorway 100 mm in from its edge
	Check(SearchAll(map, 0, start, goalTile, goal, 1) == kAlienWaysFound, "with the floor cheap, a way is found");
	CheckWay(map.Way(), { { 700, 0, 500, kFloor }, { 700, 0, 1000, kFloor }, { 700, 0, 2000, kFloor }, { 700, 0, 3000, kFloor }, { 700, 0, 4000, kFloor },
		{ 700, 0, 4500, kFloor }, { 700, 0, 5500, kFloor }, { 700, 0, 6500, kFloor }, { 700, 0, 7500, kFloor }, { 700, 0, 8000, kFloor } },
		"it goes straight along the floor, 7.5 m, across the doorway's 0.5 m");

	// Floor is lava, at 10. The start is 0.7 m from wall 4 and 1.3 m from wall 5, so the way leaves by wall 4:
	//   0.7 m of floor to wall 4, at (0, 0, 500)
	//   3.5 m along wall 4 to where it meets A's end wall 15, at (0, 0, 4000)
	//   0.6 m along the foot of 15 to the doorway's corner, (600, 0, 4000)
	//   0.5 m across the doorway's floor, along its edge, to B's end wall 17 at (600, 0, 4500): the floor the way can't do without
	//   0.6 m along the foot of 17 to wall 12, at (0, 0, 4500)
	//   3 m along wall 12 to where the goal's tile meets it, at (0, 0, 7500)
	//   and sqrt(700 x 700 + 500 x 500) = 0.86 m of floor to the goal.
	// Floor: 0.7 + 0.5 + 0.86 = 2.06 m. Wall: 3.5 + 0.6 + 0.6 + 3 = 7.7 m. It costs 10 x 2.06 + 7.7 = 28.3, against 75 along the floor.
	Check(SearchAll(map, 0, start, goalTile, goal, 10) == kAlienWaysFound, "with floor is lava, a way is found");
	CheckWay(map.Way(), { { 700, 0, 500, kFloor }, { 0, 0, 500, kWall }, { 0, 0, 4000, kWall }, { 600, 0, 4000, kFloor }, { 600, 0, 4500, kWall },
		{ 0, 0, 4500, kWall }, { 0, 0, 7500, kFloor }, { 700, 0, 8000, kFloor } },
		"it keeps to the walls, and crosses the doorway's floor at its edge, by the end walls");
	Check(MetresAre(map.Way(), 2.06, 7.7, 0.0), "2.06 m of floor, 0.5 m of it the doorway's, and 7.7 m of wall");
}

// 2. A GANTRY. A hall 6 m wide (x) and 4 m high, with a walkway across it 1.5 m up, from wall to wall, 1 m wide (z 4000 to 5000) and 100 mm
// thick. Its underside faces down, so it's a ceiling an alien can cling to. Each side wall of the hall is in two parts, below the gantry and
// above it, because that's where a level's wall has an edge for the gantry to touch: its underside meets the line where the two parts meet.
// Seen end on:
//
//     y = -4000  +===============================+   the hall's ceiling, 10
//                |                               |
//        wall 7  |                               |  wall 9
//     y = -1600  |===============================|   the gantry's top, 12 (a floor), and its sides 13 (z = 4000) and 14 (z = 5000)
//     y = -1500  |-------------------------------|   its underside, 11 (a ceiling)
//        wall 6  |                               |  wall 8
//     y = 0      +----+----+----+----+----+------+   the floor, strips 0 to 5, each 1 m wide (x) and the hall's length (z 0 to 10000)
//              x = 0                          x = 6000
static void Gantry()
{
	printf("A gantry, whose underside is a ceiling\n");
	std::vector<AlienWaysPolygon> world;

	for (int k = 0; k < 6; k++)
	{
		world.push_back(Flat(k * 1000, (k + 1) * 1000, 0, 0, 10000, kUp));
	}

	world.push_back(WallAtX(0, 0, 10000, 0, -1500));
	world.push_back(WallAtX(0, 0, 10000, -1500, -4000));
	world.push_back(WallAtX(6000, 0, 10000, 0, -1500));
	world.push_back(WallAtX(6000, 0, 10000, -1500, -4000));
	world.push_back(Flat(0, 6000, -4000, 0, 10000, kDown));
	world.push_back(Flat(0, 6000, -1500, 4000, 5000, kDown));
	world.push_back(Flat(0, 6000, -1600, 4000, 5000, kUp));
	world.push_back(WallAtZ(4000, 0, 6000, -1500, -1600));
	world.push_back(WallAtZ(5000, 0, 6000, -1500, -1600));

	int const goalStrip = 5, lowerLeft = 6, lowerRight = 8, underside = 11, top = 12;
	AlienWaysMap map;
	Builds(map, world, "the hall and its gantry");
	Check(map.KindOf(underside) == kAlienWaysCeiling && map.KindOf(top) == kAlienWaysFloor, "the gantry's underside is a ceiling, and its top a floor");
	Check(TouchAlong(map, lowerLeft, underside, { 0, -1500, 4000 }, { 0, -1500, 5000 }) && TouchAlong(map, underside, lowerRight, { 6000, -1500, 4000 }, { 6000, -1500, 5000 }),
		"the underside touches the lower part of each wall, along its own width");

	// From (500, 0, 4500), under the gantry by the left wall, to (5500, 0, 4500), under it by the right wall.
	//   Across the floor: 5 m, costing 5 x the floor's price.
	//   By the underside: 0.5 m of floor to wall 6, 1.5 m up it, 6 m along the underside, 1.5 m down wall 8, and 0.5 m of floor to the goal.
	//   That's 1 m of floor, 3 m of wall and 6 m of ceiling, costing 1 x the price + 9.
	//   By the hall's ceiling instead it would be 1 x the price + 4 + 6 + 4, always 5 more than by the gantry.
	// At a price of 2, 10 against 11, so the floor. At 3, 15 against 12, so the underside.
	AlienWaysXYZ const start = { 500, 0, 4500 }, goal = { 5500, 0, 4500 };
	std::vector<Expected> const acrossTheFloor = { { 500, 0, 4500, kFloor }, { 1000, 0, 4500, kFloor }, { 2000, 0, 4500, kFloor }, { 3000, 0, 4500, kFloor },
		{ 4000, 0, 4500, kFloor }, { 5000, 0, 4500, kFloor }, { 5500, 0, 4500, kFloor } };
	std::vector<Expected> const byTheUnderside = { { 500, 0, 4500, kFloor }, { 0, 0, 4500, kWall }, { 0, -1500, 4500, kCeiling }, { 6000, -1500, 4500, kWall },
		{ 6000, 0, 4500, kFloor }, { 5500, 0, 4500, kFloor } };

	Check(SearchAll(map, 0, start, goalStrip, goal, 1) == kAlienWaysFound, "with the floor cheap, a way is found");
	CheckWay(map.Way(), acrossTheFloor, "it goes straight across the floor, 5 m");
	Check(SearchAll(map, 0, start, goalStrip, goal, 2) == kAlienWaysFound, "at a price of 2, a way is found");
	CheckWay(map.Way(), acrossTheFloor, "and it's still across the floor: 10 against 11");
	Check(SearchAll(map, 0, start, goalStrip, goal, 3) == kAlienWaysFound, "at a price of 3, a way is found");
	CheckWay(map.Way(), byTheUnderside, "and it goes by the gantry's underside: 15 against 12");
	Check(SearchAll(map, 0, start, goalStrip, goal, 10) == kAlienWaysFound, "with floor is lava, a way is found");
	CheckWay(map.Way(), byTheUnderside, "up the wall, along the gantry's underside, and down the other wall");
	Check(MetresAre(map.Way(), 1.0, 3.0, 6.0), "1 m of floor, 3 m of wall and 6 m of ceiling");
}

// 3. A VENT OFF A WALL. A corridor 2 m wide (x) and 3 m high, with a vent in its left wall: a duct 600 mm square, its floor 2 m up, going 3 m
// into the wall to a dead end. The left wall is in four parts round the vent's mouth: below it (1), above it (2), and either side (3 and 4).
// The vent has a floor of three 1 m tiles (7, 8, 9, going in), a ceiling (10), two sides (11 at z = 5000 and 12 at z = 5600) and an end (13).
// The corridor has one floor (0), a right wall (5) and a ceiling (6). Seen from inside the corridor, looking at the left wall:
//
//     y = -3000  +------------------------------------------------+
//                |                       2                        |
//     y = -2600  +---------------------+------+-------------------+
//                |          3          | vent |         4         |
//     y = -2000  +---------------------+------+-------------------+
//                |                       1                        |
//     y = 0      +------------------------------------------------+
//              z = 0               z = 5000  5600              z = 10000
static void Vent()
{
	printf("A vent off a wall\n");
	std::vector<AlienWaysPolygon> world;
	world.push_back(Flat(0, 2000, 0, 0, 10000, kUp));
	world.push_back(WallAtX(0, 0, 10000, 0, -2000));
	world.push_back(WallAtX(0, 0, 10000, -2600, -3000));
	world.push_back(WallAtX(0, 0, 5000, -2000, -2600));
	world.push_back(WallAtX(0, 5600, 10000, -2000, -2600));
	world.push_back(WallAtX(2000, 0, 10000, 0, -3000));
	world.push_back(Flat(0, 2000, -3000, 0, 10000, kDown));

	for (int k = 0; k < 3; k++)
	{
		world.push_back(Flat(-(k + 1) * 1000, -k * 1000, -2000, 5000, 5600, kUp));
	}

	world.push_back(Flat(-3000, 0, -2600, 5000, 5600, kDown));
	world.push_back(WallAtZ(5000, -3000, 0, -2000, -2600));
	world.push_back(WallAtZ(5600, -3000, 0, -2000, -2600));
	world.push_back(WallAtX(-3000, 5000, 5600, -2000, -2600));

	int const belowTheVent = 1, firstVentTile = 7, lastVentTile = 9, ventSide = 11;
	AlienWaysMap map;
	Builds(map, world, "the corridor and its vent");
	Check(TouchAlong(map, belowTheVent, firstVentTile, { 0, -2000, 5000 }, { 0, -2000, 5600 }), "the wall below the vent touches the vent's floor along the mouth's width");
	Check(TouchAlong(map, belowTheVent, ventSide, { 0, -2000, 5000 }, { 0, -2000, 5000 }), "and the vent's side only at the mouth's corner");

	// From (500, 0, 2000) on the corridor's floor to (-2500, -2000, 5300), in the vent's last metre.
	//   Floor is lava: 0.5 m of floor to wall 1, then up and along it in a straight line to the corner of the vent's mouth, (0, -2000, 5000):
	//   sqrt(2000 x 2000 + 3000 x 3000) = 3.606 m. Into the vent along the foot of its side 11, 2 m, to where the last tile meets it at
	//   x = -2000, then sqrt(500 x 500 + 300 x 300) = 0.583 m of floor to the goal. Floor 1.083 m, wall 5.606 m. Going in on the vent's own
	//   floor instead would be 2 m more of floor, 20 more at this price.
	//   The floor cheap: the same length, 6.689 m. In the vent, its floor and the foot of its side are exactly as long, so which one the way
	//   takes then depends on which the search happens to look at first. Only the length is checked.
	AlienWaysXYZ const start = { 500, 0, 2000 }, goal = { -2500, -2000, 5300 };
	Check(SearchAll(map, 0, start, lastVentTile, goal, 1) == kAlienWaysFound, "with the floor cheap, a way into the vent is found");
	Check(Near(LengthOf(map.Way()), 6.689, 0.01), "it's 6.689 m long, climbing the wall to the vent's mouth");
	Check(SearchAll(map, 0, start, lastVentTile, goal, 10) == kAlienWaysFound, "with floor is lava, a way into the vent is found");
	CheckWay(map.Way(), { { 500, 0, 2000, kFloor }, { 0, 0, 2000, kWall }, { 0, -2000, 5000, kWall }, { -2000, -2000, 5000, kFloor }, { -2500, -2000, 5300, kFloor } },
		"it climbs to the mouth's corner, goes in along the vent's side, and steps onto the floor only in the last metre");
	Check(MetresAre(map.Way(), 1.083, 5.606, 0.0), "1.083 m of floor and 5.606 m of wall");
}

// 4. TWO FLOORS JOINED ONLY BY A LIFT. A lower floor (0, x 0 to 4000) and an upper floor (1, x 6000 to 10000) 4 m higher, with a lift between
// them. The lift's car is a floor (2, x 4000 to 6000, z 1000 to 3000) at the bottom, with a back wall (3) reaching up to the upper floor's
// edge. Nothing else joins the two floors. Seen from the side:
//
//     y = -4000                                 +------------------+   the upper floor, 1
//                                             3 |
//                                               |
//     y = 0      +------------------+-----------+
//                  the lower floor, 0   the car, 2
//              x = 0            x = 4000    x = 6000           x = 10000
//
// The map's listing of a level leaves a lift's car out, because a car isn't always where the level file has it (AddModules, in
// alien_ways_level.cpp). So in a level like this, the map has two floors that don't touch, and the search has to say there's no way, not make
// one up. With the car in, there's one way at any price: 2 m of floor to the car, 2 m across it, 4 m up its back wall, and 2 m to the goal.
static void Lift()
{
	printf("Two floors joined only by a lift\n");
	std::vector<AlienWaysPolygon> world;
	world.push_back(Flat(0, 4000, 0, 0, 4000, kUp));
	world.push_back(Flat(6000, 10000, -4000, 0, 4000, kUp));
	AlienWaysXYZ const start = { 2000, 0, 2000 }, goal = { 8000, -4000, 2000 };

	AlienWaysMap without;
	Builds(without, world, "the two floors with the car left out");
	Check(without.Joins() == 0, "they don't touch");
	Check(SearchAll(without, 0, start, 1, goal, 1) == kAlienWaysNoWay && without.Way().empty(), "with the floor cheap, there's no way, and the way is empty");
	Check(SearchAll(without, 0, start, 1, goal, 10) == kAlienWaysNoWay && without.Way().empty(), "with floor is lava, there's no way either");

	world.push_back(Flat(4000, 6000, 0, 1000, 3000, kUp));
	world.push_back(WallAtX(6000, 1000, 3000, 0, -4000));
	AlienWaysMap with;
	Builds(with, world, "the two floors with the car in");
	Check(TouchAlong(with, 0, 2, { 4000, 0, 1000 }, { 4000, 0, 3000 }) && TouchAlong(with, 3, 1, { 6000, -4000, 1000 }, { 6000, -4000, 3000 }),
		"the car's floor touches the lower floor, and its back wall the upper floor");

	std::vector<Expected> const byTheCar = { { 2000, 0, 2000, kFloor }, { 4000, 0, 2000, kFloor }, { 6000, 0, 2000, kWall }, { 6000, -4000, 2000, kFloor },
		{ 8000, -4000, 2000, kFloor } };

	for (int price : { 1, 10 })
	{
		std::string const at = " at a price of " + std::to_string(price);
		Check(SearchAll(with, 0, start, 1, goal, price) == kAlienWaysFound, "with the car in, a way is found" + at);
		CheckWay(with.Way(), byTheCar, "it's the only one there is, through the car and up its back wall" + at);
		Check(MetresAre(with.Way(), 6.0, 4.0, 0.0), "6 m of floor and 4 m of wall" + at);
	}
}

// 5. STAIRS. Five steps, each 300 mm deep and 200 mm high, from a floor (0, z -2000 to 0) up to a landing (11, z 1500 to 3500) 1 m up, all
// 2 m wide (x). Each step's upright face, its riser, is a wall (1, 3, 5, 7, 9), and each step's top, its tread, is a floor (2, 4, 6, 8, 10).
// Seen from the side, the first two steps:
//
//                         +------  and so on, up to the landing
//                       3 |
//              +----------+
//            1 |    2
//     ---------+
//      floor 0
//            z = 0       300
//
// There's nothing else to cling to, so the way is the same at any price: straight up, 1 m of floor to the first riser, then up each riser
// and across each tread, 5 x 0.2 = 1 m of wall and 5 x 0.3 = 1.5 m of floor, then 1 m across the landing. Floor 3.5 m, wall 1 m.
static void Stairs()
{
	printf("Stairs, with treads of floor and risers of wall\n");
	std::vector<AlienWaysPolygon> world;
	world.push_back(Flat(0, 2000, 0, -2000, 0, kUp));

	for (int k = 0; k < 5; k++)
	{
		world.push_back(WallAtZ(k * 300, 0, 2000, -k * 200, -(k + 1) * 200));
		world.push_back(Flat(0, 2000, -(k + 1) * 200, k * 300, (k + 1) * 300, kUp));
	}

	world.push_back(Flat(0, 2000, -1000, 1500, 3500, kUp));

	int const landing = 11;
	AlienWaysMap map;
	Builds(map, world, "the stairs");
	bool kinds = true;

	for (int k = 0; k < 5; k++)
	{
		kinds = kinds && map.KindOf(1 + 2 * k) == kAlienWaysWall && map.KindOf(2 + 2 * k) == kAlienWaysFloor;
	}

	Check(kinds, "every riser is a wall, and every tread a floor");
	Check(TouchAlong(map, 1, 2, { 0, -200, 0 }, { 2000, -200, 0 }) && TouchAlong(map, 2, 3, { 0, -200, 300 }, { 2000, -200, 300 }),
		"a tread touches the riser below it and the riser above it, along the step's width");

	std::vector<Expected> straightUp = { { 700, 0, -1000, kFloor } };

	for (int k = 0; k < 5; k++)
	{
		straightUp.push_back({ 700, -k * 200, k * 300, kWall });
		straightUp.push_back({ 700, -(k + 1) * 200, k * 300, kFloor });
	}

	straightUp.push_back({ 700, -1000, 1500, kFloor });
	straightUp.push_back({ 700, -1000, 2500, kFloor });

	for (int price : { 1, 10 })
	{
		std::string const at = " at a price of " + std::to_string(price);
		Check(SearchAll(map, 0, { 700, 0, -1000 }, landing, { 700, -1000, 2500 }, price) == kAlienWaysFound, "a way up the stairs is found" + at);
		CheckWay(map.Way(), straightUp, "it goes straight up, riser, tread, riser, tread" + at);
		Check(MetresAre(map.Way(), 3.5, 1.0, 0.0), "3.5 m of floor and 1 m of wall" + at);
	}
}

// 6. A ROOM WITH A PILLAR. A floor of 1 m squares, five by five, and a pillar 1 m square standing on the middle one, from the floor up, with four
// faces: south (24, z = 2000), west (25, x = 2000), north (26, z = 3000) and east (27, x = 3000). There's no other wall. Seen from above:
//
//     z = 5000  +---+---+---+---+---+
//               |   |   | G |   |   |     G: the goal, (2400, 0, 4500)
//               +---+---+---+---+---+
//               |   | 8 |   |   |   |
//     z = 3000  +---+---#####---+---+
//               |   |   # P #   |   |     P: the pillar
//     z = 2000  +---+---#####---+---+
//               |   |   |   |   |   |
//               +---+---+---+---+---+
//               |   |   | S |   |   |     S: the start, (2400, 0, 500)
//     z = 0     +---+---+---+---+---+
//             x = 0   2000  3000     5000
//
// The squares are numbered across from x = 0, a column at a time from z = 0, missing out the pillar's: the start's square is 10, the one north of
// it 11, the goal's 13, and 8 is the one beside the pillar's north-west corner.
static void Pillar()
{
	printf("A room with a pillar\n");
	std::vector<AlienWaysPolygon> world;
	int square[5][5];

	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 5; j++)
		{
			square[i][j] = -1;

			if (i == 2 && j == 2)
			{
				continue;
			}

			square[i][j] = static_cast<int>(world.size());
			world.push_back(Flat(i * 1000, (i + 1) * 1000, 0, j * 1000, (j + 1) * 1000, kUp));
		}
	}

	int const south = static_cast<int>(world.size()), west = south + 1;
	world.push_back(WallAtZ(2000, 2000, 3000, 0, -3000));
	world.push_back(WallAtX(2000, 2000, 3000, 0, -3000));
	world.push_back(WallAtZ(3000, 2000, 3000, 0, -3000));
	world.push_back(WallAtX(3000, 2000, 3000, 0, -3000));

	AlienWaysMap map;
	Builds(map, world, "the room and its pillar");
	Check(square[2][0] == 10 && square[2][4] == 13 && square[1][3] == 8 && south == 24, "the squares and faces have the numbers in the picture");
	Check(TouchAlong(map, south, west, { 2000, 0, 2000 }, { 2000, -3000, 2000 }), "the pillar's south and west faces meet along its corner, the whole height");
	Check(TouchAlong(map, square[2][1], south, { 2000, 0, 2000 }, { 3000, 0, 2000 }), "the square south of the pillar touches its south face along its foot");

	// The floor cheap: 0.5 m to the next square, then across it to the pillar's corner, sqrt(400 x 400 + 1000 x 1000) = 1.077 m, 1 m beside the
	// pillar, 1 m across square 8 to the goal's square, and sqrt(400 x 400 + 500 x 500) = 0.64 m to the goal: 4.217 m. The 1 m beside the
	// pillar can be along its foot or along the square next to it, which are exactly as long, so only the length is checked.
	//
	// Floor is lava: 0.5 m to the next square, and 1 m straight on to the pillar's south face, as no wall is nearer. Then 0.4 m along the foot of
	// that face to the pillar's corner, round the corner and 1 m along the west face; down onto square 8 at its corner (2000, 0, 3000), 1 m
	// across it to the goal's square, and 0.64 m to the goal. Floor 0.5 + 1 + 1 + 0.64 = 3.14 m, wall 1.4 m.
	AlienWaysXYZ const start = { 2400, 0, 500 }, goal = { 2400, 0, 4500 };
	Check(SearchAll(map, square[2][0], start, square[2][4], goal, 1) == kAlienWaysFound, "with the floor cheap, a way round the pillar is found");
	Check(Near(LengthOf(map.Way()), 4.217, 0.01), "it's 4.217 m long, cutting across to the pillar's corner");
	bool outside = true;

	for (AlienWaysPoint const &p : map.Way())
	{
		outside = outside && !(p.x > 2000 && p.x < 3000 && p.z > 2000 && p.z < 3000);
	}

	Check(outside, "and no point of it is inside the pillar");

	Check(SearchAll(map, square[2][0], start, square[2][4], goal, 10) == kAlienWaysFound, "with floor is lava, a way round the pillar is found");
	CheckWay(map.Way(), { { 2400, 0, 500, kFloor }, { 2400, 0, 1000, kFloor }, { 2400, 0, 2000, kWall }, { 2000, 0, 2000, kWall }, { 2000, 0, 3000, kFloor },
		{ 2000, 0, 4000, kFloor }, { 2400, 0, 4500, kFloor } },
		"it goes to the pillar's face, round its corner on the walls, and back to the floor past it");
	Check(MetresAre(map.Way(), 3.14, 1.4, 0.0), "3.14 m of floor and 1.4 m of wall");
}

// 7. A LONG WALL PAST SEVERAL TILES. One wall (0), 10 m long at x = 0, with five floor tiles of different lengths along its foot: 1 (z 0 to 1000),
// 2 (to 3500), 3 (to 4200), 4 (to 7000) and 5 (to 10000), each 2 m across. Tile 4 stops 11 mm short of the wall, so it doesn't touch it, only
// the tiles either side of it. The wall's bottom edge runs past every tile, but each tile touches it only along its own edge. Seen from above:
//
//     wall 0 ||   1   |     2     |  3  |      4      |      5      |      tile 4 starts at x = 11, the others at x = 0
//            z = 0   1000        3500  4200          7000         10000
static void LongWall()
{
	printf("A wall whose foot runs past several tiles\n");
	std::vector<AlienWaysPolygon> world;
	world.push_back(WallAtX(0, 0, 10000, 0, -3000));
	world.push_back(Flat(0, 2000, 0, 0, 1000, kUp));
	world.push_back(Flat(0, 2000, 0, 1000, 3500, kUp));
	world.push_back(Flat(0, 2000, 0, 3500, 4200, kUp));
	world.push_back(Flat(11, 2000, 0, 4200, 7000, kUp));
	world.push_back(Flat(0, 2000, 0, 7000, 10000, kUp));

	AlienWaysMap map;
	Builds(map, world, "the wall and its tiles");
	Check(TouchAlong(map, 0, 1, { 0, 0, 0 }, { 0, 0, 1000 }) && TouchAlong(map, 0, 2, { 0, 0, 1000 }, { 0, 0, 3500 }) && TouchAlong(map, 0, 3, { 0, 0, 3500 }, { 0, 0, 4200 })
		&& TouchAlong(map, 0, 5, { 0, 0, 7000 }, { 0, 0, 10000 }),
		"each tile touches the wall along its own edge, not the wall's whole foot");
	AlienWaysXYZ p, q;
	Check(!map.Joined(0, 4, &p, &q), "the tile 11 mm short doesn't touch the wall");
	Check(TouchAlong(map, 3, 4, { 11, 0, 4200 }, { 2000, 0, 4200 }) && TouchAlong(map, 4, 5, { 11, 0, 7000 }, { 2000, 0, 7000 }), "but it touches the tiles either side of it");

	// From (1500, 0, 500) on tile 1 to (1500, 0, 8500) on tile 5.
	//   The floor cheap: straight along, 8 m, all floor, across every tile, the one 11 mm short too.
	//   Floor is lava: 1.5 m of floor to the wall at (0, 0, 500), 6.5 m along it past tiles 2, 3 and 4 to where tile 5's own edge starts,
	//   (0, 0, 7000), and sqrt(1500 x 1500 + 1500 x 1500) = 2.121 m to the goal. Floor 3.621 m, wall 6.5 m.
	AlienWaysXYZ const start = { 1500, 0, 500 }, goal = { 1500, 0, 8500 };
	Check(SearchAll(map, 1, start, 5, goal, 1) == kAlienWaysFound, "with the floor cheap, a way is found");
	CheckWay(map.Way(), { { 1500, 0, 500, kFloor }, { 1500, 0, 1000, kFloor }, { 1500, 0, 3500, kFloor }, { 1500, 0, 4200, kFloor }, { 1500, 0, 7000, kFloor },
		{ 1500, 0, 8500, kFloor } },
		"it goes straight along the floor, 8 m, across every tile");
	Check(SearchAll(map, 1, start, 5, goal, 10) == kAlienWaysFound, "with floor is lava, a way is found");
	CheckWay(map.Way(), { { 1500, 0, 500, kFloor }, { 0, 0, 500, kWall }, { 0, 0, 7000, kFloor }, { 1500, 0, 8500, kFloor } },
		"it goes along the wall and comes down where the goal's tile starts, not at the wall's end");
	Check(MetresAre(map.Way(), 3.621, 6.5, 0.0), "3.621 m of floor and 6.5 m of wall");
}

// 8. AN OUTSIDE CORNER. A block standing in the corner of the world, x and z from 0 to 4000, 3 m high. Two of its faces show: the south face (9,
// z = 0) and the west face (10, x = 0), meeting at the block's corner edge, which sticks out towards the alien. The floor round the block is in
// 1 m strips: 0 to 3 south of it, 4 to 7 west of it, and the square 8 at the corner. Seen from above:
//
//     z = 4000  +-----++
//               |  7  ||  G: the goal, (-500, 0, 3500)
//               |  6  ||
//               |  5  ||        the block
//               |  4  ||
//     z = 0     +-----++=========================
//               |  8  |  0  |  1  |  2  |  3  |     S: the start, (3500, 0, -500), on strip 3
//    z = -1000  +-----+-----+-----+-----+-----+
//           x = -1000 0                       4000
//
//     ||  the west face, 10      ===  the south face, 9
static void OutsideCorner()
{
	printf("An outside corner\n");
	std::vector<AlienWaysPolygon> world;

	for (int k = 0; k < 4; k++)
	{
		world.push_back(Flat(k * 1000, (k + 1) * 1000, 0, -1000, 0, kUp));
	}

	for (int k = 0; k < 4; k++)
	{
		world.push_back(Flat(-1000, 0, 0, k * 1000, (k + 1) * 1000, kUp));
	}

	world.push_back(Flat(-1000, 0, 0, -1000, 0, kUp));
	world.push_back(WallAtZ(0, 0, 4000, 0, -3000));
	world.push_back(WallAtX(0, 0, 4000, 0, -3000));

	int const startStrip = 3, goalStrip = 7, southFace = 9, westFace = 10;
	AlienWaysMap map;
	Builds(map, world, "the block and the floor round it");
	Check(TouchAlong(map, southFace, westFace, { 0, 0, 0 }, { 0, -3000, 0 }), "the two faces meet along the block's corner edge, the whole height");

	// Floor is lava: 0.5 m of floor to the south face at (3500, 0, 0), 3.5 m along it to the block's corner, round the corner onto the west face
	// at (0, 0, 0), 3 m along that to where strip 7 meets it, (0, 0, 3000), and sqrt(500 x 500 + 500 x 500) = 0.707 m to the goal. Floor 1.207 m,
	// wall 6.5 m.
	// The floor cheap: along the strips south of the block, 0.5 + 1 + 1 m, across strip 0 to the corner, sqrt(1000 x 1000 + 500 x 500) = 1.118 m,
	// 3 m north beside the block, and 0.707 m to the goal: 7.325 m. Beside the block, the foot of the west face and the strips next to it are
	// exactly as long, so only the length is checked.
	AlienWaysXYZ const start = { 3500, 0, -500 }, goal = { -500, 0, 3500 };
	Check(SearchAll(map, startStrip, start, goalStrip, goal, 1) == kAlienWaysFound, "with the floor cheap, a way round the corner is found");
	Check(Near(LengthOf(map.Way()), 7.325, 0.01), "it's 7.325 m long, cutting the corner");
	Check(SearchAll(map, startStrip, start, goalStrip, goal, 10) == kAlienWaysFound, "with floor is lava, a way round the corner is found");
	CheckWay(map.Way(), { { 3500, 0, -500, kFloor }, { 3500, 0, 0, kWall }, { 0, 0, 0, kWall }, { 0, 0, 3000, kFloor }, { -500, 0, 3500, kFloor } },
		"it goes along the south face, round the block's corner onto the west face, and down by the goal");
	Check(MetresAre(map.Way(), 1.207, 6.5, 0.0), "1.207 m of floor and 6.5 m of wall");
}

// 9. A WIDE ROOM: UP, ACROSS THE CEILING, AND DOWN. A room 8 m wide (x), 4 m long (z) and 3 m high: a floor of 1 m strips (0 to 7), a wall each
// side (8 at x = 0, 9 at x = 8000) and a ceiling (10). Each wall meets the ceiling along its whole top edge. Seen end on:
//
//     y = -3000  +=======================================+   the ceiling, 10
//         wall 8 |                                       | wall 9
//     y = 0      +----+----+----+----+----+----+----+----+   strips 0 to 7
//              x = 0                                  x = 8000
//
// From (500, 0, 1500) to (7500, 0, 1500).
//   Across the floor: 7 m, costing 7 x the floor's price.
//   Over the ceiling: 0.5 m of floor to wall 8, 3 m up, 8 m across the ceiling, 3 m down wall 9, and 0.5 m of floor to the goal: 1 m of floor,
//   6 m of wall and 8 m of ceiling, costing 1 x the price + 14.
// At a price of 2 that's 14 against 16, so the floor. At 3 it's 21 against 17, so the ceiling.
static void Ceiling()
{
	printf("A wide room, crossed by its ceiling\n");
	std::vector<AlienWaysPolygon> world;

	for (int k = 0; k < 8; k++)
	{
		world.push_back(Flat(k * 1000, (k + 1) * 1000, 0, 0, 4000, kUp));
	}

	world.push_back(WallAtX(0, 0, 4000, 0, -3000));
	world.push_back(WallAtX(8000, 0, 4000, 0, -3000));
	world.push_back(Flat(0, 8000, -3000, 0, 4000, kDown));

	int const goalStrip = 7, leftWall = 8, rightWall = 9, ceiling = 10;
	AlienWaysMap map;
	Builds(map, world, "the wide room");
	Check(TouchAlong(map, leftWall, ceiling, { 0, -3000, 0 }, { 0, -3000, 4000 }) && TouchAlong(map, rightWall, ceiling, { 8000, -3000, 0 }, { 8000, -3000, 4000 }),
		"each wall meets the ceiling along its whole top edge");

	AlienWaysXYZ const start = { 500, 0, 1500 }, goal = { 7500, 0, 1500 };
	std::vector<Expected> acrossTheFloor = { { 500, 0, 1500, kFloor } };

	for (int k = 1; k <= 7; k++)
	{
		acrossTheFloor.push_back({ k * 1000, 0, 1500, kFloor });
	}

	acrossTheFloor.push_back({ 7500, 0, 1500, kFloor });
	std::vector<Expected> const overTheCeiling = { { 500, 0, 1500, kFloor }, { 0, 0, 1500, kWall }, { 0, -3000, 1500, kCeiling }, { 8000, -3000, 1500, kWall },
		{ 8000, 0, 1500, kFloor }, { 7500, 0, 1500, kFloor } };

	Check(SearchAll(map, 0, start, goalStrip, goal, 1) == kAlienWaysFound, "with the floor cheap, a way is found");
	CheckWay(map.Way(), acrossTheFloor, "it goes straight across the floor, 7 m");
	Check(SearchAll(map, 0, start, goalStrip, goal, 2) == kAlienWaysFound, "at a price of 2, a way is found");
	CheckWay(map.Way(), acrossTheFloor, "and it's still across the floor: 14 against 16");
	Check(SearchAll(map, 0, start, goalStrip, goal, 3) == kAlienWaysFound, "at a price of 3, a way is found");
	CheckWay(map.Way(), overTheCeiling, "and it goes over the ceiling: 21 against 17");
	Check(SearchAll(map, 0, start, goalStrip, goal, 10) == kAlienWaysFound, "with floor is lava, a way is found");
	CheckWay(map.Way(), overTheCeiling, "up the wall, across the ceiling, and down the other wall");
	Check(MetresAre(map.Way(), 1.0, 6.0, 8.0), "1 m of floor, 6 m of wall and 8 m of ceiling");
}

int main()
{
	Doorway();
	Gantry();
	Vent();
	Lift();
	Stairs();
	Pillar();
	LongWall();
	OutsideCorner();
	Ceiling();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all alien ways worlds checks passed\n");
	return 0;
}
