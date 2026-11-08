// The test of the proximity wake for NPCs placed asleep (3dc/avp/near_wake.h). Written 2026-11-08 by Melanikus for the AvP port (#130).
//
// WHAT IT TESTS
//
// - The header on its own: the user's numbers, how far a point is from a box, how near a player has to come (the box's own distance, but
//   no more than 4 m and no less than 1.5 m), the list of one box's sleepers and its size, which sleeper a player is near, and every word
//   of the log's lines.
// - By reading the game's files: only a machine that runs the NPCs looks; a sleeper is one of the four kinds, still asleep, and not gone;
//   one switch between a box and its NPC at most; a walk-in box is set off by a player in it or near one of its sleepers, for both kinds
//   of switch; the line is written only when the box really is set off, and at most so many a level; and the count starts again when a
//   level's blocks are all destroyed.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/near_wake_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/near_wake_test.json proves each check can fail.

#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "near_wake.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, with its line ends written \n whatever the checkout made them. /FC in this test's settings makes __FILE__ this
// file's whole path, so the repository's top is found from it.
static std::string ReadSourceFile(char const *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static bool ComesBefore(std::string const &text, char const *first, char const *second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = a == std::string::npos ? std::string::npos : text.find(second, a);

	return a != std::string::npos && b != std::string::npos;
}

// The text from one mark up to the next one after it, or nothing if either isn't there, so a check inside it fails instead of passing on nothing
static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return end == std::string::npos ? "" : text.substr(start, end - start);
}

static bool Near(double a, double b)
{
	return fabs(a - b) < 0.001;
}

// A box 2 m on each side, from (0, 0, 0) to (2000, 2000, 2000)
static NearWakeBox TheBox()
{
	NearWakeBox box;

	box.least = NearWake_Point(0, 0, 0);
	box.most = NearWake_Point(2000, 2000, 2000);
	return box;
}

static void TestTheNumbers()
{
	puts("The user's numbers (2026-11-08):");

	Check(kNearWakeMostMillimetres == 4000.0, "a player never has to come nearer than 4 m");
	Check(kNearWakeLeastMillimetres == 1500.0, "and always has to come within 1.5 m");
	Check(kNearWakeMostSwitchesBetween == 1, "one switch between a box and an NPC it wakes, at most, as in every stock level");
	Check(kNearWakeMostSleepers == 32 && kNearWakeMostLines == 100, "32 sleepers looked at for one box, and 100 lines a level");
}

static void TestTheDistances()
{
	puts("How far from a box, and how near a player has to come:");

	NearWakeBox const box = TheBox();

	Check(NearWake_DistanceToBox(NearWake_Point(1000, 1000, 1000), box) == 0, "a point inside the box is no distance from it");
	Check(NearWake_DistanceToBox(NearWake_Point(2000, 0, 1000), box) == 0, "and so is one on its edge");
	Check(Near(NearWake_DistanceToBox(NearWake_Point(5000, 1000, 1000), box), 3000), "one off one side is as far as it is past that side");
	Check(Near(NearWake_DistanceToBox(NearWake_Point(-300, 1000, 1000), box), 300), "on either side");
	Check(Near(NearWake_DistanceToBox(NearWake_Point(5000, 6000, 1000), box), 5000), "and one off a corner is as far as the corner (3 m and 4 m make 5 m)");

	NearWakeBox backwards;
	backwards.least = NearWake_Point(2000, 0, 0);
	backwards.most = NearWake_Point(0, 2000, 2000);
	double const odd = NearWake_DistanceToBox(NearWake_Point(1000, 1000, 1000), backwards);
	Check(odd == 1000.0, "a box a level gives backwards still has a distance, the larger of the two ends' (not a crash, not a nonsense number)");

	Check(NearWake_Reach(0) == 1500.0, "an NPC inside its box is woken within 1.5 m");
	Check(NearWake_Reach(1400) == 1500.0, "and so is one whose box is nearer than that");
	Check(NearWake_Reach(2600) == 2600.0, "one whose box is 2.6 m away is woken within 2.6 m, as near as the box");
	Check(NearWake_Reach(4500) == 4000.0 && NearWake_Reach(107000) == 4000.0, "and one whose box is farther than 4 m, within 4 m");
}

static void TestTheSleepers()
{
	puts("One box's sleepers:");

	NearWakeBox const box = TheBox();
	NearWakeSleepers sleepers;

	Check(sleepers.count == 0 && !sleepers.full, "a new list is empty");
	Check(NearWakeSleepers_Add(&sleepers, NearWake_Point(9500, 1000, 1000), kNearWakeFacehugger, box), "a sleeper goes in");
	Check(Near(sleepers.list[0].boxDistance, 7500) && sleepers.list[0].reach == 4000.0 && sleepers.list[0].kind == kNearWakeFacehugger,
		"with how far its box is and how near a player has to come");

	NearWakeSleepers_Add(&sleepers, NearWake_Point(1000, 1000, 1000), kNearWakeAlien, box);
	double howFar = -1;

	// The facehugger is at a height of 1 m, so a player at 1 m plus a distance is that far from it
	Check(NearWakeSleepers_NearOne(sleepers, NearWake_Point(9500, 1000, 6000), &howFar) == -1, "a player 5 m from the facehugger is too far");
	Check(NearWakeSleepers_NearOne(sleepers, NearWake_Point(9500, 1000, 5001), &howFar) == -1, "and so is one just over 4 m from it");
	Check(NearWakeSleepers_NearOne(sleepers, NearWake_Point(9500, 1000, 5000), &howFar) == -1, "and one exactly 4 m from it: nearer than means nearer");
	Check(NearWakeSleepers_NearOne(sleepers, NearWake_Point(9500, 1000, 3900), &howFar) == 0 && Near(howFar, 2900), "one 2.9 m from it is near, and says how near");
	Check(NearWakeSleepers_NearOne(sleepers, NearWake_Point(1000, 1000, 2400), &howFar) == 1 && Near(howFar, 1400), "the alien inside the box is woken within 1.5 m");
	Check(NearWakeSleepers_NearOne(sleepers, NearWake_Point(1000, 1000, 2600), &howFar) == -1, "and not from 1.6 m");

	NearWakeSleepers_Clear(&sleepers);
	Check(sleepers.count == 0 && !sleepers.full, "the list can be emptied for the next box");

	int added = 0;

	for (int i = 0; i < 40; i++)
	{
		added += NearWakeSleepers_Add(&sleepers, NearWake_Point(i * 100.0, 0, 0), kNearWakeAlien, box) ? 1 : 0;
	}

	Check(added == 32 && sleepers.count == 32 && sleepers.full, "it holds 32, says the rest didn't fit, and keeps no more");

	NearWakeSleepers_Clear(&sleepers);
	Check(sleepers.count == 0 && !sleepers.full, "and emptied for the next box, it's no longer full");
}

static void TestTheWords()
{
	puts("The words:");

	NearWakeSleeper sleeper;
	sleeper.where = NearWake_Point(-4600, 1900, 19500);
	sleeper.kind = kNearWakeFacehugger;
	sleeper.boxDistance = 7500;
	sleeper.reach = 4000;

	std::string const said = NearWake_Line(sleeper, 3200);
	Check(said == "near wake: a player came within 3.2 m of a sleeping facehugger at (-4.6, 1.9, 19.5), whose walk-in box is 7.5 m from it, so the box "
		"was set off as if walked into (it's set off within 4.0 m)", "a box set off this way is said with the NPC, the distances and the reach");

	if (said.find("facehugger") == std::string::npos)
	{
		printf("        it said: %s\n", said.c_str());
	}

	Check(std::string(NearWake_KindName(kNearWakeAlien)) == "alien" && std::string(NearWake_KindName(kNearWakePredator)) == "predator"
		&& std::string(NearWake_KindName(kNearWakeSentryGun)) == "sentry gun", "each kind has its name");
	Check(std::string(NearWake_KindName(-1)) == "NPC" && std::string(NearWake_KindName(kNearWakeKinds)) == "NPC", "and a number that isn't a kind is an NPC");
	Check(NearWake_LinesStop() == "near wake: 100 walk-in boxes set off this way in this level, so no more lines about that until the next one",
		"the line after the most a level has");
	Check(NearWake_FullLine() == "near wake: a walk-in box wakes more than 32 sleeping NPCs, so only the first 32 are looked at", "and the one for a box that wakes too many");
}

static void TestTheGameUsesIt()
{
	puts("Where the game uses it, by reading its files:");

	std::string const wake = ReadSourceFile("3dc/avp/near_wake.cpp");
	Check(Has(Between(wake, "bool NearWake_ThisMachineRunsNpcs(void)", "\n}\n"), "return AvP.Network == I_No_Network || AvP.NetworkAIServer;"),
		"only single player, or a network game's host when it runs the NPCs, looks for sleepers");

	std::string const sleeper = Between(wake, "static bool SleeperFrom(", "\nstatic void AddSleepersFrom(");
	Check(ComesBefore(sleeper, "if (!target || target->SBflags.destroyed_but_preserved || !target->SBdataptr)\n\t{\n\t\treturn false;", "switch (target->I_SBtype)"),
		"something gone isn't a sleeper, and its freed data isn't read");
	Check(Has(sleeper, "if (alien->BehaviourState != ABS_Dormant || !target->DynPtr)"), "an alien is a sleeper only while it's dormant");
	Check(Has(sleeper, "if (hugger->nearBehaviourState != FHNS_Floating || !target->DynPtr)"), "a facehugger only while it floats asleep");
	Check(Has(sleeper, "case I_BehaviourDormantPredator:") && Has(sleeper, "*where = PointOf(predator->toolsData.position);"),
		"a dormant predator always, at the place the level gives it, since it has no body yet");
	Check(Has(sleeper, "if (gun->behaviourState != I_inactive || !target->DynPtr)"), "a sentry gun only while it's switched off");
	Check(!Has(sleeper, "I_BehaviourXenoborg") && !Has(sleeper, "I_BehaviourQueenAlien") && !Has(sleeper, "I_BehaviourMarine") && Has(sleeper, "default:\n\t\t\treturn false;"),
		"and nothing else: a xenoborg wakes by itself, and the queen and marines aren't placed asleep");

	std::string const add = Between(wake, "static void AddSleepersFrom(", "\nvoid NearWake_FindSleepers(");
	Check(ComesBefore(add, "if (SleeperFrom(target, &where, &kind))\n\t{\n\t\tNearWakeSleepers_Add(sleepers, where, kind, box);\n\t\treturn;",
		"switchesBetween >= kNearWakeMostSwitchesBetween)\n\t{\n\t\treturn;"), "a sleeper a box wakes goes on its list");
	Check(Has(add, "AddSleepersFrom(between->bs_targets[i], box, sleepers, switchesBetween + 1);") && Has(add, "AddSleepersFrom(between->ls_targets[i].sbptr, box, sleepers, switchesBetween + 1);"),
		"and so do the ones a switch it sets off wakes, through either kind of switch, one switch more each time");

	std::string const find = Between(wake, "void NearWake_FindSleepers(", "\nvoid NearWake_SayBoxSetOff(");
	Check(ComesBefore(find, "NearWakeSleepers_Clear(sleepers);", "AddSleepersFrom(own->bs_targets[i], box, sleepers, 0);")
		&& Has(find, "AddSleepersFrom(own->ls_targets[i].sbptr, box, sleepers, 0);"), "a box's own targets are looked at first, from an empty list");
	Check(Has(find, "if (sleepers->full && !saidABoxWakesTooMany)\n\t{\n\t\tsaidABoxWakesTooMany = true;\n\t\tLogString(NearWake_FullLine());"), "a box that wakes too many is said once a level");

	std::string const say = Between(wake, "void NearWake_SayBoxSetOff(", "\nvoid NearWake_LevelEnds(");
	Check(ComesBefore(say, "if (linesThisLevel >= kNearWakeMostLines)\n\t{\n\t\treturn;", "LogString(NearWake_Line(sleeper, playerDistance));")
		&& Has(say, "if (++linesThisLevel == kNearWakeMostLines)\n\t{\n\t\tLogString(NearWake_LinesStop());"), "a level gets so many lines, then one that says that's all");
	Check(Has(Between(wake, "void NearWake_LevelEnds(void)", "\n}\n"), "linesThisLevel = 0;") && Has(Between(wake, "void NearWake_LevelEnds(void)", "\n}\n"), "saidABoxWakesTooMany = false;"),
		"and the next level starts again");
	Check(Has(Between(ReadSourceFile("3dc/avp/strategy_def.cpp"), "void DestroyAllStrategyBlocks(void)", "void AssignNewSBName("), "NearWake_LevelEnds();"),
		"whenever every block is destroyed, at a level's end and at a restart");

	char const *const files[] = { "3dc/avp/bh_binary_switch.cpp", "3dc/avp/bh_link_switch.cpp" };
	char const *const behaviours[] = { "void BinarySwitchBehaveFun(STRATEGYBLOCK* sbptr)", "void LinkSwitchBehaveFun(STRATEGYBLOCK* sbptr)" };
	char const *const flags[] = { "if(bs_bhv->switch_flags & SwitchFlag_UseTriggerVolume)", "if(ls_bhv->switch_flags & SwitchFlag_UseTriggerVolume)" };
	char const *const finds[] = { "NearWake_FindSleepers(sbptr, &bs_bhv->trigger_volume_min, &bs_bhv->trigger_volume_max, &sleepers);",
		"NearWake_FindSleepers(sbptr, &ls_bhv->trigger_volume_min, &ls_bhv->trigger_volume_max, &sleepers);" };
	char const *const marks[] = { "bs_bhv->triggered_last = TRUE;\n\n\t\t\t// Only when it really is set off, not on each frame the player stays near\n\t\t\tif (setOffByNearness)\n\t\t\t{\n\t\t\t\tNearWake_SayBoxSetOff(nearSleeper, nearDistance);",
		"ls_bhv->triggered_last = TRUE;\n\n\t\t\t// Only when it really is set off, not on each frame the player stays near\n\t\t\tif (setOffByNearness)\n\t\t\t{\n\t\t\t\tNearWake_SayBoxSetOff(nearSleeper, nearDistance);" };
	char const *const names[] = { "a switch", "a link switch" };

	for (int i = 0; i < 2; i++)
	{
		std::string const behaviour = Between(ReadSourceFile(files[i]), behaviours[i], "\n}\n");
		std::string const name = names[i];

		Check(ComesBefore(behaviour, flags[i], "if (NearWake_ThisMachineRunsNpcs())\n\t\t{\n\t\t\tNearWake_FindSleepers(") && Has(behaviour, finds[i]),
			name + ": a walk-in box finds its sleepers, on a machine that runs the NPCs");
		Check(Has(behaviour, "int const sleeperNearBy = inTheBox ? -1 : NearWakeSleepers_NearOne(sleepers, NearWake_Point(at.vx, at.vy, at.vz), &nearDistance);")
			&& Has(behaviour, "if (inTheBox || sleeperNearBy >= 0)"), name + ": and is set off by a player in it, or near one of them");
		Check(ComesBefore(behaviour, "if (!needToTest)\n\t\t\t{\n\t\t\t\tcontinue;", "bool const inTheBox ="), name + ": the same players count for both, this machine's and the others'");
		Check(Has(behaviour, marks[i]), name + ": the line is written only when the box really is set off");
	}
}

int main()
{
	TestTheNumbers();
	TestTheDistances();
	TestTheSleepers();
	TestTheWords();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
