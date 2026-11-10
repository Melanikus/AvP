// The test of -dev -relayhost and -dev -relayjoin's deciding (3dc/relay_auto.h). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS: which of the two a command line asks for (whole words only, nothing without -dev, neither when both are asked), and which
// of the list's games a -relayjoin copy picks (the first of this version that isn't full and has a whole code; none past the list's limit).
// WHAT IT DOESN'T: the waiting itself (relay_auto.cpp), which needs the list's thread and the network; a run with Hudson tries that.

#include <stdio.h>
#include <string.h>

#include <string>

#include "relay_auto.h"
#include "source_slice.h"

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

	// Modified 2026-11-09 by Melanikus for the AvP port: -askjoiners, a host that asks the person at it about each joiner
	Check(RelayAuto_FromCommandLine("-dev -askjoiners", true) == kRelayAutoHost && RelayAuto_AsksJoiners("-dev -askjoiners", true),
		"-dev -askjoiners hosts too, and asks about each joiner");
	Check(!RelayAuto_AsksJoiners("-dev -relayhost", true), "-dev -relayhost doesn't ask: it lets every joiner in");
	Check(RelayAuto_FromCommandLine("-dev -relayhost -askjoiners", true) == kRelayAutoHost && RelayAuto_AsksJoiners("-dev -relayhost -askjoiners", true),
		"with both host words, it's a host that asks");
	Check(RelayAuto_FromCommandLine("-dev -askjoiners -relayjoin", true) == kRelayAutoNone && !RelayAuto_AsksJoiners("-dev -askjoiners -relayjoin", true),
		"hosting and joining at once is neither, and asks nothing");
	Check(RelayAuto_FromCommandLine("-askjoiners", false) == kRelayAutoNone && !RelayAuto_AsksJoiners("-askjoiners", false), "without -dev, -askjoiners does nothing");
	Check(RelayAuto_FromCommandLine("-dev -askjoinerss", true) == kRelayAutoNone && !RelayAuto_AsksJoiners("-dev -askjoinerss", true), "and only the whole word counts");
}

// Modified 2026-11-09 by Melanikus for the AvP port: a host copy hosts by itself as the menus start (menus.cpp), which needs the whole game
// to run, so that part is read, not run. The file is read from the top of the repository, as the tests run there.
static std::string ReadSource(const char *name)
{
	std::string text;
	FILE *file = fopen(name, "rb");

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static bool Has(const std::string &text, const char *piece)
{
	return text.find(piece) != std::string::npos;
}

static void HostingByItself()
{
	printf("A host copy hosting by itself as the menus start (read, not run: it needs the whole game)\n");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const autoCode = ReadSource("3dc/relay_auto.cpp");
	Check(menus.size() > 100000 && autoCode.size() > 1000, "menus.cpp and relay_auto.cpp were found and read, from the top of the repository");

	std::string const hostItself = SourceSlice(menus, "static bool HostByItselfThroughTheRelay(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const once = hostItself.find("if (!RelayAuto_HostsByItselfNow())");
	size_t const hosts = hostItself.find("Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, netGameData.levelNumber)");
	Check(once != std::string::npos && hosts != std::string::npos && once < hosts, "it hosts only when relay_auto says so, once, and with the call the host page's Start row makes");
	Check(Has(hostItself, "netGameData.skirmishMode = FALSE;") && Has(hostItself, "netGameData.gameType = NGT_Individual;") && Has(hostItself, "netGameData.levelNumber = 0;"),
		"a deathmatch on the first level of the multiplayer list, never the game's skirmish, which has no network");
	Check(Has(hostItself, "RelayAuto_WaitForRelays()") && hostItself.find("RelayAuto_WaitForRelays()") < hosts, "it waits for the list's relays before it hosts, so it hosts through one");
	Check(Has(hostItself, "AvPMenus.MenusState = MENUSSTATE_STARTGAME;\n\tSetLevelToLoadForThisNetworkGame();\n\tSetBriefingTextForMultiplayer();"),
		"and then starts the game as the Start row does");
	Check(!Has(hostItself, "CommandLine") && !Has(hostItself, "command_line") && !Has(hostItself, "SaveMultiplayerConfiguration"),
		"nothing in it reads the command line, and it doesn't save over the player's previous game");

	// Called once the menus are set up, and before their loop, so the loop doesn't run and the game starts
	std::string const mainMenus = SourceSlice(menus, "int AvP_MainMenus(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const call = mainMenus.find("\tHostByItselfThroughTheRelay();\n");
	size_t const loop = mainMenus.find("while (AvPMenus.MenusState == MENUSSTATE_MAINMENUS && bRunning)");
	size_t const setUp = mainMenus.find("SetupNewMenu(AVPMENU_MAIN);");
	Check(call != std::string::npos && loop != std::string::npos && setUp != std::string::npos && setUp < call && call < loop,
		"the menus ask it after they're set up, and before their loop");

	// -relayhost lets every joiner in; -askjoiners doesn't
	std::string const reading = SourceSlice(autoCode, "void RelayAuto_ReadCommandLine(const char *commandLine, bool developer)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const asking = SourceSlice(reading, "if (mode == kRelayAutoHost && RelayAuto_AsksJoiners(commandLine, developer))", "\telse if (mode == kRelayAutoHost)", kSliceEndAfterTheStart, FailedSlice);
	std::string const lettingIn = SourceSlice(reading, "\telse if (mode == kRelayAutoHost)", "\telse if (mode == kRelayAutoJoin)", kSliceEndAfterTheStart, FailedSlice);
	Check(!asking.empty() && !Has(asking, "Net_LetEveryJoinerIn"), "-askjoiners leaves the request to join asking");
	Check(Has(lettingIn, "Net_LetEveryJoinerIn(true);"), "and -relayhost lets every joiner in");

	std::string const once2 = SourceSlice(autoCode, "bool RelayAuto_HostsByItselfNow(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(once2, "if (mode != kRelayAutoHost || hosted)") && Has(once2, "hosted = true;"), "relay_auto says to host by itself once only, and only for a host copy");
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
	HostingByItself();

	if (checksFailed > 0)
	{
		printf("%d check%s FAILED\n", checksFailed, checksFailed == 1 ? "" : "s");
		return 1;
	}

	printf("Every check passed\n");
	return 0;
}
