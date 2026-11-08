// The save test harness in the game (SAVE-HOSTILE-1, step 3). What it is, and where it's called from, is in save_test.h.
// Written 2026-11-08 by Melanikus for the AvP port.
#include "save_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <windows.h>

#include "command_line_word.h"
#include "logString.h"
#include "save_test_rules.h"
#include "utilities.h"

// The game's own, in the order savegame.cpp has them: savegame.h needs the behaviours' kinds and the strategy blocks' names
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "savegame.h"
#include "env_info.h"

extern bool bRunning;
extern BOOL GetFilenameForSaveSlot(int i, char *filenamePtr);

namespace
{
bool asked = false;              // -savetest or -savetest-make was on the command line
bool askLoad = false, askMake = false;
bool folderReady = false;        // the game's folder is SaveTest
bool running = false;
bool menusAnswered = false;
SaveTestAnswer answer = kSaveTestNeedsDev;
char name[kCustomMapNameMost + 1] = { 0 };
bool nameTooLong = false;
int species = -1, episode = -1;
char gameFolder[MAX_PATH] = { 0 };   // the folder the game copy runs from
char savePath[MAX_PATH] = { 0 };     // the save to load: gameFolder\SaveTest\<name>.sav
int framesInLevel = 0;

void End()
{
	running = false;
	AvP.MainLoopRunning = 0;
	bRunning = false;
}

// A whole number from text, all of it, from low to high; false if it isn't one
bool WholeNumber(const char *text, long low, long high, int *out)
{
	char *end = nullptr;
	long const value = strtol(text, &end, 10);

	if (end == text || *end != '\0' || value < low || value > high)
	{
		return false;
	}

	*out = static_cast<int>(value);
	return true;
}
}

bool SaveTest_TakeCommandLine(const char *commandLine)
{
	askLoad = commandLine && CommandLineHasWord(commandLine, "-savetest");
	askMake = commandLine && CommandLineHasWord(commandLine, "-savetest-make");
	asked = askLoad || askMake;

	if (!asked)
	{
		return false;
	}

	// First of all, before the log is cleared, the game's folder is pointed at SaveTest beside the game copy, so a save test never
	// touches the player's own log, settings, profiles or saves, even if it's refused below
	DWORD const length = GetModuleFileNameA(NULL, gameFolder, sizeof(gameFolder));

	if (length == 0 || length >= sizeof(gameFolder))
	{
		gameFolder[0] = 0;
	}
	else
	{
		char *slash = strrchr(gameFolder, '\\');

		if (slash)
		{
			*slash = 0;
		}
		else
		{
			gameFolder[0] = 0;
		}
	}

	folderReady = UseSaveTestFolder(gameFolder);

	if (askLoad)
	{
		CommandLineWordAfterAnswer const got = CommandLineWordAfter(commandLine, "-savetest", name, sizeof(name));
		nameTooLong = got == kWordAfterTooLong;
	}

	if (askMake)
	{
		// "<species>,<episode>", one word
		char word[16];

		if (CommandLineWordAfter(commandLine, "-savetest-make", word, sizeof(word)) == kWordAfterFound)
		{
			char *comma = strchr(word, ',');

			if (comma)
			{
				*comma = 0;

				if (!WholeNumber(word, 0, 99, &species) || !WholeNumber(comma + 1, 0, 99, &episode))
				{
					species = episode = -1;
				}
			}
		}
	}

	return true;
}

bool SaveTest_Start(bool dev, bool networkGame)
{
	if (!asked)
	{
		return true;
	}

	if (!folderReady)
	{
		char const *const problem = GetSaveFolderProblem();
		LogString(std::string("save test: its SaveTest folder can't be made, so it doesn't run: ") + (problem ? problem : ""));
		End();
		return false;
	}

	SaveTestAsk ask;
	memset(&ask, 0, sizeof(ask));
	ask.dev = dev;
	ask.networkGame = networkGame;
	ask.load = askLoad;
	ask.name = name;
	ask.nameRoom = sizeof(name);
	ask.make = askMake;
	ask.species = species;
	ask.episode = episode;
	ask.episodesOf[I_Marine] = MAX_NO_OF_MARINE_EPISODES;
	ask.episodesOf[I_Predator] = MAX_NO_OF_PREDATOR_EPISODES;
	ask.episodesOf[I_Alien] = MAX_NO_OF_ALIEN_EPISODES;
	char relative[MAX_PATH];
	answer = nameTooLong ? kSaveTestBadName : SaveTest_Decide(ask, relative, sizeof(relative));

	if (answer == kSaveTestLoad && _snprintf(savePath, sizeof(savePath) - 1, "%s\\%s", gameFolder, relative) < 0)
	{
		answer = kSaveTestPathTooLong;
	}

	if (answer != kSaveTestLoad && answer != kSaveTestMake)
	{
		LogString(SaveTest_Why(answer));
		End();
		return false;
	}

	running = true;
	LogString(answer == kSaveTestLoad ? std::string("save test: loading ") + savePath
		: "save test: making a save of species " + std::to_string(species) + ", episode " + std::to_string(episode));
	return true;
}

bool SaveTest_Running()
{
	return running;
}

SaveTestMenusAsk SaveTest_MenusAsk()
{
	if (!running || menusAnswered)
	{
		return kSaveTestMenusNothing;
	}

	return answer == kSaveTestLoad ? kSaveTestMenusLoad : kSaveTestMenusNewGame;
}

int SaveTest_Species()
{
	return species;
}

int SaveTest_Episode()
{
	return episode;
}

const char *SaveTest_SavePath()
{
	return savePath;
}

void SaveTest_MenusDid(bool done, const char *why)
{
	menusAnswered = true;

	if (!done)
	{
		LogString(why ? why : "save test: the menus couldn't do it");
		End();
	}
}

void SaveTest_GameFrame()
{
	// A load that's been asked for and not done yet: the frames count from when it's done
	if (!running || LoadGameRequest != SAVELOAD_REQUEST_NONE)
	{
		return;
	}

	framesInLevel++;

	if (answer == kSaveTestLoad)
	{
		if (framesInLevel >= kSaveTestFramesInLevel)
		{
			LogString("save test: the game ran " + std::to_string(framesInLevel) + " frames after loading " + name + ", and stopped there");
			End();
		}

		return;
	}

	// Making a save: ask for it, and on the next frame, once the game has saved, copy it out of the profile's slot as the seed
	if (framesInLevel == kSaveTestFramesInLevel)
	{
		SaveGameRequest = 0;
		return;
	}

	if (framesInLevel > kSaveTestFramesInLevel)
	{
		char slot[MAX_PATH], seed[MAX_PATH];
		bool copied = false;

		// The seed goes beside the saves the harness loads, in SaveTest itself
		if (GetFilenameForSaveSlot(0, slot) && _snprintf(seed, sizeof(seed) - 1, "%s\\SaveTest\\seed_%d_%d.sav", gameFolder, species, episode) >= 0)
		{
			seed[sizeof(seed) - 1] = 0;
			copied = CopyFileA(slot, seed, FALSE) != 0;
		}

		LogString(copied ? std::string("save test: saved as ") + seed : "save test: the game didn't save, or the save couldn't be copied");
		End();
	}
}
