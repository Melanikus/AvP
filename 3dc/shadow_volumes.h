// Shadow prisms kept from one frame to the next. Written 2026-11-04 by Melanikus for the AvP port.
// See shadow_volumes.cpp for how it works. This file knows nothing about the game or the graphics card, so a test can check it on its own.

#ifndef SHADOW_VOLUMES_H
#define SHADOW_VOLUMES_H

#include <stdint.h>

// What a kept shadow depends on. If all of this is the same, the prisms are the same, wherever the camera is. All in the world's numbers.
struct ShadowVolumeKey
{
	float castFrom[3];     // where this shadow is cast from, which can be one of a lamp's positions
	float lightPlace[3];   // where the light really is
	float range;           // how far the light reaches
	float lightKind;       // the light's kind, because flares don't cast from polygons they're touching
	uint32_t listNumber;   // which shape: see sh_surfacelistnumber
	int32_t place[3];      // where that shape is
};

// A stretch of the kept corners, in the order they're drawn. Every three corners make a triangle.
struct ShadowVolumeRun
{
	int first;
	int corners;
};

enum
{
	kShadowVolumeMostCorners = 1200000,    // how many corners can be kept, which is what the graphics card's list has to hold
	kShadowVolumeNumbersEach = 5,          // what's compared to see if a light has moved: x y z in the world, range, kind
	kShadowVolumeMostLights = 32,
	kShadowVolumeFramesBetweenEmptying = 60
};

// Forgets everything, as when the graphics card's list is lost.
void ShadowVolumes_Empty();

// Starts a frame. lightNumbers holds kShadowVolumeNumbersEach numbers for each light on the list this frame.
void ShadowVolumes_StartFrame(const float *lightNumbers, int numLights);

// Was this light here last frame, in the same place? numbers are its kShadowVolumeNumbersEach numbers.
bool ShadowVolumes_LightIsStill(const float *numbers);

// Finds what's kept for this key. false means nothing is. A run with no corners is a real answer: nothing casts a shadow from this light.
bool ShadowVolumes_Find(const ShadowVolumeKey &key, ShadowVolumeRun *run);

// Where this many new corners would go, or -1 if there's no room for them.
int ShadowVolumes_PlaceFor(int corners);

// Records that this many corners have been written at the place PlaceFor gave.
void ShadowVolumes_Keep(const ShadowVolumeKey &key, int corners);

// Starts a new list of what to draw, and adds to it. A run that starts where the last one ended joins it, so one draw covers both.
// false means the list is full and this run isn't on it.
void ShadowVolumes_StartRuns();
bool ShadowVolumes_AddRun(const ShadowVolumeRun &run);
int ShadowVolumes_NumRuns();
const ShadowVolumeRun &ShadowVolumes_Run(int number);

// What the store has been doing, for the log.
struct ShadowVolumeCounts
{
	int found;            // since the last report
	int kept;             // since the last report
	int refused;          // since the last report: no room, so it went the ordinary way
	int emptied;          // since the last report
	int cornersInUse;
	int entriesInUse;
};
void ShadowVolumes_TakeCounts(ShadowVolumeCounts *counts);

#endif
