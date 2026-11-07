// Fails on a limit guarded only by an assert. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHY THIS EXISTS
//
// An assert is left out of the game players run. So a limit with only an assert behind it is no limit: a level, a model, a save or another player can go past it. Every one in the game has been read,
// one at a time, and the ones that were real are now real checks. This keeps it that way. It reads all of the game's own source, finds every assert that compares something with what looks like
// a limit, and looks each up in tests/assert_limits_allowed.txt, which says why that one is safe.
//
// A NEW ONE FAILS THIS TEST. Don't just add it to the list. If anything from outside the code can reach it, it needs a real check, and then the assert can stay or go. If nothing can, add it to the
// list with the reason. A row for an assert that's gone fails too, so the list can't fill up with reasons for things that aren't there.
//
// WHAT IT CAN'T SEE: an assert that spreads over more lines than one is looked at on its first line only, and a limit with a name that doesn't look like one isn't found. A limit with no assert at
// all is out of its reach. So this holds the line. It doesn't prove there are no others.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this on its own. It reads the source next to this file, and gives back a non-zero result if anything failed. With --list, it prints every row it finds.

#include <corecrt_io.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

static std::string Trimmed(std::string const &text)
{
	size_t const first = text.find_first_not_of(" \t\r");

	if (first == std::string::npos)
	{
		return "";
	}

	return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

// Words a limit's name tends to have. Upper and lower case both, because the game's code has both.
static bool LooksLikeALimit(std::string const &condition)
{
	static char const *const words[] = { "MAX", "Max", "max", "NUM", "Num", "SIZE", "Size", "size", "COUNT", "Count", "count", "LIMIT", "Limit", "limit", "LENGTH", "Length", "length", "MOST", "Most" };

	for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
	{
		if (condition.find(words[i]) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

// What's inside an assert on this line, if there's one that compares with a limit. An assert of == can't be going past anything, and neither can one with no limit in it.
static bool FindLimitAssert(std::string const &line, std::string *found)
{
	static char const *const names[] = { "GLOBALASSERT", "LOCALASSERT", "assert" };

	for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++)
	{
		size_t at = line.find(names[n]);

		while (at != std::string::npos)
		{
			// It has to be the whole word, followed by its bracket, and not in a comment
			size_t const nameEnd = at + strlen(names[n]);
			size_t bracket = nameEnd;

			while (bracket < line.size() && (line[bracket] == ' ' || line[bracket] == '\t'))
			{
				bracket++;
			}

			bool const wordStarts = (at == 0) || !(isalnum((unsigned char)line[at - 1]) || line[at - 1] == '_');
			size_t const comment = line.find("//");

			if (wordStarts && bracket < line.size() && line[bracket] == '(' && (comment == std::string::npos || comment > at))
			{
				std::string const condition = line.substr(bracket);

				// An arrow has a > in it and isn't a comparison, so look past those
				std::string withoutArrows = condition;
				size_t arrow = withoutArrows.find("->");

				while (arrow != std::string::npos)
				{
					withoutArrows.replace(arrow, 2, "..");
					arrow = withoutArrows.find("->");
				}

				if ((withoutArrows.find('<') != std::string::npos || withoutArrows.find('>') != std::string::npos) && LooksLikeALimit(condition))
				{
					*found = Trimmed(line.substr(at));
					return true;
				}
			}

			at = line.find(names[n], at + 1);
		}
	}

	return false;
}

struct Row
{
	std::string file;
	std::string text;

	bool operator<(Row const &other) const
	{
		return (file != other.file) ? file < other.file : text < other.text;
	}
};

static std::string RepositoryTop()
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	return (tests == std::string::npos) ? std::string() : path.substr(0, tests);
}

static bool EndsWith(std::string const &text, char const *ending)
{
	size_t const length = strlen(ending);
	return text.size() >= length && text.compare(text.size() - length, length, ending) == 0;
}

// Only the game's own code. dependencies/ holds other people's, which has its own asserts and its own scanners.
static void ReadFolder(std::string const &top, std::string const &folder, std::map<Row, int> &rows, int *filesRead)
{
	_finddata_t found;
	intptr_t const search = _findfirst((top + folder + "/*").c_str(), &found);

	if (search == -1)
	{
		return;
	}

	do
	{
		std::string const name = found.name;

		if (name == "." || name == "..")
		{
			continue;
		}

		std::string const path = folder + "/" + name;

		if (found.attrib & _A_SUBDIR)
		{
			// a build leaves its own folders inside win32, and nothing in them is source
			if (name != "Release" && name != "Debug" && name != "Win32")
			{
				ReadFolder(top, path, rows, filesRead);
			}

			continue;
		}

		if (!EndsWith(name, ".cpp") && !EndsWith(name, ".c") && !EndsWith(name, ".h") && !EndsWith(name, ".hpp"))
		{
			continue;
		}

		std::ifstream file((top + path).c_str(), std::ios::binary);
		std::string line;
		(*filesRead)++;

		while (std::getline(file, line))
		{
			std::string text;

			if (FindLimitAssert(line, &text))
			{
				Row const row = { path, text };
				rows[row]++;
			}
		}
	}
	while (_findnext(search, &found) == 0);

	_findclose(search);
}

// Each line of the list is: the file | what the assert says | why it's safe. Blank lines and lines starting # are for people.
static bool ReadAllowed(std::string const &path, std::map<Row, std::string> &allowed, std::vector<std::string> &badLines)
{
	std::ifstream file(path.c_str(), std::ios::binary);

	if (!file)
	{
		return false;
	}

	std::string line;

	while (std::getline(file, line))
	{
		std::string const trimmed = Trimmed(line);

		if (trimmed.empty() || trimmed[0] == '#')
		{
			continue;
		}

		size_t const first = trimmed.find(" | ");
		size_t const last = trimmed.rfind(" | ");

		if (first == std::string::npos || last == first || Trimmed(trimmed.substr(last + 3)).empty())
		{
			badLines.push_back(trimmed);
			continue;
		}

		Row const row = { Trimmed(trimmed.substr(0, first)), Trimmed(trimmed.substr(first + 3, last - first - 3)) };
		allowed[row] = Trimmed(trimmed.substr(last + 3));
	}

	return true;
}

static void CheckTheFinder()
{
	printf("What counts as a limit:\n");

	std::string text;

	Check(FindLimitAssert("\tLOCALASSERT(count < MAX_THINGS);", &text) && text == "LOCALASSERT(count < MAX_THINGS);", "a local assert with a limit counts, and comes back without its indent");
	Check(FindLimitAssert("GLOBALASSERT(x <= maxThings);", &text), "and a global one");
	Check(FindLimitAssert("assert(i < Size);", &text), "and a plain one");
	Check(FindLimitAssert("assert (i < Size);", &text), "with a space before its bracket");
	Check(FindLimitAssert("x = 1; assert(p->count >= 0);", &text) && text == "assert(p->count >= 0);", "anywhere on its line");
	Check(!FindLimitAssert("LOCALASSERT(p->flags);", &text), "an arrow isn't a comparison");
	Check(!FindLimitAssert("LOCALASSERT(p->count);", &text), "even with a limit's name in it");
	Check(FindLimitAssert("LOCALASSERT(p->count < 4);", &text), "and a comparison after an arrow still is one");
	Check(!FindLimitAssert("LOCALASSERT(count == MAX_THINGS);", &text), "being equal can't be going past anything");
	Check(!FindLimitAssert("LOCALASSERT(x < 4);", &text), "a number with no limit's name isn't found, which is what this can't see");
	Check(!FindLimitAssert("// assert(i < Size);", &text), "one in a comment doesn't count");
	Check(!FindLimitAssert("my_assert(i < Size);", &text), "nor does some other word ending the same way");
	Check(!FindLimitAssert("int size = max < 4;", &text), "nor a line with no assert");
}

int main(int argc, char **argv)
{
	std::string const top = RepositoryTop();
	std::map<Row, int> rows;
	int filesRead = 0;

	ReadFolder(top, "3dc", rows, &filesRead);
	ReadFolder(top, "win32", rows, &filesRead);

	if (argc > 1 && strcmp(argv[1], "--list") == 0)
	{
		for (std::map<Row, int>::const_iterator i = rows.begin(); i != rows.end(); ++i)
		{
			printf("%s | %s\n", i->first.file.c_str(), i->first.text.c_str());
		}

		return 0;
	}

	CheckTheFinder();

	printf("The game's own source:\n");

	Check(filesRead > 300, "the game's source is really being read: " + std::to_string(filesRead) + " files");

	std::map<Row, std::string> allowed;
	std::vector<std::string> badLines;
	Check(ReadAllowed(top + "tests/assert_limits_allowed.txt", allowed, badLines), "the list of what's safe is there");
	Check(allowed.size() > 10, "and it has rows in it");

	for (size_t i = 0; i < badLines.size(); i++)
	{
		printf("  this line of the list has no reason: %s\n", badLines[i].c_str());
	}

	Check(badLines.empty(), "every row of the list gives its reason");

	int notListed = 0;

	for (std::map<Row, int>::const_iterator i = rows.begin(); i != rows.end(); ++i)
	{
		if (allowed.find(i->first) == allowed.end())
		{
			printf("  a limit only an assert guards, and nobody has said why it's safe: %s: %s\n", i->first.file.c_str(), i->first.text.c_str());
			notListed++;
		}
	}

	Check(notListed == 0, "every assert on a limit has been read, and is in the list with a reason");

	int gone = 0;

	for (std::map<Row, std::string>::const_iterator i = allowed.begin(); i != allowed.end(); ++i)
	{
		if (rows.find(i->first) == rows.end())
		{
			printf("  the list has a reason for something that isn't there any more: %s: %s\n", i->first.file.c_str(), i->first.text.c_str());
			gone++;
		}
	}

	Check(gone == 0, "and the list has nothing in it that isn't there");

	printf("  %d different asserts on limits read, in %d files\n", (int)rows.size(), filesRead);

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
