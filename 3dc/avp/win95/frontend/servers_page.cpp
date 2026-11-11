// The Servers page (#175 2b, PO's ruling A). Written 2026-11-10 by Melanikus for the AvP port. servers_page.h says why, and what it never
// does; servers_page_rules.h has its layout and its lines, game-free.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "hud_layout.h"     // HUD_FONT_HEIGHT, the small font's height
#include "language.h"
#include "relay_list.h"
#include "server_yes_rules.h"   // the address's and the key's lines, as the yes page shows them
#include "servers_page.h"

#include <stdio.h>
#include <string.h>

// The names the screens make safe before drawing them (online_screens.cpp): a letter the game's font doesn't have shows as ?
void OnlineScreens_SafeName(const char *from, size_t mostBytes, char *to, size_t toSize);

// servers_page_rules.h's copies of the menus' numbers must be the menus' own: it can't include menus.h, so these checks are here, where
// the game is built
static_assert(kServersPageSmallLine == HUD_FONT_HEIGHT, "the small font's height is HUD_FONT_HEIGHT");
static_assert(kServersPageBigLine == MENU_FONT_HEIGHT, "the big font's height is MENU_FONT_HEIGHT");
static_assert(kServersPageLeft == MENU_LEFTXEDGE && kServersPageRight == MENU_RIGHTXEDGE, "the page's edges are the menus' own");

namespace
{
	// What the page shows, read as it opens, and again after a pick. Only the game's main thread uses these.
	RelayListLineShown servers[kServersPageMostLines];
	int serverCount = 0;
	TEXTSTRING_ID answer = TEXTSTRING_BLANK;   // the pick's answer, under the refused lines

	void ReadTheServers(void)
	{
		int const count = RelayList_LineCount();
		serverCount = 0;

		for (int i = 0; i < count && i < kServersPageMostLines; i++)
		{
			if (!RelayList_LineAt(i, &servers[serverCount]))
			{
				break;
			}

			serverCount++;
		}
	}

	// The game's own words for where a server is (the yes page's)
	const char *WhereWords(ServersCfgPlace place)
	{
		switch (place)
		{
			case kServersCfgOnThisPC: return GetTextString(TEXTSTRING_SERVERYES_ON_THIS_PC);
			case kServersCfgOnTheOwnNetwork: return GetTextString(TEXTSTRING_SERVERYES_ON_OWN_NETWORK);
			case kServersCfgOnTheInternet: break;
		}

		return GetTextString(TEXTSTRING_SERVERYES_ON_INTERNET);
	}

	// A line cut short, with "..." after it, if it's wider than the menus' edges: measured by the font's own widths
	// (SmallFontString_WrappedLines), so nothing is drawn past the edges whatever the font's letters are. The longest start of the line
	// that fits is found by halving.
	void FitToTheEdges(char *line, size_t room)
	{
		int const width = kServersPageRight - kServersPageLeft;
		size_t const length = strlen(line);

		if (length == 0 || SmallFontString_WrappedLines(line, width) <= 1)
		{
			return;
		}

		char tried[200];
		size_t fits = 0;
		size_t tooLong = length;

		while (fits + 1 < tooLong)
		{
			size_t const middle = (fits + tooLong) / 2;
			snprintf(tried, sizeof(tried), "%.*s...", static_cast<int>(middle), line);

			if (SmallFontString_WrappedLines(tried, width) <= 1)
			{
				fits = middle;
			}
			else
			{
				tooLong = middle;
			}
		}

		snprintf(tried, sizeof(tried), "%.*s...", static_cast<int>(fits), line);
		snprintf(line, room, "%s", tried);
	}

	// One small line, centred, never wider than the menus' edges
	void SmallLine(const char *text, int y, int alpha)
	{
		char line[160];
		snprintf(line, sizeof(line), "%s", text ? text : "");
		FitToTheEdges(line, sizeof(line));
		RenderSmallMenuText(line, MENU_CENTREX, y, alpha, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
}

bool ServersPage_Opened(AVPMENU_ELEMENT *rows, int room)
{
	if (!rows || room < kServersPageRowRoom)
	{
		return false;
	}

	answer = TEXTSTRING_BLANK;
	ReadTheServers();

	// A row for each server, which picks it, then the Back row, then the end
	int row = 0;

	for (int i = 0; i < serverCount; i++)
	{
		AVPMENU_ELEMENT const server = {AVPMENU_ELEMENT_ONLINE_ACCESS, {TEXTSTRING_BLANK}, {static_cast<uint32_t>(kServersPageRowFirst + i)}, {0}, {0},
			TEXTSTRING_SERVERS_PICK_HELP};
		rows[row++] = server;
	}

	AVPMENU_ELEMENT const back = {AVPMENU_ELEMENT_GOTOMENU, {TEXTSTRING_SERVERS_BACK}, {AVPMENU_MULTIPLAYER}, {0}, {0}, TEXTSTRING_BLANK};
	AVPMENU_ELEMENT const end = {AVPMENU_ELEMENT_ENDOFMENU};
	rows[row++] = back;
	rows[row] = end;
	return true;
}

bool ServersPage_Pressed(int row)
{
	int const place = row - kServersPageRowFirst;

	if (place < 0 || place >= serverCount)
	{
		return false;
	}

	// The pick is kept for the next start (relay_list.cpp says it's never used halfway through a run); the rows are read again, so the
	// page shows which server the next start uses
	answer = RelayList_PickLine(place) ? TEXTSTRING_SERVERS_PICKED : TEXTSTRING_SERVERS_NOT_PICKED;
	ReadTheServers();
	return true;
}

void ServersPage_DrawTop(int chosen)
{
	ServersPageView const view = ServersPage_View(serverCount, chosen);
	char line[160];

	// Which servers are shown, when they don't all fit
	ServersPage_CountLine(serverCount, view, line, sizeof(line));

	if (line[0])
	{
		SmallLine(line, kServersPageCountTop, ONE_FIXED / 2);
	}

	// Why no line is used at all, or each line that isn't, in the game's own words
	const char *const fileWords = RelayList_FileWords();
	int y = kServersPageRefusedTop;

	if (fileWords)
	{
		char whole[200];
		snprintf(whole, sizeof(whole), "%s %s", GetTextString(TEXTSTRING_SERVERS_FILE_NOT_USED), fileWords);
		RECT area = { kServersPageLeft, y, kServersPageRight, y + kServersPageMostRefused * kServersPageSmallLine };
		RenderSmallFontString_Wrapped(whole, &area, ONE_FIXED, 0, 0);
	}
	else
	{
		int const refused = RelayList_RefusedLineCount();
		int const shown = ServersPage_RefusedShown(refused);

		for (int i = 0; i < shown; i++)
		{
			int lineNumber = 0;
			const char *why = NULL;

			if (RelayList_RefusedLineAt(i, &lineNumber, &why))
			{
				char safe[120];
				OnlineScreens_SafeName(why, sizeof(safe), safe, sizeof(safe));
				ServersPage_RefusedLine(GetTextString(TEXTSTRING_SERVERS_LINE), lineNumber, safe, line, sizeof(line));
				SmallLine(line, y, ONE_FIXED / 2);
			}

			y += kServersPageSmallLine;
		}

		if (refused > shown)
		{
			snprintf(line, sizeof(line), "%s %d", GetTextString(TEXTSTRING_SERVERS_MORE_NOT_USED), refused - shown);
			SmallLine(line, y, ONE_FIXED / 2);
		}
		else if (serverCount == 0 && refused == 0)
		{
			SmallLine(GetTextString(TEXTSTRING_SERVERS_NONE), y, ONE_FIXED / 2);
		}
	}

	if (answer != TEXTSTRING_BLANK)
	{
		SmallLine(GetTextString(answer), kServersPageStatusTop, ONE_FIXED);
	}
}

void ServersPage_DrawRow(const AVPMENU_ELEMENT *row, int element, int brightness, int chosen)
{
	if (!row)
	{
		return;
	}

	// The Back row, above the help line
	if (row->ElementID == AVPMENU_ELEMENT_GOTOMENU)
	{
		RenderMenuText(GetTextString(static_cast<enum TEXTSTRING_ID>(row->TextDescription)), MENU_CENTREX, kServersPageBackTop, brightness,
			AVPMENUFORMAT_CENTREJUSTIFIED);
		return;
	}

	int const place = static_cast<int>(row->MaxValue) - kServersPageRowFirst;
	int const top = ServersPage_RowTop(place, ServersPage_View(serverCount, chosen));
	(void)element;

	if (place < 0 || place >= serverCount || top < 0)
	{
		return;
	}

	// Its name, made safe first, as it comes from a file from outside the game, then what's true of it: "yes given" from the kept yeses
	// alone (RelayListLineShown.agreed), never from the pick
	RelayListLineShown const &server = servers[place];
	char name[sizeof(server.line.name) + 1];
	OnlineScreens_SafeName(server.line.name, sizeof(server.line.name), name, sizeof(name));
	char first[160];
	ServersPage_NameLine(name, server.inUse, server.nextStart, server.agreed, GetTextString(TEXTSTRING_SERVERS_IN_USE),
		GetTextString(TEXTSTRING_SERVERS_NEXT_START), GetTextString(TEXTSTRING_SERVERS_YES_GIVEN), first, sizeof(first));
	SmallLine(first, top, brightness);

	// Its address as relay_list wrote it out (RelayList_ShownAddress), where that is, and its key's two ends, as the yes page shows them
	char where[96];
	char key[64];
	char second[176];
	ServerYes_WhereLine(server.line.address, WhereWords(server.line.place), where, sizeof(where));
	ServerYes_KeyLine(GetTextString(TEXTSTRING_SERVERYES_KEY), server.line.keyStart, server.line.keyEnd, key, sizeof(key));
	snprintf(second, sizeof(second), "%s   %s", where, key);
	SmallLine(second, top + kServersPageSmallLine, brightness / 2);
}
