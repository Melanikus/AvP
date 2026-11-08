// What the common weapon code offers the guns that have files of their own. See weapon_hooks.h. Written 2026-11-06 by Melanikus for the AvP port.

// Modified 2026-11-07 by Melanikus for the AvP port:
//   a gun's own state can be kept from one level of a co-op campaign to the next. Credit: Melanikus and Bobblen (the kept kit follows Bobblen's avp-forever, rewritten)

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "savegame.h"
#include "weapon_hooks.h"
#include "hmodel.h"
#include "dynblock.h"
#include "bh_debris.h"

#include <string.h>

extern SECTION * GetNamedHierarchyFromLibrary(const char * rif_name, const char * hier_name);
extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;

static bool Weapon_HasAModelOfItsOwn(enum WEAPON_ID weapon)
{
	switch (weapon)
	{
		case WEAPON_AUTOSHOTGUN:
		case WEAPON_SONICCANNON:
		case WEAPON_BEAMCANNON:
		case WEAPON_MYSTERYGUN:
		case WEAPON_PRED_STAFF:
			return true;

		default:
			return false;
	}
}

bool Weapon_CanBeDrawn(enum WEAPON_ID weapon)
{
	if (!Weapon_HasAModelOfItsOwn(weapon))
	{
		return true;
	}

	TEMPLATE_WEAPON_DATA const *const row = &TemplateWeapon[weapon];

	// A gun whose row isn't filled in yet has no model to ask about
	return row->RiffName && row->HierarchyName && GetNamedHierarchyFromLibrary(row->RiffName, row->HierarchyName) != 0;
}

// The MelPyth mod's own versions of four of the game's marine guns: the pulse rifle with its ammo counter, the cudgel, and the
// one and two pistols with the new right-hand pistol. They're in a file of our own, avp_huds\MelPyth_MarWep.rif (loaded in
// npcsetup.cpp), so the player's own marwep.rif is never changed. These are the two files' internal names, which is how the
// game finds a loaded file.
static char kOwnMarineGunsFile[] = "MelPyth_MarWep";
static const char kGameMarineGunsFile[] = "MarineWeapons";

char *Weapon_ModelFileFor(TEMPLATE_WEAPON_DATA const *row)
{
	// A gun the game draws from marwep.rif comes from our file instead, but only when our file is loaded and has that gun in it.
	// Without our file, every gun comes from the file its row names, just as the game always did.
	if (row->RiffName && row->HierarchyName && _stricmp(row->RiffName, kGameMarineGunsFile) == 0 &&
		GetNamedHierarchyFromLibrary(kOwnMarineGunsFile, row->HierarchyName) != 0)
	{
		return kOwnMarineGunsFile;
	}

	return row->RiffName;
}

enum WEAPON_ID Weapon_ForSlot(enum WEAPON_ID weapon)
{
	return (weapon != NULL_WEAPON && !Weapon_CanBeDrawn(weapon)) ? NULL_WEAPON : weapon;
}

void Weapon_EnterState(PLAYER_WEAPON_DATA *weaponPtr, int state)
{
	weaponPtr->CurrentState = (enum WEAPON_STATE)state;
	weaponPtr->StateTimeOutCounter = WEAPONSTATE_INITIALTIMEOUTCOUNT;
}

void Weapon_TellOfNewState(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	static PLAYER_WEAPON_DATA *lastWeapon = NULL;
	static int lastState = -1;

	TEMPLATE_WEAPON_DATA const *const row = &TemplateWeapon[weaponPtr->WeaponIDNumber];

	if (weaponPtr != lastWeapon || (int)weaponPtr->CurrentState != lastState)
	{
		if (row->Hooks && row->Hooks->StateBegan)
		{
			// It may choose to go straight on to another state, and this remembers that one
			row->Hooks->StateBegan(playerStatus, weaponPtr);
		}

		lastWeapon = weaponPtr;
		lastState = (int)weaponPtr->CurrentState;
	}
}

void Weapon_ThrowCase(const char *shapeName, const char *sectionName)
{
	if (!PlayersWeapon.HModelControlBlock || !PlayersWeaponHModelController.section_data)
	{
		return;
	}

	SECTION_DATA *const eject = GetThisSectionData(PlayersWeaponHModelController.section_data, const_cast<char *>(sectionName));

	if (!eject)
	{
		return;
	}

	// In the part's own directions, as a push out of the side of the gun
	VECTORCH impulse;

	impulse.vx = -(FastRandom() & 1000);
	impulse.vy = -(FastRandom() & 2000);
	impulse.vz = (FastRandom() & 2000) + 6000;

	RotateVector(&impulse, &eject->SecMat);

	// It's moving as fast as he is when it comes out, and half of that looks right
	VECTORCH const &moving = Player->ObStrategyBlock->DynPtr->LinVelocity;

	impulse.vx += moving.vx / 2;
	impulse.vy += moving.vy / 2;
	impulse.vz += moving.vz / 2;

	MakeThrownCase(const_cast<char *>(shapeName), &eject->World_Offset, &eject->SecMat, &impulse);
}

void Guns_AtPlayerStart(void)
{
	for (int gun = 0; gun < MAX_NO_OF_WEAPON_TEMPLATES; gun++)
	{
		const WEAPON_HOOKS *const hooks = TemplateWeapon[gun].Hooks;

		if (hooks && hooks->AtPlayerStart)
		{
			hooks->AtPlayerStart();
		}
	}
}

// One gun's block in a save. The gun's own state comes straight after it.
typedef struct
{
	SAVE_BLOCK_HEADER header;
	int gun;
} GUN_OWN_STATE_SAVE_BLOCK;

// No gun's own state should be anything like this big. It's the most that's read back.
enum
{
	kMostOwnStateBytes = 512
};

void Guns_SaveOwnStates(void)
{
	for (int gun = 0; gun < MAX_NO_OF_WEAPON_TEMPLATES; gun++)
	{
		const WEAPON_HOOKS *const hooks = TemplateWeapon[gun].Hooks;

		if (!hooks || !hooks->OwnState || hooks->OwnStateSize <= 0 || hooks->OwnStateSize > kMostOwnStateBytes)
		{
			continue;
		}

		int const size = (int)sizeof(GUN_OWN_STATE_SAVE_BLOCK) + hooks->OwnStateSize;
		GUN_OWN_STATE_SAVE_BLOCK *const block = (GUN_OWN_STATE_SAVE_BLOCK *)GetPointerForSaveBlock(size);

		block->header.type = SaveBlock_GunOwnState;
		block->header.size = size;
		block->gun = gun;
		memcpy(block + 1, hooks->OwnState, hooks->OwnStateSize);
	}
}

// What each gun held when a level of a co-op campaign ended. These are the game's own numbers, so nothing here needs checking as a save does.
static unsigned char keptOwnStates[MAX_NO_OF_WEAPON_TEMPLATES][kMostOwnStateBytes];

void Guns_KeepOwnStates(void)
{
	for (int gun = 0; gun < MAX_NO_OF_WEAPON_TEMPLATES; gun++)
	{
		const WEAPON_HOOKS *const hooks = TemplateWeapon[gun].Hooks;

		if (hooks && hooks->OwnState && hooks->OwnStateSize > 0 && hooks->OwnStateSize <= kMostOwnStateBytes)
		{
			memcpy(keptOwnStates[gun], hooks->OwnState, hooks->OwnStateSize);
		}
	}
}

void Guns_GiveBackOwnStates(void)
{
	for (int gun = 0; gun < MAX_NO_OF_WEAPON_TEMPLATES; gun++)
	{
		const WEAPON_HOOKS *const hooks = TemplateWeapon[gun].Hooks;

		if (hooks && hooks->OwnState && hooks->OwnStateSize > 0 && hooks->OwnStateSize <= kMostOwnStateBytes)
		{
			memcpy(hooks->OwnState, keptOwnStates[gun], hooks->OwnStateSize);
		}
	}
}

void Guns_LoadOwnStates(void)
{
	SAVE_BLOCK_HEADER *header;

	// GetNextBlockIfOfType has checked that the whole block is inside the file
	while ((header = GetNextBlockIfOfType(SaveBlock_GunOwnState)) != NULL)
	{
		if (header->size < (int)sizeof(GUN_OWN_STATE_SAVE_BLOCK))
		{
			continue;
		}

		GUN_OWN_STATE_SAVE_BLOCK const *const block = (GUN_OWN_STATE_SAVE_BLOCK const *)header;

		// The file says which gun, and it's only a number
		if (block->gun < 0 || block->gun >= MAX_NO_OF_WEAPON_TEMPLATES)
		{
			continue;
		}

		const WEAPON_HOOKS *const hooks = TemplateWeapon[block->gun].Hooks;
		int const bytes = header->size - (int)sizeof(GUN_OWN_STATE_SAVE_BLOCK);

		if (!hooks || !hooks->OwnState || !hooks->OwnStateIsGood || bytes != hooks->OwnStateSize || bytes > kMostOwnStateBytes)
		{
			continue;
		}

		// It's looked at in a copy, so that a bad one never reaches the gun. The double makes the copy line up for any numbers a gun holds.
		union
		{
			double lineUp;
			char bytes[kMostOwnStateBytes];
		} copy;

		memcpy(copy.bytes, block + 1, bytes);

		if (hooks->OwnStateIsGood(copy.bytes))
		{
			memcpy(hooks->OwnState, copy.bytes, bytes);
		}
	}
}
