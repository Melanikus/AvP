// The ribbon: what a Hudson on foot run draws in the level, so that someone watching can see the way he means to walk and where he has really been.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// They're flat bands lying on the floor, and blocks standing at each step and each cheat. They're real shapes in the world, so walls hide them and they go up stairs. Nothing here
// changes what he does: it only makes shapes out of what's already known.
//
// This file only makes the shapes, with plain sums and no graphics card, so that a test can check them. Drawing them is DrawHudsonPlayRibbon in d3d_render.cpp, and what goes in
// them comes from hudson_play.cpp. In this game a bigger y is lower down, so going up means taking away from y.

#ifndef HUDSON_PLAY_RIBBON_H
#define HUDSON_PLAY_RIBBON_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

// One corner of a triangle: where it is in the level, in millimetres, and its colour
struct HudsonPlayRibbonCorner
{
	int x, y, z;
	uint8_t red, green, blue;
};

struct HudsonPlayRibbonColour
{
	uint8_t red, green, blue;
};

// The most corners a frame draws, however long he's been walking, so that showing the way can't eat the frame. Three corners make a triangle.
static const size_t kHudsonPlayRibbonMostCorners = 60000;

// How wide a band is, from its middle to its edge, in millimetres: wide enough to see from across a room
static const int kHudsonPlayRibbonHalfWidth = 350;

// Two points further apart than this have nothing walked between them, because he was moved or fell, so no band joins them
static const int kHudsonPlayRibbonLongestPiece = 6000;

// How much of a colour every other piece of a band keeps. The stripes are what show a band going up a stair, and which way it runs.
static const float kHudsonPlayRibbonStripe = 0.72f;

// How much of a colour each side of a block keeps, so that its corners can be seen
static const float kHudsonPlayRibbonSideOneWay = 0.72f;
static const float kHudsonPlayRibbonSideOtherWay = 0.52f;

// Adds a flat face with four corners going round it, as two triangles. False if there's no more room.
inline bool HudsonPlayRibbon_AddFace(std::vector<HudsonPlayRibbonCorner> &corners, HudsonPlayRibbonCorner const &a, HudsonPlayRibbonCorner const &b, HudsonPlayRibbonCorner const &c, HudsonPlayRibbonCorner const &d)
{
	if (corners.size() + 6 > kHudsonPlayRibbonMostCorners)
	{
		return false;
	}

	corners.push_back(a);
	corners.push_back(b);
	corners.push_back(c);
	corners.push_back(a);
	corners.push_back(c);
	corners.push_back(d);
	return true;
}

inline HudsonPlayRibbonCorner HudsonPlayRibbon_Corner(int x, int y, int z, HudsonPlayRibbonColour colour, float shade)
{
	HudsonPlayRibbonCorner corner;

	corner.x = x;
	corner.y = y;
	corner.z = z;
	corner.red = (uint8_t)(colour.red * shade);
	corner.green = (uint8_t)(colour.green * shade);
	corner.blue = (uint8_t)(colour.blue * shade);
	return corner;
}

// Adds a band along points, from the one numbered first to the last, lying this far above their floor. The far end goes in first: for a trail that's where he is now, which is what
// matters if there's no room for all of it. Point is anything with whole numbers x, y and z.
template <typename Point>
inline void HudsonPlayRibbon_AddBand(std::vector<Point> const &points, size_t first, int offTheFloor, HudsonPlayRibbonColour colour, std::vector<HudsonPlayRibbonCorner> &corners)
{
	for (size_t i = points.size(); i > first + 1; i--)
	{
		Point const &from = points[i - 2];
		Point const &to = points[i - 1];
		double const acrossX = (double)to.x - from.x, acrossZ = (double)to.z - from.z;
		double const length = sqrt(acrossX * acrossX + acrossZ * acrossZ);

		// One straight above the other has no direction for a band to run in
		if (length < 1.0 || length > (double)kHudsonPlayRibbonLongestPiece)
		{
			continue;
		}

		// A piece runs a little past each of its points, which fills the gap on the outside of a turn
		int const pastX = (int)(acrossX / length * kHudsonPlayRibbonHalfWidth), pastZ = (int)(acrossZ / length * kHudsonPlayRibbonHalfWidth);

		// Straight out to the side of the way it's going
		int const sideX = -pastZ, sideZ = pastX;
		int const fromY = from.y - offTheFloor, toY = to.y - offTheFloor;
		float const shade = i % 2 == 0 ? 1.0f : kHudsonPlayRibbonStripe;

		if (!HudsonPlayRibbon_AddFace(corners,
			HudsonPlayRibbon_Corner(from.x - pastX + sideX, fromY, from.z - pastZ + sideZ, colour, shade),
			HudsonPlayRibbon_Corner(to.x + pastX + sideX, toY, to.z + pastZ + sideZ, colour, shade),
			HudsonPlayRibbon_Corner(to.x + pastX - sideX, toY, to.z + pastZ - sideZ, colour, shade),
			HudsonPlayRibbon_Corner(from.x - pastX - sideX, fromY, from.z - pastZ - sideZ, colour, shade)))
		{
			return;
		}
	}
}

// Adds a block standing with its feet at this place, with a top and four sides. There's no bottom, because it's looked at from above.
inline void HudsonPlayRibbon_AddBlock(int x, int y, int z, int halfWidth, int height, HudsonPlayRibbonColour colour, std::vector<HudsonPlayRibbonCorner> &corners)
{
	int const top = y - height;
	int const left = x - halfWidth, right = x + halfWidth, nearSide = z - halfWidth, farSide = z + halfWidth;

#define HUDSON_RIBBON_CORNER(cx, cy, cz, shade) HudsonPlayRibbon_Corner(cx, cy, cz, colour, shade)
	HudsonPlayRibbon_AddFace(corners, HUDSON_RIBBON_CORNER(left, top, nearSide, 1.0f), HUDSON_RIBBON_CORNER(right, top, nearSide, 1.0f), HUDSON_RIBBON_CORNER(right, top, farSide, 1.0f), HUDSON_RIBBON_CORNER(left, top, farSide, 1.0f));
	HudsonPlayRibbon_AddFace(corners, HUDSON_RIBBON_CORNER(left, y, nearSide, kHudsonPlayRibbonSideOneWay), HUDSON_RIBBON_CORNER(right, y, nearSide, kHudsonPlayRibbonSideOneWay), HUDSON_RIBBON_CORNER(right, top, nearSide, kHudsonPlayRibbonSideOneWay), HUDSON_RIBBON_CORNER(left, top, nearSide, kHudsonPlayRibbonSideOneWay));
	HudsonPlayRibbon_AddFace(corners, HUDSON_RIBBON_CORNER(left, y, farSide, kHudsonPlayRibbonSideOneWay), HUDSON_RIBBON_CORNER(right, y, farSide, kHudsonPlayRibbonSideOneWay), HUDSON_RIBBON_CORNER(right, top, farSide, kHudsonPlayRibbonSideOneWay), HUDSON_RIBBON_CORNER(left, top, farSide, kHudsonPlayRibbonSideOneWay));
	HudsonPlayRibbon_AddFace(corners, HUDSON_RIBBON_CORNER(left, y, nearSide, kHudsonPlayRibbonSideOtherWay), HUDSON_RIBBON_CORNER(left, y, farSide, kHudsonPlayRibbonSideOtherWay), HUDSON_RIBBON_CORNER(left, top, farSide, kHudsonPlayRibbonSideOtherWay), HUDSON_RIBBON_CORNER(left, top, nearSide, kHudsonPlayRibbonSideOtherWay));
	HudsonPlayRibbon_AddFace(corners, HUDSON_RIBBON_CORNER(right, y, nearSide, kHudsonPlayRibbonSideOtherWay), HUDSON_RIBBON_CORNER(right, y, farSide, kHudsonPlayRibbonSideOtherWay), HUDSON_RIBBON_CORNER(right, top, farSide, kHudsonPlayRibbonSideOtherWay), HUDSON_RIBBON_CORNER(right, top, nearSide, kHudsonPlayRibbonSideOtherWay));
#undef HUDSON_RIBBON_CORNER
}

#endif
