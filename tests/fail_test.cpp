// Tests for fail(), which writes a message to the log and stops the game when one of its checks on itself finds something can't be
// put right (issue #87: the game's own, written fresh, with credit to Jamie Lokier, whose 1995 fail() did this job). The first part
// tries Fail_Words, which puts the message together. The second part reads the game's files, to see that every message a caller can
// send has as many values as the call gives it, and that his files are gone. The last part calls fail() itself, and checks the log and
// that it never comes back. Written 2026-11-08 by Melanikus for the AvP port.
#include "fail.h"

#include <ctype.h>
#include <fstream>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

// The game's log, as fail() sees it: here it only keeps the last line it was given
static std::string lastErrorLogged;
static int errorsLogged = 0;

void LogErrorString(const std::string &errorString)
{
	lastErrorLogged = errorString;
	errorsLogged++;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here may have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
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

// Fail_Words takes its values as a va_list, the way fail() hands them on
static size_t Words(char *words, size_t size, const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	size_t const wrote = Fail_Words(words, size, format, arguments);
	va_end(arguments);

	return wrote;
}

static void TestTheWords()
{
	char words[200];

	size_t wrote = Words(words, sizeof(words), "%s: Tried to get entry %d from %d-entry list.\n", "List<int>", 5, 3);
	Check(strcmp(words, "List<int>: Tried to get entry 5 from 3-entry list.\n") == 0, "the message is put together from its format and values, as printf does");
	Check(wrote == strlen(words), "and it says how long it is");

	wrote = Words(words, sizeof(words), 0);
	Check(strcmp(words, "something went wrong, and its message is missing") == 0 && wrote == strlen(words), "a missing format still gives a message");

	// Too long: cut short to fit, with its zero, and nothing past the room it was given
	char small[16];
	memset(small, 'x', sizeof(small));
	wrote = Words(small, 10, "%s", "a message that is much too long");
	Check(wrote == 9 && small[9] == '\0' && strncmp(small, "a message", 9) == 0, "a message too long is cut short to fit, with its zero");
	Check(small[10] == 'x' && small[15] == 'x', "and nothing is written past the room it was given");

	wrote = Words(small, 10, "%s", "123456789");
	Check(wrote == 9 && strcmp(small, "123456789") == 0, "a message that fits exactly isn't cut");

	memset(small, 'x', sizeof(small));
	wrote = Words(small, 1, "%s", "anything");
	Check(wrote == 0 && small[0] == '\0' && small[1] == 'x', "with room for only its zero, the message is empty");

	memset(small, 'x', sizeof(small));
	wrote = Words(small, 0, "%s", "anything");
	Check(wrote == 0 && small[0] == 'x', "with no room at all, nothing is written");

	Check(Words(0, 10, "%s", "anything") == 0, "and with nowhere to write it, nothing is either");

	// A type's name as long as any the game has, in the longest message, still fits
	std::string const longName(400, 'T');
	char big[kFailWordsLongest];
	wrote = Words(big, sizeof(big), "%s: Tried to take an Iterator past the sentinel of a List.\n", longName.c_str());
	Check(wrote < kFailWordsLongest && big[wrote] == '\0' && strstr(big, "past the sentinel") != 0, "the longest message, with a long type name, fits whole");
}

// How many values a printf format asks for: each % that isn't a %%
static int ValuesAskedFor(const std::string &format)
{
	int count = 0;

	for (size_t i = 0; i < format.size(); i++)
	{
		if (format[i] == '%')
		{
			if (i + 1 < format.size() && format[i + 1] == '%')
			{
				i++;
			}
			else
			{
				count++;
			}
		}
	}

	return count;
}

// The messages a file names, as `char const * name = "words";` lines (the game's own way of writing them)
static std::map<std::string, std::string> Messages(const std::string &text)
{
	std::map<std::string, std::string> found;
	std::istringstream lines(text);
	std::string line;

	while (std::getline(lines, line))
	{
		std::string::size_type const star = line.find("char const");
		std::string::size_type const equals = line.find('=');
		std::string::size_type const open = line.find('"');
		std::string::size_type const close = line.rfind('"');

		if (star == std::string::npos || equals == std::string::npos || open == std::string::npos || close <= open || line.find("extern") != std::string::npos)
		{
			continue;
		}

		std::string name = line.substr(star + 10, equals - star - 10);
		std::string::size_type const first = name.find_first_not_of(" \t*");
		std::string::size_type const last = name.find_last_not_of(" \t");
		name = (first == std::string::npos) ? "" : name.substr(first, last - first + 1);
		found[name] = line.substr(open + 1, close - open - 1);
	}

	return found;
}

// Every call of fail() in a file: the name of its message, and how many values the call gives after it
struct Call
{
	std::string message;
	int values;
};

static std::vector<Call> Calls(const std::string &text)
{
	std::vector<Call> found;

	for (std::string::size_type at = text.find("fail("); at != std::string::npos; at = text.find("fail(", at + 1))
	{
		// only the word fail itself, not the end of another name such as list_fail(
		if (at > 0 && (isalnum((unsigned char)text[at - 1]) || text[at - 1] == '_'))
		{
			continue;
		}

		std::string::size_type const end = text.find(')', at);

		if (end == std::string::npos)
		{
			continue;
		}

		// the call's own brackets: i+1 and n_entries have none, so the first ) ends it
		std::string inside = text.substr(at + 5, end - at - 5);
		Call call;
		std::string::size_type const comma = inside.find(',');
		call.message = inside.substr(0, comma);
		call.message.erase(0, call.message.find_first_not_of(" \t"));
		call.message.erase(call.message.find_last_not_of(" \t") + 1);
		call.values = 0;

		for (char const c : inside)
		{
			call.values += (c == ',') ? 1 : 0;
		}

		found.push_back(call);
	}

	return found;
}

static void CheckCallsMatch(const char *callingFile, const char *messageFile, int expectedCalls)
{
	std::vector<Call> const calls = Calls(ReadSourceFile(callingFile));
	std::map<std::string, std::string> const messages = Messages(ReadSourceFile(messageFile));
	bool allKnown = true, allMatch = true;

	for (const Call &call : calls)
	{
		auto const found = messages.find(call.message);

		if (found == messages.end())
		{
			allKnown = false;
			printf("        %s calls fail(%s), which %s doesn't hold\n", callingFile, call.message.c_str(), messageFile);
			continue;
		}

		if (ValuesAskedFor(found->second) != call.values)
		{
			allMatch = false;
			printf("        %s: %s asks for %d values and is given %d\n", callingFile, call.message.c_str(), ValuesAskedFor(found->second), call.values);
		}
	}

	Check((int)calls.size() == expectedCalls, std::string(callingFile) + " has its " + std::to_string(expectedCalls) + " calls of fail()");
	Check(allKnown, std::string("every message they send is one ") + messageFile + " holds");
	Check(allMatch, "and each is given exactly as many values as its words ask for, so none reads a value that isn't there");
}

static void TestTheCallers()
{
	CheckCallsMatch("3dc/win95/list_tem.hpp", "3dc/win95/list_tem.cpp", 39);
	CheckCallsMatch("3dc/avp/support/refobj.hpp", "3dc/avp/support/refobj.cpp", 3);
	CheckCallsMatch("3dc/avp/support/reflist.hpp", "3dc/avp/support/reflist.cpp", 1);

	// A release build leaves every one of these checks out
	std::string const lists = ReadSourceFile("3dc/win95/list_tem.hpp");
	Check(Has(lists, "#ifdef NDEBUG\n\t#define fail if (0)") && Has(lists, "#else\n\t#include \"fail.h\""), "a release build turns the list's checks into nothing, and only a build for finding bugs reads fail.h");
	Check(Has(ReadSourceFile("3dc/avp/support/refobj.hpp"), "\t#include \"fail.h\"\n"), "the shared objects read the game's own fail.h");

	// Jamie Lokier's files are gone, and the game builds its own
	Check(ReadSourceFile("3dc/win95/fail.cpp").empty() && ReadSourceFile("3dc/win95/fail.h").empty(), "Jamie Lokier's fail.cpp and fail.h are gone");
	std::string const project = ReadSourceFile("win32/VS2026/AvP.vcxproj");
	Check(Has(project, "3dc\\fail.cpp") && !Has(project, "3dc\\win95\\fail.cpp"), "the game builds its own fail.cpp in their place");
	Check(Has(ReadSourceFile("win32/VS2026/AvP.vcxproj.filters"), "3dc\\fail.cpp"), "and the project's list of files shows it");

	std::string const ours = ReadSourceFile("3dc/fail.cpp");
	Check(Has(ours, "Credit: this does the job of Jamie Lokier's") && Has(ReadSourceFile("3dc/fail.h"), "Credit: this does the job of Jamie Lokier's fail()"), "both new files give him the credit");
	Check(Has(ours, "LogErrorString(std::string(\"the game stopped itself: \") + words);") && Has(ours, "exit(EXIT_FAILURE);"), "fail() writes its message as an error, and ends the game with the code for failure");
}

// ---- fail() itself, which ends this program, as it ends the game ----

static void AfterTheFail()
{
	puts("3. fail() itself");
	Check(errorsLogged == 1, "fail() wrote one error to the log");
	Check(lastErrorLogged == "the game stopped itself: List<int>: Tried to delete a nonexistent List entry", "the message, put together, with no line break of its own at the end");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	// fail() is ending this program with EXIT_FAILURE, as it should. The answer this test gives is its checks' instead.
	_exit(checksFailed ? 1 : 0);
}

int main()
{
	puts("1. the message");
	TestTheWords();
	puts("2. the callers");
	TestTheCallers();

	atexit(AfterTheFail);
	fail("%s: Tried to delete a nonexistent List entry\n", "List<int>");

	// fail() never comes back. If it does, that's a failure, and AfterTheFail, which still runs as this program ends, answers 1 for it.
	Check(false, "fail() came back, when it should end the program");
	printf("SOME CHECKS FAILED\n");
	return 1;
}
