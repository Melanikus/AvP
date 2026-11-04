// Shadow prisms kept from one frame to the next. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHY THIS IS HERE
//
// A shadow is drawn as a set of prisms, one for each polygon, stretching away from the light. Working them out again every frame, for every light,
// is most of what shadows cost. But when a light stays still and a wall stays still, the prism between them never changes. So it's worked out
// once, in the world's numbers, and left on the graphics card, and the card just looks at it from wherever the camera is this frame.
//
// HOW IT WORKS
//
// This file is the bookkeeping. It says what's kept and where. Each thing it keeps is one shape's prisms for one light, and it's found by a key
// holding everything those prisms depend on (ShadowVolumeKey). If anything in the key changes, the key doesn't match and the prisms are made again.
// So nothing here is ever told that something moved. What's left behind is simply never asked for again.
//
// That means the store fills up with old prisms in the end. When it's full, what doesn't fit goes the ordinary way, worked out for that frame only,
// and at the start of a later frame the whole store is emptied and refills with what's really wanted. Emptying has to wait for the start of a frame,
// because a list of what to draw may be pointing into the store during a frame. It also waits at least a second, so a scene with more shadows than the
// store can hold doesn't throw everything out and write it all again every frame.
//
// Only a light that hasn't moved since last frame gets anything kept. A light that moves every frame would fill the store with prisms that are never
// used again. If a key is already there it's used, though, even if the light isn't listed as still: a kept prism is right whenever its key matches.

#include "shadow_volumes.h"

#include <string.h>

// Keys are kept in a table with this many places, and only this many are ever filled. A table that's kept part empty finds things quickly, and
// a search for something that isn't there always ends, because it stops at the first empty place. The first number has to be a power of two.
static const unsigned int kTablePlaces = 1 << 16;
static const int kMostEntries = (1 << 16) / 4 * 3;
static const int kMostRuns = 8192;

struct Entry
{
	ShadowVolumeKey key;
	ShadowVolumeRun run;
	bool used;
};

static Entry table[kTablePlaces];
static int entriesInUse = 0;
static int cornersInUse = 0;

static ShadowVolumeRun runs[kMostRuns];
static int numRuns = 0;

static float lastFrameLights[kShadowVolumeMostLights * kShadowVolumeNumbersEach];
static float thisFrameLights[kShadowVolumeMostLights * kShadowVolumeNumbersEach];
static int lightsLastFrame = 0;
static int lightsThisFrame = 0;

static bool storeIsFull = false;
static int framesSinceEmptied = 0;
static ShadowVolumeCounts counts;

// Turns a key into a place in the table. The same key always gives the same place, and different keys are spread out evenly.
static unsigned int PlaceInTable(const ShadowVolumeKey &key)
{
	unsigned char const *bytes = (unsigned char const *)&key;
	unsigned int number = 2166136261u;

	for (unsigned int i = 0; i < sizeof(key); i++)
	{
		number = (number ^ bytes[i]) * 16777619u;
	}

	return (number ^ (number >> 16)) & (kTablePlaces - 1);
}

void ShadowVolumes_Empty()
{
	memset(table, 0, sizeof(table));
	entriesInUse = 0;
	cornersInUse = 0;
	numRuns = 0;
	storeIsFull = false;
	framesSinceEmptied = 0;
}

void ShadowVolumes_StartFrame(const float *lightNumbers, int numLights)
{
	framesSinceEmptied++;

	if (storeIsFull && framesSinceEmptied > kShadowVolumeFramesBetweenEmptying)
	{
		ShadowVolumes_Empty();
		counts.emptied++;
	}

	// nothing can still be pointing into the store now, and a list from last frame isn't wanted
	numRuns = 0;

	memcpy(lastFrameLights, thisFrameLights, sizeof(lastFrameLights));
	lightsLastFrame = lightsThisFrame;

	// a light past the limit is never counted as still, which only means it isn't kept
	lightsThisFrame = (numLights < 0) ? 0 : (numLights > kShadowVolumeMostLights) ? kShadowVolumeMostLights : numLights;
	if (lightsThisFrame > 0)
	{
		memcpy(thisFrameLights, lightNumbers, lightsThisFrame * kShadowVolumeNumbersEach * sizeof(float));
	}
}

bool ShadowVolumes_LightIsStill(const float *numbers)
{
	for (int light = 0; light < lightsLastFrame; light++)
	{
		if (memcmp(&lastFrameLights[light * kShadowVolumeNumbersEach], numbers, kShadowVolumeNumbersEach * sizeof(float)) == 0)
		{
			return true;
		}
	}

	return false;
}

bool ShadowVolumes_Find(const ShadowVolumeKey &key, ShadowVolumeRun *run)
{
	unsigned int place = PlaceInTable(key);

	while (table[place].used)
	{
		if (memcmp(&table[place].key, &key, sizeof(key)) == 0)
		{
			*run = table[place].run;
			counts.found++;
			return true;
		}

		place = (place + 1) & (kTablePlaces - 1);
	}

	return false;
}

int ShadowVolumes_PlaceFor(int corners)
{
	if (corners < 0 || corners > kShadowVolumeMostCorners - cornersInUse || entriesInUse >= kMostEntries)
	{
		storeIsFull = true;
		counts.refused++;
		return -1;
	}

	return cornersInUse;
}

void ShadowVolumes_Keep(const ShadowVolumeKey &key, int corners)
{
	// the caller has asked PlaceFor already, and this is the same test, because a mistake here would point past the end of the card's list
	if (corners < 0 || corners > kShadowVolumeMostCorners - cornersInUse || entriesInUse >= kMostEntries)
	{
		return;
	}

	unsigned int place = PlaceInTable(key);

	while (table[place].used)
	{
		place = (place + 1) & (kTablePlaces - 1);
	}

	table[place].key = key;
	table[place].run.first = cornersInUse;
	table[place].run.corners = corners;
	table[place].used = true;

	entriesInUse++;
	cornersInUse += corners;
	counts.kept++;
}

void ShadowVolumes_StartRuns()
{
	numRuns = 0;
}

bool ShadowVolumes_AddRun(const ShadowVolumeRun &run)
{
	if (run.corners == 0)
	{
		return true;
	}

	if (numRuns > 0 && runs[numRuns - 1].first + runs[numRuns - 1].corners == run.first)
	{
		runs[numRuns - 1].corners += run.corners;
		return true;
	}

	if (numRuns == kMostRuns)
	{
		return false;
	}

	runs[numRuns++] = run;
	return true;
}

int ShadowVolumes_NumRuns()
{
	return numRuns;
}

const ShadowVolumeRun &ShadowVolumes_Run(int number)
{
	return runs[number];
}

void ShadowVolumes_TakeCounts(ShadowVolumeCounts *taken)
{
	*taken = counts;
	taken->cornersInUse = cornersInUse;
	taken->entriesInUse = entriesInUse;
	memset(&counts, 0, sizeof(counts));
}
