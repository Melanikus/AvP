// Modified 2026-11-07 by Melanikus for the AvP port:
//   how many marine classes there are (NUM_PC_SUBTYPES) is checked against the list of them when the game is built
//   the game description carries who the loading screen is waiting for
//   the co-op lobby: two new messages, the host's rows, and what the pages ask. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten)
//   whether a marine's shoulder lamp is on goes to the other machines in a bit the message already had free
// Modified 2026-11-06 by Melanikus for the AvP port:
//   the new guns' settings are in the game description, so a joiner uses the host's
//   in the same-as-single-player mode, a level starts for everyone together, with two new messages
//   a campaign's difficulty and its one number for places, in the game's data and the game description
//   the same-as-single-player mode: its one question, a campaign in the game description, and the message that moves everyone on
//   the game description has the players' names, because a client has no other way to learn them
//   three more marine classes from MelPyth M5, their limits and allow settings, and Allow Staff
//   what a client can afford, and whether the blocks are short, can be asked here
/* Patrick 10/7/97------------------------------------------------------
  Header for Multi-Player game support header
  ----------------------------------------------------------------------*/
#ifndef pldnet_h_included
#define pldnet_h_included

#define EXTRAPOLATION_TEST 1

extern NetID MultiplayerObservedPlayer;

/* Oh, for heaven's sake... */
#include "psnd.h"
#include "particle.h"
#include "coop_lobby.h"
/* ---------------------------------------------------------------------
  Some defines for multiplayer games
  ----------------------------------------------------------------------*/
#define NET_MAXPLAYERS              (8)
#define NET_MAXPLAYEROBJECTS        (15)
#define NET_MAXPLAYERSCORE          (500)
#define NET_MAXGAMETIME             (255)  /* minutes */
#define NET_MESSAGEBUFFERSIZE       (3072) /* an adequate number of bytes */
#define NET_PLAYERNAMELENGTH        (13)
#define NET_EULERSCALESHIFT         (4)
#define NET_MAXOBJECTID             (32767)
#define NET_MESSAGEITERATIONS       (5)
#define NET_MAXTEAMS                (4)

#define NET_IDNOTINPLAYERLIST       (-1)
#define NET_NOEMPTYSLOTINPLAYERLIST (-1)

#define NET_STARTUPINTEGRITY        (ONE_FIXED*2)

#define NETGAMESPEED_70PERCENT  0
#define NETGAMESPEED_80PERCENT  1
#define NETGAMESPEED_90PERCENT  2
#define NETGAMESPEED_100PERCENT 3

#define JOINNETGAME_WAITFORSTART    0
#define JOINNETGAME_WAITFORDESC     1
#define JOINNETGAME_WRONGAVPVERSION 2
#define JOINNETGAME_DONTHAVELEVEL   3

/* ---------------------------------------------------------------------
  Enum of message types
  ----------------------------------------------------------------------*/
typedef enum netmessagetype
{
	NetMT_GameDescription,
	NetMT_PlayerDescription,
	NetMT_StartGame,
	NetMT_PlayerState,
	NetMT_PlayerState_Minimal,
	NetMT_PlayerState_Medium,
	NetMT_PlayerKilled,
	NetMT_PlayerLeaving,
	NetMT_AllGameScores,
	NetMT_PlayerScores,
	NetMT_LocalRicochet,
	NetMT_LocalObjectState,
	NetMT_LocalObjectDamaged,
	NetMT_LocalObjectDestroyed,
	NetMT_ObjectPickedUp,
	NetMT_InanimateObjectDamaged,
	NetMT_InanimateObjectDestroyed,
	NetMT_LOSRequestBinarySwitch,
	NetMT_PlatformLiftState,
	NetMT_RequestPlatformLiftActivate,
	NetMT_PlayerAutoGunState,
	NetMT_MakeDecal,
	NetMT_ChatBroadcast,
	NetMT_MakeExplosion,
	NetMT_MakeFlechetteExplosion,
	NetMT_MakePlasmaExplosion,
	NetMT_PredatorSights,
	NetMT_LocalObjectOnFire,
	NetMT_RestartNetworkGame,
	NetMT_FragmentalObjectsStatus,
	NetMT_AlienAIState,
	NetMT_AlienAISequenceChange,
	NetMT_AlienAIKilled,
	NetMT_GhostHierarchyDamaged,
	NetMT_SpotAlienSound,
	NetMT_LocalObjectDestroyed_Request,
	NetMT_LastManStanding_Restart,
	NetMT_LastManStanding_RestartInfo,
	NetMT_LastManStanding_RestartCountDown,
	NetMT_LastManStanding_LastMan,
	NetMT_PredatorTag_NewPredator,    //same message also used for alien tag
	NetMT_EndGame,
	NetMT_CreateWeapon,
	NetMT_RespawnPickups,
	NetMT_ScoreChange,
	NetMT_Gibbing,
	NetMT_CorpseDeathAnim,
	NetMT_StrategySynch,
	NetMT_FrameTimer,
	NetMT_SpeciesScores,
	NetMT_FarAlienPosition,
	NetMT_SpotOtherSound,
	NetMT_CoopNextLevel,
	NetMT_LevelEvent,
	NetMT_LevelEventAsk,
	NetMT_CoopReady,
	NetMT_CoopGo,
	NetMT_NpcState,
	NetMT_NpcXenoDeltas,
	NetMT_NpcKilled,
	NetMT_NpcSound,
	NetMT_NpcThrownObject,
	NetMT_LiftState,
	NetMT_CoopCampaignChoice,
	NetMT_CoopMapAnswer
} NETMESSAGETYPE;


/* ---------------------------------------------------------------------
  Enums of game types, and local game states
  ----------------------------------------------------------------------*/
typedef enum netgame_type
{
	NGT_Individual,
	NGT_CoopDeathmatch,
	NGT_LastManStanding,
	NGT_PredatorTag,
	NGT_Coop,
	NGT_AlienTag
} NETGAME_TYPE;

typedef enum netgame_states
{
	NGS_StartUp,
	NGS_Joining,
	NGS_Playing,
	NGS_Leaving,
	NGS_EndGame,
	NGS_Error_GameFull,
	NGS_Error_GameStarted,
	NGS_Error_HostLost,
	NGS_EndGameScreen
} NETGAME_STATES;


#define NUM_PC_TYPES 3
typedef enum netgame_charactertype
{
	NGCT_Marine,
	NGCT_Predator,
	NGCT_Alien,
	NGCT_AI_Alien,
	NGCT_AI_Predalien,
	NGCT_AI_Praetorian
} NETGAME_CHARACTERTYPE;

#define NUM_PC_SUBTYPES 12
typedef enum netgame_specialistcharactertype
{
	NGSCT_General,
	NGSCT_PulseRifle,
	NGSCT_Smartgun,
	NGSCT_Flamer,
	NGSCT_Sadar,
	NGSCT_GrenadeLauncher,
	NGSCT_Minigun,
	NGSCT_Frisbee,
	NGSCT_Pistols,
	NGSCT_Shotgun,
	NGSCT_Revolver,
	NGSCT_M79
} NETGAME_SPECIALISTCHARACTERTYPE;

// NUM_PC_SUBTYPES above is how many classes there are. It sizes the lists kept for each class, and a class number that comes over the
// network has to be below it. This stops the build if a class is added to the list without it.
static_assert(NUM_PC_SUBTYPES == NGSCT_M79 + 1, "NUM_PC_SUBTYPES has to be how many classes NETGAME_SPECIALISTCHARACTERTYPE has");

/* ---------------------------------------------------------------------
  Player data structure, and game description data structure
  ----------------------------------------------------------------------*/
typedef struct netgame_playerdata
{
	NetID playerId;
	char name[NET_PLAYERNAMELENGTH];
	NETGAME_CHARACTERTYPE characterType;
	NETGAME_SPECIALISTCHARACTERTYPE characterSubType;
	int playerFrags[NET_MAXPLAYERS];
	int aliensKilled[3];
	int deathsFromAI;
	int playerScore;
	int playerScoreAgainst; //points scored by killing this player

	VECTORCH lastKnownPosition;

	unsigned int timer; //used by extrapolation stuff (doesn't matter that it isn't initialised)

	unsigned char startFlag;
	unsigned char playerAlive: 1;
	unsigned char playerHasLives: 1;
} NETGAME_PLAYERDATA;

typedef struct netgame_gamedata
{
	NETGAME_STATES myGameState;
	NETGAME_CHARACTERTYPE myCharacterType;
	NETGAME_CHARACTERTYPE myNextCharacterType; //if player is currently dead and about to become a new character
	NETGAME_SPECIALISTCHARACTERTYPE myCharacterSubType;
	unsigned char myStartFlag;
	NETGAME_PLAYERDATA playerData[NET_MAXPLAYERS];
	int teamScores[3];
	NETGAME_TYPE gameType;
	int levelNumber;
	int scoreLimit;
	int timeLimit;
	int invulnerableTime;//in seconds after respawn
	int GameTimeElapsed;

	//scoring system stuff
	int characterKillValues[3];
	int baseKillValue;
	BOOL useDynamicScoring;
	BOOL useCharacterKillValues;

	int aiKillValues[3];

	//for last man standing game
	int LMS_AlienIndex;
	int LMS_RestartTimer;

	int stateCheckTimeDelay;

	/*following timer used to prevent the game description from being sent every frame*/
	int gameDescriptionTimeDelay;

	/*sendFrequencey - how often to send messages in fixed point seconds*/
	int sendFrequency;
	int sendTimer;

	//player type limits
	int maxPredator;
	int maxAlien;
	int maxMarine;

	int maxMarineGeneral;
	int maxMarinePulseRifle;
	int maxMarineSmartgun;
	int maxMarineFlamer;
	int maxMarineSadar;
	int maxMarineGrenade;
	int maxMarineMinigun;
	int maxMarineSmartDisc;
	int maxMarinePistols;
	int maxMarineShotgun;
	int maxMarineRevolver;
	int maxMarineM79;

	//weapons allowed
	BOOL allowSmartgun;
	BOOL allowFlamer;
	BOOL allowSadar;
	BOOL allowGrenadeLauncher;
	BOOL allowMinigun;
	BOOL allowDisc;
	BOOL allowPistol;
	BOOL allowPlasmaCaster;
	BOOL allowSpeargun;
	BOOL allowMedicomp;
	BOOL allowSmartDisc;
	BOOL allowPistols;
	BOOL allowShotgun;
	BOOL allowRevolver;
	BOOL allowM79;
	BOOL allowStaff;

	//lives
	int maxLives;
	BOOL useSharedLives;
	int numDeaths[3];    //deaths for each species

	//object respawn
	int pointsForRespawn;
	int timeForRespawn; //seconds

	int lastPointsBasedRespawn;
	
	BOOL sendDecals;
	unsigned int needGameDescription: 1;

	BOOL skirmishMode; //for single player multiplayer games

	int myLastScream;

	int gameSpeed; //0 to 3, for 70,80,90 or 100 percent speed

	BOOL preDestroyLights;
	BOOL disableFriendlyFire; // stops people on the same team from hurting each other
	BOOL fallingDamage;
	BOOL pistolInfiniteAmmo;
	BOOL specialistPistols;

	// don't bother tring to synch strategies if the checksum values are different
	int myStrategyCheckSum;

	unsigned int landingNoise: 1;

	int joiningGameStatus;

	char customLevelName[40];

	// Which campaign this game is playing, and how far it's got. See env_info.h. A campaign is the mode where everything plays as it does in single player: see SameAsSinglePlayer.
	int coopCampaign;
	int coopCampaignLevel;

	// The difficulty a campaign is played at, chosen by the host, and only for a campaign. Every other network game is at Medium.
	int coopDifficulty;

	// One number for the whole game, chosen by the host, that things with several possible places take theirs from. See shared_roll.h.
	int coopRollNumber;

	// 1 if each player keeps their kit between the levels of a campaign (campaign_kit.h), and 0 for each level's own kit. Only the co-op campaign page turns it on.
	int coopKeepKit;

	// On the loading screen, the places of the players who haven't pressed a key yet, one bit each. The host works it out, and a joiner
	// has it from the game description. See coop_in_game.h.
	unsigned char coopWaitingFor;

	// A custom map chosen in a co-op lobby: its name, who plays it (0 Marine, 1 Predator, 2 Alien), and its size and sum, only to compare. See coop_lobby.h.
	// No name means no custom map.
	char coopCustomMap[kCoopMapNameBytes];
	int coopCustomSpecies;
	unsigned int coopCustomMapSize;
	unsigned char coopCustomMapSum[kCoopMapSumBytes];

} NETGAME_GAMEDATA;

/* ---------------------------------------------------------------------
  Individual message structures
  ----------------------------------------------------------------------*/
#pragma pack(push, 1)

// What the host says about an NPC near someone. Only for the kinds in npc_kinds.h: aliens have their own. See npc_ghosts.cpp.
typedef struct netmessage_npcstate
{
	signed int Guid;

	signed int xPos: 23;
	signed int xOrient: 9;
	signed int yPos: 23;
	signed int yOrient: 9;
	signed int zPos: 23;
	signed int zOrient: 9;

	unsigned char kind;
	unsigned char model;    // a marine's or predator's weapon
	unsigned char clothes;  // a marine's clothes and face: which of his model's sets
	unsigned char flags;    // see npc_kinds.h

	unsigned char sequence_type;
	unsigned char sub_sequence;
	unsigned short sequence_length;   // in 256ths of a second

	unsigned short aim;     // how far up or down a marine or predator is aiming. A queen: how far through her swipe
	unsigned char extra1;   // a predator: how cloaked he is, see NpcCloak_ToByte. A queen: the animation type of her swipe
	unsigned char extra2;   // a queen: her swipe's number plus one, and 0 if she isn't swiping
} NETMESSAGE_NPCSTATE;
static_assert(sizeof(NETMESSAGE_NPCSTATE) == 28, "the messages are sized by hand: see the row in kNetPartRules");

// Where a xenoborg's head, arms and body are turned, each in a byte, and which of his beams are on
typedef struct netmessage_npcxenodeltas
{
	signed int Guid;
	unsigned char headPan, headTilt;
	unsigned char leftArmPan, leftArmTilt;
	unsigned char rightArmPan, rightArmTilt;
	unsigned char torsoTwist;
	unsigned char beams;    // which of his five beams are on, one bit each: see kNpcXenoBeamHead
} NETMESSAGE_NPCXENODELTAS;
static_assert(sizeof(NETMESSAGE_NPCXENODELTAS) == 12, "the messages are sized by hand: see the row in kNetPartRules");

// One of an NPC's voice sounds, at a place, for a machine that only has a ghost of it
typedef struct netmessage_npcsound
{
	unsigned char species;   // kNpcSound..., see npc_kinds.h
	unsigned char voice;
	unsigned char category;
	unsigned char spare;
	int pitch;
	int vx;
	int vy;
	int vz;
} NETMESSAGE_NPCSOUND;
// Where one of the things a queen throws has got to, for a machine that isn't running her. It's found by its name in the
// level, which is the same on every machine.
typedef struct netmessage_npcthrownobject
{
	char name[8];
	int vx;
	int vy;
	int vz;
	short xOrient;   // which way round it is, in the game's units of angle
	short yOrient;
	short zOrient;
	short spare;
} NETMESSAGE_NPCTHROWNOBJECT;
static_assert(sizeof(NETMESSAGE_NPCTHROWNOBJECT) == 28, "the messages are sized by hand: see the table of them in pldnet.cpp");

// Where a platform lift's car is, from the host, in the mode that plays as single player. Every other machine's car follows it
// (bh_platform_lift.cpp). The lift is found by its name in the level, which is the same on every machine.
typedef struct netmessage_liftstate
{
	char name[8];
	int y;                // the car's height
	unsigned char state;  // PLATFORMLIFT_STATES: 0 at rest, 1 about to move, 2 going up, 3 going down
	unsigned char end;    // the end it's going to, or resting at: 0 the top (upHeight), 1 the bottom (downHeight)
	unsigned short spare;
} NETMESSAGE_LIFTSTATE;
static_assert(sizeof(NETMESSAGE_LIFTSTATE) == 16, "the messages are sized by hand: see the table of them in pldnet.cpp");

static_assert(sizeof(NETMESSAGE_NPCSOUND) == 20, "the messages are sized by hand: see the row in kNetPartRules");

typedef struct netmessage_npckilled
{
	signed int Guid;
	int death_code;
	int death_time;
	int GibbFactor;
} NETMESSAGE_NPCKILLED;
static_assert(sizeof(NETMESSAGE_NPCKILLED) == 16, "the messages are sized by hand: see the row in kNetPartRules");

typedef struct netmessageheader
{
	unsigned char type;
} NETMESSAGEHEADER;

typedef struct gamedescription_playerdata
{
	NetID playerId;
	unsigned char characterType: 2;
	unsigned char characterSubType: 6;
	unsigned char startFlag;
	char name[NET_PLAYERNAMELENGTH];
} GAMEDESCRIPTION_PLAYERDATA;

typedef struct netmessage_gamedescription
{
	GAMEDESCRIPTION_PLAYERDATA players[NET_MAXPLAYERS];
	unsigned char gameType;
	unsigned char levelNumber;
	unsigned char coopCampaign;
	unsigned char coopCampaignLevel;
	unsigned char coopDifficulty;
	unsigned char coopWaitingFor;
	int coopRollNumber;
	int scoreLimit;
	unsigned char timeLimit;
	unsigned char invulnerableTime;

	unsigned char maxLives;
	unsigned char numDeaths[3];
	unsigned char timeForRespawn;
	int pointsForRespawn;

	short GameTimeElapsed;//in seconds

	//scoring system stuff
	unsigned char characterKillValues[3];
	unsigned char baseKillValue;
	unsigned int useDynamicScoring: 1;
	unsigned int useCharacterKillValues: 1;
	
	unsigned char aiKillValues[3];

	unsigned int sendDecals: 1;
	//weapons allowed
	unsigned int allowSmartgun: 1;
	unsigned int allowFlamer: 1;
	unsigned int allowSadar: 1;
	unsigned int allowGrenadeLauncher: 1;
	unsigned int allowMinigun: 1;
	unsigned int allowDisc: 1;
	unsigned int allowPistol: 1;
	unsigned int allowPlasmaCaster: 1;
	unsigned int allowSpeargun: 1;
	unsigned int allowMedicomp: 1;
	unsigned int allowSmartDisc: 1;
	unsigned int allowPistols: 1;

	//player type limits
	unsigned int maxPredator: 4;
	unsigned int maxAlien: 4;
	unsigned int maxMarine: 4;

	unsigned int maxMarineGeneral: 4;
	unsigned int maxMarinePulseRifle: 4;
	unsigned int maxMarineSmartgun: 4;
	unsigned int maxMarineFlamer: 4;
	unsigned int maxMarineSadar: 4;
	unsigned int maxMarineGrenade: 4;
	unsigned int maxMarineMinigun: 4;
	unsigned int maxMarineSmartDisc: 4;
	unsigned int maxMarinePistols: 4;

	unsigned int useSharedLives: 1;

	unsigned int gameSpeed: 2;

	unsigned int endGame: 1;

	unsigned int preDestroyLights: 1;
	unsigned int disableFriendlyFire: 1;
	unsigned int fallingDamage: 1;
	unsigned int pistolInfiniteAmmo: 1;
	unsigned int specialistPistols: 1;

	// The guns added later, the same as the older ones above. A joiner has to use the host's, or the two machines would take different pick-ups off the level as it loads, and their lists of objects, which the sync
	// goes by, would then differ. They're in the room the last field had left, so the message is the same size.
	unsigned int allowShotgun: 1;
	unsigned int allowRevolver: 1;
	unsigned int allowM79: 1;
	unsigned int allowStaff: 1;
	unsigned int maxMarineShotgun: 4;
	unsigned int maxMarineRevolver: 4;
	unsigned int maxMarineM79: 4;
} NETMESSAGE_GAMEDESCRIPTION;

typedef struct netmessage_playerdescription
{
	unsigned char characterType: 3;
	unsigned char characterSubType: 4;
	unsigned char startFlag: 1;
} NETMESSAGE_PLAYERDESCRIPTION;

typedef struct netmessage_playerstate
{
	unsigned char characterType: 2; //send character type each frame (in case it changes)
	unsigned char nextCharacterType: 2; //if player is currently dead and about to become a new character
	unsigned char characterSubType: 4;
	signed int xPos: 23;
	signed int xOrient: 9;
	signed int yPos: 23;
	signed int yOrient: 9;
	signed int zPos: 23;
	signed int zOrient: 9;
	unsigned char sequence;
	unsigned char currentWeapon;
	unsigned short CloakingEffectiveness;

	unsigned int Elevation: 12;
	unsigned int IHaveAMuzzleFlash: 2;
	unsigned int IAmFiringPrimary: 1;
	unsigned int IAmFiringSecondary: 1;
	unsigned int IAmAlive: 1;
	unsigned int IAmOnFire: 1;
	unsigned int IHaveADisk: 1;
	unsigned int IHaveLifeLeft: 1;
	unsigned int IAmCrouched: 1;
	unsigned int Special: 1;

	unsigned int scream: 5;

	unsigned int IAmInvulnerable: 1;

#if EXTRAPOLATION_TEST
	//this lot will need to be sent more efficiently
	int standard_gravity:1;
	int velocity_x: 10; //in 10's of cm/second
	int velocity_y: 10; //in 10's of cm/second
	int velocity_z: 10; //in 10's of cm/second
#endif
	unsigned int landingNoise: 1;
	unsigned int lampOn: 1;   // his shoulder lamp is on, in the mode that plays as single player. Always 0 in any other
} NETMESSAGE_PLAYERSTATE;

// The lamp's bit went in a part that had room, so the message didn't grow. If it ever does, the sizes in kNetPartRules and the version need a look.
static_assert(sizeof(NETMESSAGE_PLAYERSTATE) == 25, "the messages are sized by hand: see the row in kNetPartRules");

typedef struct netmessage_playerstate_minimal
{
	unsigned short Elevation: 12;
	unsigned short IHaveAMuzzleFlash: 2;
	unsigned short IAmFiringPrimary: 1;
	unsigned short IAmFiringSecondary: 1;
	unsigned short IAmAlive: 1;
	unsigned char IAmOnFire: 1;
	unsigned char IHaveADisk: 1;
	unsigned char IHaveLifeLeft: 1;
	unsigned char Special: 1;

	unsigned char CloakingEffectiveness;
} NETMESSAGE_PLAYERSTATE_MINIMAL;

typedef struct netmessage_playerstate_medium
{
	NETMESSAGE_PLAYERSTATE_MINIMAL minimalMessage;

	signed int xOrient: 9;
	signed int yOrient: 9;
	signed int zOrient: 9;
} NETMESSAGE_PLAYERSTATE_MEDIUM;

typedef struct netmessage_frametimer
{
	unsigned short frame_time;
} NETMESSAGE_FRAMETIMER;

typedef struct netmessage_playerkilled
{
	int objectId;
	NetID killerId;
	NETGAME_CHARACTERTYPE myType;  //take character types at time of death , in case they change
	NETGAME_CHARACTERTYPE killerType;
	char weaponIcon;
} NETMESSAGE_PLAYERKILLED;

typedef struct netmessage_corpsedeathanim
{
	int objectId;
	int deathId;
} NETMESSAGE_CORPSEDEATHANIM;

typedef struct netmessage_allgamescores
{
	int playerFrags[NET_MAXPLAYERS][NET_MAXPLAYERS];
	int playerScores[NET_MAXPLAYERS];
	int playerScoresAgainst[NET_MAXPLAYERS];
	int aliensKilled[NET_MAXPLAYERS][3];
	int deathsFromAI[NET_MAXPLAYERS];
} NETMESSAGE_ALLGAMESCORES;

typedef struct netmessage_speciesscores
{
	int teamScores[3];
} NETMESSAGE_SPECIESSCORES;

typedef struct netmessage_playerscores
{
	unsigned char playerId;
	int playerFrags[NET_MAXPLAYERS];
	int playerScore;
	int playerScoreAgainst;
	int aliensKilled[3];
	int deathsFromAI;
} NETMESSAGE_PLAYERSCORES;

typedef struct netmessage_scorechange
{
	unsigned char killerIndex;
	unsigned char victimIndex;
	int fragCount;
	int killerScoreFor;
	int victimScoreAgainst;
} NETMESSAGE_SCORECHANGE;

typedef struct netmessage_localRicochet
{
	signed int xPos;
	signed int yPos;
	signed int zPos;
	signed int xDirn;
	signed int yDirn;
	signed int zDirn;
	unsigned char type;
} NETMESSAGE_LOCALRICOCHET;

typedef struct netmessage_lobstate
{
	signed int xPos: 23;
	signed int xOrient: 9;
	signed int yPos: 23;
	signed int yOrient: 9;
	signed int zPos: 23;
	signed int zOrient: 9;
	signed int objectId;
	unsigned char type;
	unsigned char IOType;
	unsigned char subtype;
	unsigned char event_flag;
} NETMESSAGE_LOBSTATE;


//damage message is now split into multiple parts , to avoid sending
//stuff that isn't required
typedef struct netmessage_lobdamaged_header
{
	NetID playerId;
	signed int objectId;
	short ammo_id: 11;
	
	short damageProfile: 1;
	short multiple: 1;
	short sectionID: 1;
	short delta_seq: 1;
	short direction: 1;
} NETMESSAGE_LOBDAMAGED_HEADER;

typedef struct netmessage_ghosthierarchydamaged_header
{
	signed int Guid;
	short ammo_id: 11;
	
	short damageProfile: 1;
	short multiple: 1;
	short sectionID: 1;
	short direction: 1;
} NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER;

typedef struct netmessage_inanimatedamaged_header
{
	char name[8];
	short ammo_id: 11;
	
	short damageProfile: 1;
	short multiple: 1;
} NETMESSAGE_INANIMATEDAMAGED_HEADER;

typedef struct netmessage_damage_profile
{
	short Impact;    /* nb I have copied these, as I don't think*/
	short Cutting;   /* the structure will pack if I insert it directly */
	short Penetrative;
	short Fire;
	short Electrical;
	short Acid;

	unsigned int ExplosivePower :3;
	unsigned int Slicing        :2;
	unsigned int ProduceBlood   :1;
	unsigned int ForceBoom      :2;
	unsigned int BlowUpSections :1;
	unsigned int Special        :1;
	unsigned int MakeExitWounds :1;
} NETMESSAGE_DAMAGE_PROFILE;

typedef struct netmessage_damage_multiple
{
	int multiple;
} NETMESSAGE_DAMAGE_MULTIPLE;

typedef struct netmessage_damage_section
{
	short SectionID;
} NETMESSAGE_DAMAGE_SECTION;

typedef struct netmessage_damage_delta
{
	char Delta_Sequence;
	char Delta_Sub_Sequence;
} NETMESSAGE_DAMAGE_DELTA;

typedef struct netmessage_damage_direction
{
	int direction_x:10;
	int direction_y:10;
	int direction_z:10;
} NETMESSAGE_DAMAGE_DIRECTION;
//that was the last part of the local object damage stuff

// From the host only: every machine goes on to this place in the campaign. The place just past the last level means the campaign is over.
// Something that only happened on one machine, by the name of the object it happened to. Each machine then does what its own copy of the level says that object does. NetMT_LevelEvent is the host saying it has happened. NetMT_LevelEventAsk is a client asking the host to agree that it has.
typedef struct netmessage_levelevent
{
	char source[8];
} NETMESSAGE_LEVELEVENT;

typedef struct netmessage_coopnextlevel
{
	unsigned char campaign;
	unsigned char place;
} NETMESSAGE_COOPNEXTLEVEL;

typedef struct netmessage_lobdestroyed_request
{
	NetID playerId;
	signed int objectId;
} NETMESSAGE_LOBDESTROYED_REQUEST;

typedef struct netmessage_lobdestroyed
{
	signed int objectId;
} NETMESSAGE_LOBDESTROYED;

typedef struct netmessage_objectpickedup
{
	char name[8];
} NETMESSAGE_OBJECTPICKEDUP;

typedef struct netmessage_inanimatedamaged
{
	char name[8];
	short Impact;    /* nb I have copied these, as I don't think*/
	short Cutting;   /* the structure will pack if I insert it directly */
	short Penetrative;
	short Fire;
	short Electrical;
	short Acid;

	unsigned int ExplosivePower :3;
	unsigned int Slicing        :2;
	unsigned int ProduceBlood   :1;
	unsigned int ForceBoom      :2;
	unsigned int BlowUpSections :1;
	unsigned int Special        :1;
	unsigned int MakeExitWounds :1;

	enum AMMO_ID ammo_id;

	int multiple;

} NETMESSAGE_INANIMATEDAMAGED;

typedef struct netmessage_inanimatedestroyed
{
	char name[8];
} NETMESSAGE_INANIMATEDESTROYED;

typedef struct netmessage_losrequestbinaryswitch
{
	char name[8];
} NETMESSAGE_LOSREQUESTBINARYSWITCH;

typedef struct netmessage_platformliftstate
{
	char name[8];
	char state;
} NETMESSAGE_PLATFORMLIFTSTATE;

typedef struct netmessage_requestplatformliftactivate
{
	char name[8];
} NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE;

typedef struct netmessage_agunstate
{
	signed int xPos: 23;
	signed int xOrient: 9;
	signed int yPos: 23;
	signed int yOrient: 9;
	signed int zPos: 23;
	signed int zOrient: 9;
	signed int objectId;
	unsigned char IAmFiring: 1;
	unsigned char IAmEnabled: 1;
} NETMESSAGE_AGUNSTATE;

/* KJL 17:45:21 20/01/98 - make decal message */
/* currently not optimised for space! */
#include "decal.h"
typedef struct netmessage_makedecal
{
	enum DECAL_ID DecalID;
	VECTORCH Position;
	VECTORCH Direction;
	int ModuleIndex;
} NETMESSAGE_MAKEDECAL;

/* KJL 11:32:52 27/04/98 - explosions */
typedef struct netmessage_makeexplosion
{
	enum EXPLOSION_ID ExplosionID;
	VECTORCH Position;
} NETMESSAGE_MAKEEXPLOSION;

typedef struct netmessage_makeflechetteexplosion
{
	VECTORCH Position;
	int Seed;
} NETMESSAGE_MAKEFLECHETTEEXPLOSION;

typedef struct netmessage_makeplasmaexplosion
{
	enum EXPLOSION_ID ExplosionID;
	VECTORCH Position;
	VECTORCH FromPosition;
} NETMESSAGE_MAKEPLASMAEXPLOSION;

/* KJL 11:13:59 20/05/98 - pred laser sights */
typedef struct netmessage_predatorsights
{
//	THREE_LASER_DOT_DESC Dots;
// was 85 bytes
	
	signed int xPos: 23;
	signed int xOrient: 9;
	signed int yPos: 23;
	signed int yOrient: 9;
	signed int zPos: 23;
	signed int zOrient: 9;

	NetID TargetID;
} NETMESSAGE_PREDATORSIGHTS;

typedef struct netmessage_lobonfire
{
	NetID playerId;
	signed int objectId;
} NETMESSAGE_LOBONFIRE;

typedef struct netmessage_alienaistate
{
	signed int Guid;

	signed int xPos: 23;
	signed int xOrient: 9;
	signed int yPos: 23;
	signed int yOrient: 9;
	signed int zPos: 23;
	signed int zOrient: 9;

	unsigned char sequence_type;
	unsigned char sub_sequence;
	unsigned short sequence_length:13; // in 256ths of a second , up to ~32 seconds

	unsigned short IAmOnFire: 1;
	unsigned short AlienType: 2; // alien/predalien/praetorian

	#if EXTRAPOLATION_TEST
	unsigned short speed:15;
	unsigned short standard_gravity:1;
	#endif
} NETMESSAGE_ALIENAISTATE;

typedef struct netmessage_aliensequencechange
{
	signed int Guid;

	unsigned char sequence_type;
	unsigned char sub_sequence;
	short sequence_length; //in 256ths of a second
	short tweening_time;
} NETMESSAGE_ALIENSEQUENCECHANGE;

typedef struct netmessage_alienaikilled
{
	signed int Guid;
	
	int death_code;
	int death_time;
	int GibbFactor;

	NetID killerId;
	int killCount;
	unsigned char AlienType: 2;//alien/predalien/praetorian

	char weaponIcon;
} NETMESSAGE_ALIENAIKILLED;

typedef struct netmessage_faralienposition
{
	signed int Guid;

	unsigned int targetModuleIndex:14;
	unsigned int index:15; //index is either module index , or an index in the aux locations list
	unsigned int indexIsModuleIndex:1;
	unsigned int alienType:2;
} NETMESSAGE_FARALIENPOSITION;

typedef struct netmessage_gibbing
{
	signed int Guid;
	int gibbFactor;
	int seed;
} NETMESSAGE_GIBBING;

typedef struct netmessage_spotaliensound
{
	unsigned char soundCategory:6;
	unsigned char alienType:2;
	int pitch;
	int vx;
	int vy;
	int vz;
} NETMESSAGE_SPOTALIENSOUND;

typedef struct netmessage_createweapon
{
	char name[8];
	VECTORCH location;
	int type;
} NETMESSAGE_CREATEWEAPON;

#define NUMBER_OF_FRAGMENTAL_OBJECTS (64>>3)
typedef struct netmessage_fragmentalobjectsstatus
{
	unsigned char BatchNumber; //send object states over several frames
	unsigned char StatusBitfield[NUMBER_OF_FRAGMENTAL_OBJECTS];
} NETMESSAGE_FRAGMENTALOBJECTSSTATUS;

#define NUMBER_OF_STRATEGIES_TO_SYNCH 16
typedef struct netmessage_strategysynch
{
	unsigned char BatchNumber; //send object states over several frames
	int strategyCheckSum;
	unsigned char StatusBitfield[NUMBER_OF_STRATEGIES_TO_SYNCH>>2]; //2bits per strategy
} NETMESSAGE_STRATEGYSYNCH;

//for messages that just require a player id
typedef struct netmessage_playerid
{
	NetID playerID;
} NETMESSAGE_PLAYERID;

typedef struct netmessage_lms_restart
{
	NetID playerID; 
	int seed;
} NETMESSAGE_LMS_RESTART;

typedef struct netmessage_restartgame
{
	int seed;
} NETMESSAGE_RESTARTGAME;

//countdown to restart
typedef struct netmessage_lms_restarttimer
{
	unsigned char timer;
} NETMESSAGE_LMS_RESTARTTIMER;

typedef struct netmessage_spotothersound
{
	enum soundindex SoundIndex;
	int vx;
	int vy;
	int vz;
	int explosion:1;
} NETMESSAGE_SPOTOTHERSOUND;

typedef struct multiplayer_start
{
	VECTORCH location;
	EULER orientation;
} MULTIPLAYER_START;

#pragma pack(pop)


/* ---------------------------------------------------------------------
   Some prototypes
  ----------------------------------------------------------------------*/
extern void NetCollectMessages(void);

// The co-op lobby. See coop_lobby.h for what it is.
int Net_CoopLobbyState(void);                 // kCoopLobbyNone, Open or UnderWay, for this machine
void Net_CoopLobbyOpens(void);                // a host has opened the page
void Net_CoopLobbyJoined(int state);          // a joiner has connected to a game whose description said this
void Net_CoopLobbyEnds(void);                 // a joiner has been pulled into the load, or has left
void Net_CoopHostSays(void);                  // a host says what's chosen: at once if a row has changed, and otherwise once a second
void Net_CoopHostStarts(void);                // a host has pressed Start, and everyone who has the map goes
void Net_CoopJoinerSays(void);                // a joiner answers about the map: at once if that has changed, and otherwise every 2 seconds
bool Net_CoopChoiceIsHeard(void);             // has this joiner heard what the host has chosen?
bool Net_CoopHostHasStarted(void);            // has this joiner heard the host press Start?
int Net_CoopMyMapAnswer(void);                // what this joiner found out about the map
bool Net_CoopPlayerHasTheMap(int place);      // on a host: is the player at this place known to have the map?
void Net_CoopTakeTheCampaign(void);           // this machine is about to load: everything the rows decided takes effect
extern void NetSendMessages(void);
extern void EndAVPNetGame(void);
extern int PlayerIdInPlayerList(NetID Id);
extern void RecordFinalNetGameScores(void);
extern void DoNetScoresForHostDeath(NETGAME_CHARACTERTYPE myType,NETGAME_CHARACTERTYPE killerType);
extern void RemovePlayerFromGame(NetID id);
extern bool AddPlayerToGame(NetID id, const char *name);
extern void Net_PlayerHasGone(NetID id);
extern void Net_HostHasGone(void);
extern int EmptySlotInPlayerList(void);
extern void TeleportNetPlayerToAStartingPosition(STRATEGYBLOCK *playerSbPtr, int startOfGame);
extern int AddUpPlayerFrags(int playerId);

extern void AddNetMsg_GameDescription(void);
extern void AddNetMsg_PlayerDescription(void);
extern void AddNetMsg_StartGame(void);
extern void AddNetMsg_PlayerState(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_PlayerState_Minimal(STRATEGYBLOCK *sbPtr,BOOL sendOrient);
extern void AddNetMsg_FrameTimer();
extern void AddNetMsg_PlayerKilled(int objectId,DAMAGE_PROFILE* damage);
extern void AddNetMsg_PlayerDeathAnim(int deathId, int objectId);
extern void AddNetMsg_PlayerLeaving(void);
extern void AddNetMsg_AllGameScores(void);
extern void AddNetMsg_PlayerScores(int playerId);
extern void AddNetMsg_SpeciesScores();
extern void AddNetMsg_LocalRicochet(AVP_BEHAVIOUR_TYPE behaviourType, VECTORCH *position, VECTORCH *direction);
extern void AddNetMsg_LocalObjectState(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_LocalObjectDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple, int sectionID,int delta_seq,int delta_sub_seq,VECTORCH* incoming);
extern void AddNetMsg_LocalObjectDestroyed(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_ObjectPickedUp(char *objectName);
extern void AddNetMsg_InanimateObjectDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple);
extern void AddNetMsg_InanimateObjectDestroyed(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_LOSRequestBinarySwitch(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_PlatformLiftState(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_RequestPlatformLiftActivate(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_RequestPlatformLiftReverse(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_PlayerAutoGunState(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_EndGame(void);
extern void AddNetMsg_MakeDecal(enum DECAL_ID decalID, VECTORCH *normalPtr, VECTORCH *positionPtr, int moduleIndex);
extern void AddNetMsg_ChatBroadcast(char *string,BOOL same_species_only);
extern void AddNetMsg_MakeExplosion(VECTORCH *positionPtr, enum EXPLOSION_ID explosionID);
extern void AddNetMsg_MakeFlechetteExplosion(VECTORCH *positionPtr, int seed);
extern void AddNetMsg_MakePlasmaExplosion(VECTORCH *positionPtr, VECTORCH *fromPositionPtr, enum EXPLOSION_ID explosionID);
extern void AddNetMsg_PredatorLaserSights(VECTORCH *positionPtr, VECTORCH *normalPtr, DISPLAYBLOCK *dispPtr);
extern void AddNetMsg_LocalObjectOnFire(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_RestartNetworkGame(int seed);

// THE ONE QUESTION for the mode where a network game plays as single player does. Anywhere a test on AvP.Network changes how a level behaves, that test asks this as well. True in single player, and in a game of this mode.
extern bool SameAsSinglePlayer(void);

// The host tells everyone that an NPC of the kinds in npc_kinds.h has died
extern void AddNetMsg_NpcKilled(STRATEGYBLOCK *sbPtr, int deathCode, int deathTime, int gibbFactor);
extern void AddNetMsg_NpcThrownObject(STRATEGYBLOCK *object);
extern void AddNetMsg_LiftState(STRATEGYBLOCK *lift);
extern void AddNetMsg_NpcSound(int species, int voice, int category, int pitch, VECTORCH const *place);
// In the mode that plays as single player, a dead player is out until the next level. True when everyone is out.
extern bool Coop_EveryoneIsDead(void);

// Each frame that this machine's player is out. He watches someone who's alive, and "use" moves on to the next one.
// On the host, this is also what starts the level again once everyone is out.
extern void Coop_WhileOut(bool asksToChangeWhoHeWatches);

// When a level loads, on every machine: this object, when its one-machine event happens, sets off this target. See level_events.h. Use the name in the level, before anything renames it, and call this even if the object itself won't be made on this machine.
extern void LevelEvents_Note(const char *sourceName, const char *targetName, int request);

// The same for an item, which a client may see picked up first. Where it is goes with it, for how near someone must be to ask.
extern void LevelEvents_NoteItem(const char *sourceName, const char *targetName, int request, int x, int y, int z);

// That event has just happened on this machine, whose own target has fired or is about to. This tells the others. It does nothing outside the mode, and each object's only counts once.
extern void LevelEvents_Happened(const char *sourceName);

// A new level's list starts empty.
extern void LevelEvents_NewLevel(void);
extern void LevelEvents_LevelStartsAgain(void);

// A level's end was reached on this machine. Only the host's counts: it tells everyone to go on. A client's own chain reaching the end does nothing, because the same chain reaches it on the host.
extern void Coop_LevelEndReached(void);

// Whether the level that's just ended is going on to another, with everyone still connected. Then the game keeps its connection, and the menus are skipped.
extern bool Coop_LevelChangeIsDue(void);

// What a player is told during a co-op game: see coop_in_game.h
extern bool Coop_TheCampaignIsOver(void);
extern bool Coop_LevelCompleteLine(char *line, size_t room);
extern bool Coop_WaitingForLine(char *line, size_t room);

// In a campaign everyone plays that campaign's species. Does nothing in a game with no campaign.
extern void Coop_TakeCampaignSpecies(void);

// Gets this machine ready to load the next level, and says it's underway.
extern void Coop_LevelChangeHasStarted(void);
// A message was refused, and why. It goes to the log, but only so often, because anyone can cause this as fast as they like.
extern void Net_NoteRefused(const char *why);

// For a test run only: puts these bytes on the way out as one part of a message, with no checks at all.
extern void HudsonTest_AddRawPart(int kind, const void *part, int size);

// For a test run only: the game description, with one player left off it.
extern void HudsonTest_AddDescriptionWithout(NetID left);
// Whether the level a host named is on the list for the game type it named. It says why in the log if not.
extern bool Net_HostsLevelIsOnItsList();

// Whether a client may send this message now, by the limits on the costly kinds of part. It says why in the log if not. connection is its place in the host's list.
extern bool Net_ClientCanAfford(const uint8_t *message, size_t size, size_t connection);

// In a network game, whether the blocks are down to what's kept back for this machine's own player. Anything a message asks for, or that's only for show, isn't made then.
extern bool Net_BlocksAreShort();
extern void AddNetMsg_FragmentalObjectsStatus(void);
extern void AddNetMsg_StrategySynch(void);
extern void AddNetMsg_AlienAIState(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_AlienAISeqChange(STRATEGYBLOCK *sbPtr,int sequence_type,int sub_sequence,int sequence_length,int tweening_time);
extern void AddNetMsg_AlienAIKilled(STRATEGYBLOCK *sbPtr,int death_code,int death_time, int GibbFactor,DAMAGE_PROFILE* damage);
extern void AddNetMsg_FarAlienPosition(STRATEGYBLOCK* sbPtr,int targetModuleIndex,int index,BOOL indexIsModuleIndex);
extern void AddNetMsg_GhostHierarchyDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple, int sectionID,VECTORCH* incoming);
extern void AddNetMsg_SpotAlienSound(int soundCategory,int alienType,int pitch,VECTORCH *position);
extern void AddNetMsg_LocalObjectDestroyed_Request(STRATEGYBLOCK *sbPtr);
extern void AddNetMsg_ScoreChange(int killerIndex,int victimIndex);

extern void AddNetMsg_PlayerID(NetID playerID,unsigned char message);
extern void AddNetMsg_LastManStanding_RestartTimer(char time);
extern void AddNetMsg_LastManStanding_Restart(NetID alienID,int seed);

extern void AddNetMsg_CreateWeapon(char* objectName,int type,VECTORCH* location);

extern void AddNetMsg_RespawnPickups();

extern void AddNetMsg_Gibbing(STRATEGYBLOCK* sbPtr,int gibbFactor,int seed);
extern void AddNetMsg_SpotOtherSound(enum soundindex SoundIndex,VECTORCH *position,int explosion);

extern void TransmitEndOfGameNetMsg(void);
extern void TransmitPlayerLeavingNetMsg(void);
extern void TransmitStartGameNetMsg(void);

extern void RestartNetworkGame(int seed);

extern void DeallocatePlayersMirrorImage();

void InitNetLog(void);
void LogNetInfo(char *msg);

extern BOOL AreThereAnyLivesLeft();

extern void DoMultiplayerSpecificHud();

extern void GetNextMultiplayerObservedPlayer();
extern void TurnOffMultiplayerObserveMode();
extern void CheckStateOfObservedPlayer();

static int MyPlayerHasAMuzzleFlash(STRATEGYBLOCK *sbPtr);
int LinkSwitchGetSynchData(STRATEGYBLOCK* sbPtr);
int TrackObjectGetSynchData(STRATEGYBLOCK* sbPtr);
void LinkSwitchSetSynchData(STRATEGYBLOCK* sbPtr,int status);
void TrackObjectSetSynchData(STRATEGYBLOCK* sbPtr,int status);
void SetSeededFastRandom(int seed);
int SeededFastRandom(void);
void AllNewModuleHandler(void);
void ReflectObject(DISPLAYBLOCK *dPtr);
extern void D3D_FadeDownScreen(int brightness, int colour);
extern void RenderStringVertically(char *stringPtr, int centreX, int bottomY, int colour);
extern void RenderStringCentred(char *stringPtr, int centreX, int y, int colour);
extern void RenderString(char *stringPtr, int x, int y, int colour);

/* ---------------------------------------------------------------------
   Some global references
  ----------------------------------------------------------------------*/
extern NETGAME_GAMEDATA netGameData;
extern int peerStartUpIntegrities[];

extern int numMarineStartPos;
extern int numAlienStartPos;
extern int numPredatorStartPos;

extern MULTIPLAYER_START* marineStartPositions;
extern MULTIPLAYER_START* alienStartPositions;
extern MULTIPLAYER_START* predatorStartPositions;

#define LobbiedGame_NotLobbied 0
#define LobbiedGame_Server 1
#define LobbiedGame_Client 2
extern int LobbiedGame;

#endif
