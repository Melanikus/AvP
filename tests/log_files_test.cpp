// The test of the log's files: keeping the last logs, writing only to a plain file, the crash lines, and the build at the top of the log.
// Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: testers will send logs, and a log only helps if it survives the game being started again, says which build wrote it, and
// says where a crash was. log_files.h and build_identity.h say how each part works.
//
// WHAT THIS TESTS:
// - the old logs' names, and moving the logs along, for real, on files made here and taken away afterwards: seven starts keep the five
//   newest, a folder in a log's place stops the moving and is left alone, and only a plain file is emptied or written to;
// - the crash lines' wording and their cap, on addresses in this program;
// - the lines at the top of the log, with the program file's fingerprint checked against one worked out here;
// - by reading, that the game uses all of it where it should, and that the build writes the version file.
// Links (a second name for a file, a symbolic link) are only made on GitHub's machines, where GITHUB_ACTIONS is set, because making
// them is what antivirus programs watch for. Run it from the top of the repository, because that's where the source it reads is found from.
#include "log_files.h"
#include "build_identity.h"
#include "md5.h"

#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static std::string ReadWholeFile(const std::string &name)
{
	std::ifstream file(name.c_str(), std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

// A source file, with its line ends written \n whatever the checkout made them, so the checks read the same on every machine
static std::string ReadSource(const std::string &name)
{
	std::string const text = ReadWholeFile(name);
	std::string out;
	out.reserve(text.size());

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] != '\r')
		{
			out += text[i];
		}
	}

	return out;
}

static void WriteWholeFile(const std::string &name, const std::string &text)
{
	std::ofstream file(name.c_str(), std::ios::binary | std::ios::trunc);
	file << text;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	return a != std::string::npos && text.find(second, a) != std::string::npos;
}

// This run's own folder for the files it makes, inside the folder the test runs in, named by nothing but this program's number: no
// setting from outside can point it anywhere else
static std::string TestFolder()
{
	return "log_files_test_" + std::to_string(::GetCurrentProcessId());
}

// ---- Names ---------------------------------------------------------------------------------------------------------------------------

static void TestNames()
{
	printf("The old logs' names:\n");

	Check(LogFiles_OldName("avp_log.txt", 1) == "avp_log.1.txt" && LogFiles_OldName("avp_log.txt", 5) == "avp_log.5.txt", "the number goes before the ending");
	Check(LogFiles_OldName("C:/My Games/AvP/avp_log.txt", 2) == "C:/My Games/AvP/avp_log.2.txt", "in a folder too");
	Check(LogFiles_OldName("C:/a.b/log", 3) == "C:/a.b/log.3", "a name with no ending gets the number on the end, even in a folder with a dot");
	Check(kLogFilesKept == 5, "five old logs are kept");
}

// ---- Moving along ---------------------------------------------------------------------------------------------------------------

static void TestMovingAlong()
{
	printf("Keeping the last logs:\n");

	std::string const base = TestFolder();
	::CreateDirectoryA(base.c_str(), NULL);
	std::string const log = base + "/avp_log.txt";

	// Seven starts, each writing "run N" into a fresh log
	for (int run = 0; run < 7; run++)
	{
		LogFiles_MoveAlong(log);
		Check(LogFiles_Empty(log) && ReadWholeFile(log).empty(), run == 0 ? "the first start begins with an empty log" : "each start begins with an empty log");
		HANDLE const file = LogFiles_OpenToAdd(log);
		std::string const text = "run " + std::to_string(run) + "\n";
		DWORD written = 0;
		::WriteFile(file, text.data(), (DWORD)text.size(), &written, NULL);
		::CloseHandle(file);
	}

	Check(ReadWholeFile(log) == "run 6\n", "the log is this run's");
	Check(ReadWholeFile(LogFiles_OldName(log, 1)) == "run 5\n", "old log 1 is the run before");
	Check(ReadWholeFile(LogFiles_OldName(log, 5)) == "run 1\n", "old log 5 is five runs back");
	Check(!LogFiles_SomethingIsThere(LogFiles_OldName(log, 6)), "and there's never a sixth, so they can't grow without end");

	// Something that isn't a plain file in one of the old logs' places
	std::string const folder = LogFiles_OldName(log, 3);
	::DeleteFileA(folder.c_str());
	::CreateDirectoryA(folder.c_str(), NULL);
	std::string const before2 = ReadWholeFile(LogFiles_OldName(log, 2));
	LogFiles_MoveAlong(log);
	Check((::GetFileAttributesA(folder.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0, "a folder in an old log's place is left as it is");
	Check(ReadWholeFile(LogFiles_OldName(log, 2)) == before2, "and nothing is moved into it, or over the one before it");
	Check(ReadWholeFile(log) == "run 6\n", "so the log stays where it is, rather than writing over another");
	::RemoveDirectoryA(folder.c_str());

	// Something that isn't a plain file in the log's own place
	for (int n = 1; n <= kLogFilesKept; n++)
	{
		::DeleteFileA(LogFiles_OldName(log, n).c_str());
	}

	::DeleteFileA(log.c_str());
	::CreateDirectoryA(log.c_str(), NULL);
	LogFiles_MoveAlong(log);
	Check(!LogFiles_Empty(log) && LogFiles_OpenToAdd(log) == INVALID_HANDLE_VALUE, "a folder in the log's place is neither emptied nor written to");
	Check((::GetFileAttributesA(log.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0 && !LogFiles_SomethingIsThere(LogFiles_OldName(log, 1)), "and it isn't moved along either");
	::RemoveDirectoryA(log.c_str());

	// Links: only on GitHub's machines
	std::string const other = base + "/other.txt";
	std::string const target = base + "/target.txt";

	if (getenv("GITHUB_ACTIONS"))
	{
		WriteWholeFile(other, "someone else's\n");

		if (::CreateHardLinkA(log.c_str(), other.c_str(), NULL))
		{
			Check(!LogFiles_Empty(log) && LogFiles_OpenToAdd(log) == INVALID_HANDLE_VALUE, "a second name for another file is neither emptied nor written to");
			LogFiles_MoveAlong(log);
			Check(ReadWholeFile(other) == "someone else's\n" && !LogFiles_SomethingIsThere(LogFiles_OldName(log, 1)), "nor moved along, and the other file is just as it was");
			::DeleteFileA(log.c_str());
		}
		else
		{
			Check(false, "a second name for a file can be made on this disk, so that it can be tried");
		}

		WriteWholeFile(target, "what a link points to\n");

		if (::CreateSymbolicLinkA(log.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
		{
			Check(!LogFiles_Empty(log) && LogFiles_OpenToAdd(log) == INVALID_HANDLE_VALUE, "a link is neither emptied nor written to");
			Check(ReadWholeFile(target) == "what a link points to\n", "and what it points to is just as it was");
			::DeleteFileA(log.c_str());
		}
		else
		{
			printf("  (not tried: this machine doesn't let a test make a link)\n");
		}
	}
	else
	{
		printf("  (links not tried here: they only run on GitHub's machines, where GITHUB_ACTIONS is set)\n");
	}

	::DeleteFileA(other.c_str());
	::DeleteFileA(target.c_str());
	::DeleteFileA(log.c_str());

	for (int n = 1; n <= kLogFilesKept; n++)
	{
		::DeleteFileA(LogFiles_OldName(log, n).c_str());
	}

	Check(::RemoveDirectoryA(base.c_str()) != 0, "this test's files are gone again");
}

// ---- The crash lines --------------------------------------------------------------------------------------------------------------

static void SomewhereInThisProgram()
{
}

// This program's own file name, with no folder, as Windows gives it. It isn't always log_files_test.exe: the take-outs build the test
// under a name of their own.
static std::string ThisProgramsName()
{
	char path[MAX_PATH];
	DWORD const length = ::GetModuleFileNameA(NULL, path, sizeof(path));

	if (length == 0 || length >= sizeof(path))
	{
		return "";
	}

	char const *name = path;

	for (char const *c = path; *c; c++)
	{
		if (*c == '\\' || *c == '/')
		{
			name = c + 1;
		}
	}

	return name;
}

static void TestCrashLines()
{
	printf("What a crash writes:\n");

	std::string const name = ThisProgramsName();
	std::string const inThisProgram = name + "+";
	Check(!name.empty(), "this program knows its own file name");

	char place[MAX_PATH + 32];
	LogFiles_DescribePlace((const void *)&SomewhereInThisProgram, place, sizeof(place));
	Check(std::string(place).compare(0, inThisProgram.size(), inThisProgram) == 0, "an address in this program is its file name and a distance into it");
	Check(strchr(place, '\\') == NULL && strchr(place, '/') == NULL, "with no folder, which would hold the player's Windows user name");

	LogFiles_DescribePlace((const void *)8, place, sizeof(place));
	Check(place[0] == '?', "an address in nothing that's loaded is a question mark");

	void *places[40];

	for (int i = 0; i < 40; i++)
	{
		places[i] = (void *)&SomewhereInThisProgram;
	}

	char words[8192];
	LogFiles_DescribeCrash(0xC0000005, (const void *)&SomewhereInThisProgram, places, 40, words, sizeof(words));
	std::string const text = words;
	std::string const firstLine = "crash: code C0000005 at " + inThisProgram;
	Check(text.compare(0, firstLine.size(), firstLine) == 0, "the first line names the kind of crash and where it was");
	Check(Has(text, ("\r\ncrash: on the stack: " + inThisProgram).c_str()), "the second line has the places on the stack");
	Check(text.size() >= 2 && text.compare(text.size() - 2, 2, "\r\n") == 0, "and the lines end properly");

	int named = 0;

	for (size_t at = text.find(inThisProgram); at != std::string::npos; at = text.find(inThisProgram, at + 1))
	{
		named++;
	}

	Check(named == kLogFilesMostCrashPlaces + 1, "at most kLogFilesMostCrashPlaces places on the stack are written");

	// A cramped buffer: the words stop, and nothing is written past it
	char cramped[64 + 16];
	memset(cramped, 'g', sizeof(cramped));
	LogFiles_DescribeCrash(0xC0000005, (const void *)&SomewhereInThisProgram, places, 40, cramped, 64);
	bool guardsKept = true;

	for (size_t i = 64; i < sizeof(cramped); i++)
	{
		guardsKept = guardsKept && cramped[i] == 'g';
	}

	Check(guardsKept && strlen(cramped) < 64, "a cramped space is never written past, and the words still end");
}

// ---- The build at the top of the log ------------------------------------------------------------------------------------------------

static void TestBuildLines()
{
	printf("Which build wrote the log:\n");

	std::string const lines = BuildIdentity_Lines();
	printf("    %s\n", lines.c_str());

	Check(lines.compare(0, 15, "build: version ") == 0 && Has(lines, ", commit "), "the first line says the version and the commit, or that they're not known");

	char path[MAX_PATH];
	::GetModuleFileNameA(NULL, path, MAX_PATH);
	std::string const program = ReadWholeFile(path);

	struct MD5Context md5;
	MD5Init(&md5);
	MD5Update(&md5, (md5byte const *)program.data(), (unsigned)program.size());
	unsigned char digest[16];
	MD5Final(digest, &md5);
	char expected[100];
	char hex[33];

	for (int i = 0; i < 16; i++)
	{
		_snprintf(hex + i * 2, 3, "%02x", digest[i]);
	}

	hex[32] = '\0';
	_snprintf(expected, sizeof(expected) - 1, "\nbuild: this program is %u bytes, MD5 %s (worked out in ", (unsigned)program.size(), hex);
	expected[sizeof(expected) - 1] = '\0';
	Check(Has(lines, expected), "the second line has this program's size and fingerprint, and they're right");
	Check(!Has(lines, path), "and not the program's folder");
	Check(Has(lines, "\nbuild: running on Windows ") && Has(BuildIdentity_Windows(), ", build "), "the third line says which Windows, written \"10.0, build 26200\"");
}

// ---- Leaving out the player -----------------------------------------------------------------------------------------------------------

// A made-up player on a made-up machine, so the test never depends on whose machine it runs on
static void MakeUpAPlayer(LogFiles_Private &p)
{
	LogFiles_PrivateClear(p);
	LogFiles_PrivateAddFolder(p, "C:\\Users\\TesterName\\Documents\\My Games\\Aliens versus Predator\\", "<save folder>");
	LogFiles_PrivateAddFolder(p, "D:\\Games\\AvP", "<game folder>");
	LogFiles_PrivateAddFolder(p, "C:\\Users\\TesterName", "%USERPROFILE%");
	LogFiles_PrivateAddFolder(p, "C:\\Users\\TESTER~1", "%USERPROFILE%");
	LogFiles_PrivateAddFolder(p, "C:\\Users\\TesterName\\AppData\\Local\\Temp", "%TEMP%");
	LogFiles_PrivateAddWord(p, "TesterName", "<user>");
	LogFiles_PrivateAddWord(p, "TESTER~1", "<user>");
	LogFiles_PrivateAddWord(p, "TesterPC", "<computer>");
	LogFiles_PrivateAddWord(p, "ACMECORP", "<domain>");
}

// Is any of the made-up player still in this, in any capitals?
static bool StillHasThePlayer(const std::string &text)
{
	std::string lower = text;

	for (size_t i = 0; i < lower.size(); i++)
	{
		lower[i] = (char)tolower((unsigned char)lower[i]);
	}

	static const char *const secrets[] = { "testername", "tester~1", "testerpc", "acmecorp", "someone else", "fileserver", "192.168", "203.0.113", "fe80", "1c2b" };

	for (size_t s = 0; s < sizeof(secrets) / sizeof(secrets[0]); s++)
	{
		if (lower.find(secrets[s]) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

// One line through the leaving out: it must come out exactly as expected, with none of the player left in it
static void CheckLeftOut(const LogFiles_Private &p, const char *line, const char *expected, const char *what)
{
	std::string const written = LogFiles_LeaveOutThePlayer(std::string(line), p);
	bool const right = written == expected && !StillHasThePlayer(written);

	if (!right)
	{
		printf("    in:  %s\n    out: %s\n    not: %s\n", line, written.c_str(), expected);
	}

	Check(right, what);
}

// One line that must come out just as it went in
static void CheckKept(const LogFiles_Private &p, const char *line, const char *what)
{
	CheckLeftOut(p, line, line, what);
}

static void TestLeavingOutThePlayer()
{
	printf("What the log leaves out about the player:\n");

	LogFiles_Private p;
	MakeUpAPlayer(p);

	// Folders
	CheckLeftOut(p, "saved to C:\\Users\\TesterName\\Documents\\My Games\\Aliens versus Predator\\save1.sav", "saved to <save folder>\\save1.sav", "the save folder is written <save folder>, not as a folder inside the player's own");
	CheckLeftOut(p, "c:/users/testername/documents/My Games/aliens versus predator/x.txt", "<save folder>/x.txt", "with forward slashes and other capitals too");
	CheckLeftOut(p, "read C:\\\\Users\\\\TesterName\\\\AppData\\\\Roaming\\\\x.ini", "read %USERPROFILE%\\\\AppData\\\\Roaming\\\\x.ini", "and with every slash written twice");
	CheckLeftOut(p, "temp C:\\Users\\TESTER~1\\AppData\\Local\\Temp\\avp.tmp", "temp %USERPROFILE%\\AppData\\Local\\Temp\\avp.tmp", "the short form of the player's folder is %USERPROFILE% too");
	CheckLeftOut(p, "temp C:\\Users\\TesterName\\AppData\\Local\\Temp\\avp.tmp", "temp %TEMP%\\avp.tmp", "the longest folder that fits wins");
	CheckLeftOut(p, "loading D:\\Games\\AvP\\fastfile\\common.ffl", "loading <game folder>\\fastfile\\common.ffl", "the game's own folder is written <game folder>");
	CheckKept(p, "loading D:\\Games\\AvPx\\common.ffl", "but not a different folder whose name starts the same");
	CheckLeftOut(p, "\"C:\\Users\\TesterName\" is the folder", "\"%USERPROFILE%\" is the folder", "a folder in quotes ends at the quote");
	CheckLeftOut(p, "old C:\\Users\\TesterNameOld\\x.txt", "old C:\\Users\\<user>\\x.txt", "a folder that only starts with the player's folder's name doesn't count as theirs, and is someone's own");

	// Anyone's folder
	CheckLeftOut(p, "copied C:\\Users\\Someone Else\\Desktop\\a.rif", "copied C:\\Users\\<user>\\Desktop\\a.rif", "anyone else's folder under \\Users\\ has their name left out, spaces and all");
	CheckLeftOut(p, "copied C:/USERS/someone else/a.rif", "copied C:/USERS/<user>/a.rif", "with forward slashes and other capitals too");
	CheckLeftOut(p, "old C:\\Documents and Settings\\Someone Else\\a.rif", "old C:\\Documents and Settings\\<user>\\a.rif", "and under Windows XP's \\Documents and Settings\\");
	CheckLeftOut(p, "wine Z:\\home\\someone else\\a.rif", "wine Z:\\home\\<user>\\a.rif", "and under \\home\\");
	CheckLeftOut(p, "end C:\\Users\\Someone Else", "end C:\\Users\\<user>", "and at the end of a line");

	// Network paths
	CheckLeftOut(p, "loading \\\\FILESERVER\\share\\level.rif", "loading \\\\<computer>\\share\\level.rif", "the machine at the start of a network path is written <computer>");
	CheckLeftOut(p, "\\\\FILESERVER\\Users\\TesterName\\x.txt", "\\\\<computer>\\Users\\<user>\\x.txt", "and the folders after it are looked at too");
	CheckKept(p, "long name \\\\?\\D:\\x.txt", "but not \\\\?\\, which is Windows' way of writing a long name");

	// Names
	CheckLeftOut(p, "player TesterName on TesterPC (ACMECORP)", "player <user> on <computer> (<domain>)", "the player's user name, computer and network names go on their own too");
	CheckLeftOut(p, "player testername on TESTERPC", "player <user> on <computer>", "in any capitals");
	CheckLeftOut(p, "TesterNameGames and myTesterPC", "<user>Games and my<computer>", "a name of 4 letters or more goes even inside a longer word");
	CheckLeftOut(p, "folder D:\\TESTER~1\\x", "folder D:\\<user>\\x", "and the short form of the user's folder name goes as a name too");

	LogFiles_Private shortName;
	LogFiles_PrivateClear(shortName);
	LogFiles_PrivateAddWord(shortName, "ed", "<user>");
	LogFiles_PrivateAddWord(shortName, "ed/", "<user>");
	Check(shortName.wordCount == 1, "a name with a slash on its end is the same name, kept once");
	CheckLeftOut(shortName, "ed loaded the level, said ed.", "<user> loaded the level, said <user>.", "a name of fewer than 4 letters goes only where it's a whole word, so the log can still be read");

	LogFiles_Private digits;
	LogFiles_PrivateClear(digits);
	LogFiles_PrivateAddWord(digits, "192", "<user>");
	CheckLeftOut(digits, "from 192.168.1.20 as 192", "from <local address> as <user>", "an address is looked for before the names, so a name made of digits can't cut one in two");

	// Addresses
	CheckLeftOut(p, "Enet got a connection from 192.168.1.20:23513", "Enet got a connection from <local address>:<port>", "a home network's address is written <local address>, and its port <port>");
	CheckLeftOut(p, "host 203.0.113.7 and 10.0.0.5, 172.20.1.1, 172.32.1.1", "host <internet address> and <local address>, <local address>, <internet address>", "the whole address goes, and the kind tells the internet from a home network");
	CheckLeftOut(p, "self 127.0.0.1:1234.", "self <loopback address>:<port>.", "this machine's own address is <loopback address>");
	CheckLeftOut(p, "v6 [fe80::1c2b:3d4e]:23513 and ::1 and 00:1a:2b:3c:4d:5e", "v6 [<address>]:<port> and <address> and <address>", "the newer, longer kind of address goes too, and so does a network card's number");

	// What stays
	CheckKept(p, "at 19:05:34 the join took 12.5 ms, round trip 180 ms, 3 lost", "times, and how long things took, stay");
	CheckKept(p, "build: running on Windows 10.0, build 26200", "Windows' version, written as the game writes it, stays");
	CheckKept(p, "build: version 1.0.0, commit 0123456789abcdef0123456789abcdef01234567", "our own version of three numbers stays, and so does a commit");
	CheckKept(p, "crash: code C0000005 at AvPx.exe+1A2B3C", "a crash line stays");
	CheckKept(p, "parts 1.2.3.4.5 and 300.1.1.1 and 1.2.3", "numbers with dots that can't be an address stay");
	CheckKept(p, "Jill joined as a marine; std::string and Class::Function", "a player's own name from the lobby stays, and so do names with :: in them");
	CheckLeftOut(p, "driver 27.21.14.5", "driver <internet address>", "but four numbers of up to 3 digits with dots between always go, even if they were a version: the game never writes one that way");

	// The fixed buffer, as the crash words use it
	char cramped[24 + 8];
	memset(cramped, 'g', sizeof(cramped));
	LogFiles_LeaveOutThePlayer("a crash in C:\\Users\\TesterName\\x and more", p, cramped, 24);
	bool guardsKept = true;

	for (size_t i = 24; i < sizeof(cramped); i++)
	{
		guardsKept = guardsKept && cramped[i] == 'g';
	}

	Check(guardsKept && strlen(cramped) == 23 && std::string(cramped) == std::string("a crash in %USERPROFILE%\\x and more").substr(0, 23), "a fixed buffer is never written past, ends properly, and is cut short with the player still left out");

	char exact[64];
	LogFiles_LeaveOutThePlayer("on TesterPC at 192.168.1.20", p, exact, sizeof(exact));
	Check(std::string(exact) == "on <computer> at <local address>", "and with room, it gives the same as the other way");
}

// What this machine really has, found the way the game finds it
static void TestGatheringOnThisMachine()
{
	printf("What it finds on this machine:\n");

	LogFiles_Private p;
	LogFiles_PrivateGather(p, NULL);

	char profile[MAX_PATH];
	char user[MAX_PATH];
	char computer[MAX_PATH];
	char program[MAX_PATH];
	DWORD userSize = sizeof(user);
	DWORD computerSize = sizeof(computer);

	bool const found = ::GetEnvironmentVariableA("USERPROFILE", profile, sizeof(profile)) > 0 && ::GetUserNameA(user, &userSize) && ::GetComputerNameA(computer, &computerSize) && ::GetModuleFileNameA(NULL, program, sizeof(program)) > 0;
	Check(found, "this machine has a user folder, a user name and a computer name to try");

	if (!found)
	{
		return;
	}

	std::string const fromProfile = LogFiles_LeaveOutThePlayer(std::string(profile) + "\\x.txt", p);
	Check(fromProfile == "%USERPROFILE%\\x.txt", "the player's own folder is found and written %USERPROFILE%");

	// Its short form, where this disk makes them and the name is long enough to have one
	char shortForm[MAX_PATH];
	DWORD const shortLength = ::GetShortPathNameA(profile, shortForm, sizeof(shortForm));

	if (shortLength > 0 && shortLength < sizeof(shortForm) && _stricmp(shortForm, profile) != 0)
	{
		Check(LogFiles_LeaveOutThePlayer(std::string(shortForm) + "\\x.txt", p) == "%USERPROFILE%\\x.txt", "and so is its short form");
	}
	else
	{
		printf("  (the short form isn't tried on a line: this machine's user folder has none)\n");
	}

	// Whether or not this disk makes short forms, the short form is always asked for and kept, even when it's the same as the long one.
	// So both are there, and a machine that has short forms gets them left out too.
	int userFolders = 0;

	for (int f = 0; f < p.folderCount; f++)
	{
		userFolders += strcmp(p.foldersWrittenAs[f], "%USERPROFILE%") == 0 ? 1 : 0;
	}

	Check(userFolders == 2, "the user folder's short form is asked for and kept beside it");

	std::string const fromNames = LogFiles_LeaveOutThePlayer(std::string("by ") + user + " on " + computer, p);
	Check(fromNames == "by <user> on <computer>" || (strlen(user) < kLogFilesNamesGoEverywhereFrom && fromNames.find(computer) == std::string::npos), "the user name and the computer name are found and left out");

	std::string const fromProgram = LogFiles_LeaveOutThePlayer(std::string(program), p);
	Check(fromProgram == "<game folder>\\" + ThisProgramsName(), "the game's own folder is found from where the program is");

	std::string const header = BuildIdentity_Lines();
	Check(LogFiles_LeaveOutThePlayer(header, p) == header, "the lines at the top of the log come through whole");
}

// ---- By reading -----------------------------------------------------------------------------------------------------------------------

static void TestTheGameUsesIt()
{
	printf("Where the game uses it, by reading:\n");

	std::string const log = ReadSource("3dc/logString.cpp");
	Check(ComesBefore(log, "void Empty()", "LogFiles_MoveAlong(filePath);") && ComesBefore(log, "LogFiles_MoveAlong(filePath);", "LogFiles_Empty(filePath);"), "starting the log moves the old ones along, then empties a fresh one");
	Check(Has(log, "file = LogFiles_OpenToAdd(filePath);"), "the log's lines are only added to a plain file");
	Check(!Has(log, "CREATE_ALWAYS") && !Has(log, "CreateFileA("), "and nothing in the log opens its file any other way");
	Check(ComesBefore(log, "static LONG WINAPI WriteLogBeforeCrash", "RtlCaptureStackBackTrace(") && ComesBefore(log, "RtlCaptureStackBackTrace(", "logWriter->WriteCrashWords(crashWords);"), "a crash writes what it was and where, without the lock");
	Check(!Has(log, "IsBadReadPtr"), "and doesn't read memory itself to do it");

	// Leaving out the player: found before the writer starts, and every line and the crash words go through it before they're written
	Check(ComesBefore(log, "LogWriter() :", "LogFiles_PrivateGather(privateThings, folder);") && ComesBefore(log, "LogFiles_PrivateGather(privateThings, folder);", "thread = CreateThread("), "what to leave out is found before the writer's thread starts");
	Check(ComesBefore(log, "void Add(const std::string &line, bool mustGoIn)\n\t{\n\t\tstd::string const written = LogFiles_LeaveOutThePlayer(line, privateThings);", "waiting += written;"), "every line has the player left out before it waits to be written");
	Check(!Has(log, "waiting += line;") && !Has(log, "waiting.size() + line.size()"), "and nothing puts a line on the list as it came");
	Check(ComesBefore(log, "void WriteCrashWords(const char *words)\n\t{\n\t\tLogFiles_LeaveOutThePlayer(words, privateThings, crashWritten, sizeof(crashWritten));", "WriteFile(crashFile, crashWritten, (DWORD)strlen(crashWritten), &written, NULL);"), "the crash words too, in a buffer of their own");

	// The game's log file is written in two places only: the list, which only holds lines that came through Add, and the crash words
	size_t writes = 0;

	for (size_t at = log.find("WriteFile("); at != std::string::npos; at = log.find("WriteFile(", at + 1))
	{
		writes++;
	}

	Check(writes == 2, "the log's file is written in two places only, and both are covered");

	std::string const start = ReadSource("3dc/avp/win95/winmain.cpp");
	Check(ComesBefore(start, "ClearLog();", "LogString(BuildIdentity_Lines());"), "the log starts with which build it is");

	std::string const build = ReadSource("tools/build-and-package.ps1");
	Check(ComesBefore(build, "Out-File -Encoding ascii '3dc\\build_version.h'", "& $msbuild win32\\VS2026\\AvP.sln"), "the build writes the version file before it builds the game");
	Check(Has(build, "-cmatch '^[0-9]{1,6}\\.[0-9]{1,6}\\.[0-9]{1,6}\\z'"), "and only a version of three numbers goes in it, so the log never takes it for an address");

	std::string const release = ReadSource(".github/workflows/release.yml");
	Check(ComesBefore(release, "- name: Build the game and make its package", "VERSION: ${{ inputs.version }}") && ComesBefore(release, "VERSION: ${{ inputs.version }}\n        run: ./tools/build-and-package.ps1", "Refuse a package"), "the release gives the build its version");

	// The take-outs copy folders, not single files, so their copy has no .gitignore to read
	std::string const ignored = ReadSource(".gitignore");

	if (!ignored.empty())
	{
		Check(Has(ignored, "3dc/build_version.h"), "and the version file is never committed");
	}
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestNames();
	TestMovingAlong();
	TestCrashLines();
	TestBuildLines();
	TestLeavingOutThePlayer();
	TestGatheringOnThisMachine();
	TestTheGameUsesIt();

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
