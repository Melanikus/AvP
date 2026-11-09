// How much has gone through the game's socket: packets and bytes, in and out, since the game's host was made, and how many a second lately.
// Written 2026-11-09 by Melanikus for the AvP port, for the relay's line in play (Fourth's online screens; the owner asked for packet
// counts there), through networking.cpp's Net_RelayCountsNow.
//
// WHERE THE NUMBERS COME FROM. ENet counts what it sends and what arrives at its socket (an ENetHost's totalSentPackets, totalSentData,
// totalReceivedPackets and totalReceivedData), and the relay's own small messages that arrive there are in those too, since ENet counts a
// packet before it shows it to networking.cpp. Two things go past ENet, and networking.cpp tells this about them itself:
//   - the relay's own messages the game sends (RelaySendAll sends them straight from the socket): NetTraffic_Sent;
//   - what the join through a relay reads before ENet connects (RelayHandshake reads the socket itself): NetTraffic_Received.
// ENet's totals are 32-bit, so they wrap round after 4 GB or 4 billion packets. Each look adds only what's new since the last, worked out
// in 32-bit arithmetic, so a wrap adds the right amount, and the totals here are 64-bit, so they don't wrap in any game.
//
// "LAST SECOND". Each look closes a window once a second has passed since the window opened, and gives what came in it, made a figure for
// one second (so a window that ran long, while a level loaded, gives an average, not a burst). The figures are 0 until the first window
// closes.
//
// It's game-free: the caller gives the numbers and the time. tests/net_traffic_test.cpp tries every case.
#ifndef NET_TRAFFIC_H
#define NET_TRAFFIC_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t const kNetTrafficWindowMs = 1000;

struct NetTraffic
{
	uint64_t packetsIn, packetsOut, bytesIn, bytesOut;                  // since the game's host was made
	uint32_t packetsInLastSecond, packetsOutLastSecond;                // in the last window, made a figure for one second
	uint32_t bytesInLastSecond, bytesOutLastSecond;

	// ENet's own totals at the last look, so each look adds only what's new
	uint32_t enetPacketsIn, enetPacketsOut, enetBytesIn, enetBytesOut;

	// The totals when the window being counted opened, and when that was
	uint64_t windowPacketsIn, windowPacketsOut, windowBytesIn, windowBytesOut;
	uint32_t windowOpenedAt;
	bool started;
};

// A new host: every count starts at 0, from ENet's totals as they are now (a new host's are 0, but nothing here takes that on trust)
inline void NetTraffic_Start(NetTraffic *t, uint32_t enetPacketsIn, uint32_t enetPacketsOut, uint32_t enetBytesIn, uint32_t enetBytesOut, uint32_t now)
{
	memset(t, 0, sizeof(*t));
	t->enetPacketsIn = enetPacketsIn;
	t->enetPacketsOut = enetPacketsOut;
	t->enetBytesIn = enetBytesIn;
	t->enetBytesOut = enetBytesOut;
	t->windowOpenedAt = now;
	t->started = true;
}

// ENet's totals now: what's new since the last look is added. Nothing before NetTraffic_Start.
inline void NetTraffic_FromENet(NetTraffic *t, uint32_t enetPacketsIn, uint32_t enetPacketsOut, uint32_t enetBytesIn, uint32_t enetBytesOut)
{
	if (!t->started)
	{
		return;
	}

	t->packetsIn += static_cast<uint32_t>(enetPacketsIn - t->enetPacketsIn);
	t->packetsOut += static_cast<uint32_t>(enetPacketsOut - t->enetPacketsOut);
	t->bytesIn += static_cast<uint32_t>(enetBytesIn - t->enetBytesIn);
	t->bytesOut += static_cast<uint32_t>(enetBytesOut - t->enetBytesOut);
	t->enetPacketsIn = enetPacketsIn;
	t->enetPacketsOut = enetPacketsOut;
	t->enetBytesIn = enetBytesIn;
	t->enetBytesOut = enetBytesOut;
}

// A datagram sent, or read, past ENet: one packet of that many bytes
inline void NetTraffic_Sent(NetTraffic *t, size_t bytes)
{
	if (t->started)
	{
		t->packetsOut++;
		t->bytesOut += bytes;
	}
}

inline void NetTraffic_Received(NetTraffic *t, size_t bytes)
{
	if (t->started)
	{
		t->packetsIn++;
		t->bytesIn += bytes;
	}
}

// A count over some milliseconds, as a figure for one second, at most what 32 bits hold
inline uint32_t NetTraffic_PerSecond(uint64_t count, uint32_t milliseconds)
{
	if (milliseconds == 0)
	{
		return 0;
	}

	// A count so big its sum would overflow 64 bits can only be shown as the most there is
	if (count > UINT64_MAX / kNetTrafficWindowMs)
	{
		return 0xFFFFFFFFu;
	}

	uint64_t const perSecond = count * kNetTrafficWindowMs / milliseconds;
	return perSecond > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>(perSecond);
}

// The time now: once a window has run a second or more, its figures are made, and the next window opens now
inline void NetTraffic_Tick(NetTraffic *t, uint32_t now)
{
	uint32_t const ran = now - t->windowOpenedAt;   // in 32-bit arithmetic, so the clock wrapping round (every 49 days) is still right

	if (ran < kNetTrafficWindowMs)
	{
		return;
	}

	t->packetsInLastSecond = NetTraffic_PerSecond(t->packetsIn - t->windowPacketsIn, ran);
	t->packetsOutLastSecond = NetTraffic_PerSecond(t->packetsOut - t->windowPacketsOut, ran);
	t->bytesInLastSecond = NetTraffic_PerSecond(t->bytesIn - t->windowBytesIn, ran);
	t->bytesOutLastSecond = NetTraffic_PerSecond(t->bytesOut - t->windowBytesOut, ran);
	t->windowPacketsIn = t->packetsIn;
	t->windowPacketsOut = t->packetsOut;
	t->windowBytesIn = t->bytesIn;
	t->windowBytesOut = t->bytesOut;
	t->windowOpenedAt = now;
}

#endif
