// NEWDYNAMICCOLLISIONS: how small a move the collision code may treat as nothing, so that a slide along a wall goes on at any frame
// rate. Written 2026-11-09 by Melanikus for the AvP port, for issue #180.
//
// WHY THIS IS HERE: when something moves into a wall, dynamics.cpp does two things with a fixed size of 16 mm.
// - It looks for the spot just short of the wall by halving the gap again and again, and stops looking once the halves are 16 mm or
//   less apart.
// - Whatever is left of the move, turned to slide along the wall, is thrown away if it's 16 mm or less.
// A frame's move is the speed times the frame's length. So the faster the game runs, the smaller every move is, and the more of them
// fall under 16 mm. At 240 frames a second a player sliding along a wall at under about 4 metres a second stops dead, where at 60
// frames a second they'd only stop under about 1 metre a second. That's issue #180: "easier to snag on a wall and stop instantly".
//
// WHAT THIS DOES: the 16 mm gets smaller with the frame, so that a slide is kept at the same speed, about 1 metre a second, at every
// frame rate from 60 to 500 a second. 1999's rule throws away 16 mm or less, so it keeps a move of 17 mm or more. Here a frame keeps a move of at least its
// share of 17 mm, as a 16 ms frame is to it, in whole millimetres, and throws away anything less:
// - a frame of 16 ms or longer, which is every frame at 60 frames a second or slower, throws away exactly what 1999 did: 16 mm or less;
// - a 4 ms frame (240 frames a second) throws away 3 mm or less;
// - and never less than 1 mm, however short the frame.
// The halving stops at the same new size, so it takes about as many steps a frame as it did at 60 frames a second.
//
// THE OWNER'S OWN EXCEPTION: this project keeps the 1999 movement exactly, unless something is plainly a bug. For this bug the owner
// chose to change it, and to have it on by default, with the console's NEWDYNAMICCOLLISIONS OFF to put 1999's way back and compare
// (his decision, 2026-11-09). By his choice there's no command-line switch for it, and it starts on in every run of the game.
//
// This file knows nothing about the game, so tests/new_dynamic_collisions_test.cpp can try it.

#ifndef NEW_DYNAMIC_COLLISIONS_H
#define NEW_DYNAMIC_COLLISIONS_H

#include "view_switches.h"

// 1999's size: a move of this many millimetres or fewer is nothing.
int const kNothingIn1999 = 16;

// A 16 ms frame, as the game counts time: 65536 is one second, and io.cpp's clock counts whole milliseconds, so this is 16 * 65536 / 1000,
// rounded down as the game rounds it. At 60 frames a second the clock says 16 or 17 ms for each frame.
int const kSixtyFramesFrame = 1048;

// Never less than this, however short the frame: a move of 1 mm is always nothing.
int const kNothingAtLeast = 1;

// How many millimetres of a move, or fewer, count as nothing in a frame this long. frameTime is NormalFrameTime, where 65536 is one second.
// With the new way off, it's always 1999's 16.
inline int NewDynamicCollisions_NothingUpTo(int frameTime, int newWayOn)
{
	if (!newWayOn || frameTime >= kSixtyFramesFrame)
	{
		return kNothingIn1999;
	}

	if (frameTime < 1)
	{
		return kNothingAtLeast;
	}

	// The smallest move kept is this frame's share of 17 mm. frameTime is under kSixtyFramesFrame here, so this can't overflow.
	int const smallestKept = ((kNothingIn1999 + 1) * frameTime) / kSixtyFramesFrame;
	int const nothingUpTo = smallestKept - 1;

	return nothingUpTo < kNothingAtLeast ? kNothingAtLeast : nothingUpTo;
}

// The words after NEWDYNAMICCOLLISIONS come from whoever typed them, or from a file, so only these count: nothing switches it, OFF or 0 is
// off, and ON or 1 is on, in capitals or not. Anything else gives back false and leaves it as it was.
inline bool NewDynamicCollisions_FromWords(char const *words, int *newWayOn)
{
	char const *start = "";
	size_t length = 0;

	if (words)
	{
		SwitchWords_Trim(words, &start, &length);
	}

	if (length == 0)
	{
		*newWayOn = *newWayOn ? 0 : 1;
	}
	else if (SwitchWords_Are(start, length, "0") || SwitchWords_Are(start, length, "OFF"))
	{
		*newWayOn = 0;
	}
	else if (SwitchWords_Are(start, length, "1") || SwitchWords_Are(start, length, "ON"))
	{
		*newWayOn = 1;
	}
	else
	{
		return false;
	}

	return true;
}

// What the screen and the log say: at the game's start, and each time it's switched.
inline char const *NewDynamicCollisions_Words(int newWayOn)
{
	return newWayOn ? "NEWDYNAMICCOLLISIONS ON: SLIDES ALONG WALLS GO ON AT ANY FRAME RATE" : "NEWDYNAMICCOLLISIONS OFF: WALLS ARE MET AS IN 1999";
}

// What the screen says to words it doesn't know
char const kNewDynamicCollisionsRefused[] = "NEWDYNAMICCOLLISIONS TAKES ON OR OFF, OR NOTHING TO SWITCH IT";

// The switch itself, in dynamics.cpp: 1, the new way, as every run of the game starts, or 0, 1999's way.
extern int NewDynamicCollisionsOn;


#endif
