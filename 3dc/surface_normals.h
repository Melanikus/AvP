// Which way each point of a polygon faces, on its way to the graphics card. Written 2026-11-04 by Melanikus for the AvP port.
// See surface_normals.cpp for how it works.

#ifndef SURFACE_NORMALS_H
#define SURFACE_NORMALS_H

#include <stdint.h>

// Makes the list and gives it back. The graphics card holds it, so it goes whenever the device resets, like the game's own vertices.
void SurfaceNormals_Create(uint32_t vertices);
void SurfaceNormals_Release();

// Whether the list exists. If it doesn't, nothing can use it, and the game draws as it always has.
bool SurfaceNormals_Ready();

// Opens and closes the list, at the same moments as the game's own vertices.
void SurfaceNormals_Lock();
void SurfaceNormals_Unlock();

// Says a new shape is about to be drawn. The polygons that follow belong to it.
void SurfaceNormals_StartObject();

// Writes a polygon's normals. firstVertex is the place its first corner has in the game's list of vertices.
void SurfaceNormals_WritePolygon(const struct polyheader *polygon, uint32_t firstVertex, uint32_t corners);

// Hands the list to the graphics card, or takes it back, for a pass that reads it.
void SurfaceNormals_UseList();
void SurfaceNormals_StopUsingList();

#endif
