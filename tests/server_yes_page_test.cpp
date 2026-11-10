// The test of the yes page: the player's yes to the server in servers.cfg, before anything goes to it (#175's last part). Written 2026-11-10
// by Melanikus for the AvP port.
//
// WHAT IT'S FOR: until the player says yes, nothing at all may go to a line's server (relay_list.h), and the page is where they answer.
// So a press must mean what its row says, the yes must be kept before the page goes on, a no must send nothing, and what the page shows
// must be the line's and the game's own words. The page's rules (server_yes_rules.h) are tried here directly. The page, the menus and the
// list need the whole game, so how they're put together is read in the code. relay_list_test reads the list's own side of the gate.
//
// Nothing here uses the network or the disk, besides reading the game's source files.

#include <stdio.h>
#include <string.h>
#include <string>

#include "server_yes_rules.h"
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

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

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

static bool Has(const std::string &text, const char *part)
{
	return text.find(part) != std::string::npos;
}

static size_t CountOf(const std::string &text, const char *part)
{
	size_t count = 0;

	for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
	{
		count++;
	}

	return count;
}

static void TestTheRows()
{
	printf("1. what each row means\n");
	Check(ServerYes_Choice(kServerYesRowYes) == kServerYesAgreeAndGoOn, "Yes, use this server keeps the yes and goes on");
	Check(ServerYes_Choice(kServerYesRowNo) == kServerYesGoBack, "No, not now goes back");
	Check(ServerYes_Choice(0) == kServerYesNothing && ServerYes_Choice(99) == kServerYesNothing && ServerYes_Choice(102) == kServerYesNothing &&
		ServerYes_Choice(-1) == kServerYesNothing, "and any other row means nothing");
	Check(kServerYesRowNo > 3 && kServerYesRowYes > 3, "the rows' numbers are far from the access page's own (0 to 3)");
}

static void TestTheLines()
{
	printf("2. the lines the page shows\n");
	char line[64];
	ServerYes_WhereLine("104.131.35.41", "on the internet", line, sizeof(line));
	Check(strcmp(line, "104.131.35.41, on the internet") == 0, "the address, then where it is");
	ServerYes_WhereLine("203.0.113.7:8443:27500", "on your own network", line, sizeof(line));
	Check(strcmp(line, "203.0.113.7:8443:27500, on your own network") == 0, "with both ports, when the line names them");
	ServerYes_KeyLine("key", "bb0e2958", "f28d663e", line, sizeof(line));
	Check(strcmp(line, "key bb0e2958 ... f28d663e") == 0, "the key's first 8 digits and its last 8, three dots between");

	char small[8];
	ServerYes_WhereLine("104.131.35.41", "on the internet", small, sizeof(small));
	Check(strcmp(small, "104.131") == 0, "a line too long for its room is cut short, and still ended");
	ServerYes_KeyLine(NULL, NULL, NULL, line, sizeof(line));
	Check(strcmp(line, "  ... ") == 0, "and nothing at all is written as nothing, never read from nowhere");
	ServerYes_WhereLine("x", "y", NULL, 10);
	Check(true, "nowhere to write is no harm");
}

static void TestTheWiring()
{
	printf("3. how the page is put together, read in the code\n");
	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/server_yes_page.cpp");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	Check(data.size() > 1000 && menus.size() > 1000 && page.size() > 1000 && words.size() > 1000, "the page's, the menus' and the words' sources were found and read");

	std::string const rows = SourceSlice(data, "static AVPMENU_ELEMENT AvPMenu_ServerYes[]=", "};", kSliceEndAfterTheStart, FailedSlice);
	size_t const no = rows.find("{TEXTSTRING_SERVERYES_NO},\t{kServerYesRowNo}");
	size_t const yes = rows.find("{TEXTSTRING_SERVERYES_YES},\t{kServerYesRowYes}");
	Check(no != std::string::npos && yes != std::string::npos && no < yes && rows.find("AVPMENU_ELEMENT_ENDOFMENU") > yes,
		"the page has its two rows, No first, so an Enter pressed by chance sends nothing");
	Check(Has(data, "{AVPMENU_FONT_SMALL,\tTEXTSTRING_SERVERYES_TITLE,\t\t\tAvPMenu_ServerYes,\t\t\t\t\tAVPMENU_MULTIPLAYER, 0},") &&
		Has(data, "== AVPMENU_SERVER_YES + 1,"), "its title is the question, and Esc goes back to the Multiplayer page");

	std::string const opening = SourceSlice(menus, "case AVPMENU_SERVER_YES:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opening, "if (!ServerYesPage_Opened())\n\t\t\t{\n\t\t\t\tSetupNewMenu(AVPMENU_MULTIPLAYER);\n\t\t\t\treturn;"),
		"a page with no line waiting for a yes goes back to the Multiplayer page");
	Check(Has(menus, "AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)\n\t{\n\t\tServerYesPage_Draw();"), "the page's words are drawn on it");
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_ONLINE_ACCESS:\n\t\t{\n\t\t\tenum AVPMENU_ID page = AVPMENU_ONLINE_ACCESS;", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	size_t const ours = pressing.find("if (AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)");
	Check(ours != std::string::npos && ours < pressing.find("AccessPage_Pressed(") &&
		Has(pressing, "ServerYesPage_Pressed(elementPtr->MaxValue, &page)"), "its rows are pressed as its own, never as the access page's");

	std::string const pressed = SourceSlice(page, "bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const agreeing = SourceSlice(pressed, "case kServerYesAgreeAndGoOn:", "case kServerYesGoBack:", kSliceEndAfterTheStart, FailedSlice);
	std::string const goingBack = SourceSlice(pressed, "case kServerYesGoBack:", "case kServerYesNothing:", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(agreeing, "if (!RelayList_AgreeToTheLine())") && agreeing.find("RelayList_AgreeToTheLine") < agreeing.find("*page = goToAfterYes;"),
		"a yes is kept before the page goes on, and a yes that couldn't be given goes back instead");
	Check(Has(goingBack, "*page = backToAfterNo;") && !Has(goingBack, "RelayList_"), "a no goes back, and tells the list nothing");

	std::string const needed = SourceSlice(page, "bool ServerYesPage_Needed(enum AVPMENU_ID goTo, enum AVPMENU_ID backTo)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(needed, "if (!RelayList_LineToAgree(&waiting))\n\t{\n\t\treturn false;"), "the page is needed only while the list has a line waiting for a yes");
	std::string const drawing = SourceSlice(page, "void ServerYesPage_Draw(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(drawing, "OnlineScreens_SafeName(shown.name, sizeof(shown.name), name, sizeof(name));") &&
		drawing.find("OnlineScreens_SafeName") < drawing.find("RenderMenuText(name"), "the server's name, from a file from outside the game, is made safe before it's drawn");

	size_t const question = words.find("\"Play online through this server?\",\n\t\"on the internet\",\n\t\"on this PC\",\n\t\"on your own network\",\n\t\"key\",\n");
	Check(question != std::string::npos && words.find("\"Its owner approves their own players, and sees your name, your key and your address while you play.\",\n\t\"Yes, use this server\",\n\t\"No, not now\",\n") > question,
		"the words are PO's, in langenum.h's order");
}

// THE WAYS ONLINE (PO's rulings, 2026-11-10): the yes is asked as a way's row is pressed, never as a page opens; a host asks only when
// it hosts online; after a No, Find games goes on to its page with this network's games only, the others back to the Multiplayer page,
// and nothing goes to the server. The rules are tried here; how the menus use them is read in the code. Added 2026-11-10 by Melanikus for
// the AvP port.
static void TestTheWaysOnline()
{
	printf("4. the ways online: which ask, and where a No goes\n");
	Check(ServerYes_WayAsks(kServerYesWayAccess, false) && ServerYes_WayAsks(kServerYesWayAccess, true),
		"the Online access page always asks: it would send the request to the server");
	Check(ServerYes_WayAsks(kServerYesWayFindGames, false) && ServerYes_WayAsks(kServerYesWayFindGames, true),
		"Find games always asks: it would read the server's list");
	Check(ServerYes_WayAsks(kServerYesWayHost, true) && !ServerYes_WayAsks(kServerYesWayHost, false),
		"a host asks only when it hosts online: a game at home uses no server");
	Check(!ServerYes_WayAsks(kServerYesNotAWay, true) && !ServerYes_WayAsks(kServerYesNotAWay, false), "any other row never asks");
	Check(ServerYes_NoGoesOn(kServerYesWayFindGames) && !ServerYes_NoGoesOn(kServerYesWayAccess) && !ServerYes_NoGoesOn(kServerYesWayHost) &&
		!ServerYes_NoGoesOn(kServerYesNotAWay), "after a No only Find games goes on to its page; the others go back");

	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/server_yes_page.cpp");
	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const list = ReadSource("3dc/relay_list.cpp");
	Check(menus.size() > 1000 && page.size() > 1000 && data.size() > 1000 && list.size() > 1000, "the menus', the page's, the rows' and the list's sources were found and read");

	// Which rows are the ways: by the page they're on and the page they go to
	std::string const wayOf = SourceSlice(page, "static ServerYesWay WayOf(enum AVPMENU_ID from, enum AVPMENU_ID goTo)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(wayOf, "if (from == AVPMENU_MULTIPLAYER && goTo == AVPMENU_ONLINE_ACCESS)\n\t{\n\t\treturn kServerYesWayAccess;"),
		"the Multiplayer page's row to Online access is a way online");
	Check(Has(wayOf, "if (from == AVPMENU_MULTIPLAYER && (goTo == AVPMENU_MULTIPLAYER_CONFIG || goTo == AVPMENU_COOP_HOST))\n\t{\n\t\treturn kServerYesWayHost;"),
		"so are its two host rows, Start game and Co-op campaign");
	Check(Has(wayOf, "if (from == AVPMENU_MULTIPLAYERJOINGAME && goTo == AVPMENU_MULTIPLAYERSELECTSESSION)\n\t{\n\t\treturn kServerYesWayFindGames;"),
		"and the Join game page's Find games, never the typed address's row, which uses no server");
	Check(Has(data, "static AVPMENU_ELEMENT const OnlineAccessRow = {AVPMENU_ELEMENT_GOTOMENU,\t{TEXTSTRING_ACCESS_ROW},\t{AVPMENU_ONLINE_ACCESS},") &&
		Has(data, "{AVPMENU_ELEMENT_GOTOMENU,\t{TEXTSTRING_MULTIPLAYER_STARTGAME},\t{AVPMENU_MULTIPLAYER_CONFIG},") &&
		Has(data, "{AVPMENU_ELEMENT_GOTOMENU,\t{TEXTSTRING_COOP_TITLE},\t{AVPMENU_COOP_HOST},") &&
		Has(data, "{AVPMENU_ELEMENT_GOTOMENU,\t\t{TEXTSTRING_MULTIPLAYER_FINDGAMES},\t{AVPMENU_MULTIPLAYERSELECTSESSION},"),
		"each of those rows is a row that goes to its page, so its press goes through the yes");

	std::string const wayIn = SourceSlice(page, "enum AVPMENU_ID ServerYesPage_WayIn(enum AVPMENU_ID from, enum AVPMENU_ID goTo, bool hostsOnline)", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(wayIn, "if (!ServerYes_WayAsks(way, hostsOnline))\n\t{\n\t\treturn goTo;") &&
		Has(wayIn, "return ServerYesPage_Needed(goTo, ServerYes_NoGoesOn(way) ? goTo : AVPMENU_MULTIPLAYER) ? AVPMENU_SERVER_YES : goTo;"),
		"a way that asks opens the yes page in its page's place, while the line needs it; after a No, on or back by the rule");

	// The menus: every row that goes to a page goes through it, as it's pressed, with whether a host would host online
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_GOTOMENU:\n\t\tcase AVPMENU_ELEMENT_GOTOMENU_GFX:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)",
		"\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(pressing, "SetupNewMenu(ServerYesPage_WayIn(AvPMenus.CurrentMenu, static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo), HostsOnline()));") &&
		CountOf(pressing, "SetupNewMenu(") == 1, "a pressed row goes to its page only through the yes, and no other way");
	std::string const online = SourceSlice(menus, "static bool HostsOnline(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(online, "return Relay_ServerName()[0] != 0;"), "a host hosts online when servers.cfg names a server, as Net_HostGame decides it");
	Check(CountOf(menus, "ServerYesPage_WayIn(") == 1 && CountOf(menus, "ServerYesPage_Needed(") == 0,
		"the menus ask in that one place, never as a page opens");

	// After a No nothing goes to the server: the No tells the list nothing (section 3), and the list's thread is made in one place, Start,
	// only after the player's yes to the line
	std::string const starting = SourceSlice(list, "bool Start(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const gate = starting.find("if (!line || !TheLineIsAgreed(*line))\n\t\t{\n\t\t\treturn false;\n\t\t}");
	size_t const thread = starting.find("CreateThread(");
	Check(gate != std::string::npos && thread != std::string::npos && gate < thread && CountOf(list, "CreateThread(") == 1 &&
		CountOf(list, "new (std::nothrow) Shared") == 1 && gate < starting.find("new (std::nothrow) Shared"),
		"after a No the list's thread isn't started: it's made in Start alone, after the yes, so nothing goes to the server");
}

int main()
{
	TestTheRows();
	TestTheLines();
	TestTheWiring();
	TestTheWaysOnline();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
