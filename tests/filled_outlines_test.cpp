// The test of DrawFilledOutlines, the level editor's Filled polyline switch (R-DRAWMODES-1). Written 2026-11-14 by Melanikus (Apone)
// for the AvP port.
//
// WHAT IT'S FOR: the map editor, AVP_OpenInteng, can draw each solid polygon's outline over it (inteng's "filled polyline", key 9). One
// switch, DrawFilledOutlines, turns on a second pass over the main list, and the static scene's route stands aside while it's on. The
// game itself never turns it on, so the game has to draw exactly as before:
// - the switch is 0 unless something turns it on, and the outline pass runs only inside a test of it;
// - the static scene's refusal reads as before, with only the switch added;
// - the outline pass saves every setting of the card before it changes one, and puts them all back on its one way out;
// - nothing in the game sets the switch or takes its address (which is how a console word or a saved setting would reach it).
//
// HOW: read, not run, because drawing needs the game's real device. It walks 3dc and win32 for the last rule.

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <string>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A source file, as text, with its CRs taken out. The names are this file's own or the tree walk's, never from outside.
static std::string ReadSource(const std::string &name)
{
	FILE *file = fopen(name.c_str(), "rb");
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

static size_t Count(const std::string &text, const std::string &what)
{
	size_t count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static void TestTheSwitchAndThePass()
{
	puts("The switch, at 0 as before:");

	std::string const render = ReadSource("3dc/d3d_render.cpp");
	Check(Count(render, "int DrawFilledOutlines = 0;") == 1, "the switch is defined once, in d3d_render.cpp, and is 0 unless something turns it on");

	// The pass is called once, and only inside the test of the switch
	std::string const hook = "\tif (DrawFilledOutlines && mainList->GetSize())\n\t{\n\t\tDrawOutlinesOverTheSolids();\n\t}\n";
	Check(Count(render, hook) == 1 && Count(render, "\tDrawOutlinesOverTheSolids();") == 1,
		"the outline pass runs only inside a test of the switch, so at 0 only that test is new");

	std::string const scene = ReadSource("3dc/static_scene.cpp");
	char const *const before = "if (CurrentVisionMode != VISION_MODE_NORMAL || MirroringActive || DrawFullBright || WireFrameMode || !StaticScene_ViewAllows())";
	char const *const after = "if (CurrentVisionMode != VISION_MODE_NORMAL || MirroringActive || DrawFullBright || WireFrameMode || DrawFilledOutlines || !StaticScene_ViewAllows())";
	Check(Count(scene, after) == 1 && Count(scene, before) == 0, "the static scene's route is refused as before, and also while the switch is on");

	puts("The outline pass puts the card back:");

	std::string::size_type const start = render.find("static void DrawOutlinesOverTheSolids()\n{");
	std::string::size_type const end = start == std::string::npos ? start : render.find("\n}\n", start);
	std::string const pass = (start == std::string::npos || end == std::string::npos) ? std::string() : render.substr(start, end - start);
	std::string::size_type const saved = pass.find("CreateStateBlock(D3DSBT_ALL, &asItWas)");
	std::string::size_type const firstSet = pass.find("SetRenderState(");
	std::string::size_type const applied = pass.find("asItWas->Apply();");
	std::string::size_type const released = pass.find("asItWas->Release();");

	Check(saved != std::string::npos && firstSet != std::string::npos && saved < firstSet, "it saves every setting of the card before it changes one");
	Check(applied != std::string::npos && released != std::string::npos && applied < released && released > firstSet,
		"and puts them all back, then lets the copy go, after it draws");

	// Its only early way out is the refusal before anything is saved or set
	std::string::size_type const earlyReturn = pass.find("return;");
	Check(Count(pass, "return") == 1 && earlyReturn != std::string::npos && earlyReturn < firstSet,
		"its only early way out comes before anything is changed");
	Check(pass.find("DrawOpaqueAgain(") != std::string::npos && pass.find("D3DRS_ZWRITEENABLE, FALSE") != std::string::npos,
		"it draws only the solid polygons again, and writes no depth");
}

static void TestNothingSetsIt()
{
	puts("Nothing in the game sets the switch:");

	size_t files = 0;
	size_t definitions = 0;
	bool neverSet = true;
	std::error_code missing;

	for (const char *top : { "3dc", "win32" })
	{
		for (std::filesystem::recursive_directory_iterator walk(top, missing), end; !missing && walk != end; walk.increment(missing))
		{
			std::string const extension = walk->path().extension().string();

			if (!walk->is_regular_file() || (extension != ".cpp" && extension != ".c" && extension != ".h" && extension != ".hpp"))
			{
				continue;
			}

			files++;
			std::string const text = ReadSource(walk->path().string());
			std::string const name = "DrawFilledOutlines";

			for (std::string::size_type at = text.find(name); at != std::string::npos; at = text.find(name, at + 1))
			{
				std::string::size_type after = at + name.size();

				while (after < text.size() && (text[after] == ' ' || text[after] == '\t'))
				{
					after++;
				}

				bool const assigned = after < text.size() && ((text[after] == '=' && (after + 1 >= text.size() || text[after + 1] != '='))
					|| (after + 1 < text.size() && (text.compare(after, 2, "++") == 0 || text.compare(after, 2, "--") == 0
						|| text.compare(after, 2, "+=") == 0 || text.compare(after, 2, "-=") == 0 || text.compare(after, 2, "|=") == 0
						|| text.compare(after, 2, "^=") == 0 || text.compare(after, 2, "&=") == 0)));
				bool const addressed = at > 0 && text[at - 1] == '&';
				bool const isTheDefinition = text.compare(at > 4 ? at - 4 : 0, 4, "int ") == 0 && text.compare(at, name.size() + 5, name + " = 0;") == 0;

				if (isTheDefinition)
				{
					definitions++;
				}
				else if (assigned || addressed)
				{
					printf("        %s sets DrawFilledOutlines or takes its address\n", walk->path().string().c_str());
					neverSet = false;
				}
			}
		}
	}

	Check(!missing && files > 100, "SOURCE: the game's 3dc and win32 folders are read");
	Check(neverSet && definitions == 1, "nothing in them sets the switch or takes its address: no console word, saved setting or file can turn it on");
}

int main()
{
	// Print each line straight away
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheSwitchAndThePass();
	TestNothingSetsIt();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
