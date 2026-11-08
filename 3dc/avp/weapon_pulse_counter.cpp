// The two-digit counter on the pulse rifle's model. The game takes this from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new. Written 2026-11-06 by Melanikus for the AvP port.
//
// weapon_pulse_counter_sums.h says what the counter should say. This file puts that on the model.
//
// HOW IT'S DRAWN: nothing new. Each digit is a small patch of polygons on the gun's model, with an ordinary texture animation
// of ten frames, the digits 0 to 9, the same machinery a door panel's flicker uses. The animation is told not to play, and
// each frame this picks which frame each patch shows. So the renderer has nothing to do that it doesn't do already.
//
// A model without the patches just shows no counter. Stock's marwep.rif has the panel but no digits, so nothing happens with it.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "hmodel.h"
#include "weapons.h"
#include "weapon_hooks.h"
#include "weapon_pulse_counter.h"
#include "weapon_pulse_counter_sums.h"
#include "logString.h"

#include <stdio.h>

extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;
extern int NormalFrameTime;
extern int HUD_PrimaryRoundsShown(void);

// What the model calls the patches: the part of the gun they're on, and the id on each one's texture animation
static char kPulseCounterSection[] = "pulse_crossbk";
enum
{
	kPulseCounterTensId = 124,
	kPulseCounterUnitsId = 123
};

// The marks on the reload's move, in the model file
enum
{
	kPulseCounterMagazineOutMark = 1,
	kPulseCounterMagazineInMark = 2
};

static PulseCounterState Counter;

static void PulseCounter_AtPlayerStart(void)
{
	PulseCounter_Start(&Counter);
}


// Puts the digits on the patches. The patches are looked for every frame rather than kept, because the model is made again
// whenever a gun is brought up or a game's loaded, and a kept pointer could then point at something that's gone.
static void PulseCounter_ShowOnModel(int wholeRounds)
{

	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	SECTION_DATA *const section = GetThisSectionData(PlayersWeaponHModelController.section_data, kPulseCounterSection);

	if (!section || (section->flags & section_data_notreal))
	{
		return;
	}

	for (TXACTRLBLK *patch = section->tac_ptr; patch; patch = patch->tac_next)
	{
		if (!patch->tac_txah_s)
		{
			continue;
		}

		int digit;

		if (patch->tac_txah_s->txa_anim_id == kPulseCounterTensId)
		{
			digit = PulseCounter_Tens(wholeRounds);
		}
		else if (patch->tac_txah_s->txa_anim_id == kPulseCounterUnitsId)
		{
			digit = PulseCounter_Units(wholeRounds);
		}
		else
		{
			continue;
		}

		patch->tac_txah.txa_currentframe = PulseCounter_FrameForDigit(digit, patch->tac_txah.txa_numframes);
	}
}

// This only watches, so it always leaves the common code to look after the gun
static int PulseCounter_EveryFrame(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	TEMPLATE_WEAPON_DATA const *const twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];
	bool const reloading = weaponPtr->CurrentState == WEAPONSTATE_RELOAD_PRIMARY;
	int const marks = PlayersWeapon.HModelControlBlock ? PlayersWeaponHModelController.keyframe_flags : 0;

	int const roundsOnTheScreen = HUD_PrimaryRoundsShown();

	unsigned int const afterTheReload = PulseCounter_AfterTheReload(weaponPtr->PrimaryRoundsRemaining, weaponPtr->PrimaryLeftoverRounds,
		weaponPtr->PrimaryMagazinesRemaining, (unsigned int)TemplateAmmo[twPtr->PrimaryAmmoID].AmmoPerMagazine);

	PulseCounter_Update(&Counter, weaponPtr->PrimaryRoundsRemaining, roundsOnTheScreen, afterTheReload, reloading,
		(marks & kPulseCounterMagazineOutMark) != 0, (marks & kPulseCounterMagazineInMark) != 0, NormalFrameTime);

	PulseCounter_ShowOnModel(PulseCounter_WholeRounds(Counter.shown));

	return 0;
}

// Only what's needed. The counter has nothing to save, because it's worked out again from the rounds in the gun.
const WEAPON_HOOKS PulseCounter_Hooks =
{
	PulseCounter_AtPlayerStart,
	PulseCounter_EveryFrame
};
