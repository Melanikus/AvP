// What the host says about the NPCs it runs, and how every other machine draws them. Written 2026-11-06 by Melanikus for the AvP port. See npc_ghosts.cpp.

#ifndef NPC_GHOSTS_H
#define NPC_GHOSTS_H

#include "strategy_def.h"
#include "pldghost.h"
#include "pldnet.h"

// Is this a kind of ghost that stands for one of these NPCs?
bool NpcGhosts_TypeIsAnNpc(AVP_BEHAVIOUR_TYPE type);

// ---- On the host
// Fills in what the message says about this NPC. Gives back false if it isn't one of these kinds.
bool NpcGhosts_FillState(STRATEGYBLOCK *sbPtr, NETMESSAGE_NPCSTATE *state);
bool NpcGhosts_FillXenoDeltas(STRATEGYBLOCK *sbPtr, NETMESSAGE_NPCXENODELTAS *deltas);

// Which of a xenoborg's five beams are on, one bit each: see kNpcXenoBeamHead. For the real one or a ghost of one, and 0 for anything else.
int NpcGhosts_XenoBeamsOf(STRATEGYBLOCK const *sbPtr);

// ---- On every other machine. Everything these are given has come over the network, so they're hostile numbers.
// The game's behaviour for a kind that the caller has already checked is known
AVP_BEHAVIOUR_TYPE NpcGhosts_BehaviourOfKind(int kind);

// Can this kind and model number be drawn here? The number might be for a model this level never loaded, or for nothing at all.
bool NpcGhosts_ModelIsLoaded(int kind, int model);

// A marine's model number is his gun's row, and the row says whether he's an android. The number came over the network, so it may be on no row, and then he isn't.
bool NpcGhosts_MarineIsAnAndroid(int model);

// Gives a new ghost its model. Gives back false if it can't, and then no ghost should be made.
bool NpcGhosts_MakeModel(NETGHOSTDATABLOCK *ghostData, AVP_BEHAVIOUR_TYPE type, int model);

void NpcGhosts_ApplyState(STRATEGYBLOCK *ghost, NETMESSAGE_NPCSTATE const *state, VECTORCH *position, EULER *orientation);
void NpcGhosts_ApplyXenoDeltas(STRATEGYBLOCK *ghost, NETMESSAGE_NPCXENODELTAS const *deltas);
void NpcGhosts_Kill(STRATEGYBLOCK *ghost, int deathCode, int deathTime, int gibbFactor);

#endif
