// A test of reading past the end of a file, and of finding the sound in a WAV file.
// Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The game's files can be damaged, or made badly on purpose. Two things used to go wrong when one ended early. The
// readers in FileStream gave back whatever happened to be in memory, and three loops kept going: the one that reads a
// name, the one that looks for the sound in a WAV file, and the one that reads the sound bank. The game then hung, and
// a name that never ended would grow until the memory ran out. A file could also claim up to 4 GB of sound, and the game
// stopped when it couldn't get that much memory.
//
// This test writes small files of its own and reads them with the game's real FileStream and FindWavDataChunk. It does
// not touch any game file. The sound bank's own loop and the sizes in the WAV loaders are not run here, because they
// need the whole sound system, so the last part reads the game's source for those. /FC in this test's settings makes
// __FILE__ the whole path of this file, which is how the source is found.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file and 3dc/FileStream.cpp, with the settings in tests/wav_chunks_test.args. The
// program prints a line for each check, and gives back a non-zero result if any check failed. A loop that never ends
// can't print anything, so the two kinds of job that could hang run against a time limit, and going over it fails.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#include <chrono>
#include <fstream>
#include <future>
#include <sstream>
#include <string>

#include "FileStream.h"
#include "FastFile.h"
#include "wav_chunks.h"

// FileStream can also read from the game's archives. This test uses plain files, so these two are stand-ins that
// are never asked for anything: every file the test opens exists on its own.
FastFileHandle* FF_Find(const std::string &)
{
	return 0;
}

const std::string & FF_GetFastFileName(int32_t)
{
	static const std::string noName;
	return noName;
}

static const char *const kTestFile = "wav_chunks_test.tmp";
static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("%s: %s\n", passed ? "ok" : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

// Writes the bytes the next check will read.
static void WriteTestFile(const std::string &bytes)
{
	FILE *file = fopen(kTestFile, "wb");
	if (!file)
	{
		puts("FAILED: could not write the test's own file");
		_exit(1);
	}
	fwrite(bytes.data(), 1, bytes.size(), file);
	fclose(file);
}

// Opens that file with the game's own reader.
static bool OpenTestFile(FileStream &stream)
{
	return stream.Open(kTestFile, FileStream::FileRead, FileStream::SkipFastFileCheck);
}

// Fills the part of memory the next reader will use with a value that isn't 0. Without this, a reader that forgot to
// start at 0 could pass by luck, just because that memory happened to hold 0 already.
#pragma optimize("", off)
static void FillMemoryWithRubbish()
{
	volatile char rubbish[512];
	for (int i = 0; i < (int)sizeof(rubbish); i++)
	{
		rubbish[i] = (char)0xAB;
	}
}
#pragma optimize("", on)

// Runs a job that goes round for ever if the code under test is wrong. If it takes too long, the test stops and fails
// at once. It has to stop the whole program, because a job that is stuck can't be asked to finish.
template <class JOB> static bool WithinTimeLimit(JOB job)
{
	std::future<bool> result = std::async(std::launch::async, job);
	if (result.wait_for(std::chrono::seconds(20)) != std::future_status::ready)
	{
		puts("FAILED: a job did not finish in time. In the game, this would be a hang.");
		remove(kTestFile);
		_exit(1);
	}
	return result.get();
}

// A chunk of a WAV file: a four-letter name, a size, then the bytes. The size is given separately because a damaged
// file can claim any size it likes.
static std::string Chunk(const char *name, uint32_t claimedSize, const std::string &bytes)
{
	std::string chunk(name, 4);
	chunk.append(reinterpret_cast<const char *>(&claimedSize), 4);
	return chunk + bytes;
}

// Looks for the sound in the bytes given.
static bool FindsSound(const std::string &bytes)
{
	WriteTestFile(bytes);
	return WithinTimeLimit([]
	{
		FileStream stream;
		return OpenTestFile(stream) && FindWavDataChunk(stream);
	});
}

static void TestReadingToTheEnd()
{
	WriteTestFile("ab");
	FileStream stream;
	Check(OpenTestFile(stream), "the test file opens");

	Check(stream.GetByte() == 'a' && !stream.RanOutOfData(), "the first byte is read, with no end reached");
	Check(stream.GetByte() == 'b' && !stream.RanOutOfData(), "so is the last byte: reaching the end is not going past it");

	FillMemoryWithRubbish();
	Check(stream.GetByte() == 0, "a byte read past the end is 0");
	Check(stream.RanOutOfData(), "and the stream says it ran out");

	stream.Seek(0, FileStream::SeekStart);
	Check(stream.GetByte() == 'a' && stream.RanOutOfData(), "having run out stays true, even after moving back");
}

static void TestEveryReaderGivesZero()
{
	WriteTestFile("");
	FileStream stream;
	Check(OpenTestFile(stream), "an empty file opens");

	FillMemoryWithRubbish();
	Check(stream.GetByte() == 0, "GetByte gives 0 in an empty file");
	FillMemoryWithRubbish();
	Check(stream.PeekByte() == 0, "PeekByte gives 0");
	FillMemoryWithRubbish();
	Check(stream.GetUint16LE() == 0, "GetUint16LE gives 0");
	FillMemoryWithRubbish();
	Check(stream.GetUint16BE() == 0, "GetUint16BE gives 0");
	FillMemoryWithRubbish();
	Check(stream.GetInt32LE() == 0, "GetInt32LE gives 0");
	FillMemoryWithRubbish();
	Check(stream.GetUint32LE() == 0, "GetUint32LE gives 0");
	FillMemoryWithRubbish();
	Check(stream.GetUint32BE() == 0, "GetUint32BE gives 0");
	FillMemoryWithRubbish();
	Check(stream.GetUint64LE() == 0, "GetUint64LE gives 0");
}

static void TestAValueCutShort()
{
	WriteTestFile("ab");
	FileStream stream;
	OpenTestFile(stream);

	FillMemoryWithRubbish();
	uint32_t value = stream.GetUint32LE();
	Check(value == ('a' | ('b' << 8)), "a value cut off by the end keeps what was there, with 0 for the rest");
	Check(stream.RanOutOfData(), "and the stream says it ran out");
}

static void TestPeekByte()
{
	WriteTestFile("xy");
	FileStream stream;
	OpenTestFile(stream);

	Check(stream.PeekByte() == 'x', "PeekByte shows the next byte");
	Check(stream.PeekByte() == 'x', "and showing it does not use it up");
	Check(stream.GetByte() == 'x' && stream.GetByte() == 'y', "so the real reads get the same bytes");
	Check(!stream.RanOutOfData(), "and no read has gone past the end yet");

	FillMemoryWithRubbish();
	Check(stream.PeekByte() == 0, "at the end, PeekByte gives 0");
	Check(stream.GetCurrentPos() == 2, "and it does not step back");
}

// The way ExtractWavFile and FastFile.cpp read a name: until a zero byte.
static void TestANameThatNeverEnds()
{
	WriteTestFile("sound.wav");
	std::string name;
	bool finished = WithinTimeLimit([&name]
	{
		FileStream stream;
		if (!OpenTestFile(stream))
		{
			return false;
		}
		while (stream.PeekByte() != '\0')
		{
			name += stream.GetByte();
		}
		return true;
	});
	Check(finished && name == "sound.wav", "a name with no zero byte stops at the end of the file");
}

static void TestFindingTheSound()
{
	const std::string sound = "hello";

	Check(FindsSound(Chunk("data", 5, sound)), "the sound is found in the first chunk");
	Check(FindsSound(Chunk("LIST", 4, "abcd") + Chunk("data", 5, sound)), "and after another chunk");

	// These braces close the file again at the "}": the game's reader holds a file for itself while it is open, so
	// the next check couldn't write it otherwise.
	{
		WriteTestFile(Chunk("LIST", 4, "abcd") + Chunk("fact", 0, "") + Chunk("data", 5, sound));
		FileStream stream;
		OpenTestFile(stream);
		Check(FindWavDataChunk(stream) && stream.GetUint32LE() == 5, "and it stops just before the sound's size");
		Check(!stream.RanOutOfData(), "a good file never reaches the end while searching");
	}

	Check(!FindsSound(""), "an empty file has no sound");
	Check(!FindsSound("da"), "nor does a file that stops in the middle of a chunk's name");
	Check(!FindsSound(Chunk("LIST", 4, "abcd")), "nor does a file with no data chunk");
	Check(!FindsSound(Chunk("LIST", 0xFFFFFFFF, "abcd")), "a chunk claiming 4 GB ends the search at the end of the file");
	Check(!FindsSound(Chunk("LIST", 100, "abcd")), "and so does one claiming a little more than the file has");

	std::string manyEmptyChunks;
	for (int i = 0; i < 1000; i++)
	{
		manyEmptyChunks += Chunk("LIST", 0, "");
	}
	Check(!FindsSound(manyEmptyChunks), "a thousand empty chunks still move forward to the end");
}

static void TestFailedReadsAndStartingAfresh()
{
	WriteTestFile("ab");

	{
		FileStream stream;

		Check(OpenTestFile(stream), "the test file opens again");
		stream.Seek(0, FileStream::SeekEnd);
		Check(stream.GetByte() == 0 && stream.RanOutOfData(), "a read at the end of a file runs out");

		stream.Close();
		Check(OpenTestFile(stream), "the same reader can open a file again");
		Check(!stream.RanOutOfData(), "and that starts afresh, because the new file hasn't run out");
	}

	{
		// a file opened for writing can't be read, so this read fails outright, which isn't the same as finding the end
		FileStream stream;

		Check(stream.Open(kTestFile, FileStream::FileWrite), "a file opens for writing");
		Check(!stream.RanOutOfData(), "with nothing run out yet");
		Check(stream.GetByte() == 0, "a read that fails gives 0");
		Check(stream.RanOutOfData(), "and counts as running out, or a loop could go on for ever");
	}
}

// The text of a file in the repository. Windows line endings are taken out, because a copy of the repository can have them or not.
static std::string ReadSourceFile(const char *name)
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
	std::string::size_type place = 0;

	while ((place = text.find('\r', place)) != std::string::npos)
	{
		text.erase(place, 1);
	}

	return text;
}

// The text from one marker up to the next one after it.
//
// If either marker isn't there, that's a failed check of its own, named by the marker, and the text given back is empty. An empty text alone
// wasn't enough: a check that something is NOT inside it, like "no while (1)", passes on nothing, and so can't tell code that moved from code
// that's right. A marker goes missing when the code it names is moved or renamed, and the test then has to be pointed at where it went.
// (Modified 2026-11-08 by Melanikus for the AvP port: a missing marker is now a failed check, because moving GetALFormat to openal_shared.h
// took away this file's end marker for AllocateWavData without anything saying so.)
static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);

	if (start == std::string::npos)
	{
		printf("FAILED: the code this test reads has no \"%s\"\n", from);
		checksFailed++;
		return "";
	}

	std::string::size_type const end = text.find(to, start);

	if (end == std::string::npos)
	{
		printf("FAILED: the code this test reads has no \"%s\" after \"%s\"\n", to, from);
		checksFailed++;
		return "";
	}

	return text.substr(start, end - start);
}

static bool Has(const std::string &text, const char *words)
{
	return text.find(words) != std::string::npos;
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	std::string::size_type const one = text.find(first);
	std::string::size_type const two = text.find(second);

	return one != std::string::npos && two != std::string::npos && one < two;
}

static int CountOf(const std::string &text, const char *words)
{
	int count = 0;

	for (std::string::size_type place = text.find(words); place != std::string::npos; place = text.find(words, place + 1))
	{
		count++;
	}

	return count;
}

static void TestTheGamesLoops()
{
	std::string const audio = ReadSourceFile("3dc/avp/win95/openal_audio.cpp");
	std::string const wavFile = Between(audio, "int LoadWavFile(int soundNum, const std::string &fileName)", "int ExtractWavFile(int soundNum, FileStream &fStream)");
	std::string const bankWav = Between(audio, "int ExtractWavFile(int soundNum, FileStream &fStream)", "alBufferData");

	Check(Has(wavFile, "if (!FindWavDataChunk(fStream))") && Has(bankWav, "if (!FindWavDataChunk(fStream))"), "both WAV loaders look for the sound with the search tested above");
	Check(!Has(wavFile, "while (1)") && !Has(bankWav, "while (1)"), "and neither has a search of its own");

	std::string const bank = ReadSourceFile("3dc/avp/psndproj.cpp");

	Check(ComesBefore(bank, "if (!fStream.IsGood())", "soundIndex = fStream.GetByte();"), "a sound bank that can't be opened is refused before anything is read from it");
	Check(Has(bank, "while (((soundIndex != 0xff) || (pitch != -1)) && !fStream.RanOutOfData())"), "and its loop stops at the end of the file, end mark or not");
	Check(ComesBefore(bank, "if (!ExtractWavFile(soundIndex, fStream))", "GameSounds[soundIndex].loaded = 1;"), "a sound the bank can't read isn't counted as loaded");
	Check(Has(bank, "GameSounds[soundIndex].wavName.clear();"), "and its name is forgotten, because a name left there would make it look loaded, and it would stay silent instead of trying its own WAV");
	Check(Has(bank, "wavName.clear();\n\t\t\tsoundIndex = fStream.GetByte();\n\t\t\tpitch      = fStream.GetByte();\n\t\t\tcontinue;"), "and the bank goes on to its next sound");

	Check(Has(ReadSourceFile("3dc/FastFile.cpp"), "for (uint32_t i = 0; i < nFiles && !file.RanOutOfData(); i++)"), "an archive's list of files stops at the end of the file, whatever count it claims");
}

static void TestTheSizesInSoundFiles()
{
	std::string const audio = ReadSourceFile("3dc/avp/win95/openal_audio.cpp");
	// AllocateWavData runs up to the WAV file loader, the next function in openal_audio.cpp
	std::string const allocating = Between(audio, "static bool AllocateWavData(int soundNum, uint32_t nAudioBytes)", "int LoadWavFile(int soundNum, const std::string &fileName)");

	Check(Has(audio, "static const uint32_t kMaxWavDataBytes = 64 * 1024 * 1024;"), "a WAV may hold 64 MB of sound at most");
	Check(ComesBefore(allocating, "if (nAudioBytes > kMaxWavDataBytes)", "new(std::nothrow) uint8_t[nAudioBytes]"), "and that's checked before the memory is asked for");
	Check(Has(allocating, "GameSounds[soundNum].audioBuffer != NULL"), "and memory that can't be found loses this sound, and doesn't stop the game");
	Check(CountOf(audio, "new uint8_t[nAudioBytes]") == 0 && CountOf(audio, "AllocateWavData(soundNum, nAudioBytes)") == 2, "both loaders get their memory that way, and no other");

	std::string const bankWav = Between(audio, "int ExtractWavFile(int soundNum, FileStream &fStream)", "alBufferData");

	Check(ComesBefore(bankWav, "if (!AllocateWavData(soundNum, nAudioBytes))", "fStream.Seek(startOffset + totalChunkSize, FileStream::SeekStart);\n\t\treturn ReturnInvalidWAV();"), "a sound refused from the bank is stepped over, so the next one is still found");

	std::string const scream = ReadSourceFile("3dc/avp/scream.cpp");
	std::string const list = Between(scream, "static void* AllocateScreamList(int count, size_t size)", "void CharacterSoundEffects::LoadSounds");

	Check(ComesBefore(list, "if (count < 0 || count > (int)kMaxScreamFileSize)", "PoolAllocateMem(count * size)"), "a count from a character sound file is checked where it becomes a request for memory");
	Check(CountOf(scream, "AllocateScreamList(") == 4 && !Has(scream, "PoolAllocateMem(num_voice") && !Has(scream, "PoolAllocateMem(cat->num_sounds"), "and all three lists get their memory that way");
}

int main()
{
	TestReadingToTheEnd();
	TestEveryReaderGivesZero();
	TestAValueCutShort();
	TestPeekByte();
	TestANameThatNeverEnds();
	TestFindingTheSound();
	TestFailedReadsAndStartingAfresh();
	TestTheGamesLoops();
	TestTheSizesInSoundFiles();

	remove(kTestFile);

	if (checksFailed)
	{
		printf("The WAV chunks test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The WAV chunks test passed");
	return 0;
}
