// AvP port by Barry Duncan.
// Modified 2026-11-11 by Melanikus (Third) for the AvP port:
//   the LAMPLOOK command
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the debug and cheat commands are there in a beta, for testers, and using them doesn't count as cheating (see beta_build.h)
// Modified 2026-11-05 by Melanikus for the AvP port:
//   the LIGHTING and SHADOWS commands
// Modified 2026-11-06 by Melanikus for the AvP port:
//   only a host can restart a network game
//   the LAMPFOLLOW command
//   the LAMPSPOTSIZE and LAMPINTENSITY commands
// Modified 2026-11-06 by Melanikus for the AvP port:
//   the SHADOWAMBIENT command, which says whether the map's lights that show no corona cast shadows
//   the SHADOWEVENGAP command, which says whether a shadow starts the same distance behind every part of its polygon
//   the PIXELGLOW, PIXELCURVE and PIXELLEVEL commands, which change how a lamp's light looks per pixel
// Modified 2026-11-06 by Melanikus for the AvP port:
//   only a host can restart a network game
// Modified 2026-11-09 by Melanikus for the AvP port:
//   the NEWDYNAMICCOLLISIONS command, which says whether a slide along a wall goes on at any frame rate (#180)
// Modified 2026-11-11 by Melanikus (Third) for the AvP port:
//   the FLOORISLAVA command, which says whether standard aliens climb walls and ceilings to reach you, from the next level's start (#109)
#include "3dc.h"
#include "console_command.hpp"
#include "build_identity.h"
#include "strutil.h"
#include "fmvCutscenes.h"
#include "view_switches.h"
#include "new_dynamic_collisions.h"
#include "floor_is_lava.h"
#include "logString.h"

extern void NewOnScreenMessage(char *messagePtr);
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "gameplat.h"
#include "bh_types.h"
#include "showcmds.h"
#include "version.h"
#include "equipment.h"
#include "cheat.h"
#include "dynblock.h"
#include "bh_rubberduck.h"
#include "pvisible.h"
#include "pldnet.h"
#include "lighting.h"
#include "paintball.h"		  
#include "decal.h"
#include "ConsoleLog.hpp"
#include "psndplat.h"
#include "menus.h"
#include "detaillevels.h"
#include "savegame.h"

int DebuggingCommandsActive=0;
extern void GimmeCharge(void);

// just change these to prototypes etc.
extern void QuickLoad()
{
	//set the load request
	LoadGameRequest = 0; //(that's slot 0 - not false)
}
extern void QuickSave()
{
	//set the save request
	SaveGameRequest = 0; //(that's slot 0 - not false)
}

void ConsoleCommandLoad(int slot)
{
	if(slot>=1 && slot<=NUMBER_OF_SAVE_SLOTS)
	{
		LoadGameRequest = slot-1;
	}
}

void ConsoleCommandSave(int slot)
{
	if(slot>=1 && slot<=NUMBER_OF_SAVE_SLOTS)
	{
		SaveGameRequest = slot-1;
	}
}
extern void DisplaySavesLeft();



struct DEBUGGINGTEXTOPTIONS ShowDebuggingText;

extern void ChangeNetGameType_Individual();
extern void ChangeNetGameType_Coop();
extern void ChangeNetGameType_LastManStanding();
extern void ChangeNetGameType_PredatorTag();
extern void ShowNearestPlayersName();
extern void ScreenShot(void);
extern void CastAlienBot(void);
extern void CastMarineBot(int weapon);
extern void CastPredoBot(int weapon);
extern void CastPredAlienBot(void);
extern void CastPraetorianBot(void);
extern void CastXenoborg(void);

extern int ShowMultiplayerScoreTimer;

static void ShowFPS(void)
{
	ShowDebuggingText.FPS = ~ShowDebuggingText.FPS;
}
static void ShowEnvironment(void)
{
	ShowDebuggingText.Environment = ~ShowDebuggingText.Environment;
}
static void ShowCoords(void)
{
	ShowDebuggingText.Coords = ~ShowDebuggingText.Coords;
}
static void ShowModule(void)
{
	ShowDebuggingText.Module = ~ShowDebuggingText.Module;
}
static void ShowTarget(void)
{
	ShowDebuggingText.Target = ~ShowDebuggingText.Target;
}
static void ShowNetworking(void)
{
	ShowDebuggingText.Networking = ~ShowDebuggingText.Networking;
}
static void ShowDynamics(void)
{
	ShowDebuggingText.Dynamics = ~ShowDebuggingText.Dynamics;
}
static void ShowGunPos(void)
{
	ShowDebuggingText.GunPos = ~ShowDebuggingText.GunPos;
}
static void ShowTears(void)
{
	ShowDebuggingText.Tears = ~ShowDebuggingText.Tears;
}
static void ShowPolyCount(void)
{
	ShowDebuggingText.PolyCount = ~ShowDebuggingText.PolyCount;
}
static void ShowSounds(void)
{
	ShowDebuggingText.Sounds = ~ShowDebuggingText.Sounds;
}


extern void ChangeToMarine();
static void ChangeToSpecialist_General()
{
	netGameData.myCharacterSubType=NGSCT_General;
	ChangeToMarine();
}
static void ChangeToSpecialist_PulseRifle()
{
	netGameData.myCharacterSubType=NGSCT_PulseRifle;
	ChangeToMarine();
}
static void ChangeToSpecialist_Smartgun()
{
	if(netGameData.allowSmartgun)
	{
		netGameData.myCharacterSubType=NGSCT_Smartgun;
	}
	ChangeToMarine();
}
static void ChangeToSpecialist_Flamer()
{
	if(netGameData.allowFlamer)
	{
		netGameData.myCharacterSubType=NGSCT_Flamer;
	}
	ChangeToMarine();
}
static void ChangeToSpecialist_Sadar()
{
	if(netGameData.allowSadar)
	{
		netGameData.myCharacterSubType=NGSCT_Sadar;
	}
	ChangeToMarine();
}
static void ChangeToSpecialist_GrenadeLauncher()
{
	if(netGameData.allowGrenadeLauncher)
	{
		netGameData.myCharacterSubType=NGSCT_GrenadeLauncher;
	}
	ChangeToMarine();
}
static void ChangeToSpecialist_Minigun()
{
	if(netGameData.allowMinigun)
	{
		netGameData.myCharacterSubType=NGSCT_Minigun;
	}
	ChangeToMarine();
}
static void ChangeToSpecialist_Frisbee()
{
	if(netGameData.allowSmartDisc)
	{
		netGameData.myCharacterSubType=NGSCT_Frisbee;
	}
	ChangeToMarine();
}

static void ChangeToSpecialist_Pistols()
{
	if(netGameData.allowPistols)
	{
		netGameData.myCharacterSubType=NGSCT_Pistols;
	}
	ChangeToMarine();
}

extern void ShowMultiplayerScores()
{
	ShowMultiplayerScoreTimer=5*ONE_FIXED;
}

extern void AddNetMsg_ChatBroadcast(char *string,BOOL same_species_only);

static void DoMultiplayerSay(char* string)
{
	AddNetMsg_ChatBroadcast(string,FALSE);
}

static void DoMultiplayerSaySpecies(char* string)
{
	AddNetMsg_ChatBroadcast(string,TRUE);
}

static void ForceAssertionFailure(void)
{
	LOCALASSERT("This assertion has been forced to stop the game"==0);
}


		
static void CDCommand_Play(int track)
{
/* FIXME
	if(!CDDA_IsOn()) CDDA_SwitchOn();

	CDDA_Stop();
	CDDA_Play(track);
*/
}
void CDCommand_PlayLoop(int track)
{
/* FIXME
	if(!CDDA_IsOn()) CDDA_SwitchOn();

	CDDA_Stop();
	CDDA_PlayLoop(track);
*/
}

static void CDCommand_Stop(void)
{
/* FIXME
	CDDA_Stop();
*/
}

static void CDCommand_Volume(int volume)
{
/* FIXME
	if (volume>=0 && volume<=127)
	{
		CDDA_ChangeVolume(volume);
	}
	else
	{
		// say the volume setting is incorrect
	}
*/
}


static void GunX(int x)
{
	PLAYER_STATUS *playerStatusPtr= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);
	PLAYER_WEAPON_DATA *weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
	TEMPLATE_WEAPON_DATA *twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];

	twPtr->RestPosition.vx = x;
}
static void GunY(int y)
{
	PLAYER_STATUS *playerStatusPtr= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);
	PLAYER_WEAPON_DATA *weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
	TEMPLATE_WEAPON_DATA *twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];

	twPtr->RestPosition.vy = y;
}
static void GunZ(int z)
{
	PLAYER_STATUS *playerStatusPtr= (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);
	PLAYER_WEAPON_DATA *weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
	TEMPLATE_WEAPON_DATA *twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];

	twPtr->RestPosition.vz = z;
}

static void MakeRotatingLight(void)
{
	MakeLightElement(&Player->ObWorld,LIGHTELEMENT_ROTATING);
}

static void RestartMultiplayer(void)
{
	/* obviously have to be in a network game... */
	if (AvP.Network==I_No_Network) return;

	// Only the host can restart the game for everyone: nobody would believe it from anyone else
	if (AvP.Network != I_Host) return;

	int seed=FastRandom();
	AddNetMsg_RestartNetworkGame(seed);
	RestartNetworkGame(seed);
}

static void CompleteLevel(void)
{
	AvP.LevelCompleted = 1;
}

// NEWDYNAMICCOLLISIONS, the console command: see new_dynamic_collisions.h. It's a player's setting, not a cheat, and it's there in a
// network game too. It only changes how the things this machine moves meet walls: its own player, and on a host the NPCs and objects
// it runs. Off only makes them snag more, and on, the fair way, is where every run starts. It isn't kept in the settings.
static void NewDynamicCollisionsCommand(char *words)
{
	char const *said = NewDynamicCollisions_FromWords(words, &NewDynamicCollisionsOn) ? NewDynamicCollisions_Words(NewDynamicCollisionsOn)
		: kNewDynamicCollisionsRefused;

	static char message[100];
	_snprintf(message, sizeof(message) - 1, "%s", said);
	message[sizeof(message) - 1] = 0;

	// There's no HUD to show it on until a level has started.
	if (AvP.MainLoopRunning)
	{
		NewOnScreenMessage(message);
	}

	LogString(message);
}

// FLOORISLAVA, the console command: see floor_is_lava.h. A player's setting, not a cheat, and there in a network game too, where only the
// host's counts, as only the host moves the aliens. It counts from the next level's start, as the aliens' map is built then, so in a level
// the screen says so too. It isn't kept in the settings. (Added 2026-11-11 by Melanikus (Third) for the AvP port, #109.)
static void FloorIsLavaCommand(char *words)
{
	bool const took = FloorIsLava_FromWords(words, &FloorIsLavaAsked);
	char const *said = took ? FloorIsLava_Words(FloorIsLavaAsked) : kFloorIsLavaRefused;

	static char message[100];
	_snprintf(message, sizeof(message) - 1, "%s", said);
	message[sizeof(message) - 1] = 0;

	// There's no HUD to show it on until a level has started
	if (AvP.MainLoopRunning)
	{
		NewOnScreenMessage(message);

		if (took)
		{
			static char fromNext[100];
			_snprintf(fromNext, sizeof(fromNext) - 1, "%s", kFloorIsLavaFromTheNextLevel);
			fromNext[sizeof(fromNext) - 1] = 0;
			NewOnScreenMessage(fromNext);
		}
	}

	LogString(message);
}

void CreateGameSpecificConsoleCommands(void)
{
	ShowDebuggingText.FPS = 0;
	ShowDebuggingText.Environment = 0;
	ShowDebuggingText.Coords = 0;
	ShowDebuggingText.Module = 0;
	ShowDebuggingText.Target = 0;
	ShowDebuggingText.Networking = 0;
	ShowDebuggingText.Dynamics = 0;
	ShowDebuggingText.GunPos = 0;
	ShowDebuggingText.Tears = 0;
	ShowDebuggingText.PolyCount = 0;

	// Player settings, not cheats or developer tools, so anyone can use them, or put them on a key: BIND L LIGHTING. See view_switches.h.
	ConsoleCommand::Make
	(
		"LIGHTING",
		"HOW THE WORLD IS LIT: 1 GOURAUD, 2 PERPIXEL, 3 NORMALS, 4 RELIEF. ON ITS OWN, THE NEXT ONE",
		LightingCommand
	);

	ConsoleCommand::Make
	(
		"SHADOWS",
		"SHADOWS: 1 ON, 0 OFF. ON ITS OWN, SWITCHES THEM",
		ShadowsCommand
	);

	ConsoleCommand::Make
	(
		"LAMPFOLLOW",
		"HOW MANY SECONDS THE SHOULDER LAMP TAKES TO CATCH UP WITH THE VIEW, FROM 0 TO 5. 0 STICKS IT TO THE VIEW. ON ITS OWN, SAYS WHAT IT IS",
		LampFollowCommand
	);

	ConsoleCommand::Make
	(
		"LAMPSPOTSIZE",
		"HOW BIG THE SHOULDER LAMP'S SPOT IS, FROM 0.05 TO 0.35. 0.21 IS AS BUILT. NOT IN A NETWORK GAME",
		LampSpotSizeCommand
	);

	ConsoleCommand::Make
	(
		"LAMPINTENSITY",
		"HOW STRONG THE SHOULDER LAMP'S SPOT IS, FROM 0.25 TO 2. 1 IS AS BUILT. NOT IN A NETWORK GAME",
		LampIntensityCommand
	);

	ConsoleCommand::Make
	(
		"LAMPLOOK",
		"HOW THE SHOULDER LAMP LOOKS AS A REAL LIGHT: 0 THE REAL LIGHT, 1 THE REAL LIGHT AND THE PAINTED SPOT (AS BUILT), 2 PAINTED ONLY, 3 THE REAL LIGHT AND THE PAINTED LAMP AT FULL. NOT IN A NETWORK GAME",
		LampLookCommand
	);

	ConsoleCommand::Make
	(
		"SHADOWAMBIENT",
		"WHETHER THE MAP'S LIGHTS THAT SHOW NO CORONA CAST SHADOWS: ON OR OFF. ON ITS OWN, SWITCHES IT",
		ShadowAmbientCommand
	);

	// A player's setting too, there for everyone and on by default, so that a player can switch it off to compare. See new_dynamic_collisions.h.
	ConsoleCommand::Make
	(
		"NEWDYNAMICCOLLISIONS",
		"WHETHER A SLIDE ALONG A WALL GOES ON AT ANY FRAME RATE: ON OR OFF. OFF MEETS WALLS AS IN 1999. ON ITS OWN, SWITCHES IT",
		NewDynamicCollisionsCommand
	);

	// A player's setting too, there for everyone, so that a player can put 1999's aliens back to compare. See floor_is_lava.h. (#109)
	ConsoleCommand::Make
	(
		"FLOORISLAVA",
		"WHETHER ALIENS CLIMB WALLS AND CEILINGS TO REACH YOU: ON OR OFF. OFF MOVES THEM AS IN 1999. ON ITS OWN, SWITCHES IT. FROM THE NEXT LEVEL",
		FloorIsLavaCommand
	);

	// Only do anything with -dev, and they say so without it. See view_switches.h.
	ConsoleCommand::Make("RELIEFFLIP", "SWAPS WHICH OF BLACK AND WHITE IS THE POLYGON'S LEVEL IN A DEPTH MAP, LIVE", ReliefFlipCommand);
	ConsoleCommand::Make("TILTFLIP", "SWAPS BUMPS AND DENTS IN A NORMAL MAP, LIVE", TiltFlipCommand);
	ConsoleCommand::Make("SHADOWEVENGAP", "WHETHER A SHADOW STARTS THE SAME DISTANCE BEHIND EVERY PART OF ITS POLYGON: ON OR OFF. ON ITS OWN, SWITCHES IT", ShadowEvenGapCommand);
	ConsoleCommand::Make("RELIEFDEPTH", "HOW DEEP A DEPTH MAP GOES, AS A FRACTION OF A PICTURE'S WIDTH, FROM 0 TO 0.2", ReliefDepthCommand);
	ConsoleCommand::Make("PIXELGLOW", "HOW MUCH OF A LAMP'S LIGHT GOES ON TOP WITH NO TEXTURE, FROM 0 TO 1. 0.25 IS AS BUILT", PixelGlowCommand);
	ConsoleCommand::Make("PIXELCURVE", "HOW DEEP A LAMP'S DIM TONES GO PER PIXEL, FROM 0.25 TO 4. 1 IS AS BUILT, AND HIGHER IS DARKER", PixelCurveCommand);
	ConsoleCommand::Make("PIXELLEVEL", "HOW STRONG A LAMP'S LIGHT LOOKS PER PIXEL, FROM 0 TO 2. 1 IS AS BUILT", PixelLevelCommand);

	#ifndef AVP_DEBUG_VERSION 
	bool IsACheat = true;
	#else
	bool IsACheat = false;
	#endif
	
	// In a beta these are there for testers, without DebuggingCommandsActive, which would also stop saving, statistics and the next
	// level's unlock: see beta_build.h. The 11 commands.
	#ifndef AVP_DEBUG_VERSION // allow debug commands without -debug
	#ifndef AVP_DEBUG_FOR_FOX // allow debug commands without -debug
	if (DebuggingCommandsActive || BuildIdentity_IsABeta())
	#endif
	#endif
	{
		ConsoleCommand::Make
		(
			"GIVEALLWEAPONS",
			"BE CAREFUL WHAT YOU WISH FOR",
			GiveAllWeaponsCheat,
			IsACheat
		);

		/* KJL 14:51:09 29/03/98 - show commands */
		ConsoleCommand::Make
		(
			"SHOWFPS",
			"DISPLAY THE FRAMERATE",
			ShowFPS,
			IsACheat
		);

		ConsoleCommand::Make
		(
			"SHOWPOLYCOUNT",
			"DISPLAY NUMBER OF LANDSCAPE POLYS, AND NUMBER OF POLYS ACTUALLY RENDERED",
			ShowPolyCount,
			IsACheat
		);
		ConsoleCommand::Make
		(
			"LIGHT",
			"CREATE A LIGHT",
			MakeRotatingLight,
			IsACheat
		);
		ConsoleCommand :: Make
		(
			"GIMME_CHARGE",
			"GRANTS FULL FIELD CHARGE",
			GimmeCharge,
			IsACheat
		);
		ConsoleCommand :: Make
		(
			"ALIENBOT",
			"CREATES ALIEN BOT. SAME AS ANY OTHER ALIEN.",
			CastAlienBot,
			IsACheat
		);

		ConsoleCommand :: Make
		(
			"MARINEBOT",
			"CREATES MARINE BOT. SAME AS A GENERATED MARINE.",
			CastMarineBot,
			IsACheat
		);

		ConsoleCommand :: Make
		(
			"PREDOBOT",
			"CREATES PREDATOR BOT.",
			CastPredoBot,
			IsACheat
		);

		ConsoleCommand :: Make
		(
			"PREDALIENBOT",
			"CREATES PREDATOR ALIEN BOT.",
			CastPredAlienBot,
			IsACheat
		);

		ConsoleCommand :: Make
		(
			"PRAETORIANBOT",
			"CREATES PRAETORIAN GUARD BOT.",
			CastPraetorianBot,
			IsACheat
		);

		ConsoleCommand :: Make
		(
			"XENOBORG",
			"THEY'RE ALL BOTS ANYWAY...",
			CastXenoborg,
			IsACheat
		);


	}

	#if CONSOLE_DEBUGGING_COMMANDS_ACTIVATED
	ConsoleCommand::Make
	(
		"SHOWENV",
		"DISPLAY THE ENVIRONMENT NAME",
		ShowEnvironment
	);

	ConsoleCommand::Make
	(
		"SHOWCOORDS",
		"DISPLAY THE PLAYERS CURRENT POSITION",
		ShowCoords
	);

	ConsoleCommand::Make
	(
		"SHOWMODULE",
		"DISPLAY THE PLAYERS CURRENT MODULE",
		ShowModule
	);

	ConsoleCommand::Make
	(
		"SHOWTARGET",
		"DISPLAY THE CURRENT TARGET POSITION",
		ShowTarget
	);

	ConsoleCommand::Make
	(
		"SHOWNETWORKING",
		"DISPLAY NETWORKING DEBUG TEXT",
		ShowNetworking
	);
	
	ConsoleCommand::Make
	(
		"SHOWDYNAMICS",
		"DISPLAY DYNAMICS DEBUG TEXT",
		ShowDynamics
	);

	ConsoleCommand::Make
	(
		"SHOWGUNPOS",
		"DISPLAY GUN OFFSET COORDS",
		ShowGunPos
	);

	ConsoleCommand::Make
	(
		"SHOWTEARS",
		"MAKE TEARS AND LINKING ERRORS APPEAR BRIGHT GREEN",
		ShowTears
	);

	ConsoleCommand::Make
	(
		"SHOWSOUNDS",
		"DISPLAY NUMBER OF ACTIVE SOUNDS",
		ShowSounds
	);


	#if 1
	ConsoleCommand::Make
	(
		"GUNX",
		"CHANGE POSITION",
		GunX
	);
	ConsoleCommand::Make
	(
		"GUNY",
		"CHANGE POSITION",
		GunY
	);
	ConsoleCommand::Make
	(
		"GUNZ",
		"CHANGE POSITION",
		GunZ
	);
	ConsoleCommand::Make
	(
		"DUCKBOT",
		"MAKE A RUBBER DUCK",
		CreateRubberDuckBot
	);

	ConsoleCommand::Make
	(
		"FORCEASSERTIONFAILURE",
		"MAKE AN ASSERTION FIRE, EXITING THE GAME",
		ForceAssertionFailure
	);
	#endif

	ConsoleCommand::Make
	(
		"RESTARTMULTIPLAYER",
		"RESTARTS A NETWORK GAME FROM SCRATCH",
		RestartMultiplayer
	);
	ConsoleCommand::Make
	(
		"PAINTBALL",
		"TOGGLES PAINTBALLMODE ON/OFF",
		TogglePaintBallMode
	);

	ConsoleCommand::Make
	(
		"BUG",
		"ADD A BUG REPORT TO CONSOLELOG.TXT",
		OutputBugReportToConsoleLogfile
	);
	ConsoleCommand::Make
	(
		"REMOVEDECALS",
		"DELETES ALL PRE-DECALS",
		RemoveAllFixedDecals
	);
	
	ConsoleCommand::Make
	(
		"TURN3DSOUNDHARDWAREOFF",
		"DEACTIVATES 3D SOUND IN HARDWARE",
		PlatDontUse3DSoundHW
	);
	ConsoleCommand::Make
	(
		"TURN3DSOUNDHARDWAREON",
		"ACTIVATES 3D SOUND IN HARDWARE",
		PlatUse3DSoundHW
	);

	ConsoleCommand::Make
	(
		"NETGAME_INDIVIDUAL",
		"CHANGE NETWORK GAME TYPE",
		ChangeNetGameType_Individual
	);
	ConsoleCommand::Make
	(
		"NETGAME_COOP",
		"CHANGE NETWORK GAME TYPE",
		ChangeNetGameType_Coop
	);
	ConsoleCommand::Make
	(
		"NETGAME_LASTMANSTANDING",
		"CHANGE NETWORK GAME TYPE",
		ChangeNetGameType_LastManStanding
	);
	ConsoleCommand::Make
	(
		"NETGAME_PREDATORTAG",
		"CHANGE NETWORK GAME TYPE",
		ChangeNetGameType_PredatorTag
	);


	ConsoleCommand::Make
	(
		"TRIGGER_PLOT_FMV",
		"",
		StartTriggerPlotFMV
	);

	
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_GENERAL",
		"Become a general marine (can use all weapons)",
		ChangeToSpecialist_General
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_PULSERIFLE",
		"Become a pulserifle marine",
		ChangeToSpecialist_PulseRifle
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_SMARTGUN",
		"Become a smartgun marine",
		ChangeToSpecialist_Smartgun
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_FLAMER",
		"Become a flamethrower marine",
		ChangeToSpecialist_Flamer
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_SADAR",
		"Become a sadar marine",
		ChangeToSpecialist_Sadar
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_GRENADELAUNCHER",
		"Become a grenade launcher marine",
		ChangeToSpecialist_GrenadeLauncher
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_MINIGUN",
		"Become a minigun marine",
		ChangeToSpecialist_Minigun
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_SD",
		"Become an SD marine",
		ChangeToSpecialist_Frisbee
	);
	ConsoleCommand::Make
	(
		"SPECIALISTMARINE_PISTOLS",
		"Become a pistol marine",
		ChangeToSpecialist_Pistols
	);
	ConsoleCommand::Make
	(
		"COMPLETE_LEVEL",
		"",
		CompleteLevel
	);
	#endif
	/* KJL 15:52:41 29/03/98 - version info */
	ConsoleCommand::Make
	(
		"VERSION",
		"",
		GiveVersionDetails
	);

	ConsoleCommand::Make
	(
		"SAY",
		"BROADCAST MESSAGE",
		DoMultiplayerSay
	);

	ConsoleCommand::Make
	(
		"SAY_SPECIES",
		"BROADCAST MESSAGE",
		DoMultiplayerSaySpecies
	);
 	ConsoleCommand::Make
	(
		"CDSTOP",
		"STOP THE CD PLAYING",
		CDCommand_Stop
	);

	ConsoleCommand::Make
	(
		"CDPLAY",
		"SELECT A TRACK TO PLAY",
		CDCommand_Play
	);
	ConsoleCommand::Make
	(
		"CDPLAYLOOP",
		"SELECT A TRACK TO PLAY LOOPED",
		CDCommand_PlayLoop
	);
	
	ConsoleCommand::Make
	(
		"CDVOLUME",
		"SELECT SOUND LEVEL 0 TO 127",
		CDCommand_Volume
	);
	ConsoleCommand::Make
	(
		"ID_PLAYER",
		"Get name of player nearest centre of screen",
		ShowNearestPlayersName
	);
	ConsoleCommand::Make
	(
		"SHOW_SCORE",
		"Show frag table",
		ShowMultiplayerScores
	);

	ConsoleCommand::Make
	(
		"DETAIL_LEVEL_MAX",
		"",
		SetToDefaultDetailLevels
	);
	
	ConsoleCommand::Make
	(
		"DETAIL_LEVEL_MIN",
		"",
		SetToMinimalDetailLevels
	);

	ConsoleCommand::Make
	(
		"SCREENSHOT",
		"",
		ScreenShot
	);
	
	ConsoleCommand::Make
	(
		"QUICKSAVE",
		"",
		QuickSave
	);
	ConsoleCommand::Make
	(
		"QUICKLOAD",
		"",
		QuickLoad
	);
	ConsoleCommand::Make
	(
		"SAVE",
		"Save game to slot 1-8",
		ConsoleCommandSave
	);
	ConsoleCommand::Make
	(
		"LOAD",
		"Load game from slot 1-8",
		ConsoleCommandLoad
	);
	ConsoleCommand::Make
	(
		"SAVESLEFT",
		"",
		DisplaySavesLeft
	);
}
