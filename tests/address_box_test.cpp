// The test of the join page's address box: what's drawn never holds the address unless Tab is held. Written 2026-11-07 by Melanikus for
// the AvP port.
//
// WHAT IT'S FOR: players stream and share pictures of their screens, so the host's address mustn't be on them (the user's ruling: no IPs).
// address_box.h says how the box works.
//
// WHAT THIS TESTS: AddressBox_TextToDraw itself, on made-up addresses, and by reading menus.cpp, that the box and its help line are drawn
// through it, with Tab as the key. The menus need the whole game to draw, so that part is read, not run. Run it from the top of the
// repository, because that's where the source it reads is found from.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include "address_box.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fstream>
#include <sstream>
#include <string>
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

// A source file, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const char *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();

	std::string out;

	for (char c : text.str())
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool HasADigit(const char *text)
{
	for (; *text; text++)
	{
		if (isdigit((unsigned char)*text))
		{
			return true;
		}
	}

	return false;
}

static void TestWhatsDrawn()
{
	printf("What the box shows:\n");

	char shown[64];
	static const char *const addresses[] = { "192.168.0.20", "203.0.113.7", "10.0.0.1", "8.8.8.8", "255.255.255.255" };

	bool noDigits = true;
	bool sameLength = true;
	bool allDots = true;

	for (size_t a = 0; a < sizeof(addresses) / sizeof(addresses[0]); a++)
	{
		memset(shown, 'g', sizeof(shown));
		AddressBox_TextToDraw(addresses[a], false, shown, sizeof(shown));
		noDigits = noDigits && !HasADigit(shown);
		sameLength = sameLength && strlen(shown) == strlen(addresses[a]);
		allDots = allDots && strspn(shown, ".") == strlen(shown);
	}

	Check(noDigits, "with Tab not held, nothing drawn holds a digit of the address");
	Check(allDots && sameLength, "it's a dot for each character, so the player can see how much they've typed");

	AddressBox_TextToDraw("192.168.0.20", true, shown, sizeof(shown));
	Check(strcmp(shown, "192.168.0.20") == 0, "with Tab held, the address shows as it was typed, so a mistake can be seen");

	AddressBox_TextToDraw("", false, shown, sizeof(shown));
	Check(shown[0] == '\0', "an empty box shows nothing");

	AddressBox_TextToDraw(NULL, false, shown, sizeof(shown));
	Check(shown[0] == '\0', "and so does no address at all");

	char cramped[4 + 4];
	memset(cramped, 'g', sizeof(cramped));
	AddressBox_TextToDraw("192.168.0.20", true, cramped, 4);
	Check(strcmp(cramped, "192") == 0 && cramped[4] == 'g' && cramped[7] == 'g', "a small space is never written past, and still ends");

	Check(strstr(kAddressBoxHelp, "Tab") != NULL && !HasADigit(kAddressBoxHelp), "the help line names the key, and holds no address of its own");
}

static void TestTheMenusUseIt()
{
	printf("Where the menus use it, by reading:\n");

	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	std::string const field = Between(menus, "\t\tcase AVPMENU_ELEMENT_DUMMYTEXTFIELD:\n\t\tcase AVPMENU_ELEMENT_TEXTFIELD:\n\t\t{\n\t\t\t// field where user can enter text themselves", "\t\tcase AVPMENU_ELEMENT_TEXTFIELD_SMALLWRAPPED:");

	size_t draws = 0;

	for (size_t at = field.find("RenderText(TextFieldShows(elementPtr),"); at != std::string::npos; at = field.find("RenderText(TextFieldShows(elementPtr),", at + 1))
	{
		draws++;
	}

	Check(!field.empty() && draws == 4, "all four ways a text field is drawn go through what it shows");
	Check(!field.empty() && !Has(field, "RenderText(elementPtr->TextPtr"), "and none draws its text as it is");

	std::string const shows = Between(menus, "static char *TextFieldShows(AVPMENU_ELEMENT *elementPtr)", "static void RenderHelpString()");
	Check(Has(shows, "if (elementPtr->TextPtr != IPAddressString)") && Has(shows, "AddressBox_TextToDraw(IPAddressString, KeyboardInput[KEY_TAB] != 0, shown, sizeof(shown));"), "the address box shows dots, and its address only while Tab is held");

	std::string const help = Between(menus, "static void RenderHelpString()", "static void RenderConfigurationDescriptionString()");
	Check(Has(help, "if (elementPtr->TextPtr == IPAddressString)") && Has(help, "GetTextString(elementPtr->HelpString), kAddressBoxHelp);"), "its help line says how to see it");

	std::string const data = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	Check(Has(data, "{AVPMENU_ELEMENT_TEXTFIELD, {TEXTSTRING_MULTIPLAYER_ADDRESS},\t{MAX_IPADDRESS_LENGTH},\t{(int*)IPAddressString},"), "and the box on the join page is the one that holds IPAddressString");

	// Tab can't be typed into a box: the queue of typed characters drops anything below a space, and Tab is 9
	Check(Has(menus, "extern void KeyboardEntryQueue_Add(char c)\n{\n\tif (c < 32) return;"), "holding Tab types nothing into the box");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestWhatsDrawn();
	TestTheMenusUseIt();

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
