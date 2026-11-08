// Breaks copies of a saved game, so the game's loader can be tried on each one: SAVE-HOSTILE-1, step 1. Written 2026-11-08 by Melanikus
// for the AvP port.
//
// WHY. A save file sits in the player's folder, players swap them, and anyone can edit one (AvP Tao #3: input is hostile). The loader
// checks what it reads (SAVE-2), but no save had ever been put through it. This makes the saves to put through it: each copy has ONE
// thing wrong, and is named by what, so if the game falls over on a copy, the copy's name says what did it.
//
// WHAT A SAVE IS (savegame.cpp, SaveGame and LoadSavedGame):
//
//      +--------------------+------------------------------------------------------------------+
//      | the level's header |  one Huffman package ("REBCRIF1"), packed by the game's compressor |
//      |  (a block, plain)  |                                                                   |
//      +--------------------+------------------------------------------------------------------+
//                                     unpacked, it's blocks one after another:
//                                     [type, size, ...] [type, size, ...] [type, size, ...] ...
//
// Every block starts with its type and its size in bytes, the size counting the type and the size themselves. The level's header says
// which species and episode the save is for, its difficulty, and how long it's been played.
//
// HOW A COPY IS BROKEN, one change each:
//   - a block's size: 0, one less than its header, one less, one more, 4 more, the most an int holds, and -1;
//   - a block's type: -1, and one past the last kind there is;
//   - a block dropped, doubled, or swapped with the next one;
//   - numbers inside a block, 4 bytes at a time, at places picked by a seeded random number, so two runs make the same copies: set to
//     0, -1, 1, the most and the least an int holds, and one more and one less than they were. That reaches the counts, list numbers,
//     kinds and behaviours a block holds without this knowing every block's layout;
//   - bytes anywhere in the unpacked blocks, picked the same way;
//   - the level's header: its size, its species and episode out of range, its difficulty out of range, and its "AvP save" mark changed.
// And MOVING a save onto another map, the edit that's an attack in itself: the header's episode set to another level's.
//
// It changes the blocks BEFORE they're packed again, so each copy is a save the game will unpack: what's tried is the loader, not only the
// unpacking. (The unpacking was tried on its own already, by huffman_test.)
//
// NOTHING HERE KNOWS ABOUT THE GAME but a save's layout and the game's Huffman code (3dc/win95/huffman.cpp). A copy is made one at a time,
// by its number, so a list of thousands costs one copy's memory.
//
// HOW MUCH THIS CAN TAKE. A save given to it can be anything too: every size it reads is checked against what's there, and a save it
// can't make sense of is refused, with why.
#ifndef SAVE_MUTATE_H
#define SAVE_MUTATE_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "huffman.hpp"

size_t const kSaveMutateMostFile = 64u * 1024u * 1024u;   // a save bigger than this isn't a save: the game's are tens of kilobytes
size_t const kSaveMutateMostBlocks = 1000000;              // blocks in one save
int const kSaveMutateBlockKinds = 22;                      // SaveBlock_MainHeader up to SaveBlock_GunOwnState, in savegame.h: 0 to 21
int const kSaveMutateHeaderSize = 8;                        // a block's type and size, an int each
int const kSaveMutateLevelBlockSize = 28;                   // LEVEL_SAVE_BLOCK, savegame.h: the header, the 8-byte mark, 4 bytes, 4, 2, padding

// Where the level header's fields are, from the start of its block (LEVEL_SAVE_BLOCK in savegame.h, 4-byte packing)
size_t const kSaveMutateMarkAt = 8;          // char AvP_Save_String[8]
size_t const kSaveMutateSpeciesAt = 16;      // unsigned char Species
size_t const kSaveMutateEpisodeAt = 17;      // unsigned char Episode
size_t const kSaveMutateDifficultyAt = 24;   // unsigned char Difficulty, after the hours, the minutes, two bytes of padding and the seconds

struct SaveMutateBlock
{
	size_t at;    // where it starts in the unpacked blocks
	int type;
	int size;
};

struct SaveMutateSave
{
	std::vector<uint8_t> header;          // the level's header block, as it is in the file
	std::vector<uint8_t> blocks;          // the rest, unpacked
	std::vector<SaveMutateBlock> list;    // the blocks found in it, in order
};

// One change to make, described; a copy is made from it when it's wanted
enum SaveMutateKind
{
	kSaveMutateBlockSize,      // a block's size set to value
	kSaveMutateBlockType,      // a block's type set to value
	kSaveMutateDropBlock,
	kSaveMutateDoubleBlock,
	kSaveMutateSwapBlock,      // with the next one
	kSaveMutateWord,           // the 4 bytes at a place in a block set to value
	kSaveMutateByte,           // one byte anywhere in the blocks set to value
	kSaveMutateHeaderByte,     // one byte of the level's header set to value
	kSaveMutateLevelHeaderSize,   // the level header's own size set to value
	kSaveMutateMoveTo          // the header's episode set to value: the save moved onto another map
};

struct SaveMutateChange
{
	SaveMutateKind kind;
	size_t block;     // which block, for the block changes
	size_t at;        // where, from the start of the block, or of all the blocks, or of the header
	int64_t value;
};

// The 4 bytes at a place, as the game reads an int (Windows on x86 keeps the low byte first)
inline int32_t SaveMutate_ReadInt(const uint8_t *at)
{
	return static_cast<int32_t>(static_cast<uint32_t>(at[0]) | (static_cast<uint32_t>(at[1]) << 8) | (static_cast<uint32_t>(at[2]) << 16) |
		(static_cast<uint32_t>(at[3]) << 24));
}

inline void SaveMutate_WriteInt(uint8_t *at, int64_t value)
{
	uint32_t const v = static_cast<uint32_t>(static_cast<int32_t>(value));
	at[0] = static_cast<uint8_t>(v);
	at[1] = static_cast<uint8_t>(v >> 8);
	at[2] = static_cast<uint8_t>(v >> 16);
	at[3] = static_cast<uint8_t>(v >> 24);
}

// The work of SaveMutate_Open, below, which empties out again if this refuses part way
inline const char *SaveMutate_OpenParts(const std::vector<uint8_t> &file, SaveMutateSave *out)
{
	out->header.clear();
	out->blocks.clear();
	out->list.clear();

	if (file.size() > kSaveMutateMostFile)
	{
		return "the file is too big to be a save";
	}

	if (file.size() < static_cast<size_t>(kSaveMutateHeaderSize))
	{
		return "the file is too short to hold the level's header";
	}

	int const headerSize = SaveMutate_ReadInt(&file[4]);

	if (headerSize < kSaveMutateLevelBlockSize || static_cast<size_t>(headerSize) > file.size())
	{
		return "the level's header has a size that doesn't fit the file";
	}

	if (SaveMutate_ReadInt(&file[0]) != 0)
	{
		return "the file doesn't start with a level's header";
	}

	out->header.assign(file.begin(), file.begin() + headerSize);
	size_t const left = file.size() - static_cast<size_t>(headerSize);

	if (left < sizeof(HuffmanPackage) || memcmp(&file[headerSize], COMPRESSED_RIF_IDENTIFIER, 8) != 0)
	{
		return "the rest of the file isn't a Huffman package";
	}

	if (left > static_cast<size_t>(INT_MAX))
	{
		return "the package is too big";
	}

	HuffmanPackage package;
	memcpy(&package, &file[headerSize], sizeof(package));
	char *unpacked = HuffmanDecompressChecked(reinterpret_cast<const HuffmanPackage *>(&file[headerSize]), static_cast<int>(left));

	if (!unpacked)
	{
		return "the package doesn't unpack";
	}

	out->blocks.assign(unpacked, unpacked + package.UncompressedDataSize);
	free(unpacked);

	// The blocks, walked as LoadSavedGame walks them
	size_t at = 0;

	while (at < out->blocks.size())
	{
		if (out->blocks.size() - at < static_cast<size_t>(kSaveMutateHeaderSize))
		{
			return "a block's header runs past the end";
		}

		int const type = SaveMutate_ReadInt(&out->blocks[at]);
		int const size = SaveMutate_ReadInt(&out->blocks[at + 4]);

		if (size < kSaveMutateHeaderSize || static_cast<size_t>(size) > out->blocks.size() - at)
		{
			return "a block's size doesn't fit";
		}

		if (out->list.size() >= kSaveMutateMostBlocks)
		{
			return "too many blocks";
		}

		SaveMutateBlock const block = { at, type, size };
		out->list.push_back(block);
		at += static_cast<size_t>(size);
	}

	return nullptr;
}

// Opens a save's bytes: the level's header, the package unpacked by the game's own checked decoder, and its blocks walked as the loader
// walks them. Gives back nothing if it worked, or why not; a save it refuses leaves out empty, whatever part it had read.
inline const char *SaveMutate_Open(const std::vector<uint8_t> &file, SaveMutateSave *out)
{
	const char *why = SaveMutate_OpenParts(file, out);

	if (why)
	{
		out->header.clear();
		out->blocks.clear();
		out->list.clear();
	}

	return why;
}

// Packs a header and blocks into a save file's bytes, with the game's own compressor, as SaveGame does. False if it can't.
inline bool SaveMutate_Pack(const std::vector<uint8_t> &header, const std::vector<uint8_t> &blocks, std::vector<uint8_t> *out)
{
	out->clear();

	if (blocks.size() > static_cast<size_t>(INT_MAX) || blocks.empty())
	{
		return false;
	}

	// The compressor reads but never changes what it's given; it's declared without const, so it gets a copy
	std::vector<uint8_t> source(blocks);
	HuffmanPackage *package = HuffmanCompression(&source[0], static_cast<int>(source.size()));

	if (!package)
	{
		return false;
	}

	size_t const packageSize = sizeof(HuffmanPackage) + static_cast<size_t>(package->CompressedDataSize);
	out->assign(header.begin(), header.end());
	const uint8_t *bytes = reinterpret_cast<const uint8_t *>(package);
	out->insert(out->end(), bytes, bytes + packageSize);
	// The compressor asks for its memory through AllocMem. A program using this header gives it one that's plain malloc, as huffman_test
	// does (the game's own brings most of the game with it), so it goes back with free.
	free(package);
	return true;
}

// A repeatable random number, so the same seed always makes the same copies (the C runtime's rand isn't the same on every machine)
struct SaveMutateRandom
{
	uint32_t state;
	explicit SaveMutateRandom(uint32_t seed) : state(seed ? seed : 1u) {}
	uint32_t Next()
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	size_t Below(size_t n) { return n ? static_cast<size_t>(Next() % n) : 0; }
};

// Every change to try on a save. wordsPerKind is how many numbers inside blocks to try for each kind of block there is in the save, and
// randomBytes how many single bytes anywhere. The episodes to move it to are given, as the levels of its species are the game's to say.
inline std::vector<SaveMutateChange> SaveMutate_Changes(const SaveMutateSave &save, uint32_t seed, size_t wordsPerKind, size_t randomBytes,
	const std::vector<int> &moveTo)
{
	std::vector<SaveMutateChange> changes;
	SaveMutateRandom random(seed);

	// The level's header
	int64_t const headerSizes[] = { 0, kSaveMutateHeaderSize - 1, kSaveMutateLevelBlockSize - 1, static_cast<int64_t>(save.header.size()) + 1, INT_MAX, -1 };

	for (size_t k = 0; k < sizeof(headerSizes) / sizeof(headerSizes[0]); k++)
	{
		SaveMutateChange const c = { kSaveMutateLevelHeaderSize, 0, 4, headerSizes[k] };
		changes.push_back(c);
	}

	int64_t const headerBytes[][2] = { { kSaveMutateSpeciesAt, 3 }, { kSaveMutateSpeciesAt, 255 }, { kSaveMutateEpisodeAt, 255 },
		{ kSaveMutateEpisodeAt, 100 }, { kSaveMutateDifficultyAt, 3 }, { kSaveMutateDifficultyAt, 255 }, { kSaveMutateMarkAt, 'X' } };

	for (size_t k = 0; k < sizeof(headerBytes) / sizeof(headerBytes[0]); k++)
	{
		if (static_cast<size_t>(headerBytes[k][0]) < save.header.size())
		{
			SaveMutateChange const c = { kSaveMutateHeaderByte, 0, static_cast<size_t>(headerBytes[k][0]), headerBytes[k][1] };
			changes.push_back(c);
		}
	}

	for (size_t k = 0; k < moveTo.size(); k++)
	{
		SaveMutateChange const c = { kSaveMutateMoveTo, 0, kSaveMutateEpisodeAt, moveTo[k] };
		changes.push_back(c);
	}

	// Each block: its size, its type, and dropped, doubled and swapped
	for (size_t b = 0; b < save.list.size(); b++)
	{
		int64_t const size = save.list[b].size;
		int64_t const sizes[] = { 0, kSaveMutateHeaderSize - 1, size - 1, size + 1, size + 4, INT_MAX, -1 };

		for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++)
		{
			SaveMutateChange const c = { kSaveMutateBlockSize, b, 4, sizes[k] };
			changes.push_back(c);
		}

		SaveMutateChange const types[] = { { kSaveMutateBlockType, b, 0, -1 }, { kSaveMutateBlockType, b, 0, kSaveMutateBlockKinds },
			{ kSaveMutateDropBlock, b, 0, 0 }, { kSaveMutateDoubleBlock, b, 0, 0 } };
		changes.insert(changes.end(), types, types + 4);

		if (b + 1 < save.list.size())
		{
			SaveMutateChange const c = { kSaveMutateSwapBlock, b, 0, 0 };
			changes.push_back(c);
		}
	}

	// Numbers inside blocks: for each kind of block in the save, wordsPerKind of them, at places picked at random among its blocks
	for (int kind = 0; kind < kSaveMutateBlockKinds; kind++)
	{
		std::vector<size_t> ofKind;

		for (size_t b = 0; b < save.list.size(); b++)
		{
			if (save.list[b].type == kind && save.list[b].size >= kSaveMutateHeaderSize + 4)
			{
				ofKind.push_back(b);
			}
		}

		for (size_t k = 0; k < wordsPerKind && !ofKind.empty(); k++)
		{
			size_t const b = ofKind[random.Below(ofKind.size())];
			size_t const words = (static_cast<size_t>(save.list[b].size) - kSaveMutateHeaderSize) / 4;
			size_t const at = kSaveMutateHeaderSize + 4 * random.Below(words);
			int64_t const was = SaveMutate_ReadInt(&save.blocks[save.list[b].at + at]);
			int64_t const values[] = { 0, -1, 1, INT_MAX, INT_MIN, was + 1, was - 1 };
			SaveMutateChange const c = { kSaveMutateWord, b, at, values[random.Below(sizeof(values) / sizeof(values[0]))] };
			changes.push_back(c);
		}
	}

	// Single bytes anywhere
	for (size_t k = 0; k < randomBytes && !save.blocks.empty(); k++)
	{
		SaveMutateChange const c = { kSaveMutateByte, 0, random.Below(save.blocks.size()), static_cast<int64_t>(random.Next() & 0xFF) };
		changes.push_back(c);
	}

	return changes;
}

// A change's name for a copy's file: what was changed, and to what. Only letters, digits, - and _, so it's a safe file name.
inline std::string SaveMutate_Name(const SaveMutateSave &save, const SaveMutateChange &c)
{
	char text[160];
	int const type = c.block < save.list.size() ? save.list[c.block].type : -1;
	long long const value = static_cast<long long>(c.value);
	char const *sign = value < 0 ? "minus" : "";
	unsigned long long const magnitude = value < 0 ? static_cast<unsigned long long>(-(value + 1)) + 1u : static_cast<unsigned long long>(value);

	switch (c.kind)
	{
	case kSaveMutateBlockSize: snprintf(text, sizeof(text), "block%04u_type%d_size_%s%llu", static_cast<unsigned>(c.block), type, sign, magnitude); break;
	case kSaveMutateBlockType: snprintf(text, sizeof(text), "block%04u_type%d_typeset_%s%llu", static_cast<unsigned>(c.block), type, sign, magnitude); break;
	case kSaveMutateDropBlock: snprintf(text, sizeof(text), "block%04u_type%d_dropped", static_cast<unsigned>(c.block), type); break;
	case kSaveMutateDoubleBlock: snprintf(text, sizeof(text), "block%04u_type%d_doubled", static_cast<unsigned>(c.block), type); break;
	case kSaveMutateSwapBlock: snprintf(text, sizeof(text), "block%04u_type%d_swapped_with_next", static_cast<unsigned>(c.block), type); break;
	case kSaveMutateWord: snprintf(text, sizeof(text), "block%04u_type%d_word_at%u_%s%llu", static_cast<unsigned>(c.block), type, static_cast<unsigned>(c.at), sign, magnitude); break;
	case kSaveMutateByte: snprintf(text, sizeof(text), "byte_at%u_%llu", static_cast<unsigned>(c.at), magnitude); break;
	case kSaveMutateHeaderByte: snprintf(text, sizeof(text), "header_byte_at%u_%llu", static_cast<unsigned>(c.at), magnitude); break;
	case kSaveMutateLevelHeaderSize: snprintf(text, sizeof(text), "header_size_%s%llu", sign, magnitude); break;
	case kSaveMutateMoveTo: snprintf(text, sizeof(text), "moved_to_episode%llu", magnitude); break;
	default: snprintf(text, sizeof(text), "unknown"); break;
	}

	return text;
}

// Makes the copy one change describes: the whole file's bytes, ready to write. False if the change doesn't fit this save (a block that
// isn't there, a place past the end) or the blocks don't pack.
inline bool SaveMutate_Make(const SaveMutateSave &save, const SaveMutateChange &c, std::vector<uint8_t> *file)
{
	std::vector<uint8_t> header(save.header), blocks;
	bool const isBlock = c.kind == kSaveMutateBlockSize || c.kind == kSaveMutateBlockType || c.kind == kSaveMutateDropBlock ||
		c.kind == kSaveMutateDoubleBlock || c.kind == kSaveMutateSwapBlock || c.kind == kSaveMutateWord;

	if (isBlock && c.block >= save.list.size())
	{
		return false;
	}

	SaveMutateBlock const none = { 0, 0, 0 };
	SaveMutateBlock const block = isBlock ? save.list[c.block] : none;
	const uint8_t *start = save.blocks.empty() ? nullptr : &save.blocks[0];

	switch (c.kind)
	{
	case kSaveMutateDropBlock:
		blocks.assign(start, start + block.at);
		blocks.insert(blocks.end(), start + block.at + block.size, start + save.blocks.size());
		break;

	case kSaveMutateDoubleBlock:
		blocks.assign(start, start + block.at + block.size);
		blocks.insert(blocks.end(), start + block.at, start + save.blocks.size());
		break;

	case kSaveMutateSwapBlock:
	{
		if (c.block + 1 >= save.list.size())
		{
			return false;
		}

		SaveMutateBlock const next = save.list[c.block + 1];
		blocks.assign(start, start + block.at);
		blocks.insert(blocks.end(), start + next.at, start + next.at + next.size);
		blocks.insert(blocks.end(), start + block.at, start + block.at + block.size);
		blocks.insert(blocks.end(), start + next.at + next.size, start + save.blocks.size());
		break;
	}

	default:
		blocks = save.blocks;
		break;
	}

	switch (c.kind)
	{
	case kSaveMutateBlockSize:
	case kSaveMutateBlockType:
	case kSaveMutateWord:
	{
		size_t const at = block.at + (c.kind == kSaveMutateBlockType ? 0 : c.at);

		if (c.at + 4 > static_cast<size_t>(block.size))
		{
			return false;
		}

		SaveMutate_WriteInt(&blocks[at], c.value);
		break;
	}

	case kSaveMutateByte:
		if (c.at >= blocks.size())
		{
			return false;
		}

		blocks[c.at] = static_cast<uint8_t>(c.value);
		break;

	case kSaveMutateHeaderByte:
	case kSaveMutateMoveTo:
		if (c.at >= header.size())
		{
			return false;
		}

		header[c.at] = static_cast<uint8_t>(c.value);
		break;

	case kSaveMutateLevelHeaderSize:
		SaveMutate_WriteInt(&header[4], c.value);
		break;

	default:
		break;
	}

	return SaveMutate_Pack(header, blocks, file);
}

#endif
