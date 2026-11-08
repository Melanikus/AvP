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

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "alien_ways_stuck.h"

static int checksFailed = 0;

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

	Check(AlienWaysStuck_LetGoLine(6, -4600, 1900, 19500) == "alien ways: an alien at (-4.6, 1.9, 19.5) stopped being guided, in Avoidance",
		"an alien let go is said with where it is and the state it's in");
	Check(AlienWaysStuck_LetGoLinesStop() == "alien ways: 100 aliens let go in this level, so no more lines about that until the next one",
		"and the line after the most a level has says that's all");
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
	Check(Has(frame, "if (letGo && letGoLines < kAlienWaysMostLetGoLines)") && Has(frame, "if (letGoLines == kAlienWaysMostLetGoLines)"),
		"an alien let go is a line, up to the most a level has, and then one line says so");

	std::string const steer = Between(ways, "bool AlienWays_Steer(", "bool AlienWays_Following(");
	Check(ComesBefore(steer, "Watched *const w = WatchedFor(ask.who, true);", "if (!theGuide.Steer(map, ask, &towards))") && Has(steer, "\tw->goal = *goal;\n"),
		"an alien that asks for a way is watched, with the game's own goal for it");

	std::string const end = Between(ways, "void AlienWays_LevelEnd()", "\n}\n");
	Check(Has(end, "\t\tw = Watched();\n") && Has(end, "\tletGoLines = 0;\n"), "and the list is emptied at a level's end");
}

int main()
{
	TestTheStates();
	TestTheWords();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
