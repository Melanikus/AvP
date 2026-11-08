// The things in a level that only happen on one machine, and have to reach all of them. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHY: a level is a chain of objects telling each other what to do. A press, a walk into a trigger and a destroyed object already run on every machine, so their chains do too. But picking up an item happens on one machine, and an NPC runs on the host alone, so a door waiting on either of them would only open on that machine.
//
// HOW: when a level loads, every machine writes down what each of these objects would set off, from its own copy of the level. When one of them happens, the machine that saw it says only the object's name, and each machine sets off what its own list says. So a message can only start a chain that object already has, and never one the sender made up.
//
// Each one happens once, however often it's reported. Only the host may say one has happened. A client can only ask, for an item, and only if it's near enough to have reached it.
//
// This file knows nothing about the game, so that tests/level_events_test.cpp can try all of it.

#pragma once

#include <string.h>

enum
{
	kLevelEventNameLength = 8,   // the same as SB_NAME_LENGTH
	kMostLevelEvents = 512       // the stock levels have 78 NPC death targets and 2 items at most in all 32 together. A level with more just has the rest stay on one machine
};

struct LevelEvent
{
	char source[kLevelEventNameLength];
	char target[kLevelEventNameLength];
	int request;
	bool clientMayAsk;   // an item: a client sees this happen first
	bool hasPlace;
	int x, y, z;         // where it is, for how near someone must be to ask
	bool happened;
};

struct LevelEventList
{
	LevelEvent events[kMostLevelEvents];
	int count;
};

// A name here is 8 bytes that needn't be letters, so the log writes it as numbers. The two machines' logs can then be compared. text has room for kLevelEventNameLength * 2 + 1.
inline void LevelEvents_NameAsText(const char *name, char *text)
{
	static const char kDigits[] = "0123456789abcdef";

	for (int i = 0; i < kLevelEventNameLength; i++)
	{
		text[i * 2] = kDigits[((unsigned char)name[i]) >> 4];
		text[i * 2 + 1] = kDigits[((unsigned char)name[i]) & 15];
	}

	text[kLevelEventNameLength * 2] = '\0';
}

inline void LevelEvents_Empty(LevelEventList *list)
{
	list->count = 0;
}

// The level is starting again. Its file isn't read again, and an NPC's entry is only written while the file loads, so the list has to stay. What's forgotten is what has happened.
inline void LevelEvents_ForgetWhatHappened(LevelEventList *list)
{
	for (int i = 0; i < list->count; i++)
	{
		list->events[i].happened = false;
	}
}

// The name isn't text: it's bytes that can be anything, including zero, so this is an exact comparison of all of them.
inline LevelEvent *LevelEvents_Find(LevelEventList *list, const char *source)
{
	for (int i = 0; i < list->count; i++)
	{
		if (memcmp(list->events[i].source, source, kLevelEventNameLength) == 0)
		{
			return &list->events[i];
		}
	}

	return 0;
}

// A level's own list, at load. False if the list is full, or this object is on it already.
inline bool LevelEvents_Add(LevelEventList *list, const char *source, const char *target, int request, bool clientMayAsk)
{
	if (list->count >= kMostLevelEvents || LevelEvents_Find(list, source))
	{
		return false;
	}

	LevelEvent *const added = &list->events[list->count++];

	memcpy(added->source, source, kLevelEventNameLength);
	memcpy(added->target, target, kLevelEventNameLength);
	added->request = request;
	added->clientMayAsk = clientMayAsk;
	added->hasPlace = false;
	added->x = added->y = added->z = 0;
	added->happened = false;
	return true;
}

// Whether something this far from it, in each direction, could have reached it. The player's body moves between messages, so this allows for it.
enum
{
	kLevelEventMostDistanceToAsk = 10000   // 10 metres
};

inline bool LevelEvents_IsNear(const LevelEvent *event, int x, int y, int z)
{
	if (!event->hasPlace)
	{
		return false;
	}

	// Differences stay well inside an int for any place in a level, but a message could name anything, so each is limited before being used
	long long const dx = (long long)x - event->x;
	long long const dy = (long long)y - event->y;
	long long const dz = (long long)z - event->z;
	long long const most = kLevelEventMostDistanceToAsk;

	return dx * dx + dy * dy + dz * dz <= most * most;
}

// The host hears a client ask. True if it should happen.
inline bool LevelEvents_ClientMayHaveThis(const LevelEvent *event, bool askerIsNear)
{
	return event && event->clientMayAsk && !event->happened && askerIsNear;
}
