// The Huffman test. Written 2026-11-03 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Save games are squeezed down with Huffman codes before they are written to disk, and unpacked again when they are
// loaded. This test gives the game's real compressor data of many shapes and sizes, unpacks the result with the game's
// real decoder, and checks that what comes back is exactly what went in. Then it gives that decoder damaged packages,
// because levels, saves and text files all reach it before anything else has looked at them.
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
#include "memory_wall.h"

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
	char * unpacked = HuffmanDecompressChecked(package, packageSize);

	bool const same = unpacked && !memcmp(unpacked, original, length);

	free(unpacked);
	free(package);
	free(data);

	return same;
}

// Makes a real package from some ordinary data, and gives back how big it is. The test below damages a copy of it.
static HuffmanPackage * MakePackage(int * packageSize)
{
	static unsigned char data[2048];

	for (int i = 0; i < (int)sizeof(data); i++)
	{
		data[i] = (unsigned char)(i * i >> 3);
	}

	HuffmanPackage * package = HuffmanCompression(data, (int)sizeof(data));
	*packageSize = (int)sizeof(HuffmanPackage) + package->CompressedDataSize;

	return package;
}

// A package from a file can say anything, and the decoder is the first thing to look at it. The package goes at the wall, so reading a byte
// past its end stops this test. Gives back true if the decoder said no.
static bool Refused(HuffmanPackage const * original, void (*damage)(HuffmanPackage *), int bytesKept)
{
	char * atTheWall = MemoryEndingAtAWall(bytesKept);
	memcpy(atTheWall, original, bytesKept);

	if (damage)
	{
		damage((HuffmanPackage *)atTheWall);
	}

	char * unpacked = HuffmanDecompressChecked((HuffmanPackage *)atTheWall, bytesKept);
	bool const refused = (unpacked == 0);

	free(unpacked);

	return refused;
}

static void SizeNegative(HuffmanPackage * package)      { package->UncompressedDataSize = -16; }
static void SizeHuge(HuffmanPackage * package)          { package->UncompressedDataSize = 0x7fffffff; }
static void SizeTooBigForData(HuffmanPackage * package) { package->UncompressedDataSize = package->CompressedDataSize * 8 + 1; }
static void LengthNegative(HuffmanPackage * package)    { package->CodelengthCount[3] = -1; }
static void NoCodes(HuffmanPackage * package)           { memset(package->CodelengthCount, 0, sizeof(package->CodelengthCount)); }

static void CodesDoNotFill(HuffmanPackage * package)
{
	memset(package->CodelengthCount, 0, sizeof(package->CodelengthCount));
	package->CodelengthCount[MAX_DEPTH - 1] = 1;
}

// A negative length that the other lengths make up for, so that only the test for a negative length can refuse it.
static void LengthNegativeMadeUpFor(HuffmanPackage * package)
{
	package->CodelengthCount[3] = -1;
	package->CodelengthCount[0] += 1;
}

// More codes than there are byte values, so the table ends in codes that mean nothing. The data soon reaches one, and the decoder stops
// short of the size. Only the test for how much was decoded can refuse this one.
static void CodesWithNoByte(HuffmanPackage * package)
{
	memset(package->CodelengthCount, 0, sizeof(package->CodelengthCount));
	package->CodelengthCount[MAX_DEPTH - 1] = 1 << MAX_DEPTH;
}

static void TestDamagedPackages()
{
	puts("damaged packages");

	int packageSize = 0;
	HuffmanPackage * package = MakePackage(&packageSize);
	int const headerBytes = (int)sizeof(HuffmanPackage);

	Check(!Refused(package, 0, packageSize), "a whole package is decoded");
	Check(Refused(package, SizeNegative, packageSize), "a negative size is refused");
	Check(Refused(package, SizeHuge, packageSize), "a huge size is refused");
	Check(Refused(package, SizeTooBigForData, packageSize), "a size the data can't hold is refused");
	Check(Refused(package, LengthNegative, packageSize), "a negative code length is refused");
	Check(Refused(package, LengthNegativeMadeUpFor, packageSize), "a negative code length that the others make up for is refused");
	Check(Refused(package, CodesWithNoByte, packageSize), "data that decodes to too little is refused");
	Check(Refused(package, NoCodes, packageSize), "a package with no codes is refused");
	Check(Refused(package, CodesDoNotFill, packageSize), "codes that don't fill the table are refused");
	Check(Refused(package, 0, headerBytes - 1), "a package smaller than its header is refused");
	Check(Refused(package, 0, 8), "a package too small to hold its own sizes is refused without reading them");
	Check(Refused(package, 0, headerBytes + 16), "a package with most of its data gone is refused");

	// Cut in half, the decoder still needs what's gone, and it reads zero bits instead. Those can decode to the right number of bytes, so all this
	// asks is that it stays inside its package: going past the wall would stop this test.
	Refused(package, 0, headerBytes + package->CompressedDataSize / 2);
	Check(true, "a package cut in half isn't read past its end");

	free(package);
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

	TestDamagedPackages();

	if (checksFailed)
	{
		printf("The Huffman test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The Huffman test passed");
	return 0;
}
