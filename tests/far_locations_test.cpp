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
// Added 2026-11-09 (#161): and how high each place stands, from the floor under it and the ceiling over it, four-cornered ones included
// (CheckTheHeights, near the end).
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

// ---- Added 2026-11-09 by Melanikus for the AvP port (#161): how high a far location stands, from its room's floors and ceilings
// (GetFarLocHeight). The spot goes 10 above the highest corner of the floor under it, and it's refused if the lowest corner of the
// ceiling over it leaves less than FAR_BB_HEIGHT (a crouched alien) between them. The 1999 code read a four-cornered polygon's 4th
// corner but took its 3rd corner's height, for floors and ceilings both.
//
// The room's polygons come from these, in place of the game's shape reader (plat_shp.cpp, which isn't in this test): the four calls
// GetFarLocHeight makes, over a list the test sets.
static struct ColPolyTag roomPolygons[4];
static int roomPolygonCount = 0;
static int roomPolygonAt = -1;

int SetupPolygonAccessFromShapeIndex(int)
{
	roomPolygonAt = -1;
	return roomPolygonCount;
}

void AccessNextPolygon(void)
{
	roomPolygonAt++;
}

void GetPolygonVertices(struct ColPolyTag *polyPtr)
{
	if (roomPolygonAt >= 0 && roomPolygonAt < roomPolygonCount)
	{
		polyPtr->NumberOfVertices = roomPolygons[roomPolygonAt].NumberOfVertices;
		memcpy(polyPtr->PolyPoint, roomPolygons[roomPolygonAt].PolyPoint, sizeof(polyPtr->PolyPoint));
	}
}

void GetPolygonNormal(struct ColPolyTag *polyPtr)
{
	if (roomPolygonAt >= 0 && roomPolygonAt < roomPolygonCount)
	{
		polyPtr->PolyNormal = roomPolygons[roomPolygonAt].PolyNormal;
	}
}

// IsXZinPoly needs it, and mathline.cpp isn't in this test: the same sum, a times b, then divided by c, through 64 bits
int WideMulNarrowDiv(int a, int b, int c)
{
	return static_cast<int>((static_cast<long long>(a) * b) / c);
}

// A flat square, 1000 either side of the room's middle, at this height (the game's y grows downwards), with one corner moved up or
// down. A floor faces up and a ceiling faces down.
static struct ColPolyTag Square(int height, bool ceiling, int cornerMoved, int movedBy)
{
	struct ColPolyTag square;
	memset(&square, 0, sizeof(square));
	int const xs[4] = { -1000, 1000, 1000, -1000 };
	int const zs[4] = { -1000, -1000, 1000, 1000 };
	square.NumberOfVertices = 4;

	for (int i = 0; i < 4; i++)
	{
		square.PolyPoint[i].vx = xs[i];
		square.PolyPoint[i].vy = height + (i == cornerMoved ? movedBy : 0);
		square.PolyPoint[i].vz = zs[i];
	}

	square.PolyNormal.vy = ceiling ? 65536 : -65536;
	return square;
}

// The spot in the room's middle, over this floor and under this ceiling: whether it's kept, and its height
static FARVALIDATEDLOCATION SpotBetween(struct ColPolyTag const &floor, struct ColPolyTag const &ceiling)
{
	static MODULEMAPBLOCK map;
	static MODULE room;
	memset(&map, 0, sizeof(map));
	memset(&room, 0, sizeof(room));
	room.m_mapptr = &map;
	room.m_miny = -10000;   // the room's top
	room.m_maxy = 10000;    // and its bottom

	roomPolygons[0] = floor;
	roomPolygons[1] = ceiling;
	roomPolygonCount = 2;

	FARVALIDATEDLOCATION spot;
	memset(&spot, 0, sizeof(spot));
	spot.valid = 1;
	GetFarLocHeight(&spot, &room);
	return spot;
}

static void CheckTheHeights()
{
	printf("How high a far location stands (#161):\n");

	// A floor at 0 and a ceiling 5000 above it: plenty of room
	FARVALIDATEDLOCATION spot = SpotBetween(Square(0, false, -1, 0), Square(-5000, true, -1, 0));
	FarCheck(spot.valid == 1 && spot.position.vy == -10, "over a flat floor, the spot is 10 above it");

	spot = SpotBetween(Square(0, false, 3, -300), Square(-5000, true, -1, 0));
	FarCheck(spot.valid == 1 && spot.position.vy == -310, "over a floor rising 300 at its 4th corner, it's 10 above that corner, not inside the floor");
	spot = SpotBetween(Square(0, false, 2, -300), Square(-5000, true, -1, 0));
	FarCheck(spot.valid == 1 && spot.position.vy == -310, "and the same when it's the 3rd corner that rises");
	spot = SpotBetween(Square(0, false, 3, 300), Square(-5000, true, -1, 0));
	FarCheck(spot.valid == 1 && spot.position.vy == -10, "a 4th corner lower than the rest changes nothing");

	// FAR_BB_HEIGHT between floor and ceiling, a crouched alien: 2100 is room enough, 1900 isn't
	spot = SpotBetween(Square(0, false, -1, 0), Square(-2100, true, -1, 0));
	FarCheck(FAR_BB_HEIGHT == 2000 && spot.valid == 1, "2100 between a flat floor and a flat ceiling is room enough");
	spot = SpotBetween(Square(0, false, -1, 0), Square(-2100, true, 3, 200));
	FarCheck(spot.valid == 0, "but under a ceiling that comes down 200 at its 4th corner, there's only 1900, and the spot is refused");
	spot = SpotBetween(Square(0, false, 3, -200), Square(-2100, true, -1, 0));
	FarCheck(spot.valid == 0, "and over a floor that rises 200 at its 4th corner, the same");
	spot = SpotBetween(Square(0, false, -1, 0), Square(-2100, true, 2, 200));
	FarCheck(spot.valid == 0, "as it always was when it's the 3rd corner that comes down");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	CheckAnOrdinaryLevel();
	CheckARoomInTwoGroups();
	CheckEveryRoomItsOwnGroup();
	CheckTheHeights();

	if (checksFailed)
	{
		printf("The far locations test FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("The far locations test passed");
	return 0;
}
