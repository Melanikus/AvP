// The test of the console's key: the key under Esc on every keyboard layout (3dc/win95/console_key.h). Written 2026-11-09 by Melanikus for
// the AvP port.
//
// WHAT IT TESTS
//
// 1. The rule, with the lParam a real keyboard's messages carry: the key under Esc is scan code 0x29, not an extended key, pressed, held
//    (repeating), let go, or typing its letter, and no other key is. A US, UK and Brazilian ABNT2 keyboard all send 0x29 for that key;
//    what differs between them is only what they call it (VK_OEM_3, VK_OEM_8, ...), which this never looks at.
// 2. How the game uses it, which needs a window and its messages, so it's read in the source (win_proj.cpp and di_func.cpp): the console
//    key goes to the console alone, its letter isn't typed into the console or the message line, KEY_GRAVE comes from it and nothing else,
//    and the key the layout calls VK_OEM_3 still works as KEY_APOSTROPHE. That the console opens on a real keyboard needs a run of the game.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own, run from the top of the repository, where the source it reads is found from. The program
// gives back a non-zero result if anything failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "console_key.h"
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

// A key message's lParam, as Windows makes it: the repeat count in bits 0 to 15, the scan code in 16 to 23, extended in 24, the key's last
// state in 30 and its going up in 31
static uint32_t KeyData(uint32_t scanCode, bool extended, bool wasDown, bool goingUp, uint32_t repeats)
{
	return (repeats & 0xFFFF) | ((scanCode & 0xFF) << 16) | (extended ? (1u << 24) : 0) | (wasDown ? (1u << 30) : 0) | (goingUp ? (1u << 31) : 0);
}

static void TestTheRule()
{
	puts("1. which key is the console's");

	Check(ConsoleKey_IsTheKeyUnderEsc(KeyData(0x29, false, false, false, 1)) && ConsoleKey_IsTheKeyUnderEsc(0x00290001u),
		"the key under Esc, pressed: scan code 0x29, the same number as 1999's DIK_GRAVE");
	Check(ConsoleKey_IsTheKeyUnderEsc(KeyData(0x29, false, true, false, 1)) && ConsoleKey_IsTheKeyUnderEsc(KeyData(0x29, false, true, false, 300)),
		"and held, repeating, however many times");
	Check(ConsoleKey_IsTheKeyUnderEsc(KeyData(0x29, false, true, true, 1)) && ConsoleKey_IsTheKeyUnderEsc(0xC0290001u), "and let go");
	Check(!ConsoleKey_IsTheKeyUnderEsc(KeyData(0x29, true, false, false, 1)), "an extended key with the same scan code isn't it");

	struct Other
	{
		uint32_t scanCode;
		const char *what;
	};

	Other const others[] = {
		{ 0x01, "Esc itself" },
		{ 0x02, "1" },
		{ 0x0F, "Tab, under it" },
		{ 0x28, "the key left of Enter, ' on a US keyboard" },
		{ 0x2B, "the key above Enter, \\ on a US keyboard" },
		{ 0x56, "the key left of Z on a UK or ABNT2 keyboard" },
		{ 0x73, "ABNT2's key right of the full stop" },
		{ 0x00, "a key with no scan code" },
		{ 0xA9, "0x29 with the top bit set, which no key is" }
	};

	bool noneIsIt = true;

	for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++)
	{
		bool const isIt = ConsoleKey_IsTheKeyUnderEsc(KeyData(others[i].scanCode, false, false, false, 1));
		noneIsIt = noneIsIt && !isIt;

		if (isIt)
		{
			printf("          taken for the console's key: %s\n", others[i].what);
		}
	}

	Check(noneIsIt, "and no other key is: Esc, 1, Tab, ', \\, the keys only some layouts have, or none");
}

static void TestHowTheGameUsesIt()
{
	puts("2. how the game uses it (read in the source: it needs a window and its messages)");

	std::string const window = ReadSource("3dc/avp/win95/win_proj.cpp");
	std::string const input = ReadSource("win32/di_func.cpp");
	Check(!window.empty() && !input.empty(), "both sources were found and read");

	// Pressed: to the console alone, before the layout's name for it is given to the game
	// (Each case is found with its { after it, because IsSomeonesHand, above the window's own procedure, names the same messages)
	std::string const down = SourceSlice(window, "case WM_SYSKEYDOWN:\n\t\t{", "break;\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const consoleDown = down.find("if (ConsoleKey_IsTheKeyUnderEsc((uint32_t)lParam))\n\t\t\t{\n\t\t\t\tIngameKeyboardInput_ConsoleKey(true);\n\t\t\t\tbreak;\n\t\t\t}");
	Check(consoleDown != std::string::npos && consoleDown < down.find("IngameKeyboardInput_KeyDown((char)wParam);"),
		"the key under Esc, pressed, goes to the console, and never also as whatever the layout calls it");

	std::string const up = SourceSlice(window, "case WM_KEYUP:\n\t\t{", "break;\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const consoleUp = up.find("if (ConsoleKey_IsTheKeyUnderEsc((uint32_t)lParam))\n\t\t\t{\n\t\t\t\tIngameKeyboardInput_ConsoleKey(false);\n\t\t\t\tbreak;\n\t\t\t}");
	Check(consoleUp != std::string::npos && consoleUp < up.find("IngameKeyboardInput_KeyUp((char)wParam);"), "and let go, the same way");

	// Its letter: never typed into the console or the message line, still taken by the menus' text boxes
	std::string const typed = SourceSlice(window, "case WM_CHAR:\n\t\t{", "\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(typed, "bool const fromTheConsoleKey = ConsoleKey_IsTheKeyUnderEsc((uint32_t)lParam);") &&
		Has(typed, "if (!fromTheConsoleKey)\n\t\t\t{\n\t\t\t\tRE_ENTRANT_QUEUE_WinProc_AddMessage_WM_CHAR((char)wParam);") &&
		Has(typed, "if (!fromTheConsoleKey)\n\t\t\t\t{\n\t\t\t\t\tCon_AddTypedChar((char)wParam);"),
		"what that key types isn't put in the console or the message line");
	Check(Has(typed, "}\n\n\t\t\tKeyboardEntryQueue_Add((char)wParam);"), "but the menus' text boxes still get it, as a letter");

	// KEY_GRAVE, the console's own key, comes from that alone
	Check(CountOf(input, "KeyboardInput[KEY_GRAVE] = TRUE;") == 1 && Has(input, "if (consoleKeyDown)\n\t{\n\t\tKeyboardInput[KEY_GRAVE] = TRUE;") &&
		!Has(input, "IngameKeyboardInput[0xDF]") && !Has(input, "IngameKeyboardInput[VK_OEM_8]"),
		"the console's KEY_GRAVE comes from the key under Esc, and no longer from VK_OEM_8, the UK's name for it");
	Check(Has(input, "if (IngameKeyboardInput[VK_OEM_3])\n\t{\n\t\tKeyboardInput[KEY_APOSTROPHE] = TRUE;"),
		"and the key the layout calls VK_OEM_3 is still KEY_APOSTROPHE, wherever it is");

	// (Found with its { after it, since the file declares it too, further up)
	std::string const cleared = SourceSlice(input, "extern void IngameKeyboardInput_ClearBuffer(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(cleared, "consoleKeyDown = false;"), "and the console's key is let go when the keys are cleared, as every other key is");
}

int main()
{
	TestTheRule();
	TestHowTheGameUsesIt();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
