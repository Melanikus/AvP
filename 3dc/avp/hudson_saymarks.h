// hudson_saymarks.h: SAY marks, the lane probe WATCHING a game the player plays himself. Written 2026-11-21 by Melanikus (Frost) for the AvP
// port, Bishop for the owner: "if I say 'stuck' in SAY log do you see it".
//
// ASKED FOR with -dev -saymarks, each as a word on its own on the command line (HudsonSayMarks_IsAskedFor). In a normal game none of it runs:
// SayMarksAreOn starts false and only HudsonSayMarks_ReadCommandLine makes it true, once, when both words are there.
//
// WHAT IT DOES: while he plays, from the level's first frame, the lane probe (hudson_lanes.h) reads every near alien each frame, as it does in a
// Hudson run, and keeps the last 3 seconds of those lines in memory. When he types SAY <words> in the console (F11 opens it with SAY typed), or
// presses the tester's mark key F2, it writes into say-marks.txt, in the game's save folder:
//   MARK n t= f= said="<his words>" me pos= facing= health= mod=      (say_words.h: his words are hostile input, kept printable, 250 at most)
//   NEAR a<n> dist=<mm> | <that alien's last A line>                   one for every alien within 20 m of him
//   BEFORE <the A and EVENT lines of the 3 seconds before the mark>    oldest first
//   MARK n END ...
// AvP-docs/tools/frost/lanes/marks-report.cjs lists each mark with them.
//
// WHAT IT NEVER DOES (Bishop's conditions; tools/check-hudson-asleep.ps1 holds the code to them): it only watches. No Hudson play, no supply, no
// immortality, no stock forcing, no standat, no time end, no menu, window or video call: those start asleep on HudsonRunIsOn alone, and
// SayMarksAreOn is read only in the first check of the functions the check lists as watch-only. In a Hudson run it does nothing (the run's
// own probe writes hudson-lanes.txt). A normal game's SAY is untouched: its words still go to the network as before, and SAY marks only read
// them. At most kSayMarksMost marks a run, at most one a second, each with at most kSayMarksLinesKept lines, and say-marks.txt is never
// written past kSayMarksFileMostBytes, so holding a key down can't fill the disk.
#pragma once

// The switch: false unless -dev -saymarks. Only its reader sets it (check-hudson-asleep.ps1); only Hudson's files name it.
extern bool SayMarksAreOn;

// Whether the command line asks for SAY marks: both words, -dev and -saymarks
bool HudsonSayMarks_IsAskedFor(const char *commandLine);

// Once, as the game starts (winmain.cpp, beside Hudson_ReadCommandLine): turns SAY marks on if they are asked for
void HudsonSayMarks_ReadCommandLine(const char *commandLine);

// Each frame of play (winmain.cpp, beside the tester's mark key): the watch's frame count and game time, a new level noticed, its log opened
void HudsonSayMarks_EndOfFrame();

// What he said (game_commands.cpp's SAY, before its words go to the network as always), or "F2" from the tester's mark key: a MARK
void HudsonSayMarks_Said(const char *words);
