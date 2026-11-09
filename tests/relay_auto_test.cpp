// The test of -dev -relayhost and -dev -relayjoin's deciding (3dc/relay_auto.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS: which of the two a command line asks for (whole words only, nothing without -dev, neither when both are asked), and which
// of the list's games a -relayjoin copy picks (the first of this version that isn't full and has a whole code; none past the list's limit).
// WHAT IT DOESN'T: the waiting itself (relay_auto.cpp), which needs the list's thread and the network; a run with Hudson tries that.

#include <stdio.h>
#include <string.h>

#include "relay_auto.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

int const kOurs = 113;

static RelayListGame AGame(int version, int players, int maxPlayers, const char *code)
{
	RelayListGame game;
	memset(&game, 0, sizeof(game));
	strcpy(game.region, "local");
	memcpy(game.code, code, strlen(code) + 1);
	strcpy(game.name, "HUDSON");
	game.version = version;
	game.players = players;
	game.maxPlayers = maxPlayers;
	return game;
}

static void TheWords()
{
	printf("Which one the command line asks for\n");
	Check(RelayAuto_FromCommandLine("-dev -relayhost", true) == kRelayAutoHost, "-dev -relayhost hosts through the relay");
	Check(RelayAuto_FromCommandLine("-dev -secondcopy -relayjoin", true) == kRelayAutoJoin, "-dev -secondcopy -relayjoin joins");
	Check(RelayAuto_FromCommandLine("-dev\t-relayjoin", true) == kRelayAutoJoin, "words apart by a tab count too");
	Check(RelayAuto_FromCommandLine("-relayhost", false) == kRelayAutoNone && RelayAuto_FromCommandLine("-relayjoin", false) == kRelayAutoNone,
		"without -dev, neither does anything");
	Check(RelayAuto_FromCommandLine("-dev -relayhost -relayjoin", true) == kRelayAutoNone, "both at once is neither");
	Check(RelayAuto_FromCommandLine("-dev -relayhosts", true) == kRelayAutoNone && RelayAuto_FromCommandLine("-dev x-relayjoin", true) == kRelayAutoNone,
		"only whole words: -relayhosts and x-relayjoin are neither");
	Check(RelayAuto_FromCommandLine(NULL, true) == kRelayAutoNone && RelayAuto_FromCommandLine("", true) == kRelayAutoNone && RelayAuto_FromCommandLine("-dev", true) == kRelayAutoNone,
		"no command line, an empty one, or -dev alone: neither");
}

static void ThePick()
{
	printf("Which of the list's games a -relayjoin copy joins\n");
	RelayListGame games[kRelayListMostGames + 1];

	games[0] = AGame(kOurs, 1, 4, "ABCD2345");
	Check(RelayAuto_PickGame(games, 1, kOurs) == 0, "a game of this version with room: that one");

	games[0] = AGame(kOurs, 1, 4, "ABCD2345");
	games[1] = AGame(kOurs, 1, 4, "ABCD2346");
	Check(RelayAuto_PickGame(games, 2, kOurs) == 0, "of two, the first: the list gives the newest first");

	games[0] = AGame(112, 1, 4, "ABCD2345");
	games[1] = AGame(kOurs, 1, 4, "ABCD2346");
	Check(RelayAuto_PickGame(games, 2, kOurs) == 1, "another version is passed over, even first");

	games[0] = AGame(kOurs, 4, 4, "ABCD2345");
	Check(RelayAuto_PickGame(games, 1, kOurs) == -1, "a full game is passed over");
	games[0] = AGame(kOurs, -1, 4, "ABCD2345");
	Check(RelayAuto_PickGame(games, 1, kOurs) == -1, "a game saying it has fewer than no players is passed over");
	games[0] = AGame(kOurs, 0, 0, "ABCD2345");
	Check(RelayAuto_PickGame(games, 1, kOurs) == -1, "a game with room for nobody is passed over");

	games[0] = AGame(kOurs, 1, 4, "ABCD234");
	Check(RelayAuto_PickGame(games, 1, kOurs) == -1, "a code of 7 letters is passed over");
	games[0] = AGame(kOurs, 1, 4, "ABCD2345");
	games[0].code[kRelayCodeLetters] = 'X';
	Check(RelayAuto_PickGame(games, 1, kOurs) == -1, "a code with no end after its 8 letters is passed over");

	Check(RelayAuto_PickGame(games, 0, kOurs) == -1 && RelayAuto_PickGame(NULL, 1, kOurs) == -1 && RelayAuto_PickGame(games, -1, kOurs) == -1,
		"no games, no list, or a count below 0: none");

	// One more than the list's limit, with the only good one last: it mustn't be looked at
	for (int i = 0; i < kRelayListMostGames; i++)
	{
		games[i] = AGame(112, 1, 4, "ABCD2345");
	}

	games[kRelayListMostGames] = AGame(kOurs, 1, 4, "ABCD2346");
	Check(RelayAuto_PickGame(games, kRelayListMostGames + 1, kOurs) == -1, "a count past the list's limit: none, and nothing past the limit is read");
	Check(RelayAuto_PickGame(games, kRelayListMostGames, kOurs) == -1, "and at the limit, the ones within it are looked at as usual");
}

int main()
{
	TheWords();
	ThePick();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
