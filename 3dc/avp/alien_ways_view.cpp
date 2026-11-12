// The ALIEN WAYS developer view: the way each alien is following, drawn in the level as lines. Written 2026-11-08 by Melanikus for the AvP
// port (issue #109). See alien_ways_view.h.
#include "alien_ways_view.h"

#include <algorithm>

uint32_t AlienWaysView_ColourOf(AlienWaysKind kind)
{
	switch (kind)
	{
	case kAlienWaysFloor:
		return kAlienWaysViewFloor;

	case kAlienWaysWall:
		return kAlienWaysViewWall;

	case kAlienWaysCeiling:
		return kAlienWaysViewCeiling;

	default:
		return kAlienWaysViewUnknown;
	}
}

// Takes a place in the level into the card's numbers: the camera's view, by the game's ViewMatrix, then up turned over, as the game's own
// polygons are (d3d_render.cpp writes their corners' y as -Y)
static AlienWaysViewEnd IntoView(const float *matrix, float x, float y, float z, uint32_t colour)
{
	AlienWaysViewEnd end;
	end.x = matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3];
	end.y = -(matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7]);
	end.z = matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11];
	end.colour = colour;
	return end;
}

// One line, if there's room for both its ends
static bool AddLine(const float *matrix, float x1, float y1, float z1, float x2, float y2, float z2, uint32_t colour, std::vector<AlienWaysViewEnd> *ends)
{
	if (ends->size() + 2 > kAlienWaysViewMostEnds)
	{
		return false;
	}

	ends->push_back(IntoView(matrix, x1, y1, z1, colour));
	ends->push_back(IntoView(matrix, x2, y2, z2, colour));
	return true;
}

size_t AlienWaysView_AddWay(const float *matrix, const AlienWaysPoint *points, int count, int heading, std::vector<AlienWaysViewEnd> *ends)
{
	if (!matrix || !points || !ends || count <= 0)
	{
		return 0;
	}

	size_t const before = ends->size();
	int const drawn = count < (int)kAlienWaysMostWayPoints ? count : (int)kAlienWaysMostWayPoints;

	for (int i = 0; i < drawn; i++)
	{
		AlienWaysPoint const &point = points[i];
		float const x = (float)point.x, y = (float)point.y, z = (float)point.z;
		bool const headingHere = i == heading;

		// The stretch from this point to the next is on this point's kind. The one into the point it's heading for is the one it's on.
		if (i + 1 < drawn)
		{
			AlienWaysPoint const &next = points[i + 1];
			uint32_t const colour = (i + 1 == heading) ? kAlienWaysViewHere : AlienWaysView_ColourOf(point.kind);

			if (!AddLine(matrix, x, y, z, (float)next.x, (float)next.y, (float)next.z, colour, ends))
			{
				break;
			}
		}

		// A cross of three lines, one along each of the level's directions
		float const arm = (float)(headingHere ? 2 * kAlienWaysViewCrossArm : kAlienWaysViewCrossArm);
		uint32_t const colour = headingHere ? kAlienWaysViewHere : AlienWaysView_ColourOf(point.kind);

		if (!AddLine(matrix, x - arm, y, z, x + arm, y, z, colour, ends)
			|| !AddLine(matrix, x, y - arm, z, x, y + arm, z, colour, ends)
			|| !AddLine(matrix, x, y, z - arm, x, y, z + arm, colour, ends))
		{
			break;
		}
	}

	return ends->size() - before;
}

uint32_t AlienWaysView_Dimmed(uint32_t colour)
{
	uint32_t const red = ((colour >> 16) & 0xffu) * 2 / 5;
	uint32_t const green = ((colour >> 8) & 0xffu) * 2 / 5;
	uint32_t const blue = (colour & 0xffu) * 2 / 5;
	return (colour & 0xff000000u) | (red << 16) | (green << 8) | blue;
}

size_t AlienWaysView_AddKeptWay(const float *matrix, const AlienWaysPoint *points, int count, std::vector<AlienWaysViewEnd> *ends)
{
	if (!ends)
	{
		return 0;
	}

	size_t const before = ends->size();
	size_t const added = AlienWaysView_AddWay(matrix, points, count, -1, ends);

	for (size_t e = before; e < before + added; e++)
	{
		(*ends)[e].colour = AlienWaysView_Dimmed((*ends)[e].colour);
	}

	return added;
}

// Added 2026-11-11 by Melanikus (Third) for the AvP port (#109): thicker lines (alien_ways_view.h)
size_t AlienWaysView_Thicken(std::vector<AlienWaysViewEnd> *ends, float aside)
{
	if (!ends || !(aside > 0.0f && aside < 1.0f))
	{
		return 0;
	}

	// Right, left, up and down on the screen
	static const float kMoves[4][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f } };
	size_t const before = ends->size();
	size_t const lines = before / 2;
	ends->reserve(std::min(before * 5, kAlienWaysViewMostThickEnds));

	for (int m = 0; m < 4; m++)
	{
		for (size_t line = 0; line < lines; line++)
		{
			if (ends->size() + 2 > kAlienWaysViewMostThickEnds)
			{
				return ends->size() - before;
			}

			for (size_t e = 0; e < 2; e++)
			{
				// A copy first, then added: the list can move in memory as it grows
				AlienWaysViewEnd moved = (*ends)[line * 2 + e];
				moved.x += kMoves[m][0] * aside * moved.z;
				moved.y += kMoves[m][1] * aside * moved.z;
				ends->push_back(moved);
			}
		}
	}

	return ends->size() - before;
}
