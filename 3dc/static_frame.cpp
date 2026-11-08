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
//
// TIES
//
// The surface pictures are painted over the game's finished depth buffer, and they don't write to it. A polygon gets to paint a pixel if it's as near as the
// depth the game kept, or within a small margin of it. So when two polygons can both paint a pixel, say a sign flat on a wall, it isn't the nearer that stays
// but the one painted last. Those two polygons tie. The two routes paint in a different order, so on a tie they can keep different polygons, and neither
// has left anything out.
//
// To find those pixels, the old route's pictures are painted once more with every polygon in the opposite order. A pixel only one polygon can paint comes out the
// same whatever the order, so a pixel that changes has a tie on it. A place where the routes differ and there's no tie isn't explained by the order of painting.
// It's either the new route drawing something else, or a tie that only exists on the new route, because the card worked out a corner slightly differently.

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

static bool FollowsOn(const StaticRange &first, const StaticRange &next, uint32_t mostTriangles)
{
	return first.texture == next.texture && first.twoSided == next.twoSided && first.baseVertex == next.baseVertex
		&& first.firstIndex + first.triangles * 3 == next.firstIndex && next.triangles <= mostTriangles - first.triangles;
}

static bool IsInside(const StaticRange &part, const StaticRange &whole)
{
	return part.texture == whole.texture && part.twoSided == whole.twoSided && part.baseVertex == whole.baseVertex
		&& part.firstIndex >= whole.firstIndex && part.firstIndex + part.triangles * 3 <= whole.firstIndex + whole.triangles * 3;
}

int StaticFrame_SortAndJoin(StaticRange *ranges, int count, uint32_t mostTriangles)
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

		if (FollowsOn(last, next, mostTriangles))
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

static int LargestColourStep(const uint8_t *a, const uint8_t *b)
{
	return (std::max)(Difference(a[0], b[0]), (std::max)(Difference(a[1], b[1]), Difference(a[2], b[2])));
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

			if (oldHasSurface)
			{
				differences->pixelsWithSurface++;
			}

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

			int const colourStep = LargestColourStep(oldColour, newColour);

			if (colourStep > differences->largestColourStep)
			{
				differences->largestColourStep = colourStep;
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

static uint8_t const *ColourAt(const SurfacePicturesInMemory &pictures, uint32_t x, uint32_t y)
{
	return pictures.colour + y * pictures.colourPitch + x * 4;
}

static float DistanceAt(const SurfacePicturesInMemory &pictures, uint32_t x, uint32_t y)
{
	float distance;

	memcpy(&distance, pictures.distance + y * pictures.distancePitch + x * 4, sizeof(distance));

	return distance;
}

// Do two sets of pictures differ on this pixel, in any way the compare counts?
static bool PicturesDifferAt(const SurfacePicturesInMemory &a, const SurfacePicturesInMemory &b, uint32_t x, uint32_t y, bool normalIsTenBit)
{
	bool const surfaceInA = ColourAt(a, x, y)[3] == 255;
	bool const surfaceInB = ColourAt(b, x, y)[3] == 255;

	if (surfaceInA != surfaceInB)
	{
		return true;
	}

	if (!surfaceInA)
	{
		return false;
	}

	return ColoursDiffer(ColourAt(a, x, y), ColourAt(b, x, y))
		|| NormalsDiffer(a.normal + y * a.normalPitch + x * 4, b.normal + y * b.normalPitch + x * 4, normalIsTenBit)
		|| fabsf(DistanceAt(a, x, y) - DistanceAt(b, x, y)) > kDistanceRounding;
}

// Red, green and blue from the picture's blue, green, red and alpha.
static uint32_t WebColour(const uint8_t *colour)
{
	return ((uint32_t)colour[2] << 16) | ((uint32_t)colour[1] << 8) | (uint32_t)colour[0];
}

// Adds a pixel that differs to what's known about where they differ. Pixels arrive reading each row from the top, so the first to arrive is the first one.
static void NotePixelThatDiffers(const SurfacePicturesInMemory &a, const SurfacePicturesInMemory &b, uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool normalIsTenBit, WhereTheyDiffer *where)
{
	if (!where->pixels)
	{
		where->left = where->right = (int)x;
		where->top = where->bottom = (int)y;
		where->firstX = (int)x;
		where->firstY = (int)y;
		where->firstColourBefore = WebColour(ColourAt(a, x, y));
		where->firstColourAfter = WebColour(ColourAt(b, x, y));
		where->firstDistanceBefore = DistanceAt(a, x, y);
		where->firstDistanceAfter = DistanceAt(b, x, y);
	}

	where->pixels++;
	where->left = (std::min)(where->left, (int)x);
	where->right = (std::max)(where->right, (int)x);
	where->bottom = (std::max)(where->bottom, (int)y);

	// a pixel on the picture's edge has nothing beyond it, so it can't be surrounded
	if (x > 0 && y > 0 && x + 1 < width && y + 1 < height
		&& PicturesDifferAt(a, b, x - 1, y, normalIsTenBit) && PicturesDifferAt(a, b, x + 1, y, normalIsTenBit)
		&& PicturesDifferAt(a, b, x, y - 1, normalIsTenBit) && PicturesDifferAt(a, b, x, y + 1, normalIsTenBit))
	{
		where->solidPixels++;
	}
}

void StaticFrame_WhereTheyDiffer(const SurfacePicturesInMemory &before, const SurfacePicturesInMemory &after, uint32_t width, uint32_t height, bool normalIsTenBit, WhereTheyDiffer *where)
{
	memset(where, 0, sizeof(*where));

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			if (PicturesDifferAt(before, after, x, y, normalIsTenBit))
			{
				NotePixelThatDiffers(before, after, x, y, width, height, normalIsTenBit, where);
			}
		}
	}
}

void StaticFrame_SplitByTies(const SurfacePicturesInMemory &oldRoute, const SurfacePicturesInMemory &newRoute, const SurfacePicturesInMemory &oldRouteBackwards, uint32_t width, uint32_t height, bool normalIsTenBit, TiesBetweenPolygons *ties)
{
	memset(ties, 0, sizeof(*ties));

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			bool const aTie = PicturesDifferAt(oldRoute, oldRouteBackwards, x, y, normalIsTenBit);

			if (aTie)
			{
				ties->pixelsThatTie++;
			}

			if (PicturesDifferAt(oldRoute, newRoute, x, y, normalIsTenBit))
			{
				NotePixelThatDiffers(oldRoute, newRoute, x, y, width, height, normalIsTenBit, aTie ? &ties->onATie : &ties->notOnATie);
			}
		}
	}
}
