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

#include <stdio.h>
#include <string.h>

// The names the screens make safe before drawing them (online_screens.cpp): a letter the game's font doesn't have shows as ?
void OnlineScreens_SafeName(const char *from, size_t mostBytes, char *to, size_t toSize);

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
// the Join game page's row; the Open address page's row goes to the same games page, but for a typed address on this network, which uses
// no server, so it asks nothing.
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

	// After a No: Find games goes on to its page with this network's games only, every other way back to the Multiplayer page
	return ServerYesPage_Needed(goTo, ServerYes_NoGoesOn(way) ? goTo : AVPMENU_MULTIPLAYER) ? AVPMENU_SERVER_YES : goTo;
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

void ServerYesPage_Draw(void)
{
	if (!haveShown)
	{
		return;
	}

	// The server's name, large: it comes from servers.cfg, a file from outside the game, so it's made safe before it's drawn
	char name[sizeof(shown.name) + 1];
	OnlineScreens_SafeName(shown.name, sizeof(shown.name), name, sizeof(name));
	RenderMenuText(name, MENU_CENTREX, 60, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	// Where it is, and its key's two ends. The address was written out again from its four numbers, and the key's digits are hex.
	char where[96];
	ServerYes_WhereLine(shown.address, WhereWords(shown.place), where, sizeof(where));
	RenderSmallMenuText(where, MENU_CENTREX, 98, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	char key[64];
	ServerYes_KeyLine(GetTextString(TEXTSTRING_SERVERYES_KEY), shown.keyStart, shown.keyEnd, key, sizeof(key));
	RenderSmallMenuText(key, MENU_CENTREX, 116, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	// What the server's owner does and sees
	int x = 0;
	int y = 0;
	RECT area = { 60, 140, 580, 190 };
	RenderSmallFontString_Wrapped(GetTextString(TEXTSTRING_SERVERYES_OWNER), &area, ONE_FIXED, &x, &y);
}
