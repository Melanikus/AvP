// The game's side of the proximity wake (#130): what near_wake.cpp gives the two switches' behaviours. Written 2026-11-08 by Melanikus
// for the AvP port. near_wake.h says what the wake is and why. This needs the game's own headers included first, for STRATEGYBLOCK and MODULE.

#ifndef NEAR_WAKE_GAME_H
#define NEAR_WAKE_GAME_H

#include "near_wake.h"

// Does this machine run the level's NPCs? Single player does, and so does a network game's host when it runs them. A machine that joined
// doesn't make them, so there's nothing here for it to wake.
extern bool NearWake_ThisMachineRunsNpcs(void);

// The sleeping NPCs a walk-in box wakes, directly or through a switch it sets off, each with how near a player has to come.
// boxLeast and boxMost are the box's corners, as the switch keeps them.
extern void NearWake_FindSleepers(STRATEGYBLOCK *areaSwitch, VECTORCH const *boxLeast, VECTORCH const *boxMost, NearWakeSleepers *sleepers);

// Is a player in this module in the sleeper's own module, or one the game sees from it? The user's "same room or in view".
extern bool NearWake_InTheRoomOrInView(NearWakeSleeper const &sleeper, MODULE *playerModule);

// Does everything else the box does just open the way to this sleeper? The user's "only safe jobs": then the whole box may be set off.
extern bool NearWake_BoxOnlyOpensTheWay(STRATEGYBLOCK *areaSwitch, NearWakeSleeper const &sleeper);

// Writes the log's line for a box set off this way, up to the most a level has
extern void NearWake_SayBoxSetOff(NearWakeSleeper const &sleeper, double playerDistance);

// Wakes this sleeper by itself, as its box would have, and says so in the log, when the box does other things that have to wait
extern void NearWake_WakeItAlone(STRATEGYBLOCK *areaSwitch, NearWakeSleeper const &sleeper, double playerDistance);

// Forgets how many lines this level has had, for the next one. DestroyAllStrategyBlocks calls it.
extern void NearWake_LevelEnds(void);

#endif
