// The test for tools/save_mutate/save_mutate.h: the tool that breaks copies of a saved game, one thing in each, so the game's loader can be
// tried on them (SAVE-HOSTILE-1, step 1). Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game and reads no
// game file: it makes a small save of its own, laid out as the game lays one out (savegame.cpp), packed with the game's own compressor.
//
//      the level's header (28 bytes, plain)   then, packed:   [Strategy block, 24 bytes] [MiscGlobal block, 16] [Particles block, 12]
//
// Each copy the tool makes is unpacked here with the game's own checked decoder and set beside the save it came from, so each check shows
// the copy has exactly the change its name says, and nothing else.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "custom_map_name.h"
#include "huffman.hpp"
#include "save_mutate.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The compressor asks for its memory through AllocMem, the game's own request. The real one brings most of the game with it, so this plain
// stand-in takes its place, as in huffman_test, and stops the test if there's none.
extern "C" void *AllocMem(size_t size)
{
	void *memory = malloc(size);

	if (!memory)
	{
		printf("  FAILED  the test ran out of memory\n");
		exit(1);
	}

	return memory;
}

static void PutInt(std::vector<uint8_t> *bytes, int64_t value)
{
	uint8_t four[4];
	SaveMutate_WriteInt(four, value);
	bytes->insert(bytes->end(), four, four + 4);
}

// A block: its type, its size (the whole block's, header included), and then these numbers
static std::vector<uint8_t> Block(int type, std::vector<int> const &numbers)
{
	std::vector<uint8_t> block;
	PutInt(&block, type);
	PutInt(&block, kSaveMutateHeaderSize + 4 * static_cast<int>(numbers.size()));

	for (size_t k = 0; k < numbers.size(); k++)
	{
		PutInt(&block, numbers[k]);
	}

	return block;
}

// The level's header: a marine's save (species 0), episode 2, 1 hour 5 minutes 7 seconds in, on difficulty 1, 3 saves left
static std::vector<uint8_t> LevelHeader()
{
	std::vector<uint8_t> header;
	PutInt(&header, 0);
	PutInt(&header, kSaveMutateLevelBlockSize);
	char const mark[8] = { 'A', 'V', 'P', 'S', 'A', 'V', 'E', '0' };
	header.insert(header.end(), mark, mark + 8);
	uint8_t const four[4] = { 0, 2, 1, 5 };      // species, episode, hours, minutes
	header.insert(header.end(), four, four + 4);
	PutInt(&header, 7);                            // seconds
	uint8_t const rest[4] = { 1, 3, 0, 0 };       // difficulty, saves left, and two bytes of padding
	header.insert(header.end(), rest, rest + 4);
	return header;
}

static std::vector<uint8_t> Blocks()
{
	std::vector<uint8_t> blocks = Block(2, { 11, 22, 33, 44 });
	std::vector<uint8_t> const misc = Block(10, { 5, 6 }), particles = Block(12, { 9 });
	blocks.insert(blocks.end(), misc.begin(), misc.end());
	blocks.insert(blocks.end(), particles.begin(), particles.end());
	return blocks;
}

// What a save file holds, unpacked plainly, with no walk of its blocks: for setting a copy beside the original
static bool Unpack(std::vector<uint8_t> const &file, std::vector<uint8_t> *header, std::vector<uint8_t> *blocks)
{
	size_t const headerSize = kSaveMutateLevelBlockSize;

	if (file.size() < headerSize + sizeof(HuffmanPackage))
	{
		return false;
	}

	header->assign(file.begin(), file.begin() + headerSize);
	char *unpacked = HuffmanDecompressChecked(reinterpret_cast<const HuffmanPackage *>(&file[headerSize]), static_cast<int>(file.size() - headerSize));

	if (!unpacked)
	{
		return false;
	}

	HuffmanPackage package;
	memcpy(&package, &file[headerSize], sizeof(package));
	blocks->assign(unpacked, unpacked + package.UncompressedDataSize);
	free(unpacked);
	return true;
}

static void Opening()
{
	printf("Opening a save\n");
	std::vector<uint8_t> file;
	Check(SaveMutate_Pack(LevelHeader(), Blocks(), &file), "a save is packed with the game's own compressor");
	SaveMutateSave save;
	Check(SaveMutate_Open(file, &save) == nullptr, "and opens again");
	Check(save.header == LevelHeader() && save.blocks == Blocks(), "giving back the same header and the same blocks");
	Check(save.list.size() == 3 && save.list[0].type == 2 && save.list[0].size == 24 && save.list[1].type == 10 && save.list[1].size == 16 &&
		save.list[2].type == 12 && save.list[2].size == 12 && save.list[1].at == 24 && save.list[2].at == 40,
		"three blocks, walked as the loader walks them: their kinds, sizes and places");

	// Saves it refuses, each with its reason
	std::vector<uint8_t> shortFile(file.begin(), file.begin() + 6);
	Check(SaveMutate_Open(shortFile, &save) != nullptr && std::string(SaveMutate_Open(shortFile, &save)) == "the file is too short to hold the level's header",
		"a file too short for a header is refused");
	std::vector<uint8_t> badSize(file);
	SaveMutate_WriteInt(&badSize[4], 9);
	Check(std::string(SaveMutate_Open(badSize, &save)) == "the level's header has a size that doesn't fit the file", "a header too small to be one is refused");
	SaveMutate_WriteInt(&badSize[4], static_cast<int64_t>(file.size()) + 1);
	Check(std::string(SaveMutate_Open(badSize, &save)) == "the level's header has a size that doesn't fit the file", "a header past the file's end is refused");
	std::vector<uint8_t> notHeader(file);
	SaveMutate_WriteInt(&notHeader[0], 3);
	Check(std::string(SaveMutate_Open(notHeader, &save)) == "the file doesn't start with a level's header", "a file that starts with another block is refused");
	std::vector<uint8_t> notPackage(file);
	notPackage[kSaveMutateLevelBlockSize] = 'X';
	Check(std::string(SaveMutate_Open(notPackage, &save)) == "the rest of the file isn't a Huffman package", "a file with no package after its header is refused");
	// A package cut to 4 bytes of packed data. One byte off the end isn't enough: the decoder reads the packed data 4 bytes at a time and
	// leaves a part word at the end, so a copy one byte short can still unpack. 4 bytes hold 32 bits, and the blocks unpack to 52 bytes,
	// so the decoder refuses it whatever the code lengths are (it can't be more bytes than the bits it was given).
	std::vector<uint8_t> cut(file.begin(), file.begin() + kSaveMutateLevelBlockSize + sizeof(HuffmanPackage) + 4);
	Check(SaveMutate_Open(file, &save) == nullptr && !save.list.empty(), "before it, the whole save opens, so there's something to leave behind");
	Check(std::string(SaveMutate_Open(cut, &save)) == "the package doesn't unpack", "a package cut short doesn't unpack, and is refused");
	Check(save.list.empty() && save.blocks.empty() && save.header.empty(), "a refused save leaves nothing behind");

	// A package whose blocks don't walk: the last block's size one more than what's left
	std::vector<uint8_t> blocks = Blocks();
	SaveMutate_WriteInt(&blocks[40 + 4], 13);
	std::vector<uint8_t> badWalk;
	SaveMutate_Pack(LevelHeader(), blocks, &badWalk);
	Check(std::string(SaveMutate_Open(badWalk, &save)) == "a block's size doesn't fit", "a block whose size runs past the end is refused");
	SaveMutate_WriteInt(&blocks[40 + 4], 7);
	SaveMutate_Pack(LevelHeader(), blocks, &badWalk);
	Check(std::string(SaveMutate_Open(badWalk, &save)) == "a block's size doesn't fit", "and one smaller than its own header");
	std::vector<uint8_t> tooBig(kSaveMutateMostFile + 1, 0);
	Check(std::string(SaveMutate_Open(tooBig, &save)) == "the file is too big to be a save", "a file bigger than any save is refused before it's read");
}

static void Copies()
{
	printf("Broken copies\n");
	std::vector<uint8_t> file;
	SaveMutate_Pack(LevelHeader(), Blocks(), &file);
	SaveMutateSave save;
	SaveMutate_Open(file, &save);
	std::vector<int> const moveTo = { 0, 1, 3 };
	std::vector<SaveMutateChange> const changes = SaveMutate_Changes(save, 145, 4, 5, moveTo);

	// 6 header sizes, 7 header bytes, 3 moves; for each of 3 blocks 7 sizes, 2 types, dropped and doubled, and 2 swaps; 4 words for each of
	// the 3 kinds of block; 5 bytes. 6 + 7 + 3 + 3 x 11 + 2 + 12 + 5 = 68.
	Check(changes.size() == 68, "68 changes for a save of three blocks (" + std::to_string(changes.size()) + ")");

	std::set<std::string> names;
	bool namesSafe = true;

	for (size_t k = 0; k < changes.size(); k++)
	{
		std::string const name = SaveMutate_Name(save, changes[k]);
		names.insert(name);

		for (size_t i = 0; i < name.size(); i++)
		{
			char const c = name[i];
			namesSafe = namesSafe && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-');
		}

		// and the harness reads it: it keeps to the custom map name rule, which also means no more than 39 letters
		namesSafe = namesSafe && name.size() <= kSaveMutateMostName && CustomMapName_IsSafe(name.c_str(), name.size() + 1);
	}

	// Two random picks can land on the same word or byte with the same value, so a name can come twice; the names never say anything but
	// letters, digits and _
	Check(names.size() >= 60, "nearly every change has a name of its own (" + std::to_string(names.size()) + " names for " + std::to_string(changes.size()) + ")");
	Check(namesSafe, "and every name is only letters, digits, - and _, and keeps to the rule the harness reads names by");

	std::vector<SaveMutateChange> const again = SaveMutate_Changes(save, 145, 4, 5, moveTo);
	bool same = again.size() == changes.size();

	for (size_t k = 0; same && k < changes.size(); k++)
	{
		same = again[k].kind == changes[k].kind && again[k].block == changes[k].block && again[k].at == changes[k].at && again[k].value == changes[k].value;
	}

	Check(same, "the same seed makes the same changes");
	std::vector<SaveMutateChange> const other = SaveMutate_Changes(save, 146, 4, 5, moveTo);
	bool differs = false;

	for (size_t k = 0; k < changes.size() && k < other.size(); k++)
	{
		differs = differs || other[k].at != changes[k].at || other[k].value != changes[k].value;
	}

	Check(differs, "and another seed makes other ones");

	// Each copy, unpacked and set beside the original: exactly its own change
	size_t made = 0, right = 0;
	std::vector<uint8_t> const original = Blocks(), originalHeader = LevelHeader();

	for (size_t k = 0; k < changes.size(); k++)
	{
		SaveMutateChange const &c = changes[k];
		std::vector<uint8_t> copy, header, blocks;

		if (!SaveMutate_Make(save, c, &copy) || !Unpack(copy, &header, &blocks))
		{
			continue;
		}

		made++;
		std::vector<uint8_t> wantHeader(originalHeader), want;
		SaveMutateBlock const b = c.block < save.list.size() ? save.list[c.block] : SaveMutateBlock{ 0, 0, 0 };

		switch (c.kind)
		{
		case kSaveMutateBlockSize:
		case kSaveMutateWord:
			want = original;
			SaveMutate_WriteInt(&want[b.at + c.at], c.value);
			break;
		case kSaveMutateBlockType:
			want = original;
			SaveMutate_WriteInt(&want[b.at], c.value);
			break;
		case kSaveMutateDropBlock:
			want.assign(original.begin(), original.begin() + b.at);
			want.insert(want.end(), original.begin() + b.at + b.size, original.end());
			break;
		case kSaveMutateDoubleBlock:
			want.assign(original.begin(), original.begin() + b.at + b.size);
			want.insert(want.end(), original.begin() + b.at, original.end());
			break;
		case kSaveMutateSwapBlock:
		{
			SaveMutateBlock const n = save.list[c.block + 1];
			want.assign(original.begin(), original.begin() + b.at);
			want.insert(want.end(), original.begin() + n.at, original.begin() + n.at + n.size);
			want.insert(want.end(), original.begin() + b.at, original.begin() + b.at + b.size);
			want.insert(want.end(), original.begin() + n.at + n.size, original.end());
			break;
		}
		case kSaveMutateByte:
			want = original;
			want[c.at] = static_cast<uint8_t>(c.value);
			break;
		case kSaveMutateHeaderByte:
		case kSaveMutateMoveTo:
			want = original;
			wantHeader[c.at] = static_cast<uint8_t>(c.value);
			break;
		case kSaveMutateLevelHeaderSize:
			want = original;
			SaveMutate_WriteInt(&wantHeader[4], c.value);
			break;
		}

		right += (header == wantHeader && blocks == want) ? 1 : 0;
	}

	Check(made == changes.size(), "every change makes a copy that unpacks (" + std::to_string(made) + " of " + std::to_string(changes.size()) + ")");
	Check(right == made, "and each copy has exactly its change, and nothing else (" + std::to_string(right) + " of " + std::to_string(made) + ")");

	// A move onto another map changes only the episode
	SaveMutateChange const move = { kSaveMutateMoveTo, 0, kSaveMutateEpisodeAt, 3 };
	std::vector<uint8_t> moved, header, blocks;
	Check(SaveMutate_Make(save, move, &moved) && Unpack(moved, &header, &blocks) && header[kSaveMutateEpisodeAt] == 3 && header[kSaveMutateSpeciesAt] == 0 &&
		blocks == original, "a save moved to episode 3 is the same save with only its episode changed");
	Check(SaveMutate_Name(save, move) == "episode3", "and its name says so");
	SaveMutateChange const size = { kSaveMutateBlockSize, 1, 4, -1 };
	Check(SaveMutate_Name(save, size) == "b1_k10_size_m1", "a block's size set to -1 is named by the block, its kind and the value");

	// A name that would be longer than the harness reads: a name put at 4000000000 in block 4000000000 (kind -1, as there's no such block),
	// from block 4000000000, would be b4000000000_k-1_n4000000000_from_b4000000000, 44 letters
	SaveMutateChange const huge = { kSaveMutateName, 4000000000u, 4000000000u, 4000000000LL };
	std::string const hugeName = SaveMutate_Name(save, huge);
	Check(hugeName.size() == 15 && hugeName.compare(0, 7, "change_") == 0 && CustomMapName_IsSafe(hugeName.c_str(), hugeName.size() + 1) &&
		SaveMutate_Name(save, huge) == hugeName, "a name past 39 letters is change_ and a number, the same each time (" + hugeName + ")");

	// Changes that don't fit this save make no copy
	SaveMutateChange const noBlock = { kSaveMutateDropBlock, 9, 0, 0 }, pastEnd = { kSaveMutateByte, 0, 1000, 1 }, lastSwap = { kSaveMutateSwapBlock, 2, 0, 0 };
	// A word just past the first block: still inside the blocks, so even a loader of copies that let it through would write nothing past an end
	SaveMutateChange const wordPast = { kSaveMutateWord, 0, 24, 0 };
	std::vector<uint8_t> none;
	Check(!SaveMutate_Make(save, noBlock, &none) && !SaveMutate_Make(save, pastEnd, &none) && !SaveMutate_Make(save, lastSwap, &none) &&
		!SaveMutate_Make(save, wordPast, &none), "a block that isn't there, a byte past the end, swapping the last block, and a word past its block make no copy");
}

// A strategy block: its behaviour, its own name, and then these bytes
static std::vector<uint8_t> Strategy(int behaviour, const char *name, std::vector<uint8_t> const &rest)
{
	std::vector<uint8_t> block;
	PutInt(&block, kSaveMutateStrategyKind);
	PutInt(&block, static_cast<int64_t>(kSaveMutateHeaderSize + 4 + kSaveMutateNameLength + rest.size()));
	PutInt(&block, behaviour);
	std::string const eight(name, kSaveMutateNameLength);
	block.insert(block.end(), eight.begin(), eight.end());
	block.insert(block.end(), rest.begin(), rest.end());
	return block;
}

static void Names()
{
	printf("Names inside blocks\n");

	// Two strategy blocks: AAAAAAAA, behaviour 7, and BBBBBBBB, behaviour 9, which holds AAAAAAAA as its target. And a block of another
	// kind, big enough to hold a name where a strategy block keeps its own, holding CCCCCCCC and DDDDDDDD, which name nothing in the save.
	std::vector<uint8_t> const four = { 5, 0, 0, 0 };
	std::string const targetName = "AAAAAAAA";
	std::vector<uint8_t> target = { 6, 0, 0, 0 };
	target.insert(target.end(), targetName.begin(), targetName.end());
	std::vector<uint8_t> blocks = Strategy(7, "AAAAAAAA", four);
	std::vector<uint8_t> const second = Strategy(9, "BBBBBBBB", target), other = Block(10, { 0x43434343, 0x43434343, 0x44444444, 0x44444444 });
	blocks.insert(blocks.end(), second.begin(), second.end());
	blocks.insert(blocks.end(), other.begin(), other.end());
	std::vector<uint8_t> file;
	SaveMutateSave save;
	Check(SaveMutate_Pack(LevelHeader(), blocks, &file) && SaveMutate_Open(file, &save) == nullptr && save.list.size() == 3,
		"a save with two strategy blocks opens");

	std::vector<SaveMutateChange> named;
	std::vector<SaveMutateChange> const all = SaveMutate_Changes(save, 145, 0, 0, std::vector<int>());

	for (size_t k = 0; k < all.size(); k++)
	{
		if (all[k].kind == kSaveMutateName)
		{
			named.push_back(all[k]);
		}
	}

	// Each one's own name, at 12, gets the other's; and the target in the second, at 24, gets the second's own, of another behaviour.
	// The third block's CCCCCCCC and DDDDDDDD name nothing in the save, so nothing is put there.
	Check(named.size() == 3, "three names to change (" + std::to_string(named.size()) + ")");
	Check(named.size() == 3 && named[0].block == 0 && named[0].at == 12 && named[0].value == 1 && named[1].block == 1 && named[1].at == 12 &&
		named[1].value == 0 && named[2].block == 1 && named[2].at == 24 && named[2].value == 1,
		"the first's name gets the second's, the second's the first's, and the target in the second gets the second's own");
	Check(named.size() == 3 && SaveMutate_Name(save, named[2]) == "b1_k2_n24_from_b1", "named by the block, the place and where the name comes from");

	bool right = named.size() == 3;

	for (size_t k = 0; k < named.size() && right; k++)
	{
		std::vector<uint8_t> copy, header, unpacked;
		std::vector<uint8_t> want(blocks);
		SaveMutateBlock const into = save.list[named[k].block], from = save.list[static_cast<size_t>(named[k].value)];
		memcpy(&want[into.at + named[k].at], &blocks[from.at + kSaveMutateNameAt], kSaveMutateNameLength);
		right = SaveMutate_Make(save, named[k], &copy) && Unpack(copy, &header, &unpacked) && unpacked == want;
	}

	Check(right, "each copy has the other name in that one place, and nothing else changed");

	SaveMutateChange const notStrategy = { kSaveMutateName, 1, 24, 2 }, pastEnd = { kSaveMutateName, 1, 25, 0 }, noBlock = { kSaveMutateName, 1, 12, 9 };
	std::vector<uint8_t> none;
	Check(!SaveMutate_Make(save, notStrategy, &none) && !SaveMutate_Make(save, pastEnd, &none) && !SaveMutate_Make(save, noBlock, &none),
		"a name from a block that isn't a strategy block, one that wouldn't fit, or from no block makes no copy");
}

int main()
{
	Opening();
	Copies();
	Names();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all save_mutate checks passed\n");
	return 0;
}
