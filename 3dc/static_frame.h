// One frame's use of the level's static buffer. Written 2026-11-04 by Melanikus for the AvP port.
// See static_frame.cpp for how it works. This file knows nothing about the game, so a test can check it on its own.

#ifndef STATIC_FRAME_H
#define STATIC_FRAME_H

#include <stdint.h>

#include "static_level.h"

// Puts ranges in drawing order and joins those that sit next to each other, so that they're drawn in one go, as long as the join isn't more than
// mostTriangles, the card's limit for one draw. Gives back how many are left.
int StaticFrame_SortAndJoin(StaticRange *ranges, int count, uint32_t mostTriangles);

// One route's three surface pictures, read back into memory. Each is four bytes a pixel, and a pitch is how many bytes a row takes.
struct SurfacePicturesInMemory
{
	const uint8_t *distance;     // one float a pixel
	const uint8_t *normal;
	const uint8_t *colour;       // blue, green, red, alpha. An alpha of 255 means a surface is here
	uint32_t distancePitch;
	uint32_t normalPitch;
	uint32_t colourPitch;
};

// How two routes' pictures differ.
struct PictureDifferences
{
	int pixelsCompared;        // every pixel of the picture, whether anything was drawn there or not
	int pixelsWithSurface;     // how many of those the old route drew something on. With none, everything below is 0 and that proves nothing
	int holesInNew;            // a surface in the old route's pictures, and none in the new. On screen, this is a wrong pixel
	int extraInNew;            // the other way round
	int colourChanged;         // both have a surface, and its colour differs by more than rounding
	int normalChanged;         // the same, for which way it faces
	int distanceChanged;       // the same, for how far away it is
	float largestDistanceChange;
	int largestColourStep;     // how far the furthest of red, green or blue moved, out of 255, on any pixel with a surface in both. A step or two is rounding, and a lot is another surface
};

// Compares every pixel. normalIsTenBit says the normal picture holds ten bits for each direction, and not eight.
void StaticFrame_Compare(const SurfacePicturesInMemory &oldRoute, const SurfacePicturesInMemory &newRoute, uint32_t width, uint32_t height, bool normalIsTenBit, PictureDifferences *differences);

// Whereabouts two sets of pictures differ, so that someone can go and look.
struct WhereTheyDiffer
{
	int pixels;                      // how many differ, in any way the compare counts
	int solidPixels;                 // how many of those have a difference on all four sides. A thin line along a polygon's edge has none, and a patch of wall has plenty
	int left, top, right, bottom;    // the box round them all
	int firstX, firstY;              // the first one, reading each row from the top
	uint32_t firstColourBefore;      // what's at the first one in each picture: red, green and blue, the way a web page writes them
	uint32_t firstColourAfter;
	float firstDistanceBefore;
	float firstDistanceAfter;
};

void StaticFrame_WhereTheyDiffer(const SurfacePicturesInMemory &before, const SurfacePicturesInMemory &after, uint32_t width, uint32_t height, bool normalIsTenBit, WhereTheyDiffer *where);

// The same, with the differences split by whether two polygons tie on that pixel. See TIES in static_frame.cpp.
struct TiesBetweenPolygons
{
	int pixelsThatTie;           // how many pixels in the whole picture change when the old route draws its polygons in the opposite order
	WhereTheyDiffer onATie;      // where the routes differ on one of those
	WhereTheyDiffer notOnATie;   // where they differ anywhere else, so the order of drawing doesn't explain it
};

void StaticFrame_SplitByTies(const SurfacePicturesInMemory &oldRoute, const SurfacePicturesInMemory &newRoute, const SurfacePicturesInMemory &oldRouteBackwards, uint32_t width, uint32_t height, bool normalIsTenBit, TiesBetweenPolygons *ties);

#endif
