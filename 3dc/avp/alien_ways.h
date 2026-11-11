// The game's side of the alien ways map (alien_ways_map.h): building it at each level's start, letting it go at the end, and saying in the
// log how that went. Written 2026-11-08 by Melanikus for the AvP port ("For Aliens, Floor is lava", #109 and #118).
//
// How all of this works, in plain words: ALIEN-WAYS.md, beside this file.
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
//      each standard alien that's coming for its target, hunting it or wandering (AlienNearState_Approach, _Hunt and _Wander, bh_near.cpp)
//        AlienWays_Steer: the direction of its way, if it has one, and then it climbs. If not, the game steers it as it always has.
//      after each near alien's state has had its turn (NearAlienBehaviour, bh_near.cpp)
//        AlienWays_KeepDriving: an alien on its way whose state didn't ask this frame is kept going along it, holding on
//      level restart (RestartLevel, system.cpp)
//        AlienWays_LevelRestarts: every alien's way goes, the map stays
//      level end (Destroy_CurrentEnvironment, system.cpp)
//        AlienWays_LevelEnd: ask the build to stop if it's still going, wait for its thread, let go of the map and every way
#ifndef ALIEN_WAYS_H
#define ALIEN_WAYS_H

#include <stddef.h>

#include "alien_ways_map.h"
#include "alien_ways_lookahead.h"

// 1 when -alienways was given with -dev. Set once, at start, by winmain.cpp.
extern int AlienWaysSwitchedOn;

// 1 when -alienwaysgrid was given too (with -dev and -alienways). Then the aliens search the grid every NPC will share (npc_grid.h) instead
// of the polygon map. The map is still built, first, and is what they search when the grid can't be had. Each level's build makes the grid
// right after the map, on the same thread, and the guide is told each frame which one to search (alien_ways_guide.h, UseGrid). Set once,
// at start, by winmain.cpp.
extern int AlienWaysGridSwitchedOn;

// Modified 2026-11-08 by Melanikus for the AvP port: the mover (#149).
// 1 when -alienwaysmover was given too (with -dev, -alienways and -alienwaysgrid). Then where an alien heads is picked by alien ways itself,
// the same in Approach, Hunt and Wander (its target, where it last saw it, or the strongest scent of the marines), and with no way to follow
// it heads straight for that goal instead of the game's own steering. How, in alien_ways.cpp above AlienWays_Steer. Its own switch, so one
// exe can film the grid with the mover and without it. Set once, at start, by winmain.cpp.
extern int AlienWaysMoverSwitchedOn;

// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109; PO's decision R-LAVA-P2): 1 when -alienwaysview was given too (with -dev
// and -alienways). Then each alien's way is drawn over the level as coloured lines (alien_ways_view.h, drawn by d3d_render.cpp), for films.
// Its own switch, so the lines stay a developer's tool when the ways are on for every player: no player ever sees them. Set once, at start,
// by winmain.cpp.
extern int AlienWaysViewSwitchedOn;

// How far along its way an alien aims, in mm, so it curves round bends instead of running corner to corner (alien_ways_lookahead.h).
// kAlienWaysAimAheadMm, 1.5 m, unless -alienwaysaim=N gives another, from 0 to 10000; 0 aims at the way's next point, as before, for a film
// to compare the two. Set once, at start, by winmain.cpp; the guide is given it at each level's start.
extern int AlienWaysAimAheadMm;

// Modified 2026-11-08 by Melanikus for the AvP port: a watch on one alien, every frame (#109).
// -alienwayswatch=N, with -dev, picks the alien with the guide's place N, from 0 to kAlienWaysMostWatchPlace (the guide's
// kAlienWaysMostFollowers - 1). Each frame that alien gets one line in the log: its state, what that state did (AlienWays_WatchNote), whether
// the alien ways were asked and answered, whether the guide kept driving it (AlienWays_KeepDriving), its grip, its speed, whether it has a
// target, and the marine scent in its AI module and the ones next to it, which is what Hunt follows. -1, the default, watches nothing. At most
// kAlienWaysMostWatchLines lines a level, so a long run can't fill the log. Set once, at start, by winmain.cpp.
extern int AlienWaysWatchPlace;
int const kAlienWaysMostWatchPlace = 63;
int const kAlienWaysMostWatchLines = 6000;   // 100 seconds of frames at 60 a second

void AlienWays_LevelStart();
void AlienWays_Frame();
void AlienWays_LevelEnd();

// The level restarts (RestartLevel, system.cpp): every alien is made again, so nothing the guide knew of the ones before is kept, their ways
// and claims included. The map and the doors stay, because the level's shape is the same. Secondary's read of 9e8c30fb.
void AlienWays_LevelRestarts();

// One alien's way, for the developer view to draw (alien_ways_view.h): its points from where it started to where it's going, each one's kind
// saying what the way is on from there to the next, and which point the alien is heading for now.
struct AlienWaysShown
{
	const AlienWaysPoint *points;
	int count;
	int heading;
	bool kept;   // the last way of an alien that isn't following it now, drawn dimmed (added 2026-11-11 by Melanikus (Third), #109)
};

// Fills out with up to most of the ways aliens are following now, then the last ways of aliens that aren't (kept), and gives back how many. Only on the game's thread. The points are good until
// the next AlienWays_Frame, so they're drawn in the same frame and never kept. Gives back 0 while there's no map.
size_t AlienWays_WaysToShow(AlienWaysShown *out, size_t most);

// The game's own types, named here so this file needs none of the game's headers
struct strategyblock;
struct vectorch;

// For an alien that's coming for its target, hunting it or wandering (AlienNearState_Approach, _Hunt and _Wander, bh_near.cpp), heading for
// goal, the point the game's own code picked:
// the target itself, or the way into the next module towards it. When the target is in a module the alien's can see, the way goes to the
// target itself instead, so it can go round by the walls and ceilings all the way. Gives back true, and the direction to go in, a length of
// 65536, when the alien has a way to follow across the walls and ceilings now. False means: steer it the way the game always has. Only on the game's thread.
bool AlienWays_Steer(struct strategyblock *alien, struct strategyblock *target, const struct vectorch *goal, struct vectorch *direction);

// Whether this alien's last AlienWays_Steer gave back true. An alien on a way crawls (AlienShouldBeCrawling, bh_near.cpp).
bool AlienWays_Following(struct strategyblock *alien);

// Whether AlienWays_Steer was asked for this alien in this frame and gave back true: its way steers it now. False with the switch off, and
// in a frame nothing asked. Hunt and Wander (bh_near.cpp) use it to hold back the game's own avoidance only while a way really steers them.
bool AlienWays_GuidedThisFrame(struct strategyblock *alien);

// Whether the game's own avoidance is to be held back for this alien although no way steers it: it rests after it was found stuck, or
// waits for a re-plan's way (THE REST, HELD FROM AVOIDING, alien_ways_guide.h). Approach, Hunt and Wander (bh_near.cpp) hold back their
// avoidance then, as they do while a way steers them. False with the switch off. Added 2026-11-11 by Melanikus (Third) for the AvP port
// (#109; Primary's placed runs: in that rest, 1999's Avoidance carried an alien from 19 m off its target to 37 m).
bool AlienWays_HoldsBackAvoiding(struct strategyblock *alien);

// The guide's place for this alien: the N of the log's "alien N" lines, so another line can name the same alien. -1 when it has none, and
// with the switch off. A place is given again once its alien is let go, so it names an alien only together with when. Added 2026-11-11 by
// Melanikus (Third) for the AvP port, for Hudson's per-alien lines (#109).
int AlienWays_PlaceOf(const struct strategyblock *alien);

// On a frame its way steers an alien, the game's own tests that would send it off to avoid something are kept from acting, because running
// into a wall is how it climbs (AlienNearState_Approach, _Hunt and _Wander, bh_near.cpp). When one of them would have acted, this says so in
// the log: which one,
// where the alien is, and its module, at most once a second for each alien and kAlienWaysMostWouldHaveLines times a level. turnedBack means
// the test for turning back (NPC_CannotReachTarget); otherwise it's the test for running into something (NPC_IsObstructed).
void AlienWays_WouldHaveAvoided(struct strategyblock *alien, bool turnedBack);

// Modified 2026-11-08 by Melanikus for the AvP port: keeps an alien going along its way on a frame its state didn't ask (#109).
// Asked once a frame for each near alien, after its state has run (NearAlienBehaviour, bh_near.cpp). For an alien in Approach, Hunt or Wander
// that wasn't asked about this frame, but that the guide still keeps (it last asked within half a second), and that has a way it's on, it
// gives back true and the direction along that way, a length of 65536 (AlienWaysGuide::Drive). The game then keeps it holding on to what
// it's on and sets its speed, as a state on its way does. False means: nothing to do, the frame stays as its state left it.
bool AlienWays_KeepDriving(struct strategyblock *alien, struct vectorch *direction);

// The watch on one alien (AlienWaysWatchPlace). WatchNote adds a few words to this frame's line for that alien, saying what its state did;
// for any other alien, or with the watch off, it does nothing. WatchLine writes the line at the end of the alien's frame, with hasTarget and
// keptDriving from the game, and starts the next.
void AlienWays_WatchNote(struct strategyblock *alien, const char *note);
void AlienWays_WatchLine(struct strategyblock *alien, bool hasTarget, bool keptDriving);

#endif
