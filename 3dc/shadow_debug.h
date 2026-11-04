// Tools for finding out what the shadows are doing. Written 2026-11-04 by Melanikus for the AvP port.
// See shadow_debug.cpp for how they work. These are for finding problems. They come out, or go behind a switch, before this is merged.

#ifndef SHADOW_DEBUG_H
#define SHADOW_DEBUG_H

#include <d3d9.h>
#include <string>
#include <vector>

// THE CAPTURE: one press saves the next 16 frames as images, with their numbers. frame is the number the frame has in the log.
void ShadowCapture_Start(int frame);
bool ShadowCapture_IsOn();

// This frame's numbers, as lines of text, for the capture's numbers.txt.
void ShadowCapture_AddNumbers(const std::string &numbers);

// The shadow mask as it is now. It's only a copy, and nothing about the picture changes.
void ShadowCapture_TakeMask(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *mask);

// The end of a frame, once it's all drawn. When the last frame has been taken, this saves everything, and the game stops for a few seconds.
void ShadowCapture_EndFrame(LPDIRECT3DDEVICE9 device, int frame);

// Drops a capture that's under way. The device is about to be reset, and a capture across that would mean nothing.
void ShadowCapture_GiveUp();

// THE FLICKER NUMBER, with -shadowflicker: see flicker_meter.cpp. A picture is a list of samples, taken from every second pixel each way.
enum FlickerPicture
{
	kFlickerMask,      // the lamps' shadows, before softening
	kFlickerScreen,    // the world at the end of the shadow pass, with nothing on top of it yet
	kNumFlickerPictures
};

// How far from the camera each sample is, for the pictures that follow. They all have to be the same size.
void Flicker_SetDistances(const std::vector<float> &distances);

void Flicker_AddPicture(FlickerPicture picture, const std::vector<float> &samples);

// The end of a frame. cameraPlace is three numbers, and cameraAxes is nine: together they say whether the camera moved.
void Flicker_EndFrame(int frame, const float *cameraPlace, const float *cameraAxes);

// Writes what's been counted so far to the log, so that the end of a level isn't lost.
void Flicker_WriteReport();

#endif
