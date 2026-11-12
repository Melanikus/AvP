// The relay server's list, from the game's side: a host's game goes on it, the Find games page reads it, and the networking code finds
// each region's relay through it. Written 2026-11-08 by Melanikus for the AvP port.
//
// relay_shared.h says what the relay is. The list is HTTPS, made with WinHTTP, Windows' own, so the game needs no library of its own for
// it. Modified 2026-11-10 by Melanikus for the AvP port (#175): the server is a line of servers.cfg in the game's folder, its address and
// one fingerprint for both its keys (servers_cfg.h), and the list is trusted by that alone: relay_list.cpp's top says how. What goes back
// and forth is in relay_list_text.h, which reads the answers as hostile.
//
// Every call here returns at once. The HTTPS is done on a thread of its own, so the game never waits for the network, and what it says
// comes back to the game's own thread, through the calls below that say so. All of these are called from the game's main thread only.
//
// Nothing about a call is written to the log except whether it worked: never the list's name, a pass, an owner, a code or a game's name.

#ifndef RELAY_LIST_H
#define RELAY_LIST_H

#include "relay_shared.h"
#include "relay_list_text.h"
#include "servers_cfg.h"

struct SessionDescription;

// ---- A host's game. networking.cpp calls these. Nothing here keeps the session itself, only what the list shows of it.

// This run's identity's whole id (net_identity.h), which every listing carries so a joiner knows which host to expect. networking.cpp
// gives it once the identity is made, before the room opens: a room that opens before then isn't put on the list, and the log says why.
// The id is kept with each listing from the moment its room opens, so the list sees the same one for the listing's whole life.
// Added 2026-11-09 by Melanikus for the AvP port; the whole id, not its 16-byte fingerprint, since 2026-11-10 (SERVER-BLOCKERS item 6).
void RelayList_HostIdentity(const uint8_t hostId[kNetIdentityIdBytes]);

// The relay has given the host its room: the game goes on the list. region is the host's line's relay's, pass and passTime
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

// ---- The player's yes to a servers.cfg line (#175's last part; servers_trust.h says what it is and how it's kept). Added 2026-11-10 by
// Melanikus for the AvP port. Until a line has had it, nothing at all goes to its server or its relay: no call, no listing, no access
// request, no game found (each line's own yes since 2026-11-11: RelayList_LineToAgreeAt, below). A Hudson run needs none, as nobody is there to answer (HudsonRunIsOn, which
// tools/check-hudson-asleep.ps1 proves a player's game never sets). Only the main thread calls these.

// What the yes-screen shows of the line, so the player sees where it goes before anything does
struct RelayListLineToAgree
{
	char name[kServersCfgNameLetters + 1];   // as the line names it
	char address[kRelayListShownAddressBytes];   // "104.131.35.41", or with both ports when they aren't the usual ones: "203.0.113.7:8443:27500"
	                                             // (relay_list_text.h, RelayList_ShownAddress; modified 2026-11-10: 24 bytes could cut one)
	ServersCfgPlace place;                    // on the internet, on this PC, or on the player's own network
	char keyStart[9];                         // its fingerprint's first 8 digits ("bb0e2958"), and its last 8 ("f28d663e"), which a
	char keyEnd[9];                           // person can hold against what the server's owner gave out
};

// ---- Every servers.cfg line (#175 2b). Added 2026-11-10 by Melanikus for the AvP port. For the Online page and the Find games page: how
// many good lines there are (each one's own state is RelayList_LineNow, below), and every refused line by its number,
// so someone following an instruction can see what to fix. Only the main thread calls these, and none of them contacts anything: they
// read what servers.cfg said when it was read, once a run (relay_list.cpp, Server).
// (THE PICK, the player's choice of one line for the next start, went 2026-11-11: no line is "in use", every line is reached by its own
// place, and the host chooses where to host on the host page. REMOVED-CODE.md has the row.)

// How many good lines there are (0 to 64), in the file's order
int RelayList_LineCount(void);

// How many lines were refused, and one of them: its number in servers.cfg, and the game's own words for why (servers_cfg.h,
// ServersCfg_LineWords), never anything from the line. False, with lineNumber 0 and words NULL, for a place out of range.
int RelayList_RefusedLineCount(void);
bool RelayList_RefusedLineAt(int place, int *lineNumber, const char **words);

// Why no line of servers.cfg is used at all, in the game's own words: no file, a file that isn't one, one that couldn't be read, or one
// refused whole (too big, too many lines). NULL when the file was read, whatever its lines said.
const char *RelayList_FileWords(void);

// Whether there's no servers.cfg in the game's folder at all: the one case the Online page says in words of its own, "There's no
// servers.cfg in the game's folder, so there's no online play" (a release with online play off ships none: the owner's ruling R-GOAL-8,
// 2026-11-11). False whenever there's a file, good or not. Added 2026-11-11 by Melanikus for the AvP port.
bool RelayList_HasNoServersFile(void);

// Hudson's yespage= (hudson_plan.h), called only from Hudson's own files: from now on, this Hudson run's line needs its own Yes on the yes
// page, as a player's does, before the list starts. That yes is held for the run alone (RelayList_AgreeToTheLineAt). Added 2026-11-10 by
// Melanikus (Third) for the AvP port.
void HudsonTest_RelayListShowsTheYesPage(void);

// ---- Finding games. The menus call these.

// Asks the list for the games of this version of the game. The answers come a moment later.
// Modified 2026-11-11 by Melanikus for the AvP port (the owner's ruling: Find games lists every server servers.cfg names, "all servers go
// through the same pipeline"): it asks every good line the player has said yes to, one after another, in the file's order, each a visit
// of its own (relay_list.cpp, THE VISITS). A line without the yes isn't asked: nothing goes to its server. The answers come through
// RelayList_FoundAllGames and RelayList_LineNow once every visit has ended.
void RelayList_FindGames(void);

// ---- Every agreed line's games (added 2026-11-11 by Melanikus for the AvP port; the owner's ruling). For the page of games found.

// One game, and the good line it was listed on: its place among the good lines, as RelayList_LineNow numbers them. The page names the
// server by that line (RelayList_LineNow's line.name, made safe), never by anything the server sent. A game is joined through its own line
// (RelayList_RelayOf and RelayList_AskForRelayKeysOn with that place; modified 2026-11-11, one model for every line), once the player's
// access on that line is approved.
struct RelayListServerGame
{
	RelayListGame game;
	int line;
};

// (kRelayListMostVisits, the most good lines asked in one find, is in relay_list_text.h, with the choice of which: RelayList_ChooseVisits)
int const kRelayListMostAllGames = kRelayListMostVisits * kRelayListMostGames;    // the most games of all of them together

// Once for each find, after every visit has ended: true, with every agreed line's games, each tagged with its line, in the lines' order
// and each line's newest first. games has room for kRelayListMostAllGames. False while there's no new answer.
bool RelayList_FoundAllGames(RelayListServerGame *games, int *count);

// ---- ONE MODEL FOR EVERY LINE. Added 2026-11-11 by Melanikus for the AvP port (PO's design from the owner's rulings of that day: "They
// don't PICK a server, all authorized ones show up", "UNIFY THE UI", "they will choose their friends' server, not least ping", "No auto
// choice, they choose"). A line is its PLACE among servers.cfg's good lines: 0 to RelayList_LineCount() - 1, in the file's order, fixed for
// the whole run. Every line goes through the same code: the same yes, the same key check, the same calls, the same states, with no first
// line and no line "in use". Each call to a line's server is a visit of its own (relay_list.cpp, THE VISITS): its state made fresh from the
// line alone, its key checked against the line's fingerprint before anything else goes, the call made, its result copied out, everything
// of the visit wiped. Only the main thread calls these, and each returns at once; answers come a moment later.

// Where the player stands with a line's server, from its last answer this run (nothing is kept past the run: PO, 2026-11-11). These are
// the pages' meanings, not the list's own answers (relay_list_text.h's RelayAccessAnswer), which they're made from.
int const kRelayAccessNotAsked = 0;          // not asked this run
int const kRelayAccessAsking = 1;            // asked; no answer yet
int const kRelayAccessWaitingApproval = 2;   // the server has the request, and a person hasn't approved it yet
int const kRelayAccessIsApproved = 3;        // its games can be joined, and a game can be hosted on it
int const kRelayAccessIsRefused = 4;         // refused, or approved and then revoked: it stays so
int const kRelayAccessNoAnswer = 5;          // the request got no answer it could use: accessWhy says why

// One line, as the pages show it
struct RelayListLineNow
{
	RelayListLineToAgree line;   // its name, address, where it is and its key's two ends, as the yes page shows them
	int lineNumber;              // its number in servers.cfg, counted from 1
	bool ours;                   // one of the servers the game ships with, told from what the game holds, never from the file
	int state;                   // kRelayLine...: no yes, being asked, answered, no answer (why), or not asked (past kRelayListMostVisits)
	const char *why;             // kRelayLineNoAnswer's why, in the game's own fixed words; NULL otherwise
	int pingMs;                  // the last visit's round trip to its server, in ms, about one round trip; -1 when none was measured
	int access;                  // kRelayAccess...
	const char *accessWhy;       // kRelayAccessNoAnswer's why, fixed words of the game's own; NULL otherwise
	int games;                   // how many of the last find's games (RelayList_FoundAllGames) are its own
	// Its relay's region, from the line's last GOOD visit: the word its server gave with the relay's keys, read as a region (1 to 16 small
	// letters, digits and -) and kept only when those keys matched the line's fingerprint; "" when there's none. It's the server's own
	// word, so a page shows it made safe and as the server's; nothing chooses a relay by it (a relay is a line's: RelayList_RelayOf).
	// Added 2026-11-11 by Melanikus for the AvP port, for the host page.
	char region[kRelayListRegionLetters + 1];
};

// False, with out wiped, for a place out of range
bool RelayList_LineNow(int place, RelayListLineNow *out);

// The yes, per line. Every line needs it, the ones the game ships with too ("Every line asks the player once at first use", 2026-11-10).
// Kept in servers-trusted.cfg by the line's hash (its address, ports and fingerprint), never its name or place, so a line whose key
// changed asks again. LineToAgreeAt: true, with out filled, for a line without the yes. AgreeToTheLineAt: false, with nothing changed,
// for a place out of range; a yes that couldn't be kept holds for this run. A Hudson run's yes (yespage=) is held for the run alone.
bool RelayList_LineToAgreeAt(int place, RelayListLineToAgree *out);
bool RelayList_AgreeToTheLineAt(int place);

// Access, per line: the request goes to that line's server, after its yes, as a visit of its own. Only the page's own press sends one, or
// the page opening on a line this run has asked. The body is the same for every line (there's one identity). AccessCameOn: once for each
// answer, true with the line it was for, so the page can redraw that row; the answer itself is in RelayList_LineNow (access, accessWhy).
bool RelayList_AskForAccessOn(int place, const char *name, const uint8_t statics[kNetIdentityStaticBytes],
	const uint8_t fingerprint[kNetIdentityFingerprintBytes]);
bool RelayList_AccessCameOn(int *place);

// Before a new identity is made (the Online page's Make a new identity): every line's answer this run was about the old identity, so
// each is forgotten, every line is "not asked" again, and no line stays chosen to host on. False, with nothing changed, while any line's
// request is still on its way: its answer would come back about an identity this PC no longer has. Added 2026-11-11 by Melanikus for
// the AvP port.
bool RelayList_ForgetAccess(void);

// Joining any game, through ITS OWN line. RelayOf: that line's relay, its address (as ENetAddress.host keeps it) and relay port, straight
// from servers.cfg; nothing is looked up. AskForRelayKeysOn: that line's server's relay keys, proved by that line's fingerprint, on a
// visit to that line; with a region, only if it's that relay's own region; with NULL or "" (a host's asking), the region comes back with
// the keys. False, with nothing asked, for a place out of range, a line without the yes, or a word that isn't a region.
// RelayKeysCameOn: for the newest asking, waiting, the keys (with the line they're for and its relay's region), or refused with why.
bool RelayList_RelayOf(int place, uint32_t *address, int *port);
bool RelayList_AskForRelayKeysOn(int place, const char *region);
int RelayList_RelayKeysCameOn(int *place, char region[kRelayListRegionLetters + 1], uint8_t p384[kNetCryptoP384PublicBytes],
	uint8_t kem[kNetPqKemPublicBytes], const char **why);

// Hosting, on the line the PLAYER chose: no default, no automatic choice. ApprovedLines: the lines a host can choose, for the host page's
// "Host on:" row, in the file's order (each with its ping in RelayList_LineNow); 0 when there's none. HostOn: the player's choice, refused
// for a line that isn't approved. HostLine: the line the open room is on, or else the choice, or -1. A room keeps its line for its whole
// life; a choice made while it's open is for the next. Every listing call of the room (RelayList_HostRoomOpened and the rest) goes to that
// line's server, and its relay is RelayList_RelayOf(RelayList_HostLine()).
int RelayList_ApprovedLines(int *places, int most);
bool RelayList_HostOn(int place);
int RelayList_HostLine(void);

// Modified 2026-11-09 by Melanikus for the AvP port: for the page of games found's failure screen. Whether a why from RelayList_FoundGames
// means the game never got a connection to the list's server (it couldn't be reached, took too long, its secure connection failed, or
// the call couldn't start), rather than that it connected and couldn't fetch the games (an answer too big, a no, or an answer refused).
// It compares the why with this file's own words, never what anybody sent; NULL and "no relay server is set" are neither, so false.
bool RelayList_WhyIsNoConnection(const char *why);

// ---- A relay's keys (protocol version 2, step 7). Added 2026-11-09 by Melanikus for the AvP port. Asked of a line: RelayList_AskForRelayKeysOn.
//
// Handshake A proves a relay to the game only with the relay's real static public keys. Modified 2026-11-10 by Melanikus for the AvP port
// (#175): they're the keys the server's check took (GET /v1/relay-keys, CRYPTO.md 12.1), proved by the server's servers.cfg line together
// with the list's own key (net_server_fingerprint.h), before anything else is sent to the server.

// What RelayList_RelayKeysCameOn gives for the newest asking: waiting, the keys (into p384 and kem), or refused, with why in fixed words
// of the game's own. p384 and kem are all 0 unless the keys came.
int const kRelayKeysWaiting = 0;
int const kRelayKeysCame = 1;
int const kRelayKeysRefused = 2;

#endif
