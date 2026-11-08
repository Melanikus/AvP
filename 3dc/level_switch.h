// The -n switch: a level named on the command line, loaded instead of the one the menus chose. Written 2026-11-08 by Melanikus for the
// AvP port (LEVEL-SWITCH-1).
//
// The game's makers used "-n <level>" to go straight to a level while they worked on it, and players still use it to try maps. It is
// kept for single player, where it changes only this player's own game. It's ignored, with a line in the log saying why:
// - in a network game. Every machine there has to be on the level the host chose. The messages that come over the network name things
//   on the host's level, so a machine on another level would read them against the wrong map. The first beta tester had -n in their
//   launch options, chose Derelict on the co-op page, and was taken to Colony, with nothing to say so.
// - while a saved game is being loaded. A save goes back onto the level it was saved on, which the menus have already chosen.
//
// A save says only which level of a campaign it is, never which map it was made on (SAVE-LEVEL-1). The level -n names isn't the
// campaign's level, so while it's played the game doesn't save (a save made then would later go onto the campaign's map), and a load
// during it loads the save's own level first. savegame.cpp does both, from PlayingLevelFromSwitch below.
//
// The name comes from outside the game (Tao 3), and the game makes a file's path from it (avp_rifs/<name>.rif), so it has to keep to
// the rule for a custom map's name in 3dc/custom_map_name.h: no '/', '\', ':' or '.', which could reach outside the folder, no names
// Windows keeps for devices, and so on. It may start with "Custom/", in any capitals, to name a custom map in avp_rifs/Custom. That part
// is fixed, and what follows it keeps to the rule as well.

#pragma once

#include <stddef.h>
#include <string.h>

#include "command_line_word.h"
#include "custom_map_name.h"

// Whether the level being played is one -n named, in place of the level the menus chose. winmain.cpp sets it as each level starts.
extern bool PlayingLevelFromSwitch;

// What LevelSwitch_Read decided
enum LevelSwitchAnswer
{
	kLevelSwitchNotAsked,      // there's no -n on the command line
	kLevelSwitchUseIt,         // the level's name is in out, to be loaded

	// The rest are refusals, and the level the menus chose is loaded. LevelSwitch_Why puts each one in words for the log.
	kLevelSwitchNetworkGame,   // this is a network game
	kLevelSwitchLoadingASave,  // a saved game is being loaded
	kLevelSwitchNoName,        // nothing follows -n
	kLevelSwitchAnotherSwitch, // what follows -n starts with '-', so it's the next switch, not a name
	kLevelSwitchTooLong,       // what follows -n is longer than any name it could be
	kLevelSwitchBreaksTheRule  // the name breaks the rule for a map's name
};

// The folder a custom map is in, as a name starts with it, and the most letters a name after -n can have: that folder and a map's name
static const char kLevelSwitchCustomFolder[] = "Custom/";
static const size_t kLevelSwitchCustomFolderLetters = sizeof(kLevelSwitchCustomFolder) - 1;
static const size_t kLevelSwitchMostLetters = kLevelSwitchCustomFolderLetters + kCustomMapNameMost;

// Whether name starts with "Custom/", in any capitals. It stops at the first letter that's different, so a shorter name's end marker
// stops it, and nothing past the end is read.
static inline bool LevelSwitch_StartsWithCustomFolder(const char *name)
{
	for (size_t i = 0; i < kLevelSwitchCustomFolderLetters; i++)
	{
		if (CustomMapName_Upper(name[i]) != CustomMapName_Upper(kLevelSwitchCustomFolder[i]))
		{
			return false;
		}
	}

	return true;
}

// Reads -n from the command line. networkGame and loadingASave say what the game is about to load. out has room bytes, its end marker
// included. It gets the name only when the answer is kLevelSwitchUseIt and is left empty otherwise, so it must not be LevelName itself:
// on a refusal, the menus' choice in LevelName has to stay as it is.
static inline LevelSwitchAnswer LevelSwitch_Read(const char *commandLine, bool networkGame, bool loadingASave, char *out, size_t room)
{
	if (out && room > 0)
	{
		out[0] = 0;
	}

	char name[kLevelSwitchMostLetters + 1];
	const CommandLineWordAfterAnswer after = CommandLineWordAfter(commandLine, "-n", name, sizeof(name));

	if (after == kWordAfterNoSwitch)
	{
		return kLevelSwitchNotAsked;
	}

	// These two come first, so that in a network game, or while a save is loaded, -n is never used, whatever follows it
	if (networkGame)
	{
		return kLevelSwitchNetworkGame;
	}

	if (loadingASave)
	{
		return kLevelSwitchLoadingASave;
	}

	if (after == kWordAfterNothing)
	{
		return kLevelSwitchNoName;
	}

	if (after == kWordAfterTooLong)
	{
		return kLevelSwitchTooLong;
	}

	if (name[0] == '-')
	{
		return kLevelSwitchAnotherSwitch;
	}

	// The part the rule is asked about: what follows "Custom/", if the name starts with that
	const char *const mapName = LevelSwitch_StartsWithCustomFolder(name) ? name + kLevelSwitchCustomFolderLetters : name;

	if (!CustomMapName_IsSafe(mapName, sizeof(name) - (size_t)(mapName - name)))
	{
		return kLevelSwitchBreaksTheRule;
	}

	size_t const length = strlen(name);

	// Only a mistake of ours could stop it here: the game gives out room for the longest name
	if (!out || length + 1 > room)
	{
		return kLevelSwitchTooLong;
	}

	memcpy(out, name, length + 1);
	return kLevelSwitchUseIt;
}

// A refusal in words, for the log's line "level: -n is ignored: <these words>". Empty for the two answers that aren't refusals.
static inline const char *LevelSwitch_Why(LevelSwitchAnswer answer)
{
	switch (answer)
	{
		case kLevelSwitchNetworkGame: return "this is a network game, where every machine has to load the level the host chose";
		case kLevelSwitchLoadingASave: return "a saved game is being loaded, and it goes back onto the level it was saved on";
		case kLevelSwitchNoName: return "no level's name follows it";
		case kLevelSwitchAnotherSwitch: return "what follows it starts with '-', so it's another switch, not a level's name";
		case kLevelSwitchTooLong: return "what follows it is longer than any level's name can be";
		case kLevelSwitchBreaksTheRule: return "the name breaks the rule for a map's name (custom_map_name.h): up to 39 letters, digits, '_', '-', '(' and ')', after Custom/ if it starts with that, and not the name of a device";
		default: return "";
	}
}
