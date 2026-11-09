// What Hudson can tell by reading the pictures a frame was drawn from. Written 2026-11-05 by Melanikus for the AvP port.
//
// Each frame, the game draws its solid polygons once into three pictures the size of the screen: how far away each pixel is, which
// way it faces, and what colour it is (surface_pictures.cpp). With vision=check, Hudson reads those three once a view and counts
// what's missing, so that nobody has to look.
//
// A finding is a count, the box round it on the screen, and the first pixel of it. hudson.cpp then sends a ray through that pixel, to
// say which polygon it is.
//
// The counting is in this file, on plain arrays, with nothing from the game and no graphics card, so that a test can hand it pictures
// made wrong on purpose. A finding nobody has seen fire proves nothing.
#ifndef HUDSON_VISION_H
#define HUDSON_VISION_H

#include <stddef.h>
#include <stdint.h>

#include "static_frame.h"

enum
{
	kHudsonVisionMostSamples = 32,    // how many pixels of nothing get a ray sent through them, each view. A ray isn't free
	kHudsonVisionMostChecks = 8       // how many pixels with a surface get a ray too, to check rays against the distance picture
};

// A picture's distance reads as this where nothing was drawn, because the game clears it with all ones, and on a picture holding
// floats the card takes that as 1. Real distances are in the game's own units, thousands of them, so nothing real is exactly this.
float const kHudsonVisionClearedDistance = 1.0f;

// Some kind of pixel, wherever it was found.
struct HudsonVisionFinding
{
	int pixels;
	int left, top, right, bottom;     // the box round them, inclusive. All 0 if there are none. Pixels are looked at from the top down, so the first is always the top
	int firstX, firstY;               // the first, reading across and down
};

struct HudsonVisionCounts
{
	int pixelsLookedAt;
	int surfaces;                     // pixels where a surface was drawn
	float nearest, furthest;          // how far away the surfaces are, not counting any with no distance. Both 0 if there are none
	HudsonVisionFinding noNormal;     // a surface with colour, and no way it faces. That's never right: see below
	HudsonVisionFinding noDistance;   // a surface with colour, and no distance
	HudsonVisionFinding nothingDrawn; // no surface here. That can be right: it can be sky, or a gap in a grate. A ray has to say
	int samples;                      // some of the pixels of nothing, spread out evenly over them all
	int sampleX[kHudsonVisionMostSamples];
	int sampleY[kHudsonVisionMostSamples];
	int checks;                       // some of the pixels with a surface and a distance, spread out evenly over them all
	int checkX[kHudsonVisionMostChecks];
	int checkY[kHudsonVisionMostChecks];
	float checkDistance[kHudsonVisionMostChecks];
};

// What the frame's shadow pass did, for the one shadow finding that needs no working out: if the shadows are on, lights were chosen, and there are polygons that could cast a shadow, then no corners sent to the card at
// all is a fault. It says nothing about any one light, because the counts are for the whole frame.
struct HudsonVisionShadows
{
	bool volumesOn;
	int lights;
	int casters;
	int corners;
};

bool HudsonVision_ShadowsAreMissing(const HudsonVisionShadows &shadows);

// Whether a ray and the distance picture agree about a pixel. The picture holds how far away a surface is straight ahead of the camera, and a ray says how far along itself it went, so one has to be turned into the other
// by how much the ray points ahead. The ray stops at the polygon, and the picture is rounded, so they're allowed 3% or 20 units, whichever is more. direction has a length of 1, and view is as for HudsonVision_RayThrough.
// How far straight ahead a ray of this length goes, which is what the distance picture holds. view is the world-to-view turn, whose third column is the way the camera looks.
double HudsonVision_HowFarAhead(int rayDistance, const float *direction, const float *view);

// Did a ray going this way reach a polygon from behind? plane starts with an arrow out of the polygon's back, the way a shape keeps it, so a ray that meets it from the front is heading the same way as the arrow.
bool HudsonVision_SeenFromBehind(const float *direction, const float *plane);

bool HudsonVision_RayAgrees(float pictureDistance, int rayDistance, const float *direction, const float *view);

// A pixel on the finished screen, against the eight round it, to tell whether a pixel missing from the surface pictures is missing from the screen as well. Red, green and blue.
struct HudsonVisionScreenPixel
{
	int x, y;
	int colour[3];
	int around[3];         // the average of those round it that are on the screen
	bool differs;          // see below
	bool tooDark;          // it and those round it are all so dark that nothing can be said either way
};

enum
{
	kHudsonVisionScreenDiffers = 48,  // a crack in the game's own drawing shows what's behind it. Lighting that differs a little doesn't count
	kHudsonVisionScreenTooDark = 16   // below this in every colour, on the pixel and those round it, a difference could be rounding
};

// A pixel differs if it's more than kHudsonVisionScreenDiffers from those round it in any colour, or if it's a quarter as bright or less, because in a dark room a crack can't be that far away from anything. The game clears the
// screen to black, so a crack shows black.

// bits is the screen in memory at four bytes a pixel: blue, green, red and one unused. False if the pixel isn't on it.
bool HudsonVision_ScreenAt(const uint8_t *bits, uint32_t pitch, int width, int height, int x, int y, HudsonVisionScreenPixel *pixel);

// Looks at every pixel. normalIsTenBit says the normal picture holds ten bits for each direction, and not eight.
//
// A normal of nothing is always a fault, because the shader writes a direction as (direction * 0.5 + 0.5). All zeros would mean a
// direction of (-1, -1, -1), and that isn't one, because its length isn't 1.
void HudsonVision_Count(const SurfacePicturesInMemory &pictures, uint32_t width, uint32_t height, bool normalIsTenBit, HudsonVisionCounts *counts);

// Which way in the world a pixel looks, from the frame's view and the two numbers of its projection that say how wide it sees. view is
// three rows of four: across, up and forward, each as the way it points in the world. False if the numbers can't be used, or the pixel
// isn't on the screen.
bool HudsonVision_RayThrough(int x, int y, int width, int height, float projection11, float projection22, const float *view, float *direction);

// A number worked out from a file's bytes (FNV-1a, 32 bits), so that the log can show which file it really was. It isn't for security: it's for comparing with what a person put there, and tools can work out the same number.
uint32_t HudsonVision_Check(const uint8_t *bytes, size_t size);

// What's needed to send a ray through a pixel of this frame. In d3d_render.cpp, with the card's pictures.
struct HudsonVisionView
{
	int width, height;
	float projection11, projection22;
	float view[12];
	HudsonVisionShadows shadows;
	int screenPixels;      // the pixels of nothing that were found on the screen. 0 if the screen couldn't be read
	HudsonVisionScreenPixel screen[kHudsonVisionMostSamples];
};

// Reads this frame's three pictures and counts them. False if they can't be read, which the caller says in the log.
bool SurfacePictures_LookAtThisFrame(HudsonVisionCounts *counts, HudsonVisionView *view);

#endif
