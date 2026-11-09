// The relay server's list, from the game's side: a host's game goes on it, and the Find games page reads it. Written 2026-11-08 by
// Melanikus for the AvP port.
//
// relay_shared.h says what the relay is. The list is HTTPS, made with WinHTTP, Windows' own, so the game needs no library of its own for
// it, and Windows checks the server's certificate the way it does for any program. The server's name is one setting (Relay_ServerName).
// What goes back and forth is in relay_list_text.h, which reads the answers as hostile.
//
// Every call here returns at once. The HTTPS is done on a thread of its own, so the game never waits for the network, and what it says
// comes back to the game's own thread through RelayList_FoundGames. All of these are called from the game's main thread only.
//
// Nothing about a call is written to the log except whether it worked: never the server's name, a token, a code or a game's name.

#ifndef RELAY_LIST_H
#define RELAY_LIST_H

#include "relay_shared.h"
#include "relay_list_text.h"

struct SessionDescription;

// ---- A host's game. networking.cpp calls these, with its session as it is at that moment. Nothing here keeps the session itself.

// The relay has given the host its room: the game goes on the list
void RelayList_HostRoomOpened(const uint8_t token[kRelayTokenBytes], const char code[kRelayCodeLetters], const SessionDescription &session);

// Something the list shows may have changed: who's playing, or the level. Called as often as networking.cpp likes. The list is told
// only when what it shows has really changed.
void RelayList_HostSessionNow(const SessionDescription &session);

// The room has ended: the game comes off the list
void RelayList_HostRoomClosed(void);

// ---- Finding games. The menus call these.

// Asks the list for the games of this version of the game. The answer comes a moment later, through RelayList_FoundGames.
void RelayList_FindGames(void);

// Once for each answer: true, with the games, newest first (games has room for kRelayListMostGames). An answer that failed is true with
// no games, and why says why in words for the page, fixed words of the game's own, never the server's. False while there's no new answer.
bool RelayList_FoundGames(RelayListGame *games, int *count, const char **why);

#endif
