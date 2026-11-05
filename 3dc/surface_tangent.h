// Which way a polygon's texture runs across it. Written 2026-11-04 by Melanikus for the AvP port.
// See surface_tangent.cpp for how it works. This file knows nothing about the game, so a test can check it on its own.

#ifndef SURFACE_TANGENT_H
#define SURFACE_TANGENT_H

// One for each polygon, worked out once at load, in the shape's own numbers. All zero means there's no way to tell, and then nothing bends that
// polygon's normal.
struct SurfaceTangent
{
	float x, y, z;   // an arrow 1 long along the polygon's surface, pointing the way the texture's U number grows
	float flip;      // +1 or -1: the texture's V direction is this times the normal crossed with the arrow above
};

// corners holds the polygon's first three corners, x y z each. coordinates holds their texture coordinates, u v each. Any scale will do for those,
// as long as u and v use the same one.
void SurfaceTangent_Work(const float *corners, const float *coordinates, SurfaceTangent *tangent);

#endif
