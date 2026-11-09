// How another player's figure, or an alien the host runs, is guessed between messages. Written 2026-11-08 by Melanikus for the AvP port
// (issue #88). The sums are the game's own, moved here without a change from pldghost.cpp (PlayerGhostExtrapolation) and pldnet.cpp (where
// a player's full state arrives, and where an alien's arrives: the aliens the host's AI runs in co-op and skirmish), so that
// tests/ghost_guess_test.cpp can run them without the game.
//
// HOW IT WORKS: a full message about another player says where they are and how fast they're going. Between messages, the game moves
// their figure on at that speed, so it doesn't stand still. A clock says how far into the gap the figure is: it runs on with this machine's
// frames, and each full message takes off the time the sender says has passed since the last one. When messages come steadily, that keeps
// the guess to the gap between two of them. The clock stops at half a second, so a figure with no news for longer stands still. A sharp
// change of speed starts the clock again. An alien's message carries its speed too, so aliens are guessed the same way. The other NPCs the
// host runs in co-op (marines, predators, xenoborgs, the queen, facehuggers) share a message with no speed, so they aren't guessed: they
// step from one message to the next (see engine/coop-lag-plan.md).
//
// The clock counts 65536 to a second, as the game's frame time does (ONE_FIXED).

#ifndef GHOST_GUESS_H
#define GHOST_GUESS_H

#include "3dc.h"
#include "inline.h"

// The longest the guess goes on: half a second
static const int kGhostGuessLongest = ONE_FIXED / 2;

// A full message about the figure has come. speedChanged says whether its speed is different enough to start the guess again, which each
// kind of message decides in its own way. senderTime is how much time the sender says has passed in all, its frame times added up, and
// *lastSenderTime is what it said at the last full message. The clock is *timer, and *timerLast is how much of it has been used.
inline void GhostGuess_Message(int *timer, int *timerLast, unsigned int *lastSenderTime, bool speedChanged, int senderTime)
{
	if (speedChanged)
	{
		*timer = -ONE_FIXED;
	}

	*timerLast = 0;

	// The time the sender says has passed since its last full message is taken off the clock. A sender's time that goes backwards, which
	// happens when its count starts again, starts the clock again too.
	if (senderTime >= (int)*lastSenderTime)
	{
		*timer -= (senderTime - *lastSenderTime);
	}
	else
	{
		*timer = -ONE_FIXED;
	}

	*lastSenderTime = senderTime;
}

// One of this machine's frames, frameTime long. Gives back the speed to move the figure at for this frame: its last speed, part of it if
// the guess runs out partway through the frame, or none at all once the guess has run out. A figure with no speed has nothing to guess:
// *stopped says so, and its clock starts from nothing when it moves again.
inline VECTORCH GhostGuess_FrameSpeed(int *timer, int *timerLast, VECTORCH speed, int frameTime, bool *stopped)
{
	VECTORCH none = { 0, 0, 0 };

	*timer += frameTime;

	if (*timer < 0)
	{
		*timer = 0;
	}

	if (*timer > kGhostGuessLongest)
	{
		*timer = kGhostGuessLongest;
	}

	// How much of the clock this frame uses
	int const time = *timer - *timerLast;
	*timerLast = *timer;

	*stopped = speed.vx == 0 && speed.vy == 0 && speed.vz == 0;

	if (*stopped)
	{
		*timerLast = *timer = 0;
		return none;
	}

	if (time <= 0)
	{
		return none;
	}

	// Only part of a frame is guessed, so only that part of the speed is used
	if (time != frameTime)
	{
		speed.vx = WideMulNarrowDiv(speed.vx, time, frameTime);
		speed.vy = WideMulNarrowDiv(speed.vy, time, frameTime);
		speed.vz = WideMulNarrowDiv(speed.vz, time, frameTime);
	}

	return speed;
}

#endif
