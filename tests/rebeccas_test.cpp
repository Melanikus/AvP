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
// Test 10 does the same for the corners of a polygon, and test 11 for sub-shapes and morphing. Test 12 shows that a shape
// stored in a level as a raw copy of memory is skipped. Tests 13 to 15 cover repeated and missing chunks, merge data and
// animated shapes, test 16 the lists of texture coordinates, test 17 the lists of normals, test 18 whether a lift has all
// its parts, test 19 chunks of morph frames, test 20 chunks in the wrong place, and test 21 how deep chunks can
// sit inside chunks. Tests from 30 on cover the chunks whose
// constructors used to walk the file with no look at the chunk's size, starting with the two checks they all share.
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
#include <stdlib.h>
#include <string.h>

#include "chunk.hpp"
#include "chunk_type.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "chunk_light.hpp"
#include "hierarchy_chunk.hpp"
#include "mishchnk.hpp"
#include "module.h"
#include "strategy_def.h"
#include "bh_lift.h"
#include "environment_chunk.hpp"
#include "avp_chunk.hpp"
#include "hierplace.hpp"
#include "strategy_chunk.hpp"
#include "animation_chunk.hpp"

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

// The memory code stops the game through these if the computer runs out of memory. Here that stops the test.
void avp_MessageBox(const char * message, int)
{
	puts(message);
}

void avp_exit(int code)
{
	exit(code);
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

	// How many points a shape claims, at byte 24 of its data. These made-up shapes have no chunk of points, so any claim but
	// 0 is refused. Test 13 tries the game's limit on shapes with real points.
	struct PointCount { int points; bool accepted; char const * what; };
	PointCount const pointCounts[] =
	{
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
// one here has 10001 points, at 12 bytes each, with a normal for each point, also 12 bytes. The biggest chunk of morph frames
// here has 32768 frames, also at 12 bytes each.
alignas(int) static char wholeShape[500000];

// Memory that ends at a wall. A read past the end of a chunk nearly always lands in the next chunk, which proves nothing either way, so
// a test can't see it. So here, what's being read ends exactly where readable memory does: the next page is marked so that touching
// it stops the program. Reading even one byte past the end crashes the test, every time, and that counts as a failure. Nothing here is
// ever run as code. Gives back where to put this many bytes so that they end at the wall.
static char * MemoryEndingAtAWall(size_t bytes)
{
	static char * block = 0;

	SYSTEM_INFO system;
	GetSystemInfo(&system);

	size_t const page = system.dwPageSize;
	size_t const readable = 16 * page;

	if (!block)
	{
		DWORD before = 0;
		block = (char *)VirtualAlloc(NULL, readable + page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if (!block || !VirtualProtect(block + readable, page, PAGE_NOACCESS, &before))
		{
			puts("Couldn't make memory that ends at a wall");
			exit(1);
		}
	}

	if (bytes > readable)
	{
		puts("Too many bytes for the memory that ends at a wall");
		exit(1);
	}

	return block + readable - bytes;
}

// Writes a chunk's header and gives back where its data goes. The data starts as all zeros.
static char * PutChunk(char * where, char const * name, size_t dataBytes)
{
	int const chunkBytes = (int)(ChunkHeaderBytes + dataBytes);

	memset(where, 0, chunkBytes);
	memcpy(where, name, 8);
	memcpy(where + 8, &chunkBytes, sizeof(chunkBytes));

	return where + ChunkHeaderBytes;
}

// Writes a shape's two lists of normals, with every normal all zeros, which is allowed, and gives back where the next chunk goes.
static char * PutNormals(char * where, int points, int polygons)
{
	char * polygonNormals = PutChunk(where, "SHPVNORM", 12 * (size_t)points) + 12 * (size_t)points;

	return PutChunk(polygonNormals, "SHPPNORM", 12 * (size_t)polygons) + 12 * (size_t)polygons;
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

	return PutNormals(polygon + 36, points, polygons) - where;
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
		{ { -1, -1, -1, -1, -1 }, false, "a polygon with no corners is refused" },
		{ { 0, -1, -1, -1, -1 }, false, "a polygon with 1 corner is refused" },
		{ { 0, 1, -1, -1, -1 }, false, "a polygon with 2 corners is refused" },
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

		size_t const built = BuildShapeData(wholeShape, false, 4, triangle);
		memcpy(wholeShape + ChunkHeaderBytes + ShapePointCountAt, &countRows[i].points, sizeof(int));
		memcpy(wholeShape + ChunkHeaderBytes + ShapePolygonCountAt, &countRows[i].polygons, sizeof(int));

		// The normals go with what the header claims, so that only the check on the header can refuse these.
		char * normals = wholeShape + built - (ChunkHeaderBytes + 12) - (ChunkHeaderBytes + 48);
		size_t const bytes = PutNormals(normals, countRows[i].points, 1) - wholeShape;
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
		alignas(int) static char frame[32768];
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

// 12. A level could once hold a shape as a raw copy of the game's memory, its counts and addresses included, and the game
// used that copy as it stood. Nothing checked it, so a level could choose where the game wrote. The game no longer reads such
// a chunk. It's kept as a chunk of a kind the game doesn't know, and the shape comes from its ordinary, checked chunks.
static void TestRawMemoryShape()
{
	puts("12. shapes stored as raw memory");

	int const triangle[5] = { 0, 1, 2, -1, -1 };

	// the first number in the chunk was the size of the copy, taken on trust
	struct RawRow { int size; char const * skipped; char const * loaded; };
	RawRow const rows[] =
	{
		{ 0,          "an empty raw copy is skipped, not read", "the rest of that shape still loads" },
		{ 0x7fffffff, "a raw copy claiming a huge size is skipped, not read", "the rest of that shape still loads" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const mainBytes = BuildShapeData(wholeShape, false, 4, triangle);
		size_t const rawBytes = 12;
		char * rawCopy = PutChunk(wholeShape + mainBytes, "SHPPRPRO", rawBytes);
		memcpy(rawCopy, &rows[i].size, sizeof(rows[i].size));

		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, mainBytes + ChunkHeaderBytes + rawBytes);
		file.post_input_processing();

		Chunk * rawChunk = shape->lookup_single_child("SHPPRPRO");

		Check(rawChunk && rawChunk->r_u_miscellaneous(), rows[i].skipped);
		Check(file.error_code == 0, rows[i].loaded);
	}

	Parent_File = 0;
}

// These write one chunk each, for shapes laid out in unusual ways, and give back where the next chunk goes.
static char * PutPoints(char * where, int points)
{
	return PutChunk(where, "SHPRAWVT", 12 * (size_t)points) + 12 * (size_t)points;
}

static char * PutPolygons(char * where, int polygons, int const * corners)
{
	char * data = PutChunk(where, "SHPPOLYS", 36 * (size_t)polygons);

	for (int i = 0; i < polygons; i++)
	{
		memcpy(data + 36 * i + 16, corners, 5 * sizeof(int));
	}

	return data + 36 * (size_t)polygons;
}

static char * PutShapeHeader(char * where, int points, int polygons)
{
	char * data = PutChunk(where, "SHPHEAD1", ShapeHeaderDataBytes);

	memcpy(data + ShapePointCountAt, &points, sizeof(points));
	memcpy(data + ShapePolygonCountAt, &polygons, sizeof(polygons));

	return data + ShapeHeaderDataBytes;
}

// Loads the shape in wholeShape, which ends at "end", and says whether the level would be accepted.
static bool ShapeIsAccepted(char const * end)
{
	File_Chunk file;
	Parent_File = &file;

	new Shape_Chunk(&file, wholeShape, end - wholeShape);
	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;
	return accepted;
}

// 13. A chunk of points or polygons replaces the shape's list as it's read, while the checks look at the first one. So a
// second chunk could leave the shape with a shorter list than the one that was checked. Each header here agrees with the
// first chunk, not the last. And a shape with no chunk of points has no list of points, so it can't claim to have any.
static void TestRepeatedAndMissingChunks()
{
	puts("13. repeated and missing chunks");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	char * end;

	end = PutPoints(wholeShape, 5);
	end = PutPoints(end, 4);
	end = PutPolygons(end, 1, triangle);
	end = PutNormals(end, 5, 1);
	end = PutShapeHeader(end, 5, 1);
	Check(!ShapeIsAccepted(end), "a shape with two chunks of points is refused");

	end = PutPoints(wholeShape, 4);
	end = PutPolygons(end, 2, triangle);
	end = PutPolygons(end, 1, triangle);
	end = PutNormals(end, 4, 2);
	end = PutShapeHeader(end, 4, 2);
	Check(!ShapeIsAccepted(end), "a shape with two chunks of polygons is refused");

	// With this check gone, the shape above would go on to read past the end of its short list, so what happened next would be
	// down to chance. Here the second chunk is the longer one, so nothing else can refuse the shape, and this check is shown to work.
	end = PutPoints(wholeShape, 4);
	end = PutPolygons(end, 1, triangle);
	end = PutPolygons(end, 2, triangle);
	end = PutNormals(end, 4, 1);
	end = PutShapeHeader(end, 4, 1);
	Check(!ShapeIsAccepted(end), "a shape with a longer second chunk of polygons is refused");

	end = PutShapeHeader(wholeShape, 4, 0);
	Check(!ShapeIsAccepted(end), "a header that claims points with no chunk of points is refused");

	end = PutShapeHeader(wholeShape, 0, 0);
	Check(ShapeIsAccepted(end), "an empty shape that claims nothing is accepted");

	// The game's list of points holds 10000.
	struct PointCount { int points; bool accepted; char const * what; };
	PointCount const pointCounts[] =
	{
		{ 10000, true,  "a shape with as many points as the game can hold is accepted" },
		{ 10001, false, "one point too many is refused" },
	};

	for (size_t i = 0; i < sizeof(pointCounts) / sizeof(pointCounts[0]); i++)
	{
		size_t const bytes = BuildShapeData(wholeShape, false, pointCounts[i].points, triangle);

		Check(ShapeIsAccepted(wholeShape + bytes) == pointCounts[i].accepted, pointCounts[i].what);
	}
}

// 14. Merge data has one entry per polygon: -1 if it stands alone, or the place of the polygon it joins. The game uses that
// place to reach into its lists, so it has to be real, and the two polygons of a pair have to name each other. A place just
// outside the list would be refused anyway, for not naming its partner back, by looking at whatever is there. So the "far"
// rows use places so far off that looking there would crash, which shows that the limit itself is doing the refusing.
// The game joins a pair at the edge they share, so a pair must be two triangles with exactly one edge in common. These three all do.
static void TestMergeData()
{
	puts("14. merge data");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const secondTriangle[5] = { 0, 2, 3, -1, -1 };
	int const thirdTriangle[5] = { 1, 2, 3, -1, -1 };

	int const farAway = 0x10000000;

	struct MergeRow { int entries; int merge[4]; bool accepted; char const * what; };
	MergeRow const rows[] =
	{
		{ 3, { -1, -1, -1 }, true,  "polygons that stand alone are accepted" },
		{ 3, {  1,  0, -1 }, true,  "a pair of polygons that name each other is accepted" },
		{ 2, { -1, -1 },     false, "merge data for too few polygons is refused" },
		{ 4, { -1, -1, -1, -1 }, false, "merge data for too many polygons is refused" },
		{ 3, {  1,  2,  0 }, false, "a polygon whose partner names another is refused" },
		{ 3, {  0, -1, -1 }, false, "a polygon that is its own partner is refused" },
		{ 3, {  3, -1, -1 }, false, "a partner past the last polygon is refused" },
		{ 3, { farAway, -1, -1 }, false, "a partner far past the last polygon is refused" },
		{ 3, { -2, -1, -1 }, false, "a negative partner is refused" },
		{ 3, { -farAway, -1, -1 }, false, "a hugely negative partner is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		size_t const mergeBytes = rows[i].entries * sizeof(int);

		char * end = PutShapeHeader(wholeShape, 4, 3);
		end = PutPoints(end, 4);
		end = PutPolygons(end, 3, triangle);
		memcpy(end - 2 * 36 + 16, secondTriangle, 5 * sizeof(int));
		memcpy(end - 36 + 16, thirdTriangle, 5 * sizeof(int));
		end = PutNormals(end, 4, 3);

		char * merge = PutChunk(end, "SHPMRGDT", mergeBytes);
		memcpy(merge, rows[i].merge, mergeBytes);

		Check(ShapeIsAccepted(merge + mergeBytes) == rows[i].accepted, rows[i].what);
	}

	// The two polygons of a pair, each given in full, in a shape of 6 points. All but the first must be refused.
	struct PairRow { int first[5]; int second[5]; bool accepted; char const * what; };
	PairRow const pairRows[] =
	{
		{ { 0, 1, 2, -1, -1 }, { 0, 2, 3, -1, -1 }, true,  "two triangles sharing one edge can merge" },
		{ { 0, 1, 2, -1, -1 }, { 3, 4, 5, -1, -1 }, false, "two triangles with nothing in common are refused" },
		{ { 0, 1, 2, -1, -1 }, { 2, 3, 4, -1, -1 }, false, "two triangles sharing one corner are refused" },
		{ { 0, 1, 2, -1, -1 }, { 2, 1, 0, -1, -1 }, false, "two triangles with all their corners in common are refused" },
		{ { 0, 1, 2, -1, -1 }, { 0, 1, 1, -1, -1 }, false, "a second polygon with all its corners in the first is refused" },
		{ { 0, 1, 1, -1, -1 }, { 0, 1, 2, -1, -1 }, false, "a first polygon with all its corners in the second is refused" },
		{ { 0, 1, 2, 3, -1 },  { 0, 1, 4, -1, -1 }, false, "a first polygon with four corners is refused" },
		{ { 0, 1, 4, -1, -1 }, { 0, 1, 2, 3, -1 },  false, "a second polygon with four corners is refused" },
	};

	for (size_t i = 0; i < sizeof(pairRows) / sizeof(pairRows[0]); i++)
	{
		int const pair[2] = { 1, 0 };

		char * end = PutShapeHeader(wholeShape, 6, 2);
		end = PutPoints(end, 6);
		end = PutPolygons(end, 2, triangle);
		memcpy(end - 2 * 36 + 16, pairRows[i].first, 5 * sizeof(int));
		memcpy(end - 36 + 16, pairRows[i].second, 5 * sizeof(int));
		end = PutNormals(end, 6, 2);

		char * merge = PutChunk(end, "SHPMRGDT", sizeof(pair));
		memcpy(merge, pair, sizeof(pair));

		Check(ShapeIsAccepted(merge + sizeof(pair)) == pairRows[i].accepted, pairRows[i].what);
	}
}

// Writes one animation with one frame, or with none. A frame has its own points and its own polygon normals. The data of
// a sequence and of a frame is five numbers and a name, here an empty one. The first number is its place, and the second
// a frame's flags. Gives back where the next chunk goes.
static char * PutAnimation(char * where, int sequence, bool hasFrame, int frameNumber, int points, int normals, int flags)
{
	size_t const dataBytes = 24;
	size_t const frameBytes = (ChunkHeaderBytes + dataBytes) + (ChunkHeaderBytes + 12 * (size_t)points) + (ChunkHeaderBytes + 12 * (size_t)normals);
	size_t const sequenceBytes = (ChunkHeaderBytes + dataBytes) + (hasFrame ? ChunkHeaderBytes + frameBytes : 0);

	char * sequenceChunk = PutChunk(where, "ANIMSEQU", sequenceBytes);
	char * sequenceData = PutChunk(sequenceChunk, "ANISEQDT", dataBytes);
	memcpy(sequenceData, &sequence, sizeof(sequence));

	if (hasFrame)
	{
		char * frameChunk = PutChunk(sequenceData + dataBytes, "ANIMFRAM", frameBytes);
		char * frameData = PutChunk(frameChunk, "ANIFRADT", dataBytes);
		memcpy(frameData, &frameNumber, sizeof(frameNumber));
		memcpy(frameData + 4, &flags, sizeof(flags));

		char * pointsEnd = PutPoints(frameData + dataBytes, points);
		PutChunk(pointsEnd, "SHPPNORM", 12 * (size_t)normals);
	}

	return where + ChunkHeaderBytes + sequenceBytes;
}

// 15. To show a frame, the game points the shape at that frame's lists and goes on using the shape's own counts, so a frame
// has to match its shape. These shapes have 4 points and 1 polygon.
static void TestAnimatedShapes()
{
	puts("15. animated shapes");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const badCorner[5] = { 0, 1, 9, -1, -1 };
	int const madeBetween = animframeflag_interpolated_frame;

	struct AnimationRow { int sequence; int points; int normals; int flags; bool accepted; char const * what; };
	AnimationRow const rows[] =
	{
		{    0, 4, 1, 0,           true,  "a frame that matches its shape is accepted" },
		{ 1023, 4, 1, 0,           true,  "the biggest sequence number is accepted" },
		{ 1024, 4, 1, 0,           false, "a sequence number too big for the list is refused" },
		{   -1, 4, 1, 0,           false, "a negative sequence number is refused" },
		{    0, 3, 1, 0,           false, "a frame with too few points is refused" },
		{    0, 5, 1, 0,           false, "a frame with too many points is refused" },
		{    0, 4, 0, 0,           false, "a frame with too few normals is refused" },
		{    0, 4, 2, 0,           false, "a frame with too many normals is refused" },
		{    0, 4, 1, madeBetween, false, "a frame marked as made by the game is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * end = PutAnimation(wholeShape + bytes, rows[i].sequence, true, 0, rows[i].points, rows[i].normals, rows[i].flags);

		Check(ShapeIsAccepted(end) == rows[i].accepted, rows[i].what);
	}

	size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
	Check(!ShapeIsAccepted(PutAnimation(wholeShape + bytes, 0, false, 0, 0, 0, 0)), "an animation with no frames is refused");

	// A frame's number is its place in the list of frames. The game already throws away a sequence with a number too big. A
	// number just below the list would be thrown away by luck, so the second row is so far below that looking there would crash.
	int const negativeFrames[] = { -1, -0x10000000 };

	for (size_t i = 0; i < sizeof(negativeFrames) / sizeof(negativeFrames[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		char * end = PutAnimation(wholeShape + bytes, 0, true, negativeFrames[i], 4, 1, 0);
		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, end - wholeShape);

		Check(shape->count_children("ANIMSEQU") == 0, "a sequence with a negative frame number is thrown away");
	}

	// A sub-shape that fails has to stop there. Working out an animation's normals is the step that would write through the
	// bad corner, and that step is what makes the list of normals, so an empty list shows that it never ran.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const subBytes = BuildShapeData(wholeShape, true, 4, badCorner);
		char * end = PutAnimation(wholeShape + subBytes, 0, true, 0, 4, 1, 0);

		Shape_Sub_Shape_Chunk * subShape = new Shape_Sub_Shape_Chunk(&file, wholeShape, end - wholeShape);
		file.post_input_processing();

		Anim_Shape_Sequence_Chunk * animation = (Anim_Shape_Sequence_Chunk *) subShape->lookup_single_child("ANIMSEQU");

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "an animated sub-shape with a bad corner is refused");
		Check(animation && animation->sequence_data.v_normal_list == 0, "its animation is left alone");
	}

	Parent_File = 0;
}

// Writes a chunk of texture coordinates with this many lists, each with this many corners. A list is its number of corners,
// then two numbers for each corner.
static char * PutUVLists(char * where, int lists, int cornersInEach)
{
	size_t const listBytes = sizeof(int) + 2 * sizeof(float) * (size_t)cornersInEach;
	size_t const chunkBytes = sizeof(int) + (size_t)lists * listBytes;

	char * data = PutChunk(where, "SHPUVCRD", chunkBytes);
	memcpy(data, &lists, sizeof(lists));

	for (int i = 0; i < lists; i++)
	{
		memcpy(data + sizeof(int) + i * listBytes, &cornersInEach, sizeof(cornersInEach));
	}

	return data + chunkBytes;
}

// Writes one polygon with a texture on it. A polygon's kind is the first number of its record, and its colour the fourth.
static char * PutTexturedPolygon(char * where, int const * corners, unsigned int colour)
{
	char * end = PutPolygons(where, 1, corners);
	int const kind = I_3dTexturedPolygon;

	memcpy(end - 36, &kind, sizeof(kind));
	memcpy(end - 36 + 12, &colour, sizeof(colour));

	return end;
}

// 16. A chunk of texture coordinates says how many lists it holds, and each list how many corners, and a list has 4 places. A
// textured polygon's colour says which list is its own, and the game writes into its own lists at that place. So all of these
// numbers have to be real.
static void TestTextureCoordinates()
{
	puts("16. texture coordinates");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const fourCorners[5] = { 0, 1, 2, 3, -1 };

	// How much the chunk holds, then the two numbers at its start: how many lists, and how many corners in the first.
	struct UVChunkRow { size_t bytes; int lists; int corners; bool accepted; char const * what; };
	UVChunkRow const chunkRows[] =
	{
		{  0,          0,  0, false, "a chunk with no room for its count is refused" },
		{  4,          0,  0, true,  "a chunk with no lists is accepted" },
		{  8,          1,  0, true,  "an empty list is accepted" },
		{ 40,          1,  4, true,  "a list with 4 corners is accepted" },
		{ 48,          1,  5, false, "a list with 5 corners is refused" },
		{  8,          1, -1, false, "a negative number of corners is refused" },
		{  8,          1,  3, false, "a list that runs past the end of the chunk is refused" },
		{ 16,          2,  1, false, "a list with no room for its number of corners is refused" },
		{  4,          1,  0, false, "a count with no room for its lists is refused" },
		{  4,         -1,  0, false, "a negative number of lists is refused" },
		{  8, 0x7fffffff,  0, false, "a huge number of lists is refused" },
	};

	for (size_t i = 0; i < sizeof(chunkRows) / sizeof(chunkRows[0]); i++)
	{
		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * data = PutChunk(wholeShape + bytes, "SHPUVCRD", chunkRows[i].bytes);

		if (chunkRows[i].bytes >= sizeof(int))
		{
			memcpy(data, &chunkRows[i].lists, sizeof(int));
		}

		if (chunkRows[i].bytes >= 2 * sizeof(int))
		{
			memcpy(data + sizeof(int), &chunkRows[i].corners, sizeof(int));
		}

		// the bytes after the chunk, which a read past its end would find
		memset(data + chunkRows[i].bytes, 0, 2 * sizeof(int));

		Check(ShapeIsAccepted(data + chunkRows[i].bytes) == chunkRows[i].accepted, chunkRows[i].what);
	}

	// A sub-shape reads its lists with its own constructor.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, true, 4, triangle);
		char * end = PutUVLists(wholeShape + bytes, 1, 5);

		new Shape_Sub_Shape_Chunk(&file, wholeShape, end - wholeShape);

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a sub-shape's list with 5 corners is refused");
		Parent_File = 0;
	}

	// The numbers themselves have to arrive.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * end = PutUVLists(wholeShape + bytes, 2, 3);
		float const lastNumber = 7.5f;
		memcpy(end - sizeof(float), &lastNumber, sizeof(lastNumber));

		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, end - wholeShape);

		Check(shape->shape_data.num_uvs == 2 && shape->shape_data.uv_list[1].num_verts == 3
			&& shape->shape_data.uv_list[1].vert[2].v == lastNumber, "the lists and their numbers are all read");
		Parent_File = 0;
	}

	// These shapes have 2 lists. The list's place is the top half of the polygon's colour.
	struct UVIndexRow { unsigned int colour; int const * corners; int listCorners; bool accepted; char const * what; };
	UVIndexRow const indexRows[] =
	{
		{ 0x00000000, triangle,    3, true,  "a polygon using the first list is accepted" },
		{ 0x00010000, triangle,    3, true,  "a polygon using the last list is accepted" },
		{ 0x00020000, triangle,    3, false, "a list just past the last is refused" },
		{ 0x00008000, triangle,    3, false, "a textured polygon with the bit that skips its coordinates is refused" },
		{ 0x7fff0000, triangle,    3, false, "a list far past the last is refused" },
		{ 0xffff0000, triangle,    3, false, "a negative list is refused" },
		{ 0x80000000, triangle,    3, false, "a hugely negative list is refused" },
		{ 0x00000000, fourCorners, 3, false, "a polygon with more corners than its list is refused" },
		{ 0x00000000, triangle,    4, true,  "a list with more corners than its polygon is accepted" },
		{ 0x00000000, triangle,    0, true,  "an empty list is accepted, as the game shows that polygon as a line" },
	};

	for (size_t i = 0; i < sizeof(indexRows) / sizeof(indexRows[0]); i++)
	{
		char * end = PutShapeHeader(wholeShape, 4, 1);
		end = PutPoints(end, 4);
		end = PutTexturedPolygon(end, indexRows[i].corners, indexRows[i].colour);
		end = PutNormals(end, 4, 1);
		end = PutUVLists(end, 2, indexRows[i].listCorners);

		Check(ShapeIsAccepted(end) == indexRows[i].accepted, indexRows[i].what);
	}

	char * end = PutShapeHeader(wholeShape, 4, 1);
	end = PutPoints(end, 4);
	end = PutTexturedPolygon(end, triangle, 0);
	end = PutNormals(end, 4, 1);
	Check(!ShapeIsAccepted(end), "a textured polygon in a shape with no lists is refused");

	// A polygon with no texture uses its colour for something else, so the same number means nothing.
	end = PutShapeHeader(wholeShape, 4, 1);
	end = PutPoints(end, 4);
	char * polygonEnd = PutPolygons(end, 1, triangle);
	unsigned int const colour = 0x7fff0000;
	memcpy(polygonEnd - 36 + 12, &colour, sizeof(colour));
	Check(ShapeIsAccepted(PutNormals(polygonEnd, 4, 1)), "a polygon with no texture can have any colour");
}

// 17. A shape's normals are in two lists, one for its points and one for its polygons. Each list comes from a chunk with a count of
// its own, while the game reads both by the shape's counts. So each must be there, at exactly the right length, and only once. Their
// numbers go through the game's whole-number form and into the shaders, so they have to be sensible. And a polygon says where in
// its list its own normal is, so that has to be a real place.
static void TestNormals()
{
	puts("17. normals");

	int const triangle[5] = { 0, 1, 2, -1, -1 };

	// How many normals each chunk holds, where -1 means there's no chunk at all. This shape has 4 points and 1 polygon.
	struct CountRow { int forPoints; int forPolygons; bool accepted; char const * what; };
	CountRow const countRows[] =
	{
		{  4,  1, true,  "one normal for each point and each polygon is accepted" },
		{ -1,  1, false, "a shape with no normals for its points is refused" },
		{  4, -1, false, "a shape with no normals for its polygons is refused" },
		{  3,  1, false, "too few normals for the points are refused" },
		{  5,  1, false, "too many normals for the points are refused" },
		{  4,  0, false, "too few normals for the polygons are refused" },
		{  4,  2, false, "too many normals for the polygons are refused" },
	};

	for (size_t i = 0; i < sizeof(countRows) / sizeof(countRows[0]); i++)
	{
		char * end = PutShapeHeader(wholeShape, 4, 1);
		end = PutPoints(end, 4);
		end = PutPolygons(end, 1, triangle);

		if (countRows[i].forPoints >= 0)
		{
			end = PutChunk(end, "SHPVNORM", 12 * (size_t)countRows[i].forPoints) + 12 * (size_t)countRows[i].forPoints;
		}

		if (countRows[i].forPolygons >= 0)
		{
			end = PutChunk(end, "SHPPNORM", 12 * (size_t)countRows[i].forPolygons) + 12 * (size_t)countRows[i].forPolygons;
		}

		Check(ShapeIsAccepted(end) == countRows[i].accepted, countRows[i].what);
	}

	// A second chunk replaces the list as it's read, while the checks look at the first. The danger is a short second chunk, but
	// here it's the longer one, on purpose. With a short one and this check gone, what came next would be down to whatever memory
	// lay past the end of the list. With a longer one, nothing else can refuse the shape, so this shows that the check does.
	char * end = PutShapeHeader(wholeShape, 4, 1);
	end = PutPoints(end, 4);
	end = PutPolygons(end, 1, triangle);
	end = PutNormals(end, 4, 1);
	end = PutChunk(end, "SHPVNORM", 12 * 5) + 12 * 5;
	Check(!ShapeIsAccepted(end), "a second chunk of normals for the points is refused");

	end = PutShapeHeader(wholeShape, 4, 1);
	end = PutPoints(end, 4);
	end = PutPolygons(end, 1, triangle);
	end = PutNormals(end, 4, 1);
	end = PutChunk(end, "SHPPNORM", 12 * 2) + 12 * 2;
	Check(!ShapeIsAccepted(end), "a second chunk of normals for the polygons is refused");

	// The numbers. These are the bits of a float, since "not a number" can't be written any other way. 0x40000000 is 2.
	struct NumberRow { unsigned int bits; bool accepted; char const * what; };
	NumberRow const numberRows[] =
	{
		{ 0x00000000, true,  "a normal number of 0 is accepted" },
		{ 0x3f800000, true,  "a normal number of 1 is accepted" },
		{ 0x40000000, true,  "a normal number of 2 is accepted" },
		{ 0xc0000000, true,  "a normal number of -2 is accepted" },
		{ 0x40000001, false, "the first number above 2 is refused" },
		{ 0xc0000001, false, "the first number below -2 is refused" },
		{ 0x7f7fffff, false, "the biggest real number is refused" },
		{ 0x7f800000, false, "infinity is refused" },
		{ 0xff800000, false, "minus infinity is refused" },
		{ 0x7fc00000, false, "a float that isn't a number is refused" },
	};

	// Every number goes in each of the 3 places of the last normal in each list, so each place and the end of each list are tested.
	for (size_t i = 0; i < sizeof(numberRows) / sizeof(numberRows[0]); i++)
	{
		for (int list = 0; list < 2; list++)
		{
			for (int place = 0; place < 3; place++)
			{
				end = PutShapeHeader(wholeShape, 4, 1);
				end = PutPoints(end, 4);
				end = PutPolygons(end, 1, triangle);
				end = PutNormals(end, 4, 1);

				// the polygon list is the last thing written, and the point list ends just before its chunk
				size_t const backFromEnd = (list == 0) ? ChunkHeaderBytes + 12 + 12 : 12;
				memcpy(end - backFromEnd + place * sizeof(float), &numberRows[i].bits, sizeof(float));

				Check(ShapeIsAccepted(end) == numberRows[i].accepted, numberRows[i].what);
			}
		}
	}

	// A polygon's own normal, for each of 2 polygons. It's the second number of a polygon's record.
	struct IndexRow { int index; bool accepted; char const * what; };
	IndexRow const indexRows[] =
	{
		{ 0,          true,  "a polygon naming the first normal is accepted" },
		{ 1,          true,  "a polygon naming the last normal is accepted" },
		{ 2,          false, "a polygon naming a normal past the end is refused" },
		{ -1,         false, "a polygon naming a negative place is refused" },
		{ 0x7fffffff, false, "a polygon naming a place far past the end is refused" },
	};

	for (size_t i = 0; i < sizeof(indexRows) / sizeof(indexRows[0]); i++)
	{
		for (int polygon = 0; polygon < 2; polygon++)
		{
			end = PutShapeHeader(wholeShape, 4, 2);
			end = PutPoints(end, 4);
			end = PutPolygons(end, 2, triangle);
			memcpy(end - 36 * (2 - polygon) + 4, &indexRows[i].index, sizeof(int));
			end = PutNormals(end, 4, 2);

			Check(ShapeIsAccepted(end) == indexRows[i].accepted, indexRows[i].what);
		}
	}

	// A sub-shape goes through the same checks. Renaming the chunk leaves everything else in place.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, true, 4, triangle);
		size_t const pointNormalsAt = bytes - (ChunkHeaderBytes + 12) - (ChunkHeaderBytes + 48);
		Check(memcmp(wholeShape + pointNormalsAt, "SHPVNORM", 8) == 0, "the test found the sub-shape's normals");
		memcpy(wholeShape + pointNormalsAt, "NOTNORMS", 8);

		new Shape_Sub_Shape_Chunk(&file, wholeShape, bytes);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a sub-shape with no normals for its points is refused");
		Parent_File = 0;
	}
}

// 18. A lift's stations, doors and modules are found by numbers and names from the level file, so any of them can be missing,
// and the code that runs a lift reads through all of them with no check. A lift with a part missing has to stay still.
static void TestLifts()
{
	puts("18. lifts");

	LIFT_STATION stations[2];
	LIFT_STATION *list[2] = { &stations[0], &stations[1] };

	// only ever compared with nothing, never read through
	STRATEGYBLOCK *const someDoor = (STRATEGYBLOCK *)&stations[0];
	MODULE *const someModule = (MODULE *)&stations[0];

	memset(stations, 0, sizeof(stations));
	stations[0].lift_door = someDoor;
	stations[0].lift_module = someModule;
	stations[1].lift_door = someDoor;
	stations[1].lift_module = someModule;

	LIFT_CONTROL_BLOCK lift;
	memset(&lift, 0, sizeof(lift));
	lift.num_stations = 2;
	lift.lift_stations = list;
	Check(LiftIsComplete(&lift) != 0, "a lift with all its parts can run");

	lift.num_stations = 0;
	Check(LiftIsComplete(&lift) == 0, "a lift with no stations can't run");

	lift.num_stations = -1;
	Check(LiftIsComplete(&lift) == 0, "a lift with a negative number of stations can't run");

	lift.num_stations = 2;
	lift.lift_stations = 0;
	Check(LiftIsComplete(&lift) == 0, "a lift with no list of stations can't run");

	lift.lift_stations = list;
	list[1] = 0;
	Check(LiftIsComplete(&lift) == 0, "a lift with a floor left empty can't run");

	list[1] = &stations[1];
	stations[1].lift_door = 0;
	Check(LiftIsComplete(&lift) == 0, "a lift with a door missing can't run");

	stations[1].lift_door = someDoor;
	stations[1].lift_module = 0;
	Check(LiftIsComplete(&lift) == 0, "a lift with a module missing can't run");
}

// 19. A chunk of morph frames holds three numbers, the last a count of frames, and then three numbers for each frame. The count sizes
// a list that's then filled from the file, so the chunk has to be exactly the size the count says.
static void TestMorphFrames()
{
	puts("19. morph frames");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	size_t const countAt = 8;

	// The count is written even when it falls past the end of the chunk, because that's where a read with no check would find it. In
	// the row of 8 bytes it's the one count that would pass the size check, because 8 less 12 wraps round to the same number as 12 times
	// that count. So only the check on the chunk's room can refuse that row.
	struct FrameRow { size_t bytes; int count; bool accepted; char const * what; };
	FrameRow const rows[] =
	{
		{ 12,          0, true,  "a chunk with no frames is accepted" },
		{ 24,          1, true,  "one frame is accepted" },
		{ 36,          2, true,  "two frames are accepted" },
		{  8, 0x15555555, false, "a chunk with no room for its three numbers is refused" },
		{ 24,          2, false, "a count too big for the chunk is refused" },
		{ 24,          0, false, "a count too small for the chunk is refused" },
		{ 28,          1, false, "bytes left over after the last frame are refused" },
		{ 24,         -1, false, "a negative count is refused" },
		{ 24, 0x55555556, false, "a count that wraps round when multiplied is refused" },
		{ 24, 0x7fffffff, false, "a huge count is refused" },
		{ 12 + 12 * 32767, 32767, true,  "as many frames as the game can count are accepted" },
		{ 12 + 12 * 32768, 32768, false, "one frame more than the game can count is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		size_t const mainBytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * morph = PutChunk(wholeShape + mainBytes, "SHPMORPH", ChunkHeaderBytes + rows[i].bytes);
		char * frames = PutChunk(morph, "FRMMORPH", rows[i].bytes);

		memcpy(frames + countAt, &rows[i].count, sizeof(int));

		Check(ShapeIsAccepted(frames + rows[i].bytes) == rows[i].accepted, rows[i].what);
	}

	// With no room for its three numbers, the chunk mustn't even read them, and the checks after that can't see whether it did. So this
	// shape ends at the wall, with that chunk last.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const tooSmall = 8;
		size_t const mainBytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * morph = PutChunk(wholeShape + mainBytes, "SHPMORPH", ChunkHeaderBytes + tooSmall);
		PutChunk(morph, "FRMMORPH", tooSmall);

		size_t const allBytes = mainBytes + 2 * ChunkHeaderBytes + tooSmall;
		char * atTheWall = MemoryEndingAtAWall(allBytes);
		memcpy(atTheWall, wholeShape, allBytes);

		new Shape_Chunk(&file, atTheWall, allBytes);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a chunk with no room for its numbers doesn't read past its end");
		Parent_File = 0;
	}

	// The numbers themselves have to arrive. A shape number of -1 means the shape the morph belongs to, so this frame needs nothing else.
	{
		File_Chunk file;
		Parent_File = &file;

		int const numbers[6] = { 0, 65536, 1, -1, -1, 0 };
		size_t const mainBytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * morph = PutChunk(wholeShape + mainBytes, "SHPMORPH", ChunkHeaderBytes + sizeof(numbers));
		char * frames = PutChunk(morph, "FRMMORPH", sizeof(numbers));
		memcpy(frames, numbers, sizeof(numbers));

		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, (frames + sizeof(numbers)) - wholeShape);
		file.post_input_processing();

		Chunk_With_Children * morphChunk = (Chunk_With_Children *)shape->lookup_single_child("SHPMORPH");
		Shape_Morphing_Frame_Data_Chunk * frameChunk = (Shape_Morphing_Frame_Data_Chunk *)morphChunk->lookup_single_child("FRMMORPH");

		Check(file.error_code == 0, "a real chunk of morph frames is accepted");
		Check(frameChunk->anim_frames.size() == 1, "its frame was read");
		Check(frameChunk->a_speed == 65536, "and so was its speed");
		Parent_File = 0;
	}
}

// 20. Some chunks trust their parent: they write into it, or treat it as a particular kind of chunk. A file can put any chunk anywhere,
// so these are only made under the parent they trust. Anywhere else they're a chunk nothing reads, and the level still loads.
static void TestChunksInTheWrongPlace()
{
	puts("20. chunks in the wrong place");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	size_t const wrongPlaceBytes = 200;

	struct PlaceRow { char const * id; char const * what; };
	PlaceRow const rows[] =
	{
		{ "OBJHEAD1", "an object's header inside a shape is left alone" },
		{ "GMODHEAD", "a game mode's header inside a shape is left alone" },
		{ "GAMEMODE", "a game mode inside a shape is left alone" },
		{ "RIFCHILD", "a game mode's child inside a shape is left alone" },
		{ "AVPGENER", "a generator inside a shape is left alone" },
		{ "AIMODMAS", "a module's master chunk inside a shape is left alone" },
		{ "AIMODSLA", "a module's slave chunk inside a shape is left alone" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		PutChunk(wholeShape + bytes, rows[i].id, wrongPlaceBytes);

		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, bytes + ChunkHeaderBytes + wrongPlaceBytes);
		Chunk * placed = shape->lookup_single_child(rows[i].id);

		Check(placed && placed->r_u_miscellaneous(), rows[i].what);
		Parent_File = 0;
	}

	// A shape's centre has to be inside a shape, and a chunk of morph frames sitting inside one isn't. With no shape to write to, the
	// old code wrote through nothing.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * morph = PutChunk(wholeShape + bytes, "SHPMORPH", ChunkHeaderBytes + 16);
		PutChunk(morph, "SHPCENTR", 16);

		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, bytes + 2 * ChunkHeaderBytes + 16);
		Chunk_With_Children * morphChunk = (Chunk_With_Children *)shape->lookup_single_child("SHPMORPH");
		Chunk * centre = morphChunk->lookup_single_child("SHPCENTR");

		Check(centre && centre->r_u_miscellaneous(), "a shape's centre outside a shape is left alone");
		Parent_File = 0;
	}

	// In the right place, it still works.
	{
		File_Chunk file;
		Parent_File = &file;

		int const centreNumbers[3] = { 7, 8, 9 };
		float const radius = 2.5f;

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * centre = PutChunk(wholeShape + bytes, "SHPCENTR", 16);
		memcpy(centre, centreNumbers, sizeof(centreNumbers));
		memcpy(centre + sizeof(centreNumbers), &radius, sizeof(radius));

		Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, bytes + ChunkHeaderBytes + 16);

		Check(shape->shape_data.centre.y == 8 && shape->shape_data.radius_about_centre == radius, "a shape's centre inside a shape is read");
		Parent_File = 0;
	}

	// The game reads a game mode's name through its header, so a game mode needs one. A header of all zeros is an empty name and no files.
	struct ModeRow { bool hasHeader; bool accepted; char const * what; };
	ModeRow const modeRows[] =
	{
		{ true,  true,  "a game mode with a header is accepted" },
		{ false, false, "a game mode with no header is refused" },
	};

	for (size_t i = 0; i < sizeof(modeRows) / sizeof(modeRows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const headerBytes = 64;
		size_t const modeBytes = modeRows[i].hasHeader ? ChunkHeaderBytes + headerBytes : 0;

		char * mode = PutChunk(wholeShape, "GAMEMODE", modeBytes);

		if (modeRows[i].hasHeader)
		{
			PutChunk(mode, "GMODHEAD", headerBytes);
		}

		new Environment_Data_Chunk(&file, wholeShape, ChunkHeaderBytes + modeBytes);

		Check((file.error_code == 0) == modeRows[i].accepted, modeRows[i].what);
		Parent_File = 0;
	}
}

// 21. Chunks can hold chunks, and each level uses more of the stack, so a small file could nest deep enough to crash the game. These
// are chunks of special objects, one inside the next, each holding nothing but the next. The game can go 64 deep.
static void TestNesting()
{
	puts("21. nesting");

	struct NestRow { int levels; bool accepted; char const * what; };
	NestRow const rows[] =
	{
		{     15, true,  "as deep as the game's own levels go is accepted" },
		{     64, true,  "as deep as the limit is accepted" },
		{     65, false, "one level past the limit is refused" },
		{ 200000, false, "a file nested deep enough to run off the stack is refused" },
	};

	alignas(int) static char nested[200000 * ChunkHeaderBytes];

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		for (int level = 0; level < rows[i].levels; level++)
		{
			PutChunk(nested + level * ChunkHeaderBytes, "SPECLOBJ", (rows[i].levels - level - 1) * ChunkHeaderBytes);
		}

		// PutChunk clears what a chunk holds, so these go in from the inside out
		for (int level = rows[i].levels - 1; level >= 0; level--)
		{
			int const chunkBytes = (rows[i].levels - level) * (int)ChunkHeaderBytes;

			memcpy(nested + level * ChunkHeaderBytes, "SPECLOBJ", 8);
			memcpy(nested + level * ChunkHeaderBytes + 8, &chunkBytes, sizeof(chunkBytes));
		}

		new Special_Objects_Chunk(&file, nested, rows[i].levels * ChunkHeaderBytes);

		Check((file.error_code == 0) == rows[i].accepted, rows[i].what);
		Parent_File = 0;
	}
}

// 30. The two checks every chunk constructor shares. Each is proven alone here, so that a mistake in one can't hide behind a test of something
// that uses it.
static void TestChunkBoundChecks()
{
	puts("30. the checks on a count and a name");

	struct CountRow { int count; size_t itemBytes; size_t bytesLeft; bool fits; char const * what; };
	CountRow const counts[] =
	{
		{           0,  4,   0, true,  "no items fit in no room" },
		{           1,  4,   0, false, "one item doesn't fit in no room" },
		{          25,  4, 100, true,  "items that fill the room exactly fit" },
		{          26,  4, 100, false, "one item too many doesn't fit" },
		{          25,  4,  99, false, "room that's one byte short doesn't fit" },
		{          -1,  4, 100, false, "a negative count doesn't fit" },
		{          -1,  0, 100, false, "a negative count doesn't fit, even of items that take no room" },
		{  (int)0x80000000, 4, 100, false, "the most negative count doesn't fit" },
		{  0x40000001,  4, 100, false, "a count that wraps to 4 bytes doesn't fit" },
		{  0x10000000, 16,   4, false, "a count that wraps to nothing doesn't fit" },
		{  0x7fffffff, 12, 0xffffffff, false, "the largest count doesn't fit in the largest room" },
	};

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
	{
		Check(CountFitsChunk(counts[i].count, counts[i].itemBytes, counts[i].bytesLeft) == counts[i].fits, counts[i].what);
	}

	struct NameRow { char const * bytes; size_t bytesLeft; int length; char const * what; };
	NameRow const names[] =
	{
		{ "abc\0xyz", 8,  3, "a name stops at its zero byte" },
		{ "abc\0xyz", 4,  3, "a zero in the last byte counts" },
		{ "abc\0xyz", 3, -1, "a zero one byte out of reach doesn't count" },
		{ "\0abc",    4,  0, "an empty name has length 0" },
		{ "abc",      0, -1, "no room at all means no name" },
	};

	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		Check(NameLengthInChunk(names[i].bytes, names[i].bytesLeft) == names[i].length, names[i].what);
	}

	// Filled to the wall with no zero byte. A check that looks one byte too far crashes here, every time.
	size_t const wallBytes = 8;
	char * atTheWall = MemoryEndingAtAWall(wallBytes);
	memset(atTheWall, 'x', wallBytes);
	Check(NameLengthInChunk(atTheWall, wallBytes) == -1, "a name with no zero stops at the end of its room");
}

// Writes one animated polygon of a chunk of texture animation: its six numbers, then each sequence's five numbers and its frames, all zeros.
static char * PutAnimatedPolygon(char * where, int polygon, int sequences, int corners, int frames, unsigned int flags)
{
	int * out = (int *)where;

	*out++ = polygon;
	*out++ = 0;
	*out++ = sequences;
	*out++ = corners;
	*out++ = 0;
	*out++ = 0;

	for (int i = 0; i < sequences; i++)
	{
		*out++ = 65536;
		*out++ = (int)flags;
		*out++ = frames;
		*out++ = 0;
		*out++ = 0;

		for (int j = 0; j < frames * (1 + 2 * corners); j++)
		{
			*out++ = 0;
		}
	}

	return (char *)out;
}

// Adds a chunk of texture animation that gives one polygon a sequence of one or more frames.
static char * PutAnimation(char * where, int polygon, int frames)
{
	int const polygons = 1;
	char temporary[4096];

	memcpy(temporary, &polygons, sizeof(polygons));

	size_t const bytes = PutAnimatedPolygon(temporary + sizeof(polygons), polygon, 1, 3, frames, 0) - temporary;
	char * data = PutChunk(where, "TEXTANIM", bytes);

	memcpy(data, temporary, bytes);

	return data + bytes;
}

// Loads a shape from memory that ends right after it, so that a read past the end of its last chunk crashes the test, every time.
static bool ShapeIsAcceptedAtTheWall(char const * end)
{
	size_t const allBytes = end - wholeShape;
	char * atTheWall = MemoryEndingAtAWall(allBytes);

	memcpy(atTheWall, wholeShape, allBytes);

	File_Chunk file;
	Parent_File = &file;

	new Shape_Chunk(&file, atTheWall, allBytes);
	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;
	return accepted;
}

// 31. A chunk of texture animation is lists inside lists, and every count comes from the level file. The game then uses its numbers to reach
// into the shape's lists, and writes there. So the chunk has to fit its own size, and its numbers have to fit the shape it's in.
static void TestTextureAnimation()
{
	puts("31. texture animation");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const secondTriangle[5] = { 0, 2, 3, -1, -1 };

	// A whole chunk is a count of 1, then a polygon with one sequence of one frame. Each row keeps some of it, and can change one number.
	size_t const wholeChunkBytes = 76;
	size_t const polygonsAt = 0, sequencesAt = 12, cornersAt = 16, flagsAt = 32, framesAt = 36;
	size_t const nothing = wholeChunkBytes;

	struct ChunkRow { size_t bytes; size_t at; int value; bool accepted; char const * what; };
	ChunkRow const rows[] =
	{
		{ 76, nothing,      0,          true,  "a polygon with one frame is accepted" },
		{  4, polygonsAt,   0,          true,  "a chunk with no polygons is accepted" },
		{  0, nothing,      0,          false, "a chunk with no room for its count is refused" },
		{  3, nothing,      0,          false, "a chunk too short for its count is refused" },
		{ 76, polygonsAt,   0,          false, "bytes left over with no polygons are refused" },
		{ 80, nothing,      0,          false, "bytes left over after the last frame are refused" },
		{  4, polygonsAt,   -1,         false, "a negative count of polygons is refused" },
		{ 76, polygonsAt,   2,          false, "a count of polygons too big for the chunk is refused" },
		{ 76, polygonsAt,   0x40000001, false, "a count of polygons that wraps round is refused" },
		{ 27, nothing,      0,          false, "a polygon cut short is refused" },
		{ 47, nothing,      0,          false, "a sequence cut short is refused" },
		{ 75, nothing,      0,          false, "a frame cut short is refused" },
		{ 28, sequencesAt,  0,          false, "a polygon with no sequences is refused" },
		{ 28, sequencesAt,  -1,         false, "a negative count of sequences is refused" },
		{ 76, sequencesAt,  2,          false, "a count of sequences too big for the chunk is refused" },
		{ 76, cornersAt,    2,          false, "a polygon with 2 corners is refused" },
		{ 76, cornersAt,    4,          false, "a polygon with 4 corners is refused" },
		{ 48, framesAt,     0,          false, "a sequence with no frames is refused" },
		{ 48, framesAt,     -1,         false, "a negative count of frames is refused" },
		{ 76, framesAt,     2,          false, "a count of frames too big for the chunk is refused" },
		{ 76, framesAt,     0x7fffffff, false, "a huge count of frames is refused" },
		{ 76, flagsAt,      (int)0x80000000, false, "one frame with nothing to lead back to is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		char whole[100] = { 0 };
		int const polygons = 1;

		memcpy(whole, &polygons, sizeof(polygons));
		PutAnimatedPolygon(whole + sizeof(polygons), 0, 1, 3, 1, 0);

		if (rows[i].at != nothing)
		{
			memcpy(whole + rows[i].at, &rows[i].value, sizeof(int));
		}

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * data = PutChunk(wholeShape + bytes, "TEXTANIM", rows[i].bytes);

		memcpy(data, whole, rows[i].bytes);

		// The same row twice. In ordinary memory a read past the end only finds the next bytes, so it's the wall that shows there wasn't one.
		Check(ShapeIsAccepted(data + rows[i].bytes) == rows[i].accepted, rows[i].what);
		Check(ShapeIsAcceptedAtTheWall(data + rows[i].bytes) == rows[i].accepted, "and the same at the end of readable memory");
	}

	// A count of frames too big for the chunk, with another polygon still to come. The last check, that the chunk ends where the walk ends,
	// can't save this one: the sum of what's left would wrap round to a huge number, and the walk would go on past the end of the chunk.
	{
		char whole[100] = { 0 };
		int const polygons = 2;
		int const frames = 2;

		memcpy(whole, &polygons, sizeof(polygons));
		PutAnimatedPolygon(whole + sizeof(polygons), 0, 1, 3, 1, 0);
		memcpy(whole + framesAt, &frames, sizeof(frames));

		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * data = PutChunk(wholeShape + bytes, "TEXTANIM", wholeChunkBytes);

		memcpy(data, whole, wholeChunkBytes);

		Check(!ShapeIsAcceptedAtTheWall(data + wholeChunkBytes), "a count of frames too big, with more to come, stops at the end of the chunk");
	}

	// A flag that leaves out the frame leading back is fine with two frames, because the two still lead into each other.
	{
		int const polygons = 1;
		size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * data = wholeShape + bytes + ChunkHeaderBytes;

		memcpy(data, &polygons, sizeof(polygons));
		size_t const chunkBytes = PutAnimatedPolygon(data + sizeof(polygons), 0, 1, 3, 2, 0x80000000) - data;

		char saved[200];
		memcpy(saved, data, chunkBytes);
		PutChunk(wholeShape + bytes, "TEXTANIM", chunkBytes);
		memcpy(data, saved, chunkBytes);

		Check(ShapeIsAccepted(data + chunkBytes), "two frames with nothing to lead back to are accepted");
	}

	// The most frames the game can count, then one more. A chunk this big is needed to reach the limit: anything shorter fails on its size.
	{
		static int bigChunk[2 + 6 + 5 + 7 * 32768];
		int const most = 32767;

		for (int frames = most; frames <= most + 1; frames++)
		{
			File_Chunk file;
			Parent_File = &file;

			int const polygons = 1;
			memcpy(bigChunk, &polygons, sizeof(polygons));

			size_t const bytes = PutAnimatedPolygon((char *)bigChunk + sizeof(polygons), 0, 1, 3, frames, 0) - (char *)bigChunk;

			new Animation_Chunk(&file, (char *)bigChunk, bytes);

			Check((file.error_code == 0) == (frames == most), frames == most ? "the most frames the game can count are accepted" : "one frame more is refused");
			Parent_File = 0;
		}
	}

	// What the chunk holds has to arrive.
	{
		File_Chunk file;
		Parent_File = &file;

		int numbers[1 + 6 + 5 + 7] = { 0 };
		numbers[0] = 1;
		PutAnimatedPolygon((char *)&numbers[1], 7, 1, 3, 1, 0);
		numbers[12] = 9;
		numbers[18] = 11;

		Animation_Chunk * chunk = new Animation_Chunk(&file, (char *)numbers, sizeof(numbers));

		Check(file.error_code == 0 && chunk->NumPolys == 1, "a real chunk is accepted");
		Check(chunk->AnimList[0]->poly == 7, "its polygon's number was read");
		Check(chunk->AnimList[0]->Seq[0]->Textures[0] == 9, "its frame's texture number was read");
		Check(chunk->AnimList[0]->Seq[0]->UVCoords[5] == 11, "and the last of its frame's coordinates");
		Parent_File = 0;
	}

	// The polygon it names, in a shape of one polygon.
	struct PolygonRow { int corners[5]; int polygon; bool accepted; char const * what; };
	PolygonRow const polygonRows[] =
	{
		{ { 0, 1, 2, -1, -1 }, 0,          true,  "a real triangle can be animated" },
		{ { 0, 1, 2, -1, -1 }, 1,          false, "a polygon past the last is refused" },
		{ { 0, 1, 2, -1, -1 }, -1,         false, "a negative polygon is refused" },
		{ { 0, 1, 2, -1, -1 }, 0x7fffffff, false, "a polygon far past the last is refused" },
		{ { 0, 1, 2,  3, -1 }, 0,          false, "a polygon with four corners is refused" },
		{ { 0, 0, 1, -1, -1 }, 0,          false, "a triangle with its first two corners the same is refused" },
		{ { 0, 1, 1, -1, -1 }, 0,          false, "a triangle with its last two corners the same is refused" },
		{ { 1, 0, 1, -1, -1 }, 0,          false, "a triangle with its first and last corners the same is refused" },
	};

	for (size_t i = 0; i < sizeof(polygonRows) / sizeof(polygonRows[0]); i++)
	{
		size_t const bytes = BuildShapeData(wholeShape, false, 4, polygonRows[i].corners);

		Check(ShapeIsAccepted(PutAnimation(wholeShape + bytes, polygonRows[i].polygon, 1)) == polygonRows[i].accepted, polygonRows[i].what);
	}

	// A number outside the list reads whatever is there, and that's unlikely to look like a triangle, so the shape above would be refused
	// whether or not the number was checked. Here the shape's one polygon sits in the middle of three good triangles, so the only thing
	// that can refuse these numbers is the check on them.
	{
		struct NeighbourRow { int polygon; bool accepted; char const * what; };
		NeighbourRow const neighbourRows[] =
		{
			{  0, true,  "the polygon between two good ones is accepted" },
			{  1, false, "a polygon just past the last is refused, even with a good triangle there" },
			{ -1, false, "a polygon just before the first is refused, even with a good triangle there" },
		};

		for (size_t i = 0; i < sizeof(neighbourRows) / sizeof(neighbourRows[0]); i++)
		{
			File_Chunk file;
			Parent_File = &file;

			size_t const bytes = BuildShapeData(wholeShape, false, 4, triangle);
			char * end = PutAnimation(wholeShape + bytes, neighbourRows[i].polygon, 1);

			Shape_Chunk * shape = new Shape_Chunk(&file, wholeShape, end - wholeShape);

			ChunkPoly threeTriangles[3];

			for (int j = 0; j < 3; j++)
			{
				threeTriangles[j] = shape->shape_data.poly_list[0];
			}

			ChunkPoly * const ownList = shape->shape_data.poly_list;
			ChunkShape & data = const_cast<ChunkShape &>(shape->shape_data);
			data.poly_list = &threeTriangles[1];

			Check(shape->numbers_are_valid() == neighbourRows[i].accepted, neighbourRows[i].what);

			data.poly_list = ownList;
			Parent_File = 0;
		}
	}

	// A merge pair. If both polygons are animated, the game reads the second's frames by the first's counts.
	struct PairRow { int firstSequences; int firstFrames; int secondSequences; int secondFrames; bool accepted; char const * what; };
	PairRow const pairRows[] =
	{
		{ 1, 2, 1, 2, true,  "a pair that matches is accepted" },
		{ 2, 2, 2, 2, true,  "a pair that matches in two sequences is accepted" },
		{ 1, 2, 0, 0, true,  "a pair with only its first polygon animated is accepted" },
		{ 1, 3, 1, 2, false, "a second polygon with fewer frames is refused" },
		{ 1, 2, 1, 3, false, "a second polygon with more frames is refused" },
		{ 2, 2, 1, 2, false, "a second polygon with fewer sequences is refused" },
		{ 1, 2, 2, 2, false, "a second polygon with more sequences is refused" },
	};

	for (size_t i = 0; i < sizeof(pairRows) / sizeof(pairRows[0]); i++)
	{
		PairRow const & row = pairRows[i];

		char * end = PutShapeHeader(wholeShape, 4, 2);
		end = PutPoints(end, 4);
		end = PutPolygons(end, 2, triangle);
		memcpy(end - 36 + 16, secondTriangle, 5 * sizeof(int));
		end = PutNormals(end, 4, 2);

		int const merge[2] = { 1, 0 };
		char * mergeData = PutChunk(end, "SHPMRGDT", sizeof(merge));
		memcpy(mergeData, merge, sizeof(merge));
		end = mergeData + sizeof(merge);

		int const polygons = row.secondSequences ? 2 : 1;
		char list[1000];
		memcpy(list, &polygons, sizeof(polygons));

		char * listEnd = PutAnimatedPolygon(list + sizeof(polygons), 0, row.firstSequences, 3, row.firstFrames, 0);

		if (row.secondSequences)
		{
			listEnd = PutAnimatedPolygon(listEnd, 1, row.secondSequences, 3, row.secondFrames, 0);
		}

		char * data = PutChunk(end, "TEXTANIM", listEnd - list);
		memcpy(data, list, listEnd - list);

		Check(ShapeIsAccepted(data + (listEnd - list)) == row.accepted, row.what);
	}

	// An animation replaces its polygon's list of texture coordinates, so a polygon that isn't animated can't be using the same one.
	struct SharingRow { unsigned int secondColour; bool accepted; char const * what; };
	SharingRow const sharingRows[] =
	{
		{ 0x00010000, true,  "a polygon with a list of its own is accepted beside an animated one" },
		{ 0x00000000, false, "a polygon sharing an animated polygon's list is refused" },
	};

	for (size_t i = 0; i < sizeof(sharingRows) / sizeof(sharingRows[0]); i++)
	{
		char * end = PutShapeHeader(wholeShape, 4, 2);
		end = PutPoints(end, 4);
		end = PutPolygons(end, 2, triangle);

		int const kind = I_3dTexturedPolygon;
		memcpy(end - 2 * 36, &kind, sizeof(kind));
		memcpy(end - 36, &kind, sizeof(kind));
		memcpy(end - 36 + 12, &sharingRows[i].secondColour, sizeof(unsigned int));

		end = PutNormals(end, 4, 2);
		end = PutUVLists(end, 2, 3);

		Check(ShapeIsAccepted(PutAnimation(end, 0, 1)) == sharingRows[i].accepted, sharingRows[i].what);
	}

	// A sub-shape isn't a Shape_Chunk, which is what the game takes the holder of this chunk to be.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, true, 4, triangle);
		char * end = PutAnimation(wholeShape + bytes, 0, 1);

		new Shape_Sub_Shape_Chunk(&file, wholeShape, end - wholeShape);
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "texture animation in a sub-shape is refused");
		Parent_File = 0;
	}

	// Each frame's picture. These can only be checked once the level's pictures are known, so the game asks this function, with a list that
	// has a picture for texture numbers 0 and 2.
	{
		int const list[4] = { 99, 5, -1, 7 };
		int const * pictures = &list[1];

		struct PictureRow { int first; int second; int const * list; bool known; char const * what; };
		PictureRow const pictureRows[] =
		{
			{  0, 2, pictures, true,  "frames with real pictures are known" },
			{  1, 2, pictures, false, "a texture number with no picture is not known" },
			{  0, 1, pictures, false, "the same in a later frame" },
			{  0, 3, pictures, false, "a texture number past the end of the list is not known" },
			{ -1, 0, pictures, false, "a negative texture number is not known" },
			{  0, 2, 0,        false, "with no list at all, nothing is known" },
		};

		for (size_t i = 0; i < sizeof(pictureRows) / sizeof(pictureRows[0]); i++)
		{
			File_Chunk file;
			Parent_File = &file;

			int numbers[1 + 6 + 5 + 14] = { 0 };
			numbers[0] = 1;
			PutAnimatedPolygon((char *)&numbers[1], 0, 1, 3, 2, 0);
			numbers[12] = pictureRows[i].first;
			numbers[13] = pictureRows[i].second;

			Animation_Chunk * chunk = new Animation_Chunk(&file, (char *)numbers, sizeof(numbers));

			Check(chunk->AnimList[0]->ImagesAreKnown(2, pictureRows[i].list) == pictureRows[i].known, pictureRows[i].what);
			Parent_File = 0;
		}
	}
}

// 32. A generator's settings: twenty bytes, then a block of weights whose first number is its own size. That size went straight into a
// copy, so it has to fill the rest of the chunk exactly. The chunk is put at the wall, so reading a byte past its end stops the test.
static void TestGeneratorSettings()
{
	puts("32. a generator's settings");

	struct SettingsRow { size_t bytes; int blockSize; bool accepted; char const * what; };
	SettingsRow const rows[] =
	{
		{  84,  64, true,  "the size the game's own levels have is accepted" },
		{  24,   4, true,  "a shorter block is accepted, as the rest of the weights stay at 0" },
		{ 100,  80, true,  "a longer block is accepted" },
		{  23,   3, false, "a chunk with no room for the block's size is refused" },
		{  84,  65, false, "a block one byte longer than the chunk is refused" },
		{  84,  63, false, "a block one byte shorter than the chunk is refused" },
		{  84,  -1, false, "a negative size is refused" },
		{  84, 0x7fffffff, false, "a huge size is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		char * atTheWall = MemoryEndingAtAWall(rows[i].bytes);
		memset(atTheWall, 0, rows[i].bytes);

		if (rows[i].bytes >= 24)
		{
			memcpy(atTheWall + 20, &rows[i].blockSize, sizeof(int));
		}

		AVP_Generator_Extended_Settings_Chunk * settings = new AVP_Generator_Extended_Settings_Chunk(&file, atTheWall, rows[i].bytes);

		Check((file.error_code == 0) == rows[i].accepted, rows[i].what);
		Check(settings->weights != 0 && settings->weights->data_size >= (int)sizeof(AVP_Generator_Weighting), "and it can still be read");
		Parent_File = 0;
	}

	// The weights have to arrive. The third number of the block is in place 8.
	{
		File_Chunk file;
		Parent_File = &file;

		int const flameMarines = 7;
		int const blockSize = 64;
		alignas(int) char bytes[84] = { 0 };
		memcpy(bytes + 20, &blockSize, sizeof(int));
		memcpy(bytes + 20 + 8, &flameMarines, sizeof(int));

		AVP_Generator_Extended_Settings_Chunk * settings = new AVP_Generator_Extended_Settings_Chunk(&file, bytes, sizeof(bytes));

		Check(settings->weights->FlameMarine_Wt == flameMarines, "a weight arrives where it should");
		Parent_File = 0;
	}
}

// 33. Three more chunks with counts that size a list. Each goes at the wall, so reading a byte past its end stops the test.
static bool ConstructedAtTheWall(char const * bytes, size_t size, int kind)
{
	File_Chunk file;
	Parent_File = &file;

	char * atTheWall = MemoryEndingAtAWall(size);
	memcpy(atTheWall, bytes, size);

	if (kind == 0)
	{
		new Placed_Hierarchy_Data_Chunk(&file, atTheWall, size);
	}
	else if (kind == 1)
	{
		new Placed_Hierarchy_Sequence_Chunk(&file, atTheWall, size);
	}
	else
	{
		new Rainbow6_Generator_Extra_Data_Chunk(&file, atTheWall, size);
	}

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

static void TestPlacedHierarchies()
{
	puts("33. placed hierarchies, and Rainbow 6 generators");

	alignas(int) static char bytes[256];

	// A name of 3 letters takes 4 bytes. Then 40 bytes of numbers, 4 for the count, then the list.
	size_t const numbersBytes = 4 + 12 + 16 + 8;
	size_t const dataCountAt = 4 + numbersBytes;

	struct DataRow { size_t size; int count; bool accepted; char const * what; };
	DataRow const dataRows[] =
	{
		{ dataCountAt + 4,     0, true,  "a placed hierarchy with no extra numbers is accepted" },
		{ dataCountAt + 12,    2, true,  "a placed hierarchy with two is accepted" },
		{ dataCountAt + 4,     1, false, "a count too big for the chunk is refused" },
		{ dataCountAt + 8,     0, false, "bytes left over are refused" },
		{ dataCountAt + 4,    -1, false, "a negative count is refused" },
		{ dataCountAt + 4, 0x40000001, false, "a count that wraps round is refused" },
		{ dataCountAt,         0, false, "a chunk with no room for its count is refused" },
	};

	for (size_t i = 0; i < sizeof(dataRows) / sizeof(dataRows[0]); i++)
	{
		memset(bytes, 0, sizeof(bytes));
		memcpy(bytes, "abc", 3);

		if (dataRows[i].size >= dataCountAt + 4)
		{
			memcpy(bytes + dataCountAt, &dataRows[i].count, sizeof(int));
		}

		Check(ConstructedAtTheWall(bytes, dataRows[i].size, 0) == dataRows[i].accepted, dataRows[i].what);
	}

	memset(bytes, 'x', sizeof(bytes));
	Check(!ConstructedAtTheWall(bytes, 60, 0), "a name with no end is refused");

	// Five numbers, the count of sounds, 12 bytes for each sound, the count of extra numbers, then those.
	size_t const soundCountAt = 20;

	struct SequenceRow { size_t size; int sounds; int extras; bool accepted; char const * what; };
	SequenceRow const sequenceRows[] =
	{
		{ 28, 0, 0, true,  "a sequence with no sounds is accepted" },
		{ 52, 2, 0, true,  "a sequence with two sounds is accepted" },
		{ 44, 1, 1, true,  "a sequence with a sound and an extra number is accepted" },
		{ 24, 0, 0, false, "a chunk with no room for its second count is refused" },
		{ 28, 1, 0, false, "more sounds than the chunk holds are refused" },
		{ 28, -1, 0, false, "a negative count of sounds is refused" },
		{ 28, 0x7fffffff, 0, false, "a huge count of sounds is refused" },
		{ 28, 0x15555556, 0, false, "a count of sounds that wraps round is refused" },
		{ 28, 0, 1, false, "more extra numbers than the chunk holds are refused" },
		{ 28, 0, -1, false, "a negative count of extra numbers is refused" },
		{ 32, 0, 0, false, "bytes left over after the lists are refused" },
	};

	for (size_t i = 0; i < sizeof(sequenceRows) / sizeof(sequenceRows[0]); i++)
	{
		memset(bytes, 0, sizeof(bytes));

		if (sequenceRows[i].size >= soundCountAt + 4)
		{
			memcpy(bytes + soundCountAt, &sequenceRows[i].sounds, sizeof(int));
		}

		// the second count follows the sounds, as long as they're a real number of sounds
		if (sequenceRows[i].sounds >= 0 && sequenceRows[i].sounds <= 2 && soundCountAt + 8 + 12 * (size_t)sequenceRows[i].sounds <= sequenceRows[i].size)
		{
			memcpy(bytes + soundCountAt + 4 + 12 * sequenceRows[i].sounds, &sequenceRows[i].extras, sizeof(int));
		}

		Check(ConstructedAtTheWall(bytes, sequenceRows[i].size, 1) == sequenceRows[i].accepted, sequenceRows[i].what);
	}

	struct GeneratorRow { size_t size; int count; bool accepted; char const * what; };
	GeneratorRow const generatorRows[] =
	{
		{ 12,  0, true,  "a Rainbow 6 generator with no list is accepted" },
		{ 20,  2, true,  "a Rainbow 6 generator with a list is accepted" },
		{ 24,  2, true,  "with no size measured from the game's levels, spare bytes are allowed" },
		{ 16,  2, false, "a list too long for the chunk is refused" },
		{ 16, -1, false, "a negative count is refused" },
		{ 16, 0x40000001, false, "a count that wraps round is refused" },
		{  8,  0, false, "a chunk with no room for its count is refused" },
	};

	for (size_t i = 0; i < sizeof(generatorRows) / sizeof(generatorRows[0]); i++)
	{
		memset(bytes, 0, sizeof(bytes));

		if (generatorRows[i].size >= 12)
		{
			memcpy(bytes + 8, &generatorRows[i].count, sizeof(int));
		}

		Check(ConstructedAtTheWall(bytes, generatorRows[i].size, 2) == generatorRows[i].accepted, generatorRows[i].what);
	}
}

// 34. A module's extents, and the ball round it that the shadow pass trusts, come from the box stored with its shape. So a module's points have to
// fit that box. Other shapes don't have to: some of the game's own have points outside theirs.
static void TestModuleBoxes()
{
	puts("34. a module's box");

	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const shapeNumber = 7;

	struct BoxRow { bool module; int pointX; bool accepted; char const * what; };
	BoxRow const rows[] =
	{
		{ true,  0, true,  "a module whose points fit its box is accepted" },
		{ true,  1, false, "a module with a point past the top of its box is refused" },
		{ true, -1, false, "a module with a point past the bottom of its box is refused" },
		{ false, 1, true,  "an object that isn't a module can have a point outside its box" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const shapeBytesUsed = BuildShapeData(wholeShape, false, 4, triangle);
		memcpy(wholeShape + ChunkHeaderBytes + ShapeNumberAt, &shapeNumber, sizeof(shapeNumber));
		memcpy(wholeShape + ChunkHeaderBytes + ShapeHeaderDataBytes + ChunkHeaderBytes, &rows[i].pointX, sizeof(int));

		new Shape_Chunk(&file, wholeShape, shapeBytesUsed);

		size_t objectBytesUsed = BuildObject(5, shapeNumber);

		if (rows[i].module)
		{
			PutChunk(objectBytes + objectBytesUsed, "MODULEDT", 0);
			objectBytesUsed += ChunkHeaderBytes;
		}

		new Object_Chunk(&file, objectBytes, objectBytesUsed);
		file.post_input_processing();

		Check((file.error_code == 0) == rows[i].accepted, rows[i].what);
		Parent_File = 0;
	}
}

// 35. A strategy chunk holds two numbers, then a strategy, whose first number says what kind it is. Each kind reads a fixed set of numbers with no
// look at the chunk, so it reads from a padded copy, and afterwards it has to be exactly the size its class says. Each chunk here is at the
// wall, so if anything reads the chunk itself past its end, this stops.
static size_t const StrategyNumbersBytes = 8;

static bool StrategyAccepted(char const * strategy, size_t strategyBytes)
{
	File_Chunk file;
	Parent_File = &file;

	size_t const bytes = StrategyNumbersBytes + strategyBytes;
	char * atTheWall = MemoryEndingAtAWall(bytes);
	memset(atTheWall, 0, StrategyNumbersBytes);
	memcpy(atTheWall + StrategyNumbersBytes, strategy, strategyBytes);

	new AVP_Strategy_Chunk(&file, atTheWall, bytes);

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// How big a strategy of this kind says it is, with every list in it empty.
static size_t StrategySizeOfKind(int kind)
{
	File_Chunk file;
	Parent_File = &file;

	alignas(int) static char bytes[1024];
	memset(bytes, 0, sizeof(bytes));
	memcpy(bytes + StrategyNumbersBytes, &kind, sizeof(kind));

	AVP_Strategy_Chunk * chunk = new AVP_Strategy_Chunk(&file, bytes, sizeof(bytes));
	size_t const size = chunk->Strategy->GetStrategySize();

	Parent_File = 0;

	return size;
}

static void TestStrategies()
{
	puts("35. strategies");

	alignas(int) static char strategy[1024];
	size_t const biggestAsked = sizeof(strategy);

	// Every number a kind can be. Any kind the game doesn't know is kept whole as plain bytes, and is as big as its chunk, so it has no size to break.
	for (int kind = 0; kind < 128; kind++)
	{
		size_t const size = StrategySizeOfKind(kind);

		if (size + StrategyNumbersBytes >= biggestAsked)
		{
			continue;
		}

		memset(strategy, 0, sizeof(strategy));
		memcpy(strategy, &kind, sizeof(kind));

		Check(StrategyAccepted(strategy, size), "a strategy of its own size is accepted");
		Check(!StrategyAccepted(strategy, size - 1), "one byte short of its size is refused");
		Check(!StrategyAccepted(strategy, size + 4), "four bytes over its size is refused");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		char * atTheWall = MemoryEndingAtAWall(StrategyNumbersBytes + 3);
		memset(atTheWall, 0, StrategyNumbersBytes + 3);

		new AVP_Strategy_Chunk(&file, atTheWall, StrategyNumbersBytes + 3);

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a chunk with no room for its kind is refused");
		Parent_File = 0;
	}

	// Each counted list, and where its count is with every list before it empty. An item's size is what each entry in the list takes.
	struct ListRow { int kind; size_t countAt; size_t itemBytes; char const * good; };
	ListRow const rows[] =
	{
		{ StratLift,             12, 8,                                   "a lift with lifts joined to it is accepted" },
		{ StratLift,             48, sizeof(ExtLift),                     "a lift with external lifts is accepted" },
		{ StratMultiSwitch,      28, sizeof(SwitchTarget),                "a multi switch with targets is accepted" },
		{ StratMultiSwitch,      32, 8,                                   "a multi switch with links is accepted" },
		{ StratMissionObjective, 32, sizeof(MissionMessage),              "a mission objective with targets is accepted" },
		{ StratMissionObjective, 36, sizeof(RequestTarget),               "a mission objective with requests is accepted" },
		{ StratTrack,            12, 3 * sizeof(int),                     "a track with effects is accepted" },
		{ StratHierarchy,        12, 3 * sizeof(int),                     "a hierarchy with effects is accepted" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		ListRow const & row = rows[i];
		size_t const size = StrategySizeOfKind(row.kind);
		int const two = 2;

		// Items go right after the count, so everything after them moves along, and it's all zeros.
		memset(strategy, 0, sizeof(strategy));
		memcpy(strategy, &row.kind, sizeof(row.kind));
		memcpy(strategy + row.countAt, &two, sizeof(two));
		Check(StrategyAccepted(strategy, size + 2 * row.itemBytes), row.good);

		int const badCounts[] = { -1, 0x7fffffff, (int)(0x100000000ULL / row.itemBytes) + 1, 3 };

		for (size_t j = 0; j < sizeof(badCounts) / sizeof(badCounts[0]); j++)
		{
			memcpy(strategy + row.countAt, &badCounts[j], sizeof(int));
			Check(!StrategyAccepted(strategy, size + 2 * row.itemBytes), "a count that doesn't fit its chunk is refused");
		}
	}

	// A link switch's count is its last number.
	{
		size_t const size = StrategySizeOfKind(StratLinkSwitch);
		int const kind = StratLinkSwitch;
		int const two = 2;

		memset(strategy, 0, sizeof(strategy));
		memcpy(strategy, &kind, sizeof(kind));
		memcpy(strategy + size - sizeof(int), &two, sizeof(two));
		Check(StrategyAccepted(strategy, size + 2 * 8), "a link switch with links is accepted");
		Check(!StrategyAccepted(strategy, size + 8), "a link switch with too few links for its count is refused");
	}

	// An effect's own list of targets. The third number of an effect is that list's count.
	{
		size_t const size = StrategySizeOfKind(StratTrack);
		int const kind = StratTrack;
		int const one = 1;
		int const tooMany = 0x7fffffff;

		memset(strategy, 0, sizeof(strategy));
		memcpy(strategy, &kind, sizeof(kind));
		memcpy(strategy + 12, &one, sizeof(one));
		memcpy(strategy + 12 + 4 + 8, &tooMany, sizeof(tooMany));
		Check(!StrategyAccepted(strategy, size + 12), "an effect with too many targets is refused");
	}

	// A list of effects that fits by count, until the first effect's targets use up the room. Called directly and not through the chunk, because the
	// chunk's spare bytes would hide this. The strategy's last two numbers still get read, so the wall is those 8 bytes further on.
	{
		File_Chunk file;
		Parent_File = &file;

		int const kind = StratTrack;
		int const effects = 4;
		int const targets = 2;
		size_t const room = 12 + 4 + 4 * 12;

		memset(strategy, 0, sizeof(strategy));
		memcpy(strategy, &kind, sizeof(kind));
		memcpy(strategy + 12, &effects, sizeof(effects));
		memcpy(strategy + 12 + 4 + 8, &targets, sizeof(targets));

		char * atTheWall = MemoryEndingAtAWall(room + 8);
		memset(atTheWall, 0, room + 8);
		memcpy(atTheWall, strategy, room);

		new TrackStrategy(atTheWall, room);

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "effects that run out of room are refused");
		Parent_File = 0;
	}
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
	TestRawMemoryShape();
	TestRepeatedAndMissingChunks();
	TestMergeData();
	TestAnimatedShapes();
	TestTextureCoordinates();
	TestNormals();
	TestLifts();
	TestMorphFrames();
	TestChunksInTheWrongPlace();
	TestNesting();
	TestChunkBoundChecks();
	TestTextureAnimation();
	TestGeneratorSettings();
	TestPlacedHierarchies();
	TestModuleBoxes();
	TestStrategies();

	if (checksFailed)
	{
		printf("Rebecca's test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("Rebecca's test passed");
	return 0;
}
