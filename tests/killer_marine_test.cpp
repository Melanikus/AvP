// The test of the rules for the marine who can kill Olde (3dc/avp/killer_marine.h). Written 2026-11-10 by Melanikus for the AvP port
// (#210).
//
// WHAT IT TESTS
//
// - Kill Olde mode: the list holds only the stand-in, clearly named; a fingerprint is compared whole, so a difference in its first, a
//   middle or its last byte is no match; a player of all zeros is nobody and never matches, even an entry of zeros, while a fingerprint
//   with any one byte that isn't zero is somebody; every player and every entry of the list is looked at; more players than a game can
//   have leaves the mode off; and the log's line names the entry and never writes its bytes.
// - The reaction time: nothing before 500 ms of unbroken sight (499 ms isn't enough); losing sight for one look starts the wait again;
//   being told nothing for more than 250 ms starts it again; a clock that went backwards starts it again; the clock going back to 0 is
//   no break.
// - The aim: he aims at what he saw at least 500 ms ago, carried forward at the speed he saw, which is exact on a straight line; after a
//   turn his aim keeps going the old way for 500 ms; for the first tenth of a second he saw, he can't tell a speed; it still works with a
//   frame every millisecond, and after many seconds; and a sum past the edge of the game's numbers stops at the edge.
// - By reading the game's files: the numbers the header copies (16 bytes a fingerprint, 8 players) are the game's, and a joiner's
//   fingerprint is still wiped to zeros until his handshake is done.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/killer_marine_test.args, and the program gives back a non-zero result if any check failed.
// tests/takeouts/killer_marine_test.json proves each check can fail.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "killer_marine.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A file of the game's, with its line ends written \n whatever the checkout made them. /FC in this test's settings makes __FILE__ this
// file's whole path, so the repository's top is found from it.
static std::string ReadSourceFile(char const *name)
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
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

// ----- Kill Olde mode

typedef uint8_t Fingerprint[kKillerMarineFingerprintBytes];

// The stand-in's fingerprint with one byte changed, or none changed for a place past the end
static void StandInWith(Fingerprint out, size_t changed)
{
	memcpy(out, kKillerMarineList[0].fingerprint, kKillerMarineFingerprintBytes);

	if (changed < kKillerMarineFingerprintBytes)
	{
		out[changed] ^= 0x01;
	}
}

static size_t const kNoneChanged = kKillerMarineFingerprintBytes;

// A fingerprint of all zeros but one byte
static void ZerosBut(Fingerprint out, size_t place, uint8_t value)
{
	memset(out, 0, kKillerMarineFingerprintBytes);
	out[place] = value;
}

static void TestTheNumbers()
{
	puts("The numbers, and the game's files they come from:");

	std::string const identity = ReadSourceFile("3dc/net_identity.h");
	std::string const players = ReadSourceFile("3dc/avp/win95/pldnet.h");
	std::string const peer = ReadSourceFile("3dc/net_peer_v2.h");
	std::string const crypto = ReadSourceFile("3dc/net_crypto_cng.cpp");

	Check(!identity.empty() && !players.empty() && !peer.empty() && !crypto.empty(), "the game's four files are read");
	Check(kKillerMarineFingerprintBytes == 16 && Has(identity, "size_t const kNetIdentityFingerprintBytes = 16;"),
		"a fingerprint is 16 bytes, here and in net_identity.h");
	Check(kKillerMarineMostPlayers == 8 && Has(players, "#define NET_MAXPLAYERS              (8)"),
		"a game has at most 8 players, here and in pldnet.h");
	Check(Has(peer, "NetCrypto_Wipe(joinerFingerprint, sizeof(joinerFingerprint));") && Has(crypto, "SecureZeroMemory(bytes, size);"),
		"a joiner's place wipes his fingerprint to zeros until his handshake is done (net_peer_v2.h, net_crypto_cng.cpp)");
	Check(kKillerMarineReactionMs == 500, "the owner's reaction time is 500 ms");
	Check(kKillerMarineStaleMs == 250 && kKillerMarineEveryMs == 16 && kKillerMarineGlimpses == 64 && kKillerMarineSpeedOverMs == 100,
		"told nothing for 250 ms starts the wait again; a glimpse at most every 16 ms, 64 kept; a speed is measured over 100 ms or more");
}

static void TestTheList()
{
	puts("The list:");

	Check(kKillerMarineListCount == 1, "it has one entry");
	Check(std::string(kKillerMarineList[0].name) == "the stand-in (not Olde)", "named as the stand-in, not Olde");
	Check(memcmp(kKillerMarineList[0].fingerprint, "NOT-OLDE-STANDIN", kKillerMarineFingerprintBytes) == 0,
		"its 16 bytes are the letters NOT-OLDE-STANDIN, which no real fingerprint is");
	Check(!KillerMarine_IsNobody(kKillerMarineList[0].fingerprint), "so it isn't nobody");
}

static void TestTheMode()
{
	puts("Kill Olde mode:");

	Fingerprint players[kKillerMarineMostPlayers + 1];

	StandInWith(players[0], kNoneChanged);
	Check(KillerMarine_SameFingerprint(players[0], kKillerMarineList[0].fingerprint), "a fingerprint is the same as itself");
	Check(KillerMarine_WhoIsIn(players, 1) == &kKillerMarineList[0], "the stand-in in the game turns it on, naming the list's entry");
	Check(KillerMarine_WhoIsIn(players, 0) == NULL, "nobody in the game leaves it off");
	Check(KillerMarine_WhoIsIn(NULL, 1) == NULL, "and so does no list of players at all");

	StandInWith(players[0], 0);
	Check(KillerMarine_WhoIsIn(players, 1) == NULL, "a fingerprint that differs only in its first byte is no match");
	StandInWith(players[0], 7);
	Check(KillerMarine_WhoIsIn(players, 1) == NULL, "nor one that differs only in a middle byte");
	StandInWith(players[0], kKillerMarineFingerprintBytes - 1);
	Check(KillerMarine_WhoIsIn(players, 1) == NULL, "nor one that differs only in its last byte");

	// Eight players, the stand-in first or last; then nine
	for (size_t i = 0; i < kKillerMarineMostPlayers + 1; i++)
	{
		StandInWith(players[i], i % kKillerMarineFingerprintBytes);
	}

	StandInWith(players[kKillerMarineMostPlayers - 1], kNoneChanged);
	Check(KillerMarine_WhoIsIn(players, kKillerMarineMostPlayers) == &kKillerMarineList[0], "the stand-in as the eighth of eight players is found");
	Check(KillerMarine_WhoIsIn(players, kKillerMarineMostPlayers - 1) == NULL, "and the seven before him aren't him");
	StandInWith(players[0], kNoneChanged);
	StandInWith(players[kKillerMarineMostPlayers - 1], 3);
	Check(KillerMarine_WhoIsIn(players, kKillerMarineMostPlayers) == &kKillerMarineList[0], "the stand-in as the first of eight is found");
	StandInWith(players[kKillerMarineMostPlayers], kNoneChanged);
	Check(KillerMarine_WhoIsIn(players, kKillerMarineMostPlayers + 1) == NULL, "nine players is more than a game has, so it stays off");

	// Lists of the test's own
	KillerMarineListed list[2] = {
		{ "first", { 0 } },
		{ "second", { 0 } },
	};

	ZerosBut(list[0].fingerprint, 0, 0xA1);
	ZerosBut(list[1].fingerprint, 5, 0xB2);
	ZerosBut(players[0], 5, 0xB2);
	Check(KillerMarine_ListedPlayerIn(players, 1, list, 2) == &list[1], "the last entry of a list is looked at too");
	Check(KillerMarine_ListedPlayerIn(players, 1, NULL, 2) == NULL, "no list at all finds nobody");

	ZerosBut(list[0].fingerprint, 0, 0x01);
	ZerosBut(players[0], 0, 0x01);
	Check(KillerMarine_ListedPlayerIn(players, 1, list, 1) == &list[0], "a fingerprint whose first byte isn't zero is somebody");
	ZerosBut(list[0].fingerprint, kKillerMarineFingerprintBytes - 1, 0x01);
	ZerosBut(players[0], kKillerMarineFingerprintBytes - 1, 0x01);
	Check(KillerMarine_ListedPlayerIn(players, 1, list, 1) == &list[0], "and so is one whose last byte isn't");

	memset(list[0].fingerprint, 0, kKillerMarineFingerprintBytes);
	memset(players[0], 0, kKillerMarineFingerprintBytes);
	Check(KillerMarine_ListedPlayerIn(players, 1, list, 1) == NULL, "a player of all zeros is nobody: he never matches, even an entry of zeros");
}

static void TestTheLine()
{
	puts("The log's line:");

	char line[200];
	Fingerprint player;

	StandInWith(player, kNoneChanged);
	KillerMarine_ModeLine(KillerMarine_WhoIsIn(&player, 1), line, sizeof(line));
	Check(std::string(line) == "killer marine: Kill Olde mode is on, because the stand-in (not Olde) is in the game", "on, it names the entry");
	Check(!Has(line, "NOT-OLDE-STANDIN") && !Has(line, "4e4f542d4f4c44452d5354414e44494e") && !Has(line, "4E4F542D4F4C44452D5354414E44494E"),
		"and never writes the fingerprint, as letters or as hex");

	KillerMarine_ModeLine(NULL, line, sizeof(line));
	Check(std::string(line) == "killer marine: Kill Olde mode is off, because nobody on the list is in the game", "off, it says so");

	KillerMarine_ModeLine(NULL, line, 10);
	Check(strlen(line) == 9, "a line too long for its place is cut short, and still ends with a 0");

	KillerMarine_ModeLine(NULL, NULL, sizeof(line));
	Check(true, "no place for the line writes nothing");
}

// ----- The reaction time and the aim

// Where the target is, ms after the test's start
typedef void (*Where)(uint32_t ms, int32_t *x, int32_t *y, int32_t *z);

// Standing still
static void StandingStill(uint32_t, int32_t *x, int32_t *y, int32_t *z)
{
	*x = 1000;
	*y = -500;
	*z = 2000;
}

// A straight line in all three directions, a whole number of mm each ms
static void StraightLine(uint32_t ms, int32_t *x, int32_t *y, int32_t *z)
{
	*x = 10000 + 4 * static_cast<int32_t>(ms);
	*y = -2000 - 1 * static_cast<int32_t>(ms);
	*z = 500 + 2 * static_cast<int32_t>(ms);
}

// Going along x at 4 mm a ms, then at 1000 ms turning to go along z instead
static void TurnsAt1000(uint32_t ms, int32_t *x, int32_t *y, int32_t *z)
{
	int32_t const t = static_cast<int32_t>(ms);

	*x = t <= 1000 ? 4 * t : 4000;
	*y = 0;
	*z = t <= 1000 ? 0 : 4 * (t - 1000);
}

// Going along x towards the largest number the game's positions hold, reaching it at 1000 ms and stopping there
static void ToTheTopEdge(uint32_t ms, int32_t *x, int32_t *y, int32_t *z)
{
	int32_t const t = static_cast<int32_t>(ms < 1000 ? ms : 1000);

	*x = INT32_MAX - 4000 + 4 * t;
	*y = 0;
	*z = 0;
}

// And towards the smallest
static void ToTheBottomEdge(uint32_t ms, int32_t *x, int32_t *y, int32_t *z)
{
	int32_t const t = static_cast<int32_t>(ms < 1000 ? ms : 1000);

	*x = INT32_MIN + 4000 - 4 * t;
	*y = 0;
	*z = 0;
}

// He looks, seeing the target, at start + from, then every step ms up to start + to
static void LookEvery(KillerMarineEyes *eyes, uint32_t start, uint32_t from, uint32_t to, uint32_t step, Where where)
{
	for (uint32_t ms = from; ms <= to; ms += step)
	{
		int32_t x, y, z;

		where(ms, &x, &y, &z);
		KillerMarine_Look(eyes, start + ms, true, x, y, z);
	}
}

// Whether, asked at now, he aims where the target was ms after the test's start, give or take a millimetre
static bool AimsWhereItWas(const KillerMarineEyes *eyes, uint32_t now, uint32_t ms, Where where)
{
	int32_t x = 0, y = 0, z = 0;
	int32_t wantX, wantY, wantZ;

	where(ms, &wantX, &wantY, &wantZ);

	return KillerMarine_AimPoint(eyes, now, &x, &y, &z) && llabs((long long)x - wantX) <= 1 && llabs((long long)y - wantY) <= 1
		&& llabs((long long)z - wantZ) <= 1;
}

// Whether he aims where the target is at that time
static bool AimsAt(const KillerMarineEyes *eyes, uint32_t start, uint32_t ms, Where where)
{
	return AimsWhereItWas(eyes, start + ms, ms, where);
}

static void TestTheWait()
{
	puts("The reaction time:");

	KillerMarineEyes eyes;
	uint32_t const start = 1000;

	KillerMarine_EyesShut(&eyes);
	Check(!KillerMarine_MayAct(&eyes, start) && !KillerMarine_MayAct(&eyes, start + 1000), "with nothing seen, he may not act");

	LookEvery(&eyes, start, 0, 496, 16, StandingStill);
	LookEvery(&eyes, start, 499, 499, 1, StandingStill);
	Check(!KillerMarine_MayAct(&eyes, start + 499), "after 499 ms of seeing, he may not act yet");

	int32_t x = 7, y = 7, z = 7;
	Check(!KillerMarine_AimPoint(&eyes, start + 499, &x, &y, &z) && x == 7 && y == 7 && z == 7, "nor aim, and nothing is written");

	LookEvery(&eyes, start, 500, 500, 1, StandingStill);
	Check(KillerMarine_MayAct(&eyes, start + 500), "after 500 ms, he may");
	Check(AimsAt(&eyes, start, 500, StandingStill), "and he aims where it stands");

	KillerMarine_Look(&eyes, start + 516, false, 0, 0, 0);
	Check(!KillerMarine_MayAct(&eyes, start + 516), "losing sight for one look, he may not act");
	LookEvery(&eyes, start, 532, 1028, 16, StandingStill);
	LookEvery(&eyes, start, 1031, 1031, 1, StandingStill);
	Check(!KillerMarine_MayAct(&eyes, start + 1031), "and seeing again, the wait starts again: not after 499 ms");
	LookEvery(&eyes, start, 1032, 1032, 1, StandingStill);
	Check(KillerMarine_MayAct(&eyes, start + 1032), "but after 500");

	Check(KillerMarine_MayAct(&eyes, start + 1032 + 250), "told nothing for 250 ms, he may still act");
	Check(!KillerMarine_MayAct(&eyes, start + 1032 + 251), "told nothing for 251 ms, he may not");

	// Told nothing for a while, then seeing it
	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 400, 16, StandingStill);
	LookEvery(&eyes, start, 650, 650, 1, StandingStill);
	Check(KillerMarine_MayAct(&eyes, start + 650), "a gap of 250 ms between looks is no break");

	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 400, 16, StraightLine);
	LookEvery(&eyes, start, 700, 1196, 16, StraightLine);
	LookEvery(&eyes, start, 1199, 1199, 1, StraightLine);
	Check(!KillerMarine_MayAct(&eyes, start + 1199), "a gap of 300 ms is: the wait starts again, and 499 ms later he may not act");
	LookEvery(&eyes, start, 1200, 1200, 1, StraightLine);
	Check(KillerMarine_MayAct(&eyes, start + 1200), "500 ms later he may");
	Check(AimsWhereItWas(&eyes, start + 1200, 700, StraightLine),
		"and what he saw before the gap is forgotten: with no speed to tell yet, he aims where he first saw it again, 500 ms ago");

	// The clock going backwards
	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start + 4000, 0, 600, 16, StandingStill);
	Check(KillerMarine_MayAct(&eyes, start + 4600), "seeing for 600 ms, he may act");
	KillerMarine_Look(&eyes, start + 3000, true, 1000, -500, 2000);
	Check(!KillerMarine_MayAct(&eyes, start + 3000), "a look with the clock gone backwards starts the wait again");
	LookEvery(&eyes, start + 3000, 16, 496, 16, StandingStill);
	Check(!KillerMarine_MayAct(&eyes, start + 3496), "and he may not act 496 ms after it");

	// The clock going back to 0: 256 ms before it does, and on past it
	uint32_t const nearTheEnd = 0xFFFFFF00u;

	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, nearTheEnd, 0, 496, 16, StraightLine);
	LookEvery(&eyes, nearTheEnd, 499, 499, 1, StraightLine);
	Check(!KillerMarine_MayAct(&eyes, nearTheEnd + 499), "across the clock going back to 0: not after 499 ms");
	LookEvery(&eyes, nearTheEnd, 500, 500, 1, StraightLine);
	Check(KillerMarine_MayAct(&eyes, nearTheEnd + 500), "but after 500, which is no break");
	LookEvery(&eyes, nearTheEnd, 512, 2000, 16, StraightLine);
	Check(AimsAt(&eyes, nearTheEnd, 2000, StraightLine), "and his aim on a straight line, past it, is exact");
}

static void TestTheAim()
{
	puts("The aim:");

	KillerMarineEyes eyes;
	uint32_t const start = 50000;

	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 576, 16, StraightLine);
	Check(AimsAt(&eyes, start, 590, StraightLine) == false, "for the first tenth of a second he saw, he can't tell a speed, so his aim isn't where it is now...");

	int32_t x, y, z;
	int32_t wantX, wantY, wantZ;

	StraightLine(80, &wantX, &wantY, &wantZ);
	Check(KillerMarine_AimPoint(&eyes, start + 590, &x, &y, &z) && x == wantX && y == wantY && z == wantZ,
		"...but where he saw it 510 ms before: the newest glimpse at least 500 ms old");

	LookEvery(&eyes, start, 592, 2000, 16, StraightLine);
	Check(AimsAt(&eyes, start, 2000, StraightLine), "once he can tell a speed, his aim on a straight line is where it is now, in all three directions");

	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 5000, 16, StraightLine);
	Check(AimsAt(&eyes, start, 5000, StraightLine), "and still after 5 seconds, with the kept glimpses gone round many times");

	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 1000, 1, StraightLine);
	Check(AimsAt(&eyes, start, 1000, StraightLine), "and with a frame every millisecond, when only one look in 16 is kept");

	// A turn at 1000 ms
	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 1000, 16, TurnsAt1000);

	bool oldWay = true;

	for (uint32_t ms = 1008; ms <= 1504; ms += 16)
	{
		LookEvery(&eyes, start, ms, ms, 1, TurnsAt1000);
		oldWay = oldWay && KillerMarine_AimPoint(&eyes, start + ms, &x, &y, &z) && z == 0 && x > 4000;
	}

	Check(oldWay, "after a turn, his aim keeps going the old way for 500 ms");

	bool newWay = true;

	for (uint32_t ms = 1520; ms <= 2400; ms += 16)
	{
		LookEvery(&eyes, start, ms, ms, 1, TurnsAt1000);

		if (ms >= 1700)
		{
			newWay = newWay && AimsAt(&eyes, start, ms, TurnsAt1000);
		}
	}

	Check(newWay, "and once both glimpses he goes by are after the turn, it's where the target is now");

	// The edges of the game's numbers. On x86 a number too small to hold already comes out as the smallest one when it's made a whole
	// number, so the second check can't tell that the header keeps it there; the header does because C++ doesn't promise that.
	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 1248, 16, ToTheTopEdge);
	Check(KillerMarine_AimPoint(&eyes, start + 1248, &x, &y, &z) && x == INT32_MAX, "a sum past the largest position stops there");
	KillerMarine_EyesShut(&eyes);
	LookEvery(&eyes, start, 0, 1248, 16, ToTheBottomEdge);
	Check(KillerMarine_AimPoint(&eyes, start + 1248, &x, &y, &z) && x == INT32_MIN, "and one past the smallest stops there");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	puts("killer_marine_test: the rules for the marine who can kill Olde (3dc/avp/killer_marine.h)");

	TestTheNumbers();
	TestTheList();
	TestTheMode();
	TestTheLine();
	TestTheWait();
	TestTheAim();

	if (checksFailed != 0)
	{
		printf("killer_marine_test: %d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("killer_marine_test: every check passed");
	return 0;
}
