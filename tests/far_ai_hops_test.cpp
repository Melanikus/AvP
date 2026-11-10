// The test of the far AI's hops in the log (3dc/avp/far_ai_hops.h, #109). Written 2026-11-10 by Melanikus (Third) for the AvP port.
//
// WHAT IT TESTS
//
// - The line, from the header: what it says for a hop onto an entry point in view and onto a far location out of view, a missing name
//   or kind, a module name from the level file cut short, and a line too long for the caller's room, which is still ended.
// - The limit: kFarAiHopsALevel lines a level, then one saying the rest aren't written, then nothing, until the count is set to 0 again.
// - By reading the game's files: bh_far.cpp says where each hop goes, after the check that can call it off and before the NPC is moved,
//   onto the entry point unless a far location is chosen, looks up the shape only by a number the list has, and starts the count again at
//   each level's start, which game.cpp asks for beside BuildFarModuleLocs.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/far_ai_hops_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/far_ai_hops_test.json proves each check can fail.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "far_ai_hops.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// How tests/source_slice.h fails a check when a marker isn't there
static void FailBecause(std::string const &why)
{
	Check(false, why);
}

// A file of the game's, with its line ends written \n whatever the checkout made them. /FC in this test's settings makes __FILE__ this
// file's whole path, so the repository's top is found from it.
static std::string ReadSourceFile(char const *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, FailBecause);
}

// The hop seen in the Genshd1 film: an alien into PROCm07 from procpath04, onto their entry point, with a body standing on its feet
static FarAiHop TheFilmsHop()
{
	FarAiHop hop;

	hop.kind = "alien";
	hop.from = "procpath04";
	hop.to = "PROCm07";
	hop.entryPoint = true;
	hop.inView = true;
	hop.x = -37690;
	hop.y = 1940;
	hop.z = -62250;
	hop.shapeLowestY = -1100;
	hop.shapeHighestY = 0;
	return hop;
}

static void TestTheLine()
{
	printf("The line\n");
	char out[400];
	int logged = 0;
	FarAiHop hop = TheFilmsHop();

	Check(FarAiHops_Line(hop, &logged, out, sizeof(out)) && logged == 1, "a hop gives a line, and counts it");
	Check(strcmp(out, "far AI: alien went from module procpath04 to PROCm07, onto the entry point, in view, put at -37690 1940 -62250; "
		"its shape goes from -1100 to 0 about that point") == 0, std::string("and says what, where and how its shape sits: ") + out);

	hop.kind = "marine";
	hop.entryPoint = false;
	hop.inView = false;
	hop.shapeLowestY = -550;
	hop.shapeHighestY = 550;
	FarAiHops_Line(hop, &logged, out, sizeof(out));
	Check(strstr(out, "far AI: marine went from module procpath04 to PROCm07, onto a far location, out of view,") == out, "onto a far location, out of view");
	Check(strstr(out, "its shape goes from -550 to 550 about that point") != 0, "with its shape about its middle");

	hop.kind = 0;
	hop.from = 0;
	hop.to = 0;
	FarAiHops_Line(hop, &logged, out, sizeof(out));
	Check(strstr(out, "far AI: npc went from module (no name) to (no name),") == out, "no kind is an npc, and no name says so");

	// A level file's name is anyone's: only the first 32 characters of it go in
	std::string const longName(50, 'Q');
	hop.from = longName.c_str();
	hop.to = "PROCm07";
	FarAiHops_Line(hop, &logged, out, sizeof(out));
	Check(strstr(out, (std::string(" ") + std::string(32, 'Q') + " to PROCm07").c_str()) != 0, "a long module name is cut to 32 characters");
	Check(strstr(out, std::string(33, 'Q').c_str()) == 0, "and no more");

	char tight[20];
	hop = TheFilmsHop();
	Check(FarAiHops_Line(hop, &logged, tight, sizeof(tight)) && strlen(tight) == sizeof(tight) - 1, "a line too long for the room is cut, and ended");
	Check(!FarAiHops_Line(hop, &logged, out, 0), "no room gives no line");
	strcpy(out, "unchanged");
	Check(!FarAiHops_Line(hop, 0, out, sizeof(out)) && out[0] == 0, "with no count there's no line, and out is ended");
}

static void TestTheLimit()
{
	printf("The limit\n");
	char out[400];
	int logged = 0;
	FarAiHop const hop = TheFilmsHop();
	bool everyLine = true;

	for (int i = 0; i < kFarAiHopsALevel; i++)
	{
		everyLine = everyLine && FarAiHops_Line(hop, &logged, out, sizeof(out)) && strncmp(out, "far AI: alien went", 18) == 0;
	}

	Check(everyLine && logged == kFarAiHopsALevel, "a level's first 400 hops each have a line");
	Check(FarAiHops_Line(hop, &logged, out, sizeof(out)) && strcmp(out, "far AI: 400 hops are written this level, and the rest aren't") == 0,
		std::string("the next says the rest aren't written: ") + out);
	strcpy(out, "unchanged");
	Check(!FarAiHops_Line(hop, &logged, out, sizeof(out)) && out[0] == 0, "and after that there's nothing");
	Check(!FarAiHops_Line(hop, &logged, out, sizeof(out)) && logged == kFarAiHopsALevel + 1, "and the count stays put");

	logged = 0;
	Check(FarAiHops_Line(hop, &logged, out, sizeof(out)) && strncmp(out, "far AI: alien went", 18) == 0, "a new level's count gives lines again");
}

static void TestTheGameUsesIt()
{
	printf("The game\n");
	std::string const far = ReadSourceFile("3dc/avp/bh_far.cpp");
	std::string const game = ReadSourceFile("3dc/avp/game.cpp");
	std::string const locate = Between(far, "void LocateFarNPCInAIModule(STRATEGYBLOCK *sbPtr, AIMODULE *targetModule)", "\n}\n");
	std::string const saying = Between(far, "static void SayWhereTheFarAIPutIt(", "\n}\n");

	Check(!far.empty() && !game.empty(), "SOURCE: the game's files can be read");
	Check(Has(locate, "if (goesIntoView)\n\t{\n\t\tnewPosition = targetEntryPoint->position;"), "SOURCE: a hop into view goes onto the entry point, as 1999 had it");
	Check(Has(locate, "newPosition = auxLocsList[targetLocInx];\n\t\t\tontoEntryPoint = false;"), "SOURCE: and a far location is said to be one");
	Check(locate.find("return;\n\t\t}\n\n\t\t// The hop is going ahead") != std::string::npos
		&& locate.find("SayWhereTheFarAIPutIt(sbPtr, targetModule, temp_Pos, ontoEntryPoint, goesIntoView);") < locate.find("dynPtr->Position = newPosition;"),
		"SOURCE: the line is written after the check that can call the hop off, and before the NPC is moved");
	Check(Has(saying, "(sbPtr->shapeIndex >= 0 && sbPtr->shapeIndex < maxshapes) ? mainshapelist[sbPtr->shapeIndex] : 0"), "SOURCE: the shape is looked up only by a number the list has");
	Check(Has(saying, "if (FarAiHops_Line(hop, &farAiHopsLogged, line, sizeof(line)))\n\t{\n\t\tLogString(line);"), "SOURCE: the header makes the line, and the count");
	Check(Has(Between(far, "void FarAiHops_LevelStarts(void)", "\n}\n"), "farAiHopsLogged = 0;"), "SOURCE: a level's start counts from 0 again");
	Check(Has(game, "\tBuildFarModuleLocs();\n\tFarAiHops_LevelStarts();\n"), "SOURCE: and game.cpp asks for it beside the far AI's own level setup");
}

int main()
{
	TestTheLine();
	TestTheLimit();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
