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
// EMPTY MEANS "NOT IN THE PICTURES": each normal has a fourth number, 1 for a polygon that goes in the surface pictures, and 2 for one that also lives in the
// level's fixed buffer, so that surface_pictures.cpp can draw it from there and leave it out here. The list starts
// each frame full of zeros, so a polygon nobody wrote down stays out of them, and keeps the light the game gave it. A reflection does that
// on purpose. A shape can also really hold a normal of zero: that's a surface like any other, with a fourth number of 1, and the shaders
// light it the way the game does, as a surface turned side-on.
//
// THE LIST OF POLYGONS: a level's shape also keeps one entry for each polygon, worked out once when the shape is loaded and never again.
// It only works if a polygon's corners can't move within its shape, so these shapes have none: a shape that animates, which swaps its points
// in play, and a door that morphs, which swaps its polygons as well. Models have none either, because a model is meant to look smooth and
// its stored polygon normals aren't reliable. Anything without a list does without, or works things out as it goes.
//
// THE TANGENTS: relief pictures need to know which way a polygon's texture runs across it. That's a third list, four more numbers for each vertex, made only
// while relief pictures are on, so that with them off nothing here is any different. Unlike the list of normals it isn't cleared each frame, because a
// polygon nobody wrote down is already kept out of the pictures by its normal's fourth number, whatever this list holds.
//
// THE WAY ROUND: a polygon's arrow here points out of its back, the opposite way from the way it faces. The game's list of vertices has y
// turned over, which makes it a mirror image of the shape, and in a mirror image corners that went round clockwise go round the other way. The
// shadow code works on that list, so it reads a polygon's corners the other way round from the shape. This arrow is the one that agrees with it.

#include <math.h>
#include <string.h>

#include "3dc.h"
#include "module.h"

#include "surface_normals.h"
#include "static_level.h"
#include "surface_tangent.h"
#include "polygon_facing.h"
#include "texture_relief.h"
#include "d3_func.h"
#include "VertexBuffer.h"
#include "mempool.h"

extern DISPLAYBLOCK *Global_ODB_Ptr;
extern SHAPEHEADER *Global_ShapeHeaderPtr;
extern VECTORCH *Global_ShapeVNormals;
extern MATRIXCH IdentityMatrix;
extern int DrawingAReflection;     // a piece of the level or an object is being drawn again in a mirror
extern int ToyShadowsInMirror;     // the same, for a body part

static const uint32_t kNumbersPerVertex = 4;

// Polygons that meet at a point share its normal, so at the edge of a wall, the normal is an average of the wall and whatever meets it. The
// game's lighting only uses it at the corners, where it hardly shows. Lit pixel by pixel, the whole wall would curve like a pillow. So a
// corner this many degrees from its polygon uses the polygon's own normal, and the wall stays flat.
static const double kMostCornerLean = 30.0;

static const int kCornersInList = 4;   // flatCorners has room for this many

static VertexBuffer *normalList = 0;
static uint32_t verticesInList = 0;
static float *openList = 0;   // only while the list is open: four numbers for each vertex

static const uint32_t kTangentNumbersPerVertex = 4;

static VertexBuffer *tangentList = 0;
static float *openTangents = 0;   // only while the list is open: four numbers for each vertex

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
static int moduleBeingDrawn = -1;     // the module being drawn, or -1 if it's anything else

static const int kMostModulesNoted = kMostModulesInAFrame;
static int modulesDrawn[kMostModulesNoted];
static int numModulesDrawn = 0;

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
		return;
	}

	if (textureRelief.IsOn())
	{
		tangentList = new VertexBuffer;

		if (!tangentList->Create(vertices, kTangentNumbersPerVertex * sizeof(float), USAGE_DYNAMIC))
		{
			delete tangentList;
			tangentList = 0;
		}
	}
}

void SurfaceNormals_Release()
{
	delete normalList;
	normalList = 0;
	verticesInList = 0;
	openList = 0;

	delete tangentList;
	tangentList = 0;
	openTangents = 0;
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

	if (tangentList && tangentList->Lock(&memory))
	{
		openTangents = (float *)memory;
	}
}

void SurfaceNormals_Unlock()
{
	if (normalList && openList)
	{
		normalList->Unlock();
	}

	openList = 0;

	if (tangentList && openTangents)
	{
		tangentList->Unlock();
	}

	openTangents = 0;
}

// Works out one polygon's entry. Every number here comes from the level file, so each is checked before it's used. A polygon that fails a
// check keeps an empty entry, which casts no shadow and uses its points' own normals.
static void WorkOutPolygon(const SHAPEHEADER *shape, int polygonNumber, SurfacePolygon &facts)
{
	memset(&facts, 0, sizeof(facts));

	const POLYHEADER *polygon = (const POLYHEADER *)shape->items[polygonNumber];
	const VECTORCH *points = (const VECTORCH *)*shape->points;
	const int *corners = &polygon->Poly1stPt;
	int count = 0;

	while (count < kCornersInList && corners[count] != kTerminationMarker)
	{
		if (corners[count] < 0 || corners[count] >= shape->numpoints)
		{
			return;
		}

		count++;
	}

	if (count < 3)
	{
		return;
	}

	VECTORCH const &first = points[corners[0]];

	// the way the polygon faces, from the whole of it: see polygon_facing.h
	double face[3];
	PolygonFacing(points, corners, count, face);

	double const faceX = face[0], faceY = face[1], faceZ = face[2];
	double const faceLength = sqrt(faceX * faceX + faceY * faceY + faceZ * faceZ);

	if (faceLength < 0.001)
	{
		return;   // all its corners are in a line: no area, no flat surface
	}

	double const nx = -faceX / faceLength, ny = -faceY / faceLength, nz = -faceZ / faceLength;

	facts.nx = (float)nx;
	facts.ny = (float)ny;
	facts.nz = (float)nz;
	facts.distance = (float)(nx * first.vx + ny * first.vy + nz * first.vz);

	// The rest decides which corners lean too far. It compares a corner with the normal stored for the polygon. That one is left out if it's
	// empty, or points the other way from the corners, because then which way the polygon faces is in doubt, and the game's normals stay.
	int const place = polygon->PolyNormalIndex;   // counted in single numbers, three for each polygon

	if (!shape->sh_normals || !*shape->sh_normals || !shape->sh_vnormals || !*shape->sh_vnormals || place < 0 || place > (shape->numitems - 1) * 3)
	{
		return;
	}

	VECTORCH const &stored = *(const VECTORCH *)(*shape->sh_normals + place);
	double const storedLength = sqrt((double)stored.vx * stored.vx + (double)stored.vy * stored.vy + (double)stored.vz * stored.vz);

	if (storedLength <= 0.0 || (stored.vx * faceX + stored.vy * faceY + stored.vz * faceZ) <= 0.0)
	{
		return;
	}

	double const mostLean = cos(kMostCornerLean * 3.14159265358979 / 180.0);
	const VECTORCH *pointNormals = (const VECTORCH *)*shape->sh_vnormals;

	for (int corner = 0; corner < kCornersInList && corners[corner] >= 0 && corners[corner] < shape->numpoints; corner++)
	{
		VECTORCH const &normal = pointNormals[corners[corner]];
		double const length = sqrt((double)normal.vx * normal.vx + (double)normal.vy * normal.vy + (double)normal.vz * normal.vz);

		// a zero here is left as it is: the game lights that corner as a surface turned side-on
		if (length > 0.0 && ((double)normal.vx * stored.vx + (double)normal.vy * stored.vy + (double)normal.vz * stored.vz) / (length * storedLength) < mostLean)
		{
			facts.flatCorners |= 1u << corner;
		}
	}
}

void SurfaceNormals_PrepareShape(SHAPEHEADER *shape)
{
	if (!shape || shape->animation_header || shape->numitems <= 0 || shape->numpoints <= 0 || !shape->items || !shape->points || !*shape->points)
	{
		return;
	}

	SurfacePolygon *list = (SurfacePolygon *)PoolAllocateMem(shape->numitems * sizeof(SurfacePolygon));

	if (!list)
	{
		return;
	}

	for (int polygonNumber = 0; polygonNumber < shape->numitems; polygonNumber++)
	{
		WorkOutPolygon(shape, polygonNumber, list[polygonNumber]);
	}

	shape->sh_surfacepolygons = list;

	// Anything kept for a shape is found by this number. A shape's place in memory wouldn't do: the next level's shapes use the same memory, so
	// something kept from the old level could pass for the new one. This only counts up, and no run of the game loads four billion shapes.
	static unsigned int lastListNumber = 0;
	shape->sh_surfacelistnumber = ++lastListNumber;
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

	// an object can use the same shape as a module, and only the module is in the fixed buffer
	moduleBeingDrawn = Global_ODB_Ptr->ObMyModule ? Global_ODB_Ptr->ObMyModule->m_index : -1;

	// The modules a frame draws, which is what the fixed buffer has to draw too. This is here, beside the polygons being marked as in the buffer, so
	// that the two can't disagree: a polygon marked and not drawn from the buffer would vanish from the pictures. A reflection writes nothing.
	if (moduleBeingDrawn >= 0 && !DrawingAReflection && !ToyShadowsInMirror)
	{
		SurfaceNormals_NoteModuleDrawn(moduleBeingDrawn);
	}

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

void SurfaceNormals_NoteModuleDrawn(int moduleNumber)
{
	if (numModulesDrawn < kMostModulesNoted)
	{
		modulesDrawn[numModulesDrawn] = moduleNumber;
	}

	numModulesDrawn++;
}

const int *SurfaceNormals_ModulesDrawn(int *count)
{
	*count = numModulesDrawn;
	return modulesDrawn;
}

void SurfaceNormals_ForgetModulesDrawn()
{
	numModulesDrawn = 0;
}

void SurfaceNormals_PolygonTangent(int polygonNumber, float *tangent)
{
	tangent[0] = 0.0f;
	tangent[1] = 0.0f;
	tangent[2] = 0.0f;
	tangent[3] = 0.0f;

	if (!Global_ShapeHeaderPtr || !Global_ShapeHeaderPtr->sh_surfacetangents || polygonNumber < 0 || polygonNumber >= Global_ShapeHeaderPtr->numitems)
	{
		return;
	}

	SurfaceTangent const &stored = Global_ShapeHeaderPtr->sh_surfacetangents[polygonNumber];

	// Turning an object doesn't change which side a texture's V is on, so only the arrow is turned.
	tangent[0] = objectIsTurned ? turning[0] * stored.x + turning[1] * stored.y + turning[2] * stored.z : stored.x;
	tangent[1] = objectIsTurned ? turning[3] * stored.x + turning[4] * stored.y + turning[5] * stored.z : stored.y;
	tangent[2] = objectIsTurned ? turning[6] * stored.x + turning[7] * stored.y + turning[8] * stored.z : stored.z;
	tangent[3] = stored.flip;
}

// The way a polygon faces, in the world's directions. Its arrow is stored pointing the other way: the top of this file says why.
static void PolygonNormal(const SurfacePolygon &facts, float *normal)
{
	float const x = -facts.nx, y = -facts.ny, z = -facts.nz;

	if (objectIsTurned)
	{
		normal[0] = turning[0] * x + turning[1] * y + turning[2] * z;
		normal[1] = turning[3] * x + turning[4] * y + turning[5] * z;
		normal[2] = turning[6] * x + turning[7] * y + turning[8] * z;
	}
	else
	{
		normal[0] = x;
		normal[1] = y;
		normal[2] = z;
	}
}

// THE CHOICE OF NORMAL: which normal a corner of a polygon uses. This is the only place that decides. A corner marked as leaning too far uses
// its polygon's normal, and every other corner uses its point's own, the one the game lights it with. polygonNormal only holds an answer if
// one of the polygon's corners is marked.
static void ChooseNormal(const SurfacePolygon *facts, uint32_t corner, int point, const float *polygonNormal, float *normal)
{
	if (facts && corner < (uint32_t)kCornersInList && (facts->flatCorners & (1u << corner)))
	{
		normal[0] = polygonNormal[0];
		normal[1] = polygonNormal[1];
		normal[2] = polygonNormal[2];
		return;
	}

	PointNormal(point, normal);
}

void SurfaceNormals_CornerNormal(const SHAPEHEADER *shape, int polygonNumber, int corner, float *normal)
{
	normal[0] = 0.0f;
	normal[1] = 0.0f;
	normal[2] = 0.0f;

	if (!shape || !shape->items || polygonNumber < 0 || polygonNumber >= shape->numitems || corner < 0 || corner >= maxpolypts)
	{
		return;
	}

	const POLYHEADER *polygon = (const POLYHEADER *)shape->items[polygonNumber];
	const int *cornerNumbers = &polygon->Poly1stPt;

	for (int i = 0; i <= corner; i++)
	{
		if (cornerNumbers[i] == kTerminationMarker)
		{
			return;
		}
	}

	// the same two choices, in the same order, as ChooseNormal
	if (shape->sh_surfacepolygons && corner < kCornersInList)
	{
		const SurfacePolygon &facts = shape->sh_surfacepolygons[polygonNumber];

		if (facts.flatCorners & (1u << corner))
		{
			normal[0] = -facts.nx;
			normal[1] = -facts.ny;
			normal[2] = -facts.nz;
			return;
		}
	}

	int const point = cornerNumbers[corner];

	if (!shape->sh_vnormals || !*shape->sh_vnormals || point < 0 || point >= shape->numpoints || point >= maxrotpts)
	{
		return;
	}

	VECTORCH const &stored = ((const VECTORCH *)*shape->sh_vnormals)[point];
	float const one = (float)ONE_FIXED;

	normal[0] = (float)stored.vx / one;
	normal[1] = (float)stored.vy / one;
	normal[2] = (float)stored.vz / one;
}

void SurfaceNormals_WritePolygon(const POLYHEADER *polygon, int polygonNumber, uint32_t firstVertex, uint32_t corners)
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

	const SurfacePolygon *facts = 0;

	if (Global_ShapeHeaderPtr->sh_surfacepolygons && polygonNumber >= 0 && polygonNumber < Global_ShapeHeaderPtr->numitems)
	{
		facts = &Global_ShapeHeaderPtr->sh_surfacepolygons[polygonNumber];
	}

	// worked out once, however many of the polygon's corners use it
	float polygonNormal[3] = { 0.0f, 0.0f, 0.0f };

	if (facts && facts->flatCorners)
	{
		PolygonNormal(*facts, polygonNormal);
	}

	float *place = openList + firstVertex * kNumbersPerVertex;
	float const fourthNumber = (moduleBeingDrawn >= 0 && StaticLevel_PolygonIsIn(moduleBeingDrawn, polygonNumber)) ? 2.0f : 1.0f;

	for (uint32_t corner = 0; corner < corners; corner++)
	{
		float normal[3];
		ChooseNormal(facts, corner, cornerNumbers[corner], polygonNormal, normal);

		*place++ = normal[0];
		*place++ = normal[1];
		*place++ = normal[2];
		*place++ = fourthNumber;
	}

	// one tangent for the whole polygon, repeated at each corner. The checks above cover this list too: it's the same length.
	if (openTangents)
	{
		float tangent[kTangentNumbersPerVertex];
		SurfaceNormals_PolygonTangent(polygonNumber, tangent);

		float *tangentPlace = openTangents + firstVertex * kTangentNumbersPerVertex;

		for (uint32_t corner = 0; corner < corners; corner++, tangentPlace += kTangentNumbersPerVertex)
		{
			memcpy(tangentPlace, tangent, sizeof(tangent));
		}
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

bool SurfaceNormals_TangentsReady()
{
	return tangentList != 0;
}

void SurfaceNormals_UseTangents()
{
	if (tangentList)
	{
		d3d.lpD3DDevice->SetStreamSource(2, tangentList->_vertexBuffer.vertexBuffer, 0, kTangentNumbersPerVertex * sizeof(float));
	}
}

void SurfaceNormals_StopUsingTangents()
{
	d3d.lpD3DDevice->SetStreamSource(2, NULL, 0, 0);
}
