// Where a test copy's window goes, with -dev -windowed. Written 2026-11-09 by Melanikus for the AvP port. window_slot.h says why.

#include "window_slot.h"

// Screens are far smaller than this, so no sum below can overflow
static const int kMostScreen = 100000;

static bool IsAScreenNumber(int n)
{
	return n >= -kMostScreen && n <= kMostScreen;
}

bool WindowSlot_Work(int areaLeft, int areaTop, int areaWidth, int areaHeight, int frameWidth, int frameHeight, bool secondCopy,
	const int *modeWidths, const int *modeHeights, int count, WindowSlot *slot)
{
	if (!slot || !modeWidths || !modeHeights || count <= 0 || areaWidth < 2 || areaHeight < 1 || !IsAScreenNumber(areaWidth) ||
		!IsAScreenNumber(areaHeight) || !IsAScreenNumber(areaLeft) || !IsAScreenNumber(areaTop) || frameWidth < 0 || frameHeight < 0)
	{
		return false;
	}

	// Each copy has half the width, and all the height
	int const half = areaWidth / 2;

	if (frameWidth >= half || frameHeight >= areaHeight)
	{
		return false;
	}

	// The most picture that fits inside the frame in that half
	int const roomWidth = half - frameWidth;
	int const roomHeight = areaHeight - frameHeight;

	// The largest mode that fits, by its area. Kept as a long long, so a pair of big numbers can't overflow when multiplied.
	int bestWidth = 0, bestHeight = 0;
	long long bestArea = -1;
	int smallestWidth = 0, smallestHeight = 0;
	long long smallestArea = -1;

	for (int i = 0; i < count; i++)
	{
		int const w = modeWidths[i];
		int const h = modeHeights[i];

		if (w <= 0 || h <= 0 || w > kMostScreen || h > kMostScreen)
		{
			continue;
		}

		long long const area = (long long)w * h;

		if (smallestArea < 0 || area < smallestArea)
		{
			smallestArea = area;
			smallestWidth = w;
			smallestHeight = h;
		}

		if (w <= roomWidth && h <= roomHeight && area > bestArea)
		{
			bestArea = area;
			bestWidth = w;
			bestHeight = h;
		}
	}

	if (smallestArea < 0)
	{
		return false;
	}

	slot->left = areaLeft + (secondCopy ? half : 0);
	slot->top = areaTop;
	slot->pictureWidth = bestArea >= 0 ? bestWidth : smallestWidth;
	slot->pictureHeight = bestArea >= 0 ? bestHeight : smallestHeight;
	return true;
}
