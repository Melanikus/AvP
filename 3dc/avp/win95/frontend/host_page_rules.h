// The host page's rules, with nothing of the game's in them, so a test can try every case (host_page_test). Written 2026-11-11 by
// Melanikus for the AvP port (beta.2). host_page.h says what the page is for.
//
// THE OWNER'S RULINGS IT CARRIES (AvP-docs RULINGS.md, his words):
//   R-SRV-5 "Their friends will make servers and they will choose their friends' server", "Not 'least'" [ping]: the player chooses, and
//           the rows are in servers.cfg's order, never sorted by ping.
//   R-SRV-6 "No auto choice, they choose": the page opens with nothing chosen, its first row is the page's own question, which chooses
//           nothing, and the cursor starts there. Only Enter on a server's row chooses it.
//   R-SRV-7 "Yes 'host' <ny - ping>, <eu - ping>, <we - ping> on host screen": each server with its round trip.
//   R-SRV-1 "Each server should show": every servers.cfg line has a row, even one that can't be chosen, which says why.
//
// EVERY LINE THE SAME WAY. Every row is decided by the same functions below from what relay_list.cpp says of its line (RelayList_LineNow):
// nothing here asks which line it is.
//
// WHERE THINGS GO, top to bottom, on the menus' 640 by 480 grid, under the title (drawn at 70 in the big font):
//   the question, "Choose the server to host on:", its own row;
//   the column headings: server, ping, games, players, region;
//   the servers, at most kHostPageMostShown at a time, one small line each. The screenful follows the chosen row, so every line can be
//   reached; when there are more than that, which of them are shown ("9-16 / 20");
//   the page's answer to the last Enter (why nothing was chosen), at most two lines;
//   and nothing at or below kHostPageHostLineTop, where the menus draw a host's "Asking the server..." (menus.cpp, DrawTheHostLine).
// host_page_test tries every number of lines up to servers.cfg's 64, and every chosen row, and checks that nothing overlaps.

#ifndef HOST_PAGE_RULES_H
#define HOST_PAGE_RULES_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "relay_list.h"   // what a line's row is read from: kRelayLine... (the find's) and kRelayAccess... (access's)

int const kHostPageMostLines = 64;          // servers.cfg's own limit (servers_cfg.h): a line more than that is never read
int const kHostPageSmallLine = 15;          // the small font's height (HUD_FONT_HEIGHT; host_page.cpp checks it)
int const kHostPageBigLine = 21;            // the big font's (MENU_FONT_HEIGHT)
int const kHostPageTitleTop = 70;           // every page's title (menus.cpp, RenderMenu)
int const kHostPageChooseTop = kHostPageTitleTop + kHostPageBigLine + 14;
int const kHostPageHeadingsTop = kHostPageChooseTop + kHostPageSmallLine + 14;
int const kHostPageRowsTop = kHostPageHeadingsTop + kHostPageSmallLine + 5;
int const kHostPageRowStep = kHostPageSmallLine + 3;
int const kHostPageMostShown = 8;           // the most lines a find asks (kRelayListMostVisits): all of them fit at once
int const kHostPageCountTop = kHostPageRowsTop + kHostPageMostShown * kHostPageRowStep + 4;
int const kHostPageWordsTop = kHostPageCountTop + kHostPageSmallLine + 8;
int const kHostPageWordsLines = 2;
int const kHostPageHostLineTop = 390;       // MENU_BOTTOMYEDGE: a host's line goes from here to 420, where the help line starts
int const kHostPageLeft = 30;               // the menus' edges (MENU_LEFTXEDGE, MENU_RIGHTXEDGE)
int const kHostPageRight = 610;

static_assert(kHostPageMostShown == kRelayListMostVisits, "a screenful holds every line one find asks");
static_assert(kHostPageChooseTop + kHostPageSmallLine <= kHostPageHeadingsTop, "the question ends before the headings");
static_assert(kHostPageHeadingsTop + kHostPageSmallLine <= kHostPageRowsTop, "the headings end before the first row");
static_assert(kHostPageRowsTop + kHostPageMostShown * kHostPageRowStep <= kHostPageCountTop, "the last row ends before the count");
static_assert(kHostPageCountTop + kHostPageSmallLine <= kHostPageWordsTop, "the count ends before the page's answer");
static_assert(kHostPageWordsTop + kHostPageWordsLines * kHostPageSmallLine <= kHostPageHostLineTop, "the answer ends before a host's line");

// The rows' MaxValue: the question's, then each line's, by its place. Far from every other page's rows (the Online page's start at 300).
int const kHostPageChooseRow = 400;
int const kHostPageLineRowFirst = 401;
int const kHostPageRowRoom = 1 + kHostPageMostLines + 1;   // the question, a row for each line, and the end
static_assert(kHostPageChooseRow < kHostPageLineRowFirst, "the question's row is never a line's row");

// The columns of a line's row, left to right, as x and width. A cell is cut to its width before it's drawn (OnlineScreens_FitToWidth),
// so nothing ever reaches the next column. A line that can't be chosen shows why across the four columns after its name.
struct HostPageColumn
{
	int x;
	int width;
};

int const kHostPageColumns = 5;
int const kHostPageColumnServer = 0;
int const kHostPageColumnPing = 1;
int const kHostPageColumnGames = 2;
int const kHostPageColumnPlayers = 3;
int const kHostPageColumnRegion = 4;

inline HostPageColumn HostPage_Column(int column)
{
	static const HostPageColumn columns[kHostPageColumns] = { { 35, 200 }, { 240, 80 }, { 325, 70 }, { 400, 80 }, { 485, 125 } };
	return columns[column < 0 ? 0 : column >= kHostPageColumns ? kHostPageColumns - 1 : column];
}

// Where a line that can't be chosen says why: from the ping column to the right edge
inline HostPageColumn HostPage_WhyColumn(void)
{
	// From the games column on: the ping column shows every line's own round trip, whatever else is true of it (modified 2026-11-11, the
	// owner's "We should show the server ping to the user if no games are up")
	HostPageColumn const games = HostPage_Column(kHostPageColumnGames);
	HostPageColumn const region = HostPage_Column(kHostPageColumnRegion);
	HostPageColumn const why = { games.x, region.x + region.width - games.x };
	return why;
}

// A line's ping, on every row, games or none (the owner, 2026-11-11: "We should show the server ping to the user if no games are up"): the
// round trip of its last visit ("42 ms", taken as 9999 above that), or askingWords ("Asking...", PO's word) while its first answer is still
// to come, or "-" for a line that has no number and isn't being asked (no yes, not asked, or no answer: its row says why). Always ended.
inline void HostPage_PingWords(int pingMs, int lineState, const char *msWords, const char *askingWords, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	if (pingMs >= 0)
	{
		snprintf(out, room, "%d %s", pingMs > 9999 ? 9999 : pingMs, msWords ? msWords : "");
	}
	else if (lineState == kRelayLineAsking)
	{
		snprintf(out, room, "%s", askingWords ? askingWords : "");
	}
	else
	{
		snprintf(out, room, "-");
	}

	out[room - 1] = 0;
}

// Which lines are shown: a screenful of at most kHostPageMostShown, the one that holds the chosen line. With the question chosen (or
// nothing), the first screenful. With no lines, none.
struct HostPageView
{
	int first;
	int shown;
};

inline HostPageView HostPage_View(int count, int chosenPlace)
{
	HostPageView view = { 0, 0 };
	int const lines = count < 0 ? 0 : count > kHostPageMostLines ? kHostPageMostLines : count;

	if (lines == 0)
	{
		return view;
	}

	int const place = chosenPlace < 0 ? 0 : chosenPlace >= lines ? lines - 1 : chosenPlace;
	view.first = (place / kHostPageMostShown) * kHostPageMostShown;
	view.shown = lines - view.first < kHostPageMostShown ? lines - view.first : kHostPageMostShown;
	return view;
}

// Where a line's row starts, or -1 when it isn't in the screenful shown
inline int HostPage_RowTop(int place, HostPageView view)
{
	if (place < view.first || place >= view.first + view.shown)
	{
		return -1;
	}

	return kHostPageRowsTop + (place - view.first) * kHostPageRowStep;
}

// WHAT A LINE'S ROW SAYS, from what relay_list.cpp says of its line, the first that holds. A line can be hosted on only when the player
// has said yes to it, its owner has approved this PC, and its server answered the last find (so its relay is known to be there). Every
// other line still shows, with why.
enum HostPageSays
{
	kHostPageSaysCanHost,       // can be chosen: its round trip, games, players and region
	kHostPageSaysNoYes,         // no yes yet: the Online page asks it
	kHostPageSaysRefused,       // its owner refused this PC, or approved it and took it back
	kHostPageSaysNotApproved,   // not approved yet: not asked this run, being asked, waiting for its owner, or no answer to the request
	kHostPageSaysAsking,        // approved, and the find is asking its server now
	kHostPageSaysNoAnswer,      // approved, and its server didn't answer the last find: why
	kHostPageSaysNotAsked       // approved, but past the lines a find asks (kRelayListMostVisits)
};

inline HostPageSays HostPage_Says(int lineState, int access)
{
	if (lineState == kRelayLineNoYes)
	{
		return kHostPageSaysNoYes;
	}

	if (access == kRelayAccessIsRefused)
	{
		return kHostPageSaysRefused;
	}

	if (access != kRelayAccessIsApproved)
	{
		return kHostPageSaysNotApproved;
	}

	if (lineState == kRelayLineAnswered)
	{
		return kHostPageSaysCanHost;
	}

	if (lineState == kRelayLineNoAnswer)
	{
		return kHostPageSaysNoAnswer;
	}

	if (lineState == kRelayLineNotAsked)
	{
		return kHostPageSaysNotAsked;
	}

	return kHostPageSaysAsking;
}

// WHAT ENTER DOES on a row. On the question, or on a line that can't be chosen, nothing is chosen, and the page says why; on a line whose
// server didn't answer, the servers are asked again, as Enter is how a player tries again. Only a line that can be hosted on is chosen.
enum HostPagePress
{
	kHostPageChooseIt,     // host on this line
	kHostPageSayChoose,    // the question: "Choose one of the servers below."
	kHostPageSayWhy,       // a line that can't be chosen: the page says why, as its row does
	kHostPageAskAgain      // a line whose server didn't answer: ask every line again
};

inline HostPagePress HostPage_Pressed(bool onTheQuestion, HostPageSays says)
{
	if (onTheQuestion)
	{
		return kHostPageSayChoose;
	}

	if (says == kHostPageSaysCanHost)
	{
		return kHostPageChooseIt;
	}

	return says == kHostPageSaysNoAnswer ? kHostPageAskAgain : kHostPageSayWhy;
}

// The players a line's games have, added up from the find's games, each as its host said it. The host is hostile: a count below 0 counts
// as 0, one above kHostPageMostPlayersAGame as that, and the sum stops at kHostPageMostPlayers, so no number can overflow or mislead much.
int const kHostPageMostPlayersAGame = 64;
int const kHostPageMostPlayers = 9999;

inline int HostPage_AddPlayers(int sum, int players)
{
	int const one = players < 0 ? 0 : players > kHostPageMostPlayersAGame ? kHostPageMostPlayersAGame : players;
	int const before = sum < 0 ? 0 : sum > kHostPageMostPlayers ? kHostPageMostPlayers : sum;
	return before + one > kHostPageMostPlayers ? kHostPageMostPlayers : before + one;
}

// The words each state is said with: the game's own, filled in by host_page.cpp (GetTextString), so a test can give its own. A state with
// something of its own in it (the line's name for kHostPageSaysNotApproved, as PO's ruling R-SRV-P3 words it: "Not approved on <name> yet:
// ask on the Online page"; the why for kHostPageSaysNoAnswer; kRelayListMostVisits for kHostPageSaysNotAsked) has it put between its two
// parts. The words are never used as a printf form: the text file is outside the code, so a % in it is only a letter (Tao 3).
struct HostPageWords
{
	const char *before[kHostPageSaysNotAsked + 1];
	const char *after[kHostPageSaysNotAsked + 1];
};

// Why a line can't be chosen, for its row and for Enter's answer. safeName is its name, made safe already; why the game's own fixed words
// for a server that didn't answer (RelayList_LineNow's why, never anything a server sent). Empty for a line that can be chosen. Cut short at
// out's end, always ended.
inline void HostPage_WhyLine(const HostPageWords &words, HostPageSays says, const char *safeName, const char *why, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	out[0] = 0;
	int const at = says < 0 ? 0 : says > kHostPageSaysNotAsked ? kHostPageSaysNotAsked : says;

	if (at == kHostPageSaysCanHost)
	{
		return;
	}

	const char *const before = words.before[at] ? words.before[at] : "";
	const char *const after = words.after[at] ? words.after[at] : "";

	if (at == kHostPageSaysNotApproved)
	{
		snprintf(out, room, "%s%s%s", before, safeName ? safeName : "", after);
	}
	else if (at == kHostPageSaysNoAnswer)
	{
		snprintf(out, room, "%s%s%s", before, why ? why : "", after);
	}
	else if (at == kHostPageSaysNotAsked)
	{
		snprintf(out, room, "%s%d%s", before, kRelayListMostVisits, after);
	}
	else
	{
		snprintf(out, room, "%s%s", before, after);
	}

	out[room - 1] = 0;
}

// The other cells of a line that can be chosen (its ping is HostPage_PingWords, on every row): how many games its server listed, how many
// players they have ("..." until the find's games have come), and its relay's region as its server named it (made safe already), in quotes
// so nobody takes it for the game's own word, or "-" when it gave none. Each always ended.
struct HostPageCells
{
	char games[12];
	char players[12];
	char region[kRelayListRegionLetters + 3];
};

inline HostPageCells HostPage_Cells(int games, int players, bool playersKnown, const char *safeRegion)
{
	HostPageCells cells;

	snprintf(cells.games, sizeof(cells.games), "%d", games < 0 ? 0 : games > kRelayListMostGames ? kRelayListMostGames : games);

	if (playersKnown)
	{
		snprintf(cells.players, sizeof(cells.players), "%d", players < 0 ? 0 : players > kHostPageMostPlayers ? kHostPageMostPlayers : players);
	}
	else
	{
		snprintf(cells.players, sizeof(cells.players), "...");
	}

	if (safeRegion && safeRegion[0])
	{
		snprintf(cells.region, sizeof(cells.region), "\"%.*s\"", static_cast<int>(kRelayListRegionLetters), safeRegion);
	}
	else
	{
		snprintf(cells.region, sizeof(cells.region), "-");
	}

	cells.games[sizeof(cells.games) - 1] = 0;
	cells.players[sizeof(cells.players) - 1] = 0;
	cells.region[sizeof(cells.region) - 1] = 0;
	return cells;
}

// Which lines the screenful shows, as "9-16 / 20", only when they don't all fit: otherwise nothing
inline void HostPage_CountLine(int count, HostPageView view, char *out, size_t room)
{
	if (!out || room == 0)
	{
		return;
	}

	out[0] = 0;

	if (count > kHostPageMostShown && view.shown > 0)
	{
		snprintf(out, room, "%d-%d / %d", view.first + 1, view.first + view.shown, count);
		out[room - 1] = 0;
	}
}

#endif
