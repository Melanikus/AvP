// The test for 3dc/command_line_word.h, which decides whether the developer tools are on. Written 2026-11-04 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no files, no graphics card
// and none of the game.

#include <stdio.h>

#include "command_line_word.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	Check(CommandLineHasWord("-dev", "-dev"), "the word alone");
	Check(CommandLineHasWord("-windowed -dev", "-dev"), "the word at the end");
	Check(CommandLineHasWord("-dev -windowed", "-dev"), "the word at the start");
	Check(CommandLineHasWord("-a\t-dev\t-b", "-dev"), "tabs count as spaces");

	Check(!CommandLineHasWord("", "-dev"), "an empty line has nothing on it");
	Check(!CommandLineHasWord("-windowed", "-dev"), "another switch isn't it");
	Check(!CommandLineHasWord("-developer", "-dev"), "not the start of a longer word");
	Check(!CommandLineHasWord("c:\\games-dev", "-dev"), "not the end of a longer word");
	Check(!CommandLineHasWord("c:\\games-dev\\x", "-dev"), "not the middle of a longer word");
	Check(!CommandLineHasWord("-de", "-dev"), "a line cut short isn't it, and isn't read past its end");

	Check(CommandLineHasWord("-developer c:\\games-dev -dev", "-dev"), "found after two that only look like it");

	Check(!CommandLineHasWord(0, "-dev"), "no line at all");
	Check(!CommandLineHasWord("-dev", 0), "no word at all");
	Check(!CommandLineHasWord("-dev", ""), "an empty word isn't on any line");

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all command line checks passed\n");
	return 0;
}
