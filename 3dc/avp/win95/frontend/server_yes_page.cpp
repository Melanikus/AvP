// The yes page: the player's yes to the server in servers.cfg. Written 2026-11-10 by Melanikus for the AvP port (#175's last part).
// server_yes_page.h says why, what it shows and how it's reached.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "language.h"
#include "relay_list.h"
#include "server_yes_page.h"
#include "hud_layout.h"
// The names made safe and cut to a width, and the online pages' own measures (modified 2026-11-10 by Melanikus for the AvP port: these
// headers in place of the one name this file used to repeat)
#include "online_screens.h"
#include "online_screens_page.h"

#include <stdio.h>
#include <string.h>

// server_yes_rules.h's copies of the fonts' heights and the menus' edges must be the game's own: it can't include the game's headers, so
// the check is here, where the game is built (added 2026-11-10)
static_assert(kServerYesBigLine == MENU_FONT_HEIGHT && kServerYesSmallLine == HUD_FONT_HEIGHT, "the yes page's lines are spaced by the fonts' own heights");
static_assert(kServerYesLeft == MENU_LEFTXEDGE && kServerYesRight == MENU_RIGHTXEDGE, "the yes page's widest box is the menus' own edges");

namespace
{
	// Where the page goes after a yes, and after a no: the page the player was on the way to, and the one they came from
	enum AVPMENU_ID goToAfterYes = AVPMENU_MULTIPLAYER;
	enum AVPMENU_ID backToAfterNo = AVPMENU_MULTIPLAYER;

	// What the page shows of the line, taken as it opens
	RelayListLineToAgree shown;
	bool haveShown = false;

	// The game's own words for where a server is
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
}

bool ServerYesPage_Needed(enum AVPMENU_ID goTo, enum AVPMENU_ID backTo)
{
	RelayListLineToAgree waiting;

	if (!RelayList_LineToAgree(&waiting))
	{
		return false;
	}

	goToAfterYes = goTo;
	backToAfterNo = backTo;
	return true;
}

// Which way online a row is, by the page it's on and the page it goes to (added 2026-11-10 by Melanikus for the AvP port). Find games is
// the Join game page's row.
static ServerYesWay WayOf(enum AVPMENU_ID from, enum AVPMENU_ID goTo)
{
	if (from == AVPMENU_MULTIPLAYER && goTo == AVPMENU_ONLINE_ACCESS)
	{
		return kServerYesWayAccess;
	}

	if (from == AVPMENU_MULTIPLAYER && (goTo == AVPMENU_MULTIPLAYER_CONFIG || goTo == AVPMENU_COOP_HOST))
	{
		return kServerYesWayHost;
	}

	if (from == AVPMENU_MULTIPLAYERJOINGAME && goTo == AVPMENU_MULTIPLAYERSELECTSESSION)
	{
		return kServerYesWayFindGames;
	}

	return kServerYesNotAWay;
}

enum AVPMENU_ID ServerYesPage_WayIn(enum AVPMENU_ID from, enum AVPMENU_ID goTo, bool hostsOnline)
{
	ServerYesWay const way = WayOf(from, goTo);

	if (!ServerYes_WayAsks(way, hostsOnline))
	{
		return goTo;
	}

	// After a No: back to the Multiplayer page, for every way
	return ServerYesPage_Needed(goTo, AVPMENU_MULTIPLAYER) ? AVPMENU_SERVER_YES : goTo;
}

bool ServerYesPage_Opened(void)
{
	haveShown = RelayList_LineToAgree(&shown);
	return haveShown;
}

bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page)
{
	switch (ServerYes_Choice(row))
	{
		case kServerYesAgreeAndGoOn:
			// The yes is kept, and from now on the list may start; where the player was going then asks the server as it always would
			if (!RelayList_AgreeToTheLine())
			{
				*page = backToAfterNo;
				return true;
			}

			*page = goToAfterYes;
			return true;

		case kServerYesGoBack:
			*page = backToAfterNo;
			return true;

		case kServerYesNothing:
			break;
	}

	return false;
}

// Modified 2026-11-10 by Melanikus for the AvP port: each part in its place (server_yes_rules.h, THE PAGE'S LAYOUT). The name was drawn in
// the big font on top of the page's title, and the owner's words ended with a last word alone.
void ServerYesPage_Draw(int rowsTop)
{
	if (!haveShown)
	{
		return;
	}

	// What the server's owner does and sees: wrapped to the narrowest width that keeps its lines, and its lines counted by the very rule
	// that wraps them, so the parts above and the rows below are placed around exactly what's drawn
	char *const owner = GetTextString(TEXTSTRING_SERVERYES_OWNER);
	int const widest = kServerYesRight - kServerYesLeft;
	int const ownerWidth = OnlinePage_BalancedWidth(owner, widest);
	ServerYesLayout const at = ServerYes_Layout(SmallFontString_WrappedLines(owner, ownerWidth), rowsTop);

	// The server's name, on its own line under the page's title. It comes from servers.cfg, a file from outside the game, so it's made
	// safe before it's drawn, and cut to the menus' edges by the small font's own widths.
	char name[sizeof(shown.name) + 1];
	OnlineScreens_SafeName(shown.name, sizeof(shown.name), name, sizeof(name));
	OnlineScreens_FitToWidth(name, widest, OnlinePage_SmallWidth);
	RenderSmallMenuText(name, MENU_CENTREX, at.nameTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	// Where it is, and its key's two ends. The address was written out again from its four numbers, and the key's digits are hex.
	char where[96];
	ServerYes_WhereLine(shown.address, WhereWords(shown.place), where, sizeof(where));
	RenderSmallMenuText(where, MENU_CENTREX, at.whereTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	char key[64];
	ServerYes_KeyLine(GetTextString(TEXTSTRING_SERVERYES_KEY), shown.keyStart, shown.keyEnd, key, sizeof(key));
	RenderSmallMenuText(key, MENU_CENTREX, at.keyTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	// The owner's words, in a box exactly as tall as the lines that fit above the rows, in the middle of the menus' edges
	if (at.ownerLines > 0)
	{
		int x = 0;
		int y = 0;
		int const left = ServerYes_BoxLeft(ownerWidth);
		RECT area = { left, at.ownerTop, left + ownerWidth, at.ownerTop + at.ownerLines * kServerYesSmallLine };
		RenderSmallFontString_Wrapped(owner, &area, ONE_FIXED, &x, &y);
	}
}
