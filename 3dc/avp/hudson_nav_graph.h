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
//   - A ray at knee height, and one at head height, have to get all the way to the next spot and half a marine past it. A surface facing up doesn't stop them, because that's only the floor rising. A door doesn't
//     either, because doors open.
//   - A ray straight down at the next spot, from head height, has to land on something he can walk on. How steep he can walk on is what the game's own collisions say. It can be as much higher as a step, or a
//     slope can take him higher still, and it can be lower by as far as he'd be willing to drop. A stride can take in two stairs, so if it's higher than one step, a ray halfway there has to find the stair between.
//   - A ray straight up from there has to find room for a marine.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It only asks the rays, so a test can give it a small made-up world and check every answer. The game's own line of sight is what answers in a real run: see hudson_nav.cpp.
//
// HOW MUCH THIS CAN TAKE. There's a limit on how many spots it holds, and on how many a search looks at. Work is told how many rays it may use each time, so that a frame never takes long, and it goes over by
// no more than one spot's worth. It can always end: every spot is looked at once.

#ifndef HUDSON_NAV_GRAPH_H
#define HUDSON_NAV_GRAPH_H

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
int const kHudsonNavMostDrop = 16000;      // the furthest he'd step down. Some of the levels' ways really do go down a shaft. It's a way he can't come back, so a drop counts as a long way round: see kHudsonNavDropCost
int const kHudsonNavDropCost = 3;          // going down a drop counts as this many times its height, so that he takes the stairs if there are any
int const kHudsonNavDoorCost = 2000;       // going through a door counts as this much further, so that he goes round by an open way if it's about as short
int const kHudsonNavReachOfGoal = 1500;    // a spot this close to where he's going, sideways, is close enough: the last bit is in a straight line
int const kHudsonNavGoalHeight = 1500;     // and this close in height, so that the floor above or below it doesn't count

size_t const kHudsonNavMostNodes = 30000;  // spots a level's map can hold. That's about 1.5 MB, and 30 000 square metres of floor
int const kHudsonNavMostLooks = 8192;      // spots one search can look at before it gives up
int const kHudsonNavMostRaysForASpot = 32; // what looking at one spot can take: eight ways, at four rays each

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

class HudsonNavGraph
{
public:
	HudsonNavGraph();

	// Forgets everything. A new level is a new map.
	void Clear();

	// Starts looking for a way between two places, each at floor height. The spots it knows are kept, but each is looked at again: a door may have opened since, and the rays can only see what's loaded, so a
	// room that wasn't there before may be there now.
	void StartSearch(int fromX, int fromY, int fromZ, int toX, int toY, int toZ);

	// Carries on looking, using about this many rays, and says how it stands.
	HudsonNavSearch Work(HudsonNavRays &rays, int raysAllowed);

	// The way the last search found, from where it started to where it was going.
	const std::vector<HudsonNavPoint> &Way() const { return way; }

	// The point just past the one he's got to, for when he can't see any of it. -1 if there's no way.
	int NextAlong() const;

	// Goes along the way as far as he's got, and gives back how far along it the furthest point is that he can see from where he stands. So he cuts corners that are safe to cut, and goes round the ones that
	// aren't. Uses at most two rays for each of mostAhead points. -1 if there's no way, or he can't see any of it.
	int FurthestInSight(HudsonNavRays &rays, int x, int y, int z, int mostAhead);

	// Whether both rays get there, from one place to another at floor height: at the knee and at the head. Doors count as in the way here, unless doorsCount is false.
	bool LineIsClear(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int toX, int toZ, bool doorsCount);

	// The same, and also two more at the knee, each as far to the side as he is wide. So that a corner he'd really hit counts as in the way, though a line down his middle would miss it.
	bool WayIsWideEnough(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int toX, int toZ);

	size_t Spots() const { return nodes.size(); }
	size_t FlatSpots() const { return flatSpots; }
	size_t SlopeSpots() const { return slopeSpots; }
	int LastSearchLooks() const { return looks; }
	uint64_t RaysUsed() const { return raysUsed; }

private:
	struct Node
	{
		int x, y, z;
		int gridX, gridZ;
		int next[8];            // the spot each of the eight ways round leads to, or -1 if he can't go that way
		uint8_t doors;          // one bit for each of the eight: this way goes through a door
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

	std::vector<HudsonNavPoint> way;
	size_t alongTheWay;

	size_t flatSpots, slopeSpots;
	uint64_t raysUsed;
	int raysThisWork;
};

#endif
