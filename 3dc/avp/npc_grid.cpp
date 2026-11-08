// A grid on every surface of a level, for every kind of NPC: issue #145, step 1. What it is and why is in npc_grid.h.
// Written 2026-11-08 by Melanikus for the AvP port.
//
// The build is the node copy's (AvP-docs, engine/pathfinding-study/npc-grid.js), step for step, in the game's numbers (y goes down), so the
// numbers measured there on the 63 stock levels are this code's.
#include "npc_grid.h"

#include <math.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

NpcGridLimits NpcGrid_UsualLimits(bool advancedAI)
{
	NpcGridLimits limits;
	limits.mostSheets = 200000;
	limits.mostCellsInASheet = 16000000;
	limits.mostCells = 64000000;
	limits.mostBoxEntries = 8000000;
	limits.mostRayTests = 100000000;
	limits.mostSquareTests = 100000000;
	limits.mostLinks = 20000000;
	limits.mostFalls = 200000;
	limits.mostBytesKept = (advancedAI ? 250u : 64u) * 1024u * 1024u;
	limits.mostBytesBuilding = 128u * 1024u * 1024u;
	return limits;
}

// Empties a list and gives its memory back. clear() alone would keep the memory.
template <typename T> static void GiveBack(std::vector<T> *list)
{
	std::vector<T>().swap(*list);
}

static double Dot(const double a[3], const double b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void Cross(const double a[3], const double b[3], double out[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

// Makes a vector a unit long. One of no length is left as it is, all 0.
static void MakeUnit(double a[3])
{
	double const size = sqrt(Dot(a, a));

	if (size > 0.0)
	{
		a[0] /= size;
		a[1] /= size;
		a[2] /= size;
	}
}

// What the build works out for each polygon of the map, and lets go of when it's done
struct GridPolygon
{
	int given;             // its place in the list handed to Build
	int number;            // its number in the alien ways map
	double n[3];           // which way it faces, out of the surface, a unit long
	double d;              // its plane, along n
	double middle[3];      // the middle of its corners
	double area;           // in square millimetres
	double low[3], high[3];   // its box
	int sheet;             // -1 if it's on no sheet: a door's polygon that isn't a floor
	bool blocks;           // whether it's in the way of a ray or a marine's square: everything but a door's
};

// What's left of a polygon on one side of a straight cut (Sutherland and Hodgman's clipping): the corners on the kept side, and a new corner
// wherever an edge crosses the cut. Each corner is size numbers long (2 on a sheet, 3 in the world), and a polygon cut by a plane through
// a box has at most its own corners and one more for each cut, so out has room for kAlienWaysMostCorners + 6 corners.
int const kMostCutCorners = kAlienWaysMostCorners + 6;

static int CutPolygon(const double *in, int corners, int size, int axis, double at, bool keepAbove, double *out)
{
	int kept = 0;

	for (int a = 0; a < corners; a++)
	{
		const double *p = in + a * size;
		const double *q = in + ((a + 1) % corners) * size;
		bool const pIn = keepAbove ? p[axis] >= at : p[axis] <= at;
		bool const qIn = keepAbove ? q[axis] >= at : q[axis] <= at;

		if (pIn && kept < kMostCutCorners)
		{
			memcpy(out + kept * size, p, sizeof(double) * size);
			kept++;
		}

		if (pIn != qIn && kept < kMostCutCorners)
		{
			double const f = (at - p[axis]) / (q[axis] - p[axis]);

			for (int k = 0; k < size; k++)
			{
				out[kept * size + k] = p[k] + (q[k] - p[k]) * f;
			}

			kept++;
		}
	}

	return kept;
}

// Whether a point is inside a polygon, seen along one axis: the usual count of edges crossed by a line from the point
static bool Inside(const double *corners, int count, int size, int a, int b, double pa, double pb)
{
	bool inside = false;

	for (int x = 0, y = count - 1; x < count; y = x++)
	{
		const double *p = corners + x * size;
		const double *q = corners + y * size;

		if ((p[b] > pb) != (q[b] > pb) && pa < (q[a] - p[a]) * (pb - p[b]) / (q[b] - p[b]) + p[a])
		{
			inside = !inside;
		}
	}

	return inside;
}

// The number of the metre box a coordinate is in. Every corner of the map is within kAlienWaysMostCoordinate of the middle, so a box number
// is within plus or minus 64,001, and 17 bits hold it once kBoxOffset is added.
int const kBoxOffset = 65536;

// Millimetres either side of a polygon's plane, in the boxes it's listed in: for a polygon whose corners aren't quite in one plane
double const kBoxSlack = 2.0;

static int BoxOf(double at)
{
	return static_cast<int>(floor(at / kNpcGridBox));
}

static uint64_t BoxKey(int x, int y, int z)
{
	return (static_cast<uint64_t>(x + kBoxOffset) << 34) | (static_cast<uint64_t>(y + kBoxOffset) << 17) | static_cast<uint64_t>(z + kBoxOffset);
}

// The same for a column a marine's place is in, seen from above: x and z in cells
static uint64_t ColumnKey(double x, double z)
{
	int64_t const cx = static_cast<int64_t>(floor(x / kNpcGridCell)) + (1 << 20);
	int64_t const cz = static_cast<int64_t>(floor(z / kNpcGridCell)) + (1 << 20);
	return (static_cast<uint64_t>(cx) << 32) | static_cast<uint64_t>(cz);
}

struct BoxEntry
{
	uint64_t key;
	uint32_t polygon;
	bool operator<(const BoxEntry &other) const { return key < other.key || (key == other.key && polygon < other.polygon); }
};

struct Pair
{
	uint32_t from, to;
	bool operator<(const Pair &other) const { return from < other.from || (from == other.from && to < other.to); }
	bool operator==(const Pair &other) const { return from == other.from && to == other.to; }
};

NpcGrid::NpcGrid()
	: searchFrom(kNpcGridNoCell), searchTo(kNpcGridNoCell), searchBody(kNpcGridAlien), searchPrice(1), searchLooks(0), searchMostLooks(0),
	searchState(kNpcGridNoWay), searchClaims(nullptr), searchSearcher(-1), searchClaimStep(kAlienWaysClaimPriceStep),
	  searchClaimMost(kAlienWaysClaimMostPriceFactor), searchShut(nullptr), cutShort(false),
	  surfaceCells(0), steps(0), rayTests(0), squareTests(0), boxEntries(0), bytesKept(0), bytesBuilding(0), bytesBuildingUsed(0), limits(NpcGrid_UsualLimits(false)),
	  ready(false), stopAsked(false), looksAtStop(0), stopAtLook(0)
{
	searchStart.x = searchStart.y = searchStart.z = 0;
	searchGoal = searchStart;
}

void NpcGrid::Clear()
{
	ready = false;
	GiveBack(&sheets);
	GiveBack(&flags);
	GiveBack(&room);
	GiveBack(&polygonOf);
	std::unordered_map<uint32_t, Seen>().swap(seen);
	GiveBack(&open);
	GiveBack(&wayCells);
	GiveBack(&way);
	GiveBack(&wayPointCells);
	GiveBack(&wayPolygons);
	searchState = kNpcGridNoWay;
	searchLooks = 0;
	searchClaims = nullptr;
	searchShut = nullptr;
	cutShort = false;
	GiveBack(&linkFrom);
	GiveBack(&linkTo);
	GiveBack(&fallFrom);
	GiveBack(&fallTo);
	surfaceCells = steps = rayTests = squareTests = boxEntries = 0;
	bytesKept = bytesBuilding = bytesBuildingUsed = 0;
	stopAsked.store(false);
	looksAtStop = 0;
}

// Every loop of the build asks this. It also counts the asking, so a test can make the build stop at a chosen point.
bool NpcGrid::StopNow()
{
	looksAtStop++;

	if (stopAtLook != 0 && looksAtStop >= stopAtLook)
	{
		stopAsked.store(true);
	}

	return stopAsked.load();
}

// Gives up: keeps nothing, and gives back why
const char *NpcGrid::Fail(const char *why)
{
	bool const stop = stopAsked.load();
	size_t const looks = looksAtStop;
	Clear();
	stopAsked.store(stop);
	looksAtStop = looks;
	return why;
}

// Adds bytes to what the build holds while it's building, unless that would take it past the ceiling. Says whether it did.
bool NpcGrid::CountBuilding(size_t bytes)
{
	if (bytesBuilding > limits.mostBytesBuilding || bytes > limits.mostBytesBuilding - bytesBuilding)
	{
		return false;
	}

	bytesBuilding += bytes;
	bytesBuildingUsed = std::max(bytesBuildingUsed, bytesBuilding);
	return true;
}

int NpcGrid::SheetOf(uint32_t cell) const
{
	if (cell >= flags.size() || sheets.empty())
	{
		return -1;
	}

	// The last sheet whose first cell is at or before this one
	size_t low = 0, high = sheets.size() - 1;

	while (low < high)
	{
		size_t const mid = (low + high + 1) / 2;

		if (sheets[mid].first <= cell)
		{
			low = mid;
		}
		else
		{
			high = mid - 1;
		}
	}

	return static_cast<int>(low);
}

AlienWaysKind NpcGrid::KindOf(uint32_t cell) const
{
	int const sheet = SheetOf(cell);
	return sheet < 0 ? kAlienWaysWall : static_cast<AlienWaysKind>(sheets[sheet].kind);
}

bool NpcGrid::Middle(uint32_t cell, double out[3]) const
{
	int const number = SheetOf(cell);

	if (number < 0)
	{
		return false;
	}

	Sheet const &s = sheets[number];
	uint32_t const k = cell - s.first;
	double const a = (static_cast<double>(k % s.w) + s.i0 + 0.5) * kNpcGridCell;
	double const b = (static_cast<double>(k / s.w) + s.j0 + 0.5) * kNpcGridCell;

	for (int axis = 0; axis < 3; axis++)
	{
		out[axis] = s.n[axis] * s.d + s.u[axis] * a + s.v[axis] * b;
	}

	return true;
}

bool NpcGrid::CellPlace(uint32_t cell, int *sheet, int *i, int *j) const
{
	int const number = SheetOf(cell);

	if (number < 0)
	{
		return false;
	}

	Sheet const &s = sheets[number];
	*sheet = number;
	*i = static_cast<int>((cell - s.first) % static_cast<uint32_t>(s.w));
	*j = static_cast<int>((cell - s.first) / static_cast<uint32_t>(s.w));
	return true;
}

uint32_t NpcGrid::CellAt(int sheet, int i, int j) const
{
	if (sheet < 0 || sheet >= static_cast<int>(sheets.size()))
	{
		return kNpcGridNoCell;
	}

	Sheet const &s = sheets[sheet];

	if (i < 0 || j < 0 || i >= s.w || j >= s.h)
	{
		return kNpcGridNoCell;
	}

	return s.first + static_cast<uint32_t>(j) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(i);
}

uint32_t NpcGrid::CellOver(int sheet, double x, double y, double z) const
{
	if (sheet < 0 || sheet >= static_cast<int>(sheets.size()))
	{
		return kNpcGridNoCell;
	}

	Sheet const &s = sheets[sheet];
	double const r[3] = { x - s.n[0] * s.d, y - s.n[1] * s.d, z - s.n[2] * s.d };
	double const i = floor(Dot(r, s.u) / kNpcGridCell) - s.i0;
	double const j = floor(Dot(r, s.v) / kNpcGridCell) - s.j0;

	if (!(i >= 0.0 && j >= 0.0 && i < s.w && j < s.h))
	{
		return kNpcGridNoCell;
	}

	return s.first + static_cast<uint32_t>(j) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(i);
}

uint32_t NpcGrid::NearestCell(double x, double y, double z, NpcGridBody body, int within) const
{
	uint8_t const bit = body == kNpcGridMarine ? kNpcGridStands : kNpcGridClings;
	uint32_t best = kNpcGridNoCell;
	double bestFar = static_cast<double>(within);
	double const point[3] = { x, y, z };

	for (size_t number = 0; number < sheets.size(); number++)
	{
		Sheet const &s = sheets[number];

		if (fabs(Dot(point, s.n) - s.d) > bestFar)
		{
			continue;
		}

		uint32_t const over = CellOver(static_cast<int>(number), x, y, z);

		if (over == kNpcGridNoCell)
		{
			continue;
		}

		// The cell under the point and the ones round it: a point near an edge can be nearer the next cell's middle
		int const i = static_cast<int>((over - s.first) % s.w), j = static_cast<int>((over - s.first) / s.w);

		for (int dj = -1; dj <= 1; dj++)
		{
			for (int di = -1; di <= 1; di++)
			{
				int const I = i + di, J = j + dj;

				if (I < 0 || J < 0 || I >= s.w || J >= s.h)
				{
					continue;
				}

				uint32_t const c = s.first + static_cast<uint32_t>(J) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(I);
				double m[3];

				if (!(flags[c] & bit) || !Middle(c, m))
				{
					continue;
				}

				double const far = sqrt((m[0] - x) * (m[0] - x) + (m[1] - y) * (m[1] - y) + (m[2] - z) * (m[2] - z));

				if (far <= bestFar)
				{
					bestFar = far;
					best = c;
				}
			}
		}
	}

	return best;
}

// The links (or falls) from one cell: the run of it in a list sorted by the cell they're from
static const uint32_t *RunOf(const std::vector<uint32_t> &from, const std::vector<uint32_t> &to, uint32_t cell, size_t *count)
{
	std::vector<uint32_t>::const_iterator const first = std::lower_bound(from.begin(), from.end(), cell);
	std::vector<uint32_t>::const_iterator const end = std::upper_bound(first, from.end(), cell);
	*count = static_cast<size_t>(end - first);
	return *count ? &to[static_cast<size_t>(first - from.begin())] : nullptr;
}

const uint32_t *NpcGrid::LinksOf(uint32_t cell, size_t *count) const
{
	return RunOf(linkFrom, linkTo, cell, count);
}

const uint32_t *NpcGrid::FallsOf(uint32_t cell, size_t *count) const
{
	return RunOf(fallFrom, fallTo, cell, count);
}

const char *NpcGrid::Build(const std::vector<AlienWaysPolygon> &polygons, const AlienWaysMap &map)
{
	bool const stop = stopAsked.load();
	size_t const looks = looksAtStop;
	Clear();
	stopAsked.store(stop);
	looksAtStop = looks;

	if (!map.Ready())
	{
		return Fail("the alien ways map isn't ready");
	}

	if (StopNow())
	{
		return Fail("asked to stop");
	}

	// 1. THE POLYGONS: the map's, each with its facing, plane and box. Newell's sums over every corner give the facing, as the map's own
	// AlienWays_HowMuchItFacesUp does, and its "up" is minus the facing's y.
	std::vector<GridPolygon> grid;
	std::vector<int> gridOfMap(map.Polygons(), -1);

	if (!CountBuilding(map.Polygons() * (sizeof(GridPolygon) + sizeof(int))))
	{
		return Fail("too much memory while building, for the polygons");
	}

	grid.reserve(map.Polygons());

	for (size_t given = 0; given < polygons.size(); given++)
	{
		int const number = map.MapNumberOf(given);

		if (number < 0)
		{
			continue;
		}

		AlienWaysPolygon const &p = polygons[given];
		GridPolygon g;
		double newell[3] = { 0.0, 0.0, 0.0 };
		g.middle[0] = g.middle[1] = g.middle[2] = 0.0;
		g.low[0] = g.low[1] = g.low[2] = 1e300;
		g.high[0] = g.high[1] = g.high[2] = -1e300;

		for (int i = 0; i < p.corners; i++)
		{
			int const j = (i + 1) % p.corners;
			newell[0] += (static_cast<double>(p.y[i]) - p.y[j]) * (static_cast<double>(p.z[i]) + p.z[j]);
			newell[1] += (static_cast<double>(p.z[i]) - p.z[j]) * (static_cast<double>(p.x[i]) + p.x[j]);
			newell[2] += (static_cast<double>(p.x[i]) - p.x[j]) * (static_cast<double>(p.y[i]) + p.y[j]);
			double const corner[3] = { static_cast<double>(p.x[i]), static_cast<double>(p.y[i]), static_cast<double>(p.z[i]) };

			for (int axis = 0; axis < 3; axis++)
			{
				g.middle[axis] += corner[axis] / p.corners;
				g.low[axis] = std::min(g.low[axis], corner[axis]);
				g.high[axis] = std::max(g.high[axis], corner[axis]);
			}
		}

		g.given = static_cast<int>(given);
		g.number = number;
		g.area = sqrt(Dot(newell, newell)) / 2.0;
		memcpy(g.n, newell, sizeof(g.n));
		MakeUnit(g.n);
		g.d = Dot(g.n, g.middle);
		// A door opens for whoever comes: none of it is in anyone's way, and only its floor gets cells
		g.blocks = p.door == 0;
		g.sheet = (p.door == 0 || map.KindOf(number) == kAlienWaysFloor) ? 0 : -1;

		// A polygon with its corners in a line has no plane and no surface: it's in no one's way, and gets no cells
		if (g.area < 1.0)
		{
			g.blocks = false;
			g.sheet = -1;
		}
		gridOfMap[number] = static_cast<int>(grid.size());
		grid.push_back(g);
	}

	// 2. SHEETS: joined polygons that face the same way and lie in one plane, joined up with a union-find
	std::vector<int> parent(grid.size());

	if (!CountBuilding(grid.size() * sizeof(int)))
	{
		return Fail("too much memory while building, for the sheets");
	}

	for (size_t i = 0; i < grid.size(); i++)
	{
		parent[i] = static_cast<int>(i);
	}

	struct Find
	{
		static int Root(std::vector<int> &parent, int i)
		{
			while (parent[i] != i)
			{
				parent[i] = parent[parent[i]];
				i = parent[i];
			}

			return i;
		}
	};

	std::vector<int> const &firstJoin = map.FirstJoins();
	std::vector<int> const &joinOther = map.JoinOthers();

	for (size_t a = 0; a < map.Polygons(); a++)
	{
		if (StopNow())
		{
			return Fail("asked to stop");
		}

		for (int k = firstJoin[a]; k < firstJoin[a + 1]; k++)
		{
			int const b = joinOther[k];

			if (b <= static_cast<int>(a) || gridOfMap[a] < 0 || gridOfMap[b] < 0)
			{
				continue;
			}

			GridPolygon const &P = grid[gridOfMap[a]], &Q = grid[gridOfMap[b]];

			if (P.sheet < 0 || Q.sheet < 0 || Dot(P.n, Q.n) < kNpcGridSameFacing)
			{
				continue;
			}

			if (fabs(Dot(P.n, Q.middle) - P.d) > kNpcGridSamePlane || fabs(Dot(Q.n, P.middle) - Q.d) > kNpcGridSamePlane)
			{
				continue;
			}

			parent[Find::Root(parent, gridOfMap[a])] = Find::Root(parent, gridOfMap[b]);
		}
	}

	// Each sheet's polygons, in the order they came
	std::vector<int> sheetOfRoot(grid.size(), -1);
	std::vector<std::vector<int> > members;

	for (size_t i = 0; i < grid.size(); i++)
	{
		if (grid[i].sheet < 0)
		{
			continue;
		}

		int const root = Find::Root(parent, static_cast<int>(i));

		if (sheetOfRoot[root] < 0)
		{
			if (members.size() >= limits.mostSheets)
			{
				return Fail("too many sheets");
			}

			sheetOfRoot[root] = static_cast<int>(members.size());
			members.push_back(std::vector<int>());
		}

		grid[i].sheet = sheetOfRoot[root];
		members[sheetOfRoot[root]].push_back(static_cast<int>(i));
	}

	GiveBack(&parent);
	GiveBack(&sheetOfRoot);

	// 3. EACH SHEET'S PLANE, ITS TWO AXES AND ITS GRID. The facing is its polygons' facings, each as much as its area. The first axis lies
	// along the sheet: east-west where it can on a floor or a ceiling, and level along a wall. The second is square to it, along the sheet.
	// (These are the node copy's axes, turned into the game's numbers, so a cell here is the same cell there.)
	std::vector<double> flat;      // every sheet polygon's corners, laid flat on its sheet: two numbers each
	std::vector<size_t> firstFlat(grid.size() + 1, 0);
	size_t cornersInAll = 0;

	for (size_t i = 0; i < grid.size(); i++)
	{
		firstFlat[i] = cornersInAll;
		cornersInAll += grid[i].sheet >= 0 ? static_cast<size_t>(polygons[grid[i].given].corners) : 0;
	}

	firstFlat[grid.size()] = cornersInAll;

	if (!CountBuilding(cornersInAll * 2 * sizeof(double) + grid.size() * sizeof(size_t)))
	{
		return Fail("too much memory while building, for the sheets' corners");
	}

	flat.resize(cornersInAll * 2);
	sheets.resize(members.size());
	size_t cells = 0;

	for (size_t number = 0; number < members.size(); number++)
	{
		if (StopNow())
		{
			return Fail("asked to stop");
		}

		Sheet &s = sheets[number];
		double n[3] = { 0.0, 0.0, 0.0 }, area = 0.0, d = 0.0;

		for (size_t k = 0; k < members[number].size(); k++)
		{
			GridPolygon const &g = grid[members[number][k]];

			for (int axis = 0; axis < 3; axis++)
			{
				n[axis] += g.n[axis] * g.area;
			}

			area += g.area;
		}

		MakeUnit(n);

		for (size_t k = 0; k < members[number].size(); k++)
		{
			GridPolygon const &g = grid[members[number][k]];
			d += Dot(n, g.middle) * g.area;
		}

		d = area > 0.0 ? d / area : 0.0;
		memcpy(s.n, n, sizeof(s.n));
		s.d = d;

		if (fabs(n[1]) >= 0.5)
		{
			double const u[3] = { 1.0 - n[0] * n[0], -n[0] * n[1], -n[0] * n[2] };
			memcpy(s.u, u, sizeof(s.u));
		}
		else
		{
			double const upwards[3] = { 0.0, -1.0, 0.0 };
			Cross(n, upwards, s.u);
		}

		MakeUnit(s.u);
		Cross(s.u, s.n, s.v);
		float const up = static_cast<float>(-n[1]);
		s.kind = static_cast<uint8_t>(AlienWays_KindOf(up));

		// Every corner laid flat on the sheet, and the grid's span
		double lowA = 1e300, lowB = 1e300, highA = -1e300, highB = -1e300;

		for (size_t k = 0; k < members[number].size(); k++)
		{
			int const g = members[number][k];
			AlienWaysPolygon const &p = polygons[grid[g].given];

			for (int c = 0; c < p.corners; c++)
			{
				double const r[3] = { p.x[c] - n[0] * d, p.y[c] - n[1] * d, p.z[c] - n[2] * d };
				double const a = Dot(r, s.u), b = Dot(r, s.v);
				flat[(firstFlat[g] + c) * 2] = a;
				flat[(firstFlat[g] + c) * 2 + 1] = b;
				lowA = std::min(lowA, a);
				lowB = std::min(lowB, b);
				highA = std::max(highA, a);
				highB = std::max(highB, b);
			}
		}

		s.i0 = static_cast<int>(floor(lowA / kNpcGridCell));
		s.j0 = static_cast<int>(floor(lowB / kNpcGridCell));
		double const w = floor(highA / kNpcGridCell) - s.i0 + 1.0, h = floor(highB / kNpcGridCell) - s.j0 + 1.0;

		// A sheet's grid too big to hold is a level that's broken, or made to break this: no stock sheet comes near (Tao 3)
		if (w * h > static_cast<double>(limits.mostCellsInASheet))
		{
			return Fail("a sheet has too many cells");
		}

		s.w = static_cast<int>(w);
		s.h = static_cast<int>(h);
		s.first = static_cast<uint32_t>(cells);

		if (static_cast<size_t>(s.w) * static_cast<size_t>(s.h) > limits.mostCells - cells)
		{
			return Fail("too many cells");
		}

		cells += static_cast<size_t>(s.w) * static_cast<size_t>(s.h);
	}

	// What's kept: six bytes a cell (its flags, its room and its polygon), and the sheets. Links and falls are counted when they're known.
	bytesKept = cells * (2 + sizeof(int32_t)) + sheets.size() * sizeof(Sheet);

	if (bytesKept > limits.mostBytesKept)
	{
		return Fail("the grid would keep too much memory, for its cells");
	}

	flags.assign(cells, 0);
	room.assign(cells, 0);
	polygonOf.assign(cells, -1);

	// 4. WHICH CELLS ARE ON THE SURFACE
	for (size_t number = 0; number < members.size(); number++)
	{
		if (StopNow())
		{
			return Fail("asked to stop");
		}

		Sheet const &s = sheets[number];

		for (size_t k = 0; k < members[number].size(); k++)
		{
			int const g = members[number][k];
			int const corners = polygons[grid[g].given].corners;
			const double *f = &flat[firstFlat[g] * 2];
			double lowA = 1e300, lowB = 1e300, highA = -1e300, highB = -1e300;

			for (int c = 0; c < corners; c++)
			{
				lowA = std::min(lowA, f[c * 2]);
				highA = std::max(highA, f[c * 2]);
				lowB = std::min(lowB, f[c * 2 + 1]);
				highB = std::max(highB, f[c * 2 + 1]);
			}

			for (int j = static_cast<int>(floor(lowB / kNpcGridCell)); j <= static_cast<int>(floor(highB / kNpcGridCell)); j++)
			{
				for (int i = static_cast<int>(floor(lowA / kNpcGridCell)); i <= static_cast<int>(floor(highA / kNpcGridCell)); i++)
				{
					uint32_t const at = s.first + static_cast<uint32_t>(j - s.j0) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(i - s.i0);

					if (flags[at] & kNpcGridMiddleOn)
					{
						continue;
					}

					if (Inside(f, corners, 2, 0, 1, (i + 0.5) * kNpcGridCell, (j + 0.5) * kNpcGridCell))
					{
						flags[at] = static_cast<uint8_t>((flags[at] & ~kNpcGridCoveredOnly) | kNpcGridMiddleOn);
						polygonOf[at] = grid[g].number;
						continue;
					}

					// A floor's cell is on it too if the polygon covers enough of the cell: the polygon cut to the cell's square, and its area
					if (s.kind != kAlienWaysFloor || (flags[at] & kNpcGridCoveredOnly))
					{
						continue;
					}

					double one[kMostCutCorners * 2], two[kMostCutCorners * 2];
					int left = CutPolygon(f, corners, 2, 0, static_cast<double>(i) * kNpcGridCell, true, one);
					left = left ? CutPolygon(one, left, 2, 0, static_cast<double>(i + 1) * kNpcGridCell, false, two) : 0;
					left = left ? CutPolygon(two, left, 2, 1, static_cast<double>(j) * kNpcGridCell, true, one) : 0;
					left = left ? CutPolygon(one, left, 2, 1, static_cast<double>(j + 1) * kNpcGridCell, false, two) : 0;
					double twice = 0.0;

					for (int x = 0, y = left - 1; x < left; y = x++)
					{
						twice += two[y * 2] * two[x * 2 + 1] - two[x * 2] * two[y * 2 + 1];
					}

					if (fabs(twice) / 2.0 >= kNpcGridLeastCover)
					{
						flags[at] |= kNpcGridCoveredOnly;
						polygonOf[at] = grid[g].number;
					}
				}
			}
		}
	}

	for (size_t c = 0; c < cells; c++)
	{
		surfaceCells += (flags[c] & (kNpcGridMiddleOn | kNpcGridCoveredOnly)) ? 1 : 0;
	}

	// 5. THE METRE BOXES: every polygon in the way of something, listed in each box its plane passes through, sorted, for the rays and the
	// squares. Not every box of its box: a big sloped ground polygon (Genshd1's proplant) would fill millions. Seen along the axis the
	// polygon faces most, each column of boxes it crosses holds the plane between the lowest and highest it reaches over that column, kept
	// within the polygon's own box, with kBoxSlack either side for a polygon whose corners aren't quite in one plane. It's done twice: once
	// to count, so the list is checked against its limit and its memory before it's made, and once to fill it.
	std::vector<BoxEntry> boxes;
	size_t entries = 0, columns = 0;

	struct Listing
	{
		// The boxes one polygon goes in. With list given, they're added to it; either way, how many there are is given back.
		static size_t Boxes(const GridPolygon &g, uint32_t polygon, std::vector<BoxEntry> *list)
		{
			int const ax = (fabs(g.n[0]) >= fabs(g.n[1]) && fabs(g.n[0]) >= fabs(g.n[2])) ? 0 : fabs(g.n[1]) >= fabs(g.n[2]) ? 1 : 2;
			int const A = ax == 0 ? 1 : 0, B = ax == 2 ? 1 : 2;
			size_t count = 0;

			for (int a = BoxOf(g.low[A]); a <= BoxOf(g.high[A]); a++)
			{
				for (int b = BoxOf(g.low[B]); b <= BoxOf(g.high[B]); b++)
				{
					double const a0 = std::max(g.low[A], static_cast<double>(a) * kNpcGridBox), a1 = std::min(g.high[A], (a + 1.0) * kNpcGridBox);
					double const b0 = std::max(g.low[B], static_cast<double>(b) * kNpcGridBox), b1 = std::min(g.high[B], (b + 1.0) * kNpcGridBox);
					double const corners[4] = { Along(g, ax, A, B, a0, b0), Along(g, ax, A, B, a0, b1), Along(g, ax, A, B, a1, b0), Along(g, ax, A, B, a1, b1) };
					double const low = std::max(g.low[ax], *std::min_element(corners, corners + 4) - kBoxSlack);
					double const high = std::min(g.high[ax], *std::max_element(corners, corners + 4) + kBoxSlack);

					for (int c = BoxOf(low); c <= BoxOf(high); c++)
					{
						count++;

						if (list)
						{
							int at[3];
							at[ax] = c;
							at[A] = a;
							at[B] = b;
							BoxEntry const e = { BoxKey(at[0], at[1], at[2]), polygon };
							list->push_back(e);
						}
					}
				}
			}

			return count;
		}

		// Where the polygon's plane is along axis ax, at a and b along the other two
		static double Along(const GridPolygon &g, int ax, int A, int B, double a, double b)
		{
			return (g.d - g.n[A] * a - g.n[B] * b) / g.n[ax];
		}
	};

	for (size_t i = 0; i < grid.size(); i++)
	{
		if (!grid[i].blocks)
		{
			continue;
		}

		// The columns of boxes a polygon crosses are counted before they're walked, so a polygon made to cross millions costs nothing to
		// turn down. Each column holds at most 4 boxes of the plane (along the axis it faces most, the plane moves at most 2 m over a
		// column a metre square), so this bounds the boxes too.
		GridPolygon const &g = grid[i];
		int const ax = (fabs(g.n[0]) >= fabs(g.n[1]) && fabs(g.n[0]) >= fabs(g.n[2])) ? 0 : fabs(g.n[1]) >= fabs(g.n[2]) ? 1 : 2;
		int const A = ax == 0 ? 1 : 0, B = ax == 2 ? 1 : 2;
		double const across = (BoxOf(g.high[A]) - BoxOf(g.low[A]) + 1.0) * (BoxOf(g.high[B]) - BoxOf(g.low[B]) + 1.0);

		if (across > static_cast<double>(limits.mostBoxEntries - columns))
		{
			return Fail("too many columns of metre boxes to look through");
		}

		columns += static_cast<size_t>(across);
		size_t const count = Listing::Boxes(g, static_cast<uint32_t>(i), nullptr);

		if (count > limits.mostBoxEntries - entries)
		{
			return Fail("too many polygons in the metre boxes");
		}

		entries += count;
	}

	if (!CountBuilding(entries * sizeof(BoxEntry) + grid.size() * sizeof(uint32_t)))
	{
		return Fail("too much memory while building, for the metre boxes");
	}

	boxes.reserve(entries);
	boxEntries = entries;

	for (size_t i = 0; i < grid.size(); i++)
	{
		if (grid[i].blocks)
		{
			Listing::Boxes(grid[i], static_cast<uint32_t>(i), &boxes);
		}
	}

	std::sort(boxes.begin(), boxes.end());

	// Which polygons a ray, or a square, has tried already, by the number of the try
	std::vector<uint32_t> triedIn(grid.size(), 0);
	uint32_t tryNumber = 0;

	struct Boxes
	{
		static std::pair<size_t, size_t> In(const std::vector<BoxEntry> &boxes, uint64_t key)
		{
			BoxEntry const low = { key, 0 }, high = { key, 0xFFFFFFFFu };
			std::vector<BoxEntry>::const_iterator const a = std::lower_bound(boxes.begin(), boxes.end(), low);
			std::vector<BoxEntry>::const_iterator const b = std::upper_bound(a, boxes.end(), high);
			return std::make_pair(static_cast<size_t>(a - boxes.begin()), static_cast<size_t>(b - boxes.begin()));
		}
	};

	// 6. ROOM: from each cell whose middle is on the surface, straight out, how far to the nearest polygon. The boxes the ray passes through
	// are walked one at a time, in the order it passes them (Amanatides and Woo's walk), and each polygon is tried once a ray.
	for (size_t number = 0; number < sheets.size(); number++)
	{
		if (StopNow())
		{
			return Fail("asked to stop");
		}

		Sheet const &s = sheets[number];

		for (uint32_t c = s.first; c < s.first + static_cast<uint32_t>(s.w) * static_cast<uint32_t>(s.h); c++)
		{
			if (!(flags[c] & kNpcGridMiddleOn))
			{
				continue;
			}

			double o[3];
			Middle(c, o);
			tryNumber++;
			double best = kNpcGridMostRoom;
			int at[3], step[3];
			double next[3], each[3];

			for (int k = 0; k < 3; k++)
			{
				at[k] = BoxOf(o[k]);
				step[k] = s.n[k] > 0.0 ? 1 : s.n[k] < 0.0 ? -1 : 0;
				next[k] = s.n[k] != 0.0 ? ((s.n[k] > 0.0 ? (at[k] + 1.0) * kNpcGridBox : static_cast<double>(at[k]) * kNpcGridBox) - o[k]) / s.n[k] : 1e300;
				each[k] = s.n[k] != 0.0 ? kNpcGridBox / fabs(s.n[k]) : 1e300;
			}

			double entered = 0.0;

			while (entered <= best)
			{
				std::pair<size_t, size_t> const run = Boxes::In(boxes, BoxKey(at[0], at[1], at[2]));

				for (size_t e = run.first; e < run.second; e++)
				{
					uint32_t const i = boxes[e].polygon;

					if (triedIn[i] == tryNumber)
					{
						continue;
					}

					triedIn[i] = tryNumber;
					GridPolygon const &p = grid[i];
					double const towards = Dot(p.n, s.n);

					if (fabs(towards) < 1e-6)
					{
						continue;
					}

					double const t = (p.d - Dot(p.n, o)) / towards;

					if (t <= 5.0 || t >= best)
					{
						continue;
					}

					if (++rayTests > limits.mostRayTests)
					{
						return Fail("too many polygons tried by the rays");
					}

					// Inside the polygon, seen along its own most-facing axis
					double const hit[3] = { o[0] + s.n[0] * t, o[1] + s.n[1] * t, o[2] + s.n[2] * t };
					int const axis = (fabs(p.n[0]) >= fabs(p.n[1]) && fabs(p.n[0]) >= fabs(p.n[2])) ? 0 : fabs(p.n[1]) >= fabs(p.n[2]) ? 1 : 2;
					int const A = axis == 0 ? 1 : 0, B = axis == 2 ? 1 : 2;
					AlienWaysPolygon const &q = polygons[p.given];
					double corners[kAlienWaysMostCorners * 3];

					for (int k = 0; k < q.corners; k++)
					{
						corners[k * 3] = q.x[k];
						corners[k * 3 + 1] = q.y[k];
						corners[k * 3 + 2] = q.z[k];
					}

					if (Inside(corners, q.corners, 3, A, B, hit[A], hit[B]))
					{
						best = t;
					}
				}

				int const k = next[0] < next[1] ? (next[0] < next[2] ? 0 : 2) : (next[1] < next[2] ? 1 : 2);
				entered = next[k];
				at[k] += step[k];
				next[k] += each[k];

				if (entered > kNpcGridMostRoom)
				{
					break;
				}
			}

			room[c] = static_cast<uint8_t>(std::min(255.0, floor(best / kNpcGridRoomStep)));
		}
	}

	// 7. LINKS ALONG THE JOINS between sheets: along the stretch two polygons share, every half a cell, the nearest cell of each sheet whose
	// middle is on it, within kNpcGridStep of the point
	std::vector<Pair> links;

	struct Near
	{
		static uint32_t Cell(const NpcGrid &grid, const std::vector<uint8_t> &flags, int sheet, const double p[3])
		{
			uint32_t const over = grid.CellOver(sheet, p[0], p[1], p[2]);

			if (over == kNpcGridNoCell)
			{
				return kNpcGridNoCell;
			}

			uint32_t best = kNpcGridNoCell;
			double bestFar = kNpcGridStep;
			int const sheetW = grid.sheets[sheet].w, sheetH = grid.sheets[sheet].h;
			uint32_t const first = grid.sheets[sheet].first;
			int const i = static_cast<int>((over - first) % sheetW), j = static_cast<int>((over - first) / sheetW);

			for (int dj = -1; dj <= 1; dj++)
			{
				for (int di = -1; di <= 1; di++)
				{
					int const I = i + di, J = j + dj;

					if (I < 0 || J < 0 || I >= sheetW || J >= sheetH)
					{
						continue;
					}

					uint32_t const c = first + static_cast<uint32_t>(J) * static_cast<uint32_t>(sheetW) + static_cast<uint32_t>(I);
					double m[3];

					if (!(flags[c] & kNpcGridMiddleOn) || !grid.Middle(c, m))
					{
						continue;
					}

					double const far = sqrt((m[0] - p[0]) * (m[0] - p[0]) + (m[1] - p[1]) * (m[1] - p[1]) + (m[2] - p[2]) * (m[2] - p[2]));

					if (far < bestFar)
					{
						bestFar = far;
						best = c;
					}
				}
			}

			return best;
		}
	};

	// Adds a link both ways, unless it's the one just added: the points along one stretch often land on the same two cells
	struct Add
	{
		static bool Both(std::vector<Pair> *links, size_t most, uint32_t a, uint32_t b)
		{
			if (!links->empty() && ((links->back().from == b && links->back().to == a) || (links->back().from == a && links->back().to == b)))
			{
				return true;
			}

			if (links->size() + 2 > most)
			{
				return false;
			}

			Pair const ab = { a, b }, ba = { b, a };
			links->push_back(ab);
			links->push_back(ba);
			return true;
		}
	};

	for (size_t a = 0; a < map.Polygons(); a++)
	{
		if (StopNow())
		{
			return Fail("asked to stop");
		}

		for (int k = firstJoin[a]; k < firstJoin[a + 1]; k++)
		{
			int const b = joinOther[k];

			if (b <= static_cast<int>(a) || gridOfMap[a] < 0 || gridOfMap[b] < 0)
			{
				continue;
			}

			int const sa = grid[gridOfMap[a]].sheet, sb = grid[gridOfMap[b]].sheet;
			AlienWaysXYZ from, to;

			if (sa < 0 || sb < 0 || sa == sb || !map.Joined(static_cast<int>(a), b, &from, &to))
			{
				continue;
			}

			double const length = sqrt(static_cast<double>(to.x - from.x) * (to.x - from.x) + static_cast<double>(to.y - from.y) * (to.y - from.y) +
				static_cast<double>(to.z - from.z) * (to.z - from.z));
			int const count = std::max(1, static_cast<int>(ceil(length / (kNpcGridCell / 2))));

			for (int step = 0; step <= count; step++)
			{
				double const f = static_cast<double>(step) / count;
				double const p[3] = { from.x + (to.x - from.x) * f, from.y + (to.y - from.y) * f, from.z + (to.z - from.z) * f };
				uint32_t const ca = Near::Cell(*this, flags, sa, p), cb = Near::Cell(*this, flags, sb, p);

				if (ca != kNpcGridNoCell && cb != kNpcGridNoCell)
				{
					if (!CountBuilding(2 * sizeof(Pair)) || !Add::Both(&links, limits.mostLinks, ca, cb))
					{
						return Fail("too many links");
					}
				}
			}
		}
	}

	GiveBack(&members);
	GiveBack(&flat);
	GiveBack(&firstFlat);

	// 8. FITS. A marine stands where nothing that's in the way lies in his square, from a step above his feet up to his height. Each test's
	// box is a millimetre smaller than his all round, so a wall that only touches it, or a step exactly kNpcGridStep high, isn't in his way.
	// y goes down, so "above his feet" is smaller y.
	int const half = kNpcGridStandHalf - 1;

	struct Square
	{
		// Whether nothing in the way lies in a box: each polygon in the metre boxes it passes through, cut to the box
		static int Clear(NpcGrid &grid, const std::vector<BoxEntry> &boxes, const std::vector<GridPolygon> &polygonsOfGrid,
			const std::vector<AlienWaysPolygon> &polygons, std::vector<uint32_t> &triedIn, uint32_t tryNumber, const double low[3], const double high[3])
		{
			for (int x = BoxOf(low[0]); x <= BoxOf(high[0]); x++)
			{
				for (int y = BoxOf(low[1]); y <= BoxOf(high[1]); y++)
				{
					for (int z = BoxOf(low[2]); z <= BoxOf(high[2]); z++)
					{
						std::pair<size_t, size_t> const run = Boxes::In(boxes, BoxKey(x, y, z));

						for (size_t e = run.first; e < run.second; e++)
						{
							uint32_t const i = boxes[e].polygon;

							if (triedIn[i] == tryNumber)
							{
								continue;
							}

							triedIn[i] = tryNumber;
							GridPolygon const &g = polygonsOfGrid[i];

							if (g.high[0] < low[0] || g.low[0] > high[0] || g.high[1] < low[1] || g.low[1] > high[1] || g.high[2] < low[2] || g.low[2] > high[2])
							{
								continue;
							}

							if (++grid.squareTests > grid.limits.mostSquareTests)
							{
								return -1;
							}

							AlienWaysPolygon const &q = polygons[g.given];
							double one[kMostCutCorners * 3], two[kMostCutCorners * 3];
							int left = q.corners;

							for (int k = 0; k < q.corners; k++)
							{
								one[k * 3] = q.x[k];
								one[k * 3 + 1] = q.y[k];
								one[k * 3 + 2] = q.z[k];
							}

							for (int axis = 0; axis < 3 && left; axis++)
							{
								left = CutPolygon(one, left, 3, axis, low[axis], true, two);
								left = left ? CutPolygon(two, left, 3, axis, high[axis], false, one) : 0;
							}

							if (left)
							{
								return 0;
							}
						}
					}
				}
			}

			return 1;
		}
	};

	for (size_t number = 0; number < sheets.size(); number++)
	{
		if (StopNow())
		{
			return Fail("asked to stop");
		}

		Sheet const &s = sheets[number];

		if (s.kind != kAlienWaysFloor)
		{
			continue;
		}

		bool const steep = -s.n[1] < kNpcGridNearlyFlat;

		for (uint32_t c = s.first; c < s.first + static_cast<uint32_t>(s.w) * static_cast<uint32_t>(s.h); c++)
		{
			if (!(flags[c] & (kNpcGridMiddleOn | kNpcGridCoveredOnly)))
			{
				continue;
			}

			double q[3];
			Middle(c, q);
			double const low[3] = { q[0] - half, q[1] - (kNpcGridStandHeight - 1), q[2] - half };
			double const high[3] = { q[0] + half, q[1] - (kNpcGridStep + 1), q[2] + half };
			int const clear = Square::Clear(*this, boxes, grid, polygons, triedIn, ++tryNumber, low, high);

			if (clear < 0)
			{
				return Fail("too many polygons tried against a marine's square");
			}

			if (clear)
			{
				flags[c] |= steep ? (kNpcGridStands | kNpcGridSteep) : kNpcGridStands;
			}
		}
	}

	// An alien clings where every cell of the 3 x 3 round it whose middle is on its sheet has the room it needs
	for (size_t number = 0; number < sheets.size(); number++)
	{
		Sheet const &s = sheets[number];

		for (int j = 0; j < s.h; j++)
		{
			for (int i = 0; i < s.w; i++)
			{
				uint32_t const c = s.first + static_cast<uint32_t>(j) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(i);

				if (!(flags[c] & kNpcGridMiddleOn))
				{
					continue;
				}

				int least = 255;

				for (int dj = -1; dj <= 1; dj++)
				{
					for (int di = -1; di <= 1; di++)
					{
						int const I = i + di, J = j + dj;

						if (I < 0 || J < 0 || I >= s.w || J >= s.h)
						{
							continue;
						}

						uint32_t const n = s.first + static_cast<uint32_t>(J) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(I);

						if (flags[n] & kNpcGridMiddleOn)
						{
							least = std::min(least, static_cast<int>(room[n]));
						}
					}
				}

				if (least * kNpcGridRoomStep >= kNpcGridClingRoom)
				{
					flags[c] |= kNpcGridClings;
				}
			}
		}
	}

	// 9. STEPS: every marine's place, by the column it's in seen from above, and linked to each place on another sheet in the columns round
	// it, within one and a half cells across and a step up or down
	std::vector<std::pair<uint64_t, uint32_t> > places;

	for (uint32_t c = 0; c < static_cast<uint32_t>(cells); c++)
	{
		if (flags[c] & kNpcGridStands)
		{
			if (!CountBuilding(sizeof(std::pair<uint64_t, uint32_t>)))
			{
				return Fail("too much memory while building, for the marine's places");
			}

			double q[3];
			Middle(c, q);
			places.push_back(std::make_pair(ColumnKey(q[0], q[2]), c));
		}
	}

	std::sort(places.begin(), places.end());

	struct Column
	{
		static std::pair<size_t, size_t> Of(const std::vector<std::pair<uint64_t, uint32_t> > &places, uint64_t key)
		{
			std::pair<uint64_t, uint32_t> const low(key, 0), high(key, 0xFFFFFFFFu);
			std::vector<std::pair<uint64_t, uint32_t> >::const_iterator const a = std::lower_bound(places.begin(), places.end(), low);
			std::vector<std::pair<uint64_t, uint32_t> >::const_iterator const b = std::upper_bound(a, places.end(), high);
			return std::make_pair(static_cast<size_t>(a - places.begin()), static_cast<size_t>(b - places.begin()));
		}
	};

	uint64_t const oneColumnAcross = static_cast<uint64_t>(1) << 32;

	for (size_t k = 0; k < places.size(); k++)
	{
		if ((k & 4095) == 0 && StopNow())
		{
			return Fail("asked to stop");
		}

		uint32_t const c = places[k].second;
		int const sc = SheetOf(c);
		double q[3];
		Middle(c, q);

		for (int dx = -1; dx <= 1; dx++)
		{
			for (int dz = -1; dz <= 1; dz++)
			{
				std::pair<size_t, size_t> const run = Column::Of(places, places[k].first + static_cast<uint64_t>(static_cast<int64_t>(dx)) * oneColumnAcross + static_cast<uint64_t>(static_cast<int64_t>(dz)));

				for (size_t e = run.first; e < run.second; e++)
				{
					uint32_t const o = places[e].second;

					if (o <= c || SheetOf(o) == sc)
					{
						continue;
					}

					double r[3];
					Middle(o, r);

					if (sqrt((r[0] - q[0]) * (r[0] - q[0]) + (r[2] - q[2]) * (r[2] - q[2])) > 1.5 * kNpcGridCell || fabs(r[1] - q[1]) > kNpcGridStep)
					{
						continue;
					}

					if (!CountBuilding(2 * sizeof(Pair)) || !Add::Both(&links, limits.mostLinks, c, o))
					{
						return Fail("too many links");
					}

					steps++;
				}
			}
		}
	}

	// 10. FALLS, one way, as Hudson's grid has them: where the floor ends and a marine walks on, he doesn't drop until his whole square is
	// past the edge, so he lands two columns out, on the highest place there more than a step below, if that's less than kNpcGridMostFall
	// down. Nothing may be in his way, from his landing's step up to his own height, in the square round where he lands. That square
	// covers the two columns he walks out through, and his own square covers the one he starts in, so the walk out needs no test of its own.
	std::vector<Pair> falls;

	struct Place
	{
		// The highest place in a column within a step of y, or (below) the highest more than a step under y and less than a fall away.
		// y goes down, so the highest is the smallest y.
		static uint32_t Near(const NpcGrid &grid, const std::vector<std::pair<uint64_t, uint32_t> > &places, double x, double z, double y, bool below)
		{
			std::pair<size_t, size_t> const run = Column::Of(places, ColumnKey(x, z));
			uint32_t best = kNpcGridNoCell;
			double bestY = 1e300;

			for (size_t e = run.first; e < run.second; e++)
			{
				double m[3];
				grid.Middle(places[e].second, m);
				bool const fits = below ? (m[1] > y + kNpcGridStep && m[1] - y < kNpcGridMostFall) : fabs(m[1] - y) <= kNpcGridStep;

				if (fits && m[1] < bestY)
				{
					bestY = m[1];
					best = places[e].second;
				}
			}

			return best;
		}
	};

	for (size_t k = 0; k < places.size(); k++)
	{
		if ((k & 4095) == 0 && StopNow())
		{
			return Fail("asked to stop");
		}

		uint32_t const c = places[k].second;
		double q[3];
		Middle(c, q);
		int const ways[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

		for (int w = 0; w < 4; w++)
		{
			double const dx = ways[w][0] * kNpcGridCell, dz = ways[w][1] * kNpcGridCell;

			if (Place::Near(*this, places, q[0] + dx, q[2] + dz, q[1], false) != kNpcGridNoCell)
			{
				continue;
			}

			double const x = q[0] + dx * 2, z = q[2] + dz * 2;
			uint32_t const landing = Place::Near(*this, places, x, z, q[1], true);

			if (landing == kNpcGridNoCell)
			{
				continue;
			}

			double l[3];
			Middle(landing, l);
			double const low[3] = { x - half, q[1] - (kNpcGridStandHeight - 1), z - half };
			double const high[3] = { x + half, l[1] - (kNpcGridStep + 1), z + half };
			int const answer = Square::Clear(*this, boxes, grid, polygons, triedIn, ++tryNumber, low, high);

			if (answer < 0)
			{
				return Fail("too many polygons tried against a marine's square");
			}

			if (answer == 0)
			{
				continue;
			}

			if (falls.size() >= limits.mostFalls || !CountBuilding(sizeof(Pair)))
			{
				return Fail("too many falls");
			}

			Pair const fall = { c, landing };
			falls.push_back(fall);
		}
	}

	GiveBack(&boxes);
	GiveBack(&triedIn);
	GiveBack(&places);
	GiveBack(&grid);
	GiveBack(&gridOfMap);

	// 11. WHAT'S KEPT: the links and falls, sorted by the cell they're from, each once
	std::sort(links.begin(), links.end());
	links.erase(std::unique(links.begin(), links.end()), links.end());
	std::sort(falls.begin(), falls.end());
	falls.erase(std::unique(falls.begin(), falls.end()), falls.end());
	size_t const more = (links.size() + falls.size()) * 2 * sizeof(uint32_t);

	if (more > limits.mostBytesKept - std::min(limits.mostBytesKept, bytesKept) || bytesKept > limits.mostBytesKept)
	{
		return Fail("the grid would keep too much memory, for its links");
	}

	bytesKept += more;
	linkFrom.resize(links.size());
	linkTo.resize(links.size());

	for (size_t k = 0; k < links.size(); k++)
	{
		linkFrom[k] = links[k].from;
		linkTo[k] = links[k].to;
	}

	fallFrom.resize(falls.size());
	fallTo.resize(falls.size());

	for (size_t k = 0; k < falls.size(); k++)
	{
		fallFrom[k] = falls[k].from;
		fallTo[k] = falls[k].to;
	}

	bytesBuilding = 0;
	ready = true;
	return nullptr;
}

NpcGridSearch NpcGrid::BeginCells(uint32_t from, uint32_t to, NpcGridBody body, int floorPrice, size_t mostLooks)
{
	seen.clear();
	open.clear();
	wayCells.clear();
	way.clear();
	wayPointCells.clear();
	wayPolygons.clear();
	cutShort = false;
	searchLooks = 0;
	searchFrom = from;
	searchTo = to;
	searchBody = body;
	searchPrice = std::max(1, std::min(floorPrice, kAlienWaysMostFloorPrice));
	searchMostLooks = mostLooks;
	uint8_t const bit = body == kNpcGridMarine ? kNpcGridStands : kNpcGridClings;

	if (!ready || from >= flags.size() || to >= flags.size() || !(flags[from] & bit) || !(flags[to] & bit))
	{
		searchState = kNpcGridNoWay;
		return searchState;
	}

	Seen const start = { 0.0, kNpcGridNoCell, false };
	seen[from] = start;
	open.push_back(std::make_pair(0.0, from));
	searchState = kNpcGridSearching;
	return searchState;
}

const char *NpcGrid::StartSearch(AlienWaysXYZ start, AlienWaysXYZ goal, NpcGridBody body, int floorPrice, size_t mostLooks,
	const AlienWaysClaims *claims, int searcher, const std::vector<uint8_t> *shut, double claimStep, double claimMost)
{
	searchClaims = claims;
	searchSearcher = searcher;
	searchClaimStep = claimStep;
	searchClaimMost = claimMost;
	searchShut = shut;
	searchStart = start;
	searchGoal = goal;
	uint32_t const from = NearestCell(start.x, start.y, start.z, body, kNpcGridReach);
	uint32_t const to = NearestCell(goal.x, goal.y, goal.z, body, kNpcGridReach);

	if (from == kNpcGridNoCell || to == kNpcGridNoCell)
	{
		BeginCells(kNpcGridNoCell, kNpcGridNoCell, body, floorPrice, mostLooks);
		return from == kNpcGridNoCell ? "no cell near the start" : "no cell near the goal";
	}

	BeginCells(from, to, body, floorPrice, mostLooks);
	return nullptr;
}

NpcGridSearch NpcGrid::Search(uint32_t from, uint32_t to, NpcGridBody body, int floorPrice, size_t mostLooks, std::vector<uint32_t> *found, size_t *looks)
{
	searchClaims = nullptr;
	searchSearcher = -1;
	searchShut = nullptr;
	BeginCells(from, to, body, floorPrice, mostLooks);
	NpcGridSearch const result = Work(static_cast<size_t>(-1));
	*found = result == kNpcGridFound ? wayCells : std::vector<uint32_t>();

	if (looks)
	{
		*looks = searchLooks;
	}

	return result;
}

// What a cell costs to cross: for an alien, a floor's is floorPrice times a wall's; and with claims, more where other aliens' ways are,
// by the claimed polygon under the cell. Never under 1, so the search's guess never says more than a cell really costs.
double NpcGrid::PriceOf(uint32_t cell) const
{
	double price = searchBody == kNpcGridAlien && KindOf(cell) == kAlienWaysFloor ? searchPrice : 1.0;
	int const polygon = PolygonOf(cell);

	if (searchClaims && polygon >= 0)
	{
		price *= searchClaims->PriceFactor(polygon, searchSearcher, searchClaimStep, searchClaimMost);
	}

	return price;
}

// Whether a cell is on a shut door's polygon. The start and the goal never are, so an alien on a door can leave it.
bool NpcGrid::ShutOff(uint32_t cell) const
{
	int const polygon = PolygonOf(cell);
	return searchShut && cell != searchFrom && cell != searchTo && polygon >= 0 && static_cast<size_t>(polygon) < searchShut->size() &&
		(*searchShut)[polygon] != 0;
}

// Tries one move of the search, to cell n at a new cost
void NpcGrid::TryMove(const double goalAt[3], const double at[3], uint32_t c, uint32_t n, double newCost)
{
	std::unordered_map<uint32_t, Seen>::iterator const found = seen.find(n);

	if ((found != seen.end() && (found->second.done || found->second.cost <= newCost)) || ShutOff(n))
	{
		return;
	}

	Seen const s = { newCost, c, false };
	seen[n] = s;
	double const guess = sqrt((at[0] - goalAt[0]) * (at[0] - goalAt[0]) + (at[1] - goalAt[1]) * (at[1] - goalAt[1]) + (at[2] - goalAt[2]) * (at[2] - goalAt[2]));
	open.push_back(std::make_pair(newCost + guess, n));
	std::push_heap(open.begin(), open.end(), std::greater<std::pair<double, uint32_t> >());
}

NpcGridSearch NpcGrid::Work(size_t looksAllowed)
{
	if (searchState != kNpcGridSearching)
	{
		return searchState;
	}

	uint8_t const bit = searchBody == kNpcGridMarine ? kNpcGridStands : kNpcGridClings;
	double goal[3];
	Middle(searchTo, goal);
	size_t looksNow = 0;

	while (!open.empty())
	{
		if (looksNow >= looksAllowed)
		{
			return searchState;
		}

		std::pop_heap(open.begin(), open.end(), std::greater<std::pair<double, uint32_t> >());
		uint32_t const c = open.back().second;
		open.pop_back();
		Seen &here = seen[c];

		// A cell finished already was put in the list again before it was, and this is the older copy: the guess never says more than a
		// cell really costs, so the first time a cell comes out is at its best
		if (here.done)
		{
			continue;
		}

		here.done = true;
		searchLooks++;
		looksNow++;

		if (c == searchTo)
		{
			for (uint32_t at = searchTo; at != kNpcGridNoCell; at = seen[at].cameFrom)
			{
				wayCells.push_back(at);
			}

			std::reverse(wayCells.begin(), wayCells.end());
			MakeWay();
			searchState = kNpcGridFound;
			return searchState;
		}

		if (searchLooks >= searchMostLooks)
		{
			searchState = kNpcGridTooFar;
			return searchState;
		}

		// here can move if the list grows, so what's needed of it is copied first
		double const cost = here.cost;
		double a[3];
		Middle(c, a);
		Sheet const &s = sheets[SheetOf(c)];
		int const i = static_cast<int>((c - s.first) % s.w), j = static_cast<int>((c - s.first) / s.w);
		double const price = PriceOf(c);

		for (int dj = -1; dj <= 1; dj++)
		{
			for (int di = -1; di <= 1; di++)
			{
				int const I = i + di, J = j + dj;

				if ((di == 0 && dj == 0) || I < 0 || J < 0 || I >= s.w || J >= s.h)
				{
					continue;
				}

				uint32_t const n = s.first + static_cast<uint32_t>(J) * static_cast<uint32_t>(s.w) + static_cast<uint32_t>(I);

				if (!(flags[n] & bit))
				{
					continue;
				}

				// Going diagonally needs both cells beside it, or the body cuts a corner
				if (di != 0 && dj != 0 && (!(flags[s.first + static_cast<uint32_t>(j) * s.w + static_cast<uint32_t>(I)] & bit) ||
					!(flags[s.first + static_cast<uint32_t>(J) * s.w + static_cast<uint32_t>(i)] & bit)))
				{
					continue;
				}

				double b[3];
				Middle(n, b);

				// A marine doesn't climb a steep floor. y goes down, so up is a smaller y.
				if (searchBody == kNpcGridMarine && ((flags[n] | flags[c]) & kNpcGridSteep) && b[1] < a[1])
				{
					continue;
				}

				double const step = (di != 0 && dj != 0 ? 1.4142 : 1.0) * kNpcGridCell * price;
				TryMove(goal, b, c, n, cost + step);
			}
		}

		size_t count = 0;
		const uint32_t *list = LinksOf(c, &count);

		for (size_t k = 0; k < count; k++)
		{
			uint32_t const n = list[k];

			if (!(flags[n] & bit))
			{
				continue;
			}

			double b[3];
			Middle(n, b);

			if (searchBody == kNpcGridMarine)
			{
				if (KindOf(n) != kAlienWaysFloor || fabs(a[1] - b[1]) > kNpcGridStep)
				{
					continue;
				}

				// Up onto a steep floor, or up off one, by more than half a step
				if (((flags[n] | flags[c]) & kNpcGridSteep) && b[1] < a[1] - kNpcGridStep / 2)
				{
					continue;
				}
			}

			double const far = sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
			TryMove(goal, b, c, n, cost + far * PriceOf(n));
		}

		if (searchBody == kNpcGridMarine)
		{
			list = FallsOf(c, &count);

			for (size_t k = 0; k < count; k++)
			{
				double b[3];
				Middle(list[k], b);
				double const far = sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
				TryMove(goal, b, c, list[k], cost + far);
			}
		}
	}

	searchState = kNpcGridNoWay;
	return searchState;
}

// One point of a way: a place, and the kind and facing of its cell's sheet, the facing out of the surface in 32767ths as the level stores one
AlienWaysPoint NpcGrid::PointAt(uint32_t cell, double x, double y, double z) const
{
	AlienWaysPoint p;
	p.x = static_cast<int>(floor(x + 0.5));
	p.y = static_cast<int>(floor(y + 0.5));
	p.z = static_cast<int>(floor(z + 0.5));
	int const sheet = SheetOf(cell);
	p.kind = KindOf(cell);

	for (int k = 0; k < 3; k++)
	{
		p.facing[k] = sheet < 0 ? static_cast<int16_t>(0) : static_cast<int16_t>(floor(sheets[sheet].n[k] * 32767.0 + 0.5));
	}

	return p;
}

// The cells of the way found, as points: the start, then a cell's middle wherever the way turns or goes onto another sheet, then the goal.
// Each point's kind and facing are those of its cell's sheet: the surface the way is on from that point to the next.
void NpcGrid::MakeWay()
{
	way.clear();
	wayPointCells.clear();
	wayPolygons.clear();
	cutShort = false;

	if (wayCells.empty())
	{
		return;
	}

	way.push_back(PointAt(wayCells.front(), searchStart.x, searchStart.y, searchStart.z));
	wayPointCells.push_back(wayCells.front());
	wayPolygons.push_back(PolygonOf(wayCells.front()));

	for (size_t k = 1; k + 1 < wayCells.size(); k++)
	{
		double a[3], b[3], c[3];
		Middle(wayCells[k - 1], a);
		Middle(wayCells[k], b);
		Middle(wayCells[k + 1], c);
		bool const sameSheet = SheetOf(wayCells[k - 1]) == SheetOf(wayCells[k]) && SheetOf(wayCells[k]) == SheetOf(wayCells[k + 1]);

		// The same direction in and out: the two steps point the same way, so their cross is nothing
		double const in[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, out[3] = { c[0] - b[0], c[1] - b[1], c[2] - b[2] };
		double turn[3];
		Cross(in, out, turn);

		if (sameSheet && Dot(turn, turn) < 1.0 && Dot(in, out) > 0.0)
		{
			continue;
		}

		way.push_back(PointAt(wayCells[k], b[0], b[1], b[2]));
		wayPointCells.push_back(wayCells[k]);
		wayPolygons.push_back(PolygonOf(wayCells[k]));

		// Room is kept for the goal: a longer way is cut short there, and says so
		if (way.size() + 1 >= kAlienWaysMostWayPoints)
		{
			cutShort = true;
			return;
		}
	}

	way.push_back(PointAt(wayCells.back(), searchGoal.x, searchGoal.y, searchGoal.z));
	wayPointCells.push_back(wayCells.back());
	wayPolygons.push_back(PolygonOf(wayCells.back()));
}
