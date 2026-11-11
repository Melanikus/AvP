// The Online page (beta.2). Written 2026-11-11 by Melanikus for the AvP port. online_page.h says why there's one page for every
// servers.cfg line and what Enter does; online_page_rules.h has its layout, its words and its decisions, game-free, and online_page_test
// tries them. This is the part that needs the game: the rows, the drawing, the clipboard, and this run's identity (networking.h).

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "hud_layout.h"         // HUD_FONT_HEIGHT, the small font's height
#include "language.h"
#include "user_profile.h"
#include "relay_list.h"
#include "net_identity_run.h"   // this run's identity, from networking.cpp
#include "server_yes_page.h"    // the yes page, opened for one line
#include "online_page.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

// The names the screens make safe before drawing them (online_screens.cpp): a letter the game's font doesn't have shows as ?
void OnlineScreens_SafeName(const char *from, size_t mostBytes, char *to, size_t toSize);

// online_page_rules.h's copies of the menus' numbers must be the menus' own: it can't include menus.h, so these checks are here, where
// the game is built
static_assert(kOnlinePageSmallLine == HUD_FONT_HEIGHT, "the small font's height is HUD_FONT_HEIGHT");
static_assert(kOnlinePageBigLine == MENU_FONT_HEIGHT, "the big font's height is MENU_FONT_HEIGHT");
static_assert(kOnlinePageLeft == MENU_LEFTXEDGE && kOnlinePageRight == MENU_RIGHTXEDGE, "the page's edges are the menus' own");
static_assert(kOnlinePageMostLines == kServersCfgMostLines, "a row for every line servers.cfg can have");

char OnlinePageName[kRelayAccessNameMostBytes + 1];

namespace
{
	// What the page knows. Only the game's main thread uses these.
	int lineCount = 0;                              // the good lines, read as the page opens (they're fixed for the run)
	bool askedThisRun[kOnlinePageMostLines];        // this run has sent the line's server a request, for this identity
	uint32_t lastAskedAt[kOnlinePageMostLines];     // when (GetTickCount)
	bool nameStarted = false;                       // the profile's name has been put in the name row, once this run
	TEXTSTRING_ID answer = TEXTSTRING_BLANK;        // the page's answer to the last thing pressed, under the code

	bool haveIdentity = false;
	uint8_t ourStatics[kNetIdentityStaticBytes];
	uint8_t ourFingerprint[kNetIdentityFingerprintBytes];
	char ourCode[kRelayAccessCodeTextBytes];

	uint32_t Now(void)
	{
		return (uint32_t)GetTickCount();
	}

	// This run's identity, for the requests and the code: only its public halves. With none, the answer's line says why. A kept one that
	// couldn't be used waits for the player's yes on the question page: nothing here replaces it.
	void TakeTheIdentity(void)
	{
		haveIdentity = Net_OurIdentity(ourStatics, ourFingerprint);

		if (haveIdentity)
		{
			RelayAccess_Code(ourFingerprint, ourCode);
			return;
		}

		ourCode[0] = '\0';
		NetIdentityStoreResult const kept = Net_KeptIdentityResult();
		bool const keptButUnusable = kept == kNetIdentityStoreRefused || kept == kNetIdentityStoreCantUnseal || kept == kNetIdentityStoreCantRead;
		answer = keptButUnusable ? TEXTSTRING_ACCESS_KEPT_UNUSABLE : TEXTSTRING_ACCESS_NO_IDENTITY;
	}

	// The page's words for a name that can't be sent
	TEXTSTRING_ID NameWords(RelayAccessNameRefusal why)
	{
		switch (why)
		{
			case kRelayAccessNameGood: return TEXTSTRING_BLANK;
			case kRelayAccessNameEmpty: return TEXTSTRING_ACCESS_NAME_EMPTY;
			case kRelayAccessNameTooLong: return TEXTSTRING_ACCESS_NAME_TOO_LONG;
			case kRelayAccessNameBadLetter: return TEXTSTRING_ACCESS_NAME_BAD_LETTER;
			case kRelayAccessNameSpaceAtAnEnd: return TEXTSTRING_ACCESS_NAME_SPACE_AT_END;
		}

		return TEXTSTRING_ACCESS_NAME_BAD_LETTER;
	}

	// What a line's row says now, from what relay_list.cpp says of its line
	OnlinePageSays SaysOf(int place, RelayListLineNow *now)
	{
		if (!RelayList_LineNow(place, now))
		{
			return kOnlinePageSaysNoYes;
		}

		return OnlineLines_Says(now->state, now->access);
	}

	// Sends a line's server the request: the name, and this PC's public halves. relay_list.cpp sends it only after that line's yes, as a
	// visit of its own. True when it went.
	bool Ask(int place)
	{
		if (!haveIdentity)
		{
			TakeTheIdentity();

			if (!haveIdentity)
			{
				return false;
			}
		}

		RelayAccessNameRefusal const refusal = RelayAccess_NameRefusal(OnlinePageName);

		if (refusal != kRelayAccessNameGood)
		{
			answer = NameWords(refusal);
			return false;
		}

		if (!RelayList_AskForAccessOn(place, OnlinePageName, ourStatics, ourFingerprint))
		{
			answer = TEXTSTRING_ACCESS_CANT_REACH;
			return false;
		}

		askedThisRun[place] = true;
		lastAskedAt[place] = Now();
		answer = TEXTSTRING_BLANK;
		return true;
	}

	// The code on the clipboard, as text. Windows owns the memory once it has taken it, and frees it itself.
	bool CopyToClipboard(const char *text)
	{
		size_t const size = strlen(text) + 1;
		HGLOBAL const memory = GlobalAlloc(GMEM_MOVEABLE, size);

		if (!memory)
		{
			return false;
		}

		void *const at = GlobalLock(memory);

		if (!at)
		{
			GlobalFree(memory);
			return false;
		}

		memcpy(at, text, size);
		GlobalUnlock(memory);

		if (!OpenClipboard(NULL))
		{
			GlobalFree(memory);
			return false;
		}

		bool const put = EmptyClipboard() && SetClipboardData(CF_TEXT, memory) != NULL;
		CloseClipboard();

		if (!put)
		{
			GlobalFree(memory);
		}

		return put;
	}

	// A line cut short, with "..." after it, if it's wider than the menus' edges: measured by the font's own widths
	// (SmallFontString_WrappedLines), so nothing is drawn past the edges whatever the font's letters are. The longest start of the line
	// that fits is found by halving.
	void FitToTheEdges(char *line, size_t room)
	{
		int const width = kOnlinePageRight - kOnlinePageLeft;
		size_t const length = strlen(line);

		if (length == 0 || SmallFontString_WrappedLines(line, width) <= 1)
		{
			return;
		}

		char tried[260];
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
		char line[260];
		snprintf(line, sizeof(line), "%s", text ? text : "");
		FitToTheEdges(line, sizeof(line));
		RenderSmallMenuText(line, MENU_CENTREX, y, alpha, AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	// The name to send starts as the profile's, once a run; a letter the rule won't take is left out of it, and the page says so, rather
	// than drop it without a word
	void StartTheName(void)
	{
		if (nameStarted)
		{
			return;
		}

		RelayAccess_NameFromProfile(UserProfilePtr ? UserProfilePtr->Name : NULL, sizeof(UserProfilePtr->Name), OnlinePageName);
		nameStarted = true;

		if (RelayAccess_NameLeftLettersOut(UserProfilePtr ? UserProfilePtr->Name : NULL, sizeof(UserProfilePtr->Name)))
		{
			answer = TEXTSTRING_ACCESS_NAME_LEFT_OUT;
		}
	}

	// How many good lines there are, for the rows (they're fixed for the run)
	void CountTheLines(void)
	{
		int const count = RelayList_LineCount();
		lineCount = count < 0 ? 0 : (count > kOnlinePageMostLines ? kOnlinePageMostLines : count);
	}

	// The game's own words for each state of a line's row (online_page_rules.h, OnlineLines_StateLine)
	OnlinePageWords TheWords(void)
	{
		OnlinePageWords words = {};
		words.before[kOnlinePageSaysNoYes] = GetTextString(TEXTSTRING_ONLINE_NOYES);
		words.before[kOnlinePageSaysRefused] = GetTextString(TEXTSTRING_ONLINE_REFUSED);
		words.before[kOnlinePageSaysAsking] = GetTextString(TEXTSTRING_ACCESS_SENDING);
		words.before[kOnlinePageSaysWaiting] = GetTextString(TEXTSTRING_ONLINE_WAITING);
		words.after[kOnlinePageSaysWaiting] = GetTextString(TEXTSTRING_ONLINE_WAITING_AFTER);
		words.before[kOnlinePageSaysNoAnswer] = GetTextString(TEXTSTRING_ONLINE_NOANSWER);
		words.after[kOnlinePageSaysNoAnswer] = GetTextString(TEXTSTRING_ONLINE_NOANSWER_AFTER);
		words.before[kOnlinePageSaysNotAsked] = GetTextString(TEXTSTRING_ONLINE_NOTASKED);
		words.before[kOnlinePageSaysApprovedGames] = GetTextString(TEXTSTRING_ONLINE_APPROVED_GAMES);
		words.after[kOnlinePageSaysApprovedGames] = GetTextString(TEXTSTRING_ONLINE_APPROVED_GAMES_AFTER);
		words.before[kOnlinePageSaysApprovedFinding] = GetTextString(TEXTSTRING_ONLINE_APPROVED_FINDING);
		words.before[kOnlinePageSaysApprovedNoGames] = GetTextString(TEXTSTRING_ONLINE_APPROVED_NOGAMES);
		words.before[kOnlinePageSaysApprovedNotVisited] = GetTextString(TEXTSTRING_ONLINE_APPROVED_NOTVISITED);
		words.after[kOnlinePageSaysApprovedNotVisited] = GetTextString(TEXTSTRING_ONLINE_APPROVED_NOTVISITED_AFTER);
		return words;
	}

	// Which line a row is for, or -1 for one of the page's own rows
	int PlaceOfRow(int row)
	{
		int const place = row - kOnlinePageLineRowFirst;
		return place >= 0 && place < lineCount ? place : -1;
	}
}

bool OnlineLines_Opened(AVPMENU_ELEMENT *rows, int room)
{
	if (!rows || room < kOnlinePageRowRoom)
	{
		return false;
	}

	CountTheLines();
	StartTheName();
	TakeTheIdentity();

	// A row for each line, the same for every line, then the page's own: the name to send, Copy access code, Make a new identity, Back
	int row = 0;

	for (int place = 0; place < lineCount; place++)
	{
		AVPMENU_ELEMENT const line = {AVPMENU_ELEMENT_ONLINE_ACCESS, {TEXTSTRING_BLANK}, {static_cast<uint32_t>(kOnlinePageLineRowFirst + place)}, {0}, {0},
			TEXTSTRING_ONLINE_LINE_HELP};
		rows[row++] = line;
	}

	AVPMENU_ELEMENT const name = {AVPMENU_ELEMENT_TEXTFIELD, {TEXTSTRING_ACCESS_NAME}, {kRelayAccessNameMostBytes}, {(int*)OnlinePageName}, {0}, TEXTSTRING_ACCESS_NAME_HELP};
	AVPMENU_ELEMENT const copy = {AVPMENU_ELEMENT_ONLINE_ACCESS, {TEXTSTRING_ACCESS_COPY}, {kOnlinePageRowCopy}, {0}, {0}, TEXTSTRING_ACCESS_COPY_HELP};
	AVPMENU_ELEMENT const newIdentity = {AVPMENU_ELEMENT_GOTOMENU, {TEXTSTRING_ACCESS_NEWID}, {AVPMENU_ONLINE_NEW_IDENTITY}, {0}, {0}, TEXTSTRING_ONLINE_NEWID_HELP};
	AVPMENU_ELEMENT const back = {AVPMENU_ELEMENT_GOTOMENU, {TEXTSTRING_SERVERS_BACK}, {AVPMENU_MULTIPLAYER}, {0}, {0}, TEXTSTRING_BLANK};
	AVPMENU_ELEMENT const end = {AVPMENU_ELEMENT_ENDOFMENU};
	rows[row++] = name;
	rows[row++] = copy;
	rows[row++] = newIdentity;
	rows[row++] = back;
	rows[row] = end;

	// PO's ruling, 2026-11-09, kept for each line: a line this run has already asked is asked again as the page opens, since that sends
	// nothing new, only the same name and key. Never a line not asked this run, and never within a minute of its last request.
	uint32_t const now = Now();

	for (int place = 0; place < lineCount; place++)
	{
		RelayListLineNow line;

		if (askedThisRun[place] && OnlineLines_Pressed(SaysOf(place, &line), true, now, lastAskedAt[place]) == kOnlinePageAsk)
		{
			Ask(place);
		}
	}

	return true;
}

// PO's ruling, 2026-11-11 (after the owner's picture run): approval is known from this run's answers alone, nothing is kept, so a player
// approved yesterday is asked again as the Multiplayer page opens, on every line with its yes, by the same request and the same rule as
// a row's Enter: never a line without its yes (its row's Enter opens its yes page instead), never a line refused or being asked, and
// never a line asked less than a minute ago. Only the profile's name and this PC's public halves go, as with every request.
void OnlineLines_AskTheAgreedLines(void)
{
	CountTheLines();
	StartTheName();

	if (!haveIdentity)
	{
		TakeTheIdentity();
	}

	uint32_t const now = Now();

	for (int place = 0; haveIdentity && place < lineCount; place++)
	{
		RelayListLineNow line;
		OnlinePageSays const says = SaysOf(place, &line);

		if (says != kOnlinePageSaysNoYes && OnlineLines_Pressed(says, askedThisRun[place], now, lastAskedAt[place]) == kOnlinePageAsk)
		{
			Ask(place);
		}
	}
}

bool OnlineLines_QuestionOpened(void)
{
	return true;
}

void OnlineLines_Frame(void)
{
	// Each answer that has come: its row reads it from relay_list.cpp as it's drawn (RelayList_LineNow). An approval is found at once
	// (added 2026-11-11 by Melanikus for the AvP port, the owner's "all authorized ones show up"): one find for every answer of the frame.
	int place = -1;
	bool approvedNow = false;

	while (RelayList_AccessCameOn(&place))
	{
		RelayListLineNow line;
		approvedNow = approvedNow || (RelayList_LineNow(place, &line) && line.access == kRelayAccessIsApproved);
	}

	if (approvedNow)
	{
		RelayList_FindGames();
	}
}

bool OnlineLines_Pressed(int row, enum AVPMENU_ID *page)
{
	int const place = PlaceOfRow(row);

	if (place >= 0)
	{
		RelayListLineNow line;
		OnlinePageSays const says = SaysOf(place, &line);

		switch (OnlineLines_Pressed(says, askedThisRun[place], Now(), lastAskedAt[place]))
		{
			case kOnlinePageOpenTheYes:
			{
				// The yes page for this line, which comes back here after a yes or a no
				if (ServerYesPage_OpenFor(place))
				{
					*page = AVPMENU_SERVER_YES;
					return true;
				}

				return false;
			}

			case kOnlinePageAsk: Ask(place); return false;
			case kOnlinePageWaitAMinute: answer = TEXTSTRING_ACCESS_WAIT_A_MINUTE; return false;
			case kOnlinePageNothing: return false;
		}

		return false;
	}

	switch (row)
	{
		case kOnlinePageRowCopy:
		{
			if (!haveIdentity)
			{
				TakeTheIdentity();
			}

			if (haveIdentity)
			{
				answer = CopyToClipboard(ourCode) ? TEXTSTRING_ACCESS_COPIED : TEXTSTRING_ACCESS_NOT_COPIED;
			}

			return false;
		}

		case kOnlineNewIdentityNo:
		{
			*page = AVPMENU_ONLINE;
			return true;
		}

		case kOnlineNewIdentityYes:
		{
			// Every line's answer this run was about the old identity, so they're forgotten first, and every line is "not asked" again. While
			// a request is still on its way, nothing is made: its answer would come back about an identity this PC no longer has.
			if (!RelayList_ForgetAccess())
			{
				answer = TEXTSTRING_ONLINE_NEWID_WAIT;
				*page = AVPMENU_ONLINE;
				return true;
			}

			bool const made = Net_MakeNewIdentity();
			memset(askedThisRun, 0, sizeof(askedThisRun));
			TakeTheIdentity();

			if (haveIdentity)
			{
				answer = made ? TEXTSTRING_ACCESS_NEWID_MADE : TEXTSTRING_ACCESS_NEWID_FAILED;
			}

			*page = AVPMENU_ONLINE;
			return true;
		}
	}

	return false;
}

void OnlineLines_DrawTop(int chosen)
{
	char line[260];

	// This PC's access code, once: the same for every server
	if (haveIdentity && ourCode[0])
	{
		snprintf(line, sizeof(line), "%s: %s %s", GetTextString(TEXTSTRING_ACCESS_YOUR_CODE), ourCode, GetTextString(TEXTSTRING_ONLINE_CODE_SAME));
		SmallLine(line, kOnlinePageCodeTop, ONE_FIXED);
	}

	// The page's answer to the last thing pressed
	if (answer != TEXTSTRING_BLANK)
	{
		SmallLine(GetTextString(answer), kOnlinePageStatusTop, ONE_FIXED);
	}

	// Which lines are shown, when they don't all fit. A chosen row past the lines' (one of the page's own) shows the last screenful.
	OnlinePageView const view = OnlineLines_View(lineCount, chosen);
	OnlineLines_CountLine(lineCount, view, line, sizeof(line));

	if (line[0])
	{
		SmallLine(line, kOnlinePageCountTop, ONE_FIXED / 2);
	}

	// Why no line is used at all, or each line that isn't, in the game's own words, never the line's own text
	const char *const fileWords = RelayList_FileWords();
	int y = kOnlinePageRefusedTop;

	if (fileWords)
	{
		char whole[260];
		snprintf(whole, sizeof(whole), "%s %s", GetTextString(TEXTSTRING_SERVERS_FILE_NOT_USED), fileWords);
		SmallLine(whole, y, ONE_FIXED / 2);
		return;
	}

	int const refused = RelayList_RefusedLineCount();
	int const shown = OnlineLines_RefusedShown(refused);

	for (int i = 0; i < shown; i++)
	{
		int lineNumber = 0;
		const char *why = NULL;

		if (RelayList_RefusedLineAt(i, &lineNumber, &why))
		{
			char safe[120];
			OnlineScreens_SafeName(why, sizeof(safe), safe, sizeof(safe));
			OnlineLines_RefusedLine(GetTextString(TEXTSTRING_SERVERS_LINE), lineNumber, safe, line, sizeof(line));
			SmallLine(line, y, ONE_FIXED / 2);
		}

		y += kOnlinePageSmallLine;
	}

	if (refused > shown)
	{
		snprintf(line, sizeof(line), "%s %d", GetTextString(TEXTSTRING_SERVERS_MORE_NOT_USED), refused - shown);
		SmallLine(line, y, ONE_FIXED / 2);
	}
	else if (lineCount == 0 && refused == 0)
	{
		SmallLine(GetTextString(TEXTSTRING_SERVERS_NONE), y, ONE_FIXED / 2);
	}
}

bool OnlineLines_DrawLineRow(const AVPMENU_ELEMENT *row, int brightness, int chosen)
{
	if (!row || row->ElementID != AVPMENU_ELEMENT_ONLINE_ACCESS)
	{
		return false;
	}

	int const place = PlaceOfRow(static_cast<int>(row->MaxValue));

	if (place < 0)
	{
		return false;
	}

	int const top = OnlineLines_RowTop(place, OnlineLines_View(lineCount, chosen));
	RelayListLineNow now;

	if (top < 0 || !RelayList_LineNow(place, &now))
	{
		return true;
	}

	// Its name, made safe first, as it comes from a file from outside the game; ours or not, from what the game holds; its last round trip
	char name[sizeof(now.line.name) + 1];
	OnlineScreens_SafeName(now.line.name, sizeof(now.line.name), name, sizeof(name));
	char first[200];
	OnlinePagePingSays const pingSays = OnlineLines_PingSays(now.state, now.access);
	const char *const noPing = pingSays == kOnlinePagePingAsking ? GetTextString(TEXTSTRING_ONLINE_PING_ASKING) :
		(pingSays == kOnlinePagePingNoAnswer ? GetTextString(TEXTSTRING_ONLINE_PING_NOANSWER) : "");
	OnlineLines_NameLine(name, now.ours, now.pingMs, GetTextString(TEXTSTRING_ONLINE_OURS), GetTextString(TEXTSTRING_ONLINE_NOT_OURS),
		GetTextString(TEXTSTRING_ONLINE_MS), noPing, first, sizeof(first));
	SmallLine(first, top, brightness);

	// Where it stands, in words. A why is the game's own fixed words (relay_list.cpp), never anything a server sent, and is made safe all
	// the same.
	OnlinePageSays const says = OnlineLines_Says(now.state, now.access);
	const char *const why = says == kOnlinePageSaysNoAnswer ? now.accessWhy : (says == kOnlinePageSaysApprovedNoGames ? now.why : NULL);
	char safeWhy[120];
	OnlineScreens_SafeName(why ? why : "", sizeof(safeWhy), safeWhy, sizeof(safeWhy));
	char second[260];
	OnlineLines_StateLine(TheWords(), says, ourCode, safeWhy, now.games, second, sizeof(second));
	SmallLine(second, top + kOnlinePageSmallLine, brightness);
	return true;
}

int OnlineLines_OwnRowTopOf(const AVPMENU_ELEMENT *row)
{
	if (!row)
	{
		return -1;
	}

	if (row->ElementID == AVPMENU_ELEMENT_TEXTFIELD)
	{
		return OnlineLines_OwnRowTop(kOnlinePageRowName);
	}

	if (row->ElementID == AVPMENU_ELEMENT_ONLINE_ACCESS && row->MaxValue == kOnlinePageRowCopy)
	{
		return OnlineLines_OwnRowTop(kOnlinePageRowCopy);
	}

	if (row->ElementID == AVPMENU_ELEMENT_GOTOMENU && row->MenuToGoTo == AVPMENU_ONLINE_NEW_IDENTITY)
	{
		return OnlineLines_OwnRowTop(kOnlinePageRowNewIdentity);
	}

	if (row->ElementID == AVPMENU_ELEMENT_GOTOMENU)
	{
		return OnlineLines_OwnRowTop(kOnlinePageRowBack);
	}

	return -1;
}

int OnlineLines_DrawQuestion(int menuHeight)
{
	// The question's words under the title, wrapped to the menus' edges, then R-SRV-P4's words, and the two rows right under them
	(void)menuHeight;
	char words[400];
	snprintf(words, sizeof(words), "%s %s", GetTextString(TEXTSTRING_ACCESS_NEWID_EXPLAIN), GetTextString(TEXTSTRING_ONLINE_NEWID_HELP));
	int const width = kOnlinePageRight - kOnlinePageLeft;
	int const lines = SmallFontString_WrappedLines(words, width);
	int const top = kOnlinePageCodeTop;
	int x = 0;
	int y = 0;
	RECT area = { kOnlinePageLeft, top, kOnlinePageRight, top + lines * kOnlinePageSmallLine };
	RenderSmallFontString_Wrapped(words, &area, ONE_FIXED, &x, &y);
	return top + lines * kOnlinePageSmallLine + kOnlinePageBigLine;
}

int OnlineLines_LineCount(void)
{
	return lineCount;
}

bool OnlineLines_AskingOn(int place)
{
	RelayListLineNow now;
	return place >= 0 && place < lineCount && RelayList_LineNow(place, &now) && now.access == kRelayAccessAsking;
}

bool OnlineLines_SaysTheNewIdentityWasMade(void)
{
	return answer == TEXTSTRING_ACCESS_NEWID_MADE;
}

bool OnlineLines_NeedsTheYes(int place)
{
	RelayListLineNow now;
	return place >= 0 && place < lineCount && RelayList_LineNow(place, &now) && now.state == kRelayLineNoYes;
}
