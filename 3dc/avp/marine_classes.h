// The marine classes a multiplayer game offers beyond stock's, and the sidearm that goes with a class. The classes and how
// they start are taken from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// There's no sniper class: the user wants the sniper rifle kept to single player for now.
//
// This file knows nothing about the game, so that tests/marine_classes_test.cpp can try all of it.

#pragma once

// A marine's class has a number. These are stock's two that the code below has to know, then the three new ones.
enum
{
	kClassGeneral = 0,
	kClassPistols = 8,
	kClassShotgun = 9,
	kClassRevolver = 10,
	kClassM79 = 11
};

// The class list in the menus is a single slider. 0 is a marine with no class, 1 a Predator and 2 an alien, and after them
// come the marine classes in order, starting at 3.
enum
{
	kClassSliderFirstClass = 3,
	kClassSliderFirstNew = 11,
	kClassSliderLast = 13
};

// What a class starts with, beside its sidearm. These guns count their spares as single rounds.
enum
{
	kClassShotgunStartRounds = 60,
	kClassRevolverStartRounds = 100,
	kClassM79StartEachKind = 15,
	kSidearmPistolStartMagazines = 10,
	kSidearmRevolverStartRounds = 50
};

// A marine's sidearm: what he carries beside his class's own gun.
enum
{
	kSidearmNone = -1,      // no sidearm, so he has the cudgel
	kSidearmPistol = 0,
	kSidearmRevolver = 1
};

// The marine class for a place on the slider. It only means something from kClassSliderFirstClass on.
inline int ClassSlider_ToClass(int slider)
{
	return slider - 2;
}

// The game's text keeps stock's class names in a row, with something else straight after them, so the new names are kept
// elsewhere. This says which new name a place on the slider wants: 0 for the first, and -1 if it's one of stock's.
inline int ClassSlider_NewNameIndex(int slider)
{
	if (slider < kClassSliderFirstNew || slider > kClassSliderLast)
	{
		return -1;
	}

	return slider - kClassSliderFirstNew;
}

// Which sidearm a player has.
//
// A marine with no class, or anyone else, has none. Nor does any class when the host has turned specialist pistols off.
// A pistols marine already has pistols, so he only gets the revolver, and only if the host allows it.
// Any other class has the pistol, unless the host allows the revolver and the player chose it.
//
// This is worked out on each player's own machine. It never goes over the network: others just see what's in his hands.
inline int Sidearm_Choose(bool isMarine, int marineClass, bool specialistPistols, bool revolverAllowed, int choiceInTheMenu)
{
	if (!isMarine || marineClass == kClassGeneral || !specialistPistols)
	{
		return kSidearmNone;
	}

	if (marineClass == kClassPistols)
	{
		return revolverAllowed ? kSidearmRevolver : kSidearmNone;
	}

	if (!revolverAllowed)
	{
		return kSidearmPistol;
	}

	return choiceInTheMenu == kSidearmRevolver ? kSidearmRevolver : kSidearmPistol;
}

// A setting from a settings file. A file can say anything, so a number the menu couldn't have chosen is ignored, and the
// setting stays as it was.
inline int Setting_FromFile(int fromFile, int lowest, int highest, int asItWas)
{
	if (fromFile < lowest || fromFile > highest)
	{
		return asItWas;
	}

	return fromFile;
}
