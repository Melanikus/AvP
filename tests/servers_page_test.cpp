// The test of the Servers page: every servers.cfg line, which one the game uses, and the player's pick (#175 2b, PO's ruling A). Written
// 2026-11-10 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: the page shows up to 64 servers and up to 64 lines the game can't use, on the menus' 640 by 480 grid, so nothing may be
// drawn off it, and every server must be reachable. What it shows must be the game's own words and relay_list's own fields, the names made
// safe, and "yes given" only from the kept yeses. Nothing on it may contact a server. The page's rules (servers_page_rules.h) are tried
// here directly; the page and the menus need the whole game, so how they're put together is read in the code.
//
// Nothing here uses the network or the disk, besides reading the game's source files.

#include <stdio.h>
#include <string.h>
#include <string>

#include "servers_page_rules.h"
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

// 1. Where things go: for every number of servers up to 64 and every chosen row, the screenful shown holds the chosen server, stays in
// the servers' own room, and every server can be reached; and the refused lines, the pick's answer and the Back row stay above the help
static void TestTheLayout()
{
	printf("1. nothing drawn off the page, for every number of servers and refused lines up to 64\n");
	bool holdsTheChosen = true;
	bool inItsRoom = true;
	bool screensful = true;
	bool everyReached = true;

	for (int count = 0; count <= kServersPageMostLines; count++)
	{
		for (int chosen = -1; chosen <= count + 1; chosen++)
		{
			ServersPageView const view = ServersPage_View(count, chosen);
			int const place = chosen < 0 ? 0 : chosen >= count ? count - 1 : chosen;

			if (count == 0)
			{
				holdsTheChosen = holdsTheChosen && view.shown == 0;
				continue;
			}

			holdsTheChosen = holdsTheChosen && view.first <= place && place < view.first + view.shown;
			screensful = screensful && view.first % kServersPageMostShown == 0 && view.shown >= 1 && view.shown <= kServersPageMostShown &&
				view.first + view.shown <= count;

			for (int p = 0; p < count; p++)
			{
				int const top = ServersPage_RowTop(p, view);

				if (top < 0)
				{
					continue;
				}

				inItsRoom = inItsRoom && top >= kServersPageRowsTop && top + kServersPageRowHeight <= kServersPageCountTop &&
					p >= view.first && p < view.first + view.shown;
			}
		}

		for (int p = 0; p < count; p++)
		{
			everyReached = everyReached && ServersPage_RowTop(p, ServersPage_View(count, p)) >= 0;
		}
	}

	Check(holdsTheChosen, "the screenful shown always holds the chosen server (the Back row's shows the last)");
	Check(screensful, "a screenful is at most 5 servers, starts at a multiple of 5, and never runs past the last");
	Check(inItsRoom, "and every server shown is drawn in the servers' room, under the title and above the count's line");
	Check(everyReached, "every server, up to the 64th, is shown when it's chosen");

	bool refusedFit = true;

	for (int refused = 0; refused <= kServersPageMostLines; refused++)
	{
		int const shown = ServersPage_RefusedShown(refused);
		int const lines = shown + (refused > shown ? 1 : 0);
		refusedFit = refusedFit && shown >= 0 && shown <= refused && lines <= kServersPageMostRefused &&
			kServersPageRefusedTop + lines * kServersPageSmallLine <= kServersPageStatusTop;
	}

	Check(refusedFit, "the refused lines, with the last saying how many more, never run past their room into the pick's answer");
	Check(ServersPage_RefusedShown(0) == 0 && ServersPage_RefusedShown(4) == 4 && ServersPage_RefusedShown(5) == 3 && ServersPage_RefusedShown(64) == 3,
		"all of four are shown; of five or more, three and a line for the rest");
	Check(kServersPageStatusTop + kServersPageSmallLine <= kServersPageBackTop && kServersPageBackTop + kServersPageBigLine <= kServersPageHelpTop &&
		kServersPageRowsTop >= 92, "the pick's answer, then the Back row, end before the help line; the servers start under the title");

}

// 2. What each line says
static void TestTheLines()
{
	printf("2. the lines the page writes\n");
	char line[96];
	ServersPage_NameLine("New York", true, true, true, "in use", "used from the next start", "yes given", line, sizeof(line));
	Check(strcmp(line, "New York: in use, yes given") == 0, "the server in use, with the yes: its name and both, and no next start, as it's the same line");
	ServersPage_NameLine("Paris", false, true, false, "in use", "used from the next start", "yes given", line, sizeof(line));
	Check(strcmp(line, "Paris: used from the next start") == 0, "the player's pick, not yet in use and with no yes: said so, and no yes");
	ServersPage_NameLine("Tokyo", false, false, false, "in use", "used from the next start", "yes given", line, sizeof(line));
	Check(strcmp(line, "Tokyo") == 0, "any other server: its name alone");
	ServersPage_NameLine(NULL, false, false, true, "in use", NULL, NULL, line, sizeof(line));
	Check(strcmp(line, ": ") == 0, "nothing at all is written as nothing, never read from nowhere");

	ServersPage_RefusedLine("servers.cfg line", 4, "its fingerprint isn't 64 small hex digits", line, sizeof(line));
	Check(strcmp(line, "servers.cfg line 4: its fingerprint isn't 64 small hex digits") == 0, "a refused line is its number and the game's words");

	ServersPage_CountLine(64, ServersPage_View(64, 7), line, sizeof(line));
	Check(strcmp(line, "6-10 / 64") == 0, "with more servers than fit, which are shown");
	ServersPage_CountLine(64, ServersPage_View(64, 64), line, sizeof(line));
	Check(strcmp(line, "61-64 / 64") == 0, "the Back row chosen shows the last ones");
	ServersPage_CountLine(5, ServersPage_View(5, 0), line, sizeof(line));
	Check(line[0] == 0, "and nothing when they all fit");

	char small[8];
	ServersPage_NameLine("New York", true, false, true, "in use", "x", "yes given", small, sizeof(small));
	Check(strcmp(small, "New Yor") == 0, "a line too long for its room is cut short, and still ended");
	ServersPage_RefusedLine("a", 1, "b", NULL, 10);
	ServersPage_CountLine(64, ServersPage_View(64, 0), NULL, 10);
	Check(true, "nowhere to write is no harm");
}

// 3. How the page is put together, read in the code
static void TestTheWiring()
{
	printf("3. how the page is put together, read in the code\n");
	std::string const page = ReadSource("3dc/avp/win95/frontend/servers_page.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const yes = ReadSource("3dc/avp/win95/frontend/server_yes_page.cpp");
	std::string const games = ReadSource("3dc/avp/win95/frontend/online_screens_page.cpp");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	std::string const check = ReadSource("tools/check-online-play.ps1");
	Check(page.size() > 1000 && menus.size() > 1000 && data.size() > 1000 && yes.size() > 1000 && games.size() > 1000 && words.size() > 1000 &&
		check.size() > 1000, "the page's, the menus', the rows', the yes page's, the games page's, the words' and the release check's sources were found and read");

	// Nothing on it contacts a server: it reads relay_list's own lists, and its one change is the pick
	Check(Has(page, "RelayList_LineCount()") && Has(page, "RelayList_LineAt(i, &servers[serverCount])") && Has(page, "RelayList_RefusedLineCount()") &&
		Has(page, "RelayList_RefusedLineAt(i, &lineNumber, &why)") && Has(page, "RelayList_FileWords()") && CountOf(page, "RelayList_PickLine(") == 1,
		"the page reads every line through relay_list's own calls, and its one change is the pick");
	Check(!Has(page, "WinHttp") && !Has(page, "socket(") && !Has(page, "RelayList_FindGames") && !Has(page, "RelayList_FindRelays") &&
		!Has(page, "RelayList_AskForAccess") && !Has(page, "RelayList_AgreeToTheLine") && !Has(page, "Net_Host") && !Has(page, "ServersCfg_") &&
		!Has(page, "fopen") && !Has(page, "CreateFile"), "and nothing else: no call to any server, no yes, and never servers.cfg itself");

	// What it shows: relay_list's fields, the name made safe first, the address as relay_list wrote it, "yes given" from agreed alone
	std::string const row = SourceSlice(page, "void ServersPage_DrawRow(const AVPMENU_ELEMENT *row, int element, int brightness, int chosen)", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	size_t const safe = row.find("OnlineScreens_SafeName(server.line.name, sizeof(server.line.name), name, sizeof(name));");
	size_t const notes = row.find("ServersPage_NameLine(name, server.inUse, server.nextStart, server.agreed,");
	Check(safe != std::string::npos && notes != std::string::npos && safe < notes,
		"a server's name is made safe before it's written, and its notes are relay_list's own: yes given from agreed alone");
	Check(Has(row, "ServerYes_WhereLine(server.line.address, WhereWords(server.line.place), where, sizeof(where));") && !Has(page, "%u.") &&
		!Has(page, ".agreed =") && !Has(page, "agreed = "), "its address is relay_list's, never written out here, and nothing here decides a yes");
	std::string const top = SourceSlice(page, "void ServersPage_DrawTop(int chosen)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const whySafe = top.find("OnlineScreens_SafeName(why, sizeof(safe), safe, sizeof(safe));");
	size_t const refusedLine = top.find("ServersPage_RefusedLine(GetTextString(TEXTSTRING_SERVERS_LINE), lineNumber, safe, line, sizeof(line));");
	Check(whySafe != std::string::npos && refusedLine != std::string::npos && whySafe < refusedLine,
		"a refused line is its number and the game's own words, made safe too");
	std::string const pressed = SourceSlice(page, "bool ServersPage_Pressed(int row)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(pressed, "if (place < 0 || place >= serverCount)\n\t{\n\t\treturn false;") &&
		Has(pressed, "answer = RelayList_PickLine(place) ? TEXTSTRING_SERVERS_PICKED : TEXTSTRING_SERVERS_NOT_PICKED;") &&
		pressed.find("RelayList_PickLine") < pressed.find("ReadTheServers();"), "a pressed server is picked for the next start, and the page says which answer it got");
	std::string const opened = SourceSlice(page, "bool ServersPage_Opened(AVPMENU_ELEMENT *rows, int room)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opened, "if (!rows || room < kServersPageRowRoom)\n\t{\n\t\treturn false;") && Has(opened, "rows[row] = end;") &&
		Has(page, "for (int i = 0; i < count && i < kServersPageMostLines; i++)"), "the page writes at most its room's rows, and always its end");

	// The menus: the page opens, its rows are pressed as its own, and it draws its rows itself
	std::string const opening = SourceSlice(menus, "case AVPMENU_SERVERS:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opening, "if (!ServersPage_Opened(AvPMenus.MenuElements, kServersPageRowRoom))\n\t\t\t{\n\t\t\t\tSetupNewMenu(AVPMENU_MULTIPLAYER);"),
		"the page writes its rows as it opens, and one that can't goes back to the Multiplayer page");
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_ONLINE_ACCESS:\n\t\t{\n\t\t\tenum AVPMENU_ID page = AVPMENU_ONLINE_ACCESS;", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	size_t const ours = pressing.find("if (AvPMenus.CurrentMenu == AVPMENU_SERVERS)");
	Check(ours != std::string::npos && ours < pressing.find("AccessPage_Pressed(") && Has(pressing, "ServersPage_Pressed(elementPtr->MaxValue);"),
		"its rows are pressed as its own, never as the access page's");
	Check(Has(menus, "if (serversPage)\n\t{\n\t\tServersPage_DrawTop(AvPMenus.CurrentlySelectedElement);") &&
		Has(menus, "else if (serversPage)\n\t\t{\n\t\t\tServersPage_DrawRow(elementPtr, e, elementPtr->Brightness, AvPMenus.CurrentlySelectedElement);"),
		"and it draws its own rows, a screenful at a time, and its words around them");

	// The Multiplayer page's Servers row: always there, under the Host row; it goes to a page that contacts nothing, so it asks no yes
	std::string const setter = SourceSlice(data, "void MenuData_SetMultiplayerRows(bool aServerIsNamed, char **hostWhereNames)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const hostRow = setter.find("AvPMenu_Multiplayer[row] = HostWhereRow;");
	size_t const serversRow = setter.find("\tAvPMenu_Multiplayer[row++] = ServersRow;\n");
	Check(serversRow != std::string::npos && hostRow < serversRow && serversRow < setter.find("= StartGameRow;") &&
		setter.rfind("if (aServerIsNamed)", serversRow) < hostRow, "the Servers row is on the Multiplayer page always, just under the Host row");
	Check(Has(data, "static AVPMENU_ELEMENT const ServersRow = {AVPMENU_ELEMENT_GOTOMENU,\t{TEXTSTRING_SERVERS_ROW},\t{AVPMENU_SERVERS},") &&
		Has(data, "static AVPMENU_ELEMENT AvPMenu_Servers[kServersPageRowRoom] = { {AVPMENU_ELEMENT_ENDOFMENU} };") &&
		Has(data, "== AVPMENU_SERVERS + 1,"), "it goes to the Servers page, whose table has room for every row, and has its entry");
	std::string const wayOf = SourceSlice(yes, "static ServerYesWay WayOf(enum AVPMENU_ID from, enum AVPMENU_ID goTo)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(!wayOf.empty() && !Has(wayOf, "AVPMENU_SERVERS"), "and it isn't a way online: the page contacts nothing, so it asks no yes");

	// Find games: one line, which server the games come from
	std::string const gamesTop = SourceSlice(games, "void OnlinePage_DrawTop(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const fromSafe = gamesTop.find("OnlineScreens_SafeName(Relay_ServerName(), kServersCfgNameLetters, server, sizeof(server));");
	size_t const fromLine = gamesTop.find("snprintf(from, sizeof(from), \"%s %s\", GetTextString(TEXTSTRING_FINDGAMES_FROM), server);");
	Check(fromSafe != std::string::npos && fromLine != std::string::npos && fromSafe < fromLine,
		"the games page says which server its games come from, its name made safe, only ever as an argument");

	Check(Has(check, "avp/win95/frontend/(access_page|server_yes|online_screens|servers_page)[\\w]*\\.(cpp|h))$'"),
		"release check rule D covers the Servers page: -dev decides nothing on it");
	Check(Has(words, "\t\"Servers\",\n\t\"Every server in servers.cfg: which one the game uses, and picking another for the next start.\",\n\t\"Servers\",\n") &&
		Has(words, "\t\"Used from the next start of the game.\",\n\t\"The pick couldn't be kept. The log says why.\",\n\t\"Games from\",\n"),
		"the page's words, in langenum.h's order");

	// Nothing is drawn past the menus' edges, whatever the font's letters are: the font's widths are only known as the game runs (each
	// letter up to 17 pixels), so every small line the page draws is cut there, by the font's own measure, with "..." after it
	std::string const fitting = SourceSlice(page, "void FitToTheEdges(char *line, size_t room)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const small = SourceSlice(page, "void SmallLine(const char *text, int y, int alpha)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(fitting, "int const width = kServersPageRight - kServersPageLeft;") && Has(fitting, "if (SmallFontString_WrappedLines(tried, width) <= 1)") &&
		Has(fitting, "snprintf(tried, sizeof(tried), \"%.*s...\", static_cast<int>(fits), line);"),
		"a line wider than the menus' edges is cut to the longest start of it that fits, by the font's own widths");
	size_t const fitted = small.find("FitToTheEdges(line, sizeof(line));");
	Check(fitted != std::string::npos && fitted < small.find("RenderSmallMenuText(") && CountOf(page, "RenderSmallMenuText(") == 1,
		"and every small line the page draws goes through it before it's drawn");
	std::string const wrapped = SourceSlice(page, "if (fileWords)\n\t{", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(wrapped, "RECT area = { kServersPageLeft, y, kServersPageRight, y + kServersPageMostRefused * kServersPageSmallLine };") &&
		CountOf(page, "RenderSmallFontString_Wrapped(") == 1, "the one wrapped text, why no line is used, is held to the refused lines' own box");
}

int main()
{
	TestTheLayout();
	TestTheLines();
	TestTheWiring();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
