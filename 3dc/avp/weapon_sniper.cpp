// The sniper rifle. The game takes this from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new. Written 2026-11-06 by Melanikus for the AvP port.
// Modified 2026-11-07 by Melanikus for the AvP port:
//   declarations written so the security scanner can read this file (CODING-STANDARD.md, rule 1)
//
// weapon_sniper_sums.h says how the rifle is used, how far it zooms and what follows what. This file joins that to the game:
// it turns those steps into the weapon states the common code runs, and plays the model's moves.
//
// THE ZOOM goes through CameraZoomScale, the same number the Predator's zoom uses, so the world, the shadows and the
// per-pixel lights all follow it with nothing changed. While the scope's up the gun and its crosshair aren't drawn, and the
// scope's picture is drawn flat over the screen.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "hmodel.h"
#include "sequences.h"
#include "weapons.h"
#include "psnd.h"
#include "weapon_hooks.h"
#include "weapon_scope.h"
#include "weapon_sniper.h"
#include "weapon_sniper_sums.h"

#include <string.h>

extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;
extern float CameraZoomScale;
extern int playerNoise;
extern int NormalFrameTime;
extern int FireNonAutomaticWeapon(PLAYER_WEAPON_DATA *weaponPtr);

// The moves on the rifle's model that have no name of their own. The scope's two are filed under the Predator's kind of move.
enum
{
	kSniperScopeMoveKind = HMSQT_PredatorHUD,
	kSniperScopeRisingMove = 10,
	kSniperScopeLoweringMove = 6
};

static SniperRifleState Rifle;

// Whether secondary fire was held last frame, because a press and a hold do different things
static int SecondaryWasHeld;

// Plays one of the rifle's moves, unless it's playing already. There may be no model, or the file may not have this move, and then it's left out.
static void Sniper_Play(int kind, int move, int time)
{
	if (!PlayersWeapon.HModelControlBlock
		|| (PlayersWeaponHModelController.Sequence_Type == kind && PlayersWeaponHModelController.Sub_Sequence == move)
		|| !HModelSequence_Exists(&PlayersWeaponHModelController, kind, move))
	{
		return;
	}

	InitHModelSequence(&PlayersWeaponHModelController, kind, move, time);
	PlayersWeaponHModelController.Looped = 0;
}

// Each shot starts this move again from the beginning, even straight after the last one
void Sniper_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT
		&& PlayersWeapon.HModelControlBlock
		&& HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Fire))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Fire, ONE_FIXED / 2);
		PlayersWeaponHModelController.Looped = 0;
	}
}

void Sniper_Reload(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_Play(HMSQT_MarineHUD, (int)MHSS_Standard_Reload, ONE_FIXED);
}

void Sniper_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_Play(HMSQT_MarineHUD, (int)MHSS_Come, ONE_FIXED * 7 / 10);
}

void Sniper_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_Play(HMSQT_MarineHUD, (int)MHSS_Go, ONE_FIXED / 2);
}

void Sniper_Bolt(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_Play(HMSQT_MarineHUD, (int)MHSS_Tertiary_Fire, ONE_FIXED * 7 / 10);

	// The move marks the moment the case comes out. On this model the part it comes from has the same name as the case.
	if (Rifle.spentCaseToThrow && PlayersWeapon.HModelControlBlock && (PlayersWeaponHModelController.keyframe_flags & 1))
	{
		Rifle.spentCaseToThrow = false;
		Weapon_ThrowCase("SniperRifleEjectionCase", "SniperRifleEjectionCase");
	}
}

void Sniper_ScopeRising(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_Play(kSniperScopeMoveKind, kSniperScopeRisingMove, ONE_FIXED / 10);
}

void Sniper_ScopeLowering(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_Play(kSniperScopeMoveKind, kSniperScopeLoweringMove, ONE_FIXED / 10);
}

// A shot in the head gets a sound of its own, unless it's something that doesn't count: the Queen, a piece of something that's already
// been blown up, or a body on the network. It doesn't change the damage.
static void Sniper_SoundIfAimingAtAHead(void)
{
	DISPLAYBLOCK const *const target = PlayersTarget.DispPtr;
	SECTION_DATA const *const section = PlayersTarget.HModelSection;

	if (!target || !target->ObStrategyBlock || !section || !section->sempai || !section->sempai->Section_Name)
	{
		return;
	}

	int const kind = (int)target->ObStrategyBlock->I_SBtype;

	if (kind == I_BehaviourQueenAlien || kind == I_BehaviourHierarchicalFragment || kind == I_BehaviourNetCorpse)
	{
		return;
	}

	if (!strcmp(section->sempai->Section_Name, "head") || !strcmp(section->sempai->Section_Name, "helm"))
	{
		Sound_Play(SID_WEAPON_HEADSHOT, "h");
	}
}

int FireSniperRifle(PLAYER_WEAPON_DATA *weaponPtr)
{
	Sniper_SoundIfAimingAtAHead();

	Sound_Play(SID_WEAPON_SNIPER_FIRE, "h");
	playerNoise = 1;

	return FireNonAutomaticWeapon(weaponPtr);
}

// Which step of the rifle's a weapon state is. Everything else counts as idle here.
static enum SniperStep Sniper_StepOfState(int state)
{
	switch (state)
	{
		case kSniperState_Bolt:
			return kSniperWorkingBolt;

		case kSniperState_ScopeRising:
			return kSniperScopeRising;

		case kSniperState_ScopeLowering:
			return kSniperScopeLowering;

		case kSniperState_WaitingForTrigger:
			return kSniperWaitingForTrigger;

		case kSniperState_ZoomingIn:
			return kSniperZoomingIn;

		case kSniperState_ZoomingBack:
			return kSniperZoomingBack;

		case kSniperState_ZoomingOut:
			return kSniperZoomingOut;

		default:
			return kSniperIdle;
	}
}

static void Sniper_GoTo(PLAYER_WEAPON_DATA *weaponPtr, enum SniperStep step)
{
	switch (step)
	{
		case kSniperIdle:
			weaponPtr->CurrentState = WEAPONSTATE_IDLE;
			weaponPtr->StateTimeOutCounter = 0;
			break;

		case kSniperWorkingBolt:
			Weapon_EnterState(weaponPtr, kSniperState_Bolt);
			break;

		case kSniperScopeRising:
			Weapon_EnterState(weaponPtr, kSniperState_ScopeRising);
			break;

		case kSniperScopeLowering:
			Weapon_EnterState(weaponPtr, kSniperState_ScopeLowering);
			break;

		case kSniperWaitingForTrigger:
			Weapon_EnterState(weaponPtr, kSniperState_WaitingForTrigger);
			break;

		case kSniperZoomingIn:
			Weapon_EnterState(weaponPtr, kSniperState_ZoomingIn);
			break;

		case kSniperZoomingBack:
			Weapon_EnterState(weaponPtr, kSniperState_ZoomingBack);
			break;

		case kSniperZoomingOut:
			Weapon_EnterState(weaponPtr, kSniperState_ZoomingOut);
			break;

		case kSniperReloading:
			Weapon_EnterState(weaponPtr, WEAPONSTATE_RELOAD_PRIMARY);
			break;

		case kSniperPuttingAway:
			Weapon_EnterState(weaponPtr, WEAPONSTATE_UNREADYING);
			break;
	}
}

static void Sniper_AtPlayerStart(void)
{
	SniperRifle_Start(&Rifle);
	SecondaryWasHeld = 0;

	// He may have died looking through the scope
	CameraZoomScale = 1.0f;
}

static int Sniper_EveryFrame(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS *const playerStatusPtr = (PLAYER_STATUS *)playerStatus;
	TEMPLATE_WEAPON_DATA const *const twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];
	const SniperStep step = Sniper_StepOfState((int)weaponPtr->CurrentState);

	int const secondaryHeld = playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon;
	bool const secondaryJustPressed = secondaryHeld && !SecondaryWasHeld;

	SecondaryWasHeld = secondaryHeld;

	Sniper_MoveZoom(&Rifle, step, NormalFrameTime);
	CameraZoomScale = SniperZoom_ViewScale(Rifle.zoom);

	// The further it's zoomed, the slower he goes. With no zoom this leaves his speed alone, so firing's own slowdown still works.
	if (Rifle.zoom > kSniperLeastZoom)
	{
		playerStatusPtr->Encumberance.MovementMultiple = SniperZoom_Slowed(twPtr->Encum_Idle.MovementMultiple, kSniperFullZoomMoving, Rifle.zoom);
		playerStatusPtr->Encumberance.TurningMultiple = SniperZoom_Slowed(twPtr->Encum_Idle.TurningMultiple, kSniperFullZoomTurning, Rifle.zoom);
		playerStatusPtr->Encumberance.JumpingMultiple = SniperZoom_Slowed(twPtr->Encum_Idle.JumpingMultiple, kSniperFullZoomJumping, Rifle.zoom);
	}

	if (weaponPtr->CurrentState == WEAPONSTATE_IDLE)
	{
		bool const reloadIsDue = weaponPtr->PrimaryRoundsRemaining == 0 && weaponPtr->PrimaryMagazinesRemaining != 0;
		const SniperStep next = Sniper_WhenIdle(&Rifle, secondaryJustPressed, reloadIsDue);

		if (next != kSniperIdle)
		{
			Sniper_GoTo(weaponPtr, next);
			return 1;
		}

		// Nothing of ours to do, so the common code looks after the trigger, changing weapon and reloading
		return 0;
	}

	// The common code has nothing to do for a state of the rifle's own
	return (step != kSniperIdle) ? 1 : 0;
}

// The common code has asked for something that the scope has to come down for first
static void Sniper_StateBegan(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	switch ((int)weaponPtr->CurrentState)
	{
		case WEAPONSTATE_RELOAD_PRIMARY:
		{
			const SniperStep next = Sniper_BeforeReload(&Rifle, weaponPtr->PrimaryRoundsRemaining != 0);

			if (next != kSniperReloading)
			{
				Sniper_GoTo(weaponPtr, next);
			}
			break;
		}

		case WEAPONSTATE_UNREADYING:
		{
			const SniperStep next = Sniper_BeforePuttingAway(&Rifle);

			if (next != kSniperPuttingAway)
			{
				Sniper_GoTo(weaponPtr, next);
			}
			break;
		}

		case WEAPONSTATE_READYING:
		{
			const SniperStep next = Sniper_WhenDrawn(&Rifle);

			if (next != kSniperIdle)
			{
				Sniper_GoTo(weaponPtr, next);
			}
			break;
		}

		default:
			break;
	}
}

static int Sniper_MayLeaveState(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS const *const playerStatusPtr = (PLAYER_STATUS const *)playerStatus;

	bool const primaryHeld = playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon != 0;
	bool const secondaryHeld = playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon != 0;

	return SniperStep_MayEnd(&Rifle, Sniper_StepOfState((int)weaponPtr->CurrentState), primaryHeld, secondaryHeld) ? 1 : 0;
}

static int Sniper_StateEnded(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS const *const playerStatusPtr = (PLAYER_STATUS const *)playerStatus;

	switch ((int)weaponPtr->CurrentState)
	{
		case WEAPONSTATE_RECOIL_PRIMARY:
			Sniper_GoTo(weaponPtr, Sniper_AfterRecoil(&Rifle));
			return 1;

		case WEAPONSTATE_RELOAD_PRIMARY:
		{
			// It fills up from his spares, which this rifle counts as single rounds
			int const taken = Sniper_RoundsToLoad(weaponPtr->PrimaryRoundsRemaining,
				(unsigned int)TemplateAmmo[TemplateWeapon[weaponPtr->WeaponIDNumber].PrimaryAmmoID].AmmoPerMagazine, weaponPtr->PrimaryMagazinesRemaining);

			weaponPtr->PrimaryRoundsRemaining += taken << 16;
			weaponPtr->PrimaryMagazinesRemaining -= (unsigned char)taken;

			bool const changingWeapon = playerStatusPtr->SwapToWeaponSlot != WEAPON_FINISHED_SWAPPING
				&& playerStatusPtr->SwapToWeaponSlot != playerStatusPtr->SelectedWeaponSlot;

			Sniper_GoTo(weaponPtr, Sniper_AfterReload(&Rifle, changingWeapon));
			return 1;
		}

		case kSniperState_Bolt:
		case kSniperState_ZoomingBack:
			Sniper_GoTo(weaponPtr, kSniperIdle);
			return 1;

		case kSniperState_ScopeRising:
			Sniper_GoTo(weaponPtr, Sniper_AfterScopeRose(&Rifle, playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon != 0));
			return 1;

		case kSniperState_ScopeLowering:
			Sniper_GoTo(weaponPtr, Sniper_AfterScopeLowered(&Rifle));
			return 1;

		case kSniperState_WaitingForTrigger:
			Sniper_GoTo(weaponPtr, Sniper_AfterWaitingForTrigger(&Rifle));
			return 1;

		case kSniperState_ZoomingIn:
			Sniper_GoTo(weaponPtr, Sniper_AfterZoomingIn(&Rifle));
			return 1;

		case kSniperState_ZoomingOut:
			Sniper_GoTo(weaponPtr, Sniper_AfterZoomingOut(&Rifle));
			return 1;

		default:
			return 0;
	}
}

// The gun isn't drawn while looking through the scope, and the common code leaves the crosshair out too
static int Sniper_DrawGun(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	return Rifle.scopeIsUp ? 0 : 1;
}

static void Sniper_DrawOverHud(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (Rifle.scopeIsUp)
	{
		WeaponScope_Draw();
	}
}

static int Sniper_OwnStateIsGood(const void *state)
{
	return SniperRifle_StateIsGood((SniperRifleState const *)state) ? 1 : 0;
}

const WEAPON_HOOKS Sniper_Hooks =
{
	Sniper_AtPlayerStart,
	Sniper_EveryFrame,
	Sniper_StateBegan,
	Sniper_MayLeaveState,
	Sniper_StateEnded,
	NULL, /* MayTakeAmmo */
	Sniper_DrawGun,
	Sniper_DrawOverHud,
	&Rifle,
	sizeof(Rifle),
	Sniper_OwnStateIsGood,
	1 // SparesAreSingleRounds
};
