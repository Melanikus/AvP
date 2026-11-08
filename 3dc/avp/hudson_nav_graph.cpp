// Hudson's map of a level, made from what his lidar finds. Written 2026-11-05 by Melanikus for the AvP port. hudson_nav_graph.h says what it is.

#include "hudson_nav_graph.h"
#include "hudson_nav_plan.h"

#include <math.h>
#include <stdlib.h>
#include <algorithm>

// The eight ways round, in strides along x and z
static int const kWayX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static int const kWayZ[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

// Which place on the grid a number is in. Rounded down, whichever side of zero it's on.
static int GridOf(int value)
{
	int const grid = value / kHudsonNavGridStep;

	return (value < 0 && value % kHudsonNavGridStep != 0) ? grid - 1 : grid;
}

// Where the middle of that place on the grid is
static int MiddleOfGrid(int grid)
{
	return grid * kHudsonNavGridStep + kHudsonNavGridStep / 2;
}

static uint64_t ColumnKey(int gridX, int gridZ)
{
	return ((uint64_t)(uint32_t)gridX << 32) | (uint32_t)gridZ;
}

HudsonNavGraph::HudsonNavGraph()
{
	Clear();
}

void HudsonNavGraph::Clear()
{
	nodes.clear();
	columns.clear();
	cost.clear();
	cameFrom.clear();
	stamp.clear();
	waiting = std::priority_queue<Waiting>();
	way.clear();
	searchNumber = 0;
	order = 0;
	looks = 0;
	nearest = -1;
	nearestGuess = 0.0;
	toX = toY = toZ = 0;
	searching = false;
	alongTheWay = 0;
	flatSpots = slopeSpots = 0;
	raysUsed = 0;
	raysThisWork = 0;
}

HudsonNavRayAnswer HudsonNavGraph::Ray(HudsonNavRays &rays, int x, int y, int z, int directionX, int directionY, int directionZ, int longest)
{
	raysUsed++;
	raysThisWork++;

	return rays.Cast(x, y, z, directionX, directionY, directionZ, longest);
}

// The spot on this place on the grid that's the same floor as this height, or -1. Two floors can't be this close, because he has to be able to stand on each.
int HudsonNavGraph::NodeAt(int gridX, int gridZ, int floorHeight) const
{
	std::map<uint64_t, std::vector<int> >::const_iterator const column = columns.find(ColumnKey(gridX, gridZ));

	if (column == columns.end())
	{
		return -1;
	}

	for (size_t i = 0; i < column->second.size(); i++)
	{
		int const number = column->second[i];

		if (abs(nodes[number].y - floorHeight) <= kHudsonNavStepUp)
		{
			return number;
		}
	}

	return -1;
}

// -1 if the map is full.
int HudsonNavGraph::AddNode(int x, int y, int z, int gridX, int gridZ, int normalY)
{
	if (nodes.size() >= kHudsonNavMostNodes)
	{
		return -1;
	}

	Node node;

	node.x = x;
	node.y = y;
	node.z = z;
	node.gridX = gridX;
	node.gridZ = gridZ;
	node.doors = 0;
	node.lookedAtIn = 0;

	for (int i = 0; i < 8; i++)
	{
		node.next[i] = -1;
	}

	if (normalY <= -kHudsonNavFlatFloor)
	{
		flatSpots++;
	}
	else
	{
		slopeSpots++;
	}

	int const number = (int)nodes.size();

	nodes.push_back(node);
	columns[ColumnKey(gridX, gridZ)].push_back(number);

	cost.push_back(0.0);
	cameFrom.push_back(-1);
	stamp.push_back(0);

	return number;
}

// One of the eight ways round from this spot: see the top of hudson_nav_graph.h. This never moves the node it's given by reference, because adding a node can move them all.
void HudsonNavGraph::TryStep(HudsonNavRays &rays, int number, int way)
{
	// Until it's shown again that he can
	nodes[number].next[way] = -1;
	nodes[number].doors &= (uint8_t)~(1u << way);

	int const fromX = nodes[number].x;
	int const fromY = nodes[number].y;
	int const fromZ = nodes[number].z;
	int const gridX = nodes[number].gridX + kWayX[way];
	int const gridZ = nodes[number].gridZ + kWayZ[way];
	int const toSpotX = MiddleOfGrid(gridX);
	int const toSpotZ = MiddleOfGrid(gridZ);
	double const acrossX = (double)toSpotX - fromX;
	double const acrossZ = (double)toSpotZ - fromZ;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

	if (length <= 0.0)
	{
		return;
	}

	int const directionX = (int)floor(acrossX * kHudsonNavOneWhole / length + 0.5);
	int const directionZ = (int)floor(acrossZ * kHudsonNavOneWhole / length + 0.5);
	int const reach = (int)length + kHudsonNavMarineRadius;
	bool throughADoor = false;

	int const heights[2] = { kHudsonNavKneeHeight, kHudsonNavHeadHeight };

	for (int i = 0; i < 2; i++)
	{
		HudsonNavRayAnswer const across = Ray(rays, fromX, fromY - heights[i], fromZ, directionX, 0, directionZ, reach);

		if (across.hit && across.distance < reach)
		{
			if (across.door)
			{
				throughADoor = true;
			}
			else if (across.normalY > -kHudsonNavWalkable)
			{
				return;    // a wall in the way. A surface he can walk on only means the floor's rising
			}
		}
	}

	// Down from head height at the next spot. A floor can be no higher than that, and the ray that found it has to have started above it.
	int const startHeight = fromY - kHudsonNavHeadHeight;
	HudsonNavRayAnswer const down = Ray(rays, toSpotX, startHeight, toSpotZ, 0, kHudsonNavOneWhole, 0, kHudsonNavHeadHeight + kHudsonNavMostDrop);

	if (!down.hit || down.distance > kHudsonNavHeadHeight + kHudsonNavMostDrop)
	{
		return;    // nothing to stand on within reach
	}

	// In a closed door, what's underneath can be the door itself. He'd be standing on the same floor he's on.
	int const floorHeight = down.door ? fromY : startHeight + down.distance;

	if (!down.door && down.normalY > -kHudsonNavWalkable)
	{
		return;    // too steep to stand on
	}

	int const rise = fromY - floorHeight;

	if (rise > kHudsonNavStepUp)
	{
		// A slope can only go so far in one stride
		if ((double)rise > length * 2.0)
		{
			return;
		}

		// Higher than one stair, on the flat: it can still be two stairs, with the second halfway there. So a ray there has to find a floor that's no more than a step above where he is, and no more than a step below the
		// next spot. A wall that's only a little too high has nothing like that.
		if (down.normalY <= -kHudsonNavFlatFloor)
		{
			int const halfX = (fromX + toSpotX) / 2;
			int const halfZ = (fromZ + toSpotZ) / 2;
			HudsonNavRayAnswer const halfway = Ray(rays, halfX, startHeight, halfZ, 0, kHudsonNavOneWhole, 0, kHudsonNavHeadHeight + kHudsonNavMostDrop);

			if (!halfway.hit || halfway.normalY > -kHudsonNavWalkable)
			{
				return;
			}

			int const halfHeight = startHeight + halfway.distance;

			if (abs(fromY - halfHeight) > kHudsonNavStepUp || abs(halfHeight - floorHeight) > kHudsonNavStepUp)
			{
				return;
			}
		}
	}

	if (!down.door)
	{
		HudsonNavRayAnswer const up = Ray(rays, toSpotX, floorHeight - kHudsonNavKneeHeight, toSpotZ, 0, -kHudsonNavOneWhole, 0, kHudsonNavMarineHeight - kHudsonNavKneeHeight);

		// A door above him is one that's opening
		if (up.hit && !up.door && up.distance < kHudsonNavMarineHeight - kHudsonNavKneeHeight)
		{
			return;    // no room for him
		}
	}

	int spot = NodeAt(gridX, gridZ, floorHeight);

	if (spot < 0)
	{
		spot = AddNode(toSpotX, floorHeight, toSpotZ, gridX, gridZ, down.door ? -kHudsonNavOneWhole : down.normalY);

		if (spot < 0)
		{
			return;    // the map is full
		}
	}

	if (throughADoor)
	{
		nodes[number].doors |= (uint8_t)(1u << way);
	}

	nodes[number].next[way] = spot;
}

void HudsonNavGraph::LookAt(HudsonNavRays &rays, int number)
{
	for (int way = 0; way < 8; way++)
	{
		TryStep(rays, number, way);
	}

	nodes[number].lookedAtIn = searchNumber;
}

// The straight-line distance to where he's going. The real way can only be longer, which is what makes the first way found the shortest.
double HudsonNavGraph::GuessFrom(int number) const
{
	double const x = (double)nodes[number].x - toX;
	double const y = (double)nodes[number].y - toY;
	double const z = (double)nodes[number].z - toZ;

	return sqrt(x * x + y * y + z * z);
}

void HudsonNavGraph::StartSearch(int fromX, int fromY, int fromZ, int goalX, int goalY, int goalZ)
{
	waiting = std::priority_queue<Waiting>();
	way.clear();
	alongTheWay = 0;
	searchNumber++;
	order = 0;
	looks = 0;
	nearest = -1;
	nearestGuess = 0.0;
	toX = goalX;
	toY = goalY;
	toZ = goalZ;
	searching = true;

	int const gridX = GridOf(fromX);
	int const gridZ = GridOf(fromZ);
	int start = NodeAt(gridX, gridZ, fromY);

	// A spot where he stands is one he's been shown he can stand on, wherever on its grid place it is, and whichever way it faces
	if (start < 0)
	{
		start = AddNode(fromX, fromY, fromZ, gridX, gridZ, -kHudsonNavOneWhole);
	}

	if (start < 0)
	{
		return;    // the map is full, and Work will say there's no way
	}

	cost[start] = 0.0;
	cameFrom[start] = -1;
	stamp[start] = searchNumber;

	Waiting const first = { GuessFrom(start), order++, start };

	waiting.push(first);
}

void HudsonNavGraph::MakeTheWay(int lastNode)
{
	way.clear();

	// The chain goes back from the end, and it can't be longer than there are spots, even if something had gone wrong with it
	int number = lastNode;

	for (size_t i = 0; number >= 0 && i <= nodes.size(); i++)
	{
		HudsonNavPoint const point = { nodes[number].x, nodes[number].y, nodes[number].z };

		way.push_back(point);
		number = cameFrom[number];
	}

	std::reverse(way.begin(), way.end());
	alongTheWay = 0;
}

HudsonNavSearch HudsonNavGraph::Work(HudsonNavRays &rays, int raysAllowed)
{
	if (!searching)
	{
		return way.empty() ? kHudsonNavNoWay : kHudsonNavFound;
	}

	raysThisWork = 0;

	while (!waiting.empty() && looks < kHudsonNavMostLooks)
	{
		// Not even started on another spot, if that would use more than it's got left. Always at least one, or it would never get anywhere.
		if (raysThisWork > 0 && raysThisWork + kHudsonNavMostRaysForASpot > raysAllowed)
		{
			return kHudsonNavSearching;
		}

		Waiting const best = waiting.top();
		waiting.pop();

		int const number = best.node;
		double const guess = GuessFrom(number);

		// A spot can be waiting more than once, if a shorter way to it turned up later. Only the best counts.
		if (best.total > cost[number] + guess + 0.001)
		{
			continue;
		}

		if (nearest < 0 || guess < nearestGuess)
		{
			nearest = number;
			nearestGuess = guess;
		}

		if (abs(nodes[number].x - toX) <= kHudsonNavReachOfGoal && abs(nodes[number].z - toZ) <= kHudsonNavReachOfGoal && abs(nodes[number].y - toY) <= kHudsonNavGoalHeight)
		{
			MakeTheWay(number);

			HudsonNavPoint const goal = { toX, toY, toZ };

			way.push_back(goal);
			searching = false;
			return kHudsonNavFound;
		}

		looks++;

		if (nodes[number].lookedAtIn != searchNumber)
		{
			LookAt(rays, number);
		}

		for (int i = 0; i < 8; i++)
		{
			int const next = nodes[number].next[i];

			if (next < 0)
			{
				continue;
			}

			double const acrossX = (double)nodes[next].x - nodes[number].x;
			double const acrossY = (double)nodes[next].y - nodes[number].y;
			double const acrossZ = (double)nodes[next].z - nodes[number].z;
			double const dropped = (acrossY > kHudsonNavStepUp) ? acrossY * kHudsonNavDropCost : 0.0;    // y goes down, so going down is a bigger number
			double const total = cost[number] + sqrt(acrossX * acrossX + acrossY * acrossY + acrossZ * acrossZ) + dropped + ((nodes[number].doors & (1u << i)) ? kHudsonNavDoorCost : 0);

			if (stamp[next] != searchNumber || total < cost[next])
			{
				stamp[next] = searchNumber;
				cost[next] = total;
				cameFrom[next] = number;

				Waiting const later = { total + GuessFrom(next), order++, next };

				waiting.push(later);
			}
		}
	}

	// Nothing more to look at, or it's looked at as many as it may: as near as it got is all there is
	if (nearest >= 0)
	{
		MakeTheWay(nearest);
	}

	searching = false;
	return kHudsonNavNoWay;
}

bool HudsonNavGraph::LineIsClear(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int goalX, int goalZ, bool doorsCount)
{
	double const acrossX = (double)goalX - fromX;
	double const acrossZ = (double)goalZ - fromZ;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

	if (length <= 0.0)
	{
		return true;
	}

	int const directionX = (int)floor(acrossX * kHudsonNavOneWhole / length + 0.5);
	int const directionZ = (int)floor(acrossZ * kHudsonNavOneWhole / length + 0.5);
	int const reach = (int)length;
	int const heights[2] = { kHudsonNavKneeHeight, kHudsonNavHeadHeight };

	for (int i = 0; i < 2; i++)
	{
		HudsonNavRayAnswer const across = Ray(rays, fromX, fromY - heights[i], fromZ, directionX, 0, directionZ, reach);

		if (across.hit && across.distance < reach && across.normalY > -kHudsonNavWalkable && (doorsCount || !across.door))
		{
			return false;
		}
	}

	return true;
}

bool HudsonNavGraph::WayIsWideEnough(HudsonNavRays &rays, int fromX, int fromY, int fromZ, int goalX, int goalZ)
{
	if (!LineIsClear(rays, fromX, fromY, fromZ, goalX, goalZ, false))
	{
		return false;
	}

	double const acrossX = (double)goalX - fromX;
	double const acrossZ = (double)goalZ - fromZ;
	double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

	if (length <= 0.0)
	{
		return true;
	}

	// Square to the way he's going, as far as his side is from his middle
	int const sideX = (int)(-acrossZ * kHudsonNavMarineRadius / length);
	int const sideZ = (int)(acrossX * kHudsonNavMarineRadius / length);
	int const directionX = (int)floor(acrossX * kHudsonNavOneWhole / length + 0.5);
	int const directionZ = (int)floor(acrossZ * kHudsonNavOneWhole / length + 0.5);

	for (int side = -1; side <= 1; side += 2)
	{
		HudsonNavRayAnswer const across = Ray(rays, fromX + side * sideX, fromY - kHudsonNavKneeHeight, fromZ + side * sideZ, directionX, 0, directionZ, (int)length);

		if (across.hit && across.distance < (int)length && !across.door && across.normalY > -kHudsonNavWalkable)
		{
			return false;
		}
	}

	return true;
}

int HudsonNavGraph::NextAlong() const
{
	if (way.empty())
	{
		return -1;
	}

	return (int)(std::min)(way.size() - 1, alongTheWay + 1);
}

int HudsonNavGraph::FurthestInSight(HudsonNavRays &rays, int x, int y, int z, int mostAhead)
{
	if (way.empty())
	{
		return -1;
	}

	// How far along he's got: the nearest point from here on, not looking far ahead, so that a way that doubles back doesn't jump.
	double nearestAhead = -1.0;
	size_t const lookTo = (std::min)(way.size(), alongTheWay + 8);
	size_t closest = alongTheWay;

	for (size_t i = alongTheWay; i < lookTo; i++)
	{
		double const acrossX = (double)way[i].x - x;
		double const acrossZ = (double)way[i].z - z;
		double const here = acrossX * acrossX + acrossZ * acrossZ;

		if (nearestAhead < 0.0 || here < nearestAhead)
		{
			nearestAhead = here;
			closest = i;
		}
	}

	alongTheWay = closest;

	size_t const last = (std::min)(way.size() - 1, alongTheWay + (size_t)(mostAhead > 0 ? mostAhead : 0));

	// Furthest first: the first one he can see is the one. Points on another floor don't count as seen, or he'd try to walk through it.
	for (size_t i = last + 1; i-- > alongTheWay;)
	{
		if (abs(way[i].y - y) <= kHudsonNavStepUp && WayIsWideEnough(rays, x, y, z, way[i].x, way[i].z))
		{
			return (int)i;
		}
	}

	return -1;
}
