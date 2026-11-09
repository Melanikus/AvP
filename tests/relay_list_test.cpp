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
// Nothing here uses the network. Run it from the top of the repository, where the source it reads is found from.

#include "relay_list_text.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

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
static RelayListRelay relays[kRelayListMostRelays];

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
	std::string hostMs = "42";
	std::string seconds = "75";
	std::string extra;   // written after the others, comma and all
};

static std::string Game(const GameFields &f)
{
	return "{\"region\":" + f.region + ",\"code\":" + f.code + ",\"name\":" + f.name + ",\"level\":" + f.level + ",\"mode\":" + f.mode +
		",\"gameStyle\":" + f.gameStyle + ",\"levelNumber\":" + f.levelNumber + ",\"coopLobby\":" + f.coopLobby + ",\"players\":" + f.players +
		",\"maxPlayers\":" + f.maxPlayers + ",\"version\":" + f.version + ",\"hostMs\":" + f.hostMs + ",\"seconds\":" + f.seconds + f.extra + "}";
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
	return RelayList_ReadGames(text.data(), text.size(), version, games, count);
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

	std::string const shuffled = "{\"seconds\":0,\"hostMs\":0,\"version\":113,\"maxPlayers\":8,\"players\":8,\"coopLobby\":255,\"levelNumber\":100,"
		"\"gameStyle\":255,\"mode\":\"m\",\"level\":\"l\",\"name\":\"n\",\"code\":\"ZZZZZZZZ\",\"region\":\"a\"}";
	Check(Read(ListOf(shuffled), &count) == kRelayListAccepted && count == 1 && games[0].players == 8 && games[0].seconds == 0 &&
		games[0].levelNumber == 100 && games[0].coopLobby == 255 && games[0].hostMs == 0, "the fields in any order, each at an end of its range");

	std::string const spaced = " \r\n{ \"games\" :\t[ { \"region\" : \"virginia\" , \"code\" : \"ABCDEFGH\" , \"name\" : \"a\" , \"level\" : \"b\" ,"
		" \"mode\" : \"c\" , \"gameStyle\" : 4 , \"levelNumber\" : 0 , \"coopLobby\" : 1 , \"players\" : 1 , \"maxPlayers\" : 1 ,"
		" \"version\" : 113 , \"hostMs\" : 9999 , \"seconds\" : 31536000 } ]\n}\n";
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
	// characters JSON writes as two, and every number at its longest. The protocol says that comes to 41,111 bytes.
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
	Check(text.size() == 41111, "the largest list the protocol allows is the 41,111 bytes it says");
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

// A relay's text, and a list of them
static std::string Relay(const char *region, const char *host, const char *port)
{
	return std::string("{\"region\":") + region + ",\"host\":" + host + ",\"port\":" + port + "}";
}

static RelayListRefusal ReadRelays(const std::string &text, int *count)
{
	return RelayList_ReadRelays(text.data(), text.size(), relays, count);
}

static void RelaysRefused(const std::string &text, RelayListRefusal reason, const char *what)
{
	int count = -1;
	RelayListRefusal const why = ReadRelays(text, &count);
	Check(why == reason && count == 0, what);

	if (why != reason)
	{
		printf("          it said: %s\n", RelayList_RefusalWords(why));
	}
}

static void TestRelays()
{
	puts("7. the relays");

	int count = -1;
	std::string const three = "{\"relays\":[" + Relay("\"saopaulo\"", "\"sp.relay.example\"", "27500") + "," +
		Relay("\"virginia\"", "\"va.relay.example\"", "27500") + "," + Relay("\"paris\"", "\"fr.relay.example\"", "1") + "]}";
	Check(ReadRelays(three, &count) == kRelayListAccepted && count == 3, "a list of three relays is read");
	Check(strcmp(relays[0].region, "saopaulo") == 0 && strcmp(relays[0].host, "sp.relay.example") == 0 && relays[0].port == 27500 &&
		relays[2].port == 1, "with each one's region, host and port");
	Check(ReadRelays("{\"relays\":[]}", &count) == kRelayListAccepted && count == 0, "and a list of none");

	// The largest the protocol allows: 16 relays, every region 16 characters and every host 63, port 65535. 1,868 bytes, it says.
	std::string text;

	for (int i = 0; i < kRelayListMostRelays; i++)
	{
		std::string const region = std::string("\"region-") + "abcdefghijklmnop"[i] + "xxxxxxxx\"";
		std::string const host = "\"" + std::string(61, 'h') + "." + "abcdefghijklmnop"[i] + "\"";
		text += (i ? "," : "") + Relay(region.c_str(), host.c_str(), "65535");
	}

	text = "{\"relays\":[" + text + "]}";
	Check(text.size() == 1868, "the largest list of relays the protocol allows is the 1,868 bytes it says");
	Check(ReadRelays(text, &count) == kRelayListAccepted && count == kRelayListMostRelays, "and all 16 are read");

	std::string const one = Relay("\"saopaulo\"", "\"sp.relay.example\"", "27500");
	RelaysRefused(std::string(kRelayListMostRelaysBytes + 1, ' '), kRelayListTooBig, "a list of relays over 4 KB is refused");
	RelaysRefused("{\"games\":[]}", kRelayListNotItsField, "a list of games where relays should be is refused");
	RelaysRefused("{\"relays\":[" + one + "," + one + "]}", kRelayListTwice, "two relays for one region are refused");
	RelaysRefused("{\"relays\":[" + Relay("\"saopaulo\"", "\"203.0.113.9\"", "27500") + "]}", kRelayListBadHost,
		"a relay given as an address, not a name, is refused");
	RelaysRefused("{\"relays\":[" + Relay("\"saopaulo\"", ("\"" + std::string(64, 'h') + "\"").c_str(), "27500") + "]}", kRelayListBadHost,
		"and a host of 64 characters");
	RelaysRefused("{\"relays\":[" + Relay("\"saopaulo\"", "\"sp_relay.example\"", "27500") + "]}", kRelayListBadHost, "and a host with an _");
	RelaysRefused("{\"relays\":[" + Relay("\"saopaulo\"", "\"sp.relay.example\"", "0") + "]}", kRelayListBadNumber, "port 0 is refused");
	RelaysRefused("{\"relays\":[" + Relay("\"saopaulo\"", "\"sp.relay.example\"", "65536") + "]}", kRelayListBadNumber, "and port 65536");
	RelaysRefused("{\"relays\":[" + Relay("\"Sao\"", "\"sp.relay.example\"", "27500") + "]}", kRelayListBadRegion, "a region with capitals is refused");
	RelaysRefused("{\"relays\":[{\"region\":\"saopaulo\",\"host\":\"sp.relay.example\"}]}", kRelayListFieldMissing, "a relay without its port is refused");

	std::string seventeen;

	for (int i = 0; i <= kRelayListMostRelays; i++)
	{
		std::string const region = std::string("\"r") + "abcdefghijklmnopq"[i] + "\"";
		seventeen += (i ? "," : "") + Relay(region.c_str(), "\"relay.example\"", "27500");
	}

	RelaysRefused("{\"relays\":[" + seventeen + "]}", kRelayListTooMany, "17 relays are refused");

	Check(RelayList_IsRelayHost("relay.example") && RelayList_IsRelayHost("1relay.example"), "a name, even one starting with a digit, is a host");
	Check(!RelayList_IsRelayHost("10.0.0.1") && !RelayList_IsRelayHost("8"), "and an address, or a bare number, isn't");
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

	for (int a = kRelayListAccepted; a <= kRelayListMoreAfterTheEnd; a++)
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
	return l;
}

static void TestBodies()
{
	puts("10. the bodies the game sends");

	char body[kRelayListMostBodyBytes + 1];
	RelayListListing l = GoodListing();

	Check(RelayList_ListingBody(l, NULL, body, sizeof(body)), "a listing is made");
	Check(strcmp(body, "{\"region\":\"saopaulo\",\"code\":\"ABCDEFGH\",\"pass\":\"00112233445566778899aabbccddeeff\",\"passTime\":1762646400123,"
		"\"name\":\"Hicks's game\",\"level\":\"Derelict\",\"mode\":\"Deathmatch\",\"gameStyle\":0,\"levelNumber\":3,\"coopLobby\":0,\"players\":2,"
		"\"maxPlayers\":4,\"version\":113,\"hostMs\":42}") == 0, "exactly as the list takes a POST, the pass in small-letter hex");

	uint8_t owner[kRelayListSecretBytes];
	memset(owner, 0xAB, sizeof(owner));
	std::string const putStart = "{\"owner\":\"abababababababababababababababab\",\"region\":";
	Check(RelayList_ListingBody(l, owner, body, sizeof(body)) && std::string(body).compare(0, putStart.size(), putStart) == 0,
		"a PUT is the same, with the owner first");

	l.passTime = kRelayListMostPassTime;
	Check(RelayList_ListingBody(l, NULL, body, sizeof(body)) && strstr(body, "\"passTime\":9007199254740991,") != NULL, "the largest passTime is written whole");

	l.passTime = kRelayListMostPassTime + 1;
	Check(!RelayList_ListingBody(l, NULL, body, sizeof(body)) && body[0] == '\0', "and one past it isn't sent");

	l = GoodListing();
	l.name = "say \"hi\" \\o/";
	Check(RelayList_ListingBody(l, NULL, body, sizeof(body)) && strstr(body, "\"name\":\"say \\\"hi\\\" \\\\o/\"") != NULL, "a \" and a \\ in a name are escaped");

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
	Check(RelayList_ListingBody(l, owner, body, sizeof(body)) && strlen(body) < kRelayListMostBodyBytes, "the largest PUT fits in the 1 KB the list takes");

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
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		RelayListListing x = GoodListing();
		bad[i].make(x);
		Check(!RelayList_ListingBody(x, NULL, body, sizeof(body)) && body[0] == '\0', bad[i].what);
	}

	l = GoodListing();
	Check(!RelayList_ListingBody(l, NULL, body, 40) && body[0] == '\0', "a body with too little room isn't sent, and is left empty");

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

static void TestServerNames()
{
	puts("11. the list's name, from the settings");

	const char *const good[] = { "list.example.org", "a", "localhost", "a-b.c1.d", "127.0.0.1" };

	for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); i++)
	{
		Check(Relay_IsServerName(good[i]), (std::string("a name like ") + good[i] + " is taken").c_str());
	}

	std::string const part63(63, 'p');
	Check(Relay_IsServerName((part63 + ".org").c_str()), "a part of 63 characters is taken");
	Check(!Relay_IsServerName((part63 + "p.org").c_str()), "and one of 64 isn't");

	std::string longest;

	while (longest.size() < 253)
	{
		longest += longest.empty() ? "a" : ".a";
	}

	Check(Relay_IsServerName(longest.c_str()), "a name of 253 characters is taken");
	Check(!Relay_IsServerName((longest + "a").c_str()), "and one of 254 isn't");

	const char *const bad[][2] = {
		{ "", "an empty name isn't a name" }, { ".", "nor a dot" }, { "a..b", "nor two dots together" }, { ".a", "nor a dot first" },
		{ "a.b.", "nor a dot last" }, { "-a.b", "nor a part starting with -" }, { "a-.b", "nor a part ending with -" },
		{ "a_b", "nor an _" }, { "a b", "nor a space" }, { "a:443", "nor a port" }, { "a/b", "nor a /" },
		{ "https://a", "nor a web address" }, { "a\\b", "nor a \\" }, { "caf\xc3\xa9.org", "nor a letter outside ASCII" },
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		Check(!Relay_IsServerName(bad[i][0]), bad[i][1]);
	}

	Check(!Relay_IsServerName(NULL), "and no name at all isn't one");
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

	std::string const call = SourceSlice(source, "Answer Call(const wchar_t *method", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(call, "WINHTTP_FLAG_SECURE)"), "every call is HTTPS");
	Check(Has(call, "WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;") && !Has(source, "TLS1_1") && !Has(source, "PROTOCOL_TLS1 ") &&
		!Has(source, "PROTOCOL_SSL") && !Has(source, "PROTOCOL_ALL"), "with TLS 1.2 or newer, and nothing older named anywhere");
	// Modified 2026-11-09 by Melanikus for the AvP port: the certificate check is turned off nowhere but on -dev's pinned path, and there only
	// for an unknown issuer and another name, and only once the pin's own check is in place for the call (relay_list.h, RelayListPin)
	std::string const passLine = "\t\tif (shared->pinned)\n\t\t{\n\t\t\tDWORD pass = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;\n";
	size_t const pinnedBlock = call.find(passLine);
	size_t const callback = call.find("WinHttpSetStatusCallback(request, CheckThePin, WINHTTP_CALLBACK_FLAG_SEND_REQUEST, 0)");
	size_t const passed = call.find("WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &pass, sizeof(pass))");
	size_t const firstSend = call.find("WinHttpSendRequest(");
	Check(CountOf(source, "SECURITY_FLAG_IGNORE") == 2 && CountOf(source, "WINHTTP_OPTION_SECURITY_FLAGS") == 1 && pinnedBlock != std::string::npos,
		"nothing turns the certificate check off but -dev's pinned path, and there only for an unknown issuer and another name");
	Check(callback != std::string::npos && passed != std::string::npos && firstSend != std::string::npos && pinnedBlock < callback && callback < passed &&
		passed < firstSend, "and there the pin's check is in place before that, and both before the call's first byte");
	std::string const pinCheck = SourceSlice(source, "void CALLBACK CheckThePin(", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(pinCheck, "status != WINHTTP_CALLBACK_STATUS_SENDING_REQUEST") && Has(pinCheck, "WINHTTP_OPTION_SERVER_CERT_CONTEXT") &&
		Has(pinCheck, "Sha256(certificate->pbCertEncoded, certificate->cbCertEncoded, digest)") && Has(pinCheck, "RelayList_PinMatches(digest, check->pin)") &&
		Has(pinCheck, "CertFreeCertificateContext(certificate);"),
		"the pin's check reads the list's certificate just before the first byte, compares its SHA-256 with the pin, and gives it back");
	Check(Has(pinCheck, "if (!check->matched)\n\t\t{\n\t\t\tcheck->closed = true;\n\t\t\tWinHttpCloseHandle(request);"),
		"and a certificate that isn't the pinned one, or none it could read, closes the call there, unsent");
	size_t const closedSaid = call.find("if (check.closed)\n\t\t{\n\t\t\ta.why = kNotPinned;");
	size_t const unreadSaid = call.find("if (shared->pinned && !check.matched)\n\t\t{\n\t\t\ta.why = kPinUnread;");
	size_t const firstRead = call.find("WinHttpReceiveResponse(");
	Check(closedSaid != std::string::npos && unreadSaid != std::string::npos && firstRead != std::string::npos && firstSend < closedSaid &&
		closedSaid < unreadSaid && unreadSaid < firstRead, "a call the check closed says so, and a pinned call whose check never ran has nothing of its answer read");
	// Added 2026-11-09 by Melanikus for the AvP port (Primary's read of the pin): the request goes out with its headers alone, and its body
	// (a listing's pass and owner token) is written only once the pin's check has passed, and all of it or the call fails
	size_t const bodyWritten = call.find("bool const bodyWent = sent && (!body || (WinHttpWriteData(request, body, bodyBytes, &written) && written == bodyBytes));");
	Check(CountOf(call, "WinHttpSendRequest(") == 2 && CountOf(call, "WINHTTP_NO_REQUEST_DATA, 0, ") == 2 && !Has(call, "const_cast<char *>(body)"),
		"the request goes out with its headers alone: neither way of sending it carries the body");
	Check(bodyWritten != std::string::npos && unreadSaid < bodyWritten && bodyWritten < firstRead && CountOf(call, "WinHttpWriteData(") == 1,
		"the body is written once, whole, only after the pin's check has passed, and before the answer is waited for");
	// and a request the pin's check closed is never used again, nor closed a second time: after the send, every close is of NULL or of what
	// StillOpen gives, and a call the check closed at any send ends before anything more is done with its request
	std::string const stillOpen = SourceSlice(source, "HINTERNET StillOpen(const PinCheck &check, HINTERNET request)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(stillOpen, "return check.closed ? NULL : request;"), "StillOpen gives nothing to close for a request the pin's check closed");
	std::string const afterTheSend = firstSend != std::string::npos ? call.substr(firstSend) : std::string();
	size_t const closes = CountOf(afterTheSend, "CloseAll(");
	size_t const safeCloses = CountOf(afterTheSend, "CloseAll(NULL, connection, session);") +
		CountOf(afterTheSend, "CloseAll(StillOpen(check, request), connection, session);");
	Check(closes >= 9 && closes == safeCloses, "after the send, every one of the call's 9 or more closes leaves out a request the check closed");
	size_t const closedAgain = firstRead != std::string::npos ? call.find("if (check.closed)\n\t\t{\n\t\t\ta.why = kNotPinned;", firstRead) : std::string::npos;
	size_t const notAnswered = call.find("if (!answered)");
	Check(Has(call, "bodyWent && !check.closed && WinHttpReceiveResponse(request, NULL)") && closedAgain != std::string::npos &&
		notAnswered != std::string::npos && closedAgain < notAnswered, "a call the check closed while the answer was awaited ends there, unread");
	Check(Has(call, "WINHTTP_DISABLE_REDIRECTS") && Has(call, "WINHTTP_DISABLE_COOKIES") && Has(call, "WINHTTP_DISABLE_AUTHENTICATION"),
		"redirects, cookies and signing in are off");
	Check(Has(call, "WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000)"), "each step has 5 seconds");
	Check(Has(call, "if (got >= room)") && Has(call, "a.why = kTooBig;"), "an answer bigger than its room is refused, not read on");
	Check(Has(source, "char gamesAnswer[kRelayListMostGamesBytes];") && Has(source, "RelayList_ReadGames(gamesAnswer, a.bytes, kMultiplayerVersion"),
		"the list of games is read into 48 KB, and through the hostile reader, for this version");
	Check(Has(source, "char relaysAnswer[kRelayListMostRelaysBytes];") && Has(source, "RelayList_ReadRelays(relaysAnswer, a.bytes"),
		"and the relays into 4 KB, the same way");

	std::string const thread = SourceSlice(source, "// ---- The list's thread", "// Makes the store and starts the thread", kSliceEndAfterTheStart, FailedSlice);
	Check(thread.size() > 1000 && !Has(thread, "LogString(") && !Has(thread, "LogErrorString("), "the list's own thread never writes to the game's log");
	Check(!Has(source, " printf(") && !Has(source, "\tprintf(") && !Has(source, "OutputDebugString"), "and nothing prints what a call sent or got");

	// The thread's news: every piece since the main thread last looked, kept in a ring, each written with "Relay list: " in front, so one
	// search of the log finds them all
	std::string const news = SourceSlice(source, "void WriteTheNews(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(news, "for (; newsWritten < number; newsWritten++)") && Has(news, "LogString(std::string(\"Relay list: \") + std::string(piece, strnlen(piece, kMostNewsLetters)));"),
		"the main thread writes every piece of the list's news, each starting \"Relay list: \"");

	// The settings file is shared, so it mustn't point anyone's game at another list (settings_file_test): both settings are read only with -dev
	std::string const serverName = SourceSlice(source, "const char *Relay_ServerName(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(serverName, "if (!DevMode)") && Has(serverName, "memcpy(name, kRelayBuiltInServer, sizeof(kRelayBuiltInServer));"),
		"without -dev, the list's name is the game's own, whatever the settings say");

	// Modified 2026-11-09 by Melanikus for the AvP port: -dev's test server reached with nothing changed on the tester's computer (relay_list.h)
	Check(Has(serverName, "(!OnlyDigitsAndDots(setting.c_str()) || IsUsableAddress(setting.c_str()))"),
		"with -dev, digits and dots in RelayServer must be one plain address a server can have");
	std::string const pinSetting = SourceSlice(source, "const PinSetting &ThePinSetting(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const pinDev = pinSetting.find("else if (!DevMode)");
	size_t const pinRead = pinSetting.find("else if (RelayList_ReadPin(setting.c_str(), pinSetting.pin))");
	Check(pinDev != std::string::npos && pinRead != std::string::npos && pinDev < pinRead && Has(pinSetting, "pinSetting.refused = true;"),
		"RelayListPin is read only with -dev, and one that's set but isn't a pin is refused");
	std::string const starting = SourceSlice(source, "bool Start(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(starting, "if (!name[0] || PinRefused())"), "and then there's no list at all, so a test meant to be pinned is never called unpinned");
	std::string const relaysRead = SourceSlice(source, "void ReadTheRelays(void)", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(relaysRead, "if (shared->atAnAddress && shared->pinned)\n\t\t\t{\n\t\t\t\taddress = shared->serverAddress;") &&
		Has(relaysRead, "RelayList_AddressRefusal(ntohl(address), shared->developer)"),
		"a test server at an address, with a pin: its relays are taken to be at that address, no name looked up, and the address still checked");
	std::string const shownName = SourceSlice(source, "const char *Relay_ServerShownName(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const shownDev = shownName.find("else if (!DevMode)");
	size_t const shownRead = shownName.find("RelayList_IsShownName(setting.c_str())");
	Check(shownDev != std::string::npos && shownRead != std::string::npos && shownDev < shownRead,
		"the name people see for the server is read only with -dev, and only if it's one");
	std::string const listPort = SourceSlice(source, "int Relay_ListPort(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(listPort, "if (DevMode)\n\t\t{\n\t\t\tint const setting = Config_GetInt(\"[Network]\", \"RelayListPort\""),
		"and the list's port is the settings' only with -dev");

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
		"no connection to the list's server is: it couldn't be reached, took too long, its certificate wasn't accepted, or the call couldn't start");
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

	// The Join page's first row says what its search does now: this network and the online list, not 1999's "Find Local Servers". The
	// words are the game's own (language.cpp, after everything language.txt holds), so a player's language file isn't touched.
	std::string const pages = ReadSource("3dc/avp/win95/frontend/menu_data.cpp");
	std::string const joinPage = SourceSlice(pages, "static AVPMENU_ELEMENT AvPMenu_MultiplayerJoinGame[]=", "\n};\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(joinPage, "{TEXTSTRING_MULTIPLAYER_FINDGAMES},") && Has(joinPage, "TEXTSTRING_MULTIPLAYER_FINDGAMES_HELP}") && !Has(joinPage, "FINDLOCAL"),
		"the Join page's first row is Find Games, with its own help, and nothing of the old local-servers words");
	std::string const words = ReadSource("3dc/avp/language.cpp");
	Check(Has(words, "\t\"Find Games\",\n") && Has(words, "on this network and on the online list of games"), "and its words say both: this network, and the online list");

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
	puts("15. the address a relay's name looks up to");

	struct Case
	{
		uint32_t address;
		RelayAddressRefusal played;      // without -dev
		RelayAddressRefusal developer;   // with -dev
		const char *what;
	};

	Case const cases[] = {
		{ Address(8, 8, 8, 8), kRelayAddressUsable, kRelayAddressUsable, "an address out on the internet is used" },
		{ Address(223, 255, 255, 255), kRelayAddressUsable, kRelayAddressUsable, "and so is the last one before the multicast block" },
		{ Address(0, 0, 0, 0), kRelayAddressThisNetwork, kRelayAddressThisNetwork, "ANY, 0.0.0.0, is refused, -dev or not" },
		{ Address(0, 1, 2, 3), kRelayAddressThisNetwork, kRelayAddressThisNetwork, "and all of 0.0.0.0/8" },
		{ Address(1, 0, 0, 0), kRelayAddressUsable, kRelayAddressUsable, "but not 1.0.0.0, just past it" },
		{ Address(255, 255, 255, 255), kRelayAddressBroadcast, kRelayAddressBroadcast, "the broadcast address is refused, -dev or not" },
		{ Address(224, 0, 0, 1), kRelayAddressMulticast, kRelayAddressMulticast, "a multicast address is refused, -dev or not" },
		{ Address(239, 255, 255, 255), kRelayAddressMulticast, kRelayAddressMulticast, "up to the last of them" },
		{ Address(240, 0, 0, 1), kRelayAddressReserved, kRelayAddressReserved, "an address set aside is refused, -dev or not" },
		{ Address(255, 255, 255, 254), kRelayAddressReserved, kRelayAddressReserved, "up to the one before broadcast" },
		{ Address(127, 0, 0, 1), kRelayAddressLoopback, kRelayAddressUsable, "this machine's own address only with -dev" },
		{ Address(10, 1, 2, 3), kRelayAddressPrivate, kRelayAddressUsable, "10/8 only with -dev" },
		{ Address(172, 16, 0, 1), kRelayAddressPrivate, kRelayAddressUsable, "172.16/12 only with -dev" },
		{ Address(172, 31, 255, 255), kRelayAddressPrivate, kRelayAddressUsable, "up to its last" },
		{ Address(172, 15, 255, 255), kRelayAddressUsable, kRelayAddressUsable, "but not 172.15.255.255, just before it" },
		{ Address(172, 32, 0, 0), kRelayAddressUsable, kRelayAddressUsable, "nor 172.32.0.0, just after it" },
		{ Address(192, 168, 1, 1), kRelayAddressPrivate, kRelayAddressUsable, "192.168/16 only with -dev" },
		{ Address(192, 169, 0, 1), kRelayAddressUsable, kRelayAddressUsable, "but not 192.169.0.1" },
		{ Address(169, 254, 1, 1), kRelayAddressPrivate, kRelayAddressUsable, "169.254/16 only with -dev" },
		{ Address(100, 64, 0, 1), kRelayAddressPrivate, kRelayAddressUsable, "100.64/10, a carrier's own, only with -dev" },
		{ Address(100, 127, 255, 255), kRelayAddressPrivate, kRelayAddressUsable, "up to its last" },
		{ Address(100, 63, 255, 255), kRelayAddressUsable, kRelayAddressUsable, "but not 100.63.255.255, just before it" },
		{ Address(100, 128, 0, 0), kRelayAddressUsable, kRelayAddressUsable, "nor 100.128.0.0, just after it" },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		bool const right = RelayList_AddressRefusal(cases[i].address, false) == cases[i].played &&
			RelayList_AddressRefusal(cases[i].address, true) == cases[i].developer;
		Check(right, cases[i].what);
	}

	bool allHaveWords = true;
	bool allDiffer = true;

	for (int a = kRelayAddressUsable; a <= kRelayAddressLoopback; a++)
	{
		const char *const words = RelayList_AddressWords(static_cast<RelayAddressRefusal>(a));
		allHaveWords = allHaveWords && words && words[0] && strstr(words, "no words") == NULL;

		for (int b = kRelayAddressUsable; b < a; b++)
		{
			allDiffer = allDiffer && strcmp(words, RelayList_AddressWords(static_cast<RelayAddressRefusal>(b))) != 0;
		}
	}

	Check(allHaveWords && allDiffer, "each refusal of an address has words of its own");

	// Where the names are looked up: on the list's own thread, IPv4 only, each address held to the rule before the relay is kept
	std::string const source = ReadSource("3dc/relay_list.cpp");
	std::string const thread = SourceSlice(source, "// ---- The list's thread", "// Makes the store and starts the thread", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(thread, "getaddrinfo(name, NULL, &hints, &found)") && Has(thread, "hints.ai_family = AF_INET;"),
		"a relay's name is looked up on the list's own thread, IPv4 only");
	Check(Has(thread, "RelayList_AddressRefusal(ntohl(address), shared->developer)") && Has(thread, "if (why != kRelayAddressUsable)"),
		"and a relay whose address the rule refuses is left out");
	Check(CountOf(source, "getaddrinfo(") == 1 && !Has(source, "enet_address_set_host") && !Has(source, "gethostbyname"),
		"and nowhere else in relay_list.cpp looks a name up");
}

// Added 2026-11-09 by Melanikus for the AvP port: -dev's test server reached with nothing changed on the tester's computer (relay_list.h):
// its address written plainly, the pin of its certificate, and the name people see for it
static void TestTheTestServer()
{
	puts("16. a test server by its address, its certificate's pin, and the name people see");

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

	// The pin
	char const *const nyc3 = "7f1096882c32876dbc4606b871b2c06bfdc504f9af483e4314a439403ece6767";
	uint8_t pin[kRelayListPinBytes];
	memset(pin, 0xAA, sizeof(pin));
	Check(RelayList_PinRefusal(nyc3, pin) == kRelayPinRead && pin[0] == 0x7f && pin[1] == 0x10 && pin[31] == 0x67, "a pin of 64 hex digits is read, in order");
	uint8_t upper[kRelayListPinBytes];
	Check(RelayList_ReadPin("7F1096882C32876DBC4606B871B2C06BFDC504F9AF483E4314A439403ECE6767", upper) && memcmp(pin, upper, sizeof(pin)) == 0,
		"capitals read the same");
	uint8_t untouched[kRelayListPinBytes];
	memset(untouched, 0x5A, sizeof(untouched));
	std::string const shortPin(nyc3, 63);
	std::string const longPin = std::string(nyc3) + "0";
	std::string colons;
	for (int i = 0; i < 32; i++)
	{
		colons += std::string(nyc3 + 2 * i, 2) + (i < 31 ? ":" : "");
	}
	Check(RelayList_PinRefusal(shortPin.c_str(), untouched) == kRelayPinWrongLength && RelayList_PinRefusal(longPin.c_str(), untouched) == kRelayPinWrongLength &&
		RelayList_PinRefusal(colons.c_str(), untouched) == kRelayPinWrongLength,
		"63 or 65 digits, or the pin written with colons as some tools print it, is the wrong length: the settings take the digits alone");
	std::string spaced = nyc3;
	spaced[10] = ' ';
	std::string notHex = nyc3;
	notHex[63] = 'g';
	Check(RelayList_PinRefusal(spaced.c_str(), untouched) == kRelayPinNotHex && RelayList_PinRefusal(notHex.c_str(), untouched) == kRelayPinNotHex,
		"a space or a letter that isn't hex, at the right length, is not hex");
	Check(RelayList_PinRefusal("", untouched) == kRelayPinNoText && RelayList_PinRefusal(NULL, untouched) == kRelayPinNoText &&
		RelayList_PinRefusal(nyc3, NULL) == kRelayPinNoText, "nothing at all, or nowhere to put it, is no text");
	Check(untouched[0] == 0x5A && untouched[31] == 0x5A, "and no refusal wrote anything, not even the part it had read");

	uint8_t digest[kRelayListPinBytes];
	memcpy(digest, pin, sizeof(digest));
	Check(RelayList_PinMatches(digest, pin), "the pinned certificate's SHA-256 matches its pin");
	digest[17] ^= 0x01;
	Check(!RelayList_PinMatches(digest, pin), "one bit different in the middle, and it doesn't");
	digest[17] ^= 0x01;
	digest[31] ^= 0x80;
	Check(!RelayList_PinMatches(digest, pin), "nor one in the last byte, which a compare that stopped early would reach last");
	Check(!RelayList_PinMatches(NULL, pin) && !RelayList_PinMatches(digest, NULL), "nor does a missing one");

	// The name people see
	Check(RelayList_ShownNameRefusal("New York (Mel's server)") == kRelayShownNameGood && RelayList_IsShownName("A") && RelayList_IsShownName("nyc3, test-1."),
		"the name people see: letters, digits, spaces and ' ( ) - . ,");
	Check(RelayList_IsShownName(std::string(32, 'x').c_str()) && RelayList_ShownNameRefusal(std::string(33, 'x').c_str()) == kRelayShownNameTooLong,
		"up to 32 of them, and 33 is too long");
	Check(RelayList_ShownNameRefusal("") == kRelayShownNameNoText && RelayList_ShownNameRefusal(NULL) == kRelayShownNameNoText, "none is no text");
	Check(RelayList_ShownNameRefusal("New York\n") == kRelayShownNameBadCharacter && RelayList_ShownNameRefusal("a\"b") == kRelayShownNameBadCharacter &&
		RelayList_ShownNameRefusal("a%s") == kRelayShownNameBadCharacter && RelayList_ShownNameRefusal("a/b") == kRelayShownNameBadCharacter &&
		RelayList_ShownNameRefusal("a\tb") == kRelayShownNameBadCharacter && RelayList_ShownNameRefusal("caf\xc3\xa9") == kRelayShownNameBadCharacter,
		"and anything else is a bad character: a newline, a quote, a %, a slash, a tab, a letter beyond ASCII");
}

int main()
{
	TestGoodLists();
	TestTheWholeList();
	TestOneGame();
	TestTexts();
	TestCodesAndRegions();
	TestNumbers();
	TestRelays();
	TestOwner();
	TestWords();
	TestBodies();
	TestServerNames();
	TestModeWords();
	TestTheCalls();
	TestTheMenus();
	TestAddresses();
	TestTheTestServer();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
