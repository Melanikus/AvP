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
	Check(!HudsonSupply_HoldsASlot(true, 41000), "alive but over 40 m from him: it frees its slot, so the supply keeps coming");
	Check(!HudsonSupply_HoldsASlot(false, 1000), "dead: it frees its slot");
	Check(HudsonSupply_MostMade(4) == 16, "at most 4 times the cap made in a run, so a run can't fill the level");
}

int main()
{
	TestWhereTheNextOneIsMade();
	TestWhichOnesCountAsAlive();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all ok");
	return checksFailed ? 1 : 0;
}
