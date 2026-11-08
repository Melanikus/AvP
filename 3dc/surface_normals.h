// Which way each point of a polygon faces, on its way to the graphics card. Written 2026-11-04 by Melanikus for the AvP port.
// Modified 2026-11-06 by Melanikus for the AvP port:
//   can keep a copy of what it writes, for the NORMAL LINES view
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

// For the NORMAL LINES view: whether to keep a copy of the list, from the next time it's opened. The card's own list can't be read back. TheCopy gives back four numbers for each vertex, the same ones
// the card was given, in the same places, and how many vertices that is. It gives back nothing if the list that's current wasn't copied.
void SurfaceNormals_KeepACopy(bool wanted);
const float *SurfaceNormals_TheCopy(uint32_t *vertices);

// Opens and closes the list, at the same moments as the game's own vertices.
void SurfaceNormals_Lock();
void SurfaceNormals_Unlock();

// Says a new shape is about to be drawn. The polygons that follow belong to it.
void SurfaceNormals_StartObject();

// The normal a corner of a polygon uses, in the shape's own directions: the same answer SurfaceNormals_WritePolygon gives for an object lined up with the world.
// For anything built ahead of time from a shape, so it asks nothing of the frame being drawn. corner counts from 0. Gives back zero if there's no such
// corner, and also if that's what the shape really holds there.
void SurfaceNormals_CornerNormal(const struct shapeheader *shape, int polygonNumber, int corner, float *normal);

// Writes a polygon's normals. polygonNumber is its place in the shape, and firstVertex is the place its first corner has in the game's list
// of vertices. The polygon has to be a real one of the shape being drawn, because its corner numbers are looked up in that shape.
void SurfaceNormals_WritePolygon(const struct polyheader *polygon, int polygonNumber, uint32_t firstVertex, uint32_t corners);

// Gives four numbers for a polygon of the shape being drawn: which way its texture's U runs, in the world's directions, and which side its V is on.
// All zero means the polygon has none, and then nothing should bend its normal. See SurfaceTangent.
void SurfaceNormals_PolygonTangent(int polygonNumber, float *tangent);

// The most modules this notes in a frame.
enum { kMostModulesInAFrame = 4096 };

// The modules drawn so far this frame, and how many. count can be more than the list holds, which means some weren't noted.
const int *SurfaceNormals_ModulesDrawn(int *count);
void SurfaceNormals_NoteModuleDrawn(int moduleNumber);
void SurfaceNormals_ForgetModulesDrawn();

// Hands the list to the graphics card, or takes it back, for a pass that reads it.
void SurfaceNormals_UseList();
void SurfaceNormals_StopUsingList();

// The same for the list of tangents, which only exists while relief pictures are on: see "THE TANGENTS" in surface_normals.cpp.
bool SurfaceNormals_TangentsReady();
void SurfaceNormals_UseTangents();
void SurfaceNormals_StopUsingTangents();

#endif
