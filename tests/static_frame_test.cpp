// The test for 3dc/static_frame.cpp: putting a frame's ranges in order, and comparing two routes' pictures.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this with the real static_frame.cpp, and the program gives back a non-zero result if any check failed. It uses no files,
// no graphics card and none of the game.

#include <stdio.h>
#include <string.h>

#include "static_frame.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static StaticRange Range(uint32_t texture, uint32_t firstIndex, uint32_t triangles, bool twoSided = false, uint32_t baseVertex = 0)
{
	StaticRange range;

	range.texture = texture;
	range.baseVertex = baseVertex;
	range.firstVertex = firstIndex;
	range.vertexCount = triangles * 3;
	range.firstIndex = firstIndex;
	range.triangles = triangles;
	range.twoSided = twoSided;

	return range;
}

static void CheckJoining()
{
	Check(StaticFrame_SortAndJoin(0, 0) == 0, "no ranges at all: none come back");

	{
		StaticRange ranges[3] = { Range(5, 60, 10), Range(5, 0, 10), Range(5, 30, 10) };
		int const left = StaticFrame_SortAndJoin(ranges, 3);
		Check(left == 1 && ranges[0].firstIndex == 0 && ranges[0].triangles == 30 && ranges[0].vertexCount == 90, "three neighbours, in any order, become one");
	}

	{
		StaticRange ranges[4] = { Range(5, 0, 10), Range(5, 30, 10), Range(5, 0, 10), Range(5, 30, 10) };
		int const left = StaticFrame_SortAndJoin(ranges, 4);
		Check(left == 1 && ranges[0].triangles == 20, "a module noted twice is only drawn once");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10), Range(5, 33, 10) };
		Check(StaticFrame_SortAndJoin(ranges, 2) == 2, "a gap in the buffer keeps two ranges apart: nothing between them may be drawn");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10), Range(6, 30, 10) };
		Check(StaticFrame_SortAndJoin(ranges, 2) == 2, "two textures stay apart, even side by side");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10, false), Range(5, 30, 10, true) };
		int const left = StaticFrame_SortAndJoin(ranges, 2);
		Check(left == 2 && ranges[0].twoSided && !ranges[1].twoSided, "a two-sided range stays apart from an ordinary one, and goes first");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10, false, 0), Range(5, 30, 10, false, 1000) };
		Check(StaticFrame_SortAndJoin(ranges, 2) == 2, "a different base vertex keeps ranges apart: the same index would mean another vertex");
	}

	{
		StaticRange ranges[4] = { Range(9, 90, 5), Range(2, 30, 5), Range(9, 105, 5), Range(2, 45, 5) };
		int const left = StaticFrame_SortAndJoin(ranges, 4);
		Check(left == 2 && ranges[0].texture == 2 && ranges[0].triangles == 10 && ranges[1].texture == 9 && ranges[1].triangles == 10, "sorted by texture, with each texture joined");
	}

	{
		StaticRange first = Range(5, 0, 10);
		StaticRange second = Range(5, 30, 10);
		first.firstVertex = 500;
		first.vertexCount = 10;
		second.firstVertex = 100;
		second.vertexCount = 20;

		StaticRange ranges[2] = { first, second };
		StaticFrame_SortAndJoin(ranges, 2);
		Check(ranges[0].firstVertex == 100 && ranges[0].vertexCount == 410, "a joined range covers every vertex either part uses, or the card could leave some out");
	}
}

// Two small sets of pictures, the same to start with.
static const int kWidth = 4;
static const int kHeight = 3;

struct TestPictures
{
	float distance[kWidth * kHeight];
	uint8_t normal[kWidth * kHeight * 4];
	uint8_t colour[kWidth * kHeight * 4];

	TestPictures()
	{
		for (int i = 0; i < kWidth * kHeight; i++)
		{
			distance[i] = 1000.0f + (float)i;
			memset(&normal[i * 4], 128, 4);
			colour[i * 4 + 0] = 40;
			colour[i * 4 + 1] = 80;
			colour[i * 4 + 2] = 120;
			colour[i * 4 + 3] = 255;
		}
	}

	SurfacePicturesInMemory InMemory() const
	{
		SurfacePicturesInMemory pictures;

		pictures.distance = (const uint8_t *)distance;
		pictures.normal = normal;
		pictures.colour = colour;
		pictures.distancePitch = kWidth * 4;
		pictures.normalPitch = kWidth * 4;
		pictures.colourPitch = kWidth * 4;

		return pictures;
	}
};

static void CheckCompare()
{
	PictureDifferences found;

	{
		TestPictures a, b;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.pixelsCompared == kWidth * kHeight && !found.holesInNew && !found.extraInNew && !found.colourChanged && !found.normalChanged && !found.distanceChanged, "the same pictures: no differences, and every pixel compared");
	}

	{
		TestPictures a, b;
		b.colour[7] = 0;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.holesInNew == 1 && found.extraInNew == 0, "a surface missing from the new route is a hole");
	}

	{
		TestPictures a, b;
		a.colour[7] = 0;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.extraInNew == 1 && found.holesInNew == 0, "a surface only in the new route is counted as well");
	}

	{
		TestPictures a, b;
		b.colour[4] = 42;
		b.distance[2] += 0.25f;
		b.normal[12] += 2;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(!found.colourChanged && !found.distanceChanged && !found.normalChanged && found.largestDistanceChange == 0.25f, "rounding isn't a change, but the largest distance is still reported");
	}

	{
		TestPictures a, b;
		b.colour[4] = 44;
		b.distance[2] += 3.0f;
		b.normal[12] += 3;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.colourChanged == 1 && found.distanceChanged == 1 && found.normalChanged == 1 && found.largestDistanceChange == 3.0f, "real changes are each counted on their own");
	}

	{
		TestPictures a, b;
		uint32_t const oldNormal = (500u << 20) | (500u << 10) | 500u;
		uint32_t const nearby = (505u << 20) | (500u << 10) | 500u;
		uint32_t const further = (500u << 20) | (509u << 10) | 500u;

		memcpy(&a.normal[0], &oldNormal, 4);
		memcpy(&b.normal[0], &nearby, 4);
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, true, &found);
		Check(found.normalChanged == 0, "ten bits a direction: a small difference is rounding");

		memcpy(&b.normal[0], &further, 4);
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, true, &found);
		Check(found.normalChanged == 1, "ten bits a direction: each direction is read on its own");
	}

	{
		TestPictures a, b;
		a.colour[3] = 0;
		b.colour[3] = 0;
		b.colour[0] = 200;
		b.distance[0] = 9.0f;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(!found.colourChanged && !found.distanceChanged, "where neither route has a surface, nothing in the pictures matters");
	}
}

int main()
{
	CheckJoining();
	CheckCompare();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all static frame checks passed\n");
	return 0;
}
