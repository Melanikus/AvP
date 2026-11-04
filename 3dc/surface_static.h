// The surface pictures, drawn from the level's fixed buffer. Written 2026-11-04 by Melanikus for the AvP port.
// See surface_static.cpp for how it works.

#ifndef SURFACE_STATIC_H
#define SURFACE_STATIC_H

#include <d3d9.h>

#include "static_level.h"

// Says which runs of the fixed buffer this frame draws. The list has to stay as it is until the pictures have been drawn.
void SurfaceStatic_SetFrame(const StaticRange *ranges, int count);

// Whether this route can draw at all. If not, the old route draws everything.
bool SurfaceStatic_Ready();

// Draws the frame's runs into the pictures that are set. withRelief picks the shaders, and it's the same answer the rest of the pass is using.
void SurfaceStatic_Draw(LPDIRECT3DDEVICE9 device, bool withRelief);

#endif
