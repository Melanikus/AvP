// The test of the Online access pages (server #24): their rules, and that nothing of them is there without -dev. Written 2026-11-09 by
// Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 1. The page's rules (3dc/avp/win95/frontend/access_page_rules.h). It never asks the list by itself before the player has sent a request;
//    it never asks within a minute of its last call, which is the list's limit for one address, even when the clock goes round to 0; and it
//    has words of its own for every answer and every name that can't be sent.
// 2. The owner's ruling, 2026-11-09: "I want the code to test but NOTHING that can be actually accessed by people with the release". The
//    pages need the whole game, so this reads their source, at each of the three places that keep them out of a release game:
//    - the Multiplayer page's table ends where the Online access row would be, and only -dev puts the row there;
//    - the pages themselves send a player without -dev straight back;
//    - relay_list.cpp refuses the call without -dev, whatever asks it.
//    And the pages' only way to the network is that one call. What a row does on a real screen needs a run of the game with -dev.
// 3. Beta.2's words, by reading the text table.
// 4. Where the page's parts go (the owner's pictures of 2026-11-10): under the title, in order, nothing on anything, inside the menus'
//    edges, padded evenly; its rows further apart and brighter; and Copy access code chosen once the code is there. What it looks like
//    on a real screen needs Hudson's picture of the page.
// 5. An access request goes only to a list whose key is checked before a byte is sent, which the 8-letter code needs (PO's ruling,
//    2026-11-10), by reading relay_list.cpp.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own, run from the top of the repository, where the source it reads is found from. The program
// gives back a non-zero result if anything failed.

#include <stdio.h>
#include <string.h>
#include <string>

#include "access_page_rules.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static bool Has(const std::string &text, const char *part)
{
	return text.find(part) != std::string::npos;
}

static size_t CountOf(const std::string &text, const char *part)
{
	size_t count = 0;

	for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
	{
		count++;
	}

	return count;
}

static void TestWhenItAsks()
{
	puts("1. when the page asks the list");

	Check(!AccessPage_MinuteHasGone(1000 + 59999, 1000) && AccessPage_MinuteHasGone(1000 + 60000, 1000), "a minute is 60,000 ms, not one less");
	Check(!AccessPage_MinuteHasGone(20000, 0xFFFFFFFFu - 20000) && AccessPage_MinuteHasGone(40001, 0xFFFFFFFFu - 20000),
		"and it's counted across the clock going round to 0 after 49 days");
	Check(!AccessPage_MinuteHasGone(0xFFFFFFF5u, 0xFFFFFFF0u), "5 ms after a call made just before the clock goes round is no minute");

	Check(!AccessPage_MayAskByItself(false, false, 500000, 0), "the page never asks by itself before the player has sent a request");
	Check(!AccessPage_MayAskByItself(true, true, 500000, 0), "nor while a call is on its way");
	Check(!AccessPage_MayAskByItself(true, false, 30000, 0), "nor within a minute of the last call");
	Check(AccessPage_MayAskByItself(true, false, 61000, 0), "but once one has been sent, a minute after the last call, opening the page asks again");

	Check(AccessPage_PressedAsk(false, false, 5, 0) == kAccessPageAskSends, "the first press of the run sends at once");
	Check(AccessPage_PressedAsk(true, true, 500000, 0) == kAccessPageAskOnItsWay, "a press while a call is on its way sends nothing more");
	Check(AccessPage_PressedAsk(true, false, 30000, 0) == kAccessPageAskWaitAMinute, "a press within a minute of the last call waits");
	Check(AccessPage_PressedAsk(true, false, 60000, 0) == kAccessPageAskSends, "and one a minute after it sends");
}

static void TestTheWords()
{
	puts("2. the page's words");

	bool eachHasWords = true;
	int const answers[] = { kRelayAccessWaiting, kRelayAccessApproved, kRelayAccessRefused, kRelayAccessRevoked, kRelayAccessBusy, kRelayAccessClosed };

	for (size_t a = 0; a < sizeof(answers) / sizeof(answers[0]); a++)
	{
		TEXTSTRING_ID const words = AccessPage_AnswerText(static_cast<RelayAccessAnswer>(answers[a]));
		eachHasWords = eachHasWords && words != TEXTSTRING_BLANK && words != TEXTSTRING_ACCESS_CANT_REACH;

		for (size_t b = 0; b < a; b++)
		{
			eachHasWords = eachHasWords && words != AccessPage_AnswerText(static_cast<RelayAccessAnswer>(answers[b]));
		}
	}

	Check(eachHasWords, "waiting, approved, refused, revoked, busy and closed each have words of their own");
	Check(AccessPage_AnswerText(kRelayAccessApproved) == TEXTSTRING_ACCESS_APPROVED && AccessPage_AnswerText(kRelayAccessWaiting) == TEXTSTRING_ACCESS_WAITING,
		"and approved says approved, waiting says waiting");
	Check(AccessPage_AnswerText(kRelayAccessBadName) == TEXTSTRING_ACCESS_NOT_TAKEN && AccessPage_AnswerText(kRelayAccessBadKey) == TEXTSTRING_ACCESS_NOT_TAKEN &&
		AccessPage_AnswerText(kRelayAccessBadShape) == TEXTSTRING_ACCESS_NOT_TAKEN, "a request the server wouldn't take says so, whichever part it was");

	Check(AccessPage_NameText(kRelayAccessNameGood) == TEXTSTRING_BLANK, "a name that can be sent needs no words");
	TEXTSTRING_ID const nameWords[] = { AccessPage_NameText(kRelayAccessNameEmpty), AccessPage_NameText(kRelayAccessNameTooLong),
		AccessPage_NameText(kRelayAccessNameBadLetter), AccessPage_NameText(kRelayAccessNameSpaceAtAnEnd) };
	bool namesDiffer = true;

	for (size_t a = 0; a < 4; a++)
	{
		namesDiffer = namesDiffer && nameWords[a] != TEXTSTRING_BLANK;

		for (size_t b = 0; b < a; b++)
		{
			namesDiffer = namesDiffer && nameWords[a] != nameWords[b];
		}
	}

	Check(namesDiffer, "and each reason one can't be sent has words of its own");
	Check(AccessPage_NeedsANewIdentity(kRelayAccessRefused) && AccessPage_NeedsANewIdentity(kRelayAccessRevoked) &&
		!AccessPage_NeedsANewIdentity(kRelayAccessWaiting) && !AccessPage_NeedsANewIdentity(kRelayAccessBusy),
		"only refused and revoked need a new identity to ask again");
}

// Modified 2026-11-10 by Melanikus for the AvP port (beta.2, PO's go): the pages were -dev only for beta.1; now they're there when
// servers.cfg names a server, the one they ask, and -dev decides nothing
static void TestOnlyWithAServer()
{
	puts("3. nothing of it when servers.cfg names no server, and -dev decides nothing (read in the source)");

	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/access_page.cpp");
	std::string const list = ReadSource("3dc/relay_list.cpp");
	Check(!data.empty() && !menus.empty() && !page.empty() && !list.empty(), "the four sources were found and read");

	// The Multiplayer page's own table: its four rows of 1999, then its end, and room for the rows a named server brings. Modified
	// 2026-11-10 by Melanikus for the AvP port: the rows are written as the page opens (menu_data.cpp, MenuData_SetMultiplayerRows).
	std::string const table = SourceSlice(data, "static AVPMENU_ELEMENT AvPMenu_Multiplayer[]=", "\n};\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(table, "\tPlayerNameRow,\n\tStartGameRow,\n\tCoopRow,\n\tJoinGameRow,\n\tEndOfTheRows,\n") && !Has(table, "OnlineAccessRow") &&
		!Has(table, "HostWhereRow"), "the Multiplayer page's table has its four rows of 1999, then its end, and the Online access row isn't in it");

	std::string const setter = SourceSlice(data, "void MenuData_SetMultiplayerRows(bool aServerIsNamed, char **hostWhereNames)", "\n}\n", kSliceEndAfterTheStart,
		FailedSlice);
	size_t const joinRow = setter.find("AvPMenu_Multiplayer[row++] = JoinGameRow;");
	size_t const accessRow = setter.find("\tif (aServerIsNamed)\n\t{\n\t\tAvPMenu_Multiplayer[row++] = OnlineAccessRow;\n\t}");
	Check(joinRow != std::string::npos && accessRow != std::string::npos && joinRow < accessRow && CountOf(data, "= OnlineAccessRow;") == 1 &&
		CountOf(data, "AvPMenu_Multiplayer[row") == CountOf(setter, "AvPMenu_Multiplayer[row"),
		"the row is put there, last, only when a server is named, and nothing else writes the table");
	Check(CountOf(menus, "MenuData_SetMultiplayerRows(") == 1 && Has(menus, "MenuData_SetMultiplayerRows(Relay_ServerName()[0] != 0, HostWhereNames());"),
		"and the menus write the rows in one place only, saying whether servers.cfg names a server (relay_list.h, Relay_ServerName)");

	std::string const opening = SourceSlice(menus, "case AVPMENU_MULTIPLAYER:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opening, "MenuData_SetMultiplayerRows(Relay_ServerName()[0] != 0, HostWhereNames());"), "each time the Multiplayer page opens, before its rows are counted");

	std::string const opened = SourceSlice(page, "bool AccessPage_Opened(AVPMENU_ELEMENT *rows)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(opened.find("if (!Relay_ServerName()[0])\n\t{\n\t\treturn false;\n\t}") != std::string::npos &&
		opened.find("if (!Relay_ServerName()[0])") < opened.find("Ask();"), "the page itself refuses to open with no server named, before it could ask anything");
	Check(Has(page, "bool AccessPage_QuestionOpened(void)\n{\n\treturn Relay_ServerName()[0] != 0;\n}"), "and so does the question before a new identity");
	Check(!Has(page, "DevMode") && !Has(menus, "DevMode"), "and -dev decides nothing on the pages or in the menus");

	std::string const pageCase = SourceSlice(menus, "case AVPMENU_ONLINE_ACCESS:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(pageCase, "if (!AccessPage_Opened(AvPMenus.MenuElements))\n\t\t\t{\n\t\t\t\tSetupNewMenu(AVPMENU_MULTIPLAYER);\n\t\t\t\treturn;"),
		"and a page that refuses sends the menus back to the Multiplayer page");

	std::string const ask = SourceSlice(list, "bool RelayList_AskForAccess(", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3b): -dev decides nothing online, so relay_list.cpp no longer asks it; the
	// page itself is the way in, and it stays -dev only until the access page's own pull request opens it (PO, 2026-11-10)
	Check(ask.find("if (!fingerprint)\n\t{\n\t\treturn false;") != std::string::npos && ask.find("if (!fingerprint)") < ask.find("Wake();")
		&& !Has(ask, "DevMode"), "relay_list.cpp refuses the call with no fingerprint to ask about, before anything is sent, and never asks -dev");

	// The pages' only way to the network is that one call, made from two places: a pressed row, and the opening once a request was sent
	bool const noOtherWay = !Has(page, "WinHttp") && !Has(page, "socket(") && !Has(page, "RelayList_FindGames") && !Has(page, "RelayList_FindRelays") &&
		!Has(page, "Net_HostGame") && CountOf(page, "RelayList_AskForAccess(") == 1;
	Check(noOtherWay, "the pages reach the network only through RelayList_AskForAccess, in one place");
	Check(CountOf(page, "\tAsk();") + CountOf(page, ": Ask();") == 2 && Has(opened, "AccessPage_MayAskByItself(sentThisRun, onItsWay, Now(), lastCallAt)"),
		"which is asked from a pressed row, or as the page opens only by the rule that needs a request sent first");

	// Refused and revoked stay so for an identity: pressing Request access again shows that answer again, and sends nothing
	std::string const pressed = SourceSlice(page, "case kAccessRowRequest:", "case kAccessRowCopy:", kSliceEndAfterTheStart, FailedSlice);
	size_t const refusedStays = pressed.find("if (needsANewIdentity)\n\t\t\t{\n\t\t\t\tstateLine = AccessPage_AnswerText(lastAnswer);\n\t\t\t\treturn false;");
	Check(refusedStays != std::string::npos && refusedStays < pressed.find("AccessPage_PressedAsk(") &&
		Has(page, "needsANewIdentity = AccessPage_NeedsANewIdentity(answer);"), "an identity the server refused or revoked asks no more, and uses up no calls");

	// The question before a new identity: No first, so an Enter pressed by chance keeps the identity
	std::string const question = SourceSlice(data, "static AVPMENU_ELEMENT AvPMenu_OnlineAccessNew[]=", "\n};\n", kSliceEndAfterTheStart, FailedSlice);
	Check(question.find("kAccessRowNewNo") != std::string::npos && question.find("kAccessRowNewNo") < question.find("kAccessRowNewYes"),
		"the question's first row is No");
}

// Beta.2's words (the owner's rulings and Secondary's dry run of the tester's path, 2026-11-09), by reading language.cpp's table and the
// page: Discord is the one place to send the code, which isn't secret; a request waits 30 days; refused and revoked say what to do next;
// busy says why; and a profile's letters left out of the name are said. Added 2026-11-09 by Melanikus for the AvP port.
static void TestTheBeta2Words()
{
	puts("4. beta.2's words (by reading the table and the page)");

	std::string const table = ReadSource("3dc/avp/language.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/access_page.cpp");
	// language.cpp is about 16 KB (2026-11-10): the sizes only show each file was found and read, so they sit well under what each holds
	Check(table.size() > 10000 && page.size() > 5000, "language.cpp and the page were found and read");

	Check(Has(table, "\"Send this code to the owner on Discord. It isn't secret: it only matches your request, and only a request with this code is approved.\","),
		"the code goes to the owner on Discord, the one place, and isn't secret: it only matches the request");
	Check(Has(table, "\"Waiting for approval. A request nobody answers in 30 days is forgotten: if you hear nothing in a month, ask again.\","),
		"a request waiting says it's forgotten after 30 days");
	Check(Has(table, "\"Not approved. Ask the owner on Discord if you think that's a mistake. To ask again, make a new identity.\",") &&
		Has(table, "\"Your approval was taken back. Ask the owner on Discord why. To ask again, make a new identity.\","),
		"refused and revoked say what a player can do next");
	Check(Has(table, "\"The server takes one request a minute from each home, and another PC there may just have asked: try again in a minute.\","),
		"and busy says why, for two PCs behind one home address");
	Check(Has(page, "RelayAccess_NameLeftLettersOut(UserProfilePtr ? UserProfilePtr->Name : NULL, sizeof(UserProfilePtr->Name)))\n\t\t{\n\t\t\tstateLine = TEXTSTRING_ACCESS_NAME_LEFT_OUT;"),
		"a profile's letters left out of the name to send are said, under the rows");
}

// Where the page's parts go, and its rows (access_page_rules.h). Added 2026-11-10 by Melanikus for the AvP port, after the owner's
// pictures: the page's title, "Your access code" and the code were drawn on top of each other, with an empty gap above the rows; then
// "we have space for some more padding", the rows not chosen too faint to find "Copy access code", and "They have to use arrows to get
// it?". The numbers are the menus' 640 by 480 grid, which is stretched to the screen, so what holds here holds at 16:9 and at 4:3 alike.
static void TestWhereThePartsGo()
{
	puts("5. where the page's parts go, and its rows");

	// A row of the small font is HUD_FONT_HEIGHT + 4 high (menus.cpp, HeightOfMenuElement): the access page has 4 rows, the question 2
	int const row = kAccessPageSmallLine + 4;
	int const titleBottom = kAccessPageTitleTop + kAccessPageBigLine;

	// The title's letters can hang below its line: menus.cpp starts a picture under a title at 106, "a few pixels clear of the tails"
	Check(kAccessPageTop >= titleBottom + kAccessPageSmallLine, "the page starts well under the title, clear of its letters' tails");
	Check(AccessPage_RowsHeight(4 * row, 4) == 4 * row + 3 * kAccessPageRowsApart && AccessPage_RowsHeight(row, 1) == row &&
		AccessPage_RowsHeight(0, 0) == 0 && kAccessPageRowsApart > 0, "the rows are kAccessPageRowsApart further apart than the menus' own, between each two");

	bool allInside = true;
	bool inOrder = true;
	bool wordsWholeWhenTheyFit = true;
	bool underWhole = true;
	bool spareShared = true;

	for (int shown = 0; shown < 2; shown++)
	{
		for (int rows = 0; rows <= 5; rows++)
		{
			int const rowsHeight = AccessPage_RowsHeight(rows * row, rows);

			for (int under = 0; under <= 3; under++)
			{
				for (int lines = -1; lines <= 40; lines++)
				{
					AccessPageLayout const at = AccessPage_Layout(shown != 0, lines, rowsHeight, under);
					int const first = shown ? at.labelTop : at.wordsTop;
					int const wordsBottom = at.wordsTop + at.wordsLines * kAccessPageSmallLine;
					int const kept = under > kAccessPageUnderKept ? under : kAccessPageUnderKept;

					allInside = allInside && first >= kAccessPageTop && at.underTop + kept * kAccessPageSmallLine <= kAccessPageBottom &&
						at.wordsLines >= 0 && at.underLines >= 0;

					if (shown)
					{
						// A clear gap under the code, of a small line at least (the owner: the code and the words nearly touched)
						inOrder = inOrder && at.labelTop + kAccessPageSmallLine <= at.codeTop &&
							at.codeTop + kAccessPageBigLine + kAccessPageSmallLine <= at.wordsTop;
					}
					else
					{
						inOrder = inOrder && at.labelTop == -1 && at.codeTop == -1;
					}

					// A small line's gap at least above the first row, and the lines under the rows below them
					inOrder = inOrder && wordsBottom + kAccessPageSmallLine <= at.rowsTop &&
						at.rowsTop + rowsHeight + kAccessPageLeastUnderGap <= at.underTop;

					// Words are cut only when there's no room for them: the lines drawn are all of them, or so many that one more, with every
					// gap at its least, would push the lines kept under the rows below the bottom edge
					int const least = (shown ? kAccessPageSmallLine + kAccessPageCodeGap + kAccessPageBigLine + kAccessPageLeastWordsGap : 0) +
						kAccessPageLeastRowsGap + rowsHeight + kAccessPageLeastUnderGap + kept * kAccessPageSmallLine;
					wordsWholeWhenTheyFit = wordsWholeWhenTheyFit && (lines < 0 ? at.wordsLines == 0 : (at.wordsLines == lines ||
						(at.wordsLines < lines && least + (at.wordsLines + 1) * kAccessPageSmallLine > kAccessPageBottom - kAccessPageTop)));

					// Up to 5 rows and 3 lines under them always fit: those lines are never cut
					underWhole = underWhole && at.underLines == under;

					// The room left is shared, up to kAccessPageMostSpare a gap, and never more than there is
					int const gaps = shown ? 4 : 3;
					spareShared = spareShared && at.spare >= 0 && at.spare <= kAccessPageMostSpare &&
						least + at.wordsLines * kAccessPageSmallLine + gaps * at.spare <= kAccessPageBottom - kAccessPageTop;
				}
			}
		}
	}

	Check(allInside, "every part, for any number of lines, up to 5 rows and 3 lines under them, sits between MENU_TOPYEDGE (130) and MENU_BOTTOMYEDGE (390)");
	Check(inOrder, "top to bottom, nothing on anything, with a small line's gap at least under the code and above the rows");
	Check(wordsWholeWhenTheyFit, "the words are cut only when there's no room left for another line");
	Check(underWhole, "the lines under the rows are kept whole, their room kept first");
	Check(spareShared, "and the room left is shared out between the gaps, up to its most, and never more than there is");

	// The three pages as they are, with the lines their words take in Hudson's pictures at 1920x1080 (2026-11-10): before a request is
	// sent, 3 lines above the rows; with the code shown, 2, and 2 under the rows ("Waiting for approval..."); the question, 2. Each has
	// room for at least one more line of words than that.
	Check(AccessPage_Layout(false, 99, AccessPage_RowsHeight(4 * row, 4), 1).wordsLines >= 4, "before a request is sent, there's room for 4 lines of words above the 4 rows");
	Check(AccessPage_Layout(true, 99, AccessPage_RowsHeight(4 * row, 4), 2).wordsLines >= 3, "with the code shown and 2 lines under the rows, 3");
	Check(AccessPage_Layout(false, 99, AccessPage_RowsHeight(2 * row, 2), 0).wordsLines >= 3, "and on the question before a new identity, 3, above its 2 rows");

	// The page the owner saw, with the code and the answer: padded, and the rows don't move when the answer changes
	AccessPageLayout const waiting = AccessPage_Layout(true, 2, AccessPage_RowsHeight(4 * row, 4), 2);
	AccessPageLayout const approved = AccessPage_Layout(true, 2, AccessPage_RowsHeight(4 * row, 4), 1);
	Check(waiting.spare > 0 && waiting.labelTop > kAccessPageTop, "with the code shown, the room left goes into the gaps, the one under the title too");
	Check(waiting.rowsTop == approved.rowsTop && waiting.underTop == approved.underTop, "and the rows stay put when Waiting... becomes Approved");

	// The rows not chosen: brighter than the menus' quarter, still clearly dimmer than the chosen one
	Check(kAccessPageRowNotChosenOutOf100 > 25 && kAccessPageRowNotChosenOutOf100 <= 75, "a row not chosen is drawn brighter than the menus' 25 out of 100, and dimmer than the chosen 100");

	// The row the page chooses: Copy access code, as the page opens with the code on it, or the moment a press first puts it there
	Check(AccessPage_ChoosesCopy(true, false, true) && AccessPage_ChoosesCopy(true, true, true), "the page opens on Copy access code when the code is on it");
	Check(AccessPage_ChoosesCopy(false, false, true), "and chooses it the moment Request access first puts the code there");
	Check(!AccessPage_ChoosesCopy(false, true, true), "but never again while the code stays, so the arrows move from wherever the player goes");
	Check(!AccessPage_ChoosesCopy(true, false, false) && !AccessPage_ChoosesCopy(false, false, false) && !AccessPage_ChoosesCopy(false, true, false),
		"and never while there's no code to copy");

	// And the menus use it, by reading their source: the title is where this file says, a row is as high, the rows start where the page
	// says and are as far apart, the rows not chosen are as bright, and the row the page chooses is taken
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/access_page.cpp");
	std::string const render = SourceSlice(menus, "static void RenderMenu(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(render, "RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);") && kAccessPageTitleTop == 70,
		"RenderMenu draws the title at y 70, where this page's rule says it is");
	Check(Has(menus, "h = HUD_FONT_HEIGHT+4;"), "a row of the small font is HUD_FONT_HEIGHT + 4 high, as this test counts it");
	Check(Has(render, "y = AccessPage_DrawTop(AvPMenus.MenuHeight, AvPMenus.NumberOfElementsInMenu);") &&
		Has(render, "y = AccessPage_DrawQuestion(AvPMenus.MenuHeight, AvPMenus.NumberOfElementsInMenu);") &&
		CountOf(menus, "AccessPage_DrawTop(") == 1 && CountOf(menus, "AccessPage_DrawQuestion(") == 1,
		"RenderMenu starts both pages' rows where the page puts them, in one place each");
	Check(Has(render, "y += HeightOfMenuElement(elementPtr) + (accessPage ? kAccessPageRowsApart : 0);") &&
		Has(render, "targetBrightness = ONE_FIXED * kAccessPageRowNotChosenOutOf100 / 100;"),
		"and draws their rows kAccessPageRowsApart further apart, the ones not chosen at kAccessPageRowNotChosenOutOf100");
	Check(Has(menus, "int const choose = AccessPage_Frame(AvPMenus.MenuElements);") &&
		Has(menus, "if (choose >= 0 && choose < AvPMenus.NumberOfElementsInMenu)\n\t\t{\n\t\t\tAvPMenus.CurrentlySelectedElement = choose;"),
		"the row the page chooses is taken, only when it's one of the page's rows");
	Check(Has(page, "chooseCopy = AccessPage_ChoosesCopy(true, codeWasShown, CodeShown());") &&
		Has(page, "if (AccessPage_ChoosesCopy(false, codeWasShown, CodeShown()))"),
		"and the page asks the rule as it opens, and on each frame");
}

// An access request goes only to the server a servers.cfg line names, and only after the player's yes to that line (PO's ruling,
// 2026-11-10). Only then is the list's key checked before a byte of the request is sent, and the 8-letter access code is safe only then
// (relay_list_text.h, RelayAccess_Code): to a list Windows merely trusts, someone on the way could drop the player's request and send one
// with a key made to match its code. Read in the source, since the call needs the network. Added 2026-11-10 by Melanikus for the AvP port;
// modified the same day (#175): the line in place of the -dev pin, which the line took out.
static void TestOnlyToTheLinesServer()
{
	puts("6. an access request goes only to the server a servers.cfg line names, after the player's yes (by reading the source)");

	std::string const list = ReadSource("3dc/relay_list.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/access_page.cpp");
	std::string const table = ReadSource("3dc/avp/language.cpp");
	Check(!list.empty() && !page.empty() && table.size() > 10000, "relay_list.cpp, the page and language.cpp were found and read");

	// No line: nothing is made or sent, and the log says why
	std::string const ask = SourceSlice(list, "bool RelayList_AskForAccess(", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const refusal = ask.find("if (!Server())\n\t{\n\t\tLogString(\"Relay list: an access request goes only to the server a \" SERVERS_CFG_FILE_NAME \" line names");
	size_t const refusalEnds = ask.find("return false;", refusal);
	Check(refusal != std::string::npos && refusalEnds != std::string::npos && refusalEnds < ask.find("\n\t}\n", refusal) + 4,
		"RelayList_AskForAccess refuses when no servers.cfg line names a server, and says why in the log");
	Check(refusal != std::string::npos && ask.find("if (!fingerprint)") < refusal && refusal < ask.find("RelayList_AccessBody(") &&
		refusal < ask.find("shared->accessWanted = true;") && refusal < ask.find("Wake();"),
		"after the fingerprint check, and before the request is made, queued or the thread woken");

	// With a line: queued only once the list has started, and the list starts only for a line the player has said yes to
	size_t const started = ask.find("if (!RelayList_AccessBody(name, statics, body, sizeof(body)) || !Start())\n\t{\n\t\treturn false;\n\t}");
	Check(started != std::string::npos && started < ask.find("shared->accessWanted = true;") && started < ask.find("Wake();"),
		"with a line, the request is queued only once the list has started");
	std::string const starting = SourceSlice(list, "bool Start(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const yes = starting.find("if (!line || !TheLineIsAgreed(*line))\n\t\t{\n\t\t\treturn false;\n\t\t}");
	Check(yes != std::string::npos && yes < starting.find("new (std::nothrow) Shared"),
		"and the list starts only for a line the player has said yes to, so a line without the yes sends no request either");

	// The request goes through Call alone, which checks the server's key against the line before a byte of it leaves
	std::string const thread = SourceSlice(list, "void AskForAccess(const char *body, const uint8_t fingerprint[kNetIdentityFingerprintBytes])", "\n\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	std::string const call = SourceSlice(list, "Answer Call(const wchar_t *method, const wchar_t *path, const char *body, char *answer, size_t room)", "\n\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(thread, "Answer const a = Call(L\"POST\", L\"/v1/access-requests\", body, accessAnswerText, sizeof(accessAnswerText));") &&
		CountOf(list, "/v1/access-requests") == 1, "the request goes out in one place, through Call");
	Check(call.find("const char *const unchecked = CheckTheServer();") < call.find("CallChecked(") && Has(call, "check.keepIt = false;"),
		"and Call checks the server first, then sends only to the key that matched, never keeping a new one");

	Check(Has(page, "stateLine = !Relay_ServerName()[0] ? TEXTSTRING_ACCESS_NO_SERVER : TEXTSTRING_ACCESS_CANT_REACH;"),
		"the page says so when no line names a server, never nothing");
	Check(Has(table, "\"No server is set, so nothing was sent.\","), "in words of its own: nothing was sent, and why");
	// The words above the rows name the server servers.cfg names, made safe first, and only ever as a %s's argument (added 2026-11-10)
	std::string const top = SourceSlice(page, "int AccessPage_DrawTop(int menuHeight, int rows)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const safe = top.find("OnlineScreens_SafeName(Relay_ServerName(), kServersCfgNameLetters, server, sizeof(server));");
	Check(safe != std::string::npos && safe < top.find("snprintf(paragraph, sizeof(paragraph), \"%s %s %s %s\", GetTextString(TEXTSTRING_ACCESS_EXPLAIN_ASKS), server,"),
		"the words above the rows name the line's server, made safe before they're written, as an argument, never the format");
	Check(Has(table, "\"Request access asks\",\n\t\"to let this PC play online. It sends the name below and a key made on this PC, and nothing else.\",") &&
		!Has(table, "Mel's server"), "and the words around the name say what Request access does, never Mel's server");

}

int main()
{
	TestWhenItAsks();
	TestTheWords();
	TestOnlyWithAServer();
	TestTheBeta2Words();
	TestWhereThePartsGo();
	TestOnlyToTheLinesServer();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
