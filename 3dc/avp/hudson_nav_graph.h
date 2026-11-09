// Hudson's map of a level, made from what his lidar finds. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY THERE'S A MAP. The marine code walks in a straight line. A straight line to the next door can go through a wall, and it leaves him pressed against that wall. So when the line isn't clear, he works out
// a way round, from the real shape of the level, the way a person glancing round a room would: can I step there? And from there?
//
// WHAT THE MAP IS. Spots about a stride apart, each one a place on a floor where he has been shown he can stand. Joined up by the steps he's been shown he can take. It's three-dimensional: one floor above
// another are two different spots at the same x and z. A step down that's too big to come back up is a join that only goes one way. It's only ever as big as he's needed it to be, because it's made as he
// asks about each spot, starting from where he stands and going first towards where he's going.
//
// WHAT A STEP IS. From a spot, looking one of eight ways round:
//   - A ray at head height has to get all the way to the next spot and half a marine past it. A surface facing up doesn't stop it, because that's only the floor rising. A door doesn't either, because doors open.
//   - Then he's walked along the floor, a foot at a time. At each foot, a ray straight down from head height has to land on something he can walk on, how steep being what the game's own collisions say. It can be
//     no more than a step higher than the last foot, which is what lets stairs be stairs: each stair is seen as one stair, however many there are in a stride. A slope can rise more than that, up to twice as far as the
//     foot is long. It can be lower by as far as he'd be willing to drop.
//   - A ray at his knee has to get from each foot to the next. It's that short, and starts from the height he's got to, because on stairs a longer one from the bottom would run into the second stair, and read it as a
//     wall. A stair is no higher than a step, and the knee is above that, so only something he really can't step over stops this one. A rail that's too thin for the rays straight down to land on is what it's for.
//   - A ray straight up from the next spot has to find room for a marine.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It only asks the rays, so a test can give it a small made-up world and check every answer. The game's own line of sight is what answers in a real run: see hudson_nav.cpp.
//
// HOW MUCH THIS CAN TAKE. There's a limit on how many spots it holds, and on how many a search looks at. Work is told how many rays it may use each time, so that a frame never takes long, and it goes over by
// no more than one spot's worth. It can always end: every spot is looked at once.

#ifndef HUDSON_NAV_GRAPH_H
#define HUDSON_NAV_GRAPH_H

#include "hudson_grid.h"

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <queue>
#include <vector>

// What one ray found. distance is in millimetres. normalY is which way what it hit faces, from -65536, straight up, to 65536, straight down, because y goes down in this game. The answer always faces the way the
// ray came from.
struct HudsonNavRayAnswer
{
	bool hit;
	bool door;
	int distance;
	int normalY;
};

// Whatever answers the rays. The direction is one whole length, which is 65536.
class HudsonNavRays
{
public:
	virtual ~HudsonNavRays() {}
	virtual HudsonNavRayAnswer Cast(int x, int y, int z, int directionX, int directionY, int directionZ, int longest) = 0;
};

// All in millimetres, and these are the marine's own: extents.cpp, dynamics.h.
int const kHudsonNavGridStep = 1000;       // about a stride
int const kHudsonNavMarineRadius = 450;
int const kHudsonNavMarineHeight = 1950;
int const kHudsonNavStepUp = 450;          // MAXIMUM_STEP_HEIGHT: the most the game's collisions lift him without a slope
int const kHudsonNavKneeHeight = 600;      // just above that, because a ray lower than that would stop at every stair
int const kHudsonNavHeadHeight = 1700;
int const kHudsonNavRayOverlap = 20;       // how far a ray from one foot goes past the next. Feet are in whole millimetres, so without this a wall exactly at a foot could be just past the end of one ray and just behind the start of the next
int const kHudsonNavFootStep = 250;        // how far apart the floor is looked at along a stride: shorter than any stair anyone can climb, and the game's are 780 deep, so that it can't miss one
int const kHudsonNavMostDrop = 8000;       // the furthest he'd step down, which is what a marine survives: Marine_Terminal_Velocity, 20000 a second, under GRAVITY_STRENGTH, 25000, is a fall of 8 metres. It's a way he can't come back, so a drop counts as a long way round: see kHudsonNavDropCost
int const kHudsonNavDropCost = 3;          // going down a drop counts as this many times its height, so that he takes the stairs if there are any
int const kHudsonNavDoorCost = 2000;       // going through a door counts as this much further, so that he goes round by an open way if it's about as short
int const kHudsonNavReachOfGoal = 1500;    // a spot this close to where he's going, sideways, is close enough: the last bit is in a straight line
int const kHudsonNavGoalHeight = 1500;     // and this close in height, so that the floor above or below it doesn't count

size_t const kHudsonNavMostNodes = 30000;  // spots a level's map can hold. That's about 1.5 MB, and 30 000 square metres of floor
int const kHudsonNavMostLooks = 8192;      // spots one search can look at before it gives up
int const kHudsonNavMostFeet = 9;          // the most feet in a stride, for the longest one: the first, corner to corner across two places on the grid, because he starts anywhere on his
int const kHudsonNavMostRaysForASpot = 160; // what looking at one spot can take: for each of eight ways, the head, the room above, and a ray at the knee and one down for each foot
int const kHudsonNavEndOfTheWayIsWithin = 600; // this close to the last point of a way, sideways, he's come to the end of it

enum HudsonNavSearch
{
	kHudsonNavSearching,    // not finished: call Work again
	kHudsonNavFound,        // Way goes all the way there
	kHudsonNavNoWay         // there's none it could find. Way goes as near as it got
};

// One spot on the way.
struct HudsonNavPoint
{
	int x, y, z;
};

// One spot on the map, for anything that wants to look at it, such as drawing it. x, y and z are on the floor, in millimetres, and y goes down. next is the spot each of the eight ways round leads to, or -1 if he can't go that way. It
// only goes one way: a drop has no way back up. doors and drops have one bit for each of the eight: that way goes through a door, or has a drop in it.
struct HudsonNavSpot
{
	int x, y, z;
	bool sloping;
	int next[8];
	uint8_t doors;
	uint8_t drops;
};

class HudsonNavGraph
{
public:
	HudsonNavGraph();

	// Forgets everything. A new level is a new map.
	void Clear();

	// map=grid: from now on StartSearch and Work ask this grid, which is worked out from the level's polygons, and not the rays. Everything else here is the same, and still asks the rays.
	void UseGrid(HudsonGrid *grid) { gridToUse = grid; }

	// If the grid is finding the way, the floor it took for where he's going, which is what arriving has to be measured against.
	bool GridGoalFloor(int *y) const;

	// Starts looking for a way between two places, each at floor height. The spots it knows are kept, but each is looked at again: a door may have opened since, and the rays can only see what's loaded, so a
	// room that wasn't there before may be there now.
	void StartSearch(int fromX, int fromY, int fromZ, int toX, int toY, int toZ);

	// Carries on looking, using about this many rays, and says how it stands.
	HudsonNavSearch Work(HudsonNavRays &rays, int raysAllowed);

	// How the last search ended, or kHudsonNavSearching if one's still going, when Way() is empty. kHudsonNavNoWay if there's never been one. It's what says whether Way() arrives, or only goes as near as it got.
	HudsonNavSearch LastSearch() const { return lastSearch; }

	// The way the last search found, from where it started to where it was going.
	const std::vector<HudsonNavPoint> &Way() const { return way; }

	// The point just past the one he's got to, for when he can't see any of the way ahead. -1 if there's no way.
	int NextAlong() const;

	// Whether he's at the last point of the way, so that there's nothing more of it to walk. False if there's no way.
	bool HasComeToTheEnd(int x, int z) const;

	// Goes along the way as far as he's got, and gives back how far along it the furthest point ahead of him is that he can see from where he stands. So he cuts corners that are safe to cut, and goes round the ones
	// that aren't. The point he's got to doesn't count, because walking to where he already is gets him nowhere. Uses at most four rays for each of mostAhead points. -1 if there's no way, or he can't see any of it.
	int FurthestInSight(HudsonNavRays &rays, int x, int y, int z, int mostAhead);

	// Whether both rays get there, from one place to another at floor height: at the knee and at the head. Doors count as in the way here, unless doorsCount is false.
	bool LineIsClear(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int toX, int toZ, bool doorsCount);

	// Whether there's floor all the way along a line, no lower than a step, as he'd need to walk it. A ray straight down each stride, up to mostStrides of them, because a line can be clear of walls and go straight off a
	// ledge. Rising floor is fine, so long as it's no more than his head height up. Further than mostStrides is left for when he gets there.
	bool LineHasFloor(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int toX, int toZ, int mostStrides);

	// The same, and also two more at the knee, each as far to the side as he is wide. So that a corner he'd really hit counts as in the way, though a line down his middle would miss it.
	bool WayIsWideEnough(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int toX, int toZ);

	size_t Spots() const { return nodes.size(); }

	// The spot of this number, up to Spots(). A number that isn't one gives a spot with no ways out of it, at the origin.
	HudsonNavSpot SpotAt(size_t number) const;

	size_t FlatSpots() const { return flatSpots; }
	size_t SlopeSpots() const { return slopeSpots; }
	int LastSearchLooks() const { return looks; }
	uint64_t RaysUsed() const { return raysUsed; }

private:
	HudsonGrid *gridToUse;
	bool searchIsOnTheGrid;   // a search goes on the way it began, whichever map is set meanwhile

	struct Node
	{
		int x, y, z;
		int gridX, gridZ;
		int next[8];            // the spot each of the eight ways round leads to, or -1 if he can't go that way
		uint8_t doors;          // one bit for each of the eight: this way goes through a door
		uint8_t drops;          // and this way has a drop in it, which walking down stairs hasn't
		bool sloping;           // what it's standing on isn't flat, which a stair is
		uint32_t lookedAtIn;    // which search last looked at it
	};

	struct Waiting
	{
		double total;           // how far it is by way of this spot, as far as anyone can tell
		uint32_t order;         // which came first, so that two runs always choose the same one
		int node;

		bool operator<(const Waiting &other) const
		{
			// The queue gives back its biggest, so smaller totals have to count as bigger
			return total != other.total ? total > other.total : order > other.order;
		}
	};

	HudsonNavRayAnswer Ray(HudsonNavRays &rays, int x, int y, int z, int directionX, int directionY, int directionZ, int longest);
	int NodeAt(int gridX, int gridZ, int floorHeight) const;
	int AddNode(int x, int y, int z, int gridX, int gridZ, int normalY);
	void LookAt(HudsonNavRays &rays, int number);
	void TryStep(HudsonNavRays &rays, int number, int way);
	double GuessFrom(int number) const;
	void MakeTheWay(int lastNode);

	std::vector<Node> nodes;
	std::map<uint64_t, std::vector<int> > columns;    // for each place on the grid, the spots on it: one for each floor

	// The search going on now. stamp says which search each spot's numbers are from, so nothing has to be cleared between one and the next.
	std::vector<double> cost;
	std::vector<int> cameFrom;
	std::vector<uint32_t> stamp;
	uint32_t searchNumber;
	std::priority_queue<Waiting> waiting;
	uint32_t order;
	int looks;
	int nearest;
	double nearestGuess;
	int toX, toY, toZ;
	bool searching;
	HudsonNavSearch lastSearch;

	std::vector<HudsonNavPoint> way;
	size_t alongTheWay;

	size_t flatSpots, slopeSpots;
	uint64_t raysUsed;
	int raysThisWork;
};

#endif
