// The test of a SAY mark's words and its line (3dc/say_words.h). Written 2026-11-21 by Melanikus (Frost) for the AvP port.
//
// WHAT IT'S FOR: with -dev -saymarks, what the player types after SAY (or the F2 mark key) drops a MARK line into the probe's log
// (hudson_saymarks.h), so "if I say 'stuck' in SAY log do you see it" (the owner) is answered by reading the log. The words come from a
// person's keyboard, so they are hostile input (Tao 3): only printable characters, 250 at most, written as data inside the line, and never
// made into a file name or a path.
//
// WHAT THIS TESTS: SayWords_Clean (what is kept of the words, what is changed, how many), and SayWords_MarkLine (the MARK line's fields, with
// the words between said=" and the next "), on words made to break each rule.
#include "say_words.h"

#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static void TestTheWords()
{
	Check(SayWords_Clean("STUCK") == "STUCK", "plain words are kept as they are");
	Check(SayWords_Clean(nullptr).empty(), "no words at all gives no words, not a crash");
	Check(SayWords_Clean("").empty(), "and empty words give empty words");

	std::string const many(300, 'A');
	Check(SayWords_Clean(many.c_str()).size() == 250 && kSayWordsMost == 250, "at most 250 characters are kept (the console's own most is 250 too)");

	Check(SayWords_Clean("A\nB\rC\tD") == "A?B?C?D", "a new line, a return or a tab can't start a line of its own in the log: each becomes ?");
	Check(SayWords_Clean("\x01\x1f\x7f") == "???", "and nor can any other control character");
	Check(SayWords_Clean("\x80\xe9\xff") == "???", "a byte above 126 (another code page's letter, or half of one) becomes ? too");
	Check(SayWords_Clean("SAY \"HI\" \\N") == "SAY 'HI' /N", "a double quote can't end said=\"...\" early (it becomes '), and a backslash can't start an escape (it becomes /)");
	Check(SayWords_Clean("%s%n%x") == "%s%n%x", "printf's % codes are kept as plain text: the words are never used as a format");
	Check(SayWords_Clean("..\\..\\WINDOWS") == "../../WINDOWS", "a path is only words: kept as text, never used as a file's name");

	std::string const zero = std::string("AB") + '\0' + "CD";
	Check(SayWords_Clean(zero.c_str()) == "AB", "the words end at the first zero, as the console's own string does");
}

static void TestTheLine()
{
	std::string const line = SayWords_MarkLine(3, 12.5, 740, SayWords_Clean("STUCK \"HERE\""), 1000, -200, 30000, 1024, 87, "MED05#34");
	Check(line == "MARK 3 t=12.500 f=740 said=\"STUCK 'HERE'\" me pos=1000,-200,30000 facing=1024 health=87 mod=MED05#34",
		"the MARK line: its number, game time, frame, the words between said=\" and the next \", and his place, facing, health and module");

	std::string const none = SayWords_MarkLine(1, 0.0, 1, SayWords_Clean(nullptr), 0, 0, 0, 0, 0, "");
	Check(none == "MARK 1 t=0.000 f=1 said=\"\" me pos=0,0,0 facing=0 health=0 mod=-", "a mark with no words (the F2 key) and no module still makes a whole line");

	std::string const odd = SayWords_MarkLine(2, 1.0, 2, SayWords_Clean("%s%s%s%s"), 0, 0, 0, 0, 0, "A B");
	Check(odd.find("said=\"%s%s%s%s\"") != std::string::npos, "printf's % codes in the words come out as typed");
	Check(odd.find("mod=A_B") != std::string::npos, "a space in a module's name can't split the line's fields: it becomes _");

	Check(kSayMarksNearMm == 20000 && kSayMarksSecondsBefore == 3, "a mark names every alien within 20 m and keeps the 3 seconds before it");
	Check(kSayMarksMost == 100 && kSayMarksLinesKept == 4096 && kSayMarksFileMostBytes == 64L * 1024 * 1024,
		"so a held key can't fill the disk: at most 100 marks a run, 4096 lines kept for each, and say-marks.txt never written past 64 MB");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheWords();
	TestTheLine();

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
