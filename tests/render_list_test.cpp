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
// THE REAL WRITERS
//
// The writers themselves are in d3d_render.cpp, which brings most of the game with it, so the writers run here are copies of
// their shape, and D3D_DrawParticle_Rain's is one of them. The compiler only catches a writer that throws the list's answer
// away. It's fooled by a cast, or by looking at the answer and doing nothing. So this test also reads d3d_render.cpp itself
// as text, and checks every writer is built the right way: that's CheckTheRealWriters at the end. That's a reading, not a run.
// It sees what the code says, and it can only check the rules written there.

#include <fstream>
#include <map>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RenderList.h"
#include "console.h"
#include "logString.h"
#include "texture_relief.h"

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

// RenderList::DrawOpaqueAgain, which draws a list a second time for the shadow work, needs these names to link. It talks
// to the graphics card directly, so this test never calls it, and these are never used.
D3DInfo d3d;
TextureRelief textureRelief;

TextureRelief::TextureRelief(size_t, size_t)
{
}

texID_t TextureRelief::PictureFor(texID_t) const
{
	return 0;
}

r_Texture const &Tex_GetTexture(texID_t)
{
	static r_Texture none = 0;
	return none;
}

void NoteTextureUse(texID_t)
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

	for (uint32_t corners = 3; corners <= 12; corners++)
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

	for (uint32_t corners = 1; corners <= 2; corners++)
	{
		TestList t("counts", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const added = WriteShape(t, corners, 1);
		Check(added && t.indexBuffer.GetSize() == 0 && GetNumIndices(corners) == 0, std::to_string(corners) + " corners aren't a shape, so no index numbers are said to be needed, and none are written");
		t.Close();
	}
}

// Which corners make which triangle, from how the game has always done it up to eight corners. Nine and up carry on the same way.
static void CheckTheTriangles()
{
	printf("Which corners make which triangle:\n");

	struct Expected
	{
		uint32_t corners;
		std::vector<int> triangles;
	};

	Expected const expected[] =
	{
		{ 3, { 0,2,1 } },
		{ 4, { 0,1,2, 0,2,3 } },
		{ 5, { 0,1,4, 1,3,4, 1,2,3 } },
		{ 6, { 0,4,5, 0,3,4, 0,2,3, 0,1,2 } },
		{ 7, { 0,5,6, 0,4,5, 0,3,4, 0,2,3, 0,1,2 } },
		{ 8, { 0,6,7, 0,5,6, 0,4,5, 0,3,4, 0,2,3, 0,1,2 } },
		{ 9, { 0,7,8, 0,6,7, 0,5,6, 0,4,5, 0,3,4, 0,2,3, 0,1,2 } },
	};

	for (size_t e = 0; e < sizeof(expected) / sizeof(expected[0]); e++)
	{
		TestList t("triangles", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();

		// a shape first, so that these corners aren't numbered from zero
		bool const first = WriteShape(t, 4, 1);
		uint32_t const startIndex = t.indexBuffer.GetSize();
		bool const added = WriteShape(t, expected[e].corners, 1);
		bool same = first && added && (t.indexBuffer.GetSize() - startIndex == expected[e].triangles.size());

		for (size_t i = 0; same && i < expected[e].triangles.size(); i++)
		{
			same = t.indices[startIndex + i] == 4 + expected[e].triangles[i];
		}

		Check(same, std::to_string(expected[e].corners) + " corners make the triangles they always have");
		t.Close();
	}

	// A shape with nine corners used to say it needed index numbers, and write none, so that the draw read whatever was already there
	{
		TestList t("nine", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		bool const added = WriteShape(t, 9, 1);
		t.Close();
		drawn.clear();
		t.list.Draw();
		Check(added && drawn.size() == 1 && drawn[0].triangles == 7, "a nine-cornered shape draws its seven triangles");
		Check(drawn.size() == 1 && drawn[0].startIndex + drawn[0].triangles * 3 == t.indexBuffer.GetSize(), "from exactly the index numbers it wrote");
	}
}

// A list's buffers can be gone: they're let go of while the game gets the graphics card back
static void CheckMissingBuffers()
{
	printf("A list whose buffers aren't there:\n");

	TestList t("missing", 10, 100, 100, sizeof(ORTHOVERTEX));
	t.vb = 0;
	t.ib = 0;

	Check(!t.list.Check(4), "it has no room");
	Check(!t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL), "it says no to a shape");
	Check(t.list.GetLeftOutBecauseClosed() == 1, "and counts it as closed");
	Check(t.list.GetSize() == 0, "and saves nothing");
	t.list.CreateOrthoIndices(0);
	t.list.CreateIndices(0, 4);
	t.list.AddTriangle(0, 0, 1, 2, 3);
	t.list.IncrementIndexCount(3);
	t.list.Reset();
	t.list.AddCommand(kCommandZWriteDisable);
	drawn.clear();
	t.list.Draw();
	Check(drawn.empty() && t.list.GetVertexCount() == 0 && t.list.GetIndexCount() == 0, "and everything else it can be asked to do is safe");

	t.vb = &t.vertexBuffer;
	t.ib = &t.indexBuffer;
	t.Open();
	Check(WriteScreenQuad(t, 1), "and when they're back, it carries on");
	t.Close();

	t.decl = 0;
	drawn.clear();
	t.list.Draw();
	Check(drawn.empty(), "and with no description of its corners, it draws nothing");
}

// What's used can be set in several ways, so the room check can't assume it's never past the room
static void CheckSizesPastTheRoom()
{
	printf("A list said to hold more than it has room for:\n");

	{
		TestList t("past", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		t.vertexBuffer.SetSize(101);
		Check(!t.list.Check(1), "too many corners: no room, instead of wrapping round to a lot of room");
		t.vertexBuffer.SetSize(0xFFFFFFF0u);
		Check(!t.list.Check(1), "far too many corners: no room");
		Check(!t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL), "and a shape is refused");
		t.Close();
	}

	{
		TestList t("past", 10, 100, 100, sizeof(ORTHOVERTEX));
		t.Open();
		t.indexBuffer.SetSize(101);
		Check(!t.list.Check(3), "too many index numbers: no room");
		t.list.AddTriangle(t.indices, 0, 1, 2, 3);
		Check(t.indexBuffer.GetSize() == 101 && t.NothingPastTheEnd(), "and no more are written");
		t.Close();
	}
}

// Asking for room for a certain number of each, which is what an overlay in two parts does
static void CheckRoomForTwoParts()
{
	printf("Room for a shape in two parts:\n");

	TestList t("parts", 10, 8, 12, sizeof(ORTHOVERTEX));
	t.Open();
	Check(t.list.Check(8, 12), "two rectangles fit in exactly their room");
	Check(!t.list.Check(9, 12), "one corner more doesn't");
	Check(!t.list.Check(8, 13), "and neither does one index number more");
	Check(WriteScreenQuad(t, 1), "the first goes in");
	Check(!t.list.Check(8, 12), "and then two more don't fit");
	Check(t.list.Check(4, 6), "but one does");
	uint32_t const before = t.list.GetLeftOutBecauseFull();
	logLines.clear();
	t.list.NoteLeftOutBecauseFull();
	t.list.NoteLeftOutBecauseFull();
	Check(t.list.GetLeftOutBecauseFull() == before + 2, "what an overlay leaves out is counted");
	Check(LogLinesContaining("parts is full") == 1, "and said once");
	t.Close();
}

// A mesh only has one AddItem for all its triangles, so nothing stops the triangles but the code writing them
static void CheckTrianglesCantRunOver()
{
	printf("Too many triangles for what was saved:\n");

	TestList t("mesh", 10, 300, 12, sizeof(ORTHOVERTEX));
	t.Open();
	bool const added = t.list.AddItem(4, 1, TRANSLUCENCY_NORMAL);

	for (int i = 0; i < 2000; i++)
	{
		t.list.AddTriangle(t.indices, 0, 1, 2, 4);
	}

	Check(added && t.indexBuffer.GetSize() == 12, "they stop at the end of the room");
	Check(t.NothingPastTheEnd(), "and nothing is written past the end");
	Check(t.list.GetLeftOutBecauseFull() > 0, "and it's counted");
	t.Close();
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

	// Picked so that adding it to what's used would wrap round to almost nothing, in the corners and in the index numbers at once: with 12 index numbers used, this one needs 12 fewer than none
	TestList both("both", 10, 100, 100, sizeof(ORTHOVERTEX));
	both.Open();
	bool const two = WriteScreenQuad(both, 1) && WriteScreenQuad(both, 1);
	Check(two && !both.list.Check(0xFFFFFFFEu), "a count that would wrap both sums round still gets a no");
	both.Close();
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

// ---- Reading the real writers, as text ----------------------------------------------------------------------------------

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	path = path.substr(0, tests) + name;

	std::ifstream file(path.c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

// The code with its comments gone, and what's inside quotes gone, and anything in an #if 0, so none of those can be mistaken for code
static std::string JustTheCode(std::string const &source)
{
	std::string out;
	out.reserve(source.size());

	for (size_t i = 0; i < source.size();)
	{
		char const c = source[i];

		if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
		{
			while (i < source.size() && source[i] != '\n')
			{
				i++;
			}
		}
		else if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
		{
			std::string::size_type const end = source.find("*/", i + 2);
			i = (end == std::string::npos) ? source.size() : end + 2;
		}
		else if (c == '"' || c == '\'')
		{
			out += c;
			i++;

			while (i < source.size() && source[i] != c && source[i] != '\n')
			{
				i += (source[i] == '\\') ? 2 : 1;
			}

			if (i < source.size())
			{
				out += source[i];
				i++;
			}
		}
		else
		{
			out += c;
			i++;
		}
	}

	// The lines between an #if 0 and its #endif, counting any others inside it
	std::stringstream lines(out);
	std::string line, kept;
	int insideIfZero = 0;

	while (std::getline(lines, line))
	{
		std::string::size_type const first = line.find_first_not_of(" \t");
		std::string const trimmed = (first == std::string::npos) ? "" : line.substr(first);

		if (insideIfZero > 0)
		{
			insideIfZero += (trimmed.compare(0, 3, "#if") == 0) ? 1 : (trimmed.compare(0, 6, "#endif") == 0) ? -1 : 0;
			continue;
		}

		if (trimmed.compare(0, 5, "#if 0") == 0)
		{
			insideIfZero = 1;
			continue;
		}

		kept += line;
		kept += '\n';
	}

	return kept;
}

struct SourceFunction
{
	std::string name;
	std::string body;
};

// Splits code at the outermost braces. What comes before an opening brace says what it is, and it's a function if the name is followed by round brackets.
static std::vector<SourceFunction> SplitIntoFunctions(std::string const &code, bool *balanced)
{
	std::vector<SourceFunction> functions;
	int depth = 0;
	size_t headerStart = 0, bodyStart = 0;

	for (size_t i = 0; i < code.size(); i++)
	{
		char const c = code[i];

		if (c == '{')
		{
			if (depth == 0)
			{
				bodyStart = i + 1;
			}
			depth++;
		}
		else if (c == '}')
		{
			depth--;

			if (depth == 0)
			{
				std::string const header = code.substr(headerStart, bodyStart - 1 - headerStart);
				std::string::size_type const bracket = header.find('(');

				if (bracket != std::string::npos)
				{
					std::string::size_type end = bracket;

					while (end > 0 && isspace((unsigned char)header[end - 1]))
					{
						end--;
					}

					std::string::size_type start = end;

					while (start > 0 && (isalnum((unsigned char)header[start - 1]) || header[start - 1] == '_'))
					{
						start--;
					}

					SourceFunction const function = { header.substr(start, end - start), code.substr(bodyStart, i - bodyStart) };
					functions.push_back(function);
				}

				headerStart = i + 1;
			}
		}
		else if (c == ';' && depth == 0)
		{
			headerStart = i + 1;
		}
	}

	*balanced = (depth == 0);
	return functions;
}

static size_t SkipSpace(std::string const &text, size_t at)
{
	while (at < text.size() && isspace((unsigned char)text[at]))
	{
		at++;
	}

	return at;
}

// Where the round bracket that closes the one at this place is
static size_t ClosingBracket(std::string const &text, size_t open)
{
	int depth = 0;

	for (size_t i = open; i < text.size(); i++)
	{
		depth += (text[i] == '(') ? 1 : (text[i] == ')') ? -1 : 0;

		if (depth == 0)
		{
			return i;
		}
	}

	return std::string::npos;
}

static int CountOf(std::string const &text, std::string const &words)
{
	int count = 0;

	for (std::string::size_type at = text.find(words); at != std::string::npos; at = text.find(words, at + words.size()))
	{
		count++;
	}

	return count;
}

// The checks on the source, kept apart from running it, so that a test of the checks themselves can hand them broken copies
struct SourceFindings
{
	std::vector<std::string> addItemNotFollowed;     // writers that don't stop when a list says no
	std::vector<std::string> wrongWayOut;            // those that stop the wrong way
	std::vector<std::string> mainWithoutRoom;        // main list writers that don't make room first
	std::vector<std::string> writesBeforeAsking;     // corners written ahead of asking for room
	std::vector<std::string> wrongCornerCount;       // a different number of corners from what was asked for
	std::vector<std::string> flushOutsideTheHelper;  // places that flush the lists by hand, with no check that the frame is open
	std::vector<std::string> twoPartsUnchecked;      // two-part overlays that don't ask for room for both
	bool writesBeforeClosing;
	bool executeWrites;
	bool flushChecksTheFrame;
	bool frameEndChecksTheFrame;
	bool frameEndAlwaysFinishes;
	bool frameEndForgetsCasters;
	bool frameEndEmptiesTheLists;
	bool frameStartRemembers;
	int writersRead;
	int mainWritersRead;
	bool balanced;
};

static SourceFindings ReadTheWriters(std::string const &source)
{
	SourceFindings found;
	std::string const code = JustTheCode(source);
	std::vector<SourceFunction> const functions = SplitIntoFunctions(code, &found.balanced);

	found.writesBeforeClosing = false;
	found.executeWrites = false;
	found.flushChecksTheFrame = false;
	found.frameEndChecksTheFrame = false;
	found.frameEndAlwaysFinishes = false;
	found.frameEndForgetsCasters = false;
	found.frameStartRemembers = false;
	found.writersRead = 0;
	found.mainWritersRead = 0;

	static char const * const lists[] = { "orthoList", "particleList", "decalList", "mainList", "weaponList" };
	static char const * const stores[] = { "orthoVertex[", "particleVertex[", "decalVertex[", "mainVertex[" };

	for (size_t f = 0; f < functions.size(); f++)
	{
		SourceFunction const &function = functions[f];
		std::string const &body = function.body;

		// Every AddItem has to be the question of an if, and the answer no has to leave
		size_t firstAdd = std::string::npos;
		size_t firstMainAdd = std::string::npos;
		int adds = 0;

		for (size_t l = 0; l < sizeof(lists) / sizeof(lists[0]); l++)
		{
			std::string const call = std::string(lists[l]) + "->AddItem(";

			for (std::string::size_type at = body.find(call); at != std::string::npos; at = body.find(call, at + call.size()))
			{
				adds++;
				firstAdd = (at < firstAdd) ? at : firstAdd;

				if (l >= 3)
				{
					firstMainAdd = (at < firstMainAdd) ? at : firstMainAdd;
				}

				size_t const before = body.find_last_not_of(" \t\r\n", at - 1);
				bool const asked = before != std::string::npos && before >= 4 && body.compare(before - 4, 5, "if (!") == 0;
				size_t const end = ClosingBracket(body, at + call.size() - 1);
				bool stops = false;

				if (asked && end != std::string::npos)
				{
					size_t next = SkipSpace(body, end + 1);

					if (next < body.size() && body[next] == ')')
					{
						next = SkipSpace(body, next + 1);

						if (next < body.size() && body[next] == '{')
						{
							next = SkipSpace(body, next + 1);
						}

						bool const leavesByReturn = body.compare(next, 7, "return;") == 0;
						bool const leavesByContinue = body.compare(next, 9, "continue;") == 0;

						stops = leavesByReturn || leavesByContinue;

						// A corona is in a loop, and the end of its function has to run. Everything else just leaves.
						if ((function.name == "DrawCoronas") != leavesByContinue)
						{
							found.wrongWayOut.push_back(function.name);
						}
					}
				}

				if (!stops)
				{
					found.addItemNotFollowed.push_back(function.name);
				}
			}
		}

		found.writersRead += (adds > 0) ? 1 : 0;

		// Anything adding to the main list has to make room first, the way the level's shapes do
		if (firstMainAdd != std::string::npos)
		{
			found.mainWritersRead++;
			size_t const check = body.find("mainList->Check(");
			size_t const flush = (check == std::string::npos) ? std::string::npos : body.find("FlushLists()", check);

			if (check == std::string::npos || check > firstMainAdd || flush == std::string::npos || flush > firstMainAdd)
			{
				found.mainWithoutRoom.push_back(function.name);
			}
		}

		// No corner can be written until a list has said yes
		for (size_t s = 0; s < sizeof(stores) / sizeof(stores[0]); s++)
		{
			size_t const write = body.find(stores[s]);

			if (write != std::string::npos && (firstAdd == std::string::npos || write < firstAdd))
			{
				found.writesBeforeAsking.push_back(function.name);
			}
		}

		// The screen writers ask for 4 corners, and that's what they have to write. A loop that goes round twice for two parts is checked below.
		bool const twoParts = CountOf(body, "orthoList->AddItem(") == 2;

		if (twoParts && (body.find("orthoList->Check(8, 12)") == std::string::npos || body.find("orthoList->Check(8, 12)") > firstAdd))
		{
			found.twoPartsUnchecked.push_back(function.name);
		}

		if (!twoParts && CountOf(body, "orthoList->AddItem(4,") == 1)
		{
			int const corners = CountOf(body, "orthoVBOffset++") + 4 * CountOf(body, "orthoVBOffset += 4");

			if (corners != 4)
			{
				found.wrongCornerCount.push_back(function.name);
			}
		}

		// Everything has to go through the one place that checks a frame is open, apart from the end of a frame, which checks for itself
		if (body.find("UnlockExecuteBufferAndPrepareForUse()") != std::string::npos && function.name != "FlushLists" && function.name != "ThisFramesRenderingHasFinished")
		{
			found.flushOutsideTheHelper.push_back(function.name);
		}

		if (function.name == "UnlockExecuteBufferAndPrepareForUse")
		{
			size_t const sort1 = body.find("particleList->Sort()");
			size_t const sort2 = body.find("decalList->Sort()");
			size_t const particles = body.find("DrawParticles()");
			size_t const coronas = body.find("DrawCoronas()");
			size_t const close = body.find("->Unlock()");

			found.writesBeforeClosing = sort1 != std::string::npos && sort1 < sort2 && sort2 < particles && particles < coronas && coronas < close && close != std::string::npos;
		}

		if (function.name == "ExecuteBuffer")
		{
			found.executeWrites = body.find("DrawParticles()") != std::string::npos || body.find("DrawCoronas()") != std::string::npos || body.find("->Sort()") != std::string::npos;
		}

		if (function.name == "FlushLists")
		{
			size_t const guard = body.find("if (!frameIsOpen)");
			found.flushChecksTheFrame = guard != std::string::npos && guard < body.find("UnlockExecuteBufferAndPrepareForUse()");
		}

		if (function.name == "ThisFramesRenderingHasFinished")
		{
			size_t const guard = body.find("if (!frameIsOpen)");
			found.frameEndChecksTheFrame = guard != std::string::npos && guard < body.find("UnlockExecuteBufferAndPrepareForUse()");
			found.frameEndAlwaysFinishes = body.find("LightBlockDeallocation()") != std::string::npos && body.find("return") == std::string::npos;
			found.frameEndForgetsCasters = guard != std::string::npos && body.find("ToyForgetCasters();", guard) < body.find("frameIsOpen = false");
			found.frameEndEmptiesTheLists = guard != std::string::npos
				&& guard < body.find("particleList->Reset();") && guard < body.find("mainList->Reset();") && guard < body.find("orthoList->Reset();")
				&& guard < body.find("decalList->Reset();") && guard < body.find("weaponList->Reset();")
				&& body.find("weaponList->Reset();") != std::string::npos && body.find("decalList->Reset();") != std::string::npos
				&& body.find("orthoList->Reset();") != std::string::npos && body.find("mainList->Reset();") != std::string::npos && body.find("particleList->Reset();") != std::string::npos;
		}

		if (function.name == "ThisFramesRenderingHasBegun")
		{
			size_t const begin = body.find("R_BeginScene()");
			size_t const remember = body.find("frameIsOpen = true");
			size_t const open = body.find("LockExecuteBuffer()");
			found.frameStartRemembers = begin != std::string::npos && begin < remember && remember != std::string::npos && remember < open && open != std::string::npos;
		}
	}

	return found;
}

static std::string Names(std::vector<std::string> const &names)
{
	std::string all;

	for (size_t i = 0; i < names.size(); i++)
	{
		all += (i ? ", " : "") + names[i];
	}

	return all;
}

static void CheckTheRealWriters()
{
	printf("Reading the real writers in d3d_render.cpp:\n");

	std::string const source = ReadSourceFile("3dc/d3d_render.cpp");
	Check(source.size() > 100000, "the source file was found and read");

	SourceFindings const found = ReadTheWriters(source);

	Check(found.balanced, "its braces add up, so the functions were read as they are");
	Check(found.writersRead >= 25, "it read " + std::to_string(found.writersRead) + " functions that write shapes");
	Check(found.mainWritersRead >= 6, "of which " + std::to_string(found.mainWritersRead) + " write to the main or weapon list");
	Check(found.addItemNotFollowed.empty(), "every AddItem is asked as a question, and a no leaves at once: " + Names(found.addItemNotFollowed));
	Check(found.wrongWayOut.empty(), "a corona skips to the next one, and every other writer returns: " + Names(found.wrongWayOut));
	Check(found.mainWithoutRoom.empty(), "every main list writer makes room first, rain included: " + Names(found.mainWithoutRoom));
	Check(found.writesBeforeAsking.empty(), "no corner is written before a list has said yes: " + Names(found.writesBeforeAsking));
	Check(found.wrongCornerCount.empty(), "each screen writer writes the 4 corners it asked for: " + Names(found.wrongCornerCount));
	Check(found.twoPartsUnchecked.empty(), "an overlay in two parts asks for room for both first: " + Names(found.twoPartsUnchecked));
	Check(found.flushOutsideTheHelper.empty(), "nothing flushes the lists except through the one place that checks the frame is open: " + Names(found.flushOutsideTheHelper));
	Check(found.writesBeforeClosing, "particles and coronas are written before their buffers close, after the two sorts, in that order");
	Check(!found.executeWrites, "and not afterwards");
	Check(found.flushChecksTheFrame, "a flush checks the frame is open");
	Check(found.frameEndChecksTheFrame, "the end of a frame checks it too");
	Check(found.frameEndAlwaysFinishes, "and a frame that couldn't be drawn still gets to the end of that function, where the lights that are due to go are let go of");
	Check(found.frameEndForgetsCasters, "and it forgets what it collected for shadows, because the shadow pass, which would have, didn't run");
	Check(found.frameEndEmptiesTheLists, "and every list is emptied, because commands still go on them with nothing to draw them");
	Check(found.frameStartRemembers, "and the start of a frame only says it's open if it did start");

	// That these checks can catch anything has to be shown as well. Each of these breaks one writer in a copy of the file.
	struct Broken
	{
		char const *what;
		char const *from;
		char const *to;
		std::vector<std::string> SourceFindings::*list;
		char const *function;
	};

	Broken const broken[] =
	{
		{ "an answer that's looked at and ignored", "if (!decalList->AddItem(RenderPolygon.NumberOfVertices, textureID, decalDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {\n\t\treturn;\n\t}", "if (!decalList->AddItem(RenderPolygon.NumberOfVertices, textureID, decalDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {\n\t}", &SourceFindings::addItemNotFollowed, "D3D_Decal_Output" },
		{ "an answer that's thrown away with a cast", "if (!decalList->AddItem(RenderPolygon.NumberOfVertices, textureID, decalDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP)) {\n\t\treturn;\n\t}", "(void)decalList->AddItem(RenderPolygon.NumberOfVertices, textureID, decalDescPtr->TranslucencyType, FILTERING_BILINEAR_ON, TEXTURE_CLAMP);", &SourceFindings::addItemNotFollowed, "D3D_Decal_Output" },
		{ "a writer that writes one corner too many", "if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {\n\t\treturn;\n\t}", "if (!orthoList->AddItem(4, NO_TEXTURE, TRANSLUCENCY_GLOWING, FILTERING_BILINEAR_ON, TEXTURE_WRAP)) {\n\t\treturn;\n\t}\n\torthoVBOffset++;", &SourceFindings::wrongCornerCount, "DrawFadeQuad" },
		{ "a writer that writes before it asks", "\tif (!particleList->AddItem(4,", "\tparticleVertex[pVb].x = 0.0f;\n\tif (!particleList->AddItem(4,", &SourceFindings::writesBeforeAsking, "D3D_Particle_Output" },
		{ "a corona that returns", "continue; // no room for this one", "return; // no room for this one", &SourceFindings::wrongWayOut, "DrawCoronas" },
		{ "rain that doesn't make room", "if (!mainList->Check(3)) {\n\t\t\tFlushLists();\n\t\t}", "", &SourceFindings::mainWithoutRoom, "D3D_DrawParticle_Rain" },
		{ "a flush by hand", "\t\tif (!mainList->Check(3)) {\n\t\t\tFlushLists();\n\t\t}", "\t\tif (!mainList->Check(3)) {\n\t\t\tUnlockExecuteBufferAndPrepareForUse();\n\t\t\tExecuteBuffer();\n\t\t\tLockExecuteBuffer();\n\t\t}", &SourceFindings::flushOutsideTheHelper, "D3D_DrawParticle_Rain" },
		{ "an overlay that doesn't check for both parts", "// alien overlay\n\tint theta[2];\n\tint colour = 0xffffffff;\n\n\t// It's two rectangles that only look right together, so if both won't fit, neither is drawn\n\tif (!orthoList->Check(8, 12))", "// alien overlay\n\tint theta[2];\n\tint colour = 0xffffffff;\n\n\tif (false)", &SourceFindings::twoPartsUnchecked, "D3D_ScreenInversionOverlay" },
	};

	std::string plain = source;

	for (std::string::size_type at = plain.find("\r\n"); at != std::string::npos; at = plain.find("\r\n", at))
	{
		plain.erase(at, 1);
	}

	for (size_t b = 0; b < sizeof(broken) / sizeof(broken[0]); b++)
	{
		std::string copy = plain;
		std::string::size_type const at = copy.find(broken[b].from);
		Check(at != std::string::npos, std::string("the copy for \"") + broken[b].what + "\" could be made");

		if (at != std::string::npos)
		{
			copy.replace(at, strlen(broken[b].from), broken[b].to);
			SourceFindings const caught = ReadTheWriters(copy);
			bool named = false;

			for (size_t n = 0; n < (caught.*broken[b].list).size(); n++)
			{
				named = named || (caught.*broken[b].list)[n] == broken[b].function;
			}

			Check(named, std::string("and ") + broken[b].what + " is caught, in " + broken[b].function);
		}
	}

	{
		// Writing particles and coronas after the buffers have closed, which is how it used to be
		std::string copy = plain;
		std::string::size_type const unlock = copy.find("static bool UnlockExecuteBufferAndPrepareForUse()");
		std::string::size_type const particles = (unlock == std::string::npos) ? unlock : copy.find("DrawParticles();", unlock);
		std::string::size_type const coronas = (particles == std::string::npos) ? particles : copy.find("DrawCoronas();", particles);
		bool made = coronas != std::string::npos;

		if (made)
		{
			copy.erase(coronas, strlen("DrawCoronas();"));
			copy.erase(particles, strlen("DrawParticles();"));
			std::string::size_type const execute = copy.find("static bool ExecuteBuffer()\n{");
			made = execute != std::string::npos;

			if (made)
			{
				copy.insert(execute + strlen("static bool ExecuteBuffer()\n{"), "\n\tDrawParticles();\n\tDrawCoronas();\n");
			}
		}

		SourceFindings const caught = ReadTheWriters(copy);
		Check(made && !caught.writesBeforeClosing && caught.executeWrites, "and writing particles and coronas after the buffers close is caught");
	}

	// The compiler has to be asked to refuse a thrown away answer, and the list has to ask for that
	std::string const header = JustTheCode(ReadSourceFile("3dc/RenderList.h"));
	Check(header.find("[[nodiscard]] bool AddItem") != std::string::npos, "the list says its answer can't be thrown away");
	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj");
	Check(CountOf(project, "/we4834") >= 2, "and both of the game's builds treat throwing it away as an error");

	// The wait for a lost graphics card can't be run here, because it needs a graphics card, so it's read.
	std::string const device = JustTheCode(ReadSourceFile("win32/d3_func.cpp"));
	bool deviceBalanced = false;
	std::vector<SourceFunction> const deviceFunctions = SplitIntoFunctions(device, &deviceBalanced);
	std::string beginScene;

	for (size_t f = 0; f < deviceFunctions.size(); f++)
	{
		beginScene = (deviceFunctions[f].name == "R_BeginScene") ? deviceFunctions[f].body : beginScene;
	}

	size_t const wait = beginScene.find("while (1)");
	size_t const askAgain = (wait == std::string::npos) ? wait : beginScene.find("TestCooperativeLevel()", wait);
	size_t const makeAgain = beginScene.find("if (!d3d.mainVB)");

	Check(deviceBalanced && wait != std::string::npos, "the wait for a lost graphics card was found");
	Check(askAgain != std::string::npos, "it asks the card again each time round, so it can't wait on an old answer forever");
	Check(wait != std::string::npos && beginScene.find("if (!bRunning)", wait) != std::string::npos, "and it ends if the player closes the game");
	Check(makeAgain != std::string::npos && makeAgain > wait && beginScene.find("BeginScene()", makeAgain) != std::string::npos, "and what it let go of is made again before a frame can start");
	Check(CountOf(beginScene, "CreateVolatileResources()") == 1, "in one place, whichever way the wait ended");

	// The wait goes round twenty times a second, so anything it says has to be said once. Saying something and then leaving the wait is fine too.
	size_t const waitEnd = (wait == std::string::npos) ? wait : beginScene.find("Sleep(50)", wait);
	bool saysEachThingOnce = waitEnd != std::string::npos;
	int thingsSaid = 0;
	char const *const ways[] = { "LogDebugString(", "Con_PrintError(" };

	for (size_t w = 0; saysEachThingOnce && w < sizeof(ways) / sizeof(ways[0]); w++)
	{
		for (size_t at = beginScene.find(ways[w], wait); at != std::string::npos && at < waitEnd; at = beginScene.find(ways[w], at + 1))
		{
			thingsSaid++;

			size_t const lineEnd = beginScene.find(';', at);
			size_t const after = beginScene.find_first_not_of(" \t\r\n", lineEnd + 1);
			bool const leavesStraightAfter = after != std::string::npos && beginScene.compare(after, 6, "break;") == 0;

			// Otherwise it has to be inside "if (!said...)", with nothing closing that between the two, and it has to note that it's been said
			size_t const guard = beginScene.rfind("if (!said", at);
			size_t const blockEnd = beginScene.find('}', at);
			bool const insideGuard = guard != std::string::npos && guard > wait && beginScene.find('}', guard) == blockEnd;
			bool const notesIt = blockEnd != std::string::npos && beginScene.substr(at, blockEnd - at).find("= true") != std::string::npos;

			if (!leavesStraightAfter && !(insideGuard && notesIt))
			{
				saysEachThingOnce = false;
			}
		}
	}

	Check(saysEachThingOnce && thingsSaid >= 4, "and everything it says, it says once, so a long wait doesn't fill the log");

	size_t const clear = beginScene.find("->Clear(");
	size_t const closedAfterClear = (clear == std::string::npos) ? clear : beginScene.find("R_EndScene()", clear);
	size_t const stoppedAfterClear = (clear == std::string::npos) ? clear : beginScene.find("return false", clear);
	Check(closedAfterClear != std::string::npos && stoppedAfterClear != std::string::npos && closedAfterClear < stoppedAfterClear, "and a frame stopped by a failed clear closes the scene it opened, or no frame could ever start again");

	std::string const windows = JustTheCode(ReadSourceFile("3dc/win95/win_func.cpp"));
	Check(windows.find("while (!bActive && bRunning)") != std::string::npos, "and being closed also ends the wait for the game to become active");

	// Nothing else adds to these lists. kshape.cpp has the only other mention, in code that's switched off.
	std::string const shapes = JustTheCode(ReadSourceFile("3dc/kshape.cpp"));
	Check(shapes.size() > 100000 && shapes.find("->AddItem(") == std::string::npos, "and the only other file that uses the lists doesn't add shapes to them");
}

int main()
{
	CheckTheCountsAgree();
	CheckTheTriangles();
	CheckMissingBuffers();
	CheckSizesPastTheRoom();
	CheckRoomForTwoParts();
	CheckTrianglesCantRunOver();
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
	CheckTheRealWriters();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
