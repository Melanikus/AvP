// The test of the rule for a custom map's name. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the custom
// maps follow Bobblen's avp-forever, rewritten).
//
// WHAT IT TESTS
//
// 3dc/custom_map_name.h decides which names may be used to build a path, be shown, or be sent to other players. This tries it with
// names that keep to the rule and names that each break one part of it, so that every part is shown to matter.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/custom_map_name_test.args, and the program gives back a non-zero result
// if any check failed. It needs no game files. tests/takeouts/custom_map_name_test.json proves each check can fail.

#include <stdio.h>

#include <string>

#include "custom_map_name.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Safe(const char *name)
{
	return CustomMapName_IsSafe(name, strlen(name) + 1);
}

static void TestNames()
{
	puts("Names that keep to the rule:");

	Check(Safe("Stranded"), "a plain name");
	Check(Safe("My map (v2) - final_1"), "letters, digits, spaces, '_', '-', '(' and ')'");
	Check(Safe("A"), "one letter");
	Check(Safe("123456789012345678901234567890123456789"), "39 letters, the most");
	Check(Safe("CONSOLE") && Safe("COM0") && Safe("COMA") && Safe("LPT") && Safe("NULL"), "names that only look like a device's");

	puts("Names that break it:");

	Check(!CustomMapName_IsSafe(0, 40), "no name at all");
	Check(!CustomMapName_IsSafe("Map", 0), "no room at all");

	char unended[8] = { 'M', 'a', 'p', 'M', 'a', 'p', 'M', 'a' };
	Check(!CustomMapName_IsSafe(unended, sizeof(unended)), "a name that doesn't end within its room");
	Check(!Safe(""), "an empty name");
	Check(!Safe("1234567890123456789012345678901234567890"), "40 letters, one too many");
	Check(!Safe("..") && !Safe("a.b") && !Safe("map."), "any dot");
	Check(!Safe("..\\Maps") && !Safe("a\\b"), "a backslash");
	Check(!Safe("a/b") && !Safe("/abs"), "a slash");
	Check(!Safe("C:x") && !Safe("map:stream"), "a colon");
	Check(!Safe("tab\there") && !Safe("bell\a"), "a control letter");
	Check(!Safe("caf\xe9") && !Safe("\xff"), "a letter outside plain ASCII");
	Check(!Safe("*") && !Safe("map?") && !Safe("a<b") && !Safe("\"q\"") && !Safe("a|b") && !Safe("a%b"), "the other letters Windows or a format would read");
	Check(!Safe(" Map"), "a space at the start");
	Check(!Safe("Map "), "a space at the end");
	Check(!Safe("CON") && !Safe("con") && !Safe("Prn") && !Safe("aux") && !Safe("NUL"), "the four device names, in any case");
	Check(!Safe("COM1") && !Safe("com9") && !Safe("LPT1") && !Safe("lpt9"), "COM1 to COM9 and LPT1 to LPT9");
}

// Every byte past outRoom must be left as it was: a name is never written beyond the room it's given
static bool leftItsRoomAlone = true;

static bool FromFile(const char *fileName, std::string *name, size_t outRoom = 40)
{
	char out[64];

	memset(out, 'x', sizeof(out));

	bool const good = CustomMapName_FromFileName(fileName, strlen(fileName) + 1, out, outRoom);

	for (size_t i = outRoom; i < sizeof(out); i++)
	{
		leftItsRoomAlone = leftItsRoomAlone && out[i] == 'x';
	}

	out[sizeof(out) - 1] = 0;
	*name = out;

	return good;
}

static void TestFileNames()
{
	puts("A file name found in the folder:");

	std::string name;

	Check(FromFile("Stranded.rif", &name) && name == "Stranded", "Name.rif gives Name");
	Check(FromFile("Stranded.RIF", &name) && name == "Stranded", "in any case");
	Check(!FromFile("Stranded.rifx", &name) && name.empty(), "a file that doesn't end in .rif is left out, and nothing is written");
	Check(!FromFile("Stranded.rof", &name) && name.empty(), "nor one ending in another three letters");
	Check(!FromFile("Strandedxrif", &name) && name.empty(), "nor one ending in rif with no dot before it");
	Check(!FromFile(".rif", &name) && name.empty(), "nor .rif with no name before it");
	Check(!FromFile("rif", &name) && name.empty(), "nor a name too short to end in .rif");
	Check(!FromFile("a.b.rif", &name) && name.empty(), "a name that breaks the rule before .rif is left out, and nothing is kept of it");
	Check(!FromFile("1234567890123456789012345678901234567890.rif", &name) && name.empty(), "a name too long for the rule");
	Check(!FromFile("Stranded.rif", &name, 8) && name.empty(), "a name with no room for it and its end marker");
	Check(FromFile("Strande.rif", &name, 8) && name == "Strande", "and one that just fits");
	Check(leftItsRoomAlone, "and nothing was ever written past the room given for the name");

	char unended[12] = { 'a', '.', 'r', 'i', 'f', 'a', '.', 'r', 'i', 'f', 'x', 'y' };
	char out[40];
	Check(!CustomMapName_FromFileName(unended, sizeof(unended), out, sizeof(out)) && out[0] == 0, "a file name that doesn't end within its room");
	Check(!CustomMapName_FromFileName("Map.rif", 8, 0, 40), "nowhere to put the name");
}

int main()
{
	TestNames();
	TestFileNames();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
