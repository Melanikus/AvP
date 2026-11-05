// Is a name from a level file one the game may open? Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY: a level file names its textures and sounds, and the game adds a folder to the front and opens the result. A level file can come from anyone, so a name like ..\..\..\Users\someone\secret.rim would open a
// file outside the game. So every name from a level passes through here on its way to being opened.
//
// THE RULE is a list of what a name may be, not of what it may not, so that a trick nobody thought of is refused anyway. A name is a path inside the folder the game put in front of it, made of plain parts:
//
//   letters, digits, and  _ - . + & @  and a space, in parts separated by / or \
//
// Those are the only characters in the 2,938 names in the game's own levels, measured through the real loader on 2026-11-05.
//
// & and @ look out of place in a list like this. They're here because the game's own textures use them ("Frame & Panels.RIM", "Pipe01-04@256.RIM"), and they mean nothing to Windows in a path. They only mean something
// to a command line, and nothing in the game hands a name to one.
//
// The list leaves out, among other things, the colon that a drive letter and a hidden file stream both need. On top of that, these are refused for what they mean to Windows:
//
// - Starting with a slash, which starts again from the top of the drive, whatever came in front.
// - Two dots together, which go up a folder.
// - A part ending in a dot or a space. Windows quietly takes those off, so the name it opens isn't the name that was checked: a part written as ". ." could turn into something else entirely.
// - An empty part, as two slashes together would make.
// - A part that's the name of a device: CON, PRN, AUX, NUL, COM1 to COM9 and LPT1 to LPT9, in capitals or not, with or without an ending. Windows opens the device, in any folder.
//
// A refused name counts as a missing file. The game already goes on without a missing texture or a missing sound.
//
// This file knows nothing about the game, so tests/level_file_name_test.cpp can run it on its own.

#ifndef LEVEL_FILE_NAME_H
#define LEVEL_FILE_NAME_H

#include <stddef.h>

// Longer than any name in the game's own levels, and short enough that with a folder in front it fits what Windows allows.
enum { kLevelFileNameMost = 200 };

// Why a name was refused.
static const char *const kLevelNameEmpty = "the name is empty";
static const char *const kLevelNameTooLong = "the name is too long";
static const char *const kLevelNameStartsAtTheTop = "the name starts with a slash";
static const char *const kLevelNameCharacter = "the name has a character no game file's name has";
static const char *const kLevelNameGoesUp = "the name goes up a folder";
static const char *const kLevelNameEmptyPart = "the name has an empty part";
static const char *const kLevelNamePartEnding = "a part of the name ends in a dot or a space";
static const char *const kLevelNameDevice = "a part of the name is the name of a device";

inline bool LevelFileName_IsSeparator(char c)
{
	return c == '/' || c == '\\';
}

inline bool LevelFileName_CharacterIsAllowed(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
		|| c == '_' || c == '-' || c == '.' || c == '+' || c == '&' || c == '@' || c == ' ' || LevelFileName_IsSeparator(c);
}

inline char LevelFileName_Capital(char c)
{
	return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

// Does this part start with a device's name, ending there or at a dot or a space? Windows reads "nul.rim" and "nul .rim" as the device too.
inline bool LevelFileName_PartIsDevice(const char *part, size_t length)
{
	size_t nameLength = 0;

	while (nameLength < length && part[nameLength] != '.' && part[nameLength] != ' ')
	{
		nameLength++;
	}

	if (nameLength == 3)
	{
		static const char *const kThreeLetters[] = { "CON", "PRN", "AUX", "NUL" };

		for (size_t i = 0; i < sizeof(kThreeLetters) / sizeof(kThreeLetters[0]); i++)
		{
			if (LevelFileName_Capital(part[0]) == kThreeLetters[i][0] && LevelFileName_Capital(part[1]) == kThreeLetters[i][1] && LevelFileName_Capital(part[2]) == kThreeLetters[i][2])
			{
				return true;
			}
		}
	}

	if (nameLength == 4 && part[3] >= '1' && part[3] <= '9')
	{
		char const a = LevelFileName_Capital(part[0]), b = LevelFileName_Capital(part[1]), c = LevelFileName_Capital(part[2]);

		return (a == 'C' && b == 'O' && c == 'M') || (a == 'L' && b == 'P' && c == 'T');
	}

	return false;
}

// A refused name, safe to put in the log: it's cut short, and anything it shouldn't have shows as a question mark. A name from a level can hold anything, including what would make a line of the log look like another.
inline void LevelFileName_ForTheLog(const char *name, char *line, size_t room)
{
	size_t used = 0;

	if (room == 0)
	{
		return;
	}

	for (; name && name[used] && used + 1 < room; used++)
	{
		line[used] = LevelFileName_CharacterIsAllowed(name[used]) ? name[used] : '?';
	}

	line[used] = 0;
}

// Gives back 0 if the name may be opened, or why it may not.
inline const char *LevelFileName_WhyRefused(const char *name)
{
	if (!name || !name[0])
	{
		return kLevelNameEmpty;
	}

	if (LevelFileName_IsSeparator(name[0]))
	{
		return kLevelNameStartsAtTheTop;
	}

	size_t length = 0;

	for (; name[length]; length++)
	{
		if (length >= kLevelFileNameMost)
		{
			return kLevelNameTooLong;
		}

		if (!LevelFileName_CharacterIsAllowed(name[length]))
		{
			return kLevelNameCharacter;
		}

		if (name[length] == '.' && name[length + 1] == '.')
		{
			return kLevelNameGoesUp;
		}
	}

	size_t partStart = 0;

	for (size_t i = 0; i <= length; i++)
	{
		if (i < length && !LevelFileName_IsSeparator(name[i]))
		{
			continue;
		}

		size_t const partLength = i - partStart;

		if (partLength == 0)
		{
			return kLevelNameEmptyPart;
		}

		char const last = name[i - 1];

		if (last == '.' || last == ' ')
		{
			return kLevelNamePartEnding;
		}

		if (LevelFileName_PartIsDevice(name + partStart, partLength))
		{
			return kLevelNameDevice;
		}

		partStart = i + 1;
	}

	return 0;
}

#endif
