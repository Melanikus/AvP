// The test for 3dc/avp/floor_is_lava.h, the FLOORISLAVA console command ("For Aliens, Floor is lava", #109). Written 2026-11-11 by
// Melanikus (Third) for the AvP port.
//
// WHAT IT TESTS
//
// The words after FLOORISLAVA come from whoever typed them, or from a file (AvP's third Tao: input is treated as hostile), so only the few
// it knows count, and anything else is refused and changes nothing. And what it says fits the command's space and says which it is.
// And from the game's own files, which can't be built on their own here: the console command is a player's, not a cheat, reads its words
// with the rule here and says what it did; what it asks is read into the alien ways' switch at each level's start and nowhere else. Since
// the flip it starts on, and only the console changes it, and a Hudson run's floorislava=off, which types the console's own OFF.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs /FC, so that its source
// files are found.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "floor_is_lava.h"

// floor_is_lava.h names the game's own switch; this test only reads the game's files, so it has one of its own to link
int FloorIsLavaAsked = 0;

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, found from where this test's own source is
static std::string ReadSourceFile(const char *name)
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

	// Line ends as \n alone, whatever the checkout made them
	std::string read = text.str();
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

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int Count(const std::string &text, const char *what)
{
	int count = 0;
	std::string const wanted = what;

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + wanted.size()))
	{
		count++;
	}

	return count;
}

// The text from the first from to the first to after it, or nothing if either isn't there
static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start + 1);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static void TestTheWords()
{
	printf("The console's words:\n");

	int on = 1;
	Check(FloorIsLava_FromWords("", &on) && on == 0, "nothing switches it off when it's on");
	Check(FloorIsLava_FromWords("  \t", &on) && on == 1, "only spaces switch it too, back on");
	Check(FloorIsLava_FromWords(0, &on) && on == 0, "no words at all switch it too");

	char const *const onWords[] = { "ON", "on", " On ", "1", "1\r\n" };

	for (char const *words : onWords)
	{
		on = 0;
		Check(FloorIsLava_FromWords(words, &on) && on == 1, std::string("\"") + words + "\" is on");
	}

	char const *const offWords[] = { "OFF", "off", "\tOfF", "0", "0 " };

	for (char const *words : offWords)
	{
		on = 1;
		Check(FloorIsLava_FromWords(words, &on) && on == 0, std::string("\"") + words + "\" is off");
	}

	char const *const refused[] = { "O", "OF", "ONN", "OFFF", "ON OFF", "2", "-1", "00", "01", "1x", "TRUE", "YES", "LAVA", "\x80", "O\xffN" };

	for (char const *words : refused)
	{
		on = 1;
		bool const tookItWhenOn = FloorIsLava_FromWords(words, &on);
		bool const stayedOn = on == 1;
		on = 0;
		bool const tookItWhenOff = FloorIsLava_FromWords(words, &on);
		bool const stayedOff = on == 0;

		Check(!tookItWhenOn && stayedOn && !tookItWhenOff && stayedOff, std::string("\"") + words + "\" is refused, and changes nothing either way");
	}

	std::string const said[] = { FloorIsLava_Words(1), FloorIsLava_Words(0), kFloorIsLavaRefused, kFloorIsLavaFromTheNextLevel, kFloorIsLavaOnlyTheHost };
	bool fits = true;

	for (std::string const &words : said)
	{
		fits = fits && words.size() < 100 && words.compare(0, 11, "FLOORISLAVA") == 0;
	}

	Check(fits, "every line it says starts with its name, and fits the command's 100-letter space");
	Check(Has(FloorIsLava_Words(1), " ON:") && Has(FloorIsLava_Words(0), " OFF:"), "the lines say which it is");
	Check(std::string(FloorIsLava_Words(1)) == "FLOORISLAVA ON: ALIENS CLIMB WALLS AND CEILINGS TO REACH YOU"
		&& std::string(FloorIsLava_Words(0)) == "FLOORISLAVA OFF: ALIENS MOVE AS IN 1999", "in PO's words, 2026-11-11");
}

static void TestTheGamesFiles()
{
	printf("The game's own files:\n");

	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const ways = ReadSourceFile("3dc/avp/alien_ways.cpp");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const hudson = ReadSourceFile("3dc/avp/hudson.cpp");

	Check(!commands.empty() && !ways.empty() && !winmain.empty() && !hudson.empty(), "the files are found");

	std::string const command = Between(commands, "static void FloorIsLavaCommand(char *words)", "\n}");
	Check(Has(command, "bool const took = FloorIsLava_FromWords(words, &FloorIsLavaAsked);"), "the command reads its words with the rule here");
	Check(Has(command, "NewOnScreenMessage(message);") && Has(command, "LogString(message);"), "the command says what it did, on the screen and in the log");
	Check(Has(command, "_snprintf(fromNext, sizeof(fromNext) - 1, \"%s\", kFloorIsLavaFromTheNextLevel);"), "and in a level, that it counts from the next level's start");
	Check(Has(command, "\t\tif (AvP.Network != I_No_Network && !AvP.NetworkAIServer)\n\t\t{\n\t\t\tstatic char hostOnly[100];")
		&& Has(command, "_snprintf(hostOnly, sizeof(hostOnly) - 1, \"%s\", kFloorIsLavaOnlyTheHost);") && Has(command, "\t\t\tNewOnScreenMessage(hostOnly);\n"),
		"and on a machine that joined a network game, that only the host's counts (the flip's F7)");

	std::string::size_type const made = commands.find("\"FLOORISLAVA\",");
	std::string::size_type const debugOnly = commands.find("bool IsACheat = true;");
	Check(made != std::string::npos && debugOnly != std::string::npos && made < debugOnly,
		"the console command is made with the players' settings, before the debugging commands");

	std::string const making = Between(commands, "\"FLOORISLAVA\",", ");");
	Check(Has(making, "FloorIsLavaCommand") && !Has(making, "IsACheat") && !Has(making, "true"), "the console command calls the game's command, and isn't a cheat");

	// What's asked counts at a level's start, before the level's map is asked for, and nothing else in the alien ways turns them on
	std::string const levelStart = Between(ways, "void AlienWays_LevelStart()\n{", "\n}");
	Check(Has(levelStart, "\tAlienWaysSwitchedOn = FloorIsLavaAsked ? 1 : 0;\n\tLogString(FloorIsLava_Words(AlienWaysSwitchedOn));"),
		"each level's start takes what was asked, and says so in the log");
	Check(levelStart.find("AlienWaysSwitchedOn = FloorIsLavaAsked ? 1 : 0;") < levelStart.find("if (!AlienWays_On())"),
		"before it asks whether the ways are on");
	Check(Count(ways, "AlienWaysSwitchedOn = ") == 2 && Has(ways, "\nint AlienWaysSwitchedOn = 0;\n"), "and nothing else in the alien ways sets the switch");

	// THE FLIP (2026-11-11, #109): it starts on, for every player, and the aliens search the grid, as the owner has always played them.
	// Nothing on the command line changes either. A Hudson run's floorislava=off is the one other way in, and it types the console's own OFF.
	Check(Has(ways, "\nint FloorIsLavaAsked = 1;\n"), "what's asked starts on: the alien ways are on for every player");
	Check(Has(ways, "\nint AlienWaysGridSwitchedOn = 1;\n") && Count(winmain, "AlienWaysGridSwitchedOn") == 0, "and the aliens search the grid, with nothing to turn it off");
	Check(Count(winmain, "FloorIsLavaAsked") == 0 && Count(commands, "FloorIsLavaAsked = ") == 0 && !Has(winmain, "devSwitch(\"-alienways\")")
		&& !Has(winmain, "devSwitch(\"-alienwaysgrid\")"), "nothing on the command line sets either, and the old developer's switches are gone");
	Check(Has(hudson, "\tif (plan.floorIsLavaOff)\n\t{\n\t\tFloorIsLava_FromWords(\"OFF\", &FloorIsLavaAsked);\n\t}\n") && Count(hudson, "FloorIsLavaAsked") == 1,
		"a Hudson run's floorislava=off types the console's own OFF, through its own rule, and Hudson touches it nowhere else");
}

int main()
{
	TestTheWords();
	TestTheGamesFiles();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
