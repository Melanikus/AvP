// The test of how much room the game makes for far locations. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A level's rooms ("modules") are put into groups ("AI modules"), and aliens and other creatures use the groups to find
// their way about. When a level loads, the game works out a list of places in each room where a creature can stand when
// it's far from the player. Each room gets room for FAR_MAX_LOCS of those places, and the list of them is shared out by
// going through the groups.
//
// The groups come from the level file. The room used to be counted a different way from the way it was shared out, so a
// level could put a room in two groups and get more shared out than had been made. An assert was all that said so, and
// asserts are left out of the game players run.
//
// This test sets up groups like those and checks that there's room for what's shared out.
//
// HOW IT RUNS
//
// pfarlocs.cpp is included below, because the function that makes the room can only be reached from inside that file. That
// file calls much of the rest of the game, which isn't here, so tests/far_locations_test.args tells the linker to carry on
// anyway (/FORCE:UNRESOLVED). A call to something that isn't here would crash this test, which would fail it. The function
// tested here doesn't make one.
//
// WHAT IT CAN'T TEST
//
// How the places get written into that room needs a real level, so that's shown only by reading the code. Every room writes
// to its own group's share, and it's in that group's list, so the share is big enough for all of them.

#include "pfarlocs.cpp"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

int ModuleArraySize = 0;
int memoryInitialisationFailure = 0;

// The memory code tells the game's sounds about memory that's about to go (DeallocMem in our_mem.cpp). There are no sounds in this
// test. Without this, that call would go to nothing, which /FORCE:UNRESOLVED lets through the build, and the test would crash the
// first time anything is freed.
void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static int checksFailed = 0;

static void FarCheck(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static MODULE rooms[8];
static MODULE *roomList[9];
static AIMODULE groups[8];

// A group's list of rooms, which ends with an empty entry. Group 0 is never used, and it has no list.
static MODULE *lists[8][8];

// The game's own count of modules is always one more than the rooms, because the list starts with a module that stands for
// everywhere outside the level. So the list of groups, which starts at 1, always has a place for every group.
static int ModulesFor(int howManyRooms)
{
	return howManyRooms + 1;
}

static void StartWith(int howManyRooms, int howManyGroups)
{
	memset(rooms, 0, sizeof(rooms));
	memset(roomList, 0, sizeof(roomList));
	memset(groups, 0, sizeof(groups));
	memset(lists, 0, sizeof(lists));

	for (int i = 0; i < howManyRooms; i++)
	{
		rooms[i].m_index = i;
		roomList[i] = &rooms[i];
	}

	for (int i = 1; i < howManyGroups; i++)
	{
		groups[i].m_index = i;
		groups[i].m_module_ptrs = lists[i];
	}

	AIModuleArray = groups;
	AIModuleArraySize = howManyGroups;
	ModuleArraySize = ModulesFor(howManyRooms);
	FALLP_AuxLocs = 0;
	FL_AuxData = 0;
	FL_TotalNumAuxLocs = 0;
	memoryInitialisationFailure = 0;
}

// Does every group's share lie inside the room that was made?
static bool SharesFit(int howManyGroups)
{
	for (int i = 1; i < howManyGroups; i++)
	{
		int inGroup = 0;

		while (lists[i][inGroup])
		{
			inGroup++;
		}

		VECTORCH const *end = FALLP_AuxLocs[i].locationsList + (inGroup * FAR_MAX_LOCS);

		if (end > FL_AuxData + FL_TotalNumAuxLocs)
		{
			return false;
		}
	}

	return true;
}

static void CheckAnOrdinaryLevel()
{
	printf("An ordinary level:\n");

	StartWith(3, 3);
	lists[1][0] = &rooms[0];
	lists[1][1] = &rooms[1];
	lists[2][0] = &rooms[2];

	InitFarLocDataAreas(roomList, ModulesFor(3));

	FarCheck(memoryInitialisationFailure == 0, "the room is made");
	FarCheck(FL_TotalNumAuxLocs == 3 * FAR_MAX_LOCS, "there's room for each of the 3 rooms, and no more");
	FarCheck(FALLP_AuxLocs[1].locationsList == FL_AuxData, "the first group's share is at the start");
	FarCheck(FALLP_AuxLocs[2].locationsList == FL_AuxData + (2 * FAR_MAX_LOCS), "the second group's share comes after its two rooms");
	FarCheck(FALLP_AuxLocs[0].locationsList == 0, "group 0 has no share");
	FarCheck(SharesFit(3), "every share fits");
}

static void CheckARoomInTwoGroups()
{
	printf("A level with a room in two groups:\n");

	StartWith(3, 3);
	lists[1][0] = &rooms[0];
	lists[1][1] = &rooms[1];
	lists[1][2] = &rooms[2];
	lists[2][0] = &rooms[2];

	InitFarLocDataAreas(roomList, ModulesFor(3));

	FarCheck(FL_TotalNumAuxLocs == 4 * FAR_MAX_LOCS, "there's room for everything the groups list");
	FarCheck(SharesFit(3), "every share fits");

	printf("A level with a room listed twice in its group:\n");

	StartWith(2, 2);
	lists[1][0] = &rooms[0];
	lists[1][1] = &rooms[1];
	lists[1][2] = &rooms[1];

	InitFarLocDataAreas(roomList, ModulesFor(2));

	FarCheck(FL_TotalNumAuxLocs == 3 * FAR_MAX_LOCS, "there's room for everything the group lists");
	FarCheck(SharesFit(2), "every share fits");
}

static void CheckEveryRoomItsOwnGroup()
{
	printf("A level where every room is a group of its own:\n");

	// Groups are numbered from 1, so 3 rooms make groups 1, 2 and 3, and the list of groups needs four places. It has them,
	// because of the extra module. This row holds that in place, in case the count of modules ever changes.
	StartWith(3, 4);
	lists[1][0] = &rooms[0];
	lists[2][0] = &rooms[1];
	lists[3][0] = &rooms[2];

	InitFarLocDataAreas(roomList, ModulesFor(3));

	FarCheck(_msize(FALLP_AuxLocs) >= 4 * sizeof(FARLOCATIONSHEADER), "the list of groups has a place for each of them");
	FarCheck(SharesFit(4), "every share fits");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	CheckAnOrdinaryLevel();
	CheckARoomInTwoGroups();
	CheckEveryRoomItsOwnGroup();

	if (checksFailed)
	{
		printf("The far locations test FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("The far locations test passed");
	return 0;
}
