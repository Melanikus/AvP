// Rebecca's test: the first unit test in this repository.
// Written 2026-11-03 by Melanikus for the AvP port.
// Modified 2026-11-06 by Melanikus for the AvP port:
//   a stand-in for what the memory code now tells the game's sounds
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
#include <limits>

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

// The memory code tells the game's sounds about memory that's about to go. There are no sounds in this test.
void Sound_ForgetAddressesInside(const void *, size_t)
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

// The check for the hierarchy rows (38), with a name of its own, so a reader of the code can't take it for another test's
// Check. Its words come from a list of literals, never from the bytes a row builds.
static void CheckRow(bool passed, char const * what)
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

// A placed hierarchy's data chunk that's fine: a name of 3 letters in 4 bytes, then 40 bytes of numbers, all zero, and a count of 0
// extra numbers. Gives back where the next chunk goes.
static char * PutGoodPlacedData(char * where)
{
	size_t const dataBytes = 4 + (4 + 12 + 16 + 8) + 4;
	char * const data = PutChunk(where, "PLACHIDT", dataBytes);

	memcpy(data, "abc", 3);

	return data + dataBytes;
}

// A placed hierarchy holding the chunks from start up to end, and whether the level would be accepted
static bool PlacedHierarchyAccepted(char const * start, char const * end)
{
	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Placed_Hierarchy_Chunk> const made(new Placed_Hierarchy_Chunk(&file, start, (size_t)(end - start)));
	file.post_input_processing();

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

	// A placed hierarchy has to hold its data chunk: the game reads it as soon as it sets the level up, for which model it is, where it
	// stands and its name. One without it refuses the level, whatever else it holds. (Added 2026-11-14 by Melanikus for the AvP port.)
	alignas(int) static char placed[256];

	Check(PlacedHierarchyAccepted(placed, PutGoodPlacedData(placed)), "a placed hierarchy with its data chunk is accepted");
	Check(!PlacedHierarchyAccepted(placed, placed), "a placed hierarchy with nothing in it is refused");
	Check(!PlacedHierarchyAccepted(placed, PutChunk(placed, "NOTADATA", 4) + 4), "and so is one holding only a chunk of another kind, with no data chunk");
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

// Where a fan's times to speed up and slow down are in its strategy: after the kind, the type and the extra number, as FanStrategy reads them
static size_t const FanSpeedUpAt = 12;
static size_t const FanSlowDownAt = 16;

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

		// A fan needs some time to speed up and slow down (FanTimesAccepted), so it gets the game's own second for both, and only its size is tried here
		if (kind == StratFan)
		{
			int const second = 65536;
			memcpy(strategy + FanSpeedUpAt, &second, sizeof(second));
			memcpy(strategy + FanSlowDownAt, &second, sizeof(second));
		}

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
	// goodAt and goodNumber: a number the kind refuses at 0, such as a light's range, which is put in first so only the count can refuse the chunk (goodNumber 0: none).
	struct CountedRow { MakesChunk make; size_t countAt; size_t itemBytes; char const * good; size_t bytesAfter; size_t goodAt; int goodNumber; };
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
		{ MakeChunk<Random_Texture_ID_Chunk>,                 4,  sizeof(int),                  "a list of random textures is accepted", 8, 0, 0x00636261 },   // named "abc", as a set needs a name
		{ MakeChunk<Placed_Object_Light_Chunk>,               sizeof(Placed_Object_Light_Data), sizeof(int), "a light's extra numbers are accepted", 0,
			offsetof(Placed_Object_Light_Data, range), 10000 },
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
		if (row.goodNumber) PutNumber(bytes, row.goodAt, row.goodNumber);
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

// A placed hierarchy holding a strategy of this kind, with every number in it zero. It holds its data chunk first, as it has to, so the
// strategy is all that's being tried.
static bool PlacedStrategyAccepted(int kind)
{
	size_t const strategyBytes = StrategySizeOfKind(kind);
	char * data = PutChunk(PutGoodPlacedData(wholeShape), "AVPSTRAT", StrategyNumbersBytes + strategyBytes);
	memcpy(data + StrategyNumbersBytes, &kind, sizeof(kind));

	return PlacedHierarchyAccepted(wholeShape, data + StrategyNumbersBytes + strategyBytes);
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

// A fan with these times to speed up and slow down, in 65536ths of a second, and every other number zero. (Added 2026-11-14.)
static bool FanTimesAccepted(int speedUp, int slowDown)
{
	alignas(int) static char strategy[64];
	int const kind = StratFan;

	memset(strategy, 0, sizeof(strategy));
	memcpy(strategy, &kind, sizeof(kind));
	memcpy(strategy + FanSpeedUpAt, &speedUp, sizeof(speedUp));
	memcpy(strategy + FanSlowDownAt, &slowDown, sizeof(slowDown));

	return StrategyAccepted(strategy, StrategySizeOfKind(StratFan));
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

// A placed light with one of its times set. Its range is 10 metres, as a light has to reach somewhere to be accepted at all (see LightRangeAccepted).
static bool LightTimeAccepted(size_t at, int time)
{
	alignas(int) static char bytes[128];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, offsetof(Placed_Object_Light_Data, range), 10000);
	PutNumber(bytes, at, time);

	return ChunkAccepted(MakeChunk<Placed_Object_Light_Chunk>, bytes, sizeof(Placed_Object_Light_Data) + sizeof(int));
}

// A light with this range, and every other number 0: a placed light (PLOBJLIT), or one of a level's lights (STDLIGHT). Each reads its
// numbers in the order its class holds them, so a number's place in the chunk is its place in the class. (Added 2026-11-14.)
static bool LightRangeAccepted(bool placed, int range)
{
	alignas(int) static char bytes[128];

	memset(bytes, 0, sizeof(bytes));

	if (placed)
	{
		PutNumber(bytes, offsetof(Placed_Object_Light_Data, range), range);
		return ChunkAccepted(MakeChunk<Placed_Object_Light_Chunk>, bytes, sizeof(Placed_Object_Light_Data) + sizeof(int));
	}

	PutNumber(bytes, offsetof(Light_Data, range), range);
	return ChunkAccepted(MakeChunk<Light_Chunk>, bytes, sizeof(Light_Data));
}

// A level's scale, the one number its ENVSDSCL chunk holds. (Added 2026-11-14.)
static bool ScaleAccepted(double scale)
{
	alignas(double) static char bytes[16];

	memset(bytes, 0, sizeof(bytes));
	memcpy(bytes, &scale, sizeof(scale));

	return ChunkAccepted(MakeChunk<Environment_Scale_Chunk>, bytes, sizeof(scale));
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

	// A fan's times to speed up and slow down: the game divides one second by each, so each has to be at least a thousandth of a second, 65, or the
	// speed the game works out from it isn't a sensible one. (Added 2026-11-14 by Melanikus for the AvP port.) The game's own fans take a second, 65536.
	{
		int const quickest = 65;
		int const second = 65536;

		Check(FanTimesAccepted(second, second), "a fan that takes a second to speed up and to slow down, as the game's own do, is accepted");
		Check(FanTimesAccepted(quickest, quickest), "a fan with the shortest times, a thousandth of a second, is accepted");
		Check(FanTimesAccepted(0x7fffffff, 0x7fffffff), "a fan with the longest times a number holds is accepted");
		Check(!FanTimesAccepted(quickest - 1, second), "a fan that speeds up in less than a thousandth of a second is refused");
		Check(!FanTimesAccepted(second, quickest - 1), "a fan that slows down in less than a thousandth of a second is refused");
		Check(!FanTimesAccepted(0, second), "a fan that takes no time to speed up is refused");
		Check(!FanTimesAccepted(second, 0), "a fan that takes no time to slow down is refused");
		Check(!FanTimesAccepted(2, second), "a fan time of 2 is refused");
		Check(!FanTimesAccepted(-1, second), "a negative time to speed up is refused");
		Check(!FanTimesAccepted(second, -1), "a negative time to slow down is refused");
		Check(!FanTimesAccepted((int)0x80000000, (int)0x80000000), "the most negative times are refused");
	}

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

	// A light has to reach somewhere: the game lights only what's nearer than its range, and divides its brightness by it. (Added 2026-11-14 by
	// Melanikus for the AvP port.) The shortest ranges in the game's own levels are 100 for a placed light and 900 for one of a level's lights.
	// And it can reach at most 2^28 millimetres, so the sums the game makes with it stay whole numbers it can hold; the game's own longest is
	// 1,024,900 (Breakout's).
	for (int kind = 0; kind < 2; kind++)
	{
		bool const placed = (kind == 0);
		std::string const which = placed ? "a placed light" : "one of a level's lights";

		Check(LightRangeAccepted(placed, 1), (which + " with the shortest range there is, a millimetre, is accepted").c_str());
		Check(LightRangeAccepted(placed, placed ? 100 : 900), (which + " with the game's own shortest range is accepted").c_str());
		Check(LightRangeAccepted(placed, 1024900), (which + " with the game's own longest range is accepted").c_str());
		Check(LightRangeAccepted(placed, 1 << 28), (which + " with the longest range there is, 2^28 millimetres, is accepted").c_str());
		Check(!LightRangeAccepted(placed, (1 << 28) + 1), (which + " with a range one past the longest is refused").c_str());
		Check(!LightRangeAccepted(placed, 0x7fffffff), (which + " with the longest range a number holds is refused").c_str());
		Check(!LightRangeAccepted(placed, 0), (which + " with a range of nothing is refused: it would reach nowhere").c_str());
		Check(!LightRangeAccepted(placed, -1), (which + " with a negative range is refused").c_str());
		Check(!LightRangeAccepted(placed, (int)0x80000000), (which + " with the most negative range is refused").c_str());
	}

	// A level's scale multiplies every place and distance in it, and each answer is turned back into a whole number, so it has to keep every
	// whole number a whole number: from 0.5 to 1. The game's own levels use 0.8 and 0.9, or none. (Added 2026-11-14 by Melanikus for the AvP port.)
	{
		double const notANumber = std::numeric_limits<double>::quiet_NaN();
		double const endless = std::numeric_limits<double>::infinity();

		Check(ScaleAccepted(1.0), "a level at its own size is accepted");
		Check(ScaleAccepted(0.8) && ScaleAccepted(0.9), "the scales the game's own levels use are accepted");
		Check(ScaleAccepted(0.5), "a level at half its size, the smallest scale, is accepted");
		Check(!ScaleAccepted(0.4999), "a scale under a half is refused");
		Check(!ScaleAccepted(1.0001), "a scale over 1 is refused");
		Check(!ScaleAccepted(2.0), "a level at twice its size is refused");
		Check(!ScaleAccepted(0.0), "a scale of nothing is refused");
		Check(!ScaleAccepted(-1.0), "a negative scale is refused");
		Check(!ScaleAccepted(notANumber), "a scale that isn't a number is refused");
		Check(!ScaleAccepted(endless), "an endless scale is refused");
		Check(!ScaleAccepted(-endless), "an endlessly negative scale is refused");
		Check(!ScaleAccepted(1e300), "a huge scale is refused");
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
// Modified 2026-11-14 by Melanikus for the AvP port: a chunk whose other numbers are checked too gets ones it allows, so that it's refused for its name and nothing else.
// placeAt is where its places start, "places" of them one after another, each put at x, y and z all -1, which is inside the level (CheckPlace) and has no zero byte; and
// kindAt is where its kind goes, put at 0; and turnAt is where its turn goes, each of its four parts the bytes 0x3F808080, about 1.004, which fits (CheckTurn) and has no
// zero byte. Each comes before the name, where the search for the name's end never looks.
static bool AcceptedWithNoZeroAnywhere(MakesChunk make, size_t size, int flagsAt, int placeAt = -1, int places = 1, int kindAt = -1, int turnAt = -1)
{
	File_Chunk file;
	Parent_File = &file;

	char * atTheWall = MemoryEndingAtAWall(size + SpareAfterChunk);
	memset(atTheWall, 'x', size + SpareAfterChunk);

	if (flagsAt >= 0)
	{
		PutNumber(atTheWall, flagsAt, 1);
	}

	for (int i = 0; placeAt >= 0 && i < places; i++)
	{
		ChunkVectorInt const insideTheLevel = { -1, -1, -1 };
		memcpy(atTheWall + placeAt + i * sizeof(insideTheLevel), &insideTheLevel, sizeof(insideTheLevel));
	}

	if (kindAt >= 0)
	{
		PutNumber(atTheWall, kindAt, 0);
	}

	for (int part = 0; turnAt >= 0 && part < 4; part++)
	{
		PutNumber(atTheWall, (size_t)turnAt + part * sizeof(int), 0x3F808080);
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

	// placeAt, places and kindAt: see AcceptedWithNoZeroAnywhere (added 2026-11-14)
	struct NameRow { MakesChunk make; int flagsAt; char const * what; int placeAt = -1; int places = 1; int kindAt = -1; int turnAt = -1; };
	NameRow const rows[] =
	{
		{ MakeChunk<AVP_Generator_Chunk>,                         -1, "a generator's name with no end is refused", 0 },
		{ MakeChunk<AVP_Generator_Extra_Name_Chunk>,              -1, "a generator's other name" },
		{ MakeChunk<AVP_Power_Cable_Chunk>,                       -1, "a power cable's name", 0 },
		{ MakeChunk<AVP_Particle_Generator_Data_Chunk>,           -1, "a particle generator's name", 40, 1, 0, 52 },
		{ MakeChunk<AVP_Path_Chunk>,                              -1, "a path's name" },
		{ MakeChunk<Virtual_Object_Properties_Chunk>,             -1, "a virtual object's name", 0 },
		{ MakeChunk<Object_Animation_Sequence_Header_Chunk>,      -1, "an animation sequence's name" },
		{ MakeChunk<External_Shape_BMPs_Store_Chunk>,             -1, "an outside shape's list of bitmaps" },
		{ MakeChunk<Bitmap_MD5_Chunk>,                            -1, "a bitmap's check sum names" },
		{ MakeChunk<Fragment_Type_Data_Chunk>,                    -1, "a fragment type's name" },
		{ MakeChunk<Fragment_Type_Shape_Chunk>,                   -1, "a fragment shape's name", 8 },
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
		{ MakeChunk<Sound_Object_Chunk>,                          -1, "a sound's name", 0 },
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
		{ MakeChunk<Dummy_Object_Data_Chunk>,                     -1, "a dummy object's name", 0, 3, -1, 36 },
		{ MakeChunk<Dummy_Object_Text_Chunk>,                     -1, "a dummy object's text" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		Check(!AcceptedWithNoZeroAnywhere(rows[i].make, 120, rows[i].flagsAt, rows[i].placeAt, rows[i].places, rows[i].kindAt, rows[i].turnAt), rows[i].what);
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

	// What each row should give. Its words are in a list of their own below, apart from the fills that write the bytes,
	// so nothing a fill wrote can be taken for them.
	struct SectionRow { FillsSection fill; bool good; };
	SectionRow const rows[] =
	{
		// What real files hold
		{ PutGoodCompact, true },
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 }, { 30000, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, true },
		{ [](char * at) { LongFrame const frames[] = { { 30000, 1 }, { 0, 0 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, true },
		{ [](char * at) { int const starts[] = { 65535 }; return PutCompactSequence(PutSectionName(at), 1, starts); }, true },
		{ [](char * at) { LongFrame const frames[] = { { 65535, 0 } }; return PutLongSequence(PutSectionName(at), true, 1, frames); }, true },
		{ [](char * at) { int const starts[] = { 100, 200 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, true },

		// One thing wrong, in the compact list. The first frame starting at 65536 is the one that hangs the game: the only frame, and it comes to a length of 0.
		{ [](char * at) { int const starts[] = { 0 }; return PutCompactSequence(at, 1, starts); }, false },
		{ [](char * at) { return PutEmptyCompactList(PutSectionName(at)); }, false },
		{ [](char * at) { return PutCompactSequence(PutSectionName(at), 0, 0); }, false },
		{ [](char * at) { int const starts[] = { 65536 }; return PutCompactSequence(PutSectionName(at), 1, starts); }, false },
		{ [](char * at) { int const starts[] = { 0, 65536 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false },
		{ [](char * at) { int const starts[] = { -1, 100 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false },
		{ [](char * at) { int const starts[] = { 100, 100 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false },
		{ [](char * at) { int const starts[] = { 100, 50 }; return PutCompactSequence(PutSectionName(at), 2, starts); }, false },

		// The same in the long list, and what only it can get wrong
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 } }; return PutLongSequence(at, true, 1, frames); }, false },
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 } }; return PutLongSequence(PutSectionName(at), false, 1, frames); }, false },
		{ [](char * at) { return PutLongSequence(PutSectionName(at), true, 0, 0); }, false },
		{ [](char * at) { LongFrame const frames[] = { { 65536, 0 } }; return PutLongSequence(PutSectionName(at), true, 1, frames); }, false },
		{ [](char * at) { LongFrame const frames[] = { { -1, 0 }, { 100, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, false },
		{ [](char * at) { LongFrame const frames[] = { { 100, 0 }, { 100, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, false },
		{ [](char * at) { LongFrame const frames[] = { { 100, 0 }, { 50, 1 } }; return PutLongSequence(PutSectionName(at), true, 2, frames); }, false },

		// A frame's place far outside its sequence. The chunk's own check refuses this, and the rule on starts mustn't go ahead and use the place anyway.
		{ [](char * at) { LongFrame const frames[] = { { 0, 0x10000000 } }; return PutLongSequence(PutSectionName(at), true, 1, frames); }, false },

		// A section inside the first section is held to the same, and it's refused for having no sequences, which only the first section can get away with
		{ [](char * at) { return PutSection(PutGoodCompact(at), PutGoodCompact); }, true },
		{ [](char * at) { return PutSection(PutGoodCompact(at), [](char * inside) { return PutSectionName(inside); }); }, false },
		{ [](char * at) { return PutSection(PutGoodCompact(at), [](char * inside) { int const starts[] = { 100, 100 }; return PutCompactSequence(PutSectionName(inside), 2, starts); }); }, false },
		{ [](char * at) { return PutSection(PutGoodCompact(at), [](char * inside) { int const starts[] = { 0 }; return PutCompactSequence(inside, 1, starts); }); }, false },

		// The game leaves a hierarchy out altogether if its first section has no sequences, so nothing in that one is built, and it can't refuse the level. Two of the game's own are like this.
		{ [](char * at) { return PutSectionName(at); }, true },
		{ [](char * at) { return PutSection(PutSectionName(at), [](char * inside) { int const starts[] = { 100, 100 }; return PutCompactSequence(PutSectionName(inside), 2, starts); }); }, true },

		// The game reads the times in the first section's long list even when it has a compact list too, so each sequence there needs its header (added 2026-11-14 by Melanikus)
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 } }; return PutLongSequence(PutGoodCompact(at), true, 1, frames); }, true },
		{ [](char * at) { LongFrame const frames[] = { { 0, 0 } }; return PutLongSequence(PutGoodCompact(at), false, 1, frames); }, false },
	};

	// What each row above checks, in the same order
	char const * const rowWords[] =
	{
		"a compact sequence whose frames go up is accepted",
		"a long sequence whose frames go up is accepted",
		"a long sequence's frames go by their places, not by where they are in the file",
		"a compact sequence can start its last frame as late as 65535",
		"and so can a long one",
		"a sequence doesn't have to start at 0",
		"a section with a compact list and no name chunk is refused",
		"a compact list with no sequences is refused",
		"a compact sequence with no frames is refused",
		"a compact sequence's only frame starting at 65536 is refused",
		"a compact sequence's last frame starting at 65536 is refused",
		"a compact sequence starting below 0 is refused",
		"two compact frames starting together are refused",
		"a compact frame starting before the one ahead of it is refused",
		"a section with a long list and no name chunk is refused",
		"a long sequence with no header is refused",
		"a long sequence with no frames is refused",
		"a long sequence's only frame starting at 65536 is refused",
		"a long sequence starting below 0 is refused",
		"two long frames starting together are refused",
		"a long frame starting before the one ahead of it is refused",
		"a frame's place far outside its sequence is refused, and goes no further",
		"a good section inside a good section is accepted",
		"a section inside with no sequences is refused",
		"a section inside with bad frames is refused",
		"a section inside with no name chunk is refused",
		"a first section with no sequences at all is left alone",
		"and so is whatever's inside it",
		"a first section with a compact list and a long one, whose sequence has its header, is accepted",
		"but a sequence with no header in that long list is refused, though the compact list is the one built from",
	};

	static_assert(sizeof(rowWords) / sizeof(rowWords[0]) == sizeof(rows) / sizeof(rows[0]), "every row has its words");

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		CheckRow(HierarchyAccepted(PutSection(StartHierarchy(), rows[i].fill)) == rows[i].good, rowWords[i]);
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

// 39. Places. Every place a level gives, a point, a box's corner, a centre, where something stands or how far a frame moves it, has to be
// within 2^24 millimetres of the level's middle along each of x, y and z, so that the sums the game makes with places stay whole numbers it
// can hold (CheckPlace, in chunk.hpp). Each kind of chunk that gives a place is made here with that one place set, and everything else as
// the game's own levels could have it. This test keeps its own copy of the limit, so a change to the game's limit shows here.
// (Added 2026-11-14 by Melanikus for the AvP port.)
static int const TestFarthestPlace = 1 << 24;

static ChunkVectorInt PlaceAt(int x, int y, int z)
{
	ChunkVectorInt const place = { x, y, z };
	return place;
}

static void PutPlace(char * where, ChunkVectorInt const & place)
{
	memcpy(where, &place, sizeof(place));
}

// A chunk of this kind and size, all zeros but for the place at "at", and a count of 1 at "countAt" when the chunk holds a list.
static bool PlaceInChunkAccepted(MakesChunk make, size_t bytes, size_t at, ChunkVectorInt const & place, int countAt = -1)
{
	alignas(int) static char data[256];

	memset(data, 0, sizeof(data));
	PutPlace(data + at, place);

	if (countAt >= 0)
	{
		PutNumber(data, (size_t)countAt, 1);
	}

	return ChunkAccepted(make, data, bytes);
}

// A strategy of this kind, at its own size, all zeros but for its kind and the place at "at", counted from its kind.
static bool PlaceInStrategyAccepted(int kind, size_t at, ChunkVectorInt const & place)
{
	alignas(int) static char strategy[256];

	memset(strategy, 0, sizeof(strategy));
	memcpy(strategy, &kind, sizeof(kind));
	PutPlace(strategy + at, place);

	return StrategyAccepted(strategy, StrategySizeOfKind(kind));
}

// Loads the sub-shape in wholeShape, this many bytes long, and says whether the level would be accepted.
static bool SubShapeIsAccepted(size_t bytes)
{
	File_Chunk file;
	Parent_File = &file;

	std::unique_ptr<Shape_Sub_Shape_Chunk> const made(new Shape_Sub_Shape_Chunk(&file, wholeShape, bytes));

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;
	return accepted;
}

static int const PlaceTriangle[5] = { 0, 1, 2, -1, -1 };

// Where a shape's first point is: after the shape's header chunk and the points chunk's own header. A sub-shape's header holds 44 bytes.
static bool ShapePointAccepted(ChunkVectorInt const & place)
{
	size_t const bytes = BuildShapeData(wholeShape, false, 4, PlaceTriangle);
	PutPlace(wholeShape + ChunkHeaderBytes + ShapeHeaderDataBytes + ChunkHeaderBytes, place);
	return ShapeIsAccepted(wholeShape + bytes);
}

static bool SubShapePointAccepted(ChunkVectorInt const & place)
{
	size_t const bytes = BuildShapeData(wholeShape, true, 4, PlaceTriangle);
	PutPlace(wholeShape + ChunkHeaderBytes + 44 + ChunkHeaderBytes, place);
	return SubShapeIsAccepted(bytes);
}

// An animation frame's first point comes after the sequence's header, its data chunk (a header and 24 bytes), the frame's header, the
// frame's data chunk (a header and 24 bytes), and the frame's points chunk's own header (see PutAnimation).
static bool FramePointAccepted(ChunkVectorInt const & place)
{
	size_t const bytes = BuildShapeData(wholeShape, false, 4, PlaceTriangle);
	char * const end = PutAnimation(wholeShape + bytes, 0, true, 0, 4, 1, 0);
	size_t const firstPointAt = ChunkHeaderBytes + (ChunkHeaderBytes + 24) + ChunkHeaderBytes + (ChunkHeaderBytes + 24) + ChunkHeaderBytes;

	PutPlace(wholeShape + bytes + firstPointAt, place);
	return ShapeIsAccepted(end);
}

// A shape's header keeps its box as max x, min x, max y, min y, max z, min z, from byte 36; a sub-shape's from byte 20.
static bool ShapeBoxAccepted(bool subShape, bool top, ChunkVectorInt const & place)
{
	size_t const bytes = BuildShapeData(wholeShape, subShape, 4, PlaceTriangle);
	char * const box = wholeShape + ChunkHeaderBytes + (subShape ? 20 : 36) + (top ? 0 : 4);

	memcpy(box, &place.x, sizeof(int));
	memcpy(box + 8, &place.y, sizeof(int));
	memcpy(box + 16, &place.z, sizeof(int));

	return subShape ? SubShapeIsAccepted(bytes) : ShapeIsAccepted(wholeShape + bytes);
}

static bool ShapeBoxTopAccepted(ChunkVectorInt const & place) { return ShapeBoxAccepted(false, true, place); }
static bool ShapeBoxBottomAccepted(ChunkVectorInt const & place) { return ShapeBoxAccepted(false, false, place); }
static bool SubShapeBoxTopAccepted(ChunkVectorInt const & place) { return ShapeBoxAccepted(true, true, place); }
static bool SubShapeBoxBottomAccepted(ChunkVectorInt const & place) { return ShapeBoxAccepted(true, false, place); }

// A shape's centre chunk holds the centre and then the radius about it.
static bool ShapeCentreAccepted(ChunkVectorInt const & place)
{
	size_t const bytes = BuildShapeData(wholeShape, false, 4, PlaceTriangle);
	char * const centre = PutChunk(wholeShape + bytes, "SHPCENTR", 16);

	PutPlace(centre, place);
	return ShapeIsAccepted(centre + 16);
}

static bool AnimatedShapeCentreAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Anim_Shape_Centre_Chunk>, 20, 0, place); }
static bool PieceAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Shape_Fragment_Location_Chunk>, 28, 0, place); }

// An object's header keeps its place after its flags and the 16 bytes of its lock's user.
static bool ObjectPlaceAccepted(ChunkVectorInt const & place)
{
	TestFile file;
	size_t const bytes = BuildObjectHeader("object", 7);

	PutPlace(objectBytes + ChunkHeaderBytes + 20, place);
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, bytes);

	return file.error_code == 0;
}

// A track with one section that takes a second, with one of its places set.
static bool TrackPlaceAccepted(size_t placeAt, ChunkVectorInt const & place)
{
	alignas(int) static char bytes[256];
	size_t const sectionAt = sizeof(int);

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, 1);
	PutNumber(bytes, sectionAt + offsetof(ChunkTrackSection, time_for_section), 65536);
	PutPlace(bytes + sectionAt + placeAt, place);

	return ChunkAccepted(MakeChunk<Object_Track_Chunk2>, bytes, sectionAt + sizeof(ChunkTrackSection) + 2 * sizeof(int));
}

static bool TrackStartAccepted(ChunkVectorInt const & place) { return TrackPlaceAccepted(offsetof(ChunkTrackSection, pivot_start), place); }
static bool TrackEndAccepted(ChunkVectorInt const & place) { return TrackPlaceAccepted(offsetof(ChunkTrackSection, pivot_end), place); }
static bool TrackOffsetAccepted(ChunkVectorInt const & place) { return TrackPlaceAccepted(offsetof(ChunkTrackSection, object_offset), place); }

// An object's other places: a group, a spare number, a count, and then the places.
static bool OtherPlaceAccepted(ChunkVectorInt const & place)
{
	return PlaceInChunkAccepted(MakeChunk<Object_Alternate_Locations_Chunk>, 12 + sizeof(AltObjectLocation), 12 + offsetof(AltObjectLocation, position), place, 8);
}

// A module's ways in: a count, and then for each an object's number, its flags and the place.
static bool WayInAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Adjacent_Module_Entry_Points_Chunk>, 24, 12, place, 0); }

// One of a level's lights, with a range of 10 metres, as a light has to reach somewhere to be accepted at all (see LightRangeAccepted).
static bool LevelLightAccepted(ChunkVectorInt const & place)
{
	alignas(int) static char bytes[128];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, offsetof(Light_Data, range), 10000);
	PutPlace(bytes + offsetof(Light_Data, location), place);

	return ChunkAccepted(MakeChunk<Light_Chunk>, bytes, sizeof(Light_Data));
}

// These keep their place first, and end with an empty name, so their size is their numbers and a zero byte, rounded up to whole numbers.
static bool SoundAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Sound_Object_Chunk>, 44, 0, place); }
static bool GeneratorAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<AVP_Generator_Chunk>, 32, 0, place); }
static bool PlayerStartAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<AVP_Player_Start_Chunk>, 56, 0, place); }
static bool CableAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<AVP_Power_Cable_Chunk>, 48, 0, place); }
static bool VirtualObjectAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Virtual_Object_Properties_Chunk>, 36, 0, place); }

// A particle generator keeps its place after 40 bytes of other numbers.
static bool ParticleGeneratorAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<AVP_Particle_Generator_Data_Chunk>, 72, 40, place); }

// A level's decals: a count, how big each is, and then the decals, each with its number and then its four corners.
static bool DecalCornerAccepted(int corner, ChunkVectorInt const & place)
{
	alignas(int) static char bytes[256];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, 1);
	PutNumber(bytes, 4, (int)sizeof(AVP_Decal));
	PutPlace(bytes + 8 + offsetof(AVP_Decal, Vertices) + corner * sizeof(ChunkVectorInt), place);

	return ChunkAccepted(MakeChunk<AVP_Decal_Chunk>, bytes, 8 + sizeof(AVP_Decal));
}

static bool DecalFirstCornerAccepted(ChunkVectorInt const & place) { return DecalCornerAccepted(0, place); }
static bool DecalLastCornerAccepted(ChunkVectorInt const & place) { return DecalCornerAccepted(3, place); }

// Where a breaking shape's pieces start: an empty name in 4 bytes, the number of pieces, and then the place.
static bool PiecesAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Fragment_Type_Shape_Chunk>, 32, 8, place); }

// A dummy object: its place, the bottom and top corners of its box, its turn and an empty name.
static bool DummyAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Dummy_Object_Data_Chunk>, 56, 0, place); }
static bool DummyBoxBottomAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Dummy_Object_Data_Chunk>, 56, 12, place); }
static bool DummyBoxTopAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Dummy_Object_Data_Chunk>, 56, 24, place); }

// A hierarchy's box: its bottom corner and its top corner, and nothing else.
static bool HierarchyBoxBottomAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Hierarchy_Bounding_Box_Chunk>, 24, 0, place); }
static bool HierarchyBoxTopAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Hierarchy_Bounding_Box_Chunk>, 24, 12, place); }

// A path: an empty name in 4 bytes, its number, its flags, a spare number, a count, and then its points.
static bool PathPointAccepted(ChunkVectorInt const & place)
{
	return PlaceInChunkAccepted(MakeChunk<AVP_Path_Chunk>, 20 + sizeof(ChunkPathPoint), 20 + offsetof(ChunkPathPoint, location), place, 16);
}

// A waypoint: a count, and then for each its box's bottom and top corners (from its centre) and its centre, its flags, a spare number, and
// two empty lists of links.
static bool WaypointAccepted(size_t placeAt, ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Module_Waypoint_Chunk>, 56, 4 + placeAt, place, 0); }
static bool WaypointBottomAccepted(ChunkVectorInt const & place) { return WaypointAccepted(0, place); }
static bool WaypointTopAccepted(ChunkVectorInt const & place) { return WaypointAccepted(12, place); }
static bool WaypointCentreAccepted(ChunkVectorInt const & place) { return WaypointAccepted(24, place); }

// A hierarchy's list of sequences: a count, and then for each its number of frames and three more numbers, and then its frames.
static bool HierarchyFrameMoveAccepted(ChunkVectorInt const & place)
{
	alignas(int) static char bytes[256];
	size_t const frameAt = 4 + 16;

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, 1);
	PutNumber(bytes, 4, 1);
	PutPlace(bytes + frameAt + offsetof(Object_Animation_Frame, transform), place);

	return ChunkAccepted(MakeChunk<Object_Animation_All_Sequence_Chunk>, bytes, frameAt + sizeof(Object_Animation_Frame));
}

// An animation frame: its turn, then how far it moves the object, then three numbers and an empty list.
static bool AnimationFrameMoveAccepted(ChunkVectorInt const & place) { return PlaceInChunkAccepted(MakeChunk<Object_Animation_Sequence_Frame_Chunk>, 44, 16, place); }

// The strategies that keep a box or a place. An area switch keeps its box after its multi-switch numbers, here with no targets and no links.
static bool AreaSwitchBottomAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratAreaSwitch, 44, place); }
static bool AreaSwitchTopAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratAreaSwitch, 56, place); }
static bool DeathVolumeBottomAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratDeathVolume, 12, place); }
static bool DeathVolumeTopAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratDeathVolume, 24, place); }
static bool TriggerVolumeBottomAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyTriggerVolume, 12, place); }
static bool TriggerVolumeTopAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyTriggerVolume, 24, place); }
static bool PivotObjectBottomAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyPivotObject, 36, place); }
static bool PivotObjectTopAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyPivotObject, 48, place); }
static bool ChestCameraAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyChest, 16, place); }
static bool CameraZoneBottomAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyAlterCameraRange, 12, place); }
static bool CameraZoneTopAccepted(ChunkVectorInt const & place) { return PlaceInStrategyAccepted(StratMummyAlterCameraRange, 24, place); }

typedef bool (*PlaceAccepted)(ChunkVectorInt const & place);

static void TestPlaces()
{
	puts("39. places");

	int const farthest = TestFarthestPlace;

	// Every way past the farthest, on one kind of chunk: each of x, y and z, each way
	Check(ObjectPlaceAccepted(PlaceAt(farthest, farthest, farthest)), "an object at the farthest corner there is, every way positive, is accepted");
	Check(ObjectPlaceAccepted(PlaceAt(-farthest, -farthest, -farthest)), "and at the farthest corner every way negative");
	Check(!ObjectPlaceAccepted(PlaceAt(farthest + 1, 0, 0)), "an object a millimetre past the farthest along x is refused: it would be outside the level");
	Check(!ObjectPlaceAccepted(PlaceAt(-farthest - 1, 0, 0)), "and the other way along x");
	Check(!ObjectPlaceAccepted(PlaceAt(0, farthest + 1, 0)), "and along y");
	Check(!ObjectPlaceAccepted(PlaceAt(0, -farthest - 1, 0)), "and the other way along y");
	Check(!ObjectPlaceAccepted(PlaceAt(0, 0, farthest + 1)), "and along z");
	Check(!ObjectPlaceAccepted(PlaceAt(0, 0, -farthest - 1)), "and the other way along z");
	Check(!ObjectPlaceAccepted(PlaceAt(0x7fffffff, 0, 0)), "and an object at the largest number there is");
	Check(ObjectPlaceAccepted(PlaceAt(1627790, -1627790, 1627790)), "the farthest an object is in the game's own levels, 1,627,790, is accepted");
	Check(LevelLightAccepted(PlaceAt(1855168, -1855168, 1855168)), "and the farthest place of all in them, a light at 1,855,168");

	// And every kind of chunk with a place in it: at the middle, at the farthest corner, and a millimetre past it one way or another
	struct PlaceRow { PlaceAccepted accepted; char const * what; };
	PlaceRow const rows[] =
	{
		{ ShapePointAccepted,           "a shape's point" },
		{ SubShapePointAccepted,        "a sub-shape's point" },
		{ FramePointAccepted,           "an animation frame's point" },
		{ ShapeBoxTopAccepted,          "the top corner of a shape's box" },
		{ ShapeBoxBottomAccepted,       "the bottom corner of a shape's box" },
		{ SubShapeBoxTopAccepted,       "the top corner of a sub-shape's box" },
		{ SubShapeBoxBottomAccepted,    "the bottom corner of a sub-shape's box" },
		{ ShapeCentreAccepted,          "a shape's centre" },
		{ AnimatedShapeCentreAccepted,  "an animated shape's centre" },
		{ PieceAccepted,                "where a shape's piece goes" },
		{ ObjectPlaceAccepted,          "an object" },
		{ TrackStartAccepted,           "a track's first pivot" },
		{ TrackEndAccepted,             "a track's last pivot" },
		{ TrackOffsetAccepted,          "an object's place on its track" },
		{ OtherPlaceAccepted,           "an object's other place" },
		{ WayInAccepted,                "a way into a module" },
		{ LevelLightAccepted,           "one of a level's lights" },
		{ SoundAccepted,                "a sound" },
		{ GeneratorAccepted,            "a generator" },
		{ PlayerStartAccepted,          "the player's start" },
		{ CableAccepted,                "a power cable" },
		{ ParticleGeneratorAccepted,    "a particle generator" },
		{ DecalFirstCornerAccepted,     "a decal's first corner" },
		{ DecalLastCornerAccepted,      "a decal's last corner" },
		{ VirtualObjectAccepted,        "a virtual object" },
		{ PiecesAccepted,               "where a breaking shape's pieces start" },
		{ DummyAccepted,                "a dummy object" },
		{ DummyBoxBottomAccepted,       "the bottom corner of a dummy object's box" },
		{ DummyBoxTopAccepted,          "the top corner of a dummy object's box" },
		{ HierarchyBoxBottomAccepted,   "the bottom corner of a hierarchy's box" },
		{ HierarchyBoxTopAccepted,      "the top corner of a hierarchy's box" },
		{ PathPointAccepted,            "a point on a path" },
		{ WaypointBottomAccepted,       "the bottom corner of a waypoint's box" },
		{ WaypointTopAccepted,          "the top corner of a waypoint's box" },
		{ WaypointCentreAccepted,       "a waypoint's centre" },
		{ HierarchyFrameMoveAccepted,   "how far a hierarchy's frame moves its object" },
		{ AnimationFrameMoveAccepted,   "how far an animation frame moves its object" },
		{ AreaSwitchBottomAccepted,     "the bottom corner of an area switch's box" },
		{ AreaSwitchTopAccepted,        "the top corner of an area switch's box" },
		{ DeathVolumeBottomAccepted,    "the bottom corner of a death volume" },
		{ DeathVolumeTopAccepted,       "the top corner of a death volume" },
		{ TriggerVolumeBottomAccepted,  "the bottom corner of a trigger volume" },
		{ TriggerVolumeTopAccepted,     "the top corner of a trigger volume" },
		{ PivotObjectBottomAccepted,    "the bottom corner of a pivoting object's trigger" },
		{ PivotObjectTopAccepted,       "the top corner of a pivoting object's trigger" },
		{ ChestCameraAccepted,          "a chest's camera" },
		{ CameraZoneBottomAccepted,     "the bottom corner of a camera's zone" },
		{ CameraZoneTopAccepted,        "the top corner of a camera's zone" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		std::string const what = rows[i].what;

		// The way past changes from row to row, so that between them every one of x, y and z is tried both ways
		int const past[3] = { (i % 3 == 0) ? farthest + 1 : 0, (i % 3 == 1) ? farthest + 1 : 0, (i % 3 == 2) ? farthest + 1 : 0 };
		int const sign = ((i / 3) % 2 == 0) ? 1 : -1;

		Check(rows[i].accepted(PlaceAt(0, 0, 0)), (what + " at the level's middle is accepted").c_str());
		Check(rows[i].accepted(PlaceAt(farthest, -farthest, farthest)), (what + " at the farthest a place can be is accepted").c_str());
		Check(!rows[i].accepted(PlaceAt(sign * past[0], sign * past[1], sign * past[2])),
			(what + " a millimetre past the farthest is refused: it would be outside the level").c_str());
	}
}

// 40. A shape's radius. The file keeps it as a fraction, and the game makes a whole number of it and adds it to other distances, so it has
// to be a real number from 0 up to 2^24 (CheckRadius, in chunk.hpp). Shapes and sub-shapes keep one in their headers, and a shape's centre
// chunk keeps its radius about the centre. This test is built with the ordinary arithmetic, under which a value that isn't a number never
// compares as in range, so it shows that the bit test on its own is what refuses one. (Added 2026-11-14 by Melanikus for the AvP port.)
static bool ShapeRadiusAccepted(int kind, float radius)
{
	if (kind == 2)
	{
		size_t const bytes = BuildShapeData(wholeShape, false, 4, PlaceTriangle);
		char * const centre = PutChunk(wholeShape + bytes, "SHPCENTR", 16);

		memcpy(centre + 12, &radius, sizeof(radius));
		return ShapeIsAccepted(centre + 16);
	}

	bool const subShape = (kind == 1);
	size_t const bytes = BuildShapeData(wholeShape, subShape, 4, PlaceTriangle);

	memcpy(wholeShape + ChunkHeaderBytes + (subShape ? 16 : 32), &radius, sizeof(radius));
	return subShape ? SubShapeIsAccepted(bytes) : ShapeIsAccepted(wholeShape + bytes);
}

static void TestShapeRadius()
{
	puts("40. a shape's radius");

	float const farthest = 16777216.0f;
	float const justPast = 16777218.0f;   // the next number a float can hold after 2^24
	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();
	char const * const kinds[3] = { "a shape's radius", "a sub-shape's radius", "a shape's radius about its centre" };

	for (int kind = 0; kind < 3; kind++)
	{
		std::string const what = kinds[kind];

		Check(ShapeRadiusAccepted(kind, 0.0f), (what + " of nothing is accepted").c_str());
		Check(ShapeRadiusAccepted(kind, 1520512.625f), (what + " the largest in the game's own levels, 1,520,512.6, is accepted").c_str());
		Check(ShapeRadiusAccepted(kind, farthest), (what + " the largest there can be, 2^24, is accepted").c_str());
		Check(!ShapeRadiusAccepted(kind, justPast), (what + " just past 2^24 is refused").c_str());
		Check(!ShapeRadiusAccepted(kind, -1.0f), (what + " that's negative is refused: a distance can't be").c_str());
		Check(!ShapeRadiusAccepted(kind, notANumber), (what + " that isn't a number is refused").c_str());
		Check(!ShapeRadiusAccepted(kind, endless), (what + " that's endless is refused").c_str());
		Check(!ShapeRadiusAccepted(kind, -endless), (what + " that's endlessly negative is refused").c_str());
	}

	unsigned int const negativeNotANumberBits = 0xFFC00000u;
	float negativeNotANumber;
	memcpy(&negativeNotANumber, &negativeNotANumberBits, sizeof(negativeNotANumber));
	Check(!ShapeRadiusAccepted(0, negativeNotANumber), "a shape's radius that isn't a number, with its sign bit set, is refused too");
}

// 41. A particle generator's kind. It's one of four, from 0 (sparks) to 3 (flames), and the game picks what to make by it, so any other
// number is refused. Its kind is the first number in its data chunk. (Added 2026-11-14 by Melanikus for the AvP port.)
static bool ParticleKindAccepted(int kind)
{
	alignas(int) static char bytes[128];

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, kind);

	return ChunkAccepted(MakeChunk<AVP_Particle_Generator_Data_Chunk>, bytes, 72);
}

static void TestParticleKind()
{
	puts("41. a particle generator's kind");

	Check(ParticleKindAccepted(0), "a particle generator of the first kind, sparks, is accepted");
	Check(ParticleKindAccepted(1) && ParticleKindAccepted(2), "and the two kinds between, steam and black smoke");
	Check(ParticleKindAccepted(3), "and the last kind, flames");
	Check(!ParticleKindAccepted(4), "a kind past the last is refused: there are only four kinds");
	Check(!ParticleKindAccepted(-1), "a negative kind is refused");
	Check(!ParticleKindAccepted(0x7fffffff), "the largest number there is as a kind is refused");
	Check(!ParticleKindAccepted((int)0x80000000), "and the most negative");
}

// 42. Turns. A turn is kept as four fractions, and each part has to be a real number from -2 to 2 (CheckTurn, in chunk.hpp): a real turn's
// parts are from -1 to 1, a tool can save one a rounding step past that, and the game makes a whole number of each. Each kind of chunk that
// keeps a turn is made here with that turn set and everything else as the game's own levels could have it. This test is built with the
// ordinary arithmetic, under which a part that isn't a number never compares as in range, so it shows that the bit test on its own is what
// refuses one. (Added 2026-11-14 by Melanikus for the AvP port.)
static float FloatFromBits(unsigned int bits)
{
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static ChunkQuat TurnOf(float x, float y, float z, float w)
{
	ChunkQuat const turn = { x, y, z, w };
	return turn;
}

static void PutTurn(char * where, ChunkQuat const & turn)
{
	memcpy(where, &turn, sizeof(turn));
}

// A chunk of this kind and size, all zeros but for the turn at "at", and a count of 1 at "countAt" when the chunk holds a list.
static bool TurnInChunkAccepted(MakesChunk make, size_t bytes, size_t at, ChunkQuat const & turn, int countAt = -1)
{
	alignas(int) static char data[256];

	memset(data, 0, sizeof(data));
	PutTurn(data + at, turn);

	if (countAt >= 0)
	{
		PutNumber(data, (size_t)countAt, 1);
	}

	return ChunkAccepted(make, data, bytes);
}

// An object's header keeps its turn after its place, at byte 32.
static bool ObjectTurnAccepted(ChunkQuat const & turn)
{
	TestFile file;
	size_t const bytes = BuildObjectHeader("object", 7);

	PutTurn(objectBytes + ChunkHeaderBytes + 32, turn);
	std::unique_ptr<Object_Chunk> const objectOwner = LoadObject(file, bytes);

	return file.error_code == 0;
}

// A track with one section that takes a second, with one of its turns set.
static bool TrackTurnAccepted(size_t turnAt, ChunkQuat const & turn)
{
	alignas(int) static char bytes[256];
	size_t const sectionAt = sizeof(int);

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, 1);
	PutNumber(bytes, sectionAt + offsetof(ChunkTrackSection, time_for_section), 65536);
	PutTurn(bytes + sectionAt + turnAt, turn);

	return ChunkAccepted(MakeChunk<Object_Track_Chunk2>, bytes, sectionAt + sizeof(ChunkTrackSection) + 2 * sizeof(int));
}

static bool TrackStartTurnAccepted(ChunkQuat const & turn) { return TrackTurnAccepted(offsetof(ChunkTrackSection, quat_start), turn); }
static bool TrackEndTurnAccepted(ChunkQuat const & turn) { return TrackTurnAccepted(offsetof(ChunkTrackSection, quat_end), turn); }

static bool OtherTurnAccepted(ChunkQuat const & turn)
{
	return TurnInChunkAccepted(MakeChunk<Object_Alternate_Locations_Chunk>, 12 + sizeof(AltObjectLocation), 12 + offsetof(AltObjectLocation, orientation), turn, 8);
}

static bool ParticleTurnAccepted(ChunkQuat const & turn) { return TurnInChunkAccepted(MakeChunk<AVP_Particle_Generator_Data_Chunk>, 72, 52, turn); }
static bool DummyTurnAccepted(ChunkQuat const & turn) { return TurnInChunkAccepted(MakeChunk<Dummy_Object_Data_Chunk>, 56, 36, turn); }
static bool AnimationFrameTurnAccepted(ChunkQuat const & turn) { return TurnInChunkAccepted(MakeChunk<Object_Animation_Sequence_Frame_Chunk>, 44, 0, turn); }

// A hierarchy's list of sequences, with one sequence of one frame, whose turn is set
static bool HierarchyFrameTurnAccepted(ChunkQuat const & turn)
{
	alignas(int) static char bytes[256];
	size_t const frameAt = 4 + 16;

	memset(bytes, 0, sizeof(bytes));
	PutNumber(bytes, 0, 1);
	PutNumber(bytes, 4, 1);
	PutTurn(bytes + frameAt + offsetof(Object_Animation_Frame, orientation), turn);

	return ChunkAccepted(MakeChunk<Object_Animation_All_Sequence_Chunk>, bytes, frameAt + sizeof(Object_Animation_Frame));
}

typedef bool (*TurnAccepted)(ChunkQuat const & turn);

static void TestTurns()
{
	puts("42. turns");

	float const justPast = FloatFromBits(0x40000001);   // the next number a float can hold after 2
	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const negativeNotANumber = FloatFromBits(0xFFC00000);
	float const endless = std::numeric_limits<float>::infinity();

	// Every way a part can be wrong, on one kind of chunk
	Check(ObjectTurnAccepted(TurnOf(0.7071068f, 0.0f, 0.7071068f, 0.0f)), "an object turned a quarter of the way round is accepted");
	Check(ObjectTurnAccepted(TurnOf(1.0f, -1.0f, 1.0f, -1.0f)), "and with every part at 1 or -1, as far as the game's own levels go");
	char const * const parts[4] = { "x", "y", "z", "w" };

	for (int part = 0; part < 4; part++)
	{
		float values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		std::string const which = std::string("an object's turn with its ") + parts[part];

		values[part] = justPast;
		Check(!ObjectTurnAccepted(TurnOf(values[0], values[1], values[2], values[3])), (which + " just past 2 is refused: no turn is that far").c_str());
		values[part] = -justPast;
		Check(!ObjectTurnAccepted(TurnOf(values[0], values[1], values[2], values[3])), (which + " just past -2 is refused").c_str());
		values[part] = notANumber;
		Check(!ObjectTurnAccepted(TurnOf(values[0], values[1], values[2], values[3])), (which + " not a number is refused").c_str());
	}

	Check(!ObjectTurnAccepted(TurnOf(negativeNotANumber, 0.0f, 0.0f, 1.0f)), "and one with a part that isn't a number, with its sign bit set");
	Check(!ObjectTurnAccepted(TurnOf(endless, 0.0f, 0.0f, 1.0f)), "and one with an endless part");
	Check(!ObjectTurnAccepted(TurnOf(-endless, 0.0f, 0.0f, 1.0f)), "and one with an endlessly negative part");

	// And every kind of chunk with a turn in it
	struct TurnRow { TurnAccepted accepted; char const * what; };
	TurnRow const rows[] =
	{
		{ ObjectTurnAccepted,          "an object's turn" },
		{ TrackStartTurnAccepted,      "a track's first turn" },
		{ TrackEndTurnAccepted,        "a track's last turn" },
		{ OtherTurnAccepted,           "an object's other turn" },
		{ ParticleTurnAccepted,        "a particle generator's turn" },
		{ DummyTurnAccepted,           "a dummy object's turn" },
		{ HierarchyFrameTurnAccepted,  "a hierarchy's frame's turn" },
		{ AnimationFrameTurnAccepted,  "an animation frame's turn" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		std::string const what = rows[i].what;
		float past[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float broken[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		// The part that's wrong changes from row to row, and so does the way past
		past[i % 4] = ((i / 4) % 2 == 0) ? justPast : -justPast;
		broken[(i + 1) % 4] = notANumber;

		Check(rows[i].accepted(TurnOf(0.0f, 0.0f, 0.0f, 1.0f)), (what + " of no turn at all is accepted").c_str());
		Check(rows[i].accepted(TurnOf(2.0f, -2.0f, 2.0f, -2.0f)), (what + " with every part as far as it can be is accepted").c_str());
		Check(!rows[i].accepted(TurnOf(past[0], past[1], past[2], past[3])), (what + " with a part just past 2 is refused: no turn is that far").c_str());
		Check(!rows[i].accepted(TurnOf(broken[0], broken[1], broken[2], broken[3])), (what + " with a part that isn't a number is refused").c_str());
	}
}

// 43. A set of random textures is found by its name, so one with no name is refused as it's read, and the level loader passes such a set
// over as well (projload.cpp, which level_file_name_test reads). A set: its name, a count of 0 here, and two spare numbers. (Added
// 2026-11-14 by Melanikus for the AvP port; a set with no name was read and passed over, and is now refused.)
static bool RandomTexturesAccepted(char const * name)
{
	File_Chunk file;
	Parent_File = &file;

	alignas(int) char bytes[16] = { 0 };
	memcpy(bytes, name, strlen(name));
	std::unique_ptr<Random_Texture_ID_Chunk> const set(new Random_Texture_ID_Chunk(&file, bytes, sizeof(bytes)));

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;
	return accepted;
}

static void TestRandomTextureName()
{
	puts("43. a set of random textures with no name");

	Check(RandomTexturesAccepted("abc"), "a set of random textures with a name is accepted");
	Check(!RandomTexturesAccepted(""), "a set of random textures with an empty name is refused: it couldn't be found by its name");
}

// 44. A shape's name for the list of named shapes. An outside shape keeps it in an object name or a level name chunk, and a sub-shape (a
// skin) in a shape name chunk, and each can have none. An outside shape with neither, and a skin in an animated shape's alternate textures
// with none, are refused as the level is read; the level loader also leaves a shape with no name off that list (chunk_load.cpp, which
// level_file_name_test reads). (Added 2026-11-14 by Melanikus for the AvP port; refused at reading since N2r.)
static void TestShapesWithNoName()
{
	puts("44. shapes with no name");

	{
		File_Chunk file;
		Parent_File = &file;

		std::unique_ptr<Shape_External_File_Chunk> const outside(new Shape_External_File_Chunk(&file, wholeShape, 0));
		Check(outside->get_shape_name() == 0, "an outside shape with no name chunk has no name");
		outside->post_input_processing();
		Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "so it's refused as the level is read: it couldn't be found by its name");

		Parent_File = 0;
	}

	{
		File_Chunk file;
		Parent_File = &file;

		// An object name chunk: a spare number, then the name
		char * const named = PutChunk(wholeShape, "EXTOBJNM", 8);
		memcpy(named + 4, "abc", 4);

		std::unique_ptr<Shape_External_File_Chunk> const outside(new Shape_External_File_Chunk(&file, wholeShape, ChunkHeaderBytes + 8));
		outside->post_input_processing();
		Check(file.error_code == 0 && outside->get_shape_name() != 0, "an outside shape with an object name chunk is accepted, and has its name");

		Parent_File = 0;
	}

	{
		File_Chunk file;
		Parent_File = &file;

		size_t const bytes = BuildShapeData(wholeShape, true, 4, PlaceTriangle);
		std::unique_ptr<Shape_Sub_Shape_Chunk> const loose(new Shape_Sub_Shape_Chunk(&file, wholeShape, bytes));
		loose->post_input_processing();
		Check(file.error_code == 0 && loose->get_shape_name() == 0, "a sub-shape with no shape name chunk has no name, and elsewhere that's allowed");

		Parent_File = 0;
	}

	for (int named = 0; named < 2; named++)
	{
		File_Chunk file;
		Parent_File = &file;

		std::unique_ptr<Anim_Shape_Alternate_Texturing_Chunk> const textures(new Anim_Shape_Alternate_Texturing_Chunk(&file, wholeShape, 0));
		size_t bytes = BuildShapeData(wholeShape, true, 4, PlaceTriangle);

		if (named)
		{
			char * const name = PutChunk(wholeShape + bytes, "SHPFNAME", 4);
			memcpy(name, "abc", 4);
			bytes += ChunkHeaderBytes + 4;
		}

		std::unique_ptr<Shape_Sub_Shape_Chunk> const skin(new Shape_Sub_Shape_Chunk(textures.get(), wholeShape, bytes));
		skin->post_input_processing();

		if (named)
		{
			Check(file.error_code == 0, "a skin in an animated shape's alternate textures with a shape name chunk is accepted");
		}
		else
		{
			Check(file.error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "and one with none is refused: it couldn't be found by its name");
		}

		Parent_File = 0;
	}
}

// 45. An animation frame's polygon normals have to be sensible numbers by the same rule as a shape's own (normals_are_valid, in
// chunk_shape.cpp): each a real number no bigger than 2 either way. The frame's normal comes after the sequence's header and data, the
// frame's header and data, and its points (see PutAnimation). (Added 2026-11-14 by Melanikus for the AvP port.)
static bool FrameNormalAccepted(float x, float y, float z)
{
	size_t const bytes = BuildShapeData(wholeShape, false, 4, PlaceTriangle);
	char * const end = PutAnimation(wholeShape + bytes, 0, true, 0, 4, 1, 0);
	size_t const normalAt = ChunkHeaderBytes + (ChunkHeaderBytes + 24) + ChunkHeaderBytes + (ChunkHeaderBytes + 24) + (ChunkHeaderBytes + 12 * 4) + ChunkHeaderBytes;
	float const normal[3] = { x, y, z };

	memcpy(wholeShape + bytes + normalAt, normal, sizeof(normal));
	return ShapeIsAccepted(end);
}

static void TestFrameNormals()
{
	puts("45. an animation frame's normals");

	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();
	unsigned int const justPastTwoBits = 0x40000001u;
	unsigned int const negativeNotANumberBits = 0xFFC00000u;
	float justPastTwo;
	float negativeNotANumber;
	memcpy(&justPastTwo, &justPastTwoBits, sizeof(justPastTwo));
	memcpy(&negativeNotANumber, &negativeNotANumberBits, sizeof(negativeNotANumber));

	Check(FrameNormalAccepted(0.0f, 0.0f, 1.0f), "an animation frame whose normal points straight up is accepted");
	Check(FrameNormalAccepted(2.0f, -2.0f, 2.0f), "and one with each number as far as the rule allows, 2 either way");
	Check(!FrameNormalAccepted(justPastTwo, 0.0f, 0.0f), "a frame's normal with a number just past 2 is refused, as a shape's would be");
	Check(!FrameNormalAccepted(0.0f, notANumber, 0.0f), "and one with a number that isn't a number");
	Check(!FrameNormalAccepted(0.0f, 0.0f, negativeNotANumber), "and one that isn't a number with its sign bit set");
	Check(!FrameNormalAccepted(endless, 0.0f, 0.0f), "and one with an endless number");
}

// 46. A texture coordinate, u or v, is a place on a picture in texels, and the game makes a whole number of it, so it has to be a real
// number no further either way than an animated corner's can be (kTextureAnimationFurthestTexel, texture_animation.h). A shape's or a
// sub-shape's list of them: how many lists, then each list's number of corners and u and v for each. (Added 2026-11-14 by Melanikus for
// the AvP port.)
static bool TextureCoordinateAccepted(bool subShape, float u, float v)
{
	size_t const bytes = BuildShapeData(wholeShape, subShape, 4, PlaceTriangle);
	char * const data = PutChunk(wholeShape + bytes, "SHPUVCRD", 2 * sizeof(int) + 3 * 2 * sizeof(float));
	int const lists = 1;
	int const corners = 3;
	float const place[2] = { u, v };

	memcpy(data, &lists, sizeof(lists));
	memcpy(data + sizeof(int), &corners, sizeof(corners));
	memcpy(data + 2 * sizeof(int) + 2 * sizeof(float), place, sizeof(place));

	char * const end = data + 2 * sizeof(int) + 3 * 2 * sizeof(float);
	return subShape ? SubShapeIsAccepted(end - wholeShape) : ShapeIsAccepted(end);
}

static void TestTextureCoordinateNumbers()
{
	puts("46. a texture coordinate's numbers");

	float const furthest = 16383.0f;
	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();
	unsigned int const justPastBits = 0x467FFC01u;   // the next float after 16383
	unsigned int const negativeNotANumberBits = 0xFFC00000u;
	float justPast;
	float negativeNotANumber;
	memcpy(&justPast, &justPastBits, sizeof(justPast));
	memcpy(&negativeNotANumber, &negativeNotANumberBits, sizeof(negativeNotANumber));

	Check(TextureCoordinateAccepted(false, 127.0f, 127.0f), "a corner at 127, 127, as most in the game's own levels are, is accepted");
	Check(TextureCoordinateAccepted(false, -1800.0f, -4715.0f) && TextureCoordinateAccepted(false, 3857.0f, 3081.0f), "and so are the furthest in them, either way");
	Check(TextureCoordinateAccepted(false, 0.5f, 63.25f), "and a place between whole texels");
	Check(TextureCoordinateAccepted(false, furthest, -furthest), "and the furthest a corner can be, 16383 texels either way");
	Check(!TextureCoordinateAccepted(false, justPast, 0.0f), "a u just past 16383 is refused: no corner can be that far");
	Check(!TextureCoordinateAccepted(false, 0.0f, -justPast), "and a v just past it the other way");
	Check(!TextureCoordinateAccepted(false, notANumber, 0.0f), "a u that isn't a number is refused");
	Check(!TextureCoordinateAccepted(false, 0.0f, notANumber), "and a v");
	Check(!TextureCoordinateAccepted(false, negativeNotANumber, 0.0f), "and one that isn't a number with its sign bit set");
	Check(!TextureCoordinateAccepted(false, endless, 0.0f) && !TextureCoordinateAccepted(false, 0.0f, -endless), "and endless either way");
	Check(TextureCoordinateAccepted(true, 127.0f, 127.0f), "a sub-shape's corner at 127, 127 is accepted");
	Check(!TextureCoordinateAccepted(true, notANumber, 0.0f), "and one that isn't a number is refused");
}

// 47. A light scale: its mode, then six fractions (the prelight's multiplier, the level it starts above and what it adds, then the same three
// for the light at run time), then three spare numbers. Four of the fractions are made whole numbers as it's read, so each has to be a real
// number no bigger than 2^24 either way. (Added 2026-11-14 by Melanikus for the AvP port.)
static bool LightScaleAccepted(float const numbers[6])
{
	alignas(int) static char bytes[64];

	memset(bytes, 0, sizeof(bytes));
	memcpy(bytes + 4, numbers, 6 * sizeof(float));

	return ChunkAccepted(MakeChunk<Light_Scale_Chunk>, bytes, 40);
}

static void TestLightScale()
{
	puts("47. a light scale");

	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();
	unsigned int const justPastBits = 0x4B800001u;   // the next float after 2^24
	float justPast;
	memcpy(&justPast, &justPastBits, sizeof(justPast));

	float const nothing[6] = { 0, 0, 0, 0, 0, 0 };
	float const plain[6] = { 1.0f, 0, 0, 1.0f, 0, 0 };
	float const largest[6] = { 16777216.0f, -16777216.0f, 16777216.0f, -16777216.0f, 16777216.0f, -16777216.0f };

	Check(LightScaleAccepted(nothing), "a light scale of nothing is accepted");
	Check(LightScaleAccepted(plain), "and one that changes nothing, a multiplier of 1 and nothing added");
	Check(LightScaleAccepted(largest), "and one with every number as big as it can be, 2^24 either way");

	char const * const names[6] = { "prelight multiplier", "prelight's level to start above", "prelight's addition", "run-time multiplier", "run-time level to start above", "run-time addition" };

	for (int i = 0; i < 6; i++)
	{
		std::string const which = std::string("a light scale whose ") + names[i];
		float numbers[6] = { 1.0f, 0, 0, 1.0f, 0, 0 };

		numbers[i] = notANumber;
		Check(!LightScaleAccepted(numbers), (which + " isn't a number is refused").c_str());
		numbers[i] = (i % 2 == 0) ? justPast : -justPast;
		Check(!LightScaleAccepted(numbers), (which + " is just past 2^24 is refused: it couldn't be a whole number the game holds").c_str());
		numbers[i] = endless;
		Check(!LightScaleAccepted(numbers), (which + " is endless is refused").c_str());
	}
}

// 48. A level's and a module's amount of echo: a sound setting's number, then the amount as a fraction, then a spare number. The game compares
// the amount to choose what it means (from 0 to 1 is how much, and anything else leaves it to distance), so it has to be a real number.
// (Added 2026-11-14 by Melanikus for the AvP port.)
static bool EchoAccepted(bool module, float amount)
{
	alignas(int) static char bytes[16];

	memset(bytes, 0, sizeof(bytes));
	memcpy(bytes + 4, &amount, sizeof(amount));

	return module ? ChunkAccepted(MakeChunk<Module_Acoustics_Chunk>, bytes, 12) : ChunkAccepted(MakeChunk<Environment_Acoustics_Chunk>, bytes, 12);
}

static void TestEcho()
{
	puts("48. the amount of echo");

	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();
	unsigned int const negativeNotANumberBits = 0xFFC00000u;
	float negativeNotANumber;
	memcpy(&negativeNotANumber, &negativeNotANumberBits, sizeof(negativeNotANumber));

	for (int kind = 0; kind < 2; kind++)
	{
		bool const module = (kind == 1);
		std::string const which = module ? "a module's amount of echo" : "a level's amount of echo";

		Check(EchoAccepted(module, -1.0f) && EchoAccepted(module, 0.2f) && EchoAccepted(module, 2.0f), (which + " of -1, 0.2 or 2, as in the game's own levels, is accepted").c_str());
		Check(EchoAccepted(module, 0.5f), (which + " of a half is accepted").c_str());
		Check(!EchoAccepted(module, notANumber), (which + " that isn't a number is refused: no comparison can choose what it means").c_str());
		Check(!EchoAccepted(module, negativeNotANumber), (which + " that isn't a number, with its sign bit set, is refused").c_str());
		Check(!EchoAccepted(module, endless) && !EchoAccepted(module, -endless), (which + " that's endless either way is refused").c_str());
	}
}

// 49. A generator's weights. The game adds a generator's 15 weights up and divides by the total, so each has to be from 0 to 2^24. Its
// extended settings: five numbers, then the block's size (64 here) and its 15 weights, from place 24. (Added 2026-11-14 by Melanikus for the
// AvP port.)
static bool GeneratorWeightAccepted(int which, int weight)
{
	File_Chunk file;
	Parent_File = &file;

	int const blockSize = 64;
	alignas(int) char bytes[84] = { 0 };
	memcpy(bytes + 20, &blockSize, sizeof(int));
	memcpy(bytes + 24 + which * sizeof(int), &weight, sizeof(int));

	std::unique_ptr<AVP_Generator_Extended_Settings_Chunk> const settings(new AVP_Generator_Extended_Settings_Chunk(&file, bytes, sizeof(bytes)));

	bool const accepted = (file.error_code == 0);

	Parent_File = 0;
	return accepted;
}

static void TestGeneratorWeights()
{
	puts("49. a generator's weights");

	Check(GeneratorWeightAccepted(0, 12), "a weight of 12, the largest in the game's own levels, is accepted");
	Check(GeneratorWeightAccepted(14, 1 << 24), "and the largest a weight can be, 2^24, in the last place");
	Check(!GeneratorWeightAccepted(0, (1 << 24) + 1), "a weight just past 2^24 is refused: the total couldn't be held");
	Check(!GeneratorWeightAccepted(0, 0x7fffffff), "and the largest number there is");

	for (int which = 0; which < 15; which++)
	{
		std::string const what = "a negative weight in place " + std::to_string(which + 1) + " of 15 is refused";
		Check(!GeneratorWeightAccepted(which, -1), what.c_str());
	}
}

// 50. A module's master and slave chunks. A slave module shares its master's room in the AI, and the game gives it a place there only
// through its master. A module left with no place would still be linked to by the modules beside it, so a slave has to name a module that
// is a master and isn't a slave itself, or the level is refused. projload.cpp's own check that no such place is used is in
// ai_module_places_test. (Added 2026-11-14 by Melanikus for the AvP port.)

// One module of a test level: its object number, whether it has module data at all, and in it a master chunk, a slave chunk naming
// another module by its number (or -1 for none), or both
struct ModuleRole { int number; bool hasModuleData; bool master; int slaveOf; };

// Gives a level these modules, all of one shape that fits its box (as in section 34), and says whether the level would be accepted
static bool ModulesAccepted(ModuleRole const * roles, size_t howMany)
{
	int const triangle[5] = { 0, 1, 2, -1, -1 };
	int const shapeNumber = 7;
	int const firstPointX = 0;

	File_Chunk file;
	Parent_File = &file;

	size_t const shapeBytesUsed = BuildShapeData(wholeShape, false, 4, triangle);
	memcpy(wholeShape + ChunkHeaderBytes + ShapeNumberAt, &shapeNumber, sizeof(shapeNumber));
	memcpy(wholeShape + ChunkHeaderBytes + ShapeHeaderDataBytes + ChunkHeaderBytes, &firstPointX, sizeof(int));
	std::unique_ptr<Shape_Chunk> const shape(new Shape_Chunk(&file, wholeShape, shapeBytesUsed));

	std::unique_ptr<Object_Chunk> modules[4];

	for (size_t i = 0; i < howMany && i < sizeof(modules) / sizeof(modules[0]); i++)
	{
		ModuleRole const &role = roles[i];
		size_t used = BuildObject(role.number, shapeNumber);

		if (role.hasModuleData)
		{
			size_t const inside = (role.master ? ChunkHeaderBytes : 0) + (role.slaveOf >= 0 ? ChunkHeaderBytes + sizeof(int) : 0);
			char * at = PutChunk(objectBytes + used, "MODULEDT", inside);

			if (role.master)
			{
				at = PutChunk(at, "AIMODMAS", 0);
			}

			if (role.slaveOf >= 0)
			{
				at = PutChunk(at, "AIMODSLA", sizeof(int));
				memcpy(at, &role.slaveOf, sizeof(int));
			}

			used += ChunkHeaderBytes + inside;
		}

		modules[i].reset(new Object_Chunk(&file, objectBytes, used));
	}

	file.post_input_processing();

	bool const accepted = (file.error_code == 0);

	for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++)
	{
		modules[i].reset();
	}

	Parent_File = 0;

	return accepted;
}

static void TestModuleMasters()
{
	puts("50. a module's master and slaves");

	// A master, and two slaves that name it. The game's own levels are all like this.
	{
		ModuleRole const roles[] = { { 5, true, true, -1 }, { 6, true, false, 5 }, { 8, true, false, 5 } };
		Check(ModulesAccepted(roles, 3), "a master and the slaves that name it are accepted");
	}

	// Modules that are neither, and a master with no slaves, are as they always were
	{
		ModuleRole const roles[] = { { 5, true, false, -1 }, { 6, false, false, -1 }, { 8, true, true, -1 } };
		Check(ModulesAccepted(roles, 3), "a module that is neither, one with no module data, and a master with no slaves are accepted");
	}

	struct BadRow { ModuleRole roles[3]; size_t howMany; char const * what; };
	BadRow const rows[] =
	{
		{ { { 5, true, true, -1 }, { 6, true, false, 9 } }, 2, "a slave naming no object at all is refused" },
		{ { { 5, false, false, -1 }, { 6, true, false, 5 } }, 2, "a slave naming an object that isn't a module is refused" },
		{ { { 5, true, false, -1 }, { 6, true, false, 5 } }, 2, "a slave naming a module that isn't a master is refused" },
		{ { { 7, true, true, -1 }, { 5, true, true, 7 }, { 6, true, false, 5 } }, 3, "a slave naming a master that is a slave itself is refused" },
		{ { { 6, true, true, 6 } }, 1, "a module that is its own master and its own slave is refused" },
		{ { { 6, true, false, 6 } }, 1, "a slave naming itself is refused" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		Check(!ModulesAccepted(rows[i].roles, rows[i].howMany), rows[i].what);
	}
}

// 51. Flags a file may and may not give. A module's flags (MODFLAGS) may hold only what a level maker can set in the editor, and an item in
// a module's visibility list (VMDARRAY) no flags at all: the rest is the game's own state while it runs, such as "this list's branches are
// already places", which a file could otherwise use to have a number taken as a place. (Added 2026-11-14 by Melanikus for the AvP port.)

// A module's flags chunk holding these flags, and nothing in its spare number
static bool ModuleFlagsAccepted(unsigned int flags)
{
	alignas(int) char bytes[8] = { 0 };
	memcpy(bytes, &flags, sizeof(flags));

	return ChunkAccepted(MakeChunk<Module_Flag_Chunk>, bytes, sizeof(bytes));
}

// A visibility list of one item, seeing object 0, with no branch, and these flags
static bool VisibilityItemAccepted(int flags)
{
	alignas(int) char bytes[4 + 4 * sizeof(int)] = { 0 };
	int const items = 1;
	memcpy(bytes, &items, sizeof(items));
	memcpy(bytes + 4 + 2 * sizeof(int), &flags, sizeof(flags));

	return ChunkAccepted(MakeChunk<VModule_Array_Chunk>, bytes, sizeof(bytes));
}

static void TestFlagsFromAFile()
{
	puts("51. flags a file may give");

	// Everything a level maker can set, all at once, and each part of it
	Check(ModuleFlagsAccepted(0xFFFF0200u), "a module with every flag the editor sets is accepted");
	Check(ModuleFlagsAccepted(0), "and one with none");
	Check(ModuleFlagsAccepted(0x20000000u), "and the sky alone, as most of the game's own have");
	Check(ModuleFlagsAccepted(127u << 21), "and a message number, the highest");

	// The game's own state while it runs: none of it may come from a file
	struct FlagRow { unsigned int flag; char const * what; };
	FlagRow const running[] =
	{
		{ 0x00000001u, "a module's 'no extent test' from a file is refused" },
		{ 0x00000002u, "a module's 'lists already turned into places' from a file is refused" },
		{ 0x00000004u, "a module's 'door open' from a file is refused" },
		{ 0x00000008u, "a module's 'not active' from a file is refused" },
		{ 0x00000010u, "a module's 'got its matrix' from a file is refused" },
		{ 0x00000020u, "a module's 'seen on the map' from a file is refused" },
		{ 0x00000040u, "a module's 'slipped inside' from a file is refused" },
		{ 0x00000100u, "a bit nobody names, below 'exclude from the map', is refused" },
		{ 0x00008000u, "and one just under 'victory zone'" },
	};

	for (size_t i = 0; i < sizeof(running) / sizeof(running[0]); i++)
	{
		Check(!ModuleFlagsAccepted(running[i].flag), running[i].what);
	}

	Check(!ModuleFlagsAccepted(0x20000002u), "and a running-state bit beside an editor's one is still refused");

	// An item in a visibility list has no flags a file may set
	Check(VisibilityItemAccepted(0), "a visibility list's item with no flags is accepted");
	Check(!VisibilityItemAccepted(1), "an item's 'branch already a place' from a file is refused");
	Check(!VisibilityItemAccepted(2), "and so is any other flag");
	Check(!VisibilityItemAccepted((int)0x80000000u), "even the top one");
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
	TestPlaces();
	TestShapeRadius();
	TestParticleKind();
	TestTurns();
	TestRandomTextureName();
	TestShapesWithNoName();
	TestFrameNormals();
	TestTextureCoordinateNumbers();
	TestLightScale();
	TestEcho();
	TestGeneratorWeights();
	TestModuleMasters();
	TestFlagsFromAFile();

	if (checksFailed)
	{
		printf("Rebecca's test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("Rebecca's test passed");
	return 0;
}
