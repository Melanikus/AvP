// The test of the thinking behind Hudson playing a level on foot. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 3dc/avp/hudson_play_steps.h: reading a level's list of steps, which comes from outside the code, and the sums for steering and for getting stuck. Every refusal of a
// bad file is checked by its exact reason, so a later check can't hide a missing one. It also reads the real list of each of the 32 levels.
//
// WHAT IT DOESN'T TEST
//
// Whether he really gets through a level, or whether the lists are right about the levels. hudson_play.cpp is too tied to the rest of the game to compile here, so only the
// game building checks it, and only a run shows what it does. In particular a sum here being right doesn't show the game turns the way the sum says it does.

#include <stdio.h>
#include <string>
#include <vector>

#include "hudson_play_steps.h"

static int checksFailed = 0;

static void Check(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A list that has to be refused, for exactly this reason, and with nothing kept from it
static void Refused(std::string const &text, std::string const &reason)
{
	std::vector<HudsonPlayStep> steps;
	std::string whyNot;
	bool const read = HudsonPlay_ReadSteps(text, &steps, &whyNot);

	if (read || whyNot != reason)
	{
		printf("    wanted \"%s\", got \"%s\"\n", reason.c_str(), read ? "(it was read)" : whyNot.c_str());
	}

	Check(!read && whyNot == reason && steps.empty(), reason.c_str());
}

static std::vector<HudsonPlayStep> Read(std::string const &text, char const *what)
{
	std::vector<HudsonPlayStep> steps;
	std::string whyNot;
	bool const read = HudsonPlay_ReadSteps(text, &steps, &whyNot);

	if (!read)
	{
		printf("    refused: %s\n", whyNot.c_str());
	}

	Check(read, what);
	return steps;
}

static void TestAGoodList()
{
	printf("A good list\n");

	std::vector<HudsonPlayStep> const steps = Read("# a comment\n\npress 1 2 3 | one\r\nwalk -4 5 -6 | two words  \npickup 7 8 9 | three\ndestroy 10 11 12 | four\nkill 13 14 15 | five", "every kind of step is read, with its comments, blank lines and line ends");

	Check(steps.size() == 5, "five steps");

	if (steps.size() == 5)
	{
		Check(steps[0].kind == kHudsonPlayPress && steps[0].x == 1 && steps[0].y == 2 && steps[0].z == 3 && steps[0].name == "one", "press, with its place and name");
		Check(steps[1].kind == kHudsonPlayWalkInto && steps[1].x == -4 && steps[1].y == 5 && steps[1].z == -6 && steps[1].name == "two words", "walk, numbers below nothing, a name of two words and the spaces after it gone");
		Check(steps[2].kind == kHudsonPlayPickUp, "pickup");
		Check(steps[3].kind == kHudsonPlayDestroy, "destroy");
		Check(steps[4].kind == kHudsonPlayKill, "kill");
	}

	std::vector<HudsonPlayStep> reused;
	std::string whyNot;

	HudsonPlay_ReadSteps("press 1 2 3 | one\n", &reused, &whyNot);
	Check(HudsonPlay_ReadSteps("kill 4 5 6 | two\n", &reused, &whyNot) && reused.size() == 1 && reused[0].name == "two", "nothing is left over from a file read earlier");

	// The most that's allowed of everything
	Check(Read("press 64000000 -64000000 0 | a\n", "the furthest place there is").size() == 1, "and it's one step");
	Check(Read("press 1 2 3 | " + std::string(80, 'n') + "\n", "the longest name").size() == 1, "and it's one step");
	Check(Read("press 1 2 3 | ~ !\n", "the first and last plain characters are fine in a name").size() == 1, "and it's one step");

	std::string fullList;

	for (int i = 0; i < 256; i++)
	{
		fullList += "press 1 2 3 | a\n";
	}

	Check(Read(fullList, "the most steps").size() == 256, "and all of them are kept");
	Read("# " + std::string(298, 'x') + "\npress 1 2 3 | a\n", "the longest line");
	Read(std::string(256 * 1024 - 17, '\n') + "press 1 2 3 | a\n\n", "the biggest file");
}

static void TestBadFiles()
{
	printf("Bad files\n");

	Refused("", "there are no steps");
	Refused("# only a comment\n\n", "there are no steps");
	Refused(std::string(256 * 1024 + 1, '\n'), "the file is too big");
	Refused("# " + std::string(299, 'x') + "\n", "line 1: the line is too long");
	Refused("press 1 2 3\n", "line 1: a step needs a bar and a name");
	Refused("press 1 2 3 |\n", "line 1: a step needs a bar and a name");
	Refused("press 1 2 3 | " + std::string(81, 'n') + "\n", "line 1: that can't be a name");
	Refused("press 1 2 3 | a\tb\n", "line 1: that can't be a name");
	Refused(std::string("press 1 2 3 | a\x7f" "b\n"), "line 1: that can't be a name");
	Refused(std::string("press 1 2 3 | a\xc3\xa9\n"), "line 1: that can't be a name");
	Refused("press 1 2 3 | a | b\n", "line 1: that can't be a name");
	Refused("jump 1 2 3 | a\n", "line 1: this file doesn't use that word");
	Refused("Press 1 2 3 | a\n", "line 1: this file doesn't use that word");
	Refused(" | a\n", "line 1: this file doesn't use that word");
	Refused(std::string("\xef\xbb\xbfpress 1 2 3 | a\n"), "line 1: this file doesn't use that word");
	Refused("press 1 2 | a\n", "line 1: a step needs a place");
	Refused("press 1 2 3 4 | a\n", "line 1: a step needs a place");
	Refused("press  1 2 3 | a\n", "line 1: a step needs a place");
	Refused("press | a\n", "line 1: a step needs a place");
	Refused("press  1 2 | a\n", "line 1: that isn't a place");
	Refused("press x 2 3 | a\n", "line 1: that isn't a place");
	Refused("press 1 x 3 | a\n", "line 1: that isn't a place");
	Refused("press 1 2 x | a\n", "line 1: that isn't a place");
	Refused("press 1a 2 3 | a\n", "line 1: that isn't a place");
	Refused("press 1 2 3/ | a\n", "line 1: that isn't a place");
	Refused("press - 2 3 | a\n", "line 1: that isn't a place");
	Refused("press 64000001 2 3 | a\n", "line 1: that isn't a place");
	Refused("press 1 -64000001 3 | a\n", "line 1: that isn't a place");
	Refused("press 99999999 2 3 | a\n", "line 1: that isn't a place");

	// This one wraps round to 1 in a whole number of the usual size, so without the limit on digits it would look like a good place
	Refused("press 4294967297 2 3 | a\n", "line 1: that isn't a place");

	std::string tooMany;

	for (int i = 0; i < 257; i++)
	{
		tooMany += "press 1 2 3 | a\n";
	}

	Refused(tooMany, "line 257: too many steps");

	// Nothing is kept from a file with one bad line in it, and the reason gives the right line
	Refused("press 1 2 3 | a\n\n# comment\nnope\n", "line 4: a step needs a bar and a name");
}

static void TestSteering()
{
	printf("Steering\n");

	Check(HudsonPlay_AngleToTurn(0, 65536, 0, 100) == 0, "facing it: no turn");
	Check(HudsonPlay_AngleToTurn(0, 65536, 100, 0) == 1024, "it's on his right: a quarter of a turn to the right");
	Check(HudsonPlay_AngleToTurn(0, 65536, -100, 0) == -1024, "it's on his left: a quarter of a turn to the left");
	Check(HudsonPlay_AngleToTurn(65536, 0, 0, 100) == -1024, "facing the other way, the same sum");

	// Facing almost straight back, a hair to one side, so that the plain sum comes to three quarters of a turn one way. The short way is a quarter of a turn the other.
	Check(HudsonPlay_AngleToTurn(-1, -65536, 100, 0) > -1030 && HudsonPlay_AngleToTurn(-1, -65536, 100, 0) < -1020, "the short way round, to the left, when the plain sum says three quarters to the right");
	Check(HudsonPlay_AngleToTurn(1, -65536, -100, 0) > 1020 && HudsonPlay_AngleToTurn(1, -65536, -100, 0) < 1030, "the short way round, to the right, when the plain sum says three quarters to the left");
	Check(HudsonPlay_AngleToTurn(0, 0, 100, 0) == 0, "facing nowhere: no turn");
	Check(HudsonPlay_AngleToTurn(65536, 0, 0, 0) == 0, "going nowhere: no turn");

	// A frame of a sixtieth of a second turns 2000 x 1092 / 65536 = 33 units at full
	Check(HudsonPlay_TurnControl(4000, 2000, 1092) == 65536, "further than a frame can turn: full to the right");
	Check(HudsonPlay_TurnControl(-4000, 2000, 1092) == -65536, "further than a frame can turn: full to the left");

	int const control = HudsonPlay_TurnControl(10, 2000, 1092);
	int const turned = (int)((double)control / 65536.0 * 2000.0 * 1092.0 / 65536.0 + 0.5);

	Check(control > 0 && control < 65536 && turned == 10, "just enough to turn the rest of the way, and no further");
	Check(HudsonPlay_TurnControl(-10, 2000, 1092) < 0, "and the same to the left");
	Check(HudsonPlay_TurnControl(0, 2000, 1092) == 0, "nothing to turn: no control");
	Check(HudsonPlay_TurnControl(100, 2000, 0) == 0, "a frame that takes no time can't turn");
	Check(HudsonPlay_TurnControl(100, 2000, -5) == 0, "nor one that takes less than none");
	Check(HudsonPlay_TurnControl(100, 0, 1092) == 0, "nor can a player who has no turning speed");

	Check(HudsonPlay_MayWalk(0) && HudsonPlay_MayWalk(511) && HudsonPlay_MayWalk(-511), "roughly facing the way: he walks");
	Check(!HudsonPlay_MayWalk(512), "too far to the right: he turns first");
	Check(!HudsonPlay_MayWalk(-512), "too far to the left: he turns first");
	Check(HudsonPlay_AimedWell(0) && HudsonPlay_AimedWell(59) && HudsonPlay_AimedWell(-59), "aimed: he fires");
	Check(!HudsonPlay_AimedWell(60), "too far to the right: he doesn't fire yet");
	Check(!HudsonPlay_AimedWell(-60), "too far to the left: he doesn't fire yet");

	Check(HudsonPlay_ReachedPoint(0, 0, 0, 700, 2500), "standing on it");
	Check(HudsonPlay_ReachedPoint(700, 0, 0, 700, 2500), "just close enough across");
	Check(!HudsonPlay_ReachedPoint(701, 0, 0, 700, 2500), "too far along one way");
	Check(!HudsonPlay_ReachedPoint(0, 701, 0, 700, 2500), "too far along the other");
	Check(!HudsonPlay_ReachedPoint(600, 600, 0, 700, 2500), "too far on the slant, though close along each way");
	Check(HudsonPlay_ReachedPoint(0, 0, 2500, 700, 2500) && HudsonPlay_ReachedPoint(0, 0, -2500, 700, 2500), "just close enough up and down");
	Check(!HudsonPlay_ReachedPoint(0, 0, 2501, 700, 2500), "the floor below isn't reached");
	Check(!HudsonPlay_ReachedPoint(0, 0, -2501, 700, 2500), "nor is the floor above");
	Check(!HudsonPlay_ReachedPoint(65536, 65536, 0, 700, 2500), "a distance whose square is too big for a whole number doesn't wrap round to nothing");
}

static void TestGettingStuck()
{
	printf("Getting stuck\n");

	Check(HudsonPlay_NextRescue(0) == kHudsonPlayBackUp, "first he backs up");
	Check(HudsonPlay_NextRescue(1) == kHudsonPlayStepLeft, "then he steps left");
	Check(HudsonPlay_NextRescue(2) == kHudsonPlayStepRight, "then right");
	Check(HudsonPlay_NextRescue(3) == kHudsonPlayFindTheWayAgain, "then he looks for the way again");
	Check(HudsonPlay_NextRescue(4) == kHudsonPlayCrouch, "then he crouches");
	Check(HudsonPlay_NextRescue(5) == kHudsonPlayJump, "then he jumps");
	Check(HudsonPlay_NextRescue(6) == kHudsonPlayBackUp, "then he has a second go, backing up");
	Check(HudsonPlay_NextRescue(7) == kHudsonPlayStepRight, "stepping the other way first this time");
	Check(HudsonPlay_NextRescue(8) == kHudsonPlayStepLeft, "then back");
	Check(HudsonPlay_NextRescue(9) == kHudsonPlayFindTheWayAgain, "and looking again");
	Check(HudsonPlay_NextRescue(10) == kHudsonPlayTeleport, "and only then is he moved on, which is a cheat");
	Check(kHudsonPlayStuckAfter <= 1000, "pushing and getting nowhere is noticed within a second");
	Check(HudsonPlay_NextRescue(1000) == kHudsonPlayTeleport, "and that goes on for as long as he's stuck");

	// The log is how anyone finds out what he did, so each has to say something different
	HudsonPlayRescue const all[] = { kHudsonPlayBackUp, kHudsonPlayFindTheWayAgain, kHudsonPlayStepLeft, kHudsonPlayStepRight, kHudsonPlayCrouch, kHudsonPlayJump, kHudsonPlayTeleport };
	bool allDifferent = true;

	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
	{
		for (size_t j = i + 1; j < sizeof(all) / sizeof(all[0]); j++)
		{
			allDifferent = allDifferent && std::string(HudsonPlay_RescueWords(all[i])) != HudsonPlay_RescueWords(all[j]);
		}
	}

	Check(allDifferent, "the log says something different for each");
	Check(std::string(HudsonPlay_RescueWords(kHudsonPlayTeleport)) == "he's moved on along his way", "and says when he's moved");
}

static void TestWalkingHard()
{
	printf("How hard he walks\n");

	Check(HudsonPlay_WalkControl(0) == 65536, "facing it, flat out");
	Check(HudsonPlay_WalkControl(kHudsonPlayFacingItWithin) == 65536, "and nearly facing it, the same");
	Check(HudsonPlay_WalkControl(kHudsonPlayFacingItWithin + 1) < 65536, "and any more than that, slower");
	Check(HudsonPlay_WalkControl(512) == kHudsonPlaySlowestWalk, "as far off as he may walk, the slowest");
	Check(HudsonPlay_WalkControl(5000) == kHudsonPlaySlowestWalk, "and never less");
	Check(HudsonPlay_WalkControl(-512) == HudsonPlay_WalkControl(512) && HudsonPlay_WalkControl(-200) == HudsonPlay_WalkControl(200), "turning left is the same as right");
	Check(HudsonPlay_WalkControl(100) > HudsonPlay_WalkControl(300) && HudsonPlay_WalkControl(300) > HudsonPlay_WalkControl(500), "the further off, the slower, all the way");
	Check(HudsonPlay_WalkControl(511) >= kHudsonPlaySlowestWalk, "and it doesn't go below the slowest on the way there");
}

static void TestHowFarOff()
{
	printf("How far he is from the way\n");

	Check(HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, 500, 0) == 0, "on the line, nothing");
	Check(HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, 500, 300) == 300, "beside it, how far");
	Check(HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, 500, -300) == 300, "the other side, the same");
	Check(HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, -300, 400) == 500, "before its start, how far from the start");
	Check(HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, 1300, 400) == 500, "past its end, how far from the end");
	Check(HudsonPlay_HowFarOffTheLine(100, 100, 100, 100, 400, 500) == 500, "two points in one place are just a point, with nothing divided by nothing");

	double along = -1.0;

	HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, 250, 100, &along);
	Check(along == 0.25, "it says how far along the line the nearest place is");
	HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, -500, 100, &along);
	Check(along == 0.0, "from nothing, before its start");
	HudsonPlay_HowFarOffTheLine(0, 0, 1000, 0, 1500, 100, &along);
	Check(along == 1.0, "to all of it, past its end");
}

static void TestFollowingTheWay()
{
	printf("Following the way\n");

	struct Point { int x, y, z; };
	std::vector<Point> const way = { { 0, 1000, 0 }, { 1000, 1000, 0 }, { 2000, 1000, 0 }, { 3000, 1000, 0 }, { 4000, 1000, 0 }, { 5000, 1000, 0 } };

	HudsonPlayAim aim = HudsonPlay_AimAlongTheWay(way, 0, 500, 1000, 0);

	Check(aim.x == 500 + kHudsonPlayAimAhead && aim.z == 0 && aim.y == 1000, "on the line, he aims a little way further along it");
	Check(aim.nextPoint == 1 && aim.off == 0 && aim.onTheWay, "and he knows which point is next, and that he's on the way");
	Check(aim.furtherX == 500 + kHudsonPlayLookFurther, "and which way it's going further on");

	aim = HudsonPlay_AimAlongTheWay(way, 1, 1500, 1000, 700);
	Check(aim.x == 1500 + kHudsonPlayAimAhead && aim.z == 0 && aim.off == 700, "off to one side, he aims back at the line ahead of him, not straight across to it");
	Check(aim.nextPoint == 2, "and it's found where he is along the line");

	// The heart of it: having gone past a point, he never aims back at it
	aim = HudsonPlay_AimAlongTheWay(way, 1, 2500, 1000, 200);
	Check(aim.x > 2500 && aim.nextPoint == 3, "having passed points, he never aims back at them");
	aim = HudsonPlay_AimAlongTheWay(way, 3, 500, 1000, 0);
	Check(aim.nextPoint == 3 && aim.x >= 2000, "and a part of the way behind where he's got to isn't looked at, even if he's next to it");

	// The way turns
	std::vector<Point> const corner = { { 0, 0, 0 }, { 1000, 0, 0 }, { 2000, 0, 0 }, { 2000, 0, 1000 }, { 2000, 0, 2000 } };

	aim = HudsonPlay_AimAlongTheWay(corner, 1, 1500, 0, 0);
	Check(aim.x == 2000 && aim.z == 1000 - (2000 - 1500) + 0 + 0 && aim.nextPoint == 2, "the place he aims for goes round a corner, by the distance along the way");
	Check(aim.furtherX == 2000 && aim.furtherZ == 2000, "and it stops at the way's end, however far it's asked to look");

	// The stair on invasion, 2026-11-06: on the low floor beside it, with the way on the treads above him
	std::vector<Point> const stair = { { 5804, 3868, 10989 }, { 6104, 3478, 10989 }, { 6404, 3478, 11289 }, { 6704, 3088, 11589 }, { 6704, 2699, 12489 } };

	aim = HudsonPlay_AimAlongTheWay(stair, 4, 6704, 3999, 11600);
	Check(!aim.onTheWay && aim.nextPoint == 4, "a part of the way more than two treads above him isn't on his level, even right beside him, and it says so");
	aim = HudsonPlay_AimAlongTheWay(stair, 1, 5804, 3868, 10989);
	Check(aim.onTheWay && aim.nextPoint == 1, "from the floor at the foot of the first tread he's on it");

	std::vector<Point> const level = { { 0, 2000, 0 }, { 1000, 2000, 0 }, { 2000, 2000, 0 } };

	Check(HudsonPlay_AimAlongTheWay(level, 1, 500, 2000 + kHudsonPlayOnTheSameLevel, 0).onTheWay, "so far below is still the same level");
	Check(!HudsonPlay_AimAlongTheWay(level, 1, 500, 2000 + kHudsonPlayOnTheSameLevel + 1, 0).onTheWay, "and a little more isn't");
	Check(HudsonPlay_AimAlongTheWay(level, 1, 500, 2000 - kHudsonPlayOnTheSameLevel, 0).onTheWay, "so far above, the same");
	Check(!HudsonPlay_AimAlongTheWay(level, 1, 500, 2000 - kHudsonPlayOnTheSameLevel - 1, 0).onTheWay, "and a little more isn't");

	// The end of the way
	Check(HudsonPlay_AimAlongTheWay(way, 5, 5000, 1000, 0).nextPoint == way.size(), "at the last point, he's at the end");
	Check(HudsonPlay_AimAlongTheWay(way, 5, 5000 - kHudsonPlayPointReachedAcross, 1000, 0).nextPoint == way.size(), "and just close enough counts");
	Check(HudsonPlay_AimAlongTheWay(way, 5, 5000 - kHudsonPlayPointReachedAcross - 1, 1000, 0).nextPoint == 5, "and just too far doesn't");
	Check(HudsonPlay_AimAlongTheWay(way, 5, 5000, 1000 + kHudsonPlayPointReachedUpOrDown + 1, 0).nextPoint == 5, "and nor does right under it, more than a step down");
	Check(HudsonPlay_AimAlongTheWay(way, 6, 5000, 1000, 0).nextPoint == way.size(), "past the end he stays at the end, and reads nothing he shouldn't");

	std::vector<Point> const nothing;

	Check(HudsonPlay_AimAlongTheWay(nothing, 0, 1, 2, 3).nextPoint == 0, "no way at all is the end of it");

	std::vector<Point> const onePoint = { { 7000, 0, 0 } };

	aim = HudsonPlay_AimAlongTheWay(onePoint, 0, 0, 0, 0);
	Check(aim.x == 7000 && aim.nextPoint == 0, "a way of one point is just that point");
	Check(HudsonPlay_AimAlongTheWay(onePoint, 0, 7000, 0, 0).nextPoint == 1, "and being there is the end of it");

	// A way that comes back past him, far along: it mustn't be taken for where he is
	std::vector<Point> far;

	for (int i = 0; i < 30; i++)
	{
		Point const point = { 0, 0, i < 15 ? i * 1000 : (29 - i) * 1000 };

		far.push_back(point);
	}

	aim = HudsonPlay_AimAlongTheWay(far, 1, 0, 0, 100);
	Check(aim.nextPoint == 1, "a way that comes back past him further on doesn't make him skip what's between");
	Check(kHudsonPlayPartsToLookAhead < 15, "it only looks a little way ahead");

	Point const endOfFar = far[29];

	Check(HudsonPlay_AimAlongTheWay(far, 1, endOfFar.x, endOfFar.y, endOfFar.z).nextPoint == 1, "and nor does being right by its far end");

	Check(kHudsonPlayPointReachedUpOrDown == 450, "one step is what the game lifts him");
	Check(kHudsonPlayAimAhead <= 1000, "and he aims close enough ahead that he cuts little off a corner");
}

static void TestWalkControls()
{
	printf("Forward and sidestep together\n");

	int forward = 0, side = 0;

	HudsonPlay_WalkControls(1.0, 0.0, 65536, &forward, &side);
	Check(forward == 65536 && side == 0, "facing it, all forward");
	HudsonPlay_WalkControls(0.0, 1.0, 65536, &forward, &side);
	Check(forward == 0 && side == 65536, "with it on his right, all sidestep");
	HudsonPlay_WalkControls(0.0, -1.0, 65536, &forward, &side);
	Check(forward == 0 && side == -65536, "and on his left, the other way");
	HudsonPlay_WalkControls(-1.0, 0.0, 65536, &forward, &side);
	Check(forward == -65536 && side == 0, "behind him, he backs along it without turning round");
	HudsonPlay_WalkControls(0.6, 0.8, 32768, &forward, &side);
	Check(forward == 19660 && side == 26214, "and how hard is shared between them");
}

static void TestHopping()
{
	printf("A hop along his way\n");

	struct Point { int x, y, z; };
	std::vector<Point> const way = { { 0, 0, 0 }, { 1000, 0, 0 }, { 2000, 0, 0 }, { 3000, 0, 0 }, { 4000, 0, 0 }, { 4000, 0, 3000 } };

	Check(HudsonPlay_PointToHopTo(way, 0, 3000) == 3, "it lands on the first point that far along");
	Check(HudsonPlay_PointToHopTo(way, 1, 3000) == 4, "counted from the one he's going to");
	Check(HudsonPlay_PointToHopTo(way, 0, 2500) == 3, "and it's at least that far, not nearly");
	Check(HudsonPlay_PointToHopTo(way, 4, 3000) == 5, "a turn in the way counts, and so does a point exactly that far");
	Check(HudsonPlay_PointToHopTo(way, 2, 100000) == 5, "a way that's too short lands him on its last point");
	Check(HudsonPlay_PointToHopTo(way, 5, 3000) == 5, "and from the last point he stays on it");

	std::vector<Point> const oneOnly = { { 7, 8, 9 } };

	Check(HudsonPlay_PointToHopTo(oneOnly, 0, 3000) == 0, "a way of one point has only that to land on");

	// Up and down isn't how far along he's got
	std::vector<Point> const stairs = { { 0, 0, 0 }, { 0, 5000, 0 }, { 3000, 5000, 0 } };

	Check(HudsonPlay_PointToHopTo(stairs, 0, 3000) == 2, "going up or down doesn't count");

	Check(kHudsonPlayHopAtLeast == 3000, "a hop is at least 3 m, which is more than a door is thick");
}

static bool ReadWholeFile(std::string const &path, std::string *text)
{
	FILE *const file = fopen(path.c_str(), "rb");

	if (!file)
	{
		return false;
	}

	char buffer[4096];
	size_t got;

	text->clear();

	while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
	{
		text->append(buffer, got);
	}

	fclose(file);
	return true;
}

static void TestTheRealLists()
{
	printf("The real lists\n");

	static char const *const levels[] =
	{
		"derelict", "genshd1", "invasion", "odobenus", "sulaco", "hangar", "temple_m", "vaults_m", "nost03_m", "stat101_m", "fall_m",
		"fall", "area52", "vaults", "furyall", "caverns", "battle", "invasion_p", "escape_p", "temple_p", "breakout_p", "sulaco_p",
		"temple", "escape", "nost03", "stat101", "breakout", "invasion_a", "derelict_a", "sulaco_a", "caverns_a", "furyall_a"
	};

	int stepsInAll = 0;

	for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
	{
		std::string const path = std::string("tools/hudson/chains/") + levels[i] + ".play";
		std::string text;

		Check(ReadWholeFile(path, &text), (path + " is there").c_str());
		stepsInAll += (int)Read(text, (path + " is read").c_str()).size();
	}

	// So that losing a level's steps, or a whole level's, doesn't pass unnoticed. It changes when the routes do.
	Check(stepsInAll == 159, "159 steps in all");
}

int main()
{
	TestAGoodList();
	TestBadFiles();
	TestSteering();
	TestGettingStuck();
	TestWalkingHard();
	TestHowFarOff();
	TestFollowingTheWay();
	TestWalkControls();
	TestHopping();
	TestTheRealLists();

	if (checksFailed > 0)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
