// The test for 3dc/command_line_word.h, which reads the switches on the command line as whole words. Written 2026-11-04 by Melanikus for
// the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the word after a switch, and the -l switch (LEVEL-SWITCH-1)
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It uses no files, no graphics card
// and none of the game.

#include <stdio.h>
#include <string.h>

#include <string>

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

static void Check(bool good, std::string const &what)
{
	Check(good, what.c_str());
}

static void WholeWords()
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

	// -w is read this way now (winmain.cpp)
	Check(CommandLineHasWord("-dev -w", "-w"), "-w as a word");
	Check(!CommandLineHasWord("-windowed", "-w"), "a switch starting -w isn't -w");
}

static void WordByWord()
{
	const char *const line = "  -a\t-bb  ";
	const char *at = line;
	const char *start = 0;
	size_t length = 0;

	bool const first = CommandLineNextWord(&at, &start, &length);
	Check(first && start == line + 2 && length == 2, "the first word, after the spaces in front of it");

	bool const second = CommandLineNextWord(&at, &start, &length);
	Check(second && start == line + 5 && length == 3, "the next word, after a tab");

	Check(!CommandLineNextWord(&at, &start, &length), "and then no more, with only spaces left");
	Check(!CommandLineNextWord(&at, &start, &length), "asked again, still no more, and nothing past the end is read");

	const char *empty = "";
	Check(!CommandLineNextWord(&empty, &start, &length), "an empty line has no words");

	const char *missing = 0;
	Check(!CommandLineNextWord(&missing, &start, &length), "no line at all has no words");
	Check(!CommandLineNextWord(0, &start, &length), "nowhere to start from");

	Check(CommandLineWordIs("-n", 2, "-n"), "a word is itself");
	Check(!CommandLineWordIs("-noshadows", 10, "-n"), "a longer word isn't");
	Check(!CommandLineWordIs("-n", 2, "-N"), "capitals count");
	Check(!CommandLineWordIs("-n", 1, "-n"), "a shorter word isn't");
	Check(!CommandLineWordIs(0, 2, "-n"), "no word at all");
	Check(!CommandLineWordIs("-n", 2, 0), "nothing to compare it with");
}

// What CommandLineWordAfter gives for line, with room bytes to put the word in. word gets what it put there.
static CommandLineWordAfterAnswer After(const char *line, size_t room, std::string *word)
{
	char out[64];
	memset(out, 'z', sizeof(out)); // anything left in it that isn't an end marker shows up
	out[sizeof(out) - 1] = 0;

	const CommandLineWordAfterAnswer answer = CommandLineWordAfter(line, "-n", out, room);
	*word = out;
	return answer;
}

static void TheWordAfter()
{
	std::string word;

	Check(After("-n derelict", 64, &word) == kWordAfterFound && word == "derelict", "the word after the switch");
	Check(After("-w -n derelict -dev", 64, &word) == kWordAfterFound && word == "derelict", "with other switches either side");
	Check(After("-n \t derelict", 64, &word) == kWordAfterFound && word == "derelict", "however many spaces and tabs come between");
	Check(After("-n first -n second", 64, &word) == kWordAfterFound && word == "first", "only the first one counts, as it did with strstr");

	Check(After("-n", 64, &word) == kWordAfterNothing && word.empty(), "nothing after it, and out is left empty");
	Check(After("-dev -n   ", 64, &word) == kWordAfterNothing && word.empty(), "only spaces after it");

	Check(After("-noshadows derelict", 64, &word) == kWordAfterNoSwitch && word.empty(), "a longer switch starting -n isn't -n");
	Check(After("x-n derelict", 64, &word) == kWordAfterNoSwitch, "nor is a word ending -n");
	Check(After("-N derelict", 64, &word) == kWordAfterNoSwitch, "nor -N");
	Check(After("", 64, &word) == kWordAfterNoSwitch, "an empty line");
	Check(After(0, 64, &word) == kWordAfterNoSwitch && word.empty(), "no line at all, and out is left empty");

	// Room for four letters and the end marker
	Check(After("-n abcd", 5, &word) == kWordAfterFound && word == "abcd", "a word that just fits");
	Check(After("-n abcde", 5, &word) == kWordAfterTooLong && word.empty(), "one letter more is refused whole, not cut short, and out is left empty");
	Check(After("-n a", 0, &word) == kWordAfterTooLong, "no room at all");

	Check(CommandLineWordAfter("-n derelict", "-n", 0, 64) == kWordAfterTooLong, "nowhere to put it");
	Check(CommandLineWordAfter("-n derelict", 0, 0, 0) == kWordAfterNoSwitch, "no switch to look for");
}

static void TheLoadSwitch()
{
	Check(CommandLineLoadAsks("-l", 'a'), "-l on its own asks for aliens");
	Check(!CommandLineLoadAsks("-l", 'm'), "and nothing else");
	Check(CommandLineLoadAsks("-lmp", 'm') && CommandLineLoadAsks("-lmp", 'p'), "letters after it ask for those kinds");
	Check(!CommandLineLoadAsks("-lmp", 'a'), "and not for aliens, when it has letters");
	Check(CommandLineLoadAsks("-la -w -lq", 'a') && CommandLineLoadAsks("-la -w -lq", 'q'), "every -l counts");

	Check(!CommandLineLoadAsks("-lights", 'h') && !CommandLineLoadAsks("-lights", 't') && !CommandLineLoadAsks("-lights", 's'),
		"a longer switch starting -l, with a letter that names no kind, asks for nothing");
	Check(!CommandLineLoadAsks("-lza", 'a'), "one letter that names nothing and the whole word isn't -l");
	Check(!CommandLineLoadAsks("x-lm", 'm'), "-l inside a word isn't -l");
	Check(!CommandLineLoadAsks("-L", 'a') && !CommandLineLoadAsks("-Lm", 'm'), "nor is -L");
	Check(!CommandLineLoadAsks("-dev -w", 'a'), "a line with no -l asks for nothing");
	Check(!CommandLineLoadAsks(0, 'a'), "no line at all asks for nothing");
	Check(!CommandLineLoadAsks("-l", 0) && !CommandLineLoadAsks("-la", 0), "the end marker isn't a kind");

	for (const char *letter = kCommandLineLoadLetters; *letter; letter++)
	{
		std::string const asked = std::string("-l") + *letter;
		Check(CommandLineLoadAsks(asked.c_str(), *letter), asked + " asks for its kind");
	}

	Check(CommandLineLoadWordsNotRead("-lights -lza -w") == 2, "the words starting -l that aren't -l are counted, for the log");
	Check(CommandLineLoadWordsNotRead("-l -lmp x-lz -dev") == 0, "real -l words aren't, and nor is a word with -l inside it");
	Check(CommandLineLoadWordsNotRead(0) == 0, "no line at all has none");

	Check(CommandLineIsLoadWord("-lmp", 4), "-lmp is an -l");
	Check(!CommandLineIsLoadWord("-l\0a", 4), "an end marker inside the word isn't a letter that names a kind");
	Check(!CommandLineIsLoadWord("-", 1), "a word too short to be -l");
	Check(!CommandLineIsLoadWord("-k", 2), "-k isn't -l");
	Check(!CommandLineIsLoadWord(0, 2), "no word at all");
}

int main()
{
	WholeWords();
	WordByWord();
	TheWordAfter();
	TheLoadSwitch();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all command line checks passed\n");
	return 0;
}
