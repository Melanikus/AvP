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

static void TestOnlyWithDev()
{
	puts("3. nothing of it without -dev (the owner's ruling, read in the source)");

	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/access_page.cpp");
	std::string const list = ReadSource("3dc/relay_list.cpp");
	Check(!data.empty() && !menus.empty() && !page.empty() && !list.empty(), "the four sources were found and read");

	// The Multiplayer page's own table: four rows of 1999 and the Connection check's (always there: blocked-silently), then the end where
	// the row would go, and the row is nowhere in it. Modified 2026-11-10 by Melanikus for the AvP port, when the two branches met.
	std::string const table = SourceSlice(data, "static AVPMENU_ELEMENT AvPMenu_Multiplayer[]=", "\n};\n", kSliceEndAfterTheStart, FailedSlice);
	Check(CountOf(table, "{AVPMENU_ELEMENT_GOTOMENU,") == 4 && CountOf(table, "{AVPMENU_ELEMENT_TEXTFIELD,") == 1 && CountOf(table, "{AVPMENU_ELEMENT_ENDOFMENU}") == 2 &&
		!Has(table, "TEXTSTRING_ACCESS"), "the Multiplayer page's table has its five rows, then its end where the Online access row would go");
	Check(Has(data, "static const int kMultiplayerAccessRow = 5;"), "and that place is the sixth row, just after the five");

	std::string const setter = SourceSlice(data, "void MenuData_ShowOnlineAccessRow(bool developer)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	// "AvPMenu_Multiplayer[" is in the file twice: the table's own name, and the one line that writes the row
	Check(Has(setter, "AvPMenu_Multiplayer[kMultiplayerAccessRow] = developer ? OnlineAccessRow : NoOnlineAccessRow;") && CountOf(data, "AvPMenu_Multiplayer[") == 2,
		"the row is put there only when it's asked for with developer true, and nothing else writes the table");
	Check(CountOf(menus, "MenuData_ShowOnlineAccessRow(") == 1 && Has(menus, "MenuData_ShowOnlineAccessRow(DevMode != 0);"),
		"and the menus ask for it in one place only, with -dev's own answer");

	std::string const opening = SourceSlice(menus, "case AVPMENU_MULTIPLAYER:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opening, "MenuData_ShowOnlineAccessRow(DevMode != 0);"), "each time the Multiplayer page opens, before its rows are counted");

	std::string const opened = SourceSlice(page, "bool AccessPage_Opened(AVPMENU_ELEMENT *rows)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(opened.find("if (!DevMode)\n\t{\n\t\treturn false;\n\t}") != std::string::npos && opened.find("if (!DevMode)") < opened.find("Ask();"),
		"the page itself refuses to open without -dev, before it could ask anything");
	Check(Has(page, "bool AccessPage_QuestionOpened(void)\n{\n\treturn DevMode != 0;\n}"), "and so does the question before a new identity");

	std::string const pageCase = SourceSlice(menus, "case AVPMENU_ONLINE_ACCESS:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(pageCase, "if (!AccessPage_Opened(AvPMenus.MenuElements))\n\t\t\t{\n\t\t\t\tSetupNewMenu(AVPMENU_MULTIPLAYER);\n\t\t\t\treturn;"),
		"and a page that refuses sends the menus back to the Multiplayer page");

	std::string const ask = SourceSlice(list, "bool RelayList_AskForAccess(", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(ask.find("if (!DevMode || !fingerprint)\n\t{\n\t\treturn false;") != std::string::npos && ask.find("if (!DevMode") < ask.find("Wake();"),
		"relay_list.cpp refuses the call without -dev, whatever asks it, before anything is sent");

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

int main()
{
	TestWhenItAsks();
	TestTheWords();
	TestOnlyWithDev();
	TestTheBeta2Words();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
