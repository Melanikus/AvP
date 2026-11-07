// The test of the checks on what a network message says about a platform lift. Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// 3dc/avp/lift_checks.h holds the checks, with nothing but plain numbers in them, so this test tries them with good and bad values.
// The code that receives the message brings most of the game with it, so this reads it as text instead: it has to ask before it
// uses the state, and say so in the log. tests/takeouts/lift_checks_test.json proves each check can fail.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/lift_checks_test.args, and the program gives back a non-zero result if
// any check failed. It needs no game files. /FC in its settings is how the source files are found.

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "lift_checks.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One source file, with Windows line ends made plain so the text below can say what it means.
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
	std::string plain;

	for (char const letter : text)
	{
		if (letter != '\r')
		{
			plain += letter;
		}
	}

	return plain;
}

// The text from one mark up to the next, or nothing if either isn't there.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		return "";
	}

	std::string::size_type const end = text.find(to, start + from.size());

	return end == std::string::npos ? "" : text.substr(start, end - start);
}

static bool Has(std::string const &text, std::string const &part)
{
	return text.find(part) != std::string::npos;
}

// Is the first one there, the second one there too, and the first earlier?
static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

static void TestTheStateNumbers()
{
	puts("A lift's state as a number:");

	// Four states, numbered 0 to 3, as PLATFORMLIFT_STATES has them
	Check(Lift_StateIsKnown(0, 4) && Lift_StateIsKnown(3, 4), "the first and the last state are kept");
	Check(!Lift_StateIsKnown(4, 4), "one past the last is refused");
	Check(!Lift_StateIsKnown(255, 4), "and the largest a byte can hold");
	Check(!Lift_StateIsKnown(-1, 4), "and one below the first, in case a byte is ever read as signed");
}

static void TestTheMessage()
{
	puts("The host's message about a lift's state, on another machine:");

	std::string const lift = ReadSourceFile("3dc/avp/bh_platform_lift.h");
	std::string const received = Between(ReadSourceFile("3dc/avp/win95/pldnet.cpp"), "static void ProcessNetMsg_PlatformLiftState(NETMESSAGE_PLATFORMLIFTSTATE *msgPtr)\n{", "static void ProcessNetMsg_RequestPlatformLiftActivate(");
	std::string const check = "if (!Lift_StateIsKnown(msgPtr->state, (int)PLBS_GoingDown + 1))";

	Check(Has(lift, "\tPLBS_GoingDown,\n} PLATFORMLIFT_STATES;"), "the last of a lift's states is still PLBS_GoingDown, which the check counts to");
	Check(ComesBefore(received, check, "NetworkPeerChangePlatformLiftState(objectPtr, (PLATFORMLIFT_STATES)(msgPtr->state));"), "the state is checked before the car is given it");
	Check(Has(received, check + "\n\t{\n\t\tNoteRefusedLiftState();\n\t\treturn;"), "and one that isn't a lift's is refused");
	Check(Has(ReadSourceFile("3dc/avp/win95/pldnet.cpp"), "LogString(\"net: refused: a lift's message named a state lifts don't have. Any more aren't written down\");"), "and the first refusal says so in the log, in those words");
}

int main()
{
	TestTheStateNumbers();
	TestTheMessage();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
