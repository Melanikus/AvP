// The test of the relay list's words: the list of games the game reads back, and the bodies it sends. Written 2026-11-08 by Melanikus
// for the AvP port.
//
// WHAT IT'S FOR: the Find games page shows what the relay server's list says, and that comes from the network, so it's hostile (AVP Tao
// 3). relay_list_text.h reads only exactly what PROTOCOL.md (the server's repository, section 4) says, and refuses anything else, all of
// it, for a reason of its own. This tries a good list, the largest the protocol allows, and each way a list can go wrong, and checks each
// is refused for its own reason. Then the bodies the game sends, which have to be exactly what the server takes, the check on the server's
// name from the settings, and the words for a game's style.
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

// One game's text, with these fields. Each is written as it goes in the JSON, quotes and all, so a test can put anything there.
struct GameFields
{
	std::string code = "\"ABCDEFGH\"";
	std::string name = "\"Hicks's game\"";
	std::string level = "\"Derelict\"";
	std::string mode = "\"Deathmatch\"";
	std::string players = "2";
	std::string maxPlayers = "4";
	std::string version = "113";
	std::string seconds = "75";
	std::string extra;   // written after the eight, comma and all
};

static std::string Game(const GameFields &f)
{
	return "{\"code\":" + f.code + ",\"name\":" + f.name + ",\"level\":" + f.level + ",\"mode\":" + f.mode + ",\"players\":" + f.players +
		",\"maxPlayers\":" + f.maxPlayers + ",\"version\":" + f.version + ",\"seconds\":" + f.seconds + f.extra + "}";
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
	puts("1. lists as the server writes them");

	int count = -1;
	Check(Read("{\"games\":[]}", &count) == kRelayListAccepted && count == 0, "a list with no games is read, with none");

	GameFields f;
	Check(Read(ListOf(Game(f)), &count) == kRelayListAccepted && count == 1, "a list with one game is read");
	Check(strcmp(games[0].code, "ABCDEFGH") == 0 && strcmp(games[0].name, "Hicks's game") == 0 && strcmp(games[0].level, "Derelict") == 0 &&
		strcmp(games[0].mode, "Deathmatch") == 0, "with its code, name, level and mode");
	Check(games[0].players == 2 && games[0].maxPlayers == 4 && games[0].version == 113 && games[0].seconds == 75, "and its numbers");

	std::string const shuffled = "{\"seconds\":0,\"version\":113,\"maxPlayers\":8,\"players\":8,\"mode\":\"m\",\"level\":\"l\",\"name\":\"n\",\"code\":\"ZZZZZZZZ\"}";
	Check(Read(ListOf(shuffled), &count) == kRelayListAccepted && count == 1 && games[0].players == 8 && games[0].seconds == 0,
		"the fields in any order, with 8 of 8 players and 0 seconds");

	std::string const spaced = " \r\n{ \"games\" :\t[ { \"code\" : \"ABCDEFGH\" , \"name\" : \"a\" , \"level\" : \"b\" , \"mode\" : \"c\" , \"players\" : 1 ,"
		" \"maxPlayers\" : 1 , \"version\" : 113 , \"seconds\" : 31536000 } ]\n}\n";
	Check(Read(spaced, &count) == kRelayListAccepted && count == 1 && games[0].seconds == kRelayListMostSeconds,
		"spaces, tabs and line ends between everything, and a game listed for a year");

	f.name = "\"\\\"quoted\\\" and \\\\back\\\\\"";
	Check(Read(ListOf(Game(f)), &count) == kRelayListAccepted && strcmp(games[0].name, "\"quoted\" and \\back\\") == 0,
		"\\\" and \\\\ in a name come out as \" and \\");

	f = GameFields();
	f.name = "\"" + std::string(32, 'x') + "\"";
	Check(Read(ListOf(Game(f)), &count) == kRelayListAccepted && strlen(games[0].name) == 32, "a name of 32 characters, the most");

	// The largest list the protocol allows, as the server's own test builds it: 100 games, every text 32 characters long, every one of
	// them a " or a \, which JSON writes as two, and every number at its longest. The protocol says that comes to 30,511 bytes.
	std::string text;
	std::string longest;

	for (int i = 0; i < 32; i++)
	{
		longest += (i & 1) ? "\\\\" : "\\\"";
	}

	for (int g = 0; g < kRelayListMostGames; g++)
	{
		GameFields big;
		big.code = CodeNumber(g);
		big.name = big.level = big.mode = "\"" + longest + "\"";
		big.players = "8";
		big.maxPlayers = "8";
		big.version = "65535";
		big.seconds = "31536000";
		text += (g ? "," : "") + Game(big);
	}

	text = ListOf(text);
	Check(text.size() == 30511, "the largest list the protocol allows is the 30,511 bytes it says");
	Check(text.size() <= kRelayListMostAnswerBytes, "which fits in what's read");
	Check(Read(text, &count, kRelayListMostVersion) == kRelayListAccepted && count == kRelayListMostGames, "and all 100 of its games are read");
	Check(strlen(games[99].mode) == 32, "each text 32 characters long");
}

static void TestTheWholeList()
{
	puts("2. the list as a whole");

	GameFields f;
	std::string const good = ListOf(Game(f));

	Refused(std::string(kRelayListMostAnswerBytes + 1, ' '), kRelayListTooBig, "a list over 32 KB is refused, without being read");
	Refused(good.substr(0, 10) + std::string(1, '\0') + good.substr(10), kRelayListHasAZero, "a list with a 0 byte in it is refused");
	Refused("", kRelayListEndsTooSoon, "an empty answer is refused");
	Refused("   ", kRelayListEndsTooSoon, "and one of only spaces");
	Refused("[]", kRelayListNotAnObject, "a list that isn't an object is refused");
	Refused("<html>", kRelayListNotAnObject, "and so is a web page");
	Refused("{}", kRelayListNotGames, "an object without \"games\" is refused");
	Refused("{\"game\":[]}", kRelayListNotGames, "and one with another field instead");
	Refused("{\"games\":[],\"addresses\":[]}", kRelayListNotGames, "and one with a field after \"games\"");
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

	Refused(ListOf(tooMany), kRelayListTooManyGames, "101 games are refused");

	GameFields second;
	second.code = "\"ABCDEFGH\"";
	Refused(ListOf(Game(f) + "," + Game(second)), kRelayListSameGameTwice, "two games with one code are refused");

	// A refusal after some games were read keeps none of them
	GameFields bad;
	bad.code = "\"ZZZZZZZZ\"";
	bad.players = "0";
	Refused(ListOf(Game(f) + "," + Game(bad)), kRelayListBadNumber, "a bad second game refuses the whole list, the good first one too");
}

static void TestOneGame()
{
	puts("3. a game on the list");

	Refused(ListOf("5"), kRelayListNotAGame, "a number where a game should be is refused");
	Refused(ListOf("\"game\""), kRelayListNotAGame, "and a text");
	Refused(ListOf("[]"), kRelayListNotAGame, "and a list");
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

	std::string const noSeconds = "{\"code\":\"ABCDEFGH\",\"name\":\"n\",\"level\":\"l\",\"mode\":\"m\",\"players\":1,\"maxPlayers\":1,\"version\":113}";
	Refused(ListOf(noSeconds), kRelayListFieldMissing, "a game missing a field is refused");

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

static void TestCodes()
{
	puts("5. codes");

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

	GameFields f;
	f.maxPlayers = "9";
	Refused(ListOf(Game(f)), kRelayListBadNumber, "room for 9 players is refused");

	f = GameFields();
	f.players = "5";
	f.maxPlayers = "4";
	Refused(ListOf(Game(f)), kRelayListMorePlayersThanRoom, "5 players in room for 4 are refused");

	f = GameFields();
	f.version = "0";
	Refused(ListOf(Game(f)), kRelayListBadNumber, "version 0 is refused");

	f.version = "65536";
	Refused(ListOf(Game(f)), kRelayListBadNumber, "and 65536");

	f.version = "112";
	Refused(ListOf(Game(f)), kRelayListOtherVersion, "a game for another version than the one asked for is refused");

	f = GameFields();
	f.seconds = "31536001";
	Refused(ListOf(Game(f)), kRelayListBadNumber, "a game listed for more than a year is refused");

	f.seconds = "9999999999";
	Refused(ListOf(Game(f)), kRelayListBadNumber, "and a number of 10 digits, without overflowing");

	f.seconds = "99999999999999999999999999";
	Refused(ListOf(Game(f)), kRelayListBadNumber, "and one of 26 digits");
}

static void TestWords()
{
	puts("7. the words for each refusal");

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

static void TestBodies()
{
	puts("8. the bodies the game sends");

	uint8_t token[kRelayTokenBytes];

	for (int i = 0; i < kRelayTokenBytes; i++)
	{
		token[i] = static_cast<uint8_t>(i * 17);
	}

	char body[kRelayListMostBodyBytes + 1];

	Check(RelayList_ListingBody(token, "Hicks's game", "Derelict", "Deathmatch", 2, 4, 113, body, sizeof(body)), "a listing is made");
	Check(strcmp(body, "{\"token\":\"00112233445566778899aabbccddeeff\",\"name\":\"Hicks's game\",\"level\":\"Derelict\",\"mode\":\"Deathmatch\","
		"\"players\":2,\"maxPlayers\":4,\"version\":113}") == 0, "exactly as the server takes it, the token in small-letter hex");

	Check(RelayList_ListingBody(token, "say \"hi\" \\o/", "l", "m", 1, 1, 1, body, sizeof(body)) &&
		strstr(body, "\"name\":\"say \\\"hi\\\" \\\\o/\"") != NULL, "a \" and a \\ in a name are escaped");

	Check(!RelayList_ListingBody(token, "", "l", "m", 1, 1, 113, body, sizeof(body)) && body[0] == '\0', "an empty name isn't sent");
	Check(!RelayList_ListingBody(token, std::string(33, 'x').c_str(), "l", "m", 1, 1, 113, body, sizeof(body)), "nor a name of 33 characters");
	Check(!RelayList_ListingBody(token, "n", "caf\xc3\xa9", "m", 1, 1, 113, body, sizeof(body)), "nor a level outside ASCII");
	Check(!RelayList_ListingBody(token, "n", "l", "m", 0, 4, 113, body, sizeof(body)), "nor 0 players");
	Check(!RelayList_ListingBody(token, "n", "l", "m", 5, 4, 113, body, sizeof(body)), "nor more players than room");
	Check(!RelayList_ListingBody(token, "n", "l", "m", 9, 9, 113, body, sizeof(body)), "nor room for 9");
	Check(!RelayList_ListingBody(token, "n", "l", "m", 1, 1, 65536, body, sizeof(body)), "nor version 65536");
	Check(!RelayList_ListingBody(NULL, "n", "l", "m", 1, 1, 113, body, sizeof(body)), "nor a listing with no token");
	Check(!RelayList_ListingBody(token, "n", "l", "m", 1, 1, 113, body, 20) && body[0] == '\0', "a body with too little room isn't sent, and is left empty");

	Check(RelayList_EndingBody(token, body, sizeof(body)) && strcmp(body, "{\"token\":\"00112233445566778899aabbccddeeff\"}") == 0,
		"the end of a game is its token");
	Check(!RelayList_EndingBody(token, body, 40) && body[0] == '\0', "and with too little room isn't sent");

	char fitted[kRelayListTextLetters + 1];
	RelayList_FitText("Hicks\x01's g\xe9me", "Game", fitted);
	Check(strcmp(fitted, "Hicks?'s g?me") == 0, "the game's own words have anything outside printable ASCII made ?");
	RelayList_FitText(std::string(40, 'y').c_str(), "Game", fitted);
	Check(strlen(fitted) == 32, "and are cut to 32 characters");
	RelayList_FitText("", "Game", fitted);
	Check(strcmp(fitted, "Game") == 0, "and nothing becomes the fallback");
	RelayList_FitText(NULL, NULL, fitted);
	Check(strcmp(fitted, "?") == 0, "and with no fallback either, a ?, so it's never empty");
}

static void TestServerNames()
{
	puts("9. the relay server's name, from the settings");

	const char *const good[] = { "relay.example.org", "a", "localhost", "a-b.c1.d", "127.0.0.1" };

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
	puts("10. the words for a game's style");

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
			RelayList_FitText(words, "x", fitted);
			allFit = allFit && strcmp(fitted, words) == 0;
		}
	}

	Check(allFit, "and every one of them fits the list as it is");
}

// relay_list.cpp's own source, as text, from the top of the repository
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
	puts("11. how relay_list.cpp makes its calls (read, not run: it needs the network)");

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
	Check(Has(source, "char findAnswer[kRelayListMostAnswerBytes];") && Has(source, "RelayList_ReadGames(findAnswer, a.bytes, kMultiplayerVersion"),
		"the list is read into 32 KB, and through the hostile reader, for this version");

	std::string const thread = SourceSlice(source, "// ---- The list's thread", "// Makes the store and starts the thread", kSliceEndAfterTheStart, FailedSlice);
	Check(thread.size() > 1000 && !Has(thread, "LogString(") && !Has(thread, "LogErrorString("), "the list's own thread never writes to the game's log");
	Check(!Has(source, " printf(") && !Has(source, "\tprintf(") && !Has(source, "OutputDebugString"), "and nothing prints what a call sent or got");
}

int main()
{
	TestGoodLists();
	TestTheWholeList();
	TestOneGame();
	TestTexts();
	TestCodes();
	TestNumbers();
	TestWords();
	TestBodies();
	TestServerNames();
	TestModeWords();
	TestTheCalls();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
