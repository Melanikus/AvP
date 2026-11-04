// The Huffman test. Written 2026-11-03 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Save games are squeezed down with Huffman codes before they are written to disk, and unpacked again when they are
// loaded. This test gives the game's real compressor data of many shapes and sizes, unpacks the result with both of the
// game's real decoders, and checks that what comes back is exactly what went in.
//
// Compressing does not always make data smaller. If every byte value is about as common as every other, as in random
// data, the codes save nothing and the result comes out bigger than the data it started from. The compressor once
// allowed only for the data's own size, so such data ran past the end of its block of memory. The random cases here are
// the ones that catch that: without the room for it, this program crashes (Windows reports heap corruption).
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file together with the game's real huffman.cpp, using the settings in
// tests/huffman_test.args, which are the game's own. The program gives back a non-zero result if any check failed. It
// needs none of the game's files, and it never opens a file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.hpp"

static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

// The same numbers on every run, so that a failure can be repeated.
static unsigned int randomState = 4242;

static unsigned int RandomNumber()
{
	randomState = randomState * 1103515245u + 12345u;
	return (randomState >> 8) & 0xFFFFFF;
}

// The shapes of data the codes behave differently on.
enum DataShape { EveryByteAlike, RepeatingPattern, OneByteMostlyAndSomeOthers, TextLike, AllOneByte, NumberOfShapes };

static char const * const ShapeNames[] =
{
	"every byte value about as common as the others",
	"a repeating pattern",
	"one byte value far more common than the rest",
	"text",
	"one byte value only",
};

static void FillWithShape(unsigned char * data, int length, int shape)
{
	for (int i = 0; i < length; i++)
	{
		unsigned int const number = RandomNumber();

		switch (shape)
		{
			case EveryByteAlike:             data[i] = (unsigned char)number; break;
			case RepeatingPattern:           data[i] = (unsigned char)(i * 7); break;
			case OneByteMostlyAndSomeOthers: data[i] = (number % 100 < 90) ? 'a' : (unsigned char)(number >> 8); break;
			case TextLike:                   data[i] = (unsigned char)(' ' + number % 64); break;
			default:                         data[i] = 0; break;
		}
	}
}

// Packs the data and unpacks it with both decoders. True if both give back exactly what went in.
static bool ComesBackTheSame(unsigned char const * original, int length, int * compressedSize)
{
	unsigned char * data = (unsigned char *)malloc(length);
	memcpy(data, original, length);

	HuffmanPackage * package = HuffmanCompression(data, length);
	*compressedSize = package->CompressedDataSize;

	int const packageSize = (int)sizeof(HuffmanPackage) + package->CompressedDataSize;
	char * unpacked = HuffmanDecompress(package);
	char * unpackedChecked = HuffmanDecompressChecked(package, packageSize);

	bool const same = unpacked && unpackedChecked && !memcmp(unpacked, original, length) && !memcmp(unpackedChecked, original, length);

	free(unpackedChecked);
	free(unpacked);
	free(package);
	free(data);

	return same;
}

int main()
{
	int const sizes[] = { 1, 2, 3, 15, 16, 17, 255, 256, 257, 4096, 65536, 300000 };
	int const numberOfSizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

	for (int shape = 0; shape < NumberOfShapes; shape++)
	{
		bool allSame = true;

		for (int s = 0; s < numberOfSizes; s++)
		{
			int const length = sizes[s];
			unsigned char * original = (unsigned char *)malloc(length);
			FillWithShape(original, length, shape);

			int compressedSize = 0;
			if (!ComesBackTheSame(original, length, &compressedSize))
			{
				allSame = false;
			}

			// The case the room in the compressor is there for: the big random data really has to come out bigger.
			if (shape == EveryByteAlike && length == 300000)
			{
				Check(compressedSize > length, "random data does come out bigger than it went in");
			}

			free(original);
		}

		Check(allSame, ShapeNames[shape]);
	}

	if (checksFailed)
	{
		printf("The Huffman test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The Huffman test passed");
	return 0;
}
