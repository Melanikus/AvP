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
 * THE STOCK LAYOUT (AvP on Steam plays its films with RAD's own player, not with this library's decoder): the file is laid out as AvP's own
 * message videos are, as read from message100.smk, Message154.smk and message103.smk:
 *   - one palette, in frame 0 only (768 bytes there);
 *   - all four header trees there (MMAP and MCLR too, though no block uses them), each table's size field its entries x 4 + 12, every tree
 *     and byte tree with two leaves at least, no code longer than RAD's own (the deepest there: 21 bits in FULL, 16 in a byte tree);
 *   - runs of 256, 128 or 1 to 59 blocks, the runs RAD's FULL blocks use, ending at the frame's end (a whole frame skipped is 512 + 256);
 *   - sound chunks of a multiple of 4 samples (1468 / 1472 at 15 a second), frame 0's 16 frames' worth.
 * THE STREAM (Smacker_Begin / AddFrame / End): one Encode writes the file from a Source (the frames and sound: in memory for Smacker_Encode,
 * in the stream's own two files for Smacker_End) to a Sink (memory, or the film's file), reading the frames three times (colours, pixel
 * pairs, the frames themselves) and keeping one frame at a time, so the same frames make the same file either way.
 */

#include "SmackerEncoder.h"
#include "BitWriter.h"
#include "HuffmanTree.h"

#include <stdlib.h>
#include <string.h>

#include <wchar.h>

#include <algorithm>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

// the stream's files: Windows' own calls (the library is built for Windows only)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

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

// The film's colours: how many pixels of each key, over every frame, and which palette entry each key gets. One is made for each call, with the
// table of nearest levels, so the encoder keeps no state between calls (it may be called from more than one thread at once).
struct Colours
{
	Levels levels;
	std::vector<uint64_t> count;   // by key
	std::vector<uint8_t> slot;     // by key: its palette entry
	std::vector<uint32_t> keys;    // the keys the film has
	uint8_t palette[256][3] = {};  // levels, 0..63; the entries not used stay 0
	uint32_t used = 0;             // palette entries used, 1 to kMostColours

	Colours() : count(kKeys, 0), slot(kKeys, 0)
	{
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
uint32_t MedianCut(std::vector<uint32_t> &keys, const std::vector<uint64_t> &count, const Levels &levels, uint8_t palette[256][3])
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

			palette[b][c] = levels.nearest[mean >= 255.0 ? 255 : (int)mean];
		}
	}

	return (uint32_t)boxes.size();
}

void CountColours(const uint8_t *rgb, size_t pixels, Colours &colours)
{
	const Levels &levels = colours.levels;

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

	colours.used = MedianCut(keys, colours.count, colours.levels, colours.palette);

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
	const Levels &levels = colours.levels;

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

// What a film is, whatever holds it
struct Film
{
	uint32_t width, height, frameCount, fps, sampleRate;
	size_t pcmBytes;   // the sound's samples, 0 for none
};

// Where the frames and the sound come from: the whole film in memory (Smacker_Encode), or a stream's own files (Smacker_End)
class Source
{
	public:
		virtual ~Source() {}

		// frame f's pixels (width x height x 3 bytes), good until the next call; null when they cannot be had
		virtual const uint8_t *Frame(uint32_t f) = 0;

		// the samples from `from`, count of them (all inside the sound), into out
		virtual bool Sound(size_t from, size_t count, uint8_t *out) = 0;
};

// Where the file goes: written from its start to its end, and then the frame sizes written again in place
class Sink
{
	public:
		virtual ~Sink() {}
		virtual bool Write(const uint8_t *bytes, size_t count) = 0;
		virtual bool Patch(uint64_t at, const uint8_t *bytes, size_t count) = 0;
};

class MemorySource : public Source
{
	public:
		explicit MemorySource(const SmackerEncodeInput &in) : in(in), frameBytes((size_t)in.width * in.height * 3) {}

		const uint8_t *Frame(uint32_t f) { return in.rgb + frameBytes * f; }

		bool Sound(size_t from, size_t count, uint8_t *out)
		{
			memcpy(out, in.pcm + from, count);
			return true;
		}

	private:
		const SmackerEncodeInput &in;
		size_t frameBytes;
};

class MemorySink : public Sink
{
	public:
		std::vector<uint8_t> bytes;

		bool Write(const uint8_t *from, size_t count)
		{
			bytes.insert(bytes.end(), from, from + count);
			return true;
		}

		bool Patch(uint64_t at, const uint8_t *from, size_t count)
		{
			if (at > bytes.size() || count > bytes.size() - at)
				return false;

			memcpy(bytes.data() + at, from, count);
			return true;
		}
};

// THE WHOLE FILE: the palette from every frame's colours, the trees from every frame's pixel pairs, then the header, the frames one by one,
// and last the frame sizes put in their places. Three reads of the frames, one write of the file; nothing of a frame is kept past it.
int Encode(const Film &film, Source &source, Sink &sink)
{
	size_t const pixels = (size_t)film.width * film.height;
	uint32_t const blocksAcross = film.width / 4, blocksDown = film.height / 4;
	std::vector<uint8_t> index;

	// 1. the film's one palette, from every frame's colours
	Colours colours;

	for (uint32_t f = 0; f < film.frameCount; f++)
	{
		const uint8_t *const rgb = source.Frame(f);

		if (!rgb)
			return SMACKER_ENCODE_FILE;

		CountColours(rgb, pixels, colours);
	}

	MakePalette(colours);

	// 2. the counts of the pixel pairs over the whole film and of the block type codes (the trees are the header's, one for all frames)
	std::vector<uint64_t> pairCount(65536, 0), typeCount(65536, 0);
	std::vector<uint32_t> types;

	RunsOf(blocksAcross * blocksDown, &types);

	for (uint32_t f = 0; f < film.frameCount; f++)
	{
		const uint8_t *const rgb = source.Frame(f);

		if (!rgb)
			return SMACKER_ENCODE_FILE;

		MapFrame(rgb, pixels, colours, &index);

		for (uint32_t by = 0; by < blocksDown; by++)
			for (uint32_t bx = 0; bx < blocksAcross; bx++)
				PairsOf(index, film.width, by, bx, &pairCount, 0, 0);

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

	// 4. where each frame's sound is. Frame 0's sound is one second more than the others' (frames 0 to fps), so the player's buffer (sized from
	// frame 0) holds any frame's; frame f after it has the sound of frame f + fps. Every chunk is a whole number of 4 samples (the sound is
	// filled out to one with its last sample), as AvP's own films have.
	bool const sound = film.pcmBytes > 0;
	uint64_t const lead = film.fps;
	uint64_t const soundLength = sound ? (film.pcmBytes + 3) & ~(uint64_t)3 : 0;
	uint8_t lastSample = 0;

	if (sound && !source.Sound(film.pcmBytes - 1, 1, &lastSample))
		return SMACKER_ENCODE_FILE;

	auto soundAt = [&](uint64_t frame) -> uint64_t
	{
		uint64_t const at = (frame * film.sampleRate / film.fps) & ~(uint64_t)3;
		return std::min<uint64_t>(at, soundLength);
	};

	std::vector<uint8_t> flags(film.frameCount, 0);
	uint32_t mostSound = 0;

	flags[0] |= kFramePalette;

	for (uint32_t f = 0; f < film.frameCount && sound; f++)
	{
		uint64_t const from = f == 0 ? 0 : soundAt(f + lead);
		uint64_t const to = soundAt(f + lead + 1);

		if (to > from)
		{
			flags[f] |= kFrameSound0;
			mostSound = std::max(mostSound, (uint32_t)(to - from));
		}
	}

	// 5. the header (Open), the frame sizes (written again at the end), the flags and the trees
	std::vector<uint8_t> head;

	head.push_back('S');
	head.push_back('M');
	head.push_back('K');
	head.push_back('2');
	PutU32(head, film.width);
	PutU32(head, film.height);
	PutU32(head, film.frameCount);
	PutU32(head, (uint32_t)-(int32_t)(100000 / film.fps));   // a frame's time in 1/100 ms, as AvP's own (-6666: 15 a second)
	PutU32(head, 0);                                         // flags: no ring frame

	for (int t = 0; t < 7; t++)
		PutU32(head, t == 0 ? mostSound : 0);               // each track's largest sound chunk, unpacked

	PutU32(head, (uint32_t)trees.Bytes().size());
	PutU32(head, (EntriesOf(spare) + 3) * 4);                // each table's size: its entries and the three "last" places, 4 bytes each
	PutU32(head, (EntriesOf(spare) + 3) * 4);
	PutU32(head, ((uint32_t)full.nodes.size() + 3) * 4);
	PutU32(head, ((uint32_t)type.nodes.size() + 3) * 4);

	for (int t = 0; t < 7; t++)
		PutU32(head, t == 0 && sound ? kSoundPacked | kSoundPresent | film.sampleRate : 0);

	PutU32(head, 0);                                         // pad
	head.resize(head.size() + 4 * (size_t)film.frameCount, 0);
	head.insert(head.end(), flags.begin(), flags.end());
	head.insert(head.end(), trees.Bytes().begin(), trees.Bytes().end());

	uint64_t total = head.size();

	if (total > kMostFileBytes)
		return SMACKER_ENCODE_TOO_BIG;

	if (!sink.Write(head.data(), head.size()))
		return SMACKER_ENCODE_FILE;

	// 6. the frames, each written as it is made
	std::vector<uint8_t> sizes;
	std::vector<uint8_t> out, samples;

	for (uint32_t f = 0; f < film.frameCount; f++)
	{
		out.clear();

		if (f == 0)
			PutPalette(out, colours.palette, colours.used);

		if (flags[f] & kFrameSound0)
		{
			uint64_t const from = f == 0 ? 0 : soundAt(f + lead);
			uint64_t const to = soundAt(f + lead + 1);
			uint64_t const real = from < film.pcmBytes ? std::min<uint64_t>(to, film.pcmBytes) - from : 0;

			samples.assign((size_t)(to - from), lastSample);

			if (real && !source.Sound((size_t)from, (size_t)real, samples.data()))
				return SMACKER_ENCODE_FILE;

			PutSound(out, samples.data(), samples.size());
		}

		const uint8_t *const rgb = source.Frame(f);

		if (!rgb)
			return SMACKER_ENCODE_FILE;

		MapFrame(rgb, pixels, colours, &index);

		BitWriter picture;
		uint32_t blk = 0;

		for (uint32_t t : types)
		{
			HuffTree_PutSymbol(picture, type, t);

			for (int n = block_runs[(t >> 2) & 63]; n > 0; n--, blk++)
				PairsOf(index, film.width, blk / blocksAcross, blk % blocksAcross, 0, &picture, &full);
		}

		out.insert(out.end(), picture.Bytes().begin(), picture.Bytes().end());
		PadTo4(out, 0);

		total += out.size();

		if (out.size() > 0xFFFFFFFCu || total > kMostFileBytes)
			return SMACKER_ENCODE_TOO_BIG;

		if (!sink.Write(out.data(), out.size()))
			return SMACKER_ENCODE_FILE;

		PutU32(sizes, (uint32_t)out.size());
	}

	// 7. the frame sizes, in their places after the 104-byte header
	if (!sink.Patch(kHeaderBytes, sizes.data(), sizes.size()))
		return SMACKER_ENCODE_FILE;

	return SMACKER_ENCODE_DONE;
}

// ---- the stream's own files (Windows' own calls: the DLL is Windows' only)

bool WriteAll(HANDLE file, const uint8_t *bytes, size_t count)
{
	while (count > 0)
	{
		DWORD const step = (DWORD)std::min<size_t>(count, 1u << 30);
		DWORD done = 0;

		if (!WriteFile(file, bytes, step, &done, nullptr) || done != step)
			return false;

		bytes += step;
		count -= step;
	}

	return true;
}

bool ReadAllAt(HANDLE file, uint64_t at, uint8_t *bytes, size_t count)
{
	LARGE_INTEGER place;

	place.QuadPart = (LONGLONG)at;

	if (!SetFilePointerEx(file, place, nullptr, FILE_BEGIN))
		return false;

	while (count > 0)
	{
		DWORD const step = (DWORD)std::min<size_t>(count, 1u << 30);
		DWORD done = 0;

		if (!ReadFile(file, bytes, step, &done, nullptr) || done != step)
			return false;

		bytes += step;
		count -= step;
	}

	return true;
}

// A file of the stream's own: made new (never over one that is there), taken away by Windows when it is closed, after a crash too
HANDLE MakeOwnFile(const std::wstring &path)
{
	return CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
}

class StreamSource : public Source
{
	public:
		StreamSource(HANDLE frames, HANDLE sound, size_t frameBytes) : frames(frames), sound(sound), frame(frameBytes) {}

		const uint8_t *Frame(uint32_t f)
		{
			return ReadAllAt(frames, (uint64_t)f * frame.size(), frame.data(), frame.size()) ? frame.data() : nullptr;
		}

		bool Sound(size_t from, size_t count, uint8_t *out)
		{
			return ReadAllAt(sound, from, out, count);
		}

	private:
		HANDLE frames, sound;
		std::vector<uint8_t> frame;
};

class FileSink : public Sink
{
	public:
		explicit FileSink(HANDLE file) : file(file) {}

		bool Write(const uint8_t *bytes, size_t count)
		{
			return WriteAll(file, bytes, count);
		}

		bool Patch(uint64_t at, const uint8_t *bytes, size_t count)
		{
			LARGE_INTEGER place, zero;

			place.QuadPart = (LONGLONG)at;
			zero.QuadPart = 0;
			return SetFilePointerEx(file, place, nullptr, FILE_BEGIN) && WriteAll(file, bytes, count) && SetFilePointerEx(file, zero, nullptr, FILE_END);
		}

	private:
		HANDLE file;
};

int CheckSettings(const SmackerStreamSettings &s)
{
	if (s.width == 0 || s.height == 0 || s.width % 4 || s.height % 4 || s.width > SMACKER_ENCODE_MOST_SIDE || s.height > SMACKER_ENCODE_MOST_SIDE ||
		(uint64_t)s.width * s.height > SMACKER_ENCODE_MOST_PIXELS)
		return SMACKER_ENCODE_BAD_SIZE;

	if (s.fps == 0 || s.fps > SMACKER_ENCODE_MOST_FPS)
		return SMACKER_ENCODE_BAD_FPS;

	if (s.sampleRate != 0 && (s.sampleRate < SMACKER_ENCODE_LEAST_RATE || s.sampleRate > SMACKER_ENCODE_MOST_RATE))
		return SMACKER_ENCODE_BAD_SOUND;

	return SMACKER_ENCODE_DONE;
}

} // close the unnamed namespace

struct SmackerStream
{
	SmackerStreamSettings settings;
	std::wstring path;
	HANDLE film = INVALID_HANDLE_VALUE;     // the film's own file, made at Begin
	HANDLE frames = INVALID_HANDLE_VALUE;   // the frames, kept until End
	HANDLE sound = INVALID_HANDLE_VALUE;    // the sound, kept until End
	uint32_t frameCount = 0;
	uint64_t pcmBytes = 0;
	int failed = SMACKER_ENCODE_DONE;       // a file that could not be written fails the stream
};

namespace {

// The most sound a stream takes: under 2 GB, as the file itself must be
const uint64_t kMostStreamSound = 0x7FFFFFF0u;

// The longest path Windows takes (with the "\\?\" form), less room for ".frames.tmp"
const size_t kMostPathChars = 32000;

// The stream's files closed and the stream freed. keepFilm false: the film's file is taken away, only when the stream made it.
void CloseStream(SmackerStream *stream, bool keepFilm)
{
	bool const madeFilm = stream->film != INVALID_HANDLE_VALUE;

	if (stream->frames != INVALID_HANDLE_VALUE)
		CloseHandle(stream->frames);   // its own files go as they are closed (FILE_FLAG_DELETE_ON_CLOSE)
	if (stream->sound != INVALID_HANDLE_VALUE)
		CloseHandle(stream->sound);
	if (madeFilm)
		CloseHandle(stream->film);

	if (madeFilm && !keepFilm)
		DeleteFileW(stream->path.c_str());

	delete stream;
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
		Film const film = { in->width, in->height, in->frameCount, in->fps, in->sampleRate, in->pcmBytes };
		MemorySource source(*in);
		MemorySink sink;
		int const result = Encode(film, source, sink);

		if (result != SMACKER_ENCODE_DONE)
			return result;

		uint8_t *const bytes = (uint8_t *)malloc(sink.bytes.size());

		if (!bytes)
			return SMACKER_ENCODE_NO_MEMORY;

		memcpy(bytes, sink.bytes.data(), sink.bytes.size());
		*smk = bytes;
		*smkBytes = sink.bytes.size();
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

int Smacker_Begin(const SmackerStreamSettings *settings, const wchar_t *path, SmackerStream **stream)
{
	if (stream)
		*stream = 0;

	if (!settings || !path || !stream || !path[0] || wcsnlen(path, kMostPathChars + 1) > kMostPathChars)
		return SMACKER_ENCODE_BAD_CALL;

	int const checked = CheckSettings(*settings);

	if (checked != SMACKER_ENCODE_DONE)
		return checked;

	SmackerStream *const made = new (std::nothrow) SmackerStream;

	if (!made)
		return SMACKER_ENCODE_NO_MEMORY;

	std::wstring framesPath, soundPath;

	try
	{
		made->path = path;
		framesPath = made->path + L".frames.tmp";
		soundPath = made->path + L".sound.tmp";
	}
	catch (const std::bad_alloc &)
	{
		delete made;
		return SMACKER_ENCODE_NO_MEMORY;
	}
	catch (const std::length_error &)
	{
		delete made;
		return SMACKER_ENCODE_NO_MEMORY;
	}

	made->settings = *settings;

	// its own two files first: when either cannot be made, a film already at path is left as it was
	made->frames = MakeOwnFile(framesPath);
	made->sound = made->frames != INVALID_HANDLE_VALUE ? MakeOwnFile(soundPath) : INVALID_HANDLE_VALUE;
	made->film = made->sound != INVALID_HANDLE_VALUE ?
		CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;

	if (made->film == INVALID_HANDLE_VALUE)
	{
		CloseStream(made, false);
		return SMACKER_ENCODE_FILE;
	}

	*stream = made;
	return SMACKER_ENCODE_DONE;
}

int Smacker_AddFrame(SmackerStream *stream, const uint8_t *rgb, size_t rgbBytes, const uint8_t *pcm, size_t pcmBytes)
{
	if (!stream)
		return SMACKER_ENCODE_BAD_CALL;

	if (stream->failed != SMACKER_ENCODE_DONE)
		return stream->failed;

	uint64_t const frameBytes = (uint64_t)stream->settings.width * stream->settings.height * 3;

	if (!rgb || rgbBytes != frameBytes)
		return SMACKER_ENCODE_BAD_PIXELS;

	if (stream->frameCount >= SMACKER_ENCODE_MOST_FRAMES)
		return SMACKER_ENCODE_BAD_FRAMES;

	if (pcmBytes && (!pcm || stream->settings.sampleRate == 0 || pcmBytes > kMostStreamSound - stream->pcmBytes))
		return SMACKER_ENCODE_BAD_SOUND;

	if (!WriteAll(stream->frames, rgb, rgbBytes) || (pcmBytes && !WriteAll(stream->sound, pcm, pcmBytes)))
	{
		stream->failed = SMACKER_ENCODE_FILE;
		return stream->failed;
	}

	stream->frameCount++;
	stream->pcmBytes += pcmBytes;
	return SMACKER_ENCODE_DONE;
}

int Smacker_End(SmackerStream *stream)
{
	if (!stream)
		return SMACKER_ENCODE_BAD_CALL;

	int result = stream->failed;

	if (result == SMACKER_ENCODE_DONE && stream->frameCount == 0)
		result = SMACKER_ENCODE_BAD_FRAMES;

	if (result == SMACKER_ENCODE_DONE && stream->pcmBytes > 0 && stream->pcmBytes < 8)
		result = SMACKER_ENCODE_BAD_SOUND;

	if (result == SMACKER_ENCODE_DONE)
	{
		try
		{
			Film const film = { stream->settings.width, stream->settings.height, stream->frameCount, stream->settings.fps, stream->settings.sampleRate, (size_t)stream->pcmBytes };
			StreamSource source(stream->frames, stream->sound, (size_t)film.width * film.height * 3);
			FileSink sink(stream->film);

			result = Encode(film, source, sink);
		}
		catch (const std::bad_alloc &)
		{
			result = SMACKER_ENCODE_NO_MEMORY;
		}
		catch (const std::length_error &)
		{
			result = SMACKER_ENCODE_NO_MEMORY;
		}
	}

	CloseStream(stream, result == SMACKER_ENCODE_DONE);
	return result;
}

void Smacker_Abandon(SmackerStream *stream)
{
	if (stream)
		CloseStream(stream, false);
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
		case SMACKER_ENCODE_FILE: return "the video's file, or the encoder's own files beside it, could not be made, written or read";
	}

	return "an unknown answer";
}
