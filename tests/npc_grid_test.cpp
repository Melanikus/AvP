// The test for 3dc/avp/npc_grid.h: the grid on every surface of a level that every kind of NPC finds its way on (issue #145, step 1).
// Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game. Every world here is
// made up, a few polygons at a time, so each answer can be worked out by hand, and the sums are in the comments beside the checks.
//
// Coordinates are the game's: millimetres, and y goes down, so a ceiling 3 m up is at y = -3000.
//
// THE MAIN WORLD is a room 6 m square and 3 m high with a pillar 600 mm square in its middle, from the floor to the ceiling. Seen from above:
//
//      z = 6000  +--------------------------+
//                |                          |
//                |          +----+          |      the pillar: x and z from 2700 to 3300
//                |          |    |          |      the floor and the ceiling each have a hole there,
//                |          +----+          |      made of four polygons round it
//                |                          |
//      z = 0     +--------------------------+
//              x = 0                      x = 6000
//
// Cells are 300 mm, lined up on multiples of 300 from the middle of the world, so a cell's middle is at 150, 450, 750 and so on.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "alien_ways_map.h"
#include "npc_grid.h"

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

static std::string Number(size_t n)
{
	char text[32];
	snprintf(text, sizeof(text), "%u", static_cast<unsigned>(n));
	return text;
}

struct Corner
{
	int x, y, z;
};

// A polygon from its corners, turned round if need be so that it faces the way given (nx, ny, nz: only which way matters, not how far),
// as the level's own polygons face out of the surface into the room. door is the door it's part of, 0 for none.
static AlienWaysPolygon Facing(std::vector<Corner> corners, double nx, double ny, double nz, int door = 0)
{
	// Newell's sums, as the map and the grid work out a facing
	double s[3] = { 0.0, 0.0, 0.0 };
	int const n = static_cast<int>(corners.size());

	for (int i = 0; i < n; i++)
	{
		Corner const &a = corners[i], &b = corners[(i + 1) % n];
		s[0] += (static_cast<double>(a.y) - b.y) * (static_cast<double>(a.z) + b.z);
		s[1] += (static_cast<double>(a.z) - b.z) * (static_cast<double>(a.x) + b.x);
		s[2] += (static_cast<double>(a.x) - b.x) * (static_cast<double>(a.y) + b.y);
	}

	if (s[0] * nx + s[1] * ny + s[2] * nz < 0.0)
	{
		std::vector<Corner> turned(corners.rbegin(), corners.rend());
		corners = turned;
		s[0] = -s[0];
		s[1] = -s[1];
		s[2] = -s[2];
	}

	double const size = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	AlienWaysPolygon p;
	memset(&p, 0, sizeof(p));
	p.corners = n;

	for (int i = 0; i < n && i < kAlienWaysMostCorners; i++)
	{
		p.x[i] = corners[i].x;
		p.y[i] = corners[i].y;
		p.z[i] = corners[i].z;
	}

	p.up = size > 0.0 ? static_cast<float>(-s[1] / size) : 0.0f;

	for (int k = 0; k < 3; k++)
	{
		p.facing[k] = size > 0.0 ? static_cast<float>(s[k] / size) : 0.0f;
	}

	p.door = door;
	return p;
}

// A floor at height y, from x0 to x1 and z0 to z1, facing up
static AlienWaysPolygon Floor(int x0, int x1, int z0, int z1, int y)
{
	return Facing({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, 0, -1, 0);
}

// A ceiling at height y, facing down
static AlienWaysPolygon Ceiling(int x0, int x1, int z0, int z1, int y)
{
	return Facing({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, 0, 1, 0);
}

// A wall standing at x, from z0 to z1 and from height top (the smaller y) down to bottom, facing the way of facesX (1 or -1)
static AlienWaysPolygon WallAtX(int x, int z0, int z1, int top, int bottom, int facesX, int door = 0)
{
	return Facing({ { x, bottom, z0 }, { x, bottom, z1 }, { x, top, z1 }, { x, top, z0 } }, facesX, 0, 0, door);
}

// The same at z, from x0 to x1, facing the way of facesZ
static AlienWaysPolygon WallAtZ(int z, int x0, int x1, int top, int bottom, int facesZ, int door = 0)
{
	return Facing({ { x0, bottom, z }, { x1, bottom, z }, { x1, top, z }, { x0, top, z } }, 0, 0, facesZ, door);
}

// The room with the pillar, drawn at the top of this file
static std::vector<AlienWaysPolygon> PillarRoom()
{
	std::vector<AlienWaysPolygon> world;

	// The floor and the ceiling, each as four pieces round the pillar's hole
	world.push_back(Floor(0, 2700, 0, 6000, 0));
	world.push_back(Floor(3300, 6000, 0, 6000, 0));
	world.push_back(Floor(2700, 3300, 0, 2700, 0));
	world.push_back(Floor(2700, 3300, 3300, 6000, 0));
	world.push_back(Ceiling(0, 2700, 0, 6000, -3000));
	world.push_back(Ceiling(3300, 6000, 0, 6000, -3000));
	world.push_back(Ceiling(2700, 3300, 0, 2700, -3000));
	world.push_back(Ceiling(2700, 3300, 3300, 6000, -3000));

	// The room's four walls, facing in
	world.push_back(WallAtX(0, 0, 6000, -3000, 0, 1));
	world.push_back(WallAtX(6000, 0, 6000, -3000, 0, -1));
	world.push_back(WallAtZ(0, 0, 6000, -3000, 0, 1));
	world.push_back(WallAtZ(6000, 0, 6000, -3000, 0, -1));

	// The pillar's four walls, facing out
	world.push_back(WallAtX(2700, 2700, 3300, -3000, 0, -1));
	world.push_back(WallAtX(3300, 2700, 3300, -3000, 0, 1));
	world.push_back(WallAtZ(2700, 2700, 3300, -3000, 0, -1));
	world.push_back(WallAtZ(3300, 2700, 3300, -3000, 0, 1));
	return world;
}

// Builds the alien ways map, then the grid, on a world. Says whether both worked; why holds the grid's answer.
static bool Build(std::vector<AlienWaysPolygon> const &world, AlienWaysMap *map, NpcGrid *grid, const char **why)
{
	*why = nullptr;

	if (map->Build(world) != nullptr)
	{
		*why = "the map didn't build";
		return false;
	}

	*why = grid->Build(world, *map);
	return *why == nullptr;
}

static size_t CountWith(NpcGrid const &grid, uint8_t bit)
{
	size_t count = 0;

	for (uint32_t c = 0; c < grid.Cells(); c++)
	{
		count += (grid.Flags(c) & bit) ? 1 : 0;
	}

	return count;
}

// Whether every step of a way is a real move: to a cell beside it on the same sheet, or along a link, or down a fall
static bool WayIsMoves(NpcGrid const &grid, std::vector<uint32_t> const &way)
{
	for (size_t k = 1; k < way.size(); k++)
	{
		uint32_t const a = way[k - 1], b = way[k];
		bool ok = false;

		if (grid.SheetOf(a) == grid.SheetOf(b))
		{
			double ma[3], mb[3];
			grid.Middle(a, ma);
			grid.Middle(b, mb);
			double const far = sqrt((ma[0] - mb[0]) * (ma[0] - mb[0]) + (ma[1] - mb[1]) * (ma[1] - mb[1]) + (ma[2] - mb[2]) * (ma[2] - mb[2]));
			ok = far < kNpcGridCell * 1.5;
		}

		size_t count = 0;
		const uint32_t *list = grid.LinksOf(a, &count);

		for (size_t i = 0; i < count && !ok; i++)
		{
			ok = list[i] == b;
		}

		list = grid.FallsOf(a, &count);

		for (size_t i = 0; i < count && !ok; i++)
		{
			ok = list[i] == b;
		}

		if (!ok)
		{
			return false;
		}
	}

	return true;
}

// How many cells of a way are on a kind of surface
static size_t WayOnKind(NpcGrid const &grid, std::vector<uint32_t> const &way, AlienWaysKind kind)
{
	size_t count = 0;

	for (size_t k = 0; k < way.size(); k++)
	{
		count += grid.KindOf(way[k]) == kind ? 1 : 0;
	}

	return count;
}

static void Cells()
{
	printf("The cells of the room with the pillar\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;
	bool const built = Build(PillarRoom(), &map, &grid, &why);
	Check(built, "the room with the pillar builds");

	if (!built)
	{
		return;
	}

	// The floor's four pieces join edge to edge in one plane, so they're one sheet, and so is the ceiling: 1 + 1 + 4 walls + 4 pillar walls
	Check(grid.Sheets() == 10, "10 sheets: the floor, the ceiling and 8 walls (" + Number(grid.Sheets()) + ")");

	// Each sheet's whole grid runs from its lowest corner's cell to its highest's, and a corner exactly on a multiple of 300 starts one more:
	// the floor and the ceiling 21 x 21 = 441 each, the room's walls 21 x 11 = 231 each, the pillar's walls 3 x 11 = 33 each.
	// 441 + 441 + 4 x 231 + 4 x 33 = 1938.
	Check(grid.Cells() == 1938, "1938 cells in the sheets' whole grids (" + Number(grid.Cells()) + ")");

	// On the surface: the floor 20 x 20 less the 2 x 2 under the pillar, 396, and the ceiling the same; the room's walls 20 x 10 = 200
	// each, the pillar's 2 x 10 = 20 each. 396 + 396 + 800 + 80 = 1672.
	Check(grid.SurfaceCells() == 1672, "1672 cells on the surface (" + Number(grid.SurfaceCells()) + ")");
	Check(CountWith(grid, kNpcGridMiddleOn) == 1672, "every one of them has its middle on a polygon");
	Check(CountWith(grid, kNpcGridCoveredOnly) == 0, "the hole's edges are on cells' edges, so no cell is only partly covered");

	// A marine's square is 450 either way, so his middle has to be 450 from a room wall: middles from 450 to 5550, 18 along each side, 324.
	// Round the pillar his square reaches it from middles 2550 to 3450, 4 x 4 = 16 of those 324, 4 of them in the hole: 324 - 16 = 308.
	Check(CountWith(grid, kNpcGridStands) == 308, "a marine stands on 308 cells (" + Number(CountWith(grid, kNpcGridStands)) + ")");
	Check(CountWith(grid, kNpcGridSteep) == 0, "nothing is steep");

	// Every surface has at least 2700 of room straight out (the pillar to a wall), so an alien clings everywhere it has a middle
	Check(CountWith(grid, kNpcGridClings) == 1672, "an alien clings to all 1672 (" + Number(CountWith(grid, kNpcGridClings)) + ")");

	// The floor's corner cell: its middle, its room up to the ceiling, and that a marine doesn't fit there, against two walls
	uint32_t const corner = grid.NearestCell(150, 0, 150, kNpcGridAlien, 10);
	double m[3] = { 0.0, 0.0, 0.0 };
	Check(corner != kNpcGridNoCell && grid.Middle(corner, m) && m[0] == 150 && m[1] == 0 && m[2] == 150, "the floor's corner cell is at (150, 0, 150)");
	Check(grid.KindOf(corner) == kAlienWaysFloor, "and it's on the floor");
	Check(grid.Room(corner) == 3000, "with 3000 of room, up to the ceiling (" + Number(grid.Room(corner)) + ")");
	Check(!(grid.Flags(corner) & kNpcGridStands), "where a marine doesn't fit");
	Check(grid.NearestCell(150, 0, 150, kNpcGridMarine, 400) == kNpcGridNoCell, "nor anywhere within 400 of it");
	Check(grid.NearestCell(450, 0, 450, kNpcGridMarine, 10) != kNpcGridNoCell, "but at (450, 0, 450) he does");

	// A point under the floor is behind it: the floor faces up, away from it, so it has no cell there, though one is 200 away. And a point
	// 400 outside the room's wall is behind that wall's face. A body only ever clings to the side of a surface it's on.
	Check(grid.NearestCell(1650, 500, 1650, kNpcGridAlien, 600) == kNpcGridNoCell, "a point 500 under the floor has no cell: the floor faces away from it");
	Check(grid.NearestCell(-700, -1650, 1650, kNpcGridAlien, 800) == kNpcGridNoCell, "nor a point 700 outside the room, behind its wall");
	Check(grid.NearestCell(1650, 200, 1650, kNpcGridAlien, 400) != kNpcGridNoCell,
		"but a point 200 under it still has the floor's cell: a body stands right on its floor, so up to a cell behind counts (kNpcGridSideSlack)");
	Check(grid.NearestCell(1650, -200, 1650, kNpcGridAlien, 400) != kNpcGridNoCell, "and so does a point 200 above it");
	Check(grid.NearestCell(nan(""), 0, 0, kNpcGridAlien, 400) == kNpcGridNoCell && grid.NearestCell(0, HUGE_VAL, 0, kNpcGridAlien, 400) == kNpcGridNoCell,
		"a point that isn't a number, or is endless, has no cell");

	// Its links reach the walls beside it
	size_t count = 0;
	const uint32_t *links = grid.LinksOf(corner, &count);
	size_t onWalls = 0;

	for (size_t k = 0; k < count; k++)
	{
		onWalls += grid.KindOf(links[k]) == kAlienWaysWall ? 1 : 0;
	}

	Check(count > 0 && onWalls == count, "the corner cell is linked to the walls beside it, and only to walls (" + Number(onWalls) + " of " + Number(count) + ")");

	// A pillar wall's cell: room to the room's wall, 2700
	uint32_t const onPillar = grid.NearestCell(2700, -1650, 2850, kNpcGridAlien, 10);
	Check(onPillar != kNpcGridNoCell && grid.KindOf(onPillar) == kAlienWaysWall && grid.Room(onPillar) == 2700,
		"a cell on the pillar has 2700 of room, out to the room's wall (" + Number(grid.Room(onPillar)) + ")");

	// A cell on the room's wall at x = 0, at z 1650, out of the pillar's row: its ray goes right across the room, through the plane of the
	// pillar's face at x = 2700, which isn't there at z 1650, to the far wall
	uint32_t const onWall = grid.NearestCell(0, -1650, 1650, kNpcGridAlien, 10);
	Check(onWall != kNpcGridNoCell && grid.Room(onWall) == 6000, "a cell on the room's wall has 6000 of room, to the far wall (" + Number(grid.Room(onWall)) + ")");

	// The same at z 2550, 150 short of the pillar's side. Added 2026-11-10 by Melanikus (Third) for the AvP port: a ray tries only the
	// polygons in the metre boxes it passes through (kNpcGridBox), and the one at z 1650, above, never shares a box with the pillar, so it
	// can't show this. This one passes through the box the pillar's face at x = 2700 is in (x, y and z from 2000, -2000 and 2000), so the
	// face is tried, and the ray crosses its plane at z 2550, off the face. Only a hit inside the polygon counts, so the room goes on past it
	uint32_t const besidePillar = grid.NearestCell(0, -1650, 2550, kNpcGridAlien, 10);
	Check(besidePillar != kNpcGridNoCell && grid.Room(besidePillar) == 6000,
		"a cell on the room's wall beside the pillar's row has 6000 of room: crossing the pillar face's plane off the face is no hit (" + Number(grid.Room(besidePillar)) + ")");

	// Asking about cells that aren't there
	Check(grid.SheetOf(static_cast<uint32_t>(grid.Cells())) == -1 && grid.Flags(static_cast<uint32_t>(grid.Cells())) == 0 &&
		grid.Room(static_cast<uint32_t>(grid.Cells())) == 0 && !grid.Middle(static_cast<uint32_t>(grid.Cells()), m),
		"a cell number past the last is no cell: sheet -1, no flags, no room, no middle");
	Check(grid.CellOver(-1, 0, 0, 0) == kNpcGridNoCell && grid.CellOver(static_cast<int>(grid.Sheets()), 0, 0, 0) == kNpcGridNoCell,
		"a sheet number that isn't a sheet has no cells");
	size_t none = 99;
	Check(grid.LinksOf(kNpcGridNoCell, &none) == nullptr && none == 0, "no cell has no links");
	Check(grid.Falls() == 0, "a room with a flat floor has no falls");
}

static void Searches()
{
	printf("Searches in the room with the pillar\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (!Build(PillarRoom(), &map, &grid, &why))
	{
		Check(false, "the room with the pillar builds");
		return;
	}

	std::vector<uint32_t> way;
	size_t looks = 0;

	// A marine, across the room past the pillar
	uint32_t const from = grid.NearestCell(1650, 0, 1650, kNpcGridMarine, 10), to = grid.NearestCell(4350, 0, 4350, kNpcGridMarine, 10);
	Check(from != kNpcGridNoCell && to != kNpcGridNoCell, "a marine stands at (1650, 0, 1650) and at (4350, 0, 4350)");
	Check(grid.Search(from, to, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridFound, "a marine finds his way across");
	Check(way.size() > 1 && way.front() == from && way.back() == to, "the way runs from the start to the goal");
	Check(WayIsMoves(grid, way), "every step of it is a move he can make");
	Check(WayOnKind(grid, way, kAlienWaysFloor) == way.size(), "all of it on the floor");
	bool allStand = true;

	for (size_t k = 0; k < way.size(); k++)
	{
		allStand = allStand && (grid.Flags(way[k]) & kNpcGridStands);
	}

	Check(allStand, "on cells he fits on");
	Check(looks > 0 && looks <= 100000, "it says how many cells it looked at (" + Number(looks) + ")");

	// The same way spread over frames gives points: pulled straight across the floor, far fewer than its cells. The pillar's in the way of
	// the diagonal from (1650, 1650) to (4350, 4350), so it takes at least one turn, and a straight line can't do it in one.
	AlienWaysXYZ const marineFrom = { 1650, 0, 1650 }, marineTo = { 4350, 0, 4350 };
	Check(grid.StartSearch(marineFrom, marineTo, kNpcGridMarine, 1, 100000) == nullptr && grid.Work(static_cast<size_t>(-1)) == kNpcGridFound,
		"a marine's way from positions is found");
	Check(grid.Way().size() >= 3 && grid.Way().size() * 2 <= grid.WayPath().size(),
		"pulled straight, it has a turn or two, not a point for each cell (" + Number(grid.Way().size()) + " points for " + Number(grid.WayPath().size()) + " cells)");

	// At a slant across open floor, well clear of the pillar: the search's cells go diagonally, then straight, but a marine fits all along the
	// straight line between the two, so pulled straight it's that line, the start and the goal and nothing between
	AlienWaysXYZ const slantFrom = { 450, 0, 450 }, slantTo = { 5550, 0, 2250 };
	Check(grid.StartSearch(slantFrom, slantTo, kNpcGridMarine, 1, 100000) == nullptr && grid.Work(static_cast<size_t>(-1)) == kNpcGridFound &&
		grid.Way().size() == 2 && grid.WayPath().size() > 2, "a way at a slant across open floor is one straight line, start to goal (" +
		Number(grid.Way().size()) + " points for " + Number(grid.WayPath().size()) + " cells)");

	// The same with too few looks
	Check(grid.Search(from, to, kNpcGridMarine, 1, 3, &way, &looks) == kNpcGridTooFar && way.empty() && looks == 3,
		"allowed to look at only 3 cells, it gives up, with no way");

	// An alien from the floor to the ceiling, with the floor dear
	uint32_t const low = grid.NearestCell(1650, 0, 1650, kNpcGridAlien, 10), high = grid.NearestCell(1650, -3000, 1650, kNpcGridAlien, 10);
	Check(low != kNpcGridNoCell && high != kNpcGridNoCell && grid.KindOf(high) == kAlienWaysCeiling, "an alien clings to the floor and the ceiling there");
	Check(grid.Search(low, high, kNpcGridAlien, 10, 100000, &way, &looks) == kNpcGridFound, "an alien finds its way from the floor to the ceiling");
	Check(WayIsMoves(grid, way), "every step of it is a move");
	Check(WayOnKind(grid, way, kAlienWaysWall) > 0, "up a wall");

	// An alien beside a wall, going 2.7 m along it. With the floor at 10 times a wall, it goes up onto the wall: 2700 of floor costs 27000,
	// and the wall about 212 + 2700 + 212 x 10 = 5032. With the floor at 1, the floor's 2700 is less than the wall's 3124.
	uint32_t const near1 = grid.NearestCell(150, 0, 1650, kNpcGridAlien, 10), near2 = grid.NearestCell(150, 0, 4350, kNpcGridAlien, 10);
	Check(grid.Search(near1, near2, kNpcGridAlien, 10, 100000, &way, &looks) == kNpcGridFound && WayOnKind(grid, way, kAlienWaysWall) > 0 &&
		WayOnKind(grid, way, kAlienWaysFloor) == 2, "with the floor dear, an alien beside a wall goes along the wall, on the floor only at its two ends");
	Check(grid.Search(near1, near2, kNpcGridAlien, 1, 100000, &way, &looks) == kNpcGridFound && WayOnKind(grid, way, kAlienWaysWall) == 0,
		"with the floor at a wall's price, it stays on the floor");

	// A marine can't go there: the ceiling isn't his
	Check(grid.Search(from, high, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridNoWay && way.empty(), "a marine has no way to the ceiling");

	// An alien round the pillar, from its face at x = 2700 to its face at x = 3300. The pillar runs from the floor to the ceiling, so the
	// cheapest way is round its corners, along a face at z = 2700 or z = 3300, as at Ops01's outside corner.
	uint32_t const west = grid.NearestCell(2700, -1650, 2850, kNpcGridAlien, 10), east = grid.NearestCell(3300, -1650, 3150, kNpcGridAlien, 10);
	Check(west != kNpcGridNoCell && east != kNpcGridNoCell, "an alien clings to the pillar's two faces");
	Check(grid.Search(west, east, kNpcGridAlien, 10, 100000, &way, &looks) == kNpcGridFound && WayIsMoves(grid, way), "and finds its way round the pillar");
	size_t onSide = 0;

	for (size_t k = 0; k < way.size(); k++)
	{
		double m[3];
		grid.Middle(way[k], m);
		onSide += (grid.KindOf(way[k]) == kAlienWaysWall && (m[2] == 2700 || m[2] == 3300) && m[0] > 2700 && m[0] < 3300) ? 1 : 0;
	}

	Check(onSide > 0, "along one of the pillar's other faces (" + Number(onSide) + " cells of it)");
	Check(WayOnKind(grid, way, kAlienWaysFloor) == 0, "and none of it on the floor");

	// The same kind of search spread over frames, as the guide asks for one: from an alien's middle, 500 above the floor, to another,
	// 10 cells a frame
	AlienWaysXYZ const startAt = { 1650, -500, 1650 }, goalAt = { 4350, -500, 4350 };
	Check(grid.StartSearch(startAt, goalAt, kNpcGridAlien, 10, 100000) == nullptr, "a search from two positions starts");
	int frames = 0;
	NpcGridSearch state = kNpcGridSearching;

	while (state == kNpcGridSearching && frames < 100000)
	{
		state = grid.Work(10);
		frames++;
	}

	Check(state == kNpcGridFound && frames > 1, "spread over frames, 10 cells at a time, it finds its way (" + Number(frames) + " frames)");
	Check(grid.Way().size() >= 2 && grid.Way().front().x == 1650 && grid.Way().front().y == -500 && grid.Way().back().x == 4350 &&
		grid.Way().back().z == 4350, "its way runs from the start's position to the goal's");
	Check(grid.Work(10) == kNpcGridFound && grid.LastSearchLooks() > 0, "asked again, it says the same");
	bool faces = true;

	for (size_t k = 0; k < grid.Way().size(); k++)
	{
		AlienWaysPoint const &p = grid.Way()[k];
		faces = faces && (p.kind != kAlienWaysFloor || (p.facing[0] == 0 && p.facing[1] == -32767 && p.facing[2] == 0));
	}

	Check(faces, "a point on the floor faces straight up: (0, -32767, 0), as the level stores a facing");
	Check(grid.WayCells().size() == grid.Way().size() && grid.WayPath().size() >= grid.Way().size() && grid.WayPath().front() == grid.WayCells().front() &&
		grid.WayPath().back() == grid.WayCells().back(), "a cell for each point of the way, and every cell of it, start to goal");
	bool polygonsMatch = grid.WayPolygons().size() == grid.Way().size();

	for (size_t k = 0; polygonsMatch && k < grid.WayPolygons().size(); k++)
	{
		polygonsMatch = grid.WayPolygons()[k] == grid.PolygonOf(grid.WayCells()[k]) && grid.WayPolygons()[k] >= 0;
	}

	Check(polygonsMatch, "and the map polygon under each point's cell, as the map's own WayPolygons gives them");
	int sheet = -1, ci = -1, cj = -1;
	uint32_t const first = grid.WayPath().front();
	Check(grid.CellPlace(first, &sheet, &ci, &cj) && grid.CellAt(sheet, ci, cj) == first, "a cell's place on its sheet gives the cell back");
	Check(!grid.CellPlace(static_cast<uint32_t>(grid.Cells()), &sheet, &ci, &cj) && grid.CellAt(sheet, -1, 0) == kNpcGridNoCell &&
		grid.CellAt(-1, 0, 0) == kNpcGridNoCell, "and a place off a sheet, or a number past the cells, is none");
	AlienWaysXYZ const nowhere = { 100000, 0, 100000 };
	Check(Says(grid.StartSearch(nowhere, goalAt, kNpcGridAlien, 10, 100000), "no cell near the start") && grid.Work(10) == kNpcGridNoWay && grid.Way().empty(),
		"a start with no cell near it has no way");

	// Searches that can't start
	Check(grid.Search(kNpcGridNoCell, to, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridNoWay, "a start that isn't a cell has no way");
	Check(grid.Search(high, to, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridNoWay, "nor a start a marine doesn't stand on");
	Check(grid.Search(low, high, kNpcGridAlien, -5, 100000, &way, &looks) == kNpcGridFound, "a floor price under 1 is taken as 1");
}

// Stairs going up along z: the floor at y = 0 up to z = 3000, then four treads, each 300 deep and 300 higher, then the top floor at
// y = -1500 from z = 4200. Each tread is a sheet of its own, and its riser is a wall. No walls round it and no ceiling.
static std::vector<AlienWaysPolygon> Stairs()
{
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(0, 3000, 0, 3000, 0));

	for (int k = 1; k <= 4; k++)
	{
		world.push_back(Floor(0, 3000, 3000 + 300 * (k - 1), 3000 + 300 * k, -300 * k));
	}

	world.push_back(Floor(0, 3000, 4200, 7200, -1500));

	for (int k = 1; k <= 5; k++)
	{
		world.push_back(WallAtZ(3000 + 300 * (k - 1), 0, 3000, -300 * k, -300 * (k - 1), -1));
	}

	return world;
}

static void Steps()
{
	printf("Stairs\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (!Build(Stairs(), &map, &grid, &why))
	{
		Check(false, "the stairs build");
		return;
	}

	// No join links one tread to the next: they meet only through the riser, a wall. A marine goes up by the steps.
	Check(grid.Steps() > 0, "the treads are linked by steps (" + Number(grid.Steps()) + ")");
	std::vector<uint32_t> way;
	size_t looks = 0;
	uint32_t const bottom = grid.NearestCell(1650, 0, 1650, kNpcGridMarine, 10), top = grid.NearestCell(1650, -1500, 6150, kNpcGridMarine, 10);
	Check(bottom != kNpcGridNoCell && top != kNpcGridNoCell, "a marine stands at the bottom and at the top");
	Check(grid.Search(bottom, top, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridFound && WayIsMoves(grid, way), "and goes up the stairs");
	bool everyTread[4] = { false, false, false, false };

	for (size_t k = 0; k < way.size(); k++)
	{
		double m[3];
		grid.Middle(way[k], m);

		for (int t = 1; t <= 4; t++)
		{
			everyTread[t - 1] = everyTread[t - 1] || m[1] == -300 * t;
		}
	}

	Check(everyTread[0] && everyTread[1] && everyTread[2] && everyTread[3], "on every tread");
	Check(grid.Search(top, bottom, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridFound, "and comes down them");

	// A door's floor 500 above the floor beside it, with no riser. Added 2026-11-10 by Melanikus (Third) for the AvP port. No part of a
	// door is in anyone's way, so a marine stands on both edges, 300 apart, and only the step's own height keeps them apart: 500 is more
	// than kNpcGridStep, all the game lifts him. (On the ledge, below, the riser's own square keeps the edges 900 apart, whatever the step.)
	{
		std::vector<AlienWaysPolygon> world;
		world.push_back(Floor(0, 3000, 0, 3000, 0));
		world.push_back(Facing({ { 0, -500, 3000 }, { 3000, -500, 3000 }, { 3000, -500, 6000 }, { 0, -500, 6000 } }, 0, -1, 0, 1));
		AlienWaysMap doorMap;
		NpcGrid doorGrid;
		bool const built = Build(world, &doorMap, &doorGrid, &why);
		uint32_t const edge = built ? doorGrid.NearestCell(1650, 0, 2850, kNpcGridMarine, 10) : kNpcGridNoCell;
		uint32_t const doorEdge = built ? doorGrid.NearestCell(1650, -500, 3150, kNpcGridMarine, 10) : kNpcGridNoCell;
		Check(edge != kNpcGridNoCell && doorEdge != kNpcGridNoCell, "a marine stands on a floor's edge, and on a door's floor 500 above it, 300 beyond");
		Check(built && doorGrid.Steps() == 0, "and they're no step: 500 is more than kNpcGridStep (" + Number(doorGrid.Steps()) + " steps)");
	}
}

// A ledge: the floor at y = 0 up to z = 3000, and a platform 500 higher beyond it, with the wall between. 500 is more than a step.
static std::vector<AlienWaysPolygon> Ledge()
{
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(0, 3000, 0, 3000, 0));
	world.push_back(Floor(0, 3000, 3000, 6000, -500));
	world.push_back(WallAtZ(3000, 0, 3000, -500, 0, -1));
	return world;
}

static void Falls()
{
	printf("A ledge\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (!Build(Ledge(), &map, &grid, &why))
	{
		Check(false, "the ledge builds");
		return;
	}

	Check(grid.Falls() > 0, "a marine can fall off the platform's edge (" + Number(grid.Falls()) + " falls)");
	std::vector<uint32_t> way;
	size_t looks = 0;
	uint32_t const below = grid.NearestCell(1650, 0, 1650, kNpcGridMarine, 10), above = grid.NearestCell(1650, -500, 4650, kNpcGridMarine, 10);
	Check(below != kNpcGridNoCell && above != kNpcGridNoCell, "a marine stands below and above");
	Check(grid.Search(above, below, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridFound && WayIsMoves(grid, way), "he comes down off the platform");
	Check(grid.Search(below, above, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridNoWay, "and can't climb back up: 500 is more than a step");

	// A fall lands two columns out from the edge he walks off, on the floor below
	bool landsRight = grid.Falls() > 0;

	for (uint32_t c = 0; c < grid.Cells() && landsRight; c++)
	{
		size_t count = 0;
		const uint32_t *list = grid.FallsOf(c, &count);

		for (size_t k = 0; k < count; k++)
		{
			double a[3], b[3];
			grid.Middle(c, a);
			grid.Middle(list[k], b);
			landsRight = landsRight && a[1] == -500 && a[2] == 3150 && b[1] == 0 && b[2] == 2550 && a[0] == b[0];
		}
	}

	Check(landsRight, "every fall goes from the platform's edge cell (z 3150) to the floor two columns out (z 2550)");

	// An alien just goes down the wall and up it
	uint32_t const a = grid.NearestCell(1650, 0, 1650, kNpcGridAlien, 10), b = grid.NearestCell(1650, -500, 4650, kNpcGridAlien, 10);
	Check(grid.Search(a, b, kNpcGridAlien, 1, 100000, &way, &looks) == kNpcGridFound, "an alien climbs the ledge");
}

// A ramp of 40 degrees from the floor at y = 0 (z 0 to 3000) up to a floor at y = -2517 (from z = 6000). 40 degrees is floor to the game,
// which calls anything up to about 63 degrees floor, but steeper than its "nearly flat", about 24.
static std::vector<AlienWaysPolygon> Ramp()
{
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(0, 3000, 0, 3000, 0));
	world.push_back(Facing({ { 0, 0, 3000 }, { 3000, 0, 3000 }, { 3000, -2517, 6000 }, { 0, -2517, 6000 } }, 0, -1, -1));
	world.push_back(Floor(0, 3000, 6000, 9000, -2517));
	return world;
}

static void Slopes()
{
	printf("A steep ramp\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (!Build(Ramp(), &map, &grid, &why))
	{
		Check(false, "the ramp builds");
		return;
	}

	Check(CountWith(grid, kNpcGridSteep) > 0, "the ramp's cells are steep (" + Number(CountWith(grid, kNpcGridSteep)) + ")");
	std::vector<uint32_t> way;
	size_t looks = 0;
	uint32_t const bottom = grid.NearestCell(1650, 0, 1650, kNpcGridMarine, 10), top = grid.NearestCell(1650, -2517, 7650, kNpcGridMarine, 10);
	Check(bottom != kNpcGridNoCell && top != kNpcGridNoCell, "a marine stands at the bottom and at the top");
	Check(grid.Search(top, bottom, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridFound && WayIsMoves(grid, way), "he walks down the ramp");
	Check(grid.Search(bottom, top, kNpcGridMarine, 1, 100000, &way, &looks) == kNpcGridNoWay, "and not up it");
}

// Two rooms side by side, joined by a doorway 1500 wide and 2400 high in the wall between them at x = 3000, with a door's panel in it.
// door is the panel's door number: 0 makes it a plain wall.
static std::vector<AlienWaysPolygon> TwoRooms(int door)
{
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(0, 6000, 0, 6000, 0));
	world.push_back(Ceiling(0, 6000, 0, 6000, -3000));
	world.push_back(WallAtX(0, 0, 6000, -3000, 0, 1));
	world.push_back(WallAtX(6000, 0, 6000, -3000, 0, -1));
	world.push_back(WallAtZ(0, 0, 6000, -3000, 0, 1));
	world.push_back(WallAtZ(6000, 0, 6000, -3000, 0, -1));

	for (int side = -1; side <= 1; side += 2)
	{
		world.push_back(WallAtX(3000, 0, 2250, -3000, 0, side));
		world.push_back(WallAtX(3000, 3750, 6000, -3000, 0, side));
		world.push_back(WallAtX(3000, 2250, 3750, -3000, -2400, side));
		world.push_back(WallAtX(3000, 2250, 3750, -2400, 0, side, door));
	}

	return world;
}

static void Doors()
{
	printf("A door\n");

	for (int door = 1; door >= 0; door--)
	{
		AlienWaysMap map;
		NpcGrid grid;
		const char *why;

		if (!Build(TwoRooms(door), &map, &grid, &why))
		{
			Check(false, "the two rooms build");
			return;
		}

		std::vector<uint32_t> way;
		size_t looks = 0;
		uint32_t const here = grid.NearestCell(1650, 0, 3150, kNpcGridMarine, 10), there = grid.NearestCell(4350, 0, 3150, kNpcGridMarine, 10);
		NpcGridSearch const result = grid.Search(here, there, kNpcGridMarine, 1, 100000, &way, &looks);
		uint32_t const onPanel = grid.NearestCell(3000, -1200, 3000, kNpcGridAlien, 400);

		if (door)
		{
			Check(here != kNpcGridNoCell && there != kNpcGridNoCell && result == kNpcGridFound, "a marine goes through a door: it opens for him");
			Check(onPanel == kNpcGridNoCell, "a door's panel has no cells");
		}
		else
		{
			Check(result == kNpcGridNoWay, "with a plain wall there instead, he has no way");
			Check(onPanel != kNpcGridNoCell, "and that wall has cells");
		}
	}
}

// A room whose ceiling is 1910 up: lower than a marine
static std::vector<AlienWaysPolygon> LowRoom()
{
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(0, 3000, 0, 3000, 0));
	world.push_back(Ceiling(0, 3000, 0, 3000, -1910));
	world.push_back(WallAtX(0, 0, 3000, -1910, 0, 1));
	world.push_back(WallAtX(3000, 0, 3000, -1910, 0, -1));
	world.push_back(WallAtZ(0, 0, 3000, -1910, 0, 1));
	world.push_back(WallAtZ(3000, 0, 3000, -1910, 0, -1));
	return world;
}

static void Room()
{
	printf("A low room, a narrow strip, and a polygon with no area\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (Build(LowRoom(), &map, &grid, &why))
	{
		Check(CountWith(grid, kNpcGridStands) == 0, "a marine stands nowhere under a ceiling 1910 up");
		Check(CountWith(grid, kNpcGridClings) == grid.SurfaceCells() && grid.SurfaceCells() > 0, "an alien clings everywhere: 1910 is more than its 900");
		uint32_t const c = grid.NearestCell(1350, 0, 1350, kNpcGridAlien, 10);
		Check(grid.Room(c) == 1900, "a floor cell has 1910 of room, kept as 1900: room is rounded down to 25 mm steps (" + Number(grid.Room(c)) + ")");
	}
	else
	{
		Check(false, "the low room builds");
	}

	// A floor strip 130 wide, from z 3160 to 3290. No cell's middle is on it (they're at 3150 and 3450), but it covers 130 x 300 of each cell
	// in its row, far more than 1%, so those cells are on it, and only a marine uses them.
	std::vector<AlienWaysPolygon> strip(1, Floor(0, 3000, 3160, 3290, 0));

	if (Build(strip, &map, &grid, &why))
	{
		Check(grid.SurfaceCells() == 10, "a floor strip with no middle on it still has its row of 10 cells (" + Number(grid.SurfaceCells()) + ")");
		Check(CountWith(grid, kNpcGridCoveredOnly) == 10 && CountWith(grid, kNpcGridMiddleOn) == 0, "each one only covered");
		Check(CountWith(grid, kNpcGridStands) == 10 && CountWith(grid, kNpcGridClings) == 0, "a marine stands on them, and an alien doesn't");
	}
	else
	{
		Check(false, "the strip builds");
	}

	// The same strip standing up as a wall gets no cells: covering counts on a floor only
	std::vector<AlienWaysPolygon> wallStrip(1, WallAtX(0, 3160, 3290, -3000, 0, 1));

	if (Build(wallStrip, &map, &grid, &why))
	{
		Check(grid.SurfaceCells() == 0, "the same strip as a wall has no cells");
	}
	else
	{
		Check(false, "the wall strip builds");
	}

	// A polygon whose corners lie in a line, in the middle of the room with the pillar: it has no plane, so it's in no one's way and has no
	// cells, and the room's answers don't change
	std::vector<AlienWaysPolygon> world = PillarRoom();
	world.push_back(Facing({ { 1000, -1500, 1000 }, { 2000, -1500, 1000 }, { 3000, -1500, 1000 } }, 0, -1, 0));

	if (Build(world, &map, &grid, &why))
	{
		Check(map.MapNumberOf(world.size() - 1) >= 0, "the map keeps the polygon with no area");
		Check(grid.SurfaceCells() == 1672 && CountWith(grid, kNpcGridStands) == 308 && CountWith(grid, kNpcGridClings) == 1672,
			"and the grid is just as without it: 1672 cells, 308 a marine stands on, 1672 an alien clings to");
	}
	else
	{
		Check(false, "the room with a polygon with no area builds (" + std::string(why ? why : "") + ")");
	}
}

// A floor from x0 to 3000, and beside it one from 3000 to x1 that rises 1.5 degrees (79 in 3000). That's within the 2 degrees of facing,
// so it's the plane test that says whether they're one sheet.
static std::vector<AlienWaysPolygon> Tilted(int x0, int x1)
{
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(x0, 3000, 0, 3000, 0));
	int const rise = static_cast<int>(floor((x1 - 3000) * 79.0 / 3000.0 + 0.5));
	world.push_back(Facing({ { 3000, 0, 0 }, { x1, -rise, 0 }, { x1, -rise, 3000 }, { 3000, 0, 3000 } }, 0, -1, 0));
	return world;
}

static void Sheets()
{
	printf("Which polygons make one sheet\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	// Each 3000 long: each one's middle is 1500 from the join, so 39 off the other's plane, more than 15
	if (Build(Tilted(0, 6000), &map, &grid, &why))
	{
		Check(grid.Sheets() == 2, "a floor and one beside it tilted 1.5 degrees, each 3 m long, are two sheets (" + Number(grid.Sheets()) + ")");
	}
	else
	{
		Check(false, "the long tilted floors build");
	}

	// Each 600 long: each middle is 300 from the join, 8 off the other's plane, so they're one sheet
	if (Build(Tilted(2400, 3600), &map, &grid, &why))
	{
		Check(grid.Sheets() == 1, "the same, each 600 long, are one sheet (" + Number(grid.Sheets()) + ")");
	}
	else
	{
		Check(false, "the short tilted floors build");
	}

	// A floor in two pieces, split at x = 3100, off a cell's edge. The cell from 3000 to 3300 has its middle (3150) on the second piece and is
	// a third covered by the first. It counts as a cell with its middle on: "covered only" means no polygon has it in the middle.
	std::vector<AlienWaysPolygon> split;
	split.push_back(Floor(0, 3100, 0, 3000, 0));
	split.push_back(Floor(3100, 6000, 0, 3000, 0));

	if (Build(split, &map, &grid, &why))
	{
		Check(grid.Sheets() == 1 && grid.SurfaceCells() == 200, "a floor in two pieces is one sheet of 20 x 10 = 200 cells (" + Number(grid.SurfaceCells()) + ")");
		Check(CountWith(grid, kNpcGridCoveredOnly) == 0, "and none is only covered, though one piece covers part of a cell the other has the middle of");
	}
	else
	{
		Check(false, "the floor in two pieces builds");
	}
}

// One wall, 3 m square, at x = 0 from z 0 to 3000, facing +x, and nothing else
static void Reach()
{
	printf("Looking round for a cell\n");
	std::vector<AlienWaysPolygon> world(1, WallAtX(0, 0, 3000, -3000, 0, 1));
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (!Build(world, &map, &grid, &why))
	{
		Check(false, "the wall builds");
		return;
	}

	// A point 500 out from the wall and 600 past its end (z 3600). Seen onto the wall's plane it's two cells past the wall's grid, so neither
	// the cell under it nor any of the 3 x 3 round it is there; but the wall's last column of cells, at z 2850, is 901 away (500 out and
	// 750 along), inside the reach.
	uint32_t const c = grid.NearestCell(500, -1650, 3600, kNpcGridAlien, 1500);
	double m[3] = { 0.0, 0.0, 0.0 };
	Check(c != kNpcGridNoCell && grid.Middle(c, m) && m[0] == 0 && m[1] == -1650 && m[2] == 2850,
		"a point two cells past a wall's end finds the wall's nearest cell, further than the 3 x 3 round it reaches");

	// 1000 past the end, the nearest cell is 500 out and 1150 along: 1254 away, more than the reach of 1200 given here
	Check(grid.NearestCell(500, -1650, 4000, kNpcGridAlien, 1200) == kNpcGridNoCell, "and one further than the reach finds none");
}

static void Pieces()
{
	printf("Pieces that don't join\n");
	std::vector<AlienWaysPolygon> world;
	world.push_back(Floor(0, 3000, 0, 3000, 0));
	world.push_back(Floor(10000, 13000, 0, 3000, 0));
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	if (!Build(world, &map, &grid, &why))
	{
		Check(false, "two floors far apart build");
		return;
	}

	Check(grid.Pieces() == 2 && grid.PiecesGaveUp() == nullptr, "two floors far apart are two pieces (" + Number(grid.Pieces()) + ")");
	AlienWaysXYZ const here = { 1500, -500, 1500 }, there = { 11500, -500, 1500 }, alsoHere = { 600, -500, 2400 };
	Check(Says(grid.StartSearch(here, there, kNpcGridAlien, 10, 100000), "the start and the goal are in pieces that don't join") &&
		grid.Work(1000) == kNpcGridNoWay && grid.LastSearchLooks() == 0 && grid.Way().empty(),
		"an alien's search from one to the other is refused at once, having looked at nothing");
	Check(grid.StartSearch(here, alsoHere, kNpcGridAlien, 10, 100000) == nullptr && grid.Work(static_cast<size_t>(-1)) == kNpcGridFound,
		"while one across a floor runs as before");
	// Each cell keeps 6 bytes of its own (flags, room, polygon), and the two sheets about 200 more, which is less than the pieces' 484
	// (2 bytes for each of the 242 cells): so what's kept reaches the cells' and the pieces' together only if the pieces are counted
	Check(grid.PieceBytes() >= grid.Cells() * 2 && grid.BytesKept() >= grid.Cells() * 6 + grid.PieceBytes(),
		"the pieces' 2 bytes a cell are counted in what the grid keeps (" + Number(grid.PieceBytes()) + " of " + Number(grid.BytesKept()) + ")");
}

// What a refused build leaves: nothing
static bool KeptNothing(NpcGrid const &grid)
{
	return !grid.Ready() && grid.Cells() == 0 && grid.Sheets() == 0 && grid.Links() == 0 && grid.Falls() == 0 && grid.BytesKept() == 0;
}

static void Limits()
{
	printf("Limits\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;

	Check(NpcGrid_UsualLimits(false).mostBytesKept == 64u * 1024u * 1024u, "the usual cap is 64 MB");
	Check(NpcGrid_UsualLimits(true).mostBytesKept == 250u * 1024u * 1024u, "and 250 MB with advanced AI");

	// The map not built
	Check(Says(grid.Build(PillarRoom(), map), "the alien ways map isn't ready") && KeptNothing(grid), "with no map, there's no grid");

	// The room with the pillar, as it is, for its counts
	if (!Build(PillarRoom(), &map, &grid, &why))
	{
		Check(false, "the room with the pillar builds");
		return;
	}

	size_t const sheets = grid.Sheets(), cells = grid.Cells(), kept = grid.BytesKept(), rays = grid.RayTests(), squares = grid.SquareTests();
	size_t const building = grid.BytesWhileBuilding(), forLinks = (grid.Links() + grid.Falls()) * 2 * sizeof(uint32_t),
		forPieces = grid.PieceBytes();
	Check(kept > 0 && rays > 0 && squares > 0 && building > 0, "it counts what it keeps, its rays, its squares and what it held while building");
	std::vector<AlienWaysPolygon> const world = PillarRoom();

	struct Try
	{
		// Builds with one limit changed, and gives back the answer
		static const char *With(NpcGridLimits limits, std::vector<AlienWaysPolygon> const &world, AlienWaysMap const &map, NpcGrid *grid)
		{
			grid->SetLimits(limits);
			const char *why = grid->Build(world, map);
			grid->SetLimits(NpcGrid_UsualLimits(false));
			return why;
		}
	};

	NpcGridLimits limits = NpcGrid_UsualLimits(false);
	limits.mostSheets = sheets - 1;
	Check(Says(Try::With(limits, world, map, &grid), "too many sheets") && KeptNothing(grid), "one sheet more than the most is refused, and nothing kept");
	limits.mostSheets = sheets;
	Check(Try::With(limits, world, map, &grid) == nullptr, "the most sheets is let through");

	limits = NpcGrid_UsualLimits(false);
	limits.mostCellsInASheet = 440;
	Check(Says(Try::With(limits, world, map, &grid), "a sheet has too many cells") && KeptNothing(grid), "a sheet of 441 cells is refused when the most is 440");
	limits.mostCellsInASheet = 441;
	Check(Try::With(limits, world, map, &grid) == nullptr, "and let through when it's 441");

	limits = NpcGrid_UsualLimits(false);
	limits.mostCells = cells - 1;
	Check(Says(Try::With(limits, world, map, &grid), "too many cells") && KeptNothing(grid), "one cell more than the most is refused");
	limits.mostCells = cells;
	Check(Try::With(limits, world, map, &grid) == nullptr, "the most cells is let through");

	limits = NpcGrid_UsualLimits(false);
	// What's kept is the cells and sheets, then the links and falls, then the pieces. The grid needs the first two; the pieces it goes
	// without, if they don't fit.
	Check(forPieces > 0, "the room with the pillar has its pieces");
	limits.mostBytesKept = kept - forPieces - forLinks - 1;
	Check(Says(Try::With(limits, world, map, &grid), "the grid would keep too much memory, for its cells") && KeptNothing(grid), "a byte too many for its cells is refused");
	limits.mostBytesKept = kept - forPieces - 1;
	Check(Says(Try::With(limits, world, map, &grid), "the grid would keep too much memory, for its links") && KeptNothing(grid), "a byte too many for its links is refused");
	limits.mostBytesKept = kept - 1;
	Check(Try::With(limits, world, map, &grid) == nullptr && grid.PieceBytes() == 0 && grid.Pieces() == 0 && grid.BytesKept() == kept - forPieces &&
		Says(grid.PiecesGaveUp(), "they would keep more than the cap allows"),
		"a byte too few for the pieces builds the grid without them, keeping no more than the cap, and says why");
	limits.mostBytesKept = kept;
	Check(Try::With(limits, world, map, &grid) == nullptr && grid.BytesKept() == kept, "exactly the cap is let through");

	limits = NpcGrid_UsualLimits(false);
	limits.mostRayTests = rays - 1;
	Check(Says(Try::With(limits, world, map, &grid), "too many polygons tried by the rays") && KeptNothing(grid), "one ray test more than the most is refused");
	limits.mostRayTests = rays;
	Check(Try::With(limits, world, map, &grid) == nullptr, "the most ray tests is let through");

	limits = NpcGrid_UsualLimits(false);
	limits.mostSquareTests = squares - 1;
	Check(Says(Try::With(limits, world, map, &grid), "too many polygons tried against a marine's square") && KeptNothing(grid), "one square test more than the most is refused");
	limits.mostSquareTests = squares;
	Check(Try::With(limits, world, map, &grid) == nullptr, "the most square tests is let through");

	// Every polygon of the room is flat on an axis, so each crosses one box per column of boxes: the floor's 4 pieces 3 x 6 + 3 x 6 + 1 x 3 +
	// 1 x 3, and so on. The limit is worked out from what the build lists, not hand-counted here: one less is refused.
	limits = NpcGrid_UsualLimits(false);
	limits.mostBoxEntries = 1;
	Check(Says(Try::With(limits, world, map, &grid), "too many columns of metre boxes to look through") && KeptNothing(grid),
		"the columns of boxes are counted first: more than the most is refused before any box is listed");

	// The ramp crosses more boxes than columns: its plane rises 0.84 m over each metre, so most of its columns hold two boxes of it. So
	// there the columns are under the limit when the boxes are one over it.
	AlienWaysMap rampMap;
	std::vector<AlienWaysPolygon> const ramp = Ramp();

	if (Build(ramp, &rampMap, &grid, &why))
	{
		size_t const entries = grid.BoxEntries();
		limits = NpcGrid_UsualLimits(false);
		limits.mostBoxEntries = entries - 1;
		Check(Says(Try::With(limits, ramp, rampMap, &grid), "too many polygons in the metre boxes") && KeptNothing(grid),
			"one polygon in a metre box more than the most is refused (" + Number(entries) + " in all)");
		limits.mostBoxEntries = entries;
		Check(Try::With(limits, ramp, rampMap, &grid) == nullptr, "the most is let through");
	}
	else
	{
		Check(false, "the ramp builds");
	}

	limits = NpcGrid_UsualLimits(false);
	limits.mostLinks = 1;
	Check(Says(Try::With(limits, world, map, &grid), "too many links") && KeptNothing(grid), "too many links is refused");

	limits = NpcGrid_UsualLimits(false);
	limits.mostBytesBuilding = building - 1;
	Check(Try::With(limits, world, map, &grid) != nullptr && KeptNothing(grid), "a byte less than it needs while building is refused");
	limits.mostBytesBuilding = building;
	Check(Try::With(limits, world, map, &grid) == nullptr, "exactly what it needs is let through");
	limits.mostBytesBuilding = 1;
	Check(Says(Try::With(limits, world, map, &grid), "too much memory while building, for the polygons") && KeptNothing(grid), "with almost nothing, it stops at the polygons");

	// Falls: the ledge's
	AlienWaysMap ledgeMap;
	std::vector<AlienWaysPolygon> const ledge = Ledge();

	if (Build(ledge, &ledgeMap, &grid, &why))
	{
		size_t const falls = grid.Falls();
		limits = NpcGrid_UsualLimits(false);
		limits.mostFalls = falls - 1;
		Check(Says(Try::With(limits, ledge, ledgeMap, &grid), "too many falls") && KeptNothing(grid), "one fall more than the most is refused");
		limits.mostFalls = falls;
		Check(Try::With(limits, ledge, ledgeMap, &grid) == nullptr, "the most falls is let through");
	}
	else
	{
		Check(false, "the ledge builds");
	}

	// A floor of one polygon 70 m across, just under the map's scenery cut (5,000 square metres), is a normal floor. 233 middles fit along
	// each side (150 up to 69,750 + 150 = 69,900), and the cell after them, from 69,900 to 70,200, is a third covered: 234 x 234 = 54,756
	// cells, 233 x 233 = 54,289 of them with their middles on it and 467 only covered.
	std::vector<AlienWaysPolygon> big(1, Floor(0, 70000, 0, 70000, 0));
	AlienWaysMap bigMap;

	if (Build(big, &bigMap, &grid, &why))
	{
		Check(grid.SurfaceCells() == 54756 && CountWith(grid, kNpcGridCoveredOnly) == 467,
			"a floor 70 m across has 54,756 cells, 467 of them only covered (" + Number(grid.SurfaceCells()) + ", " + Number(CountWith(grid, kNpcGridCoveredOnly)) + ")");
	}
	else
	{
		Check(false, "a floor 70 m across builds (" + std::string(why ? why : "") + ")");
	}
}

static void Stopping()
{
	printf("Stopping\n");
	AlienWaysMap map;
	std::vector<AlienWaysPolygon> const world = PillarRoom();
	Check(map.Build(world) == nullptr, "the map builds");
	NpcGrid grid;

	grid.AskToStop();
	Check(Says(grid.Build(world, map), "asked to stop") && KeptNothing(grid), "asked to stop before it starts, it stops at once");
	Check(Says(grid.Build(world, map), "asked to stop"), "and goes on stopping until Clear");
	grid.Clear();
	Check(grid.Build(world, map) == nullptr && grid.Ready(), "after Clear, it builds");

	// Stopped at every look in turn, until a build gets past them all
	size_t stopped = 0, kept = 0;
	size_t look = 1;

	for (; look < 100000; look++)
	{
		grid.Clear();
		grid.StopAtLookForATest(look);
		const char *why = grid.Build(world, map);

		if (why == nullptr)
		{
			break;
		}

		stopped += Says(why, "asked to stop") ? 1 : 0;
		kept += KeptNothing(grid) ? 0 : 1;
	}

	grid.StopAtLookForATest(0);
	Check(stopped == look - 1 && stopped > 10, "stopped at each of its first " + Number(stopped) + " looks, it says so");
	Check(kept == 0, "and keeps nothing each time");
	grid.Clear();
	Check(grid.Build(world, map) == nullptr, "with no stop asked, it builds");
}

static void Again()
{
	printf("Built again\n");
	AlienWaysMap map;
	NpcGrid grid;
	const char *why;
	Build(Stairs(), &map, &grid, &why);
	size_t const stairsCells = grid.Cells();
	Check(Build(PillarRoom(), &map, &grid, &why) && grid.Cells() == 1938 && CountWith(grid, kNpcGridStands) == 308,
		"built again on another level, it has the new level's cells only (" + Number(stairsCells) + " then " + Number(grid.Cells()) + ")");
	grid.Clear();
	Check(KeptNothing(grid) && grid.SurfaceCells() == 0, "Clear lets go of everything");
}

int main()
{
	Cells();
	Sheets();
	Searches();
	Reach();
	Pieces();
	Steps();
	Falls();
	Slopes();
	Doors();
	Room();
	Limits();
	Stopping();
	Again();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all npc grid checks passed\n");
	return 0;
}
