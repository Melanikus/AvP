// The test for 3dc/static_level.cpp, which builds the lists of the level's still polygons that go to the graphics card.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// It makes up shapes in the game's own structures and builds from them. Good shapes have to come out exactly as the game draws them: the same corners,
// the same triangles, the right way round, in the right order. Hostile shapes have to be refused, every one of them, with nothing left behind, because
// a bad number in these lists goes to the graphics driver and nothing can catch it there.

#include "static_level.h"
#include "3dc.h"
#include "surface_tangent.h"

#include <float.h>
#include <limits>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		failures++;
	}
}

// ---- made-up textures and normals ----

static const uint32_t kNoTexture = 99;
static const uint32_t kTextureA = 1, kTextureB = 2, kTextureC = 3;
static const uint32_t kTextureNoWidth = 7, kTextureNoHeight = 8;

static bool MadeUpTextureSize(uint32_t texture, uint32_t *width, uint32_t *height)
{
	if (texture == kTextureA || texture == kTextureB || texture == kTextureC)
	{
		*width = 256;
		*height = 128;
		return true;
	}

	if (texture == kTextureNoWidth || texture == kTextureNoHeight)
	{
		*width = (texture == kTextureNoWidth) ? 0 : 256;
		*height = (texture == kTextureNoHeight) ? 0 : 256;
		return true;
	}

	return false;
}

static float normalToGive = 1.0f;

static void MadeUpCornerNormal(const shapeheader *, int polygonNumber, int corner, float *normal)
{
	normal[0] = (float)polygonNumber / 10.0f;
	normal[1] = (float)corner / 10.0f;
	normal[2] = normalToGive;
}

// ---- made-up shapes ----

// the same layout as a polygon in the game: POLYHEADER's four numbers, then the five places for corners
struct MadeUpPolygon
{
	int type, normalIndex, flags, colour;
	int corners[5];
};

static_assert(sizeof(MadeUpPolygon) == 9 * sizeof(int), "a polygon's record is nine numbers");

struct MadeUpShape
{
	SHAPEHEADER header;
	std::vector<MadeUpPolygon> polygons;
	std::vector<int *> items;
	std::vector<VECTORCH> points;
	int *pointsPointer;
	std::vector<VECTORCH> normals;
	int *normalsPointer;
	std::vector<std::vector<int> > lists;
	std::vector<int *> listPointers;
	std::vector<int> listLengths;
	std::vector<SurfaceTangent> tangents;
	std::vector<EXTRAITEMDATA> colours;

	MadeUpShape()
	{
		memset(&header, 0, sizeof(header));
		pointsPointer = 0;
		normalsPointer = 0;
		AddPoint(0, 0, 0);
		AddPoint(100, 0, 0);
		AddPoint(100, 100, 0);
		AddPoint(0, 100, 0);
	}

	void AddPoint(int x, int y, int z)
	{
		VECTORCH point = { x, y, z };
		points.push_back(point);
	}

	int AddList(int numbers)
	{
		std::vector<int> list((size_t)numbers * 2);

		for (size_t i = 0; i < list.size(); i++)
		{
			list[i] = 10 * ((int)i + 1);
		}

		lists.push_back(list);
		return (int)lists.size() - 1;
	}

	// adds a polygon facing up z, which is the way its corners go round
	int AddPolygon(int type, uint32_t texture, int list, int cornerCount, int flags = 0)
	{
		MadeUpPolygon polygon;
		polygon.type = type;
		polygon.normalIndex = (int)normals.size() * 3;
		polygon.flags = flags;
		polygon.colour = (int)(((uint32_t)list << TxDefn) | (texture & ClrTxDefn));

		for (int i = 0; i < 5; i++)
		{
			polygon.corners[i] = (i < cornerCount) ? i : kTerminationMarker;
		}

		polygons.push_back(polygon);

		VECTORCH normal = { 0, 0, ONE_FIXED };
		normals.push_back(normal);

		SurfaceTangent tangent = { 1.0f, 0.0f, 0.0f, -1.0f };
		tangents.push_back(tangent);

		return (int)polygons.size() - 1;
	}

	int AddQuad(uint32_t texture, int flags = 0)
	{
		int const list = AddList(4);
		return AddPolygon(I_ZB_Gouraud3dTexturedPolygon, texture, list, 4, flags);
	}

	int AddTriangle(uint32_t texture, int flags = 0)
	{
		int const list = AddList(3);
		return AddPolygon(I_ZB_Gouraud3dTexturedPolygon, texture, list, 3, flags);
	}

	// call this when the shape is complete: it points the game's structure at the lists
	void Finish()
	{
		items.clear();
		listPointers.clear();
		listLengths.clear();

		for (size_t i = 0; i < polygons.size(); i++)
		{
			items.push_back((int *)&polygons[i]);
		}

		for (size_t i = 0; i < lists.size(); i++)
		{
			listPointers.push_back(lists[i].empty() ? 0 : &lists[i][0]);
			listLengths.push_back((int)(lists[i].size() / 2));
		}

		pointsPointer = (int *)&points[0];
		normalsPointer = normals.empty() ? 0 : (int *)&normals[0];

		header.numitems = (int)polygons.size();
		header.numpoints = (int)points.size();
		header.items = items.empty() ? 0 : &items[0];
		header.points = &pointsPointer;
		header.sh_normals = normalsPointer ? &normalsPointer : 0;
		header.sh_textures = listPointers.empty() ? 0 : &listPointers[0];
		header.sh_uvlistlengths = listLengths.empty() ? 0 : &listLengths[0];
		header.sh_numuvlists = (int)lists.size();
		header.sh_surfacetangents = tangents.empty() ? 0 : &tangents[0];

		if (!colours.empty())
		{
			header.shapeflags |= ShapeFlag_PreLit;
			header.sh_extraitemdata = &colours[0];
			header.sh_numextraitems = (int)colours.size();
		}
	}

	// a different baked colour for each point: red, green and blue that can't be mistaken for each other
	void BakeColours()
	{
		colours.clear();

		for (size_t i = 0; i < points.size(); i++)
		{
			EXTRAITEMDATA colour;
			colour.EID_VertexI = (int)(0x7F000000u | ((uint32_t)(0x10 + i) << 16) | ((uint32_t)(0x40 + i) << 8) | (uint32_t)(0x70 + i));
			colours.push_back(colour);
		}
	}
};

static StaticLevelSetup MakeSetup(const StaticLevelInput *modules, int numModules)
{
	StaticLevelSetup setup;
	setup.modules = modules;
	setup.numModules = numModules;
	setup.moduleNumbers = 20;
	setup.noTexture = kNoTexture;
	setup.mostVerticesForOneBase = 0xFFFFFFFFu;
	setup.mostTrianglesInOneRange = 0xFFFFFFFFu;
	setup.textureSize = MadeUpTextureSize;
	setup.cornerNormal = MadeUpCornerNormal;
	return setup;
}

static bool BuildOne(MadeUpShape &shape, StaticLevelData *data, int moduleNumber = 3)
{
	shape.Finish();
	StaticLevelInput const input = { (const shapeheader *)&shape.header, { 1000, 2000, 3000 }, moduleNumber };
	return StaticLevel_Work(MakeSetup(&input, 1), data);
}

static bool RefusedFor(const StaticLevelData &data, const char *reason)
{
	return data.refused && strcmp(data.refused, reason) == 0;
}

static bool IndicesAre(const StaticLevelData &data, size_t first, uint32_t a, uint32_t b, uint32_t c)
{
	return data.indices.size() >= first + 3 && data.indices[first] == a && data.indices[first + 1] == b && data.indices[first + 2] == c;
}

// ---- what comes out for good shapes ----

static void CheckAQuad()
{
	MadeUpShape shape;
	shape.AddQuad(kTextureA);

	StaticLevelData data;
	data.Clear();
	Check(BuildOne(shape, &data), "a plain quad is built");
	Check(!data.refused, "and nothing is refused");
	Check(data.vertices.size() == 4 && data.indices.size() == 6, "four corners and two triangles");

	StaticVertex const &second = data.vertices[1];
	Check(second.x == 100.0f + 1000.0f && second.y == 2000.0f && second.z == 3000.0f, "a corner is the point plus the shape's place");
	Check(data.vertices[2].y == 100.0f + 2000.0f, "with y not turned over");
	Check(second.u == 30.0f * (1.0f / 256.0f) && second.v == 40.0f * (1.0f / 128.0f), "texture coordinates are the game's sum");
	Check(second.nx == 0.0f && second.ny == 0.1f && second.nz == 1.0f, "each corner asks for its own normal");
	Check(second.tx == 1.0f && second.ty == 0.0f && second.tz == 0.0f && second.flip == -1.0f, "the tangent is copied, flip and all");

	Check(IndicesAre(data, 0, 0, 1, 2) && IndicesAre(data, 3, 0, 2, 3), "the game's two triangles, folded along the same line");

	int count = 0;
	StaticRange const *ranges = data.RangesOf(3, &count);
	Check(ranges && count == 1, "the module has one range");
	Check(ranges && ranges[0].texture == kTextureA && ranges[0].baseVertex == 0 && ranges[0].firstVertex == 0 && ranges[0].vertexCount == 4
		&& ranges[0].firstIndex == 0 && ranges[0].triangles == 2 && !ranges[0].twoSided, "and its numbers are right");

	Check(data.modulesIn == 1 && data.polygonsIn == 1 && data.polygonsLeftOut == 0, "the counts for the log are right");
}

static void CheckTrianglesAndWinding()
{
	MadeUpShape shape;
	shape.AddTriangle(kTextureA);

	StaticLevelData data;
	data.Clear();
	BuildOne(shape, &data);
	Check(IndicesAre(data, 0, 0, 1, 2) && data.indices.size() == 3, "a triangle agreeing with its normal keeps its order, unlike CreateIndices");

	MadeUpShape turnedTriangle;
	turnedTriangle.AddTriangle(kTextureA);
	turnedTriangle.normals[0].vz = -ONE_FIXED;
	BuildOne(turnedTriangle, &data);
	Check(IndicesAre(data, 0, 0, 2, 1), "a triangle whose stored normal disagrees is turned round");

	MadeUpShape turnedQuad;
	turnedQuad.AddQuad(kTextureA);
	turnedQuad.normals[0].vz = -ONE_FIXED;
	BuildOne(turnedQuad, &data);
	Check(IndicesAre(data, 0, 0, 2, 1) && IndicesAre(data, 3, 0, 3, 2), "a quad is turned round triangle by triangle, keeping the line it folds along");

	// a first triangle with no area mustn't decide it: the first three corners are in a line
	MadeUpShape line;
	line.points[1].vx = 50;
	line.points[2].vx = 100;
	line.points[2].vy = 0;
	line.AddQuad(kTextureA);
	line.normals[0].vz = -ONE_FIXED;
	BuildOne(line, &data);
	Check(IndicesAre(data, 3, 0, 3, 2), "the way round is decided by the whole polygon, not its first three corners");
}

static void CheckTwoSided()
{
	MadeUpShape byFlag;
	byFlag.AddQuad(kTextureA, iflag_no_bfc);

	StaticLevelData data;
	data.Clear();
	BuildOne(byFlag, &data);

	int count = 0;
	StaticRange const *ranges = data.RangesOf(3, &count);
	Check(count == 1 && ranges[0].twoSided, "a polygon the game shows both sides of is marked");

	MadeUpShape noNormal;
	noNormal.AddQuad(kTextureA);
	noNormal.normals[0].vz = 0;
	BuildOne(noNormal, &data);
	ranges = data.RangesOf(3, &count);
	Check(count == 1 && ranges[0].twoSided, "so is one with no stored normal, because the game's test passes it both ways");

	MadeUpShape disagreeing;
	disagreeing.AddQuad(kTextureA, iflag_no_bfc);
	disagreeing.normals[0].vz = -ONE_FIXED;
	BuildOne(disagreeing, &data);
	Check(IndicesAre(data, 0, 0, 1, 2), "a two-sided polygon isn't turned round");

	MadeUpShape mixed;
	mixed.AddQuad(kTextureA);
	mixed.AddQuad(kTextureA, iflag_no_bfc);
	mixed.AddQuad(kTextureB, iflag_no_bfc);
	BuildOne(mixed, &data);
	Check(data.vertices[0].nx == 0.1f && data.vertices[4].nx == 0.2f && data.vertices[8].nx == 0.0f, "two-sided polygons come first, then by texture");

	ranges = data.RangesOf(3, &count);
	Check(count == 3 && ranges[0].twoSided && ranges[1].twoSided && !ranges[2].twoSided, "and they don't share a range with one-sided polygons");

	MadeUpShape side;
	side.AddQuad(kTextureA);
	side.AddQuad(kTextureA, iflag_no_bfc);
	BuildOne(side, &data);
	ranges = data.RangesOf(3, &count);
	Check(count == 2 && ranges[0].twoSided && !ranges[1].twoSided, "even with the same texture, right next to each other");
}

static void CheckWhatsLeftOut()
{
	MadeUpShape shape;
	shape.AddQuad(kTextureA);
	int const seeThrough = shape.AddQuad(kTextureA, iflag_transparent);
	int const hidden = shape.AddQuad(kTextureA, iflag_notvis);
	int const animated = shape.AddQuad(kTextureA, iflag_txanim);
	int const corona = shape.AddQuad(kTextureA, iflag_light_corona);
	int const line = shape.AddPolygon(I_Polyline, kTextureA, 0, 4);
	int const flat = shape.AddPolygon(I_ZB_Polygon, kTextureA, 0, 4);
	int const plain = shape.AddPolygon(I_ZB_3dTexturedPolygon, kTextureA, 0, 4);
	int const flatTextured = shape.AddPolygon(I_ZB_Gouraud2dTexturedPolygon, kTextureA, 0, 4);

	StaticLevelData data;
	data.Clear();
	Check(BuildOne(shape, &data), "a shape with polygons that stay on the old route is built");
	Check(data.PolygonIsIn(3, 0), "a solid polygon is in");
	Check(!data.PolygonIsIn(3, seeThrough), "a see-through polygon isn't");
	Check(!data.PolygonIsIn(3, hidden), "a hidden polygon isn't");
	Check(!data.PolygonIsIn(3, animated), "an animated polygon isn't");
	Check(!data.PolygonIsIn(3, corona), "a light's corona isn't");
	Check(!data.PolygonIsIn(3, line), "a type the game doesn't draw isn't");
	Check(!data.PolygonIsIn(3, flat), "nor is another");
	Check(!data.PolygonIsIn(3, plain), "nor is a third");
	Check(data.PolygonIsIn(3, flatTextured), "the third type the game does draw is in");
	Check(data.polygonsIn == 2 && data.polygonsLeftOut == 7 && data.vertices.size() == 8, "and the counts agree");
}

static void CheckNoTexture()
{
	MadeUpShape shape;
	shape.AddPolygon(I_ZB_GouraudPolygon, 0, 5000, 4);

	StaticLevelData data;
	data.Clear();
	Check(BuildOne(shape, &data), "a polygon with no texture needs no texture coordinates");

	int count = 0;
	StaticRange const *ranges = data.RangesOf(3, &count);
	Check(count == 1 && ranges[0].texture == kNoTexture, "it uses the game's no-texture picture");
	Check(data.vertices[2].u == 0.0f && data.vertices[2].v == 0.0f, "and its coordinates are zero, as the game's are");
}

static void CheckBakedColours()
{
	MadeUpShape shape;
	shape.BakeColours();
	shape.AddQuad(kTextureA);

	StaticLevelData data;
	data.Clear();
	Check(BuildOne(shape, &data), "a shape with baked colours is built");
	Check(data.colours.size() == data.vertices.size(), "with one colour for each corner");
	Check(data.colours[0] == 0xFF104070u && data.colours[3] == 0xFF134373u, "each the colour of its own point, with red, green and blue the right way round, and alpha saying it takes live light");

	MadeUpShape plain;
	plain.AddQuad(kTextureA);
	BuildOne(plain, &data);
	Check(data.colours.size() == 4 && data.colours[1] == 0xFF000000u, "a shape with no baked colours starts from black");

	MadeUpShape glowing;
	glowing.BakeColours();
	glowing.AddQuad(kTextureA, iflag_nolight);
	BuildOne(glowing, &data);
	Check(data.colours[2] == 0x00FFFFFFu, "a polygon that takes no light is white, with alpha saying so");

	MadeUpShape turned;
	turned.BakeColours();
	turned.AddQuad(kTextureA);
	turned.polygons[0].corners[0] = 3;
	turned.polygons[0].corners[3] = 0;
	BuildOne(turned, &data);
	Check(data.colours[0] == 0xFF134373u, "the colour follows the corner's point, not its place in the polygon");

	MadeUpShape noTexture;
	noTexture.BakeColours();
	noTexture.AddPolygon(I_ZB_GouraudPolygon, 0, 0, 4);
	BuildOne(noTexture, &data);

	uint32_t const average = (0x10 + 0x40 + 0x70) / 3;
	Check(data.colours[0] == (0xFF000000u | (average << 16) | (average << 8)), "a polygon with no texture is the game's yellow");
	Check(!data.ModuleIsWhole(3) && data.PolygonIsIn(3, 0), "it's in the list for the pictures, but its module stays on the old route, because the game gives it no gamma");

	MadeUpShape noTextureNoLight;
	noTextureNoLight.BakeColours();
	noTextureNoLight.AddPolygon(I_ZB_GouraudPolygon, 0, 0, 4, iflag_nolight);
	BuildOne(noTextureNoLight, &data);
	Check(data.colours[0] == (0xFF000000u | (average << 16) | (average << 8)), "and it takes no notice of the no-light flag, as the game doesn't");

	MadeUpShape full;
	full.BakeColours();
	full.AddQuad(kTextureA);
	full.Finish();
	Check(StaticLevel_ShapeHasItsColours((const shapeheader *)&full.header), "a shape with a colour for every point has its colours");

	full.header.sh_numextraitems = full.header.numpoints - 1;
	Check(!StaticLevel_ShapeHasItsColours((const shapeheader *)&full.header), "one short doesn't");

	full.header.sh_numextraitems = full.header.numpoints;
	full.header.sh_extraitemdata = 0;
	Check(!StaticLevel_ShapeHasItsColours((const shapeheader *)&full.header), "a baked shape with no list doesn't");

	MadeUpShape unbaked;
	unbaked.AddQuad(kTextureA);
	unbaked.Finish();
	Check(StaticLevel_ShapeHasItsColours((const shapeheader *)&unbaked.header), "a shape that isn't baked needs none");
}

static void CheckWholeModules()
{
	MadeUpShape all;
	all.AddQuad(kTextureA);
	all.AddQuad(kTextureB);

	StaticLevelData data;
	data.Clear();
	BuildOne(all, &data);
	Check(data.ModuleIsWhole(3), "a module with everything in the list is whole");
	Check(!data.ModuleIsWhole(4), "a module not in the list isn't");
	Check(!data.ModuleIsWhole(-1), "an object asking with -1 isn't");
	Check(!data.ModuleIsWhole(20), "a number past the end isn't");

	MadeUpShape hidden;
	hidden.AddQuad(kTextureA);
	hidden.AddQuad(kTextureA, iflag_notvis);
	hidden.AddPolygon(I_Polyline, kTextureA, 0, 4);
	BuildOne(hidden, &data);
	Check(data.ModuleIsWhole(3), "what the game never draws doesn't stop a module being whole");

	int const flags[3] = { iflag_transparent, iflag_txanim, iflag_light_corona };

	for (int i = 0; i < 3; i++)
	{
		MadeUpShape part;
		part.AddQuad(kTextureA);
		part.AddQuad(kTextureA, flags[i]);
		BuildOne(part, &data);
		Check(!data.ModuleIsWhole(3), "a module with something drawn that isn't in the list isn't whole");
	}
}

static void CheckOrderAndRanges()
{
	MadeUpShape first, second;
	first.AddQuad(kTextureB);
	first.AddQuad(kTextureA);
	first.AddQuad(kTextureB);
	second.AddQuad(kTextureA);
	first.Finish();
	second.Finish();

	StaticLevelInput const inputs[2] = { { (const shapeheader *)&first.header, { 0, 0, 0 }, 7 }, { (const shapeheader *)&second.header, { 0, 0, 0 }, 2 } };

	StaticLevelData data;
	data.Clear();
	Check(StaticLevel_Work(MakeSetup(inputs, 2), &data), "two modules are built");

	int count = 0;
	StaticRange const *ranges = data.RangesOf(7, &count);
	Check(count == 2 && ranges[0].texture == kTextureA && ranges[1].texture == kTextureB, "a module's ranges are in texture order");
	Check(count == 2 && ranges[1].triangles == 4 && ranges[1].vertexCount == 8, "polygons of one texture join, even with another between them in the shape");
	Check(count == 2 && ranges[0].firstVertex == 0 && ranges[0].firstIndex == 0, "the first texture of the first module comes first");

	StaticRange const *other = data.RangesOf(2, &count);
	Check(count == 1 && other[0].texture == kTextureA && other[0].firstVertex == 4 && other[0].firstIndex == 6, "the same texture in the next module comes straight after it, so the two can join");

	Check(!data.RangesOf(5, &count) && count == 0, "a module with nothing in the list has no ranges");
	Check(data.moduleEntries.size() == 20, "every module number has an entry");
}

static void CheckSmallerIndices()
{
	MadeUpShape shape;

	for (int i = 0; i < 10; i++)
	{
		shape.AddQuad(kTextureA);
	}

	shape.Finish();
	StaticLevelInput const input = { (const shapeheader *)&shape.header, { 0, 0, 0 }, 1 };
	StaticLevelSetup setup = MakeSetup(&input, 1);
	setup.mostVerticesForOneBase = 16;

	StaticLevelData data;
	data.Clear();
	Check(StaticLevel_Work(setup, &data), "a list too long for one base is built");
	Check(data.vertices.size() == 40, "with every corner in it");

	int count = 0;
	StaticRange const *ranges = data.RangesOf(1, &count);
	Check(count == 3, "split into ranges with a base each");

	bool allReach = true, allStartRight = true;
	uint32_t trianglesSeen = 0;

	for (int i = 0; i < count; i++)
	{
		allStartRight = allStartRight && ranges[i].baseVertex == (uint32_t)i * 16 && ranges[i].firstVertex == 0;
		trianglesSeen += ranges[i].triangles;

		for (uint32_t j = 0; j < ranges[i].triangles * 3; j++)
		{
			allReach = allReach && data.indices[ranges[i].firstIndex + j] < 16;
		}
	}

	Check(allStartRight, "each base starts where the last one's corners end");
	Check(allReach, "no index reaches past what one base can");
	Check(trianglesSeen == 20, "and no triangle is lost");

	setup.mostVerticesForOneBase = 7;
	Check(StaticLevel_Work(setup, &data), "a base with room left over is built");

	ranges = data.RangesOf(1, &count);
	Check(count == 10 && ranges[1].baseVertex == 4, "and a polygon that wouldn't fit in what's left goes to the next base");

	setup.mostVerticesForOneBase = 3;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "a base that can't hold a polygon at all is refused");
}

static void CheckTrianglesInOneDraw()
{
	MadeUpShape shape;

	for (int i = 0; i < 5; i++)
	{
		shape.AddQuad(kTextureA);
	}

	shape.Finish();
	StaticLevelInput const input = { (const shapeheader *)&shape.header, { 0, 0, 0 }, 1 };
	StaticLevelSetup setup = MakeSetup(&input, 1);
	setup.mostTrianglesInOneRange = 4;

	StaticLevelData data;
	data.Clear();
	Check(StaticLevel_Work(setup, &data), "a range longer than the card can draw at once is built");

	int count = 0;
	StaticRange const *ranges = data.RangesOf(1, &count);
	Check(count == 3 && ranges[0].triangles == 4 && ranges[1].triangles == 4 && ranges[2].triangles == 2, "and it becomes several, each as long as the card allows");
	Check(ranges[1].firstVertex == 8 && ranges[1].firstIndex == 12 && ranges[1].baseVertex == 0, "each carrying on where the last stopped");

	setup.mostTrianglesInOneRange = 1;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "a card that can't draw a quad is refused");
}

static void CheckModulesThatSitStill()
{
	Check(StaticLevel_ModuleSitsStill(0, 0, 0, 0), "an ordinary module sits where its points say");
	Check(StaticLevel_ModuleSitsStill(ObFlag_NotVis, 0, 0, 0), "other flags don't matter here");
	Check(!StaticLevel_ModuleSitsStill(ObFlag_ArbRot, 0, 0, 0), "a module placed by the camera doesn't");
	Check(!StaticLevel_ModuleSitsStill(0, 1, 0, 0), "a module turned round x doesn't");
	Check(!StaticLevel_ModuleSitsStill(0, 0, -1, 0), "a module turned round y doesn't");
	Check(!StaticLevel_ModuleSitsStill(0, 0, 0, 4095), "a module turned round z doesn't");
}

static void CheckLimits()
{
	MadeUpShape shape;
	shape.AddQuad(kTextureA);
	shape.Finish();

	// the same polygon many times over: this reaches the limit without many real polygons
	std::vector<int *> many(250000, shape.items[0]);
	std::vector<SurfaceTangent> tangents(250001, shape.tangents[0]);
	shape.header.items = &many[0];
	shape.header.numitems = 250000;
	shape.header.sh_surfacetangents = &tangents[0];

	StaticLevelInput const input = { (const shapeheader *)&shape.header, { 0, 0, 0 }, 1 };
	StaticLevelData data;
	data.Clear();
	Check(StaticLevel_Work(MakeSetup(&input, 1), &data), "exactly as many corners as the limit is built");
	Check(data.vertices.size() == 1000000, "and they're all there");

	many.push_back(shape.items[0]);
	shape.header.items = &many[0];
	shape.header.numitems = 250001;
	Check(!StaticLevel_Work(MakeSetup(&input, 1), &data) && RefusedFor(data, "the level has too many corners"), "one polygon over the limit is refused");
	Check(data.vertices.capacity() == 0, "and nothing is left allocated");

	// hidden polygons take no corners, but they still count, and a shape can't claim more of them than the limit
	MadeUpShape hidden;
	hidden.AddQuad(kTextureA, iflag_notvis);
	hidden.Finish();

	std::vector<int *> lots(1 << 20, hidden.items[0]);
	hidden.header.items = &lots[0];
	hidden.header.numitems = 1 << 20;

	StaticLevelInput const fine[4] = { { (const shapeheader *)&hidden.header, { 0, 0, 0 }, 1 }, { (const shapeheader *)&hidden.header, { 0, 0, 0 }, 2 },
		{ (const shapeheader *)&hidden.header, { 0, 0, 0 }, 3 }, { (const shapeheader *)&hidden.header, { 0, 0, 0 }, 4 } };
	Check(!StaticLevel_Work(MakeSetup(fine, 4), &data) && RefusedFor(data, "the level has too many polygons"), "too many polygons in all is refused");
}

// ---- hostile shapes ----

static int hostileCases = 0;

// Every one of these has to be refused, for this reason and no other, and leave nothing behind. The reason matters: the builder has safety nets behind
// its checks, and without it a net could catch a shape after the check that should have stopped it had already read something it shouldn't.
static void CheckRefused(MadeUpShape &shape, const char *reason, const char *what, bool alreadyFinished = false)
{
	if (!alreadyFinished)
	{
		shape.Finish();
	}

	StaticLevelInput const input = { (const shapeheader *)&shape.header, { 0, 0, 0 }, 3 };
	StaticLevelData data;
	data.Clear();

	bool const built = StaticLevel_Work(MakeSetup(&input, 1), &data);
	int count = 0;

	Check(!built && data.refused != 0 && strcmp(data.refused, reason) == 0 && data.vertices.empty() && data.indices.empty() && data.ranges.empty() && !data.RangesOf(3, &count) && !data.PolygonIsIn(3, 0), what);
	hostileCases++;
}

static void CheckHostilePolygons()
{
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].corners[2] = 4;
		CheckRefused(s, "a polygon's corners are bad", "a corner number one past the points");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].corners[1] = -2;
		CheckRefused(s, "a polygon's corners are bad", "a negative corner number");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].corners[4] = 0;
		CheckRefused(s, "a polygon's corners are bad", "five corners, so no end mark, and the read would run into the next polygon");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].corners[2] = kTerminationMarker;
		CheckRefused(s, "a polygon's corners are bad", "only two corners");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].corners[0] = kTerminationMarker;
		CheckRefused(s, "a polygon's corners are bad", "no corners at all");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].normalIndex = -3;
		CheckRefused(s, "a polygon's normal number is out of range", "a negative normal number");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].normalIndex = 1;
		CheckRefused(s, "a polygon's normal number is out of range", "a normal number that would read past the end of the list");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].normalIndex = 2147483647;
		CheckRefused(s, "a polygon's normal number is out of range", "the largest normal number there is");
	}
	{
		MadeUpShape s;
		s.AddQuad(55);
		CheckRefused(s, "a polygon's texture doesn't exist", "a texture that doesn't exist");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureNoWidth);
		CheckRefused(s, "a polygon's texture doesn't exist", "a texture with no width, which would be a division by zero");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureNoHeight);
		CheckRefused(s, "a polygon's texture doesn't exist", "a texture with no height");
	}
	{
		MadeUpShape s;
		s.AddPolygon(I_ZB_Gouraud3dTexturedPolygon, kTextureA, 1, 4);
		s.AddList(4);
		CheckRefused(s, "a polygon's texture coordinate list is out of range", "a texture coordinate list number one past the end");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.polygons[0].colour |= 0x80000000;
		CheckRefused(s, "a polygon's texture coordinate list is out of range", "a texture coordinate list number that's negative");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.lists[0].clear();
		CheckRefused(s, "a polygon's texture coordinate list is too short", "a texture coordinate list that's empty");
	}
	{
		MadeUpShape s;
		int const list = s.AddList(3);
		s.AddPolygon(I_ZB_Gouraud3dTexturedPolygon, kTextureA, list, 4);
		CheckRefused(s, "a polygon's texture coordinate list is too short", "a texture coordinate list one corner too short");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.lists[0][3] = 2147483647;
		CheckRefused(s, "a polygon's texture coordinate is out of range", "a huge texture coordinate");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.lists[0][7] = -(1 << 24) - 1;
		CheckRefused(s, "a polygon's texture coordinate is out of range", "a texture coordinate just past the limit, on the last number of the polygon");
	}
	{
		MadeUpShape s;
		int const shared = s.AddList(4);
		s.AddPolygon(I_ZB_Gouraud3dTexturedPolygon, kTextureA, shared, 4);
		s.AddPolygon(I_ZB_Gouraud3dTexturedPolygon, kTextureA, shared, 4, iflag_txanim);
		CheckRefused(s, "a polygon shares an animated texture's list", "a plain polygon sharing a list with an animated one that comes after it");
	}
	{
		MadeUpShape s;
		int const shared = s.AddList(4);
		s.AddPolygon(I_ZB_Gouraud3dTexturedPolygon, kTextureA, shared, 4, iflag_txanim);
		s.AddPolygon(I_ZB_Gouraud3dTexturedPolygon, kTextureA, shared, 4);
		CheckRefused(s, "a polygon shares an animated texture's list", "and one that comes before it");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.tangents[0].x = std::numeric_limits<float>::quiet_NaN();
		CheckRefused(s, "a polygon has a number that isn't sensible", "a tangent that isn't a real number");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.tangents[0].flip = std::numeric_limits<float>::infinity();
		CheckRefused(s, "a polygon has a number that isn't sensible", "a tangent that's infinite, on its last number");
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.points[2].vx = 2000000000;
		CheckRefused(s, "a polygon has a number that isn't sensible", "a corner too far away to be sensible");
	}

	normalToGive = std::numeric_limits<float>::quiet_NaN();
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		CheckRefused(s, "a polygon has a number that isn't sensible", "a normal that isn't a real number");
	}
	normalToGive = 1.0f;
}

static void CheckHostileShapes()
{
	{
		MadeUpShape s;
		s.BakeColours();
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_extraitemdata = 0;
		CheckRefused(s, "a shape's baked colours are too short", "a shape that says it's baked, with no colours", true);
	}
	{
		MadeUpShape s;
		s.BakeColours();
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_numextraitems = s.header.numpoints - 1;
		CheckRefused(s, "a shape's baked colours are too short", "one baked colour fewer than the shape has points", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.numitems = -1;
		CheckRefused(s, "a shape's counts are out of range", "a negative polygon count", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.numitems = (1 << 20) + 1;
		CheckRefused(s, "a shape's counts are out of range", "a polygon count over the limit, which has to be caught before anything is read", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.numpoints = -1;
		CheckRefused(s, "a shape's counts are out of range", "a negative point count", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.numpoints = maxrotpts + 1;
		CheckRefused(s, "a shape's counts are out of range", "more points than the game can turn", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.items = 0;
		CheckRefused(s, "a shape is missing its lists", "no polygon list", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.items[0] = 0;
		CheckRefused(s, "a shape has a polygon missing", "a polygon that isn't there", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.points = 0;
		CheckRefused(s, "a shape is missing its lists", "no point list", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.pointsPointer = 0;
		CheckRefused(s, "a shape is missing its lists", "a point list that's empty", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_normals = 0;
		CheckRefused(s, "a shape is missing its lists", "no normal list", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.normalsPointer = 0;
		CheckRefused(s, "a shape is missing its lists", "a normal list that's empty", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_numuvlists = 0;
		CheckRefused(s, "a polygon's texture coordinate list is out of range", "a shape that says it has no texture coordinate lists", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_numuvlists = -1;
		CheckRefused(s, "a shape's texture coordinate lists don't add up", "a negative count of lists", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_numuvlists = (1 << 20) + 1;
		CheckRefused(s, "a shape's texture coordinate lists don't add up", "a count of lists over the limit", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_uvlistlengths = 0;
		CheckRefused(s, "a shape's texture coordinate lists don't add up", "lists with no record of how long they are", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.listPointers[0] = 0;
		CheckRefused(s, "a polygon's texture coordinate list is too short", "a list with a good length and nothing there", true);
	}
	{
		MadeUpShape s;
		s.AddQuad(kTextureA);
		s.Finish();
		s.header.sh_textures = 0;
		CheckRefused(s, "a shape's texture coordinate lists don't add up", "a count of lists with no lists", true);
	}
}

static void CheckBadSetups()
{
	MadeUpShape shape;
	shape.AddQuad(kTextureA);
	shape.Finish();

	StaticLevelInput input = { (const shapeheader *)&shape.header, { 0, 0, 0 }, 3 };
	StaticLevelData data;
	data.Clear();

	input.shape = 0;
	Check(!StaticLevel_Work(MakeSetup(&input, 1), &data) && RefusedFor(data, "a module has no shape"), "a module with no shape is refused");
	input.shape = (const shapeheader *)&shape.header;

	input.moduleNumber = -1;
	Check(!StaticLevel_Work(MakeSetup(&input, 1), &data) && RefusedFor(data, "a module's number is out of range"), "a negative module number is refused");

	input.moduleNumber = 20;
	Check(!StaticLevel_Work(MakeSetup(&input, 1), &data) && RefusedFor(data, "a module's number is out of range"), "a module number one past the end is refused");

	input.moduleNumber = 3;
	StaticLevelInput const twice[2] = { input, input };
	Check(!StaticLevel_Work(MakeSetup(twice, 2), &data) && RefusedFor(data, "two modules have the same number"), "two modules with one number are refused");

	StaticLevelSetup setup = MakeSetup(&input, 1);
	setup.textureSize = 0;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "no way to ask a texture's size is refused");

	setup = MakeSetup(&input, 1);
	setup.cornerNormal = 0;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "no way to ask for a normal is refused");

	setup = MakeSetup(&input, 1);
	setup.numModules = -1;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "a negative count of modules is refused");

	setup = MakeSetup(0, 1);
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "a count of modules with no list is refused");

	setup = MakeSetup(&input, 1);
	setup.moduleNumbers = (1 << 20) + 1;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "too many module numbers is refused before anything is allocated");

	setup = MakeSetup(&input, 1);
	setup.mostVerticesForOneBase = 0;
	Check(!StaticLevel_Work(setup, &data) && RefusedFor(data, "the builder was set up wrongly"), "a base that holds nothing is refused");

	Check(data.refused && data.refusedModule == -1, "a refusal says why");
}

static void CheckRefusalsArePlaced()
{
	MadeUpShape s;
	s.AddQuad(kTextureA);
	s.AddQuad(kTextureA);
	s.polygons[1].corners[0] = 9;

	StaticLevelData data;
	data.Clear();
	BuildOne(s, &data);
	Check(data.refusedModule == 3 && data.refusedPolygon == 1, "a refusal says which module and which polygon, for the log");
}

static void CheckStartingAgain()
{
	MadeUpShape bad;
	bad.AddQuad(55);

	StaticLevelData data;
	data.Clear();
	BuildOne(bad, &data);

	MadeUpShape good;
	good.AddQuad(kTextureA);
	Check(BuildOne(good, &data) && !data.refused, "a refusal leaves nothing behind to spoil the next build");

	Check(BuildOne(good, &data) && data.vertices.size() == 4 && data.polygonsIn == 1, "and building again starts clean, adding nothing to the last");
}

static void CheckAsking()
{
	StaticLevelData empty;
	empty.Clear();

	int count = 5;
	Check(!empty.RangesOf(0, &count) && count == 0, "asking an empty list for ranges is safe");
	Check(!empty.PolygonIsIn(0, 0), "and asking for a polygon");

	MadeUpShape shape;
	shape.AddQuad(kTextureA);

	StaticLevelData data;
	data.Clear();
	BuildOne(shape, &data);

	Check(!data.RangesOf(-1, &count) && count == 0, "an object passes -1 for its module, and that has no ranges");
	Check(!data.PolygonIsIn(-1, 0), "and no polygons");
	Check(!data.RangesOf(20, &count), "a module number past the end has no ranges");
	Check(!data.PolygonIsIn(20, 0), "and no polygons");
	Check(!data.PolygonIsIn(3, -1), "a negative polygon number isn't in");
	Check(!data.PolygonIsIn(3, 1), "nor is one past the end of the shape");
	Check(data.PolygonIsIn(3, 0), "and the real one still is");

	// each module's bits have to be its own: a polygon of the first module mustn't answer for the second
	MadeUpShape first, second;
	first.AddQuad(kTextureA);
	first.AddQuad(kTextureA, iflag_notvis);
	second.AddQuad(kTextureA, iflag_notvis);
	second.AddQuad(kTextureA);
	first.Finish();
	second.Finish();

	StaticLevelInput const inputs[2] = { { (const shapeheader *)&first.header, { 0, 0, 0 }, 4 }, { (const shapeheader *)&second.header, { 0, 0, 0 }, 9 } };
	StaticLevel_Work(MakeSetup(inputs, 2), &data);

	Check(data.PolygonIsIn(4, 0) && !data.PolygonIsIn(4, 1), "the first module's polygons answer for themselves");
	Check(!data.PolygonIsIn(9, 0) && data.PolygonIsIn(9, 1), "and so do the second's");

	MadeUpShape one, next;
	one.AddQuad(kTextureA);
	next.AddQuad(kTextureA);
	one.Finish();
	next.Finish();

	StaticLevelInput const neighbours[2] = { { (const shapeheader *)&one.header, { 0, 0, 0 }, 4 }, { (const shapeheader *)&next.header, { 0, 0, 0 }, 9 } };
	StaticLevel_Work(MakeSetup(neighbours, 2), &data);

	Check(data.PolygonIsIn(4, 0) && data.PolygonIsIn(9, 0) && !data.PolygonIsIn(4, 1), "one past a module's end doesn't read the next module's answer");
}

static void CheckManyPolygonBits()
{
	// past one word of bits, and past a whole number of words
	MadeUpShape shape;

	for (int i = 0; i < 70; i++)
	{
		shape.AddQuad(kTextureA, (i % 3 == 0) ? iflag_notvis : 0);
	}

	StaticLevelData data;
	data.Clear();
	BuildOne(shape, &data);

	bool allRight = true;

	for (int i = 0; i < 70; i++)
	{
		allRight = allRight && data.PolygonIsIn(3, i) == (i % 3 != 0);
	}

	Check(allRight, "every polygon's answer is its own, across many words");
}

int main()
{
	CheckAQuad();
	CheckTrianglesAndWinding();
	CheckTwoSided();
	CheckWhatsLeftOut();
	CheckNoTexture();
	CheckBakedColours();
	CheckWholeModules();
	CheckOrderAndRanges();
	CheckSmallerIndices();
	CheckTrianglesInOneDraw();
	CheckModulesThatSitStill();
	CheckLimits();
	CheckHostilePolygons();
	CheckHostileShapes();
	CheckBadSetups();
	CheckRefusalsArePlaced();
	CheckStartingAgain();
	CheckAsking();
	CheckManyPolygonBits();

	printf("  (%d hostile shapes tried)\n", hostileCases);

	if (failures)
	{
		printf("FAILED: %d checks\n", failures);
		return 1;
	}

	printf("The static level test passed\n");
	return 0;
}
