// The relay server's list, from the game's side: a host's game goes on it, the Find games page reads it, and the networking code finds
// each region's relay through it. Written 2026-11-08 by Melanikus for the AvP port.
//
// relay_shared.h says what the relay is. The list is HTTPS, made with WinHTTP, Windows' own, so the game needs no library of its own for
// it, and Windows checks the list's certificate the way it does for any program. The list's name is one setting (Relay_ServerName).
// What goes back and forth is in relay_list_text.h, which reads the answers as hostile.
//
// Every call here returns at once. The HTTPS is done on a thread of its own, so the game never waits for the network, and what it says
// comes back to the game's own thread, through RelayList_Relays and RelayList_FoundGames. All of these are called from the game's main
// thread only.
//
// Nothing about a call is written to the log except whether it worked: never the list's name, a pass, an owner, a code or a game's name.

#ifndef RELAY_LIST_H
#define RELAY_LIST_H

#include "relay_shared.h"
#include "relay_list_text.h"

struct SessionDescription;

// The list's name, from the settings: [Network] RelayServer in the game's settings file. Read once, and checked as a host name
// (Relay_IsServerName). Empty when it isn't set, or isn't a host name, and then there's no relay: hosting and joining are as they are
// without one.
const char *Relay_ServerName(void);

// The list's HTTPS port: [Network] RelayListPort, 443 unless the settings say otherwise, as a test on one machine does. A setting that
// isn't a whole number from 1 to 65535 is written in the log, and 443 is used.
int Relay_ListPort(void);

// ---- A host's game. networking.cpp calls these. Nothing here keeps the session itself, only what the list shows of it.

// The relay has given the host its room: the game goes on the list. region is the relay's (from RelayList_Relays), pass and passTime
// what HOST_OPENED gave, and hostMs the host's round trip to that relay, 0 if it wasn't measured.
void RelayList_HostRoomOpened(const char *region, const char code[kRelayCodeLetters], const uint8_t pass[kRelayListSecretBytes], uint64_t passTime,
	int hostMs, const SessionDescription &session);

// Each HOST_KEPT that brings a newer pass: the listing is renewed with it, which the list needs every 15 seconds
void RelayList_HostKept(const uint8_t pass[kRelayListSecretBytes], uint64_t passTime, int hostMs);

// Something the list shows may have changed: who's playing, or the level. Called as often as networking.cpp likes. The list is told
// only when what it shows has really changed.
void RelayList_HostSessionNow(const SessionDescription &session);

// The room has ended: the game comes off the list
void RelayList_HostRoomClosed(void);

// ---- The relays: where each region's relay is

// Asks the list for its relays. The answer comes a moment later. The menus ask when the host page and the Find games page open.
void RelayList_FindRelays(void);

// The relays from the last good answer, into relays, which has room for kRelayListMostRelays. False while there's been no good answer.
bool RelayList_Relays(RelayListRelay *relays, int *count);

// ---- Finding games. The menus call these.

// Asks the list for the games of this version of the game. The answer comes a moment later, through RelayList_FoundGames.
void RelayList_FindGames(void);

// Once for each answer: true, with the games, newest first (games has room for kRelayListMostGames). An answer that failed is true with
// no games, and why says why in words for the page, fixed words of the game's own, never the list's. False while there's no new answer.
bool RelayList_FoundGames(RelayListGame *games, int *count, const char **why);

#endif
