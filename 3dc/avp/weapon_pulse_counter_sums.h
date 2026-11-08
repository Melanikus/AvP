// What the two-digit counter on the pulse rifle's model should say. Written 2026-11-06 by Melanikus for the AvP port.
// Behaviour worked out from Mupp's M5 version of the game.
//
// This file knows nothing about the rest of the game, so tests/weapon_pulse_counter_test.cpp can try it without a level
// or a model. weapon_pulse_counter.cpp is what joins it to the gun.
//
// HOW THE COUNTER BEHAVES:
//   - It shows what the number of rounds on the screen shows. That number climbs when a gun has just been filled, as when the game
//     loads, so the digits climb with it and the two agree. When he fires, both drop at once.
//   - In a reload it holds its number until the magazine comes out, then spins quickly down to 00. An empty gun has nothing to spin.
//   - When the new magazine goes in, it jumps to what the gun will hold when the reload ends. That's usually a full magazine, but
//     with no spare magazines it can be only the rounds left over from earlier reloads. The number on the screen only starts
//     climbing when the reload ends, so the digits don't go back to climb with it: they stay with the gun's rounds until it has
//     caught up.
//
// Rounds are in the game's numbers throughout, where 65536 is one round.

#ifndef WEAPON_PULSE_COUNTER_SUMS_H
#define WEAPON_PULSE_COUNTER_SUMS_H

#include "weapon_reload.h"

// How fast it spins down: 512 rounds a second, so under a fifth of a second from 99
static int const kPulseCounterSpinSpeed = 512;

// Two digits can't show any more than this
static int const kPulseCounterMost = 99;

struct PulseCounterState
{
	unsigned int shown;    // the rounds it's showing
	bool spinningDown;     // the magazine has come out
	bool newMagazineIn;    // the new one has gone in. The game counts its rounds at the end of the reload.
	bool waitingForTheScreen;  // it has already jumped to a new magazine, and the number on the screen is still climbing up to it
};

static inline void PulseCounter_Start(struct PulseCounterState *counter)
{
	counter->shown = 0;
	counter->spinningDown = false;
	counter->newMagazineIn = false;
	counter->waitingForTheScreen = false;
}

// What the gun will hold when this reload ends, worked out by the same sum the reload itself uses. These numbers are this function's
// own copies, so asking changes nothing in the gun.
static inline unsigned int PulseCounter_AfterTheReload(unsigned int roundsInGun, unsigned short pile, unsigned char magazines, unsigned int fullMagazine)
{
	Reload_FillGun(&roundsInGun, &pile, &magazines, fullMagazine);

	return roundsInGun;
}

// What it shows outside a reload. roundsOnTheScreen is in whole rounds, as that number is.
static inline unsigned int PulseCounter_Following(struct PulseCounterState *counter, unsigned int roundsInGun, int roundsOnTheScreen)
{
	unsigned int const wholeInGun = (roundsInGun >> 16) + (((roundsInGun & 0xffff) != 0) ? 1 : 0);
	unsigned int const onTheScreen = (roundsOnTheScreen > 0) ? (unsigned int)roundsOnTheScreen : 0;

	// The screen has got there, or the gun has just dropped below it because he fired
	if (onTheScreen >= wholeInGun)
	{
		counter->waitingForTheScreen = false;
		return roundsInGun;
	}

	// It's less than the gun's rounds here, and those can't be more than 65536 whole ones, so this can't overflow
	return counter->waitingForTheScreen ? roundsInGun : onTheScreen << 16;
}

// Once each frame. magazineOut and magazineIn say whether the reload's movement has just got to those points.
static inline void PulseCounter_Update(struct PulseCounterState *counter, unsigned int roundsInGun, int roundsOnTheScreen, unsigned int afterTheReload, bool reloading, bool magazineOut, bool magazineIn, int frameTime)
{
	if (!reloading)
	{
		counter->spinningDown = false;
		// A reload that got as far as the new magazine has just ended
		counter->waitingForTheScreen = counter->waitingForTheScreen || counter->newMagazineIn;
		counter->newMagazineIn = false;
		counter->shown = PulseCounter_Following(counter, roundsInGun, roundsOnTheScreen);
		return;
	}

	if (magazineOut && !counter->newMagazineIn)
	{
		counter->spinningDown = true;
	}

	if (magazineIn)
	{
		counter->spinningDown = false;
		counter->newMagazineIn = true;
	}

	if (counter->newMagazineIn)
	{
		counter->shown = afterTheReload;
	}
	else if (counter->spinningDown)
	{
		// Worked out in 64 bits, because a frame after a long pause could otherwise wrap round to a small number
		unsigned long long const fall = (frameTime > 0) ? (unsigned long long)frameTime * (unsigned int)kPulseCounterSpinSpeed : 0;

		counter->shown = (fall >= counter->shown) ? 0 : counter->shown - (unsigned int)fall;
	}
}

// The whole number to show. Part of a round left counts as a round, so it only shows 00 when the magazine really is empty.
static inline int PulseCounter_WholeRounds(unsigned int rounds)
{
	unsigned int const whole = (rounds >> 16) + (((rounds & 0xffff) != 0) ? 1 : 0);

	return (whole > (unsigned int)kPulseCounterMost) ? kPulseCounterMost : (int)whole;
}

static inline int PulseCounter_Tens(int wholeRounds)
{
	return wholeRounds / 10;
}

static inline int PulseCounter_Units(int wholeRounds)
{
	return wholeRounds % 10;
}

// Which frame of a patch's animation shows this digit. Frames are counted in the game's numbers too, where 65536 is one frame.
// The patch comes from a model file, which might not have ten frames. A digit it doesn't have would read past the end of
// its list of frames, so it shows its first one instead.
static inline int PulseCounter_FrameForDigit(int digit, int framesInThePatch)
{
	return (digit >= 0 && digit < framesInThePatch) ? (digit << 16) : 0;
}

#endif
