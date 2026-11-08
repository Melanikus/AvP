// The test of what a player is told during a co-op campaign. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the co-op
// campaign system follows Bobblen's avp-forever, rewritten).
//
// WHAT IT TESTS
//
// 3dc/coop_in_game.h: who's alive, who the loading screen is waiting for, and why a game ended. The list of who's being waited for comes from
// another machine, so it's tried with lies: places with nobody in them, the player's own place, and a name with no end.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/coop_in_game_test.args, and the program gives back a non-zero result if any check
// failed. It reads some of the game's own source files too. tests/takeouts/coop_in_game_test.json proves each check can fail.

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "coop_in_game.h"
#include "memory_wall.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void TestWhoIsAlive(void)
{
	puts("Who's in and alive:");

	Check(CoopPlayer_State(true, false, true) == kCoopPlayerAlive, "in the level and alive");
	Check(CoopPlayer_State(true, false, false) == kCoopPlayerOut, "in the level and dead is out until the next level");
	Check(CoopPlayer_State(false, false, false) == kCoopPlayerJoining, "not in the level yet is joining, not out");
	Check(CoopPlayer_State(false, false, true) == kCoopPlayerJoining, "and that goes for one still marked alive");
	Check(CoopPlayer_State(false, true, true) == kCoopPlayerAlive, "the host never presses in, because it starts the game, and it's never joining");
	Check(CoopPlayer_State(false, true, false) == kCoopPlayerOut, "a host who dies is out like anyone");
}

static void TestTheHostsList(void)
{
	puts("The host's list of who's being waited for:");

	bool hasPlayer[kCoopMostPlayers] = { true, true, true, false, false, false, false, true };
	bool isReady[kCoopMostPlayers] = { false, true, false, true, false, false, false, false };

	Check(CoopWaiting_Places(hasPlayer, isReady, kCoopMostPlayers, 0, true) == ((1u << 2) | (1u << 7)), "each player who hasn't pressed, and nobody who has");
	Check(CoopWaiting_Places(hasPlayer, isReady, kCoopMostPlayers, 0, false) == (1u | (1u << 2) | (1u << 7)), "the host itself, until its own key is pressed");
	Check(CoopWaiting_Places(hasPlayer, isReady, kCoopMostPlayers, 2, true) == (1u | (1u << 7)), "the host's own key is what counts for the host's place, not its row");
	Check((CoopWaiting_Places(hasPlayer, isReady, kCoopMostPlayers, 0, true) & (1u << 3)) == 0, "a place with nobody in it is never on the list, ready or not");
	Check(CoopWaiting_Places(hasPlayer, isReady, 2, 0, true) == 0, "it stops at the places it's given");

	bool manyHavePlayers[kCoopMostPlayers + 4];
	bool manyAreReady[kCoopMostPlayers + 4];

	memset(manyHavePlayers, 1, sizeof(manyHavePlayers));
	memset(manyAreReady, 0, sizeof(manyAreReady));

	Check(CoopWaiting_Places(manyHavePlayers, manyAreReady, kCoopMostPlayers + 4, -1, true) == 0xff, "and past the last bit it has, it stops");

	// The game's own lists have exactly as many places as the list has bits. This one ends where readable memory does, so if the walk went on
	// past its last place, as it would if it only stopped at the number it's given, it would read past the end and the test would crash.
	bool *const wallHasPlayers = (bool *)MemoryEndingAtAWall(kCoopMostPlayers * sizeof(bool));

	memset(wallHasPlayers, 1, kCoopMostPlayers * sizeof(bool));

	Check(CoopWaiting_Places(wallHasPlayers, manyAreReady, kCoopMostPlayers + 4, -1, true) == 0xff, "and it never reads a place past the last bit");
}

static std::string Waiting(unsigned char places, int myPlace, size_t room = 64, const char *const *names = NULL, size_t nameBytes = 13)
{
	static const char *const everyone[kCoopMostPlayers] = { "Ripley", "Hicks", NULL, "Vasquez", NULL, NULL, NULL, "Bishop" };

	char line[128];

	memset(line, 'x', sizeof(line));

	bool const said = CoopWaiting_Line(line, room, "Waiting for:", places, names ? names : everyone, nameBytes, myPlace);

	Check(memchr(line, 0, room) != NULL, "the line has its end within its room");
	Check(room >= sizeof(line) || line[room] == 'x', "and nothing was written past the room");
	Check(said == (line[0] != 0), "and it says it has a line exactly when it has");

	return line;
}

static void TestTheLine(void)
{
	puts("The Waiting for line:");

	Check(Waiting(1u << 1, 0) == "Waiting for: Hicks", "one name");
	Check(Waiting((1u << 1) | (1u << 3), 0) == "Waiting for: Hicks, Vasquez", "two, in order, with a comma between");
	Check(Waiting(0, 0).empty(), "nobody to wait for is no line at all");
	Check(Waiting(1u << 0, 0).empty(), "a player never waits for themselves");
	Check(Waiting((1u << 0) | (1u << 1), 0) == "Waiting for: Hicks", "themselves is left out of a longer list");
	Check(Waiting(1u << 2, 0).empty(), "a place with nobody in it names nobody");
	Check(Waiting(0xff, -1) == "Waiting for: Ripley, Hicks, Vasquez, Bishop", "a list with every bit set only names the players there are");
	Check(Waiting(0xff, 7) == "Waiting for: Ripley, Hicks, Vasquez", "the last place is looked at too");

	Check(Waiting(1u << 1, 0, 10).empty() == false, "a line too long for its room is cut, not lost");
	Check(Waiting(1u << 1, 0, 10) == "Waiting f", "and it's cut to fit");
	Check(Waiting((1u << 1) | (1u << 3), 0, 16) == "Waiting for: Hi", "wherever the room runs out");

	// A name that fills its bytes and has no end: the letters after it must not be read
	char noEnd[20];

	memset(noEnd, 'N', sizeof(noEnd));
	noEnd[sizeof(noEnd) - 1] = 0;

	const char *const named[kCoopMostPlayers] = { NULL, noEnd, NULL, NULL, NULL, NULL, NULL, NULL };

	Check(Waiting(1u << 1, 0, 64, named, 13) == "Waiting for: NNNNNNNNNNNNN", "a name with no end stops at the bytes a name has");

	char empty[4] = "abc";

	Check(CoopWaiting_Line(empty, 0, "Waiting for:", 1u << 1, named, 13, 0) == false, "no room at all is no line");
	Check(strcmp(empty, "abc") == 0, "and nothing is written");
}

static void TestTheLastWord(void)
{
	puts("Why a game ended:");

	Check(CoopLastWord_AtTheEnd(false, false, false, false, true) == kCoopLastWordNone, "any other kind of game keeps what it did");
	Check(CoopLastWord_AtTheEnd(false, false, true, true, true) == kCoopLastWordNone, "whatever else is true of it");
	Check(CoopLastWord_AtTheEnd(true, false, false, false, false) == kCoopLastWordNone, "a player who left by themselves isn't told anything");
	Check(CoopLastWord_AtTheEnd(true, false, false, false, true) == kCoopLastWordHostLeft, "the host leaving");
	Check(CoopLastWord_AtTheEnd(true, false, false, true, true) == kCoopLastWordRemoved, "being removed closes the connection too, and it's not the host leaving");
	Check(CoopLastWord_AtTheEnd(true, false, true, false, false) == kCoopLastWordCampaignComplete, "the last level finished");
	Check(CoopLastWord_AtTheEnd(true, true, true, false, false) == kCoopLastWordMapComplete, "a custom map has its own words");
	Check(CoopLastWord_AtTheEnd(true, false, true, false, true) == kCoopLastWordCampaignComplete, "a finished campaign stays finished if the host then leaves");
	Check(CoopLastWord_AtTheEnd(true, true, false, false, true) == kCoopLastWordHostLeft, "a custom map not finished is the host leaving");
	Check(CoopLastWord_AtTheEnd(true, false, true, true, false) == kCoopLastWordRemoved, "and someone removed at the last moment was still removed");
}

// A source file, with the returns taken out, so a line can be looked for the same way whatever the file's line ends are
static std::string ReadSourceFile(const std::string &name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream all;

	all << file.rdbuf();

	std::string const text = all.str();
	std::string withoutReturns;

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] != '\r')
		{
			withoutReturns += text[i];
		}
	}

	if (withoutReturns.empty())
	{
		printf("  FAILED  the test couldn't read %s\n", name.c_str());
		checksFailed++;
	}

	return withoutReturns;
}

static bool Has(const std::string &text, const std::string &what)
{
	return text.find(what) != std::string::npos;
}

// The part of a file between two pieces of text, or nothing
static std::string Between(const std::string &text, const std::string &from, const std::string &to)
{
	size_t const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	size_t const end = text.find(to, start + from.size());

	return end == std::string::npos ? "" : text.substr(start, end - start);
}

static void TestTheGameUsesIt(void)
{
	puts("The game's own files:");

	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	std::string const pages = ReadSourceFile("3dc/avp/win95/frontend/coop_menu.cpp");
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const data = ReadSourceFile("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const loading = ReadSourceFile("3dc/avp/win95/progress_bar.cpp");

	Check(Has(pldnet, "static_assert(kCoopMostPlayers == NET_MAXPLAYERS,"), "the list has a bit for every player, or the game doesn't build");
	Check(Has(pldnet, "netGameData.coopWaitingFor = CoopWaiting_Places(placeHasPlayer, coopPlaceIsReady, NET_MAXPLAYERS, PlayerIdInPlayerList(AvPNetID), coopThisPlayerPressed);"), "the host works out who's being waited for, its own key included");
	Check(Has(pldnet, "messagePtr->coopWaitingFor = netGameData.coopWaitingFor;"), "and says so in the game description");

	std::string const campaignBlock = Between(pldnet, "// Any number at all is safe here: see shared_roll.h", "\n\t\t}\n");

	Check(Has(campaignBlock, "netGameData.coopWaitingFor = messagePtr->coopWaitingFor;"), "a joiner takes it only where a campaign's other numbers are taken, outside the lobby");
	Check(Has(pldnet, "return CoopWaiting_Line(line, room, GetTextString(TEXTSTRING_COOP_WAITING_FOR), netGameData.coopWaitingFor, names, NET_PLAYERNAMELENGTH, PlayerIdInPlayerList(AvPNetID));"), "the line is made by the rule, which never waits for this player");
	Check(Has(pldnet, "if (!coopThisPlayerPressed)\n\t{\n\t\treturn false;\n\t}\n\n\tconst char *names[NET_MAXPLAYERS];"), "and only once this player has pressed a key");
	Check(Has(pldnet, "coopFinishedPlace = place == netGameData.coopCampaignLevel + 1 ? netGameData.coopCampaignLevel : -1;"), "only the level after a finished one says Level complete");
	Check(Has(pldnet, "if (SameAsSinglePlayer() && netGameData.myGameState != NGS_EndGameScreen)\n\t\t{\n\t\t\tCoop_ShowWhoIsAlive();"), "in a campaign the score key shows who's alive");
	Check(Has(loading, "if (startsTogether)\n\t\t{\n\t\t\tchar line[160];"), "the loading screen's lines are only for a game that starts together");

	Check(Has(pages, "CoopPlayer_State(player.startFlag != 0, player.playerId == host, player.playerAlive != 0);"), "the list knows which player is the host");
	Check(Has(pages, "NetID const host = AvP.Network == I_Host ? AvPNetID : Net_HostPlayerID();"), "on the host's machine and on a joiner's");
	Check(Has(pages, "_snprintf(line, room - 1, \"%s: %s\", name, GetTextString(words));"), "a player's name is printed as text, never as a format");
	Check(Has(pages, "memcpy(name, player.name, NET_PLAYERNAMELENGTH);\n\tname[NET_PLAYERNAMELENGTH] = 0;"), "and copied to its room first, so a name with no end stops there");
	Check(Has(pages, "lastWord = CoopLastWord_AtTheEnd(campaign != kCampaignNone, campaign == kCampaignCustom, Coop_TheCampaignIsOver(), Net_WasRemovedByTheHost(), hostHasGone);"), "why a game ended comes from the rule");
	Check(Has(pages, "\t// Said once: the next game starts with nothing to say\n\tlastWord = kCoopLastWordNone;"), "and is said once");
	Check(Has(pages, "if (AvP.Network == I_Host)\n\t{\n\t\tquit->ElementID = AVPMENU_ELEMENT_GOTOMENU;\n\t\tquit->MenuToGoTo = AVPMENU_COOP_HOSTQUITS;"), "a host is asked before leaving ends the game for everyone");
	Check(Has(pages, "\tif (AvP.Network == I_Host)\n\t{\n\t\tchanged = HostRemoveRows(&AvPMenu_CoopPlayers[rows], playerRemoveRowsShown);"), "only the host's Players page has Remove rows");

	// Both have to be found first: a search that finds nothing gives back the largest number there is, which would come "after" anything
	size_t const gameEndNoted = winmain.find("CoopMenu_TheGameIsEnding();");
	size_t const gameWiped = winmain.find("EndAVPNetGame();\n\t\t\t//EndOfNetworkGameScreen();");

	Check(gameEndNoted != std::string::npos && gameWiped != std::string::npos && gameEndNoted < gameWiped, "why a game ended is noted before the game is wiped");
	Check(Has(menus, "if (CoopMenu_HasALastWord())\n\t{"), "and the page that says so comes first after the game");
	Check(Has(menus, "menuID = SameAsSinglePlayer() ? AVPMENU_INCOOPGAME : AVPMENU_INNETGAME;"), "a campaign's pause menu is its own, and every other network game's is as it was");

	std::string const hostQuits = Between(data, "AvPMenu_CoopHostQuits[] =", "};");

	Check(hostQuits.find("TEXTSTRING_NO") < hostQuits.find("TEXTSTRING_YES"), "No comes first on the host's question, so a key pressed by mistake doesn't end the game");
}

int main()
{
	TestWhoIsAlive();
	TestTheHostsList();
	TestTheLine();
	TestTheLastWord();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
