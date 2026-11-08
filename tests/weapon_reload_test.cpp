// Tests for what a reload does with the rounds left in a stock gun, in 3dc/avp/weapon_reload.h. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT DOESN'T TEST: nothing here presses the key in the game. A gun=1 run does that.

#include "weapon_reload.h"

#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static const unsigned int kPulseMagazine = 99u << 16;

struct Gun
{
	unsigned int rounds;
	unsigned short pile;
	unsigned char magazines;
};

static Gun Filled(unsigned int wholeRounds, unsigned short pile, unsigned char magazines, unsigned int full = kPulseMagazine)
{
	Gun gun = { wholeRounds << 16, pile, magazines };

	Reload_FillGun(&gun.rounds, &gun.pile, &gun.magazines, full);

	return gun;
}

static long long Total(const Gun &gun, unsigned int full = kPulseMagazine)
{
	return (long long)(gun.rounds >> 16) + gun.pile + (long long)gun.magazines * (full >> 16);
}

static void TestAStockGunThatsEmpty()
{
	Gun const gun = Filled(0, 0, 5);

	Check(gun.rounds == kPulseMagazine, "an empty gun is full again");
	Check(gun.magazines == 4, "for one magazine");
	Check(gun.pile == 0, "with nothing over: just what the game always did");
}

static void TestPartWayThrough()
{
	Gun const gun = Filled(90, 0, 5);

	Check(gun.rounds == kPulseMagazine, "at 90 of 99 the gun is full again");
	Check(gun.magazines == 4, "from a new magazine");
	Check(gun.pile == 90, "and the 90 that weren't needed go in the pile");
	Check(Total(gun) == 90 + 5 * 99, "not one round is lost");

	Gun const next = Filled(80, gun.pile, gun.magazines);

	Check(next.rounds == kPulseMagazine, "the next reload fills it too");
	Check(next.magazines == 4, "without opening another magazine");
	Check(next.pile == 71, "taking from the pile");
	Check(Total(next) == Total(Gun{ 80u << 16, gun.pile, gun.magazines }), "and still not one round is lost");

	Gun const shortPile = Filled(10, 20, 3);

	Check(shortPile.rounds == kPulseMagazine && shortPile.magazines == 2, "a pile too small for what's needed opens a magazine");
	Check(shortPile.pile == 30, "and the rest joins it");
	Check(Total(shortPile) == 10 + 20 + 3 * 99, "still with nothing lost");
}

static void TestWithNoMagazinesLeft()
{
	Gun const gun = Filled(0, 40, 0);

	Check(gun.rounds == 40u << 16, "with no magazines, the pile is all he has");
	Check(gun.pile == 0 && gun.magazines == 0, "and it's used up");

	Gun const nothing = Filled(30, 0, 0);

	Check(nothing.rounds == 30u << 16 && nothing.pile == 0 && nothing.magazines == 0, "with nothing at all, nothing changes");
}

static void TestABigPile()
{
	Gun const gun = Filled(90, 95, 5);

	Check(gun.pile == 86 && gun.magazines == 5, "a pile that covers it opens nothing");

	Gun const over = Filled(0, 198, 5);

	Check(over.rounds == kPulseMagazine, "a pile bigger than a magazine fills the gun");
	Check(over.pile == 0 && over.magazines == 6, "and what's a whole magazine becomes one");

	Gun const atTheMost = Filled(0, 300, 99);

	Check(atTheMost.magazines == 99, "and never past the most he can carry");
	Check(atTheMost.pile == 201, "and the rest stays in the pile");
}

static void TestParts()
{
	Gun gun = { (50u << 16) + 1, 0, 2 };

	Reload_FillGun(&gun.rounds, &gun.pile, &gun.magazines, kPulseMagazine);
	Check(gun.pile == 51, "part of a round counts as one, as the HUD shows it, so 51 are in the gun and 48 are needed");

	Gun full = { kPulseMagazine, 7, 2 };

	Reload_FillGun(&full.rounds, &full.pile, &full.magazines, kPulseMagazine);
	Check(full.rounds == kPulseMagazine && full.pile == 7 && full.magazines == 2, "a full gun doesn't change");
}

static void TestHostileNumbers()
{
	Gun huge = { 0xFFFFFFFFu, 65535, 255 };

	Reload_FillGun(&huge.rounds, &huge.pile, &huge.magazines, kPulseMagazine);
	Check(huge.rounds == 0xFFFFFFFFu, "a gun that says it's fuller than full isn't touched");

	Gun none = { 0, 5, 5 };

	Reload_FillGun(&none.rounds, &none.pile, &none.magazines, 0);
	Check(none.rounds == 0 && none.pile == 5 && none.magazines == 5, "a magazine of nothing changes nothing, and doesn't loop");

	Gun most = { 0, 65535, 255 };

	Reload_FillGun(&most.rounds, &most.pile, &most.magazines, 1u << 16);
	Check(most.rounds == 1u << 16 && most.magazines == 255, "the biggest pile and count there are stay in their own numbers");

	Gun tiny = { 0, 65535, 0 };

	Reload_FillGun(&tiny.rounds, &tiny.pile, &tiny.magazines, 1u << 16);
	Check(tiny.magazines == kReloadMostMagazines && tiny.pile == 65535 - 1 - kReloadMostMagazines, "a huge pile can only make as many magazines as he can carry");

	Check(Reload_PileFromASave(90, kPulseMagazine, 5) == 90, "a pile from a save under a magazine is kept");
	Check(Reload_PileFromASave(99, kPulseMagazine, 5) == 0, "one of a whole magazine can't be real, with room for it to have become one");
	Check(Reload_PileFromASave(65535, kPulseMagazine, 5) == 0, "and nor can a huge one");
	Check(Reload_PileFromASave(150, kPulseMagazine, 99) == 150, "with as many magazines as he can carry a bigger one is real");
	Check(Reload_PileFromASave(65535, kPulseMagazine, 99) == 0, "up to a point");
	Check(Reload_PileFromASave(5, 0, 5) == 0, "and a gun with no magazine has no pile");
}

int main()
{
	TestAStockGunThatsEmpty();
	TestPartWayThrough();
	TestWithNoMagazinesLeft();
	TestABigPile();
	TestParts();
	TestHostileNumbers();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
