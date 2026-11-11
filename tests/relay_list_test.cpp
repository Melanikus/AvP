// The test of the relay list's words: the answers the game reads back, and the bodies it sends. Written 2026-11-08 by Melanikus for the
// AvP port.
//
// WHAT IT'S FOR: the Find games page shows what the relay server's list says, and the game finds each region's relay through it. That comes
// from the network, so it's hostile (AVP Tao 3). relay_list_text.h reads only exactly what PROTOCOL.md (the server's repository, section 4,
// at 7014de1) says, and refuses anything else, all of it, for a reason of its own. This tries good answers, the largest the protocol
// allows, and each way an answer can go wrong, and checks each is refused for its own reason. Then the bodies the game sends, which have to
// be exactly what the list takes, the check on the server's name from the settings, and the words for a game's style.
//
// relay_list.cpp makes the calls, and needs the network and the game, so that part is read, not run: that every call is HTTPS, with TLS
// 1.2 or newer, the certificate checked, and no redirects.
//
// Every game also carries its host's fingerprint (net_identity.h), for protocol version 2's handshake B: a game without a good one is
// refused, a listing is never sent without one, and a host's listing keeps the one its room opened with (added 2026-11-09).
//
// Nothing here uses the network. Run it from the top of the repository, where the source it reads is found from.

#include "relay_list_text.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

// net_identity.h's reader of a fingerprint wipes what it was given when it refuses one, with net_crypto.h's NetCrypto_Wipe. The real one is
// in net_crypto_cng.cpp, with the rest of the game's cryptography, which this test doesn't need: this one wipes the same way, a byte at a time
// through a volatile pointer, so the compiler can't leave it out.
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

static const int kOurVersion = 113;
static RelayListGame games[kRelayListMostGames];

// One game's text, with these fields. Each is written as it goes in the JSON, quotes and all, so a test can put anything there.
struct GameFields
{
	std::string region = "\"saopaulo\"";
	std::string code = "\"ABCDEFGH\"";
	std::string name = "\"Hicks's game\"";
	std::string level = "\"Derelict\"";
	std::string mode = "\"Deathmatch\"";
	std::string gameStyle = "0";
	std::string levelNumber = "3";
	std::string coopLobby = "0";
	std::string players = "2";
	std::string maxPlayers = "4";
	std::string version = "113";
	std::string fingerprint = "\"0123456789abcdeffedcba9876543210\"";
	std::string hostMs = "42";
	std::string seconds = "75";
	std::string extra;   // written after the others, comma and all
};

// The bytes that fingerprint stands for
static const uint8_t kGameFingerprint[kNetIdentityFingerprintBytes] =
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 };

static std::string Game(const GameFields &f)
{
	return "{\"region\":" + f.region + ",\"code\":" + f.code + ",\"name\":" + f.name + ",\"level\":" + f.level + ",\"mode\":" + f.mode +
		",\"gameStyle\":" + f.gameStyle + ",\"levelNumber\":" + f.levelNumber + ",\"coopLobby\":" + f.coopLobby + ",\"players\":" + f.players +
		",\"maxPlayers\":" + f.maxPlayers + ",\"version\":" + f.version + ",\"fingerprint\":" + f.fingerprint + ",\"hostMs\":" + f.hostMs +
		",\"seconds\":" + f.seconds + f.extra + "}";
}

static std::string ListOf(const std::string &gamesText)
{
	return "{\"games\":[" + gamesText + "]}";
}

// A different code for each number, from 0 to 1023
static std::string CodeNumber(int n)
{
	std::string code = "222222";
	code += kRelayCodeAlphabet[n % 32];
	code += kRelayCodeAlphabet[(n / 32) % 32];
	return "\"" + code + "\"";
}

// A text of `letters` characters, every one of them a " or a \, which JSON writes as two
static std::string AllEscaped(int letters)
{
	std::string text = "\"";

	for (int i = 0; i < letters; i++)
	{
		text += (i & 1) ? "\\\\" : "\\\"";
	}

	return text + "\"";
}

static RelayListRefusal Read(const std::string &text, int *count, int version = kOurVersion)
{
	return RelayList_ReadGames(text.data(), text.size(), version, true, games, count);
}

// The list has to be refused, for exactly this reason, with no games kept
static void Refused(const std::string &text, RelayListRefusal reason, const char *what)
{
	int count = -1;
	RelayListRefusal const why = Read(text, &count);
	Check(why == reason && count == 0, what);

	if (why != reason)
	{
		printf("          it said: %s\n", RelayList_RefusalWords(why));
	}
}

static void TestGoodLists()
{
	puts("1. lists of games as the list writes them");

	int count = -1;
	Check(Read("{\"games\":[]}", &count) == kRelayListAccepted && count == 0, "a list with no games is read, with none");

	GameFields f;
	Check(Read(ListOf(Game(f)), &count) == kRelayListAccepted && count == 1, "a list with one game is read");
	Check(strcmp(games[0].region, "saopaulo") == 0 && strcmp(games[0].code, "ABCDEFGH") == 0 && strcmp(games[0].name, "Hicks's game") == 0 &&
		strcmp(games[0].level, "Derelict") == 0 && strcmp(games[0].mode, "Deathmatch") == 0, "with its region, code, name, level and mode");
	Check(games[0].gameStyle == 0 && games[0].levelNumber == 3 && games[0].coopLobby == 0 && games[0].players == 2 && games[0].maxPlayers == 4 &&
		games[0].version == 113 && games[0].hostMs == 42 && games[0].seconds == 75, "and its numbers");
	Check(memcmp(games[0].fingerprint, kGameFingerprint, sizeof(kGameFingerprint)) == 0, "and its host's fingerprint, as its 16 bytes");

	std::string const shuffled = "{\"seconds\":0,\"hostMs\":0,\"fingerprint\":\"ffffffffffffffffffffffffffffffff\",\"version\":113,\"maxPlayers\":8,"
		"\"players\":8,\"coopLobby\":255,\"levelNumber\":100,\"gameStyle\":255,\"mode\":\"m\",\"level\":\"l\",\"name\":\"n\",\"code\":\"ZZZZZZZZ\",\"region\":\"a\"}";
	Check(Read(ListOf(shuffled), &count) == kRelayListAccepted && count == 1 && games[0].players == 8 && games[0].seconds == 0 &&
		games[0].levelNumber == 100 && games[0].coopLobby == 255 && games[0].hostMs == 0, "the fields in any order, each at an end of its range");

	std::string const spaced = " \r\n{ \"games\" :\t[ { \"region\" : \"virginia\" , \"code\" : \"ABCDEFGH\" , \"name\" : \"a\" , \"level\" : \"b\" ,"
		" \"mode\" : \"c\" , \"gameStyle\" : 4 , \"levelNumber\" : 0 , \"coopLobby\" : 1 , \"players\" : 1 , \"maxPlayers\" : 1 ,"
		" \"version\" : 113 , \"fingerprint\" : \"00000000000000000000000000000001\" , \"hostMs\" : 9999 , \"seconds\" : 31536000 } ]\n}\n";
	Check(Read(spaced, &count) == kRelayListAccepted && count == 1 && games[0].seconds == kRelayListMostSeconds && games[0].hostMs == 9999,
		"spaces, tabs and line ends between everything, and a game listed for a year");

	f.name = "\"\\\"quoted\\\" and \\\\back\\\\\"";
	Check(Read(ListOf(Game(f)), &count) == kRelayListAccepted && strcmp(games[0].name, "\"quoted\" and \\back\\") == 0,
		"\\\" and \\\\ in a name come out as \" and \\");

	f = GameFields();
	f.name = "\"" + std::string(32, 'x') + "\"";
	f.level = "\"" + std::string(39, 'y') + "\"";
	Check(Read(ListOf(Game(f)), &count) == kRelayListAccepted && strlen(games[0].name) == 32 && strlen(games[0].level) == 39,
		"a name of 32 characters and a level of 39, the most of each");

	GameFields other;
	other.region = "\"paris\"";
	Check(Read(ListOf(Game(f) + "," + Game(other)), &count) == kRelayListAccepted && count == 2,
		"one code in two regions is two games: a game is its region and code together");

	// The largest list the protocol allows, as the server's own test builds it: 100 games, every text at its longest, made of the
	// characters JSON writes as two, and every number at its longest. That came to 41,111 bytes, and each game's fingerprint adds 49 more:
	// a comma, "fingerprint" in quotes, a colon, and its 32 digits in quotes. So 46,011, still under the 48 KB the game reads.
	std::string text;

	for (int g = 0; g < kRelayListMostGames; g++)
	{
		GameFields big;
		big.region = "\"abcdefghijklmnop\"";
		big.code = CodeNumber(g);
		big.name = big.mode = AllEscaped(32);
		big.level = AllEscaped(39);
		big.gameStyle = "255";
		big.levelNumber = "100";
		big.coopLobby = "255";
		big.players = big.maxPlayers = "8";
		big.version = "65535";
		big.hostMs = "9999";
		big.seconds = "31536000";
		text += (g ? "," : "") + Game(big);
	}

	text = ListOf(text);
	Check(text.size() == 46011, "the largest list the protocol allows is 46,011 bytes, fingerprints and all");
	Check(text.size() <= kRelayListMostGamesBytes, "which fits in what's read");
	Check(Read(text, &count, kRelayListMostVersion) == kRelayListAccepted && count == kRelayListMostGames, "and all 100 of its games are read");
	Check(strlen(games[99].level) == 39 && strlen(games[99].mode) == 32, "each text at its longest");
}

static void TestTheWholeList()
{
	puts("2. a list of games as a whole");

	GameFields f;
	std::string const good = ListOf(Game(f));

	Refused(std::string(kRelayListMostGamesBytes + 1, ' '), kRelayListTooBig, "a list over 48 KB is refused, without being read");
	Refused(good.substr(0, 10) + std::string(1, '\0') + good.substr(10), kRelayListHasAZero, "a list with a 0 byte in it is refused");
	Refused("", kRelayListEndsTooSoon, "an empty answer is refused");
	Refused("   ", kRelayListEndsTooSoon, "and one of only spaces");
	Refused("[]", kRelayListNotAnObject, "a list that isn't an object is refused");
	Refused("<html>", kRelayListNotAnObject, "and so is a web page");
	Refused("{}", kRelayListNotItsField, "an object without \"games\" is refused");
	Refused("{\"relays\":[]}", kRelayListNotItsField, "and one with another field instead");
	Refused("{\"games\":[],\"addresses\":[]}", kRelayListNotItsField, "and one with a field after \"games\"");
	Refused("{\"games\":{}}", kRelayListNotAList, "\"games\" that isn't a list is refused");
	Refused("{\"games\":5}", kRelayListNotAList, "and a number for it");
	Refused("{\"games\" []}", kRelayListNotWhatComesNext, "a missing : is refused");
	Refused(ListOf(Game(f) + ";" + Game(f)), kRelayListNotWhatComesNext, "and a ; between games");
	Refused("{\"games\":[", kRelayListEndsTooSoon, "a list that stops after its [ is refused");
	Refused("{\"games\":[]", kRelayListEndsTooSoon, "and one with no closing }");
	Refused(good + "x", kRelayListMoreAfterTheEnd, "anything after the closing } is refused");
	Refused(good + "{}", kRelayListMoreAfterTheEnd, "and so is a second object");

	std::string tooMany;

	for (int g = 0; g <= kRelayListMostGames; g++)
	{
		GameFields small;
		small.code = CodeNumber(g);
		tooMany += (g ? "," : "") + Game(small);
	}

	Refused(ListOf(tooMany), kRelayListTooMany, "101 games are refused");

	GameFields second;
	Refused(ListOf(Game(f) + "," + Game(second)), kRelayListTwice, "two games with one region and code are refused");

	// A refusal after some games were read keeps none of them
	GameFields bad;
	bad.code = "\"ZZZZZZZZ\"";
	bad.players = "0";
	Refused(ListOf(Game(f) + "," + Game(bad)), kRelayListBadNumber, "a bad second game refuses the whole list, the good first one too");
}

static void TestOneGame()
{
	puts("3. a game on the list");

	Refused(ListOf("5"), kRelayListNotAnEntry, "a number where a game should be is refused");
	Refused(ListOf("\"game\""), kRelayListNotAnEntry, "and a text");
	Refused(ListOf("[]"), kRelayListNotAnEntry, "and a list");
	Refused(ListOf("{}"), kRelayListFieldMissing, "a game with no fields is refused");

	GameFields f;
	f.extra = ",\"address\":\"203.0.113.9\"";
	Refused(ListOf(Game(f)), kRelayListUnknownField, "a game with an address on it is refused: the list never holds one");

	f.extra = ",\"Code\":\"ABCDEFGH\"";
	Refused(ListOf(Game(f)), kRelayListUnknownField, "a field name in other capitals is another field, and refused");

	f.extra = ",\"" + std::string(40, 'a') + "\":1";
	Refused(ListOf(Game(f)), kRelayListUnknownField, "and a field name 40 letters long");

	f.extra = ",\"name\":\"again\"";
	Refused(ListOf(Game(f)), kRelayListFieldTwice, "a field given twice is refused");

	f = GameFields();
	std::string missing = Game(f);
	missing.replace(missing.find(",\"hostMs\":42"), strlen(",\"hostMs\":42"), "");
	Refused(ListOf(missing), kRelayListFieldMissing, "a game missing a field is refused");

	// The host's fingerprint (net_identity.h): every game has one, of exactly 32 hex digits in small letters, read by net_identity.h's own
	// rule, the one the joining code uses
	std::string noFingerprint = Game(f);
	std::string const fingerprintField = ",\"fingerprint\":" + f.fingerprint;
	noFingerprint.replace(noFingerprint.find(fingerprintField), fingerprintField.size(), "");
	Refused(ListOf(noFingerprint), kRelayListFieldMissing, "a game without its host's fingerprint is refused, so no joiner meets a host it can't check");

	struct BadFingerprint
	{
		const char *text;
		const char *what;
	};

	BadFingerprint const badPrints[] = {
		{ "\"0123456789ABCDEFFEDCBA9876543210\"", "a fingerprint in capitals is refused: the list writes small letters" },
		{ "\"0123456789abcdeffedcba987654321\"", "and one of 31 digits" },
		{ "\"0123456789abcdeffedcba98765432100\"", "and one of 33" },
		{ "\"0123456789abcdefgedcba9876543210\"", "and one with a letter that isn't a hex digit" },
		{ "\"0123456789abcdef fedcba987654321\"", "and one with a space in it" },
		{ "\"\"", "and an empty one" },
		{ "\"0123456789abcdeffedcba987654321\\\\\"", "and one whose last character is an escaped \\" },
	};

	for (size_t i = 0; i < sizeof(badPrints) / sizeof(badPrints[0]); i++)
	{
		f = GameFields();
		f.fingerprint = badPrints[i].text;
		Refused(ListOf(Game(f)), kRelayListBadFingerprint, badPrints[i].what);
	}

	f = GameFields();
	f.fingerprint = "\"0123456789abcdeffedcba98765432\\u0030\"";
	Refused(ListOf(Game(f)), kRelayListBadEscape, "a fingerprint with an escape the list never writes is refused for that");
	f = GameFields();

	Refused(ListOf(Game(f).substr(0, 30)), kRelayListEndsTooSoon, "a game that stops halfway is refused");
	Refused(ListOf("{\"code\":\"ABCD"), kRelayListEndsTooSoon, "and one that stops inside a text");

	std::string colonless = Game(f);
	colonless.replace(colonless.find("\"name\":"), 7, "\"name\" ");
	Refused(ListOf(colonless), kRelayListNotWhatComesNext, "a field without its : is refused");
}

static void TestTexts()
{
	puts("4. names, levels and modes");

	GameFields f;
	f.name = "\"\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "an empty name is refused");

	f.name = "\"" + std::string(33, 'x') + "\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "a name of 33 characters is refused");

	f = GameFields();
	f.level = "\"" + std::string(40, 'y') + "\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "a level of 40 characters is refused");

	f = GameFields();
	f.mode = "\"" + std::string(33, 'z') + "\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "a mode of 33 characters is refused");

	f = GameFields();
	f.level = "\"tab\there\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "a level with a tab in it is refused");

	f.level = "\"line\nend\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "and with a line end");

	f.level = "\"del\x7f\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "and with the character 0x7F");

	f.level = "\"caf\xc3\xa9\"";
	Refused(ListOf(Game(f)), kRelayListBadText, "and with a letter outside ASCII");

	f = GameFields();
	f.mode = "\"\\u0041\"";
	Refused(ListOf(Game(f)), kRelayListBadEscape, "a \\u escape is refused, even for a plain letter");

	f.mode = "\"a\\nb\"";
	Refused(ListOf(Game(f)), kRelayListBadEscape, "and a \\n");

	f.mode = "\"a\\/b\"";
	Refused(ListOf(Game(f)), kRelayListBadEscape, "and a \\/");

	Refused("{\"games\":[{\"mode\":\"ends with \\", kRelayListEndsTooSoon, "a \\ at the very end of the answer is refused");

	f = GameFields();
	f.name = "Hicks";
	Refused(ListOf(Game(f)), kRelayListNotWhatComesNext, "a name without quotes is refused");

	f = GameFields();
	f.extra = ",\"na\\u006de\":\"x\"";
	Refused(ListOf(Game(f)), kRelayListBadEscape, "and an escape in a field's name");
}

static void TestCodesAndRegions()
{
	puts("5. codes and regions");

	const char *const bad[] = { "ABCDEFG", "ABCDEFGHJ", "ABCDEFGO", "ABCDEFG1", "ABCDEFG0", "ABCDEFGI", "abcdefgh", "ABCD EFG", "" };
	const char *const why[] = { "a code of 7 letters is refused", "and of 9", "and with an O in it", "and with a 1", "and with a 0", "and with an I",
		"and in small letters", "and with a space", "and an empty one" };

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		GameFields f;
		f.code = std::string("\"") + bad[i] + "\"";
		Refused(ListOf(Game(f)), kRelayListBadCode, why[i]);
	}

	GameFields f;
	f.code = "12345678";
	Refused(ListOf(Game(f)), kRelayListNotWhatComesNext, "a code without quotes is refused");

	Check(Relay_IsCode("23456789", 8) && Relay_IsCode("ZYXWVUTS", 8), "codes of code letters are codes");
	Check(!Relay_IsCode("2345678", 7) && !Relay_IsCode(NULL, 8), "a short one and none at all aren't");

	const char *const badRegions[][2] = {
		{ "\"\"", "an empty region is refused" }, { "\"SaoPaulo\"", "and one with capitals" }, { "\"sao paulo\"", "and one with a space" },
		{ "\"sao_paulo\"", "and one with an _" }, { "\"abcdefghijklmnopq\"", "and one of 17 characters" },
	};

	for (size_t i = 0; i < sizeof(badRegions) / sizeof(badRegions[0]); i++)
	{
		f = GameFields();
		f.region = badRegions[i][0];
		Refused(ListOf(Game(f)), kRelayListBadRegion, badRegions[i][1]);
	}

	Check(RelayList_IsRegion("saopaulo") && RelayList_IsRegion("us-east-1") && RelayList_IsRegion("abcdefghijklmnop"), "regions like these are taken");
	Check(!RelayList_IsRegion(NULL), "and no region at all isn't one");
}

static void TestNumbers()
{
	puts("6. numbers");

	// Each of these as a game's number of players, and what refusing it shows
	const char *const values[][2] = {
		{ "0", "0 players are refused" }, { "9", "and 9" }, { "-1", "and -1" }, { "1.0", "and 1.0" }, { "1e0", "and 1e0" },
		{ "01", "and 01" }, { "\"2\"", "and \"2\", a text" }, { "2 3", "and two numbers" }, { "", "and none" },
	};

	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
	{
		GameFields f;
		f.players = values[i][0];
		RelayListRefusal const expected = (strcmp(values[i][0], "2 3") == 0) ? kRelayListNotWhatComesNext : kRelayListBadNumber;
		Refused(ListOf(Game(f)), expected, values[i][1]);
	}

	struct OutOfRange
	{
		std::string GameFields::*field;
		const char *value;
		const char *what;
	};

	OutOfRange const outOfRange[] = {
		{ &GameFields::maxPlayers, "9", "room for 9 players is refused" },
		{ &GameFields::version, "0", "version 0 is refused" },
		{ &GameFields::version, "65536", "and 65536" },
		{ &GameFields::gameStyle, "256", "game style 256 is refused" },
		{ &GameFields::levelNumber, "101", "level number 101 is refused" },
		{ &GameFields::coopLobby, "256", "co-op lobby 256 is refused" },
		{ &GameFields::hostMs, "10000", "a host's round trip of 10,000 ms is refused" },
		{ &GameFields::seconds, "31536001", "a game listed for more than a year is refused" },
		{ &GameFields::seconds, "9999999999", "and a number of 10 digits, without overflowing" },
		{ &GameFields::seconds, "99999999999999999999999999", "and one of 26 digits" },
	};

	for (size_t i = 0; i < sizeof(outOfRange) / sizeof(outOfRange[0]); i++)
	{
		GameFields f;
		f.*(outOfRange[i].field) = outOfRange[i].value;
		Refused(ListOf(Game(f)), kRelayListBadNumber, outOfRange[i].what);
	}

	GameFields f;
	f.players = "5";
	f.maxPlayers = "4";
	Refused(ListOf(Game(f)), kRelayListMorePlayersThanRoom, "5 players in room for 4 are refused");

	f = GameFields();
	f.version = "112";
	Refused(ListOf(Game(f)), kRelayListOtherVersion, "a game for another version than the one asked for is refused");
}

static void TestOwner()
{
	puts("8. the owner a POST gives back");

	uint8_t owner[kRelayListSecretBytes];
	memset(owner, 0xEE, sizeof(owner));
	// Digits that read differently each way round, so a byte read with its two digits swapped is noticed
	std::string const good = "{\"owner\":\"0123456789abcdef0123456789abcdef\"}";
	Check(RelayList_ReadOwner(good.data(), good.size(), owner) == kRelayListAccepted && owner[0] == 0x01 && owner[1] == 0x23 &&
		owner[7] == 0xEF && owner[15] == 0xEF, "an owner of 32 hex digits is read, as its 16 bytes");

	const char *const bad[][2] = {
		{ "{\"owner\":\"00112233445566778899AABBCCDDEEFF\"}", "an owner in capital letters is refused" },
		{ "{\"owner\":\"00112233445566778899aabbccddeef\"}", "and one of 31 digits" },
		{ "{\"owner\":\"00112233445566778899aabbccddeeff0\"}", "and one of 33" },
		{ "{\"owner\":\"0011223344556677889gaabbccddeeff\"}", "and one with a g" },
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		uint8_t kept[kRelayListSecretBytes];
		memset(kept, 0xEE, sizeof(kept));
		RelayListRefusal const why = RelayList_ReadOwner(bad[i][0], strlen(bad[i][0]), kept);
		Check(why == kRelayListBadSecret && kept[0] == 0xEE, bad[i][1]);
	}

	std::string const twoFields = "{\"owner\":\"00112233445566778899aabbccddeeff\",\"x\":1}";
	uint8_t kept[kRelayListSecretBytes];
	memset(kept, 0xEE, sizeof(kept));
	Check(RelayList_ReadOwner(twoFields.data(), twoFields.size(), kept) == kRelayListNotItsField && kept[0] == 0xEE,
		"an answer with more than the owner is refused, and gives no owner");
	Check(RelayList_ReadOwner("{\"code\":\"ABCDEFGH\"}", 19, kept) == kRelayListNotItsField, "and one with something else instead");
}

static void TestWords()
{
	puts("9. the words for each refusal");

	bool allHaveWords = true;
	bool allDiffer = true;

	for (int a = kRelayListAccepted; a <= kRelayListNotOurKey; a++)
	{
		const char *const words = RelayList_RefusalWords(static_cast<RelayListRefusal>(a));
		allHaveWords = allHaveWords && words && words[0] && strstr(words, "no words") == NULL;

		for (int b = kRelayListAccepted; b < a; b++)
		{
			allDiffer = allDiffer && strcmp(words, RelayList_RefusalWords(static_cast<RelayListRefusal>(b))) != 0;
		}
	}

	Check(allHaveWords, "every refusal has words of its own");
	Check(allDiffer, "and no two have the same words");
}

static RelayListListing GoodListing(void)
{
	RelayListListing l;
	memset(&l, 0, sizeof(l));
	l.region = "saopaulo";
	l.code = "ABCDEFGH";

	for (int i = 0; i < kRelayListSecretBytes; i++)
	{
		l.pass[i] = static_cast<uint8_t>(i * 17);
	}

	l.passTime = 1762646400123ull;
	l.name = "Hicks's game";
	l.level = "Derelict";
	l.mode = "Deathmatch";
	l.gameStyle = 0;
	l.levelNumber = 3;
	l.coopLobby = 0;
	l.players = 2;
	l.maxPlayers = 4;
	l.version = 113;
	l.hostMs = 42;
	l.fingerprint = kGameFingerprint;
	return l;
}

static void TestBodies()
{
	puts("10. the bodies the game sends");

	char body[kRelayListMostBodyBytes + 1];
	RelayListListing l = GoodListing();

	Check(RelayList_ListingBody(l, true, NULL, body, sizeof(body)), "a listing is made");
	Check(strcmp(body, "{\"region\":\"saopaulo\",\"code\":\"ABCDEFGH\",\"pass\":\"00112233445566778899aabbccddeeff\",\"passTime\":1762646400123,"
		"\"name\":\"Hicks's game\",\"level\":\"Derelict\",\"mode\":\"Deathmatch\",\"gameStyle\":0,\"levelNumber\":3,\"coopLobby\":0,\"players\":2,"
		"\"maxPlayers\":4,\"version\":113,\"fingerprint\":\"0123456789abcdeffedcba9876543210\",\"hostMs\":42}") == 0,
		"exactly as the list takes a POST, the pass and the host's fingerprint in small-letter hex");

	uint8_t owner[kRelayListSecretBytes];
	memset(owner, 0xAB, sizeof(owner));
	std::string const putStart = "{\"owner\":\"abababababababababababababababab\",\"region\":";
	Check(RelayList_ListingBody(l, true, owner, body, sizeof(body)) && std::string(body).compare(0, putStart.size(), putStart) == 0,
		"a PUT is the same, with the owner first");

	l.passTime = kRelayListMostPassTime;
	Check(RelayList_ListingBody(l, true, NULL, body, sizeof(body)) && strstr(body, "\"passTime\":9007199254740991,") != NULL, "the largest passTime is written whole");

	l.passTime = kRelayListMostPassTime + 1;
	Check(!RelayList_ListingBody(l, true, NULL, body, sizeof(body)) && body[0] == '\0', "and one past it isn't sent");

	l = GoodListing();
	l.name = "say \"hi\" \\o/";
	Check(RelayList_ListingBody(l, true, NULL, body, sizeof(body)) && strstr(body, "\"name\":\"say \\\"hi\\\" \\\\o/\"") != NULL, "a \" and a \\ in a name are escaped");

	// The largest body: every text at its longest, made of characters JSON writes as two, under the 1 KB the list takes
	std::string const name(32, '"'), level(39, '\\'), mode(32, '"');
	l.name = name.c_str();
	l.level = level.c_str();
	l.mode = mode.c_str();
	l.passTime = kRelayListMostPassTime;
	l.gameStyle = l.coopLobby = 255;
	l.levelNumber = 100;
	l.players = l.maxPlayers = 8;
	l.version = 65535;
	l.hostMs = 9999;
	l.region = "abcdefghijklmnop";
	Check(RelayList_ListingBody(l, true, owner, body, sizeof(body)) && strlen(body) < kRelayListMostBodyBytes, "the largest PUT fits in the 1 KB the list takes");

	struct Bad
	{
		void (*make)(RelayListListing &l);
		const char *what;
	};

	Bad const bad[] = {
		{ [](RelayListListing &x) { x.name = ""; }, "an empty name isn't sent" },
		{ [](RelayListListing &x) { x.name = "123456789012345678901234567890123"; }, "nor a name of 33 characters" },
		{ [](RelayListListing &x) { x.level = "1234567890123456789012345678901234567890"; }, "nor a level of 40" },
		{ [](RelayListListing &x) { x.level = "caf\xc3\xa9"; }, "nor a level outside ASCII" },
		{ [](RelayListListing &x) { x.region = "Sao"; }, "nor a region with capitals" },
		{ [](RelayListListing &x) { x.code = "ABCDEFGO"; }, "nor a code with an O" },
		{ [](RelayListListing &x) { x.players = 0; }, "nor 0 players" },
		{ [](RelayListListing &x) { x.players = 5; }, "nor more players than room" },
		{ [](RelayListListing &x) { x.maxPlayers = 9; x.players = 9; }, "nor room for 9" },
		{ [](RelayListListing &x) { x.version = 65536; }, "nor version 65536" },
		{ [](RelayListListing &x) { x.gameStyle = 256; }, "nor game style 256" },
		{ [](RelayListListing &x) { x.levelNumber = 101; }, "nor level number 101" },
		{ [](RelayListListing &x) { x.coopLobby = -1; }, "nor a co-op lobby of -1" },
		{ [](RelayListListing &x) { x.hostMs = 10000; }, "nor a round trip of 10,000 ms" },
		{ [](RelayListListing &x) { x.fingerprint = NULL; }, "nor a listing with no fingerprint" },
		{ [](RelayListListing &x) { static const uint8_t none[kNetIdentityFingerprintBytes] = {}; x.fingerprint = none; },
			"nor one whose fingerprint is all 0, which is what an identity never made looks like" },
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		RelayListListing x = GoodListing();
		bad[i].make(x);
		Check(!RelayList_ListingBody(x, true, NULL, body, sizeof(body)) && body[0] == '\0', bad[i].what);
	}

	l = GoodListing();
	Check(!RelayList_ListingBody(l, true, NULL, body, 40) && body[0] == '\0', "a body with too little room isn't sent, and is left empty");

	Check(RelayList_EndingBody("saopaulo", "ABCDEFGH", owner, body, sizeof(body)) &&
		strcmp(body, "{\"region\":\"saopaulo\",\"code\":\"ABCDEFGH\",\"owner\":\"abababababababababababababababab\"}") == 0,
		"the end of a listing is its region, code and owner");
	Check(!RelayList_EndingBody("saopaulo", "ABCDEFGH", owner, body, 40) && body[0] == '\0', "and with too little room isn't sent");
	Check(!RelayList_EndingBody("saopaulo", "ABCDEFGH", NULL, body, sizeof(body)), "nor with no owner");

	char fitted[kRelayListLevelLetters + 1];
	RelayList_FitText("Hicks\x01's g\xe9me", "Game", fitted, kRelayListTextLetters);
	Check(strcmp(fitted, "Hicks?'s g?me") == 0, "the game's own words have anything outside printable ASCII made ?");
	RelayList_FitText(std::string(50, 'y').c_str(), "Game", fitted, kRelayListLevelLetters);
	Check(strlen(fitted) == 39, "and are cut to the most for their field");
	RelayList_FitText("", "Game", fitted, kRelayListTextLetters);
	Check(strcmp(fitted, "Game") == 0, "and nothing becomes the fallback");
	RelayList_FitText(NULL, NULL, fitted, kRelayListTextLetters);
	Check(strcmp(fitted, "?") == 0, "and with no fallback either, a ?, so it's never empty");
}

static void TestModeWords()
{
	puts("12. the words for a game's style");

	Check(strcmp(RelayList_ModeWords(0, false), "Deathmatch") == 0 && strcmp(RelayList_ModeWords(5, false), "Alien tag") == 0,
		"each style has its words");
	Check(strcmp(RelayList_ModeWords(4, false), "Co-op") == 0 && strcmp(RelayList_ModeWords(4, true), "Co-op campaign") == 0,
		"and co-op says when it's the campaign");
	Check(strcmp(RelayList_ModeWords(6, false), "Game") == 0 && strcmp(RelayList_ModeWords(-1, false), "Game") == 0,
		"a style the game doesn't have is just a game");

	bool allFit = true;

	for (int style = -1; style <= 6; style++)
	{
		for (int campaign = 0; campaign < 2; campaign++)
		{
			const char *const words = RelayList_ModeWords(style, campaign != 0);
			char fitted[kRelayListTextLetters + 1];
			RelayList_FitText(words, "x", fitted, kRelayListTextLetters);
			allFit = allFit && strcmp(fitted, words) == 0;
		}
	}

	Check(allFit, "and every one of them fits the list as it is");
}

// A source file, as text, from the top of the repository
static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static bool Has(const std::string &text, const char *piece)
{
	return text.find(piece) != std::string::npos;
}

static size_t CountOf(const std::string &text, const char *piece);   // below

static void TestTheCalls()
{
	puts("13. how relay_list.cpp makes its calls (read, not run: it needs the network)");

	std::string const source = ReadSource("3dc/relay_list.cpp");
	Check(source.size() > 5000, "the source was found and read, from the top of the repository");

	// Modified 2026-11-10 by Melanikus for the AvP port (#175): the list is its servers.cfg line's, trusted by its key alone (relay_list.cpp's
	// top, TRUSTED BY ITS KEY ALONE). The call is CallChecked; Call is every call after the first.
	std::string const call = SourceSlice(source, "Answer CallChecked(const wchar_t *method", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(call, "WINHTTP_FLAG_SECURE)"), "every call is HTTPS");
	Check(Has(call, "WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;") && !Has(source, "TLS1_1") && !Has(source, "PROTOCOL_TLS1 ") &&
		!Has(source, "PROTOCOL_SSL") && !Has(source, "PROTOCOL_ALL"), "with TLS 1.2 or newer, and nothing older named anywhere");

	// Nothing goes to a server whose keys didn't match its line, and nothing but the first call goes before they're checked
	size_t const refusedFirst = call.find("if (shared->keyState == kKeyRefused || (!check.keepIt && shared->keyState != kKeyGood))");
	size_t const sessionOpened = call.find("WinHttpOpen(");
	Check(refusedFirst != std::string::npos && sessionOpened != std::string::npos && refusedFirst < sessionOpened,
		"a call to a server whose keys didn't match, or any but the first before they're checked, ends before anything is opened");

	// Windows' certificate rules are switched off in one place, for these calls, and only once the key's check is in place for the call
	std::string const passLine = "\t\tDWORD pass = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |\n"
		"\t\t\tSECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;\n";
	size_t const passMade = call.find(passLine);
	size_t const callback = call.find("WinHttpSetStatusCallback(request, CheckTheKey, WINHTTP_CALLBACK_FLAG_SEND_REQUEST, 0)");
	size_t const passed = call.find("WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &pass, sizeof(pass))");
	size_t const firstSend = call.find("WinHttpSendRequest(");
	Check(CountOf(source, "SECURITY_FLAG_IGNORE") == 4 && CountOf(source, "WINHTTP_OPTION_SECURITY_FLAGS") == 1 && passMade != std::string::npos,
		"Windows' certificate rules (issuer, name, dates, use) are switched off in one place, which the key's check stands in for");
	Check(callback != std::string::npos && passed != std::string::npos && firstSend != std::string::npos && passMade < callback && callback < passed &&
		passed < firstSend, "and the key's check is in place before that, and both before the call's first byte");

	std::string const keyCheck = SourceSlice(source, "void CALLBACK CheckTheKey(", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(keyCheck, "status != WINHTTP_CALLBACK_STATUS_SENDING_REQUEST") && Has(keyCheck, "WINHTTP_OPTION_SERVER_CERT_CONTEXT") &&
		Has(keyCheck, "NetServerKey_OfCertificate(certificate->pbCertEncoded, certificate->cbCertEncoded, check->shown,") &&
		Has(keyCheck, "CertFreeCertificateContext(certificate);"),
		"the key's check reads the list's key from its certificate just before the first byte, and gives the certificate back");
	Check(Has(keyCheck, "if (check->keepIt)\n\t\t{\n\t\t\tcheck->matched = read;") &&
		Has(keyCheck, "bool const sameSize = read && check->shownSize == shared->goodKeySize;") &&
		Has(keyCheck, "differences |= static_cast<uint8_t>(check->shown[i] ^ shared->goodKey[i]);") &&
		Has(keyCheck, "check->matched = sameSize && differences == 0;"),
		"the first call keeps the key it was shown; every other must be shown the key that matched, every byte compared");
	Check(Has(keyCheck, "if (!check->matched)\n\t\t{\n\t\t\tcheck->closed = true;\n\t\t\tWinHttpCloseHandle(request);"),
		"and a key that isn't that one, or none it could read, closes the call there, unsent");
	size_t const closedSaid = call.find("if (check.closed)\n\t\t{\n\t\t\ta.why = check.looked ? kKeyChanged : kKeyUnread;");
	size_t const unreadSaid = call.find("if (!check.matched)\n\t\t{\n\t\t\ta.why = kKeyUnread;");
	size_t const firstRead = call.find("WinHttpReceiveResponse(");
	Check(closedSaid != std::string::npos && unreadSaid != std::string::npos && firstRead != std::string::npos && firstSend < closedSaid &&
		closedSaid < unreadSaid && unreadSaid < firstRead, "a call the check closed says so, and a call whose check never ran has nothing of its answer read");

	// Added 2026-11-09 by Melanikus for the AvP port (Primary's read of the pin): the request goes out with its headers alone, and its body
	// (a listing's pass and owner token) is written only once the key's check has passed, and all of it or the call fails
	size_t const bodyWritten = call.find("bool const bodyWent = sent && (!body || (WinHttpWriteData(request, body, bodyBytes, &written) && written == bodyBytes));");
	Check(CountOf(call, "WinHttpSendRequest(") == 2 && CountOf(call, "WINHTTP_NO_REQUEST_DATA, 0, ") == 2 && !Has(call, "const_cast<char *>(body)"),
		"the request goes out with its headers alone: neither way of sending it carries the body");
	Check(bodyWritten != std::string::npos && unreadSaid < bodyWritten && bodyWritten < firstRead && CountOf(call, "WinHttpWriteData(") == 1,
		"the body is written once, whole, only after the key's check has passed, and before the answer is waited for");
	// and a request the key's check closed is never used again, nor closed a second time
	std::string const stillOpen = SourceSlice(source, "HINTERNET StillOpen(const KeyCheck &check, HINTERNET request)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(stillOpen, "return check.closed ? NULL : request;"), "StillOpen gives nothing to close for a request the key's check closed");
	std::string const afterTheSend = firstSend != std::string::npos ? call.substr(firstSend) : std::string();
	size_t const closes = CountOf(afterTheSend, "CloseAll(");
	size_t const safeCloses = CountOf(afterTheSend, "CloseAll(NULL, connection, session);") +
		CountOf(afterTheSend, "CloseAll(StillOpen(check, request), connection, session);");
	Check(closes >= 9 && closes == safeCloses, "after the send, every one of the call's 9 or more closes leaves out a request the check closed");
	size_t const closedAgain = firstRead != std::string::npos ? call.find("if (check.closed)\n\t\t{\n\t\t\ta.why = check.looked ? kKeyChanged : kKeyUnread;", firstRead) : std::string::npos;
	size_t const notAnswered = call.find("if (!answered)");
	Check(Has(call, "bodyWent && !check.closed && WinHttpReceiveResponse(request, NULL)") && closedAgain != std::string::npos &&
		notAnswered != std::string::npos && closedAgain < notAnswered, "a call the check closed while the answer was awaited ends there, unread");
	Check(Has(call, "WINHTTP_DISABLE_REDIRECTS") && Has(call, "WINHTTP_DISABLE_COOKIES") && Has(call, "WINHTTP_DISABLE_AUTHENTICATION"),
		"redirects, cookies and signing in are off");
	Check(Has(call, "WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000)"), "each step has 5 seconds");
	Check(Has(call, "if (got >= room)") && Has(call, "a.why = kTooBig;"), "an answer bigger than its room is refused, not read on");

	// The first call, and the check of the server against its line
	std::string const first = SourceSlice(source, "const char *CheckTheServer(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(first, "first.keepIt = true;") && Has(first, "CallChecked(L\"GET\", L\"/v1/relay-keys\", NULL, keysAnswer, sizeof(keysAnswer), first)") &&
		CountOf(source, "keepIt = true;") == 1, "SOURCE: the one call that keeps the key it's shown is GET /v1/relay-keys, which asks for nothing and sends nothing");
	Check(Has(first, "RelayList_ReadRelayKeys(keysAnswer, a.bytes, region, keys.p384, keys.kem)") &&
		Has(first, "bool const good = NetServerFingerprint_IsTheLines(first.shown, first.shownSize, &keys, shared->lineFingerprint);") &&
		Has(first, "shared->keyState = good ? kKeyGood : kKeyRefused;"),
		"its answer is read as hostile, and the key it was shown and the relay's keys must make the line's fingerprint, or the server is refused for the run");
	std::string const later = SourceSlice(source, "Answer Call(const wchar_t *method", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const checkedFirst = later.find("const char *const unchecked = CheckTheServer();");
	size_t const laterCall = later.find("return CallChecked(method, path, body, answer, room, check);");
	Check(checkedFirst != std::string::npos && laterCall != std::string::npos && checkedFirst < laterCall && Has(later, "check.keepIt = false;"),
		"every later call checks the server first, and goes only to the key that matched");
	Check(!Has(source, "getaddrinfo") && !Has(source, "gethostbyname") && !Has(source, "Config_Get"),
		"nothing is looked up by name, and nothing about a server is read from the settings file");
	Check(Has(source, "char gamesAnswer[kRelayListMostGamesBytes];") && Has(source, "RelayList_ReadGames(gamesAnswer, a.bytes, shared->version, shared->v2Draft"),
		"the list of games is read into 48 KB, and through the hostile reader, for this version");

	// Modified 2026-11-09 by Melanikus for the AvP port: the version and the v2 draft (networking.h) are this copy's, read once on the main
	// thread when the list starts, so a copy without the draft asks for, reads and writes the list exactly as version 1 does
	Check(Has(source, "made->version = Net_OurVersion();") && Has(source, "made->v2Draft = Net_V2DraftOn();")
		&& Has(source, "L\"/v1/games?version=%d\", shared->version);") && Has(source, "RelayList_ListingBody(send, shared->v2Draft, "),
		"SOURCE: the list asks for this copy's version's games, and reads and writes fingerprints only with the v2 draft");
	std::string const relaysGiven = SourceSlice(source, "bool RelayList_Relays(RelayListRelay *relays, int *count)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(relaysGiven, "bool const known = shared->keyState == kKeyGood;") && Has(relaysGiven, "relays[0].address = shared->relayAddress;") &&
		Has(relaysGiven, "*count = 1;"), "and the one relay is the server's own, at its line's address, known only once the server is checked and good");

	std::string const thread = SourceSlice(source, "// ---- The list's thread", "// Makes the store and starts the thread", kSliceEndAfterTheStart, FailedSlice);
	Check(thread.size() > 1000 && !Has(thread, "LogString(") && !Has(thread, "LogErrorString("), "the list's own thread never writes to the game's log");
	Check(!Has(source, " printf(") && !Has(source, "\tprintf(") && !Has(source, "OutputDebugString"), "and nothing prints what a call sent or got");

	// The thread's news: every piece since the main thread last looked, kept in a ring, each written with "Relay list: " in front, so one
	// search of the log finds them all
	std::string const news = SourceSlice(source, "void WriteTheNews(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(news, "for (; newsWritten < number; newsWritten++)") && Has(news, "LogString(std::string(\"Relay list: \") + piece);"),
		"the main thread writes every piece of the list's news, each starting \"Relay list: \"");

	// Modified 2026-11-09 by Melanikus for the AvP port (Snyk Code): each piece is a copy of its text, kept with its end, never a pointer
	// handed from one thread to the other, and it's ended again at the last place of its room before it's written
	std::string const tell = SourceSlice(source, "void Tell(const char *news)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(tell, "size_t const letters = strnlen(news, kMostNewsLetters);") && Has(tell, "memcpy(kept, news, letters);") && Has(tell, "kept[letters] = 0;"),
		"each piece of news is kept as a copy of at most kMostNewsLetters letters, with its end");
	Check(Has(news, "piece[kMostNewsLetters] = 0;"), "and it's ended again at the last place of its room before it's written");

	// Until version 2 is on for players, there's a server only with -dev, so a player's game reaches none: without -dev, servers.cfg isn't
	// even read. The server is the file's first good line, and its address is written out from its four numbers, never a name.
	std::string const serverName = SourceSlice(source, "const char *Relay_ServerName(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const devFirst = serverName.find("if (!DevMode)\n\t{\n\t\treturn \"\";\n\t}");
	size_t const fileRead = serverName.find("Server()");
	Check(devFirst != std::string::npos && fileRead != std::string::npos && devFirst < fileRead,
		"without -dev, there's no server, and servers.cfg isn't read");
	std::string const theServer = SourceSlice(source, "const ServersCfgServer *Server(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(theServer, "ServersCfg_ReadTheGames(&file)") && Has(theServer, "theServer.line = file.servers[0];") && !Has(theServer, ".name"),
		"the server is servers.cfg's first good line, read from the game's folder, and the log never says its name");
	std::string const starting = SourceSlice(source, "bool Start(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(starting, "if (!Relay_ServerName()[0])") && Has(starting, "L\"%u.%u.%u.%u\", (line->address >> 24) & 0xFFu,") &&
		Has(starting, "made->port = static_cast<INTERNET_PORT>(line->listPort);") && Has(starting, "made->relayPort = line->relayPort;"),
		"and the list is started only for it, at its address, written as four numbers, and its ports");

	// Both keep their first answer for the run, so nothing may ask them before winmain.cpp reads -dev: winmain.cpp itself never asks.
	// Modified 2026-11-09 by Melanikus for the AvP port: its one call of the list's is RelayList_BeforeQuitting, after the menus have ended
	// and before the game shuts down, and that never starts the list (below).
	std::string const winmain = ReadSource("3dc/avp/win95/winmain.cpp");
	size_t const firstListCall = winmain.find("RelayList_");
	// The call itself, not the tab before it, so it's where winmain's first "RelayList_" is (Fourth's fix of 2026-11-09: with the tab the
	// two were a letter apart, and the check below could never pass)
	size_t const quitting = winmain.find("RelayList_BeforeQuitting(2000);");
	size_t const afterMenus = winmain.find("TimeStampedMessage(\"After Menus\");");
	size_t const exitSystem = winmain.find("\tExitSystem();\n");
	Check(winmain.size() > 10000 && !Has(winmain, "Relay_ServerName(") && !Has(winmain, "Relay_ListPort(") && firstListCall == quitting
		&& winmain.find("RelayList_", quitting + 1) == std::string::npos,
		"nothing asks for the list's name or port before -dev has been read: winmain's only call of the list's is the one as the game quits");
	Check(quitting != std::string::npos && afterMenus != std::string::npos && exitSystem != std::string::npos && afterMenus < quitting && quitting < exitSystem,
		"and that comes after the menus have ended, before the game shuts down");

	// As the game quits, a game still on the list comes off it, waiting at most that long for the list's thread to send the DELETE (server
	// #14's run: the game had gone before the DELETE did). The wait starts nothing, and the thread says it has caught up only after the DELETE.
	std::string const beforeQuitting = SourceSlice(source, "void RelayList_BeforeQuitting(unsigned int mostMs)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(beforeQuitting, "if (!shared)\n\t{\n\t\treturn;\n\t}") && !Has(beforeQuitting, "Start()") && Has(beforeQuitting, "RelayList_HostRoomClosed();"),
		"as the game quits, a game still on the list is asked off it, and a list never used isn't started for that");
	Check(Has(beforeQuitting, "if (waited >= mostMs)") && Has(beforeQuitting, "WaitForSingleObject(shared->endDone, mostMs - waited);"),
		"and the game waits for the list's thread no longer than it was told to");
	size_t const takenOff = thread.find("TakeOffTheList();\n");
	size_t const caughtUp = thread.find("shared->endsDone = endsAsked;");
	Check(takenOff != std::string::npos && caughtUp != std::string::npos && takenOff < caughtUp && Has(thread, "SetEvent(shared->endDone);"),
		"the list's thread says it has caught up only once the DELETE has been sent");

	// The host's fingerprint (net_identity.h). networking.cpp gives it once this run's identity is made. A room that opens before then isn't
	// listed at all, rather than listed with nothing a joiner could check, and the listing keeps the one it opened with for its whole life,
	// as a PUT must carry the POST's (a different one is refused by the list).
	std::string const identity = SourceSlice(source, "void RelayList_HostIdentity(const uint8_t fingerprint[kNetIdentityFingerprintBytes])\n{", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(identity, "hostIdentityGiven = false;") && Has(identity, "hostIdentityGiven = hostIdentityGiven || hostIdentity[i] != 0;"),
		"a fingerprint given to the host counts only if it isn't all 0, which is what an identity never made looks like");
	std::string const opened = SourceSlice(source, "void RelayList_HostRoomOpened(", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const noIdentity = opened.find("if (Net_V2DraftOn() && !hostIdentityGiven)\n\t{\n\t\tLogString(");
	size_t const started = opened.find("if (!Start())");
	Check(noIdentity != std::string::npos && started != std::string::npos && noIdentity < started,
		"a room that opens with no identity given isn't put on the list, and the log says so, before the list is even started");
	Check(Has(opened, "memcpy(l.fingerprint, hostIdentity, sizeof(l.fingerprint));"), "a listing keeps the fingerprint its room opened with");
	std::string const now = SourceSlice(source, "void ShowSession(const SessionDescription &session, Listing &l)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(!now.empty() && !Has(now, "fingerprint"), "and what the session says later never changes it");
	std::string const toSend = SourceSlice(source, "RelayListListing ListingToSend(const Listing &l)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(toSend, "out.fingerprint = l.fingerprint;"), "every POST and PUT of it sends it");
}

static size_t CountOf(const std::string &text, const char *piece)
{
	size_t count = 0;

	for (size_t at = text.find(piece); at != std::string::npos; at = text.find(piece, at + 1))
	{
		count++;
	}

	return count;
}

static void TestTheMenus()
{
	puts("14. when the menus ask the list (read, not run: the menus need the whole game)");

	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	Check(menus.size() > 100000, "menus.cpp was found and read, from the top of the repository");

	// The page of games found asks the list when it's first opened, and then only when the player asks again: Refresh on the list, or
	// Enter on a failure (the user's design: online_screens_page.h). Never on each answer, or one answer would bring the next, for as long
	// as the page is open, and never by itself on a timer.
	std::string const firstOpen = SourceSlice(menus, "\t\t\tif (previousMenuID != AVPMENU_MULTIPLAYERSELECTSESSION)\n\t\t\t{", "\n\t\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	std::string const askAgain = SourceSlice(menus, "\t\tif (OnlinePage_TimeToAskAgain())\n\t\t{", "\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(firstOpen, "RelayList_FindGames();") && Has(askAgain, "RelayList_FindGames();") && CountOf(menus, "RelayList_FindGames();") == 2,
		"the page of games found asks the list when it's opened and when the player asks again, and nowhere else");

	std::string const page = ReadSource("3dc/avp/win95/frontend/online_screens_page.cpp");
	Check(page.size() > 20000, "online_screens_page.cpp was found and read");
	std::string const timeToAsk = SourceSlice(page, "bool OnlinePage_TimeToAskAgain(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const findAgain = SourceSlice(page, "void OnlinePage_FindAgain(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(timeToAsk, "if (!askNow)\n\t{\n\t\treturn false;\n\t}") && !Has(timeToAsk, "Now()") && Has(findAgain, "askNow = true;") &&
		CountOf(page, "askNow = true;") == 1, "it's time to ask again only when Refresh or Enter has asked for it, never by the clock");
	Check(Has(findAgain, "if (pageState == kPageList || pageState == kPageCantConnect || pageState == kPageCantFetch)\n\t{\n\t\taskNow = true;"),
		"and only from the list or a failure, never while a fetch is going: one at a time");
	std::string const refresh = SourceSlice(menus, "\t\tcase AVPMENU_ELEMENT_FINDAGAIN:\n\t\t{", "\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(refresh, "if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)\n\t\t\t{\n\t\t\t\tOnlinePage_FindAgain();"),
		"and the page's Refresh row asks for it when it's chosen");

	// "Couldn't connect" and "connected, but couldn't fetch its open sessions" are told apart by the list code's own reason (the user's
	// design): no connection is the four reasons a call gives before any answer has come, compared as the very words relay_list.cpp hands
	// out. An answer that came and was too big, a no, or one the hostile reader refused, is a failure to fetch.
	std::string const list = ReadSource("3dc/relay_list.cpp");
	std::string const noConnection = SourceSlice(list, "bool RelayList_WhyIsNoConnection(const char *why)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(noConnection, "return why == kCantReach || why == kTooLong || why == kCertificate || why == kCantStart;"),
		"no connection to the list's server is: it couldn't be reached, took too long, its secure connection failed, or the call couldn't start");
	Check(!Has(noConnection, "kTooBig") && !Has(noConnection, "kSaidNo") && !Has(noConnection, "strcmp") && !Has(noConnection, "strstr"),
		"an answer that came isn't one of them, and nothing is compared by its letters");
	Check(Has(page, "listCantConnect = why != NULL && RelayList_WhyIsNoConnection(why);") &&
		Has(page, "pageState = listCantConnect ? kPageCantConnect : kPageCantFetch;"), "and the page shows which of the two it was by that");

	std::string const multiplayerPage = SourceSlice(menus, "\t\tcase AVPMENU_MULTIPLAYER:\n", "\t\t\tbreak;", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(multiplayerPage, "RelayList_FindRelays();"), "the multiplayer page asks where the relays are, before a game can be hosted");

	// A new search of the LAN empties the page's list of games, so the relay's games go back on it each time the page is set up: after
	// that search, and before the page's rows are made from the list
	std::string const setup = SourceSlice(menus, "\t\tcase AVPMENU_MULTIPLAYERSELECTSESSION:\n", "\t\t\tbreak;", kSliceEndAfterTheStart, FailedSlice);
	size_t const search = setup.find("Net_FindAvPSessions();");
	size_t const put = setup.find("PutTheRelayGamesOnThePage();");
	size_t const rows = setup.find("MakeSelectSessionMenu();");
	Check(search != std::string::npos && put != std::string::npos && rows != std::string::npos && search < put && put < rows,
		"the relay's games go back on the page each time it's set up, after the LAN's search and before its rows are made");
	Check(Has(menus, "if (Net_UpdateSessionList(selection) || TheRelayGamesHaveCome())"), "and an answer from the list sets the page up again");

	// The join's wait through a relay, up to about 26 seconds: the window keeps answering Windows, Esc or closing the game gives it up, and
	// the hook is there only while that one join waits
	std::string const hook = SourceSlice(menus, "static bool KeepTheWindowAliveWhileJoining(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(hook, "CheckForWindowsMessages();") && Has(hook, "return bRunning && !KeyboardInput[KEY_ESCAPE];"),
		"while a join through a relay waits, the window keeps answering, and Esc or closing the game gives it up");
	size_t const set = menus.find("Net_SetRelayJoinWaitHook(KeepTheWindowAliveWhileJoining);");
	size_t const join = menus.find("Net_ConnectToSession(s, MP_PlayerName);");
	size_t const unset = menus.find("Net_SetRelayJoinWaitHook(NULL);");
	Check(set != std::string::npos && join != std::string::npos && unset != std::string::npos && set < join && join < unset &&
		CountOf(menus, "Net_SetRelayJoinWaitHook(") == 2, "and that's only for as long as the join waits");

	// The wait is called from inside the menus' own frame, and Direct3D refuses a frame opened inside another (D3DERR_INVALIDCALL, in the
	// first footage's log). So the wait's drawing closes the menus' frame once, before its first frame of its own, and a new one is opened
	// for the menus' loop to finish once the wait is over, however it ended.
	std::string const joining = SourceSlice(page, "void OnlinePage_DrawJoining(const char *region)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const closesTheMenus = joining.find("if (!joinDrewFrames)\n\t{\n\t\tThisFramesRenderingHasFinished();\n\t\tjoinDrewFrames = true;\n\t}");
	size_t const opensItsOwn = joining.find("ThisFramesRenderingHasBegun();");
	Check(closesTheMenus != std::string::npos && opensItsOwn != std::string::npos && closesTheMenus < opensItsOwn,
		"the join's wait closes the menus' open frame, once, before it opens one of its own");
	std::string const waitOver = SourceSlice(page, "void OnlinePage_JoinWaitOver(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(waitOver, "if (joinDrewFrames)\n\t{\n\t\tjoinDrewFrames = false;\n\t\tThisFramesRenderingHasBegun();"),
		"and once it's over, a new frame is opened for the menus, only if the wait drew any");
	size_t const over = menus.find("OnlinePage_JoinWaitOver();");
	Check(unset != std::string::npos && over != std::string::npos && unset < over && over < menus.find("if (joined != NET_FAIL)", unset) &&
		CountOf(menus, "OnlinePage_JoinWaitOver();") == 1, "which the menus ask for straight after the join, before what it did is looked at");

	// A join goes to the game on its row, found by its relay code or its address (online_screens_page.h). Never by the row's number: each
	// search empties the list of games found and fills it again, so by the time Enter is pressed, a row's number can be another game's.
	Check(Has(menus, "int const s = OnlinePage_SessionForRow(AvPMenus.CurrentlySelectedElement);") && !Has(menus, "int s = AvPMenus.CurrentlySelectedElement;"),
		"a join goes to the game on its row, found by its code or its address, never by the row's number");

	// The menu's rows start with the Search box and Refresh, and the search hides some games, so a menu row is turned into the game the
	// search showed on it, the same way, both for the join and for the game the page selects again when the join ends
	std::string const sessionForRow = SourceSlice(page, "int OnlinePage_SessionForRow(int menuRow)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const joiningRow = SourceSlice(page, "void OnlinePage_Joining(int menuRow)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(sessionForRow, "int const row = ShownRowOf(menuRow);") && Has(joiningRow, "int const row = ShownRowOf(menuRow);") &&
		Has(joiningRow, "joinedKey = shownRows[row].key;"), "and the game remembered for the join's end is the same one, by the same rows");

	// A player asking to join: the host's two answer rows come in front of the page's own, and the selection moves down with the row it
	// was on, so it's never on "Let NAME in" by itself: Enter pressed by chance can't let anyone in
	std::string const answers = SourceSlice(menus, "static void UpdateTheAnswerRows(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(answers, "int const selected = AvPMenus.CurrentlySelectedElement + (ours ? 0 : 2);") && Has(answers, "answerRows[answer].MaxValue = answer == 0 ? 1 : 0;"),
		"the host's answer rows never take the selection when they come, and only the first lets the player in");

	// The Join page's first row is Find Games, not 1999's "Find Local Servers". Its help says only what a player's search does: it looks on
	// this network. The online list is asked only with -dev, and a release game reaches no server, so the help never promises it (the
	// owner's ruling). The words are the game's own (language.cpp, after everything language.txt holds), so a player's language file isn't
	// touched.
	std::string const pages = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const joinPage = SourceSlice(pages, "static AVPMENU_ELEMENT AvPMenu_MultiplayerJoinGame[]=", "\n};\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(joinPage, "{TEXTSTRING_MULTIPLAYER_FINDGAMES},") && Has(joinPage, "TEXTSTRING_MULTIPLAYER_FINDGAMES_HELP}") && !Has(joinPage, "FINDLOCAL"),
		"the Join page's first row is Find Games, with its own help, and nothing of the old local-servers words");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	Check(Has(words, "\t\"Find Games\",\n\t\"Press ENTER to look for games on this network.\",\n") && !Has(words, "online list of games\""),
		"and its help says only this network: nothing a player's game can't reach");

	// -dev -findgames: only with -dev, and then the player's own way to the page of games found, one page after another
	std::string const startUp = ReadSource("3dc/avp/win95/winmain.cpp");
	std::string const findGamesWord = SourceSlice(startUp, "if (CommandLineHasWord(command_line, \"-findgames\"))", "\n\t\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(findGamesWord, "if (DevMode)\n\t\t\t{\n\t\t\t\tOnlinePage_FindGamesAtStart();"), "-findgames does anything only with -dev");
	std::string const findGames = SourceSlice(menus, "if (OnlinePage_GoStraightToFindGames())", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const toMultiplayer = findGames.find("SetupNewMenu(AVPMENU_MULTIPLAYER);");
	size_t const toJoin = findGames.find("SetupNewMenu(AVPMENU_MULTIPLAYERJOINGAME);");
	size_t const toFind = findGames.find("SetupNewMenu(AVPMENU_MULTIPLAYERSELECTSESSION);");
	Check(toMultiplayer != std::string::npos && toJoin != std::string::npos && toFind != std::string::npos && toMultiplayer < toJoin && toJoin < toFind,
		"and it goes Multiplayer, then Join Game, then Find Games, each page set up as pressing it would");
}

// An IPv4 address with its first number in the highest byte, as RelayList_AddressRefusal takes it
static uint32_t Address(unsigned a, unsigned b, unsigned c, unsigned d)
{
	return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

static void TestAddresses()
{
	// Modified 2026-11-10 by Melanikus for the AvP port (#175): the kind of an address, which the servers.cfg reader uses to say which can be
	// a server's (servers_cfg_test tries that). It has no -dev half now: that was for the -dev settings, which are gone.
	puts("15. what kind of address an address is");

	struct Case
	{
		uint32_t address;
		RelayAddressRefusal kind;
		const char *what;
	};

	Case const cases[] = {
		{ Address(8, 8, 8, 8), kRelayAddressUsable, "an address out on the internet is usable" },
		{ Address(223, 255, 255, 255), kRelayAddressUsable, "and so is the last one before the multicast block" },
		{ Address(0, 0, 0, 0), kRelayAddressThisNetwork, "ANY, 0.0.0.0, is this network" },
		{ Address(0, 1, 2, 3), kRelayAddressThisNetwork, "and so is all of 0.0.0.0/8" },
		{ Address(1, 0, 0, 0), kRelayAddressUsable, "but not 1.0.0.0, just past it" },
		{ Address(255, 255, 255, 255), kRelayAddressBroadcast, "the broadcast address is broadcast" },
		{ Address(224, 0, 0, 1), kRelayAddressMulticast, "a multicast address is multicast" },
		{ Address(239, 255, 255, 255), kRelayAddressMulticast, "up to the last of them" },
		{ Address(240, 0, 0, 1), kRelayAddressReserved, "an address set aside is reserved" },
		{ Address(255, 255, 255, 254), kRelayAddressReserved, "up to the one before broadcast" },
		{ Address(127, 0, 0, 1), kRelayAddressLoopback, "this machine's own address is loopback" },
		{ Address(10, 1, 2, 3), kRelayAddressPrivate, "10/8 is private" },
		{ Address(172, 16, 0, 1), kRelayAddressPrivate, "172.16/12 is private" },
		{ Address(172, 31, 255, 255), kRelayAddressPrivate, "up to its last" },
		{ Address(172, 15, 255, 255), kRelayAddressUsable, "but not 172.15.255.255, just before it" },
		{ Address(172, 32, 0, 0), kRelayAddressUsable, "nor 172.32.0.0, just after it" },
		{ Address(192, 168, 1, 1), kRelayAddressPrivate, "192.168/16 is private" },
		{ Address(192, 169, 0, 1), kRelayAddressUsable, "but not 192.169.0.1" },
		{ Address(169, 254, 1, 1), kRelayAddressPrivate, "169.254/16 is private" },
		{ Address(100, 64, 0, 1), kRelayAddressPrivate, "100.64/10, a carrier's own, is private" },
		{ Address(100, 127, 255, 255), kRelayAddressPrivate, "up to its last too" },
		{ Address(100, 63, 255, 255), kRelayAddressUsable, "but not 100.63.255.255, just before it" },
		{ Address(100, 128, 0, 0), kRelayAddressUsable, "nor 100.128.0.0, just after it" },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		Check(RelayList_AddressRefusal(cases[i].address) == cases[i].kind, cases[i].what);
	}

	// Modified 2026-11-10 by Melanikus for the AvP port (#175): no name is looked up at all, now the server is a servers.cfg line with its
	// address written plainly, and its relay is at that same address
	std::string const source = ReadSource("3dc/relay_list.cpp");
	Check(!Has(source, "getaddrinfo") && !Has(source, "enet_address_set_host") && !Has(source, "gethostbyname") && !Has(source, "GetAddrInfo"),
		"nothing in relay_list.cpp looks a name up");
}

// ---- Asking for online access (server #24). Added 2026-11-09 by Melanikus for the AvP port.

static void TestAccessCodes()
{
	puts("16. the access code made from a fingerprint");

	// The same examples are in the server's own test of its approve command, so both sides make the same code from the same fingerprint.
	// They were worked out a second way, with whole-number arithmetic rather than bit by bit (Fourth's access-code.js, 2026-11-09).
	struct Example
	{
		const char *fingerprint;
		const char *code;
		const char *what;
	};

	Example const examples[] = {
		{ "00000000000000000000000000000000", "2222-2222-2222-2222", "all 0 is the alphabet's first letter, 16 times" },
		{ "ffffffffffffffffffffffffffffffff", "ZZZZ-ZZZZ-ZZZZ-ZZZZ", "all 1s is its last" },
		{ "000102030405060708090a0b0c0d0e0f", "222J-62S6-2N52-G42B", "a fingerprint counting up" },
		{ "0123456789abcdef0123456789abcdef", "26KN-CTWB-PH8Y-Y2B5", "another" },
		{ "f0e1d2c3b4a5968778695a4b3c2d1e0f", "Y5JX-7JXN-NQDA-GY5B", "and one counting down" },
		{ "80000000000000000000000000000000", "J222-2222-2222-2222", "the highest bit of the first byte is the first letter's highest" },
		{ "00000000000000000001000000000000", "2222-2222-2222-2223", "the 80th bit is the last letter's lowest" },
		{ "0000000000000000000000ffffffffff", "2222-2222-2222-2222", "bits past the 80th make no difference" },
		{ "08421084210842108421000000000000", "3333-3333-3333-3333", "and each letter is exactly 5 bits" }
	};

	for (size_t i = 0; i < sizeof(examples) / sizeof(examples[0]); i++)
	{
		uint8_t fingerprint[kNetIdentityFingerprintBytes];
		char code[kRelayAccessCodeTextBytes];
		memset(code, 'x', sizeof(code));
		bool const read = NetIdentity_FingerprintFromHex(examples[i].fingerprint, fingerprint);
		RelayAccess_Code(fingerprint, code);
		Check(read && strcmp(code, examples[i].code) == 0, examples[i].what);

		if (read && strcmp(code, examples[i].code) != 0)
		{
			printf("          it made %.20s, not %s\n", code, examples[i].code);
		}
	}
}

static void TestAccessNames()
{
	puts("17. the name an access request is sent with");

	Check(RelayAccess_NameRefusal("Melanikus") == kRelayAccessNameGood && RelayAccess_NameRefusal("A") == kRelayAccessNameGood &&
		RelayAccess_NameRefusal("Dwayne Hicks") == kRelayAccessNameGood, "a plain name can be sent, with spaces inside it");
	Check(RelayAccess_NameRefusal("ABCDEFGHIJKLMNO") == kRelayAccessNameGood, "and one of 15 letters");
	Check(RelayAccess_NameRefusal("ABCDEFGHIJKLMNOP") == kRelayAccessNameTooLong, "but not one of 16");
	Check(RelayAccess_NameRefusal("") == kRelayAccessNameEmpty && RelayAccess_NameRefusal(NULL) == kRelayAccessNameEmpty, "nor an empty one");
	Check(RelayAccess_NameRefusal(" Ripley") == kRelayAccessNameSpaceAtAnEnd && RelayAccess_NameRefusal("Ripley ") == kRelayAccessNameSpaceAtAnEnd,
		"nor one with a space first or last");
	Check(RelayAccess_NameRefusal("Bishop\"") == kRelayAccessNameBadLetter && RelayAccess_NameRefusal("C:\\Bishop") == kRelayAccessNameBadLetter,
		"nor one with a \" or a \\, which would need escaping");
	Check(RelayAccess_NameRefusal("Bis\thop") == kRelayAccessNameBadLetter && RelayAccess_NameRefusal("Bis\x7Fhop") == kRelayAccessNameBadLetter &&
		RelayAccess_NameRefusal("Jos\xE9") == kRelayAccessNameBadLetter && RelayAccess_NameRefusal("\x1B[2J") == kRelayAccessNameBadLetter,
		"nor one with a tab, a delete, a letter outside ASCII, or a terminal's escape");

	bool everyOtherSignGoes = true;

	for (int c = 0x20; c <= 0x7E; c++)
	{
		char name[4] = { 'a', static_cast<char>(c), 'b', '\0' };
		bool const allowed = c != '"' && c != '\\';
		everyOtherSignGoes = everyOtherSignGoes && ((RelayAccess_NameRefusal(name) == kRelayAccessNameGood) == allowed);
	}

	Check(everyOtherSignGoes, "every other printable character can be in a name, and those two can't");

	char name[kRelayAccessNameMostBytes + 1];
	RelayAccess_NameFromProfile("Melanikus", 16, name);
	Check(strcmp(name, "Melanikus") == 0, "a profile's name is the name to start from");
	RelayAccess_NameFromProfile("  Ripley  ", 16, name);
	Check(strcmp(name, "Ripley") == 0, "with the spaces at its ends taken off");
	RelayAccess_NameFromProfile("Jos\xE9 \"Hicks\"", 16, name);
	Check(strcmp(name, "Jos Hicks") == 0, "and the letters a name can't have left out");
	RelayAccess_NameFromProfile("ABCDEFGHIJKLMNOPQRST", 32, name);
	Check(strcmp(name, "ABCDEFGHIJKLMNO") == 0, "cut to 15 letters");
	RelayAccess_NameFromProfile("ABCDEFGHIJKLMN P", 32, name);
	Check(strcmp(name, "ABCDEFGHIJKLMN") == 0 && RelayAccess_NameRefusal(name) == kRelayAccessNameGood,
		"and with a space the cut leaves at the end taken off too");
	RelayAccess_NameFromProfile("ABCDEFGH", 4, name);
	Check(strcmp(name, "ABCD") == 0, "a profile's name is read no further than its room, ended or not");
	RelayAccess_NameFromProfile("\xE9\xE9\"\\", 16, name);
	Check(name[0] == '\0', "a name with nothing left is empty, for the player to type one");
	RelayAccess_NameFromProfile(NULL, 16, name);
	Check(name[0] == '\0', "and so is no name at all");

	// The page's words for each reason are access_page_test's to check (AccessPage_NameText)
}

// A made-up identity's public halves: 04, then byte i is (i * 37 + 11) & 255, the same as access-code.js
static void MadeUpStatics(uint8_t statics[kNetIdentityStaticBytes])
{
	statics[0] = 0x04;

	for (size_t i = 1; i < kNetIdentityStaticBytes; i++)
	{
		statics[i] = static_cast<uint8_t>((i * 37 + 11) & 255);
	}
}

static void TestAccessBody()
{
	puts("18. the body of an access request");

	uint8_t statics[kNetIdentityStaticBytes];
	MadeUpStatics(statics);
	char body[kRelayAccessMostBodyBytes + 1];
	Check(RelayList_AccessBody("Melanikus", statics, body, sizeof(body)), "a request with a good name and an identity's key is written");

	std::string const text = body;
	std::string const start = "{\"v\":1,\"name\":\"Melanikus\",\"key\":\"";
	Check(text.compare(0, start.size(), start) == 0 && text.size() == start.size() + kRelayAccessKeyLetters + 2 &&
		text.compare(text.size() - 2, 2, "\"}") == 0, "it's {\"v\":1,\"name\":\"...\",\"key\":\"...\"}, and the key is exactly 2,220 letters");

	std::string const key = text.size() > start.size() + 2 ? text.substr(start.size(), text.size() - start.size() - 2) : std::string();
	Check(key.compare(0, 12, "BDBVep/E6Q4z") == 0 && key.size() >= 12 && key.compare(key.size() - 12, 12, "Y4it0vccQWaL") == 0 &&
		key.find('=') == std::string::npos, "it starts and ends as standard base64 does it, with no = at the end");

	// Read back here, a letter at a time, to the bytes it was made from
	static const char kLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	bool readsBack = key.size() == kRelayAccessKeyLetters;

	for (size_t i = 0; readsBack && i + 4 <= key.size(); i += 4)
	{
		uint32_t three = 0;

		for (size_t j = 0; j < 4; j++)
		{
			const char *const at = strchr(kLetters, key[i + j]);
			readsBack = readsBack && at != NULL && *at != '\0';
			three = three * 64 + (at ? static_cast<uint32_t>(at - kLetters) : 0);
		}

		size_t const byte = i / 4 * 3;
		readsBack = readsBack && statics[byte] == ((three >> 16) & 255) && statics[byte + 1] == ((three >> 8) & 255) &&
			statics[byte + 2] == (three & 255);
	}

	Check(readsBack, "and it reads back to the identity's public halves, every byte");

	Check(!RelayList_AccessBody("Bishop\"", statics, body, sizeof(body)) && body[0] == '\0', "a name that breaks the rule isn't sent");
	Check(!RelayList_AccessBody("", statics, body, sizeof(body)) && body[0] == '\0', "nor an empty one");

	uint8_t none[kNetIdentityStaticBytes] = {};
	Check(!RelayList_AccessBody("Melanikus", none, body, sizeof(body)) && body[0] == '\0' &&
		!RelayList_AccessBody("Melanikus", NULL, body, sizeof(body)), "nor an identity that was never made");

	size_t const needed = text.size() + 1;
	Check(!RelayList_AccessBody("Melanikus", statics, body, needed - 1) && body[0] == '\0', "a body with a byte too little room isn't written");
	Check(RelayList_AccessBody("Melanikus", statics, body, needed), "and one with exactly its room is");
	Check(needed - 1 > kRelayListMostBodyBytes && needed - 1 <= kRelayAccessMostBodyBytes,
		"it's bigger than the 1 KB the list's other calls take, and within the 4 KB this one does");
}

// Reads an access answer, and gives back the refusal; answer is left alone unless it was read
static RelayListRefusal Access(int status, const std::string &body, const uint8_t *ours, RelayAccessAnswer *answer)
{
	return RelayList_ReadAccess(status, body.data(), body.size(), ours, answer);
}

static void TestAccessAnswers()
{
	puts("19. the answer to an access request");

	const char *const ourHex = "0123456789abcdef0123456789abcdef";
	uint8_t ours[kNetIdentityFingerprintBytes];
	NetIdentity_FingerprintFromHex(ourHex, ours);
	std::string const fp = std::string("\"fp\":\"") + ourHex + "\"";

	RelayAccessAnswer answer = kRelayAccessClosed;
	bool allFour = Access(200, "{\"v\":1," + fp + ",\"state\":\"waiting\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessWaiting;
	allFour = allFour && Access(200, "{\"v\":1," + fp + ",\"state\":\"approved\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessApproved;
	allFour = allFour && Access(200, "{\"v\":1," + fp + ",\"state\":\"refused\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessRefused;
	allFour = allFour && Access(200, "{\"v\":1," + fp + ",\"state\":\"revoked\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessRevoked;
	Check(allFour, "each of the four states is read, about this PC's own key");

	Check(Access(200, " {\n \"state\" : \"approved\" , " + fp + ", \"v\":1 }\n", ours, &answer) == kRelayListAccepted && answer == kRelayAccessApproved,
		"with the fields in any order, and spaces between them");

	bool allErrors = Access(400, "{\"v\":1,\"error\":\"name\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessBadName;
	allErrors = allErrors && Access(400, "{\"v\":1,\"error\":\"key\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessBadKey;
	allErrors = allErrors && Access(400, "{\"v\":1,\"error\":\"shape\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessBadShape;
	allErrors = allErrors && Access(429, "{\"v\":1,\"error\":\"busy\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessBusy;
	allErrors = allErrors && Access(503, "{\"v\":1,\"error\":\"closed\"}", ours, &answer) == kRelayListAccepted && answer == kRelayAccessClosed;
	Check(allErrors, "and each error, with its own status");

	// From here on every answer is refused, and answer has to be left as it was
	RelayAccessAnswer const untouched = static_cast<RelayAccessAnswer>(99);

	struct Bad
	{
		int status;
		std::string body;
		RelayListRefusal why;
		const char *what;
	};

	std::string const waiting = "{\"v\":1," + fp + ",\"state\":\"waiting\"}";
	Bad const bad[] = {
		{ 201, waiting, kRelayListBadStatus, "a status other than 200, 400, 429 and 503 is refused: 201" },
		{ 404, "{\"v\":1,\"error\":\"name\"}", kRelayListBadStatus, "404" },
		{ 500, "{\"v\":1,\"error\":\"closed\"}", kRelayListBadStatus, "and 500" },
		{ 200, "{\"v\":1," + fp + ",\"state\":\"busy\"}", kRelayListBadWord, "an error's word as a state is refused" },
		{ 429, "{\"v\":1,\"error\":\"name\"}", kRelayListBadWord, "and an error's word with another error's status" },
		{ 503, "{\"v\":1,\"error\":\"busy\"}", kRelayListBadWord, "busy is 429's, not 503's" },
		{ 200, "{\"v\":1," + fp + ",\"state\":\"Waiting\"}", kRelayListBadWord, "a word in other letters is another word" },
		{ 200, "{\"v\":1," + fp + ",\"state\":\"approvedx\"}", kRelayListBadWord, "a word longer than any of the protocol's is refused" },
		{ 200, "{\"v\":1," + fp + ",\"state\":\"\"}", kRelayListBadWord, "and an empty one" },
		{ 200, "{\"v\":1,\"fp\":\"0123456789abcdef0123456789abcdee\",\"state\":\"approved\"}", kRelayListNotOurKey,
			"an approval for another key isn't taken for this PC's" },
		{ 200, "{\"v\":1,\"fp\":\"0123456789ABCDEF0123456789ABCDEF\",\"state\":\"waiting\"}", kRelayListBadFingerprint,
			"a fingerprint in capital letters isn't one" },
		{ 200, "{\"v\":2," + fp + ",\"state\":\"waiting\"}", kRelayListBadNumber, "a version other than 1 is refused" },
		{ 200, "{" + fp + ",\"state\":\"waiting\"}", kRelayListFieldMissing, "and an answer without one" },
		{ 200, "{\"v\":1,\"state\":\"waiting\"}", kRelayListFieldMissing, "a state without its fingerprint is refused" },
		{ 200, "{\"v\":1," + fp + ",\"state\":\"waiting\",\"queue\":3}", kRelayListUnknownField, "and one with a field the protocol hasn't" },
		{ 200, "{\"v\":1," + fp + ",\"state\":\"waiting\",\"state\":\"approved\"}", kRelayListFieldTwice, "and one with a field twice" },
		{ 200, "{\"v\":1,\"error\":\"busy\"}", kRelayListUnknownField, "a 200 with an error is refused" },
		{ 400, "{\"v\":1," + fp + ",\"state\":\"approved\"}", kRelayListUnknownField, "and a 400 with a state" },
		{ 200, waiting + "{}", kRelayListMoreAfterTheEnd, "anything after the answer is refused" },
		{ 200, "[" + waiting + "]", kRelayListNotAnObject, "an answer that isn't an object is refused" },
		{ 200, "", kRelayListEndsTooSoon, "and an empty one" },
		{ 200, waiting.substr(0, waiting.size() - 1), kRelayListEndsTooSoon, "and one that stops before its end" },
		{ 200, std::string("{\"v\":1,\0", 8) + fp + ",\"state\":\"waiting\"}", kRelayListHasAZero, "and one with a 0 byte in it" },
		{ 200, waiting + std::string(kRelayAccessMostAnswerBytes + 1 - waiting.size(), ' '), kRelayListTooBig,
			"and one a byte bigger than any honest answer, even if it's only spaces" }
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		answer = untouched;
		RelayListRefusal const why = Access(bad[i].status, bad[i].body, ours, &answer);
		Check(why == bad[i].why && answer == untouched, bad[i].what);

		if (why != bad[i].why)
		{
			printf("          it said: %s\n", RelayList_RefusalWords(why));
		}
	}

	answer = untouched;
	std::string const spaced = waiting + std::string(kRelayAccessMostAnswerBytes - waiting.size(), ' ');
	Check(Access(200, spaced, ours, &answer) == kRelayListAccepted && answer == kRelayAccessWaiting, "an answer of exactly 512 bytes is read");

	answer = untouched;
	Check(Access(200, waiting, NULL, &answer) == kRelayListNotOurKey && answer == untouched, "and none is taken without this PC's own key to hold it to");
	Check(RelayList_ReadAccess(200, NULL, 10, ours, &answer) == kRelayListEndsTooSoon && answer == untouched &&
		RelayList_ReadAccess(200, waiting.data(), waiting.size(), ours, NULL) == kRelayListEndsTooSoon, "nor with no answer, or nowhere to put it");

	bool allHaveWords = true;
	bool allDiffer = true;

	for (int a = kRelayAccessWaiting; a <= kRelayAccessClosed; a++)
	{
		const char *const words = RelayAccess_AnswerWords(static_cast<RelayAccessAnswer>(a));
		allHaveWords = allHaveWords && words && words[0] && strstr(words, "no words") == NULL;

		for (int b = kRelayAccessWaiting; b < a; b++)
		{
			allDiffer = allDiffer && strcmp(words, RelayAccess_AnswerWords(static_cast<RelayAccessAnswer>(b))) != 0;
		}
	}

	Check(allHaveWords && allDiffer, "each answer has words of its own, for the log");
}

// Added 2026-11-09 by Melanikus for the AvP port: VERSION 1'S LIST, WITHOUT THE V2 DRAFT (networking.h, Net_V2DraftOn). Only a v2 draft's
// games and listings carry the host's fingerprint. A copy without the draft, which is every player's, reads and writes the list exactly as
// version 1 does.
static void TestVersionOne()
{
	puts("20. version 1's list, without the v2 draft");
	GameFields f;
	std::string noFingerprint = Game(f);
	std::string const fingerprintField = ",\"fingerprint\":" + f.fingerprint;
	noFingerprint.replace(noFingerprint.find(fingerprintField), fingerprintField.size(), "");
	std::string const v1List = ListOf(noFingerprint);
	int count = -1;
	memset(games, 0xA5, sizeof(games));
	bool allZero = true;
	RelayListRefusal const read = RelayList_ReadGames(v1List.data(), v1List.size(), kOurVersion, false, games, &count);

	for (size_t i = 0; i < sizeof(games[0].fingerprint); i++)
	{
		allZero = allZero && games[0].fingerprint[i] == 0;
	}

	Check(read == kRelayListAccepted && count == 1 && strcmp(games[0].code, "ABCDEFGH") == 0 && allZero,
		"without the draft, a game with no fingerprint is read, as version 1 reads it, and its fingerprint is left all 0");
	std::string const v2List = ListOf(Game(f));
	count = -1;
	Check(RelayList_ReadGames(v2List.data(), v2List.size(), kOurVersion, false, games, &count) == kRelayListUnknownField && count == 0,
		"and a game that carries one is refused for a field version 1 doesn't have, as version 1 refuses it");
	count = -1;
	Check(RelayList_ReadGames(v1List.data(), v1List.size(), kOurVersion, true, games, &count) == kRelayListFieldMissing && count == 0,
		"while a draft refuses a game with no fingerprint, as before");

	// A listing without the draft is the draft's with its fingerprint field taken out, and nothing else changed
	char withIt[kRelayListMostBodyBytes + 1];
	char without[kRelayListMostBodyBytes + 1];
	char withoutAny[kRelayListMostBodyBytes + 1];
	RelayListListing l = GoodListing();
	Check(RelayList_ListingBody(l, true, NULL, withIt, sizeof(withIt)) && RelayList_ListingBody(l, false, NULL, without, sizeof(without)),
		"a listing is made with the draft and without it");
	std::string expected = withIt;
	std::string const listed = ",\"fingerprint\":\"0123456789abcdeffedcba9876543210\"";
	size_t const at = expected.find(listed);

	if (at != std::string::npos)
	{
		expected.replace(at, listed.size(), "");
	}

	Check(at != std::string::npos && expected == without && strstr(without, "fingerprint") == NULL,
		"without the draft, the body has no fingerprint at all: it's the draft's with that one field taken out");
	l.fingerprint = NULL;
	Check(!RelayList_ListingBody(l, true, NULL, withoutAny, sizeof(withoutAny)) && RelayList_ListingBody(l, false, NULL, withoutAny, sizeof(withoutAny))
		&& strcmp(withoutAny, without) == 0, "a listing with no fingerprint isn't sent by a draft, and is sent, the same, without one");
}

// Added 2026-11-09 by Melanikus for the AvP port: A RELAY'S KEYS (protocol version 2's draft, CRYPTO.md 12.1), the answer to
// GET /v1/relay-keys, read as hostile. Only the keys' form is the reader's to check; whether they're the relay's is the fingerprint's.
static std::string HexOf(size_t bytes, uint8_t start)
{
	static const char kDigits[] = "0123456789abcdef";
	std::string text;

	for (size_t i = 0; i < bytes; i++)
	{
		uint8_t const b = static_cast<uint8_t>(start + i * 7);
		text += kDigits[b >> 4];
		text += kDigits[b & 15];
	}

	return text;
}

static std::string InCapitals(std::string text)
{
	for (char &c : text)
	{
		c = (c >= 'a' && c <= 'f') ? static_cast<char>(c - 'a' + 'A') : c;
	}

	return text;
}

static std::string KeysAnswer(const std::string &region, const std::string &p384, const std::string &mlkem, const std::string &extra = "")
{
	return "{\"region\":\"" + region + "\",\"p384\":\"" + p384 + "\",\"mlkem\":\"" + mlkem + "\"" + extra + "}";
}

static bool KeysAreZero(const uint8_t *p384, const uint8_t *kem)
{
	bool zero = true;

	for (size_t i = 0; i < kNetCryptoP384PublicBytes; i++)
	{
		zero = zero && p384[i] == 0;
	}

	for (size_t i = 0; i < kNetPqKemPublicBytes; i++)
	{
		zero = zero && kem[i] == 0;
	}

	return zero;
}

// Modified 2026-11-10 by Melanikus for the AvP port (#175): the region comes back from the answer (GET /v1/relay-keys, asked with no region)
static char keysRegion[kRelayListRegionLetters + 1];

static RelayListRefusal ReadKeys(const std::string &answer, uint8_t *p384, uint8_t *kem)
{
	memset(keysRegion, 'x', sizeof(keysRegion));
	memset(p384, 0xA5, kNetCryptoP384PublicBytes);
	memset(kem, 0xA5, kNetPqKemPublicBytes);
	return RelayList_ReadRelayKeys(answer.data(), answer.size(), keysRegion, p384, kem);
}

// Nothing of a refused answer is kept: no region, and keys all 0
static bool NothingKept(const uint8_t *p384, const uint8_t *kem)
{
	return keysRegion[0] == '\0' && KeysAreZero(p384, kem);
}

static void TestRelayKeys()
{
	puts("21. a relay's keys (protocol version 2's draft)");
	uint8_t p384[kNetCryptoP384PublicBytes];
	uint8_t kem[kNetPqKemPublicBytes];
	std::string const goodP384 = HexOf(kNetCryptoP384PublicBytes, 4);
	std::string const goodKem = HexOf(kNetPqKemPublicBytes, 9);
	std::string const good = KeysAnswer("newyork", goodP384, goodKem);
	Check(ReadKeys(good, p384, kem) == kRelayListAccepted && strcmp(keysRegion, "newyork") == 0,
		"a relay's keys are read: its region, given back, its P-384 key's 97 bytes and its ML-KEM key's 1,568, as hex");
	bool same = true;

	for (size_t i = 0; i < kNetCryptoP384PublicBytes; i++)
	{
		same = same && p384[i] == static_cast<uint8_t>(4 + i * 7);
	}

	for (size_t i = 0; i < kNetPqKemPublicBytes; i++)
	{
		same = same && kem[i] == static_cast<uint8_t>(9 + i * 7);
	}

	Check(same, "and each byte is the one its two digits say");
	std::string const shuffled = "{ \"mlkem\" : \"" + goodKem + "\" , \"region\" : \"newyork\" , \"p384\" : \"" + goodP384 + "\" }\n";
	Check(ReadKeys(shuffled, p384, kem) == kRelayListAccepted && strcmp(keysRegion, "newyork") == 0, "in any order, with room between");
	Check(ReadKeys(KeysAnswer("sixteen-letters1", goodP384, goodKem), p384, kem) == kRelayListAccepted && strcmp(keysRegion, "sixteen-letters1") == 0,
		"a region of 16, the most there is, comes back whole");

	struct Bad
	{
		std::string answer;
		RelayListRefusal why;
		const char *what;
	};

	Bad const bad[] =
	{
		{ KeysAnswer("New York", goodP384, goodKem), kRelayListBadRegion, "a region that can't be one (capitals and a space) is refused" },
		{ KeysAnswer("seventeen-letter1", goodP384, goodKem), kRelayListBadRegion, "and so is one of 17 letters" },
		{ KeysAnswer("", goodP384, goodKem), kRelayListBadRegion, "and an empty one" },
		{ KeysAnswer("newyork", goodP384.substr(2), goodKem), kRelayListBadText, "a P-384 key a byte short is refused" },
		{ KeysAnswer("newyork", goodP384.substr(1), goodKem), kRelayListBadText, "and one a digit short" },
		{ KeysAnswer("newyork", goodP384 + "0", goodKem), kRelayListBadText, "and one a digit long" },
		{ KeysAnswer("newyork", goodP384, goodKem.substr(2)), kRelayListBadText, "an ML-KEM key a byte short is refused" },
		{ KeysAnswer("newyork", goodP384, goodKem + "00"), kRelayListBadText, "and one a byte long" },
		{ KeysAnswer("newyork", InCapitals(goodP384), goodKem), kRelayListBadText, "a key in capitals is refused: the server writes small letters" },
		{ KeysAnswer("newyork", "g" + goodP384.substr(1), goodKem), kRelayListBadText, "a key with a letter that isn't a hex digit is refused" },
		{ KeysAnswer("newyork", goodP384, goodKem, ",\"extra\":1"), kRelayListUnknownField, "a field the answer doesn't have is refused" },
		{ "{\"region\":\"newyork\",\"p384\":\"" + goodP384 + "\"}", kRelayListFieldMissing, "an answer without its ML-KEM key is refused" },
		{ good + "{}", kRelayListMoreAfterTheEnd, "anything after the answer is refused" },
		{ "[" + good + "]", kRelayListNotAnEntry, "an answer that isn't one entry is refused" },
	};

	for (Bad const &b : bad)
	{
		Check(ReadKeys(b.answer, p384, kem) == b.why && NothingKept(p384, kem), b.what);
	}

	std::string const big = good + std::string(kRelayKeysMostAnswerBytes, ' ');
	Check(ReadKeys(big, p384, kem) == kRelayListTooBig && NothingKept(p384, kem), "an answer bigger than 8 KB isn't read at all");
	std::string zeroed = good;
	zeroed[5] = '\0';
	Check(ReadKeys(zeroed, p384, kem) == kRelayListHasAZero && NothingKept(p384, kem), "nor one with a 0 byte in it");
	memset(keysRegion, 'x', sizeof(keysRegion));
	Check(RelayList_ReadRelayKeys(NULL, 0, keysRegion, p384, kem) == kRelayListEndsTooSoon && NothingKept(p384, kem), "no answer at all is refused");
	Check(RelayList_ReadRelayKeys(good.data(), good.size(), NULL, p384, kem) == kRelayListBadRegion && KeysAreZero(p384, kem),
		"and with nowhere to give the region back, nothing is read");

	// How relay_list.cpp asks for them, and gives them to the game (read, not run: it needs the network)
	std::string const source = ReadSource("3dc/relay_list.cpp");
	// Modified 2026-11-10 by Melanikus for the AvP port (#175): they're the keys the server's check took (CheckTheServer, whose own checks
	// are in TestTheCalls), proved by the server's servers.cfg line, and handed over only for its relay's region
	std::string const fetch = SourceSlice(source, "void ReadTheRelayKeys(const char *region, int asking)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const checked = fetch.find("const char *why = CheckTheServer();");
	size_t const sameRegion = fetch.find("if (!why && strncmp(region, shared->relayRegion, sizeof(shared->relayRegion)) != 0)");
	size_t const given = fetch.find("memcpy(p384, shared->relayP384, sizeof(p384));");
	Check(checked != std::string::npos && sameRegion != std::string::npos && given != std::string::npos && checked < sameRegion && sameRegion < given,
		"SOURCE: the keys handed over are the ones the server's check proved, and only for the region of the server's own relay");
	std::string const came = SourceSlice(source, "int RelayList_RelayKeysCame(", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(came, "if (shared->keysAsked > 0 && shared->keysAnswered == shared->keysAsked)") && Has(came, "if (came == kRelayKeysCame)\n\t\t{"),
		"SOURCE: only the answer to the newest asking is taken, and only keys that came whole are handed over");
	Check(CountOf(source, "RelayKey\"") == 0 && CountOf(source, "relay-keys?region") == 0,
		"SOURCE: no RelayKey setting is read, and no region is asked about: the line's own server answers for its own relay");
}

// Added 2026-11-09 by Melanikus for the AvP port, for -dev's test server reached by its address. Modified 2026-11-10 by Melanikus for the
// AvP port (#175): the server's pin and shown name are gone with the -dev settings, and what's left is how an address written plainly is
// read, a servers.cfg line's and a Hudson run's relay beyond this machine
static void TestTheTestServer()
{
	puts("22. an address written plainly (a servers.cfg line's, and a Hudson run's relay beyond this machine)");

	// The address, written plainly. Each refusal says why, and writes nothing.
	uint32_t address = 0x12345678u;
	Check(RelayList_IPv4Refusal("203.0.113.9", &address) == kRelayIPv4Read && address == 0xCB007109u, "an address written plainly is read, first number highest");
	Check(RelayList_ReadIPv4("0.0.0.0", &address) && address == 0 && RelayList_ReadIPv4("255.255.255.255", &address) && address == 0xFFFFFFFFu,
		"the lowest and the highest are read (whether a server can have one is RelayList_AddressRefusal's)");
	address = 7;
	Check(RelayList_IPv4Refusal("203.0.113.09", &address) == kRelayIPv4LeadingZero && RelayList_IPv4Refusal("010.1.1.1", &address) == kRelayIPv4LeadingZero,
		"a leading zero is refused for being one, as some readers take it as octal");
	Check(RelayList_IPv4Refusal("256.1.1.1", &address) == kRelayIPv4TooBig && RelayList_IPv4Refusal("1.1.1.1000", &address) == kRelayIPv4TooBig &&
		RelayList_IPv4Refusal("1.1.1.99999999999", &address) == kRelayIPv4TooBig, "a number past 255, or of four digits or more, is too big");
	Check(RelayList_IPv4Refusal("1.1.1", &address) == kRelayIPv4WrongShape && RelayList_IPv4Refusal("1.1.1.1.1", &address) == kRelayIPv4WrongShape &&
		RelayList_IPv4Refusal("1.1.1.1.", &address) == kRelayIPv4WrongShape, "three numbers, five, or a dot after the last, is the wrong shape");
	Check(RelayList_IPv4Refusal(" 1.1.1.1", &address) == kRelayIPv4NotANumber && RelayList_IPv4Refusal("1.1.1.1 ", &address) == kRelayIPv4NotANumber &&
		RelayList_IPv4Refusal("1.1.1.1\n", &address) == kRelayIPv4NotANumber && RelayList_IPv4Refusal("1..1.1", &address) == kRelayIPv4NotANumber &&
		RelayList_IPv4Refusal(".1.1.1.1", &address) == kRelayIPv4NotANumber && RelayList_IPv4Refusal("+1.1.1.1", &address) == kRelayIPv4NotANumber &&
		RelayList_IPv4Refusal("0x1.1.1.1", &address) == kRelayIPv4NotANumber, "a space, a newline, an empty number, a sign or hex is not a number");
	Check(RelayList_IPv4Refusal("", &address) == kRelayIPv4NoText && RelayList_IPv4Refusal(NULL, &address) == kRelayIPv4NoText &&
		RelayList_IPv4Refusal("1.1.1.1", NULL) == kRelayIPv4NoText, "nothing at all, or nowhere to put it, is no text");
	Check(address == 7, "and no refusal wrote anything");
}

int main()
{
	TestGoodLists();
	TestTheWholeList();
	TestOneGame();
	TestTexts();
	TestCodesAndRegions();
	TestNumbers();
	TestOwner();
	TestWords();
	TestBodies();
	TestModeWords();
	TestTheCalls();
	TestTheMenus();
	TestAddresses();
	TestAccessCodes();
	TestAccessNames();
	TestAccessBody();
	TestAccessAnswers();
	TestVersionOne();
	TestRelayKeys();
	TestTheTestServer();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
