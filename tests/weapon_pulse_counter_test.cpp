// What the pulse rifle's counter should say, without the game. Written 2026-11-06 by Melanikus for the AvP port.
// The last part, added 2026-11-07, reads the game's source to check where the counter's model comes from.
//
// What a real model does with these answers needs a run of the game.

#include <limits.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "hud_rounds_number.h"
#include "weapon_pulse_counter_sums.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static unsigned int const kOneRound = 65536;
static int const kOneSecond = 65536;
static unsigned int const kFull = 99 * kOneRound;

// In the tests that aren't about the number on the screen, it has already got to what the gun holds
static int const kScreenHasGotThere = 99;

static void TestTheNumber()
{
	puts("the number it shows");

	Check(PulseCounter_WholeRounds(0) == 0, "an empty magazine shows 00");
	Check(PulseCounter_WholeRounds(kOneRound) == 1, "one round shows 01");
	Check(PulseCounter_WholeRounds(kOneRound / 2) == 1, "half a round shows 01, so 00 means really empty");
	Check(PulseCounter_WholeRounds(1) == 1, "and so does the smallest part of one");
	Check(PulseCounter_WholeRounds(kOneRound + 1) == 2, "just over one round shows 02");
	Check(PulseCounter_WholeRounds(42 * kOneRound) == 42, "a whole number shows as itself");
	Check(PulseCounter_WholeRounds(99 * kOneRound) == 99, "99 shows 99");
	Check(PulseCounter_WholeRounds(100 * kOneRound) == 99, "more than two digits hold shows 99");
	Check(PulseCounter_WholeRounds(UINT_MAX) == 99, "and so does the biggest number there is, without wrapping round to 00");

	Check(PulseCounter_Tens(42) == 4 && PulseCounter_Units(42) == 2, "42 is a 4 and a 2");
	Check(PulseCounter_Tens(7) == 0 && PulseCounter_Units(7) == 7, "7 is a 0 and a 7");
	Check(PulseCounter_Tens(0) == 0 && PulseCounter_Units(0) == 0, "0 is two 0s");
	Check(PulseCounter_Tens(99) == 9 && PulseCounter_Units(99) == 9, "99 is two 9s");
}

static void TestTheFrames()
{
	puts("which frame shows a digit");

	Check(PulseCounter_FrameForDigit(0, 10) == 0, "0 is the first frame");
	Check(PulseCounter_FrameForDigit(1, 10) == 65536, "1 is the next");
	Check(PulseCounter_FrameForDigit(9, 10) == 9 * 65536, "9 is the last of ten");
	Check(PulseCounter_FrameForDigit(9, 9) == 0, "a model with too few frames shows its first, and doesn't read past the end");
	Check(PulseCounter_FrameForDigit(1, 1) == 0, "nor with one frame");
	Check(PulseCounter_FrameForDigit(0, 0) == 0, "nor with none");
	Check(PulseCounter_FrameForDigit(-1, 10) == 0, "nor with a digit below nothing");
	Check(PulseCounter_FrameForDigit(5, -3) == 0, "nor a number of frames below nothing");
}

static void TestFollowingTheRounds()
{
	puts("following the rounds");

	PulseCounterState counter;
	PulseCounter_Start(&counter);
	Check(counter.shown == 0 && !counter.spinningDown && !counter.newMagazineIn, "it starts at nothing");

	PulseCounter_Update(&counter, 50 * kOneRound, kScreenHasGotThere, kFull, false, false, false, kOneSecond / 60);
	Check(counter.shown == 50 * kOneRound, "out of a reload it shows the rounds in the gun");
	PulseCounter_Update(&counter, 49 * kOneRound, kScreenHasGotThere, kFull, false, false, false, kOneSecond / 60);
	Check(counter.shown == 49 * kOneRound, "and follows them each frame");

	// A key frame's flags mean nothing outside a reload
	PulseCounter_Update(&counter, 48 * kOneRound, kScreenHasGotThere, kFull, false, true, false, kOneSecond / 60);
	Check(!counter.spinningDown && counter.shown == 48 * kOneRound, "the magazine-out mark means nothing outside a reload");
	PulseCounter_Update(&counter, 47 * kOneRound, kScreenHasGotThere, kFull, false, false, true, kOneSecond / 60);
	Check(!counter.newMagazineIn && counter.shown == 47 * kOneRound, "and nor does the magazine-in mark");

	PulseCounter_Update(&counter, 46 * kOneRound, kScreenHasGotThere, kFull, false, false, false, kOneSecond / 60);
	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, false, false, kOneSecond / 60);
	Check(counter.shown == 46 * kOneRound, "in a reload, until the magazine comes out, it holds what it showed, whatever the gun's count says");
}

static void TestAReload()
{
	puts("a reload");

	PulseCounterState counter;
	PulseCounter_Start(&counter);
	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, false, false, false, 1000);

	// The stock reload starts from empty, so nothing spins. The gun's rounds stay 0 until the game adds them at the end.
	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, false, false, 1000);
	Check(counter.shown == 0, "before the magazine's out, it still shows the gun");

	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, true, false, 1000);
	Check(counter.spinningDown, "the magazine coming out starts the spin");
	Check(counter.shown == 0, "from empty, there's nothing to spin");

	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, false, true, 1000);
	Check(!counter.spinningDown && counter.newMagazineIn, "the new magazine stops it");
	Check(counter.shown == kFull, "and it jumps to what the new one holds, though the game hasn't counted them yet");

	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, false, false, 1000);
	Check(counter.shown == kFull, "and stays there");

	PulseCounter_Update(&counter, kFull, kScreenHasGotThere, kFull, false, false, false, 1000);
	Check(!counter.newMagazineIn && counter.shown == kFull, "and at the reload's end the gun's own count takes over, the same number");
}

// A reload of a magazine with rounds left, as a reload key would ask for
static void TestSpinningDown()
{
	puts("spinning down");

	PulseCounterState counter;
	PulseCounter_Start(&counter);
	unsigned int const left = 99 * kOneRound;

	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, false, false, false, 0);
	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, true, false, 0);
	Check(counter.spinningDown && counter.shown == left, "with no time gone, it starts from what it showed, whatever the gun's count says now");

	PulseCounter_Update(&counter, 0, kScreenHasGotThere, kFull, true, false, false, kOneSecond / 10);
	Check(PulseCounter_WholeRounds(counter.shown) == 48, "a tenth of a second takes off 51.2 rounds, 512 a second");

	unsigned int const before = counter.shown;
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, true, false, 0);
	Check(counter.shown == before, "the magazine-out mark lasting another frame doesn't start it again");

	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, false, false, kOneSecond);
	Check(counter.shown == 0, "it stops at nothing, and doesn't wrap round");

	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, false, false, kOneSecond);
	Check(counter.shown == 0, "and stays there");

	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, false, true, 1000);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, true, false, 1000);
	Check(!counter.spinningDown && counter.shown == kFull, "a mark after the new magazine's in doesn't spin it down again");

	// A frame that long would wrap round in 32 bits to a small number, and hardly move the counter
	PulseCounter_Start(&counter);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, false, false, false, 0);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, true, false, 0);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, false, false, 8388608);
	Check(counter.shown == 0, "a very long frame takes it all the way down");

	PulseCounter_Start(&counter);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, false, false, false, 0);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, true, false, 0);
	PulseCounter_Update(&counter, left, kScreenHasGotThere, kFull, true, false, false, -1000);
	Check(counter.shown == left, "and a frame of less than no time doesn't add any");

	PulseCounter_Update(&counter, 3 * kOneRound, kScreenHasGotThere, kFull, false, false, false, 1000);
	Check(!counter.spinningDown && counter.shown == 3 * kOneRound, "leaving a reload part way through goes straight back to the gun");
}

static void Follow(PulseCounterState *counter, unsigned int rounds, int onTheScreen)
{
	PulseCounter_Update(counter, rounds, onTheScreen, kFull, false, false, false, kOneSecond / 60);
}

static void TestCountingUpWithTheScreen()
{
	puts("counting up with the number on the screen");

	PulseCounterState counter;
	PulseCounter_Start(&counter);

	Follow(&counter, kFull, 0);
	Check(counter.shown == 0, "the game has loaded with a full gun and the screen says 0, so it says 00");
	Follow(&counter, kFull, 40);
	Check(counter.shown == 40 * kOneRound, "it climbs with the screen");
	Follow(&counter, kFull, 98);
	Check(counter.shown == 98 * kOneRound, "all the way");
	Follow(&counter, kFull, 99);
	Check(counter.shown == kFull, "and they get there together");
	Follow(&counter, kFull, 101);
	Check(counter.shown == kFull, "a screen number past the gun's rounds doesn't take it past them");
	Follow(&counter, 98 * kOneRound, 99);
	Check(counter.shown == 98 * kOneRound, "a shot drops it at once, even if the screen hasn't changed yet this frame");

	Follow(&counter, 98 * kOneRound + kOneRound / 2, 98);
	Check(counter.shown == 98 * kOneRound, "part of a round counts as a round, so with 98 and a half in the gun, a screen on 98 hasn't got there");
	Follow(&counter, 98 * kOneRound + kOneRound / 2, 99);
	Check(counter.shown == 98 * kOneRound + kOneRound / 2, "a screen on 99 has, so it's the gun's own count again");

	Follow(&counter, 90 * kOneRound, 50);
	Check(counter.shown == 50 * kOneRound, "firing while it climbs, the screen still says what it shows");

	PulseCounter_Start(&counter);
	Follow(&counter, kFull, -1);
	Check(counter.shown == 0, "a number below nothing counts as nothing");
	Follow(&counter, 0xFFFFFFFFu, 65535);
	Check(counter.shown == 65535u * kOneRound, "the most the gun's count can be doesn't wrap round");
	Follow(&counter, 0xFFFFFFFFu, INT_MAX);
	Check(counter.shown == 0xFFFFFFFFu, "nor does the most the screen's number can be");
}

static void TestAfterAReload()
{
	puts("after a reload");

	PulseCounterState counter;
	PulseCounter_Start(&counter);
	Follow(&counter, 46 * kOneRound, 46);

	PulseCounter_Update(&counter, 46 * kOneRound, 46, kFull, true, false, false, 1000);
	Check(counter.shown == 46 * kOneRound, "in the reload it holds its number");
	PulseCounter_Update(&counter, 46 * kOneRound, 46, kFull, true, true, false, 1000);
	Check(counter.spinningDown && counter.shown < 46 * kOneRound, "it spins down, though the screen stays where it is");
	PulseCounter_Update(&counter, 46 * kOneRound, 46, kFull, true, false, true, 1000);
	Check(counter.shown == kFull, "it jumps when the new magazine goes in, though the screen still says 46");

	Follow(&counter, kFull, 46);
	Check(counter.shown == kFull && counter.waitingForTheScreen, "the reload is over, and it doesn't go back to 46 to climb with the screen");
	Follow(&counter, kFull, 70);
	Check(counter.shown == kFull, "it waits while the screen climbs");
	Follow(&counter, 98 * kOneRound, 71);
	Check(counter.shown == 98 * kOneRound, "if he fires meanwhile it stays with the gun");
	Follow(&counter, 98 * kOneRound, 98);
	Check(counter.shown == 98 * kOneRound && !counter.waitingForTheScreen, "the screen has caught up, so that's the end of the waiting");
	Follow(&counter, kFull, 50);
	Check(counter.shown == 50 * kOneRound, "so the next climb, as when the gun is drawn again, goes with the screen");

	PulseCounter_Start(&counter);
	Follow(&counter, 46 * kOneRound, 46);
	PulseCounter_Update(&counter, 46 * kOneRound, 46, kFull, true, false, false, 1000);
	Follow(&counter, kFull, 46);
	Check(counter.shown == 46 * kOneRound && !counter.waitingForTheScreen, "a reload that never reached the new magazine's mark made no jump, so it climbs with the screen");
}

static void TestTheNumberOnTheScreen()
{
	puts("the number on the screen");

	int const framesASecond[] = { 30, 60, 144, 200, 500, 1000 };

	for (int rate : framesASecond)
	{
		int shown = 0;
		int leftOver = 0;

		for (int frame = 0; frame < rate; frame++)
		{
			HudRounds_Move(&shown, &leftOver, 100000, kOneSecond / rate);
		}

		Check(shown >= 126 && shown <= 128, "one second at " + std::to_string(rate) + " frames a second climbs about 128 rounds: " + std::to_string(shown));
	}

	int shown = 0;
	int leftOver = 0;
	bool wentPast = false;

	for (int frame = 0; frame < 200; frame++)
	{
		HudRounds_Move(&shown, &leftOver, 99, kOneSecond / 60);
		wentPast = wentPast || shown > 99;
	}

	Check(shown == 99 && !wentPast, "it gets to what the gun holds and never goes past");

	HudRounds_Move(&shown, &leftOver, 40, kOneSecond / 60);
	Check(shown == 40, "when he fires it drops straight there");

	shown = 10;
	leftOver = 0;
	HudRounds_Move(&shown, &leftOver, 99, kHudRoundsClimbTime - 1);
	Check(shown == 10 && leftOver == kHudRoundsClimbTime - 1, "time too short for a round is kept");
	HudRounds_Move(&shown, &leftOver, 99, 1);
	Check(shown == 11 && leftOver == 0, "and used next frame");

	HudRounds_Move(&shown, &leftOver, 99, kHudRoundsClimbTime - 1);
	HudRounds_Move(&shown, &leftOver, 11, 1000);
	HudRounds_Move(&shown, &leftOver, 99, 1);
	Check(shown == 11, "time kept doesn't last past the end of a climb");

	shown = 0;
	leftOver = kHudRoundsClimbTime - 1;
	HudRounds_Move(&shown, &leftOver, 99, INT_MAX);
	Check(shown == 99 && leftOver == 0, "the longest frame there can be doesn't wrap round");

	shown = 10;
	leftOver = 0;
	HudRounds_Move(&shown, &leftOver, 99, -65536);
	Check(shown == 10 && leftOver == 0, "a frame of less than no time does nothing");
}

static void TestWhatTheReloadWillLoad()
{
	puts("what the reload will load");

	Check(PulseCounter_AfterTheReload(46 * kOneRound, 0, 3, kFull) == kFull, "with a spare magazine, a full magazine");
	Check(PulseCounter_AfterTheReload(46 * kOneRound, 60, 0, kFull) == kFull, "with enough in the pile, a full magazine");
	Check(PulseCounter_AfterTheReload(46 * kOneRound, 12, 0, kFull) == 58 * kOneRound, "with only 12 in the pile and no magazines, 58");
	Check(PulseCounter_AfterTheReload(0, 12, 0, kFull) == 12 * kOneRound, "from empty, with only 12 in the pile, 12");
	Check(PulseCounter_AfterTheReload(46 * kOneRound, 0, 0, kFull) == 46 * kOneRound, "with nothing to load, what's there");
	Check(PulseCounter_AfterTheReload(kFull, 12, 3, kFull) == kFull, "a full gun stays as it is");

	PulseCounterState counter;
	PulseCounter_Start(&counter);
	PulseCounter_Update(&counter, 46 * kOneRound, 46, 58 * kOneRound, false, false, false, 1000);
	PulseCounter_Update(&counter, 46 * kOneRound, 46, 58 * kOneRound, true, false, true, 1000);
	Check(counter.shown == 58 * kOneRound, "the jump shows what it will load, not a full magazine");
	PulseCounter_Update(&counter, 58 * kOneRound, 46, 58 * kOneRound, false, false, false, 1000);
	Check(counter.shown == 58 * kOneRound, "and when the reload ends the number doesn't change");
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

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

// The counter's digits are in the MelPyth mod's version of the pulse rifle. That's in our own file, MelPyth_MarWep.rif, so the
// player's marwep.rif is never changed. These read the source to check the game draws the gun from our file when it's there,
// and from marwep.rif, as it always did, when it isn't.
static void TestWhereTheModelComesFrom()
{
	std::string const hooks = ReadSourceFile("3dc/avp/weapon_hooks.cpp");
	std::string const weapons = ReadSourceFile("3dc/avp/weapons.cpp");
	std::string const npcs = ReadSourceFile("3dc/avp/win95/npcsetup.cpp");
	std::string const choice = Between(hooks, "char *Weapon_ModelFileFor(TEMPLATE_WEAPON_DATA const *row)\n{", "\n}\n");
	std::string const marine = Between(npcs, "if (AvP.PlayerType == I_Marine || Load_HNPC[HNPC_Marine])", "// The Predator's staff is drawn");

	Check(!hooks.empty() && !weapons.empty() && !npcs.empty(), "the game's source can be read");
	Check(Has(hooks, "static char kOwnMarineGunsFile[] = \"MelPyth_MarWep\";"), "our file's internal name is MelPyth_MarWep");
	Check(Has(hooks, "static const char kGameMarineGunsFile[] = \"MarineWeapons\";"), "and marwep.rif's is MarineWeapons");
	Check(Has(choice, "_stricmp(row->RiffName, kGameMarineGunsFile) == 0"), "only a gun the game draws from marwep.rif can come from our file");
	Check(Has(choice, "GetNamedHierarchyFromLibrary(kOwnMarineGunsFile, row->HierarchyName) != 0"), "and only when our file is loaded and has that gun");
	Check(Has(choice, "return kOwnMarineGunsFile;\n\t}\n\n\treturn row->RiffName;"), "otherwise the gun comes from the file its row names");
	Check(Has(weapons, "GetHierarchicalWeapon(Weapon_ModelFileFor(twPtr),twPtr->HierarchyName,"), "the gun in his hands is made from the file that chose");
	Check(!Has(weapons, "GetHierarchicalWeapon(twPtr->RiffName,"), "and nowhere from the row's file without asking");
	Check(Has(marine, "FILE * ownMarineGuns = avp_fopen(\"avp_huds/MelPyth_MarWep.rif\", \"r\");"), "a marine's level looks for our file");
	Check(Has(marine, "LoadedNPC tempnpc(\"MelPyth_MarWep.rif\");\n\t\t\tif (tempnpc.IsValid())"), "and loads it when it's there, keeping it only if it read");
}

int main()
{
	TestTheNumber();
	TestTheFrames();
	TestFollowingTheRounds();
	TestAReload();
	TestSpinningDown();
	TestCountingUpWithTheScreen();
	TestAfterAReload();
	TestTheNumberOnTheScreen();
	TestWhatTheReloadWillLoad();
	TestWhereTheModelComesFrom();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
