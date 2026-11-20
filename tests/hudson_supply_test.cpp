// hudson_supply_test.cpp: the lane probe's choice of where to make an alien around Hudson (hudson_supply.h), with no game in it. Written 2026-11-20 by Melanikus (Frost)
// for the AvP port (Bishop for the owner, on the second stretch: the first supplyat=around made its aliens a storey above him, so few reached him).

#include <stdio.h>
#include <string>
#include <vector>

#include "hudson_supply.h"

static int checksFailed = 0;

static void Check(bool passed, char const *what)
{
	printf("%s  %s\n", passed ? "  ok" : "  FAILED", what);
	checksFailed += passed ? 0 : 1;
}

static HudsonSupplyRoom Room(int x, int y, int z, bool inHisView)
{
	HudsonSupplyRoom r;

	r.x = x;
	r.y = y;
	r.z = z;
	r.inHisView = inHisView;
	return r;
}

static void TestWhereTheNextOneIsMade()
{
	printf("Where the next one is made\n");

	// Hudson at the origin on the floor (y 1948, down), his route going +x: places ahead at 10, 20, 30 m along x
	std::vector<HudsonSupplyPlace> ahead;

	for (int k = 1; k <= 3; k++)
	{
		HudsonSupplyPlace p;

		p.x = 10000 * k;
		p.y = 1948;
		p.z = 0;
		ahead.push_back(p);
	}

	std::vector<HudsonSupplyRoom> rooms;

	rooms.push_back(Room(20000, 1948 - 8000, 0, false));   // 0: on his route 20 m ahead but a storey up (8 m)
	rooms.push_back(Room(0, 1948, 20000, false));          // 1: on his level, 20 m away, but 20 m off his route
	rooms.push_back(Room(20500, 1948, 2000, true));        // 2: on his level, by his route, but in his view
	rooms.push_back(Room(19000, 1948 - 500, 3000, false)); // 3: on his level (0.5 m), by his route (3 m off the 20 m place), out of view: THE ONE
	rooms.push_back(Room(30000, 1948, 0, false));          // 4: on his route but 30 m away: too far
	rooms.push_back(Room(9000, 1948, 0, false));           // 5: on his route but 9 m away: too near

	Check(HudsonSupply_Pick(rooms, 0, 1948, 0, ahead) == 3, "the room on his level, by his route ahead, out of his view, 15 to 25 m from him");

	rooms[3].inHisView = true;
	Check(HudsonSupply_Pick(rooms, 0, 1948, 0, ahead) == -1, "none when no room fits (a storey up, off his route, in view, too far, too near)");

	rooms.push_back(Room(15500, 1948 + 1500, 2500, false));   // 6: 1.5 m lower: still his level
	rooms.push_back(Room(21000, 1948, 1000, false));          // 7: nearer to 20 m than 6
	Check(HudsonSupply_Pick(rooms, 0, 1948, 0, ahead) == 7, "of those that fit, the one nearest 20 m from him, so the same walk makes the same aliens");
	Check(HudsonSupply_Pick(rooms, 0, 1948, 0, std::vector<HudsonSupplyPlace>()) == -1, "none with no route ahead");
}

static void TestWhichOnesCountAsAlive()
{
	printf("Which ones keep their slot\n");

	Check(HudsonSupply_HoldsASlot(true, 39000), "alive and within 40 m: it holds its slot");
	Check(HudsonSupply_HoldsASlot(true, 41000), "alive, even over 40 m from him: it holds its slot (Bishop 11-20, the owner's 'swarm': a far alien doesn't free a slot)");
	Check(!HudsonSupply_HoldsASlot(false, 1000), "dead: it frees its slot");
	Check(HudsonSupply_MostMade(4) == 8, "at most twice the cap made in a run: 8 for 4 (the owner: \"Let's limit to 4\", Bishop: at most 8 made)");
}

static void TestInTheNamedRooms()
{
	printf("In the named rooms (supplyin=)\n");

	std::vector<HudsonSupplyRoom> rooms;

	rooms.push_back(Room(30000, 1948, 0, false));          // 0: 30 m, on his level, out of view
	rooms.push_back(Room(21000, 1948, 0, true));           // 1: 21 m but in his view
	rooms.push_back(Room(19000, 1948 - 6000, 0, false));   // 2: 19 m but a storey up
	rooms.push_back(Room(8000, 1948, 0, false));           // 3: 8 m: too near
	rooms.push_back(Room(45000, 1948, 0, false));          // 4: 45 m: too far
	rooms.push_back(Room(0, 1948, 22000, false));          // 5: 22 m, on his level, out of view, nearer 20 m than 0: THE ONE

	Check(HudsonSupply_PickIn(rooms, 0, 1948, 0) == 5, "of the named rooms: out of his view, on his level, 10 to 40 m from him, the one nearest 20 m");
	rooms[5].inHisView = true;
	Check(HudsonSupply_PickIn(rooms, 0, 1948, 0) == 0, "and the next nearest 20 m when that one comes into view");
	rooms[0].inHisView = true;
	Check(HudsonSupply_PickIn(rooms, 0, 1948, 0) == -1, "none when none fits");
}

int main()
{
	TestWhereTheNextOneIsMade();
	TestWhichOnesCountAsAlive();
	TestInTheNamedRooms();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all ok");
	return checksFailed ? 1 : 0;
}
