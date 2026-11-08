// The names placed NPCs have in a level, for a network game: see 3dc/avp/npc_level_names.h. Written 2026-11-06 by Melanikus for the AvP port.
//
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a restart that doesn't empty the list leaves the first run's name to be found, and the game's lines that empty it are read (#126)

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "npc_level_names.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void NameFrom(char *name, int number)
{
	memset(name, 0, kNpcNameLength);
	memcpy(name, &number, sizeof(number));
}

static NpcLevelNames list;

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// A file has one kind of line ending or the other depending on how it was cloned, and these checks shouldn't care
	std::string text = all.str();
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

// The text from one marker up to the next one after it. Empty if either isn't there, so a check looking inside it fails instead of passing on nothing.
static std::string Between(std::string const &text, char const *from, char const *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + 1);

	return (end == std::string::npos) ? "" : text.substr(start, end - start);
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

// #126, as the game had it before the fix. In a co-op game, a placed facehugger (sulaco's hugger1) gets a new name when it's made, and its
// pair goes in the list. The host dies, and the restart makes it again with the next new name. If the list isn't emptied in between, the
// lookup a switch makes by its level name finds the first run's pair, a name nothing has any more, so the switch that wakes it finds nothing.
static void TheRestart()
{
	char level[kNpcNameLength], firstRun[kNpcNameLength], afterRestart[kNpcNameLength];

	NameFrom(level, 444);
	NameFrom(firstRun, 555);
	NameFrom(afterRestart, 556);

	puts("a restart");
	NpcLevelNames_Clear(&list);
	NpcLevelNames_Add(&list, level, firstRun);
	NpcLevelNames_Add(&list, level, afterRestart);
	char const *found = NpcLevelNames_NetworkNameFor(&list, level);
	Check(found && memcmp(found, firstRun, kNpcNameLength) == 0, "not emptied in between, the switch finds the first run's name: the state carried over");

	NpcLevelNames_Clear(&list);
	NpcLevelNames_Add(&list, level, firstRun);
	NpcLevelNames_Clear(&list);   // what DestroyAllStrategyBlocks does now, before the NPCs are made again
	NpcLevelNames_Add(&list, level, afterRestart);
	found = NpcLevelNames_NetworkNameFor(&list, level);
	Check(found && memcmp(found, afterRestart, kNpcNameLength) == 0, "emptied in between, it finds the NPC the restart made");
	Check(list.count == 1, "and the list holds only this run's pair, so a long game can't fill it");

	puts("the game empties it whenever its NPCs are destroyed (strategy_def.cpp and system.cpp, read as text)");
	std::string const strategies = ReadSourceFile("3dc/avp/strategy_def.cpp");
	std::string const destroyAll = Between(strategies, "void DestroyAllStrategyBlocks(void)", "void AssignNewSBName(STRATEGYBLOCK *sbPtr)");
	Check(Has(destroyAll, "NpcLevelNames_Clear(&TheNpcLevelNames);"), "DestroyAllStrategyBlocks empties the list");
	Check(Has(strategies, "static NpcLevelNames TheNpcLevelNames;") && Has(strategies, "NpcLevelNames_Add(&TheNpcLevelNames, levelName, sbPtr->SBname)"),
		"and that's the list NameFromTheLevel fills");

	std::string const systemFile = ReadSourceFile("3dc/avp/win95/system.cpp");
	std::string const restart = Between(systemFile, "void RestartLevel()\n{", "create_strategies_from_list();");
	Check(Has(restart, "DestroyAllStrategyBlocks();"), "a restart destroys every NPC before it makes them again");
	Check(Has(Between(systemFile, "int Destroy_CurrentEnvironment(void)", "KillFarModuleLocs();"), "DestroyAllStrategyBlocks();"), "and so does a level's end");
}


int main()
{
	char level[kNpcNameLength], network[kNpcNameLength], other[kNpcNameLength], none[kNpcNameLength];

	NameFrom(level, 111);
	NameFrom(network, 222);
	NameFrom(other, 333);
	memset(none, 0, sizeof(none));

	puts("finding an NPC by the name it has in the level");
	NpcLevelNames_Clear(&list);
	Check(list.count == 0, "it starts empty");
	Check(NpcLevelNames_NetworkNameFor(&list, level) == 0, "an empty list finds nothing");

	Check(NpcLevelNames_Add(&list, level, network), "a name goes in");
	char const *found = NpcLevelNames_NetworkNameFor(&list, level);
	Check(found && memcmp(found, network, kNpcNameLength) == 0, "and its new name comes back");
	Check(NpcLevelNames_NetworkNameFor(&list, other) == 0, "another name finds nothing");
	Check(NpcLevelNames_NetworkNameFor(&list, network) == 0, "its new name isn't a name in the level");

	puts("finding the name in the level from the new name");
	found = NpcLevelNames_LevelNameFor(&list, network);
	Check(found && memcmp(found, level, kNpcNameLength) == 0, "the new name leads back to the level's");
	Check(NpcLevelNames_LevelNameFor(&list, level) == 0, "the level's own name isn't a new name");
	Check(NpcLevelNames_LevelNameFor(&list, other) == 0, "and a name that was never given finds nothing, as for something a generator made");

	puts("no name at all");
	Check(NpcLevelNames_Add(&list, none, network) && list.count == 1, "an NPC with no name in the level isn't kept");
	Check(NpcLevelNames_NetworkNameFor(&list, none) == 0, "and asking for no name finds nothing");

	// The list never keeps an entry with no name, but the lookup doesn't count on that. It answers no name with nothing even when the list
	// somehow holds such an entry, so a name that's all zeros is never taken for an NPC's.
	{
		int const before = list.count;

		memset(list.names[list.count].level, 0, kNpcNameLength);
		memcpy(list.names[list.count].network, network, kNpcNameLength);
		list.count++;
		Check(NpcLevelNames_NetworkNameFor(&list, none) == 0, "even in a list that holds an entry with no name");
		list.count = before;
	}

	puts("a name that's the same as its new one");
	NpcLevelNames_Clear(&list);
	NpcLevelNames_Add(&list, level, level);
	Check(NpcLevelNames_NetworkNameFor(&list, level) == 0, "isn't an answer, so a caller can't go round for ever");

	puts("the same name twice");
	NpcLevelNames_Clear(&list);
	NpcLevelNames_Add(&list, level, network);
	NpcLevelNames_Add(&list, level, other);
	found = NpcLevelNames_NetworkNameFor(&list, level);
	Check(found && memcmp(found, network, kNpcNameLength) == 0, "the first one is the one found");

	puts("too many, as a hostile level file could ask for");
	NpcLevelNames_Clear(&list);

	for (int i = 0; i < kMostNpcLevelNames; i++)
	{
		char a[kNpcNameLength], b[kNpcNameLength];

		NameFrom(a, 1000 + i);
		NameFrom(b, 100000 + i);

		if (!NpcLevelNames_Add(&list, a, b))
		{
			Check(false, "a name that fits was refused");
			break;
		}
	}

	Check(list.count == kMostNpcLevelNames, "it holds as many as it says");
	NameFrom(level, 1000 + kMostNpcLevelNames - 1);
	NameFrom(network, 100000 + kMostNpcLevelNames - 1);
	found = NpcLevelNames_NetworkNameFor(&list, level);
	Check(found && memcmp(found, network, kNpcNameLength) == 0, "and the last that fits can be found");

	NameFrom(level, 999999);
	Check(!NpcLevelNames_Add(&list, level, network), "one too many says so, so the game can log it");
	Check(list.count == kMostNpcLevelNames, "and isn't written");
	Check(NpcLevelNames_NetworkNameFor(&list, level) == 0, "and can't be found");
	Check(NpcLevelNames_Add(&list, level, network), "the next one doesn't say so again, so it's logged once");
	Check(list.count == kMostNpcLevelNames, "and isn't written either");

	NpcLevelNames_Clear(&list);
	Check(list.count == 0 && !list.saidItIsFull, "a new level starts again");

	TheRestart();

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
