// The test of servers_cfg.cpp, the reader of servers.cfg (#175). Written 2026-11-10 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: servers.cfg says where the game goes online, and anyone can write it (a download, a friend's "replace your line with
// this", a program on the PC), so it's hostile (AVP Tao 3). servers_cfg.h lists every rule. This tries good files and lines, pasted every
// way people paste, each rule at its edge and one step past it, and each way a line or the file can be wrong, and checks each is refused
// for its own reason, with the other lines still read. The text is always given in a buffer of exactly its own size, with no 0 after it,
// so a read past its end would be a read past the buffer, which the standard library's checks (every_test.h) stop.
//
// Nothing here reads a file or uses the network.

#include "servers_cfg.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int checksFailed = 0;

// relay_list_text.cpp, whose reader of an address this uses, also holds net_identity.h's reader of a fingerprint, which wipes what it
// refuses with net_crypto.h's NetCrypto_Wipe. The real one is with the game's cryptography, which this test doesn't need: this one wipes
// the same way, a byte at a time through a volatile pointer, so the compiler can't leave it out.
void NetCrypto_Wipe(void *bytes, size_t size)
{
	volatile unsigned char *byte = static_cast<volatile unsigned char *>(bytes);

	while (size--)
	{
		*byte++ = 0;
	}
}

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static void Check(bool passed, const std::string &what)
{
	Check(passed, what.c_str());
}

// A good fingerprint: every small hex digit, in turn
static const std::string kFp = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

// One line, with one space between its fields
static std::string Line(const std::string &address, const std::string &name = "New York", const std::string &fingerprint = kFp)
{
	return address + " " + fingerprint + " " + name;
}

static ServersCfgFileRead read;

// Reads text as the file, from a buffer of exactly its size (no 0 after it)
static void ReadText(const std::string &text)
{
	std::vector<char> exact(text.begin(), text.end());
	ServersCfg_Read(exact.empty() ? NULL : exact.data(), exact.size(), &read);
}

// One line read on its own: why it was refused, or that it's good. -1 when the file was refused, -2 when it gave anything else.
static ServersCfgLineRefusal OneLine(const std::string &line)
{
	ReadText(line + "\n");

	if (read.fileRefusal != kServersCfgFileRead)
	{
		return static_cast<ServersCfgLineRefusal>(-1);
	}

	if (read.serverCount == 1 && read.refusedCount == 0)
	{
		return kServersCfgLineGood;
	}

	if (read.serverCount == 0 && read.refusedCount == 1 && read.refused[0].lineNumber == 1)
	{
		return read.refused[0].why;
	}

	return static_cast<ServersCfgLineRefusal>(-2);
}

static void TestGoodLines(void)
{
	puts("Good lines");

	Check(OneLine(Line("203.0.113.7")) == kServersCfgLineGood, "one good line is one server");
	ServersCfgServer const s = read.servers[0];
	Check(strcmp(s.name, "New York") == 0, "its name, with the space inside it");
	Check(s.address == 0xCB007107u && s.place == kServersCfgOnTheInternet, "its address, 203.0.113.7, on the internet");
	Check(s.listPort == 443 && s.relayPort == 27500, "with no ports written, the list is on 443 and the relay on 27500");
	Check(s.fingerprint[0] == 0x01 && s.fingerprint[7] == 0xef && s.fingerprint[31] == 0xef, "its fingerprint, as bytes");
	Check(s.lineNumber == 1, "it says it's line 1");

	Check(OneLine(Line("203.0.113.7:8443:47800")) == kServersCfgLineGood && read.servers[0].listPort == 8443 && read.servers[0].relayPort == 47800,
		"both ports written: the list's, then the relay's");
	Check(OneLine(Line("1.0.0.1:1:65535")) == kServersCfgLineGood && read.servers[0].listPort == 1 && read.servers[0].relayPort == 65535,
		"ports 1 and 65535");
	Check(OneLine(Line("223.255.255.255")) == kServersCfgLineGood, "the last address before multicast");

	// Pasted every way people paste
	Check(OneLine("203.0.113.7\t" + kFp + "\tNew York") == kServersCfgLineGood && strcmp(read.servers[0].name, "New York") == 0, "tabs between the fields");
	Check(OneLine("203.0.113.7    " + kFp + " \t  New York") == kServersCfgLineGood, "runs of spaces and tabs between them");
	Check(OneLine("   203.0.113.7 " + kFp + " New York  \t ") == kServersCfgLineGood && strcmp(read.servers[0].name, "New York") == 0,
		"spaces and tabs at either end of the line");
	Check(OneLine(Line("203.0.113.7", "New    York")) == kServersCfgLineGood && strcmp(read.servers[0].name, "New York") == 0,
		"a run of spaces inside the name is one space");
	Check(OneLine(Line("203.0.113.7", "New\t \tYork")) == kServersCfgLineGood && strcmp(read.servers[0].name, "New York") == 0,
		"and so is a run of tabs and spaces");
	Check(OneLine(Line("203.0.113.7", "Mel's server (New York, 2)")) == kServersCfgLineGood, "a name of several words with signs in it");
	Check(OneLine(Line("203.0.113.7", "~!\"$%&'()*+,-./:;<=>?@[\\]^_`{|}")) == kServersCfgLineGood, "any printable characters make a name");
	Check(OneLine(Line("203.0.113.7", "x#")) == kServersCfgLineGood, "a # inside a name is fine");

	ReadText(Line("203.0.113.7") + "\r\n" + Line("198.51.100.1", "Paris") + "\r\n");
	Check(read.serverCount == 2 && read.refusedCount == 0 && strcmp(read.servers[1].name, "Paris") == 0, "Windows' line ends (Notepad's)");
	ReadText(Line("203.0.113.7") + "\r");
	Check(read.serverCount == 1 && read.refusedCount == 0, "a carriage return as the file's very last byte");
	ReadText("\xEF\xBB\xBF" + Line("203.0.113.7") + "\n");
	Check(read.serverCount == 1 && read.servers[0].lineNumber == 1, "UTF-8's mark at the start of the file is skipped");

	std::string const text = "# Our servers\n\n" + Line("203.0.113.7") + "\n   # an indented note\n" + Line("198.51.100.1", "Paris");
	ReadText(text);
	Check(read.serverCount == 2 && read.refusedCount == 0, "notes and empty lines are skipped, and the last line needs no newline");
	Check(read.servers[0].lineNumber == 3 && read.servers[1].lineNumber == 5, "the line numbers count the notes and empty lines, as an editor does");

	ReadText("");
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 0 && read.refusedCount == 0, "an empty file is read, with no servers");
	ServersCfg_Read(NULL, 0, &read);
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 0, "and so is no text at all");
	ServersCfg_Read(NULL, 5, &read);
	Check(read.fileRefusal == kServersCfgFileNoText && read.serverCount == 0, "a size with no text is refused, and never read");
	ReadText("\xEF\xBB\xBF");
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 0 && read.refusedCount == 0, "a file of only UTF-8's mark is empty");
}

static void TestWholeFile(void)
{
	puts("The whole file refused");

	// 64 lines is the most, notes and empty lines counted
	std::string sixtyThree;

	for (int i = 0; i < 63; i++)
	{
		sixtyThree += "# a note\n";
	}

	ReadText(sixtyThree + Line("203.0.113.7") + "\n");
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 1 && read.servers[0].lineNumber == 64, "64 lines are read");
	ReadText(sixtyThree + Line("203.0.113.7") + "\n\n");
	Check(read.fileRefusal == kServersCfgFileTooManyLines && read.serverCount == 0, "a 65th line, even an empty one, refuses the whole file");
	ReadText(Line("198.51.100.1", "First") + "\n" + sixtyThree + "#");
	Check(read.fileRefusal == kServersCfgFileTooManyLines && read.serverCount == 0, "a 65th line with no newline counts too, and the good first line isn't used");
	ReadText("\xEF\xBB\xBF" + sixtyThree + Line("203.0.113.7") + "\n");
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 1, "UTF-8's mark isn't a line");

	// 16 KB is the most
	std::string const line = Line("203.0.113.7") + "\n";
	std::string const big = line + "#" + std::string(kServersCfgMostBytes - line.size() - 2, 'x') + "\n";
	Check(big.size() == static_cast<size_t>(kServersCfgMostBytes), "(the 16 KB file is 16 KB)");
	ReadText(big);
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 1, "a file of 16 KB is read");
	ReadText(big + "#");
	Check(read.fileRefusal == kServersCfgFileTooBig && read.serverCount == 0, "a file a byte over 16 KB is refused whole");
}

static void TestBadBytes(void)
{
	puts("Bytes that refuse their line, and only it");

	char const bad[] = { '\r', '\0', 0x01, 0x0B, 0x0C, 0x1B, 0x7F, static_cast<char>(0x80), static_cast<char>(0xC3), static_cast<char>(0xEF), static_cast<char>(0xFF) };
	char const *const badNames[] = { "a lone carriage return", "a 0", "a 1", "a vertical tab", "a form feed", "an escape", "DEL", "0x80",
		"0xC3 (UTF-8's start of an accent)", "0xEF (the start of UTF-8's mark)", "0xFF" };

	for (size_t i = 0; i < sizeof(bad); i++)
	{
		std::string const b(1, bad[i]);

		// In a note, between two good lines
		ReadText(Line("203.0.113.7") + "\n# a note " + b + " here\n" + Line("198.51.100.1", "Paris") + "\n");
		Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == 2 && read.refusedCount == 1 && read.refused[0].lineNumber == 2 &&
			read.refused[0].why == kServersCfgBadByte, std::string(badNames[i]) + " in a note: that line is refused, and the two good lines are read");

		// In a server's line: in its name, and in its address
		ReadText(Line("203.0.113.7", "New" + b + "York") + "\n" + Line("198.51.100.1", "Paris") + "\n");
		Check(read.serverCount == 1 && strcmp(read.servers[0].name, "Paris") == 0 && read.refusedCount == 1 && read.refused[0].why == kServersCfgBadByte,
			std::string(badNames[i]) + " in a name refuses that line alone");
		ReadText(Line("203.0.113.7" + b) + "\n");
		Check(read.serverCount == 0 && read.refusedCount == 1 && read.refused[0].why == kServersCfgBadByte, std::string(badNames[i]) + " after an address");
	}

	// Nothing inside a line moves where lines start: a lone carriage return or a 0 in line 1 leaves line 2 as line 2
	ReadText("# one\rtwo\n" + Line("203.0.113.7") + "\n");
	Check(read.serverCount == 1 && read.servers[0].lineNumber == 2 && read.refusedCount == 1 && read.refused[0].lineNumber == 1,
		"a lone carriage return doesn't start a line");
	ReadText(std::string("# one") + '\0' + "two\n" + Line("203.0.113.7") + "\n");
	Check(read.serverCount == 1 && read.servers[0].lineNumber == 2, "and neither does a 0");
	ReadText(Line("203.0.113.7") + "\r\r\n");
	Check(read.serverCount == 0 && read.refusedCount == 1 && read.refused[0].why == kServersCfgBadByte, "only one carriage return can end a line");
	ReadText("# note\n\xEF\xBB\xBF" + Line("203.0.113.7") + "\n");
	Check(read.serverCount == 0 && read.refusedCount == 1 && read.refused[0].lineNumber == 2, "UTF-8's mark anywhere but the very start refuses its line");
}

static void TestFields(void)
{
	puts("Fields");

	Check(OneLine("203.0.113.7") == kServersCfgFieldMissing, "an address alone");
	Check(OneLine("203.0.113.7 " + kFp) == kServersCfgFieldMissing, "an address and a fingerprint, with no name");
	Check(OneLine("203.0.113.7 " + kFp + "   ") == kServersCfgFieldMissing, "with only spaces where the name would be");
	Check(OneLine("New York") == kServersCfgFieldMissing, "two words");

	struct Bad { std::string text; const char *what; };
	Bad const addresses[] = {
		{ "203.0.113.7:443", "one port only, which would leave which one it is to a guess" },
		{ "203.0.113.7:443:27500:1", "three ports" },
		{ "203.0.113.7:", "a colon with no ports" },
		{ "203.0.113.7::27500", "an empty list port" },
		{ "203.0.113.7:443:", "an empty relay port" },
		{ ":443:27500", "ports with no address" },
		{ "203.0.113.7:0:27500", "port 0" },
		{ "203.0.113.7:443:65536", "port 65536" },
		{ "203.0.113.7:0443:27500", "a port with a leading zero" },
		{ "203.0.113.7:+443:27500", "a port with a sign" },
		{ "203.0.113.007", "an address with a leading zero" },
		{ "203.0.113", "three parts" },
		{ "203.0.113.7.1", "five parts" },
		{ "203.0.113.256", "a part past 255" },
		{ "list.example.com", "a name, which is never an address" },
		{ "localhost", "localhost, a name too" },
		{ "[::1]", "an IPv6 address" },
		{ "0x7f.0.0.1", "a hex part" },
		{ "255.255.255.255:65535:655350", "longer than any address and ports" },
	};

	for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++)
	{
		Check(OneLine(Line(addresses[i].text)) == kServersCfgBadAddress, std::string("the address: ") + addresses[i].what);
	}

	Check(OneLine(Line("203.0.113.7", "x", kFp.substr(1))) == kServersCfgBadFingerprint, "a fingerprint of 63 digits");
	Check(OneLine(Line("203.0.113.7", "x", kFp + "0")) == kServersCfgBadFingerprint, "a fingerprint of 65 digits");
	Check(OneLine(Line("203.0.113.7", "x", "0123456789ABCDEF" + kFp.substr(16))) == kServersCfgBadFingerprint, "a fingerprint with capitals");
	Check(OneLine(Line("203.0.113.7", "x", "g" + kFp.substr(1))) == kServersCfgBadFingerprint, "a fingerprint with a letter past f");
	Check(OneLine(Line("203.0.113.7", "x", "sha256:" + kFp)) == kServersCfgBadFingerprint, "a fingerprint with a word before it");

	Check(OneLine(Line("203.0.113.7", std::string(31, 'n'))) == kServersCfgLineGood, "a name of 31 letters");
	Check(OneLine(Line("203.0.113.7", std::string(32, 'n'))) == kServersCfgBadName, "a name of 32 letters");
	Check(OneLine(Line("203.0.113.7", "a" + std::string(40, ' ') + "b")) == kServersCfgLineGood && strcmp(read.servers[0].name, "a b") == 0,
		"a name is counted after its runs of spaces are one space each");
	Check(OneLine(Line("203.0.113.7", std::string(16, 'n') + " " + std::string(15, 'n'))) == kServersCfgLineGood, "31 letters with a space in them");
	Check(OneLine(Line("203.0.113.7", std::string(16, 'n') + " " + std::string(16, 'n'))) == kServersCfgBadName, "and 32");
	Check(OneLine(Line("203.0.113.7", "#hash")) == kServersCfgBadName, "a name starting with #, which would look like a note");
	Check(OneLine("#" + Line("203.0.113.7")) == static_cast<ServersCfgLineRefusal>(-2) && read.serverCount == 0 && read.refusedCount == 0,
		"a line starting with # is a note, never a server");

	// The first field that's wrong is the reason
	Check(OneLine(Line("bad", "#bad", "bad")) == kServersCfgBadAddress, "with every field wrong, the address is the reason");
	Check(OneLine(Line("203.0.113.7", "#bad", "bad")) == kServersCfgBadFingerprint, "and with the address right, the fingerprint");
}

static void TestPlaces(void)
{
	puts("Where an address may be");

	struct Place { const char *address; bool ever; ServersCfgPlace place; const char *what; };
	Place const places[] = {
		{ "0.0.0.0", false, kServersCfgOnTheInternet, "0.0.0.0, \"this network\"" },
		{ "0.255.255.255", false, kServersCfgOnTheInternet, "the end of 0/8" },
		{ "255.255.255.255", false, kServersCfgOnTheInternet, "broadcast" },
		{ "224.0.0.1", false, kServersCfgOnTheInternet, "multicast" },
		{ "239.255.255.255", false, kServersCfgOnTheInternet, "the end of multicast" },
		{ "240.0.0.1", false, kServersCfgOnTheInternet, "the reserved block" },
		{ "100.64.0.0", false, kServersCfgOnTheInternet, "a carrier's network, 100.64/10, at its start" },
		{ "100.127.255.255", false, kServersCfgOnTheInternet, "and at its end" },
		{ "100.63.255.255", true, kServersCfgOnTheInternet, "just before a carrier's network" },
		{ "100.128.0.0", true, kServersCfgOnTheInternet, "just after it" },
		{ "127.0.0.1", true, kServersCfgOnThisPC, "127.0.0.1" },
		{ "127.255.255.254", true, kServersCfgOnThisPC, "the end of 127/8" },
		{ "10.0.0.1", true, kServersCfgOnTheOwnNetwork, "10/8" },
		{ "172.16.0.1", true, kServersCfgOnTheOwnNetwork, "172.16/12 at its start" },
		{ "172.31.255.254", true, kServersCfgOnTheOwnNetwork, "172.16/12 at its end" },
		{ "172.15.255.255", true, kServersCfgOnTheInternet, "just before 172.16/12" },
		{ "172.32.0.0", true, kServersCfgOnTheInternet, "just after it" },
		{ "192.168.1.10", true, kServersCfgOnTheOwnNetwork, "192.168/16" },
		{ "192.169.0.1", true, kServersCfgOnTheInternet, "just after 192.168/16" },
		{ "169.254.1.1", true, kServersCfgOnTheOwnNetwork, "169.254/16, link-local" },
		{ "8.8.8.8", true, kServersCfgOnTheInternet, "an ordinary address" },
	};

	char const *const placeWords[] = { "on the internet", "on this PC", "on the player's own network" };

	for (size_t i = 0; i < sizeof(places) / sizeof(places[0]); i++)
	{
		Place const &p = places[i];
		ServersCfgLineRefusal const why = OneLine(Line(p.address));

		if (!p.ever)
		{
			Check(why == kServersCfgNeverAServer, std::string(p.what) + ": never a server's");
		}
		else
		{
			Check(why == kServersCfgLineGood && read.servers[0].place == p.place, std::string(p.what) + ": a server " + placeWords[p.place]);
		}
	}
}

static void TestShared(void)
{
	puts("Names and addresses two lines share");

	// The same name, capitals and runs of spaces aside: both refused, and the line after still counts
	ReadText(Line("203.0.113.7") + "\n" + Line("198.51.100.1", "NEW   york") + "\n" + Line("198.51.100.9", "Paris") + "\n");
	Check(read.serverCount == 1 && strcmp(read.servers[0].name, "Paris") == 0, "two lines with one name, capitals and spaces aside: only the third is a server");
	Check(read.refusedCount == 2 && read.refused[0].lineNumber == 1 && read.refused[0].why == kServersCfgNameShared &&
		read.refused[1].lineNumber == 2 && read.refused[1].why == kServersCfgNameShared, "and both of the two are refused, the first too");

	// The same address, whatever the ports
	ReadText(Line("203.0.113.7") + "\n" + Line("203.0.113.7:8443:47800", "Copy") + "\n");
	Check(read.serverCount == 0 && read.refusedCount == 2 && read.refused[0].why == kServersCfgAddressShared && read.refused[1].why == kServersCfgAddressShared,
		"two lines with one address, on other ports: both refused");

	// Three with one name: all three
	ReadText(Line("1.1.1.1", "A") + "\n" + Line("2.2.2.2", "a") + "\n" + Line("3.3.3.3", "A") + "\n");
	Check(read.serverCount == 0 && read.refusedCount == 3, "three lines with one name: all three refused");

	// A line refused on its own doesn't take another line's name or address away
	ReadText(Line("203.0.113.7") + "\n" + Line("203.0.113.7", "New York", "bad") + "\n");
	Check(read.serverCount == 1 && read.servers[0].lineNumber == 1 && read.refusedCount == 1 && read.refused[0].why == kServersCfgBadFingerprint,
		"a bad line with a good line's name and address doesn't refuse the good one");

	// One line sharing its name with one and its address with another
	ReadText(Line("1.1.1.1", "A") + "\n" + Line("2.2.2.2", "a") + "\n" + Line("1.1.1.1", "C") + "\n");
	Check(read.serverCount == 0 && read.refusedCount == 3 && read.refused[0].why == kServersCfgNameShared && read.refused[1].why == kServersCfgNameShared &&
		read.refused[2].why == kServersCfgAddressShared, "a line sharing a name with one and an address with another: each says its first reason");
}

static void TestManyLines(void)
{
	puts("A file of good and bad lines");

	std::string text;
	int good = 0;

	for (int i = 0; i < kServersCfgMostLines; i++)
	{
		if (i % 2 == 0)
		{
			text += Line("198.51.100." + std::to_string(i + 1), "Server " + std::to_string(i)) + "\n";
			good++;
		}
		else
		{
			text += "bad line " + std::to_string(i) + "\n";
		}
	}

	ReadText(text);
	Check(read.fileRefusal == kServersCfgFileRead && read.serverCount == good && read.refusedCount == kServersCfgMostLines - good,
		"64 lines, every other one bad: each good one is a server, and each bad one is told");
	Check(read.servers[good - 1].lineNumber == kServersCfgMostLines - 1 && read.refused[read.refusedCount - 1].lineNumber == kServersCfgMostLines,
		"and the last of each has its own line number");
}

static void TestWords(void)
{
	puts("Words for the log and the screen");

	const char *const unknownLine = ServersCfg_LineWords(static_cast<ServersCfgLineRefusal>(999));
	bool everyLine = true;

	for (int i = kServersCfgLineGood; i <= kServersCfgAddressShared; i++)
	{
		const char *const words = ServersCfg_LineWords(static_cast<ServersCfgLineRefusal>(i));
		everyLine = everyLine && words && words[0] && strcmp(words, unknownLine) != 0;
	}

	Check(everyLine, "every line refusal has words of its own");

	const char *const unknownFile = ServersCfg_FileWords(static_cast<ServersCfgFileRefusal>(999));
	bool everyFile = true;

	for (int i = kServersCfgFileRead; i <= kServersCfgFileNoText; i++)
	{
		const char *const words = ServersCfg_FileWords(static_cast<ServersCfgFileRefusal>(i));
		everyFile = everyFile && words && words[0] && strcmp(words, unknownFile) != 0;
	}

	Check(everyFile, "and so does every file refusal");
	Check(strcmp(SERVERS_CFG_FILE_NAME, "servers.cfg") == 0, "the file's name, as the log gives it");
}

int main()
{
	TestGoodLines();
	TestWholeFile();
	TestBadBytes();
	TestFields();
	TestPlaces();
	TestShared();
	TestManyLines();
	TestWords();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
