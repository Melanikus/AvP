// -dev -relayhost and -dev -relayjoin: a copy of the game that hosts, or joins, through the relay on its own, for tests with nobody at the
// menus (server card #14: two Hudsons in one co-op game through the relay on one PC). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT THEY DO.
//   - -relayhost: the copy hosts by itself. Before it hosts, it asks the list for the relays and waits (at most
//     kRelayAutoRelaysWithinMs) until they've come, so Net_HostGame opens the game through the nearest relay. The real menus ask when the
//     multiplayer page opens; a copy that hosts by itself never opens that page. It lets every joiner in without asking.
//     In a Hudson run, Hudson hosts as it starts. Otherwise (added 2026-11-09), the copy hosts as the menus start, once, doing what the host
//     page's Start row does with that page's answers fixed (menus.cpp, HostByItselfThroughTheRelay).
//   - -askjoiners (added 2026-11-09): the same as -relayhost, but the person at the copy is asked about each joiner (the request to join's
//     yes/no rows), so that can be filmed with nobody's hands needed for anything else.
//   - -relayjoin: in a Hudson run, instead of joining a game on this machine, the copy asks the list for games of its own version, waits
//     (at most kRelayAutoGameWithinMs) for one, puts the first on its list of sessions (Net_AddRelaySession), and joins it through its
//     relay, as the page of games found does when a player picks one. Without Hudson it does nothing, and someone joins from that page.
//
// RULES (AVP Tao 3: the command line comes from outside):
//   - Only with -dev. Without it the words do nothing, and the log says so.
//   - Words only: nothing on the command line is read as a value. Which game to join is the list's first for this version; which level
//     to host is the test's own (Hudson's), or else the first of the multiplayer list.
//   - Both words together are neither: the log says so.
//   - Every wait is bounded, the window keeps answering while it waits, and every step writes one line of fixed words. No line writes an
//     address, a code or a region.
//
// The deciding is here, game-free, so tests/relay_auto_test.cpp can try every case. relay_auto.cpp does the waiting.

#ifndef RELAY_AUTO_H
#define RELAY_AUTO_H

#include <stdint.h>

#include "command_line_word.h"
#include "relay_list_text.h"

int const kRelayAutoNone = 0;   // neither: the copy hosts and joins as it always has
int const kRelayAutoHost = 1;   // -dev -relayhost
int const kRelayAutoJoin = 2;   // -dev -relayjoin

uint32_t const kRelayAutoRelaysWithinMs = 15000;   // the most a host waits for the list's relays
uint32_t const kRelayAutoGameWithinMs = 60000;     // the most a joiner waits for a game of its version on the list
uint32_t const kRelayAutoAskAgainMs = 3000;        // a joiner asks the list for its games again this often, until one comes
uint32_t const kRelayAutoWaitMs = 50;              // how long each wait sleeps between looks

// What the command line asks for, read as whole words (CommandLineHasWord). Nothing without -dev, and neither if it asks for both.
// Modified 2026-11-09 by Melanikus for the AvP port: -askjoiners asks for a host too (RelayAuto_AsksJoiners).
inline int RelayAuto_FromCommandLine(const char *commandLine, bool developer)
{
	bool const host = CommandLineHasWord(commandLine, "-relayhost") || CommandLineHasWord(commandLine, "-askjoiners");
	bool const join = CommandLineHasWord(commandLine, "-relayjoin");

	if (!developer || host == join)
	{
		return kRelayAutoNone;
	}

	return host ? kRelayAutoHost : kRelayAutoJoin;
}

// Added 2026-11-09 by Melanikus for the AvP port: -dev -askjoiners is a host as -dev -relayhost is, but it asks the person at it about each
// joiner (the request to join's yes/no rows), where -relayhost lets every joiner in without asking. That's so the rows can be filmed.
inline bool RelayAuto_AsksJoiners(const char *commandLine, bool developer)
{
	return RelayAuto_FromCommandLine(commandLine, developer) == kRelayAutoHost && CommandLineHasWord(commandLine, "-askjoiners");
}

// Which of the list's games a -relayjoin copy joins: the first (the list gives the newest first) that's this game's network version,
// isn't full, and has a code of 8 letters. -1 if none. The list asks only for games of this version, so the version is checked again
// here, as anything from outside is.
inline int RelayAuto_PickGame(const RelayListGame *games, int count, int ourVersion)
{
	if (!games || count < 0 || count > kRelayListMostGames)
	{
		return -1;
	}

	for (int i = 0; i < count; i++)
	{
		const RelayListGame &game = games[i];
		bool codeIsGood = true;

		for (int letter = 0; letter < kRelayCodeLetters; letter++)
		{
			codeIsGood = codeIsGood && game.code[letter] != 0;
		}

		codeIsGood = codeIsGood && game.code[kRelayCodeLetters] == 0;

		if (game.version == ourVersion && game.players >= 0 && game.players < game.maxPlayers && codeIsGood)
		{
			return i;
		}
	}

	return -1;
}

// ---- The waiting, in relay_auto.cpp. All on the game's main thread.

// Reads -relayhost and -relayjoin, once -dev has been read (winmain.cpp), and says in the log what this copy will do
void RelayAuto_ReadCommandLine(const char *commandLine, bool developer);

// kRelayAutoNone, kRelayAutoHost or kRelayAutoJoin
int RelayAuto_Mode(void);

// Added 2026-11-09 by Melanikus for the AvP port: whether a host copy (-dev -relayhost or -dev -askjoiners) should host by itself now, as
// the menus start, with nobody at them (menus.cpp, HostByItselfThroughTheRelay). True once only: after its game, the copy waits at the
// menus like any other. A Hudson run hosts in its own way, before the menus would ask.
bool RelayAuto_HostsByItselfNow(void);

// A -relayhost copy, before it hosts: asks the list for the relays and waits until they've come. False if none came in time, and then
// the copy hosts on this network only, as it would with no relay at all.
bool RelayAuto_WaitForRelays(void);

// A -relayjoin copy: asks the list for games of its version, waits for one, and puts the first on the list of sessions. Its index there,
// for RelayAuto_Join, or -1 if none came in time.
int RelayAuto_FindAGame(void);

// Joins that session through its relay, keeping the window answering while the relay's handshake waits (Net_SetRelayJoinWaitHook). The
// same answer as Net_ConnectToSession: true if it connected.
bool RelayAuto_Join(int session, char *playerName);

#endif
