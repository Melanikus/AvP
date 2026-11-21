// The test of a self destruct's countdown from a level file (3dc/self_destruct_time.h, used by object_setup.cpp add_selfdestruct). Written 2026-11-16 by Melanikus for the AvP port.
//
// A level file gives the countdown in whole seconds and the game shifts it into 16.16 fixed point. Shifted as it is, 40000 seconds overflows a signed int (a negative countdown, which runs
// out at once) and -1 is undefined. CHECKS: the file's number is held to 0 to 32767 before it's turned (a hostile 40000 and -1, and the ends of an int); a number in the range is turned as
// before; and object_setup.cpp turns the file's number only through the hold, and says so in the level's log when it holds one. The program gives back a non-zero result if any check failed.

#include <limits.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "self_destruct_time.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

	if (!good)
	{
		checksFailed++;
	}
}

static std::string ReadSourceFile(const char *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");
	std::string const root = tests == std::string::npos ? std::string() : path.substr(0, tests);
	std::ifstream file(root + name, std::ios::binary);
	std::stringstream text;

	text << file.rdbuf();

	std::string out = text.str();
	std::string::size_type at;

	while ((at = out.find("\r\n")) != std::string::npos)
	{
		out.erase(at, 1);
	}

	return out;
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

int main()
{
	puts("A countdown the game can hold is turned as before:");
	Check(SelfDestruct_FixedTimer(0) == 0, "0 seconds is 0");
	Check(SelfDestruct_FixedTimer(60) == 60 * 65536, "60 seconds is 60 x 65536");
	Check(SelfDestruct_FixedTimer(kSelfDestructMostSeconds) == 32767 * 65536 && 32767 * 65536 > 0, "32767 seconds, the most, is 32767 x 65536, still positive");
	Check(SelfDestruct_SecondsFit(0) && SelfDestruct_SecondsFit(60) && SelfDestruct_SecondsFit(32767), "and each fits as it is");

	puts("A hostile countdown is held to 0 to 32767 seconds before it's turned:");
	Check(!SelfDestruct_SecondsFit(40000) && SelfDestruct_FixedTimer(40000) == 32767 * 65536, "40000 seconds (which shifted as it is overflows to a negative countdown) is held to 32767");
	Check(!SelfDestruct_SecondsFit(32768) && SelfDestruct_FixedTimer(32768) == 32767 * 65536, "32768, the first that overflows, is held to 32767");
	Check(!SelfDestruct_SecondsFit(-1) && SelfDestruct_FixedTimer(-1) == 0, "-1 (shifted, undefined) is held to 0, as the old editor kept a negative answer");
	Check(SelfDestruct_FixedTimer(INT_MAX) == 32767 * 65536 && SelfDestruct_FixedTimer(INT_MIN) == 0, "and so are the ends of an int");

	puts("The game turns the file's number only through the hold:");
	std::string const setup = ReadSourceFile("3dc/avp/win95/object_setup.cpp");
	std::string::size_type const start = setup.find("static void add_selfdestruct(");
	std::string::size_type const end = start == std::string::npos ? start : setup.find("\n}\n", start);
	std::string const add = start == std::string::npos || end == std::string::npos ? std::string() : setup.substr(start, end - start);

	Check(!add.empty(), "object_setup.cpp still has add_selfdestruct");
	Check(Has(add, "sdtt->timer=SelfDestruct_FixedTimer(sds->timer);") && !Has(add, "<<"), "add_selfdestruct turns the countdown with SelfDestruct_FixedTimer, and shifts nothing itself");
	Check(Has(add, "if (!SelfDestruct_SecondsFit(sds->timer))") && Has(add, "LogErrorString(\"level file gives a self destruct a countdown outside 0 to 32767 seconds"), "and a held number is written in the level's log");

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("all self destruct countdown checks passed");
	return 0;
}
