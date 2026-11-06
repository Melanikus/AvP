// The test for 3dc/light_choice.cpp, which works out how much of a light reaches what's on screen. Written 2026-11-06 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no graphics card and none of the game: a made-up room of polygons, and made-up lights.
// The last rows read the game's source, to check it uses these the way the header says it does. What they do on a card needs a run of the game.

#include <math.h>
#include <stdio.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "light_choice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// How much the made-up view magnifies things: 90 degrees across
static float const kAcross = 1.0f;
static float const kUp = 1.0f;

static LightChoiceSamples samples;

static void AddWall(float left, float right, float bottom, float top, float distance)
{
	float const pts[] = { left, bottom, distance, right, bottom, distance, right, top, distance, left, top, distance };
	LightChoice_AddPolygon(&samples, pts, 4, kAcross, kUp);
}

static float const kWholeScreenWall = 2000.0f;

// A wall straight ahead, filling the screen and a bit more
static void StartWithAWallAhead()
{
	LightChoice_Forget(&samples);
	AddWall(-kWholeScreenWall, kWholeScreenWall, -kWholeScreenWall, kWholeScreenWall, kWholeScreenWall);
	LightChoice_KeepTheLargest(&samples);
}

static void WhatAPolygonCounts()
{
	LightChoice_Forget(&samples);
	Check(!LightChoice_HasSamples(samples), "nothing noted means nothing to go by");

	AddWall(-1000.0f, 1000.0f, -1000.0f, 1000.0f, 1000.0f);
	Check(LightChoice_HasSamples(samples), "a polygon on screen is noted");
	Check(samples.count == 1, "as one surface");
	Check(fabsf(samples.list[0].weight - 1.0f) < 0.001f, "one that just fills the view covers all of the screen");
	Check(fabsf(samples.list[0].x) < 0.01f && fabsf(samples.list[0].y) < 0.01f && fabsf(samples.list[0].z - 1000.0f) < 0.01f, "it's judged by its middle");
	Check(samples.list[0].nz < -0.99f, "and it faces the camera");
	Check(fabsf(samples.list[0].spread - 1414.2f) < 1.0f, "and it knows how far it spreads from there");

	LightChoice_Forget(&samples);
	AddWall(-250.0f, 250.0f, -250.0f, 250.0f, 1000.0f);
	Check(fabsf(samples.list[0].weight - 0.0625f) < 0.001f, "a quarter of the width and height is a sixteenth of the screen");

	// The same polygon with its corners going the other way
	LightChoice_Forget(&samples);
	float const backwards[] = { -250.0f, 250.0f, 1000.0f, 250.0f, 250.0f, 1000.0f, 250.0f, -250.0f, 1000.0f, -250.0f, -250.0f, 1000.0f };
	LightChoice_AddPolygon(&samples, backwards, 4, kAcross, kUp);
	Check(samples.count == 1 && fabsf(samples.list[0].weight - 0.0625f) < 0.001f, "it covers the same with its corners going the other way");
	Check(samples.list[0].nz < -0.99f, "and it still faces the camera");

	// A long wall, with its middle well off the screen
	LightChoice_Forget(&samples);
	AddWall(0.0f, 40000.0f, -500.0f, 500.0f, 1000.0f);
	Check(samples.count == 1, "a polygon going off the screen is noted");
	Check(samples.list[0].weight > 0.2f && samples.list[0].weight <= 0.5f, "for about what's on screen");
	Check(samples.list[0].x < 1100.0f, "and is judged by that part, not its own middle, 20 metres off");

	// Off the screen
	LightChoice_Forget(&samples);
	AddWall(3000.0f, 5000.0f, -100.0f, 100.0f, 1000.0f);
	Check(samples.count == 0 && !LightChoice_HasSamples(samples), "a polygon wholly off to one side isn't noted");

	LightChoice_Forget(&samples);
	AddWall(-100.0f, 100.0f, -100.0f, 100.0f, -500.0f);
	Check(samples.count == 0, "nor is one wholly behind the camera");

	// A tall one, the same way up
	LightChoice_Forget(&samples);
	AddWall(-500.0f, 500.0f, 0.0f, 40000.0f, 1000.0f);
	Check(samples.list[0].weight > 0.2f && samples.list[0].weight <= 0.5f, "going off the top counts for what's on screen too");

	// The floor under the camera's feet, going away in front of it and behind it
	LightChoice_Forget(&samples);
	float const floor[] = { -5000.0f, -1700.0f, -5000.0f, 5000.0f, -1700.0f, -5000.0f, 5000.0f, -1700.0f, 5000.0f, -5000.0f, -1700.0f, 5000.0f };
	LightChoice_AddPolygon(&samples, floor, 4, kAcross, kUp);
	Check(samples.count == 1 && samples.list[0].weight > 0.2f && samples.list[0].weight < 0.5f, "a floor going behind the camera counts for its part on screen");
	Check(samples.list[0].z > 1000.0f, "and is judged by a spot on that part, not under the camera");
	Check(samples.list[0].ny > 0.99f, "and it faces up, towards the camera");

	// Round the camera: corners in front and behind
	LightChoice_Forget(&samples);
	float const beside[] = { 1000.0f, -1000.0f, -1000.0f, 1000.0f, -1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f, -1000.0f };
	LightChoice_AddPolygon(&samples, beside, 4, kAcross, kUp);
	Check(samples.count <= 1, "a wall beside the camera, part in front and part behind, doesn't upset it");
}

static void BadNumbers()
{
	float const nan = std::numeric_limits<float>::quiet_NaN();
	float const huge = std::numeric_limits<float>::infinity();

	LightChoice_Forget(&samples);

	float const good[] = { -100.0f, -100.0f, 500.0f, 100.0f, -100.0f, 500.0f, 100.0f, 100.0f, 500.0f, -100.0f, 100.0f, 500.0f };

	LightChoice_AddPolygon(&samples, good, 2, kAcross, kUp);
	Check(samples.count == 0, "a polygon with 2 corners is refused");

	// A real polygon going round the middle of the screen, so that only its number of corners can refuse it
	float many[(kLightChoiceMostCorners + 1) * 3];

	for (int i = 0; i <= kLightChoiceMostCorners; i++)
	{
		many[i * 3 + 0] = 100.0f * (float)cos((float)i * 0.3f);
		many[i * 3 + 1] = 100.0f * (float)sin((float)i * 0.3f);
		many[i * 3 + 2] = 500.0f;
	}

	LightChoice_AddPolygon(&samples, many, kLightChoiceMostCorners, kAcross, kUp);
	Check(samples.count == 1, "one with as many corners as there's room for is noted");
	LightChoice_Forget(&samples);
	LightChoice_AddPolygon(&samples, many, kLightChoiceMostCorners + 1, kAcross, kUp);
	Check(samples.count == 0, "and one with more corners than there's room for");
	LightChoice_AddPolygon(&samples, good, -1, kAcross, kUp);
	Check(samples.count == 0, "and a count below nothing");

	float const notNumbers[] = { nan, nan, nan, nan, nan, nan, nan, nan, nan, nan, nan, nan };
	LightChoice_AddPolygon(&samples, notNumbers, 4, kAcross, kUp);
	Check(samples.count == 0 && !LightChoice_HasSamples(samples), "corners that aren't numbers aren't noted");

	float const oneBad[] = { -100.0f, -100.0f, 500.0f, nan, nan, nan, 100.0f, 100.0f, 500.0f, -100.0f, 100.0f, 500.0f };
	LightChoice_AddPolygon(&samples, oneBad, 4, kAcross, kUp);
	Check(samples.count == 0, "and one bad corner refuses the polygon");

	float const infinite[] = { -huge, -huge, huge, huge, -huge, huge, huge, huge, huge, -huge, huge, huge };
	LightChoice_AddPolygon(&samples, infinite, 4, kAcross, kUp);
	Check(samples.totalWeight == samples.totalWeight, "corners at infinity don't spoil the total");

	StartWithAWallAhead();
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, 0.0f, 1.0f, true) == 0.0f, "a light with no range scores nothing");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, -5.0f, 1.0f, true) == 0.0f, "nor one with a range below nothing, which would divide the wrong way");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, nan, 1.0f, true) == 0.0f, "nor one whose range isn't a number");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, 5000.0f, 0.0f, true) == 0.0f, "nor one with no brightness");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, 5000.0f, -1.0f, true) == 0.0f, "nor one with a brightness below nothing");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, 5000.0f, nan, true) == 0.0f, "nor one whose brightness isn't a number");
	Check(LightChoice_Score(samples, nan, nan, nan, 5000.0f, 1.0f, true) == 0.0f, "nor one whose place isn't a number");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, huge, 1.0f, true) > 0.0f, "a range of infinity is held to the most a light can reach");
}

static void TheListStaysItsSize()
{
	LightChoice_Forget(&samples);

	// Many more than it can hold, growing in size so that the largest arrive last
	bool stayedInside = true;

	for (int i = 0; i < 20000; i++)
	{
		float const size = 1.0f + (float)i * 0.01f;
		AddWall(-size, size, -size, size, 1000.0f);
		stayedInside = stayedInside && samples.count <= kLightChoiceSamplesHeld;
	}

	Check(stayedInside, "the list never holds more than it has room for");

	LightChoice_KeepTheLargest(&samples);
	Check(samples.count == kLightChoiceSamplesKept, "and comes down to what's kept");

	float smallest = 1.0f;

	for (int i = 0; i < samples.count; i++)
	{
		smallest = (samples.list[i].weight < smallest) ? samples.list[i].weight : smallest;
	}

	float const size = 1.0f + 19999.0f * 0.01f;
	float const largest = (size * 2.0f / 1000.0f) * (size * 2.0f / 1000.0f) / 4.0f;
	float const lastKept = (1.0f + (float)(20000 - kLightChoiceSamplesKept) * 0.01f);

	Check(smallest >= (lastKept * 2.0f / 1000.0f) * (lastKept * 2.0f / 1000.0f) / 4.0f * 0.99f, "and what's kept is the largest of them");
	Check(smallest < largest, "not just the very largest");

	float const total = samples.totalWeight;
	AddWall(-20.0f, 20.0f, -20.0f, 20.0f, 1000.0f);
	Check(samples.count == kLightChoiceSamplesKept, "something too small to keep isn't kept");
	Check(samples.totalWeight > total, "but still counts towards what's on screen");

	LightChoice_Forget(&samples);
	Check(samples.count == 0 && samples.totalWeight == 0.0f && samples.leastWorthKeeping == 0.0f, "and forgetting starts again from nothing");
}

// LightShape in shaders/lightShapes.fxh, as it's written there, for comparing
static float ShaderShape(float const *light, float const *position, float const *normal, float range, bool baked)
{
	float const toX = light[0] - position[0], toY = light[1] - position[1], toZ = light[2] - position[2];
	float const howFar = sqrtf(toX * toX + toY * toY + toZ * toZ);
	float const reach = (howFar / range > 1.0f) ? 1.0f : howFar / range;
	float const divide = (howFar > 1.0f) ? howFar : 1.0f;
	float const turned = (normal[0] * toX + normal[1] * toY + normal[2] * toZ) / divide;
	float const facing = (turned < 0.0f) ? 0.0f : (turned > 1.0f) ? 1.0f : turned;

	if (baked)
	{
		return (float)cos(1.5707963f * reach) * facing;
	}

	return (1.0f - reach) * ((turned > 0.0f) ? (turned + 0.25f) * 0.5f : 0.0f);
}

static void TheSameSumAsTheShaders()
{
	LightChoiceSample sample = {};
	sample.z = 1000.0f;
	sample.nz = -1.0f;
	sample.weight = 1.0f;
	sample.spread = 0.0f;

	float const position[] = { 0.0f, 0.0f, 1000.0f };
	float const normal[] = { 0.0f, 0.0f, -1.0f };
	float const lights[][3] = { { 0.0f, 0.0f, 0.0f }, { 300.0f, -200.0f, 500.0f }, { 1200.0f, 0.0f, 900.0f }, { 0.0f, 0.0f, 950.0f }, { 2000.0f, 2000.0f, 10.0f } };

	for (float const *light : lights)
	{
		for (float const range : { 800.0f, 2500.0f, 9000.0f })
		{
			float const baked = LightChoice_LightOnOneSurface(sample, light[0], light[1], light[2], range, true);
			float const live = LightChoice_LightOnOneSurface(sample, light[0], light[1], light[2], range, false);
			Check(fabsf(baked - ShaderShape(light, position, normal, range, true)) < 0.0001f, "a baked light gets the same as in the shader");
			Check(fabsf(live - ShaderShape(light, position, normal, range, false)) < 0.0001f, "and a live one");
		}
	}

	Check(LightChoice_LightOnOneSurface(sample, 0.0f, 0.0f, 1000.0f, 5000.0f, true) == 0.0f, "a light exactly on the spot doesn't divide by nothing");
	Check(LightChoice_LightOnOneSurface(sample, 0.0f, 0.0f, 2000.0f, 5000.0f, true) == 0.0f, "a baked light behind a surface gives it nothing");
	Check(LightChoice_LightOnOneSurface(sample, 0.0f, 0.0f, 2000.0f, 5000.0f, false) == 0.0f, "nor does a live one");

	// A big polygon: part of it can be in reach when its spot isn't
	sample.spread = 5000.0f;
	Check(LightChoice_LightOnOneSurface(sample, 0.0f, 3000.0f, 800.0f, 2000.0f, false) > 0.0f, "a light over the near end of a big polygon counts, with the spot it's judged by out of reach");
	sample.spread = 0.0f;
	Check(LightChoice_LightOnOneSurface(sample, 0.0f, 3000.0f, 800.0f, 2000.0f, false) == 0.0f, "which a small one wouldn't");
}

static void WhatAScoreMeans()
{
	StartWithAWallAhead();

	float const closeBy = LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, 5000.0f, 1.0f, true);
	float const dim = LightChoice_Score(samples, 0.0f, 0.0f, 1000.0f, 5000.0f, 0.5f, true);
	float const further = LightChoice_Score(samples, 0.0f, 0.0f, -2000.0f, 5000.0f, 1.0f, true);
	float const behind = LightChoice_Score(samples, 0.0f, 0.0f, 3000.0f, 5000.0f, 1.0f, true);
	float const outOfReach = LightChoice_Score(samples, 0.0f, 0.0f, -90000.0f, 1000.0f, 1.0f, true);

	Check(closeBy > 0.0f, "a light in front of a wall on screen scores");
	Check(fabsf(dim - closeBy * 0.5f) < 0.0001f, "half as bright scores half");
	Check(further < closeBy, "further away scores less");
	Check(behind == 0.0f, "behind the wall, on its dark side, scores nothing");
	Check(outOfReach < kLightChoiceShowsNothing, "and so does a light that can't reach it");

	// Range alone earns nothing: a floating light with no surface in reach, however far its range goes
	LightChoice_Forget(&samples);
	AddWall(-50.0f, 50.0f, -50.0f, 50.0f, 500.0f);
	LightChoice_KeepTheLargest(&samples);

	float const reachingFar = LightChoice_Score(samples, 90000.0f, 0.0f, 500.0f, 5000.0f, 1.0f, true);
	Check(reachingFar == 0.0f, "a light with no surface in reach scores nothing");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, 400.0f, 90000.0f, 1.0f, true) > LightChoice_Score(samples, 0.0f, 0.0f, 400.0f, 800.0f, 1.0f, true) * 0.9f, "and a long range is only worth what it lights");

	// The most a light counts as reaching. A small wall, a long way off, so that how far it reaches makes a difference.
	LightChoice_Forget(&samples);
	AddWall(-50.0f, 50.0f, -50.0f, 50.0f, 500.0f);
	LightChoice_KeepTheLargest(&samples);
	Check(LightChoice_Score(samples, 0.0f, 0.0f, -50000.0f, 400000.0f, 1.0f, true) == LightChoice_Score(samples, 0.0f, 0.0f, -50000.0f, kLightChoiceMostRange, 1.0f, true), "a range past the most counts as the most");
	Check(LightChoice_Score(samples, 0.0f, 0.0f, -50000.0f, kLightChoiceMostRange, 1.0f, true) < LightChoice_Score(samples, 0.0f, 0.0f, -50000.0f, kLightChoiceMostRange * 0.99f, 1.0f, true) + 0.01f, "which is still a real distance");

	// Over the whole of what's on screen, so a light on all of it gets what it gives each part, whatever size the screen's surfaces are
	LightChoice_Forget(&samples);
	AddWall(-250.0f, 250.0f, -250.0f, 250.0f, 1000.0f);
	LightChoice_KeepTheLargest(&samples);
	Check(fabsf(LightChoice_Score(samples, 0.0f, 0.0f, 500.0f, 5000.0f, 0.5f, true) - 0.5f * LightChoice_LightOnOneSurface(samples.list[0], 0.0f, 0.0f, 500.0f, 5000.0f, true)) < 0.0001f, "a score is a share of what's on screen, not of the whole screen");

	// How much of the screen each surface stands for
	LightChoice_Forget(&samples);
	AddWall(-1000.0f, 0.0f, -1000.0f, 1000.0f, 1000.0f);
	AddWall(0.0f, 100.0f, -100.0f, 100.0f, 1000.0f);
	LightChoice_KeepTheLargest(&samples);

	float const onTheBigWall = LightChoice_Score(samples, -500.0f, 0.0f, 800.0f, 1000.0f, 1.0f, true);
	float const onTheSmallWall = LightChoice_Score(samples, 2000.0f, 0.0f, 800.0f, 1000.0f, 1.0f, true);
	Check(onTheBigWall > onTheSmallWall, "a light on most of the screen beats a light on a little of it");
}

static LightChoiceCandidate Candidate(float score, bool comesFirst = false, bool hasAPlace = false, bool keepsItsPlace = false)
{
	LightChoiceCandidate c;
	c.score = score;
	c.comesFirst = comesFirst;
	c.hasAPlace = hasAPlace;
	c.keepsItsPlace = keepsItsPlace;
	return c;
}

static void WhoGetsAPlace()
{
	int picked[kLightChoiceMostCandidates];
	int nothing = -1;

	{
		LightChoiceCandidate const c[] = { Candidate(0.1f), Candidate(0.5f), Candidate(0.3f) };
		int const n = LightChoice_Pick(c, 3, 2, picked, &nothing);
		Check(n == 2 && picked[0] == 1 && picked[1] == 2, "the most light first, up to the limit");
		Check(nothing == 0, "and one passed over for want of a place isn't one lighting nothing");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.9f), Candidate(0.0f, true), Candidate(0.5f) };
		int const n = LightChoice_Pick(c, 3, 2, picked, &nothing);
		Check(n == 2 && picked[0] == 1 && picked[1] == 0, "a flare or the player's own light comes ahead of any score, even with none");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.0f, true), Candidate(0.0f, true), Candidate(0.9f) };
		int const n = LightChoice_Pick(c, 3, 1, picked, &nothing);
		Check(n == 1 && picked[0] == 0, "and fills the list first, nearest first");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.4f), Candidate(0.5f, false, true) };
		int const n = LightChoice_Pick(c, 2, 1, picked, &nothing);
		Check(n == 1 && picked[0] == 1, "a light with a place keeps it against a slightly better light");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.5f), Candidate(0.3f, false, true) };
		int const n = LightChoice_Pick(c, 2, 1, picked, &nothing);
		Check(n == 1 && picked[0] == 0, "and loses it to one that's clearly better");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.45f), Candidate(0.36f, false, true) };
		int const n = LightChoice_Pick(c, 2, 1, picked, &nothing);
		Check(n == 1 && picked[0] == 1, "within a quarter counts as close enough");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.0f), Candidate(kLightChoiceShowsNothing * 0.5f), Candidate(kLightChoiceShowsNothing) };
		int const n = LightChoice_Pick(c, 3, 8, picked, &nothing);
		Check(n == 1 && picked[0] == 2, "a score of about nothing takes no place, with places to spare");
		Check(nothing == 2, "and is counted as lighting nothing");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(kLightChoiceShowsNothing * 0.9f, false, true) };
		int const n = LightChoice_Pick(c, 1, 8, picked, &nothing);
		Check(n == 1, "a light with a place stays a little past that, so it doesn't flip at the line");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.0f, false, true, true), Candidate(0.0f, false, true, false) };
		int const n = LightChoice_Pick(c, 2, 8, picked, &nothing);
		Check(n == 1 && picked[0] == 0, "a lamp dark for the moment keeps its place, and any other at nothing doesn't");
		Check(nothing == 1, "and only the other one counts as lighting nothing");
	}
	{
		LightChoiceCandidate const c[] = { Candidate(0.5f), Candidate(0.5f) };
		int const n = LightChoice_Pick(c, 2, 1, picked, &nothing);
		Check(n == 1 && picked[0] == 0, "on a tie, the one that came first, which is the nearest");
	}

	// The ends of the numbers
	{
		LightChoiceCandidate const c[] = { Candidate(0.5f) };
		Check(LightChoice_Pick(c, 1, 0, picked, &nothing) == 0, "no room means no places");
		Check(LightChoice_Pick(c, 1, -3, picked, &nothing) == 0, "and so does room below nothing");
		Check(LightChoice_Pick(c, 0, 8, picked, &nothing) == 0 && nothing == 0, "nothing up for a place means none taken");
		Check(LightChoice_Pick(c, -2, 8, picked, &nothing) == 0, "and so does a count below nothing");
		Check(LightChoice_Pick(c, 1, 8, picked, 0) == 1, "and it doesn't have to be told how many light nothing");
	}
	{
		std::vector<LightChoiceCandidate> many(kLightChoiceMostCandidates + 20, Candidate(0.5f));
		Check(LightChoice_Pick(&many[0], (int)many.size(), kLightChoiceMostCandidates, picked, &nothing) == kLightChoiceMostCandidates, "more up for a place than it can look at stops at what it can");
		many[kLightChoiceMostCandidates + 5].score = 9.0f;
		Check(LightChoice_Pick(&many[0], (int)many.size(), 1, picked, &nothing) == 1 && picked[0] == 0, "and never looks past that, however good");
	}
}

static std::string ReadSourceFile(std::string const &name)
{
	std::string const path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	std::string all = text.str();
	std::string out;
	out.reserve(all.size());

	for (char const c : all)
	{
		if (c != '\r')
		{
			out.push_back(c);
		}
	}

	return out;
}

static bool Has(std::string const &text, char const *wanted)
{
	return text.find(wanted) != std::string::npos;
}

static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return std::string();
	}

	std::string::size_type const end = text.find(to, start);

	return (end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static void TheRealGame()
{
	std::string const shapes = ReadSourceFile("3dc/kshape.cpp");
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const shader = ReadSourceFile("shaders/lightShapes.fxh");
	std::string const limits = ReadSourceFile("3dc/d3d_render.h");
	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj");

	// If the shader's sums change, the score has to change with them
	Check(Has(shader, "return cos(1.5707963f * reach) * saturate(turned);"), "the shader still has the baked sum that LightChoice_LightOnOneSurface copies");
	Check(Has(shader, "return (1.0f - reach) * ((turned > 0.0f) ? slant : (turned == 0.0f) ? 0.125f : 0.0f);"), "and the live one");
	Check(Has(shader, "float  slant   = (turned + 0.25f) * 0.5f;"), "and its slant");
	Check(Has(limits, "kToyMostRange = 100000"), "the most a light can reach is still what kLightChoiceMostRange says");
	Check(Has(render, "kToyMostPolyPoints  = 16;"), "and the most corners on a polygon");

	// What's noted
	std::string const adding = Between(render, "void ToyShadows_AddCaster(", "int const firstPoint = toyNumCasterPoints;");

	Check(Has(adding, "if (kind != kToyCasterCastOnly)\n\t{\n\t\tLightChoice_AddPolygon(&lightChoiceSamples, pts, count, d3d.matProjection._11, d3d.matProjection._22);"), "every polygon on its way to the screen is noted, and none that's only there to cast");
	Check(adding.find("LightChoice_AddPolygon") > adding.find("if (toyNumCasters == kToyMaxCasters"), "after the limit on polygons, so a frame can't cost more than that many");

	// What's scored
	std::string const choosing = Between(shapes, "void ToyShadows_ChooseLights()", "int placeOf[kToyMostLights];");

	Check(Has(choosing, "memcpy(samplesViewMatrix, toyViewMatrix, sizeof(samplesViewMatrix));\n\tmemcpy(toyViewMatrix, ViewMatrix, sizeof(toyViewMatrix));"), "the samples are from the frame before, so the view of that frame is kept for them");
	Check(Has(choosing, "numSteady = ToyKeepTheLightsThatShowMost(steady, numSteady, mostSteady, &lightsThisFrame.steadyInReach);"), "the steady lights are chosen by score");
	Check(Has(choosing, "numBrief = ToyKeepTheLightsThatShowMost(brief, numBrief, mostBrief, &lightsThisFrame.briefInReach);"), "and the brief lights");
	Check(Has(choosing, "ToyFoundLight steady[kLightChoiceMostCandidates];") && Has(choosing, "ToyFoundLight brief[kLightChoiceMostCandidates];"), "from a list with room for all it can score");
	Check(Has(choosing, "LightChoice_KeepTheLargest(&lightChoiceSamples);\n\tnumSteady = "), "only the largest samples are scored, which is what holds the cost down");
	Check(Has(Between(choosing, "lightsThisFrame.briefChosen = numBrief;", "ToyFoundLight chosen[kToyMostLights];"), "LightChoice_Forget(&lightChoiceSamples);"), "and the samples start again for the frame about to be drawn");
	Check(Has(Between(choosing, "if (!ToyShadowsMode || CurrentVisionMode != VISION_MODE_NORMAL)", "return;"), "LightChoice_Forget(&lightChoiceSamples);"), "and when nothing's chosen, so old ones can't meet a newer view");
	Check(Has(ReadSourceFile("3dc/avp/win95/winmain.cpp"), "LightChoice_Forget(&lightChoiceSamples);\n\t\tStaticLevel_BuildThisLevel();"), "a new level starts with nothing to go by");

	std::string const keeping = Between(shapes, "static int ToyKeepTheLightsThatShowMost(", "// The lights chosen for this frame");

	Check(Has(keeping, "if (!LightChoice_HasSamples(lightChoiceSamples) || StaticSceneOn)"), "with nothing to go by, or walls that were never noted, it goes by nearest");
	Check(Has(keeping, "return (numFound < most) ? numFound : most;"), "keeping the list as it came");
	Check(Has(keeping, "TranslatePoint(source, view, samplesViewMatrix);"), "a light is looked at from where the samples were seen");
	Check(Has(keeping, "-view[1]"), "with y the way up the samples have it");
	Check(Has(keeping, "candidates[i].comesFirst = found[i].comesFirst;"), "a flare or the player's own light comes first");
	Check(Has(keeping, "candidates[i].keepsItsPlace = hasAPlace && LightIsDarkForNow(light);"), "and a dark lamp with a place keeps it");
	Check(Has(keeping, "(*inReach) -= showNothing;") && Has(keeping, "lightsThisFrame.lightNothing += showNothing;"), "and what lights nothing is counted as that, and not as finding no place left");

	std::string const considering = Between(shapes, "static void ToyConsiderLight(", "// A fixed point number as a fraction");

	Check(Has(considering, "found[place].comesFirst = block == Player || (block && ToyBlockIsFlare(block));"), "what comes first is the player's own lights and flares");

	// The strength
	std::string const strength = Between(shapes, "static float ToyLightStrength(", "static int ToyKeepTheLightsThatShowMost(");

	Check(Has(strength, "light->LightBright ? light->LightBright : light->LightBrightStore"), "a dark lamp is judged by how bright it'll be when it comes back");

	// The game is built with it
	Check(Has(project, "..\\..\\3dc\\light_choice.cpp"), "the game is built with it");
}

int main()
{
	WhatAPolygonCounts();
	BadNumbers();
	TheListStaysItsSize();
	TheSameSumAsTheShaders();
	WhatAScoreMeans();
	WhoGetsAPlace();
	TheRealGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all light choice checks passed\n");
	return 0;
}
