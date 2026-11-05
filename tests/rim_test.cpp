// The test of reading a .RIM picture. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Every picture in the game is a .RIM file, and anyone can make one, so nothing in it can be trusted. This builds pictures byte by byte, good and bad, writes them out as files, and reads them
// with the game's own RimLoader and FileStream. The picture is written into memory with a known pattern after it, so anything written past the end shows up. It also checks the sum that
// decides whether a font picture is big enough for the fixed layout its letters are read from.
//
// The game's own 1,028 pictures aren't here, because they aren't in the repository. A separate program, kept out of it because it reads them, compared them all through the old loader and the new one.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own. The program gives back a non-zero result if anything failed.

#include <fstream>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "RimLoader.h"
#include "FastFile.h"
#include "utilities.h"
#include "font_checks.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- What FileStream asks of the rest of the game. These pictures are ordinary files, never in a fastfile.

// One file can be made to look as if it's inside a fastfile, to show it stops at its own end.
static std::string memberName, containerName;
static FastFileHandle memberHandle;

FastFileHandle *FF_Find(const std::string &name)
{
	return (!memberName.empty() && name == memberName) ? &memberHandle : 0;
}

const std::string &FF_GetFastFileName(int32_t)
{
	return containerName;
}

namespace Util
{
	void FtoBslash(std::string &text)
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			text[i] = (text[i] == '/') ? '\\' : text[i];
		}
	}
}

// ---- Building a picture

typedef std::vector<uint8_t> Bytes;

static void Add(Bytes &to, char const *text)
{
	to.insert(to.end(), text, text + strlen(text));
}

static void Add32(Bytes &to, uint32_t value)
{
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		to.push_back((uint8_t)(value >> shift));
	}
}

static void Add16(Bytes &to, uint32_t value)
{
	to.push_back((uint8_t)(value >> 8));
	to.push_back((uint8_t)value);
}

struct Picture
{
	uint32_t width, height, planes, masking, compression, transparent;
	Bytes colours;
	Bytes body;
	bool stockOrder;

	Picture(uint32_t w, uint32_t h, uint32_t p, uint32_t c) : width(w), height(h), planes(p), masking(0), compression(c), transparent(0), stockOrder(false) {}

	Bytes File() const
	{
		Bytes file;

		if (stockOrder)
		{
			// the order all 1,028 of the game's own pictures have
			Add(file, "LIST");
			Add32(file, 0);
			Add(file, "ILBM");
			Add(file, "PROP");
			Add32(file, 0);
			Add(file, "ILBM");
			Add(file, "TRAN");
			Add32(file, 4);
			Add32(file, 0);
			Add32(file, 0);
		}

		Add(file, "FORM");
		Add32(file, 0);
		Add(file, "ILBM");
		Add(file, "BMHD");
		Add32(file, 20);
		Add16(file, width);
		Add16(file, height);
		Add16(file, 0);
		Add16(file, 0);
		file.push_back((uint8_t)planes);
		file.push_back((uint8_t)masking);
		file.push_back((uint8_t)compression);
		file.push_back(0);
		Add16(file, transparent);
		file.push_back(1);
		file.push_back(1);
		Add16(file, width);
		Add16(file, height);

		if (!colours.empty())
		{
			Add(file, "CMAP");
			Add32(file, (uint32_t)colours.size());
			file.insert(file.end(), colours.begin(), colours.end());

			if (colours.size() % 2)
			{
				file.push_back(0);
			}
		}

		Add(file, "BODY");
		Add32(file, (uint32_t)body.size());
		file.insert(file.end(), body.begin(), body.end());
		return file;
	}
};

// Red, green and blue of colour number n are n, n + 1 and n + 2, so a pixel shows which colour it got
static Bytes CountingColours(int howMany)
{
	Bytes colours;

	for (int n = 0; n < howMany; n++)
	{
		colours.push_back((uint8_t)n);
		colours.push_back((uint8_t)(n + 1));
		colours.push_back((uint8_t)(n + 2));
	}

	return colours;
}

static std::string TestFilePath()
{
	static std::string path;

	if (path.empty())
	{
		char *name = _tempnam(NULL, "rimtest");
		path = name ? name : "rimtest.tmp";
		free(name);
	}

	return path;
}

static void WriteFile(Bytes const &bytes)
{
	FILE *file = fopen(TestFilePath().c_str(), "wb");

	if (file)
	{
		if (!bytes.empty())
		{
			fwrite(&bytes[0], 1, bytes.size(), file);
		}

		fclose(file);
	}
}

// ---- Reading it back

static uint8_t const kPattern = 0xA5;
static size_t const kWatched = 4096;

struct Result
{
	bool opened;
	bool finished;      // whether it read all its picture
	RimProblem problem; // why it didn't, or why it wouldn't open
	uint32_t width, height;
	std::vector<uint8_t> pixels;
	bool nothingPastTheEnd;

	Result() : opened(false), finished(false), problem(kRimFine), width(0), height(0), nothingPastTheEnd(true) {}

	// blue, green, red and alpha of a pixel, in the order they're written
	bool PixelIs(uint32_t x, uint32_t y, int b, int g, int r, int a) const
	{
		size_t const at = ((size_t)y * width + x) * 4;
		return at + 3 < pixels.size() && pixels[at] == b && pixels[at + 1] == g && pixels[at + 2] == r && pixels[at + 3] == a;
	}

	bool ShowsColour(uint32_t x, uint32_t y, int n) const
	{
		return PixelIs(x, y, (n + 2) & 255, (n + 1) & 255, n & 255, 255);
	}
};

// Decode writes where it's told, trusting that there's room for the size it gives. This gives it exactly that, with a pattern after it, as the game does: the game's check that the card
// made what was asked for is in R_CreateTexture. Any picture that opens is small enough to read in full, because of the loader's own limit.
static Result ReadFrom(std::string const &path)
{
	Result result;
	RimLoader picture;
	result.opened = picture.Open(path);
	result.problem = picture.Problem();

	if (result.opened)
	{
		picture.GetDimensions(result.width, result.height);
		size_t const size = (size_t)result.width * result.height * 4;

		std::vector<uint8_t> memory(size + kWatched, kPattern);
		result.finished = picture.Decode(&memory[0], result.width * 4);
		result.problem = picture.Problem();

		for (size_t i = size; i < memory.size(); i++)
		{
			result.nothingPastTheEnd = result.nothingPastTheEnd && (memory[i] == kPattern);
		}

		memory.resize(size);
		result.pixels.swap(memory);
	}

	return result;
}

static Result Read(Bytes const &bytes)
{
	WriteFile(bytes);
	return ReadFrom(TestFilePath());
}

static int CountOf(std::string const &text, std::string const &words)
{
	int count = 0;
	std::string::size_type place = 0;

	while ((place = text.find(words, place)) != std::string::npos)
	{
		count++;
		place += words.size();
	}

	return count;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

// ---- The checks

static void CheckGoodPictures()
{
	printf("Pictures like the game's own:\n");

	{
		// eight planes, plain. Two pixels: colour 1 and colour 2.
		Picture p(2, 1, 8, 0);
		p.colours = CountingColours(4);
		uint8_t const body[8] = { 0x80, 0x40, 0, 0, 0, 0, 0, 0 };
		p.body.assign(body, body + 8);
		Result const r = Read(p.File());
		Check(r.opened && r.width == 2 && r.height == 1, "a plain picture opens, at its own size");
		Check(r.ShowsColour(0, 0, 1) && r.ShowsColour(1, 0, 2), "and each pixel gets the colour its planes spell out");
		Check(r.nothingPastTheEnd, "and nothing is written past the end");
	}

	{
		Picture p(2, 1, 8, 0);
		p.stockOrder = true;
		p.colours = CountingColours(4);
		uint8_t const body[8] = { 0x80, 0x40, 0, 0, 0, 0, 0, 0 };
		p.body.assign(body, body + 8);
		Result const r = Read(p.File());
		Check(r.opened && r.ShowsColour(0, 0, 1) && r.ShowsColour(1, 0, 2), "the same in the order of chunks the game's own pictures have");
	}

	{
		// the most planes any stock picture has: colour number 1 + 1024, from the first plane and the last
		Picture p(1, 1, 11, 0);
		p.colours = CountingColours(1100);
		uint8_t const body[11] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80 };
		p.body.assign(body, body + 11);
		Result const r = Read(p.File());
		Check(r.opened && r.ShowsColour(0, 0, 1025), "11 planes, the most the game's own pictures have, spells out the right colour");
	}

	{
		// packed: a run of two bytes the same, then a plain byte, in a row of three bytes (24 pixels wide), one plane
		Picture p(24, 1, 1, 1);
		p.colours = CountingColours(2);
		int8_t const body[4] = { -1, (int8_t)0xFF, 0, (int8_t)0xF0 };
		p.body.assign((uint8_t const *)body, (uint8_t const *)body + 4);
		Result const r = Read(p.File());
		bool right = r.opened;

		for (uint32_t x = 0; right && x < 24; x++)
		{
			right = r.ShowsColour(x, 0, (x < 20) ? 1 : 0);
		}

		Check(right, "packed runs and plain bytes come out as they should");
		Check(r.nothingPastTheEnd, "and nothing is written past the end");
	}

	{
		Picture p(2, 1, 8, 0);
		p.masking = 2;
		p.transparent = 1;
		p.colours = CountingColours(4);
		uint8_t const body[8] = { 0x80, 0x40, 0, 0, 0, 0, 0, 0 };
		p.body.assign(body, body + 8);
		Result const r = Read(p.File());
		Check(r.PixelIs(0, 0, 3, 2, 1, 0), "the see-through colour comes out see-through");
		Check(r.ShowsColour(1, 0, 2), "and the others don't");
	}
}

static void CheckRefusals()
{
	printf("Pictures that are refused when they're opened:\n");

	{
		Picture p(1, 1, 16, 0);
		p.colours = CountingColours(1);
		p.body.assign(16, 0);
		Check(Read(p.File()).opened, "16 planes opens, because a colour number has 16 bits");
	}

	// 24 is what a hand-made map can have, which only its own reader takes, in relief_map_file.h. A picture of the game's own can never get there through this.
	uint32_t const tooMany[4] = { 17, 24, 33, 255 };

	for (int i = 0; i < 4; i++)
	{
		Picture p(1, 1, tooMany[i], 0);
		p.colours = CountingColours(1);
		p.body.assign(tooMany[i], 0);
		Check(Read(p.File()).problem == kRimPlanesWrong, std::to_string(tooMany[i]) + " planes is refused, because the sum that works out a colour number can't hold them");
	}

	uint32_t const compressions[3] = { 2, 3, 255 };

	for (int i = 0; i < 3; i++)
	{
		Picture p(1, 1, 8, compressions[i]);
		p.colours = CountingColours(1);
		p.body.assign(8, 0);
		Check(Read(p.File()).problem == kRimPackingWrong, "packing method " + std::to_string(compressions[i]) + " is refused, because it can't be read");
	}

	Check(Read(Bytes()).problem == kRimPartsWrong, "an empty file is refused");

	// A few bytes asking for a huge picture: this used to take 256 MB, and minutes of reading from nothing
	{
		Picture p(8192, 8192, 16, 1);
		p.colours = CountingColours(1);
		Check(p.File().size() < 100 && Read(p.File()).problem == kRimTooLarge, "a tiny file saying it's 8192 x 8192 is refused");

		Picture tooWide(8193, 1, 1, 0);
		tooWide.colours = CountingColours(1);
		tooWide.body.assign(1025, 0);
		Check(Read(tooWide.File()).problem == kRimTooLarge, "a picture wider than the limit is refused, with all its data there");

		Picture tooTall(1, 8193, 1, 0);
		tooTall.colours = CountingColours(1);
		tooTall.body.assign(8193, 0);
		Check(Read(tooTall.File()).problem == kRimTooLarge, "and one taller");

		Picture tooMany(2049, 2048, 1, 0);
		tooMany.colours = CountingColours(1);
		tooMany.body.assign((size_t)257 * 2048, 0);
		Check(Read(tooMany.File()).problem == kRimTooLarge, "and one with too many pixels in all");

		Picture most(2048, 2048, 1, 0);
		most.colours = CountingColours(2);
		most.body.assign((size_t)256 * 2048, 0x80);
		Result const r = Read(most.File());
		Check(r.opened && r.finished && r.ShowsColour(0, 2047, 1) && r.ShowsColour(1, 2047, 0) && r.nothingPastTheEnd, "the most pixels there can be is read to the end");

		Picture tall(1, 7392, 1, 0);
		tall.colours = CountingColours(2);
		tall.body.assign(7392, 0x80);
		Check(Read(tall.File()).opened, "as tall as the game's own font opens");
	}

	// The picture's data has to be able to hold what its size says
	{
		Picture plain(8, 8, 8, 0);
		plain.colours = CountingColours(1);
		plain.body.assign(63, 0);
		Check(Read(plain.File()).problem == kRimBodyTooShort, "a plain picture one byte short is refused");
		plain.body.assign(64, 0);
		Check(Read(plain.File()).opened, "and with exactly enough it opens");

		Picture packed(8, 8, 8, 1);
		packed.colours = CountingColours(1);
		Check(Read(packed.File()).problem == kRimBodyTooShort, "a packed picture with nothing in it is refused");
		packed.body.assign(1, 0);
		Check(Read(packed.File()).opened, "a 64th of its size can't be ruled out until it's unpacked");

		Picture big(64, 64, 8, 1);
		big.colours = CountingColours(1);
		big.body.assign(63, 0);
		Check(Read(big.File()).problem == kRimBodyTooShort, "less than the best packing could make it from is refused");
		big.body.assign(64, 0);
		Check(Read(big.File()).opened, "and exactly that opens");

		Bytes file = plain.File();
		file.pop_back();
		Check(Read(file).problem == kRimBodyPastEnd, "a body that says it's longer than the file is refused");

		Bytes tableFile;
		Add(tableFile, "CMAP");
		Add32(tableFile, 300);
		Check(Read(tableFile).problem == kRimColourTablePastEnd, "and so is a colour table longer than the file");

		// really there, all of it, with more colours than a number can name
		Picture manyColours(1, 1, 16, 0);
		manyColours.colours.assign(3 * 65537, 0);
		manyColours.body.assign(16, 0);
		Check(Read(manyColours.File()).problem == kRimColourTableTooBig, "a colour table with more colours than can be used is refused");
		manyColours.colours.assign(3 * 65536, 0);
		Check(Read(manyColours.File()).opened, "and with exactly as many as can be used it opens");
	}

	{
		Bytes junk;
		Add(junk, "JUNK");
		Add32(junk, 0);
		Check(Read(junk).problem == kRimPartsWrong, "a file that starts with something else is refused");
	}

	{
		Bytes file;
		Add(file, "FORM");
		Add32(file, 0);
		Add(file, "ILBM");
		Check(Read(file).problem == kRimPartsWrong, "a file that ends before its picture is refused, instead of being searched forever");
	}

	{
		Bytes file;
		Add(file, "ILBM");
		Add(file, "JUNK");
		Check(Read(file).problem == kRimPartsWrong, "a part it doesn't know is refused");
	}

	{
		Bytes file;
		Add(file, "CMAP");
		Add32(file, 3u * 65537u);
		Check(Read(file).problem == kRimColourTableTooBig, "a colour table bigger than it can be is refused");
	}
}

static void CheckBadPixels()
{
	printf("Pictures with odd pixels:\n");

	{
		// A no-op among packed runs should change nothing. It used to count as the bytes of the run before it, so the row stopped short.
		Picture withNoOp(24, 1, 1, 1);
		withNoOp.colours = CountingColours(2);
		int8_t const body[5] = { -1, (int8_t)0xFF, -128, 0, (int8_t)0xF0 };
		withNoOp.body.assign((uint8_t const *)body, (uint8_t const *)body + 5);
		Result const r = Read(withNoOp.File());
		bool right = r.opened;

		for (uint32_t x = 0; right && x < 24; x++)
		{
			right = r.ShowsColour(x, 0, (x < 20) ? 1 : 0);
		}

		Check(right, "a no-op among packed runs changes nothing");
	}

	{
		// Only no-ops, all the way to the end. They have to end, and the end of the picture's data does that.
		Picture p(8, 2, 1, 1);
		p.colours = CountingColours(2);
		p.body.assign(5000, 0x80);
		Result const r = Read(p.File());
		Check(r.opened && !r.finished && r.problem == kRimBodyTooLong && r.nothingPastTheEnd, "a picture of nothing but no-ops ends, says so, and writes nothing past the end");
	}

	{
		// Data follows the picture's own, so a loader reading past the end of its data would find something to read, and carry on
		Picture p(8, 2, 1, 1);
		p.colours = CountingColours(2);
		p.body.assign(1, 0);
		Bytes file = p.File();
		file.insert(file.end(), 100, 0xFF);
		Result const r = Read(file);
		Check(r.opened && !r.finished && r.problem == kRimRanOut, "what comes after a picture's data isn't read as part of it");
	}

	{
		// The last thing in the picture's data says "repeat the next byte", and there isn't a next byte, only what follows the data
		Picture p(8, 1, 1, 1);
		p.colours = CountingColours(2);
		p.body.assign(1, (uint8_t)(int8_t)-1);
		Bytes file = p.File();
		file.insert(file.end(), 10, 0xFF);
		Result const r = Read(file);
		Check(r.opened && !r.finished && r.problem == kRimRanOut, "a repeated run with nothing to repeat stops there");
	}

	{
		// A run longer than what's left of the row is cut at the end of the row, whichever kind it is. Two planes, each one byte: if the first ran on, the second would land in the wrong place.
		int8_t const runs[2][4] = { { -100, (int8_t)0xFF, 0, (int8_t)0x80 }, { 99, (int8_t)0xFF, 0, (int8_t)0x80 } };
		char const *const kinds[2] = { "a run of one byte repeated", "a run of plain bytes" };

		for (int kind = 0; kind < 2; kind++)
		{
			Picture p(8, 1, 2, 1);
			p.colours = CountingColours(4);
			p.body.assign((uint8_t const *)runs[kind], (uint8_t const *)runs[kind] + 4);
			Result const r = Read(p.File());
			Check(r.opened && r.ShowsColour(0, 0, 3) && r.ShowsColour(1, 0, 1), std::string(kinds[kind]) + " longer than its row is cut at the end of the row");
			Check(r.nothingPastTheEnd, "and nothing is written past the end");
		}
	}

	{
		// a colour number past the end of the table
		Picture p(1, 1, 8, 0);
		p.colours = CountingColours(1);
		uint8_t const body[8] = { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 };
		p.body.assign(body, body + 8);
		Result const r = Read(p.File());
		Check(r.opened && r.PixelIs(0, 0, 0, 0, 0, 255), "a colour past the end of the table comes out black");
	}

	{
		Picture p(1, 1, 8, 0);
		uint8_t const body[8] = { 0x80, 0, 0, 0, 0, 0, 0, 0 };
		p.body.assign(body, body + 8);
		Result const r = Read(p.File());
		Check(r.opened && r.PixelIs(0, 0, 0, 0, 0, 255), "a picture with no colour table comes out black");
	}

	{
		// no planes at all
		Picture p(2, 2, 0, 0);
		p.colours = CountingColours(1);
		Result const r = Read(p.File());
		Check(r.problem == kRimPlanesWrong, "a picture with no planes is refused, because a few bytes could ask for any size");
	}

	{
		// Packed, the file stops part way through the picture. Each row of each plane takes 2 bytes here, and there's only enough for the first row and part of the next.
		Picture p(32, 32, 8, 1);
		p.colours = CountingColours(4);

		for (int i = 0; i < 8 * 4 + 4; i++)
		{
			p.body.push_back(0);
			p.body.push_back(0xFF);
		}

		p.body.resize(32 * 8 * 4 / 64 > p.body.size() ? 32 * 8 * 4 / 64 : p.body.size());
		Result const r = Read(p.File());
		Check(r.opened && !r.finished && r.problem == kRimRanOut, "packed and cut short: it opens, and says it couldn't finish");
		Check(r.nothingPastTheEnd, "and writes nothing past the end");
		Check(r.pixels.size() > 32 * 4 && r.pixels[32 * 4] == kPattern, "and a row it couldn't finish isn't written at all");
	}

	{
		Picture p(0, 0, 8, 0);
		Result const r = Read(p.File());
		Check(r.opened && r.width == 0 && r.height == 0 && r.nothingPastTheEnd, "a picture with no size has nothing to write, and writes nothing");
	}

	{
		// as wide as it can be
		Picture p(8192, 1, 1, 0);
		p.colours = CountingColours(2);
		p.body.assign(1024, 0);
		Result const r = Read(p.File());
		Check(r.opened && r.finished && r.width == 8192 && r.nothingPastTheEnd, "8192 wide: nothing is written past the end of its row");
	}

	{
		// a second colour table replaces the first
		Picture p(1, 1, 1, 0);
		p.colours = CountingColours(2);
		p.body.assign(1, 0x80);
		Bytes file = p.File();
		Bytes extra;
		Add(extra, "CMAP");
		Add32(extra, 6);
		uint8_t const second[6] = { 9, 9, 9, 50, 60, 70 };
		extra.insert(extra.end(), second, second + 6);
		size_t const body = std::string((char const *)&file[0], file.size()).find("BODY");
		file.insert(file.begin() + body, extra.begin(), extra.end());
		Result const r = Read(file);
		Check(r.opened && r.PixelIs(0, 0, 70, 60, 50, 255), "with two colour tables, the last one counts");
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

// A file can be any length, and every byte read is a call to Windows. So no file should be able to keep this reading just by going on.
static void CheckLongFiles()
{
	printf("Long files:\n");

	{
		// The longest honest way to pack a row: every byte in a run of its own, which takes 2
		Picture p(8, 1, 1, 1);
		p.colours = CountingColours(2);
		uint8_t const body[2] = { 0, 0xFF };
		p.body.assign(body, body + 2);
		Result const r = Read(p.File());
		Check(r.opened && r.finished && r.problem == kRimFine && r.ShowsColour(0, 0, 1), "every byte packed in a run of its own is read");
	}

	{
		Picture p(32, 32, 8, 1);
		p.colours = CountingColours(2);

		for (int i = 0; i < 4 * 8 * 32; i++)
		{
			p.body.push_back(0);
			p.body.push_back(0xFF);
		}

		Result const r = Read(p.File());
		Check(r.opened && r.finished && r.problem == kRimFine, "a whole picture packed that way is read to its end");
	}

	{
		// One no-op in front of the longest honest row, with all of the row's data there after it. Only the limit can stop this.
		Picture p(8, 1, 1, 1);
		p.colours = CountingColours(2);
		uint8_t const body[3] = { 0x80, 0, 0xFF };
		p.body.assign(body, body + 3);
		Result const r = Read(p.File());
		Check(r.opened && !r.finished && r.problem == kRimBodyTooLong, "one byte past the most a picture can need is refused, though the rest of it is there");
	}

	{
		// A plain picture's data can be longer than it needs. The game's own include some, and the rest just isn't read.
		Picture p(8, 1, 1, 0);
		p.colours = CountingColours(2);
		p.body.assign(100, 0x80);
		Result const r = Read(p.File());
		Check(r.opened && r.finished && r.problem == kRimFine && r.ShowsColour(0, 0, 1), "a plain picture with more data than it needs is fine");
	}

	Picture good(1, 1, 8, 0);
	good.colours = CountingColours(1);
	good.body.assign(8, 0);
	Bytes const goodFile = good.File();

	// That picture has 4 parts: FORM, ILBM with BMHD, CMAP and BODY. The 16 is written out here, because a test that asked the code for its limit couldn't notice the limit being wrong.
	Bytes parts;

	for (int i = 0; i < 16 - 4; i++)
	{
		Add(parts, "FORM");
		Add32(parts, 0);
	}

	Bytes atTheLimit = parts;
	atTheLimit.insert(atTheLimit.end(), goodFile.begin(), goodFile.end());
	Check(Read(atTheLimit).finished, "a picture with the most parts there can be is read");

	Bytes overTheLimit = atTheLimit;
	overTheLimit.insert(overTheLimit.begin(), goodFile.begin(), goodFile.begin() + 8);
	Check(Read(overTheLimit).problem == kRimTooManyParts, "and one more is refused");

	Bytes onlyParts;

	for (int i = 0; i < 100000; i++)
	{
		Add(onlyParts, "FORM");
		Add32(onlyParts, 0);
	}

	Check(Read(onlyParts).problem == kRimTooManyParts, "a long file of nothing but parts is refused early");

	Check(ReadFrom(TestFilePath() + ".not-there").problem == kRimNoFile, "a file that isn't there says so");

	bool allSayDifferentThings = true;

	for (int i = 0; i < kRimProblemCount; i++)
	{
		std::string const words = RimLoader_ProblemWords((RimProblem)i);
		allSayDifferentThings = allSayDifferentThings && !words.empty() && words != RimLoader_ProblemWords(kRimProblemCount);

		for (int j = 0; j < i; j++)
		{
			allSayDifferentThings = allSayDifferentThings && words != RimLoader_ProblemWords((RimProblem)j);
		}
	}

	Check(allSayDifferentThings, "every reason has words of its own for the log");

	std::string const textures = ReadSourceFile("3dc/TextureManager.cpp");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menu_gfx.cpp");
	Check(textures.find("if (newRim.Problem() != kRimNoFile)") != std::string::npos, "a picture that's there and refused says why in the game, not one that isn't there");
	Check(menus.find("if (!introFontRIM.Decode(srcBuffer, width*4))") != std::string::npos, "and the menu font doesn't use rows it didn't read");
}

static void CheckDamagedPictures()
{
	printf("Damaged pictures:\n");

	Picture p(16, 16, 8, 1);
	p.stockOrder = true;
	p.colours = CountingColours(16);

	for (int i = 0; i < 16 * 8; i++)
	{
		p.body.push_back((uint8_t)(-1));
		p.body.push_back((uint8_t)RandomBits());
	}

	Bytes const good = p.File();
	int opened = 0, refused = 0, wrote = 0, finished = 0;

	for (int round = 0; round < 500; round++)
	{
		Bytes damaged = good;
		int const changes = 1 + (int)(RandomBits() % 8);

		for (int c = 0; c < changes; c++)
		{
			damaged[RandomBits() % damaged.size()] = (uint8_t)RandomBits();
		}

		if (round % 4 == 0)
		{
			damaged.resize(RandomBits() % damaged.size());
		}

		Result const r = Read(damaged);
		opened += r.opened ? 1 : 0;
		refused += r.opened ? 0 : 1;
		wrote += r.nothingPastTheEnd ? 0 : 1;
		finished += r.finished ? 1 : 0;
	}

	printf("  500 damaged copies: %d opened, %d of those finished, %d refused\n", opened, finished, refused);
	Check(opened > 50 && refused > 50, "the damage reaches both ways out");
	Check(wrote == 0, "and none of them writes past the end of its picture, and all of them end");
}

// A file inside a fastfile, which is what nearly every picture in the game is. Its neighbours are right after it, so reading on would find them.
static void CheckInsideAFastFile()
{
	printf("A picture inside a fastfile:\n");

	Picture good(8, 1, 1, 0);
	good.colours = CountingColours(2);
	good.body.assign(1, 0x80);
	Bytes const goodFile = good.File();

	containerName = TestFilePath() + ".ffl";
	memberName = "member.rim";

	Bytes const before(100, 0x77);
	Bytes const after(1000, 0xFF);
	Bytes container = before;
	container.insert(container.end(), goodFile.begin(), goodFile.end());
	container.insert(container.end(), after.begin(), after.end());

	FILE *file = fopen(containerName.c_str(), "wb");

	if (file)
	{
		fwrite(&container[0], 1, container.size(), file);
		fclose(file);
	}

	memberHandle.fastFileIndex = 0;
	memberHandle.fileOffset = (uint32_t)before.size();
	memberHandle.fileSize = (uint32_t)goodFile.size();

	{
		Result const r = ReadFrom(memberName);
		Check(r.opened && r.finished && r.ShowsColour(0, 0, 1) && r.ShowsColour(1, 0, 0), "it's read from where it is");
	}

	// The same bytes, with the file said to end one byte sooner: its body now reaches past its end, into its neighbour
	{
		memberHandle.fileSize = (uint32_t)goodFile.size() - 1;
		Check(ReadFrom(memberName).problem == kRimBodyPastEnd, "a body past the end of its own file is refused, with a neighbour there to read");
	}

	// The file said to end inside its colour table. All the rest of a good picture is right after it, so read on and it would open.
	{
		size_t const table = std::string((char const *)&goodFile[0], goodFile.size()).find("CMAP");
		memberHandle.fileSize = (uint32_t)table + 8 + 3;
		Check(ReadFrom(memberName).problem == kRimColourTablePastEnd, "and so is a file that ends inside its colour table, because no body can follow it");
	}

	// Any reader of a file inside a fastfile, not just the picture loader, stops at the end of its own bytes
	{
		memberHandle.fileSize = 4;

		FileStream member;
		member.Open(memberName, FileStream::FileRead);
		Check(member.BytesLeft() == 4, "a file inside a fastfile knows how much of it there is");

		uint8_t bytes[8];
		memset(bytes, 0x11, sizeof(bytes));
		Check(member.ReadBytes(bytes, sizeof(bytes)) == 4, "asking for more than it holds gets what it holds");
		Check(memcmp(bytes, &goodFile[0], 4) == 0 && bytes[4] == 0x11, "from the right place, and nothing more");
		Check(member.GetByte() == 0 && member.GetUint32LE() == 0, "past its end it reads as 0, with a neighbour there to read");

		member.Seek(-2, FileStream::SeekCurrent);
		Check(member.BytesLeft() == 2 && member.GetByte() == goodFile[2], "going back finds its own bytes again");
		Check(member.GetUint16LE() == goodFile[3], "and a number across its end only gets the bytes that are its own");

		member.Seek(100, FileStream::SeekCurrent);
		Check(member.GetByte() == 0, "going past its end finds nothing, though the neighbour is there");

		member.Seek(0, FileStream::SeekStart);
		Check(member.GetByte() == 0, "going before its start finds nothing, though there's something there");
	}

	memberName.clear();
	remove(containerName.c_str());
}

// What the game reads a letter's width with, on a real picture in memory, against the sums it used before
static int OldLetterWidth(uint8_t const *pixels, uint32_t textureWidth, int c, int letterWidth, int letterHeight, int widest)
{
	int const x1 = 1 + ((c - 32) & 15) * 16;
	int const y1 = 1 + ((c - 32) >> 4) * 16;
	int width = widest;

	for (int x = x1 + letterWidth; x > x1; x--)
	{
		bool blank = true;

		for (int y = y1; y < y1 + letterHeight; y++)
		{
			if (pixels[(x + y * textureWidth) * 4] >= 0x80)
			{
				blank = false;
				break;
			}
		}

		if (blank)
		{
			width--;
		}
		else
		{
			break;
		}
	}

	return width;
}

static void CheckLetterWidths()
{
	printf("How wide a letter is:\n");

	uint32_t const size = 256;
	std::vector<uint8_t> picture((size_t)size * size * 4, 0);

	Check(FontChecks_LetterWidth(&picture[0], size * 4, 65, 15, 15, 17) == 2, "a blank letter is what's left of its width");

	// letter 65 is at 1 + 1 * 16, 1 + 2 * 16
	int const left = FontChecks_LetterX(65), top = FontChecks_LetterY(65);
	picture[((size_t)top * size + left + 3) * 4] = 0x80;
	Check(FontChecks_LetterWidth(&picture[0], size * 4, 65, 15, 15, 17) == 5, "a bright pixel in its third column sets its width");
	picture[((size_t)top * size + left + 3) * 4] = 0x7F;
	Check(FontChecks_LetterWidth(&picture[0], size * 4, 65, 15, 15, 17) == 2, "one that isn't bright enough doesn't");
	picture[((size_t)top * size + left + 3) * 4] = 0;
	picture[((size_t)top * size + left + 3) * 4 + 1] = 0xFF;
	Check(FontChecks_LetterWidth(&picture[0], size * 4, 65, 15, 15, 17) == 2, "only blue is looked at");
	picture[((size_t)top * size + left + 3) * 4 + 1] = 0;
	picture[((size_t)(top + 14) * size + left + 15) * 4] = 0xFF;
	Check(FontChecks_LetterWidth(&picture[0], size * 4, 65, 15, 15, 17) == 17, "something in its last column and last row is its whole width");
	picture[((size_t)(top + 14) * size + left + 15) * 4] = 0;
	picture[((size_t)(top + 15) * size + left + 15) * 4] = 0xFF;
	picture[((size_t)top * size + left + 16) * 4] = 0xFF;
	picture[((size_t)top * size + left) * 4] = 0xFF;
	Check(FontChecks_LetterWidth(&picture[0], size * 4, 65, 15, 15, 17) == 2, "what's just outside the letter doesn't count");

	// Rows longer than the width: the letter is found through the pitch
	{
		uint32_t const pitch = size * 4 + 64;
		std::vector<uint8_t> padded((size_t)pitch * size, 0);
		padded[(size_t)top * pitch + (size_t)(left + 3) * 4] = 0xFF;
		Check(FontChecks_LetterWidth(&padded[0], pitch, 65, 15, 15, 17) == 5, "rows longer than the picture's width are found by their real length");
	}

	// the same answers as before, for every letter of a picture of noise, as every picture the game has had
	for (size_t i = 0; i < picture.size(); i++)
	{
		picture[i] = (RandomBits() % 40 == 0) ? 0xFF : 0;
	}

	bool same = true;

	for (int c = 33; c < 255; c++)
	{
		same = same && FontChecks_LetterWidth(&picture[0], size * 4, c, 15, 15, 17) == OldLetterWidth(&picture[0], size, c, 15, 15, 17);
	}

	Check(same, "every letter comes out as it did, with rows as long as the picture is wide");
}

static void CheckFontSum()
{
	printf("A font picture big enough for where its letters are read from:\n");

	Check(FontChecks_LettersFit(256, 256, 255, 15, 15), "the game's own, 256 x 256");
	Check(FontChecks_LettersFit(128, 44, 64, 15, 15), "the demo's, 128 x 44, which is smaller than the fixed layout but only goes as far as its 64 letters");
	Check(!FontChecks_LettersFit(1, 1, 255, 15, 15), "1 x 1, which is what a missing or crafted one could be");
	Check(!FontChecks_LettersFit(0, 0, 255, 15, 15), "no size at all");
	Check(!FontChecks_LettersFit(256, 16, 255, 15, 15), "wide enough, but not tall enough");
	Check(!FontChecks_LettersFit(8, 8, 64, 15, 15), "too small for even the demo's");
	Check(!FontChecks_LettersFit(128, 128, 255, 15, 15), "128 x 128 is too small for the full game's");

	// The last letter read is number 254, in place 14 of its row of the grid and on the grid's row 13. Its last pixel is at x 240, y 223, so 256 wide needs 224 rows.
	Check(FontChecks_LetterX(254) + 15 == 240 && FontChecks_LetterY(254) + 15 - 1 == 223, "the last letter ends where the grid says");
	Check(FontChecks_LettersFit(256, 224, 255, 15, 15), "just tall enough passes");
	Check(!FontChecks_LettersFit(256, 223, 255, 15, 15), "one row fewer doesn't");
	Check(FontChecks_LettersFit(256, 16, 33, 15, 15), "with no letters to read, anything fits");
	Check(FontChecks_LetterX(33) == 17 && FontChecks_LetterY(33) == 1 && FontChecks_LetterX(48) == 1 && FontChecks_LetterY(48) == 17, "letters go along the row, then on to the next");
	Check(FontChecks_LettersFit(65535, 65535, 255, 15, 15), "the largest a picture can be fits");
	Check(FontChecks_LettersFit(65536, 65536, 255, 15, 15), "and a size whose area doesn't fit in 32 bits isn't taken for no pixels at all");

	// Places chosen so that the very last letter is the only one that doesn't fit: its last pixel, at x 240, y 223, is number 240 + 223 * width.
	Check(!FontChecks_LettersFit(230, 224, 255, 15, 15), "only the last letter past the end, and its width is what takes it there: that's refused");
	Check(!FontChecks_LettersFit(240, 224, 255, 15, 15), "its last pixel exactly one past the end of the picture: refused");
	Check(FontChecks_LettersFit(241, 224, 255, 15, 15), "and exactly on the last pixel of the picture: fits");

	// As wide as a card allows, and one pixel tall. Each letter's row is multiplied by the width, so this would read 14.6 MB past the end of the picture.
	Check(!FontChecks_LettersFit(16384, 1, 255, 15, 15), "16384 x 1 is refused, because width alone doesn't make up for height");

	std::string const loader = ReadSourceFile("3dc/RimLoader.cpp");
	std::string const textures = ReadSourceFile("3dc/TextureManager.cpp");

	// What's left of a picture isn't read once its data has run out. Those reads would only give zeros, so a picture alone can't show this, only the time it takes.
	Check(loader.find("y < header.height && !ranOut") != std::string::npos, "no more rows are read once the data has run out");

	// The game's side of a picture that can't be finished
	Check(textures.find("if (!decoded)") != std::string::npos && textures.find("R_ReleaseTexture(newTexture);", textures.find("if (!decoded)")) != std::string::npos, "the game doesn't keep a picture that couldn't be finished");

	// What's past the end of a table is whatever memory holds, which can happen to be zero, so a picture alone can't always show this.
	Check(loader.find("if (index < numColours)") != std::string::npos, "a colour is only looked up if it's in the table");

	std::string const menu = ReadSourceFile("3dc/avp/win95/frontend/menu_gfx.cpp");
	size_t const function = menu.rfind("void CalculateWidthsOfAAFont()");
	size_t const check = (function == std::string::npos) ? function : menu.find("FontChecks_LettersFit(textureWidth, textureHeight, nChars, HUD_FONT_WIDTH, HUD_FONT_HEIGHT)", function);
	size_t const lock = (function == std::string::npos) ? function : menu.find("Tex_Lock(AAFontImageNumber", function);
	Check(function != std::string::npos && check != std::string::npos && lock != std::string::npos && check < lock, "the game checks that before it reads any letters");

	Check(menu.find("if (pitch < textureWidth * 4)") != std::string::npos, "a picture whose rows are shorter than its width isn't read");
	Check(menu.find("FontChecks_LetterWidth(originalSrcPtr, pitch, c, HUD_FONT_WIDTH, HUD_FONT_HEIGHT, MAX_SMALL_LETTER_WIDTH)") != std::string::npos, "each letter is read with the rows the card gives it");
	Check(CountOf(menu, "GiveEveryLetterOneWidth();") == 3, "a font that's missing, too small or laid out wrong gets letters that take up room");
}

int main()
{
	CheckGoodPictures();
	CheckRefusals();
	CheckBadPixels();
	CheckLongFiles();
	CheckDamagedPictures();
	CheckInsideAFastFile();
	CheckLetterWidths();
	CheckFontSum();

	remove(TestFilePath().c_str());

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
