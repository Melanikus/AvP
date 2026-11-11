// The test of servers_trust.cpp: what a player's yes to a servers.cfg line is the hash of, and the kept file's text (#175's last part).
// Written 2026-11-10 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: servers-trusted.cfg decides which lines the game uses without asking, so a file that says more than the game wrote must
// never be taken (AVP Tao 3). servers_trust.h says what the file may hold. This tries what the game writes, read back; each way a line or
// the file can be wrong, each refused whole for its own reason; and the 68 bytes a yes is the hash of, byte for byte, for nyc3's line. The
// hash itself is SHA-256, with the game's cryptography, and its known answer is net_relay_keys_test's, so this test needs none.
// The text is always given in a buffer of exactly its own size, with no 0 after it, so a read past its end would be a read past the
// buffer, which the standard library's checks (every_test.h) stop.
//
// Nothing here uses the network or the disk.

#include "servers_trust.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// Reads text from a buffer of exactly its size
static ServersTrustRefusal ReadText(const std::string &text, ServersTrustYeses *out, int *badLine = NULL)
{
	std::vector<char> exact(text.begin(), text.end());
	return ServersTrust_Read(exact.empty() ? NULL : exact.data(), exact.size(), out, badLine);
}

// A hash of the test's own: every byte the same number. Modified 2026-11-10 by Melanikus for the AvP port: written a byte at a time, not
// with memset (Snyk's "an optimizing compiler may remove memset": it sees this function fill its caller's bytes and return, and can't
// tell the caller reads them). A loop says the same and can't be taken out.
static void MadeUpHash(uint8_t hash[kServersTrustHashBytes], uint8_t value)
{
	for (size_t i = 0; i < kServersTrustHashBytes; i++)
	{
		hash[i] = value;
	}
}

static std::string YesLine(uint8_t value)
{
	char digits[2 * kServersTrustHashBytes + 1];

	for (size_t i = 0; i < kServersTrustHashBytes; i++)
	{
		snprintf(digits + 2 * i, 3, "%02x", value);
	}

	return std::string("yes ") + digits + "\n";
}

// nyc3's line in the servers.cfg the game ships: 104.131.35.41, ports 443 and 27500, and its fingerprint
static uint8_t const kNyc3Fingerprint[kServersTrustFingerprintBytes] = {
	0xbb, 0x0e, 0x29, 0x58, 0xda, 0xde, 0x79, 0x03, 0x3a, 0x90, 0xf3, 0x48, 0x11, 0xc8, 0x48, 0x37,
	0xb4, 0x04, 0x70, 0x8c, 0xc0, 0x04, 0xc5, 0xb1, 0xd3, 0x7d, 0x9a, 0x26, 0xf2, 0x8d, 0x66, 0x3e };

static void TestWhatAYesIs()
{
	printf("1. what a yes is the hash of\n");
	uint8_t input[kServersTrustInputBytes];
	Check(kServersTrustInputBytes == 68, "68 bytes: the 27 words, a 0, the address, both ports and the fingerprint");
	Check(ServersTrust_HashInput(0x68832329u, 443, 27500, kNyc3Fingerprint, input), "nyc3's line makes them");

	std::vector<uint8_t> want(kServersTrustWords, kServersTrustWords + 27);
	uint8_t const rest[] = { 0, 0x68, 0x83, 0x23, 0x29, 0x01, 0xbb, 0x6b, 0x6c };
	want.insert(want.end(), rest, rest + sizeof(rest));
	want.insert(want.end(), kNyc3Fingerprint, kNyc3Fingerprint + sizeof(kNyc3Fingerprint));
	Check(want.size() == kServersTrustInputBytes && memcmp(input, want.data(), want.size()) == 0,
		"\"AVPx servers-trusted line 1\", 0, 104.131.35.41 first number first, 443 and 27500 higher byte first, then the fingerprint");

	uint8_t other[kServersTrustInputBytes];
	ServersTrust_HashInput(0x68832329u, 8443, 27500, kNyc3Fingerprint, other);
	Check(memcmp(input, other, sizeof(input)) != 0, "another list port is another line");
	ServersTrust_HashInput(0x68832329u, 443, 27501, kNyc3Fingerprint, other);
	Check(memcmp(input, other, sizeof(input)) != 0, "and another relay port");
	ServersTrust_HashInput(0x6883232Au, 443, 27500, kNyc3Fingerprint, other);
	Check(memcmp(input, other, sizeof(input)) != 0, "and another address");

	memset(other, 0x55, sizeof(other));
	Check(!ServersTrust_HashInput(0x68832329u, 0, 27500, kNyc3Fingerprint, other) && other[0] == 0 && other[kServersTrustInputBytes - 1] == 0,
		"a list port of 0 makes nothing, and leaves nothing behind");
	Check(!ServersTrust_HashInput(0x68832329u, 443, 65536, kNyc3Fingerprint, other), "nor a relay port past 65535");
	Check(!ServersTrust_HashInput(0x68832329u, 443, 27500, NULL, other), "nor no fingerprint");
	Check(!ServersTrust_HashInput(0x68832329u, 443, 27500, kNyc3Fingerprint, NULL), "and nowhere to put them is refused");
}

static void TestWrittenAndReadBack()
{
	printf("2. what the game writes, read back\n");
	ServersTrustYeses yeses;
	yeses.count = 0;
	char text[kServersTrustMostBytes];

	size_t size = ServersTrust_Write(yeses, text, sizeof(text));
	ServersTrustYeses read;
	Check(size > 0 && text[0] == '#' && text[size - 1] == '\n', "with no yes, the file is the game's note alone");
	Check(ReadText(std::string(text, size), &read) == kServersTrustRead && read.count == 0, "and it reads back as no yes");

	uint8_t hash[kServersTrustHashBytes];

	for (int i = 0; i < kServersTrustMostYeses; i++)
	{
		MadeUpHash(hash, static_cast<uint8_t>(i));
		Check(ServersTrust_Add(&yeses, hash), i == 0 ? "a yes is added" : (i == kServersTrustMostYeses - 1 ? "and so is the 64th" : "and another"));
	}

	MadeUpHash(hash, 200);
	Check(!ServersTrust_Add(&yeses, hash) && yeses.count == kServersTrustMostYeses, "a 65th has no room");
	MadeUpHash(hash, 7);
	Check(ServersTrust_Add(&yeses, hash) && yeses.count == kServersTrustMostYeses, "one that's there already is there, and isn't added again");

	size = ServersTrust_Write(yeses, text, sizeof(text));
	Check(size > 0 && size <= static_cast<size_t>(kServersTrustMostBytes), "64 yeses fit in the file");
	Check(ReadText(std::string(text, size), &read) == kServersTrustRead && read.count == kServersTrustMostYeses &&
		memcmp(read.hashes, yeses.hashes, sizeof(yeses.hashes)) == 0, "and they read back, the same and in the same order");

	MadeUpHash(hash, 63);
	Check(ServersTrust_Has(read, hash), "a yes read back is there");
	MadeUpHash(hash, 64);
	Check(!ServersTrust_Has(read, hash), "and one never said isn't");

	char small[100];
	Check(ServersTrust_Write(yeses, small, sizeof(small)) == 0, "with too little room, nothing is written");
	Check(ServersTrust_Write(yeses, text, size) == size && ServersTrust_Write(yeses, text, size - 1) == 0, "it needs room for every byte, and no more");

	std::string withReturns;
	std::string const written(text, size);

	for (char c : written)
	{
		if (c == '\n')
		{
			withReturns += '\r';
		}

		withReturns += c;
	}

	Check(ReadText(withReturns, &read) == kServersTrustRead && read.count == kServersTrustMostYeses, "Notepad's line ends, a return before each newline, are taken");
}

static void Refused(const std::string &text, ServersTrustRefusal why, int line, const char *what)
{
	ServersTrustYeses read;
	read.count = 99;
	int badLine = -1;
	ServersTrustRefusal const got = ReadText(text, &read, &badLine);
	Check(got == why && read.count == 0 && badLine == line, what);
}

static void TestHostileFiles()
{
	printf("3. a file the game didn't write is refused whole, and nothing of it is taken\n");
	std::string const one = YesLine(1);
	std::string const two = YesLine(2);
	Check(ReadText("", NULL) == kServersTrustBadLine, "nowhere to put the yeses is refused");
	ServersTrustYeses read;
	Check(ReadText("", &read) == kServersTrustRead && read.count == 0, "an empty file holds no yes");

	Refused(one + two.substr(0, two.size() - 1), kServersTrustBadLine, 2, "a last line with no newline (a file cut short)");
	Refused(one + "Yes " + two.substr(4), kServersTrustBadLine, 2, "Yes with a capital");
	Refused(one + "yes  " + two.substr(4), kServersTrustBadLine, 2, "two spaces after yes");
	Refused(one + "yes\t" + two.substr(4), kServersTrustBadLine, 2, "a tab after yes");
	Refused(one + " " + two, kServersTrustBadLine, 2, "a space before it");
	Refused(one + two.substr(0, two.size() - 1) + " \n", kServersTrustBadLine, 2, "a space after it");
	Refused(one + "yes " + std::string(63, 'a') + "\n", kServersTrustBadLine, 2, "63 digits");
	Refused(one + "yes " + std::string(65, 'a') + "\n", kServersTrustBadLine, 2, "65 digits");
	Refused(one + "yes " + std::string(63, 'a') + "A\n", kServersTrustBadLine, 2, "a capital hex digit");
	Refused(one + "yes " + std::string(63, 'a') + "g\n", kServersTrustBadLine, 2, "a letter past f");
	Refused(one + "\n" + two, kServersTrustBadLine, 2, "an empty line, which the game never writes");
	Refused(one + "add 203.0.113.7\n", kServersTrustBadLine, 2, "anything else, an address among them");
	// In a note, whose length is free, so it's the byte that's refused and not the line's length
	Refused("# a note with a return\r inside it\n" + one, kServersTrustBadLine, 1, "a return inside a line, a note's too");
	Refused(std::string("# a note with a 0") + std::string(1, '\0') + " inside it\n" + one, kServersTrustBadLine, 1, "a 0 inside a line, a note's too");
	Refused("# a note with an accent \xc3\xa9\n" + one, kServersTrustBadLine, 1, "a note with a byte that isn't printable");
	Refused(one + one, kServersTrustTwice, 2, "the same yes twice");

	std::string many;

	for (int i = 0; i <= kServersTrustMostYeses; i++)
	{
		many += YesLine(static_cast<uint8_t>(i));
	}

	Refused(many, kServersTrustTooMany, kServersTrustMostYeses + 1, "65 yeses");
	Refused(std::string(kServersTrustMostBytes + 1, '#'), kServersTrustTooBig, 0, "more than 16 KB, before any of it is read");

	Check(ReadText("# a note\twith a tab\n" + one, &read) == kServersTrustRead && read.count == 1, "a note may hold a tab, and is skipped");
	Check(ReadText(std::string(kServersTrustMostBytes - 1, '#') + "\n", &read) == kServersTrustRead, "and a file of exactly 16 KB is read");
}

// (The pick, servers-picked.cfg, went 2026-11-11 with the player's choice of a line, R-SRV-4: REMOVED-CODE.md has the row.)

static void TestWords()
{
	printf("4. words for each refusal\n");
	ServersTrustRefusal const all[] = { kServersTrustRead, kServersTrustTooBig, kServersTrustBadLine, kServersTrustTooMany, kServersTrustTwice };
	bool allHaveWords = true;
	bool allDiffer = true;

	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
	{
		const char *const words = ServersTrust_Words(all[i]);
		allHaveWords = allHaveWords && words && words[0];

		for (size_t j = 0; j < i; j++)
		{
			allDiffer = allDiffer && strcmp(words, ServersTrust_Words(all[j])) != 0;
		}
	}

	Check(allHaveWords && allDiffer, "every refusal has words of its own");
	Check(ServersTrust_Words(static_cast<ServersTrustRefusal>(99)) != NULL, "and a value that isn't one still gives words");
}

int main()
{
	TestWhatAYesIs();
	TestWrittenAndReadBack();
	TestHostileFiles();
	TestWords();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
