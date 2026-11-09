// A small made-up world of boxes, for testing what Hudson does with the rays he sends out. Written 2026-11-05 by Melanikus for the AvP port.
//
// In this world, as in the game, y goes down. A floor at height 0 is a box from y = 0 down to y = 100.

#ifndef HUDSON_NAV_TEST_WORLD_H
#define HUDSON_NAV_TEST_WORLD_H

#include "hudson_nav_graph.h"
#include "hudson_nav_plan.h"

#include <vector>

struct Box
{
	int minX, minY, minZ;
	int maxX, maxY, maxZ;
	bool door;
	int topFaces;     // which way its top faces, if it isn't straight up: how a slope is pretended here
};

class World : public HudsonNavRays
{
public:
	World() : asked(0) {}

	void Add(int minX, int minY, int minZ, int maxX, int maxY, int maxZ, bool door = false, int topFaces = -65536)
	{
		Box const box = { minX, minY, minZ, maxX, maxY, maxZ, door, topFaces };

		boxes.push_back(box);
	}

	void Floor(int minX, int minZ, int maxX, int maxZ, int height)
	{
		Add(minX, height, minZ, maxX, height + 100, maxZ);
	}

	void Wall(int minX, int minZ, int maxX, int maxZ)
	{
		Add(minX, -10000, minZ, maxX, 10000, maxZ);
	}

	HudsonNavRayAnswer Cast(int x, int y, int z, int directionX, int directionY, int directionZ, int longest)
	{
		asked++;

		double const dx = directionX / 65536.0, dy = directionY / 65536.0, dz = directionZ / 65536.0;
		HudsonNavRayAnswer answer = { false, false, longest, 0 };
		double nearest = longest;

		for (size_t i = 0; i < boxes.size(); i++)
		{
			Box const &box = boxes[i];
			double enter = 0.0, leave = nearest;
			int faceAxis = -1;
			bool fromTheLow = false;

			double const from[3] = { (double)x, (double)y, (double)z };
			double const way[3] = { dx, dy, dz };
			double const low[3] = { (double)box.minX, (double)box.minY, (double)box.minZ };
			double const high[3] = { (double)box.maxX, (double)box.maxY, (double)box.maxZ };
			bool missed = false;

			for (int axis = 0; axis < 3 && !missed; axis++)
			{
				if (way[axis] == 0.0)
				{
					missed = from[axis] < low[axis] || from[axis] > high[axis];
					continue;
				}

				double a = (low[axis] - from[axis]) / way[axis];
				double b = (high[axis] - from[axis]) / way[axis];
				bool lowFirst = true;

				if (a > b)
				{
					double const swap = a;
					a = b;
					b = swap;
					lowFirst = false;
				}

				if (a > enter)
				{
					enter = a;
					faceAxis = axis;
					fromTheLow = lowFirst;
				}

				if (b < leave)
				{
					leave = b;
				}

				missed = enter > leave;
			}

			// A ray that starts inside a box goes straight out of it, as a ray from inside a polygon's room does
			if (missed || faceAxis < 0)
			{
				continue;
			}

			if (enter <= nearest)
			{
				nearest = enter;
				answer.hit = true;
				answer.distance = (int)enter;
				answer.door = box.door;

				// Boxes can't slope, so one pretending to be a slope says so from whichever side a ray comes, as a real sloping polygon would
				if (box.topFaces != -65536)
				{
					answer.normalY = box.topFaces;
				}
				else
				{
					answer.normalY = (faceAxis == 1) ? (fromTheLow ? box.topFaces : 65536) : 0;
				}
			}
		}

		return answer;
	}

	void RemoveLast()
	{
		boxes.pop_back();
	}

	int asked;

private:
	std::vector<Box> boxes;
};

// A world that says odd things when a ray hits nothing. Nothing should go by them.
class WorldThatMisleads : public HudsonNavRays
{
public:
	explicit WorldThatMisleads(World &real) : world(real) {}

	HudsonNavRayAnswer Cast(int x, int y, int z, int directionX, int directionY, int directionZ, int longest)
	{
		HudsonNavRayAnswer answer = world.Cast(x, y, z, directionX, directionY, directionZ, longest);

		if (!answer.hit)
		{
			answer.distance = kHudsonNavHeadHeight;
			answer.normalY = -65536;
		}

		return answer;
	}

private:
	World &world;
};

#endif
