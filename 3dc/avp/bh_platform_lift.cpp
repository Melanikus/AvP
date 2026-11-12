// Modified 2026-11-11 by Melanikus (Third) for the AvP port:
//   a save can't turn the level's walls or its gravity off for a lift (Save_RestoreDynamics, save_checks.h)
/*------------------------------Patrick 14/3/97-----------------------------------
  Source for Platform lift behaviour functions
  --------------------------------------------------------------------------------*/
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed a repeated "ghostData =" from one line; the game runs exactly as before
//   in the same-as-single-player mode, a lift comes for someone within 5 m of its edge, because some lifts are wider than 5 m
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a saved dynamics block is put back through Save_RestoreDynamics, which keeps out what a save must not set.
// Modified 2026-11-07 by Melanikus for the AvP port:
//   in the same-as-single-player mode, the host says where each lift's car is, and every other machine's car follows it
// Modified 2026-11-08 by Melanikus for the AvP port:
//   how long since the host spoke is counted by the machine's own clock, so a long frame or a pause no longer leaves a joiner's car
//   behind the host's (#90)
// Modified 2026-11-10 by Melanikus for the AvP port:
//   in the same-as-single-player mode, someone waits at a lift's far end only on that half of the shaft, within 5 m of that end's
//   height and within 5 m of the car's edge seen from above; before, a tall car's shape made its own end count as the other (#189).
//   And the host's own player calls a car as single player does: it's drawn on his screen, and he's on the other half of the shaft
#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "triggers.h"
#include "mslhand.h"
#include "dynblock.h"
#include "save_checks.h"
#include "dynamics.h"
#include "showcmds.h"
#include "io.h"
#include "logString.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "bh_platform_lift.h"
#include "pldnet.h"
#include "pldghost.h"
#include "weapons.h"
#include "net_checks.h"

/* prototypes for this file */
static int SquashingSomething(DYNAMICSBLOCK *dynPtr);
static void PushPassengersUpwards(DYNAMICSBLOCK *dynPtr);
static int ActivatedByPlayer(DYNAMICSBLOCK *dynPtr);
static void Lift_FollowTheHostsCar(STRATEGYBLOCK *sbPtr);
static int PlayerIsNearOtherTerminal(STRATEGYBLOCK *sbPtr);
static int NetPlayerAtOtherTerminal(STRATEGYBLOCK *sbPtr);
static int WaitingAtTheFarEnd(VECTORCH *where, VECTORCH *farEnd, int nearEndHeight, int footprint);
static int HostSeesTheCarFromTheOtherHalf(STRATEGYBLOCK *sbPtr);
static int PLiftIsNearerUpThanDown(STRATEGYBLOCK *sbPtr);

/*------------------------------Patrick 15/3/97-----------------------------------
  Behaviour function for platform lift
  --------------------------------------------------------------------------------*/
void PlatformLiftBehaviour(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	dynPtr = sbPtr->DynPtr;
	LOCALASSERT(dynPtr);

	// In the mode that plays as single player, a joiner's car follows the host's once the host has said where it is, and doesn't
	// move by itself. Touching it still asks the host to start it, as before.
	BOOL const followsTheHost = AvP.Network == I_Peer && SameAsSinglePlayer() && platformliftdata->syncHeard;

	if (followsTheHost)
	{
		Lift_FollowTheHostsCar(sbPtr);

		if (platformliftdata->state == PLBS_AtRest && ActivatedByPlayer(dynPtr))
		{
			AddNetMsg_RequestPlatformLiftActivate(sbPtr);
		}
	}
	else switch(platformliftdata->state)
	{
		case(PLBS_AtRest):
		{
			if(AvP.Network==I_Host)
			{
				/* multiplayer host */
				if(NetPlayerAtOtherTerminal(sbPtr))
				{
					if(platformliftdata->Enabled) ActivatePlatformLift(sbPtr);
				}
				else if(sbPtr->SBdptr)
				{			
					/* check for activation by this player*/
					// In the mode that plays as single player, the host's own player also calls the car the way single player does
					// (#189), so a car the level starts at the other end comes to him, as it would there
					if(ActivatedByPlayer(dynPtr) || HostSeesTheCarFromTheOtherHalf(sbPtr))
					{
						if (platformliftdata->Enabled) ActivatePlatformLift(sbPtr);
					}
			 	}
			}
			else if(AvP.Network==I_No_Network)
			{
				/* single player */
				if(sbPtr->SBdptr)
				{			
					/* check for activation */
					if(ActivatedByPlayer(dynPtr) || PlayerIsNearOtherTerminal(sbPtr))
					{
						if (platformliftdata->Enabled) ActivatePlatformLift(sbPtr);
					}
			 	}
			}
			else
			{
				LOCALASSERT(AvP.Network==I_Peer);
				if(ActivatedByPlayer(dynPtr)) AddNetMsg_RequestPlatformLiftActivate(sbPtr);
			}						
			//textprint("Platform state: at rest\n");
			break;
		}
		case(PLBS_Activating):
		{
			if(AvP.Network!=I_Peer)
			{
				if(platformliftdata->activationDelayTimer>0)
				{
					platformliftdata->activationDelayTimer -= NormalFrameTime;
				}
								
				
				if(platformliftdata->activationDelayTimer<=0)
				{
					//play start sound if the lift has any
					if(platformliftdata->start_sound)
					{
						Start_Track_Sound(platformliftdata->start_sound,&sbPtr->DynPtr->Position);
					}
					
					
					platformliftdata->activationDelayTimer = 0;
					if(dynPtr->Position.vy > ((platformliftdata->upHeight+platformliftdata->downHeight)/2))
					{
						SendPlatformLiftUp(sbPtr);
					}
					else SendPlatformLiftDown(sbPtr);
				}
			}
			//textprint("Platform state: activating\n");
			//textprint("Platform timer: %d\n",platformliftdata->activationDelayTimer);
			break;	
		}
		case(PLBS_GoingUp):
		{
			if(dynPtr->Position.vy <= platformliftdata->upHeight)
			{
				/* finished */
				StopPlatformLift(sbPtr);
				if (platformliftdata->OneUse)
					platformliftdata->Enabled = FALSE;
			}
			else
			{
				sbPtr->DynPtr->Displacement.vy = -MUL_FIXED(PLATFORMLIFT_SPEED,NormalFrameTime);
				{
					int d = platformliftdata->upHeight - sbPtr->DynPtr->Position.vy;

				 	if (d>sbPtr->DynPtr->Displacement.vy)
				 	{
				 		sbPtr->DynPtr->Displacement.vy=d;
					}
				}
			}
			
			PushPassengersUpwards(dynPtr);
			//textprint("Platform state: going up\n");
			break; 
		}
		case(PLBS_GoingDown):
		{
			if(dynPtr->Position.vy >= platformliftdata->downHeight)
			{
				/* finished */
				StopPlatformLift(sbPtr);
				if (platformliftdata->OneUse)
					platformliftdata->Enabled = FALSE;
			}
			else 
			{
				sbPtr->DynPtr->Displacement.vy = MUL_FIXED(PLATFORMLIFT_SPEED,NormalFrameTime);
				{
					int d = platformliftdata->downHeight - sbPtr->DynPtr->Position.vy;

				 	if (d<sbPtr->DynPtr->Displacement.vy)
				 	{
				 		sbPtr->DynPtr->Displacement.vy=d;
					}
				}
			
				if(AvP.Network!=I_Peer)
				{
					if(SquashingSomething(dynPtr))
					{
//						sbPtr->DynPtr->Displacement.vy = 0;					
						//	SendPlatformLiftUp(sbPtr);
					}
				}
			}

			//textprint("Platform state: going down\n");
			break; 
		}
		default:
		{
			LOCALASSERT(1==0);
		}
	}
	
	//update sound
	if(platformliftdata->state==PLBS_GoingUp || platformliftdata->state==PLBS_GoingDown)
	{
		BOOL mainSoundShouldBePlaying=TRUE;
		
		//the main sound should be started if there is no start sound , or the start sound has
		//almost finished
		if(platformliftdata->start_sound && platformliftdata->start_sound->playing)
		{
			Update_Track_Sound(platformliftdata->start_sound,&dynPtr->Position);

			if(platformliftdata->start_sound->time_left>NormalFrameTime)
			{
				mainSoundShouldBePlaying=FALSE;
			}

		}
		
		if(platformliftdata->sound)
		{
			if(platformliftdata->sound->playing)
			{
				Update_Track_Sound(platformliftdata->sound,&dynPtr->Position);
			}
			else if(mainSoundShouldBePlaying)
			{
				Start_Track_Sound(platformliftdata->sound,&sbPtr->DynPtr->Position);
			}
		}
	}


	//textprint("Platform pos: %d / %d / %d\n",platformliftdata->upHeight,dynPtr->Position.vy,platformliftdata->downHeight);

	/* send state messages in net game */
	if(AvP.Network==I_Host)
	{
		if(platformliftdata->netMsgCount>0)
		{
			/* don't send at rest messages: peers detect end of movement locally. */
			if(platformliftdata->state!=PLBS_AtRest) 
			{
				AddNetMsg_PlatformLiftState(sbPtr);
			}
			platformliftdata->netMsgCount--;
		}
	}

	// In the mode that plays as single player, the host also says where the car is: when its state changes, and once a second, so
	// a joiner who missed a message or arrived late has it right within a second
	if (AvP.Network == I_Host && SameAsSinglePlayer())
	{
		platformliftdata->syncSinceSent += NormalFrameTime;

		if ((int)platformliftdata->state != platformliftdata->syncLastSentState || platformliftdata->syncSinceSent >= ONE_FIXED)
		{
			AddNetMsg_LiftState(sbPtr);
			platformliftdata->syncLastSentState = (int)platformliftdata->state;
			platformliftdata->syncSinceSent = 0;
		}
	}
}
	   
void InitialisePlatformLift(void* bhdata, STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_TOOLS_TEMPLATE *toolsData = (PLATFORMLIFT_TOOLS_TEMPLATE *)bhdata;
	PLATFORMLIFT_BEHAVIOUR_BLOCK* platformliftdata;
	int i;

	LOCALASSERT(sbPtr->I_SBtype == I_BehaviourPlatform);
	LOCALASSERT(toolsData);

	/* create, initialise and attach a data block */
	platformliftdata = static_cast<PLATFORMLIFT_BEHAVIOUR_BLOCK*>(AllocateMem(sizeof(PLATFORMLIFT_BEHAVIOUR_BLOCK)));
	if(!platformliftdata)
	{
		memoryInitialisationFailure = 1;
		return;
	}

	sbPtr->SBdataptr = platformliftdata; 
	platformliftdata->homePosition = toolsData->position;
	platformliftdata->activationDelayTimer = 0;
	platformliftdata->state = PLBS_AtRest;
	platformliftdata->netMsgCount = 0;

	// The lift sync: the host sends at once, and a joiner's car moves by itself until the host has spoken
	platformliftdata->syncSinceSent = ONE_FIXED;
	platformliftdata->syncLastSentState = -1;
	platformliftdata->syncHeard = FALSE;
	platformliftdata->syncHeight = toolsData->position.vy;
	platformliftdata->syncState = PLBS_AtRest;
	platformliftdata->syncEnd = 0;
	platformliftdata->syncHeardAt = 0;
	platformliftdata->syncMessages = 0;
	
	if(toolsData->travel<0)
	{
		platformliftdata->upHeight = platformliftdata->homePosition.vy + toolsData->travel;
		platformliftdata->downHeight = platformliftdata->homePosition.vy;
	} 		
	else
	{
		platformliftdata->upHeight = platformliftdata->homePosition.vy;
		platformliftdata->downHeight = platformliftdata->homePosition.vy + toolsData->travel;
	}
	
	LOCALASSERT(platformliftdata->upHeight < platformliftdata->downHeight);

	platformliftdata->sound = toolsData->sound;
	platformliftdata->start_sound = toolsData->start_sound;
	platformliftdata->end_sound = toolsData->end_sound;

	platformliftdata->Enabled = toolsData->Enabled;
	platformliftdata->OneUse = toolsData->OneUse;


	/* create and initialise the dynamics block */
	{
		DYNAMICSBLOCK *dynPtr = AllocateDynamicsBlock(DYNAMICS_TEMPLATE_PLATFORM_LIFT);
		GLOBALASSERT(dynPtr);
		
		sbPtr->DynPtr = dynPtr;      	
      	dynPtr->PrevPosition = dynPtr->Position = toolsData->position;
		dynPtr->LinVelocity.vx = dynPtr->LinVelocity.vy = dynPtr->LinVelocity.vz = 0;
		dynPtr->OrientEuler = toolsData->orientation;
		CreateEulerMatrix(&dynPtr->OrientEuler, &dynPtr->OrientMat);
		TransposeMatrixCH(&dynPtr->OrientMat);      
	}

	/* strategy block initialisation */
	sbPtr->shapeIndex = toolsData->shapeIndex;
	for(i=0;i<SB_NAME_LENGTH;i++) sbPtr->SBname[i] = toolsData->nameID[i];
	sbPtr->integrity = 1;

	// The lift sync finds a car by its name, so it can't tell two lifts with the same name apart, and refuses messages about them.
	// No stock level has one; a level that does is told in the log as it loads.
	if (PlatformLift_NameIsShared(sbPtr))
	{
		LogString("lift: two platform lifts in this level share a name, so a network game can't keep their cars together");
	}
}

/*------------------------------Patrick 2/4/97-----------------------------------
  State change functions: these are alos used by network message processing fns
  --------------------------------------------------------------------------------*/
void ActivatePlatformLift(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);

	platformliftdata->state = PLBS_Activating;
	platformliftdata->activationDelayTimer = PLATFORMLIFT_ACTIVATIONTIME;
	platformliftdata->netMsgCount = PLATFORMLIFT_NUMNETMESSAGES;

}

void SendPlatformLiftUp(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	
	platformliftdata->state = PLBS_GoingUp;
	sbPtr->DynPtr->Displacement.vx = sbPtr->DynPtr->Displacement.vz = 0;
	platformliftdata->netMsgCount = PLATFORMLIFT_NUMNETMESSAGES;
	
	sbPtr->DynPtr->Displacement.vy = -MUL_FIXED(PLATFORMLIFT_SPEED,NormalFrameTime);
	{
		int d = platformliftdata->upHeight - sbPtr->DynPtr->Position.vy;

	 	if (d>sbPtr->DynPtr->Displacement.vy)
	 	{
	 		sbPtr->DynPtr->Displacement.vy=d;
		}
	}
}

void SendPlatformLiftDown(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	
	platformliftdata->state = PLBS_GoingDown;
	sbPtr->DynPtr->Displacement.vx = sbPtr->DynPtr->Displacement.vz = 0;
	platformliftdata->netMsgCount = PLATFORMLIFT_NUMNETMESSAGES;

	sbPtr->DynPtr->Displacement.vy = MUL_FIXED(PLATFORMLIFT_SPEED,NormalFrameTime);
	{
		int d = platformliftdata->downHeight - sbPtr->DynPtr->Position.vy;

	 	if (d<sbPtr->DynPtr->Displacement.vy)
	 	{
	 		sbPtr->DynPtr->Displacement.vy=d;
		}
	}
}

void StopPlatformLift(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);

	sbPtr->DynPtr->Displacement.vx = sbPtr->DynPtr->Displacement.vy = sbPtr->DynPtr->Displacement.vz = 0;
	platformliftdata->state = PLBS_AtRest;

	/* just to stop drift... */
	/*
	if(PLiftIsNearerUpThanDown(sbPtr))
		sbPtr->DynPtr->Position.vy = platformliftdata->upHeight;
	else
		sbPtr->DynPtr->Position.vy = platformliftdata->downHeight;
	*/
	platformliftdata->netMsgCount = PLATFORMLIFT_NUMNETMESSAGES;

	//stop sound if the lift has any
	if(platformliftdata->sound)
	{
		Stop_Track_Sound(platformliftdata->sound);
	}
	//start end sound if lift has any
	if(platformliftdata->end_sound)
	{
		Start_Track_Sound(platformliftdata->end_sound,&sbPtr->DynPtr->Position);
	}

}
					

/*------------------------------Patrick 15/3/97-----------------------------------
  Support functions
  --------------------------------------------------------------------------------*/
static int SquashingSomething(DYNAMICSBLOCK *dynPtr)
{
	struct collisionreport *nextReport;
	int squashingSomething = 0;
	LOCALASSERT(dynPtr);
	nextReport = dynPtr->CollisionReportPtr;
				
	/* walk the collision report list, looking for collisions against objects */
	while(nextReport)
	{
		STRATEGYBLOCK *sbPtr = nextReport->ObstacleSBPtr;
		if(sbPtr)
		{
			if(nextReport->ObstacleNormal.vy < -46340)
			{
				extern DAMAGE_PROFILE FallingDamage;
				squashingSomething=1;
				
				/* squish! */
				CauseDamageToObject(sbPtr, &FallingDamage, (100*NormalFrameTime), NULL);
			}
			else if (nextReport->ObstacleNormal.vy > 46340)
			{
				DYNAMICSBLOCK *objDynPtr = sbPtr->DynPtr;
				if (objDynPtr)
				{
					if (objDynPtr->LinImpulse.vy < PLATFORMLIFT_SPEED)
					{
						objDynPtr->LinImpulse.vy = PLATFORMLIFT_SPEED;
					}
				}
			}
		}		
		nextReport = nextReport->NextCollisionReportPtr;
	}
	return squashingSomething;
}

static void PushPassengersUpwards(DYNAMICSBLOCK *dynPtr)
{
	#if 0
	struct collisionreport *nextReport;

	LOCALASSERT(dynPtr);
	nextReport = dynPtr->CollisionReportPtr;
				
	/* walk the collision report list, looking for collisions against objects */
	while(nextReport)
	{
		STRATEGYBLOCK *sbPtr = nextReport->ObstacleSBPtr;
		if(sbPtr)
		{
			DYNAMICSBLOCK *objDynPtr = sbPtr->DynPtr;
			if (objDynPtr)
			{
				int upwardsImpulse = -MUL_FIXED(GRAVITY_STRENGTH,NormalFrameTime) - PLATFORMLIFT_SPEED;

				if (objDynPtr->LinImpulse.vy > upwardsImpulse)
				{
					objDynPtr->LinImpulse.vy = upwardsImpulse;
					objDynPtr->IsInContactWithFloor =1;
					PrintDebuggingText("I'M PUSHING UP AN OBJECT!");
				}
			}
		}		
		nextReport = nextReport->NextCollisionReportPtr;
	}
	#endif
}

static int ActivatedByPlayer(DYNAMICSBLOCK *dynPtr)
{
	struct collisionreport *nextReport;

	LOCALASSERT(dynPtr);
	nextReport = dynPtr->CollisionReportPtr;
				
	/* walk the collision report list, looking for collisions against objects */
	while(nextReport)
	{
///		textprint("collision with %p\n",nextReport->ObstacleSBPtr);
		if(nextReport->ObstacleSBPtr == Player->ObStrategyBlock) return 1;		
	
		nextReport = nextReport->NextCollisionReportPtr;
	}
	return 0;
}

static int PlayerIsNearOtherTerminal(STRATEGYBLOCK *sbPtr)
{
	int averageHeight;
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	DYNAMICSBLOCK *dynPtr;

	LOCALASSERT(sbPtr);
	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	LOCALASSERT(platformliftdata->state == PLBS_AtRest);
 	dynPtr = sbPtr->DynPtr;
	LOCALASSERT(dynPtr);
	
	averageHeight = (platformliftdata->upHeight+platformliftdata->downHeight)/2;

	if(dynPtr->Position.vy > averageHeight)
	{
		if (Player->ObStrategyBlock->DynPtr->Position.vy < averageHeight)
			return 1;
	}
	else
	{
		if (Player->ObStrategyBlock->DynPtr->Position.vy > averageHeight)
			return 1;
	}
	return 0;
}

/* returns 1 if the player or a remote player is within 5 m of the other end of
the lift track...*/
// In the mode that plays as single player, what "at the other end" means is NetLiftIsWaitingAtTheFarEnd's (net_checks.h, #189)
static int NetPlayerAtOtherTerminal(STRATEGYBLOCK *sbPtr)
{
	int sbIndex = 0;
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	DYNAMICSBLOCK *dynPtr;
	VECTORCH otherTerminal;
	int nearEndHeight;
	// The car seen from above: how far its footprint reaches from its middle, from its shape in the level's file
	SHAPEHEADER const *const liftShape = GetShapeData(sbPtr->shapeIndex);
	int const footprint = liftShape ? NetLiftFootprintRadius(liftShape->shapeminx, liftShape->shapemaxx, liftShape->shapeminz, liftShape->shapemaxz) : 0;

	LOCALASSERT(sbPtr);
	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	LOCALASSERT(platformliftdata->state == PLBS_AtRest);
 	dynPtr = sbPtr->DynPtr;
	LOCALASSERT(dynPtr);

	if(PLiftIsNearerUpThanDown(sbPtr))
	{
		otherTerminal = dynPtr->Position;
		otherTerminal.vy = platformliftdata->downHeight;
		nearEndHeight = platformliftdata->upHeight;
	}
	else
	{
		otherTerminal = dynPtr->Position;
		otherTerminal.vy = platformliftdata->upHeight;	
		nearEndHeight = platformliftdata->downHeight;
	}

	while(sbIndex < NumActiveStBlocks)
	{	
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[sbIndex++];

		if(sbPtr->I_SBtype==I_BehaviourNetGhost)
		{
			NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
			LOCALASSERT(ghostData);

			if((ghostData->type==I_BehaviourMarinePlayer)||
	   		   (ghostData->type==I_BehaviourAlienPlayer)||
	   		   (ghostData->type==I_BehaviourPredatorPlayer))
			{
				/* remote player */
				LOCALASSERT(sbPtr->DynPtr);
				if(WaitingAtTheFarEnd(&(sbPtr->DynPtr->Position),&(otherTerminal),nearEndHeight,footprint)) return 1;
			}
		}
		else if(sbPtr==Player->ObStrategyBlock)
		{
			/* local player */
			LOCALASSERT(sbPtr->DynPtr);
			if(WaitingAtTheFarEnd(&(sbPtr->DynPtr->Position),&(otherTerminal),nearEndHeight,footprint)) return 1;
		}
	}		
	return 0;
}

// Whether someone standing at "where" counts as waiting at the lift's far end. Outside the mode that plays as single player, it's
// 1999's 5 m from the middle of the far end, measured as the game always has (VectorDistance). In the mode, it's the far half of the
// shaft and near that end: NetLiftIsWaitingAtTheFarEnd, net_checks.h, which the tests check against every stock lift (#189).
static int WaitingAtTheFarEnd(VECTORCH *where, VECTORCH *farEnd, int nearEndHeight, int footprint)
{
	return NetLiftIsWaitingAtTheFarEnd(VectorDistance(where, farEnd), where->vx, where->vy, where->vz, farEnd->vx, farEnd->vz, nearEndHeight, farEnd->vy,
		footprint, SameAsSinglePlayer() != FALSE) ? 1 : 0;
}

// In the mode that plays as single player, the host's own player calls a car at rest as single player's rule does: the car is
// drawn on his screen, and he's on the other half of its shaft (PlayerIsNearOtherTerminal). The joiners' screens aren't known
// here, so they call it from its far end, or by touching it. Outside the mode, this is never true, as in 1999's network game.
static int HostSeesTheCarFromTheOtherHalf(STRATEGYBLOCK *sbPtr)
{
	if (!SameAsSinglePlayer() || !sbPtr->SBdptr || !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->DynPtr)
	{
		return 0;
	}

	return PlayerIsNearOtherTerminal(sbPtr);
}


static int PLiftIsNearerUpThanDown(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	int upDist, downDist;

	LOCALASSERT(sbPtr);
	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	LOCALASSERT(sbPtr->DynPtr);

	upDist = sbPtr->DynPtr->Position.vy - platformliftdata->upHeight;
	downDist = platformliftdata->downHeight - sbPtr->DynPtr->Position.vy;

	if(upDist<downDist) return 1;
	else return 0;
}


void NetworkPeerChangePlatformLiftState(STRATEGYBLOCK* sbPtr,PLATFORMLIFT_STATES new_state)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	LOCALASSERT(sbPtr);
	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platformliftdata);
	LOCALASSERT(sbPtr->DynPtr);

	// In the mode that plays as single player, the host's lift sync message says where the car is, and this one is left alone
	if (SameAsSinglePlayer() && platformliftdata->syncHeard)
	{
		return;
	}

	//check to see if this is a new state , and change any sound playing as necessary

	if(new_state==PLBS_Activating)
	{
		/*
		ignore this state change , since only the host does anything while the lift is
		activating , and this will prevent the lift from stopping at the ends correctly
		*/
		return;

	}
	
	if(new_state==PLBS_GoingUp &&
	   !(platformliftdata->state==PLBS_GoingUp || platformliftdata->state==PLBS_GoingDown))
	{
		//platform lift has started moving
		if(platformliftdata->start_sound)
		{
			Start_Track_Sound(platformliftdata->start_sound,&sbPtr->DynPtr->Position);
		}
	}
	
	platformliftdata->state=new_state;
	

}


// Which end a car is going to, or resting at: 0 the top (upHeight, the smaller height), 1 the bottom (downHeight)
int PlatformLift_EndItsGoingTo(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK const *const lift = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;

	if (lift->state == PLBS_GoingUp)
	{
		return 0;
	}

	if (lift->state == PLBS_GoingDown)
	{
		return 1;
	}

	return sbPtr->DynPtr->Position.vy - lift->upHeight <= lift->downHeight - sbPtr->DynPtr->Position.vy ? 0 : 1;
}

// Whether another platform lift in the level has this one's name. The network finds a lift by its name, so it couldn't tell them apart.
BOOL PlatformLift_NameIsShared(STRATEGYBLOCK *sbPtr)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const other = ActiveStBlockList[i];

		if (other != sbPtr && other->I_SBtype == I_BehaviourPlatform && NAME_ISEQUAL(other->SBname, sbPtr->SBname))
		{
			return TRUE;
		}
	}

	return FALSE;
}

// The host's message about this car, already checked (ProcessNetMsg_LiftState, pldnet.cpp). The car follows it from now on.
void PlatformLift_TakeTheHostsCar(STRATEGYBLOCK *sbPtr, int height, int state, int end)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *const lift = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	BOOL const wasMoving = lift->state == PLBS_GoingUp || lift->state == PLBS_GoingDown;
	BOOL const isMoving = state == PLBS_GoingUp || state == PLBS_GoingDown;

	// The sound of it starting, as the old state message plays it
	if (isMoving && !wasMoving && lift->start_sound)
	{
		Start_Track_Sound(lift->start_sound, &sbPtr->DynPtr->Position);
	}

	lift->syncHeight = height;
	lift->syncState = state;
	lift->syncEnd = end;
	lift->syncHeardAt = timeGetTime();
	lift->syncHeard = TRUE;
	lift->syncMessages++;
	lift->state = (PLATFORMLIFT_STATES)state;
}

// In the mode that plays as single player, a joiner's car follows the host's (PlatformLift_TakeTheHostsCar). From the host's last
// message it works out where the host's car should be by now, and moves this one there. With someone on it, or close, it travels at
// a little over lift speed, so a rider is carried and never dropped or passed through. Further than a step away with nobody on it,
// it goes straight there. A joiner's touch still asks the host to start it (PlatformLiftBehaviour).
static void Lift_FollowTheHostsCar(STRATEGYBLOCK *sbPtr)
{
	PLATFORMLIFT_BEHAVIOUR_BLOCK *const lift = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	DYNAMICSBLOCK *const dynPtr = sbPtr->DynPtr;
	BOOL const moving = lift->syncState == PLBS_GoingUp || lift->syncState == PLBS_GoingDown;
	int const endHeight = lift->syncEnd == 0 ? lift->upHeight : lift->downHeight;

	// How long since the host said where its car was, by this machine's own clock: the host's car went on moving all that time, however
	// long this machine's frames were, or if it stood still (net_checks.h, NetLiftMsSince). #90.
	int const msSince = NetLiftMsSince(timeGetTime(), lift->syncHeardAt);
	int const hostHeight = NetLiftHostHeightNow(lift->syncHeight, moving != FALSE, endHeight, PLATFORMLIFT_SPEED, msSince);

	// But how far this car may move in one frame stays in game time, the time everything on it moves by, so a rider is carried with it
	int const mostThisFrame = MUL_FIXED(PLATFORMLIFT_SPEED + (PLATFORMLIFT_SPEED >> 2), NormalFrameTime);
	BOOL const someoneOnIt = ActivatedByPlayer(dynPtr) ? TRUE : FALSE;
	int const height = NetLiftFollowStep(dynPtr->Position.vy, hostHeight, mostThisFrame, someoneOnIt != FALSE, MAXIMUM_STEP_HEIGHT);
	int const gap = height - dynPtr->Position.vy;

	dynPtr->Displacement.vx = 0;
	dynPtr->Displacement.vz = 0;

	if (!someoneOnIt && (gap > MAXIMUM_STEP_HEIGHT || -gap > MAXIMUM_STEP_HEIGHT))
	{
		dynPtr->Position.vy = height;
		dynPtr->PrevPosition.vy = height;
		dynPtr->Displacement.vy = 0;
	}
	else
	{
		dynPtr->Displacement.vy = gap;
	}
}

/*--------------------**
** Loading and Saving **
**--------------------*/
#include "savegame.h"
typedef struct platform_lift_save_block
{
	SAVE_BLOCK_STRATEGY_HEADER header;

	DYNAMICSBLOCK dynamics;

	int activationDelayTimer;
	PLATFORMLIFT_STATES	state;
	
	BOOL Enabled; 
	BOOL OneUse; //if set ,lift becomes disabled after changing position once
	

}PLATFORM_LIFT_SAVE_BLOCK;

//defines for load/save macros
#define SAVELOAD_BLOCK block
#define SAVELOAD_BEHAV platformliftdata

void LoadStrategy_PlatformLift(SAVE_BLOCK_STRATEGY_HEADER* header)
{
	STRATEGYBLOCK* sbPtr;
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	PLATFORM_LIFT_SAVE_BLOCK* block = (PLATFORM_LIFT_SAVE_BLOCK*) header; 

	//check the size of the save block
	if(header->size!=sizeof(*block)) return;

	//find the existing strategy block
	sbPtr = FindSBWithName(header->SBname);
	if(!sbPtr) return;

	//make sure the strategy found is of the right type
	if(sbPtr->I_SBtype != I_BehaviourPlatform) return;

	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK*)sbPtr->SBdataptr;

	//start copying stuff
	
	COPYELEMENT_LOAD(activationDelayTimer)
	COPYELEMENT_LOAD(state)
	COPYELEMENT_LOAD(Enabled)
	COPYELEMENT_LOAD(OneUse)

	Save_RestoreDynamics(sbPtr->DynPtr, &block->dynamics, kSaveHoldsNeitherOff);

}


void SaveStrategy_PlatformLift(STRATEGYBLOCK* sbPtr)
{
	PLATFORM_LIFT_SAVE_BLOCK *block;
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platformliftdata;
	
	platformliftdata = (PLATFORMLIFT_BEHAVIOUR_BLOCK*)sbPtr->SBdataptr;

	GET_STRATEGY_SAVE_BLOCK(PLATFORM_LIFT_SAVE_BLOCK,block,sbPtr);

	
	COPYELEMENT_SAVE(activationDelayTimer)
	COPYELEMENT_SAVE(state)
	COPYELEMENT_SAVE(Enabled)
	COPYELEMENT_SAVE(OneUse)

	block->dynamics = *sbPtr->DynPtr;
	block->dynamics.CollisionReportPtr=0;
	
}
