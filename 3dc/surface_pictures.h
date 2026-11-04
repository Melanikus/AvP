// One drawing of the surfaces each frame, for every light to read. Written 2026-11-04 by Melanikus for the AvP port.
// See surface_pictures.cpp for how it works.

#ifndef SURFACE_PICTURES_H
#define SURFACE_PICTURES_H

#include <d3d9.h>

// Whether the pictures exist, making them the first time it's asked. False on a card that can't do this, and then no light may rely on them.
bool SurfacePictures_Ready(LPDIRECT3DDEVICE9 device);

// Gives the pictures back. The graphics card holds them, so they go whenever the device resets, and are made again after it.
void SurfacePictures_Release();

// Draws the frame's solid polygons into the pictures. depthPicture is where each pixel's distance goes, and it has already been cleared.
// worldAxes holds the world's three directions in the light list's space, three numbers each.
// The caller has set the depth test and the rest of the device up. Says false if nothing was drawn.
bool SurfacePictures_Draw(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *depthPicture, const float *worldAxes);

// The pictures themselves, for a shader to read. Which way each pixel faces, and its texture's colour.
IDirect3DTexture9 *SurfacePictures_Normal();
IDirect3DTexture9 *SurfacePictures_Colour();

#endif
