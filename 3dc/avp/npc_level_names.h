// The names placed NPCs have in a level, for a network game. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHY THIS IS HERE: everything in a level aims at things by name: a switch that wakes an alien, for example. But in a network game a placed NPC gets a new name, because
// the network messages use part of it to say which NPC they mean. So the switch's name found nothing, and the alien stayed asleep for ever. On the stock levels that's 195
// switch links. Each NPC's old and new name are kept here, so that anything looking for the old name can be sent to the new one.
//
// The names come from a level file, which is treated as hostile: this list has a fixed size, and one too many is refused and not written.
//
// ONE LIST FOR ONE SET OF NPCs: a level name can be in the list more than once, and the first is the one found. So the list has to be emptied
// whenever the NPCs it names are destroyed, before they're made again: strategy_def.cpp does that in DestroyAllStrategyBlocks, which a level's
// end and a restart both run. Until 2026-11-08 it was emptied only when the program started. After a co-op restart a switch then found the
// first run's name, which nothing had any more, and the facehugger it wakes floated for ever (#126).
//
// Modified 2026-11-08 by Melanikus for the AvP port:
//   says when the list has to be emptied, and why (#126)
//   a lookup by a name in the level is here, NpcLevelNames_FindByLevelName, and it reads the list once at most (#131)
//
// This file knows nothing about the game, so tests/npc_level_names_test.cpp can try all of it.

#ifndef NPC_LEVEL_NAMES_H
#define NPC_LEVEL_NAMES_H

#include <string.h>

enum
{
	kNpcNameLength = 8,		// the same as SB_NAME_LENGTH. strategy_def.cpp checks that.
	kMostNpcLevelNames = 2048
};

struct NpcLevelName
{
	char level[kNpcNameLength];
	char network[kNpcNameLength];
};

struct NpcLevelNames
{
	int count;
	bool saidItIsFull;
	NpcLevelName names[kMostNpcLevelNames];
};

static inline void NpcLevelNames_Clear(NpcLevelNames *list)
{
	list->count = 0;
	list->saidItIsFull = false;
}

static inline bool NpcLevelName_IsEmpty(char const *name)
{
	for (int i = 0; i < kNpcNameLength; i++)
	{
		if (name[i])
		{
			return false;
		}
	}

	return true;
}

// Gives back false only the first time the list is too full to hold one, so that the game logs it once. A name of all zeros means no name, so nothing can ever ask for it,
// and it isn't kept.
static inline bool NpcLevelNames_Add(NpcLevelNames *list, char const *level, char const *network)
{
	if (NpcLevelName_IsEmpty(level))
	{
		return true;
	}

	if (list->count >= kMostNpcLevelNames)
	{
		bool const firstTime = !list->saidItIsFull;

		list->saidItIsFull = true;
		return !firstTime;
	}

	memcpy(list->names[list->count].level, level, kNpcNameLength);
	memcpy(list->names[list->count].network, network, kNpcNameLength);
	list->count++;
	return true;
}

// The new name of the NPC with this name in the level, or nothing. A level's name that's the same as its own new name isn't an answer either.
static inline char const *NpcLevelNames_NetworkNameFor(NpcLevelNames const *list, char const *level)
{
	if (NpcLevelName_IsEmpty(level))
	{
		return 0;
	}

	for (int i = 0; i < list->count; i++)
	{
		if (memcmp(list->names[i].level, level, kNpcNameLength) == 0)
		{
			return memcmp(list->names[i].network, level, kNpcNameLength) == 0 ? 0 : list->names[i].network;
		}
	}

	return 0;
}

// Finds something by a name from the level, the way the game does it (FindSBWithName in strategy_def.cpp calls this). findLive looks among
// the things that exist now, and gives back what has that name, or nothing. In a network game a name that finds nothing may be a placed
// NPC's name in the level: then its new name is looked for, among the things that exist, and that's the end of it. The list is read once
// at most, and never again for the new name, so a lookup always ends, whatever names a level file holds.
//
// It couldn't go round before this either, but only because of a rule kept far from here: every id a level gives anything ends in a 0 byte
// (each CalculateID: chunk_object.cpp, chunk_sound.cpp, avp_chunk.cpp, strategy_chunk.cpp), and a new name ends in 10 + the player's place,
// so 9 at the least (AssignNewSBName in strategy_def.cpp). So a new name is never a name in the level, and the list never leads from a new
// name anywhere. This doesn't count on that rule, and tests/npc_level_names_test.cpp checks the rule is still kept.
template <typename FindLive>
static inline auto NpcLevelNames_FindByLevelName(NpcLevelNames const *list, char const *name, bool networkGame, FindLive findLive) -> decltype(findLive(name))
{
	// A name that's all zeros is no name. Many things made while the game runs have one, so looking for it would find one of them. So
	// it's never looked for, and that goes for a new name in the list too, though nothing writes one: AssignNewSBName's last byte is never 0.
	decltype(findLive(name)) const nothing = 0;

	if (NpcLevelName_IsEmpty(name))
	{
		return nothing;
	}

	auto const found = findLive(name);

	if (found || !networkGame)
	{
		return found;
	}

	char const *const newName = NpcLevelNames_NetworkNameFor(list, name);

	return newName && !NpcLevelName_IsEmpty(newName) ? findLive(newName) : nothing;
}

// The name in the level of the NPC with this new name, or nothing if it didn't come from the level's list
static inline char const *NpcLevelNames_LevelNameFor(NpcLevelNames const *list, char const *network)
{
	for (int i = 0; i < list->count; i++)
	{
		if (memcmp(list->names[i].network, network, kNpcNameLength) == 0)
		{
			return list->names[i].level;
		}
	}

	return 0;
}

#endif
