// The test of what the log says about an alien the alien ways guide finds stuck, or lets go (3dc/avp/alien_ways_stuck.h). Written 2026-11-08
// by Melanikus for the AvP port (#109).
//
// WHAT IT TESTS
//
// - The header on its own: the states' names are bh_alien.h's, in its order; a state's history keeps the last 90 frames and counts them; the
//   angle between two downs; and every word of the stuck line's second part and of the let-go lines, including what's said when the alien
//   is gone, has no module, no goal, or a direction with no length.
// - By reading alien_ways.cpp: the stuck line gets the second part, from the alien's own blocks; each watched alien's state is noted every
//   frame, whatever state it's in, and one that's gone is taken off the list; a let-go line names the state, at most so many a level; the
//   game's own goal is kept when an alien asks; and the list is emptied at a level's end.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/alien_ways_stuck_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/alien_ways_stuck_test.json proves each check can fail.

#include <ctype.h>
#include <stdio.h>

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "alien_ways_stuck.h"

static int checksFailed = 0;

// An alien's name as the game keeps it, 8 bytes, and that name as the guide's number for it (AlienWaysWho::name is a memcpy of it, in
// alien_ways.cpp's WhoIs). Added 2026-11-11 by Melanikus for the AvP port (#109).
static unsigned char const kName[8] = { 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f, 0x60, 0x71 };

static uint64_t NameOf(const unsigned char bytes[8])
{
	uint64_t name = 0;
	memcpy(&name, bytes, sizeof(name));
	return name;
}

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const char *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();

	std::string out = text.str();
	out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
	return out;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(second, a);
	return a != std::string::npos && b != std::string::npos;
}

static std::string Between(const std::string &text, const char *start, const char *end)
{
	size_t const a = text.find(start);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(end, a + 1);
	return (a == std::string::npos || b == std::string::npos) ? std::string() : text.substr(a, b - a);
}

// The names in bh_alien.h's AlienBhState, in order, without their ABS_
static std::vector<std::string> GameStateNames()
{
	std::string const list = Between(ReadSource("3dc/avp/bh_alien.h"), "typedef enum AlienBhState", "}ALIEN_BHSTATE;");
	std::vector<std::string> names;

	for (size_t at = list.find("ABS_"); at != std::string::npos; at = list.find("ABS_", at + 4))
	{
		size_t end = at + 4;

		while (end < list.size() && (isalnum(static_cast<unsigned char>(list[end])) || list[end] == '_'))
		{
			end++;
		}

		names.push_back(list.substr(at + 4, end - at - 4));
	}

	return names;
}

static void TestTheStates()
{
	printf("The states and their history:\n");

	std::vector<std::string> const game = GameStateNames();
	bool same = !game.empty() && static_cast<int>(game.size()) == kAlienWaysStateKinds;

	for (int i = 0; same && i < kAlienWaysStateKinds; i++)
	{
		same = game[i] == kAlienWaysStateNames[i];
	}

	Check(same, "the states' names are bh_alien.h's, in its order (" + std::to_string(game.size()) + " there)");
	Check(std::string(AlienWaysStuck_StateName(6)) == "Avoidance", "state 6 is Avoidance");
	Check(std::string(AlienWaysStuck_StateName(-1)) == "an unknown state" && std::string(AlienWaysStuck_StateName(kAlienWaysStateKinds)) == "an unknown state",
		"a number that isn't a state is said to be unknown");

	AlienWaysStateHistory history;
	Check(AlienWaysStateHistory_Counts(history) == "none kept" && history.filled == 0, "a new history has nothing in it");

	for (int i = 0; i < 41; i++)
	{
		AlienWaysStateHistory_Note(&history, 1);
	}

	for (int i = 0; i < 49; i++)
	{
		AlienWaysStateHistory_Note(&history, 6);
	}

	Check(history.filled == 90 && AlienWaysStateHistory_Counts(history) == "Approach 41, Avoidance 49", "90 frames are counted by state, in the list's order");

	for (int i = 0; i < 10; i++)
	{
		AlienWaysStateHistory_Note(&history, 0);
	}

	Check(history.filled == 90 && AlienWaysStateHistory_Counts(history) == "Wait 10, Approach 31, Avoidance 49", "only the last 90 are kept: the oldest go first");

	AlienWaysStateHistory odd;
	AlienWaysStateHistory_Note(&odd, 99);
	AlienWaysStateHistory_Note(&odd, -1);
	Check(AlienWaysStateHistory_Counts(odd) == "unknown 2", "a state that isn't on the list is counted as unknown");
}

static void TestTheWords()
{
	printf("The words:\n");

	double const down[3] = { 0, 65536, 0 }, up[3] = { 0, -65536, 0 }, side[3] = { 65536, 0, 0 }, none[3] = { 0, 0, 0 };
	Check(AlienWaysStuck_DegreesApart(down, down) == 0 && AlienWaysStuck_DegreesApart(down, side) == 90 && AlienWaysStuck_DegreesApart(down, up) == 180,
		"two downs are 0, 90 and 180 degrees apart as they should be");
	Check(AlienWaysStuck_DegreesApart(down, none) == -1, "and one with no length can't be compared");

	AlienWaysStuckFacts facts;
	facts.found = true;
	facts.stateNow = 1;

	for (int i = 0; i < 41; i++)
	{
		AlienWaysStateHistory_Note(&facts.history, 1);
	}

	for (int i = 0; i < 49; i++)
	{
		AlienWaysStateHistory_Note(&facts.history, 6);
	}

	facts.gravity[1] = 65536;
	facts.bodyDown[1] = 65536;
	facts.onFloor = true;
	facts.obstructedFrames = 11;
	facts.module = "genent03";
	facts.hasGoal = true;
	facts.goal[0] = 1400;
	facts.goal[1] = 1900;
	facts.goal[2] = 27300;

	std::string const expected = ". It's in Approach now; over the last 90 frames: Approach 41, Avoidance 49. It's moving at 0.0 m/s, standard gravity off, touching the "
		"floor yes; its gravity (0.00, 1.00, 0.00) and its body's down (0.00, 1.00, 0.00), 0 degrees apart; obstructed 11 frames running; in module "
		"genent03; the game's own goal (1.4, 1.9, 27.3)";
	std::string const said = AlienWaysStuck_Details(facts);
	Check(said == expected, "a stuck alien's line says its state, its last frames, its speed, gravity, down, obstruction, module and goal");

	if (said != expected)
	{
		printf("        it said: %s\n", said.c_str());
	}

	AlienWaysStuckFacts other = facts;
	other.velocity[0] = 3000;
	other.velocity[2] = 4000;
	other.standardGravity = true;
	other.onFloor = false;
	other.bodyDown[1] = 0;
	other.bodyDown[0] = 65536;
	std::string const moving = AlienWaysStuck_Details(other);
	Check(Has(moving, "moving at 5.0 m/s, standard gravity on, touching the floor no;") && Has(moving, "(1.00, 0.00, 0.00), 90 degrees apart;"),
		"one moving, on standard gravity, off the floor and turned on its side says so");

	AlienWaysStuckFacts bare = facts;
	bare.module = "";
	bare.hasGoal = false;
	bare.gravity[1] = 0;
	std::string const plain = AlienWaysStuck_Details(bare);
	Check(Has(plain, "which can't be compared;") && Has(plain, "in module (none)") && !Has(plain, "goal"),
		"with no gravity, no module's name and no goal, it says so and leaves the goal out");

	AlienWaysStuckFacts gone;
	Check(AlienWaysStuck_Details(gone) == ". It's no longer one of the game's live aliens, so nothing more can be said", "an alien that's gone is said to be gone");

	// Modified 2026-11-11 by Melanikus for the AvP port (#109): the let-go line names the alien by its place and id
	Check(AlienWaysStuck_LetGoLine(1, NameOf(kName), 6, -4600, 1900, 19500) == "alien ways: alien 1 (id 0a1b2c3d4e5f6071) at (-4.6, 1.9, 19.5) stopped being guided, in Avoidance",
		"an alien let go is said by its place and id, with where it is and the state it's in");
	Check(AlienWaysStuck_LetGoLinesStop() == "alien ways: 100 aliens let go in this level, so no more lines about that until the next one",
		"and the line after the most a level has says that's all");
}

// THE IDS, AND THE LINES THAT FOLLOW ONE ALIEN (alien_ways_stuck.h). Added 2026-11-11 by Melanikus for the AvP port (#109).
static void TestTheIds()
{
	printf("An alien's id, and the reached and leaving lines:\n");

	Check(AlienWaysStuck_Id(NameOf(kName)) == "0a1b2c3d4e5f6071",
		"an id is the 8 bytes of the alien's name as 16 small hex digits, in the name's own order, as Hudson's blow line prints the alien that struck");
	unsigned char const allOn[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	Check(AlienWaysStuck_Id(0) == "0000000000000000" && AlienWaysStuck_Id(NameOf(allOn)) == "ffffffffffffffff",
		"a name of all 0s or all 1s is still 16 digits, two a byte, with none dropped or doubled");
	Check(AlienWaysStuck_Who(2, NameOf(kName)) == "alien 2 (id 0a1b2c3d4e5f6071)", "an alien is named by its place in the guide, then its id");

	Check(AlienWaysStuck_ReachedLine(0, NameOf(kName), 12400, 1.94) == "alien ways: alien 0 (id 0a1b2c3d4e5f6071) reached its target, 12.4 s after its first way, at 1.9 m",
		"an alien that reached its target says how long after its first way, and how far from its target");
	Check(AlienWaysStuck_ReachedLine(0, NameOf(kName), -1, 1.5) == "alien ways: alien 0 (id 0a1b2c3d4e5f6071) reached its target, before any way was found for it, at 1.5 m",
		"one that reached it before the guide found it any way says so, with no time");
	Check(AlienWaysStuck_ReachedLine(0, NameOf(kName), 0, -1.0) == "alien ways: alien 0 (id 0a1b2c3d4e5f6071) reached its target, 0.0 s after its first way, at an unknown distance",
		"and one whose target isn't known says the distance isn't known, not a number below 0");

	const char *const words[] = { "dead", "gone", "far", "target gone" };
	bool allSaid = true;

	for (int why = 0; why < kAlienWaysLeavingKinds; why++)
	{
		allSaid = allSaid && AlienWaysStuck_LeavesLine(1, NameOf(kName), why) == std::string("alien ways: alien 1 (id 0a1b2c3d4e5f6071) leaves the guide: ") + words[why];
	}

	Check(kAlienWaysLeavingKinds == 4 && allSaid, "an alien that left the guide says why in one of four ways: dead, gone, far or target gone");
	Check(AlienWaysStuck_LeavesLine(1, NameOf(kName), kAlienWaysLeavingKinds) == "alien ways: alien 1 (id 0a1b2c3d4e5f6071) leaves the guide: an unknown reason"
		&& AlienWaysStuck_LeavesLine(1, NameOf(kName), -1) == "alien ways: alien 1 (id 0a1b2c3d4e5f6071) leaves the guide: an unknown reason",
		"a reason that isn't one of them is said to be unknown, and nothing past the list is read");

	// The forms Primary's count reads (AvP-docs\tools\primary\stuck\stuck-count.js), so a change here that it can't read fails here first
	std::regex const reached("^alien ways: alien (\\d+) \\(id [0-9a-f]{16}\\) reached its target, ([0-9.]+) s after its first way, at ([0-9.]+) m$");
	std::regex const leaves("^alien ways: alien (\\d+) \\(id [0-9a-f]{16}\\) leaves the guide: (dead|gone|far|target gone)$");
	Check(std::regex_match(AlienWaysStuck_ReachedLine(2, NameOf(kName), 3000, 2.0), reached) && std::regex_match(AlienWaysStuck_LeavesLine(2, NameOf(kName), kAlienWaysLeavesFar), leaves),
		"the reached and leaving lines are in the forms agreed with Primary's count");
}

static void TestTheGameUsesIt()
{
	printf("Where the game uses it, by reading alien_ways.cpp:\n");

	std::string const ways = ReadSource("3dc/avp/alien_ways.cpp");
	Check(!ways.empty(), "alien_ways.cpp can be read");

	std::string const tell = Between(ways, "static void TellHowASearchWent(const AlienWaysReport &line)", "void AlienWays_LevelStart()");
	Check(ComesBefore(tell, "case kAlienWaysOutcomeStuck:", "\t\t\tdetails = StuckDetails(line.who);\n") && Has(tell, "LogString(std::string(text) + details);"),
		"the stuck line gets its second part, about that alien");

	std::string const details = Between(ways, "static std::string StuckDetails(const AlienWaysWho &who)", "// A point in metres, for the log");
	Check(Has(details, "STRATEGYBLOCK *const alien = LiveAlien(who);") && Has(details, "if (!alien)\n\t{\n\t\treturn AlienWaysStuck_Details(facts);"),
		"it's read only from a live alien, found by who it is");
	Check(Has(details, "facts.bodyDown[0] = dynamics->OrientMat.mat21;") && Has(details, "facts.bodyDown[1] = dynamics->OrientMat.mat22;")
		&& Has(details, "facts.bodyDown[2] = dynamics->OrientMat.mat23;"), "its body's down is its matrix's second row, as the guide's own down is");
	Check(Has(details, "facts.obstructedFrames = status ? status->moveData.numObstructiveCollisions : 0;")
		&& Has(details, "facts.module = alien->containingModule ? alien->containingModule->name : 0;"),
		"the obstruction count is the alien's own, and its module is the one it's in");

	std::string const stateOf = Between(ways, "static int StateOf(const STRATEGYBLOCK *alien)", "static bool SameWho(");
	Check(Has(stateOf, "return status ? static_cast<int>(status->BehaviourState) : -1;"), "a state is the alien's own BehaviourState");

	std::string const frame = Between(ways, "void AlienWays_Frame()", "bool AlienWays_Steer(");
	Check(ComesBefore(frame, "\t\tAlienWaysStateHistory_Note(&w.history, StateOf(alien));\n", "theGuide.Frame(ReadyMap()"),
		"every frame each watched alien's state is noted, whatever state it's in, before the guide's turn");
	Check(Has(frame, "\t\tif (!alien)\n\t\t{\n\t\t\tw = Watched();\n\t\t\tcontinue;"), "and one that's gone is taken off the list");
	// Modified 2026-11-11 by Melanikus for the AvP port (#109): every alien let go is a line, a dead or gone one too
	Check(Has(frame, "\t\tif (letGoLines < kAlienWaysMostLetGoLines)\n") && Has(frame, "if (letGoLines == kAlienWaysMostLetGoLines)"),
		"an alien let go is a line, up to the most a level has, and then one line says so");

	std::string const steer = Between(ways, "bool AlienWays_Steer(", "bool AlienWays_Following(");
	Check(ComesBefore(steer, "Watched *const w = WatchedFor(ask.who, true);", "if (theGuide.Steer(map, ask, &towards))") && Has(steer, "\tw->goal = *goal;\n"),
		"an alien that asks for a way is watched, with the game's own goal for it");

	// The cut stops just before the function's closing "\n}", so its last line has no line end in it: the lines looked for here don't end in one,
	// and the check holds wherever in the function they are
	std::string const end = Between(ways, "void AlienWays_LevelEnd()", "\n}\n");
	Check(Has(end, "\t\tw = Watched();") && Has(end, "\tletGoLines = 0;"), "and the list is emptied at a level's end");

	// THE IDS IN THE GAME'S LINES (added 2026-11-11 by Melanikus for the AvP port, #109), in the same cut of TellHowASearchWent as above
	size_t named = 0;

	for (size_t at = tell.find("alien.c_str(), "); at != std::string::npos; at = tell.find("alien.c_str(), ", at + 1))
	{
		named++;
	}

	Check(Has(tell, "std::string const alien = AlienWaysStuck_Who(line.follower, line.who.name);") && named == 8 && !Has(tell, "\"alien ways: alien %u"),
		"each of a search's eight lines names its alien by its place and its id, and none by its place alone");
	Check(Has(tell, "if (line.outcome == kAlienWaysOutcomeFound || line.outcome == kAlienWaysOutcomeAllFloor)")
		&& Has(tell, "if (s && s->firstWayMs < 0)\n\t\t{\n\t\t\ts->firstWayMs = clockMs;"),
		"an alien's first way found starts the clock its reached line gives, and a later way doesn't restart it");
	Check(Has(frame, "theGuide.WhoAt(slots[i], &who);") && Has(frame, "AlienWaysStuck_Who(slots[i], who.name) + \" at \""),
		"the once-a-second places name each alien by its place and its id");
	std::string const wouldHave = Between(ways, "void AlienWays_WouldHaveAvoided(", "bool AlienWays_Following(");
	Check(Has(wouldHave, "AlienWaysStuck_Who(SlotOf(who), who.name).c_str(), Metres(at).c_str(), module,"),
		"a line about a test that would have sent an alien off names it by its place and its id too");
	Check(ComesBefore(steer, "Seen *const seenNow = SeenFor(ask.who.name, true);", "if (theGuide.Steer(map, ask, &towards))")
		&& Has(steer, "if (seenNow && ask.hasTarget)\n\t{\n\t\tseenNow->hasTarget = true;\n\t\tseenNow->target = ask.target;"),
		"an alien that asks for a way is on the seen list, with the target it's coming for, kept when an ask has none");
	Check(ComesBefore(frame, "\t\t\ts->slot = slot;\n", "theGuide.Frame(ReadyMap()"),
		"each alien's place in the guide is noted every frame before the guide's turn, so one let go can still be named by it");

	// REACHED: the first frame in Attack, once an alien a level, before the guide's turn lets it go for Attack
	Check(ComesBefore(frame, "alien->I_SBtype != I_BehaviourAlien || !alien->DynPtr || StateOf(alien) != ABS_Attack)", "theGuide.Frame(ReadyMap()")
		&& Has(frame, "for (int i = 0; i < NumActiveStBlocks && reachedLines < kAlienWaysMostReachedLines; i++)"),
		"every live alien in its own Attack is looked at, up to the most reached lines a level has, before the guide's turn");
	Check(Has(frame, "if (!s || s->reached)\n\t\t{\n\t\t\tcontinue;") && ComesBefore(frame, "s->reached = true;", "LogString(AlienWaysStuck_ReachedLine(SlotOf(who), who.name, "
		"s->firstWayMs < 0 ? -1 : clockMs - s->firstWayMs, MetresToItsTarget(alien, *s)));"),
		"only an alien that has asked for a way gets a reached line, once, with the time since its first way and its distance to its target");

	// LEAVES THE GUIDE: why, in alien_ways_stuck.h's order, and the let-go line for a state of its own otherwise
	std::string const why = Between(ways, "static int WhyItLeaves(", "static double MetresToItsTarget(");
	size_t const dead = why.find("return BlockOf(who) ? kAlienWaysLeavesDead : kAlienWaysLeavesGone;");
	size_t const dying = why.find("if (StateOf(alien) == ABS_Dying || alien->SBDamageBlock.Health <= 0)");
	size_t const far = why.find("if (!alien->SBdptr)");
	size_t const target = why.find("if (!target || target->SBDamageBlock.Health <= 0)");
	Check(dead != std::string::npos && dying != std::string::npos && far != std::string::npos && target != std::string::npos && dead < dying && dying < far && far < target
		&& Has(why, "return -1;"), "an alien let go is dead, gone, far or without its target, asked in that order, or else left for a state of its own");
	Check(Has(frame, "int const why = WhyItLeaves(lapsed[i], letGo);") && Has(frame, "LogString(why >= 0 || !letGo ? AlienWaysStuck_LeavesLine(slot, lapsed[i].name, why)"),
		"so the line for an alien let go says why it left, or the state it left for");

	// Nothing of the seen list carries into the next level, or past a restart, where a remade alien can have an old one's name
	std::string const restarts = Between(ways, "void AlienWays_LevelRestarts()", "\n}\n");
	Check(Has(end, "\tstd::vector<Seen>().swap(seen);") && Has(end, "\treachedLines = 0;") && Has(restarts, "\tstd::vector<Seen>().swap(seen);"),
		"the seen list is emptied at a level's end and when it restarts");
}

int main()
{
	TestTheStates();
	TestTheWords();
	TestTheIds();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
