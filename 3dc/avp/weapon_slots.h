// How the marine's 14 weapon slots fit with a save made when there were 11. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT CHANGED: the first 10 slots hold the same guns as they always did, in the same places (the 10th was empty, and now holds the shotgun). The cudgel was in slot 11 and is now in slot 14, and slots 11, 12 and 13 are the new guns.
//
// WHY A SAVE CARES: a save holds a copy of every slot, so the part of the file that holds them has grown. The old code looks at the size of that part and quietly skips it if it's not what it expects, and he'd come back with the level's starting weapons, health and position. So a part with the older size is made into today's shape first: everything he had stays his, the cudgel moves, and the new slots stay as the level set them up.
//
// A save is a file, so anybody could have changed it. Each number here is checked before it's used.
//
// This file knows nothing about the game, so that tests/weapon_slots_test.cpp can try all of it.

#pragma once

#include <stddef.h>
#include <string.h>

enum
{
	kSlotsInOlderSaves = 11,
	kSlotsToday = 14,
	kSlotsAddedSinceOlderSaves = kSlotsToday - kSlotsInOlderSaves,

	// The first this many slots are the same in both
	kSlotsThatStayPut = 10,

	kCudgelSlotInOlderSaves = 10,
	kCudgelSlotToday = 13,

	// The number that means "not changing weapon", which comes just after the list of slots
	kNotSwappingInOlderSaves = 12,
	kNotSwappingToday = 15
};

// A slot number from an older save, as it is today. Anything else comes back as -1, for the caller to refuse.
inline int Slots_FromOlderSave(int olderSlot)
{
	if (olderSlot >= 0 && olderSlot < kSlotsThatStayPut)
	{
		return olderSlot;
	}

	if (olderSlot == kCudgelSlotInOlderSaves)
	{
		return kCudgelSlotToday;
	}

	if (olderSlot == kNotSwappingInOlderSaves)
	{
		return kNotSwappingToday;
	}

	return -1;
}

// Is this one of the slots an older save knows nothing about?
inline bool Slots_IsNewSinceOlderSaves(int slot)
{
	return slot >= kSlotsThatStayPut && slot < kCudgelSlotToday;
}

// Makes one list of slots in a block from an older save into today's list. Whatever comes before and after the list comes with it, and the entries for the new slots are left as zeros.
// older and today have to be different places. arrayAt says how far into the block the list starts. It gives back the block's new size, or 0 if the numbers can't be right, and then nothing has been written.
inline size_t Slots_UpgradeArray(const void *older, size_t olderSize, void *today, size_t todayRoom, size_t arrayAt, size_t oneEntry)
{
	size_t const olderListBytes = kSlotsInOlderSaves * oneEntry;
	size_t const added = kSlotsAddedSinceOlderSaves * oneEntry;

	if (oneEntry == 0 || olderSize < olderListBytes || arrayAt > olderSize - olderListBytes || olderSize + added > todayRoom)
	{
		return 0;
	}

	const char *const from = (const char *)older;
	char *const to = (char *)today;
	size_t const afterTheOlderList = arrayAt + olderListBytes;

	// Everything before the list, and the slots that stay where they are
	memcpy(to, from, arrayAt + kSlotsThatStayPut * oneEntry);

	// The new slots
	memset(to + arrayAt + kSlotsThatStayPut * oneEntry, 0, added);

	// The cudgel, in its new place
	memcpy(to + arrayAt + kCudgelSlotToday * oneEntry, from + arrayAt + kCudgelSlotInOlderSaves * oneEntry, oneEntry);

	// Everything after the list
	memcpy(to + afterTheOlderList + added, from + afterTheOlderList, olderSize - afterTheOlderList);

	return olderSize + added;
}

// ---- Changing to the next or the previous weapon ----
//
// What the search needs to know about one of the player's slots. weapons.cpp fills these in.
struct SlotForChanging
{
	bool possessed;     // he's carrying a weapon in this slot. An empty slot is false.
	bool mayBeChosen;   // false for a disc launcher with no discs, and for a Gold edition weapon when they aren't allowed
	bool isCudgel;
	bool hasAmmo;
};

// The slot that Next Weapon (step 1) or Previous Weapon (step -1) goes to, starting from the slot "from", round the whole list and past its ends.
//
// WHAT HE CAN STAY ON. The game moves a marine off the cudgel, and off a gun with no ammunition, as soon as he's holding it, and on to the first gun on its list that has some
// (MarineZeroAmmoFunctionality in weapons.cpp). A search that offered one of those would send him there, and then back to the same gun every time, so that no gun beyond it could be reached.
// So while any gun has ammunition, the search passes over the cudgel and the empty guns. When no gun has any, the game leaves him on the cudgel if he's carrying it, so that's where the search
// goes. Other species keep holding an empty gun, so emptyGunsMoveOn is only true for a marine.
//
// The original game went to the cudgel from any gun as soon as the pulse rifle had run out, and from there back to the first gun on the list, so the other guns could only be reached by their number
// keys. The guns in slots 11 to 13 have none.
//
// If there's nowhere else to go, it gives back "from". It looks at each slot once at most, because a save can leave a list where nothing can be chosen, and going round until something
// could be would hang the game.
inline int Slots_NextToChangeTo(SlotForChanging const *slots, int count, int from, int step, bool emptyGunsMoveOn)
{
	if (from < 0 || from >= count)
	{
		return from;
	}

	int cudgel = -1;
	bool someGunHasAmmo = false;

	for (int i = 0; i < count; i++)
	{
		cudgel = (slots[i].isCudgel && slots[i].possessed) ? i : cudgel;
		someGunHasAmmo = someGunHasAmmo || (!slots[i].isCudgel && slots[i].possessed && slots[i].mayBeChosen && slots[i].hasAmmo);
	}

	int slot = from;

	// Every other slot, once. Going on round to "from" itself would change nothing, because that's what comes back anyway.
	for (int tried = 0; tried < count - 1; tried++)
	{
		slot = (slot + step + count) % count;

		if (slots[slot].isCudgel || !slots[slot].possessed || !slots[slot].mayBeChosen)
		{
			continue;
		}

		if (emptyGunsMoveOn && !slots[slot].hasAmmo)
		{
			if (someGunHasAmmo)
			{
				continue;
			}

			if (cudgel != -1)
			{
				return cudgel;
			}
		}

		return slot;
	}

	return from;
}
