// The test for 3dc/avp/hudson_grid.cpp. Written 2026-11-05 by Melanikus for the AvP port.
//
// It builds small made-up worlds out of polygons and asks the grid about them. There's no game and no graphics card. The numbers are the marine's: a square 450 mm either way from
// his middle, 1950 tall, stepping up to 450. Each world uses walls that land on a column's edge the same way every time, so that a test isn't passing by luck of where the columns fall.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "hudson_grid.h"
#include "hudson_nav_save.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

typedef std::vector<HudsonGridPolygon> World;

// A flat floor, at floor height y. Remember y goes down, so a higher floor has a smaller y.
static void AddFloor(World *world, int x0, int z0, int x1, int z1, int y, bool crawl = false)
{
	HudsonGridPolygon p;

	memset(&p, 0, sizeof(p));
	p.corners = 4;
	p.x[0] = x0; p.z[0] = z0;
	p.x[1] = x1; p.z[1] = z0;
	p.x[2] = x1; p.z[2] = z1;
	p.x[3] = x0; p.z[3] = z1;

	for (int i = 0; i < 4; i++)
	{
		p.y[i] = y;
	}

	p.up = 1.0f;
	p.blocks = true;
	p.canStandOn = true;
	p.crawl = crawl;
	world->push_back(p);
}

// A ceiling: flat, and facing down
static void AddCeiling(World *world, int x0, int z0, int x1, int z1, int y)
{
	AddFloor(world, x0, z0, x1, z1, y);
	world->back().up = -1.0f;
	world->back().canStandOn = false;
}

// A wall from one point to another, from a floor up to a top. y goes down, so top is the smaller number.
static void AddWall(World *world, int x0, int z0, int x1, int z1, int bottom, int top)
{
	HudsonGridPolygon p;

	memset(&p, 0, sizeof(p));
	p.corners = 4;
	p.x[0] = x0; p.z[0] = z0; p.y[0] = bottom;
	p.x[1] = x1; p.z[1] = z1; p.y[1] = bottom;
	p.x[2] = x1; p.z[2] = z1; p.y[2] = top;
	p.x[3] = x0; p.z[3] = z0; p.y[3] = top;
	p.up = 0.0f;
	p.blocks = true;
	p.canStandOn = false;
	world->push_back(p);
}

// Runs a search to its end, one big go
static HudsonGridSearch Search(HudsonGrid &grid, int fromX, int fromY, int fromZ, int toX, int toY, int toZ)
{
	grid.StartSearch(fromX, fromY, fromZ, toX, toY, toZ);
	return grid.Work(100000000);
}

static void TestAFloor()
{
	World world;
	HudsonGrid grid;

	AddFloor(&world, 0, 0, 12000, 12000, 1000);
	Check(grid.Build(world) == 0, "a plain floor builds");
	Check(grid.Ready(), "and it's ready");
	Check(grid.Places() > 1000, "and has places to stand: " + std::to_string(grid.Places()));
	Check(Search(grid, 1500, 1000, 1500, 10500, 1000, 10500) == kHudsonGridFound, "a way across it is found");
	Check(grid.Way().size() >= 2, "with points on it");
	Check(grid.Way().back().x == 10500 && grid.Way().back().z == 10500, "and the last point is the goal itself");
	Check(grid.Way().back().y == 1000, "at floor height");
	Check(grid.GoalWasFound() && grid.GoalFloorY() == 1000, "and it says which floor it took");
	Check(grid.StartWasFound(), "and that it found one where he started");

	// About a point a metre: the way is about 12.7 m corner to corner
	Check(grid.Way().size() >= 8 && grid.Way().size() <= 20, "about a point a metre: " + std::to_string(grid.Way().size()));

	// Nothing but the floor in the way, so the way shouldn't wander far off the straight line
	bool nearTheLine = true;

	for (size_t i = 0; i < grid.Way().size(); i++)
	{
		nearTheLine = nearTheLine && abs(grid.Way()[i].x - grid.Way()[i].z) < 3000;
	}

	Check(nearTheLine, "and it stays near the straight line");
}

static void TestAskedAgain()
{
	World world;
	HudsonGrid grid;

	AddFloor(&world, 0, 0, 12000, 12000, 1000);
	grid.Build(world);

	grid.StartSearch(1500, 1000, 1500, 10500, 1000, 10500);

	HudsonGridSearch state = grid.Work(1);
	int calls = 1;

	Check(state == kHudsonGridSearching, "one place at a time isn't enough to finish");

	while (state == kHudsonGridSearching && calls < 100000)
	{
		state = grid.Work(1);
		calls++;
	}

	Check(state == kHudsonGridFound && calls > 5, "but it gets there, over many goes: " + std::to_string(calls));
	Check(grid.Work(1) == kHudsonGridFound, "and asked again, it gives the same answer");
	Check(calls == grid.LastSearchLooks(), "looking at one place each go");
}

static void TestAWall()
{
	World world;
	HudsonGrid grid;

	AddFloor(&world, 0, 0, 12000, 6000, 1000);
	AddWall(&world, 6000, 0, 6000, 6000, 1000, -3000);
	AddWall(&world, 0, 0, 12000, 0, 1000, -3000);
	AddWall(&world, 0, 6000, 12000, 6000, 1000, -3000);
	grid.Build(world);

	Check(Search(grid, 2000, 1000, 3000, 10000, 1000, 3000) == kHudsonGridNoWay, "a wall all the way across leaves no way");
	Check(!grid.Way().empty(), "but the way goes as near as it got");
	Check(grid.Way().back().x <= 6000, "which is on his own side of the wall: " + std::to_string(grid.Way().back().x));
	Check(grid.Way().back().x >= 4800, "and right up by it");

	// The same wall with a gap
	World gap;

	AddFloor(&gap, 0, 0, 12000, 6000, 1000);
	AddWall(&gap, 6000, 0, 6000, 2000, 1000, -3000);
	AddWall(&gap, 6000, 4000, 6000, 6000, 1000, -3000);
	grid.Build(gap);
	Check(Search(grid, 2000, 1000, 3000, 10000, 1000, 3000) == kHudsonGridFound, "a gap 2 m wide lets him through");

	bool through = false;

	for (size_t i = 0; i < grid.Way().size(); i++)
	{
		through = through || (abs(grid.Way()[i].x - 6000) < 900 && grid.Way()[i].z > 2000 && grid.Way()[i].z < 4000);
	}

	Check(through, "and that's the way it goes");

	// Too narrow for his square, wherever the columns fall. It's the gap's size that matters, not where it is, so try it at several places.
	for (int offset = 0; offset < 300; offset += 75)
	{
		World narrow;

		AddFloor(&narrow, 0, 0, 12000, 6000, 1000);
		AddWall(&narrow, 6000, 0, 6000, 2600 + offset, 1000, -3000);
		AddWall(&narrow, 6000, 3400 + offset, 6000, 6000, 1000, -3000);
		grid.Build(narrow);
		Check(Search(grid, 2000, 1000, 3000, 10000, 1000, 3000) == kHudsonGridNoWay, "a gap 800 wide is too narrow for a marine 900 wide, offset " + std::to_string(offset));
	}
}

static void TestHeights()
{
	HudsonGrid grid;

	// A low ceiling over the middle of a floor
	{
		World world;

		AddFloor(&world, 0, 0, 12000, 3000, 1000);
		AddCeiling(&world, 4000, 0, 8000, 3000, 1000 - 1900);
		AddWall(&world, 0, 0, 12000, 0, 1000, -4000);
		AddWall(&world, 0, 3000, 12000, 3000, 1000, -4000);
		grid.Build(world);
		Check(Search(grid, 1000, 1000, 1500, 11000, 1000, 1500) == kHudsonGridNoWay, "a ceiling 1900 up is too low for someone 1950 tall");
	}

	{
		World world;

		AddFloor(&world, 0, 0, 12000, 3000, 1000);
		AddCeiling(&world, 4000, 0, 8000, 3000, 1000 - 2000);
		AddWall(&world, 0, 0, 12000, 0, 1000, -4000);
		AddWall(&world, 0, 3000, 12000, 3000, 1000, -4000);
		grid.Build(world);
		Check(Search(grid, 1000, 1000, 1500, 11000, 1000, 1500) == kHudsonGridFound, "and one 2000 up isn't");
	}

	// In an air duct he crawls, and fits under 1200. Only where the floor's marked.
	{
		World world;

		AddFloor(&world, 0, 0, 4000, 3000, 1000);
		AddFloor(&world, 4000, 0, 8000, 3000, 1000, true);
		AddFloor(&world, 8000, 0, 12000, 3000, 1000);
		AddCeiling(&world, 4000, 0, 8000, 3000, 1000 - 1300);
		grid.Build(world);
		Check(Search(grid, 1000, 1000, 1500, 11000, 1000, 1500) == kHudsonGridFound, "a duct 1300 high is fine, because he crawls in it");
	}

	{
		World world;

		AddFloor(&world, 0, 0, 12000, 3000, 1000, true);
		AddCeiling(&world, 4000, 0, 8000, 3000, 1000 - 1100);
		grid.Build(world);
		Check(Search(grid, 1000, 1000, 1500, 11000, 1000, 1500) == kHudsonGridNoWay, "but not one 1100 high");
	}

	// Two floors, one over the other, are different places
	{
		World world;

		AddFloor(&world, 0, 0, 6000, 6000, 1000);
		AddFloor(&world, 0, 0, 6000, 6000, 1000 - 4000);
		grid.Build(world);
		Check(Search(grid, 3000, 1000, 3000, 3000, -3000, 3000) == kHudsonGridNoWay, "two floors with no way between them don't join, though they're in the same columns");
		Check(Search(grid, 1500, -3000, 1500, 4500, -3000, 4500) == kHudsonGridFound && grid.Way().back().y == -3000, "and walking on the upper one stays on it");
		Check(Search(grid, 1500, 1000, 1500, 4500, 1000, 4500) == kHudsonGridFound && grid.Way().back().y == 1000, "and on the lower one");
	}
}

// Stairs, going up along x, each one rise high and tread deep
static void AddStairs(World *world, int count, int rise, int tread)
{
	AddFloor(world, 0, 0, 3000, 3000, 1000);

	for (int i = 0; i < count; i++)
	{
		AddFloor(world, 3000 + i * tread, 0, 3000 + (i + 1) * tread, 3000, 1000 - (i + 1) * rise);
		AddWall(world, 3000 + i * tread, 0, 3000 + i * tread, 3000, 1000 - i * rise, 1000 - (i + 1) * rise);
	}

	int const end = 3000 + count * tread;

	AddFloor(world, end, 0, end + 3000, 3000, 1000 - count * rise);
}

static void TestStairs()
{
	HudsonGrid grid;

	{
		// The real ones, COMM-ENT on Derelict
		World world;

		AddStairs(&world, 20, 390, 780);
		grid.Build(world);

		int const top = 1000 - 20 * 390;

		Check(Search(grid, 1500, 1000, 1500, 3000 + 20 * 780 + 1500, top, 1500) == kHudsonGridFound, "stairs like Derelict's, 390 up and 780 along, can be climbed");
		Check(grid.Way().back().y == top, "all the way to the top");

		int biggest = 0;

		for (size_t i = 1; i < grid.Way().size(); i++)
		{
			biggest = (std::max)(biggest, abs(grid.Way()[i].y - grid.Way()[i - 1].y));
		}

		Check(biggest <= 450 * 2, "and the way climbs them a little at a time: " + std::to_string(biggest));
		Check(Search(grid, 3000 + 20 * 780 + 1500, top, 1500, 1500, 1000, 1500) == kHudsonGridFound, "and they can be walked down");
	}

	{
		World world;

		AddStairs(&world, 6, 460, 780);
		grid.Build(world);
		Check(Search(grid, 1500, 1000, 1500, 3000 + 6 * 780 + 1500, 1000 - 6 * 460, 1500) == kHudsonGridNoWay, "stairs 460 up are more than the game lifts him");
	}
}

// A ramp going up along x: from a floor at y 1000, across 6 m, up by climb
static void AddRamp(World *world, int climb, float up)
{
	AddFloor(world, 0, 0, 3000, 3000, 1000);

	HudsonGridPolygon ramp;

	memset(&ramp, 0, sizeof(ramp));
	ramp.corners = 4;
	ramp.x[0] = 3000; ramp.z[0] = 0; ramp.y[0] = 1000;
	ramp.x[1] = 9000; ramp.z[1] = 0; ramp.y[1] = 1000 - climb;
	ramp.x[2] = 9000; ramp.z[2] = 3000; ramp.y[2] = 1000 - climb;
	ramp.x[3] = 3000; ramp.z[3] = 3000; ramp.y[3] = 1000;
	ramp.up = up;
	ramp.blocks = true;
	ramp.canStandOn = true;
	world->push_back(ramp);
	AddFloor(world, 9000, 0, 12000, 3000, 1000 - climb);
}

static void TestSlopes()
{
	HudsonGrid grid;

	{
		// About 15 degrees: nearly flat, by the game's way of looking at it
		World world;

		AddRamp(&world, 1600, 0.966f);
		grid.Build(world);
		Check(Search(grid, 1500, 1000, 1500, 10500, 1000 - 1600, 1500) == kHudsonGridFound, "a gentle slope can be walked up");
		Check(Search(grid, 10500, 1000 - 1600, 1500, 1500, 1000, 1500) == kHudsonGridFound, "and down");
	}

	{
		// About 45 degrees, like the one on Sulaco that stopped him
		World world;

		AddRamp(&world, 6000, 0.707f);
		grid.Build(world);
		Check(Search(grid, 1500, 1000, 1500, 10500, 1000 - 6000, 1500) == kHudsonGridNoWay, "a steep slope can't be walked up");
		Check(Search(grid, 10500, 1000 - 6000, 1500, 1500, 1000, 1500) == kHudsonGridFound, "but it can be walked down");
	}

	{
		// Steeper than the game calls floor at all
		World world;

		AddRamp(&world, 6000, 0.3f);
		grid.Build(world);
		Check(Search(grid, 10500, 1000 - 6000, 1500, 1500, 1000, 1500) == kHudsonGridNoWay, "too steep to be floor can't be walked at all");
	}
}

static void TestDrops()
{
	HudsonGrid grid;

	// A ledge, and a floor below it
	{
		World world;

		AddFloor(&world, 0, 0, 6000, 6000, -4000);
		AddFloor(&world, 6000, 0, 12000, 6000, 1000);
		grid.Build(world);
		Check(Search(grid, 2000, -4000, 3000, 10000, 1000, 3000) == kHudsonGridFound, "a 5 m drop can be taken");
		Check(grid.Drops() > 0, "so it's counted as one");
		Check(Search(grid, 10000, 1000, 3000, 2000, -4000, 3000) == kHudsonGridNoWay, "but only one way");

		// The drop is both ends of a jump, so the way has to hold both
		Search(grid, 2000, -4000, 3000, 10000, 1000, 3000);

		bool edgeKept = false;

		for (size_t i = 1; i < grid.Way().size(); i++)
		{
			edgeKept = edgeKept || (grid.Way()[i - 1].y == -4000 && grid.Way()[i].y == 1000 && abs(grid.Way()[i].x - grid.Way()[i - 1].x) <= 600);
		}

		Check(edgeKept, "and the way keeps the edge and the landing, right next to each other");
	}

	// Stairs beside a drop: he takes the stairs, even though the drop is shorter, because a fall hurts
	{
		World world;

		// A high floor, 6 m up, with an edge he can drop off. Further along it, stairs go down from the same edge.
		AddFloor(&world, 0, 0, 3000, 9000, -5000);

		for (int i = 0; i < 15; i++)
		{
			AddFloor(&world, 3000 + i * 300, 6000, 3000 + (i + 1) * 300, 9000, -5000 + (i + 1) * 400);
		}

		AddFloor(&world, 3000, 0, 7500, 6000, 1000);
		AddFloor(&world, 7500, 0, 20000, 9000, 1000);
		grid.Build(world);
		Check(Search(grid, 1500, -5000, 1500, 14000, 1000, 1500) == kHudsonGridFound, "a drop and some stairs both lead down");

		bool dropped = false;

		for (size_t i = 1; i < grid.Way().size(); i++)
		{
			dropped = dropped || grid.Way()[i].y - grid.Way()[i - 1].y > 3000;
		}

		Check(!dropped, "and he takes the stairs");
	}

	// Too far: 8 m is what kills him
	{
		World world;

		AddFloor(&world, 0, 0, 6000, 6000, -7000);
		AddFloor(&world, 6000, 0, 12000, 6000, 1000);
		grid.Build(world);
		Check(Search(grid, 2000, -7000, 3000, 10000, 1000, 3000) == kHudsonGridNoWay, "a drop of 8 m is one he won't take");
		Check(grid.Drops() == 0, "so it isn't one");
	}

	// A drop with a wall right at the edge isn't one
	{
		World world;

		AddFloor(&world, 0, 0, 6000, 6000, -4000);
		AddFloor(&world, 6000, 0, 12000, 6000, 1000);
		AddWall(&world, 6000, 0, 6000, 6000, 1000, -8000);
		grid.Build(world);
		Check(Search(grid, 2000, -4000, 3000, 10000, 1000, 3000) == kHudsonGridNoWay, "a wall at the edge stops him stepping off it");
	}
}

static void TestCorners()
{
	HudsonGrid grid;
	World world;

	// An L-shaped corridor, 3 m wide. The inside corner's at 3000, 3000. The floor only exists in the L.
	AddFloor(&world, 0, 0, 3000, 12000, 1000);
	AddFloor(&world, 3000, 0, 12000, 3000, 1000);
	AddWall(&world, 3000, 3000, 12000, 3000, 1000, -3000);
	AddWall(&world, 3000, 3000, 3000, 12000, 1000, -3000);
	grid.Build(world);

	Check(Search(grid, 1500, 1000, 10500, 10500, 1000, 1500) == kHudsonGridFound, "round an L-shaped corner");

	// Stays out of the wall by his square: nothing inside the corner, or within 450 of it
	bool clear = true;

	for (size_t i = 0; i < grid.Way().size(); i++)
	{
		HudsonGridPoint const &p = grid.Way()[i];

		clear = clear && !(p.x > 3000 - 450 && p.z > 3000 - 450);
	}

	Check(clear, "and it keeps his square off the corner");
}

// Two floors whose columns only meet corner to corner, with a gap between the floors themselves. Going diagonally needs the two columns beside it as well, and those have no floor.
static void TestOnlyTouchingAtACorner()
{
	HudsonGrid grid;
	World world;

	AddFloor(&world, 0, 0, 2900, 2900, 1000);
	AddFloor(&world, 3100, 3100, 6000, 6000, 1000);
	grid.Build(world);
	Check(Search(grid, 1500, 1000, 1500, 4500, 1000, 4500) == kHudsonGridNoWay, "columns that only meet at a corner don't join");
}

static void TestTheMiddle()
{
	HudsonGrid grid;
	World world;

	// A hall 9 m wide. He starts and ends against one wall. Walking straight along it would be shortest, but the middle of the room costs less.
	AddFloor(&world, 0, 0, 30000, 9000, 1000);
	AddWall(&world, 0, 0, 30000, 0, 1000, -3000);
	AddWall(&world, 0, 9000, 30000, 9000, 1000, -3000);
	grid.Build(world);
	Check(Search(grid, 1500, 1000, 700, 28500, 1000, 700) == kHudsonGridFound, "along a wall in a wide hall");

	int furthest = 0;

	for (size_t i = 0; i < grid.Way().size(); i++)
	{
		furthest = (std::max)(furthest, grid.Way()[i].z);
	}

	Check(furthest >= 1500, "he walks well away from the wall: " + std::to_string(furthest));
	Check(furthest <= 3000, "but not further out than he has to");
}

static void TestSpots()
{
	HudsonGrid grid;

	// The top of a lamp, in a room. A spot in the air above it means the floor, not the lamp.
	{
		World world;

		AddFloor(&world, 0, 0, 12000, 12000, 1000);
		AddFloor(&world, 5400, 5400, 6600, 6600, -1500);    // the lamp's top: a square 1.2 m across, 2.5 m up
		grid.Build(world);
		Check(Search(grid, 1500, 1000, 1500, 6000, -3000, 6000) == kHudsonGridFound, "a way to a spot high above a lamp");
		Check(grid.GoalFloorY() == 1000, "goes to the floor, not the top of the lamp: " + std::to_string(grid.GoalFloorY()));
	}

	// A spot in the air, over a floor
	{
		World world;

		AddFloor(&world, 0, 0, 12000, 12000, 1000);
		grid.Build(world);
		Check(Search(grid, 1500, 1000, 1500, 9000, -7000, 9000) == kHudsonGridFound, "a spot 8 m up means the floor under it");
		Check(grid.GoalWasFound() && grid.GoalFloorY() == 1000, "and it says that's the floor it took");

		// How far a spot reaches. 30 m up is the limit. These are the numbers from hudson_grid.h: far enough for a join between rooms high in the air, but not for just anything.
		Check(Search(grid, 1500, 1000, 1500, 9000, 1000 - 29000, 9000) == kHudsonGridFound, "a spot 29 m up still means the floor under it");
		Check(Search(grid, 1500, 1000, 1500, 9000, 1000 - 31000, 9000) == kHudsonGridNoWay && !grid.GoalWasFound(), "but one 31 m up means nothing");
		Check(Search(grid, 1500, 1000, 1500, 9000, 1000 + 2900, 9000) == kHudsonGridFound, "a spot 2.9 m below a floor means that floor");
		Check(Search(grid, 1500, 1000, 1500, 9000, 1000 + 3100, 9000) == kHudsonGridNoWay && !grid.GoalWasFound(), "but one 3.1 m below it doesn't");

		// Sideways it's a circle of 3 m, not the square it looks in. This spot is in that square's corner.
		Check(Search(grid, 1500, 1000, 1500, 12000 + 2000, 1000, 12000 + 2000) == kHudsonGridFound, "a spot 2.8 m off the corner of the floor still means that floor");
		Check(Search(grid, 1500, 1000, 1500, 12000 + 2400, 1000, 12000 + 2400) == kHudsonGridNoWay && !grid.GoalWasFound(), "but one 3.3 m off doesn't");

		// Nowhere near any floor
		Check(Search(grid, 1500, 1000, 1500, 50000, 1000, 50000) == kHudsonGridNoWay, "a spot nowhere near a floor has no way");
		Check(!grid.GoalWasFound(), "and it says the goal was the trouble");
		Check(grid.StartWasFound(), "not the start");
		Check(Search(grid, 50000, 1000, 50000, 1500, 1000, 1500) == kHudsonGridNoWay && !grid.StartWasFound(), "and the other way round");
		Check(grid.Way().empty(), "with no way at all, because it never started");
	}
}

static void TestFacing()
{
	HudsonGridPolygon p;

	memset(&p, 0, sizeof(p));
	p.corners = 4;

	// A floor whose first three corners are in a line, as one on Derelict really is. These run the way a floor's do in the stock levels.
	int const xs[4] = { 0, 3000, 6000, 3000 };
	int const zs[4] = { 0, 0, 0, 3000 };

	for (int i = 0; i < 4; i++)
	{
		p.x[i] = xs[i];
		p.z[i] = zs[i];
		p.y[i] = 1000;
	}

	float const up = HudsonGrid_HowMuchItFacesUp(p);

	Check(up > 0.99f || up < -0.99f, "three corners in a line still give a facing: " + std::to_string(up));

	// The same polygon wound the other way is the opposite
	HudsonGridPolygon q = p;

	for (int i = 0; i < 4; i++)
	{
		q.x[i] = p.x[3 - i];
		q.z[i] = p.z[3 - i];
	}

	Check(HudsonGrid_HowMuchItFacesUp(q) == -up, "and winding it the other way turns it over");

	// A wall
	HudsonGridPolygon w;

	memset(&w, 0, sizeof(w));
	w.corners = 4;
	w.x[1] = 3000; w.x[2] = 3000;
	w.y[2] = -2000; w.y[3] = -2000;
	Check(HudsonGrid_HowMuchItFacesUp(w) == 0.0f, "a wall doesn't face up at all");

	// Nothing at all
	HudsonGridPolygon none;

	memset(&none, 0, sizeof(none));
	none.corners = 3;
	Check(HudsonGrid_HowMuchItFacesUp(none) == 0.0f, "a polygon with no size gives no facing, and no nonsense");
}

// What a crafted level could try. Each one has to end in a plain refusal or a plain leaving out, and never in a crash or running out of memory.
static void TestHostile()
{
	HudsonGrid grid;

	{
		World world;

		Check(std::string(grid.Build(world) ? grid.Build(world) : "") == "the level has no polygons the grid can use", "no polygons at all");
		Check(!grid.Ready(), "leaves no map");
	}

	// Corner counts that aren't: a polygon with too few, too many, or a nonsense number
	{
		World world;

		AddFloor(&world, 0, 0, 3000, 3000, 1000);

		HudsonGridPolygon bad = world[0];

		// With this at 0, a corner read past the end of the list looks like a good one, so only the count itself can catch it
		bad.up = 0.0f;
		bad.corners = 2;
		world.push_back(bad);
		bad.corners = kHudsonGridMostCorners + 1;
		world.push_back(bad);
		bad.corners = -5;
		world.push_back(bad);
		bad.corners = 2000000000;
		world.push_back(bad);
		Check(grid.Build(world) == 0, "polygons with nonsense corner counts don't stop the rest");
		Check(grid.PolygonsLeftOut() == 4, "they're left out: " + std::to_string(grid.PolygonsLeftOut()));
		Check(grid.PolygonsUsed() == 1, "and the good one is used");
	}

	// Corners a very long way away
	{
		World world;

		AddFloor(&world, 0, 0, 3000, 3000, 1000);
		AddFloor(&world, 2000000000, 2000000000, 2000000100, 2000000100, 1000);
		AddFloor(&world, -2147483647, -2147483647, -2147483000, -2147483000, 1000);
		Check(grid.Build(world) == 0, "corners at the edge of what a number can hold are left out, and don't overflow anything");
		Check(grid.PolygonsLeftOut() == 2, "both of them");
	}

	// A sky: enormous, a long way above everything. It's left out, and the level still works.
	{
		World world;

		AddFloor(&world, 0, 0, 12000, 12000, 1000);
		AddFloor(&world, -500000, -500000, 500000, 500000, -80000);
		Check(grid.Build(world) == 0, "a level with a sky builds");
		Check(grid.PolygonsLeftOut() == 1, "without it");
		Check(grid.Places() < 2000, "and the sky adds no places: " + std::to_string(grid.Places()));
	}

	// Two rooms too far apart for the grid
	{
		World world;

		AddFloor(&world, 0, 0, 3000, 3000, 1000);
		AddFloor(&world, 11000000, 0, 11003000, 3000, 1000);

		char const *const answer = grid.Build(world);

		Check(answer && std::string(answer) == "the level is wider than the grid takes", std::string("rooms 11 km apart are refused: ") + (answer ? answer : "built"));
		Check(!grid.Ready(), "and leave no map");
		Check(grid.Places() == 0, "and nothing of one");
	}

	// Many big polygons, each under the limit for one, that together would take all the memory there is
	{
		World world;

		for (int i = 0; i < 2000; i++)
		{
			AddFloor(&world, 0, 0, 60000, 60000, 1000 - i * 2500);
		}

		char const *const answer = grid.Build(world);

		Check(answer && std::string(answer) == "the level's polygons cross more columns than the grid takes", std::string("too much to mark is refused, not run out of memory on: ") + (answer ? answer : "built"));
		Check(!grid.Ready(), "and leaves no map");
	}

	// Each limit, with its own reason. Lowered, so that this doesn't need millions of polygons.
	{
		World world;

		AddFloor(&world, 0, 0, 6000, 6000, 1000);
		AddFloor(&world, 6000, 0, 12000, 6000, 1000);

		struct LimitCase
		{
			size_t polygons, bits, places, moves;
			const char *reason;
		};

		LimitCase const cases[] = {
			{ 1, 8000000, 2000000, 20000000, "the level has more polygons than the grid takes" },
			{ 2000000, 100, 2000000, 20000000, "the level's polygons cross more columns than the grid takes" },
			{ 2000000, 8000000, 100, 20000000, "the level has more places to stand than the grid takes" },
			{ 2000000, 8000000, 2000000, 100, "the level has more moves than the grid takes" },
		};

		for (LimitCase const &limit : cases)
		{
			HudsonGrid limited;

			limited.LowerLimitsForATest(limit.polygons, limit.bits, limit.places, limit.moves);

			char const *const answer = limited.Build(world);

			Check(answer && std::string(answer) == limit.reason, std::string("a limit gives its own reason: ") + (answer ? answer : "it built"));
			Check(!limited.Ready() && limited.Places() == 0 && limited.Moves() == 0, "and leaves nothing of a map");
		}

		HudsonGrid roomy;

		roomy.LowerLimitsForATest(2, 8000000, 2000000, 20000000);
		Check(roomy.Build(world) == 0, "exactly as many polygons as the limit is fine");
	}

	// Searching with no map
	{
		HudsonGrid empty;

		empty.StartSearch(0, 0, 0, 1000, 0, 1000);
		Check(empty.Work(1000) == kHudsonGridNoWay, "a search with no map finds no way");
		Check(empty.Way().empty(), "and has none");

		HudsonGridCell cells[4];

		Check(empty.CellsNear(0, 0, 0, 5000, cells, 4) == 0, "and there's nothing to draw");
	}

	// A search far outside the grid, or at the ends of what a number can hold
	{
		World world;

		AddFloor(&world, 0, 0, 3000, 3000, 1000);
		grid.Build(world);
		grid.StartSearch(2147483647, 2147483647, 2147483647, -2147483647, -2147483647, -2147483647);
		Check(grid.Work(1000) == kHudsonGridNoWay, "a search for places nowhere near finds no way, and doesn't overflow");

		HudsonGridCell cells[4];

		Check(grid.CellsNear(2147483647, 0, 2147483647, 5000, cells, 4) == 0, "and asking what's near there finds nothing");
		Check(grid.CellsNear(1500, 1000, 1500, 0, cells, 4) == 0, "and asking for nothing gets nothing");
		Check(grid.CellsNear(1500, 1000, 1500, 5000, cells, 0) == 0, "and asking for no room writes nothing");
	}
}

static void TestCells()
{
	HudsonGrid grid;
	World world;

	AddFloor(&world, 0, 0, 6000, 6000, -4000);
	AddFloor(&world, 6000, 0, 12000, 6000, 1000);
	grid.Build(world);

	std::vector<HudsonGridCell> cells(4096);
	int const count = grid.CellsNear(6000, -4000, 3000, 3000, &cells[0], (int)cells.size());

	Check(count > 0 && count < (int)cells.size(), "the cells near a point: " + std::to_string(count));

	bool foundDrop = false, linkGood = true, nearEnough = true;

	for (int i = 0; i < count; i++)
	{
		if (cells[i].kind == kHudsonGridDrop)
		{
			foundDrop = true;
			linkGood = linkGood && cells[i].link >= 0 && cells[i].link < (int)grid.Places();
		}

		nearEnough = nearEnough && abs(cells[i].y + 4000) <= 3000;
	}

	Check(foundDrop, "include where he can drop");
	Check(linkGood, "and each says where he lands");
	Check(nearEnough, "and none from a floor further away than that");

	HudsonGridCell few[3];

	Check(grid.CellsNear(6000, -4000, 3000, 3000, few, 3) == 3, "and it stops at the room it's given");
}

// Reading a place by its number, which is how a run saves its map
static void TestCellAt()
{
	HudsonGrid grid;
	World world;
	HudsonGridCell cell;

	Check(!grid.CellAt(0, &cell), "with no map there's no place 0");

	AddFloor(&world, 0, 0, 6000, 6000, 1000);
	grid.Build(world);
	Check(grid.CellAt(0, &cell) && cell.y == 1000 && cell.number == 0, "the first place is on the floor");
	Check(cell.kind == kHudsonGridFlat, "and it's flat");
	Check(grid.CellAt((int)grid.Places() - 1, &cell), "and so is the last");
	Check(!grid.CellAt((int)grid.Places(), &cell), "one more isn't a place");
	Check(!grid.CellAt(-1, &cell), "and nor is -1");
	Check(!grid.CellAt(2147483647, &cell), "or the biggest number there is");
}

static std::string ReadWholeFile(const char *path)
{
	std::string text;
	FILE *const file = fopen(path, "rb");

	if (file)
	{
		char some[256];
		size_t got;

		while ((got = fread(some, 1, sizeof(some), file)) > 0)
		{
			text.append(some, got);
		}

		fclose(file);
	}

	return text;
}

// Saving a run's paths. It only ever writes, and nothing it meets can stop the run.
static void TestSaving()
{
	char const *const path = "hudson_grid_test_saved.tmp";

	{
		HudsonNavSaveFile file;

		Check(file.Open(path, 1000), "a file opens");
		file.Line("%d %d %d %s %d", 1, 2, 3, "flat", -1);
		file.Line("way %d", 7);
		file.Close();
		Check(ReadWholeFile(path) == "1 2 3 flat -1\nway 7\n" || ReadWholeFile(path) == "1 2 3 flat -1\r\nway 7\r\n", "and holds what it was given, a line each");
		Check(file.WhyItStopped() == 0, "and it didn't stop early");
		Check(!file.IsOpen(), "and it's closed");
		Check(file.Bytes() == 20, "and it knows how much it wrote: " + std::to_string(file.Bytes()));
	}

	// Stopping at its limit
	{
		HudsonNavSaveFile file;

		file.Open(path, 12);
		file.Line("12345");
		file.Line("12345");
		Check(file.IsOpen() && file.WhyItStopped() == 0, "under its limit it goes on");
		file.Line("12345");
		Check(!file.IsOpen(), "at its limit it stops");
		Check(file.WhyItStopped() && std::string(file.WhyItStopped()) == "too big", "and says why");
		file.Line("more");
		file.Flush();
		file.Close();

		std::string const text = ReadWholeFile(path);

		Check(text.find("# stopped here") != std::string::npos, "and the file says it isn't all there");
		Check(text.find("more") == std::string::npos, "and nothing goes in after that");
	}

	// A line too long is cut, not run off the end of anything
	{
		HudsonNavSaveFile file;
		std::string const longLine(5000, 'x');

		file.Open(path, 100000);
		file.Line("%s", longLine.c_str());
		file.Close();

		size_t const kept = ReadWholeFile(path).find('x') == 0 ? ReadWholeFile(path).find_last_of('x') + 1 : 0;

		Check(kept > 100 && kept < (size_t)kHudsonNavSaveMostLine, "a line far too long is cut: " + std::to_string(kept));
	}

	remove(path);

	// A disk that says no: a folder that isn't there
	{
		HudsonNavSaveFile file;

		Check(!file.Open("there_is_no_such_folder_at_all/saved.txt", 1000), "a file that can't be made says so");
		Check(file.WhyItStopped() && std::string(file.WhyItStopped()) == "the disk said no", "and says why");
		file.Line("nothing");
		file.Flush();
		file.Close();
		Check(!file.IsOpen(), "and writing to it does nothing, and doesn't crash");
	}

	// Never opened at all
	{
		HudsonNavSaveFile file;

		file.Line("nothing");
		file.Flush();
		file.Close();
		Check(!file.IsOpen() && file.WhyItStopped() == 0, "a file nobody opened does nothing");
	}
}

// What can be walked to from a point: the whole of his own side of a wall, none of the other side, and none of a floor above with no way up
static void TestWhatCanBeReached()
{
	World world;
	HudsonGrid grid;
	std::vector<uint8_t> reached;

	AddFloor(&world, 0, 0, 12000, 6000, 1000);
	AddWall(&world, 6000, 0, 6000, 6000, 1000, -3000);
	AddWall(&world, 0, 0, 12000, 0, 1000, -3000);
	AddWall(&world, 0, 6000, 12000, 6000, 1000, -3000);
	AddFloor(&world, 0, 0, 5000, 6000, 1000 - 4000);
	grid.Build(world);

	Check(grid.MarkWhatCanBeReachedFrom(2000, 1000, 3000, &reached), "what can be reached is found from a point on a floor");
	Check(reached.size() == grid.Places(), "with a mark for every place");
	Check(grid.IsMarked(reached, 4500, 1000, 1500), "his own side of the wall can be reached");
	Check(!grid.IsMarked(reached, 10000, 1000, 3000), "the other side can't");
	Check(!grid.IsMarked(reached, 2000, -3000, 3000), "and nor can the floor above, with no way up to it");
	Check(grid.MarkWhatCanBeReachedFrom(2000, -3000, 3000, &reached) && grid.IsMarked(reached, 4000, -3000, 1500) && !grid.IsMarked(reached, 10000, 1000, 3000), "from the floor above, the floor above can be reached, and still not the far side of the wall");
	Check(grid.MarkWhatCanBeReachedFrom(10000, 1000, 3000, &reached) && grid.IsMarked(reached, 11000, 1000, 1000) && !grid.IsMarked(reached, 2000, 1000, 3000), "and from the other side of the wall, only that side");
	Check(!grid.MarkWhatCanBeReachedFrom(50000, 1000, 50000, &reached), "a point with no floor near it gives false");
	Check(!grid.IsMarked(std::vector<uint8_t>(3, 1), 2000, 1000, 3000), "and marks of the wrong size never count");

	HudsonGrid empty;

	Check(!empty.MarkWhatCanBeReachedFrom(2000, 1000, 3000, &reached) && reached.empty(), "a grid that isn't built reaches nothing");
}

int main()
{
	TestWhatCanBeReached();
	TestAFloor();
	TestAskedAgain();
	TestAWall();
	TestHeights();
	TestStairs();
	TestSlopes();
	TestDrops();
	TestCorners();
	TestOnlyTouchingAtACorner();
	TestTheMiddle();
	TestSpots();
	TestFacing();
	TestHostile();
	TestCells();
	TestCellAt();
	TestSaving();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
