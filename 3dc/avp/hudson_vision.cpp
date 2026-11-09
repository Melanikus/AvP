// See hudson_vision.h. Written 2026-11-05 by Melanikus for the AvP port. This file knows nothing about the game or the graphics card.
#include "hudson_vision.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void AddToFinding(HudsonVisionFinding *finding, int x, int y)
{
	if (finding->pixels == 0)
	{
		finding->firstX = finding->left = finding->right = x;
		finding->firstY = finding->top = finding->bottom = y;
	}

	finding->pixels++;
	finding->left = x < finding->left ? x : finding->left;
	finding->right = x > finding->right ? x : finding->right;
	finding->bottom = y > finding->bottom ? y : finding->bottom;
}

// Four bytes a pixel either way. In eight bits it's blue, green and red, with the fourth byte unused. In ten bits the top two bits are unused.
static bool NormalIsNothing(const uint8_t *pixel, bool normalIsTenBit)
{
	if (normalIsTenBit)
	{
		uint32_t bits = 0;
		memcpy(&bits, pixel, sizeof(bits));
		return (bits & 0x3FFFFFFF) == 0;
	}

	return pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0;
}

// Which one of total to pick next, having picked some already, so that most of them are spread evenly over the lot. If there aren't more than most, it's all of them.
static int NextToPick(int picked, int total, int most)
{
	return total <= most ? picked : (int)((long long)picked * total / most);
}

void HudsonVision_Count(const SurfacePicturesInMemory &pictures, uint32_t width, uint32_t height, bool normalIsTenBit, HudsonVisionCounts *counts)
{
	memset(counts, 0, sizeof(*counts));

	bool anyDistance = false;

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			counts->pixelsLookedAt++;

			if (pictures.colour[y * pictures.colourPitch + x * 4 + 3] != 255)
			{
				AddToFinding(&counts->nothingDrawn, (int)x, (int)y);
				continue;
			}

			counts->surfaces++;

			if (NormalIsNothing(pictures.normal + y * pictures.normalPitch + x * 4, normalIsTenBit))
			{
				AddToFinding(&counts->noNormal, (int)x, (int)y);
			}

			float distance = 0.0f;
			memcpy(&distance, pictures.distance + y * pictures.distancePitch + x * 4, sizeof(distance));

			if (distance == kHudsonVisionClearedDistance)
			{
				AddToFinding(&counts->noDistance, (int)x, (int)y);
			}
			else
			{
				counts->nearest = (!anyDistance || distance < counts->nearest) ? distance : counts->nearest;
				counts->furthest = (!anyDistance || distance > counts->furthest) ? distance : counts->furthest;
				anyDistance = true;
			}
		}
	}

	// A second time over, now that it's known how many there are, for a few spread out evenly over them all
	int seen = 0;

	for (uint32_t y = 0; y < height && counts->samples < kHudsonVisionMostSamples; y++)
	{
		for (uint32_t x = 0; x < width && counts->samples < kHudsonVisionMostSamples; x++)
		{
			if (pictures.colour[y * pictures.colourPitch + x * 4 + 3] != 255)
			{
				if (seen == NextToPick(counts->samples, counts->nothingDrawn.pixels, kHudsonVisionMostSamples))
				{
					counts->sampleX[counts->samples] = (int)x;
					counts->sampleY[counts->samples] = (int)y;
					counts->samples++;
				}

				seen++;
			}
		}
	}

	// And a few with a surface to check rays against: one in each of eight columns and each of eight rows, mixed up so that they don't fall along one line. Picking every so many, the way the samples above do, would put
	// them all down the left edge of a full screen, because every row is the same length. A pixel with no surface or no distance there is simply one check fewer.
	static uint32_t const rowForColumn[kHudsonVisionMostChecks] = { 0, 5, 2, 7, 4, 1, 6, 3 };

	for (uint32_t column = 0; width > 0 && height > 0 && column < (uint32_t)kHudsonVisionMostChecks; column++)
	{
		uint32_t const x = width * (2 * column + 1) / (2 * kHudsonVisionMostChecks);
		uint32_t const y = height * (2 * rowForColumn[column] + 1) / (2 * kHudsonVisionMostChecks);

		float distance = 0.0f;
		memcpy(&distance, pictures.distance + y * pictures.distancePitch + x * 4, sizeof(distance));

		if (pictures.colour[y * pictures.colourPitch + x * 4 + 3] == 255 && distance != kHudsonVisionClearedDistance)
		{
			counts->checkX[counts->checks] = (int)x;
			counts->checkY[counts->checks] = (int)y;
			counts->checkDistance[counts->checks] = distance;
			counts->checks++;
		}
	}
}

uint32_t HudsonVision_Check(const uint8_t *bytes, size_t size)
{
	uint32_t check = 2166136261u;

	for (size_t i = 0; i < size; i++)
	{
		check ^= bytes[i];
		check *= 16777619u;
	}

	return check;
}

bool HudsonVision_ScreenAt(const uint8_t *bits, uint32_t pitch, int width, int height, int x, int y, HudsonVisionScreenPixel *pixel)
{
	if (x < 0 || y < 0 || x >= width || y >= height)
	{
		return false;
	}

	int total[3] = { 0, 0, 0 };
	int count = 0;

	memset(pixel, 0, sizeof(*pixel));
	pixel->x = x;
	pixel->y = y;

	for (int aroundY = y - 1; aroundY <= y + 1; aroundY++)
	{
		for (int aroundX = x - 1; aroundX <= x + 1; aroundX++)
		{
			if (aroundX < 0 || aroundY < 0 || aroundX >= width || aroundY >= height)
			{
				continue;
			}

			uint8_t const *const here = bits + aroundY * pitch + aroundX * 4;

			if (aroundX == x && aroundY == y)
			{
				pixel->colour[0] = here[2];
				pixel->colour[1] = here[1];
				pixel->colour[2] = here[0];
				continue;
			}

			total[0] += here[2];
			total[1] += here[1];
			total[2] += here[0];
			count++;
		}
	}

	int brightestHere = 0, brightestAround = 0;

	for (int i = 0; i < 3; i++)
	{
		pixel->around[i] = count ? total[i] / count : pixel->colour[i];

		if (abs(pixel->colour[i] - pixel->around[i]) > kHudsonVisionScreenDiffers)
		{
			pixel->differs = true;
		}

		brightestHere = pixel->colour[i] > brightestHere ? pixel->colour[i] : brightestHere;
		brightestAround = pixel->around[i] > brightestAround ? pixel->around[i] : brightestAround;
	}

	pixel->tooDark = brightestHere < kHudsonVisionScreenTooDark && brightestAround < kHudsonVisionScreenTooDark;

	if (!pixel->tooDark && brightestHere * 4 <= brightestAround)
	{
		pixel->differs = true;
	}

	return true;
}

bool HudsonVision_ShadowsAreMissing(const HudsonVisionShadows &shadows)
{
	return shadows.volumesOn && shadows.lights > 0 && shadows.casters > 0 && shadows.corners == 0;
}

double HudsonVision_HowFarAhead(int rayDistance, const float *direction, const float *view)
{
	double const ahead = (double)direction[0] * view[8] + (double)direction[1] * view[9] + (double)direction[2] * view[10];

	return (double)rayDistance * ahead;
}

bool HudsonVision_SeenFromBehind(const float *direction, const float *plane)
{
	return (double)direction[0] * plane[0] + (double)direction[1] * plane[1] + (double)direction[2] * plane[2] < 0.0;
}

bool HudsonVision_RayAgrees(float pictureDistance, int rayDistance, const float *direction, const float *view)
{
	double const fromTheRay = HudsonVision_HowFarAhead(rayDistance, direction, view);
	double const allowed = fabs((double)pictureDistance) * 0.03 > 20.0 ? fabs((double)pictureDistance) * 0.03 : 20.0;

	return fabs(fromTheRay - (double)pictureDistance) <= allowed;
}

bool HudsonVision_RayThrough(int x, int y, int width, int height, float projection11, float projection22, const float *view, float *direction)
{
	if (x < 0 || y < 0 || x >= width || y >= height || projection11 == 0.0f || projection22 == 0.0f)
	{
		return false;
	}

	// A pixel's middle in Direct3D 9 is exactly on its number. This is how far across and up it is, for each step forward.
	double const across = ((double)x * 2.0 / (double)width - 1.0) / (double)projection11;
	double const up = (1.0 - (double)y * 2.0 / (double)height) / (double)projection22;
	double along[3], length = 0.0;

	// The same sum as shadow_shoot.cpp's, where the row for up points down the screen in the game's numbers
	for (int i = 0; i < 3; i++)
	{
		along[i] = (double)view[8 + i] + across * (double)view[0 + i] - up * (double)view[4 + i];
		length += along[i] * along[i];
	}

	length = sqrt(length);

	if (length == 0.0)
	{
		return false;
	}

	for (int i = 0; i < 3; i++)
	{
		direction[i] = (float)(along[i] / length);
	}

	return true;
}
