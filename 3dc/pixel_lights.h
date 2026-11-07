// Lighting each pixel from the surface pictures. Written 2026-11-04 by Melanikus for the AvP port.
// See pixel_lights.cpp for how it works. Include this after 3dc.h and kshape.h, which it takes LIGHTBLOCK and RENDERVERTEX from.

#ifndef PIXEL_LIGHTS_H
#define PIXEL_LIGHTS_H

#include <d3d9.h>

// Whether lights can be worked out from the pictures, making what's needed the first time it's asked. If this is false, no light may leave
// the game's own lighting, because nothing here would add it back.
bool PixelLights_Ready(LPDIRECT3DDEVICE9 device);

// Gives back what's held on the graphics card, which has to go whenever the device resets. It's made again when next needed.
void PixelLights_Release();

// Adds up all the lights at each pixel, once a frame, after the surface pictures are drawn and before any light reads them. lights is the
// frame's list of chosen lights, kToyLightFloats numbers each. reachesScreen says which of them reach the screen at all, and reach says which
// rectangle of it each of those can light. This changes which picture is being drawn on, so the caller sets that next. It gives back how many
// screens' worth of pixels all the lights' rectangles came to, which is what this costs.
float PixelLights_DrawTotal(LPDIRECT3DDEVICE9 device, const float *lights, int numLights, const RECT *reach, const bool *reachesScreen, IDirect3DTexture9 *depth);

// For a light baked into the walls: on every pixel the stencil has marked, adds this light's share to the mask being drawn on.
// strength is how much of its share to take, which is 1 for an ordinary shadow. Says whether the card took it.
HRESULT PixelLights_AddShareToMask(LPDIRECT3DDEVICE9 device, const float *light, float strength, IDirect3DTexture9 *depth);

// For a light added per pixel: adds its light to the picture being drawn on, wherever the mask says it isn't blocked. softWidth and
// softStaysWideTo say how soft the shadow's edge is: its width in pixels up close, and how far away it stays that wide.
void PixelLights_AddLight(LPDIRECT3DDEVICE9 device, const float *light, IDirect3DTexture9 *mask, IDirect3DTexture9 *depth, float softWidth, float softStaysWideTo);

// LightSourcesInRangeOfObject tells this which lights it left out of the object it's working on, because they're being added per pixel.
void PixelLights_ForgetLeftOut();
void PixelLights_NoteLeftOut(LIGHTBLOCK *light);

// A polygon you can see through isn't in the surface pictures, so per-pixel lights never reach it. This gives one corner of it back the
// lights its object left out, as the game would have given them. vertexNumber is the corner's place in its shape.
void PixelLights_LightSeeThroughCorner(RENDERVERTEX *corner, int vertexNumber);

#endif
