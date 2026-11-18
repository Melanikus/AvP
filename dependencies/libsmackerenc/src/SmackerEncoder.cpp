/*
 * libsmackerenc - Smacker video encoder
 * Copyright (C) 2011 Barry Duncan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This code is derived from libsmackerdec's SmackerDecoder.cpp (Copyright (C) 2011 Barry Duncan), which is based on smacker.c from the
 * FFmpeg project (http://www.ffmpeg.org/). Below is the license from smacker.c
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Smacker decoder
 * Copyright (c) 2006 Konstantin Shishkov
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Modified 2026-11-18 by Melanikus for the AvP port: an ENCODER, written the other way round from Barry's decoder (SmackerEncoder.h says
 * what it writes). Each part names the part of his SmackerDecoder.cpp that reads it back:
 *   the header and the frame tables              Open
 *   the header trees                             DecodeHeaderTrees, DecodeHeaderTree, DecodeTree, DecodeBigTree
 *   frame 0's palette                            ReadPacket (the palette change: new entries on smk_pal's 64 levels, then a skip)
 *   a frame's sound                              ReadPacket and DecodeAudio (8-bit mono, his "packed" kind)
 *   a frame's picture                            DecodeFrame (one block type, FULL, for every block; mode 0, as SMK2 has)
 * THE STOCK LAYOUT (the owner, 11-18: AvP on Steam plays films with RAD's own player, not Barry's decoder): the file is laid out as AvP's own
 * message videos are, read with AvP-docs/tools/frost/smk/smk-trees.js from message100.smk, Message154.smk and message103.smk:
 *   - one palette, in frame 0 only (768 bytes there);
 *   - all four header trees there (MMAP and MCLR too, though no block uses them), each table's size field its entries x 4 + 12, every tree
 *     and byte tree with two leaves at least, no code longer than RAD's own (the deepest there: 21 bits in FULL, 16 in a byte tree);
 *   - runs of 256, 128 or 1 to 59 blocks, the runs RAD's FULL blocks use, ending at the frame's end (a whole frame skipped is 512 + 256);
 *   - sound chunks of a multiple of 4 samples (1468 / 1472 at 15 a second), frame 0's 16 frames' worth.
 */

#include "SmackerEncoder.h"
#include "BitWriter.h"
#include "HuffmanTree.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <new>
#include <stdexcept>
#include <vector>

using SmackerCommon::BitWriter;
using SmackerCommon::HuffTree;

namespace {

/* palette used in Smacker (his smk_pal): a palette entry is one of these 64 levels for each of red, green and blue */
const uint8_t smk_pal[64] = {
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
	0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
	0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
	0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
	0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
};

/* possible runs of blocks (his block_runs) */
const int block_runs[64] = {
	  1,    2,    3,    4,    5,    6,    7,    8,
	  9,   10,   11,   12,   13,   14,   15,   16,
	 17,   18,   19,   20,   21,   22,   23,   24,
	 25,   26,   27,   28,   29,   30,   31,   32,
	 33,   34,   35,   36,   37,   38,   39,   40,
	 41,   42,   43,   44,   45,   46,   47,   48,
	 49,   50,   51,   52,   53,   54,   55,   56,
	 57,   58,   59,  128,  256,  512, 1024, 2048 };

// 255 colours at most: palette entry 255 is never used, so every 16-bit value with a byte of 255 is free for a tree's three "escape"
// values (DecodeBigTree takes a leaf of an escape's value as one of its "last" places, which this encoder doesn't use; RAD's own MMAP
// trees have no such leaf either)
const uint32_t kMostColours = 255;

const uint32_t kBlockFull = 1;                           // SMK_BLK_FULL
const uint8_t kFramePalette = 0x01;                      // kSMKpal
const uint8_t kFrameSound0 = 0x02;                       // sound track 0 (ReadPacket shifts the flags right once)
const uint32_t kSoundPacked = 0x80000000u;               // SMK_AUD_PACKED
const uint32_t kSoundPresent = 0x40000000u;              // as AvP's own films set it (message100.smk: 0xC0005622)
const uint32_t kByteTreeLongest = 15;                    // RAD's own byte trees: 16 bits at most in AvP's films (DecodeTree takes 32)
const uint32_t kBigTreeLongest = 20;                     // RAD's own FULL trees: 21 at most (DecodeBigTree takes 500)
const uint64_t kMostFileBytes = 0x7FFFFFF0u;             // ReadPacket keeps the next frame's place in an int32_t
const uint32_t kHeaderBytes = 104;

// The 64 levels nearest each 8-bit value
struct Levels
{
	uint8_t nearest[256];

	Levels()
	{
		for (int v = 0; v < 256; v++)
		{
			int best = 0;

			for (int i = 1; i < 64; i++)
			{
				if (abs((int)smk_pal[i] - v) < abs((int)smk_pal[best] - v))
					best = i;
			}

			nearest[v] = (uint8_t)best;
		}
	}
};

const Levels &TheLevels()
{
	static const Levels levels;
	return levels;
}

// a colour as three levels, 6 bits each: red 12..17, green 6..11, blue 0..5
const uint32_t kKeys = 1u << 18;

uint32_t KeyOf(const uint8_t *rgb, const Levels &levels)
{
	return ((uint32_t)levels.nearest[rgb[0]] << 12) | ((uint32_t)levels.nearest[rgb[1]] << 6) | (uint32_t)levels.nearest[rgb[2]];
}

uint32_t LevelOf(uint32_t key, int channel)
{
	return (key >> (12 - 6 * channel)) & 63;
}

// The film's colours: how many pixels of each key, over every frame, and which palette entry each key gets
struct Colours
{
	std::vector<uint64_t> count;   // by key
	std::vector<uint8_t> slot;     // by key: its palette entry
	std::vector<uint32_t> keys;    // the keys the film has
	uint8_t palette[256][3];       // levels, 0..63
	uint32_t used;                 // palette entries used, 1 to kMostColours

	Colours() : count(kKeys, 0), slot(kKeys, 0), used(0)
	{
		memset(palette, 0, sizeof(palette));
	}
};

struct Box
{
	size_t begin, end;   // in the keys
	uint64_t pixels;
	int axis;            // its widest channel
	uint32_t range;      // how wide, in levels
};

Box BoxOf(const std::vector<uint32_t> &keys, const std::vector<uint64_t> &count, size_t begin, size_t end)
{
	Box box = { begin, end, 0, 0, 0 };
	uint32_t low[3] = { 63, 63, 63 }, high[3] = { 0, 0, 0 };

	if (begin >= end)
	{
		return box;
	}

	for (size_t i = begin; i < end; i++)
	{
		box.pixels += count[keys[i]];

		for (int c = 0; c < 3; c++)
		{
			uint32_t const level = LevelOf(keys[i], c);

			low[c] = std::min(low[c], level);
			high[c] = std::max(high[c], level);
		}
	}

	for (int c = 0; c < 3; c++)
	{
		if (c == 0 || high[c] - low[c] > box.range)
		{
			box.range = high[c] - low[c];
			box.axis = c;
		}
	}

	return box;
}

// More colours than a palette holds: MEDIAN CUT. The box of colours with the most pixels times width is cut at its pixels' middle
// along its widest channel, until there are kMostColours boxes; each box's colour is its pixels' mean, on the nearest levels.
uint32_t MedianCut(std::vector<uint32_t> &keys, const std::vector<uint64_t> &count, uint8_t palette[256][3])
{
	std::vector<Box> boxes;

	boxes.push_back(BoxOf(keys, count, 0, keys.size()));

	while (boxes.size() < kMostColours)
	{
		size_t pick = boxes.size();
		double best = 0.0;

		for (size_t b = 0; b < boxes.size(); b++)
		{
			double const score = (double)boxes[b].range * (double)boxes[b].pixels;

			if (boxes[b].range > 0 && boxes[b].end - boxes[b].begin >= 2 && score > best)
			{
				best = score;
				pick = b;
			}
		}

		if (pick == boxes.size())
		{
			break;   // every box is one colour
		}

		Box const box = boxes[pick];
		int const axis = box.axis;

		std::sort(keys.begin() + (std::ptrdiff_t)box.begin, keys.begin() + (std::ptrdiff_t)box.end, [axis](uint32_t a, uint32_t b)
		{
			uint32_t const la = LevelOf(a, axis), lb = LevelOf(b, axis);
			return la != lb ? la < lb : a < b;
		});

		// cut after the key that takes the count past half the box's pixels, leaving at least one key on each side
		uint64_t seen = 0;
		size_t cut = box.end - 1;

		for (size_t i = box.begin; i + 1 < box.end; i++)
		{
			seen += count[keys[i]];

			if (seen * 2 >= box.pixels)
			{
				cut = i + 1;
				break;
			}
		}

		boxes[pick] = BoxOf(keys, count, box.begin, cut);
		boxes.push_back(BoxOf(keys, count, cut, box.end));
	}

	for (size_t b = 0; b < boxes.size(); b++)
	{
		double sum[3] = { 0.0, 0.0, 0.0 };

		for (size_t i = boxes[b].begin; i < boxes[b].end; i++)
		{
			for (int c = 0; c < 3; c++)
			{
				sum[c] += (double)smk_pal[LevelOf(keys[i], c)] * (double)count[keys[i]];
			}
		}

		for (int c = 0; c < 3; c++)
		{
			double const mean = boxes[b].pixels ? sum[c] / (double)boxes[b].pixels + 0.5 : 0.0;

			palette[b][c] = TheLevels().nearest[mean >= 255.0 ? 255 : (int)mean];
		}
	}

	return (uint32_t)boxes.size();
}

void CountColours(const uint8_t *rgb, size_t pixels, Colours &colours)
{
	const Levels &levels = TheLevels();

	for (size_t i = 0; i < pixels; i++)
	{
		uint32_t const key = KeyOf(rgb + i * 3, levels);

		if (colours.count[key]++ == 0)
		{
			colours.keys.push_back(key);
		}
	}
}

// The film's ONE palette (as AvP's own films have, in frame 0 only). 255 colours or fewer (on the levels) are kept exactly; more are cut down.
void MakePalette(Colours &colours)
{
	// the same film always gets the same palette
	std::sort(colours.keys.begin(), colours.keys.end());

	if (colours.keys.size() <= kMostColours)
	{
		for (size_t k = 0; k < colours.keys.size(); k++)
		{
			for (int c = 0; c < 3; c++)
			{
				colours.palette[k][c] = (uint8_t)LevelOf(colours.keys[k], c);
			}

			colours.slot[colours.keys[k]] = (uint8_t)k;
		}

		colours.used = (uint32_t)colours.keys.size();
		return;
	}

	std::vector<uint32_t> keys = colours.keys;

	colours.used = MedianCut(keys, colours.count, colours.palette);

	// each colour to its nearest entry (not always its box's)
	for (uint32_t key : colours.keys)
	{
		uint32_t best = 0, bestDistance = 0xFFFFFFFFu;

		for (uint32_t e = 0; e < colours.used; e++)
		{
			uint32_t distance = 0;

			for (int c = 0; c < 3; c++)
			{
				int const d = (int)smk_pal[LevelOf(key, c)] - (int)smk_pal[colours.palette[e][c]];
				distance += (uint32_t)(d * d);
			}

			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = e;
			}
		}

		colours.slot[key] = (uint8_t)best;
	}
}

void MapFrame(const uint8_t *rgb, size_t pixels, const Colours &colours, std::vector<uint8_t> *index)
{
	const Levels &levels = TheLevels();

	index->resize(pixels);

	for (size_t i = 0; i < pixels; i++)
	{
		(*index)[i] = colours.slot[KeyOf(rgb + i * 3, levels)];
	}
}

// Each 4 x 4 block, left to right and top to bottom (DecodeFrame's blk), as FULL: each of its 4 rows is the code of pixels 2 and 3 (the
// low byte the left one), then of pixels 0 and 1. count gives each pair to the counts; else the pairs' codes are written.
void PairsOf(const std::vector<uint8_t> &index, uint32_t width, uint32_t by, uint32_t bx, std::vector<uint64_t> *count, BitWriter *bits, const HuffTree *full)
{
	for (uint32_t r = 0; r < 4; r++)
	{
		const uint8_t *const row = &index[((size_t)by * 4 + r) * width + (size_t)bx * 4];
		uint32_t const right = (uint32_t)row[2] | ((uint32_t)row[3] << 8);
		uint32_t const left = (uint32_t)row[0] | ((uint32_t)row[1] << 8);

		if (count)
		{
			(*count)[right]++;
			(*count)[left]++;
		}
		else
		{
			HuffTree_PutSymbol(*bits, *full, right);
			HuffTree_PutSymbol(*bits, *full, left);
		}
	}
}

// A frame's blocks as runs, each a block type code: 256, 128, or 1 to 59 blocks, the longest that fits first (RAD's own FULL runs), so
// the last run ends at the frame's last block
void RunsOf(uint32_t blocks, std::vector<uint32_t> *types)
{
	types->clear();

	while (blocks > 0)
	{
		int run = blocks >= 256 ? 60 : blocks >= 128 ? 59 : (int)std::min<uint32_t>(blocks, 59) - 1;

		types->push_back(kBlockFull | ((uint32_t)run << 2));
		blocks -= (uint32_t)block_runs[run];
	}
}

// Two leaves at least, as every tree of AvP's own films has: a lone symbol gets a second, never used (the first symbol of count 0)
void TwoAtLeast(std::vector<uint64_t> &counts)
{
	size_t had = 0, firstFree = counts.size();

	for (size_t i = 0; i < counts.size(); i++)
	{
		if (counts[i])
			had++;
		else if (firstFree == counts.size())
			firstFree = i;
	}

	for (; had < 2 && firstFree < counts.size(); had++)
	{
		counts[firstFree] = 1;

		while (firstFree < counts.size() && counts[firstFree])
			firstFree++;
	}
}

// A 16-bit tree as DecodeHeaderTree reads it: "there" (1), its low bytes' tree (there 1, the tree, 0), its high bytes' tree (the same),
// three escape values no leaf holds, the tree, and a 0 bit. false when no three such values are free, or a tree can't be made.
bool PutHeaderTree(BitWriter &bits, std::vector<uint64_t> counts)
{
	TwoAtLeast(counts);

	HuffTree tree;

	if (!HuffTree_Build(counts, kBigTreeLongest, &tree))
	{
		return false;
	}

	std::vector<uint64_t> lowCount(256, 0), highCount(256, 0);

	for (uint32_t symbol : tree.leaves)
	{
		lowCount[symbol & 0xFF]++;
		highCount[symbol >> 8]++;
	}

	TwoAtLeast(lowCount);
	TwoAtLeast(highCount);

	HuffTree low, high;

	if (!HuffTree_Build(lowCount, kByteTreeLongest, &low) || !HuffTree_Build(highCount, kByteTreeLongest, &high))
	{
		return false;
	}

	uint32_t escapes[3];
	int found = 0;

	for (int32_t v = 0xFFFF; v >= 0 && found < 3; v--)
	{
		if (!counts[(size_t)v])
		{
			escapes[found++] = (uint32_t)v;
		}
	}

	if (found < 3)
	{
		return false;
	}

	bits.PutBit(1);
	bits.PutBit(1);
	HuffTree_WriteByteTree(bits, low);
	bits.PutBit(0);
	bits.PutBit(1);
	HuffTree_WriteByteTree(bits, high);
	bits.PutBit(0);

	for (int e = 0; e < 3; e++)
	{
		bits.PutBits(escapes[e] & 0xFF, 8);
		bits.PutBits(escapes[e] >> 8, 8);
	}

	HuffTree_WriteBigTree(bits, tree, low, high);
	bits.PutBit(0);
	return true;
}

// How many entries a tree of these counts has, as PutHeaderTree makes it (its header size field is that x 4 + 12, as RAD's)
uint32_t EntriesOf(std::vector<uint64_t> counts)
{
	TwoAtLeast(counts);

	size_t leaves = 0;

	for (uint64_t c : counts)
	{
		leaves += c ? 1 : 0;
	}

	return (uint32_t)(2 * leaves - 1);
}

void PutU32(std::vector<uint8_t> &out, uint32_t value)
{
	out.push_back((uint8_t)value);
	out.push_back((uint8_t)(value >> 8));
	out.push_back((uint8_t)(value >> 16));
	out.push_back((uint8_t)(value >> 24));
}

void PadTo4(std::vector<uint8_t> &out, size_t start)
{
	while ((out.size() - start) % 4)
	{
		out.push_back(0);
	}
}

// The palette change ReadPacket reads: a size byte (the chunk's length in 4s), then a "new entry" command of three levels for each entry
// used (a level is under 0x40, so it reads as neither a skip, 0x80, nor a copy, 0x40), a skip over the rest (0x80 | n - 1, 128 at most
// each), padded to the size: 768 bytes for 255 colours, as AvP's own films' palette
void PutPalette(std::vector<uint8_t> &out, const uint8_t palette[256][3], uint32_t used)
{
	size_t const start = out.size();

	out.push_back(0);

	for (uint32_t e = 0; e < used; e++)
	{
		out.push_back(palette[e][0]);
		out.push_back(palette[e][1]);
		out.push_back(palette[e][2]);
	}

	for (uint32_t left = 256 - used; left > 0;)
	{
		uint32_t const n = std::min<uint32_t>(left, 128);

		out.push_back((uint8_t)(0x80 | (n - 1)));
		left -= n;
	}

	PadTo4(out, start);
	out[start] = (uint8_t)((out.size() - start) / 4);
}

// One frame's sound as DecodeAudio reads it (8-bit mono, packed): the chunk's size (4 bytes, counting itself), then bits: the sample count
// (32), "has data" (1), mono (0), 8-bit (0), the tree of the steps from sample to sample ("there" 1, the tree, then 0), the first sample
// (8), and each step's code. A step is the next sample less this one, kept to a byte; the decoder adds it back.
void PutSound(std::vector<uint8_t> &out, const uint8_t *samples, size_t count)
{
	std::vector<uint64_t> stepCount(256, 0);

	for (size_t i = 1; i < count; i++)
	{
		stepCount[(uint8_t)(samples[i] - samples[i - 1])]++;
	}

	TwoAtLeast(stepCount);

	HuffTree steps;

	HuffTree_Build(stepCount, kByteTreeLongest, &steps);

	BitWriter bits;

	bits.PutBits((uint32_t)count, 32);
	bits.PutBit(1);
	bits.PutBit(0);
	bits.PutBit(0);
	bits.PutBit(1);
	HuffTree_WriteByteTree(bits, steps);
	bits.PutBit(0);
	bits.PutBits(samples[0], 8);

	for (size_t i = 1; i < count; i++)
	{
		HuffTree_PutSymbol(bits, steps, (uint8_t)(samples[i] - samples[i - 1]));
	}

	size_t const start = out.size();

	PutU32(out, 0);
	out.insert(out.end(), bits.Bytes().begin(), bits.Bytes().end());
	PadTo4(out, start);

	uint32_t const size = (uint32_t)(out.size() - start);

	out[start] = (uint8_t)size;
	out[start + 1] = (uint8_t)(size >> 8);
	out[start + 2] = (uint8_t)(size >> 16);
	out[start + 3] = (uint8_t)(size >> 24);
}

int CheckInput(const SmackerEncodeInput &in)
{
	if (in.width == 0 || in.height == 0 || in.width % 4 || in.height % 4 || in.width > SMACKER_ENCODE_MOST_SIDE || in.height > SMACKER_ENCODE_MOST_SIDE ||
		(uint64_t)in.width * in.height > SMACKER_ENCODE_MOST_PIXELS)
		return SMACKER_ENCODE_BAD_SIZE;

	if (in.frameCount == 0 || in.frameCount > SMACKER_ENCODE_MOST_FRAMES)
		return SMACKER_ENCODE_BAD_FRAMES;

	if (in.fps == 0 || in.fps > SMACKER_ENCODE_MOST_FPS)
		return SMACKER_ENCODE_BAD_FPS;

	// at most 4096 x 4096 x 3 x 100000 bytes: no overflow in 64 bits
	uint64_t const pixelBytes = (uint64_t)in.width * in.height * 3 * in.frameCount;

	if (!in.rgb || pixelBytes > (uint64_t)SIZE_MAX || in.rgbBytes != (size_t)pixelBytes)
		return SMACKER_ENCODE_BAD_PIXELS;

	if (in.pcmBytes && (!in.pcm || in.pcmBytes < 8 || in.sampleRate < SMACKER_ENCODE_LEAST_RATE || in.sampleRate > SMACKER_ENCODE_MOST_RATE))
		return SMACKER_ENCODE_BAD_SOUND;

	return SMACKER_ENCODE_DONE;
}

int Encode(const SmackerEncodeInput &in, std::vector<uint8_t> *file)
{
	size_t const pixels = (size_t)in.width * in.height;
	size_t const frameBytes = pixels * 3;
	uint32_t const blocksAcross = in.width / 4, blocksDown = in.height / 4;
	std::vector<uint8_t> index;

	// 1. the film's one palette, from every frame's colours
	Colours colours;

	for (uint32_t f = 0; f < in.frameCount; f++)
	{
		CountColours(in.rgb + frameBytes * f, pixels, colours);
	}

	MakePalette(colours);

	// 2. the counts of the pixel pairs over the whole film and of the block type codes (the trees are the header's, one for all frames)
	std::vector<uint64_t> pairCount(65536, 0), typeCount(65536, 0), unused(65536, 0);
	std::vector<uint32_t> types;

	RunsOf(blocksAcross * blocksDown, &types);

	for (uint32_t f = 0; f < in.frameCount; f++)
	{
		MapFrame(in.rgb + frameBytes * f, pixels, colours, &index);

		for (uint32_t by = 0; by < blocksDown; by++)
			for (uint32_t bx = 0; bx < blocksAcross; bx++)
				PairsOf(index, in.width, by, bx, &pairCount, 0, 0);

		for (uint32_t type : types)
			typeCount[type]++;
	}

	// 3. the header trees, all four there as in AvP's own films: MMAP and MCLR (no MONO block uses them: two leaves each), FULL, TYPE
	std::vector<uint64_t> spare(65536, 0);

	spare[0] = 1;
	spare[1] = 1;

	BitWriter trees;

	if (!PutHeaderTree(trees, spare) || !PutHeaderTree(trees, spare) || !PutHeaderTree(trees, pairCount) || !PutHeaderTree(trees, typeCount))
		return SMACKER_ENCODE_NO_MEMORY;   // not reached: palette entry 255 is never used, so escapes are free

	TwoAtLeast(pairCount);
	TwoAtLeast(typeCount);

	HuffTree full, type;

	if (!HuffTree_Build(pairCount, kBigTreeLongest, &full) || !HuffTree_Build(typeCount, kBigTreeLongest, &type))
		return SMACKER_ENCODE_NO_MEMORY;

	// 4. the frames. Frame 0's sound is one second more than the others' (frames 0 to fps), so the player's buffer (sized from frame 0) holds
	// any frame's; frame f after it has the sound of frame f + fps. Every chunk is a whole number of 4 samples (the sound is filled out to
	// one with its last sample), as AvP's own films have.
	bool const sound = in.pcmBytes > 0;
	uint64_t const lead = in.fps;
	std::vector<uint8_t> pcm;

	if (sound)
	{
		pcm.assign(in.pcm, in.pcm + in.pcmBytes);

		while (pcm.size() % 4)
			pcm.push_back(pcm.back());
	}

	std::vector<std::vector<uint8_t> > frames(in.frameCount);
	std::vector<uint8_t> flags(in.frameCount, 0);
	uint32_t mostSound = 0;
	uint64_t total = kHeaderBytes + 5 * (uint64_t)in.frameCount + trees.Bytes().size();

	auto soundAt = [&](uint64_t frame) -> size_t
	{
		uint64_t const at = (frame * in.sampleRate / in.fps) & ~(uint64_t)3;
		return (size_t)std::min<uint64_t>(at, pcm.size());
	};

	for (uint32_t f = 0; f < in.frameCount; f++)
	{
		std::vector<uint8_t> &out = frames[f];

		if (f == 0)
		{
			PutPalette(out, colours.palette, colours.used);
			flags[f] |= kFramePalette;
		}

		if (sound)
		{
			size_t const from = f == 0 ? 0 : soundAt(f + lead);
			size_t const to = soundAt(f + lead + 1);

			if (to > from)
			{
				PutSound(out, pcm.data() + from, to - from);
				flags[f] |= kFrameSound0;
				mostSound = std::max(mostSound, (uint32_t)(to - from));
			}
		}

		MapFrame(in.rgb + frameBytes * f, pixels, colours, &index);

		BitWriter picture;
		uint32_t blk = 0;

		for (uint32_t t : types)
		{
			HuffTree_PutSymbol(picture, type, t);

			for (int n = block_runs[(t >> 2) & 63]; n > 0; n--, blk++)
				PairsOf(index, in.width, blk / blocksAcross, blk % blocksAcross, 0, &picture, &full);
		}

		out.insert(out.end(), picture.Bytes().begin(), picture.Bytes().end());
		PadTo4(out, 0);

		total += out.size();

		if (out.size() > 0xFFFFFFFCu || total > kMostFileBytes)
			return SMACKER_ENCODE_TOO_BIG;
	}

	// 5. the header (Open), the frame sizes and flags, the trees, the frames
	file->clear();
	file->reserve((size_t)total);
	file->push_back('S');
	file->push_back('M');
	file->push_back('K');
	file->push_back('2');
	PutU32(*file, in.width);
	PutU32(*file, in.height);
	PutU32(*file, in.frameCount);
	PutU32(*file, (uint32_t)-(int32_t)(100000 / in.fps));   // a frame's time in 1/100 ms, as AvP's own (-6666: 15 a second)
	PutU32(*file, 0);                                       // flags: no ring frame

	for (int t = 0; t < 7; t++)
		PutU32(*file, t == 0 ? mostSound : 0);             // each track's largest sound chunk, unpacked

	PutU32(*file, (uint32_t)trees.Bytes().size());
	PutU32(*file, (EntriesOf(spare) + 3) * 4);              // each table's size: its entries and the three "last" places, 4 bytes each
	PutU32(*file, (EntriesOf(spare) + 3) * 4);
	PutU32(*file, ((uint32_t)full.nodes.size() + 3) * 4);
	PutU32(*file, ((uint32_t)type.nodes.size() + 3) * 4);

	for (int t = 0; t < 7; t++)
		PutU32(*file, t == 0 && sound ? kSoundPacked | kSoundPresent | in.sampleRate : 0);

	PutU32(*file, 0);                                       // pad

	for (uint32_t f = 0; f < in.frameCount; f++)
		PutU32(*file, (uint32_t)frames[f].size());

	file->insert(file->end(), flags.begin(), flags.end());
	file->insert(file->end(), trees.Bytes().begin(), trees.Bytes().end());

	for (uint32_t f = 0; f < in.frameCount; f++)
	{
		file->insert(file->end(), frames[f].begin(), frames[f].end());
		std::vector<uint8_t>().swap(frames[f]);
	}

	return SMACKER_ENCODE_DONE;
}

} // close the unnamed namespace

int Smacker_Encode(const SmackerEncodeInput *in, uint8_t **smk, size_t *smkBytes)
{
	if (smk)
		*smk = 0;
	if (smkBytes)
		*smkBytes = 0;

	if (!in || !smk || !smkBytes)
		return SMACKER_ENCODE_BAD_CALL;

	int const checked = CheckInput(*in);

	if (checked != SMACKER_ENCODE_DONE)
		return checked;

	// no C++ exception may leave the library: running out of memory fails the call
	try
	{
		std::vector<uint8_t> file;
		int const result = Encode(*in, &file);

		if (result != SMACKER_ENCODE_DONE)
			return result;

		uint8_t *const bytes = (uint8_t *)malloc(file.size());

		if (!bytes)
			return SMACKER_ENCODE_NO_MEMORY;

		memcpy(bytes, file.data(), file.size());
		*smk = bytes;
		*smkBytes = file.size();
		return SMACKER_ENCODE_DONE;
	}
	catch (const std::bad_alloc &)
	{
		return SMACKER_ENCODE_NO_MEMORY;
	}
	catch (const std::length_error &)
	{
		return SMACKER_ENCODE_NO_MEMORY;
	}
}

void Smacker_FreeEncoded(uint8_t *smk)
{
	free(smk);
}

const char *Smacker_EncodeWords(int result)
{
	switch (result)
	{
		case SMACKER_ENCODE_DONE: return "encoded";
		case SMACKER_ENCODE_BAD_CALL: return "no input, or nowhere to put the video";
		case SMACKER_ENCODE_BAD_SIZE: return "the width and height must be multiples of 4, from 4 to 4096";
		case SMACKER_ENCODE_BAD_FRAMES: return "a video needs 1 to 100000 frames";
		case SMACKER_ENCODE_BAD_FPS: return "the frame rate must be 1 to 100 frames a second";
		case SMACKER_ENCODE_BAD_PIXELS: return "the pixels given are not frames x width x height x 3 bytes";
		case SMACKER_ENCODE_BAD_SOUND: return "the sound must be at least 8 samples, at 1000 to 96000 a second";
		case SMACKER_ENCODE_TOO_BIG: return "the video would be too big for a Smacker file";
		case SMACKER_ENCODE_NO_MEMORY: return "there was not enough memory to encode the video";
	}

	return "an unknown answer";
}
