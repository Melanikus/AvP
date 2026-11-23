// The plain file name test. Written 2026-11-23 by Melanikus (Drake) for the AvP port (card 6, Tao 1 and 3).
//
// WHAT IT TESTS
//
// 3dc/plain_file_name.h says when a name, given to the game to save or read one of its settings files, is only a file's name. The
// multiplayer settings are saved as <save folder>MPConfig/<name>.cfg, and the name comes from outside: the player types it, Ctrl+V
// pastes it (anything at all, control characters too), and every multiplayer start saves "Previous Game" under a name read from
// language.txt, which anyone can change. This test gives it good names, and the hostile ones: a way into other folders (the language
// text "..\..\..\Users\Public\x" among them), the characters Windows refuses, control characters, folder names, names Windows would
// shorten, devices, and a name too long. Then it reads mp_config.cpp, to see that every path it makes from a name is made only after
// the name has been judged, and that a name refused there is never used.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles it with the settings in tests/plain_file_name_test.args. Run it from the top of the repository, where the
// source it reads is found. It gives back a non-zero result if any check failed.
#include "plain_file_name.h"

#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static bool Plain(const char *name)
{
	return PlainFileName_Why(name).empty();
}

// A source file, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const std::string &name)
{
	std::ifstream file(name.c_str(), std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	std::string out;

	for (char c : text.str())
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

// The text of a function, from the line that starts it to the first line that is only "}"; empty if it isn't there
static std::string FunctionText(const std::string &source, const std::string &start)
{
	size_t const from = source.find(start);

	if (from == std::string::npos)
	{
		return std::string();
	}

	size_t const to = source.find("\n}\n", from);
	return source.substr(from, to == std::string::npos ? std::string::npos : to + 3 - from);
}

static void TestGoodNames()
{
	puts("names that are plain file names");

	Check(Plain("Previous Game"), "the English \"Previous Game\" is one");
	Check(Plain("Partie pr\xe9" "c\xe9" "dente"), "a name with letters above 127 (French) is one");
	Check(Plain("my.game"), "a dot inside a name is fine");
	Check(Plain("CONSOLE") && Plain("COM10") && Plain("LPT") && Plain("NULL") && Plain("my con") && Plain("Auxiliary"),
		"names that only start like a device's are fine");
	Check(Plain(std::string(kPlainFileNameMost, 'a').c_str()), "a name of the longest length is one");
}

static void TestHostileNames()
{
	puts("names that are not, and are refused");

	// The language text: every multiplayer start saves under it, so a changed language.txt chose where the game wrote
	Check(!Plain("..\\..\\..\\Users\\Public\\x"), "language.txt's \"..\\..\\..\\Users\\Public\\x\" is refused");
	Check(!Plain("../../evil") && !Plain("a/b") && !Plain("a\\b"), "a name with \\ or / is refused, whatever is around them");
	Check(!Plain("C:x") && !Plain("name:stream"), "a name with : (another disk, or a hidden part of a file) is refused");
	Check(!Plain("a*") && !Plain("a?") && !Plain("a\"b") && !Plain("<a") && !Plain("a>") && !Plain("a|b"), "the other characters Windows refuses are refused");
	Check(!Plain("a\tb") && !Plain("line\nbreak") && !Plain("a\x7f"), "a control character (pasted with Ctrl+V) is refused");
	Check(!Plain(".") && !Plain(".."), "\".\" and \"..\" are refused");
	Check(!Plain("name.") && !Plain("name "), "a name ending in a dot or a space (Windows would leave it off) is refused");
	Check(!Plain("CON") && !Plain("con") && !Plain("Nul") && !Plain("PRN") && !Plain("AUX") && !Plain("CONIN$") && !Plain("CONOUT$"),
		"a device's name is refused, in any case");
	Check(!Plain("COM1") && !Plain("lpt9") && !Plain("COM0") && !Plain("COM\xb9") && !Plain("LPT\xb3"), "COM and LPT with a digit (or a raised one) are refused");
	Check(!Plain("nul.cfg") && !Plain("CON .x") && !Plain("aux..txt"), "a device's name with an ending after a dot is refused too");
	Check(!Plain("") && !Plain(NULL), "no name at all is refused");
	Check(!Plain(std::string(kPlainFileNameMost + 1, 'a').c_str()), "a name one byte too long is refused");

	std::string const why = PlainFileName_Why("..\\x");
	Check(why.find('\\') != std::string::npos, "and the words say what's wrong with it, for the log");
	Check(PlainFileName_Printable("a\nb\x01" "c") == "a?b?c", "a name is shown in the log with each control character as ?");
}

// By reading: mp_config.cpp makes every path from a name in BuildConfigPath, which judges the name before anything else
static void TestMpConfigUsesIt()
{
	puts("mp_config.cpp, by reading");

	std::string const source = ReadSource("3dc/avp/win95/frontend/mp_config.cpp");
	Check(!source.empty(), "3dc/avp/win95/frontend/mp_config.cpp is read (run from the top of the repository)");

	std::string const build = FunctionText(source, "static BOOL BuildConfigPath(");
	size_t const judged = build.find("PlainFileName_Why(name)");
	size_t const made = build.find("_snprintf(");
	Check(judged != std::string::npos && made != std::string::npos && judged < made, "BuildConfigPath judges the name before it makes the path");

	size_t const refused = build.find("return FALSE;", judged == std::string::npos ? 0 : judged);
	Check(judged != std::string::npos && refused != std::string::npos && refused < made, "and a name refused makes no path: it gives back FALSE first");
	Check(build.find("PlainFileName_Printable(name)") != std::string::npos, "and the log shows the refused name only as PlainFileName_Printable makes it");

	// Every file mp_config.cpp opens or deletes by a name is one whose path BuildConfigPath made, earlier in the same function: so every
	// one of them was judged. (Its other _snprintf calls make words for the screen, and one the folder's fixed search pattern.)
	const char *const uses[] = { "file.Open(filename", "avp_fopen(filename", "avp_open_userfile(filename", "DeleteFile(filename" };
	size_t found = 0;
	size_t unjudged = 0;

	for (const char *use : uses)
	{
		for (size_t at = source.find(use); at != std::string::npos; at = source.find(use, at + 1))
		{
			size_t const functionStart = source.rfind("\n}\n", at);
			size_t const built = source.rfind("BuildConfigPath(filename", at);
			found++;

			if (built == std::string::npos || (functionStart != std::string::npos && built < functionStart))
			{
				unjudged++;
			}
		}
	}

	Check(found >= 6 && unjudged == 0, "every file mp_config.cpp opens or deletes by a name has its path from BuildConfigPath, in the same function");

	// The save writes through FileStream's judged write (FileStream.cpp: PlainWrite_Open), and the IP address the same way
	std::string const save = FunctionText(source, "void SaveMultiplayerConfiguration(");
	Check(save.find("FileStream::FileWrite") != std::string::npos && save.find("fopen") == std::string::npos,
		"SaveMultiplayerConfiguration writes through FileStream's write, never fopen");
	std::string const ip = FunctionText(source, "void SaveIPAddress(");
	Check(!ip.empty() && ip.find("FileStream::FileWrite") != std::string::npos && ip.find("fopen") == std::string::npos,
		"SaveIPAddress writes through FileStream's write too, never fopen");

	std::string const stream = ReadSource("3dc/FileStream.cpp");
	std::string const actual = FunctionText(stream, "bool FileStream::OpenActual(");
	Check(actual.find("PlainWrite_Open(") != std::string::npos, "and FileStream opens a file to write only with PlainWrite_Open (3dc/plain_write.h)");

	// The language text reaches a file only as a name given to these functions
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	Check(menus.find("SaveMultiplayerConfiguration(GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME))") != std::string::npos,
		"menus.cpp saves \"Previous Game\" under language.txt's name only through SaveMultiplayerConfiguration, so it's judged there");
}

int main()
{
	TestGoodNames();
	TestHostileNames();
	TestMpConfigUsesIt();

	if (checksFailed)
	{
		printf("The plain file name test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The plain file name test passed");
	return 0;
}
