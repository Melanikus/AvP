// AvP port by Barry Duncan.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   in a Hudson run with dieat=, a host left alone and out starts the level again by itself, as single player does (#126)
// Modified 2026-11-06 by Melanikus for the AvP port:
//   two test-only helpers a Hudson hostile network run uses. Nothing calls them until the network step: hudson.cpp refuses net= until then
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a player's class number from the network has to be one there is, or it's taken as the general one, as the character type beside
//   it already was
// Modified 2026-11-03 by Melanikus for the AvP port:
//   chat lines, multiplayer console messages and the lives, score and time texts are written with
//   size limits, so no name, chat line or language-file text can run past the end of its buffer;
//   a pointer the compiler could not prove was set now starts at a safe value; the game runs the same as before
//   the disc is checked before use, because a save file can leave that part out
// Modified 2026-11-07 by Melanikus for the AvP port:
//   in a co-op campaign the score key lists who's alive, a player who's out is told so, and the loading screen says which level was finished and who it's waiting for
//   the co-op lobby: what it takes, the host's choice, a joiner's answer, and a custom map as a campaign of one level. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten)
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a level's end notes what each player carries on with in a co-op campaign. Credit: Melanikus and Bobblen (the kept kit follows Bobblen's avp-forever, rewritten)
// Modified 2026-11-06 by Melanikus for the AvP port:
//   the new guns' settings are in the game description, so a joiner uses the host's
//   the room needed round a level's start is for however tall the player is
//   what has to go out before a level stops goes at once, not at the next turn of the send clock; someone left behind in a level catches up; the places round a level's start are measured from the floor under it
//   in a campaign only its species is on offer, and looking for an allowed species reaches every place on the slider
//   a level with no multiplayer starts puts each player beside the level's own start, or beside a teammate who has gone on ahead
//   in a campaign a dead player is out until the next level, and when everyone is out the level starts again
//   a campaign has a difficulty chosen by the host, sent in the game description and checked
//   and one number from the host for where things with several possible places go
//   a campaign has no time or score limit, and no score on the screen
//   the build stops if a message or a level list changes while the multiplayer version doesn't
//   a press of a switch with a link group now reaches the other machines
//   in the same-as-single-player mode, a level starts for everyone together
//   in the same-as-single-player mode, the messages about damage to breakable things can name a track object
//   the same-as-single-player mode: its one question, a campaign in the game description, and the message that moves everyone on
//   others see a muzzle flash from the revolver, and a kill by M5's guns or the staff has its picture in the message
//   three more marine classes from MelPyth M5, on the class slider, with their limits and allow settings, and Allow Staff
//   a player on another machine can hit every kind of NPC the host runs, and which part of it he hit counts
//   in a game that plays the same as single player, the host tells everyone about the NPCs it runs: see npc_ghosts.cpp
//   system messages are all handled in networking.cpp now, so that the rest of this file only ever sees game messages from someone who has been let in; a client learns the other players' names from the game description
//   the buffer that receives a message has room for its header as well as what the game can send
//   in a game that plays as single player, a host says what its doors, switches and breakable things are doing 10 times a second, not once
//   a level starting again keeps its list of what can happen, and forgets only what has; a new game starts a new list; a host says again what has happened, so that a joiner who came late, or a message with no room, does not lose it; a host's loading screen says the game's description once a second
//   a message is checked from end to end before any of it is read, and what only a host should say is only believed from the host: see Net_GameMessageIsGood
//   numbers in messages that are places in the player list, the alien kinds, the decals, the ammunition, the weapons or the score tables are checked before they're used
//   a damage message naming ammunition that isn't in the table is ignored, where only an assert stood before, and that's compiled out of the game players run
//   a client may only send so many of the kinds of part that cost every machine something, what one player has alive is counted as well, and what's kept back of the blocks is for this machine's own player
//   outside the campaign mode, which keeps a level's own autoguns, a message about an autogun is refused by name
//   a machine sends 60 times a second, not every frame, and what has to go out before it stops playing goes at once
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a platform lift's state from the host is checked before it's used, and the first one refused is written in the log
//   whether a player's shoulder lamp is on goes to the other machines in a bit the message already had free, and an NPC message that sets a flag
//   this version doesn't know is refused
//   a game description that comes after the word to go on to the next level can't put a campaign's place back, so a joiner
//   loads the level the host loads, and the log says when one tried
//   a decal from another machine has to be on one of this level's modules, or on none; an alien's far place has to
//   be in the list of far places as well as in the list of AI modules
//   the log says when the host's switches can't be matched with this machine's, once a level and with how many, and when a frame's
//   messages don't all fit, so a tester's log shows both
// Modified 2026-11-08 by Melanikus for the AvP port:
//   each frame of a network game tells the 10-second report how long it took (net_watch.h)
/* Patrick 19/3/97 -----------------------------------------------------
  Source for Multi-Player game support
  ----------------------------------------------------------------------*/

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "dynamics.h"
#include "bh_debris.h"
#include "pvisible.h"
#include "bh_platform_lift.h"
#include "pldnet.h"
#include "net_checks.h"
#include "env_info.h"
#include "level_events.h"
#include "extents.h"
#include "logString.h"
#include "marine_classes.h"
#include "player_spots.h"
#include "los.h"
#include "pldghost.h"
#include "equipment.h"
#include "weapons.h"
#include "bh_generator.h"
#include "psnd.h"
#include "kshape.h"
#include "pfarlocs.h"
#include "io.h"
#include "pheromone.h"
/* these required sequence enumerations...*/
#include "bh_predator.h"
#include "bh_alien.h"
#include "bh_marine.h"
#include "bh_binary_switch.h"
#include "bh_agun.h"
#include "bh_weapon.h"
#include "bh_corpse.h"
#include "bh_light.h"
#include "scream.h"
#include "targeting.h"
#include "iofocus.h"
#include "Input.h"

#define UseLocalAssert TRUE
#include "ourasert.h"
#include "showcmds.h"

#define CalculateBytesSentPerSecond 0

int IsThisObjectVisibleFromThisPosition_WithIgnore(DISPLAYBLOCK *objectPtr, DISPLAYBLOCK *ignoredObjectPtr, VECTORCH *positionPtr, int maxRange);

#include "networking.h"
#include "level_lists.h"
#include "inventory.h"
#include "custom_maps.h"
#include "lift_checks.h"
#include "logString.h"
#include "MemoryStream.h"
#include <assert.h>
#include "bh_xeno.h"
#include "bh_queen.h"
#include "bh_facehugger.h"
#include "hmodel.h"
#include "npc_kinds.h"
#include "shoulder_lamp.h"
#include "npc_ghosts.h"
#include "coop_in_game.h"
#include "coop_menu.h"
#include "hudson.h"

// coop_in_game.h keeps one bit for each place on the player list
static_assert(kCoopMostPlayers == NET_MAXPLAYERS, "coop_in_game.h needs one bit for each player");

/*----------------------------------------------------------------------
  Some globals for use in this file
  ----------------------------------------------------------------------*/
NETGAME_GAMEDATA netGameData =
{
	NGS_StartUp,    // NETGAME_STATES myGameState;
	NGCT_Marine,    // NETGAME_CHARACTERTYPE myCharacterType;
	NGCT_Marine,    // NETGAME_CHARACTERTYPE myNextCharacterType; - if player is currently dead and about to become a new character
	NGSCT_General,  // NETGAME_SPECIALISTCHARACTERTYPE myCharacterSubType;
	0,      // unsigned char myStartFlag;
	{0,},   // NETGAME_PLAYERDATA playerData[NET_MAXPLAYERS];
	{0,},   // int teamScores[3];
	NGT_Individual, // NETGAME_TYPE gameType;
	0,  // unsigned char levelNumber;
	1000000,    // unsigned int scoreLimit;
	255,    // unsigned char timeLimit;
	5,  // int invulnerableTime;//in seconds after respawn
	0,  // int GameTimeElapsed;

	// scoring system stuff
	{100, 150, 75}, // int characterKillValues[3];
	100,    // int baseKillValue;
	TRUE,   // BOOL useDynamicScoring;
	TRUE,   // BOOL useCharacterKillValues;

	{75, 100, 150}, // int aiKillValues[3];

	// for last man standing game
	-1, // int LMS_AlienIndex;
	0,  // int LMS_RestartTimer;

	0,  // int stateCheckTimeDelay;

	/*following timer used to prevent the game description from being sent every frame*/
	0,  // int gameDescriptionTimeDelay;

	/*sendFrequencey - how often to send messages in fixed point seconds*/
	0,  // int sendFrequency;
	0,  // int sendTimer;

	// player type limits
	8,  // unsigned int maxPredator;
	8,  // unsigned int maxAlien;
	8,  // unsigned int maxMarine;

	8,  // unsigned int maxMarineGeneral;
	8,  // unsigned int maxMarinePulseRifle;
	8,  // unsigned int maxMarineSmartgun;
	8,  // unsigned int maxMarineFlamer;
	8,  // unsigned int maxMarineSadar;
	8,  // unsigned int maxMarineGrenade;
	8,  // unsigned int maxMarineMinigun;
	8,  // unsigned int maxMarineSmartDisc;
	8,  // unsigned int maxMarinePistols;
	8,  // unsigned int maxMarineShotgun;
	8,  // unsigned int maxMarineRevolver;
	8,  // unsigned int maxMarineM79;

	// weapons allowed
	TRUE,   // BOOL allowSmartgun;
	TRUE,   // BOOL allowFlamer;
	TRUE,   // BOOL allowSadar;
	TRUE,   // BOOL allowGrenadeLauncher;
	TRUE,   // BOOL allowMinigun;
	TRUE,   // BOOL allowDisc;
	TRUE,   // BOOL allowPistol;
	TRUE,   // BOOL allowPlasmaCaster;
	TRUE,   // BOOL allowSpeargun;
	TRUE,   // BOOL allowMedicomp;
	TRUE,   // BOOL allowSmartDisc;
	TRUE,   // BOOL allowPistols;
	TRUE,   // BOOL allowShotgun;
	TRUE,   // BOOL allowRevolver;
	TRUE,   // BOOL allowM79;
	FALSE,  // BOOL allowStaff;

	0,         // int maxLives;
	FALSE,     // BOOL useSharedLives;
	{0, 0, 0}, // int numDeaths[3];

	0,      // int pointsForResapwn;
	40,     // int timeForRespawn; //seconds

	0,      // int lastPointsBasedRespawn;

	TRUE,   // BOOL sendDecals;
	0,      // unsigned int needGameDescription :1;
	FALSE,  // BOOL skirmishMode

	-1, // char myLastScream;

	NETGAMESPEED_100PERCENT, // int gameSpeed;
	0,  // BOOL preDestroyLights;
	0,  // BOOL disableFriendlyFire;
	1,  // BOOL fallingDamage;
	1,  // BOOL pistolInfiniteAmmo;
	1,  // BOOL specialistPistols;
	0,  // int myStrategyCheckSum;
	0,  // landingNoise:1;
	0,  // int joiningGameStatus;
	"", // char customLevelName[];
};

// How many times a second a machine sends what it has. Without a limit it was every frame, so traffic rose with frame rate, about 200 a second on a fast machine, and one lost packet holds up everything behind it. What's said between sends waits for the next one.
static const int kSendsEachSecond = 60;

void SetDefaultMultiplayerConfig()
{
	netGameData.scoreLimit = 1000000;
	netGameData.timeLimit = 255;
	netGameData.invulnerableTime = 5;
	netGameData.characterKillValues[0] = 100;
	netGameData.characterKillValues[1] = 150;
	netGameData.characterKillValues[2] = 75;
	netGameData.baseKillValue = 100;
	netGameData.useDynamicScoring = 1;
	netGameData.useCharacterKillValues = 1;
	netGameData.aiKillValues[0] = 75;
	netGameData.aiKillValues[1] = 100;
	netGameData.aiKillValues[2] = 150;
	netGameData.maxMarine = 8;
	netGameData.maxAlien = 8;
	netGameData.maxPredator = 8;
	netGameData.maxMarineGeneral = 8;
	netGameData.maxMarinePulseRifle = 8;
	netGameData.maxMarineSmartgun = 8;
	netGameData.maxMarineFlamer = 8;
	netGameData.maxMarineSadar = 8;
	netGameData.maxMarineGrenade = 8;
	netGameData.maxMarineMinigun = 8;
	netGameData.maxMarineSmartDisc = 8;
	netGameData.maxMarinePistols = 8;
	netGameData.maxMarineShotgun = 8;
	netGameData.maxMarineRevolver = 8;
	netGameData.maxMarineM79 = 8;
	netGameData.allowSmartgun = 1;
	netGameData.allowFlamer = 1;
	netGameData.allowSadar = 1;
	netGameData.allowGrenadeLauncher = 1;
	netGameData.allowMinigun = 1;
	netGameData.allowDisc = 1;
	netGameData.allowPistol = 1;
	netGameData.allowPlasmaCaster = 1;
	netGameData.allowSpeargun = 1;
	netGameData.allowMedicomp = 1;
	netGameData.allowSmartDisc = 1;
	netGameData.allowPistols = 1;
	netGameData.allowShotgun = 1;
	netGameData.allowRevolver = 1;
	netGameData.allowM79 = 1;
	netGameData.allowStaff = 0;
	netGameData.maxLives = 0;
	netGameData.useSharedLives = 0;
	netGameData.pointsForRespawn = 0;
	netGameData.coopDifficulty = I_Medium;
	netGameData.timeForRespawn = 40;
	netGameData.gameSpeed = NETGAMESPEED_100PERCENT;
	netGameData.preDestroyLights = FALSE;
	netGameData.disableFriendlyFire = FALSE;
	netGameData.fallingDamage = TRUE;
	netGameData.pistolInfiniteAmmo = TRUE;
	netGameData.specialistPistols = TRUE;
}

int LobbiedGame = 0;

static uint8_t sendBuffer[NET_MESSAGEBUFFERSIZE];
static uint8_t *endSendBuffer = NULL;

static int netNextLocalObjectId = 1;
NetID myNetworkKillerId = 0;
NetID myIgniterId = 0;
int MyHitBodyPartId = -1;
NetID MultiplayerObservedPlayer = 0;

/* for testing */
static int numMessagesReceived = 0;
static int numMessagesTransmitted = 0;

static char OnScreenMessageBuffer[300];

static unsigned char FragmentalObjectStatus[NUMBER_OF_FRAGMENTAL_OBJECTS];
static unsigned char StrategySynchArray[NUMBER_OF_STRATEGIES_TO_SYNCH >> 2];

int numMarineStartPos = 0;
int numAlienStartPos = 0;
int numPredatorStartPos = 0;

MULTIPLAYER_START *marineStartPositions = 0;
MULTIPLAYER_START *alienStartPositions = 0;
MULTIPLAYER_START *predatorStartPositions = 0;


int ShowMultiplayerScoreTimer = 0;
int MultiplayerRestartSeed = 0;

static int GameTimeSinceLastSend = 0;

static unsigned int TimeCounterForExtrapolation = 0;

extern DAMAGE_PROFILE FlechetteDamage;
extern int InanimateDamageFromNetHost;
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
extern char LevelName[];
extern int StaffAttack;
extern STRATEGYBLOCK *Biting;
extern int Bit;
extern int StaffAttack;

/*----------------------------------------------------------------------
  External globals (& protoypes)
  ----------------------------------------------------------------------*/
extern int TimeScale;
extern int PrevNormalFrameTime;
extern int GlobalFrameCounter;
extern THREE_LASER_DOT_DESC PredatorLaserSights[];

extern void UpdateAlienAIGhostAnimSequence(STRATEGYBLOCK *sbPtr, HMODEL_SEQUENCE_TYPES type, int subtype, int length, int tweeningtime);
extern void RemovePickedUpObject(STRATEGYBLOCK *objectPtr);
extern void PrintStringTableEntryInConsole(enum TEXTSTRING_ID string_id);
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern int LastHand;  // For alien claws and two pistols

extern void CreateSpearPossiblyWithFragment(DISPLAYBLOCK *dispPtr, VECTORCH *spearPositionPtr, VECTORCH *spearDirectionPtr);
extern void NewOnScreenMessage(char *messagePtr);

extern void ChangeToPredator();
extern void ChangeToAlien();
extern void ChangeToMarine();
extern void ChangeToAlien();
extern void NetworkPeerChangePlatformLiftState(STRATEGYBLOCK *sbPtr, PLATFORMLIFT_STATES new_state);
extern void MakeFlechetteExplosionAt(VECTORCH *positionPtr, int seed);
extern void KillFragmentalObjectForRespawn(STRATEGYBLOCK *sbPtr);
extern void PostDynamicsExtrapolationUpdate();
extern void PlayerGhostExtrapolation();

/*----------------------------------------------------------------------
  Some protoypes for this file
  ----------------------------------------------------------------------*/
static void ProcessGameMessage(NetID senderId, uint8_t *msgP, size_t msgSize);
static void AddPlayerAndObjectUpdateMessages(void);
static void UpdateNetworkGameScores(NetID playerKilledId, NetID killerId, NETGAME_CHARACTERTYPE playerKilledType, NETGAME_CHARACTERTYPE killerType);
static void InitFinalNetGameScores(void);
static void ConvertNetNameToUpperCase(char *strPtr);

static void ProcessNetMsg_GameDescription(NETMESSAGE_GAMEDESCRIPTION *msgPtr);
static void ProcessNetMsg_PlayerDescription(NETMESSAGE_PLAYERDESCRIPTION *msgPtr, NetID senderId);
static void ProcessNetMsg_StartGame(void);
static void ProcessNetMsg_PlayerState(NETMESSAGE_PLAYERSTATE *msgPtr, NetID senderId);
static void ProcessNetMsg_PlayerState_Minimal(NETMESSAGE_PLAYERSTATE_MINIMAL *msgPtr, NetID senderId, BOOL orientation);
static void ProcessNetMsg_FrameTimer(unsigned short frame_time, NetID senderId);
static void ProcessNetMsg_PlayerKilled(NETMESSAGE_PLAYERKILLED *msgPtr, NetID senderId);
static void ProcessNetMsg_PlayerDeathAnim(NETMESSAGE_CORPSEDEATHANIM *messagePtr, NetID senderId);
static void ProcessNetMsg_PlayerLeaving(NetID senderId);
static void ProcessNetMsg_AllGameScores(NETMESSAGE_ALLGAMESCORES *msgPtr);
static void ProcessNetMsg_PlayerScores(NETMESSAGE_PLAYERSCORES *msgPtr);
static void ProcessNetMsg_SpeciesScores(NETMESSAGE_SPECIESSCORES *msgPtr);
static void ProcessNetMsg_LocalRicochet(NETMESSAGE_LOCALRICOCHET *msgPtr);
static void ProcessNetMsg_LocalObjectState(NETMESSAGE_LOBSTATE *msgPtr, NetID senderId);
static void ProcessNetMsg_LocalObjectDamaged(char *msgPtr, NetID senderId);
static void ProcessNetMsg_LocalObjectDestroyed(NETMESSAGE_LOBDESTROYED *msgPtr, NetID senderId);
static void ProcessNetMsg_ObjectPickedUp(NETMESSAGE_OBJECTPICKEDUP *messagePtr);
static void ProcessNetMsg_EndGame(void);
static void ProcessNetMsg_InanimateObjectDamaged(char *msgPtr);
static void ProcessNetMsg_InanimateObjectDestroyed(NETMESSAGE_INANIMATEDESTROYED *msgPtr);
static void ProcessNetMsg_LOSRequestBinarySwitch(NETMESSAGE_LOSREQUESTBINARYSWITCH *msgPtr);
static void ProcessNetMsg_PlatformLiftState(NETMESSAGE_PLATFORMLIFTSTATE *msgPtr);
static void ProcessNetMsg_RequestPlatformLiftActivate(NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE *msgPtr);
static void ProcessNetMsg_PlayerAutoGunState(NETMESSAGE_AGUNSTATE *msgPtr, NetID senderId);
static unsigned char *ProcessNetMsg_ChatBroadcast(unsigned char *subMessagePtr, NetID senderId);
static void ProcessNetMsg_MakeExplosion(NETMESSAGE_MAKEEXPLOSION *messagePtr);
static void ProcessNetMsg_MakeFlechetteExplosion(NETMESSAGE_MAKEFLECHETTEEXPLOSION *messagePtr);
static void ProcessNetMsg_MakePlasmaExplosion(NETMESSAGE_MAKEPLASMAEXPLOSION *messagePtr);
static void ProcessNetMsg_PredatorSights(NETMESSAGE_PREDATORSIGHTS *messagePtr, NetID senderId);
static void ProcessNetMsg_FragmentalObjectsStatus(NETMESSAGE_FRAGMENTALOBJECTSSTATUS *messagePtr);
static void ProcessNetMsg_StrategySynch(NETMESSAGE_STRATEGYSYNCH *messagePtr);
static void ProcessNetMsg_LocalObjectOnFire(NETMESSAGE_LOBONFIRE *messagePtr, NetID senderId);
static void ProcessNetMsg_AlienAIState(NETMESSAGE_ALIENAISTATE *messagePtr, NetID senderId);
static void ProcessNetMsg_FarAlienPosition(NETMESSAGE_FARALIENPOSITION *messagePtr, NetID senderId);
static void ProcessNetMsg_SpotAlienSound(NETMESSAGE_SPOTALIENSOUND *messagePtr, NetID senderId);
static void ProcessNetMsg_LocalObjectDestroyed_Request(NETMESSAGE_LOBDESTROYED_REQUEST *messagePtr, NetID senderId);
static void ProcessNetMsg_ScoreChange(NETMESSAGE_SCORECHANGE *messagePtr);
static void ProcessNetMsg_CreateWeapon(NETMESSAGE_CREATEWEAPON *messagePtr, NetID senderId);
static void ProcessNetMsg_Gibbing(NETMESSAGE_GIBBING *messagePtr, NetID senderId);
static void ProcessNetMsg_GhostHierarchyDamaged(char *messagePtr, NetID senderId);
static void ProcessNetMsg_MakeDecal(NETMESSAGE_MAKEDECAL *messagePtr);
static void ProcessNetMsg_AlienAISequenceChange(NETMESSAGE_ALIENSEQUENCECHANGE *messagePtr, NetID senderId);
static void ProcessNetMsg_AlienAIKilled(NETMESSAGE_ALIENAIKILLED *messagePtr, NetID senderId);
static void ProcessNetMsg_SpotOtherSound(NETMESSAGE_SPOTOTHERSOUND *messagePtr, NetID senderId);
static void ProcessNetMsg_CoopNextLevel(NETMESSAGE_COOPNEXTLEVEL *messagePtr);
static void Coop_GoToPlace(int place);
static void ProcessNetMsg_CoopReady(NetID senderId);
static void LevelEvents_SayAgainWhatHappened(void);
static const DWORD kMillisecondsBetweenObjectSynchs = 100;
static void ProcessNetMsg_CoopGo(void);
static void ProcessNetMsg_CoopCampaignChoice(NETMESSAGE_COOPCAMPAIGNCHOICE *messagePtr);
static void ProcessNetMsg_CoopMapAnswer(NETMESSAGE_COOPMAPANSWER *messagePtr, NetID senderId);
static void ProcessNetMsg_LevelEvent(NETMESSAGE_LEVELEVENT *messagePtr);
static void ProcessNetMsg_LevelEventAsk(NETMESSAGE_LEVELEVENT *messagePtr, NetID senderId);
void StartOfGame_PlayerPlacement(STRATEGYBLOCK *playerSbPtr, int seed);

static void CheckLastManStandingState();
static void Handle_LastManStanding_Restart(NetID alienID, int seed);
static void Handle_LastManStanding_RestartInfo(NetID alienID);
static void Handle_LastManStanding_LastMan(NetID marineID);
static void Handle_LastManStanding_RestartTimer(unsigned char time);

static void CheckSpeciesTagState();
static void Handle_SpeciesTag_NewPersonIt(NetID predatorID);
static int CountPlayersOfType(NETGAME_CHARACTERTYPE species);

// Who plays the custom map a co-op lobby chose
static NETGAME_CHARACTERTYPE Coop_CustomMapCharacter(void)
{
	return netGameData.coopCustomSpecies == 1 ? NGCT_Predator : netGameData.coopCustomSpecies == 2 ? NGCT_Alien : NGCT_Marine;
}

static int GetSizeOfLocalObjectDamagedMessage(char *messagePtr);
static int GetSizeOfGhostHierarchyDamagedMessage(char *messagePtr);
static int GetSizeOfInanimateDamagedMessage(char *messagePtr);

static STRATEGYBLOCK *FindObjectFromNetIndex(int obIndex);
static void ProcessNetMsg_NpcState(NETMESSAGE_NPCSTATE *messagePtr, NetID senderId);
static void ProcessNetMsg_NpcSound(NETMESSAGE_NPCSOUND *messagePtr, NetID senderId);
static void ProcessNetMsg_NpcThrownObject(NETMESSAGE_NPCTHROWNOBJECT *messagePtr);
static void ProcessNetMsg_LiftState(NETMESSAGE_LIFTSTATE *messagePtr);
static void ProcessNetMsg_NpcXenoDeltas(NETMESSAGE_NPCXENODELTAS *messagePtr, NetID senderId);
static void ProcessNetMsg_NpcKilled(NETMESSAGE_NPCKILLED *messagePtr, NetID senderId);
static void AddNpcStateMessages(void);
HMODELCONTROLLER *Npc_HModelController(STRATEGYBLOCK *sbPtr);
static STRATEGYBLOCK *FindEnvironmentObjectFromName(char *name);
static STRATEGYBLOCK *FindBreakableFromName(char *name);
static STRATEGYBLOCK *FindSwitchFromName(char *name);
static void InitialiseSendMessageBuffer(void);

static MARINE_SEQUENCE GetMyMarineSequence(void);
static ALIEN_SEQUENCE GetMyAlienSequence(void);
static PREDATOR_SEQUENCE GetMyPredatorSequence(void);


static void Inform_PlayerHasDied(NetID killer, NetID victim, NETGAME_CHARACTERTYPE killerType, char weaponIcon);
static void Inform_AiHasDied(NetID killer, ALIEN_TYPE type, char weaponIcon);
static void Inform_PlayerHasLeft(NetID player);
static void Inform_PlayerHasJoined(NetID player);
static void Inform_PlayerHasConnected(NetID player);

static void WriteFragmentStatus(int fragmentNumber, int status);
static int ReadFragmentStatus(int fragmentNumber);

static void WriteStrategySynch(int objectNumber, int status);
static int ReadStrategySynch(int objectNumber);

static int GetStrategySynchObjectChecksum();

static int GetDynamicScoreMultiplier(int playerKilledIndex, int killerIndex);
static int GetNetScoreForKill(int playerKilledIndex, int killerIndex);

static void NetworkGameConsoleMessage(enum TEXTSTRING_ID stringID, const char *name1, const char *name2);
static void NetworkGameConsoleMessageWithWeaponIcon(enum TEXTSTRING_ID stringID, const char *name1, const char *name2, const char *weaponSymbol);

static void CheckForPointBasedObjectRespawn();

static int CalculateMyScore();

static void PeriodicScoreUpdate();

void CheckStateOfObservedPlayer();

/*----------------------------------------------------------------------
  Initalisation of net game
  ----------------------------------------------------------------------*/
// How many of the host's switch messages couldn't be matched with this machine's switches in this level (see NetLevelCount)
static NetLevelCount switchSyncsNotMatched;

// The level is over, or another is starting: the log hears how many of the host's switch messages couldn't be used in it, if any were
static void SwitchSyncs_LevelIsOver(void)
{
	unsigned long long const notMatched = NetLevelCount_LevelIsOver(switchSyncsNotMatched);

	if (notMatched > 0)
	{
		LogString("net: in that level, " + std::to_string(notMatched) + " of the host's messages about switches, doors and lifts couldn't be matched with this machine's, so they were left out");
	}
}

static void Coop_ForgetTheLobby(void);

void InitAVPNetGameForHost(int species, int gamestyle, int level)
{
	AvP.GameMode = I_GM_Playing;
	AvP.Network = I_Host;

	// A new game, so nothing that happened in the last one counts, even on the same level
	LevelEvents_NewLevel();
	Campaign_ForgetKit();
	Coop_ForgetTheLobby();
	AvP.NetworkAIServer = (gamestyle == NGT_Coop);
	/* init the send message buffer */
	InitialiseSendMessageBuffer();
	netGameData.myGameState = NGS_Joining;
	/* base initialisation of game description */
	{
		for (int i = 0; i < (NET_MAXPLAYERS); i++)
		{
			netGameData.playerData[i].playerId = 0;

			for (int j = 0; j < (NET_PLAYERNAMELENGTH); j++)
			{
				netGameData.playerData[i].name[j] = '\0';
			}

			netGameData.playerData[i].characterType = NGCT_Marine;
			netGameData.playerData[i].characterSubType = NGSCT_General;

			for (int j = 0; j < (NET_MAXPLAYERS); j++)
			{
				netGameData.playerData[i].playerFrags[j] = 0;
			}

			netGameData.playerData[i].playerScore = 0;
			netGameData.playerData[i].playerScoreAgainst = 0;
			netGameData.playerData[i].aliensKilled[0] = 0;
			netGameData.playerData[i].aliensKilled[1] = 0;
			netGameData.playerData[i].aliensKilled[2] = 0;
			netGameData.playerData[i].deathsFromAI = 0;
			netGameData.playerData[i].playerAlive  = 1;
			netGameData.playerData[i].playerHasLives = 1;
			netGameData.playerData[i].startFlag = 0;
		}

		for (int j = 0; j < 3; j++) {
			netGameData.teamScores[j] = 0;
		}

		//      netGameData.myGameState = NGS_Playing;
		netGameData.myCharacterSubType = NGSCT_General;

		switch (species)
		{
			default:
			case 0:
				netGameData.myCharacterType = NGCT_Marine;
				netGameData.myNextCharacterType = NGCT_Marine;
				AvP.PlayerType = I_Marine;
				break;

			case 1:
				netGameData.myCharacterType = NGCT_Predator;
				netGameData.myNextCharacterType = NGCT_Predator;
				AvP.PlayerType = I_Predator;
				break;

			case 2:
				netGameData.myCharacterType = NGCT_Alien;
				netGameData.myNextCharacterType = NGCT_Alien;
				AvP.PlayerType = I_Alien;
				break;

			case 3: //various marine subtypes
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
				netGameData.myCharacterType = NGCT_Marine;
				netGameData.myNextCharacterType = NGCT_Marine;
				netGameData.myCharacterSubType = (NETGAME_SPECIALISTCHARACTERTYPE)(species - 2);
				AvP.PlayerType = I_Marine;
				break;
		}

		netGameData.myStartFlag = 0;
		netGameData.gameType = static_cast<enum netgame_type>(gamestyle);
		netGameData.levelNumber = level;
		//      netGameData.scoreLimit = 0;
		//      netGameData.timeLimit = 0;
		//      netGameData.invulnerableTime = 5;
		netGameData.GameTimeElapsed = 0;
		//      netGameData.characterKillValues[NGCT_Marine]=100;
		//      netGameData.characterKillValues[NGCT_Predator]=150;
		//      netGameData.characterKillValues[NGCT_Alien]=75;
		//      netGameData.baseKillValue=100;
		//      netGameData.useDynamicScoring=1;
		//      netGameData.useCharacterKillValues=1;
		netGameData.stateCheckTimeDelay = 0;
		netGameData.gameDescriptionTimeDelay = 0;
		netGameData.sendTimer = 0;

		if (LobbiedGame)
		{
			netGameData.sendFrequency = ONE_FIXED / 15;
			netGameData.sendDecals = FALSE;
		}
		else
		{
			netGameData.sendFrequency = ONE_FIXED / kSendsEachSecond;
			netGameData.sendDecals = TRUE;
		}
	}
	myNetworkKillerId = AvPNetID;    /* init global id of player who killed me last */
	netNextLocalObjectId = 1;        /* init local object network id */
	numMessagesReceived = 0;         /* these are for testing */
	numMessagesTransmitted = 0;

	/* If I'm the host, add myself to the game data */
	netGameData.playerData[0].playerId = AvPNetID;
	strncpy(netGameData.playerData[0].name, thisClientPlayer.name, NET_PLAYERNAMELENGTH - 1);
	netGameData.playerData[0].name[NET_PLAYERNAMELENGTH - 1] = '\0';

	//  ConvertNetNameToUpperCase(netGameData.playerData[0].name);
	{
		int myIndex = PlayerIdInPlayerList(AvPNetID);
		LOCALASSERT(myIndex != NET_IDNOTINPLAYERLIST);
		netGameData.playerData[myIndex].characterType = netGameData.myCharacterType;
		netGameData.playerData[myIndex].characterSubType = netGameData.myCharacterSubType;
	}
	netGameData.LMS_AlienIndex = -1;
	InitNetLog();

	// A campaign runs at full speed as single player does, whatever speed a saved game of another kind left here
	if (netGameData.coopCampaign != kCampaignNone)
	{
		netGameData.gameSpeed = NETGAMESPEED_100PERCENT;
	}

	//make sure our time scale is set correctly
	switch (netGameData.gameSpeed)
	{
		case NETGAMESPEED_70PERCENT :
			TimeScale = (ONE_FIXED * 70) / 100;
			break;

		case NETGAMESPEED_80PERCENT :
			TimeScale = (ONE_FIXED * 80) / 100;
			break;

		case NETGAMESPEED_90PERCENT :
			TimeScale = (ONE_FIXED * 90) / 100;
			break;

		case NETGAMESPEED_100PERCENT :
			TimeScale = (ONE_FIXED * 100) / 100;
			break;
	}

	SwitchSyncs_LevelIsOver();
	netGameData.myStrategyCheckSum = 0;
	netGameData.numDeaths[0] = 0;
	netGameData.numDeaths[1] = 0;
	netGameData.numDeaths[2] = 0;
}

void InitAVPNetGameForJoin(void)
{
	AvP.GameMode = I_GM_Playing;
	AvP.Network  = I_Peer;

	// A new game, so nothing that happened in the last one counts, even on the same level
	LevelEvents_NewLevel();
	Campaign_ForgetKit();
	netGameData.coopKeepKit = 0;
	Coop_ForgetTheLobby();
	netGameData.coopCampaign = kCampaignNone;
	netGameData.coopCampaignLevel = 0;
	AvP.NetworkAIServer = 0;

	// init the send message buffer
	InitialiseSendMessageBuffer();

	netGameData.myGameState = NGS_Joining;
	/* base initialisation of game description */
	{
		for (int i = 0; i < (NET_MAXPLAYERS); i++)
		{
			netGameData.playerData[i].playerId = 0;

			for (int j = 0; j < (NET_PLAYERNAMELENGTH); j++) {
				netGameData.playerData[i].name[j] = '\0';
			}

			netGameData.playerData[i].characterType = NGCT_Marine;
			netGameData.playerData[i].characterSubType = NGSCT_General;

			for (int j = 0; j < (NET_MAXPLAYERS); j++) {
				netGameData.playerData[i].playerFrags[j] = 0;
			}

			netGameData.playerData[i].playerScore = 0;
			netGameData.playerData[i].playerScoreAgainst = 0;
			netGameData.playerData[i].aliensKilled[0] = 0;
			netGameData.playerData[i].aliensKilled[1] = 0;
			netGameData.playerData[i].aliensKilled[2] = 0;
			netGameData.playerData[i].deathsFromAI    = 0;
			netGameData.playerData[i].playerAlive     = 1;
			netGameData.playerData[i].playerHasLives  = 1;
			netGameData.playerData[i].startFlag       = 0;
		}

		for (int j = 0; j < 3; j++) {
			netGameData.teamScores[j] = 0;
		}

		netGameData.myStartFlag = 0;
		netGameData.myGameState = NGS_Joining;
		netGameData.myCharacterType = NGCT_Marine;
		netGameData.myNextCharacterType = NGCT_Marine;
		netGameData.myCharacterSubType = NGSCT_General;
		netGameData.stateCheckTimeDelay = 0;
		netGameData.gameDescriptionTimeDelay = 0;

		if (LobbiedGame)
		{
			netGameData.sendFrequency = ONE_FIXED / 15;
			netGameData.sendDecals = FALSE;
		}
		else
		{
			netGameData.sendFrequency = ONE_FIXED / kSendsEachSecond;
			netGameData.sendDecals = TRUE;
		}

		netGameData.sendTimer = 0;
		netGameData.needGameDescription = 1;
	}
	myNetworkKillerId = AvPNetID;    /* init global id of player who killed me last */
	netNextLocalObjectId   = 1;      /* init local object network id */
	numMessagesReceived    = 0;      /* these are for testing */
	numMessagesTransmitted = 0;
	InitNetLog();
	SwitchSyncs_LevelIsOver();
	netGameData.myStrategyCheckSum = 0;
	netGameData.numDeaths[0] = 0;
	netGameData.numDeaths[1] = 0;
	netGameData.numDeaths[2] = 0;
	netGameData.joiningGameStatus = JOINNETGAME_WAITFORSTART;
}

static uint8_t msg[kMostMessageBytes];

/*----------------------------------------------------------------------
  Core message collection function
  ----------------------------------------------------------------------*/
void MinimalNetCollectMessages(void)
{
	NetID fromID = 0;
	NetID toID   = 0;
	size_t msgSize  = 0;

	// collects messages until something other than NET_OK is returned (eg NET_NO_MESSAGES)
	if (!netGameData.skirmishMode)
	{
		Net_ServiceNetwork();

		NetResult res = NET_OK;

		while ((NET_OK == res) /*&& AvPNetID*/)
		{
			res = Net_Receive(msg, msgSize);
			if (NET_OK == res)
			{
				if (msgSize >= sizeof(MessageHeader)) 
				{
					MessageHeader newHeader;
					MemoryReadStream rs(msg, msgSize);
					rs.GetBytes(&newHeader, sizeof(MessageHeader));

					fromID = newHeader.fromID;
					toID   = newHeader.toID;

					// Only game messages from someone who's been let in get this far. See WHO IS WHO in networking.cpp.
					ProcessGameMessage(fromID, msg, msgSize);
				}
			}
		}
	}
}

void NetCollectMessages(void)
{
	/* first off, some assertions about our game state */
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Leaving)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_GameFull)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_GameStarted)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_HostLost)));

	/* only bother colecting messages under certain game conditions... */
	if ((netGameData.myGameState != NGS_StartUp) && (netGameData.myGameState != NGS_Playing) && (netGameData.myGameState != NGS_Joining) && (netGameData.myGameState != NGS_EndGameScreen))
	{
		return;
	}

	InitNetLog();
	LogNetInfo("Collecting Messages... \n");

	/* collects messages until something other than NET_OK is returned (eg NET_NO_MESSAGES) */
	if (!netGameData.skirmishMode)
	{
		Net_ServiceNetwork();
			
		NetID fromID = 0;
		NetID toID   = 0;
		size_t msgSize = 0;
		NetResult res = NET_OK;

		while ((NET_OK == res) /*&& AvPNetID*/)
		{
			res = Net_Receive(msg, msgSize);
			if (NET_OK == res)
			{
				if (msgSize >= sizeof(MessageHeader)) 
				{
					numMessagesReceived++;

					MessageHeader newHeader;
					MemoryReadStream rs(msg, msgSize);
					rs.GetBytes(&newHeader, sizeof(MessageHeader));

					fromID = newHeader.fromID;
					toID   = newHeader.toID;

					// Only game messages from someone who's been let in get this far. See WHO IS WHO in networking.cpp.
					ProcessGameMessage(fromID, msg, msgSize);
				}
			}
		}
	}

	LogNetInfo("... Finished collecting Messages\n");
	//  OutputDebugString("...Finished collecting Messages\n");

	/* check ghost integrities */
	MaintainGhosts();

	/* time and score limit checks...*/
	{
		//update timer
		if (netGameData.myGameState == NGS_Playing) {
			netGameData.GameTimeElapsed += RealFrameTime;
		}

		if ((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Playing))
		{
			// A campaign ends when its levels do, as single player does, not on a time or a score
			if (netGameData.timeLimit > 0 && !SameAsSinglePlayer())
			{
				if (((netGameData.GameTimeElapsed >> 16) / 60) >= netGameData.timeLimit)
				{
					/* game has timed-out */
					TransmitEndOfGameNetMsg();
					netGameData.myGameState = NGS_EndGameScreen;
					// AvP.MainLoopRunning = 0;
				}
			}

			if (netGameData.scoreLimit > 0 && !SameAsSinglePlayer())
			{
				/* nb this doesn't check team scores */

				for (int i = 0; i < NET_MAXPLAYERS; i++)
				{
					if (netGameData.playerData[i].playerId)
					{
						int score = 0;

						for (int j = 0; j < 3; j++) {
							score += netGameData.playerData[i].aliensKilled[j] * netGameData.aiKillValues[j];
						}

						if (netGameData.playerData[i].playerScore >= netGameData.scoreLimit || score >= netGameData.scoreLimit)
						{
							/* someone has reached the score limit */
							TransmitEndOfGameNetMsg();
							netGameData.myGameState = NGS_EndGameScreen;
							break;
							//  AvP.MainLoopRunning = 0;
						}
					}
				}
			}

			// (In a campaign, being out and starting again are in Coop_WhileOut. It has no lives to count.)
			if (netGameData.maxLives > 0 && !SameAsSinglePlayer())
			{
				int numPlayers = 0;
				int numPlayersWithLifeLeft = 0;
				int numAliens = 0;
				int numAliensWithLifeLeft = 0;
				int numMarines = 0;
				int numMarinesWithLifeLeft = 0;
				int numPredators = 0;
				int numPredatorsWithLifeLeft = 0;
				BOOL gameOver = FALSE;

				for (int i = 0; i < NET_MAXPLAYERS; i++)
				{
					if (netGameData.playerData[i].playerId)
					{
						numPlayers++;
						numPlayersWithLifeLeft += netGameData.playerData[i].playerHasLives;

						switch (netGameData.playerData[i].characterType)
						{
							case NGCT_Alien :
								numAliens++;
								numAliensWithLifeLeft += netGameData.playerData[i].playerHasLives;
								break;

							case NGCT_Marine :
								numMarines++;
								numMarinesWithLifeLeft += netGameData.playerData[i].playerHasLives;
								break;

							case NGCT_Predator :
								numPredators++;
								numPredatorsWithLifeLeft += netGameData.playerData[i].playerHasLives;
								break;
						}
					}
				}

				//game is over if no one has life left
				if (numPlayersWithLifeLeft == 0)
				{
					gameOver = TRUE;
				}

				if (netGameData.gameType == NGT_Individual)
				{
					if (numPlayersWithLifeLeft <= 1 && numPlayers > 1)
					{
						//if in a deathmatch game and there is only one player with life , then thats it
						gameOver = TRUE;
					}
				}

				if (netGameData.gameType == NGT_CoopDeathmatch)
				{
					/*
					In a cooperative deathmatch ,the game ends when only one team has life left
					*/
					int numTeams = 0;
					int numTeamsWithLifeLeft = 0;

					if (numAliens) {
						numTeams++;
					}

					if (numAliensWithLifeLeft) {
						numTeamsWithLifeLeft++;
					}

					if (numPredators) {
						numTeams++;
					}

					if (numPredatorsWithLifeLeft) {
						numTeamsWithLifeLeft++;
					}

					if (numMarines) {
						numTeams++;
					}

					if (numMarinesWithLifeLeft) {
						numTeamsWithLifeLeft++;
					}

					if (numTeams > 1 && numTeamsWithLifeLeft <= 1) {
						gameOver = TRUE;
					}
				}

				if (gameOver)
				{
					TransmitEndOfGameNetMsg();
					netGameData.myGameState = NGS_EndGameScreen;
				}
			}
		}
	}

	/* print my score on the screen (temporary) */
	if (netGameData.myGameState == NGS_Playing)
	{
#if 0
		int myScore;
		int myIndex = PlayerIdInPlayerList(AvPNetID);
		myScore = AddUpPlayerFrags(myIndex);
		LOCALASSERT(myIndex != NET_IDNOTINPLAYERLIST);
#if PreBeta
		{
			extern void jtextprint(const char * t, ...);
			jtextprint("NET GAME SCORE: %d \n", myScore);
		}
#else
		textprint("NET GAME SCORE: %d \n", myScore);
#endif
#else
#endif
#if EXTRAPOLATION_TEST
		PlayerGhostExtrapolation();
#endif
		// For the 10-second report: how long the frame took (net_watch.h). This is the frame just played, so it counts frames of play only.
		Net_NoteFrame(RealFrameTime);
	}

	LogNetInfo("Finished message collection post-processing \n");

	if (MultiplayerObservedPlayer) {
		CheckStateOfObservedPlayer();
	}
}

// A new player, once networking.cpp has let them in. False if there's no room for them.
bool AddPlayerToGame(NetID id, const char *name)
{
	OutputDebugString("AddPlayerToGame() called\n");
	LOCALASSERT(AvP.Network == I_Host);
	
	// find a free slot for the player
	int freePlayerIndex = EmptySlotInPlayerList();
	if (freePlayerIndex == NET_NOEMPTYSLOTINPLAYERLIST) {
		return false;
	}

	/* initialise the slot */
	netGameData.playerData[freePlayerIndex].playerId = id;
	strncpy(netGameData.playerData[freePlayerIndex].name, name, NET_PLAYERNAMELENGTH - 1);
	netGameData.playerData[freePlayerIndex].name[NET_PLAYERNAMELENGTH - 1] = '\0';
	NetCleanText(netGameData.playerData[freePlayerIndex].name, NET_PLAYERNAMELENGTH);

	// Nobody else may have this name, or they could seem to speak for each other. Eight players can always find a number.
	NetMakeNameUnique(netGameData.playerData[freePlayerIndex].name, NET_PLAYERNAMELENGTH, [freePlayerIndex](const char *candidate)
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			if (i != freePlayerIndex && netGameData.playerData[i].playerId != 0 && _strnicmp(netGameData.playerData[i].name, candidate, NET_PLAYERNAMELENGTH) == 0)
			{
				return true;
			}
		}

		return false;
	}, NET_MAXPLAYERS + 1);

	//  ConvertNetNameToUpperCase(netGameData.playerData[freePlayerIndex].name);
	netGameData.playerData[freePlayerIndex].characterType = NGCT_Marine;
	netGameData.playerData[freePlayerIndex].characterSubType = NGSCT_General;
	netGameData.playerData[freePlayerIndex].startFlag = 0;
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++) {
			netGameData.playerData[freePlayerIndex].playerFrags[i] = 0;
		}

		netGameData.playerData[freePlayerIndex].playerScore = 0;
		netGameData.playerData[freePlayerIndex].playerScoreAgainst = 0;
		netGameData.playerData[freePlayerIndex].aliensKilled[0] = 0;
		netGameData.playerData[freePlayerIndex].aliensKilled[1] = 0;
		netGameData.playerData[freePlayerIndex].aliensKilled[2] = 0;
		netGameData.playerData[freePlayerIndex].deathsFromAI = 0;
		netGameData.playerData[freePlayerIndex].playerAlive = 1;
		netGameData.playerData[freePlayerIndex].playerHasLives = 1;
		netGameData.playerData[freePlayerIndex].lastKnownPosition.vx = 0;
		netGameData.playerData[freePlayerIndex].lastKnownPosition.vy = 100000000;
		netGameData.playerData[freePlayerIndex].lastKnownPosition.vz = 0;
	}
	Inform_PlayerHasConnected(id);
	return true;
}

// A client's connection has closed, however that happened. If it said it was leaving first, it has already gone, and this changes nothing.
void Net_PlayerHasGone(NetID id)
{
	if (PlayerIdInPlayerList(id) != NET_IDNOTINPLAYERLIST)
	{
		ProcessNetMsg_PlayerLeaving(id);
	}
}

// A client's connection to its host has closed, so its game is over: clients can only talk to each other through the host. In the menus nothing is running, and the menus see this themselves.
void Net_HostHasGone(void)
{
	if (netGameData.myGameState == NGS_Error_HostLost)
	{
		return;
	}

	bool const inTheMenus = netGameData.myGameState == NGS_Joining;

	netGameData.myGameState = NGS_Error_HostLost;

	if (!inTheMenus)
	{
		AvP.MainLoopRunning = 0;
	}
}

void RemovePlayerFromGame(NetID id)
{
	//LOCALASSERT(AvP.Network==I_Host);
	/* get player index from dpid */
	int playerIndex = PlayerIdInPlayerList(id);

	if (playerIndex == NET_IDNOTINPLAYERLIST)
	{
		/* the player is not actually in the game, so do nothing. This might occur, for
		example if a player tries to join when the game is full */
		return;
	}

	/* free the slot */
	netGameData.playerData[playerIndex].playerId = 0;

	for (int j = 0; j < NET_PLAYERNAMELENGTH; j++) {
		netGameData.playerData[playerIndex].name[j] = '\0';
	}

	netGameData.playerData[playerIndex].characterType = NGCT_Marine;
	netGameData.playerData[playerIndex].characterSubType = NGSCT_General;
	netGameData.playerData[playerIndex].startFlag = 0;
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			netGameData.playerData[playerIndex].playerFrags[i] = 0;
			netGameData.playerData[i].playerFrags[playerIndex] = 0;
		}

		netGameData.playerData[playerIndex].playerScore = 0;
		netGameData.playerData[playerIndex].playerScoreAgainst = 0;
		netGameData.playerData[playerIndex].aliensKilled[0] = 0;
		netGameData.playerData[playerIndex].aliensKilled[1] = 0;
		netGameData.playerData[playerIndex].aliensKilled[2] = 0;
		netGameData.playerData[playerIndex].deathsFromAI = 0;
		netGameData.playerData[playerIndex].playerAlive = 1;
		netGameData.playerData[playerIndex].playerHasLives = 1;
		netGameData.playerData[playerIndex].lastKnownPosition.vx = 0;
		netGameData.playerData[playerIndex].lastKnownPosition.vy = 100000000;
		netGameData.playerData[playerIndex].lastKnownPosition.vz = 0;
	}
}

/*----------------------------------------------------------------------
  What a game message is allowed to be. See WHAT A GAME MESSAGE IS MADE OF in net_checks.h
  ----------------------------------------------------------------------*/

// What a client may send each second of the kinds that cost every machine something. Each is far more than a real game sends: nobody dies twice in a second, and a burst of cluster grenades is a few explosions. A big explosion alone makes about 4 pieces of flying debris, and a message can hold 180 of them.
static const unsigned short kMostKillsEachSecond = 2;
static const unsigned short kMostChatLinesEachSecond = 4;
static const unsigned short kMostExplosionsEachSecond = 24;
static const unsigned short kMostGibbingsEachSecond = 16;
static const unsigned short kMostThingsDestroyedEachSecond = 60;
static const unsigned short kMostHitsEachSecond = 240; // the biggest shotgun fires 32 pellets, and a body can be hit by each
static const unsigned short kMostMapAnswersEachSecond = 4; // a joiner answers every 2 seconds, and again each time a choice changes
static const unsigned short kMostLeavingsEachSecond = NET_MESSAGEITERATIONS; // a machine that's leaving says so this many times, because it gets no other chance

// In the same order as NETMESSAGETYPE, which the compiler checks below. The last column is whether only the host may say it. A client has no way to send these in a real game, and whoever hears one believes it.
static constexpr NetPartRule kNetPartRules[] =
{
	{ NetMT_GameDescription, sizeof(NETMESSAGE_GAMEDESCRIPTION), false, true },
	{ NetMT_PlayerDescription, sizeof(NETMESSAGE_PLAYERDESCRIPTION), false, false },
	{ NetMT_StartGame, 0, false, true },
	{ NetMT_PlayerState, sizeof(NETMESSAGE_PLAYERSTATE), false, false },
	{ NetMT_PlayerState_Minimal, sizeof(NETMESSAGE_PLAYERSTATE_MINIMAL), false, false },
	{ NetMT_PlayerState_Medium, sizeof(NETMESSAGE_PLAYERSTATE_MEDIUM), false, false },
	{ NetMT_PlayerKilled, sizeof(NETMESSAGE_PLAYERKILLED), false, false, kMostKillsEachSecond },
	{ NetMT_PlayerLeaving, 0, false, false, kMostLeavingsEachSecond },
	{ NetMT_AllGameScores, sizeof(NETMESSAGE_ALLGAMESCORES), false, true },
	{ NetMT_PlayerScores, sizeof(NETMESSAGE_PLAYERSCORES), false, true },
	{ NetMT_LocalRicochet, sizeof(NETMESSAGE_LOCALRICOCHET), false, false },
	{ NetMT_LocalObjectState, sizeof(NETMESSAGE_LOBSTATE), false, false },
	{ NetMT_LocalObjectDamaged, sizeof(NETMESSAGE_LOBDAMAGED_HEADER), true, false, kMostHitsEachSecond },
	{ NetMT_LocalObjectDestroyed, sizeof(NETMESSAGE_LOBDESTROYED), false, false, kMostThingsDestroyedEachSecond },
	{ NetMT_ObjectPickedUp, sizeof(NETMESSAGE_OBJECTPICKEDUP), false, false },
	{ NetMT_InanimateObjectDamaged, sizeof(NETMESSAGE_INANIMATEDAMAGED_HEADER), true, false },
	{ NetMT_InanimateObjectDestroyed, sizeof(NETMESSAGE_INANIMATEDESTROYED), false, true },
	{ NetMT_LOSRequestBinarySwitch, sizeof(NETMESSAGE_LOSREQUESTBINARYSWITCH), false, false },
	{ NetMT_PlatformLiftState, sizeof(NETMESSAGE_PLATFORMLIFTSTATE), false, true },
	{ NetMT_RequestPlatformLiftActivate, sizeof(NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE), false, false },
	{ NetMT_PlayerAutoGunState, sizeof(NETMESSAGE_AGUNSTATE), false, true },
	{ NetMT_MakeDecal, sizeof(NETMESSAGE_MAKEDECAL), false, false },
	{ NetMT_ChatBroadcast, 2, true, false, kMostChatLinesEachSecond },
	{ NetMT_MakeExplosion, sizeof(NETMESSAGE_MAKEEXPLOSION), false, false, kMostExplosionsEachSecond },
	{ NetMT_MakeFlechetteExplosion, sizeof(NETMESSAGE_MAKEFLECHETTEEXPLOSION), false, false, kMostExplosionsEachSecond },
	{ NetMT_MakePlasmaExplosion, sizeof(NETMESSAGE_MAKEPLASMAEXPLOSION), false, false, kMostExplosionsEachSecond },
	{ NetMT_PredatorSights, sizeof(NETMESSAGE_PREDATORSIGHTS), false, false },
	{ NetMT_LocalObjectOnFire, sizeof(NETMESSAGE_LOBONFIRE), false, false },
	{ NetMT_RestartNetworkGame, sizeof(NETMESSAGE_RESTARTGAME), false, true },
	{ NetMT_FragmentalObjectsStatus, sizeof(NETMESSAGE_FRAGMENTALOBJECTSSTATUS), false, true },
	{ NetMT_AlienAIState, sizeof(NETMESSAGE_ALIENAISTATE), false, true },
	{ NetMT_AlienAISequenceChange, sizeof(NETMESSAGE_ALIENSEQUENCECHANGE), false, true },
	{ NetMT_AlienAIKilled, sizeof(NETMESSAGE_ALIENAIKILLED), false, true },
	{ NetMT_GhostHierarchyDamaged, sizeof(NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER), true, false, kMostHitsEachSecond },
	{ NetMT_SpotAlienSound, sizeof(NETMESSAGE_SPOTALIENSOUND), false, true },
	{ NetMT_LocalObjectDestroyed_Request, sizeof(NETMESSAGE_LOBDESTROYED_REQUEST), false, false },
	{ NetMT_LastManStanding_Restart, sizeof(NETMESSAGE_LMS_RESTART), false, true },
	{ NetMT_LastManStanding_RestartInfo, sizeof(NETMESSAGE_PLAYERID), false, true },
	{ NetMT_LastManStanding_RestartCountDown, sizeof(NETMESSAGE_LMS_RESTARTTIMER), false, true },
	{ NetMT_LastManStanding_LastMan, sizeof(NETMESSAGE_PLAYERID), false, true },
	{ NetMT_PredatorTag_NewPredator, sizeof(NETMESSAGE_PLAYERID), false, true },
	{ NetMT_EndGame, 0, false, true },
	{ NetMT_CreateWeapon, sizeof(NETMESSAGE_CREATEWEAPON), false, false },
	{ NetMT_RespawnPickups, 0, false, true },
	{ NetMT_ScoreChange, sizeof(NETMESSAGE_SCORECHANGE), false, true },
	{ NetMT_Gibbing, sizeof(NETMESSAGE_GIBBING), false, false, kMostGibbingsEachSecond },
	{ NetMT_CorpseDeathAnim, sizeof(NETMESSAGE_CORPSEDEATHANIM), false, false, kMostKillsEachSecond },
	{ NetMT_StrategySynch, sizeof(NETMESSAGE_STRATEGYSYNCH), false, true },
	{ NetMT_FrameTimer, sizeof(NETMESSAGE_FRAMETIMER), false, false },
	{ NetMT_SpeciesScores, sizeof(NETMESSAGE_SPECIESSCORES), false, true },
	{ NetMT_FarAlienPosition, sizeof(NETMESSAGE_FARALIENPOSITION), false, true },
	{ NetMT_SpotOtherSound, sizeof(NETMESSAGE_SPOTOTHERSOUND), false, false },
	{ NetMT_CoopNextLevel, sizeof(NETMESSAGE_COOPNEXTLEVEL), false, true },
	{ NetMT_LevelEvent, sizeof(NETMESSAGE_LEVELEVENT), false, true },
	{ NetMT_LevelEventAsk, sizeof(NETMESSAGE_LEVELEVENT), false, false },
	{ NetMT_CoopReady, 0, false, false },
	{ NetMT_CoopGo, 0, false, true },
	{ NetMT_NpcState, sizeof(NETMESSAGE_NPCSTATE), false, true },
	{ NetMT_NpcXenoDeltas, sizeof(NETMESSAGE_NPCXENODELTAS), false, true },
	{ NetMT_NpcKilled, sizeof(NETMESSAGE_NPCKILLED), false, true },
	{ NetMT_NpcSound, sizeof(NETMESSAGE_NPCSOUND), false, true },
	{ NetMT_NpcThrownObject, sizeof(NETMESSAGE_NPCTHROWNOBJECT), false, true },
	{ NetMT_LiftState, sizeof(NETMESSAGE_LIFTSTATE), false, true },
	{ NetMT_CoopCampaignChoice, sizeof(NETMESSAGE_COOPCAMPAIGNCHOICE), false, true },
	{ NetMT_CoopMapAnswer, sizeof(NETMESSAGE_COOPMAPANSWER), false, false, kMostMapAnswersEachSecond }
};

// The build stops here if a message changes while the multiplayer version doesn't. See that file.
#include "net_version_check.h"

static constexpr bool NetPartRulesAreInOrder()
{
	for (size_t i = 0; i < sizeof(kNetPartRules) / sizeof(kNetPartRules[0]); i++)
	{
		if (kNetPartRules[i].kind != i)
		{
			return false;
		}
	}

	return true;
}

static_assert(NetPartRulesAreInOrder(), "the table has to be in the same order as NETMESSAGETYPE, because a kind is looked up by its place");

// How long a part whose length varies really is. The damage messages say with their flags what else they carry, and the rule has already made sure the flags are there.
static size_t LengthOfANetPart(unsigned char kind, const unsigned char *part, size_t bytesLeft)
{
	char *const asRead = (char *)part;

	switch (kind)
	{
		case NetMT_LocalObjectDamaged:
			return (size_t)GetSizeOfLocalObjectDamagedMessage(asRead);

		case NetMT_InanimateObjectDamaged:
			return (size_t)GetSizeOfInanimateDamagedMessage(asRead);

		case NetMT_GhostHierarchyDamaged:
			return (size_t)GetSizeOfGhostHierarchyDamagedMessage(asRead);

		case NetMT_ChatBroadcast:
			return NetChatPartLength(part, bytesLeft);

		default:
			return 0;
	}
}

// Asked by networking.cpp about everything that arrives, before any of it reaches the game. See WHO IS WHO there.
void Net_NoteRefused(const char *why)
{
	static NetRefusalCounts refused;

	unsigned long long const count = NetCountRefusal(refused, why);

	if (NetLogThisRefusal(count))
	{
		LogString(std::string("net: refused: ") + why + " (number " + std::to_string(count) + " of these)");
	}
}

// How long each list of levels is. The menus' own code fills these in.
extern int NumMultiplayerLevels;
extern int NumCoopLevels;

bool Net_HostsLevelIsOnItsList()
{
	if (NetLevelIsOnItsList(netGameData.levelNumber, netGameData.gameType == NGT_Coop ? NumCoopLevels : NumMultiplayerLevels))
	{
		return true;
	}

	Net_NoteRefused("a host named a level that isn't on that game's list");
	return false;
}

bool Net_ClientCanAfford(const uint8_t *message, size_t size, size_t connection)
{
	static NetPartsSpent spent[NET_MAXPLAYERS + 1];

	if (connection >= sizeof(spent) / sizeof(spent[0]) || size < kMessageHeaderSize)
	{
		return false;
	}

	if (NetPartsFitTheBudget(message + kMessageHeaderSize, size - kMessageHeaderSize, kNetPartRules, sizeof(kNetPartRules) / sizeof(kNetPartRules[0]), LengthOfANetPart, spent[connection], timeGetTime()))
	{
		return true;
	}

	Net_NoteRefused("a client sent more of one kind of part in a second than any game does");
	return false;
}

// What's kept back of the game's 1000 strategy blocks and 750 display blocks. A player's own corpse and shots come out of this. The sums for a crowded level, about 70 computer players near, use 70 of the first and up to 140 of the second.
static const int kStrategyBlocksKeptBack = 100;
static const int kDisplayBlocksKeptBack = 75;

// In object.cpp: how many display blocks are in use
extern int NumActiveBlocks;

bool Net_BlocksAreShort()
{
	return AvP.Network != I_No_Network && (NetPoolIsShort(NumActiveStBlocks, maxstblocks, kStrategyBlocksKeptBack) || NetPoolIsShort(NumActiveBlocks, maxobjects, kDisplayBlocksKeptBack));
}

// THE CO-OP LOBBY. See coop_lobby.h for what it is and why. These two lists are all it takes: every other kind is refused here, before
// anything reads it, and before a host passes anything on. Chat isn't on them, because a menu page can neither type it nor show it, and
// a player's description isn't on the host's, because a host sends none in the lobby. tests/coop_lobby_test.cpp holds these same lists
// and every kind against them, so adding to one has to be decided there too.
static const unsigned char kCoopLobbyFromAJoiner[] = { NetMT_PlayerDescription, NetMT_PlayerLeaving, NetMT_CoopMapAnswer };
static const unsigned char kCoopLobbyFromTheHost[] = { NetMT_GameDescription, NetMT_PlayerLeaving, NetMT_CoopCampaignChoice };
static const unsigned char kCoopJustTheAnswer[] = { NetMT_CoopMapAnswer };

static_assert(kCoopChoiceCustomCampaign == kCampaignCustom, "coop_lobby.h and env_info.h have to agree on what a custom map's campaign number is");
static_assert(kCoopChoiceFirstCampaign == kCampaignMarine, "coop_lobby.h and env_info.h have to agree on where the campaigns start");
static_assert(sizeof(netGameData.coopCustomMap) == kCoopMapNameBytes, "the game's copy of the name has to be the size of the message's");

static int coopLobbyState = kCoopLobbyNone;
static bool coopHostHasStarted = false;

int Net_CoopLobbyState(void)
{
	return coopLobbyState;
}

// An answer about the map means nothing to the other joiners, so a host keeps it to itself
bool Net_MessageIsForTheHostAlone(const uint8_t *message, size_t size)
{
	size_t partCount = 0;
	size_t onTheList = 0;

	return NetCountPartsOnTheList(message + kMessageHeaderSize, size - kMessageHeaderSize, kNetPartRules, sizeof(kNetPartRules) / sizeof(kNetPartRules[0]), LengthOfANetPart, kCoopJustTheAnswer, sizeof(kCoopJustTheAnswer), &partCount, &onTheList) && onTheList != 0;
}

// Does this machine take this message, as far as the lobby goes? It has already been shown to be well made.
static bool Net_CoopLobbyTakesThis(const uint8_t *message, size_t size)
{
	unsigned char const *const parts = message + kMessageHeaderSize;
	size_t const bytes = size - kMessageHeaderSize;
	size_t const ruleCount = sizeof(kNetPartRules) / sizeof(kNetPartRules[0]);
	size_t partCount = 0;
	size_t onTheList = 0;

	if (coopLobbyState == kCoopLobbyNone)
	{
		// An answer means nothing in a game with no lobby. Once a host has started, one can still be on its way, which is honest, so that isn't refused.
		if (AvP.Network == I_Host && !coopHostHasStarted && Net_MessageIsForTheHostAlone(message, size))
		{
			Net_NoteRefused(kCoopRefusedAnswerNoLobby);
			return false;
		}

		return true;
	}

	bool const isHost = AvP.Network == I_Host;
	unsigned char const *const list = isHost ? kCoopLobbyFromAJoiner : kCoopLobbyFromTheHost;
	size_t const listCount = isHost ? sizeof(kCoopLobbyFromAJoiner) : sizeof(kCoopLobbyFromTheHost);

	if (NetCountPartsOnTheList(parts, bytes, kNetPartRules, ruleCount, LengthOfANetPart, list, listCount, &partCount, &onTheList) && onTheList == partCount)
	{
		return true;
	}

	// A game under way is honest traffic this machine isn't ready for, so the log says that instead
	Net_NoteRefused(coopLobbyState == kCoopLobbyUnderWay ? kCoopRefusedPartBeforeJoining : kCoopRefusedPartInLobby);
	return false;
}

bool Net_GameMessageIsGood(const uint8_t *message, size_t size, bool fromTheHost)
{
	if (size < kMessageHeaderSize)
	{
		Net_NoteRefused("a message too short to have a header");
		return false;
	}

	switch (NetCheckParts(message + kMessageHeaderSize, size - kMessageHeaderSize, kNetPartRules, sizeof(kNetPartRules) / sizeof(kNetPartRules[0]), fromTheHost, LengthOfANetPart))
	{
		case kNetPartsGood:
			return Net_CoopLobbyTakesThis(message, size);
		case kNetPartsOnlyFromTheHost:
			Net_NoteRefused("a part only a host may send");
			return false;
		case kNetPartsUnknownKind:
			Net_NoteRefused("a part of a kind that doesn't exist");
			return false;
		default:
			Net_NoteRefused("a part that doesn't fit its message");
			return false;
	}
}

// How many new things other machines may make here in a second. Far more than a real battle. A limit on a second isn't enough by itself, because a spear lasts 20 seconds, so what one player has alive is counted as well.
static const int kMostNewGhostsEachSecond = 60;
static const int kMostLiveThingsOfOnePlayer = 48;
static const int kMostDroppedGunsEachSecond = 16;

// Each player has their own count, so one flooding can't stop anyone else's ghosts appearing
static unsigned int newGhostsSecond[NET_MAXPLAYERS];
static int newGhostsUsed[NET_MAXPLAYERS];
static unsigned int droppedGunsSecond[NET_MAXPLAYERS];
static int droppedGunsUsed[NET_MAXPLAYERS];

static bool GhostKindCanBeMadeByMessage(AVP_BEHAVIOUR_TYPE type)
{
	switch (type)
	{
		case I_BehaviourRocket:
		case I_BehaviourGrenade:
		case I_BehaviourProximityGrenade:
		case I_BehaviourPulseGrenade:
		case I_BehaviourFragmentationGrenade:
		case I_BehaviourClusterGrenade:
		case I_BehaviourPredatorEnergyBolt:
		case I_BehaviourFrisbeeEnergyBolt:
		case I_BehaviourPPPlasmaBolt:
		case I_BehaviourNPCPredatorDisc:
		case I_BehaviourPredatorDisc_SeekTrack:
		case I_BehaviourFrisbee:
		case I_BehaviourAlienSpit:
		case I_BehaviourSpeargunBolt:
		case I_BehaviourFlareGrenade:
		case I_BehaviourInanimateObject:
			return true;
		default:
			return false;
	}
}

// The alien messages find a ghost by its number, and any other kind has no alien's model to play
static bool IsAnAlienGhost(STRATEGYBLOCK *sbPtr)
{
	return ((NETGHOSTDATABLOCK *)sbPtr->SBdataptr)->type == I_BehaviourAlien;
}

static bool IsAPlayersGhost(STRATEGYBLOCK *sbPtr)
{
	AVP_BEHAVIOUR_TYPE const type = ((NETGHOSTDATABLOCK *)sbPtr->SBdataptr)->type;

	return type == I_BehaviourMarinePlayer || type == I_BehaviourAlienPlayer || type == I_BehaviourPredatorPlayer;
}

/*----------------------------------------------------------------------
  Core function for processing game messages
  ----------------------------------------------------------------------*/
static void ProcessGameMessage(NetID senderId, uint8_t *msgP, size_t msgSize)
{
	uint8_t *subMessagePtr = NULL;
	uint8_t *endOfMessage = NULL;
	NETMESSAGEHEADER *headerPtr = NULL;
	LogNetInfo("Processing a game message \n");

	// check for invalid parameters
	if ((msgSize == 0) || (msgP == NULL)) {
		return;
	}

	// some assertions about our game state
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Leaving)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_GameFull)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_GameStarted)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_HostLost)));

	// In leaving or error states, we can ignore game messages
	if ((netGameData.myGameState != NGS_StartUp)
	    && (netGameData.myGameState != NGS_Playing)
	    && (netGameData.myGameState != NGS_Joining)
	    && (netGameData.myGameState != NGS_EndGameScreen))
	{
		return;
	}

	/* skip past our header */
	subMessagePtr = &msgP[kMessageHeaderSize];
	endOfMessage = &msgP[msgSize];

	/* Read through to the end of the message... */
	while (subMessagePtr < endOfMessage)
	{
		headerPtr = (NETMESSAGEHEADER *)subMessagePtr;
		subMessagePtr += sizeof(NETMESSAGEHEADER);

		switch (headerPtr->type)
		{
			case (NetMT_GameDescription):
			{
				ProcessNetMsg_GameDescription((NETMESSAGE_GAMEDESCRIPTION *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_GAMEDESCRIPTION);
				break;
			}

			case (NetMT_PlayerDescription):
			{
				ProcessNetMsg_PlayerDescription((NETMESSAGE_PLAYERDESCRIPTION *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERDESCRIPTION);
				break;
			}

			case (NetMT_StartGame):
			{
				ProcessNetMsg_StartGame();
				break;
			}

			case (NetMT_PlayerState):
			{
				ProcessNetMsg_PlayerState((NETMESSAGE_PLAYERSTATE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERSTATE);
				break;
			}

			case (NetMT_PlayerState_Minimal):
			{
				ProcessNetMsg_PlayerState_Minimal((NETMESSAGE_PLAYERSTATE_MINIMAL *)subMessagePtr, senderId, FALSE);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERSTATE_MINIMAL);
				break;
			}

			case (NetMT_PlayerState_Medium):
			{
				ProcessNetMsg_PlayerState_Minimal((NETMESSAGE_PLAYERSTATE_MINIMAL *)subMessagePtr, senderId, TRUE);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERSTATE_MEDIUM);
				break;
			}

			case (NetMT_FrameTimer):
			{
				ProcessNetMsg_FrameTimer(((NETMESSAGE_FRAMETIMER *)subMessagePtr)->frame_time, senderId);
				subMessagePtr += sizeof(NETMESSAGE_FRAMETIMER);
				break;
			}

			case (NetMT_PlayerKilled):
			{
				ProcessNetMsg_PlayerKilled((NETMESSAGE_PLAYERKILLED *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERKILLED);
				break;
			}

			case (NetMT_CorpseDeathAnim):
			{
				ProcessNetMsg_PlayerDeathAnim((NETMESSAGE_CORPSEDEATHANIM *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_CORPSEDEATHANIM);
				break;
			}

			case (NetMT_PlayerLeaving):
			{
				ProcessNetMsg_PlayerLeaving(senderId);
				break;
			}

			case (NetMT_AllGameScores):
			{
				ProcessNetMsg_AllGameScores((NETMESSAGE_ALLGAMESCORES *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_ALLGAMESCORES);
				break;
			}

			case (NetMT_PlayerScores):
			{
				ProcessNetMsg_PlayerScores((NETMESSAGE_PLAYERSCORES *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERSCORES);
				break;
			}

			case (NetMT_LocalRicochet):
			{
				ProcessNetMsg_LocalRicochet((NETMESSAGE_LOCALRICOCHET *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_LOCALRICOCHET);
				break;
			}

			case (NetMT_LocalObjectState):
			{
				ProcessNetMsg_LocalObjectState((NETMESSAGE_LOBSTATE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_LOBSTATE);
				break;
			}

			case (NetMT_LocalObjectDamaged):
			{
				ProcessNetMsg_LocalObjectDamaged((char *)subMessagePtr, senderId);
				subMessagePtr += GetSizeOfLocalObjectDamagedMessage((char *)subMessagePtr);
				break;
			}

			case (NetMT_LocalObjectDestroyed):
			{
				ProcessNetMsg_LocalObjectDestroyed((NETMESSAGE_LOBDESTROYED *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_LOBDESTROYED);
				break;
			}

			case (NetMT_ObjectPickedUp):
			{
				ProcessNetMsg_ObjectPickedUp((NETMESSAGE_OBJECTPICKEDUP *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_OBJECTPICKEDUP);
				break;
			}

			case (NetMT_EndGame):
			{
				ProcessNetMsg_EndGame();
				break;
			}

			case (NetMT_InanimateObjectDamaged):
			{
				ProcessNetMsg_InanimateObjectDamaged((char *)subMessagePtr);
				subMessagePtr += GetSizeOfInanimateDamagedMessage((char *)subMessagePtr);
				break;
			}

			case (NetMT_InanimateObjectDestroyed):
			{
				ProcessNetMsg_InanimateObjectDestroyed((NETMESSAGE_INANIMATEDESTROYED *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_INANIMATEDESTROYED);
				break;
			}

			case (NetMT_LOSRequestBinarySwitch):
			{
				ProcessNetMsg_LOSRequestBinarySwitch((NETMESSAGE_LOSREQUESTBINARYSWITCH *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_LOSREQUESTBINARYSWITCH);
				break;
			}

			case (NetMT_PlatformLiftState):
			{
				ProcessNetMsg_PlatformLiftState((NETMESSAGE_PLATFORMLIFTSTATE *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_PLATFORMLIFTSTATE);
				break;
			}

			case (NetMT_RequestPlatformLiftActivate):
			{
				ProcessNetMsg_RequestPlatformLiftActivate((NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE);
				break;
			}

			case (NetMT_PlayerAutoGunState):
			{
				ProcessNetMsg_PlayerAutoGunState((NETMESSAGE_AGUNSTATE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_AGUNSTATE);
				break;
			}

			case (NetMT_MakeDecal):
			{
				ProcessNetMsg_MakeDecal((NETMESSAGE_MAKEDECAL *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_MAKEDECAL);
				break;
			}

			case (NetMT_ChatBroadcast):
			{
				subMessagePtr = ProcessNetMsg_ChatBroadcast(subMessagePtr, senderId);
				break;
			}

			case (NetMT_MakeExplosion):
			{
				ProcessNetMsg_MakeExplosion((NETMESSAGE_MAKEEXPLOSION *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_MAKEEXPLOSION);
				break;
			}

			case (NetMT_MakeFlechetteExplosion):
			{
				ProcessNetMsg_MakeFlechetteExplosion((NETMESSAGE_MAKEFLECHETTEEXPLOSION *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_MAKEFLECHETTEEXPLOSION);
				break;
			}

			case (NetMT_MakePlasmaExplosion):
			{
				ProcessNetMsg_MakePlasmaExplosion((NETMESSAGE_MAKEPLASMAEXPLOSION *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_MAKEPLASMAEXPLOSION);
				break;
			}

			case (NetMT_PredatorSights):
			{
				ProcessNetMsg_PredatorSights((NETMESSAGE_PREDATORSIGHTS *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_PREDATORSIGHTS);
				break;
			}

			case (NetMT_LocalObjectOnFire):
			{
				ProcessNetMsg_LocalObjectOnFire((NETMESSAGE_LOBONFIRE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_LOBONFIRE);
				break;
			}

			case (NetMT_RestartNetworkGame):
			{
				RestartNetworkGame(((NETMESSAGE_RESTARTGAME *)subMessagePtr)->seed);
				subMessagePtr += sizeof(NETMESSAGE_RESTARTGAME);
				break;
			}

			case (NetMT_FragmentalObjectsStatus):
			{
				ProcessNetMsg_FragmentalObjectsStatus((NETMESSAGE_FRAGMENTALOBJECTSSTATUS *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_FRAGMENTALOBJECTSSTATUS);
				break;
			}

			case (NetMT_StrategySynch):
			{
				ProcessNetMsg_StrategySynch((NETMESSAGE_STRATEGYSYNCH *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_STRATEGYSYNCH);
				break;
			}

			case (NetMT_AlienAIState):
			{
				ProcessNetMsg_AlienAIState((NETMESSAGE_ALIENAISTATE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_ALIENAISTATE);
				break;
			}

			case (NetMT_AlienAISequenceChange):
			{
				ProcessNetMsg_AlienAISequenceChange((NETMESSAGE_ALIENSEQUENCECHANGE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_ALIENSEQUENCECHANGE);
				break;
			}

			case (NetMT_AlienAIKilled):
			{
				ProcessNetMsg_AlienAIKilled((NETMESSAGE_ALIENAIKILLED *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_ALIENAIKILLED);
				break;
			}

			case (NetMT_FarAlienPosition):
			{
				ProcessNetMsg_FarAlienPosition((NETMESSAGE_FARALIENPOSITION *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_FARALIENPOSITION);
				break;
			}

			case (NetMT_GhostHierarchyDamaged):
			{
				ProcessNetMsg_GhostHierarchyDamaged((char *)subMessagePtr, senderId);
				subMessagePtr += GetSizeOfGhostHierarchyDamagedMessage((char *)subMessagePtr) ;
				break;
			}

			case (NetMT_Gibbing):
			{
				ProcessNetMsg_Gibbing((NETMESSAGE_GIBBING *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_GIBBING);
				break;
			}

			case (NetMT_SpotAlienSound):
			{
				ProcessNetMsg_SpotAlienSound((NETMESSAGE_SPOTALIENSOUND *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_SPOTALIENSOUND);
				break;
			}

			case (NetMT_SpotOtherSound):
			{
				ProcessNetMsg_SpotOtherSound((NETMESSAGE_SPOTOTHERSOUND *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_SPOTOTHERSOUND);
				break;
			}

			case (NetMT_CoopNextLevel):
			{
				ProcessNetMsg_CoopNextLevel((NETMESSAGE_COOPNEXTLEVEL *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_COOPNEXTLEVEL);
				break;
			}

			case (NetMT_CoopReady):
			{
				ProcessNetMsg_CoopReady(senderId);
				break;
			}

			case (NetMT_CoopGo):
			{
				ProcessNetMsg_CoopGo();
				break;
			}

			case (NetMT_NpcState):
			{
				ProcessNetMsg_NpcState((NETMESSAGE_NPCSTATE *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_NPCSTATE);
				break;
			}

			case (NetMT_NpcXenoDeltas):
			{
				ProcessNetMsg_NpcXenoDeltas((NETMESSAGE_NPCXENODELTAS *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_NPCXENODELTAS);
				break;
			}

			case (NetMT_NpcKilled):
			{
				ProcessNetMsg_NpcKilled((NETMESSAGE_NPCKILLED *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_NPCKILLED);
				break;
			}

			case (NetMT_NpcSound):
			{
				ProcessNetMsg_NpcSound((NETMESSAGE_NPCSOUND *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_NPCSOUND);
				break;
			}

			case (NetMT_NpcThrownObject):
			{
				ProcessNetMsg_NpcThrownObject((NETMESSAGE_NPCTHROWNOBJECT *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_NPCTHROWNOBJECT);
				break;
			}

			case (NetMT_LiftState):
			{
				ProcessNetMsg_LiftState((NETMESSAGE_LIFTSTATE *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_LIFTSTATE);
				break;
			}

			case (NetMT_CoopCampaignChoice):
			{
				ProcessNetMsg_CoopCampaignChoice((NETMESSAGE_COOPCAMPAIGNCHOICE *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_COOPCAMPAIGNCHOICE);
				break;
			}

			case (NetMT_CoopMapAnswer):
			{
				ProcessNetMsg_CoopMapAnswer((NETMESSAGE_COOPMAPANSWER *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_COOPMAPANSWER);
				break;
			}

			case (NetMT_LevelEvent):
			{
				ProcessNetMsg_LevelEvent((NETMESSAGE_LEVELEVENT *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_LEVELEVENT);
				break;
			}

			case (NetMT_LevelEventAsk):
			{
				ProcessNetMsg_LevelEventAsk((NETMESSAGE_LEVELEVENT *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_LEVELEVENT);
				break;
			}

			case (NetMT_LocalObjectDestroyed_Request):
			{
				ProcessNetMsg_LocalObjectDestroyed_Request((NETMESSAGE_LOBDESTROYED_REQUEST *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_LOBDESTROYED_REQUEST);
				break;
			}

			case (NetMT_LastManStanding_Restart):
			{
				Handle_LastManStanding_Restart(((NETMESSAGE_LMS_RESTART *)subMessagePtr)->playerID, ((NETMESSAGE_LMS_RESTART *)subMessagePtr)->seed);
				subMessagePtr += sizeof(NETMESSAGE_LMS_RESTART);
				break;
			}

			case (NetMT_LastManStanding_RestartInfo):
			{
				Handle_LastManStanding_RestartInfo(((NETMESSAGE_PLAYERID *)subMessagePtr)->playerID);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERID);
				break;
			}

			case (NetMT_LastManStanding_LastMan):
			{
				Handle_LastManStanding_LastMan(((NETMESSAGE_PLAYERID *)subMessagePtr)->playerID);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERID);
				break;
			}

			case (NetMT_LastManStanding_RestartCountDown):
			{
				Handle_LastManStanding_RestartTimer(((NETMESSAGE_LMS_RESTARTTIMER *)subMessagePtr)->timer);
				subMessagePtr += sizeof(NETMESSAGE_LMS_RESTARTTIMER);
				break;
			}

			case (NetMT_PredatorTag_NewPredator):
			{
				Handle_SpeciesTag_NewPersonIt(((NETMESSAGE_PLAYERID *)subMessagePtr)->playerID);
				subMessagePtr += sizeof(NETMESSAGE_PLAYERID);
				break;
			}

			case (NetMT_CreateWeapon):
			{
				ProcessNetMsg_CreateWeapon((NETMESSAGE_CREATEWEAPON *)subMessagePtr, senderId);
				subMessagePtr += sizeof(NETMESSAGE_CREATEWEAPON);
				break;
			}

			// (In a campaign nothing comes back, as in single player, whoever asks)
			case (NetMT_RespawnPickups):
			{
				if (!SameAsSinglePlayer() && (netGameData.myGameState == NGS_Playing ||
				    netGameData.myGameState == NGS_EndGameScreen))
				{
					RespawnAllPickups();
					PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_WEAPON_RESPAWN);
				}
				break;
			}

			case (NetMT_ScoreChange):
			{
				ProcessNetMsg_ScoreChange((NETMESSAGE_SCORECHANGE *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_SCORECHANGE);
				break;
			}

			case (NetMT_SpeciesScores):
			{
				ProcessNetMsg_SpeciesScores((NETMESSAGE_SPECIESSCORES *)subMessagePtr);
				subMessagePtr += sizeof(NETMESSAGE_SPECIESSCORES);
				break;
			}

			default:
			{
				// Net_GameMessageIsGood has refused a message with a part it doesn't know, so this can't happen. If it ever did, nothing after it could be read.
				return;
			}
		}
	}

	/* if we have read the message correctly, the message pointer should be exactly
	at the end of the message buffer*/
	LOCALASSERT(subMessagePtr == endOfMessage);
	LogNetInfo("Finished processing a game message \n");
}

#if CalculateBytesSentPerSecond
int GetBytesPerSecond(int bytesThisFrame)
{
	static int times[100];
	static int bytes[100];
	static int next_index;
	int i;
	int totalBytes = 0;
	times[next_index] = 0;
	bytes[next_index] = bytesThisFrame;
	next_index = (next_index + 1) % 100;

	for (i = 0; i < 100; i++)
	{
		times[i] += NormalFrameTime;
		totalBytes += bytes[i];
	}

	if (!times[next_index])
	{
		return 0;
	}

	return(DIV_FIXED(totalBytes, times[next_index]));
}
#endif

/*----------------------------------------------------------------------
  Core function for sending messages
  ----------------------------------------------------------------------*/
void NetSendMessages(void)
{
	/* some assertions about our game state */
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_GameFull)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_GameStarted)));
	LOCALASSERT(!((AvP.Network == I_Host) && (netGameData.myGameState == NGS_Error_HostLost)));

	/* only bother sending messages under certain game conditions... */
	if ((netGameData.myGameState != NGS_StartUp) && (netGameData.myGameState != NGS_EndGameScreen) && (netGameData.myGameState != NGS_Playing) && (netGameData.myGameState != NGS_Joining))
	{
		return;
	}

	LogNetInfo("Sending net messages... \n");

	/* at this point, add player and other object updates... doing this here
	ensures we are sending our most upto date info */
	if (netGameData.myGameState == NGS_Playing ||
	    netGameData.myGameState == NGS_EndGameScreen)
	{
		netGameData.gameDescriptionTimeDelay -= RealFrameTime;
		GameTimeSinceLastSend += NormalFrameTime;
		TimeCounterForExtrapolation += NormalFrameTime;
#if EXTRAPOLATION_TEST
		// update muzzle flashes here, since this happens after dynamics
		PostDynamicsExtrapolationUpdate();
#endif

		if (netGameData.sendFrequency)
		{
			// check to see if messages should be sent this frame
			netGameData.sendTimer -= RealFrameTime;

			if (netGameData.sendTimer > 0)
			{
				// don't send messages this frame
#if CalculateBytesSentPerSecond
				PrintDebuggingText("Bytes/Second: %d\n", GetBytesPerSecond(0));
#endif
				return;
			}

			netGameData.sendTimer += netGameData.sendFrequency;
			netGameData.sendTimer = max(0, netGameData.sendTimer);
		}

		if (netGameData.myGameState == NGS_EndGameScreen)
		{
			if (AvP.Network == I_Host)
			{
				PeriodicScoreUpdate();

				// send game description once per second
				if (netGameData.gameDescriptionTimeDelay < 0)
				{
					netGameData.gameDescriptionTimeDelay += 2 * ONE_FIXED;
					AddNetMsg_GameDescription();
					AddNetMsg_StrategySynch();
					AddNetMsg_FragmentalObjectsStatus();
				}
			}
		}
		else // game state is NGS_Playing
		{
			AddPlayerAndObjectUpdateMessages();

			if (AvP.Network == I_Host)
			{
				// send game description once per second
				if (netGameData.gameDescriptionTimeDelay < 0)
				{
					netGameData.gameDescriptionTimeDelay += ONE_FIXED;
					AddNetMsg_GameDescription();
					LevelEvents_SayAgainWhatHappened();
					Net_CoopHostSays();

					if (!SameAsSinglePlayer())
					{
						AddNetMsg_StrategySynch();
						AddNetMsg_FragmentalObjectsStatus();
					}
				}

				// Where a level has to be finished together, a door that's shut on one machine and open on another stops the game. Each message holds 16 objects, so this takes a level of 160 round in a second, for about 130 bytes a second to each machine. It's safe to say often: a machine ignores it for an object that's moving, or that it has just pressed itself.
				static DWORD lastObjectSynch = 0;

				if (SameAsSinglePlayer() && timeGetTime() - lastObjectSynch >= kMillisecondsBetweenObjectSynchs)
				{
					lastObjectSynch = timeGetTime();
					AddNetMsg_StrategySynch();
					AddNetMsg_FragmentalObjectsStatus();
				}

				PeriodicScoreUpdate();

				if (netGameData.gameType == NGT_LastManStanding) {
					CheckLastManStandingState();
				}
				else if (netGameData.gameType == NGT_PredatorTag || netGameData.gameType == NGT_AlienTag) {
					CheckSpeciesTagState();
				}

				CheckForPointBasedObjectRespawn();
			}
		}
	}
	else if (netGameData.myGameState == NGS_Leaving ||
	         netGameData.myGameState == NGS_Joining ||
	         netGameData.myGameState == NGS_StartUp)
	{
		// Once a second, as in play. A loading screen comes here every frame, and a joiner still loading can't take what that would pile up.
		static DWORD lastDescription = 0;

		if (AvP.Network == I_Host && timeGetTime() - lastDescription >= 1000) {
			lastDescription = timeGetTime();
			AddNetMsg_GameDescription();
			Net_CoopHostSays();
		}
	}

	{
		/* send our message buffer...
		NB it should always be non-empty, and always less than the maximum message size */
		bool clearSendBuffer = true;
		uintptr_t nBytes = (endSendBuffer - sendBuffer);

		if (netGameData.myGameState == NGS_EndGameScreen || netGameData.myGameState == NGS_Joining)
		{
			// there may not be any messages while showing the end game screen
			if (0 == nBytes) {
				return;
			}
		}

		LOCALASSERT(nBytes <= NET_MESSAGEBUFFERSIZE);

		if (!netGameData.skirmishMode)
		{
//			if (AvPNetID)
			{
				NetResult res = Net_Send(NET_ID_ALLPLAYERS, AvPNetID, 0, sendBuffer, nBytes);

				if (res != NET_OK)
				{
					// we have some problem sending...
					switch (res)
					{
						case NET_FAIL:
							NewOnScreenMessage("some problem sending");
							break;
						case NET_ERR_BUSY :

							/*
								failed to send this frame, try preserving the contents of the send buffer,
								unless it is getting to full.
							*/
							if (nBytes < NET_MESSAGEBUFFERSIZE / 2) {
								clearSendBuffer = false;
							}
							break;

						case NET_ERR_CONNECTIONLOST :
							NewOnScreenMessage("Connection lost!!");
							break;

						case NET_ERR_INVALIDPARAMS  :
							LOCALASSERT(0 == "Send - Invalid parameters");
							break;

						case NET_ERR_INVALIDPLAYER :
							LOCALASSERT(0 == "Send - Invalid player");
							break;

						case NET_ERR_NOTLOGGEDIN :
							LOCALASSERT(0 == "Send - Not logged in");
							break;

						case NET_ERR_SENDTOOBIG :
							LOCALASSERT(0 == "Send - Send to big");
							break;

						default :
							LOCALASSERT(0 == "Send - Unknown error");
							break;
					}
				}
			}
		}

		numMessagesTransmitted++;

		/* re-initialise the send message buffer */
		/*(unless the send failed because it was to busy)*/
		if (clearSendBuffer) {
			InitialiseSendMessageBuffer();
		}

		//      PrintDebuggingText("Bytes: %d\n",numBytes);
#if CalculateBytesSentPerSecond
		PrintDebuggingText("Bytes/Second: %d\n", GetBytesPerSecond(numBytes));
#endif
	}

	LogNetInfo("...Finished sending net message \n");
}

// NetSendMessages sends only so many times a second, and between those times it keeps what it has for a later call.
// Something that has to go out before this machine stops playing can't wait for that, because there may not be a later
// call. This sends it now. (Without it, the message moving everyone to the next level could sit here unsent until the
// level change threw it away, and the others would stay behind in the old level for good.)
static void NetSendMessagesNow(void)
{
	netGameData.sendTimer = 0;
	NetSendMessages();
}

static void InitialiseSendMessageBuffer(void)
{
	/* NB the receive buffer is maintained internally to garry's dp-extension */
	/* reset the end of send buffer pointer */
	endSendBuffer = &sendBuffer[0];
}


/*----------------------------------------------------------------------
this function examines the sb-list and adds messages for the player, local
objects, and other such things. NB It would be possible to do this in the
behaviour functions, but would involve a lag
  ----------------------------------------------------------------------*/
static void AddPlayerAndObjectUpdateMessages(void)
{
	/* don't bother adding these if we're finishing
	(host gets here even if finishing, but doesn't need to send these messages) */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* NB IF THE LIST OF OBJECT TYPES WHICH ARE UPDATED CHANGES, THERE MUST BE A
	CORESPONDING CHANGE IN THE LIST OF OBJECTS TESTED BY FindObjectFromNetIndex(),
	ELSE PLAYER WILL NOT BE ABLE TO RECEIVE MESSAGES ABOUT OWN OBJECTS OF THAT TYPE! */
#if EXTRAPOLATION_TEST
	AddNetMsg_FrameTimer();
#endif

	int sbIndex = 0;

	while (sbIndex < NumActiveStBlocks)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[sbIndex++];

		switch (sbPtr->I_SBtype)
		{
			case (I_BehaviourMarinePlayer):
			case (I_BehaviourAlienPlayer):
			case (I_BehaviourPredatorPlayer):
			{
				/* the player update message */
				AddNetMsg_PlayerState(sbPtr);
				break;
			}

			case (I_BehaviourRocket):
			case (I_BehaviourGrenade):
			case (I_BehaviourProximityGrenade):
			case (I_BehaviourPulseGrenade):
			case (I_BehaviourFragmentationGrenade):
			case (I_BehaviourPredatorEnergyBolt):
			case (I_BehaviourFrisbeeEnergyBolt):
			case (I_BehaviourClusterGrenade):
			case (I_BehaviourNPCPredatorDisc):
			case (I_BehaviourPredatorDisc_SeekTrack):
			case (I_BehaviourAlienSpit):
			case (I_BehaviourNetCorpse):
			case (I_BehaviourPPPlasmaBolt):
			case (I_BehaviourFrisbee):
				//          case(I_BehaviourSpeargunBolt): //spear location is sent once , upon creation
			{
				AddNetMsg_LocalObjectState(sbPtr);
				break;
			}

			case (I_BehaviourFlareGrenade):
			{
				FLARE_BEHAV_BLOCK *bbPtr = (FLARE_BEHAV_BLOCK *) sbPtr->SBdataptr;
				DYNAMICSBLOCK *dynPtr = sbPtr->DynPtr;

				//only send messages for flares while they are moving
				if (!dynPtr->IsStatic || bbPtr->becomeStuck) {
					AddNetMsg_LocalObjectState(sbPtr);
				}

				break;
			}

			case (I_BehaviourInanimateObject):
			{
				INANIMATEOBJECT_STATUSBLOCK *objStatPtr = static_cast<INANIMATEOBJECT_STATUSBLOCK *>(sbPtr->SBdataptr);

				if (objStatPtr->ghosted_object) {
					AddNetMsg_LocalObjectState(sbPtr);
				}

				break;
			}

			case (I_BehaviourAutoGun):
			{
				/* this MUST be a player placed autogun */
				/* the autogun update message */
				// In the mode that plays like single player it's a level's own, and it goes out with the other NPCs
				if (!SameAsSinglePlayer())
				{
					AddNetMsg_PlayerAutoGunState(sbPtr);
				}

				break;
			}

			case (I_BehaviourAlien):
			{
				//only add alien details , if it is near (to someone)
				if (sbPtr->SBdptr) {
					AddNetMsg_AlienAIState(sbPtr);
				}

				break;
			}

			default:
				break;
		}
	}

	AddNpcStateMessages();
}



/*----------------------------------------------------------------------
  End of network game clean up
  ----------------------------------------------------------------------*/
void EndAVPNetGame(void)
{
	SwitchSyncs_LevelIsOver();
	RemovePlayerFromGame(AvPNetID);
	TransmitPlayerLeavingNetMsg();

	if (!netGameData.skirmishMode) {
		Net_Disconnect();
	}

	/* reset our game mode here */
	AvP.Network = I_No_Network;
	AvP.NetworkAIServer = 0;

	// Or the next game hosted from the menus would still count as a campaign
	netGameData.coopCampaign = kCampaignNone;
	netGameData.coopCampaignLevel = 0;
	netGameData.coopKeepKit = 0;
	Campaign_ForgetKit();
	Coop_ForgetTheLobby();
	netGameData.skirmishMode = FALSE;
	TurnOffMultiplayerObserveMode();
	//reset time scale to default value
	TimeScale = ONE_FIXED;
}

/*----------------------------------------------------------------------
  Functions for adding each message type to the send message buffer
  ----------------------------------------------------------------------*/

// A message didn't fit in what's going out this frame, so it's left out. The log hears of it, a few times and then less often, with how
// many there have been, so a tester's log shows a game sending more than a frame holds. This used to be an assert, which only stopped a
// test build, but a busy game can fill a frame: it isn't a mistake in the code.
static void NoteSendBufferFull(int bytesWanted)
{
	static unsigned long long timesFull = 0;

	if (LogThisDrop(timesFull))
	{
		LogString("net: a message of " + std::to_string(bytesWanted) + " bytes didn't fit in this frame's " + std::to_string(NET_MESSAGEBUFFERSIZE) +
			" bytes, so it was left out (number " + std::to_string(timesFull) + " of these)");
	}
}
void AddNetMsg_GameDescription(void)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_GAMEDESCRIPTION *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_GAMEDESCRIPTION);

	/* some conditions */
	LOCALASSERT(AvP.Network == I_Host);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	// set up pointers to header and message structures
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_GAMEDESCRIPTION *)endSendBuffer;
	endSendBuffer += messageSize;
	
	// fill out the header
	headerPtr->type = (unsigned char)NetMT_GameDescription;

	// fill out the message
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			messagePtr->players[i].playerId = netGameData.playerData[i].playerId;
			messagePtr->players[i].characterType = (unsigned char)netGameData.playerData[i].characterType;
			messagePtr->players[i].characterSubType = (unsigned char)netGameData.playerData[i].characterSubType;
			messagePtr->players[i].startFlag = netGameData.playerData[i].startFlag;
			NetCopyPlayerName(messagePtr->players[i].name, netGameData.playerData[i].name, NET_PLAYERNAMELENGTH);
		}

		messagePtr->gameType = (unsigned char)netGameData.gameType;
		messagePtr->levelNumber = (unsigned char)netGameData.levelNumber;
		messagePtr->coopCampaign = (unsigned char)netGameData.coopCampaign;
		messagePtr->coopCampaignLevel = (unsigned char)netGameData.coopCampaignLevel;
		messagePtr->coopDifficulty = (unsigned char)netGameData.coopDifficulty;
		messagePtr->coopWaitingFor = netGameData.coopWaitingFor;
		messagePtr->coopRollNumber = netGameData.coopRollNumber;
		messagePtr->scoreLimit = netGameData.scoreLimit;
		messagePtr->timeLimit = (unsigned char)netGameData.timeLimit;
		messagePtr->invulnerableTime = (unsigned char)netGameData.invulnerableTime;

		for (int i = 0; i < 3; i++)
		{
			messagePtr->characterKillValues[i] = (unsigned char)netGameData.characterKillValues[i];
			messagePtr->aiKillValues[i] = (unsigned char)netGameData.aiKillValues[i];
		}

		messagePtr->baseKillValue = (unsigned char)netGameData.baseKillValue;
		messagePtr->useDynamicScoring = (unsigned char)netGameData.useDynamicScoring;
		messagePtr->useCharacterKillValues = (unsigned char)netGameData.useCharacterKillValues;
		messagePtr->sendDecals = netGameData.sendDecals;
		messagePtr->allowSmartgun = netGameData.allowSmartgun;
		messagePtr->allowFlamer = netGameData.allowFlamer;
		messagePtr->allowSadar = netGameData.allowSadar;
		messagePtr->allowGrenadeLauncher = netGameData.allowGrenadeLauncher;
		messagePtr->allowMinigun = netGameData.allowMinigun;
		messagePtr->allowDisc = netGameData.allowDisc;
		messagePtr->allowPistol = netGameData.allowPistol;
		messagePtr->allowPlasmaCaster = netGameData.allowPlasmaCaster;
		messagePtr->allowSpeargun = netGameData.allowSpeargun;
		messagePtr->allowMedicomp = netGameData.allowMedicomp;
		messagePtr->allowSmartDisc = netGameData.allowSmartDisc;
		messagePtr->allowPistols = netGameData.allowPistols;
		messagePtr->allowShotgun = netGameData.allowShotgun;
		messagePtr->allowRevolver = netGameData.allowRevolver;
		messagePtr->allowM79 = netGameData.allowM79;
		messagePtr->allowStaff = netGameData.allowStaff;
		messagePtr->maxPredator = netGameData.maxPredator;
		messagePtr->maxAlien = netGameData.maxAlien;
		messagePtr->maxMarine = netGameData.maxMarine;
		messagePtr->maxMarineGeneral = netGameData.maxMarineGeneral;
		messagePtr->maxMarinePulseRifle = netGameData.maxMarinePulseRifle;
		messagePtr->maxMarineSmartgun = netGameData.maxMarineSmartgun;
		messagePtr->maxMarineFlamer = netGameData.maxMarineFlamer;
		messagePtr->maxMarineSadar = netGameData.maxMarineSadar;
		messagePtr->maxMarineGrenade = netGameData.maxMarineGrenade;
		messagePtr->maxMarineMinigun = netGameData.maxMarineMinigun;
		messagePtr->maxMarineSmartDisc = netGameData.maxMarineSmartDisc;
		messagePtr->maxMarinePistols = netGameData.maxMarinePistols;
		messagePtr->maxMarineShotgun = netGameData.maxMarineShotgun;
		messagePtr->maxMarineRevolver = netGameData.maxMarineRevolver;
		messagePtr->maxMarineM79 = netGameData.maxMarineM79;
		messagePtr->useSharedLives = netGameData.useSharedLives;
		messagePtr->maxLives = netGameData.maxLives;
		messagePtr->numDeaths[0] = (unsigned char) min(netGameData.numDeaths[0], 255);
		messagePtr->numDeaths[1] = (unsigned char) min(netGameData.numDeaths[1], 255);
		messagePtr->numDeaths[2] = (unsigned char) min(netGameData.numDeaths[2], 255);
		messagePtr->timeForRespawn = (unsigned char)netGameData.timeForRespawn;
		messagePtr->pointsForRespawn = netGameData.pointsForRespawn;
		messagePtr->GameTimeElapsed = netGameData.GameTimeElapsed >> 16;
		messagePtr->gameSpeed = netGameData.gameSpeed;
		messagePtr->preDestroyLights = netGameData.preDestroyLights;
		messagePtr->disableFriendlyFire = netGameData.disableFriendlyFire;
		messagePtr->fallingDamage = netGameData.fallingDamage;
		messagePtr->pistolInfiniteAmmo = netGameData.pistolInfiniteAmmo;
		messagePtr->specialistPistols = netGameData.specialistPistols;

		if (netGameData.myGameState == NGS_EndGameScreen) {
			messagePtr->endGame = 1;
		}
		else {
			messagePtr->endGame = 0;
		}
	}
}

void AddNetMsg_PlayerDescription(void)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLAYERDESCRIPTION *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLAYERDESCRIPTION);
	/* some conditions */
	LOCALASSERT(AvP.Network == I_Peer);
	LOCALASSERT(netGameData.myGameState == NGS_Joining);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLAYERDESCRIPTION *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PlayerDescription;
	/*fill out the message */
	{
		messagePtr->characterType = (unsigned char)netGameData.myCharacterType;
		messagePtr->characterSubType = (unsigned char)netGameData.myCharacterSubType;
		messagePtr->startFlag = (unsigned char)netGameData.myStartFlag;
	}
}

void AddNetMsg_StartGame(void)
{
	NETMESSAGEHEADER *headerPtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	/* some conditions */
	LOCALASSERT(AvP.Network == I_Host);
	LOCALASSERT(netGameData.myGameState == NGS_Joining);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_StartGame;
}

extern int UseExtrapolation;
// Whether others should see this player's shoulder lamp: only in the mode that plays as single player, only a live marine, and only if it's on
static bool ThisPlayersLampShows()
{
	PLAYER_STATUS const *const status = Player && Player->ObStrategyBlock ? (PLAYER_STATUS const *)Player->ObStrategyBlock->SBdataptr : 0;

	return SameAsSinglePlayer() && AvP.PlayerType == I_Marine && status && status->IsAlive && ShoulderLamp_IsOn();
}

void AddNetMsg_PlayerState(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLAYERSTATE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLAYERSTATE);
	int playerIndex;

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// A host decides the list, and this game's place in it is used below. Not on it means nothing to say.
	if (PlayerIdInPlayerList(AvPNetID) == NET_IDNOTINPLAYERLIST) {
		Net_NoteRefused("this game isn't on the list the host sent");
		return;
	}

#if EXTRAPOLATION_TEST

	if (UseExtrapolation && netGameData.sendFrequency)
	{
		//see if we can get away with sending reduced information about the player's state
		static VECTORCH previousVelocity;
		static EULER previousOrient;
		static int previousWeapon;
		static unsigned int TimeOfLastPlayerState;
		BOOL sendMinimalState = TRUE;

		if (TimeCounterForExtrapolation < TimeOfLastPlayerState ||
		    TimeCounterForExtrapolation - TimeOfLastPlayerState > ONE_FIXED / 4)
		{
			//It has been over 1/4 second since the last full update , so better do a full update now
			sendMinimalState = FALSE;
		}

		if (sendMinimalState)
		{
			if (sbPtr->DynPtr->LinImpulse.vx ||
			    sbPtr->DynPtr->LinImpulse.vy ||
			    sbPtr->DynPtr->LinImpulse.vz)
			{
				/*
				The player is probably jumping. This screws up the extrapolation somewhat.
				Therefore better send full player update.
				*/
				sendMinimalState = FALSE;
			}
		}

		if (sendMinimalState)
		{
			VECTORCH diff = sbPtr->DynPtr->LinVelocity;
			SubVector(&previousVelocity, &diff);

			if (Approximate3dMagnitude(&diff) > 100)
			{
				//the player's velocity has changed , so need a full update.
				sendMinimalState = FALSE;
			}
		}

		if (sendMinimalState)
		{
			PLAYER_WEAPON_DATA *weaponPtr;
			PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
			LOCALASSERT(playerStatusPtr);
			weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);

			if (weaponPtr->WeaponIDNumber != previousWeapon)
			{
				//the player's weapon has changed ,so need a full update
				previousWeapon = weaponPtr->WeaponIDNumber;
				sendMinimalState = FALSE;
			}
		}

		// A lamp switched on or off goes at once, in a full update, because only that has its bit
		static bool lampShowedLastTime = false;

		if (ThisPlayersLampShows() != lampShowedLastTime)
		{
			lampShowedLastTime = !lampShowedLastTime;
			sendMinimalState = FALSE;
		}

		if (sendMinimalState)
		{
			//don't need to send positional information , but has the player's orientation changed?
			BOOL sendOrient = FALSE;

			if (previousOrient.EulerX != sbPtr->DynPtr->OrientEuler.EulerX ||
			    previousOrient.EulerY != sbPtr->DynPtr->OrientEuler.EulerY ||
			    previousOrient.EulerZ != sbPtr->DynPtr->OrientEuler.EulerZ)
			{
				previousOrient = sbPtr->DynPtr->OrientEuler;
				//we better send the medium sized player state message
				sendOrient = TRUE;
			}

			AddNetMsg_PlayerState_Minimal(sbPtr, sendOrient);
			return;
		}
		else
		{
			previousVelocity = sbPtr->DynPtr->LinVelocity;
			previousOrient = sbPtr->DynPtr->OrientEuler;
			TimeOfLastPlayerState = TimeCounterForExtrapolation;
		}
	}

#endif
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLAYERSTATE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PlayerState;
	//check our carrent character type
	{
		switch (AvP.PlayerType)
		{
			case I_Marine :
				netGameData.myCharacterType = NGCT_Marine;
				break;

			case I_Predator :
				netGameData.myCharacterType = NGCT_Predator;
				break;

			case I_Alien :
				netGameData.myCharacterType = NGCT_Alien;
				break;
		}

		playerIndex = PlayerIdInPlayerList(AvPNetID);
		GLOBALASSERT(playerIndex != NET_IDNOTINPLAYERLIST);
		messagePtr->characterType = netGameData.myCharacterType;
		messagePtr->characterSubType = netGameData.myCharacterSubType;
		messagePtr->nextCharacterType = netGameData.myNextCharacterType;
		netGameData.playerData[playerIndex].characterType = static_cast<NETGAME_CHARACTERTYPE>(messagePtr->nextCharacterType);
	}
	/* fill out our position and orientation */
	{
		DYNAMICSBLOCK *dynPtr = sbPtr->DynPtr;
		LOCALASSERT(dynPtr);
		LOCALASSERT((dynPtr->OrientEuler.EulerX >= 0) && (dynPtr->OrientEuler.EulerX < 4096));  /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerY >= 0) && (dynPtr->OrientEuler.EulerY < 4096));  /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerZ >= 0) && (dynPtr->OrientEuler.EulerZ < 4096));  /* 9 bits of signed data */

		/* NB we can fit +-4194303 into 23 bits */
		if (dynPtr->Position.vx < -4100000) {
			messagePtr->xPos = -4100000;
		}
		else if (dynPtr->Position.vx > 4100000) {
			messagePtr->xPos = 4100000;
		}
		else {
			messagePtr->xPos = dynPtr->Position.vx;
		}

		messagePtr->xOrient = (dynPtr->OrientEuler.EulerX >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vy < -4100000) {
			messagePtr->yPos = -4100000;
		}
		else if (dynPtr->Position.vy > 4100000) {
			messagePtr->yPos = 4100000;
		}
		else {
			messagePtr->yPos = dynPtr->Position.vy;
		}

		messagePtr->yOrient = (dynPtr->OrientEuler.EulerY >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vz < -4100000) {
			messagePtr->zPos = -4100000;
		}
		else if (dynPtr->Position.vz > 4100000) {
			messagePtr->zPos = 4100000;
		}
		else {
			messagePtr->zPos = dynPtr->Position.vz;
		}

		messagePtr->zOrient = (dynPtr->OrientEuler.EulerZ >> NET_EULERSCALESHIFT);
#if EXTRAPOLATION_TEST
		messagePtr->velocity_x = dynPtr->LinVelocity.vx / 100;
		messagePtr->velocity_y = dynPtr->LinVelocity.vy / 100;
		messagePtr->velocity_z = dynPtr->LinVelocity.vz / 100;
		messagePtr->standard_gravity = dynPtr->UseStandardGravity;
#endif
	}
	/* KJL 17:04:22 26/01/98 - elevation (for weapon, etc.) */
	{
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		messagePtr->Elevation = playerStatusPtr->ViewPanX;

		switch (AvP.PlayerType)
		{
			case (I_Marine):
			{
				messagePtr->sequence = (unsigned char)GetMyMarineSequence();

				/* Taunt handling. */
				if (playerStatusPtr->tauntTimer == -1)
				{
					if (messagePtr->sequence != MSQ_Taunt)
					{
						/* Taunt overridden. */
						playerStatusPtr->tauntTimer = 0;
					}
					else
					{
						/* Taunt is go. */
						playerStatusPtr->tauntTimer = TAUNT_LENGTH;
					}
				}
				else if (playerStatusPtr->tauntTimer != 0)
				{
					if (messagePtr->sequence != MSQ_Taunt)
					{
						/* Taunt cancelled. */
						playerStatusPtr->tauntTimer = 0;
					}
				}

				break;
			}

			case (I_Predator):
			{
				messagePtr->sequence = (unsigned char)GetMyPredatorSequence();

				/* Taunt handling. */
				if (playerStatusPtr->tauntTimer == -1)
				{
					if (messagePtr->sequence != PredSQ_Taunt)
					{
						/* Taunt overridden. */
						playerStatusPtr->tauntTimer = 0;
					}
					else
					{
						/* Taunt is go. */
						playerStatusPtr->tauntTimer = TAUNT_LENGTH;
					}
				}
				else if (playerStatusPtr->tauntTimer != 0)
				{
					if (messagePtr->sequence != PredSQ_Taunt)
					{
						/* Taunt cancelled. */
						playerStatusPtr->tauntTimer = 0;
					}
				}

				break;
			}

			case (I_Alien):
			{
				messagePtr->sequence = (unsigned char)GetMyAlienSequence();

				/* Taunt handling. */
				if (playerStatusPtr->tauntTimer == -1)
				{
					if (messagePtr->sequence != ASQ_Taunt)
					{
						/* Taunt overridden. */
						playerStatusPtr->tauntTimer = 0;
					}
					else
					{
						/* Taunt is go. */
						playerStatusPtr->tauntTimer = TAUNT_LENGTH;
					}
				}
				else if (playerStatusPtr->tauntTimer != 0)
				{
					if (messagePtr->sequence != ASQ_Taunt)
					{
						/* Taunt cancelled. */
						playerStatusPtr->tauntTimer = 0;
					}
				}

				break;
			}

			default:
			{
				LOCALASSERT(1 == 0);
				break;
			}
		}

		if (PlayerStatusPtr->ShapeState != PMph_Standing) {
			messagePtr->IAmCrouched = 1;
		}
		else {
			messagePtr->IAmCrouched = 0;
		}
	}
	/* my current weapon id, and whether I am firing it... */
	{
		PLAYER_WEAPON_DATA *weaponPtr;
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		LOCALASSERT(playerStatusPtr);
		weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
		messagePtr->currentWeapon = (signed char)(weaponPtr->WeaponIDNumber);

		if (weaponPtr->WeaponIDNumber == WEAPON_TWO_PISTOLS)
		{
			if (((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY) || (weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY))
			    && (PlayerStatusPtr->IsAlive))
			{
				if (LastHand)
				{
					messagePtr->IAmFiringPrimary = 0;
					messagePtr->IAmFiringSecondary = 1;
				}
				else
				{
					messagePtr->IAmFiringPrimary = 1;
					messagePtr->IAmFiringSecondary = 0;
				}
			}
			else
			{
				messagePtr->IAmFiringPrimary = 0;
				messagePtr->IAmFiringSecondary = 0;
			}

			/* Whether in tertiary fire */
			if (AreTwoPistolsInTertiaryFire()) {
				messagePtr->Special = 1;
			} else {
				messagePtr->Special = 0;
			}

			/* whether or not I'm displaying a gun flash */
			messagePtr->IHaveAMuzzleFlash = MyPlayerHasAMuzzleFlash(sbPtr);
		}
		else
		{
			if ((weaponPtr->WeaponIDNumber == WEAPON_MARINE_PISTOL)
			    && ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY) || (weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY))
			   )
			{
				messagePtr->Special = 1;
			} else {
				messagePtr->Special = 0;
			}

			if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY) && (PlayerStatusPtr->IsAlive)) {
				messagePtr->IAmFiringPrimary = 1;
			} else {
				messagePtr->IAmFiringPrimary = 0;
			}

			if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY) && (PlayerStatusPtr->IsAlive)) {
				messagePtr->IAmFiringSecondary = 1;
			} else {
				messagePtr->IAmFiringSecondary = 0;
			}

			/* whether or not I'm displaying a gun flash */
			if (MyPlayerHasAMuzzleFlash(sbPtr)) {
				messagePtr->IHaveAMuzzleFlash = 1;
			} else {
				messagePtr->IHaveAMuzzleFlash = 0;
			}
		}
	}

	/* whether or not I'm alive */
	if (PlayerStatusPtr->IsAlive) {
		messagePtr->IAmAlive = 1;
	} else {
		messagePtr->IAmAlive = 0;
	}

	netGameData.playerData[playerIndex].playerAlive = messagePtr->IAmAlive;

	//Is the player alive or in possesion of extra lives?
	if (messagePtr->IAmAlive) {
		messagePtr->IHaveLifeLeft = 1;
	} else {
		messagePtr->IHaveLifeLeft = AreThereAnyLivesLeft();
	}

	netGameData.playerData[playerIndex].playerHasLives = messagePtr->IHaveLifeLeft;

	/*Am I currently invulnerable?*/
	if (PlayerStatusPtr->invulnerabilityTimer > 0) {
		messagePtr->IAmInvulnerable = 1;
	} else {
		messagePtr->IAmInvulnerable = 0;
	}

	/* whether or not I'm the host */
	//  if(AvP.Network==I_Host) messagePtr->IAmHost = 1;
	//  else messagePtr->IAmHost = 0;
	/* whether or not I'm a cloaked predator */
	{
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		LOCALASSERT(playerStatusPtr);

		if (playerStatusPtr->cloakOn)
		{
			LOCALASSERT(AvP.PlayerType == I_Predator);

			if (playerStatusPtr->CloakingEffectiveness > 65535) {
				messagePtr->CloakingEffectiveness = 65535;
			} else {
				messagePtr->CloakingEffectiveness = (unsigned short)(playerStatusPtr->CloakingEffectiveness);
			}
		} else {
			messagePtr->CloakingEffectiveness = 0;
		}
	}

	/* Am I on fire? */
	if (sbPtr->SBDamageBlock.IsOnFire) {
		messagePtr->IAmOnFire = 1;
	} else {
		messagePtr->IAmOnFire = 0;
	}

	/* Do I have a disk?  Probably not. */
	messagePtr->IHaveADisk = 0;

	if (AvP.PlayerType == I_Predator)
	{
		if (messagePtr->currentWeapon == WEAPON_PRED_DISC)
		{
			if (PlayersWeapon.HModelControlBlock)
			{
				SECTION_DATA *disc_section;
				disc_section = GetThisSectionData(PlayersWeapon.HModelControlBlock->section_data, "disk");

				if (disc_section && (disc_section->flags & section_data_notreal) == 0) {
					messagePtr->IHaveADisk = 1;
				}
			}
		}
	}

	//have we been screaming?
	if (netGameData.myLastScream != -1) {
		messagePtr->scream = netGameData.myLastScream;
	} else {
		messagePtr->scream = 31;
	}

	//reset last scream , so we don't keep sending it
	netGameData.myLastScream = -1;
	/* CDF 21/4/99 Add landing noise? */
	messagePtr->landingNoise = netGameData.landingNoise;
	//reset that too!
	netGameData.landingNoise = 0;

	// Always written, because this part of the message isn't cleared first: a bit left out would send whatever was in the buffer
	messagePtr->lampOn = ThisPlayersLampShows() ? 1 : 0;
}

void AddNetMsg_PlayerState_Minimal(STRATEGYBLOCK *sbPtr, BOOL sendOrient)
{
	PLAYER_WEAPON_DATA *weaponPtr = 0;
	int playerIndex;
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLAYERSTATE_MINIMAL *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLAYERSTATE_MINIMAL);

	if (sendOrient) {
		messageSize = sizeof(NETMESSAGE_PLAYERSTATE_MEDIUM);
	}

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if (PlayerIdInPlayerList(AvPNetID) == NET_IDNOTINPLAYERLIST) {
		Net_NoteRefused("this game isn't on the list the host sent");
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLAYERSTATE_MINIMAL *)endSendBuffer;
	endSendBuffer += messageSize;

	/* fill out the header */
	if (sendOrient) {
		headerPtr->type = (unsigned char)NetMT_PlayerState_Medium;
	}
	else {
		headerPtr->type = (unsigned char)NetMT_PlayerState_Minimal;
	}

	playerIndex = PlayerIdInPlayerList(AvPNetID);
	GLOBALASSERT(playerIndex != NET_IDNOTINPLAYERLIST);
	/* KJL 17:04:22 26/01/98 - elevation (for weapon, etc.) */
	{
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		messagePtr->Elevation = playerStatusPtr->ViewPanX;
	}
	/* my current weapon id, and whether I am firing it... */
	{
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		LOCALASSERT(playerStatusPtr);
		weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);

		if (weaponPtr->WeaponIDNumber == WEAPON_TWO_PISTOLS)
		{
			if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY) && (PlayerStatusPtr->IsAlive))
			{
				if (LastHand)
				{
					messagePtr->IAmFiringPrimary = 0;
					messagePtr->IAmFiringSecondary = 1;
				}
				else
				{
					messagePtr->IAmFiringPrimary = 1;
					messagePtr->IAmFiringSecondary = 0;
				}
			}
			else
			{
				messagePtr->IAmFiringPrimary = 0;
				messagePtr->IAmFiringSecondary = 0;
			}

			/* whether or not I'm displaying a gun flash */
			messagePtr->IHaveAMuzzleFlash = MyPlayerHasAMuzzleFlash(sbPtr);
		}
		else
		{
			if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY) && (PlayerStatusPtr->IsAlive)) {
				messagePtr->IAmFiringPrimary = 1;
			} else {
				messagePtr->IAmFiringPrimary = 0;
			}

			if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY) && (PlayerStatusPtr->IsAlive)) {
				messagePtr->IAmFiringSecondary = 1;
			} else {
				messagePtr->IAmFiringSecondary = 0;
			}

			/* whether or not I'm displaying a gun flash */
			if (MyPlayerHasAMuzzleFlash(sbPtr)) {
				messagePtr->IHaveAMuzzleFlash = 1;
			} else {
				messagePtr->IHaveAMuzzleFlash = 0;
			}
		}
	}

	/* whether or not I'm alive */
	if (PlayerStatusPtr->IsAlive) {
		messagePtr->IAmAlive = 1;
	} else {
		messagePtr->IAmAlive = 0;
	}

	netGameData.playerData[playerIndex].playerAlive = (unsigned char)(messagePtr->IAmAlive);

	//Is the player alive or in possesion of extra lives?
	if (messagePtr->IAmAlive) {
		messagePtr->IHaveLifeLeft = 1;
	} else {
		messagePtr->IHaveLifeLeft = AreThereAnyLivesLeft();
	}

	netGameData.playerData[playerIndex].playerHasLives = messagePtr->IHaveLifeLeft;
	/* whether or not I'm a cloaked predator */
	{
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		LOCALASSERT(playerStatusPtr);

		if (playerStatusPtr->cloakOn)
		{
			LOCALASSERT(AvP.PlayerType == I_Predator);

			if (playerStatusPtr->CloakingEffectiveness > 65535) {
				messagePtr->CloakingEffectiveness = 65535 >> 8;
			} else {
				messagePtr->CloakingEffectiveness = (unsigned short)(playerStatusPtr->CloakingEffectiveness >> 8);
			}
		} else {
			messagePtr->CloakingEffectiveness = 0;
		}
	}

	/* Am I on fire? */
	if (sbPtr->SBDamageBlock.IsOnFire) {
		messagePtr->IAmOnFire = 1;
	} else {
		messagePtr->IAmOnFire = 0;
	}

	/* Do I have a disk?  Probably not. */
	messagePtr->IHaveADisk = 0;

	if (AvP.PlayerType == I_Predator)
	{
		if (weaponPtr->WeaponIDNumber == WEAPON_PRED_DISC)
		{
			if (PlayersWeapon.HModelControlBlock)
			{
				SECTION_DATA *disc_section;
				disc_section = GetThisSectionData(PlayersWeapon.HModelControlBlock->section_data, "disk");

				if (disc_section && (disc_section->flags & section_data_notreal) == 0) {
					messagePtr->IHaveADisk = 1;
				}
			}
		}
	}

	if (sendOrient)
	{
		NETMESSAGE_PLAYERSTATE_MEDIUM *mediumMessage = (NETMESSAGE_PLAYERSTATE_MEDIUM *) messagePtr;
		mediumMessage->xOrient = (sbPtr->DynPtr->OrientEuler.EulerX >> NET_EULERSCALESHIFT);
		mediumMessage->yOrient = (sbPtr->DynPtr->OrientEuler.EulerY >> NET_EULERSCALESHIFT);
		mediumMessage->zOrient = (sbPtr->DynPtr->OrientEuler.EulerZ >> NET_EULERSCALESHIFT);
	}
}

void AddNetMsg_FrameTimer()
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_FRAMETIMER *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_FRAMETIMER);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_FRAMETIMER *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_FrameTimer;
	/* fill out the message */
	messagePtr->frame_time = (unsigned short)GameTimeSinceLastSend;
	GameTimeSinceLastSend = 0;
}

/* support function for addnetmsg_playerstate() */
static int MyPlayerHasAMuzzleFlash(STRATEGYBLOCK *sbPtr)
{
	PLAYER_WEAPON_DATA *weaponPtr;
	TEMPLATE_WEAPON_DATA *twPtr;
	PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
	LOCALASSERT(playerStatusPtr);
	weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
	twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];

	/* first check if we are displaying a muzle flash ourselves */
	if (twPtr->MuzzleFlashShapeName == NULL) {
		return 0;
	}

	if (twPtr->PrimaryIsMeleeWeapon) {
		return 0;
	}

	if (weaponPtr->WeaponIDNumber == WEAPON_TWO_PISTOLS)
	{
		if (weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY)
		{
			if (LastHand) {
				return 2;
			} else {
				return 1;
			}
		}
		else if (weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY)
		{
			if (LastHand) {
				return 2;
			} else {
				return 1;
			}

			return 0;
		}
	}

	if (weaponPtr->WeaponIDNumber == WEAPON_MARINE_PISTOL)
	{
		if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY)
		    || (weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY))
		{	
			return 1; //ReleasePrintDebuggingText("Pistol Muzzle Flash 1\n");
		} else {	
			return 0; //ReleasePrintDebuggingText("Pistol Muzzle Flash 0\n");
		}
	}

	if (weaponPtr->CurrentState != WEAPONSTATE_FIRING_PRIMARY) {
		return 0;
	}

	/* even if we are displaying our own muzzle flash, we don't neccessarily want it to
	be visible to other players (because it looks stupid) */
	if ((weaponPtr->WeaponIDNumber == WEAPON_PULSERIFLE) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_MARINE_PISTOL) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_AUTOSHOTGUN) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_SMARTGUN) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_MINIGUN) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_FRISBEE_LAUNCHER) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_SONICCANNON) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_BEAMCANNON) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_PRED_PISTOL) ||
	    (weaponPtr->WeaponIDNumber == WEAPON_PRED_RIFLE))
	{
		/* if we get this far, we want to display a muzzle flash */
		return 1;
	}

	return 0;
}

char GetWeaponIconFromDamage(DAMAGE_PROFILE *damage)
{
#define ICON_CUDGEL 177
#define ICON_PULSERIFLE 177
#define ICON_PULSERIFLE_GRENADE 177
#define ICON_SMARTGUN 138
#define ICON_FLAMER 137
#define ICON_SADAR 140
#define ICON_GRENADE_STANDARD 139
#define ICON_GRENADE_PROX 139
#define ICON_GRENADE_FRAG 139
#define ICON_MINIGUN 141
#define ICON_SKEETER 143
#define ICON_MARINE_PISTOL 142
#define ICON_CLAW 152
#define ICON_TAIL 151
#define ICON_JAWS 176
#define ICON_WRISTBLADE 154
#define ICON_PRED_PISTOL 158
#define ICON_SHOULDERCANNON 156
#define ICON_SPEARGUN 155
#define ICON_DISC 157

	if (!damage) {
		return 0;
	}

	switch (damage->Id)
	{
		case AMMO_10MM_CULW :
			return ICON_PULSERIFLE;

		case AMMO_PULSE_GRENADE :
		case AMMO_PULSE_GRENADE_STRIKE :
			return ICON_PULSERIFLE_GRENADE;

		case AMMO_SMARTGUN :
			return ICON_SMARTGUN;

		case AMMO_FLAMETHROWER :
		case AMMO_FIREDAMAGE_POSTMAX :
			return ICON_FLAMER;

		case AMMO_SADAR_TOW :
		case AMMO_SADAR_BLAST :
			return ICON_SADAR;

		// M5's guns. There's no picture for a shotgun, so a kill by one says nothing here, as the game does for what it has no picture of.
		case AMMO_M79_STANDARD :
		case AMMO_M79_FIRE :
		case AMMO_M79_FRAG :
		case AMMO_M79_DIRECT_HIT :
		case AMMO_M79_FIRE_DIRECT_HIT :
			return ICON_GRENADE_STANDARD;

		case AMMO_REVOLVER :
			return ICON_MARINE_PISTOL;

		case AMMO_PRED_STAFF :
			return ICON_WRISTBLADE;

		case AMMO_GRENADE :
			return ICON_GRENADE_STANDARD;

		case AMMO_FRAGMENTATION_GRENADE :
		case AMMO_FLECHETTE_POSTMAX :
			return ICON_GRENADE_FRAG;

		case AMMO_PROXIMITY_GRENADE :
			return ICON_GRENADE_PROX;

		case AMMO_MINIGUN :
			return ICON_MINIGUN ;

		case AMMO_MARINE_PISTOL :
		case AMMO_MARINE_PISTOL_PC :
			return ICON_MARINE_PISTOL;

		case AMMO_CUDGEL :
			return ICON_CUDGEL;

		case AMMO_FRISBEE :
		case AMMO_FRISBEE_BLAST :
		case AMMO_FRISBEE_FIRE :
			return ICON_SKEETER;

		case AMMO_PRED_WRISTBLADE :
		case AMMO_HEAVY_PRED_WRISTBLADE :
		case AMMO_PRED_TROPHY_KILLSECTION :
			return ICON_WRISTBLADE;

		case AMMO_PRED_PISTOL :
			return ICON_PRED_PISTOL;

		case AMMO_PRED_RIFLE :
			return ICON_SPEARGUN;

		case AMMO_PRED_ENERGY_BOLT :
		case AMMO_PLASMACASTER_NPCKILL :
		case AMMO_PLASMACASTER_PCKILL :
			return ICON_SHOULDERCANNON;

		case AMMO_PRED_DISC :
			return ICON_DISC;

		case AMMO_PREDPISTOL_STRIKE :
			return ICON_PRED_PISTOL;

		case AMMO_ALIEN_CLAW :
			return ICON_CLAW;

		case AMMO_ALIEN_TAIL :
			return ICON_TAIL;

		case AMMO_ALIEN_BITE_KILLSECTION :
		case AMMO_PC_ALIEN_BITE :
		case AMMO_ALIEN_BITE_KILLSECTION_SUPER :
			return ICON_JAWS;
	}

	return 0;
}

void AddNetMsg_PlayerKilled(int objectId, DAMAGE_PROFILE *damage)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLAYERKILLED *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLAYERKILLED);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* definitely should be actually dead */
	LOCALASSERT(PlayerStatusPtr->IsAlive == 0);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	// set up pointers to header and message structures
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLAYERKILLED *)endSendBuffer;
	endSendBuffer += messageSize;
	
	// fill out the header
	headerPtr->type = (unsigned char)NetMT_PlayerKilled;
	
	/* fill out the message: myNetworkkillerId should either be NULL indicating that player
	has killed himself, or the NetID of the killer (which may in fact be the player's NetID)*/
	messagePtr->objectId = objectId; /* ID of the new corpse. */
	messagePtr->killerId = myNetworkKillerId;
	messagePtr->myType = netGameData.myCharacterType;

	if (myNetworkKillerId)
	{
		int killer_index = PlayerIdInPlayerList(myNetworkKillerId);

		if (killer_index != NET_IDNOTINPLAYERLIST) {
			messagePtr->killerType = netGameData.playerData[killer_index].characterType;
		}
		else {
			// the player doing the damage has either left the game , or never existed.
			// call it suicide then.
			myNetworkKillerId = AvPNetID;
			messagePtr->killerId = 0;
		}
	}

	if (!myNetworkKillerId || myNetworkKillerId == AvPNetID)
	{
		// suicide (or killed by alien , in which case this will be corrected a few lines later)
		messagePtr->killerType = messagePtr->myType;
	}

	// find the icon for the weapon used
	messagePtr->weaponIcon = GetWeaponIconFromDamage(damage);

	/*look at the damage type to see if the damage was done by an ai alien*/
	if (damage)
	{
		switch (damage->Id)
		{
			case AMMO_NPC_ALIEN_CLAW :
			case AMMO_NPC_ALIEN_TAIL :
			case AMMO_NPC_ALIEN_BITE :
				messagePtr->killerType = NGCT_AI_Alien;
				break;

			case AMMO_NPC_PREDALIEN_CLAW :
			case AMMO_NPC_PREDALIEN_BITE :
			case AMMO_NPC_PREDALIEN_TAIL :
				messagePtr->killerType = NGCT_AI_Predalien;
				break;

			case AMMO_NPC_PRAETORIAN_CLAW :
			case AMMO_NPC_PRAETORIAN_BITE :
			case AMMO_NPC_PRAETORIAN_TAIL :
				messagePtr->killerType = NGCT_AI_Praetorian;
				break;
		}
	}

	Inform_PlayerHasDied(myNetworkKillerId, AvPNetID, messagePtr->killerType, messagePtr->weaponIcon);

	if (AvP.Network == I_Host) {
		DoNetScoresForHostDeath(messagePtr->myType, messagePtr->killerType);
	}
}

void AddNetMsg_PlayerDeathAnim(int deathId, int objectId)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_CORPSEDEATHANIM *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_CORPSEDEATHANIM);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing)
	{
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_CORPSEDEATHANIM *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_CorpseDeathAnim;
	messagePtr->objectId = objectId; /* ID of the new corpse. */
	messagePtr->deathId = deathId;
}

void AddNetMsg_PlayerLeaving(void)
{
	NETMESSAGEHEADER *headerPtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	/* some conditions */
	//LOCALASSERT(AvP.Network==I_Peer);
	/* yes: need to send this before changing state, as we need to know our previous state,
	and sendMessage requires one of these two states anyway */
	LOCALASSERT((netGameData.myGameState == NGS_StartUp) || (netGameData.myGameState == NGS_Playing) || (netGameData.myGameState == NGS_Joining) || (netGameData.myGameState == NGS_EndGame) || (netGameData.myGameState == NGS_EndGameScreen));
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PlayerLeaving;
}

void AddNetMsg_AllGameScores(void)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_ALLGAMESCORES *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_ALLGAMESCORES);
	/* should be sent by host only, whilst in end game */
	LOCALASSERT(AvP.Network == I_Host);
	LOCALASSERT((netGameData.myGameState == NGS_Playing) || (netGameData.myGameState == NGS_EndGameScreen));
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_ALLGAMESCORES *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_AllGameScores;
	/*fill out the message */
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			for (int j = 0; j < NET_MAXPLAYERS; j++) {
				messagePtr->playerFrags[i][j] = netGameData.playerData[i].playerFrags[j];
			}

			messagePtr->playerScores[i] = netGameData.playerData[i].playerScore;
			messagePtr->playerScoresAgainst[i] = netGameData.playerData[i].playerScoreAgainst;

			for (int j = 0; j < 3; j++) {
				messagePtr->aliensKilled[i][j] = netGameData.playerData[i].aliensKilled[j];
			}

			messagePtr->deathsFromAI[i] = netGameData.playerData[i].deathsFromAI;
		}
	}
}

void AddNetMsg_PlayerScores(int playerId)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLAYERSCORES *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLAYERSCORES);
	/* should be sent by host only, whilst playing game */
	LOCALASSERT(AvP.Network == I_Host);
	LOCALASSERT((netGameData.myGameState == NGS_Playing));
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLAYERSCORES *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PlayerScores;
	/*fill out the message */
	messagePtr->playerId = (unsigned char)playerId;
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++) {
			messagePtr->playerFrags[i] = netGameData.playerData[playerId].playerFrags[i];
		}

		messagePtr->playerScore = netGameData.playerData[playerId].playerScore;
		messagePtr->playerScoreAgainst = netGameData.playerData[playerId].playerScoreAgainst;

		for (int i = 0; i < 3; i++) {
			messagePtr->aliensKilled[i] = netGameData.playerData[playerId].aliensKilled[i];
		}

		messagePtr->deathsFromAI = netGameData.playerData[playerId].deathsFromAI;
	}
}

void AddNetMsg_ScoreChange(int killerIndex, int victimIndex)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_SCORECHANGE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_SCORECHANGE);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_SCORECHANGE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_ScoreChange;
	/* Fill in message. */
	messagePtr->killerIndex = (unsigned char) killerIndex;
	messagePtr->victimIndex = (unsigned char) victimIndex;

	if (killerIndex == NET_MAXPLAYERS)
	{
		//killed by ai
		messagePtr->fragCount = netGameData.playerData[victimIndex].deathsFromAI;
	}
	else
	{
		//killed by a player
		messagePtr->fragCount = netGameData.playerData[killerIndex].playerFrags[victimIndex];
		messagePtr->killerScoreFor = netGameData.playerData[killerIndex].playerScore;
	}

	messagePtr->victimScoreAgainst = netGameData.playerData[victimIndex].playerScoreAgainst;
}

void AddNetMsg_SpeciesScores()
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_SPECIESSCORES *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_SPECIESSCORES);

	/* check there's enough room in the send buffer */
	int numBytesReqd = headerSize + messageSize;
	int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if (numBytesReqd > numBytesLeft)
	{
		NoteSendBufferFull(numBytesReqd);
		/* don't add it */
		return;
	}

	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_SPECIESSCORES *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_SpeciesScores;

	/* Fill in message. */
	for (int i = 0; i < 3; i++) {
		messagePtr->teamScores[i] = netGameData.teamScores[i];
	}
}

/* for sending information about bullet ricochets and plasma impacts */
void AddNetMsg_LocalRicochet(AVP_BEHAVIOUR_TYPE behaviourType, VECTORCH *position, VECTORCH *direction)
{
}

void AddNetMsg_LocalObjectState(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LOBSTATE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LOBSTATE);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LOBSTATE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LocalObjectState;
	/* fill out the message */
	{
		DYNAMICSBLOCK *dynPtr = sbPtr->DynPtr;
		LOCALASSERT(dynPtr);
		//      LOCALASSERT((dynPtr->Position.vx < 4194304)&&(dynPtr->Position.vx > -4194304)); /* 23 bits of data */
		//      LOCALASSERT((dynPtr->Position.vy < 4194304)&&(dynPtr->Position.vy > -4194304)); /* 23 bits of data */
		//      LOCALASSERT((dynPtr->Position.vz < 4194304)&&(dynPtr->Position.vz > -4194304)); /* 23 bits of data */
		LOCALASSERT((dynPtr->OrientEuler.EulerX >= 0) && (dynPtr->OrientEuler.EulerX < 4096)); /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerY >= 0) && (dynPtr->OrientEuler.EulerY < 4096)); /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerZ >= 0) && (dynPtr->OrientEuler.EulerZ < 4096));  /* 9 bits of signed data */

		/* NB we can fit +-4194303 into 23 bits */
		if (dynPtr->Position.vx < -4100000) {
			messagePtr->xPos = -4100000;
		}
		else if (dynPtr->Position.vx > 4100000) {
			messagePtr->xPos = 4100000;
		}
		else {
			messagePtr->xPos = dynPtr->Position.vx;
		}

		messagePtr->xOrient = (dynPtr->OrientEuler.EulerX >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vy < -4100000) {
			messagePtr->yPos = -4100000;
		}
		else if (dynPtr->Position.vy > 4100000) {
			messagePtr->yPos = 4100000;
		}
		else {
			messagePtr->yPos = dynPtr->Position.vy;
		}

		messagePtr->yOrient = (dynPtr->OrientEuler.EulerY >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vz < -4100000) {
			messagePtr->zPos = -4100000;
		}
		else if (dynPtr->Position.vz > 4100000) {
			messagePtr->zPos = 4100000;
		}
		else {
			messagePtr->zPos = dynPtr->Position.vz;
		}

		messagePtr->zOrient = (dynPtr->OrientEuler.EulerZ >> NET_EULERSCALESHIFT);
	}
	{
		int obId = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((obId >= -NET_MAXOBJECTID)&&(obId <= NET_MAXOBJECTID));
		messagePtr->objectId = obId;
	}
	messagePtr->event_flag = 0;
	LOCALASSERT((sbPtr->I_SBtype >= 0) && (sbPtr->I_SBtype < 256));
	messagePtr->type = (unsigned char)sbPtr->I_SBtype;
	LOCALASSERT((sbPtr->I_SBtype == I_BehaviourRocket) ||
	            (sbPtr->I_SBtype == I_BehaviourPredatorEnergyBolt) ||
	            (sbPtr->I_SBtype == I_BehaviourFrisbeeEnergyBolt) ||
	            (sbPtr->I_SBtype == I_BehaviourPPPlasmaBolt) ||
	            (sbPtr->I_SBtype == I_BehaviourSpeargunBolt) ||
	            (sbPtr->I_SBtype == I_BehaviourGrenade) ||
	            (sbPtr->I_SBtype == I_BehaviourPulseGrenade) ||
	            (sbPtr->I_SBtype == I_BehaviourFlareGrenade) ||
	            (sbPtr->I_SBtype == I_BehaviourFragmentationGrenade) ||
	            (sbPtr->I_SBtype == I_BehaviourClusterGrenade) ||
	            (sbPtr->I_SBtype == I_BehaviourNPCPredatorDisc) ||
	            (sbPtr->I_SBtype == I_BehaviourPredatorDisc_SeekTrack) ||
	            (sbPtr->I_SBtype == I_BehaviourAlienSpit) ||
	            (sbPtr->I_SBtype == I_BehaviourProximityGrenade) ||
	            (sbPtr->I_SBtype == I_BehaviourInanimateObject) ||
	            (sbPtr->I_SBtype == I_BehaviourFrisbee) ||
	            (sbPtr->I_SBtype == I_BehaviourNetCorpse));

	if (sbPtr->I_SBtype == I_BehaviourInanimateObject)
	{
		INANIMATEOBJECT_STATUSBLOCK *objStatPtr = static_cast<INANIMATEOBJECT_STATUSBLOCK *>(sbPtr->SBdataptr);
		messagePtr->IOType = (unsigned char)objStatPtr->typeId;
		messagePtr->subtype = (unsigned char)objStatPtr->subType;
	}
	else
	{
		messagePtr->IOType = (unsigned char)IOT_Non;
		messagePtr->subtype = (unsigned char)0;
	}

	if (sbPtr->I_SBtype == I_BehaviourPredatorDisc_SeekTrack)
	{
		PC_PRED_DISC_BEHAV_BLOCK *bbPtr = (PC_PRED_DISC_BEHAV_BLOCK *) sbPtr->SBdataptr;

		if (bbPtr->Stuck)
		{
			/* Signal stuck-ness. */
			messagePtr->event_flag = 1;
		}
		else if (bbPtr->Bounced)
		{
			messagePtr->event_flag = 2;
			bbPtr->Bounced = 0;
		}
	}
	else if (sbPtr->I_SBtype == I_BehaviourFrisbee)
	{
		FRISBEE_BEHAV_BLOCK *fbPtr = (FRISBEE_BEHAV_BLOCK *) sbPtr->SBdataptr;

		if (fbPtr->Bounced)
		{
			messagePtr->event_flag = 2;
			fbPtr->Bounced = 0;
		}
	}
	else if (sbPtr->I_SBtype == I_BehaviourFlareGrenade)
	{
		FLARE_BEHAV_BLOCK *bbPtr = (FLARE_BEHAV_BLOCK *) sbPtr->SBdataptr;

		if (bbPtr->becomeStuck)
		{
			//set the event flag so that other players will start the flare noise
			messagePtr->event_flag = 1;
			bbPtr->becomeStuck = 0;
		}
	}
	else if ((sbPtr->I_SBtype == I_BehaviourGrenade) || (sbPtr->I_SBtype == I_BehaviourClusterGrenade))
	{
		GRENADE_BEHAV_BLOCK *bbPtr = (GRENADE_BEHAV_BLOCK *) sbPtr->SBdataptr;

		if (bbPtr->bouncelastframe)
		{
			/* Set event flag to record bounce sound. */
			messagePtr->event_flag = 1;
		}
	}
}


void AddNetMsg_LocalObjectDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple, int sectionID, int delta_seq, int delta_sub_seq, VECTORCH *incoming)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LOBDAMAGED_HEADER *messageHeader = 0;
	NETMESSAGE_DAMAGE_PROFILE *messageProfile = 0;
	NETMESSAGE_DAMAGE_MULTIPLE *messageMultiple = 0;
	NETMESSAGE_DAMAGE_SECTION *messageSection = 0;
	NETMESSAGE_DAMAGE_DELTA *messageDelta = 0;
	NETMESSAGE_DAMAGE_DIRECTION *messageDirection = 0;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int maxMessageSize = sizeof(NETMESSAGE_LOBDAMAGED_HEADER) +
	                     sizeof(NETMESSAGE_DAMAGE_PROFILE) +
	                     sizeof(NETMESSAGE_DAMAGE_MULTIPLE) +
	                     sizeof(NETMESSAGE_DAMAGE_SECTION) +
	                     sizeof(NETMESSAGE_DAMAGE_DELTA) +
	                     sizeof(NETMESSAGE_DAMAGE_DIRECTION);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + maxMessageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LocalObjectDamaged;
	{
		NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		LOCALASSERT(ghostData);

		if (sbPtr->I_SBtype != I_BehaviourNetGhost) {
			LOCALASSERT(1 == 0);
		}

		/*--------------------**
		**  set up the header **
		**--------------------*/
		messageHeader = (NETMESSAGE_LOBDAMAGED_HEADER *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_LOBDAMAGED_HEADER);
		messageHeader->playerId = ghostData->playerId;
		messageHeader->objectId = ghostData->playerObjectId;
		messageHeader->ammo_id = damage->Id;
		/*-----------------**
		**  damage profile **
		**-----------------*/
		messageHeader->damageProfile = 1;

		if (damage->Id > AMMO_NONE && damage->Id < MAX_NO_OF_AMMO_TEMPLATES)
		{
			if (AreDamageProfilesEqual(damage, &TemplateAmmo[damage->Id].MaxDamage[AvP.Difficulty])) {
				messageHeader->damageProfile = 0;
			}
		}
		else if (damage->Id == AMMO_FLECHETTE_POSTMAX) {
			messageHeader->damageProfile = 0;
		}

		if (messageHeader->damageProfile)
		{
			messageProfile = (NETMESSAGE_DAMAGE_PROFILE *)endSendBuffer;
			endSendBuffer += sizeof(NETMESSAGE_DAMAGE_PROFILE);
			messageProfile->Impact = damage->Impact;
			messageProfile->Cutting = damage->Cutting;
			messageProfile->Penetrative = damage->Penetrative;
			messageProfile->Fire = damage->Fire;
			messageProfile->Electrical = damage->Electrical;
			messageProfile->Acid = damage->Acid;
			messageProfile->ExplosivePower = damage->ExplosivePower;
			messageProfile->Slicing = damage->Slicing;
			messageProfile->ProduceBlood = damage->ProduceBlood;
			messageProfile->ForceBoom = damage->ForceBoom;
			messageProfile->BlowUpSections = damage->BlowUpSections;
			messageProfile->Special = damage->Special;
			messageProfile->MakeExitWounds = damage->MakeExitWounds;
		}

		/*-----------------**
		** damage multiple **
		**-----------------*/
		if (multiple != ONE_FIXED)
		{
			messageHeader->multiple = 1;
			messageMultiple = (NETMESSAGE_DAMAGE_MULTIPLE *)endSendBuffer;
			endSendBuffer += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
			messageMultiple->multiple = multiple;
		}
		else {
			messageHeader->multiple = 0;
		}

		/*------------**
		** section id **
		**------------*/
		if (sectionID != -1)
		{
			messageHeader->sectionID = 1;
			messageSection = (NETMESSAGE_DAMAGE_SECTION *)endSendBuffer;
			endSendBuffer += sizeof(NETMESSAGE_DAMAGE_SECTION);
			messageSection->SectionID = (short)sectionID;
		}
		else {
			messageHeader->sectionID = 0;
		}

		/*----------------**
		** delta sequence **
		**----------------*/
		if (delta_seq != -1)
		{
			messageHeader->delta_seq = 1;
			messageDelta = (NETMESSAGE_DAMAGE_DELTA *)endSendBuffer;
			endSendBuffer += sizeof(NETMESSAGE_DAMAGE_DELTA);
			messageDelta->Delta_Sequence = (char)delta_seq;
			messageDelta->Delta_Sub_Sequence = (char)delta_sub_seq;
		}
		else {
			messageHeader->delta_seq = 0;
		}

		/*-------------------**
		** direction         **
		**-------------------*/

		if (incoming && sbPtr->DynPtr)
		{
			VECTORCH direction = *incoming;
			messageHeader->direction = 1;
			messageDirection = (NETMESSAGE_DAMAGE_DIRECTION *)endSendBuffer;
			endSendBuffer += sizeof(NETMESSAGE_DAMAGE_DIRECTION);
			//need to rotate the vector into world space
			RotateVector(&direction, &sbPtr->DynPtr->OrientMat);
			//compress vector
			messageDirection->direction_x = direction.vx >> 7;
			messageDirection->direction_y = direction.vy >> 7;
			messageDirection->direction_z = direction.vz >> 7;
		}
		else {
			messageHeader->direction = 0;
		}
	}
}

void AddNetMsg_LocalObjectDestroyed_Request(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LOBDESTROYED_REQUEST *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LOBDESTROYED_REQUEST);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LOBDESTROYED_REQUEST *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LocalObjectDestroyed_Request;
	/* fill out message */
	{
		NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		LOCALASSERT(ghostData);

		if (sbPtr->I_SBtype != I_BehaviourNetGhost) {
			LOCALASSERT(1 == 0);
		}

		messagePtr->playerId = ghostData->playerId;
		messagePtr->objectId = ghostData->playerObjectId;
	}
}

void AddNetMsg_LocalObjectDestroyed(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LOBDESTROYED *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LOBDESTROYED);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LOBDESTROYED *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LocalObjectDestroyed;
	/* fill out message */
	{
		int obId = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((obId >= -NET_MAXOBJECTID)&&(obId <= NET_MAXOBJECTID));
		messagePtr->objectId = obId;
	}
}

void AddNetMsg_InanimateObjectDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_INANIMATEDAMAGED_HEADER *messageHeader = 0;
	NETMESSAGE_DAMAGE_PROFILE *messageProfile = 0;
	NETMESSAGE_DAMAGE_MULTIPLE *messageMultiple = 0;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int maxMessageSize = sizeof(NETMESSAGE_INANIMATEDAMAGED_HEADER) +
	                     sizeof(NETMESSAGE_DAMAGE_PROFILE) +
	                     sizeof(NETMESSAGE_DAMAGE_MULTIPLE);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* shouldn't be sending this if we are the host */
	LOCALASSERT(AvP.Network != I_Host);
	/* only send for inanimate objects*/
	LOCALASSERT(sbPtr->I_SBtype == I_BehaviourInanimateObject ||
	            sbPtr->I_SBtype == I_BehaviourPlacedLight ||
	            sbPtr->I_SBtype == I_BehaviourTrackObject);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + maxMessageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_InanimateObjectDamaged;
	/*--------------------**
	**  set up the header **
	**--------------------*/
	messageHeader = (NETMESSAGE_INANIMATEDAMAGED_HEADER *)endSendBuffer;
	endSendBuffer += sizeof(NETMESSAGE_INANIMATEDAMAGED_HEADER);
	COPY_NAME(&(messageHeader->name), &(sbPtr->SBname));
	messageHeader->ammo_id = damage->Id;
	/*-----------------**
	**  damage profile **
	**-----------------*/
	messageHeader->damageProfile = 1;

	if (damage->Id > AMMO_NONE && damage->Id < MAX_NO_OF_AMMO_TEMPLATES)
	{
		if (AreDamageProfilesEqual(damage, &TemplateAmmo[damage->Id].MaxDamage[AvP.Difficulty])) {
			messageHeader->damageProfile = 0;
		}
	}

	if (messageHeader->damageProfile)
	{
		messageProfile = (NETMESSAGE_DAMAGE_PROFILE *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_DAMAGE_PROFILE);
		messageProfile->Impact = damage->Impact;
		messageProfile->Cutting = damage->Cutting;
		messageProfile->Penetrative = damage->Penetrative;
		messageProfile->Fire = damage->Fire;
		messageProfile->Electrical = damage->Electrical;
		messageProfile->Acid = damage->Acid;
		messageProfile->ExplosivePower = damage->ExplosivePower;
		messageProfile->Slicing = damage->Slicing;
		messageProfile->ProduceBlood = damage->ProduceBlood;
		messageProfile->ForceBoom = damage->ForceBoom;
		messageProfile->BlowUpSections = damage->BlowUpSections;
		messageProfile->Special = damage->Special;
		messageProfile->MakeExitWounds = damage->MakeExitWounds;
	}

	/*-----------------**
	** damage multiple **
	**-----------------*/
	if (multiple != ONE_FIXED)
	{
		messageHeader->multiple = 1;
		messageMultiple = (NETMESSAGE_DAMAGE_MULTIPLE *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
		messageMultiple->multiple = multiple;
	}
	else {
		messageHeader->multiple = 0;
	}
}


void AddNetMsg_InanimateObjectDestroyed(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_INANIMATEDESTROYED *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_INANIMATEDESTROYED);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* should only send if we're the host */
	LOCALASSERT(AvP.Network == I_Host);
	LOCALASSERT(sbPtr->I_SBtype == I_BehaviourInanimateObject ||
	            sbPtr->I_SBtype == I_BehaviourPlacedLight ||
	            sbPtr->I_SBtype == I_BehaviourTrackObject);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_INANIMATEDESTROYED *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_InanimateObjectDestroyed;
	/* fill out message */
	
	COPY_NAME(&(messagePtr->name), &(sbPtr->SBname));
}

void AddNetMsg_ObjectPickedUp(char *objectName)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_OBJECTPICKEDUP *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_OBJECTPICKEDUP);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_OBJECTPICKEDUP *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_ObjectPickedUp;
	/* fill out the message */
	COPY_NAME((&messagePtr->name[0]), objectName);
}

void AddNetMsg_EndGame(void)
{
	NETMESSAGEHEADER *headerPtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	/* should be sent by host only, in endGame state */
	LOCALASSERT(AvP.Network == I_Host);
	/* yes: need to send this before changing state, as we need to know our previous state,
	and sendMessage requires one of these two states anyway */
	LOCALASSERT((netGameData.myGameState == NGS_StartUp) || (netGameData.myGameState == NGS_Playing) || (netGameData.myGameState == NGS_Joining));
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_EndGame;
}

void AddNetMsg_LOSRequestBinarySwitch(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LOSREQUESTBINARYSWITCH *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LOSREQUESTBINARYSWITCH);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* also, the object must be a binary switch */
	if (sbPtr->I_SBtype != I_BehaviourBinarySwitch && sbPtr->I_SBtype != I_BehaviourLinkSwitch) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LOSREQUESTBINARYSWITCH *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LOSRequestBinarySwitch;
	/* fill out the message */
	COPY_NAME((&messagePtr->name[0]), &(sbPtr->SBname));
}

void AddNetMsg_PlatformLiftState(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLATFORMLIFTSTATE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLATFORMLIFTSTATE);
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platLiftData;
	/* only hosts should send this*/
	LOCALASSERT(AvP.Network == I_Host);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if (sbPtr->I_SBtype != I_BehaviourPlatform) {
		return;
	}

	platLiftData = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platLiftData);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLATFORMLIFTSTATE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PlatformLiftState;
	/* fill out the message */
	COPY_NAME((&messagePtr->name[0]), &(sbPtr->SBname));
	messagePtr->state = (char)(platLiftData->state);
}

void AddNetMsg_RequestPlatformLiftActivate(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE);
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platLiftData;
	/* only peers should send this*/
	LOCALASSERT(AvP.Network == I_Peer);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if (sbPtr->I_SBtype != I_BehaviourPlatform) {
		return;
	}

	platLiftData = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)sbPtr->SBdataptr;
	LOCALASSERT(platLiftData);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_RequestPlatformLiftActivate;
	/* fill out the message */
	COPY_NAME((&messagePtr->name[0]), &(sbPtr->SBname));
}

/* KJL 17:49:31 20/01/98 - transmit make decal info */
void AddNetMsg_PlayerAutoGunState(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_AGUNSTATE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_AGUNSTATE);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_AGUNSTATE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PlayerAutoGunState;
	/* fill out the message */
	{
		DYNAMICSBLOCK *dynPtr = sbPtr->DynPtr;
		LOCALASSERT(dynPtr);
		LOCALASSERT((dynPtr->Position.vx < 4194304) && (dynPtr->Position.vx > -4194304)); /* 23 bits of data */
		LOCALASSERT((dynPtr->Position.vy < 4194304) && (dynPtr->Position.vy > -4194304)); /* 23 bits of data */
		LOCALASSERT((dynPtr->Position.vz < 4194304) && (dynPtr->Position.vz > -4194304)); /* 23 bits of data */
		LOCALASSERT((dynPtr->OrientEuler.EulerX >= 0) && (dynPtr->OrientEuler.EulerX < 4096)); /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerY >= 0) && (dynPtr->OrientEuler.EulerY < 4096)); /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerZ >= 0) && (dynPtr->OrientEuler.EulerZ < 4096));  /* 9 bits of signed data */

		/* NB we can fit +-4194303 into 23 bits */
		if (dynPtr->Position.vx < -4100000) {
			messagePtr->xPos = -4100000;
		}
		else if (dynPtr->Position.vx > 4100000) {
			messagePtr->xPos = 4100000;
		}
		else {
			messagePtr->xPos = dynPtr->Position.vx;
		}

		messagePtr->xOrient = (dynPtr->OrientEuler.EulerX >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vy < -4100000) {
			messagePtr->yPos = -4100000;
		}
		else if (dynPtr->Position.vy > 4100000) {
			messagePtr->yPos = 4100000;
		}
		else {
			messagePtr->yPos = dynPtr->Position.vy;
		}

		messagePtr->yOrient = (dynPtr->OrientEuler.EulerY >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vz < -4100000) {
			messagePtr->zPos = -4100000;
		}
		else if (dynPtr->Position.vz > 4100000) {
			messagePtr->zPos = 4100000;
		}
		else {
			messagePtr->zPos = dynPtr->Position.vz;
		}

		messagePtr->zOrient = (dynPtr->OrientEuler.EulerZ >> NET_EULERSCALESHIFT);
	}
	/* add the object Id */
	{
		int obId = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((obId >= -NET_MAXOBJECTID)&&(obId <= NET_MAXOBJECTID));
		messagePtr->objectId = obId;
	}
	/* am I firing?? */
	{
		AUTOGUN_STATUS_BLOCK *agData;
		agData = (AUTOGUN_STATUS_BLOCK *)(sbPtr->SBdataptr);
		LOCALASSERT(agData);

		if (agData->Firing) {
			messagePtr->IAmFiring = 1;
		}
		else {
			messagePtr->IAmFiring = 0;
		}

		if (agData->behaviourState == I_disabled) {
			messagePtr->IAmEnabled = 0;
		}
		else {
			messagePtr->IAmEnabled = 1;
		}
	}
}

/* KJL 17:49:31 20/01/98 - transmit make decal info */
void AddNetMsg_MakeDecal(enum DECAL_ID decalID, VECTORCH *normalPtr, VECTORCH *positionPtr, int moduleIndex)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_MAKEDECAL *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_MAKEDECAL);
	extern int GlobalFrameCounter;
	static int DecalCountThisFrame = 0;
	static int FrameStamp;

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if (FrameStamp != GlobalFrameCounter)
	{
		FrameStamp = GlobalFrameCounter;
		DecalCountThisFrame = 0;
	}

	/*Limit the decal count per frame, even in lan games. Otherwise it is easy to get assertions from having tons
	of alien blood particles*/
	if (DecalCountThisFrame >= 10) {
		return;
	}

	DecalCountThisFrame++;
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > (numBytesLeft - 1000))
		{
			//  LOCALASSERT(1==0);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_MAKEDECAL *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_MakeDecal;
	/* fill out the message */
	messagePtr->DecalID = decalID;
	messagePtr->Position = *positionPtr;
	messagePtr->Direction = *normalPtr;
	messagePtr->ModuleIndex = moduleIndex;
}

/* KJL 14:34:45 08/04/98 - broadcast a message to the other players */
void AddNetMsg_ChatBroadcast(char *string, BOOL same_species_only)
{
	NETMESSAGEHEADER *headerPtr;
	unsigned char *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize;
	unsigned char stringLength = 1;

	//remove the console (assuming it is on screen)
	if (IOFOCUS_AcceptTyping()) {
		IOFOCUS_Toggle();
	}

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	char *ptr = string;

	while (*ptr && stringLength < 255)
	{
		stringLength++;
		ptr++;
	}

	if (stringLength == 1 || stringLength >= 255) {
		return;
	}

	messageSize = stringLength + 1;
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_ChatBroadcast;
	/* fill out the message */
	{
		char *ptr = string;
		//first byte used to distinguish between SAY and SPECIES_SAY
		*messagePtr++ = (char)same_species_only;

		do {
			*messagePtr++ = *ptr;
		}
		while (*ptr++);
	}
	// Show our own chat line on our screen as "NAME says: message".
	// OnScreenMessageBuffer holds 300 bytes: up to 299 characters (letters,
	// digits, spaces and so on), then a zero byte that marks where the text
	// stops (the "end marker").
	// The check near the top of this function turns away a message of 254
	// characters or more, and a player's name is kept in a 13-byte box (at
	// most 12 characters and its end marker). So today the line is at most
	// 12 + 7 + 253 = 272 characters long and always fits.
	// The limits are also put right here, so the line can never run past the
	// end of the buffer, even if one of those checks is changed one day:
	// - "%.*s" with NET_PLAYERNAMELENGTH reads no more than the 13 bytes of
	//   the name box, even if its end marker were missing;
	// - _snprintf writes at most 299 characters. When it has to cut the text
	//   short it leaves out the end marker, so we always write one ourselves
	//   into the last byte.
	// A line that fits comes out exactly as before.
	// A host's description can leave this machine off the list, and it would be read from before the list's start
	int const myPlaceInTheList = PlayerIdInPlayerList(AvPNetID);

	if (myPlaceInTheList == NET_IDNOTINPLAYERLIST)
	{
		return;
	}

	_snprintf(OnScreenMessageBuffer, sizeof(OnScreenMessageBuffer) - 1, "%.*s says: %s", NET_PLAYERNAMELENGTH, netGameData.playerData[myPlaceInTheList].name, string);
	OnScreenMessageBuffer[sizeof(OnScreenMessageBuffer) - 1] = '\0';
	NewOnScreenMessage(OnScreenMessageBuffer);
}

/* KJL 11:28:44 27/04/98 - make an explosion (just the sfx; damage is handled separately */
void AddNetMsg_MakeExplosion(VECTORCH *positionPtr, enum EXPLOSION_ID explosionID)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_MAKEEXPLOSION *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_MAKEEXPLOSION);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_MAKEEXPLOSION *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_MakeExplosion;
	/* fill out the message */
	messagePtr->Position = *positionPtr;
	messagePtr->ExplosionID = explosionID;
}
void AddNetMsg_MakeFlechetteExplosion(VECTORCH *positionPtr, int seed)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_MAKEFLECHETTEEXPLOSION *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_MAKEFLECHETTEEXPLOSION);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_MAKEFLECHETTEEXPLOSION *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_MakeFlechetteExplosion;
	/* fill out the message */
	messagePtr->Position = *positionPtr;
	messagePtr->Seed = seed;
}

void AddNetMsg_MakePlasmaExplosion(VECTORCH *positionPtr, VECTORCH *fromPositionPtr, enum EXPLOSION_ID explosionID)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_MAKEPLASMAEXPLOSION *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_MAKEPLASMAEXPLOSION);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_MAKEPLASMAEXPLOSION *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_MakePlasmaExplosion;
	/* fill out the message */
	messagePtr->Position = *positionPtr;
	messagePtr->FromPosition = *fromPositionPtr;
	messagePtr->ExplosionID = explosionID;
}

/* KJL 11:27:47 20/05/98 - predator laser sights */
void AddNetMsg_PredatorLaserSights(VECTORCH *positionPtr, VECTORCH *normalPtr, DISPLAYBLOCK *dispPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PREDATORSIGHTS *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PREDATORSIGHTS);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PREDATORSIGHTS *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_PredatorSights;
	/* fill out the message */
	{
		MATRIXCH orientMat;
		EULER orientation;
		MakeMatrixFromDirection(normalPtr, &orientMat);
		MatrixToEuler(&orientMat, &orientation);

		/* NB we can fit +-4194303 into 23 bits */
		if (positionPtr->vx < -4100000) {
			messagePtr->xPos = -4100000;
		}
		else if (positionPtr->vx > 4100000) {
			messagePtr->xPos = 4100000;
		}
		else {
			messagePtr->xPos = positionPtr->vx;
		}

		messagePtr->xOrient = (orientation.EulerX >> NET_EULERSCALESHIFT);

		if (positionPtr->vy < -4100000) {
			messagePtr->yPos = -4100000;
		}
		else if (positionPtr->vy > 4100000) {
			messagePtr->yPos = 4100000;
		}
		else {
			messagePtr->yPos = positionPtr->vy;
		}

		messagePtr->yOrient = (orientation.EulerY >> NET_EULERSCALESHIFT);

		if (positionPtr->vz < -4100000) {
			messagePtr->zPos = -4100000;
		}
		else if (positionPtr->vz > 4100000) {
			messagePtr->zPos = 4100000;
		}
		else {
			messagePtr->zPos = positionPtr->vz;
		}

		messagePtr->zOrient = (orientation.EulerZ >> NET_EULERSCALESHIFT);
		messagePtr->TargetID = 0;

		if (dispPtr)
		{
			if (dispPtr->ObStrategyBlock)
			{
				if (dispPtr->ObStrategyBlock->I_SBtype == I_BehaviourNetGhost)
				{
					NETGHOSTDATABLOCK *ghostDataPtr = (NETGHOSTDATABLOCK *)dispPtr->ObStrategyBlock->SBdataptr;

					if (ghostDataPtr->playerObjectId == GHOST_PLAYEROBJECTID) {
						messagePtr->TargetID = ghostDataPtr->playerId;
					}
				}
			}
		}
	}
}

void AddNetMsg_LocalObjectOnFire(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LOBONFIRE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LOBONFIRE);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LOBONFIRE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LocalObjectOnFire;
	/* fill out message */
	{
		NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		LOCALASSERT(ghostData);

		if (sbPtr->I_SBtype != I_BehaviourNetGhost) {
			LOCALASSERT(1 == 0);
		}

		messagePtr->playerId = ghostData->playerId;
		/* LOCALASSERT((ghostData->playerObjectId >= -NET_MAXOBJECTID)&&(ghostData->playerObjectId <= NET_MAXOBJECTID)); */
		/*
		        if((ghostData->playerObjectId < -NET_MAXOBJECTID)||(ghostData->playerObjectId > NET_MAXOBJECTID))
		        {
		            LOCALASSERT(1==0);
		        }
		*/
		messagePtr->objectId = ghostData->playerObjectId;
		/* That's all, folks.  This object is on fire. */
	}
}

void AddNetMsg_RestartNetworkGame(int seed)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_RESTARTGAME *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_RESTARTGAME);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_RESTARTGAME *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = NetMT_RestartNetworkGame;
	/* Fill in message. */
	messagePtr->seed = seed;
}

void AddNetMsg_FragmentalObjectsStatus(void)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_FRAGMENTALOBJECTSSTATUS *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_FRAGMENTALOBJECTSSTATUS);
	static int object_batch = 0;

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* only the host should send this */
	LOCALASSERT(AvP.Network == I_Host);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_FRAGMENTALOBJECTSSTATUS *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_FragmentalObjectsStatus;
	messagePtr->BatchNumber = object_batch;
	/* fill out the message */
	{
		int fragNumber = 0;
		int objectsToSkip = object_batch * (NUMBER_OF_FRAGMENTAL_OBJECTS << 3);
		int noPlacedLights = 0;
		LOCALASSERT(AvP.Network != I_No_Network);

		int i;
		for (i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *sbPtr = ActiveStBlockList[i];

			if (sbPtr->I_SBtype == I_BehaviourInanimateObject)
			{
				INANIMATEOBJECT_STATUSBLOCK *objectStatusPtr = static_cast<INANIMATEOBJECT_STATUSBLOCK *>(sbPtr->SBdataptr);
				LOCALASSERT(objectStatusPtr);

				if ((objectStatusPtr->typeId == IOT_Static) && (!objectStatusPtr->Indestructable) && (!objectStatusPtr->ghosted_object) && (!objectStatusPtr->lifespanTimer))
				{
					if (objectsToSkip > 0)
					{
						objectsToSkip--;
						continue;
					}

					WriteFragmentStatus(fragNumber++, (objectStatusPtr->respawnTimer == 0));
				}
			}
			else if (sbPtr->I_SBtype == I_BehaviourPlacedLight)
			{
				PLACED_LIGHT_BEHAV_BLOCK *pl_bhv = static_cast<PLACED_LIGHT_BEHAV_BLOCK *>(sbPtr->SBdataptr);
				LOCALASSERT(pl_bhv);

				if (!pl_bhv->Indestructable)
				{
					if (objectsToSkip > 0)
					{
						objectsToSkip--;
						continue;
					}

					WriteFragmentStatus(fragNumber++, (pl_bhv->state != Light_State_Broken));
				}
			}

			if (fragNumber >= (NUMBER_OF_FRAGMENTAL_OBJECTS << 3)) {
				break;
			}
		}

		if (i == NumActiveStBlocks) {
			object_batch = 0;
		}
		else {
			//there are more objects to look at , so increment batch
			object_batch++;
		}

		textprint("noPlacedLights %d\n", noPlacedLights);
		textprint("fragNumber %d\n", fragNumber);

		for (i = 0; i < NUMBER_OF_FRAGMENTAL_OBJECTS; i++) {
			messagePtr->StatusBitfield[i] = FragmentalObjectStatus[i];
		}
	}
}

void AddNetMsg_StrategySynch(void)
{
	/*
	    Scan through objects looking for strategies that may need to be synchronized between players.
	    Mainly switches and track objects
	    Only do 16 objects at a time , so as not to send too much data
	*/
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_STRATEGYSYNCH *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_STRATEGYSYNCH);
	static int object_batch = 0;

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* only the host should send this */
	LOCALASSERT(AvP.Network == I_Host);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_STRATEGYSYNCH *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_StrategySynch;
	messagePtr->BatchNumber = object_batch;
	/* fill out the message */
	{
		int i;
		int objectNumber = 0;
		int objectsToSkip = object_batch * (NUMBER_OF_STRATEGIES_TO_SYNCH);
		LOCALASSERT(AvP.Network != I_No_Network);

		for (i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *sbPtr = ActiveStBlockList[i];

			if (sbPtr->I_SBtype == I_BehaviourBinarySwitch ||
			    sbPtr->I_SBtype == I_BehaviourLinkSwitch ||
			    sbPtr->I_SBtype == I_BehaviourTrackObject)
			{
				if (objectsToSkip > 0)
				{
					objectsToSkip--;
					continue;
				}

				switch (sbPtr->I_SBtype)
				{
					case I_BehaviourBinarySwitch :
						WriteStrategySynch(objectNumber++, BinarySwitchGetSynchData(sbPtr));
						break;

					case I_BehaviourLinkSwitch :
						WriteStrategySynch(objectNumber++, LinkSwitchGetSynchData(sbPtr));
						break;

					case I_BehaviourTrackObject :
						WriteStrategySynch(objectNumber++, TrackObjectGetSynchData(sbPtr));
						break;
				}
			}

			if (objectNumber >= (NUMBER_OF_STRATEGIES_TO_SYNCH)) {
				break;
			}
		}

		if (i == NumActiveStBlocks) {
			object_batch = 0;
		}
		else
		{
			//there are more objects to look at , so increment batch
			object_batch++;
		}

		for (i = 0; i<NUMBER_OF_STRATEGIES_TO_SYNCH >> 2; i++) {
			messagePtr->StatusBitfield[i] = StrategySynchArray[i];
		}
	}

	if (!netGameData.myStrategyCheckSum) {
		netGameData.myStrategyCheckSum = GetStrategySynchObjectChecksum();
	}

	messagePtr->strategyCheckSum = netGameData.myStrategyCheckSum;
}

void AddNetMsg_CreateWeapon(char *objectName, int type, VECTORCH *location)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_CREATEWEAPON *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_CREATEWEAPON);

	/* only send this if we are playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_CREATEWEAPON *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_CreateWeapon;
	/* fill out the message */
	COPY_NAME((&messagePtr->name[0]), objectName);
	messagePtr->location = *location;
	messagePtr->type = type;
}

/* KJL 16:32:06 17/06/98 - alien AI network messages */
void AddNetMsg_AlienAIState(STRATEGYBLOCK *sbPtr)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_ALIENAISTATE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_ALIENAISTATE);
	DYNAMICSBLOCK *dynPtr = sbPtr->DynPtr;
	ALIEN_STATUS_BLOCK *alienStatusPtr = (ALIEN_STATUS_BLOCK *)(sbPtr->SBdataptr);
	LOCALASSERT(dynPtr);
	GLOBALASSERT(alienStatusPtr);
#if EXTRAPOLATION_TEST

	if (UseExtrapolation && netGameData.sendFrequency)
	{
		BOOL updateRequired = FALSE;
		VECTORCH facing;
		//can we get away with not sending an update this frame

		//has it been a while since the last send
		if (TimeCounterForExtrapolation < (unsigned int)alienStatusPtr->timeOfLastSend ||
		    TimeCounterForExtrapolation - alienStatusPtr->timeOfLastSend > ONE_FIXED / 4)
		{
			updateRequired = TRUE;
		}

		if (dynPtr->LinImpulse.vx || dynPtr->LinImpulse.vy || dynPtr->LinImpulse.vz)
		{
			//alien is probably jumping , extrapolation doesn't work in this case
			updateRequired = TRUE;
		}

		//has the velocity changed
		if (!updateRequired)
		{
			int diff = Magnitude(&dynPtr->LinVelocity) - Magnitude(&alienStatusPtr->lastVelocitySent);
			if (diff > 500 || -diff > 500) {
				updateRequired = TRUE;
			}
		}

		facing.vx = dynPtr->OrientMat.mat31;
		facing.vy = dynPtr->OrientMat.mat32;
		facing.vz = dynPtr->OrientMat.mat33;

		//has the facing changed
		if (!updateRequired)
		{
			if (DotProduct(&facing, &alienStatusPtr->lastFacingSent) < 64000) {
				updateRequired = TRUE;
			}
		}

		if (!updateRequired) {
			return;
		}

		//okay , we do need to send this frame
		alienStatusPtr->timeOfLastSend = TimeCounterForExtrapolation;
		alienStatusPtr->lastVelocitySent = dynPtr->LinVelocity;
		alienStatusPtr->lastFacingSent = facing;
	}

#endif
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_ALIENAISTATE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_AlienAIState;
	/* fill out our position and orientation */
	{
		LOCALASSERT((dynPtr->OrientEuler.EulerX >= 0) && (dynPtr->OrientEuler.EulerX < 4096));  /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerY >= 0) && (dynPtr->OrientEuler.EulerY < 4096));  /* 9 bits of signed data */
		LOCALASSERT((dynPtr->OrientEuler.EulerZ >= 0) && (dynPtr->OrientEuler.EulerZ < 4096));  /* 9 bits of signed data */

		/* NB we can fit +-4194303 into 23 bits */
		if (dynPtr->Position.vx < -4100000) {
			messagePtr->xPos = -4100000;
		}
		else if (dynPtr->Position.vx > 4100000) {
			messagePtr->xPos = 4100000;
		}
		else {
			messagePtr->xPos = dynPtr->Position.vx;
		}

		messagePtr->xOrient = (dynPtr->OrientEuler.EulerX >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vy < -4100000) {
			messagePtr->yPos = -4100000;
		}
		else if (dynPtr->Position.vy > 4100000) {
			messagePtr->yPos = 4100000;
		}
		else {
			messagePtr->yPos = dynPtr->Position.vy;
		}

		messagePtr->yOrient = (dynPtr->OrientEuler.EulerY >> NET_EULERSCALESHIFT);

		if (dynPtr->Position.vz < -4100000) {
			messagePtr->zPos = -4100000;
		}
		else if (dynPtr->Position.vz > 4100000) {
			messagePtr->zPos = 4100000;
		}
		else {
			messagePtr->zPos = dynPtr->Position.vz;
		}

		messagePtr->zOrient = (dynPtr->OrientEuler.EulerZ >> NET_EULERSCALESHIFT);
#if EXTRAPOLATION_TEST
		messagePtr->standard_gravity = dynPtr->UseStandardGravity;
		messagePtr->speed = Magnitude(&dynPtr->LinVelocity);
#endif
	}
	/* fill out anim sequence */
	{
#if 1
		messagePtr->sequence_type = alienStatusPtr->HModelController.Sequence_Type;
		messagePtr->sub_sequence = alienStatusPtr->HModelController.Sub_Sequence;
		//send the sequence length in 256ths of a second (instead of 65536ths)
		GLOBALASSERT(alienStatusPtr->HModelController.Seconds_For_Sequence >= 0 && alienStatusPtr->HModelController.Seconds_For_Sequence < 32 * ONE_FIXED);
		messagePtr->sequence_length = alienStatusPtr->HModelController.Seconds_For_Sequence >> 8;
#else

		if (alienStatusPtr->HModelController.Tweening == Controller_NoTweening)
		{
			messagePtr->sequence_type = alienStatusPtr->HModelController.Sequence_Type;
			messagePtr->sub_sequence = alienStatusPtr->HModelController.Sub_Sequence;
			messagePtr->sequence_length = alienStatusPtr->HModelController.Seconds_For_Sequence;
		}
		else
		{
			/* Might be junk. */
			messagePtr->sequence_type  = -1;
			messagePtr->sub_sequence   = -1;
			messagePtr->sequence_length = -1;
		}

#endif
		messagePtr->AlienType = alienStatusPtr->Type;
	}

	if (sbPtr->SBDamageBlock.IsOnFire) {
		messagePtr->IAmOnFire = 1;
	}
	else {
		messagePtr->IAmOnFire = 0;
	}

	/* fill out guid */
	{
		int guid = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((guid >= -NET_MAXOBJECTID)&&(guid <= NET_MAXOBJECTID));
		messagePtr->Guid = guid;
	}
}

/* CDF 24/8/98 A better message. */
void AddNetMsg_AlienAISeqChange(STRATEGYBLOCK *sbPtr, int sequence_type, int sub_sequence, int sequence_length, int tweening_time)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_ALIENSEQUENCECHANGE *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_ALIENSEQUENCECHANGE);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_ALIENSEQUENCECHANGE *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_AlienAISequenceChange;
	/* fill out anim sequence */
	messagePtr->sequence_type  = sequence_type  ;
	messagePtr->sub_sequence   = sub_sequence   ;

	// convert times into 256ths of a second
	if (sequence_length == -1) {
		messagePtr->sequence_length = -1;
	}
	else {
		messagePtr->sequence_length = sequence_length >> 8;
	}

	if (tweening_time == -1) {
		messagePtr->tweening_time  = -1  ;
	}
	else {
		messagePtr->tweening_time  = tweening_time >> 8  ;
	}

	/* fill out guid */
	{
		int guid = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((guid >= -NET_MAXOBJECTID)&&(guid <= NET_MAXOBJECTID));
		messagePtr->Guid = guid;
	}
}

void AddNetMsg_AlienAIKilled(STRATEGYBLOCK *sbPtr, int death_code, int death_time, int GibbFactor, DAMAGE_PROFILE *damage)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_ALIENAIKILLED *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_ALIENAIKILLED);
	ALIEN_STATUS_BLOCK *alienStatus = (ALIEN_STATUS_BLOCK *)sbPtr->SBdataptr;

	// don't do this if we aren't playing (possibly on end game screen)
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_ALIENAIKILLED *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_AlienAIKilled;
	/* fill out anim sequence */
	messagePtr->death_code = death_code;
	messagePtr->death_time = death_time;
	messagePtr->GibbFactor = GibbFactor;
	messagePtr->killerId = myNetworkKillerId;
	{
		int killerIndex = PlayerIdInPlayerList(messagePtr->killerId);

		if (killerIndex != NET_IDNOTINPLAYERLIST)
		{
			netGameData.playerData[killerIndex].aliensKilled[alienStatus->Type]++;
			messagePtr->killCount = netGameData.playerData[killerIndex].aliensKilled[alienStatus->Type];
			//record ai's death for purposes of adjusting difficulty.
			//(assuming that optionis being used)
			GeneratorBalance_NoteAIDeath();
		}
		else
		{
			/*
			the player doing the damage has either left the game , or never existed.
			call it suicide then.
			(Could also be 'neutral' damage - flame jets)
			*/
			messagePtr->killerId = 0;
			messagePtr->killCount = 0;
		}
	}
	/* fill out guid */
	{
		int guid = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((guid >= -NET_MAXOBJECTID)&&(guid <= NET_MAXOBJECTID));
		messagePtr->Guid = guid;
	}
	messagePtr->AlienType = alienStatus->Type;
	//find the icon for the weapon used
	messagePtr->weaponIcon = GetWeaponIconFromDamage(damage);
	Inform_AiHasDied(messagePtr->killerId, static_cast<enum AlienType>(messagePtr->AlienType), messagePtr->weaponIcon);
}

void AddNetMsg_FarAlienPosition(STRATEGYBLOCK *sbPtr, int targetModuleIndex, int index, BOOL indexIsModuleIndex)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_FARALIENPOSITION *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_FARALIENPOSITION);
	ALIEN_STATUS_BLOCK *alienStatusPtr = (ALIEN_STATUS_BLOCK *)(sbPtr->SBdataptr);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_FARALIENPOSITION *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_FarAlienPosition;
	/* fill out indeces */
	messagePtr->targetModuleIndex = targetModuleIndex;
	messagePtr->index = index;
	messagePtr->indexIsModuleIndex = indexIsModuleIndex;
	messagePtr->alienType = alienStatusPtr->Type;
	/* fill out guid */
	{
		int guid = *((int *)(&(sbPtr->SBname[4])));
		messagePtr->Guid = guid;
	}
}

void AddNetMsg_GhostHierarchyDamaged(STRATEGYBLOCK *sbPtr, DAMAGE_PROFILE *damage, int multiple, int sectionID, VECTORCH *incoming)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER *messageHeader = 0;
	NETMESSAGE_DAMAGE_PROFILE *messageProfile = 0;
	NETMESSAGE_DAMAGE_MULTIPLE *messageMultiple = 0;
	NETMESSAGE_DAMAGE_SECTION *messageSection = 0;
	NETMESSAGE_DAMAGE_DIRECTION *messageDirection = 0;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int maxMessageSize = sizeof(NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER) +
	                     sizeof(NETMESSAGE_DAMAGE_PROFILE) +
	                     sizeof(NETMESSAGE_DAMAGE_MULTIPLE) +
	                     sizeof(NETMESSAGE_DAMAGE_SECTION) +
	                     sizeof(NETMESSAGE_DAMAGE_DIRECTION);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + maxMessageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_GhostHierarchyDamaged;
	/*--------------------**
	**  set up the header **
	**--------------------*/
	messageHeader = (NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER *)endSendBuffer;
	endSendBuffer += sizeof(NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER);
	/* fill out guid */
	{
		int guid = *((int *)(&(sbPtr->SBname[4])));
		messageHeader->Guid = guid;
	}
	messageHeader->ammo_id = damage->Id;
	/*-----------------**
	**  damage profile **
	**-----------------*/
	messageHeader->damageProfile = 1;

	if (damage->Id > AMMO_NONE && damage->Id < MAX_NO_OF_AMMO_TEMPLATES)
	{
		if (AreDamageProfilesEqual(damage, &TemplateAmmo[damage->Id].MaxDamage[AvP.Difficulty])) {
			messageHeader->damageProfile = 0;
		}
	}

	if (messageHeader->damageProfile)
	{
		messageProfile = (NETMESSAGE_DAMAGE_PROFILE *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_DAMAGE_PROFILE);
		messageProfile->Impact = damage->Impact;
		messageProfile->Cutting = damage->Cutting;
		messageProfile->Penetrative = damage->Penetrative;
		messageProfile->Fire = damage->Fire;
		messageProfile->Electrical = damage->Electrical;
		messageProfile->Acid = damage->Acid;
		messageProfile->ExplosivePower = damage->ExplosivePower;
		messageProfile->Slicing = damage->Slicing;
		messageProfile->ProduceBlood = damage->ProduceBlood;
		messageProfile->ForceBoom = damage->ForceBoom;
		messageProfile->BlowUpSections = damage->BlowUpSections;
		messageProfile->Special = damage->Special;
		messageProfile->MakeExitWounds = damage->MakeExitWounds;
	}

	/*-----------------**
	** damage multiple **
	**-----------------*/
	if (multiple != ONE_FIXED)
	{
		messageHeader->multiple = 1;
		messageMultiple = (NETMESSAGE_DAMAGE_MULTIPLE *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
		messageMultiple->multiple = multiple;
	}
	else {
		messageHeader->multiple = 0;
	}

	/*------------**
	** section id **
	**------------*/
	if (sectionID != -1)
	{
		messageHeader->sectionID = 1;
		messageSection = (NETMESSAGE_DAMAGE_SECTION *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_DAMAGE_SECTION);
		messageSection->SectionID = (short)sectionID;
	}
	else {
		messageHeader->sectionID = 0;
	}

	/*-------------------**
	** direction         **
	**-------------------*/

	if (incoming && sbPtr->DynPtr)
	{
		VECTORCH direction = *incoming;
		messageHeader->direction = 1;
		messageDirection = (NETMESSAGE_DAMAGE_DIRECTION *)endSendBuffer;
		endSendBuffer += sizeof(NETMESSAGE_DAMAGE_DIRECTION);
		//need to rotate the vector into world space
		RotateVector(&direction, &sbPtr->DynPtr->OrientMat);
		//compress vector
		messageDirection->direction_x = direction.vx >> 7;
		messageDirection->direction_y = direction.vy >> 7;
		messageDirection->direction_z = direction.vz >> 7;
	}
	else {
		messageHeader->direction = 0;
	}
}


void AddNetMsg_Gibbing(STRATEGYBLOCK *sbPtr, int gibbFactor, int seed)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_GIBBING *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_GIBBING);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_GIBBING *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_Gibbing;
	messagePtr->gibbFactor = gibbFactor;
	messagePtr->seed = seed;
	/* fill out guid */
	{
		int guid = *((int *)(&(sbPtr->SBname[4])));
		//      LOCALASSERT((guid >= -NET_MAXOBJECTID)&&(guid <= NET_MAXOBJECTID));
		messagePtr->Guid = guid;
	}
}


void AddNetMsg_SpotAlienSound(int soundCategory, int alienType, int pitch, VECTORCH *position)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_SPOTALIENSOUND *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_SPOTALIENSOUND);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_SPOTALIENSOUND *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_SpotAlienSound;
	/* Fill in message. */
	messagePtr->soundCategory = (unsigned char)soundCategory;
	messagePtr->pitch = pitch;
	messagePtr->alienType = (unsigned char)alienType;
	messagePtr->vx = position->vx;
	messagePtr->vy = position->vy;
	messagePtr->vz = position->vz;
}

// for messages that just require a player id
void AddNetMsg_PlayerID(NetID playerID, unsigned char message)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_PLAYERID *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_PLAYERID);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_PLAYERID *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = message;
	/* Fill in message. */
	messagePtr->playerID = playerID;
}

void AddNetMsg_LastManStanding_RestartTimer(char time)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LMS_RESTARTTIMER *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LMS_RESTARTTIMER);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LMS_RESTARTTIMER *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_LastManStanding_RestartCountDown;
	/* Fill in message. */
	messagePtr->timer = time;
}

void AddNetMsg_LastManStanding_Restart(NetID alienID, int seed)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_LMS_RESTART *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_LMS_RESTART);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_LMS_RESTART *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = NetMT_LastManStanding_Restart;
	/* Fill in message. */
	messagePtr->playerID = alienID;
	messagePtr->seed = seed;
}


void AddNetMsg_RespawnPickups(void)
{
	int headerSize = sizeof(NETMESSAGEHEADER);

	/* only send this if we are playing or on the end game screen*/
	if (netGameData.myGameState != NGS_Playing && netGameData.myGameState != NGS_EndGameScreen) {
		return;
	}

	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	NETMESSAGEHEADER *headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_RespawnPickups;
}

void AddNetMsg_SpotOtherSound(enum soundindex SoundIndex, VECTORCH *position, int explosion)
{
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_SPOTOTHERSOUND);
	/* check there's enough room in the send buffer */
	{
		int numBytesReqd = headerSize + messageSize;
		int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

		if (numBytesReqd > numBytesLeft)
		{
			NoteSendBufferFull(numBytesReqd);
			/* don't add it */
			return;
		}
	}
	/* set up pointers to header and message structures */
	NETMESSAGEHEADER *headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	NETMESSAGE_SPOTOTHERSOUND *messagePtr = (NETMESSAGE_SPOTOTHERSOUND *)endSendBuffer;
	endSendBuffer += messageSize;
	/* fill out the header */
	headerPtr->type = (unsigned char)NetMT_SpotOtherSound;
	/* Fill in message. */
	messagePtr->SoundIndex = SoundIndex;

	if (explosion) {
		messagePtr->explosion = 1;
	}
	else {
		messagePtr->explosion = 0;
	}

	messagePtr->vx = position->vx;
	messagePtr->vy = position->vy;
	messagePtr->vz = position->vz;
}

/*----------------------------------------------------------------------
  Functions for processing each message type, as retrieved from the read
  message buffer...
  ----------------------------------------------------------------------*/
static void ProcessNetMsg_GameDescription(NETMESSAGE_GAMEDESCRIPTION *messagePtr)
{
	OutputDebugString("ProcessNetMsg_GameDescription\n");

	/* should only get this if we're not the host, and we're in start-up state */
	if (AvP.Network != I_Peer)
	{
		//Vaguely possible that a host could receive a message that is only intended for peers
		//if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
		return;
	}

	/* fill out the game description player list with the new player id's */
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			int playerChanged = 0;

			if ((netGameData.playerData[i].playerId != messagePtr->players[i].playerId)
			    || (netGameData.playerData[i].startFlag != messagePtr->players[i].startFlag))
			{
				playerChanged = 1;
			}

			// One that timed out never said goodbye, so its body is taken away here, even if someone else has its place already
			if (netGameData.myGameState == NGS_Playing && netGameData.playerData[i].playerId != 0 && netGameData.playerData[i].playerId != messagePtr->players[i].playerId)
			{
				RemovePlayersGhosts(netGameData.playerData[i].playerId);
				Inform_PlayerHasLeft(netGameData.playerData[i].playerId);
			}

			if (netGameData.myGameState == NGS_Playing && playerChanged)
			{
				if (messagePtr->players[i].playerId == 0) {
					// Taken care of above
				}
				else if (messagePtr->players[i].startFlag) {
					Inform_PlayerHasJoined(messagePtr->players[i].playerId);
				}
				else {
					Inform_PlayerHasConnected(messagePtr->players[i].playerId);
				}
			}

			// This is how a client learns the other players' names
			NetCopyPlayerName(netGameData.playerData[i].name, messagePtr->players[i].name, NET_PLAYERNAMELENGTH);

			netGameData.playerData[i].playerId = messagePtr->players[i].playerId;
			// The message has room to say 3, which is no kind of player, and scores look things up by this
			netGameData.playerData[i].characterType = NetNumberFitsTable(messagePtr->players[i].characterType, NGCT_AI_Alien) ? (NETGAME_CHARACTERTYPE)messagePtr->players[i].characterType : NGCT_Marine;
			netGameData.playerData[i].characterSubType = NetNumberFitsTable(messagePtr->players[i].characterSubType, NUM_PC_SUBTYPES) ? (NETGAME_SPECIALISTCHARACTERTYPE)messagePtr->players[i].characterSubType : NGSCT_General;
			netGameData.playerData[i].startFlag = messagePtr->players[i].startFlag;
		}

		netGameData.gameType = (NETGAME_TYPE)messagePtr->gameType;

		// In the co-op lobby a joiner hears all this only from the host's choice, so that an old description can't undo a newer choice
		if (coopLobbyState == kCoopLobbyNone)
		{
			if (Coop_LevelChangeIsDue())
			{
				// The host's word to go on has come, and this description can be one it made before it moved. When the host
				// reaches a level's end, it adds the next-level message and sends at once, and that send can add the
				// once-a-second description behind it, still naming the old place. Written down, that old place would make this
				// machine load the level it has just finished, while the host loads the next. So while a move is due, the
				// campaign and the place stay the pair the next-level message gave and was checked against. The descriptions
				// sent while the next level loads name them too.
				static unsigned long long oldPlacesLeft = 0;

				if ((messagePtr->coopCampaign != netGameData.coopCampaign || messagePtr->coopCampaignLevel != netGameData.coopCampaignLevel) && LogThisDrop(oldPlacesLeft))
				{
					LogString("campaign: a game description naming campaign " + std::to_string(messagePtr->coopCampaign) + " place " + std::to_string(messagePtr->coopCampaignLevel) +
						" came after the word to go on to place " + std::to_string(netGameData.coopCampaignLevel) + ", so the campaign and place stay as they are (number " +
						std::to_string(oldPlacesLeft) + " of these)");
				}
			}
			// Where the campaign has got to picks a level from a list, so a number that isn't a real place means no campaign at all
			else if (NetCampaignPlaceIsGood(messagePtr->coopCampaign, messagePtr->coopCampaignLevel, Campaign_LevelCount(messagePtr->coopCampaign)))
			{
				// Someone playing a level who hears here that the campaign has gone on wasn't told the proper way. If this
				// only wrote the number down, he'd stay in the old level for good, with the others waiting for him. It
				// shouldn't ever happen, so the log says when it does.
				bool const leftBehind = netGameData.myGameState == NGS_Playing && AvP.MainLoopRunning
					&& NetCampaignMoveIsGood(messagePtr->coopCampaign, messagePtr->coopCampaignLevel, netGameData.coopCampaign, netGameData.coopCampaignLevel, Campaign_LevelCount(netGameData.coopCampaign));

				netGameData.coopCampaign = messagePtr->coopCampaign;

				if (leftBehind)
				{
					LogErrorString("campaign: the host has gone on to another level and nobody told this machine, so it's going on now");
					Coop_GoToPlace(messagePtr->coopCampaignLevel);
				}
				else
				{
					netGameData.coopCampaignLevel = messagePtr->coopCampaignLevel;
				}
			}
			else
			{
				netGameData.coopCampaign = kCampaignNone;
				netGameData.coopCampaignLevel = 0;
			}

			// It picks a row in the tables of damage and health, so it's checked in the same way. A bad number gets the
			// game's usual difficulty, not a refusal, because the rest of the description may be good.
			netGameData.coopDifficulty = NetCampaignDifficultyIsGood(messagePtr->coopDifficulty) ? messagePtr->coopDifficulty : I_Medium;

			// Any number at all is safe here: see shared_roll.h
			netGameData.coopRollNumber = messagePtr->coopRollNumber;

			// Any list is safe too: it only picks which names go on a line of text, and a place with nobody in it is passed over
			netGameData.coopWaitingFor = messagePtr->coopWaitingFor;
		}

		// level number got from the session description instead
		// netGameData.levelNumber = messagePtr->levelNumber;
		netGameData.scoreLimit = messagePtr->scoreLimit;
		netGameData.timeLimit = messagePtr->timeLimit;
		netGameData.invulnerableTime = messagePtr->invulnerableTime;

		for (int i = 0; i < 3; i++)
		{
			netGameData.characterKillValues[i] = messagePtr->characterKillValues[i];
			netGameData.aiKillValues[i] = messagePtr->aiKillValues[i];
		}

		netGameData.baseKillValue = messagePtr->baseKillValue;
		netGameData.useDynamicScoring = messagePtr->useDynamicScoring;
		netGameData.useCharacterKillValues = messagePtr->useCharacterKillValues;
		netGameData.sendDecals = messagePtr->sendDecals;
		netGameData.gameSpeed = messagePtr->gameSpeed;

		// One of the host's rows in the co-op lobby, like the campaign above
		if (coopLobbyState == kCoopLobbyNone)
		{
			netGameData.disableFriendlyFire = messagePtr->disableFriendlyFire;
		}

		netGameData.fallingDamage = messagePtr->fallingDamage;
		netGameData.pistolInfiniteAmmo = messagePtr->pistolInfiniteAmmo;
		netGameData.specialistPistols = messagePtr->specialistPistols;

		if (netGameData.needGameDescription)
		{
			/*We were waiting for the game description , best make sure that our player id appears
			int the player list*/
			if (PlayerIdInPlayerList(AvPNetID) != NET_IDNOTINPLAYERLIST)
			{
				/*we now have the game description, so we can stop waiting if we were
				trying to join*/
				netGameData.needGameDescription = 0;

				/*
				make sure our time scale is set correctly
				(Only set it the once , so that it can be overridden for debugging purposes)
				*/
				if (netGameData.coopCampaign != kCampaignNone)
				{
					netGameData.gameSpeed = NETGAMESPEED_100PERCENT;
				}

				switch (netGameData.gameSpeed)
				{
					case NETGAMESPEED_70PERCENT :
						TimeScale = (ONE_FIXED * 70) / 100;
						break;

					case NETGAMESPEED_80PERCENT :
						TimeScale = (ONE_FIXED * 80) / 100;
						break;

					case NETGAMESPEED_90PERCENT :
						TimeScale = (ONE_FIXED * 90) / 100;
						break;

					case NETGAMESPEED_100PERCENT :
						TimeScale = (ONE_FIXED * 100) / 100;
						break;
				}
			}
		}

		netGameData.allowSmartgun = messagePtr->allowSmartgun;
		netGameData.allowFlamer = messagePtr->allowFlamer;
		netGameData.allowSadar = messagePtr->allowSadar;
		netGameData.allowGrenadeLauncher = messagePtr->allowGrenadeLauncher;
		netGameData.allowMinigun = messagePtr->allowMinigun;
		netGameData.allowDisc = messagePtr->allowDisc;
		netGameData.allowPistol = messagePtr->allowPistol;
		netGameData.allowPlasmaCaster = messagePtr->allowPlasmaCaster;
		netGameData.allowSpeargun = messagePtr->allowSpeargun;
		netGameData.allowMedicomp = messagePtr->allowMedicomp;
		netGameData.allowSmartDisc = messagePtr->allowSmartDisc;
		netGameData.allowPistols = messagePtr->allowPistols;
		netGameData.allowShotgun = messagePtr->allowShotgun;
		netGameData.allowRevolver = messagePtr->allowRevolver;
		netGameData.allowM79 = messagePtr->allowM79;
		netGameData.allowStaff = messagePtr->allowStaff;
		netGameData.maxPredator = messagePtr->maxPredator;
		netGameData.maxAlien = messagePtr->maxAlien;
		netGameData.maxMarine = messagePtr->maxMarine;
		netGameData.maxMarineGeneral = messagePtr->maxMarineGeneral;
		netGameData.maxMarinePulseRifle = messagePtr->maxMarinePulseRifle;
		netGameData.maxMarineSmartgun = messagePtr->maxMarineSmartgun;
		netGameData.maxMarineFlamer = messagePtr->maxMarineFlamer;
		netGameData.maxMarineSadar = messagePtr->maxMarineSadar;
		netGameData.maxMarineGrenade = messagePtr->maxMarineGrenade;
		netGameData.maxMarineMinigun = messagePtr->maxMarineMinigun;
		netGameData.maxMarineSmartDisc = messagePtr->maxMarineSmartDisc;
		netGameData.maxMarinePistols = messagePtr->maxMarinePistols;
		netGameData.maxMarineShotgun = messagePtr->maxMarineShotgun;
		netGameData.maxMarineRevolver = messagePtr->maxMarineRevolver;
		netGameData.maxMarineM79 = messagePtr->maxMarineM79;
		netGameData.useSharedLives = messagePtr->useSharedLives;
		netGameData.maxLives = messagePtr->maxLives;
		netGameData.numDeaths[0] = messagePtr->numDeaths[0];
		netGameData.numDeaths[1] = messagePtr->numDeaths[1];
		netGameData.numDeaths[2] = messagePtr->numDeaths[2];
		netGameData.timeForRespawn = messagePtr->timeForRespawn;
		netGameData.pointsForRespawn = messagePtr->pointsForRespawn;
		{
			// check to if the host's elapsed time is
			// significantly different from our elapsed time value
			int receivedTime = messagePtr->GameTimeElapsed;
			int diff;
			receivedTime <<= 16;
			diff = netGameData.GameTimeElapsed - receivedTime;

			if (diff < -ONE_FIXED || diff > ONE_FIXED || netGameData.myGameState == NGS_EndGameScreen)
			{
				// best take the host's value
				netGameData.GameTimeElapsed = receivedTime;
			}
		}
	}

	if (messagePtr->endGame)
	{
		if (netGameData.myGameState == NGS_Playing)
		{
			// we must have missed the end game message
			netGameData.myGameState = NGS_EndGameScreen;
		}
	}
	else
	{
		if (netGameData.myGameState == NGS_EndGameScreen)
		{
			// must have missed message to restart game
			// therefore probably better restart now
			RestartNetworkGame(0);
		}
	}
}

static void ProcessNetMsg_PlayerDescription(NETMESSAGE_PLAYERDESCRIPTION *messagePtr, NetID senderId)
{
	// only act on this if we're the host and in start-up
	if (AvP.Network != I_Host) {
		return;
	}

	/* find the player and fill out their details from the message */
	{
		int id = PlayerIdInPlayerList(senderId);
		if (id == NET_IDNOTINPLAYERLIST)
		{
			// player does not seem to be in the player list, so ignore it
			return;
		}

		// The message has room for more than there are kinds of player, and scores look things up by this
		if (!NetNumberFitsTable(messagePtr->characterType, NGCT_AI_Alien))
		{
			return;
		}

		netGameData.playerData[id].characterType = (NETGAME_CHARACTERTYPE)messagePtr->characterType;
		netGameData.playerData[id].characterSubType = NetNumberFitsTable(messagePtr->characterSubType, NUM_PC_SUBTYPES) ? (NETGAME_SPECIALISTCHARACTERTYPE)messagePtr->characterSubType : NGSCT_General;

		if (netGameData.myGameState == NGS_Playing)
		{
			if (messagePtr->startFlag && (netGameData.playerData[id].startFlag != messagePtr->startFlag)) {
				Inform_PlayerHasJoined(netGameData.playerData[id].playerId);
			}
		}

		netGameData.playerData[id].startFlag = messagePtr->startFlag;
	}
}

static void ProcessNetMsg_StartGame(void)
{
	// only act on this if we're a peer and in start-up
	return;
#if 0
	if (AvP.Network != I_Peer) {
		return;
	}

	if (netGameData.myGameState != NGS_Joining) {
		return;
	}

	/* switch our game state... if we are in the player list, switch to start, otherwise
	switch to error */
	if (PlayerIdInPlayerList(AvPNetID) != NET_IDNOTINPLAYERLIST)
	{
		netGameData.myGameState = NGS_Playing;
	}
	else
	{
		TransmitPlayerLeavingNetMsg();
		netGameData.myGameState = NGS_Error_HostLost;
		AvP.MainLoopRunning = 0;
	}
#endif
}

static void ProcessNetMsg_PlayerState(NETMESSAGE_PLAYERSTATE *messagePtr, NetID senderId)
{
#if 0

	/* state check: if we're in startup and we've received this message from the host, we
	should go into an error state: */
	if ((netGameData.myGameState == NGS_Joining) && (messagePtr->IAmHost))
	{
		TransmitPlayerLeavingNetMsg();
		netGameData.myGameState = NGS_Error_HostLost;
		AvP.MainLoopRunning = 0;
	}

#endif
	// A message that says something impossible isn't trusted for anything else in it. Only a live marine, in the mode that plays as single player, has a lamp others see.
	if (messagePtr->lampOn && !(SameAsSinglePlayer() && messagePtr->characterType == NGCT_Marine && messagePtr->IAmAlive))
	{
		Net_NoteRefused("a player's message said his lamp is on, and he can't have one, so none of it was used");
		return;
	}

	VECTORCH position;
	position.vx = messagePtr->xPos;
	position.vy = messagePtr->yPos;
	position.vz = messagePtr->zPos;

	// recored the players position, even if we aren't currently playing
	int playerIndex = PlayerIdInPlayerList(senderId);
	if (playerIndex != NET_IDNOTINPLAYERLIST) {
		netGameData.playerData[playerIndex].lastKnownPosition = position;
	}

	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	playerIndex = PlayerIdInPlayerList(senderId);

	// KJL 14:47:22 06/04/98 - we don't seem to know about this person yet... ignore them
	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	// The message has room to say 3, which is no kind of player, and scores look things up by these
	if (!NetNumberFitsTable(messagePtr->characterType, NGCT_AI_Alien) || !NetNumberFitsTable(messagePtr->nextCharacterType, NGCT_AI_Alien))
	{
		return;
	}

	STRATEGYBLOCK *sbPtr = FindGhost(senderId, GHOST_PLAYEROBJECTID);
	// record whether the player is in the land of the living
	netGameData.playerData[playerIndex].playerAlive = messagePtr->IAmAlive;
	netGameData.playerData[playerIndex].playerHasLives = messagePtr->IHaveLifeLeft;
	// check the player type
	// the value in the netgamedata should be set to next character type
	netGameData.playerData[playerIndex].characterType = static_cast<NETGAME_CHARACTERTYPE>(messagePtr->nextCharacterType);
	netGameData.playerData[playerIndex].characterSubType = NetNumberFitsTable(messagePtr->characterSubType, NUM_PC_SUBTYPES) ? static_cast<NETGAME_SPECIALISTCHARACTERTYPE>(messagePtr->characterSubType) : NGSCT_General;

	if (sbPtr)
	{
		NETGHOSTDATABLOCK *ghostData;
		ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		GLOBALASSERT(ghostData);

		// here we need to use the current character type
		switch (messagePtr->characterType)
		{
			case NGCT_Marine :
				if (ghostData->type != I_BehaviourMarinePlayer)
				{
					sbPtr->SBflags.please_destroy_me = 1;
					return;
				}

				break;

			case NGCT_Predator :
				if (ghostData->type != I_BehaviourPredatorPlayer)
				{
					sbPtr->SBflags.please_destroy_me = 1;
					return;
				}

				break;

			case NGCT_Alien :
				if (ghostData->type != I_BehaviourAlienPlayer)
				{
					sbPtr->SBflags.please_destroy_me = 1;
					return;
				}

				break;
		}
	}

	if (!MultiplayerObservedPlayer)
	{
		if (sbPtr && sbPtr->SBdptr)
		{
			// make sure the model is visivle
			sbPtr->SBdptr->ObFlags &= ~ObFlag_NotVis;
		}
	}

	{
		EULER orientation;
		orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
		orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
		orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);
		int sequence = (int)messagePtr->sequence;
		int weapon = (int)messagePtr->currentWeapon;
		int firingPrimary = (int)messagePtr->IAmFiringPrimary;
		int firingSecondary = (int)messagePtr->IAmFiringSecondary;

		//ReleasePrintDebuggingText("Primary %d Secondary %d\n",firingPrimary,firingSecondary);

		if (!sbPtr)
		{
			/* If we are not a dead alien then we should have a ghost */
			//            if(!(((!(messagePtr->IAmAlive)))&&(netGameData.playerData[playerIndex].characterType==NGCT_Alien)))
			if (messagePtr->IAmAlive)
			{
				{
					AVP_BEHAVIOUR_TYPE type;

					if (messagePtr->characterType == NGCT_Marine) {
						type = I_BehaviourMarinePlayer;
					}
					else if (messagePtr->characterType == NGCT_Alien) {
						type = I_BehaviourAlienPlayer;
					}
					else {
						type = I_BehaviourPredatorPlayer;
					}

					sbPtr = CreateNetGhost(senderId, GHOST_PLAYEROBJECTID, &position, &orientation, type, IOT_Non, 0);

					if (sbPtr)
					{
						HandleWeaponElevation(sbPtr, (int)messagePtr->Elevation, weapon);

						// don't draw muzzle flash if observing from this player
						if (MultiplayerObservedPlayer != senderId) {
							HandleGhostGunFlashEffect(sbPtr, messagePtr->IHaveAMuzzleFlash);
						}
						else {
							HandleGhostGunFlashEffect(sbPtr, 0);
						}

						HandlePlayerGhostWeaponSound(sbPtr, weapon, firingPrimary, firingSecondary);
						MaintainGhostCloakingStatus(sbPtr, (int)messagePtr->CloakingEffectiveness);
						MaintainGhostFireStatus(sbPtr, (int)messagePtr->IAmOnFire);
					}
				}
			}
		}
		else
		{
			if (!(((!(messagePtr->IAmAlive))) && (netGameData.playerData[playerIndex].characterType == NGCT_Alien)))
			{
				// We are not a dead alien
				HandleWeaponElevation(sbPtr, (int)messagePtr->Elevation, weapon);
				UpdateGhost(sbPtr, &position, &orientation, sequence, messagePtr->Special);

				// don't draw muzzle flash if observing from this player
				if (MultiplayerObservedPlayer != senderId) {
					HandleGhostGunFlashEffect(sbPtr, messagePtr->IHaveAMuzzleFlash);
				}
				else {
					HandleGhostGunFlashEffect(sbPtr, 0);
				}

				HandlePlayerGhostWeaponSound(sbPtr, weapon, firingPrimary, firingSecondary);
				MaintainGhostCloakingStatus(sbPtr, (int)messagePtr->CloakingEffectiveness);
				MaintainGhostFireStatus(sbPtr, (int)messagePtr->IAmOnFire);
				/* Now, more disc. */

				if (messagePtr->IHaveADisk)
				{
					NETGHOSTDATABLOCK *ghostData;
					SECTION_DATA *disc;
					// Find the thrower's ghost, and add his disc...
					ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
					GLOBALASSERT(ghostData);
					GLOBALASSERT(ghostData->type == I_BehaviourPredatorPlayer);
					disc = GetThisSectionData(ghostData->HModelController.section_data, "disk");
					if (disc) {
						disc->flags &= ~section_data_notreal;
					}
				}
			}
			else
			{
				// We are a dead alien with a ghost
				RemoveGhost(sbPtr);
				return;
			}
		}

		if (sbPtr && messagePtr->IAmAlive)
		{
			NETGHOSTDATABLOCK *ghostData;
			ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
			ghostData->invulnerable = messagePtr->IAmInvulnerable;

			// Only stored. A message from anyone who can't have a lamp, saying it's on, was refused whole at the top.
			ghostData->lampOn = (messagePtr->lampOn && ghostData->type == I_BehaviourMarinePlayer) ? 1 : 0;
#if EXTRAPOLATION_TEST
			{
				VECTORCH velocity, diff;
				int playerTimer = netGameData.playerData[playerIndex].timer;
				velocity.vx = messagePtr->velocity_x * 100;
				velocity.vy = messagePtr->velocity_y * 100;
				velocity.vz = messagePtr->velocity_z * 100;
				diff = ghostData->velocity;
				SubVector(&velocity, &diff);

				if (Approximate3dMagnitude(&diff) > 1000)
				{
					// change in velocity , so reset extrapolation timer
					ghostData->extrapTimer = -ONE_FIXED;
				}

				ghostData->velocity = velocity;
				ghostData->extrapTimerLast = 0;

				if (playerTimer >= (int)ghostData->lastTimeRead) {
					ghostData->extrapTimer -= (playerTimer - ghostData->lastTimeRead);
				}
				else {
					ghostData->extrapTimer = -ONE_FIXED;
				}

				ghostData->lastTimeRead = playerTimer;
			}

			if (ghostData->type == I_BehaviourAlienPlayer)
			{
				sbPtr->DynPtr->UseStandardGravity = messagePtr->standard_gravity;

				if (!sbPtr->DynPtr->UseStandardGravity)
				{
					if (sbPtr->DynPtr->GravityDirection.vy == ONE_FIXED)
					{
						MATRIXCH mat;
						// alien is crawling , so we need to get an appropriate gravity direction
						CreateEulerMatrix(&orientation, &mat);
						sbPtr->DynPtr->GravityDirection.vx = mat.mat12;
						sbPtr->DynPtr->GravityDirection.vy = mat.mat22;
						sbPtr->DynPtr->GravityDirection.vz = mat.mat32;
					}

					sbPtr->DynPtr->LinImpulse.vx = 0;
					sbPtr->DynPtr->LinImpulse.vy = 0;
					sbPtr->DynPtr->LinImpulse.vz = 0;
					sbPtr->DynPtr->ToppleForce = TOPPLE_FORCE_ALIEN;
				}
			}

#endif

			if (messagePtr->scream != 31)
			{
				// this character is screaming
				switch (messagePtr->characterType)
				{
					case NGCT_Marine :
						PlayMarineScream(0, (SOUND_CATERGORY)messagePtr->scream, 0, &ghostData->SoundHandle2, &position);
						break;

					case NGCT_Alien :
						PlayAlienSound(0, (ALIEN_SOUND_CATEGORY)messagePtr->scream, 0, &ghostData->SoundHandle2, &position);
						break;

					case NGCT_Predator :
						if ((PREDATOR_SOUND_CATEGORY)messagePtr->scream == PSC_Medicomp_Special) {
							Sound_Play(SID_PRED_NEWROAR, "de", &position, &ghostData->SoundHandle2);
						}
						else {
							PlayPredatorSound(0, (PREDATOR_SOUND_CATEGORY)messagePtr->scream, 0, &ghostData->SoundHandle2, &position);
						}

						break;
				}
			}

			// Landing noise.
			if (messagePtr->landingNoise)
			{
				switch (messagePtr->characterType)
				{
					case NGCT_Marine :
						Sound_Play(SID_MARINE_SMALLLANDING, "d", &position);
						break;

					case NGCT_Alien :
						// FALSE sound for aliens.
						break;

					case NGCT_Predator :
						Sound_Play(SID_PRED_SMALLLANDING, "d", &position);
						break;
				}
			}

			// are we currently following this player's movements
			if (MultiplayerObservedPlayer)
			{
				if (MultiplayerObservedPlayer == senderId)
				{
					PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
					playerStatusPtr->ViewPanX = messagePtr->Elevation;
					Player->ObStrategyBlock->DynPtr->Position = position;
					Player->ObStrategyBlock->DynPtr->PrevPosition = position;
					Player->ObStrategyBlock->DynPtr->OrientEuler = orientation;
					CreateEulerMatrix(&Player->ObStrategyBlock->DynPtr->OrientEuler, &Player->ObStrategyBlock->DynPtr->OrientMat);
					TransposeMatrixCH(&Player->ObStrategyBlock->DynPtr->OrientMat);

					if (messagePtr->IAmCrouched) {
						PlayerStatusPtr->ShapeState = PMph_Crouching;
					}
					else {
						PlayerStatusPtr->ShapeState = PMph_Standing;
					}

					// don't draw the player we're observing
					if (sbPtr && sbPtr->SBdptr) {
						sbPtr->SBdptr->ObFlags |= ObFlag_NotVis;
					}
				}
			}
		}
	}
}
static void ProcessNetMsg_PlayerState_Minimal(NETMESSAGE_PLAYERSTATE_MINIMAL *messagePtr, NetID senderId, BOOL orientation)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	int playerIndex = PlayerIdInPlayerList(senderId);

	// KJL 14:47:22 06/04/98 - we don't seem to know about this person yet... ignore them
	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	STRATEGYBLOCK *sbPtr = FindGhost(senderId, GHOST_PLAYEROBJECTID);

	if (sbPtr && !IsAPlayersGhost(sbPtr))
	{
		Net_NoteRefused("a player message named a ghost that isn't a player");
		return;
	}

	// record whether the player is in the land of the living
	netGameData.playerData[playerIndex].playerAlive = messagePtr->IAmAlive;
	netGameData.playerData[playerIndex].playerHasLives = messagePtr->IHaveLifeLeft;

	if (!sbPtr)
	{
		// if we don't have a ghost for this player , wait for a full player state message
		return;
	}

	if (!MultiplayerObservedPlayer)
	{
		if (sbPtr && sbPtr->SBdptr)
		{
			// make sure the model is visivle
			sbPtr->SBdptr->ObFlags &= ~ObFlag_NotVis;
		}
	}

	{
		int firingPrimary = (int)messagePtr->IAmFiringPrimary;
		int firingSecondary = (int)messagePtr->IAmFiringSecondary;

		if (!(((!(messagePtr->IAmAlive))) && (netGameData.playerData[playerIndex].characterType == NGCT_Alien)))
		{
			NETGHOSTDATABLOCK *ghostData;
			ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
			GLOBALASSERT(ghostData);

			if (orientation)
			{
				NETMESSAGE_PLAYERSTATE_MEDIUM *mediumMessage = (NETMESSAGE_PLAYERSTATE_MEDIUM *) messagePtr;
				EULER orientation;
				orientation.EulerX = (mediumMessage->xOrient << NET_EULERSCALESHIFT);
				orientation.EulerY = (mediumMessage->yOrient << NET_EULERSCALESHIFT);
				orientation.EulerZ = (mediumMessage->zOrient << NET_EULERSCALESHIFT);
				UpdateGhost(sbPtr, &sbPtr->DynPtr->Position, &orientation, -1, messagePtr->Special);
			}

			// We are not a dead alien
			HandleWeaponElevation(sbPtr, (int)messagePtr->Elevation, ghostData->CurrentWeapon);

			// don't draw muzzle flash if observing from this player
			if (MultiplayerObservedPlayer != senderId) {
				HandleGhostGunFlashEffect(sbPtr, messagePtr->IHaveAMuzzleFlash);
			}
			else {
				HandleGhostGunFlashEffect(sbPtr, 0);
			}

			MaintainGhostCloakingStatus(sbPtr, (int)messagePtr->CloakingEffectiveness << 8);
			MaintainGhostFireStatus(sbPtr, (int)messagePtr->IAmOnFire);
			/* Now, more disc. */

			if (messagePtr->IHaveADisk)
			{
				SECTION_DATA *disc;
				// Find the thrower's ghost, and add his disc...
				GLOBALASSERT(ghostData->type == I_BehaviourPredatorPlayer);
				disc = GetThisSectionData(ghostData->HModelController.section_data, "disk");

				if (disc) {
					disc->flags &= ~section_data_notreal;
				}
			}
		}
		else
		{
			// We are a dead alien with a ghost
			RemoveGhost(sbPtr);
			return;
		}

		if (sbPtr && messagePtr->IAmAlive)
		{
			NETGHOSTDATABLOCK *ghostData;
			ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;

			// are we currently following this player's movements
			if (MultiplayerObservedPlayer)
			{
				if (MultiplayerObservedPlayer == senderId)
				{
					PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
					playerStatusPtr->ViewPanX = messagePtr->Elevation;

					// don't draw the player we're observing
					if (sbPtr && sbPtr->SBdptr) {
						sbPtr->SBdptr->ObFlags |= ObFlag_NotVis;
					}
				}
			}
		}
	}
}

static void ProcessNetMsg_FrameTimer(unsigned short frame_time, NetID senderId)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	int senderPlayerIndex = PlayerIdInPlayerList(senderId);

	if (senderPlayerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	netGameData.playerData[senderPlayerIndex].timer += frame_time;
}

static void ProcessNetMsg_PlayerKilled(NETMESSAGE_PLAYERKILLED *messagePtr, NetID senderId)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// These go on to be places in the tables of scores and deaths. The one who died is a player, one of the kinds before the computer's aliens, and the killer can be any kind there is.
	if (!NetNumberFitsTable((int)messagePtr->myType, NGCT_AI_Alien) || !NetNumberFitsTable((int)messagePtr->killerType, NGCT_AI_Praetorian + 1)) {
		return;
	}

	/* if this player is not in the play list, messages are probably out of order,
	so just ignore it... */
	int senderPlayerIndex = PlayerIdInPlayerList(senderId);
	if (senderPlayerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	// find the ghost for this player
	STRATEGYBLOCK *sbPtr = FindGhost(senderId, GHOST_PLAYEROBJECTID);
	if (!sbPtr)
	{
		/* we don't have a ghost for this player, which is odd, and implies that messages
		have got dis-ordered... best just ignore it then */
		return;
	}

	// The number names the new corpse. A corpse with the number of the player's body would go on being treated as the player's body, by later messages that were never meant for a corpse.
	if (messagePtr->objectId == GHOST_PLAYEROBJECTID)
	{
		Net_NoteRefused("a kill message gave the corpse the number of a player's body");
		return;
	}

	/* we have a ghost for this player: remove it if it's an alien */
	//  if(netGameData.playerData[senderPlayerIndex].characterType==NGCT_Alien)
	// RemoveGhost(sbPtr);
	KillGhost(sbPtr, messagePtr->objectId);
	Inform_PlayerHasDied(messagePtr->killerId, senderId, messagePtr->killerType, messagePtr->weaponIcon);

	// now attempt to update the scores
	if (AvP.Network == I_Host) {
		UpdateNetworkGameScores(senderId, messagePtr->killerId, messagePtr->myType, messagePtr->killerType);
	}

	// do some sound...
	LOCALASSERT(sbPtr->DynPtr);

	switch (netGameData.playerData[senderPlayerIndex].characterType)
	{
		case (NGCT_Alien):
		{
			PlayAlienSound(0, ASC_Scream_Dying, 0, NULL, &(sbPtr->DynPtr->Position));
			break;
		}

		case (NGCT_Marine):
		{
			PlayMarineScream(0, SC_Death, 0, NULL, &(sbPtr->DynPtr->Position));
			break;
		}

		case (NGCT_Predator):
		{
			PlayPredatorSound(0, PSC_Scream_Dying, 0, NULL, &(sbPtr->DynPtr->Position));
			break;
		}

		default:
		{
			LOCALASSERT(1 == 0);
			break;
		}
	}
}

static void ProcessNetMsg_PlayerDeathAnim(NETMESSAGE_CORPSEDEATHANIM *messagePtr, NetID senderId)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// find the ghost for this player
	STRATEGYBLOCK *sbPtr = FindGhost(senderId, messagePtr->objectId);

	if (!sbPtr)
	{
		/* we don't have a ghost for this player, which is odd, and implies that messages
		have got dis-ordered... best just ignore it then */
		return;
	}

	// set the death animation for this player
	ApplyGhostCorpseDeathAnim(sbPtr, messagePtr->deathId);
}

static void ProcessNetMsg_AllGameScores(NETMESSAGE_ALLGAMESCORES *messagePtr)
{
	// should only get this if we're not the host
	if (AvP.Network != I_Peer)
	{
		// Vaguely possible that a host could receive a message that is only intended for peers
		// if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
		return;
	}

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// fill in the game scores from the message
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			for (int j = 0; j < NET_MAXPLAYERS; j++) {
				netGameData.playerData[i].playerFrags[j] = messagePtr->playerFrags[i][j];
			}

			netGameData.playerData[i].playerScore = messagePtr->playerScores[i];
			netGameData.playerData[i].playerScoreAgainst = messagePtr->playerScoresAgainst[i];

			for (int j = 0; j < 3; j++) {
				netGameData.playerData[i].aliensKilled[j] = messagePtr->aliensKilled[i][j];
			}

			netGameData.playerData[i].deathsFromAI = messagePtr->deathsFromAI[i];
		}
	}
	netGameData.myGameState = NGS_EndGameScreen;
}

static void ProcessNetMsg_PlayerScores(NETMESSAGE_PLAYERSCORES *messagePtr)
{
	// should only get this if we're not the host
	if (AvP.Network != I_Peer)
	{
		// Vaguely possible that a host could receive a message that is only intended for peers
		// if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
		return;
	}

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// find the player. This is a place in the list, whatever it's called, and it's whatever the message says.
	int playerId = (int)messagePtr->playerId;

	if (!NetNumberFitsTable(playerId, NET_MAXPLAYERS))
	{
		return;
	}

	// fill in the player's scores from the message
	for (int i = 0; i < NET_MAXPLAYERS; i++) {
		netGameData.playerData[playerId].playerFrags[i] = messagePtr->playerFrags[i];
	}

	netGameData.playerData[playerId].playerScore = messagePtr->playerScore;
	netGameData.playerData[playerId].playerScoreAgainst = messagePtr->playerScoreAgainst;

	for (int i = 0; i < 3; i++) {
		netGameData.playerData[playerId].aliensKilled[i] = messagePtr->aliensKilled[i];
	}

	netGameData.playerData[playerId].deathsFromAI = messagePtr->deathsFromAI;
}

static void ProcessNetMsg_ScoreChange(NETMESSAGE_SCORECHANGE *messagePtr)
{
	// should only get this if we're not the host
	if (AvP.Network != I_Peer)
	{
		// Vaguely possible that a host could receive a message that is only intended for peers
		// if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
		return;
	}

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// These are places in the list, and they're whatever the message says. The killer can also be one past the end, which means an alien the computer plays.
	if (!NetNumberFitsTable(messagePtr->victimIndex, NET_MAXPLAYERS) || !NetNumberFitsTable(messagePtr->killerIndex, NET_MAXPLAYERS + 1))
	{
		return;
	}

	if (messagePtr->killerIndex == NET_MAXPLAYERS)
	{
		// killed by ai
		netGameData.playerData[messagePtr->victimIndex].deathsFromAI = messagePtr->fragCount;
	}
	else
	{
		netGameData.playerData[messagePtr->killerIndex].playerFrags[messagePtr->victimIndex] = messagePtr->fragCount;
		netGameData.playerData[messagePtr->killerIndex].playerScore = messagePtr->killerScoreFor;
	}

	netGameData.playerData[messagePtr->victimIndex].playerScoreAgainst = messagePtr->victimScoreAgainst;
}

static void ProcessNetMsg_SpeciesScores(NETMESSAGE_SPECIESSCORES *messagePtr)
{
	// should only get this if we're not the host
	if (AvP.Network != I_Peer)
	{
		// Vaguely possible that a host could receive a message that is only intended for peers
		// if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
		return;
	}

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	for (int i = 0; i < 3; i++) {
		netGameData.teamScores[i] = messagePtr->teamScores[i];
	}
}

static void ProcessNetMsg_LocalRicochet(NETMESSAGE_LOCALRICOCHET *messagePtr)
{
}

static void ProcessNetMsg_LocalObjectState(NETMESSAGE_LOBSTATE *messagePtr, NetID senderId)
{
	/* only do this if we're playing */
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* get the object id from the message */
	int objectId = (int)messagePtr->objectId;

	// -1 means a player's own body, which has its own message, and this one could move it, or make a thing with its number. Either way nothing honest sends it.
	if (objectId == GHOST_PLAYEROBJECTID)
	{
		Net_NoteRefused("a ghost message used the number of a player's body");
		return;
	}

	STRATEGYBLOCK *sbPtr = FindGhost(senderId, objectId);

	if (!sbPtr)
	{
		// Someone can later walk over this, and the game then uses this number as a place in its table of ammunition or of weapons. See inventory.cpp.
		if ((messagePtr->IOType == IOT_Ammo && !NetNumberFitsTable(messagePtr->subtype, MAX_NO_OF_AMMO_TEMPLATES)) || (messagePtr->IOType == IOT_Weapon && !NetNumberFitsTable(messagePtr->subtype, MAX_NO_OF_WEAPON_TEMPLATES)))
		{
			return;
		}

		/* we don't seem to have a ghost for this object, so create one */
		VECTORCH position;
		EULER orientation;
		AVP_BEHAVIOUR_TYPE type;
		position.vx = messagePtr->xPos;
		orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
		position.vy = messagePtr->yPos;
		orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
		position.vz = messagePtr->zPos;
		orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);
		type = (AVP_BEHAVIOUR_TYPE)messagePtr->type;

		// A corpse says where it is, as everything does, and this machine doesn't make corpses from that. One can arrive here honestly just after a level restarts, when the corpse has gone, so it isn't worth a line in the log.
		if (type == I_BehaviourNetCorpse)
		{
			return;
		}

		// The kind is the sender's word
		if (!GhostKindCanBeMadeByMessage(type))
		{
			Net_NoteRefused("a ghost message named a kind that isn't on the list");
			return;
		}

		// A flood of these would use up the game's 1000 blocks, which everything in a level needs
		int const sender = PlayerIdInPlayerList(senderId);

		if (sender == NET_IDNOTINPLAYERLIST || !NetThisSecondHasRoom(timeGetTime(), &newGhostsSecond[sender], &newGhostsUsed[sender], kMostNewGhostsEachSecond))
		{
			Net_NoteRefused("too many new ghosts from one player in a second");
			return;
		}

		// Only the kinds a message can make count. A player's body and the computer's fighters are made some other way, and don't use this up.
		int liveThings = 0;

		for (int i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const block = ActiveStBlockList[i];

			if (block && block->I_SBtype == I_BehaviourNetGhost && block->SBdataptr)
			{
				NETGHOSTDATABLOCK const *const liveGhost = (NETGHOSTDATABLOCK const *)block->SBdataptr;

				if (liveGhost->playerId == senderId && GhostKindCanBeMadeByMessage(liveGhost->type))
				{
					liveThings++;
				}
			}
		}

		if (liveThings >= kMostLiveThingsOfOnePlayer)
		{
			Net_NoteRefused("a player has as many things alive as one player may");
			return;
		}

		/* NB there is no sequence required for local objects, so just pass zero */
		sbPtr = CreateNetGhost(senderId, objectId, &position, &orientation, type, messagePtr->IOType, messagePtr->subtype);

		if (type == I_BehaviourPredatorDisc_SeekTrack)
		{
			NETGHOSTDATABLOCK *ghostData;
			STRATEGYBLOCK *sbPtr2;
			SECTION_DATA *disc;

			/* Find the thrower's ghost, and rip off his disc :-) */
			int playerIndex = PlayerIdInPlayerList(senderId);
			if (playerIndex == NET_IDNOTINPLAYERLIST)
			{
				return;
			}

			sbPtr2 = FindGhost(senderId, GHOST_PLAYEROBJECTID);
			if (!sbPtr2) {
				return;
			}

			/* Got 'em. */
			ghostData = (NETGHOSTDATABLOCK *)sbPtr2->SBdataptr;
			GLOBALASSERT(ghostData);

			disc = GetThisSectionData(ghostData->HModelController.section_data, "disk");
			if (!disc) {
				return;
			}

			disc->flags |= section_data_notreal;
		}
	}
	else
	{
		/* update the ghost... */
		VECTORCH position;
		EULER orientation;
		position.vx = messagePtr->xPos;
		orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
		position.vy = messagePtr->yPos;
		orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
		position.vz = messagePtr->zPos;
		orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);
		/* NB don't need to update the object type */
		{
			NETGHOSTDATABLOCK *ghostData;
			ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;

			if (messagePtr->event_flag)
			{
				if (ghostData->type == I_BehaviourPredatorDisc_SeekTrack)
				{
					if (messagePtr->event_flag == 1)
					{
						Convert_DiscGhost_To_PickupGhost(sbPtr);
					}
					else if (messagePtr->event_flag == 2)
					{
						// disc has bounced off a wall , so play appropriate sound
						Sound_Play(SID_PREDATOR_DISK_HITTING_WALL, "dp", &position, ((FastRandom() & 511) - 255));
					}
				}
				else if (ghostData->type == I_BehaviourFrisbee)
				{
					if (messagePtr->event_flag == 2)
					{
						// disc has bounced off a wall , so play appropriate sound
						Sound_Play(SID_ED_SKEETERDISC_HITWALL, "dp", &position, ((FastRandom() & 511) - 255));
					}
				}
				else if (ghostData->type == I_BehaviourFlareGrenade)
				{
					// flare has hit wall , so start the flare sound
					if (ghostData->SoundHandle == SOUND_NOACTIVEINDEX) {
						Sound_Play(SID_BURNING_FLARE, "dle", &position, &ghostData->SoundHandle);
					}
				}
				else if ((ghostData->type == I_BehaviourGrenade) || (ghostData->type == I_BehaviourClusterGrenade))
				{
					// Bounce sound.
					Sound_Play(SID_GRENADE_BOUNCE, "dp", &position, ((FastRandom() & 511) - 255));
				}
			}

			// NB there is no sequence required for local objects, so just pass zero
			UpdateGhost(sbPtr, &position, &orientation, 0, 0);
		}
	}
}

static int GetSizeOfLocalObjectDamagedMessage(char *messagePtr)
{
	int size = sizeof(NETMESSAGE_LOBDAMAGED_HEADER);
	NETMESSAGE_LOBDAMAGED_HEADER *messageHeader = (NETMESSAGE_LOBDAMAGED_HEADER *) messagePtr;

	if (messageHeader->damageProfile) {
		size += sizeof(NETMESSAGE_DAMAGE_PROFILE);
	}

	if (messageHeader->multiple) {
		size += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
	}

	if (messageHeader->sectionID) {
		size += sizeof(NETMESSAGE_DAMAGE_SECTION);
	}

	if (messageHeader->delta_seq) {
		size += sizeof(NETMESSAGE_DAMAGE_DELTA);
	}

	if (messageHeader->direction) {
		size += sizeof(NETMESSAGE_DAMAGE_DIRECTION);
	}

	return size;
}

// A damage message says which ammunition did the damage, in a number that can be anything from -1024 to 1023, because another player's machine wrote it. If
// the message leaves out its damage figures, they come from that number's place in the ammunition table, so a number with no place in the table would read
// whatever is in memory there. A real game can't send one: for any number outside the table, the sender includes the figures. So a message like this didn't
// come from a real game, and it's ignored. Anyone can send these as fast as they like, so the log only gets a few of them.
static void LogIgnoredDamageMessage(int ammoNumber)
{
	static unsigned long long ignored = 0;

	if (LogThisDrop(ignored))
	{
		char line[128];
		_snprintf(line, sizeof(line) - 1, "network: ignored a damage message naming ammunition %d, which isn't in the table (%llu so far)", ammoNumber, ignored);
		line[sizeof(line) - 1] = 0;
		LogErrorStringC(line);
	}
}

static void ProcessNetMsg_LocalObjectDamaged(char *messagePtr, NetID senderId)
{
	NETMESSAGE_LOBDAMAGED_HEADER *messageHeader = 0;
	NETMESSAGE_DAMAGE_PROFILE *messageProfile = 0;
	NETMESSAGE_DAMAGE_MULTIPLE *messageMultiple = 0;
	NETMESSAGE_DAMAGE_SECTION *messageSection = 0;
	NETMESSAGE_DAMAGE_DELTA *messageDelta = 0;
	NETMESSAGE_DAMAGE_DIRECTION *messageDirection = 0;

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	messageHeader = (NETMESSAGE_LOBDAMAGED_HEADER *) messagePtr;
	messagePtr += sizeof(NETMESSAGE_LOBDAMAGED_HEADER);

	// find out which elements of the damage message have been sent
	if (messageHeader->damageProfile)
	{
		messageProfile = (NETMESSAGE_DAMAGE_PROFILE *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_PROFILE);
	}

	if (messageHeader->multiple)
	{
		messageMultiple = (NETMESSAGE_DAMAGE_MULTIPLE *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
	}

	if (messageHeader->sectionID)
	{
		messageSection = (NETMESSAGE_DAMAGE_SECTION *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_SECTION);
	}

	if (messageHeader->delta_seq)
	{
		messageDelta = (NETMESSAGE_DAMAGE_DELTA *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_DELTA);
	}

	if (messageHeader->direction)
	{
		messageDirection = (NETMESSAGE_DAMAGE_DIRECTION *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_DIRECTION);
	}

	/* This message is for the player who owns the object, so first check
	if the message is meant for us */
	if (messageHeader->playerId != AvPNetID)
	{
		// however we may need to play a delta sequence on the ghost
		if (messageDelta)
		{
			if (messageDelta->Delta_Sequence > 0 && messageDelta->Delta_Sub_Sequence > 0)
			{
				STRATEGYBLOCK *sbPtr;
				sbPtr = FindGhost(messageHeader->playerId, (int)messageHeader->objectId);

				if (sbPtr) {
					PlayHitDeltaOnGhost(sbPtr, messageDelta->Delta_Sequence, messageDelta->Delta_Sub_Sequence);
				}
			}
		}

		return;
	}

	// This comes before anything is changed, so that a message that's ignored can't be blamed for a kill. This ammunition has its figures in a place of its own.
	if (!messageProfile && messageHeader->ammo_id != AMMO_FLECHETTE_POSTMAX && !NetNumberFitsTable(messageHeader->ammo_id, MAX_NO_OF_AMMO_TEMPLATES))
	{
		LogIgnoredDamageMessage(messageHeader->ammo_id);
		return;
	}

	// next we have to find this object in our strategyblock list
	{
		int objectId;
		STRATEGYBLOCK *sbPtr;
		DAMAGE_PROFILE damage;
		SECTION_DATA *section_data;
		HMODELCONTROLLER *controller;
		int multiple;
		VECTORCH incoming;
		VECTORCH direction;
		VECTORCH *incoming_ptr = 0;
		objectId = (int)messageHeader->objectId;
		sbPtr = FindObjectFromNetIndex(objectId);
		/* we set this global to remember the id of the player who sent this damage message,
		so that if the player is killed, we know the id of the killer. this is a bit of
		a nasty hack, but means that we don't have to make any changes to the core damage functions */
		LOCALASSERT(myNetworkKillerId == 0 || myNetworkKillerId == AvPNetID);

		/*
		Don't bother setting the killer id for molotov damage. This is because the only source of this damage
		is explosions from flamethrowers. This damage will always come from the net host , and we don't want to credit the
		host for all kills caused by this damage.

		Similarly all things that cause falling damage should count as suicide (in particular platform lifts)
		*/
		// In the mode that plays like single player the host runs every NPC, so a hit from the host is usually an NPC's, and the message can't say which.
		// So the host's name doesn't go on the kill. Nothing is scored in that mode, so nothing is lost when it really was the host's player.
		bool const couldBeAnNpc = SameAsSinglePlayer() && senderId == Net_HostPlayerID();

		if (messageHeader->ammo_id != AMMO_MOLOTOV && messageHeader->ammo_id != AMMO_FALLING_POSTMAX && !couldBeAnNpc) {
			myNetworkKillerId = senderId;
		}

		/* check if we have found an sb: if not the object has probably been
		destroyed already, so just ignore it */
		if (sbPtr)
		{
			// fill out damage profile
			damage.Id = static_cast<enum AMMO_ID>(messageHeader->ammo_id);

			if (messageProfile)
			{
				damage.Impact = messageProfile->Impact;
				damage.Cutting = messageProfile->Cutting;
				damage.Penetrative = messageProfile->Penetrative;
				damage.Fire = messageProfile->Fire;
				damage.Electrical = messageProfile->Electrical;
				damage.Acid = messageProfile->Acid;
				damage.ExplosivePower = messageProfile->ExplosivePower;
				damage.Slicing = messageProfile->Slicing;
				damage.ProduceBlood = messageProfile->ProduceBlood;
				damage.ForceBoom = messageProfile->ForceBoom;
				damage.BlowUpSections = messageProfile->BlowUpSections;
				damage.Special = messageProfile->Special;
				damage.MakeExitWounds = messageProfile->MakeExitWounds;
			}
			else
			{
				if (damage.Id == AMMO_FLECHETTE_POSTMAX)
				{
					damage = FlechetteDamage;
				}
				else
				{
					// the number has been checked above
					damage = TemplateAmmo[damage.Id].MaxDamage[AvP.Difficulty];
				}
			}

			if (messageMultiple) {
				multiple = messageMultiple->multiple;
			}
			else {
				multiple = ONE_FIXED;
			}

			if (!NetDamageMultipleIsGood(multiple))
			{
				Net_NoteRefused("a damage message counted its hit a number of times it can't");
				return;
			}

			section_data = NULL;
			controller = NULL;

			// get the direction vector if there is one
			if (sbPtr->DynPtr && messageDirection)
			{
				if (messageDirection->direction_x ||
				    messageDirection->direction_y ||
				    messageDirection->direction_z)
				{
					MATRIXCH mat = sbPtr->DynPtr->OrientMat;
					TransposeMatrixCH(&mat);
					// extract the direction vector
					incoming.vx = messageDirection->direction_x;
					incoming.vy = messageDirection->direction_y;
					incoming.vz = messageDirection->direction_z;
					// normalise it
					Normalise(&incoming);
					direction = incoming;
					// and rotate it from world space to the object's local space
					RotateVector(&incoming, &mat);
					// set the incoming pointer
					incoming_ptr = &incoming;
				}
			}

			/*Record the id of the hit body part in a global variable.
			This way, if this blow kill the player, we know which body part has been hit*/
			if (messageSection) {
				MyHitBodyPartId = messageSection->SectionID;
			}
			else {
				MyHitBodyPartId = -1;
			}

			if (messageSection && messageSection->SectionID != -1)
			{
				// The kinds the host runs, each with its own model
				controller = Npc_HModelController(sbPtr);

				if (controller)
				{
					section_data = GetThisSectionData_FromID(controller->section_data, (int)messageSection->SectionID);
				}
				else if (sbPtr->I_SBtype == I_BehaviourNetCorpse)
				{
					NETCORPSEDATABLOCK *corpseData = (NETCORPSEDATABLOCK *)sbPtr->SBdataptr;
					GLOBALASSERT(corpseData);
					controller = &corpseData->HModelController;
					section_data = GetThisSectionData_FromID(corpseData->HModelController.section_data,
					               (int)messageSection->SectionID);
				}
			}

			if (section_data)
			{
				DISPLAYBLOCK *fragged_section = 0;
				fragged_section = CauseDamageToHModel(controller, sbPtr, (&damage),
				                                      multiple, section_data, incoming_ptr, NULL, 0);

				if (fragged_section && damage.Id == AMMO_PRED_RIFLE && incoming_ptr)
				{
					// a speargun has fragged off a body part , so we need to create a spear
					CreateSpearPossiblyWithFragment(fragged_section, &fragged_section->ObWorld, &direction);
				}
			}
			else {
				CauseDamageToObject(sbPtr, (&damage), multiple, incoming_ptr);
			}
		}

		myNetworkKillerId = AvPNetID;
		MyHitBodyPartId = -1;
	}
}


static void ProcessNetMsg_LocalObjectDestroyed_Request(NETMESSAGE_LOBDESTROYED_REQUEST *messagePtr, NetID senderId)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* This message is for the player who owns the object, so first check
	if the message is meant for us */
	if (messagePtr->playerId != AvPNetID) {
		return;
	}

	// next we have to find this object in our strategyblock list
	int objectId = (int)messagePtr->objectId;
	STRATEGYBLOCK *sbPtr = FindObjectFromNetIndex(objectId);

	/* check if we have found an sb: if not the object has probably been
	destroyed already, so just ignore it */
	if (sbPtr)
	{
		// Er... deal with it, okay?
		if (sbPtr->I_SBtype == I_BehaviourInanimateObject) {
			RemovePickedUpObject(sbPtr);
		}
		else {
			GLOBALASSERT(0);
		}
	}
}

static void ProcessNetMsg_LocalObjectDestroyed(NETMESSAGE_LOBDESTROYED *messagePtr, NetID senderId)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* this message is a cue to destroy a ghost... except for the player which has
	a seperate 'playerkilled' message... */
	/* start by finding the ghost for this object */
	int objectId = (int)messagePtr->objectId;
	STRATEGYBLOCK *sbPtr = FindGhost(senderId, objectId);

	if (!sbPtr)
	{
		/* if we haven't got a ghost for this object things have gone wrong: eg, we haven't
		yet received the object creation message. Just ignore it then... */
		return;
	}

	RemoveGhost(sbPtr);
}


static int GetSizeOfInanimateDamagedMessage(char *messagePtr)
{
	int size = sizeof(NETMESSAGE_INANIMATEDAMAGED_HEADER);
	NETMESSAGE_INANIMATEDAMAGED_HEADER *messageHeader = (NETMESSAGE_INANIMATEDAMAGED_HEADER *) messagePtr;

	if (messageHeader->damageProfile) {
		size += sizeof(NETMESSAGE_DAMAGE_PROFILE);
	}

	if (messageHeader->multiple) {
		size += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
	}

	return size;
}

static void ProcessNetMsg_InanimateObjectDamaged(char *messagePtr)
{
	NETMESSAGE_INANIMATEDAMAGED_HEADER *messageHeader = 0;
	NETMESSAGE_DAMAGE_PROFILE *messageProfile = 0;
	NETMESSAGE_DAMAGE_MULTIPLE *messageMultiple = 0;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// only process this if we're the host
	if (AvP.Network != I_Host) {
		return;
	}

	messageHeader = (NETMESSAGE_INANIMATEDAMAGED_HEADER *) messagePtr;
	messagePtr += sizeof(NETMESSAGE_INANIMATEDAMAGED_HEADER);

	// find out which elements of the damage message have been sent
	if (messageHeader->damageProfile)
	{
		messageProfile = (NETMESSAGE_DAMAGE_PROFILE *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_PROFILE);
	}

	if (messageHeader->multiple)
	{
		messageMultiple = (NETMESSAGE_DAMAGE_MULTIPLE *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
	}

	// start by finding the object
	STRATEGYBLOCK *sbPtr = FindBreakableFromName(messageHeader->name);

	if (!sbPtr)
	{
		/* if we haven't got a ghost for this object things have gone wrong: eg, we haven't
		yet received the object creation message. Just ignore it then... */
		return;
	}

	// ok: cause the damage
	{
		STRATEGYBLOCK *objectPtr = FindBreakableFromName(messageHeader->name);
		DAMAGE_PROFILE damage;
		int multiple;

		if (!objectPtr) {
			return;    // couldn't find it
		}

		// fill out damage profile
		damage.Id = static_cast<enum AMMO_ID>(messageHeader->ammo_id);

		if (messageProfile)
		{
			damage.Impact = messageProfile->Impact;
			damage.Cutting = messageProfile->Cutting;
			damage.Penetrative = messageProfile->Penetrative;
			damage.Fire = messageProfile->Fire;
			damage.Electrical = messageProfile->Electrical;
			damage.Acid = messageProfile->Acid;
			damage.ExplosivePower = messageProfile->ExplosivePower;
			damage.Slicing = messageProfile->Slicing;
			damage.ProduceBlood = messageProfile->ProduceBlood;
			damage.ForceBoom = messageProfile->ForceBoom;
			damage.BlowUpSections = messageProfile->BlowUpSections;
			damage.Special = messageProfile->Special;
			damage.MakeExitWounds = messageProfile->MakeExitWounds;
		}
		else
		{
			if (!NetNumberFitsTable(damage.Id, MAX_NO_OF_AMMO_TEMPLATES))
			{
				LogIgnoredDamageMessage(damage.Id);
				return;
			}

			damage = TemplateAmmo[damage.Id].MaxDamage[AvP.Difficulty];
		}

		// get damage multiple
		if (messageMultiple) {
			multiple = messageMultiple->multiple;
		}
		else {
			multiple = ONE_FIXED;
		}

		if (!NetDamageMultipleIsGood(multiple))
		{
			Net_NoteRefused("a damage message counted its hit a number of times it can't");
			return;
		}

		CauseDamageToObject(sbPtr, (&damage), multiple, NULL);
	}
}

static void ProcessNetMsg_InanimateObjectDestroyed(NETMESSAGE_INANIMATEDESTROYED *messagePtr)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// only peers should get this
	LOCALASSERT(AvP.Network != I_Host);
	// start by finding the object
	STRATEGYBLOCK *sbPtr = FindBreakableFromName(messagePtr->name);

	if (!sbPtr)
	{
		/* if we haven't got a ghost for this object things have gone wrong: eg, we haven't
		yet received the object creation message. Just ignore it then... */
		return;
	}

	// ok: drop a nuke on it
	InanimateDamageFromNetHost = 1;
	CauseDamageToObject(sbPtr, &certainDeath, ONE_FIXED, NULL);
	InanimateDamageFromNetHost = 0;
}

static void ProcessNetMsg_ObjectPickedUp(NETMESSAGE_OBJECTPICKEDUP *messagePtr)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	STRATEGYBLOCK *objectPtr = FindEnvironmentObjectFromName(messagePtr->name);

	if (!objectPtr) {
		return;    // couldn't find it
	}

	// The name finds lights, switches and lifts as well as things that can be picked up. Treating one of those as a pick-up would write over its data.
	if (objectPtr->I_SBtype != I_BehaviourInanimateObject)
	{
		Net_NoteRefused("a pick-up message named something that can't be picked up");
		return;
	}

	// Play an appropriate sound?
	{
		INANIMATEOBJECT_STATUSBLOCK *objStatPtr = static_cast<INANIMATEOBJECT_STATUSBLOCK *>(objectPtr->SBdataptr);

		// patrick, for e3- add a sound effect to explosions
		if (objectPtr->DynPtr)
		{
			switch (objStatPtr->typeId)
			{
				case (IOT_Weapon):
					// Ignore predator case for now!
					Sound_Play(SID_MARINE_PICKUP_WEAPON, "%d", &objectPtr->DynPtr->Position);
					break;

				case (IOT_Ammo):
					Sound_Play(SID_MARINE_PICKUP_AMMO, "%d", &objectPtr->DynPtr->Position);
					break;

				case (IOT_Armour):
					Sound_Play(SID_MARINE_PICKUP_ARMOUR, "%d", &objectPtr->DynPtr->Position);
					break;

				case (IOT_FieldCharge):
					Sound_Play(SID_PREDATOR_PICKUP_FIELDCHARGE, "%d", &objectPtr->DynPtr->Position);
					break;

				default:
					Sound_Play(SID_PICKUP, "%d", &objectPtr->DynPtr->Position);
					break;
			}
		}
	}
	KillInanimateObjectForRespawn(objectPtr);
}

static void ProcessNetMsg_EndGame(void)
{
	// should only get this if we're not the host
	if (AvP.Network != I_Peer)
	{
		// Vaguely possible that a host could receive a message that is only intended for peers
		// if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
	}

	/* only do this if we're playing or in startup */
	/* check start flags on all players (including ourselves) */
	{
		// A client that isn't in the list has no place in it to look at
		int const myIndex = PlayerIdInPlayerList(AvPNetID);

		if (myIndex != NET_IDNOTINPLAYERLIST && netGameData.playerData[myIndex].startFlag) {
			netGameData.myGameState = NGS_Playing;
		}
	}

	if ((netGameData.myGameState != NGS_Playing) && (netGameData.myGameState != NGS_StartUp) && (netGameData.myGameState != NGS_Joining)) {
		return;
	}

	netGameData.myGameState = NGS_EndGame;
	AvP.MainLoopRunning = 0;
}

static void ProcessNetMsg_PlayerLeaving(NetID senderId)
{
	// only do this if we're playing or in startup
	if (netGameData.myGameState == NGS_Playing || netGameData.myGameState == NGS_EndGameScreen) {
		RemovePlayersGhosts(senderId);
	}

	Inform_PlayerHasLeft(senderId);
	RemovePlayerFromGame(senderId);
}

static void ProcessNetMsg_LOSRequestBinarySwitch(NETMESSAGE_LOSREQUESTBINARYSWITCH *msgPtr)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	STRATEGYBLOCK *objectPtr = FindSwitchFromName(msgPtr->name);

	if (!objectPtr) {
		return;    // no such switch
	}

	// change the state of this object, then, via request state
	RequestState(objectPtr, 1, NULL);
}

// The first refused lift state is written in the log, so a bad host can be found. Any more aren't, so a flood of them can't fill the disk.
static void NoteRefusedLiftState()
{
	static bool saidSo = false;

	if (!saidSo)
	{
		saidSo = true;
		LogString("net: refused: a lift's message named a state lifts don't have. Any more aren't written down");
	}
}

static void ProcessNetMsg_PlatformLiftState(NETMESSAGE_PLATFORMLIFTSTATE *msgPtr)
{
	// only peers should get this
	if (AvP.Network != I_Peer)
	{
		// Vaguely possible that a host could receive a message that is only intended for peers
		// if this computer has only just become the host.
		LOCALASSERT(AvP.Network == I_Host);
		return;
	}

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	STRATEGYBLOCK *objectPtr = FindEnvironmentObjectFromName(msgPtr->name);

	if (!objectPtr) {
		return;    // no object
	}

	if (objectPtr->I_SBtype != I_BehaviourPlatform) {
		return;    // we only do binary switches
	}

	// The state is a plain byte from another machine. A car given one that isn't a lift's would never move again here.
	if (!Lift_StateIsKnown(msgPtr->state, (int)PLBS_GoingDown + 1))
	{
		NoteRefusedLiftState();
		return;
	}

	// update the lift state
	NetworkPeerChangePlatformLiftState(objectPtr, (PLATFORMLIFT_STATES)(msgPtr->state));
}

static void ProcessNetMsg_RequestPlatformLiftActivate(NETMESSAGE_REQUESTPLATFORMLIFTACTIVATE *msgPtr)
{
	// only host should process this
	if (AvP.Network != I_Host) {
		return;
	}

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	STRATEGYBLOCK *objectPtr = FindEnvironmentObjectFromName(msgPtr->name);

	if (!objectPtr) {
		return; // no object
	}

	if (objectPtr->I_SBtype != I_BehaviourPlatform) {
		return; // we only do platform lifts
	}

	// update the lift state
	PLATFORMLIFT_BEHAVIOUR_BLOCK *platLiftData = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)objectPtr->SBdataptr;
	LOCALASSERT(platLiftData);

	if (platLiftData->state == PLBS_AtRest && platLiftData->Enabled) {
		ActivatePlatformLift(objectPtr);
	}
}

static void ProcessNetMsg_PlayerAutoGunState(NETMESSAGE_AGUNSTATE *messagePtr, NetID senderId)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// get the object id from the message
	int objectId = (int)messagePtr->objectId;
	STRATEGYBLOCK *sbPtr = FindGhost(senderId, objectId);

	// Only the campaign mode has autoguns on the network at all, and this number has to name one. A player's body has no model an autogun's messages can use.
	if (!SameAsSinglePlayer() || objectId == GHOST_PLAYEROBJECTID || (sbPtr && (sbPtr->I_SBtype != I_BehaviourNetGhost || !sbPtr->SBdataptr || ((NETGHOSTDATABLOCK *)sbPtr->SBdataptr)->type != I_BehaviourAutoGun)))
	{
		Net_NoteRefused("an autogun message named something that isn't an autogun");
		return;
	}

	if (!sbPtr)
	{
		// we don't seem to have a ghost for this autoGun, so create one
		VECTORCH position;
		EULER orientation;
		position.vx = messagePtr->xPos;
		orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
		position.vy = messagePtr->yPos;
		orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
		position.vz = messagePtr->zPos;
		orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);
		// NB there is no sequence required for autoguns, so just pass zero
		sbPtr = CreateNetGhost(senderId, objectId, &position, &orientation, I_BehaviourAutoGun, IOT_Non, 0);

		if (sbPtr)
		{
			HandleGhostAutoGunMuzzleFlash(sbPtr, (int)messagePtr->IAmFiring);
			HandleGhostAutoGunSound(sbPtr, (int)messagePtr->IAmFiring);
		}
	}
	else
	{
		// update the autogun ghost...
		VECTORCH position;
		EULER orientation;
		position.vx = messagePtr->xPos;
		orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
		position.vy = messagePtr->yPos;
		orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
		position.vz = messagePtr->zPos;
		orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);
		UpdateGhost(sbPtr, &position, &orientation, 0, 0);
		HandleGhostAutoGunMuzzleFlash(sbPtr, (int)messagePtr->IAmFiring);
		HandleGhostAutoGunSound(sbPtr, (int)messagePtr->IAmFiring);
	}
}


static void ProcessNetMsg_MakeDecal(NETMESSAGE_MAKEDECAL *messagePtr)
{
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// The game uses this as a place in its table of decals
	if (!NetNumberFitsTable((int)messagePtr->DecalID, MAX_NO_OF_DECAL_IDS)) {
		return;
	}

	// Which of the level's modules the decal is on. It's kept with the decal, so it's checked here, before anything
	// can use it as a place in the module lists. The game that made the decal gives -1 when what it hit isn't one of
	// the level's modules (ParticleDynamics in dynamics.cpp), so -1 is let in.
	if (messagePtr->ModuleIndex != -1 && !NetNumberFitsTable(messagePtr->ModuleIndex, ModuleArraySize)) {
		Net_NoteRefused("a decal message named a module this level doesn't have");
		return;
	}

	AddDecal(messagePtr->DecalID, &(messagePtr->Direction), &(messagePtr->Position), messagePtr->ModuleIndex);
}

static unsigned char *ProcessNetMsg_ChatBroadcast(unsigned char *subMessagePtr, NetID senderId)
{
	BOOL same_species_only;
	// get player index from dpid
	unsigned char *ptr = subMessagePtr;
	int stringLength = 1;
	int playerIndex = PlayerIdInPlayerList(senderId);
	// get same_species flag
	same_species_only = *ptr++;

	while (*ptr++ && stringLength < 255) {
		stringLength++;
	}

	if (stringLength == 1 || stringLength >= 255) {
		LOCALASSERT(0);
		return ptr;
	}

	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return ptr;
	}

	if (netGameData.myGameState == NGS_Playing)
	{
		if (same_species_only)
		{
			// was a species say message, check to see if we are the correct species
			if (netGameData.playerData[playerIndex].characterType != netGameData.myCharacterType) {
				return ptr;
			}
		}

		// Another player's computer sent us this chat line over the network.
		// We show it as "NAME says: message" in OnScreenMessageBuffer, which holds 300
		// bytes: up to 299 characters, then the end marker (the zero byte that
		// marks where the text stops).
		// The check at the top of this function refuses a line of 254 characters
		// or more, and the sender's name is kept in a 13-byte box, so the line
		// fits today. A broken or hostile computer must never be able to push it
		// past the end of the buffer, so the limits are also right here: at most
		// the 13 bytes of the name box ("%.*s"), at most 299 characters in all
		// (_snprintf), and the end marker written by us into the last byte,
		// because _snprintf leaves it out when it cuts the text short.
		// A line that fits comes out exactly as before.
		_snprintf(OnScreenMessageBuffer, sizeof(OnScreenMessageBuffer) - 1, "%.*s says: %s", NET_PLAYERNAMELENGTH, netGameData.playerData[playerIndex].name, subMessagePtr + 1);
		OnScreenMessageBuffer[sizeof(OnScreenMessageBuffer) - 1] = '\0';
		// Every chat line says who's speaking, and can't break into a line that looks like the game's own
		NetCleanText(OnScreenMessageBuffer, sizeof(OnScreenMessageBuffer));
		NewOnScreenMessage(OnScreenMessageBuffer);

		/* KJL 99/2/5 - play 'incoming message' sound */
		switch (netGameData.playerData[playerIndex].characterType)
		{
			case NGCT_Marine:
			{
				Sound_Play(SID_CONSOLE_MARINEMESSAGE, NULL);
				break;
			}

			case NGCT_Predator:
			{
				Sound_Play(SID_CONSOLE_PREDATORMESSAGE, NULL);
				break;
			}

			case NGCT_Alien:
			{
				Sound_Play(SID_CONSOLE_ALIENMESSAGE, NULL);
				break;
			}

			default:
				break;
		}
	}

	return ptr;
}

static void ProcessNetMsg_MakeExplosion(NETMESSAGE_MAKEEXPLOSION *messagePtr)
{
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// Only these go on the network. Any other would make damaging fire here as if this machine had started it.
	if (messagePtr->ExplosionID != EXPLOSION_SMALL_NOCOLLISIONS && messagePtr->ExplosionID != EXPLOSION_PREDATORPISTOL && messagePtr->ExplosionID != EXPLOSION_HUGE && messagePtr->ExplosionID != EXPLOSION_PULSEGRENADE)
	{
		Net_NoteRefused("an explosion message named a kind no game sends");
		return;
	}

	MakeVolumetricExplosionAt(&(messagePtr->Position), messagePtr->ExplosionID);
}

static void ProcessNetMsg_MakeFlechetteExplosion(NETMESSAGE_MAKEFLECHETTEEXPLOSION *messagePtr)
{
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// A seed of 0 is how the machine that fired asks for a new one, and tells everyone else
	if (messagePtr->Seed == 0)
	{
		Net_NoteRefused("a flechette message had no seed");
		return;
	}

	MakeFlechetteExplosionAt(&(messagePtr->Position), messagePtr->Seed);
}

static void ProcessNetMsg_MakePlasmaExplosion(NETMESSAGE_MAKEPLASMAEXPLOSION *messagePtr)
{
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	MakePlasmaExplosion(&(messagePtr->Position), &(messagePtr->FromPosition), messagePtr->ExplosionID);
}

static void ProcessNetMsg_PredatorSights(NETMESSAGE_PREDATORSIGHTS *messagePtr, NetID senderId)
{
	int playerIndex = PlayerIdInPlayerList(senderId);

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	{
		int i = 2;
		VECTORCH offset[3] =
		{
			{0, -50, 0},
			{43, 25, 0},
			{ -43, 25, 0},
		};
		MATRIXCH matrix;
		VECTORCH centre;
		EULER orientation;
		centre.vx = messagePtr->xPos;
		orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
		centre.vy = messagePtr->yPos;
		orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
		centre.vz = messagePtr->zPos;
		orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);
		CreateEulerMatrix(&orientation, &matrix);
		TransposeMatrixCH(&matrix);

		do
		{
			VECTORCH position = offset[i];
			RotateVector(&position, &matrix);
			PredatorLaserSights[playerIndex].Position[i].vx = centre.vx + position.vx;
			PredatorLaserSights[playerIndex].Position[i].vy = centre.vy + position.vy;
			PredatorLaserSights[playerIndex].Position[i].vz = centre.vz + position.vz;
			PredatorLaserSights[playerIndex].Normal[i].vx = matrix.mat31;
			PredatorLaserSights[playerIndex].Normal[i].vy = matrix.mat32;
			PredatorLaserSights[playerIndex].Normal[i].vz = matrix.mat33;
		}
		while (i--);

		PredatorLaserSights[playerIndex].TargetID = messagePtr->TargetID;
	}
}

static void ProcessNetMsg_FragmentalObjectsStatus(NETMESSAGE_FRAGMENTALOBJECTSSTATUS *messagePtr)
{
	int fragNumber = 0;
	int objectsToSkip = messagePtr->BatchNumber * (NUMBER_OF_FRAGMENTAL_OBJECTS << 3);

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	LOCALASSERT(AvP.Network != I_No_Network);

	for (int i = 0; i < NUMBER_OF_FRAGMENTAL_OBJECTS; i++) {
		FragmentalObjectStatus[i] = messagePtr->StatusBitfield[i];
	}

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[i];
		int status;

		if (sbPtr->I_SBtype == I_BehaviourInanimateObject)
		{
			INANIMATEOBJECT_STATUSBLOCK *objectStatusPtr = static_cast<INANIMATEOBJECT_STATUSBLOCK *>(sbPtr->SBdataptr);
			LOCALASSERT(objectStatusPtr);

			if ((objectStatusPtr->typeId == IOT_Static) && (!objectStatusPtr->Indestructable) && (!objectStatusPtr->lifespanTimer))
			{
				if (objectsToSkip > 0)
				{
					objectsToSkip--;
					continue;
				}

				status = ReadFragmentStatus(fragNumber++);

				if (status) // should exist
				{
#if 0

					/* so if it doesn't exist, respawn it */
					if (objectStatusPtr->respawnTimer != 0)
					{
						RespawnInanimateObject(sbPtr);
						objectStatusPtr->respawnTimer = 0;
					}

#endif
				}
				else // shouldn't exist
				{
					if (objectStatusPtr->respawnTimer == 0) {
						KillFragmentalObjectForRespawn(sbPtr);
					}
				}
			}
		}
		else if (sbPtr->I_SBtype == I_BehaviourPlacedLight)
		{
			PLACED_LIGHT_BEHAV_BLOCK *pl_bhv = static_cast<PLACED_LIGHT_BEHAV_BLOCK *>(sbPtr->SBdataptr);
			LOCALASSERT(pl_bhv);

			if (!pl_bhv->Indestructable)
			{
				if (objectsToSkip > 0)
				{
					objectsToSkip--;
					continue;
				}

				status = ReadFragmentStatus(fragNumber++);

				if (status) // should exist
				{
#if 0

					/* so if it doesn't exist, respawn it */
					if (pl_bhv->state == Light_State_Broken)
					{
						RespawnInanimateObject(sbPtr);
						objectStatusPtr->respawnTimer = 0;
					}

#endif
				}
				else // shouldn't exist
				{
					if (pl_bhv->state != Light_State_Broken) {
						KillLightForRespawn(sbPtr);
					}
				}
			}
		}

		if (fragNumber >= (NUMBER_OF_FRAGMENTAL_OBJECTS << 3)) {
			break;
		}
	}
}

static void ProcessNetMsg_StrategySynch(NETMESSAGE_STRATEGYSYNCH *messagePtr)
{
	int objectNumber = 0;
	int objectsToSkip = messagePtr->BatchNumber * (NUMBER_OF_STRATEGIES_TO_SYNCH);

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	LOCALASSERT(AvP.Network != I_No_Network);

	if (!netGameData.myStrategyCheckSum)
	{
		netGameData.myStrategyCheckSum = GetStrategySynchObjectChecksum();
	}

	if (messagePtr->strategyCheckSum != netGameData.myStrategyCheckSum)
	{
		//strategies are obviously different from those on the host's machine
		textprint("Strategy checksums don't match");

		// The host's message names its switches by where they are in its list. A different list means it would name the wrong ones, so the
		// whole message is left out. The log hears of it once a level: the checksums are only numbers, so nothing the host sent is written as text.
		if (NetLevelCount_One(switchSyncsNotMatched))
		{
			LogString("net: the host's switches, doors and lifts can't be matched with this machine's (the host's checksum is " + std::to_string(messagePtr->strategyCheckSum) +
				", this machine's " + std::to_string(netGameData.myStrategyCheckSum) + "), so in this level they won't follow the host's. The two machines may not have the same level file");
		}
		return;
	}

	for (int i = 0; i<NUMBER_OF_STRATEGIES_TO_SYNCH >> 2; i++)
	{
		StrategySynchArray[i] = messagePtr->StatusBitfield[i];
	}

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[i];
		//      int status;

		if (sbPtr->I_SBtype == I_BehaviourBinarySwitch ||
		    sbPtr->I_SBtype == I_BehaviourLinkSwitch ||
		    sbPtr->I_SBtype == I_BehaviourTrackObject)
		{
			if (objectsToSkip > 0)
			{
				objectsToSkip--;
				continue;
			}

			switch (sbPtr->I_SBtype)
			{
				case I_BehaviourBinarySwitch :
					BinarySwitchSetSynchData(sbPtr, ReadStrategySynch(objectNumber++));
					break;

				case I_BehaviourLinkSwitch :
					LinkSwitchSetSynchData(sbPtr, ReadStrategySynch(objectNumber++));
					break;

				case I_BehaviourTrackObject :
					TrackObjectSetSynchData(sbPtr, ReadStrategySynch(objectNumber++));
					break;
			}
		}

		if (objectNumber >= (NUMBER_OF_STRATEGIES_TO_SYNCH))
		{
			break;
		}
	}
}

static void ProcessNetMsg_LocalObjectOnFire(NETMESSAGE_LOBONFIRE *messagePtr, NetID senderId)
{
	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing)
	{
		return;
	}

	/* This message is for the player who owns the object, so first check
	if the message is meant for us */
	if (messagePtr->playerId != AvPNetID) {
		return;
	}

	/* next we have to find this object in our strategyblock list */
	{
		int objectId;
		STRATEGYBLOCK *sbPtr;
		objectId = (int)messagePtr->objectId;
		sbPtr = FindObjectFromNetIndex(objectId);

		/* check if we have found an sb: if not the object has probably been
		destroyed already, so just ignore it */
		if (sbPtr)
		{
			sbPtr->SBDamageBlock.IsOnFire = 1;

			if (sbPtr == Player->ObStrategyBlock)
			{
				myIgniterId = senderId;
				PlayerStatusPtr->fireTimer = PLAYER_ON_FIRE_TIME;
			}
			else if (sbPtr->I_SBtype == I_BehaviourAlien)
			{
				// need to note who burnt this alien
				ALIEN_STATUS_BLOCK *alienStatus = (ALIEN_STATUS_BLOCK *)sbPtr->SBdataptr;
				alienStatus->aliensIgniterId = senderId;
			}
		}
	}
}

static void ProcessNetMsg_AlienAIState(NETMESSAGE_ALIENAISTATE *messagePtr, NetID senderId)
{
	STRATEGYBLOCK *sbPtr;
	int objectId;
	VECTORCH position;
	EULER orientation;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// get the object id from the message
	objectId = (int)messagePtr->Guid;
	sbPtr = FindGhost(senderId, objectId);

	if (sbPtr && !IsAnAlienGhost(sbPtr))
	{
		Net_NoteRefused("an alien message named a ghost that isn't an alien");
		return;
	}
	position.vx = messagePtr->xPos;
	orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT);
	position.vy = messagePtr->yPos;
	orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT);
	position.vz = messagePtr->zPos;
	orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT);

	if (!sbPtr)
	{
		// we don't seem to have a ghost for this object, so create one, if this level has that kind of alien's model at all
		if (!AlienModelIsLoaded((int)messagePtr->AlienType))
		{
			return;
		}

		sbPtr = CreateNetGhost(senderId, objectId, &position, &orientation, I_BehaviourAlien, IOT_Non, messagePtr->AlienType);
	}
	else
	{
		// update the ghost...
		MaintainGhostFireStatus(sbPtr, (int)messagePtr->IAmOnFire);
		// NB don't need to update the object type
		UpdateAlienAIGhost(sbPtr, &position, &orientation, messagePtr->sequence_type, messagePtr->sub_sequence, messagePtr->sequence_length << 8);
	}

#if EXTRAPOLATION_TEST

	if (sbPtr)
	{
		NETGHOSTDATABLOCK *ghostData;
		VECTORCH velocity;
		int diff;
		int playerTimer;
		int playerIndex = PlayerIdInPlayerList(senderId);

		if (playerIndex == NET_IDNOTINPLAYERLIST) {
			return;
		}

		ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		GLOBALASSERT(ghostData);
		playerTimer = netGameData.playerData[playerIndex].timer;
		velocity.vx = MUL_FIXED(sbPtr->DynPtr->OrientMat.mat31, messagePtr->speed);
		velocity.vy = MUL_FIXED(sbPtr->DynPtr->OrientMat.mat32, messagePtr->speed);
		velocity.vz = MUL_FIXED(sbPtr->DynPtr->OrientMat.mat33, messagePtr->speed);
		diff = Approximate3dMagnitude(&ghostData->velocity) - Approximate3dMagnitude(&velocity);

		if (diff > 1500 || -diff > 1500)
		{
			// change in velocity , so reset extrapolation timer
			ghostData->extrapTimer = -ONE_FIXED;
		}

		ghostData->velocity = velocity;
		ghostData->extrapTimerLast = 0;

		if (playerTimer >= (int)ghostData->lastTimeRead) {
			ghostData->extrapTimer -= (playerTimer - ghostData->lastTimeRead);
		}
		else {
			ghostData->extrapTimer = -ONE_FIXED;
		}

		ghostData->lastTimeRead = playerTimer;
		sbPtr->DynPtr->UseStandardGravity = messagePtr->standard_gravity;

		if (!sbPtr->DynPtr->UseStandardGravity)
		{
			if (sbPtr->DynPtr->GravityDirection.vy == ONE_FIXED)
			{
				MATRIXCH mat;
				// alien is crawling , so we need to get an appropriate gravity direction
				CreateEulerMatrix(&orientation, &mat);
				sbPtr->DynPtr->GravityDirection.vx = mat.mat12;
				sbPtr->DynPtr->GravityDirection.vy = mat.mat22;
				sbPtr->DynPtr->GravityDirection.vz = mat.mat32;
			}

			sbPtr->DynPtr->LinImpulse.vx = 0;
			sbPtr->DynPtr->LinImpulse.vy = 0;
			sbPtr->DynPtr->LinImpulse.vz = 0;
			sbPtr->DynPtr->ToppleForce = TOPPLE_FORCE_ALIEN;
		}
	}

#endif
}

/* CDF 24/8/98 A better message. */
static void ProcessNetMsg_AlienAISequenceChange(NETMESSAGE_ALIENSEQUENCECHANGE *messagePtr, NetID senderId)
{
	STRATEGYBLOCK *sbPtr;
	int objectId;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// get the object id from the message
	objectId = (int)messagePtr->Guid;
	sbPtr = FindGhost(senderId, objectId);

	if (sbPtr && !IsAnAlienGhost(sbPtr))
	{
		Net_NoteRefused("an alien message named a ghost that isn't an alien");
		return;
	}

	if (!sbPtr)
	{
		// Gordon Bennet.  Return, then.
		return;
	}
	else
	{
		int sequence_length, tweening_time;
		
		// update the ghost...
		if (messagePtr->sequence_length == -1) {
			sequence_length = -1;
		}
		else {
			sequence_length = messagePtr->sequence_length << 8;
		}

		if (messagePtr->tweening_time == -1) {
			tweening_time = -1;
		}
		else {
			tweening_time = messagePtr->tweening_time << 8;
		}

		UpdateAlienAIGhostAnimSequence(sbPtr, static_cast<enum HModelSequenceTypes>(messagePtr->sequence_type), messagePtr->sub_sequence, sequence_length, tweening_time);
	}
}

static void ProcessNetMsg_AlienAIKilled(NETMESSAGE_ALIENAIKILLED *messagePtr, NetID senderId)
{
	STRATEGYBLOCK *sbPtr;
	int objectId;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	Inform_AiHasDied(messagePtr->killerId, static_cast<enum AlienType>(messagePtr->AlienType), messagePtr->weaponIcon);
	{
		int killerIndex = PlayerIdInPlayerList(messagePtr->killerId);

		// The message has room to say 3, and there are three kinds of alien, from 0
		if (killerIndex != NET_IDNOTINPLAYERLIST && NetNumberFitsTable(messagePtr->AlienType, 3)) {
			netGameData.playerData[killerIndex].aliensKilled[messagePtr->AlienType] = messagePtr->killCount;
		}
	}
	// get the object id from the message
	objectId = (int)messagePtr->Guid;
	sbPtr = FindGhost(senderId, objectId);

	if (sbPtr && !IsAnAlienGhost(sbPtr))
	{
		Net_NoteRefused("an alien message named a ghost that isn't an alien");
		return;
	}

	if (!sbPtr)
	{
		// Gordon Bennet.  Return, then. It's not that important.
		return;
	}
	else
	{
		// 'update' the ghost...
		KillAlienAIGhost(sbPtr, messagePtr->death_code, messagePtr->death_time, messagePtr->GibbFactor);
	}
}

// Adds this to what's going out this frame, if there's room. Gives back false if there isn't.
static bool AddPartToSendBuffer(NETMESSAGETYPE type, void const *part, int size)
{
	int const headerSize = sizeof(NETMESSAGEHEADER);
	int const left = NET_MESSAGEBUFFERSIZE - (int)(endSendBuffer - &sendBuffer[0]);

	if (headerSize + size > left)
	{
		NoteSendBufferFull(headerSize + size);
		return false;
	}

	NETMESSAGEHEADER *const header = (NETMESSAGEHEADER *)endSendBuffer;

	endSendBuffer += headerSize;
	header->type = (unsigned char)type;
	memcpy(endSendBuffer, part, size);
	endSendBuffer += size;
	return true;
}

// Each NPC near someone, as the host sees it. If the buffer fills up, the next frame starts from the first one left out, so it's never the same ones who don't get told, and their
// ghosts don't vanish on the others' machines.
static void AddNpcStateMessages(void)
{
	static int startAt = 0;

	if (!AvP.NetworkAIServer || !SameAsSinglePlayer() || NumActiveStBlocks <= 0)
	{
		return;
	}

	if (startAt >= NumActiveStBlocks)
	{
		startAt = 0;
	}

	int stoppedAt = -1;
	char const *const bufferBefore = (char const *)endSendBuffer;

	for (int i = 0; i < NumActiveStBlocks && stoppedAt < 0; i++)
	{
		int const place = (startAt + i) % NumActiveStBlocks;
		STRATEGYBLOCK *const sbPtr = ActiveStBlockList[place];

		if (!sbPtr->SBdptr)
		{
			continue;
		}

		NETMESSAGE_NPCSTATE state;

		if (!NpcGhosts_FillState(sbPtr, &state))
		{
			continue;
		}

		if (!AddPartToSendBuffer(NetMT_NpcState, &state, sizeof(state)))
		{
			stoppedAt = place;
			break;
		}


		NETMESSAGE_NPCXENODELTAS deltas;

		if (NpcGhosts_FillXenoDeltas(sbPtr, &deltas) && !AddPartToSendBuffer(NetMT_NpcXenoDeltas, &deltas, sizeof(deltas)))
		{
			stoppedAt = place;
		}
	}

	startAt = stoppedAt >= 0 ? stoppedAt : 0;
}

// Called for every sound an NPC's voice makes on the host. Only the mode that plays as single player has NPCs other than aliens on a network.
void AddNetMsg_NpcSound(int species, int voice, int category, int pitch, VECTORCH const *place)
{
	if (AvP.Network == I_No_Network || !AvP.NetworkAIServer || !SameAsSinglePlayer() || !place)
	{
		return;
	}

	NETMESSAGE_NPCSOUND sound;

	memset(&sound, 0, sizeof(sound));
	sound.species = (unsigned char)species;
	sound.voice = (unsigned char)voice;
	sound.category = (unsigned char)category;
	sound.pitch = pitch;
	sound.vx = place->vx;
	sound.vy = place->vy;
	sound.vz = place->vz;
	AddPartToSendBuffer(NetMT_NpcSound, &sound, sizeof(sound));
}

// A platform lift's car, as the host has it: its height, its state, and the end it's going to or resting at. In the mode that plays as
// single player only. Every other machine's car follows it (bh_platform_lift.cpp), so a joiner who missed a message, or arrived
// after the car had moved, has it right within a second. Sent when its state changes, and once a second.
void AddNetMsg_LiftState(STRATEGYBLOCK *lift)
{
	if (AvP.Network != I_Host || !SameAsSinglePlayer() || !lift->DynPtr || !lift->SBdataptr)
	{
		return;
	}

	PLATFORMLIFT_BEHAVIOUR_BLOCK *const data = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)lift->SBdataptr;
	NETMESSAGE_LIFTSTATE message;

	memset(&message, 0, sizeof(message));
	memcpy(message.name, lift->SBname, sizeof(message.name));
	message.y = lift->DynPtr->Position.vy;
	message.state = (unsigned char)data->state;
	message.end = (unsigned char)PlatformLift_EndItsGoingTo(lift);
	AddPartToSendBuffer(NetMT_LiftState, &message, sizeof(message));
	data->syncMessages++;
}

// The host says where a lift's car is. Everything in this message came from another machine, so all of it is checked first.
// Then this machine's car follows it (PlatformLift_TakeTheHostsCar, bh_platform_lift.cpp).
static void ProcessNetMsg_LiftState(NETMESSAGE_LIFTSTATE *messagePtr)
{
	if (AvP.Network != I_Peer || netGameData.myGameState != NGS_Playing || !SameAsSinglePlayer())
	{
		return;
	}

	STRATEGYBLOCK *const lift = FindEnvironmentObjectFromName(messagePtr->name);

	if (!lift || lift->I_SBtype != I_BehaviourPlatform || !lift->SBdataptr || !lift->DynPtr)
	{
		Net_NoteRefused("a lift's sync message named something that isn't a platform lift");
		return;
	}

	// The name is how the car is found, so a name two lifts share can't say which car it means. No stock level has one.
	if (PlatformLift_NameIsShared(lift))
	{
		Net_NoteRefused("a lift's sync message named a lift whose name another lift shares");
		return;
	}

	PLATFORMLIFT_BEHAVIOUR_BLOCK const *const data = (PLATFORMLIFT_BEHAVIOUR_BLOCK *)lift->SBdataptr;

	if (!Lift_StateIsKnown(messagePtr->state, (int)PLBS_GoingDown + 1))
	{
		Net_NoteRefused("a lift's sync message named a state lifts don't have");
		return;
	}

	if (!NetLiftEndIsKnown(messagePtr->end))
	{
		Net_NoteRefused("a lift's sync message named an end a lift doesn't have");
		return;
	}

	// A car can be a little past its end for a frame, so the track counts a step's height further at each end
	if (!NetLiftHeightIsOnItsTrack(messagePtr->y, data->upHeight, data->downHeight, MAXIMUM_STEP_HEIGHT))
	{
		Net_NoteRefused("a lift's sync message put the car off its track");
		return;
	}

	PlatformLift_TakeTheHostsCar(lift, messagePtr->y, messagePtr->state, messagePtr->end);
}

// A thing the queen throws has moved. She's only on the host, so every other machine has to be told, or it would sit where the level put it.
void AddNetMsg_NpcThrownObject(STRATEGYBLOCK *object)
{
	if (AvP.Network == I_No_Network || !AvP.NetworkAIServer || !SameAsSinglePlayer() || !object->DynPtr)
	{
		return;
	}

	NETMESSAGE_NPCTHROWNOBJECT message;
	EULER way;

	// She holds it by setting its matrix, so the matrix is what's true
	MatrixToEuler(&object->DynPtr->OrientMat, &way);

	memset(&message, 0, sizeof(message));
	memcpy(message.name, object->SBname, sizeof(message.name));
	message.vx = object->DynPtr->Position.vx;
	message.vy = object->DynPtr->Position.vy;
	message.vz = object->DynPtr->Position.vz;
	message.xOrient = (short)way.EulerX;
	message.yOrient = (short)way.EulerY;
	message.zOrient = (short)way.EulerZ;
	AddPartToSendBuffer(NetMT_NpcThrownObject, &message, sizeof(message));
}

// The host says where a thing the queen threw has got to. Everything in this message came from another machine.
// It's moved to where it's told and stays there: only the host works out how it flies and whom it hits.
static void ProcessNetMsg_NpcThrownObject(NETMESSAGE_NPCTHROWNOBJECT *messagePtr)
{
	if (netGameData.myGameState != NGS_Playing || !SameAsSinglePlayer())
	{
		return;
	}

	STRATEGYBLOCK *const object = FindEnvironmentObjectFromName(messagePtr->name);

	if (!object)
	{
		return;
	}

	// She only throws these (see the search for them in bh_queen.cpp). The name also finds lights, lifts and switches.
	if (object->I_SBtype != I_BehaviourInanimateObject || !object->DynPtr || !object->name || !strstr(object->name, "QueenAmmo"))
	{
		Net_NoteRefused("a thrown object's message named something a queen doesn't throw");
		return;
	}

	VECTORCH place;

	place.vx = messagePtr->vx;
	place.vy = messagePtr->vy;
	place.vz = messagePtr->vz;

	// The game works out what's visible from the room a thing is in, so it has to be in one
	MODULE *const room = ModuleFromPosition(&place, object->containingModule);

	if (!room)
	{
		Net_NoteRefused("a thrown object's message put it outside the level");
		return;
	}

	DYNAMICSBLOCK *const body = object->DynPtr;

	// An angle goes round the circle 4096 parts to the turn, and the code that turns one into a matrix looks it up in a table of that size
	body->OrientEuler.EulerX = messagePtr->xOrient & 4095;
	body->OrientEuler.EulerY = messagePtr->yOrient & 4095;
	body->OrientEuler.EulerZ = messagePtr->zOrient & 4095;
	CreateEulerMatrix(&body->OrientEuler, &body->OrientMat);
	TransposeMatrixCH(&body->OrientMat);

	body->Position = place;
	body->PrevPosition = place;
	body->LinVelocity.vx = body->LinVelocity.vy = body->LinVelocity.vz = 0;
	body->LinImpulse.vx = body->LinImpulse.vy = body->LinImpulse.vz = 0;
	body->GravityOn = 0;
	body->OnlyCollideWithEnvironment = 1;
	object->containingModule = room;
}

// PlaySound checks the voice and the category against what it has loaded, so those can be any number.
static void ProcessNetMsg_NpcSound(NETMESSAGE_NPCSOUND *messagePtr, NetID senderId)
{
	if (netGameData.myGameState != NGS_Playing || !SameAsSinglePlayer())
	{
		return;
	}

	if (!NetNumberFitsTable(messagePtr->species, kNpcSoundKinds))
	{
		Net_NoteRefused("an NPC's sound named a voice that isn't on the list");
		return;
	}

	if (!NpcSoundPitch_IsGood(messagePtr->pitch))
	{
		Net_NoteRefused("an NPC's sound had a pitch that can't be right");
		return;
	}

	VECTORCH place;

	place.vx = messagePtr->vx;
	place.vy = messagePtr->vy;
	place.vz = messagePtr->vz;

	switch (messagePtr->species)
	{
		case kNpcSoundMarine: PlayMarineScream(messagePtr->voice, messagePtr->category, messagePtr->pitch, NULL, &place); break;
		case kNpcSoundPredator: PlayPredatorSound(messagePtr->voice, messagePtr->category, messagePtr->pitch, NULL, &place); break;
		default: PlayQueenSound(messagePtr->voice, messagePtr->category, messagePtr->pitch, NULL, &place); break;
	}
}

void AddNetMsg_NpcKilled(STRATEGYBLOCK *sbPtr, int deathCode, int deathTime, int gibbFactor)
{
	if (AvP.Network == I_No_Network || !AvP.NetworkAIServer)
	{
		return;
	}

	NETMESSAGE_NPCKILLED killed;

	killed.Guid = *((int *)(&(sbPtr->SBname[4])));
	killed.death_code = deathCode;
	killed.death_time = deathTime;
	killed.GibbFactor = gibbFactor;
	AddPartToSendBuffer(NetMT_NpcKilled, &killed, sizeof(killed));
}

// Every number in these has come over the network, so nothing is used until it has been checked. The sender has already been checked to be the host: see kNetPartRules.
static void ProcessNetMsg_NpcState(NETMESSAGE_NPCSTATE *messagePtr, NetID senderId)
{
	if (netGameData.myGameState != NGS_Playing || !SameAsSinglePlayer())
	{
		return;
	}

	if (!NpcGuid_IsGood(messagePtr->Guid))
	{
		Net_NoteRefused("an NPC message with a number no NPC can have");
		return;
	}

	if (!NetNumberFitsTable(messagePtr->kind, kNpcKindCount))
	{
		Net_NoteRefused("an NPC message of a kind that isn't on the list");
		return;
	}

	if (!NpcSequenceLength_IsGood(messagePtr->sequence_length))
	{
		Net_NoteRefused("an NPC message with an animation length that can't be right");
		return;
	}

	if (!NpcFlags_AreKnown(messagePtr->flags))
	{
		Net_NoteRefused("an NPC message set a flag this version doesn't know");
		return;
	}

	AVP_BEHAVIOUR_TYPE const type = NpcGhosts_BehaviourOfKind(messagePtr->kind);
	VECTORCH position;
	EULER orientation;

	position.vx = messagePtr->xPos;
	position.vy = messagePtr->yPos;
	position.vz = messagePtr->zPos;
	orientation.EulerX = (messagePtr->xOrient << NET_EULERSCALESHIFT) & 4095;
	orientation.EulerY = (messagePtr->yOrient << NET_EULERSCALESHIFT) & 4095;
	orientation.EulerZ = (messagePtr->zOrient << NET_EULERSCALESHIFT) & 4095;

	STRATEGYBLOCK *sbPtr = FindGhost(senderId, messagePtr->Guid);

	if (!sbPtr)
	{
		if (!NpcGhosts_ModelIsLoaded(messagePtr->kind, messagePtr->model))
		{
			Net_NoteRefused("an NPC message with a model this level hasn't loaded");
			return;
		}

		sbPtr = CreateNetGhost(senderId, messagePtr->Guid, &position, &orientation, type, IOT_Non, messagePtr->model);

		if (!sbPtr)
		{
			return;
		}
	}
	else if (((NETGHOSTDATABLOCK *)sbPtr->SBdataptr)->type != type)
	{
		// Something else already has this name, or it has died, and isn't to be turned into something else
		Net_NoteRefused("an NPC message named a ghost that isn't that kind");
		return;
	}

	NpcGhosts_ApplyState(sbPtr, messagePtr, &position, &orientation);
}

static void ProcessNetMsg_NpcXenoDeltas(NETMESSAGE_NPCXENODELTAS *messagePtr, NetID senderId)
{
	if (netGameData.myGameState != NGS_Playing || !SameAsSinglePlayer())
	{
		return;
	}

	if (!NpcGuid_IsGood(messagePtr->Guid))
	{
		Net_NoteRefused("an NPC message with a number no NPC can have");
		return;
	}

	if (!NpcXenoBeams_AreGood(messagePtr->beams))
	{
		Net_NoteRefused("a xenoborg's message named a beam he hasn't got");
		return;
	}

	STRATEGYBLOCK *const sbPtr = FindGhost(senderId, messagePtr->Guid);

	if (!sbPtr)
	{
		return;
	}

	if (((NETGHOSTDATABLOCK *)sbPtr->SBdataptr)->type != I_BehaviourXenoborg)
	{
		Net_NoteRefused("a xenoborg's head named a ghost that isn't a xenoborg");
		return;
	}

	NpcGhosts_ApplyXenoDeltas(sbPtr, messagePtr);
}

static void ProcessNetMsg_NpcKilled(NETMESSAGE_NPCKILLED *messagePtr, NetID senderId)
{
	if (netGameData.myGameState != NGS_Playing || !SameAsSinglePlayer())
	{
		return;
	}

	if (!NpcGuid_IsGood(messagePtr->Guid))
	{
		Net_NoteRefused("an NPC message with a number no NPC can have");
		return;
	}

	if (!NpcDeathTime_IsGood(messagePtr->death_time))
	{
		Net_NoteRefused("an NPC's death had a time that can't be right");
		return;
	}

	STRATEGYBLOCK *const sbPtr = FindGhost(senderId, messagePtr->Guid);

	if (!sbPtr)
	{
		return;
	}

	if (!NpcGhosts_TypeIsAnNpc(((NETGHOSTDATABLOCK *)sbPtr->SBdataptr)->type))
	{
		Net_NoteRefused("an NPC's death named a ghost that isn't an NPC");
		return;
	}

	NpcGhosts_Kill(sbPtr, messagePtr->death_code, messagePtr->death_time, messagePtr->GibbFactor);
}

static void ProcessNetMsg_FarAlienPosition(NETMESSAGE_FARALIENPOSITION *messagePtr, NetID senderId)
{
	STRATEGYBLOCK *sbPtr;
	EULER orientation = {0, 0, 0};
	VECTORCH position;
	AIMODULE *targetModule = 0;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// make sure the target module index is in range
	if (messagePtr->targetModuleIndex >= (unsigned int)AIModuleArraySize) {
		return;
	}

	targetModule = &AIModuleArray[messagePtr->targetModuleIndex];

	if (messagePtr->indexIsModuleIndex)
	{
		FARENTRYPOINT *targetEntryPoint;
		AIMODULE *startModule = 0;

		// The alien is located at an entry point , the second index is the source module index
		// Make sure it is a valid module index
		if (messagePtr->index >= (unsigned int)AIModuleArraySize) {
			return;
		}

		startModule = &AIModuleArray[messagePtr->index];
		// find the appropriate entry point
		targetEntryPoint = GetAIModuleEP(targetModule, startModule);

		if (!targetEntryPoint)
		{
			return; // forget it then
		}

		// found the alien's location (relative to module)
		position = targetEntryPoint->position;
	}
	else
	{
		// The alien is at one of the modules auxilary locations.
		// The list of far places has one entry for each of the level's modules (InitFarLocDataAreas in pfarlocs.cpp),
		// not one for each AI module. So the number checked above against the AI modules is checked against this
		// list's own size too, and the list has to be there.
		if (!FALLP_AuxLocs || !NetNumberFitsTable((int)messagePtr->targetModuleIndex, ModuleArraySize)) {
			Net_NoteRefused("a far alien message named a module with no far places");
			return;
		}

		int noOfAuxLocs = FALLP_AuxLocs[messagePtr->targetModuleIndex].numLocations;
		VECTORCH *auxLocsList = FALLP_AuxLocs[messagePtr->targetModuleIndex].locationsList;

		// make sure we have a valid index
		if (messagePtr->index >= (unsigned int)noOfAuxLocs) {
			return;
		}

		// found the alien's location (relative to module)
		position = auxLocsList[messagePtr->index];
	}

	// convert the position into a world position
	position.vx += targetModule->m_world.vx;
	position.vy += targetModule->m_world.vy;
	position.vz += targetModule->m_world.vz;
	// find the alien
	sbPtr = FindGhost(senderId, messagePtr->Guid);

	if (!sbPtr)
	{
		// need to create a new ghost then
		sbPtr = CreateNetGhost(senderId, messagePtr->Guid, &position, &orientation, I_BehaviourAlien, IOT_Non, messagePtr->alienType);
	}

	if (sbPtr)
	{
		NETGHOSTDATABLOCK *ghostData;
		ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		GLOBALASSERT(ghostData);

		// make sure this is a ghost of an alien
		if (ghostData->type != I_BehaviourAlien) {
			return;
		}

		// update the position , and mark it as valid for far use only
		ghostData->onlyValidFar = 1;
		sbPtr->DynPtr->Position = position;
		sbPtr->DynPtr->PrevPosition = position;
		sbPtr->DynPtr->LinVelocity.vx = 0;
		sbPtr->DynPtr->LinVelocity.vy = 0;
		sbPtr->DynPtr->LinVelocity.vz = 0;
		sbPtr->DynPtr->LinImpulse.vx = 0;
		sbPtr->DynPtr->LinImpulse.vy = 0;
		sbPtr->DynPtr->LinImpulse.vz = 0;

		// make sure the alien is far
		if (sbPtr->SBdptr) {
			MakeGhostFar(sbPtr);
		}
	}
}

static int GetSizeOfGhostHierarchyDamagedMessage(char *messagePtr)
{
	int size = sizeof(NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER);
	NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER *messageHeader = (NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER *) messagePtr;

	if (messageHeader->damageProfile) {
		size += sizeof(NETMESSAGE_DAMAGE_PROFILE);
	}

	if (messageHeader->multiple) {
		size += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
	}

	if (messageHeader->sectionID) {
		size += sizeof(NETMESSAGE_DAMAGE_SECTION);
	}

	if (messageHeader->direction) {
		size += sizeof(NETMESSAGE_DAMAGE_DIRECTION);
	}

	return size;
}

static void ProcessNetMsg_GhostHierarchyDamaged(char *messagePtr, NetID senderId)
{
	NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER *messageHeader = 0;
	NETMESSAGE_DAMAGE_PROFILE *messageProfile = 0;
	NETMESSAGE_DAMAGE_MULTIPLE *messageMultiple = 0;
	NETMESSAGE_DAMAGE_SECTION *messageSection = 0;
	NETMESSAGE_DAMAGE_DIRECTION *messageDirection = 0;
	STRATEGYBLOCK *sbPtr;
	int objectId;
	NETGHOSTDATABLOCK *ghostData;
	VECTORCH direction;
	VECTORCH incoming;
	VECTORCH *incoming_ptr = 0;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	messageHeader = (NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER *) messagePtr;
	messagePtr += sizeof(NETMESSAGE_GHOSTHIERARCHYDAMAGED_HEADER);

	// find out which elements of the damage message have been sent
	if (messageHeader->damageProfile)
	{
		messageProfile = (NETMESSAGE_DAMAGE_PROFILE *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_PROFILE);
	}

	if (messageHeader->multiple)
	{
		messageMultiple = (NETMESSAGE_DAMAGE_MULTIPLE *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_MULTIPLE);
	}

	if (messageHeader->sectionID)
	{
		messageSection = (NETMESSAGE_DAMAGE_SECTION *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_SECTION);
	}

	if (messageHeader->direction)
	{
		messageDirection = (NETMESSAGE_DAMAGE_DIRECTION *) messagePtr;
		messagePtr += sizeof(NETMESSAGE_DAMAGE_DIRECTION);
	}

	// get the object id from the message
	objectId = (int)messageHeader->Guid;
	sbPtr = FindGhost(senderId, objectId);

	if (!sbPtr) {
		// Gordon Bennet.  Return, then. It's not that important.
		return;
	}
	else
	{
		DAMAGE_PROFILE damage;
		int multiple;
		// damage the ghost...
		SECTION_DATA *section_data = NULL;
		HMODELCONTROLLER *controller = NULL;
		ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;
		// fill out damage profile
		damage.Id = static_cast<enum AMMO_ID>(messageHeader->ammo_id);

		if (messageProfile)
		{
			damage.Impact = messageProfile->Impact;
			damage.Cutting = messageProfile->Cutting;
			damage.Penetrative = messageProfile->Penetrative;
			damage.Fire = messageProfile->Fire;
			damage.Electrical = messageProfile->Electrical;
			damage.Acid = messageProfile->Acid;
			damage.ExplosivePower = messageProfile->ExplosivePower;
			damage.Slicing = messageProfile->Slicing;
			damage.ProduceBlood = messageProfile->ProduceBlood;
			damage.ForceBoom = messageProfile->ForceBoom;
			damage.BlowUpSections = messageProfile->BlowUpSections;
			damage.Special = messageProfile->Special;
			damage.MakeExitWounds = messageProfile->MakeExitWounds;
		}
		else
		{
			if (!NetNumberFitsTable(damage.Id, MAX_NO_OF_AMMO_TEMPLATES))
			{
				LogIgnoredDamageMessage(damage.Id);
				return;
			}

			damage = TemplateAmmo[damage.Id].MaxDamage[AvP.Difficulty];
		}

		if (messageMultiple) {
			multiple = messageMultiple->multiple;
		}
		else {
			multiple = ONE_FIXED;
		}

		if (!NetDamageMultipleIsGood(multiple))
		{
			Net_NoteRefused("a damage message counted its hit a number of times it can't");
			return;
		}

		if (sbPtr->DynPtr && messageDirection)
		{
			if (messageDirection->direction_x ||
			    messageDirection->direction_y ||
			    messageDirection->direction_z)
			{
				MATRIXCH mat = sbPtr->DynPtr->OrientMat;
				TransposeMatrixCH(&mat);
				// extract the direction vector
				incoming.vx = messageDirection->direction_x;
				incoming.vy = messageDirection->direction_y;
				incoming.vz = messageDirection->direction_z;
				// normalise it
				Normalise(&incoming);
				direction = incoming;
				// and rotate it from world space to the object's local space
				RotateVector(&incoming, &mat);
				// set the incoming pointer
				incoming_ptr = &incoming;
			}
		}

		if (messageSection && messageSection->SectionID != -1)
		{
			// Hmm.
			controller = &ghostData->HModelController;
			section_data = GetThisSectionData_FromID(ghostData->HModelController.section_data,
			               messageSection->SectionID);
		}

		if (section_data)
		{
			DISPLAYBLOCK *fragged_section = 0;
			fragged_section = CauseDamageToHModel(controller, sbPtr, (&damage),
			                                      multiple, section_data, incoming_ptr, NULL, 1);

			if (fragged_section && damage.Id == AMMO_PRED_RIFLE && incoming_ptr)
			{
				// a speargun has fragged off a body part , so we need to create a spear
				CreateSpearPossiblyWithFragment(fragged_section, &fragged_section->ObWorld, &direction);
			}
		}
	}
}

static void ProcessNetMsg_Gibbing(NETMESSAGE_GIBBING *messagePtr, NetID senderId)
{
	STRATEGYBLOCK *sbPtr;
	int objectId;
	NETGHOSTDATABLOCK *ghostData;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// get the object id from the message
	objectId = (int)messagePtr->Guid;
	sbPtr = FindGhost(senderId, objectId);

	if (!sbPtr)
	{
		// Gordon Bennet.  Return, then. It's not that important.
		return;
	}
	else
	{
		SECTION_DATA *section_data = NULL;
		HMODELCONTROLLER *controller = NULL;
		ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;

		// only interested in gibbing corpses
		if (ghostData->type != I_BehaviourNetCorpse) {
			return;
		}

		// use the random number seed
		SetSeededFastRandom(messagePtr->seed);

		// now do the gibbing
		if (messagePtr->gibbFactor > 0) {
			Extreme_Gibbing(sbPtr, ghostData->HModelController.section_data, messagePtr->gibbFactor);
		}
		else if (messagePtr->gibbFactor < 0) {
			KillRandomSections(ghostData->HModelController.section_data, -(messagePtr->gibbFactor));
		}
	}
}

static void ProcessNetMsg_SpotAlienSound(NETMESSAGE_SPOTALIENSOUND *messagePtr, NetID senderId)
{
	VECTORCH position;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// Just play the thing.
	position.vx = messagePtr->vx;
	position.vy = messagePtr->vy;
	position.vz = messagePtr->vz;
	PlayAlienSound((int)messagePtr->alienType, (int)messagePtr->soundCategory, messagePtr->pitch, NULL, &position);
}

static void ProcessNetMsg_CreateWeapon(NETMESSAGE_CREATEWEAPON *messagePtr, NetID senderId)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// A game that plays as single player has no dropped guns, so nobody has a reason to send one, and each would hand out ammunition
	if (SameAsSinglePlayer())
	{
		Net_NoteRefused("a dropped gun was sent in a game that plays as single player");
		return;
	}

	// It becomes a pickup, and the game uses this number as a place in its table of weapons when someone walks over it
	if (!NetNumberFitsTable(messagePtr->type, MAX_NO_OF_WEAPON_TEMPLATES)) {
		return;
	}

	// Each one lasts 20 seconds and uses one of the game's 1000 blocks
	int const sender = PlayerIdInPlayerList(senderId);

	if (sender == NET_IDNOTINPLAYERLIST || !NetThisSecondHasRoom(timeGetTime(), &droppedGunsSecond[sender], &droppedGunsUsed[sender], kMostDroppedGunsEachSecond))
	{
		Net_NoteRefused("too many dropped guns from one player in a second");
		return;
	}

	if (Net_BlocksAreShort())
	{
		Net_NoteRefused("the game is short of blocks, so a dropped gun wasn't made");
		return;
	}

	// create the weapon
	CreateMultiplayerWeaponPickup(&messagePtr->location, messagePtr->type, &messagePtr->name[0]);
}

static void ProcessNetMsg_SpotOtherSound(NETMESSAGE_SPOTOTHERSOUND *messagePtr, NetID senderId)
{
	VECTORCH position;

	// only do this if we're playing
	if (netGameData.myGameState != NGS_Playing)
	{
		return;
	}

	// Just play the thing.
	position.vx = messagePtr->vx;
	position.vy = messagePtr->vy;
	position.vz = messagePtr->vz;
	PlayOtherSound(messagePtr->SoundIndex, &position, messagePtr->explosion);
}

/*----------------------------------------------------------------------
  These support functions are used to examine the current game state
  ----------------------------------------------------------------------*/

/* returns index if the given NetID is in the player list */
int PlayerIdInPlayerList(NetID Id)
{
	// check first, if we've been passed a null id
	if (Id == 0) {
		return NET_IDNOTINPLAYERLIST;
	}

	// check player list
	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == Id) {
			return i;
		}
	}

	// failed to find Id
	return NET_IDNOTINPLAYERLIST;
}

// returns true if there are any empty slots
int EmptySlotInPlayerList(void)
{
	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == 0) {
			return i;
		}
	}

	return NET_NOEMPTYSLOTINPLAYERLIST;
}

// Finds the local strategy block who's network object id is that passed
// The model of an NPC the host runs, for working out which part of it a player on another machine hit. Nothing for anything else.
HMODELCONTROLLER *Npc_HModelController(STRATEGYBLOCK *sbPtr)
{
	if (!sbPtr->SBdataptr)
	{
		return NULL;
	}

	switch (sbPtr->I_SBtype)
	{
		case I_BehaviourAlien:
			return &((ALIEN_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		case I_BehaviourMarine:
		case I_BehaviourSeal:
			return &((MARINE_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		case I_BehaviourPredator:
			return &((PREDATOR_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		case I_BehaviourXenoborg:
			return &((XENO_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		case I_BehaviourQueenAlien:
			return &((QUEEN_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		case I_BehaviourFaceHugger:
			return &((FACEHUGGER_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		case I_BehaviourAutoGun:
			return &((AUTOGUN_STATUS_BLOCK *)sbPtr->SBdataptr)->HModelController;
		default:
			return NULL;
	}
}

static STRATEGYBLOCK *FindObjectFromNetIndex(int obIndex)
{
	int sbIndex = 0;

	// first of all, check for index "GHOST_PLAYEROBJECTID": that's the player
	if (obIndex == GHOST_PLAYEROBJECTID) {
		return (Player->ObStrategyBlock);
	}

	while (sbIndex < NumActiveStBlocks)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[sbIndex++];

		/* need to be careful here: netIndexes maybe the same as pre-assigned names
		for global objects, therefore we must check the type of the object */
		if ((sbPtr->I_SBtype == I_BehaviourRocket) ||
		    (sbPtr->I_SBtype == I_BehaviourGrenade) ||
		    (sbPtr->I_SBtype == I_BehaviourPulseGrenade) ||
		    (sbPtr->I_SBtype == I_BehaviourFragmentationGrenade) ||
		    (sbPtr->I_SBtype == I_BehaviourFlareGrenade) ||
		    (sbPtr->I_SBtype == I_BehaviourPredatorEnergyBolt) ||
		    (sbPtr->I_SBtype == I_BehaviourFrisbeeEnergyBolt) ||
		    (sbPtr->I_SBtype == I_BehaviourPPPlasmaBolt) ||
		    (sbPtr->I_SBtype == I_BehaviourSpeargunBolt) ||
		    (sbPtr->I_SBtype == I_BehaviourClusterGrenade) ||
		    (sbPtr->I_SBtype == I_BehaviourNPCPredatorDisc) ||
		    (sbPtr->I_SBtype == I_BehaviourPredatorDisc_SeekTrack) ||
		    (sbPtr->I_SBtype == I_BehaviourAlienSpit) ||
		    (sbPtr->I_SBtype == I_BehaviourAutoGun) ||
		    (sbPtr->I_SBtype == I_BehaviourAlien) ||
		    (sbPtr->I_SBtype == I_BehaviourMarine) ||
		    (sbPtr->I_SBtype == I_BehaviourSeal) ||
		    (sbPtr->I_SBtype == I_BehaviourPredator) ||
		    (sbPtr->I_SBtype == I_BehaviourXenoborg) ||
		    (sbPtr->I_SBtype == I_BehaviourQueenAlien) ||
		    (sbPtr->I_SBtype == I_BehaviourFaceHugger) ||
		    (sbPtr->I_SBtype == I_BehaviourNetCorpse) ||
		    (sbPtr->I_SBtype == I_BehaviourInanimateObject) ||
		    (sbPtr->I_SBtype == I_BehaviourProximityGrenade))
		{
			int *sbIdPtr = (int *)(&(sbPtr->SBname[4]));

			if (*sbIdPtr == obIndex) {
				return sbPtr;
			}
		}
	}

	return (STRATEGYBLOCK *)0;
}

/* Finds the inanimate objects strategy block who's name is passed */
/* NB must be careful finding obejcts from their name: local and global objects may end up
with the same name, so we must make sure we differentiate between them... */
static STRATEGYBLOCK *FindEnvironmentObjectFromName(char *name)
{
	int sbIndex = 0;

	while (sbIndex < NumActiveStBlocks)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[sbIndex++];

		if ((sbPtr->I_SBtype == I_BehaviourInanimateObject) ||
		    (sbPtr->I_SBtype == I_BehaviourPlatform) ||
		    (sbPtr->I_SBtype == I_BehaviourPlacedLight) ||
		    (sbPtr->I_SBtype == I_BehaviourBinarySwitch))
		{
			if (NAME_ISEQUAL((&(sbPtr->SBname[0])), (name))) {
				return sbPtr;
			}
		}
	}

	return (STRATEGYBLOCK *)0;
}


// What a message about damage to a breakable thing names. In the mode that plays as single player does, that can be a track
// object. It's a search of its own because the one above is shared with messages about other things.
static STRATEGYBLOCK *FindBreakableFromName(char *name)
{
	STRATEGYBLOCK *const found = FindEnvironmentObjectFromName(name);

	if (found || !SameAsSinglePlayer())
	{
		return found;
	}

	for (int sbIndex = 0; sbIndex < NumActiveStBlocks; sbIndex++)
	{
		STRATEGYBLOCK *const sbPtr = ActiveStBlockList[sbIndex];

		if (sbPtr->I_SBtype == I_BehaviourTrackObject && NAME_ISEQUAL((&(sbPtr->SBname[0])), (name)))
		{
			return sbPtr;
		}
	}

	return (STRATEGYBLOCK *)0;
}

// The switch a press message names. The search above leaves out switches with a link group, so a press of one from another machine used to be lost, and the host's sync then put it back on the
// machine that pressed it. This is a search of its own because the one above is shared with messages that do other things to what they find, such as damage, and they shouldn't get a new kind of thing.
static STRATEGYBLOCK *FindSwitchFromName(char *name)
{
	for (int sbIndex = 0; sbIndex < NumActiveStBlocks; sbIndex++)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[sbIndex];

		if (NetPressMessageCanPress(sbPtr->I_SBtype, I_BehaviourBinarySwitch, I_BehaviourLinkSwitch) && NAME_ISEQUAL((&(sbPtr->SBname[0])), (name)))
		{
			return sbPtr;
		}
	}

	return (STRATEGYBLOCK *)0;
}

/* asigns an object id to a strategyblock, such as a projectile */
#if 0
void AddNetGameObjectID(STRATEGYBLOCK *sbPtr)
{
	int *sbIdPtr = (int *)(&(sbPtr->SBname[0]));
	*sbIdPtr = netNextLocalObjectId++;
}
#endif


/* called by host only: updates the scores for a described kill, and sends a
game score update message */

static void UpdateNetworkGameScores(NetID playerKilledId, NetID killerId, NETGAME_CHARACTERTYPE playerKilledType, NETGAME_CHARACTERTYPE killerType)
{
	int playerKilledIndex;
	int killerIndex;
	int scoreForKill;
	int i;
	LOCALASSERT(AvP.Network == I_Host);

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	/* get the index of the player who has been killed. If they're not in
	the player list, can't have been killed ! */
	playerKilledIndex = PlayerIdInPlayerList(playerKilledId);

	if (playerKilledIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	if (killerId == 0 || killerId == playerKilledId || killerType >= NGCT_AI_Alien)
	{
		// suicide
		killerIndex = playerKilledIndex;
		// record player's death for purposes of adjusting difficulty.
		// (assuming that optionis being used)
		GeneratorBalance_NotePlayerDeath();
	}
	else
	{
		killerIndex = PlayerIdInPlayerList(killerId);

		if (killerIndex == NET_IDNOTINPLAYERLIST) {
			return;
		}
	}

	if (killerType >= NGCT_AI_Alien)
	{
		// update deaths from AI;
		netGameData.playerData[playerKilledIndex].deathsFromAI++;
	}
	else
	{
		// update frag count;
		netGameData.playerData[killerIndex].playerFrags[playerKilledIndex]++;
	}

	// update overall number of kills
	netGameData.numDeaths[playerKilledType]++;

	if (netGameData.gameType == NGT_LastManStanding)
	{
		if (killerType == NGCT_Alien || killerIndex == playerKilledIndex)
		{
			if (playerKilledType != NGCT_Alien)
			{
				int marineCount = 0;
				int marineIndex;

				// give a point to the alien for killing a marine
				if (killerType == NGCT_Alien) {
					netGameData.playerData[killerIndex].playerScore += 1;
				}

				// also give a point to every surviving marine
				for (i = 0; i < NET_MAXPLAYERS; i++)
				{
					if (i == playerKilledIndex) {
						continue;
					}

					if (netGameData.playerData[i].playerId)
					{
						if (netGameData.playerData[i].characterType != NGCT_Alien)
						{
							marineCount++;
							marineIndex = i;
							netGameData.playerData[i].playerScore += 1;
							AddNetMsg_ScoreChange(i, i);
						}
					}
				}

				if (marineCount == 1)
				{
					// only one marine left , tell everyone
					AddNetMsg_PlayerID(netGameData.playerData[marineIndex].playerId, NetMT_LastManStanding_LastMan);
					Handle_LastManStanding_LastMan(netGameData.playerData[marineIndex].playerId);
				}
			}
		}
		else
		{
			if (playerKilledType != NGCT_Alien)
			{
				// lose a point for killing a marine
				netGameData.playerData[killerIndex].playerScore -= 1;
			}
			else
			{
				// if we're the last marine gain a point for killing an alien
				for (i = 0; i < NET_MAXPLAYERS; i++)
				{
					if (i == killerIndex) {
						continue;
					}

					if (netGameData.playerData[i].playerId)
					{
						if (netGameData.playerData[i].characterType != NGCT_Alien) {
							break;
						}
					}
				}

				if (i == NET_MAXPLAYERS) {
					netGameData.playerData[killerIndex].playerScore += 1;
				}
			}
		}
	}
	else if (netGameData.gameType == NGT_Coop)
	{
		// do nothing
	}
	else
	{
		NETGAME_CHARACTERTYPE playerKilledCopy = netGameData.playerData[playerKilledIndex].characterType;
		NETGAME_CHARACTERTYPE killerCopy = netGameData.playerData[killerIndex].characterType;
		// temporarily set the character types to what they were at the time of death
		netGameData.playerData[playerKilledIndex].characterType = playerKilledType;
		netGameData.playerData[killerIndex].characterType = killerType;
		// get score for this kill
		scoreForKill = GetNetScoreForKill(playerKilledIndex, killerIndex);
		// restore character types
		netGameData.playerData[playerKilledIndex].characterType = playerKilledCopy;
		netGameData.playerData[killerIndex].characterType = killerCopy;
		// update score
		netGameData.playerData[killerIndex].playerScore += scoreForKill;

		if (scoreForKill > 0)
		{
			// note score against person being killed
			netGameData.playerData[playerKilledIndex].playerScoreAgainst += scoreForKill;
		}
		else
		{
			netGameData.playerData[killerIndex].playerScoreAgainst += scoreForKill;
		}

		if (netGameData.gameType == NGT_CoopDeathmatch)
		{
			// need to adjust the species scores as well
			// The killer can be the computer's aliens, which are numbered past the three species
			if (NetNumberFitsTable(killerType, 3))
			{
				netGameData.teamScores[killerType] += scoreForKill;
			}
			else
			{
				Net_NoteRefused("a killer's kind has no species score");
			}

			AddNetMsg_SpeciesScores();
		}
	}

	if (killerType >= NGCT_AI_Alien) {
		// killed by alien ai
		AddNetMsg_ScoreChange(NET_MAXPLAYERS, playerKilledIndex);
	}
	else {
		AddNetMsg_ScoreChange(killerIndex, playerKilledIndex);
	}

	// AddNetMsg_PlayerScores(killerIndex);

	if (netGameData.gameType == NGT_PredatorTag || netGameData.gameType == NGT_AlienTag)
	{
		NETGAME_CHARACTERTYPE tagSpecies = NGCT_Predator;

		if (netGameData.gameType == NGT_AlienTag) {
			tagSpecies = NGCT_Alien;
		}

		// was the predator killed?
		if (playerKilledType == tagSpecies)
		{
			if (CountPlayersOfType(tagSpecies) == 1)
			{
				// select new predator
				// if it wasn't suicide , choose killer
				if (playerKilledIndex != killerIndex)
				{
					AddNetMsg_PlayerID(killerId, NetMT_PredatorTag_NewPredator);
					Handle_SpeciesTag_NewPersonIt(killerId);
				}
				else
				{
					//c hoose next player
					for (i = playerKilledIndex + 1;; i++)
					{
						i = i % NET_MAXPLAYERS;

						if (netGameData.playerData[i].playerId)
						{
							/*
							don't choose player if he was the previous predator
							(In fact this should only happen if someone is silly enough to player single player
							predator tag).
							*/
							if (i != playerKilledIndex)
							{
								AddNetMsg_PlayerID(netGameData.playerData[i].playerId, NetMT_PredatorTag_NewPredator);
								Handle_SpeciesTag_NewPersonIt(netGameData.playerData[i].playerId);
							}

							break;
						}
					}
				}
			}
		}
	}
}

/* This function is a hook for the playerdead() function in player.c:
if we're the host, we need to update the scores seperately in the event that we have
been killed during a netgame. For other players, scores are updated (by the host) in
response to a 'playerKilled' message.  However, the host will not receive it's own
playerKilled message, and must therefore do it seperately... */
void DoNetScoresForHostDeath(NETGAME_CHARACTERTYPE myType, NETGAME_CHARACTERTYPE killerType)
{
	LOCALASSERT(AvP.Network == I_Host);

	if (myNetworkKillerId)
	{
		int killer_index = PlayerIdInPlayerList(myNetworkKillerId);

		if (killer_index == NET_IDNOTINPLAYERLIST)
		{
			// the player doing the damage has either left the game , or never existed.
			// call it suicide then.
			myNetworkKillerId = AvPNetID;
		}
	}

	UpdateNetworkGameScores(AvPNetID, myNetworkKillerId, myType, killerType);
}

int AddUpPlayerFrags(int playerId)
{
	int score = 0;
	LOCALASSERT(netGameData.playerData[playerId].playerId != 0);

	for (int j = 0; j < (NET_MAXPLAYERS); j++)
	{
		score += netGameData.playerData[playerId].playerFrags[j];
	}

	return score;
}

static void ConvertNetNameToUpperCase(char *strPtr)
{
	int count = 0;

	while ((count < (NET_PLAYERNAMELENGTH - 1)) && (strPtr[count] != '\0'))
	{
		strPtr[count] = toupper(strPtr[count]);
		count++;
	}
}

/* Patrick 11/7/97 ----------------------------------------------
Functions for determining our sequence for player update messages
-----------------------------------------------------------------*/

static MARINE_SEQUENCE GetMyMarineSequence(void)
{
	int playerIsMoving = 0;
	int playerIsFiring = 0;
	int playerIsCrouching = 0;
	int playerIsAlive = 0;
	int playerIsJumping = 0;
	int usingCloseAttackWeapon;

	// sort out what state we're in
	if (PlayerStatusPtr->IsAlive) {
		playerIsAlive = 1;
	}
	else {
		playerIsAlive = 0;
	}

	if (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward) {
		playerIsMoving = -1;
	}
	else if ((PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward) ||
	         (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft) ||
	         (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight))
	{
		playerIsMoving = 1;
	}
	else {
		playerIsMoving = 0;
	}

	if ((Player->ObStrategyBlock->DynPtr->Position.vx == Player->ObStrategyBlock->DynPtr->PrevPosition.vx)
	    && (Player->ObStrategyBlock->DynPtr->Position.vy == Player->ObStrategyBlock->DynPtr->PrevPosition.vy)
	    && (Player->ObStrategyBlock->DynPtr->Position.vz == Player->ObStrategyBlock->DynPtr->PrevPosition.vz))
	{
		// Actually not moving - overruled!
		playerIsMoving = 0;
	}

	if (PlayerStatusPtr->ShapeState != PMph_Standing) {
		playerIsCrouching = 1;
	}
	else {
		playerIsCrouching = 0;
	}

	if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_FIRING_PRIMARY) ||
	    (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_PRIMARY))
	{
		playerIsFiring = 1;
	}
	else
	{
		if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_MARINE_PISTOL)
		{
			if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_FIRING_SECONDARY) ||
			    (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_SECONDARY))
			{
				playerIsFiring = 1;
			}
			else {
				playerIsFiring = 0;
			}
		}
		else {
			playerIsFiring = 0;
		}
	}

	if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_CUDGEL) {
		usingCloseAttackWeapon = 1;
	}
	else {
		usingCloseAttackWeapon = 0;
	}

	/* Fix cudgel. */
	if (usingCloseAttackWeapon)
	{
		if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_FIRING_SECONDARY) ||
		    (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_SECONDARY))
		{
			playerIsFiring = 1;
		}
	}

	/* KJL 14:27:14 10/29/97 - deal with jumping & falling */
	{
		DYNAMICSBLOCK *dynPtr = Player->ObStrategyBlock->DynPtr;

		if (!dynPtr->IsInContactWithFloor && (dynPtr->TimeNotInContactWithFloor == 0)) {
			playerIsJumping = 1;
		}
	}

	// and deduce the sequence
	if (playerIsAlive == 0)
	{
		if (playerIsCrouching) {
			return MSQ_CrouchDie;
		}
		else {
			return MSQ_StandDieFront;
		}
	}

	if (playerIsJumping) {
		return MSQ_Jump;
	}

	// Put this in here... no running cudgel attacks yet.
	if (playerIsFiring && usingCloseAttackWeapon)
	{
		// Deal with cudgel case!
		if (StaffAttack >= 0) {
			return(static_cast<enum MarineSequence>(MSQ_BaseOfCudgelAttacks + StaffAttack));
		}
	}

	if (playerIsCrouching)
	{
		if (playerIsMoving > 0) {
			return MSQ_Crawl;
		}
		else if (playerIsMoving < 0) {
			return MSQ_Crawl_Backwards;
		}
		else {
			return MSQ_Crouch;
		}
	}

	if (playerIsMoving > 0)
	{
		if (playerIsFiring)
		{
			if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_TWO_PISTOLS)
			    && (LastHand))
			{
				return MSQ_RunningFireSecondary;
			}
			else {
				return MSQ_RunningFire;
			}
		}
		else {
			return MSQ_Walk;
		}
	}
	else if (playerIsMoving < 0)
	{
		if (playerIsFiring)
		{
			if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_TWO_PISTOLS)
			    && (LastHand))
			{
				return MSQ_RunningFireSecondary_Backwards;
			}
			else {
				return MSQ_RunningFire_Backwards;
			}
		}
		else {
			return MSQ_Walk_Backwards;
		}
	}

	if (playerIsFiring)
	{
		if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_TWO_PISTOLS)
		    && (LastHand))
		{
			return MSQ_StandingFireSecondary;
		}
		else {
			return MSQ_StandingFire;
		}
	}
	else
	{
		if (PlayerStatusPtr->tauntTimer != 0) {
			return MSQ_Taunt;
		}
		else {
			return MSQ_Stand;
		}
	}
}

static ALIEN_SEQUENCE GetMyAlienSequence(void)
{
	int playerIsMoving = 0;
	int playerIsFiring = 0;
	int playerIsCrouching = 0;
	int playerIsAlive = 0;
	int playerIsJumping = 0;

	// sort out what state we're in
	if (PlayerStatusPtr->IsAlive) {
		playerIsAlive = 1;
	}
	else {
		playerIsAlive = 0;
	}

	if (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward) {
		playerIsMoving = -1;
	}
	else if ((PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward) ||
	         (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft) ||
	         (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight))
	{
		playerIsMoving = 1;
	}
	else {
		playerIsMoving = 0;
	}

	if ((Player->ObStrategyBlock->DynPtr->Position.vx == Player->ObStrategyBlock->DynPtr->PrevPosition.vx)
	    && (Player->ObStrategyBlock->DynPtr->Position.vy == Player->ObStrategyBlock->DynPtr->PrevPosition.vy)
	    && (Player->ObStrategyBlock->DynPtr->Position.vz == Player->ObStrategyBlock->DynPtr->PrevPosition.vz))
	{
		// Actually not moving - overruled!
		playerIsMoving = 0;
	}

	if (PlayerStatusPtr->ShapeState != PMph_Standing) {
		playerIsCrouching = 1;
	}
	else {
		playerIsCrouching = 0;
	}

	/* ChrisF 20/4/98: playerIsFiring now specifies alien weapon behaviour. */
	// if((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState==WEAPONSTATE_FIRING_PRIMARY)||
	//   (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState==WEAPONSTATE_RECOIL_PRIMARY))
	//      playerIsFiring = 1;
	// else playerIsFiring = 0;
	//
	// if(PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber!=WEAPON_ALIEN_SPIT) {
	//  usingCloseAttackWeapon = 1;
	// } else {
	//  usingCloseAttackWeapon = 0;
	// }

	switch (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState)
	{
		case (WEAPONSTATE_FIRING_PRIMARY):
			if (Biting) {
				playerIsFiring = 4; // Eat.
			}
			else {
				playerIsFiring = 1; // Claw.
			}

			break;

		case (WEAPONSTATE_FIRING_SECONDARY):
			playerIsFiring = 2; // Tail Poise.
			break;

		case (WEAPONSTATE_RECOIL_SECONDARY):
			playerIsFiring = 3; // Tail Strike.
			break;

		default:
			playerIsFiring = 0; // Nothing.
			break;
	}

	/* KJL 14:27:14 10/29/97 - deal with jumping & falling */
	{
		DYNAMICSBLOCK *dynPtr = Player->ObStrategyBlock->DynPtr;

		if (!dynPtr->IsInContactWithFloor && (dynPtr->TimeNotInContactWithFloor == 0))
		{
			playerIsJumping = 1;
		}
	}

	// and deduce the sequence
	if (playerIsAlive == 0)
	{
		return ASQ_Stand; // kind of irrelevant really
	}

	if (playerIsJumping) // TODO: consider jump & crouch
	{
		switch (playerIsFiring)
		{
			case 1:
				return ASQ_Pounce;
				break;

			case 2:
				return ASQ_JumpingTailPoise;
				break;

			case 3:
				return ASQ_JumpingTailStrike;
				break;

			case 4:
				// What the hell?
				return ASQ_Eat;
				break;

			default:
				return ASQ_Jump;
				break;
		}
	}

	if (playerIsCrouching)
	{
		if (playerIsMoving > 0)
		{
			switch (playerIsFiring)
			{
				case 1:
					return ASQ_CrawlingAttack_Claw;
					break;

				case 2:
					return ASQ_CrawlingTailPoise;
					break;

				case 3:
					return ASQ_CrawlingTailStrike;
					break;

				case 4:
					// What the hell?
					return ASQ_CrouchEat;
					break;

				default:
					if (Player->ObStrategyBlock->DynPtr->OrientMat.mat22 > 50000) {
						return ASQ_Scamper;
					}
					else {
						return ASQ_Crawl;
					}
					break;
			}
		}
		else if (playerIsMoving < 0)
		{
			switch (playerIsFiring)
			{
				case 1:
					return ASQ_CrawlingAttack_Claw_Backwards;
					break;

				case 2:
					return ASQ_CrawlingTailPoise_Backwards;
					break;

				case 3:
					return ASQ_CrawlingTailStrike_Backwards;
					break;

				case 4:
					// What the hell?
					return ASQ_CrouchEat;
					break;

				default:
					if (Player->ObStrategyBlock->DynPtr->OrientMat.mat22 > 50000) {
						return ASQ_Scamper_Backwards;
					}
					else {
						return ASQ_Crawl_Backwards;
					}

					break;
			}
		}

		switch (playerIsFiring)
		{
			case 1:
				return ASQ_CrouchedAttack_Claw;
				break;

			case 2:
				return ASQ_CrouchedTailPoise;
				break;

			case 3:
				return ASQ_CrouchedTailStrike;
				break;

			case 4:
				return ASQ_Eat;
				break;

			default:
				return ASQ_Crouch;
				break;
		}
	}

	if (playerIsMoving > 0)
	{
		switch (playerIsFiring)
		{
			case 1:
				return ASQ_RunningAttack_Claw;
				break;

			case 2:
				return ASQ_RunningTailPoise;
				break;

			case 3:
				return ASQ_RunningTailStrike;
				break;

			case 4:
				// What the hell?
				return ASQ_Eat;
				break;

			default:
				return ASQ_Run;
				break;
		}
	}
	else if (playerIsMoving < 0)
	{
		switch (playerIsFiring)
		{
			case 1:
				return ASQ_RunningAttack_Claw_Backwards;
				break;

			case 2:
				return ASQ_RunningTailPoise_Backwards;
				break;

			case 3:
				return ASQ_RunningTailStrike_Backwards;
				break;

			case 4:
				// What the hell?
				return ASQ_Eat;
				break;

			default:
				return ASQ_Run_Backwards;
				break;
		}
	}

	switch (playerIsFiring)
	{
		case 1:
			return ASQ_StandingAttack_Claw;
			break;

		case 2:
			return ASQ_StandingTailPoise;
			break;

		case 3:
			return ASQ_StandingTailStrike;
			break;

		case 4:
			return ASQ_Eat;
			break;

		default:
			if (PlayerStatusPtr->tauntTimer != 0)
			{
				return ASQ_Taunt; // Second lowest priority ever.
			}
			else {
				return ASQ_Stand;
			}
			break;
	}
}

static PREDATOR_SEQUENCE GetMyPredatorSequence(void)
{
	int playerIsMoving = 0;
	int playerIsFiring = 0;
	int playerIsCrouching = 0;
	int playerIsAlive = 0;
	int playerIsJumping = 0;
	int usingCloseAttackWeapon;

	// sort out what state we're in
	if (PlayerStatusPtr->IsAlive) {
		playerIsAlive = 1;
	}
	else {
		playerIsAlive = 0;
	}

	if (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_Backward) {
		playerIsMoving = -1;
	}
	else if ((PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_Forward) ||
	         (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepLeft) ||
	         (PlayerStatusPtr->Mvt_InputRequests.Flags.Rqst_SideStepRight))
	{
		playerIsMoving = 1;
	}
	else {
		playerIsMoving = 0;
	}

	if ((Player->ObStrategyBlock->DynPtr->Position.vx == Player->ObStrategyBlock->DynPtr->PrevPosition.vx)
	    && (Player->ObStrategyBlock->DynPtr->Position.vy == Player->ObStrategyBlock->DynPtr->PrevPosition.vy)
	    && (Player->ObStrategyBlock->DynPtr->Position.vz == Player->ObStrategyBlock->DynPtr->PrevPosition.vz))
	{
		// Actually not moving - overruled!
		playerIsMoving = 0;
	}

	if (PlayerStatusPtr->ShapeState != PMph_Standing) {
		playerIsCrouching = 1;
	}
	else {
		playerIsCrouching = 0;
	}

	playerIsFiring = 0;

	if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_PRED_SHOULDERCANNON)
	{
		// the shoulder cannon is fired during recoil (I think)
		if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_PRIMARY) {
			playerIsFiring = 1;
		}
	}
	else if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_PRED_WRISTBLADE)
	{
		if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_FIRING_PRIMARY) ||
		    (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_PRIMARY) ||
		    (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_SECONDARY))
		{
			if (StaffAttack != -1) {
				playerIsFiring = 1;
			}
		}
	}
	else
	{
		if ((PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_FIRING_PRIMARY) ||
		    (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].CurrentState == WEAPONSTATE_RECOIL_PRIMARY))
		{
			playerIsFiring = 1;
		}
	}

	if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_PRED_WRISTBLADE) {
		usingCloseAttackWeapon = 3;
	}
	else if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_PRED_DISC) {
		usingCloseAttackWeapon = 1;
	}
	else if (PlayerStatusPtr->WeaponSlot[PlayerStatusPtr->SelectedWeaponSlot].WeaponIDNumber == WEAPON_PRED_STAFF) {
		usingCloseAttackWeapon = 2;
	}
	else {
		usingCloseAttackWeapon = 0;
	}

	/* KJL 14:27:14 10/29/97 - deal with jumping & falling */
	{
		DYNAMICSBLOCK *dynPtr = Player->ObStrategyBlock->DynPtr;

		if (!dynPtr->IsInContactWithFloor && (dynPtr->TimeNotInContactWithFloor == 0)) {
			playerIsJumping = 1;
		}
	}

	/* and deduce the sequence */
	if (playerIsAlive == 0)
	{
		if (playerIsCrouching) {
			return PredSQ_CrouchDie;
		}
		else {
			return PredSQ_StandDie;
		}
	}

	if (playerIsJumping) {
		return(PredSQ_Jump);
	}

	if (playerIsCrouching)
	{
		if (playerIsMoving > 0)
		{
			if (playerIsFiring && usingCloseAttackWeapon)
			{
				// Deal with staff case!
				if (usingCloseAttackWeapon == 2)
				{
					if (StaffAttack >= 0) {
						return(static_cast<enum predatorsequence>(PredSQ_BaseOfStaffAttacks + StaffAttack));
					}
				}
				else if (usingCloseAttackWeapon == 3)
				{
					if (StaffAttack >= 0) {
						return(static_cast<enum predatorsequence>(PredSQ_BaseOfWristbladeAttacks + StaffAttack));
					}
				}

				return PredSQ_CrawlingSwipe;
			}
			else {
				return PredSQ_Crawl;
			}
		}
		else if (playerIsMoving < 0)
		{
			if (playerIsFiring && usingCloseAttackWeapon)
			{
				// Deal with staff case!
				if (usingCloseAttackWeapon == 2)
				{
					if (StaffAttack >= 0) {
						return(static_cast<enum predatorsequence>(PredSQ_BaseOfStaffAttacks + StaffAttack));
					}
				}
				else if (usingCloseAttackWeapon == 3)
				{
					if (StaffAttack >= 0) {
						return(static_cast<enum predatorsequence>(PredSQ_BaseOfWristbladeAttacks + StaffAttack));
					}
				}

				return PredSQ_CrawlingSwipe_Backwards;
			}
			else {
				return PredSQ_Crawl_Backwards;
			}
		}

		if (playerIsFiring && usingCloseAttackWeapon)
		{
			// Deal with staff case!
			if (usingCloseAttackWeapon == 2)
			{
				if (StaffAttack >= 0) {
					return(static_cast<enum predatorsequence>(PredSQ_BaseOfStaffAttacks + StaffAttack));
				}
			}
			else if (usingCloseAttackWeapon == 3)
			{
				if (StaffAttack >= 0) {
					return(static_cast<enum predatorsequence>(PredSQ_BaseOfWristbladeAttacks + StaffAttack));
				}
			}

			return PredSQ_CrouchedSwipe;
		}
		else {
			return PredSQ_Crouch;
		}
	}

	if (playerIsMoving > 0)
	{
		if (playerIsFiring && usingCloseAttackWeapon)
		{
			// Deal with staff case!
			if (usingCloseAttackWeapon == 2)
			{
				if (StaffAttack >= 0) {
					return(static_cast<enum predatorsequence>(PredSQ_BaseOfStaffAttacks + StaffAttack));
				}
			}
			else if (usingCloseAttackWeapon == 3)
			{
				if (StaffAttack >= 0) {
					return(static_cast<enum predatorsequence>(PredSQ_BaseOfWristbladeAttacks + StaffAttack));
				}
			}

			return PredSQ_RunningSwipe;
		}
		else {
			return PredSQ_Run;
		}
	}
	else if (playerIsMoving < 0)
	{
		if (playerIsFiring && usingCloseAttackWeapon)
		{
			// Deal with staff case!
			if (usingCloseAttackWeapon == 2)
			{
				if (StaffAttack >= 0) {
					return(static_cast<enum predatorsequence>(PredSQ_BaseOfStaffAttacks + StaffAttack));
				}
			}
			else if (usingCloseAttackWeapon == 3)
			{
				if (StaffAttack >= 0) {
					return(static_cast<enum predatorsequence>(PredSQ_BaseOfWristbladeAttacks + StaffAttack));
				}
			}

			return PredSQ_RunningSwipe_Backwards;
		}
		else {
			return PredSQ_Run_Backwards;
		}
	}

	if (playerIsFiring && usingCloseAttackWeapon)
	{
		// Deal with staff case!
		if (usingCloseAttackWeapon == 2)
		{
			if (StaffAttack >= 0) {
				return(static_cast<enum predatorsequence>(PredSQ_BaseOfStaffAttacks + StaffAttack));
			}
		}
		else if (usingCloseAttackWeapon == 3)
		{
			if (StaffAttack >= 0) {
				return(static_cast<enum predatorsequence>(PredSQ_BaseOfWristbladeAttacks + StaffAttack));
			}
		}

		return PredSQ_StandingSwipe;
	}
	else
	{
		if (PlayerStatusPtr->tauntTimer != 0) {
			return PredSQ_Taunt;
		}
		else {
			return PredSQ_Stand;
		}
	}
}

/* Patrick 11/7/97 ----------------------------------------------
Functions for transmitting state changes
-----------------------------------------------------------------*/
void TransmitEndOfGameNetMsg(void)
{
	/* first, initialise the send buffer: this means that we may loose some stacked
	messages, but this should be ok.*/
	InitialiseSendMessageBuffer();
	netGameData.myGameState = NGS_EndGameScreen;

	for (int i = 0; i < NET_MESSAGEITERATIONS; i++)
	{
		AddNetMsg_AllGameScores();
		// AddNetMsg_EndGame();
		NetSendMessagesNow();
	}

	netGameData.stateCheckTimeDelay = 3 * ONE_FIXED;
}

void TransmitPlayerLeavingNetMsg(void)
{
	/* first, initialise the send buffer: this means that we may loose some stacked
	messages, but this should be ok.*/
	InitialiseSendMessageBuffer();

	for (int i = 0; i < NET_MESSAGEITERATIONS; i++)
	{
		AddNetMsg_PlayerLeaving();
		NetSendMessagesNow();
	}
}

void TransmitStartGameNetMsg(void)
{
	/* first, initialise the send buffer: this means that we may loose some stacked
	messages, but this should be ok.*/
	InitialiseSendMessageBuffer();

	for (int i = 0; i < NET_MESSAGEITERATIONS; i++)
	{
		AddNetMsg_StartGame();
		NetSendMessagesNow();
	}
}

/* Patrick 29/7/97 --------------------------------------------------
Stuff for assigning starting positions to network players
---------------------------------------------------------------------*/
#if 0
void TeleportNetPlayerToAStartingPosition(STRATEGYBLOCK *playerSbPtr, int startOfGame)
{
	int numStartPositions;
	VECTORCH *startPositions;
	int sbIndex = 0;
	int numReadThro, numConsidered;
	int start_index = 0;
	int found = 0;
	PLAYER_STATUS *psPtr = (PLAYER_STATUS *)playerSbPtr->SBdataptr;

	/* some basic checks */
	if (playerSbPtr == NULL)
	{
		return;
	}

	if (playerSbPtr->DynPtr == NULL)
	{
		return;
	}

	if (!ActiveStBlockList)
	{
		return;
	}

	if (NumActiveStBlocks <= 0)
	{
		return;
	}

	//set the players invulnerability timer
	GLOBALASSERT(psPtr);
	psPtr->invulnerabilityTimer = netGameData.invulnerableTime * ONE_FIXED;

	//select the start positions for this character type
	switch (AvP.PlayerType)
	{
		case I_Marine :
			numStartPositions = numMarineStartPos;
			startPositions = marineStartPositions;
			break;

		case I_Predator :
			numStartPositions = numPredatorStartPos;
			startPositions = predatorStartPositions;
			break;

		case I_Alien :
			numStartPositions = numAlienStartPos;
			startPositions = alienStartPositions;
			break;
	}

	if (!numStartPositions)
	{
		return;
	}

	/* pick a starting point*/
	if (startOfGame)
	{
		numReadThro = (PlayerIdInPlayerList(AvPNetID)) % 8;

		while (numReadThro < 0)
		{
			//Probably haven't received the details of all the players yet
			MinimalNetCollectMessages();
			numReadThro = (PlayerIdInPlayerList(AvPNetID)) % 8;
		}
	}
	else
	{
		numReadThro = FastRandom() % numStartPositions;
	}

	numConsidered = 0;

	/* and go through the list */
	for (start_index = numReadThro; !found; start_index++)
	{
		start_index %= numStartPositions;
		/* we are going to try this one: see if it's clear */
		{
			int sbIndex2 = 0;
			STRATEGYBLOCK *sbPtr2;
			int obstructed = 0;

			while ((sbIndex2 < NumActiveStBlocks) && (!obstructed))
			{
				sbPtr2 = ActiveStBlockList[sbIndex2++];

				if (sbPtr2->I_SBtype == I_BehaviourNetGhost)
				{
					NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK *)sbPtr2->SBdataptr;
					LOCALASSERT(ghostData);

					if ((ghostData->type == I_BehaviourMarinePlayer) ||
					    (ghostData->type == I_BehaviourPredatorPlayer) ||
					    (ghostData->type == I_BehaviourAlienPlayer))
					{
						VECTORCH seperationVec;
						LOCALASSERT(sbPtr2->DynPtr);
						seperationVec = sbPtr2->DynPtr->Position;
						seperationVec.vx -= startPositions[start_index].vx;
						seperationVec.vy -= startPositions[start_index].vy;
						seperationVec.vz -= startPositions[start_index].vz;

						if ((Magnitude(&seperationVec) < 2000) && (numConsidered < 8))
						{
							obstructed = 1;
						}
					}
				}
			}

			numConsidered++;

			if (!obstructed)
			{
				/* found a clear start position */
				playerSbPtr->DynPtr->Position = playerSbPtr->DynPtr->PrevPosition = startPositions[start_index];
				found = 1;
			}
		}
	}
}
#else
/*----------------------------------------------------------------------
  Where a player goes on a level with no multiplayer starts. None of the 32 single player levels has any.
  ----------------------------------------------------------------------*/

extern VECTORCH PlayerStartLocation;
extern MATRIXCH PlayerStartMat;

// Everything about the spot that this looks at: a body is 450 mm from middle to edge, with its "position" at its
// feet (CollisionExtents, in extents.cpp). The lines are cast from about chest height.
static const int kChestHeight = 1000;
static const int kBodyWidth = 500; // the 450, and a little to spare
static const int kMostStepDown = 800;
static const int kMostStepUp = 400;
// How far above the floor a level's start can be, and how far above his feet the line looking for that floor begins.
static const int kMostDropFromAStart = 8000;
static const int kLookForTheFloorFromAbove = 300;

// How tall this player stands, which depends on what he is: a Marine 1950 mm, an Alien 2033 and a Predator 2098.
// A place that only just has room for a Marine's head would leave a Predator stuck.
static int BodyHeight(void)
{
	COLLISION_EXTENTS_ID const kind = AvP.PlayerType == I_Predator ? CE_PREDATOR : AvP.PlayerType == I_Alien ? CE_ALIEN : CE_MARINE;

	return -CollisionExtents[kind].StandingTop;
}

// How far a line runs before it meets the level or something in it, up to this many mm. Other players don't
// count, since they move. The line can pass through one of them, but the second one it meets stops it.
static int HowFarALineRuns(VECTORCH *from, VECTORCH *direction, int mostMillimetres)
{
	DISPLAYBLOCK *otherPlayer = 0;

	for (int tries = 0; tries < 2; tries++)
	{
		FindPolygonInLineOfSight_TwoIgnores(direction, from, 0, Player, otherPlayer);

		if (LOS_ObjectHitPtr == 0 || LOS_Lambda >= mostMillimetres)
		{
			return mostMillimetres;
		}

		if (otherPlayer == 0 && LOS_ObjectHitPtr->ObStrategyBlock && LOS_ObjectHitPtr->ObStrategyBlock->I_SBtype == I_BehaviourNetGhost)
		{
			otherPlayer = LOS_ObjectHitPtr;
			continue;
		}

		return LOS_Lambda;
	}

	return LOS_Lambda;
}

// Whether a body would fit at this spot on the level, and could get there from the middle spot. If so,
// "spot" comes back with its height changed so that the feet are on the floor.
static bool ABodyFitsHere(VECTORCH const *middle, VECTORCH *spot)
{
	VECTORCH from = *middle;
	from.vy -= kChestHeight;

	// Could he walk here in a straight line, with room for his body at the far end?
	VECTORCH way;
	way.vx = spot->vx - middle->vx;
	way.vy = 0;
	way.vz = spot->vz - middle->vz;
	int const distance = Magnitude(&way);
	Normalise(&way);

	if (HowFarALineRuns(&from, &way, distance + kBodyWidth) < distance + kBodyWidth)
	{
		return false;
	}

	VECTORCH chest = *spot;
	chest.vy -= kChestHeight;

	if (ModuleFromPosition(&chest, (MODULE *)0) == 0)
	{
		return false;
	}

	// Is there a floor near enough where his feet would be?  (In this game, down is a bigger y.)
	VECTORCH down = { 0, ONE_FIXED, 0 };
	int const mostDown = kChestHeight + kMostStepDown;
	int const toFloor = HowFarALineRuns(&chest, &down, mostDown);

	if (toFloor >= mostDown || toFloor < kChestHeight - kMostStepUp)
	{
		return false;
	}

	spot->vy = chest.vy + toFloor;
	chest.vy = spot->vy - kChestHeight;

	// Room for his head, and for his body on every side?
	VECTORCH up = { 0, -ONE_FIXED, 0 };

	if (HowFarALineRuns(&chest, &up, BodyHeight() - kChestHeight) < BodyHeight() - kChestHeight)
	{
		return false;
	}

	static VECTORCH const sides[4] = { { ONE_FIXED, 0, 0 }, { -ONE_FIXED, 0, 0 }, { 0, 0, ONE_FIXED }, { 0, 0, -ONE_FIXED } };

	for (int side = 0; side < 4; side++)
	{
		VECTORCH sideways = sides[side];

		if (HowFarALineRuns(&chest, &sideways, kBodyWidth) < kBodyWidth)
		{
			return false;
		}
	}

	return true;
}

// Whether some other player is standing on this spot.
static bool SomeoneIsOnThisSpot(VECTORCH const *spot)
{
	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == 0 || netGameData.playerData[i].playerId == AvPNetID)
		{
			continue;
		}

		STRATEGYBLOCK *ghost = FindGhost(netGameData.playerData[i].playerId, GHOST_PLAYEROBJECTID);

		if (ghost && ghost->DynPtr)
		{
			VECTORCH apart;
			apart.vx = ghost->DynPtr->Position.vx - spot->vx;
			apart.vy = ghost->DynPtr->Position.vy - spot->vy;
			apart.vz = ghost->DynPtr->Position.vz - spot->vz;

			if (Magnitude(&apart) < kPlayerSpotGap - kBodyWidth)
			{
				return true;
			}
		}
	}

	return false;
}

// Finds a place for this player around a middle spot: the middle first, then the list in player_spots.h.
// Every machine that can see the same level finds the same places in the same order, and this player
// takes the one that goes with his place in the player list, so two people arriving together never pick
// the same one and nothing has to be sent. False if there's no free place here at all.
static bool FindPlaceAround(VECTORCH const *middle, MATRIXCH const *facing, int playerNumber, VECTORCH *place)
{
	// The ways he's facing, flat along the ground
	VECTORCH right = { facing->mat11, 0, facing->mat13 };
	VECTORCH forward = { facing->mat31, 0, facing->mat33 };
	static VECTORCH const noRight = { ONE_FIXED, 0, 0 };
	static VECTORCH const noForward = { 0, 0, ONE_FIXED };

	if (Magnitude(&right) == 0)
	{
		right = noRight;
	}

	if (Magnitude(&forward) == 0)
	{
		forward = noForward;
	}

	Normalise(&right);
	Normalise(&forward);

	// A level's start isn't always on the floor. temple_m's is a metre up, and the first player simply drops when the
	// level begins. Everyone else's places are looked for from the floor under it, because from up there none of them
	// would seem to have a floor within reach, and everyone would end up inside each other on the one spot.
	VECTORCH ground = *middle;
	VECTORCH lookFrom = *middle;
	VECTORCH lookDown = { 0, ONE_FIXED, 0 };
	lookFrom.vy -= kLookForTheFloorFromAbove;
	int const toTheFloor = HowFarALineRuns(&lookFrom, &lookDown, kMostDropFromAStart);

	if (toTheFloor < kMostDropFromAStart)
	{
		ground.vy = lookFrom.vy + toTheFloor;
	}

	VECTORCH places[NET_MAXPLAYERS * 2];
	int found = 1;
	places[0] = *middle;

	for (int i = 0; i < kPlayerSpotCount && found < NET_MAXPLAYERS * 2; i++)
	{
		int const toRight = kPlayerSpots[i].stepsRight * kPlayerSpotGap;
		int const toFront = kPlayerSpots[i].stepsForward * kPlayerSpotGap;

		VECTORCH spot = ground;
		spot.vx += MUL_FIXED(right.vx, toRight) + MUL_FIXED(forward.vx, toFront);
		spot.vz += MUL_FIXED(right.vz, toRight) + MUL_FIXED(forward.vz, toFront);

		if (ABodyFitsHere(&ground, &spot))
		{
			places[found++] = spot;
		}
	}

	// For the proof that all 32 levels have room. It stops counting at twice as many as can play.
	{
		char line[120];
		_snprintf(line, sizeof(line) - 1, "campaign: %d places for players were found round %d %d %d", found, middle->vx, middle->vy, middle->vz);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	if (playerNumber >= found)
	{
		LogErrorString("campaign: there are more players than places here, so some of them are sharing one");
	}

	// From this player's own place on, the first that nobody's standing on. Someone can be, if he arrived
	// earlier in the game, or the place he'd have had was taken by another who left.
	for (int i = 0; i < found; i++)
	{
		VECTORCH const *candidate = &places[(playerNumber + i) % found];

		if (!SomeoneIsOnThisSpot(candidate))
		{
			*place = *candidate;
			return true;
		}
	}

	return false;
}

// Moves the player, and brings the rest of the game up to date with where he now is.
static void PutPlayerAt(STRATEGYBLOCK *playerSbPtr, VECTORCH const *place, MATRIXCH const *facing)
{
	DYNAMICSBLOCK *dynPtr = playerSbPtr->DynPtr;

	Player->ObWorld = dynPtr->Position = dynPtr->PrevPosition = *place;
	dynPtr->OrientMat = dynPtr->PrevOrientMat = *facing;
	MatrixToEuler(&dynPtr->OrientMat, &dynPtr->OrientEuler);
	dynPtr->PrevOrientEuler = dynPtr->OrientEuler;
	playerSbPtr->containingModule = ModuleFromPosition(&Player->ObWorld, (MODULE *)0);
	playerPherModule = playerSbPtr->containingModule;

	// Dynamics needs the visibilities right for this frame, as it does after any move like this
	Global_VDB_Ptr->VDB_World = Player->ObWorld;
	AllNewModuleHandler();
	DoObjectVisibilities();
}

// Before asking about the level around a spot, the rooms round it have to be the ones in play.
static void BringThisSpotIntoPlay(VECTORCH const *spot)
{
	Global_VDB_Ptr->VDB_World = *spot;
	AllNewModuleHandler();
	DoObjectVisibilities();
}

// A teammate this far from the level's start has gone on ahead: 10 metres, in millimetres.
static const int kFarFromTheStart = 10000;

static void PlacePlayerWithNoMultiplayerStart(STRATEGYBLOCK *playerSbPtr)
{
	int playerNumber = PlayerIdInPlayerList(AvPNetID);

	if (playerNumber == NET_IDNOTINPLAYERLIST)
	{
		playerNumber = 0;
	}

	VECTORCH place;

	// Someone who joins a campaign's level after the others have gone on ahead goes to them, because the way
	// from the start may have shut behind them. Anyone still near the start doesn't count: then he
	// starts at the start, with everyone. There's no lobby to gather in first, so someone can connect
	// long after the host has started, and this is how they catch up.
	if (netGameData.coopCampaign != kCampaignNone)
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			if (netGameData.playerData[i].playerId == 0 || netGameData.playerData[i].playerId == AvPNetID || !netGameData.playerData[i].playerAlive)
			{
				continue;
			}

			STRATEGYBLOCK *teammate = FindGhost(netGameData.playerData[i].playerId, GHOST_PLAYEROBJECTID);

			if (teammate && teammate->DynPtr)
			{
				VECTORCH const middle = teammate->DynPtr->Position;
				MATRIXCH const facing = teammate->DynPtr->OrientMat;
				VECTORCH apart;

				apart.vx = middle.vx - PlayerStartLocation.vx;
				apart.vy = middle.vy - PlayerStartLocation.vy;
				apart.vz = middle.vz - PlayerStartLocation.vz;

				if (Magnitude(&apart) < kFarFromTheStart)
				{
					continue;
				}

				BringThisSpotIntoPlay(&middle);

				if (FindPlaceAround(&middle, &facing, playerNumber, &place))
				{
					PutPlayerAt(playerSbPtr, &place, &facing);
					return;
				}
			}
		}
	}

	// Otherwise, the level's own start, which is exactly where single player puts him if he's the first.
	BringThisSpotIntoPlay(&PlayerStartLocation);

	if (!FindPlaceAround(&PlayerStartLocation, &PlayerStartMat, playerNumber, &place))
	{
		// Nowhere free at all. On the start itself he's at least in the level, and they can walk apart.
		place = PlayerStartLocation;
		LogErrorString("a player has been put on the level's start with someone already on it: there's no room round it");
	}

	PutPlayerAt(playerSbPtr, &place, &PlayerStartMat);
}

void TeleportNetPlayerToAStartingPosition(STRATEGYBLOCK *playerSbPtr, int startOfGame)
{
	int numStartPositions = 0;
	MULTIPLAYER_START *startPositions = 0;
	int sbIndex = 0;
	int start_index;
	int numChecked = 0;
	int bestDistance = -1;
	int bestIndex = -1;
	PLAYER_STATUS *psPtr = (PLAYER_STATUS *)playerSbPtr->SBdataptr;

	// A campaign's level starts as single player's does, at the level's own start with no time of being safe.
	// Nobody comes back after dying in this mode, so this is only ever someone arriving.
	if (netGameData.coopCampaign != kCampaignNone)
	{
		psPtr->invulnerabilityTimer = 0;
		MultiplayerRestartSeed = 0;
		PlacePlayerWithNoMultiplayerStart(playerSbPtr);
		return;
	}

	if (MultiplayerRestartSeed)
	{
		StartOfGame_PlayerPlacement(playerSbPtr, MultiplayerRestartSeed);
		MultiplayerRestartSeed = 0;
		return;
	}

	// some basic checks
	if (playerSbPtr == NULL) {
		return;
	}

	if (playerSbPtr->DynPtr == NULL) {
		return;
	}

	// set the players invulnerability timer
	GLOBALASSERT(psPtr);
	psPtr->invulnerabilityTimer = netGameData.invulnerableTime * ONE_FIXED;

	// select the start positions for this character type
	switch (AvP.PlayerType)
	{
		case I_Marine :
			numStartPositions = numMarineStartPos;
			startPositions = marineStartPositions;
			break;

		case I_Predator :
			numStartPositions = numPredatorStartPos;
			startPositions = predatorStartPositions;
			break;

		case I_Alien :
			numStartPositions = numAlienStartPos;
			startPositions = alienStartPositions;
			break;
	}

	// None for this species: it used to do nothing here, which on coming back after dying left him at 0,0,0.
	if (!numStartPositions) {
		PlacePlayerWithNoMultiplayerStart(playerSbPtr);
		return;
	}

	for (start_index = FastRandom() % numStartPositions; numChecked < numStartPositions; start_index++, numChecked++)
	{
		int closestDistance = 0x7fffffff;
		int playerIndex;

		if (start_index >= numStartPositions) {
			start_index = 0;
		}

		// find the closest player to this start position
		for (playerIndex = 0; playerIndex < NET_MAXPLAYERS; playerIndex++)
		{
			if (netGameData.playerData[playerIndex].playerId &&
			    netGameData.playerData[playerIndex].playerId != AvPNetID &&
			    netGameData.playerData[playerIndex].playerAlive)
			{
				VECTORCH seperationVec;
				int distance;
				seperationVec = netGameData.playerData[playerIndex].lastKnownPosition;
				seperationVec.vx -= startPositions[start_index].location.vx;
				seperationVec.vy -= startPositions[start_index].location.vy;
				seperationVec.vz -= startPositions[start_index].location.vz;
				distance = Approximate3dMagnitude(&seperationVec);

				if (distance < closestDistance) {
					closestDistance = distance;
				}
			}
		}

		if (closestDistance > 20000)
		{
			// no player near this start position, so it will do
			bestIndex = start_index;
			break;
		}
		else if (closestDistance > bestDistance)
		{
			bestDistance = closestDistance;
			bestIndex = start_index;
		}
	}

	if (bestIndex != -1)
	{
		DYNAMICSBLOCK *dynPtr = playerSbPtr->DynPtr;
		// found a start postion
		Player->ObWorld = dynPtr->Position = dynPtr->PrevPosition = startPositions[bestIndex].location;
		dynPtr->OrientEuler = startPositions[bestIndex].orientation;
		CreateEulerMatrix(&dynPtr->OrientEuler, &dynPtr->OrientMat);
		TransposeMatrixCH(&dynPtr->OrientMat);
		playerSbPtr->containingModule = ModuleFromPosition(&Player->ObWorld, (MODULE *)0);
		playerPherModule = playerSbPtr->containingModule;
		/*
		After teleportation we need to update the visibilities , to ensure that
		dynamics will work properly this frame.
		*/
		{
			Global_VDB_Ptr->VDB_World = Player->ObWorld;
			AllNewModuleHandler();
			DoObjectVisibilities();
		}
	}
}
#endif

/* Works out everyone's starting positions . Use a shared random numer seed
   in order to avoid having several players appearing at the same place*/
void StartOfGame_PlayerPlacement(STRATEGYBLOCK *playerSbPtr, int seed)
{
	int numStartPositions;
	MULTIPLAYER_START *startPositions;
	PLAYER_STATUS *psPtr = (PLAYER_STATUS *)playerSbPtr->SBdataptr;
	VECTORCH ChosenPositions[NET_MAXPLAYERS];
	int NumChosen = 0;

	// set the players invulnerability timer
	GLOBALASSERT(psPtr);
	psPtr->invulnerabilityTimer = netGameData.invulnerableTime * ONE_FIXED;
	SetSeededFastRandom(seed);

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		int index;
		int numChecked = 0;

		if (!netGameData.playerData[i].playerId) {
			continue;
		}

		switch (netGameData.playerData[i].characterType)
		{
			case NGCT_Marine :
				numStartPositions = numMarineStartPos;
				startPositions = marineStartPositions;
				break;

			case NGCT_Alien :
				numStartPositions = numAlienStartPos;
				startPositions = alienStartPositions;
				break;

			case NGCT_Predator :
				numStartPositions = numPredatorStartPos;
				startPositions = predatorStartPositions;
				break;

			default :
				continue; // hmm
				break;
		}

		if (!numStartPositions) {
			if (netGameData.playerData[i].playerId == AvPNetID) {
				PlacePlayerWithNoMultiplayerStart(playerSbPtr);
				return;
			}

			continue;
		}

		for (index = SeededFastRandom() % numStartPositions; numChecked < numStartPositions; index++, numChecked++)
		{
			int j;

			if (index >= numStartPositions) {
				index = 0;
			}

			// see if anyone is at this position
			for (j = 0; j < NumChosen; j++)
			{
				if (ChosenPositions[j].vx == startPositions[index].location.vx &&
				    ChosenPositions[j].vy == startPositions[index].location.vy &&
				    ChosenPositions[j].vz == startPositions[index].location.vz)
				{
					break;
				}
			}

			if (j == NumChosen)
			{
				// found a clear start position
				break;
			}
		}

		if (index >= numStartPositions) {
			index = 0;
		}

		// we've either found a clear start position , or gone through the entire list
		// without finding one.Either way take the starting position index that we reached
		ChosenPositions[NumChosen] = startPositions[index].location;
		NumChosen++;

		if (netGameData.playerData[i].playerId == AvPNetID)
		{
			// this was our new start position
			DYNAMICSBLOCK *dynPtr = playerSbPtr->DynPtr;
			// found a start postion
			Player->ObWorld = dynPtr->Position = dynPtr->PrevPosition = startPositions[index].location;
			dynPtr->OrientEuler = startPositions[index].orientation;
			CreateEulerMatrix(&dynPtr->OrientEuler, &dynPtr->OrientMat);
			TransposeMatrixCH(&dynPtr->OrientMat);
			playerSbPtr->containingModule = ModuleFromPosition(&Player->ObWorld, (MODULE *)0);
			playerPherModule = playerSbPtr->containingModule;
			/*
			After teleportation we need to update the visibilities , to ensure that
			dynamics will work properly this frame.
			*/
			{
				Global_VDB_Ptr->VDB_World = Player->ObWorld;
				AllNewModuleHandler();
				DoObjectVisibilities();
			}
			// don't care about the positioning of anyone further on in the list
			return;
		}
	}
}

/* Patrick 4/8/97-------------------------------------
   For testing purposes...
   ---------------------------------------------------*/
#define logNetGameProcesses 0
#if logNetGameProcesses
static FILE *netLogfile;
#endif
void InitNetLog(void)
{
#if logNetGameProcesses
	netLogfile = avp_open_userfile("NETINFO.TXT", "w");
	fprintf(netLogfile, "NETGAME DEBUGGING LOG \n \n");
	fclose(netLogfile);
#endif
}

void LogNetInfo(char *_msg)
{
#if logNetGameProcesses

	if (!_msg) {
		return;
	}

	netLogfile = avp_open_userfile("NETINFO.TXT", "a");
	fprintf(netLogfile, _msg);
	fclose(netLogfile);
#endif
}

DISPLAYBLOCK PlayersMirrorImage;
STRATEGYBLOCK PlayersMirrorImageSB;
NETGHOSTDATABLOCK PlayersMirrorGhost;
DYNAMICSBLOCK PlayersMirrorDynBlock;

BOOL Current_Level_Requires_Mirror_Image()
{
	if ((!_stricmp(LevelName, "e3demo")) || (!_stricmp(LevelName, "e3demosp")) || (!_stricmp(LevelName, "derelict"))) {
		return TRUE;
	}

	return FALSE;
}

void CreatePlayersImageInMirror(void)
{
	AVP_BEHAVIOUR_TYPE type;
	STRATEGYBLOCK *sbPtr = &PlayersMirrorImageSB;
	NETGHOSTDATABLOCK *ghostData = &PlayersMirrorGhost;
	PlayersMirrorImage.ObStrategyBlock = sbPtr;
	sbPtr->SBdptr = &PlayersMirrorImage;
	sbPtr->SBdataptr = (void *)ghostData;
	sbPtr->DynPtr = &PlayersMirrorDynBlock;

	switch (AvP.PlayerType)
	{
		case (I_Marine):
		{
			type = I_BehaviourMarinePlayer;
			break;
		}

		case (I_Predator):
		{
			type = I_BehaviourPredatorPlayer;
			break;
		}

		case (I_Alien):
		{
			type = I_BehaviourAlienPlayer;
			break;
		}
		default:
		{
			assert(1 == 0);
			return;
			break;
		}
	}

	ghostData->type = type;
	ghostData->IOType = IOT_Non;
	ghostData->subtype = 0;
	ghostData->myGunFlash = NULL;
	ghostData->SoundHandle = SOUND_NOACTIVEINDEX;
	ghostData->currentAnimSequence = 0;
	ghostData->CloakingEffectiveness = 0;
	ghostData->IgnitionHandshaking = 0;
	ghostData->soundStartFlag = 0;

	if (AvP.Network == I_No_Network) {
		ghostData->playerId = 0;
	}
	else {
		ghostData->playerId = AvPNetID;
	}

	// set the shape
	switch (type)
	{
		case (I_BehaviourMarinePlayer):
		{
			extern void CreateMarineHModel(NETGHOSTDATABLOCK * ghostDataPtr, int weapon); // remove undefined warning
			CreateMarineHModel(ghostData, WEAPON_PULSERIFLE);
			break;
		}

		case (I_BehaviourAlienPlayer):
		{
			// extern void CreateAlienHModel(NETGHOSTDATABLOCK *ghostDataPtr,int alienType); // remove undefined warning
			CreateAlienHModel(ghostData, AT_Standard);
			break;
		}

		case (I_BehaviourPredatorPlayer):
		{
			extern void CreatePredatorHModel(NETGHOSTDATABLOCK * ghostDataPtr, int weapon); // remove undefined warning
			CreatePredatorHModel(ghostData, WEAPON_PRED_WRISTBLADE);
			break;
		}
	}

	sbPtr->SBdptr->HModelControlBlock = &ghostData->HModelController;
	ProveHModel(sbPtr->SBdptr->HModelControlBlock, sbPtr->SBdptr);
}

void DeallocatePlayersMirrorImage()
{
#if MIRRORING_ON

	if (Current_Level_Requires_Mirror_Image())
	{
		Dispel_HModel(&PlayersMirrorGhost.HModelController);
	}

#endif
}

void RenderPlayersImageInMirror(void)
{
	STRATEGYBLOCK *sbPtr = &PlayersMirrorImageSB;
	NETGHOSTDATABLOCK *ghostData = &PlayersMirrorGhost;
	int sequence;
	int weapon;
	int firingPrimary;
	int firingSecondary;

	switch (AvP.PlayerType)
	{
		case (I_Marine):
		{
			sequence = (unsigned char)GetMyMarineSequence();

			// check for change of charcter type
			if (ghostData->type != I_BehaviourMarinePlayer)
			{
				ghostData->type = I_BehaviourMarinePlayer;
				// settings currentweapon to -1 will forec the hmodel to be updated
				ghostData->CurrentWeapon = -1;
			}

			break;
		}

		case (I_Predator):
		{
			sequence = (unsigned char)GetMyPredatorSequence();

			// check for change of charcter type
			if (ghostData->type != I_BehaviourPredatorPlayer)
			{
				ghostData->type = I_BehaviourPredatorPlayer;
				// settings currentweapon to -1 will forec the hmodel to be updated
				ghostData->CurrentWeapon = -1;
			}

			break;
		}

		case (I_Alien):
		{
			sequence = (unsigned char)GetMyAlienSequence();

			// check for change of charcter type
			if (ghostData->type != I_BehaviourAlienPlayer)
			{
				ghostData->type = I_BehaviourAlienPlayer;
				// setting currentweapon to -1 will force the hmodel to be updated
				ghostData->CurrentWeapon = -1;
			}

			break;
		}

		default:
		{
			LOCALASSERT(1 == 0);
			break;
		}
	}

	// my current weapon id, and whether I am firing it...
	{
		PLAYER_WEAPON_DATA *weaponPtr;
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
		LOCALASSERT(playerStatusPtr);
		weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
		weapon = (signed char)(weaponPtr->WeaponIDNumber);

		if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_PRIMARY) && (playerStatusPtr->IsAlive)) {
			firingPrimary = 1;
		}
		else {
			firingPrimary = 0;
		}

		if ((weaponPtr->CurrentState == WEAPONSTATE_FIRING_SECONDARY) && (playerStatusPtr->IsAlive)) {
			firingSecondary = 1;
		}
		else {
			firingSecondary = 0;
		}
	}
	//      if(!(((!(messagePtr->IAmAlive)))&&(netGameData.playerData[playerIndex].characterType==NGCT_Alien)))
	{
		{
			PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
			HandleWeaponElevation(sbPtr, playerStatusPtr->ViewPanX, weapon);
			UpdateGhost(sbPtr, &(Player->ObStrategyBlock->DynPtr->Position), &(Player->ObStrategyBlock->DynPtr->OrientEuler), sequence, AreTwoPistolsInTertiaryFire());
			MaintainGhostCloakingStatus(sbPtr, (int)playerStatusPtr->cloakOn);
		}
	}
	{
		DISPLAYBLOCK *dPtr = &PlayersMirrorImage;
		dPtr->ObWorld = PlayersMirrorDynBlock.Position;
		dPtr->ObMat = PlayersMirrorDynBlock.OrientMat;
		ReflectObject(dPtr);
		PlayersMirrorImage.ObStrategyBlock = 0;
		AddShape(dPtr, Global_VDB_Ptr);
		PlayersMirrorImage.ObStrategyBlock = &PlayersMirrorImageSB;
	}
	HandleGhostGunFlashEffect(sbPtr, MyPlayerHasAMuzzleFlash(sbPtr));
}

/* KJL 15:32:17 24/05/98 - respawn all game objects that have been destroyed */
void RestartNetworkGame(int seed)
{
	if (netGameData.myGameState != NGS_Playing && netGameData.myGameState != NGS_EndGameScreen) {
		return;
	}

	// reset all the scores
	for (int i = 0; i < (NET_MAXPLAYERS); i++)
	{
		for (int j = 0; j < (NET_MAXPLAYERS); j++) {
			netGameData.playerData[i].playerFrags[j] = 0;
		}

		netGameData.playerData[i].playerScore = 0;
		netGameData.playerData[i].playerScoreAgainst = 0;
		netGameData.playerData[i].aliensKilled[0] = 0;
		netGameData.playerData[i].aliensKilled[1] = 0;
		netGameData.playerData[i].aliensKilled[2] = 0;
		netGameData.playerData[i].deathsFromAI = 0;
		netGameData.playerData[i].startFlag = 0;
		netGameData.playerData[i].playerAlive = 1;
		netGameData.playerData[i].playerHasLives = 1;
	}

	for (int j = 0; j < 3; j++) {
		netGameData.teamScores[j] = 0;
	}

	netGameData.stateCheckTimeDelay = 0;
	netGameData.GameTimeElapsed = 0;
	netGameData.LMS_AlienIndex = -1;
	netGameData.LMS_RestartTimer = 0;
	netGameData.numDeaths[0] = 0;
	netGameData.numDeaths[1] = 0;
	netGameData.numDeaths[2] = 0;
	netGameData.lastPointsBasedRespawn = 0;
	AvP.RestartLevel = 1;

	if (netGameData.gameType == NGT_LastManStanding)
	{
		// in a last man standing game , turn everyone in marines to start with
		ChangeToMarine();

		for (int i = 0; i < NET_MAXPLAYERS; i++) {
			netGameData.playerData[i].characterType = NGCT_Marine;
		}
	}

	TurnOffMultiplayerObserveMode();

	// The level starts again, so everything already reported of it has to be forgotten, as at a new level
	if (netGameData.coopCampaign != kCampaignNone)
	{
		LogString("campaign: the level is starting again");
		LevelEvents_LevelStartsAgain();
	}

	// go to a new start position
	// StartOfGame_PlayerPlacement(Player->ObStrategyBlock, seed);
	netGameData.myGameState = NGS_Playing;
	// record seed for later (used during restart level process)
	MultiplayerRestartSeed = seed;

	if (!MultiplayerRestartSeed) {
		MultiplayerRestartSeed = 1;
	}
}

// KJL 15:46:19 09/04/98 - processing info pertaining to multiplayer games
static void Inform_PlayerHasDied(NetID killer, NetID victim, NETGAME_CHARACTERTYPE killerType, char weaponIcon)
{
	int victimIndex = PlayerIdInPlayerList(victim);

	// KJL 15:35:38 09/04/98 - not knowing who the victim is what make things a bit awkward...
	if (victimIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	switch (killerType)
	{
		case NGCT_AI_Alien :
		{
			NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_KILLEDBY_ALIEN, netGameData.playerData[victimIndex].name, 0);
			break;
		}

		case NGCT_AI_Predalien :
		{
			NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_KILLEDBY_PREDALIEN, netGameData.playerData[victimIndex].name, 0);
			break;
		}

		case NGCT_AI_Praetorian :
		{
			NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_KILLEDBY_PRAETORIAN, netGameData.playerData[victimIndex].name, 0);
			break;
		}

		default :
		{
			/* KJL 15:36:03 09/04/98 - killer should be set to null if it's a suicide */
			/* killer==vitim means suicide now ,as well*/
			if (killer && killer != victim)
			{
				int killerIndex = PlayerIdInPlayerList(killer);

				if (killerIndex != NET_IDNOTINPLAYERLIST)
				{
					char weaponSymbol[5] = "";

					if (weaponIcon) {
						sprintf(weaponSymbol, " %c", weaponIcon);
					}

					NetworkGameConsoleMessageWithWeaponIcon(TEXTSTRING_MULTIPLAYERCONSOLE_KILLEDBY, netGameData.playerData[victimIndex].name, netGameData.playerData[killerIndex].name, weaponSymbol);
				}
			}
			// In the mode that plays like single player this usually means an NPC did it, and it mustn't say "suicide". Single player says nothing either.
			else if (!SameAsSinglePlayer()) {
				NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_SUICIDE, netGameData.playerData[victimIndex].name, 0);
			}

			break;
		}
	}
}

static void Inform_AiHasDied(NetID killer, ALIEN_TYPE type, char weaponIcon)
{
	int killerIndex = PlayerIdInPlayerList(killer);

	if (killerIndex != NET_IDNOTINPLAYERLIST)
	{
		char weaponSymbol[5] = "";

		if (weaponIcon) {
			sprintf(weaponSymbol, " %c", weaponIcon);
		}

		switch (type)
		{
			case AT_Standard :
			{
				NetworkGameConsoleMessageWithWeaponIcon(TEXTSTRING_MULTIPLAYERCONSOLE_ALIEN_KILLED, netGameData.playerData[killerIndex].name, 0, weaponSymbol);
				break;
			}

			case AT_Predalien :
			{
				NetworkGameConsoleMessageWithWeaponIcon(TEXTSTRING_MULTIPLAYERCONSOLE_PREDALIEN_KILLED, netGameData.playerData[killerIndex].name, 0, weaponSymbol);
				break;
			}

			case AT_Praetorian :
			{
				NetworkGameConsoleMessageWithWeaponIcon(TEXTSTRING_MULTIPLAYERCONSOLE_PRAETORIAN_KILLED, netGameData.playerData[killerIndex].name, 0, weaponSymbol);
				break;
			}
		}
	}
}

static void Inform_PlayerHasLeft(NetID player)
{
	int playerIndex = PlayerIdInPlayerList(player);

	// KJL 15:35:38 09/04/98 - not knowing who the player is what make things a bit awkward...
	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_LEAVEGAME, netGameData.playerData[playerIndex].name, 0);
}

static void Inform_PlayerHasJoined(NetID player)
{
	int playerIndex = PlayerIdInPlayerList(player);

	// KJL 15:35:38 09/04/98 - not knowing who the player is what make things a bit awkward...
	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_JOINGAME, netGameData.playerData[playerIndex].name, 0);
}

static void Inform_PlayerHasConnected(NetID player)
{
	int playerIndex = PlayerIdInPlayerList(player);

	// KJL 15:35:38 09/04/98 - not knowing who the player is what make things a bit awkward...
	if (playerIndex == NET_IDNOTINPLAYERLIST) {
		return;
	}

	NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_CONNECTGAME, netGameData.playerData[playerIndex].name, 0);
}

static void WriteFragmentStatus(int fragmentNumber, int status)
{
	int n = fragmentNumber >> 3;
	int r = fragmentNumber - (n << 3);
	unsigned char *ptr = &FragmentalObjectStatus[n];
	unsigned char mask = 1 << r;

	if (status) {
		*ptr |= mask;
	}
	else {
		*ptr &= ~mask;
	}
}

static int ReadFragmentStatus(int fragmentNumber)
{
	int n = fragmentNumber >> 3;
	int r = fragmentNumber - (n << 3);
	unsigned char *ptr = &FragmentalObjectStatus[n];
	unsigned char mask = 1 << r;
	return((*ptr)&mask);
}

static void WriteStrategySynch(int objectNumber, int status)
{
	int n = objectNumber >> 2;
	int shift = (objectNumber - (n << 2)) * 2;
	unsigned char *ptr = &StrategySynchArray[n];
	unsigned char mask = 3 << shift;
	*ptr &= ~ mask;
	*ptr |= (status & 3) << shift;
}

static int ReadStrategySynch(int objectNumber)
{
	int n = objectNumber >> 2;
	int shift = (objectNumber - (n << 2)) * 2;
	unsigned char *ptr = &StrategySynchArray[n];
	unsigned char mask = 3 << shift;
	return(((*ptr)&mask) >> shift);
}

#if 1
static int GetDynamicScoreMultiplier(int playerKilledIndex, int killerIndex)
{
	int scoreFor;
	int scoreAgainst;
	int mult;
	int playerCount = 0;
	GLOBALASSERT(playerKilledIndex != killerIndex);
	scoreFor = netGameData.playerData[playerKilledIndex].playerScore;
	scoreAgainst = netGameData.playerData[playerKilledIndex].playerScoreAgainst;
	scoreFor = max(500, scoreFor + 500);
	scoreAgainst = max(500, scoreAgainst + 500);

	//count players
	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == 0) {
			continue;
		}
		playerCount++;
	}

	// only bother if there are at least 3 players
	if (playerCount < 3) {
		return ONE_FIXED;
	}

	// value of player depends on comparing player's score with the number of points scored against that player
	if (scoreFor > scoreAgainst)
	{
		int ratio = DIV_FIXED(scoreFor, scoreAgainst);
		mult = DIV_FIXED(10 * ratio, 9 * ONE_FIXED + ratio);

		if (mult < ONE_FIXED) {
			mult = ONE_FIXED;
		}
	}
	else
	{
		int ratio = DIV_FIXED(scoreAgainst, scoreFor);
		mult = DIV_FIXED(9 * ONE_FIXED + ratio, 10 * ratio);

		if (mult > ONE_FIXED) {
			mult = ONE_FIXED;
		}
	}

	return mult;
}
#else
static int GetDynamicScoreMultiplier(int playerKilledIndex, int killerIndex)
{
	int i;
	int scoreTotal = 0;
	int playerCount = 0;
	int playerKilledScore;
	int mult;
	GLOBALASSERT(playerKilledIndex != killerIndex);

	//add up score of all players
	for (i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == NULL)
		{
			continue;
		}

		playerCount++;
		//give everyone a minimum score of 500 for the purpose of this calculation
		scoreTotal += max(500, netGameData.playerData[i].playerScore + 500);
	}

	if (playerCount < 3 || !scoreTotal)
	{
		return ONE_FIXED;
	}

	playerKilledScore = max(500, netGameData.playerData[playerKilledIndex].playerScore + 500);
	//get average score of all players other than killed player
	playerCount--;
	scoreTotal = (scoreTotal - playerKilledScore) / playerCount;

	if (playerKilledScore > scoreTotal)
	{
		int ratio = DIV_FIXED(playerKilledScore, scoreTotal);
		mult = DIV_FIXED(10 * ratio, 10 * ONE_FIXED + ratio);

		if (mult < ONE_FIXED)
		{
			mult = ONE_FIXED;
		}
	}
	else
	{
		int ratio = DIV_FIXED(scoreTotal, playerKilledScore);
		mult = DIV_FIXED(10 * ONE_FIXED + ratio, 10 * ratio);

		if (mult > ONE_FIXED)
		{
			mult = ONE_FIXED;
		}
	}

	return mult;
}
#endif

static int GetNetScoreForKill(int playerKilledIndex, int killerIndex)
{
	NETGAME_CHARACTERTYPE killerType = netGameData.playerData[killerIndex].characterType;
	NETGAME_CHARACTERTYPE playerKilledType = netGameData.playerData[playerKilledIndex].characterType;
	int score = netGameData.baseKillValue;

	if (playerKilledIndex == killerIndex) {
		// suicide
		return -netGameData.baseKillValue;
	}

	if (netGameData.gameType == NGT_PredatorTag)
	{
		// only the predator can score
		if (killerType != NGCT_Predator) {
			return 0;
		}
	}
	else if (netGameData.gameType == NGT_AlienTag)
	{
		// only the alien can score
		if (killerType != NGCT_Alien) {
			return 0;
		}
	}
	else if (netGameData.gameType == NGT_CoopDeathmatch)
	{
		// have we killed someone on the same team
		if (killerType == playerKilledType) {
			return -netGameData.baseKillValue;
		}
	}

	if (netGameData.useCharacterKillValues) {
		score = netGameData.characterKillValues[playerKilledType];
	}

	if (netGameData.useDynamicScoring && score > 0)
	{
		int dynamicScoreMult = GetDynamicScoreMultiplier(playerKilledIndex, killerIndex);
		score = MUL_FIXED(score, dynamicScoreMult);

		// make sure player gets at least one point
		if (score < 1) {
			score = 1;
		}
	}

	return score;
}

static void CheckLastManStandingState()
{
	// make sure that ther is at least 1 alien and 1 non-alien
	int alienCount = 0;
	int nonAlienCount = 0;
	int alienIndex;

	if (netGameData.stateCheckTimeDelay > 0)
	{
		// still too soon after last restart to check again
		netGameData.stateCheckTimeDelay -= RealFrameTime;
		return;
	}

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId)
		{
			if (netGameData.playerData[i].characterType == NGCT_Alien) {
				alienCount++;
			}
			else {
				nonAlienCount++;
			}
		}
	}

	// if there is only one player forget it
	if ((alienCount + nonAlienCount) < 2) {
		return;
	}

	if (alienCount && nonAlienCount)
	{
		// the game is still going
		// make sure the restarttimer is 0
		netGameData.LMS_RestartTimer = 0;
		return;
	}

	// find out who the next alien will be
	for (alienIndex = netGameData.LMS_AlienIndex + 1;; alienIndex++)
	{
		if (alienIndex >= NET_MAXPLAYERS)
		{
			// everyone has had a turn as an alien
			TransmitEndOfGameNetMsg();
			netGameData.myGameState = NGS_EndGameScreen;
			return;
		}

		if (netGameData.playerData[alienIndex].playerId)
		{
			// this player will be our new alien
			break;
		}
	}

	// game needs to be restarted.
	// has the countdown started?
	if (netGameData.LMS_RestartTimer > 0)
	{
		int secondsBefore, secondsAfter;
		secondsBefore = (netGameData.LMS_RestartTimer + ONE_FIXED - 1) / ONE_FIXED;
		netGameData.LMS_RestartTimer -= RealFrameTime;
		secondsAfter = (netGameData.LMS_RestartTimer + ONE_FIXED - 1) / ONE_FIXED;

		// has the timer reached 0?
		if (netGameData.LMS_RestartTimer < 0)
		{
			// get a random number seed for starting position
			int seed = FastRandom();
			netGameData.LMS_RestartTimer = 0;
			netGameData.LMS_AlienIndex = alienIndex;
			AddNetMsg_LastManStanding_Restart(netGameData.playerData[alienIndex].playerId, seed);
			Handle_LastManStanding_Restart(netGameData.playerData[alienIndex].playerId, seed);
			// set time delay until we next check for need to restart
			// this gives time for the other players to get their messages
			netGameData.stateCheckTimeDelay = 5 * ONE_FIXED;
		}
		else
		{
			// if the timer has gone down another second , tell everyone
			if (secondsAfter < secondsBefore)
			{
				AddNetMsg_LastManStanding_RestartTimer((char)secondsAfter);
				Handle_LastManStanding_RestartTimer((char)secondsAfter);
			}
		}
	}
	else
	{
		static int ReminderTimer = 0;

		if (netGameData.LMS_RestartTimer == -1)
		{
			ReminderTimer -= RealFrameTime;

			if (ReminderTimer < 0)
			{
				// give a reminder (forced by setting reset timer to 0)
				ReminderTimer = 0;
				netGameData.LMS_RestartTimer = 0;
			}
		}

		if (netGameData.LMS_RestartTimer != -1)
		{
			// tell host to press operate
			PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_LMS_ALLMARINESDEAD);
			PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_LMS_HOSTPRESSOPERATE);
			AddNetMsg_LastManStanding_RestartTimer(255);
			netGameData.LMS_RestartTimer = -1;
			ReminderTimer = 6 * ONE_FIXED;
		}

		// wait for host to press operate
		{
			PLAYER_STATUS *psPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);

			if (psPtr->Mvt_InputRequests.Flags.Rqst_Operate)
			{
				// say who the next alien will be
				AddNetMsg_PlayerID(netGameData.playerData[alienIndex].playerId, NetMT_LastManStanding_RestartInfo);
				Handle_LastManStanding_RestartInfo(netGameData.playerData[alienIndex].playerId);
				// start the restart timer
				netGameData.LMS_RestartTimer = 4 * ONE_FIXED;
			}
		}
	}
}

static void Handle_LastManStanding_Restart(NetID alienID, int seed)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing){
		return;
	}

	if (AvPNetID == alienID)
	{
		// become an alien
		ChangeToAlien();
	}
	else
	{
		// become a marine
		ChangeToMarine();
	}

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == alienID) {
			netGameData.playerData[i].characterType = NGCT_Alien;
		}
		else {
			netGameData.playerData[i].characterType = NGCT_Marine;
		}
	}

	/*
	// go to an new start position
	StartOfGame_PlayerPlacement(Player->ObStrategyBlock,seed);

	// restore all the destroyed objects.
	RespawnAllObjects();
	*/
	MultiplayerRestartSeed = seed;
	AvP.RestartLevel = 1;
	PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_LMS_GO);
}

static void Handle_LastManStanding_RestartInfo(NetID alienID)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	// find the alien's name
	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == alienID)
		{
			NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYER_LMS_WILLBEALIEN, netGameData.playerData[i].name, 0);
		}
	}
}

static void Handle_LastManStanding_LastMan(NetID marineID)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing)
	{
		return;
	}

	// find the marine's name
	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == marineID) {
			NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYER_LMS_LASTMANSTANDING, netGameData.playerData[i].name, 0);
		}
	}
}

static void Handle_LastManStanding_RestartTimer(unsigned char time)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if ((int)time == 255)
	{
		// tell player to wait
		PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_LMS_ALLMARINESDEAD);
		PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_LMS_WAITFORHOST);
	}
	else
	{
		// show countdown
		sprintf(OnScreenMessageBuffer, "%u...", time);
		NewOnScreenMessage(OnScreenMessageBuffer);
	}
}

static int CountPlayersOfType(NETGAME_CHARACTERTYPE species)
{
	int numPredators = 0;

	for (int i = 0; i < (NET_MAXPLAYERS); i++)
	{
		if (netGameData.playerData[i].playerId)
		{
			if (netGameData.playerData[i].characterType == species) {
				numPredators++;
			}
		}
	}

	return numPredators;
}

static void CheckSpeciesTagState()
{
	NETGAME_CHARACTERTYPE tagSpecies = NGCT_Predator;

	if (netGameData.gameType == NGT_AlienTag) {
		tagSpecies = NGCT_Alien;
	}

	//make sure that there is at least one predator in the game
	if (netGameData.stateCheckTimeDelay > 0)
	{
		netGameData.stateCheckTimeDelay -= RealFrameTime;
		return;
	}

	if (CountPlayersOfType(tagSpecies)) {
		return;
	}

	// we need to choose a predator player
	// make it the person with the lowest score
	{
		NetID predID = 0;
		int lowScore = 1000000000;

		for (int i = 0; i < (NET_MAXPLAYERS); i++)
		{
			if (netGameData.playerData[i].playerId)
			{
				if (netGameData.playerData[i].playerScore < lowScore)
				{
					lowScore = netGameData.playerData[i].playerScore;
					predID = netGameData.playerData[i].playerId;
				}
			}
		}

		AddNetMsg_PlayerID(predID, NetMT_PredatorTag_NewPredator);
		Handle_SpeciesTag_NewPersonIt(predID);
	}
}


static void Handle_SpeciesTag_NewPersonIt(NetID predatorID)
{
	// if we're not playing, ignore it
	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	if (AvPNetID == predatorID)
	{
		// become aa predator or alien
		if (netGameData.gameType == NGT_PredatorTag) {
			ChangeToPredator();
		}
		else if (netGameData.gameType == NGT_AlienTag) {
			ChangeToAlien();
		}
	}

	netGameData.stateCheckTimeDelay = 5 * ONE_FIXED;
}

void ChangeNetGameType_Individual()
{
	// A campaign is a co-op game all the way through, as single player has no other kind
	if (SameAsSinglePlayer()) {
		return;
	}

	netGameData.gameType = NGT_Individual;
}

void ChangeNetGameType_Coop()
{
	// A campaign is a co-op game all the way through, as single player has no other kind
	if (SameAsSinglePlayer()) {
		return;
	}

	netGameData.gameType = NGT_CoopDeathmatch;
}

void ChangeNetGameType_LastManStanding()
{
	// A campaign is a co-op game all the way through, as single player has no other kind
	if (SameAsSinglePlayer()) {
		return;
	}

	netGameData.gameType = NGT_LastManStanding;
	netGameData.LMS_AlienIndex = 0;
	netGameData.stateCheckTimeDelay = 0;
}

void ChangeNetGameType_PredatorTag()
{
	// A campaign is a co-op game all the way through, as single player has no other kind
	if (SameAsSinglePlayer()) {
		return;
	}

	netGameData.gameType = NGT_PredatorTag;
}


/*
    This function takes a string from the list of language localised strings, and replaces
    instances of %1 and %2 with the appropriate names.
    It then displays the message in the console.
*/
static void NetworkGameConsoleMessage(enum TEXTSTRING_ID stringID, const char *name1, const char *name2)
{
	// This is the same job as the weapon-icon version below, without a weapon
	// icon, so it hands the work over to that one.
	NetworkGameConsoleMessageWithWeaponIcon(stringID, name1, name2, 0);
}

static void NetworkGameConsoleMessageWithWeaponIcon(enum TEXTSTRING_ID stringID, const char *name1, const char *name2, const char *weaponSymbol)
{
	// We build the message in OnScreenMessageBuffer, which holds 300 bytes:
	// up to 299 characters (letters, digits, spaces and so on), then a zero
	// byte that marks where the text stops (the "end marker").
	// The message pattern, such as "%1 was killed by %2", comes from the
	// game's language text file on disk. Without the limits below, a pattern
	// line made very long in that file would make this code write past the
	// end of the buffer and damage other memory. Every copy below stops at
	// the 299th character instead, so a message that is too long is cut
	// short. A message that fits comes out exactly as before.
	char *messageptr = &OnScreenMessageBuffer[0];
	char * const messageend = &OnScreenMessageBuffer[sizeof(OnScreenMessageBuffer) - 1]; // the last byte is kept for the end marker
	const char *string = GetTextString(stringID);

	if (!string) {
		return;
	}

	while (*string && messageptr < messageend)
	{
		// "%1" means "put the first name here" and "%2" the second one.
		// "%3" to "%9" have no name to put in, so they are left out, as before.
		if (string[0] == '%' && string[1] >= '1' && string[1] <= '9')
		{
			const char *name = (string[1] == '1') ? name1 : ((string[1] == '2') ? name2 : 0);

			if (name)
			{
				// Every caller passes a player's name from netGameData, which is
				// kept in a 13-byte box (NET_PLAYERNAMELENGTH). We read no more
				// than that box, and copy no more than still fits in the buffer.
				size_t const nameBox = NET_PLAYERNAMELENGTH;
				size_t const room = messageend - messageptr;
				size_t const nameLength = strnlen(name, (room < nameBox) ? room : nameBox);
				memcpy(messageptr, name, nameLength);
				messageptr += nameLength;
			}

			string += 2;
			continue;
		}

		*messageptr++ = *string++;
	}

	// A kill message can end with the weapon that was used: a space and one
	// special character that the console font draws as a small picture of
	// the weapon. It is added only as far as it still fits.
	if (weaponSymbol)
	{
		size_t const symbolLength = strnlen(weaponSymbol, messageend - messageptr);
		memcpy(messageptr, weaponSymbol, symbolLength);
		messageptr += symbolLength;
	}

	*messageptr = 0;
	NewOnScreenMessage(OnScreenMessageBuffer);
}

static_assert(NGSCT_Pistols == kClassPistols && NGSCT_Shotgun == kClassShotgun && NGSCT_Revolver == kClassRevolver && NGSCT_M79 == kClassM79, "marine_classes.h has to number the classes as NGSCT_ does");

static int CharacterTypesAvailable[NUM_PC_TYPES];
static int CharacterSubTypesAvailable[NUM_PC_SUBTYPES];

// In a campaign everyone plays the species its levels were written for, because a level's objects, and
// who fights whom in it, go by what the player is. A marine is a plain marine, since the kit is single
// player's and not a class's. Only these two lists of what's on offer change, not the host's limits, so
// that those are still as he left them if he goes on to choose another kind of game.
static void CampaignFixesWhoMayBeWhat(void)
{
	if (netGameData.coopCampaign == kCampaignNone)
	{
		return;
	}

	CharacterTypesAvailable[NGCT_Marine] = 0;
	CharacterTypesAvailable[NGCT_Predator] = 0;
	CharacterTypesAvailable[NGCT_Alien] = 0;

	for (int i = 0; i < NUM_PC_SUBTYPES; i++)
	{
		CharacterSubTypesAvailable[i] = 0;
	}

	switch (netGameData.coopCampaign)
	{
		case kCampaignMarine:
			CharacterTypesAvailable[NGCT_Marine] = NET_MAXPLAYERS;
			CharacterSubTypesAvailable[NGSCT_General] = NET_MAXPLAYERS;
			break;
		case kCampaignPredator:
			CharacterTypesAvailable[NGCT_Predator] = NET_MAXPLAYERS;
			break;
		case kCampaignAlien:
			CharacterTypesAvailable[NGCT_Alien] = NET_MAXPLAYERS;
			break;
		case kCampaignCustom:
			CharacterTypesAvailable[Coop_CustomMapCharacter()] = NET_MAXPLAYERS;
			CharacterSubTypesAvailable[NGSCT_General] = NET_MAXPLAYERS;
			break;
	}
}

int DetermineAvailableCharacterTypes(bool ConsiderUsedCharacters)
{
	int i;
	int maxMarines = 0;

	// set limits for disallowed marine types to zero
	if (!netGameData.allowSmartgun) {
		netGameData.maxMarineSmartgun = 0;
	}

	if (!netGameData.allowFlamer) {
		netGameData.maxMarineFlamer = 0;
	}

	if (!netGameData.allowMinigun) {
		netGameData.maxMarineMinigun = 0;
	}

	if (!netGameData.allowSadar) {
		netGameData.maxMarineSadar = 0;
	}

	if (!netGameData.allowGrenadeLauncher) {
		netGameData.maxMarineGrenade = 0;
	}

	if (!netGameData.allowSmartDisc) {
		netGameData.maxMarineSmartDisc = 0;
	}

	if (!netGameData.allowPistols) {
		netGameData.maxMarinePistols = 0;
	}

	if (!netGameData.allowShotgun) {
		netGameData.maxMarineShotgun = 0;
	}

	if (!netGameData.allowRevolver) {
		netGameData.maxMarineRevolver = 0;
	}

	if (!netGameData.allowM79) {
		netGameData.maxMarineM79 = 0;
	}

	if (netGameData.gameType == NGT_PredatorTag)
	{
		// force limit of one predator
		netGameData.maxPredator = 1;
	}
	else if (netGameData.gameType == NGT_AlienTag)
	{
		// force limit of one alien
		netGameData.maxAlien = 1;
	}
	else if (netGameData.gameType == NGT_LastManStanding)
	{
		// no predators are allowed
		netGameData.maxPredator = 0;
		// the host will be the first alien (and so is the only person that can select alien on the starting screen)
		netGameData.maxAlien = 1;
	}
	else if (netGameData.gameType == NGT_Coop)
	{
		// no pc aliens allowed in coop games
		netGameData.maxAlien = 0;
	}

	if (netGameData.skirmishMode)
	{
		// Skirmish mode - player can be anything except an alien
		netGameData.maxAlien = 0;
		netGameData.maxPredator = 8;
		netGameData.maxMarine = 8;
		netGameData.maxMarineGeneral = 8;
		netGameData.maxMarinePulseRifle = 8;
		netGameData.maxMarineSmartgun = 8;
		netGameData.maxMarineFlamer = 8;
		netGameData.maxMarineSadar = 8;
		netGameData.maxMarineGrenade = 8;
		netGameData.maxMarineMinigun = 8;
		netGameData.maxMarineSmartDisc = 8;
		netGameData.maxMarinePistols = 8;
		netGameData.maxMarineShotgun = 8;
		netGameData.maxMarineRevolver = 8;
		netGameData.maxMarineM79 = 8;
	}

	CharacterTypesAvailable[NGCT_Marine] = netGameData.maxMarine;
	CharacterTypesAvailable[NGCT_Alien] = netGameData.maxAlien;
	CharacterTypesAvailable[NGCT_Predator] = netGameData.maxPredator;
	CharacterSubTypesAvailable[NGSCT_General] = netGameData.maxMarineGeneral;
	CharacterSubTypesAvailable[NGSCT_PulseRifle] = netGameData.maxMarinePulseRifle;
	CharacterSubTypesAvailable[NGSCT_Smartgun] = netGameData.maxMarineSmartgun;
	CharacterSubTypesAvailable[NGSCT_Flamer] = netGameData.maxMarineFlamer;
	CharacterSubTypesAvailable[NGSCT_Sadar] = netGameData.maxMarineSadar;
	CharacterSubTypesAvailable[NGSCT_GrenadeLauncher] = netGameData.maxMarineGrenade;
	CharacterSubTypesAvailable[NGSCT_Minigun] = netGameData.maxMarineMinigun;
	CharacterSubTypesAvailable[NGSCT_Frisbee] = netGameData.maxMarineSmartDisc;
	CharacterSubTypesAvailable[NGSCT_Pistols] = netGameData.maxMarinePistols;
	CharacterSubTypesAvailable[NGSCT_Shotgun] = netGameData.maxMarineShotgun;
	CharacterSubTypesAvailable[NGSCT_Revolver] = netGameData.maxMarineRevolver;
	CharacterSubTypesAvailable[NGSCT_M79] = netGameData.maxMarineM79;
	CampaignFixesWhoMayBeWhat();

	// go through all the other players to see which character types are being used
	if (ConsiderUsedCharacters)
	{
		if (AvP.Network != I_No_Network)
		{
			// (it will still be I_No_Network while the host is setting things up)
			for (i = 0; i < NET_MAXPLAYERS; i++)
			{
				if (netGameData.playerData[i].playerId && netGameData.playerData[i].playerId != AvPNetID)
				{
					switch (netGameData.playerData[i].characterType)
					{
						case NGCT_Marine :
							CharacterTypesAvailable[NGCT_Marine]--;

							switch (netGameData.playerData[i].characterSubType)
							{
								case NGSCT_General :
									CharacterSubTypesAvailable[NGSCT_General]--;
									break;

								case NGSCT_PulseRifle :
									CharacterSubTypesAvailable[NGSCT_PulseRifle]--;
									break;

								case NGSCT_Smartgun :
									CharacterSubTypesAvailable[NGSCT_Smartgun]--;
									break;

								case NGSCT_Flamer :
									CharacterSubTypesAvailable[NGSCT_Flamer]--;
									break;

								case NGSCT_Sadar :
									CharacterSubTypesAvailable[NGSCT_Sadar]--;
									break;

								case NGSCT_GrenadeLauncher :
									CharacterSubTypesAvailable[NGSCT_GrenadeLauncher]--;
									break;

								case NGSCT_Minigun :
									CharacterSubTypesAvailable[NGSCT_Minigun]--;
									break;

								case NGSCT_Frisbee :
									CharacterSubTypesAvailable[NGSCT_Frisbee]--;
									break;

								case NGSCT_Pistols :
									CharacterSubTypesAvailable[NGSCT_Pistols]--;
									break;

								case NGSCT_Shotgun :
									CharacterSubTypesAvailable[NGSCT_Shotgun]--;
									break;

								case NGSCT_Revolver :
									CharacterSubTypesAvailable[NGSCT_Revolver]--;
									break;

								case NGSCT_M79 :
									CharacterSubTypesAvailable[NGSCT_M79]--;
									break;
							}

							break;

						case NGCT_Predator :
							CharacterTypesAvailable[NGCT_Predator]--;
							break;

						case NGCT_Alien :
							CharacterTypesAvailable[NGCT_Alien]--;
							break;

						default:
							break;
					}
				}
			}
		}
	}

	// make sure all the limits are at least 0
	for (i = 0; i < NUM_PC_TYPES; i++) {
		CharacterTypesAvailable[i] = max(0, CharacterTypesAvailable[i]);
	}

	for (i = 0; i < NUM_PC_SUBTYPES; i++) {
		CharacterSubTypesAvailable[i] = max(0, CharacterSubTypesAvailable[i]);
	}

	// adjust the marine limits to take into account that there must be both sufficent marine slots ,
	// and also sufficient subtype slots
	maxMarines = 0;

	for (i = 0; i < NUM_PC_SUBTYPES; i++)
	{
		CharacterSubTypesAvailable[i] = min(CharacterSubTypesAvailable[i], CharacterTypesAvailable[NGCT_Marine]);
		maxMarines += CharacterSubTypesAvailable[i];
	}

	CharacterTypesAvailable[NGCT_Marine] = min(CharacterTypesAvailable[NGCT_Marine], maxMarines);
	// return the total number of players available
	return CharacterTypesAvailable[NGCT_Marine] +
	       CharacterTypesAvailable[NGCT_Alien] +
	       CharacterTypesAvailable[NGCT_Predator];
}

void GetNextAllowedSpecies(int *species, BOOL search_forwards)
{
	int count = 0;

	if (AvP.Network == I_No_Network)
	{
		if (netGameData.gameType == NGT_PredatorTag)
		{
			// this computer is the host setting up a predator tag game
			// therefore must be a predator
			*species = 1;
			return;
		}
		else if (netGameData.gameType == NGT_AlienTag || netGameData.gameType == NGT_LastManStanding)
		{
			// this computer is the host setting up an alien tag or last man standing game
			// therefore must be a alien
			*species = 2;
			return;
		}
	}

	DetermineAvailableCharacterTypes(true);

	do
	{
		switch (*species)
		{
			case 0: // marine (general)
				if (CharacterSubTypesAvailable[NGSCT_General] > 0) {
					return;
				}

				break;

			case 1: // predator
				if (CharacterTypesAvailable[NGCT_Predator] > 0) {
					return;
				}

				break;

			case 2: // alien
				if (CharacterTypesAvailable[NGCT_Alien] > 0) {
					return;
				}

				break;

			case 3: // marine (pulse rifle)
				if (CharacterSubTypesAvailable[NGSCT_PulseRifle]) {
					return;
				}

				break;

			case 4: // marine (smartgun)
				if (CharacterSubTypesAvailable[NGSCT_Smartgun]) {
					return;
				}

				break;

			case 5: // marine (flamer)
				if (CharacterSubTypesAvailable[NGSCT_Flamer]) {
					return;
				}

				break;

			case 6: // marine (sadar)
				if (CharacterSubTypesAvailable[NGSCT_Sadar]) {
					return;
				}

				break;

			case 7: // marine (grenade)
				if (CharacterSubTypesAvailable[NGSCT_GrenadeLauncher]) {
					return;
				}

				break;

			case 8: // marine (minigun)
				if (CharacterSubTypesAvailable[NGSCT_Minigun]) {
					return;
				}

				break;

			case 9: // marine (frisbee)
				if (CharacterSubTypesAvailable[NGSCT_Frisbee]) {
					return;
				}

				break;

			case 10: // marine (pistols)
				if (CharacterSubTypesAvailable[NGSCT_Pistols]) {
					return;
				}

				break;

			case 11: // marine (shotgun)
				if (CharacterSubTypesAvailable[NGSCT_Shotgun]) {
					return;
				}

				break;

			case 12: // marine (revolver)
				if (CharacterSubTypesAvailable[NGSCT_Revolver]) {
					return;
				}

				break;

			case 13: // marine (M79)
				if (CharacterSubTypesAvailable[NGSCT_M79]) {
					return;
				}

				break;
		}

		if (search_forwards)
		{
			(*species)++;

			if (*species > kClassSliderLast) {
				*species = 0;
			}
		}
		else
		{
			(*species)--;

			if (*species < 0) {
				*species = kClassSliderLast;
			}
		}

		count++;
	}
	while (count <= kClassSliderLast);

	// oh dear no allowable species
	*species = 0;
}

void SpeciesTag_DetermineMyNextCharacterType()
{
	NETGAME_CHARACTERTYPE tagSpecies;
	NETGAME_CHARACTERTYPE otherSpecies;

	if (netGameData.gameType == NGT_PredatorTag)
	{
		if (AvP.PlayerType != I_Predator) {
			return;
		}

		tagSpecies = NGCT_Predator;
		otherSpecies = NGCT_Alien;
	}
	else
	{
		if (AvP.PlayerType != I_Alien) {
			return;
		}

		tagSpecies = NGCT_Alien;
		otherSpecies = NGCT_Predator;
	}

	if (myNetworkKillerId && myNetworkKillerId != AvPNetID && CountPlayersOfType(tagSpecies) == 1)
	{
		//become the character that killed me
		int killer_index = PlayerIdInPlayerList(myNetworkKillerId);

		if (killer_index != NET_IDNOTINPLAYERLIST)
		{
			netGameData.myNextCharacterType = netGameData.playerData[killer_index].characterType;
			netGameData.myCharacterSubType = netGameData.playerData[killer_index].characterSubType;
		}
		else
		{
			//the player doing the damage has either left the game , or never existed.
			//call it suicide then.
			myNetworkKillerId = 0;
		}
	}

	if (!(myNetworkKillerId && myNetworkKillerId != AvPNetID && CountPlayersOfType(tagSpecies) == 1))
	{
		int total = 0;
		//either suicide , or we have too many predators for some reason
		//pick marine/alien at random
		//first determine available character types
		DetermineAvailableCharacterTypes(true);
		total = CharacterTypesAvailable[NGCT_Marine] +
		        CharacterTypesAvailable[otherSpecies];

		if (total == 0)
		{
			//hmm , no available character types
			//in that case look at the character types that are allowed in the first place
			DetermineAvailableCharacterTypes(false);
			total = CharacterTypesAvailable[NGCT_Marine] +
			        CharacterTypesAvailable[otherSpecies];
		}

		if (total > 0)
		{
			int dieroll = (FastRandom() % total);

			if (dieroll < CharacterTypesAvailable[NGCT_Marine])
			{
				//become a marine
				netGameData.myNextCharacterType = NGCT_Marine;
				//but what type of marine ?
				total = 0;

				for (int i = 0; i < NUM_PC_SUBTYPES; i++) {
					total += CharacterSubTypesAvailable[i];
				}

				//since there were available marine slots , there must be at least some
				//marine subtype slots
				GLOBALASSERT(total > 0);
				dieroll = (FastRandom() % total);

				for (int i = 0; i < NUM_PC_SUBTYPES; i++)
				{
					dieroll -= CharacterSubTypesAvailable[i];

					if (dieroll < 0)
					{
						netGameData.myCharacterSubType = static_cast<enum netgame_specialistcharactertype>(i);
						return;
					}
				}

				GLOBALASSERT(0 == "Shouldn't be possible to reach this line");
			}
			else
			{
				//become an alien
				netGameData.myNextCharacterType = otherSpecies;
			}
		}
		else
		{
			//no marines or aliens allowed?
			//tough, the player can become a standard marine
			netGameData.myNextCharacterType = NGCT_Marine;
			netGameData.myCharacterSubType = NGSCT_General;
		}
	}
}


/*----------------------------------------------------------------**
** Identify the network player closest to the centre of the screen **
**----------------------------------------------------------------*/
void ShowNearestPlayersName()
{
	extern int NumOnScreenBlocks;
	extern DISPLAYBLOCK *OnScreenBlockList[];
	extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
	int numberOfObjects = NumOnScreenBlocks;
	NetID nearestID = 0;
	int nearestDist = 0x7fffffff;

	//search through all the nearby objects for players
	while (numberOfObjects--)
	{
		DISPLAYBLOCK *objectPtr = OnScreenBlockList[numberOfObjects];
		STRATEGYBLOCK *sbPtr = objectPtr->ObStrategyBlock;

		if (sbPtr && sbPtr->I_SBtype == I_BehaviourNetGhost)
		{
			NETGHOSTDATABLOCK *ghostData;
			ghostData = (NETGHOSTDATABLOCK *)sbPtr->SBdataptr;

			//we're only interested in player netghosts
			if (ghostData)
			{
				if (ghostData->type == I_BehaviourMarinePlayer ||
				    ghostData->type == I_BehaviourPredatorPlayer ||
				    ghostData->type == I_BehaviourAlienPlayer)
				{
					VECTORCH targetView;
					SmartTarget_GetCofM(objectPtr, &targetView);

					//get the players screen coordinates
					if (targetView.vz > 0)
					{
						int screenX = WideMulNarrowDiv
						              (
						                  targetView.vx,
						                  Global_VDB_Ptr->VDB_ProjX,
						                  targetView.vz
						              );
						int screenY = WideMulNarrowDiv
						              (
						                  targetView.vy,
						                  Global_VDB_Ptr->VDB_ProjY,
						                  targetView.vz
						              );

						if (screenX < 0) {
							screenX = -screenX;
						}

						if (screenY < 0) {
							screenY = -screenY;
						}

						//make sure the player is in fact on screen
						if (screenX < ScreenDescriptorBlock.SDB_Width / 2 && screenY < ScreenDescriptorBlock.SDB_Height / 2)
						{
							int dist = screenX * screenX + screenY * screenY;

							//is this player closer to the centre of the screen than any before?
							if (dist < nearestDist)
							{
								//the player must be visible
								if (IsThisObjectVisibleFromThisPosition_WithIgnore(objectPtr, Player, &(Global_VDB_Ptr->VDB_World), 1000000))
								{
									nearestDist = dist;
									nearestID = ghostData->playerId;
								}
							}
						}
					}
				}
			}
		}
	}

	{
		int nearestIndex = PlayerIdInPlayerList(nearestID);

		if (nearestIndex != NET_IDNOTINPLAYERLIST)
		{
			//we've found the nearest on screen player, so show the name in the console
			NetworkGameConsoleMessage(TEXTSTRING_MULTIPLAYERCONSOLE_PLAYERSEEN, netGameData.playerData[nearestIndex].name, 0);
		}
	}
}


static void CheckForPointBasedObjectRespawn()
{
	int score = 0;

	// (Nothing comes back in a game that plays as single player.)
	if (netGameData.pointsForRespawn == 0 || SameAsSinglePlayer()) {
		return;
	}

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.gameType == NGT_Coop)
		{
			for (int j = 0; j < 3; j++) {
				score += netGameData.playerData[i].aliensKilled[j] * netGameData.aiKillValues[j];
			}
		}
		else {
			score += netGameData.playerData[i].playerScore;
		}
	}

	if (score >= (netGameData.lastPointsBasedRespawn + netGameData.pointsForRespawn))
	{
		//time for pickups to respawn
		AddNetMsg_RespawnPickups();
		netGameData.lastPointsBasedRespawn = score - (score % netGameData.pointsForRespawn);
		RespawnAllPickups();
		PrintStringTableEntryInConsole(TEXTSTRING_MULTIPLAYER_WEAPON_RESPAWN);
	}
}


static int CountMultiplayerLivesLeft()
{
	int livesUsed = 0;

	//count the lives used
	if (netGameData.useSharedLives)
	{
		//lives used is equal to number of deaths + number of currently living players
		if (netGameData.gameType == NGT_CoopDeathmatch)
		{
			//shared lives , are shared between team members
			livesUsed = netGameData.numDeaths[netGameData.myCharacterType];
		}
		else {
			livesUsed = netGameData.numDeaths[0] + netGameData.numDeaths[1] + netGameData.numDeaths[2];
		}

		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			if (netGameData.playerData[i].playerId)
			{
				//add an extra life if this player is currently alive
				if (netGameData.gameType == NGT_CoopDeathmatch)
				{
					if (netGameData.playerData[i].characterType != netGameData.myCharacterType)
					{
						//don't count this player , since he isn't on the same team
						continue;
					}
				}

				livesUsed += netGameData.playerData[i].playerAlive;
			}
		}
	}
	else
	{
		int index = PlayerIdInPlayerList(AvPNetID);

		if (index == NET_IDNOTINPLAYERLIST) {
			return FALSE;
		}

		for (int i = 0; i < NET_MAXPLAYERS; i++) {
			livesUsed += netGameData.playerData[i].playerFrags[index];
		}

		livesUsed += netGameData.playerData[index].deathsFromAI;
		livesUsed += netGameData.playerData[index].playerAlive;
	}

	if (livesUsed > netGameData.maxLives) {
		return 0;
	}

	return (netGameData.maxLives - livesUsed);
}

BOOL AreThereAnyLivesLeft()
{
	if (netGameData.maxLives == 0) {
		return TRUE;    //infinite lives
	}

	if (netGameData.gameType != NGT_Individual &&
	    netGameData.gameType != NGT_Coop &&
	    netGameData.gameType != NGT_CoopDeathmatch)
	{
		return TRUE;    //lives only affect some game types
	}

	return (CountMultiplayerLivesLeft() > 0);
}

#define FONT_ALIENSYMBOL    176
#define FONT_MARINESYMBOL   177
#define FONT_PREDATORSYMBOL 178

// How far apart the lines below are on the screen, in its own pixels: the small font is 15 high
static const int kCoopLineStep = 20;

// The score key in a campaign: each player and whether they're alive, out until the next level, or still joining. For the same 5 seconds the
// score table shows for, and drawn from the player list every machine has, so it needs no message of its own.
static void Coop_ShowWhoIsAlive(void)
{
	ShowMultiplayerScoreTimer -= RealFrameTime;

	if (ShowMultiplayerScoreTimer < 0)
	{
		ShowMultiplayerScoreTimer = 0;
	}

	int y = ScreenDescriptorBlock.SDB_Height / 3;

	for (int place = 0; place < NET_MAXPLAYERS; place++)
	{
		char line[100];

		if (CoopMenu_PlayerLine(place, line, sizeof(line)))
		{
			RenderStringCentred(line, ScreenDescriptorBlock.SDB_Width / 2, y, 0xffffffff);
			y += kCoopLineStep;
		}
	}
}

void DoMultiplayerEndGameScreen(void)
{
	PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
	int i, j;
	int x, y;
	// text holds 100 bytes: up to 99 characters (letters, digits, spaces and so
	// on), then a zero byte that marks where the text stops (the "end marker").
	// The "lives left" line below holds words from the game's language text file on
	// disk, so it uses _snprintf, which stops at 99 characters, and then writes the
	// end marker into the last byte itself, because _snprintf leaves it out when it
	// has to cut the text short. Text that fits is shown exactly as before. (The
	// other lines here only write numbers, which always fit.)
	char text[100];

	if (netGameData.myGameState == NGS_EndGameScreen) {
		D3D_FadeDownScreen(16384, 0);
	}
	else
	{
		ShowMultiplayerScoreTimer -= RealFrameTime;

		if (ShowMultiplayerScoreTimer <= 0) {
			ShowMultiplayerScoreTimer = 0;
		}
	}

	//   RenderStringCentred("Test Endgame Screen",ScreenDescriptorBlock.SDB_Width/2,ScreenDescriptorBlock.SDB_Height/2,0xffffffff);
	//draw headings
	y = 150;
	x = 120;

	for (i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId)
		{
			RenderStringVertically(netGameData.playerData[i].name, x, y, 0xffffffff);
			x += 20;
		}
	}

	x += 30;

	if (netGameData.gameType == NGT_Coop)
	{
		if (NPCHive.AliensCanBeGenerated)
		{
			RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_ALIENS), x, y, 0xffffffff);
			x += 20;
		}

		if (NPCHive.PredAliensCanBeGenerated)
		{
			RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_PREDALIENS), x, y, 0xffffffff);
			x += 20;
		}

		if (NPCHive.PraetoriansCanBeGenerated)
		{
			RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_PRAETORIANS), x, y, 0xffffffff);
			x += 20;
		}

		x += 20;
		RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_SCORE), x, y, 0xffffffff);
		x += 20;
	}
	else
	{
		RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_SCOREFOR), x, y, 0xffffffff);
		x += 30;
		RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_SCOREAGAINST), x, y, 0xffffffff);
		x += 30;
	}

	y += 10;

	for (i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId)
		{
			RenderStringCentred(netGameData.playerData[i].name, 50, y, 0xffffffff);
			// draw the player's species symbol
			char symbol[2] = {0, 0};

			switch (netGameData.playerData[i].characterType)
			{
				case NGCT_Marine :
					symbol[0] = FONT_MARINESYMBOL;
					RenderStringCentred(symbol, 100, y, 0xffffffff);
					break;

				case NGCT_Alien :
					symbol[0] = FONT_ALIENSYMBOL;
					RenderStringCentred(symbol, 100, y, 0xffffffff);
					break;

				case NGCT_Predator :
					symbol[0] = FONT_PREDATORSYMBOL;
					RenderStringCentred(symbol, 100, y, 0xffffffff);
					break;

				default:
					break;
			}

			x = 120;

			for (j = 0; j < NET_MAXPLAYERS; j++)
			{
				if (netGameData.playerData[j].playerId)
				{
					sprintf(text, "%d", netGameData.playerData[i].playerFrags[j]);

					if (i == j) {
						RenderStringCentred(text, x, y, 0xffff0000);
					}
					else {
						RenderStringCentred(text, x, y, 0xff00ff00);
					}

					x += 20;
				}
			}

			x += 30;

			if (netGameData.gameType == NGT_Coop)
			{
				int score = 0;

				if (NPCHive.AliensCanBeGenerated)
				{
					sprintf(text, "%d", netGameData.playerData[i].aliensKilled[0]);
					RenderStringCentred(text, x, y, 0xff00ff00);
					x += 20;
				}

				if (NPCHive.PredAliensCanBeGenerated)
				{
					sprintf(text, "%d", netGameData.playerData[i].aliensKilled[1]);
					RenderStringCentred(text, x, y, 0xff00ff00);
					x += 20;
				}

				if (NPCHive.PraetoriansCanBeGenerated)
				{
					sprintf(text, "%d", netGameData.playerData[i].aliensKilled[2]);
					RenderStringCentred(text, x, y, 0xff00ff00);
					x += 20;
				}

				for (j = 0; j < 3; j++) {
					score += netGameData.playerData[i].aliensKilled[j] * netGameData.aiKillValues[j];
				}

				x += 20;
				sprintf(text, "%d", score);
				RenderStringCentred(text, x, y, 0xff00ff00);
			}
			else
			{
				sprintf(text, "%d", netGameData.playerData[i].playerScore);
				RenderStringCentred(text, x, y, 0xff00ff00);
				x += 30;
				sprintf(text, "%d", netGameData.playerData[i].playerScoreAgainst);
				RenderStringCentred(text, x, y, 0xffff0000);
				x += 30;
			}

			y += 20;
		}
	}

	if (netGameData.gameType == NGT_Coop)
	{
		//show kills by ai aliens
		RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_ALIENS), 50, y, 0xffffffff);
		x = 120;

		for (j = 0; j < NET_MAXPLAYERS; j++)
		{
			if (netGameData.playerData[j].playerId)
			{
				sprintf(text, "%d", netGameData.playerData[j].deathsFromAI);
				RenderStringCentred(text, x, y, 0xff00ff00);
				x += 20;
			}
		}

		y += 20;
	}

	if (netGameData.maxLives != 0)
	{
		if (netGameData.gameType == NGT_Individual ||
		    netGameData.gameType == NGT_Coop ||
		    netGameData.gameType == NGT_CoopDeathmatch)
		{
			y += 20;

			if (y < 240)
			{
				//make sure the text appears far enough down the screen , so we don't
				//overlap with the species score stuff
				y = 240;
			}

			//display remaining lives
			// size-limited: see the note where text is declared
			_snprintf(text, sizeof(text) - 1, "%s: %d", GetTextString(TEXTSTRING_MULTIPLAYER_LIVES_LEFT), CountMultiplayerLivesLeft());
			text[sizeof(text) - 1] = '\0';
			RenderStringCentred(text, ScreenDescriptorBlock.SDB_Width / 2, y, 0xffffffff);
		}
	}

	if (netGameData.gameType == NGT_CoopDeathmatch)
	{
		//show species scores
		x += 50;
		//titles
		RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_ALIEN), x, 160, 0xffffffff);
		RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_MARINE), x, 180, 0xffffffff);
		RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_PREDATOR), x, 200, 0xffffffff);
		//scores
		x += 50;
		RenderStringVertically(GetTextString(TEXTSTRING_MULTIPLAYER_SPECIESSCORE), x, 150, 0xffffffff);
		sprintf(text, "%d", netGameData.teamScores[NGCT_Alien]);
		RenderStringCentred(text, x, 160, 0xffffffff);
		sprintf(text, "%d", netGameData.teamScores[NGCT_Marine]);
		RenderStringCentred(text, x, 180, 0xffffffff);
		sprintf(text, "%d", netGameData.teamScores[NGCT_Predator]);
		RenderStringCentred(text, x, 200, 0xffffffff);
	}

	if (netGameData.myGameState == NGS_EndGameScreen)
	{
		if (AvP.Network == I_Host)
		{
			//pause before the host can restart
			if (netGameData.stateCheckTimeDelay > 0)
			{
				netGameData.stateCheckTimeDelay -= RealFrameTime;
				return;
			}

			netGameData.stateCheckTimeDelay = 0;
			RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_PRESSKEYTORESTARTGAME), ScreenDescriptorBlock.SDB_Width / 2, ScreenDescriptorBlock.SDB_Height - 20, 0xffffffff);

			if (DebouncedGotAnyKey)
			{
				int seed = FastRandom();
				RestartNetworkGame(seed);
				AddNetMsg_RestartNetworkGame(seed);
			}
		}
		else
		{
			RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_WAITFORRESTARTGAME), ScreenDescriptorBlock.SDB_Width / 2, ScreenDescriptorBlock.SDB_Height - 20, 0xffffffff);
		}
	}
	else if (!playerStatusPtr->IsAlive)
	{
		if (AreThereAnyLivesLeft()) {
			RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_OPERATETORESPAWN), ScreenDescriptorBlock.SDB_Width / 2, ScreenDescriptorBlock.SDB_Height - 20, 0xffffffff);
		}
		else {
			RenderStringCentred(GetTextString(TEXTSTRING_MULTIPLAYER_OPERATETOOBSERVE), ScreenDescriptorBlock.SDB_Width / 2, ScreenDescriptorBlock.SDB_Height - 20, 0xffffffff);
		}
	}
}

static BOOL IsItPossibleToScore()
{
	if (CalculateMyScore()) {
		return TRUE;
	}

	if (netGameData.gameType == NGT_LastManStanding) {
		return TRUE;
	}

	if (netGameData.gameType == NGT_Coop)
	{
		if (netGameData.aiKillValues[0]) {
			return TRUE;
		}

		if (netGameData.aiKillValues[1]) {
			return TRUE;
		}

		if (netGameData.aiKillValues[2]) {
			return TRUE;
		}

		return FALSE;
	}
	else
	{
		if (netGameData.baseKillValue == 0) {
			return FALSE;
		}

		if (netGameData.useCharacterKillValues && !netGameData.useDynamicScoring)
		{
			if (netGameData.characterKillValues[0]) {
				return TRUE;
			}

			if (netGameData.characterKillValues[1]) {
				return TRUE;
			}

			if (netGameData.characterKillValues[2]) {
				return TRUE;
			}

			return FALSE;
		}
	}

	return TRUE;
}

void DoMultiplayerSpecificHud()
{
	PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
	// text holds 200 bytes: up to 199 characters (letters, digits, spaces and so
	// on), then a zero byte that marks where the text stops (the "end marker").
	// The score and time lines below hold words from the game's language text file
	// on disk, so they use _snprintf, which stops at 199 characters, and then write
	// the end marker into the last byte themselves, because _snprintf leaves it out
	// when it has to cut the text short. Text that fits is shown exactly as before.
	char text[200];

	// Show score table if either
	// 1. Player has asked to
	// 2. Game is over
	// 3. Player is dead and not observing anyone
	if (ShowMultiplayerScoreTimer > 0 ||
	    netGameData.myGameState == NGS_EndGameScreen ||
	    (!playerStatusPtr->IsAlive && !MultiplayerObservedPlayer && !SameAsSinglePlayer()))
	{
		// In a campaign the scores mean nothing, so the same key shows who's in and alive
		if (SameAsSinglePlayer() && netGameData.myGameState != NGS_EndGameScreen)
		{
			Coop_ShowWhoIsAlive();
		}
		else
		{
			DoMultiplayerEndGameScreen();
		}
	}
	else
	{
		if (MultiplayerObservedPlayer)
		{
			// show the name of the observed player
			int index = PlayerIdInPlayerList(MultiplayerObservedPlayer);

			if (index != NET_IDNOTINPLAYERLIST) {
				RenderStringCentred(netGameData.playerData[index].name, ScreenDescriptorBlock.SDB_Width / 2, ScreenDescriptorBlock.SDB_Height / 2, 0xff0000ff);
			}

			// Watching someone can look like a fault, so a campaign says what it is
			if (SameAsSinglePlayer())
			{
				RenderStringCentred(GetTextString(TEXTSTRING_COOP_WATCHING), ScreenDescriptorBlock.SDB_Width / 2, ScreenDescriptorBlock.SDB_Height / 2 + kCoopLineStep, 0xffffffff);
			}
		}
	}

	// show the player's score
	{
		int score = CalculateMyScore();

		// (Single player has no score on the screen, so a campaign doesn't either.)
		if (!SameAsSinglePlayer() && (score || IsItPossibleToScore()))
		{
			// size-limited: see the note where text is declared
			_snprintf(text, sizeof(text) - 1, "%s : %d", GetTextString(TEXTSTRING_MULTIPLAYER_SCORE), score);
			text[sizeof(text) - 1] = '\0';
			RenderString(text, 5, 0, 0xffffffff);
		}
	}

	// show time left
	if (netGameData.timeLimit > 0 && !SameAsSinglePlayer())
	{
		int hoursLeft;
		int minutesLeft;
		int secondsLeft = (netGameData.timeLimit * 60) - (netGameData.GameTimeElapsed >> 16);

		if (secondsLeft < 0) {
			secondsLeft = 0;
		}

		hoursLeft = secondsLeft / 3600;
		minutesLeft = (secondsLeft / 60) % 60;
		secondsLeft %= 60;
		// display time left as hh:mm:ss
		// size-limited: see the note where text is declared
		_snprintf(text, sizeof(text) - 1, "%s : %02d:%02d:%02d", GetTextString(TEXTSTRING_MULTIPLAYER_TIME), hoursLeft, minutesLeft, secondsLeft);
		text[sizeof(text) - 1] = '\0';
		RenderString(text, 5, 20, 0xffffffff);
	}
}

void GetNextMultiplayerObservedPlayer()
{
	static int LastFrameTried;

	// Use the frame counter to debounce changing player to observe
	if (LastFrameTried == GlobalFrameCounter || LastFrameTried + 1 == GlobalFrameCounter)
	{
		// obviously tried last frame , so don't bother trying to find a player to watch
		LastFrameTried = GlobalFrameCounter;
		return;
	}
	else
	{
		int cur_index = PlayerIdInPlayerList(MultiplayerObservedPlayer);
		int next_index;
		int count = 0;
		LastFrameTried = GlobalFrameCounter;

		for (next_index = cur_index + 1; count < NET_MAXPLAYERS; next_index++, count++)
		{
			if (next_index == NET_MAXPLAYERS) {
				next_index = 0;
			}

			if (netGameData.playerData[next_index].playerId)
			{
				if (netGameData.playerData[next_index].playerAlive)
				{
					DYNAMICSBLOCK *dynPtr = Player->ObStrategyBlock->DynPtr;
					MultiplayerObservedPlayer = netGameData.playerData[next_index].playerId;
					// need to disable gravity , and make sure the playher isn't moving
					// of his own accord
					dynPtr->GravityOn = 0;
					dynPtr->LinImpulse.vx = 0;
					dynPtr->LinImpulse.vy = 0;
					dynPtr->LinImpulse.vz = 0;
					dynPtr->LinVelocity.vx = 0;
					dynPtr->LinVelocity.vy = 0;
					dynPtr->LinVelocity.vz = 0;
					return;
				}
			}
		}

		// oh well , no one available to observe
		TurnOffMultiplayerObserveMode();
	}
}

void TurnOffMultiplayerObserveMode()
{
	if (MultiplayerObservedPlayer)
	{
		DYNAMICSBLOCK *dynPtr = Player->ObStrategyBlock->DynPtr;
		MultiplayerObservedPlayer = 0;
		// need to turn gravity and collisions back on
		dynPtr->GravityOn = 1;
	}
}

void CheckStateOfObservedPlayer()
{
	int index = PlayerIdInPlayerList(MultiplayerObservedPlayer);

	if (!MultiplayerObservedPlayer) {
		return;
	}

	// our observed player must exist
	if (index == NET_IDNOTINPLAYERLIST)
	{
		TurnOffMultiplayerObserveMode();
		return;
	}

	// he must also be alive
	if (!netGameData.playerData[index].playerAlive)
	{
		TurnOffMultiplayerObserveMode();
		return;
	}
}

static int CalculateMyScore()
{
	int myIndex = PlayerIdInPlayerList(AvPNetID);

	if (myIndex == NET_IDNOTINPLAYERLIST) {
		return 0;
	}

	if (netGameData.gameType == NGT_Coop)
	{
		int score = 0;

		for (int i = 0; i < 3; i++) {
			score += netGameData.playerData[myIndex].aliensKilled[i] * netGameData.aiKillValues[i];
		}

		return score;
	}
	else {
		return netGameData.playerData[myIndex].playerScore;
	}
}

static void PeriodicScoreUpdate()
{
	static unsigned int timer = 0;
	static int playerIndex = 0;
	int count = 0;
	
	// cycle through the players , sending scores every 2 seconds
	timer += RealFrameTime;

	if (timer < 2 * ONE_FIXED) {
		return;
	}

	timer = 0;

	if (netGameData.myGameState != NGS_Playing) {
		return;
	}

	for (playerIndex++; count < NET_MAXPLAYERS; count++, playerIndex++)
	{
		if (playerIndex >= NET_MAXPLAYERS && netGameData.gameType == NGT_CoopDeathmatch)
		{
			// for species deathmatch games also update team scores occasionly
			AddNetMsg_SpeciesScores();
		}

		playerIndex %= NET_MAXPLAYERS;

		if (netGameData.playerData[playerIndex].playerId)
		{
			AddNetMsg_PlayerScores(playerIndex);
			return;
		}
	}
}

static int GetStrategySynchObjectChecksum()
{
	/*
	Generate a number from the sbnames of all the strategies that need to be synched
	*/
	int sum = 0;
	int position = 0;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *sbPtr = ActiveStBlockList[i];

		if (sbPtr->I_SBtype == I_BehaviourBinarySwitch ||
		    sbPtr->I_SBtype == I_BehaviourLinkSwitch ||
		    sbPtr->I_SBtype == I_BehaviourTrackObject)
		{
			position++;
			sum += (*(int *)&sbPtr->SBname[0]) * position;
		}
	}

	if (sum == 0) {
		sum = 1;
	}

	return sum;
}

/*----------------------------------------------------------------------
  The mode where a network game plays as single player does, and moving on through a campaign
  ----------------------------------------------------------------------*/

// Whether everyone playing is out. A player only counts once he's really in the level, which shows here as a
// body on this machine: someone who has connected and is still in the menus doesn't stop the others starting again.
bool Coop_EveryoneIsDead(void)
{
	if (AvP.Network == I_No_Network || netGameData.coopCampaign == kCampaignNone)
	{
		return false;
	}

	if (PlayerStatusPtr->IsAlive)
	{
		return false;
	}

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId == 0 || netGameData.playerData[i].playerId == AvPNetID)
		{
			continue;
		}

		if (netGameData.playerData[i].playerAlive && FindGhost(netGameData.playerData[i].playerId, GHOST_PLAYEROBJECTID))
		{
			return false;
		}
	}

	return true;
}

// How long the host's machine waits for a key, once everyone is out, before starting the level again for them. A
// player alone waits as long as he likes, as in single player.
static const int kSecondsToWaitWithOthersWatching = 10;

void Coop_WhileOut(bool asksToChangeWhoHeWatches)
{
	static int everyoneOutFor = 0;

	// He watches someone who's alive, if there's anyone else here at all. A player alone has nobody to watch, and
	// he mustn't be given himself. The game moves on by itself when the one he's watching dies.
	bool someoneElseIsHere = false;

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		if (netGameData.playerData[i].playerId != 0 && netGameData.playerData[i].playerId != AvPNetID)
		{
			someoneElseIsHere = true;
		}
	}

	if (someoneElseIsHere && (asksToChangeWhoHeWatches || !MultiplayerObservedPlayer))
	{
		GetNextMultiplayerObservedPlayer();
	}

	// Only the host knows for sure, and only the host can start a level again for everyone
	if (AvP.Network != I_Host || !Coop_EveryoneIsDead())
	{
		everyoneOutFor = 0;
		return;
	}

	everyoneOutFor += RealFrameTime;

	bool const timeIsUp = someoneElseIsHere && everyoneOutFor >= kSecondsToWaitWithOthersWatching * ONE_FIXED;

	// A Hudson run that asked to die (dieat=) presses its key here too, 3 seconds after he went out, as it does in single player
	if (DebouncedGotAnyKey || timeIsUp || Hudson_PressesAKeyAfterHeDied())
	{
		// The same thing the host's "restart" command does, and the one single player's death leads to in the end
		int const seed = FastRandom();

		everyoneOutFor = 0;
		LogString("campaign: everyone is out, so the host starts the level again");
		AddNetMsg_RestartNetworkGame(seed);
		NetSendMessagesNow();
		RestartNetworkGame(seed);
	}
}

bool SameAsSinglePlayer(void)
{
	return AvP.Network == I_No_Network || netGameData.coopCampaign != kCampaignNone;
}

// Set when the level that's running is ending because everyone is going on to the next one.
static bool coopLevelChangeIsDue = false;

// The level just finished, for the next loading screen to name, or -1. And whether that was the last level, for the page after the game.
static int coopFinishedPlace = -1;
static bool coopCampaignIsOver = false;

static void Coop_LevelStopsRunning(void);

static void Coop_GoToPlace(int place)
{
	Coop_LevelStopsRunning();

	// What this player carries on with, while there's still a level and a player to read it from. A message that arrives during a load finds neither.
	if (netGameData.myGameState == NGS_Playing)
	{
		Campaign_NoteKit(place);
	}

	// Someone out and watching gets his own body back. If he kept watching, the next level would pull him along
	// behind whoever he was watching, because watching works by moving the player to where they are.
	TurnOffMultiplayerObserveMode();

	// The loading screen says which level was finished, if this is the level after it
	coopFinishedPlace = place == netGameData.coopCampaignLevel + 1 ? netGameData.coopCampaignLevel : -1;

	netGameData.coopCampaignLevel = place;
	coopLevelChangeIsDue = place < Campaign_LevelCount(netGameData.coopCampaign);
	AvP.MainLoopRunning = 0;

	if (!coopLevelChangeIsDue)
	{
		coopCampaignIsOver = true;
	}
}

/*----------------------------------------------------------------------
  WAITING FOR EVERYONE ON THE LOADING SCREEN. Single player waits there for a key, and in this mode so does everyone. A player who has pressed a key tells the host once a second until the host says go,
  and the host says go when it has pressed its own key and every other player has told it. Nobody can hold the others up for long: after a minute from your own key, you go anyway, and someone
  who turns up after that is let straight in, because the host answers anyone who asks once the level is running. What these messages can do is only move that moment, so a player who lies about being ready only starts the level sooner.
  ----------------------------------------------------------------------*/
static DWORD const kMostWaitOnLoadingScreenMilliseconds = 60000;
static DWORD const kSayReadyEveryMilliseconds = 1000;

static bool coopPlaceIsReady[NET_MAXPLAYERS];
static bool coopGoHasCome = false;
static bool coopLevelIsRunningOnHost = false;

// The host keeps answering "go" while a level runs, and it mustn't say so once the next one is loading.
static void Coop_LevelStopsRunning(void)
{
	coopLevelIsRunningOnHost = false;
}

static bool coopThisPlayerPressed = false;
static DWORD coopPressedAt = 0;
static DWORD coopLastSaidReadyAt = 0;

static void AddNetMsg_CoopWithNothingElse(NETMESSAGETYPE type)
{
	int const headerSize = sizeof(NETMESSAGEHEADER);
	int const numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if (headerSize > numBytesLeft)
	{
		NoteSendBufferFull(headerSize);
		return;
	}

	NETMESSAGEHEADER *const headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	headerPtr->type = (unsigned char)type;
}

void Coop_WaitOnLoadingScreenBegins(void)
{
	netGameData.coopWaitingFor = 0;
	memset(coopPlaceIsReady, 0, sizeof(coopPlaceIsReady));
	coopGoHasCome = false;
	coopLevelIsRunningOnHost = false;
	coopThisPlayerPressed = false;
}

bool Coop_MayStartLevel(bool keyHasBeenPressed)
{
	DWORD const now = GetTickCount();

	if (keyHasBeenPressed && !coopThisPlayerPressed)
	{
		coopThisPlayerPressed = true;
		coopPressedAt = now;
		coopLastSaidReadyAt = now - kSayReadyEveryMilliseconds;
	}

	if (!coopThisPlayerPressed)
	{
		return false;
	}

	bool const waitedLongEnough = now - coopPressedAt >= kMostWaitOnLoadingScreenMilliseconds;

	if (AvP.Network == I_Host)
	{
		bool placeHasPlayer[NET_MAXPLAYERS];

		for (int place = 0; place < NET_MAXPLAYERS; place++)
		{
			placeHasPlayer[place] = netGameData.playerData[place].playerId != 0;
		}

		// Who everyone is waiting for, which goes out in the game description
		netGameData.coopWaitingFor = CoopWaiting_Places(placeHasPlayer, coopPlaceIsReady, NET_MAXPLAYERS, PlayerIdInPlayerList(AvPNetID), coopThisPlayerPressed);

		if (!waitedLongEnough && !NetEveryoneElseIsReady(placeHasPlayer, coopPlaceIsReady, NET_MAXPLAYERS, PlayerIdInPlayerList(AvPNetID)))
		{
			return false;
		}

		coopLevelIsRunningOnHost = true;
		AddNetMsg_CoopWithNothingElse(NetMT_CoopGo);
		NetSendMessages();

		return true;
	}

	if (coopGoHasCome || waitedLongEnough)
	{
		return true;
	}

	if (now - coopLastSaidReadyAt >= kSayReadyEveryMilliseconds)
	{
		coopLastSaidReadyAt = now;
		AddNetMsg_CoopWithNothingElse(NetMT_CoopReady);
	}

	return false;
}

static void ProcessNetMsg_CoopReady(NetID senderId)
{
	if (AvP.Network != I_Host || !SameAsSinglePlayer())
	{
		return;
	}

	int const place = PlayerIdInPlayerList(senderId);

	if (place == NET_IDNOTINPLAYERLIST)
	{
		return;
	}

	coopPlaceIsReady[place] = true;

	// Someone arriving late has missed the word, so they get it now. Someone asking again and again costs one answer a second at most.
	static unsigned int secondStarted = 0;
	static int answersThisSecond = 0;

	if (coopLevelIsRunningOnHost && NetThisSecondHasRoom(GetTickCount(), &secondStarted, &answersThisSecond, NET_MAXPLAYERS))
	{
		AddNetMsg_CoopWithNothingElse(NetMT_CoopGo);
	}
}

static void ProcessNetMsg_CoopGo(void)
{
	if (AvP.Network == I_Peer && SameAsSinglePlayer())
	{
		coopGoHasCome = true;
	}
}

void AddNetMsg_CoopNextLevel(int place)
{
	NETMESSAGEHEADER *headerPtr;
	NETMESSAGE_COOPNEXTLEVEL *messagePtr;
	int headerSize = sizeof(NETMESSAGEHEADER);
	int messageSize = sizeof(NETMESSAGE_COOPNEXTLEVEL);
	int numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if (headerSize + messageSize > numBytesLeft)
	{
		NoteSendBufferFull(headerSize + messageSize);
		return;
	}

	headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	messagePtr = (NETMESSAGE_COOPNEXTLEVEL *)endSendBuffer;
	endSendBuffer += messageSize;

	headerPtr->type = (unsigned char)NetMT_CoopNextLevel;
	messagePtr->campaign = (unsigned char)netGameData.coopCampaign;
	messagePtr->place = (unsigned char)place;
}

void Coop_LevelEndReached(void)
{
	// For proof that the same chain reached the end on every machine. Only a test run writes this, in the log, because a level's end happens once.
	LogString(AvP.Network == I_Host ? "net: the level's end was reached on the host, so everyone goes on" : "net: the level's end was reached on this client too, and the host decides");

	if (AvP.Network != I_Host)
	{
		return;
	}

	int const place = netGameData.coopCampaignLevel + 1;

	AddNetMsg_CoopNextLevel(place);

	// Now, because this machine is about to stop sending anything until the next level has loaded, or leave if this was the last
	NetSendMessagesNow();
	Net_ServiceNetwork();

	Coop_GoToPlace(place);
}

static void ProcessNetMsg_CoopNextLevel(NETMESSAGE_COOPNEXTLEVEL *messagePtr)
{
	if (AvP.Network != I_Peer)
	{
		return;
	}

	if (!NetCampaignMoveIsGood(messagePtr->campaign, messagePtr->place, netGameData.coopCampaign, netGameData.coopCampaignLevel, Campaign_LevelCount(netGameData.coopCampaign)))
	{
		Net_NoteRefused("a next-level message named a place this campaign can't go to, so it was ignored");
		return;
	}

	Coop_GoToPlace(messagePtr->place);
}

/*----------------------------------------------------------------------
  THE CO-OP LOBBY'S TWO MESSAGES. See coop_lobby.h. On a host, coopChoice is what it last said, and on a joiner what it last took.
  ----------------------------------------------------------------------*/
static NETMESSAGE_COOPCAMPAIGNCHOICE coopChoice;
static bool coopChoiceIsInUse = false;   // a host: this game came from the page, so the choice is said. A joiner: one has been taken
static DWORD coopLastSaidAt = 0;
static int coopMyMapAnswer = kCoopMapIsMissing;
static int coopMyLastAnswerSaid = -1;
static unsigned int coopChoiceLastAnswered = 0;

// What a host has heard from each player, for the choice it's on. kCoopNoAnswerYet until they say.
static int const kCoopNoAnswerYet = -1;
static int coopMapAnswers[NET_MAXPLAYERS];

static void Coop_ForgetTheAnswers(void)
{
	for (int place = 0; place < NET_MAXPLAYERS; place++)
	{
		coopMapAnswers[place] = kCoopNoAnswerYet;
	}
}

static void Coop_ForgetTheLobby(void)
{
	coopLobbyState = kCoopLobbyNone;
	coopFinishedPlace = -1;
	coopCampaignIsOver = false;
	netGameData.coopWaitingFor = 0;
	memset(&coopChoice, 0, sizeof(coopChoice));
	coopChoiceIsInUse = false;
	coopHostHasStarted = false;
	coopMyMapAnswer = kCoopMapIsMissing;
	coopMyLastAnswerSaid = -1;
	coopChoiceLastAnswered = 0;
	netGameData.coopCustomMap[0] = 0;
	Coop_ForgetTheAnswers();
}

void Net_CoopLobbyOpens(void)
{
	Coop_ForgetTheLobby();
	coopLobbyState = kCoopLobbyOpen;
	coopChoiceIsInUse = true;
}

void Net_CoopLobbyJoined(int state)
{
	Coop_ForgetTheLobby();
	coopLobbyState = CoopLobby_StateIsReal(state) ? state : kCoopLobbyNone;
}

void Net_CoopLobbyEnds(void)
{
	coopLobbyState = kCoopLobbyNone;
}

bool Net_CoopChoiceIsHeard(void)
{
	return coopChoiceIsInUse;
}

bool Net_CoopHostHasStarted(void)
{
	return coopHostHasStarted;
}

int Net_CoopMyMapAnswer(void)
{
	return coopMyMapAnswer;
}

bool Net_CoopPlayerHasTheMap(int place)
{
	return NetNumberFitsTable(place, NET_MAXPLAYERS) && coopMapAnswers[place] == kCoopMapIsHere;
}

// The rows, as the message says them. Everything not used stays nothing, so two choices with the same rows are the same bytes.
static void Coop_ChoiceFromTheRows(NETMESSAGE_COOPCAMPAIGNCHOICE &choice)
{
	bool const isCustom = netGameData.coopCampaign == kCampaignCustom;

	memset(&choice, 0, sizeof(choice));
	choice.number = coopChoice.number;
	choice.campaign = (unsigned char)netGameData.coopCampaign;
	choice.place = (unsigned char)netGameData.coopCampaignLevel;
	choice.species = (unsigned char)(isCustom ? netGameData.coopCustomSpecies : netGameData.coopCampaign - kCoopChoiceFirstCampaign);
	choice.difficulty = (unsigned char)netGameData.coopDifficulty;
	choice.keepKit = netGameData.coopKeepKit ? 1 : 0;
	choice.friendlyFireOff = netGameData.disableFriendlyFire ? 1 : 0;
	choice.started = coopHostHasStarted ? 1 : 0;
	choice.rollNumber = netGameData.coopRollNumber;

	if (isCustom)
	{
		memcpy(choice.mapName, netGameData.coopCustomMap, sizeof(choice.mapName));
		choice.mapSize = netGameData.coopCustomMapSize;
		memcpy(choice.mapSum, netGameData.coopCustomMapSum, sizeof(choice.mapSum));
	}
}

static void AddNetMsg_CoopCampaignChoice(void)
{
	int const headerSize = sizeof(NETMESSAGEHEADER);
	int const messageSize = sizeof(NETMESSAGE_COOPCAMPAIGNCHOICE);
	int const numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if (headerSize + messageSize > numBytesLeft)
	{
		return;
	}

	NETMESSAGEHEADER *const headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	NETMESSAGE_COOPCAMPAIGNCHOICE *const messagePtr = (NETMESSAGE_COOPCAMPAIGNCHOICE *)endSendBuffer;
	endSendBuffer += messageSize;

	headerPtr->type = (unsigned char)NetMT_CoopCampaignChoice;
	*messagePtr = coopChoice;
}

void Net_CoopHostSays(void)
{
	if (AvP.Network != I_Host || !coopChoiceIsInUse)
	{
		return;
	}

	NETMESSAGE_COOPCAMPAIGNCHOICE now;

	Coop_ChoiceFromTheRows(now);

	// Anything different is a new choice with a higher number, and what anyone said about the last one no longer counts
	if (memcmp(&now, &coopChoice, sizeof(now)) != 0)
	{
		now.number = coopChoice.number + 1;
		coopChoice = now;
		Coop_ForgetTheAnswers();
	}
	else if (timeGetTime() - coopLastSaidAt < 1000)
	{
		return;
	}

	coopLastSaidAt = timeGetTime();
	AddNetMsg_CoopCampaignChoice();
}

void Net_CoopHostStarts(void)
{
	coopHostHasStarted = true;
	coopLobbyState = kCoopLobbyNone;

	Net_CoopHostSays();

	// Now, because a load is about to begin, and joiners are waiting on this word
	NetSendMessagesNow();
}

// What this joiner can say about the map the host has chosen. A stock campaign's levels are on every machine.
static int Coop_WorkOutMyMapAnswer(void)
{
	if (coopChoice.campaign != kCoopChoiceCustomCampaign)
	{
		return kCoopMapIsHere;
	}

	unsigned int size = 0;
	unsigned char sum[kCoopMapSumBytes] = { 0 };

	// Only among the maps this machine listed itself. The host's words are never made into a path.
	bool const haveIt = CustomMaps_SizeAndSum(CustomMaps_Named(coopChoice.mapName, sizeof(coopChoice.mapName)), &size, sum);

	return CoopMap_Answer(haveIt, size, sum, coopChoice.mapSize, coopChoice.mapSum);
}

static void ProcessNetMsg_CoopCampaignChoice(NETMESSAGE_COOPCAMPAIGNCHOICE *messagePtr)
{
	// Once a joiner is in the game it has what it needs. This is still said, for anyone arriving late.
	if (AvP.Network != I_Peer || coopLobbyState == kCoopLobbyNone)
	{
		return;
	}

	switch (CoopChoice_Step(messagePtr->number, coopChoiceIsInUse, coopChoice.number, coopHostHasStarted))
	{
		case kCoopChoiceIsARepeat:
			return;
		case kCoopChoiceIsOlder:
			Net_NoteRefused(kCoopRefusedOlder);
			return;
		case kCoopChoiceIsAfterTheStart:
			Net_NoteRefused(kCoopRefusedAfterStart);
			return;
		default:
			break;
	}

	// A custom map is one level. Campaign_LevelCount only says so once a map is chosen, and this is what would choose it.
	int const levels = messagePtr->campaign == kCoopChoiceCustomCampaign ? 1 : Campaign_LevelCount(messagePtr->campaign);
	const char *const why = CoopChoice_WhyRefused(*messagePtr, levels);

	if (why)
	{
		Net_NoteRefused(why);
		return;
	}

	// It's good, all of it, so now it's this machine's copy of the host's rows
	coopChoice = *messagePtr;
	coopChoiceIsInUse = true;
	coopHostHasStarted = messagePtr->started != 0;

	netGameData.coopCampaign = messagePtr->campaign;
	netGameData.coopCampaignLevel = messagePtr->place;
	netGameData.coopCustomSpecies = messagePtr->species;
	netGameData.coopDifficulty = messagePtr->difficulty;
	netGameData.coopKeepKit = messagePtr->keepKit;
	netGameData.disableFriendlyFire = messagePtr->friendlyFireOff;
	netGameData.coopRollNumber = messagePtr->rollNumber;

	// The rule says the name ends inside its field, and this makes sure of it
	memcpy(netGameData.coopCustomMap, messagePtr->mapName, sizeof(netGameData.coopCustomMap));
	netGameData.coopCustomMap[sizeof(netGameData.coopCustomMap) - 1] = 0;
	netGameData.coopCustomMapSize = messagePtr->mapSize;
	memcpy(netGameData.coopCustomMapSum, messagePtr->mapSum, sizeof(netGameData.coopCustomMapSum));

	coopMyMapAnswer = Coop_WorkOutMyMapAnswer();
}

static void AddNetMsg_CoopMapAnswer(void)
{
	int const headerSize = sizeof(NETMESSAGEHEADER);
	int const messageSize = sizeof(NETMESSAGE_COOPMAPANSWER);
	int const numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if (headerSize + messageSize > numBytesLeft)
	{
		return;
	}

	NETMESSAGEHEADER *const headerPtr = (NETMESSAGEHEADER *)endSendBuffer;
	endSendBuffer += headerSize;
	NETMESSAGE_COOPMAPANSWER *const messagePtr = (NETMESSAGE_COOPMAPANSWER *)endSendBuffer;
	endSendBuffer += messageSize;

	headerPtr->type = (unsigned char)NetMT_CoopMapAnswer;
	messagePtr->number = coopChoice.number;
	messagePtr->answer = (unsigned char)coopMyMapAnswer;
}

void Net_CoopJoinerSays(void)
{
	// Nothing to answer yet, or nobody is listening any more
	if (AvP.Network != I_Peer || coopLobbyState != kCoopLobbyOpen || !coopChoiceIsInUse || coopHostHasStarted)
	{
		return;
	}

	DWORD const kSayAnswerEveryMilliseconds = 2000;

	// A host flicking along a row makes a new choice each time, and the host only takes so many answers a second. The last answer always
	// gets said, on a later frame, so this only thins them out.
	DWORD const kLeastMillisecondsBetweenAnswers = 500;

	DWORD const sinceLast = timeGetTime() - coopLastSaidAt;
	bool const hasNewsToSay = coopMyLastAnswerSaid != coopMyMapAnswer || coopChoiceLastAnswered != coopChoice.number;

	if (sinceLast < kLeastMillisecondsBetweenAnswers || (!hasNewsToSay && sinceLast < kSayAnswerEveryMilliseconds))
	{
		return;
	}

	coopLastSaidAt = timeGetTime();
	coopMyLastAnswerSaid = coopMyMapAnswer;
	coopChoiceLastAnswered = coopChoice.number;
	AddNetMsg_CoopMapAnswer();
}

static void ProcessNetMsg_CoopMapAnswer(NETMESSAGE_COOPMAPANSWER *messagePtr, NetID senderId)
{
	// A joiner never hears these, and they aren't passed on. One that crosses the host's Start on its way has nothing left to say.
	if (AvP.Network != I_Host || coopHostHasStarted)
	{
		return;
	}

	int const place = PlayerIdInPlayerList(senderId);
	const char *const why = CoopAnswer_WhyRefused(*messagePtr, coopLobbyState == kCoopLobbyOpen, place != NET_IDNOTINPLAYERLIST, coopChoice.number);

	if (why)
	{
		Net_NoteRefused(why);
		return;
	}

	// An answer to an earlier choice says nothing about this one
	if (messagePtr->number == coopChoice.number)
	{
		coopMapAnswers[place] = messagePtr->answer;
	}
}

void Net_CoopTakeTheCampaign(void)
{
	netGameData.gameType = NGT_Coop;
	netGameData.gameSpeed = NETGAMESPEED_100PERCENT;
	TimeScale = ONE_FIXED;

	// The loading screen and the score page show a custom map by this name. The copy reads as many bytes as the name it goes into
	// holds, so the map's name has to be at least that long.
	static_assert(sizeof(netGameData.customLevelName) <= sizeof(netGameData.coopCustomMap), "the copy would read past the end of the map's name");
	memcpy(netGameData.customLevelName, netGameData.coopCustomMap, sizeof(netGameData.customLevelName));
	netGameData.customLevelName[sizeof(netGameData.customLevelName) - 1] = 0;

	Campaign_SetDifficulty();
	Coop_TakeCampaignSpecies();

	// The page opened the game before anyone had chosen, so this machine's own place on the list has to catch up
	netGameData.myCharacterSubType = NGSCT_General;

	int const myPlace = PlayerIdInPlayerList(AvPNetID);

	if (myPlace != NET_IDNOTINPLAYERLIST)
	{
		netGameData.playerData[myPlace].characterType = netGameData.myCharacterType;
		netGameData.playerData[myPlace].characterSubType = netGameData.myCharacterSubType;
	}
}

void Coop_TakeCampaignSpecies(void)
{
	switch (netGameData.coopCampaign)
	{
		case kCampaignMarine:
			netGameData.myCharacterType = netGameData.myNextCharacterType = NGCT_Marine;
			AvP.PlayerType = I_Marine;
			break;
		case kCampaignPredator:
			netGameData.myCharacterType = netGameData.myNextCharacterType = NGCT_Predator;
			AvP.PlayerType = I_Predator;
			break;
		case kCampaignAlien:
			netGameData.myCharacterType = netGameData.myNextCharacterType = NGCT_Alien;
			AvP.PlayerType = I_Alien;
			break;
		case kCampaignCustom:
			netGameData.myCharacterType = netGameData.myNextCharacterType = Coop_CustomMapCharacter();
			AvP.PlayerType = netGameData.coopCustomSpecies == 1 ? I_Predator : netGameData.coopCustomSpecies == 2 ? I_Alien : I_Marine;
			break;
		default:
			break;
	}
}

bool Coop_TheCampaignIsOver(void)
{
	return coopCampaignIsOver;
}

// "Level complete: Derelict", on the loading screen of the level after it. False if this loading screen isn't one.
bool Coop_LevelCompleteLine(char *line, size_t room)
{
	if (room == 0 || coopFinishedPlace < 0 || !SameAsSinglePlayer() || AvP.Network == I_No_Network)
	{
		return false;
	}

	_snprintf(line, room - 1, "%s %s", GetTextString(TEXTSTRING_COOP_LEVEL_COMPLETE), CoopMenu_LevelName(netGameData.coopCampaign, coopFinishedPlace));
	line[room - 1] = 0;
	return true;
}

// "Waiting for: Hicks", once this player has pressed a key and is waiting for others
bool Coop_WaitingForLine(char *line, size_t room)
{
	if (!coopThisPlayerPressed)
	{
		return false;
	}

	const char *names[NET_MAXPLAYERS];

	for (int place = 0; place < NET_MAXPLAYERS; place++)
	{
		names[place] = netGameData.playerData[place].playerId != 0 ? netGameData.playerData[place].name : NULL;
	}

	return CoopWaiting_Line(line, room, GetTextString(TEXTSTRING_COOP_WAITING_FOR), netGameData.coopWaitingFor, names, NET_PLAYERNAMELENGTH, PlayerIdInPlayerList(AvPNetID));
}

bool Coop_LevelChangeIsDue(void)
{
	return coopLevelChangeIsDue;
}

void Coop_LevelChangeHasStarted(void)
{
	coopLevelChangeIsDue = false;

	// What this machine had to say about the last level has no meaning in the next
	InitialiseSendMessageBuffer();

	netGameData.myGameState = NGS_Joining;
	SwitchSyncs_LevelIsOver();
	netGameData.myStrategyCheckSum = 0;
	netGameData.stateCheckTimeDelay = 0;
	netGameData.GameTimeElapsed = 0;
	LevelEvents_NewLevel();

	for (int i = 0; i < NET_MAXPLAYERS; i++)
	{
		netGameData.playerData[i].playerAlive = 1;
		netGameData.playerData[i].playerHasLives = 1;
	}

}


/*----------------------------------------------------------------------
  Things that only happen on one machine, and have to reach all of them. See level_events.h.
  ----------------------------------------------------------------------*/

static LevelEventList levelEvents;

void LevelEvents_NewLevel(void)
{
	LevelEvents_Empty(&levelEvents);
}

void LevelEvents_LevelStartsAgain(void)
{
	LevelEvents_ForgetWhatHappened(&levelEvents);
}

void LevelEvents_Note(const char *sourceName, const char *targetName, int request)
{
	LevelEvents_Add(&levelEvents, sourceName, targetName, request, false);
}

void LevelEvents_NoteItem(const char *sourceName, const char *targetName, int request, int x, int y, int z)
{
	if (LevelEvents_Add(&levelEvents, sourceName, targetName, request, true))
	{
		LevelEvent *const added = LevelEvents_Find(&levelEvents, sourceName);

		added->hasPlace = true;
		added->x = x;
		added->y = y;
		added->z = z;
	}
}

static void AddNetMsg_LevelEvent(NETMESSAGETYPE type, const char *source)
{
	int const headerSize = sizeof(NETMESSAGEHEADER);
	int const messageSize = sizeof(NETMESSAGE_LEVELEVENT);
	int const numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if (netGameData.myGameState != NGS_Playing)
	{
		return;
	}

	if (headerSize + messageSize > numBytesLeft)
	{
		NoteSendBufferFull(headerSize + messageSize);
		return;
	}

	NETMESSAGEHEADER *const headerPtr = (NETMESSAGEHEADER *)endSendBuffer;

	endSendBuffer += headerSize;

	NETMESSAGE_LEVELEVENT *const messagePtr = (NETMESSAGE_LEVELEVENT *)endSendBuffer;

	endSendBuffer += messageSize;
	headerPtr->type = (unsigned char)type;
	memcpy(messagePtr->source, source, sizeof(messagePtr->source));
}

// A host says again what has happened, a few each second. Someone who joined late never heard the first time, and a message with no room left loses one, but a machine that has already heard does nothing with a repeat, so saying it again costs only these few bytes.
static void LevelEvents_SayAgainWhatHappened(void)
{
	static const int kMostEachSecond = 16;
	static int nextToSay = 0;

	if (AvP.Network != I_Host || !SameAsSinglePlayer())
	{
		return;
	}

	int said = 0;

	for (int looked = 0; looked < levelEvents.count && said < kMostEachSecond; looked++)
	{
		nextToSay = nextToSay < levelEvents.count ? nextToSay : 0;

		if (levelEvents.events[nextToSay].happened)
		{
			AddNetMsg_LevelEvent(NetMT_LevelEvent, levelEvents.events[nextToSay].source);
			said++;
		}

		nextToSay++;
	}
}

// What this object sets off, from this machine's own copy of the level
static void LevelEvents_SetOff(LevelEvent *event)
{
	char source[kLevelEventNameLength * 2 + 1];
	STRATEGYBLOCK *const target = FindSBWithName(event->target);

	event->happened = true;

	// Each machine says so, so that comparing two logs shows whether a level moved on for both
	LevelEvents_NameAsText(event->source, source);
	LogString(std::string("level event: ") + source + " set off here, and its target is " + (target ? "here" : "not here"));
	RequestState(target, event->request, NULL);
}

void LevelEvents_Happened(const char *sourceName)
{
	if (AvP.Network == I_No_Network || !SameAsSinglePlayer())
	{
		return;
	}

	LevelEvent *const event = LevelEvents_Find(&levelEvents, sourceName);

	if (!event || event->happened)
	{
		return;
	}

	// This machine's own target fires where the event happened, so it's only marked here
	event->happened = true;

	{
		char source[kLevelEventNameLength * 2 + 1];

		LevelEvents_NameAsText(event->source, source);
		LogString(std::string("level event: ") + source + " happened on this machine");
	}

	if (AvP.Network == I_Host)
	{
		AddNetMsg_LevelEvent(NetMT_LevelEvent, sourceName);
	}
	else if (event->clientMayAsk)
	{
		AddNetMsg_LevelEvent(NetMT_LevelEventAsk, sourceName);
	}
}

// From the host only, by the message table. A name that isn't on this level's list does nothing, so no message can start a chain the level doesn't have.
static void ProcessNetMsg_LevelEvent(NETMESSAGE_LEVELEVENT *messagePtr)
{
	if (AvP.Network != I_Peer || !SameAsSinglePlayer())
	{
		return;
	}

	LevelEvent *const event = LevelEvents_Find(&levelEvents, messagePtr->source);

	if (!event)
	{
		Net_NoteRefused("a host named something happening in the level that isn't in this level");
		return;
	}

	if (!event->happened)
	{
		LevelEvents_SetOff(event);
	}
}

// A client saying it picked something up. The host passes every message on, so other clients get this too, and ignore it. Only the host decides.
static void ProcessNetMsg_LevelEventAsk(NETMESSAGE_LEVELEVENT *messagePtr, NetID senderId)
{
	if (AvP.Network != I_Host || !SameAsSinglePlayer())
	{
		return;
	}

	LevelEvent *const event = LevelEvents_Find(&levelEvents, messagePtr->source);
	STRATEGYBLOCK *const asker = FindGhost(senderId, GHOST_PLAYEROBJECTID);
	bool const closeEnough = event && asker && asker->DynPtr && LevelEvents_IsNear(event, asker->DynPtr->Position.vx, asker->DynPtr->Position.vy, asker->DynPtr->Position.vz);

	if (!LevelEvents_ClientMayHaveThis(event, closeEnough))
	{
		Net_NoteRefused("a client asked for something to happen in the level that couldn't have, so it was ignored");
		return;
	}

	LevelEvents_SetOff(event);
	AddNetMsg_LevelEvent(NetMT_LevelEvent, messagePtr->source);
}

/*----------------------------------------------------------------------
  For the hostile test run only. Nothing else calls these.
  ----------------------------------------------------------------------*/

void HudsonTest_AddRawPart(int kind, const void *part, int size)
{
	int const numBytesLeft = NET_MESSAGEBUFFERSIZE - ((int)(endSendBuffer - &sendBuffer[0]));

	if ((int)sizeof(NETMESSAGEHEADER) + size > numBytesLeft)
	{
		return;
	}

	((NETMESSAGEHEADER *)endSendBuffer)->type = (unsigned char)kind;
	endSendBuffer += sizeof(NETMESSAGEHEADER);
	memcpy(endSendBuffer, part, size);
	endSendBuffer += size;
}

void HudsonTest_AddDescriptionWithout(NetID left)
{
	int const place = PlayerIdInPlayerList(left);

	if (place == NET_IDNOTINPLAYERLIST)
	{
		return;
	}

	NetID const id = netGameData.playerData[place].playerId;

	netGameData.playerData[place].playerId = 0;
	AddNetMsg_GameDescription();
	netGameData.playerData[place].playerId = id;
}
