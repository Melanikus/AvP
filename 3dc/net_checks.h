// Checks on what arrives from other players' machines. Written 2026-11-06 by Melanikus for the AvP port.
//
// Anyone on the network can send anything, so what's in a message is only something someone said until it's been checked. These are kept apart from the game's network code, and need none of its headers, so that a test can run them on their own.

#ifndef NET_CHECKS_H
#define NET_CHECKS_H

#include <stddef.h>
#include <string.h>

// Is this number a place in a table with this many places? The first place is 0.
inline bool NetNumberFitsTable(int number, int places)
{
	return number >= 0 && number < places;
}

// For a log line that any peer or LAN machine can cause once per packet or datagram: counts the event in count and
// says whether to log this one. The first kDropsLoggedInFull are logged, then only the 8th, 16th, 32nd, ... (each
// power of two), so a flood adds a few dozen lines to avp_log.txt at most, each with the running total.
static const unsigned long long kDropsLoggedInFull = 5;

inline bool LogThisDrop(unsigned long long &count)
{
	count++;
	return (count <= kDropsLoggedInFull) || ((count & (count - 1)) == 0);
}

// A player's name, from one fixed place to another. Nothing says the one that arrived ends, and the game treats a name as ending, so this makes sure of it.
inline void NetCopyPlayerName(char *to, const char *from, size_t size)
{
	memcpy(to, from, size);
	to[size - 1] = '\0';
}

#endif
