// Asks whether a whole word is on the command line. Written 2026-11-04 by Melanikus for the AvP port.
//
// The game's usual way of looking for a switch, strstr, also finds it inside a longer word. So asking for -dev that way would also say
// yes to -developer, or to a folder called c:\games-dev. That's harmless for most switches, but -dev turns on tools a tester must not
// reach by accident, so this one has to be the word itself. The command line comes from outside the game, so nothing is assumed about it.

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

#endif
