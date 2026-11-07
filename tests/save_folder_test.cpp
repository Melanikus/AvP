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
#include "save_file_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

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
	// A folder of this run's own, inside the folder the test runs in: something an earlier run left behind must not count against this one,
	// and this only takes away what it knows it made. It's named by nothing but this program's number, so no setting from outside can point
	// it anywhere else.
	std::string base = "save_folder_test_files_" + std::to_string(::GetCurrentProcessId());

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
	std::string const log = ReadSourceFile("3dc/logString.cpp");
	std::string const clearing = Between(log, "void ClearLog()\n{\n\tif (LogWriter *writer = TheLogWriter())", "void WriteToLog");

	Check(ComesBefore(clearing, "writer->Empty();", "GetSaveFolderProblem()"), "the log says what went wrong with the save folder after it's been emptied, because that wipes anything before it");
	Check(ComesBefore(clearing, "GetSaveFolderProblem()", "LogErrorString(problem);"), "as an error");
	Check(!Has(Between(log, "static LogWriter *TheLogWriter()", "void ClearLog()"), "GetSaveFolderProblem"), "and not when the log is made, where it would be wiped");
	Check(!clearing.empty(), "the function that empties the log is where it's expected to be");
	Check(Has(log, "filePath = GetSaveFolderPath();"), "the log uses the save folder as it comes, because it's never null");

	std::string const startingOn = Between(ReadSourceFile("3dc/avp/win95/winmain.cpp"), "SetWindowTitle();", "LogString(");

	Check(Has(startingOn, "ClearLog();"), "the game empties the log first thing, so that's when it's said");

	std::string const asking = Between(ReadSourceFile("3dc/utilities.cpp"), "char const *GetSaveFolderProblem()", "char *GetSaveFolderPath()");

	Check(Has(asking, "noSaveFolder ? saveFolderProblem : NULL"), "there's only something to say if it went wrong");

	std::string const saving = Between(ReadSourceFile("3dc/avp/savegame.cpp"), "void SaveGame()", "static void EndLoadGame()");
	std::string const cantWrite = Between(saving, "if (!saved)", "TEXTSTRING_SAVEGAME_GAMESAVED");

	Check(!cantWrite.empty(), "the save game's check is where it's expected to be");
	Check(!Has(saving, "avp_CreateFile") && !Has(saving, "WriteFile(file"), "the save game doesn't open the slot's file itself, because that empties it");
	Check(Has(saving, "packagePtr && SaveFile_WriteSafely(filename, SaveInfo.BufferStart, headerLength, packagePtr, packagePtr->CompressedDataSize + sizeof(HuffmanPackage));"), "it goes through the safe way, with both parts, and only if it could compress them");
	Check(Has(saving, "DeallocateMem(packagePtr);"), "and lets go of the compressed copy");
	// Compressing never gives back nothing for want of memory: AllocMem stops the game with a message instead, so it can't write through nothing
	Check(Has(ReadSourceFile("3dc/win95/huffman.cpp"), "outpackage = (HuffmanPackage*)AllocMem(sizeof(HuffmanPackage) + mostBytes);"), "compressing takes its memory from AllocMem, which stops the game rather than give back nothing");
	Check(Has(cantWrite, "NewOnScreenMessage(\"GAME NOT SAVED: ITS FILE CANNOT BE WRITTEN\");"), "a save that can't be written tells the player");
	Check(ComesBefore(cantWrite, "NewOnScreenMessage", "return;"), "before it gives up");
	Check(Has(cantWrite, "if (!unlimitedSaves)") && Has(cantWrite, "NumberOfSavesLeft++;"), "and they get back the save it took off them");
	Check(!Has(cantWrite, "GLOBALASSERT"), "and a debug build doesn't stop the game over it");
}

static std::string ReadWholeFile(const std::string &name)
{
	std::ifstream file(name.c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

static bool FileIsThere(const std::string &name)
{
	return ::GetFileAttributesA(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Something has a file for a moment, the way a virus scanner would, and lets go of it about 150 ms after this starts. The tries come 100 ms apart, so the first has to fail and a later one
// has to work. A machine under load could wake that thread too late for all of them, so this goes three times before calling it a failure. Never trying again fails all three.
template <class StartHolding, class LetGo, class Try>
static bool WorksOnceItsLetGo(StartHolding startHolding, LetGo letGo, Try tryIt)
{
	for (int attempt = 0; attempt < 3; attempt++)
	{
		startHolding();

		std::thread scanner([&letGo]()
		{
			::Sleep(150);
			letGo();
		});

		bool const worked = tryIt();

		scanner.join();

		if (worked)
		{
			return true;
		}
	}

	return false;
}

// Tries again when the file is only busy, on the two answers Windows gives. Each has to be what Windows really says, or this wouldn't be trying what it claims to.
static void TestTryingAgain(std::string const &base, std::string const &slot, std::string const &newFile)
{
	std::string const made = base + "/made.new";
	std::ofstream(made.c_str(), std::ios::binary) << "made";

	// The new file being read, which is what a scanner does just after it's written
	HANDLE scanning = ::CreateFileA(made.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	Check(scanning != INVALID_HANDLE_VALUE, "the test can hold the new file open");
	Check(!::MoveFileExA(made.c_str(), slot.c_str(), MOVEFILE_REPLACE_EXISTING) && ::GetLastError() == ERROR_SHARING_VIOLATION, "Windows really does say it's being shared");
	::CloseHandle(scanning);

	bool const afterSharing = WorksOnceItsLetGo(
		[&]() { scanning = ::CreateFileA(made.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); },
		[&]() { ::CloseHandle(scanning); },
		[&]() { return SaveFile_TakeThePlaceOf(made.c_str(), slot.c_str()); });

	Check(afterSharing, "and a new file that's let go of in time still takes the old one's place");
	Check(ReadWholeFile(slot) == "made", "with what it holds");
	Check(!FileIsThere(made), "and it's gone from where it was");

	// The old file refusing, which Windows calls being denied
	std::ofstream(made.c_str(), std::ios::binary) << "made again";
	::SetFileAttributesA(slot.c_str(), FILE_ATTRIBUTE_READONLY);
	Check(!::MoveFileExA(made.c_str(), slot.c_str(), MOVEFILE_REPLACE_EXISTING) && ::GetLastError() == ERROR_ACCESS_DENIED, "Windows really does say it's denied");

	bool const afterDenied = WorksOnceItsLetGo(
		[&]() { ::SetFileAttributesA(slot.c_str(), FILE_ATTRIBUTE_READONLY); },
		[&]() { ::SetFileAttributesA(slot.c_str(), FILE_ATTRIBUTE_NORMAL); },
		[&]() { return SaveFile_TakeThePlaceOf(made.c_str(), slot.c_str()); });

	Check(afterDenied, "and a file that's let go of in time still takes the old one's place then too");
	Check(ReadWholeFile(slot) == "made again", "with what it holds");

	// The whole save, with something holding the old one
	HANDLE holding = INVALID_HANDLE_VALUE;
	bool const wholeSave = WorksOnceItsLetGo(
		[&]() { holding = ::CreateFileA(slot.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); },
		[&]() { ::CloseHandle(holding); },
		[&]() { return SaveFile_WriteSafely(slot.c_str(), "wo", 2, "rked", 4); });

	Check(wholeSave, "a whole save goes through if the old one is let go of in time");
	Check(ReadWholeFile(slot) == "worked", "and takes its place");
	Check(!FileIsThere(newFile), "with nothing left beside it");

	// Never let go of. It gives up, and it did try: four waits come to 400 ms, and the clock can run a little short.
	holding = ::CreateFileA(slot.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	ULONGLONG const started = ::GetTickCount64();
	bool const gaveUp = !SaveFile_WriteSafely(slot.c_str(), "no", 2, "pe", 2);
	ULONGLONG const took = ::GetTickCount64() - started;

	::CloseHandle(holding);
	Check(gaveUp, "a file that's never let go of is a failed save");
	Check(took >= 300, "after it's tried for a while");
	Check(took < 5000, "and not for long");
	Check(ReadWholeFile(slot) == "worked", "and the old save is just as it was");

	// A new file left behind by an earlier save, because a scanner had it when that save tried to take it away
	std::ofstream(newFile.c_str(), std::ios::binary) << "left over from before, and longer than the save";
	Check(SaveFile_WriteSafely(slot.c_str(), "ne", 2, "xt", 2), "the next save writes over a new file left behind");
	Check(ReadWholeFile(slot) == "next", "and none of what it held gets into the save");
	Check(!FileIsThere(newFile), "and it's gone");

	// And if something still has that one, there's nowhere to write
	std::ofstream(newFile.c_str(), std::ios::binary) << "left over";
	holding = ::CreateFileA(newFile.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	Check(!SaveFile_WriteSafely(slot.c_str(), "no", 2, "pe", 2), "a new file left behind that's still held is a failed save");
	::CloseHandle(holding);
	Check(ReadWholeFile(slot) == "next", "and the old save is just as it was");
	Check(FileIsThere(newFile), "and what was held can't be taken away, which is why that comment says what it says");
	::DeleteFileA(newFile.c_str());
}

// On real files, because what matters here is what's on the disk afterwards.
static void TestWritingASave()
{
	// This run's own folder, inside the folder the test runs in, named by nothing but this program's number
	std::string base = "save_write_test_files_" + std::to_string(::GetCurrentProcessId());
	std::string const slot = base + "/slot_1.sav";
	std::string const newFile = slot + ".new";

	::DeleteFileA(slot.c_str());
	::DeleteFileA(newFile.c_str());
	::RemoveDirectoryA(base.c_str());
	::CreateDirectoryA(base.c_str(), NULL);

	Check(SaveFile_WriteSafely(slot.c_str(), "abc", 3, "de", 2), "a save gets written");
	Check(ReadWholeFile(slot) == "abcde", "with both of its parts, in order");
	Check(!FileIsThere(newFile), "and nothing else left beside it");

	Check(SaveFile_WriteSafely(slot.c_str(), "xy", 2, "z", 1), "a save over an old one gets written");
	Check(ReadWholeFile(slot) == "xyz", "and takes its place, all of it");
	Check(!FileIsThere(newFile), "with nothing else left beside it");

	// Windows refuses to read from a null pointer, which is a real failed write without needing a full disk
	Check(!SaveFile_WriteSafely(slot.c_str(), NULL, 5, "z", 1), "a first part that can't be written is a failed save");
	Check(ReadWholeFile(slot) == "xyz", "and the old save is just as it was");
	Check(!FileIsThere(newFile), "with nothing left behind");

	Check(!SaveFile_WriteSafely(slot.c_str(), "abc", 3, NULL, 5), "so is a second part that can't be written");
	Check(ReadWholeFile(slot) == "xyz", "and the old save is just as it was, though the first part was written");
	Check(!FileIsThere(newFile), "with nothing left behind");

	// Something else holding the old file stops the new one taking its place
	HANDLE const held = ::CreateFileA(slot.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	Check(held != INVALID_HANDLE_VALUE, "the test can hold the old save open");
	Check(!SaveFile_WriteSafely(slot.c_str(), "abc", 3, "de", 2), "a new save that can't take the old one's place is a failed save");
	::CloseHandle(held);
	Check(ReadWholeFile(slot) == "xyz", "and the old save is just as it was");
	Check(!FileIsThere(newFile), "with nothing left behind");

	std::string const nowhere = base + "/there is no such folder/slot_1.sav";

	Check(!SaveFile_WriteSafely(nowhere.c_str(), "abc", 3, "de", 2), "a save with no folder to go in is a failed save");
	Check(!FileIsThere(nowhere), "and it isn't there");
	Check(!SaveFile_WriteSafely(NULL, "abc", 3, "de", 2), "and so is one with no name");
	Check(!SaveFile_WriteSafely("", "abc", 3, "de", 2), "or an empty name");

	std::string const code = ReadSourceFile("3dc/save_file_write.h");

	Check(Has(code, "&& written == size;"), "Windows writing less than it was asked to counts as failing");
	Check(Has(code, "::FlushFileBuffers(file) != 0;"), "and so does the disk refusing it when it's flushed");
	Check(Has(code, "if (!::CloseHandle(file))"), "or when the file is closed");
	Check(Has(code, "why != ERROR_SHARING_VIOLATION && why != ERROR_ACCESS_DENIED"), "taking the old one's place tries again only when something has the file open for a moment");
	Check(Has(code, "if (tries + 1 < kSaveFileTries)"), "and doesn't wait after the last try");

	TestTryingAgain(base, slot, newFile);

	::DeleteFileA(slot.c_str());
	::RemoveDirectoryA(base.c_str());
	Check(!FileIsThere(base), "this test's files are gone again");
}

int main()
{
	printf("Making the folder the game keeps its files in:\n");

	TestMakingIt();
	TestTheGamesCode();
	TestWhatTheyAreTold();
	TestWritingASave();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
