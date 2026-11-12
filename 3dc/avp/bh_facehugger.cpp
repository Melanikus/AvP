// AvP port by Barry Duncan.
// Modified 2026-11-11 by Melanikus (Third) for the AvP port:
//   a save can't turn the level's walls off for a facehugger; its gravity is as saved, as its leap turns it off
//   (Save_RestoreDynamics, save_checks.h)
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a saved dynamics block is put back through Save_RestoreDynamics, which keeps out what a save must not set.
//   a saved curve of no length is worked out again, because the game divides by its length
//   a value a save has refused is counted for the log
// Modified 2026-11-06 by Melanikus for the AvP port:
//   a network game that plays the same as single player has facehuggers, on the host
//   when this dies on the host, its death target reaches the other machines
//   in a network game a facehugger goes after the nearest player on any machine, and can jump on someone on another machine
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a facehugger whose model isn't loaded isn't made, and the log says which model (MODEL-NULL-1; found through
//   Bobblen's avp-forever, commit 207704e)
/* Patrick 9/7/97 ----------------------------------------------------
  Source file for Facehugger AI behaviour functions....
  --------------------------------------------------------------------*/
#include "3dc.h"
#include "io.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "game_statistics.h"
#include "dynblock.h"
#include "save_checks.h"
#include "dynamics.h"
#include "compiled_shapes.h"
#include "bh_types.h"
#include "bh_ais.h"
#include "pldnet.h"
#include "pldghost.h"
#include "bh_predator.h"
#include "bh_facehugger.h"
#include "bh_debris.h"
#include "pfarlocs.h"
#include "pvisible.h"
#include "weapons.h"
#include "psnd.h"
#include "psndplat.h"
#include "targeting.h"
#include "vdb.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "showcmds.h"
#include "sfx.h"
#include "kzsort.h"
#include "tables.h"

#define HUGGER_STATE_PRINT	0

/* external global variables used in this file */
extern int ModuleArraySize;
extern char *ModuleCurrVisArray;

extern ACTIVESOUNDSAMPLE ActiveSounds[];

/* prototypes for this file */
static void Execute_FHNS_Approach(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_Attack(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_Wait(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_Avoidance(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_Dying(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_Float(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_Jumping(STRATEGYBLOCK *sbPtr);
static void Execute_FHNS_AboutToJump(STRATEGYBLOCK *sbPtr);

void Wake_Hugger(STRATEGYBLOCK *sbPtr);

static int HuggerShouldAttackPlayer(void);
static void SetHuggerAnimationSequence(STRATEGYBLOCK *sbPtr, HUGGER_SUBSEQUENCES seq, int length);
static void KillFaceHugger(STRATEGYBLOCK *sbPtr,DAMAGE_PROFILE *damage);

static void JumpAtPlayer(STRATEGYBLOCK *sbPtr);

extern SECTION *GetHierarchyFromLibrary(const char *rif_name);

/* -------------------------------------------------------------------
   Initilaiser, damage, and visibility functions + behaviour shell
  --------------------------------------------------------------------*/
extern NetID AvPNetID;

// Who it's after, and where they are. See below.
static STRATEGYBLOCK *Hugger_TargetOf(STRATEGYBLOCK *sbPtr);
static int Hugger_ChooseTarget(STRATEGYBLOCK *sbPtr);

// The display block's place for the player on this machine, as it has always used, and the dynamics block's for a ghost, which may have no display block
static VECTORCH Hugger_PlaceOfTarget(STRATEGYBLOCK *sbPtr)
{
	STRATEGYBLOCK *const target = Hugger_TargetOf(sbPtr);

	return target == Player->ObStrategyBlock ? Player->ObWorld : target->DynPtr->Position;
}

void InitFacehuggerBehaviour(void* bhdata, STRATEGYBLOCK *sbPtr)
{
	TOOLS_DATA_FACEHUGGER *toolsData;
	int i;

	LOCALASSERT(bhdata);
	LOCALASSERT(sbPtr);
	toolsData = (TOOLS_DATA_FACEHUGGER *)bhdata;

	/* A network game has no facehuggers, unless it plays the same as single player */
	if(!SameAsSinglePlayer())
	{
		RemoveBehaviourStrategy(sbPtr);
		return;
	}

	/* make the assumption that the loader has initialised the strategy block sensibly...
	so just set the shapeIndex from the tools data & copy the name id*/
	sbPtr->shapeIndex = toolsData->shapeIndex;
	NameFromTheLevel(sbPtr, toolsData->nameID);

	sbPtr->SBdptr=NULL;
	/* create, initialise and attach a dynamics block */
	sbPtr->DynPtr = AllocateDynamicsBlock(DYNAMICS_TEMPLATE_ALIEN_NPC);
	if(sbPtr->DynPtr)
	{
		EULER zeroEuler = {0,0,0};
		DYNAMICSBLOCK *dynPtr = sbPtr->DynPtr;
      	dynPtr->PrevPosition = dynPtr->Position = toolsData->position;
		dynPtr->OrientEuler = zeroEuler;
		CreateEulerMatrix(&dynPtr->OrientEuler, &dynPtr->OrientMat);
		TransposeMatrixCH(&dynPtr->OrientMat);
		/* zero linear velocity in dynamics block */
		dynPtr->LinVelocity.vx = 0;
		dynPtr->LinVelocity.vy = 0;
		dynPtr->LinVelocity.vz = 0;
		dynPtr->Mass=10;
	}
	else
	{
		RemoveBehaviourStrategy(sbPtr);
		return;
	}

	/* Initialise hugger's stats */
	{
		NPC_DATA *NpcData;

		NpcData=GetThisNpcData(I_NPC_FaceHugger);
		LOCALASSERT(NpcData);
		sbPtr->SBDamageBlock.Health=NpcData->StartingStats.Health<<ONE_FIXED_SHIFT;
		sbPtr->SBDamageBlock.Armour=NpcData->StartingStats.Armour<<ONE_FIXED_SHIFT;
		sbPtr->SBDamageBlock.SB_H_flags=NpcData->StartingStats.SB_H_flags;
	}
	/* create, initialise and attach a facehugger data block */
	sbPtr->SBdataptr = (void *)AllocateMem(sizeof(FACEHUGGER_STATUS_BLOCK));
	if(sbPtr->SBdataptr)
	{
		SECTION *root_section;
		FACEHUGGER_STATUS_BLOCK *facehuggerStatus = (FACEHUGGER_STATUS_BLOCK *)sbPtr->SBdataptr;

		// New memory holds whatever was there before
		memset(facehuggerStatus, 0, sizeof(FACEHUGGER_STATUS_BLOCK));

		NPC_InitMovementData(&(facehuggerStatus->moveData));
     	facehuggerStatus->health = FACEHUGGER_STARTING_HEALTH;
   		sbPtr->integrity = facehuggerStatus->health;
		facehuggerStatus->stateTimer = 0;
		facehuggerStatus->DoomTimer = 0;
		facehuggerStatus->CurveRadius = 0;
		facehuggerStatus->CurveLength = 0;
		facehuggerStatus->CurveTimeOut = 0;
		facehuggerStatus->jumping = 0;

		// Nobody yet. New memory holds whatever was there before, and this pointer gets followed.
		facehuggerStatus->Target = NULL;
		memset(facehuggerStatus->Target_SBname, 0, sizeof(facehuggerStatus->Target_SBname));

		// Nothing of its model is made yet, and it makes no sound yet, so if it's taken out below, there's nothing to
		// free or stop. (Its sounds are set again further down, the same way.)
		facehuggerStatus->HModelController.section_data=NULL;
		facehuggerStatus->HModelController.Deltas=NULL;
		facehuggerStatus->soundHandle = SOUND_NOACTIVEINDEX;
		facehuggerStatus->soundHandle2 = SOUND_NOACTIVEINDEX;

		root_section=GetHierarchyFromLibrary("hnpchugger");

		if (!NpcModelIsLoaded(root_section, "a facehugger the level places", "hnpchugger", "Template"))
		{
			RemoveBehaviourStrategy(sbPtr);
			return;
		}

		Create_HModel(&facehuggerStatus->HModelController,root_section);
		InitHModelSequence(&facehuggerStatus->HModelController,0,0,ONE_FIXED);
		ProveHModel_Far(&facehuggerStatus->HModelController,sbPtr);

		if (toolsData->startInactive==0) {
	   		facehuggerStatus->nearBehaviourState = FHNS_Approach;
	   		SetHuggerAnimationSequence(sbPtr,HSS_Stand,ONE_FIXED);
		} else {
	   		facehuggerStatus->nearBehaviourState = FHNS_Floating;
	   		SetHuggerAnimationSequence(sbPtr,HSS_Floats,(ONE_FIXED<<1));
			sbPtr->DynPtr->GravityOn=0;
		}

		facehuggerStatus->soundHandle = SOUND_NOACTIVEINDEX;
		facehuggerStatus->soundHandle2 = SOUND_NOACTIVEINDEX;

		for(i=0;i<SB_NAME_LENGTH;i++) facehuggerStatus->death_target_ID[i] = toolsData->death_target_ID[i];
		facehuggerStatus->death_target_sbptr=0;
		facehuggerStatus->death_target_request=toolsData->death_target_request;
	}
	else
	{
		RemoveBehaviourStrategy(sbPtr);
		return;
	}

}

// Makes a new facehugger during a level, which the game itself never does: they're only ever placed in a level's file. It starts on its feet and walking, and doesn't float in a tube until something wakes it. Everything has to be done
// before its set-up, which can take this block away again. Its name comes from there too. The loader is what turns maintainVisibility on for what's placed in a level, and without it this would never get a body.
void CreateFacehugger(VECTORCH *Position)
{
	MODULE *const room = ModuleFromPosition(Position, (MODULE *)0);

	if (!room)
	{
		return;
	}

	STRATEGYBLOCK *const sbPtr = CreateActiveStrategyBlock();

	if (!sbPtr)
	{
		return;
	}

	TOOLS_DATA_FACEHUGGER toolsData;

	InitialiseSBValues(sbPtr);
	sbPtr->I_SBtype = I_BehaviourFaceHugger;
	sbPtr->maintainVisibility = 1;
	sbPtr->containingModule = room;
	memset(&toolsData, 0, sizeof(toolsData));
	toolsData.position = *Position;
	InitFacehuggerBehaviour(&toolsData, sbPtr);
}

void FacehuggerBehaviour(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;

	LOCALASSERT(sbPtr);
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(facehuggerStatusPointer);

	/* test if we've got a containing module: if we haven't, do nothing.
	This is important as the object could have been marked for deletion by the visibility
	management system...*/
	if(!sbPtr->containingModule)
	{
		if (facehuggerStatusPointer->soundHandle2 != SOUND_NOACTIVEINDEX) {
			Sound_Stop(facehuggerStatusPointer->soundHandle2);
		}
		DestroyAnyStrategyBlock(sbPtr); /* just to make sure */
		return;
	}

	/* set velocity to zero */
	LOCALASSERT(sbPtr->DynPtr);
	sbPtr->DynPtr->LinVelocity.vx = 0;
	sbPtr->DynPtr->LinVelocity.vy = 0;
	sbPtr->DynPtr->LinVelocity.vz = 0;

	if (sbPtr->SBDamageBlock.IsOnFire) {

		CauseDamageToObject(sbPtr,&firedamage,NormalFrameTime,NULL);

		if (facehuggerStatusPointer->soundHandle2==SOUND_NOACTIVEINDEX) {
		 	Sound_Play(SID_FIRE,"dlev",&(sbPtr->DynPtr->Position),&facehuggerStatusPointer->soundHandle2,127);
		} else {
			if (ActiveSounds[facehuggerStatusPointer->soundHandle2].soundIndex!=SID_FIRE) {
				Sound_Stop(facehuggerStatusPointer->soundHandle2);
			} else {
				Sound_Update3d(facehuggerStatusPointer->soundHandle2,&(sbPtr->DynPtr->Position));
			}
		}
	} else {
		Sound_Stop(facehuggerStatusPointer->soundHandle2);
	}

	/* No far behaviour for facehuggerss */
	if(sbPtr->SBdptr)
	{
		if(sbPtr->maintainVisibility) LOCALASSERT(ModuleCurrVisArray[(sbPtr->containingModule->m_index)]);
		switch(facehuggerStatusPointer->nearBehaviourState)
		{
			case(FHNS_Approach):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Approaching...\n");
				#endif
				Execute_FHNS_Approach(sbPtr);
				break;
			}
			case(FHNS_Attack):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Attacking...\n");
				#endif
				Execute_FHNS_Attack(sbPtr);
				break;
			}
			case(FHNS_Wait):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Waiting...\n");
				#endif
				Execute_FHNS_Wait(sbPtr);
				break;
			}
			case(FHNS_Avoidance):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Avoiding...\n");
				#endif
				Execute_FHNS_Avoidance(sbPtr);
				break;
			}
			case(FHNS_Dying):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Dying...\n");
				#endif
				Execute_FHNS_Dying(sbPtr);
				break;
			}
			case(FHNS_Floating):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Floating...\n");
				#endif
				Execute_FHNS_Float(sbPtr);
				break;
			}
			case(FHNS_Jumping):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger Jumping...\n");
				#endif
				Execute_FHNS_Jumping(sbPtr);
				break;
			}
			case(FHNS_AboutToJump):
			{
				#if HUGGER_STATE_PRINT
					PrintDebuggingText("Hugger AboutToJump...\n");
				#endif
				Execute_FHNS_AboutToJump(sbPtr);
				break;
			}
   			default:
			{
				LOCALASSERT(1==0);
				break;
			}
		}
	}

	if((facehuggerStatusPointer->nearBehaviourState == FHNS_Dying)&&(facehuggerStatusPointer->stateTimer <= 0)) {
		if (facehuggerStatusPointer->soundHandle2 != SOUND_NOACTIVEINDEX)	{
			Sound_Stop(facehuggerStatusPointer->soundHandle2);
		}
		DestroyAnyStrategyBlock(sbPtr);
	} else if (facehuggerStatusPointer->DoomTimer>(ONE_FIXED*FACEHUGGER_EXPIRY_TIME)) {
		/* Kill facehugger */
		sbPtr->SBDamageBlock.Health=0;
		if (facehuggerStatusPointer->nearBehaviourState != FHNS_Dying) {
			KillFaceHugger(sbPtr,NULL);
		}
	}

}

void MakeFacehuggerNear(STRATEGYBLOCK *sbPtr)
{
	extern MODULEMAPBLOCK AlienDefaultMap;

	MODULE tempModule;
	DISPLAYBLOCK *dPtr;
	DYNAMICSBLOCK *dynPtr;
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;

	LOCALASSERT(sbPtr);
	dynPtr = sbPtr->DynPtr;
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
    LOCALASSERT(facehuggerStatusPointer);
    LOCALASSERT(dynPtr);
	LOCALASSERT(sbPtr->SBdptr == NULL);

	AlienDefaultMap.MapShape = sbPtr->shapeIndex;
	tempModule.m_mapptr = &AlienDefaultMap;
	tempModule.m_sbptr = (STRATEGYBLOCK*)NULL;
	tempModule.m_numlights = 0;
	tempModule.m_lightarray = (struct lightblock *)0;
	tempModule.m_extraitemdata = (struct extraitemdata *)0;
	tempModule.m_dptr = NULL;
	AllocateModuleObject(&tempModule);
	dPtr = tempModule.m_dptr;
	if(dPtr==NULL) return; /* cannot create displayblock, so leave far */

	sbPtr->SBdptr = dPtr;
	dPtr->ObStrategyBlock = sbPtr;
	dPtr->ObMyModule = NULL;

	/* need to initialise positional information in the new display block */
	dPtr->ObWorld = dynPtr->Position;
	dPtr->ObEuler = dynPtr->OrientEuler;
	dPtr->ObMat = dynPtr->OrientMat;

	/* zero linear velocity in dynamics block */
	sbPtr->DynPtr->LinVelocity.vx = 0;
	sbPtr->DynPtr->LinVelocity.vy = 0;
	sbPtr->DynPtr->LinVelocity.vz = 0;

	/* facehugger data block init */
	facehuggerStatusPointer->stateTimer = 0;
	facehuggerStatusPointer->CurveRadius = 0;
	facehuggerStatusPointer->CurveLength = 0;
	facehuggerStatusPointer->CurveTimeOut = 0;
	facehuggerStatusPointer->jumping = 0;

   	/* state and sequence init */
	//dPtr->ShapeAnimControlBlock = &facehuggerStatusPointer->ShpAnimCtrl;
	dPtr->HModelControlBlock=&facehuggerStatusPointer->HModelController;
   	if ((facehuggerStatusPointer->nearBehaviourState!=FHNS_Floating)
   		&&(facehuggerStatusPointer->nearBehaviourState!=FHNS_Dying)) {
   		if(Hugger_ChooseTarget(sbPtr))
		{
			NPC_InitMovementData(&(facehuggerStatusPointer->moveData));
   			facehuggerStatusPointer->nearBehaviourState = FHNS_Approach;
   			SetHuggerAnimationSequence(sbPtr,HSS_Run,(ONE_FIXED*2)/3);
		}
		else
   		{
			NPC_InitMovementData(&(facehuggerStatusPointer->moveData));
   			facehuggerStatusPointer->nearBehaviourState = FHNS_Wait;
   			SetHuggerAnimationSequence(sbPtr,HSS_Stand,ONE_FIXED);
		}
	}
	ProveHModel(dPtr->HModelControlBlock,dPtr);

}

void MakeFacehuggerFar(STRATEGYBLOCK *sbPtr)
{
	int i;
	LOCALASSERT(sbPtr->SBdptr != NULL);
	/* get rid of the displayblock */
	i = DestroyActiveObject(sbPtr->SBdptr);
	LOCALASSERT(i==0);
	sbPtr->SBdptr = NULL;
	/* zero linear velocity in dynamics block */
	sbPtr->DynPtr->LinVelocity.vx = 0;
	sbPtr->DynPtr->LinVelocity.vy = 0;
	sbPtr->DynPtr->LinVelocity.vz = 0;
}

void FacehuggerIsDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple)
{
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;

	LOCALASSERT(sbPtr);
	//Sound_Play(SID_ACID_SPRAY,"dp",&(sbPtr->DynPtr->Position),(FastRandom() & 255) - 128);

 	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(facehuggerStatusPointer);

	#if 0
	/* reduce facehugger health */
	if(facehuggerStatusPointer->health>0) facehuggerStatusPointer->health -= damage;
	#endif

	if (facehuggerStatusPointer->nearBehaviourState==FHNS_Floating) {
		Wake_Hugger(sbPtr);
	}

	/* check if we've been killed */
	if ( (sbPtr->SBDamageBlock.Health <= 0)&&(facehuggerStatusPointer->nearBehaviourState!=FHNS_Dying) )
	{
		CurrentGameStats_CreatureKilled(sbPtr,NULL);
		KillFaceHugger(sbPtr,damage);
		return;
	}

}

static void KillFaceHugger(STRATEGYBLOCK *sbPtr,DAMAGE_PROFILE *damage)
{
	/* The other machines' ghost of this melts as this does */
	AddNetMsg_NpcKilled(sbPtr, 0, FACEHUGGER_DYINGTIME, 0);

	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	fhugStatusPointer=(FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(fhugStatusPointer);
	LOCALASSERT(dynPtr);

	fhugStatusPointer->nearBehaviourState=FHNS_Dying;
	fhugStatusPointer->stateTimer=FACEHUGGER_DYINGTIME<<1;

	/* Stop any hugger sound if playing */
	if (fhugStatusPointer->soundHandle != SOUND_NOACTIVEINDEX)
	{
		Sound_Stop(fhugStatusPointer->soundHandle);
	}

	//Sound_Play(SID_BUGDIE3,"d",&(dynPtr->Position));

	#if 1
	{
		PLAYER_STATUS *playerStatusPointer= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

		if (playerStatusPointer->MyFaceHugger==sbPtr) {
			/* You cheater! */
			playerStatusPointer->MyFaceHugger=NULL;
		}
	}
	#endif

	/*If face hugger has a death target ,send a request*/
	if(fhugStatusPointer->death_target_sbptr)
	{
		RequestState(fhugStatusPointer->death_target_sbptr,fhugStatusPointer->death_target_request, 0);
		LevelEvents_Happened(NpcNameInTheLevel(sbPtr));
	}

	/* More restrained death. */
	{
		/* switch sequence */
		if (damage) {
			if ( (damage->Impact==0)
			   &&(damage->Cutting==0)
			   &&(damage->Penetrative==0)
			   &&(damage->Fire!=0)
			   &&(damage->Electrical==0)
			   &&(damage->Acid==0))
			{
				SetHuggerAnimationSequence(sbPtr,HSS_DieOnFire,FACEHUGGER_DYINGTIME>>3);
			} else {
				SetHuggerAnimationSequence(sbPtr,HSS_Dies,FACEHUGGER_DYINGTIME>>3);
			}
		} else {
			SetHuggerAnimationSequence(sbPtr,HSS_Dies,FACEHUGGER_DYINGTIME>>3);
		}
		fhugStatusPointer->HModelController.Looped=0;
		fhugStatusPointer->HModelController.LoopAfterTweening=0;
		/* switch state */
		fhugStatusPointer->nearBehaviourState=FHNS_Dying;
		fhugStatusPointer->stateTimer=FACEHUGGER_DYINGTIME;

		/* stop motion */
		LOCALASSERT(sbPtr->DynPtr);
		dynPtr->Friction	= 400000;
		dynPtr->LinImpulse.vx=sbPtr->DynPtr->LinVelocity.vx;
		dynPtr->LinImpulse.vy=sbPtr->DynPtr->LinVelocity.vy;
		dynPtr->LinImpulse.vz=sbPtr->DynPtr->LinVelocity.vz;
		dynPtr->LinVelocity.vx = sbPtr->DynPtr->LinVelocity.vy = sbPtr->DynPtr->LinVelocity.vz = 0;
		/* Okay... */

	}
}


/* -------------------------------------------------------------------
   Behaviour state functions
  --------------------------------------------------------------------*/

static void Execute_FHNS_Approach(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer;
	DYNAMICSBLOCK *dynPtr;
	VECTORCH targetPos;
	//int approachingAirDuct = 0;

	LOCALASSERT(sbPtr);
	fhugStatusPointer=(FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(fhugStatusPointer);
	LOCALASSERT(dynPtr);

	/* Stop any hugger attack sound if playing */
	if (fhugStatusPointer->soundHandle != SOUND_NOACTIVEINDEX &&
			ActiveSounds[fhugStatusPointer->soundHandle].soundIndex != SID_FHUG_MOVE)
	{
		Sound_Stop(fhugStatusPointer->soundHandle);
	}

	/* Start the hugger movement sound if needed */
	if (fhugStatusPointer->soundHandle == SOUND_NOACTIVEINDEX)
	{

		Sound_Play(SID_FHUG_MOVE,"ed",&fhugStatusPointer->soundHandle,&dynPtr->Position);
	}

	/* do climb on walls, etc */
	dynPtr->UseStandardGravity=1;
	/* If you think I'm going to let facehuggers climb on walls for *
	 * ONE SECOND, you are INSANE, Jack!!! */

 	/* target acquisition ? */
	{
		extern DISPLAYBLOCK *Player;
		if (sbPtr->SBDamageBlock.IsOnFire==0) {
			targetPos=Hugger_TargetOf(sbPtr)->DynPtr->Position;
		} else {
			targetPos=Hugger_TargetOf(sbPtr)->DynPtr->Position;

			targetPos.vx+=((FastRandom()&8191)-4096);
			targetPos.vy+=((FastRandom()&8191)-4096);
			targetPos.vz+=((FastRandom()&8191)-4096);
		}
	}

	/* translate target into hugger local space */
	{
		MATRIXCH toLocalSpaceMatrix = dynPtr->OrientMat;
		TransposeMatrixCH(&toLocalSpaceMatrix);

		targetPos.vx -= dynPtr->Position.vx;
		targetPos.vy -= dynPtr->Position.vy;
		targetPos.vz -= dynPtr->Position.vz;
		RotateVector(&targetPos,&toLocalSpaceMatrix);
	}

	/* Fix vy. */
	targetPos.vy=0;

	/* tracking movement */
	{
		int distanceToTarget = Magnitude(&targetPos);
	    if (dynPtr->IsInContactWithFloor)
        {
			int offset;

			if (fhugStatusPointer->CurveTimeOut<=0)
			{
				fhugStatusPointer->CurveLength = distanceToTarget;
				fhugStatusPointer->CurveRadius = ((FastRandom()&16383)-8192)*2;
				fhugStatusPointer->CurveTimeOut= ONE_FIXED*3;
			} else {
				fhugStatusPointer->CurveTimeOut-=NormalFrameTime;
			}

			offset =
				MUL_FIXED
				(
					fhugStatusPointer->CurveRadius,
					GetCos((1024*(distanceToTarget)/fhugStatusPointer->CurveLength)&4095)
				);

			dynPtr->LinVelocity.vx =
				WideMulNarrowDiv
				(
					FACEHUGGER_NEAR_SPEED,
					targetPos.vx,
					distanceToTarget
				)
				-WideMulNarrowDiv
				(
					offset,
					targetPos.vz,
					distanceToTarget
				);


			dynPtr->LinVelocity.vz =
				WideMulNarrowDiv
				(
					FACEHUGGER_NEAR_SPEED,
					targetPos.vz,
					distanceToTarget
				)+
				WideMulNarrowDiv
				(
					offset,
					targetPos.vx,
					distanceToTarget
				);

		 	RotateVector(&dynPtr->LinVelocity,&dynPtr->OrientMat);
			/* align to velocity */

			NPCOrientateToVector(sbPtr,&dynPtr->LinVelocity,NPC_TURNRATE,NULL);

			/* within test for in contact with floor: test if
			jump flag is set- if so switch anim back to run... */
			if(fhugStatusPointer->jumping==1)
			{
				fhugStatusPointer->jumping = 0;
				SetHuggerAnimationSequence(sbPtr,HSS_Run,(ONE_FIXED*2)/3);
			}
		}
	}

	/* is player visible?: if not, go to wait */
	if(!Hugger_ChooseTarget(sbPtr))
	{
		fhugStatusPointer->nearBehaviourState = FHNS_Wait;
		fhugStatusPointer->stateTimer = 0;
		return;
	}

	#if 1
	/* should we jump at the player? */
	if (sbPtr->SBDamageBlock.IsOnFire==0) {
		int distanceToPlayer = VectorDistance(&(dynPtr->Position),&(Hugger_TargetOf(sbPtr)->DynPtr->Position));
		if((distanceToPlayer<=FACEHUGGER_JUMPDISTANCE)&&(dynPtr->IsInContactWithFloor))
		{
			#if 0
			JumpAtPlayer(sbPtr);
			#else
			fhugStatusPointer->nearBehaviourState = FHNS_AboutToJump;
			fhugStatusPointer->stateTimer = 0;
			#endif
			return;
		}
	}
	#endif

	{
		STRATEGYBLOCK *destructableObject = NULL;
		NPC_OBSTRUCTIONREPORT obstruction;
		NPC_IsObstructed(sbPtr,&(fhugStatusPointer->moveData),&obstruction,&destructableObject);
		if(obstruction.environment)
		{
			/* go to avoidance */
			NPC_InitMovementData(&(fhugStatusPointer->moveData));
			NPCGetAvoidanceDirection(sbPtr, &(fhugStatusPointer->moveData.avoidanceDirn),&obstruction);
			fhugStatusPointer->nearBehaviourState = FHNS_Avoidance;
			fhugStatusPointer->stateTimer = NPC_AVOIDTIME;
			/* no sequence change required */
			return;
		}
		if(obstruction.destructableObject)
		{
			LOCALASSERT(destructableObject);

			CauseDamageToObject(destructableObject,&TemplateAmmo[AMMO_ALIEN_OBSTACLE_CLEAR].MaxDamage[AvP.Difficulty], ONE_FIXED,NULL);

		}
	}

	{
		VECTORCH velocityDirection = dynPtr->LinVelocity;
		Normalise(&velocityDirection);

		if(NPC_CannotReachTarget(&(fhugStatusPointer->moveData), &targetPos, &velocityDirection))
		{
			/* go to avoidance */
			NPC_OBSTRUCTIONREPORT obstruction = {1,0,0};
			NPC_InitMovementData(&(fhugStatusPointer->moveData));
			NPCGetAvoidanceDirection(sbPtr, &(fhugStatusPointer->moveData.avoidanceDirn),&obstruction);
			fhugStatusPointer->nearBehaviourState = FHNS_Avoidance;
			fhugStatusPointer->stateTimer = NPC_AVOIDTIME;
			/* no sequence change required */
			return;
		}
	}
}

void PlotFaceHugger(STRATEGYBLOCK *sbPtr) 
{
	VIEWDESCRIPTORBLOCK *VDBPtr = ActiveVDBList[0];
	DYNAMICSBLOCK *dynPtr;
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;

	LOCALASSERT(sbPtr);
	dynPtr=sbPtr->DynPtr;
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(facehuggerStatusPointer);
	LOCALASSERT(dynPtr);
	LOCALASSERT(sbPtr->SBdptr);


	{
		VECTORCH x,y,z;

		x.vx = -VDBPtr->VDB_Mat.mat11;
		x.vy = -VDBPtr->VDB_Mat.mat21;
		x.vz = -VDBPtr->VDB_Mat.mat31;
		y.vx = -VDBPtr->VDB_Mat.mat13;
		y.vy = -VDBPtr->VDB_Mat.mat23;
		y.vz = -VDBPtr->VDB_Mat.mat33;
		z.vx = -VDBPtr->VDB_Mat.mat12;
		z.vy = -VDBPtr->VDB_Mat.mat22;
		z.vz = -VDBPtr->VDB_Mat.mat32;

		Normalise(&x);
		Normalise(&y);
		Normalise(&z);

		dynPtr->OrientMat.mat11 = x.vx;
		dynPtr->OrientMat.mat12 = x.vy;
		dynPtr->OrientMat.mat13 = x.vz;
		dynPtr->OrientMat.mat21 = y.vx;
		dynPtr->OrientMat.mat22 = y.vy;
		dynPtr->OrientMat.mat23 = y.vz;
		dynPtr->OrientMat.mat31 = z.vx;
		dynPtr->OrientMat.mat32 = z.vy;
		dynPtr->OrientMat.mat33 = z.vz;
	}

	/* set position */
	dynPtr->Position.vx = 0;
	dynPtr->Position.vz = FACEHUGGER_ATTACKZOFFSET/4;
	dynPtr->Position.vy = FACEHUGGER_ATTACKYOFFSET;
	{
		MATRIXCH myMat = VDBPtr->VDB_Mat;
		TransposeMatrixCH(&myMat);
		RotateVector(&(dynPtr->Position), &(myMat));
	}

	dynPtr->Position.vx += VDBPtr->VDB_World.vx;
	dynPtr->Position.vy += VDBPtr->VDB_World.vy;
	dynPtr->Position.vz += VDBPtr->VDB_World.vz;
	sbPtr->SBdptr->ObFlags&=~ObFlag_NotVis;

	sbPtr->SBdptr->ObWorld = dynPtr->Position;
	sbPtr->SBdptr->ObMat = dynPtr->OrientMat;

	ProveHModel(sbPtr->SBdptr->HModelControlBlock,sbPtr->SBdptr);
	RenderThisDisplayblock(sbPtr->SBdptr);

	sbPtr->SBdptr->ObFlags|=ObFlag_NotVis;

}

static void Execute_FHNS_Attack(STRATEGYBLOCK *sbPtr)
{
	DYNAMICSBLOCK *dynPtr;
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;

	LOCALASSERT(sbPtr);
	dynPtr=sbPtr->DynPtr;
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(facehuggerStatusPointer);
	LOCALASSERT(dynPtr);
	LOCALASSERT(sbPtr->SBdptr);

	/* Play the hugger attack loop sound */
	if (facehuggerStatusPointer->soundHandle == SOUND_NOACTIVEINDEX)
	{
		Sound_Play(SID_FHUG_ATTACKLOOP,"edl",&facehuggerStatusPointer->soundHandle,&dynPtr->Position);
	}

	textprint("face hugger attack \n");

	/* In a network game the player it's on can leave or die, and a player on another machine has no one but this to get it off them */
	if (AvP.Network != I_No_Network)
	{
		STRATEGYBLOCK *const victim = facehuggerStatusPointer->Target;

		if (!victim || !NAME_ISEQUAL(victim->SBname, facehuggerStatusPointer->Target_SBname) || NPC_IsDead(victim))
		{
			KillFaceHugger(sbPtr, NULL);
			return;
		}
	}

	/* Make not vis */

	sbPtr->SBdptr->ObFlags|=ObFlag_NotVis;

	/* do damage */
	facehuggerStatusPointer->DoomTimer += NormalFrameTime;
	facehuggerStatusPointer->stateTimer -= NormalFrameTime;
	if(facehuggerStatusPointer->stateTimer <= 0)
	{
		facehuggerStatusPointer->stateTimer = FACEHUGGER_NEARATTACKTIME;
		CauseDamageToObject(Hugger_TargetOf(sbPtr), &TemplateAmmo[AMMO_FACEHUGGER].MaxDamage[AvP.Difficulty], ONE_FIXED,NULL);
		/* FRI? */
	}

}

static void Execute_FHNS_Wait(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;

	LOCALASSERT(sbPtr);
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(facehuggerStatusPointer);

	if(Hugger_ChooseTarget(sbPtr))
	{
		NPC_InitMovementData(&(facehuggerStatusPointer->moveData));
   		facehuggerStatusPointer->nearBehaviourState = FHNS_Approach;
		facehuggerStatusPointer->stateTimer = 0;
		facehuggerStatusPointer->CurveTimeOut = 0;
		SetHuggerAnimationSequence(sbPtr,HSS_Run,(ONE_FIXED*2)/3);
	}
}

static void Execute_FHNS_Avoidance(STRATEGYBLOCK *sbPtr)
{
	int terminateState = 0;
	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	fhugStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(fhugStatusPointer);
	LOCALASSERT(dynPtr);

	/* Stop any hugger attack sound if playing */
	if (fhugStatusPointer->soundHandle != SOUND_NOACTIVEINDEX &&
			ActiveSounds[fhugStatusPointer->soundHandle].soundIndex != SID_FHUG_MOVE)
	{
		Sound_Stop(fhugStatusPointer->soundHandle);
	}

	/* Start the hugger movement sound if needed */
	if (fhugStatusPointer->soundHandle == SOUND_NOACTIVEINDEX)
	{
		Sound_Play(SID_FHUG_MOVE,"ed",&fhugStatusPointer->soundHandle,&dynPtr->Position);
	}

	/* set velocity */
	LOCALASSERT((fhugStatusPointer->moveData.avoidanceDirn.vx!=0)||
				(fhugStatusPointer->moveData.avoidanceDirn.vy!=0)||
				(fhugStatusPointer->moveData.avoidanceDirn.vz!=0));
	NPCSetVelocity(sbPtr, &(fhugStatusPointer->moveData.avoidanceDirn), (FACEHUGGER_NEAR_SPEED));

	/* next, decrement state timer */
	fhugStatusPointer->stateTimer -= NormalFrameTime;
	if(fhugStatusPointer->stateTimer <= 0) terminateState = 1;

	/* and check for an impeding collision */
	{
		STRATEGYBLOCK *destructableObject = NULL;
		NPC_OBSTRUCTIONREPORT obstruction;
		NPC_IsObstructed(sbPtr,&(fhugStatusPointer->moveData),&obstruction,&destructableObject);
		if(obstruction.anySingleObstruction)
		{
			terminateState = 1;
		}
	}

	if(terminateState)
	{
		if(Hugger_ChooseTarget(sbPtr))
		{
			/* switch to approach */
			NPC_InitMovementData(&(fhugStatusPointer->moveData));
			fhugStatusPointer->nearBehaviourState = FHNS_Approach;
			fhugStatusPointer->stateTimer = 0;
			/* no sequence change required */
		}
		else
		{
			/* switch to wait */
			NPC_InitMovementData(&(fhugStatusPointer->moveData));
			fhugStatusPointer->nearBehaviourState = FHNS_Wait;
			fhugStatusPointer->stateTimer = 0;
			/* no sequence change required */
		}
	}
}

/* -------------------------------------------------------------------
FaceHugger behaviour support functions
  --------------------------------------------------------------------*/
static void SetHuggerAnimationSequence(STRATEGYBLOCK *sbPtr, HUGGER_SUBSEQUENCES seq, int length)
{

	FACEHUGGER_STATUS_BLOCK *fhugStatus=(FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);

	InitHModelTweening(&fhugStatus->HModelController,(ONE_FIXED>>3),(int)HMSQT_Hugger,(int)seq,length,1);

	if (seq==HSS_Jump) fhugStatus->HModelController.LoopAfterTweening=0;

}

// The player this one is after, as long as that block is still theirs. In single player, or before it has chosen, that's the player on this machine.
static STRATEGYBLOCK *Hugger_TargetOf(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *const status = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);

	if (status->Target && NAME_ISEQUAL(status->Target->SBname, status->Target_SBname))
	{
		return status->Target;
	}

	return Player->ObStrategyBlock;
}

// Is another facehugger already on this player's face?
static bool Hugger_SomeoneIsOnThem(STRATEGYBLOCK const *player)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const other = ActiveStBlockList[i];

		if (other->I_SBtype == I_BehaviourFaceHugger && other->SBdataptr)
		{
			FACEHUGGER_STATUS_BLOCK const *const status = (FACEHUGGER_STATUS_BLOCK const *)other->SBdataptr;

			if (status->nearBehaviourState == FHNS_Attack && status->Target == player)
			{
				return true;
			}
		}
	}

	return false;
}

static int HuggerShouldAttackPlayer(void);

// Chooses who to go after, and says whether there's anybody. In a network game that's the nearest player who's alive, isn't an Alien, and hasn't one on their face already.
static int Hugger_ChooseTarget(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *const status = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	STRATEGYBLOCK *chosen = NULL;

	if (HuggerShouldAttackPlayer() && (AvP.Network == I_No_Network || !NPC_IsDead(Player->ObStrategyBlock)))
	{
		chosen = Player->ObStrategyBlock;
	}

	if (AvP.Network != I_No_Network)
	{
		int nearest = chosen ? VectorDistance(&sbPtr->DynPtr->Position, &chosen->DynPtr->Position) : 0;

		for (int i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const other = ActiveStBlockList[i];

			if (other->I_SBtype != I_BehaviourNetGhost || !other->DynPtr || !NPC_IsAPlayer(other) || NPC_IsDead(other)
				|| ((NETGHOSTDATABLOCK *)other->SBdataptr)->type == I_BehaviourAlienPlayer || Hugger_SomeoneIsOnThem(other))
			{
				continue;
			}

			int const distance = VectorDistance(&sbPtr->DynPtr->Position, &other->DynPtr->Position);

			if (!chosen || distance < nearest)
			{
				chosen = other;
				nearest = distance;
			}
		}
	}

	status->Target = chosen;

	if (chosen)
	{
		COPY_NAME(status->Target_SBname, chosen->SBname);
	}

	return chosen != NULL;
}

int Hugger_FaceVictimForTheNetwork(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK const *const status = (FACEHUGGER_STATUS_BLOCK const *)(sbPtr->SBdataptr);

	if (status->nearBehaviourState != FHNS_Attack)
	{
		return 0;
	}

	STRATEGYBLOCK *const target = Hugger_TargetOf(sbPtr);
	int const place = target->I_SBtype == I_BehaviourNetGhost ? PlayerIdInPlayerList(((NETGHOSTDATABLOCK *)target->SBdataptr)->playerId) : PlayerIdInPlayerList(AvPNetID);

	return (place >= 0 && place < NET_MAXPLAYERS) ? place + 1 : 0;
}

int Hugger_IsDyingForTheNetwork(STRATEGYBLOCK *sbPtr)
{
	return ((FACEHUGGER_STATUS_BLOCK const *)(sbPtr->SBdataptr))->nearBehaviourState == FHNS_Dying;
}

static int HuggerShouldAttackPlayer(void)
{
	/* test for player being cloaked */
	{
		PLAYER_STATUS *playerStatusPointer= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);
		LOCALASSERT(playerStatusPointer);

		if((playerStatusPointer->cloakOn==1)&&(playerStatusPointer->cloakPositionGivenAway==0)) {

			return 1;
			/* Was '0'. */
		}

		if (playerStatusPointer->MyFaceHugger!=NULL) return(0);
	}

	/* test for player being an alien */
	if(AvP.PlayerType==I_Alien) return 0;

	return 1;
}

static void JumpAtPlayer(STRATEGYBLOCK *sbPtr)
{

	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer=(FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	/* Set up jump! */

    if (sbPtr->DynPtr->IsInContactWithFloor==0) {
		/* Jump Not! */
		return;
	}

	/* set animation */
	SetHuggerAnimationSequence(sbPtr, HSS_Jump, (ONE_FIXED));
	fhugStatusPointer->jumping = 1;
	fhugStatusPointer->CurveTimeOut = 0;

	fhugStatusPointer->HModelController.Looped=0;
	fhugStatusPointer->HModelController.LoopAfterTweening=0;

	fhugStatusPointer->nearBehaviourState = FHNS_Jumping;
	fhugStatusPointer->stateTimer=0;

}

static void FHugApplyPounceImpulse(STRATEGYBLOCK *sbPtr) {

	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer;
	DYNAMICSBLOCK *dynPtr;
	VECTORCH pounceVector,targetPoint;
	int dist,speed;

	LOCALASSERT(sbPtr);
	fhugStatusPointer=(FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr = sbPtr->DynPtr;
	LOCALASSERT(fhugStatusPointer);
	LOCALASSERT(dynPtr);

	if (Hugger_TargetOf(sbPtr) == Player->ObStrategyBlock)
	{
		GetTargetingPointOfObject(Player,&targetPoint);
	}
	else
	{
		GetTargetingPointOfObject_Far(Hugger_TargetOf(sbPtr),&targetPoint);
	}

	dist = VectorDistance(&(dynPtr->Position),&targetPoint);

	/* Apply a correction based on range. */
	targetPoint.vy-=(dist>>3);

	pounceVector.vx=targetPoint.vx-dynPtr->Position.vx;
	pounceVector.vy=targetPoint.vy-dynPtr->Position.vy;
	pounceVector.vz=targetPoint.vz-dynPtr->Position.vz;

	Normalise(&pounceVector);
	/* Must jump at least a little bit upwards. */
	if (pounceVector.vy>-10000) {
		pounceVector.vy=-10000;
	}

	speed=FACEHUGGER_JUMP_SPEED;

	pounceVector.vx=MUL_FIXED(speed,pounceVector.vx);
	pounceVector.vy=MUL_FIXED(speed,pounceVector.vy);
	pounceVector.vz=MUL_FIXED(speed,pounceVector.vz);

	sbPtr->DynPtr->LinImpulse=pounceVector;

}

static void Execute_FHNS_Jumping(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	fhugStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(fhugStatusPointer);
	LOCALASSERT(dynPtr);

	/* don't climb on walls, etc */
	dynPtr->UseStandardGravity=1;

	/* Firstly, are we actually pouncing yet? */
	/* NearStateTimer is a status flag. */

	if (fhugStatusPointer->stateTimer!=2) {
		/* Still tweening? */

		if (fhugStatusPointer->HModelController.keyframe_flags) {
			/* We have the flag. */
			fhugStatusPointer->HModelController.Playing=0;
			fhugStatusPointer->stateTimer=1;
		}

		if (fhugStatusPointer->stateTimer==1) {
			/* We've finished!  Are we facing right? */
			VECTORCH orientationDirn;
			int i;

			orientationDirn.vx = Hugger_PlaceOfTarget(sbPtr).vx - dynPtr->Position.vx;
			orientationDirn.vy = 0;
			orientationDirn.vz = Hugger_PlaceOfTarget(sbPtr).vz - dynPtr->Position.vz;
			i = NPCOrientateToVector(sbPtr, &orientationDirn,NPC_TURNRATE<<2,NULL);

			if (i==0) {
				/* Still not right!  Wait for proper facing. */
				fhugStatusPointer->HModelController.Playing=0;
				//CheckPounceIntegrity(sbPtr);
				return;
			} else {
				/* Okay, pounce! */

				FHugApplyPounceImpulse(sbPtr);

				fhugStatusPointer->HModelController.Playing=1;
				fhugStatusPointer->stateTimer=2;
			}
		} else {
			/* Yup, still tweening.  Check state validity? */
			//CheckPounceIntegrity(sbPtr);
		}
	} else {
		/* We must be in the jump.  Can't break out of this until we hit something. */
		COLLISIONREPORT *reportPtr = dynPtr->CollisionReportPtr;

		while (reportPtr)
		{
			STRATEGYBLOCK *hitSbPtr = reportPtr->ObstacleSBPtr;
			/* You know what?  Just did! */

			fhugStatusPointer->nearBehaviourState=FHNS_Approach;
			fhugStatusPointer->stateTimer = 0;
			fhugStatusPointer->CurveTimeOut = 0;

			if (hitSbPtr) {
				if ((hitSbPtr==Hugger_TargetOf(sbPtr))&&(sbPtr->SBDamageBlock.IsOnFire==0)) {
					/* Got him, My Precious, we've Got Him! */

					/* Someone on another machine hears this on their own machine */
					if (hitSbPtr==Player->ObStrategyBlock) {
						Sound_Play(SID_ED_FACEHUGGERSLAP,"h");
					}

					/* Test for attach, or merely bite? */
					fhugStatusPointer->nearBehaviourState = FHNS_Attack;
					fhugStatusPointer->stateTimer = FACEHUGGER_NEARATTACKTIME;
					SetHuggerAnimationSequence(sbPtr,HSS_Attack,ONE_FIXED);
					dynPtr->DynamicsType = DYN_TYPE_NO_COLLISIONS; 	/* turn off collisons */
					dynPtr->GravityOn = 0;							/* turn off gravity */
					sbPtr->maintainVisibility = 0;					/* turn off visibility support- be carefull! */

					/* Attach to player! A player on another machine finds out from the host's next message about this facehugger: see NpcGhosts_ApplyState */
					if (hitSbPtr==Player->ObStrategyBlock) {
						PLAYER_STATUS *playerStatusPointer= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

						playerStatusPointer->MyFaceHugger=sbPtr;
					}
					return;

				}
			}

			reportPtr = reportPtr->NextCollisionReportPtr;
		}

	}

}

static void Execute_FHNS_Dying(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *huggerStatusPointer;
	DISPLAYBLOCK *dispPtr;

	LOCALASSERT(sbPtr);
	huggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(huggerStatusPointer);

	huggerStatusPointer->stateTimer -= NormalFrameTime;

	dispPtr=sbPtr->SBdptr;

	if (dispPtr)
	{
		dispPtr->SpecialFXFlags |= SFXFLAG_MELTINGINTOGROUND;
		dispPtr->ObFlags2 = huggerStatusPointer->stateTimer/2;
	}
}

static void Execute_FHNS_Float(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(facehuggerStatusPointer);
	LOCALASSERT(dynPtr);

	dynPtr->LinVelocity.vx=0;
	dynPtr->LinVelocity.vy=0;
	dynPtr->LinVelocity.vz=0;

	dynPtr->LinImpulse.vx=0;
	dynPtr->LinImpulse.vy=0;
	dynPtr->LinImpulse.vz=0;

	dynPtr->GravityOn=0;

	/* Just float there... */
}

void Wake_Hugger(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *facehuggerStatusPointer;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	facehuggerStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(facehuggerStatusPointer);
	LOCALASSERT(dynPtr);

	if (facehuggerStatusPointer->nearBehaviourState==FHNS_Floating) {
		facehuggerStatusPointer->nearBehaviourState = FHNS_Approach;
		dynPtr->GravityOn=1;
	 	SetHuggerAnimationSequence(sbPtr,HSS_Run,(ONE_FIXED*2)/3);
	}
}

static void Execute_FHNS_AboutToJump(STRATEGYBLOCK *sbPtr)
{
	FACEHUGGER_STATUS_BLOCK *fhugStatusPointer;
	DYNAMICSBLOCK *dynPtr;

	/* Should still be playing Walk. */

	LOCALASSERT(sbPtr);
	fhugStatusPointer = (FACEHUGGER_STATUS_BLOCK *)(sbPtr->SBdataptr);
	dynPtr=sbPtr->DynPtr;
	LOCALASSERT(fhugStatusPointer);
	LOCALASSERT(dynPtr);

	/* don't climb on walls, etc */
	dynPtr->UseStandardGravity=1;

	{
		/* Orientate to target. */
		VECTORCH orientationDirn;
		int i;

		orientationDirn.vx = Hugger_PlaceOfTarget(sbPtr).vx - dynPtr->Position.vx;
		orientationDirn.vy = 0;
		orientationDirn.vz = Hugger_PlaceOfTarget(sbPtr).vz - dynPtr->Position.vz;
		i = NPCOrientateToVector(sbPtr, &orientationDirn,NPC_TURNRATE,NULL);

		if (i==0) {
			/* Still not right!  Wait for proper facing. */
			return;
		} else {
			/* Okay, pounce! */
			int distanceToPlayer = VectorDistance(&(dynPtr->Position),&(Hugger_TargetOf(sbPtr)->DynPtr->Position));
			if((distanceToPlayer<=FACEHUGGER_JUMPDISTANCE)&&(dynPtr->IsInContactWithFloor)
				&&(sbPtr->SBDamageBlock.IsOnFire==0)) {
				JumpAtPlayer(sbPtr);
				return;
			} else {
				/* Return to approach. */
				NPC_InitMovementData(&(fhugStatusPointer->moveData));
				fhugStatusPointer->nearBehaviourState = FHNS_Approach;
				fhugStatusPointer->stateTimer = 0;
				/* no sequence change required */
			}
		}
	}
}






/*--------------------**
** Loading and Saving **
**--------------------*/
#include "savegame.h"

typedef struct face_hugger_save_block
{
	SAVE_BLOCK_STRATEGY_HEADER header;

	signed int health;
	FACEHUGGER_NEAR_BHSTATE nearBehaviourState;
	int stateTimer;
	int DoomTimer;
	int CurveRadius;
	int CurveLength;
	int CurveTimeOut;
  	BOOL jumping;


	//strategyblock stuff
	int integrity;
	DAMAGEBLOCK SBDamageBlock;
	DYNAMICSBLOCK dynamics;
}FACE_HUGGER_SAVE_BLOCK;

//defines for load/save macros
#define SAVELOAD_BLOCK block
#define SAVELOAD_BEHAV huggerStatusPointer

void LoadStrategy_FaceHugger(SAVE_BLOCK_STRATEGY_HEADER* header)
{
	STRATEGYBLOCK* sbPtr;
	FACEHUGGER_STATUS_BLOCK* huggerStatusPointer;
	FACE_HUGGER_SAVE_BLOCK* block = (FACE_HUGGER_SAVE_BLOCK*) header;

	//check the size of the save block
	if(header->size!=sizeof(*block)) return;

	//find the existing strategy block
	sbPtr = FindSBWithName(header->SBname);
	if(!sbPtr) return;

	//make sure the strategy found is of the right type
	if(sbPtr->I_SBtype != I_BehaviourFaceHugger) return;

	huggerStatusPointer = (FACEHUGGER_STATUS_BLOCK*)sbPtr->SBdataptr;

	//start copying stuff
	COPYELEMENT_LOAD(health)
	COPYELEMENT_LOAD(nearBehaviourState)
	COPYELEMENT_LOAD(stateTimer)
	COPYELEMENT_LOAD(DoomTimer)
	COPYELEMENT_LOAD(CurveRadius)
	COPYELEMENT_LOAD(CurveLength)
	COPYELEMENT_LOAD(CurveTimeOut)
	if(huggerStatusPointer->CurveLength <= 0)
	{
		//the length gets divided by, so this makes the facehugger work out a new curve
		Save_NoteRefused("a curve with no length");
		huggerStatusPointer->CurveTimeOut = 0;
	}
	COPYELEMENT_LOAD(jumping)

	//copy strategy block stuff
	Save_RestoreDynamics(sbPtr->DynPtr, &block->dynamics, kSaveHoldsGravityOff);
	sbPtr->integrity = block->integrity;
	sbPtr->SBDamageBlock = block->SBDamageBlock;


	//load the hierarchy
	{
		SAVE_BLOCK_HEADER* hier_header = GetNextBlockIfOfType(SaveBlock_Hierarchy);
		if(hier_header)
		{
			LoadHierarchy(hier_header,&huggerStatusPointer->HModelController);
		}
	}
	Load_SoundState(&huggerStatusPointer->soundHandle);
	Load_SoundState(&huggerStatusPointer->soundHandle2);
}

void SaveStrategy_FaceHugger(STRATEGYBLOCK* sbPtr)
{
	FACE_HUGGER_SAVE_BLOCK* block;
	FACEHUGGER_STATUS_BLOCK* huggerStatusPointer;

	huggerStatusPointer = (FACEHUGGER_STATUS_BLOCK*)sbPtr->SBdataptr;
	GET_STRATEGY_SAVE_BLOCK(FACE_HUGGER_SAVE_BLOCK,block,sbPtr);

	//start copying stuff
	COPYELEMENT_SAVE(health)
	COPYELEMENT_SAVE(nearBehaviourState)
	COPYELEMENT_SAVE(stateTimer)
	COPYELEMENT_SAVE(DoomTimer)
	COPYELEMENT_SAVE(CurveRadius)
	COPYELEMENT_SAVE(CurveLength)
	COPYELEMENT_SAVE(CurveTimeOut)
	COPYELEMENT_SAVE(jumping)

	//save strategy block stuff
	block->dynamics = *sbPtr->DynPtr;
	block->dynamics.CollisionReportPtr=0;

	block->integrity = sbPtr->integrity;
	block->SBDamageBlock = sbPtr->SBDamageBlock;

	//save the hierarchy
	SaveHierarchy(&huggerStatusPointer->HModelController);

	Save_SoundState(&huggerStatusPointer->soundHandle);
	Save_SoundState(&huggerStatusPointer->soundHandle2);

}
