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

// The list's name: the one built into the game, or, only with -dev, [Network] RelayServer from the settings, checked as a host name
// (Relay_IsServerName). Empty when there's none, and then there's no relay: hosting and joining are as they are without one.
//
// Both of these read their answer the first time they're asked, and keep it for the whole run. So they must never be asked before
// winmain.cpp has read -dev from the command line, or -dev would be missed for the run. Only the menus and hosting ask, long after;
// relay_list_test checks that winmain.cpp itself never does.
const char *Relay_ServerName(void);

// The list's HTTPS port: 443, or, only with -dev, [Network] RelayListPort, as a test on one machine needs. A setting that isn't a whole
// number from 1 to 65535 is written in the log, and 443 is used.
int Relay_ListPort(void);

// Added 2026-11-09 by Melanikus for the AvP port: a test server reached with nothing changed on the tester's computer. Only with -dev,
// read with the two above, and each kept for the whole run:
//   - RelayServer may be one plain IPv4 address (RelayList_ReadIPv4) instead of a name. Digits and dots that aren't one are refused.
//   - [Network] RelayListPin: the SHA-256 of the list's own certificate (RelayList_ReadPin). With a pin, every call to the list checks the
//     certificate it's given against the pin before a byte of the call is sent, and only then lets Windows pass a certificate it doesn't
//     know, or one for another name (relay_list.cpp, Call). A pin that isn't 64 hex digits means no list at all.
//   - With an address and a pin, every relay the list names is taken to be at that same address: a test server runs its list and its
//     relay on one machine, and the relay's name may be one no name service knows.
// Without -dev these settings are ignored, with a line in the log, and the list is called as it always was.
bool Relay_ListIsPinned(void);

// The name people see for the relay server, from -dev's [Network] RelayServerName (RelayList_IsShownName), for the online screens to show
// instead of a region's id; "" when there's none, or it isn't one. Like the region, the screen makes it safe before drawing it.
const char *Relay_ServerShownName(void);

// ---- A host's game. networking.cpp calls these. Nothing here keeps the session itself, only what the list shows of it.

// This run's identity's fingerprint (net_identity.h), which every listing carries so a joiner knows which host to expect. networking.cpp
// gives it once the identity is made, before the room opens: a room that opens before then isn't put on the list, and the log says why.
// The fingerprint is kept with each listing from the moment its room opens, so the list sees the same one for the listing's whole life.
// Added 2026-11-09 by Melanikus for the AvP port.
void RelayList_HostIdentity(const uint8_t fingerprint[kNetIdentityFingerprintBytes]);

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

// The game is quitting (winmain.cpp, just before the game shuts down). Added 2026-11-09 by Melanikus for the AvP port. A game this
// machine still has on the list is taken off it, and this waits at most mostMs for the list's thread to finish doing that: otherwise
// the game can be gone before the DELETE is sent, and the list goes on showing a game nobody can join until it forgets it by itself,
// 45 seconds after its last renewal (server #14's run, 2026-11-09). Nothing waits if nothing has to come off, or if the list was
// never used.
void RelayList_BeforeQuitting(unsigned int mostMs);

// ---- The relays: where each region's relay is

// Asks the list for its relays. The answer comes a moment later. The menus ask when the host page and the Find games page open.
void RelayList_FindRelays(void);

// The relays from the last good answer, into relays, which has room for kRelayListMostRelays. False while there's been no good answer.
// Each one's name has been looked up already, on the list's own thread, so the game's thread never waits for that: only the relays whose
// name gave an address that can be a relay's are here (RelayList_AddressRefusal), each with its address.
bool RelayList_Relays(RelayListRelay *relays, int *count);

// ---- Finding games. The menus call these.

// Asks the list for the games of this version of the game. The answer comes a moment later, through RelayList_FoundGames.
void RelayList_FindGames(void);

// Once for each answer: true, with the games, newest first (games has room for kRelayListMostGames). An answer that failed is true with
// no games, and why says why in words for the page, fixed words of the game's own, never the list's. False while there's no new answer.
bool RelayList_FoundGames(RelayListGame *games, int *count, const char **why);

// ---- Asking for online access (server #24): relay_list_text.h says what's sent and why it's safe. Added 2026-11-09 by Melanikus for the
// AvP port. Only the Online access page calls these, and that page is only there with -dev (the owner's ruling: nothing a release game
// does may reach a server yet), so these refuse without -dev as well, whatever calls them.

// Sends the request: the player's name, this PC's public halves, and the fingerprint the answer has to be about. Only when the player has
// pressed the page's row for it, or (PO's ruling) when the page opens after the player has sent one this run. False, with nothing sent,
// without -dev, with no list set, or if the name or key can't be sent (RelayList_AccessBody). The answer comes a moment later.
bool RelayList_AskForAccess(const char *name, const uint8_t statics[kNetIdentityStaticBytes], const uint8_t fingerprint[kNetIdentityFingerprintBytes]);

// Once for each answer: true, with answered true and what the list said, or answered false and why in fixed words of the game's own (never
// the list's). False while there's no new answer. The log gets only those words: never the name, the key, the fingerprint or the code.
bool RelayList_AccessCame(bool *answered, RelayAccessAnswer *answer, const char **why);
// Modified 2026-11-09 by Melanikus for the AvP port: for the page of games found's failure screen. Whether a why from RelayList_FoundGames
// means the game never got a connection to the list's server (it couldn't be reached, took too long, its certificate wasn't accepted, or
// the call couldn't start), rather than that it connected and couldn't fetch the games (an answer too big, a no, or an answer refused).
// It compares the why with this file's own words, never what anybody sent; NULL and "no relay server is set" are neither, so false.
bool RelayList_WhyIsNoConnection(const char *why);

#endif
