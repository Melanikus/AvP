// Other marines' shoulder lamps: see other_lamps.h. Written 2026-11-07 by Melanikus for the AvP port.

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
#include "pldnet.h"
#include "pldghost.h"
#include "view.h"

#include <string.h>

#include "lamp_pick.h"
#include "other_lamps.h"

// Where a lamp sits on a model with none of the parts it's looked for on: this far up from where the body stands, in millimetres. The game's
// numbers go down as things go up, so up is less.
static const int kLampAboveTheBody = -1500;

// The parts of a marine's model the lamp sits on, the first one his model has: his right shoulder pad, else his head. Not every model has both.
static char const *const kLampParts[] = { "Rpad", "head" };

// The lamps with any strength, shown now or fading out. Twice as many as are ever shown, so that a full set fading out and a new full set
// fading in both fit.
static OtherLamp lamps[kLampMostShown * 2];
static int numLamps = 0;

// Which owners were shown last frame, for lamp_pick.h's rule that one shown keeps its place
static unsigned int shownLastFrame[kLampMostShown];
static int numShownLastFrame = 0;

// A number for whose lamp it is that stays the same from frame to frame: one made from the block's name, which the game keeps unique.
// Two names giving the same number only means two lamps share a place in the stay rule, never anything worse.
static unsigned int OwnerOf(STRATEGYBLOCK const *block)
{
	unsigned int number = 2166136261u;

	for (int i = 0; i < SB_NAME_LENGTH; i++)
	{
		number = (number ^ (unsigned char)block->SBname[i]) * 16777619u;
	}

	return number;
}

// The model a block's lamp would sit on, if its lamp is on and it can be shown on this machine. 0 if not.
static HMODELCONTROLLER *LampModelOf(STRATEGYBLOCK *block)
{
	if (!block || !block->SBdataptr || !block->SBdptr || !block->DynPtr || block->SBflags.please_destroy_me)
	{
		return 0;
	}

	if (block->I_SBtype == I_BehaviourMarine)
	{
		MARINE_STATUS_BLOCK *const marine = (MARINE_STATUS_BLOCK *)block->SBdataptr;

		return (marine->lampOn && block->SBDamageBlock.Health > 0) ? &marine->HModelController : 0;
	}

	if (block->I_SBtype == I_BehaviourNetGhost)
	{
		NETGHOSTDATABLOCK *const ghost = (NETGHOSTDATABLOCK *)block->SBdataptr;

		// Not the player this machine is watching through the eyes of, whose own lamp is his, and not ours to draw
		if (ghost->type == I_BehaviourMarinePlayer && MultiplayerObservedPlayer && ghost->playerId == MultiplayerObservedPlayer)
		{
			return 0;
		}

		return (ghost->lampOn && (ghost->type == I_BehaviourMarine || ghost->type == I_BehaviourMarinePlayer)) ? &ghost->HModelController : 0;
	}

	return 0;
}

// Where the lamp is and which way it points, from the model as it stands this frame
static void PlaceTheLamp(STRATEGYBLOCK *block, HMODELCONTROLLER *model, OtherLamp *lamp)
{
	DYNAMICSBLOCK const *const body = block->DynPtr;
	SECTION_DATA *part = 0;

	lamp->place = body->Position;
	lamp->place.vy += kLampAboveTheBody;

	if (model->section_data)
	{
		// The model's parts are only where they are this frame once it has been worked out for this frame, as the gun flash does
		ProveHModel(model, block->SBdptr);

		for (int i = 0; i < (int)(sizeof(kLampParts) / sizeof(kLampParts[0])) && !part; i++)
		{
			part = GetThisSectionData(model->section_data, (char *)kLampParts[i]);
		}
	}

	if (part)
	{
		lamp->place = part->World_Offset;
	}

	// The way his body faces. Which way his gun's own part points isn't known yet, so it isn't used: a film will show it.
	lamp->aim.vx = body->OrientMat.mat31;
	lamp->aim.vy = body->OrientMat.mat32;
	lamp->aim.vz = body->OrientMat.mat33;
}

static OtherLamp *FindLamp(unsigned int owner)
{
	for (int i = 0; i < numLamps; i++)
	{
		if (lamps[i].owner == owner)
		{
			return &lamps[i];
		}
	}

	return 0;
}

void OtherLamps_Gather(float seconds)
{
	LampCandidate candidates[kLampMostCandidates];
	STRATEGYBLOCK *blocks[kLampMostCandidates];
	HMODELCONTROLLER *models[kLampMostCandidates];
	int numCandidates = 0;

	// The user's ruling: single player, and the network mode that plays as single player. Anywhere else, every lamp goes at once.
	bool const allowed = Global_VDB_Ptr && (AvP.Network == I_No_Network || SameAsSinglePlayer());

	for (int i = 0; allowed && i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];
		HMODELCONTROLLER *const model = LampModelOf(block);

		if (!model)
		{
			continue;
		}

		if (numCandidates < kLampMostCandidates)
		{
			VECTORCH apart = block->DynPtr->Position;

			apart.vx -= Global_VDB_Ptr->VDB_World.vx;
			apart.vy -= Global_VDB_Ptr->VDB_World.vy;
			apart.vz -= Global_VDB_Ptr->VDB_World.vz;

			candidates[numCandidates].owner = OwnerOf(block);
			candidates[numCandidates].distance = Approximate3dMagnitude(&apart);
			blocks[numCandidates] = block;
			models[numCandidates] = model;
			numCandidates++;
		}
	}

	unsigned int shown[kLampMostShown];
	int const numShown = LampPick_Choose(candidates, numCandidates, shownLastFrame, numShownLastFrame, kLampMostShown, shown);

	memcpy(shownLastFrame, shown, sizeof(unsigned int) * numShown);
	numShownLastFrame = numShown;

	// Every lamp still here fades towards shown or not, the shown ones first, so a crowd fading out can't take their places. One whose marine has
	// gone, or has died, goes at once: there's nowhere left to put it.
	OtherLamp kept[kLampMostShown * 2];
	int numKept = 0;

	for (int pass = 0; pass < 2; pass++)
	{
		for (int i = 0; i < numCandidates; i++)
		{
			bool const isShown = LampPick_Has(shown, numShown, candidates[i].owner);

			if (isShown != (pass == 0))
			{
				continue;
			}

			OtherLamp const *const before = FindLamp(candidates[i].owner);
			float const strength = LampPick_Fade(before ? before->strength : 0.0f, isShown, seconds);

			if (strength <= 0.0f || numKept >= kLampMostShown * 2)
			{
				continue;
			}

			OtherLamp *const lamp = &kept[numKept++];

			lamp->owner = candidates[i].owner;
			lamp->strength = strength;
			lamp->body = blocks[i]->SBdptr;
			PlaceTheLamp(blocks[i], models[i], lamp);
		}
	}

	memcpy(lamps, kept, sizeof(OtherLamp) * numKept);
	numLamps = numKept;
}

int OtherLamps_Count()
{
	return numLamps;
}

OtherLamp const *OtherLamps_At(int which)
{
	return (which >= 0 && which < numLamps) ? &lamps[which] : 0;
}
