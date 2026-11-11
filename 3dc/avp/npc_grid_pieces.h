// The grid's PIECES: which cells a body can get between at all (#145). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY. A grid search that finds no way has looked at every cell it can reach before it says so. On BAR-01 (2026-11-08) one alien's
// search did that 7 times running, 148,759 cells and about 125 ms each time, because the cells it started on don't join its goal's.
// Cells that join each other make a PIECE: a floor and the walls and ceiling it runs into, say, or a ledge nothing reaches. If the start
// and the goal are in different pieces there is certainly no way, and the search can say so at once, without looking at anything.
//
// HOW. Once, after the grid is built, every cell gets the number of its piece:
//
//   1. Every move the search can make joins two cells: a step to one of the 8 cells round it on its sheet, a link (from one sheet to
//      another, or a step up), and, for a body that falls, a fall. Joined cells are in the same piece. (The joining is the "union-find" way: each cell points at another in its piece, and the one at the
//      end of the pointing is the piece's first cell.)
//   2. The pieces are counted, and the biggest kNpcGridPiecesMostKnown of them get a number each, the biggest first. Every other cell,
//      in a smaller piece or one the body can't be on, is "not known".
//   3. Each cell's number is kept, in 2 bytes.
//
// WHY IT'S NEVER WRONG. The joining counts more moves than the search makes, never fewer:
//   - the search turns some moves down (a marine never climbs a steep floor, never takes a link that isn't onto a floor or is too high
//     a step, never goes through a shut door, nobody cuts a corner without both cells beside it), and here every one of them counts,
//     both ways;
//   IF THE SEARCH EVER MAKES A MOVE OF A NEW KIND (npc_grid.cpp, where it looks round a cell), it has to be joined here too, or the
//   pieces can say "apart" where there's a way. tests/npc_grid_pieces_test.cpp checks it against a copy of the search's moves.
//   - a fall only goes down, and here it joins both ways;
//   - a link that's only stored one way joins both ways too.
// So two cells the search can get between are always in one piece. Pieces can be joined where the search would find no way, and then
// the search runs as it does today: CertainlyApart says "apart" only when both cells have a number and the numbers differ. A cell in
// a piece past the biggest kNpcGridPiecesMostKnown is "not known", so it never says "apart" for one. That costs nothing that matters: a
// small piece is quick to search all of anyway. The 148,759-cell searches come from big pieces, and big pieces always get a number.
//
// WHAT IT NEEDS FROM THE GRID, through a few small calls, so it can be tried on made-up grids (tests/npc_grid_pieces_test.cpp):
//   size_t Cells(); uint8_t Flags(cell); bool CellPlace(cell, &sheet, &i, &j); uint32_t CellAt(sheet, i, j);
//   const uint32_t *LinksOf(cell, &count); const uint32_t *FallsOf(cell, &count).
// A cell number the grid gives back that isn't one of its cells is turned away.
//
// HOW MUCH IT TAKES. While it works: 4 bytes a cell, a bit a cell if the body falls, and 8 bytes a piece. It keeps 2 bytes a cell. If
// that would go past the most it's allowed (Begin's mostBytes), or memory runs out, it GIVES UP: every cell is "not known", nothing is
// ever refused because of it, and WhyGaveUp says why. It works in steps (Step's budget is how many cells to go through), so whoever
// calls it can stop between steps.
#ifndef NPC_GRID_PIECES_H
#define NPC_GRID_PIECES_H

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <new>
#include <vector>

uint16_t const kNpcGridPieceNotKnown = 0xFFFF;   // the number of a cell whose piece isn't known
size_t const kNpcGridPiecesMostKnown = 65534;    // the most pieces that get a number: 0 to 65,533

class NpcGridPieces
{
public:
	NpcGridPieces() : stage(kIdle), cells(0), bit(0), withFalls(false), mostKnown(0), mostBytes(0), at(0), why(nullptr) {}

	// Starts finding the pieces of a grid of `cells` cells, for the body whose cells have `bit` in their flags. withFalls: the body
	// falls (a marine). mostKnown: how many of the biggest pieces get a number, at most kNpcGridPiecesMostKnown. mostBytes: the most
	// memory it may hold at once. Gives back nothing if it started, or why not.
	const char *Begin(size_t cellCount, uint8_t bodyBit, bool bodyFalls, size_t mostPieces, size_t mostMemory)
	{
		LetGo();

		if (bodyBit == 0)
		{
			return GiveUp("no body was named");
		}

		// A cell's place in the joining is kept with its top bit free (see Number), so there can't be 2^31 cells or more
		if (cellCount >= static_cast<size_t>(kNoPiece & ~kNumbered))
		{
			return GiveUp("too many cells");
		}

		cells = cellCount;
		bit = bodyBit;
		withFalls = bodyFalls;
		mostKnown = std::min(mostPieces, kNpcGridPiecesMostKnown);
		mostBytes = mostMemory;

		// A grid with no cells has no pieces: done already, with nothing kept
		if (cells == 0)
		{
			stage = kDone;
			return nullptr;
		}

		// Every cell starts as a piece of its own (this one pass over the cells is done here, not in steps)
		if (!Take(&parent, cells))
		{
			return why;
		}

		for (size_t c = 0; c < cells; c++)
		{
			parent[c] = static_cast<uint32_t>(c);
		}

		if (withFalls && !Take(&fallTarget, cells))
		{
			return why;
		}

		stage = withFalls ? kMarkFalls : kJoin;
		at = 0;
		return nullptr;
	}

	// Goes on with the work, through at most `budget` cells (putting the pieces in size order counts as one). True once it's done, or
	// has given up. The grid has to be the same one, with the same cells, as at Begin.
	template <class Grid>
	bool Step(const Grid &grid, size_t budget)
	{
		if (stage == kIdle || stage == kDone)
		{
			return true;
		}

		if (grid.Cells() != cells)
		{
			GiveUp("the grid changed while its pieces were being found");
			return true;
		}

		for (; budget > 0 && stage != kDone; budget--)
		{
			switch (stage)
			{
			case kMarkFalls: MarkFalls(grid); break;
			case kJoin: Join(grid); break;
			case kNumber: Number(grid); break;
			case kRank: Rank(); break;
			case kWrite: Write(); break;
			default: break;
			}
		}

		return stage == kDone;
	}

	bool Done() const { return stage == kDone; }
	bool GaveUp() const { return why != nullptr; }
	const char *WhyGaveUp() const { return why; }
	size_t Pieces() const { return pieces; }

	// The number of a cell's piece, or kNpcGridPieceNotKnown: before it's done, if it gave up, for a cell past the grid, for a cell the
	// body can't be on, and for one in a piece past the biggest mostKnown
	uint16_t PieceOf(uint32_t cell) const
	{
		return stage == kDone && cell < labels.size() ? labels[cell] : kNpcGridPieceNotKnown;
	}

	// True only when the search certainly can't get from one cell to the other: both pieces are known, and they're different
	bool CertainlyApart(uint32_t a, uint32_t b) const
	{
		uint16_t const pa = PieceOf(a), pb = PieceOf(b);
		return pa != kNpcGridPieceNotKnown && pb != kNpcGridPieceNotKnown && pa != pb;
	}

	size_t BytesKept() const { return labels.capacity() * sizeof(uint16_t); }
	size_t BytesHeld() const
	{
		return parent.capacity() * sizeof(uint32_t) + fallTarget.capacity() + sizes.capacity() * sizeof(uint32_t) +
			order.capacity() * sizeof(uint32_t) + BytesKept();
	}

	// Lets go of everything, and it's as if it had never started
	void LetGo()
	{
		Free();
		std::vector<uint16_t>().swap(labels);
		stage = kIdle;
		cells = 0;
		at = 0;
		pieces = 0;
		why = nullptr;
	}

private:
	enum Stage { kIdle, kMarkFalls, kJoin, kNumber, kRank, kWrite, kDone };

	// While numbering, a cell's entry in parent becomes its piece's number with this bit set, so it can't be taken for a cell
	static constexpr uint32_t kNumbered = 0x80000000u;
	static constexpr uint32_t kNoPiece = 0xFFFFFFFFu;   // a cell the body can't be on: it's in no piece

	Stage stage;
	size_t cells;
	uint8_t bit;
	bool withFalls;
	size_t mostKnown;
	size_t mostBytes;
	size_t at;                         // the next cell (or piece) this stage looks at
	size_t pieces = 0;
	const char *why;                   // why it gave up, or nothing
	std::vector<uint32_t> parent;      // the joining; then each cell's piece (Number)
	std::vector<uint8_t> fallTarget;   // 1 for a cell a fall lands on: a falling body can be there even if its flags say it can't stand
	std::vector<uint32_t> sizes;       // each piece's size; then each piece's number (Rank)
	std::vector<uint32_t> order;       // the pieces, biggest first
	std::vector<uint16_t> labels;      // what's kept: each cell's piece number

	const char *GiveUp(const char *reason)
	{
		Free();
		std::vector<uint16_t>().swap(labels);
		why = reason;
		stage = kDone;
		return why;
	}

	void Free()
	{
		std::vector<uint32_t>().swap(parent);
		std::vector<uint8_t>().swap(fallTarget);
		std::vector<uint32_t>().swap(sizes);
		std::vector<uint32_t>().swap(order);
	}

	// Makes a list `count` long, if that keeps within mostBytes and there's the memory; otherwise it gives up and says false
	template <class T>
	bool Take(std::vector<T> *list, size_t count)
	{
		size_t const now = BytesHeld() - list->capacity() * sizeof(T);

		if (count > (SIZE_MAX - now) / sizeof(T) || now + count * sizeof(T) > mostBytes)
		{
			GiveUp("it would take more memory than it's allowed");
			return false;
		}

		try
		{
			list->assign(count, T());
		}
		catch (const std::bad_alloc &)
		{
			GiveUp("it ran out of memory");
			return false;
		}

		return true;
	}

	bool Has(const uint8_t flags) const { return (flags & bit) != 0; }

	// Whether the body can be on a cell at all: its flags say so, or (for a body that falls) a fall lands on it
	template <class Grid>
	bool CanBeOn(const Grid &grid, uint32_t cell) const
	{
		return Has(grid.Flags(cell)) || (withFalls && fallTarget[cell] != 0);
	}

	// The piece's first cell: follow the pointing to its end. Each step points at a lower cell (see Unite), so it always ends. On the way
	// each cell is pointed two along ("path halving"), so the next time is quicker.
	uint32_t Root(uint32_t x)
	{
		while (parent[x] != x)
		{
			parent[x] = parent[parent[x]];
			x = parent[x];
		}

		return x;
	}

	// The two cells' pieces become one: the higher first cell points at the lower, so a cell always points at a lower one
	void Unite(uint32_t a, uint32_t b)
	{
		uint32_t const ra = Root(a), rb = Root(b);

		if (ra < rb)
		{
			parent[rb] = ra;
		}
		else if (rb < ra)
		{
			parent[ra] = rb;
		}
	}

	// 0. For a body that falls: every cell a fall lands on is marked, so it counts as a cell the body can be on
	template <class Grid>
	void MarkFalls(const Grid &grid)
	{
		size_t count = 0;
		const uint32_t *falls = grid.FallsOf(static_cast<uint32_t>(at), &count);

		for (size_t k = 0; falls && k < count; k++)
		{
			if (falls[k] < cells)
			{
				fallTarget[falls[k]] = 1;
			}
		}

		if (++at == cells)
		{
			stage = kJoin;
			at = 0;
		}
	}

	// 1. One cell's moves, each joining it to where it goes: the four cells after it on its sheet (the four before it join it when they
	// have their turn, so every pair round a cell is looked at once), its links, and its falls
	template <class Grid>
	void Join(const Grid &grid)
	{
		uint32_t const c = static_cast<uint32_t>(at);

		if (CanBeOn(grid, c))
		{
			int sheet = 0, i = 0, j = 0;

			if (grid.CellPlace(c, &sheet, &i, &j))
			{
				int const offsets[4][2] = { { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } };
				bool const cHas = Has(grid.Flags(c));

				for (int k = 0; k < 4; k++)
				{
					int const di = offsets[k][0], dj = offsets[k][1];
					uint32_t const n = grid.CellAt(sheet, i + di, j + dj);

					if (n >= cells)
					{
						continue;
					}

					// The search steps from a cell it's on to one round it that has the body's flag. So c and n are joined if it can
					// step either way: onto n from c, or onto c from n.
					bool const nHas = Has(grid.Flags(n));

					if (!(nHas || (cHas && CanBeOn(grid, n))))
					{
						continue;
					}

					// Across a corner too, whatever is beside it. The search only cuts a corner with both cells beside it, and then it could
					// have gone round through either of them anyway, so this joins nothing new today. It's here so that the pieces stay
					// right if the search ever cuts corners more freely: joining too much is safe, too little isn't.
					Unite(c, n);
				}
			}

			// A link onto a cell with the body's flag, as the search takes it
			size_t count = 0;
			const uint32_t *links = grid.LinksOf(c, &count);

			for (size_t k = 0; links && k < count; k++)
			{
				if (links[k] < cells && Has(grid.Flags(links[k])))
				{
					Unite(c, links[k]);
				}
			}

			// A fall, for a body that falls: the search takes it whatever the flags of where it lands
			if (withFalls)
			{
				const uint32_t *falls = grid.FallsOf(c, &count);

				for (size_t k = 0; falls && k < count; k++)
				{
					if (falls[k] < cells)
					{
						Unite(c, falls[k]);
					}
				}
			}
		}

		if (++at == cells)
		{
			stage = kNumber;
			at = 0;
		}
	}

	// 2. The pieces are numbered in the order of their first cells, and counted. A cell always points at a lower one, and a piece's
	// first cell is its lowest, so going up from cell 0, every cell a cell points at has already been numbered: its number is read from
	// there. A cell's entry becomes its piece's number, with kNumbered set.
	template <class Grid>
	void Number(const Grid &grid)
	{
		uint32_t const c = static_cast<uint32_t>(at);

		if (!CanBeOn(grid, c))
		{
			parent[c] = kNoPiece;
		}
		else if (parent[c] == c)
		{
			// The first cell of a new piece
			if (sizes.size() == sizes.capacity())
			{
				std::vector<uint32_t> bigger;
				size_t const room = std::max<size_t>(1024, sizes.capacity() * 2);

				if (!Take(&bigger, room))
				{
					return;
				}

				bigger.assign(sizes.begin(), sizes.end());
				sizes.swap(bigger);
			}

			parent[c] = kNumbered | static_cast<uint32_t>(sizes.size());
			sizes.push_back(1);
		}
		else
		{
			uint32_t const above = parent[c];
			uint32_t const entry = above < c ? parent[above] : kNoPiece;

			// That cell was numbered on its way past: if it wasn't, the joining is broken, and nothing is trusted
			if (entry == kNoPiece || !(entry & kNumbered) || (entry & ~kNumbered) >= sizes.size())
			{
				GiveUp("the pieces didn't number as they should");
				return;
			}

			sizes[entry & ~kNumbered]++;
			parent[c] = entry;
		}

		if (++at == cells)
		{
			pieces = sizes.size();
			stage = kRank;
			at = 0;
		}
	}

	// 3. The biggest mostKnown pieces get the numbers 0, 1, 2 and up, the biggest first (two the same size go by their first cell, so
	// the same grid always gets the same numbers). sizes then holds each piece's number, or kNpcGridPieceNotKnown.
	void Rank()
	{
		if (!Take(&order, pieces))
		{
			return;
		}

		for (size_t p = 0; p < pieces; p++)
		{
			order[p] = static_cast<uint32_t>(p);
		}

		const std::vector<uint32_t> &size = sizes;
		auto const bigger = [&size](uint32_t a, uint32_t b) { return size[a] != size[b] ? size[a] > size[b] : a < b; };
		size_t const known = std::min(mostKnown, pieces);

		if (known < pieces)
		{
			std::nth_element(order.begin(), order.begin() + known, order.end(), bigger);
		}

		std::sort(order.begin(), order.begin() + known, bigger);

		for (size_t p = 0; p < pieces; p++)
		{
			sizes[p] = kNpcGridPieceNotKnown;
		}

		for (size_t r = 0; r < known; r++)
		{
			sizes[order[r]] = static_cast<uint32_t>(r);
		}

		std::vector<uint32_t>().swap(order);

		if (Take(&labels, cells))
		{
			stage = kWrite;
			at = 0;
		}
	}

	// 4. Each cell's number is written where it's kept; once they all are, everything used on the way is let go of
	void Write()
	{
		uint32_t const entry = parent[at];
		labels[at] = entry == kNoPiece ? kNpcGridPieceNotKnown : static_cast<uint16_t>(sizes[entry & ~kNumbered]);

		if (++at == cells)
		{
			Free();
			stage = kDone;
		}
	}
};

#endif
