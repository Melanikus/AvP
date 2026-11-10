// Modified 2026-11-06 by Melanikus for the AvP port:
//   the players of a pair run face each other at each level's start, and a fight's idle player looks at the fight
//   a run's window goes on the second screen by the game's own doing, with a part of it for each game
//   killkind=, killat= and killer= have one machine really kill a level's own named NPC
//   a run is held to 60 frames a second unless it asks for another rate, or for none, by name
//   levelfilms= and filmskip= play a level's own films in a run, and stop one partway
//   listplace=, meets=, dieat=, difficulty=, and species=alien
//   coop=yes plays a net= run as a campaign, and coopend= reaches each level's end after so many seconds
//   gunmarine=yes stands a marine holding that gun in front of him on a gun= run
//   gunalien=yes makes an alien in front of him on a gun= run
//   class= starts a gun= run as a skirmish as that marine class
//   gun= tries a marine's gun
//   species=predator lets a gun= run play as a Predator
//   botfight= has the host make an NPC beside the joiner in a network test
//   friendlyfire=off has the host of a network test turn friendly fire off
// Modified 2026-11-07 by Melanikus for the AvP port:
//   custommap= plays a custom map made for single player, by the name the Custom maps page lists it under
// Modified 2026-11-09 by Melanikus for the AvP port:
//   access= and accesswait= press the Online access page's rows, and answer= and answerafter= the host's answer to a player asking to join
// What a Hudson run has been asked to do, read from the command line. Written 2026-11-04 by Melanikus for the AvP port.
//
// Hudson is a marine the camera follows, so that the game can be measured with nobody at the keyboard: see hudson.cpp. A run is asked for like this:
//
//   AvPx.exe -dev -hudson level=derelict frames=300 seconds=600 idle=120
//
// or, with a marine for the camera to follow, standing in one room and looking two ways. Then frames= is the most it may take, and it ends sooner if it's done:
//
//   AvPx.exe -dev -hudson level=derelict frames=5000 rooms=1 views=2
//
// A run can also say how the world is drawn, and ask for a picture of each view. Saving a picture stops the game for a moment, so a run with pictures is no use for timings:
//
//   AvPx.exe -dev -hudson level=derelict frames=5000 rooms=1 lighting=normals shadows=on record=pictures
//
// A run that says machine=shared also makes a film of itself, an mp4 in its folder, unless it says film=no or record=numbers. record=pictures keeps a film as well. Taking a film's pictures slows the frames, so a run that measures time has no film,
// and asking for one by name, with record=video, is refused:
//
//   AvPx.exe -dev -hudson level=derelict frames=400000 seconds=1200 rooms=40 views=2 holdseconds=2 machine=shared
//
// Or, to judge how the world is drawn: he stands in front of the biggest wall showing one picture, and without him moving, the same view is drawn each way there is, with a picture of each. wallrank=2 is the next
// biggest wall, and wallroom=hole only looks in the room of that name, because the biggest walls can all be in one dim room:
//
// Or, to see the level's strategies work: strategies=list writes them all to the log. strategy= stands him in front of the one
// of that name, looks at it, and asks it to go on the way a switch would. A space in a name is written as _ here:
//
//   AvPx.exe -dev -hudson level=derelict frames=500000 rooms=1 strategies=list strategy=DR-VAENT01 strategyseconds=12
//
// To see a chain work, strategylook= says what to stand in front of, when that isn't the thing being asked: here he watches
// a door while the switch that opens it is asked. The log has every strategy that changes, so the whole chain can be read.
//
//   AvPx.exe -dev -hudson level=derelict frames=500000 rooms=1 strategy=swch-ALCK1 strategylook=DR-VAENT01 strategyseconds=30
//
//   AvPx.exe -dev -hudson level=derelict frames=20000 rooms=1 views=3 wall=hive01 wallroom=hole compare=relief record=pictures lighting=relief shadows=off
//
// Or, to watch a fight: the world runs with nothing held back, an alien is made in front of him, and he deals with it his own way. aliens=5 makes five, one after another, because one is over in about a second.
// The run ends a few seconds after the last one dies, or when frames= runs out.
//
//   AvPx.exe -dev -hudson level=derelict frames=20000 seconds=600 rooms=1 views=1 combat=alien aliens=5
//
// The command line comes from outside the game, so nothing on it is believed. A level's name is only ever compared with the game's own list, and it's never used as a path. Every number has
// limits. Anything that isn't exactly right stops the run before it starts, and says which thing was wrong. Nothing here is a guess at what someone meant.
//
// This file knows nothing about the game, so tests/hudson_plan_test.cpp can run it on its own.

#ifndef HUDSON_PLAN_H
#define HUDSON_PLAN_H

#include "custom_map_name.h"

enum
{
	kHudsonLongestLevelName = 31,
	kHudsonMostFrames = 1000000,
	kHudsonDefaultFramesPerSecond = 60, // what most players see, so a run tests the game they play. A bot can behave differently much faster than this
	kHudsonFewestFramesPerSecond = 10,
	kHudsonMostFramesPerSecond = 1000,
	kHudsonFewestSeconds = 10,
	kHudsonMostSeconds = 14400,        // four hours
	kHudsonDefaultSeconds = 1800,
	kHudsonMostIdleSeconds = 3600,
	kHudsonDefaultIdleSeconds = 120,
	kHudsonMostRooms = 1000,           // more than any level has
	kHudsonMostViews = 8,
	kHudsonMostAliens = 50,
	kHudsonDefaultViews = 2,
	kHudsonDefaultWidth = 1280,        // what a measured run and a picture always use, so that they compare with the last one
	kHudsonDefaultHeight = 720,
	kHudsonWatchedWidth = 1920,        // what a run sharing the machine uses if nothing's asked, because someone may be watching
	kHudsonWatchedHeight = 1080,
	kHudsonRoomForTheWindowsFrame = 80,   // a window is bigger than its picture, by a title bar and edges
	kHudsonMostHoldSeconds = 3600,
	kHudsonDefaultHoldFrames = 300,    // as long as one of the game's reports
	kHudsonFewestHoldFrames = 30,      // long enough that a picture comes after anything new has settled in
	kHudsonMostHoldFrames = 100000,
	kHudsonFewestNavModules = 1,
	kHudsonMostNavModules = 1000,      // far more modules than any level has, and he stops when there are no more
	kHudsonLongestRouteName = 32,
	kHudsonViewDistance = 3000,        // how far away the point he faces is: three metres
	kHudsonLongestWallName = 31,
	kHudsonLongestRelayAddress = 15,   // remoterelay=: an IPv4 address is at most 15 letters, 255.255.255.255 (modified 2026-11-10, #175: never a name)
	kHudsonLongestStrategyName = 59,
	kHudsonMostTogether = 8,       // together=: the most aliens a watched fight has at once. Each is followed once a second
	kHudsonDefaultStrategySeconds = 12,
	kHudsonMostStrategySeconds = 300,
	kHudsonMostWallRank = 1000,
	kHudsonWallDistance = 2000,        // how far from a wall he stands: two metres
	kHudsonSmallestWallSide = 500      // a polygon narrower or shorter than this, half a metre, is no wall to judge anything by
};

// How the world is drawn. As it is means the game's own start, whatever that is, so a run that compares anything should say.
enum HudsonLightingWord
{
	kHudsonLightingAsItIs,
	kHudsonLightingGouraud,
	kHudsonLightingPerPixel,
	kHudsonLightingNormals,
	kHudsonLightingRelief
};

enum HudsonShadowsWord
{
	kHudsonShadowsAsTheyAre,
	kHudsonShadowsOff,
	kHudsonShadowsOn
};

// botfight=: which kind of NPC the host makes beside the joiner
enum
{
	kHudsonBotWeaponNone = 0,
	kHudsonBotWeaponBlades = 1,
	kHudsonBotWeaponPlasma = 2,
	kHudsonRateTestNone = 0,
	kHudsonRateTestFall = 1,
	kHudsonRateTestFlame = 2,
	kHudsonBotFightNone = 0,
	kHudsonBotFightAlien,
	kHudsonBotFightMarine,
	kHudsonBotFightPredator,
	kHudsonBotFightXenoborg,
	kHudsonBotFightQueen,
	kHudsonBotFightFacehugger,
	kHudsonBotFightSentry,
	kHudsonBotFightPlasma,    // a Predator with a plasma caster. "predator" alone has blades.
	kHudsonBotFightFlame      // a marine with a flamethrower. "marine" alone has a pulse rifle.
};

// net=: this run is one of two games on this machine, one hosting and one joining. tools/hudson/run-net-pair.ps1 runs both.
enum
{
	kHudsonNetNone = 0,
	kHudsonNetHost,
	kHudsonNetJoin
};

// access=: an access run stays at the menus and presses the Online access page's rows as a player does: Multiplayer's Online access row,
// then Request access (request), the same and Check again after accesswait= (check), or Make a new identity and Yes (newid). Each press
// is one fixed line in the log, and the page is pictured once the server's answer is on it. For the local rehearsal of beta.2's flow
// (Third's, 2026-11-09). Added 2026-11-09 by Melanikus for the AvP port.
enum
{
	kHudsonAccessNone = 0,
	kHudsonAccessRequest,
	kHudsonAccessCheck,
	kHudsonAccessNewId
};

// answer=: a net=host run doesn't let every joiner in by itself, as runs do. When a player asks, it opens its menu, as a host presses
// Esc, and after answerafter= seconds presses Let in (letin) or Say no (no), with a picture of the asking line, the check code and the
// rows just before. Added 2026-11-09 by Melanikus for the AvP port.
enum
{
	kHudsonAnswerNone = 0,
	kHudsonAnswerLetIn,
	kHudsonAnswerSayNo
};

int const kHudsonDefaultAccessWaitSeconds = 70;   // past the list's one call a minute from one address
int const kHudsonLeastAccessWaitSeconds = 61;
int const kHudsonMostAccessWaitSeconds = 3600;
int const kHudsonDefaultAnswerAfterSeconds = 3;
int const kHudsonMostAnswerAfterSeconds = 15;     // well inside the 20 s a Hudson joiner waits for the host's word (menus.cpp)

// What a hostile run does, beyond sending each bad message in turn.
// drain: this client uses up the other machine's blocks with messages, inside every limit.
// watch: the host of a drain, hold or flood run. It logs what's left, and in a drain its player is killed at the peak.
// hold: this client stops answering, so the host has to hold a queue for it.
// flood: this client sends as much as it's allowed, for the host to pass on to the one that stopped.
enum HudsonHostileKind
{
	kHudsonHostileRows = 0,
	kHudsonHostileDrain,
	kHudsonHostileWatch,
	kHudsonHostileHold,
	kHudsonHostileFlood
};

struct HudsonPlan
{
	char level[kHudsonLongestLevelName + 1];
	int frames;        // how many frames to run for. With a marine, the most it may take
	int seconds;       // the most it may take in real time, however many frames it's reached
	int idleSeconds;   // how long nobody must have touched the machine before it starts
	int rooms;         // how many rooms the marine stands in. 0 means there's no marine, and the level just runs
	int views;         // how many ways he looks in each room
	int navModules;    // how many new modules he walks to, instead of holding still. 0 means he doesn't walk. See hudson_nav.cpp
	char route[kHudsonLongestRouteName + 1];   // the name of a route for him to follow. Nothing means he goes wherever's new
	bool paceRun;      // he goes at the marine's combat speed, and not his walk
	bool mapGrid;      // map=grid: he finds his way with the 3D grid (hudson_grid.h), and not by feeling about with rays
	bool frameHoldOff; // a walk isn't held at 60 frames a second
	int holdFrames;    // how many frames he holds still for each view. One of the game's reports covers 300 frames and isn't lined up with a hold, so 600 is sure to have a whole one inside it
	int lighting;      // one of HudsonLightingWord
	int shadows;       // one of HudsonShadowsWord
	bool pictures;     // save a picture in the middle of each hold
	bool listStrategies;   // write every strategy in the level to the log
	char strategy[kHudsonLongestStrategyName + 1];   // the name of the strategy to stand in front of and ask to go on. Nothing means none
	int strategySeconds;   // how long to watch it for afterwards
	char strategyLook[kHudsonLongestStrategyName + 1];   // the name of the strategy to stand in front of instead. Nothing means the one being asked
	char wall[kHudsonLongestWallName + 1];   // the name of the picture on the wall to stand in front of. Nothing means where the level starts him
	int wallRank;      // which wall showing it: 1 is the biggest
	char wallRoom[kHudsonLongestWallName + 1];   // only look in the room of this name. Nothing means every room
	int holdSeconds;      // holdseconds=: how long he holds still for each view in real seconds, for someone watching. 0 means hold= counts frames instead
	char tourFrom[kHudsonLongestWallName + 1];   // tourfrom=: the room a tour starts from, by name. Nothing means the room the player starts in
	bool steamFolderAllowed;   // steam=yes: this run may go in a folder that's otherwise refused. See HudsonPlan_FolderIsAllowed
	bool brightWorld;     // bright=yes: draw the whole world fully lit, the game's own way, so that a crack shows up against it. It needs vision=
	bool lampOn;          // the marine's shoulder lamp is on for the whole run, switched the way the key does. A run has it off unless it says lamp=on, and a run the lamp would spoil can't: see HudsonPlan_Read
	bool lampBlink;       // lamp=blink: it goes on and off every second and a half, so that a film shows the difference in the same view
	bool lampReal;        // lampkind=real: the lamp is a real light with real shadows, rather than the painted version. See shoulder_lamp.h
	bool lampWide;        // lampat=wide: the real lamp sits further from his eyes, where more of its shadows show
	int lampSizeHundredths; // lampsize=0.05 to 0.35: how big the lamp's spot is, as LAMPSPOTSIZE says it, in hundredths. 0 means as built. The user's alien films have 0.35, the most it goes to
	bool playAsAlien;     // species=alien: a meets=yes run plays as an Alien, for comparing with a campaign's level
	bool levelFilms;      // levelfilms=yes: a level's own films play, which a run otherwise skips, to prove a network game stays up under one
	int filmSkipSeconds;  // filmskip=: a film stops this many seconds in, as if a key had been pressed. Needs levelfilms=yes
	int dieAtSeconds;     // dieat=: he dies this many seconds in, once in the run, and three seconds later "presses a key", for trying what dying does
	int difficulty;       // difficulty=: the game's number for it. Medium unless asked.
	bool windowOnMainScreen; // screen=main: the window stays on the main screen. Otherwise, or with screen=second, it goes on the first screen that isn't the main one, if there is one
	int windowSlot;       // windowslot=: which part of that screen this game's window has, when several run together, counted from 0
	int windowsTogether;  // windowsof=: how many games share that screen, from 1 to 4. Unless a run asks, 1
	int windowBig;        // windowbig=: with two windows, the number of the one that has the big part of the screen, the left three quarters. Unless a run asks, -1, and they share it half and half
	bool meets;           // meets=yes: the log says what the player meets when the level starts, in lines made for comparing runs
	char siegeRoom[kHudsonLongestWallName + 1]; // siege=: after the smoke test both players go to the room of this name, armed and unable to die, and the host keeps enemies coming. Nothing means none
	char siegeFar[kHudsonLongestWallName + 1];  // siegefar=: the room a siege's xenoborgs are made in, further off, so that they have time to wake. Nothing means where the others come from
	char customMap[kCustomMapNameMost + 1]; // custommap=: a custom map made for single player, played instead of level=, which still says who plays. Nothing means none
	char siegeFrom[kHudsonLongestWallName + 1]; // siegefrom=: the room a siege's enemies are made in, with _ for a space in its name. Nothing means the far side of siegeRoom
	char remoteRelay[kHudsonLongestRelayAddress + 1]; // remoterelay=: the one relay beyond this machine a run with net= may use, by its address, as its server's servers.cfg line gives it (server #14's run through a relay on another machine, 2026-11-09; an address since 2026-11-10, #175). Nothing means none: this machine only
	bool lookAllTheTime;  // look=yes or look=no: after the smoke test, a player the fight isn't aiming goes on looking at the nearest of what's being fought, or else at the other player. Unless a run asks, a fight or a kill on the campaign does
	bool listPlaceAsked;  // listplace= was given
	int listPlace;        // listplace=: a skirmish on this place of the co-op level list, started the way the level row does it
	bool playAsPredator;  // species=predator: a gun= run plays as a Predator, for trying the staff. Nothing else can ask for it.
	bool play;            // play=yes: he plays the level on foot, as a player would. See hudson_play.h
	bool gunAlien;        // gunalien=yes: a gun= run makes an alien in front of him before the throws and shots that it times, for them to hit
	bool noFriendlyFire;  // friendlyfire=off: the host of a net= run with coop=yes turns friendly fire off, as its menu row would
	int framesPerSecond;  // fps=: a run goes no faster than this many frames a second. Unless it asks, that's kHudsonDefaultFramesPerSecond. 0, from fps=uncapped, is as fast as it can.
	int killKind;         // killkind=: this run kills the level's own NPC of this kind, the one placed nearest killat=, from kHudsonBotFight.... 0 for none
	int killX, killY, killZ; // killat=x,y,z: where the level file places it
	bool killAtSaid;
	bool killerIsHost;    // killer=host or killer=join: which machine kills it
	bool killerIsNobody;  // killer=nobody: the same, but nobody fires, to watch it fight
	bool killerSaid;
	bool noNetPictures;   // pictures=no: a pair run takes none of its own pictures. Each stalls a frame for most of a second, so this is how to see what's left without them
	bool botSingle;       // botsingle=yes: the same fight in plain single player, as the thing to compare the network's with
	bool vsyncOff;        // vsync=off: this run doesn't wait for the screen, even sharing the machine. The screen's refresh would set the rate when a run is measuring what the rate changes.
	int rateTest;         // ratetest=: fall or flame, from kHudsonRateTest. What a fall or a flamethrower does at this frame rate, to choose the game's frame cap.
	int deathWay;         // deathway=: every NPC dies this way, the number on its list in deaths.cpp, so that one way can be looked at on both machines. -1 if not asked.
	bool ghostLog;        // ghostlog=yes: every machine logs each NPC it runs or has a ghost of. tools/hudson/ghost-compare.js lines up the two logs.
	bool botOwn;          // botown=yes: a bot fight brings the level's own NPC of that kind to the joiner, and makes none.
	int botWeapon;        // botweapon=: blades or plasma, from kHudsonBotWeapon. Both players in a fight with botstay=yes fight it with that Predator's weapon. 0 is whatever they hold, and only the joiner fights.
	bool botStay;         // botstay=yes: the host stays beside the joiner in a bot fight, instead of going to the far end of the level
	int botFight;         // botfight=: which kind the host makes beside the joiner, from kHudsonBotFight..., or 0 for none
	int classSlider;      // class=: a gun= run starts as a skirmish, as this marine class, by its place on the menu's slider. 0 if not asked.
	bool gunMarine;       // gunmarine=yes: a gun= run stands a marine holding that gun in front of him, for pictures of what other players see
	int gunSlot;          // gun=: the slot of a marine's gun to try, from 1 to 14, or 15 to try each in turn. 0 means no gun is tried. See hudson.cpp
	int playGunSlot;      // playgun=: with play=yes, the slot of the gun he's given and holds, from 1 to 14. 0 leaves him what the level gives him
	int playStepSeconds;  // stepseconds=: with play=yes, how long one step may take before he's moved to it, a logged cheat: 10 to 600. 0 is the usual 180
	bool flareEachHold;   // flare=yes: he throws a flare as each hold starts, for seeing what a flare's light does. See hudson.cpp
	bool recordVideo;     // a film of what's drawn, in the run's folder. See hudson_video.h. machine=shared has one unless it says film=no or record=numbers. Nothing else can, because reading frames back slows them
	bool breakVideo;      // break=video: the encoder fails for real part way, to prove that a run survives it. It needs a run with a film
	bool breakPrisms;     // break=prisms: don't send the shadow prisms to the card, so that vision=check can be seen to notice. It needs vision=
	bool vision;          // vision=check: read the frame's pictures once a view, and say what's missing. See hudson_vision.h
	int width;            // how big the picture is. size= says, from a short list
	int height;
	bool sizeAsked;       // size= said so. A size nobody asked for gives way if it doesn't fit the screen, and one somebody asked for is refused
	bool showWindow;      // show=yes: the window opens as a normal game's does, in front, for someone watching or recording. It needs machine=shared
	bool machineShared;   // machine=shared: someone may use the machine during the run, so touching it doesn't end the run, and its timings mean nothing
	bool hostile;         // hostile=yes: a net= run sends each bad message in turn, to see it refused
	int hostileKind;      // hostile= can also be drain, watch, hold or flood: see HudsonHostileKind
	bool typed;           // typed=yes: a net= run's joiner finds the game by searching for a typed address, 127.0.0.1
	bool coopWalk;        // coopwalk=yes: each level's chain file is walked on both machines
	bool coop;            // coop=yes: a net= run plays the campaign its level is on
	int coopEndSeconds;   // coopend=: on the host, each level's end is reached this long after it starts
	int netMode;          // net=host or net=join: one of two games on this machine, talking over its own address, 127.0.0.1, and open to nothing else. It needs machine=shared and a multiplayer level, and it can't go with anything that needs a game to itself
	bool compareRelief;   // draw each view every way in the list below, with a picture of each
	bool combatAlien;     // run the world, and make an alien in front of him to fight
	int combatAliens;     // aliens=: how many to make, one after another. 1 unless it says
	bool watchAlien;      // watch=alien: in a fight, the camera follows the alien from the side, and the log says where it goes once a second. See "WATCHING AN ALIEN" in hudson.cpp
	int watchEveryMs;     // every=0.1 to 10 seconds: with together=, the others come one at a time this long apart, as a level's generator sends them. 0 is all at once
	int watchTogether;    // together=2 to 8: a watched fight has this many aliens at once, made a few metres apart round the first, each with its own line once a second. 0 or 1 is one
	bool watchFromEyes;   // watchfrom=eyes: in a watched fight, the camera stays at his own eyes, as a player would see the alien come. watchfrom=side is the camera beside the alien, as without it
	bool holdFire;        // holdfire=yes: in a watched fight, he never fires, so that the alien gets to him
	bool alienAtSaid;     // alienat=x,y,z: the alien is made 300 mm above this place, instead of somewhere in sight of him
	int alienAtX, alienAtY, alienAtZ;
	int alienAtPlaces;    // alienat= can name up to kHudsonMostTogether places with / between them: the first is the watched alien's, the rest together='s, in order
	int alienAtMore[kHudsonMostTogether - 1][3];
	bool standAtSaid;     // standat=x,y,z: he stands here, instead of where the level starts the player
	int standAtX, standAtY, standAtZ;
	char follow[kHudsonLongestStrategyName + 1];   // follow=: the name of one thing in the level, often an NPC, whose place and state the log says once a second. Nothing means none. For #126's floating facehugger
	bool walkSaid;        // walkfrom=x,y,z and walkto=x,y,z: he's put at walkfrom= and walks to walkto= as a player does, with the forward key. With dieat=, only once the level has started again. See "A WALK" in hudson.cpp
	int walkFromX, walkFromY, walkFromZ;
	int walkToX, walkToY, walkToZ;
	bool lookAtSaid;      // lookat=x,y,z: once he's at walkto=, he looks at this place, and with record=pictures a picture is taken 2 and 5 seconds later
	int lookAtX, lookAtY, lookAtZ;
	int accessPress;      // access=: from kHudsonAccess...: an access run stays at the menus and presses the Online access page's rows. 0 for none
	int accessWaitSeconds; // accesswait=: with access=check, the seconds from the request to Check again, 61 to 3600. Unless asked, 70
	int answerPress;      // answer=: from kHudsonAnswer...: a net=host run answers a player asking to join with that row. 0 for none: every joiner is let in, as before
	int answerAfterSeconds; // answerafter=: the seconds the host's answer rows are up before the press, 1 to 15. Unless asked, 3
	bool findGames;       // findgames=yes: a net=join run through the relay joins as a player does: Find Games, then the list's game's row, with pictures of the joining page asking (its check code) and let in. Otherwise the list's game is joined from the code, as before
};

// One way of drawing a view, in a run with compare=relief. Each says everything about itself, so that none depends on the one before.
struct HudsonCompareCase
{
	const char *name;     // this goes in the picture's file name
	int lighting;         // one of HudsonLightingWord
	bool heightFlipped;   // what the RELIEFFLIP command changes
	bool tiltFlipped;     // what the TILTFLIP command changes
	bool noDepth;         // what RELIEFDEPTH 0 does. Otherwise it's the depth the game starts with
};

int HudsonPlan_CompareCaseCount();
const HudsonCompareCase &HudsonPlan_CompareCase(int number);

// Where to stand to look at a polygon, and how good a wall it is.
struct HudsonWallSpot
{
	double wide, tall;        // how wide the polygon is along the wall, and how tall
	double size;              // how big it is: one of those times the other
	int x, y, z;              // where to stand: straight out from its middle, at its middle's height
	int towardsX, towardsZ;   // from there, the way to the wall, kHudsonViewDistance long
};

// Is this polygon a wall to stand in front of, and if so, where? corners holds x, y and z for each corner, in the world's numbers. The face numbers are the way it faces, of any length: the side it can be seen from.
// It's not one if it leans more than 30 degrees, because then it's more a floor or a ceiling, or if it's too small to judge anything by.
bool HudsonPlan_WallSpot(const int (*corners)[3], int count, double faceX, double faceY, double faceZ, HudsonWallSpot *spot);

// Is this level's strategy called word? In capitals or not, and a _ in the word also stands for a space, because a word on the
// command line can't hold one.
bool HudsonPlan_NameMatches(const char *levelName, const char *word);

// Is this the picture called word? Only the last part of the path counts, without its ending, in capitals or not, so hive01 is graphics/Envrnmts/Derelict/Hive01.RIM.
bool HudsonPlan_PictureIsNamed(const char *path, const char *word);

// In front of a wall, where each view looks from, and which way. The first looks straight at it. The second is turned, to see it on the slant, where a depth map should shift more. The third looks straight at it again
// from a step along the wall, because a depth map that's right holds still against the wall when the camera moves, and one that's wrong slides. The rest are the same again the other way round, and then further.
int HudsonPlan_WallViewDegrees(int view);
int HudsonPlan_WallViewStep(int view);
void HudsonPlan_WallViewDirection(int view, int towardsX, int towardsZ, int *x, int *z);

// How far that view's step is from where he first stood, along the world's two level directions. It's square to the way to the wall, so he stays the same distance from it.
void HudsonPlan_WallViewPlace(int view, int towardsX, int towardsZ, int *x, int *z);

// How much of the wall one whole picture covers, along one direction: how far the wall goes, over how much of the picture it shows. 0 if it shows none.
double HudsonPlan_OnePictureCovers(double wallLength, double pictureCoordinatesRange, double pictureSize);

// Is a run being asked for? It needs both words, each on its own. Without them, none of this exists.
bool HudsonPlan_IsAskedFor(const char *commandLine);

// Reads what comes after -hudson, up to the next word starting with a dash. Gives back 0 if it's all good, or one of the reasons below.
const char *HudsonPlan_Read(const char *commandLine, HudsonPlan *plan);

// Why a plan was refused.
extern const char *const kHudsonRefusedNotNameAndValue;
extern const char *const kHudsonRefusedBotFightUnknown;
extern const char *const kHudsonRefusedFramesPerSecond;
extern const char *const kHudsonRefusedKillWords;
extern const char *const kHudsonRefusedPicturesWord;
extern const char *const kHudsonRefusedRateTest;
extern const char *const kHudsonRefusedAccessWord;
extern const char *const kHudsonRefusedAccessWithNet;
extern const char *const kHudsonRefusedAccessWaitNotANumber;
extern const char *const kHudsonRefusedAccessWaitOutOfRange;
extern const char *const kHudsonRefusedAccessWaitWithoutCheck;
extern const char *const kHudsonRefusedAnswerWord;
extern const char *const kHudsonRefusedAnswerWithoutHost;
extern const char *const kHudsonRefusedAnswerAfterNotANumber;
extern const char *const kHudsonRefusedAnswerAfterOutOfRange;
extern const char *const kHudsonRefusedAnswerAfterWithoutAnswer;
extern const char *const kHudsonRefusedFindGamesWord;
extern const char *const kHudsonRefusedFindGamesWithoutJoin;
extern const char *const kHudsonRefusedBotWeapon;
extern const char *const kHudsonRefusedDeathWay;
extern const char *const kHudsonRefusedVsync;

// When is the frame after this one due, given when this one was due and what the clock says now? It counts from when this frame was due, not from when it really went. A short sleep and the screen's own refresh can both make a frame late, and if each frame counted from when it really went, every late frame would be lost for good: a run asked for 60 on a 240 Hz screen comes out near 48. Counting this way, the next frame makes up for a late one, and the average holds. Very late is different, like a level loading: making that up would be a burst of frames as fast as they can go, so the count starts again from now.
inline long long HudsonNextFrameDue(long long thisFrameWasDue, long long now, long long ticksEachFrame)
{
	long long const next = thisFrameWasDue + ticksEachFrame;

	return now - next > ticksEachFrame ? now : next;
}
extern const char *const kHudsonRefusedBotFightWithoutCoop;
extern const char *const kHudsonRefusedFriendlyFireNotOff;
extern const char *const kHudsonRefusedFriendlyFireWithoutCoop;
extern const char *const kHudsonRefusedUnknownName;
extern const char *const kHudsonRefusedAskedTwice;
extern const char *const kHudsonRefusedNoLevel;
extern const char *const kHudsonRefusedLevelTooLong;
extern const char *const kHudsonRefusedLevelCharacter;
extern const char *const kHudsonRefusedNoFrames;
extern const char *const kHudsonRefusedFramesNotANumber;
extern const char *const kHudsonRefusedFramesOutOfRange;
extern const char *const kHudsonRefusedSecondsNotANumber;
extern const char *const kHudsonRefusedSecondsOutOfRange;
extern const char *const kHudsonRefusedIdleNotANumber;
extern const char *const kHudsonRefusedIdleOutOfRange;
extern const char *const kHudsonRefusedRoomsNotANumber;
extern const char *const kHudsonRefusedRoomsOutOfRange;
extern const char *const kHudsonRefusedViewsNotANumber;
extern const char *const kHudsonRefusedViewsOutOfRange;
extern const char *const kHudsonRefusedViewsWithoutRooms;
extern const char *const kHudsonRefusedHoldNotANumber;
extern const char *const kHudsonRefusedHoldOutOfRange;
extern const char *const kHudsonRefusedNavNotANumber;
extern const char *const kHudsonRefusedNavOutOfRange;
extern const char *const kHudsonRefusedNavWithoutOneRoom;
extern const char *const kHudsonRefusedNavWithWall;
extern const char *const kHudsonRefusedRouteEmpty;
extern const char *const kHudsonRefusedRouteTooLong;
extern const char *const kHudsonRefusedRouteCharacter;
extern const char *const kHudsonRefusedRouteWithoutOneRoom;
extern const char *const kHudsonRefusedRouteWithWall;
extern const char *const kHudsonRefusedPaceUnknown;
extern const char *const kHudsonRefusedPaceWithoutWalking;
extern const char *const kHudsonRefusedMapUnknown;
extern const char *const kHudsonRefusedMapWithoutWalking;
extern const char *const kHudsonRefusedFrameHoldNotOff;
extern const char *const kHudsonRefusedFrameHoldWithoutWalking;
extern const char *const kHudsonRefusedHoldWithoutRooms;
extern const char *const kHudsonRefusedLightingUnknown;
extern const char *const kHudsonRefusedShadowsUnknown;
extern const char *const kHudsonRefusedRecordUnknown;
extern const char *const kHudsonRefusedPicturesWithoutRooms;
extern const char *const kHudsonRefusedHoldSecondsNotANumber;
extern const char *const kHudsonRefusedHoldSecondsOutOfRange;
extern const char *const kHudsonRefusedHoldTwoWays;
extern const char *const kHudsonRefusedTourFromEmpty;
extern const char *const kHudsonRefusedTourFromTooLong;
extern const char *const kHudsonRefusedTourFromCharacter;
extern const char *const kHudsonRefusedTourFromWithoutATour;
extern const char *const kHudsonRefusedSteamNotYes;
extern const char *const kHudsonRefusedOneJobAtATime;
extern const char *const kHudsonRefusedBrightNotYes;
extern const char *const kHudsonRefusedBrightWithoutVision;
extern const char *const kHudsonRefusedGunNotASlot;
extern const char *const kHudsonRefusedSpeciesNotPredator;
extern const char *const kHudsonRefusedSpeciesWithoutGun;
extern const char *const kHudsonRefusedGunAlienNotYes;
extern const char *const kHudsonRefusedGunMarineNotYes;
extern const char *const kHudsonRefusedGunMarineWithoutGun;
extern const char *const kHudsonRefusedGunAlienWithoutGun;
extern const char *const kHudsonRefusedPlayNotYes;
extern const char *const kHudsonRefusedPlayWithSomethingElse;
extern const char *const kHudsonRefusedPlayNetWithoutCoop;
extern const char *const kHudsonRefusedClassNotOne;
extern const char *const kHudsonRefusedClassWithoutGun;
extern const char *const kHudsonRefusedClassAsPredator;
extern const char *const kHudsonRefusedCoopWithoutNet;
extern const char *const kHudsonRefusedMeetsNotYes;
extern const char *const kHudsonRefusedDieAtNotANumber;
extern const char *const kHudsonRefusedLevelFilmsNotYes;
extern const char *const kHudsonRefusedScreenWord;
extern const char *const kHudsonRefusedWindowSlot;
extern const char *const kHudsonRefusedWindowsOf;
extern const char *const kHudsonRefusedWindowPartsOnMain;
extern const char *const kHudsonRefusedWindowBig;
extern const char *const kHudsonRefusedLookNotYesOrNo;
extern const char *const kHudsonRefusedSiegeEmpty;
extern const char *const kHudsonRefusedSiegeTooLong;
extern const char *const kHudsonRefusedSiegeCharacter;
extern const char *const kHudsonRefusedSiegeNeedsAPair;
extern const char *const kHudsonRefusedSiegeFromEmpty;
extern const char *const kHudsonRefusedSiegeFromTooLong;
extern const char *const kHudsonRefusedSiegeFromCharacter;
extern const char *const kHudsonRefusedSiegeFromWithoutSiege;
extern const char *const kHudsonRefusedRemoteRelayEmpty;
extern const char *const kHudsonRefusedRemoteRelayTooLong;
extern const char *const kHudsonRefusedRemoteRelayCharacter;
extern const char *const kHudsonRefusedRemoteRelayWithoutNet;
extern const char *const kHudsonRefusedRemoteRelayHostile;
extern const char *const kHudsonRefusedStepSeconds;
extern const char *const kHudsonRefusedStepSecondsWithoutPlay;
extern const char *const kHudsonRefusedSiegeFarEmpty;
extern const char *const kHudsonRefusedSiegeFarTooLong;
extern const char *const kHudsonRefusedSiegeFarCharacter;
extern const char *const kHudsonRefusedCustomMapEmpty;
extern const char *const kHudsonRefusedCustomMapName;
extern const char *const kHudsonRefusedSiegeFarWithoutSiege;
extern const char *const kHudsonRefusedLookNeedsAPair;
extern const char *const kHudsonRefusedFilmSkipNotANumber;
extern const char *const kHudsonRefusedFilmSkipWithoutFilms;
extern const char *const kHudsonRefusedDifficultyUnknown;
extern const char *const kHudsonRefusedListPlaceNotANumber;
extern const char *const kHudsonRefusedListPlaceWithSpecies;
extern const char *const kHudsonRefusedListPlaceWithClass;
extern const char *const kHudsonRefusedListPlaceWithNet;
extern const char *const kHudsonRefusedAlienWithGun;
extern const char *const kHudsonRefusedGunWithAMarine;
extern const char *const kHudsonRefusedFlareNotYes;
extern const char *const kHudsonRefusedFlareSpoilsComparing;
extern const char *const kHudsonRefusedFlareWithoutHolds;
extern const char *const kHudsonRefusedLampUnknown;
extern const char *const kHudsonRefusedLampSpoilsComparing;
extern const char *const kHudsonRefusedLampKindUnknown;
extern const char *const kHudsonRefusedLampAtUnknown;
extern const char *const kHudsonRefusedLampWordsWithoutReal;
extern const char *const kHudsonRefusedVideoMeasuresTime;
extern const char *const kHudsonRefusedNetUnknown;
extern const char *const kHudsonRefusedNetMeasuresTime;
extern const char *const kHudsonRefusedNetNotAlone;
extern const char *const kHudsonRefusedFilmUnknown;
extern const char *const kHudsonRefusedFilmBothWays;
extern const char *const kHudsonRefusedBreakVideoWithoutVideo;
extern const char *const kHudsonRefusedBreakUnknown;
extern const char *const kHudsonRefusedBreakWithoutVision;
extern const char *const kHudsonRefusedVisionUnknown;
extern const char *const kHudsonRefusedVisionWithoutRooms;
extern const char *const kHudsonRefusedVisionWithoutPictures;
extern const char *const kHudsonRefusedVisionWithoutViews;
extern const char *const kHudsonRefusedSizeUnknown;
extern const char *const kHudsonRefusedShowNotYes;
extern const char *const kHudsonRefusedShowWithoutSharing;
extern const char *const kHudsonRefusedMachineUnknown;
extern const char *const kHudsonRefusedWallInManyRooms;
extern const char *const kHudsonRefusedWallEmpty;
extern const char *const kHudsonRefusedWallTooLong;
extern const char *const kHudsonRefusedWallCharacter;
extern const char *const kHudsonRefusedWallWithoutRooms;
extern const char *const kHudsonRefusedWallRankNotANumber;
extern const char *const kHudsonRefusedWallRankOutOfRange;
extern const char *const kHudsonRefusedWallRankWithoutWall;
extern const char *const kHudsonRefusedWallRoomEmpty;
extern const char *const kHudsonRefusedWallRoomTooLong;
extern const char *const kHudsonRefusedWallRoomCharacter;
extern const char *const kHudsonRefusedWallRoomWithoutWall;
extern const char *const kHudsonRefusedStrategiesUnknown;
extern const char *const kHudsonRefusedStrategiesWithoutRooms;
extern const char *const kHudsonRefusedStrategyEmpty;
extern const char *const kHudsonRefusedStrategyTooLong;
extern const char *const kHudsonRefusedStrategyCharacter;
extern const char *const kHudsonRefusedStrategyWithoutRooms;
extern const char *const kHudsonRefusedStrategyWithWall;
extern const char *const kHudsonRefusedStrategyLookEmpty;
extern const char *const kHudsonRefusedStrategyLookTooLong;
extern const char *const kHudsonRefusedStrategyLookCharacter;
extern const char *const kHudsonRefusedStrategyLookWithoutStrategy;
extern const char *const kHudsonRefusedStrategySecondsNotANumber;
extern const char *const kHudsonRefusedStrategySecondsOutOfRange;
extern const char *const kHudsonRefusedStrategySecondsWithoutStrategy;
extern const char *const kHudsonRefusedCompareUnknown;
extern const char *const kHudsonRefusedCompareWithoutPictures;
extern const char *const kHudsonRefusedCombatUnknown;
extern const char *const kHudsonRefusedCombatWithoutRooms;
extern const char *const kHudsonRefusedCombatNotAlone;
extern const char *const kHudsonRefusedAliensNotANumber;
extern const char *const kHudsonRefusedAliensOutOfRange;
extern const char *const kHudsonRefusedAliensWithoutCombat;
extern const char *const kHudsonRefusedWatchUnknown;
extern const char *const kHudsonRefusedHoldFireNotYes;
extern const char *const kHudsonRefusedPlaceNotThreeNumbers;
extern const char *const kHudsonRefusedWatchWordsWithoutCombat;
extern const char *const kHudsonRefusedHoldFireWithoutWatch;
extern const char *const kHudsonRefusedWatchWithAliens;
extern const char *const kHudsonRefusedWatchFromUnknown;
extern const char *const kHudsonRefusedWatchFromWithoutWatch;
extern const char *const kHudsonRefusedLampSizeNotANumber;
extern const char *const kHudsonRefusedLampSizeWithoutLamp;
extern const char *const kHudsonRefusedFollowName;
extern const char *const kHudsonRefusedWalkPlaceNotThreeNumbers;
extern const char *const kHudsonRefusedWalkNeedsBoth;
extern const char *const kHudsonRefusedLookAtWithoutWalk;
extern const char *const kHudsonRefusedWalkNotAlone;
extern const char *const kHudsonRefusedTogetherNotANumber;
extern const char *const kHudsonRefusedTogetherWithoutWatch;
extern const char *const kHudsonRefusedEveryNotASecond;
extern const char *const kHudsonRefusedEveryWithoutTogether;
extern const char *const kHudsonRefusedTooManyPlaces;
extern const char *const kHudsonRefusedPlacesWithoutWatch;
extern const char *const kHudsonRefusedTogetherNotThePlaces;

// A run must never touch the player's own copy of the game. So it won't start from anything that looks like a Steam folder. This is against a slip, not against someone determined.
bool HudsonPlan_FolderIsRefused(const char *folder);

// Whether a window with a picture this big fits on a screen this big, title bar and all.
bool HudsonPlan_SizeFits(int width, int height, int screenWidth, int screenHeight);

// Where one game's window goes on a screen, given the part of that screen that's free. One window has the whole of it, two sit side by side, and three or four share a grid.
// *width and *height say how big the window would like to be. It's made smaller if it doesn't fit its part, the same shape and never bigger, and the game goes on drawing the same size of picture, which Windows shrinks to fit.
// A window that doesn't know its size yet, which is any size that isn't more than 0, is given the whole of its part.
void HudsonPlan_WindowPlace(int areaLeft, int areaTop, int areaWidth, int areaHeight, int slot, int together, int *left, int *top, int *width, int *height);

// The part of the screen's free area one of two windows has, when one of them has the big part: that one has the left three quarters, and the other the right quarter, each the whole
// height. A window is then fitted into its part by HudsonPlan_WindowPlace, as the only window there. Three quarters of the second screen here is 1280 wide in the game's own numbers,
// which, because the game isn't told about the screen's 150% scaling, is about 1920 real pixels.
void HudsonPlan_WindowPart(int areaLeft, int areaTop, int areaWidth, int areaHeight, bool big, int *left, int *top, int *width, int *height);

// Whether a run may go in this folder. The one way into a refused folder is steam=yes, on a command line with nothing else wrong with it. The user asked for that on 2026-11-05, so that a run can be recorded
// from their own copy of the game. A run's files all go in its own folder beside the game, wherever that is.
bool HudsonPlan_FolderIsAllowed(const char *folder, const char *refusal, const HudsonPlan &plan);

// Which way this view looks, as a distance along the world's two level directions: kHudsonViewDistance away, spaced evenly from straight along z. The same view of the same number of views always gives the same answer.
void HudsonPlan_ViewDirection(int view, int views, int *x, int *z);

// Is someone facing this way, as near as makes no difference? Each is a direction along the world's two level directions, of any length. A direction with no length isn't facing anywhere.
bool HudsonPlan_IsFacing(int forwardX, int forwardZ, int wantedX, int wantedZ);

// One frame's turn from one angle towards another, the short way round, as someone would turn their head: quickly at first, slowing as it arrives, and never more than mostThisFrame. Angles are 4096ths of a circle.
int HudsonPlan_LookStep(int current, int wanted, int mostThisFrame);

// Whether the players of this run can't be hurt at all. Only siege= says so, and it only ever comes from a run's own command line. The game's own protection, invulnerabilityTimer, ends whenever a player fires, and a siege's players fire nearly all the time.
bool HudsonPlan_PlayersCannotDie(const HudsonPlan &plan);

// Writes a ten figure number in the top left of a picture of four bytes to the pixel, white on a black box, for finding the same moment in two films. What doesn't fit in the picture is left out.
void HudsonPlan_StampNumber(unsigned char *pixels, int pitch, int width, int height, unsigned long number);

// How the clock looks, which HudsonPlan_StampNumber writes and tools/hudson/join_films_reads.cpp reads back. Each figure is 3 squares
// across and 5 down, one row of bits for each line, with the left-hand square as the highest bit. A fourth, dark square after each
// figure is the gap before the next. Changing any of this changes every film's clock, so the reader has to agree.
const unsigned char kHudsonStampFigures[10][5] =
{
	{ 7, 5, 5, 5, 7 }, { 2, 2, 2, 2, 2 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 }, { 5, 5, 7, 1, 1 },
	{ 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 }
};
int const kHudsonStampFiguresInANumber = 10;
int const kHudsonStampSquare = 4;       // pixels to a square
int const kHudsonStampFromTheEdge = 8;  // pixels from the picture's top and left edges

// The smoke test: what this machine can tell about the other player of a pair, and how long it has been trying.
struct HudsonSmokeFacts
{
	bool dead;      // this player has died
	bool bodyHere;  // the other player's ghost exists on this machine
	bool facing;    // this player is facing it
	bool drawn;     // the game has it on its list of what's on the screen, with a model
	bool wayClear;  // nothing stands between this player's eyes and it
	unsigned long millisecondsWaiting; // since someone else was in the game, for a body that doesn't come
	unsigned long millisecondsSeen;    // since its body first came
};

enum
{
	kHudsonSmokeWaiting = 0,
	kHudsonSmokePassed,
	kHudsonSmokeFailed
};

enum
{
	kHudsonSmokeLeastLook = 3000,  // they turn, and someone watching has time to see them look at each other
	kHudsonSmokeMostLook = 10000,  // a short limit, so that it can't hang: then it fails, and the run goes on
	kHudsonSmokeMostWaitForABody = 60000 // a joiner can be on its loading screen this long, by the game's own limit, before its body comes
};

// Whether the test is still going on, or has passed or failed. If it failed, *why says what was wrong, the first thing that was. It's only passed when all of it is true at once.
int HudsonPlan_SmokeVerdict(const HudsonSmokeFacts &facts, const char **why);

#endif
