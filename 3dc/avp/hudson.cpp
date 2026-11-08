// Modified 2026-11-06 by Melanikus for the AvP port:
//   siege= has both players of a pair stand in a room against every kind of enemy the host can make, and a film of it has a clock on it
//   every game of a pair run starts with a smoke test: the players turn to each other, and each checks it can see the other, before anything else the run does
//   a fight's idle player looks at the fight
//   a run's window goes on the second screen by the game's own doing, with a part of it for each game
//   killkind=, killat= and killer= have one machine really kill a level's own named NPC, and each machine says what it saw
//   a run is held to 60 frames a second unless it asks otherwise, a frame that's late is made up by the next, and a run says what rate it really reached
//   a run can play a level's own films, and say how the connection fared under one
//   what is on the next level is said once it is there; a pair stays a while after a level starts again; a run says when a player's spare ammunition changes
//   listplace= starts a level as the level row does, meets=yes says what the player meets, dieat= kills him once, difficulty= sets it
//   coop=yes and coopend= play a net= run as a campaign, a level's end and the next level are logged, and a dead player says so and comes back
//   gunmarine=yes stands a marine holding that gun in front of him on a gun= run, and he takes its picture
//   in a level with a mirror, a gun= run turns him round and takes pictures on the way
//   gunalien=yes: aliens stand still, and he looks at the middle of the last one made
//   in a level with a mirror, a gun= run says which model the marine in it has
//   gunalien=yes makes an alien in front of him on a gun= run, for a throw to hit
//   class= starts a gun= run as a skirmish as that marine class, and logs what he started with
//   gun= tries a marine's gun in the player's own hands, with nobody at the keyboard
//   gun=15 ends by trying Next Weapon and Previous Weapon with an empty pulse rifle
//   species=predator lets a gun= run play as a Predator
//   a gun=7 run looks a different way before each throw
//   a gun=12 run goes on to the revolver's reloads and gun changes that its timed steps never reach
//   each level says how many of each kind of strategy it holds, once it has settled
//   botfight=: the host makes an NPC beside the joiner, the joiner fires at it, and both say what happens
// Hudson: a marine the camera follows, so that the game can be measured with nobody at the keyboard. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: someone playing can't do the same thing twice, and can't stand in forty rooms every time the code changes. So a run starts a level by itself, does what it was asked, writes what it found,
// and ends with a number a script can read. The plan is AvP-docs\engine\hudson-plan.md. So far this file holds the start of that: a run that loads a level, plays some frames with nobody touching anything,
// and shuts down the normal way. Hudson himself comes next.
//
// A RUN MUSTN'T GET IN ANYONE'S WAY. The machine it runs on is someone's, and they may be working, or recording. So in a run the game makes no sound, leaves the mouse alone, doesn't take the focus, keeps going
// without it, and doesn't change the Windows settings it changes for a player. It keeps all its files in a folder of its own, beside the game. It won't start unless the machine has been left alone for a while.
//
// Those are promises, and reading the code doesn't prove them: Windows has its own ideas about a new window and the focus. So the result line says what really happened: how many frames the window had the focus
// for, whether the mouse is where it started, and whether the sound is on. It also says what size the picture is and whether the game waits for the screen, read back from what the graphics card was really given,
// because a run that's a different size from the last isn't the same run. A normal run says size=1280x720 vsync=off focus_frames=0 mouse=still sound=off.
//
// THE WATCHDOG is a small thread of its own, because the game can be stuck loading, or stuck altogether, and then nothing else would notice anything. It does two things. If anyone touches the keyboard or the
// mouse, it says so in the log and ends the run at once: they get their machine back, and the run's timings would mean nothing anyway. And if the run goes past its time limit, it says so and ends it, because a
// run that's stuck should say so and stop. Ending it this way skips the game's shutdown, on purpose: the shutdown could be what's stuck.
//
// One thing can stop even the watchdog. It writes to the log as the game does, one at a time, so a game stuck in the middle of writing to the log stops the watchdog too. That's what the limit in
// tools/hudson/run-hudson.ps1 is for: it ends the game a minute after this one should have.
//
// THE MARINE. With rooms= on the command line there's a real marine, made by the game's own CreateMarineBot, standing under the same guard mission as the marines a level places. He has his own brain and nothing is
// faked: all this file does is tell him where to look, by the same field a level would, and put the camera where he is. The player stays where the level started him, as an Observer, which nothing aims at and which has
// no gun on the screen. Nobody else runs, and nothing makes more of them, so nothing happens to him. He's looked up by name each time, not kept as a pointer, because the game can destroy him at any moment.
//
// A view goes like this: he's told which way to look, turns until he's facing that way and has stood still for a moment, then holds still for a set number of frames. The log has a line at each end of the hold, with where
// he is and where the camera is, so that anyone can check the camera is where he is. If he goes, won't settle, moves during a hold, or frames= runs out, the run says so and ends with its own number.
//
// HOW THE WORLD IS DRAWN. lighting= and shadows= set the same two values the game's own commands change. If this machine can't draw what was asked for, the game would quietly draw something else, and a comparison would
// compare the wrong thing. So the first frame of the level checks, and the run ends there with its own number. The result line says what was really drawn at the end of the run, not what was asked for.
//
// PICTURES. record=pictures saves one picture in the middle of each hold, named for its room and view, so that the same view from two runs has the same name. It's the whole frame, as a file that loses nothing. Saving
// one stops the game for a moment, so a run with pictures says nothing about timings.
//
// A WALL. wall= puts him in front of the biggest wall in the level showing one picture, found from the level's own polygons, so that nobody has to know the numbers for a place, and so that the place is sure to show what
// it's meant to. The player moves there too, the same way the game moves a player in a network game, because the game decides which rooms to draw and which marines to think about from where the player is. It says
// in the log which wall it chose and how many there were, how big it is in the world and in its picture, and which way each view looks, because a depth map should shift more the more slanted the view is. One view is
// from a step along the wall, and he and the player are moved there the same way, because a depth map that's right holds still against the wall when the camera moves.
//
// COMPARING. compare=relief draws the same view each way in the list in hudson_plan.cpp, with a picture of each, with him standing still between them. Lighting goes through what the game's commands use, because that
// keeps the rest of the view in step. The flips and the depth are only read into the shader each frame, so they're set directly. tools/hudson/compare-pictures.ps1 reads the pictures afterwards.
//
// A TOUR. rooms= more than 1 takes him through the level. The rooms come in order of how many doorways they are from the first, from the game's own list of which room joins which. He's put in the middle of each one, on its floor, not walked there, by the
// test the game itself makes for standing in a place, and he looks round it. If the middle has no floor, the log says how far off he is. If he goes, won't settle or moves during a hold, the log says so and the tour goes on to the next room, because someone's watching. Nothing can kill him.
// tourfrom= starts at a room of that name and goes on to its neighbours. Without it, the tour spreads out over the whole of what joins up. holdseconds= holds each view for a number of real seconds, for someone watching.
//
// WHILE SOMEONE USES THE MACHINE. machine=shared is for runs that don't measure time, like pictures and logs. It doesn't wait for the machine to be idle, and touching the machine doesn't end the run: the result line says
// machine=shared, and touched=yes if anyone did. It waits for the screen as a player's game does, so it draws no more than can be seen. Every other promise stands. What a person does never reaches any run: the game doesn't listen to the mouse or the game pads, and the window ignores keys, buttons and the wheel.
//
// A window can be covered by another while someone works. Pictures don't care, because they come from the picture the card has just drawn, before it's sent to the window. If Windows takes the card away altogether, as a locked
// screen can, the game waits to get it back, the way it does for a player, and the time limit ends the run if it never comes.
//
// THE USER'S OWN COPY. A run won't start in a Steam folder. steam=yes is the one way in, for a run the user starts from Steam to record. All the run writes goes in HudsonRun beside the exe, as anywhere else, and it never touches their Documents.
// tools/hudson/run-hudson.ps1 won't do it, because that script empties folders. With steam=yes and anything else wrong with the words, it's still refused, with nothing written.
//
// A GUN. gun= with a slot number tries a marine's gun in the player's own hands, with nobody at the keyboard, and writes all it does to the log. It goes through what his hands go through: the same call as GIVEALLWEAPONS, the same requests as his keys, and the same weapon code. So a gun can be tried, and a crash found, without anyone at the machine. See Hudson_GunInput for the order things happen in. record=pictures saves a picture of each state the gun goes through, the first time it's there.
//
// A FIGHT. combat=alien is for watching the game's own marine fight the game's own alien. Nothing is kept still: everything in the level runs, the hive and the squad too. He stands guard, and a few seconds later
// an alien is made in front of him by CreateAlienBot, the same call the game's own cheat uses. What he does about it is all his own: his own search for a target, his own aim, his own gun. All this file does is keep
// him alive, keep the camera behind him so that he can be seen, and count. aliens= makes more than one, the next a couple of seconds after the last one dies, because one alien is over in about a second, whatever the
// frame rate: the game's clock is real time. The run ends a few seconds after the last one dies, or when frames= runs out, and the log and the result line say which. A network game refuses it.
//
// A fight with someone who can't die can go on for ever, in a way the game never has to deal with, so two things keep it moving. He keeps his courage, because a marine who runs out of it only tries to run, and the first
// run with five aliens ended with him standing still, not firing, being bitten for four minutes. And if the alien hasn't been hurt for a while, that fight has stalled: the log says so, the alien is taken away, and the next
// one comes. The result line counts those as stalled=, so a run that did that can't be mistaken for one where he won them all.
//
// A FLARE. flare=yes has him throw a flare as each hold starts, through the same call as the key, so that a picture and a film can show what a flare's light does to the room. It's a run that looks round and
// nothing else, and never one that compares what's on the screen. The game allows only a few at a time and throws no more past that, without saying so, and that's why the result counts the ones that really went.
//
// SHOWING THE WINDOW. A run's window stays out of the way: it can't be activated and has no taskbar button. show=yes is for someone watching or recording: it opens as a normal game's window does and comes to the front, and Alt+F4 closes it.
// It writes where the window is to hudson-window.txt as it goes, for someone who can't see it.
//
// THE SIZE. A run is 1280x720, so that one run compares with the last. size= can ask for 1600x900, 1920x1080 or 2560x1440. A run sharing the machine is 1920x1080 unless it says otherwise or saves pictures, because someone may be watching, and it gives way
// to 1280x720 if that doesn't fit the main screen. The result line says what size it really was.
//
// READING WHAT HE SEES. vision=check reads the frame's three surface pictures once in each hold, and says in the log what's missing, with the polygon it's on. hudson_vision.h says what it counts. It stops the game for a moment each view, so such a run says nothing about timings.
//
// THE STATIC SCENE ROUTE. A route that's switched on and one that said no look the same on the screen, so the result says how many frames of the level drew walls from the fixed buffer, out of how many asked. With the
// route's switch off, that's 0/0. tools/hudson/run-hudson.ps1 turns the switch on with -Static.
//
// WHICH LEVEL. level= is the exact name of any of the game's own levels: a marine's, a Predator's, an Alien's, or a multiplayer or co-op one. He's a marine wherever he is, the way the game's own cheat menu lets a player be any species on any level. What a level expects of its player
// can be wrong for him: a level for an Alien has places only an Alien can reach. A tour puts him in rooms, so it doesn't care, but a walk might.
//
// IN A NORMAL GAME NONE OF THIS RUNS. It needs -dev and -hudson, each as a word on its own. Without both, HudsonRunIsOn is false from start to finish, and every place in the game that asks it carries on as if this
// file weren't there.
//
// Only two small places here touch the shadow code: Hudson_SetTheMode, and what reads the mode back. Leave those out, and a run can be added to a local build of an older branch, to compare with.

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "logString.h"
#include "ConfigFile.h"
#include "psnd.h"
#include "psndplat.h"
#include "detaillevels.h"
#include "networking.h"
#include "enet\enet.h"
#include <psapi.h>
#include "d3_func.h"
#include "bh_types.h"

#include "targeting.h"
#include "weapons.h"
#include "pldghost.h"
#include "pldnet.h"

extern NetID AvPNetID;
extern MODULE *playerPherModule;
extern char LevelName[];
extern void RemovePickedUpObject(STRATEGYBLOCK *objectPtr);
extern bool NetLoopbackSearch;                                      // networking.cpp
extern bool NetLoopbackOnly;                                        // networking.cpp
extern int MultiplayerEpisodeForEnvironment(int environment);       // env_info.cpp
#include "dynblock.h"
#include "bh_marine.h"
#include "bh_predator.h"
#include "bh_corpse.h"
#include "npc_ghosts.h"
#include "bh_xeno.h"
#include "bh_queen.h"
#include "bh_facehugger.h"
#include "bh_agun.h"
#include "bh_alien.h"
#include "bh_ais.h"
#include "equipment.h"
#include "los.h"
#include "Player.h"
#include "view_switches.h"
#include "pvisible.h"
#include "pheromone.h"
#include "pfarlocs.h"
#include "hudson_vision.h"
#include "hudson_video.h"
#include "shoulder_lamp.h"

void ShadowLights_ForTheResult(char *line, int size);   // kshape.cpp

// lampat=wide: about a marine's shoulder width out from his eyes, and a bit up, in millimetres
static const float kHudsonLampWideAcross = 500.0f;
static const float kHudsonLampWideUp = 200.0f;
#include "hudson_video_writer.h"
#include "surface_pictures.h"
#include "shadow_shoot.h"
#include "TextureManager.h"
#include "hudson.h"
#include "load_shape.h"
#include "hudson_plan.h"
#include "custom_maps.h"
#include "hmodel.h"
#include "weapon_hooks.h"
#include "weapon_revolver.h"
#include "static_scene.h"
#include "hudson_strategies.h"
#include "level_events.h"
#include "coop_reach.h"
#include "hudson_nav.h"
#include "pmove.h"
#include "win95/frontend/env_info.h"
#include "win95/frontend/level_lists.h"
#include <map>
#include <utility>
#include "pldnet.h"
#include "pldghost.h"

#include <math.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

// The commit this was built from, for the result. This file isn't in the repository, and nothing in the repository writes it: whoever builds the game can make 3dc/build_commit.h with one line,
// #define AVP_BUILD_COMMIT "abc1234". Without it the result says "not known". tools/hudson/run-hudson.ps1 also keeps a fingerprint of the exe, which is right whatever this says.
#if __has_include("build_commit.h")
#include "build_commit.h"
#else
#define AVP_BUILD_COMMIT "not known"
#endif

extern int MarineEpisodeNamed(const char *name);   // env_info.cpp
extern int BuiltInEnvironmentNamed(const char *name);   // env_info.cpp
extern bool MirroringActive;                       // kshape.cpp
extern int DrawFullBright;                         // kshape.cpp: the game's own switch for drawing everything fully lit
extern HWND hWndMain;                              // win_proj.cpp
extern BOOL ForceLoad_Marine;                      // npcsetup.cpp
extern BOOL ForceLoad_Alien;                       // npcsetup.cpp
extern BOOL ForceLoad_Predator;                    // npcsetup.cpp
extern BOOL ForceLoad_Xenoborg;                    // npcsetup.cpp
extern BOOL ForceLoad_Hugger;                      // npcsetup.cpp
extern BOOL ForceLoad_SentryGun;                   // npcsetup.cpp
extern char *ModuleCurrVisArray;                   // which rooms the game has up just now, by each room's number
void CreateAlienBot(VECTORCH *Position, int type); // bh_alien.cpp. Type 0 is an ordinary alien
SECTION *GetNamedHierarchyFromLibrary(const char *rif_name, const char *hier_name); // projload.cpp
void MakeMatrixFromDirection(VECTORCH *directionPtr, MATRIXCH *matrixPtr); // weapons.cpp
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;        // the camera
extern int ToyShadowsMode;                         // d3d_render.cpp: the view being shown, and 0 is the one called OFF, with no shadow pass at all
extern int ModuleArraySize;                        // how many rooms the level has
void ViewSwitches_AskLighting(int choice);         // d3d_render.cpp: the way the game's commands and menu ask
bool ViewSwitches_MainDrawWentThroughHeights();    // d3d_render.cpp

bool HudsonRunIsOn = false;
bool HudsonBreaksPrisms = false;
bool HudsonMarineIsOn = false;
bool HudsonPeopleStandStill = false;
int HudsonWayOfDying = -1;
bool HudsonWorldIsLive = false;

static const DWORD kWatchdogLooksEveryMilliseconds = 50;
static const char kRunFolderName[] = "HudsonRun";
static const char kResultFileName[] = "hudson-result.txt";
static const char kWindowFileName[] = "hudson-window.txt";

// These count frames, which go by at whatever speed the machine manages until a run has a fixed clock. So a moment here is a different time on a different machine, and that's fine: it only has to be long enough.
static const int kFramesToSettle = 30;         // how long he has to stand still facing the right way before a hold starts
static const int kMostFramesToSettle = 20000;  // more than this and he isn't going to
static const int kFramesForACase = 60;         // how long each way of drawing is held. A new way of drawing takes a frame or two to settle in, and the picture comes half way through
static const int kMostCornersOfAWall = 8;      // more corners than the game's polygons have
static const int kEndOfCorners = -1;           // what ends a polygon's list of corners
static const DWORD kMostMillisecondsToSettleOnATour = 10000;   // someone is watching, and one bad room shouldn't hold up the rest
static const DWORD kMostMillisecondsToSettle = 30000;          // with one room there's nothing else to go on to, but a long turn takes seconds, so this is plenty
static const DWORD kMillisecondsBetweenSettlingReports = 5000; // nothing else goes in the log while he's settling, so a stuck run would look like a slow one
static const int kPlayerFollowsEvery = 500;    // how far he walks before the player is moved to where he is: half a metre
static const int kStillIsWithin = 8;           // how far he can drift from one frame to the next and still be standing still: 8 millimetres

// A fight is for someone to watch, so these are real time and not frames, which go by faster on a faster machine.
static const DWORD kMillisecondsBeforeTheAlien = 3000;   // long enough to see him standing there first
static const DWORD kMillisecondsBetweenAliens = 2000;    // long enough to see that one's down before the next
static const DWORD kMillisecondsAfterTheKill = 5000;     // long enough to see what's left
static const DWORD kMillisecondsBeforeAFightHasStalled = 20000;   // the longest the first run went between hurting an alien and hurting it again, in a fight that went on to end, was under 7 seconds
static const int kBlowsToLog = 20;              // enough to show they're landing, and the result line has how many there were
static const int kFramesBetweenFightReports = 60;
static const int kAlienDistances[] = { 7000, 5000, 3000 };   // how far from him to try to make it, in millimetres, furthest first, so that he has time to see it coming
static const int kWaysToTryForTheAlien = 8;
static const int kRoomBehindTheAlien = 1000;    // it needs somewhere to stand, so the way has to stay clear this far past it
static const int kChestHeight = 800;            // how far above where he stands to look from, so that a look along the floor doesn't hit the floor
static const int kCameraBehindHim = 2500;
static const int kCameraAboveHisEyes = 400;     // the game adds eye height itself, later on. This is only to see over his shoulder
static const int kCameraOffAWall = 200;         // how far short of a wall the camera stops, so that it doesn't see through it

static HudsonPlan plan;
static VECTORCH playerFollowedTo = { 0, 0, 0 };
static const char *refusalFromReading = 0;
static int episode = -1;
static int environment = -1;
static int customMapPlace = -1;
static DWORD startedAt = 0;
static DWORD lastTouchAtStart = 0;

static int exitCode = kHudsonExitFinished;

// The smoke test: see Hudson_SmokeTest. Each level, and each restart, has one. The first failure on any level is what the run says.
static bool smokeIsOver = false;
static bool smokeIsFinishing = false;
static bool smokeWentOver = false;
static DWORD smokeWaitingSince = 0;
static DWORD smokeSawThemAt = 0;
static VECTORCH smokeCameFrom;
static int smokeLevelsPassed = 0;
static char smokeFailure[160] = "";

static void Hudson_SmokeTestStartsAgain()
{
	smokeIsOver = false;
	smokeIsFinishing = false;
	smokeWentOver = false;
	smokeWaitingSince = 0;
	smokeSawThemAt = 0;
}
static double FramesEachSecondReached();
static volatile LONG framesDone = 0;
static volatile LONG machineWasTouched = 0;
static volatile LONG resultIsWritten = 0;     // only the first to end the run gets to say how it ended
static volatile LONG resultIsOnDisk = 0;      // and this says it has finished saying it
static volatile LONG levelHasEnded = 0;       // nobody touching the machine matters only until then
static volatile LONG framesWithTheFocus = 0;
static POINT mouseAtStart = { 0, 0 };
static bool mouseAtStartIsKnown = false;

enum MarineStage
{
	kMarineNotMade,
	kMarineSettling,
	kMarineHolding,
	kMarineWalking,    // nav= is taking him through the level: see hudson_nav.cpp
	kMarineFighting,
	kMarineAfterTheKill,
	kMarineFinished
};

enum EnemyState
{
	kEnemyNotMade,
	kEnemyAlive,
	kEnemyDead,
	kEnemyTakenAway
};

static MarineStage marineStage = kMarineNotMade;
static char marineName[SB_NAME_LENGTH] = { 0 };
static VECTORCH marinePlaceLastFrame = { 0, 0, 0 };
static int viewNow = 0;
static DWORD settlingSince = 0;
static DWORD lastSettlingReportAt = 0;
static VECTORCH toldToFaceFrom = { 0, 0, 0 };   // where he was when he was last told which way to face
static DWORD holdStartedAt = 0;
static bool pictureTakenThisHold = false;
static long flaresThrown = 0;
static int wantedX = 0;
static int wantedZ = 0;
static int framesInThisStage = 0;
static int framesSettled = 0;
static int caseNow = 0;
static int wallTowardsX = 0;
static int wallTowardsZ = 0;
static int wallStandX = 0;
static int wallStandZ = 0;
static volatile LONG roomsReached = 0;
static volatile LONG viewsDone = 0;
static volatile LONG picturesSaved = 0;
static bool pictureIsDue = false;
static bool visionIsDue = false;
static bool visionTakenThisHold = false;
static LONG visionViews = 0, visionReadsFailed = 0, visionMirrored = 0, visionNoNormal = 0, visionNoDistance = 0, visionHoles = 0, visionNothingDrawn = 0, visionRaysAgree = 0, visionRaysCompared = 0, visionNoShadows = 0, visionScreenDiffers = 0, visionScreenCompared = 0, visionScreenTooDark = 0, visionDoorRaysAgree = 0, visionDoorRaysCompared = 0, visionRaysAtBacks = 0, visionHolesAtBacks = 0;

// The fight. The ones another thread can read are for the result line, which can be written from there.
static char alienName[SB_NAME_LENGTH] = { 0 };
static volatile LONG enemyState = kEnemyNotMade;
static volatile LONG tookTheAlienAsATarget = 0;
static volatile LONG shotsFired = 0;
static volatile LONG timesItWasHurt = 0;
static volatile LONG blowsOnHim = 0;
static volatile LONG acidOnHim = 0;
static volatile LONG aliensKilled = 0;
static volatile LONG fightsStalled = 0;
static DWORD alienLastHurtAt = 0;
static int aliensMade = 0;
static bool thisAlienIsHisTarget = false;
static int clipLastFrame = 0;
static int alienHealthLastFrame = 0;
static DWORD thisPartOfTheFightStartedAt = 0;
static bool saidTheFirstShot = false;
static char fightPictureName[32] = { 0 };

// What was really drawn at the end of the last frame, for the result, which another thread can be writing. Asking the game itself is only for the thread that draws. -1 is before there's been a frame.
static volatile LONG lightingDrawn = -1;
static volatile LONG shadowsDrawn = -1;
static volatile LONG staticFramesWithWalls = 0;
static volatile LONG staticFramesAsked = 0;

// When anyone last touched the keyboard or the mouse, in the same milliseconds as GetTickCount. false means Windows wouldn't say.
static bool LastTouch(DWORD *when)
{
	LASTINPUTINFO info;

	info.cbSize = sizeof(info);
	info.dwTime = 0;

	if (!GetLastInputInfo(&info))
	{
		return false;
	}

	*when = info.dwTime;
	return true;
}

// Is the mouse where it was when the run started? Windows won't say if the screen is locked, and then this doesn't guess.
static const char *WhereTheMouseIs()
{
	POINT mouseNow = { 0, 0 };

	if (!mouseAtStartIsKnown || !GetCursorPos(&mouseNow))
	{
		return "unknown";
	}

	return mouseNow.x == mouseAtStart.x && mouseNow.y == mouseAtStart.y ? "still" : "moved";
}

// What the graphics card was given, which is what counts: a size it won't take falls back to another by itself. A run stopped before the card was set up has no answer yet, and its size says 0x0.
static const char *WhetherItWaitsForTheScreen()
{
	if (d3d.d3dpp.BackBufferWidth == 0)
	{
		return "unknown";
	}

	return d3d.d3dpp.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ? "off" : "on";
}

static const char *WordForLightingDrawn()
{
	return lightingDrawn < 0 ? "unknown" : Lighting_Word(lightingDrawn);
}

static const char *WordForShadowsDrawn()
{
	return shadowsDrawn < 0 ? "unknown" : shadowsDrawn ? "on" : "off";
}

static const char *WordForTheEnemy()
{
	return enemyState == kEnemyDead ? "dead" : enemyState == kEnemyAlive ? "alive" : enemyState == kEnemyTakenAway ? "stalled" : "none";
}

// One line, beside the log, that a script can read. The first thing that ends the run writes it.
//
// Something can still go wrong after that: the result is written before the shutdown starts, and the shutdown can stick, or fail an assert. Then the file would go on saying exit=0 about a run that ended
// another way, so whatever comes later adds a line of its own, saying what really happened. The exit code Windows reports is always the last word.
// A run's film. See hudson_video.h. The file's name has the time in it, so that a run started by hand can't write over the last one. run-hudson.ps1 moves it to the folder it keeps a run in.
static char videoName[64] = { 0 };
static volatile LONG videoGrabs = 0, videoGrabMicroseconds = 0, videoGrabWorstMicroseconds = 0;
static volatile LONG videoSaidItStopped = 0, videoSaidItDropped = 0;

static void SayIfTheVideoHasNews(const HudsonVideoFacts &facts)
{
	if (facts.stopped[0] != 0 && InterlockedExchange(&videoSaidItStopped, 1) == 0)
	{
		LogErrorString(std::string("video: ") + facts.why);
	}

	if (facts.dropped > 0 && InterlockedExchange(&videoSaidItDropped, 1) == 0)
	{
		char line[160] = { 0 };

		_snprintf(line, sizeof(line) - 1, "video: at frame %ld the writer has fallen behind and pictures are being dropped, so the film will have gaps", framesDone);
		LogString(line);
	}
}

// Every way a run ends comes through WriteResult, which calls this, so the film is always finished if it can be. If the writer is stuck, this stops waiting, and what was written so far can still be played.
static void FinishTheVideo(HudsonVideoFacts *facts)
{
	HudsonVideoWriter_Finish(kHudsonVideoFinishWaitMilliseconds);
	HudsonVideoWriter_GetFacts(facts);
	SayIfTheVideoHasNews(*facts);

	char line[400] = { 0 };
	long const grabs = videoGrabs;

	_snprintf(line, sizeof(line) - 1, "video: %s has %ld pictures in %lld bytes. %ld moments of the film had no frame from the game. Reading a frame back from the card took %.2f ms on average and %.2f ms at worst, %ld times", videoName[0] ? videoName : "no film",
		facts->written, facts->bytes, facts->noPicture, grabs ? videoGrabMicroseconds / 1000.0 / grabs : 0.0, videoGrabWorstMicroseconds / 1000.0, grabs);
	LogString(line);

	if (facts->dropped > 0)
	{
		_snprintf(line, sizeof(line) - 1, "video: this is not a clean recording: %ld pictures were dropped because the writer fell behind, so the film has gaps", facts->dropped);
		LogErrorString(line);
	}
}

// The first to end a run can take a while over it, because it finishes the film first. Anyone else ending it has to wait for that, or the game can end with the result not yet written, and a run that really finished looks as if it failed
// with no numbers. The wait has a limit, in case the first to end the run is itself stuck.
static void WaitForTheResultToBeOnDisk()
{
	DWORD const most = kHudsonVideoFinishWaitMilliseconds + 5000;
	DWORD const started = GetTickCount();

	while (!resultIsOnDisk && GetTickCount() - started < most)
	{
		Sleep(10);
	}
}

// net=: how far this one has got. See Hudson_NetSay.
static char netStage[160] = "it hasn't got as far as the network";

static void WriteResult(const char *howItEnded, int code)
{
	bool const isTheFirst = InterlockedExchange(&resultIsWritten, 1) == 0;

	char const *folder = GetSaveFolderPath();
	std::string const path = std::string(folder ? folder : "") + kResultFileName;
	char line[640] = { 0 };

	if (!isTheFirst)
	{
		WaitForTheResultToBeOnDisk();
		_snprintf(line, sizeof(line) - 1, "and_then=%s exit=%d", howItEnded, code);

		FILE *const laterFile = fopen(path.c_str(), "a");

		if (laterFile)
		{
			fprintf(laterFile, "%s\n", line);
			fclose(laterFile);
		}

		LogErrorString(std::string("hudson: ") + line);
		return;
	}

	HudsonVideoFacts videoFacts = {};

	if (plan.recordVideo)
	{
		FinishTheVideo(&videoFacts);
	}

	// Only in a run with a fight, so that the line from any other run stays as it was.
	char fight[140] = { 0 };

	if (plan.combatAlien)
	{
		_snprintf(fight, sizeof(fight) - 1, " target=%s shots=%ld hits=%ld blows=%ld acid=%ld killed=%ld/%d stalled=%ld enemy=%s", tookTheAlienAsATarget ? "yes" : "no", shotsFired, timesItWasHurt, blowsOnHim, acidOnHim, aliensKilled, plan.combatAliens, fightsStalled, WordForTheEnemy());
	}

	_snprintf(line, sizeof(line) - 1, "commit=%s level=%s frames_asked=%d frames_done=%ld seconds=%.1f rooms_asked=%d rooms_reached=%ld views_done=%ld pictures=%ld%s lighting=%s shadows=%s static_frames=%ld/%ld size=%ux%u vsync=%s focus_frames=%ld mouse=%s sound=%s machine=%s touched=%s ended=%s exit=%d", AVP_BUILD_COMMIT, plan.level, plan.frames, framesDone,
		(double)(GetTickCount() - startedAt) / 1000.0, plan.rooms, roomsReached, viewsDone, picturesSaved, fight, WordForLightingDrawn(), WordForShadowsDrawn(), staticFramesWithWalls, staticFramesAsked, d3d.d3dpp.BackBufferWidth, d3d.d3dpp.BackBufferHeight, WhetherItWaitsForTheScreen(), framesWithTheFocus, WhereTheMouseIs(), SoundSys_IsOn() ? "on" : "off", plan.machineShared ? "shared" : "alone", machineWasTouched ? "yes" : "no", howItEnded, code);

	FILE *const file = fopen(path.c_str(), "w");

	if (file)
	{
		fprintf(file, "%s\n", line);

		// Always, so that a result never has to be read as if a run went at the rate it asked for
		fprintf(file, "frames_each_second_asked=%d frames_each_second_reached=%.1f\n", plan.framesPerSecond, FramesEachSecondReached());

		// Walking and being put somewhere are two numbers, so that being put somewhere can't read as walking.
		if (plan.navModules > 0)
		{
			fprintf(file, "nav_asked=%d nav_walked_into=%d nav_moved_on=%d nav_pressed=%d nav_pressed_by_asking=%d nav_presses_that_did_something=%d\n", plan.navModules, HudsonNav_ModulesWalkedInto(), HudsonNav_TimesMovedOn(), HudsonNav_ThingsPressed(), HudsonNav_ThingsPressedByAsking(), HudsonNav_PressesThatDidSomething());
		}

		if (plan.vision)
		{
			fprintf(file, "vision_views=%ld vision_mirrored=%ld vision_reads_failed=%ld vision_no_normal=%ld vision_no_distance=%ld vision_holes=%ld vision_nothing_drawn=%ld vision_no_shadows=%ld vision_rays_agree=%ld vision_rays_compared=%ld vision_door_rays_agree=%ld vision_door_rays_compared=%ld vision_rays_at_backs=%ld vision_holes_at_backs=%ld vision_screen_differs=%ld vision_screen_too_dark=%ld vision_screen_compared=%ld vision_shader=%s\n", visionViews, visionMirrored, visionReadsFailed, visionNoNormal, visionNoDistance, visionHoles, visionNothingDrawn, visionNoShadows, visionRaysAgree, visionRaysCompared, visionDoorRaysAgree, visionDoorRaysCompared, visionRaysAtBacks, visionHolesAtBacks, visionScreenDiffers, visionScreenTooDark, visionScreenCompared, SurfacePictures_ShaderUsedLast());
		}

		// Always, so that nobody has to guess whether a run had it
		fprintf(file, "lamp=%s lamp_kind=%s lamp_at=%s lamp_frames=%ld lamp_real_frames=%ld\n", plan.lampOn ? "on" : plan.lampBlink ? "blink" : "off", plan.lampReal ? "real" : "painted", plan.lampWide ? "wide" : "shoulder", ShoulderLamp_FramesDrawn(), ShoulderLamp_FramesAsARealLight());
		fprintf(file, "flares_thrown=%ld\n", flaresThrown);

		// Always, like the lamp's. shadow_light_frames says whether it means anything: with shadows off, the shadow pass never ran.
		char shadowLights[320] = { 0 };

		ShadowLights_ForTheResult(shadowLights, (int)sizeof(shadowLights));
		fprintf(file, "%s\n", shadowLights);

		if (plan.netMode != kHudsonNetNone)
		{
			fprintf(file, "net=%s net_stage=%s\n", plan.netMode == kHudsonNetHost ? "host" : "join", netStage);

			if (smokeFailure[0])
			{
				fprintf(file, "smoke=failed: %s\n", smokeFailure);
			}
			else if (smokeLevelsPassed > 0)
			{
				fprintf(file, "smoke=passed on %d levels\n", smokeLevelsPassed);
			}
			else
			{
				fprintf(file, "smoke=failed: the run ended before it was done\n");
			}
		}

		// Always, so that nobody has to guess whether a run has a film, or where. run-hudson.ps1 changes this to where it moves the film to.
		fprintf(file, "film=%s\n", videoFacts.written > 0 ? (std::string(folder ? folder : "") + videoName).c_str() : "none");

		if (plan.recordVideo)
		{
			fprintf(file, "video_file=%s video_size=%dx%d video_frames=%ld video_dropped=%ld video_no_frame=%ld video_bytes=%lld video_stopped=%s video_grab_ms_average=%.2f video_grab_ms_worst=%.2f\n", videoName[0] ? videoName : "none", videoFacts.width, videoFacts.height, videoFacts.written, videoFacts.dropped, videoFacts.noPicture, videoFacts.bytes,
				videoFacts.stopped[0] ? videoFacts.stopped : "no", videoGrabs ? videoGrabMicroseconds / 1000.0 / videoGrabs : 0.0, videoGrabWorstMicroseconds / 1000.0);
		}

		fclose(file);
	}

	// A run that finished is no error, and the game's shutdown still has to write what's waiting. Anything else may be about to end without that shutdown, and an error goes to the disk at once, with
	// everything waiting ahead of it. So a log with "Error:" in it is a log with something to read.
	{
		char rate[96] = { 0 };

		// Asked for 0 means it asked for no cap
		_snprintf(rate, sizeof(rate) - 1, "hudson: frames each second: asked for %d, reached %.1f", plan.framesPerSecond, FramesEachSecondReached());
		LogString(rate);
	}

	if (code == kHudsonExitFinished)
	{
		LogString(std::string("hudson: ") + line);
	}
	else
	{
		LogErrorString(std::string("hudson: ") + line);
	}

	InterlockedExchange(&resultIsOnDisk, 1);
}

// Ends the run where it stands, with nothing more from the game. What's on the disk by now is all there is.
static void EndAtOnce(const char *howItEnded, int code)
{
	WriteResult(howItEnded, code);
	TerminateProcess(GetCurrentProcess(), (UINT)code);
}

// When frames stop with nothing on the log to say why, this says what Windows thinks the game's thread is doing. Holding a
// window by its title bar, or a menu on it, keeps that thread inside Windows, where no frame can start.
static const DWORD kFramesHaveStoppedAfterMilliseconds = 5000;
static DWORD gameThread = 0;
static LONG framesLastSeen = 0;
static DWORD framesLastMoved = 0;
static bool saidFramesStopped = false;

static void WatchForFramesStopping()
{
	LONG const frames = framesDone;
	DWORD const now = GetTickCount();

	if (frames != framesLastSeen)
	{
		if (saidFramesStopped)
		{
			char line[100] = { 0 };
			_snprintf(line, sizeof(line) - 1, "hudson: frames came back at frame %ld, after %lu seconds", frames, (now - framesLastMoved) / 1000);
			LogString(line);
		}

		framesLastSeen = frames;
		framesLastMoved = now;
		saidFramesStopped = false;
		return;
	}

	if (saidFramesStopped || frames == 0 || levelHasEnded || now - framesLastMoved < kFramesHaveStoppedAfterMilliseconds)
	{
		return;
	}

	saidFramesStopped = true;

	GUITHREADINFO state;
	memset(&state, 0, sizeof(state));
	state.cbSize = sizeof(state);
	bool const known = GetGUIThreadInfo(gameThread, &state) != FALSE;

	char line[300] = { 0 };
	_snprintf(line, sizeof(line) - 1, "hudson: no new frame for %lu seconds, at frame %ld. The game's thread: %s, moving or sizing its window %s, in a menu %s, its window minimised %s, the mouse held by its window %s",
		(unsigned long)(kFramesHaveStoppedAfterMilliseconds / 1000), frames, known ? "known" : "Windows wouldn't say",
		(state.flags & GUI_INMOVESIZE) ? "yes" : "no", (state.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE)) ? "yes" : "no",
		IsIconic(hWndMain) ? "yes" : "no", (state.hwndCapture == hWndMain) ? "yes" : "no");
	LogString(line);
}

static unsigned __stdcall WatchdogThread(void *)
{
	for (;;)
	{
		Sleep(kWatchdogLooksEveryMilliseconds);

		WatchForFramesStopping();

		DWORD touched = lastTouchAtStart;

		if (!levelHasEnded && LastTouch(&touched) && touched != lastTouchAtStart)
		{
			InterlockedExchange(&machineWasTouched, 1);

			// With machine=shared that's expected, and the result line says it happened
			if (!plan.machineShared)
			{
				EndAtOnce("someone touched the machine", kHudsonExitSomeoneTouchedIt);
			}
		}

		// this goes on through the shutdown, because that can get stuck too
		if (GetTickCount() - startedAt > (DWORD)plan.seconds * 1000)
		{
			EndAtOnce("it went past its time limit", kHudsonExitOutOfTime);
		}
	}
}

void Hudson_ReadCommandLine(const char *commandLine)
{
	if (!HudsonPlan_IsAskedFor(commandLine))
	{
		return;
	}

	// This has to be the first thing a run does. Everything after this point writes beside the game, starting with the log, and this copy of the game may be one a run must not touch. So it ends here with
	// nothing written at all, not even to say why, and the exit code is the only answer. A path too long to read gets the same answer, because nobody can say where it is.
	char exePath[MAX_PATH] = { 0 };
	DWORD const pathLength = GetModuleFileNameA(NULL, exePath, sizeof(exePath));

	// Reading the words writes nothing, and the folder's answer depends on one of them
	refusalFromReading = HudsonPlan_Read(commandLine, &plan);

	if (pathLength == 0 || pathLength >= sizeof(exePath) || !HudsonPlan_FolderIsAllowed(exePath, refusalFromReading, plan))
	{
		TerminateProcess(GetCurrentProcess(), (UINT)kHudsonExitWrongFolder);
	}

	// A crash, a missing disk or a file that won't open can bring up a box from Windows, which would wait for someone who isn't there. This tells Windows not to ask, so a crash ends the run with its own exit code.
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

	HudsonRunIsOn = true;
	HudsonBreaksPrisms = plan.breakPrisms;
	HudsonWayOfDying = plan.deathWay;
	startedAt = GetTickCount();
}

// The part of the first screen that isn't the main one left free by any taskbar, in the game's own numbers. False if there's only one screen. Defined further down.
static bool Hudson_FindTheSecondScreen(RECT *area);

int Hudson_CheckBeforeStarting()
{
	gameThread = GetCurrentThreadId();

	if (!HudsonRunIsOn)
	{
		return 0;
	}

	if (refusalFromReading)
	{
		WriteResult(refusalFromReading, kHudsonExitBadArguments);
		return kHudsonExitBadArguments;
	}

	// It only has to fit the main screen if that's where it goes. On the second screen it's made smaller to fit its part, and the game goes on drawing the size that was asked for.
	RECT secondScreen = { 0, 0, 0, 0 };

	if (plan.sizeAsked && !HudsonPlan_SizeFits(plan.width, plan.height, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)) && (plan.windowOnMainScreen || !Hudson_FindTheSecondScreen(&secondScreen)))
	{
		WriteResult("a window of the size= that was asked for wouldn't fit on the main screen", kHudsonExitBadArguments);
		return kHudsonExitBadArguments;
	}

	environment = BuiltInEnvironmentNamed(plan.level);

	if (environment < 0)
	{
		WriteResult("level= isn't the exact name of one of the game's own levels", kHudsonExitNoSuchLevel);
		return kHudsonExitNoSuchLevel;
	}

	// custommap=: a custom map made for single player, found the way the Custom maps page finds it, so that only a map the page would
	// list can be played. The menus load it in place of level=.
	if (plan.customMap[0])
	{
		if (plan.netMode != kHudsonNetNone)
		{
			WriteResult("custommap= is for single player, so it can't go with net=", kHudsonExitBadArguments);
			return kHudsonExitBadArguments;
		}

		CustomMaps_Find();
		customMapPlace = CustomMaps_Named(plan.customMap, sizeof(plan.customMap));

		if (customMapPlace < 0)
		{
			WriteResult("custommap= isn't one of the custom maps made for single player in avp_rifs/Custom", kHudsonExitNoSuchLevel);
			return kHudsonExitNoSuchLevel;
		}
	}

	// listplace= has to say the same level as level=, or the run's name and folder would be for the wrong one
	if (plan.listPlaceAsked)
	{
		int campaign = kCampaignNone;
		int campaignLevel = 0;

		if (!LevelList_CampaignAt(plan.listPlace, MAX_NO_OF_COOPERATIVE_EPISODES, &campaign, &campaignLevel)
			|| Campaign_Environment(campaign, campaignLevel) != environment)
		{
			WriteResult("listplace= isn't the place on the co-op list of the level that level= names", kHudsonExitNoSuchLevel);
			return kHudsonExitNoSuchLevel;
		}
	}

	// Only this machine's own address, and before anything has been opened on any other. See NetLoopbackOnly in networking.cpp.
	if (plan.netMode != kHudsonNetNone)
	{
		NetLoopbackOnly = true;
		NetLoopbackSearch = plan.typed;

		if (MultiplayerEpisodeForEnvironment(environment) < 0 && !plan.coop)
		{
			WriteResult("net= needs a level that's on the game's multiplayer list, and this one isn't", kHudsonExitNoSuchLevel);
			return kHudsonExitNoSuchLevel;
		}
	}

	// The game wants a marine's episode number whatever the level, for the briefing and its statistics. It's only used for those in a run, so on another player's level, or a multiplayer one, it's the first.
	episode = MarineEpisodeNamed(plan.level);

	if (episode < 0)
	{
		episode = 0;
	}

	bool const touchKnown = LastTouch(&lastTouchAtStart);

	// With machine=shared, someone being at the machine is the whole idea, so it doesn't wait for them to leave
	if (!plan.machineShared && (!touchKnown || GetTickCount() - lastTouchAtStart < (DWORD)plan.idleSeconds * 1000))
	{
		// How long ago goes in the result: a machine touched five seconds ago and one touched a minute ago ask for different answers
		char tooRecently[100] = { 0 };

		_snprintf(tooRecently, sizeof(tooRecently) - 1, "someone has touched the machine too recently, %lu seconds ago", (unsigned long)((GetTickCount() - lastTouchAtStart) / 1000));
		WriteResult(tooRecently, kHudsonExitMachineInUse);
		return kHudsonExitMachineInUse;
	}

	mouseAtStartIsKnown = GetCursorPos(&mouseAtStart) != 0;

	// So that the last run's result can't be taken for this one's, however this one ends. A run that ends properly writes over this.
	char const *const resultFolder = GetSaveFolderPath();
	FILE *const startingFile = fopen((std::string(resultFolder ? resultFolder : "") + kResultFileName).c_str(), "w");

	if (startingFile)
	{
		fprintf(startingFile, "commit=%s level=%s ended=it hasn't ended: it's running, or it stopped with no chance to say why\n", AVP_BUILD_COMMIT, plan.level);
		fclose(startingFile);
	}

	// A gun run with something to hit needs the alien's model
	if (plan.gunAlien)
	{
		ForceLoad_Alien = TRUE;
		HudsonPeopleStandStill = true;
	}

	// And one with a marine to look at needs the marine's
	if (plan.gunMarine)
	{
		ForceLoad_Marine = TRUE;
		HudsonPeopleStandStill = true;
	}

	// ratetest=flame burns a marine who stands still, so that only the flame changes from one frame rate to the next
	if (plan.rateTest == kHudsonRateTestFlame)
	{
		ForceLoad_Marine = TRUE;
		HudsonPeopleStandStill = true;
	}

	// A siege makes all of these, and a network game only loads the first three by itself. Both machines need the models.
	if (plan.siegeRoom[0])
	{
		ForceLoad_Alien = TRUE;
		ForceLoad_Marine = TRUE;
		ForceLoad_Predator = TRUE;
		ForceLoad_Xenoborg = TRUE;
		ForceLoad_Hugger = TRUE;
		ForceLoad_SentryGun = TRUE;
	}

	// A fight in plain single player has to load what it's going to make. A network game that plays like single player already loads every kind.
	if (plan.botSingle)
	{
		ForceLoad_Predator = ForceLoad_Predator || plan.botFight == kHudsonBotFightPredator || plan.botFight == kHudsonBotFightPlasma;
		ForceLoad_Xenoborg = ForceLoad_Xenoborg || plan.botFight == kHudsonBotFightXenoborg;
		ForceLoad_Marine = ForceLoad_Marine || plan.botFight == kHudsonBotFightMarine || plan.botFight == kHudsonBotFightFlame;
		ForceLoad_Alien = ForceLoad_Alien || plan.botFight == kHudsonBotFightAlien;
	}

	// Before the level loads, because this decides which models it loads. A marine level needn't have any marines in it. As an Observer the player has no gun on the screen, and nothing aims at him.
	if (plan.rooms > 0)
	{
		HudsonMarineIsOn = true;
		ForceLoad_Marine = TRUE;
		Observer = 1;

		if (plan.combatAlien)
		{
			HudsonWorldIsLive = true;
			ForceLoad_Alien = TRUE;
		}
	}

	char line[256] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: a run starts: level %s, %d frames, at most %d seconds. Nobody has touched the machine for %lu seconds", plan.level, plan.frames, plan.seconds,
		(unsigned long)((GetTickCount() - lastTouchAtStart) / 1000));
	LogString(line);

	// It's never waited for: the game's normal way out ends it with the process, and until then it has to be able to stop anything.
	uintptr_t const thread = _beginthreadex(NULL, 0, WatchdogThread, NULL, 0, NULL);

	if (thread)
	{
		CloseHandle((HANDLE)thread);
	}
	else
	{
		LogErrorString("hudson: the watchdog couldn't start, so nothing would stop this run if it got stuck, or if someone needed the machine");
		WriteResult("the watchdog couldn't start", kHudsonExitOutOfTime);
		return kHudsonExitOutOfTime;
	}

	return 0;
}

bool Hudson_RunFolder(char *folder, size_t size)
{
	char exePath[MAX_PATH] = { 0 };
	DWORD const pathLength = GetModuleFileNameA(NULL, exePath, sizeof(exePath));

	if (pathLength == 0 || pathLength >= sizeof(exePath))
	{
		return false;
	}

	char *const lastSlash = strrchr(exePath, '\\');

	if (!lastSlash)
	{
		return false;
	}

	*lastSlash = 0;

	std::string const path = std::string(exePath) + "\\" + kRunFolderName;

	if (path.size() + 1 > size)
	{
		return false;
	}

	if (!CreateDirectoryA(path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		return false;
	}

	memcpy(folder, path.c_str(), path.size() + 1);
	return true;
}

// Setting one of these only changes a value that's already there, and in a new folder none is yet: the first run would get the game's own defaults, and the second run these, so the two wouldn't be the same. Asking
// for a value that's missing puts it there, so each is asked for first. Then it's set, because the file may already hold something else.
void Hudson_SayWhichShaderWasCompiled(const char *path)
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	FILE *const file = fopen(path, "rb");
	std::vector<uint8_t> bytes;

	if (file)
	{
		uint8_t buffer[4096];
		size_t got;

		while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
		{
			bytes.insert(bytes.end(), buffer, buffer + got);
		}

		fclose(file);
	}

	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: compiled %s, %lu bytes, check %08X", path, (unsigned long)bytes.size(), HudsonVision_Check(bytes.empty() ? 0 : &bytes[0], bytes.size()));
	LogString(line);
}

bool Hudson_StaysOutOfTheWay()
{
	return HudsonRunIsOn && !plan.showWindow;
}

// With show=yes, someone wants to see this window. Windows can get in the way of that in three ways, so each is dealt with. The first ShowWindow of a program can be ignored, if whatever started it, like Steam,
// said how to show it, so it's asked again. It can refuse to let a program come to the front, but not to put its window above the others for a moment. And a window can end up on a monitor that's gone.
void Hudson_BringTheWindowForward()
{
	if (!HudsonRunIsOn || !plan.showWindow || !hWndMain)
	{
		return;
	}

	ShowWindow(hWndMain, IsIconic(hWndMain) ? SW_RESTORE : SW_SHOW);

	if (!MonitorFromWindow(hWndMain, MONITOR_DEFAULTTONULL))
	{
		SetWindowPos(hWndMain, 0, 50, 50, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
	}

	SetWindowPos(hWndMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetWindowPos(hWndMain, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetForegroundWindow(hWndMain);
}

// With show=yes, says where the window really is, in a file of its own that's written straight away, so that someone who can't see the window can look while the run is going. The log only reaches the disk later.
static void SayWhereTheWindowIs(LONG frame)
{
	RECT place = { 0, 0, 0, 0 };
	MONITORINFOEXA monitor;

	GetWindowRect(hWndMain, &place);
	memset(&monitor, 0, sizeof(monitor));
	monitor.cbSize = sizeof(monitor);

	HMONITOR const onMonitor = MonitorFromWindow(hWndMain, MONITOR_DEFAULTTONULL);
	bool const monitorKnown = onMonitor && GetMonitorInfoA(onMonitor, &monitor);
	char line[300] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: the window at frame %ld: visible=%s minimised=%s in_front=%s left=%ld top=%ld right=%ld bottom=%ld monitor=%s monitor_left=%ld monitor_top=%ld monitor_right=%ld monitor_bottom=%ld",
		frame, IsWindowVisible(hWndMain) ? "yes" : "no", IsIconic(hWndMain) ? "yes" : "no", GetForegroundWindow() == hWndMain ? "yes" : "no", place.left, place.top, place.right, place.bottom,
		monitorKnown ? monitor.szDevice : "none", monitor.rcMonitor.left, monitor.rcMonitor.top, monitor.rcMonitor.right, monitor.rcMonitor.bottom);
	LogString(line);

	char const *const folder = GetSaveFolderPath();
	FILE *const file = fopen((std::string(folder ? folder : "") + kWindowFileName).c_str(), frame == 1 ? "w" : "a");

	if (file)
	{
		fprintf(file, "%s\n", line);
		fclose(file);
	}
}

// Notes the first screen that isn't the main one, and stops looking: the part of it left free by any taskbar.
static BOOL CALLBACK Hudson_NoteASecondScreen(HMONITOR monitor, HDC, LPRECT, LPARAM found)
{
	MONITORINFO info;

	memset(&info, 0, sizeof(info));
	info.cbSize = sizeof(info);

	if (GetMonitorInfoA(monitor, &info) && !(info.dwFlags & MONITORINFOF_PRIMARY))
	{
		*(RECT *)found = info.rcWork;
		return FALSE;
	}

	return TRUE;
}

static bool Hudson_FindTheSecondScreen(RECT *area)
{
	area->left = area->top = area->right = area->bottom = 0;
	EnumDisplayMonitors(NULL, NULL, Hudson_NoteASecondScreen, (LPARAM)area);
	return area->right > area->left && area->bottom > area->top;
}

bool Hudson_PlaceTheWindow(int *left, int *top, int *width, int *height)
{
	if (!HudsonRunIsOn || plan.windowOnMainScreen)
	{
		return false;
	}

	RECT area = { 0, 0, 0, 0 };

	if (!Hudson_FindTheSecondScreen(&area))
	{
		return false;
	}

	// One of two with the big part: its part of the screen is worked out first, and then the window is fitted into it as if it were the only one there
	if (plan.windowBig >= 0 && plan.windowsTogether == 2)
	{
		int partLeft = 0, partTop = 0, partWidth = 0, partHeight = 0;

		HudsonPlan_WindowPart(area.left, area.top, area.right - area.left, area.bottom - area.top, plan.windowSlot == plan.windowBig, &partLeft, &partTop, &partWidth, &partHeight);
		HudsonPlan_WindowPlace(partLeft, partTop, partWidth, partHeight, 0, 1, left, top, width, height);
		return true;
	}

	HudsonPlan_WindowPlace(area.left, area.top, area.right - area.left, area.bottom - area.top, plan.windowSlot, plan.windowsTogether, left, top, width, height);
	return true;
}

void Hudson_SayWhereTheWindowWent()
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	RECT place = { 0, 0, 0, 0 };
	MONITORINFOEXA monitor;

	GetWindowRect(hWndMain, &place);
	memset(&monitor, 0, sizeof(monitor));
	monitor.cbSize = sizeof(monitor);

	HMONITOR const onMonitor = MonitorFromWindow(hWndMain, MONITOR_DEFAULTTONULL);
	bool const known = onMonitor && GetMonitorInfoA(onMonitor, &monitor);
	bool const onTheMainOne = known && (monitor.dwFlags & MONITORINFOF_PRIMARY) != 0;
	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: the window is at %ld %ld, %ld x %ld, on %s, %s.%s", place.left, place.top, place.right - place.left, place.bottom - place.top, known ? monitor.szDevice : "no screen",
		onTheMainOne ? "the main screen" : "not the main screen", (onTheMainOne && !plan.windowOnMainScreen) ? " There's no second screen, so it stays there." : "");
	LogString(line);
}

void Hudson_SetVideoSettings()
{
	// With no second screen, or with screen=main, the window goes in the top left of the main screen, so it has to fit there. On a second screen it's made smaller if it has to be. A size somebody asked for and doesn't fit is refused in Hudson_CheckBeforeStarting. One nobody asked for just gives way.
	bool const fits = HudsonPlan_SizeFits(plan.width, plan.height, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
	int const width = (fits || plan.sizeAsked) ? plan.width : kHudsonDefaultWidth;
	int const height = (fits || plan.sizeAsked) ? plan.height : kHudsonDefaultHeight;

	Config_GetInt("[VideoMode]", "Width", width);
	Config_SetInt("[VideoMode]", "Width", width);
	Config_GetInt("[VideoMode]", "Height", height);
	Config_SetInt("[VideoMode]", "Height", height);
	// A run that measures time can't wait for the screen. One sharing the machine has no timings to protect, and it may be being watched or recorded, so it draws no more than the screen can show. A walk
	// is for someone to watch, so it runs the way a player's game does too.
	bool const waitsForTheScreen = (plan.machineShared || plan.navModules > 0) && !plan.vsyncOff;

	Config_GetBool("[VideoMode]", "UseVSync", waitsForTheScreen);
	Config_SetBool("[VideoMode]", "UseVSync", waitsForTheScreen);
}

int Hudson_MarineEpisode()
{
	return episode;
}

int Hudson_Environment()
{
	return environment;
}

int Hudson_CustomMapPlace()
{
	return HudsonRunIsOn ? customMapPlace : -1;
}

bool Hudson_PlayAsAlien()
{
	return plan.playAsAlien;
}

int Hudson_ListPlace()
{
	return plan.listPlaceAsked ? plan.listPlace : -1;
}

bool Hudson_PlayAsPredator()
{
	return plan.playAsPredator;
}

int Hudson_ClassSlider()
{
	return plan.classSlider;
}

// The game's own way out of a level, so that its shutdown gets tested as well.
static void EndTheLevel(const char *howItEnded, int code)
{
	InterlockedExchange(&levelHasEnded, 1);
	exitCode = code;
	WriteResult(howItEnded, code);
	AvP.MainLoopRunning = 0;
}

extern void GiveAllWeaponsCheat(void);
extern float CameraZoomScale;
extern HMODELCONTROLLER PlayersWeaponHModelController;

// The same frame before and after the painted lamp, every couple of seconds of a gun test with pictures
static bool lampCompareIsDue = false;
static int lampCompareNumber = 0;

bool Hudson_LampCompareIsDue()
{
	bool const due = lampCompareIsDue;

	lampCompareIsDue = false;

	return HudsonRunIsOn && due;
}

void Hudson_SaveLampCompareScreen(const char *stage)
{
	char const *const folder = GetSaveFolderPath();
	std::string const pictures = std::string(folder ? folder : "") + "pictures\\";
	char name[48] = { 0 };

	CreateDirectoryA(pictures.c_str(), NULL);
	_snprintf(name, sizeof(name) - 1, "lamp-%02d-%s.png", lampCompareNumber, stage);

	LPDIRECT3DDEVICE9 const device = d3d.lpD3DDevice;
	IDirect3DSurface9 *screen = 0;
	IDirect3DSurface9 *copy = 0;
	D3DSURFACE_DESC size;
	bool saved = false;

	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &screen)))
	{
		if (SUCCEEDED(screen->GetDesc(&size)) && SUCCEEDED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
		{
			saved = SUCCEEDED(device->GetRenderTargetData(screen, copy)) && SUCCEEDED(D3DXSaveSurfaceToFileA((pictures + name).c_str(), D3DXIFF_PNG, copy, NULL, NULL));
			copy->Release();
		}

		screen->Release();
	}

	if (!saved)
	{
		LogErrorString(std::string("hudson: couldn't save ") + name);
	}
}

void Hudson_SaveLampCompareSurface(struct IDirect3DTexture9 *surface)
{
	char const *const folder = GetSaveFolderPath();
	std::string const pictures = std::string(folder ? folder : "") + "pictures\\";
	char name[48] = { 0 };

	_snprintf(name, sizeof(name) - 1, "lamp-%02d-surface.png", lampCompareNumber);

	if (!surface)
	{
		LogString(std::string("hudson: no surface picture this frame, for ") + name);
	}
	else if (FAILED(D3DXSaveTextureToFileA((pictures + name).c_str(), D3DXIFF_PNG, surface, NULL)))
	{
		LogErrorString(std::string("hudson: couldn't save ") + name);
	}
}

static STRATEGYBLOCK *TheAlienJustMadeAt(const VECTORCH &spot);

// Which state's picture is being taken, for its name
static int gunPictureState = 0;
static int gunPictureSlot = 0;

// Seconds from the level's first frame. He's given the guns and chooses this one. He holds the trigger until it's empty, and on an empty gun that only clicks, as it does for any gun, so he lets go and it starts to load. He presses again part way through, which stops the loading, and holds it through a few rounds of that. Then he lets go, and it loads right up. Then secondary fire: he holds it for a while, fires once while still holding it, lets go, and taps it twice, which is enough to raise and lower a scope or move a stock. A gun with no secondary fire ignores all that.
enum
{
	kGunGiveAtSecond = 2,
	kGunChooseAtSecond = 3,
	kGunFireFromSecond = 9,
	kGunFireUntilSecond = 24,
	kGunFireAgainFromSecond = 26,
	kGunFireAgainUntilSecond = 34,
	kGunSecondaryFromSecond = 44,
	kGunSecondaryUntilSecond = 48,
	kGunFireWithSecondaryAtSecond = 47,
	kGunFirstTapAtSecond = 50,
	kGunSecondTapAtSecond = 53,
	kGunThirdTapAtSecond = 58,
	kGunFourthTapAtSecond = 63,
	kGunPartFullShotAtSecond = 40,
	kGunPartFullReloadKeyAtSecond = 42,
	kGunReloadKeyAtSecond = 68,
	kGunReloadPictureEveryTicks = 200,
	kGunReloadPicturesFrom = 200,

	// In a level with a mirror: he turns slowly between these seconds, with a picture every so often, numbered from here
	kGunMirrorTurnFromSecond = 5,
	kGunMirrorTurnUntilSecond = 9,
	kGunMirrorPicturesFrom = 300,
	kGunMirrorPictureEveryMilliseconds = 250,

	// gunmarine=yes: he's made a second after the gun is chosen, 3 metres away, with a picture each second, numbered from here
	kGunMarineAtSecond = 4,
	kGunMarineMetres = 3000,
	kGunMarinePicturesFrom = 400,
	kGunReloadPicturesMost = 30,

	// The M79 alone: a shot, then a tap a second later with the gun still empty, so that changing kind has to load it
	kGunM79Slot = 13,
	kGunM79EmptyShotAtSecond = 71,
	kGunM79EmptyTapAtSecond = 72,

	// The staff alone: where he looks before each throw, so that they aren't all one case. Looking up is below 0.
	kGunStaffSlot = 7,
	kGunAimUpALittle = 4096 - 170,
	kGunAimUp = 4096 - 340,
	kGunAimDown = 340,
	kGunAimAtTheFloor = 680,

	kGunEndAtSecond = 75,

	// The revolver alone goes on after that: see Hudson_GunTriesRevolverReloads
	kGunRevolverSlot = 12,
	kGunRevolverHolds = 5,
	kGunRevolverEndAtSecond = 130,
	kGunRevolverSparesForTheLastSteps = 30,

	// gun=15 gives each slot this long, and fires in the last second of it
	kGunEverySlot = 15,
	kGunSlotsInTurn = 14,
	kGunSecondsOnEachSlot = 4,

	// And after all the slots, it empties the pulse rifle and presses Next Weapon and Previous Weapon, each time the gun has been idle this long. The log's gun: lines say where each
	// press lands. That's enough presses to go right round the list and a bit further, and then some back. A pair is a press and another while the gun is still changing, which goes
	// through different code. If something stops the presses, it stops anyway before a run's usual two minutes are up.
	kGunChangeFromSecond = kGunChooseAtSecond + kGunSlotsInTurn * kGunSecondsOnEachSlot,
	kGunChangeLatestEndSecond = 115,
	kGunChangeWaitMilliseconds = 300,
	kGunNextPresses = 15,
	kGunPreviousPresses = 6,
	kGunPressPairs = 2,

	kGunLampCompareEveryTicks = 2000,
	kGunLampComparesMost = 40,

	kGunPictureAfterTicks = 150,
	kGunMostStatesWithPictures = 32,

	// A counter on a gun changes with every shot, all within one state, so firing also takes a picture this often. They're
	// numbered from here, in the same names as a state's pictures, so that they can't overwrite one.
	kGunFiringPictureEveryTicks = 3000,
	kGunFiringPicturesFrom = 100,

	// The sniper rifle's scope works by a press and a hold, which the other guns' steps can't tell apart, and it can't fire while
	// secondary's held. So after its first magazines it has steps of its own, in milliseconds. It ends later than the other guns.
	kGunSniperSlot = 11,
	kSniperHoldToFullZoomFrom = 44000,
	kSniperHoldToFullZoomUntil = 47000,
	kSniperPictureAtFullZoom = 47500,
	kSniperShootThroughScopeFrom = 48000,
	kSniperShootThroughScopeUntil = 49000,
	kSniperTapToTakeItDown = 51000,
	kSniperTapToBringItUpAtNoZoom = 53000,
	kSniperPictureAtNoZoom = 54000,
	kSniperTapToTakeItDownAgain = 55000,
	kSniperHoldAgainFrom = 57000,
	kSniperHoldAgainUntil = 59000,
	kSniperShotsFrom = 60000,
	kSniperShotsUntil = 80000,
	kSniperShotEvery = 3000,
	kSniperShotLasts = 500,
	kSniperLastTap = 82000,
	kSniperEndAtSecond = 84,

	// Pictures named for these instead of a state, because each of those views is the rifle lying idle
	kSniperPictureNumberAtFullZoom = 30,
	kSniperPictureNumberAtNoZoom = 31
};

// gun=15's last step. Only the pulse rifle's ammunition is touched, and everything after that is the common code answering the same requests as his keys. It gives back true when it's finished.
static bool Hudson_GunTriesChangingWeapon(PLAYER_STATUS *status, DWORD now)
{
	static bool emptied = false;
	static bool pressAgainWhileChanging = false;
	static bool letGoOfTheKey = false;
	static int presses = 0;
	static DWORD idleSince = 0;

	if (!emptied)
	{
		emptied = true;

		for (int slot = 0; slot < MAX_NO_OF_WEAPON_SLOTS; slot++)
		{
			PLAYER_WEAPON_DATA *const weapon = &status->WeaponSlot[slot];

			if (weapon->WeaponIDNumber == WEAPON_PULSERIFLE)
			{
				weapon->PrimaryRoundsRemaining = 0;
				weapon->PrimaryMagazinesRemaining = 0;
				weapon->SecondaryRoundsRemaining = 0;
				weapon->SecondaryMagazinesRemaining = 0;
			}
		}

		LogString("gun: emptied the pulse rifle, to try Next Weapon and Previous Weapon");
	}

	bool const gunIsIdle = status->WeaponSlot[status->SelectedWeaponSlot].CurrentState == WEAPONSTATE_IDLE;

	if (pressAgainWhileChanging)
	{
		// The game takes a press with no frame of the key being up as the same press held down, and ignores it. So one frame goes by with nothing pressed, as it does for a player's finger.
		if (!gunIsIdle)
		{
			if (letGoOfTheKey)
			{
				letGoOfTheKey = false;
				pressAgainWhileChanging = false;
				status->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
				LogString("gun: pressed Next Weapon again while changing");
			}
			else
			{
				letGoOfTheKey = true;
			}
		}

		idleSince = now;
		return false;
	}

	if (!gunIsIdle)
	{
		idleSince = now;
		return false;
	}

	if (idleSince == 0)
	{
		idleSince = now;
	}

	if (now - idleSince < kGunChangeWaitMilliseconds)
	{
		return false;
	}

	idleSince = now;

	if (presses >= kGunNextPresses + kGunPreviousPresses + kGunPressPairs)
	{
		return true;
	}

	if (presses < kGunNextPresses)
	{
		status->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
		LogString("gun: pressed Next Weapon");
	}
	else if (presses < kGunNextPresses + kGunPreviousPresses)
	{
		status->Mvt_InputRequests.Flags.Rqst_PreviousWeapon = 1;
		LogString("gun: pressed Previous Weapon");
	}
	else
	{
		status->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
		pressAgainWhileChanging = true;
		LogString("gun: pressed Next Weapon, and will press it again while changing");
	}

	presses++;
	return false;
}

// True for one frame, the first one at or after this moment. A press has to be let go again before the scope has finished rising,
// which takes a tenth of a second, and one frame is the only length that's safe however slowly the game is running.
static bool Hudson_OneFrameAt(DWORD now, DWORD moment, bool *done)
{
	if (*done || now < moment)
	{
		return false;
	}

	*done = true;
	return true;
}

// Holds the scope's key down, and says yes once the scope has risen and the gun has come to rest with it up.
static bool Hudson_GunHoldsScopeUp(PLAYER_STATUS *status, int state)
{
	static bool hasRisen = false;

	status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;

	if (state == kRevolverState_ScopeRises)
	{
		hasRisen = true;
	}

	if (hasRisen && state == WEAPONSTATE_IDLE)
	{
		hasRisen = false;
		return true;
	}

	return false;
}

// The revolver's reloads and gun changes that the timed steps never reach. A player gets to all of them, so each has to have happened once in the real game:
//   the reload key on a full gun, which should do nothing;
//   the key on a gun with nothing spent in it, so there are no cases to throw out and it only opens. The way to get a gun like that is to stop a reload part way;
//   too few spare rounds to fill the gun, so the loading ends early;
//   the reload key with the scope up, which brings the scope down first. Once with the scope's key still held, which stops the loading after one round and puts the scope back up, and once with it let go;
//   another gun with the scope's key held, which brings the scope down and changes gun, and then another gun with the scope already down;
//   another gun while it loads, which waits for the gun to close, and then a reload when the revolver comes back, to show the first one left nothing behind;
//   a gun emptied with the scope's key held, which shouldn't reload by itself until the key is let go.
// Each step waits for the gun to come to rest instead of watching a clock, because the pictures this test saves can hold up a frame for over a second.
// Each one writes a line to the log when it happens, so a step that never happened is a line missing from the log.
static void Hudson_GunTriesRevolverReloads(PLAYER_STATUS *status, DWORD seconds)
{
	enum
	{
		kPressOnAFullGun,
		kSeeNothingHappen,
		kFireOneThenReload,
		kStopTheLoadingEarly,
		kReloadWithNothingSpent,
		kWaitUntilFullAgain,
		kFireThreeThenReload,
		kGiveTheSparesBack,
		kReloadHoldingTheScopeUp,
		kKeepHoldingUntilScopeIsBackUp,
		kLetTheScopeComeDown,
		kReloadThenLetGoOfTheScope,
		kWaitUntilFullBeforeChanging,
		kPressNextWithTheScopeUp,
		kKeepHoldingUntilGunHasGone,
		kChangeGunWithTheScopeDown,
		kWaitUntilRevolverIsBack,
		kFireTwoThenReload,
		kChangeGunWhileLoading,
		kWaitUntilGunHasGone,
		kReloadOnceBack,
		kWaitUntilFullBeforeEmptying,
		kScopeUpToEmptyIt,
		kEmptyItHoldingTheScope,
		kKeepHoldingOnAnEmptyGun,
		kAllDone
	};

	static int step = kPressOnAFullGun;
	static int stepOnceBack = kAllDone;
	static DWORD pressedAtSecond = 0;

	PLAYER_WEAPON_DATA *const weaponPtr = &status->WeaponSlot[status->SelectedWeaponSlot];
	int const state = (int)weaponPtr->CurrentState;
	bool const atRest = state == WEAPONSTATE_IDLE;
	int const rounds = weaponPtr->PrimaryRoundsRemaining >> ONE_FIXED_SHIFT;
	bool const onTheRevolver = (int)status->SelectedWeaponSlot == kGunRevolverSlot - 1;

	switch (step)
	{
		case kPressOnAFullGun:
			if (atRest && rounds == kGunRevolverHolds)
			{
				LogString("gun: pressed the reload key on a full gun");
				Weapon_ReloadKeyPressed(status);
				pressedAtSecond = seconds;
				step = kSeeNothingHappen;
			}
			break;

		// Long enough for the gun to start a reload if it was going to, and the log would show it. The count goes up in whole seconds, so 2 is at least one full second.
		case kSeeNothingHappen:
			if (seconds >= pressedAtSecond + 2)
			{
				step = kFireOneThenReload;
			}
			break;

		// The game fires after this function has run, so the trigger is let go as soon as the round has gone, and that makes one shot
		case kFireOneThenReload:
			if (rounds == kGunRevolverHolds)
			{
				status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
			}
			else if (atRest)
			{
				LogString("gun: pressed the reload key with one case spent");
				Weapon_ReloadKeyPressed(status);
				step = kStopTheLoadingEarly;
			}
			break;

		// A touch of the scope's key stops the loading after the round going in. It's let go at once, so the scope itself doesn't come up.
		case kStopTheLoadingEarly:
			if (state == kRevolverState_LoadRound && rounds >= 2)
			{
				LogString("gun: touched the scope's key to stop the loading");
				status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
				step = kReloadWithNothingSpent;
			}
			break;

		case kReloadWithNothingSpent:
			if (atRest)
			{
				LogString("gun: pressed the reload key with nothing spent");
				Weapon_ReloadKeyPressed(status);
				step = kWaitUntilFullAgain;
			}
			break;

		case kWaitUntilFullAgain:
			if (atRest && rounds == kGunRevolverHolds)
			{
				step = kFireThreeThenReload;
			}
			break;

		// Three cases and two live rounds in the gun, and one spare: the two go back with the spare, so three rounds are all there are to load
		case kFireThreeThenReload:
			if (rounds > kGunRevolverHolds - 3)
			{
				status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
			}
			else if (atRest)
			{
				LogString("gun: left one spare round and pressed the reload key with three cases spent");
				weaponPtr->PrimaryMagazinesRemaining = 1;
				Weapon_ReloadKeyPressed(status);
				step = kGiveTheSparesBack;
			}
			break;

		// The last reload used up the spares, and the reload key does nothing without any
		case kGiveTheSparesBack:
			if (atRest)
			{
				weaponPtr->PrimaryMagazinesRemaining = kGunRevolverSparesForTheLastSteps;
				step = kReloadHoldingTheScopeUp;
			}
			break;

		case kReloadHoldingTheScopeUp:
			if (Hudson_GunHoldsScopeUp(status, state))
			{
				LogString("gun: pressed the reload key with the scope up, still holding the scope's key");
				Weapon_ReloadKeyPressed(status);
				step = kKeepHoldingUntilScopeIsBackUp;
			}
			break;

		case kKeepHoldingUntilScopeIsBackUp:
			if (Hudson_GunHoldsScopeUp(status, state))
			{
				step = kLetTheScopeComeDown;
			}
			break;

		// Nothing is held here, and letting go is what brings it down
		case kLetTheScopeComeDown:
			if (state == kRevolverState_ScopeLastBitDown)
			{
				step = kReloadThenLetGoOfTheScope;
			}
			break;

		// The key is held for this one frame and not after it, so the loading isn't stopped this time
		case kReloadThenLetGoOfTheScope:
			if (Hudson_GunHoldsScopeUp(status, state))
			{
				LogString("gun: pressed the reload key with the scope up, then let go of the scope's key");
				Weapon_ReloadKeyPressed(status);
				step = kWaitUntilFullBeforeChanging;
			}
			break;

		case kWaitUntilFullBeforeChanging:
			if (atRest && rounds == kGunRevolverHolds)
			{
				step = kPressNextWithTheScopeUp;
			}
			break;

		case kPressNextWithTheScopeUp:
			if (Hudson_GunHoldsScopeUp(status, state))
			{
				LogString("gun: pressed Next Weapon with the scope up, still holding the scope's key");
				status->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
				stepOnceBack = kChangeGunWithTheScopeDown;
				step = kKeepHoldingUntilGunHasGone;
			}
			break;

		// A player's finger stays where it is, so the key is held all the way through
		case kKeepHoldingUntilGunHasGone:
			if (onTheRevolver)
			{
				status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
			}
			else
			{
				step = kWaitUntilRevolverIsBack;
			}
			break;

		case kChangeGunWithTheScopeDown:
			if (atRest)
			{
				LogString("gun: pressed Next Weapon with the scope down");
				status->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
				stepOnceBack = kFireTwoThenReload;
				step = kWaitUntilGunHasGone;
			}
			break;

		// The test asks for its own gun back by itself, every frame, as soon as it's on another
		case kWaitUntilRevolverIsBack:
			if (onTheRevolver && atRest)
			{
				step = stepOnceBack;
			}
			break;

		case kFireTwoThenReload:
			if (rounds > kGunRevolverHolds - 2)
			{
				status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
			}
			else if (atRest)
			{
				LogString("gun: pressed the reload key with two cases spent");
				Weapon_ReloadKeyPressed(status);
				step = kChangeGunWhileLoading;
			}
			break;

		case kChangeGunWhileLoading:
			if (state == kRevolverState_LoadRound && rounds >= 1)
			{
				LogString("gun: pressed Next Weapon while it loads");
				status->Mvt_InputRequests.Flags.Rqst_NextWeapon = 1;
				stepOnceBack = kReloadOnceBack;
				step = kWaitUntilGunHasGone;
			}
			break;

		case kWaitUntilGunHasGone:
			if (!onTheRevolver)
			{
				step = kWaitUntilRevolverIsBack;
			}
			break;

		case kReloadOnceBack:
			LogString("gun: pressed the reload key after coming back to the revolver");
			Weapon_ReloadKeyPressed(status);
			step = kWaitUntilFullBeforeEmptying;
			break;

		case kWaitUntilFullBeforeEmptying:
			if (atRest && rounds == kGunRevolverHolds)
			{
				step = kScopeUpToEmptyIt;
			}
			break;

		case kScopeUpToEmptyIt:
			if (Hudson_GunHoldsScopeUp(status, state))
			{
				step = kEmptyItHoldingTheScope;
			}
			break;

		case kEmptyItHoldingTheScope:
			status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;

			if (rounds > 0)
			{
				status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
			}
			else if (atRest)
			{
				LogString("gun: emptied it with the scope's key still held");
				pressedAtSecond = seconds;
				step = kKeepHoldingOnAnEmptyGun;
			}
			break;

		// As on the full gun: long enough for a reload to start if it was going to, and the log would show it. Then the key is let go.
		case kKeepHoldingOnAnEmptyGun:
			status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;

			if (seconds >= pressedAtSecond + 2)
			{
				LogString("gun: let go of the scope's key on an empty gun");
				step = kAllDone;
			}
			break;

		default:
			break;
	}
}

void Hudson_GunInput(void *playerStatus)
{
	if (!HudsonRunIsOn || plan.gunSlot == 0 || levelHasEnded)
	{
		return;
	}

	static DWORD started = 0;
	static bool gaveThem = false;
	static int lastState = -1, lastRounds = -1, lastSpares = -1, lastSlot = -1;
	static DWORD stateStarted = 0;
	static bool pictureTaken[kGunMostStatesWithPictures] = { false };

	PLAYER_STATUS *const status = (PLAYER_STATUS *)playerStatus;
	DWORD const now = GetTickCount();

	if (started == 0)
	{
		started = now;
	}

	DWORD const seconds = (now - started) / 1000;
	bool const everySlot = plan.gunSlot == kGunEverySlot;
	DWORD const slotsGoneBy = seconds >= kGunChooseAtSecond ? (seconds - kGunChooseAtSecond) / kGunSecondsOnEachSlot : 0;
	int const wantedSlot = everySlot ? (slotsGoneBy < kGunSlotsInTurn ? (int)slotsGoneBy + 1 : kGunSlotsInTurn) : plan.gunSlot;
	bool const triesChanging = everySlot && seconds >= kGunChangeFromSecond;
	bool const finishedChanging = triesChanging && Hudson_GunTriesChangingWeapon(status, now);

	if (!gaveThem && seconds >= kGunGiveAtSecond)
	{
		gaveThem = true;

		if (plan.classSlider)
		{
			// A class keeps what it started with, so the run shows that rather than giving him everything.
			char line[96];

			_snprintf(line, sizeof(line) - 1, "class: he is class %d, in a %s game", (int)netGameData.myCharacterSubType, netGameData.skirmishMode ? "skirmish" : "another kind of");
			line[sizeof(line) - 1] = 0;
			LogString(line);

			for (int slot = 0; slot < MAX_NO_OF_WEAPON_SLOTS; slot++)
			{
				PLAYER_WEAPON_DATA const *const weapon = &status->WeaponSlot[slot];

				if (weapon->Possessed == 1)
				{
					_snprintf(line, sizeof(line) - 1, "class: slot %d weapon %d rounds %d spares %d", slot, (int)weapon->WeaponIDNumber, weapon->PrimaryRoundsRemaining >> ONE_FIXED_SHIFT, weapon->PrimaryMagazinesRemaining);
					line[sizeof(line) - 1] = 0;
					LogString(line);
				}
			}
		}
		else
		{
			GiveAllWeaponsCheat();
			LogString("gun: gave him all the weapons, the way GIVEALLWEAPONS does");
		}
	}

	// Asked for every frame until it happens, because the game only listens while a gun is idle. An empty slot never happens, and the log shows that.
	if (!triesChanging && seconds >= kGunChooseAtSecond && (int)status->SelectedWeaponSlot != wantedSlot - 1)
	{
		status->Mvt_InputRequests.Flags.Rqst_WeaponNo = wantedSlot;
	}

	if (everySlot && !triesChanging && seconds >= kGunChooseAtSecond && (seconds - kGunChooseAtSecond) % kGunSecondsOnEachSlot == kGunSecondsOnEachSlot - 1)
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
	}

	if (!everySlot && ((seconds >= kGunFireFromSecond && seconds < kGunFireUntilSecond) || (seconds >= kGunFireAgainFromSecond && seconds < kGunFireAgainUntilSecond)))
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
	}

	bool const sniper = !everySlot && plan.gunSlot == kGunSniperSlot;

	if (sniper)
	{
		static bool tookItDown = false, broughtItUp = false, tookItDownAgain = false, lastTap = false, pictureAtFull = false, pictureAtNone = false;
		DWORD const sinceStart = now - started;

		bool const secondary = (sinceStart >= kSniperHoldToFullZoomFrom && sinceStart < kSniperHoldToFullZoomUntil)
			|| (sinceStart >= kSniperHoldAgainFrom && sinceStart < kSniperHoldAgainUntil)
			|| Hudson_OneFrameAt(sinceStart, kSniperTapToTakeItDown, &tookItDown)
			|| Hudson_OneFrameAt(sinceStart, kSniperTapToBringItUpAtNoZoom, &broughtItUp)
			|| Hudson_OneFrameAt(sinceStart, kSniperTapToTakeItDownAgain, &tookItDownAgain)
			|| Hudson_OneFrameAt(sinceStart, kSniperLastTap, &lastTap);

		bool const primary = (sinceStart >= kSniperShootThroughScopeFrom && sinceStart < kSniperShootThroughScopeUntil)
			|| (sinceStart >= kSniperShotsFrom && sinceStart < kSniperShotsUntil && (sinceStart - kSniperShotsFrom) % kSniperShotEvery < kSniperShotLasts);

		if (secondary)
		{
			status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
		}

		if (primary)
		{
			status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
		}

		if (plan.pictures && Hudson_OneFrameAt(sinceStart, kSniperPictureAtFullZoom, &pictureAtFull))
		{
			gunPictureState = kSniperPictureNumberAtFullZoom;
			gunPictureSlot = plan.gunSlot;
			pictureIsDue = true;
		}

		if (plan.pictures && Hudson_OneFrameAt(sinceStart, kSniperPictureAtNoZoom, &pictureAtNone))
		{
			gunPictureState = kSniperPictureNumberAtNoZoom;
			gunPictureSlot = plan.gunSlot;
			pictureIsDue = true;
		}
	}

	if (!everySlot && !sniper && ((seconds >= kGunSecondaryFromSecond && seconds < kGunSecondaryUntilSecond) || seconds == kGunFirstTapAtSecond || seconds == kGunSecondTapAtSecond || seconds == kGunThirdTapAtSecond || seconds == kGunFourthTapAtSecond))
	{
		status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
	}

	// And a shot after each of the last three taps, so that a gun with three kinds of round fires each
	if (!everySlot && !sniper && (seconds == kGunFireWithSecondaryAtSecond || seconds == 55 || seconds == 56 || seconds == 60 || seconds == 61 || seconds == 65 || seconds == 66))
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
	}

	// A shot, then the reload key on a gun that's only part full, which is the case an empty gun's own reload doesn't try
	static bool pressedReloadPartFull = false;

	if (!everySlot && !sniper && seconds == kGunPartFullShotAtSecond)
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
	}

	if (!everySlot && !sniper && !pressedReloadPartFull && seconds >= kGunPartFullReloadKeyAtSecond)
	{
		pressedReloadPartFull = true;
		LogString("gun: pressed the reload key on a part-full gun");
		Weapon_ReloadKeyPressed(status);
	}

	// The M79 alone: see the times above
	if (!everySlot && plan.gunSlot == kGunM79Slot)
	{
		if (seconds == kGunM79EmptyShotAtSecond)
		{
			status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
		}

		if (seconds == kGunM79EmptyTapAtSecond)
		{
			status->Mvt_InputRequests.Flags.Rqst_FireSecondaryWeapon = 1;
		}
	}

	// In a level with a mirror, the marine in it is a real ghost, holding the gun he's holding. The log says which model that is,
	// a few seconds after he has chosen it, because a picture may well not be looking at the mirror.
	static bool saidWhoIsInTheMirror = false;
	extern BOOL Current_Level_Requires_Mirror_Image();
	extern NETGHOSTDATABLOCK PlayersMirrorGhost;

	if (!everySlot && !saidWhoIsInTheMirror && seconds >= kGunChooseAtSecond + 3 && Current_Level_Requires_Mirror_Image())
	{
		SECTION_DATA const *const root = PlayersMirrorGhost.HModelController.section_data;
		char line[160];

		saidWhoIsInTheMirror = true;
		_snprintf(line, sizeof(line) - 1, "gun: the marine in the mirror has the model %s, for weapon %d", (root && root->sempai && root->sempai->Hierarchy_Name) ? root->sempai->Hierarchy_Name : "none", PlayersMirrorGhost.CurrentWeapon);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	// And he turns round, because the mirror may be anywhere from where he starts. It takes pictures on the way, for seeing the
	// model in it, which the line above can only name.
	if (!everySlot && !plan.gunMarine && plan.pictures && seconds >= kGunMirrorTurnFromSecond && seconds < kGunMirrorTurnUntilSecond && Current_Level_Requires_Mirror_Image())
	{
		static DWORD lastMirrorPicture = 0;
		static int mirrorPictures = 0;
		DWORD const clock = GetTickCount();

		status->Mvt_InputRequests.Flags.Rqst_TurnRight = 1;
		status->Mvt_TurnIncrement = ONE_FIXED / 2;

		if (clock - lastMirrorPicture >= kGunMirrorPictureEveryMilliseconds)
		{
			lastMirrorPicture = clock;
			gunPictureState = kGunMirrorPicturesFrom + mirrorPictures++;
			gunPictureSlot = plan.gunSlot;
			pictureIsDue = true;
		}
	}

	// gunalien=yes: something to hit, 6 metres in front of him, a second before each of these. It stands still, and he looks at the
	// middle of it, because a look straight ahead from his eyes goes over its head.
	static int alienMadeAtSecond = -1;
	static char lastAlienName[SB_NAME_LENGTH] = { 0 };

	// gunmarine=yes: a marine with the model other players would see him holding this gun, facing him. He looks at it the same way.
	static bool madeTheMarine = false;

	if (!everySlot && plan.gunMarine && !madeTheMarine && seconds >= kGunMarineAtSecond)
	{
		int const weapon = (int)status->WeaponSlot[status->SelectedWeaponSlot].WeaponIDNumber;
		const char *const model = weapon == WEAPON_AUTOSHOTGUN ? "Marine with Shotgun"
			: weapon == WEAPON_SONICCANNON ? "Sniper Marine"
			: weapon == WEAPON_BEAMCANNON ? "Revolver Marine"
			: weapon == WEAPON_MYSTERYGUN ? "M79 Marine"
			: "marine with pulse rifle";
		DYNAMICSBLOCK const *const body = Player->ObStrategyBlock->DynPtr;
		VECTORCH spot = body->Position;
		VECTORCH towardHim;
		char line[96];

		madeTheMarine = true;
		spot.vx += MUL_FIXED(body->OrientMat.mat31, kGunMarineMetres);
		spot.vz += MUL_FIXED(body->OrientMat.mat33, kGunMarineMetres);

		STRATEGYBLOCK *const marine = CreateMarineBot(&spot, 0);
		SECTION *const section = GetNamedHierarchyFromLibrary("hnpcmarine", model);

		if (marine && section)
		{
			MARINE_STATUS_BLOCK *const marineStatus = (MARINE_STATUS_BLOCK *)marine->SBdataptr;

			Transmogrify_HModels(marine, &marineStatus->HModelController, section, 1, 1, 0);
			marineStatus->My_Gunflash_Section = GetThisSectionData(marineStatus->HModelController.section_data, "dum flash");

			towardHim.vx = -body->OrientMat.mat31;
			towardHim.vy = 0;
			towardHim.vz = -body->OrientMat.mat33;
			Normalise(&towardHim);
			MakeMatrixFromDirection(&towardHim, &marine->DynPtr->OrientMat);
			MatrixToEuler(&marine->DynPtr->OrientMat, &marine->DynPtr->OrientEuler);
			memcpy(lastAlienName, marine->SBname, SB_NAME_LENGTH);
		}

		_snprintf(line, sizeof(line) - 1, "gun: %s a marine with the model %s", (marine && section) ? "made" : "couldn't make", model);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	static int lastMarinePictureSecond = -1;

	if (!everySlot && plan.pictures && madeTheMarine && seconds > kGunMarineAtSecond && seconds < kGunFireFromSecond && seconds != lastMarinePictureSecond)
	{
		lastMarinePictureSecond = seconds;
		gunPictureState = kGunMarinePicturesFrom + seconds;
		gunPictureSlot = plan.gunSlot;
		pictureIsDue = true;
	}

	if (!everySlot && plan.gunAlien && alienMadeAtSecond != seconds
		&& (seconds == kGunSecondaryFromSecond - 1 || seconds == kGunFirstTapAtSecond - 1 || seconds == kGunSecondTapAtSecond - 1 || seconds == kGunThirdTapAtSecond - 1 || seconds == kGunFourthTapAtSecond - 1))
	{
		DYNAMICSBLOCK const *const body = Player->ObStrategyBlock->DynPtr;
		VECTORCH spot = body->Position;

		alienMadeAtSecond = seconds;
		spot.vx += MUL_FIXED(body->OrientMat.mat31, 6000);
		spot.vz += MUL_FIXED(body->OrientMat.mat33, 6000);
		CreateAlienBot(&spot, 0);

		STRATEGYBLOCK const *const made = TheAlienJustMadeAt(spot);

		if (made)
		{
			memcpy(lastAlienName, made->SBname, SB_NAME_LENGTH);
		}

		LogString(made ? "gun: made an alien 6 metres in front of him" : "gun: asked for an alien 6 metres in front of him, and none is there");
	}

	if (!everySlot && (plan.gunAlien || plan.gunMarine) && !NAME_ISNULL(lastAlienName))
	{
		STRATEGYBLOCK const *const alien = FindSBWithName(lastAlienName);

		if (alien && alien->SBdptr && alien->SBdptr->HModelControlBlock && alien->SBdptr->HModelControlBlock->section_data)
		{
			VECTORCH const middle = alien->SBdptr->HModelControlBlock->section_data->World_Offset;
			float const across = sqrtf((float)(middle.vx - Global_VDB_Ptr->VDB_World.vx) * (float)(middle.vx - Global_VDB_Ptr->VDB_World.vx)
				+ (float)(middle.vz - Global_VDB_Ptr->VDB_World.vz) * (float)(middle.vz - Global_VDB_Ptr->VDB_World.vz));

			// A full turn is 4096 here, and looking down is the way the numbers grow
			status->ViewPanX = ((int)(atan2f((float)(middle.vy - Global_VDB_Ptr->VDB_World.vy), across) * 4096.0f / 6.2831853f)) & 4095;
		}
	}

	if (!everySlot && !plan.gunAlien && plan.gunSlot == kGunStaffSlot)
	{
		if (seconds == kGunFirstTapAtSecond - 1) { status->ViewPanX = kGunAimUpALittle; }
		if (seconds == kGunSecondTapAtSecond - 1) { status->ViewPanX = kGunAimDown; }
		if (seconds == kGunThirdTapAtSecond - 1) { status->ViewPanX = kGunAimAtTheFloor; }
		if (seconds == kGunFourthTapAtSecond - 1) { status->ViewPanX = kGunAimUp; }
		if (seconds == kGunFourthTapAtSecond + 2) { status->ViewPanX = 0; }
	}

	// The reload key, once, with whatever is left in the gun by then
	static bool pressedReload = false;

	if (!everySlot && !pressedReload && seconds >= kGunReloadKeyAtSecond)
	{
		pressedReload = true;
		LogString("gun: pressed the reload key");
		Weapon_ReloadKeyPressed(status);
	}

	// Every couple of seconds, one frame before and after the lamp
	{
		static DWORD lastLampCompare = 0;

		if (plan.pictures && seconds >= kGunChooseAtSecond && lampCompareNumber < kGunLampComparesMost && now - lastLampCompare >= kGunLampCompareEveryTicks)
		{
			lastLampCompare = now;
			lampCompareNumber++;
			lampCompareIsDue = true;
		}
	}

	// Nothing in the level should be able to end this test early
	status->IsImmortal = 1;

	PLAYER_WEAPON_DATA const *const weaponPtr = &status->WeaponSlot[status->SelectedWeaponSlot];
	int const state = (int)weaponPtr->CurrentState;
	int const rounds = weaponPtr->PrimaryRoundsRemaining >> ONE_FIXED_SHIFT;
	int const spares = weaponPtr->PrimaryMagazinesRemaining;
	int const slot = (int)status->SelectedWeaponSlot;

	if (plan.gunSlot == kGunRevolverSlot && seconds >= kGunEndAtSecond)
	{
		Hudson_GunTriesRevolverReloads(status, seconds);
	}

	// Every frame of a reload: which move the model is on and how far through it, for telling a reload that plays from one that only waits
	if (state == WEAPONSTATE_RELOAD_PRIMARY)
	{
		static DWORD lastReloadPicture = 0;
		static int reloadPictures = 0;
		char reloading[160];

		_snprintf(reloading, sizeof(reloading) - 1, "gun: reload %.3f s timer %d type %d move %d through %d flags %d", (now - started) / 1000.0, weaponPtr->StateTimeOutCounter, PlayersWeaponHModelController.Sequence_Type, PlayersWeaponHModelController.Sub_Sequence, PlayersWeaponHModelController.sequence_timer, PlayersWeaponHModelController.keyframe_flags);
		reloading[sizeof(reloading) - 1] = 0;
		LogString(reloading);

		if (plan.pictures && reloadPictures < kGunReloadPicturesMost && now - lastReloadPicture >= kGunReloadPictureEveryTicks)
		{
			lastReloadPicture = now;
			gunPictureState = kGunReloadPicturesFrom + reloadPictures++;
			gunPictureSlot = slot + 1;
			pictureIsDue = true;
		}
	}

	// How long a shot takes. The game fires after this function has run, so when a round has just gone, the time since this function last ran is the frame that fired it. It's set against the ordinary frames since the shot before.
	{
		static LARGE_INTEGER lastTicks = { 0 }, ticksPerSecond = { 0 };
		static double ordinaryMilliseconds = 0.0;
		static int ordinaryFrames = 0;
		LARGE_INTEGER ticks;

		if (ticksPerSecond.QuadPart == 0)
		{
			QueryPerformanceFrequency(&ticksPerSecond);
		}

		QueryPerformanceCounter(&ticks);

		if (lastTicks.QuadPart != 0)
		{
			double const milliseconds = (double)(ticks.QuadPart - lastTicks.QuadPart) * 1000.0 / (double)ticksPerSecond.QuadPart;

			if (slot == lastSlot && rounds < lastRounds)
			{
				char line[120];

				_snprintf(line, sizeof(line) - 1, "gun: the frame of a shot took %.1f ms, the %d frames before it %.1f ms each", milliseconds, ordinaryFrames, ordinaryFrames ? ordinaryMilliseconds / ordinaryFrames : 0.0);
				line[sizeof(line) - 1] = 0;
				LogString(line);
				ordinaryMilliseconds = 0.0;
				ordinaryFrames = 0;
			}
			else
			{
				ordinaryMilliseconds += milliseconds;
				ordinaryFrames++;
			}
		}

		lastTicks = ticks;
	}

	if (state != lastState || rounds != lastRounds || spares != lastSpares || slot != lastSlot)
	{
		char line[160];

		_snprintf(line, sizeof(line) - 1, "gun: %.2f s slot %d weapon %d state %d rounds %d spares %d pile %d move %d zoom %.2f speed %d", (now - started) / 1000.0, slot + 1, (int)weaponPtr->WeaponIDNumber, state, rounds, spares, (int)weaponPtr->PrimaryLeftoverRounds, PlayersWeaponHModelController.Sub_Sequence, CameraZoomScale, (int)status->Encumberance.MovementMultiple);
		line[sizeof(line) - 1] = 0;

		// A gun's own state, as plain numbers
		std::string whole = line;
		const WEAPON_HOOKS *const hooks = (int)weaponPtr->WeaponIDNumber >= 0 ? TemplateWeapon[weaponPtr->WeaponIDNumber].Hooks : NULL;

		if (hooks && hooks->OwnState && hooks->OwnStateSize > 0 && hooks->OwnStateSize % sizeof(int) == 0)
		{
			whole += " own";

			for (int i = 0; i < hooks->OwnStateSize / (int)sizeof(int) && i < 32; i++)
			{
				whole += " " + std::to_string(((const int *)hooks->OwnState)[i]);
			}
		}

		LogString(whole);

		if (state != lastState)
		{
			stateStarted = now;
		}

		// Each gun has its own pictures, on a run that goes through them all
		if (slot != lastSlot)
		{
			memset(pictureTaken, 0, sizeof(pictureTaken));
		}

		lastState = state;
		lastRounds = rounds;
		lastSpares = spares;
		lastSlot = slot;
	}

	if (plan.pictures && slot == wantedSlot - 1 && state >= 0 && state < kGunMostStatesWithPictures && !pictureTaken[state] && now - stateStarted >= kGunPictureAfterTicks)
	{
		pictureTaken[state] = true;
		gunPictureState = state;
		gunPictureSlot = slot + 1;
		pictureIsDue = true;
	}

	static DWORD lastFiringPicture = 0;
	static int firingPictures = 0;

	if (plan.pictures && !everySlot && slot == wantedSlot - 1 && status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon && !pictureIsDue && now - lastFiringPicture >= kGunFiringPictureEveryTicks)
	{
		lastFiringPicture = now;
		gunPictureState = kGunFiringPicturesFrom + firingPictures++;
		gunPictureSlot = slot + 1;
		pictureIsDue = true;
	}

	DWORD const endAtSecond = sniper ? (DWORD)kSniperEndAtSecond : plan.gunSlot == kGunRevolverSlot ? (DWORD)kGunRevolverEndAtSecond : (DWORD)kGunEndAtSecond;

	if (everySlot ? (finishedChanging || seconds >= kGunChangeLatestEndSecond) : seconds >= endAtSecond)
	{
		EndTheLevel("it finished", kHudsonExitFinished);
	}
}

// The marine, if he's still there and alive. Looked up each time, because the game can destroy him at any moment, and something kept from before would then point at nothing.
static STRATEGYBLOCK *TheMarine()
{
	if (marineStage == kMarineNotMade)
	{
		return 0;
	}

	STRATEGYBLOCK *const found = FindSBWithName(marineName);

	if (!found || found->I_SBtype != I_BehaviourMarine || !found->DynPtr || found->SBflags.please_destroy_me || found->SBDamageBlock.Health <= 0)
	{
		return 0;
	}

	return found;
}

// Two things in the same place would push each other apart, and he's standing where the player is. So neither counts as being there for the other: the game's collisions pass over anything that can't be seen.
static void HideFromCollisionsAndTheCamera(STRATEGYBLOCK *marine)
{
	Player->ObFlags |= ObFlag_NotVis;

	// In a fight he has to be seen, and to be something a claw can hit, so only the player goes unseen.
	if (marine && marine->SBdptr && !plan.combatAlien)
	{
		marine->SBdptr->ObFlags |= ObFlag_NotVis;
	}
}

static void SayWhereHeIs(const char *what, LONG frame, const STRATEGYBLOCK *marine)
{
	char line[300] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: room %ld, view %d of %d: %s at frame %ld. He's at %d %d %d, facing %d %d. The camera is at %d %d %d, looking %d %d %d", roomsReached, viewNow + 1, plan.views, what, frame,
		marine->DynPtr->Position.vx, marine->DynPtr->Position.vy, marine->DynPtr->Position.vz, marine->DynPtr->OrientMat.mat31, marine->DynPtr->OrientMat.mat33,
		Global_VDB_Ptr->VDB_World.vx, Global_VDB_Ptr->VDB_World.vy, Global_VDB_Ptr->VDB_World.vz, Global_VDB_Ptr->VDB_Mat.mat13, Global_VDB_Ptr->VDB_Mat.mat23, Global_VDB_Ptr->VDB_Mat.mat33);
	LogString(line);
}

struct WallChoice
{
	HudsonWallSpot spot;
	int room;
	int polygon;
	double acrossThePicture;    // how far its corners' picture coordinates go, in the picture's own pixels. Nothing goes wrong if they're 0: the log just says so
	double downThePicture;
	uint32_t pictureWidth;
	uint32_t pictureHeight;
};

// How far a polygon's corners go across its picture and down it. They stay at 0 if the polygon's list of them is out of range or too short. A polygon with an animated texture never gets here: its list holds something else.
static void MeasureThePicture(const SHAPEHEADER *shape, const POLYHEADER *polygon, int corners, WallChoice *wall)
{
	wall->acrossThePicture = 0.0;
	wall->downThePicture = 0.0;

	uint32_t const listNumber = (uint32_t)polygon->PolyColour >> TxDefn;

	if (listNumber >= (uint32_t)shape->sh_numuvlists || !shape->sh_textures[listNumber] || shape->sh_uvlistlengths[listNumber] < corners)
	{
		return;
	}

	const int *const coordinates = shape->sh_textures[listNumber];
	int leastAcross = 0, mostAcross = 0, leastDown = 0, mostDown = 0;

	for (int i = 0; i < corners; i++)
	{
		int const across = coordinates[i * 2], down = coordinates[i * 2 + 1];

		leastAcross = (i == 0 || across < leastAcross) ? across : leastAcross;
		mostAcross = (i == 0 || across > mostAcross) ? across : mostAcross;
		leastDown = (i == 0 || down < leastDown) ? down : leastDown;
		mostDown = (i == 0 || down > mostDown) ? down : mostDown;
	}

	wall->acrossThePicture = (double)mostAcross - (double)leastAcross;
	wall->downThePicture = (double)mostDown - (double)leastDown;
}

// The biggest first. The two numbers after that are only so that two walls of the same size always come in the same order, because a run has to be able to be repeated.
static bool ComesFirst(const WallChoice &a, const WallChoice &b)
{
	if (a.spot.size != b.spot.size)
	{
		return a.spot.size > b.spot.size;
	}

	return (a.room != b.room) ? a.room < b.room : a.polygon < b.polygon;
}

// Every polygon in one room that's a wall showing the picture asked for, with room to stand in front of it inside that same room.
static void FindWallsInRoom(MODULE *room, int roomNumber, std::vector<WallChoice> *walls)
{
	// A module with a behaviour of its own, like a door, can move while he's looking at it.
	if (!room->m_mapptr || room->m_sbptr)
	{
		return;
	}

	SHAPEHEADER const *const shape = GetShapeData(room->m_mapptr->MapShape);

	if (!shape || !shape->items || !shape->points || !*shape->points || !shape->sh_normals || !*shape->sh_normals)
	{
		return;
	}

	const VECTORCH *const points = (const VECTORCH *)*shape->points;

	for (int polygonNumber = 0; polygonNumber < shape->numitems; polygonNumber++)
	{
		const POLYHEADER *const polygon = (const POLYHEADER *)shape->items[polygonNumber];

		// A polygon with only a colour has no texture, that number means something else. A see-through one doesn't go through the depth maps, so it would look like a failure when it isn't. An animated one changes between
		// two pictures of the same thing.
		if (!polygon || polygon->PolyItemType == I_ZB_GouraudPolygon || (polygon->PolyFlags & (iflag_notvis | iflag_transparent | iflag_txanim)))
		{
			continue;
		}

		uint32_t const texture = (uint32_t)polygon->PolyColour & ClrTxDefn;
		uint32_t width = 0, height = 0;

		if (!Tex_GetRealSize(texture, &width, &height) || !HudsonPlan_PictureIsNamed(Tex_GetTextureDetails(texture).name.c_str(), plan.wall))
		{
			continue;
		}

		int const place = polygon->PolyNormalIndex;   // counted in single numbers, three for each polygon

		if (place < 0 || place > (shape->numitems - 1) * 3)
		{
			continue;
		}

		int corners[kMostCornersOfAWall][3];
		int count = 0;
		const int *const cornerNumbers = &polygon->Poly1stPt;
		bool cornersAreGood = true;

		for (; count < kMostCornersOfAWall && cornerNumbers[count] != kEndOfCorners; count++)
		{
			if (cornerNumbers[count] < 0 || cornerNumbers[count] >= shape->numpoints)
			{
				cornersAreGood = false;
				break;
			}

			VECTORCH const &point = points[cornerNumbers[count]];

			corners[count][0] = point.vx + room->m_world.vx;
			corners[count][1] = point.vy + room->m_world.vy;
			corners[count][2] = point.vz + room->m_world.vz;
		}

		VECTORCH const &face = *(const VECTORCH *)(*shape->sh_normals + place);
		WallChoice wall;

		if (cornersAreGood && HudsonPlan_WallSpot(corners, count, face.vx, face.vy, face.vz, &wall.spot))
		{
			VECTORCH standHere = { wall.spot.x, wall.spot.y, wall.spot.z };

			if (ModuleFromPosition(&standHere, (MODULE *)0) == room)
			{
				wall.room = roomNumber;
				wall.polygon = polygonNumber;
				wall.pictureWidth = width;
				wall.pictureHeight = height;
				MeasureThePicture(shape, polygon, count, &wall);
				walls->push_back(wall);
			}
		}
	}
}

// The wall this run asked for. False if the level hasn't that many.
static bool ChooseTheWall(HudsonWallSpot *chosen)
{
	SCENEMODULE const *const scene = MainSceneArray[0];   // the game only ever has the one

	if (!scene || !scene->sm_marray)
	{
		return false;
	}

	std::vector<WallChoice> walls;

	for (int i = 0; i < ModuleArraySize; i++)
	{
		MODULE *const room = scene->sm_marray[i];

		if (room && (!plan.wallRoom[0] || (room->name && _stricmp(room->name, plan.wallRoom) == 0)))
		{
			FindWallsInRoom(room, i, &walls);
		}
	}

	std::stable_sort(walls.begin(), walls.end(), ComesFirst);

	char line[300] = { 0 };

	// Each room's biggest wall, and the number to ask for it by, so that one run shows what the others could choose.
	std::vector<int> roomsSaid;

	for (size_t i = 0; i < walls.size(); i++)
	{
		if (std::find(roomsSaid.begin(), roomsSaid.end(), walls[i].room) == roomsSaid.end())
		{
			MODULE const *const room = scene->sm_marray[walls[i].room];

			roomsSaid.push_back(walls[i].room);
			_snprintf(line, sizeof(line) - 1, "hudson: wall number %d is the biggest in room %s, which has %d lights. Its size is %.0f", (int)i + 1, room->name ? room->name : "with no name", room->m_numlights, walls[i].spot.size);
			LogString(line);
		}
	}

	memset(line, 0, sizeof(line));

	_snprintf(line, sizeof(line) - 1, "hudson: this level has %d walls showing %s with room to stand in front of them, and wall number %d was asked for", (int)walls.size(), plan.wall, plan.wallRank);
	LogString(line);

	if ((int)walls.size() < plan.wallRank)
	{
		return false;
	}

	WallChoice const &wall = walls[plan.wallRank - 1];
	MODULE const *const room = scene->sm_marray[wall.room];

	*chosen = wall.spot;

	_snprintf(line, sizeof(line) - 1, "hudson: the wall is polygon %d of room %d, %s, which has %d lights. Its size is %.0f. He stands at %d %d %d, and the way to the wall is %d %d", wall.polygon, wall.room, room->name ? room->name : "with no name", room->m_numlights,
		wall.spot.size, wall.spot.x, wall.spot.y, wall.spot.z, wall.spot.towardsX, wall.spot.towardsZ);
	LogString(line);

	// Depth is a fraction of a picture's width, so how much world one picture covers says how deep it really is. This takes the picture's directions to line up with the wall's, which a polygon's may not.
	_snprintf(line, sizeof(line) - 1, "hudson: the wall is %.0f wide and %.0f tall, and shows %.0f across and %.0f down of a picture %u x %u. So one picture covers about %.0f across and %.0f down", wall.spot.wide, wall.spot.tall, wall.acrossThePicture, wall.downThePicture,
		wall.pictureWidth, wall.pictureHeight, HudsonPlan_OnePictureCovers(wall.spot.wide, wall.acrossThePicture, wall.pictureWidth), HudsonPlan_OnePictureCovers(wall.spot.tall, wall.downThePicture, wall.pictureHeight));
	LogString(line);

	return true;
}

// The same steps the game takes to move a player in a network game.
static void MoveThePlayerTo(const VECTORCH &place)
{
	STRATEGYBLOCK *const block = Player->ObStrategyBlock;

	Player->ObWorld = block->DynPtr->Position = block->DynPtr->PrevPosition = place;
	block->containingModule = ModuleFromPosition(&Player->ObWorld, (MODULE *)0);
	playerPherModule = block->containingModule;
	Global_VDB_Ptr->VDB_World = Player->ObWorld;
	AllNewModuleHandler();
	DoObjectVisibilities();
}

// Puts him and the player where this view looks from, at the height he's standing at now. False if that's in no room at all. He settles again before the view counts, so a floor that's a little lower there does no harm.
static bool StandForThisView(STRATEGYBLOCK *marine)
{
	int stepX = 0, stepZ = 0;

	HudsonPlan_WallViewPlace(viewNow, wallTowardsX, wallTowardsZ, &stepX, &stepZ);

	VECTORCH place = { wallStandX + stepX, marine->DynPtr->Position.vy, wallStandZ + stepZ };
	MODULE *const room = ModuleFromPosition(&place, (MODULE *)0);

	if (!room)
	{
		return false;
	}

	marine->DynPtr->Position = place;
	marine->DynPtr->PrevPosition = place;
	marine->containingModule = room;
	marinePlaceLastFrame = place;
	MoveThePlayerTo(place);

	return true;
}

// The rooms of a tour, nearest first. The first is the one he starts in.
static std::vector<int> tourRooms;
static size_t tourNext = 1;
static size_t tourStep = 1;

// Lists the level's rooms by how many doorways each is from the room the player starts in. The game's own list of which room joins which decides that, so the same level always gives the same tour.
static void PlanTheTour()
{
	MODULE *start = ModuleFromPosition(&Player->ObStrategyBlock->DynPtr->Position, (MODULE *)0);

	if (plan.tourFrom[0] && ModuleArraySize > 0)
	{
		MODULE *named = 0;

		for (int i = 0; i < AIModuleArraySize && !named; i++)
		{
			for (MODULE **room = AIModuleArray[i].m_module_ptrs; room && *room && !named; room++)
			{
				if ((*room)->name && _stricmp((*room)->name, plan.tourFrom) == 0)
				{
					named = *room;
				}
			}
		}

		if (named)
		{
			start = named;
			tourNext = 0;
		}
		else
		{
			LogString("hudson: this level has no room of the name in tourfrom=, so the tour starts where the player does");
		}
	}

	// A level made for a network game needn't have anywhere for one player to start. Then the tour begins with the level's first room that has anywhere to stand.
	bool const playerIsNowhere = !start;

	if (playerIsNowhere && AIModuleArray && AIModuleArraySize > 0 && AIModuleArray[0].m_module_ptrs)
	{
		start = AIModuleArray[0].m_module_ptrs[0];
		tourNext = 0;
		LogString("hudson: the level has put the player in no room, so the tour starts from the first room of the level");
	}

	if (!start || !start->m_aimodule || !AIModuleArray || start->m_aimodule->m_index < 0 || start->m_aimodule->m_index >= AIModuleArraySize)
	{
		LogString("hudson: the player isn't in a room the game knows the doorways of, so there's no tour");
		return;
	}

	std::vector<char> seen(AIModuleArraySize, 0);

	tourRooms.push_back(start->m_aimodule->m_index);
	seen[start->m_aimodule->m_index] = 1;

	for (size_t i = 0; i < tourRooms.size(); i++)
	{
		for (AIMODULE **next = AIModuleArray[tourRooms[i]].m_link_ptrs; next && *next; next++)
		{
			int const room = (*next)->m_index;

			if (room >= 0 && room < AIModuleArraySize && !seen[room])
			{
				seen[room] = 1;
				tourRooms.push_back(room);
			}
		}
	}

	// Without a room to start from, it spreads out over everything that joins up, because the nearest rooms are often all one corridor. With one, it's that room and its neighbours that were asked for.
	if (!plan.tourFrom[0] && plan.rooms > 0 && tourRooms.size() > (size_t)plan.rooms)
	{
		tourStep = tourRooms.size() / plan.rooms;
		tourNext = playerIsNowhere ? 0 : tourStep;
	}

	char line[120] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: a tour: %d rooms of this level's %d join up with the first, and %d were asked for", (int)tourRooms.size(), AIModuleArraySize, plan.rooms);
	LogString(line);
}

// How far the last place to stand was from the middle of its room, for the log. 0 means he's right in the middle.
static int awayFromTheMiddle = 0;

// A place to stand in a room: the middle of it, on its floor, so that he sees the room and not a doorway. A room can be made of several parts, so it's the middle of the biggest.
// If the middle has no floor, it's the nearest place on the list the game works out for its own marines and aliens to appear at, which were all tested the same way. They're written down relative to the room and at the
// floor's height. Either way he goes a little above the floor. False if there's nowhere.
static bool PlaceToStandIn(int room, VECTORCH *place)
{
	return HudsonNav_MiddleOfRoom(room, place, &awayFromTheMiddle);
}

// Finds where he goes next on the tour, and says so in the log. False if he's been in as many rooms as were asked for, or there are no more with anywhere to stand.
static bool NextPlaceOnTheTour(VECTORCH *place)
{
	while (roomsReached < plan.rooms && tourNext < tourRooms.size())
	{
		int const room = tourRooms[tourNext];

		if (PlaceToStandIn(room, place))
		{
			tourNext += tourStep;

			MODULE const *const module = ModuleFromPosition(place, (MODULE *)0);
			char line[160] = { 0 };

			_snprintf(line, sizeof(line) - 1, "hudson: next on the tour is room %d of the level, %s, at %d %d %d, %d from its middle", room, module->name ? module->name : "with no name", place->vx, place->vy, place->vz, awayFromTheMiddle);
			LogString(line);
			return true;
		}

		tourNext++;

		char line[100] = { 0 };

		_snprintf(line, sizeof(line) - 1, "hudson: room %d of the level has nowhere listed to stand, so the tour leaves it out", room);
		LogString(line);
	}

	return false;
}

static void StartThisView(STRATEGYBLOCK *marine);

// Puts him and the player in the next room of a tour and starts him looking round it. False if the tour's over, or it isn't a tour.
static bool MoveHimToTheNextRoom(STRATEGYBLOCK *marine)
{
	VECTORCH place;

	if (plan.rooms <= 1 || !NextPlaceOnTheTour(&place))
	{
		return false;
	}

	marine->DynPtr->Position = place;
	marine->DynPtr->PrevPosition = place;
	marine->containingModule = ModuleFromPosition(&place, (MODULE *)0);
	marinePlaceLastFrame = place;
	MoveThePlayerTo(place);
	InterlockedIncrement(&roomsReached);
	viewNow = 0;
	StartThisView(marine);
	return true;
}

// He can't stay where he is. A tour goes on to the next room, or finishes if that was the last, because whoever's watching wants the rest of it and the log has said what happened. Anything else ends there, with this reason.
static void HeCantStayHere(STRATEGYBLOCK *marine, const char *howItEnds)
{
	if (plan.rooms <= 1)
	{
		EndTheLevel(howItEnds, kHudsonExitMarineStuck);
	}
	else if (!MoveHimToTheNextRoom(marine))
	{
		marineStage = kMarineFinished;
		EndTheLevel("it finished", kHudsonExitFinished);
	}
}

// Which of the things TheMarine looks for has stopped being true, and where he last was, because a run that just says he's gone leaves nothing to go on.
static void SayWhyHeHasGone(LONG frame)
{
	STRATEGYBLOCK const *const found = FindSBWithName(marineName);
	char const *why = "his health is gone";

	if (!found)
	{
		why = "the game has nothing of that name any more";
	}
	else if (found->I_SBtype != I_BehaviourMarine)
	{
		why = "that name now belongs to something else";
	}
	else if (!found->DynPtr)
	{
		why = "he has no body";
	}
	else if (found->SBflags.please_destroy_me)
	{
		why = "the game has marked him to be removed";
	}

	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: the marine went at frame %ld because %s. He was last at %d %d %d", frame, why, marinePlaceLastFrame.vx, marinePlaceLastFrame.vy, marinePlaceLastFrame.vz);
	LogString(line);
}

// See A FLARE at the top. Only a marine has flares, as with the key. It counts the flares in the world before and after, because that's the only way to know one really went.
static void ThrowFlareForThisHold()
{
	extern int NumberOfFlaresActive;
	int const before = NumberOfFlaresActive;

	if (AvP.PlayerType == I_Marine)
	{
		ThrowAFlare();
	}

	if (NumberOfFlaresActive > before)
	{
		flaresThrown++;
	}
}

// Is all he's doing standing in places and looking round? A fight, a walk and a strategy want him behaving as any marine would, so they don't count.
static bool ThisRunOnlyLooksRound()
{
	return !plan.combatAlien && !plan.navModules && !plan.route[0] && !plan.strategy[0] && !plan.listStrategies;
}

// How many hundredths of a second one of the game's times comes to, for the log.
static int InHundredths(int time)
{
	return time / (ONE_FIXED / 100);
}

// He won't turn towards it, but whoever reads the log should know it was going on, and where.
static void SayIfTheSquadSuspectsAnything(const STRATEGYBLOCK *marine)
{
	int hisTime, squadTime;
	VECTORCH hisPoint, squadPoint;

	Marine_WhatHeAndHisSquadSuspect(marine, &hisTime, &hisPoint, &squadTime, &squadPoint);

	if (squadTime)
	{
		char line[160] = { 0 };

		_snprintf(line, sizeof(line) - 1, "hudson: the squad suspects something at %d %d %d, for another %d hundredths of a second. He's taking no notice", squadPoint.vx, squadPoint.vy, squadPoint.vz, InHundredths(squadTime));
		LogString(line);
	}
}

static void StartThisView(STRATEGYBLOCK *marine)
{
	if (plan.wall[0])
	{
		if (!StandForThisView(marine))
		{
			EndTheLevel("a view's step along the wall goes outside the level", kHudsonExitNoSuchWall);
			return;
		}

		HudsonPlan_WallViewDirection(viewNow, wallTowardsX, wallTowardsZ, &wantedX, &wantedZ);

		char line[160] = { 0 };

		_snprintf(line, sizeof(line) - 1, "hudson: view %d looks %d degrees off straight at the wall, along %d %d, from %d along the wall", viewNow + 1, HudsonPlan_WallViewDegrees(viewNow), wantedX, wantedZ, HudsonPlan_WallViewStep(viewNow));
		LogString(line);
	}
	else
	{
		HudsonPlan_ViewDirection(viewNow, plan.views, &wantedX, &wantedZ);
	}

	VECTORCH const point = { marine->DynPtr->Position.vx + wantedX, marine->DynPtr->Position.vy, marine->DynPtr->Position.vz + wantedZ };

	if (ThisRunOnlyLooksRound())
	{
		Marine_TakesNoNoticeOfTheSquad(marine);
		SayIfTheSquadSuspectsAnything(marine);
	}

	Marine_GuardAndFace(marine, &point);
	marineStage = kMarineSettling;
	framesInThisStage = 0;
	framesSettled = 0;
	settlingSince = GetTickCount();
	lastSettlingReportAt = settlingSince;
	toldToFaceFrom = marine->DynPtr->Position;
}

// In a fight the player is only there because the game decides from where he is which rooms are up. He's standing right where the marine is, and something can't be hidden from a line of sight just by not being
// seen: the game's look along a line always counts the player's box. So he's made into nothing at all. He doesn't move or fall, he isn't something to bump into, and shots and looks pass through him. Acid doesn't
// go along a line, so he's told he can't die as well, or a stray splash would end the level.
static void MakeThePlayerAGhost()
{
	STRATEGYBLOCK *const block = Player->ObStrategyBlock;

	block->DynPtr->IsStatic = 1;
	block->DynPtr->DynamicsType = DYN_TYPE_NO_COLLISIONS;
	((PLAYER_STATUS *)block->SBdataptr)->IsImmortal = 1;
}

// He starts where the player does, because the level put the player on a floor, in a room, with space to stand. A rifle is what makes him a real marine in the game's eyes, and with nothing to aim at he never fires it.
static void MakeTheMarine(LONG frame)
{
	if (plan.combatAlien && AvP.Network != I_No_Network)
	{
		EndTheLevel("combat= is refused in a network game", kHudsonExitBadArguments);
		return;
	}

	VECTORCH place = Player->ObStrategyBlock->DynPtr->Position;

	if (plan.wall[0])
	{
		HudsonWallSpot spot;

		if (!ChooseTheWall(&spot))
		{
			EndTheLevel("the level hasn't the wall that was asked for", kHudsonExitNoSuchWall);
			return;
		}

		place.vx = spot.x;
		place.vy = spot.y;
		place.vz = spot.z;
		wallTowardsX = spot.towardsX;
		wallTowardsZ = spot.towardsZ;
		wallStandX = spot.x;
		wallStandZ = spot.z;
		MoveThePlayerTo(place);
	}
	else if (plan.navModules > 0)
	{
		// He starts in the middle of the room the level put the player in, standing on its floor, and not wherever the player happened to start.
		MODULE const *const room = Player->ObStrategyBlock->containingModule;
		int away = 0;

		if (room && room->m_aimodule && HudsonNav_MiddleOfRoom(room->m_aimodule->m_index, &place, &away))
		{
			MoveThePlayerTo(place);
		}
	}

	if (plan.rooms > 1 && tourRooms.empty())
	{
		PlanTheTour();

		// Asked to start somewhere, he goes straight there
		VECTORCH first;

		if (tourNext == 0 && NextPlaceOnTheTour(&first))
		{
			place = first;
			MoveThePlayerTo(place);
		}
	}

	// A marine can't be made in no room at all. The game's own code for making one expects a room, and a tour has had its chance to find one by now.
	if (!ModuleFromPosition(&place, (MODULE *)0))
	{
		EndTheLevel("the level has put the player in no room, so there's nowhere to make the marine", kHudsonExitMarineGone);
		return;
	}

	HideFromCollisionsAndTheCamera(0);

	if (plan.combatAlien)
	{
		MakeThePlayerAGhost();
		thisPartOfTheFightStartedAt = GetTickCount();
	}

	// CreateMarineBot counts its weapons from 1, and 0 means a random one. The rifle is number 0 in the game's own list, so it's asked for as that plus 1. Without this, a run's marine could have any gun, and one run couldn't be compared with the next.
	STRATEGYBLOCK *const made = CreateMarineBot(&place, MNPCW_PulseRifle + 1);

	if (!made)
	{
		EndTheLevel("the marine couldn't be made", kHudsonExitMarineGone);
		return;
	}

	// Nothing in a run should kill him: he's only there to carry the camera, and a run with no marine has nothing to show
	made->SBDamageBlock.SB_H_flags.Indestructable = 1;

	memcpy(marineName, made->SBname, SB_NAME_LENGTH);
	HideFromCollisionsAndTheCamera(made);
	marinePlaceLastFrame = place;
	marineStage = kMarineSettling;
	InterlockedIncrement(&roomsReached);

	char line[120] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: the marine is made at frame %ld, at %d %d %d", frame, place.vx, place.vy, place.vz);
	LogString(line);

	StartThisView(made);
}

// What the game calls one of a run's words for how the world is drawn. The second is what to give back for "as it is".
static int GameLighting(int word, int asItIs)
{
	switch (word)
	{
		case kHudsonLightingGouraud:
			return kLightingGouraud;

		case kHudsonLightingPerPixel:
			return kLightingPerPixel;

		case kHudsonLightingNormals:
			return kLightingNormals;

		case kHudsonLightingRelief:
			return kLightingRelief;

		default:
			return asItIs;
	}
}

static void DrawThisCase()
{
	HudsonCompareCase const &drawing = HudsonPlan_CompareCase(caseNow);

	ViewSwitches_AskLighting(GameLighting(drawing.lighting, LightingNow));
	ReliefHeightFlipped = drawing.heightFlipped;
	ReliefTiltFlipped = drawing.tiltFlipped;
	ReliefDepthNow = ReliefDepth_Clamped(drawing.noDepth ? 0.0f : kReliefDepthAtStart);
}

// A STRATEGY. strategies=list writes them all to the log, and strategy= shows one working. He goes and stands in front of
// it, looks at it, and holds still for long enough to see how it was. Then the game's own RequestState asks it to go on,
// which is what a switch does, and he watches for strategyseconds=. The log says how it was, each change, and how it ended.
// strategylook= stands him in front of something else, so that he can watch a door while the switch that opens it is asked.
// Every strategy whose state changes goes in the log, not only the one asked, because one request can set off a whole chain.
// This takes over from the views, and it goes by the clock, because frames go by too fast to see anything in.
// Nothing else in the world is stopped for this: a door, a switch and a lift all run as they always do.
enum StrategyStage
{
	kStrategyNotStarted,
	kStrategySettling,
	kStrategyBefore,
	kStrategyWatching,
	kStrategyOnlyListed
};

static const DWORD kStrategyMillisecondsBefore = 3000;   // long enough for someone to see how it was

static StrategyStage strategyStage = kStrategyNotStarted;
static DWORD strategyStageStarted = 0;
static std::string strategyStateBefore;
static std::string strategyStateLast;
static bool strategyPictureBeforeTaken = false;
static bool strategyPictureAfterTaken = false;
static bool strategyChanged = false;
static bool strategyLookChanged = false;
static std::map<const STRATEGYBLOCK *, std::string> strategyStatesNow;   // how each one was, the last time anyone looked
static std::map<std::string, std::string> strategiesThatChanged;         // its name, and how it ended up

// Looks at every strategy whose state can be read. It writes down the ones that are new, and says which have changed.
static void WatchEveryStrategy(DWORD watched, bool saying)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (!block)
		{
			continue;
		}

		std::string const state = HudsonStrategies_State(block);
		std::map<const STRATEGYBLOCK *, std::string>::iterator const known = strategyStatesNow.find(block);

		if (known == strategyStatesNow.end())
		{
			strategyStatesNow[block] = state;
		}
		else if (known->second != state)
		{
			known->second = state;

			if (saying)
			{
				char line[60] = { 0 };
				_snprintf(line, sizeof(line) - 1, "hudson strategy: after %lu.%03lu seconds: ", watched / 1000, watched % 1000);
				LogString(line + HudsonStrategies_Title(block) + " is " + state);
				strategiesThatChanged[HudsonStrategies_Title(block)] = state;
			}
		}
	}
}

// Gives back true if this has dealt with the frame, and false if the marine should carry on with his views.
static bool WatchTheStrategy(LONG frame)
{
	if (strategyStage == kStrategyOnlyListed)
	{
		return false;
	}

	STRATEGYBLOCK *const marine = TheMarine();

	if (!marine)
	{
		EndTheLevel("the marine has gone", kHudsonExitMarineGone);
		return true;
	}

	if (AvP.Network != I_No_Network)
	{
		EndTheLevel("strategies aren't looked at or asked in a network game", kHudsonExitNetworkGame);
		return true;
	}

	if (strategyStage == kStrategyNotStarted)
	{
		if (plan.listStrategies)
		{
			HudsonStrategies_ListAll();
		}

		if (!plan.strategy[0])
		{
			strategyStage = kStrategyOnlyListed;
			return false;
		}

		STRATEGYBLOCK const *const asked = HudsonStrategies_Find(plan.strategy);
		STRATEGYBLOCK const *const wanted = plan.strategyLook[0] ? HudsonStrategies_Find(plan.strategyLook) : asked;
		VECTORCH place = { 0, 0, 0 }, looksAt = { 0, 0, 0 };

		if (!asked || !wanted)
		{
			EndTheLevel(asked ? "strategylook= asked for one this level hasn't" : "strategy= asked for one this level hasn't", kHudsonExitNoSuchStrategy);
			return true;
		}

		if (!HudsonStrategies_WhereToStand(wanted, &place, &looksAt))
		{
			LogString("hudson strategy: nowhere to stand to see " + HudsonStrategies_Title(wanted));
			EndTheLevel("there's nowhere to stand to see that strategy", kHudsonExitNoSuchStrategy);
			return true;
		}

		marine->DynPtr->Position = place;
		marine->DynPtr->PrevPosition = place;
		marine->containingModule = ModuleFromPosition(&place, (MODULE *)0);
		marinePlaceLastFrame = place;
		MoveThePlayerTo(place);

		wantedX = looksAt.vx - place.vx;
		wantedZ = looksAt.vz - place.vz;
		looksAt.vy = place.vy;
		Marine_GuardAndFace(marine, &looksAt);

		char line[200] = { 0 };
		_snprintf(line, sizeof(line) - 1, " is at %d %d %d. He stands at %d %d %d", looksAt.vx, looksAt.vy, looksAt.vz, place.vx, place.vy, place.vz);
		LogString("hudson strategy: going to " + HudsonStrategies_Title(wanted) + line);

		strategyStage = kStrategySettling;
		framesSettled = 0;
		framesInThisStage = 0;
		return true;
	}

	STRATEGYBLOCK *const target = HudsonStrategies_Find(plan.strategy);

	if (!target)
	{
		EndTheLevel("the strategy has gone from the level", kHudsonExitNoSuchStrategy);
		return true;
	}

	VECTORCH const place = marine->DynPtr->Position;
	DWORD const now = timeGetTime();

	if (strategyStage == kStrategySettling)
	{
		bool const stoodStill = abs(place.vx - marinePlaceLastFrame.vx) <= kStillIsWithin && abs(place.vy - marinePlaceLastFrame.vy) <= kStillIsWithin && abs(place.vz - marinePlaceLastFrame.vz) <= kStillIsWithin;
		bool const facing = HudsonPlan_IsFacing(marine->DynPtr->OrientMat.mat31, marine->DynPtr->OrientMat.mat33, wantedX, wantedZ);

		marinePlaceLastFrame = place;
		framesInThisStage++;
		framesSettled = (stoodStill && facing) ? framesSettled + 1 : 0;

		if (framesSettled >= kFramesToSettle)
		{
			SayWhereHeIs("he's in front of the strategy", frame, marine);
			strategyStage = kStrategyBefore;
			strategyStageStarted = now;
		}
		else if (framesInThisStage > kMostFramesToSettle)
		{
			SayWhereHeIs("he won't settle in front of the strategy", frame, marine);
			EndTheLevel("he never settled facing the strategy", kHudsonExitMarineStuck);
		}

		return true;
	}

	if (strategyStage == kStrategyBefore)
	{
		if (plan.pictures && !strategyPictureBeforeTaken && now - strategyStageStarted >= kStrategyMillisecondsBefore / 2)
		{
			strategyPictureBeforeTaken = true;
			viewNow = 0;
			pictureIsDue = true;
			return true;
		}

		if (now - strategyStageStarted < kStrategyMillisecondsBefore)
		{
			return true;
		}

		strategyStateBefore = strategyStateLast = HudsonStrategies_State(target);
		LogString("hudson strategy: before: " + HudsonStrategies_Title(target) + " is " + strategyStateBefore);

		char line[80] = { 0 };
		_snprintf(line, sizeof(line) - 1, "hudson strategy: asking it to go on at frame %ld", frame);
		LogString(line);

		WatchEveryStrategy(0, false);
		RequestState(target, 1, 0);

		strategyStage = kStrategyWatching;
		strategyStageStarted = now;
		return true;
	}

	DWORD const watched = now - strategyStageStarted;
	std::string const state = HudsonStrategies_State(target);

	WatchEveryStrategy(watched, true);

	if (state != strategyStateLast)
	{
		strategyStateLast = state;
		strategyChanged = true;
	}

	STRATEGYBLOCK const *const lookedAt = plan.strategyLook[0] ? HudsonStrategies_Find(plan.strategyLook) : target;

	if (lookedAt && strategiesThatChanged.count(HudsonStrategies_Title(lookedAt)))
	{
		strategyLookChanged = true;
	}

	DWORD const toWatch = (DWORD)plan.strategySeconds * 1000;

	if (plan.pictures && !strategyPictureAfterTaken && watched >= toWatch * 3 / 4)
	{
		strategyPictureAfterTaken = true;
		viewNow = 1;
		pictureIsDue = true;
		return true;
	}

	if (watched < toWatch)
	{
		return true;
	}

	LogString("hudson strategy: after: " + HudsonStrategies_Title(target) + " is " + state + ". Its state changed: " + (strategyChanged ? "yes" : "no"));

	if (lookedAt && lookedAt != target)
	{
		LogString("hudson strategy: after: " + HudsonStrategies_Title(lookedAt) + ", which he was looking at, is " + HudsonStrategies_State(lookedAt) + ". Its state changed: " + (strategyLookChanged ? "yes" : "no"));
	}

	{
		char line[80] = { 0 };
		_snprintf(line, sizeof(line) - 1, "hudson strategy: %d strategies changed in all:", (int)strategiesThatChanged.size());
		LogString(line);

		for (std::map<std::string, std::string>::const_iterator changed = strategiesThatChanged.begin(); changed != strategiesThatChanged.end(); ++changed)
		{
			LogString("hudson strategy:   " + changed->first + " ended up " + changed->second);
		}
	}
	InterlockedIncrement(&viewsDone);
	marineStage = kMarineFinished;

	if (strategyChanged && strategyLookChanged)
	{
		EndTheLevel("it finished", kHudsonExitFinished);
	}
	else
	{
		EndTheLevel(strategyChanged ? "the strategy was asked to go on, and what he was looking at never changed" : "the strategy was asked to go on and its state never changed", kHudsonExitStrategyDidNothing);
	}

	return true;
}

// The game decides which rooms to draw, and who's near enough to run, from where the player is. So the player goes where he goes, or he'd walk out of what's drawn and stop being a body with a place in
// the world. Moving the player makes the game work out all of that, so it's done only when he's got far enough from the last time, or into another room.
static void KeepThePlayerWithHim(const STRATEGYBLOCK *marine)
{
	VECTORCH const place = marine->DynPtr->Position;

	if (marine->containingModule != Player->ObStrategyBlock->containingModule || abs(place.vx - playerFollowedTo.vx) + abs(place.vy - playerFollowedTo.vy) + abs(place.vz - playerFollowedTo.vz) > kPlayerFollowsEvery)
	{
		MoveThePlayerTo(place);
		playerFollowedTo = place;
	}
}

// Falling, burning and the like go through this. It isn't enough by itself: a model made of parts takes a blow on the part that was hit, and his health is then taken down by what that part lost, so each part has to be
// told as well. A part that can't be hurt can't come off either. Parts can be made new at any time, so this is done every frame.
static void MakeEveryPartUnbreakable(SECTION_DATA *part)
{
	part->current_damage.SB_H_flags.Indestructable = 1;

	for (SECTION_DATA *child = part->First_Child; child; child = child->Next_Sibling)
	{
		MakeEveryPartUnbreakable(child);
	}
}

// Somewhere in front of him, in sight of him, in a room the game has up just now, because what isn't up can't stop a look along a line. It tries straight ahead first, then round him, nearer each time.
static bool FindWhereTheAlienGoes(const STRATEGYBLOCK *marine, VECTORCH *spot)
{
	VECTORCH const standing = marine->DynPtr->Position;
	VECTORCH lookFrom = standing;

	lookFrom.vy -= kChestHeight;   // in the game's numbers, up is smaller

	for (size_t d = 0; d < sizeof(kAlienDistances) / sizeof(kAlienDistances[0]); d++)
	{
		for (int i = 0; i < kWaysToTryForTheAlien; i++)
		{
			int x = 0, z = 0;

			HudsonPlan_ViewDirection(i, kWaysToTryForTheAlien, &x, &z);

			VECTORCH way = { x, 0, z };

			Normalise(&way);

			VECTORCH place = { standing.vx + MUL_FIXED(way.vx, kAlienDistances[d]), standing.vy, standing.vz + MUL_FIXED(way.vz, kAlienDistances[d]) };
			MODULE const *const room = ModuleFromPosition(&place, marine->containingModule);

			if (!room || !ModuleCurrVisArray[room->m_index])
			{
				continue;
			}

			FindPolygonInLineOfSight(&way, &lookFrom, 0, marine->SBdptr);

			if (LOS_Lambda >= kAlienDistances[d] + kRoomBehindTheAlien)
			{
				*spot = place;
				return true;
			}
		}
	}

	return false;
}

// CreateAlienBot doesn't say who it made, and the only one standing exactly there, before anything has moved, is the one it just made.
static STRATEGYBLOCK *TheAlienJustMadeAt(const VECTORCH &spot)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (block->I_SBtype == I_BehaviourAlien && block->DynPtr && block->DynPtr->Position.vx == spot.vx && block->DynPtr->Position.vy == spot.vy && block->DynPtr->Position.vz == spot.vz)
		{
			return block;
		}
	}

	return 0;
}

static void AskForAFightPicture(const char *what)
{
	if (plan.pictures)
	{
		_snprintf(fightPictureName, sizeof(fightPictureName) - 1, "alien%d-%s", aliensMade, what);
		pictureIsDue = true;
	}
}

static void MakeTheAlien(STRATEGYBLOCK *marine, LONG frame)
{
	VECTORCH spot;

	if (!FindWhereTheAlienGoes(marine, &spot))
	{
		EndTheLevel("there's nowhere in sight of him to put the alien", kHudsonExitNoEnemy);
		return;
	}

	CreateAlienBot(&spot, 0);

	STRATEGYBLOCK const *const alien = TheAlienJustMadeAt(spot);

	if (!alien)
	{
		EndTheLevel("the alien couldn't be made", kHudsonExitNoEnemy);
		return;
	}

	memcpy(alienName, alien->SBname, SB_NAME_LENGTH);
	alienHealthLastFrame = alien->SBDamageBlock.Health;
	aliensMade++;
	thisAlienIsHisTarget = false;
	alienLastHurtAt = GetTickCount();
	InterlockedExchange(&enemyState, kEnemyAlive);

	// the same field a level would set, so that he's looking the right way. Seeing it, and what to do about it, is up to him
	Marine_GuardAndFace(marine, &spot);

	marineStage = kMarineFighting;
	framesInThisStage = 0;

	char line[160] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: alien %d of %d is made at frame %ld, at %d %d %d, with %d health", aliensMade, plan.combatAliens, frame, spot.vx, spot.vy, spot.vz, alien->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
	LogString(line);
}

// The alien's still in the fight only while it's the same alien, whole and alive. When one dies, it can become something else, or be gone altogether.
static bool AlienIsAlive(const STRATEGYBLOCK *alien)
{
	return alien && alien->I_SBtype == I_BehaviourAlien && !alien->SBflags.please_destroy_me && alien->SBDamageBlock.Health > 0 && !NPC_IsDead(const_cast<STRATEGYBLOCK *>(alien));
}

static void ReportTheFight(const STRATEGYBLOCK *marine, const MARINE_STATUS_BLOCK *status, const STRATEGYBLOCK *alien, LONG frame)
{
	VECTORCH apart = { marine->DynPtr->Position.vx - alien->DynPtr->Position.vx, marine->DynPtr->Position.vy - alien->DynPtr->Position.vy, marine->DynPtr->Position.vz - alien->DynPtr->Position.vz };
	char line[260] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: frame %ld: he's in state %d at %d %d, target %s, %d in the clip, %ld shots and %ld blows so far. The alien is %d away, in state %d, with %d health", frame, (int)status->behaviourState, marine->DynPtr->Position.vx, marine->DynPtr->Position.vz,
		status->Target == alien ? "the alien" : status->Target ? "something else" : "none", status->clipammo, shotsFired, blowsOnHim, Approximate3dMagnitude(&apart), (int)((const ALIEN_STATUS_BLOCK *)alien->SBdataptr)->BehaviourState, alien->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
	LogString(line);
}

static void WatchTheFight(STRATEGYBLOCK *marine, LONG frame)
{
	MARINE_STATUS_BLOCK *const status = (MARINE_STATUS_BLOCK *)marine->SBdataptr;

	marine->SBDamageBlock.SB_H_flags.Indestructable = 1;
	MakeEveryPartUnbreakable(status->HModelController.section_data);

	// He can't die, so the biting never ends, and a marine's courage runs out as it goes on. With none left he only tries to run. A real marine would be dead long before that. This is the most it can be.
	status->Courage = ONE_FIXED << 1;

	// If he's backed off into another room, the player goes too, or the game would stop thinking of him as being near
	if (marine->containingModule && marine->containingModule != Player->ObStrategyBlock->containingModule)
	{
		MoveThePlayerTo(marine->DynPtr->Position);
	}

	// A reload makes the number go up, and that's no shot, so only what it goes down by counts.
	if (status->clipammo < clipLastFrame)
	{
		InterlockedExchangeAdd(&shotsFired, clipLastFrame - status->clipammo);
	}

	clipLastFrame = status->clipammo;
	framesInThisStage++;

	if (marineStage == kMarineSettling)
	{
		if (GetTickCount() - thisPartOfTheFightStartedAt >= kMillisecondsBeforeTheAlien)
		{
			MakeTheAlien(marine, frame);
		}

		return;
	}

	if (marineStage == kMarineAfterTheKill)
	{
		if (framesInThisStage == 30)
		{
			AskForAFightPicture("after-the-kill");
		}

		DWORD const sinceTheKill = GetTickCount() - thisPartOfTheFightStartedAt;

		if (aliensMade < plan.combatAliens)
		{
			if (sinceTheKill >= kMillisecondsBetweenAliens)
			{
				MakeTheAlien(marine, frame);
			}
		}
		else if (sinceTheKill >= kMillisecondsAfterTheKill)
		{
			marineStage = kMarineFinished;
			EndTheLevel("it finished", kHudsonExitFinished);
		}

		return;
	}

	if (marineStage != kMarineFighting)
	{
		return;
	}

	if (framesInThisStage == 30)
	{
		AskForAFightPicture("alien-made");
	}

	char line[240] = { 0 };
	STRATEGYBLOCK *const found = FindSBWithName(alienName);
	STRATEGYBLOCK *const alien = (found && found->I_SBtype == I_BehaviourAlien) ? found : 0;

	if (!saidTheFirstShot && shotsFired > 0)
	{
		saidTheFirstShot = true;
		_snprintf(line, sizeof(line) - 1, "hudson: he fired his first shot by frame %ld", frame);
		LogString(line);
		AskForAFightPicture("first-shot");
	}

	if (alien && !thisAlienIsHisTarget && status->Target == alien)
	{
		thisAlienIsHisTarget = true;
		InterlockedExchange(&tookTheAlienAsATarget, 1);
		_snprintf(line, sizeof(line) - 1, "hudson: he took alien %d as his target at frame %ld", aliensMade, frame);
		LogString(line);
	}

	// Not every bullet: several hits in one frame count once here, and so does anything else that hurts it. Knowing it's being hurt, and how fast, is what's wanted.
	bool hurtThisFrame = false;

	if (alien && alien->SBDamageBlock.Health < alienHealthLastFrame)
	{
		hurtThisFrame = true;
		alienLastHurtAt = GetTickCount();
		InterlockedIncrement(&timesItWasHurt);
		_snprintf(line, sizeof(line) - 1, "hudson: the alien was hurt at frame %ld, and has %d health left", frame, alien->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
		LogString(line);
	}

	if (alien)
	{
		alienHealthLastFrame = alien->SBDamageBlock.Health;
	}

	if (!AlienIsAlive(alien))
	{
		// The blow that kills it usually isn't seen above, because by then it has stopped being an alien. It was still a blow.
		if (!hurtThisFrame)
		{
			InterlockedIncrement(&timesItWasHurt);
		}

		InterlockedIncrement(&aliensKilled);
		InterlockedExchange(&enemyState, kEnemyDead);
		_snprintf(line, sizeof(line) - 1, "hudson: alien %d of %d died at frame %ld. So far he's fired %ld shots, aliens were hurt %ld times, and %ld blows landed on him. He %s this one as a target", aliensMade, plan.combatAliens, frame, shotsFired, timesItWasHurt, blowsOnHim, thisAlienIsHisTarget ? "had taken" : "never took");
		LogString(line);
		marineStage = kMarineAfterTheKill;
		framesInThisStage = 0;
		thisPartOfTheFightStartedAt = GetTickCount();
		return;
	}

	// Nothing in the game ends a fight like this one, so the run has to, or it would stand here until frames= ran out.
	if (GetTickCount() - alienLastHurtAt >= kMillisecondsBeforeAFightHasStalled)
	{
		ReportTheFight(marine, status, alien, frame);
		InterlockedIncrement(&fightsStalled);
		InterlockedExchange(&enemyState, kEnemyTakenAway);
		_snprintf(line, sizeof(line) - 1, "hudson: the fight with alien %d of %d has stalled at frame %ld: it hasn't been hurt for %lu seconds, so it's taken away", aliensMade, plan.combatAliens, frame, (unsigned long)(kMillisecondsBeforeAFightHasStalled / 1000));
		LogString(line);

		// so that nothing is left pointing at it
		status->Target = NULL;
		DestroyAnyStrategyBlock(alien);

		marineStage = kMarineAfterTheKill;
		framesInThisStage = 0;
		thisPartOfTheFightStartedAt = GetTickCount();
		return;
	}

	if (framesInThisStage % kFramesBetweenFightReports == 0)
	{
		ReportTheFight(marine, status, alien, frame);
	}
}

void Hudson_HeWasHit(STRATEGYBLOCK *block, int kind)
{
	if (marineStage == kMarineNotMade || !NAME_ISEQUAL(block->SBname, marineName))
	{
		return;
	}

	// A drop of an alien's blood counts for every frame it touches him, so a body bleeding beside him comes to hundreds in a few seconds (dynamics.cpp). That's no measure of a fight, so it's counted apart from blows.
	if (kind == AMMO_ALIEN_FRAG)
	{
		InterlockedIncrement(&acidOnHim);
		return;
	}

	LONG const blows = InterlockedIncrement(&blowsOnHim);

	if (blows <= kBlowsToLog)
	{
		char line[120] = { 0 };

		_snprintf(line, sizeof(line) - 1, "hudson: blow %ld landed on him at frame %ld, of kind %d. He has %d health", blows, framesDone, kind, block->SBDamageBlock.Health >> ONE_FIXED_SHIFT);
		LogString(line);
	}
}

// What frames= running out means, which is the finding if the fight didn't happen.
static const char *HowTheFightRanOut()
{
	if (enemyState == kEnemyNotMade)
	{
		return "frames= ran out before the alien was made";
	}

	if (enemyState == kEnemyDead || enemyState == kEnemyTakenAway)
	{
		return "frames= ran out between aliens";
	}

	if (!tookTheAlienAsATarget)
	{
		return "frames= ran out, and he never took the alien as a target";
	}

	return shotsFired > 0 ? "frames= ran out with the alien still alive" : "frames= ran out: he took the alien as a target and never fired";
}

// Why he isn't settled yet: what's been holding him up, where he is, and what his own brain is doing.
static void SayWhySettlingIsTakingSoLong(const STRATEGYBLOCK *marine, LONG frame, bool stoodStill, bool facing)
{
	MARINE_STATUS_BLOCK const *const status = (const MARINE_STATUS_BLOCK *)marine->SBdataptr;
	char line[260] = { 0 };

	_snprintf(line, sizeof(line) - 1, "hudson: frame %ld: he's been settling for %lu seconds. Standing still: %s. Facing the way he was told: %s. He's at %d %d %d, facing %d %d, told %d %d, in state %d, with %s", frame, (unsigned long)((GetTickCount() - settlingSince) / 1000),
		stoodStill ? "yes" : "no", facing ? "yes" : "no", marine->DynPtr->Position.vx, marine->DynPtr->Position.vy, marine->DynPtr->Position.vz, marine->DynPtr->OrientMat.mat31, marine->DynPtr->OrientMat.mat33, wantedX, wantedZ, (int)status->behaviourState, status->Target ? "a target" : "no target");
	LogString(line);

	// A suspicious marine looks at what he suspects, ahead of the way he was told, so that's the first thing to know about one who won't turn.
	int hisTime, squadTime;
	VECTORCH hisPoint, squadPoint;

	Marine_WhatHeAndHisSquadSuspect(marine, &hisTime, &hisPoint, &squadTime, &squadPoint);
	_snprintf(line, sizeof(line) - 1, "hudson: frame %ld: he suspects something at %d %d %d for another %d hundredths of a second, and the squad at %d %d %d for another %d", frame, hisPoint.vx, hisPoint.vy, hisPoint.vz, InHundredths(hisTime), squadPoint.vx, squadPoint.vy, squadPoint.vz, InHundredths(squadTime));
	LogString(line);
}

static void WatchTheMarine(LONG frame)
{
	if (marineStage == kMarineNotMade)
	{
		MakeTheMarine(frame);
		return;
	}

	if ((plan.strategy[0] || plan.listStrategies) && WatchTheStrategy(frame))
	{
		return;
	}

	STRATEGYBLOCK *const marine = TheMarine();

	if (!marine)
	{
		SayWhyHeHasGone(frame);

		// On a tour there's another marine for the next room, because whoever's watching wants to see the rest of it, and the log has said what happened
		VECTORCH next;

		if (plan.rooms > 1 && NextPlaceOnTheTour(&next))
		{
			MoveThePlayerTo(next);
			marineStage = kMarineNotMade;
			viewNow = 0;
			return;
		}

		EndTheLevel("the marine has gone", kHudsonExitMarineGone);
		return;
	}

	if (plan.combatAlien)
	{
		WatchTheFight(marine, frame);
		return;
	}

	VECTORCH const place = marine->DynPtr->Position;
	bool const stoodStill = abs(place.vx - marinePlaceLastFrame.vx) <= kStillIsWithin && abs(place.vy - marinePlaceLastFrame.vy) <= kStillIsWithin && abs(place.vz - marinePlaceLastFrame.vz) <= kStillIsWithin;
	bool const facing = HudsonPlan_IsFacing(marine->DynPtr->OrientMat.mat31, marine->DynPtr->OrientMat.mat33, wantedX, wantedZ);

	marinePlaceLastFrame = place;
	framesInThisStage++;

	if (marineStage == kMarineWalking)
	{
		// Left alone, it's the one thing different, so that a run can show whether the frame rate alone changes how fast a marine goes
		if (!plan.frameHoldOff)
		{
			HudsonNav_HoldTheFrameRate();
		}

		KeepThePlayerWithHim(marine);

		if (!HudsonNav_EndOfFrame(marine, frame))
		{
			SayWhereHeIs("he stops walking", frame, marine);
			marineStage = kMarineFinished;

			// A run where he never walked into anywhere hasn't shown he can walk, however far he was put.
			if (HudsonNav_ModulesWalkedInto() > 0)
			{
				EndTheLevel("it finished", kHudsonExitFinished);
			}
			else
			{
				EndTheLevel("he didn't walk into a single new module", kHudsonExitMarineStuck);
			}
		}

		return;
	}

	if (marineStage == kMarineSettling)
	{
		framesSettled = (stoodStill && facing) ? framesSettled + 1 : 0;

		// He can come to rest somewhere other than where he was put, and then the point he was told to face is the wrong way for this view. So while he's standing still and not facing the right way, he's told again from where he is.
		if (stoodStill && !facing)
		{
			VECTORCH const point = { place.vx + wantedX, place.vy, place.vz + wantedZ };

			// Only if he's really somewhere new. Being told makes him stop and work out where to look, and he only starts turning on the frame after that, so being told every frame would stop him ever turning.
			if (abs(place.vx - toldToFaceFrom.vx) > kStillIsWithin || abs(place.vz - toldToFaceFrom.vz) > kStillIsWithin)
			{
				toldToFaceFrom = place;
				Marine_GuardAndFace(marine, &point);
			}
		}

		if (GetTickCount() - lastSettlingReportAt >= kMillisecondsBetweenSettlingReports)
		{
			lastSettlingReportAt = GetTickCount();
			SayWhySettlingIsTakingSoLong(marine, frame, stoodStill, facing);
		}

		if (framesSettled >= kFramesToSettle && plan.navModules > 0)
		{
			SayWhereHeIs("he starts walking", frame, marine);

			if (HudsonNav_Start(marine, plan.navModules, plan.route, plan.paceRun))
			{
				marineStage = kMarineWalking;
				framesInThisStage = 0;
				playerFollowedTo = place;
			}
			else
			{
				EndTheLevel("he can't start walking from here, or the route can't be used", kHudsonExitNoSuchRoute);
			}
		}
		else if (framesSettled >= kFramesToSettle)
		{
			SayWhereHeIs("a hold starts", frame, marine);
			marineStage = kMarineHolding;
			framesInThisStage = 0;
			holdStartedAt = GetTickCount();
			pictureTakenThisHold = false;
			visionTakenThisHold = false;

			if (plan.flareEachHold)
			{
				ThrowFlareForThisHold();
			}

			if (plan.compareRelief)
			{
				caseNow = 0;
				DrawThisCase();
			}
		}
		else if (GetTickCount() - settlingSince > (plan.rooms > 1 ? kMostMillisecondsToSettleOnATour : kMostMillisecondsToSettle))
		{
			SayWhySettlingIsTakingSoLong(marine, frame, stoodStill, facing);
			SayWhereHeIs("he won't settle", frame, marine);
			HeCantStayHere(marine, "he never settled facing the way he was told");
		}

		return;
	}

	if (marineStage == kMarineHolding)
	{
		if (!stoodStill || !facing)
		{
			SayWhereHeIs("he moved during a hold", frame, marine);
			HeCantStayHere(marine, "he moved while he was meant to hold still");
			return;
		}

		if (plan.compareRelief)
		{
			if (framesInThisStage == kFramesForACase / 2)
			{
				HudsonCompareCase const &drawing = HudsonPlan_CompareCase(caseNow);

				// If this machine can't draw it, the picture would be of something else, with the wrong name on it.
				if (lightingDrawn != GameLighting(drawing.lighting, lightingDrawn))
				{
					EndTheLevel("this machine can't draw one of the ways it was asked to compare", kHudsonExitModeNotThere);
					return;
				}

				pictureIsDue = true;
			}

			if (framesInThisStage < kFramesForACase)
			{
				return;
			}

			caseNow++;

			if (caseNow < HudsonPlan_CompareCaseCount())
			{
				DrawThisCase();
				framesInThisStage = 0;
				return;
			}

			caseNow = 0;
		}
		else if (plan.pictures && !pictureTakenThisHold && (plan.holdSeconds > 0 ? GetTickCount() - holdStartedAt >= (DWORD)plan.holdSeconds * 500 : framesInThisStage == plan.holdFrames / 2))
		{
			pictureIsDue = true;
			pictureTakenThisHold = true;
		}

		// In the middle of a hold, the same as a picture, once he's had time to be still
		if (plan.vision && !visionTakenThisHold && (plan.holdSeconds > 0 ? GetTickCount() - holdStartedAt >= (DWORD)plan.holdSeconds * 500 : framesInThisStage == plan.holdFrames / 2))
		{
			visionIsDue = true;
			visionTakenThisHold = true;
		}

		if (plan.compareRelief || (plan.holdSeconds > 0 ? GetTickCount() - holdStartedAt >= (DWORD)plan.holdSeconds * 1000 : framesInThisStage >= plan.holdFrames))
		{
			SayWhereHeIs("the hold ends", frame, marine);
			InterlockedIncrement(&viewsDone);
			viewNow++;

			if (viewNow < plan.views)
			{
				StartThisView(marine);
			}
			else if (!MoveHimToTheNextRoom(marine))
			{
				marineStage = kMarineFinished;
				EndTheLevel("it finished", kHudsonExitFinished);
			}
		}
	}
}

void Hudson_SetTheMode()
{
	if (plan.lighting == kHudsonLightingAsItIs && plan.shadows == kHudsonShadowsAsTheyAre)
	{
		return;
	}

	int const lighting = GameLighting(plan.lighting, LightingNow);
	int const volumes = (plan.shadows == kHudsonShadowsAsTheyAre) ? ShadowVolumesOn : (plan.shadows == kHudsonShadowsOn) ? 1 : 0;

	// The view has to go with them. On its own, the view could stay at OFF while the switches said otherwise, and the game would draw something else.
	ViewSwitches_SetAtStart(lighting, volumes);
}

// Did this run get the mode it asked for? True if it didn't ask.
static bool GotTheModeItAskedFor()
{
	bool const lightingIsRight =
		plan.lighting == kHudsonLightingAsItIs ||
		(plan.lighting == kHudsonLightingGouraud && lightingDrawn == kLightingGouraud) ||
		(plan.lighting == kHudsonLightingPerPixel && lightingDrawn == kLightingPerPixel) ||
		(plan.lighting == kHudsonLightingNormals && lightingDrawn == kLightingNormals) ||
		(plan.lighting == kHudsonLightingRelief && lightingDrawn == kLightingRelief);

	bool const shadowsAreRight =
		plan.shadows == kHudsonShadowsAsTheyAre ||
		(plan.shadows == kHudsonShadowsOn && shadowsDrawn == 1) ||
		(plan.shadows == kHudsonShadowsOff && shadowsDrawn == 0);

	// The switches only say what was asked for. The view at OFF means no shadow pass, whatever they say, and that's only right if everything is meant to be off. This is what reads what's really being drawn.
	bool const viewAgrees =
		(plan.lighting == kHudsonLightingAsItIs && plan.shadows == kHudsonShadowsAsTheyAre) ||
		(ToyShadowsMode == 0) == (lightingDrawn == kLightingGouraud && shadowsDrawn == 0);

	return lightingIsRight && shadowsAreRight && viewAgrees;
}

// What a ray through this pixel hits, going straight through the marine, because the camera is looking out from inside him and he isn't drawn. direction can be left out.
static bool WhatIsAt(int x, int y, const HudsonVisionView &view, ShootTarget *target, float *direction = 0)
{
	float own[3];
	float *const ray = direction ? direction : own;
	STRATEGYBLOCK *const marine = TheMarine();

	target->found = false;

	if (!HudsonVision_RayThrough(x, y, view.width, view.height, view.projection11, view.projection22, view.view, ray))
	{
		return false;
	}

	ShadowShoot_FindTarget(ray, target, marine ? marine->SBdptr : 0);
	return target->found;
}

// The picture on a polygon, by name, or nothing if it hasn't one. The numbers come from the level, so they're checked first.
static const char *PictureOn(const ShootTarget &target)
{
	SHAPEHEADER const *const shape = target.shapeNumber >= 0 ? GetShapeData(target.shapeNumber) : 0;

	if (!shape || target.polygon < 0 || target.polygon >= shape->numitems)
	{
		return "no picture it can name";
	}

	POLYHEADER const *const polygon = (POLYHEADER const *)shape->items[target.polygon];

	if (!polygon || polygon->PolyItemType == I_ZB_GouraudPolygon)
	{
		return "no picture";
	}

	uint32_t const texture = (uint32_t)polygon->PolyColour & ClrTxDefn;
	uint32_t width = 0, height = 0;

	return Tex_GetRealSize(texture, &width, &height) ? Tex_GetTextureDetails(texture).name.c_str() : "no picture it can name";
}

// One line for one finding: what's wrong, how much of it, where, and what's there.
static void SayWhatsMissing(const char *what, const HudsonVisionFinding &finding, const HudsonVisionView &view)
{
	if (finding.pixels == 0)
	{
		return;
	}

	ShootTarget target;
	char line[400] = { 0 };

	if (WhatIsAt(finding.firstX, finding.firstY, view, &target))
	{
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: %s, %d pixels, in the box %d %d to %d %d. The first, at %d %d, is polygon %d of shape %d in module %d, showing %s", roomsReached, viewNow + 1, what, finding.pixels,
			finding.left, finding.top, finding.right, finding.bottom, finding.firstX, finding.firstY, target.polygon, target.shapeNumber, target.moduleNumber, PictureOn(target));
	}
	else
	{
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: %s, %d pixels, in the box %d %d to %d %d. The first, at %d %d, is nothing a ray can find", roomsReached, viewNow + 1, what, finding.pixels,
			finding.left, finding.top, finding.right, finding.bottom, finding.firstX, finding.firstY);
	}

	LogString(line);
}

// A pixel with nothing drawn is usually right. It's sky, or a gap in a grate, or something the pictures leave out on purpose. It's a hole only if a ray through it hits a solid part of the level, with nothing
// about the polygon to say it could be left out. A ray tests the polygon and not the picture on it, so a polygon whose picture can have gaps can't say either way. So this undercounts, on purpose: a finding has to mean something.
enum RayFound
{
	kRayFoundAHole,
	kRayFoundNothing,        // sky, or outside the level
	kRayFoundAModel,         // something that isn't a part of the level: models come and go, and aren't all in the pictures
	kRayFoundCanBeLeftOut,   // see-through, or with gaps in its picture, or not drawn at all
	kRayFoundADoor,          // between two shapes, and the ray only sees one of them
	kRayFoundKinds
};

static RayFound WhatARayFound(const ShootTarget &target)
{
	if (!target.found)
	{
		return kRayFoundNothing;
	}

	if (target.isPartOfAModel || target.moduleNumber < 0)
	{
		return kRayFoundAModel;
	}

	if (target.flags & (iflag_transparent | iflag_ignore0 | iflag_notvis))
	{
		return kRayFoundCanBeLeftOut;
	}

	return target.isMorphing ? kRayFoundADoor : kRayFoundAHole;
}

// The game's ray stops at a polygon from either side (los.cpp: "treat all polys as no bfc"), but a polygon is only drawn from the side it faces, unless it's marked as having two. So a ray can stop at the back of something that
// can't be in any picture from here. This says when that's what happened. A polygon whose shape keeps no plane for it can't be told, so it says no.
static bool RayStoppedAtABack(const ShootTarget &target, const float *direction)
{
	return target.hasPlane && !(target.flags & iflag_no_bfc) && HudsonVision_SeenFromBehind(direction, target.plane);
}

static void LookAtThisFrame()
{
	// The game draws a mirrored frame differently, and what's in the pictures can't be judged the same way. The rule that turns mirroring on is in view.cpp, and derelict's start room is one.
	if (MirroringActive)
	{
		InterlockedIncrement(&visionMirrored);
		return;
	}

	HudsonVisionCounts counts;
	HudsonVisionView view;

	if (!SurfacePictures_LookAtThisFrame(&counts, &view))
	{
		InterlockedIncrement(&visionReadsFailed);
		LogString("vision: couldn't read the pictures of this frame");
		return;
	}

	InterlockedIncrement(&visionViews);
	InterlockedExchangeAdd(&visionNoNormal, counts.noNormal.pixels);
	InterlockedExchangeAdd(&visionNoDistance, counts.noDistance.pixels);
	InterlockedExchangeAdd(&visionNothingDrawn, counts.nothingDrawn.pixels);

	SayWhatsMissing("colour and no normal", counts.noNormal, view);
	SayWhatsMissing("colour and no distance", counts.noDistance, view);

	int found[kRayFoundKinds] = { 0 };
	ShootTarget firstHole;
	int firstHoleX = 0, firstHoleY = 0, holesAtBacks = 0;

	firstHole.found = false;

	for (int i = 0; i < counts.samples; i++)
	{
		ShootTarget target;
		float direction[3];

		WhatIsAt(counts.sampleX[i], counts.sampleY[i], view, &target, direction);

		RayFound const kind = WhatARayFound(target);

		if (kind == kRayFoundAHole && RayStoppedAtABack(target, direction))
		{
			holesAtBacks++;
		}

		if (kind == kRayFoundAHole && found[kRayFoundAHole] == 0)
		{
			firstHole = target;
			firstHoleX = counts.sampleX[i];
			firstHoleY = counts.sampleY[i];
		}

		found[kind]++;
	}

	InterlockedExchangeAdd(&visionHoles, found[kRayFoundAHole]);
	InterlockedExchangeAdd(&visionHolesAtBacks, holesAtBacks);

	char line[400] = { 0 };

	if (found[kRayFoundAHole] > 0)
	{
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: a hole: %d of %d rays through pixels with nothing drawn hit a solid part of the level, %d of them at the back of a polygon. The first, at %d %d, is polygon %d of shape %d in module %d, showing %s", roomsReached, viewNow + 1,
			found[kRayFoundAHole], counts.samples, holesAtBacks, firstHoleX, firstHoleY, firstHole.polygon, firstHole.shapeNumber, firstHole.moduleNumber, PictureOn(firstHole));
		LogString(line);
	}

	if (counts.samples > 0)
	{
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: of %d rays through pixels with nothing drawn: %d holes, %d found nothing, %d found a model, %d found what can be left out, %d found a door", roomsReached, viewNow + 1,
			counts.samples, found[kRayFoundAHole], found[kRayFoundNothing], found[kRayFoundAModel], found[kRayFoundCanBeLeftOut], found[kRayFoundADoor]);
		LogString(line);
	}

	// Is a pixel missing from the surface pictures missing from the screen as well? If it looks like those round it, the game's own drawing has it, and only the pictures lost it. If it doesn't, the game's own drawing has a crack there too.
	int differs = 0, tooDark = 0;

	for (int i = 0; i < view.screenPixels; i++)
	{
		differs += view.screen[i].differs ? 1 : 0;
		tooDark += view.screen[i].tooDark ? 1 : 0;

		if (i < 4)
		{
			_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: pixel %d %d with nothing drawn is %d %d %d on the screen, and those round it average %d %d %d", roomsReached, viewNow + 1, view.screen[i].x, view.screen[i].y,
				view.screen[i].colour[0], view.screen[i].colour[1], view.screen[i].colour[2], view.screen[i].around[0], view.screen[i].around[1], view.screen[i].around[2]);
			LogString(line);
		}
	}

	InterlockedExchangeAdd(&visionScreenDiffers, differs);
	InterlockedExchangeAdd(&visionScreenCompared, view.screenPixels);
	InterlockedExchangeAdd(&visionScreenTooDark, tooDark);

	if (view.screenPixels > 0)
	{
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: of %d pixels with nothing drawn, on the screen %d stand out from those round them, and %d are too dark to tell", roomsReached, viewNow + 1, view.screenPixels, differs, tooDark);
		LogString(line);
	}

	// Rays against the distance picture. If they agree, a polygon a ray names really is the one at that pixel. Doors that morph are counted apart, because they can be on the move. Every ray that disagrees says what it hit,
	// because that's how to find out why.
	int agree = 0, compared = 0, doorsAgree = 0, doorsCompared = 0, atBacks = 0;

	for (int i = 0; i < counts.checks; i++)
	{
		ShootTarget target;
		float direction[3];

		if (!WhatIsAt(counts.checkX[i], counts.checkY[i], view, &target, direction))
		{
			// The picture has a surface here and the ray found nothing at all, which is as far from agreeing as it gets
			compared++;
			_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: a ray found nothing at %d %d, where the distance picture says %.0f", roomsReached, viewNow + 1, counts.checkX[i], counts.checkY[i], counts.checkDistance[i]);
			LogString(line);
			continue;
		}

		bool const agrees = HudsonVision_RayAgrees(counts.checkDistance[i], target.distance, direction, view.view);
		bool const atItsBack = RayStoppedAtABack(target, direction);

		if (target.isMorphing)
		{
			doorsCompared++;
			doorsAgree += agrees ? 1 : 0;
		}
		else
		{
			compared++;
			agree += agrees ? 1 : 0;
		}

		atBacks += atItsBack ? 1 : 0;

		if (!agrees)
		{
			_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: a ray and the distance picture disagree at %d %d: the picture says %.0f, and the ray went %d, %.0f of it straight ahead, to polygon %d of shape %d in module %d, flags %x, behaviour %d, %s, %s, showing %s", roomsReached, viewNow + 1,
				counts.checkX[i], counts.checkY[i], counts.checkDistance[i], target.distance, HudsonVision_HowFarAhead(target.distance, direction, view.view), target.polygon, target.shapeNumber, target.moduleNumber, target.flags, target.behaviour,
				target.isMorphing ? "a door" : "not a door", !target.hasPlane ? "no plane kept for it" : atItsBack ? "at its back" : "not at its back", PictureOn(target));
			LogString(line);
		}
	}

	InterlockedExchangeAdd(&visionRaysAgree, agree);
	InterlockedExchangeAdd(&visionRaysCompared, compared);
	InterlockedExchangeAdd(&visionDoorRaysAgree, doorsAgree);
	InterlockedExchangeAdd(&visionDoorRaysCompared, doorsCompared);
	InterlockedExchangeAdd(&visionRaysAtBacks, atBacks);

	if (HudsonVision_ShadowsAreMissing(view.shadows))
	{
		InterlockedIncrement(&visionNoShadows);
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: no shadows were sent: the shadows are on, with %d lights chosen and %d polygons that could cast, and no corners went to the card", roomsReached, viewNow + 1, view.shadows.lights, view.shadows.casters);
		LogString(line);
	}

	// What's at the middle of the screen, so that a person can hold the log up against a picture of the same view
	ShootTarget middle;

	if (WhatIsAt(view.width / 2, view.height / 2, view, &middle))
	{
		_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: the middle of the screen is polygon %d of shape %d in module %d, %d away, showing %s", roomsReached, viewNow + 1, middle.polygon, middle.shapeNumber, middle.moduleNumber, middle.distance, PictureOn(middle));
		LogString(line);
	}

	_snprintf(line, sizeof(line) - 1, "vision: room %ld, view %d: looked at %d pixels drawn with shaders/%s: %d surfaces from %.0f to %.0f away, %d with nothing drawn, rays agreed with the distance picture at %d of %d pixels, and at doors %d of %d, %d lights, %d polygons that could cast, %d corners", roomsReached, viewNow + 1,
		counts.pixelsLookedAt, SurfacePictures_ShaderUsedLast(), counts.surfaces, counts.nearest, counts.furthest, counts.nothingDrawn.pixels, agree, compared, doorsAgree, doorsCompared, view.shadows.lights, view.shadows.casters, view.shadows.corners);
	LogString(line);
}

// Starts the film on the first frame of the level, at the size the screen is. A film that can't start says why, and the run goes on without one.
static void StartTheVideo(D3DSURFACE_DESC *filmSize)
{
	IDirect3DSurface9 *screen = 0;

	if (FAILED(d3d.lpD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &screen)))
	{
		HudsonVideoWriter_StopBecause("grab", "couldn't find the frame to read, so there's no film, and the run goes on");
		return;
	}

	HRESULT const asked = screen->GetDesc(filmSize);

	screen->Release();

	if (FAILED(asked) || (filmSize->Format != D3DFMT_X8R8G8B8 && filmSize->Format != D3DFMT_A8R8G8B8))
	{
		HudsonVideoWriter_StopBecause("grab", "the screen isn't in the kind of pixels a film is made from, so there's no film, and the run goes on");
		return;
	}

	char const *const folder = GetSaveFolderPath();

	if (!folder || folder[0] == 0)
	{
		HudsonVideoWriter_StopBecause("start", "there's no folder to put a film in, so there's no film, and the run goes on");
		return;
	}

	SYSTEMTIME now;

	GetLocalTime(&now);
	_snprintf(videoName, sizeof(videoName) - 1, "hudson-video-%04d-%02d-%02d-%02d%02d%02d.mp4", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

	HudsonVideoWriter_Start((std::string(folder) + videoName).c_str(), (int)filmSize->Width, (int)filmSize->Height, plan.breakVideo ? kHudsonVideoBreakAtPicture : 0, kHudsonVideoMostBytes);
}

// Reads the finished frame back from the card and gives it to the film. copy is kept for the whole run, because making one for each frame would cost more than the reading does.
static void GiveTheFilmThisFrame(const D3DSURFACE_DESC &filmSize, double seconds)
{
	static IDirect3DSurface9 *copy = 0;

	LPDIRECT3DDEVICE9 const device = d3d.lpD3DDevice;
	IDirect3DSurface9 *screen = 0;
	D3DSURFACE_DESC size;
	char why[200] = { 0 };

	HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &screen);

	if (SUCCEEDED(hr))
	{
		hr = screen->GetDesc(&size);
	}

	if (SUCCEEDED(hr) && (size.Width != filmSize.Width || size.Height != filmSize.Height || size.Format != filmSize.Format))
	{
		HudsonVideoWriter_StopBecause("grab", "the screen changed size, and a film can't, so it stops here and the run goes on");
	}
	else
	{
		if (SUCCEEDED(hr) && !copy)
		{
			hr = device->CreateOffscreenPlainSurface(filmSize.Width, filmSize.Height, filmSize.Format, D3DPOOL_SYSTEMMEM, &copy, NULL);
		}

		if (SUCCEEDED(hr))
		{
			hr = device->GetRenderTargetData(screen, copy);
		}

		D3DLOCKED_RECT locked;

		if (SUCCEEDED(hr))
		{
			// A siege's film has a clock on it, so that the same moment can be found in the film from the other machine. Both games are on one machine, so they share its clock.
			hr = copy->LockRect(&locked, NULL, plan.siegeRoom[0] ? 0 : D3DLOCK_READONLY);
		}

		if (SUCCEEDED(hr))
		{
			if (plan.siegeRoom[0] && (filmSize.Format == D3DFMT_X8R8G8B8 || filmSize.Format == D3DFMT_A8R8G8B8))
			{
				HudsonPlan_StampNumber((unsigned char *)locked.pBits, locked.Pitch, (int)filmSize.Width, (int)filmSize.Height, GetTickCount());
			}

			HudsonVideoWriter_Add((const uint8_t *)locked.pBits, locked.Pitch, seconds);
			copy->UnlockRect();
		}

		if (FAILED(hr))
		{
			_snprintf(why, sizeof(why) - 1, "couldn't read the frame back from the card, which said 0x%08lX, so the film stops here and the run goes on", (unsigned long)hr);
			HudsonVideoWriter_StopBecause("grab", why);
		}
	}

	if (screen)
	{
		screen->Release();
	}
}

// Takes this moment's picture, if one's due. Reading a frame back is the only part that costs the game anything, so it isn't done unless a picture is wanted, and what it costs goes in the log.
static void TakeVideoFrame()
{
	static bool triedToStart = false;
	static D3DSURFACE_DESC filmSize;
	static LARGE_INTEGER started, ticksPerSecond;

	// framesDone only counts frames of the level, so a film starts with the level, not with whatever was on the screen before
	if (framesDone < 1)
	{
		return;
	}

	if (!triedToStart)
	{
		triedToStart = true;
		StartTheVideo(&filmSize);
		QueryPerformanceFrequency(&ticksPerSecond);
		QueryPerformanceCounter(&started);
	}

	LARGE_INTEGER before;

	QueryPerformanceCounter(&before);

	double const seconds = (double)(before.QuadPart - started.QuadPart) / (double)ticksPerSecond.QuadPart;

	if (HudsonVideoWriter_WantsPictureFor(seconds))
	{
		GiveTheFilmThisFrame(filmSize, seconds);

		LARGE_INTEGER after;

		QueryPerformanceCounter(&after);

		LONG const microseconds = (LONG)((after.QuadPart - before.QuadPart) * 1000000 / ticksPerSecond.QuadPart);

		InterlockedIncrement(&videoGrabs);
		InterlockedExchangeAdd(&videoGrabMicroseconds, microseconds);

		if (microseconds > videoGrabWorstMicroseconds)
		{
			InterlockedExchange(&videoGrabWorstMicroseconds, microseconds);
		}
	}

	HudsonVideoFacts facts;

	HudsonVideoWriter_GetFacts(&facts);
	SayIfTheVideoHasNews(facts);
}

// The other game of the pair starts once this file is there, because the host only answers once it's playing: tools/hudson/run-net-pair.ps1 waits for it. It says nothing that matters, and the next run clears it.
static void SayTheHostIsReady()
{
	static bool said = false;

	if (said || plan.netMode != kHudsonNetHost)
	{
		return;
	}

	said = true;
	Hudson_NetSay("the host is in its level, and answering");

	char const *const folder = GetSaveFolderPath();
	FILE *const file = fopen((std::string(folder ? folder : "") + "hudson-net-ready.txt").c_str(), "w");

	if (file)
	{
		fprintf(file, "ready\n");
		fclose(file);
	}
}

// What's on the screen now, in a PNG file
static bool SaveTheScreen(std::string const &path)
{
	LPDIRECT3DDEVICE9 const device = d3d.lpD3DDevice;
	IDirect3DSurface9 *screen = 0;
	IDirect3DSurface9 *copy = 0;
	D3DSURFACE_DESC size;
	bool saved = false;

	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &screen)))
	{
		if (SUCCEEDED(screen->GetDesc(&size)) && SUCCEEDED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
		{
			saved = SUCCEEDED(device->GetRenderTargetData(screen, copy)) && SUCCEEDED(D3DXSaveSurfaceToFileA(path.c_str(), D3DXIFF_PNG, copy, NULL, NULL));
			copy->Release();
		}

		screen->Release();
	}

	return saved;
}

bool Hudson_CustomMapHasEnded()
{
	return HudsonRunIsOn && plan.customMap[0] && AvP.LevelCompleted;
}

void Hudson_LooksForThePageAfterAMap(bool thePageIsBack)
{
	static int frames = 0;

	frames++;

	if (frames == 120)
	{
		Hudson_SaveThePage("custom-maps-page-after.png");
		EndAtOnce(thePageIsBack ? "the custom map ended, and its page came back" : "the custom map ended, and some other page came up", thePageIsBack ? kHudsonExitFinished : kHudsonExitNetStopped);
	}
}

void Hudson_SaveThePage(const char *name)
{
	char const *const folder = GetSaveFolderPath();
	std::string const pictures = std::string(folder ? folder : "") + "pictures\\";

	CreateDirectoryA(pictures.c_str(), NULL);

	if (SaveTheScreen(pictures + name))
	{
		InterlockedIncrement(&picturesSaved);
		LogString(std::string("hudson: saved ") + name);
	}
	else
	{
		LogErrorString(std::string("hudson: couldn't save ") + name);
	}
}

void Hudson_TakePictureIfDue()
{
	SayTheHostIsReady();

	if (plan.recordVideo)
	{
		TakeVideoFrame();
	}

	if (visionIsDue)
	{
		visionIsDue = false;
		LookAtThisFrame();
	}

	if (!pictureIsDue)
	{
		return;
	}

	pictureIsDue = false;

	char const *const folder = GetSaveFolderPath();
	std::string const pictures = std::string(folder ? folder : "") + "pictures\\";

	CreateDirectoryA(pictures.c_str(), NULL);

	char name[48] = { 0 };

	if (plan.gunSlot)
	{
		_snprintf(name, sizeof(name) - 1, "gun%d-state%d.png", gunPictureSlot, gunPictureState);
	}
	else if (plan.combatAlien)
	{
		_snprintf(name, sizeof(name) - 1, "combat-%s.png", fightPictureName);
	}
	else if (plan.compareRelief)
	{
		_snprintf(name, sizeof(name) - 1, "room%ld-view%d-%s.png", roomsReached, viewNow + 1, HudsonPlan_CompareCase(caseNow).name);
	}
	else
	{
		_snprintf(name, sizeof(name) - 1, "room%ld-view%d.png", roomsReached, viewNow + 1);
	}

	std::string const path = pictures + name;
	bool const saved = SaveTheScreen(path);

	if (saved)
	{
		InterlockedIncrement(&picturesSaved);
		LogString(std::string("hudson: saved ") + name + (ViewSwitches_MainDrawWentThroughHeights() ? ", drawn through the depth maps" : ""));
	}
	else
	{
		// the run goes on, because its numbers are still good, and the error and the count on the result line say what's missing
		LogErrorString(std::string("hudson: couldn't save ") + name);
	}
}

// Behind him, looking the way he looks, so that he can be seen. Coming in short of a wall, because a camera past a wall sees nothing but the back of it. Only walls count: an alien running through behind him isn't
// a reason to jump in close. If the place it wants is in no room at all, it stays at his eyes.
static void PutTheCameraBehindHim(const STRATEGYBLOCK *marine, VECTORCH *place)
{
	VECTORCH way = { -marine->DynPtr->OrientMat.mat31, 0, -marine->DynPtr->OrientMat.mat33 };

	if (way.vx == 0 && way.vz == 0)
	{
		return;
	}

	Normalise(&way);

	VECTORCH const standing = marine->DynPtr->Position;
	VECTORCH lookFrom = standing;

	lookFrom.vy -= kChestHeight;   // in the game's numbers, up is smaller

	FindPolygonInLineOfSight(&way, &lookFrom, 0, marine->SBdptr);

	int distance = kCameraBehindHim;

	if (LOS_ObjectHitPtr && LOS_ObjectHitPtr->ObMyModule && LOS_Lambda - kCameraOffAWall < distance)
	{
		distance = LOS_Lambda - kCameraOffAWall;
	}

	if (distance <= 0)
	{
		return;
	}

	VECTORCH wanted = { standing.vx + MUL_FIXED(way.vx, distance), standing.vy - kCameraAboveHisEyes, standing.vz + MUL_FIXED(way.vz, distance) };

	if (ModuleFromPosition(&wanted, marine->containingModule))
	{
		*place = wanted;
	}
}

void Hudson_PointTheCamera(VECTORCH *place, MATRIXCH *turn)
{
	STRATEGYBLOCK *const marine = TheMarine();

	if (!marine)
	{
		return;
	}

	// The game's turning code lets his matrix grow or shrink, and the camera would stretch with it
	HudsonNav_TidyHisTurn(marine);

	// from his dynamics, and not his picture on the screen, because he only has one of those while he's near
	*place = marine->DynPtr->Position;
	*turn = marine->DynPtr->OrientMat;

	if (plan.combatAlien)
	{
		PutTheCameraBehindHim(marine, place);
	}

	// he can get a new picture at any time, and a new one doesn't have this set
	HideFromCollisionsAndTheCamera(marine);
}

bool Hudson_KeepsStill(STRATEGYBLOCK *block)
{
	if (HudsonWorldIsLive)
	{
		return false;
	}

	switch (block->I_SBtype)
	{
		case I_BehaviourMarine:
			return !(marineStage != kMarineNotMade && NAME_ISEQUAL(block->SBname, marineName));

		case I_BehaviourAlien:
		case I_BehaviourFaceHugger:
		case I_BehaviourPredator:
		case I_BehaviourDormantPredator:
		case I_BehaviourXenoborg:
		case I_BehaviourQueenAlien:
		case I_BehaviourPredatorAlien:
		case I_BehaviourSeal:
		case I_BehaviourAutoGun:
		case I_BehaviourGenerator:
			return true;

		default:
			return false;
	}
}

// Long enough for what a level starts with to have been made, and for the host's first messages to have arrived on another machine
static const LONG kFramesBeforeTheCounts = 120;
// What a player finds when a level starts, one fact to a line, so that a run of a level in single player
// and a run of the same level in the mode that plays like it can be compared line by line.
static void SayWhatThePlayerMeets(const char *firstWords)
{
	char line[300];
	STRATEGYBLOCK *const block = Player->ObStrategyBlock;
	PLAYER_STATUS const *const status = (PLAYER_STATUS *)block->SBdataptr;
	extern VECTORCH PlayerStartLocation;

	const char *const species = AvP.PlayerType == I_Alien ? "alien" : AvP.PlayerType == I_Predator ? "predator" : "marine";

	_snprintf(line, sizeof(line) - 1, "level %s as %s", LevelName, species);
	line[sizeof(line) - 1] = 0;
	LogString(std::string(firstWords) + line);

	_snprintf(line, sizeof(line) - 1, "mode is %s, campaign %d, place %d",
		AvP.Network == I_No_Network ? "single player" : SameAsSinglePlayer() ? "same as single player" : "network rules",
		netGameData.coopCampaign, netGameData.coopCampaignLevel);
	line[sizeof(line) - 1] = 0;
	LogString(std::string("meets-only-here: ") + line);

	_snprintf(line, sizeof(line) - 1, "start %d %d %d", PlayerStartLocation.vx, PlayerStartLocation.vy, PlayerStartLocation.vz);
	line[sizeof(line) - 1] = 0;
	LogString(std::string(firstWords) + line);

	MODULE const *const room = block->containingModule;

	_snprintf(line, sizeof(line) - 1, "he's in room %s", room && room->name ? room->name : "(none)");
	line[sizeof(line) - 1] = 0;
	LogString(std::string(firstWords) + line);

	_snprintf(line, sizeof(line) - 1, "he's at %d %d %d", block->DynPtr->Position.vx, block->DynPtr->Position.vy, block->DynPtr->Position.vz);
	line[sizeof(line) - 1] = 0;
	LogString(std::string("meets-only-here: ") + line);

	_snprintf(line, sizeof(line) - 1, "health %d armour %d difficulty %d", block->SBDamageBlock.Health, block->SBDamageBlock.Armour, (int)AvP.Difficulty);
	line[sizeof(line) - 1] = 0;
	LogString(std::string(firstWords) + line);

	for (int slot = 0; slot < MAX_NO_OF_WEAPON_SLOTS; slot++)
	{
		PLAYER_WEAPON_DATA const *const weapon = &status->WeaponSlot[slot];

		if (weapon->Possessed)
		{
			_snprintf(line, sizeof(line) - 1, "slot %d weapon %d possessed %d rounds %d mags %d second %d secondmags %d", slot, (int)weapon->WeaponIDNumber, weapon->Possessed,
				weapon->PrimaryRoundsRemaining, weapon->PrimaryMagazinesRemaining, weapon->SecondaryRoundsRemaining, weapon->SecondaryMagazinesRemaining);
			line[sizeof(line) - 1] = 0;
			LogString(std::string(firstWords) + line);
		}
	}

	// Everything in the level, by what kind of thing it is, and objects by what they are
	std::map<int, int> kinds;
	std::map<std::pair<int, int>, int> objects;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const thing = ActiveStBlockList[i];

		if (!thing)
		{
			continue;
		}

		kinds[(int)thing->I_SBtype]++;

		if (thing->I_SBtype == I_BehaviourInanimateObject && thing->SBdataptr)
		{
			INANIMATEOBJECT_STATUSBLOCK const *const object = (INANIMATEOBJECT_STATUSBLOCK *)thing->SBdataptr;

			objects[std::make_pair((int)object->typeId, object->subType)]++;
		}
	}

	for (std::map<int, int>::const_iterator kind = kinds.begin(); kind != kinds.end(); ++kind)
	{
		_snprintf(line, sizeof(line) - 1, "things of kind %d: %d", kind->first, kind->second);
		line[sizeof(line) - 1] = 0;
		LogString(std::string(firstWords) + line);
	}

	for (std::map<std::pair<int, int>, int>::const_iterator object = objects.begin(); object != objects.end(); ++object)
	{
		_snprintf(line, sizeof(line) - 1, "objects of type %d subtype %d: %d", object->first.first, object->first.second, object->second);
		line[sizeof(line) - 1] = 0;
		LogString(std::string(firstWords) + line);
	}
}

extern NetID MultiplayerObservedPlayer;
static bool levelStartedAgain = false;
static DWORD endAfterLookingAt = 0;

void Hudson_TheLevelStartedAgain()
{
	levelStartedAgain = true;
}

bool Hudson_PlaysLevelFilms()
{
	return HudsonRunIsOn && plan.levelFilms;
}

// What the proof needs from a film: that it really played, for how long, and how long the connection went without being looked after.
static DWORD filmStartedAt = 0;
static DWORD filmLastFrameAt = 0;
static DWORD filmLongestGap = 0;

void Hudson_FilmStarts(const char *film)
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	filmStartedAt = filmLastFrameAt = GetTickCount();
	filmLongestGap = 0;
	LogString(std::string("film: ") + film + " starts");
}

bool Hudson_FilmGoesOn()
{
	if (!HudsonRunIsOn)
	{
		return true;
	}

	DWORD const now = GetTickCount();

	if (now - filmLastFrameAt > filmLongestGap)
	{
		filmLongestGap = now - filmLastFrameAt;
	}

	filmLastFrameAt = now;

	return plan.filmSkipSeconds == 0 || (now - filmStartedAt) / 1000 < (DWORD)plan.filmSkipSeconds;
}

void Hudson_FilmEnded()
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	char line[120];

	_snprintf(line, sizeof(line) - 1, "film: it ran for %lu ms, and the connection never went more than %lu ms without being looked after", GetTickCount() - filmStartedAt, filmLongestGap);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

bool Hudson_IsASiege()
{
	return HudsonRunIsOn && plan.siegeRoom[0] != 0;
}

bool Hudson_NoFriendlyFire()
{
	return HudsonRunIsOn && plan.noFriendlyFire;
}

int Hudson_Difficulty()
{
	return plan.difficulty;
}

bool Hudson_PressesAKeyAfterHeDied()
{
	static DWORD sawHimOutAt = 0;

	if (!HudsonRunIsOn || plan.dieAtSeconds == 0)
	{
		return false;
	}

	DWORD const now = GetTickCount();

	if (sawHimOutAt == 0)
	{
		sawHimOutAt = now;
	}

	if (now - sawHimOutAt < 3000)
	{
		return false;
	}

	sawHimOutAt = 0;
	LogString("hudson: three seconds since he went out, so a key is pressed");
	return true;
}

// dieat=: once in a run, so that a level starting again doesn't kill him again
// A player's spare ammunition doesn't change in a run where he shoots nothing, unless he picks something up. This says when it
// does and where he was. The tick is this machine's clock, and two games in a pair run share a machine, so their lines compare.
static void SayIfHisAmmunitionChanged()
{
	static int lastTotal = -1;
	static int levelSeen = -1;

	// The level's number changes as the old level stops, and the kit changes when the new one starts. Waiting here
	// keeps those two apart, so that a new level's kit isn't reported as something he picked up.
	if (Coop_LevelChangeIsDue())
	{
		return;
	}

	PLAYER_STATUS const *const status = (PLAYER_STATUS *)Player->ObStrategyBlock->SBdataptr;
	int total = 0;

	for (int slot = 0; slot < MAX_NO_OF_WEAPON_SLOTS; slot++)
	{
		PLAYER_WEAPON_DATA const *const weapon = &status->WeaponSlot[slot];

		if (weapon->Possessed)
		{
			total += weapon->PrimaryMagazinesRemaining + (weapon->SecondaryRoundsRemaining >> ONE_FIXED_SHIFT) + weapon->SecondaryMagazinesRemaining;
		}
	}

	// Each level hands out its own kit, so that isn't news
	if (lastTotal >= 0 && levelSeen == netGameData.coopCampaignLevel && total != lastTotal)
	{
		char line[160];
		VECTORCH const *const at = &Player->ObStrategyBlock->DynPtr->Position;

		_snprintf(line, sizeof(line) - 1, "meets-only-here: his spare ammunition went from %d to %d at %d %d %d, tick %u", lastTotal, total, at->vx, at->vy, at->vz, GetTickCount());
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	lastTotal = total;
	levelSeen = netGameData.coopCampaignLevel;
}

static void DieIfItIsTime()
{
	static DWORD started = 0;
	static bool diedAlready = false;

	if (plan.dieAtSeconds == 0 || diedAlready)
	{
		return;
	}

	DWORD const now = GetTickCount();

	if (started == 0)
	{
		started = now;
	}

	// A pair's smoke test comes first
	if (plan.netMode != kHudsonNetNone && !smokeIsOver)
	{
		started = now;
		return;
	}

	if (now - started >= (DWORD)plan.dieAtSeconds * 1000)
	{
		diedAlready = true;
		LogString("hudson: he dies now, as dieat= asks");
		CauseDamageToObject(Player->ObStrategyBlock, &certainDeath, ONE_FIXED, NULL);
	}
}

// listplace= with coopend=: in a skirmish, the level's end is reached this long in, on each level
static void ReachTheEndIfItIsTime()
{
	static DWORD started = 0;
	static int levelSeen = -1;

	if (plan.coopEndSeconds == 0 || (!plan.listPlaceAsked && !plan.customMap[0]))
	{
		return;
	}

	DWORD const now = GetTickCount();

	if (levelSeen != netGameData.coopCampaignLevel || started == 0)
	{
		levelSeen = netGameData.coopCampaignLevel;
		started = now;
	}

	if ((now - started) / 1000 >= (DWORD)plan.coopEndSeconds)
	{
		LogString("hudson: this level's end has been reached, as coopend= asks");
		AvP.LevelCompleted = 1;
		started = now;
	}

	// Nobody is here to press a key on the statistics page, so after a moment this does what that key does
	static int framesOnThePage = 0;

	if (plan.customMap[0] && AvP.LevelCompleted && ++framesOnThePage == 120)
	{
		LogString("hudson: custommap: the statistics page is over");
		AvP.MainLoopRunning = 0;
	}
}

// fps=: wait until the next frame is due. HudsonNextFrameDue says when that is, and why.
static void HoldToTheFrameRate()
{
	static LARGE_INTEGER ticksEachSecond = { 0 }, frameDue = { 0 };

	if (plan.framesPerSecond <= 0)
	{
		return;
	}

	if (ticksEachSecond.QuadPart == 0)
	{
		QueryPerformanceFrequency(&ticksEachSecond);
		QueryPerformanceCounter(&frameDue);
	}

	LONGLONG const ticksEachFrame = ticksEachSecond.QuadPart / plan.framesPerSecond;
	LARGE_INTEGER now;

	QueryPerformanceCounter(&now);
	frameDue.QuadPart = HudsonNextFrameDue(frameDue.QuadPart, now.QuadPart, ticksEachFrame);

	while (now.QuadPart < frameDue.QuadPart)
	{
		Sleep(1);
		QueryPerformanceCounter(&now);
	}
}

// How fast this run really went, because that isn't always what it asked for. A gap of a second or more is a level loading or a film playing, not the game running, so it doesn't count.
static LONGLONG ticksPlaying = 0;
static LONG framesTimed = 0;
static LONGLONG ticksEachSecondForTheRate = 0;

static void NoteHowFastThisFrameWent()
{
	static LARGE_INTEGER lastFrame = { 0 };
	LARGE_INTEGER now;

	if (ticksEachSecondForTheRate == 0)
	{
		LARGE_INTEGER ticks;

		QueryPerformanceFrequency(&ticks);
		ticksEachSecondForTheRate = ticks.QuadPart;
		QueryPerformanceCounter(&lastFrame);
		return;
	}

	QueryPerformanceCounter(&now);

	if (now.QuadPart - lastFrame.QuadPart < ticksEachSecondForTheRate)
	{
		ticksPlaying += now.QuadPart - lastFrame.QuadPart;
		framesTimed++;
	}

	lastFrame = now;
}

static double FramesEachSecondReached()
{
	return ticksPlaying > 0 ? (double)framesTimed * (double)ticksEachSecondForTheRate / (double)ticksPlaying : 0.0;
}

// ghostlog=yes: what each NPC really is on the host, what the host last sent about it, and what the joiner's ghost of it
// has and last got. Each machine says so once a second, and at once if its animation, its cloak or whether it's alive
// changes. tools/hudson/ghost-compare.js lines the two logs up and lists where they differ. The time is the Windows clock,
// which is the same for both games only because a pair run has them on one machine.
struct GhostLogEntry
{
	NETMESSAGE_NPCSTATE message;
	int messages;
	DWORD messageAt;
	DWORD lastLine;
	int lastSequence, lastSubSequence;
	bool lastAlive, lastCloaked, here;
	int lastBeams;
};

static std::map<int, GhostLogEntry> ghostLogEntries;

static GhostLogEntry &GhostLogEntryFor(int guid)
{
	std::map<int, GhostLogEntry>::iterator const found = ghostLogEntries.find(guid);

	if (found != ghostLogEntries.end())
	{
		return found->second;
	}

	GhostLogEntry fresh;

	memset(&fresh, 0, sizeof(fresh));
	fresh.lastSequence = -1;
	return ghostLogEntries.insert(std::make_pair(guid, fresh)).first->second;
}

// Called for each message: sent on the host, and arrived on the joiner
void Hudson_GhostLogMessage(NETMESSAGE_NPCSTATE const *message)
{
	if (!HudsonRunIsOn || !plan.ghostLog)
	{
		return;
	}

	GhostLogEntry &entry = GhostLogEntryFor(message->Guid);

	entry.message = *message;
	entry.messages++;
	entry.messageAt = timeGetTime();
}

void Hudson_GhostLogDeath(int guid, int deathCode, int deathTime, int gibbFactor)
{
	if (!HudsonRunIsOn || !plan.ghostLog)
	{
		return;
	}

	char line[160];

	_snprintf(line, sizeof(line) - 1, "ghostlog: t=%lu id=%d death=1 code=%d time=%d gibb=%d", (unsigned long)timeGetTime(), guid, deathCode, deathTime, gibbFactor);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

// The number the messages use for this block, or false if this isn't one the log is about. A real NPC and its corpse
// have it in their name, and a ghost holds it as the other machine's object number.
static bool GhostLogNumberOf(STRATEGYBLOCK const *block, int *guid, bool *alive, HMODELCONTROLLER **model, int *cloak)
{
	extern HMODELCONTROLLER *Npc_HModelController(STRATEGYBLOCK *sbPtr);

	if (block->I_SBtype == I_BehaviourNetGhost)
	{
		NETGHOSTDATABLOCK *const ghost = (NETGHOSTDATABLOCK *)block->SBdataptr;
		bool const isACorpse = ghost->type == I_BehaviourNetCorpse;

		// An alien's ghost has older messages of its own, so its lines have no message in them
		AVP_BEHAVIOUR_TYPE const kind = isACorpse ? (AVP_BEHAVIOUR_TYPE)ghost->subtype : ghost->type;

		if (!NpcGhosts_TypeIsAnNpc(kind) && kind != I_BehaviourAlien)
		{
			return false;
		}

		*guid = ghost->playerObjectId;
		*alive = !isACorpse;
		*model = &ghost->HModelController;
		*cloak = ghost->CloakingEffectiveness;
		return true;
	}

	*guid = *((int const *)&block->SBname[4]);

	// Only one that's been sent, or an alien near enough to be. Anything else in the level has a name too, and a number that means nothing here.
	if (ghostLogEntries.find(*guid) == ghostLogEntries.end() && !(block->I_SBtype == I_BehaviourAlien && block->SBdptr))
	{
		return false;
	}

	if (block->I_SBtype == I_BehaviourNetCorpse)
	{
		*alive = false;
		*model = &((NETCORPSEDATABLOCK *)block->SBdataptr)->HModelController;
		*cloak = 0;
		return true;
	}

	if (!NpcGhosts_TypeIsAnNpc(block->I_SBtype) && block->I_SBtype != I_BehaviourAlien)
	{
		return false;
	}

	*alive = true;
	*model = Npc_HModelController(const_cast<STRATEGYBLOCK *>(block));
	*cloak = block->I_SBtype == I_BehaviourPredator ? ((PREDATOR_STATUS_BLOCK const *)block->SBdataptr)->CloakingEffectiveness : 0;
	return true;
}

static void Hudson_GhostLogEachFrame()
{
	extern DISPLAYBLOCK *OnScreenBlockList[];
	extern int NumOnScreenBlocks;

	if (!plan.ghostLog)
	{
		return;
	}

	DWORD const now = timeGetTime();
	static std::map<int, DWORD> seenAt;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];
		int guid = 0, cloak = 0;
		bool alive = false;
		HMODELCONTROLLER *model = NULL;

		if (!block || !block->SBdataptr || !block->DynPtr || !GhostLogNumberOf(block, &guid, &alive, &model, &cloak))
		{
			continue;
		}

		GhostLogEntry &entry = GhostLogEntryFor(guid);
		int const sequence = model ? model->Sequence_Type : -1;
		int const subSequence = model ? model->Sub_Sequence : -1;
		int const beams = NpcGhosts_XenoBeamsOf(block);
		bool const changed = !entry.here || beams != entry.lastBeams || sequence != entry.lastSequence || subSequence != entry.lastSubSequence || alive != entry.lastAlive || (cloak != 0) != entry.lastCloaked;

		seenAt[guid] = now;

		if (!changed && now - entry.lastLine < 1000)
		{
			continue;
		}

		bool onMyScreen = false;

		for (int j = 0; j < NumOnScreenBlocks && block->SBdptr; j++)
		{
			onMyScreen = onMyScreen || OnScreenBlockList[j] == block->SBdptr;
		}

		DYNAMICSBLOCK const *const body = block->DynPtr;
		NETMESSAGE_NPCSTATE const &sent = entry.message;
		char line[460];

		_snprintf(line, sizeof(line) - 1, "ghostlog: t=%lu id=%d type=%d alive=%d x=%d y=%d z=%d yaw=%d seq=%d sub=%d seqtime=%d cloak=%d seqok=%d beams=%d health=%d drawn=%d screen=%d age=%lu sent=%d mx=%d my=%d mz=%d myaw=%d mseq=%d msub=%d mcloak=%d mflags=%d mmodel=%d mkind=%d",
			(unsigned long)now, guid, (int)block->I_SBtype, alive ? 1 : 0, body->Position.vx, body->Position.vy, body->Position.vz, body->OrientEuler.EulerY,
			sequence, subSequence, model ? model->sequence_timer : -1, cloak, model && !HModelSequence_Exists(model, sequence, subSequence) ? 0 : 1, beams, block->SBDamageBlock.Health >> 16, block->SBdptr ? 1 : 0, onMyScreen ? 1 : 0,
			entry.messages ? (unsigned long)(now - entry.messageAt) : 0UL, entry.messages,
			(int)sent.xPos, (int)sent.yPos, (int)sent.zPos, (int)sent.yOrient, (int)sent.sequence_type, (int)sent.sub_sequence, (int)sent.extra1, (int)sent.flags, (int)sent.model, (int)sent.kind);
		line[sizeof(line) - 1] = 0;
		LogString(line);

		entry.lastLine = now;
		entry.lastSequence = sequence;
		entry.lastSubSequence = subSequence;
		entry.lastAlive = alive;
		entry.lastCloaked = cloak != 0;
		entry.lastBeams = beams;
		entry.here = true;
	}

	// One that was here and isn't now
	for (std::map<int, GhostLogEntry>::iterator it = ghostLogEntries.begin(); it != ghostLogEntries.end(); ++it)
	{
		if (it->second.here && seenAt[it->first] != now)
		{
			char line[80];

			it->second.here = false;
			_snprintf(line, sizeof(line) - 1, "ghostlog: t=%lu id=%d gone=1", (unsigned long)now, it->first);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
	}
}

// A fight's lines come from the player's input, and a dead player has none, so the lines just stop. This says what happened, once each time this player goes from alive to dead.
static void Hudson_BotFightSayWhenThePlayerDies(LONG framesSoFar)
{
	static bool wasAlive = false;

	if ((plan.botFight == kHudsonBotFightNone && plan.rateTest == kHudsonRateTestNone) || !Player || !Player->ObStrategyBlock || !Player->ObStrategyBlock->SBdataptr)
	{
		return;
	}

	bool const isAlive = ((PLAYER_STATUS *)Player->ObStrategyBlock->SBdataptr)->IsAlive != 0;

	if (wasAlive && !isAlive)
	{
		char line[128];

		_snprintf(line, sizeof(line) - 1, "botfight: this player has died, at frame %ld", framesSoFar);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	wasAlive = isAlive;
}

// look=: Whether a fight aimed this player on this frame, so that the look leaves him alone.
static bool aimedThisFrame = false;

void Hudson_EndOfFrame()
{
	HoldToTheFrameRate();
	NoteHowFastThisFrameWent();

	LONG const done = InterlockedIncrement(&framesDone);

	Hudson_BotFightSayWhenThePlayerDies(done);
	Hudson_GhostLogEachFrame();

	// Each level says what it holds, once it has settled: see HudsonStrategies_SayTheCounts. A campaign's next level says so again.
	{
		static int levelWeSaidItFor = -1;
		static LONG framesAtTheLevelsStart = 0;

		if (levelWeSaidItFor != netGameData.coopCampaignLevel)
		{
			levelWeSaidItFor = netGameData.coopCampaignLevel;
			framesAtTheLevelsStart = done;
		}

		if (done - framesAtTheLevelsStart == kFramesBeforeTheCounts)
		{
			HudsonStrategies_SayTheCounts();
		}

		HudsonStrategies_NoteTheCrowd();
	}

	// On the first frame, before anything has had time to wander off or be made
	if (plan.meets && done == 1)
	{
		SayWhatThePlayerMeets("meets: ");
	}

	if (plan.meets)
	{
		SayIfHisAmmunitionChanged();
	}

	DieIfItIsTime();
	ReachTheEndIfItIsTime();

	// After dying, in single player or in the mode that plays like it: the same lines, for comparing with the first
	if (levelStartedAgain)
	{
		levelStartedAgain = false;
		Hudson_SmokeTestStartsAgain();

		if (plan.meets)
		{
			SayWhatThePlayerMeets("meets-after-dying: ");

			// With someone else in the game it goes on for a while: the other machine has to start again too and say
			// so, and each should see the other back in the level. The one who joined stops first, so that neither
			// run is cut short by the other leaving.
			endAfterLookingAt = GetTickCount() + (plan.netMode == kHudsonNetHost ? 15000 : plan.netMode == kHudsonNetJoin ? 8000 : 0);
		}
	}

	if (endAfterLookingAt != 0 && GetTickCount() >= endAfterLookingAt)
	{
		endAfterLookingAt = 0;
		EndTheLevel("the level started again after he died, and it's been looked at again", kHudsonExitFinished);
	}

	// On the next level of a campaign, once it's really here. The number changes as the old one stops, a frame before
	// that level is unloaded and long before the new one is.
	{
		static int campaignLevelSeen = -1;

		if (plan.meets && netGameData.coopCampaign != kCampaignNone && netGameData.coopCampaignLevel != campaignLevelSeen && AvP.MainLoopRunning && !Coop_LevelChangeIsDue())
		{
			if (campaignLevelSeen >= 0)
			{
				SayWhatThePlayerMeets("meets-on-next-level: ");
			}

			campaignLevelSeen = netGameData.coopCampaignLevel;
		}
	}

	if (GetForegroundWindow() == hWndMain)
	{
		InterlockedIncrement(&framesWithTheFocus);
	}

	if (levelHasEnded)
	{
		return;
	}

	// Through the same call as the key. Every frame, in case anything turned it off, or on.
	{
		bool const wanted = plan.lampOn || (plan.lampBlink && (GetTickCount() / 1500) % 2 == 0);

		if (wanted != ShoulderLamp_IsOn())
		{
			ShoulderLamp_Toggle();
		}

		ShoulderLamp_SetRealLightWanted(plan.lampReal);

		if (plan.lampWide)
		{
			ShoulderLamp_SetPlaceFromEyes(kHudsonLampWideAcross, kHudsonLampWideUp, 0.0f);
		}
	}

	if (plan.showWindow)
	{
		// It's asked again once the level is up, because whatever started the game may have come back in front while it loaded. After that it's left alone, because it may be someone's choice.
		if (done == 1)
		{
			Hudson_BringTheWindowForward();
		}

		if (done == 1 || done == 60 || done == 600 || done == 6000)
		{
			SayWhereTheWindowIs(done);
		}
	}

	// Every frame, in case anything in the game has its own idea
	if (plan.brightWorld && AvP.Network == I_No_Network)
	{
		DrawFullBright = 1;
	}

	InterlockedExchange(&lightingDrawn, LightingReallyDrawn());
	InterlockedExchange(&shadowsDrawn, ShadowVolumesOn ? 1 : 0);
	InterlockedExchange(&staticFramesWithWalls, StaticScene_FramesWithWallsThisLevel());
	InterlockedExchange(&staticFramesAsked, StaticScene_FramesAskedThisLevel());

	if (done == 1 && !GotTheModeItAskedFor())
	{
		EndTheLevel("this machine can't draw the mode it was asked for", kHudsonExitModeNotThere);
		return;
	}

	if (HudsonMarineIsOn)
	{
		WatchTheMarine(done);

		if (!levelHasEnded && done >= plan.frames)
		{
			EndTheLevel(plan.combatAlien ? HowTheFightRanOut() : "frames= ran out before he'd finished", kHudsonExitOutOfFrames);
		}
	}
	else if (done >= plan.frames)
	{
		EndTheLevel("it finished", kHudsonExitFinished);
	}
}

bool Hudson_NetIsOn()
{
	return HudsonRunIsOn && plan.netMode != kHudsonNetNone;
}


// coopwalk=yes: what ends each level, from tools/hudson/chains. A press happens on one machine, in turn, the way a player does it. A walk happens on both, as each tests the other's body itself. The log says what each step's object looks like on this machine, so the two logs can be compared. This fires the links: it doesn't play the level.
struct HudsonWalkStep
{
	std::string kind;
	std::string name;
	std::string why;
	std::vector<std::string> where;     // the rooms this step's thing is in
	std::vector<std::string> nearRooms; // the rooms it's only within 2 m of
};

static std::vector<HudsonWalkStep> walkSteps;
static size_t walkAt = 0;
static DWORD walkNextAt = 0;
static bool walkEnded = false;
static DWORD walkWaitingSince = 0;

static void Hudson_NetLoadWalk()
{
	walkSteps.clear();
	walkAt = 0;
	walkEnded = false;
	walkWaitingSince = 0;
	walkNextAt = GetTickCount() + 8000;

	std::string const path = std::string("HudsonChains\\") + LevelName + ".txt";

	// Which rooms join which. Without one, a step can't be checked, and the log says so.
	HudsonStrategies_LoadRoomList((std::string("HudsonChains\\") + LevelName + ".rooms").c_str());

	FILE *const file = fopen(path.c_str(), "r");

	if (!file)
	{
		LogString("walk: there's no chain file for this level: " + path);
		return;
	}

	// A room's name can be long, and these lines can name many rooms
	char text[4000];
	std::vector<std::string> nextWhere;
	std::vector<std::string> nextNear;

	while (fgets(text, sizeof(text), file))
	{
		std::string line = text;

		while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
		{
			line.pop_back();
		}

		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		std::string::size_type const space = line.find(' ');

		if (space == std::string::npos)
		{
			continue;
		}

		// These two belong to the next step, and aren't steps
		std::string const firstWord = line.substr(0, space);

		if (firstWord == "where")
		{
			nextWhere = CoopReach_SplitNames(line.substr(space + 1));
			continue;
		}

		if (firstWord == "near")
		{
			nextNear = CoopReach_SplitNames(line.substr(space + 1));
			continue;
		}

		HudsonWalkStep step;

		step.kind = firstWord;
		step.name = line.substr(space + 1);
		step.where = nextWhere;
		step.nearRooms = nextNear;
		nextWhere.clear();
		nextNear.clear();

		std::string::size_type const bar = step.name.find(" | ");

		if (bar != std::string::npos)
		{
			step.why = step.name.substr(bar + 3);
			step.name = step.name.substr(0, bar);
		}

		walkSteps.push_back(step);
	}

	fclose(file);

	char line[120];

	_snprintf(line, sizeof(line) - 1, "walk: this level's chain has %d steps", (int)walkSteps.size());
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

static void Hudson_NetWalk()
{
	DWORD const now = GetTickCount();

	if (walkEnded || now < walkNextAt)
	{
		return;
	}

	walkNextAt = now + 2000;

	// A switch door is only open for about 4 seconds, so this comes before anything below can return
	HudsonStrategies_SpreadReach();

	// Chains have timers in them, and lifts that take a while, so the level gets this long to end by itself before this says it hasn't
	static const DWORD kWaitForAChainToFinish = 30000;
	static DWORD allFiredAt = 0;

	if (walkAt >= walkSteps.size() && allFiredAt == 0)
	{
		allFiredAt = now;
		LogString("walk: every step has been fired, and the level has 30 seconds to end by itself");
		return;
	}

	if (walkAt >= walkSteps.size() && now - allFiredAt < kWaitForAChainToFinish)
	{
		return;
	}

	if (walkAt >= walkSteps.size())
	{
		allFiredAt = 0;
		// The chain has been fired, and the level is still here
		walkEnded = true;
		LogString("walk: it hasn't ended by itself");

		if (plan.netMode == kHudsonNetHost)
		{
			LogString("walk: so the test ended it: this level did NOT finish by its own chain");
			AvP.LevelCompleted = 1;
		}

		return;
	}

	HudsonWalkStep const &step = walkSteps[walkAt];
	int const number = (int)walkAt;
	STRATEGYBLOCK *const target = HudsonStrategies_Find(step.name.c_str());
	bool const myTurn = (number % 2 == 0) == (plan.netMode == kHudsonNetHost);
	std::string what = "walk: step " + std::to_string(number) + " " + step.kind + " " + step.name;

	walkAt++;

	if (step.kind == "missing")
	{
		LogString(what + ": can't happen here: " + step.why);
		return;
	}

	if (step.kind == "self-running")
	{
		LogString(what + ": nothing to fire, it runs by itself");
		return;
	}

	if (step.kind == "end")
	{
		LogString(what + ": not fired: this is a check that the level ends by itself");
		return;
	}

	// Could a player have got here, through doors open on this machine? It only knows which rooms join and which doors are open, so its words are "a route exists", never that anyone walked. A level stopped by this is a different result from one whose chain didn't end.
	if (step.where.empty() && step.nearRooms.empty())
	{
		LogString(what + ": no where line");
	}
	else
	{
		bool routeExists = false;
		std::string const words = HudsonStrategies_AskReach(step.where, step.nearRooms, &routeExists);

		// A step fires 2 seconds after the one before, and what that one opens can take longer: chains have timers and lifts in
		// them, and the other machine hears a press a moment late. A player would wait for the door, so this does too, and it
		// only stops once the door has had as long as a chain gets to finish. It counts from the first time this step had no route.
		if (!routeExists && words != "no room list" && walkWaitingSince == 0)
		{
			walkWaitingSince = now;
			LogString(what + ": " + words + ", so it waits up to 30 seconds for that");
		}

		if (!routeExists && words != "no room list" && now - walkWaitingSince < kWaitForAChainToFinish)
		{
			walkAt--;
			return;
		}

		walkWaitingSince = 0;
		LogString(what + ": " + words);

		if (!routeExists && words != "no room list")
		{
			walkEnded = true;
			LogString("walk: a player couldn't get here on this machine, so the walk stops: " + words);

			if (plan.netMode == kHudsonNetHost)
			{
				LogString("walk: so the test ended it: this level stopped at a step no route reaches");
				AvP.LevelCompleted = 1;
			}

			return;
		}
	}

	if (!target)
	{
		LogString(what + ": nothing with that name on this machine");
		return;
	}

	std::string const before = HudsonStrategies_State(target);
	bool fired = false;

	if (step.kind == "press")
	{
		if (myTurn)
		{
			AddNetMsg_LOSRequestBinarySwitch(target);
			RequestState(target, 1, 0);
			fired = true;
		}
	}
	else if (step.kind == "walk")
	{
		RequestState(target, 1, 0);
		fired = true;
	}
	else if (step.kind == "pickup")
	{
		if (myTurn && target->I_SBtype == I_BehaviourInanimateObject)
		{
			RemovePickedUpObject(target);
			fired = true;
		}
	}
	else if (step.kind == "destroy" || step.kind == "kill")
	{
		if (plan.netMode == kHudsonNetHost)
		{
			CauseDamageToObject(target, &TemplateAmmo[AMMO_PLAYER_SHOTGUN].MaxDamage[AvP.Difficulty], ONE_FIXED * 1000, NULL);
			fired = true;
		}
	}

	LogString(what + (fired ? ": fired here, was " : ": left to the other machine, is ") + before);
}


extern ENetHost *HudsonTest_TheConnection();

// The bytes of a datagram ENet would read as a fragment of a huge unreliable message. It's what the second independent read of this code described: a small datagram that used to make the other end set aside 131 KB. The game's copy of ENet now refuses it, and a hostile run counts on that.
static void HudsonTest_SendBadFragments(int count)
{
	ENetHost *const host = HudsonTest_TheConnection();

	if (!host)
	{
		return;
	}

	ENetPeer *peer = NULL;

	// A host's peers are its clients, and a client's only one is its host
	for (size_t i = 0; !peer && i < host->peerCount; i++)
	{
		if (host->peers[i].state == ENET_PEER_STATE_CONNECTED)
		{
			peer = &host->peers[i];
		}
	}

	if (!peer)
	{
		return;
	}

#pragma pack(push, 1)
	struct BadDatagram
	{
		enet_uint16 peerID;
		enet_uint32 checksum;
		ENetProtocolSendFragment command;
		unsigned char oneByte;
	};
#pragma pack(pop)

	for (int i = 0; i < count; i++)
	{
		BadDatagram bad;
		ENetBuffer buffer;

		memset(&bad, 0, sizeof(bad));
		bad.peerID = ENET_HOST_TO_NET_16((enet_uint16)(peer->outgoingPeerID | ((enet_uint16)peer->outgoingSessionID << ENET_PROTOCOL_HEADER_SESSION_SHIFT)));
		bad.checksum = peer->connectID;
		bad.command.header.command = ENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT;
		// One window ahead of what's been sent on this channel. Behind it, ENet would throw this away without looking any further, and a test would pass having tried nothing.
		bad.command.header.reliableSequenceNumber = ENET_HOST_TO_NET_16((enet_uint16)(peer->channels[0].outgoingReliableSequenceNumber + 4096));
		bad.command.startSequenceNumber = ENET_HOST_TO_NET_16((enet_uint16)i);
		bad.command.dataLength = ENET_HOST_TO_NET_16(1);
		bad.command.fragmentCount = ENET_HOST_TO_NET_32(ENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT);
		bad.command.totalLength = ENET_HOST_TO_NET_32((enet_uint32)kMostMessageBytes);

		buffer.data = &bad;
		buffer.dataLength = sizeof(bad);
		bad.checksum = enet_crc32(&buffer, 1);
		enet_socket_send(host->socket, &peer->address, &buffer, 1);
	}
}

// How much memory this game holds, and how many of its blocks are in use. Some rows have to leave the first about where it was, and one has to take the second to the limit.
extern int NumActiveBlocks;

static void Hudson_NetHostileLogState(DWORD now)
{
	static DWORD lastLine = 0;

	if (now - lastLine < 2000)
	{
		return;
	}

	PROCESS_MEMORY_COUNTERS_EX memory;
	char line[120];

	lastLine = now;
	memset(&memory, 0, sizeof(memory));
	memory.cb = sizeof(memory);

	if (K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&memory, sizeof(memory)))
	{
		LogString("hostile: memory " + std::to_string((unsigned long long)memory.PrivateUsage / 1024) + " KB");
	}

	_snprintf(line, sizeof(line) - 1, "hostile: blocks %d of %d strategy, %d of %d display", NumActiveStBlocks, maxstblocks, NumActiveBlocks, maxobjects);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

// The kinds of hostile run that aren't a list of rows. See HudsonHostileKind.
static void Hudson_NetHostileOtherKinds(STRATEGYBLOCK *me, STRATEGYBLOCK *other, DWORD now)
{
	static DWORD startedAt = 0;
	static DWORD lastBatch = 0;
	static int made = 0;
	static bool killed = false;
	static bool finished = false;

	Hudson_NetHostileLogState(now);

	if (startedAt == 0)
	{
		if (!other)
		{
			return;
		}

		startedAt = now;
		LogString(std::string("hostile: kind ") + (plan.hostileKind == kHudsonHostileDrain ? "drain" : plan.hostileKind == kHudsonHostileWatch ? "watch" : plan.hostileKind == kHudsonHostileHold ? "hold" : "flood"));
	}

	DWORD const since = now - startedAt;

	if (finished)
	{
		return;
	}

	if (plan.hostileKind == kHudsonHostileWatch)
	{
		// In a drain the blocks are at their lowest by now. The player's corpse has to come out of what's kept back.
		if (!killed && since >= 45000 && me)
		{
			killed = true;
			LogString("hostile: row SURVIVE: this player is being killed, at the most blocks in use");
			CauseDamageToObject(me, &TemplateAmmo[AMMO_PLAYER_SHOTGUN].MaxDamage[AvP.Difficulty], ONE_FIXED * 1000, NULL);
		}

		if (since >= 90000)
		{
			finished = true;
			Hudson_NetSay("the host has seen it through, and it's still running");
			EndTheLevel("the hostile rows have all gone", kHudsonExitFinished);
		}

		return;
	}

	if (plan.hostileKind == kHudsonHostileDrain && me)
	{
		// Every tenth of a second: 60 new ghosts a second, 20 big explosions, and 20 dropped guns, of which the limit lets 16 through. What gets made stays inside every limit.
		if (since >= 5000 && since < 60000 && now - lastBatch >= 100)
		{
			VECTORCH const at = me->DynPtr->Position;

			lastBatch = now;

			NETMESSAGE_LOBSTATE ghost;
			memset(&ghost, 0, sizeof(ghost));
			ghost.xPos = at.vx;
			ghost.yPos = at.vy;
			ghost.zPos = at.vz;
			ghost.type = I_BehaviourGrenade;

			for (int i = 0; i < 6; i++)
			{
				ghost.objectId = 100000 + made++;
				HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
			}

			NETMESSAGE_CREATEWEAPON gun;
			memset(&gun, 0, sizeof(gun));
			gun.location = at;
			gun.type = WEAPON_PULSERIFLE;

			for (int i = 0; i < 2; i++)
			{
				int const number = made++;

				memcpy(gun.name, &number, sizeof(number));
				gun.name[4] = 'H';
				HudsonTest_AddRawPart(NetMT_CreateWeapon, &gun, sizeof(gun));
			}

			NETMESSAGE_MAKEEXPLOSION explosion;
			memset(&explosion, 0, sizeof(explosion));
			explosion.Position = at;
			explosion.ExplosionID = EXPLOSION_HUGE;
			HudsonTest_AddRawPart(NetMT_MakeExplosion, &explosion, sizeof(explosion));
			HudsonTest_AddRawPart(NetMT_MakeExplosion, &explosion, sizeof(explosion));
		}

		if (since >= 70000)
		{
			finished = true;
			Hudson_NetSay("the joiner has drained what it could, and it's leaving");
			EndTheLevel("the hostile rows have all gone", kHudsonExitFinished);
		}

		return;
	}

	if (plan.hostileKind == kHudsonHostileHold)
	{
		// Nothing here answers the network while this sleeps, as if the machine had hung
		if (!killed && since >= 30000)
		{
			killed = true;
			LogString("hostile: row HOLD: this game stops answering now");
			Sleep(40000);
			LogString("hostile: row HOLD: this game is answering again");
			finished = true;
			EndTheLevel("the hostile rows have all gone", kHudsonExitFinished);
		}

		return;
	}

	if (plan.hostileKind == kHudsonHostileFlood)
	{
		// As much as a client may send, and more. Each message is full of a harmless part, for the host to pass on.
		if (since >= 5000 && since < 55000)
		{
			static unsigned char filler[3000];
			static int fillerSize = 0;

			if (fillerSize == 0)
			{
				NETMESSAGEHEADER header;
				NETMESSAGE_LEVELEVENT ask;

				memset(&header, 0, sizeof(header));
				header.type = (unsigned char)NetMT_LevelEventAsk;
				memset(&ask, 'Z', sizeof(ask));

				while (fillerSize + (int)(sizeof(header) + sizeof(ask)) <= (int)sizeof(filler))
				{
					memcpy(filler + fillerSize, &header, sizeof(header));
					memcpy(filler + fillerSize + sizeof(header), &ask, sizeof(ask));
					fillerSize += (int)(sizeof(header) + sizeof(ask));
				}
			}

			for (int i = 0; i < 4; i++)
			{
				Net_Send(NET_ID_ALLPLAYERS, AvPNetID, 0, filler, (size_t)fillerSize);
			}
		}

		if (since >= 65000)
		{
			finished = true;
			Hudson_NetSay("the flood is over, and this game is still running");
			EndTheLevel("the hostile rows have all gone", kHudsonExitFinished);
		}
	}
}

// hostile=yes, after the rows: the game behaves as if every block has gone, the player is killed, and the blocks come back. Each game does this by its own clock, starting when it first sees the other, and the two overlap. So each one's own death and the other's are both met with nothing left. Each game then stays up, to show it's still running, and the host stays longest, so it also sees the joiner go. Running out must never crash.
extern bool HudsonTest_NoStrategyBlocks;
extern bool HudsonTest_NoDisplayBlocks;
extern int TextReport_LinesStraightOn;
extern int TextReport_LinesWaited;
bool Hudson_NetIsCampaign();
extern void NewOnScreenMessage(char *messagePtr);
extern bool HudsonTest_NoLightBlocks;
extern STRATEGYBLOCK *CreateGrenadeKernel(AVP_BEHAVIOUR_TYPE behaviourID, VECTORCH *position, MATRIXCH *orient, int fromplayer);

static void Hudson_NetHostileTimeline(STRATEGYBLOCK *me, bool otherIsHere, DWORD now)
{
	static DWORD startedAt = 0;
	static int done = 0;
	bool const isHost = plan.netMode == kHudsonNetHost;
	bool const campaign = Hudson_NetIsCampaign();

	if (startedAt == 0)
	{
		if (otherIsHere)
		{
			startedAt = now;
		}

		return;
	}

	DWORD const since = now - startedAt;

	// A step that kills this player waits until there's a player to kill
	if (done == 0 && since >= 74000)
	{
		done = 1;
		HudsonTest_NoDisplayBlocks = true;
		LogString("hostile: row SURVIVE: every display block has gone on this machine");
	}
	else if (done == 1 && since >= (isHost ? 78000u : 76000u) && (me || (campaign && isHost)))
	{
		// In the campaign mode a dead player stays out, and the level restarts once everyone is, so there only the joiner dies here and only the host below
		done = 2;

		if (!campaign || !isHost)
		{
			LogString("hostile: row SURVIVE: this player is being killed with no display blocks");
			CauseDamageToObject(me, &TemplateAmmo[AMMO_PLAYER_SHOTGUN].MaxDamage[AvP.Difficulty], ONE_FIXED * 1000, NULL);
		}
	}
	else if (done == 2 && since >= 88000)
	{
		done = 3;
		HudsonTest_NoDisplayBlocks = false;
		HudsonTest_NoStrategyBlocks = true;
		LogString("hostile: row SURVIVE: every strategy block has gone on this machine");
	}
	else if (done == 3 && since >= (isHost ? 92000u : 90000u) && (me || (campaign && !isHost)))
	{
		done = 4;

		if (!campaign || isHost)
		{
			LogString("hostile: row SURVIVE: this player is being killed with no strategy blocks");
			CauseDamageToObject(me, &TemplateAmmo[AMMO_PLAYER_SHOTGUN].MaxDamage[AvP.Difficulty], ONE_FIXED * 1000, NULL);
		}
	}
	else if (done == 4 && since >= 102000)
	{
		done = 5;
		HudsonTest_NoStrategyBlocks = false;
		HudsonTest_NoLightBlocks = true;
		LogString("hostile: row SURVIVE: every light has gone on this machine");
		LocalDetailLevels.DrawLightCoronas = 1;
	}
	else if (done == 5 && since >= 104000 && (me || since >= 110000))
	{
		// This player's own flare, and one the other machine hears about, with no light for either
		done = 6;

		if (me)
		{
			CreateGrenadeKernel(I_BehaviourFlareGrenade, &me->DynPtr->Position, &me->DynPtr->OrientMat, 1);
			LogString("hostile: row SURVIVE: a flare has been thrown with no lights left");
		}
		else
		{
			LogString("hostile: row SURVIVE: NOT SENT: this player was still dead, so no flare was thrown");
		}
	}
	else if (done == 6 && since >= 112000)
	{
		done = 7;
		HudsonTest_NoLightBlocks = false;
		LogString("hostile: row SURVIVE: everything is back, and this game is still running");
		LogString("hostile: lines for the screen: " + std::to_string(TextReport_LinesStraightOn) + " went straight on, and " + std::to_string(TextReport_LinesWaited) + " waited");
	}
	else if (done == 7 && since >= (isHost ? 132000u : 120000u))
	{
		done = 8;
		Hudson_NetSay("every bad message has gone, and this game is still running");
		EndTheLevel("the hostile rows have all gone", kHudsonExitFinished);
	}
}

// hostile=yes: one bad message every two seconds, once both games are playing. Each row says what it is, and the other game's log has to name why it was refused. What a row expects is in tools/hudson/hostile-rows.txt, which the script compares.
static void Hudson_NetHostile(STRATEGYBLOCK *me, STRATEGYBLOCK *other, DWORD now)
{
	static DWORD nextAt = 0;
	static int row = 0;

	if (plan.hostileKind != kHudsonHostileRows)
	{
		Hudson_NetHostileOtherKinds(me, other, now);
		return;
	}

	Hudson_NetHostileTimeline(me, other != NULL, now);

	Hudson_NetHostileLogState(now);

	if (!other)
	{
		return;
	}

	if (nextAt == 0)
	{
		nextAt = now + 8000;
	}

	if (now < nextAt)
	{
		return;
	}

	nextAt = now + 2000;

	bool const isHost = plan.netMode == kHudsonNetHost;
	VECTORCH const at = me->DynPtr->Position;
	char line[100];
	char const *said = 0;

	NETMESSAGE_LOBSTATE ghost;
	memset(&ghost, 0, sizeof(ghost));
	ghost.xPos = at.vx;
	ghost.yPos = at.vy;
	ghost.zPos = at.vz;

	// A switch or a light, by the name the game finds things with
	char const *notAPickup = 0;

	for (int i = 0; i < NumActiveStBlocks && !notAPickup; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (block && (block->I_SBtype == I_BehaviourBinarySwitch || block->I_SBtype == I_BehaviourPlacedLight))
		{
			notAPickup = block->SBname;
		}
	}

	if (!isHost)
	{
		switch (row)
		{
			case 0:
				if (notAPickup)
				{
					NETMESSAGE_OBJECTPICKEDUP message;
					memcpy(message.name, notAPickup, sizeof(message.name));
					HudsonTest_AddRawPart(NetMT_ObjectPickedUp, &message, sizeof(message));
					said = "B1 a pick-up message naming a switch or a light";
				}
				else
				{
					said = "B1 NOT SENT: nothing in this level to name";
				}
				break;
			case 1:
				ghost.objectId = GHOST_PLAYEROBJECTID;
				ghost.type = I_BehaviourRocket;
				HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
				said = "B2 a ghost message using the number of a player's body";
				break;
			case 2:
				ghost.objectId = 7001;
				ghost.type = 250;
				HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
				said = "B2 a ghost message of a kind that isn't on the list";
				break;
			case 3:
			{
				// Header, then the multiple. This is the host's body, as a client would hit it.
				unsigned char buffer[sizeof(NETMESSAGE_LOBDAMAGED_HEADER) + sizeof(NETMESSAGE_DAMAGE_MULTIPLE)];
				NETMESSAGE_LOBDAMAGED_HEADER *const header = (NETMESSAGE_LOBDAMAGED_HEADER *)buffer;
				memset(buffer, 0, sizeof(buffer));
				header->playerId = netGameData.playerData[0].playerId;
				header->objectId = GHOST_PLAYEROBJECTID;
				header->ammo_id = AMMO_10MM_CULW;
				header->multiple = 1;
				((NETMESSAGE_DAMAGE_MULTIPLE *)(buffer + sizeof(NETMESSAGE_LOBDAMAGED_HEADER)))->multiple = -65536;
				HudsonTest_AddRawPart(NetMT_LocalObjectDamaged, buffer, sizeof(buffer));
				said = "B4 a damage message with a negative multiple";
				break;
			}
			case 4:
			{
				NETMESSAGE_PLAYERKILLED message;
				memset(&message, 0, sizeof(message));
				message.objectId = 123456;
				message.killerId = AvPNetID;
				message.myType = NGCT_Marine;
				message.killerType = (NETGAME_CHARACTERTYPE)4;
				HudsonTest_AddRawPart(NetMT_PlayerKilled, &message, sizeof(message));
				said = "B5 a kill by a killer kind past the species scores";
				break;
			}
			case 5:
				for (int i = 0; i < 90; i++)
				{
					ghost.objectId = 8000 + i;
					ghost.type = I_BehaviourGrenade;
					HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
				}
				said = "B6 ninety new ghosts in one go";
				break;
			case 6:
				for (int i = 0; i < 40; i++)
				{
					NETMESSAGE_CREATEWEAPON message;
					memset(&message, 0, sizeof(message));
					message.name[0] = 'H';
					message.name[1] = (char)(1 + i);
					message.location = at;
					message.type = WEAPON_PULSERIFLE;
					HudsonTest_AddRawPart(NetMT_CreateWeapon, &message, sizeof(message));
				}
				said = "B6 forty dropped guns in one go";
				break;
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			{
				static int const kinds[] = { NetMT_PlayerAutoGunState, NetMT_AlienAIState, NetMT_AlienAISequenceChange, NetMT_AlienAIKilled, NetMT_SpotAlienSound, NetMT_FarAlienPosition, NetMT_CoopNextLevel, NetMT_LevelEvent };
				static int const sizes[] = { sizeof(NETMESSAGE_AGUNSTATE), sizeof(NETMESSAGE_ALIENAISTATE), sizeof(NETMESSAGE_ALIENSEQUENCECHANGE), sizeof(NETMESSAGE_ALIENAIKILLED), sizeof(NETMESSAGE_SPOTALIENSOUND), sizeof(NETMESSAGE_FARALIENPOSITION), sizeof(NETMESSAGE_COOPNEXTLEVEL), sizeof(NETMESSAGE_LEVELEVENT) };
				unsigned char blank[256] = { 0 };

				HudsonTest_AddRawPart(kinds[row - 7], blank, sizes[row - 7]);
				_snprintf(line, sizeof(line) - 1, "C4 a part only a host may send, kind %d", kinds[row - 7]);
				line[sizeof(line) - 1] = 0;
				said = line;
				break;
			}
			case 15:
			{
				NETMESSAGE_LEVELEVENT message;
				memset(&message, 'Z', sizeof(message));
				HudsonTest_AddRawPart(NetMT_LevelEventAsk, &message, sizeof(message));
				said = "NEW asking for an event no such object has";
				break;
			}
			case 16:
				if (notAPickup)
				{
					NETMESSAGE_LEVELEVENT message;
					memcpy(message.source, notAPickup, sizeof(message.source));
					HudsonTest_AddRawPart(NetMT_LevelEventAsk, &message, sizeof(message));
					said = "NEW asking for an event by a switch or a light, not something picked up";
				}
				else
				{
					said = "NEW NOT SENT: nothing in this level to name";
				}
				break;
			case 17:
			{
				// A kind that's on the list, whose shape this level didn't load. Both games loaded the same level with the same species, so what's missing here is missing there.
				struct KindAndShape
				{
					AVP_BEHAVIOUR_TYPE type;
					char const *shape;
				};

				static KindAndShape const kinds[] =
				{
					{ I_BehaviourProximityGrenade, "Proxmine" },
					{ I_BehaviourAlienSpit, "Spit" },
					{ I_BehaviourSpeargunBolt, "spear" },
					{ I_BehaviourFlareGrenade, "Flare" },
					{ I_BehaviourFragmentationGrenade, "Frag" },
					{ I_BehaviourClusterGrenade, "Cluster" },
					{ I_BehaviourRocket, "missile" },
					{ I_BehaviourGrenade, "Shell" }
				};

				said = "B7 NOT SENT: every kind's shape is loaded in this level";

				for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
				{
					if (GetLoadedShapeMSL(kinds[i].shape) < 0)
					{
						ghost.objectId = 8500;
						ghost.type = kinds[i].type;
						HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
						said = "B7 a ghost whose shape this level hasn't loaded";
						break;
					}
				}
				break;
			}
			case 18:
			{
				NETMESSAGE_PLAYERKILLED message;
				memset(&message, 0, sizeof(message));
				message.objectId = GHOST_PLAYEROBJECTID;
				message.killerId = AvPNetID;
				message.myType = NGCT_Marine;
				message.killerType = NGCT_Marine;
				HudsonTest_AddRawPart(NetMT_PlayerKilled, &message, sizeof(message));
				said = "N2 a kill giving the corpse the number of a player's body";
				break;
			}
			case 19:
			{
				NETMESSAGE_MAKEEXPLOSION message;
				memset(&message, 0, sizeof(message));
				message.Position = at;
				for (int i = 0; i < 30; i++)
				{
					HudsonTest_AddRawPart(NetMT_MakeExplosion, &message, sizeof(message));
				}
				said = "B6 thirty explosions in one go";
				break;
			}
			case 20:
			{
				NETMESSAGE_GIBBING message;
				memset(&message, 0, sizeof(message));
				for (int i = 0; i < 20; i++)
				{
					HudsonTest_AddRawPart(NetMT_Gibbing, &message, sizeof(message));
				}
				said = "B6 twenty gibbings in one go";
				break;
			}
			case 21:
			{
				NETMESSAGE_PLAYERKILLED message;
				memset(&message, 0, sizeof(message));
				message.killerId = AvPNetID;
				message.myType = NGCT_Marine;
				message.killerType = NGCT_Marine;
				for (int i = 0; i < 3; i++)
				{
					message.objectId = 200000 + i;
					HudsonTest_AddRawPart(NetMT_PlayerKilled, &message, sizeof(message));
				}
				said = "B6 three kills in one go";
				break;
			}
			case 22:
			{
				unsigned char const chat[] = { 0, 'b', 'a', 'd', 0 };
				for (int i = 0; i < 10; i++)
				{
					HudsonTest_AddRawPart(NetMT_ChatBroadcast, chat, sizeof(chat));
				}
				said = "N5 ten chat lines in one go";
				break;
			}
			case 23:
			{
				// What the kill above was the start of: a sound holding an address in memory that's then freed. A test run may have no sound at all, so this uses an empty place in the list of sounds directly, and looks at what it holds afterwards.
				int place = 0;

				while (place < SOUND_MAXACTIVE && ActiveSounds[place].soundIndex != SID_NOSOUND)
				{
					place++;
				}

				if (place == SOUND_MAXACTIVE)
				{
					said = "N2 NOT SENT: there was no empty place in the list of sounds";
					break;
				}

				ACTIVESOUNDSAMPLE const saved = ActiveSounds[place];
				int *const block = (int *)AllocateMem(64);

				ActiveSounds[place].soundIndex = (SOUNDINDEX)1;
				ActiveSounds[place].externalRef = &block[3];
				DeallocateMem(block);

				said = ActiveSounds[place].externalRef == NULL ? "N2 a sound holding an address in freed memory forgot it" : "N2 NOT SENT: a sound STILL holds an address in freed memory";
				ActiveSounds[place] = saved;
				break;
			}
			case 24:
				HudsonTest_SendBadFragments(400);
				said = "N4 four hundred fragments of a huge message";
				break;
			case 25:
			{
				NETMESSAGE_LOBDESTROYED message;
				memset(&message, 0, sizeof(message));
				message.objectId = 5;
				for (int i = 0; i < 70; i++)
				{
					HudsonTest_AddRawPart(NetMT_LocalObjectDestroyed, &message, sizeof(message));
				}
				said = "B6 seventy things destroyed in one go";
				break;
			}
			case 26:
			{
				NETMESSAGE_MAKEEXPLOSION message;
				memset(&message, 0, sizeof(message));
				message.Position = at;
				message.ExplosionID = EXPLOSION_MOLOTOV;
				HudsonTest_AddRawPart(NetMT_MakeExplosion, &message, sizeof(message));
				said = "SEEDS an explosion no game sends";
				break;
			}
			case 27:
			{
				NETMESSAGE_MAKEFLECHETTEEXPLOSION message;
				memset(&message, 0, sizeof(message));
				message.Position = at;
				message.Seed = 0;
				HudsonTest_AddRawPart(NetMT_MakeFlechetteExplosion, &message, sizeof(message));
				said = "SEEDS a flechette with no seed";
				break;
			}
			case 28:
			{
				static char aLine[] = "bad";

				for (int i = 0; i < 80; i++)
				{
					NewOnScreenMessage(aLine);
				}
			}

				said = "N5 eighty lines for this machine's own screen";
				break;
			case 29:
			{
				// The real message for a hit on something breakable, with a hit as big as it may be, on every track object it finds. A track object can break in the campaign mode.
				int hits = 0;

				for (int i = 0; i < NumActiveStBlocks && hits < 10; i++)
				{
					if (ActiveStBlockList[i] && ActiveStBlockList[i]->I_SBtype == I_BehaviourTrackObject)
					{
						AddNetMsg_InanimateObjectDamaged(ActiveStBlockList[i], &TemplateAmmo[AMMO_PLAYER_SHOTGUN].MaxDamage[AvP.Difficulty], ONE_FIXED * 1000);
						hits++;
					}
				}

				_snprintf(line, sizeof(line) - 1, "NEW huge hits on track objects: %d", hits);
				line[sizeof(line) - 1] = 0;
				said = line;
				break;
			}
			case 30:
			{
				// A player's ghost on a level with no body for it. Some levels load only their own species' models. This asks the game to make one directly, because a joiner of another species isn't something the row above can send.
				VECTORCH place = at;
				EULER facing = { 0, 0, 0 };
				bool const noMarine = GetNamedHierarchyFromLibrary("hnpcmarine", "marine with pulse rifle") == NULL;
				bool const noPredator = GetNamedHierarchyFromLibrary("hnpcpredator", "pred with wristblade") == NULL;

				if (noMarine || noPredator)
				{
					CreateNetGhost(AvPNetID, 8600, &place, &facing, noMarine ? I_BehaviourMarinePlayer : I_BehaviourPredatorPlayer, IOT_Non, 0);
					said = "B3gap a player's ghost with no body on this level";
				}
				else
				{
					said = "B3gap NOT SENT: this level has both bodies";
				}
				break;
			}
			case 31:
			{
				NETMESSAGE_NPCSTATE message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9003;
				HudsonTest_AddRawPart(NetMT_NpcState, &message, sizeof(message));
				said = "NPC a joiner saying what an NPC is doing, which only the host may";
				break;
			}
			case 32:
				said = "END the joiner has sent everything, and it's still here";
				break;
			default:
				return;
		}
	}
	else
	{
		switch (row)
		{
			case 0:
				ghost.objectId = 9001;
				ghost.type = I_BehaviourRocket;
				HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
				said = "HOST makes a rocket's ghost, for the next row to name";
				break;
			case 1:
			{
				NETMESSAGE_ALIENSEQUENCECHANGE message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9001;
				message.sequence_length = -1;
				message.tweening_time = -1;
				HudsonTest_AddRawPart(NetMT_AlienAISequenceChange, &message, sizeof(message));
				said = "B2 an alien message naming a rocket's ghost";
				break;
			}
			case 2:
			{
				NETMESSAGE_ALIENAISTATE message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9002;
				message.xPos = at.vx;
				message.yPos = at.vy;
				message.zPos = at.vz;
				message.AlienType = 2;
				HudsonTest_AddRawPart(NetMT_AlienAIState, &message, sizeof(message));
				said = "B3 an alien of a kind this level may not have loaded";
				break;
			}
			case 3:
			{
				NETMESSAGE_COOPNEXTLEVEL message;
				message.campaign = 1;
				message.place = 200;
				HudsonTest_AddRawPart(NetMT_CoopNextLevel, &message, sizeof(message));
				said = "NEW a next level this game can't go to";
				break;
			}
			case 4:
			{
				NETMESSAGE_LEVELEVENT message;
				memset(&message, 'Z', sizeof(message));
				HudsonTest_AddRawPart(NetMT_LevelEvent, &message, sizeof(message));
				said = "NEW an event no such object has";
				break;
			}
			case 5:
				HudsonTest_AddDescriptionWithout(netGameData.playerData[1].playerId);
				said = "B8 a game description leaving the joiner off";
				break;
			case 6:
			{
				unsigned char const chat[] = { 0, 'b', 'a', 'd', 0 };
				for (int i = 0; i < 80; i++)
				{
					HudsonTest_AddRawPart(NetMT_ChatBroadcast, chat, sizeof(chat));
				}
				said = "N5 eighty chat lines in one go";
				break;
			}
			case 7:
			{
				NETMESSAGE_AGUNSTATE message;
				memset(&message, 0, sizeof(message));
				message.objectId = GHOST_PLAYEROBJECTID;
				message.xPos = at.vx;
				message.yPos = at.vy;
				message.zPos = at.vz;
				HudsonTest_AddRawPart(NetMT_PlayerAutoGunState, &message, sizeof(message));
				said = "N7 an autogun message naming a player's body";
				break;
			}
			case 8:
				HudsonTest_SendBadFragments(400);
				said = "N4 four hundred fragments of a huge message";
				break;
			// The rows below name the rocket's ghost from row 0, which has timed out by now, so each makes it again first.
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			{
				NETMESSAGE_NPCSTATE message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9004;
				message.xPos = at.vx;
				message.yPos = at.vy;
				message.zPos = at.vz;

				if (row == 9)
				{
					message.Guid = -1;
					said = "NPC a number no NPC can have";
				}
				else if (row == 10)
				{
					message.kind = 200;
					said = "NPC a kind that isn't on the list";
				}
				else if (row == 11)
				{
					message.sequence_length = 0;
					said = "NPC an animation length that can't be right";
				}
				else if (row == 12)
				{
					message.kind = 0;
					message.model = 250;
					message.sequence_length = 100;
					said = "NPC a model this level hasn't loaded";
				}
				else
				{
					ghost.objectId = 9001;
					ghost.type = I_BehaviourRocket;
					HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));
					message.Guid = 9001;
					message.sequence_length = 100;
					said = "NPC a message naming a rocket's ghost";
				}

				HudsonTest_AddRawPart(NetMT_NpcState, &message, sizeof(message));
				break;
			}
			case 14:
			{
				ghost.objectId = 9001;
				ghost.type = I_BehaviourRocket;
				HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));

				NETMESSAGE_NPCXENODELTAS message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9001;
				HudsonTest_AddRawPart(NetMT_NpcXenoDeltas, &message, sizeof(message));
				said = "NPC a xenoborg's head naming a rocket's ghost";
				break;
			}
			case 15:
			{
				ghost.objectId = 9001;
				ghost.type = I_BehaviourRocket;
				HudsonTest_AddRawPart(NetMT_LocalObjectState, &ghost, sizeof(ghost));

				NETMESSAGE_NPCKILLED message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9001;
				message.death_code = 7777;
				message.death_time = 8 * ONE_FIXED;
				HudsonTest_AddRawPart(NetMT_NpcKilled, &message, sizeof(message));
				said = "NPC a death naming a rocket's ghost";
				break;
			}
			case 16:
			{
				NETMESSAGE_NPCKILLED message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9004;
				message.death_time = -1;
				HudsonTest_AddRawPart(NetMT_NpcKilled, &message, sizeof(message));
				said = "NPC a death with a time that can't be right";
				break;
			}
			case 17:
			case 18:
			{
				NETMESSAGE_NPCSOUND message;
				memset(&message, 0, sizeof(message));
				message.species = row == 17 ? 200 : 0;
				message.pitch = row == 17 ? 0 : 0x7fffffff;
				message.vx = at.vx;
				message.vy = at.vy;
				message.vz = at.vz;
				HudsonTest_AddRawPart(NetMT_NpcSound, &message, sizeof(message));
				said = row == 17 ? "NPC a sound in a voice that isn't on the list" : "NPC a sound at a pitch that can't be right";
				break;
			}
			case 19:
			{
				NETMESSAGE_NPCXENODELTAS message;
				memset(&message, 0, sizeof(message));
				message.Guid = 9001;
				message.beams = 255;
				HudsonTest_AddRawPart(NetMT_NpcXenoDeltas, &message, sizeof(message));
				said = "NPC a xenoborg's beams with one he hasn't got";
				break;
			}
			case 20:
				said = "END the host has sent everything, and it's still here";
				break;
			default:
				return;
		}
	}

	if (said)
	{
		LogString(std::string("hostile: row ") + std::to_string(row) + ": " + said);
	}

	row++;
}

bool Hudson_NetIsCampaign()
{
	return HudsonRunIsOn && plan.coop;
}

bool Hudson_NetIsHostile()
{
	return HudsonRunIsOn && plan.hostile;
}

bool Hudson_NetIsTyped()
{
	return HudsonRunIsOn && plan.typed;
}

void Hudson_NetCampaignIsOver()
{
	if (HudsonRunIsOn && plan.netMode != kHudsonNetNone)
	{
		Hudson_NetSay("the campaign is over");
		EndTheLevel("the campaign is over", kHudsonExitFinished);
	}
}

static bool netPictureIsDue = false;
static bool netPictureIsTheSmokeTest = false;

const char *Hudson_NetPictureKind()
{
	bool const isTheSmokeTest = netPictureIsTheSmokeTest;

	netPictureIsTheSmokeTest = false;
	return isTheSmokeTest ? "smoke" : "net";
}

bool Hudson_NetPictureIsDue()
{
	bool const due = netPictureIsDue;

	if (due)
	{
		// The same numbering as the lamp's pictures, so nothing is ever overwritten
		lampCompareNumber++;
	}

	netPictureIsDue = false;
	return due;
}

void Hudson_NetSayARefusedGhost(int kind, int subtype)
{
	static int said = 0;

	if (HudsonRunIsOn && plan.netMode != kHudsonNetNone && said < 40)
	{
		char line[100];

		said++;
		_snprintf(line, sizeof(line) - 1, "net: the ghost refused was of behaviour %d, subtype %d", kind, subtype);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}
}

void Hudson_NetSayAnNpcDied(int deathCode)
{
	if (HudsonRunIsOn && plan.netMode != kHudsonNetNone)
	{
		char line[100];

		_snprintf(line, sizeof(line) - 1, "net: the host says an NPC has died, way of dying %d", deathCode);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}
}

void Hudson_NetSayAHit(int ammo, STRATEGYBLOCK const *hit)
{
	static int said = 0;

	if (!HudsonRunIsOn || plan.netMode == kHudsonNetNone || said >= 500 || !hit)
	{
		return;
	}

	said++;

	char line[120];

	_snprintf(line, sizeof(line) - 1, "net: a hit from the other machine, ammunition %d, on behaviour %d, whose health was %d", ammo, (int)hit->I_SBtype, hit->SBDamageBlock.Health >> 16);
	line[sizeof(line) - 1] = 0;
	LogString(line);
}

void Hudson_NetWhileDead(void *playerStatus)
{
	if (!HudsonRunIsOn || plan.netMode == kHudsonNetNone)
	{
		return;
	}

	static DWORD diedAt = 0;
	static DWORD lastCalled = 0;
	DWORD const now = GetTickCount();

	// A game whose player is dead still has its clock to keep
	if (plan.hostile && plan.hostileKind != kHudsonHostileRows)
	{
		Hudson_NetHostileOtherKinds(NULL, NULL, now);
	}
	else if (plan.hostile)
	{
		Hudson_NetHostileTimeline(NULL, true, now);
	}

	// This is called every frame he's dead, so a gap means he's been alive since, and this is another death
	if (now - lastCalled > 1000)
	{
		diedAt = 0;
	}

	lastCalled = now;

	if (diedAt == 0)
	{
		diedAt = now;
		Hudson_NetSay("I died");
	}

	// In a campaign he's out until the next level, and there's nothing to ask for. Every two seconds: who he's
	// watching, and whether the level's people are still being run, for proof that it goes on without him.
	if (SameAsSinglePlayer())
	{
		static DWORD lastLine = 0;

		if (now - lastLine >= 2000)
		{
			int people = 0;
			int x = 0;
			int z = 0;
			char line[160];

			lastLine = now;

			for (int i = 0; i < NumActiveStBlocks; i++)
			{
				STRATEGYBLOCK const *const thing = ActiveStBlockList[i];

				if (thing && thing->DynPtr && (thing->I_SBtype == I_BehaviourAlien || thing->I_SBtype == I_BehaviourMarine || thing->I_SBtype == I_BehaviourPredator
					|| thing->I_SBtype == I_BehaviourFaceHugger || thing->I_SBtype == I_BehaviourXenoborg || thing->I_SBtype == I_BehaviourQueenAlien || thing->I_SBtype == I_BehaviourPredatorAlien))
				{
					if (people == 0)
					{
						x = thing->DynPtr->Position.vx;
						z = thing->DynPtr->Position.vz;
					}

					people++;
				}
			}

			_snprintf(line, sizeof(line) - 1, "net: I'm out until the next level, watching %d. %d people in the level here, the first at %d %d", (int)MultiplayerObservedPlayer, people, x, z);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}

		return;
	}

	if (now - diedAt >= 3000)
	{
		((PLAYER_STATUS *)playerStatus)->Mvt_InputRequests.Flags.Rqst_Operate = 1;
		diedAt = 0;
		Hudson_NetSay("I asked to come back");
	}
}

bool Hudson_NetIsHost()
{
	return HudsonRunIsOn && plan.netMode == kHudsonNetHost;
}

void Hudson_NetSay(const char *stage)
{
	_snprintf(netStage, sizeof(netStage) - 1, "%s", stage);
	netStage[sizeof(netStage) - 1] = 0;
	LogString(std::string("net: ") + stage);
}

// The other player, as this game sees them: a ghost, which is how a network game keeps someone else's body. NULL if they aren't here.
static STRATEGYBLOCK *Hudson_NetOtherPlayer()
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (block && block->I_SBtype == I_BehaviourNetGhost && block->DynPtr)
		{
			NETGHOSTDATABLOCK const *const ghost = (NETGHOSTDATABLOCK *)block->SBdataptr;

			if (ghost->type == I_BehaviourMarinePlayer || ghost->type == I_BehaviourPredatorPlayer || ghost->type == I_BehaviourAlienPlayer)
			{
				return block;
			}
		}
	}

	return NULL;
}

// botfight=: can an NPC the host runs see, chase and hit a player on another machine, and can he hit and kill it? The host makes one beside the joiner, once it has seen him for a while.
// The joiner turns to the nearest NPC it can see and fires. Each game writes a line a second: the host from the real NPC, and the joiner from its ghost.
static bool Hudson_IsOfKind(STRATEGYBLOCK const *block, int kind)
{
	AVP_BEHAVIOUR_TYPE type = block->I_SBtype;

	if (type == I_BehaviourNetGhost)
	{
		type = ((NETGHOSTDATABLOCK const *)block->SBdataptr)->type;
	}

	switch (kind)
	{
		case kHudsonBotFightAlien: return type == I_BehaviourAlien;
		case kHudsonBotFightMarine:
		case kHudsonBotFightFlame:
			return type == I_BehaviourMarine;
		case kHudsonBotFightPredator:
		case kHudsonBotFightPlasma:
			return type == I_BehaviourPredator;
		case kHudsonBotFightXenoborg: return type == I_BehaviourXenoborg;
		case kHudsonBotFightQueen: return type == I_BehaviourQueenAlien;
		case kHudsonBotFightFacehugger: return type == I_BehaviourFaceHugger;
		case kHudsonBotFightSentry: return type == I_BehaviourAutoGun;
		default: return false;
	}
}

static STRATEGYBLOCK *Hudson_NearestOfKind(VECTORCH const *from, int kind)
{
	STRATEGYBLOCK *nearest = NULL;
	int nearestDistance = 0;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (block && block->DynPtr && block->SBdataptr && Hudson_IsOfKind(block, kind))
		{
			int const distance = VectorDistance(const_cast<VECTORCH *>(from), &block->DynPtr->Position);

			if (!nearest || distance < nearestDistance)
			{
				nearest = block;
				nearestDistance = distance;
			}
		}
	}

	return nearest;
}

static STRATEGYBLOCK *Hudson_BotFightNearest(VECTORCH const *from)
{
	return Hudson_NearestOfKind(from, plan.botFight);
}

// The host leaves for the room furthest from the joiner, so the NPC has only the joiner to go after, and the joiner's shots can't hit the host by accident.
static void Hudson_BotFightSendTheHostAway(STRATEGYBLOCK *other)
{
	SCENEMODULE const *const scene = MainSceneArray[0];   // the game only ever has the one
	MODULE const *furthest = NULL;
	int furthestDistance = 0;

	for (int i = 0; scene && scene->sm_marray && i < ModuleArraySize; i++)
	{
		MODULE const *const room = scene->sm_marray[i];

		if (!room)
		{
			continue;
		}

		VECTORCH middle = room->m_world;
		int const distance = VectorDistance(&middle, &other->DynPtr->Position);

		// The middle has to really be in the room, or there's nowhere to stand
		if (distance > furthestDistance && ModuleFromPosition(&middle, (MODULE *)0) == room)
		{
			furthest = room;
			furthestDistance = distance;
		}
	}

	char line[120];

	if (furthest)
	{
		MoveThePlayerTo(furthest->m_world);
		_snprintf(line, sizeof(line) - 1, "botfight: the host went away, to %d mm from the joiner", furthestDistance);
	}
	else
	{
		_snprintf(line, sizeof(line) - 1, "botfight: the host found no room to go to");
	}

	line[sizeof(line) - 1] = 0;
	LogString(line);
}

// A new NPC faces a fixed way, and a xenoborg only wakes for what's in front of it. The test is of a joiner standing in front of it.
static void Hudson_BotFightFaceTheJoiner(STRATEGYBLOCK *bot, STRATEGYBLOCK const *joiner)
{
	float const dx = (float)(joiner->DynPtr->Position.vx - bot->DynPtr->Position.vx);
	float const dz = (float)(joiner->DynPtr->Position.vz - bot->DynPtr->Position.vz);
	DYNAMICSBLOCK *const body = bot->DynPtr;

	body->OrientEuler.EulerX = 0;
	body->OrientEuler.EulerY = ((int)(atan2f(dx, dz) * 4096.0f / 6.2831853f)) & 4095;
	body->OrientEuler.EulerZ = 0;
	CreateEulerMatrix(&body->OrientEuler, &body->OrientMat);
	TransposeMatrixCH(&body->OrientMat);
	body->PrevOrientMat = body->OrientMat;
}

// What FindPolygonInLineOfSight's third word is: 0 is everything active, and 1 would be only what's on this machine's screen, which leaves out the walls behind the player.
static const int kLookAtAllThatsActive = 0;

// How far it is from this place to the nearest thing, along the four directions a fight looks. It only looks at this one height, so a low step wouldn't show.
static int Hudson_BotFightRoomAround(VECTORCH *place, DISPLAYBLOCK *leaveOut)
{
	static const int kSides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	int nearest = 0x7fffffff;

	for (int side = 0; side < 4; side++)
	{
		VECTORCH direction = { kSides[side][0] * ONE_FIXED, 0, kSides[side][1] * ONE_FIXED };

		FindPolygonInLineOfSight(&direction, place, kLookAtAllThatsActive, leaveOut);

		if (LOS_ObjectHitPtr && LOS_Lambda < nearest)
		{
			nearest = LOS_Lambda;
		}
	}

	// A room has walls. Seeing none in any direction means these rooms aren't active on this machine, so nothing is known about this place.
	return nearest == 0x7fffffff ? 0 : nearest;
}

// How close to the joiner an NPC may be made, and how much room it wants round it. Both in millimetres, and the room is a guess at a predator's width with some to spare.
static const int kNearestAnNpcIsMade = 1500;
static const int kRoomAnNpcNeeds = 1200;

// A queen or a facehugger can't be made during a level, so the host brings the level's own one to this place. False if the level has none.
static bool Hudson_BotFightBringOne(VECTORCH const *from, VECTORCH const *place)
{
	STRATEGYBLOCK *bot = Hudson_BotFightNearest(from);

	// A level's predator can start asleep, as a different kind of thing, until something in the level wakes it. This does what that would do.
	if (!bot && (plan.botFight == kHudsonBotFightPredator || plan.botFight == kHudsonBotFightPlasma))
	{
		extern void ActivateDormantPredator(STRATEGYBLOCK *sbPtr);

		for (int i = 0; i < NumActiveStBlocks && !bot; i++)
		{
			if (ActiveStBlockList[i]->I_SBtype == I_BehaviourDormantPredator)
			{
				LogString(std::string("botfight: woke a level's sleeping predator, named ") + (ActiveStBlockList[i]->name ? ActiveStBlockList[i]->name : "nothing"));
				ActivateDormantPredator(ActiveStBlockList[i]);
				bot = Hudson_BotFightNearest(from);
			}
		}
	}

	if (!bot)
	{
		return false;
	}

	LogString(std::string("botfight: brought the level's own one, named ") + (bot->name ? bot->name : "nothing, or woken"));

	bot->DynPtr->Position = *place;
	bot->DynPtr->PrevPosition = *place;
	bot->containingModule = ModuleFromPosition(const_cast<VECTORCH *>(place), (MODULE *)0);

	// A level's facehugger can float until a trigger wakes it, so this does what the trigger does
	if (bot->I_SBtype == I_BehaviourFaceHugger)
	{
		extern void Wake_Hugger(STRATEGYBLOCK *sbPtr);

		Wake_Hugger(bot);
	}

	return true;
}

static void Hudson_AskForAPicture()
{
	if (!plan.noNetPictures)
	{
		netPictureIsDue = true;
	}
}

// This player turns to look straight at a place.
static void Hudson_FaceThePlace(PLAYER_STATUS *status, VECTORCH const &middle)
{
	aimedThisFrame = true;

	DYNAMICSBLOCK *const body = Player->ObStrategyBlock->DynPtr;

	float const dx = (float)(middle.vx - body->Position.vx);
	float const dz = (float)(middle.vz - body->Position.vz);
	float const across = sqrtf(dx * dx + dz * dz);

	body->OrientEuler.EulerX = 0;
	body->OrientEuler.EulerY = ((int)(atan2f(dx, dz) * 4096.0f / 6.2831853f)) & 4095;
	body->OrientEuler.EulerZ = 0;
	CreateEulerMatrix(&body->OrientEuler, &body->OrientMat);
	TransposeMatrixCH(&body->OrientMat);
	body->AngVelocity.EulerY = 0;
	status->ViewPanX = ((int)(atan2f((float)(middle.vy - Global_VDB_Ptr->VDB_World.vy), across) * 4096.0f / 6.2831853f)) & 4095;
}

// This player turns to look straight at that.
static void Hudson_FaceIt(PLAYER_STATUS *status, STRATEGYBLOCK *target)
{
	VECTORCH middle;

	GetTargetingPointOfObject_Far(target, &middle);
	Hudson_FaceThePlace(status, middle);
}

// This player faces that and fires its own weapon, so that any damage takes the game's real road. A Predator's blades and an Alien's claws only reach so far, so it goes up to the target, a metre short of it, once a second.
static void Hudson_ShootAt(PLAYER_STATUS *status, STRATEGYBLOCK *target, DWORD now)
{
	static DWORD lastStep = 0;
	DYNAMICSBLOCK *const body = Player->ObStrategyBlock->DynPtr;
	VECTORCH middle;

	GetTargetingPointOfObject_Far(target, &middle);

	float const dx = (float)(middle.vx - body->Position.vx);
	float const dz = (float)(middle.vz - body->Position.vz);
	float const across = sqrtf(dx * dx + dz * dz);

	if (AvP.PlayerType != I_Marine && plan.killKind != kHudsonBotFightQueen && across > 1500.0f && now - lastStep >= 1000)
	{
		VECTORCH closer = body->Position;

		lastStep = now;
		closer.vx += (int)(dx * (across - 1000.0f) / across);
		closer.vz += (int)(dz * (across - 1000.0f) / across);
		MoveThePlayerTo(closer);
	}

	Hudson_FaceIt(status, target);
	status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
}

static void Hudson_NetBotFight(PLAYER_STATUS *status, STRATEGYBLOCK *other)
{
	extern HMODELCONTROLLER *Npc_HModelController(STRATEGYBLOCK *sbPtr);
	extern void CreatePredoBot(VECTORCH *position, int weapon);
	extern void CreateXenoborg(VECTORCH *Position, int type);

	static DWORD sawThemAt = 0, lastLine = 0;
	static bool made = false, sawTheBot = false, saidItDied = false;
	DWORD const now = GetTickCount();
	STRATEGYBLOCK *const me = Player->ObStrategyBlock;
	char line[480];

	if (!other)
	{
		return;
	}

	if (sawThemAt == 0)
	{
		sawThemAt = now;
	}

	bool const isHost = plan.netMode == kHudsonNetHost || plan.botSingle;

	// A queen's moves depend on the room she's in, so she stays there and the joiner goes to her
	if (plan.botFight == kHudsonBotFightQueen)
	{
		static bool wentToHer = false, wentUpToHer = false;
		SCENEMODULE const *const scene = MainSceneArray[0];

		// Once her ghost is here, up to 6 metres from her and at her height. The middle of the room can be lower than she is, and she won't go after a player down there.
		if ((!isHost || plan.botSingle) && wentToHer && !wentUpToHer)
		{
			if (STRATEGYBLOCK const *const queen = Hudson_BotFightNearest(&me->DynPtr->Position))
			{
				float const dx = (float)(me->DynPtr->Position.vx - queen->DynPtr->Position.vx);
				float const dz = (float)(me->DynPtr->Position.vz - queen->DynPtr->Position.vz);
				float const across = sqrtf(dx * dx + dz * dz);

				wentUpToHer = true;

				if (across > 1.0f)
				{
					VECTORCH place = queen->DynPtr->Position;

					place.vx += (int)(dx * 6000.0f / across);
					place.vz += (int)(dz * 6000.0f / across);

					if (ModuleFromPosition(&place, (MODULE *)0))
					{
						MoveThePlayerTo(place);
						LogString("botfight: the joiner went up to her, at her height");
					}
				}
			}
		}

		if ((!isHost || plan.botSingle) && !wentToHer && now - sawThemAt >= 8000)
		{
			wentToHer = true;
			LogString("botfight: the joiner looks for the room called Hangar");

			for (int i = 0; scene && scene->sm_marray && i < ModuleArraySize; i++)
			{
				MODULE const *const room = scene->sm_marray[i];

				if (room && room->name && _stricmp(room->name, "Hangar") == 0)
				{
					MoveThePlayerTo(room->m_world);
					LogString("botfight: the joiner went to the room called Hangar");
				}
			}
		}
	}

	// The host is only here to run the NPC. If the level's own NPCs killed it, the test would end with it. One that's fighting as well can be hurt, because whether it's hit is part of the test.
	if (isHost && !plan.botSingle && plan.botWeapon == kHudsonBotWeaponNone)
	{
		status->invulnerabilityTimer = ONE_FIXED * 10;
	}

	// The host makes it three metres from the joiner, on the first side with nothing in the way
	if (isHost && !made && plan.botFight == kHudsonBotFightQueen && now - sawThemAt >= 8000)
	{
		made = true;

		if (!plan.botStay && !plan.botSingle)
		{
			Hudson_BotFightSendTheHostAway(other);
		}
	}

	if (isHost && !made && now - sawThemAt >= 8000)
	{
		static const int kSides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		VECTORCH there = other->DynPtr->Position;

		// A queen is big, so she needs more room
		int const metres = plan.botFight == kHudsonBotFightQueen ? 6000 : 3000;

		// A place with a clear way from the joiner and room all round it. An NPC made against a wall can't move at all, so this starts as far off as a fight should, and comes nearer until a body fits.
		VECTORCH place = there;
		int away = metres, roomAround = 0;
		bool foundAPlace = false;

		for (; away >= kNearestAnNpcIsMade && !foundAPlace; away -= 500)
		{
			for (int side = 0; side < 4 && !foundAPlace; side++)
			{
				VECTORCH direction = { kSides[side][0] * ONE_FIXED, 0, kSides[side][1] * ONE_FIXED };

				FindPolygonInLineOfSight(&direction, &there, kLookAtAllThatsActive, other->SBdptr);

				if (LOS_ObjectHitPtr && LOS_Lambda <= away)
				{
					continue;
				}

				place = there;
				place.vx += kSides[side][0] * away;
				place.vz += kSides[side][1] * away;
				roomAround = Hudson_BotFightRoomAround(&place, other->SBdptr);
				foundAPlace = roomAround >= kRoomAnNpcNeeds;
			}
		}

		// The sum above takes one more off on its way out
		away += 500;

		static bool saidThereIsNoPlace = false;

		if (!foundAPlace && !saidThereIsNoPlace)
		{
			saidThereIsNoPlace = true;
			LogString("botfight: there's no place near the joiner with room for an NPC");
		}

		if (foundAPlace)
		{
			if (plan.botOwn)
			{
				if (!Hudson_BotFightBringOne(&there, &place))
				{
					LogString("botfight: this level has none of its own of that kind");
				}
			}
			else switch (plan.botFight)
			{
				case kHudsonBotFightAlien: CreateAlienBot(&place, 0); break;
				case kHudsonBotFightMarine: CreateMarineBot(&place, MNPCW_PulseRifle + 1); break;
				case kHudsonBotFightFlame: CreateMarineBot(&place, MNPCW_Flamethrower + 1); break;
				case kHudsonBotFightPredator: CreatePredoBot(&place, 0); break;
				case kHudsonBotFightPlasma: CreatePredoBot(&place, 1); break;
				case kHudsonBotFightXenoborg: CreateXenoborg(&place, 0); break;
				case kHudsonBotFightFacehugger:
				case kHudsonBotFightSentry:
					if (!Hudson_BotFightBringOne(&there, &place))
					{
						LogString("botfight: this level has none of that kind to bring");
					}
					break;
			}

			made = true;

			if (STRATEGYBLOCK *const newOne = Hudson_BotFightNearest(&place))
			{
				Hudson_BotFightFaceTheJoiner(newOne, other);
			}

			// Only now, because the walls near the joiner stop being active on this machine once it has gone
			if (!plan.botStay && !plan.botSingle)
			{
				Hudson_BotFightSendTheHostAway(other);
			}

			_snprintf(line, sizeof(line) - 1, "botfight: the host made one at %d %d %d, %d mm from the joiner, with %d mm of room around it", place.vx, place.vy, place.vz, away, roomAround);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
	}

	// The one beside the joiner, not one the level already had somewhere near the host
	STRATEGYBLOCK *const bot = Hudson_BotFightNearest(isHost ? &other->DynPtr->Position : &me->DynPtr->Position);

	if (bot && !sawTheBot && (made || !isHost))
	{
		sawTheBot = true;
		LogString(isHost ? "botfight: the host has it" : "botfight: the joiner can see its ghost");
	}

	if (sawTheBot && !bot && !saidItDied)
	{
		saidItDied = true;
		LogString(isHost ? "botfight: on the host it has died" : "botfight: on the joiner its ghost has gone");
	}

	// How many frames since the last line it and this player were on a floor. The line's "floor" is only the one frame, and being on a floor can flicker from frame to frame at a high frame rate.
	static int framesCounted = 0, itsFramesOnAFloor = 0, myFramesOnAFloor = 0;

	if (bot)
	{
		framesCounted++;
		itsFramesOnAFloor += bot->DynPtr->IsInContactWithFloor ? 1 : 0;
		myFramesOnAFloor += me->DynPtr->IsInContactWithFloor ? 1 : 0;
	}

	if (now - lastLine >= 1000)
	{
		lastLine = now;

		if (bot)
		{
			int const distanceToJoiner = VectorDistance(&bot->DynPtr->Position, isHost ? &other->DynPtr->Position : &me->DynPtr->Position);
			char const *target = "unknown here";
			int state = -1;

			if (isHost)
			{
				STRATEGYBLOCK const *aimedAt = NULL;

				switch (bot->I_SBtype)
				{
					case I_BehaviourMarine: state = (int)((MARINE_STATUS_BLOCK *)bot->SBdataptr)->behaviourState; break;
					case I_BehaviourAlien: state = (int)((ALIEN_STATUS_BLOCK *)bot->SBdataptr)->BehaviourState; break;
					case I_BehaviourPredator:
						// Its state, then which weapon it holds
						state = (int)((PREDATOR_STATUS_BLOCK *)bot->SBdataptr)->behaviourState * 100 + (int)((PREDATOR_STATUS_BLOCK *)bot->SBdataptr)->Selected_Weapon->id;
						break;
					case I_BehaviourXenoborg: state = (int)((XENO_STATUS_BLOCK *)bot->SBdataptr)->behaviourState; break;
					case I_BehaviourQueenAlien:
						// Her state, then the move she's making
						state = (int)((QUEEN_STATUS_BLOCK *)bot->SBdataptr)->QueenState * 100 + (int)((QUEEN_STATUS_BLOCK *)bot->SBdataptr)->current_move;
						break;
					case I_BehaviourFaceHugger: state = (int)((FACEHUGGER_STATUS_BLOCK *)bot->SBdataptr)->nearBehaviourState; break;
					case I_BehaviourAutoGun: state = (int)((AUTOGUN_STATUS_BLOCK *)bot->SBdataptr)->behaviourState; break;
					default: break;
				}

				switch (bot->I_SBtype)
				{
					case I_BehaviourMarine: aimedAt = ((MARINE_STATUS_BLOCK *)bot->SBdataptr)->Target; break;
					case I_BehaviourAlien: aimedAt = ((ALIEN_STATUS_BLOCK *)bot->SBdataptr)->Target; break;
					case I_BehaviourPredator: aimedAt = ((PREDATOR_STATUS_BLOCK *)bot->SBdataptr)->Target; break;
					case I_BehaviourXenoborg: aimedAt = ((XENO_STATUS_BLOCK *)bot->SBdataptr)->Target; break;
					case I_BehaviourQueenAlien: aimedAt = ((QUEEN_STATUS_BLOCK *)bot->SBdataptr)->QueenTargetSB; break;
					case I_BehaviourFaceHugger: aimedAt = ((FACEHUGGER_STATUS_BLOCK *)bot->SBdataptr)->Target; break;
					case I_BehaviourAutoGun: aimedAt = ((AUTOGUN_STATUS_BLOCK *)bot->SBdataptr)->Target; break;
					default: break;
				}

				target = !aimedAt ? "nobody" : (aimedAt == other ? "the joiner" : (aimedAt == me ? "the host" : "something else"));
			}

			bool const isAGhost = bot->I_SBtype == I_BehaviourNetGhost;
			HMODELCONTROLLER const *const model = isAGhost ? &((NETGHOSTDATABLOCK *)bot->SBdataptr)->HModelController : Npc_HModelController(bot);

			char health[40] = "a ghost has no health of its own";

			if (!isAGhost)
			{
				_snprintf(health, sizeof(health) - 1, "health %d", bot->SBDamageBlock.Health >> 16);
			}

			STRATEGYBLOCK const *const joiner = isHost ? other : me;

			// What it's bumping into this frame, and what kind of thing the first of them is. -1 is the level itself.
			int touching = 0, firstTouched = 0;

			for (struct collisionreport const *report = bot->DynPtr->CollisionReportPtr; report; report = report->NextCollisionReportPtr)
			{
				firstTouched = touching == 0 ? (report->ObstacleSBPtr ? (int)report->ObstacleSBPtr->I_SBtype : -1) : firstTouched;
				touching++;
			}

			// Can this machine see it? How cloaked it is here, with 65536 the most, and whether the game put it on this machine's screen this frame.
			int const cloak = isAGhost ? ((NETGHOSTDATABLOCK *)bot->SBdataptr)->CloakingEffectiveness : (bot->I_SBtype == I_BehaviourPredator ? ((PREDATOR_STATUS_BLOCK *)bot->SBdataptr)->CloakingEffectiveness : 0);
			extern DISPLAYBLOCK *OnScreenBlockList[];
			extern int NumOnScreenBlocks;
			bool onMyScreen = false;

			for (int i = 0; i < NumOnScreenBlocks && bot->SBdptr; i++)
			{
				onMyScreen = onMyScreen || OnScreenBlockList[i] == bot->SBdptr;
			}

			char const *const itsRoom = bot->containingModule && bot->containingModule->name ? bot->containingModule->name : "no room";
			char const *const joinersRoom = joiner->containingModule && joiner->containingModule->name ? joiner->containingModule->name : "no room";

			_snprintf(line, sizeof(line) - 1, "botfight: %d mm from the joiner, %s, target %s, %s, sequence %d %d at %d, state %d, it's in %.20s and the joiner's in %.20s, joiner is %s here, speed %d %d, at %d %d %d, floor %d, on a floor %d of %d frames and me %d, touching %d first %d, cloak %d, on my screen %d, I'm at %d %d %d, my health %d", distanceToJoiner, bot->SBdptr ? "near" : "far", target,
				health, model ? model->Sequence_Type : -1, model ? model->Sub_Sequence : -1, model ? model->sequence_timer : -1, state, itsRoom, joinersRoom, joiner->SBdptr ? "near" : "far",
				bot->DynPtr->LinVelocity.vx, bot->DynPtr->LinVelocity.vz, bot->DynPtr->Position.vx, bot->DynPtr->Position.vy, bot->DynPtr->Position.vz, bot->DynPtr->IsInContactWithFloor,
				itsFramesOnAFloor, framesCounted, myFramesOnAFloor, touching, firstTouched, cloak, onMyScreen ? 1 : 0, me->DynPtr->Position.vx, me->DynPtr->Position.vy, me->DynPtr->Position.vz, me->SBDamageBlock.Health >> 16);
			line[sizeof(line) - 1] = 0;
			LogString(line);

			framesCounted = 0;
			itsFramesOnAFloor = 0;
			myFramesOnAFloor = 0;
		}
		else
		{
			_snprintf(line, sizeof(line) - 1, "botfight: none here, my health %d", me->SBDamageBlock.Health >> 16);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
	}

	// The joiner holds its fire at first, so the NPC has time to go after it and hit it. Then it faces it and fires, so the NPC can be killed.
	static const DWORD kSecondsBeforeTheJoinerFires = 20;
	static DWORD sawTheGhostAt = 0;

	if (!isHost && bot && bot->SBdptr && sawTheGhostAt == 0)
	{
		sawTheGhostAt = now;
	}

	if (!isHost && bot && sawTheGhostAt != 0 && now - sawTheGhostAt >= kSecondsBeforeTheJoinerFires * 1000 && plan.botWeapon == kHudsonBotWeaponNone)
	{
		Hudson_ShootAt(status, bot, now);
	}

	// botweapon=: a Predator's own weapon, so that this player fights the way a person playing one would. Each player starts counting once it has an NPC here, and both hold their fire for a while, so it has time to fight them.
	if (plan.botWeapon != kHudsonBotWeaponNone)
	{
		extern int SlotForThisWeapon(enum WEAPON_ID weaponID);
		static bool gaveThem = false;
		static DWORD saidItIsHereAt = 0;

		if (!gaveThem)
		{
			gaveThem = true;
			GiveAllWeaponsCheat();
		}

		int const slot = SlotForThisWeapon(plan.botWeapon == kHudsonBotWeaponBlades ? WEAPON_PRED_WRISTBLADE : WEAPON_PRED_SHOULDERCANNON);

		if (slot < 0)
		{
			return;
		}

		if ((int)status->SelectedWeaponSlot != slot)
		{
			status->Mvt_InputRequests.Flags.Rqst_WeaponNo = slot + 1;
		}

		// The cannon runs on the same charge as the cloak
		status->FieldCharge = PLAYERCLOAK_MAXENERGY;

		if (bot && saidItIsHereAt == 0)
		{
			saidItIsHereAt = now;
		}

		if (bot && saidItIsHereAt != 0 && now - saidItIsHereAt >= kSecondsBeforeTheJoinerFires * 1000 && VectorDistance(&bot->DynPtr->Position, &me->DynPtr->Position) < 20000)
		{
			Hudson_ShootAt(status, bot, now);

			// The cannon charges while the trigger's held and fires when it's let go
			if (plan.botWeapon == kHudsonBotWeaponPlasma && now % 2000 >= 1500)
			{
				status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 0;
			}
		}
	}
}

// killkind=: can a level's own named NPC really be killed in a network game, by either machine, and does its death move the level on for both? A walk can't show this, because it fires the link with nobody dying.
// The level file says where each one is placed, and the run is told. Early in a level a placed NPC is still there, so the host takes the nearest of that kind to that place. It brings it up to whichever machine is the killer, and that machine shoots it with its own weapon, so the damage takes the game's real road. The killer can't die and doesn't run out of ammunition, because this tests the kill, not the fight, and bringing the NPC over means that how it would have come to the player isn't tested either.
// Each machine says what it saw, and tools/hudson/read-kills.js compares the two logs. A named NPC's ghost has no name on the joiner, so the host says its object number, the joiner says the number of each ghost it sees, and the reader matches them.
static bool Hudson_IsTheKindToKill(STRATEGYBLOCK const *block)
{
	// A predator can start out asleep, as a different kind of thing
	return Hudson_IsOfKind(block, plan.killKind) || (plan.killKind == kHudsonBotFightPredator && block->I_SBtype == I_BehaviourDormantPredator);
}

// A block can be gone by the next frame, so this asks, and never reads one that may have been freed.
static bool Hudson_IsStillThere(STRATEGYBLOCK const *block)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		if (ActiveStBlockList[i] == block)
		{
			return true;
		}
	}

	return false;
}

// Dying is being in the list and no longer alive. These NPCs turn into a corpse in the same block, so they stop being that kind of thing, and a dead queen stays a queen, in her dead state. A block that's gone from the list hasn't died: the level has started again. Asked only of a block that's still there.
static bool Hudson_HasDied(STRATEGYBLOCK const *block)
{
	if (!Hudson_IsTheKindToKill(block))
	{
		return true;
	}

	return block->I_SBtype == I_BehaviourQueenAlien && ((QUEEN_STATUS_BLOCK const *)block->SBdataptr)->QueenState == QBS_Dead;
}

// Where this one is. A sleeping predator has no body yet, so it says where it will be from what it's going to be woken from.
static bool Hudson_WhereItIs(STRATEGYBLOCK const *block, VECTORCH *where)
{
	if (block->I_SBtype == I_BehaviourDormantPredator)
	{
		*where = ((TOOLS_DATA_PREDATOR const *)((DORMANT_PREDATOR_STATUS_BLOCK const *)block->SBdataptr)->toolsData)->position;
		return true;
	}

	if (!block->DynPtr)
	{
		return false;
	}

	*where = block->DynPtr->Position;
	return true;
}

static STRATEGYBLOCK *Hudson_FindTheOneToKill(void)
{
	VECTORCH place;
	STRATEGYBLOCK *nearest = NULL;
	int nearestDistance = 0;

	place.vx = plan.killX;
	place.vy = plan.killY;
	place.vz = plan.killZ;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		VECTORCH where;

		if (block && block->SBdataptr && Hudson_IsTheKindToKill(block) && Hudson_WhereItIs(block, &where))
		{
			int const distance = VectorDistance(&place, &where);

			if (!nearest || distance < nearestDistance)
			{
				nearest = block;
				nearestDistance = distance;
			}
		}
	}

	char line[220];
	static const int kMostFromItsPlace = 4000;

	if (!nearest || nearestDistance > kMostFromItsPlace)
	{
		_snprintf(line, sizeof(line) - 1, "killnamed: on the host: nothing of that kind within %d mm of %d %d %d, and the nearest is %d mm away", kMostFromItsPlace, place.vx, place.vy, place.vz, nearest ? nearestDistance : -1);
		line[sizeof(line) - 1] = 0;
		LogString(line);
		return NULL;
	}

	char name[kLevelEventNameLength * 2 + 1];

	LevelEvents_NameAsText(NpcNameInTheLevel(nearest), name);
	_snprintf(line, sizeof(line) - 1, "killnamed: on the host: level name %s, object number %d, kind %d, %d mm from where the level file puts it, in %.24s, health %d", name, *(const int *)&nearest->SBname[4], (int)nearest->I_SBtype, nearestDistance, nearest->containingModule && nearest->containingModule->name ? nearest->containingModule->name : "no room", nearest->SBDamageBlock.Health >> 16);
	line[sizeof(line) - 1] = 0;
	LogString(line + std::string(", ") + HudsonStrategies_Title(nearest));

	// One that's asleep is woken the way its level would wake it. That gives it a new number, so the number is said again.
	if (nearest->I_SBtype == I_BehaviourDormantPredator)
	{
		ActivateDormantPredator(nearest);

		// The game can refuse: in a game with no predators, waking one removes it
		if (!nearest->SBdataptr || !nearest->DynPtr)
		{
			LogString("killnamed: on the host: it was asleep, and waking it removed it");
			return NULL;
		}

		_snprintf(line, sizeof(line) - 1, "killnamed: on the host: it was asleep, and it's awake now: object number %d, kind %d", *(const int *)&nearest->SBname[4], (int)nearest->I_SBtype);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	return nearest;
}

// Both players have to see it, and a rifle can't tell a friend from a target. So the sides are tried in the order that keeps the other player out of the line of fire: straight out to the side first, then away from them, and towards them last of all.
static bool Hudson_BringItToTheKiller(STRATEGYBLOCK *target, STRATEGYBLOCK *killer, STRATEGYBLOCK *otherPlayer)
{
	static const int kSides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	int const away = plan.killKind == kHudsonBotFightQueen ? 6000 : 3000; // a queen is big
	VECTORCH there = killer->DynPtr->Position;
	float const toOtherX = (float)(otherPlayer->DynPtr->Position.vx - there.vx);
	float const toOtherZ = (float)(otherPlayer->DynPtr->Position.vz - there.vz);
	float rank[4];
	int order[4] = { 0, 1, 2, 3 };

	for (int side = 0; side < 4; side++)
	{
		float const towards = kSides[side][0] * toOtherX + kSides[side][1] * toOtherZ;

		rank[side] = towards > 0.0f ? towards + 1000000.0f : -towards;
	}

	std::sort(order, order + 4, [&rank](int a, int b) { return rank[a] < rank[b]; });

	for (int tried = 0; tried < 4; tried++)
	{
		int const side = order[tried];
		VECTORCH direction = { kSides[side][0] * ONE_FIXED, 0, kSides[side][1] * ONE_FIXED };

		FindPolygonInLineOfSight(&direction, &there, kLookAtAllThatsActive, killer->SBdptr);

		if (!LOS_ObjectHitPtr || LOS_Lambda > away + 500)
		{
			VECTORCH place = there;
			char line[120];

			place.vx += kSides[side][0] * away;
			place.vz += kSides[side][1] * away;
			target->DynPtr->Position = place;
			target->DynPtr->PrevPosition = place;
			target->containingModule = ModuleFromPosition(&place, (MODULE *)0);
			Hudson_BotFightFaceTheJoiner(target, killer);

			_snprintf(line, sizeof(line) - 1, "killnamed: on the host: brought it to the killer, at %d %d %d", place.vx, place.vy, place.vz);
			line[sizeof(line) - 1] = 0;
			LogString(line);
			return true;
		}
	}

	return false;
}

// A queen's armour never wears down: a rifle takes about 1 health a hit from her 4000. So the killer of a queen is given what a player would really use. A shoulder cannon draws on the field charge, which isn't a magazine, so that's kept full too.
static void Hudson_ArmTheKillerOfAQueen(PLAYER_STATUS *status)
{
	extern int SlotForThisWeapon(enum WEAPON_ID weaponID);
	static bool gaveGuns = false;

	if (!gaveGuns)
	{
		gaveGuns = true;
		GiveAllWeaponsCheat();
		LogString(AvP.PlayerType == I_Predator ? "killnamed: the killer has the shoulder cannon, because a queen's armour never wears down" : "killnamed: the killer has the SADAR, because a queen's armour never wears down");
	}

	int const slot = SlotForThisWeapon(AvP.PlayerType == I_Predator ? WEAPON_PRED_SHOULDERCANNON : WEAPON_SADAR);

	if (slot >= 0 && (int)status->SelectedWeaponSlot != slot)
	{
		status->Mvt_InputRequests.Flags.Rqst_WeaponNo = slot + 1;
	}

	status->FieldCharge = PLAYERCLOAK_MAXENERGY;
}

// The shoulder cannon charges while the trigger's held and fires when it's let go, so its killer lets go now and then. Harmless for any other weapon, so it's only asked of this one.
static void Hudson_LetGoOfTheCannonNowAndThen(PLAYER_STATUS *status, DWORD now)
{
	if (plan.killKind == kHudsonBotFightQueen && AvP.PlayerType == I_Predator && now % 2000 >= 1500)
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 0;
	}
}

static void Hudson_KillNamed(PLAYER_STATUS *status, STRATEGYBLOCK *other)
{
	static const DWORD kLooksAfter = 5000;
	static const DWORD kBringsItAfter = 10000;
	static const DWORD kEndsAfterTheKill = 20000; // long enough for the level to move on, and for each log to say so
	// A picture stops the game for most of a second, so none is taken at the death itself, where it would sit on top of the fall. Two come after it, on every machine, and a level like fall_m ends about 6 seconds after its kill.
	static const DWORD kPictureAfterBringing = 1000;
	static const DWORD kPictureAfterTheDeath = 2000;
	static const DWORD kPictureLongAfterTheDeath = 5000;
	static const DWORD kLooksAtTheBodyFor = 5500; // past the last picture, so that each one shows the body

	static VECTORCH whereItWas = { 0, 0, 0 };
	static DWORD sawThemAt = 0, nextReach = 0, lastLine = 0, diedAt = 0;
	static STRATEGYBLOCK *target = NULL;
	static bool looked = false, brought = false, saidShooting = false, wasInReach = false, saidNear = false;
	static std::vector<int> ghostsSaid;
	static int numberShootingAt = 0;
	static DWORD lastPicture = 0;
	static DWORD ghostWentAt = 0;
	static bool saidAfter = false, saidLongAfter = false;
	DWORD const now = GetTickCount();
	bool const isHost = plan.netMode == kHudsonNetHost;
	bool const iAmTheKiller = isHost == plan.killerIsHost;
	char line[160];

	if (!other)
	{
		return;
	}

	if (sawThemAt == 0)
	{
		sawThemAt = now;
		HudsonStrategies_LoadRoomList((std::string("HudsonChains\\") + LevelName + ".rooms").c_str());
	}

	status->IsImmortal = 1;
	Player->ObStrategyBlock->SBDamageBlock.SB_H_flags.Indestructable = 1;

	if (iAmTheKiller && plan.killKind == kHudsonBotFightQueen)
	{
		Hudson_ArmTheKillerOfAQueen(status);
	}

	// So that a door moving shows in each machine's log
	if (now >= nextReach)
	{
		nextReach = now + 1000;
		HudsonStrategies_SpreadReach();

		if (iAmTheKiller)
		{
			PLAYER_WEAPON_DATA *const weapon = &status->WeaponSlot[status->SelectedWeaponSlot];

			weapon->PrimaryMagazinesRemaining = 9;
		}
	}

	if (isHost)
	{
		if (!looked && now - sawThemAt >= kLooksAfter)
		{
			looked = true;
			target = Hudson_FindTheOneToKill();
		}

		if (target && !Hudson_IsStillThere(target))
		{
			target = NULL;
			LogString("killnamed: on the host it has gone from the list, with no death seen: the level started again, or something removed it");
		}

		// So that it can keep looking at the body once it's dead
		if (target && target->DynPtr)
		{
			whereItWas = target->DynPtr->Position;
		}

		if (target && Hudson_HasDied(target))
		{
			target = NULL;
			diedAt = now;
			LogString("killnamed: on the host it has died");
		}

		if (target && !brought && now - sawThemAt >= kBringsItAfter)
		{
			brought = iAmTheKiller ? Hudson_BringItToTheKiller(target, Player->ObStrategyBlock, other) : Hudson_BringItToTheKiller(target, other, Player->ObStrategyBlock);
		}

		if (target && brought && iAmTheKiller)
		{
			if (!saidShooting)
			{
				saidShooting = true;
				LogString("killnamed: the host is shooting at it");
			}

			Hudson_ShootAt(status, target, now);
			Hudson_LetGoOfTheCannonNowAndThen(status, now);
		}
		else if (target && brought)
		{
			Hudson_FaceIt(status, target);
		}

		if (target && now - lastLine >= 1000)
		{
			lastLine = now;
			_snprintf(line, sizeof(line) - 1, "killnamed: on the host: health %d, at %d %d %d, %d mm from the joiner", target->SBDamageBlock.Health >> 16, target->DynPtr->Position.vx, target->DynPtr->Position.vy, target->DynPtr->Position.vz, VectorDistance(&target->DynPtr->Position, &other->DynPtr->Position));
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}

		// Once it's been brought, so each player's seen it, and again just after it died, when both machines should show the same dead NPC
		if (target && brought && lastPicture == 0 && now - sawThemAt >= kBringsItAfter + kPictureAfterBringing)
		{
			lastPicture = now;
			Hudson_AskForAPicture();
		}

		if (diedAt != 0 && !saidAfter && now - diedAt >= kPictureAfterTheDeath)
		{
			saidAfter = true;
			Hudson_AskForAPicture();
		}

		if (diedAt != 0 && !saidLongAfter && now - diedAt >= kPictureLongAfterTheDeath)
		{
			saidLongAfter = true;
			Hudson_AskForAPicture();
		}

		if (diedAt != 0 && now - diedAt < kLooksAtTheBodyFor)
		{
			Hudson_FaceThePlace(status, whereItWas);
		}

		if (diedAt != 0 && now - diedAt >= kEndsAfterTheKill)
		{
			EndAtOnce("the named NPC has been killed, and the level has had time to move on", kHudsonExitFinished);
		}

		return;
	}

	// The joiner says the number of each ghost of that kind, once, so that the reader can find the host's one among them
	for (int i = 0; i < NumActiveStBlocks && ghostsSaid.size() < 64; i++)
	{
		STRATEGYBLOCK const *const block = ActiveStBlockList[i];

		if (block && block->I_SBtype == I_BehaviourNetGhost && block->SBdataptr && block->DynPtr && Hudson_IsTheKindToKill(block))
		{
			int const number = ((NETGHOSTDATABLOCK const *)block->SBdataptr)->playerObjectId;

			if (std::find(ghostsSaid.begin(), ghostsSaid.end(), number) == ghostsSaid.end())
			{
				ghostsSaid.push_back(number);
				_snprintf(line, sizeof(line) - 1, "killnamed: a ghost on the joiner: object number %d, at %d %d %d", number, block->DynPtr->Position.vx, block->DynPtr->Position.vy, block->DynPtr->Position.vz);
				line[sizeof(line) - 1] = 0;
				LogString(line);
			}
		}
	}

	STRATEGYBLOCK *const ghost = Hudson_NearestOfKind(&Player->ObStrategyBlock->DynPtr->Position, plan.killKind);
	int const inReach = plan.killKind == kHudsonBotFightQueen ? 20000 : 12000;
	bool const ghostInReach = ghost && ghost->I_SBtype == I_BehaviourNetGhost && VectorDistance(&Player->ObStrategyBlock->DynPtr->Position, &ghost->DynPtr->Position) <= inReach;

	if (ghostInReach && iAmTheKiller)
	{
		int const number = ((NETGHOSTDATABLOCK const *)ghost->SBdataptr)->playerObjectId;

		if (number != numberShootingAt)
		{
			numberShootingAt = number;
			_snprintf(line, sizeof(line) - 1, "killnamed: the joiner is shooting at object number %d", number);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}

		Hudson_ShootAt(status, ghost, now);
		Hudson_LetGoOfTheCannonNowAndThen(status, now);
	}
	else if (ghostInReach)
	{
		Hudson_FaceIt(status, ghost);
	}

	if (ghost && ghost->I_SBtype == I_BehaviourNetGhost && now - lastLine >= 1000)
	{
		NETGHOSTDATABLOCK const *const ghostData = (NETGHOSTDATABLOCK const *)ghost->SBdataptr;
		DYNAMICSBLOCK *const body = Player->ObStrategyBlock->DynPtr;
		float const dx = (float)(ghost->DynPtr->Position.vx - body->Position.vx);
		float const dz = (float)(ghost->DynPtr->Position.vz - body->Position.vz);
		int const off = ((((int)(atan2f(dx, dz) * 4096.0f / 6.2831853f) - body->OrientEuler.EulerY) & 4095) + 2048) % 4096 - 2048;

		lastLine = now;
		_snprintf(line, sizeof(line) - 1, "killnamed: on the joiner: object number %d, at %d %d %d, %d mm from me, drawn here %s, has a model %s, in view %s", ghostData->playerObjectId, ghost->DynPtr->Position.vx, ghost->DynPtr->Position.vy, ghost->DynPtr->Position.vz, VectorDistance(&body->Position, &ghost->DynPtr->Position), ghost->SBdptr ? "yes" : "NO", ghostData->HModelController.Root_Section ? "yes" : "NO", off > -512 && off < 512 ? "yes" : "no");
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	// The same moments as the host's. The joiner only knows it has died when its ghost stops being that kind of thing.
	if (ghostInReach && lastPicture == 0)
	{
		lastPicture = now;
	}

	if (ghostInReach && !saidNear && now - lastPicture >= kPictureAfterBringing)
	{
		saidNear = true;
		Hudson_AskForAPicture();
	}

	if (ghostInReach)
	{
		whereItWas = ghost->DynPtr->Position;
	}

	if (wasInReach && !ghostInReach)
	{
		ghostWentAt = now;
	}

	if (ghostWentAt != 0 && now - ghostWentAt < kLooksAtTheBodyFor)
	{
		Hudson_FaceThePlace(status, whereItWas);
	}

	if (ghostWentAt != 0 && !saidAfter && now - ghostWentAt >= kPictureAfterTheDeath)
	{
		saidAfter = true;
		Hudson_AskForAPicture();
	}

	if (ghostWentAt != 0 && !saidLongAfter && now - ghostWentAt >= kPictureLongAfterTheDeath)
	{
		saidLongAfter = true;
		Hudson_AskForAPicture();
	}

	wasInReach = ghostInReach;
}

// Turns him a little towards this, and his gun with his view. A player's direction is kept in OrientMat, and the game works the angles out from that, so that's what has to change. Says how far off he still is, round the level, in 4096ths of a circle.
static int Hudson_TurnTowardsPlace(PLAYER_STATUS *status, VECTORCH const &middle)
{
	static const int kTurnEachSecond = 1400; // in 4096ths of a circle: about a third of a turn
	extern int NormalFrameTime;

	DYNAMICSBLOCK *const body = Player->ObStrategyBlock->DynPtr;

	float const dx = (float)(middle.vx - body->Position.vx);
	float const dz = (float)(middle.vz - body->Position.vz);
	float const across = sqrtf(dx * dx + dz * dz);
	int const facing = ((int)(atan2f((float)body->OrientMat.mat31, (float)body->OrientMat.mat33) * 4096.0f / 6.2831853f)) & 4095;
	int const wantedRound = ((int)(atan2f(dx, dz) * 4096.0f / 6.2831853f)) & 4095;
	int const wantedUpDown = ((int)(atan2f((float)(middle.vy - Global_VDB_Ptr->VDB_World.vy), across) * 4096.0f / 6.2831853f)) & 4095;
	int const most = MUL_FIXED(kTurnEachSecond, NormalFrameTime) + 1;

	body->OrientEuler.EulerX = 0;
	body->OrientEuler.EulerY = HudsonPlan_LookStep(facing, wantedRound, most);
	body->OrientEuler.EulerZ = 0;
	CreateEulerMatrix(&body->OrientEuler, &body->OrientMat);
	TransposeMatrixCH(&body->OrientMat);
	body->AngVelocity.EulerY = 0;
	status->ViewPanX = HudsonPlan_LookStep(status->ViewPanX, wantedUpDown, most);

	int const off = (wantedRound - body->OrientEuler.EulerY) & 4095;

	return off > 2048 ? 4096 - off : off;
}

static int Hudson_TurnTowards(PLAYER_STATUS *status, STRATEGYBLOCK *target)
{
	VECTORCH middle;

	GetTargetingPointOfObject_Far(target, &middle);
	return Hudson_TurnTowardsPlace(status, middle);
}

// Whether nothing stands between this player's eyes and that place, apart from the thing that's there, if there is one. It looks at all that's active, and not only at what's on this machine's screen.
static bool Hudson_WayIsClearTo(STRATEGYBLOCK *me, VECTORCH const &place, DISPLAYBLOCK const *whatIsThere)
{
	VECTORCH eye = Global_VDB_Ptr->VDB_World;
	VECTORCH there = place;
	VECTORCH direction = { place.vx - eye.vx, place.vy - eye.vy, place.vz - eye.vz };
	int const distance = VectorDistance(&eye, &there);

	Normalise(&direction);
	FindPolygonInLineOfSight(&direction, &eye, 0, me->SBdptr);

	return !LOS_ObjectHitPtr || LOS_ObjectHitPtr == whatIsThere || LOS_Lambda >= distance;
}

static bool Hudson_WayIsClear(STRATEGYBLOCK *me, STRATEGYBLOCK *target)
{
	VECTORCH middle;

	GetTargetingPointOfObject_Far(target, &middle);
	return Hudson_WayIsClearTo(me, middle, target->SBdptr);
}

// In a game with no campaign they can start far apart, too far to see each other. So the joiner goes over to the host for the test, on the first side with room, and Hudson_SmokeTest puts him back after it.
static void Hudson_SmokeGoOver(STRATEGYBLOCK *me, STRATEGYBLOCK *other)
{
	static const int kCloseEnoughToSee = 8000; // millimetres
	static const int kStandsThisFarAway = 3000;
	static const int kSides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

	if (plan.netMode != kHudsonNetJoin || VectorDistance(&me->DynPtr->Position, &other->DynPtr->Position) <= kCloseEnoughToSee)
	{
		return;
	}

	VECTORCH there = other->DynPtr->Position;

	for (int side = 0; side < 4; side++)
	{
		VECTORCH direction = { kSides[side][0] * ONE_FIXED, 0, kSides[side][1] * ONE_FIXED };

		FindPolygonInLineOfSight(&direction, &there, 0, other->SBdptr);

		if (!LOS_ObjectHitPtr || LOS_Lambda > kStandsThisFarAway + 1000)
		{
			VECTORCH place = { there.vx + kSides[side][0] * kStandsThisFarAway, there.vy, there.vz + kSides[side][1] * kStandsThisFarAway };

			smokeCameFrom = me->DynPtr->Position;
			smokeWentOver = true;
			MoveThePlayerTo(place);
			LogString("smoke: they started too far apart to see each other, so the joiner has gone over to the host");
			return;
		}
	}

	LogString("smoke: they started too far apart to see each other, and the joiner found no room beside the host");
}

// The user's rule: "if 2 hudsons join they should always look at each other as a 'smoke test' before acting". On every first join and each level's start, in any mode, each player turns to the other, one right and one left
// since they stand side by side, and this machine checks what it can: that the other player's ghost exists, how far it is, that it's in view, and that the game has its model on the list of what's on the screen. A picture
// from this player's eyes is the proof of the rest. It turns him and nothing else, unless they started too far apart, so what a run goes on to measure starts from where it would have. A failed test doesn't stop the run, which
// may still have something to say, but the run doesn't count as a pass: its result, run.txt and the pair's record all say so. True while it's going on, and nothing else a run does may start until it's false.
static bool Hudson_SmokeTest(PLAYER_STATUS *status, STRATEGYBLOCK *me, STRATEGYBLOCK *other, DWORD now)
{
	extern int ObjectIsOnScreen(DISPLAYBLOCK *object);

	if (smokeIsOver)
	{
		return false;
	}

	// One frame after the verdict, because the picture is taken at the end of the frame that gave it
	if (smokeIsFinishing)
	{
		if (smokeWentOver)
		{
			MoveThePlayerTo(smokeCameFrom);
			LogString("smoke: the joiner has gone back to where it started");
		}

		smokeIsOver = true;
		return false;
	}

	// A host can wait a long time on its own, and nothing is wrong with that, so the clock for a body that doesn't come starts once someone else is in the game
	if (smokeWaitingSince == 0)
	{
		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			if (netGameData.playerData[i].playerId != 0 && netGameData.playerData[i].playerId != AvPNetID)
			{
				smokeWaitingSince = now;
			}
		}
	}

	char line[300];

	if (other && smokeSawThemAt == 0)
	{
		smokeSawThemAt = now;
		_snprintf(line, sizeof(line) - 1, "smoke: the other player's body is here, %d mm away, and he's turning to it", VectorDistance(&me->DynPtr->Position, &other->DynPtr->Position));
		line[sizeof(line) - 1] = 0;
		LogString(line);
		Hudson_SmokeGoOver(me, other);
	}

	HudsonSmokeFacts facts;
	int apart = 0;

	memset(&facts, 0, sizeof(facts));
	facts.dead = !status->IsAlive;
	facts.bodyHere = other != NULL;
	facts.millisecondsWaiting = smokeWaitingSince != 0 ? now - smokeWaitingSince : 0;
	facts.millisecondsSeen = smokeSawThemAt != 0 ? now - smokeSawThemAt : 0;

	if (other && !facts.dead)
	{
		VECTORCH middle;

		Hudson_TurnTowards(status, other);
		GetTargetingPointOfObject_Far(other, &middle);
		apart = VectorDistance(&me->DynPtr->Position, &other->DynPtr->Position);
		facts.wayClear = Hudson_WayIsClear(me, other);
		facts.facing = HudsonPlan_IsFacing(me->DynPtr->OrientMat.mat31, me->DynPtr->OrientMat.mat33, middle.vx - me->DynPtr->Position.vx, middle.vz - me->DynPtr->Position.vz);
		facts.drawn = other->SBdptr && other->SBdptr->HModelControlBlock && ObjectIsOnScreen(other->SBdptr);
	}

	const char *why = "";
	int const verdict = HudsonPlan_SmokeVerdict(facts, &why);

	if (verdict == kHudsonSmokeWaiting)
	{
		return true;
	}

	if (verdict == kHudsonSmokePassed)
	{
		smokeLevelsPassed++;
	}
	else if (!smokeFailure[0])
	{
		_snprintf(smokeFailure, sizeof(smokeFailure) - 1, "%s, on %s", why, LevelName);
	}

	netPictureIsDue = true;
	netPictureIsTheSmokeTest = true;
	smokeIsFinishing = true;
	_snprintf(line, sizeof(line) - 1, "smoke: %s%s%s on %s, after %lu ms: the other player's body is %s, %d mm away, facing it: %s, on the screen: %s, the way to it: %s. The picture is lamp-%02d-smoke.png", verdict == kHudsonSmokePassed ? "passed" : "failed: ",
		why, "", LevelName, facts.millisecondsSeen, facts.bodyHere ? "here" : "not here", apart, facts.facing ? "yes" : "no", facts.drawn ? "yes" : "no", facts.wayClear ? "clear" : "blocked", lampCompareNumber + 1);
	line[sizeof(line) - 1] = 0;

	if (verdict == kHudsonSmokePassed)
	{
		LogString(line);
	}
	else
	{
		LogErrorString(line);
	}

	return true;
}

// What someone watching a fight wants: each player looking at what's going on, and not at a wall. The fight aims for itself when it's firing, so on those frames this leaves him alone. It only turns him.
static void Hudson_LookAtWhatMatters(PLAYER_STATUS *status, STRATEGYBLOCK *me, STRATEGYBLOCK *other)
{
	static const int kLooksAtAnNpcWithin = 30000; // millimetres

	bool const theFightAimed = aimedThisFrame;

	aimedThisFrame = false;

	if (!plan.lookAllTheTime || theFightAimed || !status->IsAlive)
	{
		return;
	}

	STRATEGYBLOCK *target = other;
	int const kind = plan.killKind != kHudsonBotFightNone ? plan.killKind : plan.botFight;

	if (kind != kHudsonBotFightNone)
	{
		STRATEGYBLOCK *const npc = Hudson_NearestOfKind(&me->DynPtr->Position, kind);

		if (npc && !(plan.killKind != kHudsonBotFightNone && Hudson_HasDied(npc)) && VectorDistance(&me->DynPtr->Position, &npc->DynPtr->Position) <= kLooksAtAnNpcWithin)
		{
			target = npc;
		}
	}

	if (target)
	{
		Hudson_TurnTowards(status, target);
	}
}

// Whether this ghost is someone else's player, as opposed to an NPC's.
static bool Hudson_IsAPlayersGhost(STRATEGYBLOCK const *block)
{
	AVP_BEHAVIOUR_TYPE const type = ((NETGHOSTDATABLOCK const *)block->SBdataptr)->type;

	return type == I_BehaviourMarinePlayer || type == I_BehaviourPredatorPlayer || type == I_BehaviourAlienPlayer;
}

// Everything a siege fights, on this machine: the real thing on the host and a ghost on the joiner. Says how many of each there are, for the log, and how many are close enough to matter. It gives back the nearest one he can really see,
// wherever it is, as a player would, because firing at one behind a wall only hits the wall. NPC marines are counted, and never aimed at: to marine players they're on the same side.
static STRATEGYBLOCK *Hudson_SiegeNearestInView(STRATEGYBLOCK *me, int within, int *aliens, int *marines, int *predators, int *xenoborgs, int *huggers, int *close)
{
	STRATEGYBLOCK *nearest = NULL;
	int nearestDistance = 0;

	*aliens = *marines = *predators = *xenoborgs = *huggers = *close = 0;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (!block || !block->DynPtr || !block->SBdataptr)
		{
			continue;
		}

		int *const count = Hudson_IsOfKind(block, kHudsonBotFightAlien) ? aliens : Hudson_IsOfKind(block, kHudsonBotFightMarine) ? marines : Hudson_IsOfKind(block, kHudsonBotFightPredator) ? predators : Hudson_IsOfKind(block, kHudsonBotFightXenoborg) ? xenoborgs : Hudson_IsOfKind(block, kHudsonBotFightFacehugger) ? huggers : NULL;

		if (!count)
		{
			continue;
		}

		(*count)++;

		if (count == marines)
		{
			continue;
		}

		int const distance = VectorDistance(&me->DynPtr->Position, &block->DynPtr->Position);

		if (distance > within)
		{
			continue;
		}

		(*close)++;

		if ((!nearest || distance < nearestDistance) && Hudson_WayIsClear(me, block))
		{
			nearest = block;
			nearestDistance = distance;
		}
	}

	return nearest;
}

// Finds a room by its name, in the way a name can be written on a command line: with _ for a space.
static MODULE *Hudson_RoomNamedOnTheCommandLine(const char *wanted)
{
	for (int i = 0; i < AIModuleArraySize; i++)
	{
		for (MODULE **each = AIModuleArray[i].m_module_ptrs; each && *each; each++)
		{
			char const *name = (*each)->name;
			char const *word = wanted;

			while (name && *name && *word && (tolower((unsigned char)*name) == tolower((unsigned char)*word) || (*name == ' ' && *word == '_')))
			{
				name++;
				word++;
			}

			if (name && *name == 0 && *word == 0)
			{
				return *each;
			}
		}
	}

	return NULL;
}

// A player stands a little above a room's floor, which is the bottom of its box, and settles.
static const int kSiegeAboveTheFloor = 110;

// Puts a place that's about right on the floor below it, a little above it, as a body stands. It looks straight down from a metre and a half above, as far as four metres below, so that ground a little higher or lower than
// where it was asked for is found too. A room's box isn't its floor, and the open ground of a level isn't flat: things made at the wrong height sank and stuck for good. Looking only works where the room is awake, and gives false
// with no floor found, and then nothing should be made there.
static bool Hudson_SiegePutOnTheFloor(VECTORCH *place, STRATEGYBLOCK *me)
{
	static const int kLookFromAbove = 1500; // millimetres
	static const int kLookBelow = 4000;

	VECTORCH from = *place;
	VECTORCH down = { 0, ONE_FIXED, 0 };

	from.vy -= kLookFromAbove;

	MODULE *const there = ModuleFromPosition(&from, (MODULE *)0);

	if (!there || !ModuleCurrVisArray || !ModuleCurrVisArray[there->m_index])
	{
		return false;
	}

	FindPolygonInLineOfSight(&down, &from, 0, me->SBdptr);

	if (!LOS_ObjectHitPtr || LOS_Lambda > kLookFromAbove + kLookBelow)
	{
		return false;
	}

	place->vy = from.vy + LOS_Lambda - kSiegeAboveTheFloor;
	return true;
}

// A xenoborg is made asleep, and only wakes after a while once it notices someone. One the siege makes starts waking straight away, which takes it a few seconds, the same way it wakes when it's hurt. CreateXenoborg doesn't say what it
// made, and the only one standing exactly there, before anything has moved, is the one it just made.
static void Hudson_SiegeWakeTheXenoborgAt(VECTORCH const &place)
{
	extern void Xeno_Enter_PowerUp_State(STRATEGYBLOCK *sbPtr);

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (block && block->I_SBtype == I_BehaviourXenoborg && block->DynPtr && block->SBdataptr && block->DynPtr->Position.vx == place.vx && block->DynPtr->Position.vz == place.vz)
		{
			Xeno_Enter_PowerUp_State(block);
			return;
		}
	}
}

// Whether this is one of the players' own side: a friendly marine, a sentry gun, or a player.
static bool Hudson_IsOneOfOurs(STRATEGYBLOCK const *block)
{
	if (!block)
	{
		return false;
	}

	if (block->I_SBtype == I_BehaviourMarine || block->I_SBtype == I_BehaviourAutoGun || block->I_SBtype == I_BehaviourMarinePlayer)
	{
		return true;
	}

	return block->I_SBtype == I_BehaviourNetGhost && block->SBdataptr && Hudson_IsAPlayersGhost(block);
}

// Whether one of his own side is in the way: the first thing along the way he's facing, or anything of ours nearer than the target and close to the line to it. A player in a fight doesn't fire through a friend or past his ear, and
// the game's own friendly fire setting only covers players. The smart gun steers its shots towards what it's tracking, and the minigun spreads them, so the line he faces isn't the only one that matters: between them they killed
// both friendly marines in every run that only looked along it.
static bool Hudson_OneOfOursIsInTheWay(STRATEGYBLOCK *me, STRATEGYBLOCK *target)
{
	static const float kNearTheLine = 0.26f; // radians, about 15 degrees either side of the line to the target
	static const int kMuchNearer = 1000;     // millimetres: something of ours only counts if it's nearer than the target by at least this

	VECTORCH const &mine = me->DynPtr->Position;
	VECTORCH const &theirs = target->DynPtr->Position;
	float const towardsTarget = atan2f((float)(theirs.vx - mine.vx), (float)(theirs.vz - mine.vz));
	int const targetAway = VectorDistance(const_cast<VECTORCH *>(&mine), const_cast<VECTORCH *>(&theirs));

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (!block || block == me || block == target || !block->DynPtr || !Hudson_IsOneOfOurs(block))
		{
			continue;
		}

		VECTORCH const &friendAt = block->DynPtr->Position;
		int const friendAway = VectorDistance(const_cast<VECTORCH *>(&mine), const_cast<VECTORCH *>(&friendAt));

		if (friendAway + kMuchNearer > targetAway)
		{
			continue;
		}

		float off = atan2f((float)(friendAt.vx - mine.vx), (float)(friendAt.vz - mine.vz)) - towardsTarget;

		while (off > 3.14159265f) off -= 6.2831853f;
		while (off < -3.14159265f) off += 6.2831853f;

		if (off < kNearTheLine && off > -kNearTheLine)
		{
			return true;
		}
	}

	VECTORCH eye = Global_VDB_Ptr->VDB_World;
	VECTORCH middle;

	GetTargetingPointOfObject_Far(target, &middle);

	DYNAMICSBLOCK const *const body = me->DynPtr;
	float const across = sqrtf((float)(middle.vx - eye.vx) * (float)(middle.vx - eye.vx) + (float)(middle.vz - eye.vz) * (float)(middle.vz - eye.vz));
	VECTORCH direction = { (int)((float)body->OrientMat.mat31 / 65536.0f * across), middle.vy - eye.vy, (int)((float)body->OrientMat.mat33 / 65536.0f * across) };

	Normalise(&direction);
	FindPolygonInLineOfSight(&direction, &eye, 0, me->SBdptr);

	return LOS_ObjectHitPtr && LOS_ObjectHitPtr != target->SBdptr && Hudson_IsOneOfOurs(LOS_ObjectHitPtr->ObStrategyBlock);
}

// The Stranded test's ending, 30 seconds before the run's own: the host makes a ring of facehuggers round the players, on every side, on open floor with room to stand, a little way out so that they have time to wake and close in.
// Says how many it made. It stops short of the game's limit on things in a level, keeping some for everything else.
static int Hudson_SiegeTheFinale(STRATEGYBLOCK *me)
{
	extern void CreateFacehugger(VECTORCH *Position);

	static const int kHowMany = 12;
	static const int kTries = 16;                         // places round the circle, a sixteenth of a turn apart
	static const int kRings[] = { 18000, 15000, 12000 }; // millimetres out, the furthest tried first
	static const int kBlocksKeptFree = 100;

	int made = 0;

	for (int tried = 0; tried < kTries && made < kHowMany; tried++)
	{
		// Every other place first, so that with too little room they still come from all sides
		int const step = (tried * 2) % kTries + (tried >= kTries / 2 ? 1 : 0);
		float const angle = (float)step * 6.2831853f / (float)kTries;

		for (size_t ring = 0; ring < sizeof(kRings) / sizeof(kRings[0]); ring++)
		{
			if (maxstblocks - NumActiveStBlocks < kBlocksKeptFree)
			{
				LogString("siege: the finale stopped early, to leave the level room for everything else");
				return made;
			}

			VECTORCH place = me->DynPtr->Position;

			place.vx += (int)(sinf(angle) * kRings[ring]);
			place.vz += (int)(cosf(angle) * kRings[ring]);

			if (!Hudson_SiegePutOnTheFloor(&place, me) || !ModuleFromPosition(&place, (MODULE *)0) || Hudson_BotFightRoomAround(&place, me->SBdptr) < kRoomAnNpcNeeds)
			{
				continue;
			}

			char line[140];

			// A facehugger the test makes starts on its feet and running, so it needs no waking
			CreateFacehugger(&place);
			made++;
			_snprintf(line, sizeof(line) - 1, "siege: the finale: a facehugger at %d %d %d, %d mm away", place.vx, place.vy, place.vz, kRings[ring]);
			line[sizeof(line) - 1] = 0;
			LogString(line);
			break;
		}
	}

	return made;
}

// The host makes one alien or xenoborg across a room, at this z, where there's space for it. It goes across in turn, so they don't all come from one place. In the room the players are in, it also has to be where it can be seen coming. Says whether it found anywhere.
static bool Hudson_SiegeMakeOne(int kind, STRATEGYBLOCK *me, MODULE const *room, int alongZ, bool needsView)
{
	extern void CreateXenoborg(VECTORCH *Position, int type);

	static const int kPlacesAcross = 5;
	static int nextPlace = 0;

	int const across = room->m_maxx - room->m_minx;

	for (int tried = 0; tried < kPlacesAcross; tried++)
	{
		int const along = (nextPlace + tried) % kPlacesAcross;
		VECTORCH place = room->m_world;

		place.vx += room->m_minx + across * (along + 1) / (kPlacesAcross + 1);
		place.vy += room->m_maxy - kSiegeAboveTheFloor;
		place.vz += alongZ;

		if (!Hudson_SiegePutOnTheFloor(&place, me) || !ModuleFromPosition(&place, (MODULE *)0) || Hudson_BotFightRoomAround(&place, me->SBdptr) < kRoomAnNpcNeeds || (needsView && !Hudson_WayIsClearTo(me, place, NULL)))
		{
			continue;
		}

		char line[160];

		nextPlace = along + 1;

		if (kind == kHudsonBotFightXenoborg)
		{
			CreateXenoborg(&place, 0);
			Hudson_SiegeWakeTheXenoborgAt(place);
		}
		else
		{
			CreateAlienBot(&place, 0);
		}

		_snprintf(line, sizeof(line) - 1, "siege: the host made %s at %d %d %d, %d mm away, in %.30s", kind == kHudsonBotFightXenoborg ? "a xenoborg" : "an alien", place.vx, place.vy, place.vz, VectorDistance(&me->DynPtr->Position, &place), room->name ? room->name : "no room");
		line[sizeof(line) - 1] = 0;
		LogString(line);
		return true;
	}

	return false;
}

// The host makes a xenoborg in a room further off, on its floor. A room's box isn't its floor: in canyon 02 the bottom of the box was below the ground, and xenoborgs made there sank, stuck for good. So it looks straight down for the
// floor at a few places across the middle of the room, and makes it on the first it finds with room round it. Looking only works where the room is awake, so a room out of sight makes none, and they come from where the others do.
static bool Hudson_SiegeMakeFarOff(STRATEGYBLOCK *me, MODULE const *farRoom)
{
	extern void CreateXenoborg(VECTORCH *Position, int type);

	static const int kPlacesAcross = 5;
	static const int kLookFromAbove = 300;  // millimetres above the bottom of the room's box
	static bool saidAsleep = false, saidNoFloor = false;

	int const across = farRoom->m_maxx - farRoom->m_minx;

	for (int tried = 0; tried < kPlacesAcross; tried++)
	{
		VECTORCH place = farRoom->m_world;

		place.vx += farRoom->m_minx + across * (tried + 1) / (kPlacesAcross + 1);
		place.vy += farRoom->m_maxy - kLookFromAbove;
		place.vz += (farRoom->m_minz + farRoom->m_maxz) / 2;

		MODULE *const there = ModuleFromPosition(&place, (MODULE *)0);

		if (!there)
		{
			continue;
		}

		if (!ModuleCurrVisArray || !ModuleCurrVisArray[there->m_index])
		{
			if (!saidAsleep)
			{
				saidAsleep = true;
				LogString(std::string("siege: ") + (farRoom->name ? farRoom->name : "the far room") + " is out of sight, so asleep, and no floor can be found there: xenoborgs come from where the others do while it is");
			}

			return false;
		}

		if (!Hudson_SiegePutOnTheFloor(&place, me) || !ModuleFromPosition(&place, (MODULE *)0) || Hudson_BotFightRoomAround(&place, me->SBdptr) < kRoomAnNpcNeeds)
		{
			continue;
		}

		char line[170];

		CreateXenoborg(&place, 0);
		Hudson_SiegeWakeTheXenoborgAt(place);
		_snprintf(line, sizeof(line) - 1, "siege: the host made a xenoborg at %d %d %d, %d mm away, in %.30s, on a floor it found", place.vx, place.vy, place.vz, VectorDistance(&me->DynPtr->Position, &place), farRoom->name ? farRoom->name : "no room");
		line[sizeof(line) - 1] = 0;
		LogString(line);
		return true;
	}

	if (!saidNoFloor)
	{
		saidNoFloor = true;
		LogString(std::string("siege: no floor with room round it was found across ") + (farRoom->name ? farRoom->name : "the far room") + " just then, so that xenoborg comes from where the others do. This only says so the first time");
	}

	return false;
}

// The host makes a predator or a facehugger close by, on any side, that he has to turn round and deal with. It starts a couple of dozen metres away on the same floor, with room to stand and in view. A predator starts cloaked. The places
// spread out round him by the same steps each run, so that two runs can be compared, and nothing else's random numbers are used up.
static bool Hudson_SiegeMakeNearby(int kind, STRATEGYBLOCK *me)
{
	extern void CreatePredoBot(VECTORCH *position, int weapon);
	extern void CreateFacehugger(VECTORCH *Position);

	static const int kTries = 12;
	static const int kNearest = 20000, kWidthOfTheRing = 5000; // millimetres
	static const int kStepRound = 1565;                        // in 4096ths of a circle: this step never falls back on where it's been
	static int round = 0, step = 0;

	for (int tried = 0; tried < kTries; tried++)
	{
		round = (round + kStepRound) & 4095;
		step++;

		float const angle = (float)round * 6.2831853f / 4096.0f;
		int const away = kNearest + (step * 1700) % kWidthOfTheRing;
		VECTORCH place = me->DynPtr->Position;

		place.vx += (int)(sinf(angle) * away);
		place.vz += (int)(cosf(angle) * away);

		if (!Hudson_SiegePutOnTheFloor(&place, me) || !ModuleFromPosition(&place, (MODULE *)0) || Hudson_BotFightRoomAround(&place, me->SBdptr) < kRoomAnNpcNeeds || !Hudson_WayIsClearTo(me, place, NULL))
		{
			continue;
		}

		bool cloaked = false;

		if (kind == kHudsonBotFightPredator)
		{
			CreatePredoBot(&place, 0);

			// It doesn't say what it made, and the only one standing exactly there, before anything has moved, is the one it just made. This is how the game itself starts a cloak.
			for (int i = 0; i < NumActiveStBlocks && !cloaked; i++)
			{
				STRATEGYBLOCK *const block = ActiveStBlockList[i];

				if (block && block->I_SBtype == I_BehaviourPredator && block->DynPtr && block->SBdataptr && block->DynPtr->Position.vx == place.vx && block->DynPtr->Position.vz == place.vz)
				{
					PREDATOR_STATUS_BLOCK *const predator = (PREDATOR_STATUS_BLOCK *)block->SBdataptr;

					predator->CloakStatus = PCLOAK_Activating;
					predator->CloakTimer = ONE_FIXED - 1;
					cloaked = true;
				}
			}
		}
		else
		{
			CreateFacehugger(&place);
		}

		char line[170];

		_snprintf(line, sizeof(line) - 1, "siege: the host made %s at %d %d %d, %d mm away", kind == kHudsonBotFightPredator ? (cloaked ? "a cloaked predator" : "a predator it couldn't cloak") : "a facehugger", place.vx, place.vy, place.vz, away);
		line[sizeof(line) - 1] = 0;
		LogString(line);
		return true;
	}

	return false;
}

// The two marines on the players' side. They stand either side of the players and a little ahead, in both windows and out of the line from the players and the guns to the canyon, on guard and facing the same way. NPC marines don't shoot marine players, and sentry guns and these players don't shoot them.
static void Hudson_SiegeMakeTheMarines(STRATEGYBLOCK *me, VECTORCH const &lookingAt)
{
	static const int kToTheSide = 14500; // millimetres: 10 metres outside each gun, so that the guns are out of their line of fire towards the canyon
	static const int kAhead = 2000;      // and a little ahead of the host, behind the guns' line

	for (int side = -1; side <= 1; side += 2)
	{
		VECTORCH place = me->DynPtr->Position;
		char line[150];

		place.vx += side * kToTheSide;
		place.vz += kAhead;

		if (!Hudson_SiegePutOnTheFloor(&place, me) || !ModuleFromPosition(&place, (MODULE *)0) || Hudson_BotFightRoomAround(&place, me->SBdptr) < kRoomAnNpcNeeds)
		{
			_snprintf(line, sizeof(line) - 1, "siege: there's no room for a friendly marine at %d %d %d", place.vx, place.vy, place.vz);
			line[sizeof(line) - 1] = 0;
			LogString(line);
			continue;
		}

		STRATEGYBLOCK *const made = CreateMarineBot(&place, MNPCW_PulseRifle + 1);

		// The way the level file sets up a guard: this is his post, and that's what he faces. A guard walks back to his post if he's anywhere else, and one made during a level never had one. He starts with as much health as the siege keeps him at.
		if (made)
		{
			Marine_GuardAndFace(made, &lookingAt);
			made->SBDamageBlock.Health = 1000 << ONE_FIXED_SHIFT;
		}

		_snprintf(line, sizeof(line) - 1, "siege: the host made a friendly marine at %d %d %d, %s", place.vx, place.vy, place.vz, made ? "on guard" : "or tried to, and couldn't");
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}
}

// What a blow came from, by the ammunition the game gave it, in words. Players' own guns are named by who carries them in a siege.
static const char *Hudson_WhatHit(int ammunition)
{
	switch (ammunition)
	{
		case AMMO_SMARTGUN: return "the host's smart gun";
		case AMMO_MINIGUN: return "the joiner's minigun";
		case AMMO_AUTOGUN: return "a sentry gun";
		case AMMO_10MM_CULW_NPC: return "a friendly marine's rifle";
		case AMMO_ALIEN_CLAW:
		case AMMO_NPC_ALIEN_CLAW: return "an alien's claw";
		case AMMO_ALIEN_TAIL:
		case AMMO_NPC_ALIEN_TAIL: return "an alien's tail";
		case AMMO_NPC_ALIEN_BITE:
		case AMMO_ALIEN_BITE_KILLSECTION: return "an alien's bite";
		case AMMO_ALIEN_FRAG: return "an alien's acid blood";
		case AMMO_ALIEN_DEATH: return "an alien dying";
		case AMMO_XENOBORG: return "a xenoborg";
		case AMMO_FACEHUGGER: return "a facehugger";
		case AMMO_PRED_WRISTBLADE:
		case AMMO_HEAVY_PRED_WRISTBLADE: return "a predator's blades";
		case AMMO_PRED_PISTOL:
		case AMMO_PREDPISTOL_STRIKE: return "a predator's pistol";
		case AMMO_PRED_ENERGY_BOLT:
		case AMMO_PLASMACASTER_NPCKILL:
		case AMMO_PLASMACASTER_PCKILL: return "a predator's plasma caster";
		case AMMO_PRED_DISC:
		case AMMO_PRED_DISC_PM: return "a predator's disc";
		case AMMO_NPC_PRED_STAFF:
		case AMMO_PRED_STAFF: return "a predator's staff";
		case AMMO_NPC_OBSTACLE_CLEAR:
		case AMMO_ALIEN_OBSTACLE_CLEAR: return "something clearing its way";
		default: return NULL;
	}
}

// Blows on the friendly marines and the sentry guns since the last line, counted by what they came from. There are only ever a few of these, so a short list will do.
struct SiegeBlows
{
	STRATEGYBLOCK const *target;
	int ammunition;
	int count;
};

static SiegeBlows siegeBlows[32];
static int siegeBlowsNoted = 0;

// Blows turned away from this machine's own player since the last line, counted by what they came from, with the sum of the damage each would have done before armour.
static int siegeHitsTurnedAway[256];
static int siegeDamageTurnedAway[256];

bool Hudson_SiegeTurnsAwayHit(STRATEGYBLOCK const *player, DAMAGE_PROFILE const *damage, int multiple)
{
	if (!HudsonRunIsOn || !HudsonPlan_PlayersCannotDie(plan) || !player || !damage)
	{
		return false;
	}

	int const ammunition = (int)damage->Id;
	int const kind = ammunition >= 0 && ammunition < 255 ? ammunition : 255;
	int const amount = damage->Impact + damage->Cutting + damage->Penetrative + damage->Fire + damage->Electrical + damage->Acid;

	siegeHitsTurnedAway[kind]++;
	siegeDamageTurnedAway[kind] += MUL_FIXED(amount, multiple);
	return true;
}

// Once a second, on each machine: what would have hurt this player, so that a run still shows what would have killed him.
static void Hudson_SiegeSayWhatWasTurnedAway()
{
	for (int kind = 0; kind < 256; kind++)
	{
		if (siegeHitsTurnedAway[kind] == 0)
		{
			continue;
		}

		char const *const what = kind < 255 ? Hudson_WhatHit(kind) : NULL;
		char line[200];

		_snprintf(line, sizeof(line) - 1, "siege: in the last second %d blows on this player were turned away, from %s%s%d, %d damage before armour", siegeHitsTurnedAway[kind], what ? what : "ammunition number ", what ? "" : "", what ? 0 : kind,
			siegeDamageTurnedAway[kind]);
		line[sizeof(line) - 1] = 0;

		// The number after the name is only wanted when there's no name
		if (what)
		{
			_snprintf(line, sizeof(line) - 1, "siege: in the last second %d blows on this player were turned away, from %s, %d damage before armour", siegeHitsTurnedAway[kind], what, siegeDamageTurnedAway[kind]);
			line[sizeof(line) - 1] = 0;
		}

		LogString(line);
		siegeHitsTurnedAway[kind] = 0;
		siegeDamageTurnedAway[kind] = 0;
	}
}

void Hudson_NoteDamage(STRATEGYBLOCK *target, int ammunition)
{
	if (!HudsonRunIsOn || !plan.siegeRoom[0] || !target || (target->I_SBtype != I_BehaviourMarine && target->I_SBtype != I_BehaviourAutoGun))
	{
		return;
	}

	for (int i = 0; i < siegeBlowsNoted; i++)
	{
		if (siegeBlows[i].target == target && siegeBlows[i].ammunition == ammunition)
		{
			siegeBlows[i].count++;
			return;
		}
	}

	if (siegeBlowsNoted < (int)(sizeof(siegeBlows) / sizeof(siegeBlows[0])))
	{
		siegeBlows[siegeBlowsNoted].target = target;
		siegeBlows[siegeBlowsNoted].ammunition = ammunition;
		siegeBlows[siegeBlowsNoted].count = 1;
		siegeBlowsNoted++;
	}
}

// Once a second on the host. The guns can't run out, and their health and the marines' is put back. That doesn't keep them alive: breaking a vital part, like a head, kills whatever its health is. Then each that was hit says by what, and
// one that has died says so, with the last things that hit it. The blocks are only compared, never followed, once they may have gone.
static void Hudson_SiegeLooksAfterItsOwn(STRATEGYBLOCK *me)
{
	static const int kHealthItIsKeptAt = 1000;
	static STRATEGYBLOCK const *alive[8];
	static VECTORCH whereTheyWere[8];
	static int aliveCount = 0;

	STRATEGYBLOCK const *aliveNow[8];
	VECTORCH whereNow[8];
	int aliveNowCount = 0;

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const block = ActiveStBlockList[i];

		if (!block || !block->SBdataptr || !block->DynPtr)
		{
			continue;
		}

		bool const isAGun = block->I_SBtype == I_BehaviourAutoGun;

		if (!isAGun && block->I_SBtype != I_BehaviourMarine)
		{
			continue;
		}

		if (isAGun)
		{
			((AUTOGUN_STATUS_BLOCK *)block->SBdataptr)->ammo = 500;
		}
		else
		{
			STRATEGYBLOCK const *const target = ((MARINE_STATUS_BLOCK const *)block->SBdataptr)->Target;

			if (target == me || (target && target->I_SBtype == I_BehaviourNetGhost && Hudson_IsAPlayersGhost(target)))
			{
				LogErrorString("siege: a friendly marine is aiming at a player");
			}
		}

		block->SBDamageBlock.Health = kHealthItIsKeptAt << ONE_FIXED_SHIFT;

		if (aliveNowCount < 8)
		{
			aliveNow[aliveNowCount] = block;
			whereNow[aliveNowCount] = block->DynPtr->Position;
			aliveNowCount++;
		}
	}

	char line[400];

	for (int i = 0; i < siegeBlowsNoted; i++)
	{
		bool stillHere = false;
		int at = 0;

		for (int j = 0; j < aliveNowCount; j++)
		{
			if (aliveNow[j] == siegeBlows[i].target)
			{
				stillHere = true;
				at = j;
			}
		}

		if (!stillHere)
		{
			continue;
		}

		char const *const what = Hudson_WhatHit(siegeBlows[i].ammunition);
		char unknown[40];

		_snprintf(unknown, sizeof(unknown) - 1, "ammunition number %d", siegeBlows[i].ammunition);
		unknown[sizeof(unknown) - 1] = 0;
		_snprintf(line, sizeof(line) - 1, "siege: %s at %d %d %d was hit %d times by %s in the last second", siegeBlows[i].target->I_SBtype == I_BehaviourAutoGun ? "a sentry gun" : "a friendly marine", whereNow[at].vx, whereNow[at].vy, whereNow[at].vz,
			siegeBlows[i].count, what ? what : unknown);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	// One that was here a second ago and isn't now has died: a dead one is a corpse, which is another kind of thing
	for (int i = 0; i < aliveCount; i++)
	{
		bool stillHere = false;

		for (int j = 0; j < aliveNowCount; j++)
		{
			stillHere = stillHere || aliveNow[j] == alive[i];
		}

		if (stillHere)
		{
			continue;
		}

		int written = _snprintf(line, sizeof(line) - 1, "siege: a friendly marine or a sentry gun at %d %d %d has died. In its last second it was hit by:", whereTheyWere[i].vx, whereTheyWere[i].vy, whereTheyWere[i].vz);
		bool anything = false;

		for (int k = 0; k < siegeBlowsNoted && written > 0 && written < (int)sizeof(line) - 60; k++)
		{
			if (siegeBlows[k].target == alive[i])
			{
				char const *const what = Hudson_WhatHit(siegeBlows[k].ammunition);
				int const more = what ? _snprintf(line + written, sizeof(line) - 1 - written, " %s %d times,", what, siegeBlows[k].count) : _snprintf(line + written, sizeof(line) - 1 - written, " ammunition number %d %d times,", siegeBlows[k].ammunition, siegeBlows[k].count);

				written = more > 0 ? written + more : (int)sizeof(line) - 1;
				anything = true;
			}
		}

		if (!anything && written > 0 && written < (int)sizeof(line) - 20)
		{
			_snprintf(line + written, sizeof(line) - 1 - written, " nothing that was noted");
		}

		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	for (int i = 0; i < aliveNowCount; i++)
	{
		alive[i] = aliveNow[i];
		whereTheyWere[i] = whereNow[i];
	}

	aliveCount = aliveNowCount;
	siegeBlowsNoted = 0;
}

// siege=: what the user asked to watch, a stand-up fight to show up whatever doesn't match between two machines, and with siegefrom= the Stranded test. After the smoke test both players go to the room the run names, in a line towards where the enemies come
// from: the joiner a little behind the host and to one side, so that its window has the host's animations in it. Two sentry guns stand in front of the host and two friendly marines either side. Each player has a machine gun that never runs out, and
// neither can die. Aliens and xenoborgs come from the far room, which the guns face. Predators and facehuggers start close by on any side. Each player turns to the nearest he can really see, wherever it is, and fires once he's facing it, and looks towards
// the far room when there's nothing to shoot. What's being tested is what each machine then shows. Queens aren't made, because the game can't make those during a level.
static void Hudson_Siege(PLAYER_STATUS *status, STRATEGYBLOCK *me, STRATEGYBLOCK *other, DWORD now)
{
	extern int SlotForThisWeapon(enum WEAPON_ID weaponID);

	static const int kRoster[] = { kHudsonBotFightAlien, kHudsonBotFightFacehugger, kHudsonBotFightAlien, kHudsonBotFightPredator, kHudsonBotFightAlien, kHudsonBotFightFacehugger, kHudsonBotFightAlien, kHudsonBotFightXenoborg };
	static const int kMostAtOnce = 6;
	static const DWORD kBetweenEnemies = 3000;
	static const int kFiresWithin = 50;             // in 4096ths of a circle: about 4 degrees
	static const DWORD kFinaleBeforeTheEnd = 30000; // the finale starts this long before the run's own time is up
	static const int kLooksAtAnEnemyWithin = 60000; // millimetres
	static const int kHostFromTheNearSide = 6000;   // the host stands this far into the room, which leaves it the width of the room to see across
	static const int kHostBehindTheJoiner = 3000;   // and the host is this much nearer the enemies than the joiner
	static const int kJoinerToTheSide = 1200;       // and the joiner is this far to one side, so as not to look through the host's back
	static const int kGunsInFront = 4000;           // the sentry guns are this far in front of the host, ahead of everyone, so nothing of ours is between them and the canyon
	static const int kGunsApart = 9000;             // and this far apart, side by side
	static const int kFarSideFromTheEdge = 3000;    // with no far room, enemies are made this far from the far side of this one

	static bool arrived = false, armed = false, gunsMade = false, saidNowhere = false;
	static MODULE *room = NULL;
	static MODULE *enemiesFrom = NULL;
	static MODULE *xenoborgsFrom = NULL;
	static DWORD nextEnemy = 0, nextLine = 0, nextAmmunition = 0, gunsAt = 0;
	static int nextOnTheRoster = 0;
	static int heldFire = 0;
	static bool finaleDone = false;
	bool const isHost = plan.netMode == kHudsonNetHost;

	aimedThisFrame = true;

	// The ending, on the host, once: the run's own clock started when the game did
	if (isHost && room && !finaleDone && plan.seconds * 1000 > (int)kFinaleBeforeTheEnd && GetTickCount() - startedAt >= (DWORD)plan.seconds * 1000 - kFinaleBeforeTheEnd)
	{
		char line[100];

		finaleDone = true;
		_snprintf(line, sizeof(line) - 1, "siege: the finale: %d facehuggers round them", Hudson_SiegeTheFinale(me));
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	if (!arrived)
	{
		arrived = true;
		room = Hudson_RoomNamedOnTheCommandLine(plan.siegeRoom);
		enemiesFrom = plan.siegeFrom[0] ? Hudson_RoomNamedOnTheCommandLine(plan.siegeFrom) : NULL;

		if (plan.siegeFrom[0] && !enemiesFrom)
		{
			LogErrorString(std::string("siege: this level has no room called ") + plan.siegeFrom + ", so the enemies come from the far side of the room he's in");
		}

		xenoborgsFrom = plan.siegeFar[0] ? Hudson_RoomNamedOnTheCommandLine(plan.siegeFar) : NULL;

		if (plan.siegeFar[0] && !xenoborgsFrom)
		{
			LogErrorString(std::string("siege: this level has no room called ") + plan.siegeFar + ", so the xenoborgs come from where the others do");
		}

		if (room)
		{
			VECTORCH place = room->m_world;
			char line[200];

			place.vx += (room->m_minx + room->m_maxx) / 2 + (isHost ? 0 : -kJoinerToTheSide);
			place.vy += room->m_maxy - kSiegeAboveTheFloor;
			place.vz += room->m_minz + kHostFromTheNearSide - (isHost ? 0 : kHostBehindTheJoiner);

			if (ModuleFromPosition(&place, (MODULE *)0))
			{
				MoveThePlayerTo(place);
				_snprintf(line, sizeof(line) - 1, "siege: the room is %.30s, %d by %d metres, and he's at %d %d %d. Aliens and xenoborgs come from %.30s", room->name, (room->m_maxx - room->m_minx) / 1000, (room->m_maxz - room->m_minz) / 1000, place.vx, place.vy, place.vz, enemiesFrom && enemiesFrom->name ? enemiesFrom->name : "its far side");
				line[sizeof(line) - 1] = 0;
				LogString(line);
			}
			else
			{
				room = NULL;
				LogErrorString(std::string("siege: there's nowhere to stand in ") + plan.siegeRoom + ", so he stays where he is, and no enemies are made");
			}
		}
		else
		{
			LogErrorString(std::string("siege: this level has no room called ") + plan.siegeRoom + ", so he stays where he is, and no enemies are made");
		}

		nextEnemy = now + kBetweenEnemies;
		gunsAt = now + 1000;
	}

	status->invulnerabilityTimer = ONE_FIXED * 10;

	// No pictures during the fight. A picture is written on the game's own thread, and at 2560x1440 each one stopped the game for up to a second and a half. The two films show it all, with the clock in their corner, and
	// ghostlog has the numbers. The smoke test's pictures are taken before the fight starts.

	// Where the enemies come from, which the guns and the marines face, and the players do when there's nothing to shoot
	VECTORCH lookingAt = me->DynPtr->Position;

	if (room)
	{
		MODULE const *const farRoom = enemiesFrom ? enemiesFrom : room;

		lookingAt = farRoom->m_world;
		lookingAt.vx += (farRoom->m_minx + farRoom->m_maxx) / 2;
		lookingAt.vy = room->m_world.vy + room->m_maxy - 1000;
		lookingAt.vz += enemiesFrom ? (farRoom->m_minz + farRoom->m_maxz) / 2 : farRoom->m_maxz - kFarSideFromTheEdge;
	}

	// A moment after he's got there, so that the rooms round him are awake. These run on the host, and the joiner has their ghosts.
	if (isHost && room && !gunsMade && now >= gunsAt)
	{
		extern void CreateSentrygun(VECTORCH *Position, int type);

		gunsMade = true;

		for (int side = -1; side <= 1; side += 2)
		{
			VECTORCH place = me->DynPtr->Position;

			place.vx += side * kGunsApart / 2;
			place.vz += kGunsInFront;

			// Where there's no floor to be found, the gun stands at his height, as before
			Hudson_SiegePutOnTheFloor(&place, me);
			CreateSentrygun(&place, 0);
		}

		LogString("siege: the host made two sentry guns in front of him");
		Hudson_SiegeMakeTheMarines(me, lookingAt);
	}

	if (!armed)
	{
		armed = true;
		GiveAllWeaponsCheat();
		LogString(isHost ? "siege: the host has the smart gun" : "siege: the joiner has the minigun");
	}

	int const slot = SlotForThisWeapon(isHost ? WEAPON_SMARTGUN : WEAPON_MINIGUN);

	if (slot >= 0 && (int)status->SelectedWeaponSlot != slot)
	{
		status->Mvt_InputRequests.Flags.Rqst_WeaponNo = slot + 1;
	}

	if (now >= nextAmmunition)
	{
		nextAmmunition = now + 1000;
		Hudson_SiegeSayWhatWasTurnedAway();
		status->WeaponSlot[status->SelectedWeaponSlot].PrimaryMagazinesRemaining = 9;

		if (isHost)
		{
			Hudson_SiegeLooksAfterItsOwn(me);
		}
	}

	int aliens = 0, marines = 0, predators = 0, xenoborgs = 0, huggers = 0, close = 0;
	STRATEGYBLOCK *const nearest = Hudson_SiegeNearestInView(me, kLooksAtAnEnemyWithin, &aliens, &marines, &predators, &xenoborgs, &huggers, &close);
	int const distance = nearest ? VectorDistance(&me->DynPtr->Position, &nearest->DynPtr->Position) : 0;

	// Only those close by count: the level's own can be a long way off, and they shouldn't stop any being made here
	if (isHost && room && now >= nextEnemy && close < kMostAtOnce)
	{
		int const kind = kRoster[nextOnTheRoster];
		bool made = false;

		nextEnemy = now + kBetweenEnemies;

		if (kind == kHudsonBotFightPredator || kind == kHudsonBotFightFacehugger)
		{
			made = Hudson_SiegeMakeNearby(kind, me);
		}
		else
		{
			// A xenoborg needs time to wake up and walk in, so it starts further off, round the canyon's corner, if the run says where
			if (kind == kHudsonBotFightXenoborg && xenoborgsFrom)
			{
				made = Hudson_SiegeMakeFarOff(me, xenoborgsFrom);
			}

			// The far room may not be awake, if nothing there can be seen from here. Then they come from the far side of this one.
			made = made || (enemiesFrom && Hudson_SiegeMakeOne(kind, me, enemiesFrom, (enemiesFrom->m_minz + enemiesFrom->m_maxz) / 2, false));
			made = made || Hudson_SiegeMakeOne(kind, me, room, room->m_maxz - kFarSideFromTheEdge, true);
		}

		if (made)
		{
			nextOnTheRoster = (nextOnTheRoster + 1) % (sizeof(kRoster) / sizeof(kRoster[0]));
		}
		else if (!saidNowhere)
		{
			saidNowhere = true;
			LogString("siege: nowhere had space for an enemy and a clear view of it just then, and it goes on trying. This only says so the first time");
		}
	}

	if (now >= nextLine)
	{
		char line[230];
		char inView[40] = "none in view";

		if (nearest)
		{
			_snprintf(inView, sizeof(inView) - 1, "the nearest in view %d mm away", distance);
		}

		nextLine = now + 5000;
		_snprintf(line, sizeof(line) - 1, "siege: this machine has %d aliens, %d predators, %d xenoborgs, %d facehuggers and %d friendly marines, %d enemies close, %s, my health is %d, and he held his fire %d times for one of his own in the way", aliens, predators, xenoborgs, huggers, marines, close, inView, me->SBDamageBlock.Health >> ONE_FIXED_SHIFT, heldFire);
		line[sizeof(line) - 1] = 0;
		LogString(line);
		heldFire = 0;
	}

	if (nearest)
	{
		if (Hudson_TurnTowards(status, nearest) <= kFiresWithin)
		{
			if (Hudson_OneOfOursIsInTheWay(me, nearest))
			{
				heldFire++;
			}
			else
			{
				status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;
			}
		}
	}
	else if (room)
	{
		Hudson_TurnTowardsPlace(status, lookingAt);
	}
	else if (other)
	{
		Hudson_TurnTowards(status, other);
	}
}

// What the proof is after, in the log: each game says who it is and whether it can see the other. The joiner stands 2 metres from the host and shoots it, first with the pulse rifle, then the shotgun, and then leaves, so that the host can show it carries on.
// ratetest=fall: lifts the player to each height in turn, lets go, and says how long the fall took and what it cost.
// A fall's damage goes by the speed at the bottom, so if frame rate changes how fast things fall, it changes this.
// The player gets their health back after each one, so one fall can't change the next.
static void Hudson_RateTestFall(STRATEGYBLOCK *me)
{
	static const int kHeights[] = { 3000, 5000, 6000, 7000, 8000, 9000, 10000, 12000 };
	static const int kHeightsToTry = sizeof(kHeights) / sizeof(kHeights[0]);
	static const DWORD kMillisecondsOnTheFloorFirst = 1500, kMillisecondsBeforeLandingCounts = 100;
	static const int kRoomOverTheHead = 2500;
	static int next = 0, health = 0, armour = 0;
	static bool falling = false, saidItsDone = false;
	static DWORD onTheFloorSince = 0, letGoAt = 0;

	DWORD const now = GetTickCount();
	DYNAMICSBLOCK *const body = me->DynPtr;
	char line[160];

	if (health == 0)
	{
		health = me->SBDamageBlock.Health;
		armour = me->SBDamageBlock.Armour;
	}

	if (falling)
	{
		if (now - letGoAt > kMillisecondsBeforeLandingCounts && body->IsInContactWithFloor)
		{
			_snprintf(line, sizeof(line) - 1, "ratetest: fall from %d mm: %lu ms, health %d to %d, armour %d to %d", kHeights[next], (unsigned long)(now - letGoAt),
				health >> 16, me->SBDamageBlock.Health >> 16, armour >> 16, me->SBDamageBlock.Armour >> 16);
			line[sizeof(line) - 1] = 0;
			LogString(line);
			me->SBDamageBlock.Health = health;
			me->SBDamageBlock.Armour = armour;
			falling = false;
			onTheFloorSince = now;
			next++;
		}

		return;
	}

	if (!body->IsInContactWithFloor || onTheFloorSince == 0)
	{
		onTheFloorSince = now;
		return;
	}

	if (next >= kHeightsToTry)
	{
		if (!saidItsDone)
		{
			saidItsDone = true;
			LogString("ratetest: every fall has been tried");
		}

		return;
	}

	if (now - onTheFloorSince < kMillisecondsOnTheFloorFirst)
	{
		return;
	}

	VECTORCH up = { 0, -ONE_FIXED, 0 };

	FindPolygonInLineOfSight(&up, &body->Position, kLookAtAllThatsActive, me->SBdptr);

	if (LOS_ObjectHitPtr && LOS_Lambda < kHeights[next] + kRoomOverTheHead)
	{
		_snprintf(line, sizeof(line) - 1, "ratetest: no fall from %d mm: the ceiling is %d mm up", kHeights[next], LOS_Lambda);
		line[sizeof(line) - 1] = 0;
		LogString(line);
		next++;
		return;
	}

	body->Position.vy -= kHeights[next];
	body->PrevPosition = body->Position;
	body->LinImpulse.vx = body->LinImpulse.vy = body->LinImpulse.vz = 0;
	body->LinVelocity.vx = body->LinVelocity.vy = body->LinVelocity.vz = 0;
	falling = true;
	letGoAt = now;
}

// ratetest=flame: a marine who stands still at each distance in turn, and the player's flamethrower on them until they
// die or the time's up. The audit says flames lose a fixed amount of distance each frame and hit once each frame they
// touch, so frame rate should change how far a flame reaches and how fast it kills. Nothing else moves, so a burn from
// a marine catching fire isn't counted, only the flame's own hits. That's the same at every rate, which is what matters.
static void Hudson_RateTestFlame(PLAYER_STATUS *status, STRATEGYBLOCK *me)
{
	static const int kDistances[] = { 3000, 5000, 6000, 7000, 8000 };
	static const int kDistancesToTry = sizeof(kDistances) / sizeof(kDistances[0]);
	static const int kSides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	static const DWORD kMillisecondsBeforeStarting = 3000, kMillisecondsAtEach = 6000, kMillisecondsBetween = 2000;
	static int next = 0, side = -1, slot = -1, targetHealth = 0;
	static bool burning = false, saidItsDone = false;
	static DWORD started = 0, burningSince = 0, restingSince = 0;
	static char targetName[SB_NAME_LENGTH];

	DWORD const now = GetTickCount();
	char line[160];

	if (started == 0)
	{
		started = now;
		restingSince = now;
	}

	if (now - started < kMillisecondsBeforeStarting)
	{
		return;
	}

	if (slot < 0)
	{
		GiveAllWeaponsCheat();

		for (int i = 0; i < MAX_NO_OF_WEAPON_SLOTS; i++)
		{
			if (status->WeaponSlot[i].WeaponIDNumber == WEAPON_FLAMETHROWER)
			{
				slot = i;
			}
		}

		if (slot < 0)
		{
			LogString("ratetest: this player has no flamethrower");
			slot = MAX_NO_OF_WEAPON_SLOTS;
			saidItsDone = true;
		}

		return;
	}

	if (slot >= MAX_NO_OF_WEAPON_SLOTS)
	{
		return;
	}

	if ((int)status->SelectedWeaponSlot != slot)
	{
		status->Mvt_InputRequests.Flags.Rqst_WeaponNo = slot + 1;
		return;
	}

	// As much as it can hold, so running out isn't what ends one
	status->WeaponSlot[slot].PrimaryRoundsRemaining = 100 * ONE_FIXED;

	if (side < 0)
	{
		int mostRoom = 0;

		for (int i = 0; i < 4; i++)
		{
			VECTORCH direction = { kSides[i][0] * ONE_FIXED, 0, kSides[i][1] * ONE_FIXED };

			FindPolygonInLineOfSight(&direction, &me->DynPtr->Position, kLookAtAllThatsActive, me->SBdptr);

			int const room = LOS_ObjectHitPtr ? LOS_Lambda : 0x7fffffff;

			if (room > mostRoom)
			{
				mostRoom = room;
				side = i;
			}
		}

		_snprintf(line, sizeof(line) - 1, "ratetest: flame down the side with the most room, %d mm", mostRoom);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	if (!burning)
	{
		if (next >= kDistancesToTry)
		{
			if (!saidItsDone)
			{
				saidItsDone = true;
				LogString("ratetest: every distance has been tried");
			}

			return;
		}

		if (now - restingSince < kMillisecondsBetween)
		{
			return;
		}

		VECTORCH place = me->DynPtr->Position;

		place.vx += kSides[side][0] * kDistances[next];
		place.vz += kSides[side][1] * kDistances[next];

		STRATEGYBLOCK *const target = CreateMarineBot(&place, 0);

		if (!target)
		{
			LogString("ratetest: a marine couldn't be made");
			next++;
			return;
		}

		memcpy(targetName, target->SBname, SB_NAME_LENGTH);
		targetHealth = target->SBDamageBlock.Health;
		Hudson_BotFightFaceTheJoiner(me, target);
		burning = true;
		burningSince = now;
	}

	status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;

	STRATEGYBLOCK *const target = FindSBWithName(targetName);
	bool const alive = target && target->I_SBtype == I_BehaviourMarine;

	if (!alive || now - burningSince >= kMillisecondsAtEach)
	{
		_snprintf(line, sizeof(line) - 1, "ratetest: flame at %d mm: %lu ms, marine's health %d to %d, %s", kDistances[next], (unsigned long)(now - burningSince),
			targetHealth >> 16, alive ? target->SBDamageBlock.Health >> 16 : 0, alive ? "still alive" : "dead");
		line[sizeof(line) - 1] = 0;
		LogString(line);

		if (alive)
		{
			DestroyAnyStrategyBlock(target);
		}

		burning = false;
		restingSince = now;
		next++;
	}
}

void Hudson_NetInput(void *playerStatus)
{
	if (HudsonRunIsOn && plan.rateTest != kHudsonRateTestNone && !levelHasEnded)
	{
		if (plan.rateTest == kHudsonRateTestFall)
		{
			Hudson_RateTestFall(Player->ObStrategyBlock);
		}
		else
		{
			Hudson_RateTestFlame((PLAYER_STATUS *)playerStatus, Player->ObStrategyBlock);
		}

		return;
	}

	// The bot fight in plain single player: this player is both the one who makes the NPC and the one it goes for
	if (HudsonRunIsOn && plan.botSingle && !levelHasEnded)
	{
		Hudson_NetBotFight((PLAYER_STATUS *)playerStatus, Player->ObStrategyBlock);
		return;
	}

	if (!HudsonRunIsOn || plan.netMode == kHudsonNetNone || levelHasEnded)
	{
		return;
	}

	// Seconds from when the joiner first saw the host
	static const DWORD kGiveGunsAt = 3, kGoOverAfter = 5, kShotgunFrom = 8, kShotgunUntil = 14, kGoOverAgainAt = 26, kSkeeterFrom = 30, kSkeeterUntil = 46, kLeaveAt = 60;

	// The shotgun's slot, counted from 0. On the keys it's 10.
	static const int kShotgunSlot = 9;

	static DWORD started = 0, lastLine = 0, sawThemAt = 0;
	static int lastHealth = -1;
	static bool wentOver = false, gaveGuns = false;

	PLAYER_STATUS *const status = (PLAYER_STATUS *)playerStatus;
	STRATEGYBLOCK *const me = Player->ObStrategyBlock;
	STRATEGYBLOCK *const other = Hudson_NetOtherPlayer();
	DWORD const now = GetTickCount();
	static int levelSeen = -1;

	// A new level starts everything here again, once it's really here: the number changes while the old one is still loaded
	if (levelSeen != netGameData.coopCampaignLevel && AvP.MainLoopRunning && !Coop_LevelChangeIsDue())
	{
		char line[120];

		levelSeen = netGameData.coopCampaignLevel;
		started = 0;
		lastLine = 0;
		sawThemAt = 0;
		lastHealth = -1;
		wentOver = false;
		gaveGuns = false;
		Hudson_SmokeTestStartsAgain();

		if (plan.coopWalk)
		{
			Hudson_NetLoadWalk();
		}
		_snprintf(line, sizeof(line) - 1, "net: this level is %s: campaign %d, place %d", LevelName, netGameData.coopCampaign, netGameData.coopCampaignLevel);
		line[sizeof(line) - 1] = 0;
		LogString(line);

		// So a fight's log shows the switch reached this machine. Only the host sets it, and a joiner hears it in the game description.
		if (netGameData.disableFriendlyFire)
		{
			LogString("net: friendly fire is off on this machine");
		}
	}

	if (started == 0)
	{
		started = now;
	}

	// The same flag a mission sets when a chain reaches it
	if (plan.coopEndSeconds > 0 && plan.netMode == kHudsonNetHost && (now - started) / 1000 >= (DWORD)plan.coopEndSeconds)
	{
		Hudson_NetSay("the test says this level's end has been reached");
		AvP.LevelCompleted = 1;
		started = now;
	}

	int const health = me->SBDamageBlock.Health >> ONE_FIXED_SHIFT;

	if (health != lastHealth)
	{
		char line[100];

		_snprintf(line, sizeof(line) - 1, "net: my health went from %d to %d", lastHealth, health);
		line[sizeof(line) - 1] = 0;
		LogString(line);
		lastHealth = health;
	}

	if (now - lastLine >= 2000)
	{
		char line[170];
		int players = 0;

		for (int i = 0; i < NET_MAXPLAYERS; i++)
		{
			players += netGameData.playerData[i].playerId != 0 ? 1 : 0;
		}

		lastLine = now;
		_snprintf(line, sizeof(line) - 1, "net: %.0f s, I'm player %d, %d players in the list, the other player's body is %s, my health %d, my armour %d, alive %d", (now - started) / 1000.0, (int)AvPNetID, players, other ? "here" : "not here", health, me->SBDamageBlock.Armour >> ONE_FIXED_SHIFT, (int)status->IsAlive);
		line[sizeof(line) - 1] = 0;
		LogString(line);
	}

	// A picture every so often, for proof of what this machine saw. A kill run takes them at its own moments instead, because each one stalls a frame.
	static DWORD lastPicture = 0;

	// A siege has its own pictures, at moments the two machines share
	if (plan.killKind == kHudsonBotFightNone && !plan.siegeRoom[0] && now - lastPicture >= 8000)
	{
		lastPicture = now;
		Hudson_AskForAPicture();
		LogString("net: a picture is being taken");
	}

	// Nothing else a run does starts until the smoke test is over, and the clock for the end of a level waits for it too
	if (Hudson_SmokeTest(status, me, other, now))
	{
		started = now;
		return;
	}

	if (plan.coopWalk)
	{
		Hudson_NetWalk();
	}

	if (plan.botFight != kHudsonBotFightNone)
	{
		Hudson_NetBotFight(status, other);
	}

	if (plan.killKind != kHudsonBotFightNone)
	{
		Hudson_KillNamed(status, other);
	}

	if (plan.siegeRoom[0])
	{
		Hudson_Siege(status, me, other, now);
	}

	Hudson_LookAtWhatMatters(status, me, other);

	if (plan.hostile)
	{
		Hudson_NetHostile(me, other, now);
		return;
	}

	if (plan.netMode != kHudsonNetJoin || plan.coop)
	{
		return;
	}

	if (other && sawThemAt == 0)
	{
		sawThemAt = now;
		Hudson_NetSay("the joiner can see the host's body");
	}

	if (sawThemAt == 0)
	{
		return;
	}

	DWORD const seconds = (now - sawThemAt) / 1000;

	if (seconds >= kLeaveAt)
	{
		Hudson_NetSay("the joiner has done what it came for, and it's leaving");
		EndTheLevel("the joiner left the game", kHudsonExitFinished);
		return;
	}

	if (!other)
	{
		return;
	}

	DYNAMICSBLOCK *const body = me->DynPtr;

	// The host may have died and come back somewhere else
	static bool wentOverAgain = false;

	if (seconds >= kGoOverAgainAt && !wentOverAgain)
	{
		wentOverAgain = true;
		wentOver = false;
	}

	if (seconds >= kGoOverAfter && !wentOver)
	{
		// Puts him 2 metres from the host, on the first side with nothing in the way. The game moves a player the same way when he starts.
		static const int kSides[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		VECTORCH there = other->DynPtr->Position;

		for (int side = 0; side < 4 && !wentOver; side++)
		{
			VECTORCH direction = { kSides[side][0] * ONE_FIXED, 0, kSides[side][1] * ONE_FIXED };

			FindPolygonInLineOfSight(&direction, &there, kLookAtAllThatsActive, other->SBdptr);

			if (!LOS_ObjectHitPtr || LOS_Lambda > 2500)
			{
				body->Position.vx = there.vx + kSides[side][0] * 2000;
				body->Position.vy = there.vy;
				body->Position.vz = there.vz + kSides[side][1] * 2000;
				body->PrevPosition = body->Position;

				// The game finds a player's room by starting from the one he was in, and if it can't get there it puts him back. So he's told he's in the host's room now.
				if (other->containingModule)
				{
					me->containingModule = other->containingModule;
					playerPherModule = other->containingModule;
				}

				wentOver = true;
			}
		}

		Hudson_NetSay(wentOver ? "the joiner has gone over to the host" : "the joiner found nowhere clear beside the host");
	}

	if (wentOver)
	{
		// Turned to face the middle of them, each frame. A player's direction is kept in OrientMat, and the game works the angles out from that, so that's what has to change.
		VECTORCH middle;

		GetTargetingPointOfObject_Far(other, &middle);

		float const dx = (float)(middle.vx - body->Position.vx);
		float const dz = (float)(middle.vz - body->Position.vz);
		float const across = sqrtf(dx * dx + dz * dz);

		body->OrientEuler.EulerX = 0;
		body->OrientEuler.EulerY = ((int)(atan2f(dx, dz) * 4096.0f / 6.2831853f)) & 4095;
		body->OrientEuler.EulerZ = 0;
		CreateEulerMatrix(&body->OrientEuler, &body->OrientMat);
		TransposeMatrixCH(&body->OrientMat);
		body->AngVelocity.EulerY = 0;
		status->ViewPanX = ((int)(atan2f((float)(middle.vy - Global_VDB_Ptr->VDB_World.vy), across) * 4096.0f / 6.2831853f)) & 4095;

		// Once a second, what the gun is really pointing at
		static DWORD lastAimLine = 0;

		if (now - lastAimLine >= 1000)
		{
			VECTORCH along = { Global_VDB_Ptr->VDB_Mat.mat13, Global_VDB_Ptr->VDB_Mat.mat23, Global_VDB_Ptr->VDB_Mat.mat33 };
			VECTORCH from = Global_VDB_Ptr->VDB_World;
			char line[170];

			lastAimLine = now;
			FindPolygonInLineOfSight(&along, &from, 1, Player);
			_snprintf(line, sizeof(line) - 1, "net: %d s after seeing them, the gun points at %s, %d mm away; they are %d mm away; slot %d, rounds %d", (int)seconds, !LOS_ObjectHitPtr ? "nothing" : (LOS_ObjectHitPtr->ObStrategyBlock == other ? "the other player" : "something else"), LOS_Lambda, (int)across, (int)status->SelectedWeaponSlot, status->WeaponSlot[status->SelectedWeaponSlot].PrimaryRoundsRemaining >> ONE_FIXED_SHIFT);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
	}

	if (seconds >= kGiveGunsAt && !gaveGuns)
	{
		gaveGuns = true;
		GiveAllWeaponsCheat();
		Hudson_NetSay("the joiner has all the guns");
	}

	extern int SlotForThisWeapon(enum WEAPON_ID weaponID);

	// A slot's key is one more than its number
	int const wantedSlot = seconds < kGoOverAgainAt ? kShotgunSlot : SlotForThisWeapon(WEAPON_FRISBEE_LAUNCHER);

	if (gaveGuns && wantedSlot >= 0 && (int)status->SelectedWeaponSlot != wantedSlot)
	{
		status->Mvt_InputRequests.Flags.Rqst_WeaponNo = wantedSlot + 1;
	}

	if ((seconds >= kShotgunFrom && seconds < kShotgunUntil) || (seconds >= kSkeeterFrom && seconds < kSkeeterUntil))
	{
		status->Mvt_InputRequests.Flags.Rqst_FirePrimaryWeapon = 1;

		// Which gun this is, and how much it has, so the log shows what really fired
		static DWORD lastGunLine = 0;

		if (now - lastGunLine >= 1000)
		{
			PLAYER_WEAPON_DATA const *const weapon = &status->WeaponSlot[status->SelectedWeaponSlot];
			char line[120];

			lastGunLine = now;
			_snprintf(line, sizeof(line) - 1, "net: the joiner is firing gun %d, from slot %d, with %d rounds", (int)weapon->WeaponIDNumber, (int)status->SelectedWeaponSlot, weapon->PrimaryRoundsRemaining >> 16);
			line[sizeof(line) - 1] = 0;
			LogString(line);
		}
	}
}

void Hudson_NetStopBecause(const char *why)
{
	Hudson_NetSay(why);
	LogErrorString(std::string("net: it stopped here: ") + why);
	EndAtOnce(why, kHudsonExitNetStopped);
}

void Hudson_StopIfTheGameWantsToAsk(const char *message)
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	LogErrorString(std::string("hudson: the game wanted to ask someone this, and nobody's there: ") + (message ? message : ""));
	EndAtOnce("the game wanted to ask someone something", kHudsonExitMessageBox);
}

void Hudson_StopIfAnAssertFailed(const char *file, int line, const char *condition)
{
	if (!HudsonRunIsOn)
	{
		return;
	}

	char words[512] = { 0 };

	_snprintf(words, sizeof(words) - 1, "hudson: an assert failed, line %d of %s: %s", line, file ? file : "", condition ? condition : "");
	LogErrorString(words);
	EndAtOnce("an assert failed", 0xffff);
}

int Hudson_LastThingBeforeExit()
{
	if (!HudsonRunIsOn)
	{
		return 0;
	}

	// Everything else that ends a run says how it ended before it gets here. So if nothing has, the game's own main loop stopped by itself: someone closed the window, which a run sharing the machine
	// lets them do, or the graphics card gave up. Without this it would look like a run that finished.
	if (!resultIsWritten)
	{
		exitCode = kHudsonExitStoppedEarly;
		WriteResult("the game stopped before the run had finished: its window was closed, or the graphics card failed", exitCode);
	}

	// The watchdog may have ended the run, and still be finishing the film. The game mustn't end from under it.
	WaitForTheResultToBeOnDisk();

	// a second line, only here if everything shut down: the result was written before that started
	char const *folder = GetSaveFolderPath();
	std::string const path = std::string(folder ? folder : "") + kResultFileName;
	FILE *const file = fopen(path.c_str(), "a");

	if (file)
	{
		fprintf(file, "shutdown=clean\n");
		fclose(file);
	}

	return exitCode;
}
