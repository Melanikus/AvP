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
// Blank lines and lines starting with # are left out. Nothing in the file is believed: every line is checked here, and a file with anything wrong with it isn't used at all.
enum HudsonNavStepKind
{
	kHudsonNavStepWalk,
	kHudsonNavStepPress,
	kHudsonNavStepWalkInto,
	kHudsonNavStepOther
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
