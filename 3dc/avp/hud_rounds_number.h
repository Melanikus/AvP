// How the number of rounds on the marine's HUD moves. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT DOES: when the gun holds more rounds than the number says, as when the game loads or a magazine has just gone in, the
// number climbs to it at 128 rounds a second. When the gun holds fewer, because he fired, the number drops straight there.
//
// WHY THE LEFTOVER TIME IS KEPT: the game used to add one round for each 512 parts of a frame's time, and threw the rest away. A
// frame shorter than about 8 milliseconds is worth less than one round, so above about 125 frames a second the number hardly moved
// at all, and it could say 0 with a full gun. Keeping what's left over for the next frame gives the same speed at any frame rate.
//
// This file knows nothing about the game, so tests/weapon_pulse_counter_test.cpp can try it.

#ifndef HUD_ROUNDS_NUMBER_H
#define HUD_ROUNDS_NUMBER_H

// A second of frame time is 65536 parts, so one round for each 512 parts is 128 rounds a second
static int const kHudRoundsClimbTime = 512;

// Once each frame. shown is the number on the screen, timeLeftOver is the part of a round's time that the last frame didn't use, and
// rounds is what the gun holds, in whole rounds.
static inline void HudRounds_Move(int *shown, int *timeLeftOver, int rounds, int frameTime)
{
	if (rounds <= *shown)
	{
		// Nothing to climb to, and time going by now isn't saved up for the next climb
		*shown = rounds;
		*timeLeftOver = 0;
		return;
	}

	// In 64 bits, because a frame after a long pause could otherwise wrap round to less than nothing
	long long const time = (long long)*timeLeftOver + ((frameTime > 0) ? frameTime : 0);
	long long const climbed = time / kHudRoundsClimbTime;

	if (climbed >= (long long)rounds - *shown)
	{
		// It's got there, and it stops there rather than going past
		*shown = rounds;
		*timeLeftOver = 0;
		return;
	}

	*shown += (int)climbed;
	*timeLeftOver = (int)(time % kHudRoundsClimbTime);
}

#endif
