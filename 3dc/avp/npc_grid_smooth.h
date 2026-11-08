// Pulls a grid way straight: from each point, it goes as far along the way as a straight line stays on cells the body fits on, so a way
// across a room is one line, not a staircase of cells. Written 2026-11-08 by Melanikus for the AvP port (#145).
//
// WHY. The grid's cells are 300 mm across (npc_grid.h), and a search across them goes from cell to cell, straight or diagonally. So a way
// across a room at a slant comes out as a zigzag of steps:
//
//      the search's cells:   x x                  the way pulled straight:   x
//                                x x                                           \
//                                    x x                                         \
//                                        x x                                       x
//
// The way keeps only the cells where it has to turn: start there, and go on to the farthest cell of the way that a straight line from the
// last one kept can reach with every cell under the line one the body fits on. Then keep that one, and carry on from it.
//
// WHERE IT NEVER PULLS ACROSS. A way goes from one sheet to another (a floor onto a wall, a wall onto a ceiling), or across a step or a
// fall, through a link, not a straight line on one surface. So the way is cut into runs: two cells of the way in a row are in one run only if
// they're on one sheet and next to each other on it. A line is only ever drawn inside one run, and the last cell before a run ends and the
// first cell after are both kept.
//
// THE LINE TEST walks every cell a straight line from one cell's middle to another's passes through (a "supercover" line). Where the line
// passes exactly through a corner, both cells beside the corner have to fit too, so a body never cuts a corner between two cells it can't go
// through. A cell's flags already say whether the whole body fits there (npc_grid.h: a marine's square, an alien's room), so a line of cells
// the body fits on is a line the body fits along.
//
// BOUNDED. A line may cross at most kNpcGridSmoothMostLineCells cells; a longer one isn't tried, and counts as not clear. All the lines of one
// way may test at most kNpcGridSmoothMostTests cells together, and only the first kNpcGridSmoothMostCells cells of a way are pulled. Past any
// of them, the rest of the way's cells are kept as they are, which is always a way the body can go: the search found it.
//
// It knows nothing of the grid itself. The caller says where a cell is (place: its sheet, and its place i, j in that sheet's grid) and whether
// the body fits on a place of a sheet (fits), so tests can make up a grid. For the grid, place is NpcGrid::CellPlace and fits is
// NpcGrid::Flags(NpcGrid::CellAt(sheet, i, j)) & the body's bit. For a marine, fits should also refuse a steep cell (kNpcGridSteep): a
// marine only goes down one, and a straight line has no up or down, so it's kept off them and the search's own cells are kept there.
//
// NOT DONE YET (debt): a test against the real grid, with a level; whether the line should also keep a body's width off a wall, beyond what
// the cells' flags say; the numbers are first guesses.

#ifndef NPC_GRID_SMOOTH_H
#define NPC_GRID_SMOOTH_H

#include <stddef.h>
#include <stdint.h>

size_t const kNpcGridSmoothMostLineCells = 512;    // the most cells one line may cross: about 150 m of cells
size_t const kNpcGridSmoothMostTests = 200000;     // the most cells all the lines of one way may test together
size_t const kNpcGridSmoothMostCells = 4096;       // the most cells of a way that are pulled; the rest are kept as they are

// Where a cell is: its sheet, and its place in that sheet's grid
struct NpcGridSmoothPlace
{
	int sheet, i, j;
};

// The limits, which a test can lower
struct NpcGridSmoothLimits
{
	size_t mostLineCells = kNpcGridSmoothMostLineCells;
	size_t mostTests = kNpcGridSmoothMostTests;
	size_t mostCells = kNpcGridSmoothMostCells;
};

// Whether a straight line from the middle of cell (i0, j0) to the middle of cell (i1, j1), on one sheet, stays on cells fits says yes to,
// both ends included, with both cells beside any corner it passes exactly through. A line of more than mostCells cells isn't tried, and
// counts as not clear. tested counts the cells asked about.
template <typename Fits>
inline bool NpcGridSmooth_LineClear(int i0, int j0, int i1, int j1, Fits fits, size_t mostCells, size_t *tested)
{
	long long const dx = static_cast<long long>(i1) - i0, dy = static_cast<long long>(j1) - j0;
	long long const nx = dx < 0 ? -dx : dx, ny = dy < 0 ? -dy : dy;

	if (static_cast<unsigned long long>(nx + ny) + 1 > mostCells)
	{
		return false;
	}

	int const sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
	int x = i0, y = j0;

	(*tested)++;

	if (!fits(x, y))
	{
		return false;
	}

	// ix and iy count the cells stepped across and up. Going from middle to middle, the line next crosses the cells' vertical edge first if
	// (ix + 1/2) / nx is less than (iy + 1/2) / ny, which, without division, is the sign of decision below; nothing means a corner.
	for (long long ix = 0, iy = 0; ix < nx || iy < ny;)
	{
		long long const decision = (1 + 2 * ix) * ny - (1 + 2 * iy) * nx;

		if (decision == 0)
		{
			*tested += 2;

			if (!fits(x + sx, y) || !fits(x, y + sy))
			{
				return false;
			}

			x += sx;
			y += sy;
			ix++;
			iy++;
		}
		else if (decision < 0)
		{
			x += sx;
			ix++;
		}
		else
		{
			y += sy;
			iy++;
		}

		(*tested)++;

		if (!fits(x, y))
		{
			return false;
		}
	}

	return true;
}

// Which of a way's cells to keep, as their places in way (0 to count - 1), written into kept, which has room for keptRoom of them. The first
// and the last are always kept. Gives back how many were kept, or 0, with nothing to keep, for no way or too little room (keptRoom has to be
// at least count); then the caller keeps the way as it is. tests, if given, gets how many cells the lines tested.
//
// place(cell, &where) gives back whether cell is a cell, and where it is. fits(sheet, i, j) says whether the body fits on that place of that
// sheet, and has to say no for a place outside the sheet's grid.
template <typename Place, typename Fits>
inline size_t NpcGridSmooth_Pull(const uint32_t *way, size_t count, Place place, Fits fits, size_t *kept, size_t keptRoom, size_t *tests = 0,
	const NpcGridSmoothLimits &limits = NpcGridSmoothLimits())
{
	if (!way || !kept || count == 0 || keptRoom < count)
	{
		return 0;
	}

	size_t n = 0;
	size_t used = 0;
	kept[n++] = 0;

	NpcGridSmoothPlace anchor = { -1, 0, 0 }, previous = { -1, 0, 0 };
	bool anchorKnown = place(way[0], &anchor);
	bool previousKnown = anchorKnown;
	previous = anchor;

	for (size_t k = 1; k < count; k++)
	{
		NpcGridSmoothPlace here = { -1, 0, 0 };
		bool const hereKnown = place(way[k], &here);

		long long const di = static_cast<long long>(here.i) - previous.i, dj = static_cast<long long>(here.j) - previous.j;
		bool const sameRun = hereKnown && previousKnown && here.sheet == previous.sheet && di >= -1 && di <= 1 && dj >= -1 && dj <= 1;
		bool const canTry = sameRun && anchorKnown && k < limits.mostCells && used < limits.mostTests;

		if (!canTry)
		{
			// A run ends here (another sheet, a link, a step or a fall), or the limits are reached: the cell before and this one are both kept,
			// and this one starts the next run
			if (kept[n - 1] != k - 1)
			{
				kept[n++] = k - 1;
			}

			kept[n++] = k;
			anchor = here;
			anchorKnown = hereKnown;
		}
		else
		{
			int const sheet = anchor.sheet;
			size_t tested = 0;
			bool const clear = NpcGridSmooth_LineClear(anchor.i, anchor.j, here.i, here.j, [&](int i, int j) { return fits(sheet, i, j); },
				limits.mostLineCells, &tested);
			used += tested;

			// The anchor can't see this cell: the one before it was the farthest it could, so that one is kept, and the line goes on from it.
			// This cell is next to it on the sheet, as the search went, so the way from there is the search's own.
			if (!clear)
			{
				if (kept[n - 1] != k - 1)
				{
					kept[n++] = k - 1;
				}

				anchor = previous;
				anchorKnown = previousKnown;
			}
		}

		previous = here;
		previousKnown = hereKnown;
	}

	if (kept[n - 1] != count - 1)
	{
		kept[n++] = count - 1;
	}

	if (tests)
	{
		*tests = used;
	}

	return n;
}

#endif
