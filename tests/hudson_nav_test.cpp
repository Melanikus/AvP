// Tests for the sums Hudson's navigation runs on. Written 2026-11-05 by Melanikus for the AvP port. See 3dc/avp/hudson_nav.cpp.

#include "hudson_nav_plan.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

static int checksDone = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checksDone++;

	if (!good)
	{
		checksFailed++;
		printf("FAILED: %s\n", what.c_str());
	}
}

typedef std::vector<std::vector<int> > Links;

static void CheckChoosingWhereNext()
{
	printf("Where he goes next:\n");

	// 0 - 1 - 2 - 3, a line of rooms
	Links line(4);
	line[0].push_back(1);
	line[1].push_back(0);
	line[1].push_back(2);
	line[2].push_back(1);
	line[2].push_back(3);
	line[3].push_back(2);

	std::vector<char> been(4, 0);
	been[0] = 1;
	Check(HudsonNav_NextModule(line, 0, been) == 1, "from the start of a line, the next room along");

	been[1] = 1;
	Check(HudsonNav_NextModule(line, 1, been) == 2, "and the next, not back the way he came");

	// Back at the start, with the near rooms done: the way to somewhere new starts by going through a room he's been to
	been[2] = 1;
	Check(HudsonNav_NextModule(line, 0, been) == 1, "the way to somewhere new can go through a room he's been to");

	been[3] = 1;
	Check(HudsonNav_NextModule(line, 3, been) == -1, "with everything visited there's nowhere new");

	// A fork: 0 links to 1 and 2. 1 is a dead end he's been to, and 2 leads to somewhere new.
	Links fork(4);
	fork[0].push_back(1);
	fork[0].push_back(2);
	fork[1].push_back(0);
	fork[2].push_back(0);
	fork[2].push_back(3);
	fork[3].push_back(2);

	std::vector<char> forkBeen(4, 0);
	forkBeen[0] = 1;
	forkBeen[1] = 1;
	forkBeen[2] = 1;
	Check(HudsonNav_NextModule(fork, 0, forkBeen) == 2, "of two ways round, it's the one that gets somewhere new");

	// The nearest new one wins, however the list is written
	Links nearFar(5);
	nearFar[0].push_back(1);
	nearFar[0].push_back(2);
	nearFar[1].push_back(3);
	nearFar[3].push_back(4);

	std::vector<char> nearBeen(5, 0);
	nearBeen[0] = 1;
	nearBeen[1] = 1;
	nearBeen[3] = 1;
	Check(HudsonNav_NextModule(nearFar, 0, nearBeen) == 2, "the nearest new room comes before one further off, even when the way to the other is first on the list");

	// A way that only goes one way counts one way
	Links oneWay(2);
	oneWay[1].push_back(0);

	std::vector<char> oneBeen(2, 0);
	oneBeen[0] = 1;
	Check(HudsonNav_NextModule(oneWay, 0, oneBeen) == -1, "a link that only goes the other way can't be used");

	// Rooms that link round in a ring can't make it go on for ever
	Links ring(3);
	ring[0].push_back(1);
	ring[1].push_back(2);
	ring[2].push_back(0);

	std::vector<char> ringBeen(3, 1);
	Check(HudsonNav_NextModule(ring, 0, ringBeen) == -1, "a ring of rooms he's been to ends, with nowhere new");

	// A room linked to itself
	Links self(2);
	self[0].push_back(0);
	self[0].push_back(1);

	std::vector<char> selfBeen(2, 0);
	selfBeen[0] = 1;
	Check(HudsonNav_NextModule(self, 0, selfBeen) == 1, "a room linked to itself doesn't send him nowhere");

	// Numbers that aren't modules: nothing is believed
	std::vector<char> two(2, 0);
	Links none(2);
	Check(HudsonNav_NextModule(none, -1, two) == -1, "from a number below the list");
	Check(HudsonNav_NextModule(none, 2, two) == -1, "from a number past the end of the list");
	Check(HudsonNav_NextModule(none, 0, std::vector<char>(1, 0)) == -1, "with a been-there list of the wrong size");
	Check(HudsonNav_NextModule(Links(), 0, std::vector<char>()) == -1, "with no modules at all");

	// Numbers so far out of the list that believing one would crash this test, and a crash is what shows it
	Links wrong(2);
	wrong[0].push_back(2000000000);
	wrong[0].push_back(-2000000000);
	wrong[0].push_back(1);
	std::vector<char> wrongBeen(2, 0);
	wrongBeen[0] = 1;
	Check(HudsonNav_NextModule(wrong, 0, wrongBeen) == 1, "a link to a number that isn't a module is stepped over");

	// A been-there list too short for the links. Reading past its end would find some new room among all those neighbours, or crash.
	{
		Links many(4096);

		for (int i = 1; i < 4096; i++)
		{
			many[0].push_back(i);
		}

		Check(HudsonNav_NextModule(many, 0, std::vector<char>(1, 1)) == -1, "a been-there list shorter than the list of modules is refused, and not read past its end");
	}

	// Two runs choose the same way
	std::vector<char> sameBeen(4, 0);
	sameBeen[0] = 1;
	Check(HudsonNav_NextModule(fork, 0, sameBeen) == HudsonNav_NextModule(fork, 0, sameBeen), "the same question gets the same answer");
	Check(HudsonNav_NextModule(fork, 0, sameBeen) == 1, "and the first on the list wins between two the same distance away");

	// A long line, for the way to come back from the far end
	int const longest = 8192;
	Links longLine(longest);

	for (int i = 0; i + 1 < longest; i++)
	{
		longLine[i].push_back(i + 1);
		longLine[i + 1].push_back(i);
	}

	std::vector<char> longBeen(longest, 1);
	longBeen[longest - 1] = 0;
	Check(HudsonNav_NextModule(longLine, 0, longBeen) == 1, "a level as long as it's allowed to be, with the only new room at the far end");
}

static void CheckBeingStuck()
{
	printf("Whether he's getting anywhere:\n");

	int const frame = kHudsonNavOneWhole / 30;
	HudsonNavProgress progress;

	HudsonNav_StartProgress(&progress, 100, 200, 300);
	Check(!HudsonNav_IsStuck(&progress, 100, 200, 300, frame), "a frame in the same spot isn't stuck");

	HudsonNav_StartProgress(&progress, 0, 0, 0);
	Check(!HudsonNav_IsStuck(&progress, 0, 0, 0, kHudsonNavStuckAfter - 1), "not quite long enough isn't stuck");
	Check(HudsonNav_IsStuck(&progress, 0, 0, 0, 1), "exactly long enough is");
	Check(HudsonNav_IsStuck(&progress, 0, 0, 0, frame), "and he stays stuck");

	// Near enough to count as the same spot, and far enough not to, each way
	HudsonNav_StartProgress(&progress, 0, 0, 0);
	Check(!HudsonNav_IsStuck(&progress, kHudsonNavProgressIsAtLeast - 1, 0, 0, kHudsonNavStuckAfter - 1), "just inside the reach, sideways");
	Check(HudsonNav_IsStuck(&progress, kHudsonNavProgressIsAtLeast - 1, 0, 0, 1), "shuffling about inside the reach counts as stuck");

	int const axes[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

	for (int i = 0; i < 3; i++)
	{
		int const d = kHudsonNavProgressIsAtLeast;

		HudsonNav_StartProgress(&progress, 0, 0, 0);
		progress.timeHere = kHudsonNavStuckAfter - 1;
		Check(!HudsonNav_IsStuck(&progress, d * axes[i][0], d * axes[i][1], d * axes[i][2], frame), "getting a metre away along any one direction isn't stuck, so a ramp or a fall counts");
		Check(progress.timeHere == 0, "and it starts the time again");
		Check(progress.x == d * axes[i][0] && progress.y == d * axes[i][1] && progress.z == d * axes[i][2], "from where he's got to");
	}

	// Walking steadily never trips it
	{
		HudsonNav_StartProgress(&progress, 0, 0, 0);
		bool everStuck = false;

		for (int i = 1; i < 100000; i++)
		{
			everStuck = everStuck || HudsonNav_IsStuck(&progress, i * 100, 0, 0, frame);
		}

		Check(!everStuck, "walking steadily is never stuck");
	}

	// Walking on the spot for a long time, or a very long frame, can't overflow
	{
		HudsonNav_StartProgress(&progress, 0, 0, 0);

		for (int i = 0; i < 1000000; i++)
		{
			HudsonNav_IsStuck(&progress, 0, 0, 0, frame);
		}

		Check(progress.timeHere >= kHudsonNavStuckAfter && progress.timeHere < 2 * kHudsonNavStuckAfter, "the time stops growing once it's said so, however long it goes on");

		HudsonNav_StartProgress(&progress, 0, 0, 0);
		Check(HudsonNav_IsStuck(&progress, 0, 0, 0, 2147483647), "a frame as long as a number can say doesn't overflow");
		Check(HudsonNav_IsStuck(&progress, 0, 0, 0, 2147483647), "nor another one");

		HudsonNav_StartProgress(&progress, 0, 0, 0);
		Check(!HudsonNav_IsStuck(&progress, 0, 0, 0, -5) && progress.timeHere == 0, "a frame that says it took less than no time takes nothing away");
	}

	// The two ends of the world are further apart than 32 bits can square
	HudsonNav_StartProgress(&progress, -2000000000, -2000000000, -2000000000);
	Check(!HudsonNav_IsStuck(&progress, 2000000000, 2000000000, 2000000000, frame), "a very long way away is a long way, and doesn't overflow");

	// As far apart as two numbers can be: in 32 bits, that difference comes out as just -1, which would look like the same spot
	HudsonNav_StartProgress(&progress, -2147483647 - 1, 0, 0);
	Check(!HudsonNav_IsStuck(&progress, 2147483647, 0, 0, kHudsonNavStuckAfter), "the two ends of the numbers are a long way apart");

	// 65536 away: squared in 32 bits, that comes out as nothing at all
	HudsonNav_StartProgress(&progress, 0, 0, 0);
	Check(!HudsonNav_IsStuck(&progress, 65536, 0, 0, kHudsonNavStuckAfter), "a distance whose square doesn't fit in 32 bits still counts");

	// Nearly stuck, and then as long a frame as there can be: added as it stands, that would come out below zero, and he'd never be stuck
	HudsonNav_StartProgress(&progress, 0, 0, 0);
	progress.timeHere = kHudsonNavStuckAfter - 1;
	Check(HudsonNav_IsStuck(&progress, 0, 0, 0, 2147483647), "a frame as long as a number can say, on top of what's there, doesn't overflow");
}

static void CheckTheRays()
{
	printf("What a ray found:\n");

	Check(HudsonNav_ReadRay(false, false, 0) == kHudsonNavRayOpen, "nothing hit is open");
	Check(HudsonNav_ReadRay(false, true, -65536) == kHudsonNavRayOpen, "and it doesn't matter what the other numbers say");
	Check(HudsonNav_ReadRay(true, true, -65536) == kHudsonNavRayDoor, "a door is a door, even lying flat");

	// y goes down, so a floor faces -y. Each kind is checked at its edge, from both sides, because that's where a wrong number shows.
	Check(HudsonNav_ReadRay(true, false, -65536) == kHudsonNavRayFlatFloor, "facing straight up is a flat floor");
	Check(HudsonNav_ReadRay(true, false, -kHudsonNavFlatFloor) == kHudsonNavRayFlatFloor, "and so is a floor just inside what counts as flat");
	Check(HudsonNav_ReadRay(true, false, -kHudsonNavFlatFloor + 1) == kHudsonNavRaySlope, "any more angled than that is a slope");
	Check(HudsonNav_ReadRay(true, false, -kHudsonNavWalkable) == kHudsonNavRaySlope, "as far as the steepest he can walk on");
	Check(HudsonNav_ReadRay(true, false, -kHudsonNavWalkable + 1) == kHudsonNavRayTooSteep, "and any steeper is too steep");
	Check(HudsonNav_ReadRay(true, false, -kHudsonNavUpright) == kHudsonNavRayTooSteep, "as far as nearly upright");
	Check(HudsonNav_ReadRay(true, false, -kHudsonNavUpright + 1) == kHudsonNavRayWall, "and past that it's a wall");
	Check(HudsonNav_ReadRay(true, false, 0) == kHudsonNavRayWall, "facing sideways is a wall");
	Check(HudsonNav_ReadRay(true, false, kHudsonNavUpright - 1) == kHudsonNavRayWall, "so is one leaning very slightly over");
	Check(HudsonNav_ReadRay(true, false, kHudsonNavUpright) == kHudsonNavRayCeiling, "and any more than that is a ceiling");
	Check(HudsonNav_ReadRay(true, false, 65536) == kHudsonNavRayCeiling, "as is facing straight down");
	Check(kHudsonNavWalkable == 30000, "walkable is what the game's collisions call a floor: FLOOR_THRESHOLD in dynamics.cpp");

	std::string const words[7] = { HudsonNav_RayWord(kHudsonNavRayOpen), HudsonNav_RayWord(kHudsonNavRayFlatFloor), HudsonNav_RayWord(kHudsonNavRaySlope), HudsonNav_RayWord(kHudsonNavRayTooSteep), HudsonNav_RayWord(kHudsonNavRayWall), HudsonNav_RayWord(kHudsonNavRayCeiling), HudsonNav_RayWord(kHudsonNavRayDoor) };

	Check(words[0] == "open" && words[1] == "flat" && words[2] == "slope" && words[3] == "too_steep" && words[4] == "wall" && words[5] == "ceiling" && words[6] == "door", "each has its word");
	Check(std::string(HudsonNav_RayWord((HudsonNavRayKind)99)) == "?", "a kind that doesn't exist doesn't crash");
}

static void CheckTheTurns()
{
	printf("Turning a direction:\n");

	int const whole = kHudsonNavOneWhole;
	int x = 0, z = 0;

	HudsonNav_TurnEighths(0, whole, 0, &x, &z);
	Check(x == 0 && z == whole, "no turn at all");

	HudsonNav_TurnEighths(0, whole, 2, &x, &z);
	Check(x == whole && z == 0, "a quarter turn from straight ahead is to his right");

	HudsonNav_TurnEighths(0, whole, 4, &x, &z);
	Check(x == 0 && z == -whole, "a half turn is behind him");

	HudsonNav_TurnEighths(0, whole, 6, &x, &z);
	Check(x == -whole && z == 0, "three quarters is to his left");

	HudsonNav_TurnEighths(whole, 0, 2, &x, &z);
	Check(x == 0 && z == -whole, "facing right, a quarter turn to the right faces back");

	HudsonNav_TurnEighths(0, whole, 1, &x, &z);
	Check(x == 46341 && z == 46341, "one eighth is halfway to his right");

	HudsonNav_TurnEighths(0, whole, 3, &x, &z);
	Check(x == 46341 && z == -46341, "three eighths is behind him and to his right");

	HudsonNav_TurnEighths(0, whole, 5, &x, &z);
	Check(x == -46341 && z == -46341, "five eighths is behind him and to his left");

	HudsonNav_TurnEighths(0, whole, 7, &x, &z);
	Check(x == -46341 && z == 46341, "seven eighths is ahead of him and to his left");

	HudsonNav_TurnEighths(0, whole, 8, &x, &z);
	Check(x == 0 && z == whole, "eight eighths is all the way round");

	HudsonNav_TurnEighths(0, whole, -2, &x, &z);
	Check(x == -whole && z == 0, "turning back a quarter is to his left");

	HudsonNav_TurnEighths(0, whole, -9, &x, &z);
	Check(x == -46341 && z == 46341, "a long way back comes out in the right place");

	// A big number can't overflow on its way through
	HudsonNav_TurnEighths(2000000000, 2000000000, 2, &x, &z);
	Check(x == 2000000000 && z == -2000000000, "a big number comes through whole");
}

static void CheckTheSpiral()
{
	printf("The places to try in a room:\n");

	int x = 99, z = 99;

	HudsonNav_SpiralPlace(0, &x, &z);
	Check(x == 0 && z == 0, "the first is the middle");

	HudsonNav_SpiralPlace(-5, &x, &z);
	Check(x == 0 && z == 0, "and a number below that is too");

	// Every place is different, and they never get closer as they go
	std::vector<std::pair<int, int> > seen;
	int lastRing = 0;
	bool allDifferent = true;
	bool goingOut = true;

	for (int number = 0; number < kHudsonNavSpiralPlaces; number++)
	{
		HudsonNav_SpiralPlace(number, &x, &z);

		std::pair<int, int> const place(x, z);

		for (size_t i = 0; i < seen.size(); i++)
		{
			allDifferent = allDifferent && seen[i] != place;
		}

		seen.push_back(place);

		int const ring = ((abs(x) > abs(z)) ? abs(x) : abs(z)) / kHudsonNavSpiralStep;

		goingOut = goingOut && ring >= lastRing;
		lastRing = ring;
	}

	Check(allDifferent, "no place is tried twice");
	Check(goingOut, "each is at least as far out as the one before");
	Check(lastRing == 16, "and the last is 16 strides out");

	HudsonNav_SpiralPlace(1, &x, &z);
	Check(abs(x) + abs(z) == kHudsonNavSpiralStep, "the second is one stride from the middle");
}

static void CheckBeingAtASpot()
{
	printf("Whether he's at a spot:\n");

	int const within = kHudsonNavGuardStopsWithin;

	Check(HudsonNav_IsAtSpot(0, 0, 0, within), "on it");
	Check(HudsonNav_IsAtSpot(within, 0, 0, within), "exactly as far as it's allowed to be, sideways");
	Check(!HudsonNav_IsAtSpot(within + 1, 0, 0, within), "and no further");
	Check(HudsonNav_IsAtSpot(0, 0, within, within), "exactly as far, forwards");
	Check(!HudsonNav_IsAtSpot(0, 0, -within - 1, within), "and no further, backwards");
	Check(HudsonNav_IsAtSpot(0, within * 4, 0, within), "height only counts for a quarter");
	Check(!HudsonNav_IsAtSpot(0, -(within * 4 + 4), 0, within), "and it does count");
	Check(HudsonNav_IsAtSpot(kHudsonNavMiddleIsWithin, 0, 0, kHudsonNavMiddleIsWithin) && !HudsonNav_IsAtSpot(kHudsonNavMiddleIsWithin + 1, 0, 0, kHudsonNavMiddleIsWithin), "it's the reach it's given that counts");
	Check(!HudsonNav_IsAtSpot(65536, 0, 0, within), "a distance whose square doesn't fit in 32 bits isn't taken for nothing");
	Check(!HudsonNav_IsAtSpot(2000000000, 2000000000, 2000000000, within), "a very long way away doesn't overflow");
	Check(kHudsonNavGuardStopsWithin > 1500, "where a guard stops, which is 1500 in bh_marine.cpp, always counts as there");
}

static void CheckTheUprightTurn()
{
	printf("A clean matrix for the way he's facing:\n");

	int turn[9];

	Check(!HudsonNav_UprightTurn(0, 0, turn), "not facing any way at all gives nothing");

	Check(HudsonNav_UprightTurn(0, 65536, turn), "facing straight ahead");
	Check(turn[0] == 65536 && turn[1] == 0 && turn[2] == 0 && turn[3] == 0 && turn[4] == 65536 && turn[5] == 0 && turn[6] == 0 && turn[7] == 0 && turn[8] == 65536, "is the matrix that changes nothing");

	HudsonNav_UprightTurn(65536, 0, turn);
	Check(turn[6] == 65536 && turn[8] == 0 && turn[0] == 0 && turn[2] == -65536, "facing right, his right is backwards");

	// What the two logs showed, one too long and one too short, and some others
	int const ways[6][2] = { { -5695, -67407 }, { -24239, 53475 }, { 1, 0 }, { -1, -1 }, { 2147483647, 2147483647 }, { -2147483647 - 1, 7 } };

	for (int i = 0; i < 6; i++)
	{
		HudsonNav_UprightTurn(ways[i][0], ways[i][1], turn);

		double const forward = sqrt((double)turn[6] * turn[6] + (double)turn[8] * turn[8]);
		double const right = sqrt((double)turn[0] * turn[0] + (double)turn[2] * turn[2]);
		double const square = (double)turn[0] * turn[6] + (double)turn[2] * turn[8];

		Check(fabs(forward - 65536.0) < 1.5 && fabs(right - 65536.0) < 1.5, "whatever length it came in, it's one whole length now");
		Check(fabs(square) < 2.0, "and his right is square to where he's facing");
		Check(turn[1] == 0 && turn[3] == 0 && turn[4] == 65536 && turn[5] == 0 && turn[7] == 0, "and he's upright");

		// The same way: both point the same way round, and the sideways parts don't disagree
		double const cross = (double)ways[i][0] * turn[8] - (double)ways[i][1] * turn[6];
		double const along = (double)ways[i][0] * turn[6] + (double)ways[i][1] * turn[8];

		Check(along > 0.0 && fabs(cross) / along < 0.0001, "and he still faces the way he was facing");
	}
}

static std::vector<HudsonNavStep> routeSteps;

static const char *Route(std::string const &text)
{
	return HudsonNav_ReadRoute(text.c_str(), text.size(), &routeSteps);
}

static void CheckRefused(std::string const &text, const char *reason, std::string const &what)
{
	const char *const said = Route(text);

	Check(said == reason, what);
	Check(routeSteps.size() <= kHudsonNavMostRouteSteps, what + ", and the list has a limit");
}

static void CheckReadingARoute()
{
	printf("Reading a route:\n");

	Check(Route("walk 247 1589 -8301 START-EN01\npress 34528 -274 -44682 CORR-12\nwalkinto -21959 3548 -136864 V timer airlock area switch\nother destroy Some thing\n") == 0, "a good route");
	Check(routeSteps.size() == 4, "has all its steps");

	if (routeSteps.size() == 4)
	{
		Check(routeSteps[0].kind == kHudsonNavStepWalk && routeSteps[0].x == 247 && routeSteps[0].y == 1589 && routeSteps[0].z == -8301 && routeSteps[0].name == "START-EN01", "a walk");
		Check(routeSteps[1].kind == kHudsonNavStepPress && routeSteps[1].x == 34528 && routeSteps[1].y == -274 && routeSteps[1].z == -44682 && routeSteps[1].name == "CORR-12", "a press");
		Check(routeSteps[2].kind == kHudsonNavStepWalkInto && routeSteps[2].name == "V timer airlock area switch", "a walk-into, with spaces in its name");
		Check(routeSteps[3].kind == kHudsonNavStepOther && routeSteps[3].name == "destroy Some thing" && routeSteps[3].x == 0, "and one he can't do yet, with no place");
	}

	Check(Route("walk 1 2 3 A\r\nwalk 4 5 6 B\r\n") == 0 && routeSteps.size() == 2 && routeSteps[1].name == "B", "lines can end the way Windows ends them");
	Check(Route("walk 1 2 3 A") == 0 && routeSteps.size() == 1 && routeSteps[0].name == "A", "the last line needn't end in anything");
	Check(Route("# a comment\n\n\r\nwalk 1 2 3 A\n#walk 4 5 6 B\n\n") == 0 && routeSteps.size() == 1, "comments and blank lines are left out");
	Check(Route("walk   1   2   3   A\n") == 0 && routeSteps.size() == 1 && routeSteps[0].x == 1 && routeSteps[0].name == "A", "extra spaces between are fine");
	Check(Route("walk 10000000 -10000000 0 A\n") == 0 && routeSteps[0].x == 10000000 && routeSteps[0].y == -10000000, "as far as the world goes");

	// A short read must stop where it says, whatever's after it
	{
		std::string const longer = "walk 1 2 3 A\nwalk 4 5 6 B\n";

		Check(HudsonNav_ReadRoute(longer.c_str(), 13, &routeSteps) == 0 && routeSteps.size() == 1, "only what size says is read");
		Check(HudsonNav_ReadRoute(longer.c_str(), 0, &routeSteps) == kHudsonNavRouteEmpty, "and a size of nothing is nothing");
	}

	CheckRefused("", kHudsonNavRouteEmpty, "an empty file");
	CheckRefused("# only\n\n# comments\n", kHudsonNavRouteEmpty, "a file with only comments");
	CheckRefused("jump 1 2 3 A\n", kHudsonNavRouteUnknownWord, "a word that isn't a step");
	CheckRefused("WALK 1 2 3 A\n", kHudsonNavRouteUnknownWord, "a word in the wrong case");
	CheckRefused("walkabout 1 2 3 A\n", kHudsonNavRouteUnknownWord, "a word that only starts like one");
	CheckRefused(" walk 1 2 3 A\n", kHudsonNavRouteUnknownWord, "a line starting with a space");
	CheckRefused("walk\n", kHudsonNavRouteBadNumber, "a step with nothing after it");
	CheckRefused("walk 1 2\n", kHudsonNavRouteBadNumber, "two numbers");
	CheckRefused("walk 1 2 A\n", kHudsonNavRouteBadNumber, "two numbers and a name");
	CheckRefused("walk a 2 3 A\n", kHudsonNavRouteBadNumber, "a letter for a number");
	CheckRefused("walk 1.5 2 3 A\n", kHudsonNavRouteBadNumber, "a number with a point in it");
	CheckRefused("walk 1x 2 3 A\n", kHudsonNavRouteBadNumber, "a number with a letter after it");
	CheckRefused("walk 1 2 3x A\n", kHudsonNavRouteBadNumber, "the last number with a letter after it, which would otherwise look like the start of a name");
	CheckRefused("walk - 2 3 A\n", kHudsonNavRouteBadNumber, "a minus sign alone");
	CheckRefused("walk --1 2 3 A\n", kHudsonNavRouteBadNumber, "two minus signs");
	CheckRefused("walk +1 2 3 A\n", kHudsonNavRouteBadNumber, "a plus sign");
	CheckRefused("walk 10000001 2 3 A\n", kHudsonNavRouteBadNumber, "just outside the world");
	CheckRefused("walk -10000001 2 3 A\n", kHudsonNavRouteBadNumber, "just outside the other way");
	CheckRefused("walk 1 2 99999999999999999999999999 A\n", kHudsonNavRouteBadNumber, "more digits than any number can hold");
	CheckRefused("press 1 2 3\n", kHudsonNavRouteBadName, "no name");
	CheckRefused("press 1 2 3 \n", kHudsonNavRouteBadName, "a space for a name");
	CheckRefused("other\n", kHudsonNavRouteBadName, "other with nothing to say");
	CheckRefused("walk 1 2 3 ../../up\n", kHudsonNavRouteBadName, "a name with a character a name can't have");
	CheckRefused("walk 1 2 3 A%sB\n", kHudsonNavRouteBadName, "a name that would mean something to the log");
	CheckRefused(std::string("walk 1 2 3 A\0B\n", 15), kHudsonNavRouteBadName, "a name with a nothing in the middle of it");
	CheckRefused(std::string("walk 1 2 3 A\xE9\n"), kHudsonNavRouteBadName, "a character from past the ordinary ones");

	Check(Route("walk 1 2 3 " + std::string(kHudsonNavLongestStepName, 'n') + "\n") == 0, "a name as long as it can be");
	CheckRefused("walk 1 2 3 " + std::string(kHudsonNavLongestStepName + 1, 'n') + "\n", kHudsonNavRouteBadName, "and one longer");

	Check(Route("# " + std::string(kHudsonNavLongestRouteLine - 2, 'x') + "\nwalk 1 2 3 A\n") == 0, "a line as long as it can be");
	CheckRefused("# " + std::string(kHudsonNavLongestRouteLine - 1, 'x') + "\nwalk 1 2 3 A\n", kHudsonNavRouteLineTooLong, "and one longer, even a comment");

	// As many steps as there can be, and one more
	{
		std::string many;

		for (size_t i = 0; i < kHudsonNavMostRouteSteps; i++)
		{
			many += "walk 1 2 3 A\n";
		}

		Check(Route(many) == 0 && routeSteps.size() == kHudsonNavMostRouteSteps, "as many steps as there can be");
		CheckRefused(many + "walk 1 2 3 A\n", kHudsonNavRouteTooManySteps, "and one more");
	}

	// As long a file as there can be, and one byte more
	{
		std::string const comments(kHudsonNavMostRouteBytes - 13, '\n');

		Check(Route(comments + "walk 1 2 3 A\n") == 0, "a file as long as it can be");
		CheckRefused(comments + "\nwalk 1 2 3 A\n", kHudsonNavRouteTooLong, "and one byte more");
	}

	// Throwing away anything it already had
	Route("walk 1 2 3 A\n");
	Check(Route("jump\n") != 0, "a bad file");
	Check(Route("walk 4 5 6 B\n") == 0 && routeSteps.size() == 1 && routeSteps[0].name == "B", "and each read starts again");

	// Every short piece of a good file: it's refused or it's read, and nothing goes past the end. The address sanitizer is what sees that properly.
	{
		std::string const good = "walk 247 1589 -8301 START-EN01\r\npress 34528 -274 -44682 CORR-12\nother a\n";

		for (size_t i = 0; i <= good.size(); i++)
		{
			std::vector<char> exact(good.begin(), good.begin() + i);

			HudsonNav_ReadRoute(exact.empty() ? "" : &exact[0], exact.size(), &routeSteps);
		}

		Check(true, "every short piece of a good file was read without a crash");
	}
}

int main()
{
	CheckChoosingWhereNext();
	CheckBeingStuck();
	CheckTheRays();
	CheckTheTurns();
	CheckTheSpiral();
	CheckBeingAtASpot();
	CheckTheUprightTurn();
	CheckReadingARoute();

	printf("%d checks, %d failed\n", checksDone, checksFailed);

	if (checksFailed != 0 || checksDone == 0)
	{
		printf("The test FAILED\n");
		return 1;
	}

	printf("The test passed\n");
	return 0;
}
