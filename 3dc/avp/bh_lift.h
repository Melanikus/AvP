// Modified 2026-11-08 by Melanikus for the AvP port:
//   the lift's change of environment is taken out (#135): every lift station is in the level being played, so it could never run.
//   With it go CleanUpLiftControl, the two requests for a change, and a station's environment and facing.
extern void LiftBehaveFun(STRATEGYBLOCK* sbptr);
extern void * LiftBehaveInit(void* bhdata, STRATEGYBLOCK* sbptr);

extern void TeleportContents(MODULE* new_pos, MODULE* old_pos,BOOL floor_switches_fixed);
extern BOOL BadGuyInModule();





/*********************** Lifts **************************/


typedef struct lift_stations
{
	char lift_call_switch_name[SB_NAME_LENGTH];
	STRATEGYBLOCK* lift_call_switch;
	char lift_door_name[SB_NAME_LENGTH];
	STRATEGYBLOCK* lift_door;
	char lift_floor_switch_name[SB_NAME_LENGTH];
	STRATEGYBLOCK* lift_floor_switch; // the floor switches teleport
	BOOL called;
	char my_sb_name[SB_NAME_LENGTH];	// the name its module is found by (AssignAllSBNames, bh_types.cpp)
	MODULE* lift_module;
	int num_floor;						// not the floor num but the poition
														// in the array
	BOOL starting_station;
												
	
}LIFT_STATION;	

typedef enum liftmotion{
	
	I_going_up,					/*** numbers go down (0 at surface **/
	I_going_down,				// numbers go up

}LIFT_MOTION;


typedef enum lift_ctrl_states
{
	I_ls_waiting,
	I_ls_closing_door,
	I_ls_moving,
	I_ls_opening_door,
	I_ls_delay_at_floor,

}LIFT_CTRL_STATES;

#define LIFT_FLOOR_DELAY ONE_FIXED*2  // two secs
#define LIFT_MOVE_DELAY ONE_FIXED*5   // three secs


typedef struct lift_control
{
	int num_stations;				// num staions for this lift
	LIFT_STATION** lift_stations;	// array of lift stations for this lift
	int dest_station;							// -1 when there is no floor
	int curr_station;					 		// 	tells us the lift pos
	int prev_station;							// where did we come from
	int delay_at_floor;				 		// tells us how long to stay at floor
	int delay_between_floors;	 		// tells us how long before teleport
	LIFT_MOTION motion;
	LIFT_CTRL_STATES state;
	BOOL floor_switches_fixed;					
	int SoundHandle; 

} LIFT_CONTROL_BLOCK;

// Whether a lift has everything it needs to run. Its stations, and each station's door and module, are found by numbers and names
// from the level file, so any of them can be missing, and the code that runs a lift reads through all of them with no check. So a
// lift that isn't complete doesn't run at all. No level that comes with the game has a lift of this kind, so this comes from how
// the code uses them, not from measuring levels. It's here and not in bh_lift.cpp so that a test can call it.
inline BOOL LiftIsComplete(const LIFT_CONTROL_BLOCK *lift)
{
	if (lift->num_stations < 1 || !lift->lift_stations)
	{
		return 0;
	}

	for (int i = 0; i < lift->num_stations; i++)
	{
		const LIFT_STATION *station = lift->lift_stations[i];

		if (!station || !station->lift_door || !station->lift_module)
		{
			return 0;
		}
	}

	return 1;
}


typedef struct lift_behaviour
{
	AVP_BEHAVIOUR_TYPE bhvr_type;
	char control_sb_name[SB_NAME_LENGTH];
	STRATEGYBLOCK* control_sb;	
	LIFT_CONTROL_BLOCK* lift_control;
	LIFT_STATION lift_station;
	int controller;

} LIFT_BEHAV_BLOCK;
	
#define LiftFlag_Here			0x00000001
#define LiftFlag_Airlock		0x00000002
#define LiftFlag_NoTel			0x00000004 /*switches aren't teleported*/

typedef struct lift_tools_template
{
	// for behaviour block
	char control_sb_name[SB_NAME_LENGTH];
	int controller;
	
	// for control block
	int num_stations;
	
	// for station block	
	char call_switch_name[SB_NAME_LENGTH];
	char lift_door_name[SB_NAME_LENGTH];
	char lift_floor_switch_name[SB_NAME_LENGTH];
	char my_module_name[SB_NAME_LENGTH];
	int num_floor;
	int lift_flags;

	// for strategy block
	MREF my_module;
	char nameID[SB_NAME_LENGTH];

} LIFT_TOOLS_TEMPLATE;
