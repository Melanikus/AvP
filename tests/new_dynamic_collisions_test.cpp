// The test for 3dc/avp/new_dynamic_collisions.h, NEWDYNAMICCOLLISIONS (issue #180), and for the places in the game that use it.
//
// WHAT IT TESTS
//
// When something slides along a wall, the game throws away what's left of a frame's move if it's small enough. In 1999 that was any
// move of 16 mm or less, whatever the frame's length, so the faster the game ran, the more slides it threw away. The new way makes
// the size shrink with a short frame. This checks:
// - with the new way off, it's always 1999's 16 mm;
// - with it on, every frame of 16 ms or longer, which is every frame at 60 frames a second or slower, is exactly 1999 too;
// - the size is never more than 16 mm or less than 1 mm, whatever frame length it's given, and never smaller for a longer frame;
// - the point of it: worked out the game's own way, in whole millimetres, a slide that 60 frames a second keeps, a faster frame keeps
//   too, and one that 60 frames a second throws away, a faster frame throws away too. With it off, 240 frames a second throws away
//   slides that 60 keeps: that's the bug;
// - the console's words, which come from whoever typed them, so anything but the few it knows is refused;
// - from the game's own files, which can't be built on their own here: the switch starts on, all three of 1999's 16s use it, the
//   console command is there for every player and says what it did, the log says it at the start, and nothing on the command line
//   changes it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It needs /FC, so that its source
// files are found.

#include <limits.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "new_dynamic_collisions.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, found from where this test's own source is.
static std::string ReadSourceFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream text;

	text << file.rdbuf();
	return text.str();
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int Count(const std::string &text, const char *what)
{
	int count = 0;
	std::string const wanted = what;

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + wanted.size()))
	{
		count++;
	}

	return count;
}

// What the text between two places in a file is, or nothing if either isn't there.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start);
	return end == std::string::npos ? "" : text.substr(start, end - start);
}

// The game's clock and its fixed-point sums, done the same way here: io.cpp's FrameCounterHandler turns a frame's whole milliseconds
// into NormalFrameTime with DIV_FIXED, where 65536 is one second, and a move is a speed times that, with MUL_FIXED. Both round down.
// TestTheGamesSums checks mathline.cpp and io.cpp still do it so.
static int FrameTimeOf(int milliseconds)
{
	return (int)(((long long)milliseconds << 16) / 1000);
}

static int MoveIn(int millimetresPerSecond, int frameTime)
{
	return (int)(((long long)millimetresPerSecond * frameTime) >> 16);
}

// Whether a slide at this speed, in a frame of this many milliseconds, is kept or thrown away
static bool SlideIsKept(int millimetresPerSecond, int milliseconds, int newWayOn)
{
	int const frameTime = FrameTimeOf(milliseconds);
	return MoveIn(millimetresPerSecond, frameTime) > NewDynamicCollisions_NothingUpTo(frameTime, newWayOn);
}

// The slowest speed, in millimetres a second, that's kept
static int SlowestKept(int milliseconds, int newWayOn)
{
	for (int speed = 0; speed <= 100000; speed++)
	{
		if (SlideIsKept(speed, milliseconds, newWayOn))
		{
			return speed;
		}
	}

	return -1;
}

static void TestOffIsAlways1999()
{
	printf("With the new way off, a move of 16 mm or less is always nothing:\n");

	int const frames[] = { INT_MIN, -65536, -1, 0, 1, 65, 262, 1047, 1048, 1114, 16384, 65536, INT_MAX };
	bool all = true;

	for (int frame : frames)
	{
		all = all && NewDynamicCollisions_NothingUpTo(frame, 0) == 16;
	}

	Check(all, "every frame length, the odd ones included, gives 16");
}

static void TestSixtyFramesAndSlowerAre1999()
{
	printf("With it on, a frame of 16 ms or longer is exactly 1999:\n");

	bool all = true;

	for (int frame = FrameTimeOf(16); frame <= 70000; frame++)
	{
		all = all && NewDynamicCollisions_NothingUpTo(frame, 1) == 16;
	}

	all = all && NewDynamicCollisions_NothingUpTo(INT_MAX, 1) == 16;

	Check(FrameTimeOf(16) == kSixtyFramesFrame, "a 16 ms frame is the one it measures from: 1048");
	Check(all, "every frame from 16 ms to over a second, and the longest there can be, gives 16");

	bool sameMoves = true;

	for (int milliseconds = 16; milliseconds <= 17; milliseconds++)
	{
		for (int speed = 0; speed <= 20000; speed++)
		{
			sameMoves = sameMoves && SlideIsKept(speed, milliseconds, 1) == SlideIsKept(speed, milliseconds, 0);
		}
	}

	Check(sameMoves, "at 60 frames a second (16 and 17 ms frames), on and off keep exactly the same slides, at every speed to 20 m/s");
}

static void TestNeverOutsideItsEnds()
{
	printf("With it on, never more than 16 mm, never less than 1 mm, and never smaller for a longer frame:\n");

	bool inside = true;
	bool neverSmaller = true;
	int before = NewDynamicCollisions_NothingUpTo(-5000, 1);

	for (int frame = -5000; frame <= 70000; frame++)
	{
		int const now = NewDynamicCollisions_NothingUpTo(frame, 1);
		inside = inside && now >= 1 && now <= 16;
		neverSmaller = neverSmaller && now >= before;
		before = now;
	}

	inside = inside && NewDynamicCollisions_NothingUpTo(INT_MIN, 1) == 1;

	Check(inside, "every frame length from below nothing to over a second, and the shortest there can be, stays from 1 to 16");
	Check(neverSmaller, "a longer frame never gives a smaller size");
	Check(NewDynamicCollisions_NothingUpTo(FrameTimeOf(4), 1) == 3, "a 4 ms frame (240 frames a second) throws away 3 mm or less");
	Check(NewDynamicCollisions_NothingUpTo(FrameTimeOf(1), 1) == 1, "a 1 ms frame throws away 1 mm or less, the least it ever is");
}

static void TestTheSlideIsKeptAtAnyFrameRate()
{
	printf("A slide is kept at the same speed at any frame rate:\n");

	// What 60 frames a second does: its clock says 16 or 17 ms a frame, and the two differ a little.
	Check(SlowestKept(16, 0) == 1064 && SlowestKept(17, 0) == 1001, "1999 at 60 frames a second keeps a slide from 1,001 or 1,064 mm/s");

	bool agrees = true;
	bool inBand = true;

	// From 2 ms (500 frames a second) to 15 ms. A 1 ms frame isn't here: there a move is so small that a slide of 2 m/s comes to 1 mm a
	// frame, and 1 mm is always nothing. Over 500 frames a second the game's clock, which counts whole milliseconds, isn't right anyway.
	for (int milliseconds = 2; milliseconds <= 15; milliseconds++)
	{
		for (int speed = 0; speed <= 20000; speed++)
		{
			bool const sixtyKeeps = SlideIsKept(speed, 16, 0) && SlideIsKept(speed, 17, 0);
			bool const sixtyThrowsAway = !SlideIsKept(speed, 16, 0) && !SlideIsKept(speed, 17, 0);
			bool const kept = SlideIsKept(speed, milliseconds, 1);

			agrees = agrees && (!sixtyKeeps || kept) && (!sixtyThrowsAway || !kept);
		}

		int const slowest = SlowestKept(milliseconds, 1);
		inBand = inBand && slowest >= 1001 && slowest <= 1064;
	}

	Check(agrees, "from 2 to 15 ms frames, every slide 60 frames a second keeps is kept, and every one it throws away is thrown away");
	Check(inBand, "from 2 to 15 ms frames, the slowest slide kept is from 1,001 to 1,064 mm/s, as at 60 frames a second");
	Check(SlowestKept(4, 0) == 4253, "THE BUG, with it off: at 240 frames a second only a slide of 4,253 mm/s or more is kept");
	Check(!SlideIsKept(2000, 4, 0) && SlideIsKept(2000, 4, 1) && SlideIsKept(2000, 16, 0),
		"a 2 m/s slide: 60 frames a second keeps it, and at 240 the old way throws it away, where the new way keeps it");
}

static void TestTheWords()
{
	printf("The console's words:\n");

	int on = 1;
	Check(NewDynamicCollisions_FromWords("", &on) && on == 0, "nothing switches it off when it's on");
	Check(NewDynamicCollisions_FromWords("  \t", &on) && on == 1, "only spaces switch it too, back on");
	Check(NewDynamicCollisions_FromWords(0, &on) && on == 0, "no words at all switch it too");

	char const *const onWords[] = { "ON", "on", " On ", "1", "1\r\n" };

	for (char const *words : onWords)
	{
		on = 0;
		Check(NewDynamicCollisions_FromWords(words, &on) && on == 1, std::string("\"") + words + "\" is on");
	}

	char const *const offWords[] = { "OFF", "off", "\tOfF", "0", "0 " };

	for (char const *words : offWords)
	{
		on = 1;
		Check(NewDynamicCollisions_FromWords(words, &on) && on == 0, std::string("\"") + words + "\" is off");
	}

	char const *const refused[] = { "O", "OF", "ONN", "OFFF", "ON OFF", "2", "-1", "00", "01", "1x", "TRUE", "YES", "\x80", "O\xffN" };

	for (char const *words : refused)
	{
		on = 1;
		bool const tookItWhenOn = NewDynamicCollisions_FromWords(words, &on);
		bool const stayedOn = on == 1;
		on = 0;
		bool const tookItWhenOff = NewDynamicCollisions_FromWords(words, &on);
		bool const stayedOff = on == 0;

		Check(!tookItWhenOn && stayedOn && !tookItWhenOff && stayedOff, std::string("\"") + words + "\" is refused, and changes nothing either way");
	}

	std::string const said[] = { NewDynamicCollisions_Words(1), NewDynamicCollisions_Words(0), kNewDynamicCollisionsRefused };
	bool fits = true;

	for (std::string const &words : said)
	{
		fits = fits && words.size() < 100 && words.compare(0, 20, "NEWDYNAMICCOLLISIONS") == 0;
	}

	Check(fits, "every line it says starts with its name, and fits the command's 100-letter space");
	Check(Has(NewDynamicCollisions_Words(1), " ON:") && Has(NewDynamicCollisions_Words(0), " OFF:"), "the lines say which it is");
}

static void TestTheGamesFiles()
{
	printf("The game's own files:\n");

	std::string const dynamics = ReadSourceFile("3dc/avp/dynamics.cpp");
	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const io = ReadSourceFile("3dc/win95/io.cpp");
	std::string const mathline = ReadSourceFile("3dc/mathline.cpp");

	Check(!dynamics.empty() && !commands.empty() && !winmain.empty() && !io.empty() && !mathline.empty(), "the files are found");

	Check(Has(dynamics, "int NewDynamicCollisionsOn = 1;"), "the switch starts on");
	Check(Has(dynamics, "return NewDynamicCollisions_NothingUpTo(NormalFrameTime, NewDynamicCollisionsOn);"),
		"the game asks with this frame's length and the switch");
	Check(Count(dynamics, "<=NothingUpTo()") == 3 && !Has(dynamics, "<=16)"), "all three of 1999's 16s ask it, and none is left");
	Check(Has(dynamics, "if (MUL_FIXED(highestBoundary-lowestBoundary,dynPtr->DistanceLeftToMove)<=NothingUpTo())"),
		"the search for the spot short of the wall is one of them");

	std::string const command = Between(commands, "static void NewDynamicCollisionsCommand(char *words)", "\n}");
	Check(Has(command, "NewDynamicCollisions_FromWords(words, &NewDynamicCollisionsOn)"), "the command reads its words with the rule here");
	Check(Has(command, "NewOnScreenMessage(message);") && Has(command, "LogString(message);"), "the command says what it did, on the screen and in the log");

	std::string::size_type const made = commands.find("\"NEWDYNAMICCOLLISIONS\",");
	std::string::size_type const debugOnly = commands.find("bool IsACheat = true;");
	Check(made != std::string::npos && debugOnly != std::string::npos && made < debugOnly,
		"the console command is made with the players' settings, before the debugging commands");

	std::string const making = Between(commands, "\"NEWDYNAMICCOLLISIONS\",", ");");
	Check(Has(making, "NewDynamicCollisionsCommand") && !Has(making, "IsACheat") && !Has(making, "true"),
		"the console command calls the game's command, and isn't a cheat");

	Check(Has(winmain, "LogString(NewDynamicCollisions_Words(NewDynamicCollisionsOn));"), "the log says at the start which way it is");

	// No command-line switch: the owner's choice. Nothing but the console command and the switch's own line may set it.
	bool const onlyTheseSetIt = Count(dynamics, "NewDynamicCollisionsOn =") == 1 && Count(winmain, "NewDynamicCollisionsOn =") == 0
		&& Count(commands, "NewDynamicCollisionsOn =") == 0 && Count(commands, "&NewDynamicCollisionsOn") == 1;
	std::string lowered = winmain;

	for (char &c : lowered)
	{
		c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}

	Check(onlyTheseSetIt && !Has(lowered, "-newdynamic") && !Has(lowered, "newdynamiccollisions\""), "nothing on the command line changes it");

	Check(Has(io, "NormalFrameTime = DIV_FIXED(fcnt,TimerFrame);"), "io.cpp's clock still turns whole milliseconds into the frame's length");
	Check(Has(mathline, "__int64 cc = (aa << ONE_FIXED_SHIFT) / bb;") && Has(mathline, "return (((__int64)a * b) >> ONE_FIXED_SHIFT) & 0xFFFFFFFF;"),
		"DIV_FIXED and MUL_FIXED still round down, as this test's own sums do");
}

int main()
{
	TestOffIsAlways1999();
	TestSixtyFramesAndSlowerAre1999();
	TestNeverOutsideItsEnds();
	TestTheSlideIsKeptAtAnyFrameRate();
	TestTheWords();
	TestTheGamesFiles();

	if (checksFailed)
	{
		printf("\n%d check(s) FAILED\n", checksFailed);
		return 1;
	}

	printf("\nEvery check passed\n");
	return 0;
}
