// What the host says about the NPCs it runs, and how every other machine draws them. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHY THIS IS HERE: in a network game that plays the same as single player, the host runs every marine, predator, xenoborg, queen and facehugger, and nobody else does. The others only
// need to see them, and be able to shoot them. So each of these has a ghost on the other machines: a model with no brain, which goes where the host says and plays what the host says.
// Aliens have always worked this way, with messages of their own. These kinds share one set of messages, which say which kind they mean.
//
// THE MODEL: it's chosen by the kind, and for a marine or predator by the gun he carries, because a marine is a different model for each weapon. It's built here the same way the kind builds
// its own: see InitMarineBehaviour, InitPredatorBehaviour and the others.
//
// WHAT ISN'T SHOWN YET: sounds other than an NPC's voice, such as a xenoborg's motors, aren't heard on other machines.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "hmodel.h"
#include "bh_ais.h"
#include "bh_marine.h"
#include "bh_predator.h"
#include "bh_xeno.h"
#include "bh_queen.h"
#include "bh_facehugger.h"
#include "bh_agun.h"
#include "bh_corpse.h"
#include "psnd.h"
#include "pldnet.h"
#include "pldghost.h"
#include "npc_kinds.h"
#include "npc_ghosts.h"
#include "los.h"

extern NetID AvPNetID;
extern PLAYER_STATUS *PlayerStatusPtr;
extern DEATH_DATA Marine_Deaths[];
extern DEATH_DATA Predator_Deaths[];
extern DEATH_DATA Xenoborg_Deaths[];
extern HITLOCATIONTABLE *GetThisHitLocationTable(char *id);
extern SECTION *GetNamedHierarchyFromLibrary(const char *rif_name, const char *hier_name);
extern MARINE_WEAPON_DATA *GetThisNPCMarineWeapon(MARINE_NPC_WEAPONS this_id);
extern PREDATOR_WEAPON_DATA *GetThisNPCPredatorWeapon(PREDATOR_NPC_WEAPONS this_id);
extern int Hugger_FaceVictimForTheNetwork(STRATEGYBLOCK *sbPtr);
extern int Hugger_IsDyingForTheNetwork(STRATEGYBLOCK *sbPtr);
extern void Extreme_Gibbing(STRATEGYBLOCK *sbPtr, SECTION_DATA *this_section_data, int intensity);
extern void UpdateAlienAIGhostAnimSequence(STRATEGYBLOCK *sbPtr, HMODEL_SEQUENCE_TYPES type, int subtype, int length, int tweeningtime);

bool NpcGhosts_TypeIsAnNpc(AVP_BEHAVIOUR_TYPE type)
{
	switch (type)
	{
		case I_BehaviourMarine:
		case I_BehaviourPredator:
		case I_BehaviourXenoborg:
		case I_BehaviourQueenAlien:
		case I_BehaviourFaceHugger:
		case I_BehaviourAutoGun:
			return true;

		default:
			return false;
	}
}

// ---- On the host

static int KindOfNpc(STRATEGYBLOCK const *sbPtr)
{
	switch (sbPtr->I_SBtype)
	{
		case I_BehaviourMarine:
		case I_BehaviourSeal:
			return kNpcKindMarine;
		case I_BehaviourPredator:
			return kNpcKindPredator;
		case I_BehaviourXenoborg:
			return kNpcKindXenoborg;
		case I_BehaviourQueenAlien:
			return kNpcKindQueen;
		case I_BehaviourFaceHugger:
			return kNpcKindFacehugger;
		case I_BehaviourAutoGun:
			return kNpcKindSentry;
		default:
			return -1;
	}
}

static int ElevationOf(HMODELCONTROLLER *controller)
{
	DELTA_CONTROLLER const *const delta = Get_Delta_Sequence(controller, "Elevation");

	return delta ? delta->timer : 32767;
}

bool NpcGhosts_FillState(STRATEGYBLOCK *sbPtr, NETMESSAGE_NPCSTATE *state)
{
	int const kind = KindOfNpc(sbPtr);
	HMODELCONTROLLER *controller = NULL;
	DYNAMICSBLOCK const *const dynPtr = sbPtr->DynPtr;

	if (kind < 0 || !sbPtr->SBdataptr || !dynPtr)
	{
		return false;
	}

	memset(state, 0, sizeof(*state));
	state->kind = (unsigned char)kind;

	switch (kind)
	{
		case kNpcKindMarine:
		{
			MARINE_STATUS_BLOCK *const marine = (MARINE_STATUS_BLOCK *)sbPtr->SBdataptr;

			controller = &marine->HModelController;
			state->model = (unsigned char)marine->My_Weapon->id;
			state->clothes = (unsigned char)marine->ClothesSet;
			state->aim = (unsigned short)ElevationOf(controller);

			if (marine->myGunFlash)
			{
				state->flags |= kNpcFlagFiring;
			}

			break;
		}

		case kNpcKindPredator:
		{
			PREDATOR_STATUS_BLOCK *const predator = (PREDATOR_STATUS_BLOCK *)sbPtr->SBdataptr;

			controller = &predator->HModelController;
			state->model = (unsigned char)predator->Selected_Weapon->id;
			state->aim = (unsigned short)ElevationOf(controller);
			state->extra1 = NpcCloak_ToByte(predator->CloakingEffectiveness);
			break;
		}

		case kNpcKindXenoborg:
			controller = &((XENO_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
			break;

		case kNpcKindQueen:
		{
			QUEEN_STATUS_BLOCK *const queen = (QUEEN_STATUS_BLOCK *)sbPtr->SBdataptr;
			DELTA_CONTROLLER const *const attack = queen->attack_delta;

			controller = &queen->HModelController;

			// A swipe is a second animation on top of her walk. 0 says she isn't swiping, so the one she's in goes as one more than its number.
			if (attack && attack->Playing && attack->timer > 0 && attack->timer < ONE_FIXED - 1)
			{
				state->extra1 = (unsigned char)attack->sequence_type;
				state->extra2 = (unsigned char)(attack->sub_sequence + 1);
				state->aim = (unsigned short)attack->timer;
			}

			break;
		}

		case kNpcKindFacehugger:
			// Once it's dying, the message about that has been sent, and the ghost melts by itself
			if (Hugger_IsDyingForTheNetwork(sbPtr))
			{
				return false;
			}

			controller = &((FACEHUGGER_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
			state->extra1 = (unsigned char)Hugger_FaceVictimForTheNetwork(sbPtr);
			break;

		case kNpcKindSentry:
		{
			AUTOGUN_STATUS_BLOCK *const gun = (AUTOGUN_STATUS_BLOCK *)sbPtr->SBdataptr;

			// Once it's been destroyed, the message about that has been sent
			if (gun->behaviourState == I_disabled)
			{
				return false;
			}

			controller = &gun->HModelController;
			state->extra1 = NpcDelta_ToByte(gun->gun_pan ? gun->gun_pan->timer : 32767);
			state->extra2 = NpcDelta_ToByte(gun->gun_tilt ? gun->gun_tilt->timer : 32767);

			if (gun->Firing)
			{
				state->flags |= kNpcFlagFiring;
			}

			break;
		}
	}

	if (!controller)
	{
		return false;
	}

	if (sbPtr->SBDamageBlock.IsOnFire)
	{
		state->flags |= kNpcFlagOnFire;
	}

	if (controller->Looped)
	{
		state->flags |= kNpcFlagSequenceLoops;
	}

	state->Guid = *((int *)(&(sbPtr->SBname[4])));

	// Where it is. Something that has fallen out of every room, such as an NPC made under the floor, is put back where it was
	// at the start of the frame (pvisible.cpp), but this message can be written before that happens. Another machine can't
	// draw anything outside the level, so the place sent is the one the game keeps it at.
	VECTORCH place = dynPtr->Position;
	VECTORCH startOfFrame = dynPtr->PrevPosition;

	if (!ModuleFromPosition(&place, sbPtr->containingModule) && ModuleFromPosition(&startOfFrame, sbPtr->containingModule))
	{
		place = startOfFrame;
	}

	// In the range the message can hold, as the alien's does
	int const most = 4100000;
	int const x = place.vx, y = place.vy, z = place.vz;

	state->xPos = x < -most ? -most : (x > most ? most : x);
	state->yPos = y < -most ? -most : (y > most ? most : y);
	state->zPos = z < -most ? -most : (z > most ? most : z);
	state->xOrient = dynPtr->OrientEuler.EulerX >> NET_EULERSCALESHIFT;
	state->yOrient = dynPtr->OrientEuler.EulerY >> NET_EULERSCALESHIFT;
	state->zOrient = dynPtr->OrientEuler.EulerZ >> NET_EULERSCALESHIFT;

	state->sequence_type = (unsigned char)controller->Sequence_Type;
	state->sub_sequence = (unsigned char)controller->Sub_Sequence;

	// In 256ths of a second, and never nothing, because the receiver refuses that
	int const length = controller->Seconds_For_Sequence >> 8;

	state->sequence_length = (unsigned short)(length < 1 ? 1 : (length > 65535 ? 65535 : length));
	return true;
}

int NpcGhosts_XenoBeamsOf(STRATEGYBLOCK const *sbPtr)
{
	if (sbPtr->I_SBtype == I_BehaviourNetGhost)
	{
		NETGHOSTDATABLOCK const *const ghostData = (NETGHOSTDATABLOCK const *)sbPtr->SBdataptr;

		return ghostData->type == I_BehaviourXenoborg ? ghostData->npcXenoBeams : 0;
	}

	if (sbPtr->I_SBtype != I_BehaviourXenoborg || !sbPtr->SBdataptr)
	{
		return 0;
	}

	XENO_STATUS_BLOCK const *const xeno = (XENO_STATUS_BLOCK const *)sbPtr->SBdataptr;

	return (xeno->TargetingLaser[0].BeamIsOn ? kNpcXenoBeamHead : 0)
		| (xeno->TargetingLaser[1].BeamIsOn ? kNpcXenoBeamLeftArm : 0)
		| (xeno->TargetingLaser[2].BeamIsOn ? kNpcXenoBeamRightArm : 0)
		| (xeno->LeftMainBeam.BeamIsOn ? kNpcXenoBeamLeftGun : 0)
		| (xeno->RightMainBeam.BeamIsOn ? kNpcXenoBeamRightGun : 0);
}

bool NpcGhosts_FillXenoDeltas(STRATEGYBLOCK *sbPtr, NETMESSAGE_NPCXENODELTAS *deltas)
{
	if (sbPtr->I_SBtype != I_BehaviourXenoborg || !sbPtr->SBdataptr)
	{
		return false;
	}

	XENO_STATUS_BLOCK const *const xeno = (XENO_STATUS_BLOCK const *)sbPtr->SBdataptr;

	memset(deltas, 0, sizeof(*deltas));
	deltas->Guid = *((int *)(&(sbPtr->SBname[4])));
	deltas->headPan = NpcDelta_ToByte(xeno->head_pan ? xeno->head_pan->timer : 32767);
	deltas->headTilt = NpcDelta_ToByte(xeno->head_tilt ? xeno->head_tilt->timer : 32767);
	deltas->leftArmPan = NpcDelta_ToByte(xeno->left_arm_pan ? xeno->left_arm_pan->timer : 32767);
	deltas->leftArmTilt = NpcDelta_ToByte(xeno->left_arm_tilt ? xeno->left_arm_tilt->timer : 32767);
	deltas->rightArmPan = NpcDelta_ToByte(xeno->right_arm_pan ? xeno->right_arm_pan->timer : 32767);
	deltas->rightArmTilt = NpcDelta_ToByte(xeno->right_arm_tilt ? xeno->right_arm_tilt->timer : 32767);
	deltas->torsoTwist = NpcDelta_ToByte(xeno->torso_twist ? xeno->torso_twist->timer : 32767);
	deltas->beams = (unsigned char)NpcGhosts_XenoBeamsOf(sbPtr);
	return true;
}

// ---- On every other machine

AVP_BEHAVIOUR_TYPE NpcGhosts_BehaviourOfKind(int kind)
{
	switch (kind)
	{
		case kNpcKindMarine: return I_BehaviourMarine;
		case kNpcKindPredator: return I_BehaviourPredator;
		case kNpcKindXenoborg: return I_BehaviourXenoborg;
		case kNpcKindQueen: return I_BehaviourQueenAlien;
		case kNpcKindSentry: return I_BehaviourAutoGun;
		default: return I_BehaviourFaceHugger;
	}
}

static SECTION *RootOf(AVP_BEHAVIOUR_TYPE type, int model)
{
	switch (type)
	{
		case I_BehaviourMarine:
		{
			MARINE_WEAPON_DATA const *const weapon = GetThisNPCMarineWeapon((MARINE_NPC_WEAPONS)model);

			return weapon ? GetNamedHierarchyFromLibrary(weapon->Riffname, weapon->HierarchyName) : NULL;
		}

		case I_BehaviourPredator:
		{
			PREDATOR_WEAPON_DATA const *const weapon = GetThisNPCPredatorWeapon((PREDATOR_NPC_WEAPONS)model);

			return weapon ? GetNamedHierarchyFromLibrary(weapon->Riffname, weapon->HierarchyName) : NULL;
		}

		case I_BehaviourXenoborg:
			return GetNamedHierarchyFromLibrary("hnpc_xenoborg", "xenobasic");

		case I_BehaviourQueenAlien:
			return GetNamedHierarchyFromLibrary("queen", "Template");

		case I_BehaviourFaceHugger:
			return GetNamedHierarchyFromLibrary("hnpchugger", "Template");

		case I_BehaviourAutoGun:
			return GetNamedHierarchyFromLibrary("sentry", "gun");

		default:
			return NULL;
	}
}

// Puts on the set of clothes and face the host chose for him, the way ChangeToAlternateAccoutrementSet does for the real one.
// The number comes from another machine, and GetHierarchyAlternateShapeSetCollectionNumbered only ever gives back a set the model has.
static void DressTheMarine(NETGHOSTDATABLOCK *ghostData, int set)
{
	extern HIERARCHY_VARIANT_DATA *GetHierarchyAlternateShapeSetCollectionNumbered(const char *rif_name, int number);

	MARINE_WEAPON_DATA const *const weapon = GetThisNPCMarineWeapon((MARINE_NPC_WEAPONS)ghostData->subtype);
	HIERARCHY_VARIANT_DATA const *const clothes = weapon ? GetHierarchyAlternateShapeSetCollectionNumbered(weapon->Riffname, set) : NULL;
	HIERARCHY_SHAPE_REPLACEMENT const *const parts = clothes ? (HIERARCHY_SHAPE_REPLACEMENT const *)clothes->replacements : NULL;

	for (int i = 0; parts && parts[i].replaced_section_name != NULL; i++)
	{
		SECTION_DATA *const section = GetThisSectionData(ghostData->HModelController.section_data, parts[i].replaced_section_name);

		if (section)
		{
			section->Shape = parts[i].replacement_shape;
			section->ShapeNum = parts[i].replacement_shape_index;
			section->replacement_id = parts[i].replacement_id;
			Setup_Texture_Animation_For_Section(section);
		}
	}
}

bool NpcGhosts_MarineIsAnAndroid(int model)
{
	if (model < 0 || model >= MNPCW_End)
	{
		return false;
	}

	MARINE_WEAPON_DATA const *const weapon = GetThisNPCMarineWeapon((MARINE_NPC_WEAPONS)model);

	return weapon && weapon->Android;
}

bool NpcGhosts_ModelIsLoaded(int kind, int model)
{
	return NpcKind_IsKnown(kind) && RootOf(NpcGhosts_BehaviourOfKind(kind), model) != NULL;
}

bool NpcGhosts_MakeModel(NETGHOSTDATABLOCK *ghostData, AVP_BEHAVIOUR_TYPE type, int model)
{
	SECTION *const root = RootOf(type, model);

	if (!root)
	{
		return false;
	}

	Create_HModel(&ghostData->HModelController, root);
	ghostData->currentAnimSequence = 0;
	ghostData->CurrentWeapon = 0;
	ghostData->GunflashSectionPtr = NULL;
	ghostData->hltable = NULL;

	switch (type)
	{
		case I_BehaviourAutoGun:
		{
			InitHModelSequence(&ghostData->HModelController, (int)HMSQT_Xenoborg, (int)XBSS_Powered_Up_Standard, ONE_FIXED);

			static struct { char const *name; int sequence; } const parts[] =
			{
				{ "GunTilt", XBSS_Head_Vertical_Delta },
				{ "GunPan", XBSS_Head_Horizontal_Delta },
			};

			for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
			{
				DELTA_CONTROLLER *const delta = Add_Delta_Sequence(&ghostData->HModelController, (char *)parts[i].name, (int)HMSQT_Xenoborg, parts[i].sequence, 0);

				if (delta)
				{
					delta->timer = 32767;
				}
			}

			break;
		}

		case I_BehaviourMarine:
		{
			MARINE_WEAPON_DATA *const weapon = GetThisNPCMarineWeapon((MARINE_NPC_WEAPONS)model);

			InitHModelSequence(&ghostData->HModelController, (int)HMSQT_MarineRun, (int)MRSS_Standard, ONE_FIXED);
			ghostData->hltable = GetThisHitLocationTable(weapon->HitLocationTableName);

			if (weapon->UseElevation)
			{
				DELTA_CONTROLLER *const delta = Add_Delta_Sequence(&ghostData->HModelController, "Elevation", (int)HMSQT_MarineStand, (int)MSSS_Elevation, 0);

				if (delta)
				{
					delta->timer = 32767;
				}
			}

			if (weapon->GunflashName)
			{
				ghostData->GunflashSectionPtr = GetThisSectionData(ghostData->HModelController.section_data, weapon->GunflashName);
			}

			break;
		}

		case I_BehaviourPredator:
		{
			PREDATOR_WEAPON_DATA *const weapon = GetThisNPCPredatorWeapon((PREDATOR_NPC_WEAPONS)model);

			InitHModelSequence(&ghostData->HModelController, 0, 0, ONE_FIXED);
			ghostData->hltable = GetThisHitLocationTable(weapon->HitLocationTableName);

			if (weapon->UseElevation)
			{
				DELTA_CONTROLLER *const delta = Add_Delta_Sequence(&ghostData->HModelController, "Elevation", (int)HMSQT_PredatorStand, (int)PSSS_Elevation, 0);

				if (delta)
				{
					delta->timer = 32767;
				}
			}

			break;
		}

		case I_BehaviourQueenAlien:
		{
			InitHModelSequence(&ghostData->HModelController, (int)HMSQT_QueenRightStanceTemplate, (int)QRSTSS_Standard, ONE_FIXED);

			// Her swipes: see ShowTheSwipe. It does nothing until she's told to swipe.
			DELTA_CONTROLLER *const attack = Add_Delta_Sequence(&ghostData->HModelController, (char *)"attack", (int)HMSQT_QueenRightStanceTemplate, (int)QRSTSS_LeftSwipe, ONE_FIXED);

			if (attack)
			{
				attack->Playing = 0;
				attack->Active = 0;
			}

			break;
		}

		case I_BehaviourXenoborg:
		{
			static struct { char const *name; int sequence; } const parts[] =
			{
				{ "HeadTilt", XBSS_Head_Vertical_Delta },
				{ "HeadPan", XBSS_Head_Horizontal_Delta },
				{ "LeftArmTilt", XBSS_LeftArm_Vertical_Delta },
				{ "LeftArmPan", XBSS_LeftArm_Horizontal_Delta },
				{ "RightArmTilt", XBSS_RightArm_Vertical_Delta },
				{ "RightArmPan", XBSS_RightArm_Horizontal_Delta },
				{ "TorsoTwist", XBSS_Torso_Delta }
			};

			InitHModelSequence(&ghostData->HModelController, HMSQT_Xenoborg, XBSS_Powered_Up_Standard, ONE_FIXED);
			ghostData->hltable = GetThisHitLocationTable("xenoborg");

			for (unsigned int i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
			{
				DELTA_CONTROLLER *const delta = Add_Delta_Sequence(&ghostData->HModelController, (char *)parts[i].name, (int)HMSQT_Xenoborg, parts[i].sequence, 0);

				if (delta)
				{
					delta->timer = 32767;
				}
			}

			break;
		}

		default:
			InitHModelSequence(&ghostData->HModelController, 0, 0, ONE_FIXED);
			break;
	}

	return true;
}

static void SetDelta(HMODELCONTROLLER *controller, char *name, int timer)
{
	DELTA_CONTROLLER *const delta = Get_Delta_Sequence(controller, name);

	if (delta)
	{
		delta->timer = timer;
		delta->Active = 1;
	}
}

// The swipe a queen is part-way through, held where the host says it's got to. The animation's number comes from another machine, and asking for one her model doesn't have would play her first animation by mistake, so it's looked up first.
static void ShowTheSwipe(NETGHOSTDATABLOCK *ghostData, NETMESSAGE_NPCSTATE const *state)
{
	HMODELCONTROLLER *const model = &ghostData->HModelController;
	DELTA_CONTROLLER *const attack = Get_Delta_Sequence(model, (char *)"attack");

	if (!attack)
	{
		return;
	}

	int const type = state->extra1;
	int const number = (int)state->extra2 - 1;

	if (state->extra2 == 0 || !HModelSequence_Exists(model, type, number))
	{
		attack->Active = 0;
		attack->Playing = 0;
		return;
	}

	if (attack->sequence_type != type || attack->sub_sequence != number)
	{
		Start_Delta_Sequence(attack, type, number, ONE_FIXED);
	}

	attack->timer = state->aim;
	attack->Active = 1;
	attack->Playing = 0;
}

// The marine's own gun flash and firing sound, from his weapon's row
static void ShowFiring(STRATEGYBLOCK *ghost, NETGHOSTDATABLOCK *ghostData, bool firing)
{
	MARINE_WEAPON_DATA const *const weapon = GetThisNPCMarineWeapon((MARINE_NPC_WEAPONS)ghostData->subtype);

	if (!weapon)
	{
		return;
	}

	SECTION_DATA *const section = ghostData->GunflashSectionPtr;

	if (firing && section && ghost->SBdptr)
	{
		if (ghostData->myGunFlash)
		{
			MaintainNPCGunFlashEffect(ghostData->myGunFlash, &section->World_Offset, &section->SecMat);
		}
		else
		{
			ghostData->myGunFlash = AddNPCGunFlashEffect(&section->World_Offset, &section->SecMat, weapon->SfxID);
		}
	}
	else if (ghostData->myGunFlash)
	{
		RemoveNPCGunFlashEffect(ghostData->myGunFlash);
		ghostData->myGunFlash = NULL;
	}

	if (firing)
	{
		if (ghostData->SoundHandle == SOUND_NOACTIVEINDEX && weapon->LoopSound != SID_NOSOUND)
		{
			Sound_Play(weapon->LoopSound, "del", &ghost->DynPtr->Position, &ghostData->SoundHandle);
		}
	}
	else if (ghostData->SoundHandle != SOUND_NOACTIVEINDEX)
	{
		Sound_Stop(ghostData->SoundHandle);

		if (weapon->EndSound != SID_NOSOUND)
		{
			Sound_Play(weapon->EndSound, "d", &ghost->DynPtr->Position);
		}
	}
}

// A facehugger the host says is on somebody's face. If it's mine, this machine draws it over my view and stops me firing, the way the game does for a real one: see PlotFaceHugger.
// faceVictim is that player's place in the list plus one, or nothing, and it comes from the network, so it's checked.
static void ShowOnFace(STRATEGYBLOCK *ghost, int faceVictim)
{
	PLAYER_STATUS *const me = PlayerStatusPtr;
	bool const someone = faceVictim > 0 && faceVictim <= NET_MAXPLAYERS;
	bool const onMe = someone && faceVictim - 1 == PlayerIdInPlayerList(AvPNetID);

	if (onMe && me->IsAlive && me->MyFaceHugger != ghost)
	{
		if (!ghost->SBdptr)
		{
			MakeGhostNear(ghost);
		}

		if (ghost->SBdptr)
		{
			ghost->maintainVisibility = 0;
			me->MyFaceHugger = ghost;
			Sound_Play(SID_ED_FACEHUGGERSLAP, "h");
		}
	}
	else if (!onMe && me->MyFaceHugger == ghost)
	{
		me->MyFaceHugger = NULL;
		ghost->maintainVisibility = 1;
	}

	// The host hides one that's on someone's face, so everybody else does too. Mine is drawn by PlotFaceHugger.
	if (ghost->SBdptr && someone && !onMe)
	{
		ghost->SBdptr->ObFlags |= ObFlag_NotVis;
	}
}

// The second model a marine or predator changes to for some animations, written further down
static SECTION *TemplateModelOf(AVP_BEHAVIOUR_TYPE type, int model);

void NpcGhosts_ApplyState(STRATEGYBLOCK *ghost, NETMESSAGE_NPCSTATE const *state, VECTORCH *position, EULER *orientation)
{
	NETGHOSTDATABLOCK *const ghostData = (NETGHOSTDATABLOCK *)ghost->SBdataptr;

	// The host still has this NPC, so its ghost stays, even if part of the message can't be used. A ghost the host stops
	// talking about goes 8 seconds later.
	ghostData->integrity = GHOST_INTEGRITY;

	// A marine changes model when he pulls a pistol or throws his last bottle. This loses the wounds on his old model, which is cosmetic.
	if (ghostData->type == I_BehaviourMarine && ghostData->subtype != (int)state->model)
	{
		if (!NpcGhosts_ModelIsLoaded(kNpcKindMarine, state->model))
		{
			return;
		}

		if (ghostData->myGunFlash)
		{
			RemoveNPCGunFlashEffect(ghostData->myGunFlash);
			ghostData->myGunFlash = NULL;
		}

		Dispel_HModel(&ghostData->HModelController);
		ghostData->subtype = state->model;
		NpcGhosts_MakeModel(ghostData, ghostData->type, ghostData->subtype);

		// A new model starts in its first clothes
		ghostData->npcClothesDrawn = 0;

		if (ghost->SBdptr)
		{
			ghost->SBdptr->HModelControlBlock = &ghostData->HModelController;
			ProveHModel(ghost->SBdptr->HModelControlBlock, ghost->SBdptr);
		}
	}

	DYNAMICSBLOCK *const dynPtr = ghost->DynPtr;

	dynPtr->PrevPosition = dynPtr->Position;
	dynPtr->PrevOrientMat = dynPtr->OrientMat;

	// As for every ghost: if the place is outside the level, it stays where it last was. The rest of the message is still used.
	MODULE *const module = ModuleFromPosition(position, ghost->containingModule);

	if (module)
	{
		ghost->containingModule = module;
		dynPtr->Position = *position;
	}

	dynPtr->OrientEuler = *orientation;
	CreateEulerMatrix(&dynPtr->OrientEuler, &dynPtr->OrientMat);
	TransposeMatrixCH(&dynPtr->OrientMat);

	// A predator who starts his self-destruct changes to his second model, the Template, because only that one has the
	// animations for it (Predator_Enter_SelfDestruct_State in bh_predator.cpp). So when the host asks for an animation the
	// ghost's model hasn't got and the Template has, the ghost changes the same way. He never changes back, and nor does it.
	if (ghostData->type == I_BehaviourPredator && !HModelSequence_Exists(&ghostData->HModelController, state->sequence_type, state->sub_sequence))
	{
		SECTION *const templateModel = TemplateModelOf(I_BehaviourPredator, ghostData->subtype);

		if (templateModel && HModelSequence_Exists_FromRoot(templateModel, state->sequence_type, state->sub_sequence))
		{
			RemoveAllDeltas(&ghostData->HModelController);
			Transmogrify_HModels(ghost, &ghostData->HModelController, templateModel, 1, 0, 0);
		}
	}

	// The state has the length in 256ths of a second, and the game wants 65536ths
	UpdateAlienAIGhostAnimSequence(ghost, (HMODEL_SEQUENCE_TYPES)state->sequence_type, state->sub_sequence, (int)state->sequence_length << 8, -1);
	ghostData->HModelController.Looped = (state->flags & kNpcFlagSequenceLoops) ? 1 : 0;

	if (ghostData->type == I_BehaviourMarine || ghostData->type == I_BehaviourPredator)
	{
		SetDelta(&ghostData->HModelController, (char *)"Elevation", (int)state->aim);
	}

	if (ghostData->type == I_BehaviourMarine && ghostData->npcClothesDrawn != (int)state->clothes + 1)
	{
		ghostData->npcClothesDrawn = (int)state->clothes + 1;
		DressTheMarine(ghostData, state->clothes);
	}

	if (ghostData->type == I_BehaviourMarine)
	{
		ShowFiring(ghost, ghostData, (state->flags & kNpcFlagFiring) != 0);
	}

	if (ghostData->type == I_BehaviourPredator)
	{
		MaintainGhostCloakingStatus(ghost, NpcCloak_FromByte(state->extra1));
	}

	if (ghostData->type == I_BehaviourFaceHugger)
	{
		ShowOnFace(ghost, state->extra1);
	}

	if (ghostData->type == I_BehaviourQueenAlien)
	{
		ShowTheSwipe(ghostData, state);
	}

	if (ghostData->type == I_BehaviourAutoGun)
	{
		bool const firing = (state->flags & kNpcFlagFiring) != 0;

		SetDelta(&ghostData->HModelController, (char *)"GunPan", NpcDelta_FromByte(state->extra1));
		SetDelta(&ghostData->HModelController, (char *)"GunTilt", NpcDelta_FromByte(state->extra2));

		// The flash needs something to be drawn with, so only near. The sound can be heard from further off.
		HandleGhostAutoGunMuzzleFlash(ghost, firing && ghost->SBdptr ? 1 : 0);
		HandleGhostAutoGunSound(ghost, firing ? 1 : 0);
	}

	MaintainGhostFireStatus(ghost, (state->flags & kNpcFlagOnFire) ? 1 : 0);
	ghostData->onlyValidFar = 0;
}

void NpcGhosts_ApplyXenoDeltas(STRATEGYBLOCK *ghost, NETMESSAGE_NPCXENODELTAS const *deltas)
{
	NETGHOSTDATABLOCK *const ghostData = (NETGHOSTDATABLOCK *)ghost->SBdataptr;

	if (ghostData->type != I_BehaviourXenoborg)
	{
		return;
	}

	HMODELCONTROLLER *const controller = &ghostData->HModelController;

	SetDelta(controller, (char *)"HeadPan", NpcDelta_FromByte(deltas->headPan));
	SetDelta(controller, (char *)"HeadTilt", NpcDelta_FromByte(deltas->headTilt));
	SetDelta(controller, (char *)"LeftArmPan", NpcDelta_FromByte(deltas->leftArmPan));
	SetDelta(controller, (char *)"LeftArmTilt", NpcDelta_FromByte(deltas->leftArmTilt));
	SetDelta(controller, (char *)"RightArmPan", NpcDelta_FromByte(deltas->rightArmPan));
	SetDelta(controller, (char *)"RightArmTilt", NpcDelta_FromByte(deltas->rightArmTilt));
	SetDelta(controller, (char *)"TorsoTwist", NpcDelta_FromByte(deltas->torsoTwist));
	ghostData->npcXenoBeams = deltas->beams;
}

// If the messages stop, his beams stop this long after
static int const kLongestABeamStaysOnWithoutWord = ONE_FIXED;

// Where one beam of a xenoborg's ghost goes. It's what Xeno_MaintainLasers and Xenoborg_MaintainLeftGun in bh_xeno.cpp do for
// the real one: from a small marker on his head or arm, straight out the way it points, to the first thing in the way.
// Only the real one does any damage. This is only what people see.
static void WorkOutOneBeam(STRATEGYBLOCK *ghost, NETGHOSTDATABLOCK *ghostData, int thisBeam, char const *marker, LASER_BEAM_DESC *beam)
{
	beam->BeamIsOn = 0;

	if (!(ghostData->npcXenoBeams & thisBeam))
	{
		return;
	}

	SECTION_DATA *const section = GetThisSectionData(ghostData->HModelController.section_data, (char *)marker);

	if (!section)
	{
		return;
	}

	VECTORCH from = section->World_Offset;
	VECTORCH direction;

	direction.vx = section->SecMat.mat31;
	direction.vy = section->SecMat.mat32;
	direction.vz = section->SecMat.mat33;

	FindPolygonInLineOfSight(&direction, &from, 0, ghost->SBdptr);

	beam->SourcePosition = from;
	beam->BeamHasHitPlayer = LOS_ObjectHitPtr == Player;

	if (LOS_ObjectHitPtr)
	{
		beam->TargetPosition = LOS_Point;
	}
	else
	{
		// It hits nothing, so it stops a long way off, as the real one's does
		beam->TargetPosition.vx = from.vx + (direction.vx >> 3);
		beam->TargetPosition.vy = from.vy + (direction.vy >> 3);
		beam->TargetPosition.vz = from.vz + (direction.vz >> 3);
	}

	beam->BeamIsOn = 1;
}

// Works out a xenoborg ghost's beams for this frame, ready to draw. It says false if there's nothing to draw.
// The drawing code in particle.cpp is the same for the real one and the ghost.
bool NpcGhosts_XenoBeamsToDraw(STRATEGYBLOCK *ghost, LASER_BEAM_DESC *aiming, LASER_BEAM_DESC *leftGun, LASER_BEAM_DESC *rightGun)
{
	NETGHOSTDATABLOCK *const ghostData = (NETGHOSTDATABLOCK *)ghost->SBdataptr;

	if (!ghostData->npcXenoBeams || !ghost->SBdptr || !ghostData->HModelController.section_data)
	{
		return false;
	}

	if (ghostData->integrity < GHOST_INTEGRITY - kLongestABeamStaysOnWithoutWord)
	{
		return false;
	}

	WorkOutOneBeam(ghost, ghostData, kNpcXenoBeamHead, "flash dummyZ", &aiming[0]);
	WorkOutOneBeam(ghost, ghostData, kNpcXenoBeamLeftArm, "flash dummy C", &aiming[1]);
	WorkOutOneBeam(ghost, ghostData, kNpcXenoBeamRightArm, "flash dummy D", &aiming[2]);
	WorkOutOneBeam(ghost, ghostData, kNpcXenoBeamLeftGun, "flash dummy A", leftGun);
	WorkOutOneBeam(ghost, ghostData, kNpcXenoBeamRightGun, "flash dummy B", rightGun);
	return true;
}

// The same as KillAlienAIGhost, for these kinds, each with its own list of ways to die. A number from the network that's on no list gives the first death: see GetThisDeath_FromCode.
// Most of the ways a predator or marine can die have their animation on a second model, called the Template. A real corpse
// changes to that model first, in bh_corpse.cpp. A model that's asked for an animation it hasn't got plays its first one
// instead (GetSequencePointer in hmodel.cpp). So without this, a ghost stood there while the real one fell to the floor.
// This finds that second model. It's NULL if this kind has none, or this level hasn't got it.
static SECTION *TemplateModelOf(AVP_BEHAVIOUR_TYPE type, int model)
{
	if (type == I_BehaviourPredator)
	{
		return GetNamedHierarchyFromLibrary("hnpcpredator", "Template");
	}

	if (type == I_BehaviourMarine)
	{
		MARINE_WEAPON_DATA const *const weapon = GetThisNPCMarineWeapon((MARINE_NPC_WEAPONS)model);

		return weapon ? GetNamedHierarchyFromLibrary(weapon->Riffname, weapon->TemplateName) : NULL;
	}

	return NULL;
}

void NpcGhosts_Kill(STRATEGYBLOCK *ghost, int deathCode, int deathTime, int gibbFactor)
{
	NETGHOSTDATABLOCK *const ghostData = (NETGHOSTDATABLOCK *)ghost->SBdataptr;
	AVP_BEHAVIOUR_TYPE const type = ghostData->type;
	int const model = ghostData->subtype;

	if (!NpcGhosts_TypeIsAnNpc(type))
	{
		return;
	}

	if (ghostData->onlyValidFar)
	{
		RemoveGhost(ghost);
		return;
	}

	DEATH_DATA *deaths = NULL;

	switch (type)
	{
		case I_BehaviourMarine: deaths = Marine_Deaths; break;
		case I_BehaviourPredator: deaths = Predator_Deaths; break;
		case I_BehaviourXenoborg: deaths = Xenoborg_Deaths; break;
		default: break;
	}

	DEATH_DATA const *thisDeath = deaths ? GetThisDeath_FromCode(&ghostData->HModelController, deaths, deathCode) : NULL;

	if (ghostData->SoundHandle != SOUND_NOACTIVEINDEX) Sound_Stop(ghostData->SoundHandle);
	if (ghostData->SoundHandle2 != SOUND_NOACTIVEINDEX) Sound_Stop(ghostData->SoundHandle2);
	if (ghostData->SoundHandle3 != SOUND_NOACTIVEINDEX) Sound_Stop(ghostData->SoundHandle3);
	if (ghostData->SoundHandle4 != SOUND_NOACTIVEINDEX) Sound_Stop(ghostData->SoundHandle4);

	if (ghostData->myGunFlash)
	{
		RemoveNPCGunFlashEffect(ghostData->myGunFlash);
		ghostData->myGunFlash = NULL;
	}

	ghostData->type = I_BehaviourNetCorpse;
	ghostData->IOType = (INANIMATEOBJECT_TYPE)ghostData->subtype;
	ghostData->subtype = type;
	ghostData->timer = deathTime;

	// A dead predator is drawn solid on the machine that ran it, because it isn't a predator any more. This is still a ghost, and the game draws any ghost by its cloak, so it has to lose it here or it stays cloaked until it melts.
	ghostData->CloakingEffectiveness = 0;

	if (ghostData->HModelController.Deltas)
	{
		RemoveAllDeltas(&ghostData->HModelController);
	}

	if (thisDeath && thisDeath->Template)
	{
		SECTION *const templateModel = TemplateModelOf(type, model);

		if (templateModel)
		{
			// The same call a real corpse makes. Any part the new model doesn't have falls off as debris.
			Transmogrify_HModels(ghost, &ghostData->HModelController, templateModel, 1, 0, 0);
		}
		else
		{
			// The first way on each list is on the model it already has, and falling down the plain way beats standing there
			thisDeath = deaths;
		}
	}

	if (thisDeath)
	{
		UpdateAlienAIGhostAnimSequence(ghost, (HMODEL_SEQUENCE_TYPES)thisDeath->Sequence_Type, thisDeath->Sub_Sequence, thisDeath->Sequence_Length, thisDeath->TweeningTime);
	}

	// These two have no way-of-dying list. A real facehugger plays this as it dies, in KillFaceHugger. A real gun stops its animation
	// and has pieces break off, in the code that sends the message. Each machine does that for itself, with the one animation the
	// gun's message says.
	if (type == I_BehaviourFaceHugger)
	{
		UpdateAlienAIGhostAnimSequence(ghost, HMSQT_Hugger, ghost->SBDamageBlock.IsOnFire ? HSS_DieOnFire : HSS_Dies, FACEHUGGER_DYINGTIME >> 3, ONE_FIXED >> 3);

		// A real facehugger lets go of the player's face the moment it dies (KillFaceHugger). One that was on this machine's player
		// lets go now too: until then his weapon isn't drawn and he can't fire.
		if (PlayerStatusPtr && PlayerStatusPtr->MyFaceHugger == ghost)
		{
			PlayerStatusPtr->MyFaceHugger = NULL;
			ghost->maintainVisibility = 1;
		}
	}
	else if (type == I_BehaviourAutoGun)
	{
		// A gun's death message carries the animation the real one stopped on (KillAGun), which may not be the last one it
		// sent, so the wreck ends in the same pose. The number comes from another machine, and one the model hasn't got
		// would play its first animation by mistake, so it's looked up first.
		int const sequenceType = ghostData->HModelController.Sequence_Type;

		if (HModelSequence_Exists(&ghostData->HModelController, sequenceType, deathCode))
		{
			UpdateAlienAIGhostAnimSequence(ghost, (HMODEL_SEQUENCE_TYPES)sequenceType, deathCode, -1, ONE_FIXED >> 3);
		}

		ghostData->HModelController.Looped = 0;
		KillRandomSections(ghostData->HModelController.section_data, ONE_FIXED >> 1);
	}

	// A marine, predator or xenoborg on the host becomes a corpse, which keeps sending messages and says when it's gone. These two
	// don't, so nothing would end this ghost until it had gone 8 seconds without a message. A facehugger's is only 4 seconds long.
	if (type == I_BehaviourFaceHugger || type == I_BehaviourAutoGun)
	{
		ghostData->integrity = deathTime;
	}

	ghostData->HModelController.LoopAfterTweening = 0;

	// Players can walk through a corpse, and it falls to the floor
	DYNAMICSBLOCK *const dynPtr = ghost->DynPtr;

	dynPtr->OnlyCollideWithEnvironment = 1;
	dynPtr->LinImpulse.vx = dynPtr->LinImpulse.vy = dynPtr->LinImpulse.vz = 0;
	dynPtr->LinVelocity.vx = dynPtr->LinVelocity.vy = dynPtr->LinVelocity.vz = 0;
	dynPtr->UseStandardGravity = 1;
	dynPtr->IsNetGhost = 1;
	dynPtr->ToppleForce = TOPPLE_FORCE_NONE;
	dynPtr->GravityOn = 1;

	if (gibbFactor)
	{
		Extreme_Gibbing(ghost, ghostData->HModelController.section_data, gibbFactor);
	}
}
