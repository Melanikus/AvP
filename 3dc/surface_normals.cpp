// Which way each point of a polygon faces, on its way to the graphics card. Written 2026-11-04 by Melanikus for the AvP port.
//
// A "normal" is an arrow pointing straight out of a surface. Light falling square on a surface is strong, and light skimming along it
// is weak, so to light a surface you have to know which way it faces. The shape data already holds a normal for every point, worked out
// when the level was made, so nothing here works one out.
//
// THE LIST: the graphics card gets the game's polygons as a list of vertices. The normals are a second list beside it, one for each
// vertex, in the same order, so that the game's own list and everything that draws from it stay exactly as they were. Only a pass that
// asks for this list ever sees it.
//
// THE DIRECTIONS ARE THE WORLD'S, not the camera's. A piece of the level never turns, so its numbers are already right and are simply
// copied. If they were kept relative to the camera, every normal in view would need turning again each time the camera moved.
//
// EMPTY MEANS "NOT IN THE PICTURES": each normal has a fourth number, 1 for a polygon that goes in the surface pictures. The list starts
// each frame full of zeros, so a polygon nobody wrote down stays out of them, and keeps the light the game gave it. A reflection does that
// on purpose. A shape can also really hold a normal of zero: that's a surface like any other, with a fourth number of 1, and the shaders
// light it the way the game does, as a surface turned side-on.

#include <string.h>

#include "3dc.h"

#include "surface_normals.h"
#include "d3_func.h"
#include "VertexBuffer.h"

static const uint32_t kNumbersPerVertex = 4;

static VertexBuffer *normalList = 0;
static uint32_t verticesInList = 0;
static float *openList = 0;   // only while the list is open: four numbers for each vertex

void SurfaceNormals_Create(uint32_t vertices)
{
	SurfaceNormals_Release();

	normalList = new VertexBuffer;

	if (normalList->Create(vertices, kNumbersPerVertex * sizeof(float), USAGE_DYNAMIC))
	{
		verticesInList = vertices;
	}
	else
	{
		delete normalList;
		normalList = 0;
	}
}

void SurfaceNormals_Release()
{
	delete normalList;
	normalList = 0;
	verticesInList = 0;
	openList = 0;
}

bool SurfaceNormals_Ready()
{
	return normalList != 0;
}

void SurfaceNormals_Lock()
{
	void *memory = 0;

	if (normalList && normalList->Lock(&memory))
	{
		openList = (float *)memory;
		memset(openList, 0, verticesInList * kNumbersPerVertex * sizeof(float));
	}
}

void SurfaceNormals_Unlock()
{
	if (normalList && openList)
	{
		normalList->Unlock();
	}

	openList = 0;
}

void SurfaceNormals_StartObject()
{
}

void SurfaceNormals_WritePolygon(const POLYHEADER *, uint32_t, uint32_t)
{
}

void SurfaceNormals_UseList()
{
	if (normalList)
	{
		d3d.lpD3DDevice->SetStreamSource(1, normalList->_vertexBuffer.vertexBuffer, 0, kNumbersPerVertex * sizeof(float));
	}
}

void SurfaceNormals_StopUsingList()
{
	d3d.lpD3DDevice->SetStreamSource(1, NULL, 0, 0);
}
