// Tests for how Hudson feels his way, in a small made-up world of boxes. Written 2026-11-05 by Melanikus for the AvP port.

#include "hudson_nav_feel.h"
#include "hudson_nav_plan.h"
#include "hudson_nav_test_world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

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

static double const kSlant = 0.70710678;

static void CheckFreeAhead()
{
	printf("How far he can go:\n");

	World open;

	open.Floor(-20000, -20000, 20000, 20000, 0);

	Check(HudsonNav_FreeAhead(open, 0, 0, 0, 1.0, 0.0) == kHudsonNavFeelAhead, "on an open floor, as far as he feels");
	Check(HudsonNav_FreeAhead(open, 0, 0, 0, kSlant, kSlant) == kHudsonNavFeelAhead, "and the same on a slant");

	// A wall: what stops him is the front of his box, which is further on a slant
	World wall;

	wall.Floor(-20000, -20000, 20000, 20000, 0);
	wall.Wall(1000, -20000, 1100, 20000);

	Check(HudsonNav_FreeAhead(wall, 0, 0, 0, 1.0, 0.0) == 1000 - kHudsonNavMarineRadius, "a wall stops the front of his box");
	Check(abs(HudsonNav_FreeAhead(wall, 0, 0, 0, kSlant, kSlant) - 778) <= 2, "and on a slant, his box reaches further: 636, not 450");
	Check(HudsonNav_FreeAhead(wall, 0, 0, 0, -1.0, 0.0) == kHudsonNavFeelAhead, "and it doesn't matter behind him");

	// A post beside his way, which his edge would hit, but his middle wouldn't
	World post;

	post.Floor(-20000, -20000, 20000, 20000, 0);
	post.Add(600, -3000, 300, 700, 0, 500);

	Check(HudsonNav_FreeAhead(post, 0, 0, 0, 1.0, 0.0) < kHudsonNavFeelAhead, "a post his shoulder would hit stops him, though his middle would miss it");
	Check(HudsonNav_FreeAhead(post, 0, 0, 0, 1.0, 0.0) == 600 - kHudsonNavMarineRadius, "at the front of his box");

	World postOnTheOtherSide;

	postOnTheOtherSide.Floor(-20000, -20000, 20000, 20000, 0);
	postOnTheOtherSide.Add(600, -3000, -500, 700, 0, -300);

	Check(HudsonNav_FreeAhead(postOnTheOtherSide, 0, 0, 0, 1.0, 0.0) == 600 - kHudsonNavMarineRadius, "and it's the same on his other side");

	World postWellOff;

	postWellOff.Floor(-20000, -20000, 20000, 20000, 0);
	postWellOff.Add(600, -3000, 500, 700, 0, 600);

	Check(HudsonNav_FreeAhead(postWellOff, 0, 0, 0, 1.0, 0.0) == kHudsonNavFeelAhead, "further off to the side than he is wide, it doesn't matter");

	// On a slant his box is wider. A post that the corner of him would hit, and that he'd miss if he were as wide as he is going straight.
	World postOnTheSlant;

	postOnTheSlant.Floor(-20000, -20000, 20000, 20000, 0);
	postOnTheSlant.Add(110, -3000, 960, 170, 0, 1020);

	Check(HudsonNav_FreeAhead(postOnTheSlant, 0, 0, 0, kSlant, kSlant) < kHudsonNavFeelAhead, "a post his corner would hit, on a slant, stops him");

	// A beam at his head: his knee goes under it, and the rays down start beside it
	World beam;

	beam.Floor(-20000, -20000, 20000, 20000, 0);
	beam.Add(700, -1800, -3000, 710, -1600, 3000);

	Check(HudsonNav_FreeAhead(beam, 0, 0, 0, 1.0, 0.0) == 700 - kHudsonNavMarineRadius, "a beam at his head stops the front of his box");

	// Something at his knee and no higher, between where the rays down land, at each edge of him and in his middle
	for (int offset = -400; offset <= 400; offset += 400)
	{
		World low;

		low.Floor(-20000, -20000, 20000, 20000, 0);
		low.Add(650, -800, offset - 120, 660, -400, offset + 120);

		Check(HudsonNav_FreeAhead(low, 0, 0, 0, 1.0, 0.0) == 650 - kHudsonNavMarineRadius, "a rail at his knee stops him, wherever across him it is, at the front of his box");
	}

	World rail;

	rail.Floor(-20000, -20000, 20000, 20000, 0);
	rail.Add(1000, -800, -3000, 1010, -400, 3000);

	Check(HudsonNav_FreeAhead(rail, 0, 0, 0, 1.0, 0.0) == 1000 - kHudsonNavMarineRadius, "and it's the front of his box that it stops");

	// At his knee and up on a stair he's got to: the ray has to go from the height he's reached, or it would go under this one
	World railOnAStair;

	railOnAStair.Floor(-20000, -20000, 20000, 20000, 0);
	railOnAStair.Add(600, -400, -3000, 20000, 0, 3000);
	railOnAStair.Add(1000, -1200, -3000, 1010, -800, 3000);

	Check(HudsonNav_FreeAhead(railOnAStair, 0, 0, 0, 1.0, 0.0) < kHudsonNavFeelAhead, "a rail at his knee, up a stair, stops him too");

	// The floor
	// The floor is looked at under the front of his box, a foot at a time: at 700, 950 and 1200 from his middle here
	World ledge;

	ledge.Floor(-20000, -20000, 1000, 20000, 0);
	ledge.Floor(1000, -20000, 20000, 20000, 5000);

	Check(HudsonNav_FreeAhead(ledge, 0, 0, 0, 1.0, 0.0) == 2 * kHudsonNavFootStep, "the edge of a drop stops him at the foot before it");

	World step;

	step.Floor(-20000, -20000, 1000, 20000, 0);
	step.Floor(1000, -20000, 20000, 20000, kHudsonNavStepUp);

	Check(HudsonNav_FreeAhead(step, 0, 0, 0, 1.0, 0.0) == kHudsonNavFeelAhead, "a step down isn't a drop");

	World bigStep;

	bigStep.Floor(-20000, -20000, 1000, 20000, 0);
	bigStep.Floor(1000, -20000, 20000, 20000, kHudsonNavStepUp + 1);

	Check(HudsonNav_FreeAhead(bigStep, 0, 0, 0, 1.0, 0.0) == 2 * kHudsonNavFootStep, "and any more than that is");

	World nothing;

	nothing.Floor(-20000, -20000, 1000, 20000, 0);

	Check(HudsonNav_FreeAhead(nothing, 0, 0, 0, 1.0, 0.0) == 2 * kHudsonNavFootStep, "no floor at all stops him");

	WorldThatMisleads misleading(nothing);

	Check(HudsonNav_FreeAhead(misleading, 0, 0, 0, 1.0, 0.0) == 2 * kHudsonNavFootStep, "and a ray that hit nothing has hit nothing, whatever else it says");

	World steep;

	steep.Floor(-20000, -20000, 1000, 20000, 0);
	steep.Add(1000, -100, -20000, 20000, 0, 20000, false, -kHudsonNavWalkable + 1);

	Check(HudsonNav_FreeAhead(steep, 0, 0, 0, 1.0, 0.0) == 2 * kHudsonNavFootStep, "something too steep to stand on stops him");

	// A high stair, with a slope's face and a floor's
	World highFlat;

	highFlat.Floor(-20000, -20000, 1000, 20000, 0);
	highFlat.Add(1000, -kHudsonNavStepUp - 1, -20000, 20000, 0, 20000);

	Check(HudsonNav_FreeAhead(highFlat, 0, 0, 0, 1.0, 0.0) < kHudsonNavFeelAhead, "a flat floor that's higher than a step stops him");

	World justAStep;

	justAStep.Floor(-20000, -20000, 1000, 20000, 0);
	justAStep.Add(1000, -kHudsonNavStepUp, -20000, 20000, 0, 20000);

	Check(HudsonNav_FreeAhead(justAStep, 0, 0, 0, 1.0, 0.0) == kHudsonNavFeelAhead, "and one that's a step isn't a problem");

	// A door: doors open
	World door;

	door.Floor(-20000, -20000, 20000, 20000, 0);
	door.Add(600, -3000, -3000, 700, 0, 3000, true);

	Check(HudsonNav_FreeAhead(door, 0, 0, 0, 1.0, 0.0) == kHudsonNavFeelAhead, "a door doesn't stop him, at his head or his knee");

	World lowDoor;

	lowDoor.Floor(-20000, -20000, 20000, 20000, 0);
	lowDoor.Add(600, -1000, -3000, 700, 0, 3000, true);

	Check(HudsonNav_FreeAhead(lowDoor, 0, 0, 0, 1.0, 0.0) == kHudsonNavFeelAhead, "and nor does the top of one count as a floor");
}

static void AddStairs(World &world, int rise, int along)
{
	world.Floor(-4000, -20000, 4000, 30000, 0);

	for (int stair = 0; stair < 12; stair++)
	{
		world.Add(-4000, -(stair + 1) * rise, 500 + stair * along, 4000, 0, 30000);
	}
}

static void CheckStairs()
{
	printf("Stairs:\n");

	// The real size, from Derelict's COMM-ENT. On a stair, from every place along it, up and down, straight and on a slant.
	World stairs;

	AddStairs(stairs, 390, 780);

	bool up = true, upOnASlant = true, down = true, downOnASlant = true;

	for (int z = 0; z < 3000; z += 37)
	{
		// The height of the floor at this place
		int const stairsPassed = (z < 500) ? 0 : 1 + (z - 500) / 780;
		int const y = -stairsPassed * 390;

		up = up && HudsonNav_FreeAhead(stairs, 0, y, z, 0.0, 1.0) == kHudsonNavFeelAhead;
		upOnASlant = upOnASlant && HudsonNav_FreeAhead(stairs, 0, y, z, kSlant, kSlant) == kHudsonNavFeelAhead;
		down = down && HudsonNav_FreeAhead(stairs, 0, y, z + 3000, 0.0, -1.0) >= 0;
		downOnASlant = downOnASlant && HudsonNav_FreeAhead(stairs, 0, y, z + 3000, kSlant, -kSlant) >= 0;
	}

	Check(up, "real stairs are clear going straight up, from wherever he is on them");
	Check(upOnASlant, "and on a slant");

	int const atTheTop = -8 * 390;

	Check(HudsonNav_FreeAhead(stairs, 0, atTheTop, 6500, 0.0, -1.0) == kHudsonNavFeelAhead, "and going down");
	Check(HudsonNav_FreeAhead(stairs, 0, atTheTop, 6500, kSlant, -kSlant) == kHudsonNavFeelAhead, "and down on a slant");

	World tooHigh;

	AddStairs(tooHigh, kHudsonNavStepUp + 50, 780);

	Check(HudsonNav_FreeAhead(tooHigh, 0, 0, 0, 0.0, 1.0) < kHudsonNavFeelAhead, "stairs too high for his step are no way up");

	HudsonNavFeel const feel = HudsonNav_FeelTheWay(stairs, 0, 0, 0, 0.0, 1.0);

	Check(feel.canGo && feel.turned == 0, "and feeling his way, he goes on up real ones");
}

static void CheckChoosingAWay()
{
	printf("Choosing a way:\n");

	World open;

	open.Floor(-20000, -20000, 20000, 20000, 0);

	HudsonNavFeel feel = HudsonNav_FeelTheWay(open, 0, 0, 0, 1.0, 0.0);

	Check(feel.canGo && feel.turned == 0 && feel.x == 1.0 && feel.z == 0.0, "with nothing in the way, he goes the way he was asked");
	Check(feel.freeAhead == kHudsonNavFeelAhead, "and it says how far is clear");

	// A wall ahead, with a way round on one side
	World wallToOneSide;

	wallToOneSide.Floor(-20000, -20000, 20000, 20000, 0);
	wallToOneSide.Wall(700, -20000, 800, 400);

	feel = HudsonNav_FeelTheWay(wallToOneSide, 0, 0, 0, 1.0, 0.0);

	Check(feel.canGo, "a wall ahead with a way round it is no reason to stop");
	Check(feel.turned > 0, "and he turns the way that's clear");
	Check(feel.z > 0.0, "which is that side");
	Check(HudsonNav_FreeAhead(wallToOneSide, 0, 0, 0, feel.x, feel.z) >= kHudsonNavFeelEnough, "and that way really is clear");

	World wallToTheOtherSide;

	wallToTheOtherSide.Floor(-20000, -20000, 20000, 20000, 0);
	wallToTheOtherSide.Wall(700, -400, 800, 20000);
	wallToTheOtherSide.Wall(-20000, 600, 20000, 700);

	feel = HudsonNav_FeelTheWay(wallToTheOtherSide, 0, 0, 0, 1.0, 0.0);

	Check(feel.canGo && feel.turned < 0 && feel.z < 0.0, "and the other way, for a wall the other side");

	// An edge ahead, and the floor going on to one side
	World ledge;

	ledge.Floor(-20000, -20000, 500, 20000, 0);
	ledge.Floor(-20000, 400, 20000, 20000, 0);

	feel = HudsonNav_FeelTheWay(ledge, 0, 0, 0, 1.0, 0.0);

	Check(feel.canGo && feel.z > 0.0, "he won't walk off an edge, if the floor goes on to one side");

	// Nothing he can do
	World boxedIn;

	boxedIn.Floor(-20000, -20000, 20000, 20000, 0);
	boxedIn.Wall(600, -20000, 700, 20000);
	boxedIn.Wall(-700, -20000, -600, 20000);
	boxedIn.Wall(-20000, 600, 20000, 700);
	boxedIn.Wall(-20000, -700, 20000, -600);

	feel = HudsonNav_FeelTheWay(boxedIn, 0, 0, 0, 1.0, 0.0);

	Check(!feel.canGo, "boxed in, he can't go");
	Check(feel.x == 1.0 && feel.z == 0.0 && feel.turned == 0, "and it hands back the way he asked for");

	// Only just far enough to be worth it, and only just not
	World justEnough;

	justEnough.Floor(-20000, -20000, 20000, 20000, 0);
	justEnough.Wall(kHudsonNavMarineRadius + kHudsonNavFeelEnough, -20000, kHudsonNavMarineRadius + kHudsonNavFeelEnough + 100, 20000);
	justEnough.Wall(-kHudsonNavMarineRadius - 100, -20000, -kHudsonNavMarineRadius, 20000);
	justEnough.Wall(-20000, kHudsonNavMarineRadius, 20000, kHudsonNavMarineRadius + 100);
	justEnough.Wall(-20000, -kHudsonNavMarineRadius - 100, 20000, -kHudsonNavMarineRadius);

	feel = HudsonNav_FeelTheWay(justEnough, 0, 0, 0, 1.0, 0.0);

	Check(feel.canGo && feel.freeAhead == kHudsonNavFeelEnough, "clear for just far enough, he goes");

	World notEnough;

	notEnough.Floor(-20000, -20000, 20000, 20000, 0);
	notEnough.Wall(kHudsonNavMarineRadius + kHudsonNavFeelEnough - 1, -20000, kHudsonNavMarineRadius + kHudsonNavFeelEnough + 100, 20000);
	notEnough.Wall(-kHudsonNavMarineRadius - 100, -20000, -kHudsonNavMarineRadius, 20000);
	notEnough.Wall(-20000, kHudsonNavMarineRadius, 20000, kHudsonNavMarineRadius + 100);
	notEnough.Wall(-20000, -kHudsonNavMarineRadius - 100, 20000, -kHudsonNavMarineRadius);

	feel = HudsonNav_FeelTheWay(notEnough, 0, 0, 0, 1.0, 0.0);

	Check(!feel.canGo, "and any less, he doesn't");

	// It's the same answer each time, and it has a limit
	World room;

	room.Floor(-20000, -20000, 20000, 20000, 0);
	room.Wall(700, -20000, 800, 400);

	HudsonNavFeel const first = HudsonNav_FeelTheWay(room, 0, 0, 0, 1.0, 0.0);
	HudsonNavFeel const second = HudsonNav_FeelTheWay(room, 0, 0, 0, 1.0, 0.0);

	Check(first.x == second.x && first.z == second.z && first.turned == second.turned, "the same place gives the same answer");

	room.asked = 0;
	HudsonNav_FeelTheWay(room, 0, 0, 0, 1.0, 0.0);
	Check(room.asked > 0 && room.asked <= kHudsonNavMostFeelRays, "and it doesn't use more rays than it says");

	boxedIn.asked = 0;
	HudsonNav_FeelTheWay(boxedIn, 0, 0, 0, 1.0, 0.0);
	Check(boxedIn.asked > 0 && boxedIn.asked <= kHudsonNavMostFeelRays, "even when nothing's clear");

	open.asked = 0;
	HudsonNav_FeelTheWay(open, 0, 0, 0, 1.0, 0.0);
	Check(open.asked <= 35, "and in the open, it stops as soon as it knows");
}

static void CheckKeepingToTheMiddle()
{
	printf("Keeping to the middle:\n");

	// A corridor 4 metres wide, going along x
	World corridor;

	corridor.Floor(-20000, -20000, 20000, 20000, 0);
	corridor.Wall(-20000, -2100, 20000, -2000);
	corridor.Wall(-20000, 2000, 20000, 2100);

	HudsonNavFeel feel = HudsonNav_FeelTheWay(corridor, 0, 0, 0, 1.0, 0.0);

	Check(feel.canGo && feel.x == 1.0 && feel.z == 0.0, "in the middle, he goes straight");

	feel = HudsonNav_FeelTheWay(corridor, 0, 0, 1000, 1.0, 0.0);

	Check(feel.canGo && feel.z < 0.0, "near one wall, he turns towards the middle");
	Check(feel.turned == 0, "which doesn't count as turning from the way he was asked");
	Check(feel.z > -sin(kHudsonNavMostTurnFromAWall * 3.14159265358979 / 180.0) - 0.001, "and only a little");

	feel = HudsonNav_FeelTheWay(corridor, 0, 0, -1000, 1.0, 0.0);

	Check(feel.canGo && feel.z > 0.0, "and the same from the other wall");

	// Further off than he minds, and near enough for him to see it: if it were all the same to him, he'd lean away from this one
	World wide;

	wide.Floor(-20000, -20000, 20000, 20000, 0);
	wide.Wall(-20000, kHudsonNavNearAWall + 100, 20000, kHudsonNavNearAWall + 200);

	feel = HudsonNav_FeelTheWay(wide, 0, 0, 0, 1.0, 0.0);

	Check(feel.x == 1.0 && feel.z == 0.0, "a wall further off than that makes no difference");

	// A door along his way: doors open, so it's no wall to lean away from
	World doorAlongside;

	doorAlongside.Floor(-20000, -20000, 20000, 20000, 0);
	doorAlongside.Add(-20000, -3000, 800, 20000, 0, 900, true);

	feel = HudsonNav_FeelTheWay(doorAlongside, 0, 0, 0, 1.0, 0.0);

	Check(feel.x == 1.0 && feel.z == 0.0, "a door alongside him isn't a wall to lean away from");

	// Turning away from a wall won't take him where he can't go: a post right where that would take him
	World postInTheWay;

	postInTheWay.Floor(-20000, -20000, 20000, 20000, 0);
	postInTheWay.Wall(-20000, 1400, 20000, 1500);

	feel = HudsonNav_FeelTheWay(postInTheWay, 0, 0, 500, 1.0, 0.0);

	Check(feel.canGo && HudsonNav_FreeAhead(postInTheWay, 0, 0, 500, feel.x, feel.z) >= kHudsonNavFeelEnough, "where he ends up going is clear");
}

static void CheckMostRoom()
{
	printf("The way with the most room:\n");

	World corner;

	corner.Floor(-20000, -20000, 20000, 20000, 0);
	corner.Wall(600, -20000, 700, 20000);
	corner.Wall(-20000, 600, 20000, 700);

	HudsonNavFeel room = HudsonNav_MostRoom(corner, 0, 0, 0, 1.0, 0.0);

	Check(room.canGo && room.freeAhead == kHudsonNavFeelAhead, "out of a corner, there's a way with room");
	Check(HudsonNav_FreeAhead(corner, 0, 0, 0, room.x, room.z) == kHudsonNavFeelAhead, "and it is one");
	Check(room.x <= 0.001, "and it isn't the wall he's up against");

	// In the open, and every way as good as another, it's to the side of the way that didn't work
	World open;

	open.Floor(-20000, -20000, 20000, 20000, 0);

	room = HudsonNav_MostRoom(open, 0, 0, 0, 1.0, 0.0);

	Check(fabs(room.x) < 0.001 && fabs(room.z) > 0.999, "with room all round, he goes to the side of where he couldn't get");

	room = HudsonNav_MostRoom(open, 0, 0, 0, 0.0, 1.0);

	Check(fabs(room.z) < 0.001 && fabs(room.x) > 0.999, "whichever way that was");

	World boxedIn;

	boxedIn.Floor(-20000, -20000, 20000, 20000, 0);
	boxedIn.Wall(500, -20000, 600, 20000);
	boxedIn.Wall(-600, -20000, -500, 20000);
	boxedIn.Wall(-20000, 500, 20000, 600);
	boxedIn.Wall(-20000, -600, 20000, -500);

	room = HudsonNav_MostRoom(boxedIn, 0, 0, 0, 1.0, 0.0);

	Check(!room.canGo, "boxed in, there's nowhere");
}

int main()
{
	CheckFreeAhead();
	CheckStairs();
	CheckChoosingAWay();
	CheckKeepingToTheMiddle();
	CheckMostRoom();

	printf("%d checks, %d failed\n", checksDone, checksFailed);

	if (checksFailed != 0 || checksDone == 0)
	{
		printf("The test FAILED\n");
		return 1;
	}

	printf("The test passed\n");
	return 0;
}
