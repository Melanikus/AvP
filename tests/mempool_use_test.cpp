// The test of the level memory's use (mempool.h: LevelMemoryPool_Use, LevelMemoryPool_Words). Written 2026-11-21 by Melanikus (Frost) for the AvP port.
//
// WHY (the owner, 2026-11-21, on the track import: "we calculated the limits based on AVP 1mb chunks and that we had 40mb of space in the RIF (before hitting
// engine crash limit) to put in anim"): a level's data comes out of 40 blocks of 1 MB (mempool.cpp), the level's own shapes, objects and strategies first. In
// Rebellion's 1999 game a request bigger than a block, or a 41st block, is written past the end. The level editor opens a level through the same code, so it can
// say how much of the 40 blocks the level uses, and the track import can keep its tracks to what is left. This tests what is counted.
//
// WHAT THIS TESTS: mempool.cpp itself, with the game's memory calls as malloc and free: the blocks taken, the bytes handed out, what is left in the block in
// use, what went outside the 40 blocks (a request over a block, and the blocks all taken), all back to nothing when the level ends, and the words of its
// log line.

#include "mempool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

// The game's own calls mempool.cpp makes, here without the game: its memory is malloc and free, and the rest does nothing
extern "C" void *AllocMem(size_t size) { return malloc(size ? size : 1); }   // mem3dc.h declares them C
extern "C" void DeallocMem(void *at) { free(at); }
void LogString(const std::string &) {}
void StaticLevel_Release() {}
void ShadowShoot_LevelEnded() {}
void LogErrorString(const std::string &) {}
namespace Util { std::string IntToString(const int value) { return std::to_string(value); } }

static int checksFailed = 0;

static void Check(bool passed, const std::string &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static LevelMemoryPoolUse Now()
{
	LevelMemoryPoolUse use;
	memset(&use, 0xff, sizeof(use));   // every field must be written
	LevelMemoryPool_Use(&use);
	return use;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);
	size_t const block = 1024 * 1024;
	LevelMemoryPoolUse use = Now();

	Check(use.blocksMost == 40 && use.blockBytes == block, "the level's memory is 40 blocks of 1 MB (mempool.cpp)");
	Check(use.blocksTaken == 0 && use.bytesHandedOut == 0 && use.leftInBlock == 0 && use.extraBytes == 0, "before a level, nothing is used");

	PoolAllocateMem(1000);
	use = Now();
	Check(use.blocksTaken == 1 && use.bytesHandedOut == 1000 && use.leftInBlock == block - 1000, "the first request takes a block: 1000 bytes out of it");

	PoolAllocateMem(block - 1000);
	use = Now();
	Check(use.blocksTaken == 1 && use.bytesHandedOut == block && use.leftInBlock == 0, "a request that fills the block exactly stays in it");

	PoolAllocateMem(16);
	use = Now();
	Check(use.blocksTaken == 2 && use.leftInBlock == block - 16, "the next takes a second block");

	PoolAllocateMem(2 * block);
	use = Now();
	Check(use.blocksTaken == 2 && use.extraBytes == 2 * block && use.bytesHandedOut == block + 16 + 2 * block,
		"a request bigger than a block goes outside the 40 (the 1999 game writes it past its block), and is counted there");

	for (int i = 0; i < 38; i++)
	{
		PoolAllocateMem(block);
	}

	use = Now();
	Check(use.blocksTaken == 40 && use.leftInBlock == 0, "38 more whole blocks: all 40 taken, nothing left in the last");

	PoolAllocateMem(64);
	use = Now();
	Check(use.blocksTaken == 40 && use.extraBytes == 2 * block + 64 && use.leftInBlock == 0, "with all 40 taken, the next goes to an extra block outside them (the 1999 game writes a 41st past its list): its 64 bytes counted outside");

	char words[256];
	LevelMemoryPool_Words(&use, words, sizeof(words));
	Check(strcmp(words, "level memory: 40 of 40 blocks taken (1 MB each), 0.00 MB left in the last; 41.00 MB handed out, 2.00 MB of it outside the 40 blocks") == 0,
		std::string("its log line, with what went outside the blocks: ") + words);

	char small[16];
	LevelMemoryPool_Words(&use, small, sizeof(small));
	Check(strlen(small) == sizeof(small) - 1, "in a small space the words are cut, never written past it");

	ClearMemoryPool();
	use = Now();
	Check(use.blocksTaken == 0 && use.bytesHandedOut == 0 && use.leftInBlock == 0 && use.extraBytes == 0, "when the level ends, all of it is back to nothing");

	PoolAllocateMem(300 * 1024);
	use = Now();
	LevelMemoryPool_Words(&use, words, sizeof(words));
	Check(strcmp(words, "level memory: 1 of 40 blocks taken (1 MB each), 0.71 MB left in the last; 0.29 MB handed out, 0.00 MB of it outside the 40 blocks") == 0,
		std::string("and a level that fits: ") + words);
	ClearMemoryPool();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
