// One frame's use of the level's static buffer. Written 2026-11-04 by Melanikus for the AvP port.
// See static_frame.cpp for how it works. This file knows nothing about the game, so a test can check it on its own.

#ifndef STATIC_FRAME_H
#define STATIC_FRAME_H

#include <stdint.h>

#include "static_level.h"

// Puts ranges in drawing order and joins those that sit next to each other, so that they're drawn in one go. Gives back how many are left.
int StaticFrame_SortAndJoin(StaticRange *ranges, int count);

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
	int pixelsCompared;
	int holesInNew;            // a surface in the old route's pictures, and none in the new. On screen, this is a wrong pixel
	int extraInNew;            // the other way round
	int colourChanged;         // both have a surface, and its colour differs by more than rounding
	int normalChanged;         // the same, for which way it faces
	int distanceChanged;       // the same, for how far away it is
	float largestDistanceChange;
};

// Compares every pixel. normalIsTenBit says the normal picture holds ten bits for each direction, and not eight.
void StaticFrame_Compare(const SurfacePicturesInMemory &oldRoute, const SurfacePicturesInMemory &newRoute, uint32_t width, uint32_t height, bool normalIsTenBit, PictureDifferences *differences);

#endif
