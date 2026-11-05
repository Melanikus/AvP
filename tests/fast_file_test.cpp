// The test for asking whether a file is there, in 3dc/FastFile.cpp. Written 2026-11-05 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A texture's hand-made map is allowed not to be there, and most aren't. FF_Find writes an error when a file it's asked for isn't in a fast file but shares a place in its table with one that is, so looking for maps
// used to fill the log with errors about nothing. FF_FileIsThere asks the same question and says nothing. This checks both of its answers, that it really does say nothing, that FF_Find still does, and that a name
// sharing a place with another file never gets that other file back, which is what the game's fast file code once did. It also checks that a file inside a fast file runs out at the end of its own bytes, as a real file
// runs out at the end of the file, because a damaged sound with no end of its own would otherwise search on for ever.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this, with the game's own FastFile.cpp and FileStream.cpp, and the program gives back a non-zero result if any check failed. It needs no game files: it makes a tiny fast file of its own, in
// a folder beside the program, and takes it away again.

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <future>
#include <string>
#include <vector>

#include "FastFile.h"
#include "FileStream.h"
#include "utilities.h"
#include "console.h"
#include "wav_chunks.h"

static const char *const kFolder = "fast_file_test_files";
static const char *const kPackedFile = "graphics/envrnmts/derelict/hive01.rim";
static const char *const kLooseFile = "loose_map_Normal.rim";
static const char *const kSoundWithNoData = "sounds/no_data.wav";    // comes right before the next one, whose data chunk it must not find
static const char *const kSoundWithData = "sounds/has_data.wav";

static int checksFailed = 0;
static int errorsWritten = 0;
static std::string lastError;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- What FastFile.cpp and FileStream.cpp ask of the rest of the game. The errors are counted, because whether one was written is the point of this test.

void Con_PrintError(const std::string &errorString)
{
	errorsWritten++;
	lastError = errorString;
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
}

// ---- The test's own files

static void AddNumber(std::string &bytes, uint32_t number)
{
	bytes.append(reinterpret_cast<const char *>(&number), 4);
}

// A chunk of a WAV file: a four-letter name, a size, then the bytes.
static std::string WavChunk(const char *chunkName, const std::string &chunkBytes)
{
	std::string chunk(chunkName, 4);

	AddNumber(chunk, (uint32_t)chunkBytes.size());
	return chunk + chunkBytes;
}

// A fast file: the signature, its version, how many files, how long the list is, how much data, then the list, then the data. The files are one after another, so each one's end is the start of the next.
static void WriteTinyFastFile(const char *name)
{
	struct Packed { const char *name; std::string bytes; };

	std::vector<Packed> const files =
	{
		{ kPackedFile, "data" },
		{ kSoundWithNoData, WavChunk("LIST", "abcd") },
		{ kSoundWithData, WavChunk("data", "abcd") }
	};

	std::string list, data;

	for (size_t i = 0; i < files.size(); i++)
	{
		AddNumber(list, (uint32_t)data.size());                // where its data starts
		AddNumber(list, (uint32_t)files[i].bytes.size());      // how long its data is
		list += files[i].name;
		list.append(((strlen(files[i].name) + 4) & ~3) - strlen(files[i].name), '\0');
		data += files[i].bytes;
	}

	std::string bytes = "RFFL";

	AddNumber(bytes, 0);
	AddNumber(bytes, (uint32_t)files.size());
	AddNumber(bytes, (uint32_t)list.size());
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

static void WriteLooseFile()
{
	FILE *const file = fopen(kLooseFile, "wb");

	if (!file)
	{
		puts("FAILED: the test couldn't write its own loose file");
		exit(1);
	}

	fputs("data", file);
	fclose(file);
}

// A folder of its own beside the program, so that it can't mix with anything in the place it's started from.
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
	remove("fastfile/test.ffl");
	remove(kLooseFile);
	_rmdir("fastfile");
	_chdir("..");
	_rmdir(kFolder);
}

// ---- The checks

// The table has 2,600 places and one file in it, so one name in about 2,600 lands on that file's place, and that's the case that once went wrong. Each try here is either nothing, or that. It finds one by asking until
// an error is written, because that error only comes from there.
static std::string ANameSharingAPlaceWithThePackedFile()
{
	for (int i = 0; i < 100000; i++)
	{
		std::string const name = "not_there_" + std::to_string(i) + "_Normal.rim";
		int const errorsBefore = errorsWritten;

		if (FF_Find(name) != 0)
		{
			return "";
		}

		if (errorsWritten != errorsBefore)
		{
			return name;
		}
	}

	return "";
}

static void TestBeforeTheFastFilesAreRead()
{
	Check(!FF_FileIsThere(kPackedFile), "before the fast files have been read, a packed file isn't there yet");
	Check(FF_Find(kPackedFile) == 0, "and FF_Find has nothing to give back");
	Check(errorsWritten == 0, "and neither of them writes an error");
}

static void TestAskingQuietly()
{
	errorsWritten = 0;

	Check(FF_FileIsThere(kPackedFile), "a file in a fast file is there");
	Check(FF_FileIsThere("GRAPHICS/Envrnmts/Derelict/HIVE01.RIM"), "whatever capitals its name is asked for in");
	Check(FF_FileIsThere(kLooseFile), "a loose file is there");
	Check(!FF_FileIsThere("not_anywhere.rim"), "a file that's in neither place isn't");
	Check(errorsWritten == 0, "and none of that writes an error");

	std::string const shared = ANameSharingAPlaceWithThePackedFile();

	Check(!shared.empty(), "a name can be found that shares a place with the packed file");
	Check(strstr(lastError.c_str(), "is not in a fast file") != 0, "and FF_Find still writes its error about that name, for anything that expects a file to be there");

	errorsWritten = 0;

	Check(!FF_FileIsThere(shared), "that name isn't there");
	Check(errorsWritten == 0, "and asking this way writes no error about it");

	Check(FF_Find(shared) == 0, "FF_Find doesn't hand back the file it shares a place with");
	Check(errorsWritten == 1, "and writes its one error");

	FileStream stream;

	stream.Open(shared, FileStream::FileRead);
	Check(!stream.IsGood(), "so opening that name gets nothing, and a map reader can't be given some other file");
}

// Runs a job that goes round for ever if the code under test is wrong. If it takes too long, the test stops at once, because a job that is stuck can't be asked to finish.
template <class JOB> static bool WithinTimeLimit(JOB job)
{
	std::future<bool> result = std::async(std::launch::async, job);

	if (result.wait_for(std::chrono::seconds(20)) != std::future_status::ready)
	{
		puts("FAILED: a job did not finish in time. In the game, this would be a hang.");
		_exit(1);
	}

	return result.get();
}

static void TestRunningOutInsideAFastFile()
{
	{
		FileStream stream;

		Check(stream.Open(kPackedFile, FileStream::FileRead), "a file in a fast file opens");
		Check(stream.GetUint32BE() == 'data', "and reads as its own bytes");
		Check(!stream.RanOutOfData(), "reading to its very end isn't running out");
		Check(stream.PeekByte() == 0 && stream.RanOutOfData(), "looking past its end gives 0, and that is running out, though the fast file has more in it");
		Check(stream.GetByte() == 0, "reading past its end gives 0, not the next file's first byte");
	}

	{
		FileStream stream;

		Check(stream.Open(kSoundWithData, FileStream::FileRead), "a sound in a fast file opens");
		Check(WithinTimeLimit([&stream] { return FindWavDataChunk(stream); }), "and the search finds its sound");
	}

	{
		// The next file in the fast file does have a sound. Without running out here, this search either finds that one, or goes round for ever reading nothing.
		FileStream stream;

		Check(stream.Open(kSoundWithNoData, FileStream::FileRead), "a sound with no data in it opens");
		Check(!WithinTimeLimit([&stream] { return FindWavDataChunk(stream); }), "and the search stops at the end of its own bytes, and doesn't find the next file's");
		Check(stream.RanOutOfData(), "because it ran out there");
	}
}

int main()
{
	printf("Asking whether a file is there:\n");

	GoToTheTestsOwnFolder();
	WriteTinyFastFile("fastfile/test.ffl");
	WriteLooseFile();

	TestBeforeTheFastFilesAreRead();

	Check(FF_Init(), "the test's own fast file is read");

	TestAskingQuietly();
	TestRunningOutInsideAFastFile();

	TakeAwayTheTestsFiles();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
