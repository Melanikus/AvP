// The test of how a level file is told apart as single player or multiplayer. Written 2026-11-07 for the AvP port.
// Credit: Melanikus and Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten).
//
// WHAT IT TESTS
//
// 3dc/level_kind.h reads a level file as hostile input and says what it's made for. This test builds small level files in memory,
// good ones and broken ones, and checks the answer. A broken one must be refused, and for exactly the reason it's broken: a check
// that's missing behind another check wouldn't show otherwise.
//
// Every file is built at the front of a larger buffer of zeros, and only its own size is passed on. So if a check is ever taken
// away and the reader goes too far, it reads zeros that belong to this test instead of memory that isn't there.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the settings in tests/level_kind_test.args, and the program gives back a non-zero result if
// any check failed. It needs no game files. tests/takeouts/level_kind_test.json proves each check can fail.

#include <stdio.h>

#include <string>
#include <vector>

#include "level_kind.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

typedef std::vector<unsigned char> Bytes;

static void PutNumber(Bytes &out, uint32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		out.push_back((unsigned char)(value >> (8 * i)));
	}
}

// One chunk: its 8-letter name, its size (heading included) unless told otherwise, then its body
static Bytes Chunk(const char *name, const Bytes &body, long long sizeToClaim = -1)
{
	Bytes out(name, name + 8);

	PutNumber(out, sizeToClaim >= 0 ? (uint32_t)sizeToClaim : (uint32_t)(12 + body.size()));
	out.insert(out.end(), body.begin(), body.end());

	return out;
}

static Bytes Join(std::initializer_list<Bytes> parts)
{
	Bytes out;

	for (const Bytes &part : parts)
	{
		out.insert(out.end(), part.begin(), part.end());
	}

	return out;
}

// A generator's body: its place, its facing, its type and its flags, then 4 bytes of the rest
static Bytes Generator(uint32_t type, uint32_t flags, size_t bodySize = 28)
{
	Bytes body;

	for (int i = 0; i < 4; i++)
	{
		PutNumber(body, 0);
	}

	PutNumber(body, type);
	PutNumber(body, flags);
	body.resize(bodySize, 0);

	return Chunk("AVPGENER", body);
}

// A whole file: the heading, with the file's size unless told otherwise, then the chunks
static Bytes File(const Bytes &chunks, long long sizeToClaim = -1)
{
	Bytes out = Chunk("REBINFF2", chunks, sizeToClaim);

	return out;
}

struct Answer
{
	LevelKind kind;
	LevelKindRefusal why;
	int starts;
};

// Reads the file from the front of a larger buffer of zeros, so that going too far stays inside this test's own memory
static Answer Read(const Bytes &file, size_t sizeToPass = (size_t)-1)
{
	Bytes room(file);
	room.resize(file.size() + 256, 0);

	Answer answer;
	answer.why = kLevelRefusedNot;
	answer.starts = -1;
	answer.kind = LevelKind_Of(room.data(), sizeToPass == (size_t)-1 ? file.size() : sizeToPass, &answer.why, &answer.starts);

	return answer;
}

static const uint32_t kMarine = 0x800;
static const uint32_t kStart = kLevelKindMultiplayerStartFlag;

static Bytes Level(std::initializer_list<Bytes> specialObjects)
{
	return File(Chunk("REBENVDT", Chunk("SPECLOBJ", Join(specialObjects))));
}

static void TestGoodFiles()
{
	puts("Level files that add up:");

	Answer a = Read(Level({ Generator(0, kMarine) }));
	Check(a.kind == kLevelKindSinglePlayer && a.why == kLevelRefusedNot && a.starts == 0, "a level whose only generator isn't a multiplayer start is for single player");

	a = Read(Level({ Generator(0, kMarine | kStart) }));
	Check(a.kind == kLevelKindMultiplayer && a.starts == 1, "a level with one multiplayer start is for multiplayer");

	a = Read(Level({ Generator(0, kStart), Generator(0, kMarine), Generator(0, kStart | kMarine), Generator(0, kStart) }));
	Check(a.kind == kLevelKindMultiplayer && a.starts == 3, "every multiplayer start is counted, and nothing else");

	a = Read(Level({ Generator(1, kStart) }));
	Check(a.kind == kLevelKindSinglePlayer && a.starts == 0, "a generator of another type with the flag isn't a start, as in object_setup.cpp");

	a = Read(Level({}));
	Check(a.kind == kLevelKindSinglePlayer, "a level with no generators at all is for single player");

	// The game only reads multiplayer starts from the special objects, so one anywhere else doesn't count
	a = Read(File(Chunk("REBENVDT", Join({ Generator(0, kStart), Chunk("SPECLOBJ", Bytes()) }))));
	Check(a.kind == kLevelKindSinglePlayer && a.starts == 0, "a start outside the special objects isn't counted");

	// A chunk that isn't a row of chunks is never walked into, so what's inside it doesn't matter
	Bytes junk(40, 0xEE);
	a = Read(File(Join({ Chunk("SHAPE___", junk), Chunk("REBENVDT", Join({ Chunk("CAMORIGN", junk), Chunk("SPECLOBJ", Generator(0, kStart)) })) })));
	Check(a.kind == kLevelKindMultiplayer && a.why == kLevelRefusedNot, "other chunks are stepped over, whatever their bodies hold");
}

static void Refused(const Answer &a, LevelKindRefusal why, const char *what)
{
	Check(a.kind == kLevelKindUnreadable && a.why == why && a.starts == 0, what);
}

static void TestBrokenFiles()
{
	puts("Level files that don't add up are refused, each for its own reason:");

	LevelKindRefusal why = kLevelRefusedNot;
	Check(LevelKind_Of(0, 5, &why) == kLevelKindUnreadable && why == kLevelRefusedNoData, "no data at all");
	Check(LevelKind_Of(0, 100, &why) == kLevelKindUnreadable && why == kLevelRefusedNoData, "no data at all, though a size is given");

	Bytes good = Level({ Generator(0, kMarine) });
	Refused(Read(good, 11), kLevelRefusedTooShort, "shorter than the file's heading");

	Bytes renamed = good;
	renamed[7] = '3';
	Refused(Read(renamed), kLevelRefusedNotALevel, "a heading that isn't REBINFF2");

	Refused(Read(File(Chunk("REBENVDT", Bytes()), 999)), kLevelRefusedWrongSize, "a heading whose size isn't the file's");
	Refused(Read(good, good.size() - 4), kLevelRefusedWrongSize, "a file cut short, so its heading's size is more than there is");

	Refused(Read(File(Join({ Chunk("SHAPE___", Bytes(), 8), Chunk("REBENVDT", Bytes()) }))), kLevelRefusedChunkTooSmall, "a chunk claiming to be smaller than its own heading");
	Refused(Read(File(Join({ Chunk("REBENVDT", Bytes()), Chunk("SHAPE___", Bytes(), 0xFFFFFFF0LL) }))), kLevelRefusedChunkPastTheEnd, "a chunk claiming far more than the file holds");
	Refused(Read(File(Join({ Chunk("REBENVDT", Bytes()), Bytes(5, 0) }))), kLevelRefusedChunkPastTheEnd, "bytes left over at the end, too few to be a chunk");
	Refused(Read(File(Chunk("REBENVDT", Chunk("SHAPE___", Bytes(), 200)))), kLevelRefusedChunkPastTheEnd, "a chunk going on past the end of the environment that holds it");
	Refused(Read(File(Chunk("REBENVDT", Chunk("SPECLOBJ", Chunk("AVPGENER", Bytes(30, 0), 300))))), kLevelRefusedChunkPastTheEnd, "a generator going on past the end of the special objects");

	Refused(Read(File(Chunk("SHAPE___", Bytes(4, 0)))), kLevelRefusedNoEnvironment, "a file that holds no level environment");
	Refused(Read(Level({ Generator(0, kStart, 20) })), kLevelRefusedShortGenerator, "a generator too short to hold its flags");
	Refused(Read(Level({ Generator(0, kStart), Generator(0, kStart, 23) })), kLevelRefusedShortGenerator, "and one a byte too short, after a good start, which then counts for nothing");
}

int main()
{
	TestGoodFiles();
	TestBrokenFiles();

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
