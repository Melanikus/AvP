// The test of the checks on what arrives from other players' machines. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A damage message carries an ammunition number that another player's machine wrote. The game uses it as a place in its table of ammunition, so it has to be
// checked first. This tries the check on every number the message can hold, and on the numbers either side of each end of the table. It also tests the limit on
// how much a flood of bad messages can write to the log.
//
// WHAT IT DOESN'T TEST
//
// pldnet.cpp is far too tied to the rest of the game to compile here. So this shows the check gives the right answers, and nothing here shows that the three
// handlers ask it. For those, there's only the game building, and reading the code.

#include <stdio.h>
#include <string.h>

#include "3dc.h"
#include "equipment.h"
#include "net_checks.h"

// The field the number travels in, in all three damage messages (pldnet.h).
struct AmmunitionField
{
	short ammo_id : 11;
};

// Every real number has to fit that field, or the sender's own numbers would arrive changed.
static_assert(AMMO_NONE >= -1024 && AMMO_FIREDAMAGE_POSTMAX <= 1023, "an ammunition number no longer fits the field in a damage message");

static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static bool FitsAmmunitionTable(int number)
{
	return NetNumberFitsTable(number, MAX_NO_OF_AMMO_TEMPLATES);
}

static void TestAmmunitionNumbers()
{
	puts("ammunition numbers");

	Check(!FitsAmmunitionTable(-1024), "the lowest number a message can hold is refused");
	Check(!FitsAmmunitionTable(AMMO_NONE), "the number for no ammunition is refused");
	Check(FitsAmmunitionTable(0), "the first place in the table is accepted");
	Check(FitsAmmunitionTable(MAX_NO_OF_AMMO_TEMPLATES - 1), "the last place in the table is accepted");
	Check(!FitsAmmunitionTable(MAX_NO_OF_AMMO_TEMPLATES), "one past the last place is refused");
	Check(!FitsAmmunitionTable(AMMO_FLECHETTE_POSTMAX), "a number after the table is refused");
	Check(!FitsAmmunitionTable(AMMO_FALLING_POSTMAX), "another number after the table is refused");
	Check(!FitsAmmunitionTable(AMMO_FIREDAMAGE_POSTMAX), "the last number after the table is refused");
	Check(!FitsAmmunitionTable(1023), "the highest number a message can hold is refused");

	// Every number the field can hold, as it comes out of the field, so that the sign is the one the game sees.
	int accepted = 0;
	int lowest = 0;
	int highest = 0;

	for (int bits = 0; bits < 2048; bits++)
	{
		AmmunitionField field;
		field.ammo_id = (short)bits;

		int const number = field.ammo_id;

		lowest = (number < lowest) ? number : lowest;
		highest = (number > highest) ? number : highest;

		if (FitsAmmunitionTable(number))
		{
			accepted++;
		}
	}

	Check(lowest == -1024 && highest == 1023, "the field really holds -1024 to 1023");
	Check(accepted == MAX_NO_OF_AMMO_TEMPLATES, "of all those numbers, exactly the table's places are accepted");

	Check(!NetNumberFitsTable(0, 0), "nothing fits a table with no places");
}

static void TestTheLogCantBeFlooded()
{
	puts("a flood of bad messages");

	unsigned long long count = 0;
	bool first[17];

	for (int drop = 1; drop <= 16; drop++)
	{
		first[drop] = LogThisDrop(count);
	}

	Check(first[1] && first[2] && first[3] && first[4] && first[5], "the first five are logged");
	Check(!first[6] && !first[7], "the sixth and seventh aren't");
	Check(first[8], "the eighth is");
	Check(!first[9] && !first[15], "the ones between aren't");
	Check(first[16], "the sixteenth is");

	count = 0;
	int logged = 0;

	for (int drop = 0; drop < 10000000; drop++)
	{
		if (LogThisDrop(count))
		{
			logged++;
		}
	}

	Check(logged > 5 && logged < 30, "ten million of them add only a few lines");
}

// A small made-up table, so that these say something about the walking and nothing about the game's own messages. tests/net_start_test.cpp checks the game's table against its source.
enum { kPartFixed, kPartEmpty, kPartHosts, kPartVaries, kPartBig, kPartKinds };

static const NetPartRule kTestRules[kPartKinds] =
{
	{ kPartFixed, 3, false, false },
	{ kPartEmpty, 0, false, false },
	{ kPartHosts, 2, false, true },
	{ kPartVaries, 1, true, false },
	{ kPartBig, 60000, false, false }
};

// A part whose length varies says so in its first byte. 0 means it doesn't fit, as for the real ones.
static size_t TestLength(unsigned char, const unsigned char *part, size_t)
{
	return part[0];
}

static NetPartsAnswer Walk(const unsigned char *parts, size_t size, bool fromTheHost = false)
{
	return NetCheckParts(parts, size, kTestRules, kPartKinds, fromTheHost, TestLength);
}

static void TestMessageParts()
{
	puts("the parts of a message");

	unsigned char const fixedOnly[] = { kPartFixed, 1, 2, 3 };

	Check(Walk(fixedOnly, 0) == kNetPartsGood, "nothing at all is fine: a message can have no parts");
	Check(Walk(fixedOnly, sizeof(fixedOnly)) == kNetPartsGood, "a part with all its bytes is good");
	Check(Walk(fixedOnly, sizeof(fixedOnly) - 1) == kNetPartsTooShort, "one byte short is too short");
	Check(Walk(fixedOnly, 1) == kNetPartsTooShort, "and so is the first byte alone");

	unsigned char const twoParts[] = { kPartFixed, 1, 2, 3, kPartEmpty, kPartFixed, 4, 5, 6 };

	Check(Walk(twoParts, sizeof(twoParts)) == kNetPartsGood, "several parts are walked one after another");
	Check(Walk(twoParts, sizeof(twoParts) - 1) == kNetPartsTooShort, "and a short one at the end is found");

	unsigned char const unknownNext[] = { kPartEmpty, kPartKinds };
	unsigned char const unknownFar[] = { 255 };

	Check(Walk(unknownNext, sizeof(unknownNext)) == kNetPartsUnknownKind, "the first kind past the table is unknown");
	Check(Walk(unknownFar, sizeof(unknownFar)) == kNetPartsUnknownKind, "and so is the last a byte can say");

	unsigned char const lastKind[] = { kPartBig };

	Check(Walk(lastKind, sizeof(lastKind)) == kNetPartsTooShort, "the last kind in the table is known");

	unsigned char const fromHost[] = { kPartHosts, 1, 2 };

	Check(Walk(fromHost, sizeof(fromHost), true) == kNetPartsGood, "only the host may say this, and the host can");
	Check(Walk(fromHost, sizeof(fromHost), false) == kNetPartsOnlyFromTheHost, "and nobody else");

	unsigned char const hiddenAfterOthers[] = { kPartFixed, 1, 2, 3, kPartHosts, 1, 2 };

	Check(Walk(hiddenAfterOthers, sizeof(hiddenAfterOthers)) == kNetPartsOnlyFromTheHost, "and it's no better hidden behind something else");

	unsigned char const variesFits[] = { kPartVaries, 3, 9, 9 };

	Check(Walk(variesFits, sizeof(variesFits)) == kNetPartsGood, "a part whose length varies is as long as it says");
	Check(Walk(variesFits, sizeof(variesFits) - 1) == kNetPartsTooShort, "and has to fit");

	unsigned char const variesThenMore[] = { kPartVaries, 2, 9, kPartEmpty };

	Check(Walk(variesThenMore, sizeof(variesThenMore)) == kNetPartsGood, "and the next part starts where it ends");

	unsigned char const variesNothing[] = { kPartVaries, 0 };
	unsigned char const variesMissing[] = { kPartVaries };
	unsigned char const variesFar[] = { kPartVaries, 200, 1 };

	unsigned char const variesThenWhatLooksFine[] = { kPartVaries, 0, 1, 2, 3 };

	Check(Walk(variesNothing, sizeof(variesNothing)) == kNetPartsTooShort, "saying it doesn't fit is taken at its word");
	Check(Walk(variesThenWhatLooksFine, sizeof(variesThenWhatLooksFine)) == kNetPartsTooShort, "even when what's after it would make a good message on its own");
	Check(Walk(variesMissing, sizeof(variesMissing)) == kNetPartsTooShort, "with no bytes at all it isn't even asked");
	Check(Walk(variesFar, sizeof(variesFar)) == kNetPartsTooShort, "and it can't claim more than there is");
}

static void TestChatLines()
{
	puts("chat lines");

	unsigned char line[kNetMostChatLetters + 8];

	memset(line, 'x', sizeof(line));
	line[3] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == 4, "a chat line is as long as its first zero");

	line[1] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == 2, "an empty one has the byte saying who it's for, and the zero");

	line[1] = 'x';
	line[3] = 'x';
	Check(NetChatPartLength(line, kNetMostChatLetters) == 0, "with no zero in what's there, it doesn't fit, however long it could have been");

	line[5] = 0;
	Check(NetChatPartLength(line, 5) == 0, "and a zero just past the end of the message doesn't count, because that isn't part of it");
	Check(NetChatPartLength(line, 6) == 6, "but one just inside it does");
	line[5] = 'x';

	line[kNetMostChatLetters + 1] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == kNetMostChatLetters + 2, "the longest there can be ends at the end of its letters");

	line[kNetMostChatLetters + 1] = 'x';
	line[kNetMostChatLetters + 2] = 0;
	Check(NetChatPartLength(line, sizeof(line)) == 0, "and one letter more doesn't fit");

	line[0] = 0;
	Check(NetChatPartLength(line, 1) == 0, "and a line that stops after the first byte has no zero to find");
}

static void TestNames()
{
	puts("player names");

	char from[13];
	char to[13];

	memset(from, 'n', sizeof(from));
	memset(to, 0, sizeof(to));
	NetCopyPlayerName(to, from, sizeof(to));
	Check(to[sizeof(to) - 1] == 0, "a name that never ends gets an end");
	Check(memcmp(to, from, sizeof(to) - 1) == 0, "and keeps everything before it");
}

static void TestAnswersInASecond()
{
	unsigned int started = 0;
	int used = 0;
	int allowed = 0;

	for (int i = 0; i < 25; i++)
	{
		allowed += NetThisSecondHasRoom(100, &started, &used, 10) ? 1 : 0;
	}

	Check(allowed == 10, "only so many answers in a second");
	Check(!NetThisSecondHasRoom(999, &started, &used, 10), "and still none just before the second is up");
	Check(NetThisSecondHasRoom(1100, &started, &used, 10), "and then there's room again");
	Check(started == 1100 && used == 1, "from a new second");
	started = 0xFFFFFFF0u;
	used = 10;
	Check(!NetThisSecondHasRoom(0xFFFFFFF8u, &started, &used, 10), "the clock's numbers near the top still work");
	Check(NetThisSecondHasRoom(1000u, &started, &used, 10), "and when they wrap round to the start");
}

static void TestDamageMultiples()
{
	Check(NetDamageMultipleIsGood(65536), "a hit counting once is good");
	Check(NetDamageMultipleIsGood(0), "and so is none at all");
	Check(NetDamageMultipleIsGood(kNetMostDamageMultiple), "and so is the most");
	Check(!NetDamageMultipleIsGood(kNetMostDamageMultiple + 1), "but not more");
	Check(!NetDamageMultipleIsGood(-1), "and no negative number, which would go round the damage loop about four billion times");
	Check(!NetDamageMultipleIsGood(-2147483647 - 1), "however large");
}

static void TestTypedAddresses()
{
	char const *const good[] = { "127.0.0.1", "192.168.1.20", "0.0.0.0", "255.255.255.255" };
	char const *const bad[] = { "", "localhost", "127.0.0", "127.0.0.1.5", "256.0.0.1", "1.2.3.4 ", " 1.2.3.4", "1..2.3", "1.2.3.", "1234.1.1.1", "0x7f.0.0.1", "1.2.3.4/8", "-1.2.3.4" };

	for (char const *address : good)
	{
		Check(NetTypedAddressIsGood(address, 17), address);
	}

	for (char const *address : bad)
	{
		Check(!NetTypedAddressIsGood(address, 17), address);
	}

	char noEnd[4] = { '1', '.', '2', '.' };

	Check(!NetTypedAddressIsGood(noEnd, sizeof(noEnd)), "one that runs off the end of its room isn't read past it");
	Check(!NetTypedAddressIsGood("1.2.3.4", 7), "and one that fills its room with no end isn't either");
}

static void TestWhatGoesOnTheScreen()
{
	char text[] = "one\ntwo\tthree\x7f four";

	NetCleanText(text, sizeof(text));
	Check(strcmp(text, "one two three  four") == 0, "control characters become spaces");

	char noEnd[4] = { 'a', 1, 'b', 2 };

	NetCleanText(noEnd, sizeof(noEnd));
	Check(noEnd[1] == ' ' && noEnd[3] == ' ', "text with no end is only read within its room");

	char high[] = { (char)0xC0, 'x', 0 };

	NetCleanText(high, sizeof(high));
	Check(high[0] == (char)0xC0, "a letter past plain English stays");

	char from[13];
	char to[13];

	memset(from, 3, sizeof(from));
	NetCopyPlayerName(to, from, sizeof(to));
	Check(to[0] == ' ' && to[12] == 0, "a name is cleaned as it's copied");

	auto const justOne = [](const char *candidate) { return strcmp(candidate, "BOB") == 0; };
	char name[13] = "BOB";

	Check(NetMakeNameUnique(name, sizeof(name), justOne, 9) && strcmp(name, "BOB2") == 0, "the second BOB gets a number");

	char other[13] = "ANN";

	Check(NetMakeNameUnique(other, sizeof(other), justOne, 9) && strcmp(other, "ANN") == 0, "a name nobody has stays as it is");

	auto const manyTaken = [](const char *candidate) { return strcmp(candidate, "BOB") == 0 || strcmp(candidate, "BOB2") == 0 || strcmp(candidate, "BOB3") == 0; };
	char third[13] = "BOB";

	Check(NetMakeNameUnique(third, sizeof(third), manyTaken, 9) && strcmp(third, "BOB4") == 0, "it takes the first number that's free");

	char full[13] = "ABCDEFGHIJKL";
	auto const fullTaken = [](const char *candidate) { return strcmp(candidate, "ABCDEFGHIJKL") == 0; };

	Check(NetMakeNameUnique(full, sizeof(full), fullTaken, 9) && strcmp(full, "ABCDEFGHIJK2") == 0, "a name that fills its room is cut to fit the number");

	auto const everything = [](const char *) { return true; };
	char none[13] = "BOB";

	Check(!NetMakeNameUnique(none, sizeof(none), everything, 9), "it says so if no number was free");
}

int main()
{
	TestWhatGoesOnTheScreen();
	TestTypedAddresses();
	TestDamageMultiples();
	TestAnswersInASecond();
	TestAmmunitionNumbers();
	TestTheLogCantBeFlooded();
	TestMessageParts();
	TestChatLines();
	TestNames();

	if (checksFailed)
	{
		printf("The network checks test FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("The network checks test passed");
	return 0;
}
