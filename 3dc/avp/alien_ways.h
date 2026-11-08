// The game's side of the alien ways map (alien_ways_map.h): building it at each level's start, letting it go at the end, and saying in the
// log how that went. Written 2026-11-08 by Melanikus for the AvP port ("For Aliens, Floor is lava", #109 and #118).
//
// WHILE IT'S BEING BUILT AND FILMED it's only on with the developer switch -alienways, which needs -dev (winmain.cpp). When it ships it's
// always on, in every mode, with no setting for players: that's the user's ruling, and it's the one line in AlienWays_On to change.
//
// ONLY WHERE THE AI RUNS: in single player, and on the machine that runs the AI in a network game (AvP.NetworkAIServer). Another player's
// machine only draws the aliens the host sends it, so it has no use for a map.
//
// A LEVEL'S LIFE:
//
//      level start (StartGame, game.cpp; or a lift into another environment, IntegrateNewEnvironment, system.cpp)
//        AlienWays_LevelStart: list the level's polygons here, on the game's thread (alien_ways_level.h), and start the build on its own
//        |                      thread (alien_ways_build.h). Aliens move as they always have.
//      each frame (game.cpp, with the other AI systems)
//        AlienWays_Frame: once the build has finished, one line in the log says how: ready, with its size and time, or why there's no map
//      level end (Destroy_CurrentEnvironment, system.cpp)
//        AlienWays_LevelEnd: ask the build to stop if it's still going, wait for its thread, let go of the map
//
// NOTHING USES THE MAP YET. The aliens' side, finding a way and following it, is the next step.
#ifndef ALIEN_WAYS_H
#define ALIEN_WAYS_H

#include <stddef.h>

#include "alien_ways_map.h"

// 1 when -alienways was given with -dev. Set once, at start, by winmain.cpp.
extern int AlienWaysSwitchedOn;

void AlienWays_LevelStart();
void AlienWays_Frame();
void AlienWays_LevelEnd();

// One alien's way, for the developer view to draw (alien_ways_view.h): its points from where it started to where it's going, each one's kind
// saying what the way is on from there to the next, and which point the alien is heading for now.
struct AlienWaysShown
{
	const AlienWaysPoint *points;
	int count;
	int heading;
};

// Fills out with up to most of the ways aliens are following now, and gives back how many. Only on the game's thread. The points are good until
// the next AlienWays_Frame, so they're drawn in the same frame and never kept. Gives back 0 while there's no map.
size_t AlienWays_WaysToShow(AlienWaysShown *out, size_t most);

#endif
