// What the game's two halves of the relay share. Written 2026-11-08 by Melanikus for the AvP port.
//
// Many players can't be reached from the internet at all: their provider shares one address between many homes. So a game can go
// through a relay instead, which carries its packets between the host and each joiner. There are two parts:
// - the list, over HTTPS, where a host puts its game and a player picks one, and which says where each region's relay is. The menus do
//   that (relay_list.h).
// - a relay in each region, over UDP, which carries the game. The networking code does that (net_relay.h).
// PROTOCOL.md, in the server's own repository (github.com/Melanikus/AVPx-Dedicated-Server), is the whole of what the two say. The numbers
// here are its numbers.
//
// A code's letters and length are net_relay.h's: there is one copy of them, so the list and the relay can never disagree about what a code
// is. The names here are only the list side's names for them.

#ifndef RELAY_SHARED_H
#define RELAY_SHARED_H

#include <stddef.h>
#include <stdint.h>

#include "net_relay.h"

enum
{
	kRelayCodeLetters = static_cast<int>(kNetRelayCodeLetters),  // a game's code, which a joiner picks from the list: 8 code letters
	kRelayMostPlayers = 8   // the most players in one game the relay carries, the host included
};
// (Removed 2026-11-10 by Melanikus for the AvP port, #175: kRelayListPort. The list's port is its server's servers.cfg line's,
// kServersCfgListPort in servers_cfg.h unless the line gives one.)

// The letters a code is made of: 32 of them, with no 0, 1, I or O, so that a code can be read aloud without being mistaken
static const char *const kRelayCodeAlphabet = kNetRelayCodeAlphabet;

// Whether these are exactly the 8 letters of a code. A code is never anything else, wherever it comes from.
inline bool Relay_IsCode(const char *code, size_t length)
{
	return code && length == kNetRelayCodeLetters && NetRelay_CodeIsGood(code);
}

#endif
