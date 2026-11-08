// A map of a level's surfaces for an alien, and a search across it that can make the floor dear: "floor is lava", issue #109.
// Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY. Aliens can run on walls and ceilings, but nothing in the game plans a way over them. Today an alien climbs only when it happens to run
// into a wall while it's allowed to (AlienNearState_Approach, bh_near.cpp). This map lets an alien ask for a way that keeps off the floor where
// it can. Nothing in the game uses it yet: that's the next step, after review.
//
// WHAT THE MAP IS. Every polygon an alien could cling to is a place on the map. Each one is a FLOOR, a WALL or a CEILING, by which way it faces,
// using the game's own floor angle (FLOOR_THRESHOLD in dynamics.cpp: facing up by at least 30000 of 65536). Two polygons are JOINED where they
// touch: where a corner of one lies within 10 mm of a corner or an edge of the other. The edge part matters, because a long wall's bottom edge
// often runs past the corners of several floor tiles. Each join keeps the STRETCH along which the two touch, so a way can cross at the point
// of it nearest to where it is.
//
//      wall  +----------------------+          the wall's bottom edge, and each floor tile's edge along it,
//            |                      |          touch along a stretch:  ===
//            +===+===+===+===+===+==+
//            |   |   |   |   |   |  |  floor tiles
//            +---+---+---+---+---+--+
//
// A WAY. A search over the polygons, from the polygon under the start to the polygon under the goal. Each polygon is reached at a point: the
// start, for the first, and otherwise where the way came onto it. Going on from there costs the distance to where the way leaves, times that
// polygon's price: 1 for a wall or a ceiling, and for a floor whatever price the search is given. With a price of 10, a metre of floor costs
// as much as ten metres of wall, so the way goes up the wall wherever that's less than ten times as far. The goal is reached the same way,
// from its own polygon, so the way also leaves the start by its nearest wall.
//
//        cheap floor (price 1):   A ------------------------- B          all floor
//        floor is lava (10):      A                           B
//                                  \_________________________/            up the wall, along it, and back down
//
// WHAT IT DOESN'T KNOW: the alien's width, jumps across air, doors that are shut, and lifts. A way here means the surfaces join up, not that an
// alien fits. The game's physics decides the rest.
//
// HOW IT WAS CHOSEN. Measured with no game running on the 32 stock levels (AvP-docs, engine/pathfinding-study/alien-ways.js). On 1,636 route
// legs, a floor price of 10 left 79% less floor between a leg's ends than price 1, with ways 41% longer, and past 10 almost no more floor was
// left behind. The map keeps 4.6 MB on an average level and 8 MB at most. A grid of every surface would have been 251 MB on average.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It's handed a plain list of polygons, in the game's numbers (millimetres, and y goes down), so a test can
// make up a small world and check every answer.
//
// HOW MUCH THIS CAN TAKE. The polygons come from a level file, which comes from outside the game (AvP Tao #3). So every count has a limit, and
// memory has two ceilings: one for what the map keeps, and one for while it's being built. Over any of them, Build gives back why, in words,
// and keeps no map at all. Some polygons are left out without refusing the level, because a normal level has them: the sky, and the fake view
// outside a space station, which are far too big to matter to an alien. Each limit is well above what the stock levels need.
#ifndef ALIEN_WAYS_MAP_H
#define ALIEN_WAYS_MAP_H

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <vector>

// A polygon has from 3 to this many corners. The game's own polygons have 3 or 4.
int const kAlienWaysMostCorners = 8;

// Two polygons touch if a corner of one is within this many millimetres of the other's corner or edge.
int const kAlienWaysTouch = 10;

// What counts as a floor or a ceiling: how much a polygon faces up or down, out of 1. This is the game's FLOOR_THRESHOLD.
float const kAlienWaysFloorFacing = 30000.0f / 65536.0f;

// Left out, without refusing the level, because they're scenery:
long long const kAlienWaysMostFlatArea = 5000000000LL;  // a polygon bigger than 5,000 square metres, seen from above: a sky
int const kAlienWaysMostWallHeight = 50000;             // a wall taller than 50 m: stock's biggest are 549 m tall, the view outside on stat101 and breakout
int const kAlienWaysMostCoordinate = 64000000;          // a corner more than 64 km from the middle of the world

// What a search may be asked for
int const kAlienWaysMostFloorPrice = 1000;
size_t const kAlienWaysMostWayPoints = 256;             // a longer way is cut short there, and says so

// THE LIMITS. What the stock levels need is in brackets: the most on any of the 32, measured with no game running (AvP-docs,
// engine/pathfinding-study). The ceilings in bytes stop a level that's too big. The counts of work stop one that would take too long: corners
// piled into one place make the joining slow even when there's memory for it, so that's limited twice, by how many share a place and by the
// work in all.
struct AlienWaysLimits
{
	size_t mostPolygons;            // 200,000 (39,354)
	size_t mostCornersInOnePlace;   // 4,096 corners in one of the metre boxes the joining sorts them into (1,004, on temple_m)
	size_t mostEdgeSteps;           // 10,000,000: each edge is walked in steps of half a metre, to find what touches it (742,611, on temple)
	size_t mostCornerTests;         // 100,000,000: how many times a corner is tested against an edge, in all (10,634,886, on temple_m)
	size_t mostJoins;               // 1,000,000 (189,256)
	size_t mostBytesKept;           // 32 MB (8 MB): everything the map keeps, its search's room included
	size_t mostBytesBuilding;       // 64 MB: the lists it makes while building, which it lets go of when it's done
};

AlienWaysLimits AlienWays_UsualLimits();

// One polygon, in the game's numbers: millimetres, and y going down.
struct AlienWaysPolygon
{
	int corners;                           // how many are used, from 3 to kAlienWaysMostCorners
	int x[kAlienWaysMostCorners];
	int y[kAlienWaysMostCorners];
	int z[kAlienWaysMostCorners];
	float up;                              // how much it faces up (towards smaller y), from 1 (flat, facing up) to -1 (facing down)
};

enum AlienWaysKind
{
	kAlienWaysFloor,
	kAlienWaysWall,
	kAlienWaysCeiling
};

// Which kind a polygon is, by how much it faces up
AlienWaysKind AlienWays_KindOf(float up);

// A game polygon's list of corners ends at a negative number. This adds one to a list for the map, and never cuts one down: with 3 to
// kAlienWaysMostCorners corners it goes in as it is, and with fewer or more it's left out, and the answer says which. A level file's polygons
// have at most 4 corners (its polygon records have room for 4), so one with more than 8 can only be a broken file.
enum AlienWaysAdded
{
	kAlienWaysAddedAsItIs,
	kAlienWaysLeftOutTooFewCorners,
	kAlienWaysLeftOutTooManyCorners
};

AlienWaysAdded AlienWays_AddPolygon(const int *x, const int *y, const int *z, int corners, float up, std::vector<AlienWaysPolygon> *list);

// How much a polygon faces up, worked out from all its corners, for a polygon with no normal stored with it. It has to be all of them: a level's
// polygon can begin with three corners in a line. Which way round the corners go was measured on the stock levels, not worked out from first
// principles (AvP-docs, engine/pathfinding-study/level.js). The game's own stored normal is better, where a polygon has one.
float AlienWays_HowMuchItFacesUp(const AlienWaysPolygon &polygon);

struct AlienWaysXYZ
{
	int x, y, z;
};

// One point of a way. kind is what the way is on from this point to the next.
struct AlienWaysPoint
{
	int x, y, z;
	AlienWaysKind kind;
};

enum AlienWaysSearch
{
	kAlienWaysSearching,   // not finished: call Work again
	kAlienWaysFound,       // Way goes all the way there, or the first kAlienWaysMostWayPoints points of it
	kAlienWaysNoWay        // the surfaces don't join up between the two
};

class AlienWaysMap
{
public:
	AlienWaysMap();

	// Lets go of everything, and leaves no map. A stop that was asked for is forgotten too, ready for the next level.
	void Clear();

	// Works out the whole map. It gives back nothing if the map's ready, or why there's no map, in words, and then nothing is kept.
	const char *Build(const std::vector<AlienWaysPolygon> &polygons);

	// Asks a build that's running on another thread to stop: the level is ending, or the game is closing. Every loop of the build looks at this,
	// so it stops soon after, says why, and keeps nothing. Asked before Build starts, Build stops at once. Only Clear forgets it.
	void AskToStop() { stopAsked.store(true); }

	bool Ready() const { return ready; }

	// Lowers the limits, so a test can try each one without hundreds of thousands of polygons. Nothing else calls this.
	void LowerLimitsForATest(const AlienWaysLimits &lower) { limits = lower; }

	// So a test can stop a build part way with no second thread: the build asks itself to stop at its nth look at the stop. 0 means never.
	// Nothing else calls this.
	void StopAtLookForATest(size_t look) { stopAtLook = look; }

	// The map's polygons are numbered in the order they were handed over, leaving out the ones left out. This says which number polygon i of the
	// list given to Build became, or -1 if it was left out.
	int MapNumberOf(size_t given) const;

	size_t Polygons() const { return kind.size(); }
	size_t Joins() const { return stretchA.size(); }
	size_t PolygonsLeftOut() const { return leftOut; }
	size_t BytesKept() const { return bytesKept; }
	size_t BytesWhileBuilding() const { return bytesBuildingUsed; }   // the most the last build that worked counted against its building ceiling
	size_t EdgeSteps() const { return edgeSteps; }
	size_t CornerTests() const { return cornerTests; }
	AlienWaysKind KindOf(int polygon) const { return static_cast<AlienWaysKind>(kind[polygon]); }

	// Whether two polygons of the map touch, and if so the stretch where they do.
	bool Joined(int a, int b, AlienWaysXYZ *from, AlienWaysXYZ *to) const;

	// Starts a search for a way from start, on polygon fromPolygon, to goal, on polygon toPolygon, with the floor at floorPrice times a wall's
	// price. It gives back nothing if it started, or why not, in words. A search that's running is dropped.
	const char *StartSearch(int fromPolygon, AlienWaysXYZ start, int toPolygon, AlienWaysXYZ goal, int floorPrice);

	// Carries on, looking at up to this many polygons, and says how it stands. Asked again after it's ended, it gives the same answer.
	AlienWaysSearch Work(int polygonsAllowed);

	// The way the last search found, from the start through every point where it crosses from one polygon to the next, to the goal. Empty if there
	// was no way.
	const std::vector<AlienWaysPoint> &Way() const { return way; }

	// Whether the way was longer than kAlienWaysMostWayPoints points and was cut short. Then it doesn't reach the goal, and needs asking again.
	bool WayCutShort() const { return cutShort; }

	int LastSearchLooks() const { return looks; }

	// So a test can try what happens when the search number goes all the way round. Nothing else calls this.
	void SetSearchNumberForATest(uint32_t number) { searchNumber = number; }

private:
	const char *BuildIt(const std::vector<AlienWaysPolygon> &polygons);
	void LetGo();
	bool Stopping();
	bool Count(size_t bytes, size_t *total, size_t most);
	const char *JoinEdge(int polygon, AlienWaysXYZ a, AlienWaysXYZ b);
	double PriceOf(int node) const;
	void Relax(int node, double costHere, AlienWaysXYZ at, int from);
	void HeapUp(int place);
	void HeapDown(int place);
	int HeapPop();
	void MakeTheWay();

	bool ready;
	AlienWaysLimits limits;
	std::atomic<bool> stopAsked;
	size_t stopAtLook, stopLooks;
	size_t leftOut, bytesKept, bytesBuilding, bytesBuildingUsed, edgeSteps, cornerTests;

	// Which map number each polygon handed to Build became, or -1
	std::vector<int> mapNumber;

	// Kept for searching. Polygon p's joins are joinOther and joinStretch from firstJoin[p] up to firstJoin[p + 1].
	std::vector<int> middleX, middleY, middleZ;
	std::vector<uint8_t> kind;
	std::vector<int> firstJoin;
	std::vector<int> joinOther, joinStretch;
	std::vector<AlienWaysXYZ> stretchA, stretchB;

	// Only while building: every corner of every polygon kept, and where each polygon's start in that list, and what touches what.
	// A corner, by the metre box it's in:
	struct Corner
	{
		long long box;
		int polygon;
		int x, y, z;
	};

	// One place where a corner of one polygon lies on another. low and high are the two polygons, the smaller number first.
	struct Touch
	{
		int low, high;
		int x, y, z;
	};

	std::vector<int> cornerX, cornerY, cornerZ;
	std::vector<int> firstCorner;
	std::vector<Corner> buckets;
	std::vector<Touch> touches;

	// One search. A polygon's numbers here only mean something if its stamp is this search's number, so nothing has to be cleared between searches.
	// The goal is one more node, numbered Polygons(), so the last stretch to it is chosen like any other.
	std::vector<double> cost, guess;
	std::vector<int> cameFrom;
	std::vector<AlienWaysXYZ> entry;
	std::vector<uint32_t> stamp, closed;
	std::vector<int> heap, heapPlace;
	int heapSize;
	uint32_t searchNumber;
	AlienWaysSearch state;
	int fromNode, toNode, price;
	AlienWaysXYZ goalAt;
	int looks;
	bool cutShort;
	std::vector<AlienWaysPoint> way;
};

#endif
