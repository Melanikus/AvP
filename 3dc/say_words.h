// SAY marks' words and line (with -dev -saymarks; hudson_saymarks.h). Written 2026-11-21 by Melanikus (Frost) for the AvP port, Bishop for
// the owner: "if I say 'stuck' in SAY log do you see it".
//
// WHAT IT'S FOR: while he plays, SAY <words> in the console (F11 opens it with SAY typed), or the tester's mark key F2, drops a MARK line into
// the lane probe's log, with every alien within 20 m and the 3 seconds of their lines before it. This file makes the words safe and writes
// the MARK line itself; the probe (hudson_lanes.cpp) adds the aliens.
//
// THE WORDS ARE HOSTILE INPUT (Tao 3): they come from a keyboard, so only what can't hurt the log is kept. At most 250 characters (the
// console's own most); each one a printable ASCII character, space to ~, or else ?, so no new line, return, tab or control character can make
// a line of its own, and no byte of another code page can; a double quote becomes ' and a backslash becomes /, so the words can't end said="
// early or start an escape in a reader. They are only ever written as data inside the line: never a printf format, never a file's name or
// a path. tests/say_words_test.cpp tries each rule.
#ifndef SAY_WORDS_H
#define SAY_WORDS_H

#include <stdio.h>
#include <string>

// The most characters of his words kept (the console takes 250 at most, text_input.hpp MAX_LENGTH_INPUT)
static const size_t kSayWordsMost = 250;

// A mark names every alien this near him, in millimetres (the game's units), and keeps this many seconds of their lines before it
static const int kSayMarksNearMm = 20000;
static const int kSayMarksSecondsBefore = 3;

// So a held key can't fill the disk: at most this many marks a run (and one a second), this many lines kept for each, and say-marks.txt
// never written past this many bytes
static const int kSayMarksMost = 100;
static const size_t kSayMarksLinesKept = 4096;
static const long kSayMarksFileMostBytes = 64L * 1024 * 1024;

// His words as they are written: printable ASCII only (else ?), " as ', \ as /, at most kSayWordsMost, ending at the first zero
inline std::string SayWords_Clean(const char *words)
{
	std::string kept;

	if (!words)
	{
		return kept;
	}

	for (size_t i = 0; words[i] != '\0' && kept.size() < kSayWordsMost; i++)
	{
		unsigned char const c = (unsigned char)words[i];

		if (c == '"')
		{
			kept += '\'';
		}
		else if (c == '\\')
		{
			kept += '/';
		}
		else if (c >= 32 && c <= 126)
		{
			kept += (char)c;
		}
		else
		{
			kept += '?';
		}
	}

	return kept;
}

// The MARK line: its number this run, the game time in seconds, the frame, his words (already cleaned) between said=" and the next ", and his
// place (the game's x,y,z), facing (0 to 4095), health and module (name#index as the probe writes it; a space becomes _, none gives -)
inline std::string SayWords_MarkLine(int number, double seconds, long frame, const std::string &cleanWords, int x, int y, int z, int facing,
	int health, const std::string &module)
{
	char head[96];
	_snprintf(head, sizeof(head) - 1, "MARK %d t=%.3f f=%ld said=\"", number, seconds, frame);
	head[sizeof(head) - 1] = '\0';

	char place[128];
	_snprintf(place, sizeof(place) - 1, "\" me pos=%d,%d,%d facing=%d health=%d mod=", x, y, z, facing, health);
	place[sizeof(place) - 1] = '\0';

	std::string where = module.empty() ? std::string("-") : module;

	for (char &c : where)
	{
		c = c == ' ' ? '_' : c;
	}

	return std::string(head) + cleanWords + place + where;
}

#endif
