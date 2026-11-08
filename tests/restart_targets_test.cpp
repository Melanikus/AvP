// A test that restarting a level leaves nothing aimed at what was there before it. Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// When a level restarts, everything in it is let go and made again. The sight's locked target is kept in two places outside the level
// (SmartTarget_Object and Old_SmartTarget_Object, targeting.cpp). Starting a level forgets them, and restarting one now does too
// (ReInitHUD, hud.cpp, which the restart calls). And a thrown disc keeps its target with that target's name, which it checks every frame.
// Every object in the level has a name, so a target whose name is nothing isn't an object any more, and the disc now lets it go.
//
// None of this runs on its own: it needs the whole game. So the test reads the game's source (/FC in its settings makes __FILE__ the
// whole path of this file, which is how the source is found), and a take-out row for each check shows the check notices its line going.
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(const char *name)
{
	std::string const path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return std::string();
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	std::string plain;

	for (char const letter : all.str())
	{
		if (letter != '\r')
		{
			plain += letter;
		}
	}

	return plain;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Has(const std::string &text, const char *part)
{
	return text.find(part) != std::string::npos;
}

// Whether first is there, and before second
static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const a = text.find(first), b = text.find(second);
	return a != std::string::npos && b != std::string::npos && a < b;
}

int main()
{
	std::string const hud = ReadSourceFile("3dc/avp/hud.cpp");
	std::string const system = ReadSourceFile("3dc/avp/win95/system.cpp");
	std::string const disc = ReadSourceFile("3dc/avp/bh_weapon.cpp");
	Check(!hud.empty() && !system.empty() && !disc.empty(), "SOURCE: the game's files can be read");

	// Restarting forgets the sight's target
	std::string const reinit = Between(hud, "void ReInitHUD(void)\n{", "\n}\n");
	Check(Has(reinit, "\tSmartTarget_Object = NULL;\n"), "SOURCE: restarting a level forgets the target the sight had locked");
	Check(Has(reinit, "\tOld_SmartTarget_Object = NULL;\n"), "SOURCE: and the one it had locked before that");
	std::string const restart = Between(system, "void RestartLevel()\n{", "\n}\n");
	Check(Has(restart, "\tReInitHUD();\n"), "SOURCE: and the restart does call that");

	// A disc lets go of a target with no name, before asking anything else about it
	std::string const homing = Between(disc, "textprint(\"Disc homing on target.\\n\");", "/* Target no longer valid - unset. */");
	Check(Has(homing, "if (!NAME_ISNULL(bbPtr->Target_SBname) && NAME_ISEQUAL(bbPtr->Target_SBname,bbPtr->Target->SBname)) {"),
		"SOURCE: a disc whose target's name is nothing lets it go, as one whose name has changed");
	Check(ComesBefore(homing, "!NAME_ISNULL(bbPtr->Target_SBname)", "NPC_IsDead(bbPtr->Target)"),
		"SOURCE: and it asks that before it asks anything else about the target");

	if (checksFailed)
	{
		printf("The restart targets test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	printf("The restart targets test passed\n");
	return 0;
}
