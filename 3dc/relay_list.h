// The relay server's list, from the game's side: a host's game goes on it, the Find games page reads it, and the networking code finds
// each region's relay through it. Written 2026-11-08 by Melanikus for the AvP port.
//
// relay_shared.h says what the relay is. The list is HTTPS, made with WinHTTP, Windows' own, so the game needs no library of its own for
// it. Modified 2026-11-10 by Melanikus for the AvP port (#175): the server is a line of servers.cfg in the game's folder, its address and
// one fingerprint for both its keys (servers_cfg.h), and the list is trusted by that alone: relay_list.cpp's top says how. What goes back
// and forth is in relay_list_text.h, which reads the answers as hostile.
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

// The server's name, as people see it: its servers.cfg line's (servers_cfg.h). Empty when there's none, and then there's no relay: hosting
// and joining are as they are without one. Modified 2026-11-10 by Melanikus for the AvP port (#175): nothing in AliensVsPredator.cfg names a
// server any more. Modified again the same day (#175 PR 3b): -dev decides nothing here, so a player's game has the line as its server.
//
// servers.cfg is read the first time this is asked, and kept for the whole run. Only the menus and hosting ask, long after the game has
// started; relay_list_test checks that winmain.cpp itself never does, as it did when -dev was read with it. The screens make the name
// safe before drawing it, as they do anything from outside the game.
const char *Relay_ServerName(void);

// ---- A host's game. networking.cpp calls these. Nothing here keeps the session itself, only what the list shows of it.

// This run's identity's whole id (net_identity.h), which every listing carries so a joiner knows which host to expect. networking.cpp
// gives it once the identity is made, before the room opens: a room that opens before then isn't put on the list, and the log says why.
// The id is kept with each listing from the moment its room opens, so the list sees the same one for the listing's whole life.
// Added 2026-11-09 by Melanikus for the AvP port; the whole id, not its 16-byte fingerprint, since 2026-11-10 (SERVER-BLOCKERS item 6).
void RelayList_HostIdentity(const uint8_t hostId[kNetIdentityIdBytes]);

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

// ---- The relay: the server's own (modified 2026-11-10 by Melanikus for the AvP port, #175)

// Checks the server against its servers.cfg line, if it hasn't been yet (relay_list.cpp, TRUSTED BY ITS KEY ALONE). The answer comes a
// moment later. The menus ask when the host page and the Find games page open.
void RelayList_FindRelays(void);

// The server's one relay, into relays (room for kRelayListMostRelays): at its line's address and relay port, with the region the server's
// check gave. False while the server hasn't been checked and found good. Nothing is looked up by name.
bool RelayList_Relays(RelayListRelay *relays, int *count);

// Where the server stands (added 2026-11-10 by Melanikus for the AvP port, #188), for networking.cpp's Net_ServerState, so a host can wait
// for an answer that hasn't come, and is never told "no relay" for that alone. It only reads. why is the game's own fixed words.
int const kRelayServerAsking = 0;    // not asked yet, or being asked: no answer yet
int const kRelayServerGood = 1;      // checked against its servers.cfg line, and good: RelayList_Relays names its relay
int const kRelayServerCantUse = 2;   // why says why: the last asking couldn't reach it, it said no, its answer was refused, or its keys
                                     // don't match its line (asking again, RelayList_FindRelays, tries again, but not after keys that
                                     // didn't match: those stay refused until the game restarts)
int RelayList_ServerState(const char **why);

// ---- Finding games. The menus call these.

// Asks the list for the games of this version of the game. The answer comes a moment later, through RelayList_FoundGames.
void RelayList_FindGames(void);

// Once for each answer: true, with the games, newest first (games has room for kRelayListMostGames). An answer that failed is true with
// no games, and why says why in words for the page, fixed words of the game's own, never the list's. False while there's no new answer.
bool RelayList_FoundGames(RelayListGame *games, int *count, const char **why);

// ---- Asking for online access (server #24): relay_list_text.h says what's sent and why it's safe. Added 2026-11-09 by Melanikus for the
// AvP port. Only the Online access page calls these. (Modified 2026-11-10 by Melanikus for the AvP port, #175 PR 3b: they no longer ask
// -dev, which decides nothing online. The page has its own way in.)

// Sends the request: the player's name, this PC's public halves, and the fingerprint the answer has to be about. Only when the player has
// pressed the page's row for it, or (PO's ruling) when the page opens after the player has sent one this run. False, with nothing sent,
// with no fingerprint, with no list set, or if the name or key can't be sent (RelayList_AccessBody). The answer comes a moment later.
bool RelayList_AskForAccess(const char *name, const uint8_t statics[kNetIdentityStaticBytes], const uint8_t fingerprint[kNetIdentityFingerprintBytes]);

// Once for each answer: true, with answered true and what the list said, or answered false and why in fixed words of the game's own (never
// the list's). False while there's no new answer. The log gets only those words: never the name, the key, the fingerprint or the code.
bool RelayList_AccessCame(bool *answered, RelayAccessAnswer *answer, const char **why);
// Modified 2026-11-09 by Melanikus for the AvP port: for the page of games found's failure screen. Whether a why from RelayList_FoundGames
// means the game never got a connection to the list's server (it couldn't be reached, took too long, its secure connection failed, or
// the call couldn't start), rather than that it connected and couldn't fetch the games (an answer too big, a no, or an answer refused).
// It compares the why with this file's own words, never what anybody sent; NULL and "no relay server is set" are neither, so false.
bool RelayList_WhyIsNoConnection(const char *why);

// ---- A relay's keys, for protocol version 2's draft (step 7). Added 2026-11-09 by Melanikus for the AvP port.
//
// Handshake A proves a relay to the game only with the relay's real static public keys. Modified 2026-11-10 by Melanikus for the AvP port
// (#175): they're the keys the server's check took (GET /v1/relay-keys, CRYPTO.md 12.1), proved by the server's servers.cfg line together
// with the list's own key (net_server_fingerprint.h), before anything else is sent to the server.

// Asks for the keys of the server's relay, the one in region (RelayList_Relays gives it). The answer comes a moment later, through
// RelayList_RelayKeysCame: the keys, or a refusal if the server isn't checked and good, or region isn't its relay's. False if the region
// isn't one or the list can't start (no server is set).
bool RelayList_AskForRelayKeys(const char *region);

// What came for the newest asking: waiting, the keys (into p384 and kem), or refused, with why in fixed words of the game's own. p384 and
// kem are all 0 unless the keys came.
int const kRelayKeysWaiting = 0;
int const kRelayKeysCame = 1;
int const kRelayKeysRefused = 2;
int RelayList_RelayKeysCame(uint8_t p384[kNetCryptoP384PublicBytes], uint8_t kem[kNetPqKemPublicBytes], const char **why);

#endif
