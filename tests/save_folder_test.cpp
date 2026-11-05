// What happens when the game can't make the folder it keeps its files in. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT WENT WRONG: GetSaveFolderPath gave back nothing at all if it couldn't find or make the folder, and callers that used what
// it gave back would have crashed. It asked the log to say so, and the log asks GetSaveFolderPath for the folder, so a game with
// the old log would have gone round until it ran out of memory for it, and one with the new log lost the line. It also left half a
// path behind, for the next caller to take as the whole of one. And a saved game with nowhere to go was lost without a word.
//
// WHAT THIS TESTS: the part that makes the folders is tested for real, on folders made here and taken away afterwards. The part
// that asks Windows for the player's Documents can't be made to fail from a test, so GetSaveFolderPath itself is checked by
// reading it. Run this from the top of the repository, because that's where the source it reads is found from.
#include "save_folder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok" : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static std::string ReadSourceFile(const char *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	std::string text = all.str();
	std::string withoutReturns;

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] != '\r')
		{
			withoutReturns += text[i];
		}
	}

	return withoutReturns;
}

static std::string Between(const std::string &text, const char *from, const char *to)
{
	size_t const start = text.find(from);

	if (start == std::string::npos)
	{
		return std::string();
	}

	size_t const end = text.find(to, start);

	return end == std::string::npos ? std::string() : text.substr(start, end - start);
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	size_t const b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

static bool FolderIsThere(const std::string &path)
{
	DWORD const what = ::GetFileAttributesA(path.c_str());

	return what != INVALID_FILE_ATTRIBUTES && (what & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Only ever these folders, one at a time, and only if they're empty, so this can't take away anything it didn't make.
static void TakeAwayWhatWasMade(const std::string &base)
{
	std::string const top = base + "/My Games/Aliens versus Predator";

	::RemoveDirectoryA((top + "/User_Profiles").c_str());
	::RemoveDirectoryA((top + "/MPConfig").c_str());
	::RemoveDirectoryA(top.c_str());
	::RemoveDirectoryA((base + "/My Games").c_str());
	::RemoveDirectoryA(base.c_str());
}

static void TestMakingIt()
{
	char const *const outputFolder = getenv("RUNNER_TEMP");
	std::string base = std::string(outputFolder ? outputFolder : ".") + "/save_folder_test_files";

	// The answer comes back with forward slashes, so that's what it's compared with
	std::replace(base.begin(), base.end(), '\\', '/');

	TakeAwayWhatWasMade(base);
	::CreateDirectoryA(base.c_str(), NULL);

	std::string const expected = base + "/My Games/Aliens versus Predator/";
	char folder[MAX_PATH * 2] = { 0 };

	std::string where = "something left over";

	Check(SaveFolder_Make(base.c_str(), folder, sizeof(folder), &where) == 0, "the folder gets made");
	Check(where.empty(), "and there's no folder to complain about");
	Check(expected == folder, "and the path to it ends in a slash, ready for a file's name");
	Check(FolderIsThere(base + "/My Games/Aliens versus Predator/User_Profiles"), "the folder for the players' profiles is there");
	Check(FolderIsThere(base + "/My Games/Aliens versus Predator/MPConfig"), "and the one for network game settings");

	folder[0] = 'x';
	Check(SaveFolder_Make(base.c_str(), folder, sizeof(folder), NULL) == 0 && expected == folder, "a folder that's there already is fine");

	std::string backslashes = base;

	std::replace(backslashes.begin(), backslashes.end(), '/', '\\');

	Check(SaveFolder_Make(backslashes.c_str(), folder, sizeof(folder), NULL) == 0 && expected == folder, "Windows' backslashes come back as forward slashes");

	// Exactly enough room, and one short of it
	Check(SaveFolder_Make(base.c_str(), folder, expected.size() + 1, NULL) == 0 && expected == folder, "exactly enough room is enough");

	folder[0] = 'x';
	Check(SaveFolder_Make(base.c_str(), folder, expected.size(), &where) == kSaveFolderTooLong, "one short of that is too long");
	Check(where == expected, "and it says which folder");
	Check(folder[0] == 0, "and leaves no half a path behind");

	folder[0] = 'x';
	Check(SaveFolder_Make((base + "/there is no such folder").c_str(), folder, sizeof(folder), &where) == kSaveFolderCantMake, "a folder that can't be made says so");
	Check(where == base + "/there is no such folder/My Games", "and it says which one");
	Check(folder[0] == 0, "and leaves no half a path behind");
	Check(!FolderIsThere(base + "/there is no such folder"), "and it hasn't made the folder it was told was there");

	folder[0] = 'x';
	Check(SaveFolder_Make(0, folder, sizeof(folder), NULL) == kSaveFolderNothingGiven, "nothing to make it in says so");
	Check(folder[0] == 0, "and leaves it empty");

	folder[0] = 'x';
	Check(SaveFolder_Make("", folder, sizeof(folder), NULL) == kSaveFolderNothingGiven, "and so does an empty name");
	Check(folder[0] == 0, "and leaves it empty");

	Check(SaveFolder_Make(base.c_str(), 0, sizeof(folder), NULL) == kSaveFolderNothingGiven, "nowhere to put the answer says so");

	folder[0] = 'x';
	Check(SaveFolder_Make(base.c_str(), folder, 0, NULL) == kSaveFolderNothingGiven, "and so does no room for one");
	Check(folder[0] == 'x', "without writing in the room it was told it hasn't got");

	TakeAwayWhatWasMade(base);
	Check(!FolderIsThere(base), "this test's folders are gone again");
}

static void TestTheGamesCode()
{
	std::string const code = ReadSourceFile("3dc/utilities.cpp");
	std::string const finding = Between(code, "char *GetSaveFolderPath()", "FILE *avp_fopen");

	Check(!finding.empty(), "GetSaveFolderPath is where it's expected to be");
	Check(!Has(finding, "return NULL") && !Has(finding, "return 0"), "it never gives back nothing, so no caller has to check");
	Check(Has(finding, "if (*saveFolder || noSaveFolder)"), "it doesn't try again once it's failed");
	Check(!Has(finding, "LogErrorString"), "it never asks the log for anything, because the log asks here for the folder");
	Check(Has(finding, "noSaveFolder = true;"), "it writes down that it failed");
	Check(Has(finding, "\"%s%s%s\", whatWentWrong, where.empty() ? \"\" : \": \", where.c_str());"), "and what went wrong, and which folder");
	Check(Has(finding, "sizeof(saveFolderProblem) - 1"), "without running past the end of the space for it");
	Check(Has(finding, "SaveFolder_MakeOne(\"User_Profiles/\", NULL);") && Has(finding, "SaveFolder_MakeOne(\"MPConfig/\", NULL);"), "and the folders for saved games and network settings are made where the files go instead");
	Check(Has(finding, "char documents[MAX_PATH]"), "Documents is found in a space of its own");
	Check(Has(finding, "SHGFP_TYPE_CURRENT, documents)"), "so that a path to Documents is never left where the game's folder goes");
	Check(Has(finding, "SaveFolder_Make(documents, saveFolder, sizeof(saveFolder), &where)"), "and the folder is made by the part this test tries for real");
}

static void TestWhatTheyAreTold()
{
	std::string const making = Between(ReadSourceFile("3dc/logString.cpp"), "static LogWriter *TheLogWriter()", "void ClearLog()");

	Check(ComesBefore(making, "logWriter = new LogWriter;", "GetSaveFolderProblem()"), "the log says what went wrong with the save folder, once there's a log to say it in");
	Check(ComesBefore(making, "GetSaveFolderProblem()", "LogErrorString(problem);"), "as an error");

	std::string const asking = Between(ReadSourceFile("3dc/utilities.cpp"), "char const *GetSaveFolderProblem()", "char *GetSaveFolderPath()");

	Check(Has(asking, "noSaveFolder ? saveFolderProblem : NULL"), "there's only something to say if it went wrong");

	std::string const saving = Between(ReadSourceFile("3dc/avp/savegame.cpp"), "void SaveGame()", "static void EndLoadGame()");
	std::string const cantWrite = Between(saving, "if (file == INVALID_HANDLE_VALUE)", "WriteFile(file");

	Check(!cantWrite.empty(), "the save game's check is where it's expected to be");
	Check(Has(cantWrite, "NewOnScreenMessage(\"GAME NOT SAVED: ITS FILE CANNOT BE WRITTEN\");"), "a save that can't be written tells the player");
	Check(ComesBefore(cantWrite, "NewOnScreenMessage", "return;"), "before it gives up");
	Check(Has(cantWrite, "if (!unlimitedSaves)") && Has(cantWrite, "NumberOfSavesLeft++;"), "and they get back the save it took off them");
	Check(!Has(cantWrite, "GLOBALASSERT"), "and a debug build doesn't stop the game over it");
}

int main()
{
	printf("Making the folder the game keeps its files in:\n");

	TestMakingIt();
	TestTheGamesCode();
	TestWhatTheyAreTold();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
