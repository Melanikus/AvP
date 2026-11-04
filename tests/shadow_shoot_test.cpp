// The test for 3dc/shadow_shoot_sums.h, the sums behind the shoot tool. Written 2026-11-04 by Melanikus for the AvP port. It comes out before merge, with the tool.
//
// WHAT IT TESTS
//
// That a ray through a triangle finds it, at the right depth, counted the right way round. That a ray landing exactly on an edge or corner that triangles share counts just once, because
// counting twice or not at all would make the tool report a mismatch that isn't there. That a whole prism adds up to one only for a place inside it. That the depth levels, the half floats,
// the names of a prism's parts, the corner sum from shaders/volumeCut.vsh, the tally and the list of stopped polygons all do what they say.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs no game files and none of the game's code.

#include <math.h>
#include <stdio.h>

#include "shadow_shoot_sums.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static const ShootRay kStraightAhead = { 0.0, 0.0 };

// Anticlockwise as the camera sees it, so it's facing away, and adds one.
static void TestOneTriangle()
{
	float const a[3] = { -10.0f, -10.0f, 100.0f }, b[3] = { 10.0f, -10.0f, 100.0f }, c[3] = { 0.0f, 10.0f, 300.0f };
	double depth = 0.0;
	bool addsOne = false;

	Check(Shoot_RayCrosses(a, b, c, kStraightAhead, &depth, &addsOne), "a ray through a triangle finds it");
	Check(fabs(depth - 200.0) < 0.0001, "and at the depth halfway up it");
	Check(addsOne, "anticlockwise as the camera sees it adds one");

	Check(Shoot_RayCrosses(a, c, b, kStraightAhead, &depth, &addsOne) && !addsOne, "the same triangle turned round takes one away");

	float const offA[3] = { 5.0f, 5.0f, 100.0f }, offB[3] = { 25.0f, 5.0f, 100.0f }, offC[3] = { 15.0f, 25.0f, 100.0f };

	Check(!Shoot_RayCrosses(offA, offB, offC, kStraightAhead, &depth, &addsOne), "a ray past a triangle misses it");

	float const edgeA[3] = { 0.0f, -10.0f, 100.0f }, edgeB[3] = { 0.0f, 10.0f, 100.0f }, edgeC[3] = { 0.0f, 0.0f, 300.0f };

	Check(!Shoot_RayCrosses(edgeA, edgeB, edgeC, kStraightAhead, &depth, &addsOne), "a triangle edge-on to the ray makes no pixels, and doesn't count");

	// A ray to the side, through the same triangle moved over by the same slope at each depth.
	ShootRay const toTheSide = { 0.5, -0.25 };
	float const movedA[3] = { -10.0f + 50.0f, -10.0f - 25.0f, 100.0f }, movedB[3] = { 10.0f + 50.0f, -10.0f - 25.0f, 100.0f }, movedC[3] = { 0.0f + 150.0f, 10.0f - 75.0f, 300.0f };

	Check(Shoot_RayCrosses(movedA, movedB, movedC, toTheSide, &depth, &addsOne) && fabs(depth - 200.0) < 0.0001 && addsOne, "a ray that isn't straight ahead finds it too");
}

// Counts how many of a fan of triangles round a middle corner the ray goes through.
static int CountFanHits(const float *middle, const float (*rim)[3], int count, const ShootRay &ray)
{
	int hits = 0;

	for (int i = 0; i < count; i++)
	{
		double depth = 0.0;
		bool addsOne = false;

		if (Shoot_RayCrosses(middle, rim[i], rim[(i + 1) % count], ray, &depth, &addsOne))
		{
			hits++;
		}
	}

	return hits;
}

static void TestSharedEdgesAndCorners()
{
	// two triangles sharing an edge that runs straight through the ray, each way round and on a slant
	struct Pair { float a[3], b[3], left[3], right[3]; const char *what; };

	Pair const pairs[] =
	{
		{ { 0.0f, -10.0f, 100.0f }, { 0.0f, 10.0f, 100.0f }, { -10.0f, 0.0f, 100.0f }, { 10.0f, 0.0f, 100.0f }, "an upright edge" },
		{ { -10.0f, 0.0f, 100.0f }, { 10.0f, 0.0f, 100.0f }, { 0.0f, 10.0f, 100.0f }, { 0.0f, -10.0f, 100.0f }, "a level edge" },
		{ { -10.0f, -10.0f, 100.0f }, { 10.0f, 10.0f, 100.0f }, { -10.0f, 10.0f, 100.0f }, { 10.0f, -10.0f, 100.0f }, "a slanted edge" },
	};

	for (int i = 0; i < 3; i++)
	{
		Pair const &pair = pairs[i];
		double depth = 0.0;
		bool addsOne = false;

		// these go round the same way, as two neighbouring parts of one surface do
		int const hits = (Shoot_RayCrosses(pair.a, pair.b, pair.left, kStraightAhead, &depth, &addsOne) ? 1 : 0) + (Shoot_RayCrosses(pair.b, pair.a, pair.right, kStraightAhead, &depth, &addsOne) ? 1 : 0);
		char words[80];

		sprintf(words, "a ray exactly on %s counts for just one of its two triangles", pair.what);
		Check(hits == 1, words);
	}

	// a ray exactly on a corner that a whole fan of triangles shares, for several sizes of fan and turns
	float const middle[3] = { 0.0f, 0.0f, 100.0f };

	for (int size = 3; size <= 8; size++)
	{
		for (int turn = 0; turn < 4; turn++)
		{
			float rim[8][3];

			for (int i = 0; i < size; i++)
			{
				float const angle = (float)(((double)i + (double)turn * 0.125) * 6.283185307179586 / (double)size);

				rim[i][0] = 100.0f * cosf(angle);
				rim[i][1] = 100.0f * sinf(angle);
				rim[i][2] = 100.0f;
			}

			if (CountFanHits(middle, rim, size, kStraightAhead) != 1)
			{
				Check(false, "a ray exactly on a shared corner counts for just one triangle of the fan");
				return;
			}
		}
	}

	Check(true, "a ray exactly on a shared corner counts for just one triangle of the fan, for fans of 3 to 8 at four turns");

	// A prism's silhouette: two triangles meeting at an edge the ray lies on, both on the same side of it, one facing the camera and one facing away. The card gives both of them that pixel or neither, because
	// which pixels a triangle gets doesn't depend on which way it faces. Counting one and not the other would add up to a shadow that isn't there.
	struct Fold { float a[3], b[3], side[3]; const char *what; };

	Fold const folds[] =
	{
		{ { 0.0f, -10.0f, 100.0f }, { 0.0f, 10.0f, 100.0f }, { -10.0f, 0.0f, 100.0f }, "an upright silhouette, on its left" },
		{ { 0.0f, -10.0f, 100.0f }, { 0.0f, 10.0f, 100.0f }, { 10.0f, 0.0f, 100.0f }, "an upright silhouette, on its right" },
		{ { -10.0f, 0.0f, 100.0f }, { 10.0f, 0.0f, 100.0f }, { 0.0f, 10.0f, 100.0f }, "a level silhouette, above it" },
		{ { -10.0f, 0.0f, 100.0f }, { 10.0f, 0.0f, 100.0f }, { 0.0f, -10.0f, 100.0f }, "a level silhouette, below it" },
	};

	for (int i = 0; i < 4; i++)
	{
		Fold const &fold = folds[i];
		float const further[3] = { fold.side[0] * 2.0f, fold.side[1] * 2.0f, 200.0f };
		double depth = 0.0;
		bool addsOne = false, otherAddsOne = false;
		bool const first = Shoot_RayCrosses(fold.a, fold.b, fold.side, kStraightAhead, &depth, &addsOne);
		bool const second = Shoot_RayCrosses(fold.b, fold.a, further, kStraightAhead, &depth, &otherAddsOne);
		char words[100];

		sprintf(words, "%s: both triangles count or neither does", fold.what);
		Check(first == second, words);
	}
}

static void TestDistanceFromAnEdge()
{
	// on the screen, at 100 pixels for each unit of across and up: a triangle with corners at -100 -100, 100 -100 and 0 100, and the ray in the middle. The nearest edge is a slanted one, 44.72 away.
	float const a[3] = { -100.0f, -100.0f, 100.0f }, b[3] = { 100.0f, -100.0f, 100.0f }, c[3] = { 0.0f, 100.0f, 100.0f };

	Check(fabs(Shoot_PixelsFromEdge(a, b, c, kStraightAhead, 100.0, 100.0, 64.0) - 44.72) < 0.01, "the distance to a triangle's nearest edge, in pixels");

	// a ray past its end is measured to the end of the edge, not to the line it's on
	ShootRay const pastTheEnd = { 2.0, -1.0 };

	Check(fabs(Shoot_PixelsFromEdge(a, b, c, pastTheEnd, 100.0, 100.0, 64.0) - 100.0) < 0.01, "and to its nearest corner, when that's nearer than any edge goes");

	float const tooClose[3] = { 0.0f, 100.0f, 10.0f };

	Check(Shoot_PixelsFromEdge(a, b, tooClose, kStraightAhead, 100.0, 100.0, 64.0) == -1.0, "a corner closer than the near plane can't be put on the screen, so it says so");
}

// A box with its faces wound as a prism's are: every face goes anticlockwise as seen from outside looking at its far side, so that a face turned away from the camera adds one.
static void AddUpBox(float low, float high, double wallDepth, int *count)
{
	float const corners[8][3] =
	{
		{ -50.0f, -50.0f, low }, { 50.0f, -50.0f, low }, { 50.0f, 50.0f, low }, { -50.0f, 50.0f, low },
		{ -50.0f, -50.0f, high }, { 50.0f, -50.0f, high }, { 50.0f, 50.0f, high }, { -50.0f, 50.0f, high },
	};

	// wound clockwise seen from outside, so a face seen from the inside of the box goes anticlockwise
	static const int kTriangles[12][3] =
	{
		{ 0, 2, 1 }, { 0, 3, 2 },   // the end nearest the camera
		{ 4, 5, 6 }, { 4, 6, 7 },   // the far end
		{ 0, 1, 5 }, { 0, 5, 4 },
		{ 1, 2, 6 }, { 1, 6, 5 },
		{ 2, 3, 7 }, { 2, 7, 6 },
		{ 3, 0, 4 }, { 3, 4, 7 },
	};

	ShootRay const ray = { 0.01, 0.02 };
	int total = 0;

	for (int i = 0; i < 12; i++)
	{
		double depth = 0.0;
		bool addsOne = false;

		if (Shoot_RayCrosses(corners[kTriangles[i][0]], corners[kTriangles[i][1]], corners[kTriangles[i][2]], ray, &depth, &addsOne) && depth > wallDepth)
		{
			total += addsOne ? 1 : -1;
		}
	}

	*count = total;
}

static void TestAWholeBox()
{
	int count = 99;

	AddUpBox(1000.0f, 2000.0f, 1500.0, &count);
	Check(count == 1, "a place inside a box adds up to one");

	AddUpBox(1000.0f, 2000.0f, 500.0, &count);
	Check(count == 0, "a place in front of a box adds up to nothing, because both its faces are hidden and they cancel");

	AddUpBox(1000.0f, 2000.0f, 2500.0, &count);
	Check(count == 0, "a place behind a box adds up to nothing, because nothing of it is hidden");
}

static void TestDepthLevels()
{
	// the game's own: a near plane of 64 and a far plane of 1,000,000
	double const projection33 = 1000000.0 / (1000000.0 - 64.0);
	double const projection43 = -64.0 * projection33;

	Check(fabs(Shoot_DepthLevel(64.0, projection33, projection43)) < 0.01, "the near plane is level 0");
	Check(fabs(Shoot_DepthLevel(1000000.0, projection33, projection43) - 16777216.0) < 0.01, "the far plane is the last level");

	double const level = Shoot_DepthLevel(6000.0, projection33, projection43);

	Check(fabs(Shoot_DepthOfLevel(level, projection33, projection43) - 6000.0) < 0.0001, "a level turns back into its depth");

	// a metre is 1000 units, and d3d_render.cpp says one step is 3.4 units at 60 metres
	double const step = 60000.0 * 60000.0 / (-projection43 * 16777216.0);

	Check(fabs(step - 3.35) < 0.01, "a step at 60 metres is the size DepthStepAt gives");
	Check(fabs(Shoot_DepthLevel(60000.0 + step, projection33, projection43) - Shoot_DepthLevel(60000.0, projection33, projection43) - 1.0) < 0.001, "and going that far back moves one level");
}

static void TestHalfFloats()
{
	Check(Shoot_HalfFloat(0x0000) == 0.0f, "half float 0");
	Check(Shoot_HalfFloat(0x3C00) == 1.0f, "half float 1");
	Check(Shoot_HalfFloat(0xC000) == -2.0f, "half float -2");
	Check(Shoot_HalfFloat(0x3800) == 0.5f, "half float a half");
	Check(fabsf(Shoot_HalfFloat(0x3555) - 0.33325f) < 0.0001f, "half float a third");
	Check(Shoot_HalfFloat(0x0400) == 0.00006103515625f, "the smallest ordinary half float");
	Check(Shoot_HalfFloat(0x0200) == 0.000030517578125f, "half of that, which is stored differently");
	Check(Shoot_HalfFloat(0x0001) == 0.000000059604644775390625f, "the smallest half float of all");
	Check(Shoot_HalfFloat(0x8001) == -0.000000059604644775390625f, "and the same below zero");
}

static void TestPrismParts()
{
	int side = -1;

	Check(Shoot_CornersOfPrism(3) == 24 && Shoot_CornersOfPrism(4) == 36 && Shoot_CornersOfPrism(5) == 48, "a prism's corners: 8 triangles for 3 corners, 12 for 4, 16 for 5");

	Check(Shoot_PrismPartOf(0, 4, &side) == kShootNearCap && Shoot_PrismPartOf(1, 4, &side) == kShootFarCap, "a four-cornered prism starts with a near triangle and then a far one");
	Check(Shoot_PrismPartOf(2, 4, &side) == kShootNearCap && Shoot_PrismPartOf(3, 4, &side) == kShootFarCap, "and then the same again");
	Check(Shoot_PrismPartOf(4, 4, &side) == kShootSide && side == 0 && Shoot_PrismPartOf(5, 4, &side) == kShootSide && side == 0, "then two triangles for its first side");
	Check(Shoot_PrismPartOf(11, 4, &side) == kShootSide && side == 3, "and the last triangle is on its last side");
	Check(Shoot_PrismPartOf(0, 3, &side) == kShootNearCap && Shoot_PrismPartOf(1, 3, &side) == kShootFarCap && Shoot_PrismPartOf(2, 3, &side) == kShootSide && side == 0, "a three-cornered one has one of each cap");
}

static void TestCutCorners()
{
	// a view that only moves things: x y z, take away 0.5 1.5 and 2.5
	float const view[12] = { 1.0f, 0.0f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f, -1.5f, 0.0f, 0.0f, 1.0f, -2.5f };
	float const light[3] = { 0.0f, 0.0f, 0.0f };
	float const corner[3] = { 100.75f, 200.75f, 300.75f };
	float onList[3];

	Shoot_WorldToList(view, corner, onList);
	Check(onList[0] == 100.25f && onList[1] == -199.25f && onList[2] == 298.25f, "a place in the world, in the list's numbers, has y turned over");

	// a near end: the cut corner, 100 200 298 and with y turned over -199, and then 8 along the ray from the light
	Shoot_FinishCutCorner(view, light, false, corner, 0.0f, 1000.0f, onList);
	Check(onList[0] == 100.0f && onList[1] == -199.0f && onList[2] == 298.0f, "a near end with no gap is the corner cut towards zero, with y turned over");

	float const length = sqrtf(100.0f * 100.0f + 199.0f * 199.0f + 298.0f * 298.0f);

	Shoot_FinishCutCorner(view, light, false, corner, 8.0f, 1000.0f, onList);
	Check(fabsf(onList[0] - (100.0f + 100.0f / length * 8.0f)) < 0.001f && fabsf(onList[2] - (298.0f + 298.0f / length * 8.0f)) < 0.001f, "a near end starts its gap along the ray from the light");

	// a far end, from the mark
	Shoot_FinishCutCorner(view, light, false, corner, -(1000.0f + 5000.0f), 1000.0f, onList);
	Check(fabsf(onList[0] - 100.0f / length * 5000.0f) < 0.01f && fabsf(onList[2] - 298.0f / length * 5000.0f) < 0.01f, "a far end is that far from the light, along the ray through the cut corner");

	float const exactLength = sqrtf(100.25f * 100.25f + 199.25f * 199.25f + 298.25f * 298.25f);

	Shoot_FinishCutCorner(view, light, true, corner, -(1000.0f + 5000.0f), 1000.0f, onList);
	Check(fabsf(onList[0] - 100.25f / exactLength * 5000.0f) < 0.01f, "with far ends exact, it's the ray through the corner before the cut");
}

static void TestTally()
{
	ShootTally tally = { 0, 0, 0 };

	Shoot_CountCrossing(tally, true, 100.0);      // hidden, facing away
	Check(tally.sure == 1, "a triangle clearly hidden and facing away adds one");

	Shoot_CountCrossing(tally, false, 100.0);     // hidden, facing the camera
	Check(tally.sure == 0, "one facing the camera takes it off again");

	Shoot_CountCrossing(tally, true, -100.0);     // in front of the wall
	Shoot_CountCrossing(tally, false, -100.0);
	Check(tally.sure == 0 && tally.mayAdd == 0 && tally.mayTake == 0, "triangles clearly in front of the wall don't count at all");

	Shoot_CountCrossing(tally, true, 0.5);
	Check(tally.sure == 0 && tally.mayAdd == 1, "a triangle right on the wall might add one");

	Check(Shoot_TallyAgrees(tally, 0) && Shoot_TallyAgrees(tally, 1) && !Shoot_TallyAgrees(tally, 2), "so the stencil can be 0 or 1, and not 2");

	ShootTally taking = { 0, 0, 1 };

	Check(Shoot_TallyAgrees(taking, 255) && Shoot_TallyAgrees(taking, 0) && !Shoot_TallyAgrees(taking, 1), "the stencil goes round from 0 to 255 when it takes one away, and 255 reads as minus one");
}

static void TestStops()
{
	ShootStopList stops = { { 0 }, { 0 }, 0 };
	int const shapeA = 0, shapeB = 0;

	Check(!stops.TakeOffNewest(), "with nothing stopped, undoing does nothing");
	Check(stops.Add(&shapeA, 3) == 0 && stops.Has(&shapeA, 3), "a polygon can be stopped");
	Check(!stops.Has(&shapeA, 4) && !stops.Has(&shapeB, 3), "and that's only that polygon of that shape");
	Check(stops.HasShape(&shapeA) && !stops.HasShape(&shapeB), "its shape is known to have a stop");
	Check(stops.Add(&shapeA, 3) == -1 && stops.count == 1, "stopping it twice only counts once");
	Check(stops.Add(&shapeB, 3) == 1 && stops.Add(&shapeA, 9) == 2, "others can join it");

	Check(stops.TakeOffNewest() && !stops.Has(&shapeA, 9) && stops.Has(&shapeB, 3), "undoing takes off the newest");

	stops.count = 0;

	for (int i = 0; i < kShootMostStops; i++)
	{
		stops.Add(&shapeA, i);
	}

	Check(stops.count == kShootMostStops && stops.Add(&shapeA, 1000) == -1 && stops.count == kShootMostStops && !stops.Has(&shapeA, 1000), "a full list turns a new one down and changes nothing");
}

int main()
{
	printf("The shoot tool's sums:\n");

	TestOneTriangle();
	TestSharedEdgesAndCorners();
	TestDistanceFromAnEdge();
	TestAWholeBox();
	TestDepthLevels();
	TestHalfFloats();
	TestPrismParts();
	TestCutCorners();
	TestTally();
	TestStops();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
