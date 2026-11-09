// Modified 2026-11-06 by Melanikus for the AvP port:
//   HudsonStrategies_SayTheCounts
// What Hudson can find out about a level's strategies. hudson_strategies.h says what this is for.
//
// Everything here reads what the game built from the level file, and the level file can come from anyone. So a name is
// cleaned before it goes in the log, a target that wasn't found is said to be not found, and nothing here changes the game.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "logString.h"
#include "bh_types.h"
#include "dynblock.h"
#include "bh_binary_switch.h"
#include "bh_link_switch.h"
#include "bh_lift_door.h"
#include "bh_switch_door.h"
#include "bh_platform_lift.h"
#include "bh_track.h"
#include "bh_marine.h"
#include "bh_alien.h"
#include "bh_facehugger.h"
#include "pvisible.h"
#include "hudson_plan.h"
#include "hudson_strategies.h"
#include "coop_reach.h"
#include "pldnet.h"
#include "pldghost.h"

#include <set>
#include <stdio.h>
#include <map>

extern DISPLAYBLOCK *Player;

// How far back from a door he may stand, tried in this order, measured from the door's face.
static const int kStandBackDistances[] = { 2500, 3500, 1800, 5000 };

// How far above the door's floor he starts. He then falls to the floor, so the room's floor can be a little lower.
static const int kAboveTheFloor = 500;

static const size_t kLongestNameInTheLog = 60;

static std::string CleanForTheLog(const char *text)
{
	if (!text)
	{
		return "(no name)";
	}

	std::string name;

	for (const char *c = text; *c && name.size() < kLongestNameInTheLog; c++)
	{
		// anything else could make a line of the log look like something it isn't
		name += (*c >= ' ' && *c <= '~') ? *c : '?';
	}

	return name;
}

static std::string NameForTheLog(const STRATEGYBLOCK *block)
{
	return CleanForTheLog(block->name);
}

static const char *KindOf(const STRATEGYBLOCK *block)
{
	switch (block->I_SBtype)
	{
		case I_BehaviourProximityDoor: return "proximity door";
		case I_BehaviourLiftDoor: return "lift door";
		case I_BehaviourSwitchDoor: return "switch door";
		case I_BehaviourBinarySwitch: return "switch";
		case I_BehaviourLinkSwitch: return "link switch";
		case I_BehaviourPlatform: return "platform lift";
		case I_BehaviourLift: return "lift";
		case I_BehaviourTrackObject: return "track object";
		case I_BehaviourMissionComplete: return "mission step";
		case I_BehaviourMessage: return "message";
		case I_BehaviourInanimateObject: return "object";
		case I_BehaviourPlacedLight: return "light";
		case I_BehaviourLightFX: return "light effect";
		case I_BehaviourPlacedSound: return "sound";
		case I_BehaviourFan: return "fan";
		case I_BehaviourDeathVolume: return "death volume";
		case I_BehaviourSelfDestruct: return "self-destruct";
		case I_BehaviourGenerator: return "generator";
		case I_BehaviourAlien: return "alien";
		case I_BehaviourMarine: return "marine";
		case I_BehaviourPredator: return "predator";
		case I_BehaviourDormantPredator: return "sleeping predator";
		case I_BehaviourXenoborg: return "xenoborg";
		case I_BehaviourQueenAlien: return "queen";
		case I_BehaviourFaceHugger: return "facehugger";
		case I_BehaviourAutoGun: return "sentry gun";
		case I_BehaviourParticleGenerator: return "particle generator";
		case I_BehaviourPlacedHierarchy: return "placed model";
		default: return "other";
	}
}

std::string HudsonStrategies_Title(const STRATEGYBLOCK *target)
{
	return std::string(KindOf(target)) + " \"" + NameForTheLog(target) + "\"";
}

static const char *DoorWord(int state)
{
	switch (state)
	{
		case I_door_opening: return "opening";
		case I_door_closing: return "closing";
		case I_door_open: return "open";
		case I_door_closed: return "closed";
		default: return "in an unknown state";
	}
}

std::string HudsonStrategies_State(const STRATEGYBLOCK *target)
{
	char text[120] = { 0 };

	switch (target->I_SBtype)
	{
		case I_BehaviourProximityDoor:
		{
			PROXDOOR_BEHAV_BLOCK const *const door = (PROXDOOR_BEHAV_BLOCK *)target->SBdataptr;
			_snprintf(text, sizeof(text) - 1, "%s, %s", DoorWord(door->door_state), door->door_locked ? "locked" : "not locked");
			break;
		}

		case I_BehaviourLiftDoor:
		{
			LIFT_DOOR_BEHAV_BLOCK const *const door = (LIFT_DOOR_BEHAV_BLOCK *)target->SBdataptr;
			_snprintf(text, sizeof(text) - 1, "%s, asked to be %s", DoorWord(door->door_state), DoorWord(door->request_state));
			break;
		}

		case I_BehaviourSwitchDoor:
		{
			SWITCH_DOOR_BEHAV_BLOCK const *const door = (SWITCH_DOOR_BEHAV_BLOCK *)target->SBdataptr;
			_snprintf(text, sizeof(text) - 1, "%s", DoorWord(door->doorState));
			break;
		}

		case I_BehaviourBinarySwitch:
		{
			BINARY_SWITCH_BEHAV_BLOCK const *const on = (BINARY_SWITCH_BEHAV_BLOCK *)target->SBdataptr;
			_snprintf(text, sizeof(text) - 1, "%s, asked %d", on->state ? "on" : "off", (int)on->request);
			break;
		}

		case I_BehaviourLinkSwitch:
		{
			LINK_SWITCH_BEHAV_BLOCK const *const on = (LINK_SWITCH_BEHAV_BLOCK *)target->SBdataptr;
			_snprintf(text, sizeof(text) - 1, "%s, its group %s, asked %d", on->state ? "on" : "off", on->system_state ? "on" : "off", (int)on->request);
			break;
		}

		case I_BehaviourPlatform:
		{
			PLATFORMLIFT_BEHAVIOUR_BLOCK const *const lift = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)target->SBdataptr;
			_snprintf(text, sizeof(text) - 1, "%s, state %d", lift->Enabled ? "enabled" : "disabled", (int)lift->state);
			break;
		}

		// #126: a level's facehugger can float, asleep, until a switch wakes it. Its own near state says which, and whether it has anyone to go for
		case I_BehaviourFaceHugger:
		{
			static const char *const states[] = { "approaching", "attacking", "waiting", "avoiding", "dying", "floating", "jumping", "about to jump" };
			FACEHUGGER_STATUS_BLOCK const *const hugger = (FACEHUGGER_STATUS_BLOCK *)target->SBdataptr;
			int const state = (int)hugger->nearBehaviourState;
			bool const hasTarget = hugger->Target && NAME_ISEQUAL(hugger->Target->SBname, hugger->Target_SBname);

			_snprintf(text, sizeof(text) - 1, "%s (near state %d), %s, health %d", (state >= 0 && state < (int)(sizeof(states) / sizeof(states[0]))) ? states[state] : "a state with no word",
				state, hasTarget ? "going for someone" : "going for nobody", target->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
			break;
		}

		default:
			return "a state this can't read";
	}

	return text;
}

static bool DoorIsOpenOrOpening(int state)
{
	return state == I_door_open || state == I_door_opening;
}

bool HudsonStrategies_CanGoThrough(const STRATEGYBLOCK *target)
{
	switch (target->I_SBtype)
	{
		// One of these opens by itself as anyone comes near, so it's only in the way when it's locked. A locked one that's
		// still open hasn't shut yet, so for now you could go through it.
		case I_BehaviourProximityDoor:
		{
			PROXDOOR_BEHAV_BLOCK const *const door = (PROXDOOR_BEHAV_BLOCK *)target->SBdataptr;
			return !door->door_locked || DoorIsOpenOrOpening(door->door_state);
		}

		// One of these only opens when its lift tells it to. The room list has a rule for lifts, so here it's only what the door is doing now.
		case I_BehaviourLiftDoor:
		{
			LIFT_DOOR_BEHAV_BLOCK const *const door = (LIFT_DOOR_BEHAV_BLOCK *)target->SBdataptr;
			return DoorIsOpenOrOpening(door->door_state);
		}

		// One of these opens when a switch asks, and shuts itself again about 4 seconds later (bh_switch_door.cpp)
		case I_BehaviourSwitchDoor:
		{
			SWITCH_DOOR_BEHAV_BLOCK const *const door = (SWITCH_DOOR_BEHAV_BLOCK *)target->SBdataptr;
			return DoorIsOpenOrOpening(door->doorState);
		}

		case I_BehaviourPlatform:
		{
			PLATFORMLIFT_BEHAVIOUR_BLOCK const *const lift = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)target->SBdataptr;
			return lift->Enabled != 0;
		}

		// Nothing else can shut
		default:
			return true;
	}
}

// The word the room list uses for this kind of door, or nothing if it isn't one.
static const char *DoorKindInTheRoomList(const STRATEGYBLOCK *block)
{
	switch (block->I_SBtype)
	{
		case I_BehaviourProximityDoor: return "proximity";
		case I_BehaviourSwitchDoor: return "switch";
		case I_BehaviourLiftDoor: return "lift";
		case I_BehaviourPlatform: return "platform";
		default: return 0;
	}
}

// ---- The room list: what the walking test asks about each step. coop_reach.h has all the thinking, and this only tells it what's happening in the game.

static CoopRoomList roomList;
static CoopReach roomReach;
static bool roomListIsReady = false;
static bool roomListWasCompared = false;
static std::set<std::string> roomsAlreadySaid;

// The most lines one level's room list can add to the log, so a wrong list can't fill it.
static const size_t kMostRoomListComplaints = 40;

// Says something about the room list once, however often it comes up.
static void SayOnce(std::string const &what)
{
	if (roomsAlreadySaid.size() < kMostRoomListComplaints && roomsAlreadySaid.insert(what).second)
	{
		LogString("walk: room list: " + what);
	}
}

// What each door on the room list was at the last look
static signed char const kNotSeenYet = -1;
static std::vector<signed char> doorWasOpen;

bool HudsonStrategies_LoadRoomList(const char *path)
{
	roomListIsReady = false;
	roomListWasCompared = false;
	roomsAlreadySaid.clear();
	doorWasOpen.clear();

	FILE *const file = fopen(path, "rb");

	if (!file)
	{
		LogString("walk: room list: there isn't one for this level, so no step can be checked against the doors");
		return false;
	}

	// One byte more than the limit, so a file that's too big is seen to be, and refused for it
	std::string text(kCoopMostBytes + 1, '\0');

	text.resize(fread(&text[0], 1, text.size(), file));
	fclose(file);

	std::string whyNot;

	if (!CoopReach_ReadList(text, &roomList, &whyNot))
	{
		LogString("walk: room list: refused, so no step can be checked against the doors: " + whyNot);
		return false;
	}

	CoopReach_Begin(roomList, &roomReach);
	roomListIsReady = true;

	std::string const liftWords = CoopReach_LiftWords(roomList);

	if (!liftWords.empty())
	{
		LogString("walk: room list: this level " + liftWords);
	}

	char line[120];

	_snprintf(line, sizeof(line) - 1, "walk: room list: %d rooms, %d joins, %d lifts", (int)roomList.names.size(), (int)roomList.joinCount, (int)roomList.lifts.size());
	line[sizeof(line) - 1] = 0;
	LogString(line);

	return true;
}

// The room number of where this strategy stands. A room the list doesn't have is said once, because the player being there means the list is wrong.
static int PlayersRoomNumber(const STRATEGYBLOCK *player)
{
	MODULE const *const room = player->containingModule;

	if (!room || !room->name)
	{
		return -1;
	}

	int const number = CoopReach_RoomNumber(roomList, room->name);

	if (number < 0)
	{
		SayOnce("a player is in room " + CleanForTheLog(room->name) + ", which isn't on the list");
	}

	return number;
}

void HudsonStrategies_SpreadReach()
{
	if (!roomListIsReady)
	{
		return;
	}

	size_t const rooms = roomList.names.size();
	std::vector<char> doorIsOpen(rooms, 0);
	std::vector<char> doorIsHere(rooms, 0);
	std::vector<int> playerRooms;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block)
		{
			continue;
		}

		if (block->I_SBtype == I_BehaviourNetGhost && block->SBdataptr)
		{
			NETGHOSTDATABLOCK const *const ghost = (NETGHOSTDATABLOCK *)block->SBdataptr;

			if (ghost->type == I_BehaviourMarinePlayer || ghost->type == I_BehaviourPredatorPlayer || ghost->type == I_BehaviourAlienPlayer)
			{
				playerRooms.push_back(PlayersRoomNumber(block));
			}

			continue;
		}

		char const *const kind = DoorKindInTheRoomList(block);

		if (!kind || !block->name)
		{
			continue;
		}

		int const room = CoopReach_RoomNumber(roomList, block->name);

		if (room < 0 || roomList.doorKinds[room].empty())
		{
			// The list says this isn't a door and the game says it is, so the list could be calling a shut door open
			if (!roomListWasCompared)
			{
				SayOnce(std::string(kind) + " door " + NameForTheLog(block) + " isn't a door on the list");
			}

			continue;
		}

		doorIsHere[room] = 1;
		doorIsOpen[room] = HudsonStrategies_CanGoThrough(block);

		// Said when it changes, so comparing the two machines' logs shows whether a door moved on both
		if (doorWasOpen.size() != rooms)
		{
			doorWasOpen.assign(rooms, kNotSeenYet);
		}

		signed char const now = doorIsOpen[room] ? 1 : 0;

		if (doorWasOpen[room] != kNotSeenYet && doorWasOpen[room] != now)
		{
			LogString("walk: door " + NameForTheLog(block) + (now ? " can be gone through now" : " can't be gone through now"));
		}

		doorWasOpen[room] = now;

		if (!roomListWasCompared && roomList.doorKinds[room] != kind)
		{
			SayOnce("door " + NameForTheLog(block) + " is " + kind + " in the game and " + roomList.doorKinds[room] + " on the list");
		}
	}

	if (!roomListWasCompared)
	{
		// A door on the list that this machine doesn't have stays counted as shut, which can only make the check harder
		for (size_t room = 0; room < rooms; room++)
		{
			if (!roomList.doorKinds[room].empty() && !doorIsHere[room])
			{
				SayOnce("door " + roomList.names[room] + " isn't on this machine, so it counts as shut");
			}
		}

		roomListWasCompared = true;
	}

	if (Player && Player->ObStrategyBlock)
	{
		playerRooms.push_back(PlayersRoomNumber(Player->ObStrategyBlock));
	}

	CoopReach_Spread(roomList, doorIsOpen, playerRooms, &roomReach);
}

std::string HudsonStrategies_AskReach(std::vector<std::string> const &where, std::vector<std::string> const &nearRooms, bool *routeExists)
{
	*routeExists = false;

	if (!roomListIsReady)
	{
		return "no room list";
	}

	CoopReachAnswer const answer = CoopReach_Ask(roomList, roomReach, where, nearRooms);

	*routeExists = answer.routeExists;
	return CoopReach_InWords(roomList, answer);
}

// Its middle, and how far it goes from there. A door is a whole room to the game, so its size comes from that.
static bool PlaceOf(const STRATEGYBLOCK *block, VECTORCH *middle, VECTORCH *reach)
{
	reach->vx = reach->vy = reach->vz = 0;

	if (block->SBmoptr)
	{
		MODULE const *const room = block->SBmoptr;
		middle->vx = room->m_world.vx + (room->m_minx + room->m_maxx) / 2;
		middle->vy = room->m_world.vy + (room->m_miny + room->m_maxy) / 2;
		middle->vz = room->m_world.vz + (room->m_minz + room->m_maxz) / 2;
		reach->vx = (room->m_maxx - room->m_minx) / 2;
		reach->vy = (room->m_maxy - room->m_miny) / 2;
		reach->vz = (room->m_maxz - room->m_minz) / 2;
		return true;
	}

	if (block->DynPtr)
	{
		*middle = block->DynPtr->Position;
		return true;
	}

	if (block->SBdptr)
	{
		*middle = block->SBdptr->ObWorld;
		return true;
	}

	return false;
}

static void AddTarget(std::string *text, const STRATEGYBLOCK *target, int request)
{
	char number[24] = { 0 };

	_snprintf(number, sizeof(number) - 1, " asks %d", request);
	*text += target ? HudsonStrategies_Title(target) : std::string("something not found");
	*text += number;
	*text += "; ";
}

static std::string TargetsOf(const STRATEGYBLOCK *block)
{
	std::string text;

	switch (block->I_SBtype)
	{
		case I_BehaviourBinarySwitch:
		{
			BINARY_SWITCH_BEHAV_BLOCK const *const on = (BINARY_SWITCH_BEHAV_BLOCK *)block->SBdataptr;

			for (int i = 0; i < on->num_targets; i++)
			{
				AddTarget(&text, on->bs_targets[i], on->request_messages[i]);
			}
			break;
		}

		case I_BehaviourLinkSwitch:
		{
			LINK_SWITCH_BEHAV_BLOCK const *const on = (LINK_SWITCH_BEHAV_BLOCK *)block->SBdataptr;

			for (int i = 0; i < on->num_targets; i++)
			{
				AddTarget(&text, on->ls_targets[i].sbptr, on->ls_targets[i].request_message);
			}
			break;
		}

		case I_BehaviourInanimateObject:
		{
			INANIMATEOBJECT_STATUSBLOCK const *const thing = (INANIMATEOBJECT_STATUSBLOCK *)block->SBdataptr;

			if (thing->event_target)
			{
				text += (thing->event_target->triggering_event & ObjectEventFlag_PickedUp) ? "when picked up: " : "";
				text += (thing->event_target->triggering_event & ObjectEventFlag_Destroyed) ? "when destroyed: " : "";
				AddTarget(&text, thing->event_target->event_target_sbptr, thing->event_target->request);
			}
			break;
		}

		case I_BehaviourTrackObject:
		{
			TRACK_OBJECT_BEHAV_BLOCK const *const track = (TRACK_OBJECT_BEHAV_BLOCK *)block->SBdataptr;

			if (track->destruct_target_sbptr)
			{
				text += "when destroyed: ";
				AddTarget(&text, track->destruct_target_sbptr, track->destruct_target_request);
			}
			break;
		}

		case I_BehaviourAlien:
		{
			ALIEN_STATUS_BLOCK const *const alien = (ALIEN_STATUS_BLOCK *)block->SBdataptr;

			if (alien->death_target_sbptr)
			{
				text += "when killed: ";
				AddTarget(&text, alien->death_target_sbptr, 1);
			}
			break;
		}

		case I_BehaviourMarine:
		{
			MARINE_STATUS_BLOCK const *const marine = (MARINE_STATUS_BLOCK *)block->SBdataptr;

			if (marine->death_target_sbptr)
			{
				text += "when killed: ";
				AddTarget(&text, marine->death_target_sbptr, marine->death_target_request);
			}
			break;
		}

		default:
			break;
	}

	return text;
}

void HudsonStrategies_SayTheCounts()
{
	std::map<std::string, int> counts;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block)
		{
			continue;
		}

		std::string kind = KindOf(block);

		if (block->I_SBtype == I_BehaviourNetGhost && block->SBdataptr)
		{
			NETGHOSTDATABLOCK const *const ghost = (NETGHOSTDATABLOCK const *)block->SBdataptr;

			kind = "ghost of behaviour " + std::to_string((int)ghost->type) + " subtype " + std::to_string((int)ghost->subtype);
		}
		else if (kind == "other")
		{
			kind = "behaviour " + std::to_string((int)block->I_SBtype);
		}

		counts[kind]++;
	}

	std::string text = "hudson counts: " + std::to_string(NumActiveStBlocks) + " in all";

	for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
	{
		text += ", " + it->first + " " + std::to_string(it->second);
	}

	LogString(text.c_str());
}

extern int NumActiveBlocks;

void HudsonStrategies_NoteTheCrowd()
{
	static DWORD saidAt = 0;
	static int most = 0;
	static int mostDrawn = 0;

	most = NumActiveStBlocks > most ? NumActiveStBlocks : most;
	mostDrawn = NumActiveBlocks > mostDrawn ? NumActiveBlocks : mostDrawn;

	DWORD const now = GetTickCount();

	if (saidAt == 0)
	{
		saidAt = now;
	}
	else if (now - saidAt >= 10000)
	{
		char line[140];

		_snprintf(line, sizeof(line) - 1, "hudson crowd: at most %d of %d strategy blocks and %d of %d display blocks in the last ten seconds", most, maxstblocks, mostDrawn, maxobjects);
		line[sizeof(line) - 1] = 0;
		LogString(line);
		saidAt = now;
		most = 0;
		mostDrawn = 0;
	}
}

void HudsonStrategies_ListAll()
{
	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson strategies: %d in this level", NumActiveStBlocks);
	LogString(line);

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block)
		{
			continue;
		}

		VECTORCH middle, reach;
		std::string text = "hudson strategy: " + HudsonStrategies_Title(block);

		if (PlaceOf(block, &middle, &reach))
		{
			_snprintf(line, sizeof(line) - 1, " at %d %d %d", middle.vx, middle.vy, middle.vz);
			text += line;
		}
		else
		{
			text += " with no place";
		}

		text += ": " + HudsonStrategies_State(block);

		std::string const targets = TargetsOf(block);

		if (!targets.empty())
		{
			text += ". Aims at: " + targets;
		}

		LogString(text);
	}
}

STRATEGYBLOCK *HudsonStrategies_Find(const char *word)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (block && block->name && HudsonPlan_NameMatches(block->name, word))
		{
			return block;
		}
	}

	return 0;
}

// Tries standing this far back from the middle, along one level direction, on one side. He has to end up in a room, and
// not in the door itself. Gives back true and the place if so.
static bool TryStanding(const STRATEGYBLOCK *target, const VECTORCH &middle, int floor, int alongX, int alongZ, VECTORCH *place)
{
	VECTORCH spot = { middle.vx + alongX, floor, middle.vz + alongZ };
	MODULE const *const room = ModuleFromPosition(&spot, (MODULE *)0);

	if (!room || room == target->SBmoptr)
	{
		return false;
	}

	*place = spot;
	return true;
}

bool HudsonStrategies_WhereToStand(const STRATEGYBLOCK *target, VECTORCH *place, VECTORCH *looksAt)
{
	VECTORCH middle, reach;

	if (!PlaceOf(target, &middle, &reach))
	{
		return false;
	}

	*looksAt = middle;

	int const floor = middle.vy + reach.vy - kAboveTheFloor;

	// A door is thin one way and wide the other, and the way through it is along the thin way, so that comes first.
	bool const thinAlongX = reach.vx <= reach.vz;

	for (int const distance : kStandBackDistances)
	{
		int const backX = reach.vx + distance, backZ = reach.vz + distance;

		if (thinAlongX)
		{
			if (TryStanding(target, middle, floor, backX, 0, place) || TryStanding(target, middle, floor, -backX, 0, place))
			{
				return true;
			}
		}

		if (TryStanding(target, middle, floor, 0, backZ, place) || TryStanding(target, middle, floor, 0, -backZ, place))
		{
			return true;
		}

		if (!thinAlongX && (TryStanding(target, middle, floor, backX, 0, place) || TryStanding(target, middle, floor, -backX, 0, place)))
		{
			return true;
		}
	}

	return false;
}
