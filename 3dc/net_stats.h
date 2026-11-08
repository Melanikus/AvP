// The network's own numbers, in the log, for testers playing across the world. Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: when a tester says "it lagged at the lift", the log should say what the network was doing then. So in a network game,
// every 10 seconds, the log gets one line for each connection this machine has, and one for how much went in and out:
//
//     net: connection 1 round trip 180 ms loss 2% resends 3 (round trips vary by 12 ms; 410 sent)
//     net: in 61440 bytes, out 58320 bytes, in the last 10 s
//
// And a line the moment a connection starts to struggle (a round trip over 500 ms, or more than 10% of packets lost) and another when
// it's fine again, so a bad patch stands out without reading every line. A connection is named by its number, the same one the screen
// names when a player joins, and never by its address (the user's ruling: "You can save times it took, no IPs, no usernames").
//
// WHERE THE NUMBERS COME FROM: ENet keeps them for each connection. The round trip and how much it varies are its running averages.
// Loss and resends are what ENet has counted in its own count of about 10 seconds, which it starts again after each one: a resend is a
// packet that had to go again because no answer came. Bytes are the whole machine's, since the last line.
//
// Words a log reader can look for: "net: connection N round trip N ms loss N% resends N". Anything more comes after those, so a reader that
// knows only those still matches.
//
// tests/net_stats_test.cpp tries these. networking.cpp calls them about once a second while a network game runs.
#ifndef NET_STATS_H
#define NET_STATS_H

#include <stdio.h>
#include <stddef.h>

// How often the full lines are written, in milliseconds
static const unsigned int kNetStatsEveryMilliseconds = 10000;

// A connection struggles above these: a round trip in milliseconds, and a share of packets lost, as a percentage
static const unsigned int kNetStatsSlowRoundTrip = 500;
static const unsigned int kNetStatsLossyPercent = 10;

// And it's only fine again below these, a little under, so that a connection right on the line doesn't fill the log going back and forth
static const unsigned int kNetStatsFineRoundTrip = 450;
static const unsigned int kNetStatsFinePercent = 8;

// ENet gives loss as a share of this (its ENET_PEER_PACKET_LOSS_SCALE). networking.cpp checks the two are the same when it's built.
static const unsigned int kNetStatsLossScale = 1 << 16;

// One connection's numbers, as ENet keeps them
struct NetStats_Connection
{
	unsigned int roundTrip;          // milliseconds
	unsigned int roundTripVariance;  // milliseconds
	unsigned int loss;               // a share of kNetStatsLossScale
	unsigned int packetsLost;        // in ENet's current count: what had to be sent again
	unsigned int packetsSent;        // in ENet's current count
};

// Loss as a whole percentage, rounded. A share bigger than the whole (which ENet never gives) still comes out as at most 100.
inline unsigned int NetStats_LossPercent(unsigned int loss)
{
	unsigned long long const percent = ((unsigned long long)loss * 100 + kNetStatsLossScale / 2) / kNetStatsLossScale;
	return percent > 100 ? 100 : (unsigned int)percent;
}

// The every-10-seconds line for one connection, numbered from 1, in words of size characters, which always end with a 0
inline void NetStats_ConnectionLine(int connection, const NetStats_Connection &c, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	int const used = _snprintf(words, size - 1, "net: connection %d round trip %u ms loss %u%% resends %u (round trips vary by %u ms; %u sent)",
		connection, c.roundTrip, NetStats_LossPercent(c.loss), c.packetsLost, c.roundTripVariance, c.packetsSent);

	words[(used < 0 || (size_t)used >= size) ? size - 1 : (size_t)used] = '\0';
}

// The every-10-seconds line for the whole machine: bytes in and out since the last one. ENet's totals are 32 bits and go round, and
// taking the old total away still gives the right difference when they have.
inline void NetStats_BytesLine(unsigned int inNow, unsigned int inBefore, unsigned int outNow, unsigned int outBefore, unsigned int milliseconds, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	int const used = _snprintf(words, size - 1, "net: in %u bytes, out %u bytes, in the last %u s", inNow - inBefore, outNow - outBefore, (milliseconds + 500) / 1000);
	words[(used < 0 || (size_t)used >= size) ? size - 1 : (size_t)used] = '\0';
}

// Is this connection struggling now? It was if wasStruggling: then it takes being under the "fine" numbers to stop.
inline bool NetStats_IsStruggling(bool wasStruggling, const NetStats_Connection &c)
{
	unsigned int const percent = NetStats_LossPercent(c.loss);

	if (wasStruggling)
	{
		return c.roundTrip > kNetStatsFineRoundTrip || percent > kNetStatsFinePercent;
	}

	return c.roundTrip > kNetStatsSlowRoundTrip || percent > kNetStatsLossyPercent;
}

// If the connection has just started or stopped struggling, writes the line that says so and gives back true. Otherwise writes nothing
// and gives back false. *struggling is set to what it is now.
inline bool NetStats_Change(int connection, bool wasStruggling, const NetStats_Connection &c, bool *struggling, char *words, size_t size)
{
	*struggling = NetStats_IsStruggling(wasStruggling, c);

	if (*struggling == wasStruggling || size == 0)
	{
		return false;
	}

	int const used = _snprintf(words, size - 1, *struggling ? "net: connection %d is struggling: round trip %u ms loss %u%%" : "net: connection %d is fine again: round trip %u ms loss %u%%",
		connection, c.roundTrip, NetStats_LossPercent(c.loss));

	words[(used < 0 || (size_t)used >= size) ? size - 1 : (size_t)used] = '\0';
	return true;
}

#endif
