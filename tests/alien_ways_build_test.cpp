// The test for 3dc/avp/alien_ways_build.h, which builds an alien ways map on a thread of its own, and for how the game starts and stops that
// build (alien_ways.h). Written 2026-11-08 by Melanikus for the AvP port (#109, #118).
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. The first part runs real builds, on real
// threads, of made-up worlds. The rows starting SOURCE read the game's own files, to check it starts the build at each level's start, stops it at
// the end, and only with the developer switch. What those lines do in a running game needs a run of the game.

#include <stdio.h>
#include <string.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "alien_ways_build.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Says(const char *why, const char *expected)
{
	return why != nullptr && strcmp(why, expected) == 0;
}

static AlienWaysPolygon Quad(AlienWaysXYZ a, AlienWaysXYZ b, AlienWaysXYZ c, AlienWaysXYZ d, float up)
{
	AlienWaysPolygon p;
	memset(&p, 0, sizeof(p));
	p.corners = 4;
	AlienWaysXYZ const corners[4] = { a, b, c, d };

	for (int i = 0; i < 4; i++)
	{
		p.x[i] = corners[i].x;
		p.y[i] = corners[i].y;
		p.z[i] = corners[i].z;
	}

	p.up = up;
	return p;
}

// The corridor of tests/alien_ways_map_test.cpp: tiles floor tiles of 1 m along z, two walls and a ceiling
static std::vector<AlienWaysPolygon> Corridor(int tiles)
{
	std::vector<AlienWaysPolygon> world;
	int const length = tiles * 1000;

	for (int k = 0; k < tiles; k++)
	{
		world.push_back(Quad({ 0, 0, k * 1000 }, { 2000, 0, k * 1000 }, { 2000, 0, (k + 1) * 1000 }, { 0, 0, (k + 1) * 1000 }, 1.0f));
	}

	world.push_back(Quad({ 0, 0, 0 }, { 0, -3000, 0 }, { 0, -3000, length }, { 0, 0, length }, 0.0f));
	world.push_back(Quad({ 2000, 0, 0 }, { 2000, -3000, 0 }, { 2000, -3000, length }, { 2000, 0, length }, 0.0f));
	world.push_back(Quad({ 0, -3000, 0 }, { 2000, -3000, 0 }, { 2000, -3000, length }, { 0, -3000, length }, -1.0f));
	return world;
}

// Waits up to 60 seconds for a build to finish, and gives back how it ended
static AlienWaysBuildState WaitFor(AlienWaysBuild &build)
{
	auto const giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(60);

	while (build.State() == kAlienWaysBuilding && std::chrono::steady_clock::now() < giveUp)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return build.State();
}

static void OnItsOwnThread()
{
	printf("A build on its own thread\n");
	AlienWaysBuild build;
	Check(build.State() == kAlienWaysNothing, "before anything starts, there's nothing");

	build.Start(Corridor(10));
	Check(WaitFor(build) == kAlienWaysReady, "the corridor's map gets ready");
	Check(build.BuiltOnItsOwnThread(), "on a thread of its own");
	Check(build.Map().Ready() && build.Map().Polygons() == 13, "with all 13 polygons on it");
	Check(build.Why() == nullptr, "and no reason given, since nothing was refused");
	Check(build.Milliseconds() >= 0, "it says how long it took");

	// The map is the game's to search once it's ready
	Check(build.Map().StartSearch(0, { 1000, 0, 500 }, 9, { 1000, 0, 9500 }, 10) == nullptr && build.Map().Work(1000000) == kAlienWaysFound,
		"and a way can be searched for on it");

	build.Stop();
	Check(build.State() == kAlienWaysNothing && !build.Map().Ready() && build.Map().Polygons() == 0, "stopped, it lets go of the map");
}

static void Refused()
{
	printf("A build that's refused\n");
	AlienWaysBuild build;
	std::vector<AlienWaysPolygon> broken = Corridor(10);
	broken[3].corners = 2;
	build.Start(std::move(broken));
	Check(WaitFor(build) == kAlienWaysRefused, "a world with a polygon of 2 corners is refused");
	Check(Says(build.Why(), "a polygon has fewer than 3 corners, or more than 8"), "with the map's own words");
	Check(!build.Map().Ready() && build.Map().Polygons() == 0, "and there's no map");
}

static void StoppedPartWay()
{
	printf("A build stopped part way, on its thread\n");

	// The map looks at the stop at its start, in its first look at the polygons, and once for each polygon as it joins them up, so stopping at
	// its third look stops it in the middle of the joining
	AlienWaysBuild build;
	build.MapForATest().StopAtLookForATest(3);
	build.Start(Corridor(10));
	Check(WaitFor(build) == kAlienWaysRefused && Says(build.Why(), "the build was asked to stop"), "stopped at its third look, it says it was asked to stop");
	Check(!build.Map().Ready() && build.Map().Polygons() == 0, "and keeps nothing");

	build.MapForATest().StopAtLookForATest(0);
	build.Start(Corridor(10));
	Check(WaitFor(build) == kAlienWaysReady, "the next build, not stopped, gets ready: the stop was forgotten");
}

static void StopWhileBuilding()
{
	printf("Stopping a build while it runs\n");

	// A long corridor, so the build is likely still going when it's stopped. Either way, Stop has to come back, with nothing left.
	AlienWaysBuild build;
	build.Start(Corridor(5000));
	build.Stop();
	Check(build.State() == kAlienWaysNothing && !build.Map().Ready() && build.Map().Polygons() == 0, "Stop comes back, and leaves nothing");

	build.Stop();
	Check(build.State() == kAlienWaysNothing, "stopping again, with nothing running, is fine");

	// Starting again goes on as if nothing had happened
	build.Start(Corridor(10));
	Check(WaitFor(build) == kAlienWaysReady && build.Map().Polygons() == 13, "a build after that gets ready");
}

static void StartTwice()
{
	printf("Starting a second build over the first\n");
	AlienWaysBuild build;
	build.Start(Corridor(5000));
	build.Start(Corridor(20));
	Check(WaitFor(build) == kAlienWaysReady && build.Map().Polygons() == 23, "the second world's map is the one made: 20 tiles, two walls and a ceiling");
}

static void NoThread()
{
	printf("When no thread can be had\n");
	AlienWaysBuild build;
	build.NoThreadForATest(true);
	build.Start(Corridor(10));
	Check(build.State() == kAlienWaysReady, "the build happens before Start comes back");
	Check(!build.BuiltOnItsOwnThread(), "and it says it wasn't on a thread of its own");
	Check(build.Map().Polygons() == 13, "with the whole map");
}

static void EndsWhileBuilding()
{
	printf("The program ending while a build runs\n");

	// If the thread were still going when its build was let go, the program would stop dead here. The build waits for it instead.
	{
		AlienWaysBuild build;
		build.Start(Corridor(5000));
	}

	Check(true, "a build let go while running waits for its thread, and the program carries on");
}

static std::string ReadSourceFile(std::string const &name)
{
	std::string const path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	std::string const all = text.str();
	std::string out;
	out.reserve(all.size());

	for (char const c : all)
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static bool Has(std::string const &text, char const *wanted)
{
	return text.find(wanted) != std::string::npos;
}

static size_t CountOf(std::string const &text, char const *wanted)
{
	size_t count = 0;

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + 1))
	{
		count++;
	}

	return count;
}

// The text from the first from to the first to after it. If either mark isn't there, that fails a check of its own, by name, so no check
// can pass on an empty piece of text without anyone hearing of it (card SOURCE-SLICE-1).
static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? std::string::npos : text.find(to, start + strlen(from));

	if (end == std::string::npos)
	{
		Check(false, std::string("SOURCE: the marks \"") + from + "\" and \"" + to + "\" are found, in that order");
		return std::string();
	}

	return text.substr(start, end - start);
}

static void SourceOfTheGame()
{
	printf("How the game starts and stops the build\n");
	std::string const game = ReadSourceFile("3dc/avp/game.cpp");
	std::string const system = ReadSourceFile("3dc/avp/win95/system.cpp");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const ways = ReadSourceFile("3dc/avp/alien_ways.cpp");
	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj");

	// The grid's ready line says how many joins an alien's way pays more to cross (added 2026-11-11 by Melanikus (Third) for the AvP port,
	// #109, corners repel; THE KIND OF A JOIN, npc_grid.h), so a level where the sort looks wrong shows in the log at once
	Check(Has(ways, "+ std::to_string(static_cast<long long>(grid->OutsideJoins())) + \" outside edges and \"")
		&& Has(ways, "+ std::to_string(static_cast<long long>(grid->ThroughJoins())) + \" joins onto hidden faces, dearer for an alien's way\");"),
		"SOURCE: the grid's ready line counts its outside edges and its joins onto hidden faces");

	Check(!game.empty() && !system.empty() && !winmain.empty() && !ways.empty() && !project.empty(), "SOURCE: the game's files can be read");

	std::string const startGame = Between(game, "void StartGame(void)", "InitHive();");
	Check(Has(startGame, "BuildFarModuleLocs();") && Has(startGame, "AlienWays_LevelStart();"), "SOURCE: a game's start builds the map, with the other AI's level setup");

	// Modified 2026-11-10 by Melanikus (Third) for the AvP port: a lift into another environment no longer builds it, because main took
	// that path out (#135: ChangeEnvironmentToEnv and IntegrateNewEnvironment could never run). A game's start is the one place left.
	Check(!Has(system, "void IntegrateNewEnvironment()"), "SOURCE: there's no lift into another environment to build the map for (#135)");

	std::string const destroy = Between(system, "int Destroy_CurrentEnvironment(void)", "CleanUpPheromoneSystem();");
	Check(Has(destroy, "AlienWays_LevelEnd();\n\tKillFarModuleLocs();"), "SOURCE: a level's end stops the build and lets the map go, with the other AI's");
	Check(CountOf(game, "AlienWays_LevelStart();") == 1 && CountOf(system, "AlienWays_LevelStart();") == 0, "SOURCE: and the build is started from a game's start only");

	// A restart makes every alien again: their ways and claims go, and the map stays (Secondary's read of 9e8c30fb)
	std::string const restart = Between(system, "void RestartLevel()", "create_strategies_from_list();");
	Check(Has(restart, "DestroyAllStrategyBlocks();") && Has(restart, "AlienWays_LevelRestarts();")
		&& restart.find("DestroyAllStrategyBlocks();") < restart.find("AlienWays_LevelRestarts();"),
		"SOURCE: a restart forgets the aliens' ways once the aliens are gone, before they're made again");
	std::string const restarts = Between(ways, "void AlienWays_LevelRestarts()", "\n}\n");
	Check(Has(restarts, "\ttheGuide.Clear();") && !Has(restarts, "theBuild.Stop();"), "SOURCE: it clears the guide, and keeps the map");
	Check(Has(restarts, "\tstd::fill(std::begin(watched), std::end(watched), Watched());"), "SOURCE: and the stuck line's watch list, as a remade alien can have an old one's place and name");

	Check(Has(game, "AiPheromoneSystem();\n\n\t// The alien ways, before any alien moves: the log hears how the map's build went, aliens no longer on a way get their usual\n"
		"\t// gravity back, and the searches for ways carry on. It does nothing with FLOORISLAVA OFF.\n\tAlienWays_Frame();"),
		"SOURCE: each frame, with the other AI systems and before any alien moves, the alien ways have their turn");

	// Since the flip (2026-11-11, #109) no command-line switch turns the ways on or off: each level's start does, from what the console asked
	Check(!Has(winmain, "devSwitch(\"-alienways\")") && !Has(winmain, "devSwitch(\"-alienwaysgrid\")") && CountOf(winmain, "AlienWaysSwitchedOn") == 0,
		"SOURCE: no command-line switch turns the ways on or off");
	Check(CountOf(ways, "\tAlienWaysSwitchedOn = ") == 1 && Has(ways, "\tAlienWaysSwitchedOn = FloorIsLavaAsked ? 1 : 0;\n") && !Has(ways, "AlienWaysSwitchedOn = 1;"),
		"SOURCE: only each level's start sets the switch, from what the console asked");

	std::string const on = Between(ways, "static bool AlienWays_On()", "void AlienWays_LevelStart()");
	Check(Has(on, "if (!AlienWaysSwitchedOn)\n\t{\n\t\treturn false;\n\t}"), "SOURCE: with the switch off there's no map");
	Check(Has(on, "return AvP.Network == I_No_Network || AvP.NetworkAIServer;"), "SOURCE: and only where the aliens are run: alone, or the network game's AI machine");

	std::string const start = Between(ways, "void AlienWays_LevelStart()", "void AlienWays_Frame()");
	Check(Has(start, "AlienWays_LevelEnd();") && start.find("AlienWays_LevelEnd();") < start.find("if (!AlienWays_On())"),
		"SOURCE: a level's start lets go of whatever the last level left, before anything else");

	// The listing reads a level's own numbers, so it's checked for what it trusts (Secondary's review, F1 and F2). The sums themselves are
	// tested in alien_ways_map_test, where they can be run.
	std::string const level = ReadSourceFile("3dc/avp/alien_ways_level.cpp");
	Check(Has(level, "int const kAlienWaysRecordCorners = 5;"), "SOURCE: the listing knows a polygon's record has room for 5 corners");
	Check(Has(level, "while (count < kAlienWaysRecordCorners && corners[count] >= 0)"), "SOURCE: and never reads past them");
	Check(Has(level, "\t\tif (count == kAlienWaysRecordCorners)\n\t\t{\n\t\t\tlisting->counts->noEnd++;\n\t\t\tcontinue;\n\t\t}"),
		"SOURCE: a polygon with no end to its corners within them is left out, and counted");
	Check(Has(level, "\t\t\tif (!AlienWays_PlaceCorner(corner, turning, where, &placed))") && !Has(level, "MUL_FIXED("),
		"SOURCE: every corner is turned and placed in 64 bits, and nothing of the level's is summed in int");
	Check(Has(level, "up = AlienWays_TurnedUp(AlienWaysXYZ{ stored.vx, stored.vy, stored.vz }, turning);"), "SOURCE: and every stored facing is turned in 64 bits");
	Check(Has(level, "extern SCENE Global_Scene;"), "SOURCE: the listing says which scene is loaded the way the game's own files do (the Game build needs it)");

	// HOSTILE MAPS D (added 2026-11-11 by Melanikus (Third) for the AvP port, #109, what a made-up level can do): the listing's own
	// cap, kAlienWaysMostListed. It can't be run here, as it reads the game's own shapes, so it's read. Each of the three loops that add to
	// the list (a shape's polygons, the level's modules, its still objects) stops once the list is past the cap, and the count is checked
	// after every polygon added, one at a time. So the list never holds more than one polygon past the cap. A listing past it is let go of,
	// all of it, and refused with its reason, which the game's log gives ("alien ways: no map: ..."); one the memory runs out for is too.
	std::string const levelHeader = ReadSourceFile("3dc/avp/alien_ways_level.h");
	Check(Has(levelHeader, "size_t const kAlienWaysMostListed = 400000;"), "SOURCE: the listing's cap is 400,000 polygons");
	Check(Has(level, "for (int number = 0; number < shape->numitems && !listing->tooMany; number++)")
		&& Has(level, "for (int i = 0; i < ModuleArraySize && !listing->tooMany; i++)")
		&& Has(level, "for (int i = 0; i < NumActiveStBlocks && !listing->tooMany; i++)"),
		"SOURCE: every loop that adds to the listing stops once it's past its cap");
	std::string const shapeLoop = Between(level, "for (int number = 0; number < shape->numitems && !listing->tooMany; number++)", "static void AddModules(Listing *listing)");
	Check(CountOf(level, "AlienWays_AddPolygon(") == 1 && Has(shapeLoop, "AlienWays_AddPolygon(") && !Has(level, "polygons->push_back(")
		&& !Has(level, "polygons->insert(") && !Has(level, "polygons->resize(") && !Has(level, "polygons->emplace"),
		"SOURCE: polygons are added to the list in one place only, one at a time, inside a shape's loop");
	Check(Has(shapeLoop, "\t\tif (listing->polygons->size() > kAlienWaysMostListed)\n\t\t{\n\t\t\tlisting->tooMany = true;\n\t\t}"),
		"SOURCE: and after each one, the count is checked against the cap");
	std::string const listing = Between(level, "const char *AlienWaysLevel_List(", "\n}\n");
	Check(Has(listing, "\tif (listing.tooMany)\n\t{\n\t\tstd::vector<AlienWaysPolygon>().swap(*polygons);\n\t\tstd::vector<AlienWaysRange>().swap(*modules);\n"
		"\t\tstd::vector<AlienWaysLevelDoor>().swap(*doors);\n\t\treturn kWhyTooMany;\n\t}"),
		"SOURCE: a listing past its cap is let go of, all of it, and refused with its reason");
	Check(Has(listing, "\tcatch (const std::bad_alloc &)\n\t{\n\t\tstd::vector<AlienWaysPolygon>().swap(*polygons);") && Has(listing, "\t\treturn kWhyNoMemory;\n"),
		"SOURCE: and one the memory runs out for is let go of and refused too, never a crash");
	Check(Has(ways, "\tconst char *const why = AlienWaysLevel_List(&polygons, &theModules, &theDoors, &counts);")
		&& Has(ways, "\tif (why)\n\t{\n\t\tLogErrorString(std::string(\"alien ways: no map: \") + why);\n\t\treturn;\n\t}"),
		"SOURCE: the game's log gives the listing's reason, and the level goes without the alien ways");

	char const *const files[] = { "alien_ways.cpp", "alien_ways_build.cpp", "alien_ways_guide.cpp", "alien_ways_level.cpp", "alien_ways_map.cpp" };

	for (char const *const name : files)
	{
		Check(Has(project, (std::string("<ClCompile Include=\"..\\..\\3dc\\avp\\") + name + "\" />").c_str()), std::string("SOURCE: the game's project builds ") + name);
	}
}

int main()
{
	OnItsOwnThread();
	Refused();
	StoppedPartWay();
	StopWhileBuilding();
	StartTwice();
	NoThread();
	EndsWhileBuilding();
	SourceOfTheGame();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all alien ways build checks passed\n");
	return 0;
}
