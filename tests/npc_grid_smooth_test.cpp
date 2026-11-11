// The test of pulling a grid way straight (3dc/avp/npc_grid_smooth.h, #145). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS, on made-up sheets of cells:
// - the line test: open lines, a blocked cell under a line, a line exactly through a corner beside a blocked cell, steep lines, one cell,
//   and a line too long to try;
// - the pulling: a staircase across an open room becomes one line; a way round a wall keeps the cells it turns at, and every line it keeps
//   is clear; it never pulls across a change of sheet, nor across two cells that aren't next to each other (a link, step or fall);
// - the limits: past the most tests or the most cells, the rest of the way is kept as it is;
// - what it refuses: no way, too little room for the answer; and a way of one cell;
// - and always: the first and last cells kept, in order.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/npc_grid_smooth_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/npc_grid_smooth_test.json proves each check can fail.

#include <stdio.h>

#include <string>
#include <vector>

#include "npc_grid_smooth.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// Made-up sheets: each is rows of '.' (the body fits) and '#' (it doesn't). A cell's number is its sheet times 10000, plus j times the
// width, plus i.
struct World
{
	std::vector<std::vector<std::string> > sheets;

	bool Fits(int sheet, int i, int j) const
	{
		if (sheet < 0 || sheet >= static_cast<int>(sheets.size()) || j < 0 || j >= static_cast<int>(sheets[sheet].size()) || i < 0 ||
			i >= static_cast<int>(sheets[sheet][j].size()))
		{
			return false;
		}

		return sheets[sheet][j][i] == '.';
	}

	bool Place(uint32_t cell, NpcGridSmoothPlace *where) const
	{
		int const sheet = static_cast<int>(cell / 10000), rest = static_cast<int>(cell % 10000);

		if (sheet >= static_cast<int>(sheets.size()) || sheets[sheet].empty())
		{
			return false;
		}

		int const width = static_cast<int>(sheets[sheet][0].size());
		where->sheet = sheet;
		where->i = rest % width;
		where->j = rest / width;
		return where->j < static_cast<int>(sheets[sheet].size());
	}

	uint32_t Cell(int sheet, int i, int j) const
	{
		return static_cast<uint32_t>(sheet * 10000 + j * static_cast<int>(sheets[sheet][0].size()) + i);
	}
};

static bool Line(World const &world, int sheet, int i0, int j0, int i1, int j1, size_t most = kNpcGridSmoothMostLineCells, size_t *tested = 0)
{
	size_t count = 0;
	bool const clear = NpcGridSmooth_LineClear(i0, j0, i1, j1, [&](int i, int j) { return world.Fits(sheet, i, j); }, most, &count);

	if (tested)
	{
		*tested = count;
	}

	return clear;
}

// Pulls a way given as (sheet, i, j) places, and gives back the places it kept
static std::vector<size_t> Pull(World const &world, std::vector<uint32_t> const &way, NpcGridSmoothLimits const &limits = NpcGridSmoothLimits(),
	size_t room = 0)
{
	std::vector<size_t> kept(way.size() + 4, 999999);
	size_t const n = NpcGridSmooth_Pull(way.data(), way.size(), [&](uint32_t cell, NpcGridSmoothPlace *where) { return world.Place(cell, where); },
		[&](int sheet, int i, int j) { return world.Fits(sheet, i, j); }, kept.data(), room ? room : way.size(), 0, limits);
	kept.resize(n);
	return kept;
}

// The first and last kept, in order
static bool Ordered(std::vector<size_t> const &kept, size_t count)
{
	if (kept.empty() || kept.front() != 0 || kept.back() != count - 1)
	{
		return false;
	}

	for (size_t k = 1; k < kept.size(); k++)
	{
		if (kept[k] <= kept[k - 1])
		{
			return false;
		}
	}

	return true;
}

static std::string Show(std::vector<size_t> const &kept)
{
	std::string text;

	for (size_t k = 0; k < kept.size(); k++)
	{
		text += (k ? " " : "") + std::to_string(kept[k]);
	}

	return "(kept " + text + ")";
}

static World OpenRoom()
{
	World world;
	world.sheets.push_back(std::vector<std::string>(10, std::string(10, '.')));
	return world;
}

static void TestTheLine()
{
	puts("The line test:");

	World world = OpenRoom();
	Check(Line(world, 0, 0, 0, 9, 0) && Line(world, 0, 0, 0, 9, 9) && Line(world, 0, 0, 0, 2, 9), "lines across an open room are clear");

	size_t tested = 0;
	Check(Line(world, 0, 3, 3, 3, 3, kNpcGridSmoothMostLineCells, &tested) && tested == 1, "a line of one cell asks about that cell only");

	world.sheets[0][5][5] = '#';
	Check(!Line(world, 0, 0, 0, 9, 9) && Line(world, 0, 0, 0, 9, 0), "a blocked cell under the line stops it, and a line beside it is clear");

	World corner = OpenRoom();
	corner.sheets[0][0][1] = '#';
	Check(!Line(corner, 0, 0, 0, 1, 1), "a line exactly through a corner, beside a blocked cell, isn't clear: no cutting corners");
	Check(Line(OpenRoom(), 0, 0, 0, 1, 1), "and with both cells beside the corner open, it is");

	World column = OpenRoom();
	column.sheets[0][2][1] = '#';
	Check(!Line(column, 0, 0, 0, 1, 5) && Line(OpenRoom(), 0, 0, 0, 1, 5), "a steep line looks at the cells it passes, as a flat one does");

	Check(!Line(OpenRoom(), 0, 0, 0, 9, 0, 5), "a line of more cells than allowed isn't tried, and isn't clear");
	Check(!Line(OpenRoom(), 0, 0, 0, 12, 0), "and a line off the sheet isn't clear");
}

static void TestThePull()
{
	puts("Pulling a way straight:");

	World const room = OpenRoom();
	std::vector<uint32_t> const stairs = { room.Cell(0, 0, 0), room.Cell(0, 1, 0), room.Cell(0, 2, 1), room.Cell(0, 3, 1), room.Cell(0, 4, 2),
		room.Cell(0, 5, 2), room.Cell(0, 6, 3) };
	std::vector<size_t> const pulled = Pull(room, stairs);
	Check(pulled.size() == 2 && Ordered(pulled, stairs.size()), "a staircase of cells across an open room becomes one line " + Show(pulled));

	// A wall down the middle with an opening at its foot: the way goes round it
	World wall = OpenRoom();
	for (int j = 0; j < 8; j++)
	{
		wall.sheets[0][j][5] = '#';
	}

	std::vector<uint32_t> round;
	for (int j = 1; j <= 8; j++) round.push_back(wall.Cell(0, 2, j));
	for (int i = 3; i <= 7; i++) round.push_back(wall.Cell(0, i, 8));
	for (int j = 7; j >= 1; j--) round.push_back(wall.Cell(0, 8, j));

	std::vector<size_t> const kept = Pull(wall, round);
	bool everyLineClear = true;

	for (size_t k = 1; k < kept.size(); k++)
	{
		NpcGridSmoothPlace a = {}, b = {};
		wall.Place(round[kept[k - 1]], &a);
		wall.Place(round[kept[k]], &b);
		everyLineClear = everyLineClear && Line(wall, 0, a.i, a.j, b.i, b.j);
	}

	Check(Ordered(kept, round.size()) && kept.size() > 2 && kept.size() < round.size() && everyLineClear,
		"a way round a wall keeps the cells it turns at, fewer than it had, and every line it keeps is clear " + Show(kept));

	// Two sheets, and the way goes from one onto the other, carrying straight on in i and j, so only the change of sheet can stop the line:
	// nothing is pulled across
	World two = OpenRoom();
	two.sheets.push_back(std::vector<std::string>(10, std::string(10, '.')));
	std::vector<uint32_t> const across = { two.Cell(0, 0, 0), two.Cell(0, 1, 0), two.Cell(0, 2, 0), two.Cell(1, 3, 0), two.Cell(1, 4, 0),
		two.Cell(1, 5, 0) };
	std::vector<size_t> const both = Pull(two, across);
	Check(both == std::vector<size_t>({ 0, 2, 3, 5 }), "onto another sheet, the last cell before and the first after are both kept " + Show(both));

	// On one sheet, two cells of the way that aren't next to each other: a link, a step or a fall
	std::vector<uint32_t> const jump = { room.Cell(0, 0, 0), room.Cell(0, 1, 0), room.Cell(0, 5, 0), room.Cell(0, 6, 0) };
	std::vector<size_t> const jumped = Pull(room, jump);
	Check(jumped == std::vector<size_t>({ 0, 1, 2, 3 }), "across a jump on one sheet, nothing is pulled " + Show(jumped));
}

static void TestTheLimits()
{
	puts("The limits:");

	World const room = OpenRoom();
	std::vector<uint32_t> straight;
	for (int i = 0; i < 6; i++) straight.push_back(room.Cell(0, i, 0));

	NpcGridSmoothLimits fewTests;
	fewTests.mostTests = 1;
	std::vector<size_t> const allKept = Pull(room, straight, fewTests);
	Check(allKept.size() == straight.size() && Ordered(allKept, straight.size()), "past the most tests, every cell after is kept as it is " + Show(allKept));

	NpcGridSmoothLimits fewCells;
	fewCells.mostCells = 3;
	std::vector<size_t> const firstPulled = Pull(room, straight, fewCells);
	Check(firstPulled == std::vector<size_t>({ 0, 2, 3, 4, 5 }), "only the first cells are pulled, and the rest are kept as they are " + Show(firstPulled));
}

static void TestTheRefusals()
{
	puts("What it refuses:");

	World const room = OpenRoom();
	std::vector<uint32_t> const way = { room.Cell(0, 0, 0), room.Cell(0, 1, 0), room.Cell(0, 2, 0) };
	size_t kept[8] = { 7, 7, 7, 7, 7, 7, 7, 7 };
	auto place = [&](uint32_t cell, NpcGridSmoothPlace *where) { return room.Place(cell, where); };
	auto fits = [&](int sheet, int i, int j) { return room.Fits(sheet, i, j); };

	Check(NpcGridSmooth_Pull(static_cast<const uint32_t *>(0), 3, place, fits, kept, 8) == 0, "no way: nothing kept");
	Check(NpcGridSmooth_Pull(way.data(), way.size(), place, fits, kept, 2) == 0, "too little room for the answer: nothing kept");
	Check(NpcGridSmooth_Pull(way.data(), 1, place, fits, kept, 8) == 1 && kept[0] == 0, "a way of one cell keeps that cell");

	std::vector<uint32_t> const unknown = { room.Cell(0, 0, 0), 999999u, room.Cell(0, 2, 0) };
	std::vector<size_t> const keptUnknown = Pull(room, unknown);
	Check(keptUnknown == std::vector<size_t>({ 0, 1, 2 }), "a cell it can't place breaks the way there, and nothing is pulled across it " + Show(keptUnknown));
}

int main()
{
	TestTheLine();
	TestThePull();
	TestTheLimits();
	TestTheRefusals();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
