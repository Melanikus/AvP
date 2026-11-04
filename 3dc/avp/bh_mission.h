// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   the check that stops this file being read twice now tests the name it sets; the two names
//   differed by one character, so the check never worked
#ifndef bh_mission_h
#define bh_mission_h 1


void * MissionCompleteBehaveInit(void* bhdata,STRATEGYBLOCK* sbptr);
void * MessageBehaveInit(void* bhdata,STRATEGYBLOCK* sbptr);
extern void ResetMission(void* mission_objective);
extern void PrintStringTableEntryInConsole(enum TEXTSTRING_ID string_id);
extern void SendRequestToMessageStrategy(STRATEGYBLOCK* sbptr,BOOL state,int extended_data);
extern void SendRequestToMissionStrategy(STRATEGYBLOCK* sbptr,BOOL state,int extended_data);


//when this strategy receives a request state of 1 it notifies its attached mission that
//it has been achieved

typedef struct mission_complete_tools_template
{
	void* mission_objective_ptr;
	char nameID [SB_NAME_LENGTH];
}MISSION_COMPLETE_TOOLS_TEMPLATE;

typedef struct mission_complete_target
{
	AVP_BEHAVIOUR_TYPE bhvr_type;
	void* mission_objective_ptr;

}MISSION_COMPLETE_BEHAV_BLOCK;



//this strategy displays its message upon receiving a request state of 1

typedef struct message_tools_template
{
	enum TEXTSTRING_ID string_no;
	char nameID [SB_NAME_LENGTH];
	BOOL active;
}MESSAGE_TOOLS_TEMPLATE;


typedef struct message_behav_block
{
	AVP_BEHAVIOUR_TYPE bhvr_type;
	enum TEXTSTRING_ID string_no;
	BOOL active;

}MESSAGE_BEHAV_BLOCK;

#endif
