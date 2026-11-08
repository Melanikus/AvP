// Modified 2026-11-06 by Melanikus for the AvP port:
//   the level row chooses a campaign, its difficulty and the number things with several possible places take theirs from
#ifndef _avp_env_info_h_
#define _avp_env_info_h_ 1

#define MAX_NO_OF_BASIC_ALIEN_EPISODES 5
#define MAX_NO_OF_BASIC_MARINE_EPISODES	6
#define MAX_NO_OF_BASIC_PREDATOR_EPISODES 6

#define MAX_NO_OF_ALIEN_EPISODES 10
#define MAX_NO_OF_MARINE_EPISODES 11
#define MAX_NO_OF_PREDATOR_EPISODES 11

// Edmond for Mplayer demo
#ifdef MPLAYER_DEMO
#define MAX_NO_OF_MULTIPLAYER_EPISODES 1
#else
#define MAX_NO_OF_MULTIPLAYER_EPISODES 14
#endif

#define MAX_NO_OF_COOPERATIVE_EPISODES 13

// The three campaigns, for a game in the mode where everything plays as it does in single player. 0 means this game isn't one. See SameAsSinglePlayer in pldnet.cpp.
enum
{
	kCampaignNone = 0,
	kCampaignMarine,
	kCampaignPredator,
	kCampaignAlien,

	// One custom map, played the same way. It's only a campaign once a co-op lobby has chosen a map: see Campaign_LevelCount.
	kCampaignCustom
};

// How many levels a campaign has. 0 for a number that isn't a campaign, and 0 for a custom map when no lobby has chosen one.
int Campaign_LevelCount(int campaign);

// The level at this place in a campaign, or -1 if there's no such place.
int Campaign_Environment(int campaign, int place);

// Loads the level netGameData says the campaign has got to.
void SetLevelToLoadForCampaign(void);

// What the level row on a host's page has chosen, in the two numbers the rest of the game asks. Only
// someone starting a game asks this: a joiner is told those numbers by the host.
void Campaign_SetFromLevelRow(void);

// Chooses the one number that everything with several possible places takes its place from. Only the host does this,
// once, as a game starts. See shared_roll.h.
void Campaign_ChooseRollNumber(void);

// Sets the difficulty this network game is played at, on every machine: the host's choice in a campaign, and Medium in any other.
void Campaign_SetDifficulty(void);

// Loads the level this network game is playing, whichever kind of game it is.
void SetLevelToLoadForThisNetworkGame(void);

enum AVP_ENVIRONMENT_ID
{
	// primarily Marine
	AVP_ENVIRONMENT_DERELICT,
	AVP_ENVIRONMENT_COLONY,
	AVP_ENVIRONMENT_INVASION,
	AVP_ENVIRONMENT_ORBITAL,
	AVP_ENVIRONMENT_TYRARGO,
	AVP_ENVIRONMENT_TYRARGOHANGAR,

	// primarily Predator
	AVP_ENVIRONMENT_WATERFALL,
	AVP_ENVIRONMENT_AREA52,
	AVP_ENVIRONMENT_VAULTS,
	AVP_ENVIRONMENT_FURY161,
	AVP_ENVIRONMENT_CAVERNS,
	AVP_ENVIRONMENT_CAVERNSEND,

	// primarily Alien
	AVP_ENVIRONMENT_FERARCO,
	AVP_ENVIRONMENT_TEMPLE,
	AVP_ENVIRONMENT_GATEWAY,
	AVP_ENVIRONMENT_ESCAPE,
	AVP_ENVIRONMENT_EARTHBOUND,

	// primarily Multiplayer
	AVP_ENVIRONMENT_SEWER,
	AVP_ENVIRONMENT_MASSACRE,
	AVP_ENVIRONMENT_STATUE,
	AVP_ENVIRONMENT_JOCKEY,
	AVP_ENVIRONMENT_HIVE,

	// Alien bonus levels 
	AVP_ENVIRONMENT_INVASION_A,
	AVP_ENVIRONMENT_DERELICT_A,
	AVP_ENVIRONMENT_TYRARGO_A,
	AVP_ENVIRONMENT_FURY161_A,
	AVP_ENVIRONMENT_CAVERNS_A,

	// Predator	bonus levels
	AVP_ENVIRONMENT_INVASION_P,
	AVP_ENVIRONMENT_TYRARGO_P,
	AVP_ENVIRONMENT_TEMPLE_P,
	AVP_ENVIRONMENT_ESCAPE_P,
	AVP_ENVIRONMENT_EARTHBOUND_P,
	
	// Marine bonus levels
	AVP_ENVIRONMENT_WATERFALL_M,
	AVP_ENVIRONMENT_VAULTS_M,
	AVP_ENVIRONMENT_FERARCO_M,
	AVP_ENVIRONMENT_TEMPLE_M,
	AVP_ENVIRONMENT_GATEWAY_M,

	// primarily Multiplayer cooperative
	AVP_ENVIRONMENT_KENS_COOP,
	AVP_ENVIRONMENT_HIVE_COOP,
	AVP_ENVIRONMENT_TRAPPED_COOP,
	AVP_ENVIRONMENT_ALS_DM_COOP,
	AVP_ENVIRONMENT_JOCKEY_COOP,
	
	// demo levels
	AVP_ENVIRONMENT_E3DEMOSP,

	AVP_ENVIRONMENT_END_OF_LIST,

	/*
	I'll put the multipack levels after the 'AVP_ENVIRONMENT_END_OF_LIST'
	so as to avoid having to add game statistic entries for the levels.
	*/

	//multipack multiplayer levels
	AVP_ENVIRONMENT_LEADWORKS_MP,
	AVP_ENVIRONMENT_HADLEYSHOPE_MP,
	AVP_ENVIRONMENT_MEATFACTORY_MP,
	AVP_ENVIRONMENT_NOSTROMO_MP,
	AVP_ENVIRONMENT_SUBWAY_MP,
	AVP_ENVIRONMENT_ELEVATOR_MP,
	AVP_ENVIRONMENT_LAB14_MP,
	AVP_ENVIRONMENT_COMPOUND_MP,
	AVP_ENVIRONMENT_OFFICE_MP,

	//multipack multiplayer cooperative levels
	AVP_ENVIRONMENT_LEADWORKS_COOP,
	AVP_ENVIRONMENT_HADLEYSHOPE_COOP,
	AVP_ENVIRONMENT_MEATFACTORY_COOP,
	AVP_ENVIRONMENT_NOSTROMO_COOP,
	AVP_ENVIRONMENT_SUBWAY_COOP,
	AVP_ENVIRONMENT_ELEVATOR_COOP,
	AVP_ENVIRONMENT_LAB14_COOP,
	AVP_ENVIRONMENT_COMPOUND_COOP,

	AVP_ENVIRONMENT_END_OF_MULTIPACK_LIST

};

extern void SetLevelToLoadForAlien(int episode);
extern void SetLevelToLoadForPredator(int episode);
extern void SetLevelToLoadForMarine(int episode);
extern void SetLevelToLoadForMultiplayer(int episode);
extern void SetLevelToLoadForCooperative(int episode);
extern void SetLevelToLoad(enum AVP_ENVIRONMENT_ID env);
extern void SetLevelToLoadForCheatMode(int environment);
extern int NumberForCurrentLevel(void);

#endif
