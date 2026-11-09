// What the game's two halves of the relay share. Written 2026-11-08 by Melanikus for the AvP port.
//
// Many players can't be reached from the internet at all: their provider shares one address between many homes. So a game can go
// through a relay instead, which carries its packets between the host and each joiner. There are two parts:
// - the list, over HTTPS, where a host puts its game and a player picks one, and which says where each region's relay is. The menus do
//   that (relay_list.h).
// - a relay in each region, over UDP, which carries the game. The networking code does that.
// PROTOCOL.md, in the server's own repository (github.com/Melanikus/AVPx-Dedicated-Server), is the whole of what the two say. The numbers
// here are its numbers.

#ifndef RELAY_SHARED_H
#define RELAY_SHARED_H

#include <stddef.h>
#include <stdint.h>

enum
{
	kRelayCodeLetters = 8,  // a game's code, which a joiner picks from the list or types: 8 code letters
	kRelayMostPlayers = 8,  // the most players in one game the relay carries, the host included
	kRelayListPort = 443    // the list's HTTPS. Each relay's own port comes from the list.
};

// The letters a code is made of: 32 of them, with no 0, 1, I or O, so that a code can be read aloud without being mistaken
static const char kRelayCodeAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

inline bool Relay_IsCodeLetter(char c)
{
	for (size_t i = 0; kRelayCodeAlphabet[i]; i++)
	{
		if (kRelayCodeAlphabet[i] == c)
		{
			return true;
		}
	}

	return false;
}

// Whether these are exactly the 8 letters of a code. A code is never anything else, wherever it comes from.
inline bool Relay_IsCode(const char *code, size_t length)
{
	if (!code || length != kRelayCodeLetters)
	{
		return false;
	}

	for (size_t i = 0; i < length; i++)
	{
		if (!Relay_IsCodeLetter(code[i]))
		{
			return false;
		}
	}

	return true;
}

#endif
