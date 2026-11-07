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
#include <malloc.h>
#include <memory>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

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
#include "animation_objects.hpp"
#include "bitmap_names.hpp"
#include "chunk_palette.hpp"
#include "enumsch.hpp"
#include "chunk_sound.hpp"
#include "chunk_sprites.hpp"
#include "chunk_waypoint.hpp"
#include "path_chunk.hpp"
#include "chunk_dummy.hpp"
#include "chunk_fragments.hpp"
#include <new>

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

void LogErrorStringC(const char *)
{
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

// The same, for words kept as a string. They're written out with their own length, so no ending 0 is looked for.
static void Check(bool passed, std::string const & what)
{
	printf("  %s  %.*s\n", passed ? "ok    " : "FAILED", static_cast<int>(what.size()), what.data());
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

// Each chunk a test makes has an owner, which gives it back at the end of the block and lifts it out of its parent as it goes. The parent would free it
// anyway, but this way it's plain to see that nothing is left behind.

// Hands the bytes to the game's real constructor, which walks the child chunks and makes the real Object_Header_Chunk.
static std::unique_ptr<Object_Chunk> LoadObject(TestFile & file, size_t bytes)
{
	return std::unique_ptr<Object_Chunk>(new Object_Chunk(&file, objectBytes, bytes));
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
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, BuildObjectHeader(name, sizeof(name)));
	Object_Chunk * object = objectOwner.get();

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
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, BuildObjectHeader(LongestName, sizeof(LongestName)));
	Object_Chunk * object = objectOwner.get();

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
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, BuildObjectHeader(name, sizeof(name)));
	Object_Chunk * object = objectOwner.get();

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
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, bytes);
	Object_Chunk * object = objectOwner.get();

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
		std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, ChunkHeaderBytes + tooShort[i]);
		Object_Chunk * object = objectOwner.get();

		Check(object->get_header() != 0, "the header chunk was loaded");
		Check(NameLength(object) == 0, "the name is empty");
	}

	size_t const bytes = BuildObjectHeader(LongestName, 1);
	objectBytes[bytes + 20] = '\0';

	TestFile file;
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, bytes);
	Object_Chunk * object = objectOwner.get();

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
		std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, parentBytes);
		Object_Chunk * object = objectOwner.get();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, badSizes[i].what);
		Check(object->get_header() == 0, "no child was made");
		Check(NameLength(object) == 0, "the name is empty");
	}

	// Fewer bytes left in the parent than a child's header needs, right after a good child.
	size_t const goodBytes = BuildObjectHeader(LongestName, sizeof(LongestName));

	TestFile file;
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, goodBytes + 8);
	Object_Chunk * object = objectOwner.get();

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
		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		Shape_Chunk * shape = shapeOwner.get();
		size_t const bytes = BuildObject(5, 7);
		std::unique_ptr<Object_Chunk> const objectOwner(new Object_Chunk(&file, objectBytes, bytes));
		Object_Chunk * object = objectOwner.get();
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
		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		Shape_Chunk * shape = shapeOwner.get();
		size_t const bytes = BuildObject(-1, 7);
		std::unique_ptr<Object_Chunk> const objectOwner(new Object_Chunk(&file, objectBytes, bytes));
		Object_Chunk * object = objectOwner.get();
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
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		size_t const bytes = BuildObject(badLevels[i].objectNumber, badLevels[i].shapeItNames);
		std::unique_ptr<Object_Chunk> const made2(new Object_Chunk(&file, objectBytes, bytes));
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, badLevels[i].what);
		Check(file.get_object_by_index(0) == 0, "and looking up an object is still safe");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		std::unique_ptr<Object_Chunk> const made2(new Object_Chunk(&file, objectBytes, 0));
		file.post_input_processing();

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "an object with no header is refused");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, shapeBytes, 0));
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
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		file.post_input_processing();

		Check((file.error_code == 0) == pointCounts[i].accepted, pointCounts[i].what);
	}

	Parent_File = 0;
}

// The bytes of a hierarchy's one section, as its outermost chunk holds it: an OBJCHIER chunk holding an OBJHIERD chunk with a count of extra numbers, here 0, and then its name.
static char sectionBytes[ChunkHeaderBytes + ChunkHeaderBytes + 4 + 8];

static void BuildSection(char const * name, size_t nameBytes)
{
	int const sectionChunkBytes = (int)sizeof(sectionBytes);
	int const nameChunkBytes = sectionChunkBytes - (int)ChunkHeaderBytes;

	memset(sectionBytes, 0, sizeof(sectionBytes));
	memcpy(sectionBytes, "OBJCHIER", 8);
	memcpy(sectionBytes + 8, &sectionChunkBytes, sizeof(sectionChunkBytes));
	memcpy(sectionBytes + ChunkHeaderBytes, "OBJHIERD", 8);
	memcpy(sectionBytes + ChunkHeaderBytes + 8, &nameChunkBytes, sizeof(nameChunkBytes));
	memcpy(sectionBytes + ChunkHeaderBytes + ChunkHeaderBytes + 4, name, nameBytes);
}

// 8. A hierarchy section finds its object by name, so a section with a blank name has to refuse the level.
static void TestHierarchySections()
{
	puts("8. hierarchy sections");

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		size_t const bytes = BuildObject(5, 7);
		std::unique_ptr<Object_Chunk> const objectOwner(new Object_Chunk(&file, objectBytes, bytes));
		Object_Chunk * object = objectOwner.get();
		BuildSection("object", 7);
		std::unique_ptr<Object_Hierarchy_Chunk> const hierarchyOwner(new Object_Hierarchy_Chunk(&file, sectionBytes, sizeof(sectionBytes)));
		Object_Hierarchy_Chunk * hierarchy = hierarchyOwner.get();
		file.post_input_processing();

		Check(file.error_code == 0, "a section with a name is accepted");
		Check(hierarchy->list_h_children().first_entry()->get_data()->object == object, "and it finds the object with that name");
	}

	{
		File_Chunk file;
		Parent_File = &file;

		BuildShape(7);
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, shapeBytes, sizeof(shapeBytes)));
		size_t const bytes = BuildObject(5, 7);
		std::unique_ptr<Object_Chunk> const made2(new Object_Chunk(&file, objectBytes, bytes));
		BuildSection("", 1);
		std::unique_ptr<Object_Hierarchy_Chunk> const made3(new Object_Hierarchy_Chunk(&file, sectionBytes, sizeof(sectionBytes)));
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

		std::unique_ptr<Shape_Vertex_Intensities_Chunk> const coloursOwner(new Shape_Vertex_Intensities_Chunk(&file, bytes, colourChunks[i].bytes));
		Shape_Vertex_Intensities_Chunk * colours = coloursOwner.get();

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

		std::unique_ptr<Shape_Vertex_Intensities_Chunk> const coloursOwner(new Shape_Vertex_Intensities_Chunk(&file, bigChunk, sizeof(bigChunk)));
		Shape_Vertex_Intensities_Chunk * colours = coloursOwner.get();

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
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, wholeShape, bytes));
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

		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, wholeShape, bytes));
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

		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, wholeShape, bytes));
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
		std::unique_ptr<Shape_Sub_Shape_Chunk> const made(new Shape_Sub_Shape_Chunk(&file, wholeShape, bytes));
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

		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, wholeShape, mainBytes + 2 * ChunkHeaderBytes + frameBytes));
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

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, mainBytes + ChunkHeaderBytes + rawBytes));
		Shape_Chunk * shape = shapeOwner.get();
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

	std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, wholeShape, end - wholeShape));
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

	// Each place is 4 bytes, so this is 2 GB from wherever the list is, which is outside the addresses a program like this one
	// can read, in either direction. A partner that's only a little way off reads whatever is there, which could refuse it by luck.
	int const farAway = 0x20000000;

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
	int const negativeFrames[] = { -1, -0x20000000 };

	for (size_t i = 0; i < sizeof(negativeFrames) / sizeof(negativeFrames[0]); i++)
	{
		File_Chunk file;
		Parent_File = &file;

		char * end = PutAnimation(wholeShape + bytes, 0, true, negativeFrames[i], 4, 1, 0);
		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, end - wholeShape));
		Shape_Chunk * shape = shapeOwner.get();

		Check(shape->count_children("ANIMSEQU") == 0, "a sequence with a negative frame number is thrown away");
	}

	// A sub-shape that fails has to stop there. Working out an animation's normals is the step that would write through the
	// bad corner, and that step is what makes the list of normals, so an empty list shows that it never ran.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const subBytes = BuildShapeData(wholeShape, true, 4, badCorner);
		char * end = PutAnimation(wholeShape + subBytes, 0, true, 0, 4, 1, 0);

		std::unique_ptr<Shape_Sub_Shape_Chunk> const subShapeOwner(new Shape_Sub_Shape_Chunk(&file, wholeShape, end - wholeShape));
		Shape_Sub_Shape_Chunk * subShape = subShapeOwner.get();
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

		std::unique_ptr<Shape_Sub_Shape_Chunk> const made(new Shape_Sub_Shape_Chunk(&file, wholeShape, end - wholeShape));

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

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, end - wholeShape));
		Shape_Chunk * shape = shapeOwner.get();

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

		std::unique_ptr<Shape_Sub_Shape_Chunk> const made(new Shape_Sub_Shape_Chunk(&file, wholeShape, bytes));
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

		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, atTheWall, allBytes));
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

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, (frames + sizeof(numbers)) - wholeShape));
		Shape_Chunk * shape = shapeOwner.get();
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

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, bytes + ChunkHeaderBytes + wrongPlaceBytes));
		Shape_Chunk * shape = shapeOwner.get();
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

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, bytes + 2 * ChunkHeaderBytes + 16));
		Shape_Chunk * shape = shapeOwner.get();
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

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, bytes + ChunkHeaderBytes + 16));
		Shape_Chunk * shape = shapeOwner.get();

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

		std::unique_ptr<Environment_Data_Chunk> const made(new Environment_Data_Chunk(&file, wholeShape, ChunkHeaderBytes + modeBytes));

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

		std::unique_ptr<Special_Objects_Chunk> const made(new Special_Objects_Chunk(&file, nested, rows[i].levels * ChunkHeaderBytes));

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
// objectNamesKept, if it's asked for, gets how many names the shape's header ended up holding in its list of objects.
static bool ShapeIsAcceptedAtTheWall(char const * end, int * objectNamesKept = 0)
{
	size_t const allBytes = end - wholeShape;
	char * atTheWall = MemoryEndingAtAWall(allBytes);

	memcpy(atTheWall, wholeShape, allBytes);

	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, atTheWall, allBytes));
	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	if (objectNamesKept)
	{
		*objectNamesKept = made->get_header()->object_names_store.size();
	}

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

			std::unique_ptr<Animation_Chunk> const made(new Animation_Chunk(&file, (char *)bigChunk, bytes));

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

		std::unique_ptr<Animation_Chunk> const chunkOwner(new Animation_Chunk(&file, (char *)numbers, sizeof(numbers)));
		Animation_Chunk * chunk = chunkOwner.get();

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

			std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, end - wholeShape));
			Shape_Chunk * shape = shapeOwner.get();

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

		std::unique_ptr<Shape_Sub_Shape_Chunk> const made(new Shape_Sub_Shape_Chunk(&file, wholeShape, end - wholeShape));
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

			std::unique_ptr<Animation_Chunk> const chunkOwner(new Animation_Chunk(&file, (char *)numbers, sizeof(numbers)));
			Animation_Chunk * chunk = chunkOwner.get();

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

		std::unique_ptr<AVP_Generator_Extended_Settings_Chunk> const settingsOwner(new AVP_Generator_Extended_Settings_Chunk(&file, atTheWall, rows[i].bytes));
		AVP_Generator_Extended_Settings_Chunk * settings = settingsOwner.get();

		Check((file.error_code == 0) == rows[i].accepted, rows[i].what);
		Check(settings->weights != 0 && settings->weights->data_size >= (int)sizeof(AVP_Generator_Weighting), "and it can still be read");
		Parent_File = 0;
	}

	// A block longer than the game's own needs a longer list, or it would be written past the end of the one it has. Writing past the end of a list
	// doesn't always show, because what's next to it may be nothing that matters today. So this asks how long the list really is, which has one answer.
	{
		File_Chunk file;
		Parent_File = &file;

		int const blockSize = 80;
		alignas(int) char bytes[100] = { 0 };
		memcpy(bytes + 20, &blockSize, sizeof(int));

		std::unique_ptr<AVP_Generator_Extended_Settings_Chunk> const settings(new AVP_Generator_Extended_Settings_Chunk(&file, bytes, sizeof(bytes)));

		Check(_msize(settings->weights) >= (size_t)blockSize, "a longer block gets a list with room for all of it");
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

		std::unique_ptr<AVP_Generator_Extended_Settings_Chunk> const settingsOwner(new AVP_Generator_Extended_Settings_Chunk(&file, bytes, sizeof(bytes)));
		AVP_Generator_Extended_Settings_Chunk * settings = settingsOwner.get();

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
		std::unique_ptr<Placed_Hierarchy_Data_Chunk> const made(new Placed_Hierarchy_Data_Chunk(&file, atTheWall, size));
	}
	else if (kind == 1)
	{
		std::unique_ptr<Placed_Hierarchy_Sequence_Chunk> const made(new Placed_Hierarchy_Sequence_Chunk(&file, atTheWall, size));
	}
	else
	{
		std::unique_ptr<Rainbow6_Generator_Extra_Data_Chunk> const made(new Rainbow6_Generator_Extra_Data_Chunk(&file, atTheWall, size));
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

	// The name's own end marker is the very last byte of the chunk, so there's no room for the numbers that have to come after it
	memset(bytes, 'x', sizeof(bytes));
	bytes[59] = 0;
	Check(!ConstructedAtTheWall(bytes, 60, 0), "a name that fills its chunk, with no room for the numbers after it, is refused");

	// A name is copied up to its end marker and no further, and the numbers after it are read from the right place. The chunk is at the wall, and
	// what follows the end marker isn't a name, so a copy that went on past it would show.
	{
		TestFile file;

		size_t const size = 8 + numbersBytes + 4;
		char * atTheWall = MemoryEndingAtAWall(size);
		memset(atTheWall, 0, size);
		memcpy(atTheWall, "abcdefg", 8);

		int const hierarchyIndex = 1234;
		memcpy(atTheWall + 8, &hierarchyIndex, sizeof(int));

		std::unique_ptr<Placed_Hierarchy_Data_Chunk> const chunk(new Placed_Hierarchy_Data_Chunk(&file, atTheWall, size));

		Check(file.error_code == 0, "a placed hierarchy with a 7 letter name is accepted");
		Check(chunk->name != 0 && memcmp(chunk->name, "abcdefg", 8) == 0, "and its name is copied whole");
		Check(chunk->hierarchy_index == hierarchyIndex, "and what comes after the name is read from the right place");
	}

	// Stops at the first end marker, whatever comes after it
	{
		TestFile file;

		size_t const size = 8 + numbersBytes + 4;
		char * atTheWall = MemoryEndingAtAWall(size);
		memset(atTheWall, 0, size);
		memcpy(atTheWall, "ab\0defg", 8);

		std::unique_ptr<Placed_Hierarchy_Data_Chunk> const chunk(new Placed_Hierarchy_Data_Chunk(&file, atTheWall, 4 + numbersBytes + 4));

		Check(chunk->name != 0 && memcmp(chunk->name, "ab", 3) == 0, "a name stops at its first end marker");
	}

	// That chunk would be refused anyway, because its count can't be right. The one way past that takes a count with no zero byte in it, and a
	// chunk big enough to hold that many numbers, so this one is 67 MB of 1s. Only the check on the name can refuse it.
	{
		File_Chunk file;
		Parent_File = &file;

		size_t const hugeBytes = 44 + 4 * (size_t)0x01010101;
		char * huge = (char *)malloc(hugeBytes);
		memset(huge, 1, hugeBytes);

		std::unique_ptr<Placed_Hierarchy_Data_Chunk> const made(new Placed_Hierarchy_Data_Chunk(&file, huge, hugeBytes));

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a huge chunk with no zero byte at all is refused");
		free(huge);
		Parent_File = 0;
	}

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

		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, wholeShape, shapeBytesUsed));

		size_t objectBytesUsed = BuildObject(5, shapeNumber);

		if (rows[i].module)
		{
			PutChunk(objectBytes + objectBytesUsed, "MODULEDT", 0);
			objectBytesUsed += ChunkHeaderBytes;
		}

		std::unique_ptr<Object_Chunk> const made2(new Object_Chunk(&file, objectBytes, objectBytesUsed));
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

	std::unique_ptr<AVP_Strategy_Chunk> const made(new AVP_Strategy_Chunk(&file, atTheWall, bytes));

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

	std::unique_ptr<AVP_Strategy_Chunk> const chunkOwner(new AVP_Strategy_Chunk(&file, bytes, sizeof(bytes)));
	AVP_Strategy_Chunk * chunk = chunkOwner.get();
	size_t const size = chunk->Strategy->GetStrategySize();

	Parent_File = 0;

	return size;
}

// Chunks the game has no use for, kept as plain bytes that nothing reads. Each sits right up against memory that can't be read, so a chunk class that looked at what isn't there would crash the test.
static void TestChunksTheGameDoesNotRead()
{
	puts("38. chunks the game doesn't read");

	alignas(int) static char wholeShape[4096];

	// A console shape holding children that fit
	{
		File_Chunk file;
		Parent_File = &file;

		int const triangle[5] = { 0, 1, 2, -1, -1 };
		size_t const mainBytes = BuildShapeData(wholeShape, false, 4, triangle);
		size_t const typeBytes = ChunkHeaderBytes + sizeof(int);
		size_t const polygonBytes = ChunkHeaderBytes + 36;
		size_t const consoleBytes = typeBytes + polygonBytes;

		char * inside = PutChunk(wholeShape + mainBytes, "CONSHAPE", consoleBytes);
		PutChunk(inside, "CONSTYPE", sizeof(int));
		PutChunk(inside + typeBytes, "SHPPOLYS", 36);

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, mainBytes + ChunkHeaderBytes + consoleBytes));
		Shape_Chunk * shape = shapeOwner.get();
		Chunk * console = shape->lookup_single_child("CONSHAPE");

		Check(console && console->r_u_miscellaneous(), "a console shape holding children that fit is kept as plain bytes, and the loader goes on");
		Check(file.error_code == 0, "and the level still loads");
		Parent_File = 0;
	}

	// One with a child that doesn't fit is the same, because nothing looks inside it
	{
		File_Chunk file;
		Parent_File = &file;

		int const triangle[5] = { 0, 1, 2, -1, -1 };
		size_t const mainBytes = BuildShapeData(wholeShape, false, 4, triangle);
		char * inside = PutChunk(wholeShape + mainBytes, "CONSHAPE", ChunkHeaderBytes);
		PutChunk(inside, "CONSTYPE", 0);
		int const tooBig = 0x7fffffff;
		memcpy(inside + 8, &tooBig, sizeof(tooBig));

		std::unique_ptr<Shape_Chunk> const shapeOwner(new Shape_Chunk(&file, wholeShape, mainBytes + ChunkHeaderBytes + ChunkHeaderBytes));
		Shape_Chunk * shape = shapeOwner.get();
		Chunk * console = shape->lookup_single_child("CONSHAPE");

		Check(console && console->r_u_miscellaneous() && file.error_code == 0, "a console shape whose child says it's huge is the same, because nothing looks inside");
		Parent_File = 0;
	}

	// A CONSTYPE with no number in it, anywhere a chunk can be. It used to be read from wherever it was put.
	{
		File_Chunk file;
		Parent_File = &file;

		char * atTheWall = MemoryEndingAtAWall(ChunkHeaderBytes);
		PutChunk(atTheWall, "CONSTYPE", 0);

		std::unique_ptr<Environment_Data_Chunk> const levelOwner(new Environment_Data_Chunk(&file, atTheWall, ChunkHeaderBytes));
		Environment_Data_Chunk * level = levelOwner.get();
		Chunk * type = level->lookup_single_child("CONSTYPE");

		Check(type && type->r_u_miscellaneous() && file.error_code == 0, "a console type with nothing in it is kept as plain bytes, with nothing read past it");
		Parent_File = 0;
	}

	// An external strategy with nothing in it, and one saying it's a lift. These used to be read with no look at their size.
	{
		File_Chunk file;
		Parent_File = &file;

		char * atTheWall = MemoryEndingAtAWall(ChunkHeaderBytes);
		PutChunk(atTheWall, "AVPEXSTR", 0);

		std::unique_ptr<Environment_Data_Chunk> const levelOwner(new Environment_Data_Chunk(&file, atTheWall, ChunkHeaderBytes));
		Environment_Data_Chunk * level = levelOwner.get();
		Chunk * external = level->lookup_single_child("AVPEXSTR");

		Check(external && external->r_u_miscellaneous() && file.error_code == 0, "an external strategy with nothing in it is kept as plain bytes, with nothing read past it");
		Parent_File = 0;
	}

	{
		File_Chunk file;
		Parent_File = &file;

		size_t const dataBytes = 20 + sizeof(int);
		char * atTheWall = MemoryEndingAtAWall(ChunkHeaderBytes + dataBytes);
		char * data = PutChunk(atTheWall, "AVPEXSTR", dataBytes);
		int const kind = StratLift;
		memcpy(data + 20, &kind, sizeof(kind));

		std::unique_ptr<Environment_Data_Chunk> const levelOwner(new Environment_Data_Chunk(&file, atTheWall, ChunkHeaderBytes + dataBytes));
		Environment_Data_Chunk * level = levelOwner.get();
		Chunk * external = level->lookup_single_child("AVPEXSTR");

		Check(external && external->r_u_miscellaneous() && file.error_code == 0, "one saying it's a lift, with no lift in it, is the same");
		Parent_File = 0;
	}
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

		std::unique_ptr<AVP_Strategy_Chunk> const made(new AVP_Strategy_Chunk(&file, atTheWall, StrategyNumbersBytes + 3));

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a chunk with no room for its kind is refused");
		Parent_File = 0;
	}

	// A kind the game doesn't know keeps whatever comes after its first three numbers. With fewer than three there's a negative amount to keep, and asking for that much memory stops the game.
	{
		int const unknownKind = 0x7ffffff0;
		char strategy[16] = { 0 };
		memcpy(strategy, &unknownKind, sizeof(unknownKind));

		for (size_t bytes = sizeof(int); bytes < 12; bytes++)
		{
			File_Chunk file;
			Parent_File = &file;

			char * atTheWall = MemoryEndingAtAWall(StrategyNumbersBytes + bytes);
			memset(atTheWall, 0, StrategyNumbersBytes);
			memcpy(atTheWall + StrategyNumbersBytes, strategy, bytes);

			std::unique_ptr<AVP_Strategy_Chunk> const made(new AVP_Strategy_Chunk(&file, atTheWall, StrategyNumbersBytes + bytes));

			Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, (std::string("an unknown kind with ") + std::to_string(bytes) + " bytes, too few for its three numbers, is refused").c_str());
			Parent_File = 0;
		}

		Check(StrategyAccepted(strategy, 12), "an unknown kind with exactly its three numbers is accepted");
		Check(StrategyAccepted(strategy, 13), "and with one byte more, which it keeps");
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

		std::unique_ptr<TrackStrategy> const made(new TrackStrategy(atTheWall, room));

		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "effects that run out of room are refused");
		Parent_File = 0;
	}
}

// 36. Counted lists. Each of these chunks holds a count from the file, and then that many items. A count too big asks for more memory than there is,
// which ends the game, or reads on past the chunk. So each kind is tried with counts that can't be right. After the chunk come zeros and then a wall,
// as the game's padded file has zeros after a level: a little past the end is safe, and a long way past it stops the test.
static size_t const SpareAfterChunk = 256;

typedef void (*MakesChunk)(Chunk_With_Children * parent, char const * data, size_t size);

template <class Kind>
static void MakeChunk(Chunk_With_Children * parent, char const * data, size_t size)
{
	std::unique_ptr<Chunk> const made(new Kind(parent, data, size));
}

static bool ChunkAccepted(MakesChunk make, char const * bytes, size_t size)
{
	File_Chunk file;
	Parent_File = &file;

	char * atTheWall = MemoryEndingAtAWall(size + SpareAfterChunk);
	memcpy(atTheWall, bytes, size);
	memset(atTheWall + size, 0, SpareAfterChunk);

	make(&file, atTheWall, size);

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

static void PutNumber(char * bytes, size_t at, int number)
{
	memcpy(bytes + at, &number, sizeof(number));
}

// Counts that can't be right, for a chunk with room for exactly two items: past the end of what a number can be, one that wraps round to something small
// when it's multiplied by the size of an item, one that asks for nearly 2 GB, which is more memory than there is, one more than there's room for, and one
// that reads far past the wall.
static void TestBadCounts(MakesChunk make, char * bytes, size_t size, size_t countAt, size_t itemBytes)
{
	int const badCounts[] = { -1, 0x7fffffff, (int)(0x100000000ULL / itemBytes) + 1, (int)(0x7ffff000UL / itemBytes), 3, 4000 };

	for (size_t i = 0; i < sizeof(badCounts) / sizeof(badCounts[0]); i++)
	{
		PutNumber(bytes, countAt, badCounts[i]);
		Check(!ChunkAccepted(make, bytes, size), "a count that doesn't fit its chunk is refused");
	}
}

// How many texture names the last shape below says it has. A copy of a shape reads every name it counts, so it can't count one that was never reached.
static int textureNamesCounted = 0;

// Loads a shape holding a list of texture names, with zeros and then the wall after it, and says whether the level would be accepted.
static bool TextureNamesAccepted(bool subShape, int count, char const * names, size_t namesBytes)
{
	int const triangle[5] = { 0, 1, 2, -1, -1 };

	size_t const shapeBytes = BuildShapeData(wholeShape, subShape, 4, triangle);
	char * data = PutChunk(wholeShape + shapeBytes, "SHPTEXFN", sizeof(int) + namesBytes);
	memcpy(data, &count, sizeof(count));
	memcpy(data + sizeof(int), names, namesBytes);

	size_t const allBytes = shapeBytes + ChunkHeaderBytes + sizeof(int) + namesBytes;

	File_Chunk file;
	Parent_File = &file;

	char * atTheWall = MemoryEndingAtAWall(allBytes + SpareAfterChunk);
	memcpy(atTheWall, wholeShape, allBytes);
	memset(atTheWall + allBytes, 0, SpareAfterChunk);

	if (subShape)
	{
		std::unique_ptr<Shape_Sub_Shape_Chunk> const made(new Shape_Sub_Shape_Chunk(&file, atTheWall, allBytes));
		textureNamesCounted = made->shape_data.num_texfiles;
	}
	else
	{
		std::unique_ptr<Shape_Chunk> const made(new Shape_Chunk(&file, atTheWall, allBytes));
		textureNamesCounted = made->shape_data.num_texfiles;
	}

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

static void TestCountedLists()
{
	puts("36. counted lists");

	alignas(int) static char bytes[4096];

	// where each count is, with any name before it empty, and what each item takes at the least
	// bytesAfter is what the kind holds after its list. A whole chunk has it, and the bad counts are tried without it, so that only the check on the count can refuse them.
	struct CountedRow { MakesChunk make; size_t countAt; size_t itemBytes; char const * good; size_t bytesAfter; };
	CountedRow const rows[] =
	{
		{ MakeChunk<AVP_Path_Chunk>,                          16, sizeof(ChunkPathPoint),       "a path with points is accepted" },
		{ MakeChunk<VModule_Array_Chunk>,                     0,  4 * sizeof(int),              "a module's list of what it can see is accepted" },
		{ MakeChunk<Adjacent_Module_Entry_Points_Chunk>,      0,  20,                           "a module's list of neighbours is accepted" },
		{ MakeChunk<Object_Alternate_Locations_Chunk>,        8,  sizeof(AltObjectLocation),    "an object's other places are accepted" },
		{ MakeChunk<Object_Animation_Sequence_Frame_Chunk>,   40, sizeof(int),                  "an animation frame's extra numbers are accepted" },
		{ MakeChunk<Object_Animation_Sequence_Header_Chunk>,  12, sizeof(int),                  "a sequence header's extra numbers are accepted" },
		{ MakeChunk<Object_Animation_All_Sequence_Chunk>,     0,  4 * sizeof(int),              "a list of sequences is accepted" },
		{ MakeChunk<Object_Hierarchy_Data_Chunk>,             0,  sizeof(int),                  "a hierarchy's extra numbers are accepted" },
		{ MakeChunk<Object_Hierarchy_Alternate_Shape_Set_Chunk>, 8, 8,                          "a set of alternate shapes is accepted", 16 },
		{ MakeChunk<Hierarchy_Shape_Set_Collection_Chunk>,    8,  sizeof(int),                  "a collection of shape sets is accepted", 8 },
		{ MakeChunk<Hierarchy_Degradation_Distance_Chunk>,    0,  sizeof(int),                  "a list of detail levels is accepted" },
		{ MakeChunk<Sprite_Bitmap_Scale_Chunk>,               0,  sizeof(float),                "a sprite's scales are accepted" },
		{ MakeChunk<Sprite_Bitmap_Centre_Chunk>,              0,  4 * sizeof(int),              "a sprite's centres are accepted", 4 },
		{ MakeChunk<Indexed_Sound_Chunk>,                     28, sizeof(int),                  "a sound's extra numbers are accepted" },
		{ MakeChunk<Random_Texture_ID_Chunk>,                 4,  sizeof(int),                  "a list of random textures is accepted", 8 },
		{ MakeChunk<Placed_Object_Light_Chunk>,               sizeof(Placed_Object_Light_Data), sizeof(int), "a light's extra numbers are accepted" },
		{ MakeChunk<Module_Waypoint_Chunk>,                   0,  3 * sizeof(ChunkVectorInt) + 4 * sizeof(int), "a list of waypoints is accepted" },
		{ MakeChunk<Preset_Palette_Chunk>,                    20, 24,                           "a list of palettes is accepted" },
		{ MakeChunk<Preset_Palette_Store_Chunk>,              24, 24,                           "a store of palettes is accepted" },
		{ MakeChunk<RIF_Child_Chunk>,                         24, 12,                           "a child file's list of images is accepted" },
		{ MakeChunk<BMP_Enums_Chunk>,                         12, 12,                           "a list of enums is accepted" },
		{ MakeChunk<Matching_Images_Chunk>,                   12, 52,                           "a list of matching images is accepted" },
		{ MakeChunk<Shape_Poly_Change_Info_Chunk>,            4,  3 * sizeof(int),              "a list of polygon changes is accepted" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		CountedRow const & row = rows[i];
		size_t const size = row.countAt + sizeof(int) + 2 * row.itemBytes;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, row.countAt, 2);
		Check(ChunkAccepted(row.make, bytes, size + row.bytesAfter), row.good);

		TestBadCounts(row.make, bytes, size, row.countAt, row.itemBytes);

		// The count is 0, which is always fine, so it's only the missing room that can refuse this one.
		PutNumber(bytes, row.countAt, 0);
		Check(!ChunkAccepted(row.make, bytes, row.countAt + sizeof(int) - 1), "a chunk with no room for its count is refused");
	}

	// A sprite's frames are held in three lists, one inside the next. Its six numbers come first, with the three counts in the middle of them. The
	// 97612894s are the smallest count that wraps round to nearly nothing when it's multiplied by a frame's 44 bytes, so only the step that measures that
	// count can catch them.
	{
		size_t const numbersBytes = 6 * sizeof(int);
		size_t const frameBytes = sizeof(Frame);
		int const wraps = 97612894;

		struct SpriteRow { int yaw, pitch, frames; size_t framesInChunk; bool good; char const * says; };
		SpriteRow const sprites[] =
		{
			{ 2,     2,     1, 4, true,  "a sprite's frames fit its chunk" },
			{ 0,     0,     0, 0, true,  "a sprite with no frames at all is accepted" },
			{ 2,     2,     1, 3, false, "a sprite one frame short is refused" },
			{ -1,    1,     1, 1, false, "a sprite with a negative count of yaws is refused" },
			{ 1,     -1,    1, 1, false, "a sprite with a negative count of pitches is refused" },
			{ 1,     1,     -1, 1, false, "a sprite with a negative count of frames is refused" },
			{ wraps, 1,     1, 1, false, "a count of yaws that wraps round is refused" },
			{ 1,     wraps, 1, 1, false, "a count of pitches that wraps round is refused" },
			{ 1,     1,     wraps, 1, false, "a count of frames that wraps round is refused" },
			{ 1000,  1,     0, 1, false, "a thousand lists with no frames in them are refused" },
		};

		for (size_t i = 0; i < sizeof(sprites) / sizeof(sprites[0]); i++)
		{
			memset(bytes, 0, sizeof(bytes));
			PutNumber(bytes, 4, sprites[i].yaw);
			PutNumber(bytes, 8, sprites[i].pitch);
			PutNumber(bytes, 12, sprites[i].frames);

			bool const accepted = ChunkAccepted(MakeChunk<Sprite_Action_Chunk>, bytes, numbersBytes + sprites[i].framesInChunk * frameBytes);
			Check(accepted == sprites[i].good, sprites[i].says);
		}

		memset(bytes, 0, sizeof(bytes));
		Check(!ChunkAccepted(MakeChunk<Sprite_Action_Chunk>, bytes, numbersBytes - 1), "a sprite with no room for its numbers is refused");
	}

	// Each sequence starts with its own count of frames, and its frames follow it.
	{
		size_t const sequenceBytes = 4 * sizeof(int);
		size_t const frameBytes = sizeof(Object_Animation_Frame);
		size_t const firstFramesAt = sizeof(int);
		MakesChunk const make = MakeChunk<Object_Animation_All_Sequence_Chunk>;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 1);
		PutNumber(bytes, firstFramesAt, 2);
		Check(ChunkAccepted(make, bytes, sizeof(int) + sequenceBytes + 2 * frameBytes), "a sequence with its frames is accepted");

		TestBadCounts(make, bytes, sizeof(int) + sequenceBytes + 2 * frameBytes, firstFramesAt, frameBytes);

		// Four sequences fit by their count, until the first one's frame uses up the room the second needed.
		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 4);
		PutNumber(bytes, firstFramesAt, 1);
		Check(!ChunkAccepted(make, bytes, sizeof(int) + 4 * sequenceBytes), "sequences that run out of room are refused");
	}

	// Three entries of two names fit by their count, until the first name is long enough to use up the room.
	{
		size_t const firstNameAt = 12;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 8, 3);
		memset(bytes + firstNameAt, 'x', 15);
		Check(!ChunkAccepted(MakeChunk<Object_Hierarchy_Alternate_Shape_Set_Chunk>, bytes, firstNameAt + 3 * 8), "shape sets that run out of room are refused");
	}

	// A waypoint is 44 bytes, then a count of links and the links, then a count of modules and those. The chunk's own count comes before all of them.
	{
		size_t const waypointBytes = 3 * sizeof(ChunkVectorInt) + 4 * sizeof(int);
		size_t const linkBytes = sizeof(WaypointLink);
		size_t const smallestModule = 8;
		size_t const linksAt = sizeof(int) + 44;
		size_t const modulesAt = linksAt + sizeof(int);
		MakesChunk const make = MakeChunk<Module_Waypoint_Chunk>;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 1);
		PutNumber(bytes, linksAt, 2);
		Check(ChunkAccepted(make, bytes, sizeof(int) + waypointBytes + 2 * linkBytes), "a waypoint with links is accepted");
		TestBadCounts(make, bytes, sizeof(int) + waypointBytes + 2 * linkBytes, linksAt, linkBytes);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 1);
		PutNumber(bytes, modulesAt, 2);
		Check(ChunkAccepted(make, bytes, sizeof(int) + waypointBytes + 2 * smallestModule), "a waypoint with modules is accepted");
		TestBadCounts(make, bytes, sizeof(int) + waypointBytes + 2 * smallestModule, modulesAt, smallestModule);

		// Ten waypoints fit by their count, until the first one's links use up the room all the others needed. Each waypoint after that would read its 44
		// bytes further on, so with this many they reach the wall unless the list stops.
		int const manyWaypoints = 10;
		size_t const allWaypoints = sizeof(int) + manyWaypoints * waypointBytes;
		int const links = (int)((allWaypoints - modulesAt - sizeof(int)) / linkBytes);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, manyWaypoints);
		PutNumber(bytes, linksAt, links);
		Check(!ChunkAccepted(make, bytes, allWaypoints), "waypoints that run out of room are refused");

		// Three modules fit by their count, until the first name is long enough to use up the room.
		size_t const firstModuleAt = modulesAt + sizeof(int);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 1);
		PutNumber(bytes, modulesAt, 3);
		memset(bytes + firstModuleAt, 'x', 15);
		Check(!ChunkAccepted(make, bytes, firstModuleAt + 3 * smallestModule), "modules that run out of room are refused");
	}

	// A palette holds a count of colours, then three bytes for each one.
	{
		MakesChunk const make = MakeChunk<Environment_Palette_Chunk>;
		size_t const numbersBytes = 8;
		size_t const colourBytes = 3;
		size_t const size = numbersBytes + 2 * colourBytes;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 2);
		Check(ChunkAccepted(make, bytes, size), "a palette with its colours is accepted");
		TestBadCounts(make, bytes, size, 0, colourBytes);

		PutNumber(bytes, 0, 1);
		Check(!ChunkAccepted(make, bytes, numbersBytes - 1), "a palette with no room for its numbers is refused");
	}

	// A list of palettes, each with its own count of colours in its first number.
	{
		struct PaletteRow { MakesChunk make; size_t countAt; };
		PaletteRow const palettes[] = { { MakeChunk<Preset_Palette_Chunk>, 20 }, { MakeChunk<Preset_Palette_Store_Chunk>, 24 } };
		size_t const smallestPalette = 24;

		for (size_t i = 0; i < sizeof(palettes) / sizeof(palettes[0]); i++)
		{
			MakesChunk const make = palettes[i].make;
			size_t const firstAt = palettes[i].countAt + sizeof(int);
			size_t const oneWithColours = firstAt + 20 + 8;

			memset(bytes, 0, sizeof(bytes));
			PutNumber(bytes, palettes[i].countAt, 1);
			PutNumber(bytes, firstAt, 2);
			Check(ChunkAccepted(make, bytes, oneWithColours), "a palette in a list, with its colours, is accepted");
			TestBadCounts(make, bytes, oneWithColours, firstAt, 3);

			// Two palettes fit by their count, until the first one's colours use up the room the second needed.
			memset(bytes, 0, sizeof(bytes));
			PutNumber(bytes, palettes[i].countAt, 2);
			PutNumber(bytes, firstAt, 9);
			Check(!ChunkAccepted(make, bytes, firstAt + 2 * smallestPalette), "palettes that run out of room are refused");
		}
	}

	// A lighting table is its width times its levels, both from the file, and one that doesn't fit is simply left out.
	{
		size_t const numbersBytes = 32;

		struct TableRow { int width; int levels; size_t tableBytes; bool kept; char const * says; };
		TableRow const tables[] =
		{
			{ 4,       2,       8, true,  "a lighting table that fits is kept" },
			{ 4,       2,       7, false, "a lighting table too big for its chunk is left out" },
			{ -1,      1,       8, false, "a lighting table with a negative width is left out" },
			{ 1,       -1,      8, false, "a lighting table with a negative count of levels is left out" },
			{ -2,      -4,      8, false, "two negatives that multiply to a size that fits are left out" },
			{ 0x10000, 0x10000, 8, false, "a lighting table whose size wraps round to nothing is left out" },
			{ 0x10001, 0xffff,  8, false, "a lighting table whose size wraps round to something small is left out" },
		};

		for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++)
		{
			File_Chunk file;
			Parent_File = &file;

			size_t const size = numbersBytes + tables[i].tableBytes;
			char * atTheWall = MemoryEndingAtAWall(size + SpareAfterChunk);
			memset(atTheWall, 0, size + SpareAfterChunk);
			memcpy(atTheWall, &tables[i].width, sizeof(int));
			memcpy(atTheWall + 4, &tables[i].levels, sizeof(int));

			std::unique_ptr<Environment_TLT_Chunk const> const tableOwner(new Environment_TLT_Chunk(&file, atTheWall, size));
			Environment_TLT_Chunk const * table = tableOwner.get();

			Check((table->table != 0) == tables[i].kept, tables[i].says);
			Parent_File = 0;
		}
	}

	// A chunk's list of images. Its count shares a number with a version, so it's the low half of the first number.
	{
		MakesChunk const make = MakeChunk<Global_BMP_Name_Chunk>;
		size_t const smallestImage = 24;
		size_t const firstNameAt = sizeof(int) + 20;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 2);
		Check(ChunkAccepted(make, bytes, sizeof(int) + 2 * smallestImage), "a list of images is accepted");
		TestBadCounts(make, bytes, sizeof(int) + 2 * smallestImage, 0, smallestImage);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 3);
		memset(bytes + firstNameAt, 'x', 40);
		Check(!ChunkAccepted(make, bytes, sizeof(int) + 3 * smallestImage), "images that run out of room are refused");
	}

	// The other lists of names, each with a first name long enough to use up the room the count was measured against.
	{
		size_t const firstImageAt = 24 + sizeof(int);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 24, 3);
		memset(bytes + firstImageAt + 8, 'x', 15);
		Check(!ChunkAccepted(MakeChunk<RIF_Child_Chunk>, bytes, firstImageAt + 3 * 12), "a child file's images that run out of room are refused");

		size_t const firstEnumAt = 16;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 12, 3);
		memset(bytes + firstEnumAt + 8, 'x', 15);
		Check(!ChunkAccepted(MakeChunk<BMP_Enums_Chunk>, bytes, firstEnumAt + 3 * 12), "enums that run out of room are refused");

		size_t const firstMatchAt = 16;
		size_t const firstFileNameAt = firstMatchAt + 12 + 16;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 12, 2);
		memset(bytes + firstFileNameAt, 'x', 52);
		Check(!ChunkAccepted(MakeChunk<Matching_Images_Chunk>, bytes, firstMatchAt + 2 * 52), "matching images that run out of room are refused");
	}

	// The rows above use up a list's room with a long name. A name can't be read past the end of its chunk any more, so reading the name would refuse those files by itself,
	// and they can't show that a list's own check is there at all. In these rows every name does end inside the chunk, and nothing else is wrong. The last entry just has less
	// room than any entry takes. Where a list has a check of its own, it's the only thing that can refuse these. Without it, the rest of the entry would be read from past the chunk's end.
	{
		// Each entry here is two names. The first takes 12 bytes, which leaves 5 of the 17 for the second. Its two empty names start 4 bytes apart, so they fit in those 5, and
		// the smallest entry takes 8. This list has no check of its own: the numbers read after it are what refuse this.
		size_t const firstNameAt = 12;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 8, 2);
		memset(bytes + firstNameAt, 'x', 7);
		Check(!ChunkAccepted(MakeChunk<Object_Hierarchy_Alternate_Shape_Set_Chunk>, bytes, firstNameAt + 17), "a shape set's last entry with too little room is refused");

		// One waypoint with two modules, each a name and a number. The first takes 12 bytes, which leaves 4 of the 16 for the second: enough for its name, and none for its number.
		// This list has no check of its own either: reading that number is what refuses this.
		size_t const modulesAt = sizeof(int) + 44 + sizeof(int);
		size_t const firstModuleAt = modulesAt + sizeof(int);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 0, 1);
		PutNumber(bytes, modulesAt, 2);
		memset(bytes + firstModuleAt, 'x', 7);
		Check(!ChunkAccepted(MakeChunk<Module_Waypoint_Chunk>, bytes, firstModuleAt + 16), "a waypoint's last module with too little room is refused");

		// Each image is two numbers and a name. The first takes 16 bytes, which leaves 9 of the 25 for the second: its numbers and an empty name, in less than the smallest 12.
		size_t const firstImageAt = 24 + sizeof(int);

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 24, 2);
		memset(bytes + firstImageAt + 8, 'x', 7);
		Check(!ChunkAccepted(MakeChunk<RIF_Child_Chunk>, bytes, firstImageAt + 25), "a child file's last image with too little room is refused");

		// A pair of images takes at least 52 bytes, because each image is padded to a multiple of 4. A name of 2 letters makes the first pair 56, which leaves 51 of the 107 for
		// the second. All its names end in those 51, and only its padding is missing.
		size_t const firstMatchAt = 16;
		size_t const firstFileNameAt = firstMatchAt + 12 + 16;

		memset(bytes, 0, sizeof(bytes));
		PutNumber(bytes, 12, 2);
		memset(bytes + firstFileNameAt, 'x', 2);
		Check(!ChunkAccepted(MakeChunk<Matching_Images_Chunk>, bytes, firstMatchAt + 107), "the last pair of matching images with too little room is refused");

		// A palette is five numbers, then its colours and name padded to a multiple of 4. Four colours make the first one 36 bytes, which leaves 21 of the 57 for the second: its
		// numbers and an empty name, in less than the smallest 24.
		struct PaletteRow { MakesChunk make; size_t countAt; char const * says; };
		PaletteRow const palettes[] =
		{
			{ MakeChunk<Preset_Palette_Chunk>,       20, "a list of palettes' last one with too little room is refused" },
			{ MakeChunk<Preset_Palette_Store_Chunk>, 24, "a store of palettes' last one with too little room is refused" },
		};

		for (size_t i = 0; i < sizeof(palettes) / sizeof(palettes[0]); i++)
		{
			size_t const firstAt = palettes[i].countAt + sizeof(int);

			memset(bytes, 0, sizeof(bytes));
			PutNumber(bytes, palettes[i].countAt, 2);
			PutNumber(bytes, firstAt, 4);
			Check(!ChunkAccepted(palettes[i].make, bytes, firstAt + 57), palettes[i].says);
		}
	}

	// A shape's list of texture names, in a whole shape and in a sub-shape. Each name ends with a zero, with nothing after it, so one byte is the least a name takes.
	{
		for (int subShape = 0; subShape < 2; subShape++)
		{
			char const names[] = "ab\0cd";
			char const longName[] = "abcdef";

			Check(TextureNamesAccepted(subShape != 0, 2, names, sizeof(names)), "a shape's texture names are accepted");
			Check(!TextureNamesAccepted(subShape != 0, -1, names, sizeof(names)), "a negative count of names is refused");
			Check(!TextureNamesAccepted(subShape != 0, 0x7fffffff, names, sizeof(names)), "a count of names too big to make is refused");
			Check(!TextureNamesAccepted(subShape != 0, 0x40000001, names, sizeof(names)), "a count of names that wraps round is refused");
			Check(!TextureNamesAccepted(subShape != 0, 7, names, sizeof(names)), "one more name than there are bytes is refused");
			Check(!TextureNamesAccepted(subShape != 0, 2, longName, sizeof(longName)), "names that run out of room are refused");
			Check(textureNamesCounted == 1, "a shape only counts the names it really has");
		}
	}

	// The names of the objects using a shape come after its header's numbers, each ending with a zero.
	{
		struct NameRow { int count; char const * names; size_t bytes; bool good; char const * says; };
		NameRow const nameRows[] =
		{
			{ 2,          "a\0b",  4, true,  "a shape's list of objects is accepted" },
			{ -1,         "a\0b",  4, false, "a negative count of objects is refused" },
			{ 0x7fffffff, "a\0b",  4, false, "a count of objects too big is refused" },
			{ 5,          "a\0b",  4, false, "one more object than there are bytes is refused" },
			{ 2,          "xy",     3, false, "objects that run out of room are refused" },
		};

		for (size_t i = 0; i < sizeof(nameRows) / sizeof(nameRows[0]); i++)
		{
			char * data = PutChunk(wholeShape, "SHPHEAD1", ShapeHeaderDataBytes + nameRows[i].bytes);
			memcpy(data + ShapeHeaderDataBytes - sizeof(int), &nameRows[i].count, sizeof(int));
			memcpy(data + ShapeHeaderDataBytes, nameRows[i].names, nameRows[i].bytes);

			char const * end = data + ShapeHeaderDataBytes + nameRows[i].bytes;
			Check(ShapeIsAcceptedAtTheWall(end) == nameRows[i].good, nameRows[i].says);
		}

		// The list stops as soon as its room has gone. Two objects are counted here, and the first one's name takes all the room, so there's nothing to keep for the second.
		// Reading a name with no room left refuses the file by itself, so being refused can't show this: only how many names were kept can.
		{
			int const twoObjects = 2;
			char * data = PutChunk(wholeShape, "SHPHEAD1", ShapeHeaderDataBytes + 3);
			memcpy(data + ShapeHeaderDataBytes - sizeof(int), &twoObjects, sizeof(int));
			memcpy(data + ShapeHeaderDataBytes, "xy", 3);

			int kept = 0;
			Check(!ShapeIsAcceptedAtTheWall(data + ShapeHeaderDataBytes + 3, &kept) && kept == 1, "a shape's list of objects stops when its room runs out");
		}

		// A header that ends partway through its count, at the wall, so that reading the count at all would stop the test.
		size_t const partOfTheCount = ShapeHeaderDataBytes - 2;
		char * data = PutChunk(wholeShape, "SHPHEAD1", partOfTheCount);
		Check(!ShapeIsAcceptedAtTheWall(data + partOfTheCount), "a shape's header with no room for its count of objects is refused");
	}

	// When a list stops early, the items it never reached are still freed, so each has to start with nothing to free. Each is made here on top of dirty
	// bytes, so that "nothing" can't be luck.
	{
		alignas(Object_Animation_Sequence) char sequencePlace[sizeof(Object_Animation_Sequence)];
		memset(sequencePlace, 0xee, sizeof(sequencePlace));
		Object_Animation_Sequence const * sequence = new (sequencePlace) Object_Animation_Sequence;
		Check(sequence->frames == 0 && sequence->num_frames == 0, "a sequence starts with no frames");

		alignas(ModuleLink) char linkPlace[sizeof(ModuleLink)];
		memset(linkPlace, 0xee, sizeof(linkPlace));
		ModuleLink const * link = new (linkPlace) ModuleLink;
		Check(link->module_name == 0, "a module link starts with no name");
	}
}

// 37. Numbers from a level file that the game uses as places in a list. A wrong one writes outside the list, or sends the game somewhere it never comes back from.
static bool ImageIndexAccepted(int index)
{
	alignas(int) static char bytes[64];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, 1);
	PutNumber(bytes, sizeof(int) + sizeof(int), index);

	return ChunkAccepted(MakeChunk<Global_BMP_Name_Chunk>, bytes, sizeof(int) + 24);
}

static bool BranchAccepted(int place, int branch)
{
	alignas(int) static char bytes[128];
	size_t const itemBytes = 4 * sizeof(int);
	int const items = 4;

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, items);
	PutNumber(bytes, sizeof(int) + place * itemBytes + sizeof(int), branch);

	return ChunkAccepted(MakeChunk<VModule_Array_Chunk>, bytes, sizeof(int) + items * itemBytes);
}

// Three waypoints, and the first of them has one link. firstGround is which waypoint starts the ground list, or -1 if there's only one list.
static bool LinkAccepted(int target, int firstGround, int fromWaypoint)
{
	alignas(int) static char bytes[512];
	size_t const waypointBytes = 3 * sizeof(ChunkVectorInt) + 4 * sizeof(int);
	size_t const flagsAt = 36;
	size_t const linksAt = 44;
	size_t const linkBytes = sizeof(WaypointLink);
	int const waypoints = 3;

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, waypoints);

	size_t at = sizeof(int);

	for (int i = 0; i < waypoints; i++)
	{
		if (i == firstGround)
		{
			PutNumber(bytes, at + flagsAt, WaypointFlag_FirstGroundWaypoint);
		}

		if (i == fromWaypoint)
		{
			PutNumber(bytes, at + linksAt, 1);
			PutNumber(bytes, at + linksAt + sizeof(int), target);
			at += linkBytes;
		}

		at += waypointBytes;
	}

	return ChunkAccepted(MakeChunk<Module_Waypoint_Chunk>, bytes, at);
}

// A sequence holding one frame for each number here. Each frame's place is its 33rd byte on.
static bool FramePlacesAccepted(int const * places, int frames)
{
	size_t const frameDataBytes = 44;
	size_t const placeAt = 32;
	char * end = wholeShape;

	for (int i = 0; i < frames; i++)
	{
		char * data = PutChunk(end, "OBASEQFR", frameDataBytes);
		memcpy(data + placeAt, &places[i], sizeof(int));
		end = data + frameDataBytes;
	}

	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Object_Animation_Sequence_Chunk> const made(new Object_Animation_Sequence_Chunk(&file, wholeShape, end - wholeShape));
	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// A placed hierarchy holding a strategy of this kind, with every number in it zero.
static bool PlacedStrategyAccepted(int kind)
{
	size_t const strategyBytes = StrategySizeOfKind(kind);
	char * data = PutChunk(wholeShape, "AVPSTRAT", StrategyNumbersBytes + strategyBytes);
	memcpy(data + StrategyNumbersBytes, &kind, sizeof(kind));

	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Placed_Hierarchy_Chunk> const made(new Placed_Hierarchy_Chunk(&file, wholeShape, ChunkHeaderBytes + StrategyNumbersBytes + strategyBytes));
	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// A strategy of this kind, with one number set, and every other number zero.
static bool StrategyNumberAccepted(int kind, size_t at, int number)
{
	alignas(int) static char strategy[1024];
	size_t const size = StrategySizeOfKind(kind);

	memset(strategy, 0, sizeof(strategy));
	memcpy(strategy, &kind, sizeof(kind));
	memcpy(strategy + at, &number, sizeof(number));

	return StrategyAccepted(strategy, size);
}

static bool TrackSoundAccepted(int index)
{
	alignas(int) static char bytes[64];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 20, index);

	return ChunkAccepted(MakeChunk<Object_Track_Sound_Chunk>, bytes, 28);
}

static bool PathNumberAccepted(int number)
{
	alignas(int) static char bytes[64];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 4, number);

	return ChunkAccepted(MakeChunk<AVP_Path_Chunk>, bytes, 20);
}

// Two of a level's paths, with these numbers.
static bool TwoPathsAccepted(int first, int second)
{
	alignas(int) static char bytes[64];

	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Special_Objects_Chunk> const pathsOwner(new Special_Objects_Chunk(&file));
	Special_Objects_Chunk * paths = pathsOwner.get();

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 4, first);
	std::unique_ptr<AVP_Path_Chunk> const made(new AVP_Path_Chunk(paths, bytes, 20));

	PutNumber(bytes, 4, second);
	std::unique_ptr<AVP_Path_Chunk> const made2(new AVP_Path_Chunk(paths, bytes, 20));

	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// A track with room for two sections, of these times, starting at this time.
static bool TrackAccepted(int count, int firstTime, int secondTime, int start)
{
	alignas(int) static char bytes[512];
	size_t const sectionBytes = sizeof(ChunkTrackSection);
	size_t const timeAt = offsetof(ChunkTrackSection, time_for_section);
	size_t const flagsAt = sizeof(int) + 2 * sectionBytes;

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, count);
	PutNumber(bytes, sizeof(int) + timeAt, firstTime);
	PutNumber(bytes, sizeof(int) + sectionBytes + timeAt, secondTime);
	PutNumber(bytes, flagsAt + sizeof(int), start);

	return ChunkAccepted(MakeChunk<Object_Track_Chunk2>, bytes, flagsAt + 2 * sizeof(int));
}

// A placed light with one of its times set.
static bool LightTimeAccepted(size_t at, int time)
{
	alignas(int) static char bytes[128];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, at, time);

	return ChunkAccepted(MakeChunk<Placed_Object_Light_Chunk>, bytes, sizeof(Placed_Object_Light_Data) + sizeof(int));
}

static void TestNumbersUsedAsPlaces()
{
	puts("37. numbers used as places");

	// The polygon's record comes after the header and the points in what BuildShapeData writes, and its flags are its third number.
	{
		int const triangle[5] = { 0, 1, 2, -1, -1 };
		int const pointCount = 4;
		int const onlyTheLoaderSetsThis = 0x40000000;
		size_t const polygonAt = ChunkHeaderBytes + ShapeHeaderDataBytes + ChunkHeaderBytes + 12 * pointCount + ChunkHeaderBytes;
		size_t const flagsAt = polygonAt + 2 * sizeof(int);

		size_t const bytes = BuildShapeData(wholeShape, false, pointCount, triangle);
		Check(ShapeIsAccepted(wholeShape + bytes), "a shape with plain polygons is accepted");

		memcpy(wholeShape + flagsAt, &onlyTheLoaderSetsThis, sizeof(int));
		Check(!ShapeIsAccepted(wholeShape + bytes), "a polygon that says its texture list is a table of pointers is refused");
	}

	Check(PlacedStrategyAccepted(StratHierarchy), "a placed hierarchy with a hierarchy strategy is accepted");
	Check(!PlacedStrategyAccepted(StratDoor), "a placed hierarchy with any other kind of strategy is refused");

	size_t const firstNumberAt = 12;

	Check(StrategyNumberAccepted(StratMessage, firstNumberAt, 450), "the last level message is accepted");
	Check(!StrategyNumberAccepted(StratMessage, firstNumberAt, 451), "a message past the last one is refused");
	Check(!StrategyNumberAccepted(StratMessage, firstNumberAt, -1), "a negative message is refused");
	Check(StrategyNumberAccepted(StratMissionObjective, firstNumberAt, 450), "a mission's description in the last place is accepted");
	Check(!StrategyNumberAccepted(StratMissionObjective, firstNumberAt, 451), "a mission's description past the last place is refused");
	Check(!StrategyNumberAccepted(StratMissionObjective, firstNumberAt, -1), "a mission's negative description is refused");
	Check(StrategyNumberAccepted(StratMissionObjective, firstNumberAt + 4, 450), "a mission's end message in the last place is accepted");
	Check(!StrategyNumberAccepted(StratMissionObjective, firstNumberAt + 4, 451), "a mission's end message past the last place is refused");
	Check(!StrategyNumberAccepted(StratMissionObjective, firstNumberAt + 4, -1), "a mission's negative end message is refused");

	{
		alignas(int) static char generator[28];

		memset(generator, 0, sizeof(generator));
		Check(ChunkAccepted(MakeChunk<Global_Generator_Data_Chunk>, generator, sizeof(generator)), "generators that make aliens are accepted");

		PutNumber(generator, 0, Generate_Marines);
		Check(ChunkAccepted(MakeChunk<Global_Generator_Data_Chunk>, generator, sizeof(generator)), "generators that make marines are accepted");

		PutNumber(generator, 0, Generate_Marines + 1);
		Check(!ChunkAccepted(MakeChunk<Global_Generator_Data_Chunk>, generator, sizeof(generator)), "generators that make anything else are refused");

		PutNumber(generator, 0, -1);
		Check(!ChunkAccepted(MakeChunk<Global_Generator_Data_Chunk>, generator, sizeof(generator)), "generators with a negative kind are refused");
	}

	Check(TrackSoundAccepted(0), "a track's middle sound is accepted");
	Check(TrackSoundAccepted(2), "a track's end sound is accepted");
	Check(!TrackSoundAccepted(3), "a fourth track sound is refused");
	Check(!TrackSoundAccepted(-1), "a track sound in a negative place is refused");

	Check(PathNumberAccepted(0), "path 0 is accepted");
	Check(PathNumberAccepted(255), "the last path a switch can name is accepted");
	Check(!PathNumberAccepted(256), "a path no switch can name is refused");
	Check(!PathNumberAccepted(-1), "a negative path is refused");
	Check(!PathNumberAccepted(0x1fffffff), "a path number whose list wraps round is refused");
	Check(TwoPathsAccepted(1, 2), "two paths with their own numbers are accepted");
	Check(TwoPathsAccepted(0, 255), "the first and last paths together are accepted");
	Check(!TwoPathsAccepted(3, 3), "two paths with one number are refused");
	Check(!TwoPathsAccepted(255, 255), "two paths with the last number are refused");

	// Every section of a track has to take some time, or the game would never use up the track's timer.
	{
		int const quickest = 65;
		int const latest = 1 << 24;
		size_t const sectionBytes = sizeof(ChunkTrackSection);

		Check(TrackAccepted(2, quickest, quickest, 0), "a track of the shortest sections is accepted");
		Check(TrackAccepted(2, 3276, 1310720000, 524288), "a track with the game's own longest and shortest times is accepted");
		Check(TrackAccepted(2, quickest, quickest, latest), "a track with the latest start is accepted");
		Check(TrackAccepted(0, 0, 0, 0), "a track with no sections has no times to be wrong");
		Check(!TrackAccepted(2, quickest - 1, quickest, 0), "a first section too short is refused");
		Check(!TrackAccepted(2, quickest, quickest - 1, 0), "a last section too short is refused");
		Check(!TrackAccepted(2, 0, quickest, 0), "a section of no time is refused");
		Check(!TrackAccepted(2, -1, quickest, 0), "a section of negative time is refused");
		Check(!TrackAccepted(2, quickest, quickest, latest + 1), "a start too late is refused");
		Check(!TrackAccepted(2, quickest, quickest, -1), "a negative start is refused");
		Check(!TrackAccepted(2, quickest, quickest, (int)0x80000000), "the start that can't be turned round is refused");

		int const badCounts[] = { -1, 0x7fffffff, (int)(0x100000000ULL / sectionBytes) + 1, (int)(0x7ffff000UL / sectionBytes), 3, 4000 };

		for (size_t i = 0; i < sizeof(badCounts) / sizeof(badCounts[0]); i++)
		{
			Check(!TrackAccepted(badCounts[i], quickest, quickest, 0), "a count that doesn't fit its chunk is refused");
		}
	}

	// All five of a light's times are multiplied by the game, and any of them could wrap round.
	{
		size_t const lightTimes[] =
		{
			offsetof(Placed_Object_Light_Data, fade_up_time),
			offsetof(Placed_Object_Light_Data, fade_down_time),
			offsetof(Placed_Object_Light_Data, up_time),
			offsetof(Placed_Object_Light_Data, down_time),
			offsetof(Placed_Object_Light_Data, start_time),
		};

		for (size_t i = 0; i < sizeof(lightTimes) / sizeof(lightTimes[0]); i++)
		{
			Check(LightTimeAccepted(lightTimes[i], 0), "a light time of nothing is accepted");
			Check(LightTimeAccepted(lightTimes[i], 32767), "the longest light time is accepted");
			Check(!LightTimeAccepted(lightTimes[i], 32768), "a light time that wraps round is refused");
			Check(!LightTimeAccepted(lightTimes[i], -1), "a negative light time is refused");
		}
	}

	Check(ImageIndexAccepted(0), "the first image's index is accepted");
	Check(ImageIndexAccepted(73), "the largest image index in the game's own levels is accepted");
	Check(ImageIndexAccepted(32767), "the largest image index a polygon can name is accepted");
	Check(!ImageIndexAccepted(32768), "an image index no polygon can name is refused");
	Check(!ImageIndexAccepted(-1), "a negative image index is refused");
	Check(!ImageIndexAccepted(0x7fffffff), "an image index that would ask for all the memory is refused");

	Check(BranchAccepted(0, 2), "a jump forward in a module's list is accepted");
	Check(BranchAccepted(1, 4), "a jump to the end of a module's list is accepted");
	Check(!BranchAccepted(1, 5), "a jump past the end of a module's list is refused");
	Check(!BranchAccepted(2, 2), "a jump to itself is refused");
	Check(!BranchAccepted(2, 1), "a jump back is refused");
	Check(!BranchAccepted(2, -1), "a negative jump is refused");

	Check(LinkAccepted(2, -1, 0), "a link to the last waypoint is accepted");
	Check(!LinkAccepted(3, -1, 0), "a link past the last waypoint is refused");
	Check(!LinkAccepted(-1, -1, 0), "a negative link is refused");
	Check(LinkAccepted(1, 2, 0), "a link to the last waypoint of the first list is accepted");
	Check(!LinkAccepted(2, 2, 0), "a link past the end of its own list is refused");
	Check(LinkAccepted(0, 2, 2), "a link in the ground list to its only waypoint is accepted");
	Check(!LinkAccepted(1, 2, 2), "a link past the end of the ground list is refused");

	int const inOrder[2] = { 0, 1 };
	int const swapped[2] = { 1, 0 };
	int const repeated[2] = { 1, 1 };
	int const pastTheEnd[2] = { 0, 2 };
	int const negative[2] = { -1, 0 };
	// Just past either end could read a byte that happens to refuse it. A program like this one can only read the first 2 GB
	// of addresses, and this far from anywhere in that, in either direction, is outside them. So without the check, these crash.
	int const farPast[2] = { 0, 0x7ffffff0 };
	int const farBelow[2] = { -0x7ffffff0, 0 };

	Check(FramePlacesAccepted(inOrder, 2), "frames in order are accepted");
	Check(FramePlacesAccepted(swapped, 2), "frames out of order are accepted");
	Check(FramePlacesAccepted(inOrder, 0), "a sequence with no frames is accepted");
	Check(!FramePlacesAccepted(repeated, 2), "two frames in one place are refused");
	Check(!FramePlacesAccepted(pastTheEnd, 2), "a frame past the end of its sequence is refused");
	Check(!FramePlacesAccepted(farPast, 2), "a frame far past the end of its sequence is refused");
	Check(!FramePlacesAccepted(negative, 2), "a frame in a negative place is refused");
	Check(!FramePlacesAccepted(farBelow, 2), "a frame far below the start of its sequence is refused");
}

// 33b. A name in a level file ends at a zero byte, and the search for it must stop at the end of its chunk. Here there's no zero byte in the chunk or in anything after
// it, right up to the wall, so a search that doesn't stop runs into the wall and ends the test. flagsAt is where to put a 1, for a chunk that only has a name if a flag says so.
static bool AcceptedWithNoZeroAnywhere(MakesChunk make, size_t size, int flagsAt)
{
	File_Chunk file;
	Parent_File = &file;

	char * atTheWall = MemoryEndingAtAWall(size + SpareAfterChunk);
	memset(atTheWall, 'x', size + SpareAfterChunk);

	if (flagsAt >= 0)
	{
		PutNumber(atTheWall, flagsAt, 1);
	}

	make(&file, atTheWall, size);

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// A chunk with a list, where the name is only reached by way of a count: the numbers before it are given here, and the name after them never ends
static bool AcceptedWithNumbersThenNoEnd(MakesChunk make, int const * numbers, size_t numbersCount, size_t size)
{
	File_Chunk file;
	Parent_File = &file;

	char * atTheWall = MemoryEndingAtAWall(size + SpareAfterChunk);
	memset(atTheWall, 'x', size + SpareAfterChunk);
	memcpy(atTheWall, numbers, numbersCount * sizeof(int));

	make(&file, atTheWall, size);

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// The same, for a chunk that only exists inside other chunks: kinds are the chunks from the outside in, numbers are what the innermost one starts with, and lastBytes is how
// much it holds in all. Everything after the numbers is letters, right up to the wall.
static bool AcceptedInsideOtherChunks(MakesChunk make, char const * const * kinds, size_t kindCount, int const * numbers, size_t numbersCount, size_t lastBytes)
{
	File_Chunk file;
	Parent_File = &file;

	size_t const whole = kindCount * ChunkHeaderBytes + lastBytes;
	char * atTheWall = MemoryEndingAtAWall(whole + SpareAfterChunk);
	memset(atTheWall, 'x', whole + SpareAfterChunk);

	char * at = atTheWall;

	for (size_t i = 0; i < kindCount; i++)
	{
		int const chunkBytes = (int)(whole - i * ChunkHeaderBytes);
		memcpy(at, kinds[i], 8);
		memcpy(at + 8, &chunkBytes, sizeof(chunkBytes));
		at += ChunkHeaderBytes;
	}

	if (numbersCount)
	{
		memcpy(at, numbers, numbersCount * sizeof(int));
	}

	make(&file, atTheWall, whole);

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

static void TestNamesStayInTheirChunks()
{
	puts("33b. names stay in their chunks");

	// the two helpers themselves
	{
		File_Chunk file;
		Parent_File = &file;

		char const bytes[] = "abc\0xyz";
		size_t length = 99;
		std::unique_ptr<char[]> const name(CopyNameFromChunk(bytes, sizeof(bytes), bytes, &length));
		Check(length == 3 && memcmp(name.get(), "abc", 4) == 0 && file.error_code == 0, "a name is copied up to its zero byte");

		std::unique_ptr<char[]> const onTheLastByte(CopyNameFromChunk(bytes, 4, bytes, &length));
		Check(length == 3 && memcmp(onTheLastByte.get(), "abc", 4) == 0 && file.error_code == 0, "a zero byte on the chunk's very last byte counts");

		std::unique_ptr<char[]> const oneShort(CopyNameFromChunk(bytes, 3, bytes, &length));
		Check(oneShort[0] == 0 && length == 0 && file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "one past the chunk doesn't, and the file is refused, with an empty name");

		file.error_code = 0;
		std::unique_ptr<char[]> const nothingLeft(CopyNameFromChunk(bytes, 4, bytes + 4, &length));
		Check(nothingLeft[0] == 0 && file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a name that has to be there, with no room left for it, is refused");

		file.error_code = 0;
		std::unique_ptr<char[]> const leftOut(CopyOptionalNameFromChunk(bytes, 4, bytes + 4, &length));
		Check(!leftOut && length == 0 && file.error_code == 0, "one that can be left out, with no room left for it, is just not there");

		std::unique_ptr<char[]> const empty(CopyOptionalNameFromChunk(bytes + 3, 4, bytes + 3, &length));
		Check(!empty && length == 0 && file.error_code == 0, "and neither is an empty one");

		std::unique_ptr<char[]> const optional(CopyOptionalNameFromChunk(bytes, 4, bytes, &length));
		Check(optional && length == 3 && memcmp(optional.get(), "abc", 4) == 0, "and one that's there is copied");

		std::unique_ptr<char[]> const noEnd(CopyOptionalNameFromChunk(bytes, 3, bytes, &length));
		Check(!noEnd && file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "but letters with no end are still refused");
		Parent_File = 0;
	}

	struct NameRow { MakesChunk make; int flagsAt; char const * what; };
	NameRow const rows[] =
	{
		{ MakeChunk<AVP_Generator_Chunk>,                         -1, "a generator's name with no end is refused" },
		{ MakeChunk<AVP_Generator_Extra_Name_Chunk>,              -1, "a generator's other name" },
		{ MakeChunk<AVP_Power_Cable_Chunk>,                       -1, "a power cable's name" },
		{ MakeChunk<AVP_Particle_Generator_Data_Chunk>,           -1, "a particle generator's name" },
		{ MakeChunk<AVP_Path_Chunk>,                              -1, "a path's name" },
		{ MakeChunk<Virtual_Object_Properties_Chunk>,             -1, "a virtual object's name" },
		{ MakeChunk<Object_Animation_Sequence_Header_Chunk>,      -1, "an animation sequence's name" },
		{ MakeChunk<External_Shape_BMPs_Store_Chunk>,             -1, "an outside shape's list of bitmaps" },
		{ MakeChunk<Bitmap_MD5_Chunk>,                            -1, "a bitmap's check sum names" },
		{ MakeChunk<Fragment_Type_Data_Chunk>,                    -1, "a fragment type's name" },
		{ MakeChunk<Fragment_Type_Shape_Chunk>,                   -1, "a fragment shape's name" },
		{ MakeChunk<Fragment_Type_Sound_Chunk>,                   -1, "a fragment sound's name" },
		{ MakeChunk<Object_Track_Sound_Chunk>,                    -1, "a track sound's name" },
		{ MakeChunk<Environment_TLT_Chunk>,                       28, "a lighting table's outside file" },
		{ MakeChunk<Coloured_Polygons_Lookup_Chunk>,               0, "a colour table's outside file" },
		{ MakeChunk<RIF_Child_Chunk>,                             -1, "a child file's name" },
		{ MakeChunk<Preset_Palette_Store_Chunk>,                  -1, "a palette store's file name" },
		{ MakeChunk<Shape_External_Filename_Chunk>,               -1, "an outside shape's file name" },
		{ MakeChunk<Shape_External_Object_Name_Chunk>,            -1, "an outside shape's object name" },
		{ MakeChunk<Shape_Name_Chunk>,                            -1, "a shape's name" },
		{ MakeChunk<Shape_Fragment_Type_Chunk>,                   -1, "a shape's fragment type" },
		{ MakeChunk<Sound_Object_Chunk>,                          -1, "a sound's name" },
		{ MakeChunk<Sound_Object_Extra_Name_Chunk>,               -1, "a sound's other name" },
		{ MakeChunk<Indexed_Sound_Chunk>,                         -1, "an indexed sound's file name" },
		{ MakeChunk<BMP_Enums_Chunk>,                             -1, "a list of enums' type name" },
		{ MakeChunk<Sound_Directory_Chunk>,                       -1, "a sound folder's name" },
		{ MakeChunk<Random_Texture_ID_Chunk>,                     -1, "a random texture's name" },
		{ MakeChunk<Object_Hierarchy_Data_Chunk>,                 -1, "a hierarchy section's object name" },
		{ MakeChunk<Object_Hierarchy_Name_Chunk>,                 -1, "a hierarchy's name" },
		{ MakeChunk<Object_Hierarchy_Alternate_Shape_Set_Chunk>,  -1, "an alternate shape set's name" },
		{ MakeChunk<Hierarchy_Shape_Set_Collection_Chunk>,        -1, "a shape set collection's name" },
		{ MakeChunk<RIF_Name_Chunk>,                              -1, "a level's name" },
		{ MakeChunk<Dummy_Object_Data_Chunk>,                     -1, "a dummy object's name" },
		{ MakeChunk<Dummy_Object_Text_Chunk>,                     -1, "a dummy object's text" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		Check(!AcceptedWithNoZeroAnywhere(rows[i].make, 120, rows[i].flagsAt), rows[i].what);
	}

	// A list of bitmap names: how many, then four numbers and the name's place in the list, then the name
	{
		int const numbers[] = { 1, 0, 0, 0, 0, 0 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<Global_BMP_Name_Chunk>, numbers, 6, 40), "a name in a list of bitmaps");
	}

	// Enums: two numbers, an empty type name, how many, then each one's two numbers and its name
	{
		int const numbers[] = { 0, 0, 0, 1, 0, 0 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<BMP_Enums_Chunk>, numbers, 6, 40), "a name in a list of enums");
	}

	// Matching images: three numbers, how many, then three numbers and an image's four numbers, then its names
	{
		int const numbers[] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<Matching_Images_Chunk>, numbers, 11, 100), "a name in a list of matching images");
	}

	// A child file: two numbers and flags, an empty name, a version, an empty file name, how many images, then each one's two numbers and its name
	{
		int const numbers[] = { 0, 0, 0, 0, 0, 0, 1, 0, 0 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<RIF_Child_Chunk>, numbers, 9, 52), "a name in a child file's list of images");
	}

	// A waypoint: how many waypoints, three corners, flags and a spare, no links to other waypoints, one link to a module, then that module's name
	{
		int const numbers[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<Module_Waypoint_Chunk>, numbers, 14, 72), "a module's name in a waypoint");
	}

	// An alternate shape set: its number, an empty name, how many shapes, then each one's old name and new name. 0x00636261 is "abc" and its zero byte.
	{
		int const oldName[] = { 0, 0, 1 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<Object_Hierarchy_Alternate_Shape_Set_Chunk>, oldName, 3, 28), "a shape's old name in an alternate shape set");

		int const newName[] = { 0, 0, 1, 0x00636261 };
		Check(!AcceptedWithNumbersThenNoEnd(MakeChunk<Object_Hierarchy_Alternate_Shape_Set_Chunk>, newName, 4, 32), "and its new name");
	}

	// A game mode's header: flags, three spares and a version, its name, then a list of file names that ends at an empty one
	{
		char const * const kinds[] = { "GMODHEAD" };

		Check(!AcceptedInsideOtherChunks(MakeChunk<Environment_Game_Mode_Chunk>, kinds, 1, 0, 0, 60), "a game mode's name");

		int const nameThenList[] = { 0, 0, 0, 0, 0, 0x00636261 };
		Check(!AcceptedInsideOtherChunks(MakeChunk<Environment_Game_Mode_Chunk>, kinds, 1, nameThenList, 6, 60), "a name in a game mode's list of files");

		// "abc", then "de" and its zero byte, which is the last byte of the chunk: the list stops at the chunk's end, with nothing wrong with it.
		// The top byte of the last number, 0x78, is the letter x, and it's the first byte past the chunk. It has to be a letter: a zero there would also stop a list
		// that never looked for the chunk's end, and this row couldn't tell the two apart.
		int const allThere[] = { 0, 0, 0, 0, 0, 0x00636261, 0x78006564 };
		Check(AcceptedInsideOtherChunks(MakeChunk<Environment_Game_Mode_Chunk>, kinds, 1, allThere, 7, 27), "a game mode whose list reaches the end of its chunk is accepted");
	}

	// A shape's header: its numbers, how many objects use it, then their names. The count is the last of the 68 bytes every header has.
	{
		char const * const kinds[] = { "SHPHEAD1" };
		int numbers[ShapeHeaderDataBytes / sizeof(int)] = { 0 };
		numbers[ShapeHeaderDataBytes / sizeof(int) - 1] = 1;

		Check(!AcceptedInsideOtherChunks(MakeChunk<Shape_Chunk>, kinds, 1, numbers, ShapeHeaderDataBytes / sizeof(int), ShapeHeaderDataBytes + 16), "an object's name in a shape's header");
	}

	// A morphing shape's parts, which are only made inside the chunk they belong to. A sequence that isn't valid frees itself, so it's made as a level makes it, inside a shape.
	{
		char const * const sequence[] = { "ANIMSEQU", "ANISEQDT" };
		Check(!AcceptedInsideOtherChunks(MakeChunk<Shape_Chunk>, sequence, 2, 0, 0, 80), "a morph sequence's name");

		char const * const frame[] = { "ANIMSEQU", "ANIMFRAM", "ANIFRADT" };
		Check(!AcceptedInsideOtherChunks(MakeChunk<Shape_Chunk>, frame, 3, 0, 0, 80), "a morph frame's name");
	}
}

// 38. A hierarchy's animations. The game builds these from the one section inside a hierarchy's outermost chunk, with no checks of its own, so a level whose animations it couldn't play has
// to be refused while the file is read. Each row below changes one thing about a real shape of hierarchy, built here from bytes, the same as a file's.
static char hierarchyBytes[8192];

// Each hierarchy is built from a clean start: every byte 0, so nothing an earlier one wrote is still there to be read
static char * StartHierarchy()
{
	memset(hierarchyBytes, 0, sizeof(hierarchyBytes));

	return hierarchyBytes;
}

// Starts a chunk whose size isn't known yet. The chunk's contents go after what this gives back.
static char * OpenChunk(char * where, char const * name)
{
	memset(where, 0, ChunkHeaderBytes);
	memcpy(where, name, 8);

	return where + ChunkHeaderBytes;
}

// Ends a chunk started at chunk, now that its contents have come to end
static char * CloseChunk(char * chunk, char * end)
{
	int const bytes = (int)(end - chunk);

	memcpy(chunk + 8, &bytes, sizeof(bytes));

	return end;
}

// The chunk with a section's name in it: a count of extra numbers, here 0, and then the name. The game reads the name
// as text, up to a 0 byte, so the name's letters are copied and then its 0 is written, on a line of its own.
static char * PutSectionName(char * where)
{
	static char const name[] = "section";
	size_t const letters = sizeof(name) - 1;
	char * const data = PutChunk(where, "OBJHIERD", 4 + letters + 1);

	memcpy(data + 4, name, letters);
	data[4 + letters] = '\0';

	return data + 4 + letters + 1;
}

// A compact list with one sequence. Each frame is 36 bytes, with its start at 28.
static char * PutCompactSequence(char * where, int frames, int const * starts)
{
	size_t const sequenceBytes = 4 * sizeof(int);
	size_t const frameBytes = 36;
	char * at = OpenChunk(where, "OBANALLS");

	memset(at, 0, sizeof(int) + sequenceBytes + frames * frameBytes);
	PutNumber(at, 0, 1);
	PutNumber(at, sizeof(int), frames);
	at += sizeof(int) + sequenceBytes;

	for (int i = 0; i < frames; i++)
	{
		PutNumber(at, 28, starts[i]);
		at += frameBytes;
	}

	return CloseChunk(where, at);
}

// A compact list with no sequences in it at all
static char * PutEmptyCompactList(char * where)
{
	return PutChunk(where, "OBANALLS", sizeof(int)) + sizeof(int);
}

// The long list, with one sequence. Each frame is its own chunk, with its start at 28 and the place it takes in the sequence at 32.
struct LongFrame { int start; int place; };

static char * PutLongSequence(char * where, bool hasHeader, int frames, LongFrame const * list)
{
	char * const sequence = OpenChunk(where, "OBANSEQS");
	char * at = OpenChunk(sequence, "OBANSEQC");

	if (hasHeader)
	{
		at = PutChunk(at, "OBASEQHD", 4 * sizeof(int)) + 4 * sizeof(int);
	}

	for (int i = 0; i < frames; i++)
	{
		size_t const frameBytes = 44;
		char * const data = PutChunk(at, "OBASEQFR", frameBytes);

		PutNumber(data, 28, list[i].start);
		PutNumber(data, 32, list[i].place);
		at = data + frameBytes;
	}

	CloseChunk(sequence, at);

	return CloseChunk(where, at);
}

// A section, holding whatever fill puts in it
typedef char * (*FillsSection)(char * where);

static char * PutSection(char * where, FillsSection fill)
{
	return CloseChunk(where, fill(OpenChunk(where, "OBJCHIER")));
}

// Gives a level one hierarchy, whose outermost chunk holds the bytes up to end, and says whether the level would be accepted
static bool HierarchyAccepted(char const * end)
{
	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Object_Hierarchy_Chunk> const made(new Object_Hierarchy_Chunk(&file, hierarchyBytes, (size_t)(end - hierarchyBytes)));
	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;

	return accepted;
}

// A section that's fine by itself, for putting bad sections inside
static char * PutGoodCompact(char * at)
{
	int const starts[] = { 0, 30000 };

	return PutCompactSequence(PutSectionName(at), 2, starts);
}

static void TestHierarchyAnimations()
{
	puts("38. hierarchy animations");

	// What a row says is kept as a string, apart from the bytes its fill writes, so the words handed to Check never come
	// from the buffer a hierarchy is built in
	struct SectionRow { FillsSection fill; bool good; std::string says; };
	SectionRow const rows[] =
	{
		// What real files hold
		{ PutGoodCompact, true, "a compact sequence whose frames go up is accepted" },
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 }, { 30000, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, true, "a long sequence whose frames go up is accepted" },
		{ [](char * at) { LongFrame const frames[] = { { 30000, 1 }, { 0, 0 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, true, "a long sequence's frames go by their places, not by where they are in the file" },
		{ [](char * at) { int const starts[] = { 65535 }; return PutCompactSequence(PutSectionName(at), 1, starts); }, true, "a compact sequence can start its last frame as late as 65535" },
		{ [](char * at) { LongFrame const frames[] = { { 65535, 0 } }; return PutLongSequence(PutSectionName(at), true, 1, frames); }, true, "and so can a long one" },
		{ [](char * at) { int const starts[] = { 100, 200 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, true, "a sequence doesn't have to start at 0" },

		// One thing wrong, in the compact list. The first frame starting at 65536 is the one that hangs the game: the only frame, and it comes to a length of 0.
		{ [](char * at) { int const starts[] = { 0 }; return PutCompactSequence(at, 1, starts); }, false, "a section with a compact list and no name chunk is refused" },
		{ [](char * at) { return PutEmptyCompactList(PutSectionName(at)); }, false, "a compact list with no sequences is refused" },
		{ [](char * at) { return PutCompactSequence(PutSectionName(at), 0, 0); }, false, "a compact sequence with no frames is refused" },
		{ [](char * at) { int const starts[] = { 65536 }; return PutCompactSequence(PutSectionName(at), 1, starts); }, false, "a compact sequence's only frame starting at 65536 is refused" },
		{ [](char * at) { int const starts[] = { 0, 65536 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false, "a compact sequence's last frame starting at 65536 is refused" },
		{ [](char * at) { int const starts[] = { -1, 100 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false, "a compact sequence starting below 0 is refused" },
		{ [](char * at) { int const starts[] = { 100, 100 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false, "two compact frames starting together are refused" },
		{ [](char * at) { int const starts[] = { 100, 50 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false, "a compact frame starting before the one ahead of it is refused" },

		// The same in the long list, and what only it can get wrong
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 } }; return PutLongSequence(at, true, 1, frames); }, false, "a section with a long list and no name chunk is refused" },
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 } }; return PutLongSequence(PutSectionName(at), false, 1, frames); }, false, "a long sequence with no header is refused" },
		{ [](char * at) { return PutLongSequence(PutSectionName(at), true, 0, 0); }, false, "a long sequence with no frames is refused" },
		{ [](char * at) { LongFrame const frames[] = { { 65536, 0 } }; return PutLongSequence(PutSectionName(at), true, 1, frames); }, false, "a long sequence's only frame starting at 65536 is refused" },
		{ [](char * at) { LongFrame const frames[] = { { -1, 0 }, { 100, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, false, "a long sequence starting below 0 is refused" },
		{ [](char * at) { LongFrame const frames[] = { { 100, 0 }, { 100, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, false, "two long frames starting together are refused" },
		{ [](char * at) { LongFrame const frames[] = { { 100, 0 }, { 50, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, false, "a long frame starting before the one ahead of it is refused" },

		// A frame's place far outside its sequence. The chunk's own check refuses this, and the rule on starts mustn't go ahead and use the place anyway.
		{ [](char * at) { LongFrame const frames[] = { { 0, 0x10000000 } }; return PutLongSequence(PutSectionName(at), true, 1, frames); }, false, "a frame's place far outside its sequence is refused, and goes no further" },

		// A section inside the first section is held to the same, and it's refused for having no sequences, which only the first section can get away with
		{ [](char * at) { return PutSection(PutGoodCompact(at), PutGoodCompact); }, true, "a good section inside a good section is accepted" },
		{ [](char * at) { return PutSection(PutGoodCompact(at), [](char * inside) { return PutSectionName(inside); }); }, false, "a section inside with no sequences is refused" },
		{ [](char * at) { return PutSection(PutGoodCompact(at), [](char * inside) { int const starts[] = { 100, 100 }; return PutCompactSequence(PutSectionName(inside), 2, starts); }); }, false, "a section inside with bad frames is refused" },
		{ [](char * at) { return PutSection(PutGoodCompact(at), [](char * inside) { int const starts[] = { 0 }; return PutCompactSequence(inside, 1, starts); }); }, false, "a section inside with no name chunk is refused" },

		// The game leaves a hierarchy out altogether if its first section has no sequences, so nothing in that one is built, and it can't refuse the level. Two of the game's own are like this.
		{ [](char * at) { return PutSectionName(at); }, true, "a first section with no sequences at all is left alone" },
		{ [](char * at) { return PutSection(PutSectionName(at), [](char * inside) { int const starts[] = { 100, 100 }; return PutCompactSequence(PutSectionName(inside), 2, starts); }); }, true, "and so is whatever's inside it" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		Check(HierarchyAccepted(PutSection(StartHierarchy(), rows[i].fill)) == rows[i].good, rows[i].says);
	}

	// How many sections the outermost chunk holds. The game reads the first with no check that there is one, and only ever builds that one.
	Check(!HierarchyAccepted(PutSectionName(StartHierarchy())), "a hierarchy with no section inside is refused");
	Check(!HierarchyAccepted(PutSection(PutSection(StartHierarchy(), PutGoodCompact), PutGoodCompact)), "a hierarchy with two sections inside is refused");

	// The outermost chunk's own sequences are only names, in 61 of the game's own hierarchies, and the game never builds them
	{
		char * const after = PutSection(StartHierarchy(), PutGoodCompact);
		Check(HierarchyAccepted(PutCompactSequence(after, 0, 0)), "an outermost chunk's sequence with no frames is left alone");
	}
	{
		char * const after = PutSection(StartHierarchy(), PutGoodCompact);
		Check(HierarchyAccepted(PutLongSequence(after, false, 0, 0)), "and so is one with no header");
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
	TestNamesStayInTheirChunks();
	TestModuleBoxes();
	TestStrategies();
	TestChunksTheGameDoesNotRead();
	TestCountedLists();
	TestNumbersUsedAsPlaces();
	TestHierarchyAnimations();

	if (checksFailed)
	{
		printf("Rebecca's test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("Rebecca's test passed");
	return 0;
}
