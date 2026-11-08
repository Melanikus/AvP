// The marine's shotgun. The game takes it from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new. Written 2026-11-06 by Melanikus for the AvP port.
//
// The sums are in shotgun.h, which says what the gun does. This file joins them to the game.
//
// LOADING IT: he lifts the gun, puts shells in one at a time, and lowers it again. If it was empty when he started, he then works the pump, to put a shell in the chamber. The spare shells are counted in the slot's "magazines", one to a shell.
//
// THE STOCK: secondary fire opens it (a second), then the view zooms in. Pressing it again zooms out, then closes it (a second). Loading the gun or putting it away zooms out first, and putting it away closes the stock too. M5's model file has no move of its own for the stock, so for now the gun doesn't change on the screen: only the view does. If a model file ever has that move, as sub-sequence kShotgunStockMove, it plays with no other change here.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "hmodel.h"
#include "sequences.h"
#include "view.h"
#include "los.h"
#include "weapons.h"
#include "game_statistics.h"
#include "weapon_hooks.h"
#include "weapon_shotgun.h"
#include "huddefs.h"
#include "language.h"

extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;
extern VECTORCH GunMuzzleDirectionInVS;
extern int AccuracyStats_TargetFilter(STRATEGYBLOCK *sbPtr);
extern void PulseRifleFidget(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);
extern float CameraZoomScale;
extern int NormalFrameTime;

static ShotgunOwnState Own;

// Plays one of the gun's moves on its model, unless it's playing already. There may be no model, or the file may not have this move, and then nothing happens and the gun still works.
static void Shotgun_Play(enum MarineHUDSubSequences move, int time, int looped)
{
	if (!PlayersWeapon.HModelControlBlock
		|| PlayersWeaponHModelController.Sub_Sequence == (int)move
		|| !HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)move))
	{
		return;
	}

	InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)move, time);
	PlayersWeaponHModelController.Looped = looped;
}

void Shotgun_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (PlayersWeapon.HModelControlBlock)
	{
		PulseRifleFidget(playerStatus, weaponPtr);
	}
}

// Each shot starts this move again from the beginning, even straight after the last one
void Shotgun_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT
		&& PlayersWeapon.HModelControlBlock
		&& HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Fire))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Fire, ONE_FIXED);
		PlayersWeaponHModelController.Looped = 0;
	}

	// The move has a mark in it for when the spent shell comes out
	if (PlayersWeapon.HModelControlBlock && (PlayersWeaponHModelController.keyframe_flags & 1))
	{
		Weapon_ThrowCase("ShotgunShell", "Eject");
	}
}

void Shotgun_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play(MHSS_Come, ONE_FIXED * 4 / 5, 0);
}

void Shotgun_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play(MHSS_Go, ONE_FIXED * 4 / 5, 0);
}

void Shotgun_Lift(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play(MHSS_Right_Out, ONE_FIXED * 3 / 10, 0);
}

void Shotgun_LoadShell(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play(MHSS_Standard_Reload, ONE_FIXED * 3 / 10, 1);
}

void Shotgun_Lower(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play(MHSS_Left_Out, ONE_FIXED * 3 / 10, 0);
}

// Only a model file with this move shows anything: see THE STOCK at the top
void Shotgun_StockMoves(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play((enum MarineHUDSubSequences)kShotgunStockMove, ONE_FIXED, 0);
}

void Shotgun_Pump(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Shotgun_Play(MHSS_Tertiary_Fire, ONE_FIXED * 7 / 10, 0);
}

// One shot: every pellet is turned a little away from where the gun's pointing, and looks for what it hits. It's done in the view's own directions, so that up and across mean what they do on the screen.
int FireShotgun(PLAYER_WEAPON_DATA *weaponPtr)
{
	TEMPLATE_WEAPON_DATA const *const twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];
	PLAYER_STATUS *const playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
	MATRIXCH toTheWorld = Global_VDB_Ptr->VDB_Mat;

	bool hitSomeone = false;

	TransposeMatrixCH(&toTheWorld);
	weaponPtr->PrimaryRoundsRemaining -= 65536;

	for (int pellet = 0; pellet < kShotgunPellets; pellet++)
	{
		int const spread = Shotgun_SpreadOfPellet(pellet, Own.StockOpen != 0);
		VECTORCH direction = GunMuzzleDirectionInVS;
		MATRIXCH turning;
		EULER turn;

		turn.EulerX = Shotgun_PelletTurn(FastRandom(), spread);
		turn.EulerY = Shotgun_PelletTurn(FastRandom(), spread);
		turn.EulerZ = 0;

		CreateEulerMatrix(&turn, &turning);
		RotateVector(&direction, &turning);
		RotateVector(&direction, &toTheWorld);
		Normalise(&direction);

		FindPolygonInLineOfSight(&direction, &Global_VDB_Ptr->VDB_World, 1, Player);

		if (LOS_ObjectHitPtr)
		{
			// A pellet before this one may have killed what this one hit, and taken its body away. A body part only counts if it still belongs to what was hit.
			SECTION_DATA *section = LOS_HModel_Section;

			if (section && section->my_controller != LOS_ObjectHitPtr->HModelControlBlock)
			{
				section = NULL;
			}

			HandleWeaponImpact(&LOS_Point, LOS_ObjectHitPtr->ObStrategyBlock, twPtr->PrimaryAmmoID, &direction, ONE_FIXED, section);

			if (AccuracyStats_TargetFilter(LOS_ObjectHitPtr->ObStrategyBlock))
			{
				hitSomeone = true;
			}
		}
	}

	// The end-of-level screen works out accuracy as hits out of shots. One shot counts once, and it counts as a hit if any pellet found someone, or a good shot would score far over 100%.
	CurrentGameStats_WeaponFired(playerStatusPtr->SelectedWeaponSlot, 1);

	if (hitSomeone)
	{
		CurrentGameStats_WeaponHit(playerStatusPtr->SelectedWeaponSlot, 1);
	}

	return 1;
}

static void Shotgun_BackToIdle(PLAYER_WEAPON_DATA *weaponPtr)
{
	weaponPtr->CurrentState = WEAPONSTATE_IDLE;
	weaponPtr->StateTimeOutCounter = 0;
}

static void Shotgun_StartLoading(PLAYER_WEAPON_DATA *weaponPtr)
{
	Own.EmptyWhenLoadingBegan = weaponPtr->PrimaryRoundsRemaining == 0 ? 1 : 0;
	Own.StopLoading = 0;
	Weapon_EnterState(weaponPtr, Own.StockOpen ? kShotgunState_ZoomOutToLoad : kShotgunState_Lift);
}

// Loading is over. With the stock open he goes back to looking along the gun, unless he's holding secondary fire, which closes it.
static void Shotgun_AfterLoading(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS const *const playerStatusPtr = (PLAYER_STATUS const *)playerStatus;

	if (!Own.StockOpen)
	{
		Shotgun_BackToIdle(weaponPtr);
	}
	else if (playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon)
	{
		Weapon_EnterState(weaponPtr, kShotgunState_StockClosing);
	}
	else
	{
		Weapon_EnterState(weaponPtr, kShotgunState_ZoomInAfterLoad);
	}
}

static void Shotgun_AtPlayerStart(void)
{
	// He may have died looking along it
	if (Own.StockOpen)
	{
		CameraZoomScale = kShotgunZoomStockClosed;
	}

	Own.EmptyWhenLoadingBegan = 0;
	Own.StopLoading = 0;
	Own.StockOpen = 0;
}

// Runs when the gun is brought up, and after a saved game loads with it in his hands, so that the view matches the stock
void Shotgun_WhenBroughtUp(PLAYER_WEAPON_DATA *weaponPtr)
{
	CameraZoomScale = Own.StockOpen ? kShotgunZoomStockOpen : kShotgunZoomStockClosed;
}

static int Shotgun_EveryFrame(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	static bool secondaryWasHeld = false;

	PLAYER_STATUS *const playerStatusPtr = (PLAYER_STATUS *)playerStatus;
	int const state = (int)weaponPtr->CurrentState;
	bool const secondaryHeld = playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon != 0;
	bool const secondaryPressed = secondaryHeld && !secondaryWasHeld;
	float const secondsThisFrame = NormalFrameTime / 65536.0f;

	secondaryWasHeld = secondaryHeld;

	// The common code has just set how fast he goes for this frame
	if (Own.StockOpen && state != kShotgunState_StockClosing && state != kShotgunState_ClosingToPutAway)
	{
		playerStatusPtr->Encumberance.MovementMultiple = kShotgunStockOpenMoving;
		playerStatusPtr->Encumberance.TurningMultiple = kShotgunStockOpenTurning;
		playerStatusPtr->Encumberance.JumpingMultiple = kShotgunStockOpenJumping;
	}

	switch (state)
	{
		case kShotgunState_ZoomIn:
		case kShotgunState_ZoomInAfterLoad:
			CameraZoomScale = Shotgun_ZoomTowards(CameraZoomScale, kShotgunZoomStockOpen, secondsThisFrame);
			break;

		case kShotgunState_ZoomOut:
		case kShotgunState_ZoomOutToLoad:
		case kShotgunState_ZoomOutToPutAway:
			CameraZoomScale = Shotgun_ZoomTowards(CameraZoomScale, kShotgunZoomStockClosed, secondsThisFrame);
			break;

		case kShotgunState_LoadShell:
			// Firing, secondary fire or another weapon stops the loading after this shell. Another weapon is remembered, and the swap happens once the gun is lowered.
			if (playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon || secondaryHeld || Weapon_AskedForAnotherWeapon(playerStatus, weaponPtr))
			{
				Own.StopLoading = 1;
			}
			break;

		case WEAPONSTATE_IDLE:
			if (secondaryPressed)
			{
				Weapon_EnterState(weaponPtr, Own.StockOpen ? kShotgunState_ZoomOut : kShotgunState_StockOpening);
				return 1;
			}
			break;

		default:
			break;
	}

	return 0;
}

// The common code asks for a reload, and the gun does it in its own way
static void Shotgun_StateBegan(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->CurrentState == WEAPONSTATE_RELOAD_PRIMARY)
	{
		Shotgun_StartLoading(weaponPtr);
	}
}

static int Shotgun_StateEnded(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	switch ((int)weaponPtr->CurrentState)
	{
		// Only if a frame took so long that the time ran out before it was seen to begin. The common code would load a whole magazine.
		case WEAPONSTATE_RELOAD_PRIMARY:
			Shotgun_StartLoading(weaponPtr);
			return 1;

		// He wants another gun, so the stock closes first
		case WEAPONSTATE_UNREADYING:
			if (Own.StockOpen)
			{
				Weapon_EnterState(weaponPtr, kShotgunState_ZoomOutToPutAway);
				return 1;
			}
			return 0;

		case kShotgunState_StockOpening:
			Own.StockOpen = 1;
			NewOnScreenMessage(GetTextString(TEXTSTRING_SHOTGUN_STOCK_OPEN));
			Weapon_EnterState(weaponPtr, kShotgunState_ZoomIn);
			return 1;

		case kShotgunState_ZoomIn:
		case kShotgunState_ZoomInAfterLoad:
			CameraZoomScale = kShotgunZoomStockOpen;
			Shotgun_BackToIdle(weaponPtr);
			return 1;

		case kShotgunState_ZoomOut:
			CameraZoomScale = kShotgunZoomStockClosed;
			Weapon_EnterState(weaponPtr, kShotgunState_StockClosing);
			return 1;

		case kShotgunState_StockClosing:
			Own.StockOpen = 0;
			NewOnScreenMessage(GetTextString(TEXTSTRING_SHOTGUN_STOCK_CLOSED));
			Shotgun_BackToIdle(weaponPtr);
			return 1;

		case kShotgunState_ZoomOutToLoad:
			CameraZoomScale = kShotgunZoomStockClosed;
			Weapon_EnterState(weaponPtr, kShotgunState_Lift);
			return 1;

		case kShotgunState_ZoomOutToPutAway:
			CameraZoomScale = kShotgunZoomStockClosed;
			Weapon_EnterState(weaponPtr, kShotgunState_ClosingToPutAway);
			return 1;

		case kShotgunState_ClosingToPutAway:
			Own.StockOpen = 0;
			Weapon_EnterState(weaponPtr, WEAPONSTATE_SWAPPING_OUT);
			return 1;

		case kShotgunState_Lift:
			Weapon_EnterState(weaponPtr, kShotgunState_LoadShell);
			return 1;

		case kShotgunState_LoadShell:
		{
			int const shellsNow = weaponPtr->PrimaryRoundsRemaining >> ONE_FIXED_SHIFT;
			int const shellsIn = Shotgun_ShellsThatGoIn(weaponPtr->StateTimeOutCounter, shellsNow, weaponPtr->PrimaryMagazinesRemaining);

			weaponPtr->PrimaryRoundsRemaining += shellsIn << ONE_FIXED_SHIFT;
			weaponPtr->PrimaryMagazinesRemaining -= shellsIn;

			Weapon_EnterState(weaponPtr, Shotgun_LoadingIsOver(shellsNow + shellsIn, weaponPtr->PrimaryMagazinesRemaining, Own.StopLoading != 0) ? kShotgunState_Lower : kShotgunState_LoadShell);
			return 1;
		}

		case kShotgunState_Lower:
			if (Own.EmptyWhenLoadingBegan)
			{
				Weapon_EnterState(weaponPtr, kShotgunState_Pump);
			}
			else
			{
				Shotgun_AfterLoading(playerStatus, weaponPtr);
			}
			return 1;

		case kShotgunState_Pump:
			Shotgun_AfterLoading(playerStatus, weaponPtr);
			return 1;

		default:
			return 0;
	}
}

static int Shotgun_SavedStateIsGood(const void *state)
{
	return Shotgun_OwnStateIsGood(*(const ShotgunOwnState *)state) ? 1 : 0;
}

const WEAPON_HOOKS Shotgun_Hooks =
{
	Shotgun_AtPlayerStart,
	Shotgun_EveryFrame,
	Shotgun_StateBegan,
	NULL, // MayLeaveState
	Shotgun_StateEnded,
	NULL, // MayTakeAmmo
	NULL, // DrawGun
	NULL, // DrawOverHud
	&Own,
	sizeof(Own),
	Shotgun_SavedStateIsGood,
	1 // SparesAreSingleRounds
};
