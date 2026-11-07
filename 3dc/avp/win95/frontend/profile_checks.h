// What a player's profile file is allowed to say. Written 2026-11-04 by Melanikus for the AvP port.
//
// A profile is a file in the player's own folder, read straight into memory, so any number in it can be anything. Most of them can't do harm, because whatever they do goes
// through a check already. These can. It's on its own here, away from the game, so that a test can check it.

#ifndef PROFILE_CHECKS_H
#define PROFILE_CHECKS_H

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

// What the file holds for each level is the hardest difficulty it's been finished on: none, easy, medium or hard
const unsigned char kProfileHardest = 3;

// Makes every level's difficulty one of those. levels is three species with sixteen levels each.
inline void ProfileChecks_LimitLevels(char levels[3][16])
{
	for (int species = 0; species < 3; species++)
	{
		for (int level = 0; level < 16; level++)
		{
			if ((unsigned char)levels[species][level] > kProfileHardest)
			{
				levels[species][level] = (char)kProfileHardest;
			}
		}
	}
}

// How many difficulties the menu can offer for a level, from what the level before it was finished on. This becomes the number of rows in a list of them, and the
// highlighted row is the last, so it's kept to at least one row and no more than the list holds.
inline int ProfileChecks_DifficultiesAllowed(char finishedOn)
{
	unsigned char const value = (unsigned char)finishedOn;

	return (value < 1) ? 1 : (value > kProfileHardest) ? kProfileHardest : value;
}

#ifdef _WIN32
// When a profile was last used or a game saved, as text for a menu. The time comes straight from a file, and the player's own settings decide how long each part is, up to 180 characters. So:
// - Windows refuses a time that can't be, such as a 13th month, and then that part is left empty. Without that, the box would hold whatever was in it, and adding to it could run on down the stack.
// - Windows also refuses one too long for its space, for the same outcome. It's told 80 for the date because the game always has, and 99 for the time, so there's room for the end of the text.
// - The two together are cut to fit the box the text goes in, however long they are.
// The game gives no formats, which means the player's own. A test gives its own, so that it can make the parts long for real.
inline void ProfileChecks_MakeTimeText(const SYSTEMTIME &when, DWORD dateKind, const char *dateFormat, const char *timeFormat, char *text, size_t size)
{
	if (size == 0)
	{
		return;
	}

	char date[100] = { 0 };
	char time[100] = { 0 };

	// Windows doesn't take a kind of date and a format as well
	if (!GetDateFormatA(GetThreadLocale(), dateFormat ? 0 : dateKind, &when, dateFormat, date, 80))
	{
		date[0] = 0;
	}

	if (!GetTimeFormatA(GetThreadLocale(), 0, &when, timeFormat, time, 99))
	{
		time[0] = 0;
	}

	_snprintf(text, size - 1, "%s  %s", time, date);
	text[size - 1] = 0;
}
#endif

// The cheat-mode menu's sliders step round their choices until they land on one that's allowed. A profile says what's allowed, so a crafted one can allow none at all: a cheat mode turned on
// with no level completed, say. Stepping round for ever would hang the menu. So this goes once round, which brings the slider back to where this search started if nothing was allowed. A key
// press moves the slider one before it asks, so with nothing allowed the slider walks along a step a press. It's the Start button that refuses to go: CheatMode_SelectionIsAllowed. "allowed"
// can look at the slider's own number as well as what it's given, because the number is kept up to date on the way round.
template <class Allowed>
inline void ProfileChecks_StepToAllowed(int *value, int howMany, int searchForward, Allowed allowed)
{
	for (int tried = 0; tried < howMany; tried++)
	{
		if (allowed(*value))
		{
			return;
		}

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

#endif
