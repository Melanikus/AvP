// The test of the log line that says which level file was loaded, and of the loader's reads that work it out. Written 2026-11-08 by
// Melanikus for the AvP port (#89).
//
// WHAT IT TESTS
//
// - level_file_sum.h on its own: which sizes a level file may be, the words of the line, with the sum the MD5 standard gives for
//   its own example, and the words for a file refused for its size.
// - The game's real level loader (File_Chunk, in mishchnk.cpp), on real files this test writes in a folder of its own, which it makes
//   inside the folder it runs in and names by its own process number:
//   - a plain level and a packed level both load, and the size and sum each one gives are those of the file as it is on disk;
//   - a copy with one byte changed gives another sum;
//   - a file too short to hold a level's heading is refused, and so is a file larger than any level the game can use, even though
//     everything in it adds up; neither gives a size or a sum, and the log says why each was refused.
// - By reading the game's source: loading a level writes the line, once, after the level is in and before it's copied into the game,
//   naming the path it was loaded from; the loader reads the size as a 64-bit number, and every read it makes has to get all it asked
//   for and adds what it got to the sum; the second loader, which read sizes the old way, stays gone; the custom maps list uses the
//   loader's own largest size.
//
// WHAT IT CAN'T SHOW: a file of 4 GB or more, whose size's lower 32 bits look small. Making one would fill the disk of the computer
// running the tests. The rule's own check below covers that size, and the source check shows the loader reads all 64 bits.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/level_file_sum_test.args, and the program gives back a non-zero result if any check
// failed. It needs no game files: it makes its own. tests/takeouts/level_file_sum_test.json proves each check can fail.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "huffman.hpp"
#include "md5.h"
#include "level_file_sum.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code uses

// The loader opens its file through this. Here it's Windows' own CreateFile, as it is in the game (utilities.cpp), so the loader
// reads real files, with the same settings the game gives.
HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *)
{
	return INVALID_FILE_ATTRIBUTES;
}

// The memory code stops the game through these if the computer runs out of memory. Here that stops the test.
void avp_MessageBox(const char *message, int)
{
	puts(message);
}

void avp_exit(int code)
{
	exit(code);
}

// What the loader writes in the log as an error, kept so the test can read it
static std::vector<std::string> errorLines;

void LogErrorStringC(const char *line)
{
	errorLines.push_back(line ? line : "");
}

// The memory code tells the game's sounds about memory that's about to go. There are no sounds in this test.
void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// ---- Making level files

typedef std::vector<unsigned char> Bytes;

// The first count bytes of some text. Both ends of the range come from the one pointer: two copies of the same words in quotes needn't
// be the same place in memory, so a range from the start of one copy to the end of another can be any length, even running backwards.
// (The test's first run on GitHub stopped with "vector too long" that way.)
static Bytes BytesOf(const char *text, size_t count)
{
	return Bytes(text, text + count);
}

static void PutNumber(Bytes &out, unsigned int value)
{
	for (int i = 0; i < 4; i++)
	{
		out.push_back((unsigned char)(value >> (8 * i)));
	}
}

// A chunk: its 8-letter name, its size in bytes with this 12-byte heading included, then what it holds. A whole level file is one
// of these, named REBINFF2.
static Bytes MakeChunk(const char *name, const Bytes &body)
{
	Bytes out(name, name + 8);

	PutNumber(out, (unsigned int)(12 + body.size()));
	out.insert(out.end(), body.begin(), body.end());

	return out;
}

// The level, packed the way the game packs its levels, as one file: the package and what follows it
static Bytes Packed(const Bytes &level)
{
	Bytes copy(level);
	HuffmanPackage *const package = HuffmanCompression(copy.data(), (int)copy.size());
	unsigned char const *const start = (unsigned char const *)package;
	Bytes out(start, start + sizeof(HuffmanPackage) + package->CompressedDataSize);

	free(package);

	return out;
}

static void Write(const std::string &name, const Bytes &data)
{
	FILE *const file = fopen(name.c_str(), "wb");

	if (!file || fwrite(data.data(), 1, data.size(), file) != data.size())
	{
		printf("  FAILED  the test couldn't write %s\n", name.c_str());
		exit(1);
	}

	fclose(file);
}

// A level file of the given size that adds up: its heading, then one chunk of a kind the game doesn't know, which it keeps as plain
// bytes, filling the rest. Only the two headings are written. Then the file is made longer, and Windows fills what's added with zeros.
static void WriteLongLevel(const std::string &name, unsigned int size)
{
	Bytes start = BytesOf("REBINFF2", 8);

	PutNumber(start, size);
	Bytes const partName = BytesOf("LONGPART", 8);
	start.insert(start.end(), partName.begin(), partName.end());
	PutNumber(start, size - 12);

	FILE *const file = fopen(name.c_str(), "wb");

	if (!file || fwrite(start.data(), 1, start.size(), file) != start.size() || _fseeki64(file, (long long)size - 1, SEEK_SET) != 0 ||
		fputc(0, file) == EOF)
	{
		printf("  FAILED  the test couldn't write %s\n", name.c_str());
		exit(1);
	}

	fclose(file);
}

// ---- Sums

struct Sum
{
	unsigned char md5[16];
};

static Sum SumOf(const Bytes &bytes)
{
	Sum sum;
	MD5Context context;

	MD5Init(&context);
	MD5Update(&context, bytes.data(), (unsigned int)bytes.size());
	MD5Final(sum.md5, &context);

	return sum;
}

static bool Same(const unsigned char a[16], const Sum &b)
{
	return memcmp(a, b.md5, 16) == 0;
}

// Loads a file through the game's loader, as load_rif does, and gives back whether it loaded, and the size and sum it worked out
static bool Loads(const std::string &path, LevelFileSum *sum)
{
	File_Chunk *const file = new File_Chunk(path.c_str());
	bool const loaded = file->error_code == 0;

	*sum = file->file_sum;
	delete file;

	return loaded;
}

// ---- Reading the game's source

static std::string ReadWholeFile(const std::string &name)
{
	std::ifstream file(name.c_str(), std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int Count(const std::string &text, const char *what)
{
	int count = 0;

	for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

// Does first come before second in text, with both there?
static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(second, a);
	return a != std::string::npos && b != std::string::npos;
}

// The part of text from start up to end, or nothing if either isn't there
static std::string Between(const std::string &text, const char *start, const char *end)
{
	size_t const a = text.find(start);
	size_t const b = a == std::string::npos ? std::string::npos : text.find(end, a);
	return (a == std::string::npos || b == std::string::npos) ? std::string() : text.substr(a, b - a);
}

// ---- The tests

static void TestTheRule()
{
	printf("The sizes a level file may be, and the line:\n");

	Check(!LevelFile_SizeIsUsable(11), "a file one byte shorter than a level's heading may not be read as a level");
	Check(LevelFile_SizeIsUsable(12), "a file just as long as the heading may");
	Check(LevelFile_SizeIsUsable(32LL << 20), "and so may one of 32 MB, the most a level may be");
	Check(!LevelFile_SizeIsUsable((32LL << 20) + 1), "but not one byte more");
	Check(!LevelFile_SizeIsUsable((4LL << 30) + 12), "nor a file of 4 GB and 12 bytes, though the lower 32 bits of its size say 12");
	Check(!LevelFile_SizeIsUsable(-1), "nor a size that couldn't be found");

	// The MD5 standard's own example (RFC 1321, appendix A.5): the sum of the three letters "abc"
	LevelFileSum abc;
	abc.bytes = 3;
	memcpy(abc.md5, SumOf(BytesOf("abc", 3)).md5, 16);
	Check(LevelFileSum_Line("avp_rifs/Test.rif", abc) == "level: avp_rifs/Test.rif is 3 bytes, MD5 900150983cd24fb0d6963f7d28e17f72",
		"the line names the file, its size and its sum, written as md5sum writes a sum");

	LevelFileSum most;
	most.bytes = 4294967295u;
	memset(most.md5, 0xff, 16);
	Check(LevelFileSum_Line("x", most) == "level: x is 4294967295 bytes, MD5 ffffffffffffffffffffffffffffffff",
		"the largest size there is, and the highest digits, are written in full");

	LevelFileSum const none = LevelFileSum();
	Check(LevelFileSum_Line("x", none) == "level: x is 0 bytes, MD5 00000000000000000000000000000000", "a sum never worked out is all zeros");

	Check(LevelFile_SizeRefusalLine("x", 11) == "level: x can't be read: it's 11 bytes, too short to be a level file", "a file too short is refused in words that say so");
	Check(LevelFile_SizeRefusalLine("x", (4LL << 30) + 12) == "level: x can't be read: it's 4294967308 bytes, larger than any level the game can use",
		"and so is a file too large, with its whole size");
	Check(LevelFile_SizeRefusalLine("x", -1) == "level: x can't be read: its size can't be found", "and a file whose size can't be found");
}

static void TestTheLoader(const std::string &folder)
{
	printf("The game's level loader, on real files:\n");

	Bytes const level = MakeChunk("REBINFF2", MakeChunk("SUMTESTS", Bytes(40, 'x')));
	Bytes changed = level;
	changed.back() = 'y';
	Bytes const packed = Packed(level);
	printf("  (the level is packed: %u bytes)\n", (unsigned int)packed.size());
	Bytes const tooShort = BytesOf("REBINFF2abc", 11);

	std::string const plainFile = folder + "Plain.rif";
	std::string const changedFile = folder + "Changed.rif";
	std::string const packedFile = folder + "Packed.rif";
	std::string const shortFile = folder + "Short.rif";
	std::string const longFile = folder + "Long.rif";

	Write(plainFile, level);
	Write(changedFile, changed);
	Write(packedFile, packed);
	Write(shortFile, tooShort);
	WriteLongLevel(longFile, (unsigned int)kLevelFileMostBytes + 1);
	printf("  (the test's files are written, in %s)\n", folder.c_str());

	LevelFileSum got;

	Check(Loads(plainFile, &got), "a plain level loads");
	Check(got.bytes == level.size() && Same(got.md5, SumOf(level)), "and its size and sum are those of the file");

	Check(Loads(changedFile, &got), "a copy with its last byte changed loads");
	Check(got.bytes == changed.size() && Same(got.md5, SumOf(changed)) && !Same(got.md5, SumOf(level)), "and its sum is its own, not the first file's");

	Check(Loads(packedFile, &got), "a packed level loads");
	Check(got.bytes == packed.size() && Same(got.md5, SumOf(packed)), "and its size and sum are those of the file as it is on disk, still packed");

	Check(errorLines.empty(), "none of those writes an error in the log");

	got.bytes = 1;
	Check(!Loads(shortFile, &got) && got.bytes == 0, "a file too short to hold a level's heading is refused, and has no size or sum");
	Check(errorLines.size() == 1 && errorLines.back() == LevelFile_SizeRefusalLine(shortFile.c_str(), 11), "and the log says why, naming the file");

	got.bytes = 1;
	Check(!Loads(longFile, &got), "a file one byte larger than any level the game can use is refused, though everything in it adds up");
	Check(got.bytes == 0, "and none of it was read: it has no size or sum");
	Check(errorLines.size() == 2 && errorLines.back() == LevelFile_SizeRefusalLine(longFile.c_str(), kLevelFileMostBytes + 1), "and the log says why");

	got.bytes = 1;
	Check(!Loads(folder + "Missing.rif", &got) && got.bytes == 0, "a file that isn't there is refused, and has no size or sum");

	remove(plainFile.c_str());
	remove(changedFile.c_str());
	remove(packedFile.c_str());
	remove(shortFile.c_str());
	remove(longFile.c_str());
}

static void TestTheGameUsesIt()
{
	printf("Where the game uses it, by reading:\n");

	std::string const system = ReadWholeFile("3dc/avp/win95/system.cpp");
	std::string const loading = Between(system, "void LoadRifFile()", "textureRelief.LogWhatIsHeld();");
	char const *const line = "LogString(LevelFileSum_Line(file_and_path, avp_rif_file_sum(env_rif)));";

	Check(ComesBefore(loading, "env_rif = avp_load_rif((const char*)&file_and_path[0]);", "exit(0x3421);") && ComesBefore(loading, "exit(0x3421);", line) &&
		ComesBefore(loading, line, "copy_rif_data(env_rif, CCF_ENVIRONMENT"),
		"a level that loads says which file it was, by the path it was loaded from, before it's copied into the game");
	Check(Count(system, "LevelFileSum_Line(") == 1, "once for each level");

	std::string const handleSum = Between(ReadWholeFile("3dc/avp/win95/projload.cpp"), "LevelFileSum avp_rif_file_sum(RIFFHANDLE h)", "RIFFHANDLE avp_load_rif_non_env(");
	Check(Has(handleSum, "if (h == INVALID_RIFFHANDLE || !h->fc)") && Has(handleSum, "return h->fc->file_sum;"),
		"the sum a level's handle gives is that of the file its loader read");

	std::string const loader = Between(ReadWholeFile("3dc/win95/mishchnk.cpp"), "File_Chunk::File_Chunk(const char * file_name)", "File_Chunk::File_Chunk()");
	Check(Has(loader, "long long const bytes_on_disk = GetFileSizeEx(rif_file, &size_on_disk) ? size_on_disk.QuadPart : -1;") &&
		Has(loader, "if (!LevelFile_SizeIsUsable(bytes_on_disk))") && Has(loader, "file_size = (DWORD)bytes_on_disk;") && !Has(loader, "GetFileSize("),
		"the loader reads the file's size as a 64-bit number, and checks it before using it");
	Check(Count(loader, "ReadFile(") == 4 && Count(loader, ", &bytes_read, 0) || bytes_read != ") == 4,
		"each of the loader's four reads has to get every byte it asked for");
	Check(Count(loader, "MD5Update(&sum_so_far, ") == 4, "and each one adds what it read to the sum");

	// There was a second loader, RIF_File_Chunk, that read a file's size the old way, with no limit. Nothing ever called it, and it went
	// (REMOVED-CODE.md). It mustn't come back that way: the loader above is the only one, so its reads are the whole file's reads.
	std::string const loaderFile = ReadWholeFile("3dc/win95/mishchnk.cpp");
	std::string const loaderHeader = ReadWholeFile("3dc/win95/mishchnk.hpp");
	Check(!loaderFile.empty() && !loaderHeader.empty() && !Has(loaderHeader, "class RIF_File_Chunk") && !Has(loaderFile, "RIF_File_Chunk::") &&
		!Has(loaderFile, "\"SUBRIFFL\""), "the second loader, which nothing called, is gone");
	Check(Count(loaderFile, "ReadFile(") == 4 && !Has(loaderFile, "GetFileSize("),
		"and the loader file's only reads are the loader's four, with no file's size read the old way");

	std::string const customMaps = ReadWholeFile("3dc/avp/win95/frontend/custom_maps.cpp");
	Check(Has(customMaps, "static const long long kCustomMapMostFileBytes = kLevelFileMostBytes;"),
		"the custom maps list never lists a file larger than the loader reads");
}

// Two ways a program can be stopped with no word of why, both with Windows' code 0xC0000409: an exception nobody catches (the C++
// library then calls abort), and a C library function given an argument it can't take. Each of these says which it was, and what, then
// ends the test as failed.
static void SayAnExceptionStoppedIt()
{
	std::string what = "one that isn't a standard exception";

	try
	{
		std::exception_ptr const thrown = std::current_exception();

		if (thrown)
		{
			std::rethrow_exception(thrown);
		}

		what = "none: something called std::terminate";
	}
	catch (const std::exception &e)
	{
		what = e.what();
	}
	catch (...)
	{
	}

	printf("  FAILED  the test was stopped by an exception nobody caught: %s\n", what.c_str());
	_exit(1);
}

static void SayABadArgumentStoppedIt(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
	printf("  FAILED  the test was stopped because a C library function was given an argument it can't take\n");
	_exit(1);
}

// The folder for this run's files, ending in a backslash. It's inside the folder the test runs in, and named by nothing but this program's
// number, as level_log_test's is: no setting from outside, and nothing the program was started with, can point it anywhere else. So every
// file the test writes, reads and deletes is a name it built itself, inside a folder it made itself.
static std::string TestFolder()
{
	return "level_file_sum_test_files_" + std::to_string(GetCurrentProcessId()) + "\\";
}

int main()
{
	// Every line goes out as soon as it's printed. The checks' output goes through a pipe, which would otherwise hold it back and lose it
	// if the program were stopped by a crash, so the last line shown is always the last step reached.
	setvbuf(stdout, NULL, _IONBF, 0);
	std::set_terminate(SayAnExceptionStoppedIt);
	_set_invalid_parameter_handler(SayABadArgumentStoppedIt);

	std::string const folder = TestFolder();

	if (!CreateDirectoryA(folder.c_str(), NULL))
	{
		printf("  FAILED  the test couldn't make its own folder %s (a folder of that name may be left from an earlier run)\n", folder.c_str());
		return 1;
	}

	TestTheRule();
	TestTheLoader(folder);
	TestTheGameUsesIt();

	RemoveDirectoryA(folder.c_str());

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
