// One frame's use of the level's static buffer. Written 2026-11-04 by Melanikus for the AvP port.
//
// THE RANGES
//
// The level's polygons sit on the graphics card for good, in order. Each frame only has to say which parts of that to draw: the parts belonging to
// the modules that are visible. Every change of texture is a separate draw, and draws are what cost the processor, so the parts are sorted by
// texture, and any two that sit next to each other in the buffer become one.
//
// THE COMPARE
//
// The new route has to draw exactly what the old one did. So, when asked, a frame draws its surface pictures both ways, and every pixel is compared.
// The answer isn't simply "same" or "different", because the graphics card does its sums in a slightly different order from the processor, and a
// corner can land one unit away. What matters most is a hole: a pixel with a surface in the old pictures and none in the new, because that's a wrong
// pixel on the screen. So holes, and changes to colour, direction and distance, are all counted apart.

#include "static_frame.h"

#include <algorithm>
#include <math.h>
#include <string.h>

// How much of a difference is only rounding, and not a real change.
static const int kColourRounding = 2;
static const int kEightBitNormalRounding = 2;
static const int kTenBitNormalRounding = 8;
static const float kDistanceRounding = 0.5f;

// Two-sided polygons first, because they need different card settings, then by texture, then in the order they sit in the buffer.
static bool DrawnBefore(const StaticRange &a, const StaticRange &b)
{
	if (a.twoSided != b.twoSided)
	{
		return a.twoSided;
	}

	if (a.texture != b.texture)
	{
		return a.texture < b.texture;
	}

	if (a.baseVertex != b.baseVertex)
	{
		return a.baseVertex < b.baseVertex;
	}

	return a.firstIndex < b.firstIndex;
}

static bool FollowsOn(const StaticRange &first, const StaticRange &next)
{
	return first.texture == next.texture && first.twoSided == next.twoSided && first.baseVertex == next.baseVertex
		&& first.firstIndex + first.triangles * 3 == next.firstIndex;
}

static bool IsInside(const StaticRange &part, const StaticRange &whole)
{
	return part.texture == whole.texture && part.twoSided == whole.twoSided && part.baseVertex == whole.baseVertex
		&& part.firstIndex >= whole.firstIndex && part.firstIndex + part.triangles * 3 <= whole.firstIndex + whole.triangles * 3;
}

int StaticFrame_SortAndJoin(StaticRange *ranges, int count)
{
	if (count <= 0)
	{
		return 0;
	}

	std::sort(ranges, ranges + count, DrawnBefore);

	int kept = 1;

	for (int i = 1; i < count; i++)
	{
		StaticRange &last = ranges[kept - 1];
		StaticRange const &next = ranges[i];

		// the same part twice, if a module was noted twice
		if (IsInside(next, last))
		{
			continue;
		}

		if (FollowsOn(last, next))
		{
			uint32_t const end = std::max(last.firstVertex + last.vertexCount, next.firstVertex + next.vertexCount);

			last.firstVertex = std::min(last.firstVertex, next.firstVertex);
			last.vertexCount = end - last.firstVertex;
			last.triangles += next.triangles;
		}
		else
		{
			ranges[kept++] = next;
		}
	}

	return kept;
}

static int Difference(int a, int b)
{
	return (a > b) ? a - b : b - a;
}

static bool ColoursDiffer(const uint8_t *a, const uint8_t *b)
{
	return Difference(a[0], b[0]) > kColourRounding || Difference(a[1], b[1]) > kColourRounding || Difference(a[2], b[2]) > kColourRounding;
}

static bool NormalsDiffer(const uint8_t *a, const uint8_t *b, bool tenBit)
{
	if (!tenBit)
	{
		return Difference(a[0], b[0]) > kEightBitNormalRounding || Difference(a[1], b[1]) > kEightBitNormalRounding || Difference(a[2], b[2]) > kEightBitNormalRounding;
	}

	uint32_t first, second;
	memcpy(&first, a, sizeof(first));
	memcpy(&second, b, sizeof(second));

	for (int shift = 0; shift <= 20; shift += 10)
	{
		if (Difference((int)((first >> shift) & 1023), (int)((second >> shift) & 1023)) > kTenBitNormalRounding)
		{
			return true;
		}
	}

	return false;
}

void StaticFrame_Compare(const SurfacePicturesInMemory &oldRoute, const SurfacePicturesInMemory &newRoute, uint32_t width, uint32_t height, bool normalIsTenBit, PictureDifferences *differences)
{
	memset(differences, 0, sizeof(*differences));

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t const *oldColour = oldRoute.colour + y * oldRoute.colourPitch + x * 4;
			uint8_t const *newColour = newRoute.colour + y * newRoute.colourPitch + x * 4;
			bool const oldHasSurface = oldColour[3] == 255;
			bool const newHasSurface = newColour[3] == 255;

			differences->pixelsCompared++;

			if (oldHasSurface != newHasSurface)
			{
				if (oldHasSurface)
				{
					differences->holesInNew++;
				}
				else
				{
					differences->extraInNew++;
				}

				continue;
			}

			if (!oldHasSurface)
			{
				continue;
			}

			if (ColoursDiffer(oldColour, newColour))
			{
				differences->colourChanged++;
			}

			if (NormalsDiffer(oldRoute.normal + y * oldRoute.normalPitch + x * 4, newRoute.normal + y * newRoute.normalPitch + x * 4, normalIsTenBit))
			{
				differences->normalChanged++;
			}

			float oldDistance, newDistance;
			memcpy(&oldDistance, oldRoute.distance + y * oldRoute.distancePitch + x * 4, sizeof(oldDistance));
			memcpy(&newDistance, newRoute.distance + y * newRoute.distancePitch + x * 4, sizeof(newDistance));

			float const change = fabsf(oldDistance - newDistance);

			if (change > kDistanceRounding)
			{
				differences->distanceChanged++;
			}

			if (change > differences->largestDistanceChange)
			{
				differences->largestDistanceChange = change;
			}
		}
	}
}
