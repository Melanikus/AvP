// The names placed NPCs have in a level, for a network game: see 3dc/avp/npc_level_names.h. Written 2026-11-06 by Melanikus for the AvP port.

#include <stdio.h>
#include <string.h>

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

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
