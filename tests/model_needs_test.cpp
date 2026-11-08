// A test of the check that refuses a model with parts missing.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// An animated model is a tree of named parts, built from a file. The game asks for some parts by name, and a damaged
// file can leave them out. 3dc/avp/model_needs.cpp holds a list of the parts the game can't do without, and the game
// holds each model against that list as it's built.
//
// This test builds small made-up trees in memory and hands them to the game's real check. It reads no game file. It
// can't prove the list matches the game's own models, because the game's files aren't in this repository, so that is
// checked separately (see the notes in model_needs.cpp).
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file and 3dc/avp/model_needs.cpp, with the settings in tests/model_needs_test.args. The
// program prints a line for each check, and gives back a non-zero result if any check failed.

#include <stdio.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>

#include "3dc.h"
#include "hmodel.h"
#include "model_needs.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("%s: %s\n", passed ? "ok    " : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

// Makes one part of a made-up model. A real model is built by Global_Hierarchy_Store::build_hierarchy, and the check only
// reads the name and the list of children, which ends with an empty entry.
static SECTION MakePart(const char *name, SECTION **children)
{
	SECTION part;
	memset(&part, 0, sizeof(part));
	part.Section_Name = const_cast<char *>(name);
	part.Children = children;
	return part;
}

static void TestFindingParts()
{
	// pelvis > chest > arm > tip of the gun, with a head beside the arm
	SECTION tip = MakePart("dum flash", NULL);
	SECTION *armChildren[] = { &tip, NULL };
	SECTION arm = MakePart("Rbicep", armChildren);
	SECTION head = MakePart("head", NULL);
	SECTION *chestChildren[] = { &arm, &head, NULL };
	SECTION chest = MakePart("chest", chestChildren);
	SECTION *pelvisChildren[] = { &chest, NULL };
	SECTION pelvis = MakePart("pelvis", pelvisChildren);

	Check(Model_Has_Part(&pelvis, "pelvis"), "the first part of a model is found");
	Check(Model_Has_Part(&pelvis, "dum flash"), "a part four levels down is found");
	Check(Model_Has_Part(&pelvis, "head"), "a part that isn't the first child is found");
	Check(!Model_Has_Part(&pelvis, "neck"), "a part the model doesn't have isn't found");
	Check(!Model_Has_Part(&pelvis, "Dum Flash"), "capitals count: \"Dum Flash\" isn't \"dum flash\"");
	Check(!Model_Has_Part(&pelvis, "dum"), "part of a name isn't a match");
	Check(!Model_Has_Part(&arm, "head"), "a branch only knows its own parts");

	// a list of children can be missing altogether, or there and empty
	SECTION *noChildren[] = { NULL };
	SECTION leaf = MakePart("leaf", noChildren);
	Check(Model_Has_Part(&leaf, "leaf") && !Model_Has_Part(&leaf, "other"), "an empty list of children is safe");
}

static void TestOneRow()
{
	// the Queen's row is the longest, and the only model of her file
	SECTION leftFoot = MakePart("left foot", NULL);
	SECTION rightFoot = MakePart("right foot", NULL);
	SECTION leftHand = MakePart("left hand", NULL);
	SECTION rightHand = MakePart("right hand", NULL);
	SECTION finger = MakePart("rrrr fing", NULL);

	SECTION *all[] = { &leftFoot, &rightFoot, &leftHand, &rightHand, &finger, NULL };
	SECTION whole = MakePart("pelvis", all);
	Check(Model_First_Missing_Part(&whole, "queen", "Template") == NULL, "a model with every part passes");

	SECTION *noFinger[] = { &leftFoot, &rightFoot, &leftHand, &rightHand, NULL };
	SECTION lacksFinger = MakePart("pelvis", noFinger);
	const char *missing = Model_First_Missing_Part(&lacksFinger, "queen", "Template");
	Check(missing && strcmp(missing, "rrrr fing") == 0, "a missing part is named");

	SECTION *nothing[] = { NULL };
	SECTION empty = MakePart("pelvis", nothing);
	missing = Model_First_Missing_Part(&empty, "queen", "Template");
	Check(missing && strcmp(missing, "left foot") == 0, "with several missing, the first one is named");

	missing = Model_First_Missing_Part(&lacksFinger, "QUEEN", "Template");
	Check(missing != NULL, "capitals don't count in a file name, as in GetNamedHierarchyFromLibrary");

	Check(Model_First_Missing_Part(&lacksFinger, "queen", "template") == NULL,
		"capitals count in a model name, so this is a model the list doesn't know");
	Check(Model_First_Missing_Part(&empty, "a file nobody asks for", "a model") == NULL,
		"a model the list doesn't mention has nothing to miss");
}

// Every row, not just one: a made-up model with exactly the row's parts passes, and taking any one of them away is
// caught and named. This also catches a badly written row, such as an empty name or a gap in the middle of a list.
static void TestEveryRow()
{
	bool allPassed = true;

	for (int row = 0; row < model_needs_count; row++)
	{
		const MODEL_NEEDS *needs = &model_needs[row];
		int partCount = 0;

		if (!needs->file || !needs->hierarchy || !needs->file[0] || !needs->hierarchy[0])
		{
			printf("row %d has no file or model name\n", row);
			allPassed = false;
			continue;
		}

		while (partCount < MODEL_NEEDS_MOST_PARTS && needs->parts[partCount])
		{
			partCount++;
		}

		for (int gap = partCount; gap < MODEL_NEEDS_MOST_PARTS; gap++)
		{
			if (needs->parts[gap])
			{
				printf("row %d (%s) has a gap in its list\n", row, needs->hierarchy);
				allPassed = false;
			}
		}

		// left == partCount is the whole model, and every smaller number leaves that one part out
		for (int left = 0; left <= partCount; left++)
		{
			SECTION parts[MODEL_NEEDS_MOST_PARTS];
			SECTION *children[MODEL_NEEDS_MOST_PARTS + 1];
			int count = 0;

			for (int part = 0; part < partCount; part++)
			{
				if (part != left)
				{
					parts[count] = MakePart(needs->parts[part], NULL);
					children[count] = &parts[count];
					count++;
				}
			}
			children[count] = NULL;

			SECTION root = MakePart("the first part of a made-up model", children);
			const char *missing = Model_First_Missing_Part(&root, needs->file, needs->hierarchy);

			if (left == partCount ? missing != NULL : (!missing || strcmp(missing, needs->parts[left]) != 0))
			{
				printf("row %d (%s) gave the wrong answer with part %d left out\n", row, needs->hierarchy, left);
				allPassed = false;
			}
		}
	}

	printf("%d rows looked at\n", model_needs_count);
	Check(model_needs_count > 0, "the list isn't empty");
	Check(allPassed, "every row passes when whole, and names any part taken away");
}

// Reads a file of the game as text. /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it.
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

static int CountOf(std::string const &text, char const *words)
{
	int count = 0;

	for (std::string::size_type at = text.find(words); at != std::string::npos; at = text.find(words, at + 1))
	{
		count++;
	}

	return count;
}

// The game's loader brings most of the game with it, so this reads it as text
static void TestTheLoaderAsks()
{
	std::string const loader = ReadSourceFile("3dc/avp/win95/projload.cpp");

	Check(Has(loader, "Model_First_Missing_Part(hd->hierarchy_root, riffname, hd->hierarchy_name)"), "the game asks about each model as it's loaded");
	Check(Has(loader, "Stop_On_Bad_Model(riffname, hd->hierarchy_name, missing_part);"), "and stops if a part is missing");
}

int main()
{
	TestTheLoaderAsks();
	TestFindingParts();
	TestOneRow();
	TestEveryRow();

	if (checksFailed)
	{
		printf("The model needs test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The model needs test passed");
	return 0;
}
