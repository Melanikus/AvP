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
// From (1000, 0, 500) on tile 0 to (1000, 0, 9500) on tile 9, with the floor at 10 times the price, the way is 4 points: the start, then
// 1 m of floor to the left wall's foot at (0, 0, 500), then 8.5 m along the wall to (0, 0, 9000), where tile 9 meets it, then 1.118 m of
// floor to the goal. The map's own test works that sum out. Coordinates are the game's: millimetres, and y goes down.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "alien_ways_guide.h"

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
AlienWaysXYZ const kOntoTheWall = { 0, 0, 500 }, kOffTheWall = { 0, 0, 9000 };
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
	AlienWaysGuide guide;
	AlienWaysMap notBuilt;
	AlienWaysXYZ towards = { 7, 7, 7 };

	Check(!guide.Steer(&notBuilt, Ask(Alien(1), kStart, kGoal, 13), &towards) && !guide.Steer(nullptr, Ask(Alien(1), kStart, kGoal, 13), &towards),
		"with no map, an alien isn't guided");
	Check(Frame(guide, notBuilt, 16) == 0, "and a frame with no map lets nothing go and searches for nothing");

	AlienWaysMap &map = CorridorMap();
	Check(map.Ready(), "the corridor's map is built");
	Check(!guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "an alien's first ask has no way yet: the game's own code steers it this frame");
	Check(!guide.Guided(Alien(1)), "so it isn't guided");

	Check(Frame(guide, map, 16) == 0, "the next frame lets nothing go");
	const AlienWaysReport *reports = nullptr;
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeFound, "and its search ends in that frame with a way, in one report");
	Check(reports[0].points == 4 && Near(reports[0].floorMm, 2118.0, 1.0) && Near(reports[0].wallMm, 8500.0, 0.5) && reports[0].ceilingMm == 0.0,
		"the report says 4 points: 2.118 m of floor, 8.5 m of wall and no ceiling");
	Check(reports[0].frames == 1 && reports[0].looks > 0 && reports[0].waitedMs == 16 && Same(reports[0].from, kStart) && Same(reports[0].to, kGoal),
		"one frame, some polygons looked at, 16 ms after it asked, from its place to its goal");
	Check(!reports[0].cutShort && reports[0].follower == 0, "not cut short, and the first of the guide's places");

	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards) && Same(towards, kOntoTheWall), "now it's steered: towards the wall's foot");
	Check(guide.Guided(Alien(1)), "and it's guided");

	const AlienWaysPoint *points[4];
	int counts[4], headings[4];
	Check(guide.Shown(points, counts, headings, 4) == 1 && counts[0] == 4 && headings[0] == 1, "the view is shown its way: 4 points, heading for the second");
	Check(guide.Shown(points, counts, headings, 0) == 0, "with no room, the view is shown nothing");
}

static void Reaching()
{
	printf("Reaching the way's points\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;

	// The wall's foot is (0, 0, 500). The alien's down is +y. 300 mm across the floor from it, and 500 mm up, is reached.
	AlienWaysGuide guide;
	GuidedAlien(guide, Alien(1));
	Check(guide.Steer(&map, Ask(Alien(1), { 300, -500, 500 }, kGoal, 13), &towards) && Same(towards, kOffTheWall),
		"300 mm across and 500 mm up from a point is reaching it: the next point is where the way comes off the wall");

	// 650 mm across isn't
	AlienWaysGuide across;
	GuidedAlien(across, Alien(1));
	Check(across.Steer(&map, Ask(Alien(1), { 650, -500, 500 }, kGoal, 13), &towards) && Same(towards, kOntoTheWall), "650 mm across isn't reached yet");

	// Up from the surface, the alien's own middle may be as far as 1.5 m from a point on it
	AlienWaysGuide high;
	GuidedAlien(high, Alien(1));
	Check(high.Steer(&map, Ask(Alien(1), { 100, -1600, 500 }, kGoal, 13), &towards) && Same(towards, kOntoTheWall), "1.6 m above it isn't reached");
	Check(high.Steer(&map, Ask(Alien(1), { 100, -1400, 500 }, kGoal, 13), &towards) && Same(towards, kOffTheWall), "1.4 m above it is");

	// An alien clinging to the left wall has the wall as its down (-x). 900 mm out from the wall is then its height off the surface, which is
	// allowed, not 900 mm across it, which wouldn't be.
	AlienWaysGuide onTheWall;
	GuidedAlien(onTheWall, Alien(1));
	AlienWaysAsk climbing = Ask(Alien(1), { 900, 0, 500 }, kGoal, 13);
	climbing.down = AlienWaysXYZ{ -65536, 0, 0 };
	Check(onTheWall.Steer(&map, climbing, &towards) && Same(towards, kOffTheWall), "with the wall as its down, 900 mm out from the wall's foot is reaching it");
	AlienWaysGuide onTheFloor;
	GuidedAlien(onTheFloor, Alien(1));
	Check(onTheFloor.Steer(&map, Ask(Alien(1), { 900, 0, 500 }, kGoal, 13), &towards) && Same(towards, kOntoTheWall),
		"with the floor as its down, the same spot is 900 mm across, and isn't");

	// The last point reached means the way is used up, and it asks again
	AlienWaysGuide end;
	GuidedAlien(end, Alien(1));
	end.Steer(&map, Ask(Alien(1), { 0, 0, 600 }, kGoal, 13), &towards);
	end.Steer(&map, Ask(Alien(1), { 0, 0, 8900 }, kGoal, 13), &towards);
	Check(!end.Steer(&map, Ask(Alien(1), { 1000, 0, 9400 }, kGoal, 13), &towards) && !end.Guided(Alien(1)), "at the goal, its way is used up, and it's no longer guided");
}

static void TakenUpWhereItIs()
{
	printf("A new way, taken up where the alien is now\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;

	// The way was searched from the start, but by the time it comes the alien is halfway along the wall's foot. It's 4.5 m from the way's first
	// stretch, which would be "off the way", but on its second, so it takes the way up there.
	AlienWaysGuide guide;
	GuidedAlien(guide, Alien(1));
	Check(guide.Steer(&map, Ask(Alien(1), { 0, 0, 5000 }, kGoal, 13), &towards) && Same(towards, kOffTheWall),
		"an alien that's gone on while its way was searched takes it up at the stretch it's on, and isn't off the way");
}

static void OffTheWay()
{
	printf("Off the way\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysGuide guide;
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

static void Stuck()
{
	printf("No headway\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysGuide guide;
	GuidedAlien(guide, Alien(1));

	// It stays where it is, 1 m from the wall's foot, frame after frame: 100 ms a frame
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

	// For 3 s it isn't guided and asks for nothing
	bool asked = false, guided = false;

	for (ms = 0; ms < 2900; ms += 100)
	{
		Frame(guide, map, 100);
		const AlienWaysReport *reports = nullptr;
		asked = asked || ReportsOf(guide, &reports) > 0;
		guided = guided || guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	}

	Check(!asked && !guided, "for the next 3 s it's left to the game's own code, and asks for no way");
	Frame(guide, map, 100);
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(guide, map, 16);
	const AlienWaysReport *reports = nullptr;
	Check(ReportsOf(guide, &reports) == 1 && guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "then it asks again, and is guided again");
}

static void AskingAgain()
{
	printf("Asking again\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	const AlienWaysReport *reports = nullptr;

	// The goal moves 2,999 mm: nothing new. Then 3,001 mm: a new search, and it's followed on the old way till that comes.
	AlienWaysGuide guide;
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
	AlienWaysGuide old;
	GuidedAlien(old, Alien(1));
	size_t searches = 0;
	int searchedAt = -1;

	for (int ms = 100; ms <= 2500; ms += 100)
	{
		AlienWaysXYZ const at = { 0, 0, 500 + ms * 3 };
		old.Steer(&map, Ask(Alien(1), at, kGoal, 13), &towards);
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
	AlienWaysGuide guide;
	GuidedAlien(guide, Alien(1));
	AlienWaysXYZ const nowhere = { 1000, 0, 50000 };
	Check(guide.Steer(&map, Ask(Alien(1), kStart, nowhere, 13), &towards), "the goal moves to where there's no polygon: the alien is still guided");
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeNoPolygon, "its search isn't started, and the report says why");
	Check(guide.Steer(&map, Ask(Alien(1), kStart, nowhere, 13), &towards) && Same(towards, kOntoTheWall), "it keeps the way it had, so nothing lets go of a wall");
	Frame(guide, map, 16);
	Check(ReportsOf(guide, &reports) == 0, "and the goal it failed for isn't searched for again in the next frame");
}

static void LetGo()
{
	printf("Let go, and given gravity back\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysWho lapsed[kAlienWaysMostFollowers];

	AlienWaysGuide guide;
	GuidedAlien(guide, Alien(1));
	guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Check(Frame(guide, map, 16, lapsed) == 0, "an alien steered this frame isn't let go");
	Check(Frame(guide, map, 16, lapsed) == 1 && lapsed[0].place == Alien(1).place && lapsed[0].name == Alien(1).name,
		"one that wasn't steered since is let go, and named, so the game gives it its gravity back");
	Check(!guide.Guided(Alien(1)), "and it's no longer guided");
	Check(!guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "asking again, it starts again from nothing");

	// One that was never steered along a way isn't named: nothing turned its gravity off
	AlienWaysGuide never;
	never.Steer(&map, Ask(Alien(2), kStart, kGoal, 13), &towards);
	Frame(never, map, 16, lapsed);
	Check(Frame(never, map, 16, lapsed) == 0, "an alien that asked but was never guided isn't named when it's let go");

	// Nor one whose last steer gave back no: the game's own code has set its gravity already
	AlienWaysGuide stopped;
	GuidedAlien(stopped, Alien(3));
	stopped.Steer(&map, Ask(Alien(3), { 2000, -3000, 0 }, kGoal, 13), &towards);
	Frame(stopped, map, 16, lapsed);
	Check(Frame(stopped, map, 16, lapsed) == 0, "nor one whose last steer said no");

	// Clear forgets everything
	AlienWaysGuide cleared;
	GuidedAlien(cleared, Alien(4));
	cleared.Steer(&map, Ask(Alien(4), kStart, kGoal, 13), &towards);
	cleared.Clear();
	const AlienWaysPoint *points[1];
	int counts[1], headings[1];
	Check(!cleared.Guided(Alien(4)) && cleared.Shown(points, counts, headings, 1) == 0 && Frame(cleared, map, 16, lapsed) == 0,
		"Clear forgets every alien and every way, and names nobody");
}

static void WhoIsWho()
{
	printf("Who is who\n");
	AlienWaysMap &map = CorridorMap();
	AlienWaysXYZ towards;
	AlienWaysGuide guide;
	GuidedAlien(guide, Alien(1));

	// A new alien kept where the old one was has another name, so it isn't given the old one's way
	AlienWaysWho newcomer = Alien(1);
	newcomer.name = 0xBEEF;
	Check(guide.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards), "the first alien is guided");
	Check(!guide.Steer(&map, Ask(newcomer, kStart, kGoal, 13), &towards) && !guide.Guided(newcomer),
		"one kept in the same place with another name is someone else, with no way yet");

	// Room for 64. The 65th is steered by the game's own code, and that's counted.
	AlienWaysGuide full;

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
	AlienWaysGuide two;
	two.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	two.Steer(&map, Ask(Alien(2), { 1000, 0, 1500 }, kGoal, 13), &towards);
	Frame(two, map, 16);
	Check(ReportsOf(two, &reports) == 2 && reports[0].outcome == kAlienWaysOutcomeFound && reports[1].outcome == kAlienWaysOutcomeFound,
		"two aliens' searches both end in one frame, within its 500 polygons");

	// With 1 polygon a frame, a search takes several frames, and ends with the same way as one done at once
	AlienWaysGuide slow;
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
	Check(ended == 1 && reports[0].points == 4 && Near(reports[0].wallMm, 8500.0, 0.5), "and ends with the same way as one done at once");

	// A search for an alien that's let go part way is dropped, so another alien in its place never gets its way
	AlienWaysGuide dropped;
	dropped.Steer(&map, Ask(Alien(1), kStart, kGoal, 13), &towards);
	Frame(dropped, map, 16, nullptr, 1);
	Frame(dropped, map, 16, nullptr, 1);
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
	AlienWaysGuide flat;
	flat.Steer(&floorOnly, Ask(Alien(1), kStart, kGoal, 10), &towards);
	Frame(flat, floorOnly, 16);
	Check(ReportsOf(flat, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeAllFloor && Near(reports[0].floorMm, 9000.0, 0.5),
		"a way that's all floor is reported as that");
	Check(!flat.Steer(&floorOnly, Ask(Alien(1), kStart, kGoal, 10), &towards), "and isn't followed: the game's own code steers the alien");
	Frame(flat, floorOnly, 1000);
	flat.Steer(&floorOnly, Ask(Alien(1), kStart, kGoal, 10), &towards);
	Frame(flat, floorOnly, 16);
	Check(ReportsOf(flat, &reports) == 0, "and it doesn't ask again for 2 s");

	// Two squares 5 m apart: no way joins them
	AlienWaysMap apart;
	apart.Build({ Polygon({ { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 0, 1000 }, { 0, 0, 1000 } }, kUp),
		Polygon({ { 6000, 0, 0 }, { 7000, 0, 0 }, { 7000, 0, 1000 }, { 6000, 0, 1000 } }, kUp) });
	AlienWaysGuide islands;
	islands.Steer(&apart, Ask(Alien(1), { 500, 0, 500 }, { 6500, 0, 500 }, 2), &towards);
	Frame(islands, apart, 16);
	Check(ReportsOf(islands, &reports) == 1 && reports[0].outcome == kAlienWaysOutcomeNoWay, "between two places no way joins, the report says so");
	Check(!islands.Steer(&apart, Ask(Alien(1), { 500, 0, 500 }, { 6500, 0, 500 }, 2), &towards), "and the alien isn't guided");
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

	std::string const crawling = Between(near, "int AlienShouldBeCrawling(STRATEGYBLOCK *sbPtr)", "int AlienIsAbleToClimb(STRATEGYBLOCK *sbPtr)");
	Check(Has(crawling, "\tif (AlienWays_Following(sbPtr)) {\n\t\treturn(1);\n\t}"), "SOURCE: an alien on a way crawls");

	// Gravity back, for every alien the guide lets go
	std::string const back = Between(ways, "static void GiveGravityBack(const AlienWaysWho &who)", "// A point in metres, for the log");
	Check(Has(back, "if (now.name == who.name && object->I_SBtype == I_BehaviourAlien && object->DynPtr)\n\t\t{\n\t\t\tobject->DynPtr->UseStandardGravity = 1;"),
		"SOURCE: an alien let go gets its usual gravity back, if it's the same one, still an alien");
	Check(Has(back, "for (int i = 0; i < NumActiveStBlocks; i++)") && Has(back, "if (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object)) != who.place)"),
		"SOURCE: and it's only looked for among the game's live objects, so nothing dead is read");

	std::string const frame = Between(ways, "void AlienWays_Frame()", "bool AlienWays_Steer(");
	Check(Has(frame, "size_t const lapsedCount = theGuide.Frame(ReadyMap(), stepMs, kAlienWaysLooksPerFrame, lapsed, kAlienWaysMostFollowers);"),
		"SOURCE: each frame the guide has its turn, with room to name every alien it guides");
	Check(Has(frame, "\t\tGiveGravityBack(lapsed[i]);"), "SOURCE: and every alien it names gets its gravity back");
	Check(Has(frame, "\t\tTellHowASearchWent(reports[i]);"), "SOURCE: and every search that ended is a line in the log");

	std::string const steer = Between(ways, "bool AlienWays_Steer(", "bool AlienWays_Following(");
	Check(Has(steer, "if (!AlienWays_On() || !map ||"), "SOURCE: no alien is guided with the switch off, or on a machine that doesn't run the aliens");

	std::string const end = Between(ways, "void AlienWays_LevelEnd()", "told = true;");
	Check(ComesBefore(end, "theGuide.Clear();", "theBuild.Stop();"), "SOURCE: at a level's end the ways go, before the map they're on");

	Check(Has(project, "<ClCompile Include=\"..\\..\\3dc\\avp\\alien_ways_guide.cpp\" />"), "SOURCE: the game's project builds the guide");
}

int main()
{
	FirstWay();
	Reaching();
	TakenUpWhereItIs();
	OffTheWay();
	Stuck();
	AskingAgain();
	FailedRenewal();
	LetGo();
	WhoIsWho();
	Allowance();
	NoBetterThanToday();
	SourceOfTheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all alien ways guide checks passed\n");
	return 0;
}
