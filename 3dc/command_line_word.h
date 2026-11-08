// Reads the switches on the command line as whole words. Written 2026-11-04 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the word that follows a switch (for -n), and the -l switch, are read here too, as whole words (LEVEL-SWITCH-1)
//
// The game's usual way of looking for a switch, strstr, also finds it inside a longer word. So asking for -dev that way would also say
// yes to -developer, or to a folder called c:\games-dev. For a switch as short as -n or -w, any longer switch that started with those
// letters would be taken for it. So switches are read here as whole words.
//
// A word is a run of letters with a space, a tab, or the start or end of the line on each side. Quotation marks are letters like any
// other: the game has never read them, so a value with a space in it can't be given to a switch.
//
// The command line comes from outside the game (Tao 3), so nothing is assumed about it: it may be missing, empty, or as long as Windows
// lets it be.

#ifndef COMMAND_LINE_WORD_H
#define COMMAND_LINE_WORD_H

#include <string.h>

inline bool CommandLineHasWord(const char *commandLine, const char *word)
{
	if (!commandLine || !word || !*word)
	{
		return false;
	}

	size_t const length = strlen(word);

	for (const char *found = strstr(commandLine, word); found; found = strstr(found + 1, word))
	{
		// what's either side of it has to be a space, a tab, or the end of the line
		bool const startsClean = found == commandLine || found[-1] == ' ' || found[-1] == '\t';
		char const after = found[length];
		bool const endsClean = after == 0 || after == ' ' || after == '\t';

		if (startsClean && endsClean)
		{
			return true;
		}
	}

	return false;
}

// Whether c is one of the letters that come between words
inline bool CommandLineIsGap(char c)
{
	return c == ' ' || c == '\t';
}

// Finds the next word on the command line, looking from *at onwards. Gives back false when there are no more words. Otherwise *start is
// where the word starts, *length is how many letters it has, and *at is moved to just past it, so the next call finds the word after.
inline bool CommandLineNextWord(const char **at, const char **start, size_t *length)
{
	if (!at || !*at || !start || !length)
	{
		return false;
	}

	const char *place = *at;

	while (CommandLineIsGap(*place))
	{
		place++;
	}

	if (*place == 0)
	{
		return false;
	}

	*start = place;

	while (*place != 0 && !CommandLineIsGap(*place))
	{
		place++;
	}

	*length = (size_t)(place - *start);
	*at = place;
	return true;
}

// Whether the word at start, length letters long, is exactly word, capitals and all
inline bool CommandLineWordIs(const char *start, size_t length, const char *word)
{
	return start && word && strlen(word) == length && memcmp(start, word, length) == 0;
}

// What CommandLineWordAfter found
enum CommandLineWordAfterAnswer
{
	kWordAfterNoSwitch, // the switch isn't on the command line
	kWordAfterFound,    // the word that follows it is in out
	kWordAfterNothing,  // the switch is the line's last word, so nothing follows it
	kWordAfterTooLong   // the word that follows it doesn't fit in out, so out is left empty
};

// Finds theSwitch on the command line, as a whole word, and copies the word that follows it into out, which has room bytes, its end
// marker included. Only the first time the switch is there counts, as it did when the game used strstr. out is left empty unless the
// answer is kWordAfterFound, and it always ends within its room.
inline CommandLineWordAfterAnswer CommandLineWordAfter(const char *commandLine, const char *theSwitch, char *out, size_t room)
{
	if (out && room > 0)
	{
		out[0] = 0;
	}

	const char *at = commandLine;
	const char *start = 0;
	size_t length = 0;

	while (CommandLineNextWord(&at, &start, &length))
	{
		if (!CommandLineWordIs(start, length, theSwitch))
		{
			continue;
		}

		if (!CommandLineNextWord(&at, &start, &length))
		{
			return kWordAfterNothing;
		}

		// The + 1 is for the end marker. A word that doesn't fit is refused whole, never cut short, because a name cut short could be
		// a different name.
		if (!out || length + 1 > room)
		{
			return kWordAfterTooLong;
		}

		memcpy(out, start, length);
		out[length] = 0;
		return kWordAfterFound;
	}

	return kWordAfterNoSwitch;
}

// The -l switch asks for more kinds of creature to be loaded with every level, so that they can be made in it even when the level has
// none of its own. -l on its own asks for aliens. Letters straight after it name kinds instead, one letter each: -lmp asks for marines
// and predators. winmain.cpp says which letter is which kind.
//
// A word is read as -l only if every letter after the -l names a kind. So a longer switch that happens to start with -l, such as a
// -lights, isn't taken for it. (The game used to find -l anywhere and pass over letters that named nothing, so -lights would have asked
// for the three kinds whose letters are in it: h, t and s.)
static const char kCommandLineLoadLetters[] = "achmpqrstx";

// Whether the word at start, length letters long, is an -l
inline bool CommandLineIsLoadWord(const char *start, size_t length)
{
	if (!start || length < 2 || start[0] != '-' || start[1] != 'l')
	{
		return false;
	}

	for (size_t i = 2; i < length; i++)
	{
		// strchr counts the end marker as part of the letters it looks through, so an end marker has to be refused by itself
		if (start[i] == 0 || !strchr(kCommandLineLoadLetters, start[i]))
		{
			return false;
		}
	}

	return true;
}

// Whether any -l on the command line asks for the kind with this letter. An -l on its own asks for aliens: 'a'.
inline bool CommandLineLoadAsks(const char *commandLine, char letter)
{
	const char *at = commandLine;
	const char *start = 0;
	size_t length = 0;

	while (CommandLineNextWord(&at, &start, &length))
	{
		if (!CommandLineIsLoadWord(start, length))
		{
			continue;
		}

		// -l on its own asks for aliens, and -l with letters after it asks for the kinds those letters name
		bool const asks = (length == 2) ? (letter == 'a') : (memchr(start + 2, letter, length - 2) != 0);

		if (asks)
		{
			return true;
		}
	}

	return false;
}

// How many words start with -l but aren't an -l, because a letter after the -l names no kind. The game says so in its log, so that a
// mistyped -l isn't passed over without a word.
inline int CommandLineLoadWordsNotRead(const char *commandLine)
{
	const char *at = commandLine;
	const char *start = 0;
	size_t length = 0;
	int notRead = 0;

	while (CommandLineNextWord(&at, &start, &length))
	{
		if (length >= 2 && start[0] == '-' && start[1] == 'l' && !CommandLineIsLoadWord(start, length))
		{
			notRead++;
		}
	}

	return notRead;
}

#endif
