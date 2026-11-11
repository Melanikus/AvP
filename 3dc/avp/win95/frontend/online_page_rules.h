// The Online page's rules, with nothing of the game's in them, so a test can try every case (online_page_test). Written 2026-11-11 by
// Melanikus for the AvP port (beta.2, the owner's "UNIFY THE UI" and "SAME CODE, FOR 2 OR MORE SERVERS"). online_page.h says what the
// page is for.
//
// ONE PAGE, EVERY LINE THE SAME WAY. Every servers.cfg line the game can use has a row here, the lines the game ships with too, and every
// row is decided by the same functions below from what relay_list.cpp says of its line (RelayList_LineNow): nothing here asks which line
// it is, so whatever is true of one row is true of every row.
//
// WHERE THINGS GO, top to bottom, on the menus' 640 by 480 grid, under the title (drawn at 70):
//   this PC's access code, once: it's made from this PC's one identity, so it's the same for every server;
//   the lines, at most kOnlinePageMostShown at a time, each two small lines: its name and what's true of it, then where its row stands,
//   in words. The page shows them a screenful at a time, and the screenful follows the chosen row, so every line can be reached;
//   when there are more than that, which of them are shown ("5-8 / 9");
//   the lines of servers.cfg the game doesn't use, each by its number and the game's own words, at most kOnlinePageMostRefused lines.
//   With more, the last says how many more there are: the game's log names every one;
//   then the page's own rows (the name to send, Copy access code, Make a new identity, Back), above the help line (which starts at 420).
// Nothing is ever drawn past those: online_page_test tries every number of lines and refused lines up to 64, and every chosen row.

#ifndef ONLINE_PAGE_RULES_H
#define ONLINE_PAGE_RULES_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "relay_list.h"   // what a line's row is read from: kRelayLine... (the find's) and kRelayAccess... (access's)

int const kOnlinePageMostLines = 64;          // servers.cfg's own limit (servers_cfg.h): a line more than that is never read
int const kOnlinePageLineRowFirst = 300;      // a line row's MaxValue is this plus its place: far from every other page's rows

int const kOnlinePageSmallLine = 15;          // the small font's height (HUD_FONT_HEIGHT; online_page.cpp checks it)
int const kOnlinePageBigLine = 21;            // the big font's (MENU_FONT_HEIGHT)
int const kOnlinePageTitleTop = 70;           // every page's title (menus.cpp, RenderMenu)
int const kOnlinePageCodeTop = kOnlinePageTitleTop + kOnlinePageBigLine + 5;
int const kOnlinePageRowsTop = kOnlinePageCodeTop + kOnlinePageSmallLine + 5;
int const kOnlinePageRowHeight = 2 * kOnlinePageSmallLine + 6;   // a line's two small lines and a gap
int const kOnlinePageMostShown = 4;
int const kOnlinePageCountTop = kOnlinePageRowsTop + kOnlinePageMostShown * kOnlinePageRowHeight + 2;
int const kOnlinePageRefusedTop = kOnlinePageCountTop + kOnlinePageSmallLine + 3;
int const kOnlinePageMostRefused = 2;
int const kOnlinePageOwnRowsTop = kOnlinePageRefusedTop + kOnlinePageMostRefused * kOnlinePageSmallLine + 4;
int const kOnlinePageOwnRows = 4;             // the name to send, Copy access code, Make a new identity, Back
int const kOnlinePageOwnRowHeight = kOnlinePageBigLine + 3;
int const kOnlinePageHelpTop = 420;           // where the menus' help line starts
int const kOnlinePageLeft = 30;               // the menus' edges (MENU_LEFTXEDGE, MENU_RIGHTXEDGE)
int const kOnlinePageRight = 610;

static_assert(kOnlinePageCodeTop >= kOnlinePageTitleTop + kOnlinePageBigLine, "the code's line starts under the title");
static_assert(kOnlinePageRowsTop >= kOnlinePageCodeTop + kOnlinePageSmallLine, "the first line's row starts under the code's line");
static_assert(kOnlinePageCountTop + kOnlinePageSmallLine <= kOnlinePageRefusedTop, "the count's line ends before the refused lines start");
static_assert(kOnlinePageRefusedTop + kOnlinePageMostRefused * kOnlinePageSmallLine <= kOnlinePageOwnRowsTop, "the refused lines end before the page's own rows");
static_assert(kOnlinePageOwnRowsTop + kOnlinePageOwnRows * kOnlinePageOwnRowHeight <= kOnlinePageHelpTop, "the page's own rows end before the help line starts");
static_assert(kOnlinePageLineRowFirst + kOnlinePageMostLines < 1000, "a line row's MaxValue stays a small number");

// The page's own rows, after the lines' rows: their MaxValue
enum
{
	kOnlinePageRowName = 0,      // the name sent with an access request: the profile's, which the player may change
	kOnlinePageRowCopy = 1,      // Copy access code: the one code, as there's one identity
	kOnlinePageRowNewIdentity = 2,   // Make a new identity: ONCE, here, never on a line's row (R-SRV-P4); it asks first, on a page of its own
	kOnlinePageRowBack = 3
};

// Which lines are shown: a screenful of at most kOnlinePageMostShown, the one that holds the chosen line. A chosen row past the lines
// (one of the page's own) shows the last screenful. With no lines, none.
struct OnlinePageView
{
	int first;   // the first line shown, counted from 0
	int shown;   // how many, from it
};

inline OnlinePageView OnlinePage_View(int count, int chosen)
{
	OnlinePageView view = { 0, 0 };

	if (count <= 0)
	{
		return view;
	}

	if (count > kOnlinePageMostLines)
	{
		count = kOnlinePageMostLines;
	}

	int const place = chosen < 0 ? 0 : chosen >= count ? count - 1 : chosen;
	view.first = (place / kOnlinePageMostShown) * kOnlinePageMostShown;
	view.shown = count - view.first < kOnlinePageMostShown ? count - view.first : kOnlinePageMostShown;
	return view;
}

// Where a line's row starts, or -1 when it isn't in the screenful shown
inline int OnlinePage_RowTop(int place, OnlinePageView view)
{
	if (place < view.first || place >= view.first + view.shown)
	{
		return -1;
	}

	return kOnlinePageRowsTop + (place - view.first) * kOnlinePageRowHeight;
}

// Where one of the page's own rows starts
inline int OnlinePage_OwnRowTop(int row)
{
	int const at = row < 0 ? 0 : row >= kOnlinePageOwnRows ? kOnlinePageOwnRows - 1 : row;
	return kOnlinePageOwnRowsTop + at * kOnlinePageOwnRowHeight;
}

// How many refused lines are shown by their number and words: all of them, up to kOnlinePageMostRefused; with more, one fewer, and the
// last line says how many more there are
inline int OnlinePage_RefusedShown(int refused)
{
	if (refused <= 0)
	{
		return 0;
	}

	return refused <= kOnlinePageMostRefused ? refused : kOnlinePageMostRefused - 1;
}

// WHAT A LINE'S ROW SAYS, from what relay_list.cpp says of its line, the first that holds. The yes comes first, for every line, the ones
// the game ships with too (R-SRV-9: "Every line asks the player once at first use"); then access, as a line its owner hasn't let this PC
// into can't be played on; then, once it's approved, what the last find found there.
enum OnlinePageSays
{
	kOnlinePageSaysNoYes,             // no yes yet: Enter shows the line and asks
	kOnlinePageSaysRefused,           // its owner refused this PC, or let it in and then took it back: it stays so
	kOnlinePageSaysAsking,            // an access request is on its way
	kOnlinePageSaysWaiting,           // the server has the request, and its owner hasn't approved it yet: the code to give them
	kOnlinePageSaysNoAnswer,          // the request got no answer it could use: why
	kOnlinePageSaysNotAsked,          // the yes is given, and no request has been sent this run
	kOnlinePageSaysApprovedGames,     // approved, and the last find found these games there
	kOnlinePageSaysApprovedFinding,   // approved, and a find is asking it
	kOnlinePageSaysApprovedNoGames,   // approved, and the last find got no answer from it: why
	kOnlinePageSaysApprovedNotVisited // approved, and the last find didn't ask it (past kRelayListMostVisits lines)
};

inline OnlinePageSays OnlinePage_Says(int lineState, int access)
{
	if (lineState == kRelayLineNoYes)
	{
		return kOnlinePageSaysNoYes;
	}

	if (access == kRelayAccessIsRefused)
	{
		return kOnlinePageSaysRefused;
	}

	if (access == kRelayAccessAsking)
	{
		return kOnlinePageSaysAsking;
	}

	if (access == kRelayAccessWaitingApproval)
	{
		return kOnlinePageSaysWaiting;
	}

	if (access == kRelayAccessNoAnswer)
	{
		return kOnlinePageSaysNoAnswer;
	}

	if (access != kRelayAccessIsApproved)
	{
		return kOnlinePageSaysNotAsked;
	}

	if (lineState == kRelayLineAnswered)
	{
		return kOnlinePageSaysApprovedGames;
	}

	if (lineState == kRelayLineNoAnswer)
	{
		return kOnlinePageSaysApprovedNoGames;
	}

	if (lineState == kRelayLineNotAsked)
	{
		return kOnlinePageSaysApprovedNotVisited;
	}

	return kOnlinePageSaysApprovedFinding;
}

// WHAT ENTER DOES on a line's row, the same for every line: the yes first (the yes page, for that line), then an access request, sent at
// most once a minute for each line, as one press of Enter after another mustn't send one request after another
enum OnlinePagePress
{
	kOnlinePageOpenTheYes,   // open the yes page for this line
	kOnlinePageAsk,          // send this line an access request
	kOnlinePageWaitAMinute,  // a request went to this line less than a minute ago: nothing is sent, and the row says so
	kOnlinePageNothing       // a request is on its way, or the line's owner refused this PC
};

uint32_t const kOnlinePageAskEveryMs = 60 * 1000;

inline OnlinePagePress OnlinePage_Pressed(OnlinePageSays says, bool askedThisRun, uint32_t now, uint32_t lastAskedAt)
{
	if (says == kOnlinePageSaysNoYes)
	{
		return kOnlinePageOpenTheYes;
	}

	if (says == kOnlinePageSaysAsking || says == kOnlinePageSaysRefused)
	{
		return kOnlinePageNothing;
	}

	// The clock's wrap is harmless: an unsigned difference is the time gone, whatever the two numbers
	if (askedThisRun && static_cast<uint32_t>(now - lastAskedAt) < kOnlinePageAskEveryMs)
	{
		return kOnlinePageWaitAMinute;
	}

	return kOnlinePageAsk;
}

// The words each state is said with: the game's own, filled in by online_page.cpp (GetTextString), so a test can give its own. A state
// with something of its own in it (the code, the why, how many games, or kRelayListMostVisits) has it put between its two parts, before
// and after. The words are never used as a printf form: the text file is outside the code, so a % in it is only a letter (Tao 3).
struct OnlinePageWords
{
	const char *before[kOnlinePageSaysApprovedNotVisited + 1];
	const char *after[kOnlinePageSaysApprovedNotVisited + 1];
};

// A line's second line: its state in words. code is this PC's access code; why the game's own fixed words for a line that didn't answer
// (RelayList_LineNow's why or accessWhy, never anything a server sent); games how many of the last find's games are the line's. Cut short
// at out's end, always ended.
inline void OnlinePage_StateLine(const OnlinePageWords &words, OnlinePageSays says, const char *code, const char *why, int games, char *out,
	size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	int const at = says < 0 ? 0 : says > kOnlinePageSaysApprovedNotVisited ? kOnlinePageSaysApprovedNotVisited : says;
	const char *const before = words.before[at] ? words.before[at] : "";
	const char *const after = words.after[at] ? words.after[at] : "";

	if (at == kOnlinePageSaysWaiting)
	{
		snprintf(out, room, "%s%s%s", before, code ? code : "", after);
	}
	else if (at == kOnlinePageSaysNoAnswer || at == kOnlinePageSaysApprovedNoGames)
	{
		snprintf(out, room, "%s%s%s", before, why ? why : "", after);
	}
	else if (at == kOnlinePageSaysApprovedGames)
	{
		snprintf(out, room, "%s%d%s", before, games < 0 ? 0 : games, after);
	}
	else if (at == kOnlinePageSaysApprovedNotVisited)
	{
		snprintf(out, room, "%s%d%s", before, kRelayListMostVisits, after);
	}
	else
	{
		snprintf(out, room, "%s%s", before, after);
	}

	out[room - 1] = 0;
}

// A line's first line: its name (made safe already), then whether it's one of the servers the game ships with, then its last round trip,
// as "New York (ours) - 42 ms" or "Friends' server (not ours) - no ping yet". The ours/not ours label is the one way the rows differ by
// which line it is, and it comes from what the game holds, never from the file (RelayList_LineNow's ours). msWords follows the number
// ("ms"); as above, no words are ever a printf form.
inline void OnlinePage_NameLine(const char *safeName, bool ours, int pingMs, const char *oursWords, const char *notOursWords,
	const char *msWords, const char *noPingWords, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	char ping[48];

	if (pingMs >= 0)
	{
		snprintf(ping, sizeof(ping), "%d %s", pingMs, msWords ? msWords : "");
	}
	else
	{
		snprintf(ping, sizeof(ping), "%s", noPingWords ? noPingWords : "");
	}

	ping[sizeof(ping) - 1] = 0;
	snprintf(out, room, "%s (%s) - %s", safeName ? safeName : "", ours ? (oursWords ? oursWords : "") : (notOursWords ? notOursWords : ""), ping);
	out[room - 1] = 0;
}

// A refused line, as "servers.cfg line 4: its fingerprint isn't 64 small hex digits": the line's number and the game's own words for why,
// never anything of the line itself
inline void OnlinePage_RefusedLine(const char *lineWords, int lineNumber, const char *why, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	snprintf(out, room, "%s %d: %s", lineWords ? lineWords : "", lineNumber, why ? why : "");
	out[room - 1] = 0;
}

// Which lines the screenful shows, as "5-8 / 9", only when they don't all fit: otherwise nothing
inline void OnlinePage_CountLine(int count, OnlinePageView view, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	out[0] = 0;

	if (count > kOnlinePageMostShown && view.shown > 0)
	{
		snprintf(out, room, "%d-%d / %d", view.first + 1, view.first + view.shown, count);
		out[room - 1] = 0;
	}
}

#endif
