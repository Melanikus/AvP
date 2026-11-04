// Rebecca's test: the first unit test in this repository.
// Written 2026-11-03 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A level file is a tree of chunks. Each object in a level is an RBOBJECT chunk, and inside it an OBJHEAD1 chunk holds
// the object's fixed data and then its name. The game reads these with C++ classes, Object_Chunk and
// Object_Header_Chunk, and fills in a ChunkObject struct. A level file can hold anything, so that reading has to stay
// safe whatever it finds.
//
// This test builds the bytes of an object in memory, hands them to the game's real constructors and looks at the real
// ChunkObject that comes out. So it covers the whole path, not a copy of any of it: the walk through the child chunks,
// the check that a child's size can be real (ChildChunkFits), the copy of the name and the layout of the struct.
//
// The game once kept names in 50 bytes, which is 49 characters, and copied them with no limit at all. A name that was
// too long ran past the end of that space, into whatever came next in memory. One stock level really has a name that
// long, at 53 characters, and a crafted file could have done far worse. Names now get 64 bytes. One byte is the zero
// byte that marks the end of a name, so the longest name that fits is 63 characters.
//
// Right after the name in ChunkObject comes index_num, which is filled in before the name is copied. So a name that
// runs past its end lands on a real field, and these tests would see index_num change.
//
// After a level is read, the game joins each object to its shape, using numbers from the file. So tests 7 and 8 build
// real objects and shapes in a real File_Chunk, run the real post_input_processing, and check that a level that doesn't
// join up is refused. Test 9 does the same for an object's baked colours: how many there are also comes from the file.
// Test 10 does the same for the corners of a polygon, and test 11 for sub-shapes and morphing.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file together with the game's real chunk code, using the settings in
// tests/rebeccas_test.args, which are the game's own. The program prints a line for each check, and gives back a
// non-zero result if any check failed. The repository's automatic checks run it, and a change that breaks it can't be
// merged. It needs none of the game's files: those aren't in this repository, and it never opens a file.

#include <algorithm>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "chunk.hpp"
#include "chunk_type.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "chunk_light.hpp"
#include "hierarchy_chunk.hpp"
#include "mishchnk.hpp"

// The chunk code can open files, through these two functions from another part of the game. This test only gives it
// bytes in memory, so they are never meant to run, and they answer that there is no such file.
HANDLE avp_CreateFile(const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
{
	return INVALID_HANDLE_VALUE;
}

DWORD avp_GetFileAttributes(const char *)
{
	return INVALID_FILE_ATTRIBUTES;
}

// Checks the compiler makes while building, before anything runs. If one of these fails, this file doesn't compile.
static_assert(sizeof(ChunkObject::o_name) == 64, "the name space must hold 63 characters and a zero byte");
static_assert(offsetof(ChunkObject, index_num) == offsetof(ChunkObject, o_name) + 64, "index_num must come right after the name");

// A name of exactly the longest length that fits. The text itself doesn't matter, only its length.
static char const LongestName[] = "she doesnt have bad dreams because shes just a piece of plastic";
static_assert(sizeof(LongestName) - 1 == 63, "the longest name that fits is 63 characters");

// The stock level's name starts with this many characters of the text above.
static size_t const OrdinaryCharactersInStockName = 52;

// How an OBJHEAD1 chunk is laid out: a header of an 8-byte name and a 4-byte size, then 60 bytes of fixed data with
// the object's number at byte 48, and then the name.
static size_t const ChunkHeaderBytes = 12;
static size_t const FixedDataBytes = 60;
static size_t const ObjectNumberAt = 48;

// A number with no zero byte in it, so that any byte written over it by a name that runs too far shows up.
static int const ObjectNumber = 0x01020304;

// Where the bytes of a made-up object are built. There is always room after them, full of the letter Z, so any byte
// read from past the end of a chunk shows up in the result. A zero byte further along stops such a read going far.
alignas(int) static char objectBytes[512];

static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

// Stands in for the file being read: the real chunks report problems to it, through Parent_File, and it owns the
// objects made from it, so they are given back when it goes away.
class TestFile : public Chunk_With_Children
{
public:
	TestFile() : Chunk_With_Children(0, "REBINFF2")
	{
		Parent_File = this;
	}

	~TestFile()
	{
		Parent_File = 0;
	}
};

static void ClearObjectBytes()
{
	memset(objectBytes, 'Z', sizeof(objectBytes));
	objectBytes[sizeof(objectBytes) - 1] = '\0';
}

// Writes a chunk header at the given place: the 8-byte name and then the size, which counts the header itself.
static void PutChunkHeader(char * where, int size)
{
	memcpy(where, "OBJHEAD1", 8);
	memcpy(where + 8, &size, sizeof(size));
}

// Builds one OBJHEAD1 chunk at the start of objectBytes, with the fixed data and these bytes after it as the name.
// Gives back how many bytes the chunk takes up.
static size_t BuildObjectHeader(char const * name, size_t nameBytes)
{
	size_t const chunkBytes = ChunkHeaderBytes + FixedDataBytes + nameBytes;

	ClearObjectBytes();
	PutChunkHeader(objectBytes, (int)chunkBytes);
	memset(objectBytes + ChunkHeaderBytes, 0, FixedDataBytes);
	memcpy(objectBytes + ChunkHeaderBytes + ObjectNumberAt, &ObjectNumber, sizeof(ObjectNumber));
	memcpy(objectBytes + ChunkHeaderBytes + FixedDataBytes, name, nameBytes);

	return chunkBytes;
}

// Hands the bytes to the game's real constructor, which walks the child chunks and makes the real Object_Header_Chunk.
static Object_Chunk * LoadObject(TestFile & file, size_t bytes)
{
	return new Object_Chunk(&file, objectBytes, bytes);
}

// How long the object's name is. It stops at the end of the space, so a name with no zero byte can't send it wandering.
static size_t NameLength(Object_Chunk const * object)
{
	char const * name = object->object_data.o_name;
	void const * end = memchr(name, 0, sizeof(object->object_data.o_name));
	return end ? (size_t)((char const *)end - name) : sizeof(object->object_data.o_name);
}

static void CheckFieldsAfterTheName(Object_Chunk const * object)
{
	Check(object->object_data.index_num == ObjectNumber, "the number stored after the name is unchanged");
	Check(object->object_data.ID.id1 == 0 && object->object_data.ID.id2 == 0, "the ID stored after that is unchanged");
}

// 1. The bytes of the longest name in the stock levels: 52 ordinary characters, a byte with the value 0xC0, the zero
// byte, and two leftover bytes that fill out the chunk. Everything up to the zero byte is copied, 0xC0 included.
static void TestStockName()
{
	puts("1. the longest name in the stock levels, 53 characters");

	char name[OrdinaryCharactersInStockName + 4];
	char const endOfName[] = { '\xC0', '\0', '\x9E', '\x3D' };
	memcpy(name, LongestName, OrdinaryCharactersInStockName);
	memcpy(name + OrdinaryCharactersInStockName, endOfName, sizeof(endOfName));

	TestFile file;
	Object_Chunk * object = LoadObject(file, BuildObjectHeader(name, sizeof(name)));

	Check(object->get_header() != 0, "the header chunk was loaded");
	Check(NameLength(object) == 53, "the name is 53 long");
	Check(memcmp(object->object_data.o_name, name, 53) == 0, "all 53 characters are there, 0xC0 included");
	CheckFieldsAfterTheName(object);
}

// 2. The longest name that fits: 63 characters and the zero byte use the whole space, with nothing lost.
static void TestLongestName()
{
	puts("2. the longest name that fits, 63 characters");

	TestFile file;
	Object_Chunk * object = LoadObject(file, BuildObjectHeader(LongestName, sizeof(LongestName)));

	Check(NameLength(object) == 63, "the name is 63 long");
	Check(memcmp(object->object_data.o_name, LongestName, sizeof(LongestName)) == 0, "every character is there");
	CheckFieldsAfterTheName(object);
}

// 3. One character more than fits: the name is cut short, and still ends properly.
static void TestOneCharacterTooMany()
{
	puts("3. one character too many, 64 characters");

	char name[sizeof(LongestName) + 1];
	memcpy(name, LongestName, 63);
	name[63] = '!';
	name[64] = '\0';

	TestFile file;
	Object_Chunk * object = LoadObject(file, BuildObjectHeader(name, sizeof(name)));

	Check(NameLength(object) == 63, "the name is cut at 63");
	Check(memcmp(object->object_data.o_name, LongestName, 63) == 0, "the 63 characters are the right ones");
	CheckFieldsAfterTheName(object);
}

// 4. A name with no zero byte: the copy has to stop at the end of the chunk. The bytes after the chunk are all Z, so if
// it carried on, they would turn up in the name.
static void TestNoEndMarker()
{
	puts("4. no zero byte anywhere in the chunk");

	size_t const bytes = BuildObjectHeader(LongestName, 10);
	objectBytes[bytes + 20] = '\0';

	TestFile file;
	Object_Chunk * object = LoadObject(file, bytes);

	Check(NameLength(object) == 10, "the name stops at the end of the chunk");
	Check(memcmp(object->object_data.o_name, LongestName, 10) == 0, "the 10 characters are the right ones");
	CheckFieldsAfterTheName(object);
}

// 5. A chunk too short to hold a name: its data is the fixed part, or not even all of that. The name must come out
// empty, and the Z bytes where a name would be must not be read. Then the smallest chunk with room for a name.
static void TestChunkTooShort()
{
	puts("5. chunks with 60 bytes of data or less");

	size_t const tooShort[] = { 0, 1, 59, 60 };

	for (size_t i = 0; i < sizeof(tooShort) / sizeof(tooShort[0]); i++)
	{
		ClearObjectBytes();
		PutChunkHeader(objectBytes, (int)(ChunkHeaderBytes + tooShort[i]));
		objectBytes[ChunkHeaderBytes + FixedDataBytes + 20] = '\0';

		TestFile file;
		Object_Chunk * object = LoadObject(file, ChunkHeaderBytes + tooShort[i]);

		Check(object->get_header() != 0, "the header chunk was loaded");
		Check(NameLength(object) == 0, "the name is empty");
	}

	size_t const bytes = BuildObjectHeader(LongestName, 1);
	objectBytes[bytes + 20] = '\0';

	TestFile file;
	Object_Chunk * object = LoadObject(file, bytes);

	Check(NameLength(object) == 1, "61 bytes of data leave room for one character");
}

// 6. A child chunk whose size can't be real: too small to hold its own header, or too big for its parent. The real
// loader must refuse it through ChildChunkFits, report it to the file, and stop, without making the child.
static void TestImpossibleChildSize()
{
	puts("6. a child chunk whose size can't be real");

	struct BadSize { int size; char const * what; };
	BadSize const badSizes[] =
	{
		{  -1, "a negative size is refused" },
		{   0, "a size of zero is refused" },
		{  11, "a size too small for the header is refused" },
		{ 200, "a size bigger than the parent is refused" },
	};

	for (size_t i = 0; i < sizeof(badSizes) / sizeof(badSizes[0]); i++)
	{
		size_t const parentBytes = BuildObjectHeader(LongestName, sizeof(LongestName));
		PutChunkHeader(objectBytes, badSizes[i].size);

		TestFile file;
		Object_Chunk * object = LoadObject(file, parentBytes);

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, badSizes[i].what);
		Check(object->get_header() == 0, "no child was made");
		Check(NameLength(object) == 0, "the name is empty");
	}

	// Fewer bytes left in the parent than a child's header needs, right after a good child.
	size_t const goodBytes = BuildObjectHeader(LongestName, sizeof(LongestName));

	TestFile file;
	Object_Chunk * object = LoadObject(file, goodBytes + 8);

	Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "leftover bytes too few for a header are refused");
	Check(object->get_header() != 0, "the good child before them was kept");
	Check(NameLength(object) == 63, "and its name was read");
}

// The bytes of a shape: an SHPHEAD1 chunk with 68 bytes of data, with the shape's number at byte 20 of that data, its count
// of points at byte 24 and its count of polygons at byte 28. An object names its shape at byte 56 of its own data.
static size_t const ShapeHeaderDataBytes = 68;
static size_t const ShapeNumberAt = 20;
static size_t const ShapePointCountAt = 24;
static size_t const ShapePolygonCountAt = 28;
static size_t const ObjectShapeNumberAt = 56;

alignas(int) static char shapeBytes[ChunkHeaderBytes + ShapeHeaderDataBytes];

static void BuildShape(int shapeNumber)
{
	int const chunkBytes = (int)sizeof(shapeBytes);

	memset(shapeBytes, 0, sizeof(shapeBytes));
	memcpy(shapeBytes, "SHPHEAD1", 8);
	memcpy(shapeBytes + 8, &chunkBytes, sizeof(chunkBytes));
	memcpy(shapeBytes + ChunkHeaderBytes + ShapeNumberAt, &shapeNumber, sizeof(shapeNumber));
}

// Builds an object with this number, naming the shape with this number. Gives back how many bytes it takes up.
static size_t BuildObject(int objectNumber, int shapeItNames)
{
	size_t const bytes = BuildObjectHeader("object", 7);

	memcpy(objectBytes + ChunkHeaderBytes + ObjectNumberAt, &objectNumber, sizeof(objectNumber));
	memcpy(objectBytes + ChunkHeaderBytes + ObjectShapeNumberAt, &shapeItNames, sizeof(shapeItNames));

	return bytes;
}

// 7. Joining objects to shapes, in a real File_Chunk: everything that can't be right has to refuse the level, not crash.
static void TestObjectsAndShapes()
{
	puts("7. objects and shapes that don't join up");

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		Shape_Chunk * shape = new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		size_t const bytes = BuildObject(5, 7);
		Object_Chunk * object = new Object_Chunk(&file, objectBytes, bytes);
		file.post_input_processing();

		Check(file.error_code == 0, "a level whose numbers join up is accepted");
		Check(object->get_assoc_shape() == shape, "and the object is joined to its shape");
		Check(file.get_object_by_index(5) == object, "and the object can be found by its number");
	}

	{
		// The original tools could leave -1 on an object that had no number yet. It joins up as usual, and stays out of
		// the list of objects by number.
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		Shape_Chunk * shape = new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		size_t const bytes = BuildObject(-1, 7);
		Object_Chunk * object = new Object_Chunk(&file, objectBytes, bytes);
		file.post_input_processing();

		Check(file.error_code == 0, "an object numbered -1 is accepted");
		Check(object->get_assoc_shape() == shape, "and the object is joined to its shape");
		Check(file.get_object_by_index(0) == 0, "and it stays out of the list of objects by number");
	}

	struct BadLevel { int objectNumber; int shapeItNames; int shapeNumber; char const * what; };
	BadLevel const badLevels[] =
	{
		{          5,       1000,          7, "an object naming a shape that isn't in the file is refused" },
		{          5,          3,          7, "an object naming a gap in the shape numbers is refused" },
		{          5,         -1,          7, "an object naming a negative shape number is refused" },
		{          5, 0x7FFFFFFF,          7, "an object naming a huge shape number is refused" },
		{ 0x40000000,          7,          7, "an object number too big for the list is refused" },
		{          5,         -1,         -1, "a negative shape number is refused" },
		{          5, 0x7FFFFFFF, 0x7FFFFFFF, "a huge shape number is refused" },
	};

	for (size_t i = 0; i < sizeof(badLevels) / sizeof(badLevels[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(badLevels[i].shapeNumber);
		new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		size_t const bytes = BuildObject(badLevels[i].objectNumber, badLevels[i].shapeItNames);
		new Object_Chunk(&file, objectBytes, bytes);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, badLevels[i].what);
		Check(file.get_object_by_index(0) == 0, "and looking up an object is still safe");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		new Object_Chunk(&file, objectBytes, 0);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "an object with no header is refused");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		new Shape_Chunk(&file, shapeBytes, 0);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a shape with no header is refused");
	}

	// How many points a shape claims, at byte 24 of its data. The game's list of points holds 10000. These made-up shapes
	// have no points of their own, so only the count is being tested here.
	struct PointCount { int points; bool accepted; char const * what; };
	PointCount const pointCounts[] =
	{
		{      10000, true,  "a shape with as many points as the game can hold is accepted" },
		{      10001, false, "one point too many is refused" },
		{         -1, false, "a negative number of points is refused" },
		{ 0x7FFFFFFF, false, "a huge number of points is refused" },
	};

	for (size_t i = 0; i < sizeof(pointCounts) / sizeof(pointCounts[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		memcpy(shapeBytes + ChunkHeaderBytes + 24, &pointCounts[i].points, sizeof(pointCounts[i].points));
		new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		file.post_input_processing();

		Check((file.error_code == 0) == pointCounts[i].accepted, pointCounts[i].what);
	}

	Parent_File = 0;
}

// The bytes of a hierarchy section: an OBJHIERD chunk with a count of extra numbers, here 0, and then its name.
static char sectionBytes[ChunkHeaderBytes + 4 + 8];

static void BuildSection(char const * name, size_t nameBytes)
{
	int const chunkBytes = (int)sizeof(sectionBytes);

	memset(sectionBytes, 0, sizeof(sectionBytes));
	memcpy(sectionBytes, "OBJHIERD", 8);
	memcpy(sectionBytes + 8, &chunkBytes, sizeof(chunkBytes));
	memcpy(sectionBytes + ChunkHeaderBytes + 4, name, nameBytes);
}

// 8. A hierarchy section finds its object by name, so a section with a blank name has to refuse the level.
static void TestHierarchySections()
{
	puts("8. hierarchy sections");

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		size_t const bytes = BuildObject(5, 7);
		Object_Chunk * object = new Object_Chunk(&file, objectBytes, bytes);
		BuildSection("object", 7);
		Object_Hierarchy_Chunk * hierarchy = new Object_Hierarchy_Chunk(&file, sectionBytes, sizeof(sectionBytes));
		file.post_input_processing();

		Check(file.error_code == 0, "a section with a name is accepted");
		Check(hierarchy->get_data()->object == object, "and it finds the object with that name");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes));
		size_t const bytes = BuildObject(5, 7);
		new Object_Chunk(&file, objectBytes, bytes);
		BuildSection("", 1);
		new Object_Hierarchy_Chunk(&file, sectionBytes, sizeof(sectionBytes));
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a section with a blank name is refused");
	}

	Parent_File = 0;
}

// 9. An object's baked colours: a chunk with a light set's name, a spare number, a count, then the colours. Here the
// chunk's data has room for 4 colours after its 16 bytes of fixed data.
static void TestBakedColours()
{
	puts("9. baked colours");

	struct ColourChunk { int count; size_t bytes; bool accepted; char const * what; };
	ColourChunk const colourChunks[] =
	{
		{           4, 32, true,  "a count that fills the chunk exactly is accepted" },
		{           3, 32, true,  "a count that leaves room over is accepted" },
		{           0, 16, true,  "no colours at all is accepted" },
		{           5, 32, false, "one colour too many for the chunk is refused" },
		{          -1, 32, false, "a negative count is refused" },
		{  0x40000000, 32, false, "a count that would wrap a size sum is refused" },
		{  0x7FFFFFFF, 32, false, "a huge count is refused" },
		{           0, 15, false, "a chunk too short for its fixed data is refused" },
	};

	for (size_t i = 0; i < sizeof(colourChunks) / sizeof(colourChunks[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		alignas(int) char bytes[32] = { 'N', 'O', 'R', 'M', 'A', 'L', 'L', 'T' };
		memcpy(bytes + 12, &colourChunks[i].count, sizeof(colourChunks[i].count));

		Shape_Vertex_Intensities_Chunk * colours = new Shape_Vertex_Intensities_Chunk(&file, bytes, colourChunks[i].bytes);

		int const expected = colourChunks[i].accepted ? colourChunks[i].count : 0;

		Check((file.error_code == 0) == colourChunks[i].accepted, colourChunks[i].what);
		Check(colours->num_vertices == expected, "and the chunk says how many colours it really holds");
	}

	// How many places a shape's list needs, using real numbers from the game's own levels: colours, points, places.
	int const counts[][3] = { { 224, 228, 228 }, { 1430, 402, 1430 }, { 100, 100, 100 } };
	alignas(int) static char bigChunk[16 + 4 * 1430];

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		memcpy(bigChunk + 12, &counts[i][0], sizeof(counts[i][0]));

		Shape_Vertex_Intensities_Chunk * colours = new Shape_Vertex_Intensities_Chunk(&file, bigChunk, sizeof(bigChunk));

		Check(colours->colour_places_for_shape(counts[i][1]) == counts[i][2], "the list has a place for every point and colour");
	}

	Parent_File = 0;
}

// Room for the data of a whole shape: its header, its points and its polygons, laid out as in a level file. The biggest
// one here has 10001 points, at 12 bytes each.
alignas(int) static char wholeShape[130000];

// Writes a chunk's header and gives back where its data goes. The data starts as all zeros.
static char * PutChunk(char * where, char const * name, size_t dataBytes)
{
	int const chunkBytes = (int)(ChunkHeaderBytes + dataBytes);

	memset(where, 0, chunkBytes);
	memcpy(where, name, 8);
	memcpy(where + 8, &chunkBytes, sizeof(chunkBytes));

	return where + ChunkHeaderBytes;
}

// Builds the data of a shape with this many points and one polygon. A polygon has 5 places for its corners, after 16
// bytes of other numbers. Shapes and sub-shapes have different headers. Gives back how many bytes it all takes up.
static size_t BuildShapeData(char * where, bool subShape, int points, int const * corners)
{
	int const polygons = 1;
	size_t const headerBytes = subShape ? 44 : ShapeHeaderDataBytes;

	char * header = PutChunk(where, subShape ? "SUBSHPHD" : "SHPHEAD1", headerBytes);
	memcpy(header + (subShape ? 8 : ShapePointCountAt), &points, sizeof(points));
	memcpy(header + (subShape ? 12 : ShapePolygonCountAt), &polygons, sizeof(polygons));

	char * pointData = PutChunk(header + headerBytes, "SHPRAWVT", 12 * (size_t)points);
	char * polygon = PutChunk(pointData + 12 * (size_t)points, "SHPPOLYS", 36);
	memcpy(polygon + 16, corners, 5 * sizeof(int));

	return (polygon + 36) - where;
}

// 10. A polygon's corners. It has 4 places for them, and -1 marks the end.
static void TestPolygons()
{
	puts("10. polygons");

	struct PolygonRow { int corners[5]; bool accepted; char const * what; };
	PolygonRow const rows[] =
	{
		{ { 0, 1, 2, -1, -1 }, true,  "a polygon with 3 corners is accepted" },
		{ { 0, 1, 2,  3, -1 }, true,  "a polygon with 4 corners is accepted" },
		{ { 0, 1, 2,  3,  0 }, false, "a polygon with no end marker is refused" },
		{ { 0, 1, 4, -1, -1 }, false, "a corner past the last point is refused" },
		{ { 0, 1, -2, -1, -1 }, false, "a negative corner is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, rows[i].corners);
		new Shape_Chunk(&file, wholeShape, bytes);
		file.post_input_processing();

		Check((file.error_code == 0) == rows[i].accepted, rows[i].what);
	}

	// The points and polygons chunks set the shape's counts as they're read. A header that comes after them sets the counts
	// again from its own numbers, so they can only disagree with the header last, which is how these shapes are laid out.
	int const triangle[5] = { 0, 1, 2, -1, -1 };
	size_t const headerChunkBytes = ChunkHeaderBytes + ShapeHeaderDataBytes;

	struct CountRow { int points; int polygons; bool accepted; char const * what; };
	CountRow const countRows[] =
	{
		{ 4, 1, true,  "a header at the end that agrees with the chunks is accepted" },
		{ 4, 2, false, "a header that claims more polygons than there are is refused" },
		{ 4, 0, false, "a header that claims fewer polygons than there are is refused" },
		{ 5, 1, false, "a header that claims more points than there are is refused" },
		{ 3, 1, false, "a header that claims fewer points than there are is refused" },
	};

	for (size_t i = 0; i < sizeof(countRows) / sizeof(countRows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		memcpy(wholeShape + ChunkHeaderBytes + ShapePointCountAt, &countRows[i].points, sizeof(int));
		memcpy(wholeShape + ChunkHeaderBytes + ShapePolygonCountAt, &countRows[i].polygons, sizeof(int));
		std::rotate(wholeShape, wholeShape + headerChunkBytes, wholeShape + bytes); // moves the header to the end

		new Shape_Chunk(&file, wholeShape, bytes);
		file.post_input_processing();

		Check((file.error_code == 0) == countRows[i].accepted, countRows[i].what);
	}

	// Polygons join up points, so a shape with polygons and no points has nothing to draw them with. A chunk the game doesn't
	// know is skipped, so giving the points chunk a made-up name is the same as it not being there.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		memcpy(wholeShape + headerChunkBytes, "NOPOINTS", 8);

		new Shape_Chunk(&file, wholeShape, bytes);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a shape with polygons and no points is refused");
	}

	Parent_File = 0;
}

// 11. Sub-shapes become shapes in the game, so they get the same checks. A sub-shape used as a morph frame must also have
// as many points as the shape that morphs, and that shape must fit the game's list of 1024 morphed points.
static void TestSubShapes()
{
	puts("11. sub-shapes");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const badCorner[5] = { 0, 1, 9, -1, -1 };

	struct SubShapeRow { int points; int const * corners; bool accepted; char const * what; };
	SubShapeRow const rows[] =
	{
		{     4, triangle,  true,  "a sub-shape with sensible numbers is accepted" },
		{ 10000, triangle,  true,  "a sub-shape with as many points as the game can hold is accepted" },
		{ 10001, triangle,  false, "a sub-shape with one point too many is refused" },
		{     4, badCorner, false, "a sub-shape with a corner past its last point is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, true, rows[i].points, rows[i].corners);
		new Shape_Sub_Shape_Chunk(&file, wholeShape, bytes);
		file.post_input_processing();

		Check((file.error_code == 0) == rows[i].accepted, rows[i].what);
	}

	struct MorphRow { int shapePoints; int framePoints; bool accepted; char const * what; };
	MorphRow const morphRows[] =
	{
		{    4,    4, true,  "a morph frame the same size as its shape is accepted" },
		{ 1024, 1024, true,  "a morphing shape that just fits the list is accepted" },
		{    4,    3, false, "a morph frame with too few points is refused" },
		{    4,    5, false, "a morph frame with too many points is refused" },
		{ 1025, 1025, false, "a morphing shape too big for the list is refused" },
	};

	for (size_t i = 0; i < sizeof(morphRows) / sizeof(morphRows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		// the frame is built first, and copied in once its size is known
		alignas(int) static char frame[16384];
		size_t const frameBytes = BuildShapeData(frame, true, morphRows[i].framePoints, triangle);
		size_t const mainBytes = BuildShapeData(wholeShape, false, morphRows[i].shapePoints, triangle);

		char * morph = PutChunk(wholeShape + mainBytes, "SHPMORPH", ChunkHeaderBytes + frameBytes);
		char * subShape = PutChunk(morph, "SUBSHAPE", frameBytes);
		memcpy(subShape, frame, frameBytes);

		new Shape_Chunk(&file, wholeShape, mainBytes + 2 * ChunkHeaderBytes + frameBytes);
		file.post_input_processing();

		Check((file.error_code == 0) == morphRows[i].accepted, morphRows[i].what);
	}

	Parent_File = 0;
}

int main()
{
	// Print each line straight away. If the code under test crashes, the lines so far still show how far it got.
	setvbuf(stdout, NULL, _IONBF, 0);

	TestStockName();
	TestLongestName();
	TestOneCharacterTooMany();
	TestNoEndMarker();
	TestChunkTooShort();
	TestImpossibleChildSize();
	TestObjectsAndShapes();
	TestHierarchySections();
	TestBakedColours();
	TestPolygons();
	TestSubShapes();

	if (checksFailed)
	{
		printf("Rebecca's test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("Rebecca's test passed");
	return 0;
}
