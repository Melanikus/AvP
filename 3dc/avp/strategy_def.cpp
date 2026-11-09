// AvP port by Barry Duncan.
// Modified 2026-11-04 by Melanikus for the AvP port:
//   carrying objects to the next level stops if the strategy blocks run out
// Modified 2026-11-06 by Melanikus for the AvP port:
//   in a network game a placed NPC can still be found by the name it has in the level: see npc_level_names.h
//   a level with no block left to carry something over goes on
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the list of placed NPCs' names is emptied whenever every strategy block is destroyed, so after a restart in a network game a switch finds the
//   NPC it's aimed at again. It used to be emptied only when the program started (#126)
//   so is what the log has said about links that find nothing, and about walk-in boxes set off by nearness (#130)
//   a lookup by name follows the list of placed NPCs' names once at most, to the live blocks only (#131)
//   a new name's number passes over any number something alive still has, so when the count goes round no two live things share an id
//   (see sb_name_numbers.h, #139)
//   the blocks kept for a change of environment by lift are taken out (InitPreservedSBs, PreserveStBlocksInModule, SBNeededForNextEnv,
//   AddPreservedSBsToActiveList, TeleportPreservedSBsToNewEnvModule): that change could never run (#135)
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "bh_alien.h"
#include "bh_marine.h"
#include "bh_xeno.h"
#include "bh_corpse.h"
#include "bh_debris.h"
#include "pldnet.h"
#include "tables.h"
#include "logString.h"
#include "npc_level_names.h"
#include "sb_name_numbers.h"
#include "near_wake_game.h"

#include <string.h>

/* 
	this attaches runtime and precompiled object
	strategyblocks
*/

/*** exported globals *************/

int NumActiveStBlocks;
STRATEGYBLOCK  *ActiveStBlockList[maxstblocks];


/*** static globals ***************/

static int NumFreeStBlocks;
static STRATEGYBLOCK *FreeStBlockList[maxstblocks];
static STRATEGYBLOCK **FreeStBlockListPtr = &FreeStBlockList[maxstblocks-1];
STRATEGYBLOCK FreeStBlockData[maxstblocks];
static STRATEGYBLOCK  **ActiveStBlockListPtr = &ActiveStBlockList[0];

unsigned int IncrementalSBname;

static NpcLevelNames TheNpcLevelNames;
static_assert(kNpcNameLength == SB_NAME_LENGTH, "npc_level_names.h has to hold names of the game's length");

/*

 Support functions for Strategy Blocks

*/

void InitialiseStrategyBlocks(void)
{
	STRATEGYBLOCK *FreeBlkPtr = &FreeStBlockData[0];

	FreeStBlockListPtr = &FreeStBlockList[maxstblocks-1];

	for (NumFreeStBlocks=0; NumFreeStBlocks < maxstblocks; NumFreeStBlocks++) 
	{
		FreeStBlockList[NumFreeStBlocks] = FreeBlkPtr;
		FreeBlkPtr->SBflags.destroyed_but_preserved=0;
		#if debug
		FreeBlkPtr->SBIsValid = 0;
		#endif
		FreeBlkPtr++;
	}
	
    /* KJL 17:31:18 11/13/96 - seems like a logical place to initialise dynamics blocks */
    InitialiseDynamicsBlocks();
    
    NumActiveStBlocks = 0;
    ActiveStBlockListPtr = &ActiveStBlockList[0];

	IncrementalSBname=0;
	NpcLevelNames_Clear(&TheNpcLevelNames);
}


STRATEGYBLOCK* AllocateStrategyBlock(void)
{
	STRATEGYBLOCK *FreeBlkPtr = 0;		/* Default to null ptr */
	int *sptr;
	int i;

	if (NumFreeStBlocks) 
	{
		FreeBlkPtr = *FreeStBlockListPtr--;

		NumFreeStBlocks--;	/* One less free block */

		/* Clear the block */
		sptr = (int *)FreeBlkPtr;

		// bjd - CHECK
		for (i = sizeof(STRATEGYBLOCK)/4; i != 0; i--)
		{
			*sptr++ = 0;
		}
	}

	return(FreeBlkPtr);
}


void DeallocateStrategyBlock(STRATEGYBLOCK *sptr)
{
	int a;
	/* Reset name */

	for(a = 0; a < SB_NAME_LENGTH; a++) {	
		sptr->SBname[a] = '\0';
	}													

	FreeStBlockListPtr++;

	*FreeStBlockListPtr = sptr;

	NumFreeStBlocks++;						/* One more free block */
}


STRATEGYBLOCK* CreateActiveStrategyBlock(void)
{
	STRATEGYBLOCK *sb;

	sb = AllocateStrategyBlock();

	if(sb) 
  	{
  		#if debug
  		GLOBALASSERT(sb->SBIsValid == 0);
  		sb->SBIsValid = 1;
  		#endif

  		*ActiveStBlockListPtr++ = sb;
  		NumActiveStBlocks++;
  	}

	return sb;
}


int DestroyActiveStrategyBlock(STRATEGYBLOCK* sb)
{
	int j = -1;
	int i;

	/* If the block ptr is OK, search the Active Blocks List */

	if(sb) 
	{
		for(i = 0; i < NumActiveStBlocks && j!=0; i++) 
		{

			if(ActiveStBlockList[i] == sb) 
			{
				ActiveStBlockList[i] = ActiveStBlockList[NumActiveStBlocks-1];
				NumActiveStBlocks--;
				ActiveStBlockListPtr--;

				if(!sb->SBflags.preserve_until_end_of_level)
				{
					DeallocateStrategyBlock(sb);		/* Back to Free List */
				}
				else
				{
					sb->SBflags.destroyed_but_preserved=1;
				}	
					

				j = 0;												/* Flag OK */
			}
		}
	}

	return(j);
}



STRATEGYBLOCK * AttachNewStratBlock
(
	MODULE* moptr,
	MODULEMAPBLOCK* momptr,
	DISPLAYBLOCK* dptr
)
{
	/*oh for a constructor*/
	/* fails if any of the above has a 
		 stratgey block attached*/

	STRATEGYBLOCK* sptr;
	int i;

	GLOBALASSERT(momptr || dptr);

	sptr = CreateActiveStrategyBlock();
	if (sptr == 0) return 0;

	InitialiseSBValues(sptr);

	for(i = 0; i < SB_NAME_LENGTH; i++)
	{
		sptr->SBname[i] = '\0';
	}

	sptr->SBmomptr = momptr;

	if(moptr)
	{
/*		GLOBALASSERT(!moptr->m_sbptr); HACK*/
		moptr->m_sbptr = sptr;
		sptr->SBmoptr = moptr;
	}

	if(dptr)
	{
		GLOBALASSERT(!dptr->ObStrategyBlock);
		dptr->ObStrategyBlock = sptr;
		sptr->SBdptr = dptr;
	}
	else
	{
		sptr->SBflags.no_displayblock = 1;
	}

	return(sptr);
}


void InitialiseSBValues(STRATEGYBLOCK* sptr)
{
	sptr->I_SBtype = I_BehaviourNull;
	sptr->SBdataptr = (void *)0x0;

	sptr->SBDamageBlock.Health=0;
	sptr->SBDamageBlock.Armour=0;
	sptr->SBDamageBlock.SB_H_flags.AcidResistant=0;
	sptr->SBDamageBlock.SB_H_flags.FireResistant=0;
	sptr->SBDamageBlock.SB_H_flags.ElectricResistant=0;
	sptr->SBDamageBlock.SB_H_flags.PerfectArmour=0;
	sptr->SBDamageBlock.SB_H_flags.ElectricSensitive=0;
	sptr->SBDamageBlock.SB_H_flags.Indestructable=0;
	
	sptr->SBflags.please_destroy_me = 0;
	sptr->SBflags.no_displayblock = 0;
	sptr->SBflags.request_operate = 0;
	sptr->SBflags.preserve_until_end_of_level = 0;
	sptr->SBflags.destroyed_but_preserved = 0;
	sptr->SBflags.not_on_motiontracker = 0;
	// These blocks get used again, so a generator's alien mustn't inherit "placed in the level" from the block's last owner, or the generator miscounts
	sptr->SBflags.placed_in_the_level = 0;
	
	sptr->integrity = 0;

	sptr->maintainVisibility = 0;		  /* patrRWH - function to search thgough the list of active*/
	sptr->containingModule = (MODULE *)0; /* patrstrat blocks and return the pointer*/
	sptr->shapeIndex = 0;				  /* patr*/

	sptr->SBmoptr = (MODULE*)0x0;
	sptr->SBmomptr = (MODULEMAPBLOCK*)0x0;
	sptr->SBmorphctrl = (MORPHCTRL*)0x0;

	sptr->SBdptr=NULL;

	sptr->name=0;
}


// The live strategy block with this name, or nothing
static STRATEGYBLOCK *FindLiveSBWithName(char const *id_name)
{
	int stratblock = NumActiveStBlocks;

	while(--stratblock >= 0)
	{
		STRATEGYBLOCK* sbptr = ActiveStBlockList[stratblock];
		GLOBALASSERT(sbptr);

		if(NAME_ISEQUAL(sbptr->SBname, id_name))
		{
			return sbptr;
		}
	}

	return NULL;
}

/*
RWH - function to search thgough the list of active
strat blocks and return the pointer
*/


STRATEGYBLOCK* FindSBWithName(char* id_name)
{
	int i;
	GLOBALASSERT(NumActiveStBlocks);

	if(!id_name)
		return NULL;

	//If the name is all 0`s I want to return a null pointer - Richard.
	for(i=0;i<SB_NAME_LENGTH;i++)
	{
		if(id_name[i]) break;
	}
	if(i==SB_NAME_LENGTH)
	{
		return NULL;
	}

	// A placed NPC has a different name in a network game, so anything aimed at it by its name in the level is sent on to its new one.
	// npc_level_names.h does that, and reads the list once at most, so this always ends, whatever names a level file holds.
	// Nothing found is nothing: a lift, for one, needs that, to know a station is outside this environment.
	return NpcLevelNames_FindByLevelName(&TheNpcLevelNames, id_name, AvP.Network != I_No_Network, FindLiveSBWithName);
}

void NameFromTheLevel(STRATEGYBLOCK *sbPtr, const char *levelName)
{
	if (AvP.Network == I_No_Network)
	{
		for (int i = 0; i < SB_NAME_LENGTH; i++)
		{
			sbPtr->SBname[i] = levelName[i];
		}

		return;
	}

	// The network messages use part of this name to say which NPC they mean, so it gets a new one, as aliens always have
	AssignNewSBName(sbPtr);
	sbPtr->SBflags.placed_in_the_level = 1;

	if (!NpcLevelNames_Add(&TheNpcLevelNames, levelName, sbPtr->SBname))
	{
		LogString("A level has more placed NPCs than the list of their names holds, so a switch aimed at one of the others won't find it. This isn't logged again until the level starts again.");
	}
}

const char *NpcNameInTheLevel(const STRATEGYBLOCK *sbPtr)
{
	char const *const name = NpcLevelNames_LevelNameFor(&TheNpcLevelNames, sbPtr->SBname);

	return name ? name : sbPtr->SBname;
}

bool BehaviourIsAnNpcTheHostRuns(AVP_BEHAVIOUR_TYPE type)
{
	switch (type)
	{
		case I_BehaviourAlien:
		case I_BehaviourQueenAlien:
		case I_BehaviourFaceHugger:
		case I_BehaviourPredator:
		case I_BehaviourDormantPredator:
		case I_BehaviourXenoborg:
		case I_BehaviourMarine:
		case I_BehaviourSeal:
		case I_BehaviourPredatorAlien:
		case I_BehaviourAutoGun:
			return true;

		default:
			return false;
	}
}

// RWH 5/6/97 these next two functions were 1, changed and 2, added
// to deal with deallocating stblocks at the end of behaviours
// this stops problems with accessing defunct strategy blocks
// via collision reports


void DestroyAnyStrategyBlock(STRATEGYBLOCK *sbptr) 
{
	GLOBALASSERT(sbptr);

	sbptr->SBflags.please_destroy_me = 1;
}


void RemoveDestroyedStrategyBlocks(void)
{
	/*
	Go backwards through the strategy block.
	This should prevent any strategy blocks from being skipped when they
	get shuffled down from the end of the array.
	*/
	int i = NumActiveStBlocks;

	while(i)
	{
		STRATEGYBLOCK* sbptr = ActiveStBlockList[--i];
	
		if(sbptr->SBflags.please_destroy_me)
		{
			RemoveBehaviourStrategy(sbptr);
		} else {
		
			/* Also... gibb aliens? */
			if (sbptr->I_SBtype==I_BehaviourAlien) {

				ALIEN_STATUS_BLOCK *alienStatusPointer;
				LOCALASSERT(sbptr);	
				LOCALASSERT(sbptr->DynPtr);	
		
				alienStatusPointer=(ALIEN_STATUS_BLOCK *)(sbptr->SBdataptr);    

				if (alienStatusPointer->GibbFactor>0) {
					Extreme_Gibbing(sbptr,alienStatusPointer->HModelController.section_data,alienStatusPointer->GibbFactor);
					alienStatusPointer->GibbFactor=0;
				} else if (alienStatusPointer->GibbFactor<0) {
					KillRandomSections(alienStatusPointer->HModelController.section_data,alienStatusPointer->GibbFactor);
					alienStatusPointer->GibbFactor=0;
				}

			} else if (
				(sbptr->I_SBtype==I_BehaviourMarine)
				|| (sbptr->I_SBtype==I_BehaviourSeal)
				) {

				MARINE_STATUS_BLOCK *marineStatusPointer;
				LOCALASSERT(sbptr);	
				LOCALASSERT(sbptr->DynPtr);	
		
				marineStatusPointer=(MARINE_STATUS_BLOCK *)(sbptr->SBdataptr);    

				if (marineStatusPointer->GibbFactor>0) {
					Extreme_Gibbing(sbptr,marineStatusPointer->HModelController.section_data,marineStatusPointer->GibbFactor);
					marineStatusPointer->GibbFactor=0;
				} else if (marineStatusPointer->GibbFactor<0) {
					KillRandomSections(marineStatusPointer->HModelController.section_data,-(marineStatusPointer->GibbFactor));
					marineStatusPointer->GibbFactor=0;
				}
			} else if (sbptr->I_SBtype==I_BehaviourPredator) {
				PREDATOR_STATUS_BLOCK *predatorStatusPointer;
				LOCALASSERT(sbptr);	
				LOCALASSERT(sbptr->DynPtr);	
		
				predatorStatusPointer=(PREDATOR_STATUS_BLOCK *)(sbptr->SBdataptr);    

				if (predatorStatusPointer->GibbFactor>0) {
					Extreme_Gibbing(sbptr,predatorStatusPointer->HModelController.section_data,predatorStatusPointer->GibbFactor);
					predatorStatusPointer->GibbFactor=0;
				} else if (predatorStatusPointer->GibbFactor<0) {
					KillRandomSections(predatorStatusPointer->HModelController.section_data,-(predatorStatusPointer->GibbFactor));
					predatorStatusPointer->GibbFactor=0;
				}
			} else if (sbptr->I_SBtype==I_BehaviourXenoborg) {
				XENO_STATUS_BLOCK *xenoStatusPointer;
				LOCALASSERT(sbptr);	
				LOCALASSERT(sbptr->DynPtr);	

				xenoStatusPointer=(XENO_STATUS_BLOCK *)(sbptr->SBdataptr);    

				if (xenoStatusPointer->GibbFactor>0) {
					Extreme_Gibbing(sbptr,xenoStatusPointer->HModelController.section_data,xenoStatusPointer->GibbFactor);
					xenoStatusPointer->GibbFactor=0;
				} else if (xenoStatusPointer->GibbFactor<0) {
					KillRandomSections(xenoStatusPointer->HModelController.section_data,-(xenoStatusPointer->GibbFactor));
					xenoStatusPointer->GibbFactor=0;
				}
			} else if (sbptr->I_SBtype==I_BehaviourNetCorpse) {

				NETCORPSEDATABLOCK *corpseStatusPointer;
				LOCALASSERT(sbptr);	
				LOCALASSERT(sbptr->DynPtr);	
		
				corpseStatusPointer=(NETCORPSEDATABLOCK *)(sbptr->SBdataptr);    
				if(corpseStatusPointer->GibbFactor)
				{
					/*get a seed for the RNG so that we can send gibbing to other
					network players*/
					int seed=FastRandom();
					SetSeededFastRandom(seed);

					if(AvP.Network != I_No_Network)
					{
						AddNetMsg_Gibbing(sbptr,corpseStatusPointer->GibbFactor,seed);
					}

					if (corpseStatusPointer->GibbFactor>0) {
						Extreme_Gibbing(sbptr,corpseStatusPointer->HModelController.section_data,corpseStatusPointer->GibbFactor);
						corpseStatusPointer->GibbFactor=0;
					} else if (corpseStatusPointer->GibbFactor<0) {
						KillRandomSections(corpseStatusPointer->HModelController.section_data,-(corpseStatusPointer->GibbFactor));
						corpseStatusPointer->GibbFactor=0;
					}
				}
			} else if (sbptr->I_SBtype==I_BehaviourHierarchicalFragment) {

				HDEBRIS_BEHAV_BLOCK *debrisStatusPointer;
				LOCALASSERT(sbptr);	
				LOCALASSERT(sbptr->DynPtr);	
		
				debrisStatusPointer=(HDEBRIS_BEHAV_BLOCK *)(sbptr->SBdataptr);

				if (debrisStatusPointer->GibbFactor>0) {
					Extreme_Gibbing(sbptr,debrisStatusPointer->HModelController.section_data,debrisStatusPointer->GibbFactor);
					debrisStatusPointer->GibbFactor=0;
				} else if (debrisStatusPointer->GibbFactor<0) {
					KillRandomSections(debrisStatusPointer->HModelController.section_data,-(debrisStatusPointer->GibbFactor));
					debrisStatusPointer->GibbFactor=0;
				}
			}
		}
	}
}



void DestroyAllStrategyBlocks(void)
{
	int i = 0;
	int a = NumActiveStBlocks;

	while(i < a)
		{
			RemoveBehaviourStrategy(ActiveStBlockList[i++]);
		}

	//get rid of all the strategyblocks that have been preserved until the end of the level

	for(i=0;i<maxstblocks;i++)
	{
		if(FreeStBlockData[i].SBflags.destroyed_but_preserved)
		{
			FreeStBlockData[i].SBflags.destroyed_but_preserved=0;
			FreeStBlockData[i].SBflags.preserve_until_end_of_level=0;
			DeallocateStrategyBlock(&FreeStBlockData[i]);
		}
	}

	// Every NPC the list of level names leads to is gone now. A level's end and a restart both come here, and a restart makes the NPCs again
	// with new names. A pair left from before would be found first (NpcLevelNames_NetworkNameFor), and send a switch to a name nothing has
	// any more: in a co-op game, the facehugger a switch wakes would never wake (#126).
	NpcLevelNames_Clear(&TheNpcLevelNames);

	// And the next level's links, or this one's made again, get their own lines about any that find nothing (link_misses.h), and their
	// own lines about walk-in boxes set off by a player near a sleeping NPC (near_wake.h)
	LinkMisses_LevelEnds();
	NearWake_LevelEnds();
}

// Whether something alive other than self already has this number and tag in its name: the four bytes from SBname[4], which the network
// uses as a thing's id. The number's three bytes come first, low byte first, as the game writes them, and the tag is the fourth.
static bool NameNumberIsInUse(STRATEGYBLOCK const *self, unsigned int number, char tag)
{
	unsigned char const id[4] = { (unsigned char)(number & 0xff), (unsigned char)((number >> 8) & 0xff), (unsigned char)((number >> 16) & 0xff), (unsigned char)tag };

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const other = ActiveStBlockList[i];

		if (other && other != self && memcmp(&other->SBname[4], id, sizeof(id)) == 0)
		{
			return true;
		}
	}

	return false;
}

void AssignNewSBName(STRATEGYBLOCK *sbPtr)
{
	// The tag first, in the name's last byte: 1 playing alone, or 10 + the player's place in a network game, so two players' names never meet
	if(AvP.Network != I_No_Network)
	{
		//modify name to ensure uniqueness between players
		extern NetID AvPNetID;
		sbPtr->SBname[SB_NAME_LENGTH-1]=+10 + PlayerIdInPlayerList(AvPNetID); /* Just to make sure... */
	}
	else
	{
		sbPtr->SBname[SB_NAME_LENGTH-1]=1; /* Just to make sure... */
	}

	char const tag = sbPtr->SBname[SB_NAME_LENGTH-1];

	// The next number, passing over one that something alive still has (sb_name_numbers.h): after the count goes round, a number can come
	// back while the thing that had it is still there
	IncrementalSBname = SBNameNumber_Next(IncrementalSBname, [sbPtr, tag](unsigned int number) { return NameNumberIsInUse(sbPtr, number, tag); }, maxstblocks);

	// The number's three bytes, low byte first, as the game has always written them, leaving the tag in the fourth
	sbPtr->SBname[4] = (char)(IncrementalSBname & 0xff);
	sbPtr->SBname[5] = (char)((IncrementalSBname >> 8) & 0xff);
	sbPtr->SBname[6] = (char)((IncrementalSBname >> 16) & 0xff);
}

void GivePlayerCloakAway(void) {

	PLAYER_STATUS *playerStatusPtr= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

	playerStatusPtr->cloakPositionGivenAway = 1;
	playerStatusPtr->cloakPositionGivenAwayTimer = PLAYERCLOAK_POSTIONGIVENAWAYTIME;
}
