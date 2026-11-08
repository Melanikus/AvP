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
//      each frame (game.cpp, with the other AI systems, before any alien moves)
//        AlienWays_Frame: once the build has finished, one line in the log says how: ready, with its size and time, or why there's no map.
//        |                Then the guide (alien_ways_guide.h): aliens no longer guided get their usual gravity back, the searches carry on, and
//        |                each search that ends is one line in the log.
//      each standard alien coming for its target (AlienNearState_Approach, bh_near.cpp)
//        AlienWays_Steer: the direction of its way, if it has one, and then it climbs. If not, the game steers it as it always has.
//      level end (Destroy_CurrentEnvironment, system.cpp)
//        AlienWays_LevelEnd: ask the build to stop if it's still going, wait for its thread, let go of the map and every way
//
// ONLY APPROACH, FOR NOW: Hunt and Wander get the same call once Approach has been played.
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

// The game's own types, named here so this file needs none of the game's headers
struct strategyblock;
struct vectorch;

// For an alien that's coming for its target (AlienNearState_Approach, bh_near.cpp), heading for goal, the point the game's own code picked:
// the target itself, or the way into the next module towards it. When the target is in a module the alien's can see, the way goes to the
// target itself instead, so it can go round by the walls and ceilings all the way. Gives back true, and the direction to go in, a length of
// 65536, when the alien has a way to follow across the walls and ceilings now. False means: steer it the way the game always has. Only on the game's thread.
bool AlienWays_Steer(struct strategyblock *alien, struct strategyblock *target, const struct vectorch *goal, struct vectorch *direction);

// Whether this alien's last AlienWays_Steer gave back true. An alien on a way crawls (AlienShouldBeCrawling, bh_near.cpp).
bool AlienWays_Following(struct strategyblock *alien);

#endif
