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

FastFileHandle *FF_Find(const std::string &) { return 0; }

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string const none;
	return none;
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
	uint32_t width, height;
	std::vector<uint8_t> pixels;
	bool nothingPastTheEnd;

	Result() : opened(false), width(0), height(0), nothingPastTheEnd(true) {}

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

static Result Read(Bytes const &bytes)
{
	Result result;
	WriteFile(bytes);

	RimLoader picture;
	result.opened = picture.Open(TestFilePath());

	if (result.opened)
	{
		picture.GetDimensions(result.width, result.height);
		size_t const size = (size_t)result.width * result.height * 4;

		// a full-sized picture would take a long time and tell us nothing more
		if (size <= (1u << 24))
		{
			std::vector<uint8_t> memory(size + kWatched, kPattern);
			picture.Decode(&memory[0], result.width * 4);

			for (size_t i = size; i < memory.size(); i++)
			{
				result.nothingPastTheEnd = result.nothingPastTheEnd && (memory[i] == kPattern);
			}

			memory.resize(size);
			result.pixels.swap(memory);
		}
	}

	return result;
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

	uint32_t const tooMany[3] = { 17, 33, 255 };

	for (int i = 0; i < 3; i++)
	{
		Picture p(1, 1, tooMany[i], 0);
		p.colours = CountingColours(1);
		p.body.assign(tooMany[i], 0);
		Check(!Read(p.File()).opened, std::to_string(tooMany[i]) + " planes is refused, because the sum that works out a colour number can't hold them");
	}

	uint32_t const compressions[3] = { 2, 3, 255 };

	for (int i = 0; i < 3; i++)
	{
		Picture p(1, 1, 8, compressions[i]);
		p.colours = CountingColours(1);
		p.body.assign(8, 0);
		Check(!Read(p.File()).opened, "packing method " + std::to_string(compressions[i]) + " is refused, because it can't be read");
	}

	Check(!Read(Bytes()).opened, "an empty file is refused");

	{
		Bytes junk;
		Add(junk, "JUNK");
		Add32(junk, 0);
		Check(!Read(junk).opened, "a file that starts with something else is refused");
	}

	{
		Bytes file;
		Add(file, "FORM");
		Add32(file, 0);
		Add(file, "ILBM");
		Check(!Read(file).opened, "a file that ends before its picture is refused, instead of being searched forever");
	}

	{
		Bytes file;
		Add(file, "ILBM");
		Add(file, "JUNK");
		Check(!Read(file).opened, "a part it doesn't know is refused");
	}

	{
		Bytes file;
		Add(file, "CMAP");
		Add32(file, 3u * 65537u);
		Check(!Read(file).opened, "a colour table bigger than it can be is refused");
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
		// Only no-ops, all the way to the end of the file. They have to end, and zeros past the end do that.
		Picture p(8, 2, 1, 1);
		p.colours = CountingColours(2);
		p.body.assign(5000, 0x80);
		Result const r = Read(p.File());
		Check(r.opened && r.nothingPastTheEnd, "a picture of nothing but no-ops ends, and writes nothing past the end");
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
		Check(r.opened && r.ShowsColour(0, 0, 0) && r.nothingPastTheEnd, "a picture with no planes is all colour 0");
	}

	{
		// the file stops part way through the picture, both ways of packing
		for (uint32_t compression = 0; compression < 2; compression++)
		{
			Picture p(32, 32, 8, compression);
			p.colours = CountingColours(4);
			p.body.assign(10, 0);
			Result const r = Read(p.File());
			Check(r.opened && r.nothingPastTheEnd, std::string(compression ? "packed" : "plain") + ", cut short: it reads what's there, and ends");
		}
	}

	{
		Picture p(0, 0, 8, 0);
		Result const r = Read(p.File());
		Check(r.opened && r.width == 0 && r.height == 0, "a picture with no size has nothing to write");
	}

	{
		// as wide as it can say
		Picture p(65535, 1, 1, 0);
		p.colours = CountingColours(2);
		Result const r = Read(p.File());
		Check(r.opened && r.width == 65535 && r.nothingPastTheEnd, "65535 wide: nothing is written past the end of its row");
	}

	{
		// a second colour table replaces the first
		Picture p(1, 1, 1, 0);
		p.colours = CountingColours(2);
		Bytes file = p.File();
		Bytes extra;
		Add(extra, "CMAP");
		Add32(extra, 6);
		uint8_t const second[6] = { 9, 9, 9, 50, 60, 70 };
		extra.insert(extra.end(), second, second + 6);
		size_t const body = std::string((char const *)&file[0], file.size()).find("BODY");
		file.insert(file.begin() + body, extra.begin(), extra.end());
		file.push_back(0x80);
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
	int opened = 0, refused = 0, wrote = 0;

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
	}

	printf("  500 damaged copies: %d opened, %d refused\n", opened, refused);
	Check(opened > 50 && refused > 50, "the damage reaches both ways out");
	Check(wrote == 0, "and none of them writes past the end of its picture, and all of them end");
}

static void CheckFontSum()
{
	printf("A font picture big enough for where its letters are read from:\n");

	Check(FontChecks_LettersFit(256, 256, 255, 15, 15), "the game's own, 256 x 256");
	Check(FontChecks_LettersFit(128, 44, 64, 5, 8), "the demo's, 128 x 44, which is smaller than the fixed layout but only goes as far as its 64 letters");
	Check(!FontChecks_LettersFit(1, 1, 255, 15, 15), "1 x 1, which is what a missing or crafted one could be");
	Check(!FontChecks_LettersFit(0, 0, 255, 15, 15), "no size at all");
	Check(!FontChecks_LettersFit(256, 16, 255, 15, 15), "wide enough, but not tall enough");
	Check(!FontChecks_LettersFit(8, 8, 64, 5, 8), "too small for even the demo's");

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

	// What's past the end of a table is whatever memory holds, which can happen to be zero, so a picture alone can't always show this.
	Check(loader.find("if (index < numColours)") != std::string::npos, "a colour is only looked up if it's in the table");

	std::string const menu = ReadSourceFile("3dc/avp/win95/frontend/menu_gfx.cpp");
	size_t const function = menu.rfind("void CalculateWidthsOfAAFont()");
	size_t const check = (function == std::string::npos) ? function : menu.find("FontChecks_LettersFit(textureWidth, textureHeight, nChars, HUD_FONT_WIDTH, HUD_FONT_HEIGHT)", function);
	size_t const lock = (function == std::string::npos) ? function : menu.find("Tex_Lock(AAFontImageNumber", function);
	Check(function != std::string::npos && check != std::string::npos && lock != std::string::npos && check < lock, "the game checks that before it reads any letters");

	size_t const letterPlace = (function == std::string::npos) ? function : menu.find("int x1 = FontChecks_LetterX(c);", function);
	Check(letterPlace != std::string::npos, "and reads them from where the check says they are");
	Check(menu.find("uint8_t *s = &srcPtr[y*pitch + x*4];") != std::string::npos, "with each row found from the length the card gives it, which is never shorter than the check's");
}

int main()
{
	CheckGoodPictures();
	CheckRefusals();
	CheckBadPixels();
	CheckDamagedPictures();
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
