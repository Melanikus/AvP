// A 3D grid of a whole level, for finding a marine's way through it. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY. Hudson's first map was made by feeling about with rays, a stride at a time, which only works in rooms the game has loaded and costs a lot of rays. This one is worked
// out once, from the level's own polygons. Tried on the stock levels with no game running, it found 93.6% of the steps on the marine route lists (AvP-docs, engine/pathfinding-study).
//
// WHAT THE GRID IS. Seen from above, the level is cut into columns 300 mm square. Each column keeps a list of the heights where polygons pass through it, and between those
// is empty air. So a whole level's empty space costs nothing, but it's still truly 3D: one column can hold a floor, the floor above it, and the one above that. One place he can stand is
// one column and one height. That's called a PLACE here.
//
// HE FITS IN A PLACE if nothing that blocks him lies in a square 450 mm either way around him, from 450 mm above his feet up to his height. His box is square and doesn't turn
// (extents.cpp, CE_MARINE), so that's his real shape. 450 is the most the game lifts him up a step (MAXIMUM_STEP_HEIGHT), so anything lower he walks up, and isn't in his way.
// He's 1950 tall, or 1200 in a room marked as an air duct, where he crawls by himself. 300 mm columns suit this: 450 is exactly one and a half columns.
//
// HE CAN MOVE to a neighbouring column if he doesn't rise or fall by more than 450. Going diagonally, the two columns beside it have to be open as well, or his square would catch a corner.
// A STEEP SLOPE ONLY GOES DOWN. The game calls a slope floor up to about 63 degrees, but on Sulaco, 2026-11-05, he stood pushing against one of about 46 degrees and couldn't climb it. So anything
// steeper than the game's "nearly flat" (about 24 degrees) can be walked down and not up. That's one slope measured, and where the real limit lies between 24 and 46 is still to be found.
// HE CAN DROP one way, if it's under 8 m, which is what a marine survives. He's a square, so he doesn't fall until his whole square is past the edge. So a drop lands two columns out.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It's handed a list of polygons, so a test can make up a small world and check every answer, and hudson_grid_level.cpp hands it the real ones.
//
// HOW MUCH THIS CAN TAKE. A level file can claim anything, so every count has a limit, and Build says why it refused rather than running out of memory. See the limits below.
// Each one is well above what the stock levels need.
#ifndef HUDSON_GRID_H
#define HUDSON_GRID_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

int const kHudsonGridCell = 300;
int const kHudsonGridMarineHalfWidth = 450;
int const kHudsonGridMarineHeight = 1950;
int const kHudsonGridCrawlHeight = 1200;      // his box's top in a room marked as an air duct: MarineShouldBeCrawling, in bh_marine.cpp
int const kHudsonGridStepUp = 450;            // MAXIMUM_STEP_HEIGHT
int const kHudsonGridMostFall = 8000;         // Marine_Terminal_Velocity: a fall of more than this kills him

// What counts as floor: how much a polygon has to face up, out of 1. This is the game's FLOOR_THRESHOLD, and its nearly flat one.
float const kHudsonGridFloorFacing = 30000.0f / 65536.0f;
float const kHudsonGridNearlyFlatFacing = 60000.0f / 65536.0f;

int const kHudsonGridMostCorners = 8;

// THE LIMITS. What a stock level needs is in brackets: the most on any of the 32.
size_t const kHudsonGridMostPolygons = 2000000;       // (40 000)
int64_t const kHudsonGridMostPolygonArea = 5000000000LL;  // how big a polygon can be, across its box, in square millimetres: 5000 square metres. Bigger is a sky, or the fake outside of a space
                                                      // station (stat101 has them over 200 m across). It's left out, not refused, because a level with a sky is a normal level.
int const kHudsonGridMostCoordinate = 64000000;       // 64 km from the middle of the world. Anything further is left out the same way
int const kHudsonGridMostSide = 32768;                // columns along one side, which is 9.8 km
size_t const kHudsonGridMostBits = 8000000;           // polygons crossing columns, before they're joined up. 16 bytes each, while it's building
size_t const kHudsonGridMostPlaces = 2000000;         // (about 800 000) about 60 bytes each, so about 120 MB at the very most
size_t const kHudsonGridMostMoves = 20000000;         // 4 bytes each
int const kHudsonGridSpotReach = 3000;                // how far sideways a spot can be from the floor that's taken for it
int const kHudsonGridSpotAbove = 30000;               // how far a spot can be above its floor: a join between rooms can be well up in the air
int const kHudsonGridSpotBelow = 3000;                // and how far below it

// One polygon, in the world's numbers: millimetres, and y goes down, as in the game.
struct HudsonGridPolygon
{
	int corners;                         // how many are used, from 3 to kHudsonGridMostCorners
	int x[kHudsonGridMostCorners];
	int y[kHudsonGridMostCorners];
	int z[kHudsonGridMostCorners];
	float up;                            // how much it faces up, from 1, flat and facing up, to -1, facing down. See HudsonGrid_HowMuchItFacesUp
	bool blocks;                         // is it in his way? A lift's car isn't, because it won't be where the file has it
	bool canStandOn;                     // can it be a floor at all, however it faces?
	bool crawl;                          // it's in a room marked as an air duct
};

// How much a polygon faces up, worked out from all its corners. It has to be all of them: polygons in these levels can begin with three corners in a line, and a floor on Derelict does.
// Which way round the corners go was measured on the stock levels, not worked out from first principles. The game's own stored normal is better, where a polygon has one.
float HudsonGrid_HowMuchItFacesUp(const HudsonGridPolygon &polygon);

enum HudsonGridSearch
{
	kHudsonGridSearching,    // not finished: call Work again
	kHudsonGridFound,        // Way goes all the way there
	kHudsonGridNoWay         // there's none. Way goes as near as it got
};

struct HudsonGridPoint
{
	int x, y, z;             // on the floor, in the world's numbers
};

// What one place is, for anything drawing the map.
enum HudsonGridKind
{
	kHudsonGridFlat,
	kHudsonGridSlope,
	kHudsonGridStair,        // walking from here rises or falls by a step
	kHudsonGridDrop          // he can drop from here. link says where he lands
};

struct HudsonGridCell
{
	int x, y, z;             // the middle of its column, on the floor
	HudsonGridKind kind;
	int link;                // for a drop, the number of the place he lands on, which may not be in this list. Otherwise -1
	int number;              // this place's own number
};

class HudsonGrid
{
public:
	HudsonGrid();

	void Clear();

	// Works out the whole map. This is the slow part: seconds, on a big level. It gives back nothing if the map's ready, or why there's no map, in words. If there's no map, nothing is left of it.
	const char *Build(const std::vector<HudsonGridPolygon> &polygons);

	bool Ready() const { return ready; }

	// Lowers the limits, so that a test can try each one without millions of polygons. Nothing else calls this.
	void LowerLimitsForATest(size_t mostPolygons, size_t mostBits, size_t mostPlaces, size_t mostMoves);

	// Starts looking for a way between two places. They don't have to be exactly on a floor: the nearest suitable floor is taken for each. toY can be in mid-air.
	void StartSearch(int fromX, int fromY, int fromZ, int toX, int toY, int toZ);

	// Carries on looking at up to this many places, and says how it stands. Asked again after it's ended, it gives the same answer.
	HudsonGridSearch Work(int placesAllowed);

	// The way the last search found, about a point a metre, from where it started to where it was going, or as near as it got. A search that ended with no way still has as much of one as it found.
	const std::vector<HudsonGridPoint> &Way() const { return way; }

	// The floor it took for where it's going, in the world's numbers. Only means anything if GoalWasFound is true.
	int GoalFloorY() const { return goalFloorY; }
	bool GoalWasFound() const { return goalWasFound; }
	bool StartWasFound() const { return startWasFound; }

	int LastSearchLooks() const { return looks; }

	// Marks every place that can be walked to from a point, with the same moves a search uses, drops and all: reached gets one entry for each place, 1 for each one he can get to. False if the
	// point has no floor near it. It looks at each place once, so it takes about as long as a search that finds no way. A lift uses this, to find which floors each of its ends can be walked to from.
	bool MarkWhatCanBeReachedFrom(int x, int y, int z, std::vector<uint8_t> *reached) const;

	// Is a point one of the places marked? The point is taken the way a search takes where it's going, so a spot in the air counts as the floor under it.
	bool IsMarked(const std::vector<uint8_t> &marks, int x, int y, int z) const;

	// The places near a point, up to most of them. It gives back how many it wrote. This is what a drawing of the map reads, and it never looks further than radius, so it can't take long.
	int CellsNear(int x, int y, int z, int radius, HudsonGridCell *cells, int most) const;

	// One place, by its number, up to Places(). False if there's no such place.
	bool CellAt(int number, HudsonGridCell *cell) const;

	size_t Places() const { return placeHeight.size(); }
	size_t Moves() const { return moves.size(); }
	size_t Drops() const { return drops; }
	size_t PolygonsLeftOut() const { return polygonsLeftOut; }
	size_t PolygonsUsed() const { return polygonsUsed; }
	size_t BitsBeforeJoining() const { return bitsBeforeJoining; }
	size_t BytesKept() const;

private:
	struct Bit;

	// One stretch of height in one column where polygons are. Above it, up to the next one, is air.
	struct Span
	{
		int low, high;
		uint8_t flags;
	};


	bool MarkOnePolygon(const HudsonGridPolygon &polygon, std::vector<Bit> *bits);
	void JoinBitsIntoSpans(std::vector<Bit> *bits);
	int ColumnAt(int ix, int iz) const;
	bool InTheWay(int ix, int iz, int fromHeight, int toHeight) const;
	bool CrawlingHere(int ix, int iz, int height) const;
	bool MakePlaces();
	bool MakeMoves();
	bool WalkableInto(int ix, int iz, int height) const;
	void WorkOutHowFarFromWalls();
	void WorkOutWhichPatchesAreSmall();
	void PlacesNearSpot(int x, int y, int z, std::vector<int> *found, int *floorY) const;
	void FillCell(int place, HudsonGridCell *cell) const;
	float HowFarToGoal(int place) const;
	void MakeTheWay(int lastPlace, bool arrived);
	void Push(float cost, int place);
	int Pop();
	int WorldX(int ix) const { return minX + ix * kHudsonGridCell + kHudsonGridCell / 2; }
	int WorldZ(int iz) const { return minZ + iz * kHudsonGridCell + kHudsonGridCell / 2; }

	bool ready;
	int minX, minZ, width, depth;
	size_t limitPolygons, limitBits, limitPlaces, limitMoves;

	// While building. Each column that has anything in it, sorted by row then along the row. rowStart says where each row begins in that list.
	std::vector<int> rowStart;
	std::vector<uint16_t> columnX;
	std::vector<int> columnFirstSpan;
	std::vector<Span> spans;

	// Kept for searching. A place is one column at one height, numbered in column order, so that a column's places are next to each other.
	std::vector<int> columnFirstPlace;
	std::vector<uint16_t> placeX, placeZ;
	std::vector<int> placeHeight;         // going up, in millimetres: minus the game's y
	std::vector<uint8_t> placeFlat;
	std::vector<int> firstMove;
	std::vector<uint32_t> moves;          // each is a place's number times 2, plus 1 if it's a drop
	std::vector<uint8_t> fromWall;        // how many moves he is from a place with a way blocked, up to 255
	std::vector<uint8_t> smallPatch;      // the floor this place is on is too small to count as anywhere: the top of a lamp, say
	size_t drops, polygonsLeftOut, polygonsUsed, bitsBeforeJoining;

	// One search
	std::vector<int> seen;
	std::vector<float> costSoFar;
	std::vector<int> cameFrom;
	std::vector<float> heapCost;
	std::vector<int> heapPlace;
	std::vector<int> goals;
	std::vector<int> goalMark;
	int searchNumber;
	HudsonGridSearch state;
	int goalX, goalZ;
	int nearestPlace;
	float nearestDistance;
	int looks;
	bool goalWasFound, startWasFound;
	int goalFloorY;
	int goalAskedX, goalAskedZ;
	std::vector<HudsonGridPoint> way;
};

#endif
