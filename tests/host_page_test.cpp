// The test of the host page: where a host chooses the server their game is hosted on (beta.2). Written 2026-11-11 by Melanikus for the
// AvP port.
//
// WHAT IT'S FOR: the owner's rulings "they will choose their friends' server", "Not 'least'" [ping], "No auto choice, they choose" and
// "Yes 'host' <ny - ping>, <eu - ping>, <we - ping> on host screen". The page shows every servers.cfg line on the menus' 640 by 480 grid,
// so nothing may be drawn off it or over anything else, and every line must be reachable. Only a line that can be hosted on (yes, approved,
// answered) is ever chosen, and only by Enter on its own row: the page's own question chooses nothing. What a host sent (its players) is
// hostile and is held to its limits. The words are never a printf form. The page's rules (host_page_rules.h) are tried here directly, with
// 0, 1, 2, 8, 9 and 64 lines, and every chosen row. The game's half (host_page.cpp, menus.cpp) is read as text below, as other tests do.
//
// Nothing here uses the network or the disk, but for reading the game's own source files.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fstream>
#include <sstream>

#include "host_page_rules.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// Every number of lines, every chosen line: each shown row is inside the rows' room, under the one before, and the chosen line is
// always among those shown, so every line can be reached
static void TheRowsFit(int count, const char *what)
{
	bool fits = true;
	bool chosenShown = true;
	int const lines = count < kHostPageMostLines ? count : kHostPageMostLines;

	for (int chosen = -1; chosen <= count + 1; chosen++)
	{
		HostPageView const view = HostPage_View(count, chosen);
		int lastBottom = kHostPageRowsTop;
		int shown = 0;

		for (int place = 0; place < lines; place++)
		{
			int const top = HostPage_RowTop(place, view);

			if (top < 0)
			{
				continue;
			}

			shown++;
			fits = fits && top >= lastBottom && top + kHostPageSmallLine <= kHostPageCountTop;
			lastBottom = top + kHostPageRowStep;
		}

		if (chosen >= 0 && chosen < lines)
		{
			chosenShown = chosenShown && HostPage_RowTop(chosen, view) >= 0;
		}

		fits = fits && shown == view.shown && view.shown <= kHostPageMostShown && view.first >= 0 && view.first + view.shown <= lines &&
			(lines == 0 || view.shown > 0);
	}

	Check(fits, what);
	Check(chosenShown, "and whichever line is chosen, it's among those shown");
}

// Reads one of the game's files as text, from the repository's top (where the tests run)
static std::string FileText(const char *path)
{
	std::ifstream in(path, std::ios::binary);
	std::stringstream all;
	all << in.rdbuf();
	std::string text = all.str();
	std::string plain;

	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

// The text from the line that starts with start to the first end after it, or "" when start isn't there exactly once
static std::string Slice(const std::string &text, const std::string &start, const std::string &end)
{
	size_t const at = text.find(start);

	if (at == std::string::npos || text.find(start, at + 1) != std::string::npos)
	{
		return "";
	}

	size_t const stop = text.find(end, at + start.size());
	return stop == std::string::npos ? "" : text.substr(at, stop - at);
}

static bool Has(const std::string &text, const std::string &part)
{
	return text.find(part) != std::string::npos;
}

static int Count(const std::string &text, const std::string &part)
{
	int n = 0;

	for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
	{
		n++;
	}

	return n;
}

int main()
{
	printf("The host page's rules (host_page_rules.h)\n");

	// THE LAYOUT: every number of lines up to servers.cfg's 64, and more
	TheRowsFit(0, "with no line, no row is shown");
	TheRowsFit(1, "with 1 line, its row fits");
	TheRowsFit(2, "with 2 lines (New York and Frankfurt), both rows fit");
	TheRowsFit(8, "with 8 lines, the most one find asks, all 8 rows fit at once");
	TheRowsFit(9, "with 9 lines, a screenful at a time, every row fits");
	TheRowsFit(64, "with 64 lines, servers.cfg's limit, every row fits");
	TheRowsFit(500, "and a count past the limit is held to it");

	HostPageView const eight = HostPage_View(8, -1);
	Check(eight.first == 0 && eight.shown == 8, "8 lines are all shown at once, with the question chosen");
	HostPageView const ninth = HostPage_View(9, 8);
	Check(ninth.first == 8 && ninth.shown == 1, "the ninth line, chosen, shows on a screenful of its own");

	bool columnsFit = true;
	int lastRight = kHostPageLeft;

	for (int c = 0; c < kHostPageColumns; c++)
	{
		HostPageColumn const column = HostPage_Column(c);
		columnsFit = columnsFit && column.x >= lastRight && column.width > 6 && column.x + column.width <= kHostPageRight;
		lastRight = column.x + column.width;
	}

	Check(columnsFit, "the five columns are inside the menus' edges, left to right, and none overlaps the next");
	HostPageColumn const why = HostPage_WhyColumn();
	Check(why.x == HostPage_Column(kHostPageColumnPing).x && why.x + why.width <= kHostPageRight && why.x >= HostPage_Column(kHostPageColumnServer).x +
		HostPage_Column(kHostPageColumnServer).width, "a line that can't be chosen says why after its name, inside the right edge");
	Check(HostPage_Column(-5).x == HostPage_Column(0).x && HostPage_Column(99).x == HostPage_Column(kHostPageColumns - 1).x,
		"a column number out of range is held to the first or the last");

	char countLine[48];
	HostPage_CountLine(8, HostPage_View(8, 3), countLine, sizeof(countLine));
	Check(countLine[0] == 0, "with every line shown, no count line");
	HostPage_CountLine(20, HostPage_View(20, 10), countLine, sizeof(countLine));
	Check(strcmp(countLine, "9-16 / 20") == 0, "with more, which lines are shown: \"9-16 / 20\"");

	// WHAT A ROW SAYS: only a line with its yes, approved, and answered can be hosted on; every other line says why
	int const states[] = { kRelayLineNoYes, kRelayLineAsking, kRelayLineAnswered, kRelayLineNoAnswer, kRelayLineNotAsked, -1, 99 };
	int const accesses[] = { kRelayAccessNotAsked, kRelayAccessAsking, kRelayAccessWaitingApproval, kRelayAccessIsApproved,
		kRelayAccessIsRefused, kRelayAccessNoAnswer, -1, 99 };
	bool onlyApprovedAnswered = true;
	bool yesFirst = true;

	for (int state : states)
	{
		for (int access : accesses)
		{
			HostPageSays const says = HostPage_Says(state, access);
			bool const canHost = says == kHostPageSaysCanHost;
			onlyApprovedAnswered = onlyApprovedAnswered && canHost == (state == kRelayLineAnswered && access == kRelayAccessIsApproved);
			yesFirst = yesFirst && (state != kRelayLineNoYes || says == kHostPageSaysNoYes);
		}
	}

	Check(onlyApprovedAnswered, "of every state and every access, only an approved line that answered can be hosted on");
	Check(yesFirst, "a line without the yes says so first, whatever its access");
	Check(HostPage_Says(kRelayLineAnswered, kRelayAccessIsRefused) == kHostPageSaysRefused, "a line whose owner refused this PC says so");
	Check(HostPage_Says(kRelayLineAnswered, kRelayAccessWaitingApproval) == kHostPageSaysNotApproved &&
		HostPage_Says(kRelayLineAnswered, kRelayAccessNotAsked) == kHostPageSaysNotApproved, "a line not approved yet says so");
	Check(HostPage_Says(kRelayLineNoAnswer, kRelayAccessIsApproved) == kHostPageSaysNoAnswer &&
		HostPage_Says(kRelayLineAsking, kRelayAccessIsApproved) == kHostPageSaysAsking &&
		HostPage_Says(kRelayLineNotAsked, kRelayAccessIsApproved) == kHostPageSaysNotAsked,
		"an approved line that didn't answer, is being asked, or wasn't asked, says which");

	// WHAT ENTER DOES: the question chooses nothing; only a line that can be hosted on is chosen
	bool questionChoosesNothing = true;
	bool onlyCanHostChosen = true;

	for (int says = kHostPageSaysCanHost; says <= kHostPageSaysNotAsked; says++)
	{
		HostPageSays const s = static_cast<HostPageSays>(says);
		questionChoosesNothing = questionChoosesNothing && HostPage_Pressed(true, s) == kHostPageSayChoose;
		onlyCanHostChosen = onlyCanHostChosen && ((HostPage_Pressed(false, s) == kHostPageChooseIt) == (s == kHostPageSaysCanHost));
	}

	Check(questionChoosesNothing, "NOTHING IS CHOSEN FOR THE PLAYER: Enter on the page's question never chooses a line, whatever the lines say");
	Check(onlyCanHostChosen, "Enter chooses a line only when it can be hosted on");
	Check(HostPage_Pressed(false, kHostPageSaysNoAnswer) == kHostPageAskAgain, "Enter on a line that didn't answer asks again");

	// THE PLAYERS: the host's number is hostile, held to its limits
	Check(HostPage_AddPlayers(0, 3) == 3 && HostPage_AddPlayers(3, 4) == 7, "the players of a line's games are added up");
	Check(HostPage_AddPlayers(0, -5) == 0 && HostPage_AddPlayers(0, 1000000) == kHostPageMostPlayersAGame,
		"a host saying fewer than none, or a million, counts as none, or the most a game can have");
	int sum = 0;

	for (int i = 0; i < 1000; i++)
	{
		sum = HostPage_AddPlayers(sum, 0x7FFFFFFF);
	}

	Check(sum == kHostPageMostPlayers, "and no number of games can make the sum overflow");

	// THE CELLS
	HostPageCells const good = HostPage_Cells(42, 3, 7, true, "nyc", "ms");
	Check(strcmp(good.ping, "42 ms") == 0 && strcmp(good.games, "3") == 0 && strcmp(good.players, "7") == 0 && strcmp(good.region, "\"nyc\"") == 0,
		"a line that can be chosen: \"42 ms\", its games, its players, and its region in quotes, as the server's own word");
	HostPageCells const early = HostPage_Cells(-1, -4, 2, false, "", "ms");
	Check(strcmp(early.ping, "?") == 0 && strcmp(early.games, "0") == 0 && strcmp(early.players, "...") == 0 && strcmp(early.region, "-") == 0,
		"before it's known: \"?\" for the ping, \"...\" for the players, \"-\" for no region");
	HostPageCells const huge = HostPage_Cells(123456, 99999, 99999, true, "a-region-of-16ch-and-more", "ms");
	Check(strcmp(huge.ping, "9999 ms") == 0 && atoi(huge.games) == kRelayListMostGames && atoi(huge.players) == kHostPageMostPlayers &&
		strlen(huge.region) <= kRelayListRegionLetters + 2, "numbers past their limits are held to them, and a region to its 16 letters");

	// THE WORDS: never a printf form, and R-SRV-P3's own words for a line not approved yet
	HostPageWords words = {};
	words.before[kHostPageSaysNotApproved] = "Not approved on ";
	words.after[kHostPageSaysNotApproved] = " yet: ask on the Online page";
	words.before[kHostPageSaysNoAnswer] = "no answer: ";
	words.before[kHostPageSaysNotAsked] = "not asked: only the first ";
	words.after[kHostPageSaysNotAsked] = " servers in servers.cfg are asked";
	words.before[kHostPageSaysNoYes] = "100%s %n";
	char line[200];
	HostPage_WhyLine(words, kHostPageSaysNotApproved, "Frankfurt", NULL, line, sizeof(line));
	Check(strcmp(line, "Not approved on Frankfurt yet: ask on the Online page") == 0, "a line not approved yet: \"Not approved on <name> yet: ask on the Online page\" (R-SRV-P3)");
	HostPage_WhyLine(words, kHostPageSaysNoAnswer, "New York", "the server couldn't be reached", line, sizeof(line));
	Check(strcmp(line, "no answer: the server couldn't be reached") == 0, "a line that didn't answer: the game's own why");
	HostPage_WhyLine(words, kHostPageSaysNotAsked, "Ninth", NULL, line, sizeof(line));
	Check(strcmp(line, "not asked: only the first 8 servers in servers.cfg are asked") == 0, "a line past the ones a find asks: how many are asked");
	HostPage_WhyLine(words, kHostPageSaysNoYes, "X", NULL, line, sizeof(line));
	Check(strcmp(line, "100%s %n") == 0, "a % in the language file is only a letter, never a printf form");
	HostPage_WhyLine(words, kHostPageSaysCanHost, "X", NULL, line, sizeof(line));
	Check(line[0] == 0, "a line that can be chosen has no why");
	char small[8];
	HostPage_WhyLine(words, kHostPageSaysNotApproved, "Frankfurt", NULL, small, sizeof(small));
	Check(strlen(small) == sizeof(small) - 1, "a why too long for its room is cut, and always ended");

	// THE GAME'S HALF, read as text: what the rules can't prove alone
	printf("The game's half (host_page.cpp, menus.cpp)\n");
	std::string const page = FileText("3dc/avp/win95/frontend/host_page.cpp");
	std::string const menus = FileText("3dc/avp/win95/frontend/menus.cpp");
	Check(!page.empty() && !menus.empty(), "the page's file and the menus' file are there");

	std::string const opened = Slice(page, "bool HostPage_Opened(AVPMENU_ELEMENT *rows, int room)\n{", "\n}\n");
	size_t const cleared = opened.find("(void)RelayList_HostOn(-1);");
	size_t const question = opened.find("rows[0] = question;");
	Check(cleared != std::string::npos && question != std::string::npos && cleared < question && cleared < opened.find("RelayList_FindGames();"),
		"NO AUTO CHOICE: the page clears the choice first thing, before it asks the lines or writes its rows");
	Check(Has(opened, "AVPMENU_ELEMENT const question = {AVPMENU_ELEMENT_HOSTLINE, {TEXTSTRING_HOSTPAGE_CHOOSE}, {static_cast<uint32_t>(kHostPageChooseRow)}"),
		"and its first row, where the cursor starts, is the question, which is no line");

	std::string const pressed = Slice(page, "bool HostPage_PressedRow(int row)\n{", "\n}\n");
	size_t const chooseCase = pressed.find("case kHostPageChooseIt:");
	size_t const hostOn = pressed.find("if (RelayList_HostOn(place))");
	Check(chooseCase != std::string::npos && hostOn != std::string::npos && chooseCase < hostOn && Count(pressed, "RelayList_HostOn(") == 1 &&
		pressed.find("switch (HostPage_Pressed(false, says))") < chooseCase, "a line is chosen only in Enter's kHostPageChooseIt, decided by the rules from what relay_list says of it");
	Check(Count(page, "RelayList_HostOn(") == 2, "and the page names RelayList_HostOn only there and to clear the choice");
	std::string const askAgain = Slice(pressed, "case kHostPageAskAgain:", "return false;");
	Check(Count(page, "RelayList_FindGames();") == 2 && Has(opened, "RelayList_FindGames();") && Has(askAgain, "RelayList_FindGames();"),
		"the page asks the lines only as it opens and when the player presses Enter on a line that didn't answer: never on a timer, never on each answer");

	// The menus: Start game and Co-op campaign open the page, every time, and only a choice there goes on
	std::string const press = Slice(menus, "case AVPMENU_ELEMENT_GOTOMENU:\n\t\tcase AVPMENU_ELEMENT_GOTOMENU_GFX:\n\t\t{", "\n\t\t}\n");
	Check(Has(press, "hostPageGoesTo = static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo);") && Has(press, "SetupNewMenu(AVPMENU_HOST_ON);") &&
		Has(press, "elementPtr->MenuToGoTo == AVPMENU_MULTIPLAYER_CONFIG || elementPtr->MenuToGoTo == AVPMENU_COOP_HOST"),
		"Start game and Co-op campaign open the host page, every time");
	std::string const lineRow = Slice(menus, "case AVPMENU_ELEMENT_HOSTLINE:\n\t\t{\n\t\t\tif (interactionID", "\n\t\t}\n");
	Check(Has(lineRow, "== AVPMENU_ELEMENT_INTERACTION_SELECT && HostPage_PressedRow(elementPtr->MaxValue))") && Has(lineRow, "SetupNewMenu(hostPageGoesTo);"),
		"and the menus go on from it, to where the host was going, only when HostPage_PressedRow chose a line");
	Check(!Has(menus, "MP_HostOnChoice") && !Has(menus, "HostOnChoices") && !Has(menus, "HostOnFollowsTheRow") && !Has(menus, "HostOnChoiceForARun"),
		"the Host on row is gone, so nothing on the Multiplayer page chooses a line");

	// A Hudson run (hostrow=online with hoston=, Third's): it presses the row of the line it names, as a player does, and never one by default
	std::string const run = Slice(menus, "static bool ChooseOnTheHostPageForARun(void)\n{", "\n}\n");
	Check(Has(run, "const char *const name = Hudson_HostOnName();") && Has(run, "!HostPage_RowCanBeChosen(row = HostPage_RowNamed(name))") &&
		Has(run, "PressTheRowForARun(AVPMENU_ELEMENT_HOSTLINE, row, ") && !Has(run, "RelayList_HostOn("),
		"a Hudson run presses the row of the line hoston= names, waited for, as a player does; it never chooses a line itself, nor the first by default");
	Check(Count(menus, "ChooseOnTheHostPageForARun()") == 1 && Has(Slice(menus, "static bool HostThroughTheRowsForARun(int level)\n{", "\n}\n"),
		"if (!ChooseOnTheHostPageForARun() || AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_CONFIG)"), "and a run that chose nothing hosts nothing");

	// The Multiplayer page's rows: the player's name, the Online row, then the rows that host, and no Host on row (moved here from
	// server_yes_page_test, 2026-11-11)
	std::string const data = FileText("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const setter = Slice(data, "void MenuData_SetMultiplayerRows(void)\n{", "\n}\n");
	size_t const nameRow = setter.find("= PlayerNameRow;");
	size_t const onlineRow = setter.find("= OnlineRow;");
	size_t const startRow = setter.find("= StartGameRow;");
	Check(nameRow != std::string::npos && onlineRow != std::string::npos && startRow != std::string::npos && nameRow < onlineRow && onlineRow < startRow &&
		!Has(data, "HostOnRow") && !Has(data, "MP_HostOnChoice"), "the Multiplayer page goes from the player's name and the Online row to Start game, with no Host on row");
	Check(Has(data, "static AVPMENU_ELEMENT AvPMenu_HostOn[kHostPageRowRoom] = { {AVPMENU_ELEMENT_ENDOFMENU} };") &&
		Has(data, "{AVPMENU_FONT_SMALL,\tTEXTSTRING_HOSTPAGE_TITLE,\t\t\tAvPMenu_HostOn,\t\t\t\t\tAVPMENU_MULTIPLAYER, 0},"),
		"the host page has room for every line's row, and Esc on it goes back to the Multiplayer page");

	printf(checksFailed ? "Host page: %d FAILED\n" : "Host page: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
