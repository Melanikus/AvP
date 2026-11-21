// The test for 3dc/avp/stock_flags.h and stock_flags.cpp, THE OWNER'S STOCK SWITCH (2026-11-20, through Bishop: "we need any new behavior we added
// behind a flag in AVPX or these tests mean nothing"). Written 2026-11-20 by Melanikus (Vasquez) for the AvP port.
//
// WHAT IT TESTS
//
// - The five flags' words, as every Hudson run writes them to the log before any level: each flag's own word, "(stock)" only when all five
//   are off, "(not stock)" when any one is on.
// - The game starts with every flag on (the port's ways), and the stock switch turns all five off: the two with a console command
//   (FLOORISLAVA, NEWDYNAMICCOLLISIONS) through their own command's rule, as a player typing OFF would, and asked twice it stays off.
// - And from the game's own files, which can't be built on their own here: each flag guards the change it names, and nothing else
//   decides that change (pfarlocs.cpp's two 4th-corner heights, pheromone.cpp's door test, the two switches' near wake); a Hudson run's
//   stock=yes turns them all off before any level, and every run writes their words to the log.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs /FC, so that its source
// files are found. tests/takeouts/stock_flags_test.json breaks each piece in turn, and the test has to fail every time.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "floor_is_lava.h"
#include "new_dynamic_collisions.h"
#include "stock_flags.h"

// The two console commands' switches live in the game's alien_ways.cpp and dynamics.cpp, which this test doesn't build: it has its own,
// on, as the game starts (alien_ways.cpp: FloorIsLavaAsked = 1; dynamics.cpp: NewDynamicCollisionsOn = 1)
int FloorIsLavaAsked = 1;
int NewDynamicCollisionsOn = 1;

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, found from where this test's own source is, with its line ends as \n alone (a name of its own: Snyk follows a
// same-named helper into another test file)
static std::string StockFlagsGameFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream text;

	text << file.rdbuf();

	std::string const read = text.str();
	std::string out;
	out.reserve(read.size());

	for (char c : read)
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static int TimesIn(const std::string &text, const char *what)
{
	int count = 0;
	std::string const wanted = what;

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + wanted.size()))
	{
		count++;
	}

	return count;
}

static void TestTheWords()
{
	puts("the words");

	Check(StockFlags_WordsOf(1, 1, 1, 1, 1) == "flags: floorislava=on newdynamiccollisions=on farlocs4thcorner=on pheromonedoor=on nearwake=on (not stock)",
		"all five on: each one's word, not stock");
	Check(StockFlags_WordsOf(0, 0, 0, 0, 0) == "flags: floorislava=off newdynamiccollisions=off farlocs4thcorner=off pheromonedoor=off nearwake=off (stock)",
		"all five off: stock");

	// any single one on is not stock, and its own word says which
	char const *const names[5] = { "floorislava=on", "newdynamiccollisions=on", "farlocs4thcorner=on", "pheromonedoor=on", "nearwake=on" };

	for (int one = 0; one < 5; one++)
	{
		std::string const words = StockFlags_WordsOf(one == 0, one == 1, one == 2, one == 3, one == 4);
		Check(words.find(names[one]) != std::string::npos && words.find(" (not stock)") != std::string::npos && TimesIn(words, "=on") == 1,
			std::string("only ") + names[one] + ": not stock, and only its word says on");
	}

	Check(StockFlags_WordsOf(7, 0, 0, 0, 0) == StockFlags_WordsOf(1, 0, 0, 0, 0), "any number but 0 is on");
}

static void TestTheSwitch()
{
	puts("the switch");

	Check(FarLocsFourthCornerOn == 1 && PheromoneDoorOn == 1 && NearWakeOn == 1, "the game starts with the three on: the port's ways");
	Check(StockFlags_Words() == StockFlags_WordsOf(1, 1, 1, 1, 1), "and its words say all five on, as the game starts");

	StockFlags_AllStock();
	Check(FloorIsLavaAsked == 0 && NewDynamicCollisionsOn == 0 && FarLocsFourthCornerOn == 0 && PheromoneDoorOn == 0 && NearWakeOn == 0,
		"the stock switch turns all five off");
	Check(StockFlags_Words() == StockFlags_WordsOf(0, 0, 0, 0, 0), "and the words then say stock");

	// the console's own rule: OFF is off, not a switch over, so the stock switch asked twice leaves them off
	StockFlags_AllStock();
	Check(FloorIsLavaAsked == 0 && NewDynamicCollisionsOn == 0 && StockFlags_Words().find(" (stock)") != std::string::npos, "asked twice, it stays stock");

	// and a player's ON afterwards still works through the same rule
	Check(FloorIsLava_FromWords("ON", &FloorIsLavaAsked) && FloorIsLavaAsked == 1 && StockFlags_Words().find("floorislava=on") != std::string::npos,
		"a player's FLOORISLAVA ON after it is on again, and the words say so");
	FloorIsLavaAsked = 1;
	NewDynamicCollisionsOn = 1;
	FarLocsFourthCornerOn = 1, PheromoneDoorOn = 1, NearWakeOn = 1;
}

static void TestTheGamesFiles()
{
	puts("the game's files");

	std::string const flags = StockFlagsGameFile("3dc/avp/stock_flags.cpp");
	Check(TimesIn(flags, "FloorIsLava_FromWords(\"OFF\", &FloorIsLavaAsked);") == 1 && TimesIn(flags, "NewDynamicCollisions_FromWords(\"OFF\", &NewDynamicCollisionsOn);") == 1,
		"the switch types the two console commands' own OFF, each once");

	std::string const farlocs = StockFlagsGameFile("3dc/avp/pfarlocs.cpp");
	Check(TimesIn(farlocs, "if(FarLocsFourthCornerOn && polygonData.NumberOfVertices == 4)") == 2 && TimesIn(farlocs, "polygonData.NumberOfVertices == 4") == 2,
		"FARLOCS4THCORNER guards both 4th-corner heights in pfarlocs.cpp, and nothing else asks for a 4th corner there");

	std::string const pheromone = StockFlagsGameFile("3dc/avp/pheromone.cpp");
	Check(TimesIn(pheromone, "if (!PheromoneDoorOn)") == 1 && TimesIn(pheromone, "PheromoneDoorOn") == 1,
		"PHEROMONEDOOR chooses 1999's door test in pheromone.cpp, once");

	char const *const switches[2] = { "3dc/avp/bh_binary_switch.cpp", "3dc/avp/bh_link_switch.cpp" };

	for (char const *const name : switches)
	{
		std::string const text = StockFlagsGameFile(name);
		Check(TimesIn(text, "if (NearWakeOn && NearWake_ThisMachineRunsNpcs())") == 1 && TimesIn(text, "NearWake_ThisMachineRunsNpcs()") == 1,
			std::string("NEARWAKE guards the near wake in ") + name + ", and nothing else wakes a sleeper there");
	}

	std::string const hudson = StockFlagsGameFile("3dc/avp/hudson.cpp");
	// Since the debug views (6b57cf9f) the run sets them between the two, and its log line gives their words beside the flags' (Frost 2026-11-21, merging this test
	// into frost-saymarks for PR #275): the switch is still turned before the line, and the line is still written by every run
	size_t const stockAt = hudson.find("\tif (plan.stock)\n\t{\n\t\tStockFlags_AllStock();\n\t}\n");
	size_t const wordsAt = hudson.find("\tLogString(std::string(\"hudson: \") + StockFlags_Words() + \"; \" + DebugView_Words());");
	Check(TimesIn(hudson, "\tif (plan.stock)\n\t{\n\t\tStockFlags_AllStock();\n\t}\n") == 1 && TimesIn(hudson, "StockFlags_Words()") == 1 && stockAt != std::string::npos &&
		wordsAt != std::string::npos && stockAt < wordsAt,
		"a Hudson run's stock=yes turns all five off before any level, and every run then writes their words to the log");
}

int main()
{
	TestTheWords();
	TestTheSwitch();
	TestTheGamesFiles();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
