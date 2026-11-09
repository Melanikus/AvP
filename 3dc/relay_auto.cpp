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
	bool const host = CommandLineHasWord(commandLine, "-relayhost");
	bool const join = CommandLineHasWord(commandLine, "-relayjoin");

	if (mode == kRelayAutoHost)
	{
		LogString("relay auto: this copy hosts through the relay (-dev -relayhost), and as a test's host it lets every joiner in without asking");
		Net_LetEveryJoinerIn(true);
	}
	else if (mode == kRelayAutoJoin)
	{
		LogString("relay auto: this copy joins the first game of its version on the relays' list (-dev -relayjoin)");
	}
	else if ((host || join) && !developer)
	{
		LogString("relay auto: -relayhost and -relayjoin are for tests, and need -dev, so this copy does neither");
	}
	else if (host && join)
	{
		LogString("relay auto: -relayhost and -relayjoin can't both be done, so this copy does neither");
	}
}

int RelayAuto_Mode(void)
{
	return mode;
}

bool RelayAuto_WaitForRelays(void)
{
	LogString("relay auto: asking the list for the relays");
	RelayList_FindRelays();

	DWORD const started = GetTickCount();
	RelayListRelay relays[kRelayListMostRelays];
	int count = 0;

	while (GetTickCount() - started < kRelayAutoRelaysWithinMs && KeepAnswering())
	{
		if (RelayList_Relays(relays, &count) && count > 0)
		{
			LogString("relay auto: the list named " + std::to_string(count) + " relay(s), so this copy hosts through the nearest");
			return true;
		}

		Sleep(kRelayAutoWaitMs);
	}

	LogString("relay auto: no relay came from the list in time, so this copy hosts on this network only");
	return false;
}

int RelayAuto_FindAGame(void)
{
	LogString("relay auto: asking the list for games of this version");
	RelayList_FindRelays();
	RelayList_FindGames();

	DWORD const started = GetTickCount();
	DWORD askedAt = started;
	static RelayListGame games[kRelayListMostGames];   // kept off the stack: 100 games are about 25 KB
	RelayListGame chosen = {};                         // the game picked, kept while its region's relay is waited for
	bool haveChosen = false;
	const char *lastWhy = NULL;

	while (GetTickCount() - started < kRelayAutoGameWithinMs && KeepAnswering())
	{
		int count = 0;
		const char *why = NULL;

		if (!haveChosen && RelayList_FoundGames(games, &count, &why))
		{
			int const picked = why ? -1 : RelayAuto_PickGame(games, count, kMultiplayerVersion);

			// The list's answer was refused: its words are the game's own (relay_list.h), said once each time they change
			if (why && why != lastWhy)
			{
				LogString(std::string("relay auto: the list's answer couldn't be used: ") + why);
				lastWhy = why;
			}

			if (picked >= 0)
			{
				chosen = games[picked];
				haveChosen = true;
				LogString("relay auto: the list has a game of this version");
			}
		}

		// The relays come in an answer of their own, which may come after the games' one. The join needs its game's region's relay, so
		// the game is joined only once that's known (RelayJoin finds it the same way).
		if (haveChosen)
		{
			RelayListRelay relays[kRelayListMostRelays];
			int relayCount = 0;
			bool regionKnown = false;

			if (RelayList_Relays(relays, &relayCount))
			{
				for (int i = 0; i < relayCount && i < static_cast<int>(kRelayListMostRelays); i++)
				{
					regionKnown = regionKnown || strncmp(relays[i].region, chosen.region, sizeof(relays[i].region)) == 0;
				}
			}

			if (regionKnown)
			{
				if (!Net_AddRelaySession(chosen))
				{
					LogString("relay auto: the list's first game of this version couldn't go on the list of sessions");
					return -1;
				}

				int const session = Net_FindRelaySession(chosen.region, chosen.code);
				LogString(session >= 0 ? "relay auto: its relay is known too, so this copy joins it"
					: "relay auto: the list's game went on the list of sessions, but can't be found there");
				return session;
			}
		}

		// Nothing yet, or nothing of this version: the host may still be opening its room, so the list is asked again. Once there's a game,
		// it's the relays that are asked for again.
		if (GetTickCount() - askedAt >= kRelayAutoAskAgainMs)
		{
			if (haveChosen)
			{
				RelayList_FindRelays();
			}
			else
			{
				RelayList_FindGames();
			}

			askedAt = GetTickCount();
		}

		Sleep(kRelayAutoWaitMs);
	}

	LogString(haveChosen ? "relay auto: the list's game came, but its region's relay didn't, in time" : "relay auto: no game of this version came from the list in time");
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
