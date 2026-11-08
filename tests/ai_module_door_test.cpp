// The test of which module makes an AI module a door, and whether the aliens' scent goes through it (3dc/avp/ai_module_door.h). Written
// 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// - The header, with stand-ins for the game's modules and blocks: the door is the first module on the list that is one, wherever it is on
//   the list; only that module is asked whether it's locked or open; a list whose first module has no block, and a door after it, is
//   answered from the door; a door that can't be read lets the scent through; and an empty list or no list is no door.
// - By reading the game's files: the kinds agree with the game's, and the scent test asks only that door, never the list's first module
//   itself, checking its block is a proximity door's, with data, before reading it. The pieces of source are taken by tests/source_slice.h,
//   so a marker that has moved fails by name.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/ai_module_door_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/ai_module_door_test.json proves each check can fail.

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ai_module_door.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// How tests/source_slice.h fails a check when a marker isn't there
static void FailBecause(std::string const &why)
{
	Check(false, why);
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

// The text from one marker up to the next one after it, by tests/source_slice.h: a marker that isn't there is a failed check of its own
static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, FailBecause);
}

// Stand-ins for the game's blocks and modules, with only what the header's callers read: a block's kind (a kAIModule... number here),
// whether it has data, and for a door, whether it's locked or open
struct Block
{
	int kind;
	bool hasData;
	bool locked;
	bool open;
};

struct Module
{
	Block *m_sbptr;
};

// Every module asked whether it's locked or open, so a test can say which ones were
static std::vector<Module *> asked;

static int KindOf(Module *module)
{
	return module->m_sbptr ? module->m_sbptr->kind : kAIModuleNotADoor;
}

static int LockedOf(Module *door)
{
	asked.push_back(door);

	if (!door || !door->m_sbptr || door->m_sbptr->kind != kAIModuleProxDoor || !door->m_sbptr->hasData)
	{
		return -1;
	}

	return door->m_sbptr->locked ? 1 : 0;
}

static bool OpenOf(Module *door)
{
	asked.push_back(door);
	return door && door->m_sbptr && door->m_sbptr->open;
}

static bool LetsThrough(Module *const *list)
{
	asked.clear();
	return AIModuleDoor_LetsScentThrough(list, KindOf, LockedOf, OpenOf);
}

static void TestFindingTheDoor()
{
	puts("Finding the door:");

	Block proxDoor = { kAIModuleProxDoor, true, false, false };
	Block liftDoor = { kAIModuleLiftDoor, true, false, false };
	Block wall = { kAIModuleNotADoor, true, false, false };
	Module noBlock = { 0 }, door = { &proxDoor }, lift = { &liftDoor }, plain = { &wall };

	int kind = 99;
	Module *const list[] = { &noBlock, &plain, &door, &lift, 0 };
	Check(AIModuleDoor_Find(list, KindOf, &kind) == &door && kind == kAIModuleProxDoor, "the door is the first module that is one, wherever it is on the list");

	kind = 99;
	Module *const plainOnes[] = { &noBlock, &plain, 0 };
	Check(AIModuleDoor_Find(plainOnes, KindOf, &kind) == 0 && kind == kAIModuleNotADoor, "a list with no door has none");

	kind = 99;
	Module *const empty[] = { 0 };
	Check(AIModuleDoor_Find(empty, KindOf, &kind) == 0 && kind == kAIModuleNotADoor, "nor does an empty list");

	kind = 99;
	Check(AIModuleDoor_Find(static_cast<Module *const *>(0), KindOf, &kind) == 0 && kind == kAIModuleNotADoor, "nor no list at all");

	kind = 99;
	Module *const endsEarly[] = { &plain, 0, &door, 0 };
	Check(AIModuleDoor_Find(endsEarly, KindOf, &kind) == 0 && kind == kAIModuleNotADoor, "and nothing after the list's end is looked at");
}

static void TestTheScent()
{
	puts("Whether the scent goes through:");

	Block lockedDoor = { kAIModuleProxDoor, true, true, false };
	Block unlockedDoor = { kAIModuleProxDoor, true, false, false };
	Block doorWithNoData = { kAIModuleProxDoor, false, true, false };
	Block closedLift = { kAIModuleLiftDoor, true, false, false };
	Block openSwitchDoor = { kAIModuleSecurityDoor, true, false, true };
	Block wall = { kAIModuleNotADoor, true, false, false };
	Module noBlock = { 0 }, locked = { &lockedDoor }, unlocked = { &unlockedDoor }, noData = { &doorWithNoData };
	Module lift = { &closedLift }, switchDoor = { &openSwitchDoor }, plain = { &wall };

	// What a level file can make: a first module with no block at all, and the door after it
	Module *const hostile[] = { &noBlock, &locked, 0 };
	bool const through = LetsThrough(hostile);
	Check(!through && asked.size() == 1 && asked[0] == &locked,
		"a first module with no block, then a locked proximity door: answered from the door, which is the only module asked");

	Module *const hostileOpen[] = { &noBlock, &unlocked, 0 };
	Check(LetsThrough(hostileOpen) && asked.size() == 1 && asked[0] == &unlocked, "and with the door unlocked, the scent goes through");

	Module *const plainOnes[] = { &plain, &noBlock, 0 };
	Check(LetsThrough(plainOnes) && asked.empty(), "a module that isn't a door lets it through, and nothing is asked");

	Module *const unreadable[] = { &plain, &noData, 0 };
	Check(LetsThrough(unreadable) && asked.size() == 1, "a proximity door that can't be read lets it through, as if it weren't there");

	Module *const liftFirst[] = { &plain, &lift, &locked, 0 };
	Check(!LetsThrough(liftFirst) && asked.size() == 1 && asked[0] == &lift, "the first door decides: a closed lift door, with a locked door after it, is asked alone");

	Module *const openOne[] = { &switchDoor, 0 };
	Check(LetsThrough(openOne) && asked.size() == 1 && asked[0] == &switchDoor, "an open switch door lets it through");

	Check(LetsThrough(static_cast<Module *const *>(0)) && asked.empty(), "and no list at all is no door");
}

static void TestTheGameUsesIt()
{
	puts("Where the game uses it, by reading its files:");

	std::string const far = ReadSourceFile("3dc/avp/pfarlocs.cpp");
	Check(Has(far, "static_assert(MDT_NotADoor == kAIModuleNotADoor && MDT_ProxDoor == kAIModuleProxDoor && MDT_LiftDoor == kAIModuleLiftDoor && MDT_SecurityDoor == kAIModuleSecurityDoor,"),
		"the kinds are the game's, in its order");
	Check(Has(Between(far, "int AIModuleDoorKindOf(MODULE *module)", "\n}\n"), "return static_cast<int>(ModuleIsADoor(module));"), "a module's kind is the game's own test of it");

	std::string const scent = ReadSourceFile("3dc/avp/pheromone.cpp");
	std::string const admits = Between(scent, "int AIModuleAdmitsPheromones(AIMODULE *targetModule)", "\n}\n");
	Check(Has(admits, "return AIModuleDoor_LetsScentThrough(targetModule->m_module_ptrs, AIModuleDoorKindOf, ProxDoorIsLocked, DoorIsOpen) ? 1 : 0;")
		&& !Has(admits, "*(targetModule->m_module_ptrs)"), "the scent test asks only the door, and never reads the list's first module itself");

	std::string const locked = Between(scent, "static int ProxDoorIsLocked(MODULE *door)", "\n}\n");
	Check(Has(locked, "STRATEGYBLOCK *const block = door ? door->m_sbptr : 0;")
		&& Has(locked, "if (!block || block->I_SBtype != I_BehaviourProximityDoor || !block->SBdataptr)\n\t{\n\t\treturn -1;"),
		"a door's block has to be there, be a proximity door's and have its data");
	Check(Has(locked, "return static_cast<PROXDOOR_BEHAV_BLOCK const *>(block->SBdataptr)->door_locked ? 1 : 0;")
		&& locked.find("door_locked") > locked.find("return -1;"), "before door_locked is read");
	Check(Has(Between(scent, "static bool DoorIsOpen(MODULE *door)", "\n}\n"), "return door && GetState(door->m_sbptr);"), "and a lift or switch door is asked whether it's open, if there is one");
}

int main()
{
	TestFindingTheDoor();
	TestTheScent();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
