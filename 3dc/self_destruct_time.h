// self_destruct_time.h: a self destruct's countdown, from the level's file into the game. Written 2026-11-16 by Melanikus for the AvP port.
//
// WHY: a level file gives the countdown in whole seconds (SelfDestructStrategy::timer, strategy_chunk.cpp: an int read as it is, with no look at its value), and the game keeps it in
// 16.16 fixed point (SELF_DESTRUCT_TOOLS_TEMPLATE::timer, bh_self_destruct.h), so object_setup.cpp turns the one into the other. Shifted as it is, a signed int holds that only for 0 to
// 32767 seconds: a larger number overflows it (undefined in C++, and in practice a negative countdown, which runs out the moment the self destruct is set off), and a negative number
// shifted is undefined too. A level file can come from anyone, so the number is held before it's turned.
//
// THE RULE: the countdown is HELD to 0 to 32767 seconds (about 9 hours), never refused, so the level still loads and the countdown is one the game can hold: a negative one becomes 0, as
// the old editor kept a negative answer (inteng FUN_00535c90), and a larger one becomes 32767, which is also the most AVP_OpenInteng's editor writes (strategy_fields.cpp).
//
// This file knows nothing about the game, so tests/self_destruct_time_test.cpp can run it on its own.

#ifndef SELF_DESTRUCT_TIME_H
#define SELF_DESTRUCT_TIME_H

// The most seconds a countdown can be and still fit, turned into 16.16, in a signed int.
enum { kSelfDestructMostSeconds = 32767 };

// Whether the file's number is one the game takes as it is.
inline bool SelfDestruct_SecondsFit(int seconds)
{
	return seconds >= 0 && seconds <= kSelfDestructMostSeconds;
}

// The countdown in the game's 16.16 fixed point, for a number of seconds from the level's file: held to 0 to 32767 first, so the product always fits.
inline int SelfDestruct_FixedTimer(int seconds)
{
	if (seconds < 0)
	{
		seconds = 0;
	}

	if (seconds > kSelfDestructMostSeconds)
	{
		seconds = kSelfDestructMostSeconds;
	}

	return seconds * 65536;
}

#endif
