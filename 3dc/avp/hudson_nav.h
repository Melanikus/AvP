// Hudson's navigation: how he walks through a level by himself. Written 2026-11-05 by Melanikus for the AvP port. See hudson_nav.cpp.

#ifndef HUDSON_NAV_H
#define HUDSON_NAV_H

#include "hudson_nav_plan.h"
#include "hudson_nav_graph.h"

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
int HudsonNav_ModulesPutInto();

// How many times his box caught on the lip of a stair and he had to shuffle forward: see HudsonNav_BoxIsCaughtOnALip.
int HudsonNav_TimesShuffled();

// How many times he was stuck, and tried a step to one side before anyone moved him.
int HudsonNav_TimesSteppedAside();

// map=grid: the next walk finds its way with the 3D grid. Called before HudsonNav_Start.
void HudsonNav_UseTheGrid();

// The grid, for anything that wants to draw it. It's empty unless the run said map=grid.
const HudsonGrid &HudsonNav_TheGrid();

// For anything else that walks by the grid. Start builds it on its own thread, and it's ready when StillBuilding says no.
void HudsonNav_StartTheGrid();
bool HudsonNav_TheGridIsStillBuilding();
HudsonGrid &HudsonNav_TheGridToSearch();

// How many times a rescue put him somewhere he couldn't stay, and he was put back: nothing to stand on, or the game pushed him out.
int HudsonNav_RescuesRefused();

// Which step of the route he's on, counting from 1, and how many there are. Both 0 with no route.
int HudsonNav_StepNow();
int HudsonNav_StepsInTheRoute();

// How far he's walked on his own feet, in metres.
int HudsonNav_MetresWalked();

// A marine who's walking has come down after falling. See Hudson_MarineLanded.
void HudsonNav_MarineLanded(const STRATEGYBLOCK *marine, int fallingSpeed, bool wouldKillHim);

// How the walk ended, once HudsonNav_EndOfFrame has said it's over. Until then, or if he's only been asked to walk so far, it's the ending that says nothing.
HudsonNavEnding HudsonNav_HowItEnded();

// How many things he's pressed with the game's own use key, how many had to be asked directly when that did nothing, and how many of all those changed.
int HudsonNav_ThingsPressed();
int HudsonNav_ThingsPressedByAsking();
int HudsonNav_PressesThatDidSomething();

// Makes his matrix a clean one, so that neither he nor the camera slowly stretches: see HudsonNav_UprightTurn.
void HudsonNav_TidyHisTurn(STRATEGYBLOCK *marine);

#endif
