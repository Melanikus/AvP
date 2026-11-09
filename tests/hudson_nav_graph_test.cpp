// Tests for Hudson's map, in a small made-up world of boxes. Written 2026-11-05 by Melanikus for the AvP port. See 3dc/avp/hudson_nav_graph.h.
//
// In this world, as in the game, y goes down. A floor at height 0 is a box from y = 0 to y = 100: a smaller y is higher up.

#include "hudson_nav_graph.h"
#include "hudson_nav_plan.h"
#include "hudson_nav_test_world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
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

static HudsonNavSearch RunIt(HudsonNavGraph &map, World &world, int raysEachTime = 1000000)
{
	HudsonNavSearch search = kHudsonNavSearching;

	for (int i = 0; i < 1000000 && search == kHudsonNavSearching; i++)
	{
		search = map.Work(world, raysEachTime);
	}

	return search;
}

// True if every step of the way really can be walked in the world: no wall at his knees, nor at his head.
static bool WayIsClear(HudsonNavGraph &map, World &world)
{
	std::vector<HudsonNavPoint> const &way = map.Way();

	for (size_t i = 0; i + 1 < way.size(); i++)
	{
		if (abs(way[i].y - way[i + 1].y) <= kHudsonNavStepUp && !map.LineIsClear(world, way[i].x, way[i].y, way[i].z, way[i + 1].x, way[i + 1].z, false))
		{
			return false;
		}
	}

	return true;
}

// A floor 40 metres square, and a wall all round it
static void AddRoom(World &world, int height = 0)
{
	world.Floor(0, 0, 40000, 40000, height);
}

static void CheckTheStraightWay()
{
	printf("An open floor:\n");

	World world;
	AddRoom(world);

	HudsonNavGraph map;

	map.StartSearch(5500, 0, 5500, 15500, 0, 5500);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way across an open floor");
	Check(map.Way().size() >= 2, "with points on it");
	Check(map.Way().back().x == 15500 && map.Way().back().z == 5500, "and it ends where he was going");
	Check(map.Way().front().x == 5500 && map.Way().front().z == 5500, "and starts where he stands");
	Check(WayIsClear(map, world), "and it can all be walked");

	// It's nearly straight: the first point that's close enough is ten strides off, less the reach of the goal
	Check(map.Way().size() <= 12, "and it doesn't wander");
	Check(map.Spots() < 400, "and it only made as much map as it needed");
}

static void CheckGoingRoundACorner()
{
	printf("Round a corner:\n");

	// An L-shaped corridor: east along z = 0 to 3000, then north along x = 20000 to 23000. Everything else is wall.
	World world;

	world.Floor(0, 0, 23000, 3000, 0);
	world.Floor(20000, 0, 23000, 20000, 0);
	world.Wall(-1000, -1000, 24000, 0);        // the long side, all along z = 0
	world.Wall(-1000, 3000, 20000, 4000);      // the inside of the bend's far side
	world.Wall(23000, -1000, 24000, 21000);    // the far side of the second leg
	world.Wall(19000, 3000, 20000, 21000);     // the inside of the second leg
	world.Wall(-1000, -1000, 0, 4000);         // the end of the first leg
	world.Wall(19000, 20000, 24000, 21000);    // the end of the second leg

	HudsonNavGraph map;

	Check(!map.LineIsClear(world, 2000, 0, 1500, 21500, 15000, false), "the straight line from one end to the other goes through a wall");

	map.StartSearch(2000, 0, 1500, 21500, 0, 15000);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way round the corner");
	Check(WayIsClear(map, world), "and every step of it can be walked");

	bool wentRound = false;

	for (size_t i = 0; i < map.Way().size(); i++)
	{
		wentRound = wentRound || (map.Way()[i].x >= 20000 && map.Way()[i].z <= 3000);
	}

	Check(wentRound, "and it goes by way of the corner");

	// Where he is along the way, he can see some of it, and not all of it
	int const furthest = map.FurthestInSight(world, 2000, 0, 1500, 12);

	Check(furthest >= 1, "from the start he can see along the corridor");
	Check(map.Way()[furthest].z <= 3000, "but not round the corner");
	Check(map.NextAlong() >= 0, "and there's always a next point");

	// Near the bend, the way ahead goes round it, and he can't see the part round the corner. A place with the wall between him and it.
	{
		int const nearBend = map.FurthestInSight(world, 19000, 0, 2500, 20);

		Check(nearBend >= 0, "near the bend he can still see some of it");

		// A line down his middle can miss a corner that his shoulder would hit
		World corner;

		corner.Floor(0, 0, 20000, 20000, 0);
		corner.Wall(5000, 5000, 6000, 6000);

		HudsonNavGraph wide;

		Check(wide.LineIsClear(corner, 2000, 0, 6100, 9000, 6300, false), "a line just past a corner is clear, down his middle");
		Check(!wide.WayIsWideEnough(corner, 2000, 0, 6100, 9000, 6300), "but it's too close for his shoulders");
		Check(wide.WayIsWideEnough(corner, 2000, 0, 7000, 9000, 7000), "and one further off is wide enough");

		// So what he's said to be able to see is what his shoulders can get past as well
		wide.StartSearch(2000, 0, 6100, 9000, 0, 6300);
		Check(RunIt(wide, corner) == kHudsonNavFound, "there's a way past the corner");
		Check(wide.FurthestInSight(corner, 2000, 0, 6100, 50) != (int)wide.Way().size() - 1, "and the far end doesn't count as seen, because his shoulder would hit the corner on the way");

		// What's in the way of his middle counts too, though his shoulders would miss it: a post at his knees, and a beam at his head
		World post;

		post.Floor(0, 0, 20000, 20000, 0);
		post.Add(5400, -3000, 7000, 5600, 0, 7200);

		HudsonNavGraph narrow;

		Check(!narrow.WayIsWideEnough(post, 2000, 0, 7100, 9000, 7100), "a thin post right ahead is in the way, though it's between his shoulders");

		World beam;

		beam.Floor(0, 0, 20000, 20000, 0);
		beam.Add(5000, -kHudsonNavHeadHeight - 100, 0, 5200, -kHudsonNavHeadHeight + 100, 20000);

		Check(!narrow.WayIsWideEnough(beam, 2000, 0, 7100, 9000, 7100), "and so is a beam at his head, which the rays at his knees go under");

		if (nearBend >= 0)
		{
			Check(map.LineIsClear(world, 19000, 0, 2500, map.Way()[nearBend].x, map.Way()[nearBend].z, false), "and what he can see, he really can");
			Check((size_t)nearBend + 1 < map.Way().size(), "and it isn't simply the last of it, which is out of sight");
		}
	}

	// A wall right across, so there's no way
	world.Wall(10000, 0, 10100, 3000);

	HudsonNavGraph blocked;

	blocked.StartSearch(2000, 0, 1500, 21500, 0, 15000);
	Check(RunIt(blocked, world) == kHudsonNavNoWay, "a wall right across means there's no way");
	Check(!blocked.Way().empty() && blocked.Way().back().x < 10000, "and the way goes as near as it could get, on his own side");
	Check(blocked.Spots() < 200, "and it finishes when there's nowhere left to look");
}

static void CheckFloorsAboveEachOther()
{
	printf("Floors above each other:\n");

	// A lower floor at height 3000, an upper at height 0 over part of it, joined by stairs of 400 a step going up along z.
	World world;

	world.Floor(0, 0, 30000, 30000, 3000);
	world.Floor(0, 15000, 30000, 30000, 0);

	for (int step = 0; step < 8; step++)
	{
		int const top = 3000 - (step + 1) * 375;

		world.Add(5000, top, 7000 + step * 1000, 10000, 3000, 8000 + step * 1000);
	}

	// Under the upper floor there's room, because it's 3000 up
	HudsonNavGraph map;

	map.StartSearch(7500, 3000, 2500, 7500, 3000, 25000);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way to the lower floor under the upper one");
	Check(map.Way().back().y == 3000, "and he ends on the lower one");

	bool stayedDown = true;

	for (size_t i = 0; i < map.Way().size(); i++)
	{
		stayedDown = stayedDown && map.Way()[i].y >= 2500;
	}

	Check(stayedDown, "and he doesn't go by way of the upper one");

	HudsonNavGraph up;

	up.StartSearch(7500, 3000, 2500, 7500, 0, 25000);
	Check(RunIt(up, world) == kHudsonNavFound, "there's a way from the lower floor to the upper one, straight above where the other way ended");
	Check(up.Way().back().y == 0, "and he ends on the upper one");

	bool climbed = false, inSteps = true;

	for (size_t i = 0; i + 1 < up.Way().size(); i++)
	{
		climbed = climbed || (up.Way()[i].z > 7000 && up.Way()[i].z < 15000 && up.Way()[i].y < 3000);
		inSteps = inSteps && abs(up.Way()[i].y - up.Way()[i + 1].y) <= kHudsonNavStepUp;
	}

	Check(climbed, "by way of the stairs");
	Check(inSteps, "and never more than a step at a time");

	// The same map, one floor and then the other: the same x and z can be two different spots
	HudsonNavGraph both;

	both.StartSearch(7500, 3000, 2500, 7500, 3000, 25000);
	RunIt(both, world);

	both.StartSearch(7500, 3000, 2500, 7500, 0, 25000);
	Check(RunIt(both, world) == kHudsonNavFound, "a second search on the same map finds the other floor");
	Check(both.Way().back().y == 0, "and ends on it");

	// Along the same line, one floor directly above the other
	both.StartSearch(7500, 3000, 16500, 7500, 3000, 25000);
	RunIt(both, world);

	std::vector<HudsonNavPoint> const lowerWay = both.Way();

	both.StartSearch(7500, 0, 16500, 7500, 0, 25000);
	RunIt(both, world);

	bool sameSpotTwoFloors = false;

	for (size_t i = 0; i + 1 < lowerWay.size(); i++)
	{
		for (size_t j = 0; j + 1 < both.Way().size(); j++)
		{
			sameSpotTwoFloors = sameSpotTwoFloors || (lowerWay[i].x == both.Way()[j].x && lowerWay[i].z == both.Way()[j].z && lowerWay[i].y == 3000 && both.Way()[j].y == 0);
		}
	}

	Check(sameSpotTwoFloors, "the same place at two different heights is two different spots, on one map");

	// Coming down, he takes the stairs, though stepping off the edge would be a little shorter. These stairs have walls, so that there's no stepping off them halfway.
	{
		World walled;

		walled.Floor(0, 0, 30000, 30000, 3000);
		walled.Floor(0, 15000, 30000, 30000, 0);

		for (int step = 0; step < 8; step++)
		{
			int const top = 3000 - (step + 1) * 375;

			walled.Add(5000, top, 7000 + step * 1000, 10000, 3000, 8000 + step * 1000);
		}

		walled.Wall(4900, 7000, 5000, 15000);
		walled.Wall(10000, 7000, 10100, 15000);

		HudsonNavGraph down;

		down.StartSearch(7500, 0, 16500, 11500, 3000, 8000);
		Check(RunIt(down, walled) == kHudsonNavFound, "there's a way down from the upper floor");

		bool neverDropped = true;

		for (size_t i = 0; i + 1 < down.Way().size(); i++)
		{
			neverDropped = neverDropped && down.Way()[i + 1].y - down.Way()[i].y <= kHudsonNavStepUp;
		}

		Check(neverDropped, "and he takes the stairs, because a drop counts as a long way round");
	}

	// Stairs with the real numbers, from Derelict's COMM-ENT: 390 up and 780 along. The first map didn't know these for stairs, and he went back and forth at the bottom until he was picked up.
	{
		World flight;

		flight.Floor(0, 0, 4000, 30000, 0);

		for (int stair = 0; stair < 10; stair++)
		{
			flight.Add(0, -(stair + 1) * 390, 5000 + stair * 780, 4000, 0, 30000);
		}

		flight.Wall(-100, 0, 0, 30000);
		flight.Wall(4000, 0, 4100, 30000);

		int const top = -10 * 390;

		HudsonNavGraph up;

		up.StartSearch(2000, 0, 2500, 2000, top, 16500);
		Check(RunIt(up, flight) == kHudsonNavFound, "real stairs are a way up");

		std::vector<HudsonNavPoint> const &climb = up.Way();
		bool neverBackDown = true, neverBackAlong = true;
		double walked = 0.0;

		for (size_t i = 0; i + 1 < climb.size(); i++)
		{
			neverBackDown = neverBackDown && climb[i + 1].y <= climb[i].y;
			neverBackAlong = neverBackAlong && climb[i + 1].z >= climb[i].z;
			walked += sqrt(pow((double)climb[i + 1].x - climb[i].x, 2) + pow((double)climb[i + 1].y - climb[i].y, 2) + pow((double)climb[i + 1].z - climb[i].z, 2));
		}

		double const straight = sqrt(pow(0.0 - top, 2) + pow(16500.0 - 2500.0, 2));

		Check(neverBackDown, "and the way never goes back down");
		Check(neverBackAlong, "or back along");
		Check(walked < straight * 1.1, "and it's no longer than it has to be");
		Check(up.Way().back().y == top, "and it gets to the top");
		Check(up.SlopeSpots() == 0 && up.Spots() > 10, "and it's stairs that got him there, all flat");

		HudsonNavGraph down;

		down.StartSearch(2000, top, 16500, 2000, 0, 2500);
		Check(RunIt(down, flight) == kHudsonNavFound, "and they're a way down, too");

		double walkedDown = 0.0;

		for (size_t i = 0; i + 1 < down.Way().size(); i++)
		{
			walkedDown += abs(down.Way()[i + 1].z - down.Way()[i].z) + abs(down.Way()[i + 1].x - down.Way()[i].x);
		}

		Check(walkedDown < 14000.0 * 1.1, "and going down them doesn't count as a drop, so he doesn't go looking for another way");

		// Part way up, which is where he'd be if he were asked again
		HudsonNavGraph halfway;

		halfway.StartSearch(2000, -4 * 390, 8500, 2000, top, 16500);
		Check(RunIt(halfway, flight) == kHudsonNavFound, "and he can set off from any stair");

		// Square on, the map's already at the far end of a stride before the second stair: one that's further on, on the slant, has to be right too
		HudsonNavGraph slanting;

		slanting.StartSearch(500, 0, 2500, 3500, top, 16500);
		Check(RunIt(slanting, flight) == kHudsonNavFound, "and from one side of them to the other");

		// Stairs too high for his step, however easy they look, aren't a way up
		World tooHigh;

		tooHigh.Floor(0, 0, 4000, 30000, 0);

		for (int stair = 0; stair < 10; stair++)
		{
			tooHigh.Add(0, -(stair + 1) * (kHudsonNavStepUp + 50), 5000 + stair * 780, 4000, 0, 30000);
		}

		tooHigh.Wall(-100, 0, 0, 30000);
		tooHigh.Wall(4000, 0, 4100, 30000);

		HudsonNavGraph refused;

		refused.StartSearch(2000, 0, 2500, 2000, -10 * (kHudsonNavStepUp + 50), 16500);
		Check(RunIt(refused, tooHigh) == kHudsonNavNoWay, "stairs higher than his step aren't a way up");

		// A rail too low for the ray at his head, and between where the rays down land: only the ray at his knee can see it
		World rail;

		rail.Floor(0, 0, 20000, 4000, 0);
		rail.Add(10100, -800, 0, 10110, -400, 4000);

		HudsonNavGraph stopped;

		stopped.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(stopped, rail) == kHudsonNavNoWay, "a rail across the way stops him");

		// The same rail, above stairs he's gone up: only a ray from his new knee height can see it, because one from the bottom goes underneath it, at the height of the stairs he's climbed
		World railAtTheTop;

		railAtTheTop.Floor(0, 0, 20000, 4000, 0);

		for (int stair = 0; stair < 4; stair++)
		{
			railAtTheTop.Add(5000 + stair * 780, -(stair + 1) * 390, 0, 20000, 0, 4000);
		}

		railAtTheTop.Add(10100, -1560 - 800, 0, 10110, -1560 - 400, 4000);

		HudsonNavGraph stoppedAtTheTop;

		stoppedAtTheTop.StartSearch(2500, 0, 1500, 15500, -1560, 1500);
		Check(RunIt(stoppedAtTheTop, railAtTheTop) == kHudsonNavNoWay, "and so does one at the top of some stairs");
	}

	// Other things only one ray can see
	{
		// A beam at his head, which the rays at his knee go under, and the rays down start beside
		World beam;

		beam.Floor(0, 0, 20000, 4000, 0);
		beam.Add(10100, -1800, 0, 10110, -1600, 4000);

		HudsonNavGraph underTheBeam;

		underTheBeam.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(underTheBeam, beam) == kHudsonNavNoWay, "a beam at his head stops him");

		// Two drops in one stride, each of which he'd survive, and which together he wouldn't
		World twoDrops;

		twoDrops.Floor(0, 0, 10000, 4000, 0);
		twoDrops.Floor(10000, 0, 10400, 4000, kHudsonNavMostDrop * 5 / 8);
		twoDrops.Floor(10400, 0, 20000, 4000, kHudsonNavMostDrop * 10 / 8);

		HudsonNavGraph overTheEdge;

		overTheEdge.StartSearch(5500, 0, 1500, 15500, kHudsonNavMostDrop * 10 / 8, 1500);
		Check(RunIt(overTheEdge, twoDrops) == kHudsonNavNoWay, "two drops in a stride add up");

		// A door low enough that the top of it is under where the rays down start. He isn't climbing onto that, because doors open.
		World lowDoor;

		lowDoor.Floor(0, 0, 20000, 4000, 0);
		lowDoor.Add(10000, -1000, 0, 10300, 0, 4000, true);

		HudsonNavGraph throughIt;

		throughIt.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(throughIt, lowDoor) == kHudsonNavFound, "a low door is a way through, and not a wall or a floor");

		// A thin low door, between where the rays down land, right in the way, and an open gap a little further round. Only the ray at his knee sees this door, and he'd rather not wait for one.
		World choice;

		choice.Floor(0, 0, 20000, 8000, 0);
		choice.Wall(10100, 0, 10110, 1000);
		choice.Add(10100, -1000, 1000, 10110, 0, 3000, true);
		choice.Wall(10100, 5000, 10110, 8000);

		HudsonNavGraph byTheGap;

		byTheGap.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(byTheGap, choice) == kHudsonNavFound, "there's a way past the wall");

		bool wentByTheGap = false;

		for (size_t i = 0; i + 1 < byTheGap.Way().size(); i++)
		{
			// Where this piece of the way crosses the wall. It can cross on the slant, from just below the edge of the gap.
			wentByTheGap = wentByTheGap || (byTheGap.Way()[i].x < 10000 && byTheGap.Way()[i + 1].x > 10100 && (byTheGap.Way()[i].z + byTheGap.Way()[i + 1].z) / 2 >= 3000);
		}

		Check(wentByTheGap, "and it's by the gap, because a door at his knee costs what a door does");

		// A stride that was a drop, and then isn't, because of what's there now. The map keeps its spots, and it mustn't keep this.
		World edge;

		edge.Floor(0, 0, 10000, 8000, 0);
		edge.Floor(10000, 0, 20000, 8000, 900);
		edge.Add(10000, 450, 4000, 10400, 900, 8000);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 900, 1500);
		Check(RunIt(map, edge) == kHudsonNavFound, "there's a way down");

		bool wentRound = false;

		for (size_t i = 0; i < map.Way().size(); i++)
		{
			wentRound = wentRound || map.Way()[i].z >= 4000;
		}

		Check(wentRound, "and it's round by the stairs, rather than a drop");

		edge.Add(10000, 450, 0, 10400, 900, 4000);

		map.StartSearch(5500, 0, 1500, 15500, 900, 1500);
		Check(RunIt(map, edge) == kHudsonNavFound, "and there's one with a stair right in front of him");

		bool wentRoundAgain = false;

		for (size_t i = 0; i < map.Way().size(); i++)
		{
			wentRoundAgain = wentRoundAgain || map.Way()[i].z >= 4000;
		}

		Check(!wentRoundAgain, "and it isn't a drop any more, so he goes straight");
	}

	// A wall exactly where a foot lands, on the slant, where a foot's a fraction of a millimetre long. The ray from the last foot has to reach it, and the ray from this one starts on it.
	{
		World thin;

		thin.Floor(0, 0, 20000, 8000, 0);
		thin.Add(10000, -5000, 0, 10000, 5000, 8000);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 2500);
		Check(RunIt(map, thin) == kHudsonNavNoWay, "a wall exactly at a foot still stops him, on the slant as well as straight on");

		// That goes for a door as well: he has to know it's there, or he'd go through it for nothing rather than round by the gap. This one's low, so that only the ray at his knee can see it.
		World doorAtAFoot;

		doorAtAFoot.Floor(0, 0, 20000, 8000, 0);
		doorAtAFoot.Wall(10000, 0, 10000, 1000);
		doorAtAFoot.Add(10000, -1000, 1000, 10000, 0, 3000, true);
		doorAtAFoot.Wall(10000, 5000, 10000, 8000);

		HudsonNavGraph byTheGap;

		byTheGap.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(byTheGap, doorAtAFoot) == kHudsonNavFound, "there's a way past a door exactly at a foot");

		bool wentByTheGap = false;

		for (size_t i = 0; i + 1 < byTheGap.Way().size(); i++)
		{
			wentByTheGap = wentByTheGap || (byTheGap.Way()[i].x < 10000 && byTheGap.Way()[i + 1].x > 10000 && (byTheGap.Way()[i].z + byTheGap.Way()[i + 1].z) / 2 >= 3000);
		}

		Check(wentByTheGap, "and it knows the door's there, so it goes by the gap");
	}

	// Two flights side by side, going down. The real ones have strides that go down 780, which is more than a step, and that's still only stairs. The other flight is gentler, and a little further to walk.
	{
		World flights;

		flights.Floor(0, 0, 10000, 5000, 0);

		for (int stair = 0; stair < 10; stair++)
		{
			flights.Add(0, -(stair + 1) * 390, 5000 + stair * 780, 4000, 0, 30000);
		}

		for (int stair = 0; stair < 20; stair++)
		{
			flights.Add(4100, -(stair + 1) * 195, 5000 + stair * 500, 8100, 0, 30000);
		}

		flights.Wall(4000, 5000, 4100, 15000);
		flights.Add(4000, -3900, 15000, 4100, 0, 30000);

		HudsonNavGraph down;

		down.StartSearch(2000, -3900, 17500, 2000, 0, 2500);
		Check(RunIt(down, flights) == kHudsonNavFound, "there's a way down either flight");

		bool stayedOnTheNearOne = true;

		for (size_t i = 0; i < down.Way().size(); i++)
		{
			stayedOnTheNearOne = stayedOnTheNearOne && down.Way()[i].x < 4000;
		}

		Check(stayedOnTheNearOne, "and he takes the nearer one: a long stride down stairs isn't a drop, to be gone a long way round to miss");
	}

	// How the last search ended
	{
		World floor;

		floor.Floor(0, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		Check(map.LastSearch() == kHudsonNavNoWay, "before any search, there's no way");

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(map.LastSearch() == kHudsonNavSearching, "while it's looking, it says so");
		Check(map.Way().empty(), "and there's no way to look at yet");
		Check(RunIt(map, floor) == kHudsonNavFound && map.LastSearch() == kHudsonNavFound, "when it finds one, it says so");
		Check(map.Work(floor, 1000) == kHudsonNavFound, "and asking again says the same");

		map.StartSearch(5500, 0, 1500, 15500, 0, 90000);
		Check(RunIt(map, floor) == kHudsonNavNoWay && map.LastSearch() == kHudsonNavNoWay, "when it can't, it says so");
		Check(!map.Way().empty(), "though there's a way as near as it got");
		Check(map.Work(floor, 1000) == kHudsonNavNoWay, "and asking again says the same, and not that it found one");

		map.Clear();
		Check(map.LastSearch() == kHudsonNavNoWay, "and a new level starts again");
	}

	// The list of spots, as something drawing them would see it
	{
		World floorAndDrop;

		floorAndDrop.Floor(0, 0, 10000, 4000, 0);
		floorAndDrop.Floor(10000, 0, 20000, 4000, 3000);
		floorAndDrop.Add(0, -100, 1900, 3000, 0, 2100, true);

		HudsonNavGraph map;

		Check(map.Spots() == 0, "a new map has no spots");

		HudsonNavSpot const nothing = map.SpotAt(0);

		Check(nothing.x == 0 && nothing.y == 0 && nothing.z == 0 && nothing.next[0] == -1 && nothing.next[7] == -1 && nothing.doors == 0 && nothing.drops == 0 && !nothing.sloping, "and a number that isn't a spot gives nothing, and no ways out");

		map.StartSearch(9500, 0, 1500, 15500, 3000, 1500);
		Check(RunIt(map, floorAndDrop) == kHudsonNavFound, "there's a way down the drop");
		Check(map.Spots() > 0, "and that made spots");
		Check(map.SpotAt(map.Spots()).next[0] == -1 && map.SpotAt(map.Spots() + 1000).x == 0, "one past the end is still nothing");

		HudsonNavSpot const first = map.SpotAt(0);

		Check(first.x == 9500 && first.y == 0 && first.z == 1500, "the first spot is where he started, on the floor");

		bool goesSomewhere = false, reallyGoes = true, hasADrop = false, dropIsOneWay = true;

		for (size_t i = 0; i < map.Spots(); i++)
		{
			HudsonNavSpot const spot = map.SpotAt(i);

			for (int way = 0; way < 8; way++)
			{
				if (spot.next[way] >= 0)
				{
					goesSomewhere = true;
					reallyGoes = reallyGoes && (size_t)spot.next[way] < map.Spots();

					if (spot.drops & (1u << way))
					{
						hasADrop = true;
						dropIsOneWay = dropIsOneWay && map.SpotAt(spot.next[way]).y > spot.y;
					}
				}
			}
		}

		Check(goesSomewhere, "its ways out lead somewhere");
		Check(reallyGoes, "and only to spots that are on the list");
		Check(hasADrop, "and it says which of them has a drop in it");
		Check(dropIsOneWay, "and that goes downwards");

		// A slope says so, and a door way says so
		World sloping;

		sloping.Floor(0, 0, 10000, 4000, 0);
		sloping.Add(10000, -200, 0, 20000, 0, 4000, false, -kHudsonNavWalkable);

		HudsonNavGraph slopeMap;

		slopeMap.StartSearch(9500, 0, 1500, 15500, -200, 1500);
		RunIt(slopeMap, sloping);

		bool sawASlope = false, sawFlat = false;

		for (size_t i = 0; i < slopeMap.Spots(); i++)
		{
			sawASlope = sawASlope || slopeMap.SpotAt(i).sloping;
			sawFlat = sawFlat || !slopeMap.SpotAt(i).sloping;
		}

		Check(sawASlope && sawFlat, "and it says which are on a slope");

		World withADoor;

		withADoor.Floor(0, 0, 20000, 4000, 0);
		withADoor.Add(10100, -3000, 0, 10110, 0, 4000, true);

		HudsonNavGraph doorMap;

		doorMap.StartSearch(9500, 0, 1500, 15500, 0, 1500);
		RunIt(doorMap, withADoor);

		bool sawADoor = false;

		for (size_t i = 0; i < doorMap.Spots(); i++)
		{
			sawADoor = sawADoor || doorMap.SpotAt(i).doors != 0;
		}

		Check(sawADoor, "and which of its ways goes through a door");
	}

	// The end of a way, which is where he stops rather than going past it and back
	{
		World room;

		room.Floor(0, 0, 20000, 20000, 0);

		HudsonNavGraph map;

		Check(!map.HasComeToTheEnd(0, 0), "with no way there's no end of it");

		map.StartSearch(2500, 0, 2500, 12500, 0, 2500);
		Check(RunIt(map, room) == kHudsonNavFound, "there's a way across the room");

		HudsonNavPoint const last = map.Way().back();

		Check(map.HasComeToTheEnd(last.x, last.z), "on its last point he's at the end of it");
		Check(map.HasComeToTheEnd(last.x - kHudsonNavEndOfTheWayIsWithin, last.z), "and this close to it");
		Check(map.HasComeToTheEnd(last.x, last.z + kHudsonNavEndOfTheWayIsWithin), "whichever side he's on");
		Check(!map.HasComeToTheEnd(last.x - kHudsonNavEndOfTheWayIsWithin - 1, last.z), "but not any further off");
		Check(!map.HasComeToTheEnd(last.x, last.z - kHudsonNavEndOfTheWayIsWithin - 1), "either way");
		Check(!map.HasComeToTheEnd(map.Way().front().x, map.Way().front().z), "and at the start he isn't");
	}

	// A line along the floor, and one off the edge of it
	{
		World ledge;

		ledge.Floor(0, 0, 10000, 4000, 0);
		ledge.Floor(10000, 0, 20000, 4000, 5000);

		HudsonNavGraph map;

		Check(map.LineIsClear(ledge, 5500, 0, 1500, 15500, 1500, false), "a line off a ledge has nothing in the way of it");
		Check(!map.LineHasFloor(ledge, 5500, 0, 1500, 15500, 1500, 40), "but it has no floor all the way along");
		Check(map.LineHasFloor(ledge, 2500, 0, 1500, 8500, 1500, 40), "and one that stays on the floor has");
		Check(map.LineHasFloor(ledge, 5500, 0, 1500, 15500, 1500, 3), "and it's only looked for as far as it was asked");
		Check(map.LineHasFloor(ledge, 5500, 0, 1500, 5500, 1500, 40), "and a line of no length needs none");

		// A ray that found nothing has found nothing, whatever else its answer says
		WorldThatMisleads misleadingLedge(ledge);

		Check(!map.LineHasFloor(misleadingLedge, 5500, 0, 1500, 15500, 1500, 40), "a ray that hit nothing isn't a floor, though it says it's a level one at the right height");

		World up;

		up.Floor(0, 0, 20000, 4000, 0);

		for (int stair = 0; stair < 10; stair++)
		{
			up.Add(5000 + stair * 1000, -(stair + 1) * 300, 0, 20000, 0, 4000);
		}

		Check(map.LineHasFloor(up, 2500, 0, 1500, 12500, 1500, 40), "and a line going up stairs has floor, however far up it goes, so long as he goes up with it");

		// Up the stairs and off the far end, back to the height he started at. It's only a drop from where he's got to.
		World upAndOff;

		upAndOff.Floor(0, 0, 20000, 4000, 0);

		for (int stair = 0; stair < 8; stair++)
		{
			upAndOff.Add(5000 + stair * 1000, -(stair + 1) * 300, 0, 13000, 0, 4000);
		}

		Check(!map.LineHasFloor(upAndOff, 2500, 0, 1500, 16500, 1500, 40), "and the floor past the top of some stairs is a drop, though it's as high as where he started");

		World lower;

		lower.Floor(0, 0, 10000, 4000, 0);
		lower.Floor(10000, 0, 20000, 4000, kHudsonNavStepUp);

		Check(map.LineHasFloor(lower, 5500, 0, 1500, 15500, 1500, 40), "a step down as far as he can step is still floor");

		World steep;

		steep.Floor(0, 0, 10000, 4000, 0);
		steep.Add(10000, 0, 0, 20000, 100, 4000, false, -kHudsonNavWalkable + 1);

		Check(!map.LineHasFloor(steep, 5500, 0, 1500, 15500, 1500, 40), "something too steep to stand on isn't floor");

		World door;

		door.Floor(0, 0, 20000, 4000, 0);
		door.Add(10000, -3000, 0, 10100, 0, 4000, true);

		Check(map.LineHasFloor(door, 5500, 0, 1500, 15500, 1500, 40), "and a door on the way doesn't count against it");

		// A door isn't something he climbs onto. If it were taken for a floor, the real floor past it would be too far below him.
		World lowDoor;

		lowDoor.Floor(0, 0, 20000, 4000, 0);
		lowDoor.Add(10000, -1000, 0, 11000, 0, 4000, true);

		Check(map.LineHasFloor(lowDoor, 5500, 0, 1500, 15500, 1500, 40), "and the top of a door isn't a floor he's gone up onto");
	}

	// And it doesn't get mixed up, going back the first way
	both.StartSearch(7500, 3000, 2500, 7500, 3000, 25000);
	Check(RunIt(both, world) == kHudsonNavFound && both.Way().back().y == 3000, "and the first floor is still there");
}

static void CheckSteps()
{
	printf("Steps up and down:\n");

	// A step as high as he can go, and one a millimetre higher, each right across a corridor
	for (int rise = kHudsonNavStepUp; rise <= kHudsonNavStepUp + 1; rise++)
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -rise, 0, 20000, 0, 4000);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, -rise, 1500);

		HudsonNavSearch const search = RunIt(map, world);

		if (rise == kHudsonNavStepUp)
		{
			Check(search == kHudsonNavFound, "a step as high as the game lifts him can be walked");
		}
		else
		{
			Check(search == kHudsonNavNoWay, "and one a millimetre higher can't");
		}
	}

	// A drop he can step off, and can't come back up
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Floor(10000, 0, 20000, 4000, 2500);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 2500, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "a drop can be stepped off");

		map.StartSearch(15500, 2500, 1500, 5500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and it only goes one way");
	}

	// A stride that takes in two stairs, each as high as he can go. A wall the same height, with nothing halfway up, isn't a way.
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);

		for (int stair = 0; stair < 10; stair++)
		{
			world.Add(5000 + stair * 500, -(stair + 1) * 250, 0, 20000, 0, 4000);
		}

		HudsonNavGraph map;

		map.StartSearch(2500, 0, 1500, 12500, -2500, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "stairs of 250 every half a metre, so 500 every stride, can be walked");

		World wall;

		wall.Floor(0, 0, 20000, 4000, 0);
		wall.Add(10000, -500, 0, 20000, 0, 4000);

		HudsonNavGraph stopped;

		stopped.StartSearch(5500, 0, 1500, 15500, -500, 1500);
		Check(RunIt(stopped, wall) == kHudsonNavNoWay, "and a single step of 500 can't");
	}

	// Something halfway at the right height, but too steep to stand on, isn't a stair
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(9900, -250, 0, 10100, 0, 4000, false, -kHudsonNavWalkable + 1);
		world.Add(10100, -500, 0, 20000, 0, 4000);

		HudsonNavGraph map;

		map.StartSearch(9500, 0, 1500, 15500, -500, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a stair halfway has to be one he can stand on");
	}

	// A low stair halfway, and then more than a step up from that, isn't two stairs
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(9900, -100, 0, 10100, 0, 4000);
		world.Add(10100, -580, 0, 20000, 0, 4000);

		HudsonNavGraph map;

		map.StartSearch(9500, 0, 1500, 15500, -580, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and it has to be no more than a step below the next one, as well as above the last");
	}

	// A step that's too high halfway and then comes down again isn't a stair either
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -600, 0, 10990, 0, 4000);
		world.Add(10990, -500, 0, 20000, 0, 4000);

		HudsonNavGraph map;

		map.StartSearch(9500, 0, 1500, 15500, -500, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a ledge higher than a step, halfway across a stride, isn't a stair");
	}

	// A drop further than he'd go
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Floor(10000, 0, 20000, 4000, kHudsonNavMostDrop + 100);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, kHudsonNavMostDrop + 100, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a drop further than he'd go isn't a way");
	}

	// A hole: nothing to stand on
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Floor(13000, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a gap in the floor isn't a way");

		// However a ray that found nothing says so
		WorldThatMisleads misleading(world);
		HudsonNavGraph misled;

		misled.StartSearch(5500, 0, 1500, 15500, 0, 1500);

		HudsonNavSearch search = kHudsonNavSearching;

		for (int i = 0; i < 100000 && search == kHudsonNavSearching; i++)
		{
			search = misled.Work(misleading, 1000000);
		}

		Check(search == kHudsonNavNoWay, "and a ray that found nothing is nothing, whatever else it says");
	}

	// A beam above head height and below the top of his head: the rays across all miss it, and only the one straight up finds there's no room for him
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10400, -kHudsonNavMarineHeight, 0, 10600, -kHudsonNavHeadHeight - 50, 4000);

		HudsonNavGraph map;

		Check(map.LineIsClear(world, 5500, 0, 1500, 15500, 1500, false), "a beam just over his eyes doesn't stop a line");

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "but there's no room for him under it");
	}

	// A ceiling too low
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -kHudsonNavMarineHeight + 100, 0, 11000, -kHudsonNavKneeHeight - 10, 4000);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a ceiling too low to get under isn't a way");
	}

	// A low wall: under his eyes, and what stuck him in CORR-17
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -kHudsonNavKneeHeight - 100, 0, 11000, 0, 4000);

		HudsonNavGraph map;

		Check(!map.LineIsClear(world, 5500, 0, 1500, 15500, 1500, false), "a low wall stops the line at his knees");
		Check(map.LineIsClear(world, 5500, -1000, 1500, 15500, 1500, false), "though a line higher up would miss it");

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and it isn't a way");
	}

	// A wall at head height only, leaving room to duck, which he can't
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);
		world.Add(10000, -kHudsonNavHeadHeight - 100, 0, 11000, -kHudsonNavHeadHeight + 100, 4000);

		HudsonNavGraph map;

		Check(!map.LineIsClear(world, 5500, 0, 1500, 15500, 1500, false), "a beam stops the line at his head");
	}
}

static void CheckSlopes()
{
	printf("Slopes:\n");

	// Boxes can't slope, so each rises as a stair would, a foot along from the last, and says it's a slope. The rise is at each foot.
	struct Row { int faces; int rise; bool canGo; const char *what; };

	Row const rows[] =
	{
		{ -kHudsonNavFlatFloor + 1, kHudsonNavStepUp + 30, true, "a slope can rise further than a stair at one foot" },
		{ -kHudsonNavWalkable, kHudsonNavStepUp + 30, true, "the steepest he can walk on" },
		{ -kHudsonNavWalkable + 1, 100, false, "and any steeper can't be stood on, however little it rises" },
		{ -kHudsonNavFlatFloor, kHudsonNavStepUp + 30, false, "a floor that's flat, as high as that, is a wall and not a slope" },
		{ -kHudsonNavFlatFloor, kHudsonNavStepUp, true, "and as high as a step, it's a stair" },
		{ 0, 100, false, "something upright isn't a floor" },
		{ -kHudsonNavFlatFloor + 1, 2 * kHudsonNavFootStep + 10, false, "and even a slope can only rise so far at one foot" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);

		for (int foot = 0; foot < 8; foot++)
		{
			world.Add(10000 + foot * kHudsonNavFootStep, -(foot + 1) * rows[i].rise, 0, 12000, 0, 4000, false, rows[i].faces);
		}

		HudsonNavGraph map;

		// From well back, where he has to go up it to get there
		map.StartSearch(5500, 0, 1500, 11500, -8 * rows[i].rise, 1500);

		HudsonNavSearch const search = RunIt(map, world);

		Check((search == kHudsonNavFound) == rows[i].canGo, rows[i].what);
	}

	// A slope too high for how short the step is. The floor he's on is so small that this is his only way off it.
	{
		World world;

		world.Add(9800, 0, 1000, 10000, 100, 2000);
		world.Add(10000, -1500, 1000, 12000, 0, 2000, false, -kHudsonNavFlatFloor + 1);

		HudsonNavGraph map;

		map.StartSearch(9900, 0, 1500, 11500, -1500, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "a slope can only rise twice as far as the step is long");
	}

	// The map counts them apart
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);
		world.Add(10000, -200, 0, 20000, 0, 4000, false, -kHudsonNavWalkable);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, -200, 1500);
		RunIt(map, world);

		Check(map.FlatSpots() > 0 && map.SlopeSpots() > 0, "flat spots and sloping ones are counted apart");
		Check(map.FlatSpots() + map.SlopeSpots() == map.Spots(), "and every spot is one or the other");
	}
}

static void CheckDoors()
{
	printf("Doors:\n");

	// Two rooms with a wall between, and a door in the wall
	World world;

	world.Floor(0, 0, 20000, 10000, 0);
	world.Wall(10000, 0, 10100, 4000);
	world.Wall(10000, 6000, 10100, 10000);
	world.Add(10000, -3000, 4000, 10100, 0, 6000, true);

	HudsonNavGraph map;

	map.StartSearch(5500, 0, 5500, 15500, 0, 5500);
	Check(RunIt(map, world) == kHudsonNavFound, "there's a way through a shut door, because doors open");
	Check(map.LineIsClear(world, 5500, 0, 5500, 15500, 5500, false), "and a line through one counts as clear");
	Check(!map.LineIsClear(world, 5500, 0, 5500, 15500, 5500, true), "unless it's asked to count them");

	// A door that isn't on the way, with an open way that's about as short: he takes the open way
	World twoWays;

	twoWays.Floor(0, 0, 20000, 10000, 0);
	twoWays.Wall(10000, 0, 10100, 3000);
	twoWays.Add(10000, -3000, 3000, 10100, 0, 5000, true);
	twoWays.Wall(10000, 5000, 10100, 6000);
	twoWays.Wall(10000, 8000, 10100, 10000);

	HudsonNavGraph choosing;

	choosing.StartSearch(5500, 0, 4500, 15500, 0, 4500);
	Check(RunIt(choosing, twoWays) == kHudsonNavFound, "with a door and an open way");

	bool usedTheOpening = false;

	for (size_t i = 0; i < choosing.Way().size(); i++)
	{
		usedTheOpening = usedTheOpening || (choosing.Way()[i].z >= 6000 && choosing.Way()[i].z <= 8000);
	}

	Check(usedTheOpening, "he goes round by the open way, when it's about as short");
}

static void CheckWhatChanges()
{
	printf("What changes between one search and the next:\n");

	// A wall that wasn't there before: a door has shut, or the room it's in has only just loaded
	{
		World world;

		world.Floor(0, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "an open way");

		world.Wall(10000, 0, 10100, 4000);

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and with a wall that's come since, the old steps don't still count");
	}

	// A floor that wasn't there before: the room it's in has only just loaded
	{
		World world;

		world.Floor(0, 0, 10000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavNoWay, "no way, with the far room not there");

		world.Floor(10000, 0, 20000, 4000, 0);

		map.StartSearch(5500, 0, 1500, 15500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "and once it's there, the spots he's looked at before are looked at again, and there is");
	}

	// A door that's gone: a way through it stops costing extra
	{
		World world;

		world.Floor(0, 0, 20000, 10000, 0);
		world.Wall(10000, 0, 10100, 3000);
		world.Wall(10000, 5000, 10100, 6000);
		world.Wall(10000, 8000, 10100, 10000);
		world.Add(10000, -3000, 3000, 10100, 0, 5000, true);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 4500, 15500, 0, 4500);
		RunIt(map, world);

		bool byTheOpening = false;

		for (size_t i = 0; i < map.Way().size(); i++)
		{
			byTheOpening = byTheOpening || (map.Way()[i].z >= 6000 && map.Way()[i].z <= 8000);
		}

		Check(byTheOpening, "with the door there, he goes round by the open way");

		world.RemoveLast();

		map.StartSearch(5500, 0, 4500, 15500, 0, 4500);
		RunIt(map, world);

		bool stillRound = false;

		for (size_t i = 0; i < map.Way().size(); i++)
		{
			stillRound = stillRound || (map.Way()[i].z >= 6000 && map.Way()[i].z <= 8000);
		}

		Check(!stillRound, "and with it gone, he goes straight through where it was");
	}

	// Which place on the grid he's in, on each side of zero: a spot already known on his side of zero is the one he starts from, and not one on the other side
	{
		World world;

		world.Floor(-20000, 0, 20000, 4000, 0);

		HudsonNavGraph map;

		map.StartSearch(100, 0, 1500, -5500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound, "a way across zero");

		map.StartSearch(-100, 0, 1500, -5500, 0, 1500);
		Check(RunIt(map, world) == kHudsonNavFound && map.Way().front().x == -500, "and just below zero he's in the place on the grid below zero");
	}
}

static void CheckTheLimits()
{
	printf("The limits:\n");

	// Work uses about what it's told. One spot's worth over at most.
	{
		World world;
		AddRoom(world);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 5500, 35500, 0, 35500);

		int calls = 0;
		bool neverTooMany = true;
		HudsonNavSearch search = kHudsonNavSearching;

		while (search == kHudsonNavSearching && calls < 100000)
		{
			int const before = world.asked;

			search = map.Work(world, 48);
			calls++;
			neverTooMany = neverTooMany && world.asked - before <= 48 + kHudsonNavMostRaysForASpot;
		}

		Check(search == kHudsonNavFound, "a long way, a few rays at a time, gets there");
		Check(calls > 1, "and it did take more than one go");
		Check(neverTooMany, "and no go used more than it was told, and one spot's worth");
		Check(map.RaysUsed() == (uint64_t)world.asked, "and it counted them all");

		// The same way as with no limit: how fast it works doesn't change what it finds, so that two runs choose the same
		World same;
		AddRoom(same);

		HudsonNavGraph allAtOnce;

		allAtOnce.StartSearch(5500, 0, 5500, 35500, 0, 35500);
		RunIt(allAtOnce, same);

		bool sameWay = allAtOnce.Way().size() == map.Way().size();

		for (size_t i = 0; sameWay && i < map.Way().size(); i++)
		{
			sameWay = map.Way()[i].x == allAtOnce.Way()[i].x && map.Way()[i].y == allAtOnce.Way()[i].y && map.Way()[i].z == allAtOnce.Way()[i].z;
		}

		Check(sameWay, "and it's the same way as with all the rays at once");

		// Asking for nothing still gets somewhere, or it would never finish
		HudsonNavGraph slowest;
		World slow;
		AddRoom(slow);

		slowest.StartSearch(5500, 0, 5500, 8500, 0, 5500);

		HudsonNavSearch slowSearch = kHudsonNavSearching;

		for (int i = 0; i < 100 && slowSearch == kHudsonNavSearching; i++)
		{
			slowSearch = slowest.Work(slow, 0);
		}

		Check(slowSearch != kHudsonNavSearching, "being told it has no rays still gets it there in the end");
	}

	// A floor far too big, and nowhere to get to: it stops, at its limit
	{
		World world;

		world.Floor(-500000, -500000, 500000, 500000, 0);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 5500, 5500, -100000, 5500);

		HudsonNavSearch const search = RunIt(map, world);

		Check(search == kHudsonNavNoWay, "a floor with no end, and somewhere that can't be reached, ends with no way");
		Check(map.LastSearchLooks() <= kHudsonNavMostLooks, "and it looked at no more spots than it may");
		Check(map.Spots() <= kHudsonNavMostNodes, "and the map's no bigger than it may be");
	}

	// The map full: nothing new goes on it, and nothing goes wrong
	{
		World world;

		world.Floor(-500000, -500000, 500000, 500000, 0);

		HudsonNavGraph map;

		for (int i = 0; i < 12 && map.Spots() < kHudsonNavMostNodes; i++)
		{
			// A different place each time, each looking for somewhere it can't get to, so that each adds to the map
			map.StartSearch(-400000 + i * 70000, 0, -400000, 5500, -100000, 5500);
			RunIt(map, world);
		}

		Check(map.Spots() == kHudsonNavMostNodes, "searching and searching fills the map");

		map.StartSearch(300000, 0, 300000, 300000, -100000, 300000);
		Check(RunIt(map, world) == kHudsonNavNoWay, "and with the map full, a search from a new place ends with no way");
		Check(map.Spots() == kHudsonNavMostNodes, "and the map's no bigger");
		Check(map.Way().empty(), "and there's no way to give back");
		Check(map.FurthestInSight(world, 300000, 0, 300000, 12) == -1, "and nothing to see of it");
		Check(map.NextAlong() == -1, "and no next point");

		map.Clear();
		Check(map.Spots() == 0 && map.RaysUsed() == 0 && map.Way().empty(), "and it can be cleared for the next level");

		map.StartSearch(5500, 0, 5500, 8500, 0, 5500);
		Check(RunIt(map, world) == kHudsonNavFound, "and it works again after that");
	}

	// Below zero, where rounding could go wrong
	{
		World world;

		world.Floor(-20000, -20000, 0, 0, 0);

		HudsonNavGraph map;

		map.StartSearch(-15500, 0, -15500, -5500, 0, -5500);
		Check(RunIt(map, world) == kHudsonNavFound, "the map works on the far side of zero");
		Check(WayIsClear(map, world), "with the steps all clear");

		for (size_t i = 1; i + 1 < map.Way().size(); i++)
		{
			int const x = map.Way()[i].x, z = map.Way()[i].z;

			Check((-x) % kHudsonNavGridStep == kHudsonNavGridStep / 2 && (-z) % kHudsonNavGridStep == kHudsonNavGridStep / 2, "and each spot is in the middle of its place on the grid");
		}
	}

	// He can stand anywhere, on or off the grid, and a search with no distance at all finishes
	{
		World world;
		AddRoom(world);

		HudsonNavGraph map;

		map.StartSearch(5123, 0, 7777, 5123, 0, 7777);
		Check(RunIt(map, world) == kHudsonNavFound, "going nowhere is found straight away");
		Check(map.LineIsClear(world, 5000, 0, 5000, 5000, 5000, false), "and a line of no length is clear");

		// Called again when it's done: it just says how it ended
		Check(map.Work(world, 48) == kHudsonNavFound, "asking again says it's found");
	}

	// Going along the way: he can't go backwards on it, and being off it doesn't crash
	{
		World world;
		AddRoom(world);

		HudsonNavGraph map;

		map.StartSearch(5500, 0, 5500, 25500, 0, 5500);
		RunIt(map, world);

		int const first = map.FurthestInSight(world, 5500, 0, 5500, 12);

		Check(first > 0, "with an open floor he can see well along it");
		Check(first <= 12 && (size_t)first < map.Way().size(), "but no further than he asked");

		int const further = map.FurthestInSight(world, 15500, 0, 5500, 12);

		Check(further >= first, "and further along it he sees further on");
		Check(map.FurthestInSight(world, 15500, 0, 5500, 0) == -1, "and asking for nothing ahead gives nothing, and not the point he's standing on, which would leave him standing there");
		Check(map.NextAlong() >= 0, "and there's still a next point to go to");
		Check(map.FurthestInSight(world, 15500, -5000, 5500, 12) == -1, "and from another floor none of it counts as seen");
	}
}

int main()
{
	CheckTheStraightWay();
	CheckGoingRoundACorner();
	CheckFloorsAboveEachOther();
	CheckSteps();
	CheckSlopes();
	CheckDoors();
	CheckWhatChanges();
	CheckTheLimits();

	printf("%d checks, %d failed\n", checksDone, checksFailed);

	if (checksFailed != 0 || checksDone == 0)
	{
		printf("The test FAILED\n");
		return 1;
	}

	printf("The test passed\n");
	return 0;
}
