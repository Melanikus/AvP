// A placed decal's texture offset: how a level file keeps it, and how the game uses it. Written 2026-11-07 by Melanikus for
// the AvP port. The fix follows Bobblen's avp-forever (282ebb1, 8a0e408), rewritten.
//
// WHAT AN OFFSET IS
//
// A decal is a mark drawn on a wall: blood, a scorch, a bullet hole. Several kinds keep more than one picture side by side
// in the special effects picture, and the offset says which one to draw. It's a distance to the right, in pixels, that
// the game adds to the kind's box (kshape.cpp). Human blood, for example, has five splashes 32 pixels apart, so its
// offset is 0, 32, 64, 96 or 128. The table in decal.cpp gives each kind's step (UOffsetForSubclass) and its last picture
// (MaxSubclassNumber).
//
// HOW A LEVEL FILE KEEPS IT
//
// A level file keeps the offset as a 16.16 fixed-point number: the pixels times 65,536. Every one of the 16,378 placed
// decals in the 86 stock levels is kept that way, and every one is a picture of its own kind. The game used to copy the
// file's number as it was, so 32 pixels became 2,097,152, and the 5,428 stock decals with an offset drew the edge of
// the picture instead of their own mark.
//
// WHY IT'S CHECKED
//
// The number comes from a file, so it's outside input and may be anything. An offset is only used to place a picture,
// so a wrong one can't reach memory. It can only draw the wrong thing, so it's checked like the level's other values:
// a decal whose offset isn't one of its kind's pictures is left out.
//
// These functions hold nothing but plain numbers, so tests/decal_offset_test.cpp tries them with good and bad values.
// That test also reads projload.cpp, to check the loader and the writer still ask them.

#ifndef DECAL_OFFSET_H
#define DECAL_OFFSET_H

// One pixel, as a level file keeps it: 16.16 fixed point
const int kDecalOffsetOnePixel = 65536;

// Is this offset, in pixels, one of the decal kind's pictures? step is the kind's distance between pictures and
// lastPicture the number of its last one, counted from 0; a kind with a step of 0 has one picture, at offset 0.
inline bool DecalOffset_IsAPicture(int pixels, int step, int lastPicture)
{
	if (pixels < 0 || step < 0 || lastPicture < 0)
	{
		return false;
	}

	if (step == 0)
	{
		return pixels == 0;
	}

	// pixels / step can't run past lastPicture's offset this way, however large lastPicture is
	return pixels % step == 0 && pixels / step <= lastPicture;
}

// Turns a level file's offset into pixels, for the kind with this step and last picture. Returns false, and leaves
// pixels alone, when the file's number isn't a whole number of pixels or isn't one of the kind's pictures.
inline bool DecalOffset_FromLevelFile(int fileNumber, int step, int lastPicture, int *pixels)
{
	if (fileNumber % kDecalOffsetOnePixel != 0)
	{
		return false;
	}

	int const offset = fileNumber / kDecalOffsetOnePixel;

	if (!DecalOffset_IsAPicture(offset, step, lastPicture))
	{
		return false;
	}

	*pixels = offset;
	return true;
}

// Turns an offset in pixels into the number a level file keeps. An offset that isn't one of the kind's pictures is
// written as 0, its first picture, so a file the game writes always loads again. So is one too large for the file's
// number to hold (over 32,767 pixels), which no picture is.
inline int DecalOffset_ToLevelFile(int pixels, int step, int lastPicture)
{
	if (!DecalOffset_IsAPicture(pixels, step, lastPicture) || pixels > 0x7fffffff / kDecalOffsetOnePixel)
	{
		return 0;
	}

	return pixels * kDecalOffsetOnePixel;
}

#endif
