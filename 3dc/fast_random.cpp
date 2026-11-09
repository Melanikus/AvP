// The game's random numbers. Written 2026-11-08 by Melanikus for the AvP port (issue #121). See fast_random.h. Credit: the method is
// the University of California, Berkeley's additive random number generator; the game's earlier version was Jamie Lokier's, from 1995.
// This is new code, written from the published method, and it gives exactly the numbers his did.
#include "fast_random.h"

// One generator: a ring of numbers, where it is in the ring, and whether it has been started
template <int kSize>
struct RandomRing
{
	uint32_t numbers[kSize];
	int at;          // the place the next draw adds to
	bool started;
};

// How far behind its place a draw reaches for the number it adds
static const int kReachBack = 3;

// The multiplier and the step for the starting numbers
static const uint32_t kStartTimes = 1103515145u;
static const uint32_t kStartPlus = 12345u;

static RandomRing<31> mainRing = {};
static RandomRing<13> seededRing = {};

// Adds the number kReachBack places behind to the number at this place, keeps the sum, and gives it back without its lowest bit.
// Everything is unsigned, so the sums wrap round, as they're meant to, and the result fits a signed whole number.
template <int kSize>
static int Draw(RandomRing<kSize> &ring)
{
	int const behind = (ring.at + kSize - kReachBack) % kSize;
	ring.numbers[ring.at] += ring.numbers[behind];

	int const result = (int)(ring.numbers[ring.at] >> 1);

	ring.at = (ring.at + 1) % kSize;

	return result;
}

// Fills the ring from a starting number, puts its place back at the start, and throws away the first draws
template <int kSize>
static void Start(RandomRing<kSize> &ring, uint32_t number, int throwAway)
{
	for (int i = 0; i < kSize; i++)
	{
		number = number * kStartTimes + kStartPlus;
		ring.numbers[i] = number;
	}

	// The first draw adds the very first number to the one kReachBack places on, so it starts kReachBack in
	ring.at = kReachBack;
	ring.started = true;

	for (int i = 0; i < throwAway; i++)
	{
		(void)Draw(ring);
	}
}

void SetSeededFastRandom(int seed)
{
	Start(seededRing, (uint32_t)seed, 2 * 13);
}

int SeededFastRandom(void)
{
	if (!seededRing.started)
	{
		SetSeededFastRandom(0);
	}

	return Draw(seededRing);
}

void FastRandom_StartFrom(uint32_t number)
{
	Start(mainRing, number, 10 * 31);
	SetSeededFastRandom(Draw(mainRing));
}

int FastRandom(void)
{
	if (!mainRing.started)
	{
		FastRandom_StartFrom(0);
	}

	return Draw(mainRing);
}
