// The test of what the log says about a link in a level that finds nothing (3dc/avp/link_misses.h). Written 2026-11-08 by Melanikus for
// the AvP port (#130).
//
// WHAT IT TESTS
//
// - The header on its own: a name is written once a level, no name is nothing to say, the list stops at its size and says so once, it
//   starts again for the next level, and every word of the lines.
// - By reading the game's files: every link a level makes goes through the lookup that writes a miss, at the start of a level and at each
//   restart; a link to a placed NPC this machine hasn't made is counted, not written; the lists are emptied when a level's blocks are all
//   destroyed; and both kinds of switch say which targets weren't there when they go on or off.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/link_misses_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/link_misses_test.json proves each check can fail. The pieces of source are taken by tests/source_slice.h, so a marker
// that has moved fails by name.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "link_misses.h"
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

static bool ComesBefore(std::string const &text, char const *first, char const *second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = a == std::string::npos ? std::string::npos : text.find(second, a);

	return a != std::string::npos && b != std::string::npos;
}

// How tests/source_slice.h fails a check when a marker isn't there
static void FailBecause(std::string const &why)
{
	Check(false, why);
}

// The text from one marker up to the next one after it, by tests/source_slice.h: a marker that isn't there is a failed check of its own
static std::string Between(std::string const &text, char const *from, char const *to)
{
	return SourceSlice(text, from, to, kSliceEndFromOneOn, FailBecause);
}

static int Count(std::string const &text, char const *words)
{
	int found = 0;

	for (std::string::size_type at = text.find(words); at != std::string::npos; at = text.find(words, at + 1))
	{
		found++;
	}

	return found;
}

static void NameFrom(char *name, int number)
{
	memset(name, 0, kLinkNameLength);
	memcpy(name, &number, sizeof(number));
}

static LinkMisses misses;

static void TestTheList()
{
	puts("The names written once a level:");

	char none[kLinkNameLength], first[kLinkNameLength], second[kLinkNameLength];
	memset(none, 0, sizeof(none));
	NameFrom(first, 1001);
	NameFrom(second, 1002);

	LinkMisses_Forget(&misses);
	Check(LinkMisses_Note(&misses, none) == kLinkMissNoName && misses.count == 0, "a link with no name is nothing to say, and isn't kept");
	Check(LinkMisses_Note(&misses, first) == kLinkMissWrite && misses.count == 1, "a name that finds nothing is written the first time");
	Check(LinkMisses_Note(&misses, first) == kLinkMissAlreadyWritten && misses.count == 1, "and not again in the same level");
	Check(LinkMisses_Note(&misses, second) == kLinkMissWrite && misses.count == 2, "another name is written too");

	for (int i = 2; i < kLinkMissesMostNames; i++)
	{
		char name[kLinkNameLength];
		NameFrom(name, 2000 + i);

		if (LinkMisses_Note(&misses, name) != kLinkMissWrite)
		{
			Check(false, "a name that fits was refused");
			break;
		}
	}

	char oneTooMany[kLinkNameLength], twoTooMany[kLinkNameLength];
	NameFrom(oneTooMany, 9001);
	NameFrom(twoTooMany, 9002);
	Check(misses.count == kLinkMissesMostNames && kLinkMissesMostNames == 64, "it holds 64 names a level");
	Check(LinkMisses_Note(&misses, oneTooMany) == kLinkMissFirstOneTooMany && misses.count == kLinkMissesMostNames,
		"the first name it has no room for says so, once, and isn't kept");
	Check(LinkMisses_Note(&misses, twoTooMany) == kLinkMissTooMany, "and the next isn't written at all");
	Check(LinkMisses_Note(&misses, first) == kLinkMissAlreadyWritten, "a name it holds is still known once it's full");

	LinkMisses_Forget(&misses);
	Check(misses.count == 0 && !misses.saidItIsFull && LinkMisses_Note(&misses, first) == kLinkMissWrite, "the next level starts again, and writes it again");
}

static void TestTheWords()
{
	puts("The words:");

	char const source[kLinkNameLength] = { 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 };
	char const target[kLinkNameLength] = { 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, static_cast<char>(0xff), 0x10, 0x00 };

	Check(LinkMisses_NameAsText(target) == "0a0b0c0d0eff1000", "a name is 16 hex digits, its bytes in the order they're kept");

	std::string const placed = LinkMisses_Source("a switch", source, true, 12345, -4000, 56700);
	Check(placed == "a switch (id 1a2b3c4d5e6f7000) at (12.3, -4.0, 56.7)", "what a link is from says its kind, its id and where it is, in metres");
	Check(LinkMisses_Source("a lift", source, false, 1, 2, 3) == "a lift (id 1a2b3c4d5e6f7000)", "and leaves out a place it hasn't got");

	Check(LinkMisses_AtLoadLine(placed, target) == "links: a switch (id 1a2b3c4d5e6f7000) at (12.3, -4.0, 56.7) aims at 0a0b0c0d0eff1000, and nothing in this level has that name",
		"the line when the level's links are made");
	Check(LinkMisses_OnRequestLine(placed, target) == "links: a switch (id 1a2b3c4d5e6f7000) at (12.3, -4.0, 56.7) went on or off, and its target 0a0b0c0d0eff1000 isn't in this level",
		"the line when a switch goes on or off");
	Check(LinkMisses_TooManyLine(false) == "links: more than 64 names in this level find nothing, so no more are written until the next level", "the line when the list is full");
	Check(LinkMisses_TooManyLine(true) == "links: more than 64 names in this level find nothing when a switch goes on or off, so no more are written until the next level",
		"and the other list's");
	Check(LinkMisses_NpcsNotMadeHereLine(12) == "links: 12 links aim at placed NPCs this machine hasn't made, so they find nothing, as they should: a machine that joined a network game makes none, and some are only made at another difficulty",
		"the one line for links to placed NPCs this machine hasn't made");
	Check(LinkMisses_NpcsNotMadeHereLine(1) == "links: 1 link aims at placed NPCs this machine hasn't made, so it finds nothing, as it should: a machine that joined a network game makes none, and some are only made at another difficulty",
		"and for one");
}

static void TestTheGameUsesIt()
{
	puts("Where the game uses it, by reading its files:");

	std::string const types = ReadSourceFile("3dc/avp/bh_types.cpp");
	Check(Has(types, "static_assert(kLinkNameLength == SB_NAME_LENGTH,"), "a name in the list is as long as a strategy block's");

	std::string const find = Between(types, "static STRATEGYBLOCK *FindLinkTarget(", "\n}\n");
	Check(ComesBefore(find, "STRATEGYBLOCK *const found = FindSBWithName(name);", "if (!found)\n\t{\n\t\tSayLinkMissing(&linkMissesAtLoad, false, source, what, name);"),
		"a link is looked up as before, and one that finds nothing is said, from the list for when the level's links are made");

	std::string const say = Between(types, "static void SayLinkMissing(", "\n}\n");
	Check(ComesBefore(say, "if (LinkMisses_IsNoName(name))\n\t{\n\t\treturn;", "switch (LinkMisses_Note(misses, name))"), "a link with no name says nothing");
	Check(ComesBefore(say, "if (LevelNameIsAPlacedNpc(name))\n\t{\n\t\tif (!onRequest)\n\t\t{\n\t\t\tlinksToNpcsNotMadeHere++;\n\t\t}\n\n\t\treturn;",
		"switch (LinkMisses_Note(misses, name))"), "a link to a placed NPC this machine hasn't made is counted, once, and not written");
	Check(Has(say, "LogString(onRequest ? LinkMisses_OnRequestLine(from, name) : LinkMisses_AtLoadLine(from, name));") && Has(say, "case kLinkMissFirstOneTooMany:\n\t\t\tLogString(LinkMisses_TooManyLine(onRequest));"),
		"a new name is written, and so is a full list, once");

	std::string const assign = Between(types, "void AssignAllSBNames()\n{", "\n}\n");
	// 21 since #135 took out a proximity door's lock target and the xenoborg morph room, neither of which could ever be made
	Check(Count(assign, "FindLinkTarget(sbptr, ") == 21, "every one of the 21 links a level makes is looked up that way (" + std::to_string(Count(assign, "FindLinkTarget(sbptr, ")) + " there)");
	Check(Count(assign, "FindSBWithName(") == 1 && Has(assign, "my_sb = FindSBWithName(curr_stn->my_sb_name);"),
		"and the only one that isn't is a lift's own module");
	Check(Has(assign, "if (linksToNpcsNotMadeHere > 0)\n\t{\n\t\tLogString(LinkMisses_NpcsNotMadeHereLine(linksToNpcsNotMadeHere));"), "the links to NPCs not made here are said in one line");

	std::string const ends = Between(types, "void LinkMisses_LevelEnds(void)", "\n}\n");
	Check(Has(ends, "LinkMisses_Forget(&linkMissesAtLoad);") && Has(ends, "LinkMisses_Forget(&linkMissesOnRequest);") && Has(ends, "linksToNpcsNotMadeHere = 0;"),
		"both lists and the count are emptied for the next level");
	Check(Has(Between(ReadSourceFile("3dc/avp/strategy_def.cpp"), "void DestroyAllStrategyBlocks(void)", "void AssignNewSBName("), "LinkMisses_LevelEnds();"),
		"and that happens whenever every block is destroyed, at a level's end and at a restart");

	std::string const setup = Between(ReadSourceFile("3dc/avp/win95/object_setup.cpp"), "bool LevelNameIsAPlacedNpc(const char *name)", "\n}\n");
	Check(Has(setup, "for(LIF<BehaviourBlockData*> blif(&Behav_List);!blif.done();blif.next())")
		&& Has(setup, "if (BehaviourIsAnNpcTheHostRuns(bbd->sb_type) && memcmp(&bbd->id, name, sizeof(bbd->id)) == 0)"),
		"a placed NPC is one in the level's own list, made here or not, with the whole name compared");

	std::string const binary = Between(ReadSourceFile("3dc/avp/bh_binary_switch.cpp"), "void BinarySwitchBehaveFun(STRATEGYBLOCK* sbptr)", "\n}\n");
	Check(ComesBefore(binary, "BOOL const wasOn = bs_bhv->state;", "switch(bs_bhv->bs_mode)")
		&& ComesBefore(binary, "if (bs_bhv->state != wasOn)\n\t{\n\t\tSayTargetsNotThere(sbptr, bs_bhv);\n\t}", "\n\tbs_bhv->request = I_no_request;"),
		"a switch that goes on or off says which targets weren't there");
	std::string const notThere = Between(ReadSourceFile("3dc/avp/bh_binary_switch.cpp"), "static void SayTargetsNotThere(", "\n}\n");
	Check(Has(notThere, "if (!bs_bhv->bs_targets[i])\n\t\t{\n\t\t\tLinkMisses_SwitchWentOnOrOff(sbptr, bs_bhv->target_names[i].name);"),
		"those are the ones that found nothing, named by the level's name for them");

	std::string const link = Between(ReadSourceFile("3dc/avp/bh_link_switch.cpp"), "void LinkSwitchBehaveFun(STRATEGYBLOCK* sbptr)", "\n}\n");
	Check(ComesBefore(link, "BOOL const wasOn = ls_bhv->system_state;", "if (ls_bhv->system_state)\n\t{\n\t\tif (!check_link_switch_states(ls_bhv))")
		&& Has(link, "if (ls_bhv->system_state != wasOn)\n\t{\n\t\tfor(i=0;i<ls_bhv->num_targets;i++)\n\t\t{\n\t\t\tif (!ls_bhv->ls_targets[i].sbptr)\n\t\t\t{\n\t\t\t\tLinkMisses_SwitchWentOnOrOff(sbptr, ls_bhv->ls_targets[i].name);"),
		"and so does a link switch");
}

int main()
{
	TestTheList();
	TestTheWords();
	TestTheGameUsesIt();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
