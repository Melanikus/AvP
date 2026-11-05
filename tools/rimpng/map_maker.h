// Turns a picture of normals into a map file the game can read, for tools/rimpng. Written 2026-11-05 by Melanikus for the AvP port.
//
// 3dc/relief_map_file.h says what a map is. This only makes them. It works from arrows, not colours: each pixel of a normal map is an arrow saying which way the surface faces, so
// averaging or shrinking has to keep it an arrow one long, or the surface comes out flatter than it was drawn.

#ifndef MAP_MAKER_H
#define MAP_MAKER_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "relief_map_file.h"
#include "png_read.h"

struct MapChoices
{
	bool flipGreen;     // for images whose green means "up" (OpenGL's way): the game's means "down"
	bool flipRed;       // with flipGreen, this turns every slope round: for a map whose bumps come out as dents
	uint32_t side;      // shrink or stretch to this. 0 keeps the image's own size
};

struct MapReport
{
	uint32_t pixelsMadeFlat;      // too short to say which way they face, such as the middle grey an image can be framed with
	uint32_t pixelsFacingBack;    // pointing into the surface, which a normal map shouldn't, so they now lie along it
};

// An arrow shorter than this says nothing about which way a surface faces. A real normal map's arrows are about 1 long. Grey just under the middle, such as 122, 122, 122, comes out a
// twentieth long, and pointing slightly backwards. Stretched out to a length of 1, it would be a surface facing away, so it's made flat instead.
float const kMapShortestArrow = 0.25f;

typedef std::vector<uint8_t> MapBytes;

// Gives back 0 if it worked, or words saying why it didn't. top gets the map's largest copy, four bytes a pixel, as relief_map_file.h lays them out.
inline char const *Map_MakeTop(PngPicture const &image, MapChoices const &choices, uint32_t *side, MapBytes *top, MapReport *report)
{
	uint32_t const wanted = choices.side ? choices.side : image.width;
	ReliefMapProblem const problem = choices.side ? ReliefMap_CheckSide(wanted, wanted) : ReliefMap_CheckSide(image.width, image.height);

	if (problem != kMapFine)
	{
		return ReliefMap_ProblemWords(problem);
	}

	*side = wanted;
	report->pixelsFacingBack = 0;
	report->pixelsMadeFlat = 0;
	top->resize((size_t)wanted * wanted * 4);

	for (uint32_t y = 0; y < wanted; y++)
	{
		uint32_t const fromY0 = (uint32_t)((uint64_t)y * image.height / wanted);
		uint32_t const nextY = (uint32_t)(((uint64_t)y + 1) * image.height / wanted);
		uint32_t const fromY1 = (nextY > fromY0) ? nextY : fromY0 + 1;

		for (uint32_t x = 0; x < wanted; x++)
		{
			uint32_t const fromX0 = (uint32_t)((uint64_t)x * image.width / wanted);
			uint32_t const nextX = (uint32_t)(((uint64_t)x + 1) * image.width / wanted);
			uint32_t const fromX1 = (nextX > fromX0) ? nextX : fromX0 + 1;

			float sum[3] = { 0.0f, 0.0f, 0.0f };

			for (uint32_t fromY = fromY0; fromY < fromY1; fromY++)
			{
				uint8_t const *from = &image.rgb[((size_t)fromY * image.width + fromX0) * 3];

				for (uint32_t fromX = fromX0; fromX < fromX1; fromX++, from += 3)
				{
					float const across = ((float)from[0] - 128.0f) / 127.0f;
					float const down = ((float)from[1] - 128.0f) / 127.0f;
					float const out = ((float)from[2] - 128.0f) / 127.0f;

					if (sqrtf(across * across + down * down + out * out) < kMapShortestArrow)
					{
						sum[2] += 1.0f;
						report->pixelsMadeFlat++;
						continue;
					}

					// One that points into the surface keeps its lean, and lies along the surface, so a steep slope doesn't get a hole in it.
					float const outward = (out < 0.0f) ? 0.0f : out;
					float const length = sqrtf(across * across + down * down + outward * outward);

					report->pixelsFacingBack += (out < 0.0f) ? 1 : 0;

					if (length >= 0.0001f)
					{
						sum[0] += (choices.flipRed ? -across : across) / length;
						sum[1] += (choices.flipGreen ? -down : down) / length;
						sum[2] += outward / length;
					}
					else
					{
						sum[2] += 1.0f;
						report->pixelsMadeFlat++;
					}
				}
			}

			float const length = sqrtf(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
			uint8_t *pixel = &(*top)[((size_t)y * wanted + x) * 4];

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

	return 0;
}

// A depth map, which isn't arrows, so it's kept as it is. Nothing in the game reads one yet. Gives back 0 if it worked, or words saying why it didn't.
inline char const *Map_MakeDepthTop(PngPicture const &image, uint32_t *side, MapBytes *top)
{
	ReliefMapProblem const problem = ReliefMap_CheckSide(image.width, image.height);

	if (problem != kMapFine)
	{
		return ReliefMap_ProblemWords(problem);
	}

	*side = image.width;
	top->resize((size_t)image.width * image.height * 4);

	for (size_t i = 0; i < (size_t)image.width * image.height; i++)
	{
		ReliefMap_PutPixel(&(*top)[i * 4], image.rgb[i * 3], image.rgb[i * 3 + 1], image.rgb[i * 3 + 2]);
	}

	return 0;
}

// Every copy of a map, largest first, each worked out from the one above
inline std::vector<MapBytes> Map_MakeAllCopies(MapBytes const &top, uint32_t side)
{
	std::vector<MapBytes> copies(1, top);
	uint32_t const levels = ReliefMap_Levels(side);

	for (uint32_t level = 1; level < levels; level++)
	{
		uint32_t const levelSide = ReliefMap_LevelSide(side, level);
		MapBytes smaller((size_t)levelSide * levelSide * 4);
		ReliefMap_MakeSmallerCopy(&copies[level - 1][0], levelSide * 2, &smaller[0]);
		copies.push_back(smaller);
	}

	return copies;
}

// ---- DXT5
//
// A block of 4 x 4 pixels takes 16 bytes. The first 8 hold alpha: two numbers, and three bits for each pixel saying where it is between them. The other 8 hold colour: two 16-bit colours,
// and two bits for each pixel. x goes in alpha, which has the finer steps. y goes in green, the only colour with six bits. Red and blue aren't used.

inline uint32_t Map_GreenFromSix(uint32_t six)
{
	return (six << 2) | (six >> 4);
}

// The eight places an alpha can be. a0 is above a1, which is what makes it eight and not six.
inline uint32_t Map_AlphaAt(uint32_t a0, uint32_t a1, uint32_t index)
{
	return (index == 0) ? a0 : (index == 1) ? a1 : ((8 - index) * a0 + (index - 1) * a1) / 7;
}

inline uint32_t Map_GreenAt(uint32_t g0, uint32_t g1, uint32_t index)
{
	return (index == 0) ? g0 : (index == 1) ? g1 : (index == 2) ? (2 * g0 + g1) / 3 : (g0 + 2 * g1) / 3;
}

inline uint32_t Map_Distance(uint32_t a, uint32_t b)
{
	return (a > b) ? a - b : b - a;
}

inline void Map_EncodeBlock(uint8_t const *pixels, uint32_t levelSide, uint32_t blockX, uint32_t blockY, uint8_t *block)
{
	uint8_t xs[16], ys[16];

	// a copy smaller than a block repeats its last pixels to fill it
	for (uint32_t y = 0; y < 4; y++)
	{
		for (uint32_t x = 0; x < 4; x++)
		{
			uint32_t const atX = (blockX * 4 + x < levelSide) ? blockX * 4 + x : levelSide - 1;
			uint32_t const atY = (blockY * 4 + y < levelSide) ? blockY * 4 + y : levelSide - 1;
			uint8_t const *pixel = pixels + ((size_t)atY * levelSide + atX) * 4;
			xs[y * 4 + x] = pixel[3];
			ys[y * 4 + x] = pixel[1];
		}
	}

	uint32_t lowX = 255, highX = 0, lowY = 255, highY = 0;

	for (int i = 0; i < 16; i++)
	{
		lowX = (xs[i] < lowX) ? xs[i] : lowX;
		highX = (xs[i] > highX) ? xs[i] : highX;
		lowY = (ys[i] < lowY) ? ys[i] : lowY;
		highY = (ys[i] > highY) ? ys[i] : highY;
	}

	// two different numbers, or a block of one value would land in the six-place kind
	if (highX == lowX)
	{
		if (highX < 255) highX++; else lowX--;
	}

	block[0] = (uint8_t)highX;
	block[1] = (uint8_t)lowX;

	uint64_t alphaBits = 0;

	for (int i = 0; i < 16; i++)
	{
		uint32_t best = 0, bestDistance = 1000;

		for (uint32_t index = 0; index < 8; index++)
		{
			uint32_t const distance = Map_Distance(Map_AlphaAt(highX, lowX, index), xs[i]);

			if (distance < bestDistance)
			{
				best = index;
				bestDistance = distance;
			}
		}

		alphaBits |= (uint64_t)best << (i * 3);
	}

	for (int i = 0; i < 6; i++)
	{
		block[2 + i] = (uint8_t)(alphaBits >> (i * 8));
	}

	// Six bits can't hold every green: 128, a flat pixel's, falls between two. So the two ends always stand either side, and the steps between them get closer than either end could.
	uint32_t highSix = (highY * 63 + 254) / 255;
	uint32_t lowSix = (lowY * 63) / 255;

	if (highSix == lowSix)
	{
		if (highSix < 63) highSix++; else lowSix--;
	}

	uint32_t const colour0 = highSix << 5;
	uint32_t const colour1 = lowSix << 5;
	uint32_t const green0 = Map_GreenFromSix(highSix);
	uint32_t const green1 = Map_GreenFromSix(lowSix);

	block[8] = (uint8_t)colour0;
	block[9] = (uint8_t)(colour0 >> 8);
	block[10] = (uint8_t)colour1;
	block[11] = (uint8_t)(colour1 >> 8);

	uint32_t greenBits = 0;

	for (int i = 0; i < 16; i++)
	{
		uint32_t best = 0, bestDistance = 1000;

		for (uint32_t index = 0; index < 4; index++)
		{
			uint32_t const distance = Map_Distance(Map_GreenAt(green0, green1, index), ys[i]);

			if (distance < bestDistance)
			{
				best = index;
				bestDistance = distance;
			}
		}

		greenBits |= best << (i * 2);
	}

	for (int i = 0; i < 4; i++)
	{
		block[12 + i] = (uint8_t)(greenBits >> (i * 8));
	}
}

// What a card would read back for one pixel of a block: x and y
inline void Map_DecodePixel(uint8_t const *block, uint32_t place, uint8_t *x, uint8_t *y)
{
	uint64_t alphaBits = 0;

	for (int i = 0; i < 6; i++)
	{
		alphaBits |= (uint64_t)block[2 + i] << (i * 8);
	}

	*x = (uint8_t)Map_AlphaAt(block[0], block[1], (uint32_t)(alphaBits >> (place * 3)) & 7);

	uint32_t const colour0 = block[8] | ((uint32_t)block[9] << 8);
	uint32_t const colour1 = block[10] | ((uint32_t)block[11] << 8);
	uint32_t const greenBits = block[12] | ((uint32_t)block[13] << 8) | ((uint32_t)block[14] << 16) | ((uint32_t)block[15] << 24);

	*y = (uint8_t)Map_GreenAt(Map_GreenFromSix((colour0 >> 5) & 63), Map_GreenFromSix((colour1 >> 5) & 63), (greenBits >> (place * 2)) & 3);
}

inline void Map_PutLittle32(MapBytes &to, size_t at, uint32_t value)
{
	to[at] = (uint8_t)value;
	to[at + 1] = (uint8_t)(value >> 8);
	to[at + 2] = (uint8_t)(value >> 16);
	to[at + 3] = (uint8_t)(value >> 24);
}

inline MapBytes Map_MakeDds(std::vector<MapBytes> const &copies, uint32_t side)
{
	MapBytes file((size_t)kReliefMapDdsHeadingBytes + (size_t)ReliefMap_DxtBytes(side), 0);

	memcpy(&file[0], "DDS ", 4);
	Map_PutLittle32(file, 4, 124);
	Map_PutLittle32(file, 8, 0x000A1007);       // says which of these numbers mean anything: the ones below
	Map_PutLittle32(file, 12, side);
	Map_PutLittle32(file, 16, side);
	Map_PutLittle32(file, 20, (uint32_t)ReliefMap_DxtLevelBytes(side));
	Map_PutLittle32(file, 28, ReliefMap_Levels(side));
	Map_PutLittle32(file, 76, 32);
	Map_PutLittle32(file, 80, 4);
	memcpy(&file[84], "DXT5", 4);
	Map_PutLittle32(file, 108, 0x00401008);     // a texture with smaller copies

	size_t place = kReliefMapDdsHeadingBytes;

	for (uint32_t level = 0; level < copies.size(); level++)
	{
		uint32_t const levelSide = ReliefMap_LevelSide(side, level);
		uint32_t const blocks = ReliefMap_DxtRows(levelSide);

		for (uint32_t blockY = 0; blockY < blocks; blockY++)
		{
			for (uint32_t blockX = 0; blockX < blocks; blockX++, place += 16)
			{
				Map_EncodeBlock(&copies[level][0], levelSide, blockX, blockY, &file[place]);
			}
		}
	}

	return file;
}

// ---- .rim

inline void Map_AddBig32(MapBytes &to, uint32_t value)
{
	to.push_back((uint8_t)(value >> 24));
	to.push_back((uint8_t)(value >> 16));
	to.push_back((uint8_t)(value >> 8));
	to.push_back((uint8_t)value);
}

// Red, green and blue, 8 planes each, lowest bit first, with no colour table and nothing packed
inline MapBytes Map_MakeRim(MapBytes const &top, uint32_t side)
{
	uint32_t const rowBytes = (side + 7) / 8;
	uint32_t const bodyBytes = rowBytes * kReliefMapRimDeepPlanes * side;
	MapBytes file;

	file.reserve((size_t)bodyBytes + 64);
	file.insert(file.end(), { 'F', 'O', 'R', 'M' });
	Map_AddBig32(file, 4 + 8 + 20 + 8 + bodyBytes);
	file.insert(file.end(), { 'I', 'L', 'B', 'M', 'B', 'M', 'H', 'D' });
	Map_AddBig32(file, 20);
	file.push_back((uint8_t)(side >> 8));
	file.push_back((uint8_t)side);
	file.push_back((uint8_t)(side >> 8));
	file.push_back((uint8_t)side);
	Map_AddBig32(file, 0);                               // where it goes on a page, which means nothing here
	file.push_back((uint8_t)kReliefMapRimDeepPlanes);
	file.push_back(0);                                   // no see-through colour
	file.push_back(0);                                   // not packed
	file.push_back(0);
	file.push_back(0);                                   // the see-through colour, which it doesn't have
	file.push_back(0);
	file.push_back(1);                                   // pixels are square
	file.push_back(1);
	file.push_back((uint8_t)(side >> 8));
	file.push_back((uint8_t)side);
	file.push_back((uint8_t)(side >> 8));
	file.push_back((uint8_t)side);
	file.insert(file.end(), { 'B', 'O', 'D', 'Y' });
	Map_AddBig32(file, bodyBytes);

	size_t place = file.size();
	file.resize(place + bodyBytes, 0);

	static int const whereInPixel[3] = { 2, 1, 0 };     // red, green and blue, as the pixel holds them

	for (uint32_t y = 0; y < side; y++)
	{
		uint8_t const *row = &top[(size_t)y * side * 4];

		for (uint32_t plane = 0; plane < kReliefMapRimDeepPlanes; plane++, place += rowBytes)
		{
			int const channel = whereInPixel[plane / 8];
			uint32_t const bit = plane % 8;

			for (uint32_t x = 0; x < side; x++)
			{
				if ((row[(size_t)x * 4 + channel] >> bit) & 1)
				{
					file[place + x / 8] |= (uint8_t)(128u >> (x % 8));
				}
			}
		}
	}

	return file;
}

// ---- Reading back what was made, with the same checks the game uses. Gives back 0 if it's good, or words saying why not, and the most any x or y came out different by.

inline char const *Map_CheckDds(MapBytes const &file, MapBytes const &top, uint32_t side, uint32_t *largestError)
{
	uint32_t readSide = 0;
	ReliefMapProblem const problem = ReliefMap_CheckDds(&file[0], file.size(), &readSide);

	if (problem != kMapFine)
	{
		return ReliefMap_ProblemWords(problem);
	}

	if (readSide != side)
	{
		return "it came back a different size";
	}

	*largestError = 0;
	uint8_t const *blocks = &file[kReliefMapDdsHeadingBytes];

	for (uint32_t y = 0; y < side; y++)
	{
		for (uint32_t x = 0; x < side; x++)
		{
			uint8_t readX, readY;
			uint8_t const *pixel = &top[((size_t)y * side + x) * 4];

			Map_DecodePixel(blocks + ((size_t)(y / 4) * (side / 4) + x / 4) * 16, (y % 4) * 4 + (x % 4), &readX, &readY);

			uint32_t const errorX = Map_Distance(readX, pixel[3]);
			uint32_t const errorY = Map_Distance(readY, pixel[1]);
			*largestError = (errorX > *largestError) ? errorX : *largestError;
			*largestError = (errorY > *largestError) ? errorY : *largestError;
		}
	}

	return 0;
}

inline char const *Map_CheckRim(MapBytes const &file, MapBytes const &top, uint32_t side)
{
	ReliefMapRim rim;
	ReliefMapProblem problem = ReliefMap_FindRimParts(&file[0], file.size(), &rim);

	if (problem != kMapFine)
	{
		return ReliefMap_ProblemWords(problem);
	}

	if (rim.side != side)
	{
		return "it came back a different size";
	}

	MapBytes pixels((size_t)side * side * 4);
	problem = ReliefMap_DecodeRim(rim, &pixels[0]);

	if (problem != kMapFine)
	{
		return ReliefMap_ProblemWords(problem);
	}

	return (pixels == top) ? 0 : "it came back different";
}

#endif
