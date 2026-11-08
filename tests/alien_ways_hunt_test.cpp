// The test for the alien ways in two more of an alien's states: hunting, and wandering from module to module (issue #109). Written
// 2026-11-08 by Melanikus for the AvP port.
//
// An alien coming for its target already follows its way across the walls and ceilings (AlienNearState_Approach, bh_near.cpp). Hunting and
// wandering do too now: each asks the alien ways code for a way to the point the game's own code picked, and goes along it when there is
// one, and a wandering alien crawls when it should, as a hunting one does. That can only happen in a real game, with a level and aliens in it, so this test reads bh_near.cpp as text and checks that the lines
// are there as they should be. Every check has a row in tests/takeouts/alien_ways_hunt_test.json that breaks it, to show the check notices.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game.

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static std::string ReadSourceFile(char const *name)
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

	// Files here may have Windows' line ends, and what's looked for is written with plain ones
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

// The text from the start of from up to to, or nothing if either isn't there
static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

// The text after the first time what is in it, or nothing if it isn't there
static std::string After(const std::string &text, const char *what)
{
	std::string::size_type const at = text.find(what);

	return at == std::string::npos ? std::string() : text.substr(at);
}

// The text with no spaces or tabs in it, so a line is found however it's spaced, such as "UseStandardGravity = 1" as well as "UseStandardGravity=1"
static std::string WithoutSpaces(const std::string &text)
{
	std::string out;

	for (char const c : text)
	{
		if (c != ' ' && c != '\t')
		{
			out += c;
		}
	}

	return out;
}

// The way's branch and the old steering, the same in both states: on a way, the alien lets go of gravity and keeps the way's direction;
// without one, the old code picks the direction. Either way that one direction is the velocity, and the checks after it see it.
static const char kTheTwoBranches[] =
	"&velocityDirection))\n"
	"\t{\n"
	"\t\t/* Stick to whatever it's touching, and go along it towards the way's next point */\n"
	"\t\tdynPtr->UseStandardGravity=0;\n"
	"\t}\n"
	"\telse\n"
	"\t{\n"
	"\t\tNPCGetMovementDirection(sbPtr, &velocityDirection, &(alienStatusPointer->wanderData.worldPosition),&alienStatusPointer->waypointManager);\n"
	"\t}\n"
	"\tNPCSetVelocity(sbPtr, &velocityDirection, alienStatusPointer->MaxSpeed);\n";

static void TestHunting(const std::string &source)
{
	std::string const hunt = Between(source, "static void AlienNearState_Hunt(STRATEGYBLOCK *sbPtr)\n{", "\n}\n");
	Check(!hunt.empty(), "SOURCE: the hunting state is there to read");

	// No target, though a hunting alien may have one: given one it can see, the alien ways code heads for the target itself instead of the
	// point, and going straight for a target is Approach's job. So the way always goes where Hunt was going.
	char const *const call = "AlienWays_Steer(sbPtr, 0, &(alienStatusPointer->wanderData.worldPosition), &velocityDirection))";
	Check(Has(hunt, call), "SOURCE: a hunting alien asks for a way to the point the game picked, the way into the next module towards where it hunts, with no target to head for instead");

	Check(Has(hunt, "if ((alienStatusPointer->Type == AT_Standard) && AlienIsAbleToClimb(sbPtr)\n"
		"\t\t&& (VectorDistance(&(dynPtr->Position), &(alienStatusPointer->wanderData.worldPosition)) > ALIEN_ATTACKDISTANCE_MIN)\n"
		"\t\t&& AlienWays_Steer(sbPtr, 0"),
		"SOURCE: only a standard alien that can climb asks, and only while it's further from the point than it attacks from");

	Check(Has(hunt, (std::string("AlienWays_Steer(sbPtr, 0, &(alienStatusPointer->wanderData.worldPosition), ") + kTheTwoBranches).c_str()),
		"SOURCE: on a way it lets go of gravity and goes the way's direction; without one it's steered as it always was");

	// The state puts gravity back on at its start, every frame. That's before the call, so the way's choice is the one that counts.
	Check(!Has(WithoutSpaces(After(hunt, call)), "UseStandardGravity=1"), "SOURCE: nothing after the call puts gravity back on in the same frame, however it's written");
}

static void TestWandering(const std::string &source)
{
	std::string const wander = Between(source, "static void AlienNearState_Wander(STRATEGYBLOCK *sbPtr)\n{", "\n}\n");
	Check(!wander.empty(), "SOURCE: the wandering state is there to read");

	char const *const call = "AlienWays_Steer(sbPtr, 0, &(alienStatusPointer->wanderData.worldPosition), &velocityDirection))";
	Check(Has(wander, call), "SOURCE: a wandering alien asks for a way to its wander point, with no target, since it has let its target go");

	Check(Has(wander, "if ((alienStatusPointer->Type == AT_Standard) && AlienIsAbleToClimb(sbPtr)\n"
		"\t\t&& (alienStatusPointer->wanderData.currentModule != NPC_NOWANDERMODULE)\n"
		"\t\t&& (VectorDistance(&(dynPtr->Position), &(alienStatusPointer->wanderData.worldPosition)) > ALIEN_ATTACKDISTANCE_MIN)\n"
		"\t\t&& AlienWays_Steer(sbPtr, 0"),
		"SOURCE: only a standard alien that can climb asks, only when it has a wander point, and only while it's further from it than it attacks from");

	Check(Has(wander, (std::string("AlienWays_Steer(sbPtr, 0, &(alienStatusPointer->wanderData.worldPosition), ") + kTheTwoBranches).c_str()),
		"SOURCE: on a way it lets go of gravity and goes the way's direction; without one it's steered as it always was");

	Check(!Has(WithoutSpaces(After(wander, call)), "UseStandardGravity=1"), "SOURCE: nothing after the call puts gravity back on in the same frame, however it's written");

	// It crawls when AlienShouldBeCrawling says so, as a hunting alien does, and that says so for an alien on its way. Before, wandering
	// never changed whether the alien was crouched, so a wandering alien on a way would have climbed the walls upright. Only with the alien
	// ways switched on: AlienShouldBeCrawling's older reasons would otherwise change the 1999 game for everyone.
	Check(Has(wander, "\tif (AlienWaysSwitchedOn)\n\t{\n\t\tint redo=0;"), "SOURCE: a wandering alien's crouching changes only with the alien ways switched on");
	Check(Has(wander, "if(AlienShouldBeCrawling(sbPtr))\n\t\t\t{\n\t\t\t\talienStatusPointer->IAmCrouched = 1;\n\t\t\t\tredo=1;"),
		"SOURCE: a standing alien crawls when it should, as one on its way does");
	Check(Has(wander, "if(!AlienShouldBeCrawling(sbPtr))\n\t\t\t{\n\t\t\t\talienStatusPointer->IAmCrouched = 0;\n\t\t\t\tredo=1;"),
		"SOURCE: and a crawling one stands up again when it shouldn't any more");
	Check(Has(wander, "\t\tif (redo) {\n\t\t\tStartAlienMovementSequence(sbPtr);\n\t\t}\n\t}\n\n\t/* Update sequence! */\n\tAlienHandleMovingAnimation(sbPtr);"),
		"SOURCE: and its pose is changed to match, before the animation is moved on");
}

int main()
{
	std::string const source = ReadSourceFile("3dc/avp/bh_near.cpp");
	Check(!source.empty(), "the game's file is there to read");

	puts("1. hunting");
	TestHunting(source);
	puts("2. wandering");
	TestWandering(source);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
