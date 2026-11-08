// The names placed NPCs have in a level, for a network game. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHY THIS IS HERE: everything in a level aims at things by name: a switch that wakes an alien, for example. But in a network game a placed NPC gets a new name, because
// the network messages use part of it to say which NPC they mean. So the switch's name found nothing, and the alien stayed asleep for ever. On the stock levels that's 195
// switch links. Each NPC's old and new name are kept here, so that anything looking for the old name can be sent to the new one.
//
// The names come from a level file, which is treated as hostile: this list has a fixed size, and one too many is refused and not written.
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

// The new name of the NPC with this name in the level, or nothing. A level's name that's the same as its own new name isn't an answer, so a caller who looks that up can't
// go round for ever.
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
