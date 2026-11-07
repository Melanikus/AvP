// A test that level chunks give back exactly the memory they took, in the right way.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// C++ has two ways to ask for memory: new, for one thing, and new[], for a list. Each has its own way back: delete and
// delete[]. Mixing them is against the rules of the language. It can pass unnoticed for years, because on many
// compilers a list of plain numbers goes back the same way as a single thing, and then a new compiler or a new kind of
// data turns it into damaged memory.
//
// Some level chunks mixed the two, and one kept two lists and never gave them back. This test gives the game's real
// chunks the bytes a level file would, destroys them, and checks two things: every block came back, and each came back
// the way it was asked for.
//
// HOW IT WORKS
//
// A C++ program may supply its own new and delete, and then all of it uses those. The ones below ask the computer for a
// little more than was wanted and write down, in that extra space, which way the block was asked for. So when a block
// comes back, they can see at once if it came back the wrong way. It needs no game files and opens no file at all.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file with the settings in tests/chunk_memory_test.args, which are the game's own, and
// with the game's real chunk code. The program prints a line for each check, and gives back a non-zero result if any
// check failed.

#include <stdio.h>
#include <stdlib.h>
#include <new>
#include <vector>

#include "chunk.hpp"
#include "strategy_chunk.hpp"
#include "avp_chunk.hpp"

// The chunk code asks for its lists through the game's memory code, which stops the game through these if the computer
// runs out of memory. Here that stops the test, and what it says is the last line of the output.
void avp_MessageBox(const char *message, int)
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

// The memory code tells the game's sounds about memory that's about to go (DeallocMem in our_mem.cpp, since net-fixes). There are no
// sounds in this test, so there's nothing to tell.
void Sound_ForgetAddressesInside(const void *, size_t)
{
}

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

// -----------------------------------------------------------------------------------------------------------------
// Our own new and delete
// -----------------------------------------------------------------------------------------------------------------

enum AskedWith { AskedWithNew = 0x4E455721, AskedWithNewList = 0x4C495354 };

// This sits in front of each block. It's 16 bytes so that the block itself stays lined up the way memory has to be.
struct BlockNote
{
	int askedWith;
	int spare[3];
};

static int blocksOut = 0;           // blocks taken and not yet given back
static int blocksMixedUp = 0;       // blocks given back the wrong way

static void *TakeBlock(size_t size, AskedWith askedWith)
{
	BlockNote *note = (BlockNote *)malloc(sizeof(BlockNote) + size);
	if (!note)
	{
		puts("FAILED: the computer has no memory left for the test");
		exit(1);
	}
	note->askedWith = askedWith;
	blocksOut++;
	return note + 1;
}

static void GiveBackBlock(void *block, AskedWith givenBackWith)
{
	// Deleting nothing is allowed, and does nothing.
	if (!block)
	{
		return;
	}
	BlockNote *note = (BlockNote *)block - 1;
	if (note->askedWith != givenBackWith)
	{
		blocksMixedUp++;
	}
	blocksOut--;
	free(note);
}

void *operator new(size_t size) { return TakeBlock(size, AskedWithNew); }
void *operator new[](size_t size) { return TakeBlock(size, AskedWithNewList); }
void operator delete(void *block) noexcept { GiveBackBlock(block, AskedWithNew); }
void operator delete[](void *block) noexcept { GiveBackBlock(block, AskedWithNewList); }
void operator delete(void *block, size_t) noexcept { GiveBackBlock(block, AskedWithNew); }
void operator delete[](void *block, size_t) noexcept { GiveBackBlock(block, AskedWithNewList); }

// -----------------------------------------------------------------------------------------------------------------
// The checks
// -----------------------------------------------------------------------------------------------------------------

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("%s: %s\n", passed ? "ok" : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

// Stands in for a level file: a chunk has to have a parent.
class TestFile : public Chunk_With_Children
{
public:
	TestFile() : Chunk_With_Children(0, "REBINFF2") {}
};

// Everything in these chunks is a whole number, so the test writes its level data as a list of them.
typedef std::vector<int> Bytes;

// The start of every strategy chunk: two spare numbers, the kind of strategy, then its two fixed values.
static Bytes StrategyStart(int kind)
{
	Bytes bytes = { 0, 0, kind, 0, 0 };
	return bytes;
}

// Runs one chunk from start to finish, and checks the books.
static void CheckChunk(TestFile &file, Bytes const &bytes, const char *blocksBack, const char *rightWay)
{
	int const outBefore = blocksOut, mixedUpBefore = blocksMixedUp;

	AVP_Strategy_Chunk *chunk = new AVP_Strategy_Chunk(&file, (const char *)bytes.data(), bytes.size() * sizeof(int));
	Check(chunk->Strategy != 0 && blocksOut > outBefore, "the chunk took memory");
	delete chunk;

	Check(blocksOut == outBefore, blocksBack);
	Check(blocksMixedUp == mixedUpBefore, rightWay);
}

static void TestMultiSwitch(TestFile &file)
{
	Bytes bytes = StrategyStart(StratMultiSwitch);
	int const fixedPart[] = { 0, 0, 0, 0 };                     // rest state, mode, time, security
	bytes.insert(bytes.end(), fixedPart, fixedPart + 4);
	int const targets[] = { 2, 1, 1, 0, 2, 2, 1 };              // two targets: an ID in two numbers, and a request
	bytes.insert(bytes.end(), targets, targets + 7);
	int const links[] = { 1, 3, 3 };                            // one linked switch: an ID in two numbers
	bytes.insert(bytes.end(), links, links + 3);
	bytes.push_back(0);                                         // spare
	bytes.push_back(MultiSwitchFlag_SwitchUpdated);             // flags

	CheckChunk(file, bytes, "a multi-switch gives back all its memory", "a multi-switch gives its lists back with delete[]");
}

static void TestMissionObjective(TestFile &file)
{
	Bytes bytes = StrategyStart(StratMissionObjective);
	int const fixedPart[] = { 0, 0, 0, 0, 0 };                  // two texts, mission number, flags, completion effect
	bytes.insert(bytes.end(), fixedPart, fixedPart + 5);
	int const missions[] = { 2, 1, 1, 0, 2, 2, 0 };             // two missions: an ID in two numbers, and an effect
	bytes.insert(bytes.end(), missions, missions + 7);
	int const requests[] = { 1, 3, 3, 1 };                      // one request: an ID in two numbers, and a request
	bytes.insert(bytes.end(), requests, requests + 4);
	bytes.push_back(0);                                         // spare
	bytes.push_back(0);                                         // spare

	CheckChunk(file, bytes, "a mission objective gives back all its memory", "a mission objective gives its lists back with delete[]");
}

static void TestHierarchy(TestFile &file)
{
	Bytes bytes = StrategyStart(StratHierarchy);
	bytes.push_back(2);                                         // two point effects
	int const first[] = { 0, 0, 1, 1, 1, 0, 0 };                // point, spare, one target: ID, request, flags
	bytes.insert(bytes.end(), first, first + 7);
	int const second[] = { 1, 0, 0 };                           // point, spare, no targets
	bytes.insert(bytes.end(), second, second + 3);
	bytes.push_back(0);                                         // spare
	bytes.push_back(0);                                         // spare

	CheckChunk(file, bytes, "a hierarchy gives back all its memory", "a hierarchy gives its list back with delete[]");
}

// The settings chunk can be read from a file, or made from nothing. Both must end the same way.
static void TestEnvironmentSettings(TestFile &file)
{
	int const outBefore = blocksOut, mixedUpBefore = blocksMixedUp;

	int const saved[] = { 16, 10, 20, 30 };                     // the size of this data, then the sky's three colours
	AVP_Environment_Settings_Chunk *read = new AVP_Environment_Settings_Chunk(&file, (const char *)saved, sizeof(saved));
	Check(read->settings != 0 && read->settings->sky_colour_green == 20, "settings read from a level keep their values");
	delete read;
	Check(blocksOut == outBefore, "settings read from a level give back all their memory");
	Check(blocksMixedUp == mixedUpBefore, "and give it back the way it was asked for");

	AVP_Environment_Settings_Chunk *made = new AVP_Environment_Settings_Chunk(&file);
	Check(made->settings != 0 && made->settings->sky_colour_red == 200, "settings made from nothing start with the default sky");
	delete made;
	Check(blocksOut == outBefore, "settings made from nothing give back all their memory");
	Check(blocksMixedUp == mixedUpBefore, "and give it back the way it was asked for");
}

// The test has to be able to see a mistake, or a pass means nothing.
static void TestTheBookkeeping()
{
	int const outBefore = blocksOut, mixedUpBefore = blocksMixedUp;

	int *list = new int[4];
	Check(blocksOut == outBefore + 1, "the books see a block go out");
	delete list;
	Check(blocksOut == outBefore, "and see it come back");
	Check(blocksMixedUp == mixedUpBefore + 1, "and see a list come back the wrong way");

	blocksMixedUp = mixedUpBefore;
}

int main()
{
	TestTheBookkeeping();

	{
		TestFile file;
		TestMultiSwitch(file);
		TestMissionObjective(file);
		TestHierarchy(file);
		TestEnvironmentSettings(file);
	}

	if (checksFailed)
	{
		printf("The chunk memory test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The chunk memory test passed");
	return 0;
}
