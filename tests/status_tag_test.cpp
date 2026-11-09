// Tests that the tag on the top right of the screen is off until it's asked for. Written 2026-11-09 by Melanikus for the AvP port.
//
// The tag (ShadowStatus_Draw in 3dc/d3d_render.cpp) says how the shadows and the lighting are set: KEPT PRISMS, LIGHTING, RELIEF DEPTH,
// STATIC, PIXEL GLOW, the view, COMMIT, FRAME, ROOM and the keys. Every run with -dev used to show it, Hudson's films included, and it
// covered the top right of the game, where the relay's line in play is. The user's ruling (2026-11-09): off by default, even with -dev,
// and shown only when asked for. So:
// - it starts off;
// - it's only drawn with -dev, and while it's on;
// - F7 shows and hides it in play, and says so in the log each time;
// - -dev -statuson starts with it on, and needs -dev;
// - -statusoff, which Hudson's launch lines still say, still means off, even after -statuson.
// The code is the game's own and needs the whole game to run, so this reads its source, as view_keys_test does. Every row of
// tests/takeouts/status_tag_test.json is a SOURCE: row: it shows the line is there, and what the line does on screen needs a run of the
// game to see.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include "source_slice.h"

static int tagChecksFailed = 0;

// Called TagCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void TagCheck(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	tagChecksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int CountOf(const std::string &text, const char *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndFromTheStart, [](const std::string &why) { TagCheck(false, why.c_str()); });
}

int main()
{
	std::string const render = ReadSourceFile("3dc/d3d_render.cpp");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	TagCheck(!render.empty() && !winmain.empty(), "read 3dc/d3d_render.cpp and 3dc/avp/win95/winmain.cpp");

	// It starts off, and nothing else in d3d_render.cpp turns it on
	TagCheck(Has(render, "\nint ShadowStatusOn = 0;"), "SOURCE: the tag starts off, with -dev or without");
	TagCheck(CountOf(render, "ShadowStatusOn = 1") == 0, "SOURCE: nothing in d3d_render.cpp turns it on");

	// It's only drawn with -dev, and while it's on
	std::string const draw = Between(render, "static void ShadowStatus_Draw()\n{", "enum { kMostLines");
	TagCheck(Has(draw, "if (!DevMode || !ShadowStatusOn || !viewRan)\n\t{\n\t\treturn;\n\t}"),
	         "SOURCE: the tag is drawn only with -dev, only while it's on, and only on a frame the level was drawn");

	// F7 shows and hides it, only where the developer's keys work, and says so in the log each time
	std::string const keys = Between(render, "if (DevKeysCanAct())", "// F9 shoots");
	std::string const f7 = Between(keys, "if (DebouncedKeyboardInput[KEY_F7])", "}");
	TagCheck(Has(f7, "ShadowStatusOn = !ShadowStatusOn;"), "SOURCE: F7 shows the tag when it's hidden, and hides it when it's shown");
	TagCheck(Has(f7, "LogString(ShadowStatusOn ? \"tag: shown on the top right (F7)\" : \"tag: hidden (F7)\");"),
	         "SOURCE: and the log says which, each time");

	// -dev -statuson starts with it on, and nothing else in winmain.cpp does
	std::string const statusOn = Between(winmain, "if (devSwitch(\"-statuson\"))", "}");
	TagCheck(Has(statusOn, "ShadowStatusOn = 1;"), "SOURCE: -dev -statuson starts with the tag on");
	TagCheck(!Has(winmain, "CommandLineHasWord(command_line, \"-statuson\")") && !Has(winmain, "CommandLineFindSwitch(command_line, \"-statuson\")"),
	         "SOURCE: -statuson is a developer's switch (devSwitch): without -dev it's off, and the log says why");
	TagCheck(CountOf(winmain, "ShadowStatusOn = 1;") == 1, "SOURCE: nothing else at the start turns the tag on");

	// -statusoff still means off, and it's read after -statuson, so it wins
	std::string const statusOff = Between(winmain, "if (CommandLineHasWord(command_line, \"-statusoff\"))", "}");
	TagCheck(Has(statusOff, "ShadowStatusOn = 0;") && Has(statusOff, "LogString("), "SOURCE: -statusoff still means off, and the log says so");
	std::string::size_type const on = winmain.find("if (devSwitch(\"-statuson\"))");
	std::string::size_type const off = winmain.find("if (CommandLineHasWord(command_line, \"-statusoff\"))");
	TagCheck(on != std::string::npos && off != std::string::npos && on < off, "SOURCE: -statusoff is read after -statuson, so a line with both gets off");

	printf("%s\n", tagChecksFailed ? "SOME CHECKS FAILED" : "The tag test passed");
	return tagChecksFailed ? 1 : 0;
}
