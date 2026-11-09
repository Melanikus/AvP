// Modified 2026-11-07 by Melanikus for the AvP port:
//   a lift keeps what the host last said about it, so a joiner's car follows the host's in a game that plays the same as single player
// Modified 2026-11-08 by Melanikus for the AvP port:
//   it keeps when it heard that by the machine's own clock, not by adding up frame times (#90)
#include "track.h"
/*------------------------------Patrick 14/3/97-----------------------------------
  Header for platform lift stuff
  --------------------------------------------------------------------------------*/

typedef enum platformlift_states
{
	PLBS_AtRest,
	PLBS_Activating,
	PLBS_GoingUp,
	PLBS_GoingDown,
} PLATFORMLIFT_STATES;

typedef struct platformlift_behaviour_type
{
	VECTORCH homePosition;
	int upHeight;
	int downHeight;
	int activationDelayTimer;
	PLATFORMLIFT_STATES	state;

	TRACK_SOUND* sound;
	TRACK_SOUND* start_sound;
	TRACK_SOUND* end_sound;
	
	// A switch will set these flags on AssignSBNames
	
	BOOL Enabled; 
	BOOL OneUse; //if set ,lift becomes disabled after changing position once

	int netMsgCount;

	// The lift sync, in the mode that plays as single player (NETMESSAGE_LIFTSTATE). On the host: how long since it last sent, and
	// the state it sent. On a joiner: whether the host has spoken yet, what it last said, and when it heard it, which his car follows.
	// syncMessages is how many lift sync messages this machine has sent (the host) or had (a joiner) about this lift.
	int syncSinceSent;
	int syncLastSentState;
	BOOL syncHeard;
	int syncHeight;
	int syncState;
	int syncEnd;
	unsigned int syncHeardAt;   // when this machine had the host's last message, by timeGetTime, its own clock in ms: see NetLiftMsSince
	int syncMessages;
} PLATFORMLIFT_BEHAVIOUR_BLOCK;

typedef struct platformlift_tools_template
{
	struct vectorch position;
	struct euler orientation;
	int shapeIndex;
	int travel;	/* vertical distance from start position to end position (down = +ve) */
	BOOL Enabled;
	BOOL OneUse;
	char nameID[SB_NAME_LENGTH];

	TRACK_SOUND* sound;
	TRACK_SOUND* start_sound;
	TRACK_SOUND* end_sound;

} PLATFORMLIFT_TOOLS_TEMPLATE;

#define PLATFORMLIFT_SPEED				5000 		/* mm/s */
#define PLATFORMLIFT_ACTIVATIONTIME		((ONE_FIXED*3)>>1)	/* fixed point seconds */
#define PLATFORMLIFT_NUMNETMESSAGES			5

void InitialisePlatformLift(void* bhdata, STRATEGYBLOCK *sbPtr);
void PlatformLiftBehaviour(STRATEGYBLOCK *sbPtr);

void ActivatePlatformLift(STRATEGYBLOCK *sbPtr);
void SendPlatformLiftUp(STRATEGYBLOCK *sbPtr);
void SendPlatformLiftDown(STRATEGYBLOCK *sbPtr);
void StopPlatformLift(STRATEGYBLOCK *sbPtr);

// The lift sync, in the mode that plays as single player: see NETMESSAGE_LIFTSTATE in pldnet.h
int PlatformLift_EndItsGoingTo(STRATEGYBLOCK *sbPtr);
BOOL PlatformLift_NameIsShared(STRATEGYBLOCK *sbPtr);
void PlatformLift_TakeTheHostsCar(STRATEGYBLOCK *sbPtr, int height, int state, int end);
