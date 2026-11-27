// hive_waits.h: how long the hive and its generators wait, worked out so that no wait a level can ask for overflows. Written 2026-11-27 by Melanikus
// (Drake) for the AvP port; the game's own licence (License.txt).
//
// WHAT FOR. The hive turns between attacking and regrouping after a wait the level sets (its GLOGENDC chunk, in seconds), and each generator waits
// between the NPCs it makes (bh_generator.cpp, ResetHiveStateTime and ResetGeneratorTimer). The game holds a wait in 65536ths of a second (ONE_FIXED)
// in a whole number, and gives it "an eighth either way": seven eighths of it, and a random part of a quarter of it more. In 1999 those sums were done
// in that same whole number, and seven times a wait over 4,681 s is more than it holds. Twenty-seven of the game's own levels ask for longer, most of
// them 32,000 s, and so turned round after 3,424 s to 11,424 s where 28,000 s or more was meant. A generator making one NPC every 100 minutes (its own
// rate of 1; a rate of 0 or less is made 1) ran over the same way, and waited the shortest time, 3 s, where the longest, 120 s, was meant; so did the
// wait the generators share, with more than 78 generators to share one NPC a minute. A whole number running over like this is also something C++
// leaves undefined, so a compiler may do anything at all with it.
//
// HOW. The sums are done here in 64 bits, which hold seven times any wait a whole number can, and the caller then keeps the answer between the two
// numbers the game holds it to. Wherever 1999's sums didn't run over, the answer is the same to the last 65536th of a second, and the random number
// is asked for exactly when it was (when a quarter of the wait isn't 0), so the game's random numbers come out in the same order as before.
#pragma once

#include <stdint.h>

// Seven eighths of baseTime, and random's part of a quarter of it more: from 7/8 of baseTime to just under 9/8. random gives 0 or more, as FastRandom
// does, and isn't asked when a quarter of baseTime is 0.
inline int64_t HiveWaits_AnEighthEitherWay(int64_t baseTime, int (*random)(void))
{
	int64_t const quarter = baseTime / 4;

	return (baseTime * 7) / 8 + (quarter ? random() % quarter : 0);
}

// The wait the generators share when one has no rate of its own: a minute (60 times ONE_FIXED) shared out among the NPCs the level makes a minute,
// times the number of generators, as each takes its turn. Fewer than 1 a minute is taken as 1.
inline int64_t HiveWaits_SharedGeneratorTime(int perMinute, int generators)
{
	return (int64_t)((60 * 65536) / (perMinute > 1 ? perMinute : 1)) * generators;
}

// A wait kept from lowest to highest, both included
inline int HiveWaits_Kept(int64_t wait, int lowest, int highest)
{
	return (int)(wait < lowest ? lowest : (wait > highest ? highest : wait));
}
