// The Predator's staff. The game takes this from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// PRIMARY FIRE swings it, in one of two moves chosen at random. The model marks the moment each swing lands, and that's
// when anything in front of him, close enough, gets hurt. The numbers are in the staff's ammunition row in equipment.cpp.
//
// SECONDARY FIRE throws it. The staff in his hands stays there: what flies is a separate object, the model in
// StaffPickup.RIF, thrown from where he's looking. Its life is described further down, at THE THROWN STAFF. The mod's own
// code was read for how it flies, and this follows it, apart from the time limits, a thing that isn't a body, how a landed one
// finds the floor, and the line it looks along each frame, which here starts from where it was last frame, not where it was thrown.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "dynamics.h"
#include "hmodel.h"
#include "sequences.h"
#include "weapons.h"
#include "bh_weapon.h"
#include "view.h"
#include "load_shape.h"
#include "weapon_hooks.h"
#include "weapon_staff.h"
#include "staff.h"
#include "logString.h"
#include "savegame.h"
#include "los.h"
#include "particle.h"
#include "lighting.h"
#include "decal.h"

#define UseLocalAssert FALSE
#include "ourasert.h"

#include <string.h>

extern DISPLAYBLOCK PlayersWeapon;
extern HMODELCONTROLLER PlayersWeaponHModelController;
extern int WeaponFidgetPlaying;
extern int MeleeWeapon_180Degree_Front_Core(DAMAGE_PROFILE *damage, int multiple, int range);
extern void NewTrailPoint(DYNAMICSBLOCK *dynPtr);
extern int NormalFrameTime;

static char kStaffRootSection[] = "Staff ROOT";

// Starts a move on the model, gliding into it from wherever the model is now. The model may not have it.
static void PredStaff_GlideTo(enum PredatorHUDSubSequences move, int glideTime, int time, int looped)
{
	if (PlayersWeapon.HModelControlBlock && HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_PredatorHUD, (int)move))
	{
		InitHModelTweening(&PlayersWeaponHModelController, glideTime, HMSQT_PredatorHUD, (int)move, time, looped);
	}
}

void PredStaff_SwapIn(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT
		&& PlayersWeapon.HModelControlBlock
		&& HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_PredatorHUD, (int)PHSS_Come))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_PredatorHUD, (int)PHSS_Come, kStaffSwapInTime);
		PlayersWeaponHModelController.Looped = 0;
	}
}

void PredStaff_SwapOut(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (weaponPtr->StateTimeOutCounter == WEAPONSTATE_INITIALTIMEOUTCOUNT)
	{
		PredStaff_GlideTo(PHSS_Go, kStaffGlideTime, kStaffSwapOutTime, 0);
	}
}

// He stands with it, and now and then does something with his hands. The model has no running move.
void PredStaff_Idle(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	int const move = PlayersWeaponHModelController.Sub_Sequence;

	if (move != (int)PHSS_Stand && move != (int)PHSS_Fidget)
	{
		PredStaff_GlideTo(PHSS_Stand, ONE_FIXED >> 3, kStaffStandTime, 1);
	}

	if (weaponPtr->StateTimeOutCounter > ONE_FIXED)
	{
		if (WeaponFidgetPlaying)
		{
			if (PlayersWeaponHModelController.sequence_timer == (ONE_FIXED - 1))
			{
				PredStaff_GlideTo(PHSS_Stand, ONE_FIXED >> 3, kStaffStandTime, 1);
				WeaponFidgetPlaying = 0;
			}
		}
		else if ((FastRandom() & 255) == 0
			&& HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_PredatorHUD, (int)PHSS_Fidget))
		{
			PredStaff_GlideTo(PHSS_Fidget, ONE_FIXED >> 2, kStaffFidgetTime, 0);
			weaponPtr->StateTimeOutCounter = 0;
			WeaponFidgetPlaying = 1;
		}
	}
}

void PredStaff_Swing(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	int const move = PlayersWeaponHModelController.Sub_Sequence;

	if (move != (int)PHSS_Attack_Jab && move != (int)PHSS_Attack_Primary_Two)
	{
		PredStaff_GlideTo(Staff_UseSecondSwing(FastRandom()) ? PHSS_Attack_Primary_Two : PHSS_Attack_Jab, kStaffGlideTime, kStaffSwingTime, 0);
	}

	if (PlayersWeaponHModelController.keyframe_flags & 1)
	{
		TEMPLATE_AMMO_DATA *const ammo = &TemplateAmmo[AMMO_PRED_STAFF];

		MeleeWeapon_180Degree_Front_Core(&ammo->MaxDamage[AvP.Difficulty], ONE_FIXED, ammo->MaxRange);
	}
}

int PredStaff_StartThrow(PLAYER_WEAPON_DATA *weaponPtr)
{
	return 1;
}

// THE THROWN STAFF. What flies is a separate object, with a short life in three stages.
//
//   Flying: it goes the way he was looking, and gravity bends its path. Nothing stops it by itself: each frame a line is
//   looked along from where it was to where it is now, and it reacts to what's on that line.
//
//   Landed: it hit part of the level. It makes sparks, loses its speed, and drops to the floor, where it lies.
//
//   In a body: it hit someone. It hurts them with the speargun's damage, then stays in that part of them, and moves with it.
//
// Hitting a thing that isn't a body and isn't part of the level hurts it, and the staff is gone. A body part it cuts off
// doesn't stop it: it flies on.
//
// In the mod, a staff that has landed stays for the whole level, and so does one that hit nothing. Throws don't run out,
// so that would let one player fill the game's list of objects. Here each stage has a time limit (staff.h).
typedef struct
{
	int State;
	int TimeLeft;
	int DistanceFlown;
	int HeightItHitAt;    // for the log: how far it then dropped, when its time runs out
	int RestHeight;       // how far down it stops once it has landed: the floor below it
	char BodyName[SB_NAME_LENGTH];
	int BodyPartNumber;
	VECTORCH PlaceOnBodyPart;
	VECTORCH PointingOnBodyPart;
} THROWN_STAFF_BEHAV_BLOCK;

// One line in the log for what became of a throw, with how far it had flown. There's at most one per throw, so these can't flood it.
static void ThrownStaff_Tell(const char *whatHappened, int distanceFlown)
{
	char line[128];

	_snprintf(line, sizeof(line) - 1, "staff: %s, after flying %d mm", whatHappened, distanceFlown);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

// Makes the object, ready to fly from this place. It comes back with nothing if the game has no room for one.
static STRATEGYBLOCK *ThrownStaff_Make(VECTORCH *place)
{
	DISPLAYBLOCK *const dispPtr = MakeObject(I_BehaviourThrownStaff, place);

	if (!dispPtr)
	{
		return 0;
	}

	STRATEGYBLOCK *const sbPtr = dispPtr->ObStrategyBlock;
	DYNAMICSBLOCK *const dynPtr = AllocateDynamicsBlock(DYNAMICS_TEMPLATE_GRENADE);

	if (!dynPtr)
	{
		RemoveBehaviourStrategy(sbPtr);
		return 0;
	}

	sbPtr->DynPtr = dynPtr;
	sbPtr->SBdataptr = AllocateMem(sizeof(THROWN_STAFF_BEHAV_BLOCK));

	if (!sbPtr->SBdataptr)
	{
		RemoveBehaviourStrategy(sbPtr);
		return 0;
	}

	memset(sbPtr->SBdataptr, 0, sizeof(THROWN_STAFF_BEHAV_BLOCK));

	THROWN_STAFF_BEHAV_BLOCK *const block = (THROWN_STAFF_BEHAV_BLOCK *)sbPtr->SBdataptr;

	block->State = kThrownStaffFlying;
	block->TimeLeft = kStaffFlightTime;

	// In flight nothing collides: the line looked along each frame decides what it hits.
	dynPtr->DynamicsType = DYN_TYPE_NO_COLLISIONS;
	dynPtr->Position = *place;
	dynPtr->PrevPosition = *place;

	return sbPtr;
}

// It hit part of the level: sparks fly back the way it came, and it stops there and drops.
static void ThrownStaff_Land(STRATEGYBLOCK *sbPtr, THROWN_STAFF_BEHAV_BLOCK *block, VECTORCH *point, VECTORCH *direction)
{
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;
	VECTORCH beyond;

	beyond.vx = point->vx + direction->vx;
	beyond.vy = point->vy + direction->vy;
	beyond.vz = point->vz + direction->vz;

	MakeFocusedExplosion(point, &beyond, 20, PARTICLE_SPARK);
	MakeLightElement(point, LIGHTELEMENT_ELECTRICAL_SPARKS);

	// It comes back a little from what it hit, to be in the open, and looks straight down from there for the floor. It falls that far
	// and stops. It doesn't collide with anything on the way: when it did, as the mod's does, one in a test run ended up 88 metres
	// below where it hit. Where there's no floor in sight, it stays where it is.
	VECTORCH place;
	VECTORCH down;

	place.vx = point->vx - MUL_FIXED(direction->vx, kStaffBackOffALanding);
	place.vy = point->vy - MUL_FIXED(direction->vy, kStaffBackOffALanding);
	place.vz = point->vz - MUL_FIXED(direction->vz, kStaffBackOffALanding);
	down.vx = 0;
	down.vy = ONE_FIXED;
	down.vz = 0;

	FindPolygonInLineOfSight_TwoIgnores(&down, &place, 0, sbPtr->SBdptr, Player);

	block->RestHeight = LOS_ObjectHitPtr ? LOS_Point.vy : place.vy;
	block->HeightItHitAt = point->vy;
	block->State = kThrownStaffLanded;
	block->TimeLeft = kStaffStayTime;

	dynPtr->Position = place;
	dynPtr->PrevPosition = place;
	dynPtr->LinVelocity.vx = dynPtr->LinVelocity.vy = dynPtr->LinVelocity.vz = 0;
	dynPtr->LinImpulse.vx = dynPtr->LinImpulse.vy = dynPtr->LinImpulse.vz = 0;
}

// One frame after landing: it falls until it reaches the floor it found, and then lies there. Down is the way the numbers grow.
static void ThrownStaff_Drop(STRATEGYBLOCK *sbPtr, THROWN_STAFF_BEHAV_BLOCK *block)
{
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;

	if (dynPtr->GravityOn && dynPtr->Position.vy >= block->RestHeight)
	{
		dynPtr->Position.vy = block->RestHeight;
		dynPtr->GravityOn = 0;
		dynPtr->LinImpulse.vx = dynPtr->LinImpulse.vy = dynPtr->LinImpulse.vz = 0;
	}
}

// Nothing stopped it this frame. The game's movement code only keeps an object's last place when it has collisions, and in the
// air this has none. So it's kept here: without it, "where it was" would stay where it was thrown from, and the line looked
// along would run all the way back to him, and hit anyone who has since stepped across it.
static void ThrownStaff_FlyOn(DYNAMICSBLOCK *dynPtr)
{
	NewTrailPoint(dynPtr);
	dynPtr->PrevPosition = dynPtr->Position;
}

// One frame in the air
static void ThrownStaff_Fly(STRATEGYBLOCK *sbPtr, THROWN_STAFF_BEHAV_BLOCK *block)
{
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;
	VECTORCH step;
	VECTORCH direction;

	step.vx = dynPtr->Position.vx - dynPtr->PrevPosition.vx;
	step.vy = dynPtr->Position.vy - dynPtr->PrevPosition.vy;
	step.vz = dynPtr->Position.vz - dynPtr->PrevPosition.vz;

	if (step.vx == 0 && step.vy == 0 && step.vz == 0)
	{
		// Its first frame: it hasn't moved yet, so it looks the way it points.
		direction.vx = dynPtr->OrientMat.mat31;
		direction.vy = dynPtr->OrientMat.mat32;
		direction.vz = dynPtr->OrientMat.mat33;
	}
	else
	{
		// It always points the way it's going, so it tips over as gravity bends its path.
		direction = step;
		Normalise(&direction);
		MakeMatrixFromDirection(&direction, &dynPtr->OrientMat);
		MatrixToEuler(&dynPtr->OrientMat, &dynPtr->OrientEuler);
	}

	// It can't hit itself, or the player who threw it.
	FindPolygonInLineOfSight_TwoIgnores(&direction, &dynPtr->PrevPosition, 0, sbPtr->SBdptr, Player);

	int const stepLength = Magnitude(&step);

	block->DistanceFlown += stepLength;

	if (!LOS_ObjectHitPtr || !ThrownStaff_ReachesIt(LOS_Lambda, stepLength))
	{
		ThrownStaff_FlyOn(dynPtr);
		return;
	}

	// What the line found is kept now, because hurting something can look along other lines.
	DISPLAYBLOCK *const hit = LOS_ObjectHitPtr;
	STRATEGYBLOCK *const target = hit->ObStrategyBlock;
	SECTION_DATA *const part = LOS_HModel_Section;
	VECTORCH point = LOS_Point;
	VECTORCH normal = LOS_ObjectNormal;

	if (!target)
	{
		ThrownStaff_Tell("hit the level", block->DistanceFlown);
		ThrownStaff_Land(sbPtr, block, &point, &direction);
		return;
	}

	DAMAGE_PROFILE *const damage = &TemplateAmmo[AMMO_PRED_RIFLE].MaxDamage[AvP.Difficulty];
	VECTORCH incoming;

	GetDirectionOfAttack(target, &direction, &incoming);

	if (part && part->my_controller && part->sempai && hit->HModelControlBlock)
	{
		// Where it went in, on that body part, in the part's own terms: this has to be worked out while the part is still there.
		MATRIXCH toPart = part->SecMat;
		VECTORCH placeOnPart;
		VECTORCH pointingOnPart = direction;
		int const partNumber = part->sempai->IDnumber;
		char bodyName[SB_NAME_LENGTH];

		TransposeMatrixCH(&toPart);
		placeOnPart.vx = point.vx - part->World_Offset.vx;
		placeOnPart.vy = point.vy - part->World_Offset.vy;
		placeOnPart.vz = point.vz - part->World_Offset.vz;
		RotateVector(&placeOnPart, &toPart);
		RotateVector(&pointingOnPart, &toPart);
		COPY_NAME(bodyName, target->SBname);

		AddDecalToHModel(&normal, &point, part);

		if (CauseDamageToHModel(part->my_controller, target, damage, ONE_FIXED, part, &incoming, &point, 0))
		{
			ThrownStaff_Tell("cut off a body part, and flies on", block->DistanceFlown);
			ThrownStaff_FlyOn(dynPtr);
			return;
		}

		ThrownStaff_Tell("hit someone, and stays in them", block->DistanceFlown);

		dynPtr->GravityOn = 0;
		dynPtr->Position = point;
		dynPtr->LinVelocity.vx = dynPtr->LinVelocity.vy = dynPtr->LinVelocity.vz = 0;
		dynPtr->LinImpulse.vx = dynPtr->LinImpulse.vy = dynPtr->LinImpulse.vz = 0;

		block->State = kThrownStaffInABody;
		block->TimeLeft = kStaffStayTime;
		block->BodyPartNumber = partNumber;
		block->PlaceOnBodyPart = placeOnPart;
		block->PointingOnBodyPart = pointingOnPart;
		COPY_NAME(block->BodyName, bodyName);
		return;
	}

	CauseDamageToObject(target, damage, ONE_FIXED, &incoming);

	if (hit->ObMyModule && !hit->ObMorphCtrl)
	{
		ThrownStaff_Tell("hit a fixed part of the level", block->DistanceFlown);
		ThrownStaff_Land(sbPtr, block, &point, &direction);
		return;
	}

	char what[64];

	_snprintf(what, sizeof(what) - 1, "hit something of kind %d that isn't a body, and is gone", (int)target->I_SBtype);
	what[sizeof(what) - 1] = 0;
	ThrownStaff_Tell(what, block->DistanceFlown);
	DestroyAnyStrategyBlock(sbPtr);
}

// One frame in a body: it goes where that body part went. A body that can't be seen just now has no parts to follow, so it waits.
static void ThrownStaff_StayInBody(STRATEGYBLOCK *sbPtr, THROWN_STAFF_BEHAV_BLOCK *block)
{
	STRATEGYBLOCK *const body = FindSBWithName(block->BodyName);

	if (!body)
	{
		ThrownStaff_Tell("the body it was in has gone, so it has too", block->DistanceFlown);
		DestroyAnyStrategyBlock(sbPtr);
		return;
	}

	if (!body->SBdptr || !body->SBdptr->HModelControlBlock)
	{
		return;
	}

	SECTION_DATA *const part = GetThisSectionData_FromID(body->SBdptr->HModelControlBlock->section_data, block->BodyPartNumber);

	if (!part || (part->flags & section_data_notreal))
	{
		ThrownStaff_Tell("the body part it was in has gone, so it has too", block->DistanceFlown);
		DestroyAnyStrategyBlock(sbPtr);
		return;
	}

	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;
	VECTORCH place = block->PlaceOnBodyPart;
	VECTORCH pointing = block->PointingOnBodyPart;

	RotateVector(&place, &part->SecMat);
	RotateVector(&pointing, &part->SecMat);

	dynPtr->Position.vx = part->World_Offset.vx + place.vx;
	dynPtr->Position.vy = part->World_Offset.vy + place.vy;
	dynPtr->Position.vz = part->World_Offset.vz + place.vz;

	if (pointing.vx != 0 || pointing.vy != 0 || pointing.vz != 0)
	{
		MakeMatrixFromDirection(&pointing, &dynPtr->OrientMat);
		MatrixToEuler(&dynPtr->OrientMat, &dynPtr->OrientEuler);
	}
}

void ThrownStaffBehaviour(STRATEGYBLOCK *sbPtr)
{
	THROWN_STAFF_BEHAV_BLOCK *const block = (THROWN_STAFF_BEHAV_BLOCK *)sbPtr->SBdataptr;

	block->TimeLeft -= NormalFrameTime;

	if (block->TimeLeft <= 0)
	{
		if (block->State == kThrownStaffFlying)
		{
			ThrownStaff_Tell("flew until its time ran out, and hit nothing", block->DistanceFlown);
		}
		else if (block->State == kThrownStaffLanded)
		{
			// Down is the way the numbers grow, so this is how far it fell from where it hit.
			char line[96];

			_snprintf(line, sizeof(line) - 1, "staff: its time ran out, lying %d mm below where it hit", sbPtr->DynPtr->Position.vy - block->HeightItHitAt);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
		else
		{
			LogString("staff: its time ran out, still in the body");
		}

		DestroyAnyStrategyBlock(sbPtr);
		return;
	}

	if (block->State == kThrownStaffFlying)
	{
		ThrownStaff_Fly(sbPtr, block);
	}
	else if (block->State == kThrownStaffInABody)
	{
		ThrownStaff_StayInBody(sbPtr, block);
	}
	else
	{
		ThrownStaff_Drop(sbPtr, block);
	}
}

// A thrown staff goes into a save as plain numbers. What comes back from a file is checked, because a file can say anything.
// The way it moves is decided by its stage rather than read back, so a file can't ask for a kind of movement the game can't do.
typedef struct
{
	SAVE_BLOCK_STRATEGY_HEADER header;

	int State;
	int TimeLeft;
	int DistanceFlown;
	int HeightItHitAt;
	int RestHeight;
	char BodyName[SB_NAME_LENGTH];
	int BodyPartNumber;
	VECTORCH PlaceOnBodyPart;
	VECTORCH PointingOnBodyPart;

	VECTORCH Position;
	VECTORCH PrevPosition;
	VECTORCH LinVelocity;
	VECTORCH LinImpulse;
	VECTORCH Pointing;
} THROWN_STAFF_SAVE_BLOCK;

void SaveStrategy_ThrownStaff(STRATEGYBLOCK *sbPtr)
{
	THROWN_STAFF_BEHAV_BLOCK const *const behaviour = (THROWN_STAFF_BEHAV_BLOCK const *)sbPtr->SBdataptr;
	DYNAMICSBLOCK const *const dynPtr = sbPtr->DynPtr;
	THROWN_STAFF_SAVE_BLOCK *block;

	GET_STRATEGY_SAVE_BLOCK(THROWN_STAFF_SAVE_BLOCK, block, sbPtr);

	block->State = behaviour->State;
	block->TimeLeft = behaviour->TimeLeft;
	block->DistanceFlown = behaviour->DistanceFlown;
	block->HeightItHitAt = behaviour->HeightItHitAt;
	block->RestHeight = behaviour->RestHeight;
	COPY_NAME(block->BodyName, behaviour->BodyName);
	block->BodyPartNumber = behaviour->BodyPartNumber;
	block->PlaceOnBodyPart = behaviour->PlaceOnBodyPart;
	block->PointingOnBodyPart = behaviour->PointingOnBodyPart;

	block->Position = dynPtr->Position;
	block->PrevPosition = dynPtr->PrevPosition;
	block->LinVelocity = dynPtr->LinVelocity;
	block->LinImpulse = dynPtr->LinImpulse;
	block->Pointing.vx = dynPtr->OrientMat.mat31;
	block->Pointing.vy = dynPtr->OrientMat.mat32;
	block->Pointing.vz = dynPtr->OrientMat.mat33;
}

static bool ThrownStaff_SavedPlaceIsGood(VECTORCH const *place)
{
	return ThrownStaff_SavedNumberIsGood(place->vx) && ThrownStaff_SavedNumberIsGood(place->vy) && ThrownStaff_SavedNumberIsGood(place->vz);
}

void LoadStrategy_ThrownStaff(SAVE_BLOCK_STRATEGY_HEADER *header)
{
	THROWN_STAFF_SAVE_BLOCK const *const block = (THROWN_STAFF_SAVE_BLOCK const *)header;

	if (header->size != sizeof(*block))
	{
		return;
	}

	if (!ThrownStaff_SavedStageIsGood(block->State, block->TimeLeft)
		|| !ThrownStaff_SavedNumberIsGood(block->RestHeight)
		|| !ThrownStaff_SavedNumberIsGood(block->HeightItHitAt)
		|| !ThrownStaff_SavedPlaceIsGood(&block->Position)
		|| !ThrownStaff_SavedPlaceIsGood(&block->PrevPosition)
		|| !ThrownStaff_SavedPlaceIsGood(&block->LinVelocity)
		|| !ThrownStaff_SavedPlaceIsGood(&block->LinImpulse)
		|| !ThrownStaff_SavedPlaceIsGood(&block->PlaceOnBodyPart)
		|| !ThrownStaff_SavedPlaceIsGood(&block->PointingOnBodyPart)
		|| !ThrownStaff_SavedPlaceIsGood(&block->Pointing))
	{
		LogErrorString("a saved thrown staff has numbers it can't have, so it was left out");
		return;
	}

	VECTORCH place = block->Position;
	STRATEGYBLOCK *const sbPtr = ThrownStaff_Make(&place);

	if (!sbPtr)
	{
		return;
	}

	THROWN_STAFF_BEHAV_BLOCK *const behaviour = (THROWN_STAFF_BEHAV_BLOCK *)sbPtr->SBdataptr;
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;

	behaviour->State = block->State;
	behaviour->TimeLeft = block->TimeLeft;
	behaviour->DistanceFlown = block->DistanceFlown;
	behaviour->HeightItHitAt = block->HeightItHitAt;
	behaviour->RestHeight = block->RestHeight;
	COPY_NAME(behaviour->BodyName, block->BodyName);
	behaviour->BodyPartNumber = block->BodyPartNumber;
	behaviour->PlaceOnBodyPart = block->PlaceOnBodyPart;
	behaviour->PointingOnBodyPart = block->PointingOnBodyPart;

	dynPtr->PrevPosition = block->PrevPosition;
	dynPtr->LinVelocity = block->LinVelocity;
	dynPtr->LinImpulse = block->LinImpulse;

	if (block->Pointing.vx != 0 || block->Pointing.vy != 0 || block->Pointing.vz != 0)
	{
		VECTORCH pointing = block->Pointing;

		Normalise(&pointing);
		MakeMatrixFromDirection(&pointing, &dynPtr->OrientMat);
		MatrixToEuler(&dynPtr->OrientMat, &dynPtr->OrientEuler);
	}

	if (block->State == kThrownStaffLanded)
	{
		dynPtr->GravityOn = dynPtr->Position.vy < block->RestHeight ? 1 : 0;
	}
	else if (block->State == kThrownStaffInABody)
	{
		dynPtr->GravityOn = 0;
	}
}

// Throws the staff: the move has reached the moment it lets go.
static void PredStaff_LetGo(SECTION_DATA *staffInHands)
{
	VECTORCH place = staffInHands->World_Offset;
	STRATEGYBLOCK *const sbPtr = ThrownStaff_Make(&place);

	if (!sbPtr)
	{
		return;
	}

	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;

	// It leaves from his eyes, the way he's looking. The view's matrix goes the other way, so it needs turning over first.
	MATRIXCH facing = Global_VDB_Ptr->VDB_Mat;

	TransposeMatrixCH(&facing);

	dynPtr->Position = Global_VDB_Ptr->VDB_World;
	dynPtr->PrevPosition = dynPtr->Position;
	dynPtr->OrientMat = facing;
	dynPtr->PrevOrientMat = facing;
	MatrixToEuler(&facing, &dynPtr->OrientEuler);

	dynPtr->LinVelocity.vx = MUL_FIXED(facing.mat31, kStaffThrowSpeed);
	dynPtr->LinVelocity.vy = MUL_FIXED(facing.mat32, kStaffThrowSpeed);
	dynPtr->LinVelocity.vz = MUL_FIXED(facing.mat33, kStaffThrowSpeed);

	char line[96];

	_snprintf(line, sizeof(line) - 1, "staff: thrown, heading %d %d %d", facing.mat31, facing.mat32, facing.mat33);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

void PredStaff_Throw(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr)
{
	if (!PlayersWeapon.HModelControlBlock)
	{
		return;
	}

	if (PlayersWeaponHModelController.Sub_Sequence != (int)PHSS_Attack_Primary
		&& HModelSequence_Exists(&PlayersWeaponHModelController, HMSQT_PredatorHUD, (int)PHSS_Attack_Primary))
	{
		InitHModelSequence(&PlayersWeaponHModelController, HMSQT_PredatorHUD, (int)PHSS_Attack_Primary, kStaffThrowTime);
		PlayersWeaponHModelController.Looped = 0;
	}

	if ((PlayersWeaponHModelController.keyframe_flags & 1) && weaponPtr->PrimaryRoundsRemaining > 0)
	{
		SECTION_DATA *const staffInHands = GetThisSectionData(PlayersWeaponHModelController.section_data, kStaffRootSection);

		if (staffInHands)
		{
			PredStaff_LetGo(staffInHands);
		}
	}
}
