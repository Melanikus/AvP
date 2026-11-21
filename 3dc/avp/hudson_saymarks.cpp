// hudson_saymarks.cpp: SAY marks (hudson_saymarks.h says what they are, and what they never do). Written 2026-11-21 by Melanikus (Frost) for
// the AvP port, Bishop for the owner: "if I say 'stuck' in SAY log do you see it".
#include "command_line_word.h"
#include "say_words.h"
#include "hudson.h"
#include "hudson_lanes.h"
#include "hudson_saymarks.h"

// The switch. tools/check-hudson-asleep.ps1 checks it starts false, that only HudsonSayMarks_ReadCommandLine below makes it true, and that it is
// read only in the first check of the watch-only functions it lists.
bool SayMarksAreOn = false;

bool HudsonSayMarks_IsAskedFor(const char *commandLine)
{
	return CommandLineHasWord(commandLine, "-dev") && CommandLineHasWord(commandLine, "-saymarks");
}

// No folder check, unlike a run of Hudson's: the watch writes only its own log into the game's save folder, as the game's own log does, and
// changes nothing in the game or in the folder it runs from.
void HudsonSayMarks_ReadCommandLine(const char *commandLine)
{
	if (!HudsonSayMarks_IsAskedFor(commandLine))
	{
		return;
	}

	SayMarksAreOn = true;
}

// In a Hudson run, the run's own probe watches and writes hudson-lanes.txt, so SAY marks stand aside
void HudsonSayMarks_EndOfFrame()
{
	if (!SayMarksAreOn || HudsonRunIsOn)
	{
		return;
	}

	HudsonLanes_SayMarksFrame();
}

// His words are hostile input (Tao 3): say_words.h keeps them printable, 250 at most, and the probe writes them only as data inside its line
void HudsonSayMarks_Said(const char *words)
{
	if (!SayMarksAreOn || HudsonRunIsOn)
	{
		return;
	}

	HudsonLanes_SayMark(SayWords_Clean(words).c_str());
}
