// npc_grid_tiles.h: THE TILES, a two-layer search over npc_grid's cells (npc_grid.h), so that a way across a whole level is found in
// milliseconds, with no limit on how far it looks. Written 2026-11-26 by Melanikus (Frost) for the AvP port, on Bishop's order for XENA
// (Hudson playing an alien, Vasquez's walk on top of it). AVP_OpenInteng's own code style; the game's own licence (License.txt).
//
// WHY. npc_grid's own search (NpcGrid::Search) looks at cells one by one. Its guess of what is left is the straight line to the goal, every
// millimetre priced as a wall's (1), while an alien's floor costs floorPrice (10) times as much, so wherever the way has to cross floor the
// search spreads wide. On Temple, from XENA's start to the end switch 157 m below, it looked at 400,000 cells in 360 to 438 ms and found no
// way (Vasquez, 2026-11-25). The study before this one (AvP-docs/engine/pathfinding-study/npc-grid-tiles.js, Third, 2026-11-08) measured the
// tiles on Office: the alien's search went from 19.5 to 3.1 ms median, and its ways were at most 7% dearer than the exact ones.
//
// THE TILES. Each sheet's grid is cut into squares of kNpcGridTile x kNpcGridTile cells, its tiles. A sheet smaller than that is one tile.
//   - A CROSSING is one move, by npc_grid's own rules for the body (below), from a cell of one tile to a cell of another. The cells a tile's
//     crossings into one other tile leave from are put in RUNS: cells that touch, corners too, on their sheet. A run longer than a tile is cut
//     every kNpcGridTile cells. Each run gives ONE crossing: its middle cell's cheapest move into that tile. Its two cells are NODES.
//   - Inside a tile, a node is joined to each of that tile's exits (the cells crossings leave from) that it can reach without leaving the
//     tile, at the true price of the way there over the tile's cells: a small search over at most kNpcGridTile^2 cells.
//   - A search is A* over the nodes, from the start cell to the goal cell, with the same guess as npc_grid's (the straight line, which never
//     says more than a way costs). Its way is the tiles' ways put end to end, then SMOOTHED: searched again over the cells of the tiles it went
//     through (Smooth). A real way, cell by cell, never cheaper than the exact one.
// Nothing is worked out before it's needed: a tile's crossings are found the first time the search comes into it, and a node's joins inside
// its tile the first time the search looks at it. Both are kept, so the next search over the same grid, body and price is faster still: only
// the joins to its own goal are new. What it keeps grows with the tiles the searches went into.
//
// WHAT IT HOLDS (Drake's review, 2026-11-26). Every byte it keeps is counted (BytesHeld: its lists' room, and an estimate on the high side for
// each entry of its two hash tables and their buckets), and counted BEFORE a list grows, not after. It never holds more than MostBytes
// (kNpcGridTilesMostBytes, 64 MB, npc_grid's own cap without "advanced AI"; SetMostBytes for another): a search that would go past it ends
// kNpcGridTooFar, and Why says so. A level is hostile input, and the game is a 32-bit program. PeakBytes says the most it held in the last
// search. Forget gives it all back.
// WHO FORGETS: the walk, at each level's start, as a grid built again for another level can sit at the same address. Start also forgets by
// itself when the grid, the body or the price changes, or the grid's sheets, cells, surface cells or links are another number.
// THE SMOOTHING runs to its end inside the Work call that finds the goal, past that call's looksAllowed: it looks at no more than the cells of
// the tiles the way went through (64 each: a way through 500 tiles, 32,000 cells), and its looks are counted.
//
// THE RULES are npc_grid's own (NpcGrid::Work, read at avpx_main 66c0c68a), move for move:
//   - on a sheet, to any of the 8 cells beside it, the body's flag on (kNpcGridStands for a marine, kNpcGridClings for an alien); a diagonal
//     needs both cells beside it too, or the body cuts a corner; it costs a cell (sqrt(2) cells for a diagonal) times the price of the cell
//     it LEAVES; a marine doesn't go up a steep floor;
//   - along a link: the straight line between the middles times the price of the cell it ARRIVES on, and for an alien 3000 more over an
//     OUTSIDE join, 10000 more over a THROUGH one (JoinOf, which gives the same answer either way round, as the build stores it for both
//     directions); a marine only onto a floor, up or down no more than a step;
//   - a marine's fall, one way, its length; an alien never falls;
//   - an alien's floor costs floorPrice times as much (1 to kAlienWaysMostFloorPrice); everything else costs 1 a millimetre.
// THE GRID it's given is any class with npc_grid's few public answers: Ready, Sheets, Cells, SurfaceCells, Links, Flags, KindOf, Middle,
// CellPlace, CellAt, LinksOf, FallsOf and JoinOf. NpcGridWaySearch below is it over the game's NpcGrid; the tests use a made-up grid with the
// same answers.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "npc_grid.h"

int const kNpcGridTile = 8;                                  // a tile's side, in cells: the study's best (T = 8, at most 7% dearer on Office)
size_t const kNpcGridTilesMostBytes = 64u * 1024u * 1024u;   // the most it may hold: npc_grid's own cap without "advanced AI" (npc_grid.h)
size_t const kNpcGridTilesMapEntryBytes = 40;                // what one entry of a hash table is taken to hold: its key and value, its links and
                                                             // the heap's own bookkeeping, on the high side (about 24 on a 32-bit build)
size_t const kNpcGridTilesBucketBytes = 16;                  // and one bucket of a hash table (two iterators: 8 on a 32-bit build)

template <class Grid>
class NpcGridTilesSearch
{
public:
	NpcGridTilesSearch() { Forget(); }

	// Lets go of every tile it has made, and gives back the room its lists held. Call it at each level's start (see WHO FORGETS, above).
	void Forget()
	{
		grid = nullptr;
		body = kNpcGridAlien;
		floorPrice = 1;
		gridCells = gridSheets = gridSurface = gridLinks = 0;
		// A list's room goes back with clear and then shrink_to_fit, which only ever lets go (it never asks for room). A hash table keeps its
		// buckets after clear, and rehash doesn't make them fewer (measured, MSVC), so each table is swapped for an empty one, which lets the
		// old room go. An empty table asks for a little room of its own; if even that can't be had, the table stays as cleared.
		tiles.clear();
		tiles.shrink_to_fit();
		nodes.clear();
		nodes.shrink_to_fit();
		open.clear();
		open.shrink_to_fit();
		smoothWay.clear();
		smoothWay.shrink_to_fit();
		tileOfKey.clear();
		nodeOfCell.clear();

		try
		{
			std::unordered_map<uint64_t, uint32_t>().swap(tileOfKey);
			std::unordered_map<uint32_t, uint32_t>().swap(nodeOfCell);
		}
		catch (...)
		{
		}

		listBytes = 0;
		peakBytes = 0;
		tilesMade = 0;
		state = kNpcGridNoWay;
		looks = nodeLooks = 0;
		number = 0;
		why = nullptr;
		startNode = goalNode = kNone;
		bestNode = kNone;
		bestFar = 0.0;
		smoothCost = 0.0;
	}

	// The most it may hold, from the next search on (kNpcGridTilesMostBytes unless this says otherwise: the walk gives the grid's own cap)
	void SetMostBytes(size_t bytes) { mostBytes = bytes; }
	size_t MostBytes() const { return mostBytes; }

	// Starts a search from one cell to another, for one body: nothing if it has started, or why not, in words (then Work says kNpcGridNoWay,
	// or kNpcGridTooFar past what it may hold). The grid has to stay as it is until the search ends (and its tiles are kept for the next one).
	const char *Start(Grid const &onGrid, uint32_t from, uint32_t to, NpcGridBody forBody, int price)
	{
		if (!onGrid.Ready())
		{
			return Refuse("the grid isn't ready");
		}

		if (price < 1 || price > kAlienWaysMostFloorPrice)
		{
			return Refuse("the floor's price is out of its range");
		}

		if (grid != &onGrid || body != forBody || floorPrice != price || gridCells != onGrid.Cells() || gridSheets != onGrid.Sheets()
			|| gridSurface != onGrid.SurfaceCells() || gridLinks != onGrid.Links())
		{
			Forget();
			grid = &onGrid;
			body = forBody;
			floorPrice = price;
			gridCells = onGrid.Cells();
			gridSheets = onGrid.Sheets();
			gridSurface = onGrid.SurfaceCells();
			gridLinks = onGrid.Links();
		}

		bit = body == kNpcGridMarine ? kNpcGridStands : kNpcGridClings;

		if (from >= gridCells || to >= gridCells || !(grid->Flags(from) & bit) || !(grid->Flags(to) & bit))
		{
			return Refuse("the start or the goal is a cell this body can't be on");
		}

		number++;
		open.clear();
		looks = nodeLooks = 0;
		why = nullptr;
		state = kNpcGridSearching;
		smoothWay.clear();
		smoothCost = 0.0;
		peakBytes = BytesHeld();
		goalCell = to;
		grid->Middle(to, goalAt);

		if (!TileOf(to, &goalTile))
		{
			state = why ? kNpcGridTooFar : kNpcGridNoWay;
			return why ? why : Refuse("the goal is on no sheet");
		}

		startNode = NodeFor(from);
		goalNode = NodeFor(to);

		if (startNode == kNone || goalNode == kNone)
		{
			state = kNpcGridTooFar;
			return why ? why : TooMuch();
		}

		Node &s = nodes[startNode];
		s.stamp = number;
		s.cost = 0.0;
		s.cameFrom = kNone;
		s.cameBy = kByStart;
		s.done = false;

		if (!Push(startNode))
		{
			state = kNpcGridTooFar;
			return TooMuch();
		}

		bestNode = startNode;
		bestFar = FarFromTheGoal(startNode);
		return nullptr;
	}

	// Carries on, doing up to looksAllowed cells' worth of work (a cell looked at in a tile's own search, or a cell of a tile being made),
	// and says how it stands: kNpcGridSearching until it's done, then kNpcGridFound or kNpcGridNoWay (kNpcGridTooFar only past MostBytes).
	// Asked again after it's ended, it gives the same answer. It never stops short of the whole grid.
	NpcGridSearch Work(size_t looksAllowed)
	{
		size_t const stopAt = looks + looksAllowed;

		while (state == kNpcGridSearching)
		{
			if (looks >= stopAt)
			{
				return state;
			}

			if (open.empty())
			{
				state = kNpcGridNoWay;
				break;
			}

			std::pop_heap(open.begin(), open.end(), std::greater<std::pair<double, uint32_t> >());
			uint32_t const u = open.back().second;
			open.pop_back();

			if (nodes[u].stamp != number || nodes[u].done)
			{
				continue;   // an older copy of a node already looked at: the guess never says more than it costs, so its first time out was its best
			}

			nodes[u].done = true;
			nodeLooks++;
			looks++;

			// ("away", not "far": the game's Windows headers make "far" and "near" into nothing)
			double const away = FarFromTheGoal(u);

			if (away < bestFar)
			{
				bestFar = away;
				bestNode = u;
			}

			if (u == goalNode)
			{
				Smooth();
				state = kNpcGridFound;
				break;
			}

			if (!LookAt(u))
			{
				state = kNpcGridTooFar;
				TooMuch();
				break;
			}
		}

		return state;
	}

	// Why the last search couldn't start or go on, or nothing
	const char *Why() const { return why; }

	// The node looked at so far whose middle is nearest the goal's, in a straight line: the start, until another is
	uint32_t BestSoFar() const { return bestNode != kNone ? nodes[bestNode].cell : kNpcGridNoCell; }

	// Every cell of the way from the start to a cell this search has looked at as a node (BestSoFar, or the goal once found), start and
	// that cell included. False, and nothing in cells, for any other cell.
	bool WayTo(uint32_t cell, std::vector<uint32_t> *cells) const
	{
		cells->clear();

		// The goal, once found: the smoothed way (Smooth, below)
		if (state == kNpcGridFound && cell == goalCell && !smoothWay.empty())
		{
			*cells = smoothWay;
			return true;
		}

		auto const found = nodeOfCell.find(cell);

		if (!grid || found == nodeOfCell.end() || nodes[found->second].stamp != number || !nodes[found->second].done)
		{
			return false;
		}

		// The nodes back to the start, then each step between two of them as the cells it goes over
		std::vector<uint32_t> chain;

		for (uint32_t k = found->second; k != kNone; k = nodes[k].cameFrom)
		{
			chain.push_back(k);

			if (chain.size() > nodes.size())
			{
				return false;   // a loop can't be, but the way is never followed round one
			}
		}

		std::reverse(chain.begin(), chain.end());
		cells->push_back(nodes[chain[0]].cell);

		for (size_t k = 1; k < chain.size(); k++)
		{
			Node const &n = nodes[chain[k]];

			if (n.cameBy == kByCrossing)
			{
				cells->push_back(n.cell);
			}
			else if (!CellsInside(nodes[chain[k - 1]].cell, n.cell, cells))
			{
				cells->clear();
				return false;
			}
		}

		return true;
	}

	size_t Looks() const { return looks; }             // cells' worth of work in this search: comparable with npc_grid's looks
	size_t NodeLooks() const { return nodeLooks; }     // of which, the nodes it looked at
	size_t TilesMade() const { return tilesMade; }     // tiles whose crossings are found, kept since the last Forget
	size_t NodesKept() const { return nodes.size(); }
	double CostOfTheWay() const { return state == kNpcGridFound ? (smoothWay.empty() ? nodes[goalNode].cost : smoothCost) : -1.0; }

	// What it holds now, and the most it held in the last search (WHAT IT HOLDS, above)
	size_t BytesHeld() const
	{
		return tiles.capacity() * sizeof(Tile) + nodes.capacity() * sizeof(Node) + open.capacity() * sizeof(std::pair<double, uint32_t>) + listBytes
			+ smoothWay.capacity() * sizeof(uint32_t)
			+ (tileOfKey.size() + nodeOfCell.size()) * kNpcGridTilesMapEntryBytes + (tileOfKey.bucket_count() + nodeOfCell.bucket_count()) * kNpcGridTilesBucketBytes;
	}

	size_t PeakBytes() const { return peakBytes; }

private:
	static uint32_t const kNone = 0xFFFFFFFFu;

	enum CameBy : uint8_t
	{
		kByStart,
		kByCrossing,
		kByInside
	};

	struct Join
	{
		uint32_t to;
		float cost;
	};

	struct Node
	{
		uint32_t cell;
		uint32_t tile;
		std::vector<Join> inside;      // to the exits of its tile it can reach without leaving it (made the first time it's looked at)
		bool insideMade = false;
		std::vector<Join> cross;       // its tile's crossings that leave from this cell, if it's an exit: one for each tile they go into
		// this search's
		uint32_t stamp = 0;
		double cost = 0.0;
		uint32_t cameFrom = kNone;
		CameBy cameBy = kByStart;
		bool done = false;
	};

	struct Tile
	{
		int sheet, i0, j0;             // its first cell, on its sheet
		bool made = false;             // its crossings are found
		std::vector<uint32_t> exits;   // its nodes that a crossing leaves from
	};

	Grid const *grid;
	NpcGridBody body;
	int floorPrice;
	size_t gridCells, gridSheets, gridSurface, gridLinks;
	uint8_t bit = kNpcGridClings;
	std::unordered_map<uint64_t, uint32_t> tileOfKey;
	std::vector<Tile> tiles;
	std::unordered_map<uint32_t, uint32_t> nodeOfCell;
	std::vector<Node> nodes;
	size_t listBytes;                 // the room of the nodes' and tiles' own lists (their joins and exits), counted as they're made
	size_t mostBytes = kNpcGridTilesMostBytes;
	size_t peakBytes;
	size_t tilesMade;
	std::vector<std::pair<double, uint32_t> > open;   // a heap, the cheapest guess on top
	NpcGridSearch state;
	size_t looks, nodeLooks;
	uint32_t number;                  // this search's number: a node's search answers are this search's only when its stamp is this
	const char *why;
	uint32_t startNode, goalNode, goalTile, goalCell;
	double goalAt[3] = { 0.0, 0.0, 0.0 };
	uint32_t bestNode;
	double bestFar;
	std::vector<uint32_t> smoothWay;   // the found way, smoothed (Smooth): empty until then, or if it couldn't be
	double smoothCost;

	const char *Refuse(const char *reason)
	{
		state = kNpcGridNoWay;
		why = reason;
		return reason;
	}

	const char *TooMuch()
	{
		why = "the tiles would hold more than they may (MostBytes)";
		return why;
	}

	// ROOM BEFORE IT'S TAKEN: would `more` bytes on top of what it holds now go past MostBytes? The peak is kept as it goes.
	bool RoomFor(size_t more)
	{
		size_t const held = BytesHeld();

		if (held > mostBytes || more > mostBytes - held)
		{
			return false;
		}

		peakBytes = std::max(peakBytes, held + more);
		return true;
	}

	// What a list grows by, at most, while one more goes in and it's full: its new room, up to twice the old (MSVC takes half as much again),
	// held beside the old room until the old is let go. Nothing if it has room.
	template <class T>
	static size_t GrowthOf(std::vector<T> const &list)
	{
		return list.size() < list.capacity() ? 0 : 2 * std::max<size_t>(list.capacity(), 1) * sizeof(T);
	}

	// ROOM IN A HASH TABLE for one more entry. A table makes more buckets by itself when it's full, by its library's own rule (MSVC: eight
	// times as many while it's small), so it's never left to: when one more entry would fill it, the room for twice its buckets is asked for
	// first (the new buckets are held beside the old for a moment), and the table is given them, here. Then the entry goes in without a
	// rehash of its own. False, and nothing changed, past MostBytes.
	template <class Map>
	bool RoomInMap(Map *map)
	{
		if (static_cast<float>(map->size() + 1) > map->max_load_factor() * static_cast<float>(map->bucket_count()))
		{
			size_t const wanted = std::max<size_t>(16, 2 * map->bucket_count());

			if (!RoomFor(wanted * kNpcGridTilesBucketBytes + kNpcGridTilesMapEntryBytes))
			{
				return false;
			}

			map->rehash(wanted);
			return true;
		}

		return RoomFor(kNpcGridTilesMapEntryBytes);
	}

	double PriceOf(uint32_t cell) const
	{
		return body == kNpcGridAlien && grid->KindOf(cell) == kAlienWaysFloor ? static_cast<double>(floorPrice) : 1.0;
	}

	static double Far(const double a[3], const double b[3])
	{
		return sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
	}

	double FarFromTheGoal(uint32_t node) const
	{
		double at[3];
		return grid->Middle(nodes[node].cell, at) ? Far(at, goalAt) : 0.0;
	}

	// Every move a cell's body can make, by npc_grid's rules (above), to visit(cell, cost). The same code for the tiles' crossings, the tiles'
	// own searches, the smoothing and the tests' exact search, so they can't differ.
	template <class Visit>
	void Moves(uint32_t c, Visit visit) const
	{
		int s = 0, i = 0, j = 0;
		double a[3];

		if (!grid->CellPlace(c, &s, &i, &j) || !grid->Middle(c, a))
		{
			return;
		}

		uint8_t const here = grid->Flags(c);
		double const price = PriceOf(c);

		for (int dj = -1; dj <= 1; dj++)
		{
			for (int di = -1; di <= 1; di++)
			{
				if (di == 0 && dj == 0)
				{
					continue;
				}

				uint32_t const n = grid->CellAt(s, i + di, j + dj);

				if (n == kNpcGridNoCell || !(grid->Flags(n) & bit))
				{
					continue;
				}

				// Going diagonally needs both cells beside it, or the body cuts a corner
				if (di != 0 && dj != 0)
				{
					uint32_t const side1 = grid->CellAt(s, i + di, j), side2 = grid->CellAt(s, i, j + dj);

					if (side1 == kNpcGridNoCell || side2 == kNpcGridNoCell || !(grid->Flags(side1) & bit) || !(grid->Flags(side2) & bit))
					{
						continue;
					}
				}

				double b[3];

				if (!grid->Middle(n, b))
				{
					continue;
				}

				// A marine doesn't climb a steep floor. y goes down, so up is a smaller y.
				if (body == kNpcGridMarine && ((grid->Flags(n) | here) & kNpcGridSteep) && b[1] < a[1])
				{
					continue;
				}

				visit(n, (di != 0 && dj != 0 ? sqrt(2.0) : 1.0) * kNpcGridCell * price);
			}
		}

		size_t count = 0;
		const uint32_t *list = grid->LinksOf(c, &count);

		for (size_t k = 0; k < count; k++)
		{
			uint32_t const n = list[k];
			double b[3];

			// Every link was made between two of the grid's cells, but a number past them is turned away all the same
			if (n >= gridCells || !(grid->Flags(n) & bit) || !grid->Middle(n, b))
			{
				continue;
			}

			if (body == kNpcGridMarine)
			{
				if (grid->KindOf(n) != kAlienWaysFloor || fabs(a[1] - b[1]) > kNpcGridStep)
				{
					continue;
				}

				// Up onto a steep floor, or up off one, by more than half a step
				if (((grid->Flags(n) | here) & kNpcGridSteep) && b[1] < a[1] - kNpcGridStep / 2)
				{
					continue;
				}
			}

			double extra = 0.0;

			if (body == kNpcGridAlien)
			{
				NpcGridJoin const join = grid->JoinOf(c, n);
				extra = join == kNpcGridJoinOutside ? kNpcGridOutsidePrice : (join == kNpcGridJoinThrough ? kNpcGridThroughPrice : 0.0);
			}

			visit(n, Far(a, b) * PriceOf(n) + extra);
		}

		if (body == kNpcGridMarine)
		{
			list = grid->FallsOf(c, &count);

			for (size_t k = 0; k < count; k++)
			{
				double b[3];

				if (list[k] >= gridCells || !grid->Middle(list[k], b))
				{
					continue;
				}

				visit(list[k], Far(a, b));
			}
		}
	}

public:
	// The exact search over every cell, by the same moves: for the tests, which set the tiles' ways beside the exact ones. Not for a frame.
	NpcGridSearch ExactSearch(Grid const &onGrid, uint32_t from, uint32_t to, NpcGridBody forBody, int price, double *cost, size_t *cellsLooked)
	{
		Forget();
		grid = &onGrid;
		body = forBody;
		floorPrice = price;
		gridCells = onGrid.Cells();
		bit = body == kNpcGridMarine ? kNpcGridStands : kNpcGridClings;
		*cost = -1.0;
		*cellsLooked = 0;

		if (from >= gridCells || to >= gridCells || !(grid->Flags(from) & bit) || !(grid->Flags(to) & bit))
		{
			return kNpcGridNoWay;
		}

		double goal[3];
		grid->Middle(to, goal);
		std::unordered_map<uint32_t, std::pair<double, bool> > seen;
		std::vector<std::pair<double, uint32_t> > heap;
		seen[from] = std::make_pair(0.0, false);
		heap.push_back(std::make_pair(0.0, from));

		while (!heap.empty())
		{
			std::pop_heap(heap.begin(), heap.end(), std::greater<std::pair<double, uint32_t> >());
			uint32_t const c = heap.back().second;
			heap.pop_back();
			std::pair<double, bool> &here = seen[c];

			if (here.second)
			{
				continue;
			}

			here.second = true;
			(*cellsLooked)++;
			double const at = here.first;

			if (c == to)
			{
				*cost = at;
				return kNpcGridFound;
			}

			Moves(c, [&](uint32_t n, double step) {
				auto const old = seen.find(n);

				if (old != seen.end() && (old->second.second || old->second.first <= at + step))
				{
					return;
				}

				double b[3];
				grid->Middle(n, b);
				seen[n] = std::make_pair(at + step, false);
				heap.push_back(std::make_pair(at + step + Far(b, goal), n));
				std::push_heap(heap.begin(), heap.end(), std::greater<std::pair<double, uint32_t> >());
			});
		}

		return kNpcGridNoWay;
	}

private:
	// A tile's key: its sheet and its place on it. A sheet has at most 16 million cells (NpcGridLimits), so a tile's place along either side
	// fits in 22 bits, and the sheet's number (at most 200,000) in 20. False for a cell on no sheet.
	bool KeyOf(uint32_t cell, uint64_t *key, int *s, int *ti, int *tj) const
	{
		int i = 0, j = 0;

		if (!grid->CellPlace(cell, s, &i, &j) || *s < 0 || i < 0 || j < 0)
		{
			return false;
		}

		*ti = i / kNpcGridTile;
		*tj = j / kNpcGridTile;
		*key = (static_cast<uint64_t>(*s) << 44) | ((static_cast<uint64_t>(*tj) & 0x3FFFFFu) << 22) | (static_cast<uint64_t>(*ti) & 0x3FFFFFu);
		return true;
	}

	// The tile a cell is in, if it has a record already: nothing is made (the smoothing looks with this)
	bool FindTile(uint32_t cell, uint32_t *tile) const
	{
		uint64_t key = 0;
		int s = 0, ti = 0, tj = 0;

		if (!KeyOf(cell, &key, &s, &ti, &tj))
		{
			return false;
		}

		auto const found = tileOfKey.find(key);

		if (found == tileOfKey.end())
		{
			return false;
		}

		*tile = found->second;
		return true;
	}

	// The tile a cell is in, its record made if it has none (crossings not yet found). False for a cell on no sheet, or past MostBytes
	// (then why says so).
	bool TileOf(uint32_t cell, uint32_t *tile)
	{
		uint64_t key = 0;
		int s = 0, ti = 0, tj = 0;

		if (!KeyOf(cell, &key, &s, &ti, &tj))
		{
			return false;
		}

		auto const found = tileOfKey.find(key);

		if (found != tileOfKey.end())
		{
			*tile = found->second;
			return true;
		}

		if (!RoomFor(GrowthOf(tiles)) || !RoomInMap(&tileOfKey))
		{
			TooMuch();
			return false;
		}

		Tile t;
		t.sheet = s;
		t.i0 = ti * kNpcGridTile;
		t.j0 = tj * kNpcGridTile;
		*tile = static_cast<uint32_t>(tiles.size());
		tiles.push_back(t);
		tileOfKey[key] = *tile;
		return true;
	}

	bool InTile(uint32_t cell, uint32_t tile) const
	{
		int s = 0, i = 0, j = 0;
		Tile const &t = tiles[tile];
		return grid->CellPlace(cell, &s, &i, &j) && s == t.sheet && i >= t.i0 && i < t.i0 + kNpcGridTile && j >= t.j0 && j < t.j0 + kNpcGridTile;
	}

	// A cell's node, made if it has none: kNone past MostBytes (then why says so), or for a cell on no sheet
	uint32_t NodeFor(uint32_t cell)
	{
		auto const found = nodeOfCell.find(cell);

		if (found != nodeOfCell.end())
		{
			return found->second;
		}

		uint32_t tile = 0;

		if (!TileOf(cell, &tile))
		{
			return kNone;
		}

		if (!RoomFor(GrowthOf(nodes)) || !RoomInMap(&nodeOfCell))
		{
			TooMuch();
			return kNone;
		}

		Node n;
		n.cell = cell;
		n.tile = tile;
		uint32_t const k = static_cast<uint32_t>(nodes.size());
		nodes.push_back(std::move(n));
		nodeOfCell[cell] = k;
		return k;
	}

	bool Push(uint32_t node)
	{
		if (!RoomFor(GrowthOf(open)))
		{
			return false;
		}

		open.push_back(std::make_pair(nodes[node].cost + FarFromTheGoal(node), node));
		std::push_heap(open.begin(), open.end(), std::greater<std::pair<double, uint32_t> >());
		return true;
	}

	// To node v at a new cost, from u. False past MostBytes.
	bool Reach(uint32_t v, double cost, uint32_t u, CameBy by)
	{
		Node &n = nodes[v];

		if (n.stamp == number && (n.done || n.cost <= cost))
		{
			return true;
		}

		n.stamp = number;
		n.cost = cost;
		n.cameFrom = u;
		n.cameBy = by;
		n.done = false;
		return Push(v);
	}

	// One node looked at: its tile's crossings found if they aren't yet, its joins inside the tile made if they aren't yet, and every one of
	// them tried; and, in the goal's tile, the way to the goal. False past MostBytes.
	bool LookAt(uint32_t u)
	{
		uint32_t const tile = nodes[u].tile;

		if (!tiles[tile].made && !MakeTile(tile))
		{
			return false;
		}

		if (!nodes[u].insideMade && !MakeInside(u))
		{
			return false;
		}

		double const here = nodes[u].cost;
		Node const &n = nodes[u];   // MakeTile and MakeInside are done, so no node is added below and n stays where it is

		for (Join const &j : n.inside)
		{
			if (!Reach(j.to, here + j.cost, u, kByInside))
			{
				return false;
			}
		}

		for (Join const &j : n.cross)
		{
			if (!Reach(j.to, here + j.cost, u, kByCrossing))
			{
				return false;
			}
		}

		// The goal's tile: the way to the goal over its cells, for this search only (the goal changes from one search to the next)
		if (tile == goalTile && u != goalNode)
		{
			double cost = 0.0;

			if (InsideTo(nodes[u].cell, goalCell, &cost) && !Reach(goalNode, here + cost, u, kByInside))
			{
				return false;
			}
		}

		return true;
	}

	// THE CROSSINGS of a tile: every move from one of its cells into another tile, by the tile it goes into; their cells in runs that touch,
	// cut every kNpcGridTile cells; one crossing for each run, from its middle cell, at its cheapest move into that tile. They're worked out
	// first and only then put on the nodes and the tile, all at once, so a tile given up halfway (past MostBytes) holds none, and its next
	// making doesn't put any on twice (Drake's review).
	bool MakeTile(uint32_t tile)
	{
		Tile const t = tiles[tile];   // a copy: tiles can grow below
		struct Out
		{
			uint32_t from, to;
			double cost;
		};
		std::unordered_map<uint32_t, std::vector<Out> > byTile;   // by the tile they go into: each leaving cell's cheapest move there
		bool full = false;

		for (int dj = 0; dj < kNpcGridTile && !full; dj++)
		{
			for (int di = 0; di < kNpcGridTile && !full; di++)
			{
				uint32_t const c = grid->CellAt(t.sheet, t.i0 + di, t.j0 + dj);
				looks++;

				if (c == kNpcGridNoCell || !(grid->Flags(c) & bit))
				{
					continue;
				}

				Moves(c, [&](uint32_t n, double cost) {
					uint32_t into = 0;

					if (full || InTile(n, tile))
					{
						return;
					}

					if (!TileOf(n, &into))
					{
						full = why != nullptr;   // past MostBytes; a cell on no sheet is only left out
						return;
					}

					std::vector<Out> &list = byTile[into];

					for (Out &o : list)
					{
						if (o.from == c)
						{
							if (cost < o.cost)
							{
								o.to = n;
								o.cost = cost;
							}

							return;
						}
					}

					list.push_back(Out{ c, n, cost });
				});
			}
		}

		if (full)
		{
			return false;
		}

		// The crossings, worked out: (the node it leaves from, the node it goes to, its cost)
		struct Crossing
		{
			uint32_t from, to;
			float cost;
		};
		// (This list, and byTile above, grow before their room is asked for, as they're only this tile's: no more than its 64 cells' moves.
		// What of them is kept is counted below, before it's put on the nodes. Drake's review.)
		std::vector<Crossing> crossings;

		for (auto &pair : byTile)
		{
			std::vector<Out> &list = pair.second;
			std::vector<bool> taken(list.size(), false);

			for (size_t first = 0; first < list.size(); first++)
			{
				if (taken[first])
				{
					continue;
				}

				// The run: the leaving cells that touch this one, and the ones that touch those, on the sheet
				std::vector<size_t> run(1, first);
				taken[first] = true;

				for (size_t k = 0; k < run.size(); k++)
				{
					int s = 0, i = 0, j = 0;
					grid->CellPlace(list[run[k]].from, &s, &i, &j);

					for (size_t m = 0; m < list.size(); m++)
					{
						int s2 = 0, i2 = 0, j2 = 0;

						if (!taken[m] && grid->CellPlace(list[m].from, &s2, &i2, &j2) && abs(i2 - i) <= 1 && abs(j2 - j) <= 1)
						{
							taken[m] = true;
							run.push_back(m);
						}
					}
				}

				// Along the run's longer side, then cut every kNpcGridTile cells, one crossing from each piece's middle cell
				int loI = 1 << 30, hiI = -(1 << 30), loJ = 1 << 30, hiJ = -(1 << 30);

				for (size_t k : run)
				{
					int s = 0, i = 0, j = 0;
					grid->CellPlace(list[k].from, &s, &i, &j);
					loI = std::min(loI, i);
					hiI = std::max(hiI, i);
					loJ = std::min(loJ, j);
					hiJ = std::max(hiJ, j);
				}

				bool const alongI = hiI - loI >= hiJ - loJ;
				std::sort(run.begin(), run.end(), [&](size_t a, size_t b) {
					int sa = 0, ia = 0, ja = 0, sb = 0, ib = 0, jb = 0;
					grid->CellPlace(list[a].from, &sa, &ia, &ja);
					grid->CellPlace(list[b].from, &sb, &ib, &jb);
					return alongI ? (ia != ib ? ia < ib : ja < jb) : (ja != jb ? ja < jb : ia < ib);
				});

				for (size_t k = 0; k < run.size(); k += kNpcGridTile)
				{
					size_t const pieceEnd = std::min(run.size(), k + kNpcGridTile);
					Out const &o = list[run[k + (pieceEnd - k) / 2]];
					uint32_t const a = NodeFor(o.from), b = NodeFor(o.to);

					if (a == kNone || b == kNone)
					{
						return false;   // past MostBytes (nodes made so far stay, with no crossing on them: harmless)
					}

					crossings.push_back(Crossing{ a, b, static_cast<float>(o.cost) });
				}
			}
		}

		// Now on the nodes and the tile, all at once. A cell can be the middle of runs into several tiles (a corner cell into three): it keeps
		// a crossing into each, and is one exit of its tile.
		size_t more = 0;
		std::vector<uint32_t> exits;

		for (Crossing const &c : crossings)
		{
			more += sizeof(Join) * 2;   // room on the high side: a list that grows may double

			if (nodes[c.from].cross.empty() && std::find(exits.begin(), exits.end(), c.from) == exits.end())
			{
				exits.push_back(c.from);
			}
		}

		more += exits.size() * sizeof(uint32_t);

		if (!RoomFor(more))
		{
			return false;
		}

		for (Crossing const &c : crossings)
		{
			size_t const before = nodes[c.from].cross.capacity();
			nodes[c.from].cross.push_back(Join{ c.to, c.cost });
			listBytes += (nodes[c.from].cross.capacity() - before) * sizeof(Join);
		}

		tiles[tile].exits = std::move(exits);
		listBytes += tiles[tile].exits.capacity() * sizeof(uint32_t);
		tiles[tile].made = true;
		tilesMade++;
		return true;
	}

	// The way from one cell of a tile to others of it, over its cells only: its cost to each of targets (negative: not reached), and if
	// cameFrom is given, each cell's way back, by its place in the tile
	void SearchInside(uint32_t from, uint32_t tile, std::vector<uint32_t> const &targets, std::vector<double> *costs, std::vector<int> *cameFromOut) const
	{
		Tile const &t = tiles[tile];
		int const n = kNpcGridTile * kNpcGridTile;
		double cost[kNpcGridTile * kNpcGridTile];
		int cameFrom[kNpcGridTile * kNpcGridTile];
		bool done[kNpcGridTile * kNpcGridTile];
		uint32_t cellAt[kNpcGridTile * kNpcGridTile];

		for (int k = 0; k < n; k++)
		{
			cost[k] = -1.0;
			cameFrom[k] = -1;
			done[k] = false;
			cellAt[k] = grid->CellAt(t.sheet, t.i0 + k % kNpcGridTile, t.j0 + k / kNpcGridTile);
		}

		auto const placeOf = [&](uint32_t cell) -> int {
			int s = 0, i = 0, j = 0;

			if (!grid->CellPlace(cell, &s, &i, &j) || s != t.sheet || i < t.i0 || i >= t.i0 + kNpcGridTile || j < t.j0 || j >= t.j0 + kNpcGridTile)
			{
				return -1;
			}

			return (j - t.j0) * kNpcGridTile + (i - t.i0);
		};

		int const start = placeOf(from);
		costs->assign(targets.size(), -1.0);

		if (start < 0)
		{
			return;
		}

		std::vector<std::pair<double, int> > heap;
		cost[start] = 0.0;
		heap.push_back(std::make_pair(0.0, start));

		while (!heap.empty())
		{
			std::pop_heap(heap.begin(), heap.end(), std::greater<std::pair<double, int> >());
			int const k = heap.back().second;
			heap.pop_back();

			if (done[k])
			{
				continue;
			}

			done[k] = true;
			double const at = cost[k];

			Moves(cellAt[k], [&](uint32_t cell, double step) {
				int const m = placeOf(cell);

				if (m < 0 || done[m] || (cost[m] >= 0.0 && cost[m] <= at + step))
				{
					return;
				}

				cost[m] = at + step;
				cameFrom[m] = k;
				heap.push_back(std::make_pair(cost[m], m));
				std::push_heap(heap.begin(), heap.end(), std::greater<std::pair<double, int> >());
			});
		}

		for (size_t k = 0; k < targets.size(); k++)
		{
			int const m = placeOf(targets[k]);
			(*costs)[k] = m >= 0 && done[m] ? cost[m] : -1.0;
		}

		if (cameFromOut)
		{
			cameFromOut->assign(cameFrom, cameFrom + n);
		}
	}

	// A node's joins inside its tile: to each of the tile's exits it reaches without leaving the tile, at the cost over its cells
	bool MakeInside(uint32_t u)
	{
		uint32_t const tile = nodes[u].tile;
		std::vector<uint32_t> const &exits = tiles[tile].exits;
		std::vector<uint32_t> targets;

		for (uint32_t e : exits)
		{
			targets.push_back(nodes[e].cell);
		}

		std::vector<double> costs;
		SearchInside(nodes[u].cell, tile, targets, &costs, nullptr);
		looks += static_cast<size_t>(kNpcGridTile * kNpcGridTile);

		// (This list grows before its room is asked for, as it's only this tile's exits, at most one for each of its 64 cells. Its room is
		// counted below, before it's kept. Drake's review.)
		std::vector<Join> joins;

		for (size_t k = 0; k < exits.size(); k++)
		{
			if (exits[k] != u && costs[k] >= 0.0)
			{
				joins.push_back(Join{ exits[k], static_cast<float>(costs[k]) });
			}
		}

		joins.shrink_to_fit();

		if (!RoomFor(joins.capacity() * sizeof(Join)))
		{
			return false;
		}

		listBytes += joins.capacity() * sizeof(Join);
		nodes[u].inside = std::move(joins);
		nodes[u].insideMade = true;
		return true;
	}

	// THE SMOOTHING (the study's "margin 0"): once the way over the nodes is found, it's searched again over cells, by the same moves, but
	// only over the cells of the tiles that way went through. The tiles' way is one of the ways it can take, so this one is never dearer, and
	// it's free to cut across a tile where the tiles' way went round by an exit. Its looks are counted, and it runs to its end (SMOOTHING,
	// above). Its tiles are only looked up (FindTile), never made. What it holds while it runs is counted too, every 256 cells it looks at,
	// and if it would go past MostBytes it stops there. If it found none (it can't, as the tiles' way is there to take), or stopped, the
	// tiles' way stays as it is.
	void Smooth()
	{
		std::vector<uint32_t> corridor;

		for (uint32_t k = goalNode; k != kNone; k = nodes[k].cameFrom)
		{
			corridor.push_back(nodes[k].tile);

			if (corridor.size() > nodes.size())
			{
				return;
			}
		}

		std::sort(corridor.begin(), corridor.end());
		corridor.erase(std::unique(corridor.begin(), corridor.end()), corridor.end());

		uint32_t const from = nodes[startNode].cell;
		std::unordered_map<uint32_t, std::pair<double, uint32_t> > seen;   // a cell's cost, and where it came from
		std::unordered_map<uint32_t, bool> done;
		std::vector<std::pair<double, uint32_t> > heap;
		seen[from] = std::make_pair(0.0, kNpcGridNoCell);
		heap.push_back(std::make_pair(0.0, from));

		while (!heap.empty())
		{
			std::pop_heap(heap.begin(), heap.end(), std::greater<std::pair<double, uint32_t> >());
			uint32_t const c = heap.back().second;
			heap.pop_back();

			if (done[c])
			{
				continue;
			}

			done[c] = true;
			looks++;
			double const at = seen[c].first;

			// What it holds while it runs: its two tables (each entry, each bucket) and its heap, as it would be if each doubled now
			if ((looks & 255) == 0 && !RoomFor(2 * ((seen.size() + done.size()) * kNpcGridTilesMapEntryBytes + (seen.bucket_count() + done.bucket_count()) *
				kNpcGridTilesBucketBytes + heap.capacity() * sizeof(std::pair<double, uint32_t>))))
			{
				return;
			}

			if (c == goalCell)
			{
				std::vector<uint32_t> back;

				for (uint32_t k = goalCell; k != kNpcGridNoCell; k = seen[k].second)
				{
					back.push_back(k);

					if (back.size() > seen.size())
					{
						return;
					}
				}

				if (!RoomFor(back.size() * sizeof(uint32_t)))
				{
					return;
				}

				smoothWay.assign(back.rbegin(), back.rend());
				smoothCost = at;
				return;
			}

			Moves(c, [&](uint32_t n, double step) {
				uint32_t tile = 0;

				if (!FindTile(n, &tile) || !std::binary_search(corridor.begin(), corridor.end(), tile))
				{
					return;
				}

				auto const old = seen.find(n);

				if ((old != seen.end() && old->second.first <= at + step) || done[n])
				{
					return;
				}

				double b[3];
				grid->Middle(n, b);
				seen[n] = std::make_pair(at + step, c);
				heap.push_back(std::make_pair(at + step + Far(b, goalAt), n));
				std::push_heap(heap.begin(), heap.end(), std::greater<std::pair<double, uint32_t> >());
			});
		}
	}

	// The cost from one cell to another of the same tile, over its cells: false if it doesn't get there inside the tile
	bool InsideTo(uint32_t from, uint32_t to, double *cost)
	{
		auto const found = nodeOfCell.find(from);

		if (found == nodeOfCell.end())
		{
			return false;
		}

		std::vector<double> costs;
		SearchInside(from, nodes[found->second].tile, std::vector<uint32_t>(1, to), &costs, nullptr);
		looks += static_cast<size_t>(kNpcGridTile * kNpcGridTile);
		*cost = costs[0];
		return costs[0] >= 0.0;
	}

	// The cells of the way from one cell to another of its tile, the first left out (it's on the list already), the last put in
	bool CellsInside(uint32_t from, uint32_t to, std::vector<uint32_t> *cells) const
	{
		auto const found = nodeOfCell.find(from);

		if (found == nodeOfCell.end())
		{
			return false;
		}

		uint32_t const tile = nodes[found->second].tile;
		std::vector<double> costs;
		std::vector<int> cameFrom;
		SearchInside(from, tile, std::vector<uint32_t>(1, to), &costs, &cameFrom);

		if (costs[0] < 0.0)
		{
			return false;
		}

		Tile const &t = tiles[tile];
		int s = 0, i = 0, j = 0;
		grid->CellPlace(to, &s, &i, &j);
		std::vector<uint32_t> back;

		for (int k = (j - t.j0) * kNpcGridTile + (i - t.i0); k >= 0; k = cameFrom[k])
		{
			back.push_back(grid->CellAt(t.sheet, t.i0 + k % kNpcGridTile, t.j0 + k / kNpcGridTile));

			if (back.size() > static_cast<size_t>(kNpcGridTile * kNpcGridTile))
			{
				return false;
			}
		}

		// back runs from `to` to `from`: `from` is on the list already
		for (size_t k = back.size(); k-- > 1;)
		{
			cells->push_back(back[k - 1]);
		}

		return true;
	}
};

// The search over the game's own grid, for XENA's walk (Vasquez, hudson_play.cpp)
typedef NpcGridTilesSearch<NpcGrid> NpcGridWaySearch;
