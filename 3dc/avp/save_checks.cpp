// Checks on what a save file says. Written 2026-11-04 by Melanikus for the AvP port.
//
// A save file is just a file, and anyone can edit one, or send one to a friend. So nothing in it can be believed. The game wrote these
// structures straight out of memory and reads them straight back in, and much of what's in them is used later as a position in a list,
// a number to divide by, or an address.
//
// The checks that apply in many places live here, so that each is written once and can be tested alone. The rule for each one is the same:
// a game the player really saved has to load exactly as it did, and anything else has to do no harm.

#include "3dc.h"
#include "dynblock.h"
#include "gamedef.h"
#include "env_info.h"
#include "equipment.h"
#include "vision.h"
#include "save_checks.h"

// The slot's numbers are read when the menu lists the saves, long before the file is loaded. They pick the level's name from a table, a line
// of text, and the row of every damage table in the game.
bool Save_SlotNumbersAreValid(int species, int episode, int difficulty)
{
	int episodes = 0;

	switch (species)
	{
		case I_Marine:
			episodes = MAX_NO_OF_MARINE_EPISODES;
			break;

		case I_Predator:
			episodes = MAX_NO_OF_PREDATOR_EPISODES;
			break;

		case I_Alien:
			episodes = MAX_NO_OF_ALIEN_EPISODES;
			break;

		default:
			return false;
	}

	return episode >= 0 && episode < episodes && difficulty >= 0 && difficulty < I_MaxDifficulties;
}

// These four numbers pick an entry in a table the game then reads without asking: the player's weapon slots, the rate each weapon state
// runs at, the colour of every particle in each vision mode, and the name of each kind of ammunition.
bool Save_WeaponSlotIsValid(int slot)
{
	return slot >= 0 && slot < MAX_NO_OF_WEAPON_SLOTS;
}

bool Save_WeaponStateIsValid(int state)
{
	return state >= 0 && state < MAX_NO_OF_WEAPON_STATES;
}

bool Save_VisionModeIsValid(int mode)
{
	return mode >= 0 && mode < NUMBER_OF_VISION_MODES;
}

bool Save_AmmoIsValid(int ammo)
{
	return ammo >= 0 && ammo < MAX_NO_OF_AMMO_TEMPLATES;
}

// Many saved numbers say which entry of a list something is using, with -1 for none. A number outside the list means none as well, because
// there's nothing better to do with it, and an object that has lost track of something carries on as usual.
int Save_PlaceInListOrNone(int place, int count)
{
	return (place >= 0 && place < count) ? place : -1;
}

bool Save_NameFillsBlock(int blockSize, int fixedSize, const int *nameLength, const char *name)
{
	if (blockSize < fixedSize)
	{
		return false;
	}

	int const length = *nameLength;

	return length >= 1 && length == blockSize - fixedSize && name[length - 1] == '\0';
}

// A morph's frame number holds which frame it's on in its top half, which picks an entry in the morph's list of frames, and how far on it is in
// its bottom half. maxFrames is the end of the last frame, so a real frame is always less than that.
int Save_MorphFrameOrOwn(int saved, int own, int maxFrames)
{
	return (saved >= 0 && saved < maxFrames) ? saved : own;
}

// A texture animation's list is one number the game uses as working space, then one entry for each sequence, then an empty entry to mark the
// end (SetupAnimOnTriangle in chunk_load.cpp). The game writes through the entry it finds, so a sequence past the end would have it write
// through whatever happens to be there. The list only ever gets as far as the empty entry, so walking along it is always safe.
bool Save_TextureSequenceIsValid(const int *animationList, int sequence)
{
	if (!animationList || sequence < 0)
	{
		return false;
	}

	void *const *entries = (void *const *)animationList + 1;

	for (int i = 0; i <= sequence; i++)
	{
		if (!entries[i])
		{
			return false;
		}
	}

	return true;
}

// A dynamics block says where an object is and how it moves. Nearly all of it is only numbers, and strange numbers just give a strange
// object. These are the parts that can do real harm:
//
// CollisionReportPtr is an address. The game writes 0 here when it saves, so 0 is what a real save holds, and any other number is an address
// someone chose, which the game would read from and write through.
//
// Mass gets divided by, so it can't be 0. Every kind of object has a mass of at least 1, so a mass of 0 or less can't be real, and the
// object keeps the one it was made with.
//
// DynamicsType and ToppleForce pick what the collision code does. A number that isn't one of the choices keeps the object's own.
//
// An angle is looked up in the sine and cosine tables, which have 4096 places, and not every lookup keeps it in range. An angle goes round
// in steps of 4096, so taking it to that range is still the same angle.
void Save_RestoreDynamics(DYNAMICSBLOCK *live, const DYNAMICSBLOCK *saved)
{
	if (!live)
	{
		return;
	}

	int const ownMass = live->Mass;
	enum DYN_TYPE const ownType = live->DynamicsType;
	enum TOPPLE_FORCE const ownTopple = live->ToppleForce;

	*live = *saved;

	live->CollisionReportPtr = 0;

	if (live->Mass <= 0)
	{
		live->Mass = ownMass;
	}

	if ((int)live->DynamicsType < DYN_TYPE_NO_COLLISIONS || (int)live->DynamicsType > DYN_TYPE_CUBOID_COLLISIONS)
	{
		live->DynamicsType = ownType;
	}

	if ((int)live->ToppleForce < TOPPLE_FORCE_NONE || (int)live->ToppleForce > TOPPLE_FORCE_FULL)
	{
		live->ToppleForce = ownTopple;
	}

	live->OrientEuler.EulerX &= wrap360;
	live->OrientEuler.EulerY &= wrap360;
	live->OrientEuler.EulerZ &= wrap360;
	live->PrevOrientEuler.EulerX &= wrap360;
	live->PrevOrientEuler.EulerY &= wrap360;
	live->PrevOrientEuler.EulerZ &= wrap360;
}
