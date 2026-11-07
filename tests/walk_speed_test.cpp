// The test for 3dc/avp/walk_speeds.h: how walking and crouching slow the player down, with the TurnSpeedAdjust setting both
// ways. Written 2026-11-07 by Melanikus for the AvP port. Found through Bobblen's avp-forever (commit 2b70546).
//
// WHAT IT TESTS
//
// Walking and crouching always move at half speed. TurnSpeedAdjust ([Gameplay], game.cpp) only decides whether turning
// slows down too. Before, setting it to false also stopped walking from slowing anyone down, so nobody could walk. This runs
// the halving with the setting on and off. Then it reads pmove.cpp and game.cpp to check that the game really calls it that
// way, because the movement code needs a whole running game.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It reads two of the
// game's source files, from the folder it's run in (the top of the repository), and uses none of the game.

#include <stdio.h>
#include <string>

#include "walk_speeds.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

// A whole source file as text, or nothing if it can't be read
static std::string ReadSourceFile(const char *path)
{
	std::string text;
	FILE *file = fopen(path, "rb");

	if (!file)
	{
		return text;
	}

	char buffer[4096];
	size_t got;

	while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
	{
		text.append(buffer, got);
	}

	fclose(file);

	// The files are kept with Windows line ends; the checks below are written with plain ones
	std::string plain;

	for (char c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static size_t CountOf(const std::string &text, const char *what)
{
	size_t count = 0;

	for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static void TestTheHalving()
{
	int forward = 1000, strafe = 600, turn = 400;

	WalkSpeeds_Halve(forward, strafe, turn, true);
	Check(forward == 500 && strafe == 300 && turn == 200, "with TurnSpeedAdjust on, the default, walking halves moving and turning, as the game always has");

	forward = 1000;
	strafe = 600;
	turn = 400;
	WalkSpeeds_Halve(forward, strafe, turn, false);
	Check(forward == 500 && strafe == 300, "with it off, walking still halves moving forwards and to the side");
	Check(turn == 400, "and turning stays at full speed, which is all the setting is for");

	forward = -1000;
	strafe = -600;
	turn = -400;
	WalkSpeeds_Halve(forward, strafe, turn, true);
	Check(forward == -500 && strafe == -300 && turn == -200, "backwards, to the left and turning left are halved the same way");
}

static void TestTheGameUsesIt()
{
	std::string const pmove = ReadSourceFile("3dc/avp/pmove.cpp");
	std::string const game = ReadSourceFile("3dc/avp/game.cpp");

	Check(!pmove.empty() && !game.empty(), "pmove.cpp and game.cpp can be read");
	Check(CountOf(pmove, "WalkSpeeds_Halve(forwardSpeed, strafeSpeed, turnSpeed, bTurnSpeedAdjust);") == 1, "the player's movement halves its speeds this way, with the setting deciding the turning");
	Check(CountOf(pmove, "!bTurnSpeedAdjust") == 0, "and the setting no longer sends anyone down the full-speed path");
	// The old walking lines, which halved by themselves. (A predator going backwards at full speed still halves forwardSpeed on
	// its own, which is a different thing, so that line isn't looked for.)
	Check(CountOf(pmove, "strafeSpeed = (strafeSpeed)/2;") == 0 && CountOf(pmove, "turnSpeed = (turnSpeed)/2;") == 0, "and nothing else in it halves walking speeds by itself");
	Check(CountOf(game, "Config_GetBool(\"[Gameplay]\", \"TurnSpeedAdjust\", true)") == 1, "and the setting is still on unless a player turns it off, so the game plays as it always has");
}

int main()
{
	TestTheHalving();
	TestTheGameUsesIt();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
