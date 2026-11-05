// The test for 3dc/relief_map_file.h, which decides whether a hand-made map's file can be used. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Good files of both kinds, built byte by byte. Then a bad file for every reason there is to refuse one, each checked for that exact reason, because a check further on could otherwise
// hide a check that's missing. Then a great many damaged copies, with memory watched on both sides of what's written. The real files a game would load aren't here, because they aren't in
// the repository.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this on its own. It uses no files and no graphics card. The program gives back a non-zero result if anything failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "relief_map_file.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

typedef std::vector<uint8_t> Bytes;

static void Put32(Bytes &to, size_t at, uint32_t value)
{
	to[at] = (uint8_t)value;
	to[at + 1] = (uint8_t)(value >> 8);
	to[at + 2] = (uint8_t)(value >> 16);
	to[at + 3] = (uint8_t)(value >> 24);
}

// A correct DDS of this side. Its pixels are just bytes.
static Bytes GoodDds(uint32_t side)
{
	Bytes file((size_t)kReliefMapDdsHeadingBytes + (size_t)ReliefMap_DxtBytes(side), 0x5A);
	memset(&file[0], 0, kReliefMapDdsHeadingBytes);
	memcpy(&file[0], "DDS ", 4);
	Put32(file, 4, 124);
	Put32(file, 8, 0x000A1007);    // what the heading holds, as tools usually write it
	Put32(file, 12, side);
	Put32(file, 16, side);
	Put32(file, 28, ReliefMap_Levels(side));
	Put32(file, 76, 32);
	Put32(file, 80, 4);
	memcpy(&file[84], "DXT5", 4);
	Put32(file, 108, 0x00401008);
	return file;
}

static ReliefMapProblem DdsProblem(Bytes const &file, uint32_t *side = 0)
{
	uint32_t ignored = 0;
	return ReliefMap_CheckDds(file.empty() ? (uint8_t const *)"" : &file[0], file.size(), side ? side : &ignored);
}

static void CheckProblem(ReliefMapProblem got, ReliefMapProblem wanted, std::string const &what)
{
	Check(got == wanted, what + " is refused because " + ReliefMap_ProblemWords(wanted));
}

static void CheckTheSums()
{
	printf("The sums:\n");

	Check(ReliefMap_Levels(4) == 3 && ReliefMap_Levels(128) == 8 && ReliefMap_Levels(2048) == 12, "how many copies a map has, down to 1 x 1");
	Check(ReliefMap_LevelSide(2048, 0) == 2048 && ReliefMap_LevelSide(2048, 11) == 1 && ReliefMap_LevelSide(4, 2) == 1, "the size of each copy");
	Check(ReliefMap_DxtLevelBytes(1) == 16 && ReliefMap_DxtLevelBytes(4) == 16 && ReliefMap_DxtLevelBytes(5) == 64 && ReliefMap_DxtLevelBytes(8) == 64, "even the smallest copy takes a whole block");
	Check(ReliefMap_DxtBytes(4) == 16 + 16 + 16, "all the copies of a 4 x 4 as DXT5");
	Check(ReliefMap_PlainBytes(4) == 4 * (16 + 4 + 1), "all the copies of a 4 x 4 at four bytes a pixel");
	Check(ReliefMap_DxtBytes(2048) > 5500000 && ReliefMap_DxtBytes(2048) < 5700000, "a 2048 is about 5.6 MB as DXT5");
	Check(ReliefMap_PlainBytes(2048) > 22000000 && ReliefMap_PlainBytes(2048) < 22500000, "and about 22 MB at four bytes a pixel");

	// the longest a good file can be, against the limit that's checked before any file is read
	uint64_t const deepPlain = 256ull * 24 * 2048;
	Check(kReliefMapDdsHeadingBytes + ReliefMap_DxtBytes(2048) < kReliefMapMostFileBytes, "the largest .dds is under the limit on a file's length");
	Check(deepPlain + 1024 < kReliefMapMostFileBytes, "and so is the largest plain .rim");
	Check(kReliefMapMostRowBytes == 256 * 24, "the room for a row is the largest row of the most planes");
}

static void CheckDdsFiles()
{
	printf("name_Normal.dds:\n");

	uint32_t const sides[5] = { 4, 8, 128, 1024, 2048 };

	for (int i = 0; i < 5; i++)
	{
		uint32_t side = 0;
		Bytes const file = GoodDds(sides[i]);
		Check(DdsProblem(file, &side) == kMapFine && side == sides[i], "a good one " + std::to_string(sides[i]) + " a side is taken");
	}

	Bytes const good = GoodDds(128);

	{
		Bytes file = good;
		Put32(file, 24, 1);
		Check(DdsProblem(file) == kMapFine, "a depth of 1 means the same as none");
	}

	CheckProblem(DdsProblem(Bytes()), kMapShorterThanItsHeading, "an empty file");
	CheckProblem(DdsProblem(Bytes(good.begin(), good.begin() + 127)), kMapShorterThanItsHeading, "a heading one byte short");

	{
		// a file past the limit is refused on its length alone, without reading any of it
		uint32_t side = 0;
		CheckProblem(ReliefMap_CheckDds(&good[0], kReliefMapMostFileBytes + 1, &side), kMapFileTooLong, "a file past the limit");
	}

	struct Row { size_t at; uint32_t value; ReliefMapProblem problem; char const *what; };

	Row const rows[] =
	{
		{ 0,   0x20534444 + 1, kMapNotADds,             "a file with the wrong start" },
		{ 4,   123,            kMapHeadingWrong,        "a heading with the wrong size" },
		{ 76,  31,             kMapHeadingWrong,        "a pixel format with the wrong size" },
		{ 12,  64,             kMapNotSquare,           "a shorter one" },
		{ 16,  64,             kMapNotSquare,           "a narrower one" },
		{ 80,  0x41,           kMapFormatWrong,         "one with no four-letter format" },
		{ 84,  0x31545844,     kMapFormatWrong,         "DXT1" },
		{ 84,  0x30315844,     kMapFormatWrong,         "the newer heading, DX10" },
		{ 24,  2,              kMapNotAFlatPicture,     "a volume" },
		{ 112, 0x200,          kMapNotAFlatPicture,     "a cube" },
		{ 28,  0,              kMapSmallerCopiesWrong,  "no smaller copies" },
		{ 28,  1,              kMapSmallerCopiesWrong,  "one copy" },
		{ 28,  7,              kMapSmallerCopiesWrong,  "one copy too few" },
		{ 28,  9,              kMapSmallerCopiesWrong,  "one copy too many" },
		{ 28,  0xFFFFFFFF,     kMapSmallerCopiesWrong,  "the most copies a number can say" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		Bytes file = good;
		Put32(file, rows[i].at, rows[i].value);
		CheckProblem(DdsProblem(file), rows[i].problem, rows[i].what);
	}

	struct SideRow { uint32_t side; ReliefMapProblem problem; };

	SideRow const sideRows[] =
	{
		{ 0, kMapSideTooSmall }, { 1, kMapSideTooSmall }, { 2, kMapSideTooSmall }, { 3, kMapSideTooSmall },
		{ 5, kMapSideNotAPowerOfTwo }, { 100, kMapSideNotAPowerOfTwo }, { 2047, kMapSideNotAPowerOfTwo },
		{ 2049, kMapSideTooLarge }, { 4096, kMapSideTooLarge }, { 0x80000000u, kMapSideTooLarge }, { 0xFFFFFFFFu, kMapSideTooLarge },
	};

	for (size_t i = 0; i < sizeof(sideRows) / sizeof(sideRows[0]); i++)
	{
		Bytes file = good;
		Put32(file, 12, sideRows[i].side);
		Put32(file, 16, sideRows[i].side);
		CheckProblem(DdsProblem(file), sideRows[i].problem, "a side of " + std::to_string(sideRows[i].side));
	}

	{
		Bytes shortFile = good;
		shortFile.pop_back();
		CheckProblem(DdsProblem(shortFile), kMapLengthWrong, "a file one byte short");

		Bytes longFile = good;
		longFile.push_back(0);
		CheckProblem(DdsProblem(longFile), kMapLengthWrong, "a file one byte long");
	}
}

// ---- .rim files

static void AddBig32(Bytes &to, uint32_t value)
{
	to.push_back((uint8_t)(value >> 24));
	to.push_back((uint8_t)(value >> 16));
	to.push_back((uint8_t)(value >> 8));
	to.push_back((uint8_t)value);
}

static void AddText(Bytes &to, char const *text)
{
	to.insert(to.end(), text, text + strlen(text));
}

struct Rim
{
	uint32_t width, height, planes, packing;
	bool stockOrder;
	Bytes colours, body;

	Rim(uint32_t side, uint32_t p, uint32_t pack) : width(side), height(side), planes(p), packing(pack), stockOrder(false) {}

	Bytes File() const
	{
		Bytes file;

		if (stockOrder)
		{
			AddText(file, "LIST");
			AddBig32(file, 0);
			AddText(file, "ILBMPROP");
			AddBig32(file, 0);
			AddText(file, "ILBMTRAN");
			AddBig32(file, 4);
			AddBig32(file, 0);
			AddBig32(file, 0);
		}

		AddText(file, "FORM");
		AddBig32(file, 0);
		AddText(file, "ILBMBMHD");
		AddBig32(file, 20);
		file.push_back((uint8_t)(width >> 8));
		file.push_back((uint8_t)width);
		file.push_back((uint8_t)(height >> 8));
		file.push_back((uint8_t)height);
		AddBig32(file, 0);
		file.push_back((uint8_t)planes);
		file.push_back(0);
		file.push_back((uint8_t)packing);
		file.push_back(0);
		AddBig32(file, 0);
		AddBig32(file, 0);

		if (!colours.empty())
		{
			AddText(file, "CMAP");
			AddBig32(file, (uint32_t)colours.size());
			file.insert(file.end(), colours.begin(), colours.end());

			if (colours.size() % 2)
			{
				file.push_back(0);
			}
		}

		AddText(file, "BODY");
		AddBig32(file, (uint32_t)body.size());
		file.insert(file.end(), body.begin(), body.end());
		return file;
	}
};

static ReliefMapProblem RimProblem(Bytes const &file, std::vector<uint8_t> *pixels = 0)
{
	ReliefMapRim rim;
	ReliefMapProblem const found = ReliefMap_FindRimParts(file.empty() ? (uint8_t const *)"" : &file[0], file.size(), &rim);

	if (found != kMapFine)
	{
		return found;
	}

	std::vector<uint8_t> memory((size_t)rim.side * rim.side * 4);
	ReliefMapProblem const decoded = ReliefMap_DecodeRim(rim, &memory[0]);

	if (pixels)
	{
		pixels->swap(memory);
	}

	return decoded;
}

// x, y and z of every pixel are these, for a deep picture of side 8: one byte a row in each plane
static Bytes DeepBody(uint8_t x, uint8_t y, uint8_t z, uint32_t side)
{
	Bytes body;
	uint32_t const rowBytes = (side + 7) / 8;

	for (uint32_t row = 0; row < side; row++)
	{
		uint8_t const values[3] = { x, y, z };

		for (int channel = 0; channel < 3; channel++)
		{
			for (int bit = 0; bit < 8; bit++)
			{
				body.insert(body.end(), rowBytes, (values[channel] >> bit) & 1 ? 0xFF : 0x00);
			}
		}
	}

	return body;
}

static bool AllPixelsAre(std::vector<uint8_t> const &pixels, uint8_t x, uint8_t y, uint8_t z)
{
	for (size_t i = 0; i + 3 < pixels.size(); i += 4)
	{
		if (pixels[i] != z || pixels[i + 1] != y || pixels[i + 2] != x || pixels[i + 3] != x)
		{
			return false;
		}
	}

	return !pixels.empty();
}

static void CheckRimFiles()
{
	printf("name_Normal.rim:\n");

	{
		Rim deep(8, 24, 0);
		deep.body = DeepBody(200, 90, 255, 8);
		std::vector<uint8_t> pixels;
		Check(RimProblem(deep.File(), &pixels) == kMapFine, "24 planes, plain, is taken");
		Check(AllPixelsAre(pixels, 200, 90, 255), "and x goes in alpha and red, y in green, and z in blue");

		deep.stockOrder = true;
		Check(RimProblem(deep.File(), &pixels) == kMapFine && AllPixelsAre(pixels, 200, 90, 255), "the same with the parts the game's own pictures start with");
	}

	{
		// a table, with every pixel colour 1
		Rim paletted(8, 2, 0);
		uint8_t const colours[6] = { 1, 2, 3, 40, 50, 60 };
		paletted.colours.assign(colours, colours + 6);

		for (int row = 0; row < 8; row++)
		{
			paletted.body.push_back(0xFF);    // first plane
			paletted.body.push_back(0x00);    // second plane
		}

		std::vector<uint8_t> pixels;
		Check(RimProblem(paletted.File(), &pixels) == kMapFine && AllPixelsAre(pixels, 40, 50, 60), "planes and a colour table are taken too");
	}

	{
		// packed, 8 wide, 24 planes: a run and a plain byte, and a no-op that does nothing
		Rim packed(8, 24, 1);

		for (int row = 0; row < 8; row++)
		{
			for (int plane = 0; plane < 24; plane++)
			{
				packed.body.push_back((uint8_t)-128);
				packed.body.push_back(0);
				packed.body.push_back((plane == 0) ? 0xFF : 0x00);
			}
		}

		std::vector<uint8_t> pixels;
		Check(RimProblem(packed.File(), &pixels) == kMapFine && AllPixelsAre(pixels, 1, 0, 0), "packed rows are read, and a no-op changes nothing");
	}

	{
		// A repeated run makes 8 bytes, the whole row of a 64-wide picture with one plane, from 2 bytes
		Rim run(64, 1, 1);
		uint8_t const colours[6] = { 1, 2, 3, 40, 50, 60 };
		run.colours.assign(colours, colours + 6);

		for (int row = 0; row < 64; row++)
		{
			run.body.push_back((uint8_t)(int8_t)-7);
			run.body.push_back(0xFF);
		}

		std::vector<uint8_t> pixels;
		Check(RimProblem(run.File(), &pixels) == kMapFine && AllPixelsAre(pixels, 40, 50, 60), "a repeated run fills its row");
	}

	// Bad ones. Each starts from a good file and changes one thing.
	Rim const goodDeep = [] { Rim r(8, 24, 0); r.body = DeepBody(128, 128, 255, 8); return r; }();

	CheckProblem(RimProblem(Bytes()), kMapNoHeading, "an empty file");

	// These two are refused before any memory is wanted for the picture, so they're asked directly, with no decoding after
	{
		ReliefMapRim rim;
		Rim shortPlain = goodDeep;
		shortPlain.body.pop_back();
		Bytes const plainFile = shortPlain.File();
		CheckProblem(ReliefMap_FindRimParts(&plainFile[0], plainFile.size(), &rim), kMapCutShort, "too little body for a plain picture, before any memory is wanted");

		Rim shortPacked(2048, 24, 1);
		shortPacked.body.assign(100, 0);
		Bytes const packedFile = shortPacked.File();
		CheckProblem(ReliefMap_FindRimParts(&packedFile[0], packedFile.size(), &rim), kMapCutShort, "too little for even the best packing, before any memory is wanted");
	}

	// One row, on its own
	{
		uint8_t const bytes[2] = { 1, 2 };
		uint8_t row[4];
		uint8_t const *body = bytes;
		uint64_t left = 2;
		CheckProblem(ReliefMap_ReadRow(&body, &left, 0, row, 3), kMapCutShort, "a plain row with too little left");
		Check(left == 2, "and it uses up nothing");

		left = 2;
		Check(ReliefMap_ReadRow(&body, &left, 0, row, 2) == kMapFine && left == 0 && row[1] == 2 && body == bytes + 2, "a plain row with exactly enough left uses it all");
	}

	// Which pixel a bit belongs to: the first pixel is the top bit of its byte, and the first row of the first plane is what's lowest in the colour
	{
		Rim oneBit(8, 24, 0);
		oneBit.body.assign(8 * 24, 0);
		oneBit.body[0] = 0x80;
		std::vector<uint8_t> pixels;
		Check(RimProblem(oneBit.File(), &pixels) == kMapFine && pixels[2] == 1 && pixels[6] == 0 && pixels[(8 * 7) * 4 + 2] == 0, "a bit lands on the pixel it should");
	}

	{
		Bytes junk;
		AddText(junk, "JUNKJUNK");
		CheckProblem(RimProblem(junk), kMapPartsWrong, "a part it doesn't know");

		Bytes form;
		AddText(form, "FORM");
		AddBig32(form, 0);
		AddText(form, "ILBMJUNK");
		CheckProblem(RimProblem(form), kMapPartsWrong, "an inner part it doesn't know");

		Bytes noHeading;
		AddText(noHeading, "BODY");
		AddBig32(noHeading, 0);
		CheckProblem(RimProblem(noHeading), kMapNoHeading, "a picture ahead of its heading");

		Bytes tail = goodDeep.File();
		size_t const bodyAt = std::string((char const *)&tail[0], tail.size()).find("BODY");
		tail.resize(bodyAt);
		CheckProblem(RimProblem(tail), kMapNoPicture, "a heading with no picture");

		Bytes shortHeading = goodDeep.File();
		size_t const headingAt = std::string((char const *)&shortHeading[0], shortHeading.size()).find("BMHD");
		shortHeading[headingAt + 7] = 19;
		CheckProblem(RimProblem(shortHeading), kMapNoHeading, "a heading of the wrong size");

		shortHeading = goodDeep.File();
		shortHeading.resize(headingAt + 20);
		CheckProblem(RimProblem(shortHeading), kMapNoHeading, "a file that ends inside its heading");

		CheckProblem(ReliefMapProblem(RimProblem(Bytes(7, 'F'))), kMapNoHeading, "a file too short to hold a part");
	}

	{
		ReliefMapRim rim;
		Bytes const file = goodDeep.File();
		CheckProblem(ReliefMap_FindRimParts(&file[0], kReliefMapMostFileBytes + 1, &rim), kMapFileTooLong, "a file past the limit");
	}

	struct SizeRow { uint32_t width, height; ReliefMapProblem problem; };

	SizeRow const sizeRows[] =
	{
		{ 8, 16, kMapNotSquare }, { 16, 8, kMapNotSquare }, { 0, 0, kMapSideTooSmall }, { 2, 2, kMapSideTooSmall }, { 3, 3, kMapSideTooSmall },
		{ 7, 7, kMapSideNotAPowerOfTwo }, { 2047, 2047, kMapSideNotAPowerOfTwo }, { 4096, 4096, kMapSideTooLarge }, { 65535, 65535, kMapSideTooLarge },
	};

	for (size_t i = 0; i < sizeof(sizeRows) / sizeof(sizeRows[0]); i++)
	{
		Rim r = goodDeep;
		r.width = sizeRows[i].width;
		r.height = sizeRows[i].height;
		CheckProblem(RimProblem(r.File()), sizeRows[i].problem, std::to_string(r.width) + " x " + std::to_string(r.height));
	}

	uint32_t const badPlanes[5] = { 0, 17, 23, 25, 32 };

	for (int i = 0; i < 5; i++)
	{
		Rim r = goodDeep;
		r.planes = badPlanes[i];
		CheckProblem(RimProblem(r.File()), kMapPlanesWrong, std::to_string(badPlanes[i]) + " planes");
	}

	uint32_t const badPacking[3] = { 2, 3, 255 };

	for (int i = 0; i < 3; i++)
	{
		Rim r = goodDeep;
		r.packing = badPacking[i];
		CheckProblem(RimProblem(r.File()), kMapPackingWrong, "packing method " + std::to_string(badPacking[i]));
	}

	{
		Rim r = goodDeep;
		r.colours.assign(3, 0);
		CheckProblem(RimProblem(r.File()), kMapColourTableWrong, "24 planes with a table it has no use for");

		Rim noTable(8, 8, 0);
		noTable.body.assign(64, 0);
		CheckProblem(RimProblem(noTable.File()), kMapColourTableWrong, "8 planes with no table");

		Rim badTable(8, 8, 0);
		badTable.colours.assign(4, 0);
		badTable.body.assign(64, 0);
		CheckProblem(RimProblem(badTable.File()), kMapColourTableWrong, "a table that isn't whole colours");

		Bytes longTable;
		AddText(longTable, "CMAP");
		AddBig32(longTable, 300);
		CheckProblem(RimProblem(longTable), kMapColourTableWrong, "a table longer than the file");

		Rim tooBig(8, 8, 0);
		tooBig.colours.assign(3 * 65537, 0);
		tooBig.body.assign(64, 0);
		CheckProblem(RimProblem(tooBig.File()), kMapColourTableWrong, "a table with more colours than a number can name");

		Rim past(8, 1, 0);
		past.colours.assign(3, 0);    // one colour, and every pixel uses number 1
		past.body.assign(8, 0xFF);
		CheckProblem(RimProblem(past.File()), kMapColourPastTable, "a pixel past the end of its table");
	}

	{
		// The body says it's longer than what's left of the file
		Rim r = goodDeep;
		Bytes file = r.File();
		file.pop_back();
		CheckProblem(RimProblem(file), kMapCutShort, "a body longer than the file");

		// The body says how long it is, and that isn't enough, plain
		Rim shortBody = goodDeep;
		shortBody.body.pop_back();
		CheckProblem(RimProblem(shortBody.File()), kMapCutShort, "too little body for a plain picture");

		// packed, with not even a 64th of the picture
		Rim hopeless(2048, 24, 1);
		hopeless.body.assign(100, 0);
		CheckProblem(RimProblem(hopeless.File()), kMapCutShort, "far too little body for a packed picture, before any memory is wanted");

		// Enough to pass that sum, then it stops. Each row of each plane makes 1 byte from 2.
		Rim stops(8, 24, 1);
		stops.body.assign(10, 0);
		CheckProblem(RimProblem(stops.File()), kMapCutShort, "a packed picture that stops part way");

		Rim noLiteral(8, 1, 1);
		noLiteral.colours.assign(3, 0);
		noLiteral.body.assign(15, 0);    // seven whole rows, each a count and a byte, and then a count with no byte after it
		CheckProblem(RimProblem(noLiteral.File()), kMapCutShort, "a plain run with nothing after it");

		Rim noRepeat(16, 1, 1);
		noRepeat.colours.assign(3, 0);

		for (int row = 0; row < 15; row++)    // fifteen whole rows, each a run of two bytes, and then a run with no byte to repeat
		{
			noRepeat.body.push_back((uint8_t)(int8_t)-1);
			noRepeat.body.push_back(0);
		}

		noRepeat.body.push_back((uint8_t)(int8_t)-1);
		CheckProblem(RimProblem(noRepeat.File()), kMapCutShort, "a repeated run with nothing after it");

		Rim onlyNoOps(8, 1, 1);
		onlyNoOps.colours.assign(3, 0);
		onlyNoOps.body.assign(5000, 0x80);
		CheckProblem(RimProblem(onlyNoOps.File()), kMapCutShort, "nothing but no-ops comes to an end");

		// a row is one byte here, so both of these are too long for it
		Rim plainRun(8, 1, 1);
		plainRun.colours.assign(3, 0);
		plainRun.body.assign(20, 0);
		plainRun.body[0] = 1;
		CheckProblem(RimProblem(plainRun.File()), kMapRunPastRow, "a plain run past the end of its row");

		Rim repeatRun(8, 1, 1);
		repeatRun.colours.assign(3, 0);
		repeatRun.body.assign(20, 0);
		repeatRun.body[0] = (uint8_t)(int8_t)-1;
		CheckProblem(RimProblem(repeatRun.File()), kMapRunPastRow, "a repeated run past the end of its row");
	}

	// The largest there can be, which is what the room for a row is for. Memory is watched on both sides below, in the damaged copies.
	{
		Rim largest(2048, 24, 0);
		largest.body.assign((size_t)256 * 24 * 2048, 0);
		std::vector<uint8_t> pixels;
		Check(RimProblem(largest.File(), &pixels) == kMapFine && pixels.size() == (size_t)2048 * 2048 * 4, "the largest there can be is taken");
	}

	for (int i = 0; i < kMapProblemCount; i++)
	{
		Check(strcmp(ReliefMap_ProblemWords((ReliefMapProblem)i), "of a problem with no name") != 0, "reason " + std::to_string(i) + " has words of its own");
	}
}

// The numbers repeat exactly from run to run, so a failure can be looked at again
static uint32_t randomState = 12345;

static uint32_t RandomBits()
{
	randomState ^= randomState << 13;
	randomState ^= randomState >> 17;
	randomState ^= randomState << 5;
	return randomState;
}

static uint8_t const kPattern = 0xA5;

static void CheckDamagedFiles()
{
	printf("Damaged files:\n");

	Rim packed(16, 24, 1);
	packed.stockOrder = true;

	for (int i = 0; i < 2 * 24 * 16; i++)    // each row of each plane, two bytes to a row, as a count and a byte
	{
		packed.body.push_back(0);
		packed.body.push_back((uint8_t)RandomBits());
	}

	Rim plain(16, 8, 0);
	plain.colours.assign(3 * 256, 7);
	plain.body.assign(2 * 8 * 16, 0x11);

	Bytes const goods[3] = { packed.File(), plain.File(), GoodDds(16) };
	int taken = 0, refused = 0, wrote = 0;
	bool everyReasonNamed = true;

	for (int round = 0; round < 60000; round++)
	{
		Bytes damaged = goods[round % 3];
		int const changes = 1 + (int)(RandomBits() % 6);

		for (int c = 0; c < changes; c++)
		{
			damaged[RandomBits() % damaged.size()] = (uint8_t)RandomBits();
		}

		if (round % 5 == 0)
		{
			damaged.resize(RandomBits() % damaged.size());
		}

		ReliefMapProblem problem;

		if (round % 3 == 2)
		{
			uint32_t side = 0;
			problem = ReliefMap_CheckDds(damaged.empty() ? (uint8_t const *)"" : &damaged[0], damaged.size(), &side);
		}
		else
		{
			ReliefMapRim rim;
			problem = ReliefMap_FindRimParts(damaged.empty() ? (uint8_t const *)"" : &damaged[0], damaged.size(), &rim);

			if (problem == kMapFine)
			{
				size_t const size = (size_t)rim.side * rim.side * 4;
				std::vector<uint8_t> memory(size + 2048, kPattern);
				problem = ReliefMap_DecodeRim(rim, &memory[1024]);

				for (size_t i = 0; i < 1024; i++)
				{
					wrote += (memory[i] != kPattern || memory[1024 + size + i] != kPattern) ? 1 : 0;
				}
			}
		}

		taken += (problem == kMapFine) ? 1 : 0;
		refused += (problem == kMapFine) ? 0 : 1;
		everyReasonNamed = everyReasonNamed && problem >= kMapFine && problem < kMapProblemCount;
	}

	printf("  60000 damaged copies: %d taken, %d refused\n", taken, refused);
	Check(taken > 1000 && refused > 1000, "the damage reaches both ways out");
	Check(wrote == 0, "none of them writes outside its picture, and all of them end");
	Check(everyReasonNamed, "and each gives a reason there are words for");
}

static void CheckSmallerCopies()
{
	printf("The smaller copies of a .rim map:\n");

	// flat stays flat
	{
		uint8_t from[4 * 4 * 4], to[2 * 2 * 4];

		for (int i = 0; i < 16; i++)
		{
			ReliefMap_PutPixel(from + i * 4, 128, 128, 255);
		}

		ReliefMap_MakeSmallerCopy(from, 4, to);
		Check(to[0] == 255 && to[1] == 128 && to[2] == 128 && to[3] == 128, "flat stays exactly flat");
	}

	// Slopes that cancel out, and nothing facing out, come to nothing, which is made flat
	{
		uint8_t from[2 * 2 * 4], to[4];
		ReliefMap_PutPixel(from, 255, 128, 128);
		ReliefMap_PutPixel(from + 4, 1, 128, 128);
		ReliefMap_PutPixel(from + 8, 128, 255, 128);
		ReliefMap_PutPixel(from + 12, 128, 1, 128);
		ReliefMap_MakeSmallerCopy(from, 2, to);
		Check(to[0] == 255 && to[1] == 128 && to[2] == 128 && to[3] == 128, "arrows that cancel out come to flat, not to an arrow of no length");
	}

	// All leaning the same way stays leaning that way, at a length of 1
	{
		uint8_t from[2 * 2 * 4], to[4];

		for (int i = 0; i < 4; i++)
		{
			ReliefMap_PutPixel(from + i * 4, 128 + 90, 128, 128 + 90);
		}

		ReliefMap_MakeSmallerCopy(from, 2, to);
		Check(to[2] == to[3] && to[2] >= 217 && to[2] <= 219 && to[1] == 128 && to[0] >= 217 && to[0] <= 219, "a slope keeps its angle and its length");
	}

	// Different arrows average to something shorter than 1, which has to be put back
	{
		uint8_t from[2 * 2 * 4], to[4];
		ReliefMap_PutPixel(from, 128 + 90, 128, 128 + 90);
		ReliefMap_PutPixel(from + 4, 128 - 90, 128, 128 + 90);
		ReliefMap_PutPixel(from + 8, 128 + 90, 128, 128 + 90);
		ReliefMap_PutPixel(from + 12, 128 - 90, 128, 128 + 90);
		ReliefMap_MakeSmallerCopy(from, 2, to);
		Check(to[2] == 128 && to[1] == 128 && to[0] == 255, "arrows that lean apart average to one that's a full length again");
	}

	// each pixel really comes from its own four
	{
		uint8_t from[4 * 4 * 4], to[2 * 2 * 4];

		for (int y = 0; y < 4; y++)
		{
			for (int x = 0; x < 4; x++)
			{
				bool const right = x >= 2;
				bool const bottom = y >= 2;
				ReliefMap_PutPixel(from + (y * 4 + x) * 4, right ? 228 : 128, bottom ? 228 : 128, 200);
			}
		}

		ReliefMap_MakeSmallerCopy(from, 4, to);
		uint8_t same[4 * 4 * 4], right[2 * 2 * 4];

		for (int i = 0; i < 16; i++)
		{
			ReliefMap_PutPixel(same + i * 4, 228, 128, 200);
		}

		ReliefMap_MakeSmallerCopy(same, 4, right);
		Check(to[4 + 3] == right[3] && to[4] == right[0], "the top right comes only from the four on the top right");
		Check(to[3] == 128 && to[1] == 128, "the top left comes from the top left");
		Check(to[4 + 3] > 128 && to[4 + 1] == 128, "the top right from the top right");
		Check(to[8 + 3] == 128 && to[8 + 1] > 128, "the bottom left from the bottom left");
		Check(to[12 + 3] > 128 && to[12 + 1] > 128, "and the bottom right from the bottom right");
	}

	Check(ReliefMap_Number(1.0f) == 255 && ReliefMap_Number(0.0f) == 128 && ReliefMap_Number(-1.0f) == 1, "the numbers are what the shader takes them to be");
	Check(ReliefMap_Number(0.5f) == 192, "and they're rounded to the nearest");
	Check(ReliefMap_Number(5.0f) == 255 && ReliefMap_Number(-5.0f) == 0, "and they stay in a byte");
}

static void CheckFileNames()
{
	printf("Where a picture's map is:\n");

	std::string name;
	Check(ReliefMap_FileName("graphics/Common/burn.rim", "_Normal.dds", &name) && name == "graphics/Common/burn_Normal.dds", "the same folder and name, with its ending");
	Check(ReliefMap_FileName("graphics/Common/BURN.RIM", "_Normal.rim", &name) && name == "graphics/Common/BURN_Normal.rim", "in capitals as well");
	Check(ReliefMap_FileName("graphics/Common Objects/a b.Rim", "_Depth.dds", &name) && name == "graphics/Common Objects/a b_Depth.dds", "with spaces");
	Check(!ReliefMap_FileName("", "_Normal.dds", &name), "no name has no map");
	Check(!ReliefMap_FileName(".rim", "_Normal.dds", &name), "nor a name that's only the ending");
	Check(!ReliefMap_FileName("graphics/burn", "_Normal.dds", &name), "nor one with no ending");
	Check(!ReliefMap_FileName("graphics/burn.tga", "_Normal.dds", &name), "nor another kind of picture");
	Check(!ReliefMap_FileName("graphics/burn.rimx", "_Normal.dds", &name), "nor a longer ending");
	Check(!ReliefMap_FileName("graphics/burnxrim", "_Normal.dds", &name), "nor a name that only ends in those letters");
}

int main()
{
	CheckTheSums();
	CheckDdsFiles();
	CheckRimFiles();
	CheckDamagedFiles();
	CheckSmallerCopies();
	CheckFileNames();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
