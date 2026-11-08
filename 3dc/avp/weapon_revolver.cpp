// The marine's revolver: five rounds, a cylinder that really turns, rounds loaded one at a time, and a scope on secondary fire. Written 2026-11-06 by Melanikus for the AvP port.
// The game takes it from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.
//
// The sums are in revolver.h, which says how the cylinder works. This file joins them to the game.
//
// LOADING IT
//
//   open        the gun swings open. If nothing has been fired, the rounds in it stay there.
//   throw out   if a chamber holds a spent case, the gun is tipped out instead. Spent cases and live rounds fly out as different shapes, and the live ones go back with the
//               spare rounds.
//   load round  the free hand brings up one round and pushes it in, once for each round. Firing, the scope or another weapon stops the loading after the round going in now.
//   close       the gun swings shut, and only then can it fire.
//
// THE SCOPE
//
// Secondary fire holds it up. It takes a moment to come up and the view walks in to twice as close. While it's up, the gun isn't drawn and the scope's picture is, and the player
// moves more slowly. Letting go brings it down again. Anything else the gun wants to do while the scope is up waits until it's down. Asking for another gun brings it down, even with
// the key still held.
//
// THE MODEL
//
// All of this is drawn by showing and hiding parts of the gun's model. Each chamber has two parts, a case and the bullet on it, so a spent round shows the case alone. The cylinder
// is turned by a "delta" animation, which plays on top of whatever else the model is doing, and whose clock is set by hand here to the angle wanted.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "gameplat.h"
#include "bh_types.h"
#include "dynblock.h"
#include "dynamics.h"
#include "bh_debris.h"
#include "bh_weapon.h"
#include "hmodel.h"
#include "sequences.h"
#include "psnd.h"
#include "weapons.h"
#include "weapon_hooks.h"
#include "weapon_scope.h"
#include "weapon_revolver.h"
#include "logString.h"

#include <stdio.h>

extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;
extern float CameraZoomScale;
extern int NormalFrameTime;
extern int NumActiveBlocks;
extern int FireNonAutomaticWeapon(PLAYER_WEAPON_DATA *weaponPtr);
extern void PulseRifleFidget(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

// The parts of the model, in chamber order. The model's parts aren't numbered in the order they sit round the cylinder, so these lists aren't in number order either.
static char *const kCaseOfChamber[kRevolverChambers] = { "TaurusBullet01", "TaurusBullet03", "TaurusBullet04", "TaurusBullet05", "TaurusBullet02" };
static char *const kBulletOfChamber[kRevolverChambers] = { "TaurusBulletPellet", "TaurusBulletPellet02", "TaurusBulletPellet03", "TaurusBulletPellet04", "TaurusBulletPellet01" };
static char *const kThrownFromChamber[kRevolverChambers] = { "Eject01", "Eject02", "Eject03", "Eject04", "Eject05" };
static char *const kRoundInTheHand = "TaurusExampleBullet";
static char *const kThrownCaseShape = "TaurusEjectionCase";
static char *const kThrownRoundShape = "TaurusEjectionBullet";

// How long a thrown case or round stays on the ground, in the game's time: 8 seconds.
static int const kThrownPiecesLast = ONE_FIXED * 8;

// A name for the animation that turns the cylinder, for finding it again.
static char *const kCylinderTurnName = "RevolverCylinder";

// The flag on the model's frames at the moment things happen: the cases fly out, or the round goes in.
static int const kFlagOnThatFrame = 1;

// What the view does with the scope up. 0.5 is twice as close. It walks there at 3 a second.
static float const kZoomWithScopeUp = 0.5f;
static float const kZoomWithScopeDown = 1.0f;
static float const kZoomRatePerSecond = 3.0f;

// How well he moves with the scope up: three quarters as fast, half as fast turning, and five sixths of his usual jump.
static int const kMovementWithScopeUp = (ONE_FIXED * 3) / 4;
static int const kTurningWithScopeUp = ONE_FIXED / 2;
static int const kJumpingWithScopeUp = (ONE_FIXED * 5) / 6;

// What's in the gun. This is what goes into a save.
static RevolverCylinder Own;

// What's only true for the moment, and doesn't go into a save. A game loaded in the middle of a reload just carries on loading.
static DELTA_CONTROLLER *cylinderTurn = NULL;
static int roundsLoadedThisReload = 0;
static int roundsShownThisReload = 0;
static int nextChamberToShow = 0;
static int stopLoading = 0;
static int momentRoundGoesIn = kRevolverLastMoment;
static int scopeIsUp = 0;
static int stateAfterScopeComesDown = WEAPONSTATE_IDLE;

// Shows or hides a part of the model. There may be no model, or the model may not have this part, and then nothing happens and the gun still works.
static void Revolver_ShowPart(char *name, int show)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	SECTION_DATA *const part = GetThisSectionData(PlayersWeaponHModelController.section_data, name);

	if (!part)
	{
		return;
	}

	if (show)
	{
		part->flags &= ~section_data_notreal;
	}
	else
	{
		part->flags |= section_data_notreal;
	}
}

static void Revolver_ShowChambers(void)
{
	for (int i = 0; i < kRevolverChambers; i++)
	{
		Revolver_ShowPart(kCaseOfChamber[i], Own.somethingIn[i]);
		Revolver_ShowPart(kBulletOfChamber[i], Own.live[i]);
	}
}

static void Revolver_TurnCylinderOnModel(void)
{
	if (cylinderTurn)
	{
		cylinderTurn->timer = Revolver_CylinderAnimationTime(Own.place);
	}
}

// Plays one of the gun's moves on its model, unless it's playing already. The file may not have this move, and then nothing happens and the gun still works.
static void Revolver_Play(int type, int move, int time)
{
	if (!PlayersWeapon.HModelControlBlock
		|| (PlayersWeaponHModelController.Sequence_Type == type && PlayersWeaponHModelController.Sub_Sequence == move)
		|| !HModelSequence_Exists(&PlayersWeaponHModelController, type, move))
	{
		return;
	}

	InitHModelSequence(&PlayersWeaponHModelController, type, move, time);
	PlayersWeaponHModelController.Looped = 0;
}

// How far through the loading move the round reaches the cylinder. The model marks that moment with a flag on one of its frames. If it doesn't, the round goes in at the very end.
static int Revolver_FindMomentRoundGoesIn(void)
{
	if (!PlayersWeapon.HModelControlBlock
		|| !PlayersWeaponHModelController.Root_Section
		|| !HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Reload))
	{
		return kRevolverLastMoment;
	}

	SEQUENCE const *const sequence = GetSequencePointer(HMSQT_MarineHUD, (int)MHSS_Standard_Reload, PlayersWeaponHModelController.Root_Section);
	KEYFRAME_DATA const *frame = sequence->first_frame;
	int moment = 0;

	// The frames end with one marked as the last. The count is there so that a damaged list of frames can't go round for ever.
	for (int count = 0; count < 1024 && frame; count++)
	{
		if (frame->frame_has_extended_data && (((KEYFRAME_DATA_EXTENDED const *)frame)->flags & kFlagOnThatFrame))
		{
			return moment;
		}

		if (frame->last_frame)
		{
			break;
		}

		moment += frame->Sequence_Length;
		frame = frame->Next_Frame;
	}

	return kRevolverLastMoment;
}

// A spent case or a live round flies out of the gun as a small piece of debris, from a part of the model, with a push of its own.
static void Revolver_ThrowPiece(char *shapeName, char *fromPart)
{
	SECTION_DATA *const part = PlayersWeapon.HModelControlBlock ? GetThisSectionData(PlayersWeaponHModelController.section_data, fromPart) : NULL;

	if (!part || (NumActiveBlocks > maxobjects - 5) || (NumActiveStBlocks > maxstblocks - 5))
	{
		return;
	}

	MODULEMAPBLOCK *const map = &TempModuleMap;
	MODULE module;

	CreateShapeInstance(map, shapeName);

	module.m_numlights = 0;
	module.m_lightarray = NULL;
	module.m_mapptr = map;
	module.m_sbptr = NULL;
	module.m_dptr = NULL;
	AllocateModuleObject(&module);

	DISPLAYBLOCK *const display = module.m_dptr;

	if (!display)
	{
		return;
	}

	display->ObMyModule = NULL;
	display->ObWorld = part->World_Offset;

	STRATEGYBLOCK *const strategy = AttachNewStratBlock(NULL, map, display);

	// The display block goes back, or it would stay in use with nothing to look after it
	if (!strategy)
	{
		DestroyActiveObject(display);
		return;
	}

	strategy->I_SBtype = I_BehaviourFragment;
	strategy->SBdataptr = AllocateMem(sizeof(ONE_SHOT_BEHAV_BLOCK));

	if (!strategy->SBdataptr)
	{
		RemoveBehaviourStrategy(strategy);
		return;
	}

	((ONE_SHOT_BEHAV_BLOCK *)strategy->SBdataptr)->counter = kThrownPiecesLast;

	DYNAMICSBLOCK *const dynamics = AllocateDynamicsBlock(DYNAMICS_TEMPLATE_DEBRIS);
	strategy->DynPtr = dynamics;

	if (!dynamics)
	{
		RemoveBehaviourStrategy(strategy);
		return;
	}

	dynamics->Position = part->World_Offset;
	dynamics->OrientMat = part->SecMat;
	dynamics->AngVelocity.EulerX = (FastRandom() & 4095) << 1;
	dynamics->AngVelocity.EulerY = (FastRandom() & 4095) << 2;
	dynamics->AngVelocity.EulerZ = 0;

	// Mostly out to the side, a bit up and a bit back, and each one a little different, so they don't all fall as one.
	VECTORCH push;
	push.vx = 6000 - (FastRandom() & 8191);
	push.vy = -200 - (FastRandom() & 511);
	push.vz = -((FastRandom() & 2047) * 3) / 2;
	RotateVector(&push, &part->SecMat);
	dynamics->LinImpulse = push;

	strategy->SBflags.not_on_motiontracker = 1;
}

static int Revolver_RoundsNow(PLAYER_WEAPON_DATA const *weaponPtr)
{
	return (int)(weaponPtr->PrimaryRoundsRemaining >> ONE_FIXED_SHIFT);
}

// Anything can change the round count from outside: a cheat, a new gun, a save. If the chambers no longer agree with it, the count wins.
static void Revolver_KeepInStepWithRounds(PLAYER_WEAPON_DATA const *weaponPtr)
{
	if (Revolver_LiveRounds(&Own) != Revolver_RoundsNow(weaponPtr))
	{
		Revolver_FillFromRounds(&Own, Revolver_RoundsNow(weaponPtr));
		Revolver_ShowChambers();
	}
}

// The scope is put away with nothing to see it go, as when he dies or a game is loaded.
static void Revolver_ScopeGoesAway(void)
{
	if (scopeIsUp)
	{
		CameraZoomScale = kZoomWithScopeDown;
	}

	scopeIsUp = 0;
	stateAfterScopeComesDown = WEAPONSTATE_IDLE;
}

// Runs each time the gun's model is made: when the gun is brought up, and when a game is loaded. Whatever was set on the model before is gone by then.
void Revolver_ModelIsReady(PLAYER_WEAPON_DATA *weaponPtr)
{
	int const mostInGun = (int)TemplateAmmo[TemplateWeapon[weaponPtr->WeaponIDNumber].PrimaryAmmoID].AmmoPerMagazine;

	// The count can come from a save, which is a file anybody can change
	if (weaponPtr->PrimaryRoundsRemaining > (unsigned int)mostInGun)
	{
		weaponPtr->PrimaryRoundsRemaining = (unsigned int)mostInGun;
	}

	Revolver_ScopeGoesAway();

	// The scope's own states make no sense without it
	switch ((int)weaponPtr->CurrentState)
	{
		case kRevolverState_ScopeStartsUp:
		case kRevolverState_ScopeRises:
		case kRevolverState_ScopeComesDown:
		case kRevolverState_ScopeLastBitDown:
			weaponPtr->CurrentState = WEAPONSTATE_IDLE;
			weaponPtr->StateTimeOutCounter = 0;
			break;

		default:
			break;
	}

	cylinderTurn = NULL;

	if (PlayersWeapon.HModelControlBlock)
	{
		cylinderTurn = Get_Delta_Sequence(&PlayersWeaponHModelController, kCylinderTurnName);

		if (!cylinderTurn && HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Tertiary_Fire))
		{
			// A time of 0 means its clock doesn't run by itself. This file sets it.
			cylinderTurn = Add_Delta_Sequence(&PlayersWeaponHModelController, kCylinderTurnName, HMSQT_MarineHUD, (int)MHSS_Tertiary_Fire, 0);
		}
	}

	Revolver_ShowPart(kRoundInTheHand, 0);
	Revolver_KeepInStepWithRounds(weaponPtr);
	Revolver_TurnCylinderOnModel();
	Revolver_ShowChambers();
}

// A shot that hits a thing in the level, not a wall and not a creature, makes a loud crack where it lands. These are big bullets.
int FireRevolver(PLAYER_WEAPON_DATA *weaponPtr)
{
	STRATEGYBLOCK const *const hit = PlayersTarget.DispPtr ? PlayersTarget.DispPtr->ObStrategyBlock : NULL;

	if (hit && (!hit->SBdptr || !hit->SBdptr->HModelControlBlock))
	{
		Sound_Play(SID_NADEEXPLODE, "d", &PlayersTarget.Position);
	}

	return FireNonAutomaticWeapon(weaponPtr);
}

// The scope's two moves are stored under a different kind of animation, and the fidget code expects the marine's kind, so the gun comes back to that first.
void Revolver_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	if (PlayersWeaponHModelController.Sequence_Type != HMSQT_MarineHUD)
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Stationary, ONE_FIXED);
	}

	PulseRifleFidget(playerStatus, weaponPtr);
}

// Each shot starts this move again from the beginning, even straight after the last one
void Revolver_Recoil(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT
		&& PlayersWeapon.HModelControlBlock
		&& HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Fire))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Fire, ONE_FIXED / 2);
		PlayersWeaponHModelController.Looped = 0;
	}
}

void Revolver_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_Play(HMSQT_MarineHUD, (int)MHSS_Come, ONE_FIXED);
}

void Revolver_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_Play(HMSQT_MarineHUD, (int)MHSS_Go, ONE_FIXED * 7 / 10);
}

void Revolver_Open(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_Play(HMSQT_MarineHUD, (int)MHSS_Right_Out, ONE_FIXED * 4 / 5);
}

// At the frame the model marks, everything in the chambers flies out.
void Revolver_ThrowOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_Play(HMSQT_MarineHUD, (int)MHSS_Secondary_Fire, ONE_FIXED);

	if (!PlayersWeapon.HModelControlBlock || !(PlayersWeaponHModelController.keyframe_flags & kFlagOnThatFrame))
	{
		return;
	}

	int spentCases = 0;
	int liveRounds = 0;

	for (int i = 0; i < kRevolverChambers; i++)
	{
		switch (Revolver_WhatComesOut(&Own, i))
		{
			case kRevolverLiveRoundComesOut:
				Revolver_ThrowPiece(kThrownRoundShape, kThrownFromChamber[i]);
				liveRounds++;
				break;

			case kRevolverSpentCaseComesOut:
				Revolver_ThrowPiece(kThrownCaseShape, kThrownFromChamber[i]);
				spentCases++;
				break;

			default:
				break;
		}
	}

	// Once each reload. What flew out is only a speck on the screen, and this says what it was.
	if (spentCases || liveRounds)
	{
		char line[64];
		_snprintf(line, sizeof(line) - 1, "revolver: threw out %d spent cases and %d live rounds", spentCases, liveRounds);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	Revolver_EmptyChambers(&Own);
	Revolver_ShowChambers();
}

// The hand's move follows the state's clock, so that a slow frame can't leave the hand behind the rounds, and each round appears in its chamber as the hand gets there.
void Revolver_LoadRound(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	int animationTime = 0;

	if (PlayersWeaponHModelController.Sequence_Type == HMSQT_MarineHUD && PlayersWeaponHModelController.Sub_Sequence == (int)MHSS_Standard_Reload)
	{
		animationTime = Revolver_AnimationTimeFromClock(weaponPtr->StateTimeOutCounter);
	}
	else if (HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Reload))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_MarineHUD, (int)MHSS_Standard_Reload, ONE_FIXED);
		PlayersWeaponHModelController.Looped = 0;
		PlayersWeaponHModelController.Playing = 0;
	}

	int const toShow = Revolver_RoundsToShow(roundsLoadedThisReload, roundsShownThisReload, animationTime, momentRoundGoesIn);

	for (int i = 0; i < toShow; i++)
	{
		Revolver_ShowPart(kCaseOfChamber[nextChamberToShow], 1);
		Revolver_ShowPart(kBulletOfChamber[nextChamberToShow], 1);
		nextChamberToShow = Revolver_PlaceForward(nextChamberToShow, 1);
	}

	roundsShownThisReload += toShow;

	Revolver_ShowPart(kRoundInTheHand, animationTime < momentRoundGoesIn);
	PlayersWeaponHModelController.sequence_timer = animationTime;
}

void Revolver_Close(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_ShowPart(kRoundInTheHand, 0);
	Revolver_Play(HMSQT_MarineHUD, (int)MHSS_Left_Out, ONE_FIXED * 7 / 10);
}

void Revolver_ScopeStartsUp(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_Play(HMSQT_PredatorHUD, (int)PHSS_PullBack, ONE_FIXED / 8);
}

void Revolver_ScopeLastBitDown(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	Revolver_Play(HMSQT_PredatorHUD, (int)PHSS_Program, ONE_FIXED / 8);
}

static void Revolver_BackToIdle(PLAYER_WEAPON_DATA *weaponPtr)
{
	weaponPtr->CurrentState = WEAPONSTATE_IDLE;
	weaponPtr->StateTimeOutCounter = 0;
}

static void Revolver_StartLoading(PLAYER_WEAPON_DATA *weaponPtr)
{
	// The scope comes down first, and the reload begins again after that
	if (scopeIsUp)
	{
		stateAfterScopeComesDown = WEAPONSTATE_RELOAD_PRIMARY;
		Weapon_EnterState(weaponPtr, kRevolverState_ScopeComesDown);
		return;
	}

	momentRoundGoesIn = Revolver_FindMomentRoundGoesIn();
	stopLoading = 0;
	roundsLoadedThisReload = 0;
	roundsShownThisReload = 0;
	nextChamberToShow = Own.place;

	Weapon_EnterState(weaponPtr, Revolver_HasSpentCase(&Own) ? kRevolverState_ThrowOut : kRevolverState_Open);
}

// One loading movement is over: the rounds go in, and the gun either does another or closes.
static void Revolver_RoundsGoIn(PLAYER_WEAPON_DATA *weaponPtr)
{
	int const mostInGun = (int)TemplateAmmo[TemplateWeapon[weaponPtr->WeaponIDNumber].PrimaryAmmoID].AmmoPerMagazine;
	int const rounds = Revolver_RoundsToLoad(weaponPtr->StateTimeOutCounter, (int)weaponPtr->PrimaryRoundsRemaining, mostInGun, weaponPtr->PrimaryMagazinesRemaining);

	weaponPtr->PrimaryRoundsRemaining += (unsigned int)rounds << ONE_FIXED_SHIFT;
	weaponPtr->PrimaryMagazinesRemaining -= (unsigned char)rounds;
	roundsLoadedThisReload += rounds;

	for (int i = 0; i < rounds; i++)
	{
		Revolver_MarkLoaded(&Own);
	}

	Revolver_TurnCylinderOnModel();

	if (Revolver_KeepsLoading((int)weaponPtr->PrimaryRoundsRemaining, mostInGun, weaponPtr->PrimaryMagazinesRemaining, stopLoading != 0))
	{
		// The next movement carries on from the time this one ran over by, and the state doesn't begin again
		weaponPtr->StateTimeOutCounter += rounds << ONE_FIXED_SHIFT;
	}
	else
	{
		Weapon_EnterState(weaponPtr, kRevolverState_Close);
	}
}

static void Revolver_AtPlayerStart(void)
{
	Own.place = 0;
	Revolver_EmptyChambers(&Own);
	Revolver_ScopeGoesAway();
	cylinderTurn = NULL;
	stopLoading = 0;
}

static int Revolver_EveryFrame(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	PLAYER_STATUS *const playerStatusPtr = (PLAYER_STATUS *)playerStatus;
	float const secondsThisFrame = (float)NormalFrameTime / (float)ONE_FIXED;
	int const secondaryHeld = playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon;

	if (scopeIsUp)
	{
		playerStatusPtr->Encumberance.MovementMultiple = kMovementWithScopeUp;
		playerStatusPtr->Encumberance.TurningMultiple = kTurningWithScopeUp;
		playerStatusPtr->Encumberance.JumpingMultiple = kJumpingWithScopeUp;
	}

	switch ((int)weaponPtr->CurrentState)
	{
		case WEAPONSTATE_IDLE:
			Revolver_KeepInStepWithRounds(weaponPtr);

			// The common code looks at second fire before the weapon keys, so while the scope's key is held it would never see another gun asked for, and it's looked for here.
			// The scope comes down first. The next time round here it's down, so the change starts, and if the key has been let go by then, the common code starts it the same way.
			if (secondaryHeld && Weapon_AskedForAnotherWeapon(playerStatus, weaponPtr))
			{
				if (scopeIsUp)
				{
					stateAfterScopeComesDown = WEAPONSTATE_IDLE;
					Weapon_EnterState(weaponPtr, kRevolverState_ScopeComesDown);
				}
				else
				{
					Weapon_StartChangingGun(playerStatus, weaponPtr);
				}

				return 1;
			}

			if (!scopeIsUp && secondaryHeld)
			{
				Weapon_EnterState(weaponPtr, kRevolverState_ScopeStartsUp);
				return 1;
			}

			if (scopeIsUp && !secondaryHeld)
			{
				stateAfterScopeComesDown = WEAPONSTATE_IDLE;
				Weapon_EnterState(weaponPtr, kRevolverState_ScopeComesDown);
				return 1;
			}

			return 0;

		case kRevolverState_LoadRound:
			// Firing, the scope or another weapon stops the loading after this round. Another weapon is remembered, and the swap happens once the gun has closed.
			if (playerStatusPtr->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon || secondaryHeld || Weapon_AskedForAnotherWeapon(playerStatus, weaponPtr))
			{
				stopLoading = 1;
			}
			return 0;

		// A press of a weapon key while the scope is moving is remembered here, or it would be lost, because the common code isn't looking at the keys
		case kRevolverState_ScopeStartsUp:
		case kRevolverState_ScopeLastBitDown:
			Weapon_AskedForAnotherWeapon(playerStatus, weaponPtr);
			return 0;

		case kRevolverState_ScopeRises:
			CameraZoomScale = Revolver_ZoomTowards(CameraZoomScale, kZoomWithScopeUp, kZoomRatePerSecond, secondsThisFrame);
			Weapon_AskedForAnotherWeapon(playerStatus, weaponPtr);
			return 0;

		case kRevolverState_ScopeComesDown:
			CameraZoomScale = Revolver_ZoomTowards(CameraZoomScale, kZoomWithScopeDown, kZoomRatePerSecond, secondsThisFrame);
			Weapon_AskedForAnotherWeapon(playerStatus, weaponPtr);
			return 0;

		default:
			return 0;
	}
}

static void Revolver_StateBegan(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	switch ((int)weaponPtr->CurrentState)
	{
		// The common code asks for a reload, and the gun does it in its own way
		case WEAPONSTATE_RELOAD_PRIMARY:
			Revolver_StartLoading(weaponPtr);
			break;

		// Whatever a long frame did, the chambers now agree with the round count
		case kRevolverState_Close:
			Revolver_FillFromRounds(&Own, Revolver_RoundsNow(weaponPtr));
			Revolver_ShowChambers();
			break;

		default:
			break;
	}
}

static int Revolver_StateEnded(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	switch ((int)weaponPtr->CurrentState)
	{
		// Only if a frame took so long that the time ran out before it was seen to begin. The common code would load a whole magazine.
		case WEAPONSTATE_RELOAD_PRIMARY:
			Revolver_StartLoading(weaponPtr);
			return 1;

		// The cylinder turns as the gun settles after a shot, and the common code carries on as for any gun
		case WEAPONSTATE_RECOIL_PRIMARY:
			Revolver_MarkShot(&Own);
			Revolver_TurnCylinderOnModel();
			Revolver_ShowChambers();
			return 0;

		case kRevolverState_ThrowOut:
			// The live rounds that fell out go back with the spares. This is done here in case the frame the model marks never came.
			Revolver_EmptyChambers(&Own);
			Revolver_ShowChambers();
			weaponPtr->PrimaryMagazinesRemaining = (unsigned char)Revolver_SpareAfterTippingOut(weaponPtr->PrimaryMagazinesRemaining, Revolver_RoundsNow(weaponPtr));
			weaponPtr->PrimaryRoundsRemaining = 0;
			Weapon_EnterState(weaponPtr, kRevolverState_LoadRound);
			return 1;

		case kRevolverState_Open:
			Weapon_EnterState(weaponPtr, kRevolverState_LoadRound);
			return 1;

		case kRevolverState_LoadRound:
			Revolver_RoundsGoIn(weaponPtr);
			return 1;

		case kRevolverState_Close:
			Revolver_BackToIdle(weaponPtr);
			return 1;

		case kRevolverState_ScopeStartsUp:
			scopeIsUp = 1;
			Weapon_EnterState(weaponPtr, kRevolverState_ScopeRises);
			return 1;

		case kRevolverState_ScopeRises:
			CameraZoomScale = kZoomWithScopeUp;
			Revolver_BackToIdle(weaponPtr);
			return 1;

		case kRevolverState_ScopeComesDown:
			CameraZoomScale = kZoomWithScopeDown;
			scopeIsUp = 0;
			Weapon_EnterState(weaponPtr, kRevolverState_ScopeLastBitDown);
			return 1;

		case kRevolverState_ScopeLastBitDown:
			if (stateAfterScopeComesDown == WEAPONSTATE_IDLE)
			{
				Revolver_BackToIdle(weaponPtr);
			}
			else
			{
				Weapon_EnterState(weaponPtr, stateAfterScopeComesDown);
			}

			stateAfterScopeComesDown = WEAPONSTATE_IDLE;
			return 1;

		default:
			return 0;
	}
}

static int Revolver_DrawGun(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	return scopeIsUp ? 0 : 1;
}

static void Revolver_DrawOverHud(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (scopeIsUp)
	{
		WeaponScope_Draw();
	}
}

static int Revolver_SavedStateIsGood(const void *state)
{
	return Revolver_SavedCylinderIsGood((const RevolverCylinder *)state) ? 1 : 0;
}

const WEAPON_HOOKS Revolver_Hooks =
{
	Revolver_AtPlayerStart,
	Revolver_EveryFrame,
	Revolver_StateBegan,
	NULL, // MayLeaveState
	Revolver_StateEnded,
	NULL, // MayTakeAmmo
	Revolver_DrawGun,
	Revolver_DrawOverHud,
	&Own,
	sizeof(Own),
	Revolver_SavedStateIsGood,
	1 // SparesAreSingleRounds
};
