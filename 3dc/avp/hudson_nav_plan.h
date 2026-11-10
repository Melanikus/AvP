// Hudson's navigation: the sums, with nothing of the game in them, so that a test can run them. Written 2026-11-05 by Melanikus for the AvP port. hudson_nav.cpp says what navigation is.
//
// In this game y goes DOWN. So a floor's height gets bigger the lower it is, and a floor faces towards -y.

#ifndef HUDSON_NAV_PLAN_H
#define HUDSON_NAV_PLAN_H

#include <stddef.h>
#include <string>
#include <vector>

// The game's numbers for time and for a direction's length: 65536 means one second, or a whole length.
int const kHudsonNavOneWhole = 65536;

// He isn't getting anywhere if he's stayed within this much of one spot, in millimetres, for this long. A door takes a couple of seconds to open, so this has to be longer than that.
int const kHudsonNavProgressIsAtLeast = 1000;
int const kHudsonNavStuckAfter = 5 * kHudsonNavOneWhole;
int const kHudsonNavNearerIsAtLeast = 500;
int const kHudsonNavNoNearerAfter = 30 * kHudsonNavOneWhole;

// Which way a surface faces, as how much of it faces up: 65536 is dead level, and 0 is upright.
int const kHudsonNavFlatFloor = 64540;     // within 10 degrees of level
int const kHudsonNavWalkable = 30000;      // what the game's own collisions count as a floor, FLOOR_THRESHOLD in dynamics.cpp, about 63 degrees off level. Steeper than this, he slides
int const kHudsonNavUpright = 8000;        // within 7 degrees of upright is a wall, and anything facing further down than that is a ceiling

int const kHudsonNavSpiralStep = 1000;     // how far apart the places tried in a room are: about a stride
int const kHudsonNavSpiralPlaces = 129;    // the middle, and 8 places on each of 16 rings round it, so as far as 16 metres out

// Where to head for next, from the list of which module links to which. It finds the nearest module he hasn't been to, counting how many modules he'd pass through, and gives back the neighbour
// of "from" that starts the way there. That neighbour may be one he's been to, because getting somewhere new can mean going back through. -1 means there's nowhere new he can get to. Each module
// is looked at once, so this takes as long as the level is big and no longer, and two runs choose the same way. A number in the list that isn't a module is stepped over.
int HudsonNav_NextModule(const std::vector<std::vector<int> > &links, int from, const std::vector<char> &beenThere);

// Whether he's getting anywhere.
struct HudsonNavProgress
{
	int x, y, z;     // the spot he's being measured from
	int timeHere;    // how long he's been within reach of it
};

void HudsonNav_StartProgress(HudsonNavProgress *progress, int x, int y, int z);

// Called once a frame with where he is and how long the frame took. True when he's got nowhere for too long. Walking on starts the measuring again from where he's got to.
bool HudsonNav_IsStuck(HudsonNavProgress *progress, int x, int y, int z, int frameTime);

// Whether he's getting any nearer to where he's going. Going back and forth looks like getting somewhere to the rule above, because he's never in one place for long. A way round can take him further off for a while,
// so this one is slower. dx, dy and dz are how far he is from it.
struct HudsonNavTowards
{
	double nearest;       // the closest he's been
	int timeSinceThen;
};

void HudsonNav_StartTowards(HudsonNavTowards *towards, int dx, int dy, int dz);
bool HudsonNav_NotGettingNearer(HudsonNavTowards *towards, int dx, int dy, int dz, int frameTime);

// Whether his box has caught on the lip of a stair. In dynamics.cpp a stair is only a step if its top is above the bottom of his box. But a surface counts as touching him from a little further off than that, so if he comes to rest with the
// top of a stair just under his feet, it's a wall to him, for good, though he's already standing on the next one. normalY is which way what he's touching faces, up and down. againstHisWay is how squarely it faces him, one whole being head on. floorAhead is how
// much lower the floor just past the front of his box is than his feet. Only what's level with his feet can be walked onto by moving a little, so nothing else counts.
int const kHudsonNavLipIsUpright = 250;      // the same test as dynamics.cpp uses for whether something is a stair's face
int const kHudsonNavLipFacesHim = 32768;     // at least this squarely in the way
int const kHudsonNavLipIsWithin = 15;        // COLLISION_GRANULARITY is 10, and this is a little more

bool HudsonNav_BoxIsCaughtOnALip(int normalY, int againstHisWay, int floorAhead);

// Whether how far he came down fits how long he was in the air. The game's sum for how fast he's falling can't say: it divides last frame's movement by this frame's time, so one slow frame can make it read four times too fast, and anything
// else that moves him, such as the game putting him somewhere else, can make it read as anything at all. Time in the air can't be fooled like that: under gravity, the distance is half of gravity times the time squared, and nothing falls
// further than that. There's room for a frame's rounding, and no more. timeInTheAir is in the game's fixed point, where one second is 65536.
int const kHudsonNavGravity = 25000;                  // GRAVITY_STRENGTH in dynamics.cpp, in millimetres a second, each second
double const kHudsonNavFallCanBeOverBy = 1.5;
int const kHudsonNavShortestFatalFall = 5000;         // The game's sum can say a fall this short or shorter kills him, but only because of a slow frame: 8 metres is what really does

bool HudsonNav_ThatWasARealFall(int distanceFallen, int timeInTheAir);

// A guard stops walking when he's about this far from his post, by the rough measure bh_marine.cpp uses. It's 1500 there, and a little more here, so that where he stops always counts as there.
int const kHudsonNavGuardStopsWithin = 1800;

// As close as he has to be to the middle of a room for it to count as the middle.
int const kHudsonNavMiddleIsWithin = 600;

// Whether he's within this much of a spot. Height counts for a quarter, the way bh_marine.cpp counts it, because a spot can be written down a little above a floor.
bool HudsonNav_IsAtSpot(int dx, int dy, int dz, int within);

// The game's own turning code lets a marine's matrix slowly grow or shrink: 3% too big after half a minute, 10% too small after two and a half, because each frame's small turn is rounded. The camera
// looks through that matrix, so it would slowly stretch what's on the screen. This makes a clean one from the way he's facing: nine numbers, row by row. False if he isn't facing any way at all.
bool HudsonNav_UprightTurn(int facingX, int facingZ, int *turn);

// A ROUTE is a text file, one step to a line, that says the way through a level. tools/hudson/make-route.ps1 makes one from the route lists. A line is:
//   walk X Y Z NAME       walk into the room of that name, through the join at that spot
//   press X Y Z NAME      go to the thing of that name, face it, and press it. X Y Z is where it is
//   walkinto X Y Z NAME   go to that spot, because the thing of that name goes off when someone's there
//   other NAME            a step he can't do yet. The route stops there. NAME is whatever says what it is
// and three that only tests use, to make a death happen on purpose (tools\hudson\test-routes):
//   putat X Y Z NAME      he's put at that spot straight away
//   walkoff X Y Z NAME    he walks straight at that spot, with no map to stop him going over an edge. If he stops without falling, the step's over
//   killtheplayer NAME    the player dies, by the game's own damage. There's no place, and NAME says what it's for
// Blank lines and lines starting with # are left out. Nothing in the file is believed: every line is checked here, and a file with anything wrong with it isn't used at all.
enum HudsonNavStepKind
{
	kHudsonNavStepWalk,
	kHudsonNavStepPress,
	kHudsonNavStepWalkInto,
	kHudsonNavStepOther,
	kHudsonNavStepPutAt,
	kHudsonNavStepWalkOff,
	kHudsonNavStepKillThePlayer
};

struct HudsonNavStep
{
	HudsonNavStepKind kind;
	int x, y, z;
	std::string name;
};

size_t const kHudsonNavMostRouteBytes = 1024 * 1024;
size_t const kHudsonNavMostRouteSteps = 4096;
size_t const kHudsonNavLongestRouteLine = 200;
size_t const kHudsonNavLongestStepName = 80;
int const kHudsonNavFurthestPlace = 10000000;    // much further from the middle of the world than any level goes, in millimetres

extern const char *const kHudsonNavRouteTooLong;
extern const char *const kHudsonNavRouteEmpty;
extern const char *const kHudsonNavRouteTooManySteps;
extern const char *const kHudsonNavRouteLineTooLong;
extern const char *const kHudsonNavRouteUnknownWord;
extern const char *const kHudsonNavRouteBadNumber;
extern const char *const kHudsonNavRouteBadName;

// Reads a whole route. Gives back 0 if it's good, and otherwise words saying what's wrong, one of those above. text needn't end in anything: only size says where it ends.
const char *HudsonNav_ReadRoute(const char *text, size_t size, std::vector<HudsonNavStep> *steps);

int const kHudsonNavFatalFallSpeed = 20000; // Marine_Terminal_Velocity in bh_marine.cpp, in millimetres a second. Only for saying so in the log: the game's own number is what decides

// How a walk ended. Only the first says nothing about a route: he was only asked to walk so far, or never had one.
enum HudsonNavEnding
{
	kHudsonNavEndingNothingSpecial,
	kHudsonNavEndingCrossedTheMap,       // the game says the level is complete, he's on the route's last step, and nobody died on the way
	kHudsonNavEndingLevelDidntEnd,       // he got to the end of the route, and the game doesn't say the level is complete
	kHudsonNavEndingLevelEndedEarly,     // the game says the level is complete, and he wasn't at the end of the route. Whatever did that, it wasn't him getting there
	kHudsonNavEndingRouteStoppedShort,   // the route has a step he can't do, so he never got to its end
	kHudsonNavEndingFatalFall,           // he fell further than the game lets a marine survive. He isn't hurt in a run, so this is the only thing that says so
	kHudsonNavEndingPlayerDied
};

// What the result file says for each of those, as ended=. A test looks for these exact words, so that a run that ended some other way can't pass it.
extern const char *const kHudsonNavEndedCrossedTheMap;
extern const char *const kHudsonNavEndedLevelDidntEnd;
extern const char *const kHudsonNavEndedLevelEndedEarly;
extern const char *const kHudsonNavEndedRouteStoppedShort;
extern const char *const kHudsonNavEndedFatalFall;
extern const char *const kHudsonNavEndedPlayerDied;

// Which ending it is, given what happened. A death comes first, so that nothing he did before it can make the run read as a good one, and then whether the game says the level is over.
HudsonNavEnding HudsonNav_WhichEnding(bool fellTooFar, bool playerDied, bool levelComplete, bool onTheLastStep, bool cameToTheEnd, bool routeStoppedShort);

// The words above for an ending, or nothing for the one that has none.
const char *HudsonNav_EndingInWords(HudsonNavEnding ending);

// What one ray of the lidar found.
enum HudsonNavRayKind
{
	kHudsonNavRayOpen,        // nothing, as far as it looks
	kHudsonNavRayFlatFloor,
	kHudsonNavRaySlope,       // angled, and he can walk on it
	kHudsonNavRayTooSteep,    // faces up, but too steep to stand on
	kHudsonNavRayWall,
	kHudsonNavRayCeiling,
	kHudsonNavRayDoor,
	kHudsonNavRayKinds        // how many kinds there are
};

// normalY is which way the surface it hit faces, from -65536 to 65536. A surface that faces up has a normalY below zero.
HudsonNavRayKind HudsonNav_ReadRay(bool hitSomething, bool hitADoor, int normalY);
const char *HudsonNav_RayWord(HudsonNavRayKind kind);

// A flat direction turned to his right by this many eighths of a full turn. x is to the right and z straight ahead, so turning 0,1 by 2 eighths gives 1,0. Any whole number of eighths will do.
void HudsonNav_TurnEighths(int x, int z, int eighths, int *turnedX, int *turnedZ);

// The places to try when looking for somewhere to stand, going outward: number 0 is the middle, and each after that is further out. Only for numbers from 0 to kHudsonNavSpiralPlaces - 1.
void HudsonNav_SpiralPlace(int number, int *acrossX, int *acrossZ);

#endif
