// Tests where a door's branch in a module's visibility list may go (3dc/vmodule_branch.h). Written 2026-11-14 by Melanikus for the
// AvP port.
//
// WHY: the game walks a module's visibility list each frame, and an item that "branches if the door is closed" jumps along it. The jump
// used to go wherever the item's pointer said. Now it's taken only if it lands on a later item in the same list, no further than its end
// marker; anything else goes on to the next item, as for an open door.
//
// WHAT: the rule itself, on a list of plain numbers standing in for the game's items, with every kind of place a jump could be given;
// and, by reading module.cpp's source (it needs the whole game to run), that all three walks of the list go through the rule, each with
// its list's own end, and that module.cpp reads where a branch goes nowhere else; and that each walk skips a branch item with no module
// of its own. tests/takeouts/vmodule_branch_test.json proves each check can fail.

#include "vmodule_branch.h"

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>

static int checks = 0;
static int failures = 0;

static void Check(bool good, const std::string &what)
{
	checks++;

	if (!good)
	{
		failures++;
		printf("  FAILED  %s\n", what.c_str());
	}
	else
	{
		printf("  ok  %s\n", what.c_str());
	}
}

// A file of the game's source, found from this test's own place, with its carriage returns taken out
static std::string ReadSource(const char *name)
{
	const std::string path = __FILE__;
	const std::string::size_type tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream in((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << in.rdbuf();
	std::string text = all.str();
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

static int Times(const std::string &text, const char *what)
{
	int times = 0;
	std::string const wanted(what);

	for (std::string::size_type at = text.find(wanted); at != std::string::npos; at = text.find(wanted, at + wanted.size()))
	{
		times++;
	}

	return times;
}

// One function of a source file: from its first line (with its opening brace, so a declaration of it doesn't count) to the brace that
// closes it, the first one alone at the start of a line. Empty if the first line isn't there.
static std::string FunctionText(const std::string &source, const char *firstLine)
{
	std::string::size_type const start = source.find(firstLine);
	std::string::size_type const close = start == std::string::npos ? std::string::npos : source.find("\n}\n", start);

	return close == std::string::npos ? std::string() : source.substr(start, close + 3 - start);
}

// Is this line in the walk exactly once, and inside its loop, after the walk's "while("? Then it's done again for every item.
static bool OnceForEachItem(const std::string &walk, const char *line)
{
	std::string::size_type const loop = walk.find("while(");
	std::string::size_type const at = walk.find(line);

	return Times(walk, line) == 1 && loop != std::string::npos && at != std::string::npos && at > loop;
}

// A stand-in for the game's VMODULE: big enough that a place halfway into one can be told from the start of one
struct Item
{
	int number;
	int more[7];
};

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	puts("Where a branch may go:");

	// A list of five items and its end marker. The walk is at the second item.
	Item list[6] = {};
	Item *const item = &list[1];
	Item *const end = &list[5];
	Item *const next = &list[2];

	Check(VModuleBranchTarget(item, &list[3], end) == &list[3], "a branch to a later item is taken");
	Check(VModuleBranchTarget(item, &list[2], end) == &list[2], "and so is one to the very next item");
	Check(VModuleBranchTarget(item, end, end) == end, "and one to the end marker, which ends the walk");

	Check(VModuleBranchTarget(item, item, end) == next, "a branch to the same item isn't taken: it would go round for ever");
	Check(VModuleBranchTarget(item, &list[0], end) == next, "nor is one back up the list");
	Check(VModuleBranchTarget(item, end + 1, end) == next, "nor one past the end marker");
	Check(VModuleBranchTarget(item, (Item *)0, end) == next, "nor one to nothing");

	// A place inside a later item, not at its start, and places that are nowhere near the list at all
	Item *const halfway = (Item *)((char *)&list[3] + sizeof(int));
	Check(VModuleBranchTarget(item, halfway, end) == next, "nor one to a place halfway into an item");
	Check(VModuleBranchTarget(item, (Item *)(uintptr_t)7, end) == next, "nor one to a small number, as a level file could leave");
	Check(VModuleBranchTarget(item, (Item *)~(uintptr_t)0, end) == next, "nor one to the very top of memory");

	puts("Where the game uses it, in module.cpp:");

	std::string const module = ReadSource("3dc/module.cpp");
	Check(!module.empty(), "module.cpp is there to read");
	Check(Times(module, "#include \"vmodule_branch.h\"") == 1, "it uses the rule");
	// The three walks of a list. Each finds its own list's end first, and jumps only through the rule.
	std::string const visible = FunctionText(module, "void FindVisibleModules(VMODULE *vptr,int flag)\n{");
	std::string const objectSeen = FunctionText(module, "int ThisObjectIsInAModuleVisibleFromCurrentlyVisibleModules(STRATEGYBLOCK *sbPtr)\n{");
	std::string const moduleSeen = FunctionText(module, "int IsModuleVisibleFromModule(MODULE *source, MODULE *target) {");

	Check(Times(visible, "VMODULE *const end = EndOfVisibilityList(vptr);") == 1
		&& Times(visible, "vptr = VModuleBranchTarget(vptr, vptr->vmod_data.vmodidata_ptr, end);") == 1,
		"the walk that finds what's visible branches through it, with the end of its own list");
	Check(Times(objectSeen, "VMODULE *const end = EndOfVisibilityList(vPtr);") == 1
		&& Times(objectSeen, "vPtr = VModuleBranchTarget(vPtr, vPtr->vmod_data.vmodidata_ptr, end);") == 1,
		"and so does the walk that asks whether an object can be seen");
	Check(Times(moduleSeen, "VMODULE *const end = EndOfVisibilityList(vptr);") == 1
		&& Times(moduleSeen, "vptr = VModuleBranchTarget(vptr, vptr->vmod_data.vmodidata_ptr, end);") == 1,
		"and so does the walk that asks whether one module can see another");

	// Every place module.cpp reads where a branch goes, and every place it handles a branch at all. A new one, in a new walk or an old
	// one, changes these numbers: then it has to go through the rule too, and this test has to be looked at again, with its reason.
	int const reads = Times(module, "vmod_data.vmodidata_ptr");
	int const readsThroughTheRule = Times(module, "VModuleBranchTarget(vptr, vptr->vmod_data.vmodidata_ptr, end)")
		+ Times(module, "VModuleBranchTarget(vPtr, vPtr->vmod_data.vmodidata_ptr, end)");
	Check(reads == 3, "module.cpp reads where a branch goes in 3 places, one in each walk");
	Check(readsThroughTheRule == reads, "and none of them follows it without the rule");
	Check(Times(module, "case vmodi_bra_vc:") == 4,
		"and handles a branch in 4: the three walks, and the setup that turns its number into a place");

	puts("A branch item with no module of its own, in each walk:");

	// Each walk starts every item with no module, then takes the item's own if it has one, so a branch item with none never uses the
	// module of an item before it, whether it's the first item of its list or a later one. And it skips that item, so the walk always
	// moves on.
	Check(OnceForEachItem(visible, "MODULE *mptr = 0;") && OnceForEachItem(visible, "else vptr++;"),
		"the walk that finds what's visible starts each item with no module, and skips a branch item that has none");
	Check(OnceForEachItem(objectSeen, "MODULE *mptr = NULL;") && OnceForEachItem(objectSeen, "else vPtr++;"),
		"and so does the walk that asks whether an object can be seen");
	Check(OnceForEachItem(moduleSeen, "MODULE *mptr = 0;") && Times(moduleSeen, "MODULE *mptr") == 1 && OnceForEachItem(moduleSeen, "else vptr++;"),
		"and so does the walk that asks whether one module can see another: its first item or any later one, never with an earlier item's module");

	printf("%s: %d checks, %d failed\n", failures ? "SOME CHECKS FAILED" : "All checks passed", checks, failures);
	return failures ? 1 : 0;
}
