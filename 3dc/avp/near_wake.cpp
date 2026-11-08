// The game's side of the proximity wake (#130): which sleeping NPCs a walk-in box wakes, and the log's lines. Written 2026-11-08 by
// Melanikus for the AvP port. near_wake.h says what the wake is and why. The players are found by the switches' behaviours themselves
// (bh_binary_switch.cpp and bh_link_switch.cpp), with the same test the box makes.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "bh_alien.h"
#include "bh_facehugger.h"
#include "bh_predator.h"
#include "bh_agun.h"
#include "bh_binary_switch.h"
#include "bh_link_switch.h"
#include "logString.h"
#include "near_wake_game.h"

// Lines this level has had about boxes set off this way, and whether it has had the one about a box that wakes too many
static int linesThisLevel = 0;
static bool saidABoxWakesTooMany = false;

static NearWakePoint PointOf(VECTORCH const &place)
{
	return NearWake_Point(place.vx, place.vy, place.vz);
}

bool NearWake_ThisMachineRunsNpcs(void)
{
	return AvP.Network == I_No_Network || AvP.NetworkAIServer;
}

// If this is a sleeping NPC of a kind near_wake.h names, still asleep and not gone, says where it is and what kind it is
static bool SleeperFrom(STRATEGYBLOCK *target, NearWakePoint *where, int *kind)
{
	// A block that's gone keeps its type, but its own data has been freed
	if (!target || target->SBflags.destroyed_but_preserved || !target->SBdataptr)
	{
		return false;
	}

	switch (target->I_SBtype)
	{
		case I_BehaviourAlien:
		{
			ALIEN_STATUS_BLOCK const *const alien = static_cast<ALIEN_STATUS_BLOCK const *>(target->SBdataptr);

			if (alien->BehaviourState != ABS_Dormant || !target->DynPtr)
			{
				return false;
			}

			*where = PointOf(target->DynPtr->Position);
			*kind = kNearWakeAlien;
			return true;
		}

		case I_BehaviourFaceHugger:
		{
			FACEHUGGER_STATUS_BLOCK const *const hugger = static_cast<FACEHUGGER_STATUS_BLOCK const *>(target->SBdataptr);

			if (hugger->nearBehaviourState != FHNS_Floating || !target->DynPtr)
			{
				return false;
			}

			*where = PointOf(target->DynPtr->Position);
			*kind = kNearWakeFacehugger;
			return true;
		}

		case I_BehaviourDormantPredator:
		{
			// A dormant predator is only its level data until it wakes: it has no body yet, so its place is the one the level gives
			DORMANT_PREDATOR_STATUS_BLOCK const *const predator = static_cast<DORMANT_PREDATOR_STATUS_BLOCK const *>(target->SBdataptr);

			*where = PointOf(predator->toolsData.position);
			*kind = kNearWakePredator;
			return true;
		}

		case I_BehaviourAutoGun:
		{
			AUTOGUN_STATUS_BLOCK const *const gun = static_cast<AUTOGUN_STATUS_BLOCK const *>(target->SBdataptr);

			if (gun->behaviourState != I_inactive || !target->DynPtr)
			{
				return false;
			}

			*where = PointOf(target->DynPtr->Position);
			*kind = kNearWakeSentryGun;
			return true;
		}

		default:
			return false;
	}
}

// Adds what this target is to the list: a sleeping NPC, or, through a switch, the sleeping NPCs that switch wakes. switchesBetween is how
// many switches there are already between the box and this target.
static void AddSleepersFrom(STRATEGYBLOCK *target, NearWakeBox const &box, NearWakeSleepers *sleepers, int switchesBetween)
{
	NearWakePoint where;
	int kind = kNearWakeAlien;

	if (SleeperFrom(target, &where, &kind))
	{
		NearWakeSleepers_Add(sleepers, where, kind, box);
		return;
	}

	if (!target || target->SBflags.destroyed_but_preserved || !target->SBdataptr || switchesBetween >= kNearWakeMostSwitchesBetween)
	{
		return;
	}

	if (target->I_SBtype == I_BehaviourBinarySwitch)
	{
		BINARY_SWITCH_BEHAV_BLOCK const *const between = static_cast<BINARY_SWITCH_BEHAV_BLOCK const *>(target->SBdataptr);

		for (int i = 0; i < between->num_targets; i++)
		{
			AddSleepersFrom(between->bs_targets[i], box, sleepers, switchesBetween + 1);
		}
	}
	else if (target->I_SBtype == I_BehaviourLinkSwitch)
	{
		LINK_SWITCH_BEHAV_BLOCK const *const between = static_cast<LINK_SWITCH_BEHAV_BLOCK const *>(target->SBdataptr);

		for (int i = 0; i < between->num_targets; i++)
		{
			AddSleepersFrom(between->ls_targets[i].sbptr, box, sleepers, switchesBetween + 1);
		}
	}
}

void NearWake_FindSleepers(STRATEGYBLOCK *areaSwitch, VECTORCH const *boxLeast, VECTORCH const *boxMost, NearWakeSleepers *sleepers)
{
	NearWakeSleepers_Clear(sleepers);

	if (!areaSwitch || !areaSwitch->SBdataptr)
	{
		return;
	}

	NearWakeBox box;

	box.least = PointOf(*boxLeast);
	box.most = PointOf(*boxMost);

	// The box's own targets come first. Each is either a sleeper, or a switch whose targets are looked at in turn.
	if (areaSwitch->I_SBtype == I_BehaviourBinarySwitch)
	{
		BINARY_SWITCH_BEHAV_BLOCK const *const own = static_cast<BINARY_SWITCH_BEHAV_BLOCK const *>(areaSwitch->SBdataptr);

		for (int i = 0; i < own->num_targets; i++)
		{
			AddSleepersFrom(own->bs_targets[i], box, sleepers, 0);
		}
	}
	else if (areaSwitch->I_SBtype == I_BehaviourLinkSwitch)
	{
		LINK_SWITCH_BEHAV_BLOCK const *const own = static_cast<LINK_SWITCH_BEHAV_BLOCK const *>(areaSwitch->SBdataptr);

		for (int i = 0; i < own->num_targets; i++)
		{
			AddSleepersFrom(own->ls_targets[i].sbptr, box, sleepers, 0);
		}
	}

	if (sleepers->full && !saidABoxWakesTooMany)
	{
		saidABoxWakesTooMany = true;
		LogString(NearWake_FullLine());
	}
}

void NearWake_SayBoxSetOff(NearWakeSleeper const &sleeper, double playerDistance)
{
	if (linesThisLevel >= kNearWakeMostLines)
	{
		return;
	}

	LogString(NearWake_Line(sleeper, playerDistance));

	if (++linesThisLevel == kNearWakeMostLines)
	{
		LogString(NearWake_LinesStop());
	}
}

void NearWake_LevelEnds(void)
{
	linesThisLevel = 0;
	saidABoxWakesTooMany = false;
}
