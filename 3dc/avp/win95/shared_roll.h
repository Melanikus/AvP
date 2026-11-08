// A die roll that comes out the same on every machine in a network game.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// Some things in a level can be in any of several places, and the game rolls a die as the level loads to choose
// which. In single player that's fine. But each machine's random numbers start from its own clock, so in a network
// game a pick-up could be in one place on one machine and somewhere else on another.
//
// So in a campaign the roll is worked out instead. The host picks one number for the game and tells everyone in the
// game description, and each object mixes that number with something only it has. Every machine then gets the same
// answer for that object, and it doesn't matter what order they load things in, or if one of them skips some
// objects altogether, as a machine that doesn't run the NPCs does.
//
// The number can be anything at all, even from a machine that means harm: the caller only uses the answer modulo
// how many places that object really has.
//
// Nothing here touches the game, so a test can check it.

#ifndef _shared_roll_h_
#define _shared_roll_h_ 1

// Stirs one number into another, so that a small change in either changes the whole answer.
inline unsigned int SharedRoll_Stir(unsigned int soFar, int number)
{
	return (soFar ^ (unsigned int)number) * 16777619u;
}

// From 0 to 65535, the same range as the game's own rolls here.
inline int SharedRoll(int gameNumber, int first, int second, int third)
{
	unsigned int value = SharedRoll_Stir(2166136261u, gameNumber);

	value = SharedRoll_Stir(value, first);
	value = SharedRoll_Stir(value, second);
	value = SharedRoll_Stir(value, third);

	// The top bits are the best stirred
	return (int)((value >> 16) & 0xffff);
}

#endif
