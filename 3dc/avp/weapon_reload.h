// What a reload does with the rounds that were left in a stock gun, so that pressing the reload key part way through a magazine loses nothing. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT DOES: a stock gun counts its spares in whole magazines, and that's what the HUD shows. Press reload at 90 rounds of 99, and he opens a new magazine, takes the 9 he needs, and what's left of it, 90 rounds, goes in a small pile that nobody sees. The next reload takes from that pile before opening another magazine. A pile as big as a magazine becomes one.
//
// So no round is ever lost, the HUD still counts magazines, and from a gun that's empty with no pile it comes to just what the game always did: one magazine out, and the gun full.
//
// This file knows nothing about the game, so that tests/weapon_reload_test.cpp can try all of it.

#pragma once

enum
{
	kReloadMostMagazines = 99,
	kReloadMostInThePile = 65535
};

// Fills a gun. roundsInGun and fullMagazine are in the game's numbers, where 65536 is one round. pile is in whole rounds. All of these can come from a saved game, which anybody could have changed, so nothing here can go wrong with any number. Part of a round in the gun counts as a whole one, as the HUD shows it.
inline void Reload_FillGun(unsigned int *roundsInGun, unsigned short *pile, unsigned char *magazines, unsigned int fullMagazine)
{
	long long const full = fullMagazine >> 16;
	long long rounds = (((long long)*roundsInGun) + 65535) >> 16;

	if (full <= 0 || rounds >= full)
	{
		return;
	}

	long long heap = *pile;
	long long const needed = full - rounds;
	int spare = *magazines;

	if (heap < needed && spare > 0)
	{
		spare--;
		heap += full;
	}

	long long const taken = needed < heap ? needed : heap;

	rounds += taken;
	heap -= taken;

	while (heap >= full && spare < kReloadMostMagazines)
	{
		heap -= full;
		spare++;
	}

	*roundsInGun = (unsigned int)(rounds << 16);
	*pile = (unsigned short)(heap < kReloadMostInThePile ? heap : kReloadMostInThePile);
	*magazines = (unsigned char)spare;
}

// A pile read from a saved game, which is only a number. A real one is always less than a magazine, unless he has as many magazines as he can hold, so anything bigger than that holds is treated as none.
inline unsigned short Reload_PileFromASave(unsigned short saved, unsigned int fullMagazine, unsigned char magazines)
{
	unsigned int const full = fullMagazine >> 16;

	return (saved < full || (magazines >= kReloadMostMagazines && saved < full * 2)) ? saved : 0;
}
