// How a map asks for a pickup of one of the new guns or its ammunition. Written 2026-11-06 by Melanikus for the AvP port.
// The way of doing it comes from MelPyth M5 by Mupp, so that maps made for that mod still work.
//
// THE GAME'S OWN PICKUPS are a type and a number stored with the object in the map. The map tool's list of those doesn't
// have the new guns, so a map asks for one in the object's name instead. The object's type is set to "data tape", which
// nothing else in the game uses, and its name starts with one of the words below. Anything may come after the word,
// because a map's objects need names different from each other: AM79, AM7901 and AM79_B are all the same pickup.
//
// Capitals matter, as they did in the mod. Only objects of the data tape type are ever looked at, so a wall that happens
// to be called WM79_WALL stays a wall.
//
// This file knows nothing about the game, so that tests/weapon_pickups_test.cpp can try all of it.

#pragma once

#include <string.h>

enum PlacedPickup
{
	kPlacedPickup_None,
	kPlacedPickup_M79,
	kPlacedPickup_SniperRifle,
	kPlacedPickup_Revolver,
	kPlacedPickup_M79Grenades,
	kPlacedPickup_M79FireGrenades,
	kPlacedPickup_M79FragGrenades,
	kPlacedPickup_SniperRounds,
	kPlacedPickup_RevolverRounds
};

// Does this name start with this word?
inline bool PlacedPickup_NameStartsWith(const char *name, const char *word)
{
	return strncmp(name, word, strlen(word)) == 0;
}

// What an object's name asks for. A map is a file from outside the game, so the name may be anything at all, or missing.
inline PlacedPickup PlacedPickup_FromName(const char *name)
{
	if (!name)
	{
		return kPlacedPickup_None;
	}

	// The longer words come first, because AM79FIRE also starts with AM79
	if (PlacedPickup_NameStartsWith(name, "WM79")) return kPlacedPickup_M79;
	if (PlacedPickup_NameStartsWith(name, "WSNIPER")) return kPlacedPickup_SniperRifle;
	if (PlacedPickup_NameStartsWith(name, "WREVOLVER")) return kPlacedPickup_Revolver;
	if (PlacedPickup_NameStartsWith(name, "AM79FIRE")) return kPlacedPickup_M79FireGrenades;
	if (PlacedPickup_NameStartsWith(name, "AM79FRAG")) return kPlacedPickup_M79FragGrenades;
	if (PlacedPickup_NameStartsWith(name, "AM79")) return kPlacedPickup_M79Grenades;
	if (PlacedPickup_NameStartsWith(name, "ASNIPER")) return kPlacedPickup_SniperRounds;
	if (PlacedPickup_NameStartsWith(name, "AREVOLVER")) return kPlacedPickup_RevolverRounds;

	return kPlacedPickup_None;
}
