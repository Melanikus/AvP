// The test for 3dc/avp/alien_ways_guide.h: which aliens follow a way across the alien ways map, what each one's way is, and when an alien stops
// being guided, so the game gives it its usual gravity back ("floor is lava", issue #109). Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game: the aliens are
// made up, as numbers. The last part reads the game's own files, for the joins into the game that only a running game would reach.
//
// The world is the corridor of the map's own test (alien_ways_map_test.cpp), seen here from above:
//
//      x = 0  left wall, polygon 10                       x = 2000  right wall, polygon 11, and the ceiling, 12, 3 m up
//        |  tile 0  |  tile 1  | ... |  tile 9  |         floor tiles 0 to 9, each 2 m across (x) and 1 m long (z)
//        z = 0                         z = 10000
//
// From (1000, 0, 500) on tile 0 to (1000, 0, 9500) on tile 9, with the floor at 10 times the price, the way is 6 points: the start, then
// 1 m of floor to a wall's foot at z = 500, 1.5 m straight up it, 8.5 m along it at that height to z = 9000, where tile 9 meets it, 1.5 m
// down to its foot, then 1.118 m of floor to the goal. The two walls cost the same, so it's the one the search meets first; which one is
// worked out below, from the way itself. The map's own test works the sums out. Coordinates are the game's: millimetres, and y goes down.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "alien_ways_guide.h"

// One guide is about 280 KB: room for 64 aliens' ways of 256 points each. So every guide here is made with new, never as a function's own
// local: those go on the stack, which is 1 MB, and one test function here has six. Several guides on the
// stack overflowed it (the sanitizer job, 2026-11-08). The game keeps its one guide as a global (alien_ways.cpp), never on the stack.
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

static bool Same(AlienWaysXYZ a, AlienWaysXYZ b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

float const kUp = 1.0f, kUpright = 0.0f, kDown = -1.0f;

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

// Ten floor tiles of 1 m along z, and, if walls is true, the two walls and the ceiling
static std::vector<AlienWaysPolygon> Corridor(bool walls)
{
	std::vector<AlienWaysPolygon> world;

	for (int k = 0; k < 10; k++)
	{
		world.push_back(Polygon({ { 0, 0, k * 1000 }, { 2000, 0, k * 1000 }, { 2000, 0, (k + 1) * 1000 }, { 0, 0, (k + 1) * 1000 } }, kUp));
	}

	if (walls)
	{
		world.push_back(Polygon({ { 0, 0, 0 }, { 0, -3000, 0 }, { 0, -3000, 10000 }, { 0, 0, 10000 } }, kUpright));
		world.push_back(Polygon({ { 2000, 0, 0 }, { 2000, -3000, 0 }, { 2000, -3000, 10000 }, { 2000, 0, 10000 } }, kUpright));
		world.push_back(Polygon({ { 0, -3000, 0 }, { 2000, -3000, 0 }, { 2000, -3000, 10000 }, { 0, -3000, 10000 } }, kDown));
	}

	return world;
}

AlienWaysXYZ const kStart = { 1000, 0, 500 }, kGoal = { 1000, 0, 9500 };
// The way's points on the wall, filled in by CorridorMap from the way, as the wall it takes is either one
AlienWaysXYZ kOntoTheWall = { 0, 0, 500 }, kUpTheWall = { 0, -1500, 500 }, kAlongTheWall = { 0, -1500, 9000 }, kOffTheWall = { 0, 0, 9000 };
int kWallX = 0;          // the wall's x: 0 or 2000
int kAwayFromWall = 1;   // which way is away from that wall, into the corridor: +1 for x = 0, -1 for x = 2000

// A point beside the wall the way takes: so far into the corridor from it (along x), so high, and so far along it
static AlienWaysXYZ Beside(int out, int y, int z)
{
	return AlienWaysXYZ{ kWallX + kAwayFromWall * out, y, z };
}

// The down of an alien clinging to that wall: into it
static AlienWaysXYZ IntoTheWall()
{
	return AlienWaysXYZ{ -kAwayFromWall * 65536, 0, 0 };
}
AlienWaysXYZ const kUpright65536 = { 0, 65536, 0 };   // an alien standing on the floor: down is +y

// An alien, by a made-up place and name
static AlienWaysWho Alien(int n)
{
	AlienWaysWho who;
	who.place = 0x10000u + static_cast<uint64_t>(n);
	who.name = 0x4E41u + static_cast<uint64_t>(n);
	return who;
}

// What an alien says about itself: where it is, and where it's going. Its polygons are looked for among all the world's.
static AlienWaysAsk Ask(const AlienWaysWho &who, AlienWaysXYZ at, AlienWaysXYZ goal, size_t polygons)
{
	AlienWaysAsk ask;
	ask.who = who;
	ask.at = at;
	ask.down = kUpright65536;
	ask.atPolygons = AlienWaysRange{ 0, polygons };
	ask.goal = goal;
	ask.goalPolygons[0] = ask.atPolygons;
	ask.goalPolygons[1] = AlienWaysRange{ 0, 0 };
	ask.goalIsTheTarget = false;
	ask.hasTarget = false;
	ask.target = AlienWaysWho{ 0, 0 };
	return ask;
}

// The same, coming for a target, as a hunting alien does: the aliens coming for one target are counted for the flankers
static AlienWaysAsk AskFor(const AlienWaysWho &who, const AlienWaysWho &target)
{
	AlienWaysAsk ask = Ask(who, kStart, kGoal, 13);
	ask.hasTarget = true;
	ask.target = target;
	return ask;
}

// One frame of the game: the guide's turn, with the usual allowance, and how many were let go
static size_t Frame(AlienWaysGuide &guide, AlienWaysMap &map, int ms, AlienWaysWho *lapsed = nullptr, int looks = kAlienWaysLooksPerFrame)
{
	AlienWaysWho room[kAlienWaysMostFollowers];
	return guide.Frame(&map, ms, looks, lapsed ? lapsed : room, kAlienWaysMostFollowers);
}

static size_t ReportsOf(const AlienWaysGuide &guide, const AlienWaysReport **reports)
{
	return guide.Reports(reports);
}

static AlienWaysMap &CorridorMap()
{
	static AlienWaysMap map;

	if (!map.Ready())
	{
		map.Build(Corridor(true));

		// Which wall the way takes, from the way itself
		AlienWaysXYZ const start = { 1000, 0, 500 }, goal = { 1000, 0, 9500 };

		if (map.StartSearch(0, start, 9, goal, kAlienWaysFloorPrice) == nullptr && map.Work(1000000) == kAlienWaysFound && map.Way().size() == 6)
		{
			kWallX = map.Way()[1].x;
			kAwayFromWall = kWallX == 0 ? 1 : -1;
			kOntoTheWall = AlienWaysXYZ{ kWallX, 0, 500 };
			kUpTheWall = AlienWaysXYZ{ kWallX, -1500, 500 };
			kAlongTheWall = AlienWaysXYZ{ kWallX, -1500, 9000 };
			kOffTheWall = AlienWaysXYZ{ kWallX, 0, 9000 };
		}
	}

	return map;
}

// An alien asks, the next frame finds its way, and it's steered towards the way's next point
static AlienWaysGuide &GuidedAlien(AlienWaysGuide &guide, const AlienWaysWho &who)
{
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	guide.Steer(&map, Ask(who, kStart, kGoal, 13), &towards);
	Frame(guide, map, 16);
	return guide;
}

static void FirstWay()
{
	printf("An alien's first way\n");
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	AlienWaysMap notBuilt;
	AlienWaysXYZ towards = { 7, 7, 7 };

	Check(!guide.Steer(&notBuilt, Ask(Alien(1), kStart, kGoal, 13), &towards) && !guide.Steer(nullptr, Ask(Alien(1), kStart, kGoal, 13), &towards),
		"with no map, an alien isn't guided");
	Check(Frame(guide, notBuilt, 16) == 0, "and a frame with no map lets nothing go and searches for nothing");

	AlienWaysMap &map = CorridorMap();
	Check(map.Ready(), "the corridor's map is built");
	Check(!guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "an alien's first ask has no way yet: the game's own code steers it this frame");
	Check(!guide.Guided(Alien(1)), "so it isn't guided");
	size_t notYetSlots[4];
	AlienWaysXYZ notYetPlaces[4];
	int notYetKinds[4];
	Check(guide.GuidedPlaces(notYetSlots, notYetPlaces, notYetKinds, 4) == 0, "nor given a place in the once-a-second line, which is for guided aliens only");

	Check(Frame(guide, map, 16) == 0, "the next frame lets nothing go");
	const AlienWaysReport *reports = nullptr;
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound, "and its search ends in that frame with a way, in one report");
	Check(reports[0].points == 6 && Near(reports[0].floorMm, 2118.0, 1.0) && Near(reports[0].wallMm, 11500.0, 0.5) && reports[0].ceilingMm == 0.0,
		"the report says 6 points: 2.118 m of floor, 11.5 m of wall up, along and down, and no ceiling");
	Check(kWallX == 0 || kWallX == 2000, "the way takes one of the corridor's two walls");
	Check(reports[0].frames == 1 && reports[0].looks > 0 && reports[0].waitedMs == 16 && Same(reports[0].from, kStart) && Same(reports[0].to, kGoal),
		"one frame, some polygons looked at, 16 ms after it asked, from its place to its goal");
	Check(!reports[0].cutShort && reports[0].follower == 0, "not cut short, and the first of the guide's places");
	Check(reports[0].polygonCount == 3 && reports[0].polygons[0] == 0 && reports[0].polygons[1] == (kWallX == 0 ? 10 : 11) && reports[0].polygons[2] == 9,
		"and the polygons its way is on, each once: tile 0, the wall it climbs, and tile 9 (for measuring how ways stack)");

	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && Same(towards, kOntoTheWall), "now it's steered: towards the wall's foot");
	Check(guide.Guided(Alien(1)), "and it's guided");

	size_t slots[4];
	AlienWaysXYZ places[4];
	int kinds[4];
	Check(guide.GuidedPlaces(slots, places, kinds, 4) == 1 && slots[0] == 0 && Same(places[0], kStart) && kinds[0] == kAlienWaysFloor,
		"its place is given for the once-a-second line: the guide's first place, where it is, on the floor");
	Check(guide.GuidedPlaces(slots, places, kinds, 0) == 0, "with no room, no place is given");

	// Which alien has a place, for the log to name it by its id (added 2026-11-11 by Melanikus for the AvP port, #109)
	AlienWaysWho atFirst = { 0, 0 };
	AlienWaysWho untouched = { 7, 7 };
	Check(guide.WhoAt(0, &atFirst) && atFirst.place == Alien(1).place && atFirst.name == Alien(1).name
		&& !guide.WhoAt(1, &untouched) && !guide.WhoAt(kAlienWaysMostFollowers, &untouched) && untouched.place == 7 && untouched.name == 7,
		"the guide says which alien has its first place, and nothing, touching nothing, for a place no alien has or that isn't one");

	const AlienWaysPoint *points[4];
	int counts[4], headings[4];
	Check(guide.Shown(points, counts, headings, 4) == 1 && counts[0] == 6 && headings[0] == 1, "the view is shown its way: 6 points, heading for the second");
	Check(guide.Shown(points, counts, headings, 0) == 0, "with no room, the view is shown nothing");

	// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109): the log reads the same way by the alien's place, as places; and a way its
	// alien is following isn't a kept one as well
	const AlienWaysPoint *wayPoints = nullptr;
	Check(guide.WayOf(0, &wayPoints) == 6 && wayPoints == points[0], "the log reads the same way by its place: 6 points");
	Check(guide.WayOf(1, &wayPoints) == 0 && wayPoints == nullptr && guide.WayOf(kAlienWaysMostFollowers, &wayPoints) == 0,
		"and nothing for a place with no alien, or past the last");
	const AlienWaysPoint *keptPoints[4];
	int keptCounts[4];
	Check(guide.Kept(keptPoints, keptCounts, 4) == 0, "a way its alien is following isn't also a kept one");
}

static void Reaching()
{
	printf("Reaching the way's points\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;

	// The wall's foot, (0, 0, 500), is where the way steps up onto the wall. Near it, an alien still on the floor hasn't made that step: it
	// keeps heading for the foot, so it runs into the wall and climbs. Once it's on the wall (its down is -x), the step is made.
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	Check(guide.Steer(&map, Ask(Alien(1), Beside(300, -500, 500), kGoal, 13), &towards) && Same(towards, kOntoTheWall),
		"near the foot of the wall but still on the floor, the step onto the wall isn't made: it heads for the foot");
	AlienWaysAsk upTheWall = Ask(Alien(1), Beside(300, -500, 500), kGoal, 13);
	upTheWall.down = IntoTheWall();
	Check(guide.Steer(&map, upTheWall, &towards) && Same(towards, kUpTheWall), "on the wall, 300 mm off it and 500 mm up, it is: the next point is 1.5 m up the wall");

	// Where the way comes off the wall, 650 mm across from it isn't reached; 550 mm is, once the alien is on the floor.
	const std::unique_ptr<AlienWaysGuide> acrossRoom(new AlienWaysGuide);
	AlienWaysGuide &across = *acrossRoom;
	GuidedAlien(across, Alien(1));
	AlienWaysAsk alongTheWall = Ask(Alien(1), Beside(0, 0, 8350), kGoal, 13);
	alongTheWall.down = IntoTheWall();
	Check(across.Steer(&map, alongTheWall, &towards) && Same(towards, kOffTheWall), "650 mm across isn't reached yet");
	Check(across.Steer(&map, Ask(Alien(1), Beside(0, 0, 8450), kGoal, 13), &towards) && Same(towards, kGoal), "550 mm across, on the floor, is");

	// Coming off a wall is a step too. Near where the way comes off it, an alien still on the wall keeps heading for the wall's foot, so it
	// comes down the wall to it; on the floor, it heads for the goal. (The film on Genshd1: an alien on a leaning wall, heading straight out
	// from it for a goal 3 m away, had nothing of that direction left along the wall, and was found stuck.)
	const std::unique_ptr<AlienWaysGuide> stepDownRoom(new AlienWaysGuide);
	AlienWaysGuide &stepDown = *stepDownRoom;
	GuidedAlien(stepDown, Alien(1));
	AlienWaysAsk stillOnTheWall = Ask(Alien(1), Beside(0, -100, 8900), kGoal, 13);
	stillOnTheWall.down = IntoTheWall();
	Check(stepDown.Steer(&map, stillOnTheWall, &towards) && Same(towards, kOffTheWall), "still on the wall, near its foot, it heads for the foot, not the goal");
	Check(stepDown.Steer(&map, Ask(Alien(1), Beside(0, -100, 8900), kGoal, 13), &towards) && Same(towards, kGoal), "on the floor there, it heads for the goal");

	// Up from the surface it's on, an alien may be as far as 1.5 m from a point on it. The goal is on the floor of tile 9: first the alien is
	// put on the way's last stretch, near the goal.
	const std::unique_ptr<AlienWaysGuide> highRoom(new AlienWaysGuide);
	AlienWaysGuide &high = *highRoom;
	GuidedAlien(high, Alien(1));
	Check(high.Steer(&map, Ask(Alien(1), Beside(300, 0, 9150), kGoal, 13), &towards) && Same(towards, kGoal), "on the last stretch, it heads for the goal");
	Check(high.Steer(&map, Ask(Alien(1), { 1000, -1600, 9500 }, kGoal, 13), &towards) && Same(towards, kGoal), "1.6 m above the goal isn't reaching it");
	Check(!high.Steer(&map, Ask(Alien(1), { 1000, -1400, 9500 }, kGoal, 13), &towards), "1.4 m above it is, and the way is used up");

	// An alien clinging to the left wall has the wall as its down (-x). 900 mm out from the wall is then its height off the surface, which is
	// allowed, not 900 mm across it, which wouldn't be.
	const std::unique_ptr<AlienWaysGuide> onTheWallRoom(new AlienWaysGuide);
	AlienWaysGuide &onTheWall = *onTheWallRoom;
	GuidedAlien(onTheWall, Alien(1));
	AlienWaysAsk climbing = Ask(Alien(1), Beside(900, 0, 500), kGoal, 13);
	climbing.down = IntoTheWall();
	Check(onTheWall.Steer(&map, climbing, &towards) && Same(towards, kUpTheWall), "with the wall as its down, 900 mm out from the wall's foot is reaching it");
	const std::unique_ptr<AlienWaysGuide> onTheFloorRoom(new AlienWaysGuide);
	AlienWaysGuide &onTheFloor = *onTheFloorRoom;
	GuidedAlien(onTheFloor, Alien(1));
	Check(onTheFloor.Steer(&map, Ask(Alien(1), Beside(900, 0, 500), kGoal, 13), &towards) && Same(towards, kOntoTheWall),
		"with the floor as its down, the same spot is 900 mm across, and isn't");

	// The last point reached means the way is used up, and it asks again
	// Along the way, up the wall and back down: at the goal, it's used up
	const std::unique_ptr<AlienWaysGuide> endRoom(new AlienWaysGuide);
	AlienWaysGuide &end = *endRoom;
	GuidedAlien(end, Alien(1));
	AlienWaysAsk alongHigh = Ask(Alien(1), Beside(0, -1500, 600), kGoal, 13);
	alongHigh.down = IntoTheWall();
	bool const went = end.Steer(&map, alongHigh, &towards) && Same(towards, kAlongTheWall);
	alongHigh.at = Beside(0, -1500, 8900);
	bool const cameDown = end.Steer(&map, alongHigh, &towards) && Same(towards, kOffTheWall);
	bool const toTheGoal = end.Steer(&map, Ask(Alien(1), Beside(0, -100, 9000), kGoal, 13), &towards) && Same(towards, kGoal);
	Check(went && cameDown && toTheGoal, "it goes along the wall at head height, comes down where it leaves, and heads for the goal");
	Check(!end.Steer(&map, Ask(Alien(1), { 1000, 0, 9400 }, kGoal, 13), &towards) && !end.Guided(Alien(1)), "at the goal, its way is used up, and it's no longer guided");
}

static void TakenUpWhereItIs()
{
	printf("A new way, taken up where the alien is now\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;

	// The way was searched from the start, but by the time it comes the alien is halfway along the wall, at head height, clinging to it. It's
	// more than 4.5 m from the way's first stretch, which would be "off the way", but on the stretch along the wall, so it takes the way up there.
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	AlienWaysAsk halfway = Ask(Alien(1), Beside(0, -1500, 5000), kGoal, 13);
	halfway.down = IntoTheWall();
	Check(guide.Steer(&map, halfway, &towards) && Same(towards, kAlongTheWall),
		"an alien that's gone on while its way was searched takes it up at the stretch it's on, and isn't off the way");
}

// The aim (alien_ways_lookahead.h): with the setting at 1.5 m, an alien halfway along the wall aims 1.5 m on from its nearest place on the
// way, not at the way's next point 4 m away; at 0 it aims at the next point, as before
static void AimAhead()
{
	printf("Where an alien on its way aims\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	AlienWaysAsk halfway = Ask(Alien(1), Beside(0, -1500, 5000), kGoal, 13);
	halfway.down = IntoTheWall();
	guide.SetAimAhead(kAlienWaysAimAheadMm);
	Check(guide.Steer(&map, halfway, &towards) && Same(towards, AlienWaysXYZ{ kWallX, -1500, 6500 }),
		"aiming 1.5 m ahead, an alien halfway along the wall aims 1.5 m further along it");
	guide.SetAimAhead(0);
	Check(guide.Steer(&map, halfway, &towards) && Same(towards, kAlongTheWall), "aiming 0 ahead, it aims at the way's next point, as before");
}

static void OffTheWay()
{
	printf("Off the way\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);

	// 3.5 m from the stretch it's on, from the start to the wall's foot
	Check(!guide.Steer(&map, Ask(Alien(1), { 1000, 0, 4000 }, kGoal, 13), &towards) && !guide.Guided(Alien(1)), "more than 3 m off its way, an alien has lost it");
	Frame(guide, map, 16);
	const AlienWaysReport *reports = nullptr;
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound && Same(reports[0].from, { 1000, 0, 4000 }),
		"and asks again, from where it is");
	Check(guide.Steer(&map, Ask(Alien(1), { 1000, 0, 4000 }, kGoal, 13), &towards) && guide.Guided(Alien(1)), "and is guided again on the new way");
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109), for THE CLOCK and THE WATCHDOG (alien_ways_guide.h; PO's ruling R-LAVA-P3).
// The corridor's two walls: the one its way takes (kWallX), and the other, which a way takes when that one is shut.
static int TheWall()
{
	return kWallX == 0 ? 10 : 11;
}

static int OtherWall()
{
	return kWallX == 0 ? 11 : 10;
}

// Whether a line is of a way found by that wall: tile 0, the wall, tile 9
static bool ByTheWall(const AlienWaysReport &line, int wall)
{
	return line.outcome == kAlienWaysOutcomeFound && line.polygonCount == 3 && line.polygons[0] == 0 && line.polygons[1] == wall && line.polygons[2] == 9;
}

// The down of an alien clinging to what a point of a way in the corridor is on
static AlienWaysXYZ DownOn(const AlienWaysPoint &point)
{
	switch (point.kind)
	{
		case kAlienWaysWall: return AlienWaysXYZ{ point.x <= 1000 ? -65536 : 65536, 0, 0 };
		case kAlienWaysCeiling: return AlienWaysXYZ{ 0, -65536, 0 };
		default: return kUpright65536;
	}
}

// An alien held where ask says, a frame of 100 ms at a time, for up to most ms, until it's found stuck. Says whether it was, how many ms of
// holding that took, its stuck line, whether the mover was to leave it alone just after (RestingAfterStuck, before the next frame), and
// the line after the stuck one in that same frame, if there was one: a re-plan's search, when it was asked for at once.
struct Held
{
	bool stuck;
	int took;
	AlienWaysReport line;
	bool resting;
	bool hasNext;
	AlienWaysReport next;
	AlienWaysAsk ask;
};

static Held HeldTillStuck(AlienWaysGuide &guide, AlienWaysMap &map, const AlienWaysAsk &ask, int most)
{
	Held held = {};
	held.ask = ask;
	AlienWaysXYZ towards;

	for (int ms = 0; ms <= most && !held.stuck; ms += 100)
	{
		guide.Steer(&map, ask, &towards);
		bool const resting = guide.RestingAfterStuck(ask.who);
		Frame(guide, map, 100);
		const AlienWaysReport *reports = nullptr;
		size_t const count = ReportsOf(guide, &reports);

		for (size_t r = 0; r < count && !held.stuck; r++)
		{
			if (reports[r].outcome == kAlienWaysOutcomeStuck && reports[r].who.place == ask.who.place)
			{
				held.stuck = true;
				held.took = ms;
				held.line = reports[r];
				held.resting = resting;
				held.hasNext = r + 1 < count;
				held.next = held.hasNext ? reports[r + 1] : AlienWaysReport();
			}
		}
	}

	return held;
}

// An alien put at the second point of the way it has now, clinging to what that point is on, so it has reached a point of its way, then
// held there till it's stuck. Nothing is held, and nothing found stuck, if it has no way of 3 points or more.
static Held HeldAtItsSecondPoint(AlienWaysGuide &guide, AlienWaysMap &map, const AlienWaysWho &who)
{
	int const place = guide.PlaceOf(who);
	const AlienWaysPoint *points = nullptr;

	if (place < 0 || guide.WayOf(static_cast<size_t>(place), &points) < 3 || !points)
	{
		Held none = {};
		return none;
	}

	AlienWaysAsk there = Ask(who, AlienWaysXYZ{ points[1].x, points[1].y, points[1].z }, kGoal, 13);
	there.down = DownOn(points[1]);
	return HeldTillStuck(guide, map, there, 3000);
}

static void Stuck()
{
	printf("No headway\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));

	// It stays where it is, 1 m from the wall's foot, frame after frame: 100 ms a frame. Being found stuck is a report of its own.
	bool steered = true;
	int ms = 0;

	for (; ms < 1400; ms += 100)
	{
		steered = steered && guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
		Frame(guide, map, 100);
	}

	Check(steered, "an alien that makes no headway is still guided for 1.4 s");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "and at 1.4 s");
	Frame(guide, map, 100);
	Check(!guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "at 1.5 s with no headway it's stuck: the game's own code has it");

	// For 3 s it isn't guided and asks for nothing. The one report in that time says it was found stuck, and where it was heading.
	bool asked = false, guided = false;
	int stuckReports = 0;
	bool stuckSaysWhere = false;
	AlienWaysReport stuckLine = {};

	for (ms = 0; ms < 2900; ms += 100)
	{
		Frame(guide, map, 100);
		const AlienWaysReport *reports = nullptr;
		size_t const count = ReportsOf(guide, &reports);

		for (size_t r = 0; r < count; r++)
		{
			if (reports[r].outcome == kAlienWaysOutcomeStuck)
			{
				stuckReports++;
				stuckSaysWhere = Same(reports[r].from, kStart) && Same(reports[r].to, kOntoTheWall) && reports[r].who.place == Alien(1).place && reports[r].who.name == Alien(1).name;
				stuckLine = reports[r];
			}
			else
			{
				asked = true;
			}
		}

		guided = guided || guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	}

	Check(!asked && !guided, "for the next 3 s it's left to the game's own code, and asks for no way");

	// Its way is kept for the view meanwhile, dimmed, so the line doesn't vanish when it's stuck (the owner, 2026-11-11; added the same day
	// by Melanikus (Third) for the AvP port, #109)
	{
		const AlienWaysPoint *keptPoints[4];
		int keptCounts[4];
		const AlienWaysPoint *shownPoints[4];
		int shownCounts[4], shownHeadings[4];
		Check(guide.Kept(keptPoints, keptCounts, 4) == 1 && keptCounts[0] == 6 && guide.Shown(shownPoints, shownCounts, shownHeadings, 4) == 0,
			"while it's stuck, its last way of 6 points is kept for the view, and none is shown as followed");
		Check(guide.Kept(keptPoints, keptCounts, 0) == 0, "with no room, nothing is kept for the view");
	}
	Check(stuckReports == 1 && stuckSaysWhere, "being stuck is reported once, with who it is, where it was and where it was heading");

	// THE WATCHDOG (added 2026-11-11 by Melanikus (Third) for the AvP port, #109): held at its start, it never reached a point of its way,
	// so it's stuck where its way starts. A search from the same spot would start on the same polygon, so it rests first, as before, and
	// the wall it was heading for is shut for it.
	Check(stuckLine.afterStuck == kAlienWaysRestsAtTheStart && stuckLine.replan == 0 && stuckLine.shutPolygon == TheWall(),
		"stuck before reaching any point of its way, it rests first, uses no re-plan, and the wall it was heading for is shut for it");
	Frame(guide, map, 100);
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(guide, map, 16);
	const AlienWaysReport *reports = nullptr;
	Check(ReportsOf(guide, &reports) == 1 && guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "then it asks again, and is guided again");
	Check(ByTheWall(reports[0], OtherWall()) && !reports[0].afterReplan,
		"by the other wall, as the one it was stuck heading for is shut for it; and its line doesn't call it a re-plan");
}

// THE CLOCK (alien_ways_guide.h). Added 2026-11-11 by Melanikus (Third) for the AvP port (#109). In the owner's own play an alien got a new
// way every half second as the running marine moved its goal, made no headway, and was never found stuck while he ran.
static void TheClock()
{
	printf("The clock: new ways don't hide an alien that's stuck\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysXYZ const nearer = { 1000, 0, 5500 };   // 4 m from kGoal: a goal that moves between the two asks for a new way each time

	// Held at the start while its goal moves 4 m every 400 ms: a new way every 400 ms, and still found stuck 1.5 s after the first
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	int firstWayAt = -1, stuckAt = -1, ways = 0;

	for (int ms = 0; ms < 4000 && stuckAt < 0; ms += 100)
	{
		guide.Steer(&map, Ask(Alien(1), kStart, (ms / 400) % 2 == 0 ? kGoal : nearer, 13), &towards);
		Frame(guide, map, 100);
		const AlienWaysReport *reports = nullptr;
		size_t const count = ReportsOf(guide, &reports);

		for (size_t r = 0; r < count; r++)
		{
			if (reports[r].outcome == kAlienWaysOutcomeFound)
			{
				ways++;
				firstWayAt = firstWayAt < 0 ? ms : firstWayAt;
			}
			else if (reports[r].outcome == kAlienWaysOutcomeStuck && stuckAt < 0)
			{
				stuckAt = ms;
			}
		}
	}

	Check(ways >= 4, "its goal moving 4 m every 400 ms, an alien held at the start gets a new way each time");
	Check(firstWayAt >= 0 && stuckAt >= firstWayAt + 1400 && stuckAt <= firstWayAt + 1700,
		"and making no headway, it's found stuck 1.5 s after its first way all the same: a new way doesn't start the clock again");

	// One that has moved 1 m since it last made headway (here further from its next point, so it made none) starts the clock again with
	// its new way: it's found stuck 1.5 s after that way, not after its first
	const std::unique_ptr<AlienWaysGuide> movedRoom(new AlienWaysGuide);
	AlienWaysGuide &moved = *movedRoom;
	AlienWaysXYZ const along = { 1000, 0, 1700 };   // 1.2 m on from the start, and further from the wall's foot it was heading for
	int newWayAt = -1;
	firstWayAt = stuckAt = -1;

	for (int ms = 0; ms < 4000 && stuckAt < 0; ms += 100)
	{
		bool const hasMoved = ms >= 1000;
		moved.Steer(&map, Ask(Alien(1), hasMoved ? along : kStart, hasMoved ? nearer : kGoal, 13), &towards);
		Frame(moved, map, 100);
		const AlienWaysReport *reports = nullptr;
		size_t const count = ReportsOf(moved, &reports);

		for (size_t r = 0; r < count; r++)
		{
			if (reports[r].outcome == kAlienWaysOutcomeFound && firstWayAt < 0)
			{
				firstWayAt = ms;
			}
			else if (reports[r].outcome == kAlienWaysOutcomeFound && hasMoved && newWayAt < 0)
			{
				newWayAt = ms;
			}
			else if (reports[r].outcome == kAlienWaysOutcomeStuck && stuckAt < 0)
			{
				stuckAt = ms;
			}
		}
	}

	Check(firstWayAt == 0 && newWayAt == 1000, "an alien that has moved on 1.2 m, its goal 4 m on too, gets a new way");
	Check(stuckAt >= newWayAt + 1400 && stuckAt <= newWayAt + 1700, "and the clock starts again with it: it's found stuck 1.5 s after that way");
}

// THE WATCHDOG (alien_ways_guide.h). Added 2026-11-11 by Melanikus (Third) for the AvP port (#109; PO's ruling R-LAVA-P3).
// Stuck after it has reached a point of its way, an alien asks for a new way at once, with the polygon it couldn't reach shut for it; and
// THE CAP: at most three re-plans in a minute, then it rests, as before the watchdog.
static void ReplansAtOnce()
{
	printf("The watchdog: stuck along its way, an alien re-plans at once, three times a minute at most\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));

	// It takes its way up at the start, then steps onto the wall, so it has reached the way's second point, and stays there, 500 mm up,
	// heading for the point 1.5 m up the wall
	AlienWaysAsk onTheWall = Ask(Alien(1), Beside(300, -500, 500), kGoal, 13);
	onTheWall.down = IntoTheWall();
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Check(guide.Steer(&map, onTheWall, &towards) && Same(towards, kUpTheWall), "on the wall, it heads up it");
	Held const held = HeldTillStuck(guide, map, onTheWall, 3000);
	Check(held.stuck && held.took >= 1400 && held.took <= 1600, "held there, it's found stuck after 1.5 s");
	Check(held.line.afterStuck == kAlienWaysReplans && held.line.replan == 1 && held.line.shutPolygon == TheWall(),
		"it re-plans at once, its first re-plan this minute, and the wall it was heading up is shut for it");
	Check(held.resting, "until its new way comes, the mover leaves it to the game's own code, as in a rest");
	Check(held.hasNext && held.next.outcome == kAlienWaysOutcomeFound && held.next.afterReplan && Same(held.next.from, Beside(300, -500, 500)),
		"in that same frame its new way is found, from where it is, and its line says it came of a re-plan");
	Check(held.next.polygonCount >= 1 && held.next.polygons[0] == TheWall(),
		"the polygon it's on is never shut for its own search: the new way starts on that wall");
	Check(!guide.RestingAfterStuck(Alien(1)) && guide.Steer(&map, onTheWall, &towards), "with its new way, it's guided again");

	// Each time it's put at its new way's second point and held there: stuck after reaching a point, so the 2nd and 3rd re-plans, then
	// the 4th time in that minute it rests
	int replans[3] = { -1, -1, -1 };
	AlienWaysAfterStuck after[3] = { kAlienWaysRestsAtTheStart, kAlienWaysRestsAtTheStart, kAlienWaysRestsAtTheStart };
	Held last = {};

	for (int k = 0; k < 3; k++)
	{
		last = HeldAtItsSecondPoint(guide, map, Alien(1));
		replans[k] = last.stuck ? last.line.replan : -1;
		after[k] = last.line.afterStuck;
	}

	Check(replans[0] == 2 && replans[1] == 3 && after[0] == kAlienWaysReplans && after[1] == kAlienWaysReplans,
		"stuck twice more along its ways, it re-plans twice more: its 2nd and 3rd in this minute");
	Check(last.stuck && after[2] == kAlienWaysRestsCapped && replans[2] == 0 && last.resting && !last.hasNext,
		"the 4th time in that minute its re-plans are used up: it rests, as before the watchdog, and asks for nothing");

	// A minute on, its re-plans count no more. It's held at the same spot meanwhile, asked about every frame of 400 ms, so it's kept.
	for (int ms = 0; ms < 61000; ms += 400)
	{
		guide.Steer(&map, last.ask, &towards);
		Frame(guide, map, 400);
	}

	bool guided = false;

	for (int ms = 0; ms < 6000 && !guided; ms += 100)
	{
		guided = guide.Steer(&map, last.ask, &towards);
		Frame(guide, map, 100);
	}

	Held const later = HeldAtItsSecondPoint(guide, map, Alien(1));
	Check(guided && later.stuck && later.line.afterStuck == kAlienWaysReplans && later.line.replan == 1,
		"a minute later, stuck along its way, it re-plans again: the first of a new minute");
}

// A polygon is shut for 20 s. An alien held at the start is stuck there, heading for the wall's foot, so that wall is shut for it; then
// by the other wall the same, so both are. With both shut its ways are all floor, which the game's own code walks as well, so it rests.
// Once the first wall's 20 s are up, its way goes by that wall again.
static void ShutForTwentySeconds()
{
	printf("The watchdog: a polygon is shut for an alien for 20 s\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	std::vector<int> times;
	std::vector<AlienWaysReport> lines;

	for (int ms = 0; ms < 30000; ms += 100)
	{
		guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
		Frame(guide, map, 100);
		const AlienWaysReport *reports = nullptr;
		size_t const count = ReportsOf(guide, &reports);

		for (size_t r = 0; r < count; r++)
		{
			times.push_back(ms);
			lines.push_back(reports[r]);
		}
	}

	// In order: its first stuck, the way after it, its second stuck, then the first way by the first wall again
	size_t const none = lines.size();
	size_t firstStuck = none, byTheOther = none, secondStuck = none, byTheFirstAgain = none;
	bool allFloorMeanwhile = false;

	for (size_t i = 0; i < lines.size(); i++)
	{
		bool const stuck = lines[i].outcome == kAlienWaysOutcomeStuck;

		if (firstStuck == none)
		{
			firstStuck = stuck ? i : none;
		}
		else if (byTheOther == none)
		{
			byTheOther = lines[i].outcome == kAlienWaysOutcomeFound ? i : none;
		}
		else if (secondStuck == none)
		{
			secondStuck = stuck ? i : none;
		}
		else if (byTheFirstAgain == none)
		{
			allFloorMeanwhile = allFloorMeanwhile || lines[i].outcome == kAlienWaysOutcomeAllFloor;
			byTheFirstAgain = ByTheWall(lines[i], TheWall()) ? i : none;
		}
	}

	Check(firstStuck != none && lines[firstStuck].afterStuck == kAlienWaysRestsAtTheStart && lines[firstStuck].shutPolygon == TheWall(),
		"held at the start, it's stuck there: it rests, and the wall it was heading for is shut for it");
	Check(byTheOther != none && ByTheWall(lines[byTheOther], OtherWall()), "after its rest its way goes by the other wall");
	Check(secondStuck != none && lines[secondStuck].shutPolygon == OtherWall(), "stuck at the start again, that wall is shut for it too");
	Check(allFloorMeanwhile, "with both walls shut its ways are all floor, so it rests: the map's search keeps off what's shut for it");
	Check(byTheFirstAgain != none && times[byTheFirstAgain] - times[firstStuck] >= kAlienWaysShutFor
		&& times[byTheFirstAgain] - times[firstStuck] <= kAlienWaysShutFor + 3000,
		"once the first wall's 20 s are up, its next way goes by that wall again, and not before");
}

// What's shut for one alien isn't shut for another
static void NotShared()
{
	printf("The watchdog: what's shut for one alien is shut for it alone\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	Held const held = HeldTillStuck(guide, map, Ask(Alien(1), kStart, kGoal, 13), 3000);
	Check(held.stuck && held.line.shutPolygon == TheWall(), "one alien is stuck at the start, and the wall is shut for it");

	// While it rests, a second alien asks from the same place. The first has no way now, so it claims nothing that could send the second round.
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	guide.Steer(&map, Ask(Alien(2), kStart, kGoal, 13), &towards);
	Frame(guide, map, 100);
	const AlienWaysReport *reports = nullptr;
	size_t const count = ReportsOf(guide, &reports);
	bool second = false;

	for (size_t r = 0; r < count; r++)
	{
		second = second || (reports[r].who.place == Alien(2).place && ByTheWall(reports[r], TheWall()));
	}

	Check(second, "another alien's way goes by that wall: it isn't shut for it");
}

// An alien let go takes nothing shut with it, so nothing piles up across a level as aliens die or go (PO, 2026-11-11)
static void LetGoForgetsIt()
{
	printf("The watchdog: an alien let go, dead or gone, leaves nothing shut\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// Dead: the game lets it go at once (Forget). Asking again, it's an alien the guide has never seen: the wall isn't shut for it.
	const std::unique_ptr<AlienWaysGuide> deadRoom(new AlienWaysGuide);
	AlienWaysGuide &dead = *deadRoom;
	Held const killed = HeldTillStuck(dead, map, Ask(Alien(1), kStart, kGoal, 13), 3000);
	Check(killed.stuck && killed.line.shutPolygon == TheWall(), "stuck at the start, the wall is shut for it");
	dead.Forget(Alien(1));
	dead.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(dead, map, 100);
	Check(ReportsOf(dead, &reports) == 1 && ByTheWall(reports[0], TheWall()), "let go at once and asking again, its way goes by that wall: nothing is shut for it");

	// Gone: nothing asks about it for half a second, so the guide lets it go
	const std::unique_ptr<AlienWaysGuide> goneRoom(new AlienWaysGuide);
	AlienWaysGuide &gone = *goneRoom;
	Held const away = HeldTillStuck(gone, map, Ask(Alien(1), kStart, kGoal, 13), 3000);
	Check(away.stuck && away.line.shutPolygon == TheWall(), "another stuck at the start, the wall shut for it");
	Frame(gone, map, kAlienWaysForgetAfter);
	gone.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(gone, map, 100);
	Check(ReportsOf(gone, &reports) == 1 && ByTheWall(reports[0], TheWall()), "nothing asked about it for half a second, then asking again: nothing is shut for it");
}

static void AskingAgain()
{
	printf("Asking again\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// The goal moves 2,999 mm: nothing new. Then 3,001 mm: a new search, and it's followed on the old way till that comes.
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	Check(guide.Steer(&map, Ask(Alien(1), kStart, { 1000, 0, 9500 - 2999 }, 13), &towards), "a goal that moves 2,999 mm keeps its way");
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 0, "and asks for nothing");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, { 1000, 0, 9500 - 3001 }, 13), &towards) && Same(towards, kOntoTheWall),
		"a goal that moves 3,001 mm keeps following the way it has");
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound && Same(reports[0].to, { 1000, 0, 9500 - 3001 }),
		"and gets a new search, to where the goal is now");

	// A way 2 s old is searched again, while the alien makes headway along it
	const std::unique_ptr<AlienWaysGuide> oldRoom(new AlienWaysGuide);
	AlienWaysGuide &old = *oldRoom;
	GuidedAlien(old, Alien(1));
	size_t searches = 0;
	int searchedAt = -1;

	for (int ms = 100; ms <= 2500; ms += 100)
	{
		// Clinging to the wall at head height, going along it
		AlienWaysAsk at = Ask(Alien(1), Beside(0, -1500, 500 + ms * 3), kGoal, 13);
		at.down = IntoTheWall();
		old.Steer(&map, at, &towards);
		Frame(old, map, 100);
		size_t const now = ReportsOf(old, &reports);

		if (now > 0 && searchedAt < 0)
		{
			searchedAt = ms;
		}

		searches += now;
	}

	Check(searches == 1 && searchedAt == 2100, "a way is searched again once, in the frame it turns 2 s old");
}

static void FailedRenewal()
{
	printf("A renewal that finds nothing\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// The goal goes 40 m away, past any polygon. Its search can't start, but the alien keeps the way it has.
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	AlienWaysXYZ const nowhere = { 1000, 0, 50000 };
	Check(guide.Steer(&map, Ask(Alien(1), kStart, nowhere, 13), &towards), "the goal moves to where there's no polygon: the alien is still guided");
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeNoPolygon, "its search isn't started, and the report says why");
	Check(ReportsOf(guide, &reports) == 1 && reports[0].fromKind >= 0 && reports[0].toKind < 0, "and which end it was: the goal, not the alien");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, nowhere, 13), &towards) && Same(towards, kOntoTheWall), "it keeps the way it had, so nothing lets go of a wall");
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 0, "and the goal it failed for isn't searched for again in the next frame");
}

// Flankers (the user's rule): the first three aliens coming for one target search as usual, and the 4th is a flanker. One coming for another
// target is counted with that target's aliens, not these. And every report of a way found says what it cost and where it came in from.
static void Flankers()
{
	printf("Flankers\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	AlienWaysWho const marine = Alien(90), otherMarine = Alien(91);
	bool flankers[5] = {};
	bool reported[5] = {};

	// One alien at a time: each asks, and its search ends in the next frame. The ones before it are kept asking, so they stay on their ways.
	for (int n = 1; n <= 5; n++)
	{
		for (int before = 1; before < n; before++)
		{
			guide.Steer(&map, AskFor(Alien(before), before == 5 ? otherMarine : marine), &towards);
		}

		guide.Steer(&map, AskFor(Alien(n), n == 5 ? otherMarine : marine), &towards);
		Frame(guide, map, 16);

		if (ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound)
		{
			reported[n - 1] = true;
			flankers[n - 1] = reports[0].flanker;
		}

		if (n == 1)
		{
			Check(reported[0] && reports[0].cost > 0.0 && reports[0].arrivalDegrees == (kWallX == 0 ? 207 : 333),
				"a way found says what it cost, and the way it comes into its goal from, seen from above");
		}
	}

	Check(reported[0] && reported[1] && reported[2] && !flankers[0] && !flankers[1] && !flankers[2], "the first three aliens coming for one target search as usual");
	Check(reported[3] && flankers[3], "the 4th coming for it is a flanker");
	Check(reported[4] && !flankers[4], "one coming for another target is the first for that one, not a flanker");
}

// Where neither the alien's module nor the goal's has a polygon near, the whole map is looked through. At Third's 5-alien spot (2026-11-08)
// every way to the target was refused because no polygon of the target's own room was within 2 m of where it stood.
static void FoundAnywhere()
{
	printf("Polygons looked for anywhere when the modules have none near\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	AlienWaysAsk lost = Ask(Alien(1), kStart, kGoal, 13);
	lost.atPolygons = AlienWaysRange{ 0, 0 };
	lost.goalPolygons[0] = lost.goalPolygons[1] = AlienWaysRange{ 0, 0 };
	guide.Steer(&map, lost, &towards);
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound,
		"an alien and a goal whose modules have no polygons at all still get a way, from the polygons of the whole map");

	// Upside down as well, with no ceiling within 2 m: nothing of its own kind is near anywhere, so the last place looked is the whole map, any kind
	const std::unique_ptr<AlienWaysGuide> upsideDownRoom(new AlienWaysGuide);
	AlienWaysGuide &upsideDownGuide = *upsideDownRoom;
	AlienWaysAsk lostUpsideDown = lost;
	lostUpsideDown.at = AlienWaysXYZ{ 1000, 0, 5500 };
	lostUpsideDown.down = AlienWaysXYZ{ 0, -65536, 0 };
	upsideDownGuide.Steer(&map, lostUpsideDown, &towards);
	Frame(upsideDownGuide, map, 16);
	Check(ReportsOf(upsideDownGuide, &reports) == 1 && reports[0].outcome != kAlienWaysOutcomeNoPolygon && reports[0].fromKind == kAlienWaysFloor,
		"and one upside down, with its module empty and no ceiling within 2 m, starts on the nearest polygon of any kind in the whole map");
}

static void LetGo()
{
	printf("Let go, and given gravity back\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysWho lapsed[kAlienWaysMostFollowers];

	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Check(Frame(guide, map, 16, lapsed) == 0, "an alien steered this frame isn't let go");
	Check(Frame(guide, map, kAlienWaysForgetAfter, lapsed) == 1 && lapsed[0].place == Alien(1).place && lapsed[0].name == Alien(1).name,
		"one that nothing has asked about for half a second is let go, and named, so the game gives it its gravity back");
	Check(!guide.Guided(Alien(1)), "and it's no longer guided");
	Check(!guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "asking again, it starts again from nothing");

	// One that was never steered along a way isn't named: nothing turned its gravity off
	const std::unique_ptr<AlienWaysGuide> neverRoom(new AlienWaysGuide);
	AlienWaysGuide &never = *neverRoom;
	never.Steer(&map, Ask(Alien(2), kStart, kGoal, 13), &towards);
	Frame(never, map, 16, lapsed);
	Check(Frame(never, map, kAlienWaysForgetAfter, lapsed) == 0, "an alien that asked but was never guided isn't named when it's let go");

	// Nor one whose last steer gave back no: the game's own code has set its gravity already
	const std::unique_ptr<AlienWaysGuide> stoppedRoom(new AlienWaysGuide);
	AlienWaysGuide &stopped = *stoppedRoom;
	GuidedAlien(stopped, Alien(3));
	stopped.Steer(&map, Ask(Alien(3), { 1000, -3000, -2500 }, kGoal, 13), &towards);
	Frame(stopped, map, 16, lapsed);
	Check(Frame(stopped, map, kAlienWaysForgetAfter, lapsed) == 0, "nor one whose last steer said no");

	// Clear forgets everything
	const std::unique_ptr<AlienWaysGuide> clearedRoom(new AlienWaysGuide);
	AlienWaysGuide &cleared = *clearedRoom;
	GuidedAlien(cleared, Alien(4));
	cleared.Steer(&map, Ask(Alien(4), kStart, kGoal, 13), &towards);
	cleared.Clear();
	const AlienWaysPoint *points[1];
	int counts[1], headings[1];
	Check(!cleared.Guided(Alien(4)) && cleared.Shown(points, counts, headings, 1) == 0 && Frame(cleared, map, 16, lapsed) == 0,
		"Clear forgets every alien and every way, and names nobody");
}

// A hunting alien skips asking now and then. A frame or a few with nothing asked keep it, way, rest and all; half a second with nothing
// asked lets it go (kAlienWaysForgetAfter). In Sixth's 5-alien film (2026-11-08), when each skip dropped the way, it was searched for
// again nearly every frame and never followed.
static void KeptThroughAGap()
{
	printf("Kept through frames nothing asked\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysWho lapsed[kAlienWaysMostFollowers];
	const AlienWaysWho *missed = nullptr;
	const AlienWaysReport *reports = nullptr;

	// On its way for 600 ms, asked about every frame, so what it was last asked counts, not when it first came
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	bool onIt = true;

	for (int f = 0; f < 30; f++)
	{
		onIt = guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && onIt;
		Frame(guide, map, 20, lapsed);
	}

	Check(onIt && guide.MissedLastFrame(&missed) == 0, "an alien asked about every frame is on its way, and never missed");
	Check(Frame(guide, map, 16, lapsed) == 0, "a frame nothing asked about it doesn't let it go");
	Check(guide.MissedLastFrame(&missed) == 1 && missed[0].place == Alien(1).place && missed[0].name == Alien(1).name,
		"it's named as kept through a frame nothing asked, so the game can count it, or let it go");
	Check(guide.Guided(Alien(1)), "it's still on its way, so the game keeps it crawling, with its gravity off");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && Same(towards, kOntoTheWall), "asked again, it's on the same way");
	Frame(guide, map, 16, lapsed);
	Check(ReportsOf(guide, &reports) == 0 && guide.MissedLastFrame(&missed) == 0, "with no new search for it, and nothing missed in that frame");
	Check(Frame(guide, map, kAlienWaysForgetAfter, lapsed) == 1 && !guide.Guided(Alien(1)), "half a second with nothing asked lets it go, and names it");

	// The rest after a way that's all floor is kept through a gap too, so it isn't searched for again every frame
	AlienWaysMap floorOnly;
	floorOnly.Build(Corridor(false));
	const std::unique_ptr<AlienWaysGuide> flatRoom(new AlienWaysGuide);
	AlienWaysGuide &flat = *flatRoom;
	flat.Steer(&floorOnly, Ask(Alien(2), kStart, kGoal, 10), &towards);
	Frame(flat, floorOnly, 16);
	Check(ReportsOf(flat, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeAllFloor, "a way that's all floor, so it rests for 2 s");
	Frame(flat, floorOnly, 16);
	flat.Steer(&floorOnly, Ask(Alien(2), kStart, kGoal, 10), &towards);
	Frame(flat, floorOnly, 16);
	Check(ReportsOf(flat, &reports) == 0, "a frame nothing asked about it keeps its rest: it doesn't ask again");

	// The game lets go at once of an alien in a state that never asks
	const std::unique_ptr<AlienWaysGuide> forgetRoom(new AlienWaysGuide);
	AlienWaysGuide &forget = *forgetRoom;
	GuidedAlien(forget, Alien(3));
	forget.Steer(&map, Ask(Alien(3), kStart, kGoal, 13), &towards);
	Check(forget.Forget(Alien(3)) && !forget.Guided(Alien(3)), "Forget lets a guided alien go at once, and says it was guided, for its gravity");
	Check(!forget.Forget(Alien(3)), "one it doesn't have any more is no one to forget");
	forget.Steer(&map, Ask(Alien(4), kStart, kGoal, 13), &towards);
	Check(!forget.Forget(Alien(4)), "and one with no way yet goes, but wasn't guided");
	Check(Frame(forget, map, kAlienWaysForgetAfter, lapsed) == 0, "an alien forgotten isn't let go again later");
}

// Modified 2026-11-08 by Melanikus for the AvP port: an alien driven along its way through frames nothing asked (Drive, #109).
// A hunting alien's state skips asking every other frame. In those frames Drive gives the point Steer would have, but only then, only while
// the guide keeps the alien, and never so that it counts as asking: so it can't keep an alien a moment longer than the half second.
static void DrivenThroughAGap()
{
	printf("Driven through frames nothing asked\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards, driven;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;

	Check(!guide.Drive(&map, Alien(1), kStart, kUpright65536, &driven), "an alien the guide has never heard of isn't driven");
	GuidedAlien(guide, Alien(1));
	Check(!guide.Drive(&map, Alien(1), kStart, kUpright65536, &driven), "nor one whose way has come but isn't taken up yet: the next Steer takes it up where the alien is");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && Same(towards, kOntoTheWall), "asked, it heads for the wall's foot");
	Check(!guide.Drive(&map, Alien(1), kStart, kUpright65536, &driven), "in a frame Steer was asked, Drive leaves it to Steer");
	Frame(guide, map, 16);
	Check(guide.Drive(&map, Alien(1), kStart, kUpright65536, &driven) && Same(driven, kOntoTheWall), "in the next frame, nothing asked, it's driven to the point Steer gave");
	Check(!guide.GuidedThisFrame(Alien(1)), "and being driven isn't being asked");

	// Once it's on the wall, the step onto the wall is made, and Drive heads it on up, as Steer would
	Check(guide.Drive(&map, Alien(1), Beside(300, -500, 500), IntoTheWall(), &driven) && Same(driven, kUpTheWall), "on the wall, it's driven on to the next point, 1.5 m up");

	// Frames with only Drive, for as long as the guide keeps it: 16 ms, then 20 frames of 20 ms, is 416 ms since Steer was last asked
	bool drivenAll = true;

	for (int f = 0; f < 20; f++)
	{
		Frame(guide, map, 20);
		drivenAll = guide.Drive(&map, Alien(1), Beside(300, -500, 500), IntoTheWall(), &driven) && drivenAll;
	}

	Check(drivenAll, "it's driven in every frame of the half second the guide keeps it through");
	Check(Frame(guide, map, 100) == 1 && !guide.Guided(Alien(1)), "then the guide lets it go just as it would have with no Drive: Drive never kept it longer");
	Check(!guide.Drive(&map, Alien(1), Beside(300, -500, 500), IntoTheWall(), &driven), "and once it's let go, it isn't driven");

	// Off its way, it's left to the game, and the next Steer drops the way
	const std::unique_ptr<AlienWaysGuide> offRoom(new AlienWaysGuide);
	AlienWaysGuide &off = *offRoom;
	GuidedAlien(off, Alien(2));
	off.Steer(&map, Ask(Alien(2), kStart, kGoal, 13), &towards);
	Frame(off, map, 16);
	Check(!off.Drive(&map, Alien(2), AlienWaysXYZ{ 1000, 0, 5000 }, kUpright65536, &driven), "an alien 4.5 m off its way isn't driven along it");
	Check(off.Guided(Alien(2)), "and Drive doesn't drop the way: that's for the next Steer");

	// Resting after it was stuck, it isn't driven
	const std::unique_ptr<AlienWaysGuide> stuckRoom(new AlienWaysGuide);
	AlienWaysGuide &stuck = *stuckRoom;
	GuidedAlien(stuck, Alien(3));

	for (int ms = 0; ms < 1700; ms += 100)
	{
		stuck.Steer(&map, Ask(Alien(3), kStart, kGoal, 13), &towards);
		Frame(stuck, map, 100);
	}

	Check(!stuck.Steer(&map, Ask(Alien(3), kStart, kGoal, 13), &towards), "an alien that made no headway for 1.5 s is resting");
	Frame(stuck, map, 16);
	Check(!stuck.Drive(&map, Alien(3), kStart, kUpright65536, &driven), "and while it rests it isn't driven");
}

// Whether the way steers an alien in this frame: only in a frame Steer was asked and gave back a way (Hunt and Wander hold back the game's
// avoidance by this, so it must never be true for an alien with no way)
static void GuidedThisFrame()
{
	printf("Guided in this frame\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	Check(!guide.GuidedThisFrame(Alien(1)), "an alien the guide has never heard of isn't");
	GuidedAlien(guide, Alien(1));
	Check(!guide.GuidedThisFrame(Alien(1)), "after asking, with no way yet steering it, it isn't");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && guide.GuidedThisFrame(Alien(1)), "in a frame its way steers it, it is");
	Frame(guide, map, 16);
	Check(guide.Guided(Alien(1)) && !guide.GuidedThisFrame(Alien(1)), "in the next frame, before anything asks Steer, it isn't, though it's still guided till it's let go");
	Check(!guide.Steer(&map, Ask(Alien(1), { 1000, 0, 4000 }, kGoal, 13), &towards) && !guide.GuidedThisFrame(Alien(1)), "in a frame Steer says no, it isn't");
}

// Two aliens with the same start and goal take different walls: the second's search pays more on the first's way (lanes, #138). And an
// alien let go takes its claims with it.
static void DifferentWays()
{
	printf("Different ways for aliens together\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && towards.x == kWallX, "the first alien goes up the wall a lone alien takes");
	guide.Steer(&map, Ask(Alien(2), kStart, kGoal, 13), &towards);
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(guide, map, 16);
	Check(guide.Steer(&map, Ask(Alien(2), kStart, kGoal, 13), &towards) && towards.x == 2000 - kWallX, "the second, asking while the first is on its way, goes up the other wall");

	// One alien, let go: the next alien's way is as if it had never been. The next asks while the first still has its place, so it has a
	// place of its own, and the first's claims, if they stayed, would count against it.
	const std::unique_ptr<AlienWaysGuide> goneRoom(new AlienWaysGuide);
	AlienWaysGuide &gone = *goneRoom;
	GuidedAlien(gone, Alien(1));
	gone.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	gone.Steer(&map, Ask(Alien(3), kStart, kGoal, 13), &towards);
	Check(gone.Forget(Alien(1)), "the first alien is let go before the second's search");
	Frame(gone, map, 16);
	Check(gone.Steer(&map, Ask(Alien(3), kStart, kGoal, 13), &towards) && towards.x == kWallX, "and the second takes the same wall the first did: its claims went with it");
}

static void WhoIsWho()
{
	printf("Who is who\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const std::unique_ptr<AlienWaysGuide> guideRoom(new AlienWaysGuide);
	AlienWaysGuide &guide = *guideRoom;
	GuidedAlien(guide, Alien(1));

	// A new alien kept where the old one was has another name, so it isn't given the old one's way
	AlienWaysWho newcomer = Alien(1);
	newcomer.name = 0xBEEF;
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "the first alien is guided");
	Check(!guide.Steer(&map, Ask(newcomer, kStart, kGoal, 13), &towards) && !guide.Guided(newcomer),
		"one kept in the same place with another name is someone else, with no way yet");

	// Room for 64. The 65th is steered by the game's own code, and that's counted.
	const std::unique_ptr<AlienWaysGuide> fullRoom(new AlienWaysGuide);
	AlienWaysGuide &full = *fullRoom;

	for (int n = 0; n < static_cast<int>(kAlienWaysMostFollowers); n++)
	{
		full.Steer(&map, Ask(Alien(100 + n), kStart, kGoal, 13), &towards);
	}

	Check(full.TurnedAway() == 0, "64 aliens all get a place");
	Check(!full.Steer(&map, Ask(Alien(999), kStart, kGoal, 13), &towards) && full.TurnedAway() == 1, "the 65th is turned away, and that's counted");
}

static void Allowance()
{
	printf("The frame's allowance\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// Two ask in one frame: with the usual allowance both are found in the next
	const std::unique_ptr<AlienWaysGuide> twoRoom(new AlienWaysGuide);
	AlienWaysGuide &two = *twoRoom;
	two.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	two.Steer(&map, Ask(Alien(2), { 1000, 0, 1500 }, kGoal, 13), &towards);
	Frame(two, map, 16);
	Check(ReportsOf(two, &reports) == 2 && reports[0].outcome == kAlienWaysOutcomeFound && reports[1].outcome == kAlienWaysOutcomeFound,
		"two aliens' searches both end in one frame, within its 500 polygons");

	// With 1 polygon a frame, a search takes several frames, and ends with the same way as one done at once
	const std::unique_ptr<AlienWaysGuide> slowRoom(new AlienWaysGuide);
	AlienWaysGuide &slow = *slowRoom;
	slow.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	int frames = 0;
	size_t ended = 0;

	while (ended == 0 && frames < 100)
	{
		slow.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
		Frame(slow, map, 16, nullptr, 1);
		ended = ReportsOf(slow, &reports);
		frames++;
	}

	Check(ended == 1 && reports[0].frames > 1 && reports[0].frames == frames && reports[0].looks == frames,
		"with 1 polygon a frame, the search takes a frame for each polygon it looks at");
	Check(ended == 1 && reports[0].points == 6 && Near(reports[0].wallMm, 11500.0, 0.5), "and ends with the same way as one done at once");

	// A search for an alien that's let go part way is dropped, so another alien in its place never gets its way
	const std::unique_ptr<AlienWaysGuide> droppedRoom(new AlienWaysGuide);
	AlienWaysGuide &dropped = *droppedRoom;
	dropped.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(dropped, map, 16, nullptr, 1);
	Frame(dropped, map, kAlienWaysForgetAfter, nullptr, 1);
	AlienWaysXYZ const elsewhere = { 1000, 0, 3500 };
	dropped.Steer(&map, Ask(Alien(2), elsewhere, kGoal, 13), &towards);
	bool fromElsewhere = false;

	for (int f = 0; f < 100 && !fromElsewhere; f++)
	{
		dropped.Steer(&map, Ask(Alien(2), elsewhere, kGoal, 13), &towards);
		Frame(dropped, map, 16);
		fromElsewhere = ReportsOf(dropped, &reports) == 1;
	}

	Check(fromElsewhere && Same(reports[0].from, elsewhere), "a search for an alien let go part way is dropped: the next alien's way is searched from where it is");
}

static void NoBetterThanToday()
{
	printf("No way, or no better way\n");
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// A corridor with no walls: the only way is all floor, which the game's own code does as well, so it's left to it
	AlienWaysMap floorOnly;
	floorOnly.Build(Corridor(false));
	const std::unique_ptr<AlienWaysGuide> flatRoom(new AlienWaysGuide);
	AlienWaysGuide &flat = *flatRoom;
	flat.Steer(&floorOnly, Ask(Alien(1), kStart, kGoal, 10), &towards);
	Frame(flat, floorOnly, 16);
	Check(ReportsOf(flat, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeAllFloor && Near(reports[0].floorMm, 9000.0, 0.5),
		"a way that's all floor is reported as that");
	Check(!flat.Steer(&floorOnly, Ask(Alien(1), kStart, kGoal, 10), &towards), "and isn't followed: the game's own code steers the alien");
	Frame(flat, floorOnly, 1000);
	flat.Steer(&floorOnly, Ask(Alien(1), kStart, kGoal, 10), &towards);
	Frame(flat, floorOnly, 16);
	Check(ReportsOf(flat, &reports) == 0, "and it doesn't ask again for 2 s");

	// But a way all on the floor to the target itself is followed: the game's own hunting may never close in (Third's 5-alien spot)
	const std::unique_ptr<AlienWaysGuide> closeRoom(new AlienWaysGuide);
	AlienWaysGuide &close = *closeRoom;
	AlienWaysAsk toTheTarget = Ask(Alien(2), kStart, kGoal, 10);
	toTheTarget.goalIsTheTarget = true;
	close.Steer(&floorOnly, toTheTarget, &towards);
	Frame(close, floorOnly, 16);
	Check(ReportsOf(close, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound && Near(reports[0].floorMm, 9000.0, 0.5),
		"a way all on the floor to the target itself is taken as a way");
	Check(close.Steer(&floorOnly, toTheTarget, &towards) && close.Guided(Alien(2)), "and followed, for the last metres");

	// Two squares 5 m apart: no way joins them
	AlienWaysMap apart;
	apart.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 0, 1000 }, { 0, 0, 1000 } }, kUp),
		Polygon({ { 6000, 0, 0 }, { 7000, 0, 0 }, { 7000, 0, 1000 }, { 6000, 0, 1000 } }, kUp) });
	const std::unique_ptr<AlienWaysGuide> islandsRoom(new AlienWaysGuide);
	AlienWaysGuide &islands = *islandsRoom;
	islands.Steer(&apart, Ask(Alien(1), { 500, 0, 500 }, { 6500, 0, 500 }, 2), &towards);
	Frame(islands, apart, 16);
	Check(ReportsOf(islands, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeNoWay, "between two places no way joins, the report says so");
	Check(!islands.Steer(&apart, Ask(Alien(1), { 500, 0, 500 }, { 6500, 0, 500 }, 2), &towards), "and the alien isn't guided");
}

// Where a search starts: on the kind of surface the alien is standing on, by its down (Secondary's note on 79a00e7c)
static void StartsOnItsOwnKind()
{
	printf("Where a search starts\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// 300 mm out from the left wall and 200 mm above tile 5: the floor's box is the nearer, 200 mm against 300
	AlienWaysXYZ const low = { 300, -200, 5500 };
	const std::unique_ptr<AlienWaysGuide> standingRoom(new AlienWaysGuide);
	AlienWaysGuide &standing = *standingRoom;
	standing.Steer(&map, Ask(Alien(1), low, kGoal, 13), &towards);
	Frame(standing, map, 16);
	Check(ReportsOf(standing, &reports) == 1 && reports[0].fromKind == kAlienWaysFloor && !reports[0].toTheTarget,
		"an alien standing on the floor starts its search on the floor, and the report says the goal wasn't the target itself");

	const std::unique_ptr<AlienWaysGuide> clingingRoom(new AlienWaysGuide);
	AlienWaysGuide &clinging = *clingingRoom;
	AlienWaysAsk onTheWall = Ask(Alien(1), low, kGoal, 13);
	onTheWall.down = AlienWaysXYZ{ -65536, 0, 0 };
	clinging.Steer(&map, onTheWall, &towards);
	Frame(clinging, map, 16);
	Check(ReportsOf(clinging, &reports) == 1 && reports[0].fromKind == kAlienWaysWall, "one clinging low on the wall starts on the wall, though the floor is nearer");

	// The same, but its module holds only the floor (polygons 0 to 9): its own module comes first, whatever the kind, before a wall of another
	const std::unique_ptr<AlienWaysGuide> moduleFirstRoom(new AlienWaysGuide);
	AlienWaysGuide &moduleFirst = *moduleFirstRoom;
	AlienWaysAsk floorModule = Ask(Alien(1), low, kGoal, 13);
	floorModule.down = AlienWaysXYZ{ -65536, 0, 0 };
	floorModule.atPolygons = AlienWaysRange{ 0, 10 };
	moduleFirst.Steer(&map, floorModule, &towards);
	Frame(moduleFirst, map, 16);
	Check(ReportsOf(moduleFirst, &reports) == 1 && reports[0].fromKind == kAlienWaysFloor,
		"one clinging low on the wall, whose module holds only the floor, starts on its module's floor, not on a wall that's no part of it");

	// Upside down on the floor, with no ceiling within 2 m: it starts on the nearest polygon of any kind
	const std::unique_ptr<AlienWaysGuide> nothingNearRoom(new AlienWaysGuide);
	AlienWaysGuide &nothingNear = *nothingNearRoom;
	AlienWaysAsk upsideDown = Ask(Alien(1), { 1000, 0, 5500 }, kGoal, 13);
	upsideDown.down = AlienWaysXYZ{ 0, -65536, 0 };
	nothingNear.Steer(&map, upsideDown, &towards);
	Frame(nothingNear, map, 16);
	Check(ReportsOf(nothingNear, &reports) == 1 && reports[0].fromKind == kAlienWaysFloor, "with none of its own kind within 2 m, it starts on the nearest of any kind");

	const std::unique_ptr<AlienWaysGuide> toTheTargetRoom(new AlienWaysGuide);
	AlienWaysGuide &toTheTarget = *toTheTargetRoom;
	AlienWaysAsk toIt = Ask(Alien(1), kStart, kGoal, 13);
	toIt.goalIsTheTarget = true;
	toTheTarget.Steer(&map, toIt, &towards);
	Frame(toTheTarget, map, 16);
	Check(ReportsOf(toTheTarget, &reports) == 1 && reports[0].toTheTarget, "when the goal is the target itself, the report says so");
}

// When a step onto a new polygon counts as made (AlienWays_StepMade): by the planes where both facings are known, by kind where not
static AlienWaysPoint PointFacing(AlienWaysKind kind, double fx, double fy, double fz)
{
	AlienWaysPoint p = { 0, 0, 0, kind, { 0, 0, 0 } };
	p.facing[0] = static_cast<int16_t>(lround(fx * 32767.0));
	p.facing[1] = static_cast<int16_t>(lround(fy * 32767.0));
	p.facing[2] = static_cast<int16_t>(lround(fz * 32767.0));
	return p;
}

static void StepsMade()
{
	printf("When a step onto a new surface is made\n");
	double const half = sqrt(0.5);
	AlienWaysPoint const floor = PointFacing(kAlienWaysFloor, 0.0, -1.0, 0.0);              // y goes down, so a floor faces -y
	AlienWaysPoint const wall = PointFacing(kAlienWaysWall, 1.0, 0.0, 0.0);
	AlienWaysPoint const nextWall = PointFacing(kAlienWaysWall, 0.0, 0.0, 1.0);            // round an outside corner from wall
	AlienWaysPoint const sameWall = PointFacing(kAlienWaysWall, 0.98, 0.0, 0.2);           // the same wall, a little turned
	AlienWaysPoint const bevel = PointFacing(kAlienWaysFloor, half, -half, 0.0);           // half up: the map counts it as floor
	AlienWaysXYZ const standing = { 0, 65536, 0 }, clingingToWall = { -65536, 0, 0 }, onBevel = { -46341, 46341, 0 };

	Check(!AlienWays_StepMade(floor, wall, standing), "from the floor onto a wall: not while the alien stands on the floor");
	Check(AlienWays_StepMade(floor, wall, clingingToWall), "made once it clings to the wall");
	Check(AlienWays_StepMade(bevel, wall, onBevel),
		"from a bevel onto the wall beside it, 45 degrees: made already, where by kind (floor to wall) it waited for a turn that never came");
	Check(!AlienWays_StepMade(wall, nextWall, clingingToWall), "round an outside corner, 90 degrees: not made while it's on the first wall");
	Check(AlienWays_StepMade(wall, sameWall, standing), "onto the same wall, a little turned: there's no step to make, whatever its down");

	AlienWaysPoint unknownWall = wall;
	unknownWall.facing[0] = 0;
	Check(!AlienWays_StepMade(bevel, unknownWall, onBevel) && AlienWays_StepMade(bevel, unknownWall, clingingToWall),
		"with a facing not known, it goes by kind, as before: the bevel counts as floor");
	Check(!AlienWays_StepMade(floor, wall, AlienWaysXYZ{ 0, 0, 0 }), "with no down known, a step onto another plane isn't made");
}

// Which doors let an alien through: the rule the game's own routes for aliens follow (AIModuleAdmitsPheromones, pheromone.cpp)
static void DoorRule()
{
	printf("Which doors let an alien through\n");
	Check(AlienWays_DoorLetsAnAlienThrough(kAlienWaysProximityDoor, false, false),
		"a proximity door that isn't locked lets an alien through while it's still shut: it opens as the alien comes near");
	Check(AlienWays_DoorLetsAnAlienThrough(kAlienWaysProximityDoor, false, true), "and while it's open");
	Check(!AlienWays_DoorLetsAnAlienThrough(kAlienWaysProximityDoor, true, false), "a locked one doesn't");
	Check(!AlienWays_DoorLetsAnAlienThrough(kAlienWaysProximityDoor, true, true), "not even while it's still open, since a locked door shuts");
	Check(!AlienWays_DoorLetsAnAlienThrough(kAlienWaysLiftDoor, false, false), "a lift's door that's shut doesn't");
	Check(AlienWays_DoorLetsAnAlienThrough(kAlienWaysLiftDoor, false, true), "one that's open does");
	Check(!AlienWays_DoorLetsAnAlienThrough(kAlienWaysSwitchDoor, false, false), "a switch's door that isn't all the way open doesn't");
	Check(AlienWays_DoorLetsAnAlienThrough(kAlienWaysSwitchDoor, false, true), "one all the way open does");
	Check(!AlienWays_DoorLetsAnAlienThrough(static_cast<AlienWaysDoorKind>(7), false, true), "a kind that isn't one of them is taken as shut");
}

// THE GAME'S SIDE, read from its own files, because only a running game reaches it

// One source file, with Windows line ends made plain
static std::string ReadSourceFile(char const *name)
{
	std::string const path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return std::string();
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	std::string plain;

	for (char const letter : all.str())
	{
		if (letter != '\r')
		{
			plain += letter;
		}
	}

	return plain;
}

static bool Has(std::string const &text, char const *part)
{
	return text.find(part) != std::string::npos;
}

// The text from one mark to the next. A mark that isn't there fails a check by name, so no check passes on an empty piece of text unheard.
static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? std::string::npos : text.find(to, start + strlen(from));

	if (end == std::string::npos)
	{
		Check(false, std::string("SOURCE: the marks \"") + from + "\" and \"" + to + "\" are found, in that order");
		return std::string();
	}

	return text.substr(start, end - start);
}

// Whether first is there, and before second
static bool ComesBefore(std::string const &text, char const *first, char const *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);
	return a != std::string::npos && b != std::string::npos && a < b;
}

static void SourceOfTheGame()
{
	printf("The game's side, read from its files\n");
	std::string const near = ReadSourceFile("3dc/avp/bh_near.cpp");
	std::string const ways = ReadSourceFile("3dc/avp/alien_ways.cpp");
	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj");
	Check(!near.empty() && !ways.empty() && !project.empty(), "SOURCE: the game's files can be read");

	// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109, the owner's asks): the view keeps each alien's last way, dimmed, and a
	// found way's points go in the log as places
	std::string const shownCode = Between(ways, "size_t AlienWays_WaysToShow(AlienWaysShown *out, size_t most)\n{", "\n}\n");
	Check(Has(shownCode, "size_t const kept = theGuide.Kept(points, counts, room);") && Has(shownCode, "out[shown + i].kept = true;")
		&& Has(shownCode, "out[i].kept = false;") && Has(shownCode, "return shown + kept;"),
		"SOURCE: the view is given the followed ways, then each other alien's kept way, marked as kept");
	Check(Has(ways, "int const count = theGuide.WayOf(line.follower, &points);") && Has(ways, "details += \". Places (m):\";")
		&& Has(ways, "details += \" \" + Metres({ points[p].x, points[p].y, points[p].z });"),
		"SOURCE: a found way's line in the log ends with its points, as places in metres");

	// The hook, in AlienNearState_Approach
	std::string const approach = Between(near, "static void AlienNearState_Approach(STRATEGYBLOCK *sbPtr)\n{", "/* state change and sequence change tests must go here");
	Check(Has(approach, "\tif ((alienStatusPointer->Type == AT_Standard) && (distanceToPlayer > ALIEN_ATTACKDISTANCE_MIN) && AlienIsAbleToClimb(sbPtr)\n"
		"\t\t&& AlienWays_Steer(sbPtr, alienStatusPointer->Target, &targetPos, &wayDirection))"),
		"SOURCE: only a standard alien that can climb, further away than it attacks from, asks for a way, to the point the game's own code picked");
	Check(ComesBefore(approach, "/* Never curve in a waypoint module, you might hurt yourself. */", "AlienWays_Steer("),
		"SOURCE: and it asks after that point has been picked");
	Check(Has(approach, "\t\tdynPtr->UseStandardGravity=0;\n\t\tNPCSetVelocity(sbPtr, &wayDirection, alienStatusPointer->MaxSpeed);"),
		"SOURCE: on a way, it sticks to what it's touching and goes towards the way's next point");
	Check(Has(approach, "\t}\n\telse if(curveToPlayer)\n\t{"), "SOURCE: without a way, the game's own steering is left as it was");

	// On a frame the way steers it, the game's tests that would send it off to avoid the wall it's climbing don't act (the Colony film), and
	// the log says when they would have
	std::string const tests = Between(near, "static void AlienNearState_Approach(STRATEGYBLOCK *sbPtr)\n{", "/* still here?... make some noise */");
	Check(Has(tests, "\t\tonAWay = true;\n\t}\n\telse if(curveToPlayer)"), "SOURCE: an alien steered by its way this frame is marked so");
	Check(Has(tests, "\t\tif (onAWay)\n\t\t{\n\t\t\tif (obstruction.environment || obstruction.destructableObject)\n\t\t\t{\n\t\t\t\tAlienWays_WouldHaveAvoided(sbPtr, false);\n\t\t\t}\n\t\t}\n\t\telse if(obstruction.environment)"),
		"SOURCE: then running into the wall it climbs doesn't send it off, and the log says it would have");
	Check(Has(tests, "\t\tif (onAWay && NPC_CannotReachTarget(&(alienStatusPointer->moveData), &targetPos, &velocityDirection))\n\t\t{\n\t\t\tAlienWays_WouldHaveAvoided(sbPtr, true);\n\t\t}\n\t\telse if(!onAWay && NPC_CannotReachTarget("),
		"SOURCE: nor does turning back as it goes round by the walls, and the log says so");
	Check(Has(tests, "\t#if ALL_NEW_AVOIDANCE_ALIEN\n\tif (!onAWay)\n\t{"), "SOURCE: and the other avoidance, switched off in this game, is kept off a way too");

	// The same in Wander (Fourth's hook there)
	std::string const wander = Between(near, "static void AlienNearState_Wander(STRATEGYBLOCK *sbPtr)\n{", "static void AlienNearState_Wait(STRATEGYBLOCK *sbPtr)\n{");
	Check(Has(wander, "\tbool const onAWay = AlienWays_GuidedThisFrame(sbPtr);   // its way steers it this frame (Wander)"),
		"SOURCE: in Wander, an alien steered by its way this frame is marked so, by the guide");
	Check(!Has(wander, "onAWay = (dynPtr->UseStandardGravity"), "SOURCE: in Wander, the mark isn't read from gravity, which the state's own code can turn off with no way");
	Check(Has(wander, "\t\tif (onAWay && ((obstruction.environment)||(obstruction.otherCharacter)))   // Wander\n\t\t{\n\t\t\tAlienWays_WouldHaveAvoided(sbPtr, false);\n\t\t}\n\t\telse if((obstruction.environment)||(obstruction.otherCharacter))"),
		"SOURCE: in Wander, running into the wall it climbs doesn't send it off, and the log says it would have");
	Check(Has(wander, "\tif (onAWay && NPC_CannotReachTarget(&(alienStatusPointer->moveData), &(alienStatusPointer->wanderData.worldPosition), &velocityDirection))   // Wander\n\t{\n\t\tAlienWays_WouldHaveAvoided(sbPtr, true);\n\t}\n\telse if(!onAWay && NPC_CannotReachTarget("),
		"SOURCE: in Wander, nor does turning back, and the log says so");
	Check(Has(wander, "\t#if ALL_NEW_AVOIDANCE_ALIEN\n\tif (!onAWay)   // Wander\n\t{"), "SOURCE: in Wander, the switched-off avoidance is kept off a way too");

	// The same in Hunt (Fourth's hook there)
	std::string const hunt = Between(near, "static void AlienNearState_Hunt(STRATEGYBLOCK *sbPtr)\n{", "static void AlienNearState_Pounce(STRATEGYBLOCK *sbPtr)\n{");
	Check(Has(hunt, "\tbool const onAWay = AlienWays_GuidedThisFrame(sbPtr);   // its way steers it this frame (Hunt)"),
		"SOURCE: in Hunt, an alien steered by its way this frame is marked so, by the guide");
	Check(!Has(hunt, "onAWay = (dynPtr->UseStandardGravity"), "SOURCE: in Hunt, the mark isn't read from gravity, which the state's own code can turn off with no way");
	Check(Has(hunt, "\t\tif (onAWay && ((obstruction.environment)||(obstruction.otherCharacter)))   // Hunt\n\t\t{\n\t\t\tAlienWays_WouldHaveAvoided(sbPtr, false);\n\t\t}\n\t\telse if((obstruction.environment)||(obstruction.otherCharacter))"),
		"SOURCE: in Hunt, running into the wall it climbs doesn't send it off, and the log says it would have");
	Check(Has(hunt, "\tif (onAWay && NPC_CannotReachTarget(&(alienStatusPointer->moveData), &(alienStatusPointer->wanderData.worldPosition), &velocityDirection))   // Hunt\n\t{\n\t\tAlienWays_WouldHaveAvoided(sbPtr, true);\n\t}\n\telse if(!onAWay && NPC_CannotReachTarget("),
		"SOURCE: in Hunt, nor does turning back, and the log says so");
	Check(Has(hunt, "\t#if ALL_NEW_AVOIDANCE_ALIEN\n\tif (!onAWay)   // Hunt\n\t{"), "SOURCE: in Hunt, the switched-off avoidance is kept off a way too");

	std::string const would = Between(ways, "void AlienWays_WouldHaveAvoided(STRATEGYBLOCK *alien, bool turnedBack)", "bool AlienWays_Following(");
	Check(Has(would, "if (!w || clockMs - w->lastWouldHaveMs < 1000)"), "SOURCE: the line comes at most once a second for each alien");
	Check(Has(would, "wouldHaveLines >= kAlienWaysMostWouldHaveLines"), "SOURCE: and has a most for each level");

	std::string const guidedNow = Between(ways, "bool AlienWays_GuidedThisFrame(STRATEGYBLOCK *alien)", "size_t AlienWays_WaysToShow(");
	// Modified 2026-11-11 by Melanikus (Third): fd9da61a puts a found way's places between the polygons and the line's end, so the two are
	// checked in order, not side by side
	Check(ComesBefore(ways, "\tdetails += polygons;\n", "\tLogString(std::string(text) + details);\n") && Has(ways, "polygons = line.onGrid ? \". Cells:\" : \". Polygons:\";"),
		"SOURCE: a search's line lists the polygons its way is on, or the grid's cells on a way found on the grid");
	Check(Has(ways, "\tif (clockMs / 1000 != placesSecond)\n\t{\n\t\tplacesSecond = clockMs / 1000;"), "SOURCE: and once a second a line says where the guided aliens are");
	Check(Has(guidedNow, "return alien && AlienWays_On() && ReadyMap() && theGuide.GuidedThisFrame(WhoIs(alien));"),
		"SOURCE: an alien is guided in this frame only with the switch on, on a machine that runs the aliens, with a map, and as the guide says");

	std::string const crawling = Between(near, "int AlienShouldBeCrawling(STRATEGYBLOCK *sbPtr)", "int AlienIsAbleToClimb(STRATEGYBLOCK *sbPtr)");
	Check(Has(crawling, "\tif (AlienWays_Following(sbPtr)) {\n\t\treturn(1);\n\t}"), "SOURCE: an alien on a way crawls");

	// Gravity back, for every alien the guide lets go. The alien is found by LiveAlien, which the stuck line uses too.
	std::string const finder = Between(ways, "static STRATEGYBLOCK *LiveAlien(const AlienWaysWho &who)", "// An alien that's no longer guided gets");
	std::string const back = Between(ways, "static void GiveGravityBack(const AlienWaysWho &who)", "// A live alien's state");
	Check(Has(finder, "if (now.name == who.name && object->I_SBtype == I_BehaviourAlien && object->DynPtr)\n\t\t{\n\t\t\treturn object;")
		&& Has(back, "STRATEGYBLOCK *const alien = LiveAlien(who);") && Has(back, "\t\talien->DynPtr->UseStandardGravity = 1;"),
		"SOURCE: an alien let go gets its usual gravity back, if it's the same one, still an alien");
	Check(Has(finder, "for (int i = 0; i < NumActiveStBlocks; i++)") && Has(finder, "if (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object)) != who.place)"),
		"SOURCE: and it's only looked for among the game's live objects, so nothing dead is read");

	std::string const frame = Between(ways, "void AlienWays_Frame()", "bool AlienWays_Steer(");
	Check(Has(frame, "size_t const lapsedCount = theGuide.Frame(ReadyMap(), stepMs, kAlienWaysLooksPerFrame, lapsed, kAlienWaysMostFollowers);"),
		"SOURCE: each frame the guide has its turn, with room to name every alien it guides");
	Check(Has(frame, "\t\tGiveGravityBack(lapsed[i]);"), "SOURCE: and every alien it names gets its gravity back");
	Check(Has(frame, "\t\tTellHowASearchWent(reports[i]);"), "SOURCE: and every search that ended is a line in the log");

	std::string const steer = Between(ways, "bool AlienWays_Steer(", "bool AlienWays_Following(");
	Check(Has(steer, "if (!AlienWays_On() || !map ||"), "SOURCE: no alien is guided with the switch off, or on a machine that doesn't run the aliens");

	// The goal: the target itself when it can be seen from the alien's module (the user's first play: ways to the next module were mostly floor)
	std::string const inView = Between(ways, "static bool InView(MODULE *from, MODULE *to)", "// An alien that's no longer guided");
	Check(Has(inView, "return from->m_vmptr && IsModuleVisibleFromModule(from, to);"), "SOURCE: a module with no list of what it can see sees only itself, and no empty list is read");
	Check(Has(steer, "if (target && target->DynPtr && InView(alien->containingModule, target->containingModule))\n\t{\n"
		"\t\task.goal = AlienWaysXYZ{ target->DynPtr->Position.vx, target->DynPtr->Position.vy, target->DynPtr->Position.vz };\n\t\task.goalIsTheTarget = true;"),
		"SOURCE: when the target can be seen from the alien's module, the way goes all the way to the target");
	Check(Has(ways, "(line.toTheTarget ? \" (its target)\" : \" (on the way to its target)\")"), "SOURCE: and the log says which goal a search took");
	Check(Has(steer, "\t\task.goalPolygons[1] = PolygonsOf(ModuleFromPosition(&where, alien->containingModule));"),
		"SOURCE: otherwise the goal's polygons are looked for in the module the goal is in, found from where it is");
	// Modified 2026-11-11 by Melanikus for the AvP port (#109): the line names the alien by its place and id (alien_ways_stuck.h)
	Check(Has(ways, "case kAlienWaysOutcomeStuck:\n\t\t\tsnprintf(text, sizeof(text), \"alien ways: %s stuck at %s, heading for %s"),
		"SOURCE: an alien found stuck is a line in the log");

	std::string const end = Between(ways, "void AlienWays_LevelEnd()", "told = true;");
	Check(ComesBefore(end, "theGuide.Clear();", "theBuild.Stop();"), "SOURCE: at a level's end the ways go, before the map they're on");
	Check(Has(end, "std::vector<AlienWaysLevelDoor>().swap(theDoors);"), "SOURCE: and so do its doors");

	// Doors (#118): each frame the map is told which doors won't let an alien through, before the searches carry on, each door read from its own
	// module's block once that's checked to be its kind
	std::string const level = ReadSourceFile("3dc/avp/alien_ways_level.cpp");

	// The guide's own step check goes by AlienWays_StepMade, which StepsMade tries: the ways in this file's corridor carry no facings, so
	// only the source can show it's the one used
	std::string const guideSource = ReadSourceFile("3dc/avp/alien_ways_guide.cpp");
	Check(Has(Between(guideSource, "static bool OnTheKindNeeded(", "bool AlienWays_StepMade("), "\treturn AlienWays_StepMade(points[heading - 1], points[heading], down);"),
		"SOURCE: the guide counts a step as made by AlienWays_StepMade");

	// A flanker's search goes with a flanker's step and most (the map test tries what those do), and the game says which target each is for.
	// The search starts in two places, the grid's and the old map's, each checked by the words before its step, so neither can lose the
	// flanker's step unseen (modified 2026-11-11 by Melanikus (Third) for the AvP port: one check matched either, so the old map's could go)
	Check(Has(guideSource, "next, shut,\n\t\t\t\tflanker ? kAlienWaysFlankerClaimStep : kAlienWaysClaimPriceStep, flanker ? kAlienWaysFlankerMostPriceFactor : kAlienWaysClaimMostPriceFactor);"),
		"SOURCE: a flanker searches the grid with a flanker's step and most");
	Check(Has(guideSource, "claimsOn ? &claims : nullptr, next,\n\t\t\t\tflanker ? kAlienWaysFlankerClaimStep : kAlienWaysClaimPriceStep, flanker ? kAlienWaysFlankerMostPriceFactor : kAlienWaysClaimMostPriceFactor,\n\t\t\t\tshut);"),
		"SOURCE: a flanker searches with a flanker's step and most");

	// THE WATCHDOG (added 2026-11-11 by Melanikus (Third) for the AvP port, #109): both searches keep off what's shut for the alien, the
	// grid's as well as the map's, which only the map's is tried by here; and the log says what the watchdog did
	Check(Has(guideSource, "const std::vector<uint8_t> *const shut = ShutForTheSearch(*map, follower, from, to);"),
		"SOURCE: both searches are given what's shut for the alien searching");
	std::string const afterStuck = Between(ways, "static std::string AfterStuckWords(const AlienWaysReport &line)", "// One line in the log for each search that ended");
	Check(Has(ways, "kAlienWaysNoHeadwayFor / 1000.0, AfterStuckWords(line).c_str());")
		&& Has(afterStuck, "case kAlienWaysReplans:\n\t\t\tsnprintf(words, sizeof(words), \": it re-plans at once, re-plan %d of %d this minute, and %s\"")
		&& Has(afterStuck, "case kAlienWaysRestsAtTheStart:") && Has(afterStuck, "case kAlienWaysRestsCapped:"),
		"SOURCE: a stuck line says whether the alien re-plans, rests at its way's start, or rests with its re-plans used up");
	Check(Has(afterStuck, "\"polygon %d is shut for it for %.0f s\", line.shutPolygon, kAlienWaysShutFor / 1000.0"),
		"SOURCE: and which polygon is shut for it, and for how long");
	Check(Has(ways, "if (line.outcome != kAlienWaysOutcomeStuck && line.afterReplan)\n\t{\n\t\tdetails = \". After a re-plan\" + details;"),
		"SOURCE: a search a re-plan asked for says so in its line");
	Check(Has(ways, "\task.hasTarget = target != 0;\n\task.target = target ? WhoIs(target) : AlienWaysWho{ 0, 0 };"),
		"SOURCE: the game tells the guide which target an alien is coming for");
	Check(!level.empty(), "SOURCE: the level's lister can be read");
	Check(ComesBefore(frame, "AlienWaysLevel_ReadDoors(theDoors, ReadyMap());", "theGuide.Frame(ReadyMap()"),
		"SOURCE: each frame the map hears about the doors before the searches carry on");
	Check(Has(ways, "AlienWaysLevel_List(&polygons, &theModules, &theDoors, &counts);"), "SOURCE: the doors are listed with the level's polygons");
	Check(Has(level, "\t\t\tfor (size_t p = first; p < listing->polygons->size(); p++)\n\t\t\t{\n\t\t\t\t(*listing->polygons)[p].door = door;"),
		"SOURCE: every polygon of a door's module carries the door's number");
	Check(Has(level, "case I_BehaviourProximityDoor: isADoor = true; kind = kAlienWaysProximityDoor; break;")
		&& Has(level, "case I_BehaviourLiftDoor:      isADoor = true; kind = kAlienWaysLiftDoor;      break;")
		&& Has(level, "case I_BehaviourSwitchDoor:    isADoor = true; kind = kAlienWaysSwitchDoor;    break;"),
		"SOURCE: the three kinds of door the game knows are listed as doors");
	std::string const readDoors = Between(level, "void AlienWaysLevel_ReadDoors(", "map->SetDoorShut(");
	Check(Has(readDoors, "if (door && door->SBdataptr && door->SBmoptr == module)"),
		"SOURCE: a door with no block is never read, nor one whose block points at another module (Secondary's D-a)");
	Check(Has(readDoors, "\t\t\t\t\tif (door->I_SBtype == I_BehaviourProximityDoor)\n\t\t\t\t\t{\n\t\t\t\t\t\tPROXDOOR_BEHAV_BLOCK const *const block")
		&& Has(readDoors, "\t\t\t\t\tif (door->I_SBtype == I_BehaviourLiftDoor)\n\t\t\t\t\t{\n\t\t\t\t\t\tthrough =")
		&& Has(readDoors, "\t\t\t\t\tif (door->I_SBtype == I_BehaviourSwitchDoor)\n\t\t\t\t\t{\n\t\t\t\t\t\tthrough ="),
		"SOURCE: each door's block is read only once it's checked to be that kind");
	Check(Has(readDoors, "through = AlienWays_DoorLetsAnAlienThrough(kAlienWaysProximityDoor, block->door_locked != 0, GetState(door) != 0);"),
		"SOURCE: a proximity door is judged by whether it's locked");
	Check(Has(readDoors, "bool through = false;"), "SOURCE: a door that can't be read is taken as shut");
	Check(Has(level, "map->SetDoorShut(static_cast<int>(d) + 1, !through);"), "SOURCE: and the map is told each door's state, by the number its polygons carry");

	// The aim: 1.5 m ahead unless -alienwaysaim says otherwise, given to the guide at each level's start
	Check(Has(ways, "int AlienWaysAimAheadMm = kAlienWaysAimAheadMm;"), "SOURCE: aliens aim 1.5 m along their ways unless a switch says otherwise");
	Check(Has(Between(ways, "void AlienWays_LevelStart()", "if (!AlienWays_On())"), "\ttheGuide.SetAimAhead(AlienWaysAimAheadMm);\n"),
		"SOURCE: and the guide is told it at each level's start");

	// A polygon's stored facing goes to the map, for standing a way off an outside corner (kAlienWaysStandOff), unless it's drawn from both sides
	Check(Has(level, "\t\t\t\tif (!(polygon->PolyFlags & iflag_no_bfc))\n\t\t\t\t{\n\t\t\t\t\tAlienWays_TurnedFacing(AlienWaysXYZ{ stored.vx, stored.vy, stored.vz }, turning, facing);"),
		"SOURCE: a polygon's stored facing, turned with its object, goes with it, unless it's drawn from both sides and could face either way");
	Check(Has(level, "\t\t\t\tlisting->polygons->back().facing[0] = facing[0];\n\t\t\t\tlisting->polygons->back().facing[1] = facing[1];\n\t\t\t\tlisting->polygons->back().facing[2] = facing[2];"),
		"SOURCE: and is put on the polygon the map gets");

	// Kept through a gap (KeptThroughAGap): each kept alien is found by LiveAlien; one in a state that never asks is let go at once, with its
	// gravity back; one in a state that asks is counted for the once-a-second line
	Check(Has(frame, "\t\tSTRATEGYBLOCK *const alien = LiveAlien(missed[i]);"), "SOURCE: an alien kept through a gap is only read once it's found alive and the same");
	Check(Has(frame, "\t\tif (state != ABS_Approach && state != ABS_Hunt && state != ABS_Wander)\n"),
		"SOURCE: one that's in a state that never asks for a way is let go at once");
	Check(Has(frame, "if (theGuide.Forget(missed[i]) && letGoCount < kAlienWaysMostFollowers)\n\t\t\t{\n\t\t\t\tlapsed[letGoCount] = missed[i];"),
		"SOURCE: and joins the aliens let go, for its gravity");
	Check(Has(frame, "\tfor (size_t i = 0; i < letGoCount; i++)\n"), "SOURCE: and every alien let go, either way, gets its gravity back");
	Check(Has(frame, "\t\tmissedFrames++;\n\t\tmissedByState[state]++;"), "SOURCE: one still in a state that asks is counted by its state");
	Check(Has(frame, "\t\tif (missedFrames > 0)\n") && Has(frame, "aliens kept on their ways went "), "SOURCE: and the count goes in the log once a second");

	Check(Has(project, "<ClCompile Include=\"..\\..\\3dc\\avp\\alien_ways_guide.cpp\" />"), "SOURCE: the game's project builds the guide");
}

int main()
{
	FirstWay();
	Reaching();
	TakenUpWhereItIs();
	OffTheWay();
	Stuck();
	TheClock();
	ReplansAtOnce();
	ShutForTwentySeconds();
	NotShared();
	LetGoForgetsIt();
	AskingAgain();
	FailedRenewal();
	FoundAnywhere();
	Flankers();
	LetGo();
	GuidedThisFrame();
	KeptThroughAGap();
	DrivenThroughAGap();
	DifferentWays();
	AimAhead();
	WhoIsWho();
	Allowance();
	NoBetterThanToday();
	StartsOnItsOwnKind();
	DoorRule();
	StepsMade();
	SourceOfTheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all alien ways guide checks passed\n");
	return 0;
}
