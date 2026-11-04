// The level's still polygons, sent to the graphics card once. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHY THIS IS HERE
//
// Every frame the game turns each corner of every polygon it can see, on the processor, and sends the answers to the graphics card. But the walls of a
// level never move. So they can go to the card once, when the level has loaded, and after that each frame only has to say which pieces to draw.
//
// HOW IT WORKS
//
// This file makes the two lists the card keeps: one of corners, and one of triangles that point at those corners. It knows nothing about the card, so
// a test can run it on made-up shapes. static_level_card.cpp hands it the level's modules and sends the answer to the card.
//
// Polygons are sorted by texture and then by module, and a run of polygons with the same texture in the same module is called a range. To draw a module
// is to draw its ranges. Polygons the game shows from both sides come first, in ranges of their own, because they're drawn with different settings.
//
// A level file can't be trusted, and a bad number in these lists goes straight to the graphics driver, where nothing can catch it. So everything is
// checked here, even what the loader may have checked already: every corner number, texture, list length and float. Any failure refuses the whole
// level, and the level then simply goes the old way. Totals are added up in 64 bits and checked against a limit before any memory is asked for. Every
// index that goes to the card is checked once more at the end. Stock levels hold nothing this refuses.
//
// WHICH WAY ROUND
//
// The game hides the back of a polygon by testing its stored normal against the camera, on the processor. The card goes by the order of a triangle's
// corners. So a polygon's triangles are put in the order that makes the stored normal's side the front: the same order as its corners, unless the
// stored normal disagrees with them, as it does on a few polygons, and then each triangle is turned round. The triangles themselves don't change, so
// a quad that isn't quite flat still folds along the same line, and its depth stays where the game puts it. A polygon whose stored normal is empty
// passes the game's test from both sides, so it counts as two-sided.

#include "static_level.h"
#include "3dc.h"
#include "surface_tangent.h"

#include <math.h>
#include <string.h>
#include <algorithm>
#include <new>

static_assert(sizeof(StaticVertex) == 48, "the graphics card's declaration has to match this");
static_assert(sizeof(float) * 3 == sizeof(int) * 3, "corners are read as whole numbers and written as floats");

// A polygon's record has five places for corner numbers: four corners and the mark that ends them.
static const int kCornerPlaces = 5;
static const int kFewestCorners = 3;

// Limits, far above anything in a stock level. The largest stock level has about 105,000 corners, counting all its shapes.
static const uint64_t kMostCorners = 1000000;
static const uint64_t kMostPolygons = 4000000;
static const int kMostModuleNumbers = 1 << 20;
static const int kMostItemsInShape = 1 << 20;
static const int kMostUVLists = 1 << 20;
static const int kMostTextureCoordinate = 1 << 24;   // the largest in stock is 4,715
static const float kLargestNumber = 1.0e9f;

// One polygon on its way into the lists.
struct PolygonToPlace
{
	bool twoSided;
	uint32_t texture;
	int input;           // which of the setup's modules
	int polygonNumber;
	int corners;
	bool turnedRound;    // the stored normal disagrees with the corners
	float recipWidth;    // one over the texture's size. Zero for a polygon with no texture
	float recipHeight;
};

static bool ComesBefore(const PolygonToPlace &a, const PolygonToPlace &b)
{
	if (a.twoSided != b.twoSided)
	{
		return a.twoSided;
	}

	if (a.texture != b.texture)
	{
		return a.texture < b.texture;
	}

	if (a.input != b.input)
	{
		return a.input < b.input;
	}

	return a.polygonNumber < b.polygonNumber;
}

// A real number of a sensible size. A number that isn't real fails this too, because comparing one with anything is false.
static inline bool IsSensible(float number)
{
	return fabsf(number) <= kLargestNumber;
}

static bool Refuse(StaticLevelData *data, const char *why, int module, int polygon)
{
	data->refused = why;
	data->refusedModule = module;
	data->refusedPolygon = polygon;
	return false;
}

// Does the game draw polygons of this type? ShapePipeline quietly does nothing with any other, and the loader uses that for a polygon whose texture failed.
static inline bool GameDrawsType(int type)
{
	return type == I_ZB_GouraudPolygon || type == I_ZB_Gouraud2dTexturedPolygon || type == I_ZB_Gouraud3dTexturedPolygon;
}

static inline bool HasTexture(int type)
{
	return type != I_ZB_GouraudPolygon;
}

// Checks what a shape holds, before any of it is read.
static const char *CheckShape(const SHAPEHEADER *shape)
{
	if (!shape)
	{
		return "a module has no shape";
	}

	if (shape->numitems < 0 || shape->numitems > kMostItemsInShape || shape->numpoints < 0 || shape->numpoints > maxrotpts)
	{
		return "a shape's counts are out of range";
	}

	if (!shape->items || !shape->points || !*shape->points || !shape->sh_normals || !*shape->sh_normals)
	{
		return "a shape is missing its lists";
	}

	if (shape->sh_numuvlists < 0 || shape->sh_numuvlists > kMostUVLists || (shape->sh_numuvlists > 0 && (!shape->sh_textures || !shape->sh_uvlistlengths)))
	{
		return "a shape's texture coordinate lists don't add up";
	}

	return 0;
}

// A corner count from a polygon's record, without ever reading past the record. Gives back 0 if the record holds no end mark or too few corners.
static int CountCorners(const POLYHEADER *polygon, int numPoints)
{
	int const *corners = &polygon->Poly1stPt;

	for (int place = 0; place < kCornerPlaces; place++)
	{
		if (corners[place] == kTerminationMarker)
		{
			return (place >= kFewestCorners) ? place : 0;
		}

		if (corners[place] < 0 || corners[place] >= numPoints)
		{
			return 0;
		}
	}

	return 0;
}

// Which way a polygon's corners go round, added up over its triangles so that a first triangle with no area can't mislead it.
static void CornerFacing(const VECTORCH *points, const int *corners, int count, double *facing)
{
	facing[0] = 0.0;
	facing[1] = 0.0;
	facing[2] = 0.0;

	VECTORCH const &first = points[corners[0]];

	for (int i = 1; i < count - 1; i++)
	{
		double const ax = (double)points[corners[i]].vx - first.vx, ay = (double)points[corners[i]].vy - first.vy, az = (double)points[corners[i]].vz - first.vz;
		double const bx = (double)points[corners[i + 1]].vx - first.vx, by = (double)points[corners[i + 1]].vy - first.vy, bz = (double)points[corners[i + 1]].vz - first.vz;

		facing[0] += ay * bz - az * by;
		facing[1] += az * bx - ax * bz;
		facing[2] += ax * by - ay * bx;
	}
}

// Everything about one polygon that can be wrong. Gives back why it's refused, or nothing. polygon->PolyFlags has already said this one goes in.
static const char *CheckPolygon(const SHAPEHEADER *shape, const POLYHEADER *polygon, const std::vector<char> &animatedLists, const StaticLevelSetup &setup, PolygonToPlace *out)
{
	int const corners = CountCorners(polygon, shape->numpoints);

	if (!corners)
	{
		return "a polygon's corners are bad";
	}

	out->corners = corners;

	// the stored normal, which is what the game tests to hide a polygon's back
	int const normalPlace = polygon->PolyNormalIndex;

	if (normalPlace < 0 || (int64_t)normalPlace + 3 > (int64_t)shape->numitems * 3)
	{
		return "a polygon's normal number is out of range";
	}

	VECTORCH const &stored = *(const VECTORCH *)(*shape->sh_normals + normalPlace);
	bool const noNormal = stored.vx == 0 && stored.vy == 0 && stored.vz == 0;

	out->twoSided = noNormal || (polygon->PolyFlags & iflag_no_bfc) != 0;

	double facing[3];
	CornerFacing((const VECTORCH *)*shape->points, &polygon->Poly1stPt, corners, facing);
	out->turnedRound = !out->twoSided && facing[0] * stored.vx + facing[1] * stored.vy + facing[2] * stored.vz < 0.0;
	out->recipWidth = 0.0f;
	out->recipHeight = 0.0f;

	if (!HasTexture(polygon->PolyItemType))
	{
		out->texture = setup.noTexture;
		return 0;
	}

	uint32_t const texture = (uint32_t)polygon->PolyColour & ClrTxDefn;
	uint32_t const listNumber = (uint32_t)polygon->PolyColour >> TxDefn;
	uint32_t width = 0, height = 0;

	if (!setup.textureSize(texture, &width, &height) || width == 0 || height == 0)
	{
		return "a polygon's texture doesn't exist";
	}

	if (listNumber >= (uint32_t)shape->sh_numuvlists)
	{
		return "a polygon's texture coordinate list is out of range";
	}

	// an animated polygon's list holds something else altogether, so a plain polygon can't share it
	if (animatedLists[listNumber])
	{
		return "a polygon shares an animated texture's list";
	}

	if (!shape->sh_textures[listNumber] || shape->sh_uvlistlengths[listNumber] < corners)
	{
		return "a polygon's texture coordinate list is too short";
	}

	int const *coordinates = shape->sh_textures[listNumber];

	for (int i = 0; i < corners * 2; i++)
	{
		if (coordinates[i] < -kMostTextureCoordinate || coordinates[i] > kMostTextureCoordinate)
		{
			return "a polygon's texture coordinate is out of range";
		}
	}

	// the same sum as D3D_ZBufferedGouraudTexturedPolygon_Output, so that the answer matches to the last digit
	out->texture = texture;
	out->recipWidth = 1.0f / (float)width;
	out->recipHeight = 1.0f / (float)height;
	return 0;
}

// Writes one polygon's corners, as the game would. Gives back why it's refused, or nothing.
static const char *WriteCorners(const StaticLevelInput &input, const PolygonToPlace &placed, const StaticLevelSetup &setup, std::vector<StaticVertex> *vertices)
{
	const SHAPEHEADER *shape = input.shape;
	const POLYHEADER *polygon = (const POLYHEADER *)shape->items[placed.polygonNumber];
	const VECTORCH *points = (const VECTORCH *)*shape->points;
	int const *corners = &polygon->Poly1stPt;
	bool const textured = HasTexture(polygon->PolyItemType);
	int const *coordinates = textured ? shape->sh_textures[(uint32_t)polygon->PolyColour >> TxDefn] : 0;

	float tangent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	if (shape->sh_surfacetangents)
	{
		SurfaceTangent const &stored = shape->sh_surfacetangents[placed.polygonNumber];

		tangent[0] = stored.x;
		tangent[1] = stored.y;
		tangent[2] = stored.z;
		tangent[3] = stored.flip;
	}

	for (int corner = 0; corner < placed.corners; corner++)
	{
		VECTORCH const &point = points[corners[corner]];
		StaticVertex vertex;

		vertex.x = (float)point.vx + (float)input.place[0];
		vertex.y = (float)point.vy + (float)input.place[1];
		vertex.z = (float)point.vz + (float)input.place[2];

		vertex.u = textured ? (float)coordinates[corner * 2 + 0] * placed.recipWidth : 0.0f;
		vertex.v = textured ? (float)coordinates[corner * 2 + 1] * placed.recipHeight : 0.0f;

		float normal[3] = { 0.0f, 0.0f, 0.0f };
		setup.cornerNormal(shape, placed.polygonNumber, corner, normal);

		vertex.nx = normal[0];
		vertex.ny = normal[1];
		vertex.nz = normal[2];

		vertex.tx = tangent[0];
		vertex.ty = tangent[1];
		vertex.tz = tangent[2];
		vertex.flip = tangent[3];

		float const *number = &vertex.x;

		for (int i = 0; i < 12; i++)
		{
			if (!IsSensible(number[i]))
			{
				return "a polygon has a number that isn't sensible";
			}
		}

		vertices->push_back(vertex);
	}

	return 0;
}

// The last check, on the finished lists, because a bad index goes to the graphics driver and nothing can catch it there. Every index of every range
// has to land on a corner of that range, and every index in the list has to belong to a range.
static bool EveryIndexIsSafe(const StaticLevelData &data, uint32_t mostForOneBase)
{
	uint64_t indicesSeen = 0;

	for (size_t number = 0; number < data.ranges.size(); number++)
	{
		StaticRange const &range = data.ranges[number];
		uint64_t const count = (uint64_t)range.triangles * 3;
		uint64_t const lowest = range.firstVertex;
		uint64_t const pastHighest = lowest + range.vertexCount;

		if ((uint64_t)range.firstIndex + count > data.indices.size() || pastHighest > mostForOneBase || (uint64_t)range.baseVertex + pastHighest > data.vertices.size())
		{
			return false;
		}

		for (uint64_t i = 0; i < count; i++)
		{
			uint32_t const index = data.indices[(size_t)(range.firstIndex + i)];

			if (index < lowest || index >= pastHighest)
			{
				return false;
			}
		}

		indicesSeen += count;
	}

	return indicesSeen == data.indices.size();
}

// Which lists of a shape belong to an animated polygon. Marks them all before any polygon is checked, because it doesn't matter which comes first.
static void FindAnimatedLists(const SHAPEHEADER *shape, std::vector<char> *animatedLists)
{
	animatedLists->assign((size_t)shape->sh_numuvlists, 0);

	for (int polygonNumber = 0; polygonNumber < shape->numitems; polygonNumber++)
	{
		POLYHEADER const *polygon = (const POLYHEADER *)shape->items[polygonNumber];

		if (polygon && (polygon->PolyFlags & iflag_txanim))
		{
			uint32_t const listNumber = (uint32_t)polygon->PolyColour >> TxDefn;

			if (listNumber < (uint32_t)shape->sh_numuvlists)
			{
				(*animatedLists)[listNumber] = 1;
			}
		}
	}
}

static inline void AddTriangle(std::vector<uint32_t> *indices, uint32_t first, uint32_t a, uint32_t b, uint32_t c)
{
	indices->push_back(first + a);
	indices->push_back(first + b);
	indices->push_back(first + c);
}

static bool Work(const StaticLevelSetup &setup, StaticLevelData *data)
{
	if (setup.numModules < 0 || setup.moduleNumbers < 0 || setup.moduleNumbers > kMostModuleNumbers || (setup.numModules > 0 && !setup.modules)
		|| setup.mostVerticesForOneBase < 4 || !setup.textureSize || !setup.cornerNormal)
	{
		return Refuse(data, "the builder was set up wrongly", -1, -1);
	}

	data->moduleEntries.assign((size_t)setup.moduleNumbers, StaticLevelData::ModuleEntry());

	// First, check everything and add up what's wanted, without allocating anything that a bad file could make huge.
	uint64_t totalCorners = 0, totalTriangles = 0, totalPolygons = 0;
	std::vector<PolygonToPlace> polygons;
	std::vector<char> animatedLists;

	for (int input = 0; input < setup.numModules; input++)
	{
		StaticLevelInput const &module = setup.modules[input];
		const char *problem = CheckShape(module.shape);

		if (problem)
		{
			return Refuse(data, problem, module.moduleNumber, -1);
		}

		if (module.moduleNumber < 0 || module.moduleNumber >= setup.moduleNumbers)
		{
			return Refuse(data, "a module's number is out of range", module.moduleNumber, -1);
		}

		StaticLevelData::ModuleEntry &entry = data->moduleEntries[(size_t)module.moduleNumber];

		if (entry.numPolygons)
		{
			return Refuse(data, "two modules have the same number", module.moduleNumber, -1);
		}

		totalPolygons += (uint64_t)module.shape->numitems;

		if (totalPolygons > kMostPolygons)
		{
			return Refuse(data, "the level has too many polygons", module.moduleNumber, -1);
		}

		entry.numPolygons = module.shape->numitems;
		entry.firstBit = (int)(totalPolygons - (uint64_t)module.shape->numitems);

		FindAnimatedLists(module.shape, &animatedLists);

		for (int polygonNumber = 0; polygonNumber < module.shape->numitems; polygonNumber++)
		{
			POLYHEADER const *polygon = (const POLYHEADER *)module.shape->items[polygonNumber];

			if (!polygon)
			{
				return Refuse(data, "a shape has a polygon missing", module.moduleNumber, polygonNumber);
			}

			if (!GameDrawsType(polygon->PolyItemType) || (polygon->PolyFlags & (iflag_notvis | iflag_transparent | iflag_txanim | iflag_light_corona)))
			{
				data->polygonsLeftOut++;
				continue;
			}

			PolygonToPlace placed;
			placed.input = input;
			placed.polygonNumber = polygonNumber;

			problem = CheckPolygon(module.shape, polygon, animatedLists, setup, &placed);

			if (problem)
			{
				return Refuse(data, problem, module.moduleNumber, polygonNumber);
			}

			totalCorners += (uint64_t)placed.corners;
			totalTriangles += (uint64_t)placed.corners - 2;

			if (totalCorners > kMostCorners)
			{
				return Refuse(data, "the level has too many corners", module.moduleNumber, polygonNumber);
			}

			polygons.push_back(placed);
		}
	}

	std::sort(polygons.begin(), polygons.end(), ComesBefore);

	data->vertices.reserve((size_t)totalCorners);
	data->indices.reserve((size_t)totalTriangles * 3);
	data->polygonBits.assign((size_t)(totalPolygons / 32 + 1), 0);

	// Then write the lists. rangeInputs says which module each range belongs to, until they're put in module order at the end.
	std::vector<int> rangeInputs;
	std::vector<StaticRange> ranges;
	uint32_t base = 0;
	uint32_t sinceBase = 0;

	for (size_t number = 0; number < polygons.size(); number++)
	{
		PolygonToPlace const &placed = polygons[number];
		uint32_t const corners = (uint32_t)placed.corners;
		bool needNewRange = ranges.empty();

		if (!needNewRange)
		{
			StaticRange const &last = ranges.back();
			needNewRange = last.texture != placed.texture || last.twoSided != placed.twoSided || rangeInputs.back() != placed.input;
		}

		// with 16-bit indices, a range can only reach so far from its base, so the next one gets a new base
		if (corners > setup.mostVerticesForOneBase - sinceBase)
		{
			base = (uint32_t)data->vertices.size();
			sinceBase = 0;
			needNewRange = true;
		}

		if (needNewRange)
		{
			StaticRange range;
			range.texture = placed.texture;
			range.baseVertex = base;
			range.firstVertex = sinceBase;
			range.vertexCount = 0;
			range.firstIndex = (uint32_t)data->indices.size();
			range.triangles = 0;
			range.twoSided = placed.twoSided;

			ranges.push_back(range);
			rangeInputs.push_back(placed.input);
		}

		const char *problem = WriteCorners(setup.modules[placed.input], placed, setup, &data->vertices);

		if (problem)
		{
			return Refuse(data, problem, setup.modules[placed.input].moduleNumber, placed.polygonNumber);
		}

		// these are the game's triangles, from CreateIndices, all put the same way round
		if (corners == 3)
		{
			placed.turnedRound ? AddTriangle(&data->indices, sinceBase, 0, 2, 1) : AddTriangle(&data->indices, sinceBase, 0, 1, 2);
		}
		else
		{
			placed.turnedRound ? AddTriangle(&data->indices, sinceBase, 0, 2, 1) : AddTriangle(&data->indices, sinceBase, 0, 1, 2);
			placed.turnedRound ? AddTriangle(&data->indices, sinceBase, 0, 3, 2) : AddTriangle(&data->indices, sinceBase, 0, 2, 3);
		}

		ranges.back().vertexCount += corners;
		ranges.back().triangles += corners - 2;
		sinceBase += corners;

		StaticLevelData::ModuleEntry const &entry = data->moduleEntries[(size_t)setup.modules[placed.input].moduleNumber];
		uint32_t const bit = (uint32_t)entry.firstBit + (uint32_t)placed.polygonNumber;

		data->polygonBits[bit / 32] |= 1u << (bit % 32);
		data->polygonsIn++;
	}

	// Each module's ranges go together, keeping their order, because a module is what's asked about each frame.
	std::vector<std::vector<int> > rangesOfInput((size_t)setup.numModules);

	for (size_t number = 0; number < ranges.size(); number++)
	{
		rangesOfInput[(size_t)rangeInputs[number]].push_back((int)number);
	}

	data->ranges.reserve(ranges.size());

	for (int input = 0; input < setup.numModules; input++)
	{
		StaticLevelData::ModuleEntry &entry = data->moduleEntries[(size_t)setup.modules[input].moduleNumber];

		entry.firstRange = (int)data->ranges.size();
		entry.numRanges = (int)rangesOfInput[(size_t)input].size();

		for (size_t i = 0; i < rangesOfInput[(size_t)input].size(); i++)
		{
			data->ranges.push_back(ranges[(size_t)rangesOfInput[(size_t)input][i]]);
		}

		if (entry.numRanges)
		{
			data->modulesIn++;
		}
	}

	if (!EveryIndexIsSafe(*data, setup.mostVerticesForOneBase))
	{
		return Refuse(data, "an index came out wrong", -1, -1);
	}

	return true;
}

bool StaticLevel_Work(const StaticLevelSetup &setup, StaticLevelData *data)
{
	data->Clear();

	bool built = false;

	try
	{
		built = Work(setup, data);
	}
	catch (const std::bad_alloc &)
	{
		built = Refuse(data, "there wasn't enough memory", -1, -1);
	}

	if (!built)
	{
		// everything is emptied, but the reason is kept
		const char *why = data->refused;
		int const module = data->refusedModule, polygon = data->refusedPolygon;

		data->Clear();
		data->refused = why;
		data->refusedModule = module;
		data->refusedPolygon = polygon;
	}

	return built;
}

void StaticLevelData::Clear()
{
	std::vector<StaticVertex>().swap(vertices);
	std::vector<uint32_t>().swap(indices);
	std::vector<StaticRange>().swap(ranges);
	std::vector<ModuleEntry>().swap(moduleEntries);
	std::vector<uint32_t>().swap(polygonBits);

	refused = 0;
	refusedModule = -1;
	refusedPolygon = -1;
	modulesIn = 0;
	polygonsIn = 0;
	polygonsLeftOut = 0;
}

const StaticRange *StaticLevelData::RangesOf(int moduleNumber, int *count) const
{
	*count = 0;

	if (moduleNumber < 0 || (size_t)moduleNumber >= moduleEntries.size() || !moduleEntries[(size_t)moduleNumber].numRanges)
	{
		return 0;
	}

	ModuleEntry const &entry = moduleEntries[(size_t)moduleNumber];

	*count = entry.numRanges;
	return &ranges[(size_t)entry.firstRange];
}

bool StaticLevelData::PolygonIsIn(int moduleNumber, int polygonNumber) const
{
	if (moduleNumber < 0 || (size_t)moduleNumber >= moduleEntries.size())
	{
		return false;
	}

	ModuleEntry const &entry = moduleEntries[(size_t)moduleNumber];

	if (polygonNumber < 0 || polygonNumber >= entry.numPolygons)
	{
		return false;
	}

	uint32_t const bit = (uint32_t)entry.firstBit + (uint32_t)polygonNumber;

	return (polygonBits[bit / 32] >> (bit % 32)) & 1u;
}
