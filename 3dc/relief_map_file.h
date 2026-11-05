// What a hand-made relief map's file has to be, checked before anything is allocated for it. Written 2026-11-05 by Melanikus for the AvP port.
//
// A map is a file from a stranger, so nothing in it is trusted. This is all sums on bytes already in memory, with nothing of the game and no graphics card, so a test can throw
// anything at it. The game and tools/rimpng both use it, so a map the tool says is good is one the game will take. tools/rimpng/README.md is the page for people making maps.
//
// THE RULES FOR BOTH KINDS: square, a power of two, from 4 to 2048 a side. The map is read by texture position, so it needn't be the size of its picture. Any size of picture can
// have any size of map. Each kind says exactly what's wrong with a file, so the log can say why it wasn't used.
//
// name_Normal.rim IS WHAT THE GAME READS TODAY. It's the game's own kind of picture, and it can be up to 24 bits a pixel here, which the ordinary loader refuses: RimLoader only ever
// sees stock pictures and has no way in to this code, nor this code to it. Its smaller copies (mipmaps), which are what stop a detailed map shimmering in the distance, are worked out
// when it loads.
//
// name_Normal.dds IS CHECKED HERE, AND THE GAME DOESN'T READ IT YET. One layout only: DXT5, with a full set of smaller copies. It takes a quarter of the memory, and the graphics card does
// the unpacking, so any bytes at all are safe in it. All there is to check is one fixed heading, and that the file is exactly as long as that heading says. There's nothing to walk
// through and no table to look anything up in, which is why it's the simplest to be sure of. This code will read it, not D3DX, which is closed code that changes sizes without saying.
//
// THE PIXEL: red is x, green is y and blue is z, each 128 + 127 * a number from -1 to 1, which is what surfaceRelief.psh reads. Alpha holds x again, because DXT5 has room for only two
// good numbers, which will be alpha and green.

#ifndef RELIEF_MAP_FILE_H
#define RELIEF_MAP_FILE_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>

uint32_t const kReliefMapSmallestSide = 4;
uint32_t const kReliefMapLargestSide = 2048;
uint32_t const kReliefMapDdsHeadingBytes = 128;
uint32_t const kReliefMapRimDeepPlanes = 24;     // red, then green, then blue, 8 planes each, with no colour table

// No file that passes is longer than this: the largest .dds, or the largest .rim with its colour table, packed as badly as it can be. It's checked before a file is read at all.
uint64_t const kReliefMapMostFileBytes = 32 * 1024 * 1024;

enum ReliefMapProblem
{
	kMapFine,
	kMapFileTooLong,
	kMapShorterThanItsHeading,
	kMapNotADds,
	kMapHeadingWrong,
	kMapNotSquare,
	kMapSideNotAPowerOfTwo,
	kMapSideTooSmall,
	kMapSideTooLarge,
	kMapFormatWrong,
	kMapNotAFlatPicture,
	kMapSmallerCopiesWrong,
	kMapLengthWrong,
	kMapPartsWrong,
	kMapNoHeading,
	kMapNoPicture,
	kMapPlanesWrong,
	kMapPackingWrong,
	kMapColourTableWrong,
	kMapColourPastTable,
	kMapRunPastRow,
	kMapCutShort,
	kMapProblemCount
};

// For the log, to follow "this map isn't used, because"
inline char const *ReliefMap_ProblemWords(ReliefMapProblem problem)
{
	switch (problem)
	{
		case kMapFine:                  return "there's nothing wrong with it";
		case kMapFileTooLong:           return "it's longer than any map can be";
		case kMapShorterThanItsHeading: return "it's shorter than its heading";
		case kMapNotADds:               return "it doesn't start the way a DDS file does";
		case kMapHeadingWrong:          return "its heading isn't laid out as a DDS heading is";
		case kMapNotSquare:             return "it isn't square";
		case kMapSideNotAPowerOfTwo:    return "its side isn't a power of two";
		case kMapSideTooSmall:          return "its side is under 4";
		case kMapSideTooLarge:          return "its side is over 2048";
		case kMapFormatWrong:           return "it isn't DXT5";
		case kMapNotAFlatPicture:       return "it's a cube or a volume, not a flat picture";
		case kMapSmallerCopiesWrong:    return "it doesn't have the full set of smaller copies";
		case kMapLengthWrong:           return "it isn't exactly as long as its heading says";
		case kMapPartsWrong:            return "its parts aren't what a .rim's should be, or run past the end of the file";
		case kMapNoHeading:             return "it has no heading, or the heading is in the wrong place";
		case kMapNoPicture:             return "it has no picture in it";
		case kMapPlanesWrong:           return "it has a number of planes that can't be read: up to 16 with a colour table, or 24";
		case kMapPackingWrong:          return "it's packed in a way that can't be read";
		case kMapColourTableWrong:      return "its colour table doesn't suit its planes";
		case kMapColourPastTable:       return "a pixel uses a colour past the end of its table";
		case kMapRunPastRow:            return "a packed run goes past the end of its row";
		case kMapCutShort:              return "the file ends before the picture does";
		default:                        return "of a problem with no name";
	}
}

inline uint32_t ReliefMap_Little32(uint8_t const *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

inline uint32_t ReliefMap_Big32(uint8_t const *bytes)
{
	return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

inline ReliefMapProblem ReliefMap_CheckSide(uint32_t width, uint32_t height)
{
	if (width != height)
	{
		return kMapNotSquare;
	}

	if (width < kReliefMapSmallestSide)
	{
		return kMapSideTooSmall;
	}

	if (width > kReliefMapLargestSide)
	{
		return kMapSideTooLarge;
	}

	return (width & (width - 1)) ? kMapSideNotAPowerOfTwo : kMapFine;
}

// How many copies a map has, counting itself, down to 1 x 1. Only for a side that's passed the check above.
inline uint32_t ReliefMap_Levels(uint32_t side)
{
	uint32_t levels = 1;

	while (side > 1)
	{
		side /= 2;
		levels++;
	}

	return levels;
}

// The size of one copy. Only for a level under ReliefMap_Levels.
inline uint32_t ReliefMap_LevelSide(uint32_t side, uint32_t level)
{
	return side >> level;
}

// DXT5 keeps 16 bytes for every block of 4 x 4 pixels, and even a 1 x 1 copy takes a whole block.
inline uint32_t ReliefMap_DxtRows(uint32_t levelSide)
{
	return (levelSide + 3) / 4;
}

inline uint64_t ReliefMap_DxtLevelBytes(uint32_t levelSide)
{
	uint64_t const blocks = ReliefMap_DxtRows(levelSide);
	return blocks * blocks * 16;
}

// All the copies of a map, as DXT5
inline uint64_t ReliefMap_DxtBytes(uint32_t side)
{
	uint64_t bytes = 0;
	uint32_t const levels = ReliefMap_Levels(side);

	for (uint32_t level = 0; level < levels; level++)
	{
		bytes += ReliefMap_DxtLevelBytes(ReliefMap_LevelSide(side, level));
	}

	return bytes;
}

// All the copies of a map, at four bytes a pixel, which is what a .rim map becomes
inline uint64_t ReliefMap_PlainBytes(uint32_t side)
{
	uint64_t bytes = 0;
	uint32_t const levels = ReliefMap_Levels(side);

	for (uint32_t level = 0; level < levels; level++)
	{
		uint64_t const levelSide = ReliefMap_LevelSide(side, level);
		bytes += levelSide * levelSide * 4;
	}

	return bytes;
}

// ---- name_Normal.dds

// Gives back the side if the problem is kMapFine. The copies then start at kReliefMapDdsHeadingBytes, largest first, each ReliefMap_DxtLevelBytes long.
inline ReliefMapProblem ReliefMap_CheckDds(uint8_t const *file, uint64_t fileBytes, uint32_t *side)
{
	if (fileBytes > kReliefMapMostFileBytes)
	{
		return kMapFileTooLong;
	}

	if (fileBytes < kReliefMapDdsHeadingBytes)
	{
		return kMapShorterThanItsHeading;
	}

	if (memcmp(file, "DDS ", 4) != 0)
	{
		return kMapNotADds;
	}

	// these two are the sizes of parts of the heading, which never change
	if (ReliefMap_Little32(file + 4) != 124 || ReliefMap_Little32(file + 76) != 32)
	{
		return kMapHeadingWrong;
	}

	uint32_t const height = ReliefMap_Little32(file + 12);
	uint32_t const width = ReliefMap_Little32(file + 16);
	ReliefMapProblem const sizeProblem = ReliefMap_CheckSide(width, height);

	if (sizeProblem != kMapFine)
	{
		return sizeProblem;
	}

	// 4 is the flag that says the format is named by the four letters that follow
	if (!(ReliefMap_Little32(file + 80) & 4) || memcmp(file + 84, "DXT5", 4) != 0)
	{
		return kMapFormatWrong;
	}

	if (ReliefMap_Little32(file + 24) > 1 || ReliefMap_Little32(file + 112) != 0)
	{
		return kMapNotAFlatPicture;
	}

	if (ReliefMap_Little32(file + 28) != ReliefMap_Levels(width))
	{
		return kMapSmallerCopiesWrong;
	}

	if (fileBytes != kReliefMapDdsHeadingBytes + ReliefMap_DxtBytes(width))
	{
		return kMapLengthWrong;
	}

	*side = width;
	return kMapFine;
}

// ---- name_Normal.rim

struct ReliefMapRim
{
	uint32_t side;
	uint32_t planes;
	uint32_t packing;            // 0 is plain, 1 is packed in runs
	uint8_t const *colours;      // three bytes each
	uint32_t numColours;
	uint8_t const *body;
	uint64_t bodyBytes;
};

// Finds the parts of a .rim, in the order the game's own pictures have them. A picture of the game's own starts with parts that say nothing about the picture, and those are stepped over.
// Each step moves on through the file, so this always ends. Nothing is looked at that's past the end of the file.
inline ReliefMapProblem ReliefMap_FindRimParts(uint8_t const *file, uint64_t fileBytes, ReliefMapRim *rim)
{
	if (fileBytes > kReliefMapMostFileBytes)
	{
		return kMapFileTooLong;
	}

	bool haveHeading = false;
	uint32_t width = 0, height = 0;
	uint64_t place = 0;

	rim->colours = 0;
	rim->numColours = 0;

	for (;;)
	{
		if (fileBytes < 8 || place > fileBytes - 8)
		{
			return haveHeading ? kMapNoPicture : kMapNoHeading;
		}

		uint8_t const *part = file + place;
		uint32_t const size = ReliefMap_Big32(part + 4);

		if (memcmp(part, "LIST", 4) == 0 || memcmp(part, "FORM", 4) == 0)
		{
			place += 8;
		}
		else if (memcmp(part, "ILBMPROP", 8) == 0)
		{
			place += 12;
		}
		else if (memcmp(part, "ILBMTRAN", 8) == 0)
		{
			place += 8 + 4 + 8;
		}
		else if (memcmp(part, "ILBMBMHD", 8) == 0)
		{
			if (place > fileBytes - 32 || fileBytes < 32 || ReliefMap_Big32(part + 8) != 20)
			{
				return kMapNoHeading;
			}

			width = ((uint32_t)part[12] << 8) | part[13];
			height = ((uint32_t)part[14] << 8) | part[15];
			rim->planes = part[20];
			rim->packing = part[22];
			haveHeading = true;
			place += 32;
		}
		else if (memcmp(part, "CMAP", 4) == 0)
		{
			uint64_t const taken = (uint64_t)size + (size & 1);

			if (taken > fileBytes - place - 8 || size % 3 != 0)
			{
				return kMapColourTableWrong;
			}

			rim->colours = part + 8;
			rim->numColours = size / 3;
			place += 8 + taken;
		}
		else if (memcmp(part, "BODY", 4) == 0)
		{
			if (!haveHeading)
			{
				return kMapNoHeading;
			}

			if (size > fileBytes - place - 8)
			{
				return kMapCutShort;
			}

			rim->body = part + 8;
			rim->bodyBytes = size;
			break;
		}
		else
		{
			return kMapPartsWrong;
		}
	}

	ReliefMapProblem const sizeProblem = ReliefMap_CheckSide(width, height);

	if (sizeProblem != kMapFine)
	{
		return sizeProblem;
	}

	rim->side = width;

	bool const deep = rim->planes == kReliefMapRimDeepPlanes;

	if (!deep && (rim->planes < 1 || rim->planes > 16))
	{
		return kMapPlanesWrong;
	}

	if (rim->packing > 1)
	{
		return kMapPackingWrong;
	}

	// a deep picture's pixels are its colours, and any other's are numbers in the table
	if (deep ? rim->numColours != 0 : (rim->numColours == 0 || rim->numColours > 65536))
	{
		return kMapColourTableWrong;
	}

	// The least a body can be: all of it for a plain picture, and for a packed one, the longest run makes 128 bytes from 2.
	uint64_t const rowBytes = (width + 7) / 8;
	uint64_t const needed = rowBytes * rim->planes * height;

	if (rim->packing == 0 ? rim->bodyBytes < needed : rim->bodyBytes * 64 < needed)
	{
		return kMapCutShort;
	}

	return kMapFine;
}

// One row of one plane. Moves *body on, and takes off *bodyLeft what it used. Unlike the loader for stock pictures, a file that doesn't add up is refused, not patched up.
inline ReliefMapProblem ReliefMap_ReadRow(uint8_t const **body, uint64_t *bodyLeft, uint32_t packing, uint8_t *row, uint32_t rowBytes)
{
	if (packing == 0)
	{
		if (*bodyLeft < rowBytes)
		{
			return kMapCutShort;
		}

		memcpy(row, *body, rowBytes);
		*body += rowBytes;
		*bodyLeft -= rowBytes;
		return kMapFine;
	}

	uint32_t done = 0;

	while (done < rowBytes)
	{
		if (*bodyLeft < 1)
		{
			return kMapCutShort;
		}

		int const value = (int)(int8_t)**body;
		*body += 1;
		*bodyLeft -= 1;

		if (value == -128)
		{
			continue;    // this one does nothing, and each uses up a byte, so these can't go on for ever
		}

		uint32_t const count = (value >= 0) ? (uint32_t)value + 1 : (uint32_t)(1 - value);
		uint64_t const taken = (value >= 0) ? count : 1;

		if (count > rowBytes - done)
		{
			return kMapRunPastRow;
		}

		if (*bodyLeft < taken)
		{
			return kMapCutShort;
		}

		if (value >= 0)
		{
			memcpy(row + done, *body, count);
		}
		else
		{
			memset(row + done, **body, count);
		}

		*body += taken;
		*bodyLeft -= taken;
		done += count;
	}

	return kMapFine;
}

// Writes a pixel as the map's layout has it. In memory that's blue, green, red, alpha, and red and blue only matter to someone looking at it: red is x again, and blue is z.
inline void ReliefMap_PutPixel(uint8_t *pixel, uint8_t x, uint8_t y, uint8_t z)
{
	pixel[0] = z;
	pixel[1] = y;
	pixel[2] = x;
	pixel[3] = x;
}

// The largest row a map can have, for all its planes
uint32_t const kReliefMapMostRowBytes = (kReliefMapLargestSide / 8) * kReliefMapRimDeepPlanes;

// The whole picture, from what ReliefMap_FindRimParts found. pixels has room for side * side * 4 bytes.
inline ReliefMapProblem ReliefMap_DecodeRim(ReliefMapRim const &rim, uint8_t *pixels)
{
	uint8_t rows[kReliefMapMostRowBytes];
	uint32_t const rowBytes = (rim.side + 7) / 8;
	uint8_t const *body = rim.body;
	uint64_t bodyLeft = rim.bodyBytes;

	for (uint32_t y = 0; y < rim.side; y++)
	{
		for (uint32_t plane = 0; plane < rim.planes; plane++)
		{
			ReliefMapProblem const problem = ReliefMap_ReadRow(&body, &bodyLeft, rim.packing, rows + plane * rowBytes, rowBytes);

			if (problem != kMapFine)
			{
				return problem;
			}
		}

		uint8_t *pixel = pixels + (size_t)y * rim.side * 4;

		for (uint32_t x = 0; x < rim.side; x++, pixel += 4)
		{
			uint32_t const mask = 128u >> (x % 8);
			uint32_t const across = x / 8;
			uint32_t number = 0;

			for (uint32_t plane = 0; plane < rim.planes; plane++)
			{
				if (rows[across + plane * rowBytes] & mask)
				{
					number |= 1u << plane;
				}
			}

			if (rim.planes == kReliefMapRimDeepPlanes)
			{
				ReliefMap_PutPixel(pixel, (uint8_t)number, (uint8_t)(number >> 8), (uint8_t)(number >> 16));
			}
			else
			{
				if (number >= rim.numColours)
				{
					return kMapColourPastTable;
				}

				uint8_t const *colour = rim.colours + (size_t)number * 3;
				ReliefMap_PutPixel(pixel, colour[0], colour[1], colour[2]);
			}
		}
	}

	return kMapFine;
}

// ---- The smaller copies of a .rim map. A .dds brings its own.

inline void ReliefMap_AddArrow(uint8_t const *pixel, float *sum)
{
	sum[0] += ((float)pixel[2] - 128.0f) / 127.0f;
	sum[1] += ((float)pixel[1] - 128.0f) / 127.0f;
	sum[2] += ((float)pixel[0] - 128.0f) / 127.0f;
}

inline uint8_t ReliefMap_Number(float value)
{
	float const number = 128.0f + 127.0f * value + 0.5f;
	return (number <= 0.0f) ? 0 : (number >= 255.0f) ? 255 : (uint8_t)number;
}

// One copy from the one above it, each pixel from the four above it. Averaging arrows makes them shorter, so each is put back to a length of 1, or a distant wall would look flat. Four that
// cancel out come to a flat pixel.
inline void ReliefMap_MakeSmallerCopy(uint8_t const *from, uint32_t fromSide, uint8_t *to)
{
	uint32_t const toSide = fromSide / 2;

	for (uint32_t y = 0; y < toSide; y++)
	{
		for (uint32_t x = 0; x < toSide; x++)
		{
			float sum[3] = { 0.0f, 0.0f, 0.0f };
			uint8_t const *row0 = from + (size_t)y * 2 * fromSide * 4 + (size_t)x * 2 * 4;
			uint8_t const *row1 = row0 + (size_t)fromSide * 4;

			ReliefMap_AddArrow(row0, sum);
			ReliefMap_AddArrow(row0 + 4, sum);
			ReliefMap_AddArrow(row1, sum);
			ReliefMap_AddArrow(row1 + 4, sum);

			float const length = sqrtf(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
			uint8_t *pixel = to + ((size_t)y * toSide + x) * 4;

			if (length < 0.0001f)
			{
				ReliefMap_PutPixel(pixel, 128, 128, 255);
			}
			else
			{
				ReliefMap_PutPixel(pixel, ReliefMap_Number(sum[0] / length), ReliefMap_Number(sum[1] / length), ReliefMap_Number(sum[2] / length));
			}
		}
	}
}

// ---- Where a picture's map is

// Its own name, with .rim swapped for what's given: "graphics/Common/burn.rim" with "_Normal.dds" gives "graphics/Common/burn_Normal.dds". A picture not named like that can't have one.
inline bool ReliefMap_FileName(std::string const &pictureName, char const *ending, std::string *mapName)
{
	size_t const size = pictureName.size();

	if (size < 5 || pictureName[size - 4] != '.'
		|| (pictureName[size - 3] != 'r' && pictureName[size - 3] != 'R')
		|| (pictureName[size - 2] != 'i' && pictureName[size - 2] != 'I')
		|| (pictureName[size - 1] != 'm' && pictureName[size - 1] != 'M'))
	{
		return false;
	}

	*mapName = pictureName.substr(0, size - 4) + ending;
	return true;
}

#endif
