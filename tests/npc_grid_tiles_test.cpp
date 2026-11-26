// The test for 3dc/avp/npc_grid_tiles.h, the tiles: the two-layer search over npc_grid's cells. Written 2026-11-26 by Melanikus (Frost) for
// the AvP port. It runs on made-up grids (TestGrid, below) with npc_grid's few public answers, so no level is needed:
//   - every way it finds is a real way, move by move by npc_grid's rules, and costs what it says;
//   - it finds a way exactly when there is one (the exact search over every cell, by the same moves, says), on worlds with walls, gaps,
//     several sheets and links, and no dearer than kMostDearer times the exact way;
//   - spread over frames it gives the same answer as all at once; its best-so-far is a real way, and nearer the goal as it goes;
//   - a second search over the same grid keeps the tiles of the first; a grid, body or price it hasn't seen starts again;
//   - what it's given from outside is checked: a cell off the grid, one the body can't be on, a price out of range, a grid not ready.
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "npc_grid_tiles.h"

static int checksFailed = 0;

static void Check(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// How much dearer than the exact way a tiles way may be, on these worlds (the study's most was 1.068 on Office)
static double const kMostDearer = 1.15;

// A made-up grid: flat sheets of cells, each a plane in the world with two axes, a kind, and the flags of its cells; links and falls by hand
class TestGrid
{
public:
	struct Sheet
	{
		uint32_t first;
		int w, h;
		double origin[3], u[3], v[3];   // the corner of the sheet, and its two axes (unit length)
		AlienWaysKind kind;
	};

	bool ready = true;

	int AddSheet(int w, int h, double ox, double oy, double oz, double ux, double uy, double uz, double vx, double vy, double vz, AlienWaysKind kind)
	{
		Sheet s = { static_cast<uint32_t>(flags.size()), w, h, { ox, oy, oz }, { ux, uy, uz }, { vx, vy, vz }, kind };
		sheets.push_back(s);
		flags.resize(flags.size() + static_cast<size_t>(w) * h, kNpcGridStands | kNpcGridClings);
		links.resize(flags.size());
		falls.resize(flags.size());
		return static_cast<int>(sheets.size()) - 1;
	}

	void Block(int sheet, int i, int j) { flags[CellAt(sheet, i, j)] = 0; }
	void Link(uint32_t a, uint32_t b, NpcGridJoin join = kNpcGridJoinPlain)
	{
		links[a].push_back(b);
		links[b].push_back(a);
		joins.push_back(std::make_pair(std::make_pair(a, b), join));
		joins.push_back(std::make_pair(std::make_pair(b, a), join));
	}
	void Fall(uint32_t from, uint32_t to) { falls[from].push_back(to); }

	// npc_grid's answers
	bool Ready() const { return ready; }
	size_t Cells() const { return flags.size(); }
	uint8_t Flags(uint32_t cell) const { return cell < flags.size() ? flags[cell] : 0; }

	AlienWaysKind KindOf(uint32_t cell) const
	{
		int const s = SheetOf(cell);
		return s >= 0 ? sheets[s].kind : kAlienWaysFloor;
	}

	bool Middle(uint32_t cell, double out[3]) const
	{
		int s = 0, i = 0, j = 0;

		if (!CellPlace(cell, &s, &i, &j))
		{
			return false;
		}

		Sheet const &t = sheets[s];

		for (int k = 0; k < 3; k++)
		{
			out[k] = t.origin[k] + (i + 0.5) * kNpcGridCell * t.u[k] + (j + 0.5) * kNpcGridCell * t.v[k];
		}

		return true;
	}

	bool CellPlace(uint32_t cell, int *sheet, int *i, int *j) const
	{
		int const s = SheetOf(cell);

		if (s < 0)
		{
			return false;
		}

		*sheet = s;
		*i = static_cast<int>((cell - sheets[s].first) % static_cast<uint32_t>(sheets[s].w));
		*j = static_cast<int>((cell - sheets[s].first) / static_cast<uint32_t>(sheets[s].w));
		return true;
	}

	uint32_t CellAt(int sheet, int i, int j) const
	{
		if (sheet < 0 || sheet >= static_cast<int>(sheets.size()) || i < 0 || j < 0 || i >= sheets[sheet].w || j >= sheets[sheet].h)
		{
			return kNpcGridNoCell;
		}

		return sheets[sheet].first + static_cast<uint32_t>(j) * static_cast<uint32_t>(sheets[sheet].w) + static_cast<uint32_t>(i);
	}

	const uint32_t *LinksOf(uint32_t cell, size_t *count) const
	{
		*count = cell < links.size() ? links[cell].size() : 0;
		return *count ? links[cell].data() : nullptr;
	}

	const uint32_t *FallsOf(uint32_t cell, size_t *count) const
	{
		*count = cell < falls.size() ? falls[cell].size() : 0;
		return *count ? falls[cell].data() : nullptr;
	}

	NpcGridJoin JoinOf(uint32_t a, uint32_t b) const
	{
		for (auto const &j : joins)
		{
			if (j.first.first == a && j.first.second == b)
			{
				return j.second;
			}
		}

		return kNpcGridJoinPlain;
	}

	int SheetOf(uint32_t cell) const
	{
		for (size_t s = 0; s < sheets.size(); s++)
		{
			if (cell >= sheets[s].first && cell < sheets[s].first + static_cast<uint32_t>(sheets[s].w * sheets[s].h))
			{
				return static_cast<int>(s);
			}
		}

		return -1;
	}

	std::vector<Sheet> sheets;
	std::vector<uint8_t> flags;
	std::vector<std::vector<uint32_t> > links, falls;
	std::vector<std::pair<std::pair<uint32_t, uint32_t>, NpcGridJoin> > joins;
};

typedef NpcGridTilesSearch<TestGrid> Tiles;

static uint32_t randomState = 1451;

static uint32_t Random(uint32_t below)
{
	randomState = randomState * 1664525u + 1013904223u;
	return (randomState >> 8) % below;
}

// Is this way a real one, move by move, by the exact search's own moves, from `from` to `to`? Its cost, in *cost.
static bool IsARealWay(TestGrid const &g, std::vector<uint32_t> const &way, uint32_t from, uint32_t to, NpcGridBody body, int price, double *cost)
{
	*cost = 0.0;

	if (way.empty() || way.front() != from || way.back() != to)
	{
		return false;
	}

	for (size_t k = 1; k < way.size(); k++)
	{
		Tiles one;
		double step = -1.0;
		size_t looked = 0;

		// The cheapest single move from way[k-1] to way[k]: an exact search whose way is one move long costs exactly that move
		if (one.ExactSearch(g, way[k - 1], way[k], body, price, &step, &looked) != kNpcGridFound)
		{
			return false;
		}

		// A one-move way: the exact search's cost can be no more than the move's, and a longer way to a neighbour is never cheaper than one
		*cost += step;
	}

	return true;
}

// A floor with walls and gaps, a wall sheet standing up from its far edge, and links between them
static void MakeWorld(TestGrid *g, int w, int h, int walls, bool closed)
{
	int const floor = g->AddSheet(w, h, 0, 0, 0, 1, 0, 0, 0, 0, 1, kAlienWaysFloor);
	int const wall = g->AddSheet(w, h / 2, 0, 0, h * kNpcGridCell, 1, 0, 0, 0, -1, 0, kAlienWaysWall);

	// Walls across the floor, each with a gap (or none, if closed: then the far side can't be reached on the floor)
	for (int k = 0; k < walls; k++)
	{
		int const j = (k + 1) * h / (walls + 1);
		int const gap = static_cast<int>(Random(static_cast<uint32_t>(w)));

		for (int i = 0; i < w; i++)
		{
			if (closed || abs(i - gap) > 1)
			{
				g->Block(floor, i, j);
			}
		}
	}

	// The floor's far row to the wall's first row: an alien climbs up it
	for (int i = 0; i < w; i++)
	{
		g->Link(g->CellAt(floor, i, h - 1), g->CellAt(wall, i, 0), i % 3 == 0 ? kNpcGridJoinOutside : kNpcGridJoinInside);
	}
}

static void TestTheRules()
{
	printf("The rules, on one small floor\n");

	TestGrid g;
	int const s = g.AddSheet(20, 20, 0, 0, 0, 1, 0, 0, 0, 0, 1, kAlienWaysFloor);
	Tiles tiles;
	double exact = 0.0;
	size_t looked = 0;

	// Straight along a row: 19 cells of 300 mm, a floor at price 10
	Check(tiles.ExactSearch(g, g.CellAt(s, 0, 0), g.CellAt(s, 19, 0), kNpcGridAlien, 10, &exact, &looked) == kNpcGridFound && fabs(exact - 19 * 300 * 10.0) < 1e-6,
		"an alien's way along a floor costs its cells times the floor's price");
	Check(tiles.ExactSearch(g, g.CellAt(s, 0, 0), g.CellAt(s, 19, 0), kNpcGridMarine, 10, &exact, &looked) == kNpcGridFound && fabs(exact - 19 * 300.0) < 1e-6,
		"and a marine's at 1 a millimetre, whatever the floor's price");

	// A corner can't be cut: with the two cells beside a diagonal blocked, the diagonal isn't a move
	g.Block(s, 1, 0);
	g.Block(s, 0, 1);
	Check(tiles.ExactSearch(g, g.CellAt(s, 0, 0), g.CellAt(s, 1, 1), kNpcGridAlien, 1, &exact, &looked) == kNpcGridNoWay, "no diagonal past two blocked cells");

	// The tiles say the same
	Check(tiles.Start(g, g.CellAt(s, 0, 0), g.CellAt(s, 1, 1), kNpcGridAlien, 1) == nullptr && tiles.Work(1000000) == kNpcGridNoWay,
		"and the tiles find no way there either");

	// What it's given from outside
	Check(tiles.Start(g, static_cast<uint32_t>(g.Cells()), g.CellAt(s, 5, 5), kNpcGridAlien, 10) != nullptr && tiles.Work(10) == kNpcGridNoWay,
		"a start off the grid is refused");
	Check(tiles.Start(g, g.CellAt(s, 1, 0), g.CellAt(s, 5, 5), kNpcGridAlien, 10) != nullptr, "and a start the body can't be on");
	Check(tiles.Start(g, g.CellAt(s, 5, 5), g.CellAt(s, 6, 6), kNpcGridAlien, 0) != nullptr && tiles.Start(g, g.CellAt(s, 5, 5), g.CellAt(s, 6, 6), kNpcGridAlien, kAlienWaysMostFloorPrice + 1) != nullptr,
		"and a price out of its range");
	g.ready = false;
	Check(tiles.Start(g, g.CellAt(s, 5, 5), g.CellAt(s, 6, 6), kNpcGridAlien, 10) != nullptr, "and a grid that isn't ready");
	g.ready = true;
}

static void TestAgainstTheExact()
{
	printf("Against the exact search, on made-up worlds\n");

	int pairs = 0, foundBoth = 0, noWayBoth = 0, lost = 0, madeUp = 0, notReal = 0, wrongCost = 0, tooDear = 0;
	double dearest = 1.0;

	for (int world = 0; world < 12; world++)
	{
		TestGrid g;
		int const w = 16 + static_cast<int>(Random(40)), h = 16 + static_cast<int>(Random(40));
		MakeWorld(&g, w, h, 1 + static_cast<int>(Random(4)), world % 4 == 3);

		// Some cells blocked at random, as furniture
		for (int k = 0; k < w * h / 12; k++)
		{
			g.Block(0, static_cast<int>(Random(static_cast<uint32_t>(w))), static_cast<int>(Random(static_cast<uint32_t>(h))));
		}

		Tiles tiles;

		for (int p = 0; p < 12; p++)
		{
			uint32_t const from = g.CellAt(0, static_cast<int>(Random(static_cast<uint32_t>(w))), static_cast<int>(Random(static_cast<uint32_t>(h / 4 + 1))));
			uint32_t const to = Random(2) ? g.CellAt(1, static_cast<int>(Random(static_cast<uint32_t>(w))), static_cast<int>(Random(static_cast<uint32_t>(h / 2))))
				: g.CellAt(0, static_cast<int>(Random(static_cast<uint32_t>(w))), h - 1 - static_cast<int>(Random(static_cast<uint32_t>(h / 4 + 1))));

			if (!(g.Flags(from) & kNpcGridClings) || !(g.Flags(to) & kNpcGridClings))
			{
				continue;
			}

			pairs++;
			Tiles exactOne;
			double exact = 0.0;
			size_t looked = 0;
			NpcGridSearch const truth = exactOne.ExactSearch(g, from, to, kNpcGridAlien, 10, &exact, &looked);
			NpcGridSearch got = kNpcGridNoWay;

			if (tiles.Start(g, from, to, kNpcGridAlien, 10) == nullptr)
			{
				got = tiles.Work(100000000);
			}

			if (truth == kNpcGridFound && got == kNpcGridFound)
			{
				foundBoth++;
				std::vector<uint32_t> way;
				double cost = 0.0;

				if (!tiles.WayTo(to, &way) || !IsARealWay(g, way, from, to, kNpcGridAlien, 10, &cost))
				{
					notReal++;
				}
				else
				{
					if (fabs(cost - tiles.CostOfTheWay()) > 0.01 * tiles.CostOfTheWay() + 1.0)
					{
						wrongCost++;
					}

					double const dearer = exact > 0.0 ? tiles.CostOfTheWay() / exact : 1.0;
					dearest = std::max(dearest, dearer);

					if (dearer > kMostDearer || tiles.CostOfTheWay() < exact - 1.0)
					{
						tooDear++;
					}
				}
			}
			else if (truth == kNpcGridNoWay && got == kNpcGridNoWay)
			{
				noWayBoth++;
			}
			else if (truth == kNpcGridFound)
			{
				lost++;
			}
			else
			{
				madeUp++;
			}
		}
	}

	char line[200];
	snprintf(line, sizeof(line), "on %d pairs over 12 worlds, both found %d and both found none %d: no way lost (%d) and none made up (%d)", pairs, foundBoth, noWayBoth, lost, madeUp);
	Check(lost == 0 && madeUp == 0 && foundBoth > 20 && noWayBoth > 3, line);
	snprintf(line, sizeof(line), "every way found is a real one, move by move (%d not), and costs what it says (%d off)", notReal, wrongCost);
	Check(notReal == 0 && wrongCost == 0, line);
	snprintf(line, sizeof(line), "never cheaper than the exact way, and at most %.2f times as dear (the dearest: %.3f)", kMostDearer, dearest);
	Check(tooDear == 0, line);
}

static void TestOverFrames()
{
	printf("Spread over frames, the best so far, and the tiles kept\n");

	TestGrid g;
	MakeWorld(&g, 60, 60, 4, false);
	uint32_t const from = g.CellAt(0, 2, 2), to = g.CellAt(1, 50, 20);
	Tiles once, spread;

	Check(once.Start(g, from, to, kNpcGridAlien, 10) == nullptr && once.Work(100000000) == kNpcGridFound, "all at once, a way");

	spread.Start(g, from, to, kNpcGridAlien, 10);
	NpcGridSearch state = kNpcGridSearching;
	int frames = 0;
	bool bestIsReal = true, bestGetsNearer = true;
	double bestFar = 1e30;

	while (state == kNpcGridSearching && frames < 100000)
	{
		state = spread.Work(64);
		frames++;
		std::vector<uint32_t> partial;
		double cost = 0.0, at[3], goal[3];

		if (state == kNpcGridSearching)
		{
			uint32_t const best = spread.BestSoFar();

			if (!spread.WayTo(best, &partial) || !IsARealWay(g, partial, from, best, kNpcGridAlien, 10, &cost))
			{
				bestIsReal = false;
			}

			g.Middle(best, at);
			g.Middle(to, goal);
			double const far = sqrt((at[0] - goal[0]) * (at[0] - goal[0]) + (at[1] - goal[1]) * (at[1] - goal[1]) + (at[2] - goal[2]) * (at[2] - goal[2]));

			if (far > bestFar + 1e-6)
			{
				bestGetsNearer = false;
			}

			bestFar = std::min(bestFar, far);
		}
	}

	Check(state == kNpcGridFound && fabs(spread.CostOfTheWay() - once.CostOfTheWay()) < 1e-6 && frames > 3,
		"over many small frames, the same way at the same cost");
	Check(bestIsReal, "the best so far is a real way from the start, every frame");
	Check(bestGetsNearer, "and it only ever gets nearer the goal");
	Check(spread.Work(64) == kNpcGridFound && spread.Work(64) == kNpcGridFound, "asked again after it's ended, the same answer");

	// A second search on the same grid keeps the tiles of the first, and looks at fewer cells for the same way
	size_t const madeBefore = spread.TilesMade(), looksBefore = spread.Looks();
	spread.Start(g, from, to, kNpcGridAlien, 10);
	spread.Work(100000000);
	Check(spread.TilesMade() == madeBefore && spread.Looks() < looksBefore && fabs(spread.CostOfTheWay() - once.CostOfTheWay()) < 1e-6,
		"the same search again: no new tile, fewer cells looked at, the same way");

	// Another price is another search: its tiles are made again
	spread.Start(g, from, to, kNpcGridAlien, 1);
	spread.Work(100000000);
	Check(spread.TilesMade() > 0 && spread.TilesMade() <= madeBefore + 1000 && spread.CostOfTheWay() < once.CostOfTheWay(),
		"a cheaper floor: the tiles made again, and a cheaper way");
}

static void TestTheMarine()
{
	printf("A marine: steps and falls\n");

	TestGrid g;
	int const top = g.AddSheet(10, 10, 0, -3000, 0, 1, 0, 0, 0, 0, 1, kAlienWaysFloor);
	int const bottom = g.AddSheet(10, 10, 0, 0, 0, 1, 0, 0, 0, 0, 1, kAlienWaysFloor);
	g.Fall(g.CellAt(top, 9, 9), g.CellAt(bottom, 9, 9));
	Tiles tiles;

	Check(tiles.Start(g, g.CellAt(top, 0, 0), g.CellAt(bottom, 0, 0), kNpcGridMarine, 10) == nullptr && tiles.Work(1000000) == kNpcGridFound,
		"a marine falls from the top floor to the bottom one");
	Check(tiles.Start(g, g.CellAt(bottom, 0, 0), g.CellAt(top, 0, 0), kNpcGridMarine, 10) == nullptr && tiles.Work(1000000) == kNpcGridNoWay,
		"and can't fall back up");
	Check(tiles.Start(g, g.CellAt(top, 0, 0), g.CellAt(bottom, 0, 0), kNpcGridAlien, 10) == nullptr && tiles.Work(1000000) == kNpcGridNoWay,
		"and an alien never falls");
}

int main()
{
	TestTheRules();
	TestAgainstTheExact();
	TestOverFrames();
	TestTheMarine();

	if (checksFailed > 0)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
