// The rules for the save test harness: when it may run, and which file it may read (SAVE-HOSTILE-1, step 2). Written 2026-11-08 by
// Melanikus for the AvP port.
//
// WHY. The harness (-dev -savetest <name>) loads a save that a tool has broken on purpose (tools/save_mutate), to see what the game's
// loader does with it. That's a thing no player should ever reach by accident, and the name it's given comes from the command line, so
// it's hostile input like any other (AvP Tao #3). So:
//   - it runs only with -dev, the developers' switch;
//   - never in a network game: a save is single player, and a test must not touch another machine;
//   - it reads only from its own folder, SaveTest, beside the game copy it runs from, never the player's saves, and only a file whose
//     name keeps to the custom map name rule (custom_map_name.h): no '\', '/', ':' or '.', so it can't reach outside the folder, and
//     no name Windows keeps for a device. The ".sav" is added here;
//   - and to make a save to break (-savetest-make <species> <episode>), the species and the episode have to be ones the game has.
// Each refusal has its own reason, and the harness says it in the log and stops.
//
// NOTHING HERE KNOWS ABOUT THE GAME. It's handed what the game knows (the switches, whether it's a network game, the counts of levels), so
// a test can try every case.
#ifndef SAVE_TEST_RULES_H
#define SAVE_TEST_RULES_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "custom_map_name.h"

static const char *const kSaveTestFolder = "SaveTest";

enum SaveTestAnswer
{
	kSaveTestLoad,             // load the save at the path given back
	kSaveTestMake,             // start the level given, play a while, and save into the folder
	kSaveTestNeedsDev,
	kSaveTestNetworkGame,
	kSaveTestNoName,
	kSaveTestBadName,
	kSaveTestBadSpecies,
	kSaveTestBadEpisode,
	kSaveTestBoth,             // both -savetest and -savetest-make
	kSaveTestPathTooLong
};

struct SaveTestAsk
{
	bool dev;                  // -dev is on
	bool networkGame;          // the game is, or is about to be, a network game
	bool load;                 // -savetest was given
	const char *name;          // the word after it, or nothing
	size_t nameRoom;           // how many bytes name may be read up to, its end marker included
	bool make;                 // -savetest-make was given
	int species;               // the numbers after it: which species (0 marine, 1 alien, 2 predator, as the game numbers them)
	int episode;               // and which of its levels, from 0
	int episodesOf[3];         // how many levels each species has: the game's own counts
};

// What the harness is to do, and for a load, the file it may read: "SaveTest\<name>.sav", into path (pathRoom bytes)
static inline SaveTestAnswer SaveTest_Decide(const SaveTestAsk &ask, char *path, size_t pathRoom)
{
	if (path && pathRoom)
	{
		path[0] = '\0';
	}

	if (!ask.dev)
	{
		return kSaveTestNeedsDev;
	}

	if (ask.networkGame)
	{
		return kSaveTestNetworkGame;
	}

	if (ask.load && ask.make)
	{
		return kSaveTestBoth;
	}

	if (ask.make)
	{
		if (ask.species < 0 || ask.species > 2)
		{
			return kSaveTestBadSpecies;
		}

		if (ask.episode < 0 || ask.episode >= ask.episodesOf[ask.species])
		{
			return kSaveTestBadEpisode;
		}

		return kSaveTestMake;
	}

	if (!ask.name || ask.nameRoom == 0 || ask.name[0] == '\0')
	{
		return kSaveTestNoName;
	}

	if (!CustomMapName_IsSafe(ask.name, ask.nameRoom))
	{
		return kSaveTestBadName;
	}

	if (!path || pathRoom == 0)
	{
		return kSaveTestPathTooLong;
	}

	// The name is at most kCustomMapNameMost letters, so "SaveTest\" + it + ".sav" is at most 53 bytes with its end
	int const wrote = snprintf(path, pathRoom, "%s\\%s.sav", kSaveTestFolder, ask.name);

	if (wrote < 0 || static_cast<size_t>(wrote) >= pathRoom)
	{
		path[0] = '\0';
		return kSaveTestPathTooLong;
	}

	return kSaveTestLoad;
}

// Why the harness won't run, in words, for the log; nothing for the two answers that do run
static inline const char *SaveTest_Why(SaveTestAnswer answer)
{
	switch (answer)
	{
	case kSaveTestNeedsDev: return "save test: -savetest and -savetest-make need -dev";
	case kSaveTestNetworkGame: return "save test: not in a network game";
	case kSaveTestNoName: return "save test: -savetest needs the name of a save in the SaveTest folder";
	case kSaveTestBadName: return "save test: that name isn't one this can read: letters, digits, spaces, _, -, ( and ), up to 39";
	case kSaveTestBadSpecies: return "save test: -savetest-make's species is 0 (marine), 1 (alien) or 2 (predator)";
	case kSaveTestBadEpisode: return "save test: -savetest-make's episode isn't one of that species' levels";
	case kSaveTestBoth: return "save test: -savetest or -savetest-make, not both";
	case kSaveTestPathTooLong: return "save test: the save's path doesn't fit";
	default: return nullptr;
	}
}

#endif
