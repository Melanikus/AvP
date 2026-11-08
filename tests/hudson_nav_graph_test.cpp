// Tests for Hudson's map, in a small made-up world of boxes. Written 2026-11-05 by Melanikus for the AvP port. See 3dc/avp/hudson_nav_graph.h.
//
// In this world, as in the game, y goes down. A floor at height 0 is a box from y = 0 to y = 100: a smaller y is higher up.

#include "hudson_nav_graph.h"
#include "hudson_nav_plan.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

static int checksDone = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checksDone++;

	if (!good)
	{
		checksFailed++;
		printf("FAILED: %s\n", what.c_str());
	}
}

struct Box
{
	int minX, minY, minZ;
	int maxX, maxY, maxZ;
	bool door;
	int topFaces;     // which way its top faces, if it isn't straight up: how a slope is pretended here
};

class World : public HudsonNavRays
{
public:
	World() : asked(0) {}

	void Add(int minX, int minY, int minZ, int maxX, int maxY, int maxZ, bool door = false, int topFaces = -65536)
	{
		Box const box = { minX, minY, minZ, maxX, maxY, maxZ, door, topFaces };

		boxes.push_back(box);
	}

	void Floor(int minX, int minZ, int maxX, int maxZ, int height)
	{
		Add(minX, height, minZ, maxX, height + 100, maxZ);
	}

	void Wall(int minX, int minZ, int maxX, int maxZ)
	{
		Add(minX, -10000, minZ, maxX, 10000, maxZ);
	}

	HudsonNavRayAnswer Cast(int x, int y, int z, int directionX, int directionY, int directionZ, int longest)
	{
		asked++;

		double const dx = directionX / 65536.0, dy = directionY / 65536.0, dz = directionZ / 65536.0;
		HudsonNavRayAnswer answer = { false, false, longest, 0 };
		double nearest = longest;

		for (size_t i = 0; i < boxes.size(); i++)
		{
			Box const &box = boxes[i];
			double enter = 0.0, leave = nearest;
			int faceAxis = -1;
			bool fromTheLow = false;

			double const from[3] = { (double)x, (double)y, (double)z };
			double const way[3] = { dx, dy, dz };
			double const low[3] = { (double)box.minX, (double)box.minY, (double)box.minZ };
			double const high[3] = { (double)box.maxX, (double)box.maxY, (double)box.maxZ };
			bool missed = false;

			for (int axis = 0; axis < 3 && !missed; axis++)
			{
				if (way[axis] == 0.0)
				{
					missed = from[axis] < low[axis] || from[axis] > high[axis];
					continue;
				}

				double a = (low[axis] - from[axis]) / way[axis];
				double b = (high[axis] - from[axis]) / way[axis];
				bool lowFirst = true;

				if (a > b)
				{
					double const swap = a;
					a = b;
					b = swap;
					lowFirst = false;
				}

				if (a > enter)
				{
					enter = a;
					faceAxis = axis;
					fromTheLow = lowFirst;
				}

				if (b < leave)
				{
					leave = b;
				}

				missed = enter > leave;
			}

			// A ray that starts inside a box goes straight out of it, as a ray from inside a polygon's room does
			if (missed || faceAxis < 0)
			{
				continue;
			}

			if (enter <= nearest)
			{
				nearest = enter;
				answer.hit = true;
				answer.distance = (int)enter;
				answer.door = box.door;

				// Boxes can't slope, so one pretending to be a slope says so from whichever side a ray comes, as a real sloping polygon would
				if (box.topFaces != -65536)
				{
					answer.normalY = box.topFaces;
				}
				else
				{
					answer.normalY = (faceAxis == 1) ? (fromTheLow ? box.topFaces : 65536) : 0;
				}
			}
		}

		return answer;
	}

	void RemoveLast()
	{
		boxes.pop_back();
	}

	int asked;

private:
	std::vector<Box> boxes;
};

// A world that says odd things when a ray hits nothing. Nothing should go by them.
class WorldThatMisleads : public HudsonNavRays
{
public:
	explicit WorldThatMisleads(World &real) : world(real) {}

	HudsonNavRayAnswer Cast(int x, int y, int z, int directionX, int directionY, int directionZ, int longest)
	{
		HudsonNavRayAnswer answer = world.Cast(x, y, z, directionX, directionY, directionZ, longest);

		if (!answer.hit)
		{
			answer.distance = 0;
			answer.normalY = -65536;
		}

		return answer;
	}

private:
	World &world;
};

static HudsonNavSearch RunIt(HudsonNavGraph &map, World &world, int raysEachTime = 1000000)
{
	HudsonNavSearch search = kHudsonNavSearching;

	for (int i = 0; i < 1000000 && search == kHudsonNavSearching; i++)
	{
		search = map.Work(world, raysEachTime);
	}

	return search;
}

// True if every step of the way really can be walked in the world: no wall at his knees, nor at his head.
static bool WayIsClear(HudsonNavGraph &map, World &world)
{
	std::vector<HudsonNavPoint> const &way = map.Way();

	for (size_t i = 0; i + 1 < way.size(); i++)
	{
		if (abs(way[i].y - way[i + 1].y) <= kHudsonNavStepUp && !map.LineIsClear(world, way[i].x, way[i].y, way[i].z, way[i + 1].x, way[i + 1].z, false))
		{
			return false;
		}
	}

	return true;
}

// A floor 40 metres square, and a wall all round it
static void AddRoom(World &world, int height = 0)
{
	world.Floor(0, 0, 40000, 40000, height);
}

static void CheckTheStraightWay()
{
	printf("An open floor:\n");

	World world;
	AddRoom(world);

	HudsonNavGraph map;

	map.StartSearch(5500, 0, 5500, 15500, 0, 5500);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way across an open floor");
	Check(map.Way().size() >= 2, "with points on it");
	Check(map.Way().back().x == 15500 && map.Way().back().z == 5500, "and it ends where he was going");
	Check(map.Way().front().x == 5500 && map.Way().front().z == 5500, "and starts where he stands");
	Check(WayIsClear(map, world), "and it can all be walked");

	// It's nearly straight: the first point that's close enough is ten strides off, less the reach of the goal
	Check(map.Way().size() <= 12, "and it doesn't wander");
	Check(map.Spots() < 400, "and it only made as much map as it needed");
}

static void CheckGoingRoundACorner()
{
	printf("Round a corner:\n");

	// An L-shaped corridor: east along z = 0 to 3000, then north along x = 20000 to 23000. Everything else is wall.
	World world;

	world.Floor(0, 0, 23000, 3000, 0);
	world.Floor(20000, 0, 23000, 20000, 0);
	world.Wall(-1000, -1000, 24000, 0);        // the long side, all along z = 0
	world.Wall(-1000, 3000, 20000, 4000);      // the inside of the bend's far side
	world.Wall(23000, -1000, 24000, 21000);    // the far side of the second leg
	world.Wall(19000, 3000, 20000, 21000);     // the inside of the second leg
	world.Wall(-1000, -1000, 0, 4000);         // the end of the first leg
	world.Wall(19000, 20000, 24000, 21000);    // the end of the second leg

	HudsonNavGraph map;

	Check(!map.LineIsClear(world, 2000, 0, 1500, 21500, 15000, false), "the straight line from one end to the other goes through a wall");

	map.StartSearch(2000, 0, 1500, 21500, 0, 15000);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way round the corner");
	Check(WayIsClear(map, world), "and every step of it can be walked");

	bool wentRound = false;

	for (size_t i = 0; i < map.Way().size(); i++)
	{
		wentRound = wentRound || (map.Way()[i].x >= 20000 && map.Way()[i].z <= 3000);
	}

	Check(wentRound, "and it goes by way of the corner");

	// Where he is along the way, he can see some of it, and not all of it
	int const furthest = map.FurthestInSight(world, 2000, 0, 1500, 12);

	Check(furthest >= 1, "from the start he can see along the corridor");
	Check(map.Way()[furthest].z <= 3000, "but not round the corner");
	Check(map.NextAlong() >= 0, "and there's always a next point");

	// Near the bend, the way ahead goes round it, and he can't see the part round the corner. A place with the wall between him and it.
	{
		int const nearBend = map.FurthestInSight(world, 19000, 0, 2500, 20);

		Check(nearBend >= 0, "near the bend he can still see some of it");

		// A line down his middle can miss a corner that his shoulder would hit
		World corner;

		corner.Floor(0, 0, 20000, 20000, 0);
		corner.Wall(5000, 5000, 6000, 6000);

		HudsonNavGraph wide;

		Check(wide.LineIsClear(corner, 2000, 0, 6100, 9000, 6300, false), "a line just past a corner is clear, down his middle");
		Check(!wide.WayIsWideEnough(corner, 2000, 0, 6100, 9000, 6300), "but it's too close for his shoulders");
		Check(wide.WayIsWideEnough(corner, 2000, 0, 7000, 9000, 7000), "and one further off is wide enough");

		if (nearBend >= 0)
		{
			Check(map.LineIsClear(world, 19000, 0, 2500, map.Way()[nearBend].x, map.Way()[nearBend].z, false), "and what he can see, he really can");
			Check((size_t)nearBend + 1 < map.Way().size(), "and it isn't simply the last of it, which is out of sight");
		}
	}

	// A wall right across, so there's no way
	world.Wall(10000, 0, 10100, 3000);

	HudsonNavGraph blocked;

	blocked.StartSearch(2000, 0, 1500, 21500, 0, 15000);
	Check(RunIt(blocked, world) == kHudsonNavNoWay, "a wall right across means there's no way");
	Check(!blocked.Way().empty() && blocked.Way().back().x < 10000, "and the way goes as near as it could get, on his own side");
	Check(blocked.Spots() < 200, "and it finishes when there's nowhere left to look");
}

static void CheckFloorsAboveEachOther()
{
	printf("Floors above each other:\n");

	// A lower floor at height 3000, an upper at height 0 over part of it, joined by stairs of 400 a step going up along z.
	World world;

	world.Floor(0, 0, 30000, 30000, 3000);
	world.Floor(0, 15000, 30000, 30000, 0);

	for (int step = 0; step < 8; step++)
	{
		int const top = 3000 - (step + 1) * 375;

		world.Add(5000, top, 7000 + step * 1000, 10000, 3000, 8000 + step * 1000);
	}

	// Under the upper floor there's room, because it's 3000 up
	HudsonNavGraph map;

	map.StartSearch(7500, 3000, 2500, 7500, 3000, 25000);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way to the lower floor under the upper one");
	Check(map.Way().back().y == 3000, "and he ends on the lower one");

	bool stayedDown = true;

	for (size_t i = 0; i < map.Way().size(); i++)
	{
		stayedDown = stayedDown && map.Way()[i].y >= 2500;
	}

	Check(stayedDown, "and he doesn't go by way of the upper one");

	HudsonNavGraph up;

	up.StartSearch(7500, 3000, 2500, 7500, 0, 25000);
	Check(RunIt(up, world) == kHudsonNavFound, "there's a way from the lower floor to the upper one, straight above where the other way ended");
	Check(up.Way().back().y == 0, "and he ends on the upper one");

	bool climbed = false, inSteps = true;

	for (size_t i = 0; i + 1 < up.Way().size(); i++)
	{
		climbed = climbed || (up.Way()[i].z > 7000 && up.Way()[i].z < 15000 && up.Way()[i].y < 3000);
		inSteps = inSteps && abs(up.Way()[i].y - up.Way()[i + 1].y) <= kHudsonNavStepUp;
	}

	Check(climbed, "by way of the stairs");
	Check(inSteps, "and never more than a step at a time");

	// The same map, one floor and then the other: the same x and z can be two different spots
	HudsonNavGraph both;

	both.StartSearch(7500, 3000, 2500, 7500, 3000, 25000);
	RunIt(both, world);

	both.StartSearch(7500, 3000, 2500, 7500, 0, 25000);
	Check(RunIt(both, world) == kHudsonNavFound, "a second search on the same map finds the other floor");
	Check(both.Way().back().y == 0, "and ends on it");

	// Along the same line, one floor directly above the other
	both.StartSearch(7500, 3000, 16500, 7500, 3000, 25000);
	RunIt(both, world);

	std::vector<HudsonNavPoint> const lowerWay = both.Way();

	both.StartSearch(7500, 0, 16500, 7500, 0, 25000);
	RunIt(both, world);

	bool sameSpotTwoFloors = false;

	for (size_t i = 0; i + 1 < lowerWay.size(); i++)
	{
		for (size_t j = 0; j + 1 < both.Way().size(); j++)
		{
			sameSpotTwoFloors = sameSpotTwoFloors || (lowerWay[i].x == both.Way()[j].x && lowerWay[i].z == both.Way()[j].z && lowerWay[i].y == 3000 && both.Way()[j].y == 0);
		}
	}

	Check(sameSpotTwoFloors, "the same place at two different heights is two different spots, on one map");

	// Coming down, he takes the stairs, though stepping off the edge would be about as short
	{
		HudsonNavGraph down;

		down.StartSearch(7500, 0, 25000, 7500, 3000, 2500);
		Check(RunIt(down, world) == kHudsonNavFound, "there's a way down from the upper floor");

		bool neverDropped = true;

		for (size_t i = 0; i + 1 < down.Way().size(); i++)
		{
			neverDropped = neverDropped && down.Way()[i + 1].y - down.Way()[i].y <= kHudsonNavStepUp;
		}

		Check(neverDropped, "and he takes the stairs, because a drop counts as a long way round");
	}

	// And it doesn't get mixed up, going back the first way
	both.StartSearch(7500, 3000, 2500, 7500, 3000, 25000);
	Check(RunIt(both, world) == kHudsonNavFound && both.Way().back().y == 3000, "and the first floor is still there");
}

static void CheckSteps()
{
	printf("Steps up and down:\n");

	// A step as high as he can go, and one a millimetre higher, each right across a corridor
	for (int rise = kHudsonNavStepUp; rise <= kHudsonNavStepUp + 1; rise++)
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -rise, 0, 20000, 0, 4000);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, -rise, 1500);

		HudsonNavSearch const search = RunIt(map, world);

		if (rise == kHudsonNavStepUp)
		{
			Check(search == kHudsonNavFound, "a step as high as the game lifts him can be walked");
		}
		else
		{
			Check(search == kHudsonNavNoWay, "and one a millimetre higher can't");
		}
	}

	// A drop he can step off, and can't come back up
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Floor(10000, 0, 20000, 4000, 2500);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 2500, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "a drop can be stepped off");

		map.StartSearch(15500, 2500, 1500, 5500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and it only goes one way");
	}

	// A stride that takes in two stairs, each as high as he can go. A wall the same height, with nothing halfway up, isn't a way.
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);

		for (int stair = 0; stair < 10; stair++)
		{
			world.Add(5000 + stair * 500, -(stair + 1) * 250, 0, 20000, 0, 4000);
		}

		HudsonNavGraph map;

		map.StartSearch(2500, 0, 1500, 12500, -2500, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "stairs of 250 every half a metre, so 500 every stride, can be walked");

		World wall;

		wall.Floor(0, 0, 20000, 4000, 0);
		wall.Add(10000, -500, 0, 20000, 0, 4000);

		HudsonNavGraph stopped;

		stopped.StartSearch(5500, 0, 1500, 15500, -500, 1500);
		Check(RunIt(stopped, wall) == kHudsonNavNoWay, "and a single step of 500 can't");
	}

	// A step that's too high halfway and then comes down again isn't a stair either
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -600, 0, 10990, 0, 4000);
		world.Add(10990, -500, 0, 20000, 0, 4000);

		HudsonNavGraph map;

		map.StartSearch(9500, 0, 1500, 15500, -500, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a ledge higher than a step, halfway across a stride, isn't a stair");
	}

	// A drop further than he'd go
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Floor(10000, 0, 20000, 4000, kHudsonNavMostDrop + 100);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, kHudsonNavMostDrop + 100, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a drop further than he'd go isn't a way");
	}

	// A hole: nothing to stand on
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Floor(13000, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a gap in the floor isn't a way");

		// However a ray that found nothing says so
		WorldThatMisleads misleading(world);
		HudsonNavGraph misled;

		misled.StartSearch(5500, 0, 1500, 15500, 0, 1500);

		HudsonNavSearch search = kHudsonNavSearching;

		for (int i = 0; i < 100000 && search == kHudsonNavSearching; i++)
		{
			search = misled.Work(misleading, 1000000);
		}

		Check(search == kHudsonNavNoWay, "and a ray that found nothing is nothing, whatever else it says");
	}

	// A beam above head height and below the top of his head: the rays across all miss it, and only the one straight up finds there's no room for him
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10400, -kHudsonNavMarineHeight, 0, 10600, -kHudsonNavHeadHeight - 50, 4000);

		HudsonNavGraph map;

		Check(map.LineIsClear(world, 5500, 0, 1500, 15500, 1500, false), "a beam just over his eyes doesn't stop a line");

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "but there's no room for him under it");
	}

	// A ceiling too low
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -kHudsonNavMarineHeight + 100, 0, 11000, -kHudsonNavKneeHeight - 10, 4000);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a ceiling too low to get under isn't a way");
	}

	// A low wall: under his eyes, and what stuck him in CORR-17
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -kHudsonNavKneeHeight - 100, 0, 11000, 0, 4000);

		HudsonNavGraph map;

		Check(!map.LineIsClear(world, 5500, 0, 1500, 15500, 1500, false), "a low wall stops the line at his knees");
		Check(map.LineIsClear(world, 5500, -1000, 1500, 15500, 1500, false), "though a line higher up would miss it");

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and it isn't a way");
	}

	// A wall at head height only, leaving room to duck, which he can't
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -kHudsonNavHeadHeight - 100, 0, 11000, -kHudsonNavHeadHeight + 100, 4000);

		HudsonNavGraph map;

		Check(!map.LineIsClear(world, 5500, 0, 1500, 15500, 1500, false), "a beam stops the line at his head");
	}
}

static void CheckSlopes()
{
	printf("Slopes:\n");

	// One stride up is 500, with the floor facing the way a slope does. Boxes can't slope, so each rises as a stair would, and says it's a slope.
	struct Row { int faces; int rise; bool canGo; const char *what; };

	Row const rows[] =
	{
		{ -kHudsonNavFlatFloor + 1, 800, true, "a slope can rise further than a stair in one stride" },
		{ -kHudsonNavWalkable, 800, true, "the steepest he can walk on" },
		{ -kHudsonNavWalkable + 1, 100, false, "and any steeper can't be stood on, however little it rises" },
		{ -kHudsonNavFlatFloor, 800, false, "a floor that's flat, as high as that, is a wall and not a slope" },
		{ 0, 100, false, "something upright isn't a floor" },
		{ -kHudsonNavFlatFloor + 1, 2100, false, "and even a slope can only rise so far in one stride" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Add(10000, -rows[i].rise, 0, 12000, 0, 4000, false, rows[i].faces);

		HudsonNavGraph map;

		map.StartSearch(9500, 0, 1500, 11500, -rows[i].rise, 1500);

		// The goal's within reach of where he starts, so ask from further back, where he has to take the step to get there
		map.StartSearch(5500, 0, 1500, 11500, -rows[i].rise, 1500);

		HudsonNavSearch const search = RunIt(map, world);

		Check((search == kHudsonNavFound) == rows[i].canGo, rows[i].what);
	}

	// A slope too high for how short the step is. The floor he's on is so small that this is his only way off it.
	{
		World world;

		world.Add(9800, 0, 1000, 10000, 100, 2000);
		world.Add(10000, -1500, 1000, 12000, 0, 2000, false, -kHudsonNavFlatFloor + 1);

		HudsonNavGraph map;

		map.StartSearch(9900, 0, 1500, 11500, -1500, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a slope can only rise twice as far as the step is long");
	}

	// The map counts them apart
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Add(10000, -200, 0, 20000, 0, 4000, false, -kHudsonNavWalkable);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, -200, 1500);
		RunIt(map, world);

		Check(map.FlatSpots() > 0 && map.SlopeSpots() > 0, "flat spots and sloping ones are counted apart");
		Check(map.FlatSpots() + map.SlopeSpots() == map.Spots(), "and every spot is one or the other");
	}
}

static void CheckDoors()
{
	printf("Doors:\n");

	// Two rooms with a wall between, and a door in the wall
	World world;

	world.Floor(0, 0, 20000, 10000, 0);
	world.Wall(10000, 0, 10100, 4000);
	world.Wall(10000, 6000, 10100, 10000);
	world.Add(10000, -3000, 4000, 10100, 0, 6000, true);

	HudsonNavGraph map;

	map.StartSearch(5500, 0, 5500, 15500, 0, 5500);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way through a shut door, because doors open");
	Check(map.LineIsClear(world, 5500, 0, 5500, 15500, 5500, false), "and a line through one counts as clear");
	Check(!map.LineIsClear(world, 5500, 0, 5500, 15500, 5500, true), "unless it's asked to count them");

	// A door that isn't on the way, with an open way that's about as short: he takes the open way
	World twoWays;

	twoWays.Floor(0, 0, 20000, 10000, 0);
	twoWays.Wall(10000, 0, 10100, 3000);
	twoWays.Add(10000, -3000, 3000, 10100, 0, 5000, true);
	twoWays.Wall(10000, 5000, 10100, 6000);
	twoWays.Wall(10000, 8000, 10100, 10000);

	HudsonNavGraph choosing;

	choosing.StartSearch(5500, 0, 4500, 15500, 0, 4500);
	Check(RunIt(choosing, twoWays) == kHudsonNavFound, "with a door and an open way");

	bool usedTheOpening = false;

	for (size_t i = 0; i < choosing.Way().size(); i++)
	{
		usedTheOpening = usedTheOpening || (choosing.Way()[i].z >= 6000 && choosing.Way()[i].z <= 8000);
	}

	Check(usedTheOpening, "he goes round by the open way, when it's about as short");
}

static void CheckWhatChanges()
{
	printf("What changes between one search and the next:\n");

	// A wall that wasn't there before: a door has shut, or the room it's in has only just loaded
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "an open way");

		world.Wall(10000, 0, 10100, 4000);

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and with a wall that's come since, the old steps don't still count");
	}

	// A floor that wasn't there before: the room it's in has only just loaded
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "no way, with the far room not there");

		world.Floor(10000, 0, 20000, 4000, 0);

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "and once it's there, the spots he's looked at before are looked at again, and there is");
	}

	// A door that's gone: a way through it stops costing extra
	{
		World world;

		world.Floor(0, 0, 20000, 10000, 0);
		world.Wall(10000, 0, 10100, 3000);
		world.Wall(10000, 5000, 10100, 6000);
		world.Wall(10000, 8000, 10100, 10000);
		world.Add(10000, -3000, 3000, 10100, 0, 5000, true);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 4500, 15500, 0, 4500);
		RunIt(map, world);

		bool byTheOpening = false;

		for (size_t i = 0; i < map.Way().size(); i++)
		{
			byTheOpening = byTheOpening || (map.Way()[i].z >= 6000 && map.Way()[i].z <= 8000);
		}

		Check(byTheOpening, "with the door there, he goes round by the open way");

		world.RemoveLast();

		map.StartSearch(5500, 0, 4500, 15500, 0, 4500);
		RunIt(map, world);

		bool stillRound = false;

		for (size_t i = 0; i < map.Way().size(); i++)
		{
			stillRound = stillRound || (map.Way()[i].z >= 6000 && map.Way()[i].z <= 8000);
		}

		Check(!stillRound, "and with it gone, he goes straight through where it was");
	}

	// Which place on the grid he's in, on each side of zero: a spot already known on his side of zero is the one he starts from, and not one on the other side
	{
		World world;

		world.Floor(-20000, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(100, 0, 1500, -5500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "a way across zero");

		map.StartSearch(-100, 0, 1500, -5500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound && map.Way().front().x == -500, "and just below zero he's in the place on the grid below zero");
	}
}

static void CheckTheLimits()
{
	printf("The limits:\n");

	// Work uses about what it's told. One spot's worth over at most.
	{
		World world;
		AddRoom(world);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 5500, 35500, 0, 35500);

		int calls = 0;
		bool neverTooMany = true;
		HudsonNavSearch search = kHudsonNavSearching;

		while (search == kHudsonNavSearching && calls < 100000)
		{
			int const before = world.asked;

			search = map.Work(world, 48);
			calls++;
			neverTooMany = neverTooMany && world.asked - before <= 48 + kHudsonNavMostRaysForASpot;
		}

		Check(search == kHudsonNavFound, "a long way, a few rays at a time, gets there");
		Check(calls > 1, "and it did take more than one go");
		Check(neverTooMany, "and no go used more than it was told, and one spot's worth");
		Check(map.RaysUsed() == (uint64_t)world.asked, "and it counted them all");

		// The same way as with no limit: how fast it works doesn't change what it finds, so that two runs choose the same
		World same;
		AddRoom(same);

		HudsonNavGraph allAtOnce;

		allAtOnce.StartSearch(5500, 0, 5500, 35500, 0, 35500);
		RunIt(allAtOnce, same);

		bool sameWay = allAtOnce.Way().size() == map.Way().size();

		for (size_t i = 0; sameWay && i < map.Way().size(); i++)
		{
			sameWay = map.Way()[i].x == allAtOnce.Way()[i].x && map.Way()[i].y == allAtOnce.Way()[i].y && map.Way()[i].z == allAtOnce.Way()[i].z;
		}

		Check(sameWay, "and it's the same way as with all the rays at once");

		// Asking for nothing still gets somewhere, or it would never finish
		HudsonNavGraph slowest;
		World slow;
		AddRoom(slow);

		slowest.StartSearch(5500, 0, 5500, 8500, 0, 5500);

		HudsonNavSearch slowSearch = kHudsonNavSearching;

		for (int i = 0; i < 100 && slowSearch == kHudsonNavSearching; i++)
		{
			slowSearch = slowest.Work(slow, 0);
		}

		Check(slowSearch != kHudsonNavSearching, "being told it has no rays still gets it there in the end");
	}

	// A floor far too big, and nowhere to get to: it stops, at its limit
	{
		World world;

		world.Floor(-500000, -500000, 500000, 500000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 5500, 5500, -100000, 5500);

		HudsonNavSearch const search = RunIt(map, world);

		Check(search == kHudsonNavNoWay, "a floor with no end, and somewhere that can't be reached, ends with no way");
		Check(map.LastSearchLooks() <= kHudsonNavMostLooks, "and it looked at no more spots than it may");
		Check(map.Spots() <= kHudsonNavMostNodes, "and the map's no bigger than it may be");
	}

	// The map full: nothing new goes on it, and nothing goes wrong
	{
		World world;

		world.Floor(-500000, -500000, 500000, 500000, 0);

		HudsonNavGraph map;

		for (int i = 0; i < 12 && map.Spots() < kHudsonNavMostNodes; i++)
		{
			// A different place each time, each looking for somewhere it can't get to, so that each adds to the map
			map.StartSearch(-400000 + i * 70000, 0, -400000, 5500, -100000, 5500);
			RunIt(map, world);
		}

		Check(map.Spots() == kHudsonNavMostNodes, "searching and searching fills the map");

		map.StartSearch(300000, 0, 300000, 300000, -100000, 300000);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and with the map full, a search from a new place ends with no way");
		Check(map.Spots() == kHudsonNavMostNodes, "and the map's no bigger");
		Check(map.Way().empty(), "and there's no way to give back");
		Check(map.FurthestInSight(world, 300000, 0, 300000, 12) == -1, "and nothing to see of it");
		Check(map.NextAlong() == -1, "and no next point");

		map.Clear();
		Check(map.Spots() == 0 && map.RaysUsed() == 0 && map.Way().empty(), "and it can be cleared for the next level");

		map.StartSearch(5500, 0, 5500, 8500, 0, 5500);
		Check(RunIt(map, world) == kHudsonNavFound, "and it works again after that");
	}

	// Below zero, where rounding could go wrong
	{
		World world;

		world.Floor(-20000, -20000, 0, 0, 0);

		HudsonNavGraph map;

		map.StartSearch(-15500, 0, -15500, -5500, 0, -5500);
		Check(RunIt(map, world) == kHudsonNavFound, "the map works on the far side of zero");
		Check(WayIsClear(map, world), "with the steps all clear");

		for (size_t i = 1; i + 1 < map.Way().size(); i++)
		{
			int const x = map.Way()[i].x, z = map.Way()[i].z;

			Check((-x) % kHudsonNavGridStep == kHudsonNavGridStep / 2 && (-z) % kHudsonNavGridStep == kHudsonNavGridStep / 2, "and each spot is in the middle of its place on the grid");
		}
	}

	// He can stand anywhere, on or off the grid, and a search with no distance at all finishes
	{
		World world;
		AddRoom(world);

		HudsonNavGraph map;

		map.StartSearch(5123, 0, 7777, 5123, 0, 7777);
		Check(RunIt(map, world) == kHudsonNavFound, "going nowhere is found straight away");
		Check(map.LineIsClear(world, 5000, 0, 5000, 5000, 5000, false), "and a line of no length is clear");

		// Called again when it's done: it just says how it ended
		Check(map.Work(world, 48) == kHudsonNavFound, "asking again says it's found");
	}

	// Going along the way: he can't go backwards on it, and being off it doesn't crash
	{
		World world;
		AddRoom(world);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 5500, 25500, 0, 5500);
		RunIt(map, world);

		int const first = map.FurthestInSight(world, 5500, 0, 5500, 12);

		Check(first > 0, "with an open floor he can see well along it");
		Check(first <= 12 && (size_t)first < map.Way().size(), "but no further than he asked");

		int const further = map.FurthestInSight(world, 15500, 0, 5500, 12);

		Check(further >= first, "and further along it he sees further on");
		Check(map.FurthestInSight(world, 15500, 0, 5500, 0) >= 0, "and asking for nothing ahead still gives where he is");
		Check(map.FurthestInSight(world, 15500, -5000, 5500, 12) == -1, "and from another floor none of it counts as seen");
	}
}

int main()
{
	CheckTheStraightWay();
	CheckGoingRoundACorner();
	CheckFloorsAboveEachOther();
	CheckSteps();
	CheckSlopes();
	CheckDoors();
	CheckWhatChanges();
	CheckTheLimits();

	printf("%d checks, %d failed\n", checksDone, checksFailed);

	if (checksFailed != 0 || checksDone == 0)
	{
		printf("The test FAILED\n");
		return 1;
	}

	printf("The test passed\n");
	return 0;
}
