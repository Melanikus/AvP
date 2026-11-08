// The test for 3dc/avp/alien_ways_map.h: the map of a level's surfaces an alien can cling to, and the search across it that can make the floor
// dear ("floor is lava", issue #109). Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game. Every world here is
// made up, a few polygons at a time, so each answer can be worked out by hand, and the sums are in the comments beside the checks.
//
// The main world is a corridor, seen here from the side and from above:
//
//         y = -3000  +===========================+   ceiling, polygon 12
//                    |                           |
//      left wall     |                           |   right wall
//      x = 0, 10     |                           |   x = 2000, 11
//         y = 0      +--+--+--+--+--+--+--+--+--++   floor tiles 0 to 9, each 2 m across (x) and 1 m long (z)
//                    z = 0                  z = 10000
//
// Coordinates are the game's: millimetres, and y goes down, so the ceiling is at y = -3000.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
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

// Whether a reason is exactly the one expected. Nothing given back counts as a different reason.
static bool Says(const char *why, const char *expected)
{
	return why != nullptr && strcmp(why, expected) == 0;
}

static bool Near(double a, double b, double within)
{
	return fabs(a - b) <= within;
}

float const kUp = 1.0f, kUpright = 0.0f, kDown = -1.0f;

// A polygon from its corners, facing the way given. A list of more than kAlienWaysMostCorners keeps only that many, but says how many it was
// given, so a test can hand the map a polygon with too many.
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

// The corridor: tiles floor tiles of tileLength each along z, then the left wall, the right wall and the ceiling
static std::vector<AlienWaysPolygon> Corridor(int tiles, int tileLength)
{
	std::vector<AlienWaysPolygon> world;
	int const length = tiles * tileLength;

	for (int k = 0; k < tiles; k++)
	{
		world.push_back(Polygon({ { 0, 0, k * tileLength }, { 2000, 0, k * tileLength }, { 2000, 0, (k + 1) * tileLength }, { 0, 0, (k + 1) * tileLength } }, kUp));
	}

	world.push_back(Polygon({ { 0, 0, 0 }, { 0, -3000, 0 }, { 0, -3000, length }, { 0, 0, length } }, kUpright));
	world.push_back(Polygon({ { 2000, 0, 0 }, { 2000, -3000, 0 }, { 2000, -3000, length }, { 2000, 0, length } }, kUpright));
	world.push_back(Polygon({ { 0, -3000, 0 }, { 2000, -3000, 0 }, { 2000, -3000, length }, { 0, -3000, length } }, kDown));
	return world;
}

// A flat square, facing up, with its corner nearest the middle of the world at (x, y, z)
static AlienWaysPolygon Square(int x, int y, int z, int side)
{
	return Polygon({ { x, y, z }, { x + side, y, z }, { x + side, y, z + side }, { x, y, z + side } }, kUp);
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

static bool SameStretch(AlienWaysXYZ a, AlienWaysXYZ b, AlienWaysXYZ p, AlienWaysXYZ q)
{
	auto same = [](AlienWaysXYZ u, AlienWaysXYZ v) { return u.x == v.x && u.y == v.y && u.z == v.z; };
	return (same(a, p) && same(b, q)) || (same(a, q) && same(b, p));
}

static bool At(AlienWaysPoint p, AlienWaysXYZ q)
{
	return p.x == q.x && p.y == q.y && p.z == q.z;
}

// Runs a search to its end, looking at at most `each` polygons a call, as the game will across frames. why gets StartSearch's answer.
static AlienWaysSearch SearchAll(AlienWaysMap &map, int from, AlienWaysXYZ start, int to, AlienWaysXYZ goal, int price, int each, const char **why)
{
	*why = map.StartSearch(from, start, to, goal, price);

	if (*why)
	{
		return kAlienWaysNoWay;
	}

	AlienWaysSearch state = kAlienWaysSearching;

	for (int calls = 0; calls < 10000000 && state == kAlienWaysSearching; calls++)
	{
		state = map.Work(each);
	}

	return state;
}

// The same, all at once
static AlienWaysSearch SearchAll(AlienWaysMap &map, int from, AlienWaysXYZ start, int to, AlienWaysXYZ goal, int price)
{
	const char *why = nullptr;
	return SearchAll(map, from, start, to, goal, price, 1000000, &why);
}

static void Kinds()
{
	printf("Which kind a polygon is\n");
	Check(AlienWays_KindOf(1.0f) == kAlienWaysFloor, "facing straight up is a floor");
	Check(AlienWays_KindOf(kAlienWaysFloorFacing) == kAlienWaysFloor, "facing up by exactly the game's floor angle is still a floor");
	Check(AlienWays_KindOf(nextafterf(kAlienWaysFloorFacing, 0.0f)) == kAlienWaysWall, "a hair less is a wall");
	Check(AlienWays_KindOf(0.0f) == kAlienWaysWall, "upright is a wall");
	Check(AlienWays_KindOf(-kAlienWaysFloorFacing) == kAlienWaysCeiling, "facing down by exactly the floor angle is a ceiling");
	Check(AlienWays_KindOf(nextafterf(-kAlienWaysFloorFacing, 0.0f)) == kAlienWaysWall, "a hair less down is a wall");
	Check(AlienWays_KindOf(-1.0f) == kAlienWaysCeiling, "facing straight down is a ceiling");
}

static void Facing()
{
	printf("How much a polygon faces up, from its corners\n");

	// Newell's sum of (z of a corner - z of the next) x (x of the corner + x of the next), the only edge that adds anything going from
	// (1000, 0) to (1000, 1000): (0 - 1000) x 2000 = -2,000,000, against a size of 2,000,000, and the minus in front makes it +1
	AlienWaysPolygon const square = Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 0, 1000 }, { 0, 0, 1000 } }, 0.0f);
	Check(Near(AlienWays_HowMuchItFacesUp(square), 1.0, 1e-6), "a flat square with its corners in this order faces up");

	AlienWaysPolygon const flipped = Polygon({ { 0, 0, 0 }, { 0, 0, 1000 }, { 1000, 0, 1000 }, { 1000, 0, 0 } }, 0.0f);
	Check(Near(AlienWays_HowMuchItFacesUp(flipped), -1.0, 1e-6), "the same square with its corners the other way round faces down");

	AlienWaysPolygon const upright = Polygon({ { 0, 0, 0 }, { 0, -1000, 0 }, { 0, -1000, 1000 }, { 0, 0, 1000 } }, 0.0f);
	Check(Near(AlienWays_HowMuchItFacesUp(upright), 0.0, 1e-6), "an upright square faces neither up nor down");

	AlienWaysPolygon const lineFirst = Polygon({ { 0, 0, 0 }, { 500, 0, 0 }, { 1000, 0, 0 }, { 1000, 0, 1000 }, { 0, 0, 1000 } }, 0.0f);
	Check(Near(AlienWays_HowMuchItFacesUp(lineFirst), 1.0, 1e-6), "a square whose first three corners lie in a line still faces up");

	AlienWaysPolygon const line = Polygon({ { 0, 0, 0 }, { 500, 0, 0 }, { 1000, 0, 0 } }, 0.0f);
	Check(AlienWays_HowMuchItFacesUp(line) == 0.0f, "three corners in a line face nowhere, and give 0");
}

static void Joins()
{
	printf("Where polygons touch\n");
	AlienWaysMap map;
	Check(map.Build(Corridor(10, 1000)) == nullptr, "the corridor builds");
	Check(map.Ready() && map.Polygons() == 13 && map.PolygonsLeftOut() == 0, "all 13 of its polygons are on the map");
	Check(map.MapNumberOf(0) == 0 && map.MapNumberOf(12) == 12 && map.MapNumberOf(13) == -1, "they keep the numbers they were given, and there's no 14th");
	Check(map.KindOf(0) == kAlienWaysFloor && map.KindOf(10) == kAlienWaysWall && map.KindOf(11) == kAlienWaysWall && map.KindOf(12) == kAlienWaysCeiling,
		"the tiles are floors, the walls walls and the ceiling a ceiling");

	AlienWaysXYZ a = { 0, 0, 0 }, b = { 0, 0, 0 };
	Check(map.Joined(0, 1, &a, &b) && SameStretch(a, b, { 0, 0, 1000 }, { 2000, 0, 1000 }), "two tiles side by side touch along the whole edge they share");
	Check(map.Joined(3, 10, &a, &b) && SameStretch(a, b, { 0, 0, 3000 }, { 0, 0, 4000 }),
		"a tile touches the wall along its own edge, though the wall's bottom edge runs on past it both ways");
	Check(map.Joined(9, 11, &a, &b) && SameStretch(a, b, { 2000, 0, 9000 }, { 2000, 0, 10000 }), "and the last tile touches the right wall the same way");
	Check(map.Joined(10, 12, &a, &b) && SameStretch(a, b, { 0, -3000, 0 }, { 0, -3000, 10000 }), "the left wall and the ceiling touch along their whole length");
	Check(map.Joined(4, 5, &a, &b) && map.Joined(5, 4, &a, &b), "a join is there from both sides");
	Check(!map.Joined(0, 5, &a, &b), "tiles apart don't touch");
	Check(!map.Joined(10, 11, &a, &b), "the two walls don't touch");
	Check(!map.Joined(0, 0, &a, &b), "a polygon isn't joined to itself");
	Check(!map.Joined(0, 13, &a, &b) && !map.Joined(-1, 0, &a, &b), "asking about a polygon that isn't there says no");

	// Two squares with a gap between them. A corner within 10 mm of the other's edge touches it, and one 11 mm away doesn't.
	AlienWaysMap gap;
	Check(gap.Build({ Square(0, 0, 0, 1000), Square(1009, 0, 0, 1000) }) == nullptr && gap.Joined(0, 1, &a, &b), "a 9 mm gap counts as touching");
	Check(gap.Build({ Square(0, 0, 0, 1000), Square(1010, 0, 0, 1000) }) == nullptr && gap.Joined(0, 1, &a, &b), "so does exactly 10 mm");
	Check(gap.Build({ Square(0, 0, 0, 1000), Square(1011, 0, 0, 1000) }) == nullptr && !gap.Joined(0, 1, &a, &b), "an 11 mm gap doesn't");

	// Squares either side of a box boundary at x = 1000 and at x = 0, below 0 too, so a corner in the next box along is still found
	Check(gap.Build({ Square(-1000, 0, -1000, 1000), Square(0, 0, -1000, 1000) }) == nullptr && gap.Joined(0, 1, &a, &b), "touching across a box boundary, below 0");
}

static void CorridorWays()
{
	printf("Ways along the corridor\n");
	AlienWaysMap map;
	map.Build(Corridor(10, 1000));
	AlienWaysXYZ const start = { 1000, 0, 500 }, goal = { 1000, 0, 9500 };

	// The floor cheap: straight along the tiles, 9 m, all floor
	Check(SearchAll(map, 0, start, 9, goal, 1) == kAlienWaysFound, "with the floor cheap, a way is found");
	Metres m = MetresOf(map.Way());
	Check(Near(m.floor, 9.0, 0.01) && m.wall == 0.0 && m.ceiling == 0.0, "it's 9 m, all on the floor");
	Check(!map.Way().empty() && At(map.Way().front(), start) && At(map.Way().back(), goal), "it starts at the start and ends at the goal");

	// Floor is lava, at 10. Up the nearest wall: 1 m of floor from the start to the wall at z = 500, 8.5 m of wall to z = 9000 where the last tile
	// meets it, and sqrt(1000 x 1000 + 500 x 500) = 1.118 m of floor to the goal. That costs 10 x 2.118 + 8.5 = 29.7, against 90 along the floor.
	Check(SearchAll(map, 0, start, 9, goal, 10) == kAlienWaysFound, "with floor is lava, a way is found");
	std::vector<AlienWaysPoint> const way = map.Way();
	m = MetresOf(way);
	Check(Near(m.floor, 2.118, 0.01) && Near(m.wall, 8.5, 0.01) && m.ceiling == 0.0, "it goes 1 m to the nearest wall, 8.5 m along it, and 1.1 m back to the goal");
	Check(way.size() == 4 && (way[1].x == 0 || way[1].x == 2000) && way[1].y == 0 && way[1].z == 500 && way[1].kind == kAlienWaysWall,
		"it leaves the floor at the point of a wall nearest the start, not at a corner");
	Check(way.size() == 4 && way[2].x == way[1].x && way[2].y == 0 && way[2].z == 9000 && way[2].kind == kAlienWaysFloor,
		"and comes back onto the floor where that wall meets the goal's tile");

	// Already at 2 the wall wins: 2 x 2.118 + 8.5 = 12.7, against 18 along the floor
	Check(SearchAll(map, 0, start, 9, goal, 2) == kAlienWaysFound && Near(MetresOf(map.Way()).wall, 8.5, 0.01), "at a price of 2 it goes up the wall too");

	// The other way along the corridor gives the same lengths
	Check(SearchAll(map, 9, goal, 0, start, 10) == kAlienWaysFound && Near(MetresOf(map.Way()).floor, 2.118, 0.01), "and the same the other way round");

	// Starting and ending on the same tile is a way of one stretch
	Check(SearchAll(map, 4, { 500, 0, 4200 }, 4, { 1500, 0, 4800 }, 10) == kAlienWaysFound && map.Way().size() == 2, "on one tile, the way is just start to goal");
}

static void OneStepAtATime()
{
	printf("A search spread over many calls\n");
	AlienWaysMap map;
	map.Build(Corridor(10, 1000));
	AlienWaysXYZ const start = { 1000, 0, 500 }, goal = { 1000, 0, 9500 };
	const char *why = nullptr;

	Check(SearchAll(map, 0, start, 9, goal, 10, 1000000, &why) == kAlienWaysFound, "all at once, it's found");
	std::vector<AlienWaysPoint> const once = map.Way();
	int const looksOnce = map.LastSearchLooks();

	Check(SearchAll(map, 0, start, 9, goal, 10, 1, &why) == kAlienWaysFound, "one polygon a call, it's found too");
	bool same = once.size() == map.Way().size();

	for (size_t k = 0; same && k < once.size(); k++)
	{
		same = At(once[k], { map.Way()[k].x, map.Way()[k].y, map.Way()[k].z }) && once[k].kind == map.Way()[k].kind;
	}

	Check(same, "and it's the same way, point for point");
	Check(map.LastSearchLooks() == looksOnce, "having looked at the same number of polygons");
	Check(map.Work(5) == kAlienWaysFound && map.LastSearchLooks() == looksOnce, "asked again after it's ended, it gives the same answer and looks at nothing");
	Check(map.Work(0) == kAlienWaysFound, "even when allowed to look at nothing");
}

static void NoWay()
{
	printf("Surfaces that don't join up\n");
	AlienWaysMap map;
	Check(map.Build({ Square(0, 0, 0, 1000), Square(5000, 0, 0, 1000) }) == nullptr, "two squares 4 m apart build");
	Check(SearchAll(map, 0, { 500, 0, 500 }, 1, { 5500, 0, 500 }, 10) == kAlienWaysNoWay, "there's no way from one to the other");
	Check(map.Way().empty(), "and the way is empty");

	// A square on its own, asked to search with no polygons to stop it, ends at once
	AlienWaysMap one;
	one.Build({ Square(0, 0, 0, 1000) });
	Check(SearchAll(one, 0, { 100, 0, 100 }, 0, { 900, 0, 900 }, 10) == kAlienWaysFound && one.Way().size() == 2, "on a single square, a way is found");
}

static void CutShort()
{
	printf("A way that crosses too many polygons\n");

	// 300 tiles of 100 mm. With the floor cheap, the way goes along every one of them, which is 301 points, more than a way keeps. Going via the
	// wall would be 1 + 29.85 + 1 = 31.9 m, against 29.9 m along the floor, so the floor it is.
	AlienWaysMap map;
	Check(map.Build(Corridor(300, 100)) == nullptr, "a corridor of 300 short tiles builds");
	AlienWaysXYZ const start = { 1000, 0, 50 }, goal = { 1000, 0, 29950 };
	Check(SearchAll(map, 0, start, 299, goal, 1) == kAlienWaysFound, "a way is found");
	Check(map.Way().size() == kAlienWaysMostWayPoints && map.WayCutShort(), "it's cut short at the most points a way keeps, and says so");
	Check(!map.Way().empty() && At(map.Way().front(), start), "what's kept is its start");

	AlienWaysMap shortOne;
	shortOne.Build(Corridor(10, 1000));
	SearchAll(shortOne, 0, { 1000, 0, 500 }, 9, { 1000, 0, 9500 }, 1);
	Check(!shortOne.WayCutShort(), "a way that fits isn't cut short");
}

static void SearchRefusals()
{
	printf("What a search can be asked for\n");
	AlienWaysXYZ const start = { 1000, 0, 500 }, goal = { 1000, 0, 9500 };

	AlienWaysMap none;
	Check(Says(none.StartSearch(0, start, 0, goal, 10), "there's no map to search"), "with no map, a search says why it can't start");

	AlienWaysMap map;
	map.Build(Corridor(10, 1000));
	char const *const noPolygon = "a polygon to search from or to isn't on the map";
	Check(Says(map.StartSearch(-1, start, 9, goal, 10), noPolygon), "from a polygon before the first");
	Check(Says(map.StartSearch(13, start, 9, goal, 10), noPolygon), "from one past the last");
	Check(Says(map.StartSearch(0, start, 13, goal, 10), noPolygon), "to one past the last");
	Check(map.StartSearch(12, start, 0, goal, 10) == nullptr, "the last polygon is fine");

	char const *const badPrice = "the floor's price has to be from 1 to 1000";
	Check(Says(map.StartSearch(0, start, 9, goal, 0), badPrice), "a price of 0");
	Check(Says(map.StartSearch(0, start, 9, goal, -5), badPrice), "a price below 0");
	Check(Says(map.StartSearch(0, start, 9, goal, 1001), badPrice), "a price of 1001");
	Check(map.StartSearch(0, start, 9, goal, 1) == nullptr && map.StartSearch(0, start, 9, goal, 1000) == nullptr, "1 and 1000 are fine");

	char const *const farOut = "a point to search from or to is further out than the map goes";
	Check(Says(map.StartSearch(0, { 64000001, 0, 0 }, 9, goal, 10), farOut), "a start more than 64 km out along x");
	Check(Says(map.StartSearch(0, { 0, -64000001, 0 }, 9, goal, 10), farOut), "along y, below 0");
	Check(Says(map.StartSearch(0, start, 9, { 0, 0, 64000001 }, 10), farOut), "a goal more than 64 km out along z");
	Check(map.StartSearch(0, { 64000000, -64000000, 64000000 }, 9, goal, 10) == nullptr, "exactly 64 km out is fine");
}

static void NumberGoesRound()
{
	printf("When the search number goes all the way round\n");
	AlienWaysMap map;
	map.Build(Corridor(10, 1000));
	AlienWaysXYZ const start = { 1000, 0, 500 }, goal = { 1000, 0, 9500 };

	SearchAll(map, 0, start, 9, goal, 10);
	std::vector<AlienWaysPoint> const first = map.Way();

	// The first search used number 1. If the number went round to 1 again without clearing what it marked, its polygons would look already looked at.
	map.SetSearchNumberForATest(0xFFFFFFFFu);
	Check(SearchAll(map, 0, start, 9, goal, 10) == kAlienWaysFound && map.Way().size() == first.size(), "the search after it goes round finds the same way");
	Check(SearchAll(map, 0, start, 9, goal, 10) == kAlienWaysFound && map.Way().size() == first.size(), "and so does the next");
}

static void AtTheEdgeOfTheWorld()
{
	printf("A way at the edge of the world\n");

	// A wall 128 km long, from x = -63,999 m to +63,999 m, with a 1 m tile at each end touching its bottom edge. The way at a price of 1000 is
	// 0.5 m of floor to the wall, 127,996.5 m of wall, and 0.707 m of floor to the goal: a cost of about 128 million, far from anything that
	// could overflow, and every number on it is at the very edge of what the map takes.
	int const end = 63999000;
	std::vector<AlienWaysPolygon> world;
	world.push_back(Polygon({ { -end, 0, 0 }, { -end, -3000, 0 }, { end, -3000, 0 }, { end, 0, 0 } }, kUpright));
	world.push_back(Square(-end, 0, 0, 1000));
	world.push_back(Square(end - 1000, 0, 0, 1000));

	AlienWaysMap map;
	Check(map.Build(world) == nullptr && map.Polygons() == 3, "it builds, with the long wall kept");
	AlienWaysXYZ const start = { -end + 500, 0, 500 }, goal = { end - 500, 0, 500 };
	Check(SearchAll(map, 1, start, 2, goal, 1000) == kAlienWaysFound, "a way is found from one end to the other at the dearest price");
	Metres const m = MetresOf(map.Way());
	Check(Near(m.wall, 127996.5, 0.01) && Near(m.floor, 1.207, 0.01), "it's 128 km along the wall, and a metre and a bit of floor");
}

// The limits, each at exactly its value, where the map builds, and one short of it, where it's refused with its own words
static void OneLimit(const char *what, size_t AlienWaysLimits::*field, size_t at, std::vector<AlienWaysPolygon> const &world, const char *why)
{
	AlienWaysLimits limits = AlienWays_UsualLimits();
	limits.*field = at;
	AlienWaysMap map;
	map.LowerLimitsForATest(limits);
	Check(map.Build(world) == nullptr, std::string(what) + ": at exactly the most, it builds");

	limits.*field = at - 1;
	map.LowerLimitsForATest(limits);
	const char *const said = map.Build(world);
	Check(Says(said, why), std::string(what) + ": one short, it's refused: \"" + (said ? said : "(nothing)") + "\"");
	Check(!map.Ready() && map.Polygons() == 0 && map.Joins() == 0 && map.BytesKept() == 0, std::string(what) + ": and nothing is kept");
}

// A fan: n triangles all meeting at one corner, (500, 0, 500), which is n corners in one box. Their outer corners are 5 m out, in other boxes.
static std::vector<AlienWaysPolygon> Fan(int n)
{
	std::vector<AlienWaysPolygon> world;

	for (int k = 0; k < n; k++)
	{
		double const a = 6.283185307179586 * k / n, b = 6.283185307179586 * (k + 1) / n;
		AlienWaysXYZ const middle = { 500, 0, 500 };
		AlienWaysXYZ const p = { 500 + static_cast<int>(lround(5000.0 * cos(a))), 0, 500 + static_cast<int>(lround(5000.0 * sin(a))) };
		AlienWaysXYZ const q = { 500 + static_cast<int>(lround(5000.0 * cos(b))), 0, 500 + static_cast<int>(lround(5000.0 * sin(b))) };
		world.push_back(Polygon({ middle, p, q }, kUp));
	}

	return world;
}

static void Limits()
{
	printf("The limits\n");
	AlienWaysMap map;

	// Corners
	char const *const corners = "a polygon has fewer than 3 corners, or more than 8";
	Check(Says(map.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 } }, kUp) }), corners), "a polygon of 2 corners is refused");
	Check(Says(map.Build({ Polygon({ { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 3, 0, 0 }, { 4, 0, 0 }, { 5, 0, 0 }, { 6, 0, 0 }, { 7, 0, 0 }, { 8, 0, 0 } }, kUp) }), corners),
		"a polygon of 9 corners is refused, not cut down to 8");
	Check(map.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 } }, kUp) }) == nullptr, "3 corners is fine");
	Check(map.Build({ Polygon({ { 0, 0, 0 }, { 700, 0, -300 }, { 1000, 0, 0 }, { 1300, 0, 700 }, { 1000, 0, 1400 }, { 300, 0, 1700 }, { 0, 0, 1400 }, { -300, 0, 700 } }, kUp) }) == nullptr,
		"8 corners is fine");

	// Facing
	char const *const facing = "a polygon's facing isn't a number from -1 to 1";
	Check(Says(map.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 } }, std::numeric_limits<float>::quiet_NaN()) }), facing), "a facing that isn't a number is refused");
	Check(Says(map.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 } }, 1.5f) }), facing), "a facing of 1.5 is refused");
	Check(Says(map.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 } }, -1.0001f) }), facing), "and one just below -1");
	Check(map.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 } }, -1.0f), Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 } }, 1.0f) }) == nullptr,
		"-1 and 1 are fine");

	// Scenery, left out without refusing the level
	Check(map.Build({ Square(0, 0, 0, 80000) }) == nullptr && map.PolygonsLeftOut() == 1 && map.Polygons() == 0 && map.MapNumberOf(0) == -1,
		"a floor 80 m square, 6,400 square metres, is left out as scenery, and the level still builds");
	Check(map.Build({ Square(0, 0, 0, 70000) }) == nullptr && map.PolygonsLeftOut() == 0 && map.Polygons() == 1, "one 70 m square, 4,900 square metres, is kept");
	Check(map.Build({ Polygon({ { 0, 0, 0 }, { 0, -50001, 0 }, { 0, -50001, 1000 }, { 0, 0, 1000 } }, kUpright) }) == nullptr && map.PolygonsLeftOut() == 1,
		"a wall a millimetre over 50 m tall is left out");
	Check(map.Build({ Polygon({ { 0, 0, 0 }, { 0, -50000, 0 }, { 0, -50000, 1000 }, { 0, 0, 1000 } }, kUpright) }) == nullptr && map.Polygons() == 1,
		"one exactly 50 m tall is kept");
	Check(map.Build({ Square(64000001 - 1000, 0, 0, 1000) }) == nullptr && map.PolygonsLeftOut() == 1, "a polygon with a corner more than 64 km out is left out");
	Check(map.Build({ Square(64000000 - 1000, 0, 0, 1000) }) == nullptr && map.Polygons() == 1, "one reaching exactly 64 km is kept");
	Check(map.Build({ Square(0, 0, 0, 80000), Square(0, 0, 0, 1000) }) == nullptr && map.MapNumberOf(0) == -1 && map.MapNumberOf(1) == 0,
		"a polygon after one left out takes the next number on the map");

	// Each count and ceiling, at exactly the most a world needs and one short of it. The world's own counts come from building it with the usual limits.
	std::vector<AlienWaysPolygon> const world = Corridor(10, 1000);
	AlienWaysMap usual;
	Check(usual.Build(world) == nullptr && usual.Joins() > 0 && usual.EdgeSteps() > 0 && usual.CornerTests() > 0 && usual.BytesWhileBuilding() > 0,
		"the corridor builds with the usual limits, and counts what it did");
	// The memory counted while building has to include the room made for touches, which grows 1,024 at a time, 20 bytes each (five numbers). Everything
	// else the corridor needs while building comes to about 2 KB, so without them the count would be far under 20,480. This doesn't come from the
	// code being tested, so it catches a count that's been left out.
	Check(usual.BytesWhileBuilding() >= 1024 * 20, "the room for touches is counted while building");

	OneLimit("polygons", &AlienWaysLimits::mostPolygons, 13, world, "the level has more polygons than the map takes");
	OneLimit("edge steps", &AlienWaysLimits::mostEdgeSteps, usual.EdgeSteps(), world, "the polygons' edges are too long to join them up");
	OneLimit("corner tests", &AlienWaysLimits::mostCornerTests, usual.CornerTests(), world, "joining the polygons up would take too long");
	OneLimit("joins", &AlienWaysLimits::mostJoins, usual.Joins(), world, "the polygons touch in more places than the map takes");
	OneLimit("memory kept", &AlienWaysLimits::mostBytesKept, usual.BytesKept(), world, "the map would keep more memory than it may");
	OneLimit("memory while building", &AlienWaysLimits::mostBytesBuilding, usual.BytesWhileBuilding(), world, "building the map would need more memory than it may have");

	// Corners piled into one place: 20 triangles meeting at one corner put 20 corners in its box
	OneLimit("corners in one place", &AlienWaysLimits::mostCornersInOnePlace, 20, Fan(20), "too many corners lie close together in one place");

	// And with the usual limits: 5,000 triangles meeting at one corner, which would take about 25 million tests to join corner by corner, is
	// refused by the count of corners in one place, which comes before any joining
	AlienWaysMap fan;
	Check(Says(fan.Build(Fan(5000)), "too many corners lie close together in one place"), "a fan of 5,000 triangles is refused with the usual limits");
}

static void Stopping()
{
	printf("A build asked to stop\n");
	std::vector<AlienWaysPolygon> const world = Corridor(10, 1000);
	char const *const stopped = "the build was asked to stop";

	// The build looks at the stop once at its start, once in its first look at the polygons, once for each polygon while joining them up (13
	// here), and once for each join it makes. So these stop it in each of those parts.
	for (size_t look : { 1u, 2u, 3u, 10u, 20u, 30u })
	{
		AlienWaysMap map;
		map.StopAtLookForATest(look);
		const char *const said = map.Build(world);
		Check(Says(said, stopped) && !map.Ready() && map.Polygons() == 0 && map.Joins() == 0 && map.BytesKept() == 0,
			"stopped at its look number " + std::to_string(look) + ", it says so and keeps nothing");
	}

	AlienWaysMap map;
	map.AskToStop();
	Check(Says(map.Build(world), stopped), "asked to stop before it starts, it stops at once");
	Check(Says(map.Build(world), stopped), "and stays asked, until it's cleared");
	map.Clear();
	Check(map.Build(world) == nullptr && map.Ready(), "once cleared, it builds");
}

static void BuiltAgain()
{
	printf("Building again\n");
	AlienWaysMap map;
	map.Build(Corridor(10, 1000));
	size_t const joins = map.Joins(), bytes = map.BytesKept();
	Check(map.Build(Corridor(10, 1000)) == nullptr && map.Joins() == joins && map.BytesKept() == bytes, "the same world built again gives the same map");
	Check(Says(map.Build({ Polygon({ { 0, 0, 0 }, { 1, 0, 0 } }, kUp) }), "a polygon has fewer than 3 corners, or more than 8") && !map.Ready() && map.Polygons() == 0,
		"a refused build leaves nothing of the map before it");
	map.Clear();
	Check(!map.Ready() && map.Polygons() == 0, "Clear leaves no map");
}

int main()
{
	Kinds();
	Facing();
	Joins();
	CorridorWays();
	OneStepAtATime();
	NoWay();
	CutShort();
	SearchRefusals();
	NumberGoesRound();
	AtTheEdgeOfTheWorld();
	Limits();
	Stopping();
	BuiltAgain();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all alien ways map checks passed\n");
	return 0;
}
