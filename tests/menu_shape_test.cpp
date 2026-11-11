// The test of how the menus are drawn on a wide screen (3dc/menu_shape.h). Written 2026-11-10 by Melanikus for the AvP port, at the owner's
// word: "True shape, starfield full width", then "Yes, make it the default".
//
// WHAT IT TESTS
//
// 1. The rule (menu_shape.h): on a screen wider than 4:3, the menus' 640 by 480 grid takes only the 4:3 part in the middle, so one step of
//    the grid is as many pixels across as it is down, and a letter keeps its true shape; a 4:3 screen, or a narrower one, is drawn exactly
//    as before; the starfield always takes the whole width; and anything that isn't a screen's size gets the whole width, as before.
// 2. That the game uses it, by reading its source: d3d_render.cpp's WPos2DC scales the grid by it, around the middle of the screen, and
//    leaves the height alone; and menus.cpp says when it's the starfield being drawn, around that one draw. What it looks like on a real
//    screen needs Hudson's pictures (menus=pictures).
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own, run from the top of the repository, where the source it reads is found from. The program
// gives back a non-zero result if anything failed.

#include <math.h>
#include <stdio.h>
#include <string>

#include "menu_shape.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static bool Has(const std::string &text, const char *part)
{
	return text.find(part) != std::string::npos;
}

static size_t CountOf(const std::string &text, const char *part)
{
	size_t count = 0;

	for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
	{
		count++;
	}

	return count;
}

static bool Near(double a, double b)
{
	return fabs(a - b) < 1e-5;
}

// How many pixels one step of the grid is across, and down, at a screen size: the grid's 640 steps fill the share of the width it takes,
// and its 480 fill the whole height
static double StepAcross(int wide, int high, bool backdrop)
{
	return wide * (double)MenuShape_WidthShare(wide, high, backdrop) / 640.0;
}

static double StepDown(int high)
{
	return high / 480.0;
}

static void TestTheRule()
{
	puts("1. the rule");

	// Screens wider than 4:3, as players have them: 16:9 at three sizes, 16:10, and the two ultra-wide ones Hudson can run at
	int const wide[][2] = { { 1920, 1080 }, { 1280, 720 }, { 2560, 1440 }, { 1680, 1050 }, { 3440, 1440 }, { 2560, 1080 } };
	bool trueShape = true;
	bool narrowed = true;

	for (size_t i = 0; i < sizeof(wide) / sizeof(wide[0]); i++)
	{
		trueShape = trueShape && Near(StepAcross(wide[i][0], wide[i][1], false), StepDown(wide[i][1]));
		narrowed = narrowed && MenuShape_WidthShare(wide[i][0], wide[i][1], false) < 1.0f;
	}

	Check(trueShape, "on a screen wider than 4:3, one step of the grid is as many pixels across as down, so a letter keeps its true shape");
	Check(narrowed, "and the grid takes less than the whole width there");
	Check(Near(MenuShape_WidthShare(1920, 1080, false), 0.75), "at 1920x1080 it takes the middle three quarters: 1440 of the 1920 pixels");

	Check(MenuShape_WidthShare(1024, 768, false) == 1.0f && MenuShape_WidthShare(640, 480, false) == 1.0f && MenuShape_WidthShare(1600, 1200, false) == 1.0f,
		"a 4:3 screen is drawn exactly as before, the grid filling it");
	Check(MenuShape_WidthShare(1280, 1024, false) == 1.0f, "and so is a narrower one, such as 5:4");

	Check(MenuShape_WidthShare(1920, 1080, true) == 1.0f && MenuShape_WidthShare(3440, 1440, true) == 1.0f && MenuShape_WidthShare(1024, 768, true) == 1.0f,
		"the starfield always takes the whole width");

	Check(MenuShape_WidthShare(0, 1080, false) == 1.0f && MenuShape_WidthShare(1920, 0, false) == 1.0f && MenuShape_WidthShare(-1920, 1080, false) == 1.0f &&
		MenuShape_WidthShare(1920, -1080, false) == 1.0f && MenuShape_WidthShare(70000, 1080, false) == 1.0f && MenuShape_WidthShare(0x7FFFFFFF, 0x7FFFFFFF, false) == 1.0f,
		"and a size that isn't a screen's (none, below none, or past 65535) gets the whole width, as before, with nothing that could overflow");
}

static void TestTheGameUsesIt()
{
	puts("2. the game uses it (by reading its source)");

	std::string const render = ReadSource("3dc/d3d_render.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	Check(render.size() > 10000 && menus.size() > 10000, "d3d_render.cpp and menus.cpp were found and read");

	std::string const share = SourceSlice(render, "static float MenuWidthShare(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(render, "#include \"menu_shape.h\"") &&
		Has(share, "return MenuShape_WidthShare(ScreenDescriptorBlock.SDB_Width, ScreenDescriptorBlock.SDB_Height, MenuBackdropFullWidth);"),
		"d3d_render.cpp asks the rule, with the screen's size and whether it's the starfield");

	std::string const across = SourceSlice(render, "inline float WPos2DC(int32_t pos)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(across, "if ((GetAvPMenuState() != MENUSSTATE_STARTGAME)) {\n\t\treturn ((float(pos / (float)640) * 2) - 1) * MenuWidthShare();"),
		"and WPos2DC scales the menus' grid by it around the screen's middle (the device's 0), only in the menus");

	std::string const down = SourceSlice(render, "inline float HPos2DC(int32_t pos)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(down, "if ((GetAvPMenuState() != MENUSSTATE_STARTGAME)) {\n\t\treturn (float(pos / (float)480) * 2) - 1;"),
		"while the height still takes the whole screen");

	std::string const backdrop = SourceSlice(menus, "extern void DrawMainMenusBackdrop(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const on = backdrop.find("MenuBackdropFullWidth = true;");
	size_t const draw = backdrop.find("DrawAvPMenuGfx(AVPMENUGFX_BACKDROP, 0, 0, ONE_FIXED+1, AVPMENUFORMAT_LEFTJUSTIFIED);");
	size_t const off = backdrop.find("MenuBackdropFullWidth = false;");
	Check(on != std::string::npos && draw != std::string::npos && off != std::string::npos && on < draw && draw < off,
		"menus.cpp says it's the starfield just before its one draw, and stops saying so just after");
	Check(CountOf(menus, "MenuBackdropFullWidth = true;") == 1 && CountOf(render, "MenuBackdropFullWidth = true;") == 0,
		"and nothing else gets the whole width that way");
}

int main()
{
	TestTheRule();
	TestTheGameUsesIt();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
