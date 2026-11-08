// A grid on every surface of a level, for every kind of NPC to find its way on: issue #145, step 1.
// Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY. The user, 2026-11-08: "I want Hudson AI's grid unified and available for all NPCs to pathfind into, not a special alien pathfinding."
// Hudson's grid (hudson_grid.h, kept local) knows only floors, and alien ways (alien_ways_map.h) knows only whole polygons, which can be
// metres across: that gives the long straight lines and corner-to-corner turns the user saw. This puts small cells on every floor, wall and
// ceiling, so a marine, an alien on a wall and every other kind can search the same grid, each with its own body.
//
// WHAT THE GRID IS.
//   A SHEET is a flat piece of the level: polygons the alien ways map joins edge to edge, that face the same way (within kNpcGridSameFacing)
//   and lie in one plane (within kNpcGridSamePlane). A floor made of many triangles is one sheet, and so is a wall.
//   Each sheet has its own grid of CELLS, kNpcGridCell across, in its own plane. A cell is ON THE SURFACE if its middle is on one of the
//   sheet's polygons. On a floor, a cell is on it as well if a polygon covers at least kNpcGridLeastCover of it, as Hudson's grid counts any
//   column a floor passes through: without that, a stair's tread no deeper than a cell can fall between two middles and get no cell at all,
//   and the stair is cut in two. Only a marine stands on those cells, because an alien's room is measured from a cell's middle, and a middle
//   off the polygon can be inside the wall beside it.
//   ROOM: from each cell's middle, straight out of the surface, how far it is to the nearest polygon. That says whether an alien's back fits.
//   LINKS join the cells of two sheets, both ways:
//     - where the map joins two polygons of different sheets (a floor and a wall, a wall and a ceiling, round a corner), along the stretch
//       they share;
//     - STEPS: where a marine's place on one floor is beside a place on another, seen from above, no more than kNpcGridStep higher or lower.
//       That's a stair's treads across their risers, which no join links, and a floor that carries on under a stair.
//   FALLS go one way: off an edge, two columns out, down to the highest floor below, as Hudson's grid has them.
//
//        wall sheet    +--+--+--+--+
//                      |  |  |  |  |    cells on the wall
//                      +--+--+--+--+
//        a join        ============      the wall's bottom cells are linked to the floor's, all along it
//        floor sheet   +--+--+--+--+
//                      |  |  |  |  |    cells on the floor
//                      +--+--+--+--+
//
// WHO FITS WHERE. Each cell has a bit for each kind of body:
//   - A MARINE STANDS (kNpcGridStands) where nothing lies in his square, kNpcGridStandHalf either way, which doesn't turn, from kNpcGridStep
//     above his feet up to his kNpcGridStandHeight. It's Hudson's own rule, and his box's real shape (CE_MARINE in extents.cpp). Anything
//     lower he steps up onto, so it isn't in his way. A floor steeper than the game's "nearly flat" he walks down and not up
//     (kNpcGridSteep): on Sulaco, 2026-11-05, Hudson stood pushing against a slope of about 46 degrees and couldn't climb it.
//   - An ALIEN CLINGS (kNpcGridClings) where every cell of the 3 x 3 round it that's on its sheet has kNpcGridClingRoom. A cell missing from
//     the 3 x 3 is allowed: the surface carries on onto another sheet there, or it's an edge the alien can hold.
// A door opens for whoever comes, so a door's polygons get no cells, but for its floor, and no part of a door is in anyone's way.
//
// WHAT IT DOESN'T DO YET (the plan's later steps, AvP-docs/engine/unified-npc-grid-plan.md): a room marked as an air duct, where a marine
// crawls at 1200; lifts, which come from the game's modules; the two-layer search a big level needs; and spreading a search over frames.
// Search here looks at a set number of cells in one go, to prove the cells join up, in tests and in the node copy.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It's handed the alien ways map and the list of polygons the map was built from, in the game's numbers
// (millimetres, and y goes down), so a test can make up a small world and check every answer.
//
// HOW MUCH THIS CAN TAKE. The polygons come from a level file, which comes from outside the game (AvP Tao #3). So every count has a limit, and
// memory has two ceilings: what the grid keeps, which is the per-level cap the "advanced AI" setting raises, and what it holds while it's
// building. Over any of them, Build gives back why, in words, and keeps no grid. Each limit is well above what the stock levels need,
// measured with no game running on all 63 of them (AvP-docs, engine/pathfinding-study/npc-grid.js).
#ifndef NPC_GRID_H
#define NPC_GRID_H

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <unordered_map>
#include <utility>
#include <vector>

#include "alien_ways_map.h"

int const kNpcGridCell = 300;                 // a cell's side, in millimetres
int const kNpcGridRoomStep = 25;              // room is kept in a byte, in steps of this: 25 goes exactly into every height below
int const kNpcGridMostRoom = 255 * kNpcGridRoomStep;   // 6375: more room than this is kept as this
int const kNpcGridStandHeight = 1950;         // Hudson's height (hudson_grid.h)
int const kNpcGridStandHalf = 450;            // half his width
int const kNpcGridStep = 450;                 // the most the game lifts a marine up a step (MAXIMUM_STEP_HEIGHT)
int const kNpcGridMostFall = 8000;            // Marine_Terminal_Velocity: a fall of more than this kills a marine
int const kNpcGridClingRoom = 900;            // what an alien needs out from the surface it's on
int const kNpcGridSamePlane = 15;             // two polygons lie in one plane if each one's middle is this near the other's plane
float const kNpcGridSameFacing = 0.99939083f; // and face the same way if their facings are within 2 degrees: this is cos(2 degrees)
float const kNpcGridNearlyFlat = 60000.0f / 65536.0f;   // the game's "nearly flat" floor (NEARLYFLATFLOOR_THRESHOLD in dynamics.cpp)
int const kNpcGridLeastCover = kNpcGridCell * kNpcGridCell / 100;   // a floor polygon has to cover this many square millimetres of a cell: 1%
int const kNpcGridBox = 1000;                 // the boxes, a metre each way, that polygons are sorted into for the rays and the marine's square

// What a cell's flags hold
uint8_t const kNpcGridStands = 1;             // a marine stands here
uint8_t const kNpcGridClings = 2;             // an alien clings here
uint8_t const kNpcGridSteep = 4;              // the floor here is steeper than nearly flat: a marine only goes down it
uint8_t const kNpcGridMiddleOn = 8;           // the cell's middle is on one of its sheet's polygons
uint8_t const kNpcGridCoveredOnly = 16;       // its middle isn't, but a floor polygon covers at least kNpcGridLeastCover of it

uint32_t const kNpcGridNoCell = 0xFFFFFFFFu;
int const kNpcGridMostLookRound = 5;          // NearestCell looks at most this many cells either way on a sheet: 11 x 11, the reach
int const kNpcGridReach = 1500;              // how far from a cell a search's start or goal can be: an alien's middle is up to a metre off its surface
int const kNpcGridSideSlack = 300;           // how far behind a sheet's plane a point can be and still count as in front of it: one cell. A body
                                             // stands right on its floor, and a sheet's plane is an average of polygons up to kNpcGridSamePlane
                                             // apart, so a body on the floor can be a little behind it. A body under a floor is metres behind.

// THE LIMITS. What the stock levels need is in brackets: the most on any of the 63, measured by the node copy (npc-grid.js), whose steps are
// this code's. A level made to take more is refused, and the game moves its NPCs as before.
struct NpcGridLimits
{
	size_t mostSheets;              // 200,000 (17,349, on Temple)
	size_t mostCellsInASheet;       // 16,000,000: a sheet's whole grid, on the surface or not (881,588, on kens-co-op)
	size_t mostCells;               // 64,000,000: every sheet's whole grid (5,324,458, on Genshd1)
	size_t mostBoxEntries;          // 8,000,000: a polygon in each metre box its plane passes through, while building (642,892, on Genshd1)
	size_t mostRayTests;            // 100,000,000: polygons tried by all the rays for room (2,436,070, on Temple)
	size_t mostSquareTests;         // 100,000,000: polygons tried against a marine's square, in all (1,214,323, on Genshd1)
	size_t mostLinks;               // 20,000,000: each way of each link counts, before the ones found twice are taken out (932,556 after, on Derelict)
	size_t mostFalls;               // 200,000 (4,526)
	size_t mostBytesKept;           // the cap: everything the grid keeps. 64 MB, or 250 MB with "advanced AI" (17.3 MB, on Genshd1)
	size_t mostBytesBuilding;       // 128 MB: the lists it makes while building, which it lets go of when it's done (not yet measured in C++)
};

// The usual limits. With "advanced AI" on, the cap on what the grid keeps is the user's 250 MB, and otherwise 64 MB (the user, 2026-11-08).
NpcGridLimits NpcGrid_UsualLimits(bool advancedAI);

// Which body a search is for
enum NpcGridBody
{
	kNpcGridMarine,   // stands on kNpcGridStands cells; steps, falls, and no climbing a steep floor
	kNpcGridAlien     // clings to kNpcGridClings cells, any kind, across any link; a floor costs floorPrice times as much
};

enum NpcGridSearch
{
	kNpcGridFound,      // the way is in the list handed in, from the start cell to the goal cell
	kNpcGridNoWay,      // the cells don't join up between the two, for this body
	kNpcGridTooFar,     // it looked at as many cells as it was allowed to and didn't get there
	kNpcGridSearching   // not finished: call Work again
};

class NpcGrid
{
public:
	NpcGrid();

	// Lets go of everything, and leaves no grid. A stop that was asked for is forgotten too, ready for the next level.
	void Clear();

	// Works out the whole grid from the polygons the alien ways map was built from, and the map itself, which has to be ready. It gives
	// back nothing if the grid's ready, or why there's no grid, in words, and then nothing is kept.
	const char *Build(const std::vector<AlienWaysPolygon> &polygons, const AlienWaysMap &map);

	// Asks a build that's running on another thread to stop: the level is ending, or the game is closing. Every loop of the build looks at
	// this, so it stops soon after, says why, and keeps nothing. Asked before Build starts, Build stops at once. Only Clear forgets it.
	void AskToStop() { stopAsked.store(true); }

	bool Ready() const { return ready; }

	// Lowers the limits, so a test can try each one without millions of cells, or raises the cap for "advanced AI".
	void SetLimits(const NpcGridLimits &to) { limits = to; }

	// So a test can stop a build part way with no second thread: the build asks itself to stop at its nth look at the stop. 0 means never.
	// Nothing else calls this.
	void StopAtLookForATest(size_t look) { stopAtLook = look; }

	size_t Sheets() const { return sheets.size(); }
	size_t Cells() const { return flags.size(); }            // every sheet's whole grid
	size_t SurfaceCells() const { return surfaceCells; }     // the cells on the surface
	size_t Links() const { return linkTo.size(); }           // each way of each link counts
	size_t Steps() const { return steps; }                   // of those, how many are a marine's steps, counted once for both ways
	size_t Falls() const { return fallTo.size(); }
	size_t BytesKept() const { return bytesKept; }
	size_t BytesWhileBuilding() const { return bytesBuildingUsed; }   // the most the last build that worked held, at its top
	size_t RayTests() const { return rayTests; }
	size_t SquareTests() const { return squareTests; }
	size_t BoxEntries() const { return boxEntries; }         // polygons listed in metre boxes, while it was building

	// One cell's sheet, which kind of surface it's on, its middle in the game's numbers, its flags and its room. A number that isn't a
	// cell gives -1, a wall, false (with out untouched), 0 and 0.
	int SheetOf(uint32_t cell) const;
	AlienWaysKind KindOf(uint32_t cell) const;
	bool Middle(uint32_t cell, double out[3]) const;
	uint8_t Flags(uint32_t cell) const { return cell < flags.size() ? flags[cell] : 0; }
	int Room(uint32_t cell) const { return cell < room.size() ? room[cell] * kNpcGridRoomStep : 0; }

	// The cell of a sheet that a point lies over, seen straight down onto the sheet's plane, or kNpcGridNoCell if it's outside the sheet's
	// grid. Whether it's on the surface is in its flags.
	uint32_t CellOver(int sheet, double x, double y, double z) const;

	// The alien ways map's number for the polygon a cell is on: the one its middle is on, or for a cell only covered, the floor that covers
	// it. -1 for a cell on no polygon, or a number that isn't a cell. A way's cells give a way's polygons, for the claims and the doors.
	int PolygonOf(uint32_t cell) const { return cell < polygonOf.size() ? polygonOf[cell] : -1; }

	// Where a cell is on its sheet: the sheet, and its place along the sheet's two axes, from 0. False, with nothing set, for a number that
	// isn't a cell. CellAt is the other way round: kNpcGridNoCell outside the sheet's grid, or for a sheet that isn't one.
	bool CellPlace(uint32_t cell, int *sheet, int *i, int *j) const;
	uint32_t CellAt(int sheet, int i, int j) const;

	// The cell nearest to a point, among those a body fits on, no further than within millimetres, or kNpcGridNoCell
	uint32_t NearestCell(double x, double y, double z, NpcGridBody body, int within) const;

	// The cells a cell is linked to, both ways (*count of them), and the ones a marine falls to from it, one way. nullptr and 0 if none.
	const uint32_t *LinksOf(uint32_t cell, size_t *count) const;
	const uint32_t *FallsOf(uint32_t cell, size_t *count) const;

	// A search over the cells, from one to another, for one body, looking at no more than mostLooks cells. A cell beside it on its sheet
	// costs the distance between their middles, a link or a fall the same, and for an alien a floor's cell costs floorPrice times as much
	// (1 to kAlienWaysMostFloorPrice). The way found, start and goal included, goes into way. looks, if it's given, is set to how many cells
	// it looked at. What it remembers grows with the cells it looks at, not with the level, and goes when it ends: this one is for tests and
	// for measuring, not for a frame (the plan's step 3 spreads a search over frames).
	NpcGridSearch Search(uint32_t from, uint32_t to, NpcGridBody body, int floorPrice, size_t mostLooks, std::vector<uint32_t> *way,
		size_t *looks);

	// THE SAME SEARCH SPREAD OVER FRAMES, as the alien ways map's is, for the guide (alien_ways_guide.h): one at a time.
	//
	// StartSearch takes positions, and starts from the cell the body fits on nearest start, within kNpcGridReach, to the one nearest goal.
	// It gives back nothing if it started, or why not, in words. A search that's running is dropped. It looks at no more than mostLooks cells
	// in all.
	// claims, if given, makes a cell dearer where other aliens' ways are, by the map polygon under it (PolygonOf), as the map's own search
	// does; searcher is the guide's place for the alien searching, whose own claims don't count, and claimStep and claimMost are what each
	// claim adds and the most a cell can cost, as the map's search takes them (a flanker's are bigger). shut, if given, has a byte for each
	// map polygon, not 0 for a polygon of a door that's shut now: the search keeps off its cells, but for the start's and the goal's. claims
	// and shut have to stay as they are until the search ends.
	const char *StartSearch(AlienWaysXYZ start, AlienWaysXYZ goal, NpcGridBody body, int floorPrice, size_t mostLooks,
		const AlienWaysClaims *claims = nullptr, int searcher = -1, const std::vector<uint8_t> *shut = nullptr,
		double claimStep = kAlienWaysClaimPriceStep, double claimMost = kAlienWaysClaimMostPriceFactor);

	// Carries on, looking at up to looksAllowed more cells, and says how it stands: kNpcGridSearching until it's done. Asked again after it's
	// ended, it gives the same answer.
	NpcGridSearch Work(size_t looksAllowed);

	// The way the last search found, as the alien ways map gives one: the start, a point wherever the way turns or changes surface, and the
	// goal. Each point's kind and facing are those of the surface the way is on from it to the next. At most kAlienWaysMostWayPoints points:
	// WayCutShort says whether it was cut there. Empty if there was no way.
	const std::vector<AlienWaysPoint> &Way() const { return way; }
	bool WayCutShort() const { return cutShort; }

	// The cell under each point of Way(), one for each, for a log line; and every cell of the way, start to goal, for a smoother
	const std::vector<uint32_t> &WayCells() const { return wayPointCells; }
	const std::vector<uint32_t> &WayPath() const { return wayCells; }

	// The map polygon under each point's cell, one for each point of Way(), as the map's own WayPolygons() is: for the claims a way puts on
	// the polygons it's on. -1 for a cell on no polygon.
	const std::vector<int> &WayPolygons() const { return wayPolygons; }
	size_t LastSearchLooks() const { return searchLooks; }

private:
	struct Seen
	{
		double cost;
		uint32_t cameFrom;
		bool done;
	};

	NpcGridSearch BeginCells(uint32_t from, uint32_t to, NpcGridBody body, int floorPrice, size_t mostLooks);
	double PriceOf(uint32_t cell) const;
	bool ShutOff(uint32_t cell) const;
	void TryMove(const double goalAt[3], const double at[3], uint32_t c, uint32_t n, double newCost);
	AlienWaysPoint PointAt(uint32_t cell, double x, double y, double z) const;
	void MakeWay();
	struct Sheet
	{
		double n[3];      // which way it faces, out of the surface: a unit length, in the game's numbers (y down)
		double d;         // how far its plane is from the middle of the world, along n
		double u[3];      // its cells' first axis, along the sheet
		double v[3];      // and its second
		int i0, j0;       // its grid's first cell, along u and v, counting from the middle of the world in kNpcGridCell steps
		int w, h;         // how many cells its grid has along u and v
		uint32_t first;   // its first cell's number
		uint8_t kind;     // an AlienWaysKind
	};

	bool StopNow();
	void LetGo();
	const char *Fail(const char *why);
	bool CountBuilding(size_t bytes);

	std::vector<Sheet> sheets;
	std::vector<uint8_t> flags;          // a cell's flags
	std::vector<uint8_t> room;           // a cell's room, in kNpcGridRoomStep steps
	std::vector<int32_t> polygonOf;      // a cell's map polygon, or -1
	std::vector<uint32_t> linkFrom;      // each link, sorted by the cell it's from
	std::vector<uint32_t> linkTo;
	std::vector<uint32_t> fallFrom;      // each fall, the same
	std::vector<uint32_t> fallTo;

	// The search that's running, or the last one. What it remembers grows with the cells it looks at, up to its mostLooks.
	std::unordered_map<uint32_t, Seen> seen;
	std::vector<std::pair<double, uint32_t> > open;   // a heap, the cheapest guess on top
	uint32_t searchFrom, searchTo;
	NpcGridBody searchBody;
	int searchPrice;
	size_t searchLooks, searchMostLooks;
	NpcGridSearch searchState;
	std::vector<uint32_t> wayCells;
	std::vector<AlienWaysPoint> way;
	std::vector<uint32_t> wayPointCells;
	std::vector<int> wayPolygons;
	AlienWaysXYZ searchStart, searchGoal;
	const AlienWaysClaims *searchClaims;
	int searchSearcher;
	double searchClaimStep, searchClaimMost;
	const std::vector<uint8_t> *searchShut;
	bool cutShort;

	size_t surfaceCells, steps, rayTests, squareTests, boxEntries;
	size_t bytesKept, bytesBuilding, bytesBuildingUsed;
	NpcGridLimits limits;
	bool ready;
	std::atomic<bool> stopAsked;
	size_t looksAtStop, stopAtLook;
};

#endif
