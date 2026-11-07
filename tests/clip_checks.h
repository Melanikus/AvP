// The checks of cutting a polygon against the edges of the view, which clip_test.cpp and clip_on_test.cpp both run.
// Written 2026-11-04 by Melanikus for the AvP port, as clip_test.cpp; moved here on 2026-11-07, so that each of the two
// tests is a file with its own code that a scanner can read by itself.
//
// WHAT IT TESTS
//
// The game can clip a polygon: cut off the parts outside the view, against five flat surfaces. Each cut can add corners, and the two lists the corners are copied between hold nine.
// Nothing used to stop a cut at nine, so a polygon that grew past that was written over whatever was next in memory. In this build only the trail behind a thrown disc goes through
// clipping, and its position and turn can come from another player over the network.
//
// This file includes the game's own 3dc/frustum.cpp, so these are the real clippers. The two lists are placed right in front of two long "watches": areas of memory filled with a
// known pattern. A write past the end of a list lands in its watch, where the test sees it, instead of damaging the test. Then it does three things:
//  - Runs about a million made-up polygons through the five cuts, in the order the trail uses, and checks no list ever goes past nine and nothing past a list changes.
//  - Runs the same polygons through a copy of how clipping worked before it had a limit, with lists long enough for anything. Where that never went past nine, the real clippers must
//    give exactly the same corners, which shows nothing changes up to nine. Where it did, it shows the polygons in this test really do reach the problem.
//  - Checks the same limit on the other clippers, and that the game's log hears about it only once.
//
// clip_test.cpp runs these checks as the game builds today. clip_on_test.cpp runs them with the switch USE_FRUSTUM_CULL turned on, which is how a level file's polygons would get
// here if that were ever done. The switch has to be set before the game's frustum.cpp is read, so each test sets it (or not) and then includes this file.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles clip_test.cpp and clip_on_test.cpp, each a program of its own. Each includes this file once and calls RunTheClipChecks() from its main(), which gives back
// a non-zero result if anything failed.

#ifndef CLIP_CHECKS_H
#define CLIP_CHECKS_H

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <math.h>
#include <corecrt_io.h> // _dup and _close. The game has an io.h of its own, which would be found first

#include "frustum.cpp"

static int checksFailed = 0;

// Where this test's report goes. The game's divide (DIV_FIXED, 3dc/mathline.cpp) prints a line each time it's asked to divide by zero, and with the first corner assumed to be inside
// that happens tens of thousands of times here. It gives back zero and carries on, so that's harmless, but those lines would bury the report. So RunTheClipChecks() sends ordinary printing nowhere.
static FILE *report = 0;

static void Check(bool passed, std::string const &what)
{
	fprintf(report, "  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- What the game keeps elsewhere, that frustum.cpp asks for

#pragma section(".clips$a", read, write)
#pragma section(".clips$b", read, write)
#pragma section(".clips$c", read, write)
#pragma section(".clips$d", read, write)

// The linker puts sections with the same name before the $ next to each other, in the order of what comes after it, so each watch comes straight after its list
static int const kWatchedCorners = 128;
__declspec(allocate(".clips$a")) RENDERVERTEX VerticesBuffer[9];
__declspec(allocate(".clips$b")) RENDERVERTEX watchAfterVerticesBuffer[kWatchedCorners];
__declspec(allocate(".clips$c")) RENDERPOLYGON RenderPolygon;
__declspec(allocate(".clips$d")) RENDERVERTEX watchAfterRenderPolygon[kWatchedCorners];

static int logLines = 0;
void LogString(const std::string &logString)
{
	logLines++;
	fprintf(report, "  (the game logs: %s)\n", logString.c_str());
}

// Clipping doesn't use these. Other functions in frustum.cpp do, so they have to exist for the test to link.
void GetMorphDisplay(MORPHDISPLAY *, DISPLAYBLOCK *) {}
int _Dot(VECTORCH *, VECTORCH *) { return 0; }
char *ModuleCurrVisArray = 0;
MORPHDISPLAY MorphDisplay;
DISPLAYBLOCK *Global_ODB_Ptr = 0;
SHAPEHEADER *Global_ShapeHeaderPtr = 0;
VECTORCH *Global_ShapePoints = 0;
int *Global_ShapeNormals = 0;
VECTORCH LocalView;
VECTORCH RotatedPts[1];

// ---- Watching for writes past the end

static unsigned char const kPattern = 0xA5;
static unsigned int const kFieldPattern = 0xA5A5A5A5u;

static void SetTheWatches()
{
	memset(watchAfterVerticesBuffer, kPattern, sizeof(watchAfterVerticesBuffer));
	memset(watchAfterRenderPolygon, kPattern, sizeof(watchAfterRenderPolygon));
	RenderPolygon.MinZ = kFieldPattern;
	RenderPolygon.MaxZ = kFieldPattern;
	RenderPolygon.ImageIndex = (int)kFieldPattern;
}

static bool AllPattern(void const *where, size_t bytes)
{
	unsigned char const *p = (unsigned char const *)where;

	for (size_t i = 0; i < bytes; i++)
	{
		if (p[i] != kPattern)
		{
			return false;
		}
	}

	return true;
}

static bool WatchesUntouched()
{
	return AllPattern(watchAfterVerticesBuffer, sizeof(watchAfterVerticesBuffer)) && AllPattern(watchAfterRenderPolygon, sizeof(watchAfterRenderPolygon))
		&& RenderPolygon.MinZ == kFieldPattern && RenderPolygon.MaxZ == kFieldPattern && RenderPolygon.ImageIndex == (int)kFieldPattern;
}

// ---- How clipping worked before it had a limit. The lists are long enough for whatever comes out, and the rest of it is the game's own Test, Lambda and Output pieces.

#ifdef USE_FRUSTUM_CULL
#define REFERENCE_FIRST_CORNER(Test) Test(curVertexPtr)
#else
#define REFERENCE_FIRST_CORNER(Test) 1
#endif

#define REFERENCE_CUT(name, Test, Lambda, OutputXYZ) \
static int name(RENDERVERTEX *in, int count, RENDERVERTEX *out) \
{ \
	RENDERVERTEX *curVertexPtr = in; \
	RENDERVERTEX *outputVerticesPtr = out; \
	int verticesLeft = count; \
	int numberOfPointsOutputted = 0; \
	int curVertexInside = REFERENCE_FIRST_CORNER(Test); \
 \
	do \
	{ \
		RENDERVERTEX *nextVertexPtr = (!--verticesLeft) ? in : curVertexPtr + 1; \
 \
		if (curVertexInside) \
		{ \
			numberOfPointsOutputted++; \
			*outputVerticesPtr++ = *curVertexPtr; \
		} \
 \
		int nextVertexInside = Test(nextVertexPtr); \
 \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda = Lambda; \
			OutputXYZ \
			Clip_OutputUVI \
			numberOfPointsOutputted++; \
			outputVerticesPtr++; \
		} \
 \
		curVertexPtr = nextVertexPtr; \
		curVertexInside = nextVertexInside; \
	} \
	while (verticesLeft); \
 \
	return numberOfPointsOutputted; \
}

REFERENCE_CUT(ReferenceCutZ, Clip_Z_Test, Clip_Z_Lambda, Clip_Z_OutputXYZ)
REFERENCE_CUT(ReferenceCutNX, Clip_NX_Test, Clip_NX_Lambda, Clip_NX_OutputXYZ)
REFERENCE_CUT(ReferenceCutPY, Clip_PY_Test, Clip_PY_Lambda, Clip_PY_OutputXYZ)
REFERENCE_CUT(ReferenceCutNY, Clip_NY_Test, Clip_NY_Lambda, Clip_NY_OutputXYZ)
REFERENCE_CUT(ReferenceCutPX, Clip_PX_Test, Clip_PX_Lambda, Clip_PX_OutputXYZ)

struct Reference
{
	RENDERVERTEX corners[kWatchedCorners];
	int count;
	int most;
};

// The five cuts in the order the trail makes them, and stopping as it does when there's less than a triangle left. The result ends up in the same list as the game's would.
static void RunReference(RENDERVERTEX const *quad, Reference &result)
{
	typedef int (*Cut)(RENDERVERTEX *, int, RENDERVERTEX *);
	Cut const cuts[5] = { ReferenceCutZ, ReferenceCutNX, ReferenceCutPY, ReferenceCutNY, ReferenceCutPX };
	static RENDERVERTEX a[kWatchedCorners], b[kWatchedCorners];

	memcpy(a, quad, sizeof(RENDERVERTEX) * 4);
	RENDERVERTEX *in = a, *out = b;
	result.count = 4;
	result.most = 4;

	for (int c = 0; c < 5; c++)
	{
		result.count = cuts[c](in, result.count, out);
		result.most = (result.count > result.most) ? result.count : result.most;
		std::swap(in, out);

		if (result.count < 3)
		{
			break;
		}
	}

	memcpy(result.corners, in, sizeof(RENDERVERTEX) * result.count);
}

// ---- The game's clippers, in the same order

struct Clippers
{
	void (*z)(void);
	void (**nx)(void);
	void (**py)(void);
	void (**ny)(void);
	void (**px)(void);
};

static Clippers const kGouraudTextured = { GouraudTexturedPolygon_ClipWithZ, &GouraudTexturedPolygon_ClipWithNegativeX, &GouraudTexturedPolygon_ClipWithPositiveY, &GouraudTexturedPolygon_ClipWithNegativeY, &GouraudTexturedPolygon_ClipWithPositiveX };
static Clippers const kGouraud = { GouraudPolygon_ClipWithZ, &GouraudPolygon_ClipWithNegativeX, &GouraudPolygon_ClipWithPositiveY, &GouraudPolygon_ClipWithNegativeY, &GouraudPolygon_ClipWithPositiveX };
static Clippers const kTextured = { TexturedPolygon_ClipWithZ, &TexturedPolygon_ClipWithNegativeX, &TexturedPolygon_ClipWithPositiveY, &TexturedPolygon_ClipWithNegativeY, &TexturedPolygon_ClipWithPositiveX };

// Gives back the most corners any list held, and where the result is. An odd number of cuts leaves it in RenderPolygon.Vertices, an even number in VerticesBuffer.
static int RunGame(Clippers const &clip, RENDERVERTEX const *quad, RENDERVERTEX **resultList)
{
	memcpy(VerticesBuffer, quad, sizeof(RENDERVERTEX) * 4);
	RenderPolygon.NumberOfVertices = 4;
	int most = 4;
	void (*cuts[5])(void) = { clip.z, *clip.nx, *clip.py, *clip.ny, *clip.px };

	for (int c = 0; c < 5; c++)
	{
		cuts[c]();
		*resultList = (c % 2 == 0) ? RenderPolygon.Vertices : VerticesBuffer;
		most = ((int)RenderPolygon.NumberOfVertices > most) ? (int)RenderPolygon.NumberOfVertices : most;

		if (RenderPolygon.NumberOfVertices < 3)
		{
			break;
		}
	}

	return most;
}

static bool SameCorner(RENDERVERTEX const &a, RENDERVERTEX const &b)
{
	return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.U == b.U && a.V == b.V && a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A
		&& a.SpecularR == b.SpecularR && a.SpecularG == b.SpecularG && a.SpecularB == b.SpecularB;
}

// ---- Made-up polygons. The numbers repeat exactly from run to run, so a failure can be looked at again.

static uint32_t randomState = 12345;

static uint32_t RandomBits()
{
	// xorshift: a few lines anyone can check, and the same on every machine
	randomState ^= randomState << 13;
	randomState ^= randomState >> 17;
	randomState ^= randomState << 5;
	return randomState;
}

static double RandomBetween(double low, double high)
{
	return low + (high - low) * ((RandomBits() >> 8) / 16777216.0);
}

static void FillInTheRest(RENDERVERTEX &corner)
{
	corner.U = (int)(RandomBits() & 0xFFFFF);
	corner.V = (int)(RandomBits() & 0xFFFFF);
	corner.R = (unsigned char)RandomBits();
	corner.G = (unsigned char)RandomBits();
	corner.B = (unsigned char)RandomBits();
	corner.A = (unsigned char)RandomBits();
	corner.SpecularR = (unsigned char)RandomBits();
	corner.SpecularG = (unsigned char)RandomBits();
	corner.SpecularB = (unsigned char)RandomBits();
	corner.Fog = 0;
}

static void SetCorner(RENDERVERTEX &corner, double x, double y, double z)
{
	corner.X = (int)x;
	corner.Y = (int)y;
	corner.Z = (int)z;
	FillInTheRest(corner);
}

enum QuadKind { kFlatNoDent, kTwisted, kDiscTrail, kNumKinds };
static char const *const kKindNames[kNumKinds] = { "flat with no dent", "twisted", "shaped like a disc's trail" };

static void MakeQuad(QuadKind kind, RENDERVERTEX *quad)
{
	if (kind == kTwisted)
	{
		for (int i = 0; i < 4; i++)
		{
			SetCorner(quad[i], RandomBetween(-1000, 1000), RandomBetween(-1000, 1000), RandomBetween(-1000, 1000));
		}
	}
	else if (kind == kFlatNoDent)
	{
		// a parallelogram: a middle, and two sides. Going round it in order, it can't have a dent or a twist.
		double const middle[3] = { RandomBetween(-600, 600), RandomBetween(-600, 600), RandomBetween(-600, 600) };
		double const size = RandomBetween(200, 3000);
		double u[3], v[3];

		for (int i = 0; i < 3; i++)
		{
			u[i] = RandomBetween(-1, 1) * size;
			v[i] = RandomBetween(-1, 1) * size;
		}

		double const signs[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };

		for (int i = 0; i < 4; i++)
		{
			SetCorner(quad[i], middle[0] + u[0] * signs[i][0] + v[0] * signs[i][1], middle[1] + u[1] * signs[i][0] + v[1] * signs[i][1], middle[2] + u[2] * signs[i][0] + v[2] * signs[i][1]);
		}
	}
	else
	{
		// The way RenderTrailSegment builds it: this frame's and last frame's position, each with a line 127 either way, 500 apart. Where it is against the view is made up.
		double const here[3] = { RandomBetween(-300, 300), RandomBetween(-300, 300), RandomBetween(-300, 300) };
		double flight[3] = { RandomBetween(-1, 1), RandomBetween(-1, 1), RandomBetween(-1, 1) };
		double side[3] = { RandomBetween(-1, 1), RandomBetween(-1, 1), RandomBetween(-1, 1) };
		double const length = sqrt(flight[0] * flight[0] + flight[1] * flight[1] + flight[2] * flight[2]) + 0.001;
		double const sideLength = sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]) + 0.001;
		double const size = RandomBetween(0, 127);

		for (int i = 0; i < 3; i++)
		{
			flight[i] /= length;
			side[i] = side[i] / sideLength * size;
		}

		SetCorner(quad[0], here[0] - side[0], here[1] - side[1], here[2] - side[2]);
		SetCorner(quad[1], here[0] + side[0], here[1] + side[1], here[2] + side[2]);
		SetCorner(quad[2], here[0] - flight[0] * 500 + side[0], here[1] - flight[1] * 500 + side[1], here[2] - flight[2] * 500 + side[2]);
		SetCorner(quad[3], here[0] - flight[0] * 500 - side[0], here[1] - flight[1] * 500 - side[1], here[2] - flight[2] * 500 - side[2]);
	}
}

// One found by this test's own search, written out so it can be read. Twisted, close to the view, and it grows past nine by the old way, with or without the switch.
static RENDERVERTEX const kFoundQuad[4] =
{
	{ 168, 881, 949 },
	{ 274, -384, -848 },
	{ -609, -460, 665 },
	{ 308, 5, -463 },
};

// All the checks, run once. The test's main() calls this and gives back what it gives back: 0 if every check passed.
static int RunTheClipChecks()
{
	// The report goes to a copy of the real output, made before ordinary printing is sent nowhere. Each step is checked,
	// and everything opened here is closed again before this ends, whichever way it ends.
	int const realOutput = _dup(_fileno(stdout));
	report = (realOutput == -1) ? NULL : _fdopen(realOutput, "w");

	if (!report)
	{
		if (realOutput != -1)
		{
			_close(realOutput);
		}

		puts("FAILED: the test couldn't keep a copy of its output for the report");
		return 1;
	}

	setvbuf(report, NULL, _IONBF, 0);

	FILE *const nowhere = freopen("NUL", "w", stdout);

	if (!nowhere)
	{
		fprintf(report, "FAILED: the test couldn't send the game's own printing nowhere\n");
		fclose(report);
		return 1;
	}

	fprintf(report, "The test itself:\n");

	char const *const listEnd = (char const *)VerticesBuffer + sizeof(VerticesBuffer);
	char const *const polygonEnd = (char const *)&RenderPolygon + sizeof(RenderPolygon);
	Check((char const *)watchAfterVerticesBuffer >= listEnd && (char const *)watchAfterVerticesBuffer - listEnd < (ptrdiff_t)sizeof(RENDERVERTEX), "the watch starts within one corner of the end of VerticesBuffer");
	Check((char const *)watchAfterRenderPolygon >= polygonEnd && (char const *)watchAfterRenderPolygon - polygonEnd < (ptrdiff_t)sizeof(RENDERVERTEX), "and the other starts within one corner of the end of RenderPolygon");

	SetFrustumType(FRUSTUM_TYPE_NORMAL);

	fprintf(report, "One polygon that grows past nine:\n");

	{
		Reference old;
		RunReference(kFoundQuad, old);
		fprintf(report, "  the old way, it grew to %d corners\n", old.most);
		Check(old.most > maxpolypts, "the old way takes it past the end of the lists");

		SetTheWatches();
		logLines = 0;
		RENDERVERTEX *result = 0;
		int const most = RunGame(kGouraudTextured, kFoundQuad, &result);
		fprintf(report, "  now it reaches %d corners\n", most);
		Check(most <= maxpolypts, "now it stops at nine");
		Check(WatchesUntouched(), "and nothing past the end of either list changed");
		Check(logLines == 1, "and the game's log heard about it");
	}

	fprintf(report, "Made-up polygons, through the five cuts the trail uses:\n");

	for (int kind = 0; kind < kNumKinds; kind++)
	{
		int const tries = 300000;
		int wentPast = 0, sameUpToNine = 0, different = 0, overNine = 0, touched = 0, mostOld = 0, mostNow = 0;
		RENDERVERTEX quad[4];
		logLines = 1; // said once already, above, so anything more is a second line

		for (int i = 0; i < tries; i++)
		{
			MakeQuad((QuadKind)kind, quad);

			static Reference old;
			RunReference(quad, old);
			mostOld = (old.most > mostOld) ? old.most : mostOld;

			SetTheWatches();
			RENDERVERTEX *result = 0;
			int const most = RunGame(kGouraudTextured, quad, &result);
			mostNow = (most > mostNow) ? most : mostNow;
			overNine += (most > maxpolypts) ? 1 : 0;
			touched += WatchesUntouched() ? 0 : 1;

			if (old.most > maxpolypts)
			{
				wentPast++;
			}
			else
			{
				bool same = old.count == (int)RenderPolygon.NumberOfVertices;

				for (int c = 0; same && c < old.count; c++)
				{
					same = SameCorner(old.corners[c], result[c]);
				}

				sameUpToNine += same ? 1 : 0;
				different += same ? 0 : 1;
			}
		}

		fprintf(report, "  %s: %d tries. The old way went past nine %d times, most %d. Now most %d. %d stayed within nine, and %d of those differ.\n", kKindNames[kind], tries, wentPast, mostOld, mostNow, sameUpToNine + different, different);
		Check(overNine == 0, std::string(kKindNames[kind]) + ": no list ever goes past nine");
		Check(touched == 0, std::string(kKindNames[kind]) + ": and nothing past the end of a list ever changes");
		Check(different == 0 && sameUpToNine > 0, std::string(kKindNames[kind]) + ": and wherever the old way stayed within nine, the corners are exactly the same");
		Check(logLines == 1, std::string(kKindNames[kind]) + ": and the game's log didn't hear about it again");

#ifndef USE_FRUSTUM_CULL
		Check(wentPast > 0, std::string(kKindNames[kind]) + ": and with the switch off, these do reach the problem, even this kind");
#else
		if (kind == kTwisted)
		{
			Check(wentPast > 0, std::string(kKindNames[kind]) + ": and with the switch on, these do reach the problem");
		}
#endif
	}

	fprintf(report, "The other clippers, both shapes of view:\n");

	Clippers const others[3] = { kGouraudTextured, kGouraud, kTextured };
	char const *const otherNames[3] = { "gouraud and textured", "gouraud", "textured" };
	FrustumType const views[2] = { FRUSTUM_TYPE_NORMAL, FRUSTUM_TYPE_WIDE };

	for (int view = 0; view < 2; view++)
	{
		SetFrustumType(views[view]);

		for (int c = 0; c < 3; c++)
		{
			int mostNow = 0, touched = 0;
			RENDERVERTEX quad[4];

			for (int i = 0; i < 100000; i++)
			{
				MakeQuad(kTwisted, quad);
				SetTheWatches();
				RENDERVERTEX *result = 0;
				int const most = RunGame(others[c], quad, &result);
				mostNow = (most > mostNow) ? most : mostNow;
				touched += WatchesUntouched() ? 0 : 1;
			}

			Check(mostNow <= maxpolypts && touched == 0, std::string(otherNames[c]) + (view ? ", wide: " : ", normal: ") + "no list goes past nine, and nothing past one changes");
			Check(mostNow == maxpolypts, std::string(otherNames[c]) + (view ? ", wide: " : ", normal: ") + "and they did get as far as nine, so the limit was reached");
		}
	}

	if (checksFailed)
	{
		fprintf(report, "%d checks FAILED\n", checksFailed);
	}
	else
	{
		fprintf(report, "All checks passed\n");
	}

	fclose(nowhere);
	fclose(report);
	return checksFailed ? 1 : 0;
}

#endif
