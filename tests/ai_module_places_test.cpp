// Tests that the level loader never uses a module's place in the AI unless it's a real place. Written 2026-11-14 by Melanikus for the
// AvP port.
//
// WHY: when the loader sets up the AI (projload.cpp), each module gets a place in AIModuleArray, kept in aimodule_indeces. A module it
// gives no place keeps -1. Used as a place, -1 is the memory just before the array: before AIModuleArray, and before the list of ways
// into each module that's made beside it, which would write there. The level reader refuses the levels known to leave a module at -1
// (a slave module whose master isn't a master: rebeccas_test, section 39). This checks the other half, so that no way to -1, known or
// not, can reach the arrays: every number read from aimodule_indeces is checked by IsARealAIModule before it's used.
//
// HOW: projload.cpp needs the whole game, so this reads its source, as several of these tests do. It finds every place aimodule_indeces
// is read, as opposed to written, and checks the very next thing done with that number is to ask IsARealAIModule. A new read with no
// check fails it. tests/takeouts/ai_module_places_test.json proves each check can fail.

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

// A file of the game's source, found from this test's own place, with its carriage returns taken out so every line ends the same way
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

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// The rest of the line that starts at, from at on
static std::string LineFrom(const std::string &text, std::string::size_type at)
{
	std::string::size_type const end = text.find('\n', at);
	return text.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

int main()
{
	// Every line goes out as soon as it's printed, so a crash still shows how far it got
	setvbuf(stdout, NULL, _IONBF, 0);

	puts("A module's place in the AI, in projload.cpp:");

	std::string const loader = ReadSource("3dc/avp/win95/projload.cpp");
	Check(!loader.empty(), "projload.cpp is there to read");

	// The check itself: place 0 is kept empty, so a real place is 1 or more, and less than the array's size
	Check(Has(loader, "static bool IsARealAIModule(int index)\n{\n\treturn index >= 1 && index < AIModuleArraySize;\n}"),
		"a real place is from 1 up to the array's size, and nothing else is");

	// Every place aimodule_indeces is read. A write is the name, a place in brackets, then "=". Anything else is a read, and the
	// number it gives has to go to IsARealAIModule on the very next line, before it's used for anything.
	std::string const name = "aimodule_indeces[";
	int reads = 0;
	int writes = 0;
	int readsChecked = 0;

	for (std::string::size_type at = loader.find(name); at != std::string::npos; at = loader.find(name, at + 1))
	{
		std::string::size_type const close = loader.find(']', at);

		if (close != std::string::npos && close + 1 < loader.size() && loader[close + 1] == '=' && loader[close + 2] != '=')
		{
			writes++;
			continue;
		}

		reads++;

		// The line it's read on gives the number a name: what comes after "int " or "int const " up to " ="
		std::string const line = LineFrom(loader, loader.rfind('\n', at) + 1);
		std::string::size_type const eq = line.find('=');
		std::string::size_type const nameStart = line.rfind(' ', eq == std::string::npos ? 0 : eq - 1);
		std::string const number = (eq == std::string::npos || nameStart == std::string::npos) ? std::string() : line.substr(nameStart + 1, eq - nameStart - 1);

		std::string::size_type const nextLine = loader.find('\n', at);
		std::string const after = nextLine == std::string::npos ? std::string() : LineFrom(loader, nextLine + 1);

		bool const checked = !number.empty() && after.find("IsARealAIModule(" + number + ")") != std::string::npos;
		readsChecked += checked ? 1 : 0;

		Check(checked, "the place read here is checked on the next line: " + line.substr(line.find_first_not_of('\t')));
	}

	// How many there are now. A new read or write changes these, and then this test has to be looked at again, with its reason.
	Check(reads == 5, "aimodule_indeces is read in 5 places (the path setup, the path lookup, a module's own place, and its links, twice)");
	Check(writes == 4, "and written in 4 (-1 to start with, then a master, its slaves, and a module on its own)");
	Check(readsChecked == reads, "every read is checked");

	// The two link readers don't just check the place: they still leave out the module's own place, as they always did
	{
		std::string const link = "if(IsARealAIModule(adj_ai_module) && adj_ai_module!=this_ai_module_index)";
		std::string::size_type const first = loader.find(link);
		Check(first != std::string::npos && loader.find(link, first + 1) != std::string::npos,
			"both link readers check the other module's place, and still leave out a link to itself");
	}

	printf("%s: %d checks, %d failed\n", failures ? "SOME CHECKS FAILED" : "All checks passed", checks, failures);
	return failures ? 1 : 0;
}
