// See alien_ways_map.h. Written 2026-11-08 by Melanikus for the AvP port.
#include "alien_ways_map.h"

#include <float.h>
#include <limits.h>
#include <math.h>

#include <algorithm>
#include <new>

namespace
{
	// The corners are sorted into boxes a metre on each side, so what's near an edge can be found without looking at every corner.
	int const kBox = 1000;

	// Each edge is walked in steps of half a metre, and at each step the boxes within kAlienWaysTouch of it are looked in. Half a box, so no box
	// an edge passes through is missed.
	int const kEdgeStep = 500;

	// A pair of polygons keeps at most this many of its touching points. The two furthest apart become the stretch where they touch.
	size_t const kMostTouchesPerPair = 16;

	// The longest a single stretch of a way can be: corner to corner across the whole world the map takes, which is 128 km along each axis.
	constexpr double kLongestStretch = 2.0 * kAlienWaysMostCoordinate * 1.7320508075688772;

	// A way's cost is a sum of stretches, each at most kLongestStretch times the dearest price, and a way crosses each polygon at most once. So it
	// can never come anywhere near the largest number a double holds, and a search can't overflow, whatever the level says.
	static_assert(kLongestStretch * kAlienWaysMostFloorPrice * 1000000.0 < DBL_MAX / 1e6, "a way's cost could overflow");

	// THE SAME ANSWER EVERY TIME. Costs are doubles. The game's 32-bit build does its sums with SSE2 (EnableEnhancedInstructionSet in
	// win32/VS2026/AvP.vcxproj), not the old x87 unit whose extra precision could change from one moment to the next, so a given build
	// finds the same way for the same question on every run. Release also has /fp:fast, which lets the compiler order sums its own way, so
	// two different builds could round differently in the last place. That doesn't matter here: only the machine that runs the AI searches,
	// so no two machines ever have to agree on a way.

	// The reasons, word for word. tests/alien_ways_map_test.cpp checks each one.
	const char *const kWhyCorners = "a polygon has fewer than 3 corners, or more than 8";
	const char *const kWhyFacing = "a polygon's facing isn't a number from -1 to 1";
	const char *const kWhyPolygons = "the level has more polygons than the map takes";
	const char *const kWhyCrowded = "too many corners lie close together in one place";
	const char *const kWhyEdgeSteps = "the polygons' edges are too long to join them up";
	const char *const kWhyCornerTests = "joining the polygons up would take too long";
	const char *const kWhyJoins = "the polygons touch in more places than the map takes";
	const char *const kWhyBuilding = "building the map would need more memory than it may have";
	const char *const kWhyKept = "the map would keep more memory than it may";
	const char *const kWhyNoMemory = "the computer had no memory to give";
	const char *const kWhyStopped = "the build was asked to stop";

	const char *const kWhyNoMap = "there's no map to search";
	const char *const kWhyNoPolygon = "a polygon to search from or to isn't on the map";
	const char *const kWhyPrice = "the floor's price has to be from 1 to 1000";
	const char *const kWhyPoint = "a point to search from or to is further out than the map goes";

	bool FarOut(int v)
	{
		return v < -kAlienWaysMostCoordinate || v > kAlienWaysMostCoordinate;
	}

	// Which box a number falls in. Rounded down, for numbers below 0 too.
	int BoxOf(double v)
	{
		return static_cast<int>(floor(v / kBox));
	}

	// One box's key, from its three numbers. A kept corner is at most 64 km out and each box a metre, so each number is within 65,536 either side
	// of 0, and fits in 17 bits once 65,536 is added.
	long long KeyOf(int bx, int by, int bz)
	{
		long long const offset = 65536;
		return ((bx + offset) << 34) | ((by + offset) << 17) | (bz + offset);
	}

	double Distance(AlienWaysXYZ a, AlienWaysXYZ b)
	{
		long long const dx = static_cast<long long>(a.x) - b.x;
		long long const dy = static_cast<long long>(a.y) - b.y;
		long long const dz = static_cast<long long>(a.z) - b.z;
		return sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz));
	}

	// The point on the stretch from a to b that's nearest to p
	AlienWaysXYZ NearestOn(AlienWaysXYZ a, AlienWaysXYZ b, AlienWaysXYZ p)
	{
		double const ux = static_cast<double>(b.x) - a.x, uy = static_cast<double>(b.y) - a.y, uz = static_cast<double>(b.z) - a.z;
		double const uu = ux * ux + uy * uy + uz * uz;

		if (uu <= 0.0)
		{
			return a;
		}

		double t = ((static_cast<double>(p.x) - a.x) * ux + (static_cast<double>(p.y) - a.y) * uy + (static_cast<double>(p.z) - a.z) * uz) / uu;
		t = std::max(0.0, std::min(1.0, t));
		AlienWaysXYZ out = { static_cast<int>(lround(a.x + ux * t)), static_cast<int>(lround(a.y + uy * t)), static_cast<int>(lround(a.z + uz * t)) };
		return out;
	}

	// A point on the stretch from a to b, moved up it to at least kAlienWaysClimbHeight above the stretch's lowest point, or to its highest
	// point if that's lower. y goes down, so up is a smaller y. A stretch that's level all along can't take a point any higher, so it's left.
	AlienWaysXYZ LiftedOn(AlienWaysXYZ a, AlienWaysXYZ b, AlienWaysXYZ p)
	{
		long long const lowest = std::max(a.y, b.y), highest = std::min(a.y, b.y);
		long long const wanted = std::max(highest, lowest - kAlienWaysClimbHeight);

		if (p.y <= wanted || a.y == b.y)
		{
			return p;
		}

		double const t = std::max(0.0, std::min(1.0, (static_cast<double>(wanted) - a.y) / (static_cast<double>(b.y) - a.y)));
		AlienWaysXYZ out = { static_cast<int>(lround(a.x + (static_cast<double>(b.x) - a.x) * t)), static_cast<int>(lround(a.y + (static_cast<double>(b.y) - a.y) * t)),
			static_cast<int>(lround(a.z + (static_cast<double>(b.z) - a.z) * t)) };
		return out;
	}

	// How far p is from the edge a-b, in millimetres
	double DistanceToEdge(AlienWaysXYZ a, AlienWaysXYZ b, AlienWaysXYZ p)
	{
		double const ux = static_cast<double>(b.x) - a.x, uy = static_cast<double>(b.y) - a.y, uz = static_cast<double>(b.z) - a.z;
		double const uu = ux * ux + uy * uy + uz * uz;
		double t = uu > 0.0 ? ((static_cast<double>(p.x) - a.x) * ux + (static_cast<double>(p.y) - a.y) * uy + (static_cast<double>(p.z) - a.z) * uz) / uu : 0.0;
		t = std::max(0.0, std::min(1.0, t));
		double const dx = a.x + ux * t - p.x, dy = a.y + uy * t - p.y, dz = a.z + uz * t - p.z;
		return sqrt(dx * dx + dy * dy + dz * dz);
	}
}

AlienWaysLimits AlienWays_UsualLimits()
{
	AlienWaysLimits limits;
	limits.mostPolygons = 200000;
	limits.mostCornersInOnePlace = 4096;
	limits.mostEdgeSteps = 10000000;
	limits.mostCornerTests = 100000000;
	limits.mostJoins = 1000000;
	limits.mostBytesKept = 32u * 1024u * 1024u;
	limits.mostBytesBuilding = 64u * 1024u * 1024u;
	return limits;
}

AlienWaysKind AlienWays_KindOf(float up)
{
	if (up >= kAlienWaysFloorFacing)
	{
		return kAlienWaysFloor;
	}

	if (up <= -kAlienWaysFloorFacing)
	{
		return kAlienWaysCeiling;
	}

	return kAlienWaysWall;
}

float AlienWays_HowMuchItFacesUp(const AlienWaysPolygon &polygon)
{
	// Newell's sums over every corner give the polygon's facing, however its first three corners lie. y goes down in the game, so "up" is the
	// part of the facing towards smaller y. The minus was measured on the stock levels: AvP-docs, engine/pathfinding-study/level.js.
	double nx = 0.0, ny = 0.0, nz = 0.0;
	int const n = polygon.corners;

	for (int i = 0; i < n; i++)
	{
		int const j = (i + 1) % n;
		nx += (static_cast<double>(polygon.y[i]) - polygon.y[j]) * (static_cast<double>(polygon.z[i]) + polygon.z[j]);
		ny += (static_cast<double>(polygon.z[i]) - polygon.z[j]) * (static_cast<double>(polygon.x[i]) + polygon.x[j]);
		nz += (static_cast<double>(polygon.x[i]) - polygon.x[j]) * (static_cast<double>(polygon.y[i]) + polygon.y[j]);
	}

	double const size = sqrt(nx * nx + ny * ny + nz * nz);

	if (size <= 0.0)
	{
		return 0.0f;
	}

	return static_cast<float>(-ny / size);
}

// One of the game's 65536ths sums, a times b, in 64 bits. Two ints multiplied are under 2^62, so this can't overflow. The game's own
// MUL_FIXED does the same and then keeps the low 32 bits, which is the same number whenever the answer fits in an int.
static long long TimesFixed(int a, int b)
{
	return (static_cast<long long>(a) * b) >> 16;
}

bool AlienWays_PlaceCorner(AlienWaysXYZ point, const AlienWaysTurn *turn, AlienWaysXYZ place, AlienWaysXYZ *placed)
{
	long long x = point.x, y = point.y, z = point.z;

	if (turn)
	{
		x = TimesFixed(turn->m11, point.x) + TimesFixed(turn->m21, point.y) + TimesFixed(turn->m31, point.z);
		y = TimesFixed(turn->m12, point.x) + TimesFixed(turn->m22, point.y) + TimesFixed(turn->m32, point.z);
		z = TimesFixed(turn->m13, point.x) + TimesFixed(turn->m23, point.y) + TimesFixed(turn->m33, point.z);
	}

	// Each part is under 2^47 and the place under 2^31, so these sums are far inside what 64 bits hold
	x += place.x;
	y += place.y;
	z += place.z;

	long long const most = kAlienWaysMostCoordinate;

	if (x < -most || x > most || y < -most || y > most || z < -most || z > most)
	{
		return false;
	}

	placed->x = static_cast<int>(x);
	placed->y = static_cast<int>(y);
	placed->z = static_cast<int>(z);
	return true;
}

float AlienWays_TurnedUp(AlienWaysXYZ facing, const AlienWaysTurn *turn)
{
	long long const y = turn ? TimesFixed(turn->m12, facing.x) + TimesFixed(turn->m22, facing.y) + TimesFixed(turn->m32, facing.z) : facing.y;

	// y goes down, so facing up is a negative y. A stored facing is a little over or under 1 long at times, so it's kept from -1 to 1.
	double const up = static_cast<double>(-y) / 65536.0;
	return static_cast<float>(up > 1.0 ? 1.0 : (up < -1.0 ? -1.0 : up));
}

AlienWaysAdded AlienWays_AddPolygon(const int *x, const int *y, const int *z, int corners, float up, std::vector<AlienWaysPolygon> *list)
{
	if (corners < 3)
	{
		return kAlienWaysLeftOutTooFewCorners;
	}

	if (corners > kAlienWaysMostCorners)
	{
		return kAlienWaysLeftOutTooManyCorners;
	}

	AlienWaysPolygon p;
	p.corners = corners;

	for (int c = 0; c < kAlienWaysMostCorners; c++)
	{
		p.x[c] = c < corners ? x[c] : 0;
		p.y[c] = c < corners ? y[c] : 0;
		p.z[c] = c < corners ? z[c] : 0;
	}

	p.up = up;
	list->push_back(p);
	return kAlienWaysAddedAsItIs;
}

AlienWaysMap::AlienWaysMap()
	: ready(false), limits(AlienWays_UsualLimits()), stopAsked(false), stopAtLook(0), stopLooks(0), leftOut(0), bytesKept(0), bytesBuilding(0), bytesBuildingTop(0), bytesBuildingUsed(0),
	  edgeSteps(0), cornerTests(0), heapSize(0), searchNumber(0), state(kAlienWaysNoWay), fromNode(-1), toNode(-1), price(1), looks(0),
	  cutShort(false)
{
	goalAt.x = goalAt.y = goalAt.z = 0;
}

// Empties a list and gives its memory back. clear() alone would keep the memory.
template <typename T> static void GiveBack(std::vector<T> *list)
{
	std::vector<T>().swap(*list);
}

void AlienWaysMap::LetGo()
{
	ready = false;
	leftOut = bytesKept = bytesBuilding = bytesBuildingTop = bytesBuildingUsed = edgeSteps = cornerTests = 0;
	GiveBack(&mapNumber);
	GiveBack(&middleX); GiveBack(&middleY); GiveBack(&middleZ);
	GiveBack(&kind);
	GiveBack(&boxLow); GiveBack(&boxHigh);
	GiveBack(&firstJoin);
	GiveBack(&joinOther); GiveBack(&joinStretch);
	GiveBack(&stretchA); GiveBack(&stretchB);
	GiveBack(&cornerX); GiveBack(&cornerY); GiveBack(&cornerZ);
	GiveBack(&firstCorner);
	GiveBack(&buckets);
	GiveBack(&touches);
	GiveBack(&cost); GiveBack(&guess);
	GiveBack(&cameFrom);
	GiveBack(&entry);
	GiveBack(&stamp); GiveBack(&closed);
	GiveBack(&heap); GiveBack(&heapPlace);
	GiveBack(&way);
	heapSize = 0;
	searchNumber = 0;
	state = kAlienWaysNoWay;
	looks = 0;
	cutShort = false;
}

void AlienWaysMap::Clear()
{
	LetGo();
	stopAsked.store(false);
	stopLooks = 0;
}

// Every loop of the build asks this. It also counts the asking, so a test can make the build stop at a chosen point.
bool AlienWaysMap::Stopping()
{
	stopLooks++;

	if (stopAtLook != 0 && stopLooks >= stopAtLook)
	{
		stopAsked.store(true);
	}

	return stopAsked.load();
}

// Adds bytes to a running total, unless that would take it past the most allowed. Says whether it did.
bool AlienWaysMap::Count(size_t bytes, size_t *total, size_t most)
{
	if (*total > most || bytes > most - *total)
	{
		return false;
	}

	*total += bytes;
	return true;
}

int AlienWaysMap::MapNumberOf(size_t given) const
{
	return given < mapNumber.size() ? mapNumber[given] : -1;
}

bool AlienWaysMap::Joined(int a, int b, AlienWaysXYZ *from, AlienWaysXYZ *to) const
{
	if (!ready || a < 0 || b < 0 || a >= static_cast<int>(Polygons()) || b >= static_cast<int>(Polygons()))
	{
		return false;
	}

	for (int k = firstJoin[a]; k < firstJoin[a + 1]; k++)
	{
		if (joinOther[k] == b)
		{
			*from = stretchA[joinStretch[k]];
			*to = stretchB[joinStretch[k]];
			return true;
		}
	}

	return false;
}

int AlienWaysMap::NearestPolygon(AlienWaysXYZ point, size_t firstGiven, size_t endGiven, int within, double *howFar) const
{
	return NearestPolygonOfKind(point, firstGiven, endGiven, within, howFar, -1);
}

int AlienWaysMap::NearestPolygonOfKind(AlienWaysXYZ point, size_t firstGiven, size_t endGiven, int within, double *howFar, int wanted) const
{
	if (!ready || within < 0)
	{
		return -1;
	}

	endGiven = std::min(endGiven, mapNumber.size());
	int best = -1;
	double bestDistance = static_cast<double>(within), bestToMiddle = 0.0;

	for (size_t given = firstGiven; given < endGiven; given++)
	{
		int const polygon = mapNumber[given];

		if (polygon < 0 || (wanted >= 0 && kind[polygon] != wanted))
		{
			continue;
		}

		// How far the point is outside the box along each axis, 0 if it's within the box's span there
		AlienWaysXYZ const low = boxLow[polygon], high = boxHigh[polygon];
		double const dx = point.x < low.x ? static_cast<double>(low.x) - point.x : (point.x > high.x ? static_cast<double>(point.x) - high.x : 0.0);
		double const dy = point.y < low.y ? static_cast<double>(low.y) - point.y : (point.y > high.y ? static_cast<double>(point.y) - high.y : 0.0);
		double const dz = point.z < low.z ? static_cast<double>(low.z) - point.z : (point.z > high.z ? static_cast<double>(point.z) - high.z : 0.0);
		double const distance = sqrt(dx * dx + dy * dy + dz * dz);

		if (distance > bestDistance)
		{
			continue;
		}

		// A sloping polygon's box can hold a point that's well off the polygon itself. So when two boxes are as near, the nearer middle wins.
		double const mx = static_cast<double>(middleX[polygon]) - point.x;
		double const my = static_cast<double>(middleY[polygon]) - point.y;
		double const mz = static_cast<double>(middleZ[polygon]) - point.z;
		double const toMiddle = mx * mx + my * my + mz * mz;

		if (best < 0 || distance < bestDistance || toMiddle < bestToMiddle)
		{
			best = polygon;
			bestDistance = distance;
			bestToMiddle = toMiddle;
		}
	}

	if (howFar && best >= 0)
	{
		*howFar = bestDistance;
	}

	return best;
}

const char *AlienWaysMap::Build(const std::vector<AlienWaysPolygon> &polygons)
{
	const char *why;

	try
	{
		why = BuildIt(polygons);
	}
	catch (std::bad_alloc &)
	{
		why = kWhyNoMemory;
	}

	if (why)
	{
		// Nothing at all is kept from a map that couldn't be made
		LetGo();
	}

	return why;
}

const char *AlienWaysMap::BuildIt(const std::vector<AlienWaysPolygon> &polygons)
{
	LetGo();

	if (Stopping())
	{
		return kWhyStopped;
	}

	// --- 1. Look at every polygon. Refuse a level with one that can't be right, and leave out scenery.
	size_t kept = 0, corners = 0;
	std::vector<uint8_t> keep;

	if (!Count(polygons.size(), &bytesBuilding, limits.mostBytesBuilding))
	{
		return kWhyBuilding;
	}

	keep.assign(polygons.size(), 0);

	for (size_t i = 0; i < polygons.size(); i++)
	{
		if ((i & 255) == 0 && Stopping())
		{
			return kWhyStopped;
		}

		AlienWaysPolygon const &p = polygons[i];

		if (p.corners < 3 || p.corners > kAlienWaysMostCorners)
		{
			return kWhyCorners;
		}

		// Written this way round so that a facing that isn't a number at all is refused too: every comparison with one is false
		if (!(p.up >= -1.0f && p.up <= 1.0f))
		{
			return kWhyFacing;
		}

		bool outside = false;
		long long lowX = LLONG_MAX, highX = LLONG_MIN, lowY = LLONG_MAX, highY = LLONG_MIN, lowZ = LLONG_MAX, highZ = LLONG_MIN;

		for (int c = 0; c < p.corners; c++)
		{
			outside = outside || FarOut(p.x[c]) || FarOut(p.y[c]) || FarOut(p.z[c]);
			lowX = std::min(lowX, static_cast<long long>(p.x[c])); highX = std::max(highX, static_cast<long long>(p.x[c]));
			lowY = std::min(lowY, static_cast<long long>(p.y[c])); highY = std::max(highY, static_cast<long long>(p.y[c]));
			lowZ = std::min(lowZ, static_cast<long long>(p.z[c])); highZ = std::max(highZ, static_cast<long long>(p.z[c]));
		}

		// Scenery, left out but not refused: a polygon far out of the world, one that covers more than 5,000 square metres seen from above, and a
		// wall taller than 50 m. Far out is checked first, so the sums after it can't be too big for their numbers.
		if (outside || (highX - lowX) * (highZ - lowZ) > kAlienWaysMostFlatArea
			|| (AlienWays_KindOf(p.up) == kAlienWaysWall && highY - lowY > kAlienWaysMostWallHeight))
		{
			leftOut++;
			continue;
		}

		if (kept == limits.mostPolygons)
		{
			return kWhyPolygons;
		}

		keep[i] = 1;
		kept++;
		corners += p.corners;
	}

	// --- 2. Copy the polygons kept, and sort their corners into boxes. Everything is counted against its ceiling before it's made. Kept: which
	// number each given polygon became, and each polygon's middle, kind and box. Only while building: the corners, and the boxes they're sorted into.
	size_t const keptBytes = polygons.size() * sizeof(int) + kept * (3 * sizeof(int) + sizeof(uint8_t) + 2 * sizeof(AlienWaysXYZ));
	size_t const buildBytes = corners * 3 * sizeof(int) + (kept + 1) * sizeof(int) + corners * sizeof(Corner);

	if (!Count(keptBytes, &bytesKept, limits.mostBytesKept))
	{
		return kWhyKept;
	}

	if (!Count(buildBytes, &bytesBuilding, limits.mostBytesBuilding))
	{
		return kWhyBuilding;
	}

	mapNumber.assign(polygons.size(), -1);
	middleX.reserve(kept); middleY.reserve(kept); middleZ.reserve(kept);
	kind.reserve(kept);
	boxLow.reserve(kept); boxHigh.reserve(kept);
	cornerX.reserve(corners); cornerY.reserve(corners); cornerZ.reserve(corners);
	firstCorner.reserve(kept + 1);
	buckets.reserve(corners);

	for (size_t i = 0; i < polygons.size(); i++)
	{
		if (!keep[i])
		{
			continue;
		}

		AlienWaysPolygon const &p = polygons[i];
		int const number = static_cast<int>(firstCorner.size());
		mapNumber[i] = number;
		firstCorner.push_back(static_cast<int>(cornerX.size()));
		kind.push_back(static_cast<uint8_t>(AlienWays_KindOf(p.up)));

		// The middle is the average of the corners. The sums can't overflow: each corner is at most 64 km out, and there are at most 8.
		long long sx = 0, sy = 0, sz = 0;
		AlienWaysXYZ low = { p.x[0], p.y[0], p.z[0] }, high = low;

		for (int c = 0; c < p.corners; c++)
		{
			sx += p.x[c]; sy += p.y[c]; sz += p.z[c];
			low.x = std::min(low.x, p.x[c]); low.y = std::min(low.y, p.y[c]); low.z = std::min(low.z, p.z[c]);
			high.x = std::max(high.x, p.x[c]); high.y = std::max(high.y, p.y[c]); high.z = std::max(high.z, p.z[c]);
			cornerX.push_back(p.x[c]); cornerY.push_back(p.y[c]); cornerZ.push_back(p.z[c]);
			Corner corner = { KeyOf(BoxOf(p.x[c]), BoxOf(p.y[c]), BoxOf(p.z[c])), number, p.x[c], p.y[c], p.z[c] };
			buckets.push_back(corner);
		}

		middleX.push_back(static_cast<int>(sx / p.corners));
		middleY.push_back(static_cast<int>(sy / p.corners));
		middleZ.push_back(static_cast<int>(sz / p.corners));
		boxLow.push_back(low);
		boxHigh.push_back(high);
	}

	firstCorner.push_back(static_cast<int>(cornerX.size()));
	GiveBack(&keep);

	std::sort(buckets.begin(), buckets.end(), [](const Corner &a, const Corner &b) { return a.box < b.box; });

	// Too many corners in one box would make joining up the polygons there take a long time
	for (size_t i = 0; i < buckets.size();)
	{
		size_t j = i;

		while (j < buckets.size() && buckets[j].box == buckets[i].box)
		{
			j++;
		}

		if (j - i > limits.mostCornersInOnePlace)
		{
			return kWhyCrowded;
		}

		i = j;
	}

	// --- 3. Find where they touch: every corner lying within kAlienWaysTouch of another polygon's edge, its corners included
	for (size_t p = 0; p + 1 < firstCorner.size(); p++)
	{
		if (Stopping())
		{
			return kWhyStopped;
		}

		int const first = firstCorner[p], end = firstCorner[p + 1], n = end - first;

		for (int c = 0; c < n; c++)
		{
			int const a = first + c, b = first + (c + 1) % n;
			AlienWaysXYZ const from = { cornerX[a], cornerY[a], cornerZ[a] }, to = { cornerX[b], cornerY[b], cornerZ[b] };
			const char *const why = JoinEdge(static_cast<int>(p), from, to);

			if (why)
			{
				return why;
			}
		}
	}

	GiveBack(&buckets);
	GiveBack(&cornerX); GiveBack(&cornerY); GiveBack(&cornerZ);

	// --- 4. Make one join for each pair that touches, with the stretch between its two touching points furthest apart
	std::sort(touches.begin(), touches.end(), [](const Touch &a, const Touch &b) { return a.low != b.low ? a.low < b.low : a.high < b.high; });

	size_t joins = 0;

	for (size_t i = 0; i < touches.size();)
	{
		size_t j = i;

		while (j < touches.size() && touches[j].low == touches[i].low && touches[j].high == touches[i].high)
		{
			j++;
		}

		joins++;
		i = j;
	}

	if (joins > limits.mostJoins)
	{
		return kWhyJoins;
	}

	// The rest of what's kept, counted before any of it is made: where each polygon's joins start, the joins both ways, the stretches, and the
	// room for one search. A search has one node more than there are polygons: the goal.
	size_t const nodes = kept + 1;
	size_t const keepBytes = (nodes + 1) * sizeof(int) + joins * 2 * 2 * sizeof(int) + joins * 2 * sizeof(AlienWaysXYZ)
		+ nodes * (2 * sizeof(double) + sizeof(int) + sizeof(AlienWaysXYZ) + 2 * sizeof(uint32_t) + 2 * sizeof(int))
		+ kAlienWaysMostWayPoints * sizeof(AlienWaysPoint);

	if (!Count(keepBytes, &bytesKept, limits.mostBytesKept))
	{
		return kWhyKept;
	}

	stretchA.reserve(joins); stretchB.reserve(joins);
	std::vector<int> pairLow, pairHigh;
	pairLow.reserve(joins); pairHigh.reserve(joins);

	// Only while building: the pairs, and where each polygon's next join goes as they're laid out
	if (!Count(joins * 2 * sizeof(int) + nodes * sizeof(int), &bytesBuilding, limits.mostBytesBuilding))
	{
		return kWhyBuilding;
	}

	for (size_t i = 0; i < touches.size();)
	{
		if (Stopping())
		{
			return kWhyStopped;
		}

		size_t j = i;
		AlienWaysXYZ points[kMostTouchesPerPair];
		size_t count = 0;

		while (j < touches.size() && touches[j].low == touches[i].low && touches[j].high == touches[i].high)
		{
			AlienWaysXYZ const at = { touches[j].x, touches[j].y, touches[j].z };
			bool already = false;

			for (size_t q = 0; q < count && !already; q++)
			{
				already = Distance(points[q], at) <= kAlienWaysTouch;
			}

			if (!already && count < kMostTouchesPerPair)
			{
				points[count++] = at;
			}

			j++;
		}

		AlienWaysXYZ a = points[0], b = points[0];
		double furthest = 0.0;

		for (size_t x = 0; x < count; x++)
		{
			for (size_t y = x + 1; y < count; y++)
			{
				double const d = Distance(points[x], points[y]);

				if (d > furthest)
				{
					furthest = d;
					a = points[x];
					b = points[y];
				}
			}
		}

		stretchA.push_back(a);
		stretchB.push_back(b);
		pairLow.push_back(touches[i].low);
		pairHigh.push_back(touches[i].high);
		i = j;
	}

	GiveBack(&touches);

	// Each polygon's joins next to each other, so a search reads them in one go
	firstJoin.assign(nodes + 1, 0);

	for (size_t k = 0; k < joins; k++)
	{
		firstJoin[pairLow[k] + 1]++;
		firstJoin[pairHigh[k] + 1]++;
	}

	for (size_t p = 0; p < nodes; p++)
	{
		firstJoin[p + 1] += firstJoin[p];
	}

	joinOther.resize(joins * 2);
	joinStretch.resize(joins * 2);
	std::vector<int> next(firstJoin.begin(), firstJoin.end() - 1);

	for (size_t k = 0; k < joins; k++)
	{
		joinOther[next[pairLow[k]]] = pairHigh[k];
		joinStretch[next[pairLow[k]]++] = static_cast<int>(k);
		joinOther[next[pairHigh[k]]] = pairLow[k];
		joinStretch[next[pairHigh[k]]++] = static_cast<int>(k);
	}

	// The room for one search, made now so a search never asks for memory
	cost.assign(nodes, 0.0);
	guess.assign(nodes, 0.0);
	cameFrom.assign(nodes, -1);
	AlienWaysXYZ const nowhere = { 0, 0, 0 };
	entry.assign(nodes, nowhere);
	stamp.assign(nodes, 0);
	closed.assign(nodes, 0);
	heap.assign(nodes, 0);
	heapPlace.assign(nodes, -1);
	way.reserve(kAlienWaysMostWayPoints);
	GiveBack(&firstCorner);

	// Everything that was only for building has been let go. The most it needed is the count, or the top of a copy, if that was more.
	bytesBuildingUsed = std::max(bytesBuilding, bytesBuildingTop);
	bytesBuilding = bytesBuildingTop = 0;
	ready = true;
	return 0;
}

// Walks the edge from a to b in steps, and at each step looks in every box within kAlienWaysTouch of it for another polygon's corner lying on the
// edge. A box the step before already looked in is skipped, so each box along the edge is looked in about once.
const char *AlienWaysMap::JoinEdge(int polygon, AlienWaysXYZ a, AlienWaysXYZ b)
{
	// A kept edge is at most about 222 km long, so steps is at most about 443,000, and the sum below can't wrap round
	size_t const steps = std::max<size_t>(1, static_cast<size_t>(ceil(Distance(a, b) / kEdgeStep)));

	if (steps + 1 > limits.mostEdgeSteps - edgeSteps)
	{
		return kWhyEdgeSteps;
	}

	edgeSteps += steps + 1;

	long long before[8];
	int beforeCount = 0;

	for (size_t s = 0; s <= steps; s++)
	{
		double const t = static_cast<double>(s) / static_cast<double>(steps);
		double const x = a.x + (static_cast<double>(b.x) - a.x) * t;
		double const y = a.y + (static_cast<double>(b.y) - a.y) * t;
		double const z = a.z + (static_cast<double>(b.z) - a.z) * t;

		// The boxes within kAlienWaysTouch of this step: the one it's in, and any a corner of a 20 mm cube round it reaches into
		long long now[8];
		int nowCount = 0;

		for (int corner = 0; corner < 8; corner++)
		{
			double const ox = (corner & 1) ? kAlienWaysTouch : -kAlienWaysTouch;
			double const oy = (corner & 2) ? kAlienWaysTouch : -kAlienWaysTouch;
			double const oz = (corner & 4) ? kAlienWaysTouch : -kAlienWaysTouch;
			long long const box = KeyOf(BoxOf(x + ox), BoxOf(y + oy), BoxOf(z + oz));
			bool already = false;

			for (int k = 0; k < nowCount && !already; k++)
			{
				already = now[k] == box;
			}

			if (!already)
			{
				now[nowCount++] = box;
			}
		}

		for (int k = 0; k < nowCount; k++)
		{
			bool lookedBefore = false;

			for (int q = 0; q < beforeCount && !lookedBefore; q++)
			{
				lookedBefore = before[q] == now[k];
			}

			if (lookedBefore)
			{
				continue;
			}

			Corner const key = { now[k], 0, 0, 0, 0 };
			auto const range = std::equal_range(buckets.begin(), buckets.end(), key, [](const Corner &l, const Corner &r) { return l.box < r.box; });

			for (auto e = range.first; e != range.second; ++e)
			{
				if (e->polygon == polygon)
				{
					continue;
				}

				if (cornerTests >= limits.mostCornerTests)
				{
					return kWhyCornerTests;
				}

				cornerTests++;
				AlienWaysXYZ const c = { e->x, e->y, e->z };

				if (DistanceToEdge(a, b, c) > kAlienWaysTouch)
				{
					continue;
				}

				// Room for more touches is counted against the ceiling before it's asked for. While the touches are copied into the bigger room,
				// the old room is still held as well, so that top is checked against the ceiling too, and kept as the most the build needed.
				if (touches.size() == touches.capacity())
				{
					size_t const more = std::max<size_t>(1024, touches.capacity());
					size_t const old = touches.capacity() * sizeof(Touch);

					if (!Count(more * sizeof(Touch), &bytesBuilding, limits.mostBytesBuilding) || old > limits.mostBytesBuilding - bytesBuilding)
					{
						return kWhyBuilding;
					}

					bytesBuildingTop = std::max(bytesBuildingTop, bytesBuilding + old);
					touches.reserve(touches.capacity() + more);
				}

				Touch const touch = { std::min(polygon, e->polygon), std::max(polygon, e->polygon), e->x, e->y, e->z };
				touches.push_back(touch);
			}
		}

		for (int k = 0; k < nowCount; k++)
		{
			before[k] = now[k];
		}

		beforeCount = nowCount;
	}

	return 0;
}

// What a metre on a node costs: a floor's at the search's price, and anything else 1. The goal node is never left, so its price never counts.
// A point on a wall polygon, moved straight up to kAlienWaysClimbHeight above the wall's lowest point, or to the wall's top if that's lower.
// A point that's already that high, or on a polygon that isn't a wall, is left as it is. y goes down, so up is a smaller y.
AlienWaysXYZ AlienWaysMap::LiftOnWall(int node, AlienWaysXYZ p) const
{
	if (kind[node] != kAlienWaysWall)
	{
		return p;
	}

	long long const wanted = std::max(static_cast<long long>(boxLow[node].y), static_cast<long long>(boxHigh[node].y) - kAlienWaysClimbHeight);

	if (p.y > wanted)
	{
		p.y = static_cast<int>(wanted);
	}

	return p;
}

// The points a way has on polygon node, crossing it from one point to another: where it came on, and on a wall, where it has climbed to
// and where it starts down towards where it leaves, each only if it's somewhere new. Up to 3, written into out if it's given.
size_t AlienWaysMap::PointsAcross(int node, AlienWaysXYZ from, AlienWaysXYZ to, AlienWaysXYZ *out) const
{
	AlienWaysXYZ points[3];
	size_t count = 0;
	points[count++] = from;

	if (kind[node] == kAlienWaysWall)
	{
		AlienWaysXYZ const up = LiftOnWall(node, from), down = LiftOnWall(node, to);
		bool const upIsNew = up.x != from.x || up.y != from.y || up.z != from.z;
		bool const downIsNew = (down.x != up.x || down.y != up.y || down.z != up.z) && (down.x != to.x || down.y != to.y || down.z != to.z);

		if (upIsNew)
		{
			points[count++] = up;
		}

		if (downIsNew)
		{
			points[count++] = down;
		}
	}

	if (out)
	{
		for (size_t i = 0; i < count; i++)
		{
			out[i] = points[i];
		}
	}

	return count;
}

// What crossing polygon node from one point to another costs: the length of the way across it, up and down a wall included, at the polygon's
// price. A floor stepped onto from a wall or a ceiling costs at least kAlienWaysLeastFloorStep of it.
double AlienWaysMap::CostAcross(int node, AlienWaysXYZ from, AlienWaysXYZ to) const
{
	AlienWaysXYZ points[3];
	size_t const count = PointsAcross(node, from, to, points);
	double length = 0.0;

	for (size_t i = 0; i < count; i++)
	{
		length += Distance(points[i], i + 1 < count ? points[i + 1] : to);
	}

	int const before = cameFrom[node];

	if (kind[node] == kAlienWaysFloor && before >= 0 && kind[before] != kAlienWaysFloor)
	{
		length = std::max(length, static_cast<double>(kAlienWaysLeastFloorStep));
	}

	return length * PriceOf(node);
}

double AlienWaysMap::PriceOf(int node) const
{
	if (node < static_cast<int>(Polygons()) && kind[node] == kAlienWaysFloor)
	{
		return static_cast<double>(price);
	}

	return 1.0;
}

const char *AlienWaysMap::StartSearch(int fromPolygon, AlienWaysXYZ start, int toPolygon, AlienWaysXYZ goal, int floorPrice)
{
	if (!ready)
	{
		return kWhyNoMap;
	}

	int const onTheMap = static_cast<int>(Polygons());

	if (fromPolygon < 0 || toPolygon < 0 || fromPolygon >= onTheMap || toPolygon >= onTheMap)
	{
		return kWhyNoPolygon;
	}

	if (floorPrice < 1 || floorPrice > kAlienWaysMostFloorPrice)
	{
		return kWhyPrice;
	}

	if (FarOut(start.x) || FarOut(start.y) || FarOut(start.z) || FarOut(goal.x) || FarOut(goal.y) || FarOut(goal.z))
	{
		return kWhyPoint;
	}

	// A new number for this search, so nothing from the last one needs clearing. If the number has gone all the way round to 0, an old stamp
	// could look current, so every stamp is cleared, once in four thousand million searches.
	searchNumber++;

	if (searchNumber == 0)
	{
		std::fill(stamp.begin(), stamp.end(), 0);
		std::fill(closed.begin(), closed.end(), 0);
		searchNumber = 1;
	}

	heapSize = 0;
	looks = 0;
	way.clear();
	cutShort = false;
	price = floorPrice;
	fromNode = fromPolygon;
	toNode = toPolygon;
	goalAt = goal;
	state = kAlienWaysSearching;
	Relax(fromNode, 0.0, start, -1);
	return 0;
}

// A way to node has been found that costs costHere and arrives at the point at, from the node from. It's kept if it's the first or the cheapest yet.
void AlienWaysMap::Relax(int node, double costHere, AlienWaysXYZ at, int from)
{
	if (stamp[node] != searchNumber)
	{
		stamp[node] = searchNumber;
		heapPlace[node] = -1;
	}
	else if (costHere >= cost[node])
	{
		return;
	}

	cost[node] = costHere;
	cameFrom[node] = from;
	entry[node] = at;

	// The guess of the whole way's cost: so far, plus the straight line to the goal. Nothing costs less than 1 a millimetre, so the guess is never
	// more than the real cost, and the first way found to the goal is the cheapest the map can see: straight lines from join to join, with the
	// floor at its price. That is not always the cheapest way an alien could really walk.
	guess[node] = costHere + (node == static_cast<int>(Polygons()) ? 0.0 : Distance(at, goalAt));

	if (heapPlace[node] < 0)
	{
		heap[heapSize] = node;
		heapPlace[node] = heapSize;
		heapSize++;
	}

	HeapUp(heapPlace[node]);
}

AlienWaysSearch AlienWaysMap::Work(int polygonsAllowed)
{
	if (state != kAlienWaysSearching)
	{
		return state;
	}

	int const goalNode = static_cast<int>(Polygons());

	while (polygonsAllowed > 0 && heapSize > 0)
	{
		polygonsAllowed--;
		int const node = HeapPop();
		closed[node] = searchNumber;
		looks++;

		if (node == goalNode)
		{
			MakeTheWay();
			state = kAlienWaysFound;
			return state;
		}

		double const here = cost[node];
		AlienWaysXYZ const at = entry[node];

		// On the goal's own polygon, the last stretch is to the goal itself
		if (node == toNode)
		{
			Relax(goalNode, here + CostAcross(node, at, goalAt), goalAt, node);
		}

		for (int k = firstJoin[node]; k < firstJoin[node + 1]; k++)
		{
			int const other = joinOther[k];

			if (closed[other] == searchNumber)
			{
				continue;
			}

			// It crosses where the stretch the two share is nearest to where it came onto this polygon. From one wall onto the next, it
			// crosses no lower than kAlienWaysClimbHeight up that stretch, so a way along walls goes up them, not along their foot.
			AlienWaysXYZ cross = NearestOn(stretchA[joinStretch[k]], stretchB[joinStretch[k]], at);

			if (kind[node] == kAlienWaysWall && kind[other] == kAlienWaysWall)
			{
				cross = LiftedOn(stretchA[joinStretch[k]], stretchB[joinStretch[k]], cross);
			}

			Relax(other, here + CostAcross(node, at, cross), cross, node);
		}
	}

	if (heapSize == 0)
	{
		state = kAlienWaysNoWay;
	}

	return state;
}

// The way, from the start to the goal: point 0 is the start, then for each polygon it crosses, where it came onto it, and on a wall, where it
// has climbed to and where it starts down (PointsAcross); the last point is the goal. The points are counted first, then written from the
// goal back. It's written straight into the list made when the map was built, so this asks for no memory.
void AlienWaysMap::MakeTheWay()
{
	int const goalNode = static_cast<int>(Polygons());
	size_t points = 1, crossed = 0;
	AlienWaysXYZ leaving = goalAt;

	for (int node = cameFrom[goalNode]; node >= 0 && crossed <= Polygons(); node = cameFrom[node], crossed++)
	{
		points += PointsAcross(node, entry[node], leaving, nullptr);
		leaving = entry[node];
	}

	size_t const keep = std::min(points, kAlienWaysMostWayPoints);
	cutShort = points > kAlienWaysMostWayPoints;
	way.resize(keep);

	size_t place = points - 1;

	if (place < keep)
	{
		way[place] = AlienWaysPoint{ goalAt.x, goalAt.y, goalAt.z, KindOf(toNode) };
	}

	leaving = goalAt;
	crossed = 0;

	for (int node = cameFrom[goalNode]; node >= 0 && crossed <= Polygons(); node = cameFrom[node], crossed++)
	{
		AlienWaysXYZ across[3];
		size_t const count = PointsAcross(node, entry[node], leaving, across);

		for (size_t i = count; i > 0 && place > 0; i--)
		{
			place--;

			if (place < keep)
			{
				AlienWaysXYZ const at = across[i - 1];
				way[place] = AlienWaysPoint{ at.x, at.y, at.z, KindOf(node) };
			}
		}

		leaving = entry[node];
	}
}

void AlienWaysMap::HeapUp(int place)
{
	int const node = heap[place];
	double const key = guess[node];

	while (place > 0)
	{
		int const up = (place - 1) / 2;

		if (guess[heap[up]] <= key)
		{
			break;
		}

		heap[place] = heap[up];
		heapPlace[heap[place]] = place;
		place = up;
	}

	heap[place] = node;
	heapPlace[node] = place;
}

void AlienWaysMap::HeapDown(int place)
{
	int const node = heap[place];
	double const key = guess[node];

	for (;;)
	{
		int child = 2 * place + 1;

		if (child >= heapSize)
		{
			break;
		}

		if (child + 1 < heapSize && guess[heap[child + 1]] < guess[heap[child]])
		{
			child++;
		}

		if (guess[heap[child]] >= key)
		{
			break;
		}

		heap[place] = heap[child];
		heapPlace[heap[place]] = place;
		place = child;
	}

	heap[place] = node;
	heapPlace[node] = place;
}

int AlienWaysMap::HeapPop()
{
	int const top = heap[0];
	heapSize--;
	heapPlace[top] = -1;

	if (heapSize > 0)
	{
		heap[0] = heap[heapSize];
		heapPlace[heap[0]] = 0;
		HeapDown(0);
	}

	return top;
}
