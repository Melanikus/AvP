// The Servers page's rules, with nothing of the game's in them, so a test can try every case (servers_page_test). Written 2026-11-10 by
// Melanikus for the AvP port (#175 2b, PO's ruling A). servers_page.h says what the page is for.
//
// WHERE THINGS GO, top to bottom, on the menus' 640 by 480 grid, under the title (drawn at 70):
//   the servers, at most kServersPageMostShown at a time, each two small lines: its name and what's true of it, then its address and key;
//   when there are more than that, which of them are shown ("6-10 / 64"). The page shows them a screenful at a time, and the screenful
//   follows the chosen row, so every server can be reached;
//   the lines of servers.cfg the game doesn't use, each by its number and the game's own words, at most kServersPageMostRefused lines.
//   With more, the last says how many more there are: the game's log names every one;
//   the pick's answer; then the Back row, above the help line (which starts at 420).
// Nothing is ever drawn past those: servers_page_test tries every number of servers and refused lines up to 64, and every chosen row.

#ifndef SERVERS_PAGE_RULES_H
#define SERVERS_PAGE_RULES_H

#include <stddef.h>
#include <stdio.h>

int const kServersPageMostLines = 64;        // servers.cfg's own limit (servers_cfg.h): a line more than that is never read
int const kServersPageRowRoom = kServersPageMostLines + 2;   // the page's rows: a row for each server, the Back row and the end
int const kServersPageRowFirst = 200;        // a server row's MaxValue is this plus its place: far from the access page's and the yes page's

int const kServersPageSmallLine = 15;        // the small font's height (HUD_FONT_HEIGHT; servers_page.cpp checks it)
int const kServersPageBigLine = 21;          // the big font's (MENU_FONT_HEIGHT)
int const kServersPageRowsTop = 100;         // the first server, under the title
int const kServersPageRowHeight = 2 * kServersPageSmallLine + 6;   // a server's two lines and a gap
int const kServersPageMostShown = 5;
int const kServersPageCountTop = kServersPageRowsTop + kServersPageMostShown * kServersPageRowHeight + 2;
int const kServersPageRefusedTop = kServersPageCountTop + kServersPageSmallLine + 3;
int const kServersPageMostRefused = 4;
int const kServersPageStatusTop = kServersPageRefusedTop + kServersPageMostRefused * kServersPageSmallLine + 2;
int const kServersPageBackTop = kServersPageStatusTop + kServersPageSmallLine + 3;
int const kServersPageHelpTop = 420;         // where the menus' help line starts
int const kServersPageLeft = 30;             // the menus' edges (MENU_LEFTXEDGE, MENU_RIGHTXEDGE)
int const kServersPageRight = 610;

static_assert(kServersPageCountTop + kServersPageSmallLine <= kServersPageRefusedTop, "the count's line ends before the refused lines start");
static_assert(kServersPageStatusTop + kServersPageSmallLine <= kServersPageBackTop, "the pick's answer ends before the Back row");
static_assert(kServersPageBackTop + kServersPageBigLine <= kServersPageHelpTop, "the Back row ends before the help line starts");

// Which servers are shown: a screenful of at most kServersPageMostShown, the one that holds the chosen row. A chosen row past the servers
// (the Back row) shows the last screenful. With no servers, none.
struct ServersPageView
{
	int first;   // the first server shown, counted from 0
	int shown;   // how many, from it
};

inline ServersPageView ServersPage_View(int count, int chosen)
{
	ServersPageView view = { 0, 0 };

	if (count <= 0)
	{
		return view;
	}

	if (count > kServersPageMostLines)
	{
		count = kServersPageMostLines;
	}

	int const place = chosen < 0 ? 0 : chosen >= count ? count - 1 : chosen;
	view.first = (place / kServersPageMostShown) * kServersPageMostShown;
	view.shown = count - view.first < kServersPageMostShown ? count - view.first : kServersPageMostShown;
	return view;
}

// Where a server's row starts, or -1 when it isn't in the screenful shown
inline int ServersPage_RowTop(int place, ServersPageView view)
{
	if (place < view.first || place >= view.first + view.shown)
	{
		return -1;
	}

	return kServersPageRowsTop + (place - view.first) * kServersPageRowHeight;
}

// How many refused lines are shown by their number and words: all of them, up to kServersPageMostRefused; with more, one fewer, and the
// last line says how many more there are
inline int ServersPage_RefusedShown(int refused)
{
	if (refused <= 0)
	{
		return 0;
	}

	return refused <= kServersPageMostRefused ? refused : kServersPageMostRefused - 1;
}

// A server's first line: its name, then what's true of it, as "New York: in use, yes given". "Used from the next start" only when that
// isn't the line in use. The name must have been made safe already; the words are the game's own. Cut short at out's end, always ended.
inline void ServersPage_NameLine(const char *safeName, bool inUse, bool nextStart, bool agreed, const char *inUseWords, const char *nextWords,
	const char *agreedWords, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	const char *notes[3];
	int count = 0;

	if (inUse)
	{
		notes[count++] = inUseWords;
	}

	if (nextStart && !inUse)
	{
		notes[count++] = nextWords;
	}

	if (agreed)
	{
		notes[count++] = agreedWords;
	}

	const char *const name = safeName ? safeName : "";

	if (count == 0)
	{
		snprintf(out, room, "%s", name);
	}
	else if (count == 1)
	{
		snprintf(out, room, "%s: %s", name, notes[0] ? notes[0] : "");
	}
	else if (count == 2)
	{
		snprintf(out, room, "%s: %s, %s", name, notes[0] ? notes[0] : "", notes[1] ? notes[1] : "");
	}
	else
	{
		snprintf(out, room, "%s: %s, %s, %s", name, notes[0] ? notes[0] : "", notes[1] ? notes[1] : "", notes[2] ? notes[2] : "");
	}

	out[room - 1] = 0;
}

// A refused line, as "servers.cfg line 4: its fingerprint isn't 64 small hex digits": the line's number and the game's own words for why,
// never anything of the line itself
inline void ServersPage_RefusedLine(const char *lineWords, int lineNumber, const char *why, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	snprintf(out, room, "%s %d: %s", lineWords ? lineWords : "", lineNumber, why ? why : "");
	out[room - 1] = 0;
}

// Which servers the screenful shows, as "6-10 / 64", only when they don't all fit: otherwise nothing
inline void ServersPage_CountLine(int count, ServersPageView view, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	out[0] = 0;

	if (count > kServersPageMostShown && view.shown > 0)
	{
		snprintf(out, room, "%d-%d / %d", view.first + 1, view.first + view.shown, count);
		out[room - 1] = 0;
	}
}

#endif
