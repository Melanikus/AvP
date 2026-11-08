// The test for 3dc/static_frame.cpp: putting a frame's ranges in order, and comparing two routes' pictures.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this with the real static_frame.cpp, and the program gives back a non-zero result if any check failed. It uses no files,
// no graphics card and none of the game.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
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

static const uint32_t kNoLimit = 0xFFFFFFFF;

static void CheckJoining()
{
	Check(StaticFrame_SortAndJoin(0, 0, kNoLimit) == 0, "no ranges at all: none come back");

	{
		StaticRange ranges[3] = { Range(5, 60, 10), Range(5, 0, 10), Range(5, 30, 10) };
		int const left = StaticFrame_SortAndJoin(ranges, 3, kNoLimit);
		Check(left == 1 && ranges[0].firstIndex == 0 && ranges[0].triangles == 30 && ranges[0].vertexCount == 90, "three neighbours, in any order, become one");
	}

	{
		StaticRange ranges[4] = { Range(5, 0, 10), Range(5, 30, 10), Range(5, 0, 10), Range(5, 30, 10) };
		int const left = StaticFrame_SortAndJoin(ranges, 4, kNoLimit);
		Check(left == 1 && ranges[0].triangles == 20, "a module noted twice is only drawn once");
	}

	{
		StaticRange ranges[3] = { Range(5, 0, 10), Range(5, 30, 10), Range(5, 60, 10) };
		int const left = StaticFrame_SortAndJoin(ranges, 3, 25);
		Check(left == 2 && ranges[0].triangles == 20 && ranges[1].triangles == 10 && ranges[1].firstIndex == 60, "a join stops at the card's limit, and nothing is lost");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10), Range(5, 33, 10) };
		Check(StaticFrame_SortAndJoin(ranges, 2, kNoLimit) == 2, "a gap in the buffer keeps two ranges apart: nothing between them may be drawn");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10), Range(6, 30, 10) };
		Check(StaticFrame_SortAndJoin(ranges, 2, kNoLimit) == 2, "two textures stay apart, even side by side");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10, false), Range(5, 30, 10, true) };
		int const left = StaticFrame_SortAndJoin(ranges, 2, kNoLimit);
		Check(left == 2 && ranges[0].twoSided && !ranges[1].twoSided, "a two-sided range stays apart from an ordinary one, and goes first");
	}

	{
		StaticRange ranges[2] = { Range(5, 0, 10, false, 0), Range(5, 30, 10, false, 1000) };
		Check(StaticFrame_SortAndJoin(ranges, 2, kNoLimit) == 2, "a different base vertex keeps ranges apart: the same index would mean another vertex");
	}

	{
		StaticRange ranges[4] = { Range(9, 90, 5), Range(2, 30, 5), Range(9, 105, 5), Range(2, 45, 5) };
		int const left = StaticFrame_SortAndJoin(ranges, 4, kNoLimit);
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
		StaticFrame_SortAndJoin(ranges, 2, kNoLimit);
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

	// The reason the count is there: two empty pictures match perfectly, and every other number says 0, the same as a clean comparison
	{
		TestPictures a, b;

		for (int i = 0; i < kWidth * kHeight; i++)
		{
			a.colour[i * 4 + 3] = 0;
			b.colour[i * 4 + 3] = 0;
		}

		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.pixelsWithSurface == 0 && found.pixelsCompared == kWidth * kHeight && !found.holesInNew && !found.extraInNew && !found.colourChanged, "two empty pictures show no differences, and the count of surfaces is what gives them away");
	}

	{
		TestPictures a, b;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.pixelsWithSurface == kWidth * kHeight, "two full pictures have a surface on every pixel");
	}

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
		Check(found.pixelsWithSurface == kWidth * kHeight, "and the old route still drew every pixel");
	}

	{
		TestPictures a, b;
		a.colour[7] = 0;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.extraInNew == 1 && found.holesInNew == 0, "a surface only in the new route is counted as well");
		Check(found.pixelsWithSurface == kWidth * kHeight - 1, "and the old route drew all but that one");
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

// How far a colour moved. It's what tells rounding from another surface, so it has to count rounding as well.
static void CheckColourStep()
{
	PictureDifferences found;

	{
		TestPictures a, b;

		b.colour[1] = 130;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.largestColourStep == 50 && found.colourChanged == 1, "the largest step is how far the colour moved");
	}

	{
		TestPictures a, b;

		b.colour[0] = 41;
		b.colour[6] = 150;
		b.colour[9] = 85;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.largestColourStep == 30, "it looks at red, and at every pixel");
	}

	{
		TestPictures a, b;

		b.colour[0] = 41;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.largestColourStep == 1 && found.colourChanged == 0, "a step too small to count as a change is still reported");
	}

	{
		TestPictures a, b;

		a.colour[3] = 0;
		b.colour[3] = 0;
		b.colour[0] = 200;
		b.colour[7] = 0;
		b.colour[4] = 200;
		StaticFrame_Compare(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &found);
		Check(found.largestColourStep == 0, "with a surface in only one route, or neither, there are no two colours to compare");
	}
}

// Pixel number i is at x = i % kWidth, y = i / kWidth.
static void CheckWhereTheyDiffer()
{
	WhereTheyDiffer where;

	{
		TestPictures a, b;

		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 0 && where.solidPixels == 0, "pictures that match don't differ anywhere");
	}

	// A plus sign, with its middle at 1,1
	{
		TestPictures a, b;
		int const plus[] = { 1, 4, 5, 6, 9 };

		for (int i = 0; i < 5; i++)
		{
			b.colour[plus[i] * 4] = 200;
		}

		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 5, "each pixel that differs is counted");
		Check(where.solidPixels == 1, "only the middle of the plus has a difference on all four sides");
		Check(where.left == 0 && where.right == 2, "the box goes from the leftmost to the rightmost");
		Check(where.top == 0 && where.bottom == 2, "and from the top to the bottom");
		Check(where.firstX == 1 && where.firstY == 0, "the first is the first reading from the top, and the box doesn't move it");
		Check(where.firstColourBefore == 0x785028, "its colour before is red, green, blue");
		Check(where.firstColourAfter == 0x7850C8, "and after");
		Check(where.firstDistanceBefore == 1001.0f && where.firstDistanceAfter == 1001.0f, "with how far away it is");
	}

	// The first pixel found isn't the leftmost, so the box has to go on looking
	{
		TestPictures a, b;

		b.colour[2 * 4] = 200;
		b.colour[4 * 4] = 200;
		b.distance[2] = 1100.0f;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.left == 0 && where.right == 2 && where.top == 0 && where.bottom == 1, "the box grows to the left of the first");
		Check(where.firstDistanceAfter == 1100.0f, "and the first's distance after comes from the second picture");
	}

	// Every pixel differs. Those round the edge have nothing beyond them
	{
		TestPictures a, b;

		for (int i = 0; i < kWidth * kHeight; i++)
		{
			b.colour[i * 4] = 200;
		}

		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == kWidth * kHeight && where.solidPixels == 2, "a pixel on the edge of the picture can't be surrounded");
	}

	// Each side of the middle matters
	int const sides[] = { 4, 6, 1, 9 };
	const char *const sideNames[] = { "with nothing to its left, it isn't solid", "or to its right", "or above", "or below" };

	for (int side = 0; side < 4; side++)
	{
		TestPictures a, b;
		int const plus[] = { 1, 4, 5, 6, 9 };

		for (int i = 0; i < 5; i++)
		{
			if (plus[i] != sides[side])
			{
				b.colour[plus[i] * 4] = 200;
			}
		}

		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.solidPixels == 0, sideNames[side]);
	}

	// Each way of differing counts on its own
	{
		TestPictures a, b;

		b.colour[3] = 0;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 1, "a hole differs");
	}

	{
		TestPictures a, b;

		a.colour[3] = 0;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 1, "and so does a surface only in the new route");
	}

	{
		TestPictures a, b;

		a.colour[3] = 0;
		b.colour[3] = 0;
		b.colour[0] = 200;
		b.distance[0] = 9.0f;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 0, "with no surface in either, nothing else in the pictures matters");
	}

	{
		TestPictures a, b;

		b.normal[0] = 200;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 1, "which way it faces differs");
	}

	{
		TestPictures a, b;

		b.distance[0] = 1100.0f;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 1, "how far away it is differs");
	}

	{
		TestPictures a, b;

		b.colour[0] = 41;
		b.normal[0] = 129;
		b.distance[0] = 1000.25f;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 0, "and rounding doesn't");
	}

	{
		TestPictures a, b;

		b.normal[0] = 133;
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, false, &where);
		Check(where.pixels == 1, "five steps in one byte is past the rounding of an eight-bit normal");
		StaticFrame_WhereTheyDiffer(a.InMemory(), b.InMemory(), kWidth, kHeight, true, &where);
		Check(where.pixels == 0, "but not of a ten-bit one");
	}
}

static void CheckTies()
{
	TiesBetweenPolygons ties;

	// Pixel 0 differs and ties, pixel 5 differs with no tie, and pixel 7 ties with both routes agreeing
	{
		TestPictures oldRoute, newRoute, backwards;

		newRoute.colour[0 * 4] = 200;
		backwards.colour[0 * 4] = 200;
		newRoute.colour[5 * 4] = 200;
		backwards.colour[7 * 4] = 200;

		StaticFrame_SplitByTies(oldRoute.InMemory(), newRoute.InMemory(), backwards.InMemory(), kWidth, kHeight, false, &ties);
		Check(ties.pixelsThatTie == 2, "a tie is any pixel that changes when the old route goes backwards, whether or not the routes differ there");
		Check(ties.onATie.pixels == 1 && ties.onATie.firstX == 0 && ties.onATie.firstY == 0, "a difference with a tie goes one way");
		Check(ties.notOnATie.pixels == 1 && ties.notOnATie.firstX == 1 && ties.notOnATie.firstY == 1, "and one without a tie goes the other");
	}

	{
		TestPictures oldRoute, newRoute, backwards;

		StaticFrame_SplitByTies(oldRoute.InMemory(), newRoute.InMemory(), backwards.InMemory(), kWidth, kHeight, false, &ties);
		Check(ties.pixelsThatTie == 0 && ties.onATie.pixels == 0 && ties.notOnATie.pixels == 0, "pictures that all match have nothing to report");
	}

	// A hole the old route doesn't have, whichever way it draws, is the worst case, and no order of drawing explains it
	{
		TestPictures oldRoute, newRoute, backwards;

		newRoute.colour[3] = 0;
		StaticFrame_SplitByTies(oldRoute.InMemory(), newRoute.InMemory(), backwards.InMemory(), kWidth, kHeight, false, &ties);
		Check(ties.notOnATie.pixels == 1 && ties.onATie.pixels == 0, "a hole isn't a tie");
	}

	// Both comparisons use the same reading of a normal
	{
		TestPictures oldRoute, newRoute, backwards;

		newRoute.normal[0] = 133;
		StaticFrame_SplitByTies(oldRoute.InMemory(), newRoute.InMemory(), backwards.InMemory(), kWidth, kHeight, true, &ties);
		Check(ties.notOnATie.pixels == 0, "a ten-bit normal stays ten-bit between the routes");

		backwards.normal[0] = 133;
		newRoute.normal[0] = 128;
		StaticFrame_SplitByTies(oldRoute.InMemory(), newRoute.InMemory(), backwards.InMemory(), kWidth, kHeight, true, &ties);
		Check(ties.pixelsThatTie == 0, "and when looking for ties");
	}
}

// Only a game can draw these, so this reads the source for the lines a run depends on. A run shows what they do.
static std::string ReadSourceFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Windows line endings are taken out, because a copy of the repository can have them or not.
	std::string text = all.str();
	std::string::size_type place = 0;

	while ((place = text.find('\r', place)) != std::string::npos)
	{
		text.erase(place, 1);
	}

	return text;
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static void CheckTheDrawing()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const pictures = ReadSourceFile("3dc/surface_pictures.cpp");
	std::string const list = ReadSourceFile("3dc/RenderList.cpp");
	std::string const onTies = Between(render, "static void SayWhetherTheRoutesDifferOnTies", "// Draws the pictures both ways and compares them.");
	std::string const surfaceRoutes = Between(render, "static void CompareSurfaceRoutes", "static void CompareSceneRoutes");
	std::string const sceneRoutes = Between(render, "static void CompareSceneRoutes", "// There are two ways to show a shadow.");

	Check(Has(onTies, "ToyDrawPictures(device, worldAxes, false, true)"), "the third drawing is the old route, backwards");
	Check(Has(onTies, "backwards.Read(device, toyDepthSurface)"), "and it's read back");
	Check(Has(onTies, "device->Clear(0, NULL, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);"), "and cleared away, so that it isn't left for the frame's real pictures");
	Check(Has(onTies, "StaticFrame_SplitByTies(oldRoute.InMemory(), newRoute.InMemory(), backwards.InMemory()"), "and the three go in the right way round");
	Check(Has(onTies, "SayWhereTheyDiffer(\"static compare\", frame, \"on a tie\", ties.onATie);"), "the log says where they differ on a tie");
	Check(Has(onTies, "SayWhereTheyDiffer(\"static compare\", frame, \"off a tie\", ties.notOnATie);"), "and off one");
	Check(Has(surfaceRoutes, "SayWhetherTheRoutesDifferOnTies(device, worldAxes, frame, oldRoute, newRoute);"), "the compare asks about ties");
	Check(Has(surfaceRoutes, "differences.holesInNew || differences.extraInNew || differences.colourChanged || differences.normalChanged || differences.distanceChanged"), "whenever anything differs");
	Check(Has(surfaceRoutes, "differences.largestColourStep"), "its line says how far a colour moved");
	Check(Has(sceneRoutes, "differences.largestColourStep"), "and so does the scene's");
	Check(Has(sceneRoutes, "SayWhereTheyDiffer(\"static scene compare\", frame, \"here\", where);"), "and the scene's says where");
	Check(Has(render, "useStatic, MainDrawUsesHeights(), backwards);"), "the backwards goes on to the pictures");
	Check(Has(pictures, "mainList->DrawOpaqueAgain(d3d.surfaceDecl, false, false, backwards);"), "and from there to the list");
	Check(Has(pictures, "mainList->DrawOpaqueAgain(d3d.surfaceReliefDecl, true, march, backwards);"), "with relief pictures too, because normals draws that way and can take the route");
	Check(Has(list, "RenderItem *it = &Items[backwards ? listIndex - 1 - i : i];"), "the list goes through its items from the end");
	Check(Has(list, "for (uint32_t triangle = numPrimitives; triangle-- > 0;)"), "and through each item's triangles from the end");
	Check(Has(list, "it->indexStart + triangle * 3, 1);"), "one at a time, three corners each");
}

int main()
{
	CheckJoining();
	CheckCompare();
	CheckColourStep();
	CheckWhereTheyDiffer();
	CheckTies();
	CheckTheDrawing();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all static frame checks passed\n");
	return 0;
}
