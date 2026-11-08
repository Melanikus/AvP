// The game's side of the proximity wake (#130): what near_wake.cpp gives the two switches' behaviours. Written 2026-11-08 by Melanikus
// for the AvP port. near_wake.h says what the wake is and why; this needs the game's own headers included first, for STRATEGYBLOCK.

#ifndef NEAR_WAKE_GAME_H
#define NEAR_WAKE_GAME_H

#include "near_wake.h"

// Does this machine run the level's NPCs? Single player does, and so does a network game's host when it runs them. A machine that joined
// doesn't make them, so there's nothing here for it to wake.
extern bool NearWake_ThisMachineRunsNpcs(void);

// The sleeping NPCs a walk-in box wakes, directly or through a switch it sets off, each with how near a player has to come.
// boxLeast and boxMost are the box's corners, as the switch keeps them.
extern void NearWake_FindSleepers(STRATEGYBLOCK *areaSwitch, VECTORCH const *boxLeast, VECTORCH const *boxMost, NearWakeSleepers *sleepers);

// Writes the log's line for a box set off this way, up to the most a level has
extern void NearWake_SayBoxSetOff(NearWakeSleeper const &sleeper, double playerDistance);

// Forgets how many lines this level has had, for the next one. DestroyAllStrategyBlocks calls it.
extern void NearWake_LevelEnds(void);

#endif
