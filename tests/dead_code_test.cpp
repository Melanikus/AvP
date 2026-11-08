// The test that code taken out because it could never run stays out (#135). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY. Code that can never run only makes the game harder to understand (AVP Tao 2: dead code goes). #135 took out:
//   - the xenoborg morph room: nothing in a level makes one (no level loader asks for it);
//   - a lift's change of environment: every lift station is made in the level being played (object_setup.cpp gives each one the level
//     being played), so the code for a station in another one, and everything only it used, could never run;
//   - a proximity door's lock by another door: the level loader always made it "no lock", so its target was never looked for, and nothing
//     read it;
//   - GetMeThisStrategyBlock, which nothing called, and the list of yard doors only it read;
//   - set_link_switch_states_off, switched off with #if 0 since 1999.
// If one comes back by mistake, a merge say, this fails and names it.
//
// WHAT IT TESTS, by reading the game's files with their comments taken out (a change notice may name what was taken out):
//   - each name that was taken out is in none of the files it was in;
//   - the morph room's files are gone, from the folder and from the project;
//   - the morph room's kind keeps its number: it stays in the list of kinds, between the two kinds it was between, as saved games carry the
//     numbers (savegame.cpp reads them back), and no code handles it.
// Every file is checked to have been read, so a file that moved fails by name instead of passing with nothing in it.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/dead_code_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/dead_code_test.json proves each check can fail, by putting each thing back.

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The repository's top, from this file's own path: /FC in this test's settings makes __FILE__ the whole path
static std::string RepositoryTop()
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	return tests == std::string::npos ? std::string() : path.substr(0, tests);
}

// Whether a file of the repository's can be opened at all
static bool FileIsThere(char const *name)
{
	std::ifstream file((RepositoryTop() + name).c_str(), std::ios::binary);
	return file.good();
}

// A file of the game's, with its line ends written \n whatever the checkout made them. Empty if it can't be read.
static std::string ReadSourceFile(char const *name)
{
	std::ifstream file((RepositoryTop() + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

// The same text with its comments taken out, // to the end of the line and /* to */, but not inside "..." or '...', so a name that's
// only in a change notice doesn't count. What's left of each line stays on its line.
static std::string WithoutComments(std::string const &text)
{
	std::string out;
	out.reserve(text.size());
	size_t i = 0;

	while (i < text.size())
	{
		char const c = text[i];
		char const next = i + 1 < text.size() ? text[i + 1] : '\0';

		if (c == '/' && next == '/')
		{
			while (i < text.size() && text[i] != '\n')
			{
				i++;
			}
		}
		else if (c == '/' && next == '*')
		{
			i += 2;

			while (i < text.size() && !(text[i] == '*' && i + 1 < text.size() && text[i + 1] == '/'))
			{
				if (text[i] == '\n')
				{
					out += '\n';
				}

				i++;
			}

			i += 2;
		}
		else if (c == '"' || c == '\'')
		{
			// a string or a character, copied as it is, with its escapes
			out += c;
			i++;

			while (i < text.size() && text[i] != c && text[i] != '\n')
			{
				if (text[i] == '\\' && i + 1 < text.size())
				{
					out += text[i++];
				}

				out += text[i++];
			}

			if (i < text.size())
			{
				out += text[i++];
			}
		}
		else
		{
			out += c;
			i++;
		}
	}

	return out;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

// One of the game's files, read, with its comments out, and checked to have been read: a marker that has to be in it is in it
static std::string Code(char const *name, char const *marker)
{
	std::string const code = WithoutComments(ReadSourceFile(name));
	Check(Has(code, marker), std::string(name) + " was read (it has " + marker + ")");
	return code;
}

// None of the names in a file
static void NoneOf(std::string const &code, char const *file, char const *const *names, size_t count)
{
	for (size_t k = 0; k < count; k++)
	{
		Check(!Has(code, names[k]), std::string(file) + " has no " + names[k]);
	}
}

#define NONE_OF(code, file, ...) \
	do \
	{ \
		static char const *const names[] = { __VA_ARGS__ }; \
		NoneOf(code, file, names, sizeof(names) / sizeof(names[0])); \
	} while (0)

int main()
{
	printf("dead_code_test: code that could never run stays out (#135)\n");

	// The xenoborg morph room
	Check(!FileIsThere("3dc/avp/bh_spcl.cpp") && !FileIsThere("3dc/avp/bh_spcl.h") && FileIsThere("3dc/avp/bh_types.cpp"),
		"the morph room's files, bh_spcl.cpp and bh_spcl.h, are gone (and the folder is the right one: bh_types.cpp is there)");
	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj"), filters = ReadSourceFile("win32/VS2026/AvP.vcxproj.filters");
	Check(Has(project, "bh_types.cpp") && Has(filters, "bh_types.cpp"), "the project files were read");
	Check(!Has(project, "bh_spcl") && !Has(filters, "bh_spcl"), "neither project file lists the morph room's files");

	std::string const types = Code("3dc/avp/bh_types.cpp", "void AssignAllSBNames()");
	NONE_OF(types, "bh_types.cpp", "bh_spcl.h", "XenoMorphRoom", "XENO_MORPH_ROOM", "I_BehaviourXenoborgMorphRoom", "door_lock_target", "lockable_door",
		"has_lock_target", "RequestEnvChangeViaLift", "SBNeededForNextEnv", "curr_stn->env");

	std::string const setup = Code("3dc/avp/win95/object_setup.cpp", "static void add_prox_door");
	NONE_OF(setup, "object_setup.cpp", "bh_spcl.h", "XENO_MORPH_ROOM", "xmrtt", "Xenoborg_Morph_Thingy", "has_lock_target", "ltt->environment", "ltt->orient");

	// The morph room's kind keeps its number, and nothing handles it
	std::string const kinds = Code("3dc/avp/strategy_def.h", "I_BehaviourLightFX,");
	std::string::size_type const test = kinds.find("I_BehaviourTest,"), room = kinds.find("I_BehaviourXenoborgMorphRoom,"), light = kinds.find("I_BehaviourLightFX,");
	Check(test != std::string::npos && room != std::string::npos && light != std::string::npos && test < room && room < light &&
		kinds.find_first_not_of(" \t\n", test + 16) == room && kinds.find_first_not_of(" \t\n", room + 29) == light,
		"the morph room's kind stays in the list, right after I_BehaviourTest and before I_BehaviourLightFX, so no kind's number moves");

	// The proximity door's lock by another door
	std::string const typesHeader = Code("3dc/avp/bh_types.h", "} PROXDOOR_BEHAV_BLOCK;");
	NONE_OF(typesHeader, "bh_types.h", "door_lock_target", "lockable_door", "has_lock_target");

	// A lift's change of environment, and everything only it used
	std::string const lift = Code("3dc/avp/bh_lift.cpp", "void LiftBehaveFun(STRATEGYBLOCK* sbptr)");
	NONE_OF(lift, "bh_lift.cpp", "CleanUpLiftControl", "TeleportFloorSwitches", "EC_Lift_Ctrl", "Old_Pos_Module", "RequestEnvChange", "->env",
		"lift_tt->environment", "->orient", "IntegrateNewEnvironment", "ChangeEnvironmentToEnv", "PreservedSB", "PreserveStBlocksInModule",
		"UpdateWeaponShape", "#if 0");

	std::string const liftHeader = Code("3dc/avp/bh_lift.h", "} LIFT_TOOLS_TEMPLATE;");
	NONE_OF(liftHeader, "bh_lift.h", "CleanUpLiftControl", "RequestEnvChange", "I_AVP_ENVIRONMENTS env", "int environment", "int orient");

	std::string const game = Code("3dc/avp/game.cpp", "ObjectDynamics();");
	NONE_OF(game, "game.cpp", "RequestEnvChangeViaLift", "CleanUpLiftControl");

	std::string const system = Code("3dc/avp/win95/system.cpp", "void LoadRifFile()");
	NONE_OF(system, "system.cpp", "ChangeEnvironmentToEnv", "IntegrateNewEnvironment");

	std::string const gamedef = Code("3dc/avp/gamedef.h", "extern int Destroy_CurrentEnvironment();");
	NONE_OF(gamedef, "gamedef.h", "ChangeEnvironmentToEnv");

	std::string const strategy = Code("3dc/avp/strategy_def.cpp", "void DestroyAnyStrategyBlock(STRATEGYBLOCK *sbptr)");
	NONE_OF(strategy, "strategy_def.cpp", "PreservedSB", "PreserveStBlocksInModule", "SBNeededForNextEnv", "SB_Preserved", "MAX_PRESERVED_SB");
	NONE_OF(kinds, "strategy_def.h", "PreservedSB", "PreserveStBlocksInModule", "SBNeededForNextEnv", "MAX_PRESERVED_SB");

	std::string const weapons = Code("3dc/avp/weapons.cpp", "void GetHierarchicalWeapon(");
	std::string const weaponsHeader = Code("3dc/avp/weapons.h", "extern void GrabWeaponShape(PLAYER_WEAPON_DATA *weaponPtr);");
	NONE_OF(weapons, "weapons.cpp", "UpdateWeaponShape");
	NONE_OF(weaponsHeader, "weapons.h", "UpdateWeaponShape");

	// GetMeThisStrategyBlock and the yard doors only it read
	std::string const flow = Code("3dc/avp/gameflow.cpp", "BOOL GameFlowOn = 1;");
	std::string const flowHeader = Code("3dc/avp/gameflow.h", "GOT_LASTGAMEOBJECT,");
	NONE_OF(flow, "gameflow.cpp", "GetMeThisStrategyBlock", "GameStratblockList", "GT_YARD_DOOR");
	NONE_OF(flowHeader, "gameflow.h", "GetMeThisStrategyBlock", "GAMETARGETOBJECTS", "GAME_STRATBLOCK_LIST_ENTRY", "GT_YARD_DOOR");

	// The link switch's code under #if 0
	std::string const links = Code("3dc/avp/bh_link_switch.cpp", "static BOOL check_link_switch_states");
	NONE_OF(links, "bh_link_switch.cpp", "set_link_switch_states_off", "#if 0");

	if (checksFailed)
	{
		printf("dead_code_test: %d check(s) FAILED\n", checksFailed);
		return 1;
	}

	printf("dead_code_test: all checks passed\n");
	return 0;
}
