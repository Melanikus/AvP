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

	std::string const header = ReadSourceFile("3dc/avp/win95/wav_chunks.h");
	Check(Has(wavFile, "WavHeader_Read(fStream, kMaxWavDataBytes, &header)") && Has(bankWav, "WavHeader_Read(fStream, kMaxWavDataBytes, &header)"), "both WAV loaders read the header with the reader tested above");
	Check(Has(Between(header, "inline char const *WavHeader_Read(", "#endif"), "if (!FindWavDataChunk(stream))"), "which looks for the sound with the search tested above");
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

// ---- A WAV'S HEADER (wav_chunks.h, WavHeader_Read). Added 2026-11-27 by Melanikus (Drake). ----

static void Put16(std::string &to, uint16_t value)
{
	to.append(reinterpret_cast<const char *>(&value), 2);
}

static void Put32(std::string &to, uint32_t value)
{
	to.append(reinterpret_cast<const char *>(&value), 4);
}

// A WAV file's bytes. As given, it's like most of the game's own: plain PCM, mono, 8 bits, 11,025 a second, and 100 bytes of sound. A size can be
// claimed apart from the bytes there, as a damaged file can claim any it likes.
struct WavMaker
{
	uint16_t tag = 1;
	uint16_t channels = 1;
	uint32_t rate = 11025;
	uint32_t avg = 11025;
	uint16_t align = 1;
	uint16_t bits = 8;
	uint32_t formatBytes = 16;     // 18 puts cbSize after the 16
	uint16_t cbSize = 0;
	std::string before;            // chunks before the sound
	std::string sound = std::string(100, 'x');
	int64_t claimedSound = -1;     // the data chunk's size, when it isn't the sound's
	int64_t claimedRiff = -1;      // the RIFF chunk's, when it isn't the true one
	std::string after;             // what comes after the RIFF chunk: the next sound, in a bank

	std::string Bytes() const
	{
		std::string format;
		Put16(format, tag);
		Put16(format, channels);
		Put32(format, rate);
		Put32(format, avg);
		Put16(format, align);
		Put16(format, bits);

		if (formatBytes == 18)
		{
			Put16(format, cbSize);
		}

		std::string const body = "WAVE" + Chunk("fmt ", formatBytes, format) + before + Chunk("data", claimedSound < 0 ? (uint32_t)sound.size() : (uint32_t)claimedSound, sound);
		std::string riff = "RIFF";
		Put32(riff, claimedRiff < 0 ? (uint32_t)body.size() : (uint32_t)claimedRiff);
		return riff + body + after;
	}
};

static uint32_t const kTheMostSound = 64 * 1024 * 1024;   // openal_audio.cpp's kMaxWavDataBytes

// What the game's reader says of these bytes: 0 when it reads them, else why not
static const char *HeaderSays(const std::string &bytes, WavHeader *header, uint32_t mostBytes = kTheMostSound)
{
	WriteTestFile(bytes);
	FileStream stream;

	if (!OpenTestFile(stream))
	{
		return "the test couldn't open its own file";
	}

	return WavHeader_Read(stream, mostBytes, header);
}

static bool Read(const WavMaker &wav)
{
	WavHeader header;
	return HeaderSays(wav.Bytes(), &header) == 0;
}

static bool Refused(const WavMaker &wav)
{
	WavHeader header;
	const char *const why = HeaderSays(wav.Bytes(), &header);
	return why != 0 && why[0] != 0;
}

static void TestTheHeader()
{
	puts("the header of a WAV");

	WavMaker const stock = WavMaker();

	{
		WavHeader header;
		Check(HeaderSays(stock.Bytes(), &header) == 0, "a WAV like most of the game's own (plain PCM, mono, 8 bits, 11,025 a second) is read");
		Check(header.format.channels == 1 && header.format.bitsPerSample == 8 && header.format.samplesPerSec == 11025 && header.dataBytes == 100,
			"and what it says is kept");
	}

	{
		WriteTestFile(stock.Bytes());
		FileStream stream;
		OpenTestFile(stream);
		WavHeader header;
		Check(WavHeader_Read(stream, kTheMostSound, &header) == 0 && header.dataStart == 44 && stream.GetByte() == 'x', "and it stops at the start of the sound, 44 bytes in");
	}

	WavMaker stereo;
	stereo.channels = 2;
	stereo.bits = 16;
	stereo.rate = 44100;
	stereo.align = 4;
	stereo.avg = 176400;
	Check(Read(stereo), "stereo, 16 bits, 44,100 a second, as the game's own stereo sounds are, is read");

	WavMaker eighteen;
	eighteen.formatBytes = 18;
	Check(Read(eighteen), "an 18-byte format that says it has nothing extra, as 13 of the game's own have, is read");

	WavMaker chunksFirst;
	chunksFirst.before = Chunk("smpl", 4, "abcd") + Chunk("LIST", 2, "ab") + Chunk("fact", 0, "");
	Check(Read(chunksFirst), "chunks before the sound, as the game's own smpl, LIST and fact, are stepped over");

	WavMaker slowest;
	slowest.rate = slowest.avg = 100;
	WavMaker fastest;
	fastest.rate = fastest.avg = 100000;
	Check(Read(slowest) && Read(fastest), "the slowest and the fastest rates the sound system plays, 100 and 100,000 a second, are read");

	// What it says has to be what the game plays
	WavMaker extra = eighteen;
	extra.cbSize = 2;
	Check(Refused(extra), "an 18-byte format that says it has extra bytes is refused, not an assert");

	WavMaker adpcm;
	adpcm.tag = 2;
	Check(Refused(adpcm), "a WAV that isn't plain PCM (format tag 2, ADPCM) is refused");
	adpcm.tag = 3;
	Check(Refused(adpcm), "and so is one of floating-point samples (tag 3)");
	adpcm.tag = 0xFFFE;
	Check(Refused(adpcm), "and one that says it's an extended format (0xFFFE)");

	WavMaker channels;
	channels.channels = 0;
	channels.align = 0;
	channels.avg = 0;
	Check(Refused(channels), "a WAV of no channels is refused");
	channels.channels = 3;
	channels.align = 3;
	channels.avg = 33075;
	Check(Refused(channels), "and one of 3, not an assert");

	WavMaker bits;
	bits.bits = 24;
	bits.align = 3;
	bits.avg = 33075;
	Check(Refused(bits), "a WAV of 24-bit samples is refused, not an assert");
	bits.bits = 4;
	bits.align = 0;
	bits.avg = 0;
	Check(Refused(bits), "and one of 4-bit samples");
	bits.bits = 0;
	Check(Refused(bits), "and one of 0-bit samples");

	WavMaker rate;
	rate.rate = rate.avg = 0;
	Check(Refused(rate), "a WAV that plays 0 samples a second is refused");
	rate.rate = rate.avg = 99;
	Check(Refused(rate), "and one a sample slower than the sound system plays");
	rate.rate = rate.avg = 100001;
	Check(Refused(rate), "and one a sample faster");
	rate.rate = rate.avg = 0xFFFFFFFF;
	Check(Refused(rate), "and the fastest a number holds");

	WavMaker align;
	align.align = 2;
	align.avg = 22050;
	Check(Refused(align), "a WAV whose block size isn't its channels times its bytes a sample is refused");

	WavMaker avg;
	avg.avg = 0;
	Check(Refused(avg), "a WAV that says it plays 0 bytes a second is refused (the game works out its length from it)");
	avg.avg = 11026;
	Check(Refused(avg), "and one whose bytes a second aren't its rate times its block size");

	WavMaker none;
	none.sound.clear();
	Check(Refused(none), "a WAV with no sound in it is refused");

	WavMaker blocks;
	blocks.bits = 16;
	blocks.align = 2;
	blocks.avg = 22050;
	Check(Read(blocks), "16-bit sound of 100 bytes, 50 blocks, is read");
	blocks.sound += "x";
	Check(Refused(blocks), "and of 101 bytes, half a block over, is refused");

	// Its sizes have to be the file's own
	WavMaker more;
	more.claimedSound = 1000;
	Check(Refused(more), "a WAV that says it holds more sound than the file has is refused, before any memory is asked for");
	more.claimedSound = 0xFFFFFFFF;
	Check(Refused(more), "and one that says it holds 4 GB");

	{
		WavHeader header;
		Check(HeaderSays(stock.Bytes(), &header, 99) != 0, "a sound of more than the most the game takes from one WAV is refused");
	}

	{
		std::string bytes = stock.Bytes();
		bytes[3] = 'X';
		WavHeader header;
		Check(HeaderSays(bytes, &header) != 0 && !header.riffSizeRead, "a file that doesn't start with RIFF is refused, and has no size to step over");
	}

	{
		std::string bytes = stock.Bytes();
		bytes[8] = 'X';
		WavHeader header;
		Check(HeaderSays(bytes, &header) != 0 && header.riffSizeRead, "a RIFF that isn't a WAVE is refused, and says where it ends");
	}

	{
		std::string riff = "RIFF";
		std::string body = "WAVE" + Chunk("LIST", 4, "abcd") + stock.Bytes().substr(12);
		Put32(riff, (uint32_t)body.size());
		WavHeader header;
		Check(HeaderSays(riff + body, &header) != 0, "a WAV whose first chunk isn't its format is refused");
	}

	WavMaker twenty;
	twenty.formatBytes = 20;
	Check(Refused(twenty), "a format of 20 bytes, which plain PCM never has, is refused");

	{
		WavHeader header;
		Check(HeaderSays(stock.Bytes().substr(0, 30), &header) != 0, "a WAV that ends inside its format is refused");
		Check(HeaderSays(stock.Bytes().substr(0, 40), &header) != 0, "and one that ends inside the size of its sound");
	}

	{
		std::string const bytes = stock.Bytes();
		std::string riff = "RIFF";
		std::string const body = bytes.substr(8, 28);   // WAVE and its format, and no sound
		Put32(riff, (uint32_t)body.size());
		WavHeader header;
		Check(HeaderSays(riff + body, &header) != 0, "a WAV with no data chunk is refused");
	}

	// Its RIFF chunk has to fit: a sound bank steps over it to the next sound
	{
		WavMaker inABank;
		inABank.after = std::string(200, 'n');
		WavHeader header;
		Check(HeaderSays(inABank.Bytes(), &header) == 0, "a WAV with another sound after it, as in a sound bank, is read");
		Check(header.riffStart + header.riffSize == 8 + 36 + 100, "and its RIFF chunk ends where the next sound starts");

		WavMaker pastTheEnd;
		pastTheEnd.claimedRiff = 36 + 100 + 1000;
		Check(Refused(pastTheEnd), "a WAV whose RIFF chunk says it runs past the end of the file is refused");

		WavMaker shortRiff;
		shortRiff.claimedRiff = 30;
		shortRiff.after = std::string(200, 'n');
		Check(Refused(shortRiff), "and so is one whose sound runs past its own RIFF chunk, into the next sound");

		WavMaker refusedButStepped = adpcm;
		refusedButStepped.after = std::string(200, 'n');
		Check(HeaderSays(refusedButStepped.Bytes(), &header) != 0 && header.riffSizeRead && header.riffStart + header.riffSize == 8 + 36 + 100,
			"a WAV refused for its format still says where its RIFF chunk ends, so a bank can step over it");
	}
}

// The loaders refuse what the header's reader refuses, before any memory, and say so in the log (added 2026-11-27 by Melanikus (Drake))
static void TestTheLoadersRefuse()
{
	puts("the WAV loaders refuse what the header's reader refuses");

	std::string const audio = ReadSourceFile("3dc/avp/win95/openal_audio.cpp");
	std::string const wavFile = Between(audio, "int LoadWavFile(int soundNum, const std::string &fileName)", "int ExtractWavFile(int soundNum, FileStream &fStream)");
	std::string const bankWav = Between(audio, "int ExtractWavFile(int soundNum, FileStream &fStream)", "alBufferData");

	Check(ComesBefore(wavFile, "return RefusedWAV(fileName, refused);", "AllocateWavData(soundNum, nAudioBytes)"), "a WAV file the reader refuses is refused before any memory is asked for it");
	Check(ComesBefore(bankWav, "return RefusedWAV(GameSounds[soundNum].wavName, refused);", "AllocateWavData(soundNum, nAudioBytes)"), "and so is one in a sound bank");
	Check(ComesBefore(bankWav, "if (header.riffSizeRead)\n\t\t{\n\t\t\tfStream.Seek(startOffset + totalChunkSize, FileStream::SeekStart);", "return RefusedWAV(GameSounds[soundNum].wavName, refused);"),
		"and a refused one in a bank is stepped over when its size was read, so the next one is still found");
	Check(!Has(wavFile, "assert(") && !Has(bankWav, "assert(") && !Has(wavFile, "LOCALASSERT(") && !Has(bankWav, "LOCALASSERT("), "neither loader has an assert left: a bad WAV is refused and said in the log");
	Check(Has(audio, "LogString(\"sound: refused the WAV \" + PlainFileName_Printable(name.c_str()) + \": \" + why);"), "and the log names it, printable, and says why");
	Check(Has(audio, "static_assert(kWavSlowestRate == (uint32_t)FREQUENCY_MINPLAT && kWavFastestRate == (uint32_t)FREQUENCY_MAXPLAT"), "the rates it refuses are the sound system's own limits");
}

int main()
{
	TestReadingToTheEnd();
	TestEveryReaderGivesZero();
	TestAValueCutShort();
	TestPeekByte();
	TestANameThatNeverEnds();
	TestFindingTheSound();
	TestFindingTheSound();
	TestTheHeader();
	TestFailedReadsAndStartingAfresh();
	TestTheGamesLoops();
	TestTheSizesInSoundFiles();
	TestTheSizesInSoundFiles();
	TestTheLoadersRefuse();

	remove(kTestFile);

	if (checksFailed)
	{
		printf("The WAV chunks test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The WAV chunks test passed");
	return 0;
}
