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

extern DISPLAYBLOCK *Global_ODB_Ptr;
extern SHAPEHEADER *Global_ShapeHeaderPtr;
extern VECTORCH *Global_ShapeVNormals;
extern MATRIXCH IdentityMatrix;
extern int DrawingAReflection;     // a piece of the level or an object is being drawn again in a mirror
extern int ToyShadowsInMirror;     // the same, for a body part

static const uint32_t kNumbersPerVertex = 4;

static VertexBuffer *normalList = 0;
static uint32_t verticesInList = 0;
static float *openList = 0;   // only while the list is open: four numbers for each vertex

// A moving object's normals, after turning. Polygons that meet at a point share its normal, so the same one comes up several times while
// an object is drawn. It's turned the first time and kept here with the number of that drawing, and every polygon after that reads it
// back. The game keeps its colours the same way, in ColourIntensityArray.
struct TurnedNormal
{
	float x, y, z;
	uint32_t drawing;
};

static TurnedNormal turnedNormals[maxrotpts];

static uint32_t thisDrawing = 0;      // each object drawn gets the next number, so nothing turned for the one before is ever reused
static bool objectIsTurned = false;   // does the object being drawn point a different way from the world?

// How to turn it, as three lines of three numbers: one line for each of the world's x, y and z.
static float turning[9];

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
	thisDrawing++;

	// After four billion drawings the count comes back round to zero, and an old normal could pass for a new one, so the slate is wiped.
	if (thisDrawing == 0)
	{
		memset(turnedNormals, 0, sizeof(turnedNormals));
		thisDrawing = 1;
	}

	// A piece of the level lines up with the world, so its numbers are right as they stand. Anything else has to be turned: something that
	// moves, or something placed at an angle. The object's own matrix is what's tested, because that's the thing that's true of every case.
	MATRIXCH const &matrix = Global_ODB_Ptr->ObMat;
	objectIsTurned = (memcmp(&matrix, &IdentityMatrix, sizeof(MATRIXCH)) != 0);

	if (objectIsTurned)
	{
		float const one = (float)ONE_FIXED;

		turning[0] = (float)matrix.mat11 / one;
		turning[1] = (float)matrix.mat21 / one;
		turning[2] = (float)matrix.mat31 / one;
		turning[3] = (float)matrix.mat12 / one;
		turning[4] = (float)matrix.mat22 / one;
		turning[5] = (float)matrix.mat32 / one;
		turning[6] = (float)matrix.mat13 / one;
		turning[7] = (float)matrix.mat23 / one;
		turning[8] = (float)matrix.mat33 / one;
	}
}

// A point's stored normal, in the world's directions. A point with no usable normal gets zero, which is a real answer: the top of this
// file says what it means. The point's number comes from the level file, so it's checked before it's used.
static void PointNormal(int point, float *normal)
{
	normal[0] = 0.0f;
	normal[1] = 0.0f;
	normal[2] = 0.0f;

	if (!Global_ShapeVNormals || point < 0 || point >= Global_ShapeHeaderPtr->numpoints || point >= maxrotpts)
	{
		return;
	}

	VECTORCH const &stored = Global_ShapeVNormals[point];
	float const one = (float)ONE_FIXED;
	float const x = (float)stored.vx / one;
	float const y = (float)stored.vy / one;
	float const z = (float)stored.vz / one;

	if (!objectIsTurned)
	{
		normal[0] = x;
		normal[1] = y;
		normal[2] = z;
		return;
	}

	TurnedNormal &turned = turnedNormals[point];

	if (turned.drawing != thisDrawing)
	{
		turned.x = turning[0] * x + turning[1] * y + turning[2] * z;
		turned.y = turning[3] * x + turning[4] * y + turning[5] * z;
		turned.z = turning[6] * x + turning[7] * y + turning[8] * z;
		turned.drawing = thisDrawing;
	}

	normal[0] = turned.x;
	normal[1] = turned.y;
	normal[2] = turned.z;
}

// THE CHOICE OF NORMAL: which normal a corner of a polygon uses. This is the only place that decides, so a different rule goes here and
// nothing else has to change. Today it's the point's own normal, the one the game lights that corner with. It's handed the polygon as well
// as the point so that a rule can look at both.
static void ChooseNormal(const POLYHEADER * /*polygon*/, int point, float *normal)
{
	PointNormal(point, normal);
}

void SurfaceNormals_WritePolygon(const POLYHEADER *polygon, uint32_t firstVertex, uint32_t corners)
{
	// With no list open there's nowhere to write. A reflection stays out of the pictures on purpose.
	if (!openList || DrawingAReflection || ToyShadowsInMirror)
	{
		return;
	}

	// every place written has to be inside the list
	if (firstVertex > verticesInList || corners > verticesInList - firstVertex)
	{
		return;
	}

	// The corners drawn have to be the polygon's own, in the same order. Nothing cuts polygons up today. If something ever does, the counts
	// differ, and writing nothing keeps the polygon out of the pictures, where a wrong normal would light it wrongly.
	const int *cornerNumbers = &polygon->Poly1stPt;

	for (uint32_t corner = 0; corner < corners; corner++)
	{
		if (cornerNumbers[corner] == kTerminationMarker)
		{
			return;
		}
	}

	if (cornerNumbers[corners] != kTerminationMarker)
	{
		return;
	}

	float *place = openList + firstVertex * kNumbersPerVertex;

	for (uint32_t corner = 0; corner < corners; corner++)
	{
		float normal[3];
		ChooseNormal(polygon, cornerNumbers[corner], normal);

		*place++ = normal[0];
		*place++ = normal[1];
		*place++ = normal[2];
		*place++ = 1.0f;   // this polygon goes in the pictures
	}
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
