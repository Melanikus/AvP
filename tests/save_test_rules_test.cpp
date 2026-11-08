// The test for 3dc/avp/save_test_rules.h: when the save test harness may run, and which file it may read (SAVE-HOSTILE-1, step 2).
// Written 2026-11-08 by Melanikus for the AvP port.
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It runs none of the game: each check
// hands the rules a made-up command line and says what the harness should do with it.

#include <stdio.h>
#include <string.h>

#include <string>

#include "save_test_rules.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A load of a save called name, with -dev, in single player
static SaveTestAsk Load(const char *name)
{
	SaveTestAsk ask;
	memset(&ask, 0, sizeof(ask));
	ask.dev = true;
	ask.load = true;
	ask.name = name;
	ask.nameRoom = name ? strlen(name) + 1 : 0;
	// Made-up counts, in the game's order: marines, predators, aliens
	ask.episodesOf[0] = 11;
	ask.episodesOf[1] = 9;
	ask.episodesOf[2] = 10;
	return ask;
}

// Making a save of a species' episode, with -dev, in single player
static SaveTestAsk Make(int species, int episode)
{
	SaveTestAsk ask = Load(nullptr);
	ask.load = false;
	ask.make = true;
	ask.species = species;
	ask.episode = episode;
	return ask;
}

static SaveTestAnswer Decide(SaveTestAsk const &ask, std::string *path)
{
	char room[64];
	SaveTestAnswer const answer = SaveTest_Decide(ask, room, sizeof(room));
	*path = room;
	return answer;
}

static void Loading()
{
	printf("Loading a test save\n");
	std::string path;

	Check(Decide(Load("block0003_type2_size_minus1"), &path) == kSaveTestLoad && path == "SaveTest\\block0003_type2_size_minus1.sav",
		"a name of letters, digits and _ is read from the SaveTest folder, with .sav on the end");
	Check(Decide(Load("seed marine 0 (first)"), &path) == kSaveTestLoad && path == "SaveTest\\seed marine 0 (first).sav", "spaces, - and brackets are fine too");

	SaveTestAsk noDev = Load("a");
	noDev.dev = false;
	Check(Decide(noDev, &path) == kSaveTestNeedsDev && path.empty(), "without -dev it doesn't run, and gives no path");
	SaveTestAsk net = Load("a");
	net.networkGame = true;
	Check(Decide(net, &path) == kSaveTestNetworkGame && path.empty(), "in a network game it doesn't run");

	Check(Decide(Load(nullptr), &path) == kSaveTestNoName, "-savetest with no name after it doesn't run");
	Check(Decide(Load(""), &path) == kSaveTestNoName, "nor with an empty name");

	// Names that would reach outside the folder, or open a device, or aren't names at all
	char const *const bad[] = { "..\\..\\Saves\\player", "../x", "C:x", "a\\b", "a/b", "save.sav", "..", "CON", "nul", "COM1", " lead", "trail ",
		"0123456789012345678901234567890123456789", "tab\there" };

	for (size_t k = 0; k < sizeof(bad) / sizeof(bad[0]); k++)
	{
		Check(Decide(Load(bad[k]), &path) == kSaveTestBadName && path.empty(), std::string("a name like \"") + bad[k] + "\" is refused");
	}

	// A name that doesn't end within the room it's given
	char const unended[4] = { 'a', 'b', 'c', 'd' };
	SaveTestAsk run = Load("x");
	run.name = unended;
	run.nameRoom = sizeof(unended);
	Check(Decide(run, &path) == kSaveTestBadName, "a name with no end within its room is refused, and nothing past the room is read");

	// The longest name the rule allows fits the path
	std::string const longest(kCustomMapNameMost, 'a');
	Check(Decide(Load(longest.c_str()), &path) == kSaveTestLoad && path == "SaveTest\\" + longest + ".sav", "the longest name the rule allows fits");

	// Not room for the path
	char small[12];
	Check(SaveTest_Decide(Load("abcdefgh"), small, sizeof(small)) == kSaveTestPathTooLong && small[0] == '\0', "a path that doesn't fit its room is refused, and left empty");
	Check(SaveTest_Decide(Load("a"), nullptr, 0) == kSaveTestPathTooLong, "and with no room at all");
}

static void Making()
{
	printf("Making a save to break\n");
	std::string path;

	Check(Decide(Make(0, 0), &path) == kSaveTestMake, "a marine's first level is made");
	Check(Decide(Make(2, 9), &path) == kSaveTestMake, "and an alien's last, its 10th (species 2 is the alien, as the game numbers them)");
	Check(Decide(Make(2, 10), &path) == kSaveTestBadEpisode, "but not one past its last");
	Check(Decide(Make(1, 8), &path) == kSaveTestMake && Decide(Make(1, 9), &path) == kSaveTestBadEpisode, "a predator's 9 levels: the 9th made, a 10th refused");
	Check(Decide(Make(1, -1), &path) == kSaveTestBadEpisode, "nor a level before the first");
	Check(Decide(Make(3, 0), &path) == kSaveTestBadSpecies && Decide(Make(-1, 0), &path) == kSaveTestBadSpecies, "a species that isn't one of the three is refused");

	SaveTestAsk both = Make(0, 0);
	both.load = true;
	both.name = "a";
	both.nameRoom = 2;
	Check(Decide(both, &path) == kSaveTestBoth, "-savetest and -savetest-make together are refused");
	SaveTestAsk noDev = Make(0, 0);
	noDev.dev = false;
	Check(Decide(noDev, &path) == kSaveTestNeedsDev, "making one needs -dev too");
	SaveTestAsk net = Make(0, 0);
	net.networkGame = true;
	Check(Decide(net, &path) == kSaveTestNetworkGame, "and not in a network game");
}

static void Reasons()
{
	printf("Reasons\n");
	SaveTestAnswer const refusals[] = { kSaveTestNeedsDev, kSaveTestNetworkGame, kSaveTestNoName, kSaveTestBadName, kSaveTestBadSpecies,
		kSaveTestBadEpisode, kSaveTestBoth, kSaveTestPathTooLong };
	bool all = true;

	for (size_t a = 0; a < sizeof(refusals) / sizeof(refusals[0]); a++)
	{
		all = all && SaveTest_Why(refusals[a]) != nullptr && strncmp(SaveTest_Why(refusals[a]), "save test: ", 11) == 0;

		for (size_t b = 0; b < a; b++)
		{
			all = all && strcmp(SaveTest_Why(refusals[a]), SaveTest_Why(refusals[b])) != 0;
		}
	}

	Check(all, "each refusal has a reason of its own, starting \"save test: \"");
	Check(SaveTest_Why(kSaveTestLoad) == nullptr && SaveTest_Why(kSaveTestMake) == nullptr, "and the two that run have none");
}

int main()
{
	Loading();
	Making();
	Reasons();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all save test rules checks passed\n");
	return 0;
}
