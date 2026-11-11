// AvP port by Barry Duncan.
// Modified 2026-11-05 by Melanikus for the AvP port:
//   the cheat menu's Start only starts what its sliders are allowed to say
// Modified 2026-11-03 by Melanikus for the AvP port:
//   menu texts (the multiplayer setup, a menu number, the load-game list, the joined LAN game's name and the
//   custom level name) are copied or written with size limits; a save slot whose file path does not fit is
//   refused; the credits roll stops at the end of credits.txt; a typed letter's width is looked up with an unsigned
//   byte
// Modified 2026-11-06 by Melanikus for the AvP port:
//   going on to the next level of a campaign skips the menus
//   leaving the page of games found closes what the search has open on the network
//   a client whose host has gone goes back to the main menus
//   the class slider shows the new classes' names, and a host or joiner can start as one
// Modified 2026-11-05 by Melanikus for the AvP port:
//   the rendering page's lines for choosing a key, which can refuse one, and the controls pages show a key that's also on that page
//   a client whose host has gone goes back to the main menus
//   a network game's loading screen has all five of its lines, where some could be left unset
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a save slot whose species, episode or difficulty is not one the game can write looks like an empty slot.
//   the log says when a save slot is listed as empty because its numbers are wrong
//   the fading of a scrolling menu's rows has room for any number of rows, however many profiles a player has;
//   a profile can't give the difficulty menu more rows than it has, and a profile's or save's date text stays inside its box
// Modified 2026-11-06 by Melanikus for the AvP port:
//   choosing a campaign's level on the level row starts that campaign, with its own briefing
//   a level's films play only when the game plays as single player does, and the network goes on underneath them
//   the host chooses a campaign's difficulty, and a joiner plays what the host says rather than his last single player game's
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a co-op campaign's pause menu has a Players page, and asks a host before leaving; the page after a co-op game says why it ended
//   the single player Custom maps page: its list, its picture and its Start row. A finished custom map goes back to that page, and
//   plays no stock level's film and marks no stock level done. Credit: Melanikus and Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten)
//   the co-op campaign pages: see coop_menu.h. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten)
//   a marine's class comes from the class slider through ClassSlider_ToClass, the same sum as before, in the one place its test checks
//   the single player Custom maps page: its list, its picture and its Start row. A finished custom map goes back to that page, and
//   plays no stock level's film and marks no stock level done. Credit: Melanikus and Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten)
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the join page's address box shows a dot for each character unless Tab is held, and its help line says so (address_box.h)
// Modified 2026-11-04 by Melanikus for the AvP port:
//   with -dev -hudson, a run skips the menus and starts its marine level
// Modified 2026-11-05 by Melanikus for the AvP port:
//   a run loads and lets go of the menu pictures as the menus do, so that it plays with what a player has loaded
//   a run can load any built-in level, by the name on the game's own list
// Modified 2026-11-06 by Melanikus for the AvP port:
//   a run can ask for a level's own films
//   a gun= run with class= starts a skirmish, and a gun= run with species=predator plays as a Predator
//   a run with net= starts as a host or a client on this machine, without the multiplayer pages
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the Room echo row on the sound pages is saved when it changes, the same way the rendering page's rows are (REVERB-TOGGLE-1)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the multiplayer page asks the relay server's list where its relays are, and the page of games found asks it for its games as well
//   as the LAN (relay_list.h)
// Modified 2026-11-09 by Melanikus for the AvP port:
//   the page of games found is the online-play screens' (online_screens_page.h): a table of each game's name, level, players, region and
//   ping, kept fresh every 5 s with the selection on the same game, and the spinning tail while it looks. A join is found by its game, not
//   its row. The join's wait through a relay shows its own screen, and -dev -onlinescreens shows every screen for pictures.
//   the Online access pages (server #24, access_page.h), and the Multiplayer page's row to them, only with -dev
//   a host asked to let someone in through a relay sees the check code under the asking line (CRYPTO.md section 8)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the credits roll shows with the menu's video on, and any key pressed stops it, whatever else is held (CREDITS-1). The fix follows
//   Bobblen's avp-forever (ec6446f), rewritten
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "pldnet.h"
#include "Input.h"
#include "menus.h"
#include "marine_classes.h"
#include "view_settings.h"
#include "env_info.h"
#include "custom_maps.h"
#include "coop_menu.h"
#include "level_lists.h"
#include "io.h"
#include "hud_Layout.h"
#include "user_profile.h"
#include "huffman.hpp"
#include "FmvCutscenes.h"
#include "hud_gfx.h"
#include "user_io.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "iofocus.h"
#include <time.h>
#include "gammacontrol.h"
#include "mp_config.h"
#include "menu_data.h"
#include "psnd.h"
#include "savegame.h"
#include "save_checks.h"
#include "networking.h"
#include "utilities.h"
#include "tables.h"
#include "d3d_render.h"
#include "OnScreenKeyboard.h"
#include "logString.h"
#include "hudson.h"
#include "hudson_plan.h"
#include <vector>
#include "profile_checks.h"
#include "address_box.h"
#include "relay_list.h"
#include "relay_auto.h"
#include "online_screens.h"
#include "online_screens_page.h"
#include "access_page.h"
#include "server_yes_page.h"
#include "servers_page.h"      // every servers.cfg line, and the player's pick (#175 2b)
#include "host_wait_rules.h"   // a host's wait for the server, and a failed host's line (PO's rulings, 2026-11-10)
#include "access_page_rules.h"   // the Online access pages' rows: how far apart, and how bright when not chosen

#include "credits_roll.h"

#if defined(_MSC_VER)
#define stricmp		_stricmp
#endif

/* KJL 11:22:37 23/06/98 - Hopefully these will be the final menus! */

// extern functions
void ChangeWindowsSize(uint32_t width, uint32_t height);
extern void Reverb_MenuFrame();   // openal_audio.cpp: saves the Room echo row when it has changed
extern bool IDemandSelect();
extern void MakeSelectSessionMenu(void);
extern void MakeInGameMenu(void);
extern void MakeMarineKeyConfigMenu(void);
extern void MakePredatorKeyConfigMenu(void);
extern void MakeAlienKeyConfigMenu(void);
extern void MakeUserProfileSelectMenu(void);
extern void SaveKeyConfiguration(void);
extern void PlayIntroSequence(void);
extern void MinimalNetCollectMessages(void);
extern void ShowSplashScreens(void);
extern void Show_WinnerScreen(void);
extern void AvP_UpdateMenus(void);
extern void GetNextAllowedSpecies(int* species,BOOL search_forwards);
static void SetBriefingTextForMultiplayer();
static void StartTheCoopCampaign(void);
void SetBriefingTextToBlank(void);
extern void SelectMenuDisplayMode(void);
extern void DrawMainMenusBackdrop(void);
extern BOOL GetFilenameForSaveSlot(int i, char *filenamePtr);
extern void EndMenuBackgroundFmv(void);
extern void D3D_DrawColourBar(int yTop, int yBottom, int rScale, int gScale, int bScale);
extern int AnyCheatModesAllowed(void);
extern int CheatMode_SelectionIsAllowed(void);
extern void MakeConnectionSelectMenu();
extern void RenderHighlightRectangle(int x1, int y1, int x2, int y2, int r, int g, int b);
extern void CheatMode_GetNextAllowedSpecies(int *speciesPtr, int searchForward);
extern void CheatMode_GetNextAllowedMode(int *cheatModePtr, int searchForward);
extern void CheatMode_GetNextAllowedEnvironment(int *environmentPtr, int searchForward);
extern void RenderKeyConfigRectangle(int alpha);
extern void IngameKeyboardInput_ClearBuffer(void);
extern int LengthOfMenuText(const char *textPtr);
extern int HeightOfMenuGfx(texID_t textureID);
extern int PlayMenuBackgroundFmv();
extern char *GetGamePadButtonTextString(enum TEXTSTRING_ID stringID);

// extern variables
extern char AAFontWidths[256];
extern char MP_Config_Name[];
extern char LevelName[];
extern char IP_Address_Name[22];
extern char IPAddressString[];
extern char CommandLineIPAddressString[];
extern AVPMENU_ELEMENT* AvPMenu_Multiplayer_LoadConfig;
extern AVPMENU_ELEMENT* AvPMenu_Multiplayer_LoadIPAddress;
extern void MakeOpenIPAddressMenu();
extern int AutoWeaponChangeOn_Temp;
extern int AutoWeaponChangeOn;
extern void SetDefaultMultiplayerConfig();

void HandlePostGameFMVs(void);
void HandlePreGameFMVs(void);

static void SetupNewMenu(enum AVPMENU_ID menuID);

// The relay server's games, for the page of games found: the last good answer, kept here so that they can be put back each time the page
// is set up again. The page's list is emptied whenever a new search of the LAN starts, and these would go with it.
static RelayListGame relayGamesFound[kRelayListMostGames];
static int relayGamesKept = 0;

// Puts the kept relay games on the page's list, after whatever the LAN found. Net_AddRelaySession leaves out a game that's there already,
// one this machine can't play, and any once the list is full, so this can be called each time the page is set up.
static void PutTheRelayGamesOnThePage(void)
{
	for (int i = 0; i < relayGamesKept; i++)
	{
		(void)Net_AddRelaySession(relayGamesFound[i]);
	}
}

// Each frame of the page of games found: whether an answer has come from the relay server's list with games to show. An answer that
// failed is written in the log, in the game's own words, never the server's.
static bool TheRelayGamesHaveCome(void)
{
	int count = 0;
	const char *why = NULL;

	if (!RelayList_FoundGames(relayGamesFound, &count, &why))
	{
		return false;
	}

	relayGamesKept = why ? 0 : count;

	if (why)
	{
		LogString(std::string("Relay list: no games from the list, because ") + why);
	}

	// The page's ping column needs each game's hostMs, and it says why the list failed
	OnlinePage_RelayGamesCame(relayGamesFound, relayGamesKept, why);

	return relayGamesKept > 0;
}

// A join through a relay waits up to about 26 seconds for the relay's handshake, and the networking code asks this each 50 ms of it
// (Net_SetRelayJoinWaitHook). It keeps the window answering Windows, so the game isn't marked "Not responding", and lets the player give up
// with Esc, or by closing the game. False gives the join up. Each time, it draws the joining screen (J3), so the tail turns and the screen
// says which step the join is at, and the game's region.
static char joiningRegion[kRelayListRegionLetters + 1];

static bool KeepTheWindowAliveWhileJoining(void)
{
	extern bool bRunning;
	extern unsigned char KeyboardInput[];

	OnlinePage_DrawJoining(joiningRegion);
	CheckForWindowsMessages();
	DirectReadKeyboard();
	return bRunning && !KeyboardInput[KEY_ESCAPE];
}

static void CountTheRowsOnThisPage(void);
static void RenderMenu(void);
static void RenderBriefingScreenInfo(void);
static void RenderEpisodeSelectMenu(void);
static void RenderCustomMapPicture(void);

// How far down a page a picture starts: below the title, a few pixels clear of the tails of letters like "p"
static const int kMenuPictureTop = 106;

extern int CustomMapToPlay;
extern int CustomMapSpecies;
extern int CustomMapDifficulty;
extern texID_t MISSING_TEXTURE; // d3_func.cpp: what a picture that can't be read gives back

// The names the Custom maps page's map row shows, as the row wants them: the list's own names, or one line saying there are none
static std::vector<char *> customMapRowNames;
static void RenderKeyConfigurationMenu(void);
static void RenderUserProfileSelectMenu(void);
static void RenderLoadGameMenu(void);
static void RenderConfigurationDescriptionString();
static void ActUponUsersInput(void);
static void InteractWithMenuElement(enum AVPMENU_ELEMENT_INTERACTION_ID interactionID);
static void RenderMenuElement(AVPMENU_ELEMENT *elementPtr, int e, int y);
void DisplayVideoModeUnavailableScreen(void);
void CheckForCredits(void);
void DoCredits(void);
BOOL RollCreditsText(int position, char *textPtr);
static void TestValidityOfCheatMenu(void);
void SetBriefingTextForEpisode(int episode, I_PLAYER_TYPE playerID);
void SetBriefingTextToBlank(void);
void RenderBriefingText(int centreY, int brightness);
void CheckForKeysWithMultipleAssignments(void);
void HandleCheatModeFeatures(void);
void ShowMenuFrameRate(void);
static void KeyboardEntryQueue_Clear(void);
static void KeyboardEntryQueue_StartProcessing(void);
void ScanSaveSlots(void);
static void GetHeaderInfoForSaveSlot(SAVE_SLOT_HEADER* save_slot, const char* filename);
static void PasteFromClipboard(char* Text,int MaxTextLength);
void LoadDefaultPrimaryConfigs(void);
void TimeStampedMessage(char *stringPtr);
void ThisFramesRenderingHasFinished(void);
void PlayMenuMusic(void);
void ThisFramesRenderingHasFinished(void);
void EndMenuMusic(void);
void PlayFMV(const char *filenamePtr);
extern bool (*WhileAFilmPlays)(void);
int NumberOfAvailableLevels(I_PLAYER_TYPE playerID);
int LevelMostLikelyToPlay(I_PLAYER_TYPE playerID);
void Net_EnumConnections();
int MaxDifficultyLevelAllowed(I_PLAYER_TYPE playerID, int level);
static int HeightOfMenuElement(AVPMENU_ELEMENT *elementPtr);
char *GetVideoModeDescription2();
char *GetVideoModeDescription3();
void NextVideoMode2();
void PreviousVideoMode2();
void InitAVPNetGameForJoin(void);
void SetAndSaveDeviceAndVideoModePreferences();


/* KJL 11:23:03 23/06/98 - Requirements

	1.      'Floating' over backdrop, possibly using translucency effects
	2.      Quick to write!
	3.      Quick to change
	4.      Certain menus need to be used in-game - e.g. key configuration
*/

/* KJL 11:27:18 23/06/98 - Available menus

	1.      Main Menu
		
		Start Singleplayer
		Start Multiplayer 
		Gameplay Options
		Audio/Visual Options
		Exit

	2.      Singleplayer
		
		Alien
		Marine
		Predator
		Exit

	3.      Multiplayer

		-> Complex

	4.      Gameplay Options

	5.      Audio/Visual Options
		
 */


/* KJL 11:57:21 23/06/98 -


	Need mouse position code (?) etc. In-game menus using mouse? Ugh.
	No mouse then.

*/
SAVE_SLOT_HEADER SaveGameSlot[NUMBER_OF_SAVE_SLOTS];


extern int VideoModeNotAvailable;

extern unsigned char KeyboardInput[];
extern unsigned char DebouncedKeyboardInput[];
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern int TimeScale;

static AVP_MENUS AvPMenus;
extern AVPMENU  AvPMenusData[];

extern int AlienEpisodeToPlay;
extern int MarineEpisodeToPlay;
extern int PredatorEpisodeToPlay;
extern int UserProfileNumber;
AVP_USER_PROFILE *UserProfilePtr;

SESSION_DESC SessionData[MAX_NO_OF_SESSIONS];
uint32_t NumberOfSessionsFound;
extern NETGAME_GAMEDATA netGameData;

static int InputIsDebounced = 0;
static int KeyDepressedCounter = 0;
static int EpisodeSelectScrollOffset;
static int MaximumSelectableLevel;
static int KeyConfigSelectionColumn;

#ifdef _WIN32
// When a profile was last used or a game saved, written the way the player's own settings write dates and times: see ProfileChecks_MakeTimeText.
static void MakeTimeText(const SYSTEMTIME &when, DWORD dateKind, char *text, size_t size)
{
	ProfileChecks_MakeTimeText(when, dateKind, NULL, NULL, text, size);
}
#endif

// How bright each row of a scrolling menu is, as it fades up or down. Only about four rows are on the screen at once, and a row only
// fades while it's on the screen, so two rows 16 apart can share a number and nobody can see it. That way any number of rows fits, and a
// player can have as many profiles as they like.
static int Brightness[16];

static int &BrightnessOfRow(int row)
{
	return Brightness[row & 15];
}

static unsigned char MultipleAssignments[2][32];

char *GetDescriptionOfKey(unsigned char key);
static int OkayToPlayNextEpisode(void);
static PLAYER_INPUT_CONFIGURATION PlayerInputPrimaryConfig;
static PLAYER_INPUT_CONFIGURATION PlayerInputSecondaryConfig;
CONTROL_METHODS PlayerControlMethods;
JOYSTICK_CONTROL_METHODS PlayerJoystickControlMethods;

static void RenderScrollyMenu();
static void RenderHelpString();
static void CheckForLoadGame();

static void UpdateMultiplayerConfigurationMenu();

static char KeyboardEntryQueue_ProcessCharacter(void);

static BOOL LaunchingMplayer = FALSE;

static int MultiplayerConfigurationIndex; //just used for the configuration deletion stuff
static const char* MultiplayerConfigurationName = 0; //ditto

extern int DebuggingCommandsActive;

extern bool bRunning;

bool mainMenu = true;

int GetAvPMenuState()
{
	return AvPMenus.MenusState;
}

const int FRAMES_PER_SECOND = 60;
const int SKIP_TICKS = 1000 / FRAMES_PER_SECOND;

// The next level of a campaign: everyone's still connected, so nobody is asked anything.
static void PlayFilmAfterCampaignLevel(int finishedLevel);

static int StartNextCampaignLevel(void)
{
	PlayFilmAfterCampaignLevel(netGameData.coopCampaignLevel - 1);
	Coop_LevelChangeHasStarted();
	CheatMode_Active = CHEATMODE_NONACTIVE;
	LoadGameRequest = SAVELOAD_REQUEST_NONE;
	AvP.LevelCompleted = 0;
	TimeScale = ONE_FIXED;
	SetLevelToLoadForCampaign();
	SetBriefingTextForMultiplayer();
	AvPMenus.MenusState = MENUSSTATE_STARTGAME;
	return 1;
}

// The Start row of the co-op campaign page
static void StartTheCoopCampaign(void)
{
	if (CoopMenu_HostStarts())
	{
		AvPMenus.MenusState = MENUSSTATE_STARTGAME;
		SetLevelToLoadForThisNetworkGame();
		SetBriefingTextForMultiplayer();
	}
}

static bool JoinThroughFindGamesForARun(void);   // findgames=yes, below with the other presses a player makes
static bool HostThroughTheRowsForARun(int level);   // hostrow=, below the host's wait it uses (added 2026-11-10 by Melanikus (Third))

// A run has nobody to choose anything, so this does what choosing a marine level from the menus does, with a new player's settings. The second time it's asked, the level has ended, and false ends the game.
// A run with net= is one of two games on this machine. This is what the multiplayer pages would do, with their answers already filled in. See AVPMENU_ELEMENT_STARTMPGAME and AVPMENU_ELEMENT_JOINMPGAME.
// Everything is on this machine's own address: see NetLoopbackOnly in networking.cpp. If it can't get as far as starting a level, the run ends here, and its result says how far it got.
// A run's words for a host that failed: what it was doing, then why, in the game's own fixed words (networking.h, Net_HostFailedWhy). Added
// 2026-11-10 by Melanikus for the AvP port (PO's ruling Q4): the four hosts only a run makes put the why in their log lines.
static std::string HostFailedForARun(const char *what)
{
	const char *const why = Net_HostFailedWhy();
	return why ? std::string(what) + ", because " + why : std::string(what);
}

static void StartNetworkGameForHudsonRun(void)
{
	extern int MultiplayerEpisodeForEnvironment(int environment);

	int const onTheList = MultiplayerEpisodeForEnvironment(Hudson_Environment());
	int level = onTheList >= 0 ? onTheList : 0;

	SetDefaultMultiplayerConfig();
	netGameData.gameType = NGT_Individual;
	netGameData.coopCampaign = kCampaignNone;
	netGameData.coopCampaignLevel = 0;

	// coop=yes: the campaign this level is on, from this level on, as a host's menu would choose
	if (Hudson_NetIsHost() && Hudson_NetIsCampaign())
	{
		netGameData.coopDifficulty = Hudson_Difficulty();
		Campaign_ChooseRollNumber();
		Campaign_FindEnvironment(Hudson_Environment(), &netGameData.coopCampaign, &netGameData.coopCampaignLevel);

		// A siege can be on any of the game's levels, and every kind of NPC only reaches another machine in a game that plays like a campaign. This is the level SetLevelToLoadForCampaign loads instead.
		if (Hudson_IsASiege() && netGameData.coopCampaign == kCampaignNone)
		{
			netGameData.coopCampaign = kCampaignMarine;
			netGameData.coopCampaignLevel = 0;
		}

		netGameData.gameType = NGT_Coop;
		netGameData.disableFriendlyFire = Hudson_NoFriendlyFire();

		// The game's level number is the campaign level's place on the co-op list, as the host's own menu gives it (coop_menu.cpp), not
		// the deathmatch list's, which has no campaign level: derelict came out as -1, then 0, and the games page named it "Stranded" (the
		// showcase of 2026-11-10, Secondary's trace). A campaign with no such level (-1) keeps the number above. Modified 2026-11-10 by
		// Melanikus (Third) for the AvP port.
		int const coopPlace = LevelList_PlaceOf(MAX_NO_OF_COOPERATIVE_EPISODES, netGameData.coopCampaign, netGameData.coopCampaignLevel);

		if (coopPlace >= 0)
		{
			level = coopPlace;
		}
	}

	// hostile=yes and no campaign: a species game, the only kind that keeps the species scores one of the bad messages aims at
	if (Hudson_NetIsHost() && Hudson_NetIsHostile() && !Hudson_NetIsCampaign())
	{
		netGameData.gameType = NGT_CoopDeathmatch;
		LogString("hostile: this is a species game");
	}

	netGameData.levelNumber = level;
	MP_Species = 0;
	strcpy(MP_PlayerName, Hudson_NetIsHost() ? "HOST" : "JOIN");
	strcpy(MP_SessionName, "HUDSON");

	if (Hudson_NetIsHost() && Hudson_HostRow() != kHudsonHostRowNone)
	{
		// hostrow= (hudson_plan.h): the player's way in, the Multiplayer page's Host row, Start game and the game's Start row, with the
		// server's wait if there is one, in place of hosting from the code. Added 2026-11-10 by Melanikus (Third) for the AvP port.
		Hudson_NetSay("the host hosts online, through the Multiplayer page's Start game, as a player does");

		if (!HostThroughTheRowsForARun(level))
		{
			Hudson_NetStopBecause(HostFailedForARun("the host's Start row hosted nothing").c_str());
			return;
		}

		Hudson_NetSay("the host has its game hosted through the page's rows, and it's loading its level");
	}
	else if (Hudson_NetIsHost())
	{
		// -dev -relayhost (server #14, relay_auto.h): the list's relays first, so the game opens through the relay. If none come in time,
		// it hosts on this machine only, as a run always did, and the log says so.
		if (RelayAuto_Mode() == kRelayAutoHost)
		{
			Hudson_NetSay(RelayAuto_WaitForRelays() ? "the host has the list's relays, so it hosts through the relay"
				: "the host has no relay from the list, so it hosts on this machine only");
		}

		if (Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, level) == NET_FAIL)
		{
			Hudson_NetStopBecause(HostFailedForARun("the host couldn't open its port on this machine").c_str());
			return;
		}

		Hudson_NetSay("the host has its port open, and it's loading its level");
	}
	else
	{
		bool connected = false;

		// -dev -relayjoin (server #14, relay_auto.h): the list's first game of this version, joined through its relay, the way a player
		// picks one on the page of games found. Nothing else is tried: a run that asked for the relay and can't reach it stops, and says why.
		// Modified 2026-11-09 by Melanikus for the AvP port: with findgames=yes, as a player does it, by Find Games and the game's row
		if (RelayAuto_Mode() == kRelayAutoJoin && Hudson_JoinsThroughFindGames())
		{
			Hudson_NetSay("the joiner goes to Find Games, as a player does, to join the list's game through the relay");
			connected = JoinThroughFindGamesForARun();

			if (!connected)
			{
				Hudson_NetStopBecause("the join through Find Games failed, or the host didn't let it in");
				return;
			}
		}
		else if (RelayAuto_Mode() == kRelayAutoJoin)
		{
			int const session = RelayAuto_FindAGame();

			if (session < 0)
			{
				Hudson_NetStopBecause("no game of this version came from the relays' list");
				return;
			}

			Hudson_NetSay("the joiner has the list's game, and it's joining through the relay");
			connected = RelayAuto_Join(session, MP_PlayerName);

			if (!connected)
			{
				Hudson_NetStopBecause("the join through the relay failed");
				return;
			}
		}

		// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): a run joins through the
		// relay, as a player does, or not at all. Its plain ways, a search by a typed address (typed=yes) and a join straight to the game on
		// this machine, went with the plain game.
		if (!connected)
		{
			Hudson_NetStopBecause("the client got no answer from the host");
			return;
		}

		Hudson_NetSay("the client has connected and asked to join, and it's waiting for the host to say who it is");

		DWORD const mostMilliseconds = 20000;
		DWORD const startedWaiting = GetTickCount();

		while (netGameData.needGameDescription && GetTickCount() - startedWaiting < mostMilliseconds)
		{
			CheckForWindowsMessages();
			MinimalNetCollectMessages();
			Sleep(10);
		}

		if (netGameData.needGameDescription)
		{
			Hudson_NetStopBecause("the client connected, and the host never told it who it is");
			return;
		}

		// hostile=yes: a level one past the end of the list for the game type the host has just said. A host's answer to a search can name this in a real game, and a joiner comes here through the menu instead. This asks what the menu asks, and it has to say no.
		if (Hudson_NetIsHostile())
		{
			int const realLevel = netGameData.levelNumber;

			netGameData.levelNumber = netGameData.gameType == NGT_Coop ? NumCoopLevels : NumMultiplayerLevels;
			LogString("hostile: row 0: B9 a host's level one past the end of its list, as the menu would ask it");
			Net_HostsLevelIsOnItsList();
			netGameData.levelNumber = realLevel;
		}

		Hudson_NetSay("the client has been told who it is, and it's loading its level");
		netGameData.myStartFlag = 1;
	}

	if (netGameData.coopCampaign != kCampaignNone)
	{
		SetLevelToLoadForCampaign();
	}
	else
	{
		SetLevelToLoadForMultiplayer(level);
	}

	SetBriefingTextForMultiplayer();
	AvP.PlayerType = I_Marine;
	Coop_TakeCampaignSpecies();
	Campaign_SetDifficulty();
}

// custommap=: the Custom maps page, as a player would see it with this map chosen, for two seconds, and a picture of it
static void ShowTheCustomMapsPageForHudsonRun(void)
{
	int const framesToShow = 120;

	LoadAllAvPMenuGfx();
	AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	SetupNewMenu(AVPMENU_CUSTOMMAPS);
	CustomMapToPlay = Hudson_CustomMapPlace();

	LogString(std::string("hudson: custommap: the page lists ") + std::to_string(CustomMaps_Count()) + " maps");

	for (int map = 0; map < CustomMaps_Count(); map++)
	{
		LogString(std::string("hudson: custommap: on the list: ") + CustomMaps_Name(map));
	}

	for (int frame = 0; frame < framesToShow && bRunning; frame++)
	{
		CheckForWindowsMessages();
		ThisFramesRenderingHasBegun();
		DrawMainMenusBackdrop();
		ReadUserInput();
		AvP_UpdateMenus();
		FrameCounterHandler();
		ThisFramesRenderingHasFinished();

		if (frame == framesToShow - 1)
		{
			Hudson_SaveThePage("custom-maps-page.png");
		}

		FlipBuffers();
		Sleep(SKIP_TICKS);
	}

	// The page's own Start row, pressed the way a player does
	for (int row = 0; row < AvPMenus.NumberOfElementsInMenu; row++)
	{
		if (AvPMenus.MenuElements[row].ElementID == AVPMENU_ELEMENT_CUSTOMMAP_START)
		{
			AvPMenus.CurrentlySelectedElement = row;
		}
	}

	InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_SELECT);
	LogString(std::string("hudson: custommap: Start was pressed, and this run plays ") + LevelName);

	ReleaseAllAvPMenuGfx();
}

// ---- The presses a player makes, by Hudson (hudson_plan.h's access= and answer=), for the local rehearsal of beta.2's flow. Added
// 2026-11-09 by Melanikus for the AvP port. Each press is a row selected and Enter, as a player does, and one fixed line in the log.

// The row on this page of that kind, leading to that page or with that value (the two share a place: AVPMENU_ELEMENT's union), pressed
// as a player does: selected, then Enter. False, with a line in the log, if the page has no such row.
static bool PressTheRowForARun(enum AVPMENU_ELEMENT_ID kind, int value, const char *what)
{
	for (int row = 0; row < AvPMenus.NumberOfElementsInMenu; row++)
	{
		if (AvPMenus.MenuElements[row].ElementID == kind && AvPMenus.MenuElements[row].MaxValue == value)
		{
			AvPMenus.CurrentlySelectedElement = row;
			InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_SELECT);
			LogString(std::string("hudson: pressed ") + what);
			return true;
		}
	}

	LogErrorString(std::string("hudson: there's no row to press for ") + what + " on this page");
	return false;
}

// One frame of the main menus as a player sees them, and, if one is named, its picture before it's shown.
// pagehold= (Hudson_PageHoldSeconds, with film=menus): a page to be pictured is first held, as it is, that many seconds, so the run's film
// shows it before the picture ends its clip. holdFirst is false for a page the other game is waiting on this one to leave (the host's
// yes), which isn't held. Modified 2026-11-10 by Melanikus (Third) for the AvP port.
static void MenuFrameForARun(const char *picture, bool holdFirst = true)
{
	if (picture && holdFirst && Hudson_PageHoldSeconds() > 0)
	{
		DWORD const holdStarted = GetTickCount();

		while (bRunning && (DWORD)(GetTickCount() - holdStarted) < (DWORD)Hudson_PageHoldSeconds() * 1000u)
		{
			MenuFrameForARun(NULL);
		}
	}

	CheckForWindowsMessages();
	ThisFramesRenderingHasBegun();
	DrawMainMenusBackdrop();
	ReadUserInput();
	AvP_UpdateMenus();
	FrameCounterHandler();
	ThisFramesRenderingHasFinished();

	if (picture)
	{
		Hudson_SaveThePage(picture);
	}

	FlipBuffers();
	Sleep(SKIP_TICKS);
}

// Some seconds of the menus as they are. Taking one time from the other works across GetTickCount going round to 0.
static void MenuSecondsForARun(int seconds)
{
	DWORD const started = GetTickCount();

	while (bRunning && (DWORD)(GetTickCount() - started) < (DWORD)seconds * 1000u)
	{
		MenuFrameForARun(NULL);
	}
}

// After a press that calls the list: frames until its answer is on the page, at most 30 s, then two seconds more, and the page's picture.
// False if the answer never came.
static bool PictureTheAnswerForARun(const char *picture)
{
	DWORD const started = GetTickCount();
	bool came = true;

	while (bRunning && AccessPage_CallOnItsWay())
	{
		if ((DWORD)(GetTickCount() - started) > 30000u)
		{
			came = false;
			break;
		}

		MenuFrameForARun(NULL);
	}

	MenuSecondsForARun(2);
	MenuFrameForARun(picture);
	return came;
}

// findgames=yes: a joiner through the relay joins as a player does (PO's ruling, 2026-11-09: the rehearsal proves the player's path).
// Join Game, Find Games, then, once the list shows a game this version may join through a relay, its row, pressed. The row's own code
// joins it (Net_ConnectToSession, drawing the joining screen meanwhile). Then the joining page, drawn as a player sees it, is pictured
// asking the host with its check code (join-asking.png), and let in (join-letin.png). True once the host has let it in.
static bool JoinThroughFindGamesForARun(void)
{
	LoadAllAvPMenuGfx();
	AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	SetupNewMenu(AVPMENU_MULTIPLAYERJOINGAME);
	MenuSecondsForARun(1);

	bool joined = PressTheRowForARun(AVPMENU_ELEMENT_GOTOMENU, AVPMENU_MULTIPLAYERSELECTSESSION, "Find Games");
	int row = -1;
	DWORD const started = GetTickCount();

	// The list's games come within a few seconds of the page opening; at most 30 s
	while (joined && bRunning && row < 0 && (DWORD)(GetTickCount() - started) < 30000u)
	{
		MenuFrameForARun(NULL);

		for (int r = 0; r < AvPMenus.NumberOfElementsInMenu && row < 0; r++)
		{
			int const s = AvPMenus.MenuElements[r].ElementID == AVPMENU_ELEMENT_LISTCHOICE ? OnlinePage_SessionForRow(r) : -1;

			if (s >= 0 && SessionData[s].AllowedToJoin && SessionData[s].throughRelay)
			{
				row = r;
			}
		}
	}

	if (row < 0)
	{
		LogErrorString("hudson: Find Games showed no game this version may join through a relay");
		joined = false;
	}

	if (joined)
	{
		MenuFrameForARun("join-list.png");
		AvPMenus.CurrentlySelectedElement = row;
		InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_SELECT);
		LogString("hudson: pressed the list's game");

		// A row that joined goes on to the joining page (its MenuToGoTo); one that didn't stays on the games page, saying why
		joined = AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_JOINING;
	}

	bool askingPictured = false;
	bool letIn = false;
	DWORD const asked = GetTickCount();

	// Asking the host (J4), then let in (J5): at most 40 s, past the host's 30 s for an answer
	while (joined && bRunning && !letIn && (DWORD)(GetTickCount() - asked) < 40000u)
	{
		int const answer = Net_JoinAnswerNow();
		char digits[kNetRelayCheckCodeBytes];
		const char *picture = NULL;

		if (answer == kNetJoinAnswerYes)
		{
			picture = "join-letin.png";
			letIn = true;
		}
		else if (answer != kNetJoinAnswerWaiting && answer != kNetJoinAnswerNone)
		{
			// The host said no, was answering someone else, didn't answer, or is another build: the page says which. (None is a request
			// not under way yet, which is still waiting.)
			LogErrorString("hudson: the host didn't let this joiner in");
			MenuSecondsForARun(2);
			break;
		}
		else if (!askingPictured && Net_RelayCheckCodeNow(digits) && (DWORD)(GetTickCount() - asked) > 1000u)
		{
			picture = "join-asking.png";
			askingPictured = true;
		}

		// Let in is never held (pagehold=, MenuFrameForARun): once the host has said yes, it's waiting for this game to go on to the level
		MenuFrameForARun(picture, !letIn);
	}

	ReleaseAllAvPMenuGfx();
	return letIn;
}

// access=: Multiplayer, Online access, then Request access (and Check again after accesswait=), or Make a new identity and Yes. The run
// stays at the menus, and ends once the presses are done: it plays no level. Its kept identity is a Hudson run's own (the store's
// NetIdentityStore_FileName), never the player's.
static void RunTheAccessPagesForHudson(void)
{
	int const press = Hudson_AccessPress();

	LoadAllAvPMenuGfx();
	AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	SetupNewMenu(AVPMENU_MULTIPLAYER);
	MenuSecondsForARun(1);

	bool asAsked = PressTheRowForARun(AVPMENU_ELEMENT_GOTOMENU, AVPMENU_ONLINE_ACCESS, "Online access");
	MenuSecondsForARun(2);

	// yespage= (hudson_plan.h): the Online access row opened the yes page in its place, as a player's first time does
	// (ServerYesPage_WayIn). It's pictured, and the run's row pressed: Yes goes on to the Online access page, as the row would have; No
	// goes back to the Multiplayer page, having sent nothing, and the run ends there. The yes holds for this run alone (relay_list.h).
	// Added 2026-11-10 by Melanikus (Third) for the AvP port.
	if (asAsked && Hudson_YesPagePress() != kHudsonYesPageNone)
	{
		bool const yes = Hudson_YesPagePress() == kHudsonYesPageYes;
		asAsked = AvPMenus.CurrentMenu == AVPMENU_SERVER_YES;

		if (!asAsked)
		{
			LogErrorString("hudson: yespage= asked for the yes page, and the Online access row didn't open it");
		}

		MenuFrameForARun(asAsked ? "server-yes.png" : NULL);
		asAsked = asAsked && PressTheRowForARun(AVPMENU_ELEMENT_ONLINE_ACCESS, yes ? kServerYesRowYes : kServerYesRowNo, yes ? "Yes, use this server" : "No, not now");
		MenuSecondsForARun(2);

		if (asAsked && !yes)
		{
			bool const back = AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER;
			MenuFrameForARun("server-yes-no.png");
			ReleaseAllAvPMenuGfx();
			Hudson_AccessRunEnds(back ? "the yes page's No was pressed, and the run is back on the Multiplayer page, having asked the server nothing"
				: "the yes page's No was pressed, and the run isn't back on the Multiplayer page", back);
			return;
		}

		if (asAsked && AvPMenus.CurrentMenu != AVPMENU_ONLINE_ACCESS)
		{
			LogErrorString("hudson: the yes page's Yes was pressed, and the run isn't on the Online access page");
			asAsked = false;
		}
	}

	if (asAsked && press == kHudsonAccessNewId)
	{
		asAsked = PressTheRowForARun(AVPMENU_ELEMENT_GOTOMENU, AVPMENU_ONLINE_ACCESS_NEW, "Make a new identity");
		MenuSecondsForARun(2);
		asAsked = asAsked && PressTheRowForARun(AVPMENU_ELEMENT_ONLINE_ACCESS, kAccessRowNewYes, "Yes, make a new identity");
		MenuSecondsForARun(2);
		MenuFrameForARun("access-newid.png");
	}
	else if (asAsked)
	{
		asAsked = PressTheRowForARun(AVPMENU_ELEMENT_ONLINE_ACCESS, kAccessRowRequest, "Request access") && PictureTheAnswerForARun("access-asked.png");

		if (asAsked && press == kHudsonAccessCheck)
		{
			MenuSecondsForARun(Hudson_AccessWaitSeconds());
			asAsked = PressTheRowForARun(AVPMENU_ELEMENT_ONLINE_ACCESS, kAccessRowRequest, "Check again") && PictureTheAnswerForARun("access-checked.png");
		}
	}

	ReleaseAllAvPMenuGfx();
	Hudson_AccessRunEnds(asAsked ? "the Online access page's rows were pressed, and the server's answers pictured"
		: "a press on the Online access page couldn't be made, or the server's answer never came", asAsked);
}

// One frame of one of the intro's two still pictures, drawn as intro.cpp draws it at full brightness, and its picture (menus=pictures):
// the copyright screen (Show_CopyrightInfo), or the Aliens versus Predator logo over the starfield (Show_AvPLogo). The logos' film
// between them keeps the screen's whole width, as every film does, so it isn't pictured. Added 2026-11-10 by Melanikus for the AvP port.
static void IntroFrameForARun(bool copyright, const char *picture)
{
	CheckForWindowsMessages();
	ThisFramesRenderingHasBegun();

	if (copyright)
	{
		DrawAvPMenuGfx_Faded(AVPMENUGFX_COPYRIGHT_SCREEN, 0, 0, ONE_FIXED, AVPMENUFORMAT_LEFTJUSTIFIED);
	}
	else
	{
		uint32_t width = 0;
		uint32_t height = 0;
		Tex_GetDimensions(AVPMENUGFX_ALIENSVPREDATOR, width, height);
		int const tall = height > 480 ? 480 : static_cast<int>(height);

		DrawMainMenusBackdrop();
		DrawAvPMenuGfx(AVPMENUGFX_ALIENSVPREDATOR, MENU_CENTREX, (480 - tall) / 2, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	ThisFramesRenderingHasFinished();
	Hudson_SaveThePage(picture);
	FlipBuffers();
	Sleep(SKIP_TICKS);
}

// menus=pictures: four of the menus' pages as a player sees them, and the intro's two still pictures, each pictured, so how the menus
// are drawn can be seen at any screen size (menu_shape.h). The only row pressed is Multiplayer's Online access; the page opening sends
// nothing, since it asks the list only once a request has been sent, which a new run never has. Added 2026-11-10 by Melanikus for the
// AvP port.
static void RunTheMenuPicturesForHudson(void)
{
	LoadAllAvPMenuGfx();
	AvPMenus.MenusState = MENUSSTATE_MAINMENUS;

	SetupNewMenu(AVPMENU_MAIN);
	MenuSecondsForARun(1);
	MenuFrameForARun("menu-main.png");

	SetupNewMenu(AVPMENU_MULTIPLAYER);
	MenuSecondsForARun(1);
	MenuFrameForARun("menu-multiplayer.png");

	bool const accessOpened = PressTheRowForARun(AVPMENU_ELEMENT_GOTOMENU, AVPMENU_ONLINE_ACCESS, "Online access");
	MenuSecondsForARun(1);
	MenuFrameForARun("menu-access.png");

	SetupNewMenu(AVPMENU_OPTIONS);
	MenuSecondsForARun(1);
	MenuFrameForARun("menu-options.png");

	IntroFrameForARun(true, "menu-copyright.png");
	IntroFrameForARun(false, "menu-logo.png");

	ReleaseAllAvPMenuGfx();
	Hudson_AccessRunEnds(accessOpened ? "four of the menus' pages and the intro's two still pictures were pictured"
		: "the Online access row couldn't be pressed, so it wasn't pictured", accessOpened);
}

static int StartLevelForHudsonRun(void)
{
	static bool levelStarted = false;

	if (levelStarted)
	{
		return 0;
	}

	levelStarted = true;

	LoadDefaultPrimaryConfigs();
	TimeScale = ONE_FIXED;

	// What the menus do before any level. Letting them go keeps the small font and the sliders loaded for the in-game menus, and the level's briefing text is drawn with that font. Without this, a run's
	// frames and its texture memory wouldn't be a player's. Neither of these makes a sound.
	LoadAllAvPMenuGfx();
	ReleaseAllAvPMenuGfx();

	// the run's own folder has no profiles, so the first is the "New Profile" entry, with its default settings
	ExamineSavedUserProfiles();
	UserProfileNumber = 0;
	UserProfilePtr = GetFirstUserProfile();
	GetSettingsFromUserProfile();

	CheatMode_Active = CHEATMODE_NONACTIVE;
	LoadGameRequest = SAVELOAD_REQUEST_NONE;
	AvP.LevelCompleted = 0;

	// access=: the run stays at the menus, presses the Online access page's rows, and ends there (added 2026-11-09, hudson_plan.h)
	if (Hudson_AccessPress() != kHudsonAccessNone)
	{
		RunTheAccessPagesForHudson();
		return 0;
	}

	// menus=pictures: the run stays at the menus, pictures four pages, and ends there (added 2026-11-10, hudson_plan.h)
	if (Hudson_MenuPictures())
	{
		RunTheMenuPicturesForHudson();
		return 0;
	}

	if (Hudson_NetIsOn())
	{
		StartNetworkGameForHudsonRun();
	}
	else
	{
		AvP.PlayerType = Hudson_PlayAsPredator() ? I_Predator : I_Marine;

		if (Hudson_PlayAsAlien())
		{
			AvP.PlayerType = I_Alien;
		}

		AvP.Difficulty = (I_HARDANUFF)Hudson_Difficulty();
		MarineEpisodeToPlay = Hudson_MarineEpisode();
		SetLevelToLoad((enum AVP_ENVIRONMENT_ID)Hudson_Environment());
		SetBriefingTextForEpisode(MarineEpisodeToPlay, AvP.PlayerType);

		if (Hudson_CustomMapPlace() >= 0)
		{
			ShowTheCustomMapsPageForHudsonRun();
		}

	}

	// listplace=: what the skirmish page does with that place on its level row
	if (Hudson_ListPlace() >= 0)
	{
		netGameData.skirmishMode = TRUE;
		SetDefaultMultiplayerConfig();
		netGameData.gameType = NGT_Coop;
		netGameData.levelNumber = Hudson_ListPlace();
		netGameData.coopDifficulty = Hudson_Difficulty();
		BuildMultiplayerLevelNameArray();
		Campaign_SetFromLevelRow();
		Campaign_SetDifficulty();
		Campaign_ChooseRollNumber();
		GetNextAllowedSpecies(&MP_Species, TRUE);
		LogString(std::string("hudson: the level row reads ") + CoopLevelNames[netGameData.levelNumber]);

		if (Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, NGT_Coop, netGameData.levelNumber) != NET_OK)
		{
			LogErrorString(HostFailedForARun("listplace: the skirmish couldn't start, so this run isn't one"));
		}

		SetLevelToLoadForThisNetworkGame();
		SetBriefingTextForMultiplayer();
	}

	// A gun run with class= is a skirmish, the game's multiplayer for one, as the menus would start it. It opens no network
	// socket: Net_HostGame skips that when skirmishMode is on.
	if (Hudson_ClassSlider())
	{
		netGameData.skirmishMode = TRUE;
		SetDefaultMultiplayerConfig();
		netGameData.gameType = NGT_Coop;
		netGameData.levelNumber = 0;
		MP_Species = Hudson_ClassSlider();

		if (Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, NGT_Coop, 0) != NET_OK)
		{
			LogErrorString(HostFailedForARun("class: the skirmish couldn't start, so this run isn't one"));
		}
	}

	AvPMenus.MenusState = MENUSSTATE_STARTGAME;

	Hudson_SetTheMode();

	// A run skips a level's opening film unless it's asked for one. This is the call the menus make.
	if (Hudson_PlaysLevelFilms())
	{
		HandlePreGameFMVs();
	}

	return 1;
}

// -dev -relayhost and -dev -askjoiners (relay_auto.h): the copy hosts by itself as the menus start, once, so the online screens can be
// filmed with nobody at the game. Added 2026-11-09 by Melanikus for the AvP port. It does what the host page's Start row does
// (AVPMENU_ELEMENT_STARTMPGAME), with the multiplayer pages' answers fixed: the first profile, a deathmatch on the first level of the
// multiplayer list, a marine, and fixed names. Nothing but the word comes from the command line. Unlike the Start row it doesn't save the
// settings as the "previous game", so nobody's own choices are changed. False if it didn't host: then the menus go on as they always do.
static bool HostByItselfThroughTheRelay(void)
{
	if (!RelayAuto_HostsByItselfNow())
	{
		return false;
	}

	// Nobody is here to choose a profile, so it's the first, as a new player's would be
	if (UserProfileNumber == -1)
	{
		ExamineSavedUserProfiles();
		UserProfileNumber = 0;
		UserProfilePtr = GetFirstUserProfile();
		GetSettingsFromUserProfile();
	}

	SetDefaultMultiplayerConfig();
	netGameData.skirmishMode = FALSE;   // the game's "skirmish" is against the computer, with no network at all
	netGameData.gameType = NGT_Individual;
	netGameData.levelNumber = 0;        // the first level of the multiplayer list
	netGameData.coopCampaign = kCampaignNone;
	netGameData.coopCampaignLevel = 0;
	MP_Species = 0;                     // a marine
	strcpy(MP_PlayerName, "Host");
	strcpy(MP_SessionName, "Relay test");

	// The list's relays first, as the multiplayer page asks for them, so the game opens through the nearest
	LogString(RelayAuto_WaitForRelays() ? "relay auto: the list's relays have come, so this copy hosts through the nearest"
		: "relay auto: no relay came from the list in time, so this copy hosts on this network only");

	if (Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, netGameData.levelNumber) == NET_FAIL)
	{
		LogString(HostFailedForARun("relay auto: this copy couldn't host, so it waits at the menus"));
		return false;
	}

	LogString("relay auto: this copy hosts a deathmatch on the first level of the multiplayer list, and it's loading");
	AvPMenus.MenusState = MENUSSTATE_STARTGAME;
	SetLevelToLoadForThisNetworkGame();
	SetBriefingTextForMultiplayer();
	return true;
}

int AvP_MainMenus(void)
{
	if (Coop_LevelChangeIsDue())
	{
		return StartNextCampaignLevel();
	}

	// custommap= with coopend=: the map is finished, so the game's own way back to the menus runs, below, with nothing changed but the intro
	if (HudsonRunIsOn && !Hudson_CustomMapHasEnded())
	{
		return StartLevelForHudsonRun();
	}

	if (HudsonRunIsOn)
	{
		DebuggingCommandsActive = 0;
	}

	LoadDefaultPrimaryConfigs();
	
	SoundSys_ResetFadeLevel();
	SoundSys_Management();
	
	TimeScale = ONE_FIXED;

	if (!LobbiedGame)  // Edmond
		CheckForCredits();
	
	TimeStampedMessage("start of menus");

	// start background FMV
	StartMenuBackgroundFmv();

	#if PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO
	if (AvP.LevelCompleted)
	{
		Show_WinnerScreen();
	}
	#endif

	LoadAllAvPMenuGfx();

	TimeStampedMessage("after LoadAllAvPMenuGfx");

	ResetFrameCounter();

	if (!LobbiedGame && !HudsonRunIsOn)	// Edmond
		PlayIntroSequence();

	if (VideoModeNotAvailable)
	{
		LoadGameRequest = SAVELOAD_REQUEST_NONE;
		DisplayVideoModeUnavailableScreen();
	}
	VideoModeNotAvailable = 0;

	// -dev -onlinescreens: every online-play screen, once, with made-up games and no network, for pictures (online_screens_page.h). Without
	// the word, this does nothing.
	OnlineDemo_Run();

	if (CoopMenu_HasALastWord())
	{
		// A co-op game ended for a reason the player should hear: the page says why, before anything else. A custom map played with others
		// has no page of its own to go back to, so that's forgotten here.
		CoopMenu_PageAfterACustomMap();
		AvP.LevelCompleted = 0;
		SetupNewMenu(AVPMENU_COOP_OUT);
		AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	}
	else if (AvP.LevelCompleted && CheatMode_Active == CHEATMODE_NONACTIVE && !DebuggingCommandsActive)
	{
		if (CustomMaps_Playing())
		{
			// A custom map has no film after it, and no place in a campaign: the episode numbers still name the last stock level, so
			// using them here would play that level's film and mark it done. It goes back to its own page instead.
			SetupNewMenu((enum AVPMENU_ID)CoopMenu_PageAfterACustomMap());
		}
		else
		{
			HandlePostGameFMVs();
			OkayToPlayNextEpisode();
		}
		AvP.LevelCompleted = 0;
		AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	}
	else if (!LobbiedGame)
	{
		if (UserProfileNumber==-1)
		{
			SetupNewMenu(AVPMENU_USERPROFILESELECT);
		}
		else
		{
			SetupNewMenu(AVPMENU_MAIN);
		}
		AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	}
	else
	{
		SetupNewMenu(AVPMENU_USERPROFILESELECT);
	}

	// -dev -findgames, once: the player's own way to the page of games found, Multiplayer, then Join Game, then Find Games, each page set
	// up as pressing it would, so the search is the one a player gets. With no profile chosen yet, the first one, as the profile page
	// would start on, so the player's name for joining is that profile's.
	if (OnlinePage_GoStraightToFindGames())
	{
		if (UserProfileNumber == -1)
		{
			ExamineSavedUserProfiles();
			UserProfileNumber = 0;
			UserProfilePtr = GetFirstUserProfile();
			GetSettingsFromUserProfile();
		}

		AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
		SetupNewMenu(AVPMENU_MULTIPLAYER);
		SetupNewMenu(AVPMENU_MULTIPLAYERJOINGAME);
		SetupNewMenu(AVPMENU_MULTIPLAYERSELECTSESSION);
		LogString("online screens: this copy went straight to the page of games found (-dev -findgames)");
	}

	CheatMode_Active = CHEATMODE_NONACTIVE;

	// Whatever was played has ended and been dealt with above, so nothing is being played now
	CustomMaps_SetPlaying(false);

	// -dev -relayhost or -dev -askjoiners: host by itself, once, instead of waiting for someone at the menus. Then the menus' loop below
	// doesn't run, and the game starts as if the host page's Start row had been chosen (HostByItselfThroughTheRelay).
	HostByItselfThroughTheRelay();

	TimeStampedMessage("starting general menus");

	while (AvPMenus.MenusState == MENUSSTATE_MAINMENUS && bRunning)
	{
		mainMenu = true;

		DWORD nextGameTick = timeGetTime();
		int sleepTime = 0;

		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();

		DrawMainMenusBackdrop();

		ReadUserInput();

		AvP_UpdateMenus();

		ShowMenuFrameRate();

		FrameCounterHandler();
		PlayMenuMusic();

		SoundSys_Management();
		UpdateGammaSettings();
		ViewSettings_MenuFrame();
		Reverb_MenuFrame();

		CheckForLoadGame();

		ThisFramesRenderingHasFinished();

		if (HudsonRunIsOn)
		{
			Hudson_LooksForThePageAfterAMap(AvPMenus.CurrentMenu == AVPMENU_CUSTOMMAPS);
		}

		FlipBuffers();

		nextGameTick += SKIP_TICKS;
		sleepTime = nextGameTick - timeGetTime();
		if (sleepTime >= 0) 
		{
			Sleep(sleepTime);
		}
	}

	if (AvPMenus.MenusState == MENUSSTATE_OUTSIDEMENUS)
	{
		// Don't bother showing credits if we are just exiting in order to start a game
		// using mplayer. The credits will get shown later after the player has actually
		// played the game
		if (!LaunchingMplayer)
		{
			if (!LobbiedGame) {    // Edmond
				DoCredits();
			}
		}
	}

	TimeStampedMessage("ready to exit menus");

	EndMenuMusic();
	EndMenuBackgroundFmv();
	TimeStampedMessage("after EndMenuMusic");

	#if 0
	#if PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO
	if (/*!AvP.LevelCompleted &&*/ (AvPMenus.MenusState != MENUSSTATE_STARTGAME)) ShowSplashScreens();
	TimeStampedMessage("after ShowSplashScreens");
	#endif
	#endif

	ReleaseAllAvPMenuGfx();

	TimeStampedMessage("after ReleaseAllAvPMenuGfx");

	SoundSys_StopAll();
	SoundSys_Management();

	if (CheatMode_Active == CHEATMODE_NONACTIVE) HandlePreGameFMVs();

	HandleCheatModeFeatures();
	AvP.LevelCompleted = 0;

	return (AvPMenus.MenusState == MENUSSTATE_STARTGAME);
}

void HandlePostGameFMVs(void)
{
	switch(AvP.PlayerType)
	{
		case I_Marine:
		{
			if (MarineEpisodeToPlay==MAX_NO_OF_BASIC_MARINE_EPISODES-1)
			{
				PlayFMV("FMVs/marineoutro.bik");
			}
			break;
		}
		case I_Alien:
		{
			if (AlienEpisodeToPlay==MAX_NO_OF_BASIC_ALIEN_EPISODES-1)
			{
				PlayFMV("FMVs/alienoutro.bik");
			}
			break;
		}
		case I_Predator:
		{
			if (PredatorEpisodeToPlay==MAX_NO_OF_BASIC_PREDATOR_EPISODES-1)
			{
				PlayFMV("FMVs/predatoroutro.bik");
			}
			break;
		}
	}
}

// While a film plays, the connection has to stay alive, or the other machines give up on this one. Only that:
// messages wait until the level has loaded, as they do during any load. On the way to the next level there's
// no level here for them to act on. Each machine sees the film for itself, and anyone can skip it with a key.
static bool KeepTheNetworkGoing(void)
{
	Net_ServiceNetwork();
	return Hudson_FilmGoesOn();
}

static void PlayFilmWithTheNetworkGoing(const char *film)
{
	if (AvP.Network != I_No_Network)
	{
		WhileAFilmPlays = KeepTheNetworkGoing;
	}

	Hudson_FilmStarts(film);
	PlayFMV(film);
	Hudson_FilmEnded();
	WhileAFilmPlays = 0;
}

void HandlePreGameFMVs(void)
{
	// A deathmatch on one of these levels isn't its story, so it doesn't get its film. A campaign does.
	if (AvPMenus.MenusState == MENUSSTATE_STARTGAME && LoadGameRequest == SAVELOAD_REQUEST_NONE && SameAsSinglePlayer())
	{
		if (!stricmp("derelict", LevelName))
		{
			PlayFilmWithTheNetworkGoing("FMVs/marineintro.bik");
		}
		else if (!stricmp("temple", LevelName))
		{
			PlayFilmWithTheNetworkGoing("FMVs/alienintro.bik");
		}
		else if (!stricmp("fall", LevelName))
		{
			PlayFilmWithTheNetworkGoing("FMVs/predatorintro.bik");
		}
	}
}

// The film single player shows when its last basic level is finished, for a campaign going on past it.
static void PlayFilmAfterCampaignLevel(int finishedLevel)
{
	if (HudsonRunIsOn && !Hudson_PlaysLevelFilms())
	{
		return;
	}

	if (netGameData.coopCampaign == kCampaignMarine && finishedLevel == MAX_NO_OF_BASIC_MARINE_EPISODES - 1)
	{
		PlayFilmWithTheNetworkGoing("FMVs/marineoutro.bik");
	}
	else if (netGameData.coopCampaign == kCampaignPredator && finishedLevel == MAX_NO_OF_BASIC_PREDATOR_EPISODES - 1)
	{
		PlayFilmWithTheNetworkGoing("FMVs/predatoroutro.bik");
	}
	else if (netGameData.coopCampaign == kCampaignAlien && finishedLevel == MAX_NO_OF_BASIC_ALIEN_EPISODES - 1)
	{
		PlayFilmWithTheNetworkGoing("FMVs/alienoutro.bik");
	}
}

extern void QuickSplashScreens(void)
{
	if (AvP.LevelCompleted)
	{
		Show_WinnerScreen();
	}
	ShowSplashScreens();
}

extern void AvP_TriggerInGameMenus(void)
{
	AvPMenus.MenusState = MENUSSTATE_INGAMEMENUS;
	SetupNewMenu(AVPMENU_INGAME);

	/* KJL 16:32:55 21/07/98 - tell the console to go away */
	if (IOFOCUS_AcceptTyping()) IOFOCUS_Toggle();
}

static void AnswerTheRequestForARun(void);

extern int AvP_InGameMenus(void)
{
	if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
	{
		D3D_FadeDownScreen(ONE_FIXED/8,0);
		AvP_UpdateMenus();

		// answer=: a Hudson host answers a player asking to join, as a player does (added 2026-11-09, hudson_plan.h)
		if (HudsonRunIsOn)
		{
			AnswerTheRequestForARun();
		}

		UpdateGammaSettings();
		ViewSettings_MenuFrame();
		Reverb_MenuFrame();

		return 1;
	}
	return 0;
}

extern int InGameMenusAreRunning(void)
{
	return (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS);
}

// THE HOST'S ANSWER TO A PLAYER ASKING TO JOIN (online_screens_page.h's H1). While a player waits to be let in, the page the host is on (the
// in-game menus, or the co-op lobby) shows two rows in front of its own, "Let NAME in" and "Say no to NAME", with a line at the top saying who
// asks. The rows are a copy of the page's own with the two in front, made again each frame, so a page that changes its own rows (the lobby's,
// as players come and go) stays right. The selection stays on the row it was on, so a key pressed by chance can't let anyone in.
static AVPMENU_ELEMENT answerRows[2 + 31 + 1];
static bool answerRowsShown = false;
static char letInRow[96], sayNoRow[96], askingLine[96];
static char checkCodeLine[48];   // the request's check code, once the joiner's handshake through the relay is done; "" before or without one

static bool ThePageAnswersRequests(enum AVPMENU_ID page)
{
	return page == AVPMENU_INNETGAME || page == AVPMENU_INCOOPGAME || page == AVPMENU_COOP_HOST;
}

static void UpdateTheAnswerRows(void)
{
	char name[kOnlineRowLetters];
	int seconds = 0;
	AVPMENU_ELEMENT *const own = AvPMenusData[AvPMenus.CurrentMenu].MenuElements;
	bool const ours = answerRowsShown && AvPMenus.MenuElements == answerRows;
	bool const waiting = ThePageAnswersRequests(AvPMenus.CurrentMenu) && OnlineHost_AskWaiting(name, sizeof(name), &seconds);

	if (!waiting)
	{
		// Answered, or gone: the page's own rows again, with the same row selected
		if (ours)
		{
			int const selected = AvPMenus.CurrentlySelectedElement - 2;
			AvPMenus.MenuElements = own;
			CountTheRowsOnThisPage();
			AvPMenus.CurrentlySelectedElement = selected < 0 ? 0 : selected;
		}

		answerRowsShown = false;
		return;
	}

	OnlineScreens_LetInWords(name, letInRow, sizeof(letInRow));
	OnlineScreens_SayNoWords(name, sayNoRow, sizeof(sayNoRow));
	OnlineScreens_AskingWords(name, seconds, askingLine, sizeof(askingLine));
	OnlineHost_AskCheckCode(checkCodeLine, sizeof(checkCodeLine));

	// Each row keeps how bright it was last frame, so the copy made again doesn't flicker
	int const room = (int)(sizeof(answerRows) / sizeof(answerRows[0]));
	int brightness[sizeof(answerRows) / sizeof(answerRows[0])];

	for (int row = 0; row < room; row++)
	{
		brightness[row] = ours ? answerRows[row].Brightness : BRIGHTNESS_OF_DARKENED_ELEMENT;
	}

	memset(answerRows, 0, sizeof(answerRows));

	for (int answer = 0; answer < 2; answer++)
	{
		answerRows[answer].ElementID = AVPMENU_ELEMENT_JOINANSWER;
		answerRows[answer].TextPtr = answer == 0 ? letInRow : sayNoRow;
		answerRows[answer].MaxValue = answer == 0 ? 1 : 0;
		answerRows[answer].HelpString = TEXTSTRING_BLANK;
	}

	int rows = 2;

	for (AVPMENU_ELEMENT const *element = own; element->ElementID != AVPMENU_ELEMENT_ENDOFMENU && rows < room - 1; element++)
	{
		answerRows[rows++] = *element;
	}

	answerRows[rows].ElementID = AVPMENU_ELEMENT_ENDOFMENU;

	int const selected = AvPMenus.CurrentlySelectedElement + (ours ? 0 : 2);

	AvPMenus.MenuElements = answerRows;
	CountTheRowsOnThisPage();
	AvPMenus.CurrentlySelectedElement = selected < AvPMenus.NumberOfElementsInMenu ? selected : AvPMenus.NumberOfElementsInMenu - 1;

	if (ours)
	{
		for (int row = 0; row < AvPMenus.NumberOfElementsInMenu && row < room; row++)
		{
			answerRows[row].Brightness = brightness[row];
		}
	}

	answerRowsShown = true;
}

// answer=: the host's answer rows are up (Hudson_EndOfFrame opened this menu when a player asked, as a player presses Esc). Once they've
// been up answerafter= seconds: a picture of this frame, with the asking line, the check code and the rows; on the next frame the press,
// Let in or Say no; then Resume, as a player goes back to the game. Added 2026-11-09 by Melanikus for the AvP port.
static void AnswerTheRequestForARun(void)
{
	static DWORD rowsUpSince = 0;
	static bool pictured = false;
	int const press = Hudson_AnswerPress();

	if (press == kHudsonAnswerNone || !(answerRowsShown && AvPMenus.MenuElements == answerRows))
	{
		rowsUpSince = 0;
		pictured = false;
		return;
	}

	DWORD const now = GetTickCount();

	if (rowsUpSince == 0)
	{
		rowsUpSince = now != 0 ? now : 1;
		return;
	}

	if ((DWORD)(now - rowsUpSince) < (DWORD)Hudson_AnswerAfterSeconds() * 1000u)
	{
		return;
	}

	// The picture first, of this frame as it's drawn; the press a frame later, so the picture never misses the rows
	if (!pictured)
	{
		Hudson_PictureThisFrame("host-asked.png");
		pictured = true;
		return;
	}

	bool const letIn = press == kHudsonAnswerLetIn;
	PressTheRowForARun(AVPMENU_ELEMENT_JOINANSWER, letIn ? 1 : 0, letIn ? "Let in" : "Say no");
	rowsUpSince = 0;
	pictured = false;

	// Back to the game, by the menu's own Resume row
	PressTheRowForARun(AVPMENU_ELEMENT_RESUMEGAME, 0, "Resume");
}

// ---- A host's wait for the server, and a failed host's line (host_wait_rules.h says why). Added 2026-11-10 by Melanikus for the AvP port
// (PO's rulings Q3 and Q4).

static bool HostsOnline(void);

// What a waiting host does once the server is ready: the config page's Start row hosts the game set up there; Co-op campaign opens its
// page, which hosts as it opens
enum HostWaitFor
{
	kHostWaitingForNothing,
	kHostWaitingToStart,
	kHostWaitingToOpenCoop
};

static HostWaitFor hostWaiting = kHostWaitingForNothing;
static enum AVPMENU_ID hostWaitPage = AVPMENU_MULTIPLAYER;   // the page it waits on: leaving it ends the wait
static uint32_t hostWaitSince = 0;                           // GetTickCount when it began
static uint32_t hostWaitRetrySince = 0;                      // while a wait's host is tried: when that wait began, so a wait that
                                                             // follows keeps counting from the first press, never past 30 s in all
static char hostLine[200];                                   // "Asking the server...", or why a host failed, under the page's rows
static enum AVPMENU_ID hostLinePage = AVPMENU_MULTIPLAYER;   // the page it's for, the only one it's drawn on

static void ShowTheHostLine(enum AVPMENU_ID page, const char *text)
{
	snprintf(hostLine, sizeof(hostLine), "%s", text ? text : "");
	hostLinePage = page;
}

// Why a host failed, on the page it was pressed on: the game's words for what failed, then why in its own fixed words
static void ShowWhyTheHostFailed(enum AVPMENU_ID page, const char *why)
{
	char line[sizeof(hostLine)];
	HostWait_FailedLine(GetTextString(why ? TEXTSTRING_HOST_FAILED : TEXTSTRING_HOST_FAILED_PLAIN), why, line, sizeof(line));
	ShowTheHostLine(page, line);
}

// The config page's Start row, pressed again for the player once the server is ready, so the game is hosted just as the row hosts it
static void PressTheStartRowAgain(void)
{
	for (int row = 0; row < AvPMenus.NumberOfElementsInMenu; row++)
	{
		if (AvPMenus.MenuElements[row].ElementID == AVPMENU_ELEMENT_STARTMPGAME)
		{
			AvPMenus.CurrentlySelectedElement = row;
			InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_SELECT);
			return;
		}
	}
}

// A host pressed while the server hasn't answered yet waits for it, on the page it was pressed on. False, with nothing changed, when
// there's nothing to wait for: no server named, or a server that has answered.
static bool AHostWaits(HostWaitFor what)
{
	const char *why = NULL;

	if (!HostsOnline() || Net_ServerState(&why) != kNetServerAsking)
	{
		return false;
	}

	// Asked now, if it wasn't yet: relay_list.cpp asks once at a time, and never before the player's yes
	RelayList_FindRelays();
	hostWaiting = what;
	hostWaitPage = AvPMenus.CurrentMenu;
	hostWaitSince = hostWaitRetrySince != 0 ? hostWaitRetrySince : GetTickCount();
	ShowTheHostLine(hostWaitPage, GetTextString(TEXTSTRING_HOST_ASKING));
	return true;
}

// One frame of a host's wait (host_wait_rules.h). A line left from another page goes, too.
static void HostWaitFrame(void)
{
	if (hostLine[0] && AvPMenus.CurrentMenu != hostLinePage)
	{
		hostLine[0] = 0;
	}

	if (hostWaiting == kHostWaitingForNothing)
	{
		return;
	}

	// Esc, or any other way off the page, ends the wait, and nothing is hosted
	if (AvPMenus.MenusState != MENUSSTATE_MAINMENUS || AvPMenus.CurrentMenu != hostWaitPage)
	{
		hostWaiting = kHostWaitingForNothing;
		return;
	}

	const char *why = NULL;
	int const state = Net_ServerState(&why);
	HostWaitFor const what = hostWaiting;

	switch (HostWait_Step(state == kNetServerAsking, state == kNetServerReady, state == kNetServerCantUse, hostWaitSince, GetTickCount()))
	{
		case kHostWaitGoOn:
			return;

		case kHostWaitHostNow:
			hostWaiting = kHostWaitingForNothing;
			hostLine[0] = 0;

			// The host is tried with the wait's own start kept, so if it's refused for the server being asked again, the wait that follows
			// goes on counting from the first press
			hostWaitRetrySince = hostWaitSince;

			if (what == kHostWaitingToStart)
			{
				PressTheStartRowAgain();
			}
			else
			{
				SetupNewMenu(AVPMENU_COOP_HOST);
			}

			hostWaitRetrySince = 0;
			return;

		case kHostWaitCantUse:
			hostWaiting = kHostWaitingForNothing;
			ShowWhyTheHostFailed(hostWaitPage, why);
			return;

		case kHostWaitGiveUp:
			hostWaiting = kHostWaitingForNothing;
			ShowTheHostLine(hostWaitPage, GetTextString(TEXTSTRING_HOST_NO_ANSWER));
			return;
	}
}

// The line under the page's rows, on its own page only: two lines of the small font fit between the rows' end and the help line's top
static void DrawTheHostLine(void)
{
	if (AvPMenus.MenusState != MENUSSTATE_MAINMENUS || !hostLine[0] || AvPMenus.CurrentMenu != hostLinePage)
	{
		return;
	}

	RECT area = { MENU_LEFTXEDGE, MENU_BOTTOMYEDGE, MENU_RIGHTXEDGE, 420 };
	RenderSmallFontString_Wrapped(hostLine, &area, ONE_FIXED, 0, 0);
}

// hostrow=online (hudson_plan.h; athome went with the Host row, 2026-11-10): a Hudson host hosts the way a player does, for the owner's rehearsal of beta.2's
// screens (PO, 2026-11-10). Added 2026-11-10 by Melanikus (Third) for the AvP port. Each press is a row selected and Enter, as a player
// does, and one fixed line in the log (PressTheRowForARun):
// 1. the Multiplayer page, pictured (multiplayer.png);
// 2. Start game, which opens the game's page (through the yes page's way in, as every way online does), with the run's game on it,
//    pictured (host-config.png), and its Start row, which opens the species page (host-species.png);
// 3. that page's Start row. With a server that has answered, that hosts at once. With a server that hasn't
//    answered yet, the page waits for it as a player's does (AHostWaits, HostWaitFrame): "Asking the server..." is pictured
//    (host-asking.png), then either the row is pressed again for it and it hosts, or after 30 s the page says there was no answer
//    (host-gaveup.png) and nothing is hosted. Nothing is pressed while it waits, as leaving the page ends the wait.
// True once the game is hosted, and the menus are going on to the level. level is the game's place on its list, as the run chose it.
static bool HostThroughTheRowsForARun(int level)
{
	LoadAllAvPMenuGfx();
	AvPMenus.MenusState = MENUSSTATE_MAINMENUS;
	SetupNewMenu(AVPMENU_MULTIPLAYER);
	MenuSecondsForARun(1);

	// (Modified 2026-11-10 by Melanikus for the AvP port, the owner's ruling for beta.2: "Everything over nyc3": there's no Host row to set any more; every game is hosted
	// online.)
	MenuSecondsForARun(1);
	MenuFrameForARun("multiplayer.png");

	// A game over the network, never the game's "skirmish" against the computer, whose Start game opens another page (as
	// HostByItselfThroughTheRelay has it)
	netGameData.skirmishMode = FALSE;

	if (!PressTheRowForARun(AVPMENU_ELEMENT_GOTOMENU, AVPMENU_MULTIPLAYER_CONFIG, "Start game") || AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_CONFIG)
	{
		LogErrorString("hudson: Start game didn't open the game's page");
		ReleaseAllAvPMenuGfx();
		return false;
	}

	// The run's game, on the page whose rows show it, in case opening the page brought back another
	netGameData.levelNumber = level;
	MenuSecondsForARun(1);
	MenuFrameForARun("host-config.png");

	if (!PressTheRowForARun(AVPMENU_ELEMENT_GOTOMENU, AVPMENU_MULTIPLAYER_SPECIES_HOST, "Start, to the species page") || AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_SPECIES_HOST)
	{
		LogErrorString("hudson: the game's page's Start didn't open the species page");
		ReleaseAllAvPMenuGfx();
		return false;
	}

	MenuSecondsForARun(1);
	MenuFrameForARun("host-species.png");

	if (!PressTheRowForARun(AVPMENU_ELEMENT_STARTMPGAME, 0, "Start"))
	{
		ReleaseAllAvPMenuGfx();
		return false;
	}

	// The server's answer: hosted at once, or a wait on this page until it's hosted or the page gives up at 30 s (host_wait_rules.h). 40 s
	// is past both. The wait's own state says which: a line on the page with no wait going on is a host that wasn't hosted.
	DWORD const pressed = GetTickCount();
	bool askingPictured = false;
	bool notHosted = false;

	while (bRunning && AvPMenus.MenusState == MENUSSTATE_MAINMENUS && (DWORD)(GetTickCount() - pressed) < 40000u)
	{
		const char *picture = NULL;

		if (hostWaiting == kHostWaitingForNothing && hostLine[0])
		{
			notHosted = true;
			break;
		}

		if (!askingPictured && hostWaiting != kHostWaitingForNothing && (DWORD)(GetTickCount() - pressed) > 1000u)
		{
			LogString("hudson: the page says it's asking the server");
			picture = "host-asking.png";
			askingPictured = true;
		}

		// The page's picture isn't held while it waits (pagehold=): the wait is the page's own, and its 30 s are what's shown
		MenuFrameForARun(picture, false);
	}

	if (notHosted)
	{
		bool const noAnswer = strcmp(hostLine, GetTextString(TEXTSTRING_HOST_NO_ANSWER)) == 0;
		MenuSecondsForARun(2);
		MenuFrameForARun(noAnswer ? "host-gaveup.png" : "host-failed.png");
		LogErrorString(std::string("hudson: nothing was hosted, and the page says: ") + hostLine);
	}

	bool const hosted = AvPMenus.MenusState == MENUSSTATE_STARTGAME;

	if (!hosted && !notHosted)
	{
		LogErrorString("hudson: the host's Start row was pressed, and in 40 s neither hosted nor said why");
	}

	ReleaseAllAvPMenuGfx();
	return hosted;
}

extern void AvP_UpdateMenus(void)
{
	// A player asking to join this host: the page's two answer rows, in front of its own, while the request waits
	UpdateTheAnswerRows();

	// A host waiting for the server, and the line under the rows (added 2026-11-10, PO's rulings Q3 and Q4)
	HostWaitFrame();

	if (AvPMenus.CurrentMenu == AVPMENU_COOP_HOST || AvPMenus.CurrentMenu == AVPMENU_COOP_JOIN)
	{
		switch (CoopMenu_EveryFrame(AvPMenus.CurrentMenu == AVPMENU_COOP_HOST))
		{
			case kCoopPageGoesToTheLoad:
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				SetLevelToLoadForThisNetworkGame();
				SetBriefingTextForMultiplayer();
				break;

			case kCoopPageHostHasGone:
				CoopMenu_PageLeft();
				SetupNewMenu(AVPMENU_MAIN);
				return;

			case kCoopPageRowsChanged:
				CountTheRowsOnThisPage();
				break;

			case kCoopPageJoinerIsOut:
				SetupNewMenu(AVPMENU_COOP_OUT);
				return;

			default:
				break;
		}
	}
	else if (AvPMenus.CurrentMenu == AVPMENU_COOP_PLAYERS)
	{
		// Players come and go behind this page, so its rows are filled in each frame
		if (CoopMenu_PlayersPageFrame())
		{
			CountTheRowsOnThisPage();
		}
	}
	else if (AvPMenus.CurrentMenu == AVPMENU_ONLINE_ACCESS || AvPMenus.CurrentMenu == AVPMENU_ONLINE_ACCESS_NEW)
	{
		// The server's answer comes on the list's own thread, and the page shows it as it comes. Modified 2026-11-10 by Melanikus for the
		// AvP port: once the code is on the page, the page chooses Copy access code, so one Enter copies it (AccessPage_ChoosesCopy)
		int const choose = AccessPage_Frame(AvPMenus.MenuElements);

		if (choose >= 0 && choose < AvPMenus.NumberOfElementsInMenu)
		{
			AvPMenus.CurrentlySelectedElement = choose;
		}
	}
	else if (AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_SPECIES_JOIN ||
	    AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_CONFIG_JOIN)
	{
		MinimalNetCollectMessages();
		if (netGameData.myGameState == NGS_Error_HostLost)
		{
			// The host has gone, so there's no game to join. Back to the main menus.
			Net_Disconnect();
			AvP.Network = I_No_Network;
			SetupNewMenu(AVPMENU_MAIN);
			return;
		}

		if (AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_SPECIES_JOIN)
		{
			/* KJL 19:49:04 05/07/98 - ugh, there goes the interface */
			{
				GetNextAllowedSpecies(&MP_Species,TRUE);
				netGameData.myCharacterSubType=NGSCT_General;
				switch (MP_Species)
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

					case 3:	//various marine subtypes
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
						AvP.PlayerType = I_Marine;
						netGameData.myCharacterSubType = (NETGAME_SPECIALISTCHARACTERTYPE)ClassSlider_ToClass(MP_Species);
						break;
				}
			}
			AddNetMsg_PlayerDescription();

			NetSendMessages();
		}
	}
	else if (AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_SPECIES_HOST)
	{
		GetNextAllowedSpecies(&MP_Species,TRUE);
	}
	else if (AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_JOINING)
	{
		int retval = 0;

		// check status of joining game
		if (LobbiedGame)
		{
			retval = Net_ConnectingToLobbiedGame(MP_PlayerName);
			if (retval == NET_FAIL)
			{
				// player has aborted, go back a menu
				SetupNewMenu(AVPMENU_MULTIPLAYER_LOBBIEDCLIENT);
				return;
			}
		}
		else
		{
			retval = Net_ConnectingToSession();
			if (retval == NET_FAIL)
			{
				// The request to join's own end: the host said no, was answering someone else or didn't answer, or Esc while it waited.
				// Back to the games page, which says which, with that game selected again.
				if (!Net_WasRemovedByTheHost() && OnlinePage_JoinEnded())
				{
					SetupNewMenu(AVPMENU_MULTIPLAYERSELECTSESSION);
					return;
				}

				// player has aborted, go back a menu. Someone the host removed is told so.
				SetupNewMenu(Net_WasRemovedByTheHost() ? AVPMENU_COOP_OUT : AVPMENU_MULTIPLAYER);
				return;
			}
		}

		if (retval == AVPMENU_MULTIPLAYER_CONFIG_JOIN)
		{
			// successfully joined. A game from the co-op campaign page has its own page.
			SetupNewMenu(Net_CoopLobbyState() != kCoopLobbyNone ? AVPMENU_COOP_JOIN : AVPMENU_MULTIPLAYER_CONFIG_JOIN);
		}
	}
	else if (AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYERSELECTSESSION)
	{
		int selection = AvPMenus.CurrentlySelectedElement;
		bool listChanged = false;

		// The server's online list. Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over
		// nyc3"): it's the only one, as there's no search on this network any more.
		if (TheRelayGamesHaveCome())
		{
			listChanged = true;
		}

		// Every 5 s while the page is open, it asks again (online_screens.h's search, 12 times at most while it finds nothing): the list of
		// games found starts again empty, and the server's list is asked again
		if (OnlinePage_TimeToAskAgain())
		{
			Net_FindAvPSessions();
			RelayList_FindGames();
			listChanged = true;
		}

		// After a search, which may have emptied the list, the relay's games go back on it
		if (listChanged)
		{
			PutTheRelayGamesOnThePage();
		}

		// The rows shown, kept steady (online_screens_page.h). When they change, the page's rows are made again, with the selection on the
		// same game, still lit. The page isn't set up again, as that would start another search of the LAN.
		selection = AvPMenus.CurrentlySelectedElement;

		if (OnlinePage_Frame(listChanged, &selection))
		{
			MakeSelectSessionMenu();
			CountTheRowsOnThisPage();
			AvPMenus.CurrentlySelectedElement = selection < AvPMenus.NumberOfElementsInMenu ? selection : AvPMenus.NumberOfElementsInMenu - 1;
			AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement].Brightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
		}
	}

	/* render menu; episode select goes through a separate system */
	switch (AvPMenus.CurrentMenu)
	{
		case AVPMENU_LOADGAME:
		case AVPMENU_SAVEGAME:
		{
			RenderLoadGameMenu();
			RenderHelpString();
			break;
		}

		case AVPMENU_MARINELEVELS:
		case AVPMENU_PREDATORLEVELS:
		case AVPMENU_ALIENLEVELS:
		{
			RenderEpisodeSelectMenu();
			break;
		}
		case AVPMENU_USERPROFILESELECT:
		{
			RenderUserProfileSelectMenu();
			RenderHelpString();
			break;
		}
		case AVPMENU_USERPROFILEDELETE:
		{
			int i;
			AVP_USER_PROFILE *profilePtr = GetFirstUserProfile();
			for (i=0; i<UserProfileNumber; i++)
			{
				profilePtr = GetNextUserProfile();
			}

			RenderMenuText(profilePtr->Name,MENU_CENTREX,MENU_CENTREY-100,ONE_FIXED,AVPMENUFORMAT_CENTREJUSTIFIED);
			{
				char buffer2[100];

#ifdef _WIN32
				MakeTimeText(profilePtr->TimeLastUpdated, DATE_LONGDATE, buffer2, sizeof(buffer2));
#endif
#ifdef _XBOX
				// manually format the string as we don't have either GetDateFormat or GetTimeFormat
				sprintf(buffer2, "%02d:%02d:%02d %02d/%02d/%02d",
							profilePtr->TimeLastUpdated.wHour,
							profilePtr->TimeLastUpdated.wMinute,
							profilePtr->TimeLastUpdated.wSecond,
							profilePtr->TimeLastUpdated.wDay,
							profilePtr->TimeLastUpdated.wMonth,
							profilePtr->TimeLastUpdated.wYear
						);

#endif
				RenderSmallMenuText(buffer2, MENU_CENTREX, MENU_CENTREY-70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
			}
			
			RenderMenu();
			RenderHelpString();
			break;
		}

		case AVPMENU_MULTIPLAYER_DELETECONFIG :
		{
			//show the name of the configuration we're trying to delete
			RenderMenuText((char*)(MultiplayerConfigurationName), MENU_CENTREX,MENU_CENTREY-100, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

			RenderMenu();
			RenderHelpString();
			break;
		}

		case AVPMENU_ALIENKEYCONFIG:
		case AVPMENU_MARINEKEYCONFIG:
		case AVPMENU_PREDATORKEYCONFIG:
		{
			CheckForKeysWithMultipleAssignments();
			RenderKeyConfigurationMenu();

			if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
			{
				RenderHelpString();
			}

			break;
		}
		case AVPMENU_SKIRMISH_CONFIG:
		case AVPMENU_MULTIPLAYER_CONFIG:
		case AVPMENU_MULTIPLAYER_CONFIG_JOIN:
		{
			UpdateMultiplayerConfigurationMenu();
			RenderScrollyMenu();
			RenderHelpString();
			break;
		}
		case AVPMENU_MULTIPLAYEROPENADDRESS:
		{
			RenderMenu();
			RenderHelpString();
			break;
		}

		case AVPMENU_MULTIPLAYER_LOADIPADDRESS:
		{
			RenderScrollyMenu();
			break;
		}

		case AVPMENU_MULTIPLAYER_LOADCONFIG:
		{
			RenderScrollyMenu();
			RenderConfigurationDescriptionString();
			RenderHelpString();
			break;
		}
		case AVPMENU_INGAMEAVOPTIONS:
		{
			int scale = DIV_FIXED(480, 480);
			int h = scale/2048;
			int offset = scale/4096;
			D3D_DrawColourBar(offset, offset+h,  ONE_FIXED, 0, 0);
			D3D_DrawColourBar(offset*2+h, offset*2+h*2,  0, ONE_FIXED, 0);
			D3D_DrawColourBar(480-offset*2-h*2, 480-offset*2-h,  0, 0, ONE_FIXED);
			D3D_DrawColourBar(480-offset-h, 480-offset, ONE_FIXED, ONE_FIXED, ONE_FIXED);

#if 0
			int scale = DIV_FIXED(ScreenDescriptorBlock.SDB_Height, 480);
			int h = scale/2048;
			int offset = scale/4096;
			D3D_DrawColourBar(offset, offset+h,  ONE_FIXED, 0, 0);
			D3D_DrawColourBar(offset*2+h, offset*2+h*2,  0, ONE_FIXED, 0);
			D3D_DrawColourBar(ScreenDescriptorBlock.SDB_Height-offset*2-h*2, ScreenDescriptorBlock.SDB_Height-offset*2-h,  0, 0, ONE_FIXED);
			D3D_DrawColourBar(ScreenDescriptorBlock.SDB_Height-offset-h, ScreenDescriptorBlock.SDB_Height-offset, ONE_FIXED, ONE_FIXED, ONE_FIXED);
#endif
			RenderMenu();
			break;
		}

		case AVPMENU_CHEATOPTIONS:
		{
			TestValidityOfCheatMenu();
			RenderMenu();
			RenderHelpString();
			break;
		}
		default:
		{
			RenderMenu();
			RenderHelpString();
			break;
		}
	}
	ActUponUsersInput();
}

// How many rows this page has, and how high they are. The co-op campaign page asks again when someone comes or goes, and the row the host is on
// might have been that player's.
static void CountTheRowsOnThisPage(void)
{
	AVPMENU_ELEMENT *elementPtr = AvPMenus.MenuElements;
	AvPMenus.NumberOfElementsInMenu=0;
	AvPMenus.MenuHeight=0;
	do
	{
		elementPtr->Brightness = BRIGHTNESS_OF_DARKENED_ELEMENT;

		AvPMenus.MenuHeight += HeightOfMenuElement(elementPtr);

		AvPMenus.NumberOfElementsInMenu++;
		elementPtr++;
	}
	while (elementPtr->ElementID != AVPMENU_ELEMENT_ENDOFMENU);

	if (AvPMenus.CurrentlySelectedElement >= AvPMenus.NumberOfElementsInMenu)
	{
		AvPMenus.CurrentlySelectedElement = AvPMenus.NumberOfElementsInMenu - 1;
	}
}

static void SetupNewMenu(enum AVPMENU_ID menuID)
{
	enum AVPMENU_ID previousMenuID = AvPMenus.CurrentMenu;
	AvPMenus.CurrentMenu = menuID;

	// Leaving the page of games found. Nothing else would close what the page has open on the network.
	if (previousMenuID == AVPMENU_MULTIPLAYERSELECTSESSION && menuID != AVPMENU_MULTIPLAYERSELECTSESSION)
	{
		// And the page's own timing of the server's relay, for its ping column, with its socket (added 2026-11-10 by Melanikus for the AvP
		// port; networking.h)
		Net_StopTimingTheRelay();
	}

	/* set pointer to the start of the menu's element data */
	AvPMenus.MenuElements = AvPMenusData[menuID].MenuElements; // could use default

	AvPMenus.FontToUse = AvPMenusData[menuID].FontToUse;
	AvPMenus.CurrentlySelectedElement = 0;
	AvPMenus.UserEnteringText = 0;
	AvPMenus.UserEnteringNumber = 0;
	AvPMenus.UserChangingKeyConfig = 0;
	AvPMenus.UserChangingViewKey = 0;
	AvPMenus.PositionInTextField = 0;
	AvPMenus.WidthLeftForText = 0;

	/* menu specific stuff */
	switch (menuID)
	{
		case AVPMENU_MAIN:
		{
			GetSettingsFromUserProfile();
			SaveUserProfile(UserProfilePtr);

			if (DebuggingCommandsActive)
			{
				if (AnyCheatModesAllowed())
				{
					// turn on cheats
					SetupNewMenu(AVPMENU_DEBUG_MAIN_WITHCHEATS);
				}
				else
				{
					SetupNewMenu(AVPMENU_DEBUG_MAIN);
				}
			}
			else
			{
				if (AnyCheatModesAllowed())
				{
					// turn on cheats
					SetupNewMenu(AVPMENU_MAIN_WITHCHEATS);
				}
			}
			break;
		}

		case AVPMENU_USERPROFILESELECT:
		{
			InitialiseGammaSettings(128);
			EpisodeSelectScrollOffset=0;
			ExamineSavedUserProfiles();
			MakeUserProfileSelectMenu();
			break;
		}
		case AVPMENU_USERPROFILEENTERNAME:
		{
			AvPMenus.UserEnteringText = 1;
			KeyboardEntryQueue_Clear();
			AvPMenus.MenuElements->TextPtr = UserProfilePtr->Name;
			UserProfilePtr->Name[0] = 0;
			AvPMenus.WidthLeftForText = 0; //will be calculated properly when menus are drawn
			break;
		}
		case AVPMENU_MULTIPLAYER_SAVECONFIG:
		{
			AvPMenus.UserEnteringText = 1;
			KeyboardEntryQueue_Clear();
			AvPMenus.WidthLeftForText = 0; //will be calculated properly when menus are drawn
			break;
		}

		case AVPMENU_SINGLEPLAYER:
		{
			break;
		}

		case AVPMENU_COOP_HOST:
		{
			// This page opens the game. If that can't be done, there's no page, and the Multiplayer page says why (modified 2026-11-10 by
			// Melanikus for the AvP port, PO's ruling Q4)
			if (!CoopMenu_HostPageOpens())
			{
				const char *const why = Net_HostFailedWhy();
				SetupNewMenu(AVPMENU_MULTIPLAYER);

				// A server being asked again (the host asked it afresh) is waited for there, as before a first answer (Third's finding)
				if (!AHostWaits(kHostWaitingToOpenCoop))
				{
					ShowWhyTheHostFailed(AVPMENU_MULTIPLAYER, why);
				}
				return;
			}
			break;
		}

		case AVPMENU_COOP_JOIN:
		{
			CoopMenu_JoinPageOpens();
			break;
		}

		case AVPMENU_COOP_OUT:
		{
			// Nothing to say, so no page
			if (!CoopMenu_OutPageOpens())
			{
				SetupNewMenu(AVPMENU_MULTIPLAYER);
				return;
			}
			break;
		}

		case AVPMENU_COOP_PLAYERS:
		{
			CoopMenu_PlayersPageFrame();
			break;
		}

		case AVPMENU_CUSTOMMAPS:
		{
			// Found afresh each time the page opens, so a map added while the game runs shows up
			CustomMaps_Find();

			customMapRowNames.clear();

			for (int i = 0; i < CustomMaps_Count(); i++)
			{
				customMapRowNames.push_back(const_cast<char *>(CustomMaps_Name(i)));
			}

			if (customMapRowNames.empty())
			{
				customMapRowNames.push_back(GetTextString(TEXTSTRING_CUSTOMMAPS_NONE));
			}

			AVPMENU_ELEMENT *const mapRow = &AvPMenus.MenuElements[0];
			mapRow->TextSliderStringPointer = customMapRowNames.data();
			mapRow->MaxSliderValue = (int)customMapRowNames.size() - 1;

			// The last choice stays, if it's still one of them
			if (CustomMapToPlay < 0 || CustomMapToPlay > mapRow->MaxSliderValue)
			{
				CustomMapToPlay = 0;
			}

			if (CustomMapSpecies < 0 || CustomMapSpecies > 2)
			{
				CustomMapSpecies = 0;
			}

			if (CustomMapDifficulty < 0 || CustomMapDifficulty > 2)
			{
				CustomMapDifficulty = 1;
			}
			break;
		}
	
		case AVPMENU_MARINELEVELS:
		{
			MarineEpisodeToPlay=0;
			EpisodeSelectScrollOffset=0;
			AvPMenus.MenuElements->MaxSliderValue = NumberOfAvailableLevels(I_Marine);
			*AvPMenus.MenuElements->SliderValuePtr = LevelMostLikelyToPlay(I_Marine);
			break;
		}
		case AVPMENU_PREDATORLEVELS:
		{
			PredatorEpisodeToPlay=0;
			EpisodeSelectScrollOffset=0;
			AvPMenus.MenuElements->MaxSliderValue = NumberOfAvailableLevels(I_Predator);
			*AvPMenus.MenuElements->SliderValuePtr = LevelMostLikelyToPlay(I_Predator);
			break;
		}
		case AVPMENU_ALIENLEVELS:
		{
			AlienEpisodeToPlay=0;
			EpisodeSelectScrollOffset=0;
			AvPMenus.MenuElements->MaxSliderValue = NumberOfAvailableLevels(I_Alien);
			*AvPMenus.MenuElements->SliderValuePtr = LevelMostLikelyToPlay(I_Alien);
			break;
		}
		case AVPMENU_MULTIPLAYERSELECTSESSION:
		{
			// The list of games found starts again empty, for the online list (nothing is sent on this network)
			Net_FindAvPSessions();

			if (previousMenuID != AVPMENU_MULTIPLAYERSELECTSESSION)
			{
				// save ip address (if it has been set)
				SaveIPAddress(IP_Address_Name, IPAddressString);
				Net_JoinGame();

				// The relay server's games as well, when the page is opened, and then every 5 s from its frame (OnlinePage_TimeToAskAgain),
				// never on each answer. A joiner needs the relays too, to reach a game's region, and to time them for the ping column.
				OnlinePage_Opened();
				relayGamesKept = 0;
				RelayList_FindRelays();
				RelayList_FindGames();
			}

			// After the LAN's search, which may have emptied the list, and before the page's rows are made from it
			PutTheRelayGamesOnThePage();
			MakeSelectSessionMenu();
			break;
		}

		case AVPMENU_MULTIPLAYER_SKIRMISH:
		{
			netGameData.skirmishMode = TRUE;
			LoadMultiplayerConfiguration(GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME));
			MP_Config_Description[0] = 0;
			break;
		}

		case AVPMENU_MULTIPLAYER_CONNECTION:
		{
			//skirmishMode must be false
			netGameData.skirmishMode = FALSE;

			LoadMultiplayerConfiguration(GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME));
			MP_Config_Description[0] = 0;

			if (LobbiedGame)
			{
				//use alternative multiplayer menus for lobbied games
				if (LobbiedGame == LobbiedGame_Server)
				{
					SetupNewMenu(AVPMENU_MULTIPLAYER_LOBBIEDSERVER);
				}
				else
				{
					SetupNewMenu(AVPMENU_MULTIPLAYER_LOBBIEDCLIENT);
				}
				return;
			}

			SetupNewMenu(AVPMENU_MULTIPLAYER);
			break;
		}

		case AVPMENU_INGAME:
		case AVPMENU_INNETGAME:
		case AVPMENU_INCOOPGAME:
		{
			if (AvP.Network != I_No_Network)
			{
				// in a multiplayer game set up a menu without the restart mission option. A campaign's has a Players row.
				menuID = SameAsSinglePlayer() ? AVPMENU_INCOOPGAME : AVPMENU_INNETGAME;

				if (menuID == AVPMENU_INCOOPGAME)
				{
					CoopMenu_InGameMenuOpens();
				}

				AvPMenus.CurrentMenu = menuID;
				AvPMenus.MenuElements = AvPMenusData[menuID].MenuElements; // could use default
				AvPMenus.FontToUse = AvPMenusData[menuID].FontToUse;
			}
			MakeInGameMenu();
			break;
		}
		case AVPMENU_MARINEKEYCONFIG:
		{
			MakeMarineKeyConfigMenu();
			PlayerInputPrimaryConfig = MarineInputPrimaryConfig;
			PlayerInputSecondaryConfig = MarineInputSecondaryConfig;
			EpisodeSelectScrollOffset=0;
			KeyConfigSelectionColumn = 0;
			break;
		}
		case AVPMENU_PREDATORKEYCONFIG:
		{
			MakePredatorKeyConfigMenu();
			PlayerInputPrimaryConfig = PredatorInputPrimaryConfig;
			PlayerInputSecondaryConfig = PredatorInputSecondaryConfig;
			EpisodeSelectScrollOffset=0;
			KeyConfigSelectionColumn = 0;
			break;
		}
		case AVPMENU_ALIENKEYCONFIG:
		{
			MakeAlienKeyConfigMenu();
			PlayerInputPrimaryConfig = AlienInputPrimaryConfig;
			PlayerInputSecondaryConfig = AlienInputSecondaryConfig;
			EpisodeSelectScrollOffset=0;
			KeyConfigSelectionColumn = 0;
			break;
		}
		case AVPMENU_CONTROLS:
		{
			PlayerControlMethods = ControlMethods;
			break;
		}
		case AVPMENU_JOYSTICKCONTROLS:
		{
			PlayerJoystickControlMethods = JoystickControlMethods;
			break;
		}

		case AVPMENU_MULTIPLAYERJOINGAME:
		{
			strcpy(IPAddressString, CommandLineIPAddressString);
			IP_Address_Name[0] = 0;
			break;
		}

		case AVPMENU_MULTIPLAYER:
		{
			// The page's rows (menu_data.cpp, MenuData_SetMultiplayerRows): the Online access row is there when servers.cfg names a server
			// (modified 2026-11-10 by Melanikus for the AvP port, beta.2: it was -dev only for beta.1). They're set before the page's rows
			// are counted.
			MenuData_SetMultiplayerRows(Relay_ServerName()[0] != 0);

			// Where each region's relay is, so that a game hosted from here can go through one. The answer takes a moment, and it's
			// asked for here, before the host page, so it's there by the time a game is hosted. With no relay server set, it does nothing.
			RelayList_FindRelays();

			//skirmishMode must be false
			netGameData.skirmishMode = FALSE;

			LoadMultiplayerConfiguration(GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME));
			MP_Config_Description[0] = 0;

			if (LobbiedGame)
			{
				//use alternative multiplayer menus for lobbied games
				if (LobbiedGame == LobbiedGame_Server)
				{
					SetupNewMenu(AVPMENU_MULTIPLAYER_LOBBIEDSERVER);
				}
				else
				{
					SetupNewMenu(AVPMENU_MULTIPLAYER_LOBBIEDCLIENT);
				}
				return;
			}
			break;
		}

		case AVPMENU_ONLINE_ACCESS:
		{
			// Only when servers.cfg names a server. Without one nothing leads here, and if anything ever did, it goes straight back.
			if (!AccessPage_Opened(AvPMenus.MenuElements))
			{
				SetupNewMenu(AVPMENU_MULTIPLAYER);
				return;
			}
			break;
		}

		case AVPMENU_ONLINE_ACCESS_NEW:
		{
			if (!AccessPage_QuestionOpened())
			{
				SetupNewMenu(AVPMENU_MULTIPLAYER);
				return;
			}
			break;
		}

		// The player's yes to the server in servers.cfg (#175's last part, server_yes_page.h): with no line waiting for one, back to the
		// Multiplayer page
		case AVPMENU_SERVER_YES:
		{
			if (!ServerYesPage_Opened())
			{
				SetupNewMenu(AVPMENU_MULTIPLAYER);
				return;
			}
			break;
		}

		// Every servers.cfg line, and the player's pick (#175 2b, servers_page.h): its rows are written before they're counted. Added
		// 2026-11-10 by Melanikus for the AvP port.
		case AVPMENU_SERVERS:
		{
			if (!ServersPage_Opened(AvPMenus.MenuElements, kServersPageRowRoom))
			{
				SetupNewMenu(AVPMENU_MULTIPLAYER);
				return;
			}
			break;
		}

		case AVPMENU_MULTIPLAYER_LOADCONFIG:
		{
			if (!BuildLoadMPConfigMenu())
			{
				SetupNewMenu(AVPMENU_MULTIPLAYER_CONFIG);
				return;
			}
			AvPMenus.MenuElements = AvPMenu_Multiplayer_LoadConfig;
			break;
		}

		case AVPMENU_MULTIPLAYER_CONFIG:
		{
			//need to set the menu we return to , according to whether this is
			//a lobbied game or not.
			if (LobbiedGame)
			{
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu=AVPMENU_MULTIPLAYER_LOBBIEDSERVER;
			}
			else if (netGameData.skirmishMode)
			{
				//for skirmish games , use skirmish config menu instead
				SetupNewMenu(AVPMENU_SKIRMISH_CONFIG);
				return;
			}
			else
			{
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu=AVPMENU_MULTIPLAYER;
			}
			break;
		}

		case AVPMENU_MULTIPLAYER_CONFIG_JOIN:
		{
			// need to set the menu we return to, according to whether this is
			// a lobbied game or not.
			if (LobbiedGame)
			{
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu = AVPMENU_MULTIPLAYER_LOBBIEDCLIENT;
			}
			else
			{
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu = AVPMENU_MULTIPLAYER;
			}
			break;
		}

		case AVPMENU_MULTIPLAYER_LOADIPADDRESS:
		{
			AvPMenus.MenuElements = AvPMenu_Multiplayer_LoadIPAddress;
			break;
		}

		case AVPMENU_MULTIPLAYEROPENADDRESS:
		{

			MakeOpenIPAddressMenu();
			break;
		}
	
		case AVPMENU_SAVEGAME:
		case AVPMENU_LOADGAME:
		{
			ScanSaveSlots();
			break;
		}

		default:
			break;
	}

	CountTheRowsOnThisPage();

	switch (menuID)
	{
		case AVPMENU_CHEATOPTIONS:
		{
			CheatMode_Active = 0;
			break;
		}
		
		case AVPMENU_MARINEKEYCONFIG:
		case AVPMENU_ALIENKEYCONFIG:
		case AVPMENU_PREDATORKEYCONFIG:
			AvPMenus.MenuHeight += 50;
			break;
		case AVPMENU_LEVELBRIEFING_BASIC:
		{
			int episodeToPlay;
			switch (AvP.PlayerType)
			{
				case I_Marine:
				{
					episodeToPlay = MarineEpisodeToPlay;
					break;
				}
				case I_Predator:
				{
					episodeToPlay = PredatorEpisodeToPlay;
					break;
				}
				case I_Alien:
				{
					episodeToPlay = AlienEpisodeToPlay;
					break;
				}
			}

			/* find available difficulty levels */
			AvPMenus.NumberOfElementsInMenu = MaxDifficultyLevelAllowed(AvP.PlayerType, episodeToPlay);

			/* highlight a suitable level of difficulty */
			if (episodeToPlay)
			{
				AvPMenus.CurrentlySelectedElement = AvPMenus.NumberOfElementsInMenu-1;
			}
			else // default to medium difficulty for first level
			{
				AvPMenus.CurrentlySelectedElement = 1;
			}

			SetBriefingTextForEpisode(episodeToPlay, AvP.PlayerType);

			break;
		}
		case AVPMENU_LEVELBRIEFING_BONUS:
		{
			int episodeToPlay;
			switch (AvP.PlayerType)
			{
				case I_Marine:
				{
					episodeToPlay = MarineEpisodeToPlay;
					break;
				}
				case I_Predator:
				{
					episodeToPlay = PredatorEpisodeToPlay;
					break;
				}
				case I_Alien:
				{
					episodeToPlay = AlienEpisodeToPlay;
					break;
				}
			}

			SetBriefingTextForEpisode(episodeToPlay, AvP.PlayerType);
			break;
		}
		default:
			SetBriefingTextToBlank();
			break;
	}
}


static void RenderMenu(void)
{
	AVPMENU_ELEMENT *elementPtr = AvPMenus.MenuElements;

	int y;
	texID_t coopPicture = MISSING_TEXTURE;
	
	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
	{
		if    ((AvPMenus.CurrentMenu == AVPMENU_LEVELBRIEFING_BASIC)
			|| (AvPMenus.CurrentMenu == AVPMENU_LEVELBRIEFING_BONUS))
		{
			y = MENU_BOTTOMYEDGE - AvPMenus.MenuHeight;
			RenderBriefingScreenInfo();
		}
		else if (AvPMenus.CurrentMenu == AVPMENU_COOP_HOST || AvPMenus.CurrentMenu == AVPMENU_COOP_JOIN)
		{
			// These pages have a picture, and can have more rows than fit under it
			texID_t const picture = CoopMenu_Picture();
			CoopPageLayout const layout = CoopPage_Layout(AvPMenus.MenuHeight, MENU_CENTREY, picture != MISSING_TEXTURE ? kMenuPictureTop + HeightOfMenuGfx(picture) : 0);

			y = layout.rowsTop;
			coopPicture = layout.showPicture ? picture : MISSING_TEXTURE;
		}
		else if (AvPMenus.CurrentMenu == AVPMENU_ONLINE_ACCESS)
		{
			// Modified 2026-11-10 by Melanikus for the AvP port (the owner's picture: the code was drawn over the title, with an empty gap
			// above the rows): the Online access pages draw their own parts under the title, and their rows start right under those
			// (access_page_rules.h, "Where the page's parts go")
			y = AccessPage_DrawTop(AvPMenus.MenuHeight, AvPMenus.NumberOfElementsInMenu);
		}
		else if (AvPMenus.CurrentMenu == AVPMENU_ONLINE_ACCESS_NEW)
		{
			y = AccessPage_DrawQuestion(AvPMenus.MenuHeight, AvPMenus.NumberOfElementsInMenu);
		}
		else
		{
			y = MENU_CENTREY - AvPMenus.MenuHeight / 2;
		}
	}
	else // in game menus
	{
//		y = (ScreenDescriptorBlock.SDB_Height - AvPMenus.MenuHeight) / 2;
		y = (480 - AvPMenus.MenuHeight) / 2;
	}

	// The page of games found is drawn by the online-play screens (online_screens_page.h): its column headings, or the tail and the search's
	// words while it has no games, then each game as a row of the table
	bool const gamesPage = AvPMenus.MenusState == MENUSSTATE_MAINMENUS && AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYERSELECTSESSION;

	if (gamesPage)
	{
		OnlinePage_DrawTop();
	}

	// The Servers page draws its own rows, a screenful at a time, and its words around them (servers_page.h; #175 2b, added 2026-11-10)
	bool const serversPage = AvPMenus.MenusState == MENUSSTATE_MAINMENUS && AvPMenus.CurrentMenu == AVPMENU_SERVERS;

	if (serversPage)
	{
		ServersPage_DrawTop(AvPMenus.CurrentlySelectedElement);
	}

	// The yes page's words, above its rows (#175's last part). The Online access pages draw their own parts as RenderMenu places their rows,
	// above (access_page_rules.h)
	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS && AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)
	{
		// The rows' top, as RenderMenu places them on this page (modified 2026-11-10: so its words stay above them)
		ServerYesPage_Draw(MENU_CENTREY - AvPMenus.MenuHeight / 2);
	}

	// A host's "Asking the server...", or why it failed, under the rows of the page it was pressed on (added 2026-11-10, PO's rulings)
	DrawTheHostLine();

	// The joining page, while this player's request to join waits for the host (J4), or once the host has said yes: the tail and its words
	// instead of the page's own row
	bool const askingPage = AvPMenus.MenusState == MENUSSTATE_MAINMENUS && AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_JOINING && OnlinePage_DrawAsking();

	// A player asking to join this host: who, and how long is left, above the two answer rows (UpdateTheAnswerRows)
	if (answerRowsShown && AvPMenus.MenuElements == answerRows)
	{
		RenderSmallMenuText(askingLine, MENU_CENTREX, 40, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

		// And, through a relay, the check code for the two players to read to each other (CRYPTO.md section 8)
		if (checkCodeLine[0])
		{
			RenderSmallMenuText(checkCodeLine, MENU_CENTREX, 58, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

			// Modified 2026-11-10 by Melanikus for the AvP port (the owner's showcase pictures: the two lines were drawn on each other, and
			// ran almost edge to edge): wrapped and spaced, and never past the answer rows, whose top is y (online_screens.h)
			OnlinePage_DrawCheckCodeHelp(OnlineScreens_Words(kOnlineWordsCheckCodeMeansHost), OnlineScreens_Words(kOnlineWordsCheckCodeSkipped),
				58, y - kOnlineHelpGap);
		}
	}

	// The relay's news on the co-op lobby page, where the host waits before play (H2): for a few seconds after it comes. Modified
	// 2026-11-09 by Melanikus for the AvP port: while a join request's rows are up it waits, and is shown once they've gone, so it never
	// covers the check code drawn above them (it used to be drawn at y 56, over the code at 58)
	char relayNews[128];
	bool const answering = answerRowsShown && AvPMenus.MenuElements == answerRows;

	if (AvPMenus.CurrentMenu == AVPMENU_COOP_HOST && OnlineHost_NewsLine(relayNews, sizeof(relayNews), answering))
	{
		RenderSmallMenuText(relayNews, MENU_CENTREX, 40, ONE_FIXED / 2, AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	// The Online access pages: their rows a little further apart, and a row not chosen brighter than the menus' quarter, so it's read
	// against the starfield (modified 2026-11-10 by Melanikus for the AvP port: the owner couldn't find Copy access code; access_page_rules.h)
	bool const accessPage = AvPMenus.MenusState == MENUSSTATE_MAINMENUS &&
		(AvPMenus.CurrentMenu == AVPMENU_ONLINE_ACCESS || AvPMenus.CurrentMenu == AVPMENU_ONLINE_ACCESS_NEW);

	for (int e = 0; e < AvPMenus.NumberOfElementsInMenu; e++, elementPtr++)
	{
		int targetBrightness;

		if (e==AvPMenus.CurrentlySelectedElement)
		{
			targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
		}
		else if (accessPage)
		{
			targetBrightness = ONE_FIXED * kAccessPageRowNotChosenOutOf100 / 100;
		}
		else
		{
			targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
		}

		if (targetBrightness > elementPtr->Brightness)
		{
			elementPtr->Brightness += BRIGHTNESS_CHANGE_SPEED;
			if (elementPtr->Brightness>targetBrightness)
			{
				elementPtr->Brightness = targetBrightness;
			}
		}
		else
		{
			elementPtr->Brightness -= BRIGHTNESS_CHANGE_SPEED;
			if (elementPtr->Brightness < targetBrightness)
			{
				elementPtr->Brightness = targetBrightness;
			}
		}

		if (gamesPage)
		{
			// A game's row is drawn by the page, in its table. On the list, the Search box and the Refresh row are the menus' own rows, each
			// drawn at its place above the table. On the fetching screen, its one row draws nothing: the page draws the words.
			if (elementPtr->ElementID == AVPMENU_ELEMENT_LISTCHOICE)
			{
				OnlinePage_DrawRow(e, elementPtr->Brightness);
			}
			else if (OnlinePage_ShowsTheList())
			{
				RenderMenuElement(elementPtr, e, OnlinePage_ControlTop(e));
			}
		}
		else if (serversPage)
		{
			ServersPage_DrawRow(elementPtr, e, elementPtr->Brightness, AvPMenus.CurrentlySelectedElement);
		}
		else if (!askingPage)
		{
			RenderMenuElement(elementPtr,e,y);
		}

		y += HeightOfMenuElement(elementPtr) + (accessPage ? kAccessPageRowsApart : 0);
	}

	// Render Menu Title
	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
	{
		char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
		RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

		if (AvPMenus.CurrentMenu == AVPMENU_CUSTOMMAPS)
		{
			RenderCustomMapPicture();
		}
		else if (coopPicture != MISSING_TEXTURE)
		{
			DrawAvPMenuGfx(coopPicture, MENU_CENTREX, kMenuPictureTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
		}
	}
}

// The chosen custom map's own picture, under the title. A map with none shows nothing there. custom_maps.cpp keeps a picture
// small enough to fit above the rows.
static void RenderCustomMapPicture(void)
{
	texID_t const picture = CustomMaps_Picture(CustomMapToPlay);

	if (picture != MISSING_TEXTURE)
	{
		DrawAvPMenuGfx(picture, MENU_CENTREX, kMenuPictureTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
}

static void RenderBriefingScreenInfo(void)
{
	enum TEXTSTRING_ID textID;
	switch (AvP.PlayerType)
	{
		case I_Marine:
		{
			textID = static_cast<enum TEXTSTRING_ID>(MarineEpisodeToPlay+TEXTSTRING_MARINELEVELS_1);
			break;
		}
		case I_Predator:
		{
			textID = static_cast<enum TEXTSTRING_ID>(PredatorEpisodeToPlay+TEXTSTRING_PREDATORLEVELS_1);
			break;
		}
		case I_Alien:
		{
			textID = static_cast<enum TEXTSTRING_ID>( AlienEpisodeToPlay+TEXTSTRING_ALIENLEVELS_1);
			break;
		}
	}
	RenderMenuText(GetTextString(textID),MENU_LEFTXEDGE,120,ONE_FIXED,AVPMENUFORMAT_LEFTJUSTIFIED);

	RenderBriefingText(/*ScreenDescriptorBlock.SDB_Height*/480 / 2, ONE_FIXED);
}

/* KJL 12:11:18 24/09/98 - specialised code to handle episode selection screen, which
has features which make it too awkward to add to the general system */
static void RenderEpisodeSelectMenu(void)
{
	AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];
	int currentEpisode = *(elementPtr->SliderValuePtr);
	int centrePosition = (currentEpisode)*65536+EpisodeSelectScrollOffset;
	texID_t graphicID;
	I_PLAYER_TYPE playerID;
	int i;
	int numberOfBasicLevels;
	texID_t correctGraphicID;
	
	switch (AvPMenus.CurrentMenu)
	{
		default:
		{
			LOCALASSERT(0);/* Panic */
		}
		case AVPMENU_MARINELEVELS:
		{
			graphicID = AVPMENUGFX_MARINE_EPISODE1;
			numberOfBasicLevels = MAX_NO_OF_BASIC_MARINE_EPISODES;
			playerID = I_Marine;
			break;
		}
		case AVPMENU_PREDATORLEVELS:
		{
			graphicID = AVPMENUGFX_PREDATOR_EPISODE1;
			numberOfBasicLevels = MAX_NO_OF_BASIC_PREDATOR_EPISODES;
			playerID = I_Predator;
			break;
		}
		case AVPMENU_ALIENLEVELS:
		{
			graphicID = AVPMENUGFX_ALIEN_EPISODE1;
			numberOfBasicLevels = MAX_NO_OF_BASIC_ALIEN_EPISODES;
			playerID = I_Alien;
			break;
		}
	}
	
	// render menu title
	char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
	RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	for (i=0; i<=elementPtr->MaxSliderValue; i++)
	{
		int y;

		correctGraphicID = graphicID;

		y = MUL_FIXED(i*65536-centrePosition,100);

		if (y>=-150 && y<=150)
		{
			char *textPtr = GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription+i));
			int b;
			int targetBrightness;
			
			if (i==currentEpisode)
			{
				targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
			}
			else
			{
				targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
			}

			if (targetBrightness > BrightnessOfRow(i))
			{
				BrightnessOfRow(i) += BRIGHTNESS_CHANGE_SPEED;
				if (BrightnessOfRow(i) > targetBrightness)
				{
					BrightnessOfRow(i) = targetBrightness;
				}
			}
			else
			{
				BrightnessOfRow(i) -= BRIGHTNESS_CHANGE_SPEED;
				if (BrightnessOfRow(i) < targetBrightness)
				{
					BrightnessOfRow(i) = targetBrightness;
				}
			}

			b=BrightnessOfRow(i);
			{
				int yCoord = MENU_CENTREY+y-60;

				/* 
				 * bjd - hack to handle bonus level graphics with new texture system
				 * if 'i' is a bonus level index, just set the new adjusted graphic id
				 * to a bonus level image. otherwise, index like originally
				 */
				if (i >= numberOfBasicLevels)
				{
					correctGraphicID = AVPMENUGFX_MARINE_EPISODE7;
				}
				else
				{
					correctGraphicID += i;
				}

				RenderMenuText_Clipped(textPtr, MENU_LEFTXEDGE+150, yCoord, b, AVPMENUFORMAT_LEFTJUSTIFIED, MENU_CENTREY-60-100, MENU_CENTREY-60+180);
				DrawAvPMenuGfx_Clipped(correctGraphicID, MENU_LEFTXEDGE, yCoord, b, AVPMENUFORMAT_LEFTJUSTIFIED, MENU_CENTREY-60-100, MENU_CENTREY-60+180);

				if (MaximumSelectableLevel>=i)
				{
					char *completedTextPtr;

					if (i<numberOfBasicLevels)
					{
						completedTextPtr = GetTextString(static_cast<enum TEXTSTRING_ID>(TEXTSTRING_NOTYETCOMPLETED+UserProfilePtr->LevelCompleted[playerID][i]));
					}
					else
					{
						if (UserProfilePtr->LevelCompleted[playerID][i])
						{
							completedTextPtr = GetTextString(TEXTSTRING_COMPLETED);
						}
						else
						{
							completedTextPtr = GetTextString(TEXTSTRING_NOTYETCOMPLETED);
						}
					}
					
					RenderSmallMenuText
					(
						completedTextPtr,
						MENU_LEFTXEDGE+150,
						yCoord+30,
						b,
						AVPMENUFORMAT_LEFTJUSTIFIED
					);
				}
				else
				{
					if (i == elementPtr->MaxSliderValue)
					{
						RenderSmallMenuText
						(
							GetTextString(TEXTSTRING_NOTYETAVAILABLE_2),
							MENU_LEFTXEDGE+150,
							yCoord+30,
							b,
							AVPMENUFORMAT_LEFTJUSTIFIED
						);
					}
					else
					{
						RenderSmallMenuText
						(
							GetTextString(TEXTSTRING_NOTYETAVAILABLE_1),
							MENU_LEFTXEDGE+150,
							yCoord+30,
							b,
							AVPMENUFORMAT_LEFTJUSTIFIED
						);
					}
				}
				/*
					,MENU_CENTREY-60-100,
					MENU_CENTREY-60+180
				);*/
			}
		}
	}

	if (EpisodeSelectScrollOffset>0)
	{
		EpisodeSelectScrollOffset -= MUL_FIXED(EpisodeSelectScrollOffset*2+8192,RealFrameTime<<1);
		if (EpisodeSelectScrollOffset<0)
		{
			EpisodeSelectScrollOffset=0;
		}
	}
	else if (EpisodeSelectScrollOffset<0)
	{
		EpisodeSelectScrollOffset += MUL_FIXED(-EpisodeSelectScrollOffset*2+8192,RealFrameTime<<1);
		if (EpisodeSelectScrollOffset>0)
		{
			EpisodeSelectScrollOffset=0;
		}
	}
}

static void RenderKeyConfigurationMenu(void)
{
	AVPMENU_ELEMENT *elementPtr = AvPMenus.MenuElements;
	int centrePosition;
	int i;
	int y;
	int centreY;

//	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
	{
		centreY = 480 / 2+25;

		char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
		int b;
		if (AvPMenus.CurrentlySelectedElement >= 2)
		{
			b = ONE_FIXED;
		}
		else
		{
			b = ONE_FIXED/4;
		}
		
		RenderKeyConfigRectangle(b);

		if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS) {
			RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
		}
	}
#if 0
	else
	{
		centreY = ScreenDescriptorBlock.SDB_Height/2+25;

		int b;
		if (AvPMenus.CurrentlySelectedElement>=2)
		{
			b = ONE_FIXED;
		}
		else
		{
			b = ONE_FIXED/4;
		}
		
		RenderKeyConfigRectangle(b);
	}
#endif

	y = centreY-160;

	for (i = 0; i<2; i++, elementPtr++)
	{
		int targetBrightness;

		if (i==AvPMenus.CurrentlySelectedElement)
		{
			targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
		}
		else
		{
			targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
		}

		if (targetBrightness > elementPtr->Brightness)
		{
			elementPtr->Brightness+=BRIGHTNESS_CHANGE_SPEED;
			if (elementPtr->Brightness>targetBrightness)
			{
				elementPtr->Brightness = targetBrightness;
			}
		}
		else
		{
			elementPtr->Brightness-=BRIGHTNESS_CHANGE_SPEED;
			if (elementPtr->Brightness<targetBrightness)
			{
				elementPtr->Brightness = targetBrightness;
			}
		}
		
		RenderMenuElement(elementPtr,i,y);
		y += HeightOfMenuElement(elementPtr);
	}
	centrePosition = (AvPMenus.CurrentlySelectedElement)*ONE_FIXED;
	if (centrePosition<2*ONE_FIXED) centrePosition = 2*ONE_FIXED;

	for (i=2; i<AvPMenus.NumberOfElementsInMenu; i++,elementPtr++)
	{
		y = MUL_FIXED(i*65536-centrePosition,20);

		if (y>=-100 && y<=100)
		{
			int targetBrightness;

			if (i==AvPMenus.CurrentlySelectedElement)
			{
				targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
			}
			else
			{
				targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
			}

			elementPtr->Brightness = targetBrightness;
			RenderMenuElement(elementPtr, i, centreY+y);
		}
	}

	if (EpisodeSelectScrollOffset > 0)
	{
		EpisodeSelectScrollOffset -= MUL_FIXED(EpisodeSelectScrollOffset*2+8192, RealFrameTime<<1);
		if (EpisodeSelectScrollOffset < 0)
		{
			EpisodeSelectScrollOffset = 0;
		}
	}
	else if (EpisodeSelectScrollOffset < 0)
	{
		EpisodeSelectScrollOffset += MUL_FIXED(-EpisodeSelectScrollOffset*2 + 8192,RealFrameTime<<1);
		if (EpisodeSelectScrollOffset > 0)
		{
			EpisodeSelectScrollOffset = 0;
		}
	}
}

static void RenderScrollyMenu()
{
	AVPMENU_ELEMENT *elementPtr = AvPMenus.MenuElements;
	
	// draw the title
	char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
	RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

	{
		int first = AvPMenus.CurrentlySelectedElement;
		int last  = AvPMenus.CurrentlySelectedElement;
		int i;
		int y;
		BOOL done = FALSE;

		int available_above = (MENU_HEIGHT-HeightOfMenuElement(&elementPtr[AvPMenus.CurrentlySelectedElement]))/2;
		int available_below = available_above;

		//work out the first and last element to be drawn
		do
		{
			done = TRUE;
			if (first-1>=0)
			{
				int h=HeightOfMenuElement(&elementPtr[first-1]);
				if (h<=available_above)
				{
					available_above-=h;
					first--;
					done=FALSE;
				}
				else
				{
					available_below+=available_above;
					available_above=0;
				}
			}
			if (first==0)
			{
				//no more elements above selected element
				available_below+=available_above;
				available_above=0;
			}

			if (last+1<AvPMenus.NumberOfElementsInMenu)
			{
				int h=HeightOfMenuElement(&elementPtr[last+1]);
				if(h<=available_below)
				{
					available_below-=h;
					last++;
					done=FALSE;
				}
			}
			if(last==(AvPMenus.NumberOfElementsInMenu-1))
			{
				//no more elements below selected element
				available_above+=available_below;
				available_below=0;
			}
		}
		while(!done);

		//draw the appropriate elements
		elementPtr = &elementPtr[first];
		y = MENU_TOPY;

		for (i = first; i <= last; i++, elementPtr++)
		{
			int targetBrightness;

			if (i==AvPMenus.CurrentlySelectedElement)
			{
				targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
			}
			else
			{
				targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
			}

			elementPtr->Brightness = targetBrightness;

			RenderMenuElement(elementPtr, i, y);
			y += HeightOfMenuElement(elementPtr);
		}
	}
}

static void RenderUserProfileSelectMenu(void)
{
	AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];
	int currentEpisode = *(elementPtr->SliderValuePtr);
	int centrePosition = (currentEpisode)*65536+EpisodeSelectScrollOffset;
	int i;
	AVP_USER_PROFILE *profilePtr = GetFirstUserProfile();

	char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
	RenderMenuText(textPtr,MENU_CENTREX,70,ONE_FIXED,AVPMENUFORMAT_CENTREJUSTIFIED);

	GetLocalTime(&profilePtr->TimeLastUpdated);

	for (i=0; i<=elementPtr->MaxSliderValue; i++, profilePtr = GetNextUserProfile())
	{
		int y;

		y = MUL_FIXED(i*65536-centrePosition,80);

		if (y>=-150 && y<=150)
		{
			char *textPtr = profilePtr->Name;
			int b;
			int targetBrightness;

			if (i==currentEpisode)
			{
				targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
			}
			else
			{ 
				targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
			}

			if (targetBrightness > BrightnessOfRow(i))
			{
				BrightnessOfRow(i)+=BRIGHTNESS_CHANGE_SPEED;
				if(BrightnessOfRow(i)>targetBrightness)
				{
					BrightnessOfRow(i) = targetBrightness;
				}
			}
			else
			{
				BrightnessOfRow(i)-=BRIGHTNESS_CHANGE_SPEED;
				if(BrightnessOfRow(i)<targetBrightness)
				{
					BrightnessOfRow(i) = targetBrightness;
				}
			}
			b=BrightnessOfRow(i);
			RenderMenuText_Clipped(textPtr,MENU_CENTREX,MENU_CENTREY+y-60,b,AVPMENUFORMAT_CENTREJUSTIFIED,MENU_CENTREY-60-100,MENU_CENTREY-30+150);
			{
				char buffer[100];
				char buffer2[100];

#ifdef _WIN32
				MakeTimeText(profilePtr->TimeLastUpdated, DATE_LONGDATE, buffer2, sizeof(buffer2));
#endif
#ifdef _XBOX
				// manually format the string as we don't have either GetDateFormat or GetTimeFormat
				sprintf(buffer2, "%02d:%02d:%02d %02d/%02d/%02d", 
							profilePtr->TimeLastUpdated.wHour,
							profilePtr->TimeLastUpdated.wMinute,
							profilePtr->TimeLastUpdated.wSecond,
							profilePtr->TimeLastUpdated.wDay,
							profilePtr->TimeLastUpdated.wMonth,
							profilePtr->TimeLastUpdated.wYear
						);
#endif
				RenderSmallMenuText(buffer2,MENU_CENTREX,MENU_CENTREY+y-30,b,AVPMENUFORMAT_CENTREJUSTIFIED);

			}
		}
	}

	if (EpisodeSelectScrollOffset>0)
	{
		EpisodeSelectScrollOffset -= MUL_FIXED(EpisodeSelectScrollOffset*2+8192,RealFrameTime<<1);
		if (EpisodeSelectScrollOffset<0)
		{
			EpisodeSelectScrollOffset=0;
		}
	}
	else if (EpisodeSelectScrollOffset<0)
	{
		EpisodeSelectScrollOffset += MUL_FIXED(-EpisodeSelectScrollOffset*2+8192,RealFrameTime<<1);
		if (EpisodeSelectScrollOffset>0)
		{
			EpisodeSelectScrollOffset=0;
		}
	}
}

static void RenderLoadGameMenu(void)
{
	AVPMENU_ELEMENT *elementPtr = AvPMenus.MenuElements;
	int e;
	int y;
	int (*RenderText)(char *textPtr, int x, int y, int alpha, enum AVPMENUFORMAT_ID format);
	
//	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
	{
		y = MENU_CENTREY - (AvPMenus.MenuHeight)/2;
		RenderText = RenderSmallMenuText;
	}
#if 0
	else // in game menus
	{
		y = (ScreenDescriptorBlock.SDB_Height - AvPMenus.MenuHeight)/2;
		RenderText = Hardware_RenderSmallMenuText;
	}
#endif

	for (e = 0; e < AvPMenus.NumberOfElementsInMenu; e++, elementPtr++)
	{
		// buffer holds 100 bytes: up to 99 characters (letters, digits, spaces and so
		// on), then a zero byte that marks where the text stops (the "end marker").
		// The lines below that put words from the game's language text file on disk
		// into buffer use _snprintf, which stops at 99 characters, and then write the
		// end marker into the last byte themselves, because _snprintf leaves it out
		// when it has to cut the text short. So a line made very long in that file is
		// cut short on screen instead of writing past the end of buffer. Text that
		// fits is shown exactly as before. (The slot number line only writes a
		// number, which always fits.)
		char buffer[100];
		SAVE_SLOT_HEADER *slotPtr = &SaveGameSlot[e];
		int targetBrightness;

		if (e==AvPMenus.CurrentlySelectedElement)
		{
			RenderHighlightRectangle(MENU_LEFTXEDGE,y-2,MENU_RIGHTXEDGE,y+4+HUD_FONT_HEIGHT*2,0,128,0);
			targetBrightness = BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT;
		}
		else
		{ 
			targetBrightness = BRIGHTNESS_OF_DARKENED_ELEMENT;
		}

		if (targetBrightness > elementPtr->Brightness)
		{
			elementPtr->Brightness += BRIGHTNESS_CHANGE_SPEED;
			if (elementPtr->Brightness > targetBrightness)
			{
				elementPtr->Brightness = targetBrightness;
			}
		}
		else
		{
			elementPtr->Brightness -= BRIGHTNESS_CHANGE_SPEED;
			if (elementPtr->Brightness < targetBrightness)
			{
				elementPtr->Brightness = targetBrightness;
			}
		}
		
		sprintf(buffer,"%d.",e+1);
		RenderText(buffer, MENU_LEFTXEDGE+20, y, elementPtr->Brightness, AVPMENUFORMAT_RIGHTJUSTIFIED);
		
		if (slotPtr->SlotUsed)
		{
			int textID;
			int numberOfBasicEpisodes;
			switch(slotPtr->Species)
			{
				case I_Marine:
				{
					textID = TEXTSTRING_MARINELEVELS_1;
					numberOfBasicEpisodes = MAX_NO_OF_BASIC_MARINE_EPISODES;
					break;
				}
				case I_Alien:
				{
					textID = TEXTSTRING_ALIENLEVELS_1;
					numberOfBasicEpisodes = MAX_NO_OF_BASIC_ALIEN_EPISODES;
					break;
				}
				case I_Predator:
				{
					textID = TEXTSTRING_PREDATORLEVELS_1;
					numberOfBasicEpisodes = MAX_NO_OF_BASIC_PREDATOR_EPISODES;
					break;
				}
			}

			// size-limited: see the note where buffer is declared
			_snprintf(buffer, sizeof(buffer) - 1, "%s", GetTextString(static_cast<enum TEXTSTRING_ID>(TEXTSTRING_MULTIPLAYER_MARINE + slotPtr->Species)));
			buffer[sizeof(buffer) - 1] = '\0';
			RenderText(buffer, MENU_LEFTXEDGE+30, y, elementPtr->Brightness, AVPMENUFORMAT_LEFTJUSTIFIED);
			
			// size-limited: see the note where buffer is declared
			_snprintf(buffer, sizeof(buffer) - 1, "%s", GetTextString(static_cast<enum TEXTSTRING_ID>(textID+slotPtr->Episode)));
			buffer[sizeof(buffer) - 1] = '\0';
			RenderText(buffer, MENU_CENTREX, y, elementPtr->Brightness, AVPMENUFORMAT_CENTREJUSTIFIED);

			if (numberOfBasicEpisodes > slotPtr->Episode)
			{
				// size-limited: see the note where buffer is declared
				_snprintf(buffer, sizeof(buffer) - 1, "%s", GetTextString(static_cast<enum TEXTSTRING_ID>(TEXTSTRING_DIFFICULTY_EASY + slotPtr->Difficulty)));
				buffer[sizeof(buffer) - 1] = '\0';
				RenderText(buffer, MENU_RIGHTXEDGE-30, y, elementPtr->Brightness, AVPMENUFORMAT_RIGHTJUSTIFIED);
			}

			// size-limited: see the note where buffer is declared
			_snprintf(buffer, sizeof(buffer) - 1, "%s %02d:%02d:%02d", GetTextString(TEXTSTRING_GAMESTATS_TIMEELAPSED), slotPtr->ElapsedTime_Hours, slotPtr->ElapsedTime_Minutes, slotPtr->ElapsedTime_Seconds);
			buffer[sizeof(buffer) - 1] = '\0';
			RenderText(buffer, MENU_LEFTXEDGE+30, y+HUD_FONT_HEIGHT+1, elementPtr->Brightness, AVPMENUFORMAT_LEFTJUSTIFIED);

			// size-limited: see the note where buffer is declared
			_snprintf(buffer, sizeof(buffer) - 1, "%s: %d", GetTextString(TEXTSTRING_SAVEGAME_SAVESLEFT), slotPtr->SavesLeft);
			buffer[sizeof(buffer) - 1] = '\0';
			RenderText(buffer, MENU_CENTREX, y+HUD_FONT_HEIGHT+1, elementPtr->Brightness, AVPMENUFORMAT_CENTREJUSTIFIED);

			{
				char buffer2[100];

#ifdef _WIN32
				MakeTimeText(slotPtr->TimeStamp, DATE_SHORTDATE, buffer2, sizeof(buffer2));
#endif
#ifdef _XBOX
				// manually format the string as we don't have either GetDateFormat or GetTimeFormat
				sprintf(buffer2, "%02d:%02d:%02d %02d/%02d/%02d", 
							slotPtr->TimeStamp.wHour,
							slotPtr->TimeStamp.wMinute,
							slotPtr->TimeStamp.wSecond,
							slotPtr->TimeStamp.wDay,
							slotPtr->TimeStamp.wMonth,
							slotPtr->TimeStamp.wYear
						);
#endif
				RenderText(buffer2, MENU_RIGHTXEDGE-30, y+HUD_FONT_HEIGHT+1, elementPtr->Brightness, AVPMENUFORMAT_RIGHTJUSTIFIED);
			}
		}
		else
		{
			RenderText(GetTextString(TEXTSTRING_SAVEGAME_EMPTYSLOT), MENU_CENTREX, y, elementPtr->Brightness, AVPMENUFORMAT_CENTREJUSTIFIED);
		}

		y += HeightOfMenuElement(elementPtr);
	}


	/* Render Menu Title */
	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
	{
		char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
		RenderMenuText(textPtr, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
	else
	{
#if 0
		char *textPtr = GetTextString(AvPMenusData[AvPMenus.CurrentMenu].MenuTitle);
		AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];
		y = (ScreenDescriptorBlock.SDB_Height - AvPMenus.MenuHeight)/2 - 30;
		RenderText(textPtr, MENU_CENTREX, y, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);

		y = (ScreenDescriptorBlock.SDB_Height + AvPMenus.MenuHeight)/2 + 20;
		RenderText(GetTextString(elementPtr->HelpString), MENU_CENTREX, y, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
#endif
	}
}

// What a text field shows: its own text, except the join page's address box, which shows a dot for each character unless Tab is
// held (address_box.h). The address itself is left as it is.
static char *TextFieldShows(AVPMENU_ELEMENT *elementPtr)
{
	static char shown[64];   // the box takes 15 characters (MAX_IPADDRESS_LENGTH, menu_data.cpp), and a longer one is cut short

	if (elementPtr->TextPtr != IPAddressString)
	{
		return elementPtr->TextPtr;
	}

	AddressBox_TextToDraw(IPAddressString, KeyboardInput[KEY_TAB] != 0, shown, sizeof(shown));
	return shown;
}

static void RenderHelpString()
{
	AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];

	if (elementPtr->HelpString!=TEXTSTRING_BLANK/* && AvPMenus.MenusState != MENUSSTATE_INGAMEMENUS*/)
	{
		RECT area;
		//draw the attached string at the bottom of the screen

		area.left   = MENU_LEFTXEDGE;
		area.right  = MENU_RIGHTXEDGE;
		area.top    = 420;
		area.bottom = 480;//ScreenDescriptorBlock.SDB_Height; // bjd MENU

		// The address box's help also says how to see the address (address_box.h)
		if (elementPtr->TextPtr == IPAddressString)
		{
			static char help[512];
			_snprintf(help, sizeof(help) - 1, "%s %s", GetTextString(elementPtr->HelpString), kAddressBoxHelp);
			help[sizeof(help) - 1] = '\0';
			RenderSmallFontString_Wrapped(help, &area, BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT, 0, 0);
		}
		else
		{
			RenderSmallFontString_Wrapped(GetTextString(elementPtr->HelpString), &area,BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT, 0, 0);
		}
	}
}

static void RenderConfigurationDescriptionString()
{
	const char* text = GetMultiplayerConfigDescription(AvPMenus.CurrentlySelectedElement);
	if (text)
	{
		RECT area;
		//draw the text at the bottom of the screen
		//now at the top.

		area.left   = MENU_LEFTXEDGE;
		area.right  = MENU_RIGHTXEDGE;
		area.top    = 0;
		area.bottom = 60;

		RenderSmallFontString_Wrapped((char*)text, &area, BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT, 0, 0);
	}
}

static void ActUponUsersInput(void)
{
	static int BackspaceTimer=0;
	//Set up a keyboard repeat rate thingy for deleting long strings

	// dont process input for the menus if i'm in the console
	if (IOFOCUS_Get() & (IOFOCUS_NEWCONSOLE))
		return;

	if (KeyboardInput[KEY_BACKSPACE])
	{
		BackspaceTimer+=RealFrameTime;
	}
	else
	{
		BackspaceTimer=0;
	}
	
	if ((AvPMenus.UserEnteringText) || (AvPMenus.UserEnteringNumber))
	{
		Osk_Activate();
	}
	else 
	{
		Osk_Deactivate();
	}

	if (AvPMenus.UserEnteringText)
	{
		AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];

		if (DebouncedKeyboardInput[KEY_ESCAPE] || DebouncedKeyboardInput[KEY_CR] /*|| DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_2]*/)
		{
			elementPtr->TextPtr[AvPMenus.PositionInTextField] = 0;
			AvPMenus.UserEnteringText = 0;

			// KJL 10:09:35 09/02/00 - when the user has entered their name,
			// move down to the next option. If the user enters a null
			// string, replace it with a placeholder name		
			if ((AvPMenus.CurrentMenu == AVPMENU_USERPROFILEENTERNAME)
 			 ||(AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER_SKIRMISH)
 			 ||(AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER))
			{
				if (AvPMenus.PositionInTextField == 0)
				{
					strcpy(elementPtr->TextPtr,"DeadMeat");
				}
				AvPMenus.CurrentlySelectedElement++;
			}
		}
		else if (DebouncedKeyboardInput[KEY_BACKSPACE] || DebouncedKeyboardInput[KEY_LEFT])
		{
			if (AvPMenus.PositionInTextField>0)
			{
				elementPtr->TextPtr[--AvPMenus.PositionInTextField] = 0;
			}
		}
		else if (BackspaceTimer>ONE_FIXED/2)
		{
			//check for backspace being held down for a long time
			while (BackspaceTimer > ONE_FIXED/2)
			{
				BackspaceTimer -= ONE_FIXED/20;
				if (AvPMenus.PositionInTextField > 0)
				{
					elementPtr->TextPtr[--AvPMenus.PositionInTextField] = 0;
				}
			}
		}
		else
		{
			//allow Ctrl+V to paste from the clipboard (really just for pasting in ip addresses)
			if ((KeyboardInput[KEY_LEFTCTRL] || KeyboardInput[KEY_RIGHTCTRL]) && KeyboardInput[KEY_V])
			{
				PasteFromClipboard(elementPtr->TextPtr, elementPtr->MaxTextLength);
				AvPMenus.PositionInTextField = strlen(elementPtr->TextPtr);
			}
			else if (AvPMenus.PositionInTextField < elementPtr->MaxTextLength)
			{
				char c = 0;
				KeyboardEntryQueue_StartProcessing();
				
				while (c = KeyboardEntryQueue_ProcessCharacter())
				{
					if (AvPMenus.PositionInTextField < elementPtr->MaxTextLength)
					{
						//see if there is room for this character
						if (AvPMenus.FontToUse == AVPMENU_FONT_BIG && elementPtr->ElementID != AVPMENU_ELEMENT_TEXTFIELD_SMALLWRAPPED)
						{
							//using large font
							//allocate 32 pixels for each new character for the moment
							//the true amount used will be worked out when the font is drawn
							//Might cause a slight glitch for fast typists , but I forgot about 
							//the damned kerned fonts until after I'd written most of this
							if (AvPMenus.WidthLeftForText < 32)
								break;
							AvPMenus.WidthLeftForText -= 32;
						}
						else
						{
							//using small font
							if (AvPMenus.WidthLeftForText < AAFontWidths[(unsigned char)c]) 
								break;
							AvPMenus.WidthLeftForText -= AAFontWidths[(unsigned char)c];
						}

						elementPtr->TextPtr[AvPMenus.PositionInTextField++] = c;
						elementPtr->TextPtr[AvPMenus.PositionInTextField] = 0;
					}
				}
			}

			KeyboardEntryQueue_Clear();
		}
	}
	else if (AvPMenus.UserEnteringNumber)
	{
		AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];

		if (DebouncedKeyboardInput[KEY_ESCAPE] || DebouncedKeyboardInput[KEY_CR])
		{
			AvPMenus.UserEnteringNumber = 0;
		}
		else if (DebouncedKeyboardInput[KEY_BACKSPACE] || DebouncedKeyboardInput[KEY_LEFT])
		{
			(*elementPtr->NumberPtr)/=10;
		}
		else
		{
			char c = 0;

			KeyboardEntryQueue_StartProcessing();
			while (c = KeyboardEntryQueue_ProcessCharacter())
			{
				if (AvPMenus.PositionInTextField<elementPtr->MaxTextLength)
				{
					if (c >= '0' && c <= '9')
					{
						(*elementPtr->NumberPtr)*=10;
						(*elementPtr->NumberPtr)+=c-'0';

						if ((*elementPtr->NumberPtr) > elementPtr->MaxValue) {
							(*elementPtr->NumberPtr) = elementPtr->MaxValue;
						}
					}
				}
			}
		}
		KeyboardEntryQueue_Clear();
	}
	// A key on the rendering page. This asks view_settings.cpp rather than writing the key itself, because it can be refused, and the line's help then says why. Even a digit is passed on, for that reason.
	else if (AvPMenus.UserChangingViewKey)
	{
		if (DebouncedKeyboardInput[KEY_ESCAPE])
		{
			AvPMenus.UserChangingViewKey = 0;
		}
		else
		{
			for (int key = 0; key < KEY_VOID; key++)
			{
				if (DebouncedKeyboardInput[key])
				{
					ViewSettings_PressedKeyForLine(AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement].MaxValue, key);
					AvPMenus.UserChangingViewKey = 0;
					break;
				}
			}
		}
	}
	// bjd - key config
	else if (AvPMenus.UserChangingKeyConfig)
	{
		if (DebouncedKeyboardInput[KEY_ESCAPE])
		{
			AvPMenus.UserChangingKeyConfig = 0;
		}
		else
		{
			signed int key,selectedKey=-1;

			// see if a valid key has been pressed
			/* 
			 * bjd - changed from key <= MAX_NUMBER_OF_INPUT_KEYS
			 * to
			 * key < MAX_NUMBER_OF_INPUT_KEYS
			 */

			for (key = 0 ; key < MAX_NUMBER_OF_INPUT_KEYS ; key++)
			{
				if (!(key == KEY_ESCAPE) &&
//					!(key >= KEY_F1 && key <= KEY_F12) &&
					!(key >= KEY_0 && key <= KEY_9) )
				{
					if (DebouncedKeyboardInput[key])
					{
						selectedKey = key;
						break;
					}
				}
			}

			if (AvPMenus.ChangingPrimaryConfig)
			{
				if (selectedKey!=-1)
				{
					*(((unsigned char*)&PlayerInputPrimaryConfig)+AvPMenus.CurrentlySelectedElement-2) = selectedKey;
					AvPMenus.UserChangingKeyConfig=0;
				}
			}
			else // changing Secondary
			{
				if (selectedKey!=-1)
				{
					*(((unsigned char*)&PlayerInputSecondaryConfig)+AvPMenus.CurrentlySelectedElement-2) = selectedKey;
					AvPMenus.UserChangingKeyConfig=0;
				}
			}
		}
	}
	else
	{	
		if ((DebouncedKeyboardInput[KEY_ESCAPE] || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_4]) && (AvPMenus.CurrentMenu != AVPMENU_MAIN && AvPMenus.CurrentMenu != AVPMENU_INGAME))
		{
			switch (AvPMenus.CurrentMenu)
			{
				case AVPMENU_MULTIPLAYER_CONFIG_JOIN:
				{
					AddNetMsg_PlayerLeaving();
					NetSendMessages();
					if (!LobbiedGame)
					{
						Net_Disconnect();
					}
					break;
				}
				case AVPMENU_COOP_HOST:
				case AVPMENU_COOP_JOIN:
				{
					CoopMenu_PageLeft();
					break;
				}
				case AVPMENU_DETAILLEVELS:
				{
					SetDetailLevelsFromMenu();
					SaveUserProfile(UserProfilePtr);
					if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
					{
						SetupNewMenu(AVPMENU_INGAMEAVOPTIONS);
					}
					break;
				}
				case AVPMENU_INGAMEAVOPTIONS:
				case AVPMENU_MAINMENUAVOPTIONS:
				{
					SaveUserProfile(UserProfilePtr);
					break;
				}
				case AVPMENU_CHEATOPTIONS:
				{
					CheatMode_Active = CHEATMODE_NONACTIVE;
					break;	
				}

				case AVPMENU_MULTIPLAYER_CONFIG :
				case AVPMENU_SKIRMISH_CONFIG :
				{
					//reload the previous multiplayer configuration
					LoadMultiplayerConfiguration(GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME));
					MP_Config_Description[0]=0;
					break;
				}
			}

			if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
			{
				SetupNewMenu(AVPMENU_INGAME);
			}
			else
			{
				SetupNewMenu(AvPMenusData[AvPMenus.CurrentMenu].ParentMenu);
			}
		}
		else if (IDemandSelect()) // select element
		{
			if (InputIsDebounced)
			{
				InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_SELECT);
				InputIsDebounced = 0;
			}
		}
		else if (IDemandGoForward()) // previous element
		{
			if (InputIsDebounced)
			{
				switch (AvPMenus.CurrentMenu)
				{
					case AVPMENU_USERPROFILESELECT:
					case AVPMENU_MARINELEVELS:
					case AVPMENU_PREDATORLEVELS:
					case AVPMENU_ALIENLEVELS:
					{
						InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_DECREASE);
						break;
					}
					case AVPMENU_MARINEKEYCONFIG:
					case AVPMENU_PREDATORKEYCONFIG:
					case AVPMENU_ALIENKEYCONFIG:
					{
						if (AvPMenus.CurrentlySelectedElement>0)
						{
							AvPMenus.CurrentlySelectedElement--;
							Sound_Play(SID_MENUS_CHANGE_ITEM,"r");
						}
						break;
					}
					default:
					{
						AvPMenus.CurrentlySelectedElement--;
						if (AvPMenus.CurrentlySelectedElement<0)
						{
							AvPMenus.CurrentlySelectedElement = AvPMenus.NumberOfElementsInMenu-1;
						}
						Sound_Play(SID_MENUS_CHANGE_ITEM,"r");
						break;
					}
				}
				InputIsDebounced = 0;
			}
			else
			{
				KeyDepressedCounter += RealFrameTime;
			}
		}
		else if (IDemandGoBackward()) // next element
		{
			if (InputIsDebounced)
			{
				switch (AvPMenus.CurrentMenu)
				{
					case AVPMENU_USERPROFILESELECT:
					case AVPMENU_MARINELEVELS:
					case AVPMENU_PREDATORLEVELS:
					case AVPMENU_ALIENLEVELS:
					{
						InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_INCREASE);
						break;
					}
					case AVPMENU_MARINEKEYCONFIG:
				    case AVPMENU_PREDATORKEYCONFIG:
					case AVPMENU_ALIENKEYCONFIG:
					{
						if (AvPMenus.CurrentlySelectedElement<AvPMenus.NumberOfElementsInMenu-1)
						{
							AvPMenus.CurrentlySelectedElement++;
							Sound_Play(SID_MENUS_CHANGE_ITEM,"r");
						}
						break;
					}
					default:
					{
						AvPMenus.CurrentlySelectedElement++;
						if (AvPMenus.CurrentlySelectedElement>=AvPMenus.NumberOfElementsInMenu)
						{
							AvPMenus.CurrentlySelectedElement= 0;
						}
						Sound_Play(SID_MENUS_CHANGE_ITEM,"r");
						break;
					}
				}

				InputIsDebounced = 0;
			}
			else
			{
				KeyDepressedCounter += RealFrameTime;
			}
		}
		else if (IDemandTurnLeft()) 
		{
			if (InputIsDebounced)
			{
				InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_DECREASE);
				InputIsDebounced = 0;
			}
			else
			{
				KeyDepressedCounter += RealFrameTime;
			}
		}
		else if (IDemandTurnRight()) 
		{
			if (InputIsDebounced)
			{
				InteractWithMenuElement(AVPMENU_ELEMENT_INTERACTION_INCREASE);
				InputIsDebounced = 0;
			}
			else
			{
				KeyDepressedCounter += RealFrameTime;
			}
		}
		else if (DebouncedKeyboardInput[KEY_BACKSPACE] || DebouncedKeyboardInput[KEY_JOYSTICK_BUTTON_8])  // bjd
		{
			switch (AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement].ElementID)
			{
				case AVPMENU_ELEMENT_KEYCONFIG:
				{
					if (!KeyConfigSelectionColumn)
					{
						*(((unsigned char*)&PlayerInputPrimaryConfig)+AvPMenus.CurrentlySelectedElement-2) = KEY_VOID;
					}
					else // changing Secondary
					{
						*(((unsigned char*)&PlayerInputSecondaryConfig)+AvPMenus.CurrentlySelectedElement-2) = KEY_VOID;
					}
					break;
				}
				case AVPMENU_ELEMENT_VIEWKEY:
				{
					ViewSettings_PressedKeyForLine(AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement].MaxValue, KEY_VOID);
					break;
				}
				case AVPMENU_ELEMENT_USERPROFILE:
				{
					if (UserProfileNumber)
					{
						SetupNewMenu(AVPMENU_USERPROFILEDELETE);
					}
					break;
				}
				case AVPMENU_ELEMENT_LOADMPCONFIG :
				{
					//take note of the current configuration
					MultiplayerConfigurationIndex=AvPMenus.CurrentlySelectedElement;
					MultiplayerConfigurationName=AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement].TextPtr;
					//setup the delete configuration menu
					SetupNewMenu(AVPMENU_MULTIPLAYER_DELETECONFIG);
					break;
				}
				default:
					break;
			}
		}
		else
		{
			InputIsDebounced=1;
			KeyDepressedCounter = 0;
		}

		if (KeyDepressedCounter>ONE_FIXED)
			InputIsDebounced = 1;
	}
}

// Whether a game hosted now would be hosted online: when servers.cfg names a server, as every game is (networking.cpp, Net_HostGame).
// Added 2026-11-10 by Melanikus for the AvP port, for the ways online (ServerYesPage_WayIn); modified the same day (the owner's ruling for beta.2: "Everything over nyc3"):
// there's no Host row and no game at home, so with no server named nothing is hosted, and the host says why.
static bool HostsOnline(void)
{
	return Relay_ServerName()[0] != 0;
}

static void InteractWithMenuElement(enum AVPMENU_ELEMENT_INTERACTION_ID interactionID)
{
	AVPMENU_ELEMENT *elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];

	if (interactionID==AVPMENU_ELEMENT_INTERACTION_SELECT)
	{
		Sound_Play(SID_MENUS_SELECT_ITEM,"r");
	}
	else
	{
		Sound_Play(SID_MENUS_CHANGE_ITEM,"r");
	}
	switch(elementPtr->ElementID)
	{
		default:
		GLOBALASSERT("UNKNOWN MENU ELEMENT"==0);
		case AVPMENU_ELEMENT_FINDAGAIN:
		{
			// The page of games found's Refresh row, and Enter on its failure screen: the open sessions are fetched again
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				OnlinePage_FindAgain();
			}
			break;
		}
		case AVPMENU_ELEMENT_GOTOMENU:
		case AVPMENU_ELEMENT_GOTOMENU_GFX:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				// Check to see if we are on one of the menus for entering multiplayer name
				if (AvPMenus.CurrentMenu == AVPMENU_MULTIPLAYER ||
				   AvPMenus.CurrentMenu  == AVPMENU_MULTIPLAYER_LOBBIEDSERVER)
				{
					// save profile, so that multiplayer name is remembered
					SaveUserProfile(UserProfilePtr);
				}

				// Modified 2026-11-10 by Melanikus for the AvP port (#175's last part, PO's rulings): a way online asks for the player's yes to
				// the server in servers.cfg first, as its row is pressed (server_yes_page.h, ServerYesPage_WayIn). Every other row goes where it
				// always went.
				enum AVPMENU_ID const page = ServerYesPage_WayIn(AvPMenus.CurrentMenu, static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo), HostsOnline());

				// The co-op page hosts as it opens, so online it first waits here for the server's answer (added 2026-11-10, PO's ruling Q3)
				if (page == AVPMENU_COOP_HOST && AHostWaits(kHostWaitingToOpenCoop))
				{
					break;
				}

				SetupNewMenu(page);
			}
			break;
		}
		case AVPMENU_ELEMENT_SAVEMPCONFIG:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				if (MP_Config_Name[0])
				{
					SaveMultiplayerConfiguration(MP_Config_Name);
				}
				SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
			}
			break;
		}
		case AVPMENU_ELEMENT_TEXTFIELD:
		{
			AvPMenus.UserEnteringText = 1;
			KeyboardEntryQueue_Clear();
			AvPMenus.PositionInTextField = strlen(elementPtr->TextPtr);
			elementPtr->TextPtr[AvPMenus.PositionInTextField] = 0;
			AvPMenus.WidthLeftForText = 0; //will be calculated properly when menus are drawn
			break;
		}
		case AVPMENU_ELEMENT_TEXTFIELD_SMALLWRAPPED:
		{
			AvPMenus.UserEnteringText = 1;
			KeyboardEntryQueue_Clear();
			AvPMenus.PositionInTextField = strlen(elementPtr->TextPtr);
			AvPMenus.WidthLeftForText = 0; //will be calculated properly when menus are drawn
			break;
		}

		case AVPMENU_ELEMENT_NUMBERFIELD:
		{
			if(interactionID == AVPMENU_ELEMENT_INTERACTION_DECREASE)
			{
				(*elementPtr->NumberPtr)--;
				if(*elementPtr->NumberPtr<0)
				{
					*elementPtr->NumberPtr=0;
				}
			}
			else if(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE)
			{
				(*elementPtr->NumberPtr)++;
				if(*elementPtr->NumberPtr>elementPtr->MaxValue)
				{
					*elementPtr->NumberPtr=elementPtr->MaxValue;
				}
			}
			else
			{
				*elementPtr->NumberPtr=0;
				AvPMenus.UserEnteringNumber = 1;
				KeyboardEntryQueue_Clear();
			}
			break;
		}
		case AVPMENU_ELEMENT_DUMMYTEXTFIELD:
		case AVPMENU_ELEMENT_DUMMYTEXTSLIDER:
		case AVPMENU_ELEMENT_DUMMYTEXTSLIDER_POINTER:
		case AVPMENU_ELEMENT_DUMMYNUMBERFIELD:
		case AVPMENU_ELEMENT_TOGGLE:
		{
			break;
		}
		case AVPMENU_ELEMENT_SLIDER:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				if (*elementPtr->SliderValuePtr<elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr+=1;
				}
			}
			else
			{
				if (*elementPtr->SliderValuePtr>0)
				{
					*elementPtr->SliderValuePtr-=1;
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_TEXTSLIDER:
		case AVPMENU_ELEMENT_TEXTSLIDER_POINTER:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				*elementPtr->SliderValuePtr+=1;
				if (*elementPtr->SliderValuePtr>elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr=0;
				}
			}
			else
			{
				*elementPtr->SliderValuePtr-=1;
				if (*elementPtr->SliderValuePtr<0)
				{
					*elementPtr->SliderValuePtr=elementPtr->MaxSliderValue;
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_SPECIES_TEXTSLIDER:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				*elementPtr->SliderValuePtr+=1;
				if (*elementPtr->SliderValuePtr>elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr=0;
				}
				GetNextAllowedSpecies(elementPtr->SliderValuePtr,TRUE);
			}
			else
			{
				*elementPtr->SliderValuePtr-=1;
				if (*elementPtr->SliderValuePtr<0)
				{
					*elementPtr->SliderValuePtr=elementPtr->MaxSliderValue;
				}
				GetNextAllowedSpecies(elementPtr->SliderValuePtr,FALSE);
			}
			break;
		}
		case AVPMENU_ELEMENT_CHEATMODE_SPECIES_TEXTSLIDER:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				*elementPtr->SliderValuePtr+=1;
				if (*elementPtr->SliderValuePtr>elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr=0;
				}
				CheatMode_GetNextAllowedSpecies(elementPtr->SliderValuePtr,TRUE);
			}
			else
			{
				*elementPtr->SliderValuePtr-=1;
				if (*elementPtr->SliderValuePtr<0)
				{
					*elementPtr->SliderValuePtr=elementPtr->MaxSliderValue;
				}
				CheatMode_GetNextAllowedSpecies(elementPtr->SliderValuePtr,FALSE);
			}
			break;
		}
		case AVPMENU_ELEMENT_CHEATMODE_TEXTSLIDER:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				*elementPtr->SliderValuePtr+=1;
				if (*elementPtr->SliderValuePtr>elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr=0;
				}
				CheatMode_GetNextAllowedMode(elementPtr->SliderValuePtr,TRUE);
			}
			else
			{
				*elementPtr->SliderValuePtr-=1;
				if (*elementPtr->SliderValuePtr<0)
				{
					*elementPtr->SliderValuePtr=elementPtr->MaxSliderValue;
				}
				CheatMode_GetNextAllowedMode(elementPtr->SliderValuePtr,FALSE);
			}
			break;
		}
		case AVPMENU_ELEMENT_CHEATMODE_ENVIRONMENT_TEXTSLIDER:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				*elementPtr->SliderValuePtr+=1;
				if (*elementPtr->SliderValuePtr>elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr=0;
				}
				CheatMode_GetNextAllowedEnvironment(elementPtr->SliderValuePtr,TRUE);
			}
			else
			{
				*elementPtr->SliderValuePtr-=1;
				if (*elementPtr->SliderValuePtr<0)
				{
					*elementPtr->SliderValuePtr=elementPtr->MaxSliderValue;
				}
				CheatMode_GetNextAllowedEnvironment(elementPtr->SliderValuePtr,FALSE);
			}
			break;
		}

		case AVPMENU_ELEMENT_QUITGAME:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
				{
					AvP.MainLoopRunning = 0;
				}
				AvPMenus.MenusState = MENUSSTATE_OUTSIDEMENUS;
			}
			break;
		}
		
		case AVPMENU_ELEMENT_USERPROFILE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				/* a profile has been selected */
				int i;
				UserProfilePtr = GetFirstUserProfile();
				for (i=0; i<UserProfileNumber; i++)
					UserProfilePtr = GetNextUserProfile();

				if (UserProfileNumber)
				{
					// Edmond too
					GetSettingsFromUserProfile();
					// Edmond
					if (LobbiedGame == LobbiedGame_Server)
					{
						SetupNewMenu(AVPMENU_MULTIPLAYER_CONFIG);			// Edmond
					}
					else if (LobbiedGame == LobbiedGame_Client)
					{
						SetupNewMenu(AVPMENU_MULTIPLAYER_LOBBIEDCLIENT);
					}
					else
						SetupNewMenu(AVPMENU_MAIN);
				}
				else
				{
					SetupNewMenu(AVPMENU_USERPROFILEENTERNAME);
				}
			}
			else if (interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE)
			{
				if (*elementPtr->SliderValuePtr<elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr+=1;
					EpisodeSelectScrollOffset-=ONE_FIXED;
				}
			}
			else
			{
				if (*elementPtr->SliderValuePtr>0)
				{
					*elementPtr->SliderValuePtr-=1;
					EpisodeSelectScrollOffset+=ONE_FIXED;
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_USERPROFILE_DELETE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				DeleteUserProfile(UserProfileNumber);
				SetupNewMenu(AVPMENU_USERPROFILESELECT);
			}
			break;
		}
		case AVPMENU_ELEMENT_DELETEMPCONFIG:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				DeleteMultiplayerConfigurationByIndex(MultiplayerConfigurationIndex);
				//go back to the load config menu
				SetupNewMenu(AVPMENU_MULTIPLAYER_LOADCONFIG);
			}
			break;
		}
		case AVPMENU_ELEMENT_CUSTOMMAP_START:
		{
			// With no map found, there's nothing to start
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT && CustomMaps_SetLevelToLoad(CustomMapToPlay))
			{
				AvP.PlayerType = CustomMapSpecies == 1 ? I_Predator : CustomMapSpecies == 2 ? I_Alien : I_Marine;
				AvP.Difficulty = CustomMapDifficulty == 0 ? I_Easy : CustomMapDifficulty == 2 ? I_Hard : I_Medium;
				CustomMaps_SetPlaying(true);
				CustomMaps_ForgetPictures();
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
			}
			break;
		}
		case AVPMENU_ELEMENT_COOP_PLAYER:
		{
			break;
		}
		case AVPMENU_ELEMENT_COOP_REMOVE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				CoopMenu_HostRemoves(elementPtr->MaxValue);
			}
			break;
		}
		case AVPMENU_ELEMENT_ONLINE_ACCESS:
		{
			enum AVPMENU_ID page = AVPMENU_ONLINE_ACCESS;

			// The Servers page's rows are this kind too, one for each server (#175 2b, added 2026-11-10): pressing one picks it, for the next
			// start. Nothing goes to any server.
			if (AvPMenus.CurrentMenu == AVPMENU_SERVERS)
			{
				if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
				{
					ServersPage_Pressed(elementPtr->MaxValue);
				}
				break;
			}

			// The yes page's two rows are this kind too (#175's last part): they're its own, never the access page's
			if (AvPMenus.CurrentMenu == AVPMENU_SERVER_YES)
			{
				if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT && ServerYesPage_Pressed(elementPtr->MaxValue, &page))
				{
					SetupNewMenu(page);
				}
				break;
			}

			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT && AccessPage_Pressed(elementPtr->MaxValue, &page))
			{
				SetupNewMenu(page);
			}
			break;
		}
		case AVPMENU_ELEMENT_COOP_START:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				StartTheCoopCampaign();
			}
			break;
		}
		case AVPMENU_ELEMENT_DIFFICULTYLEVEL:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				if (AvPMenus.CurrentMenu == AVPMENU_LEVELBRIEFING_BASIC)
				{
					AvP.Difficulty = static_cast<I_HARDANUFF>(AvPMenus.CurrentlySelectedElement);
				}
				else
				{
					AvP.Difficulty = I_Medium; //bjd - was 1;
				}
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
			}
			break;
		}

		case AVPMENU_ELEMENT_ALIENEPISODE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT
			  &&MaximumSelectableLevel>=*elementPtr->SliderValuePtr)
			{
				AvP.PlayerType = I_Alien;
				SetLevelToLoadForAlien(AlienEpisodeToPlay);

				if (AlienEpisodeToPlay<MAX_NO_OF_BASIC_ALIEN_EPISODES)
				{
					SetupNewMenu(AVPMENU_LEVELBRIEFING_BASIC);
				}
				else
				{
					SetupNewMenu(AVPMENU_LEVELBRIEFING_BONUS);
				}
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu = AVPMENU_ALIENLEVELS;
				break;
			}
			/* else let it fall through */
		}
		case AVPMENU_ELEMENT_MARINEEPISODE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT
				&&MaximumSelectableLevel>=*elementPtr->SliderValuePtr)
			{
				AvP.PlayerType = I_Marine;
				SetLevelToLoadForMarine(MarineEpisodeToPlay);

				if (MarineEpisodeToPlay < MAX_NO_OF_BASIC_MARINE_EPISODES)
				{
					SetupNewMenu(AVPMENU_LEVELBRIEFING_BASIC);
				}
				else
				{
					SetupNewMenu(AVPMENU_LEVELBRIEFING_BONUS);
				}
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu = AVPMENU_MARINELEVELS;
				break;
			}
			/* else let it fall through */
		}
		case AVPMENU_ELEMENT_PREDATOREPISODE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT
				&&MaximumSelectableLevel>=*elementPtr->SliderValuePtr)
			{
				AvP.PlayerType = I_Predator;
				SetLevelToLoadForPredator(PredatorEpisodeToPlay);

				if (PredatorEpisodeToPlay<MAX_NO_OF_BASIC_PREDATOR_EPISODES)
				{
					SetupNewMenu(AVPMENU_LEVELBRIEFING_BASIC);
				}
				else
				{
					SetupNewMenu(AVPMENU_LEVELBRIEFING_BONUS);
				}
				AvPMenusData[AvPMenus.CurrentMenu].ParentMenu = AVPMENU_PREDATORLEVELS;
				break;
			}
			/* else let it fall through */
		}
		{
			/* This code is reached when an EPISODE element has been
			activated but not "selected" i.e. the user wants to change
			the current episode, not start playing the current episode */
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE)
			{
				if (*elementPtr->SliderValuePtr < elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr += 1;
					EpisodeSelectScrollOffset -= ONE_FIXED;
				}
			}
			else
			{
				if (*elementPtr->SliderValuePtr > 0)
				{
					*elementPtr->SliderValuePtr -= 1;
					EpisodeSelectScrollOffset += ONE_FIXED;
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_BUTTONSETTING:
		{
			
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT) {
				break;
			}
			/* else let it fall through */
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE)
			{
				if (*elementPtr->SliderValuePtr < elementPtr->MaxSliderValue)
				{
					*elementPtr->SliderValuePtr += 1;
					EpisodeSelectScrollOffset -= ONE_FIXED;
				}
			}
			else
			{
				if (*elementPtr->SliderValuePtr > 0)
				{
					*elementPtr->SliderValuePtr -= 1;
					EpisodeSelectScrollOffset += ONE_FIXED;
				}
			}
			break;
		}

		case AVPMENU_ELEMENT_STARTMARINEDEMO:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				AvP.PlayerType = I_Marine;
				AvP.Difficulty = I_Medium; //bjd - was 1;
				SetLevelToLoad(AVP_ENVIRONMENT_INVASION);
			}
			break;
		}
		case AVPMENU_ELEMENT_STARTPREDATORDEMO:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				AvP.PlayerType = I_Predator;
				AvP.Difficulty = I_Medium; //bjd - was 1;
				SetLevelToLoad(AVP_ENVIRONMENT_INVASION_P);
			}
			break;
		}
		case AVPMENU_ELEMENT_STARTALIENDEMO:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				AvP.PlayerType = I_Alien;
				AvP.Difficulty = I_Medium; //bjd - was 1;
				SetLevelToLoad(AVP_ENVIRONMENT_INVASION_A);
			}
			break;
		}
		case AVPMENU_ELEMENT_STARTLEVELWITHCHEAT:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT && CheatMode_SelectionIsAllowed())
			{
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				AvP.PlayerType = static_cast<I_PLAYER_TYPE>(CheatMode_Species);
				AvP.Difficulty = I_Medium; //bjd - was 1;
				SetLevelToLoadForCheatMode(CheatMode_Environment);
			}
			break;
		}
		case AVPMENU_ELEMENT_STARTMPGAME:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				// save the 'Previous Game' multiplayer configuration
				{
					const char *previousGame = GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME);
					size_t const descriptionLength = strnlen(previousGame, sizeof(MP_Config_Description) - 1);
					memcpy(MP_Config_Description, previousGame, descriptionLength);
					MP_Config_Description[descriptionLength] = '\0';
				}
				SaveMultiplayerConfiguration(GetTextString(TEXTSTRING_PREVIOUSGAME_FILENAME));

				// Online, a host pressed before the server has answered waits for it on this page, and the row is pressed again for the player
				// once it's ready (added 2026-11-10 by Melanikus for the AvP port, PO's ruling Q3)
				if (AHostWaits(kHostWaitingToStart))
				{
					break;
				}

				Campaign_SetFromLevelRow();
				Campaign_SetDifficulty();
				Campaign_ChooseRollNumber();

				// A campaign is a co-op game. Any other kind can't carry one over, from a page visited earlier or a game left without going through its end
				if (netGameData.gameType != NGT_Coop)
				{
					netGameData.coopCampaign = kCampaignNone;
					netGameData.coopCampaignLevel = 0;
				}

				// Online, through the server servers.cfg names, or not at all (modified 2026-11-10 by Melanikus for the AvP port, the owner's ruling for beta.2: "Everything over nyc3")
				NetResult const hosted = Net_HostGame(MP_PlayerName, MP_SessionName, MP_Species, netGameData.gameType, netGameData.levelNumber);

				if (hosted != NET_FAIL)
				{
					AvPMenus.MenusState = MENUSSTATE_STARTGAME;
					SetLevelToLoadForThisNetworkGame();
					SetBriefingTextForMultiplayer();
				}
				else if (!AHostWaits(kHostWaitingToStart))
				{
					// It wasn't hosted: the page says why, rather than nothing at all (added 2026-11-10, PO's ruling Q4). Unless the server is
					// being asked again: the host asks it afresh (Net_HostGame), so a server whose last asking failed reads as not answered yet,
					// and then the page waits for its answer, as before a first answer (Third's finding on rc1, 2026-11-10)
					ShowWhyTheHostFailed(AvPMenus.CurrentMenu, Net_HostFailedWhy());
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_JOINMPGAME:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				if (!Net_HostsLevelIsOnItsList())
				{
					break;
				}

				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				netGameData.myStartFlag = 1;

				// The host has to hear this, or it never says this player has joined. The only other send is on the species page, before this was set.
				AddNetMsg_PlayerDescription();
				NetSendMessages();

				// What the host said, or he'd keep whatever his last single player game was set to, with
				// other health and armour than everyone else, and his damage worked out differently.
				Campaign_SetDifficulty();

				SetLevelToLoadForThisNetworkGame();
				SetBriefingTextForMultiplayer();
			}
			break;
		}
		case AVPMENU_ELEMENT_JOINANSWER:
		{
			// The host's answer to the player waiting to join: let in, or no. The two rows go once it's answered (UpdateTheAnswerRows).
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				OnlineHost_Answer(elementPtr->MaxValue == 1);
			}
			break;
		}
		case AVPMENU_ELEMENT_LISTCHOICE:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				// The row's game, where it is in the list of games found right now: found by its relay code or its address, never by the
				// row's number, as each search empties that list and fills it again. A game that's gone isn't joined.
				int const s = OnlinePage_SessionForRow(AvPMenus.CurrentlySelectedElement);

				if (s < 0)
				{
					OnlinePage_JoinFailed(OnlineScreens_Words(kOnlineWordsGameHasGone));
				}
				else if (SessionData[s].AllowedToJoin)
				{
					// copy the session name, leaving of the player count information
					char *bracket_pos;
					// The game's name comes from another computer on the LAN and can be up to 39 characters long, but MP_SessionName
					// holds only 33 bytes, so the copy stops there. A name that fits is copied exactly as before.
					size_t nameLength = strnlen(SessionData[s].Name, sizeof(MP_SessionName) - 1);
					memcpy(MP_SessionName, SessionData[s].Name, nameLength);
					MP_SessionName[nameLength] = '\0';

					bracket_pos = strrchr(MP_SessionName,'(');
					if (bracket_pos) {
						*bracket_pos = 0;
					}

					// The joining screen names the game's region. The region came from the list, and is made safe again where it's drawn.
					memcpy(joiningRegion, SessionData[s].relayRegion, sizeof(joiningRegion));
					joiningRegion[sizeof(joiningRegion) - 1] = '\0';

					// The page remembers this game, to select it again if the join ends and the menus come back to the page
					OnlinePage_Joining(AvPMenus.CurrentlySelectedElement);

					// Only while this join waits: the hook is taken off again straight after, whatever happened. The wait's frames close
					// this loop's frame, so the page opens a new one for it afterwards.
					Net_SetRelayJoinWaitHook(KeepTheWindowAliveWhileJoining);
					NetResult const joined = Net_ConnectToSession(s, MP_PlayerName);
					Net_SetRelayJoinWaitHook(NULL);
					OnlinePage_JoinWaitOver();

					if (joined != NET_FAIL) {
						SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
					}
					else if (SessionData[s].throughRelay) {
						// The networking code's own fixed words, such as "that game is full": nothing from the network
						LogString(std::string("Relay: couldn't join the game, because ") + Net_RelayJoinFailedWhy());

						// And on the page: Esc in the wait is the player's own choice, said in the page's words; anything else in the
						// networking code's
						if (strcmp(Net_RelayJoinFailedWhy(), NetRelay_WhyText(kNetRelayGivenUp)) == 0) {
							OnlinePage_JoinGivenUp();
						}
						else {
							OnlinePage_JoinFailed(Net_RelayJoinFailedWhy());
						}
					}
					else if (Net_JoinRefusedWhy()) {
						// Added 2026-11-10 by Melanikus for the AvP port (#188, PO's ruling Q4): a game on this network whose host isn't on the
						// home network is refused before anything opens, and the page says why, in the networking code's own fixed words
						LogString(std::string("net: couldn't join the game, because ") + Net_JoinRefusedWhy());
						OnlinePage_JoinFailed(Net_JoinRefusedWhy());
					}
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_JOINLOBBIED :
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				// save profile, so that multiplayer name is remembered
				SaveUserProfile(UserProfilePtr);

				InitAVPNetGameForJoin();
				SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
			}
			break;
		}

		case AVPMENU_ELEMENT_LOADMPCONFIG:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				LoadMultiplayerConfigurationByIndex(AvPMenus.CurrentlySelectedElement);
				SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
			}
			break;
		}

		case AVPMENU_ELEMENT_LOADIPADDRESS:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				LoadIPAddress(elementPtr->TextPtr);
				SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
			}
			break;
		}

		case AVPMENU_ELEMENT_CONNECTIONCHOICE :
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
			}
			break;
		}
		
		case AVPMENU_ELEMENT_VIDEOMODE:
		{
			if ((interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			  ||(interactionID == AVPMENU_ELEMENT_INTERACTION_INCREASE))
			{
				NextVideoMode2();
			}
			else
			{
				PreviousVideoMode2();
			}
			break;
		}
		case AVPMENU_ELEMENT_VIDEOMODEOK:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				SetAndSaveDeviceAndVideoModePreferences();
				SetupNewMenu(static_cast<enum AVPMENU_ID>(elementPtr->MenuToGoTo));
			}
			break;
		}
		case AVPMENU_ELEMENT_RESUMEGAME:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				IngameKeyboardInput_ClearBuffer();
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
			}
			break;
		}
		case AVPMENU_ELEMENT_RESTARTGAME:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				AvP.RestartLevel=1;
				AvPMenus.MenusState = MENUSSTATE_STARTGAME;
			}
			break;
		}

		case AVPMENU_ELEMENT_VIEWKEY:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				ViewSettings_ChoosingKey(elementPtr->MaxValue);
				AvPMenus.UserChangingViewKey = 1;
			}
			break;
		}
		case AVPMENU_ELEMENT_KEYCONFIG:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				AvPMenus.UserChangingKeyConfig = 1;
				if (KeyConfigSelectionColumn)
				{
					AvPMenus.ChangingPrimaryConfig = 0;
				}
				else
				{
					AvPMenus.ChangingPrimaryConfig = 1;
				}
			}
			else
			{
				KeyConfigSelectionColumn=!KeyConfigSelectionColumn;     
			}
			break;
		}
		case AVPMENU_ELEMENT_KEYCONFIGOK:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				switch (AvPMenus.CurrentMenu)
				{
					case AVPMENU_MARINEKEYCONFIG:
					{
						MarineInputPrimaryConfig   = PlayerInputPrimaryConfig;
						MarineInputSecondaryConfig = PlayerInputSecondaryConfig;
						{
							AutoWeaponChangeOn = AutoWeaponChangeOn_Temp;
						}
						break;
					}
					case AVPMENU_PREDATORKEYCONFIG:
					{
						PredatorInputPrimaryConfig   = PlayerInputPrimaryConfig;
						PredatorInputSecondaryConfig = PlayerInputSecondaryConfig;
						break;
					}
					case AVPMENU_ALIENKEYCONFIG:
					{
						AlienInputPrimaryConfig   = PlayerInputPrimaryConfig;
						AlienInputSecondaryConfig = PlayerInputSecondaryConfig;
						break;
					}
					case AVPMENU_CONTROLS:
					{
						ControlMethods = PlayerControlMethods;
						break;
					}
					case AVPMENU_JOYSTICKCONTROLS:
					{
						JoystickControlMethods = PlayerJoystickControlMethods;
						break;
					}
				}
				
				SaveUserProfile(UserProfilePtr);

				if(AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
				{
					SetupNewMenu(AVPMENU_INGAME);
				}
				else
				{
					SetupNewMenu(AVPMENU_OPTIONS);
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_RESETKEYCONFIG:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				switch (AvPMenus.CurrentMenu)
				{
					case AVPMENU_MARINEKEYCONFIG:
					{
						PlayerInputPrimaryConfig = DefaultMarineInputPrimaryConfig;
						PlayerInputSecondaryConfig = DefaultMarineInputSecondaryConfig;
						break;
					}
					case AVPMENU_PREDATORKEYCONFIG:
					{
						PlayerInputPrimaryConfig = DefaultPredatorInputPrimaryConfig;
						PlayerInputSecondaryConfig = DefaultPredatorInputSecondaryConfig;
						break;
					}
					case AVPMENU_ALIENKEYCONFIG:
					{
						PlayerInputPrimaryConfig = DefaultAlienInputPrimaryConfig;
						PlayerInputSecondaryConfig = DefaultAlienInputSecondaryConfig;
						break;
					}
					case AVPMENU_CONTROLS:
					{
						PlayerControlMethods = DefaultControlMethods;
						break;
					}
					case AVPMENU_JOYSTICKCONTROLS:
					{
						PlayerJoystickControlMethods = DefaultJoystickControlMethods;
						break;
					}
				}
			}
			break;
		}

		case AVPMENU_ELEMENT_RESETMPCONFIG :
		{
			//reset the multiplayer configuration
			SetDefaultMultiplayerConfig();
			break;
		}

		case AVPMENU_ELEMENT_SAVESETTINGS:
		{
			if (interactionID == AVPMENU_ELEMENT_INTERACTION_SELECT)
			{
				SetDetailLevelsFromMenu();
				SaveUserProfile(UserProfilePtr);
				
				if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
				{
					if (AvPMenus.CurrentMenu == AVPMENU_DETAILLEVELS)
					{
						SetupNewMenu(AVPMENU_INGAMEAVOPTIONS);
					}
					else
					{
						SetupNewMenu(AVPMENU_INGAME);
					}
				}
				else
				{
					SetupNewMenu(AvPMenusData[AvPMenus.CurrentMenu].ParentMenu);
				}
			}
			break;
		}
	
		case AVPMENU_ELEMENT_LOADGAME:
		{
			int slot = AvPMenus.CurrentlySelectedElement;
			if(slot>=0 && slot<NUMBER_OF_SAVE_SLOTS)
			{
				if(SaveGameSlot[slot].SlotUsed)
				{
					//set the request
					LoadGameRequest = slot;
					if(AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
					{
						//leave the menus
						AvPMenus.MenusState = MENUSSTATE_STARTGAME;	
					}
				}
			}
			break;
		}
		case AVPMENU_ELEMENT_SAVEGAME:
		{
			int slot = AvPMenus.CurrentlySelectedElement;
			if(slot>=0 && slot<NUMBER_OF_SAVE_SLOTS)
			{
				//set the request
				SaveGameRequest = slot;
				if(AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
				{
					//leave the menus
					AvPMenus.MenusState = MENUSSTATE_STARTGAME;
				}
			}
			break;
		}
	}
}

static void RenderMenuElement(AVPMENU_ELEMENT *elementPtr, int e, int y)
{
	int (*RenderText)(char *textPtr, int x, int y, int alpha, enum AVPMENUFORMAT_ID format);
	int (*RenderText_Coloured)(char *textPtr, int x, int y, int alpha, enum AVPMENUFORMAT_ID format, int r, int g, int b);

	if (AvPMenus.FontToUse==AVPMENU_FONT_BIG)
	{
		RenderText = RenderMenuText;
		RenderText_Coloured = Hardware_RenderSmallMenuText_Coloured;
	}
	else
	{
#if 0
		if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
		{
			RenderText = Hardware_RenderSmallMenuText;
			RenderText_Coloured = Hardware_RenderSmallMenuText_Coloured;
		}
		else
#endif
		{
			RenderText = RenderSmallMenuText;
			RenderText_Coloured = RenderSmallMenuText_Coloured;
		}
	}

	switch(elementPtr->ElementID)
	{
		default:
		GLOBALASSERT("UNKNOWN MENU ELEMENT"==0);
		case AVPMENU_ELEMENT_FINDAGAIN:
		case AVPMENU_ELEMENT_GOTOMENU:
		case AVPMENU_ELEMENT_QUITGAME:
		case AVPMENU_ELEMENT_STARTMPGAME:
		case AVPMENU_ELEMENT_JOINMPGAME:
		case AVPMENU_ELEMENT_VIDEOMODEOK:
		case AVPMENU_ELEMENT_RESUMEGAME:
		case AVPMENU_ELEMENT_RESTARTGAME:
		case AVPMENU_ELEMENT_KEYCONFIGOK:
		case AVPMENU_ELEMENT_RESETKEYCONFIG:
		case AVPMENU_ELEMENT_STARTMARINEDEMO:
		case AVPMENU_ELEMENT_STARTPREDATORDEMO:
		case AVPMENU_ELEMENT_STARTALIENDEMO:
		case AVPMENU_ELEMENT_STARTLEVELWITHCHEAT:
		case AVPMENU_ELEMENT_SAVEMPCONFIG:
		case AVPMENU_ELEMENT_DIFFICULTYLEVEL:
		case AVPMENU_ELEMENT_CUSTOMMAP_START:
		case AVPMENU_ELEMENT_COOP_START:
		case AVPMENU_ELEMENT_ONLINE_ACCESS:
		case AVPMENU_ELEMENT_JOINLOBBIED:
		case AVPMENU_ELEMENT_CONNECTIONCHOICE :
		case AVPMENU_ELEMENT_USERPROFILE_DELETE:
		case AVPMENU_ELEMENT_RESETMPCONFIG:
		case AVPMENU_ELEMENT_DELETEMPCONFIG:
		case AVPMENU_ELEMENT_SAVESETTINGS:
		{
			char *textPtr = GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription));

			// general text rendering
#if 0
			if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
			{
				int menuCentreX = ScreenDescriptorBlock.SDB_Width / 2;
				RenderText(textPtr, menuCentreX, y, elementPtr->Brightness, AVPMENUFORMAT_CENTREJUSTIFIED);
				break;
			}
			else
#endif
			{
				RenderText(textPtr, MENU_CENTREX, y, elementPtr->Brightness, AVPMENUFORMAT_CENTREJUSTIFIED);
				break;
			}
		}

		case AVPMENU_ELEMENT_DUMMYTEXTSLIDER:
		case AVPMENU_ELEMENT_DUMMYTEXTSLIDER_POINTER:
		case AVPMENU_ELEMENT_TEXTSLIDER:
		case AVPMENU_ELEMENT_TEXTSLIDER_POINTER:
		case AVPMENU_ELEMENT_SPECIES_TEXTSLIDER:
		case AVPMENU_ELEMENT_CHEATMODE_TEXTSLIDER:
		case AVPMENU_ELEMENT_CHEATMODE_SPECIES_TEXTSLIDER:
		case AVPMENU_ELEMENT_CHEATMODE_ENVIRONMENT_TEXTSLIDER:
		{
			char *textPtr = "";
			if (elementPtr->ElementID == AVPMENU_ELEMENT_TEXTSLIDER_POINTER ||
			    elementPtr->ElementID == AVPMENU_ELEMENT_DUMMYTEXTSLIDER_POINTER)	
			{
				//we have a pointer to the strings rather than the first string index
				if (elementPtr->TextSliderStringPointer)
				{
					textPtr = elementPtr->TextSliderStringPointer[*(elementPtr->SliderValuePtr)];
				}
			}
			else
			{
				//we have the index of the first string
				textPtr = GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->FirstTextSliderString+*(elementPtr->SliderValuePtr)));

				// The new classes' names aren't next to stock's in the game's text
				if (elementPtr->ElementID == AVPMENU_ELEMENT_SPECIES_TEXTSLIDER && ClassSlider_NewNameIndex(*elementPtr->SliderValuePtr) >= 0)
				{
					textPtr = GetTextString(static_cast<enum TEXTSTRING_ID>(TEXTSTRING_MULTIPLAYER_MARINE_SHOTGUN + ClassSlider_NewNameIndex(*elementPtr->SliderValuePtr)));
				}
			}
			
			if (elementPtr->TextDescription!=TEXTSTRING_BLANK)
			{
#if 0
				if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
				{
					RenderText
					(
						GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),
						MENU_CENTREX-MENU_ELEMENT_SPACING,
						y,
						elementPtr->Brightness,
						AVPMENUFORMAT_RIGHTJUSTIFIED
					);
					RenderText
					(
						textPtr,
						MENU_CENTREX+MENU_ELEMENT_SPACING,
						y,
						elementPtr->Brightness,
						AVPMENUFORMAT_LEFTJUSTIFIED
					);
				}
				else
#endif
				{
					int length = LengthOfMenuText(textPtr);

					if (length>ScreenDescriptorBlock.SDB_Width-MENU_CENTREX-MENU_ELEMENT_SPACING*2)
					{
						RenderText
						(
							GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),
							ScreenDescriptorBlock.SDB_Width-MENU_ELEMENT_SPACING*2-length,
							y,
							elementPtr->Brightness,
							AVPMENUFORMAT_RIGHTJUSTIFIED
						);
						RenderText
						(
							textPtr,
							ScreenDescriptorBlock.SDB_Width-MENU_ELEMENT_SPACING,
							y,
							elementPtr->Brightness,
							AVPMENUFORMAT_RIGHTJUSTIFIED
						);
					}
					else
					{

						RenderText
						(
							GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),
							MENU_CENTREX-MENU_ELEMENT_SPACING,
							y,
							elementPtr->Brightness,
							AVPMENUFORMAT_RIGHTJUSTIFIED
						);
						RenderText
						(
							textPtr,
							MENU_CENTREX+MENU_ELEMENT_SPACING,
							y,
							elementPtr->Brightness,
							AVPMENUFORMAT_LEFTJUSTIFIED
						);
					}
				}
			}
			else
			{
				RenderText
				(
					textPtr,
					MENU_CENTREX,
					y,
					elementPtr->Brightness,
					AVPMENUFORMAT_CENTREJUSTIFIED
				);
			}
			break;
		}
		case AVPMENU_ELEMENT_DUMMYTEXTFIELD:
		case AVPMENU_ELEMENT_TEXTFIELD:
		{
			// field where user can enter text themselves (profile name etc)
			if (elementPtr->TextDescription==TEXTSTRING_BLANK)
			{
				if (AvPMenus.UserEnteringText && e==AvPMenus.CurrentlySelectedElement)
				{
					int b = GetSin(CloakingPhase&4095);
					int x = RenderText(TextFieldShows(elementPtr),MENU_CENTREX,y,elementPtr->Brightness/2,AVPMENUFORMAT_CENTREJUSTIFIED);
					x = RenderText("I",x,y,MUL_FIXED(b,b),AVPMENUFORMAT_CENTREJUSTIFIED);

					//work out how much space was left over
					if(AvPMenus.PositionInTextField)
						AvPMenus.WidthLeftForText = MENU_RIGHTXEDGE -x;
					else
						AvPMenus.WidthLeftForText = MENU_RIGHTXEDGE -MENU_CENTREX;
				}
				else
				{
					RenderText(TextFieldShows(elementPtr),MENU_CENTREX,y,elementPtr->Brightness,AVPMENUFORMAT_CENTREJUSTIFIED);
				}
			}
			else
			{
				RenderText(GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),MENU_CENTREX-MENU_ELEMENT_SPACING,y,elementPtr->Brightness,AVPMENUFORMAT_RIGHTJUSTIFIED);
			
				if (AvPMenus.UserEnteringText && e==AvPMenus.CurrentlySelectedElement)
				{
					int b = GetSin(CloakingPhase&4095);
					int x = RenderText(TextFieldShows(elementPtr),MENU_CENTREX+MENU_ELEMENT_SPACING,y,elementPtr->Brightness/2,AVPMENUFORMAT_LEFTJUSTIFIED);
					x = RenderText("I",x,y,MUL_FIXED(b,b),AVPMENUFORMAT_LEFTJUSTIFIED);
					
					//work out how much space was left over
					if(AvPMenus.PositionInTextField)
						AvPMenus.WidthLeftForText = MENU_RIGHTXEDGE -x;
					else
						AvPMenus.WidthLeftForText = MENU_RIGHTXEDGE -(MENU_CENTREX+MENU_ELEMENT_SPACING);
				}
				else
				{
					RenderText(TextFieldShows(elementPtr),MENU_CENTREX+MENU_ELEMENT_SPACING,y,elementPtr->Brightness,AVPMENUFORMAT_LEFTJUSTIFIED);
				}
			}
			break;
		}

		case AVPMENU_ELEMENT_TEXTFIELD_SMALLWRAPPED:
		{
			if (elementPtr->TextDescription==TEXTSTRING_BLANK)
			{
				RECT area;
				area.left=MENU_LEFTXEDGE;
				area.right=MENU_RIGHTXEDGE;
				area.top=y;
				area.bottom=y+4*HUD_FONT_HEIGHT;
				
				if (AvPMenus.UserEnteringText && e==AvPMenus.CurrentlySelectedElement)
				{
					int output_x,output_y;
					int b = GetSin(CloakingPhase&4095);
					RenderSmallFontString_Wrapped(elementPtr->TextPtr,&area,elementPtr->Brightness/2,&output_x,&output_y);
					output_x = RenderSmallMenuText("I",output_x,output_y,MUL_FIXED(b,b),AVPMENUFORMAT_LEFTJUSTIFIED);
					
					//work out how much space was left over
					if(area.bottom - output_y < HUD_FONT_HEIGHT)
					{
						//no space left
						AvPMenus.WidthLeftForText= 0;
					}
					else if(area.bottom - output_y >= 2*HUD_FONT_HEIGHT)
					{
						//at least a whole line left
						AvPMenus.WidthLeftForText= area.right-area.left;
					}
					else
					{
						//on the last line
						AvPMenus.WidthLeftForText= area.right-output_x;
					}
				}
				else
				{
					RenderSmallFontString_Wrapped(elementPtr->TextPtr,&area,elementPtr->Brightness,0,0);
				}
			}
			else
			{
				RECT area;
				area.left=MENU_CENTREX+MENU_ELEMENT_SPACING;
				area.right=MENU_RIGHTXEDGE;
				area.top=y;
				area.bottom=y+4*HUD_FONT_HEIGHT;
				
				RenderText(GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),MENU_CENTREX-MENU_ELEMENT_SPACING,y+HUD_FONT_HEIGHT,elementPtr->Brightness,AVPMENUFORMAT_RIGHTJUSTIFIED);
			
				if (AvPMenus.UserEnteringText && e==AvPMenus.CurrentlySelectedElement)
				{
					int output_x,output_y;
					int b = GetSin(CloakingPhase&4095);
					RenderSmallFontString_Wrapped(elementPtr->TextPtr,&area,elementPtr->Brightness/2,&output_x,&output_y);
					output_x = RenderSmallMenuText("I",output_x,output_y,MUL_FIXED(b,b),AVPMENUFORMAT_LEFTJUSTIFIED);
					
					//work out how much space was left over
					if(area.bottom - output_y < HUD_FONT_HEIGHT)
					{
						//no space left
						AvPMenus.WidthLeftForText= 0;
					}
					else if(area.bottom - output_y >= 2*HUD_FONT_HEIGHT)
					{
						//at least a whole line left
						AvPMenus.WidthLeftForText= area.right-area.left;
					}
					else
					{
						//on the last line
						AvPMenus.WidthLeftForText= area.right-output_x;
					}
				}
				else
				{
					RenderSmallFontString_Wrapped(elementPtr->TextPtr,&area,elementPtr->Brightness,0,0);
				}
			}
			break;
		}

		case AVPMENU_ELEMENT_DUMMYNUMBERFIELD:
		case AVPMENU_ELEMENT_NUMBERFIELD:
		{
			{
				static char NumberString[50];
				if(*elementPtr->NumberPtr!=0 || elementPtr->NumberFieldZeroString==TEXTSTRING_BLANK)
				{
					if(elementPtr->NumberFieldUnitsString!=TEXTSTRING_BLANK)
					{
						//add the text for the unit type
						_snprintf(NumberString,sizeof(NumberString)-1,"%d %s",*elementPtr->NumberPtr,GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->NumberFieldUnitsString)));
					}
					else
					{
						_snprintf(NumberString,sizeof(NumberString)-1,"%d",*elementPtr->NumberPtr);
					}
					NumberString[sizeof(NumberString)-1] = '\0';
				}
				else
				{
					//use the special string to represent 0
					// The special word shown instead of 0 comes from the game's language text
					// file on disk. NumberString holds 50 bytes, so a line made very long in that
					// file must not be copied in whole: _snprintf writes at most 49 characters,
					// and we always put the end marker (the zero byte that ends a text) into the
					// last byte ourselves, because _snprintf leaves it out when it cuts the text
					// short. A word that fits comes out exactly as before.
					_snprintf(NumberString,sizeof(NumberString)-1,"%s",GetTextString(elementPtr->NumberFieldZeroString));
					NumberString[sizeof(NumberString)-1] = '\0';
				}
				
				if (elementPtr->TextDescription==TEXTSTRING_BLANK)
				{
					if (AvPMenus.UserEnteringNumber && e==AvPMenus.CurrentlySelectedElement)
					{
						int b = GetSin(CloakingPhase&4095);
						int x = RenderText(NumberString,MENU_CENTREX,y,elementPtr->Brightness/2,AVPMENUFORMAT_CENTREJUSTIFIED);
						RenderText("I",x,y,MUL_FIXED(b,b),AVPMENUFORMAT_CENTREJUSTIFIED);
					}
					else
					{
						RenderText(NumberString,MENU_CENTREX,y,elementPtr->Brightness,AVPMENUFORMAT_CENTREJUSTIFIED);
					}
				}
				else
				{
					RenderText(GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),MENU_CENTREX-MENU_ELEMENT_SPACING,y,elementPtr->Brightness,AVPMENUFORMAT_RIGHTJUSTIFIED);

					if (AvPMenus.UserEnteringNumber && e==AvPMenus.CurrentlySelectedElement)
					{
						int b = GetSin(CloakingPhase&4095);
						int x = RenderText(NumberString,MENU_CENTREX+MENU_ELEMENT_SPACING,y,elementPtr->Brightness/2,AVPMENUFORMAT_LEFTJUSTIFIED);
						RenderText("I",x,y,MUL_FIXED(b,b),AVPMENUFORMAT_LEFTJUSTIFIED);
					}
					else
					{
						RenderText(NumberString,MENU_CENTREX+MENU_ELEMENT_SPACING,y,elementPtr->Brightness,AVPMENUFORMAT_LEFTJUSTIFIED);
					}
				}
			}
			break;
		}
		
		case AVPMENU_ELEMENT_GOTOMENU_GFX:
		{
			DrawAvPMenuGfx(elementPtr->textureID, MENU_CENTREX,y,elementPtr->Brightness,AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		}
		case AVPMENU_ELEMENT_COOP_PLAYER:
		case AVPMENU_ELEMENT_COOP_REMOVE:
		case AVPMENU_ELEMENT_JOINANSWER:
		case AVPMENU_ELEMENT_LISTCHOICE:
		{
			RenderText(elementPtr->TextPtr,MENU_CENTREX,y,elementPtr->Brightness,AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		}
		case AVPMENU_ELEMENT_LOADMPCONFIG:
		{
			RenderText(elementPtr->TextPtr,MENU_CENTREX,y,elementPtr->Brightness,AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		}
		case AVPMENU_ELEMENT_LOADIPADDRESS:
		{
			RenderText(elementPtr->TextPtr,MENU_CENTREX,y,elementPtr->Brightness,AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		}
		
		case AVPMENU_ELEMENT_TOGGLE:
		{
			break;
		}
		case AVPMENU_ELEMENT_SLIDER:
		{
			int x = MENU_CENTREX + MENU_ELEMENT_SPACING + 3;
			x += (201 * (*elementPtr->SliderValuePtr)) / elementPtr->MaxSliderValue;

			RenderText(GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),MENU_CENTREX-MENU_ELEMENT_SPACING,y,elementPtr->Brightness,AVPMENUFORMAT_RIGHTJUSTIFIED);
#if 0
			if (AvPMenus.MenusState == MENUSSTATE_INGAMEMENUS)
			{
				D3D_DrawSliderBar(/*MENU_CENTREX*/(/*ScreenDescriptorBlock.SDB_Width*/640/2)+MENU_ELEMENT_SPACING, y+1, elementPtr->Brightness);
				D3D_DrawSlider(x, y+4, elementPtr->Brightness);
			}
			else
#endif
			{
				DrawAvPMenuGfx(AVPMENUGFX_SLIDERBAR, MENU_CENTREX+MENU_ELEMENT_SPACING, y+1, elementPtr->Brightness, AVPMENUFORMAT_LEFTJUSTIFIED);
				DrawAvPMenuGfx(AVPMENUGFX_SLIDER, x, y+4, elementPtr->Brightness, AVPMENUFORMAT_LEFTJUSTIFIED);
			}

			break;
		}
		case AVPMENU_ELEMENT_VIDEOMODE:
		{
			RenderText
			(
				GetVideoModeDescription2(),
				MENU_CENTREX,
				y,
				elementPtr->Brightness,
				AVPMENUFORMAT_CENTREJUSTIFIED
			);
			RenderText
			(
				GetVideoModeDescription3(),
				MENU_CENTREX,
				y+MENU_FONT_HEIGHT+MENU_ELEMENT_SPACING,
				elementPtr->Brightness,
				AVPMENUFORMAT_CENTREJUSTIFIED
			);
			break;
		}
		case AVPMENU_ELEMENT_VIEWKEY:
		{
			int const line = elementPtr->MaxValue;
			char *const keyName = GetDescriptionOfKey(static_cast<unsigned char>(ViewSettings_KeyOnLine(line)));

			RenderText
			(
				GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),
				MENU_CENTREX-MENU_ELEMENT_SPACING,
				y,
				elementPtr->Brightness,
				AVPMENUFORMAT_RIGHTJUSTIFIED
			);

			if (AvPMenus.UserChangingViewKey && e==AvPMenus.CurrentlySelectedElement)
			{
				int b = GetSin(CloakingPhase&4095);
				RenderText("_",MENU_CENTREX+MENU_ELEMENT_SPACING,y,MUL_FIXED(b,b),AVPMENUFORMAT_LEFTJUSTIFIED);
			}
			else if (ViewSettings_LineIsHeldByAControl(line))
			{
				RenderText_Coloured(keyName,MENU_CENTREX+MENU_ELEMENT_SPACING,y,ONE_FIXED,AVPMENUFORMAT_LEFTJUSTIFIED,ONE_FIXED,ONE_FIXED,0);
			}
			else
			{
				RenderText(keyName,MENU_CENTREX+MENU_ELEMENT_SPACING,y,elementPtr->Brightness,AVPMENUFORMAT_LEFTJUSTIFIED);
			}
			break;
		}
		case AVPMENU_ELEMENT_KEYCONFIG:
		{
			unsigned char *primaryKey = ((unsigned char*)&PlayerInputPrimaryConfig)+e-2;
			unsigned char *secondaryKey = ((unsigned char*)&PlayerInputSecondaryConfig)+e-2;
			if (e==AvPMenus.CurrentlySelectedElement)
			{
				int x,g;
				if (KeyConfigSelectionColumn)
				{
					/* bjd - 24/11/08 - tv safe zone adjustment */
					x = MENU_RIGHTXEDGE - ScreenDescriptorBlock.SDB_SafeZoneWidthOffset;
				}
				else
				{
					x = MENU_CENTREX;
				}
				if (AvPMenus.UserChangingKeyConfig)
				{
					g = 255;
				}
				else
				{
					g = 128;
				}

				RenderHighlightRectangle(x-100,y-4,x+4,y+19,0,g,0);
			}
			if (AvPMenus.UserChangingKeyConfig && e==AvPMenus.CurrentlySelectedElement)
			{
				int b = GetSin(CloakingPhase&4095);
				if (AvPMenus.ChangingPrimaryConfig)
				{
					RenderText("_",MENU_CENTREX,y,MUL_FIXED(b,b),AVPMENUFORMAT_RIGHTJUSTIFIED);
					RenderText
					(
						GetDescriptionOfKey(*secondaryKey),
						/* bjd - 24/11/08 - tv safe zone adjustment */
						MENU_RIGHTXEDGE - ScreenDescriptorBlock.SDB_SafeZoneWidthOffset,
						y,
						elementPtr->Brightness,
						AVPMENUFORMAT_RIGHTJUSTIFIED
					);
				}
				else
				{
					RenderText
					(
						GetDescriptionOfKey(*primaryKey),
						MENU_CENTREX,
						y,
						elementPtr->Brightness,
						AVPMENUFORMAT_RIGHTJUSTIFIED
					);
					RenderText("_",MENU_RIGHTXEDGE,y,MUL_FIXED(b,b),AVPMENUFORMAT_RIGHTJUSTIFIED);
				}
			}
			else
			{
				if (MultipleAssignments[0][e-2])
				{
					RenderText_Coloured
					(
						GetDescriptionOfKey(*primaryKey),
						MENU_CENTREX,
						y,
						ONE_FIXED,
						AVPMENUFORMAT_RIGHTJUSTIFIED,
						ONE_FIXED,
						ONE_FIXED,
						0
					);
				}
				else
				{
					RenderText
					(
						GetDescriptionOfKey(*primaryKey),
						MENU_CENTREX,
						y,
						elementPtr->Brightness,
						AVPMENUFORMAT_RIGHTJUSTIFIED
					);
				}
				if (MultipleAssignments[1][e-2])
				{
					RenderText_Coloured
					(
						GetDescriptionOfKey(*secondaryKey),
						/* bjd - 24/11/08 - tv safe zone adjustment */
						MENU_RIGHTXEDGE - ScreenDescriptorBlock.SDB_SafeZoneWidthOffset,
						y,
						ONE_FIXED,
						AVPMENUFORMAT_RIGHTJUSTIFIED,
						ONE_FIXED,
						ONE_FIXED,
						0
					);
				}
				else
				{
					RenderText
					(
						GetDescriptionOfKey(*secondaryKey),
						/* bjd - 24/11/08 - tv safe zone adjustment */
						MENU_RIGHTXEDGE - ScreenDescriptorBlock.SDB_SafeZoneWidthOffset,
						y,
						elementPtr->Brightness,
						AVPMENUFORMAT_RIGHTJUSTIFIED
					);
				}
			}

			RenderText
			(
				GetTextString(static_cast<enum TEXTSTRING_ID>(elementPtr->TextDescription)),
				/* bjd - 24/11/08 - tv safe zone adjustment */
				MENU_LEFTXEDGE + ScreenDescriptorBlock.SDB_SafeZoneWidthOffset / 2,
				y,
				elementPtr->Brightness,
				AVPMENUFORMAT_LEFTJUSTIFIED
			);
		}
	}
}

static int HeightOfMenuElement(AVPMENU_ELEMENT *elementPtr)
{
	int h = MENU_ELEMENT_SPACING;

	switch(elementPtr->ElementID)
	{
		default:
		{
			if (AvPMenus.FontToUse==AVPMENU_FONT_BIG)
			{
				h += MENU_FONT_HEIGHT;
			}
			else
			{
				h = HUD_FONT_HEIGHT+4;
			}
			break;
		}
		case AVPMENU_ELEMENT_LOADGAME:
		case AVPMENU_ELEMENT_SAVEGAME:
		{
			h= HUD_FONT_HEIGHT*2+4;
			break;
		}
		case AVPMENU_ELEMENT_VIDEOMODE:
		{
			h += MENU_FONT_HEIGHT;
			h *=2;
			break;
		}

		case AVPMENU_ELEMENT_TEXTFIELD_SMALLWRAPPED :
		{
			h+=HUD_FONT_HEIGHT*4;
			break;
		}

		case AVPMENU_ELEMENT_GOTOMENU_GFX:
		{
			h += HeightOfMenuGfx(elementPtr->textureID);
			break;
		}
	}
	return h;
}

char *GetDescriptionOfKey(unsigned char key)
{
	static char KeyDescBuffer[2];
	KeyDescBuffer[0] = '\0';
	KeyDescBuffer[1] = '\0';

	char *textPtr;
	
	switch (key)
	{
		case KEY_UP:
			textPtr = GetTextString(TEXTSTRING_KEYS_UP);
			break;
		case KEY_DOWN:
			textPtr = GetTextString(TEXTSTRING_KEYS_DOWN);
			break;
		case KEY_LEFT:
			textPtr = GetTextString(TEXTSTRING_KEYS_LEFT);
			break;
		case KEY_RIGHT:
			textPtr = GetTextString(TEXTSTRING_KEYS_RIGHT);
			break;
		case KEY_CR:
			textPtr = GetTextString(TEXTSTRING_KEYS_RETURN);
			break;
		case KEY_TAB:
			textPtr = GetTextString(TEXTSTRING_KEYS_TAB);
			break;
		case KEY_INS:
			textPtr = GetTextString(TEXTSTRING_KEYS_INSERT);
			break;
		case KEY_DEL:
			textPtr = GetTextString(TEXTSTRING_KEYS_DELETE);
			break;
		case KEY_END:
			textPtr = GetTextString(TEXTSTRING_KEYS_END);
			break;
		case KEY_HOME:
			textPtr = GetTextString(TEXTSTRING_KEYS_HOME);
			break;
		case KEY_PAGEUP:
			textPtr = GetTextString(TEXTSTRING_KEYS_PGUP);
			break;
		case KEY_PAGEDOWN:
			textPtr = GetTextString(TEXTSTRING_KEYS_PGDOWN);
			break;
		case KEY_BACKSPACE:
			textPtr = GetTextString(TEXTSTRING_KEYS_BACKSP);
			break;
		case KEY_COMMA:
			textPtr = GetTextString(TEXTSTRING_KEYS_COMMA);
			break;
		case KEY_FSTOP:
			textPtr = GetTextString(TEXTSTRING_KEYS_PERIOD);
			break;
		case KEY_SPACE:
			textPtr = GetTextString(TEXTSTRING_KEYS_SPACE);
			break;
		case KEY_LMOUSE:
			textPtr = GetTextString(TEXTSTRING_KEYS_LMOUSE);
			break;
		case KEY_RMOUSE:
			textPtr = GetTextString(TEXTSTRING_KEYS_RMOUSE);
			break;
		case KEY_MMOUSE:
			textPtr = GetTextString(TEXTSTRING_KEYS_MMOUSE);
			break;
		case KEY_MOUSEBUTTON4:
			textPtr = GetTextString(TEXTSTRING_KEYS_MOUSEBUTTON4);
			break;
		case KEY_MOUSEWHEELUP:
			textPtr = GetTextString(TEXTSTRING_KEYS_MOUSEWHEELUP);
			break;
		case KEY_MOUSEWHEELDOWN:
			textPtr = GetTextString(TEXTSTRING_KEYS_MOUSEWHEELDOWN);
			break;
		case KEY_LEFTALT:
			textPtr = GetTextString(TEXTSTRING_KEYS_LALT);
			break;
		case KEY_RIGHTALT:
			textPtr = GetTextString(TEXTSTRING_KEYS_RALT);
			break;
		case KEY_LEFTCTRL:
			textPtr = GetTextString(TEXTSTRING_KEYS_LCTRL);
			break;
		case KEY_RIGHTCTRL:
			textPtr = GetTextString(TEXTSTRING_KEYS_RCTRL);
			break;
		case KEY_LEFTSHIFT:
			textPtr = GetTextString(TEXTSTRING_KEYS_LSHIFT);
			break;
		case KEY_RIGHTSHIFT:
			textPtr = GetTextString(TEXTSTRING_KEYS_RSHIFT);
			break;
		case KEY_CAPS:
			textPtr = GetTextString(TEXTSTRING_KEYS_CAPS);
			break;
		case KEY_NUMLOCK:
			textPtr = GetTextString(TEXTSTRING_KEYS_NUMLOCK);
			break;
		case KEY_SCROLLOK:
			textPtr = GetTextString(TEXTSTRING_KEYS_SCRLOCK);
			break;
		case KEY_NUMPAD0:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD0);
			break;
		case KEY_NUMPAD1:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD1);
			break;
		case KEY_NUMPAD2:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD2);
			break;
		case KEY_NUMPAD3:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD3);
			break;
		case KEY_NUMPAD4:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD4);
			break;
		case KEY_NUMPAD5:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD5);
			break;
		case KEY_NUMPAD6:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD6);
			break;
		case KEY_NUMPAD7:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD7);
			break;
		case KEY_NUMPAD8:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD8);
			break;
		case KEY_NUMPAD9:
			textPtr = GetTextString(TEXTSTRING_KEYS_PAD9);
			break;
		case KEY_NUMPADSUB:
			textPtr = GetTextString(TEXTSTRING_KEYS_PADSUB);
			break;
		case KEY_NUMPADADD:
			textPtr = GetTextString(TEXTSTRING_KEYS_PADADD);
			break;
		case KEY_NUMPADDEL:
			textPtr = GetTextString(TEXTSTRING_KEYS_PADDEL);
			break;

		case KEY_JOYSTICK_BUTTON_1:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_1);
			break;
		case KEY_JOYSTICK_BUTTON_2:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_2);
			break;
		case KEY_JOYSTICK_BUTTON_3:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_3);
			break;
		case KEY_JOYSTICK_BUTTON_4:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_4);
			break;
		case KEY_JOYSTICK_BUTTON_5:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_5);
			break;
		case KEY_JOYSTICK_BUTTON_6:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_6);
			break;
		case KEY_JOYSTICK_BUTTON_7:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_7);
			break;
		case KEY_JOYSTICK_BUTTON_8:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_8);
			break;
		case KEY_JOYSTICK_BUTTON_9:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_9);
			break;
		case KEY_JOYSTICK_BUTTON_10:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_10);
			break;
		case KEY_JOYSTICK_BUTTON_11:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_11);
			break;
		case KEY_JOYSTICK_BUTTON_12:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_12);
			break;
		case KEY_JOYSTICK_BUTTON_13:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_13);
			break;
		case KEY_JOYSTICK_BUTTON_14:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_14);
			break;
		case KEY_JOYSTICK_BUTTON_15:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_15);
			break;
		case KEY_JOYSTICK_BUTTON_16:
			textPtr = GetGamePadButtonTextString(TEXTSTRING_KEYS_JOYSTICKBUTTON_16);
			break;

		case KEY_LBRACKET:
			textPtr = GetTextString(TEXTSTRING_KEYS_LBRACKET);
			break;
		case KEY_RBRACKET:
			textPtr = GetTextString(TEXTSTRING_KEYS_RBRACKET);
			break;
		case KEY_SEMICOLON:
			textPtr = GetTextString(TEXTSTRING_KEYS_SEMICOLON);
			break;
		case KEY_APOSTROPHE:
			textPtr = GetTextString(TEXTSTRING_KEYS_APOSTROPHE);
			break;
		case KEY_GRAVE:
			textPtr = GetTextString(TEXTSTRING_KEYS_GRAVE);
			break;
		case KEY_BACKSLASH:
			textPtr = GetTextString(TEXTSTRING_KEYS_BACKSLASH);
			break;
		case KEY_SLASH:
			textPtr = GetTextString(TEXTSTRING_KEYS_SLASH);
			break;
		case KEY_NUMPADENTER:
			textPtr = GetTextString(TEXTSTRING_KEYS_NUMPADENTER);
			break;
		case KEY_NUMPADDIVIDE:
			textPtr = GetTextString(TEXTSTRING_KEYS_NUMPADDIVIDE);
			break;
		case KEY_NUMPADMULTIPLY:
			textPtr = GetTextString(TEXTSTRING_KEYS_NUMPADMULTIPLY);
			break;
		case KEY_CAPITAL:
			textPtr = GetTextString(TEXTSTRING_KEYS_CAPITAL);
			break;
		case KEY_MINUS:
			textPtr = GetTextString(TEXTSTRING_KEYS_MINUS);
			break;
		case KEY_EQUALS:
			textPtr = GetTextString(TEXTSTRING_KEYS_EQUALS);
			break;
		case KEY_LWIN:
			textPtr = GetTextString(TEXTSTRING_KEYS_LWIN);
			break;
		case KEY_RWIN:
			textPtr = GetTextString(TEXTSTRING_KEYS_RWIN);
			break;
		case KEY_APPS:
			textPtr = GetTextString(TEXTSTRING_KEYS_APPS);
			break;
		case KEY_F1:
			textPtr = GetTextString(TEXTSTRING_KEYS_F1);
			break;
		case KEY_F2:
			textPtr = GetTextString(TEXTSTRING_KEYS_F2);
			break;
		case KEY_F3:
			textPtr = GetTextString(TEXTSTRING_KEYS_F3);
			break;
		case KEY_F4:
			textPtr = GetTextString(TEXTSTRING_KEYS_F4);
			break;
		case KEY_F5:
			textPtr = GetTextString(TEXTSTRING_KEYS_F5);
			break;
		case KEY_F6:
			textPtr = GetTextString(TEXTSTRING_KEYS_F6);
			break;
		case KEY_F7:
			textPtr = GetTextString(TEXTSTRING_KEYS_F7);
			break;
		case KEY_F8:
			textPtr = GetTextString(TEXTSTRING_KEYS_F8);
			break;
		case KEY_F9:
			textPtr = GetTextString(TEXTSTRING_KEYS_F9);
			break;
		case KEY_F10:
			textPtr = GetTextString(TEXTSTRING_KEYS_F10);
			break;
		case KEY_F11:
			textPtr = GetTextString(TEXTSTRING_KEYS_F11);
			break;
		case KEY_F12:
			textPtr = GetTextString(TEXTSTRING_KEYS_F12);
			break;
		case KEY_A_UMLAUT:
			KeyDescBuffer[0] = 196; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_O_UMLAUT:
			KeyDescBuffer[0] = 214; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_U_UMLAUT:
			KeyDescBuffer[0] = 220; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_BETA:
			KeyDescBuffer[0] = 223; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_PLUS:
			KeyDescBuffer[0] = '+'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_HASH:
			KeyDescBuffer[0] = '#'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_UPSIDEDOWNEXCLAMATION:
			KeyDescBuffer[0] = 161; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_C_CEDILLA:
			KeyDescBuffer[0] = 199; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_N_TILDE:
			KeyDescBuffer[0] = 209; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_RIGHTBRACKET:
			KeyDescBuffer[0] = ')'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_ASTERISK:
			KeyDescBuffer[0] = '*'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_DOLLAR:
			KeyDescBuffer[0] = '$'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_U_GRAVE:
			KeyDescBuffer[0] = 217; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_EXCLAMATION:
			KeyDescBuffer[0] = '!'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_COLON:
			KeyDescBuffer[0] = ':';
			textPtr = KeyDescBuffer;
			break;
		case KEY_DIACRITIC_GRAVE:
			KeyDescBuffer[0] = 96; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_DIACRITIC_ACUTE:
			KeyDescBuffer[0] = 180; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_DIACRITIC_CARET:
			KeyDescBuffer[0] = '^'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_DIACRITIC_UMLAUT:
			KeyDescBuffer[0] = 168; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_ORDINAL:
			KeyDescBuffer[0] = 186; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;
		case KEY_LESSTHAN:
			KeyDescBuffer[0] = '<'; // Windows Latin 1 (ANSI)
			textPtr = KeyDescBuffer;
			break;

		case KEY_VOID:
			KeyDescBuffer[0] = 0;
			textPtr = KeyDescBuffer;
			break;

		default: // alpha-numeric case
		{
			if (key >= KEY_A && key <= KEY_Z)
			{
				KeyDescBuffer[0] = key - KEY_A + 'A';
			}
			else if (key >= KEY_0 && key <= KEY_9)
			{
				KeyDescBuffer[0] = key - KEY_0 + '0';
			}
			else // um, no idea - send blank
			{
				KeyDescBuffer[0] = 0;
			}
			textPtr = KeyDescBuffer;
			break;
		}

	}
	return textPtr;
}

static int OkayToPlayNextEpisode(void)
{
	switch(AvP.PlayerType)
	{
		default:
			GLOBALASSERT(0);
		case I_Marine:
		{
			if (UserProfilePtr->LevelCompleted[AvP.PlayerType][MarineEpisodeToPlay] < AvP.Difficulty+1)
			{
				UserProfilePtr->LevelCompleted[AvP.PlayerType][MarineEpisodeToPlay]=AvP.Difficulty+1;
			}
			SaveUserProfile(UserProfilePtr);
			SetupNewMenu(AVPMENU_MARINELEVELS);
			return 1;
			break;
		}
		case I_Predator:
		{
			if (UserProfilePtr->LevelCompleted[AvP.PlayerType][PredatorEpisodeToPlay] < AvP.Difficulty+1)
			{
				UserProfilePtr->LevelCompleted[AvP.PlayerType][PredatorEpisodeToPlay]=AvP.Difficulty+1;
			}
			SaveUserProfile(UserProfilePtr);
			SetupNewMenu(AVPMENU_PREDATORLEVELS);
			return 1;
			break;
		}
		case I_Alien:
		{
			if (UserProfilePtr->LevelCompleted[AvP.PlayerType][AlienEpisodeToPlay] < AvP.Difficulty+1)
			{
				UserProfilePtr->LevelCompleted[AvP.PlayerType][AlienEpisodeToPlay]=AvP.Difficulty+1;
			}
			SaveUserProfile(UserProfilePtr);
			SetupNewMenu(AVPMENU_ALIENLEVELS);
			return 1;
			break;
		}
	}
	return 0;
}

int NumberOfAvailableLevels(I_PLAYER_TYPE playerID)
{
	int i=0;
	int maximumLevel;
	int numberOfBasicLevels;
	int maxDifficulty=0x7fffffff;

	switch(playerID)
	{
		case I_Marine:
		{
			maximumLevel = MAX_NO_OF_MARINE_EPISODES-1;
			numberOfBasicLevels = MAX_NO_OF_BASIC_MARINE_EPISODES-1;
			break;
		}
		case I_Predator:
		{
			maximumLevel = MAX_NO_OF_PREDATOR_EPISODES-1;
			numberOfBasicLevels = MAX_NO_OF_BASIC_PREDATOR_EPISODES-1;
			break;
		}
		case I_Alien:
		{
			maximumLevel = MAX_NO_OF_ALIEN_EPISODES-1;
			numberOfBasicLevels = MAX_NO_OF_BASIC_ALIEN_EPISODES-1;
			break;
		}
	}

	while (i<numberOfBasicLevels && UserProfilePtr->LevelCompleted[playerID][i])
	{
		i++;
	}

	MaximumSelectableLevel = i;
	{
		int l;
		for (l=0; l<=numberOfBasicLevels; l++)
		{
			if (UserProfilePtr->LevelCompleted[playerID][l]<maxDifficulty)
			{
				maxDifficulty = UserProfilePtr->LevelCompleted[playerID][l];
			}
		}
		switch(maxDifficulty)
		{
			case AVP_DIFFICULTY_LEVEL_EASY:
			{
				MaximumSelectableLevel = maximumLevel-3;
				i = maximumLevel;
				break;
			}
			case AVP_DIFFICULTY_LEVEL_MEDIUM:
			{
				MaximumSelectableLevel = maximumLevel-1;
				i = maximumLevel;
				break;
			}
			case AVP_DIFFICULTY_LEVEL_HARD:
			{
				MaximumSelectableLevel = maximumLevel;
				i = maximumLevel;
				break;
			}
			default:
				break;
		}
	}

	return i;
}

int LevelMostLikelyToPlay(I_PLAYER_TYPE playerID)
{
	int i=0;
	int maximumLevel;

	switch (playerID)
	{
		case I_Marine:
		{
			maximumLevel = MAX_NO_OF_MARINE_EPISODES-1;
			break;
		}
		case I_Predator:
		{
			maximumLevel = MAX_NO_OF_PREDATOR_EPISODES-1;
			break;
		}
		case I_Alien:
		{
			maximumLevel = MAX_NO_OF_ALIEN_EPISODES-1;
			break;
		}
	}

	while (i<maximumLevel && (UserProfilePtr->LevelCompleted[playerID][i]!=0))
	{
		i++;
	}

	return i;
}

int MaxDifficultyLevelAllowed(I_PLAYER_TYPE playerID, int level)
{
	if (level == 0) 
	{
		return 3;
	}
	else
	{
		// This becomes the number of rows in the difficulty menu, and it comes from a file
		return ProfileChecks_DifficultiesAllowed(UserProfilePtr->LevelCompleted[playerID][level-1]);
	}
}

void DisplayVideoModeUnavailableScreen(void)
{
	do
	{
		CheckForWindowsMessages();
		DrawMainMenusBackdrop();
		ReadUserInput();
		{
			RECT area;
			//draw the attached string at the bottom of the screen

			area.left=MENU_LEFTXEDGE;
			area.right=MENU_RIGHTXEDGE;
			area.top=0;
			area.bottom=ScreenDescriptorBlock.SDB_Height;

			RenderSmallFontString_Wrapped(GetTextString(TEXTSTRING_NOTENOUGHMEMORY),&area,BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT,0,0);
		}

		ShowMenuFrameRate();

		FlipBuffers();
		FrameCounterHandler();
		PlayMenuMusic();
	}
	while (!DebouncedGotAnyKey);
}


void CheckForCredits(void)
{
#ifndef ALIEN_DEMO

	FILE *fp = avp_fopen("credits.txt", "rb");
	
	if (!fp)
	{
		char message[100];
		sprintf(message,"Unable to access credits.txt\n");

		ChangeWindowsSize(1, 1);
		avp_MessageBox(message, MB_OK+MB_SYSTEMMODAL);
		avp_exit(0x111);
		return;
	}
	else
	{
		fclose(fp);
	}

#endif
}

void DoCredits(void)
{
	// The roll used to be skipped whenever the menu's video was on, which is always when fmvs/menubackground.bik is there, because in a
	// Release build it couldn't be left early. That came from how the keyboard was read: DirectReadKeyboard asked DirectInput, and in a
	// Release build a failed read gave up quietly, so no key was seen. The keyboard is read from the window's messages now, which this loop
	// takes in itself each frame, and the video reads no keys, so the roll shows with the video too. CREDITS-1; found through Bobblen's
	// avp-forever (ec6446f), whose fix this follows, rewritten.
	int position = 300*2048;
	BOOL FinishedCredits = FALSE;
	
	// the buffer ends with an extra 0 byte (LoadTextFileWithSize), which RollCreditsText stops at
	size_t creditsSize = 0;
	char *creditsBufferPtr = LoadTextFileWithSize("credits.txt", &creditsSize);

	if (!creditsBufferPtr) {
		return;
	}

	char *creditsPtr;

	if (!strncmp (creditsBufferPtr, "REBCRIF1", 8))
	{
		// checked decompression, bounded by the file's size; the result has 16 spare bytes after the data
		int uncompressedSize = (creditsSize >= sizeof(HuffmanPackage)) ? ((HuffmanPackage*)creditsBufferPtr)->UncompressedDataSize : 0;
		creditsPtr = HuffmanDecompressChecked((HuffmanPackage*)(creditsBufferPtr), (creditsSize > 0x7fffffff) ? 0x7fffffff : (int)creditsSize);
		DeallocateMem(creditsBufferPtr);
		creditsBufferPtr = creditsPtr;
		if (!creditsPtr) {
			return;
		}
		creditsPtr[uncompressedSize] = 0;
		creditsSize = uncompressedSize;
	}
	else
	{
		creditsPtr = creditsBufferPtr;
	}

	// the text starts 4 bytes in (RollCreditsText(position, creditsPtr+4))
	if (creditsSize < 4) {
		UnloadTextFile("credits.txt", creditsPtr);
		return;
	}

	do
	{
		CheckForWindowsMessages();

		DirectReadKeyboard();

		ThisFramesRenderingHasBegun();

		DrawMainMenusBackdrop();

		FinishedCredits = !RollCreditsText(position, creditsPtr+4);
		ShowMenuFrameRate();

		FrameCounterHandler();
		PlayMenuMusic();

		ThisFramesRenderingHasFinished();

		FlipBuffers();

		position -= RealFrameTime;
	}
	// Any key pressed this frame stops it, each looked at on its own, so one held all along can't keep the rest from counting (credits_roll.h)
	while (!FinishedCredits && !CreditsRoll_AKeyWasJustPressed(DebouncedKeyboardInput, MAX_NUMBER_OF_INPUT_KEYS, KEY_F8));

	UnloadTextFile("credits.txt", creditsPtr);
}

BOOL RollCreditsText(int position, char *textPtr)
{
	int y=0;

	// the text ends at the first '#'; it also stops at the 0 byte DoCredits' buffer ends with, so a credits file
	// without the '#' no longer runs these scans past the buffer
	while((*textPtr!='#') && (*textPtr!='\0'))
	{
		char buffer1[96];
		char buffer2[96];
		int i;
		int centredText,splitText;
		{
			int yy = 60+y+position/2048;

			if (yy >= 60 && yy <= 400)
			{
				int b;
				{
					if (*textPtr=='}')
					{
						textPtr++;
						centredText = 1;
					}
					else
					{
						centredText = 0;
					}

					for (i=0; i<80 && (*textPtr!='\r')&&(*textPtr!='\n')&&(*textPtr!='\0')&&(*textPtr!='|'); i++)
					{
						buffer1[i] = *textPtr++;
					}
					buffer1[i]=0;

					if (*textPtr=='|')
					{
						textPtr++;
						for (i=0; i<80 && (*textPtr!='\r')&&(*textPtr!='\n')&&(*textPtr!='\0'); i++)
						{
							buffer2[i] = *textPtr++;
						}
						buffer2[i]=0;
						splitText = 1;
					}
					else
					{
						splitText = 0;
					}

					if (*textPtr!='\0') textPtr++;
					while(*textPtr=='\n') textPtr++;
				}
				
				if (yy<92)
				{
					b = (yy-60)*2048;
				}
				else if (yy>400-32)
				{
					b = (400-yy)*2048;
				}
				else 
				{
					b = ONE_FIXED;
				}

				if (splitText)
				{
					RenderSmallMenuText(buffer1,MENU_CENTREX-MENU_ELEMENT_SPACING,yy,b,AVPMENUFORMAT_RIGHTJUSTIFIED);
					RenderSmallMenuText(buffer2,MENU_CENTREX+MENU_ELEMENT_SPACING,yy,b,AVPMENUFORMAT_LEFTJUSTIFIED);
				}
				else
				{
					if (centredText)
					{
						RenderSmallMenuText(buffer1,MENU_CENTREX,yy,b,AVPMENUFORMAT_CENTREJUSTIFIED);
					}
					else
					{
						RenderSmallMenuText(buffer1,MENU_LEFTXEDGE,yy,b,AVPMENUFORMAT_LEFTJUSTIFIED);
					}
				}
			}
			else
			{
				while((*textPtr!='\0') && (*textPtr++!='\n'));
			}
		}
		y+=15;
	}

	if(y+(position/2048)<0)
	{
		//finished
		return FALSE;
	}
	return TRUE;
}
/* KJL 17:50:06 24/06/98 - setup video mode for Menus */

extern void DrawMainMenusBackdrop(void)
{
	if (!PlayMenuBackgroundFmv())
	{
		// Modified 2026-11-10 by Melanikus for the AvP port (the owner's choice, "true shape, starfield full width"): on a wide screen the
		// menus keep their true shape, and the starfield alone still fills the screen's width (menu_shape.h; d3d_render.cpp,
		// MenuBackdropFullWidth)
		extern bool MenuBackdropFullWidth;
		MenuBackdropFullWidth = true;
		DrawAvPMenuGfx(AVPMENUGFX_BACKDROP, 0, 0, ONE_FIXED+1, AVPMENUFORMAT_LEFTJUSTIFIED);
		MenuBackdropFullWidth = false;
	}
	/* PlayMenuBackgroundFmv takes care of rendering to screen */
}

static void UpdateMultiplayerConfigurationMenu()
{
	/*update the displayed levels according to the cuurent game type*/
	
	AVPMENU_ELEMENT *elementPtr = AvPMenus.MenuElements;
	
	//search for the level name element
	do
	{
		GLOBALASSERT(elementPtr->ElementID!=AVPMENU_ELEMENT_ENDOFMENU);
		elementPtr++;

	} while (elementPtr->TextDescription!=TEXTSTRING_MULTIPLAYER_ENVIRONMENT);

	
	if (netGameData.skirmishMode)
	{
		//force 'cooperative' game if using skirmish mode
		netGameData.gameType = NGT_Coop;
	}
	
	if (netGameData.gameType != NGT_Coop)
	{
		elementPtr->MaxSliderValue = NumMultiplayerLevels-1;
		elementPtr->TextSliderStringPointer = MultiplayerLevelNames;

		//make sure the level number is within bounds
		netGameData.levelNumber%=NumMultiplayerLevels;
	}
	else
	{
		elementPtr->MaxSliderValue = NumCoopLevels-1;
		elementPtr->TextSliderStringPointer = CoopLevelNames;

		//make sure the level number is within bounds
		netGameData.levelNumber%=NumCoopLevels;
	}

	// Someone setting up a game: what the level row has chosen. A joiner's page shows what the host told it.
	if (AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_CONFIG_JOIN)
	{
		Campaign_SetFromLevelRow();
	}

	// The difficulty row only means something for a campaign's level. Any other game is at Medium, so there
	// it says that and can't be changed. A joiner's page only ever shows it.
	{
		static int mediumToShow = I_Medium;
		bool const isCampaign = netGameData.coopCampaign != kCampaignNone;

		for (AVPMENU_ELEMENT *row = AvPMenus.MenuElements; row->ElementID != AVPMENU_ELEMENT_ENDOFMENU; row++)
		{
			if (row->TextDescription == TEXTSTRING_DIFFICULTY_LEVEL)
			{
				row->ElementID = (isCampaign && AvPMenus.CurrentMenu != AVPMENU_MULTIPLAYER_CONFIG_JOIN) ? AVPMENU_ELEMENT_TEXTSLIDER : AVPMENU_ELEMENT_DUMMYTEXTSLIDER;
				row->SliderValuePtr = isCampaign ? &netGameData.coopDifficulty : &mediumToShow;
			}
		}
	}

	// On the level row, say what choosing a campaign's level means
	elementPtr->HelpString = netGameData.coopCampaign != kCampaignNone ? TEXTSTRING_MPHELP_CAMPAIGN_LEVEL : TEXTSTRING_MPHELP_ENVIRONMENT;

	//see if selected element is the gamestyle element
	elementPtr = &AvPMenus.MenuElements[AvPMenus.CurrentlySelectedElement];

	if (elementPtr->TextDescription==TEXTSTRING_MULTIPLAYER_GAMESTYLE)
	{
		//change the helpstring according to the game style
		switch (netGameData.gameType)
		{
			case NGT_Individual:
				elementPtr->HelpString=TEXTSTRING_MPHELP_GAMESTYLE_DEATHMATCH;
				break;

			case NGT_CoopDeathmatch:
				elementPtr->HelpString=TEXTSTRING_MPHELP_GAMESTYLE_DEATHMATCHTEAM;
				break;

			case NGT_LastManStanding:
				elementPtr->HelpString=TEXTSTRING_MPHELP_GAMESTYLE_LASTMANSTANDING;
				break;

			case NGT_PredatorTag:
				elementPtr->HelpString=TEXTSTRING_MPHELP_GAMESTYLE_PREDATORTAG;
				break;

			case NGT_Coop:
				elementPtr->HelpString=TEXTSTRING_MPHELP_GAMESTYLE_COOPERATIVE;
				break;

			case NGT_AlienTag:
				elementPtr->HelpString=TEXTSTRING_MPHELP_GAMESTYLE_ALIENTAG;
				break;
		}
	}

	//also need to fill in customLevelName
	// (A custom level's name is its file name, up to 259 characters, but customLevelName holds only 40 bytes, so the
	// copy stops there. A name that fits is copied exactly as before.)
	{
		const char *levelName = GetCustomMultiplayerLevelName(netGameData.levelNumber,netGameData.gameType);
		size_t nameLength = strnlen(levelName, sizeof(netGameData.customLevelName) - 1);
		memcpy(netGameData.customLevelName, levelName, nameLength);
		netGameData.customLevelName[nameLength] = '\0';
	}
}

static void TestValidityOfCheatMenu(void)
{
	CheatMode_GetNextAllowedMode(AvPMenus.MenuElements[0].SliderValuePtr,TRUE);
	CheatMode_GetNextAllowedSpecies(AvPMenus.MenuElements[1].SliderValuePtr,TRUE);
	CheatMode_GetNextAllowedEnvironment(AvPMenus.MenuElements[2].SliderValuePtr,TRUE);
}

static char *BriefingTextString[5];
static char BlankLine[]="";

void SetBriefingTextForEpisode(int episode, I_PLAYER_TYPE playerID)
{
	int s;

	switch(playerID)
	{
		default:
		case I_Marine:
		{
			s = TEXTSTRING_MARINELEVELSBRIEFING_1;
			break;
		}
		case I_Predator:
		{
			s = TEXTSTRING_PREDATORLEVELSBRIEFING_1;
			break;
		}
		case I_Alien:
		{
			s = TEXTSTRING_ALIENLEVELSBRIEFING_1;
			break;
		}
	}

	for (int i = 0; i < 5; i++)
	{
		BriefingTextString[i] = GetTextString(static_cast<enum TEXTSTRING_ID>(s+i+ episode*5));
	}
}

static char MultiplayerBriefing[3][100];

static void AddMultiplayerBriefingString(const char* text)
{
	size_t shortest = 0;
	size_t shortest_length = 1000;

	for (int i=0;i<3;i++)
	{
		size_t length = strlen(MultiplayerBriefing[i]);
		if (length < shortest_length)
		{
			shortest = i;
			shortest_length = length;
		}
	}

	if (shortest_length + 3 + strlen(text)>=100) 
		return;

	if (shortest_length > 0)
	{
		strcat(MultiplayerBriefing[shortest]," , ");
	}
	strcat(MultiplayerBriefing[shortest], text);
}

static BOOL IsFlamerInLevel(int level)
{
	if (level == AVP_ENVIRONMENT_SUBWAY_MP || level == AVP_ENVIRONMENT_SUBWAY_COOP)
	{
		return FALSE;
	}
	return TRUE;
}

static BOOL IsMinigunInLevel(int level)
{
	if (level == AVP_ENVIRONMENT_SEWER)
	{
		return FALSE;
	}
	return TRUE;
}

static BOOL IsSadarInLevel(int level)
{
	if (level == AVP_ENVIRONMENT_MASSACRE ||
		level == AVP_ENVIRONMENT_MEATFACTORY_MP ||
		level == AVP_ENVIRONMENT_MEATFACTORY_COOP ||
		level == AVP_ENVIRONMENT_HADLEYSHOPE_MP ||
		level == AVP_ENVIRONMENT_HADLEYSHOPE_COOP ||
		level == AVP_ENVIRONMENT_HIVE ||
		level == AVP_ENVIRONMENT_HIVE_COOP ||
		level == AVP_ENVIRONMENT_KENS_COOP ||
		level == AVP_ENVIRONMENT_LEADWORKS_MP ||
		level == AVP_ENVIRONMENT_LEADWORKS_COOP)
	{
		return FALSE;
	}
	return TRUE;
}

static BOOL IsSkeeterInLevel(int level)
{
	if (level == AVP_ENVIRONMENT_LEADWORKS_MP || level == AVP_ENVIRONMENT_LEADWORKS_COOP)
	{
		return FALSE;
	}
	return TRUE;
}

static BOOL IsSmartgunInLevel(int level)
{
	if (level == AVP_ENVIRONMENT_NOSTROMO_MP)
	{
		return FALSE;
	}
	return TRUE;
}


static void SetBriefingTextForMultiplayer()
{
	// A campaign's level has the words single player shows for it, and none of a network game's settings count
	if (netGameData.coopCampaign != kCampaignNone && netGameData.coopCampaign != kCampaignCustom)
	{
		I_PLAYER_TYPE const species = netGameData.coopCampaign == kCampaignPredator ? I_Predator
			: netGameData.coopCampaign == kCampaignAlien ? I_Alien : I_Marine;

		SetBriefingTextForEpisode(netGameData.coopCampaignLevel, species);
		return;
	}

	// Every line starts blank, because below only some of them get set, and the loading screen reads all five. A line nothing set would crash it.
	SetBriefingTextToBlank();

	int level = NumberForCurrentLevel();

	int num_not_available = 0;

	if (netGameData.customLevelName[0] != 0)
	{
		//custom level
		BriefingTextString[0] = netGameData.customLevelName;
		return;
	}
	
	MultiplayerBriefing[0][0] = 0;
	MultiplayerBriefing[1][0] = 0;
	MultiplayerBriefing[2][0] = 0;

	if (!netGameData.allowSmartgun || !IsSmartgunInLevel(level))
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_SMARTGUN));
		num_not_available++;
	}
	if (!netGameData.allowFlamer || !IsFlamerInLevel(level))
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_FLAMETHROWER));
		num_not_available++;
	}
	if (!netGameData.allowSadar || !IsSadarInLevel(level))
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_SADAR));
		num_not_available++;
	}
	if (!netGameData.allowGrenadeLauncher)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_GRENADELAUNCHER));
		num_not_available++;
	}
	if (!netGameData.allowMinigun || !IsMinigunInLevel(level))
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_MINIGUN));
		num_not_available++;
	}
	if (!netGameData.allowSmartDisc || !IsSkeeterInLevel(level))
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_SKEETER));
		num_not_available++;
	}
	if (!netGameData.allowPistols)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_MARINE_PISTOL));
		num_not_available++;
	}
	if (!netGameData.allowDisc)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_DISC));
		num_not_available++;
	}
	if (!netGameData.allowPistol)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_BRIEFING_PREDATOR_PISTOL));
		num_not_available++;
	}
	if (!netGameData.allowPlasmaCaster)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_SHOULDERCANNON));
		num_not_available++;
	}
	if (!netGameData.allowSpeargun)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_RIFLE));
		num_not_available++;
	}
	if (!netGameData.allowMedicomp)
	{
		AddMultiplayerBriefingString(GetTextString(TEXTSTRING_INGAME_MEDICOMP));
		num_not_available++;
	}

	if (netGameData.gameType!=NGT_Coop)
	{
		BriefingTextString[0] = GetTextString(static_cast<enum TEXTSTRING_ID>(netGameData.levelNumber + TEXTSTRING_MULTIPLAYERLEVELS_1));
	}
	else
	{
		BriefingTextString[0] = GetTextString(static_cast<enum TEXTSTRING_ID>(netGameData.levelNumber + TEXTSTRING_COOPLEVEL_1));
	}
	
	if (num_not_available)
	{
		BriefingTextString[1] = GetTextString(TEXTSTRING_BRIEFING_UNAVAILABLE_WEAPONS);
		BriefingTextString[2] = MultiplayerBriefing[0];
		BriefingTextString[3] = MultiplayerBriefing[1];
		BriefingTextString[4] = MultiplayerBriefing[2];
	}
}

void SetBriefingTextToBlank(void)
{
	for (int i = 0; i < 5; i++)
	{
		BriefingTextString[i] = BlankLine;
	}
}

void RenderBriefingText(int centreY, int brightness)
{
	int lengthOfLongestLine = -1;
	int x, y, i;

	for (i = 0; i < 5; i++)
	{
		int length = 0;
		{
			char *ptr = BriefingTextString[i];

			while (*ptr)
			{
				length += AAFontWidths[(unsigned char)*ptr++];
			}
		}

		if (lengthOfLongestLine < length)
		{
			lengthOfLongestLine = length;
		}
	}

	/* bjd */

	if (AvPMenus.MenusState == MENUSSTATE_MAINMENUS)
	{
		x = (640 - lengthOfLongestLine)/2;
	}
	else
	{
		x = (ScreenDescriptorBlock.SDB_Width-lengthOfLongestLine)/2;
	}

	y = centreY - 3*HUD_FONT_HEIGHT;

	for (i=0; i<5; i++)
	{
		if (AvPMenus.MenusState != MENUSSTATE_MAINMENUS)
		{
			Hardware_RenderSmallMenuText(BriefingTextString[i], x, y, brightness, AVPMENUFORMAT_LEFTJUSTIFIED/*,MENU_CENTREY-60-100,MENU_CENTREY-60+180*/);
		}
		else
		{
			RenderSmallMenuText(BriefingTextString[i], x, y, brightness, AVPMENUFORMAT_LEFTJUSTIFIED);
		}
		if (i) y+=HUD_FONT_HEIGHT;
		else y+=HUD_FONT_HEIGHT*2;
	}
}


void CheckForKeysWithMultipleAssignments(void)
{
	unsigned char *configPtr[2];

	configPtr[0] = (unsigned char*)&PlayerInputPrimaryConfig;
	configPtr[1] = (unsigned char*)&PlayerInputSecondaryConfig;

	for (int column=0; column<=1; column++)
	{
		for (int row=0; row<32; row++)
		{
			int innerColumn,innerRow;

			MultipleAssignments[column][row] = 0;

			for (innerColumn=0; innerColumn<=1; innerColumn++)
			for (innerRow=0; innerRow<32; innerRow++)
			{
				if (innerRow==row) continue;
				if ( (configPtr[column])[row]==(configPtr[innerColumn])[innerRow] )
				{
					MultipleAssignments[column][row] = 1;
				}
			}

			// A key on the rendering page counts as well. The control wins, so this is where the player gets to see it.
			if (ViewSettings_KeyIsOnALine((configPtr[column])[row]))
			{
				MultipleAssignments[column][row] = 1;
			}
		}
	}
}

void HandleCheatModeFeatures(void)
{
	switch(CheatMode_Active)
	{
		case CHEATMODE_WARPSPEED:
		{
			TimeScale = 2*ONE_FIXED;
			AvP.Difficulty = I_Easy; // bjd - was 0
			break;
		}
		case CHEATMODE_LANDOFTHEGIANTS:
		{
			TimeScale = ONE_FIXED/2;
			break;
		}
		case CHEATMODE_IMPOSSIBLEMISSION:
		{
			AvP.Difficulty = I_Hard; // bjd - was 3
			break;
		}
		case CHEATMODE_UNDERWATER:
		{
			TimeScale = (ONE_FIXED*7)/10;
			break;
		}
		default:
			break;
	}
}

void ShowMenuFrameRate(void)
{
#if 0
	char buffer[8];
	
	sprintf(buffer,"%d fps",65536/NormalFrameTime);
	RenderSmallMenuText(buffer,20,20,ONE_FIXED,	AVPMENUFORMAT_LEFTJUSTIFIED);
#endif
}

#define MAX_ITEMS_IN_KEYBOARDENTRYQUEUE 8
static char KeyboardEntryQueue[MAX_ITEMS_IN_KEYBOARDENTRYQUEUE];
static int NumberOfItemsInKeyboardEntryQueue;
static int KeyboardEntryQueue_ProcessingIndex;

extern void KeyboardEntryQueue_Add(char c)
{
	if (c < 32) return;

	if (NumberOfItemsInKeyboardEntryQueue < MAX_ITEMS_IN_KEYBOARDENTRYQUEUE)
	{
		KeyboardEntryQueue[NumberOfItemsInKeyboardEntryQueue++] = c;
	}
}

static void KeyboardEntryQueue_Clear(void)
{
	for (int i=0; i < MAX_ITEMS_IN_KEYBOARDENTRYQUEUE; i++)
	{
		KeyboardEntryQueue[i] = 0;
	}
	NumberOfItemsInKeyboardEntryQueue = 0;
}

static void KeyboardEntryQueue_StartProcessing(void)
{
	KeyboardEntryQueue_ProcessingIndex = 0;
}

static char KeyboardEntryQueue_ProcessCharacter(void)
{
	if (KeyboardEntryQueue_ProcessingIndex==MAX_ITEMS_IN_KEYBOARDENTRYQUEUE)
	{
		return 0;
	}

	return KeyboardEntryQueue[KeyboardEntryQueue_ProcessingIndex++];
}

void ScanSaveSlots(void)
{
	char filename[MAX_PATH];
	SAVE_SLOT_HEADER *slotPtr = SaveGameSlot;

	for (int i=0; i < NUMBER_OF_SAVE_SLOTS; i++, slotPtr++)
	{
		GetFilenameForSaveSlot(i ,filename);

		GetHeaderInfoForSaveSlot(slotPtr, filename);
	}
}

extern BOOL GetFilenameForSaveSlot(int i, char *filenamePtr)
{
	// filenamePtr is a MAX_PATH (260) byte buffer at every caller (ScanSaveSlots, savegame.cpp).
	// A very long save folder path may leave no room for the whole name. It must not be cut short then: the slot
	// number is near the end of the name, so a cut could make every slot the same file, and saving to one slot would
	// silently overwrite another. Instead the name is left empty and FALSE comes back. No file has an empty name, so
	// the slot looks unused when the slots are scanned, and SaveGame refuses to save. The same goes for a profile name
	// that could lead outside the profile folder (see UserProfileNameIsSafe).
	if (!UserProfileNameIsSafe(UserProfilePtr->Name) ||
		_snprintf(filenamePtr, MAX_PATH - 1, "%s%s%s_%d.sav", GetSaveFolderPath(), USER_PROFILES_PATH, UserProfilePtr->Name, i+1) < 0)
	{
		LogErrorString(std::string("cannot make a save file path for profile ") + UserProfilePtr->Name);
		filenamePtr[0] = '\0';
		return FALSE;
	}
	filenamePtr[MAX_PATH - 1] = '\0';
	return TRUE;
}


/*------------------------------------**
** Loading and saving main level info **
**------------------------------------*/

void SaveLevelHeader()
{
	LEVEL_SAVE_BLOCK* block = (LEVEL_SAVE_BLOCK*) GetPointerForSaveBlock(sizeof(LEVEL_SAVE_BLOCK));

	//fill in the header
	block->header.type = SaveBlock_MainHeader;
	block->header.size = sizeof(*block);

	//fill in the main block
	strncpy(block->AvP_Save_String,"AVPSAVE0",8);

	block->Species = AvP.PlayerType;

	switch(block->Species)
	{
		case I_Marine :
			block->Episode = MarineEpisodeToPlay;
			break;

		case I_Alien :
			block->Episode = AlienEpisodeToPlay;
			break;

		case I_Predator :
			block->Episode = PredatorEpisodeToPlay;
			break;
	}

	block->ElapsedTime_Hours =(unsigned char) AvP.ElapsedHours;
	block->ElapsedTime_Minutes =(unsigned char) AvP.ElapsedMinutes;
	block->ElapsedTime_Seconds = AvP.ElapsedSeconds;

	block->Difficulty = AvP.Difficulty;
	block->NumberOfSavesLeft = (unsigned char) NumberOfSavesLeft;
}

void LoadLevelHeader(SAVE_BLOCK_HEADER* header)
{
	LEVEL_SAVE_BLOCK* block =(LEVEL_SAVE_BLOCK*) header;

	if(block->header.size!=sizeof(*block)) return;

	AvP.ElapsedHours = block->ElapsedTime_Hours;
	AvP.ElapsedMinutes = block->ElapsedTime_Minutes;
	AvP.ElapsedSeconds = block->ElapsedTime_Seconds;
}


static void GetHeaderInfoForSaveSlot(SAVE_SLOT_HEADER* save_slot, const char* filename)
{
	LEVEL_SAVE_BLOCK block;
	unsigned int file_size;
	unsigned int bytes_read;
	HANDLE file;

	save_slot->SlotUsed = 0;

	file = avp_CreateFile(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);

	if (file==INVALID_HANDLE_VALUE)
	{
		//failed to load (probably doesn't exist)
		return;
	}

	file_size = GetFileSize(file, 0);

	if (file_size < sizeof(LEVEL_SAVE_BLOCK))
	{
		//obviously not much of a save file then...
		CloseHandle(file);
		return;
	}

	//get the time stamp for the file
	{
		FILETIME time, localTime;
		GetFileTime(file, 0, 0, &time);
		FileTimeToLocalFileTime(&time, &localTime);
		FileTimeToSystemTime(&localTime, &save_slot->TimeStamp);
	}

	//load the level header
	ReadFile(file,&block,sizeof(block),(LPDWORD)&bytes_read,0);
	CloseHandle(file);

	//a few checks
	if (block.header.type != SaveBlock_MainHeader ||
		block.header.size != sizeof(block) ||
		strncmp(block.AvP_Save_String,"AVPSAVE0",8))
	{
		//no good.
		return;
	}

	// These three pick a level, a table entry and a line of text, so they have to be ones the game can write. A file that fails looks like
	// an empty slot.
	if (!Save_SlotNumbersAreValid(block.Species, block.Episode, block.Difficulty))
	{
		LogString("save: a save slot's numbers aren't ones the game could have written, so it's listed as empty");
		return;
	}

	//appears to be a reasonable file
	save_slot->SlotUsed = TRUE;

	//copy stuff from the block
	save_slot->Species = block.Species;
	save_slot->Episode = block.Episode;
	save_slot->ElapsedTime_Hours = block.ElapsedTime_Hours;
	save_slot->ElapsedTime_Minutes = block.ElapsedTime_Minutes;
	save_slot->ElapsedTime_Seconds = (block.ElapsedTime_Seconds >> 16);
	save_slot->Difficulty = block.Difficulty;
	save_slot->SavesLeft = block.NumberOfSavesLeft;
}


static void CheckForLoadGame()
{
	if (LoadGameRequest >=0 && LoadGameRequest<NUMBER_OF_SAVE_SLOTS)
	{
		SAVE_SLOT_HEADER* save_slot = &SaveGameSlot[LoadGameRequest];	
		if (save_slot->SlotUsed)
		{
			AvP.PlayerType = static_cast<I_PLAYER_TYPE>(save_slot->Species);
			AvP.Difficulty = static_cast<I_HARDANUFF>(save_slot->Difficulty);

			switch (AvP.PlayerType)
			{
				case I_Marine :
					MarineEpisodeToPlay = save_slot->Episode;
					SetLevelToLoadForMarine(MarineEpisodeToPlay);
					break;

				case I_Alien :
					AlienEpisodeToPlay = save_slot->Episode;
					SetLevelToLoadForAlien(AlienEpisodeToPlay);
					break;

				case I_Predator :
					PredatorEpisodeToPlay = save_slot->Episode;
					SetLevelToLoadForPredator(PredatorEpisodeToPlay);
					break;
			}
			SetBriefingTextForEpisode(save_slot->Episode, AvP.PlayerType);
			AvPMenus.MenusState = MENUSSTATE_STARTGAME;
		}
		else
		{
			//cancel request
			LoadGameRequest = SAVELOAD_REQUEST_NONE;
		}
	}
}

static void PasteFromClipboard(char* Text, int MaxTextLength)
{
#ifdef _WIN32
	HANDLE hGlobal;
	if (!Text)
	{
		return;
	}

	if (IsClipboardFormatAvailable(CF_TEXT))
	{
		OpenClipboard(0);
		hGlobal = GetClipboardData(CF_TEXT);
		if (hGlobal)
		{
			char* pGlobal = static_cast<char*>(GlobalLock(hGlobal));
			if (pGlobal)
			{
				strncpy(Text, pGlobal, MaxTextLength-1);
				Text[MaxTextLength-1] = 0;
				GlobalUnlock(hGlobal);
			}
		}
		CloseClipboard();
	}
#endif
}
