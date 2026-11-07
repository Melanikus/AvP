// The test of a font's cell sizes, in 3dc/font_cells.h. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A font's .dat file says how large its letters' cells are, and finding a letter divides by them. This checks the sum that says whether they can be used, and reads the game's source to see that
// it's asked. Font_Load needs a graphics card, so that part can't be run here.
//
// Nothing in the game loads a font today: Con_Init, the only thing that asks for one, returns before it does. So this guards the day that changes, not anything a player can reach now.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this on its own. The program gives back a non-zero result if anything failed.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

#include "font_cells.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
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

int main()
{
	printf("A font's cells:\n");

	Check(Font_CellsFit(512, 512, 32, 32), "cells smaller than the picture fit");
	Check(Font_CellsFit(512, 256, 512, 256), "and so does one cell exactly the picture's size");
	Check(Font_CellsFit(1, 1, 1, 1), "the smallest there can be");
	Check(!Font_CellsFit(512, 512, 0, 32), "a cell of no width would divide by zero");
	Check(!Font_CellsFit(512, 512, 32, 0), "and one of no height");
	Check(!Font_CellsFit(512, 512, 513, 32), "a cell wider than the picture leaves no columns to divide by");
	Check(!Font_CellsFit(512, 512, 32, 513), "and one taller leaves no rows");
	Check(!Font_CellsFit(0, 0, 32, 32), "a picture with no size has room for none");
	Check(!Font_CellsFit(512, 512, 0xFFFFFFFF, 0xFFFFFFFF), "the largest a file can say doesn't fit either");

	printf("What the game does with that:\n");

	std::string const fonts = ReadSourceFile("3dc/Fonts.cpp");
	size_t const load = fonts.find("bool Font_Load(");
	size_t const size = (load == std::string::npos) ? load : fonts.find("if (kFontDATsize != fileLength)", load);
	size_t const reads = (load == std::string::npos) ? load : fonts.find("file.GetUint32LE()", load);
	size_t const dimensions = (load == std::string::npos) ? load : fonts.find("Tex_GetDimensions(Fonts[fontType].textureID", load);
	size_t const fit = (load == std::string::npos) ? load : fonts.find("Font_CellsFit(", load);
	size_t const good = (load == std::string::npos) ? load : fonts.find("Fonts[fontType].isValid = true;", load);

	Check(size != std::string::npos && reads != std::string::npos && size < reads, "a .dat of the wrong size is refused before it's read");
	Check(dimensions != std::string::npos && fit != std::string::npos && good != std::string::npos && dimensions < fit && fit < good, "the cells are checked against the picture's real size before the font counts as good");

	std::string const console = ReadSourceFile("3dc/console.cpp");
	size_t const start = console.rfind("void Con_Init()");
	size_t const returns = (start == std::string::npos) ? start : console.find("return;", start);
	size_t const asks = (start == std::string::npos) ? start : console.find("Font_Load(", start);
	Check(start != std::string::npos && returns != std::string::npos && asks != std::string::npos && returns < asks, "nothing loads a font today, as this test's heading says. If this fails, a font can be loaded now: read Font_Load again");

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
