// The level's still polygons, sent to the graphics card once. Written 2026-11-04 by Melanikus for the AvP port.
// See static_level.cpp for how it works. The first half of this file knows nothing about the graphics card, so a test can check it on its own.

#ifndef STATIC_LEVEL_H
#define STATIC_LEVEL_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

struct shapeheader;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;

// One corner of one polygon. Corners aren't shared, because normals and tangents belong to a polygon.
struct StaticVertex
{
	float x, y, z;            // in the world. The same sum the game does: (float)point + (float)place, with y not turned over
	float u, v;               // on the texture
	float nx, ny, nz;         // which way this corner faces, in the world
	float tx, ty, tz, flip;   // which way the texture runs: see SurfaceTangent. All zero if the shape has none
};

// A stretch of the list that shares a texture and belongs to one module. The numbers are the ones DrawIndexedPrimitive wants.
struct StaticRange
{
	uint32_t texture;       // which texture
	uint32_t baseVertex;    // BaseVertexIndex
	uint32_t firstVertex;   // MinIndex, counted from baseVertex
	uint32_t vertexCount;   // NumVertices
	uint32_t firstIndex;    // StartIndex
	uint32_t triangles;     // PrimitiveCount
	bool twoSided;          // the game shows both sides of these, so they're drawn with no culling
};

// One module to go in the list.
struct StaticLevelInput
{
	const shapeheader *shape;
	int place[3];       // where the shape is, in the world
	int moduleNumber;   // m_index
};

// What the builder needs from the game.
struct StaticLevelSetup
{
	const StaticLevelInput *modules;
	int numModules;
	int moduleNumbers;                 // every module number has to be below this
	uint32_t noTexture;                // the texture a polygon with no texture uses
	uint32_t mostVerticesForOneBase;   // how far one baseVertex can reach: what the card says, and never more than 16-bit indices can count
	uint32_t mostTrianglesInOneRange;  // how many triangles the card takes in one draw. A longer range becomes two

	// The size of a texture, or false if there's no such texture.
	bool (*textureSize)(uint32_t texture, uint32_t *width, uint32_t *height);

	// Which way one corner of a polygon faces: SurfaceNormals_CornerNormal.
	void (*cornerNormal)(const shapeheader *shape, int polygonNumber, int corner, float *normal);
};

// What the builder makes. If it refuses, everything here is empty, and refused says why.
struct StaticLevelData
{
	std::vector<StaticVertex> vertices;
	std::vector<uint32_t> indices;      // each counted from its range's baseVertex
	std::vector<uint32_t> colours;      // one D3DCOLOR for each vertex, raw, before gamma. Alpha is 255, or 0 for a polygon that takes no live light
	std::vector<StaticRange> ranges;    // in module order, so one module's sit together

	const char *refused;     // nothing if it was built
	int refusedModule;       // where, or -1
	int refusedPolygon;      // where, or -1

	int modulesIn;
	int polygonsIn;
	int polygonsLeftOut;     // solid polygons are all in, so these are the see-through, animated, hidden and undrawn ones

	// The ranges of one module, or nothing if it isn't in the list.
	const StaticRange *RangesOf(int moduleNumber, int *count) const;

	// Is this polygon in the list? If not, it has to go the old way.
	bool PolygonIsIn(int moduleNumber, int polygonNumber) const;

	// Is everything of this module that the game draws in the list? If not, the module has to go the old way, all of it.
	bool ModuleIsWhole(int moduleNumber) const;

	void Clear();

	// the workings of those two: for each module number, where its ranges and its polygons' bits are
	struct ModuleEntry
	{
		int firstRange;
		int numRanges;
		int firstBit;
		int numPolygons;
		bool whole;
	};
	std::vector<ModuleEntry> moduleEntries;
	std::vector<uint32_t> polygonBits;
};

// Does this module sit exactly where its points plus its place say? Only then can its corners be worked out once and kept. flags is its MapFlags, and the
// three angles are its MapEuler. The game makes every module's matrix the identity (module.cpp), so no level can turn one today, and this keeps it so.
// ObFlag_ArbRot comes straight from the file, and makes the game place a shape by the camera.
bool StaticLevel_ModuleSitsStill(int flags, int angleX, int angleY, int angleZ);

// Does this shape hold a baked colour for every point? Its colours come from a different chunk from its points, so it needn't, and a module whose shape
// doesn't has to stay on the old route. A shape with no baked colours is fine: it starts from black.
bool StaticLevel_ShapeHasItsColours(const shapeheader *shape);

// Builds the list. false means it refused, and the level stays on the old route.
bool StaticLevel_Work(const StaticLevelSetup &setup, StaticLevelData *data);

// ---------------------------------------------------------------------------------------------------------------------------
// The level the game is playing. These are in static_level_card.cpp.

// Builds the list for this level and sends it to the card. Called once, when the level has finished loading.
void StaticLevel_BuildThisLevel();

// Lets it all go. ClearMemoryPool calls this, because that's when the shapes go. A device reset needs nothing: the buffers are managed.
void StaticLevel_Release();

// Is there a list to use? If not, everything goes the old way.
bool StaticLevel_Ready();

// See StaticLevelData. Safe to ask whether or not there's a list.
const StaticRange *StaticLevel_RangesOf(int moduleNumber, int *count);
bool StaticLevel_PolygonIsIn(int moduleNumber, int polygonNumber);
bool StaticLevel_ModuleIsWhole(int moduleNumber);

// The buffers, for the code that draws them. It sets the stream, the indices and the declaration itself.
IDirect3DVertexBuffer9 *StaticLevel_VertexBuffer();
IDirect3DIndexBuffer9 *StaticLevel_IndexBuffer();

// The baked colours: one D3DCOLOR for each vertex, in the same order, as a stream of their own so the vertices never change. See StaticLevelData::colours.
IDirect3DVertexBuffer9 *StaticLevel_ColourBuffer();

// true means 32-bit indices. Otherwise they're 16-bit, and baseVertex does the rest. Always pass baseVertex.
bool StaticLevel_IndicesAre32Bit();

#endif
