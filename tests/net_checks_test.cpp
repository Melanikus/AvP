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

int main()
{
	TestAmmunitionNumbers();
	TestTheLogCantBeFlooded();

	if (checksFailed)
	{
		printf("The network checks test FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("The network checks test passed");
	return 0;
}
