// The test of what a fast file's list may say, in 3dc/FastFile.cpp. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A fast file starts with a list saying where each packed file is and how long it is. An archive can come from anywhere, so a place in it could be past its end, or be so large that the sum
// wraps round and lands on some other file. This writes small archives saying each of those, loads them with the game's real FF_Init, and checks what can be found afterwards.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the game's own FastFile.cpp and FileStream.cpp, and the program gives back a non-zero result if anything failed. It writes its files in a folder beside the
// program, and takes them away again.

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "FastFile.h"
#include "FileStream.h"
#include "utilities.h"
#include "console.h"

static const char *const kFolder = "fast_file_list_test_files";

static int checksFailed = 0;
static std::vector<std::string> errors;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- What FastFile.cpp and FileStream.cpp ask of the rest of the game

void Con_PrintError(const std::string &errorString)
{
	errors.push_back(errorString);
}

void Con_PrintMessage(const std::string &)
{
}

namespace Util
{
	int LowercaseAndChangeSlash(char c)
	{
		if (c == '\\')
		{
			return '/';
		}

		return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
	}

	std::string IntToString(const int value)
	{
		return std::to_string(value);
	}

	void FtoBslash(std::string &text)
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			text[i] = (text[i] == '/') ? '\\' : text[i];
		}
	}
}

// ---- The test's own files

static void AddNumber(std::string &bytes, uint32_t number)
{
	bytes.append(reinterpret_cast<const char *>(&number), 4);
}

static void AddEntry(std::string &list, uint32_t place, uint32_t length, const char *name)
{
	AddNumber(list, place);
	AddNumber(list, length);
	list += name;
	list.append(((strlen(name) + 4) & ~3) - strlen(name), '\0');
}

// The signature, its version, how many files, how long the list says it is, how much data, then the list, then the data.
static void WriteArchive(const char *name, uint32_t files, std::string const &list, uint32_t listSays, std::string const &data)
{
	std::string bytes = "RFFL";

	AddNumber(bytes, 0);
	AddNumber(bytes, files);
	AddNumber(bytes, listSays);
	AddNumber(bytes, (uint32_t)data.size());
	bytes += list;
	bytes += data;

	FILE *const file = fopen(name, "wb");

	if (!file)
	{
		puts("FAILED: the test couldn't write its own fast file");
		exit(1);
	}

	fwrite(bytes.data(), 1, bytes.size(), file);
	fclose(file);
}

static void GoToTheTestsOwnFolder()
{
	std::string folder = _pgmptr ? _pgmptr : "";
	std::string::size_type const lastSlash = folder.find_last_of("\\/");

	if (lastSlash != std::string::npos)
	{
		folder.resize(lastSlash);
		_chdir(folder.c_str());
	}

	_mkdir(kFolder);
	_chdir(kFolder);
	_mkdir("fastfile");
}

static void TakeAwayTheTestsFiles()
{
	remove("fastfile/first.ffl");
	remove("fastfile/second.ffl");
	remove("fastfile/third.ffl");
	_rmdir("fastfile");
	_chdir("..");
	_rmdir(kFolder);
}

static int ErrorsSaying(std::string const &words)
{
	int count = 0;

	for (size_t i = 0; i < errors.size(); i++)
	{
		count += (errors[i].find(words) != std::string::npos) ? 1 : 0;
	}

	return count;
}

int main()
{
	GoToTheTestsOwnFolder();

	std::string const data(100, 'x');

	// Packed files, some of which can't be there
	std::string list;
	AddEntry(list, 0, 4, "first.rim");
	AddEntry(list, 96, 4, "to-the-end.rim");
	AddEntry(list, 100, 0, "nothing-at-the-end.rim");
	AddEntry(list, 97, 4, "one-past.rim");
	AddEntry(list, 101, 0, "starts-past.rim");
	AddEntry(list, 0, 0xFFFFFFFF, "longest.rim");
	AddEntry(list, 0xFFFFFFF0, 4, "wraps.rim");
	AddEntry(list, 50, 0xFFFFFFF0, "length-wraps.rim");
	AddEntry(list, 10, 4, "after-them.rim");
	WriteArchive("fastfile/first.ffl", 9, list, (uint32_t)list.size(), data);

	// Nothing wrong with this one
	std::string goodList;
	AddEntry(goodList, 0, 100, "whole.rim");
	WriteArchive("fastfile/second.ffl", 1, goodList, (uint32_t)goodList.size(), data);

	// Its list says it's far longer than it is, which, with a place from the list, wraps round to a place inside the archive
	std::string wrapList;
	AddEntry(wrapList, 0x200, 4, "list-wraps.rim");
	WriteArchive("fastfile/third.ffl", 1, wrapList, 0xFFFFFF00, data);

	Check(FF_Init(), "the archives load");

	printf("What's in an archive is found:\n");

	FastFileHandle const *const first = FF_Find("first.rim");
	Check(first != 0 && first->fileOffset == 20 + list.size() && first->fileSize == 4, "a packed file inside its archive is found, at its own place");
	Check(FF_Find("to-the-end.rim") != 0, "and one that ends exactly at the end of the archive");
	Check(FF_Find("nothing-at-the-end.rim") != 0, "and one with no bytes, at the very end");
	Check(FF_Find("after-them.rim") != 0, "and one listed after the ones that are left out");

	FastFileHandle const *const whole = FF_Find("whole.rim");
	Check(whole != 0 && whole->fileSize == 100, "and one that fills an archive with nothing wrong");

	printf("What can't be in it isn't:\n");

	Check(ErrorsSaying("first.ffl lists a file that runs past its end") == 1, "the log says so, once for the archive, with four things wrong in it");
	Check(ErrorsSaying("one-past.rim") == 1, "and names the first one");
	Check(ErrorsSaying("third.ffl lists a file that runs past its end") == 1, "and it says so for another archive");
	Check(ErrorsSaying("second.ffl") == 0, "and says nothing about an archive with nothing wrong");

	Check(FF_Find("one-past.rim") == 0, "one byte past the end is left out");
	Check(FF_Find("starts-past.rim") == 0, "and one that starts past the end, though it has no bytes");
	Check(FF_Find("longest.rim") == 0, "and one as long as a file can say");
	Check(FF_Find("wraps.rim") == 0, "and one whose place wraps round to the start");
	Check(FF_Find("length-wraps.rim") == 0, "and one whose length would wrap");
	Check(FF_Find("list-wraps.rim") == 0, "and one whose list's length wraps it round");

	TakeAwayTheTestsFiles();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
