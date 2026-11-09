// The game's random numbers. Written 2026-11-08 by Melanikus for the AvP port (issue #121). Credit: the method is the University of
// California, Berkeley's additive random number generator, as in BSD's random(). The game's earlier version of it was Jamie Lokier's,
// from 1995. This is new code, written from the published method, and it gives exactly the numbers his did.
//
// THERE ARE TWO:
// - FastRandom, for anything that should vary from one game to the next: sparks, sounds, which way a creature turns. It starts from
//   the clock when the game starts (SetFastRandom, in maths.cpp), so no two runs draw the same numbers.
// - SeededFastRandom, which starts from a seed it's given (SetSeededFastRandom). Two machines given the same seed draw the same numbers,
//   so in a network game every machine tears a body apart the same way and puts the players in the same places. Its numbers must
//   never change, or two versions of the game would disagree. tests/fast_random_test.cpp holds them to the numbers they've always had.
//
// THE METHOD: each generator keeps a ring of numbers, 31 for FastRandom and 13 for the seeded one. To draw, it adds the number three
// places behind its place in the ring to the number at its place, keeps the sum there, and gives back the sum without its lowest bit
// (a whole number from 0 to 2,147,483,647), then moves one place on. The sums simply wrap round past 4,294,967,295, as unsigned numbers do.
//
// STARTING: each number in the ring is the last one times 1103515145, plus 12345 (wrapping round the same way), beginning from the clock
// or the seed. Then the first draws are thrown away, 310 for FastRandom and 26 for the seeded one, because they're too much like the
// starting numbers. Starting FastRandom also starts the seeded one, from FastRandom's next number.
//
// This file needs nothing from the rest of the game, so the test can try it on its own.

#ifndef FAST_RANDOM_H
#define FAST_RANDOM_H

#include <stdint.h>

// The next of the game's random numbers, from 0 to 2,147,483,647. The game starts it from the clock as it starts (SetFastRandom). Asked
// before that, it starts itself from 0 first.
int FastRandom(void);

// Starts FastRandom from this number, and then the seeded one from FastRandom. SetFastRandom (maths.cpp) gives it the clock.
void FastRandom_StartFrom(uint32_t number);

// The next of the seeded random numbers, from 0 to 2,147,483,647. Asked before it's ever been given a seed, it starts itself from 0 first.
int SeededFastRandom(void);

// Starts the seeded random numbers from this seed. The same seed always gives the same numbers after it.
void SetSeededFastRandom(int seed);

#endif
