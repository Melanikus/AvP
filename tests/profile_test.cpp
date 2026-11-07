// The test of what a player's profile file is allowed to say. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A profile is read from a file straight into memory, so every number in it can be anything. One of them, the difficulty a level was finished on, becomes the number of
// rows in a menu with four entries. A file saying 200 used to make the menu walk far past the end of its list. This gives the checks every value there is. Then it reads the
// game's own source as text, because the menus and the loader bring most of the game with them and can't be linked in: it checks that they really use these checks.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own, and the program gives back a non-zero result if anything failed. It reads the source files beside it, and nothing else.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <string.h>

#include "profile_checks.h"

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

// The text of one function of a source file, found by what it starts with
static std::string FunctionStartingWith(std::string const &source, char const *start)
{
	std::string::size_type const at = source.rfind(start); // the last one, because a declaration can come ahead of it

	if (at == std::string::npos)
	{
		return "";
	}

	std::string::size_type const open = source.find('{', at);
	int depth = 0;

	for (std::string::size_type i = open; i != std::string::npos && i < source.size(); i++)
	{
		depth += (source[i] == '{') ? 1 : (source[i] == '}') ? -1 : 0;

		if (depth == 0)
		{
			return source.substr(at, i - at);
		}
	}

	return "";
}

// The way the sliders searched before: round and round until it found one, for as long as that took. Only run here where there is one to find.
static void OldSearch(int *value, int howMany, int searchForward, bool const *allowed)
{
	while (!allowed[*value])
	{
		if (searchForward)
		{
			(*value)++;

			if (*value >= howMany)
			{
				*value = 0;
			}
		}
		else
		{
			if (*value == 0)
			{
				*value = howMany;
			}

			(*value)--;
		}
	}
}

static int CountOf(std::string const &text, std::string const &what)
{
	int count = 0;

	for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static void CheckCheatModeSearch()
{
	printf("The cheat-mode menu's search:\n");

	bool sameAsBefore = true;
	bool landedOnOne = true;
	int compared = 0;

	// Every pattern of allowed and not, on a list the size of the species list and on one of 8, from every start, both ways
	for (int howMany = 3; howMany <= 8; howMany += 5)
	{
		for (int pattern = 1; pattern < (1 << howMany); pattern++)
		{
			bool allowed[8];

			for (int i = 0; i < howMany; i++)
			{
				allowed[i] = (pattern >> i) & 1;
			}

			for (int start = 0; start < howMany; start++)
			{
				for (int forward = 0; forward < 2; forward++)
				{
					int before = start;
					OldSearch(&before, howMany, forward, allowed);

					int now = start;
					ProfileChecks_StepToAllowed(&now, howMany, forward, [&allowed](int value) { return allowed[value]; });

					sameAsBefore = sameAsBefore && (before == now);
					landedOnOne = landedOnOne && allowed[now];
					compared++;
				}
			}
		}
	}

	Check(compared > 2000, "it was tried on " + std::to_string(compared) + " patterns and starts");
	Check(sameAsBefore, "wherever there's something to find, it finds what the old search found");
	Check(landedOnOne, "and that's always an allowed one");

	// Nothing allowed is what a crafted profile gives: a cheat mode on and no level completed means no species has anywhere to play. The two ends of the list are where going round can slip.
	int const starts[4] = { 0, 1, 30, 31 };

	for (int forward = 0; forward < 2; forward++)
	{
		for (int s = 0; s < 4; s++)
		{
			int value = starts[s];
			int asked = 0;
			ProfileChecks_StepToAllowed(&value, 32, forward, [&asked](int) { asked++; return false; });
			Check(value == starts[s] && asked == 32, "nothing allowed: it goes once round, and stays where it was, starting at " + std::to_string(starts[s]) + (forward ? " going forward" : " going back"));
		}
	}

	// The check can look at the slider as it goes, so the number has to be up to date when it's asked
	int moving = 0;
	bool upToDate = true;
	ProfileChecks_StepToAllowed(&moving, 3, 1, [&moving, &upToDate](int value) { upToDate = upToDate && (value == moving); return value == 2; });
	Check(upToDate && moving == 2, "and the slider's number is up to date whenever it's asked about");
}

// The date text, with the real Windows calls, because how they fail is the question. Nothing here depends on this machine's settings: the formats that matter are given, and with the player's own, only the year is looked for.
struct TimeTextBox
{
	char before[32];
	char box[100];
	char after[64];
};

static bool Untouched(const char *place, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		if (place[i] != 'x')
		{
			return false;
		}
	}

	return true;
}

static void StartBox(TimeTextBox &box)
{
	memset(&box, 'x', sizeof(box));
}

static bool StayedInside(TimeTextBox const &box)
{
	return Untouched(box.before, sizeof(box.before)) && Untouched(box.after, sizeof(box.after)) && memchr(box.box, 0, sizeof(box.box)) != 0;
}

static void CheckTimeText()
{
	SYSTEMTIME good = {};
	good.wYear = 2026;
	good.wMonth = 11;
	good.wDay = 6;
	good.wDayOfWeek = 5;
	good.wHour = 12;
	good.wMinute = 34;
	good.wSecond = 56;

	TimeTextBox text;
	std::string const oneHundred(100, 'a');

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, NULL, NULL, text.box, sizeof(text.box));
	Check(StayedInside(text) && std::string(text.box).find("2026") != std::string::npos, std::string("a real time, with the player's own settings, gives text that says the year. It said: ") + text.box);
	Check(std::string(text.box).find("  ") != std::string::npos, "with the two parts apart");

	std::string const shortDate = "'DATE'";
	std::string const shortTime = "'TIME'";

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, shortDate.c_str(), shortTime.c_str(), text.box, sizeof(text.box));
	Check(std::string(text.box) == "TIME  DATE", std::string("it's the time, two spaces and the date. It said: ") + text.box);

	// What a damaged file can hold. Each of these has to be something Windows really refuses, or this isn't trying what it says.
	char windowsSaid[100];
	SYSTEMTIME badTime = good;
	badTime.wHour = 99;
	Check(GetTimeFormatA(GetThreadLocale(), 0, &badTime, NULL, windowsSaid, sizeof(windowsSaid)) == 0, "Windows really refuses the 99th hour");

	SYSTEMTIME badDate = good;
	badDate.wMonth = 13;
	Check(GetDateFormatA(GetThreadLocale(), DATE_LONGDATE, &badDate, NULL, windowsSaid, sizeof(windowsSaid)) == 0, "and the 13th month");

	StartBox(text);
	ProfileChecks_MakeTimeText(badTime, DATE_LONGDATE, shortDate.c_str(), shortTime.c_str(), text.box, sizeof(text.box));
	Check(std::string(text.box) == "  DATE", std::string("a time Windows refuses leaves that part empty, and doesn't use what was in the box. It said: ") + text.box);
	Check(StayedInside(text), "and stays in the box");

	StartBox(text);
	ProfileChecks_MakeTimeText(badDate, DATE_LONGDATE, shortDate.c_str(), shortTime.c_str(), text.box, sizeof(text.box));
	Check(std::string(text.box) == "TIME  ", std::string("so does a date. It said: ") + text.box);
	Check(StayedInside(text), "and stays in the box");

	SYSTEMTIME nonsense;
	memset(&nonsense, 0xFF, sizeof(nonsense));

	StartBox(text);
	ProfileChecks_MakeTimeText(nonsense, DATE_LONGDATE, NULL, NULL, text.box, sizeof(text.box));
	Check(std::string(text.box) == "  ", std::string("every number at its largest leaves both empty, with the player's own settings. It said: ") + text.box);
	Check(StayedInside(text), "and stays in the box");

	// As long as Windows will write each part, which is what long settings do: the old code added these together in a box of 100
	std::string const longTime = "'" + std::string(98, 'T') + "'";
	std::string const longDate = "'" + std::string(79, 'D') + "'";
	Check(GetTimeFormatA(GetThreadLocale(), 0, &good, longTime.c_str(), windowsSaid, 99) == 99, "Windows really writes a time 98 characters long");
	Check(GetDateFormatA(GetThreadLocale(), 0, &good, longDate.c_str(), windowsSaid, 80) == 80, "and a date 79 long");

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, longDate.c_str(), longTime.c_str(), text.box, sizeof(text.box));
	Check(StayedInside(text), "both at their longest stay in the box");
	Check(strlen(text.box) == 99, "and fill it, with room for the end");
	Check(std::string(text.box, 98) == std::string(98, 'T'), "with the time first");

	// One longer than each part's space. Windows refuses these, and whatever it may have put there is forgotten.
	std::string const tooLongTime = "'" + std::string(99, 'T') + "'";
	std::string const tooLongDate = "'" + std::string(80, 'D') + "'";

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, shortDate.c_str(), tooLongTime.c_str(), text.box, sizeof(text.box));
	Check(std::string(text.box) == "  DATE", std::string("a time too long for its space is left out. It said: ") + text.box);

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, tooLongDate.c_str(), shortTime.c_str(), text.box, sizeof(text.box));
	Check(std::string(text.box) == "TIME  ", std::string("and so is a date. It said: ") + text.box);

	// A box smaller than the words
	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, shortDate.c_str(), shortTime.c_str(), text.box, 3);
	Check(std::string(text.box) == "TI", "a small box gets what fits");
	Check(Untouched(text.box + 3, sizeof(text.box) - 3), "and nothing past it");

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, shortDate.c_str(), shortTime.c_str(), text.box, 1);
	Check(text.box[0] == 0 && Untouched(text.box + 1, sizeof(text.box) - 1), "a box of one gets just the end");

	StartBox(text);
	ProfileChecks_MakeTimeText(good, DATE_LONGDATE, shortDate.c_str(), shortTime.c_str(), text.box, 0);
	Check(Untouched(text.box, sizeof(text.box)), "and a box of nothing gets nothing");
}

int main()
{
	CheckTimeText();

	CheckCheatModeSearch();

	printf("The number of difficulties a level can offer:\n");

	Check(ProfileChecks_DifficultiesAllowed(1) == 1 && ProfileChecks_DifficultiesAllowed(2) == 2 && ProfileChecks_DifficultiesAllowed(3) == 3, "easy, medium and hard stay as they are, which is all the game ever saves");

	bool allInside = true;

	for (int value = 0; value < 256; value++)
	{
		int const rows = ProfileChecks_DifficultiesAllowed((char)value);
		allInside = allInside && rows >= 1 && rows <= 3;
	}

	Check(allInside, "every value a file can hold gives between one and three rows, so there's always a row to highlight");
	Check(ProfileChecks_DifficultiesAllowed((char)4) == 3 && ProfileChecks_DifficultiesAllowed((char)200) == 3 && ProfileChecks_DifficultiesAllowed((char)255) == 3, "too large gives three");
	Check(ProfileChecks_DifficultiesAllowed((char)0) == 1, "and none gives one");

	printf("A profile's levels as they're read in:\n");

	{
		char levels[3][16];
		char before[3][16];

		for (int species = 0; species < 3; species++)
		{
			for (int level = 0; level < 16; level++)
			{
				levels[species][level] = (char)((species + level) % 4);
			}
		}

		memcpy(before, levels, sizeof(levels));
		ProfileChecks_LimitLevels(levels);
		Check(memcmp(levels, before, sizeof(levels)) == 0, "a profile the game saved isn't changed at all");
	}

	{
		char levels[3][16];
		memset(levels, 0xFF, sizeof(levels));
		ProfileChecks_LimitLevels(levels);
		bool allHard = true;

		for (int species = 0; species < 3; species++)
		{
			for (int level = 0; level < 16; level++)
			{
				allHard = allHard && (unsigned char)levels[species][level] == kProfileHardest;
			}
		}

		Check(allHard, "every value too large is brought down, in all three species and all sixteen levels");
	}

	{
		char levels[3][16];
		memset(levels, 4, sizeof(levels));
		ProfileChecks_LimitLevels(levels);
		Check((unsigned char)levels[0][0] == kProfileHardest && (unsigned char)levels[2][15] == kProfileHardest, "from the first to the last, starting just past hard");
	}

	printf("Reading the real code:\n");

	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const allowed = FunctionStartingWith(menus, "int MaxDifficultyLevelAllowed(");
	Check(allowed.find("ProfileChecks_DifficultiesAllowed(UserProfilePtr->LevelCompleted[playerID][level-1])") != std::string::npos, "the menu's number of difficulties goes through the check");

	std::string const profiles = ReadSourceFile("3dc/avp/win95/frontend/user_profile.cpp");
	std::string::size_type const read = profiles.find("ReadFile(rif_file, profilePtr, sizeof(AVP_USER_PROFILE)");
	std::string::size_type const limit = profiles.find("ProfileChecks_LimitLevels(profilePtr->LevelCompleted)", read == std::string::npos ? 0 : read);
	std::string::size_type const added = profiles.find("InsertProfileIntoList(profilePtr)", read == std::string::npos ? 0 : read);
	Check(read != std::string::npos && limit != std::string::npos && limit < added, "and a profile's levels are limited as soon as they're read, before anything can use them");
	Check(profiles.find("MultiplayerCallsign[sizeof(profilePtr->MultiplayerCallsign) - 1] = 0") != std::string::npos, "and its multiplayer name has an end");

	std::string const cheats = ReadSourceFile("3dc/avp/cheatmodes.cpp");
	Check(cheats.find("ProfileChecks_StepToAllowed(speciesPtr, NUMBER_OF_SPECIES, searchForward,") != std::string::npos, "and the species slider uses the search that ends, over all its choices");
	Check(cheats.find("ProfileChecks_StepToAllowed(environmentPtr, NUMBER_OF_ENVIRONMENTS, searchForward,") != std::string::npos, "and so does the environment slider");
	Check(cheats.find("ProfileChecks_StepToAllowed(cheatModePtr, MAX_NUMBER_OF_CHEATMODES, searchForward,") != std::string::npos, "and the cheat-mode slider");

	// With nothing allowed the sliders walk, so Start has to say no. What it asks is the same as what the sliders stop on.
	Check(menus.find("if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT && CheatMode_SelectionIsAllowed())") != std::string::npos, "and Start only starts what the sliders are allowed to say");

	std::string const selection = FunctionStartingWith(cheats, "extern int CheatMode_SelectionIsAllowed(");
	Check(selection.find("UserProfilePtr->CheatMode[CheatMode_Active]") != std::string::npos, "which needs the cheat to be on");
	Check(selection.find("PlayableForSpecies[CheatMode_Species]") != std::string::npos, "and for it to work for that species");
	Check(selection.find("PlayableForEnvironment[CheatMode_Environment]") != std::string::npos, "and in that place");
	Check(selection.find("EnvironmentPlayableBySpecies(CheatMode_Environment)") != std::string::npos, "and that place to be one that species has finished");
	Check(selection.find("CheatMode_Species >= NUMBER_OF_SPECIES") != std::string::npos && selection.find("CheatMode_Environment >= NUMBER_OF_ENVIRONMENTS") != std::string::npos && selection.find("CheatMode_Active >= MAX_NUMBER_OF_CHEATMODES") != std::string::npos, "and it won't look anything up with a number off the end");
	Check(cheats.find("while(!") == std::string::npos && cheats.find("while (!") == std::string::npos, "and none has a search of its own left");

	Check(menus.find("ProfileChecks_MakeTimeText(when, dateKind, NULL, NULL, text, size);") != std::string::npos, "the game's date text goes by the player's own settings, through what's tried above");
	Check(menus.find("GetTimeFormat(") == std::string::npos && menus.find("GetDateFormat(") == std::string::npos, "and nothing in the menus writes one any other way");

	Check(menus.find("Brightness[i]") == std::string::npos, "a menu's fading rows all go through the check");
	Check(menus.find("return Brightness[row & 15];") != std::string::npos, "and that keeps any row, however many profiles there are, on the list of 16");
	std::size_t const timeTexts = menus.find("MakeTimeText(");
	Check(timeTexts != std::string::npos && menus.find("strcat(buffer2, buffer)") == std::string::npos && menus.find("strcat(buffer2,buffer)") == std::string::npos, "and the date text is built with a limit, not by adding to a box");

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
