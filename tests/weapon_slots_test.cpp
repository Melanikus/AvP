// Tests for how the marine's 14 weapon slots fit with older saves, in 3dc/avp/weapon_slots.h. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT DOESN'T TEST: nothing here loads a real save in the game. That takes a run.

#include "weapon_slots.h"

#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static void TestSlotNumbers()
{
	bool stayPut = true;

	for (int slot = 0; slot < 10; slot++)
	{
		stayPut = stayPut && Slots_FromOlderSave(slot) == slot;
	}

	Check(stayPut, "the first 10 slots keep their numbers");
	Check(Slots_FromOlderSave(10) == 13, "the cudgel's slot moves to the last place");
	Check(Slots_FromOlderSave(12) == 15, "and so does the number for not changing weapon");
	Check(Slots_FromOlderSave(11) == -1, "the old end of the list isn't a slot");
	Check(Slots_FromOlderSave(-1) == -1, "and nor is a number below 0");
	Check(Slots_FromOlderSave(13) == -1, "or one past it, which a real older save can't hold");
	Check(Slots_FromOlderSave(0x7FFFFFFF) == -1, "or a huge one");

	Check(!Slots_IsNewSinceOlderSaves(9), "slot 10 isn't new");
	Check(Slots_IsNewSinceOlderSaves(10) && Slots_IsNewSinceOlderSaves(11) && Slots_IsNewSinceOlderSaves(12), "the next three are");
	Check(!Slots_IsNewSinceOlderSaves(13), "the cudgel's isn't");
}

// A block shaped like the real ones: something before the list, the list, something after it
struct OlderBlock
{
	int before[3];
	int slots[11];
	int after[5];
};

struct TodayBlock
{
	int before[3];
	int slots[14];
	int after[5];
};

static OlderBlock AnOlderBlock()
{
	OlderBlock older;

	for (int i = 0; i < 3; i++) older.before[i] = 100 + i;
	for (int i = 0; i < 11; i++) older.slots[i] = 200 + i;
	for (int i = 0; i < 5; i++) older.after[i] = 300 + i;

	return older;
}

static void TestUpgrade()
{
	OlderBlock const older = AnOlderBlock();
	TodayBlock today;

	memset(&today, 0x55, sizeof(today));

	size_t const size = Slots_UpgradeArray(&older, sizeof(older), &today, sizeof(today), offsetof(OlderBlock, slots), sizeof(int));

	Check(size == sizeof(today), "the block comes out at today's size");
	Check(today.before[0] == 100 && today.before[2] == 102, "what's before the list comes with it");
	Check(today.slots[0] == 200 && today.slots[9] == 209, "the slots that stay put, stay put");
	Check(today.slots[10] == 0 && today.slots[11] == 0 && today.slots[12] == 0, "the new slots are empty");
	Check(today.slots[13] == 210, "the cudgel has moved");
	Check(today.after[0] == 300 && today.after[4] == 304, "and what's after the list comes with it");
}

// The statistics have three lists in a row, so they're done one at a time, each in today's place
struct OlderStats
{
	int head;
	int a[11];
	int b[11];
	int c[11];
	int tail;
};

struct TodayStats
{
	int head;
	int a[14];
	int b[14];
	int c[14];
	int tail;
};

static void TestThreeListsInARow()
{
	OlderStats older;

	older.head = 7;
	older.tail = 8;

	for (int i = 0; i < 11; i++)
	{
		older.a[i] = 100 + i;
		older.b[i] = 200 + i;
		older.c[i] = 300 + i;
	}

	TodayStats first, second;
	size_t size = Slots_UpgradeArray(&older, sizeof(older), &first, sizeof(first), offsetof(TodayStats, a), sizeof(int));

	size = Slots_UpgradeArray(&first, size, &second, sizeof(second), offsetof(TodayStats, b), sizeof(int));
	size = Slots_UpgradeArray(&second, size, &first, sizeof(first), offsetof(TodayStats, c), sizeof(int));

	Check(size == sizeof(TodayStats), "three lists in a row come out at today's size");
	Check(first.head == 7 && first.tail == 8, "with what's around them");
	Check(first.a[9] == 109 && first.a[10] == 0 && first.a[13] == 110, "the first list is right");
	Check(first.b[9] == 209 && first.b[10] == 0 && first.b[13] == 210, "the second is");
	Check(first.c[9] == 309 && first.c[10] == 0 && first.c[13] == 310, "and the third");
}

static void TestHostileSizes()
{
	OlderBlock const older = AnOlderBlock();
	TodayBlock today;
	TodayBlock const untouched = {};

	today = untouched;

	Check(Slots_UpgradeArray(&older, sizeof(older), &today, sizeof(today) - 1, offsetof(OlderBlock, slots), sizeof(int)) == 0, "no room for the result is refused");
	Check(Slots_UpgradeArray(&older, 4, &today, sizeof(today), 0, sizeof(int)) == 0, "a block too short to hold a list is refused");
	Check(Slots_UpgradeArray(&older, sizeof(older), &today, sizeof(today), sizeof(older), sizeof(int)) == 0, "a list that would start past the end is refused");
	Check(Slots_UpgradeArray(&older, sizeof(older), &today, sizeof(today), (size_t)-1, sizeof(int)) == 0, "and one so far on that the sum would wrap round");
	Check(Slots_UpgradeArray(&older, sizeof(older), &today, sizeof(today), 0, 0) == 0, "an entry of no size is refused");
	Check(memcmp(&today, &untouched, sizeof(today)) == 0, "and after all of those, nothing has been written");
}

// The marine's list: the cudgel last, and every gun carried with ammunition. The game counts the cudgel as having some.
static void FullMarine(SlotForChanging *slots)
{
	for (int i = 0; i < 14; i++)
	{
		slots[i].possessed = true;
		slots[i].mayBeChosen = true;
		slots[i].isCudgel = i == 13;
		slots[i].hasAmmo = true;
	}
}

// The same marine with no ammunition in any gun
static void MarineOutOfAmmo(SlotForChanging *slots)
{
	FullMarine(slots);

	for (int i = 0; i < 13; i++)
	{
		slots[i].hasAmmo = false;
	}
}

static void TestChangingWeapon()
{
	SlotForChanging slots[14];

	FullMarine(slots);

	bool allReached = true;

	for (int from = 0; from < 12; from++)
	{
		allReached = allReached && Slots_NextToChangeTo(slots, 14, from, 1, true) == from + 1;
	}

	Check(allReached, "Next Weapon goes through every gun in turn, slots 11 to 13 as well");
	Check(Slots_NextToChangeTo(slots, 14, 12, 1, true) == 0, "and after the last gun it passes the cudgel and comes back to the first");
	Check(Slots_NextToChangeTo(slots, 14, 0, -1, true) == 12, "Previous Weapon from the first gun passes the cudgel and goes to the last gun");
	Check(Slots_NextToChangeTo(slots, 14, 12, -1, true) == 11, "and goes back one at a time");
	Check(Slots_NextToChangeTo(slots, 14, 13, 1, true) == 0, "from the cudgel, Next Weapon goes to the first gun");
	Check(Slots_NextToChangeTo(slots, 14, 13, -1, true) == 12, "and Previous Weapon to the last");

	FullMarine(slots);
	for (int i = 1; i < 13; i++)
	{
		slots[i].possessed = i == 5 || i == 11;
	}

	Check(Slots_NextToChangeTo(slots, 14, 0, 1, true) == 5, "guns he isn't carrying are skipped");
	Check(Slots_NextToChangeTo(slots, 14, 5, 1, true) == 11, "however many there are");
	Check(Slots_NextToChangeTo(slots, 14, 11, 1, true) == 0, "going forwards");
	Check(Slots_NextToChangeTo(slots, 14, 11, -1, true) == 5, "and backwards");

	FullMarine(slots);
	slots[1].mayBeChosen = false;

	Check(Slots_NextToChangeTo(slots, 14, 0, 1, true) == 2, "a gun that may not be chosen is skipped");

	// The game moves a marine off an empty gun straight away, so offering it would stop him getting past it
	FullMarine(slots);
	slots[5].hasAmmo = false;

	Check(Slots_NextToChangeTo(slots, 14, 4, 1, true) == 6, "an empty gun is passed over going forwards");
	Check(Slots_NextToChangeTo(slots, 14, 6, -1, true) == 4, "and going backwards");

	FullMarine(slots);
	slots[0].hasAmmo = false;

	bool stillReached = true;

	for (int from = 1; from < 12; from++)
	{
		stillReached = stillReached && Slots_NextToChangeTo(slots, 14, from, 1, true) == from + 1;
	}

	Check(stillReached, "with the pulse rifle out of ammunition, every other gun can still be reached in turn");
	Check(Slots_NextToChangeTo(slots, 14, 12, 1, true) == 1, "and going past the end doesn't stop at the cudgel, which he'd be moved straight off");
	Check(Slots_NextToChangeTo(slots, 14, 1, -1, true) == 12, "going back from the first gun with ammunition reaches the last");
	Check(Slots_NextToChangeTo(slots, 14, 13, 1, true) == 1, "from the cudgel, a pulse rifle with nothing in it is passed over");
	Check(Slots_NextToChangeTo(slots, 14, 13, -1, true) == 12, "in both directions");

	MarineOutOfAmmo(slots);

	Check(Slots_NextToChangeTo(slots, 14, 3, 1, true) == 13, "with no ammunition in any gun, the search goes to the cudgel, where the game would leave him");
	Check(Slots_NextToChangeTo(slots, 14, 13, 1, true) == 13, "and holding the cudgel, he stays on it");
	Check(Slots_NextToChangeTo(slots, 14, 13, -1, true) == 13, "in both directions");

	MarineOutOfAmmo(slots);
	slots[7].hasAmmo = true;

	Check(Slots_NextToChangeTo(slots, 14, 3, 1, true) == 7, "one gun with ammunition is found, however far along");
	Check(Slots_NextToChangeTo(slots, 14, 7, 1, true) == 7, "and with no other, he stays on it");
	Check(Slots_NextToChangeTo(slots, 14, 13, 1, true) == 7, "and from the cudgel he goes to it");
	Check(Slots_NextToChangeTo(slots, 14, 8, 1, true) == 7, "even when it's the very last slot the search comes to");

	MarineOutOfAmmo(slots);
	slots[7].hasAmmo = true;
	slots[7].possessed = false;

	Check(Slots_NextToChangeTo(slots, 14, 3, 1, true) == 13, "ammunition for a gun he isn't carrying doesn't count");

	MarineOutOfAmmo(slots);
	slots[7].hasAmmo = true;
	slots[7].mayBeChosen = false;

	Check(Slots_NextToChangeTo(slots, 14, 3, 1, true) == 13, "and nor does ammunition in a gun that may not be chosen");

	MarineOutOfAmmo(slots);
	slots[13].possessed = false;

	Check(Slots_NextToChangeTo(slots, 14, 3, 1, true) == 4, "with no ammunition and no cudgel, nothing moves him, so he can still go through the guns");

	// A Predator's list has guns in its first slots, and the rest empty
	SlotForChanging predator[14];
	for (int i = 0; i < 14; i++)
	{
		predator[i].possessed = i < 6;
		predator[i].mayBeChosen = true;
		predator[i].isCudgel = false;
		predator[i].hasAmmo = i < 6;
	}

	Check(Slots_NextToChangeTo(predator, 14, 5, 1, false) == 0, "empty slots at the end of a list are skipped going forwards");
	Check(Slots_NextToChangeTo(predator, 14, 0, -1, false) == 5, "and going backwards");

	predator[1].hasAmmo = false;

	Check(Slots_NextToChangeTo(predator, 14, 0, 1, false) == 1, "another species keeps holding an empty gun, so it can choose one");

	for (int i = 0; i < 14; i++)
	{
		slots[i].possessed = false;
		slots[i].isCudgel = false;
		slots[i].mayBeChosen = true;
		slots[i].hasAmmo = true;
	}

	Check(Slots_NextToChangeTo(slots, 14, 4, 1, true) == 4, "a save can leave nothing to choose, and the search ends where it started");
	Check(Slots_NextToChangeTo(slots, 14, 4, -1, true) == 4, "in both directions");

	FullMarine(slots);

	Check(Slots_NextToChangeTo(slots, 14, 14, 1, true) == 14, "a slot past the end of the list isn't looked up");
	Check(Slots_NextToChangeTo(slots, 14, -1, 1, true) == -1, "and nor is one before it");
}

// The list of guns the game falls back to is in the game's source, and needs too much of the game to run here. So this reads it. It only shows the guns are on the list.
static void TestFallBackList()
{
	std::string const here = __FILE__;
	std::string const top = here.substr(0, here.find_last_of("/\\"));
	std::ifstream file(top + "/../3dc/avp/weapons.cpp", std::ios::binary);
	std::stringstream words;
	words << file.rdbuf();

	std::string const source = words.str();
	size_t const start = source.find("enum WEAPON_ID MarineWeaponHierarchy[]");
	size_t const end = source.find("NULL_WEAPON", start);
	size_t const cudgel = source.find("WEAPON_CUDGEL", start);

	Check(start != std::string::npos && end != std::string::npos && cudgel != std::string::npos && cudgel < end, "the marine's list of guns to fall back to is there, with the cudgel on it");

	for (char const *gun : { "WEAPON_AUTOSHOTGUN", "WEAPON_SONICCANNON", "WEAPON_BEAMCANNON", "WEAPON_MYSTERYGUN" })
	{
		size_t const at = source.find(gun, start);

		Check(at != std::string::npos && at < cudgel, std::string(gun) + " is on the list, ahead of the cudgel");
	}
}

int main()
{
	TestSlotNumbers();
	TestUpgrade();
	TestThreeListsInARow();
	TestHostileSizes();
	TestChangingWeapon();
	TestFallBackList();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
