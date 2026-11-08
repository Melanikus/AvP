// Hudson's navigation: how he walks through a level by himself. Written 2026-11-05 by Melanikus for the AvP port. See hudson_nav.cpp.

#ifndef HUDSON_NAV_H
#define HUDSON_NAV_H

struct strategyblock;
struct vectorch;
typedef struct strategyblock STRATEGYBLOCK;
typedef struct vectorch VECTORCH;

// Where to stand in a room: as near its middle as there's a floor with space to stand on it, by the game's own test, which reads the room's polygons and so works for a room that isn't loaded yet. room is
// the number of its AI module. place is where a marine goes to stand there, and away is how far that is from the middle. False means there's nowhere in that room.
bool HudsonNav_MiddleOfRoom(int room, VECTORCH *place, int *away);

// Starts him walking. route is the name of a route to follow, or empty to go wherever's new. run is for the marine's combat speed, in place of his walk. False if he can't: a network game, he isn't in a module with links, or the route can't be used. The log says why.
bool HudsonNav_Start(STRATEGYBLOCK *marine, int modulesWanted, const char *route, bool run);

// Holds a walk at 60 frames a second. Called once a frame while he's walking.
void HudsonNav_HoldTheFrameRate();

// At the end of each frame: sees where he's got to, decides where he goes next, and looks around with the lidar. Gives back false when there's nothing more for him to do.
bool HudsonNav_EndOfFrame(STRATEGYBLOCK *marine, long frame);

// How many new modules he reached on his own feet, and how many times he had to be moved. These are kept apart, so that being moved can never read as walking.
int HudsonNav_ModulesWalkedInto();
int HudsonNav_TimesMovedOn();

// How many things he's pressed with the game's own use key, how many had to be asked directly when that did nothing, and how many of all those changed.
int HudsonNav_ThingsPressed();
int HudsonNav_ThingsPressedByAsking();
int HudsonNav_PressesThatDidSomething();

// Makes his matrix a clean one, so that neither he nor the camera slowly stretches: see HudsonNav_UprightTurn.
void HudsonNav_TidyHisTurn(STRATEGYBLOCK *marine);

#endif
