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
#include "host_wait_rules.h"
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
	size_t const pressedWayIn = pressing.find("enum AVPMENU_ID const page = ServerYesPage_WayIn(AvPMenus.CurrentMenu, static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo), HostsOnline());");
	Check(pressedWayIn != std::string::npos && pressedWayIn < pressing.find("SetupNewMenu(page);") && CountOf(pressing, "SetupNewMenu(") == 1,
		"a pressed row goes to its page only through the yes, and no other way");
	std::string const online = SourceSlice(menus, "static bool HostsOnline(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(online, "return Relay_ServerName()[0] != 0 && MP_HostWhere != kHostWhereAtHome;"),
		"a host hosts online when servers.cfg names a server and the Host row says Online");
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

// WHERE A HOST'S GAME IS HOSTED (PO's ruling Q1, 2026-11-10): the Multiplayer page's Host row, Online (the server's name) or At home. It's
// there only when servers.cfg names a server, starts on Online at every start of the game, and is never saved. Start game and Co-op
// campaign host by it. Read in the code, as it needs the whole game; net_start_test tries Net_HostAtHome itself. Added 2026-11-10 by
// Melanikus for the AvP port.
static void TestWhereAHostHosts()
{
	printf("5. the Host row: where a host's game is hosted\n");
	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const header = ReadSource("3dc/avp/win95/frontend/menu_data.h");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const coop = ReadSource("3dc/avp/win95/frontend/coop_menu.cpp");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	std::string const setups = ReadSource("3dc/avp/win95/frontend/mp_config.cpp");
	std::string const profiles = ReadSource("3dc/avp/win95/frontend/user_profile.cpp");
	std::string const settings = ReadSource("3dc/ConfigFile.cpp");
	Check(data.size() > 1000 && header.size() > 100 && menus.size() > 1000 && coop.size() > 1000 && words.size() > 1000 && setups.size() > 1000 &&
		profiles.size() > 1000 && settings.size() > 1000, "the rows', the menus', the co-op page's, the words' and the three saving files' sources were found and read");

	Check(Has(header, "enum { kHostWhereOnline = 0, kHostWhereAtHome = 1 };\nextern int MP_HostWhere;") && Has(data, "int MP_HostWhere = kHostWhereOnline;"),
		"it starts on Online at every start of the game");
	Check(!Has(setups, "MP_HostWhere") && !Has(profiles, "MP_HostWhere") && !Has(settings, "MP_HostWhere"),
		"and it's never saved: not with a multiplayer setup, not in a profile, not in the settings file");
	Check(Has(data, "static AVPMENU_ELEMENT const HostWhereRow = {AVPMENU_ELEMENT_TEXTSLIDER_POINTER,\t{TEXTSTRING_MULTIPLAYER_HOSTWHERE},\t{kHostWhereAtHome},\t{&MP_HostWhere},\t{0},\tTEXTSTRING_MULTIPLAYER_HOSTWHERE_HELP};"),
		"the row chooses between its two places, with its help line");

	std::string const setter = SourceSlice(data, "void MenuData_SetMultiplayerRows(bool aServerIsNamed, char **hostWhereNames)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const hostRow = setter.find("\tif (aServerIsNamed)\n\t{\n\t\tAvPMenu_Multiplayer[row] = HostWhereRow;\n\t\tAvPMenu_Multiplayer[row].TextSliderStringPointer = hostWhereNames;\n\t\trow++;\n\t}");
	Check(hostRow != std::string::npos && setter.find("= PlayerNameRow;") < hostRow && hostRow < setter.find("= StartGameRow;") && CountOf(data, "= HostWhereRow;") == 1,
		"it's on the page only when servers.cfg names a server, just above the rows that host");

	std::string const names = SourceSlice(menus, "static char **HostWhereNames(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const safe = names.find("OnlineScreens_SafeName(Relay_ServerName(), kServersCfgNameLetters, server, sizeof(server));");
	Check(safe != std::string::npos &&
		safe < names.find("snprintf(online, sizeof(online), \"%s (%.20s)\", GetTextString(TEXTSTRING_MULTIPLAYER_HOSTWHERE_ONLINE), server);") &&
		Has(names, "snprintf(atHome, sizeof(atHome), \"%s\", GetTextString(TEXTSTRING_MULTIPLAYER_HOSTWHERE_ATHOME));"),
		"its first choice names the server, made safe and cut to 20 letters, only ever as an argument; the second is At home");

	// The two rows that host, by it
	std::string const starting = SourceSlice(menus, "case AVPMENU_ELEMENT_STARTMPGAME:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(starting, "NetResult const hosted = MP_HostWhere == kHostWhereAtHome ?\n\t\t\t\t\tNet_HostAtHome(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, netGameData.levelNumber) :\n\t\t\t\t\tNet_HostGame(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, netGameData.levelNumber);") &&
		Has(starting, "if (hosted != NET_FAIL)") && CountOf(starting, "Net_HostGame(") == 1 && CountOf(starting, "Net_HostAtHome(") == 1, "Start game hosts at home when the row says so, online otherwise");
	Check(Has(coop, "NetResult const hosted = MP_HostWhere == kHostWhereAtHome ? Net_HostAtHome(MP_PlayerName, MP_SessionName, 0, NGT_Coop, firstLevel) :\n\t\tNet_HostGame(MP_PlayerName, MP_SessionName, 0, NGT_Coop, firstLevel);") &&
		CountOf(coop, "Net_HostGame(") == 1 && CountOf(coop, "Net_HostAtHome(") == 1, "and so does the co-op page, which hosts nowhere else");

	Check(Has(words, "\t\"Host\",\n\t\"Online\",\n\t\"At home\",\n\t\"Online: players anywhere can find your game through this server. At home: only players on this home network can join.\",\n"),
		"the row's words, and its help line says plainly who can join at home");
}

// A HOST'S WAIT, AND A FAILED HOST'S LINE (PO's rulings Q3 and Q4, 2026-11-10; host_wait_rules.h says why). The rules are tried here,
// with a server that never answers; how the menus use them is read in the code. Added 2026-11-10 by Melanikus for the AvP port.
static void TestTheHostsWait()
{
	printf("6. a host waits for the server, never forever, and a failed host says why\n");
	Check(HostWait_Step(false, true, false, 0, 0) == kHostWaitHostNow && HostWait_Step(true, true, false, 0, 99999) == kHostWaitHostNow,
		"a server that's ready hosts now, even at the limit");
	Check(HostWait_Step(false, false, true, 0, 0) == kHostWaitCantUse, "a server that can't be used stops the wait, to say why");
	Check(HostWait_Step(true, false, false, 1000, 1000) == kHostWaitGoOn && HostWait_Step(true, false, false, 1000, 1000 + kHostWaitMostMs - 1) == kHostWaitGoOn,
		"a server still being asked is waited for, up to the limit");
	Check(HostWait_Step(true, false, false, 1000, 1000 + kHostWaitMostMs) == kHostWaitGiveUp && HostWait_Step(true, false, false, 1000, 4000000000u) == kHostWaitGiveUp,
		"A SERVER THAT NEVER ANSWERS: at the limit the wait gives up, and so on every frame after");
	uint32_t const nearTheWrap = 0xFFFFFFFFu - 1000u;
	Check(HostWait_Step(true, false, false, nearTheWrap, nearTheWrap + 5000u) == kHostWaitGoOn &&
		HostWait_Step(true, false, false, nearTheWrap, nearTheWrap + kHostWaitMostMs) == kHostWaitGiveUp,
		"and the clock going round past 0 neither stops it early nor lets it wait forever");
	Check(HostWait_Step(false, false, false, 0, 0) == kHostWaitHostNow, "nothing to wait for (no server, or no yes) hosts now, and the host says what's wrong");
	Check(kHostWaitMostMs == 30000u, "the limit is 30 seconds, as the words say");

	char line[96];
	HostWait_FailedLine("Couldn't host online:", "the relay server couldn't be reached", line, sizeof(line));
	Check(strcmp(line, "Couldn't host online: the relay server couldn't be reached.") == 0, "a failed host's line is what failed, then why");
	HostWait_FailedLine("This game couldn't be hosted.", NULL, line, sizeof(line));
	Check(strcmp(line, "This game couldn't be hosted.") == 0, "with no why, what failed alone");
	char small[8];
	HostWait_FailedLine("Couldn't host online:", "x", small, sizeof(small));
	Check(strcmp(small, "Couldn'") == 0, "cut short at its room, and still ended");
	HostWait_FailedLine("a", "b", NULL, 10);
	Check(true, "nowhere to write is no harm");

	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	Check(menus.size() > 1000 && words.size() > 1000, "the menus' and the words' sources were found and read");

	std::string const begins = SourceSlice(menus, "static bool AHostWaits(HostWaitFor what)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const onlyOnline = begins.find("if (!HostsOnline() || Net_ServerState(&why) != kNetServerAsking)\n\t{\n\t\treturn false;\n\t}");
	Check(onlyOnline != std::string::npos && onlyOnline < begins.find("hostWaiting = what;") && Has(begins, "hostWaitSince = GetTickCount();") &&
		Has(begins, "ShowTheHostLine(hostWaitPage, GetTextString(TEXTSTRING_HOST_ASKING));"),
		"a host waits only online, only while the server hasn't answered, and says Asking the server...");

	std::string const frame = SourceSlice(menus, "static void HostWaitFrame(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const leaving = frame.find("if (AvPMenus.MenusState != MENUSSTATE_MAINMENUS || AvPMenus.CurrentMenu != hostWaitPage)\n\t{\n\t\thostWaiting = kHostWaitingForNothing;\n\t\treturn;\n\t}");
	size_t const step = frame.find("switch (HostWait_Step(state == kNetServerAsking, state == kNetServerReady, state == kNetServerCantUse, hostWaitSince, GetTickCount()))");
	Check(leaving != std::string::npos && step != std::string::npos && leaving < step, "Esc, or any way off the page, ends the wait before it could host");
	std::string const hostNow = SourceSlice(frame, "case kHostWaitHostNow:", "case kHostWaitCantUse:", kSliceEndAfterTheStart, FailedSlice);
	std::string const cantUse = SourceSlice(frame, "case kHostWaitCantUse:", "case kHostWaitGiveUp:", kSliceEndAfterTheStart, FailedSlice);
	std::string const givingUp = frame.substr(frame.find("case kHostWaitGiveUp:") == std::string::npos ? frame.size() : frame.find("case kHostWaitGiveUp:"));
	Check(Has(hostNow, "PressTheStartRowAgain();") && Has(hostNow, "SetupNewMenu(AVPMENU_COOP_HOST);"), "a server that's ready: the host goes on as it was pressed");
	Check(Has(cantUse, "ShowWhyTheHostFailed(hostWaitPage, why);") && !Has(cantUse, "PressTheStartRowAgain") && !Has(cantUse, "SetupNewMenu"),
		"one that can't be used: why, and nothing hosted");
	Check(Has(givingUp, "ShowTheHostLine(hostWaitPage, GetTextString(TEXTSTRING_HOST_NO_ANSWER));") && !Has(givingUp, "PressTheStartRowAgain") && !Has(givingUp, "SetupNewMenu"),
		"one that never answered: the words for that, and nothing hosted");
	std::string const pressAgain = SourceSlice(menus, "static void PressTheStartRowAgain(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(pressAgain, "if (AvPMenus.MenuElements[row].ElementID == AVPMENU_ELEMENT_STARTMPGAME)") &&
		Has(pressAgain, "InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_SELECT);") && CountOf(menus, "PressTheStartRowAgain();") == 1,
		"and the Start row is pressed again for the player, so the game is hosted just as the row hosts it");

	std::string const updating = SourceSlice(menus, "extern void AvP_UpdateMenus(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(updating, "\tHostWaitFrame();\n") && Has(SourceSlice(menus, "static void RenderMenu(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice), "\tDrawTheHostLine();\n"),
		"the wait goes on every frame of the menus, and its line is drawn with the page");

	// The two player places: Start game, and the co-op page that hosts as it opens
	std::string const startRow = SourceSlice(menus, "case AVPMENU_ELEMENT_STARTMPGAME:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	size_t const startWaits = startRow.find("if (AHostWaits(kHostWaitingToStart))\n\t\t\t\t{\n\t\t\t\t\tbreak;\n\t\t\t\t}");
	Check(startWaits != std::string::npos && startWaits < startRow.find("Campaign_SetFromLevelRow();") && startWaits < startRow.find("Net_HostGame("),
		"Start game waits for the server when it must, before it sets anything up or hosts");
	size_t const hostedOrNot = startRow.find("if (hosted != NET_FAIL)");
	Check(hostedOrNot != std::string::npos && hostedOrNot < startRow.find("ShowWhyTheHostFailed(AvPMenus.CurrentMenu, Net_HostFailedWhy());"),
		"and a Start that fails says why on its page");
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_GOTOMENU:\n\t\tcase AVPMENU_ELEMENT_GOTOMENU_GFX:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)",
		"\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const coopWaits = pressing.find("if (page == AVPMENU_COOP_HOST && AHostWaits(kHostWaitingToOpenCoop))\n\t\t\t\t{\n\t\t\t\t\tbreak;\n\t\t\t\t}");
	Check(coopWaits != std::string::npos && coopWaits < pressing.find("SetupNewMenu(page);"), "Co-op campaign waits on the Multiplayer page before its page hosts");
	std::string const coopOpening = SourceSlice(menus, "case AVPMENU_COOP_HOST:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	size_t const back = coopOpening.find("SetupNewMenu(AVPMENU_MULTIPLAYER);");
	Check(back != std::string::npos && back < coopOpening.find("ShowWhyTheHostFailed(AVPMENU_MULTIPLAYER, why);"),
		"a co-op page that can't host goes back to the Multiplayer page, which says why");
	std::string const failedLine = SourceSlice(menus, "static void ShowWhyTheHostFailed(enum AVPMENU_ID page, const char *why)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(failedLine, "HostWait_FailedLine(GetTextString(why ? TEXTSTRING_HOST_FAILED : TEXTSTRING_HOST_FAILED_PLAIN), why, line, sizeof(line));") &&
		CountOf(menus, "ShowWhyTheHostFailed(") == 4, "the line is the game's words, then a why that's only ever Net_HostFailedWhy's or Net_ServerState's");

	// The four hosts only a run makes put the why in their log lines
	Check(CountOf(menus, "HostFailedForARun(\"") == 4 && Has(menus, "return why ? std::string(what) + \", because \" + why : std::string(what);"),
		"the four hosts only a run makes say why in their log lines");

	Check(Has(words, "\t\"Asking the server...\",\n\t\"Couldn't host online:\",\n\t\"This game couldn't be hosted.\",\n\t\"The server didn't answer in 30 seconds, so nothing was hosted. Try again, or host at home.\",\n"),
		"the wait's and the failed host's words, in langenum.h's order");
}

int main()
{
	TestTheRows();
	TestTheLines();
	TestTheWiring();
	TestTheWaysOnline();
	TestWhereAHostHosts();
	TestTheHostsWait();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
