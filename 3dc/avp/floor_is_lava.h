// FLOORISLAVA: whether standard aliens find their way along walls and ceilings to reach you ("For Aliens, Floor is lava", #109), as a
// player's console command. Written 2026-11-11 by Melanikus (Third) for the AvP port.
//
// WHY THIS IS HERE: alien ways (alien_ways.h) lets a standard alien climb walls and cross ceilings to reach its target, where in 1999 it
// mostly ran along the floor. The owner chose to have it in every mode, on by default, with no setting on any menu (#109's rulings,
// 2026-11-08). The way to put 1999's aliens back and compare is the console, the same way the owner chose for NEWDYNAMICCOLLISIONS (#180):
//   FLOORISLAVA        switches it
//   FLOORISLAVA ON     (or 1) on: aliens climb walls and ceilings to reach you
//   FLOORISLAVA OFF    (or 0) off: aliens move as in 1999
// The map an alien finds its way on is built when a level starts, so what's asked here counts from the next level's start. A restart of
// the same level keeps what its start had.
// In a game over the network, only the host moves the aliens, so only the host's FLOORISLAVA counts.
//
// The command's name is PO's, 2026-11-11, from the owner's own words for #109 ("FLOOR IS LAVA IS ESSENTIAL"); the owner can rename it.
//
// This file knows nothing about the game, so tests/floor_is_lava_test.cpp can try it.

#ifndef FLOOR_IS_LAVA_H
#define FLOOR_IS_LAVA_H

#include "view_switches.h"

// The words after FLOORISLAVA come from whoever typed them, or from a file, so only these count: nothing switches it, OFF or 0 is off, and
// ON or 1 is on, in capitals or not. Anything else gives back false and leaves it as it was.
inline bool FloorIsLava_FromWords(char const *words, int *on)
{
	char const *start = "";
	size_t length = 0;

	if (words)
	{
		SwitchWords_Trim(words, &start, &length);
	}

	if (length == 0)
	{
		*on = *on ? 0 : 1;
	}
	else if (SwitchWords_Are(start, length, "0") || SwitchWords_Are(start, length, "OFF"))
	{
		*on = 0;
	}
	else if (SwitchWords_Are(start, length, "1") || SwitchWords_Are(start, length, "ON"))
	{
		*on = 1;
	}
	else
	{
		return false;
	}

	return true;
}

// What the screen and the log say: at the game's start, and each time it's switched
inline char const *FloorIsLava_Words(int on)
{
	return on ? "FLOORISLAVA ON: ALIENS CLIMB WALLS AND CEILINGS TO REACH YOU" : "FLOORISLAVA OFF: ALIENS MOVE AS IN 1999";
}

// What the screen says to words it doesn't know
char const kFloorIsLavaRefused[] = "FLOORISLAVA TAKES ON OR OFF, OR NOTHING TO SWITCH IT";

// What the screen adds when it's switched during a level: the map is built at a level's start
char const kFloorIsLavaFromTheNextLevel[] = "FLOORISLAVA COUNTS FROM THE NEXT LEVEL'S START";

// What the console asked, in alien_ways.cpp: 1 for the alien ways, 0 for 1999's aliens. AlienWays_LevelStart reads it into
// AlienWaysSwitchedOn (alien_ways.h) at each level's start, so it never changes in the middle of a level.
extern int FloorIsLavaAsked;

#endif
