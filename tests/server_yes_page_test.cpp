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
#include "online_screens.h"   // a name cut to a width (added 2026-11-10, the page's layout)
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
	// Modified 2026-11-11 by Melanikus for the AvP port (one model for every line): the page is one line's, opened from that line's row on
	// the Online page, and it goes back there
	Check(Has(data, "{AVPMENU_FONT_SMALL,\tTEXTSTRING_SERVERYES_TITLE,\t\t\tAvPMenu_ServerYes,\t\t\t\t\tAVPMENU_ONLINE, 0},") &&
		Has(data, "== AVPMENU_SERVER_YES + 1,"), "its title is the question, and Esc goes back to the Online page");

	std::string const opening = SourceSlice(menus, "case AVPMENU_SERVER_YES:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opening, "if (!ServerYesPage_Opened())\n\t\t\t{\n\t\t\t\tSetupNewMenu(AVPMENU_ONLINE);\n\t\t\t\treturn;"),
		"a page whose line has no yes waiting goes back to the Online page");
	Check(Has(menus, "AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)\n\t{\n\t\t// The rows' top, as RenderMenu places them on this page (modified 2026-11-10: so its words stay above them)\n\t\tServerYesPage_Draw(MENU_CENTREY - AvPMenus.MenuHeight / 2);"),
		"the page's words are drawn on it, told where its rows start");
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_ONLINE_ACCESS:\n\t\t{\n\t\t\tenum AVPMENU_ID page = AVPMENU_ONLINE;", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	size_t const ours = pressing.find("if (AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)");
	Check(ours != std::string::npos && ours < pressing.find("OnlineLines_Pressed(") &&
		Has(pressing, "ServerYesPage_Pressed(elementPtr->MaxValue, &page)"), "its rows are pressed as its own, never as the Online page's");

	std::string const pressed = SourceSlice(page, "bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const agreeing = SourceSlice(pressed, "case kServerYesAgreeAndGoOn:", "case kServerYesGoBack:", kSliceEndAfterTheStart, FailedSlice);
	std::string const goingBack = SourceSlice(pressed, "case kServerYesGoBack:", "case kServerYesNothing:", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(agreeing, "RelayList_AgreeToTheLineAt(linePlace);") && agreeing.find("RelayList_AgreeToTheLineAt") < agreeing.find("*page = AVPMENU_ONLINE;"),
		"a yes is kept for the page's own line before the page goes back to the Online page");
	Check(Has(goingBack, "*page = AVPMENU_ONLINE;") && !Has(goingBack, "RelayList_"), "a no goes back too, and tells the list nothing");

	std::string const openFor = SourceSlice(page, "bool ServerYesPage_OpenFor(int place)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const opened = SourceSlice(page, "bool ServerYesPage_Opened(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(openFor, "if (!RelayList_LineToAgreeAt(place, &waiting))\n\t{\n\t\treturn false;") && openFor.find("return false;") < openFor.find("linePlace = place;") &&
		Has(opened, "haveShown = linePlace >= 0 && RelayList_LineToAgreeAt(linePlace, &shown);"),
		"the page is opened only for a line still waiting for its yes, and shows that line, read again as it opens");
	std::string const drawing = SourceSlice(page, "void ServerYesPage_Draw(int rowsTop)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(drawing, "OnlineScreens_SafeName(shown.name, sizeof(shown.name), name, sizeof(name));") &&
		drawing.find("OnlineScreens_SafeName") < drawing.find("RenderSmallMenuText(name"), "the server's name, from a file from outside the game, is made safe before it's drawn");

	size_t const question = words.find("\"Play online through this server?\",\n\t\"on the internet\",\n\t\"on this PC\",\n\t\"on your own network\",\n\t\"key\",\n");
	Check(question != std::string::npos && words.find("\"Its owner approves their own players, and sees your name, your key and your address while you play.\",\n\t\"Yes, use this server\",\n\t\"No, not now\",\n") > question,
		"the words are PO's, in langenum.h's order");
}

// THE ONE WAY IN (modified 2026-11-11 by Melanikus for the AvP port, one model for every line; the owner: "UNIFY THE UI"): the yes is
// asked on a line's own row of the Online page, for that line, and nowhere else. No other row asks for "the" line's yes: Find games and
// the host page show a line without its yes as just that, and send it nothing. It was "the ways online" (2026-11-10), the three rows that
// asked for the one line's yes, which went with the line "in use" (REMOVED-CODE.md). Read in the code, as it needs the whole game.
static void TestTheOneWayIn()
{
	printf("4. the one way in: a line's own row on the Online page\n");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/server_yes_page.cpp");
	std::string const online = ReadSource("3dc/avp/win95/frontend/online_page.cpp");
	std::string const rules = ReadSource("3dc/avp/win95/frontend/server_yes_rules.h");
	std::string const list = ReadSource("3dc/relay_list.cpp");
	Check(menus.size() > 1000 && page.size() > 1000 && online.size() > 1000 && rules.size() > 100 && list.size() > 1000,
		"the menus', the page's, the Online page's, the rules' and the list's sources were found and read");

	// The Online page opens the yes page for the line pressed, when that line's row says it has no yes, and nothing else does
	std::string const pressed = SourceSlice(online, "bool OnlineLines_Pressed(int row, enum AVPMENU_ID *page)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const openTheYes = SourceSlice(pressed, "case kOnlinePageOpenTheYes:", "case kOnlinePageAsk:", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(openTheYes, "if (ServerYesPage_OpenFor(place))\n\t\t\t\t{\n\t\t\t\t\t*page = AVPMENU_SERVER_YES;\n\t\t\t\t\treturn true;") &&
		CountOf(online, "ServerYesPage_OpenFor(") == 1 && CountOf(menus, "ServerYesPage_OpenFor(") == 0,
		"a line's row opens the yes page for that line, and that's the one place it's opened");

	// No row anywhere else asks: a row that goes to a page goes there
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_GOTOMENU:\n\t\tcase AVPMENU_ELEMENT_GOTOMENU_GFX:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)",
		"\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const goes = pressing.find("enum AVPMENU_ID const page = static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo);");
	Check(goes != std::string::npos && goes < pressing.find("SetupNewMenu(page);") && CountOf(pressing, "SetupNewMenu(") == 1 &&
		!Has(menus, "ServerYesPage_WayIn") && !Has(page, "ServerYesPage_WayIn") && !Has(page, "ServerYesPage_Needed") && !Has(rules, "ServerYes_WayAsks"),
		"a pressed row goes to its page, and no row asks for a yes on the way: the ways online are gone");
	std::string const hostsOnline = SourceSlice(menus, "static bool HostsOnline(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(hostsOnline, "return RelayList_HostLine() >= 0;"),
		"a host hosts online on the line the player chose on the host page, and with none chosen, nothing is hosted (modified 2026-11-11)");

	// After a No nothing goes to that line's server: the No tells the list nothing (section 3), and every way of asking a line asks its yes
	// first (relay_list_test reads each). The list's own state has no server until a line has its yes, and every call refuses a state with
	// none.
	std::string const starting = SourceSlice(list, "bool Start(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const noLine = starting.find("if (!lineAgreed)\n\t\t{\n\t\t\tmade->keyState = kKeyNoLine;\n\t\t}");
	size_t const thread = starting.find("CreateThread(");
	Check(noLine != std::string::npos && thread != std::string::npos && noLine < thread && CountOf(list, "CreateThread(") == 1 &&
		Has(list, "if (keyNow == kKeyNoLine)\n\t\t{\n\t\t\ta.why = kNoServer;\n\t\t\treturn a;\n\t\t}"),
		"after a No the list's own state has no server, from before its thread starts, and every call refuses it, so nothing goes to the server");

	// The Multiplayer page asks each line WITH its yes where this PC stands, as nothing of it is kept between runs (PO's ruling,
	// 2026-11-11), by the row's own request and rule, and never a line without its yes. Added 2026-11-11 by Melanikus for the AvP port.
	std::string const askAll = SourceSlice(online, "void OnlineLines_AskTheAgreedLines(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(CountOf(menus, "OnlineLines_AskTheAgreedLines();") == 1 &&
		Has(menus, "\t\t\tRelayList_FindGames();\n\n\t\t\t// And where this PC stands on each line with the player's yes, asked again") &&
		Has(askAll, "if (says != kOnlinePageSaysNoYes && OnlineLines_Pressed(says, askedThisRun[place], now, lastAskedAt[place]) == kOnlinePageAsk)\n\t\t{\n\t\t\tAsk(place);") &&
		CountOf(online, "RelayList_AskForAccessOn(") == 1,
		"the Multiplayer page opening asks each line with its yes, at most once a minute, by the row's own request, and never a line without its yes");

	// A game on a line that hasn't approved this PC is on the list, greyed, never joined (PO's ruling, 2026-11-11)
	std::string const putting = SourceSlice(menus, "static void PutTheRelayGamesOnThePage(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(putting, "bool const approved = RelayList_LineNow(found.line, &now) && now.access == kRelayAccessIsApproved;") &&
		Has(putting, "if (!approved && session >= 0)\n\t\t{\n\t\t\tSessionData[session].AllowedToJoin = false;"),
		"a game on a line that hasn't approved this PC is shown, greyed, and can't be joined");
	Check(Has(menus, "uint64_t const approvedNow = TheApprovedLines();\n\n\t\tif (approvedNow != approvedWhenPut)\n\t\t{\n\t\t\tNet_FindAvPSessions();\n\t\t\tlistChanged = true;\n\t\t}") &&
		Has(menus, "\t\t\tPutTheRelayGamesOnThePage();\n\t\t\tapprovedWhenPut = approvedNow;\n"),
		"and the moment a line's approval comes or goes, the list is put on the page again, so each game is greyed or joinable as its line stands now");
}

// NO HOST ROW: EVERY GAME IS HOSTED ONLINE (the owner's ruling for beta.2, 2026-11-10: "Remove At home from beta.2", "Everything over
// nyc3"). PO's ruling Q1's Host row, Online or At home, went with the game at home: Start game and Co-op campaign host only through the
// server servers.cfg names, by Net_HostGame, which hosts nothing with no server named (net_start_test tries that). Read in the code, as it
// needs the whole game. Added 2026-11-10 by Melanikus for the AvP port, in the Host row's section's place.
static void TestEveryHostIsOnline()
{
	printf("5. no Host row: every game is hosted online, through the server\n");
	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const header = ReadSource("3dc/avp/win95/frontend/menu_data.h");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const coop = ReadSource("3dc/avp/win95/frontend/coop_menu.cpp");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	std::string const names = ReadSource("3dc/avp/langenum.h");
	Check(data.size() > 1000 && header.size() > 100 && menus.size() > 1000 && coop.size() > 1000 && words.size() > 1000 && names.size() > 1000,
		"the rows', the menus', the co-op page's and the words' sources were found and read");

	Check(!Has(header, "MP_HostWhere") && !Has(data, "MP_HostWhere") && !Has(menus, "MP_HostWhere") && !Has(coop, "MP_HostWhere") &&
		!Has(data, "HostWhereRow") && !Has(menus, "HostWhereNames") && !Has(names, "HOSTWHERE") && !Has(menus, "Net_HostAtHome") && !Has(coop, "Net_HostAtHome"),
		"the Host row is gone, with its choice, its words and the game at home");

	// The Multiplayer page's rows and the server a game is hosted on: tests/host_page_test.cpp (moved there 2026-11-11 by Melanikus for the AvP
	// port, hash 2: the Host on row is gone, and Start game and Co-op campaign open the host page)

	// Added 2026-11-11 by Melanikus (Third) for the AvP port: a Hudson joiner comes to Find Games as a player does, through the Multiplayer
	// page, whose opening asks every line with the yes where this PC stands, and its list is pictured once every answer has come
	std::string const runJoin = SourceSlice(menus, "static bool JoinThroughFindGamesForARun(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const joinFromMultiplayer = runJoin.find("SetupNewMenu(AVPMENU_MULTIPLAYER);");
	size_t const joinRow = runJoin.find("AVPMENU_MULTIPLAYERJOINGAME, \"Join Game\")");
	size_t const findRow = runJoin.find("AVPMENU_MULTIPLAYERSELECTSESSION, \"Find Games\")");
	Check(joinFromMultiplayer != std::string::npos && joinRow != std::string::npos && findRow != std::string::npos && joinFromMultiplayer < joinRow &&
		joinRow < findRow && !Has(runJoin, "SetupNewMenu(AVPMENU_MULTIPLAYERJOINGAME);") && Has(runJoin, "&& !ALineIsStillAskingForARun())"),
		"a Hudson joiner goes Multiplayer, Join Game, Find Games, as a player does, so the lines are asked where it stands, and waits for every answer");

	std::string const starting = SourceSlice(menus, "case AVPMENU_ELEMENT_STARTMPGAME:\n\t\t{\n\t\t\tif (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(starting, "NetResult const hosted = Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, netGameData.levelNumber);") &&
		Has(starting, "if (hosted != NET_FAIL)") && CountOf(starting, "Net_HostGame(") == 1, "Start game hosts online, by Net_HostGame alone");
	Check(Has(coop, "NetResult const hosted = Net_HostGame(MP_PlayerName, MP_SessionName, 0, NGT_Coop, firstLevel);") && CountOf(coop, "Net_HostGame(") == 1,
		"and so does the co-op page, which hosts nowhere else");

	Check(!Has(words, "At home") && !Has(words, "host at home") && !Has(words, "home network can join") && !Has(words, "games on this network"),
		"and none of the game's words offers a game at home, or this network's games");
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
	Check(onlyOnline != std::string::npos && onlyOnline < begins.find("hostWaiting = what;") &&
		Has(begins, "hostWaitSince = hostWaitRetrySince != 0 ? hostWaitRetrySince : GetTickCount();") &&
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
	// Third's finding on rc1 (2026-11-10): a host the server's asking again refuses waits on, counted from its first press, never a new 30 s
	size_t const keptAt = hostNow.find("hostWaitRetrySince = hostWaitSince;");
	size_t const forgotAt = hostNow.find("hostWaitRetrySince = 0;");
	Check(keptAt != std::string::npos && forgotAt != std::string::npos && keptAt < hostNow.find("PressTheStartRowAgain();") &&
		keptAt < hostNow.find("SetupNewMenu(AVPMENU_COOP_HOST);") && forgotAt > hostNow.find("SetupNewMenu(AVPMENU_COOP_HOST);"),
		"and it's tried with the wait's start kept, so a wait that follows counts on from the first press, and the start is forgotten after");
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
	Check(Has(startRow, "else if (!AHostWaits(kHostWaitingToStart))\n\t\t\t\t{") &&
		startRow.find("else if (!AHostWaits(kHostWaitingToStart))") < startRow.find("ShowWhyTheHostFailed(AvPMenus.CurrentMenu, Net_HostFailedWhy());"),
		"unless it was refused for the server being asked again: then it waits, as before a first answer (Third's finding on rc1)");
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

	// The five hosts only a run makes put the why in their log lines (modified 2026-11-11 by Melanikus for the AvP port: the fifth is
	// hostrow='s, a Hudson host through the Multiplayer page's Start row, 4d09d91c, which this count was never moved for)
	Check(CountOf(menus, "HostFailedForARun(\"") == 5 && Has(menus, "return why ? std::string(what) + \", because \" + why : std::string(what);"),
		"the five hosts only a run makes say why in their log lines");

	Check(Has(words, "\t\"Asking the server...\",\n\t\"Couldn't host online:\",\n\t\"This game couldn't be hosted.\",\n\t\"The server didn't answer in 30 seconds, so nothing was hosted. Try again.\",\n"),
		"the wait's and the failed host's words, in langenum.h's order");
}

// THE PAGE'S LAYOUT (server_yes_rules.h). Added 2026-11-10 by Melanikus for the AvP port, from the Hudson pair's picture on rc1: the name
// drawn on top of the title, and the owner's words ending with a word alone. The rules are tried here; how the page uses them is read.

// Every letter 20 wide: wider than any of the small font's (17 at most), so a 31-letter name is wider than the menus' edges
static int TwentyWide(int)
{
	return 20;
}

static void TestTheLayout()
{
	printf("7. the page's layout: the title, the name, the lines, the owner's words, the rows\n");

	// A two-row page's rows start where RenderMenu puts them, MENU_CENTREY (280) less half their height: about 255 to 265
	ServerYesLayout const at = ServerYes_Layout(2, 255);
	Check(at.nameTop >= kServerYesTitleTop + kServerYesBigLine, "the name's line starts under the title's, never on it");
	Check(at.whereTop >= at.nameTop + kServerYesSmallLine && at.keyTop >= at.whereTop + kServerYesSmallLine &&
		at.ownerTop >= at.keyTop + kServerYesSmallLine, "and each line under the one before it, by its own height");
	Check(at.ownerLines == 2 && at.bottom == at.ownerTop + 2 * kServerYesSmallLine && at.bottom <= 255 - kServerYesAboveRows,
		"the owner's two lines are drawn whole, and end above the rows with a line to spare");

	ServerYesLayout const tight = ServerYes_Layout(10, 255);
	Check(tight.ownerLines < 10 && tight.ownerLines > 0 && tight.bottom <= 255 - kServerYesAboveRows,
		"words that wouldn't fit above the rows are cut to the lines that do: nothing is drawn into the rows");
	Check(ServerYes_Layout(2, 150).ownerLines == 0 && ServerYes_Layout(-3, 255).ownerLines == 0, "with no room, or no lines, none is drawn");

	// A 31-letter name, the longest a servers.cfg line may have, cut to the menus' edges by its font's widths: its box, centred, lies
	// between the edges, under the title's
	char name[32];
	memset(name, 'W', 31);
	name[31] = 0;
	int const widest = kServerYesRight - kServerYesLeft;
	OnlineScreens_FitToWidth(name, widest, TwentyWide);
	int const nameWidth = static_cast<int>(strlen(name)) * 20;
	Check(nameWidth <= widest && strlen(name) < 31 && name[strlen(name) - 1] == '.',
		"a 31-letter name too wide for the menus' edges is cut to them, and shows it was cut");
	Check(320 - nameWidth / 2 >= kServerYesLeft && 320 + nameWidth / 2 <= kServerYesRight && at.nameTop >= kServerYesTitleTop + kServerYesBigLine,
		"so the title's box and the name's never overlap, whatever the name");

	Check(ServerYes_BoxLeft(300) == kServerYesLeft + (widest - 300) / 2 && ServerYes_BoxLeft(widest) == kServerYesLeft &&
		ServerYes_BoxLeft(0) == kServerYesLeft, "words wrapped narrower than the edges are centred between them");

	// How the page uses them, read in the code
	std::string const page = ReadSource("3dc/avp/win95/frontend/server_yes_page.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const drawing = SourceSlice(page, "void ServerYesPage_Draw(int rowsTop)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(drawing, "int const ownerWidth = OnlinePage_BalancedWidth(owner, widest);") &&
		Has(drawing, "ServerYesLayout const at = ServerYes_Layout(SmallFontString_WrappedLines(owner, ownerWidth), rowsTop);"),
		"the owner's words are wrapped to their balanced width, and their lines counted by the wrapping rule itself");
	Check(Has(drawing, "OnlineScreens_FitToWidth(name, widest, OnlinePage_SmallWidth);") &&
		Has(drawing, "RenderSmallMenuText(name, MENU_CENTREX, at.nameTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);") &&
		!Has(drawing, "RenderMenuText("), "the name is cut to the edges by the small font's widths, and drawn on its own line, never in the title's font");
	Check(Has(drawing, "RECT area = { left, at.ownerTop, left + ownerWidth, at.ownerTop + at.ownerLines * kServerYesSmallLine };"),
		"the owner's words are drawn into a box exactly as tall as the lines that fit");
	Check(Has(menus, "RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);") && kServerYesTitleTop == 70 &&
		Has(menus, "\t\telse\n\t\t{\n\t\t\ty = MENU_CENTREY - AvPMenus.MenuHeight / 2;\n\t\t}"),
		"the layout's title and rows are where RenderMenu draws them");
}

int main()
{
	TestTheRows();
	TestTheLines();
	TestTheWiring();
	TestTheOneWayIn();
	TestEveryHostIsOnline();
	TestTheHostsWait();
	TestTheLayout();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
