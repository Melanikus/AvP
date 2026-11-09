// What the game's two halves of the relay share. Written 2026-11-08 by Melanikus for the AvP port.
//
// Many players can't be reached from the internet at all: their provider shares one address between many homes. So a game can go
// through a relay server instead, which carries its packets between the host and each joiner. The server has two parts:
// - the list, over HTTPS, where a host puts its game and a player picks one. The menus do that (relay_list.h).
// - the relay itself, over UDP, which carries the game. The networking code does that.
// PROTOCOL.md, in the server's own repository (github.com/Melanikus/AVPx-Dedicated-Server), is the whole of what the two say. The numbers
// here are its numbers.

#ifndef RELAY_SHARED_H
#define RELAY_SHARED_H

#include <stddef.h>
#include <stdint.h>

enum
{
	kRelayTokenBytes = 16,  // a host's proof that a room is its own: 16 random bytes the relay hands it
	kRelayCodeLetters = 8,  // a game's code, which a joiner picks from the list or types: 8 code letters
	kRelayMostPlayers = 8,  // the most players in one game the relay carries, the host included
	kRelayListPort = 443,   // the list's HTTPS
	kRelayControlPort = 27500 // where a host asks for a room and a joiner asks to join: the server's own default
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

// The relay server's name, from the settings: [Network] RelayServer in the game's settings file. Read once, and checked as a host name
// (Relay_IsServerName in relay_list_text.h). Empty when it isn't set, or isn't a host name, and then there's no relay: hosting and joining
// are as they were without one. The list and the relay are the same machine. relay_list.cpp has it.
const char *Relay_ServerName(void);

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
