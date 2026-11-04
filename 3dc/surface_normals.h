// Which way each point of a polygon faces, on its way to the graphics card. Written 2026-11-04 by Melanikus for the AvP port.
// See surface_normals.cpp for how it works.

#ifndef SURFACE_NORMALS_H
#define SURFACE_NORMALS_H

#include <stdint.h>

// What a level's shape keeps about each of its polygons, worked out once when the shape is loaded. See surface_normals.cpp.
struct SurfacePolygon
{
	float nx, ny, nz;        // an arrow straight through the polygon's flat surface, 1 long: see "THE WAY ROUND". All zero if it has no area
	float distance;          // how far the flat surface is from the shape's own centre, along that arrow
	uint32_t flatCorners;    // one bit for each of the first four corners: set if that corner uses the polygon's normal
};

// Works out the list for a finished shape. Only for a shape whose corners never change in play, and never for a model.
void SurfaceNormals_PrepareShape(struct shapeheader *shape);

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

// Writes a polygon's normals. polygonNumber is its place in the shape, and firstVertex is the place its first corner has in the game's list
// of vertices. The polygon has to be a real one of the shape being drawn, because its corner numbers are looked up in that shape.
void SurfaceNormals_WritePolygon(const struct polyheader *polygon, int polygonNumber, uint32_t firstVertex, uint32_t corners);

// Gives four numbers for a polygon of the shape being drawn: which way its texture's U runs, in the world's directions, and which side its V is on.
// All zero means the polygon has none, and then nothing should bend its normal. See SurfaceTangent.
void SurfaceNormals_PolygonTangent(int polygonNumber, float *tangent);

// Hands the list to the graphics card, or takes it back, for a pass that reads it.
void SurfaceNormals_UseList();
void SurfaceNormals_StopUsingList();

#endif
