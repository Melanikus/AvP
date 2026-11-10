// Protocol version 2's replay window: which counters have come already, so no sealed message can be taken twice. Written 2026-11-09 by
// Melanikus for the AvP port.
//
// WHY A WINDOW. Every sealed message carries a counter, one higher than the sender's last. Datagrams can come late, twice, or out of
// order, so "only higher than the last one" would throw honest ones away. Instead the window remembers the highest counter taken, and
// which of the 1,024 counters just below it have been taken, as one bit each (CRYPTO.md section 5.1 point 13, and section 9). A counter is
// new if it's above the highest, or inside the window with its bit not set. Anything older than the window, or taken already, is refused
// (refusal 9).
//
// ONLY AFTER THE TAG. A counter is marked taken only once its message's tag has checked. Otherwise anyone could send a forged datagram with
// an honest counter, and the honest one would be refused when it came.
//
// The relay's Java (java/src/avpx/v2/ReplayWindow.java) keeps the very same window. The sealed control channel (net_sealed_control.h) and
// sealed game datagrams (net_sealed_game.h) each keep one per direction.

#ifndef NET_V2_REPLAY_H
#define NET_V2_REPLAY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint64_t const kNetV2ReplayWindow = 1024;

struct NetV2Replay
{
	uint64_t seen[kNetV2ReplayWindow / 64];   // one bit for each counter in the window, at counter mod 1,024
	uint64_t highest;                         // the highest counter taken, once any has been
	bool any;                                 // whether any has been taken yet
};

inline void NetV2_ReplayClear(NetV2Replay *r)
{
	memset(r, 0, sizeof(*r));
}

inline bool NetV2_ReplayBit(const NetV2Replay *r, uint64_t counter)
{
	uint64_t const slot = counter % kNetV2ReplayWindow;
	return (r->seen[slot / 64] & (static_cast<uint64_t>(1) << (slot % 64))) != 0;
}

// Whether counter could be taken now: above the highest, or in the window and not taken
inline bool NetV2_ReplayIsNew(const NetV2Replay *r, uint64_t counter)
{
	if (!r->any || counter > r->highest)
	{
		return true;
	}

	return r->highest - counter < kNetV2ReplayWindow && !NetV2_ReplayBit(r, counter);
}

// Marks counter taken. Call it only after its message's tag has checked, and NetV2_ReplayIsNew said yes.
inline void NetV2_ReplayTake(NetV2Replay *r, uint64_t counter)
{
	if (!r->any || counter > r->highest)
	{
		// The counters the window moves past fall out of it: their bits are cleared, so they can't be mistaken for counters to come
		if (!r->any || counter - r->highest >= kNetV2ReplayWindow)
		{
			memset(r->seen, 0, sizeof(r->seen));
		}
		else
		{
			// Counted in steps, fewer than 1,024, and never as "c <= counter": with a counter of 2^64 - 1, that test is always true, and
			// the loop would never end
			uint64_t const steps = counter - r->highest;

			for (uint64_t step = 1; step <= steps; step++)
			{
				uint64_t const slot = (r->highest + step) % kNetV2ReplayWindow;
				r->seen[slot / 64] &= ~(static_cast<uint64_t>(1) << (slot % 64));
			}
		}

		r->highest = counter;
		r->any = true;
	}

	uint64_t const slot = counter % kNetV2ReplayWindow;
	r->seen[slot / 64] |= static_cast<uint64_t>(1) << (slot % 64);
}

#endif
