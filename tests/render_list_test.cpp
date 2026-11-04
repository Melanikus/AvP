// The render list test. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Everything the game draws in a frame first goes into lists: the level's shapes, bullet marks, smoke, and the writing on
// the screen. Each list has two blocks of memory that it shares with the graphics card, called buffers. One holds the
// corners of the shapes, and the other holds the "index numbers" that say which corners make which triangle. The buffers
// are a fixed size, and they may only be written to while they're open.
//
// The code that writes shapes used to write without asking. A frame with too many shapes wrote past the end of a buffer,
// and anything written while a buffer was closed went into memory the graphics card's driver had taken back. So now a
// list is asked first, and says no when it's full or closed. This test fills lists until they say no, then checks that
// nothing was written past the end, that the list stayed in order, and that it gave the right reason.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file together with the game's real RenderList.cpp, VertexBuffer.cpp and IndexBuffer.cpp,
// using the settings in tests/render_list_test.args. There's no graphics card involved: the stand-ins below give the
// lists ordinary memory instead. After the end of each block is a long stretch filled with a known pattern. The writing
// in this test does what the game's writers do, so a list that wrongly says yes gets written past its end, the pattern
// changes, and the test notices. The program gives back a non-zero result if anything failed.
//
// WHAT IT CAN'T TEST
//
// The writers themselves are in d3d_render.cpp, which brings most of the game with it. So the writers here are copies of
// their shape, and D3D_DrawParticle_Rain's is one of them. The compiler checks the real ones: it complains about any
// writer that ignores the list's answer.

#include <map>
#include <new>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RenderList.h"
#include "console.h"
#include "logString.h"

extern uint32_t GetNumIndices(uint32_t numVerts);

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the graphics card ----------------------------------------------------------------------------------

// A buffer's memory, with the known pattern after its end. The stretch is long enough to hold the worst overrun below,
// so an overrun shows up as a failed check rather than damage to the test itself.
struct Block
{
	std::vector<unsigned char> bytes;
	size_t size;
};

static size_t const kPatternBytes = 512 * 1024;
static unsigned char const kPattern = 0xAB;

static std::map<void const *, Block> blocks;

static void MakeBlock(void const *owner, size_t size)
{
	Block &block = blocks[owner];
	block.size = size;
	block.bytes.assign(size + kPatternBytes, 0);
	memset(block.bytes.data() + size, kPattern, kPatternBytes);
}

static bool PatternUntouched(void const *owner)
{
	Block const &block = blocks[owner];

	for (size_t i = block.size; i < block.bytes.size(); i++)
	{
		if (block.bytes[i] != kPattern)
		{
			return false;
		}
	}

	return true;
}

// Set to make opening a buffer fail, which is what a graphics card that has gone away does
static bool opensFail = false;

bool R_CreateVertexBuffer(VertexBuffer &vertexBuffer)
{
	MakeBlock(&vertexBuffer, vertexBuffer._sizeInBytes);
	return true;
}

bool R_ReleaseVertexBuffer(VertexBuffer &vertexBuffer)
{
	blocks.erase(&vertexBuffer);
	return true;
}

bool R_LockVertexBuffer(VertexBuffer &vertexBuffer, uint32_t, uint32_t, void **data, enum R_USAGE)
{
	if (opensFail || vertexBuffer._isLocked)
	{
		return false;
	}

	*data = blocks[&vertexBuffer].bytes.data();
	vertexBuffer._isLocked = true;
	return true;
}

bool R_UnlockVertexBuffer(VertexBuffer &vertexBuffer)
{
	vertexBuffer._isLocked = false;
	return true;
}

bool R_SetVertexBuffer(VertexBuffer &)
{
	return true;
}

bool R_CreateIndexBuffer(IndexBuffer &indexBuffer)
{
	MakeBlock(&indexBuffer, indexBuffer._sizeInBytes);
	return true;
}

bool R_ReleaseIndexBuffer(IndexBuffer &indexBuffer)
{
	blocks.erase(&indexBuffer);
	return true;
}

bool R_LockIndexBuffer(IndexBuffer &indexBuffer, uint32_t, uint32_t, uint16_t **data, enum R_USAGE)
{
	if (opensFail || indexBuffer._isLocked)
	{
		return false;
	}

	*data = reinterpret_cast<uint16_t *>(blocks[&indexBuffer].bytes.data());
	indexBuffer._isLocked = true;
	return true;
}

bool R_UnlockIndexBuffer(IndexBuffer &indexBuffer)
{
	indexBuffer._isLocked = false;
	return true;
}

bool R_SetIndexBuffer(IndexBuffer &)
{
	return true;
}

VertexDeclaration::VertexDeclaration() : offset(0), declaration(0)
{
}

VertexDeclaration::~VertexDeclaration()
{
}

bool VertexDeclaration::Set()
{
	return true;
}

// Each draw the list asks for is written down here, so a check can look at what it would have drawn
struct DrawnBatch
{
	uint32_t startIndex;
	uint32_t triangles;
	uint32_t corners;
};

static std::vector<DrawnBatch> drawn;
static int commandsRun = 0;

bool R_DrawIndexedPrimitive(uint32_t, uint32_t, uint32_t numVerts, uint32_t startIndex, uint32_t numPrimitives)
{
	DrawnBatch const batch = { startIndex, numPrimitives, numVerts };
	drawn.push_back(batch);
	return true;
}

void R_ClearZBuffer()
{
}

void ChangeZWriteEnable(enum ZWRITE_ENABLE)
{
	commandsRun++;
}

bool R_SetTexture(uint32_t, texID_t)
{
	return true;
}

void ChangeTranslucencyMode(enum TRANSLUCENCY_TYPE)
{
}

void ChangeFilteringMode(uint32_t, enum FILTERING_MODE_ID)
{
}

void ChangeTextureAddressMode(uint32_t, enum TEXTURE_ADDRESS_MODE)
{
}

// What the list said in the log, kept so a check can count the lines
static std::vector<std::string> logLines;

void LogString(std::string const &logString)
{
	logLines.push_back(logString);
}

void Con_PrintError(std::string const &)
{
}

static int LogLinesContaining(char const *words)
{
	int count = 0;

	for (size_t i = 0; i < logLines.size(); i++)
	{
		if (logLines[i].find(words) != std::string::npos)
		{
			count++;
		}
	}

	return count;
}

// ---- A list with its buffers, and the shapes written into it ------------------------------------------------------

static uint32_t const kMaxVertices = 4096; // these two are the game's own numbers, from win32/d3_func.cpp
static uint32_t const kMaxIndices  = 9216;

struct TestList
{
	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	VertexDeclaration declaration;

	VertexBuffer *vb;
	IndexBuffer *ib;
	VertexDeclaration *decl;

	RenderList list;

	unsigned char *corners;
	uint32_t cornerSize;
	uint16_t *indices;
	uint32_t written; // which corner comes next, like orthoVBOffset, pVb and the others in the game

	TestList(char const *name, size_t items, uint32_t cornerRoom, uint32_t indexRoom, uint32_t stride):
		vb(&vertexBuffer),
		ib(&indexBuffer),
		decl(&declaration),
		list(name, items, &vb, &ib, &decl),
		corners(0),
		cornerSize(stride),
		indices(0),
		written(0)
	{
		vertexBuffer.Create(cornerRoom, stride, USAGE_DYNAMIC);
		indexBuffer.Create(indexRoom, USAGE_DYNAMIC);
	}

	// What LockExecuteBuffer does for each list at the start of a frame and after a flush
	void Open()
	{
		void *memory = 0;
		vertexBuffer.Lock(&memory);
		corners = static_cast<unsigned char *>(memory);
		indexBuffer.Lock(&indices);
		list.Reset();
		written = 0;
	}

	// What UnlockExecuteBufferAndPrepareForUse does
	void Close()
	{
		vertexBuffer.Unlock();
		indexBuffer.Unlock();
	}

	bool NothingPastTheEnd()
	{
		return PatternUntouched(&vertexBuffer) && PatternUntouched(&indexBuffer);
	}

	void WriteCorners(uint32_t howMany)
	{
		for (uint32_t i = 0; i < howMany; i++)
		{
			memset(corners + static_cast<size_t>(written) * cornerSize, 0x11, cornerSize);
			written++;
		}
	}
};

// One flat rectangle, in the order DrawFontQuad and the other screen writers do it
static bool WriteScreenQuad(TestList &t, texID_t picture)
{
	if (!t.list.AddItem(4, picture, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_CLAMP))
	{
		return false;
	}

	t.WriteCorners(4);
	t.list.CreateOrthoIndices(t.indices);
	return true;
}

// One shape with any number of corners, in the order D3D_Decal_Output and D3D_Particle_Output do it
static bool WriteShape(TestList &t, uint32_t corners, texID_t picture)
{
	if (!t.list.AddItem(corners, picture, TRANSLUCENCY_NORMAL))
	{
		return false;
	}

	t.WriteCorners(corners);
	t.list.CreateIndices(t.indices, corners);
	return true;
}

// ---- The checks -------------------------------------------------------------------------------------------------------

// The room check trusts GetNumIndices to say how many index numbers the writing will use
static void CheckTheCountsAgree()
{
	printf("The number of index numbers a shape is said to need:\n");

	for (uint32_t corners = 3; corners <= 8; corners++)
	{
		TestList t("counts", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const added = WriteShape(t, corners, 1);
		Check(added && t.indexBuffer.GetSize() == GetNumIndices(corners), std::to_string(corners) + " corners write exactly as many index numbers as GetNumIndices says");
		t.Close();
	}

	{
		TestList t("counts", 10, 1000, 2000, sizeof(ORTHOVERTEX));
		t.Open();
		bool added = t.list.AddItem(256, 1, TRANSLUCENCY_NORMAL);

		// the two triangles for each of the 15 x 15 squares of the water and molten metal meshes
		for (int square = 0; added && square < 15 * 15; square++)
		{
			t.list.AddTriangle(t.indices, 0, 1, 16, 256);
			t.list.AddTriangle(t.indices, 1, 17, 16, 256);
		}

		Check(added && t.indexBuffer.GetSize() == GetNumIndices(256), "the 256-corner mesh writes exactly as many index numbers as GetNumIndices says");
		t.Close();
	}

	{
		TestList t("counts", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const added = WriteShape(t, 9, 1);
		Check(added && t.indexBuffer.GetSize() == 0, "a shape with a size CreateIndices doesn't know writes no index numbers");
		t.Close();
	}
}

// Each of the game's four kinds of buffer, at its real size, filled with rectangles until the list says no
static void CheckFillingEveryList()
{
	printf("Filling the game's lists at their real sizes:\n");

	struct Size
	{
		char const *name;
		size_t items;
		uint32_t cornerRoom;
		uint32_t indexRoom;
		uint32_t stride;
		uint32_t rectanglesThatFit;
	};

	Size const sizes[] =
	{
		{ "orthoList",    50,  kMaxVertices,     kMaxIndices * 3,     sizeof(ORTHOVERTEX),    1024 },
		{ "particleList", 200, kMaxVertices * 6, kMaxIndices * 6 * 3, sizeof(PARTICLEVERTEX), 6144 },
		{ "decalList",    200, 8192 * 2,         12288 * 2,           sizeof(DECAL_VERTEX),   4096 },
		{ "mainList",     400, kMaxVertices * 5, kMaxIndices * 5 * 3, sizeof(D3DLVERTEX),     5120 },
	};

	for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
	{
		std::string const name = sizes[s].name;
		TestList t(sizes[s].name, sizes[s].items, sizes[s].cornerRoom, sizes[s].indexRoom, sizes[s].stride);
		t.Open();

		// Five more than fit, so a list that says yes too often writes well past its end
		uint32_t accepted = 0;
		uint32_t const tries = sizes[s].rectanglesThatFit + 5;

		for (uint32_t i = 0; i < tries; i++)
		{
			if (WriteScreenQuad(t, 1))
			{
				accepted++;
			}
		}

		Check(accepted == sizes[s].rectanglesThatFit, name + " takes exactly the rectangles that fit, " + std::to_string(accepted));
		Check(t.list.GetLeftOutBecauseFull() == 5, name + " counts the 5 it refused as full");
		Check(t.list.GetLeftOutBecauseClosed() == 0, name + " counts none of them as closed");
		Check(t.vertexBuffer.GetSize() <= t.vertexBuffer.GetCapacity(), name + " says it holds no more corners than it has room for");
		Check(t.indexBuffer.GetSize() <= t.indexBuffer.GetCapacity(), name + " says it holds no more index numbers than it has room for");
		Check(t.NothingPastTheEnd(), name + " had nothing written past the end of its buffers");
		t.Close();
	}
}

// Running out of index numbers has to stop a shape just as running out of corners does
static void CheckIndexRoomCounts()
{
	printf("When index numbers run out first:\n");

	{
		TestList t("indexes", 10, kMaxVertices, 12, sizeof(ORTHOVERTEX));
		t.Open();
		bool const first = WriteScreenQuad(t, 1);
		bool const second = WriteScreenQuad(t, 1);
		bool const third = WriteScreenQuad(t, 1);
		Check(first && second && !third, "with room for 12 index numbers, two rectangles fit and a third doesn't");
		Check(t.indexBuffer.GetSize() == 12, "the two rectangles used all 12");
		Check(t.vertexBuffer.GetSize() == 8, "the third one saved no corners");
		Check(t.NothingPastTheEnd(), "nothing was written past the end");
		t.Close();
	}

	{
		TestList t("indexes", 10, kMaxVertices, 11, sizeof(ORTHOVERTEX));
		t.Open();
		bool const first = WriteScreenQuad(t, 1);
		bool const second = WriteScreenQuad(t, 1);
		Check(first && !second, "with room for 11 index numbers, one rectangle fits and the second doesn't");
		Check(t.NothingPastTheEnd(), "nothing was written past the end");
		t.Close();
	}

	{
		// Five-cornered shapes use up decalList's index numbers before its corners: 24,576 / 9 is 2,730
		TestList t("decals", 200, 8192 * 2, 12288 * 2, sizeof(DECAL_VERTEX));
		t.Open();
		uint32_t accepted = 0;

		for (uint32_t i = 0; i < 3000; i++)
		{
			if (WriteShape(t, 5, 1))
			{
				accepted++;
			}
		}

		Check(accepted == 2730, "decalList takes exactly 2,730 five-cornered shapes, " + std::to_string(accepted));
		Check(t.NothingPastTheEnd(), "nothing was written past the end");
		t.Close();
	}
}

// Shapes of mixed sizes, to reach every way the room can run out, in a fixed order so a failure can be repeated
static void CheckMixedShapes()
{
	printf("Filling with shapes of mixed sizes:\n");

	bool agreed = true;
	bool inside = true;
	uint32_t refusals = 0;

	for (uint32_t round = 0; round < 20; round++)
	{
		TestList t("mixed", 2, 500 + round * 37, 700 + round * 53, sizeof(DECAL_VERTEX));
		t.Open();
		uint32_t state = 12345 + round;

		for (uint32_t i = 0; i < 1000; i++)
		{
			state = state * 1103515245 + 12345;
			uint32_t const corners = 3 + ((state >> 16) % 6);

			// What Check says must be what AddItem does, or the main list's "make room first" would be wrong
			bool const saidYes = t.list.Check(corners);
			bool const didIt = WriteShape(t, corners, (state >> 8) & 3);

			agreed = agreed && (saidYes == didIt);
			refusals += didIt ? 0 : 1;
			inside = inside && (t.vertexBuffer.GetSize() <= t.vertexBuffer.GetCapacity()) && (t.indexBuffer.GetSize() <= t.indexBuffer.GetCapacity());
		}

		inside = inside && t.NothingPastTheEnd();
		t.Close();
	}

	Check(refusals > 0, "the lists did fill up, with " + std::to_string(refusals) + " shapes refused");
	Check(agreed, "Check and AddItem gave the same answer every time");
	Check(inside, "no list went past the end of its buffers");
}

// A count too large to add up must not wrap round to a small number and get a yes
static void CheckHugeCounts()
{
	printf("A shape claiming a huge number of corners:\n");

	TestList t("huge", 10, 100, 100, sizeof(ORTHOVERTEX));
	t.Open();
	bool const hasSome = WriteScreenQuad(t, 1);
	uint32_t const huge[] = { 0xFFFFFFFFu, 0xFFFFFFFCu, 0x80000000u, 101u };

	for (size_t i = 0; i < sizeof(huge) / sizeof(huge[0]); i++)
	{
		uint32_t const corners = huge[i];
		Check(hasSome && !t.list.Check(corners), "there's no room for " + std::to_string(corners) + " corners");
		Check(!t.list.AddItem(corners, 1, TRANSLUCENCY_NORMAL), "the list refuses " + std::to_string(corners) + " corners");
	}

	Check(t.vertexBuffer.GetSize() == 4, "the refused shapes saved no corners");
	Check(t.list.GetLeftOutBecauseFull() == 4, "the 4 refusals are counted as full");
	t.Close();
}

// Writing to a closed buffer is the bug this test exists for
static void CheckClosedLists()
{
	printf("A list whose buffers are closed:\n");

	{
		TestList t("closed", 10, 100, 100, sizeof(ORTHOVERTEX));
		Check(!t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL), "a list that has never been opened says no");
		Check(t.list.GetLeftOutBecauseClosed() == 1, "and counts it as closed");
		Check(t.list.GetLeftOutBecauseFull() == 0, "not as full");
		Check(t.vertexBuffer.GetSize() == 0 && t.indexBuffer.GetSize() == 0 && t.list.GetSize() == 0, "and has saved nothing");
	}

	{
		TestList t("closed", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const whileOpen = WriteScreenQuad(t, 1);
		t.Close();
		Check(whileOpen, "an open list says yes");
		Check(!WriteScreenQuad(t, 1), "the same list says no once it's closed, which is where particles and coronas used to be written");
		Check(t.list.GetLeftOutBecauseClosed() == 1, "and counts it as closed");
		Check(t.vertexBuffer.GetSize() == 4 && t.list.GetSize() == 1, "and has saved nothing more");
		t.Open();
		Check(WriteScreenQuad(t, 1), "and says yes again once it's opened");
		t.Close();
	}

	{
		TestList t("closed", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		t.vertexBuffer.Unlock();
		Check(!t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL), "the list says no with only its corners closed");
		Check(t.list.GetLeftOutBecauseClosed() == 1, "and counts it as closed");
		t.Close();
	}

	{
		TestList t("closed", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		t.indexBuffer.Unlock();
		Check(!t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL), "the list says no with only its index numbers closed");
		Check(t.list.GetLeftOutBecauseClosed() == 1, "and counts it as closed");
		t.Close();
	}

	{
		TestList t("closed", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		t.Close();
		opensFail = true;
		t.Open();
		opensFail = false;
		Check(!t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL), "the list says no when opening its buffers failed");
		Check(t.list.GetLeftOutBecauseClosed() == 1, "and counts it as closed");
	}
}

// The next frame must have all its room back
static void CheckTheNextFrame()
{
	printf("The frame after a full one:\n");

	TestList t("frames", 10, 8, 100, sizeof(ORTHOVERTEX));
	t.Open();
	bool const first = WriteScreenQuad(t, 1) && WriteScreenQuad(t, 1);
	bool const third = WriteScreenQuad(t, 1);
	Check(first && !third, "the first frame fits two rectangles and refuses a third");
	t.Close();
	t.Open();
	Check(WriteScreenQuad(t, 1) && WriteScreenQuad(t, 1), "the next frame has room for two again");
	Check(t.list.GetLeftOutBecauseFull() == 1, "and the count of what's been left out stays, for the log and for a test");
	Check(t.NothingPastTheEnd(), "nothing was written past the end");
	t.Close();
}

// A command is an entry in the list that isn't a shape, like "stop writing depth". Coronas and particles each add two.
static void CheckCommands()
{
	printf("Commands at the end of the list:\n");

	{
		TestList t("commands", 2, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool inside = true;

		for (int i = 0; i < 40; i++)
		{
			t.list.AddCommand(kCommandZWriteDisable);
			inside = inside && (t.list.GetSize() <= t.list.GetItemRoom());
		}

		Check(inside, "40 commands in a row never run past the end of a list made with room for 2");
		Check(t.list.GetSize() == 40, "all 40 are there");
		commandsRun = 0;
		t.list.Draw();
		Check(commandsRun == 40, "and all 40 are carried out when the list is drawn");
		t.Close();
	}

	{
		TestList t("commands", 2, 400, 800, sizeof(ORTHOVERTEX));
		t.Open();
		bool inside = true;

		for (int i = 0; i < 30; i++)
		{
			inside = inside && WriteScreenQuad(t, static_cast<texID_t>(1 + (i & 1)));
			inside = inside && (t.list.GetSize() <= t.list.GetItemRoom());
			t.list.AddCommand(kCommandZWriteDisable);
			inside = inside && (t.list.GetSize() <= t.list.GetItemRoom());
			t.list.AddCommand(kCommandZWriteEnable);
			inside = inside && (t.list.GetSize() <= t.list.GetItemRoom());
		}

		Check(inside, "shapes and pairs of commands in turn never run past the end of the list");
		Check(t.list.GetSize() == 90, "there are 30 shapes and 60 commands");
		t.Close();
	}
}

// What the list draws afterwards has to be exactly what was written
static void CheckWhatGetsDrawn()
{
	printf("What a list draws after it has refused something:\n");

	{
		TestList t("drawn", 2, kMaxVertices, kMaxIndices * 3, sizeof(ORTHOVERTEX));
		t.Open();
		uint32_t accepted = 0;

		for (uint32_t i = 0; i < 1100; i++)
		{
			// a different picture each time, so the list can't join them into one batch
			accepted += WriteScreenQuad(t, 1 + (i & 1)) ? 1 : 0;
		}

		t.Close();
		drawn.clear();
		t.list.Draw();

		uint32_t triangles = 0;
		bool inside = true;

		for (size_t i = 0; i < drawn.size(); i++)
		{
			triangles += drawn[i].triangles;
			inside = inside && (drawn[i].startIndex + drawn[i].triangles * 3 <= t.indexBuffer.GetSize());
			inside = inside && (drawn[i].corners <= t.vertexBuffer.GetCapacity());
		}

		Check(accepted == 1024, "1024 rectangles fit");
		Check(drawn.size() == 1024, "they draw as 1024 batches");
		Check(triangles == 2048, "of two triangles each");
		Check(inside, "and no batch reaches past what was written");
	}

	{
		// A shape that's too big is refused, and a smaller one after it still fits and draws correctly
		TestList t("drawn", 10, 7, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const quad = WriteShape(t, 4, 1);
		bool const tooBig = WriteShape(t, 4, 2);
		bool const triangle = WriteShape(t, 3, 2);
		t.Close();
		drawn.clear();
		t.list.Draw();

		Check(quad && !tooBig && triangle, "after a rectangle, a second is refused and a triangle still fits");
		Check(drawn.size() == 2, "they draw as two batches");
		Check(drawn.size() == 2 && drawn[0].startIndex == 0 && drawn[0].triangles == 2, "the first is the rectangle, two triangles");
		Check(drawn.size() == 2 && drawn[1].startIndex == 6 && drawn[1].triangles == 1, "the second is the triangle, starting right after it");
		Check(t.NothingPastTheEnd(), "nothing was written past the end");
	}

	{
		// Shapes that can join into one batch still join, and a refusal in between doesn't upset that
		TestList t("drawn", 10, 7, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const quad = WriteShape(t, 4, 1);
		bool const tooBig = WriteShape(t, 4, 1);
		bool const triangle = WriteShape(t, 3, 1);
		t.Close();
		drawn.clear();
		t.list.Draw();

		Check(quad && !tooBig && triangle, "the same, with shapes that can join");
		Check(drawn.size() == 1 && drawn[0].triangles == 3, "they draw as one batch of three triangles");
	}
}

// main and weapon are two lists sharing one pair of buffers, so room has to be shared too
static void CheckSharedBuffers()
{
	printf("Two lists sharing their buffers, as the main list and the weapon list do:\n");

	TestList t("mainList", 10, 40, 400, sizeof(D3DLVERTEX));
	RenderList weapon("weaponList", 10, &t.vb, &t.ib, &t.decl);
	t.Open();
	uint32_t accepted = 0;

	for (uint32_t i = 0; i < 50; i++)
	{
		RenderList &list = (i & 1) ? weapon : t.list;

		if (list.AddItem(4, 1, TRANSLUCENCY_NORMAL))
		{
			t.WriteCorners(4);
			list.CreateIndices(t.indices, 4);
			accepted++;
		}
	}

	Check(accepted == 10, "10 rectangles fit between them in room for 40 corners");
	Check(t.list.GetLeftOutBecauseFull() + weapon.GetLeftOutBecauseFull() == 40, "the other 40 were refused, whichever list asked");
	Check(t.NothingPastTheEnd(), "nothing was written past the end");
	t.Close();
}

// Rain: 999 drops of 3 corners each, reaching the main list when it's already busy
static void CheckRain()
{
	printf("Rain on a busy frame, at the main list's real size:\n");

	uint32_t const cornerRoom = kMaxVertices * 5;
	uint32_t const busy = 18000;       // what the level has already used in this frame
	uint32_t const raindrops = 999;    // HandleRain(999) in PostLandscapeRendering
	uint32_t const fitting = (cornerRoom - busy) / 3;

	{
		// The old way: rain didn't ask. The list says no now, so nothing runs past the end, but some of the rain is lost.
		TestList t("mainList", 400, cornerRoom, kMaxIndices * 5 * 3, sizeof(D3DLVERTEX));
		t.Open();
		bool level = true;

		for (uint32_t i = 0; i < busy / 4; i++)
		{
			level = level && WriteShape(t, 4, 1 + (i & 1));
		}

		uint32_t drops = 0;

		for (uint32_t i = 0; i < raindrops; i++)
		{
			drops += WriteShape(t, 3, 0) ? 1 : 0;
		}

		Check(level, "the level's shapes fit");
		Check(drops == fitting, "rain that doesn't make room gets " + std::to_string(fitting) + " of its drops in, " + std::to_string(drops));
		Check(t.NothingPastTheEnd(), "and nothing is written past the end of the buffers");
		t.Close();
	}

	{
		// The way D3D_DrawParticle_Rain does it now: when there's no room, draw what's waiting and start the list again
		TestList t("mainList", 400, cornerRoom, kMaxIndices * 5 * 3, sizeof(D3DLVERTEX));
		t.Open();

		for (uint32_t i = 0; i < busy / 4; i++)
		{
			(void)WriteShape(t, 4, 1 + (i & 1));
		}

		uint32_t drops = 0;
		uint32_t flushes = 0;

		for (uint32_t i = 0; i < raindrops; i++)
		{
			if (!t.list.Check(3))
			{
				t.Close();
				t.Open();
				flushes++;
			}

			drops += WriteShape(t, 3, 0) ? 1 : 0;
		}

		Check(drops == raindrops, "rain that makes room gets all " + std::to_string(raindrops) + " drops in, " + std::to_string(drops));
		Check(flushes == 1, "it needed one flush to do that");
		Check(t.list.GetLeftOutBecauseFull() == 0, "and nothing was left out");
		Check(t.NothingPastTheEnd(), "and nothing is written past the end of the buffers");
		t.Close();
	}

	{
		// On a frame with room, rain makes no flush at all, so a frame like that looks just as it always has
		TestList t("mainList", 400, cornerRoom, kMaxIndices * 5 * 3, sizeof(D3DLVERTEX));
		t.Open();
		uint32_t flushes = 0;

		for (uint32_t i = 0; i < raindrops; i++)
		{
			if (!t.list.Check(3))
			{
				flushes++;
			}

			(void)WriteShape(t, 3, 0);
		}

		Check(flushes == 0, "rain on a frame with room causes no flush");
		t.Close();
	}
}

// The log lines: one for each list and reason, and one for each quarter of room used
static void CheckTheLog()
{
	printf("What goes in the log:\n");

	{
		logLines.clear();
		TestList t("loudList", 10, 8, 100, sizeof(ORTHOVERTEX));
		TestList other("otherList", 10, 8, 100, sizeof(ORTHOVERTEX));
		t.Open();
		other.Open();

		for (int i = 0; i < 100; i++)
		{
			(void)WriteScreenQuad(t, 1);
			(void)WriteScreenQuad(other, 1);
		}

		Check(LogLinesContaining("loudList is full") == 1, "a list that refuses 98 shapes says it's full once");
		Check(LogLinesContaining("otherList is full") == 1, "another list says so for itself");
		t.Close();
		(void)WriteScreenQuad(t, 1);
		(void)WriteScreenQuad(t, 1);
		Check(LogLinesContaining("loudList was asked for a shape while it was closed") == 1, "and being closed gets its own single line");
		Check(t.list.GetLeftOutBecauseFull() == 98, "the count of full refusals is still exact");
		Check(t.list.GetLeftOutBecauseClosed() == 2, "and of closed ones");
		other.Close();
	}

	{
		logLines.clear();
		TestList t("reportList", 10, 4096, 27648, sizeof(ORTHOVERTEX));

		t.Open();
		Check(logLines.empty(), "a list opened for the first time says nothing");

		for (int i = 0; i < 200; i++)
		{
			(void)WriteScreenQuad(t, 1);
		}

		t.Close();
		t.Open();
		Check(logLines.empty(), "800 of 4,096 corners, under a quarter, says nothing");

		for (int i = 0; i < 300; i++)
		{
			(void)WriteScreenQuad(t, 1);
		}

		t.Close();
		t.Open();
		Check(LogLinesContaining("reportList reached 1200 of 4096 corners and 1800 of 27648 index numbers") == 1, "1,200 corners, past a quarter, gets one line with the numbers");

		for (int i = 0; i < 300; i++)
		{
			(void)WriteScreenQuad(t, 1);
		}

		t.Close();
		t.Open();
		Check(logLines.size() == 1, "the same quarter again adds no line");

		for (int i = 0; i < 1024; i++)
		{
			(void)WriteScreenQuad(t, 1);
		}

		t.Close();
		t.Open();
		Check(logLines.size() == 2, "a full list adds one more line");
		Check(LogLinesContaining("reportList reached 4096 of 4096 corners") == 1, "and it says how full");
		t.Close();
	}

	{
		// Index numbers can be the fuller of the two, and they count as well
		logLines.clear();
		TestList t("indexReport", 10, 4096, 24, sizeof(ORTHOVERTEX));
		t.Open();
		(void)WriteScreenQuad(t, 1);
		t.Close();
		t.Open();
		Check(LogLinesContaining("indexReport reached 4 of 4096 corners and 6 of 24 index numbers") == 1, "a list a quarter full of index numbers says so, whatever its corners say");
		t.Close();
	}
}

// A new buffer's counts are read before the first frame sets them, so they have to start at zero, not at whatever number
// was lying in that memory. This builds the buffers on memory full of a different number to show which it is.
static void CheckNewBuffersStartEmpty()
{
	printf("A brand new buffer:\n");

	alignas(VertexBuffer) unsigned char vertexMemory[sizeof(VertexBuffer)];
	memset(vertexMemory, 0xCD, sizeof(vertexMemory));
	VertexBuffer *vertexBuffer = new (vertexMemory) VertexBuffer;
	Check(vertexBuffer->GetSize() == 0, "holds no corners");
	vertexBuffer->~VertexBuffer();

	alignas(IndexBuffer) unsigned char indexMemory[sizeof(IndexBuffer)];
	memset(indexMemory, 0xCD, sizeof(indexMemory));
	IndexBuffer *indexBuffer = new (indexMemory) IndexBuffer;
	Check(indexBuffer->GetSize() == 0, "holds no index numbers");
	indexBuffer->~IndexBuffer();
}

int main()
{
	CheckTheCountsAgree();
	CheckFillingEveryList();
	CheckIndexRoomCounts();
	CheckMixedShapes();
	CheckHugeCounts();
	CheckClosedLists();
	CheckTheNextFrame();
	CheckCommands();
	CheckWhatGetsDrawn();
	CheckSharedBuffers();
	CheckRain();
	CheckTheLog();
	CheckNewBuffersStartEmpty();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
