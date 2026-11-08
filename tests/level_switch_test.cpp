// The test for 3dc/level_switch.h, which decides whether the level the -n switch names is loaded (LEVEL-SWITCH-1). Written 2026-11-08 by
// Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no graphics card and runs none
// of the game: the decisions are all in the header. The last rows read winmain.cpp, to check that the game asks them the way the header
// says, and that the short switches are read as whole words there.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "level_switch.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// What LevelSwitch_Read decides for this command line. name gets what it put out for the game to load.
static LevelSwitchAnswer Read(const char *line, bool networkGame, bool loadingASave, std::string *name)
{
	char out[kLevelSwitchMostLetters + 1];
	memset(out, 'z', sizeof(out)); // anything left in it that isn't an end marker shows up
	out[sizeof(out) - 1] = 0;

	const LevelSwitchAnswer answer = LevelSwitch_Read(line, networkGame, loadingASave, out, sizeof(out));
	*name = out;
	return answer;
}

// The same, for single player with no save being loaded
static LevelSwitchAnswer Read(const char *line, std::string *name)
{
	return Read(line, false, false, name);
}

static void NamesUsed()
{
	std::string name;

	Check(Read("-n derelict", &name) == kLevelSwitchUseIt && name == "derelict", "a stock level's name, in single player");
	Check(Read("-w -n fall_m -dev", &name) == kLevelSwitchUseIt && name == "fall_m", "with other switches either side");
	Check(Read("-n Custom/My-Map(2)", &name) == kLevelSwitchUseIt && name == "Custom/My-Map(2)", "a custom map, in its folder");
	Check(Read("-n cUSTOM/mymap", &name) == kLevelSwitchUseIt && name == "cUSTOM/mymap", "the folder in any capitals, kept as it was given");

	std::string const longest(kCustomMapNameMost, 'a');
	Check(Read(("-n " + longest).c_str(), &name) == kLevelSwitchUseIt && name == longest, "the longest name a map can have");
	Check(Read(("-n Custom/" + longest).c_str(), &name) == kLevelSwitchUseIt && name == "Custom/" + longest, "and that in the custom folder");
}

static void NotAsked()
{
	std::string name;

	Check(Read("", &name) == kLevelSwitchNotAsked && name.empty(), "an empty line, and out is left empty");
	Check(Read("-dev -w", &name) == kLevelSwitchNotAsked, "no -n");
	Check(Read("-noshadows derelict", &name) == kLevelSwitchNotAsked && name.empty(), "a longer switch starting -n isn't -n");
	Check(Read(0, &name) == kLevelSwitchNotAsked, "no line at all");
	Check(Read("-dev", true, true, &name) == kLevelSwitchNotAsked, "with no -n, a network game or a save has nothing to refuse");
}

static void Refused()
{
	std::string name;

	Check(Read("-n derelict", true, false, &name) == kLevelSwitchNetworkGame && name.empty(), "a network game loads the host's level, and out is left empty");
	Check(Read("-n ../x", true, false, &name) == kLevelSwitchNetworkGame, "in a network game that's the reason, whatever the name is");
	Check(Read("-n", true, false, &name) == kLevelSwitchNetworkGame, "even with no name");
	Check(Read("-n derelict", false, true, &name) == kLevelSwitchLoadingASave && name.empty(), "a save goes back onto its own level");
	Check(Read("-n derelict", true, true, &name) == kLevelSwitchNetworkGame, "a network game is said first");

	Check(Read("-n", &name) == kLevelSwitchNoName && name.empty(), "nothing after -n");
	Check(Read("-dev -n  \t", &name) == kLevelSwitchNoName, "only spaces after it");

	Check(Read("-n -w", &name) == kLevelSwitchAnotherSwitch && name.empty(), "another switch after -n isn't a level's name");
	Check(Read("-n -", &name) == kLevelSwitchAnotherSwitch, "nor is a lone -");

	std::string const tooLong(kLevelSwitchMostLetters + 1, 'a');
	Check(Read(("-n " + tooLong).c_str(), &name) == kLevelSwitchTooLong && name.empty(), "a name longer than any can be is refused whole");
	Check(Read(("-n Custom/" + std::string(kCustomMapNameMost + 1, 'a')).c_str(), &name) == kLevelSwitchTooLong, "the custom folder and a map's name one letter too long");

	char const *const broken[] = {
		"-n ../avp_rifs/x",          // out of the folder
		"-n ../../derelict",         // and with the folder's length of letters in front of a good name
		"-n ..\\x",                  // the other way of writing it
		"-n c:x",                    // a drive
		"-n derelict.rif",           // a dot
		"-n CON",                    // a device
		"-n com1",                   // a device, in small letters
		"-n Custom/../x",            // out of the folder after Custom/
		"-n Custom/",                // Custom/ and no name
		"-n Custom/Custom/x",        // a folder inside it
		"-n Custo/x",                // a folder that isn't Custom/
		"-n \"derelict\"",           // quotation marks, which are only letters to the game
		"-n dere\x01lict",           // a control letter
		"-n aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", // 40 letters: one more than a map's name can have, though -n has room for it
	};

	for (const char *line : broken)
	{
		Check(Read(line, &name) == kLevelSwitchBreaksTheRule && name.empty(), std::string("the rule for a map's name refuses: ") + (line + 3));
	}
}

static void TheRoomItsGiven()
{
	char out[32];
	memset(out, 'z', sizeof(out));

	Check(LevelSwitch_Read("-n derelict", false, false, out, 4) == kLevelSwitchTooLong, "a good name that doesn't fit the room it's given isn't put there");
	Check(out[0] == 0 && out[4] == 'z' && out[8] == 'z', "and nothing past that room is touched");
	Check(LevelSwitch_Read("-n derelict", false, false, out, 9) == kLevelSwitchUseIt && std::string(out) == "derelict", "a name that just fits is");
	Check(LevelSwitch_Read("-n derelict", false, false, 0, 9) == kLevelSwitchTooLong, "nowhere to put it");
}

static void WhyInWords()
{
	const LevelSwitchAnswer refusals[] = { kLevelSwitchNetworkGame, kLevelSwitchLoadingASave, kLevelSwitchNoName, kLevelSwitchAnotherSwitch, kLevelSwitchTooLong, kLevelSwitchBreaksTheRule };
	std::vector<std::string> said;

	for (LevelSwitchAnswer refusal : refusals)
	{
		std::string const words = LevelSwitch_Why(refusal);
		Check(!words.empty(), "refusal " + std::to_string((int)refusal) + " has words for the log");
		Check(std::find(said.begin(), said.end(), words) == said.end(), "refusal " + std::to_string((int)refusal) + "'s words are its own");
		said.push_back(words);
	}

	Check(std::string(LevelSwitch_Why(kLevelSwitchNetworkGame)).find("network game") != std::string::npos, "the network refusal says it's a network game");
	Check(std::string(LevelSwitch_Why(kLevelSwitchLoadingASave)).find("saved game") != std::string::npos, "the save refusal says a saved game is loading");
	Check(std::string(LevelSwitch_Why(kLevelSwitchNotAsked)).empty() && std::string(LevelSwitch_Why(kLevelSwitchUseIt)).empty(), "the two that aren't refusals have none");
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

	// A file has one kind of line ending or the other depending on how it was cloned, and these rows shouldn't care
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, std::string const &words)
{
	return text.find(words) != std::string::npos;
}

static int TimesIn(std::string const &text, std::string const &words)
{
	int times = 0;

	for (std::string::size_type place = text.find(words); place != std::string::npos; place = text.find(words, place + 1))
	{
		times++;
	}

	return times;
}

static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const one = text.find(first);
	std::string::size_type const two = text.find(second);

	return one != std::string::npos && two != std::string::npos && one < two;
}

// The text from one marker up to the next one after it. Empty if either isn't there, so a row looking inside it fails instead of passing
// on nothing.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static void SourceOfTheGame()
{
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	Check(!winmain.empty(), "SOURCE: winmain.cpp is read");

	// -n
	Check(Has(winmain, "bool const networkGame = AvP.Network != I_No_Network;"), "SOURCE: -n is told whether this is a network game");
	Check(Has(winmain, "bool const loadingASave = LoadGameRequest != SAVELOAD_REQUEST_NONE;"), "SOURCE: and whether a saved game is being loaded");
	Check(Has(winmain, "const LevelSwitchAnswer answer = LevelSwitch_Read(command_line, networkGame, loadingASave, named, sizeof(named));"), "SOURCE: and asked with those, into a name of its own");

	std::string const used = Between(winmain, "if (answer == kLevelSwitchUseIt)", "else if (answer != kLevelSwitchNotAsked)");
	Check(Has(used, "strcpy_s(LevelName, sizeof(LevelName), named);"), "SOURCE: the name goes into LevelName only when it's to be used");
	Check(TimesIn(winmain, "strcpy_s(LevelName") == 1 && !Has(winmain, "sscanf(instr, \"-n"), "SOURCE: and winmain.cpp puts no other name there");
	Check(Has(winmain, "LogString(std::string(\"level: -n is ignored: \") + LevelSwitch_Why(answer)"), "SOURCE: a refusal is said in the log, with its reason");
	Check(ComesBefore(winmain, "while (AvP_MainMenus() && bRunning)", "LevelSwitch_Read(") && ComesBefore(winmain, "LevelSwitch_Read(", "LoadRifFile();"),
		"SOURCE: it's asked for every level, after the menus have chosen and before the level is loaded");
	Check(!Has(winmain, "\"-n %"), "SOURCE: the old way of reading -n is gone");

	// The short switches are whole words, or gone
	char const *const shortSwitches[] = { "-l", "-m", "-n", "-w", "-s" };

	for (char const *name : shortSwitches)
	{
		Check(!Has(winmain, std::string("strstr(command_line, \"") + name + "\")"), std::string("SOURCE: ") + name + " isn't looked for inside other words");
	}

	Check(!Has(winmain, "strstr(strpos, \"-l\")") && !Has(winmain, "\"-s%d\"") && !Has(winmain, "level_to_load"), "SOURCE: the old -l and -s are gone");
	Check(Has(winmain, "if (CommandLineHasWord(command_line, \"-w\"))\n\t{\n\t\tWindowMode = WindowModeSubWindow;"), "SOURCE: -w is read as a word");

	// -l: every letter has its kind, and nothing else is in the table
	std::string const table = Between(winmain, "LoadLetter const loadLetters[] = {", "};");

	for (const char *letter = kCommandLineLoadLetters; *letter; letter++)
	{
		Check(Has(table, std::string("{ '") + *letter + "', &ForceLoad_"), std::string("SOURCE: -l's letter ") + *letter + " loads a kind");
	}

	Check(TimesIn(table, "&ForceLoad_") == (int)strlen(kCommandLineLoadLetters), "SOURCE: and the table has no letter -l doesn't read");
	Check(Has(winmain, "if (CommandLineLoadAsks(command_line, each.letter))\n\t\t\t{\n\t\t\t\t*each.load = TRUE;"), "SOURCE: each letter asked for loads its kind");
	Check(Has(winmain, "if (int const notRead = CommandLineLoadWordsNotRead(command_line))"), "SOURCE: a word that only looks like -l is said in the log");

	// No other switch the game reads would be taken for -l, since -l takes letters straight after it
	std::vector<std::string> names;

	for (std::string::size_type place = winmain.find("\"-"); place != std::string::npos; place = winmain.find("\"-", place + 1))
	{
		std::string::size_type const end = winmain.find('"', place + 1);

		if (end != std::string::npos)
		{
			names.push_back(winmain.substr(place + 1, end - place - 1));
		}
	}

	Check(names.size() > 30, "SOURCE: the switches' names are found (" + std::to_string(names.size()) + ")");

	for (std::string const &name : names)
	{
		Check(!CommandLineIsLoadWord(name.c_str(), name.size()), "SOURCE: " + name + " isn't read as -l");
	}
}

int main()
{
	NamesUsed();
	NotAsked();
	Refused();
	TheRoomItsGiven();
	WhyInWords();
	SourceOfTheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all -n checks passed\n");
	return 0;
}
