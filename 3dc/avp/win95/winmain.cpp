// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   LevelName has room for any custom level name and the "-n" name from the command line is limited to
//   it; the Debug build's console output is closed properly on every way the game exits; the "-ip" address
//   from the command line is limited to its buffer
// Modified 2026-11-04 by Melanikus for the AvP port:
//   the level's still polygons are built and sent to the graphics card once a level has loaded
//   -keptsnap and -keptsnapfar
//   -shadowshoot
// Modified 2026-11-05 by Melanikus for the AvP port:
//   -reliefoff and -shadowsoff also say where the LIGHTING and SHADOWS commands start from
//   how the world is drawn comes from the settings file, and those two switches change it for this run only
//   from the command line is limited to its buffer; a plain new that fails now stops the game with a message
// Modified 2026-11-06 by Melanikus for the AvP port:
//   a level called off while it was still loading does not start
//   a network game going on to the next level of a campaign keeps its connection
//   -shadowambient starts with the map's lights that show no corona casting shadows
//   a new level starts with nothing noted about what was on screen
//   -shadownearest chooses lights by distance, as the game used to, to compare with what replaced it
//   -shadowlights goes up to 20 and -shadowbrief up to 12, and -shadownofloatblend tries what a card that can't add to a float picture gets
//   -shadowambient starts with the map's lights that show no corona casting shadows
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the log starts with which build this is: its version and commit, the program file's size and fingerprint, and which Windows
//   it runs on (build_identity.h)
//   F2 marks the moment for a tester, with a line in the log and a picture of the screen (mark_key.h)
//   as a co-op game ends, why it ended is noted before it's wiped, for the page after it
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the log lines for -dev and for a developer switch asked for without it start "developer tools:", not "shadows:", because the
//   switches aren't all about the shadows (#122)
//   -n is ignored in a network game and while a saved game loads, and its name has to keep to the rule for a map's name (level_switch.h);
//   the level it names is marked, so that the game doesn't save on it
//   every switch is read as a whole word (one with a number after its '=' as a word that starts with its name), so a longer word with the
//   same letters in it isn't taken for a switch; -ip refuses a word too long to be an address, instead of cutting it short
//   -staticcomparedrop turns the comparing on itself, as it did when -staticcompare was found inside it
//   -m and the Debug build's -s are gone: neither changed anything (-m asked for what's always on, and nothing read -s's number)

#include "3dc.h"
#include "build_identity.h"
#include "mark_key.h"
#include "texture_relief.h"
#include "view_settings.h"
#include "view_switches.h"
#include "light_choice.h"
#include "static_level.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "gameplat.h"
#include "bh_types.h"
#include "user_io.h"
#include <mmsystem.h>
#include "renderer.h"
#include "compiled_shapes.h"
#include "chunk_load.hpp"
#include "npcsetup.h" /* JH 30/4/97 */
#include "pldnet.h"
#include "view.h"
#include "vision.h"
#include "menus.h"
#include "coop_menu.h"
#include "ourasert.h" 
#include "davehook.h"
#include "showcmds.h"
#include "console_bind.hpp"
#include "mempool.h"
#include "gammacontrol.h"
#include "intro.h"
#include "MusicPlayer.h"
#include "psndplat.h"
#include "user_profile.h"
#include "menus.h"
#include "configFile.h"
#include "VorbisPlayer.h"
#include "networking.h"
#include "view.h"
#include "renderer.h"
#include "mp_config.h"
#include "logString.h"
#include "command_line_word.h"
#include "level_switch.h"
#include "savegame.h"
#include "frame_timing.h"
#include "FastFile.h"
#include "console.h"

#if debug
#define MainTextPrint 1
extern int alloc_cnt, deall_cnt;
extern int ItemCount;
#else
#define MainTextPrint 0
#endif

extern int PrintDebuggingText(const char* t, ...);
extern int FrameRate;
extern BOOL ForceLoad_Alien;
extern BOOL ForceLoad_Marine;
extern BOOL ForceLoad_Predator;
extern BOOL ForceLoad_Hugger;
extern BOOL ForceLoad_Queen;
extern BOOL ForceLoad_Civvie;
extern BOOL ForceLoad_PredAlien;
extern BOOL ForceLoad_Xenoborg;
extern BOOL ForceLoad_Pretorian;
extern BOOL ForceLoad_SentryGun;

BOOL UseMouseCentreing = FALSE;
BOOL KeepMainRifFile = FALSE;

// The name of the level to load. MAX_PATH (260) bytes leaves room for "Custom/" and any custom level's file name.
char LevelName[MAX_PATH] = {"predbit6"};

// Whether the level being played is one -n named, in place of the level the menus chose. Set as each level starts (level_switch.h).
bool PlayingLevelFromSwitch = false;

int VideoModeNotAvailable = 0;
int QuickStartMultiplayer = 1;

extern HWND hWndMain;
extern int WindowMode;
extern int DebuggingCommandsActive;
extern void dx_log_close();
extern void TimeStampedMessage(char *stringPtr);
extern void ThisFramesRenderingHasBegun(void);
extern void ThisFramesRenderingHasFinished(void);
extern void ScanImagesForFMVs();
extern void RestartLevel();
extern void ResetEaxEnvironment(void);
extern void ReleaseAllFMVTextures();
extern void MinimalNetCollectMessages(void);
extern void InitCentreMouseThread();
extern void IngameKeyboardInput_ClearBuffer(void);
extern void Game_Has_Loaded();
extern void FinishCentreMouseThread();
extern void DoCompletedLevelStatisticsScreen(void);
extern void DeInitialisePlayer();
extern void BuildMultiplayerLevelNameArray();
extern char CommandLineIPAddressString[];
extern int AvP_MainMenus(void);
extern int AvP_InGameMenus(void);
extern int InGameMenusAreRunning(void);
extern void InitFmvCutscenes();
extern void ChangeWindowsSize(uint32_t width, uint32_t height); 
extern void DetermineGameVersion();

extern struct DEBUGGINGTEXTOPTIONS ShowDebuggingText;

extern bool bRunning;

bool unlimitedSaves = false;

extern bool IsDemoVersion();
extern texID_t AAFontImageNumber;

void exit_break_point_function()
{
	#if debug
	if (WindowMode == WindowModeSubWindow)
	{
		__debugbreak();
	}
	#endif
}

#ifdef _DEBUG
#define _ALLOC_CONSOLE
#endif

extern void LoadKeyConfiguration();
extern bool InitialiseWindowsSystem(HINSTANCE hInstance, int nCmdShow, int WinInitMode);

// so we can disable/enable stickey keys
STICKYKEYS startupStickyKeys = {sizeof(STICKYKEYS), 0};
STICKYKEYS skOff;

#ifdef _ALLOC_CONSOLE
// The Debug build opens a console window and sends the game's printed text
// (printf) to it. This is that text stream, kept here so it can be closed.
static FILE *debugConsole = 0;
#endif

// Closes the Debug build's console text stream, if it is open. WinMain calls
// this on each of its own ways out: its normal end and its three early exit()
// calls. Nothing in the game prints after that point, so no text is lost. Code
// elsewhere that stops the game with exit() leaves the stream to the C runtime,
// which closes every open stream when the program ends.
static void CloseDebugConsole(void)
{
#ifdef _ALLOC_CONSOLE
	if (debugConsole)
	{
		fclose(debugConsole);
		debugConsole = 0;
	}
#endif
}

// entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	StopGameIfNewFails();

	SetWindowTitle();
	ClearLog();
	LogString(GetWindowTitle());
	LogString(BuildIdentity_Lines());

	char *command_line = lpCmdLine;

#ifdef _ALLOC_CONSOLE
	AllocConsole();
	debugConsole = freopen("CONOUT$", "wb", stdout);
#endif

	skOff = startupStickyKeys;

	if ((skOff.dwFlags & SKF_STICKYKEYSON) == 0)
	{
		// Disable the hotkey and the confirmation
		skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
		skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);
	}

	DetermineGameVersion();

	InitFmvCutscenes();

	// load AliensVsPredator.cfg
	Config_Load();
	ViewSettings_Load();

	Music_Init(); //load list of music tracks assigned to levels , from a text file
	LoadVorbisTrackList(); // do the same for any user ogg vorbis music files

	SetFastRandom();

	// -l asks for more kinds of creature to be loaded with every level, one letter for each kind. command_line_word.h says how it's read.
	{
		struct LoadLetter
		{
			char letter;
			BOOL *load;
			const char *kind;
		};

		// Every letter in kCommandLineLoadLetters is here, and nothing else (level_switch_test checks)
		LoadLetter const loadLetters[] = {
			{ 'a', &ForceLoad_Alien, "aliens" },
			{ 'm', &ForceLoad_Marine, "marines" },
			{ 'p', &ForceLoad_Predator, "predators" },
			{ 'h', &ForceLoad_Hugger, "facehuggers" },
			{ 'q', &ForceLoad_Queen, "the queen" },
			{ 'c', &ForceLoad_Civvie, "civilians" },
			{ 'x', &ForceLoad_Xenoborg, "xenoborgs" },
			{ 't', &ForceLoad_Pretorian, "praetorians" },
			{ 'r', &ForceLoad_PredAlien, "predaliens" },
			{ 's', &ForceLoad_SentryGun, "sentry guns" },
		};

		std::string loaded;

		for (LoadLetter const &each : loadLetters)
		{
			if (CommandLineLoadAsks(command_line, each.letter))
			{
				*each.load = TRUE;
				loaded += (loaded.empty() ? "" : ", ") + std::string(each.kind);
			}
		}

		if (!loaded.empty())
		{
			LogString("command line: -l loads these with every level: " + loaded);
		}

		if (int const notRead = CommandLineLoadWordsNotRead(command_line))
		{
			LogString("command line: " + std::to_string(notRead) + " word(s) starting -l weren't read as -l, which takes only the letters "
				+ kCommandLineLoadLetters + " after it");
		}
	}

	// not -norelief, for the same reason as -shadowsoff below
	if (CommandLineHasWord(command_line, "-reliefoff"))
	{
		textureRelief.TurnOff();

		if (Lighting_UsesMaps(LightingNow))
		{
			LightingNow = kLightingPerPixel;
		}
	}

	{
		// -dev turns on the developer tools: the ones for finding out what the shadows are doing (the tag; the keys J, or the key chosen on
		// the rendering page, F7 and F8; the shoot tool's F9, F10 and F5; the flicker number and the full report), and every switch below
		// that's read through devSwitch or devNumber, the shadows' and the others alike. A tester has no need of them, and shouldn't be able to reach them by accident, so without it they're off.
		// Their log lines start "developer tools:", because they aren't all about the shadows.
		extern int DevMode;
		DevMode = CommandLineHasWord(command_line, "-dev") ? 1 : 0;
		if (DevMode)
		{
			LogString("developer tools: on (-dev)");
		}

		// says whether a developer tool's switch is on, and if it was asked for without -dev, says why it isn't. A switch is a whole word, or
		// for one with a number after its '=', a word that starts with its name (CommandLineFindSwitch).
		auto const devSwitch = [&](const char *name)
		{
			if (!CommandLineFindSwitch(command_line, name))
			{
				return false;
			}
			if (!DevMode)
			{
				LogString(std::string("developer tools: ") + name + " needs -dev, so it's off");
			}
			return DevMode != 0;
		};

		// the same for one that's followed by a number: it gives back where the switch is, or nothing if it wasn't asked for or there's no -dev
		auto const devNumber = [&](const char *name) -> const char *
		{
			return devSwitch(name) ? CommandLineFindSwitch(command_line, name) : 0;
		};

		extern int ToyShadowsMode;
		// -shadowsoff, not -noshadows, because the level name switch -n used to take any word starting with -n. It reads only the
		// whole word now (LEVEL-SWITCH-1), but the names stay as they are, so that launch options already written keep working.
		if (CommandLineHasWord(command_line, "-shadowsoff"))
		{
			ToyShadowsMode = 0;
			ShadowVolumesOn = 0;
			LightingNow = kLightingGouraud;
		}
		// Like SHADOWAMBIENT, this only changes what this player's own screen draws, so it needs no -dev. See light_choice.h.
		if (CommandLineHasWord(command_line, "-shadowambient"))
		{
			ShadowAmbientOn = 1;
		}
		extern int ShadowLightsByNearest;
		if (devSwitch("-shadownearest"))
		{
			ShadowLightsByNearest = 1;
		}
		extern int ShadowFlickerOn;
		if (devSwitch("-shadowflicker"))
		{
			ShadowFlickerOn = 1;
		}
		extern int ShadowShootOn;
		if (devSwitch("-shadowshoot"))
		{
			ShadowShootOn = 1;
			LogString("shadows: F9 sends a ray through the crosshair and logs what it goes through, F10 stops that polygon casting, and F5 undoes that (-shadowshoot)");
		}
		extern int ShadowStatusOn;
		if (CommandLineHasWord(command_line, "-statusoff"))
		{
			ShadowStatusOn = 0;
		}
		extern int ShadowsFullReport;
		if (devSwitch("-shadowreport"))
		{
			ShadowsFullReport = 1;
		}
		extern int ShadowsEvenGap;
		if (devSwitch("-shadowevengap"))
		{
			ShadowsEvenGap = 1;
		}
		extern int StaticSurfaceOn;
		if (devSwitch("-staticsurface"))
		{
			StaticSurfaceOn = 1;
		}
		extern int StaticSceneOn;
		if (devSwitch("-staticscene"))
		{
			StaticSceneOn = 1;
		}
		extern int StaticSceneNoLiveLights;
		if (devSwitch("-staticnolivelights"))
		{
			StaticSceneNoLiveLights = 1;
		}
		extern int StaticSurfaceCompare;
		if (devSwitch("-staticcompare"))
		{
			StaticSurfaceCompare = 1;
		}
		extern int StaticCompareDropsHalf;
		if (devSwitch("-staticcomparedrop"))
		{
			StaticCompareDropsHalf = 1;

			// It only acts while the routes are compared. When switches were found anywhere in a word, -staticcompare was found inside
			// this one and turned the comparing on, so this still does that.
			StaticSurfaceCompare = 1;
		}
		extern int KeptPrismsOff;
		// not -nokeptprisms, for the same reason as -shadowsoff
		// they're off unless this says otherwise, and -keptprismsoff is still accepted, so that an old launch option does no harm
		if (devSwitch("-keptprismson"))
		{
			KeptPrismsOff = 0;
		}
		extern int ShadowsDepthGap;
		if (devSwitch("-shadowdepthgap"))
		{
			ShadowsDepthGap = 1;
			LogString("shadows: a shadow's start stands further behind its polygon as depth gets coarser (-shadowdepthgap), on prisms made each frame only: not kept prisms, and not the walls -staticscene walks");
		}
		char const *biasSwitch = devNumber("-shadowbiassteps=");
		if (biasSwitch)
		{
			extern float ShadowSurfaceBias;
			extern int ShadowBiasSteps;
			// anything but a number from 1 to 64 leaves the margin as it was, rather than quietly testing a margin nobody asked for
			char const *number = biasSwitch + strlen("-shadowbiassteps=");
			char *afterNumber = 0;
			long const asked = strtol(number, &afterNumber, 10);

			// it has to start with a digit and stop where the switch stops, so that 5abc and 5.9 aren't taken for 5
			bool const wholeNumber = *number >= '0' && *number <= '9' && (*afterNumber == 0 || *afterNumber == ' ' || *afterNumber == '\t');

			if (wholeNumber && asked >= 1 && asked <= 64)
			{
				ShadowBiasSteps = (int)asked;
				ShadowSurfaceBias = -(float)ShadowBiasSteps / 16777216.0f;
				LogString("shadows: the surface pictures' margin is " + std::to_string(asked) + " depth steps (-shadowbiassteps)");
			}
			else
			{
				LogString("shadows: -shadowbiassteps needs a number from 1 to 64, so it was ignored");
			}
		}
		extern int KeptSnap;
		// if both are given, -keptsnapfar wins, as it always has
		if (devSwitch("-keptsnapfar"))
		{
			KeptSnap = 2;
		}
		else if (devSwitch("-keptsnap"))
		{
			KeptSnap = 1;
		}

		// Walls drawn from the fixed buffer have no corners on the list, so their prisms always go through the card's own sums. Exact corners against
		// the scene's cut ones are what made kept prisms flicker, so these always use the cut ones.
		if (StaticSceneOn && !KeptSnap)
		{
			KeptSnap = 1;
		}
		extern int ToyShadowsDarkness;
		char const *darkSwitch = devNumber("-shadowdark=");
		if (darkSwitch)
		{
			int const asked = atoi(darkSwitch + strlen("-shadowdark="));
			ToyShadowsDarkness = (asked < 0) ? 0 : (asked > 99) ? 99 : asked;
		}
		extern int ToyShadowsSoft;
		char const *softSwitch = devNumber("-shadowsoft=");
		if (softSwitch)
		{
			int const asked = atoi(softSwitch + strlen("-shadowsoft="));
			ToyShadowsSoft = (asked < 0) ? 0 : (asked > 32) ? 32 : asked;
		}
		extern int ToyShadowsPositions;
		char const *positionsSwitch = devNumber("-shadowpositions=");
		if (positionsSwitch)
		{
			int const asked = atoi(positionsSwitch + strlen("-shadowpositions="));
			ToyShadowsPositions = (asked < 1) ? 1 : (asked > 8) ? 8 : asked;
		}
		extern int ToyShadowsNearLights;
		char const *nearSwitch = devNumber("-shadownearlights=");
		if (nearSwitch)
		{
			int const asked = atoi(nearSwitch + strlen("-shadownearlights="));
			ToyShadowsNearLights = (asked < 0) ? 0 : (asked > 12) ? 12 : asked;
		}
		extern int ToyShadowsLampSize;
		char const *lampSwitch = devNumber("-shadowlampsize=");
		if (lampSwitch)
		{
			int const asked = atoi(lampSwitch + strlen("-shadowlampsize="));
			ToyShadowsLampSize = (asked < 0) ? 0 : (asked > 2000) ? 2000 : asked;
		}
		extern int ToyShadowsFlareSize;
		char const *flareSizeSwitch = devNumber("-shadowflaresize=");
		if (flareSizeSwitch)
		{
			int const asked = atoi(flareSizeSwitch + strlen("-shadowflaresize="));
			ToyShadowsFlareSize = (asked < 0) ? 0 : (asked > 2000) ? 2000 : asked;
		}
		extern int ToyShadowsModelGap;
		char const *modelGapSwitch = devNumber("-shadowmodelgap=");
		if (modelGapSwitch)
		{
			int const asked = atoi(modelGapSwitch + strlen("-shadowmodelgap="));
			ToyShadowsModelGap = (asked < 8) ? 8 : (asked > 200) ? 200 : asked;
		}
		extern int ToyShadowsFlareReach;
		char const *reachSwitch = devNumber("-shadowflarereach=");
		if (reachSwitch)
		{
			int const asked = atoi(reachSwitch + strlen("-shadowflarereach="));
			ToyShadowsFlareReach = (asked < 50) ? 50 : (asked > 400) ? 400 : asked;
		}
		extern int ToyShadowsFlareCap;
		char const *flareSwitch = devNumber("-shadowflare=");
		if (flareSwitch)
		{
			int const asked = atoi(flareSwitch + strlen("-shadowflare="));
			ToyShadowsFlareCap = (asked < 10) ? 10 : (asked > 800) ? 800 : asked;
		}
		extern int ToyShadowsWorld;
		char const *worldSwitch = devNumber("-shadowworld=");
		if (worldSwitch)
		{
			int const asked = atoi(worldSwitch + strlen("-shadowworld="));
			ToyShadowsWorld = (asked < 10) ? 10 : (asked > 100) ? 100 : asked;
		}
		extern int ToyShadowsTest;
		if (devSwitch("-shadowtest"))
		{
			ToyShadowsTest = 1;
		}
		extern int ShadowsPretendNoFloatBlend;
		if (devSwitch("-shadownofloatblend"))
		{
			ShadowsPretendNoFloatBlend = 1;
		}
		extern int ToyShadowsMaxLights;
		char const *lightsSwitch = devNumber("-shadowlights=");
		if (lightsSwitch)
		{
			int const asked = atoi(lightsSwitch + strlen("-shadowlights="));
			ToyShadowsMaxLights = (asked < 1) ? 1 : (asked > 20) ? 20 : asked;
		}
		extern int ToyShadowsMaxBrief;
		char const *briefSwitch = devNumber("-shadowbrief=");
		if (briefSwitch)
		{
			int const asked = atoi(briefSwitch + strlen("-shadowbrief="));
			ToyShadowsMaxBrief = (asked < 0) ? 0 : (asked > 12) ? 12 : asked;
		}
		extern int ToyShadowsCasters;
		char const *castersSwitch = devNumber("-shadowcasters=");
		if (castersSwitch)
		{
			int const asked = atoi(castersSwitch + strlen("-shadowcasters="));
			ToyShadowsCasters = (asked < 0) ? 0 : (asked > 7) ? 7 : asked;
		}
		LogString(ToyShadowsMode ? "toy-shadows: on" : "toy-shadows: off");
		extern int KeptSnap;
		LogString((KeptSnap == 2) ? "shadows: a kept prism's near ends are cut each frame like the walls, and its far ends stay exact (-keptsnapfar)"
			: (KeptSnap == 1) ? "shadows: a kept prism's corners are cut each frame like the walls (-keptsnap)" : "shadows: a kept prism's corners stay exact");
		extern int ShadowsEvenGap;
		LogString(ShadowsEvenGap ? "shadows: each corner works out where its shadow starts (-shadowevengap)" : "shadows: one gap for a whole polygon");
	}

	if (CommandLineHasWord(command_line, "-intro"))	{
		WeWantAnIntro();
	}

	if (CommandLineHasWord(command_line, "-qm"))
	{
		QuickStartMultiplayer = 1;
	}
	else if (CommandLineHasWord(command_line, "-qa"))
	{
		QuickStartMultiplayer = 2;
	}
	else if (CommandLineHasWord(command_line, "-qp"))
	{
		QuickStartMultiplayer = 3;
	}
	else
	{
		QuickStartMultiplayer = 0;
	}

	if (CommandLineHasWord(command_line, "-keeprif"))
	{
		KeepMainRifFile = TRUE;
	}

	// The mouse is kept inside the game's window unless -dontgrabmouse says not to. (-m used to ask for this. It isn't read any more,
	// because this line has made it so whatever -m said, and an -m left in old launch options does nothing, as before.)
	UseMouseCentreing = TRUE;

	// windowed mode?
	if (CommandLineHasWord(command_line, "-w"))
	{
		WindowMode = WindowModeSubWindow;
	}

	if (CommandLineHasWord(command_line, "-dontgrabmouse"))
	{
		// the mouse is let go, whatever the lines above say
		UseMouseCentreing = FALSE;
	}

	if (UseMouseCentreing)
	{
		InitCentreMouseThread();
	}

//	#endif //AVP_DEBUG_VERSION

	// -server and -client start a network game straight away, so they have to be the words themselves: a level's name given to -n may
	// hold a '-', and a name like arena-server mustn't open a game to the network (SWITCH-WORDS-1)
	if (CommandLineHasWord(command_line, "-server"))
	{
		//game has been launched by mplayer , we best humour it
		LobbiedGame = LobbiedGame_Server;
		if (!Net_InitLobbiedGame())
		{
			CloseDebugConsole();
			exit(0x6364);
		}
	}
	else if (CommandLineHasWord(command_line, "-client"))
	{
		//ditto
		LobbiedGame = LobbiedGame_Client;
		if (!Net_InitLobbiedGame())
		{
			CloseDebugConsole();
			exit(0x6364);
		}
	}
	else if (CommandLineHasWord(command_line, "-debug"))
	{
		DebuggingCommandsActive = 1;
	}

	// -ip <address> fills in the address on the page for joining a game. It's outside input, so a word too long to be an address is
	// refused whole, not cut short: an address cut short could be another address.
	{
		// CommandLineIPAddressString holds 15 letters and its end marker (MAX_IPADDRESS_LENGTH, in menu_data.cpp)
		char address[15 + 1];
		const CommandLineWordAfterAnswer asked = CommandLineWordAfter(command_line, "-ip", address, sizeof(address));

		if (asked == kWordAfterFound)
		{
			memcpy(CommandLineIPAddressString, address, strlen(address) + 1);
		}
		else if (asked == kWordAfterNothing)
		{
			LogString("command line: -ip is ignored: no address follows it");
		}
		else if (asked == kWordAfterTooLong)
		{
			LogString("command line: -ip is ignored: what follows it is longer than an address can be");
		}
	}

	#if PLAY_INTRO//(MARINE_DEMO||ALIEN_DEMO||PREDATOR_DEMO)
	if (!LobbiedGame) { // Edmond
		WeWantAnIntro();
	}
	#endif

	// Initialise 'fast' file system
	FF_Init();

	InitGame();

	Env_List[0]->main = LevelName;

	// as per linux port
	AvP.CurrentEnv = AvP.StartingEnv = I_Gen1;

	/******* System initialisation **********/

	timeBeginPeriod(1);

	// Initialise main window, windows procedure etc
	InitialiseWindowsSystem(hInstance, nCmdShow, WinInitFull);
	InitialiseSystem();
	InitialiseRenderer();

	if (!InitialiseDirect3D())
	{
		// the device might have actually created OK, but maybe we are missing an important shader, so release
		// this first so we can exit fullscreen mode if needed
		ReleaseDirect3D();

		ChangeWindowsSize(1, 1);

		// give the user back their mouse cursor
		if (UseMouseCentreing)
		{
			FinishCentreMouseThread();
		}

		std::string error = "Failed to initialise Direct3D backend.\n" + GetLastErrorMessage();
		MessageBox(hWndMain, error.c_str(), "Couldn't create render device!", MB_OK | MB_ICONSTOP);
		CloseDebugConsole();
		exit(-1);
	}

	// load common font
	if (IsDemoVersion()) {
		AAFontImageNumber = Tex_CreateFromRIM("graphics/Menus/smallfont.RIM");
	}
	else {
		AAFontImageNumber = Tex_CreateFromRIM("graphics/Common/aa_font.RIM");
	}

	extern void CalculateWidthsOfAAFont();
	CalculateWidthsOfAAFont();

	// call a function to remove the red grid from the small font texture- only if the texture is loaded
	if (MISSING_TEXTURE != AAFontImageNumber)
	{
		DeRedTexture((Texture)Tex_GetTextureDetails(AAFontImageNumber));
	}

	// load slider graphics
	AVPMENUGFX_SLIDERBAR       = Tex_CreateFromRIM("graphics/Menus/SliderBar.RIM");
	AVPMENUGFX_SLIDER          = Tex_CreateFromRIM("graphics/Menus/Slider.RIM");

	Con_Init();

	Net_Initialise();

	LoadKeyConfiguration();

	/*-------------------Patrick 2/6/97-----------------------
	Start the sound system
	----------------------------------------------------------*/
	SoundSys_Start();
	Music_Start();

	// get rid of the mouse cursor
	SetCursor(NULL);

	// load language file and setup text string access
	InitTextStrings();

	BuildMultiplayerLevelNameArray(); //sort out multiplayer level names

	AvP.LevelCompleted = 0;
	LoadSounds("PLAYER");

	#if PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO
	if(AvP_MainMenus())
	#else

	// support removing limit on number of game saves
	unlimitedSaves = Config_GetBool("[Misc]", "UnlimitedSaves", false);

	while (AvP_MainMenus() && bRunning)
	#endif
	{
		// start of level load
		BOOL menusActive = FALSE;
		int thisLevelHasBeenCompleted = 0;

		mainMenu = false;

		#if !(PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO)
		// -n names a level to load instead of the one the menus chose. level_switch.h says when it's used, and when it's ignored.
		{
			static_assert(kLevelSwitchMostLetters < sizeof(LevelName), "LevelName has room for any name -n can give");

			char named[kLevelSwitchMostLetters + 1];
			bool const networkGame = AvP.Network != I_No_Network;
			bool const loadingASave = LoadGameRequest != SAVELOAD_REQUEST_NONE;
			const LevelSwitchAnswer answer = LevelSwitch_Read(command_line, networkGame, loadingASave, named, sizeof(named));

			PlayingLevelFromSwitch = (answer == kLevelSwitchUseIt);

			if (answer == kLevelSwitchUseIt)
			{
				LogString(std::string("level: loading ") + named + ", which -n names, instead of " + LevelName + ", which the menus chose");
				strcpy_s(LevelName, sizeof(LevelName), named);
			}
			else if (answer != kLevelSwitchNotAsked)
			{
				LogString(std::string("level: -n is ignored: ") + LevelSwitch_Why(answer) + ". Loading " + LevelName + ".");
			}
		}
		#endif

		// turn off any special effects
		d3d_light_ctrl.ctrl = LCCM_NORMAL;

		/* Check Gamma Settings are correct after video mode change */
		InitialiseGammaSettings(RequestedGammaSetting);

		// Load precompiled shapes 
		start_of_loaded_shapes = load_precompiled_shapes();

		/***********  Load up the character stuff *******/
		InitCharacter();

		LoadRifFile(); /* sets up a map*/

		/*********** Process the data ************/
		AssignAllSBNames();
		StartGame();

		// What the level before had on screen is no use for this one's lights
		extern LightChoiceSamples lightChoiceSamples;
		LightChoice_Forget(&lightChoiceSamples);

		// after StartGame, because that's when every module that's controlled by something has been told so
		StaticLevel_BuildThisLevel();

		/* JH 28/5/97 */
		/* remove resident loaded 'fast' files */
//		ffcloseall();
		/*********** Play the game ***************/

		/* KJL 15:43:25 03/11/97 - run until this boolean is set to 0 */
		// (Not if the next level was called for while this one was still loading. Without this, he'd stay here alone.)
		AvP.MainLoopRunning = Coop_LevelChangeIsDue() ? 0 : 1;

		ScanImagesForFMVs();

		ResetFrameCounter();
		Game_Has_Loaded();
		ResetFrameCounter();

		if (AvP.Network != I_No_Network)
		{
			/*Need to choose a starting position for the player , but first we must look
			through the network messages to find out which generator spots are currently clear*/
			netGameData.myGameState = NGS_Playing;
			MinimalNetCollectMessages();
			TeleportNetPlayerToAStartingPosition(Player->ObStrategyBlock,1);
		}

		IngameKeyboardInput_ClearBuffer();

		uint32_t startTime = timeGetTime();

		while (AvP.MainLoopRunning && bRunning) 
		{
			{
				ProcessorStageClock clock(kProcessorMessages);

				CheckForWindowsMessages();

				CursorHome();
			}

			#if debug
			if (memoryInitialisationFailure)
			{
				OutputDebugString("Initialisation not completed - out of memory!\n");
				textprint("Initialisation not completed - out of memory!\n");
				GLOBALASSERT(1 == 0);
			}
			#endif

			switch (AvP.GameMode)
			{
				case I_GM_Playing:
				{
					if ((!menusActive || (AvP.Network != I_No_Network && !netGameData.skirmishMode)) && !AvP.LevelCompleted)
					{
						//#if MainTextPrint 		/* debugging stuff */
						{
							if (ShowDebuggingText.FPS) ReleasePrintDebuggingText("FrameRate = %d fps\n",FrameRate);
							if (ShowDebuggingText.Environment) ReleasePrintDebuggingText("Environment %s\n", Env_List[AvP.CurrentEnv]->main);
							if (ShowDebuggingText.Coords) ReleasePrintDebuggingText("Player World Coords: %d,%d,%d\n",Player->ObWorld.vx,Player->ObWorld.vy,Player->ObWorld.vz);
							{
								PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);
								PLAYER_WEAPON_DATA* weaponPtr = &(playerStatusPtr->WeaponSlot[playerStatusPtr->SelectedWeaponSlot]);
								TEMPLATE_WEAPON_DATA *twPtr = &TemplateWeapon[weaponPtr->WeaponIDNumber];
								if (ShowDebuggingText.GunPos)
								{
									PrintDebuggingText("Gun Position x:%d,y:%d,z:%d\n",twPtr->RestPosition.vx,twPtr->RestPosition.vy,twPtr->RestPosition.vz);
								}
							}
						}
						//#endif  /* MainTextPrint */

						ThisFramesRenderingHasBegun();

						{
							ProcessorStageClock clock(kProcessorGame);

							DoAllShapeAnimations();

							UpdateGame();
						}

						{
							ProcessorStageClock clock(kProcessorViews);

							AvpShowViews();
						}

						{
							ProcessorStageClock clock(kProcessorHud);

							// Do screen shot here so that text and hud graphics aren't shown
							MaintainHUD();

							// A tester's mark: noted now, and its picture taken when this frame is drawn
							Mark_ReadKey();

							FlushTextprintBuffer();

							// check music status
							Music_Check();
						}

						// check to see if we're pausing the game;
						// if so kill off any sound effects
						if (InGameMenusAreRunning() && ((AvP.Network != I_No_Network && netGameData.skirmishMode) || (AvP.Network == I_No_Network))) {
							SoundSys_StopAll();
						}
					}
					else
					{
						ReadUserInput();
						SoundSys_Management();

						ThisFramesRenderingHasBegun();
					}

					// draw ingame menu
					{
						ProcessorStageClock clock(kProcessorMenus);

						menusActive = AvP_InGameMenus();
					}

					if (AvP.RestartLevel) menusActive = FALSE;

					if (AvP.LevelCompleted)
					{
						SoundSys_FadeOutFast();
						DoCompletedLevelStatisticsScreen();
						thisLevelHasBeenCompleted = 1;
					}

					/* after this call, no more graphics can be drawn until the next frame */
					ThisFramesRenderingHasFinished();

					FlipBuffers();
					FrameCounterHandler();
					
					PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *) (Player->ObStrategyBlock->SBdataptr);

					if (!menusActive && playerStatusPtr->IsAlive && !AvP.LevelCompleted)
					{
						DealWithElapsedTime();
					}
					
					break;
				}
				case I_GM_Menus:
				{
					AvP.GameMode = I_GM_Playing;
					LOCALASSERT(AvP.Network == I_No_Network);
					break;
				}

				default:
				{
					GLOBALASSERT(2<1);
					break;
				}
			}

			if (AvP.RestartLevel)
			{
				AvP.RestartLevel = 0;
				AvP.LevelCompleted = 0;
				FixCheatModesInUserProfile(UserProfilePtr);
				RestartLevel();
			}
		} 
		// end of main game loop

		AvP.LevelCompleted = thisLevelHasBeenCompleted;
		mainMenu = true;

		FixCheatModesInUserProfile(UserProfilePtr);

		#if !(PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO)
		TimeStampedMessage("We're out of the main loop");

		ReleaseAllFMVTextures();

		CONSBIND_WriteKeyBindingsToConfigFile();

		DeInitialisePlayer();

		DeallocatePlayersMirrorImage();

		// bjd - here temporarily as a test
		SoundSys_StopAll();

		Destroy_CurrentEnvironment();

		EndNPCs(); /* JH 30/4/97 - unload npc rifs */
		ExitGame();

		#endif

		ResetEaxEnvironment();

		// make sure the volume gets reset for the menus
		SoundSys_ResetFadeLevel();

		Music_Stop();
		Vorbis_CloseSystem(); // stop ogg vorbis player

		// netgame support
		if (AvP.Network != I_No_Network && !Coop_LevelChangeIsDue())
		{
			// The page after a co-op game says why it ended, and the game's end is about to wipe what tells
			CoopMenu_TheGameIsEnding();

			/* we cleanup and reset our game mode here, at the end of the game loop, as other 
			clean-up functions need to know if we've just exited a netgame */
			EndAVPNetGame();
			//EndOfNetworkGameScreen();
		}

		ClearMemoryPool();

#if 0 //bjd - FIXME
		if (LobbiedGame)
		{
			/*
			We have been playing a lobbied game , and have now diconnected.
			Since we can't start a new multiplayer game , exit to avoid confusion
			*/
			break;
		}
#endif
	}
	#if !(PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO)
	TimeStampedMessage("After Menus");

	/* Added 28/1/98 by DHM: hook for my code on program shutdown */
	DAVEHOOK_UnInit();

	EmptyUserProfilesList();
	ClearMultiplayerLevelNameArray();

	/*-------------------Patrick 2/6/97-----------------------
	End the sound system
	----------------------------------------------------------*/

	SoundSys_StopAll();
	SoundSys_RemoveAll(); 

	#else
//	QuickSplashScreens();
	#endif

	Tex_Release(AAFontImageNumber);
	Tex_Release(AVPMENUGFX_SLIDERBAR);
	Tex_Release(AVPMENUGFX_SLIDER);

	ExitSystem();

	Config_Save();

	// close dx logfile if open (has to be called after all calls to TimeStampedMessage()
#if debug
	dx_log_close();
#endif

	Music_Deinit();
	ClearMemoryPool();

	// restore stickey keys setting
	SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &startupStickyKeys, 0);

	// 'shutdown' timer
	timeEndPeriod(1);

	if (UseMouseCentreing)
	{
		FinishCentreMouseThread();
	}

	CloseDebugConsole();

	return 0;
}
