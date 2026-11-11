// The test of the Online page: one row for every servers.cfg line, each decided the same way (beta.2, the owner's "UNIFY THE UI" and
// "SAME CODE, FOR 2 OR MORE SERVERS"). Written 2026-11-11 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: the page shows up to 64 lines and up to 64 lines the game can't use, on the menus' 640 by 480 grid, so nothing may be
// drawn off it or over anything else, and every line must be reachable. Every row's state is said in words, decided only by what
// relay_list.cpp says of its line, so one line's row can't differ from another's but by what's true of it. The yes comes first for every
// line, and Enter never sends a request to a line without its yes, nor twice in a minute. The page's rules (online_page_rules.h) are tried
// here directly, with 1, 2, 9 and 64 lines.
//
// Nothing here uses the network or the disk.

#include <stdio.h>
#include <string.h>
#include <string>

#include "online_page_rules.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// Every number of lines, every chosen row: each shown line's row is on the page, inside the rows' room, under the one before, and the
// chosen line is always among those shown
static void TheRowsFit(int count, const char *what)
{
	bool fits = true;
	bool chosenShown = true;

	for (int chosen = -1; chosen <= count + kOnlinePageOwnRows; chosen++)
	{
		OnlinePageView const view = OnlinePage_View(count, chosen);
		int lastBottom = kOnlinePageRowsTop;

		for (int place = 0; place < count; place++)
		{
			int const top = OnlinePage_RowTop(place, view);

			if (top < 0)
			{
				continue;
			}

			fits = fits && top >= lastBottom && top + kOnlinePageRowHeight <= kOnlinePageCountTop;
			lastBottom = top + kOnlinePageRowHeight;
		}

		if (chosen >= 0 && chosen < count)
		{
			chosenShown = chosenShown && OnlinePage_RowTop(chosen, view) >= 0;
		}

		fits = fits && view.shown <= kOnlinePageMostShown && view.first >= 0 && view.first + view.shown <= (count < kOnlinePageMostLines ? count : kOnlinePageMostLines);
	}

	std::string const said = std::string(what) + ": every shown row fits under the one before, inside the rows' room, and the chosen line is always shown";
	Check(fits && chosenShown, said.c_str());
}

static OnlinePageWords TheWords(void)
{
	OnlinePageWords words = {};
	words.before[kOnlinePageSaysNoYes] = "No yes yet: press Enter to see this server and say yes or no";
	words.before[kOnlinePageSaysRefused] = "Refused: its owner hasn't let this PC in";
	words.before[kOnlinePageSaysAsking] = "Asking...";
	words.before[kOnlinePageSaysWaiting] = "Waiting for approval: give its owner your code ";
	words.after[kOnlinePageSaysWaiting] = ". Enter checks again";
	words.before[kOnlinePageSaysNoAnswer] = "Didn't answer: ";
	words.after[kOnlinePageSaysNoAnswer] = ". Enter tries again";
	words.before[kOnlinePageSaysNotAsked] = "Not asked yet: press Enter to ask its owner to let this PC in";
	words.before[kOnlinePageSaysApprovedGames] = "Approved: ";
	words.after[kOnlinePageSaysApprovedGames] = " games";
	words.before[kOnlinePageSaysApprovedFinding] = "Approved: asking for games...";
	words.before[kOnlinePageSaysApprovedNoGames] = "Approved: didn't answer: ";
	words.before[kOnlinePageSaysApprovedNotVisited] = "Approved: not asked for games (more than ";
	words.after[kOnlinePageSaysApprovedNotVisited] = " servers)";
	return words;
}

int main()
{
	printf("Online page\n");

	// WHERE THINGS GO
	Check(kOnlinePageOwnRowsTop + kOnlinePageOwnRows * kOnlinePageOwnRowHeight <= kOnlinePageHelpTop && OnlinePage_OwnRowTop(kOnlinePageRowBack) + kOnlinePageBigLine <= kOnlinePageHelpTop,
		"the page's own rows, Back the last, end before the help line");
	Check(OnlinePage_OwnRowTop(-5) == OnlinePage_OwnRowTop(0) && OnlinePage_OwnRowTop(99) == OnlinePage_OwnRowTop(kOnlinePageRowBack),
		"and a row out of range is drawn in range, never off the page");
	TheRowsFit(0, "no lines");
	TheRowsFit(1, "1 line");
	TheRowsFit(2, "2 lines");
	TheRowsFit(9, "9 lines");
	TheRowsFit(64, "64 lines");
	TheRowsFit(500, "more lines than servers.cfg is ever read for");

	OnlinePageView const nine = OnlinePage_View(9, 8);
	char count[32];
	OnlinePage_CountLine(9, nine, count, sizeof(count));
	Check(nine.first == 8 && nine.shown == 1 && strcmp(count, "9-9 / 9") == 0, "with 9 lines, the 9th chosen shows the last screenful, and says which");
	OnlinePage_CountLine(2, OnlinePage_View(2, 0), count, sizeof(count));
	Check(count[0] == 0, "and with 2, there's no count: they all fit");

	Check(OnlinePage_RefusedShown(0) == 0 && OnlinePage_RefusedShown(kOnlinePageMostRefused) == kOnlinePageMostRefused &&
		OnlinePage_RefusedShown(64) == kOnlinePageMostRefused - 1, "refused lines: all of them while they fit, else one fewer and a line saying how many more");

	// WHAT A ROW SAYS: the yes first, for every line; then access; then, once approved, the last find
	int const accesses[] = { kRelayAccessNotAsked, kRelayAccessAsking, kRelayAccessWaitingApproval, kRelayAccessIsApproved, kRelayAccessIsRefused, kRelayAccessNoAnswer };
	bool noYesFirst = true;

	for (int access : accesses)
	{
		noYesFirst = noYesFirst && OnlinePage_Says(kRelayLineNoYes, access) == kOnlinePageSaysNoYes;
	}

	Check(noYesFirst, "a line without the yes says so, whatever access says");

	bool accessNext = true;
	int const lineStates[] = { kRelayLineAsking, kRelayLineAnswered, kRelayLineNoAnswer, kRelayLineNotAsked };

	for (int state : lineStates)
	{
		accessNext = accessNext && OnlinePage_Says(state, kRelayAccessIsRefused) == kOnlinePageSaysRefused &&
			OnlinePage_Says(state, kRelayAccessAsking) == kOnlinePageSaysAsking && OnlinePage_Says(state, kRelayAccessWaitingApproval) == kOnlinePageSaysWaiting &&
			OnlinePage_Says(state, kRelayAccessNoAnswer) == kOnlinePageSaysNoAnswer && OnlinePage_Says(state, kRelayAccessNotAsked) == kOnlinePageSaysNotAsked;
	}

	Check(accessNext, "with the yes, a line not yet approved says where its access stands, whatever the find found");
	Check(OnlinePage_Says(kRelayLineAnswered, kRelayAccessIsApproved) == kOnlinePageSaysApprovedGames &&
		OnlinePage_Says(kRelayLineAsking, kRelayAccessIsApproved) == kOnlinePageSaysApprovedFinding &&
		OnlinePage_Says(kRelayLineNoAnswer, kRelayAccessIsApproved) == kOnlinePageSaysApprovedNoGames &&
		OnlinePage_Says(kRelayLineNotAsked, kRelayAccessIsApproved) == kOnlinePageSaysApprovedNotVisited,
		"and an approved line says what the last find found there");
	Check(OnlinePage_Says(kRelayLineAnswered, 77) == kOnlinePageSaysNotAsked, "an access state the page doesn't know is never called approved");

	// THE WORDS: the game's own, with the code, the why or a number put between their two parts, never used as a printf form
	OnlinePageWords const words = TheWords();
	char line[256];
	OnlinePage_StateLine(words, kOnlinePageSaysWaiting, "ABCD-EFGH", NULL, 0, line, sizeof(line));
	Check(strcmp(line, "Waiting for approval: give its owner your code ABCD-EFGH. Enter checks again") == 0, "a waiting line names this PC's code");
	OnlinePage_StateLine(words, kOnlinePageSaysNoAnswer, NULL, "its keys don't match its line in servers.cfg", 0, line, sizeof(line));
	Check(strcmp(line, "Didn't answer: its keys don't match its line in servers.cfg. Enter tries again") == 0, "a line that didn't answer says why");
	OnlinePage_StateLine(words, kOnlinePageSaysApprovedGames, NULL, NULL, 3, line, sizeof(line));
	Check(strcmp(line, "Approved: 3 games") == 0, "an approved line says how many games");
	OnlinePage_StateLine(words, kOnlinePageSaysApprovedNotVisited, NULL, NULL, 0, line, sizeof(line));
	Check(strcmp(line, "Approved: not asked for games (more than 8 servers)") == 0, "and one past the find's first 8 says so");

	OnlinePageWords hostile = {};
	hostile.before[kOnlinePageSaysWaiting] = "%s%s%s%n%x";
	hostile.after[kOnlinePageSaysWaiting] = "%d";
	OnlinePage_StateLine(hostile, kOnlinePageSaysWaiting, "ABCD-EFGH", NULL, 0, line, sizeof(line));
	Check(strcmp(line, "%s%s%s%n%xABCD-EFGH%d") == 0, "words with % in them, from a changed text file, are only letters: they're never a printf form");

	char tiny[8];
	OnlinePage_StateLine(words, kOnlinePageSaysNoYes, NULL, NULL, 0, tiny, sizeof(tiny));
	Check(strlen(tiny) == sizeof(tiny) - 1, "a line longer than its room is cut, and ended");
	OnlinePage_StateLine(words, static_cast<OnlinePageSays>(99), NULL, NULL, 0, line, sizeof(line));
	Check(strlen(line) < sizeof(line), "and a state out of range is read in range, never past the words");

	char name[128];
	OnlinePage_NameLine("New York", true, 42, "ours", "not ours", "ms", "no ping yet", name, sizeof(name));
	Check(strcmp(name, "New York (ours) - 42 ms") == 0, "a line's first line: its name, ours, and its ping");
	OnlinePage_NameLine("Friends", false, -1, "ours", "not ours", "ms", "no ping yet", name, sizeof(name));
	Check(strcmp(name, "Friends (not ours) - no ping yet") == 0, "and not ours, with no ping yet");

	char refused[128];
	OnlinePage_RefusedLine("servers.cfg line", 4, "its fingerprint isn't 64 small hex digits", refused, sizeof(refused));
	Check(strcmp(refused, "servers.cfg line 4: its fingerprint isn't 64 small hex digits") == 0, "a refused line: its number and the game's own words");

	// WHAT ENTER DOES: the yes page for a line without the yes; a request at most once a minute for each line; nothing while one is on its
	// way, or once its owner refused this PC
	bool yesFirst = true;

	for (int access : accesses)
	{
		yesFirst = yesFirst && OnlinePage_Pressed(OnlinePage_Says(kRelayLineNoYes, access), true, 0, 0) == kOnlinePageOpenTheYes;
	}

	Check(yesFirst, "Enter on a line without the yes opens its yes page, and sends nothing");
	Check(OnlinePage_Pressed(kOnlinePageSaysNotAsked, false, 5, 0) == kOnlinePageAsk, "with the yes, the first press asks for access");
	Check(OnlinePage_Pressed(kOnlinePageSaysWaiting, true, 59999, 0) == kOnlinePageWaitAMinute &&
		OnlinePage_Pressed(kOnlinePageSaysWaiting, true, 60000, 0) == kOnlinePageAsk, "a second press asks again only once a minute has gone");
	Check(OnlinePage_Pressed(kOnlinePageSaysApprovedGames, true, 10, 0xFFFFFFF0u) == kOnlinePageWaitAMinute,
		"and the clock wrapping round in that minute changes nothing");
	Check(OnlinePage_Pressed(kOnlinePageSaysAsking, false, 0, 0) == kOnlinePageNothing && OnlinePage_Pressed(kOnlinePageSaysRefused, false, 0, 0) == kOnlinePageNothing,
		"nothing is sent while a request is on its way, or to a line whose owner refused this PC");

	printf(checksFailed ? "Online page: %d FAILED\n" : "Online page: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
