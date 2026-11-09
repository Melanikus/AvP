// The test of when the credits roll stops early, and that it shows with the menu's video on (CREDITS-1). Written 2026-11-08 by Melanikus
// for the AvP port.
//
// WHAT IT TESTS
//
// - credits_roll.h: which keys stop the roll. Any one just pressed does, whatever else is held, except F8, the screenshot key; nothing
//   is read past the list.
// - The game's own files, read as text: DoCredits no longer skips the roll while the menu's video is on, and stops by the rule above; the
//   keyboard it reads comes from the window's messages, which the roll takes in itself each frame, not from DirectInput, whose quiet
//   failure in a Release build was why the roll couldn't be left early; and F8 still doesn't count as "any key".
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/credits_roll_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/credits_roll_test.json proves each check can fail. Only a running game shows the roll itself.

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "credits_roll.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's source, by its place from the top of the repository, with Windows' line ends taken out
static std::string ReadSourceFile(const char *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static void TestWhichKeysStopTheRoll()
{
	puts("Which keys stop the roll:");

	// Ten keys, and the one left out is 7, as F8 would be
	unsigned char keys[10];
	int const leaveOut = 7;

	memset(keys, 0, sizeof(keys));
	Check(!CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "nothing pressed: the roll goes on");

	keys[3] = 1;
	Check(CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "one key pressed: it stops");

	memset(keys, 0, sizeof(keys));
	keys[0] = 1;
	Check(CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "the first key counts");

	memset(keys, 0, sizeof(keys));
	keys[9] = 1;
	Check(CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "and so does the last");

	memset(keys, 0, sizeof(keys));
	keys[leaveOut] = 1;
	Check(!CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "the screenshot key alone doesn't stop it");

	keys[2] = 1;
	Check(CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "but another key pressed with it does");

	// What was wrong before: one input held all along hid every other one. Here the list says which were pressed this frame and not the
	// last, so a held one isn't in it, and the new one is, so the roll stops.
	memset(keys, 0, sizeof(keys));
	keys[5] = 1;
	Check(CreditsRoll_AKeyWasJustPressed(keys, 10, leaveOut), "a key pressed while another is held stops it, as each key counts on its own");

	memset(keys, 0, sizeof(keys));
	keys[9] = 1;
	Check(!CreditsRoll_AKeyWasJustPressed(keys, 9, leaveOut), "nothing past the list's length is read");
	Check(!CreditsRoll_AKeyWasJustPressed(keys, 0, leaveOut) && !CreditsRoll_AKeyWasJustPressed(keys, -5, leaveOut), "a list of none, or less, reads nothing");
	Check(!CreditsRoll_AKeyWasJustPressed(0, 10, leaveOut), "and no list at all reads nothing");

	memset(keys, 0, sizeof(keys));
	keys[leaveOut] = 1;
	Check(CreditsRoll_AKeyWasJustPressed(keys, 10, -1), "with no key left out, every key counts");
}

static void TestTheGamesFiles()
{
	puts("The game's own files:");

	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const input = ReadSourceFile("win32/di_func.cpp");
	std::string const credits = Between(menus, "void DoCredits(void)\n{", "BOOL RollCreditsText(int position, char *textPtr)");
	std::string const reading = Between(input, "void DirectReadKeyboard(void)\n{", "\tReadJoysticks();");
	std::string const f8 = Between(input, "\tif (IngameKeyboardInput[VK_F8])\n", "\tif (IngameKeyboardInput[VK_F9])\n");

	Check(!credits.empty() && !reading.empty() && !f8.empty(), "the game's source can be read");

	// The roll isn't skipped with the video on any more, and stops by the rule above
	Check(!Has(credits, "MenuBackgroundFMV"), "the roll is no longer skipped while the menu's video is on");
	Check(Has(credits, "while (!FinishedCredits && !CreditsRoll_AKeyWasJustPressed(DebouncedKeyboardInput, MAX_NUMBER_OF_INPUT_KEYS, KEY_F8));"),
		"it stops when the text is done, or any key but the screenshot key is just pressed");

	// Each frame it takes in the window's messages, where the keys come from, and reads them, before it asks whether one was pressed
	Check(ComesBefore(credits, "\t\tCheckForWindowsMessages();\n", "\t\tDirectReadKeyboard();\n") && ComesBefore(credits, "\t\tDirectReadKeyboard();\n", "while (!FinishedCredits"),
		"each frame it takes the window's messages, then reads the keys, before it looks at them");

	// Why it can show with the video now: the keyboard isn't read from DirectInput, whose failed read gave up quietly in a Release build,
	// so no key was ever seen. That code is still there, switched off. If it's ever switched back on, this fails, and the reason is back.
	Check(Has(input, "void DirectReadKeyboard(void)\n{\n#if 0\n") && Has(Between(input, "void DirectReadKeyboard(void)\n{\n#if 0\n", "#endif\n"), "lpdiKeyboard->GetDeviceState"),
		"the keyboard isn't read from DirectInput, which gave up quietly in a Release build");
	Check(!Has(reading, "#if 1"), "and nothing turns that reading back on");

	// The key credits_roll.h leaves out is the one DirectReadKeyboard leaves out of "any key"
	Check(Has(f8, "KeyboardInput[KEY_F8] = TRUE;") && !Has(f8, "\n\t\tGotAnyKey = true;"), "F8 still doesn't count as any key, so leaving it out matches the rest of the game");
}

int main()
{
	TestWhichKeysStopTheRoll();
	TestTheGamesFiles();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
