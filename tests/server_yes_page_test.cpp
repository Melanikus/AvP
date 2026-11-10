// The test of the yes page: the player's yes to the server in servers.cfg, before anything goes to it (#175's last part). Written 2026-11-10
// by Melanikus for the AvP port.
//
// WHAT IT'S FOR: until the player says yes, nothing at all may go to a line's server (relay_list.h), and the page is where they answer.
// So a press must mean what its row says, the yes must be kept before the page goes on, a no must send nothing, and what the page shows
// must be the line's and the game's own words. The page's rules (server_yes_rules.h) are tried here directly. The page, the menus and the
// list need the whole game, so how they're put together is read in the code. relay_list_test reads the list's own side of the gate.
//
// Nothing here uses the network or the disk, besides reading the game's source files.

#include <stdio.h>
#include <string.h>
#include <string>

#include "server_yes_rules.h"
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

static void TestTheRows()
{
	printf("1. what each row means\n");
	Check(ServerYes_Choice(kServerYesRowYes) == kServerYesAgreeAndGoOn, "Yes, use this server keeps the yes and goes on");
	Check(ServerYes_Choice(kServerYesRowNo) == kServerYesGoBack, "No, not now goes back");
	Check(ServerYes_Choice(0) == kServerYesNothing && ServerYes_Choice(99) == kServerYesNothing && ServerYes_Choice(102) == kServerYesNothing &&
		ServerYes_Choice(-1) == kServerYesNothing, "and any other row means nothing");
	Check(kServerYesRowNo > 3 && kServerYesRowYes > 3, "the rows' numbers are far from the access page's own (0 to 3)");
}

static void TestTheLines()
{
	printf("2. the lines the page shows\n");
	char line[64];
	ServerYes_WhereLine("104.131.35.41", "on the internet", line, sizeof(line));
	Check(strcmp(line, "104.131.35.41, on the internet") == 0, "the address, then where it is");
	ServerYes_WhereLine("203.0.113.7:8443:27500", "on your own network", line, sizeof(line));
	Check(strcmp(line, "203.0.113.7:8443:27500, on your own network") == 0, "with both ports, when the line names them");
	ServerYes_KeyLine("key", "bb0e2958", "f28d663e", line, sizeof(line));
	Check(strcmp(line, "key bb0e2958 ... f28d663e") == 0, "the key's first 8 digits and its last 8, three dots between");

	char small[8];
	ServerYes_WhereLine("104.131.35.41", "on the internet", small, sizeof(small));
	Check(strcmp(small, "104.131") == 0, "a line too long for its room is cut short, and still ended");
	ServerYes_KeyLine(NULL, NULL, NULL, line, sizeof(line));
	Check(strcmp(line, "  ... ") == 0, "and nothing at all is written as nothing, never read from nowhere");
	ServerYes_WhereLine("x", "y", NULL, 10);
	Check(true, "nowhere to write is no harm");
}

static void TestTheWiring()
{
	printf("3. how the page is put together, read in the code\n");
	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const page = ReadSource("3dc/avp/win95/frontend/server_yes_page.cpp");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	Check(data.size() > 1000 && menus.size() > 1000 && page.size() > 1000 && words.size() > 1000, "the page's, the menus' and the words' sources were found and read");

	std::string const rows = SourceSlice(data, "static AVPMENU_ELEMENT AvPMenu_ServerYes[]=", "};", kSliceEndAfterTheStart, FailedSlice);
	size_t const no = rows.find("{TEXTSTRING_SERVERYES_NO},\t{kServerYesRowNo}");
	size_t const yes = rows.find("{TEXTSTRING_SERVERYES_YES},\t{kServerYesRowYes}");
	Check(no != std::string::npos && yes != std::string::npos && no < yes && rows.find("AVPMENU_ELEMENT_ENDOFMENU") > yes,
		"the page has its two rows, No first, so an Enter pressed by chance sends nothing");
	Check(Has(data, "{AVPMENU_FONT_SMALL,\tTEXTSTRING_SERVERYES_TITLE,\t\t\tAvPMenu_ServerYes,\t\t\t\t\tAVPMENU_MULTIPLAYER, 0},") &&
		Has(data, "== AVPMENU_SERVER_YES + 1,"), "its title is the question, and Esc goes back to the Multiplayer page");

	std::string const opening = SourceSlice(menus, "case AVPMENU_SERVER_YES:\n", "break;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opening, "if (!ServerYesPage_Opened())\n\t\t\t{\n\t\t\t\tSetupNewMenu(AVPMENU_MULTIPLAYER);\n\t\t\t\treturn;"),
		"a page with no line waiting for a yes goes back to the Multiplayer page");
	Check(Has(menus, "AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)\n\t{\n\t\tServerYesPage_Draw();"), "the page's words are drawn on it");
	std::string const pressing = SourceSlice(menus, "case AVPMENU_ELEMENT_ONLINE_ACCESS:\n\t\t{\n\t\t\tenum AVPMENU_ID page = AVPMENU_ONLINE_ACCESS;", "\n\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	size_t const ours = pressing.find("if (AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)");
	Check(ours != std::string::npos && ours < pressing.find("AccessPage_Pressed(") &&
		Has(pressing, "ServerYesPage_Pressed(elementPtr->MaxValue, &page)"), "its rows are pressed as its own, never as the access page's");

	std::string const pressed = SourceSlice(page, "bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const agreeing = SourceSlice(pressed, "case kServerYesAgreeAndGoOn:", "case kServerYesGoBack:", kSliceEndAfterTheStart, FailedSlice);
	std::string const goingBack = SourceSlice(pressed, "case kServerYesGoBack:", "case kServerYesNothing:", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(agreeing, "if (!RelayList_AgreeToTheLine())") && agreeing.find("RelayList_AgreeToTheLine") < agreeing.find("*page = goToAfterYes;"),
		"a yes is kept before the page goes on, and a yes that couldn't be given goes back instead");
	Check(Has(goingBack, "*page = backToAfterNo;") && !Has(goingBack, "RelayList_"), "a no goes back, and tells the list nothing");

	std::string const needed = SourceSlice(page, "bool ServerYesPage_Needed(enum AVPMENU_ID goTo, enum AVPMENU_ID backTo)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(needed, "if (!RelayList_LineToAgree(&waiting))\n\t{\n\t\treturn false;"), "the page is needed only while the list has a line waiting for a yes");
	std::string const drawing = SourceSlice(page, "void ServerYesPage_Draw(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(drawing, "OnlineScreens_SafeName(shown.name, sizeof(shown.name), name, sizeof(name));") &&
		drawing.find("OnlineScreens_SafeName") < drawing.find("RenderMenuText(name"), "the server's name, from a file from outside the game, is made safe before it's drawn");

	size_t const question = words.find("\"Play online through this server?\",\n\t\"on the internet\",\n\t\"on this PC\",\n\t\"on your own network\",\n\t\"key\",\n");
	Check(question != std::string::npos && words.find("\"Its owner approves their own players, and sees your name, your key and your address while you play.\",\n\t\"Yes, use this server\",\n\t\"No, not now\",\n") > question,
		"the words are PO's, in langenum.h's order");
}

int main()
{
	TestTheRows();
	TestTheLines();
	TestTheWiring();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
