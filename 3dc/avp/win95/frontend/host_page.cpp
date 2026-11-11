// The host page (beta.2). Written 2026-11-11 by Melanikus for the AvP port. host_page.h says why, and what it never does;
// host_page_rules.h has its layout and its rules, game-free, which host_page_test checks.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "hud_layout.h"     // HUD_FONT_HEIGHT, the small font's height
#include "language.h"
#include "logstring.h"
#include "relay_list.h"
#include "online_screens.h"        // OnlineScreens_SafeName, OnlineScreens_FitToWidth
#include "online_screens_page.h"   // OnlinePage_SmallWidth, the small font's own letter widths
#include "host_page.h"

#include <stdio.h>
#include <string.h>
#include <string>

// host_page_rules.h's copies of the menus' numbers must be the menus' own: it can't include menus.h's drawing parts, so these checks are
// here, where the game is built
static_assert(kHostPageSmallLine == HUD_FONT_HEIGHT, "the small font's height is HUD_FONT_HEIGHT");
static_assert(kHostPageBigLine == MENU_FONT_HEIGHT, "the big font's height is MENU_FONT_HEIGHT");
static_assert(kHostPageLeft == MENU_LEFTXEDGE && kHostPageRight == MENU_RIGHTXEDGE, "the page's edges are the menus' own");
static_assert(kHostPageHostLineTop == MENU_BOTTOMYEDGE, "a host's line starts at the menus' bottom edge (menus.cpp, DrawTheHostLine)");

namespace
{
	// What the page knows, only on the game's main thread: how many lines it has rows for, each line's players from the find's games, and
	// its answer to the last Enter
	int lineCount = 0;
	int linePlayers[kHostPageMostLines];
	bool playersKnown = false;
	char answer[200];

	// The find's games, kept off the stack (RelayList_FoundAllGames fills up to kRelayListMostAllGames)
	RelayListServerGame gamesFound[kRelayListMostAllGames];

	// The page's words for each state, from the language file. A state with something of its own (a name, a why, a number) has two parts,
	// before and after it (host_page_rules.h, HostPage_WhyLine), so a % in the file is never read as a printf form.
	HostPageWords TheWords(void)
	{
		HostPageWords words = {};
		words.before[kHostPageSaysNoYes] = GetTextString(TEXTSTRING_HOSTPAGE_NOYES);
		words.before[kHostPageSaysRefused] = GetTextString(TEXTSTRING_HOSTPAGE_REFUSED);
		words.before[kHostPageSaysNotApproved] = GetTextString(TEXTSTRING_NOTAPPROVED_BEFORE);
		words.after[kHostPageSaysNotApproved] = GetTextString(TEXTSTRING_NOTAPPROVED_AFTER);
		words.before[kHostPageSaysAsking] = GetTextString(TEXTSTRING_HOSTPAGE_ASKING);
		words.before[kHostPageSaysNoAnswer] = GetTextString(TEXTSTRING_HOSTPAGE_NOANSWER);
		words.before[kHostPageSaysNotAsked] = GetTextString(TEXTSTRING_HOSTPAGE_NOTASKED_BEFORE);
		words.after[kHostPageSaysNotAsked] = GetTextString(TEXTSTRING_HOSTPAGE_NOTASKED_AFTER);
		return words;
	}

	// A row's line: its place, or -1 for the question, or a row that isn't one of the page's lines
	int PlaceOf(int row)
	{
		int const place = row - kHostPageLineRowFirst;
		return place >= 0 && place < lineCount ? place : -1;
	}

	// What a line's row says now, read from relay_list's own answer for it. False for a place it doesn't know.
	bool LineSays(int place, RelayListLineNow *now, HostPageSays *says)
	{
		if (place < 0 || !RelayList_LineNow(place, now))
		{
			return false;
		}

		*says = HostPage_Says(now->state, now->access);
		return true;
	}

	// The line's name, made safe to draw: it comes from servers.cfg, a file from outside the game
	void SafeNameOf(const RelayListLineNow &now, char *name, size_t room)
	{
		OnlineScreens_SafeName(now.line.name, sizeof(now.line.name), name, room);
	}

	// The chosen line's place, from the menus' chosen row: the rows are the question (0), then each line in order
	int ChosenPlace(int chosenElement)
	{
		return chosenElement >= 1 ? chosenElement - 1 : -1;
	}

	// One cell, cut to its column's width by the font's own letter widths, so it never reaches the next column
	void Cell(const char *text, HostPageColumn column, int y, int alpha)
	{
		char fitted[200];
		snprintf(fitted, sizeof(fitted), "%s", text ? text : "");
		OnlineScreens_FitToWidth(fitted, column.width - 6, OnlinePage_SmallWidth);
		RenderSmallMenuText(fitted, column.x, y, alpha, AVPMENUFORMAT_LEFTJUSTIFIED);
	}

	// Whether any line can be chosen now, or is still being asked: for the page's word when none can
	bool AnyLineIs(HostPageSays wanted)
	{
		for (int place = 0; place < lineCount; place++)
		{
			RelayListLineNow now;
			HostPageSays says;

			if (LineSays(place, &now, &says) && says == wanted)
			{
				return true;
			}
		}

		return false;
	}
}

bool HostPage_Opened(AVPMENU_ELEMENT *rows, int room)
{
	if (!rows || room < kHostPageRowRoom)
	{
		return false;
	}

	// Nothing is chosen for the player, ever: the page opens with no line to host on (the owner: "No auto choice, they choose")
	(void)RelayList_HostOn(-1);

	int const count = RelayList_LineCount();
	lineCount = count < 0 ? 0 : count > kHostPageMostLines ? kHostPageMostLines : count;
	memset(linePlayers, 0, sizeof(linePlayers));
	playersKnown = false;
	answer[0] = 0;

	// Every line is asked again, so each row's round trip, games and region are this moment's
	RelayList_FindGames();

	// The question first, where the cursor starts: Enter there chooses nothing. Then a row for each line, then the end.
	AVPMENU_ELEMENT const question = {AVPMENU_ELEMENT_HOSTLINE, {TEXTSTRING_HOSTPAGE_CHOOSE}, {static_cast<uint32_t>(kHostPageChooseRow)}, {0}, {0},
		TEXTSTRING_HOSTPAGE_CHOOSE_HELP};
	rows[0] = question;

	for (int place = 0; place < lineCount; place++)
	{
		AVPMENU_ELEMENT const line = {AVPMENU_ELEMENT_HOSTLINE, {TEXTSTRING_BLANK}, {static_cast<uint32_t>(kHostPageLineRowFirst + place)}, {0},
			{0}, TEXTSTRING_HOSTPAGE_LINE_HELP};
		rows[1 + place] = line;
	}

	AVPMENU_ELEMENT const end = {AVPMENU_ELEMENT_ENDOFMENU};
	rows[1 + lineCount] = end;
	LogString("host page: opened with no server chosen; every line is asked again");
	return true;
}

void HostPage_Frame(void)
{
	int count = 0;

	if (!RelayList_FoundAllGames(gamesFound, &count))
	{
		return;
	}

	int const kept = count < 0 ? 0 : count > kRelayListMostAllGames ? kRelayListMostAllGames : count;
	memset(linePlayers, 0, sizeof(linePlayers));

	// Each game's players, as its host said, counted for the line it was listed on: the list's code wrote that line from its own visit,
	// never from anything the server sent (relay_list.h, RelayListServerGame)
	for (int i = 0; i < kept; i++)
	{
		int const line = gamesFound[i].line;

		if (line >= 0 && line < lineCount)
		{
			linePlayers[line] = HostPage_AddPlayers(linePlayers[line], gamesFound[i].game.players);
		}
	}

	playersKnown = true;
}

bool HostPage_PressedRow(int row)
{
	answer[0] = 0;

	if (row == kHostPageChooseRow)
	{
		snprintf(answer, sizeof(answer), "%s", GetTextString(TEXTSTRING_HOSTPAGE_CHOOSE_BELOW));
		return false;
	}

	int const place = PlaceOf(row);
	RelayListLineNow now;
	HostPageSays says;

	if (!LineSays(place, &now, &says))
	{
		return false;
	}

	char name[kServersCfgNameLetters + 1];
	SafeNameOf(now, name, sizeof(name));

	switch (HostPage_Pressed(false, says))
	{
		case kHostPageChooseIt:
			// THE ONE PLACE A PLAYER'S GAME CHOOSES ITS LINE (release check rule H): only here, only for a line that can be hosted on, and
			// only because the player pressed it. RelayList_HostOn refuses it too, if the line lost its yes or its approval meanwhile.
			if (RelayList_HostOn(place))
			{
				LogString("host page: the player chose servers.cfg's good line " + std::to_string(place + 1) + " to host on");
				return true;
			}

			snprintf(answer, sizeof(answer), "%s", GetTextString(TEXTSTRING_HOSTPAGE_CANT));
			return false;

		case kHostPageAskAgain:
			// Enter on a server that didn't answer is how a player tries again: every line is asked, and the players come again with them
			RelayList_FindGames();
			playersKnown = false;
			snprintf(answer, sizeof(answer), "%s", GetTextString(TEXTSTRING_HOSTPAGE_ASKED_AGAIN));
			return false;

		default:
			HostPage_WhyLine(TheWords(), says, name, now.why, answer, sizeof(answer));
			return false;
	}
}

void HostPage_DrawTop(int chosenElement)
{
	// The column headings
	TEXTSTRING_ID const headings[kHostPageColumns] = { TEXTSTRING_HOSTPAGE_SERVER, TEXTSTRING_HOSTPAGE_PING, TEXTSTRING_HOSTPAGE_GAMES,
		TEXTSTRING_HOSTPAGE_PLAYERS, TEXTSTRING_HOSTPAGE_REGION };

	for (int c = 0; c < kHostPageColumns; c++)
	{
		Cell(GetTextString(headings[c]), HostPage_Column(c), kHostPageHeadingsTop, ONE_FIXED);
	}

	// Which lines are shown, when they don't all fit
	char count[48];
	HostPage_CountLine(lineCount, HostPage_View(lineCount, ChosenPlace(chosenElement)), count, sizeof(count));

	if (count[0])
	{
		RenderSmallMenuText(count, MENU_CENTREX, kHostPageCountTop, ONE_FIXED / 2, AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	// The answer to the last Enter; or, once every line has answered and none can be hosted on, where to go
	const char *words = answer;

	if (!words[0] && !AnyLineIs(kHostPageSaysCanHost) && !AnyLineIs(kHostPageSaysAsking))
	{
		words = GetTextString(TEXTSTRING_HOSTPAGE_NONE);
	}

	if (words[0])
	{
		char text[200];
		snprintf(text, sizeof(text), "%s", words);
		RECT area = { kHostPageLeft, kHostPageWordsTop, kHostPageRight, kHostPageWordsTop + kHostPageWordsLines * kHostPageSmallLine };
		RenderSmallFontString_Wrapped(text, &area, ONE_FIXED, 0, 0);
	}
}

void HostPage_DrawRow(const AVPMENU_ELEMENT *row, int brightness, int chosenElement)
{
	if (!row)
	{
		return;
	}

	// The question, on its own row, centred
	if (static_cast<int>(row->MaxValue) == kHostPageChooseRow)
	{
		RenderSmallMenuText(GetTextString(TEXTSTRING_HOSTPAGE_CHOOSE), MENU_CENTREX, kHostPageChooseTop, brightness, AVPMENUFORMAT_CENTREJUSTIFIED);
		return;
	}

	int const place = PlaceOf(static_cast<int>(row->MaxValue));
	int const top = HostPage_RowTop(place, HostPage_View(lineCount, ChosenPlace(chosenElement)));
	RelayListLineNow now;
	HostPageSays says;

	if (top < 0 || !LineSays(place, &now, &says))
	{
		return;
	}

	char name[kServersCfgNameLetters + 1];
	SafeNameOf(now, name, sizeof(name));

	// Every line: its name and its own ping, whether or not it has games, and whether or not it can be chosen (modified 2026-11-11 by
	// Melanikus for the AvP port, the owner's "We should show the server ping to the user if no games are up"). A line that can't be chosen
	// is dimmed.
	int const light = says == kHostPageSaysCanHost ? brightness : brightness / 2;
	char ping[48];
	HostPage_PingWords(now.pingMs, now.state, GetTextString(TEXTSTRING_HOSTPAGE_MS), GetTextString(TEXTSTRING_PING_ASKING), ping, sizeof(ping));
	Cell(name, HostPage_Column(kHostPageColumnServer), top, light);
	Cell(ping, HostPage_Column(kHostPageColumnPing), top, light);

	// A line that can be chosen: then its games, their players and its region
	if (says == kHostPageSaysCanHost)
	{
		// The region is the server's own word: made safe before it's drawn, as everything a server sends is
		char region[kRelayListRegionLetters + 1];
		OnlineScreens_SafeName(now.region, sizeof(now.region), region, sizeof(region));
		HostPageCells const cells = HostPage_Cells(now.games, linePlayers[place], playersKnown, region);
		Cell(cells.games, HostPage_Column(kHostPageColumnGames), top, light);
		Cell(cells.players, HostPage_Column(kHostPageColumnPlayers), top, light);
		Cell(cells.region, HostPage_Column(kHostPageColumnRegion), top, light);
		return;
	}

	// Any other line: why it can't be chosen, across the columns after its ping
	char why[200];
	HostPage_WhyLine(TheWords(), says, name, now.why, why, sizeof(why));
	Cell(why, HostPage_WhyColumn(), top, light);
}

int HostPage_RowNamed(const char *name)
{
	if (!name || strlen(name) > kServersCfgNameLetters)
	{
		return -1;
	}

	for (int place = 0; place < lineCount; place++)
	{
		RelayListLineNow now;

		if (RelayList_LineNow(place, &now) && strncmp(now.line.name, name, sizeof(now.line.name)) == 0)
		{
			return kHostPageLineRowFirst + place;
		}
	}

	return -1;
}

bool HostPage_RowCanBeChosen(int row)
{
	RelayListLineNow now;
	HostPageSays says;
	return LineSays(PlaceOf(row), &now, &says) && says == kHostPageSaysCanHost;
}
