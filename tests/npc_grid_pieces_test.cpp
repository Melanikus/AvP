// The test of the grid's pieces (3dc/avp/npc_grid_pieces.h, #145). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS, on made-up grids:
// - what joins two cells: a step on a sheet, a link either way, a fall (only for a body that falls, and onto a cell whatever its flags);
//   and what doesn't: a cell the body can't be on, between two others, and a link onto one;
// - the numbers: the biggest pieces first, the same grid always the same numbers, and a piece past the most that are numbered is "not
//   known", so it's never called apart;
// - what it refuses or gives up on: no body, a grid that changes while it works, more memory than it's allowed; and cell numbers past
//   the grid, from a link or a fall, turned away;
// - working in steps of one cell gives the same as all at once, and nothing is known before it's done;
// - and THE ONE THAT MATTERS: on hundreds of random grids, two cells the search can get between (worked out here by a copy of the
//   search's moves, npc_grid.cpp) are never called apart. And two cells its joining doesn't join are always apart, when both are known.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/npc_grid_pieces_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/npc_grid_pieces_test.json proves each check can fail.

#include <stdio.h>
#include <string.h>

#include <deque>
#include <string>
#include <vector>

#include "npc_grid_pieces.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

uint8_t const kBody = 2;        // the body's flag in a made-up grid ('.' in a sheet's rows)
uint32_t const kNoCell = 0xFFFFFFFFu;

// A made-up grid, with the same calls as NpcGrid: sheets of cells, numbered one sheet after another, row by row. In a sheet's rows '.'
// is a cell the body can be on and '#' one it can't.
struct Grid
{
	struct Sheet
	{
		int w, h;
		uint32_t first;
	};

	std::vector<Sheet> sheets;
	std::vector<uint8_t> flags;
	std::vector<std::vector<uint32_t> > links, falls;

	int AddSheet(const std::vector<std::string> &rows)
	{
		Sheet const s = { static_cast<int>(rows[0].size()), static_cast<int>(rows.size()), static_cast<uint32_t>(flags.size()) };

		for (size_t j = 0; j < rows.size(); j++)
		{
			for (size_t i = 0; i < rows[j].size(); i++)
			{
				flags.push_back(rows[j][i] == '.' ? kBody : 0);
			}
		}

		sheets.push_back(s);
		links.resize(flags.size());
		falls.resize(flags.size());
		return static_cast<int>(sheets.size()) - 1;
	}

	void AddSheetOf(int w, int h)
	{
		std::vector<std::string> rows(static_cast<size_t>(h), std::string(static_cast<size_t>(w), '#'));
		AddSheet(rows);
	}

	size_t Cells() const { return flags.size(); }
	uint8_t Flags(uint32_t cell) const { return cell < flags.size() ? flags[cell] : 0; }

	bool CellPlace(uint32_t cell, int *sheet, int *i, int *j) const
	{
		for (size_t s = 0; s < sheets.size(); s++)
		{
			uint32_t const count = static_cast<uint32_t>(sheets[s].w * sheets[s].h);

			if (cell >= sheets[s].first && cell - sheets[s].first < count)
			{
				*sheet = static_cast<int>(s);
				*i = static_cast<int>((cell - sheets[s].first) % static_cast<uint32_t>(sheets[s].w));
				*j = static_cast<int>((cell - sheets[s].first) / static_cast<uint32_t>(sheets[s].w));
				return true;
			}
		}

		return false;
	}

	uint32_t CellAt(int sheet, int i, int j) const
	{
		if (sheet < 0 || sheet >= static_cast<int>(sheets.size()))
		{
			return kNoCell;
		}

		Sheet const &s = sheets[static_cast<size_t>(sheet)];

		if (i < 0 || j < 0 || i >= s.w || j >= s.h)
		{
			return kNoCell;
		}

		return s.first + static_cast<uint32_t>(j * s.w + i);
	}

	const uint32_t *LinksOf(uint32_t cell, size_t *count) const { return ListOf(links, cell, count); }
	const uint32_t *FallsOf(uint32_t cell, size_t *count) const { return ListOf(falls, cell, count); }

	static const uint32_t *ListOf(const std::vector<std::vector<uint32_t> > &lists, uint32_t cell, size_t *count)
	{
		*count = cell < lists.size() ? lists[cell].size() : 0;
		return *count ? &lists[cell][0] : nullptr;
	}
};

size_t const kPlenty = 1u << 30;   // memory enough for any made-up grid

// Finds a grid's pieces all at once
static void Find(const Grid &grid, bool falls, NpcGridPieces *pieces, size_t mostKnown = kNpcGridPiecesMostKnown, size_t mostBytes = kPlenty)
{
	pieces->Begin(grid.Cells(), kBody, falls, mostKnown, mostBytes);
	pieces->Step(grid, kPlenty);
}

static void Steps()
{
	puts("What joins two cells:");
	{
		// Two rooms on one sheet, with a column the body can't be on between them
		Grid grid;
		int const s = grid.AddSheet({ "..#..", "..#..", "..#.." });
		NpcGridPieces pieces;
		Find(grid, false, &pieces);
		Check(pieces.Done() && !pieces.GaveUp(), "a sheet's pieces are found");
		Check(pieces.PieceOf(grid.CellAt(s, 0, 0)) == pieces.PieceOf(grid.CellAt(s, 1, 2)), "cells next to each other, in a row and a column, are one piece");
		Check(pieces.CertainlyApart(grid.CellAt(s, 0, 0), grid.CellAt(s, 4, 2)), "a column the body can't be on keeps two rooms apart");
		Check(pieces.PieceOf(grid.CellAt(s, 2, 1)) == kNpcGridPieceNotKnown, "and a cell it can't be on is in no piece");
		Check(pieces.Pieces() == 2, "two pieces");
	}
	{
		// Two cells that only touch at a corner: the search wouldn't cut it, but joining them is the safe side
		Grid grid;
		int const s = grid.AddSheet({ ".#", "#." });
		NpcGridPieces pieces;
		Find(grid, false, &pieces);
		Check(!pieces.CertainlyApart(grid.CellAt(s, 0, 0), grid.CellAt(s, 1, 1)), "two cells that touch at a corner are never called apart");
	}
	{
		// A link between two sheets, stored one way only
		Grid grid;
		int const a = grid.AddSheet({ "..." }), b = grid.AddSheet({ "...", "..." });
		NpcGridPieces before;
		Find(grid, false, &before);
		Check(before.CertainlyApart(grid.CellAt(a, 0, 0), grid.CellAt(b, 2, 1)), "two sheets with nothing between them are apart");

		grid.links[grid.CellAt(b, 2, 1)].push_back(grid.CellAt(a, 1, 0));
		NpcGridPieces after;
		Find(grid, false, &after);
		Check(!after.CertainlyApart(grid.CellAt(a, 0, 0), grid.CellAt(b, 0, 0)) && after.PieceOf(grid.CellAt(a, 0, 0)) == after.PieceOf(grid.CellAt(b, 0, 0)),
			"a link joins them, though it's only stored from the second sheet to the first");
	}
	{
		// A link onto a cell the body can't be on joins nothing
		Grid grid;
		int const a = grid.AddSheet({ "..#" }), b = grid.AddSheet({ ".." });
		grid.links[grid.CellAt(b, 0, 0)].push_back(grid.CellAt(a, 2, 0));
		grid.links[grid.CellAt(a, 2, 0)].push_back(grid.CellAt(b, 0, 0));
		NpcGridPieces pieces;
		Find(grid, false, &pieces);
		Check(pieces.CertainlyApart(grid.CellAt(a, 0, 0), grid.CellAt(b, 0, 0)), "a link onto a cell the body can't be on joins nothing");
	}
	{
		// A fall from a ledge down onto a floor, and one onto a cell of the floor the body can't stand on, beside the rest of it
		Grid grid;
		int const ledge = grid.AddSheet({ ".." }), floor = grid.AddSheet({ "#...", "...." });
		grid.falls[grid.CellAt(ledge, 1, 0)].push_back(grid.CellAt(floor, 0, 0));
		NpcGridPieces walking;
		Find(grid, false, &walking);
		Check(walking.CertainlyApart(grid.CellAt(ledge, 0, 0), grid.CellAt(floor, 3, 1)), "a body that doesn't fall isn't joined by a fall");

		NpcGridPieces falling;
		Find(grid, true, &falling);
		Check(!falling.CertainlyApart(grid.CellAt(ledge, 0, 0), grid.CellAt(floor, 3, 1)),
			"a body that falls is: onto a cell it can't stand on, which still joins the floor beside it");
		Check(!falling.CertainlyApart(grid.CellAt(floor, 3, 1), grid.CellAt(ledge, 0, 0)), "and both ways, though a fall only goes down");
		Check(falling.PieceOf(grid.CellAt(floor, 0, 0)) != kNpcGridPieceNotKnown, "where a fall lands is in a piece, whatever its flags");
	}
	{
		// The same, with the landing after the floor's other cells: the step between them is only looked at from the floor's side, where
		// it's a step from a cell with the body's flag onto one a fall lands on
		Grid grid;
		int const ledge = grid.AddSheet({ ".." }), floor = grid.AddSheet({ "...#" });
		grid.falls[grid.CellAt(ledge, 0, 0)].push_back(grid.CellAt(floor, 3, 0));
		NpcGridPieces falling;
		Find(grid, true, &falling);
		Check(!falling.CertainlyApart(grid.CellAt(ledge, 1, 0), grid.CellAt(floor, 0, 0)),
			"a landing joins the floor beside it from either side");
	}
	{
		// Cell numbers past the grid, from a link and from a fall, are turned away
		Grid grid;
		int const s = grid.AddSheet({ "..", ".." });
		grid.links[grid.CellAt(s, 0, 0)].push_back(kNoCell);
		grid.falls[grid.CellAt(s, 0, 0)].push_back(kNoCell);
		grid.falls[grid.CellAt(s, 1, 1)].push_back(static_cast<uint32_t>(grid.Cells()));
		NpcGridPieces pieces;
		Find(grid, true, &pieces);
		Check(pieces.Done() && !pieces.GaveUp() && pieces.Pieces() == 1, "a link or a fall to a cell past the grid is turned away");
		Check(pieces.PieceOf(static_cast<uint32_t>(grid.Cells())) == kNpcGridPieceNotKnown && pieces.PieceOf(kNoCell) == kNpcGridPieceNotKnown,
			"and a cell past the grid has no piece");
	}
}

static void Numbers()
{
	puts("The numbers:");

	// Four pieces on one sheet: 6 cells at the top left, 3 down the right (its first cell is cell 4), 3 at the bottom left (its first
	// cell is 20) and 1 at the bottom right
	Grid grid;
	int const s = grid.AddSheet({ "...#.", "...#.", "####.", "#####", "...#." });
	uint32_t const six = grid.CellAt(s, 0, 0), three = grid.CellAt(s, 4, 0), threeToo = grid.CellAt(s, 0, 4), one = grid.CellAt(s, 4, 4);

	NpcGridPieces all;
	Find(grid, false, &all);
	Check(all.Pieces() == 4, "four pieces");
	Check(all.PieceOf(six) == 0, "the biggest is number 0");
	Check(all.PieceOf(three) == 1 && all.PieceOf(threeToo) == 2, "two of the same size go by their first cell, so the same grid always has the same numbers");
	Check(all.PieceOf(one) == 3, "and the smallest is last");

	NpcGridPieces two;
	Find(grid, false, &two, 2);
	Check(two.PieceOf(six) == 0 && two.PieceOf(three) == 1, "with only two numbered, the two biggest are");
	Check(two.PieceOf(threeToo) == kNpcGridPieceNotKnown && two.PieceOf(one) == kNpcGridPieceNotKnown, "and the others aren't known");
	Check(two.CertainlyApart(six, three), "two known pieces are apart");
	Check(!two.CertainlyApart(six, one) && !two.CertainlyApart(one, six) && !two.CertainlyApart(one, threeToo),
		"but a piece that isn't known is never called apart from anything");
}

static void Refusals()
{
	puts("What it refuses, and gives up on:");
	{
		Grid grid;
		grid.AddSheet({ ".." });
		NpcGridPieces pieces;
		char const *const why = pieces.Begin(grid.Cells(), 0, false, kNpcGridPiecesMostKnown, kPlenty);
		Check(why && !strcmp(why, "no body was named") && pieces.Done() && pieces.GaveUp(), "no body is refused, by name");
	}
	{
		Grid grid;
		NpcGridPieces pieces;
		Check(pieces.Begin(0, kBody, false, kNpcGridPiecesMostKnown, kPlenty) == nullptr && pieces.Done() && !pieces.GaveUp() && pieces.Pieces() == 0,
			"a grid with no cells is done at once, with no pieces");
		Check(pieces.Step(grid, 1), "and stepping it says it's done");
	}
	{
		// More memory than it's allowed: at the start, and later, when it comes to keep the numbers
		Grid grid;
		int const s = grid.AddSheet({ "....", "#..#" });
		uint32_t const a = grid.CellAt(s, 0, 0), b = grid.CellAt(s, 1, 1);
		NpcGridPieces tiny;
		Find(grid, false, &tiny, kNpcGridPiecesMostKnown, 4);
		Check(tiny.GaveUp() && !strcmp(tiny.WhyGaveUp(), "it would take more memory than it's allowed"), "too little memory to start: it gives up, and says why");
		Check(tiny.Done() && tiny.PieceOf(a) == kNpcGridPieceNotKnown && !tiny.CertainlyApart(a, b), "and then nothing is known, and nothing is apart");

		// Enough for the joining (4 bytes a cell), the pieces' sizes (room for a first 1,024) and putting its one piece in order (4), with 4
		// over, but not for the numbers kept as well (2 bytes a cell, 16)
		NpcGridPieces late;
		Find(grid, false, &late, kNpcGridPiecesMostKnown, grid.Cells() * 4 + 1024 * 4 + 4 + 4);
		Check(late.GaveUp() && !strcmp(late.WhyGaveUp(), "it would take more memory than it's allowed") && late.PieceOf(a) == kNpcGridPieceNotKnown,
			"too little to keep the numbers: it gives up then, the same way");
		Check(late.BytesHeld() == 0, "having let go of all it held");
	}
	{
		// The grid changes between two steps
		Grid grid;
		grid.AddSheet({ "...", "..." });
		NpcGridPieces pieces;
		pieces.Begin(grid.Cells(), kBody, false, kNpcGridPiecesMostKnown, kPlenty);
		pieces.Step(grid, 2);
		grid.AddSheet({ "." });
		Check(pieces.Step(grid, kPlenty) && pieces.GaveUp() && !strcmp(pieces.WhyGaveUp(), "the grid changed while its pieces were being found"),
			"a grid that changes while it works is given up on, and it says why");
	}
}

static void InSteps()
{
	puts("Working in steps:");

	Grid grid;
	int const a = grid.AddSheet({ "..#..", ".##..", "....#" }), b = grid.AddSheet({ "...", "#.." });
	grid.links[grid.CellAt(b, 0, 0)].push_back(grid.CellAt(a, 4, 0));
	grid.falls[grid.CellAt(a, 0, 0)].push_back(grid.CellAt(b, 0, 1));

	NpcGridPieces once;
	Find(grid, true, &once);

	NpcGridPieces stepped;
	stepped.Begin(grid.Cells(), kBody, true, kNpcGridPiecesMostKnown, kPlenty);
	size_t steps = 0;
	bool knownEarly = false;

	while (!stepped.Step(grid, 1) && steps < 1000)
	{
		steps++;
		knownEarly = knownEarly || stepped.PieceOf(grid.CellAt(a, 0, 0)) != kNpcGridPieceNotKnown ||
			stepped.CertainlyApart(grid.CellAt(a, 0, 0), grid.CellAt(a, 4, 2));
	}

	bool same = true;

	for (uint32_t c = 0; c < grid.Cells(); c++)
	{
		same = same && once.PieceOf(c) == stepped.PieceOf(c);
	}

	Check(stepped.Done() && same, "one cell at a time gives the same numbers as all at once");
	Check(steps + 1 == grid.Cells() * 4 + 1, "in one step for each cell of each of its four stages, and one to put the pieces in order");
	Check(!knownEarly, "and nothing is known, or apart, before it's done");
	Check(stepped.BytesHeld() == stepped.BytesKept() && stepped.BytesKept() == grid.Cells() * sizeof(uint16_t), "once done, it holds only the numbers: 2 bytes a cell");
}

// A repeatable random number, so a failure can be found again
struct Random
{
	uint32_t state;
	explicit Random(uint32_t seed) : state(seed) {}
	uint32_t Next()
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	int Below(int n) { return static_cast<int>(Next() % static_cast<uint32_t>(n)); }
};

// Where the search can get to from a cell, as npc_grid.cpp's search moves: a step to a cell round it on its sheet that has the body's
// flag (across a corner only with both cells beside it), a link onto a cell with the flag, and, for a body that falls, a fall onto any
// cell. It turns nothing else down, so it gets to more than the real search ever can: what it reaches, the pieces must never call apart.
static std::vector<bool> SearchReaches(const Grid &grid, bool falls, uint32_t from)
{
	std::vector<bool> reached(grid.Cells(), false);
	std::deque<uint32_t> next;
	reached[from] = true;
	next.push_back(from);

	while (!next.empty())
	{
		uint32_t const c = next.front();
		next.pop_front();
		std::vector<uint32_t> to;
		int s = 0, i = 0, j = 0;

		if (grid.CellPlace(c, &s, &i, &j))
		{
			for (int dj = -1; dj <= 1; dj++)
			{
				for (int di = -1; di <= 1; di++)
				{
					uint32_t const n = grid.CellAt(s, i + di, j + dj);

					if ((di == 0 && dj == 0) || n == kNoCell || !(grid.Flags(n) & kBody))
					{
						continue;
					}

					if (di != 0 && dj != 0 && (!(grid.Flags(grid.CellAt(s, i + di, j)) & kBody) || !(grid.Flags(grid.CellAt(s, i, j + dj)) & kBody)))
					{
						continue;
					}

					to.push_back(n);
				}
			}
		}

		for (uint32_t n : grid.links[c])
		{
			if (n < grid.Cells() && (grid.Flags(n) & kBody))
			{
				to.push_back(n);
			}
		}

		if (falls)
		{
			for (uint32_t n : grid.falls[c])
			{
				if (n < grid.Cells())
				{
					to.push_back(n);
				}
			}
		}

		for (uint32_t n : to)
		{
			if (!reached[n])
			{
				reached[n] = true;
				next.push_back(n);
			}
		}
	}

	return reached;
}

// The pieces the joining should make, worked out the plain way: every pair of cells it joins (npc_grid_pieces.h, Join) made into a
// list both ways, and each piece walked from one of its cells. -1 for a cell the body can't be on.
static std::vector<int> JoinedPieces(const Grid &grid, bool falls)
{
	size_t const cells = grid.Cells();
	std::vector<bool> landing(cells, false);

	for (size_t c = 0; falls && c < cells; c++)
	{
		for (uint32_t n : grid.falls[c])
		{
			if (n < cells)
			{
				landing[n] = true;
			}
		}
	}

	auto const has = [&grid](uint32_t c) { return (grid.Flags(c) & kBody) != 0; };
	auto const canBeOn = [&](uint32_t c) { return has(c) || landing[c]; };
	std::vector<std::vector<uint32_t> > joined(cells);

	for (uint32_t c = 0; c < cells; c++)
	{
		if (!canBeOn(c))
		{
			continue;
		}

		std::vector<uint32_t> to;
		int s = 0, i = 0, j = 0;
		grid.CellPlace(c, &s, &i, &j);

		for (int dj = -1; dj <= 1; dj++)
		{
			for (int di = -1; di <= 1; di++)
			{
				uint32_t const n = grid.CellAt(s, i + di, j + dj);

				if (!(di == 0 && dj == 0) && n != kNoCell && (has(n) || (has(c) && canBeOn(n))))
				{
					to.push_back(n);
				}
			}
		}

		for (uint32_t n : grid.links[c])
		{
			if (n < cells && has(n))
			{
				to.push_back(n);
			}
		}

		for (uint32_t n : grid.falls[c])
		{
			if (falls && n < cells)
			{
				to.push_back(n);
			}
		}

		for (uint32_t n : to)
		{
			joined[c].push_back(n);
			joined[n].push_back(c);
		}
	}

	std::vector<int> piece(cells, -1);
	int pieces = 0;

	for (uint32_t c = 0; c < cells; c++)
	{
		if (!canBeOn(c) || piece[c] >= 0)
		{
			continue;
		}

		std::deque<uint32_t> next(1, c);
		piece[c] = pieces;

		while (!next.empty())
		{
			uint32_t const here = next.front();
			next.pop_front();

			for (uint32_t n : joined[here])
			{
				if (piece[n] < 0)
				{
					piece[n] = pieces;
					next.push_back(n);
				}
			}
		}

		pieces++;
	}

	return piece;
}

static void RandomGrids()
{
	puts("On random grids:");

	Random random(145);
	size_t ways = 0, wrongApart = 0, pairs = 0, wrongPieces = 0, grids = 0;

	for (int g = 0; g < 400; g++)
	{
		Grid grid;
		int const sheets = 1 + random.Below(4);

		for (int s = 0; s < sheets; s++)
		{
			grid.AddSheetOf(1 + random.Below(9), 1 + random.Below(9));
		}

		for (uint32_t c = 0; c < grid.Cells(); c++)
		{
			grid.flags[c] = random.Below(100) < 65 ? kBody : 0;
		}

		// Links, some stored one way only, as Build might; and falls, which only go one way
		bool const falls = random.Below(2) == 0;
		int const linkCount = random.Below(6), fallCount = falls ? random.Below(6) : 0;

		for (int k = 0; k < linkCount; k++)
		{
			uint32_t const a = static_cast<uint32_t>(random.Below(static_cast<int>(grid.Cells()))), b = static_cast<uint32_t>(random.Below(static_cast<int>(grid.Cells())));
			grid.links[a].push_back(b);

			if (random.Below(3) != 0)
			{
				grid.links[b].push_back(a);
			}
		}

		for (int k = 0; k < fallCount; k++)
		{
			grid.falls[static_cast<size_t>(random.Below(static_cast<int>(grid.Cells())))].push_back(static_cast<uint32_t>(random.Below(static_cast<int>(grid.Cells()))));
		}

		NpcGridPieces pieces;
		Find(grid, falls, &pieces);

		if (!pieces.Done() || pieces.GaveUp())
		{
			wrongApart++;
			continue;
		}

		grids++;

		// 1. Never apart where the search has a way. The search only ever starts and ends on a cell with the body's flag.
		for (uint32_t from = 0; from < grid.Cells(); from++)
		{
			if (!(grid.Flags(from) & kBody))
			{
				continue;
			}

			std::vector<bool> const reached = SearchReaches(grid, falls, from);

			for (uint32_t to = 0; to < grid.Cells(); to++)
			{
				if ((grid.Flags(to) & kBody) && reached[to])
				{
					ways++;
					wrongApart += pieces.CertainlyApart(from, to) ? 1 : 0;
				}
			}
		}

		// 2. The numbers are exactly the joining's pieces: two cells have the same number if and only if the joining joins them, and a
		// cell the body can't be on has none. (Every piece is numbered here: there are never more than 324 cells.)
		std::vector<int> const joined = JoinedPieces(grid, falls);

		for (uint32_t a = 0; a < grid.Cells(); a++)
		{
			if (joined[a] < 0)
			{
				pairs++;
				wrongPieces += pieces.PieceOf(a) == kNpcGridPieceNotKnown ? 0 : 1;
				continue;
			}

			for (uint32_t b = 0; b < grid.Cells(); b++)
			{
				if (joined[b] >= 0)
				{
					pairs++;
					bool const same = pieces.PieceOf(a) == pieces.PieceOf(b) && pieces.PieceOf(a) != kNpcGridPieceNotKnown;
					wrongPieces += same == (joined[a] == joined[b]) ? 0 : 1;
				}
			}
		}
	}

	Check(grids == 400, "every grid's pieces are found");
	Check(ways > 1000 && wrongApart == 0, "two cells the search can get between are never called apart (" + std::to_string(ways) + " pairs, 400 grids)");
	Check(pairs > 1000 && wrongPieces == 0,
		"and the numbers are exactly the pieces the joining makes, worked out the plain way (" + std::to_string(pairs) + " pairs)");
}

int main()
{
	Steps();
	Numbers();
	Refusals();
	InSteps();
	RandomGrids();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "passed", checksFailed);
	return checksFailed ? 1 : 0;
}
