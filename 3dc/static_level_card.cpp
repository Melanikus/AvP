// The graphics card's side of the level's still polygons. Written 2026-11-04 by Melanikus for the AvP port.
//
// static_level.cpp makes the lists, and knows nothing about the game or the card. This hands it the level's modules, and sends what comes back to the card.
// It happens once, when a level has finished loading. The card's copies are managed, which means Direct3D keeps its own copy and puts them back after a
// device reset, so nothing here has to happen when that comes. A level restart doesn't load anything, so the lists stay right for it. They go when
// the shapes do, in ClearMemoryPool.
//
// If anything goes wrong, nothing is left behind, and every polygon goes the old way.

#include "static_level.h"
#include "3dc.h"
#include "module.h"
#include "d3_func.h"
#include "logString.h"
#include "surface_normals.h"
#include "surface_pictures.h"
#include "frame_timing.h"
#include "TextureManager.h"

#include <stdio.h>
#include <string.h>

extern bool ModuleNeverMoves(const MODULE *module);
extern SCENE Global_Scene;

static StaticLevelData theLevel;
static IDirect3DVertexBuffer9 *vertexBuffer = 0;
static IDirect3DIndexBuffer9 *indexBuffer = 0;
static IDirect3DVertexBuffer9 *colourBuffer = 0;
static bool indicesAre32Bit = false;
static bool ready = false;

static bool RealTextureSize(uint32_t texture, uint32_t *width, uint32_t *height)
{
	return Tex_GetRealSize(texture, width, height);
}

// Which modules go in. Only what's staying where it is, and has the lists a polygon's normals come from.
static void ListModules(std::vector<StaticLevelInput> *modules)
{
	if (!Global_ModulePtr)
	{
		return;
	}

	SCENEMODULE const *scene = Global_ModulePtr[Global_Scene];

	if (!scene || !scene->sm_marray)
	{
		return;
	}

	for (int i = 0; i < ModuleArraySize; i++)
	{
		MODULE const *module = scene->sm_marray[i];

		if (!ModuleNeverMoves(module))
		{
			continue;
		}

		SHAPEHEADER const *shape = GetShapeData(module->m_mapptr->MapShape);

		if (!shape || !shape->sh_surfacepolygons || (shape->shapeflags & (ShapeFlag_Sprite | ShapeFlag_SpriteR)))
		{
			continue;
		}

		MODULEMAPBLOCK const *map = module->m_mapptr;

		// Nothing sets a module's own colours today, and they'd be a list with no length. A shape can also hold fewer colours than points: one stock module does.
		if (module->m_extraitemdata || !StaticLevel_ShapeHasItsColours(shape))
		{
			continue;
		}

		if (!StaticLevel_ModuleSitsStill(map->MapFlags, map->MapEuler.EulerX, map->MapEuler.EulerY, map->MapEuler.EulerZ))
		{
			continue;
		}

		VECTORCH const &place = module->m_mapptr->MapWorld;
		StaticLevelInput const input = { shape, { place.vx, place.vy, place.vz }, module->m_index };

		modules->push_back(input);
	}
}

static bool SendVertices()
{
	UINT const bytes = (UINT)(theLevel.vertices.size() * sizeof(StaticVertex));
	void *memory = 0;

	if (FAILED(d3d.lpD3DDevice->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vertexBuffer, NULL)))
	{
		vertexBuffer = 0;
		return false;
	}

	if (FAILED(vertexBuffer->Lock(0, bytes, &memory, 0)) || !memory)
	{
		return false;
	}

	memcpy(memory, &theLevel.vertices[0], bytes);
	vertexBuffer->Unlock();

	return true;
}

static bool SendColours()
{
	UINT const bytes = (UINT)(theLevel.colours.size() * sizeof(D3DCOLOR));
	void *memory = 0;

	if (FAILED(d3d.lpD3DDevice->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &colourBuffer, NULL)))
	{
		colourBuffer = 0;
		return false;
	}

	if (FAILED(colourBuffer->Lock(0, bytes, &memory, 0)) || !memory)
	{
		return false;
	}

	memcpy(memory, &theLevel.colours[0], bytes);
	colourBuffer->Unlock();

	return true;
}

static bool SendIndices()
{
	size_t const count = theLevel.indices.size();
	UINT const bytes = (UINT)(count * (indicesAre32Bit ? sizeof(uint32_t) : sizeof(uint16_t)));
	void *memory = 0;

	if (FAILED(d3d.lpD3DDevice->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, indicesAre32Bit ? D3DFMT_INDEX32 : D3DFMT_INDEX16, D3DPOOL_MANAGED, &indexBuffer, NULL)))
	{
		indexBuffer = 0;
		return false;
	}

	if (FAILED(indexBuffer->Lock(0, bytes, &memory, 0)) || !memory)
	{
		return false;
	}

	if (indicesAre32Bit)
	{
		memcpy(memory, &theLevel.indices[0], bytes);
	}
	else
	{
		uint16_t *shortIndices = (uint16_t *)memory;

		for (size_t i = 0; i < count; i++)
		{
			shortIndices[i] = (uint16_t)theLevel.indices[i];
		}
	}

	indexBuffer->Unlock();

	return true;
}

void StaticLevel_BuildThisLevel()
{
	StaticLevel_Release();

	// Off means off: with neither switch, nothing is built, and the level loads as it always has.
	extern int StaticSurfaceOn;
	extern int StaticSurfaceCompare;

	if (!StaticSurfaceOn && !StaticSurfaceCompare)
	{
		return;
	}

	char line[300] = { 0 };

	if (!d3d.lpD3DDevice || !SurfacePictures_CardCanDrawThem(d3d.lpD3DDevice))
	{
		LogString("static level: this card can't draw the surface pictures, so the level stays on the old route");
		return;
	}

	D3DCAPS9 caps;

	if (FAILED(d3d.lpD3DDevice->GetDeviceCaps(&caps)))
	{
		LogString("static level: couldn't ask what the card can do, so the level stays on the old route");
		return;
	}

	indicesAre32Bit = caps.MaxVertexIndex > 0xFFFF;

	std::vector<StaticLevelInput> modules;
	ListModules(&modules);

	StaticLevelSetup setup;
	setup.modules = modules.empty() ? 0 : &modules[0];
	setup.numModules = (int)modules.size();
	setup.moduleNumbers = ModuleArraySize;
	setup.noTexture = (uint32_t)NO_TEXTURE;

	// What the card says it can take, and not what the index format could count. Nothing can catch an index past this once it's on the card.
	uint64_t const cardReach = (uint64_t)caps.MaxVertexIndex + 1;
	uint64_t const formatReach = indicesAre32Bit ? 0xFFFFFFFFull : 0xFFFFull;

	setup.mostVerticesForOneBase = (uint32_t)((cardReach < formatReach) ? cardReach : formatReach);
	setup.mostTrianglesInOneRange = caps.MaxPrimitiveCount;
	setup.textureSize = RealTextureSize;
	setup.cornerNormal = SurfaceNormals_CornerNormal;

	if (!StaticLevel_Work(setup, &theLevel))
	{
		_snprintf(line, sizeof(line) - 1, "static level: refused at module %d, polygon %d: %s. The level stays on the old route", theLevel.refusedModule, theLevel.refusedPolygon, theLevel.refused);
		LogString(line);
		theLevel.Clear();
		return;
	}

	if (theLevel.vertices.empty() || theLevel.indices.empty())
	{
		LogString("static level: nothing to send, so the level stays on the old route");
		theLevel.Clear();
		return;
	}

	if (!SendVertices() || !SendIndices() || !SendColours())
	{
		LogString("static level: the card wouldn't take the lists, so the level stays on the old route");
		StaticLevel_Release();
		return;
	}

	_snprintf(line, sizeof(line) - 1, "static level: %d modules, %d polygons in and %d left out, %u corners, %u triangles, %u ranges, %s indices",
		theLevel.modulesIn, theLevel.polygonsIn, theLevel.polygonsLeftOut, (unsigned int)theLevel.vertices.size(), (unsigned int)(theLevel.indices.size() / 3),
		(unsigned int)theLevel.ranges.size(), indicesAre32Bit ? "32-bit" : "16-bit");
	LogString(line);

	// the card has them now, and each frame only needs the ranges
	std::vector<StaticVertex>().swap(theLevel.vertices);
	std::vector<uint32_t>().swap(theLevel.indices);
	std::vector<uint32_t>().swap(theLevel.colours);

	ready = true;
}

void StaticLevel_Release()
{
	FrameTiming_WriteStalls();   // the same moment a level ends, and this is the one place all of them pass

	ready = false;

	if (vertexBuffer)
	{
		vertexBuffer->Release();
		vertexBuffer = 0;
	}

	if (indexBuffer)
	{
		indexBuffer->Release();
		indexBuffer = 0;
	}

	if (colourBuffer)
	{
		colourBuffer->Release();
		colourBuffer = 0;
	}

	theLevel.Clear();
}

bool StaticLevel_Ready()
{
	return ready;
}

const StaticRange *StaticLevel_RangesOf(int moduleNumber, int *count)
{
	if (!ready)
	{
		*count = 0;
		return 0;
	}

	return theLevel.RangesOf(moduleNumber, count);
}

bool StaticLevel_ModuleIsWhole(int moduleNumber)
{
	return ready && theLevel.ModuleIsWhole(moduleNumber);
}

bool StaticLevel_PolygonIsIn(int moduleNumber, int polygonNumber)
{
	return ready && theLevel.PolygonIsIn(moduleNumber, polygonNumber);
}

IDirect3DVertexBuffer9 *StaticLevel_VertexBuffer()
{
	return ready ? vertexBuffer : 0;
}

IDirect3DVertexBuffer9 *StaticLevel_ColourBuffer()
{
	return ready ? colourBuffer : 0;
}

IDirect3DIndexBuffer9 *StaticLevel_IndexBuffer()
{
	return ready ? indexBuffer : 0;
}

bool StaticLevel_IndicesAre32Bit()
{
	return indicesAre32Bit;
}
