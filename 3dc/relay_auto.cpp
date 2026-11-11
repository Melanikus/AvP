// -dev -relayhost and -dev -relayjoin: the waiting (relay_auto.h says what they're for and the rules). Written 2026-11-09 by Melanikus for
// the AvP port.
//
// Each wait asks the list (relay_list.cpp, on its own thread), then looks for the answer every kRelayAutoWaitMs, answering Windows'
// messages in between so the window never shows "Not responding", until the answer has come or the wait's limit has passed. A copy that's
// being closed stops waiting at once.

// The game's own headers first, as networking.cpp has them: menus.h needs them (and 3dc.h brings in windows.h, for GetTickCount and Sleep)
#include "3dc.h"

#include <string.h>
#include <string>

#include "relay_auto.h"
#include "relay_list.h"
#include "networking.h"
#include "menus.h"
#include "logString.h"

void CheckForWindowsMessages(void);   // win95/platform.h
extern bool bRunning;                 // winmain.cpp: false once the game is being closed

namespace
{
	int mode = kRelayAutoNone;

	// Windows' messages, while a wait goes on. False once the game is being closed, which ends the wait.
	bool KeepAnswering(void)
	{
		CheckForWindowsMessages();
		return bRunning;
	}
}

void RelayAuto_ReadCommandLine(const char *commandLine, bool developer)
{
	mode = RelayAuto_FromCommandLine(commandLine, developer);
	bool const host = CommandLineHasWord(commandLine, "-relayhost") || CommandLineHasWord(commandLine, "-askjoiners");
	bool const join = CommandLineHasWord(commandLine, "-relayjoin");

	// Modified 2026-11-09 by Melanikus for the AvP port: the lines say what the words do, and no more. A host copy hosts by itself as the
	// menus start (menus.cpp, HostByItselfThroughTheRelay), or in a Hudson run, as Hudson starts. A joiner copy joins by itself only in a
	// Hudson run; otherwise someone joins from the games page. (A copy started by hand with -relayhost once sat at the menus while its log
	// said it hosted: it didn't host by itself then.)
	if (mode == kRelayAutoHost && RelayAuto_AsksJoiners(commandLine, developer))
	{
		LogString("relay auto: -dev -askjoiners: this copy hosts by itself through the relay, and asks the person at it about each joiner");
	}
	else if (mode == kRelayAutoHost)
	{
		LogString("relay auto: -dev -relayhost: this copy hosts by itself through the relay, and lets every joiner in without asking");
		Net_LetEveryJoinerIn(true);
	}
	else if (mode == kRelayAutoJoin)
	{
		LogString("relay auto: -dev -relayjoin: a Hudson run joins the first game of its version on the relays' list by itself; otherwise join from the games page");
	}
	else if ((host || join) && !developer)
	{
		LogString("relay auto: -relayhost, -askjoiners and -relayjoin are for tests, and need -dev, so this copy does none of them");
	}
	else if (host && join)
	{
		LogString("relay auto: hosting (-relayhost or -askjoiners) and joining (-relayjoin) can't both be done, so this copy does neither");
	}
}

bool RelayAuto_HostsByItselfNow(void)
{
	static bool hosted = false;

	if (mode != kRelayAutoHost || hosted)
	{
		return false;
	}

	hosted = true;
	return true;
}

int RelayAuto_Mode(void)
{
	return mode;
}

// Modified 2026-11-11 by Melanikus for the AvP port (one model for every line): a run's host needs a line to host on, as a player
// chooses one on the Host on row. Nothing chooses for a player; a run is a test of the game, and it chooses the first line that has
// approved its PC, saying so in its log, once the list's visits have said which those are.
bool RelayAuto_WaitForRelays(void)
{
	LogString("relay auto: asking every line for its state, to host on one that has approved this copy");
	RelayList_FindGames();

	DWORD const started = GetTickCount();
	int places[kServersCfgMostLines];

	while (GetTickCount() - started < kRelayAutoRelaysWithinMs && KeepAnswering())
	{
		int const approved = RelayList_ApprovedLines(places, kServersCfgMostLines);

		if (approved > 0 && RelayList_HostOn(places[0]))
		{
			LogString("relay auto: this run hosts on servers.cfg's line " + std::to_string(places[0] + 1) + " of the good ones, the first that approved it");
			return true;
		}

		Sleep(kRelayAutoWaitMs);
	}

	LogString("relay auto: no line approved this copy in time, so it hosts nothing");
	return false;
}

int RelayAuto_FindAGame(void)
{
	// Modified 2026-11-11 by Melanikus for the AvP port (one model for every line): every line's games, and the game is joined through its
	// own line (its relay comes from that line, so there's nothing more to wait for once the game has come)
	LogString("relay auto: asking every line for games of this version");
	RelayList_FindGames();

	DWORD const started = GetTickCount();
	DWORD askedAt = started;
	static RelayListServerGame all[kRelayListMostAllGames];   // kept off the stack
	static RelayListGame games[kRelayListMostAllGames];
	const char *lastWhy = NULL;

	while (GetTickCount() - started < kRelayAutoGameWithinMs && KeepAnswering())
	{
		int count = 0;

		if (RelayList_FoundAllGames(all, &count))
		{
			int const kept = count < 0 ? 0 : count > kRelayListMostAllGames ? kRelayListMostAllGames : count;

			for (int i = 0; i < kept; i++)
			{
				games[i] = all[i].game;
			}

			int const picked = RelayAuto_PickGame(games, kept, kMultiplayerVersion);

			if (picked >= 0)
			{
				int const line = all[picked].line;

				if (!Net_AddRelaySession(games[picked], line))
				{
					LogString("relay auto: the list's first game of this version couldn't go on the list of sessions");
					return -1;
				}

				int const session = Net_FindRelaySession(line, games[picked].region, games[picked].code);
				LogString(session >= 0 ? "relay auto: a line has a game of this version, so this copy joins it through that line's relay"
					: "relay auto: the list's game went on the list of sessions, but can't be found there");
				return session;
			}

			// No game of this version: each line's state says why, once each time it changes, in the list code's own words
			for (int place = 0; place < RelayList_LineCount(); place++)
			{
				RelayListLineNow now;

				if (RelayList_LineNow(place, &now) && now.state == kRelayLineNoAnswer && now.why && now.why != lastWhy)
				{
					LogString(std::string("relay auto: a line's answer couldn't be used: ") + now.why);
					lastWhy = now.why;
				}
			}
		}

		// Nothing yet, or nothing of this version: the host may still be opening its room, so every line is asked again
		if (GetTickCount() - askedAt >= kRelayAutoAskAgainMs)
		{
			RelayList_FindGames();
			askedAt = GetTickCount();
		}

		Sleep(kRelayAutoWaitMs);
	}

	LogString("relay auto: no game of this version came from any line in time");
	return -1;
}

bool RelayAuto_Join(int session, char *playerName)
{
	if (session < 0 || session >= MAX_NO_OF_SESSIONS || !playerName)
	{
		return false;
	}

	LogString("relay auto: joining through the relay");
	Net_SetRelayJoinWaitHook(KeepAnswering);
	bool const joined = Net_ConnectToSession(session, playerName) != NET_FAIL;
	Net_SetRelayJoinWaitHook(NULL);
	LogString(joined ? "relay auto: joined through the relay" : "relay auto: the join through the relay failed (the line before says why)");
	return joined;
}
