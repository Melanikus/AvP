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

static void TestTheCalls()
{
	puts("13. how relay_list.cpp makes its calls (read, not run: it needs the network)");

	std::string const source = ReadSource("3dc/relay_list.cpp");
	Check(source.size() > 5000, "the source was found and read, from the top of the repository");

	std::string const call = SourceSlice(source, "Answer Call(const wchar_t *method", "\n\t}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(call, "WINHTTP_FLAG_SECURE)"), "every call is HTTPS");
	Check(Has(call, "WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;") && !Has(source, "TLS1_1") && !Has(source, "PROTOCOL_TLS1 ") &&
		!Has(source, "PROTOCOL_SSL") && !Has(source, "PROTOCOL_ALL"), "with TLS 1.2 or newer, and nothing older named anywhere");
	Check(!Has(source, "SECURITY_FLAG_IGNORE") && !Has(source, "WINHTTP_OPTION_SECURITY_FLAGS"), "and nothing turns the certificate check off");
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

	// The page of games found is set up again each time its list changes. Asking the list there would ask again for every answer, for
	// as long as the page is open, so it has to be in the part that runs only when the page is first opened.
	std::string const firstOpen = SourceSlice(menus, "\t\t\tif (previousMenuID != AVPMENU_MULTIPLAYERSELECTSESSION)\n\t\t\t{", "\n\t\t\t}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(firstOpen, "RelayList_FindGames();") && CountOf(menus, "RelayList_FindGames();") == 1,
		"the page of games found asks the list once each time it's opened, and nowhere else");

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

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
