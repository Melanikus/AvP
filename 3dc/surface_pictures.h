// One drawing of the surfaces each frame, for every light to read. Written 2026-11-04 by Melanikus for the AvP port.
// See surface_pictures.cpp for how it works.

#ifndef SURFACE_PICTURES_H
#define SURFACE_PICTURES_H

#include <d3d9.h>

// Whether this card has what the pictures need at all: shader model 3, and three pictures drawn at once. Without that, nothing that
// feeds the pictures needs to run.
bool SurfacePictures_CardCanDrawThem(LPDIRECT3DDEVICE9 device);

// Whether the pictures exist, making them the first time it's asked. False on a card that can't do this, and then no light may rely on them.
bool SurfacePictures_Ready(LPDIRECT3DDEVICE9 device);

// Whether the relief pictures and what draws them are all there, so that asking for relief really would draw it.
bool SurfacePictures_ReliefCanRun();

// Gives the pictures back. The graphics card holds them, so they go whenever the device resets, and are made again after it.
void SurfacePictures_Release();

// Draws the frame's solid polygons into the pictures. depthPicture is where each pixel's distance goes, and it has already been cleared.
// worldAxes holds the world's three directions in the light list's space, three numbers each.
// The caller has set the depth test and the rest of the device up. Says false if nothing was drawn.
bool SurfacePictures_Draw(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *depthPicture, const float *worldAxes, bool reliefWanted, bool useStatic = false);

// Puts the picture of which way each pixel faces on the screen, for checking by eye. Each direction shows as a colour: red for right, green for
// up and blue for away from the camera, with grey for a normal of zero, and black where there's no surface.
void SurfacePictures_ShowNormals(LPDIRECT3DDEVICE9 device);

// The pictures themselves, for a shader to read. Which way each pixel faces, and its texture's colour.
// Draws only the level's walls that are in the fixed buffer into the three pictures, in their final colours, for comparing the two ways of drawing them. A depth
// buffer of its own has to be set, with depth writes on. fromBuffer picks the way. See CompareSceneRoutes in d3d_render.cpp.
bool SurfacePictures_DrawWalls(LPDIRECT3DDEVICE9 device, IDirect3DSurface9 *depthPicture, bool fromBuffer);

IDirect3DTexture9 *SurfacePictures_Normal();
IDirect3DTexture9 *SurfacePictures_Colour();

#endif
