// The game's side of the proximity wake (#130): which sleeping NPCs a walk-in box wakes, whether a player is in the same room or in view,
// what else a box does, and the log's lines. Written 2026-11-08 by Melanikus for the AvP port. near_wake.h says what the wake is and why.
// The players are found by the switches' behaviours themselves (bh_binary_switch.cpp and bh_link_switch.cpp), with the same test the box makes.

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
#include "pvisible.h"
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

// A block that's gone keeps its type, but its own data has been freed, so nothing of it may be read
static bool Gone(STRATEGYBLOCK const *block)
{
	return !block || block->SBflags.destroyed_but_preserved || !block->SBdataptr;
}

// If this is a sleeping NPC of a kind near_wake.h names, still asleep and not gone, says where it is and what kind it is
static bool SleeperFrom(STRATEGYBLOCK *target, NearWakePoint *where, int *kind)
{
	if (Gone(target))
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

// Adds what this target is to the list: a sleeping NPC, or, through a switch, the sleeping NPCs that switch wakes. message is the request
// that reaches it, and switchesBetween is how many switches there are already between the box and this target.
static void AddSleepersFrom(STRATEGYBLOCK *target, int message, NearWakeBox const &box, NearWakeSleepers *sleepers, int switchesBetween)
{
	NearWakePoint where;
	int kind = kNearWakeAlien;

	if (SleeperFrom(target, &where, &kind))
	{
		// A sleeper this box asks off isn't one it wakes (a dormant predator or a sentry gun doesn't wake on off), so it's left alone
		if ((message & 1) == 0)
		{
			return;
		}

		// Its block, to wake it by itself, and the module the game has it in. A dormant predator has none yet: RoomOf finds it later.
		NearWakeSleepers_Add(sleepers, where, kind, box, target, target->containingModule);
		return;
	}

	if (Gone(target) || switchesBetween >= kNearWakeMostSwitchesBetween)
	{
		return;
	}

	if (target->I_SBtype == I_BehaviourBinarySwitch)
	{
		BINARY_SWITCH_BEHAV_BLOCK const *const between = static_cast<BINARY_SWITCH_BEHAV_BLOCK const *>(target->SBdataptr);

		for (int i = 0; i < between->num_targets; i++)
		{
			AddSleepersFrom(between->bs_targets[i], between->request_messages[i], box, sleepers, switchesBetween + 1);
		}
	}
	else if (target->I_SBtype == I_BehaviourLinkSwitch)
	{
		LINK_SWITCH_BEHAV_BLOCK const *const between = static_cast<LINK_SWITCH_BEHAV_BLOCK const *>(target->SBdataptr);

		for (int i = 0; i < between->num_targets; i++)
		{
			AddSleepersFrom(between->ls_targets[i].sbptr, between->ls_targets[i].request_message, box, sleepers, switchesBetween + 1);
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
			AddSleepersFrom(own->bs_targets[i], own->request_messages[i], box, sleepers, 0);
		}
	}
	else if (areaSwitch->I_SBtype == I_BehaviourLinkSwitch)
	{
		LINK_SWITCH_BEHAV_BLOCK const *const own = static_cast<LINK_SWITCH_BEHAV_BLOCK const *>(areaSwitch->SBdataptr);

		for (int i = 0; i < own->num_targets; i++)
		{
			AddSleepersFrom(own->ls_targets[i].sbptr, own->ls_targets[i].request_message, box, sleepers, 0);
		}
	}

	if (sleepers->full && !saidABoxWakesTooMany)
	{
		saidABoxWakesTooMany = true;
		LogString(NearWake_FullLine());
	}
}

// The module a sleeper is in. A dormant predator has no body yet, so the game hasn't put it in one: it's found from its place, starting the
// search from the module given, which is only a place to start.
static MODULE *RoomOf(NearWakeSleeper const &sleeper, MODULE *startFrom)
{
	if (sleeper.room)
	{
		return static_cast<MODULE *>(sleeper.room);
	}

	VECTORCH place = { static_cast<int>(sleeper.where.x), static_cast<int>(sleeper.where.y), static_cast<int>(sleeper.where.z) };
	return ModuleFromPosition(&place, startFrom);
}

// Is the second module the first one, or one the game sees from it? IsModuleVisibleFromModule reads the first module's own list of what it
// sees, so that has to be there.
static bool SameOrInView(MODULE *from, MODULE *other)
{
	if (!from || !other)
	{
		return false;
	}

	return from == other || (from->m_vmptr && IsModuleVisibleFromModule(from, other));
}

bool NearWake_InTheRoomOrInView(NearWakeSleeper const &sleeper, MODULE *playerModule)
{
	return SameOrInView(RoomOf(sleeper, playerModule), playerModule);
}

// How far a sleeper is from a thing in the level: from the box of the module it is, for a door or a track that's a module, or else from
// where it is. Something with neither is taken to be far away.
static double DistanceFrom(NearWakeSleeper const &sleeper, STRATEGYBLOCK const *thing)
{
	if (thing->SBmoptr)
	{
		MODULE const *const module = thing->SBmoptr;
		NearWakeBox box;

		box.least = NearWake_Point(static_cast<double>(module->m_world.vx) + module->m_minx, static_cast<double>(module->m_world.vy) + module->m_miny,
			static_cast<double>(module->m_world.vz) + module->m_minz);
		box.most = NearWake_Point(static_cast<double>(module->m_world.vx) + module->m_maxx, static_cast<double>(module->m_world.vy) + module->m_maxy,
			static_cast<double>(module->m_world.vz) + module->m_maxz);
		return NearWake_DistanceToBox(sleeper.where, box);
	}

	if (thing->DynPtr)
	{
		return NearWake_Distance(sleeper.where, PointOf(thing->DynPtr->Position));
	}

	return kNearWakeMostJobDistance + 1.0;
}

// The jobs of a box's targets, as near_wake.h sorts them, besides waking the sleeper a player came near
struct Jobs
{
	int list[kNearWakeMostJobs];
	int count = 0;
	bool tooMany = false;
};

static void Note(Jobs *jobs, int job)
{
	if (jobs->count >= kNearWakeMostJobs)
	{
		jobs->tooMany = true;
		return;
	}

	jobs->list[jobs->count++] = job;
}

// Sorts what one target does when it's sent message, the request the box would send it, besides waking the sleeper given
static void SortJob(STRATEGYBLOCK *target, int message, NearWakeSleeper const &sleeper, int switchesBetween, Jobs *jobs)
{
	// A target that isn't in the level, or has gone, does nothing when it's asked
	if (Gone(target))
	{
		return;
	}

	bool const on = (message & 1) != 0;

	switch (target->I_SBtype)
	{
		// The NPCs a box wakes, asked on. One that's awake already does nothing more when it's asked. Asked off is something else: off
		// switches an active sentry gun off.
		case I_BehaviourAlien:
		case I_BehaviourFaceHugger:
		case I_BehaviourDormantPredator:
		case I_BehaviourPredator:
		case I_BehaviourAutoGun:
			Note(jobs, on ? kNearWakeJobWakesASleeper : kNearWakeJobAnythingElse);
			return;

		// A door, where "on" opens it or unlocks a proximity door and "off" shuts or locks it; or something on a track, where "on" starts it.
		// Asked on and near the sleeper, it's taken to open the way to it, such as hugger1's locker door on Sulaco.
		case I_BehaviourProximityDoor:
		case I_BehaviourSwitchDoor:
		case I_BehaviourLiftDoor:
		case I_BehaviourTrackObject:
			Note(jobs, on && DistanceFrom(sleeper, target) <= kNearWakeMostJobDistance ? kNearWakeJobOpensTheWay : kNearWakeJobAnythingElse);
			return;

		// A switch between: what its own targets do, as far as kNearWakeMostSwitchesBetween goes
		case I_BehaviourBinarySwitch:
		case I_BehaviourLinkSwitch:
		{
			if (switchesBetween >= kNearWakeMostSwitchesBetween)
			{
				Note(jobs, kNearWakeJobAnythingElse);
				return;
			}

			if (target->I_SBtype == I_BehaviourBinarySwitch)
			{
				BINARY_SWITCH_BEHAV_BLOCK const *const between = static_cast<BINARY_SWITCH_BEHAV_BLOCK const *>(target->SBdataptr);

				for (int i = 0; i < between->num_targets; i++)
				{
					SortJob(between->bs_targets[i], between->request_messages[i], sleeper, switchesBetween + 1, jobs);
				}
			}
			else
			{
				LINK_SWITCH_BEHAV_BLOCK const *const between = static_cast<LINK_SWITCH_BEHAV_BLOCK const *>(target->SBdataptr);

				for (int i = 0; i < between->num_targets; i++)
				{
					SortJob(between->ls_targets[i].sbptr, between->ls_targets[i].request_message, sleeper, switchesBetween + 1, jobs);
				}
			}

			return;
		}

		default:
			Note(jobs, kNearWakeJobAnythingElse);
			return;
	}
}

bool NearWake_BoxOnlyOpensTheWay(STRATEGYBLOCK *areaSwitch, NearWakeSleeper const &sleeper)
{
	if (Gone(areaSwitch))
	{
		return false;
	}

	Jobs jobs;

	if (areaSwitch->I_SBtype == I_BehaviourBinarySwitch)
	{
		BINARY_SWITCH_BEHAV_BLOCK const *const own = static_cast<BINARY_SWITCH_BEHAV_BLOCK const *>(areaSwitch->SBdataptr);

		for (int i = 0; i < own->num_targets; i++)
		{
			SortJob(own->bs_targets[i], own->request_messages[i], sleeper, 0, &jobs);
		}
	}
	else if (areaSwitch->I_SBtype == I_BehaviourLinkSwitch)
	{
		LINK_SWITCH_BEHAV_BLOCK const *const own = static_cast<LINK_SWITCH_BEHAV_BLOCK const *>(areaSwitch->SBdataptr);

		for (int i = 0; i < own->num_targets; i++)
		{
			SortJob(own->ls_targets[i].sbptr, own->ls_targets[i].request_message, sleeper, 0, &jobs);
		}
	}
	else
	{
		return false;
	}

	return NearWake_OnlyOpensTheWay(jobs.list, jobs.count, jobs.tooMany);
}

// A log line about this, up to the most a level has
static void Say(std::string const &line)
{
	if (linesThisLevel >= kNearWakeMostLines)
	{
		return;
	}

	LogString(line);

	if (++linesThisLevel == kNearWakeMostLines)
	{
		LogString(NearWake_LinesStop());
	}
}

void NearWake_SayBoxSetOff(NearWakeSleeper const &sleeper, double playerDistance)
{
	Say(NearWake_Line(sleeper, playerDistance));
}

void NearWake_WakeItAlone(STRATEGYBLOCK *areaSwitch, NearWakeSleeper const &sleeper, double playerDistance)
{
	STRATEGYBLOCK *const npc = static_cast<STRATEGYBLOCK *>(sleeper.who);

	// It was a sleeper this frame, but nothing is read of a block that's gone. RequestState wakes each kind the way its box would have.
	if (Gone(npc))
	{
		return;
	}

	RequestState(npc, 1, areaSwitch);
	Say(NearWake_WokeAloneLine(sleeper, playerDistance));
}

void NearWake_LevelEnds(void)
{
	linesThisLevel = 0;
	saidABoxWakesTooMany = false;
}
