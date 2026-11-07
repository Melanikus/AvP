// The test of a level's own log (card LEVELLOG-1). Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: when a level doesn't load, or loads with things refused, the game writes what it said beside the level, as
// <name>_errorlog.txt. The name comes from a file someone may have downloaded, so it's checked before any file is named after it, and
// the writer must never follow a link or write over a file the game didn't write. level_log.h says how.
//
// WHAT THIS TESTS:
// - the name rule, on good names and on every kind of bad one;
// - the writer, for real, on files and folders made here and taken away afterwards: a new log, its own log written over, someone
//   else's file, a folder, a second name for another file, a link if this machine lets a test make one, a missing folder, the cap;
// - the gathering of lines, with its caps;
// - LevelLog_Finish itself, the part the game calls, compiled with stand-ins for the game's log and its save folder;
// - by reading, that the game really starts and finishes gathering where it loads a level, and that its log hands every line on.
// Run it from the top of the repository, because that's where the source it reads is found from.
#include "level_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Windows' value for "let a program that isn't an administrator make a link, in developer mode". Older headers don't have its name.
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

static bool WriteWholeFile(const std::string &name, const std::string &text)
{
	std::ofstream file(name.c_str(), std::ios::binary | std::ios::trunc);
	file << text;
	return file.good();
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

// Does first come before second in text, with both there?
static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(second, a);
	return a != std::string::npos && b != std::string::npos;
}

// ---- Stand-ins for the game, for LevelLog_Finish -----------------------------------------------------------------------------

static std::vector<std::string> logged;
static std::string standInSaveFolder;

void LogString(const std::string &logString)
{
	logged.push_back(logString);
}

char *GetSaveFolderPath()
{
	return const_cast<char *>(standInSaveFolder.c_str());
}

// ---- The name rule -------------------------------------------------------------------------------------------------------------

static void TestNames()
{
	printf("The name a level's log is called after:\n");

	Check(LevelLog_NameIsSafe("invasion_a"), "one of the game's own levels' names can be used");
	Check(LevelLog_NameIsSafe("My Map v2.final"), "so can a custom one's with spaces and dots inside it");
	Check(!LevelLog_NameIsSafe(NULL) && !LevelLog_NameIsSafe(""), "but not no name at all");

	std::string longest(kLevelLogLongestName, 'a');
	Check(LevelLog_NameIsSafe(longest.c_str()), "a name as long as the longest allowed can be used");
	Check(!LevelLog_NameIsSafe((longest + "a").c_str()), "one character longer can't");

	char const *const forbidden = "/\\:*?\"<>|";

	for (char const *c = forbidden; *c; c++)
	{
		char name[8] = { 'a', *c, 'b', 0 };

		if (LevelLog_NameIsSafe(name))
		{
			Check(false, "a name with any of / \\ : * ? \" < > | in it can't be used");
			break;
		}

		if (!c[1])
		{
			Check(true, "a name with any of / \\ : * ? \" < > | in it can't be used");
		}
	}

	Check(!LevelLog_NameIsSafe("a\x01" "b") && !LevelLog_NameIsSafe("a\x1f" "b") && !LevelLog_NameIsSafe("a\x7f" "b"), "nor one with a character below a space, or the delete character");
	Check(!LevelLog_NameIsSafe("a..b") && !LevelLog_NameIsSafe(".."), "nor one with two dots together");
	Check(!LevelLog_NameIsSafe("map.") && !LevelLog_NameIsSafe("map "), "nor one ending in a dot or a space, which Windows would take off");
	Check(!LevelLog_NameIsSafe(" map"), "nor one starting with a space");
	Check(!LevelLog_NameIsSafe("CON") && !LevelLog_NameIsSafe("nul") && !LevelLog_NameIsSafe("Aux") && !LevelLog_NameIsSafe("PRN"), "nor a device's name");
	Check(!LevelLog_NameIsSafe("COM1") && !LevelLog_NameIsSafe("lpt9") && !LevelLog_NameIsSafe("COM0"), "including COM and LPT with a number");
	Check(!LevelLog_NameIsSafe("NUL.map") && !LevelLog_NameIsSafe("com3.x"), "and a device's name before a dot, which Windows still takes as the device");
	Check(LevelLog_NameIsSafe("CONSOLE") && LevelLog_NameIsSafe("COM") && LevelLog_NameIsSafe("COM10") && LevelLog_NameIsSafe("nullmap"), "but a name that only starts like one is fine");

	std::string folder, file;
	Check(LevelLog_SplitName("invasion_a", &folder, &file) && folder.empty() && file == "invasion_a", "a stock level is in no folder");
	Check(LevelLog_SplitName("Custom/mymap", &folder, &file) && folder == kLevelLogCustomFolder && file == "mymap", "a custom level is in Custom/");
	Check(LevelLog_SplitName("custom/mymap", &folder, &file) && folder == kLevelLogCustomFolder, "however the menu wrote its capitals");
	Check(!LevelLog_SplitName("Other/mymap", &folder, &file), "any other folder is refused");
	Check(!LevelLog_SplitName("Custom/../../windows/system32/x", &folder, &file), "and so is a way out of the folder");
	Check(!LevelLog_SplitName("Custom/sub/mymap", &folder, &file) && !LevelLog_SplitName("Custom/", &folder, &file), "and a folder inside Custom, or Custom with no name");
	Check(!LevelLog_SplitName("C:/mymap", &folder, &file) && !LevelLog_SplitName("\\\\server\\share\\map", &folder, &file), "and a drive, or another machine");

	Check(LevelLog_PathBesideTheLevel("avp_rifs", "invasion_a") == "avp_rifs/invasion_a_errorlog.txt", "a stock level's log goes beside it");
	Check(LevelLog_PathBesideTheLevel("avp_rifs", "Custom/mymap") == "avp_rifs/Custom/mymap_errorlog.txt", "and a custom one's beside it in Custom");
	Check(LevelLog_PathBesideTheLevel("avp_rifs", "Custom/../x").empty() && LevelLog_PathBesideTheLevel(NULL, "mymap").empty(), "a name that can't be used goes nowhere beside the level");
	Check(LevelLog_PathInTheGamesFolder("C:/Games/AvP/", "Custom/mymap") == "C:/Games/AvP/mymap_errorlog.txt", "in the game's folder it keeps the level's name");
	Check(LevelLog_PathInTheGamesFolder("C:/Games/AvP/", "Custom/../x") == std::string("C:/Games/AvP/") + kLevelLogFallbackName, "or the one fixed name if that can't be used");
	Check(LevelLog_PathInTheGamesFolder(NULL, "mymap") == "mymap_errorlog.txt", "and with no folder it's the folder the game started from");
}

// ---- The writer, on real files --------------------------------------------------------------------------------------------

// The folder for this run's files: something an earlier run left behind must not count against this one. It's inside the folder the test
// runs in, named by nothing but this program's number, so no setting from outside can point it anywhere else.
static std::string TestFolder()
{
	return "level_log_test_files_" + std::to_string(::GetCurrentProcessId());
}

static void TestTheWriter()
{
	printf("Writing a level's log:\n");

	std::string const base = TestFolder();
	::CreateDirectoryA(base.c_str(), NULL);

	std::string const log = base + "/mymap_errorlog.txt";
	std::string const first = std::string(kLevelLogFirstLine) + "the first log, which is long enough to leave some behind if a shorter one didn't cut it off\r\n";
	std::string const second = std::string(kLevelLogFirstLine) + "the second\r\n";

	Check(LevelLog_WriteFile(log, first) == 0 && ReadWholeFile(log) == first, "a new log is written");
	Check(LevelLog_WriteFile(log, second) == 0 && ReadWholeFile(log) == second, "and written over by the next, with nothing of the first left");

	std::string const empty = base + "/empty_errorlog.txt";
	WriteWholeFile(empty, "");
	Check(LevelLog_WriteFile(empty, first) == 0 && ReadWholeFile(empty) == first, "an empty file of that name is written over, because there's nothing in it to lose");

	std::string const someoneElses = base + "/theirs_errorlog.txt";
	// Longer than the first line, so that it's the comparison that refuses it, and not that it's too short to read that far
	std::string const theirText = "notes someone else keeps here, long enough to be read as far as a log's own first line\r\n";
	WriteWholeFile(someoneElses, theirText);
	Check(LevelLog_WriteFile(someoneElses, first) == kLevelLogNotOurs, "a file of that name the game didn't write is refused");
	Check(ReadWholeFile(someoneElses) == theirText, "and left just as it was");

	std::string const aFolder = base + "/folder_errorlog.txt";
	::CreateDirectoryA(aFolder.c_str(), NULL);
	const char *const folderAnswer = LevelLog_WriteFile(aFolder, first);
	Check(folderAnswer == kLevelLogCantOpen || folderAnswer == kLevelLogNotAPlainFile, "a folder of that name is refused");
	Check((::GetFileAttributesA(aFolder.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0, "and is still a folder");

	// Links: a second name for another file, and a link to one. Making links is what an antivirus program watches for, so these only
	// run on GitHub's machines, which say so with GITHUB_ACTIONS. Anywhere else they're skipped, and the test says so.
	std::string const other = base + "/other.txt";
	std::string const hardLink = base + "/hardlink_errorlog.txt";
	std::string const target = base + "/target.txt";
	std::string const symlink = base + "/link_errorlog.txt";

	if (getenv("GITHUB_ACTIONS"))
	{
		// A second name for another file: writing there would change the other file
		WriteWholeFile(other, first);

		if (::CreateHardLinkA(hardLink.c_str(), other.c_str(), NULL))
		{
			Check(LevelLog_WriteFile(hardLink, second) == kLevelLogNotAPlainFile, "a second name for another file is refused, even one that looks like a log");
			Check(ReadWholeFile(other) == first, "and the other file is left just as it was");
		}
		else
		{
			Check(false, "a second name for a file can be made on this disk, so that it can be tried");
		}

		// A link to another file. Windows only lets a program make one in developer mode, or as an administrator, so on a machine
		// where it can't, this says so and isn't counted either way.
		WriteWholeFile(target, first);

		if (::CreateSymbolicLinkA(symlink.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
		{
			Check(LevelLog_WriteFile(symlink, second) == kLevelLogNotAPlainFile, "a link is refused, even to a file that looks like a log");
			Check(ReadWholeFile(target) == first, "and what it points to is left just as it was");
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

	Check(LevelLog_WriteFile(base + "/no such folder/mymap_errorlog.txt", first) == kLevelLogCantOpen, "a folder that isn't there can't be written in");

	std::string const big = base + "/big_errorlog.txt";
	std::string const tooMuch = std::string(kLevelLogFirstLine) + std::string(kLevelLogMostBytes * 2, 'x');
	Check(LevelLog_WriteFile(big, tooMuch) == 0 && ReadWholeFile(big).size() == kLevelLogMostBytes, "a log is never more than its cap");

	// Take away what was made, links before what they point to
	::DeleteFileA(symlink.c_str());
	::DeleteFileA(target.c_str());
	::DeleteFileA(hardLink.c_str());
	::DeleteFileA(other.c_str());
	::RemoveDirectoryA(aFolder.c_str());
	::DeleteFileA(someoneElses.c_str());
	::DeleteFileA(empty.c_str());
	::DeleteFileA(big.c_str());
	::DeleteFileA(log.c_str());
	Check(::RemoveDirectoryA(base.c_str()) != 0, "this test's files are gone again");
}

// ---- Gathering lines --------------------------------------------------------------------------------------------------------------

static void TestGathering()
{
	printf("Gathering what the game says:\n");

	LevelLogLines lines = { false, std::string(), 0, 0, 0 };
	lines.Note("before\n", true);
	Check(lines.text.empty() && lines.errors == 0, "nothing is kept before gathering starts");

	lines.Start();
	lines.Note("one\n", false);
	lines.Note("\tError: two\n", true);
	lines.Note("three\n", false);
	lines.Stop();
	lines.Note("after\n", true);
	Check(lines.text == "one\n\tError: two\nthree\n" && lines.lines == 3, "every line while gathering is kept, in order");
	Check(lines.errors == 1, "and the errors among them are counted");
	Check(!Has(lines.text, "after"), "and nothing after it stops");

	lines.Start();
	Check(lines.text.empty() && lines.errors == 0 && lines.lines == 0, "starting again begins with nothing");

	for (int i = 0; i < kLevelLogMostLines + 100; i++)
	{
		lines.Note("x\n", false);
	}

	Check(lines.lines == kLevelLogMostLines && lines.linesLeftOut == 100, "past the most lines, the rest are counted and left out");

	lines.Start();
	lines.Note(std::string(kLevelLogMostBytes, 'y'), true);
	Check(lines.text.empty() && lines.linesLeftOut == 1 && lines.errors == 1, "a line too big for the cap is left out, and still counted as an error");
}

// ---- LevelLog_Finish, with the stand-ins ----------------------------------------------------------------------------------------

static void TestFinishing()
{
	printf("What the game writes at the end of a load:\n");

	std::string const base = TestFolder() + "_finish";
	std::string const levels = base + "/avp_rifs";
	std::string const games = base + "/games/";
	::CreateDirectoryA(base.c_str(), NULL);
	::CreateDirectoryA(levels.c_str(), NULL);
	::CreateDirectoryA(games.c_str(), NULL);
	standInSaveFolder = games;

	std::string const levelFile = levels + "/mymap.rif";
	WriteWholeFile(levelFile, "abc");
	std::string const beside = levels + "/mymap_errorlog.txt";

	LevelLog_StartGathering();
	LevelLog_NoteLine("loading\n", false);
	LevelLog_Finish(levels.c_str(), "mymap", levelFile.c_str(), false);
	Check(::GetFileAttributesA(beside.c_str()) == INVALID_FILE_ATTRIBUTES, "a level that loads with nothing refused writes nothing");

	LevelLog_StartGathering();
	LevelLog_NoteLine("loading\n", false);
	LevelLog_NoteLine("\tError: a decal left out\n", true);
	LevelLog_Finish(levels.c_str(), "mymap", levelFile.c_str(), false);
	std::string const refused = ReadWholeFile(beside);
	Check(refused.compare(0, strlen(kLevelLogFirstLine), kLevelLogFirstLine) == 0, "one with something refused writes its log beside it");
	Check(Has(refused, "loading\n") && Has(refused, "a decal left out") && Has(refused, "1 things refused"), "with what the game said, and how many things were refused");
	// The MD5 of "abc"
	Check(Has(refused, "3 bytes, MD5 900150983cd24fb0d6963f7d28e17f72"), "and the level file's size and fingerprint");
	Check(!logged.empty() && Has(logged.back(), "written beside the level"), "and the game's log says where it went");

	LevelLog_StartGathering();
	LevelLog_Finish(levels.c_str(), "mymap", levelFile.c_str(), true);
	Check(Has(ReadWholeFile(beside), "couldn't be loaded"), "a level that can't be loaded always writes its log");

	// Beside the level is taken by someone else's file, so it goes to the game's folder
	std::string const theirs = levels + "/othermap_errorlog.txt";
	WriteWholeFile(theirs, "theirs\r\n");
	LevelLog_StartGathering();
	LevelLog_Finish(levels.c_str(), "othermap", (levels + "/othermap.rif").c_str(), true);
	Check(ReadWholeFile(theirs) == "theirs\r\n", "a file beside the level that the game didn't write is left alone");
	Check(Has(ReadWholeFile(games + "othermap_errorlog.txt"), "couldn't be loaded"), "and the log goes to the game's own folder instead");
	Check(Has(ReadWholeFile(games + "othermap_errorlog.txt"), "it couldn't be opened"), "saying the level's file couldn't be read, when it isn't there");
	Check(Has(logged.back(), "so it was written as"), "and the game's log says so");

	// A name that can't be used goes only to the one fixed name in the game's folder
	LevelLog_StartGathering();
	LevelLog_Finish(levels.c_str(), "Custom/../../x", "", true);
	std::string const fixed = ReadWholeFile(games + kLevelLogFallbackName);
	Check(Has(fixed, "one whose name can't be used") && !Has(fixed, ".."), "a name that can't be used is never written into a path or the log");

	::DeleteFileA((games + kLevelLogFallbackName).c_str());
	::DeleteFileA((games + "othermap_errorlog.txt").c_str());
	::DeleteFileA(theirs.c_str());
	::DeleteFileA(beside.c_str());
	::DeleteFileA(levelFile.c_str());
	::RemoveDirectoryA(games.c_str());
	::RemoveDirectoryA(levels.c_str());
	Check(::RemoveDirectoryA(base.c_str()) != 0, "this test's files are gone again");
}

// ---- By reading ---------------------------------------------------------------------------------------------------------------------

static void TestTheGameUsesIt()
{
	printf("Where the game uses it, by reading:\n");

	std::string const system = ReadWholeFile("3dc/avp/win95/system.cpp");
	std::string const loading = system.substr(system.find("void LoadRifFile()") == std::string::npos ? system.size() : system.find("void LoadRifFile()"));

	Check(ComesBefore(loading, "LevelLog_StartGathering();", "avp_load_rif("), "loading a level starts gathering before it reads the file");
	Check(ComesBefore(loading, "LevelLog_Finish(GameDataDirName, Env_List[AvP.CurrentEnv]->main, pathMade ? file_and_path : \"\", true);", "exit(0x3421);"), "a level that can't be loaded has its log written before the game stops");
	Check(ComesBefore(loading, "copy_rif_data(", "LevelLog_Finish(GameDataDirName, Env_List[AvP.CurrentEnv]->main, file_and_path, false);"), "and one that loads, once all of it is in");

	// The log's writer hands every line on once the player is left out of it (log_files.h), so the level's log never has them either
	std::string const log = ReadWholeFile("3dc/logString.cpp");
	Check(ComesBefore(log, "std::string const written = LogFiles_LeaveOutThePlayer(line, privateThings);", "LevelLog_NoteLine(written, mustGoIn);") && ComesBefore(log, "LevelLog_NoteLine(written, mustGoIn);", "waiting += written;"), "every line the game logs is handed on, with the player already left out");
	Check(ComesBefore(log, "void WriteErrorToLog(const std::string &logLine)", "writer->Add(logLine, true);"), "and every error, as an error");
	Check(!Has(log, "LevelLog_NoteLine(logLine") && !Has(log, "LevelLog_NoteLine(line"), "and nothing hands on a line as it came");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestNames();
	TestTheWriter();
	TestGathering();
	TestFinishing();
	TestTheGameUsesIt();

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
