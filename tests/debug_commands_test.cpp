// The test of the console's debug and cheat commands in a beta. Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// - beta_build.h: which version texts are a beta, and that nothing else is.
// - console_value.h: a typed number kept in range before it's worked out. It tries the numbers that went wrong before: TIMESCALE 40000,
//   the largest and smallest numbers there are, a decimal too big for any number, endless ones and "not a number".
// - The game's own files, read as text: the 20 debug and cheat commands are asked for in a beta without the flag that stops saving; the
//   flag is still only set by -debug; the three alien bots and TIMESCALE are refused in a network game; and the variables and commands
//   use the safe reading.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/debug_commands_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/debug_commands_test.json proves each check can fail.

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

#include "beta_build.h"
#include "console_value.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's source, by its place from the top of the repository, with Windows' line ends taken out
static std::string ReadSourceFile(const char *name)
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

static int TimesIn(const std::string &text, const char *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
}

static void TestWhatIsABeta()
{
	puts("Which builds are a beta:");

	Check(BetaBuild_VersionIsABeta("betarelease20261107"), "betarelease and eight digits is a beta");
	Check(!BetaBuild_VersionIsABeta("1.2.3"), "a numbered release isn't");
	Check(!BetaBuild_VersionIsABeta("a test build, not a release"), "a pull request's test build isn't");
	Check(!BetaBuild_VersionIsABeta("not known (not built by tools/build-and-package.ps1)"), "and a build made any other way isn't");
	Check(!BetaBuild_VersionIsABeta("betarelease2026110"), "seven digits isn't");
	Check(!BetaBuild_VersionIsABeta("betarelease202611070"), "and nine isn't");
	Check(!BetaBuild_VersionIsABeta("betarelease2026110x"), "a letter among the digits isn't");
	Check(!BetaBuild_VersionIsABeta("Betarelease20261107"), "nor a capital where there's none");
	Check(!BetaBuild_VersionIsABeta("betarelease"), "the word with no day isn't");
	Check(!BetaBuild_VersionIsABeta("beta"), "and a text shorter than the word stops at its end");
	Check(!BetaBuild_VersionIsABeta(""), "an empty version isn't");
	Check(!BetaBuild_VersionIsABeta(0), "and nor is no version at all");
}

static void TestTypedWholeNumbers()
{
	puts("A typed whole number, kept in range before the sum:");

	Check(ConsoleValue_ToInt(LLONG_MAX) == INT_MAX && ConsoleValue_ToInt(LLONG_MIN) == INT_MIN, "digits that go on too long become the largest or smallest int");
	Check(ConsoleValue_ToInt(5) == 5 && ConsoleValue_ToInt(-5) == -5, "and an ordinary number stays as it is");

	// TIMESCALE: 655 to 4 * 65536
	int const timeMin = 655, timeMax = 65536 * 4;

	Check(ConsoleValue_WholeAsFixed(40000, timeMin, timeMax) == timeMax, "TIMESCALE 40000 is the fastest it goes, where the sum used to go wrong");
	Check(ConsoleValue_WholeAsFixed(INT_MAX, timeMin, timeMax) == timeMax, "and so is the largest int");
	Check(ConsoleValue_WholeAsFixed(INT_MIN, timeMin, timeMax) == timeMin, "and the smallest int is the slowest");
	Check(ConsoleValue_WholeAsFixed(1, timeMin, timeMax) == 65536, "1 is normal speed");
	Check(ConsoleValue_WholeAsFixed(0, timeMin, timeMax) == timeMin && ConsoleValue_WholeAsFixed(-3, timeMin, timeMax) == timeMin, "0 and below are the slowest");

	Check(ConsoleValue_InRange(300, 0, 255) == 255 && ConsoleValue_InRange(-1, 0, 255) == 0 && ConsoleValue_InRange(128, 0, 255) == 128, "a whole-number variable stays between its ends");
}

static void TestTypedDecimals()
{
	puts("A typed decimal, kept in range while it's still a decimal:");

	int value = 12345;

	Check(!ConsoleValue_FromDecimal(nan(""), 65536.0, 655, 65536 * 4, &value) && value == 12345, "not a number is refused, and the value stays as it was");
	Check(ConsoleValue_FromDecimal(INFINITY, 65536.0, 655, 65536 * 4, &value) && value == 65536 * 4, "an endless number is the largest value");
	Check(ConsoleValue_FromDecimal(-INFINITY, 65536.0, 655, 65536 * 4, &value) && value == 655, "and an endlessly small one the smallest");
	Check(ConsoleValue_FromDecimal(1e30, 65536.0, 655, 65536 * 4, &value) && value == 65536 * 4, "a decimal too big for any number is the largest value");
	Check(ConsoleValue_FromDecimal(-1e30, 1.0, 0, 255, &value) && value == 0, "and too small is the smallest");
	Check(ConsoleValue_FromDecimal(0.5, 65536.0, 655, 65536 * 4, &value) && value == 32768, "0.5 is half speed");
	Check(ConsoleValue_FromDecimal(12.9, 1.0, 0, 255, &value) && value == 12, "a decimal in range drops its fraction, as before");
	Check(ConsoleValue_FromDecimal(300.7, 1.0, 0, 255, &value) && value == 255, "and one out of range is its end");
}

static void TestTheGamesFiles()
{
	puts("The game's own files:");

	std::string const commands = ReadSourceFile("3dc/avp/game_commands.cpp");
	std::string const variables = ReadSourceFile("3dc/avp/gamevars.cpp");
	std::string const cheats = ReadSourceFile("3dc/avp/win95/cheat_codes.cpp");
	std::string const identity = ReadSourceFile("3dc/build_identity.cpp");
	std::string const winmain = ReadSourceFile("3dc/avp/win95/winmain.cpp");
	std::string const aliens = ReadSourceFile("3dc/avp/bh_alien.cpp");
	std::string const variableCode = ReadSourceFile("3dc/avp/win95/gadgets/console_variable.cpp");
	std::string const commandCode = ReadSourceFile("3dc/avp/win95/gadgets/console_command.cpp");
	char const *const guard = "\tif (DebuggingCommandsActive || BuildIdentity_IsABeta())\n";

	Check(!commands.empty() && !variables.empty() && !cheats.empty() && !identity.empty() && !winmain.empty() && !aliens.empty()
		&& !variableCode.empty() && !commandCode.empty(), "the game's source can be read");

	Check(Has(identity, "return BetaBuild_VersionIsABeta(AVP_RELEASE_VERSION);"), "a beta is told by the version the build script wrote");
	Check(Has(Between(commands, guard, "\"GIVEALLWEAPONS\","), guard), "the 11 commands are there in a beta");
	Check(Has(Between(variables, guard, "\"TIMESCALE\","), guard), "and the 8 variables");
	Check(Has(Between(cheats, guard, "\"GOD\""), guard), "and GOD");

	// The flag stops saving, statistics, the films after a level and the next level's unlock. A beta mustn't set it.
	Check(TimesIn(winmain, "DebuggingCommandsActive = 1;") == 1 && Has(winmain, "else if (strstr(command_line, \"-debug\"))\n\t{\n\t\tDebuggingCommandsActive = 1;"),
		"the flag that counts as cheating is still only set by -debug");
	Check(!Has(commands, "DebuggingCommandsActive = ") && !Has(variables, "DebuggingCommandsActive = ") && !Has(cheats, "DebuggingCommandsActive = ")
		&& !Has(identity, "DebuggingCommandsActive"), "and nothing a beta runs sets it");

	// One player mustn't be able to put live aliens into everyone's game
	char const *const refusal = "\tif (AvP.Network!=I_No_Network) {\n\t\tNewOnScreenMessage(\"NO ALIENBOTS IN MULTIPLAYER MODE\");\n\t\treturn;\n\t}\n";

	Check(Has(Between(aliens, "void CastAlienBot(void) {", "CreateAlienBot(&position,0);"), refusal), "ALIENBOT is refused in a network game");
	Check(Has(Between(aliens, "void CastPredAlienBot(void) {", "CreateAlienBot(&position,1);"), refusal), "and PREDALIENBOT");
	Check(Has(Between(aliens, "void CastPraetorianBot(void) {", "CreateAlienBot(&position,2);"), refusal), "and PRAETORIANBOT");
	Check(!Has(aliens, "#if 0\n\tif (AvP.Network!=I_No_Network)"), "and none of them is switched off");

	Check(Has(variables, "timeScale -> RefuseWhen(GameVars_InANetworkGame, \"NO TIMESCALE IN MULTIPLAYER MODE\");"), "TIMESCALE is refused in a network game");
	Check(Has(variables, "return AvP.Network != I_No_Network;"), "which is any game that isn't single player");
	Check(TimesIn(Between(variableCode, "void ConsoleVariable :: ProcessSetValue", "void ConsoleVariable :: RefuseWhen"), "\tif (IsRefusedNow())\n\t{\n\t\treturn;\n\t}\n") == 2,
		"a refused variable is left alone, typed as a whole number or a decimal");

	Check(Has(variableCode, "int NewValue = ConsoleValue_ToInt(strtoll(pProjCh_Search, NULL, 10));") && !Has(variableCode, "atoi("), "a variable's typed digits are read safely");
	Check(Has(commandCode, "return ConsoleValue_ToInt(strtoll(pProjCh_Arg, NULL, 10));") && !Has(commandCode, "atoi("), "and so are a command's");
	Check(Has(variableCode, "theValue = ConsoleValue_WholeAsFixed(Val_New, MinVal, MaxVal);"), "a fixed-point variable keeps a whole number in range before the sum");
	Check(TimesIn(variableCode, "if (ConsoleValue_FromDecimal(Val_New_F, ") == 2, "and both kinds keep a decimal in range while it's still a decimal");
	Check(!Has(variableCode, "Val_New *= ONE_FIXED;") && !Has(variableCode, "int(Val_New_F"), "and the old sums, which went wrong first, are gone");
}

int main()
{
	TestWhatIsABeta();
	TestTypedWholeNumbers();
	TestTypedDecimals();
	TestTheGamesFiles();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
