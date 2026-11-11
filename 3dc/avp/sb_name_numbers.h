// The number in a new name for something made while the game runs. Written 2026-11-08 by Melanikus for the AvP port (#139).
//
// WHY THIS IS HERE: everything made while the game runs (a grenade, an alien from a generator, a ghost of another player's rocket) gets a
// name, so the rest of the game can find it again. The name holds a number that counts up, and the network uses that number, with the
// player's tag, as the thing's id in its messages. The number has room for 16,777,215 and then goes back to 1. A long enough game reaches
// that, after days of play, sooner if other players send a flood of things, and the number that comes round again can be one something
// alive still has. Then a message, a switch or a saved link meant for one of them finds the other.
//
// So a number that something alive has is passed over. The game has at most 1000 things alive at once (maxstblocks), so among any 1001
// numbers in a row one is free, and the search always ends. It's checked every time, not only after the number has gone round once:
// a saved game brings the number back but not whether it had gone round, and the check costs at most 1000 small comparisons.
//
// This file knows nothing about the game, so tests/sb_name_numbers_test.cpp can try all of it.

#ifndef SB_NAME_NUMBERS_H
#define SB_NAME_NUMBERS_H

// The most a name's number can be: three bytes. The fourth byte of those four is the player's tag (AssignNewSBName, strategy_def.cpp).
unsigned int const kSBNameMostNumber = 0xffffff;

// The number after last, back to 1 after kSBNameMostNumber, passing over any that inUse says something alive has. At most mostAlive + 1
// numbers are tried, and if every one of them is in use, which can't happen with no more than mostAlive things alive, the last one tried
// is given. A last that's out of range (a saved game is read from a file, so it can say anything) starts again from 1.
template <typename InUse>
inline unsigned int SBNameNumber_Next(unsigned int last, InUse inUse, unsigned int mostAlive)
{
	unsigned int number = last;

	for (unsigned int tries = 0; tries <= mostAlive; tries++)
	{
		number = (number >= kSBNameMostNumber) ? 1 : number + 1;

		if (!inUse(number))
		{
			return number;
		}
	}

	return number;
}

#endif
