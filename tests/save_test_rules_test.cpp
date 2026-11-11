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

// The level memory pool, on in every build a player gets (SAVE-HOSTILE-1, added 2026-11-10 by Melanikus for the AvP port). A sanitizer
// build turns it off to see each level object as an allocation of its own, by building with USE_LEVEL_MEMORY_POOL set to 0. Nothing a
// player's game is built from may set it: the project, the packaging script and the workflows are read here.
static std::string ReadText(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void ThePool()
{
	std::string const pool = ReadText("3dc/avp/mempool.h");
	std::string const project = ReadText("win32/VS2026/AvP.vcxproj");
	std::string const packaging = ReadText("tools/build-and-package.ps1");
	std::string const builds = ReadText(".github/workflows/build.yml") + ReadText(".github/workflows/game-build.yml") + ReadText(".github/workflows/release.yml");
	Check(pool.size() > 100 && project.size() > 1000 && packaging.size() > 1000 && builds.size() > 1000, "mempool.h, the project, the packaging script and the build workflows were found and read");

	Check(pool.find("#ifndef USE_LEVEL_MEMORY_POOL\n#define USE_LEVEL_MEMORY_POOL 1\n#endif\n") != std::string::npos,
		"the pool is on unless a build asks for it off");
	Check(project.find("USE_LEVEL_MEMORY_POOL") == std::string::npos && packaging.find("USE_LEVEL_MEMORY_POOL") == std::string::npos &&
		builds.find("USE_LEVEL_MEMORY_POOL") == std::string::npos, "and nothing a player's game is built from asks for it off");
	Check(project.find("EnableASAN") == std::string::npos && packaging.find("fsanitize") == std::string::npos && builds.find("fsanitize") == std::string::npos &&
		builds.find("EnableASAN") == std::string::npos, "nor for the sanitizer: that's a local build of its own, never a player's");

	// With the pool off, a shape is freed piece by piece (DeallocateRifLoadedShapeheader in chunk_load.cpp). The 1999 code there chose how to
	// free a shape's animation frames by the shape's "multi-view sprite" flag, which comes from the level's file, and freed them as a kind of
	// frame this game never makes. So that part must free every frame as the loader made it, whatever the flag says (added 2026-11-11 by
	// Melanikus for the AvP port, SAVE-HOSTILE-1).
	std::string const chunkLoad = ReadText("3dc/win95/chunk_load.cpp");
	size_t const freeing = chunkLoad.find("void DeallocateRifLoadedShapeheader(SHAPEHEADER *shp)");
	size_t const poolOff = freeing == std::string::npos ? std::string::npos : chunkLoad.find("#if !USE_LEVEL_MEMORY_POOL", freeing);
	size_t const poolOffEnds = poolOff == std::string::npos ? std::string::npos : chunkLoad.find("#endif  //!USE_LEVEL_MEMORY_POOL", poolOff);
	Check(freeing != std::string::npos && poolOff != std::string::npos && poolOffEnds != std::string::npos,
		"chunk_load.cpp's piece-by-piece freeing of a shape, for the pool off, was found");

	if (poolOffEnds != std::string::npos)
	{
		std::string const poolOffPart = chunkLoad.substr(poolOff, poolOffEnds - poolOff);
		Check(poolOffPart.find("& ShapeFlag_MultiViewSprite") == std::string::npos,
			"with the pool off, a shape's frames are freed without asking the level's multi-view sprite flag");
		Check(poolOffPart.find("txanimframe_mvs") == std::string::npos, "and never as the multi-view frame this game doesn't have");
		Check(poolOffPart.find("DeallocateMem(th->txa_framedata[k].txf_uvdata);") != std::string::npos,
			"each frame's numbers are freed as the loader made them, one array a frame");
	}
}

int main()
{
	Loading();
	Making();
	Reasons();
	ThePool();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all save test rules checks passed\n");
	return 0;
}
