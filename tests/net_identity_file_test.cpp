// The test of the identity file's layout (3dc/net_identity_file.h, #21). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A player's identity is kept in their save folder between runs, and anyone who can write there can change the file, so it's read as
// hostile input. This writes good files and reads them back, then tries each way a file can be wrong, and checks each is refused for its
// own reason, with nothing in the view to use by mistake. It also checks the writer never makes a file the reader would refuse.
//
// Nothing here uses Windows, the network or any cryptography: the sealing and the file itself are net_identity_store.cpp's, and are tried
// where the cryptography is.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file on its own. The program gives back a non-zero result if anything failed.

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "net_identity_file.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// Made-up public halves and sealed bytes, each byte different from its neighbours, so a part read from the wrong place shows
static std::vector<uint8_t> Pattern(size_t size, uint8_t start)
{
	std::vector<uint8_t> bytes(size);

	for (size_t i = 0; i < size; i++)
	{
		bytes[i] = (uint8_t)(start + i * 7);
	}

	return bytes;
}

static std::vector<uint8_t> GoodFile(size_t sealedBytes)
{
	std::vector<uint8_t> const statics = Pattern(kNetIdentityStaticBytes, 3);
	std::vector<uint8_t> const sealed = Pattern(sealedBytes, 101);
	std::vector<uint8_t> file(kNetIdentityFileMostBytes);
	size_t const written = NetIdentityFile_Write(statics.data(), sealed.data(), sealed.size(), file.data(), file.size());
	file.resize(written);
	return file;
}

// The file has to be refused for exactly this reason, with an empty view
static void Refused(std::vector<uint8_t> const &file, NetIdentityFileRefusal reason, std::string const &what)
{
	NetIdentityFileView view = { (const uint8_t *)1, (const uint8_t *)1, 99 };
	NetIdentityFileRefusal const why = NetIdentityFile_Read(file.data(), file.size(), &view);
	Check(why == reason && !view.statics && !view.sealed && view.sealedBytes == 0, what);

	if (why != reason)
	{
		printf("          it said: %s\n", NetIdentityFile_Words(why));
	}
}

static void TestGoodFiles()
{
	puts("Good files");

	size_t const typical = kNetIdentitySealedPlainBytes + 230;
	std::vector<uint8_t> const file = GoodFile(typical);
	Check(file.size() == kNetIdentityFileHeadBytes + typical, "a file is its heading and its sealed part, and nothing more");

	NetIdentityFileView view;
	Check(NetIdentityFile_Read(file.data(), file.size(), &view) == kNetIdentityFileGood, "and is read back");
	Check(view.statics == file.data() + 12 && memcmp(view.statics, Pattern(kNetIdentityStaticBytes, 3).data(), kNetIdentityStaticBytes) == 0,
		"with the public halves where they were written");
	Check(view.sealedBytes == typical && view.sealed == file.data() + kNetIdentityFileHeadBytes &&
		memcmp(view.sealed, Pattern(typical, 101).data(), typical) == 0, "and the sealed part too, all of it");

	Check(memcmp(file.data(), "AVPX-ID2", 8) == 0 && file[8] == 1 && file[9] == 0 && file[10] == 0 && file[11] == 0,
		"it starts with its mark and its version, 1, lowest byte first");

	std::vector<uint8_t> const least = GoodFile(kNetIdentitySealedLeast);
	std::vector<uint8_t> const most = GoodFile(kNetIdentitySealedMost);
	Check(NetIdentityFile_Read(least.data(), least.size(), &view) == kNetIdentityFileGood && view.sealedBytes == kNetIdentitySealedLeast,
		"the smallest sealed part a sealed identity can have is read");
	Check(NetIdentityFile_Read(most.data(), most.size(), &view) == kNetIdentityFileGood && view.sealedBytes == kNetIdentitySealedMost &&
		most.size() <= kNetIdentityFileMostBytes, "and so is the largest, in a file within the most that's read");
}

static void TestNumbers()
{
	puts("Numbers in the file");

	uint8_t at[4] = { 0, 0, 0, 0 };
	NetIdentityFile_PutNumber(at, 0x12345678u);
	Check(at[0] == 0x78 && at[1] == 0x56 && at[2] == 0x34 && at[3] == 0x12, "a number is written lowest byte first, all four bytes of it");
	Check(NetIdentityFile_ReadNumber(at) == 0x12345678u, "and read back the same way");
}

static void TestRefusedFiles()
{
	puts("Files that are refused, each for its own reason");

	std::vector<uint8_t> const good = GoodFile(kNetIdentitySealedPlainBytes + 230);

	Refused(std::vector<uint8_t>(kNetIdentityFileMostBytes + 1, 0), kNetIdentityFileTooBig, "a file bigger than any this game writes is refused");
	Refused(std::vector<uint8_t>(), kNetIdentityFileTooShort, "an empty file is refused");
	Refused(std::vector<uint8_t>(good.begin(), good.begin() + (kNetIdentityFileHeadBytes - 1)), kNetIdentityFileTooShort,
		"and so is one that stops a byte before its heading ends");

	std::vector<uint8_t> notOurs = good;
	notOurs[7] = '3';
	Refused(notOurs, kNetIdentityFileNotOurs, "a file that doesn't start with AVPX-ID2 is refused");

	std::vector<uint8_t> otherVersion = good;
	otherVersion[8] = 2;
	Refused(otherVersion, kNetIdentityFileOtherVersion, "and one with a layout this game doesn't know");

	std::vector<uint8_t> highVersion = good;
	highVersion[11] = 1;
	Refused(highVersion, kNetIdentityFileOtherVersion, "every byte of the version counts, the highest too");

	size_t const sizeAt = 12 + kNetIdentityStaticBytes;
	std::vector<uint8_t> tooLittle = GoodFile(kNetIdentitySealedLeast);
	NetIdentityFile_PutNumber(tooLittle.data() + sizeAt, (uint32_t)(kNetIdentitySealedLeast - 1));
	tooLittle.pop_back();
	Refused(tooLittle, kNetIdentityFileSealedSize, "a sealed part a byte smaller than any sealed identity is refused, even when the size adds up");

	std::vector<uint8_t> tooMuch = GoodFile(kNetIdentitySealedMost);
	NetIdentityFile_PutNumber(tooMuch.data() + sizeAt, (uint32_t)(kNetIdentitySealedMost + 1));
	tooMuch.push_back(0);
	Refused(tooMuch, kNetIdentityFileSealedSize, "and so is one a byte bigger than any");

	std::vector<uint8_t> huge = good;
	NetIdentityFile_PutNumber(huge.data() + sizeAt, 0xFFFFFFFFu);
	Refused(huge, kNetIdentityFileSealedSize, "a sealed size of four billion is refused before anything is added to it");

	std::vector<uint8_t> extra = good;
	extra.push_back(0);
	Refused(extra, kNetIdentityFileWrongSize, "a file with a byte after its sealed part is refused");

	std::vector<uint8_t> missing = good;
	missing.pop_back();
	Refused(missing, kNetIdentityFileWrongSize, "and one a byte short of the sealed part it says it has");

	NetIdentityFileView view;
	Check(NetIdentityFile_Read(NULL, 100, &view) == kNetIdentityFileTooShort && !view.statics, "no bytes at all are refused");
	Check(NetIdentityFile_Read(good.data(), good.size(), NULL) == kNetIdentityFileTooShort, "and so is a read with nowhere to put the answer");
}

static void TestTheWriter()
{
	puts("The writer");

	std::vector<uint8_t> const statics = Pattern(kNetIdentityStaticBytes, 3);
	std::vector<uint8_t> const sealed = Pattern(kNetIdentitySealedMost + 1, 101);
	std::vector<uint8_t> out(kNetIdentityFileMostBytes + 4096, 0xEE);

	Check(NetIdentityFile_Write(statics.data(), sealed.data(), kNetIdentitySealedLeast - 1, out.data(), out.size()) == 0 && out[0] == 0xEE,
		"a sealed part too small for a sealed identity isn't written, and nothing is");
	Check(NetIdentityFile_Write(statics.data(), sealed.data(), kNetIdentitySealedMost + 1, out.data(), out.size()) == 0 && out[0] == 0xEE,
		"nor one too big");

	size_t const needed = kNetIdentityFileHeadBytes + kNetIdentitySealedLeast;
	Check(NetIdentityFile_Write(statics.data(), sealed.data(), kNetIdentitySealedLeast, out.data(), needed - 1) == 0 && out[0] == 0xEE,
		"a file that wouldn't fit in the room given isn't started");
	Check(NetIdentityFile_Write(statics.data(), sealed.data(), kNetIdentitySealedLeast, out.data(), needed) == needed && out[needed] == 0xEE,
		"one that fits exactly is written, and not a byte past it");

	Check(NetIdentityFile_Write(NULL, sealed.data(), kNetIdentitySealedLeast, out.data(), out.size()) == 0 &&
		NetIdentityFile_Write(statics.data(), NULL, kNetIdentitySealedLeast, out.data(), out.size()) == 0 &&
		NetIdentityFile_Write(statics.data(), sealed.data(), kNetIdentitySealedLeast, NULL, out.size()) == 0,
		"and nothing is written without its halves or a place to put them");

	bool readsBack = true;

	for (size_t size = kNetIdentitySealedLeast; size <= kNetIdentitySealedMost; size += 37)
	{
		std::vector<uint8_t> const file = GoodFile(size);
		NetIdentityFileView view;
		readsBack = readsBack && NetIdentityFile_Read(file.data(), file.size(), &view) == kNetIdentityFileGood && view.sealedBytes == size;
	}

	Check(readsBack, "every file the writer makes, at sealed sizes across the whole range, the reader takes");
}

static void TestWords()
{
	puts("The words for each refusal");

	bool allHaveWords = true;
	bool allDiffer = true;

	for (int a = kNetIdentityFileGood; a <= kNetIdentityFileWrongSize; a++)
	{
		const char *const words = NetIdentityFile_Words((NetIdentityFileRefusal)a);
		allHaveWords = allHaveWords && words && words[0] && strstr(words, "no words") == NULL;

		for (int b = kNetIdentityFileGood; b < a; b++)
		{
			allDiffer = allDiffer && strcmp(words, NetIdentityFile_Words((NetIdentityFileRefusal)b)) != 0;
		}
	}

	Check(allHaveWords, "every refusal has words of its own");
	Check(allDiffer, "and no two have the same words");
}

int main()
{
	TestGoodFiles();
	TestNumbers();
	TestRefusedFiles();
	TestTheWriter();
	TestWords();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
