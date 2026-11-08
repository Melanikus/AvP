// The rule for a custom map's name. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the custom maps follow
// Bobblen's avp-forever, rewritten).
//
// A custom map is a level file in avp_rifs/Custom, and its name is the file's name without ".rif". The name is used to build the
// path the game loads, it's shown in the menus, and in a co-op game the host sends it to the other players. So it's hostile input,
// and only a name that keeps to this rule is ever used:
// - it ends within the room it's given, so it can never run on into whatever follows it;
// - it's 1 to kCustomMapNameMost letters long, so it always fits the 40 bytes the game keeps for it (customLevelName);
// - it holds only letters, digits, spaces, '_', '-', '(' and ')'. So no '\', '/' or ':', which would reach outside the folder;
//   no '.', so never "..", and the ".rif" the game adds stays the last dot; and no control letters;
// - it doesn't start or end with a space, which Windows would quietly take off, so it could name a different file;
// - it isn't one of the names Windows keeps for devices (CON, PRN, AUX, NUL, COM1 to COM9, LPT1 to LPT9), which would open a device
//   instead of a file.
// A name that breaks the rule is left out, with a line in the log saying so.

#pragma once

#include <stddef.h>
#include <string.h>

static const size_t kCustomMapNameMost = 39;

static inline bool CustomMapName_LetterIsAllowed(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-' || c == '(' || c == ')';
}

static inline char CustomMapName_Upper(char c)
{
	return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

// Whether name, of length letters, is one of the names Windows keeps for a device
static inline bool CustomMapName_IsADevice(const char *name, size_t length)
{
	static const char *const devices[] = { "CON", "PRN", "AUX", "NUL" };

	for (size_t d = 0; d < sizeof(devices) / sizeof(devices[0]); d++)
	{
		if (length == 3 && CustomMapName_Upper(name[0]) == devices[d][0] && CustomMapName_Upper(name[1]) == devices[d][1] && CustomMapName_Upper(name[2]) == devices[d][2])
		{
			return true;
		}
	}

	// COM1 to COM9, LPT1 to LPT9
	if (length == 4 && name[3] >= '1' && name[3] <= '9')
	{
		char const a = CustomMapName_Upper(name[0]), b = CustomMapName_Upper(name[1]), c = CustomMapName_Upper(name[2]);

		return (a == 'C' && b == 'O' && c == 'M') || (a == 'L' && b == 'P' && c == 'T');
	}

	return false;
}

// Whether the name in name, which may be no longer than room bytes including its end marker, keeps to the rule above
// How many letters come before the end marker, looking no further than room bytes. room if there's no end marker in that room.
static inline size_t CustomMapName_Length(const char *text, size_t room)
{
	size_t length = 0;

	while (length < room && text[length] != 0)
	{
		length++;
	}

	return length;
}

static inline bool CustomMapName_IsSafe(const char *name, size_t room)
{
	if (!name)
	{
		return false;
	}

	size_t const length = CustomMapName_Length(name, room);

	if (length == room)
	{
		return false; // it doesn't end within its room
	}

	if (length == 0 || length > kCustomMapNameMost)
	{
		return false;
	}

	for (size_t i = 0; i < length; i++)
	{
		if (!CustomMapName_LetterIsAllowed(name[i]))
		{
			return false;
		}
	}

	if (name[0] == ' ' || name[length - 1] == ' ')
	{
		return false;
	}

	return !CustomMapName_IsADevice(name, length);
}

// Turns a file name found in avp_rifs/Custom ("Name.rif", any case) into the map's name, in out (outRoom bytes). Gives back false,
// with out left empty, if the file name doesn't end in ".rif" or the name before it breaks the rule.
static inline bool CustomMapName_FromFileName(const char *fileName, size_t fileNameRoom, char *out, size_t outRoom)
{
	if (!out || outRoom == 0)
	{
		return false;
	}

	out[0] = 0;

	if (!fileName)
	{
		return false;
	}

	size_t const length = CustomMapName_Length(fileName, fileNameRoom);

	if (length == fileNameRoom)
	{
		return false; // it doesn't end within its room
	}

	// The length is checked first, so the letters looked at below are all inside the name
	if (length <= 4 || fileName[length - 4] != '.' || CustomMapName_Upper(fileName[length - 3]) != 'R' || CustomMapName_Upper(fileName[length - 2]) != 'I' || CustomMapName_Upper(fileName[length - 1]) != 'F')
	{
		return false;
	}

	size_t const nameLength = length - 4;

	if (nameLength + 1 > outRoom)
	{
		return false;
	}

	memcpy(out, fileName, nameLength);
	out[nameLength] = 0;

	if (!CustomMapName_IsSafe(out, outRoom))
	{
		out[0] = 0;
		return false;
	}

	return true;
}
