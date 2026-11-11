// The yes page: the player's yes to one servers.cfg line. Written 2026-11-10 by Melanikus for the AvP port (#175's last part).
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
	// The line the page is for: its place among servers.cfg's good lines (modified 2026-11-11 by Melanikus for the AvP port: one model
	// for every line), or -1 for none
	int linePlace = -1;

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

bool ServerYesPage_OpenFor(int place)
{
	RelayListLineToAgree waiting;

	if (!RelayList_LineToAgreeAt(place, &waiting))
	{
		return false;
	}

	linePlace = place;
	return true;
}

bool ServerYesPage_Opened(void)
{
	haveShown = linePlace >= 0 && RelayList_LineToAgreeAt(linePlace, &shown);
	return haveShown;
}

bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page)
{
	switch (ServerYes_Choice(row))
	{
		case kServerYesAgreeAndGoOn:
			// The yes is kept, and from now on that line's server may be asked; the Online page's row for it then says what's next
			RelayList_AgreeToTheLineAt(linePlace);
			linePlace = -1;
			*page = AVPMENU_ONLINE;
			return true;

		case kServerYesGoBack:
			linePlace = -1;
			*page = AVPMENU_ONLINE;
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
