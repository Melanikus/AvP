// Modified 2026-11-06 by Melanikus for the AvP port:
//   a run's window goes on the second screen by the game's own doing, with a part of it for each game
//   a run can play a level's own films, and say how the connection fared under one
//   a run can start a level as the level row does, say what the player meets, die, and ask for a difficulty
//   class= starts a gun= run as a skirmish as that marine class
//   gun= tries a marine's gun
//   species=predator lets a gun= run play as a Predator
// Hudson: a marine the camera follows, so that the game can be measured with nobody at the keyboard. Written 2026-11-04 by Melanikus for the AvP port. See hudson.cpp.

#ifndef HUDSON_H
#define HUDSON_H

#include <stddef.h>

struct vectorch;
struct matrixch;
struct strategyblock;
typedef struct vectorch VECTORCH;
typedef struct matrixch MATRIXCH;
typedef struct strategyblock STRATEGYBLOCK;

// True only when the command line asked for a run, with -dev and -hudson. In a normal game this is false for the whole of the game, and everything that asks it carries on as if none of this were here.
// So that a player's game can never run Hudson, every one of Hudson's functions the game calls starts by doing nothing unless this is true, and gives back
// what it would with no run. tools/check-hudson-asleep.ps1 checks that, on every pull request and before a release.
extern bool HudsonRunIsOn;

// break=prisms: the shadow prisms aren't sent to the card, so that a run can see Hudson's vision notice. False in any other run.
extern bool HudsonBreaksPrisms;

// True only in a run that asked for a marine. Then the camera follows him and nothing else in the world moves by itself, so everything that asks this carries on as usual in a normal game, and in a run with no marine.
extern bool HudsonMarineIsOn;
extern bool HudsonPeopleStandStill; // gunalien=yes and gunmarine=yes: aliens and marines don't run, so that a gun run has something that stays where it is to look at

// deathway=: every NPC dies this way, the number on its list in deaths.cpp. -1 if the run didn't ask, and always in a real game.
extern int HudsonWayOfDying;

// True only in a run that asked for a fight, with combat=. Then nothing in the world is kept still, and a marine can have a target like any other.
extern bool HudsonWorldIsLive;

// True only in a watched fight with holdfire=yes. Then a marine takes nothing as a target, so the alien being watched gets to him. False in a real game.
extern bool HudsonMarineHoldsFire;

// Something has hit a marine, in a run with a fight. He's kept alive, so his health can't show whether anything reached him, and this says so in the log. kind is what hit him: the game's number for it, from AMMO_ID in equipment.h.
void Hudson_HeWasHit(STRATEGYBLOCK *block, int kind);

// A marine has come down after falling, at this speed, and wouldKillHim is what the game's own rule says about that speed. Nothing's hurt in a run, so this is how a walk finds out.
void Hudson_MarineLanded(const STRATEGYBLOCK *marine, int fallingSpeed, bool wouldKillHim);

// How a run ended, for a script to read. A level that doesn't load, and a failed assert, keep the game's own codes, 0x3421 and 0xffff. A crash is whatever Windows says.
enum HudsonExitCode
{
	kHudsonExitFinished = 0,
	kHudsonExitBadArguments = 21,
	kHudsonExitNoSuchLevel = 22,
	kHudsonExitWrongFolder = 23,       // this copy of the game is one a run must not touch. Nothing is written, so this number is all there is to read
	kHudsonExitMachineInUse = 24,      // someone had touched the machine too recently to start
	kHudsonExitSomeoneTouchedIt = 25,  // someone touched the machine during the run
	kHudsonExitOutOfTime = 26,
	kHudsonExitMessageBox = 27,        // the game wanted to ask someone something, and nobody's there
	kHudsonExitMarineGone = 28,        // the marine couldn't be made, or he's gone: destroyed or dead
	kHudsonExitMarineStuck = 29,       // he never settled facing the way he was told, or he moved while he was meant to hold still
	kHudsonExitOutOfFrames = 30,       // frames= ran out before he'd finished
	kHudsonExitModeNotThere = 31,      // lighting= or shadows= asked for something this machine can't draw
	kHudsonExitNoSuchRoute = 40,       // route= asked for one that isn't there, or that has something wrong with it
	kHudsonExitFatalFall = 41,         // walking, he fell further than the game lets a marine survive
	kHudsonExitPlayerDied = 42,        // walking or playing, the player died
	kHudsonExitLevelDidntEnd = 43,     // a route or a list of steps was over, and the game doesn't say the level is complete
	kHudsonExitLevelEndedEarly = 44,   // the game says the level is complete, and he wasn't at the end of the route
	kHudsonExitPlayFinished = 45,      // play=yes: the game ended the level, and he was playing it himself
	kHudsonExitPlayStuck = 46,         // play=yes: there was no grid for him to find his way on
	kHudsonExitPlayFinishedWithCheats = 47, // play=yes: the game ended the level, but only with cheats, so this doesn't show a player can
	kHudsonExitPlayAlone = 48,         // play=yes in a pair: the other's body never came, or the two never saw each other, so neither played on alone
	kHudsonExitNoSuchWall = 32,        // wall= asked for a wall this level hasn't
	kHudsonExitNoSuchStrategy = 33,    // strategy= asked for one this level hasn't, or there's nowhere to stand to see it
	kHudsonExitStrategyDidNothing = 34,// it was asked to go on, and its state never changed
	kHudsonExitNetworkGame = 35,       // strategies are only looked at or asked in a game with nobody else in it
	kHudsonExitNoEnemy = 36,           // combat= couldn't make its alien, or there was nowhere in sight of him to put it
	kHudsonExitNetStopped = 50,        // net=: the two games couldn't get as far as playing together, and the result's net_stage says where it stopped
	kHudsonExitStoppedEarly = 37       // the game's main loop stopped with nothing having ended the run: its window was closed, or the graphics card failed
};

// With gun= on the command line, presses the player's keys for this frame. The game calls this straight after reading the real ones. playerStatus is the PLAYER_STATUS. See hudson.cpp.
void Hudson_GunInput(void *playerStatus);

// The first thing the game does: finds out whether this is a run. It has to come before anything that uses the save folder, and that includes the log. If it's a run in a copy of the game that a run must not
// touch, the game ends here, with nothing written.
void Hudson_ReadCommandLine(const char *commandLine);

// Once the log can be written to: checks everything about the run that can be checked before it starts. Gives back an exit code to stop with now, or 0 to carry on. A normal game always gets 0.
int Hudson_CheckBeforeStarting();

// Where this run keeps its files, in place of the player's Documents folder. It's beside the game, it's made if it isn't there, and it isn't anything the command line said. false means it can't be used.
bool Hudson_RunFolder(char *folder, size_t size);

// After the settings have been read: the same window size and no waiting for the screen, whatever the file says.
void Hudson_SetVideoSettings();

// In a run, says in the log which pixel shader file was just compiled, how big it is, and a check of its bytes, so that nobody has to guess which one a run used.
void Hudson_SayWhichShaderWasCompiled(const char *path);

// A run's window stays out of the way, unless it was asked to show itself: then this is false, and the window opens as a normal game's does.
bool Hudson_StaysOutOfTheWay();

// A run's window goes on the second screen, where it covers nothing anyone's using, if there is one and the run didn't say screen=main. If so, this changes where it goes and how big it is, and says true. Outside a run it does nothing and says false.
bool Hudson_PlaceTheWindow(int *left, int *top, int *width, int *height);

// The log says where a run's window really went. Outside a run it does nothing.
void Hudson_SayWhereTheWindowWent();

// With show=yes, makes sure the window can be seen. See hudson.cpp.
void Hudson_BringTheWindowForward();

// Just before the level loads: sets how the world is drawn, if the run asked. Everything that decides it at the game's start has had its say by then.
void Hudson_SetTheMode();

// Once a frame has been drawn, and before it goes to the screen, which is the last moment it can be read: saves a picture of it, if one is due.
void Hudson_TakePictureIfDue();

// In a gun test with pictures: is this a frame to save with and without the painted lamp? The picture before it, the picture after it, and the surface picture it reads, all from one frame, show what the lamp does to a pixel. See hudson.cpp.
bool Hudson_LampCompareIsDue();
void Hudson_SaveLampCompareScreen(const char *stage);
void Hudson_SaveLampCompareSurface(struct IDirect3DTexture9 *surface);

// Which marine level this run is for. Only asked once the run's been checked.
int Hudson_MarineEpisode();

// Which of the game's own levels the run was asked for. See BuiltInEnvironmentNamed in env_info.cpp.
int Hudson_Environment();
// custommap=: the place in the custom maps' list of the one this run plays instead of level=, or -1 for none
int Hudson_CustomMapPlace();
// A picture of what's about to be shown, for a page of the menus
void Hudson_SaveThePage(const char *name);
// custommap= with coopend=: has the map been finished? And each frame of the menus after that: is the Custom maps page back?
bool Hudson_CustomMapHasEnded();
void Hudson_LooksForThePageAfterAMap(bool thePageIsBack);

// The presses a player makes on the Online access page and on the host's answer (hudson_plan.h's access= and answer=). Added 2026-11-09
// by Melanikus for the AvP port.
// access=: kHudsonAccess..., 0 for none, and accesswait='s seconds
int Hudson_AccessPress();
int Hudson_AccessWaitSeconds();
// answer=: kHudsonAnswer..., 0 for none, and answerafter='s seconds
int Hudson_AnswerPress();
int Hudson_AnswerAfterSeconds();
// findgames=yes: a joiner through the relay joins by Find Games and the list's game's row, as a player does
bool Hudson_JoinsThroughFindGames();
// A picture of this frame, named name, taken once it's drawn whole (Hudson_TakePictureIfDue). Only one waits at a time; a second asked for
// before the first is taken replaces it.
void Hudson_PictureThisFrame(const char *name);
// An access run is over: what happened, in fixed words, and whether it went as asked
void Hudson_AccessRunEnds(const char *howItEnded, bool asAsked);

// Whether the run plays as a Predator: species=predator on a gun= run. A marine otherwise.
bool Hudson_PlayAsPredator();

// species=alien: a run playing single player as an Alien.
bool Hudson_PlayAsAlien();

// listplace=: the place on the co-op level list that this run starts, as the level row would. -1 if it wasn't asked for.
int Hudson_ListPlace();

// difficulty=: the game's number for it. Medium unless asked.
int Hudson_Difficulty();
bool Hudson_NoFriendlyFire();

// siege=: a pair of games stands in one room of a level, played the way a campaign is, whichever level it is.
bool Hudson_IsASiege();

// In a siege, on the host: notes a blow on a friendly marine or a sentry gun, and what it came from, for the log. Called where any damage starts. Outside a siege it does nothing.
void Hudson_NoteDamage(STRATEGYBLOCK *target, int ammunition);

// levelfilms=yes: this run plays a level's own films.
bool Hudson_PlaysLevelFilms();

// For the log: a film has started, and it has ended. Outside a run they do nothing.
void Hudson_FilmStarts(const char *film);
void Hudson_FilmEnded();

// Asked for each frame of a film in a network game. False stops the film, as a key would (filmskip=). Always true outside a run.
bool Hudson_FilmGoesOn();

// dieat=: true once, three seconds after he has died, for whatever is waiting for a key at that point.
bool Hudson_PressesAKeyAfterHeDied();

// The level has just started again, as it does after dying.
void Hudson_TheLevelStartedAgain();
// net=: is this run one of two games on this machine, and is it the host? See hudson_plan.h.
bool Hudson_NetIsOn();
bool Hudson_NetIsHost();

// coop=yes: this net= run plays a campaign.
bool Hudson_NetIsCampaign();

// hostile=yes: this run sends bad messages on purpose, to see each one refused
bool Hudson_NetIsHostile();

// typed=yes: the joiner finds the game the way a player does who has typed its address
bool Hudson_NetIsTyped();

// The campaign has ended, so the run does too.
void Hudson_NetCampaignIsOver();

// A picture is due of what's on the screen, for proof of what each machine saw.
bool Hudson_NetPictureIsDue();

// What the name of the picture that's due should say: "smoke" for a smoke test's, and otherwise "net".
const char *Hudson_NetPictureKind();

// A dead player in a net= run: says so once, and asks to come back a little later, so a run can see the game's respawn.
void Hudson_NetWhileDead(void *playerStatus);

// How far it's got, for the log and for the result. A run that stops says the last of these, which is how to see where a join stopped.
void Hudson_NetSay(const char *stage);

// A test run's log says what a damage message from another machine landed on. A real game says nothing.
void Hudson_NetSayAHit(int ammo, STRATEGYBLOCK const *hit);

// And when an NPC's death reaches it, so a ghost that died can be told from one that only timed out.
void Hudson_NetSayAnNpcDied(int deathCode);

// And what kind of ghost it refused for having no model or shape, because the reason alone doesn't say.
void Hudson_NetSayARefusedGhost(int kind, int subtype);

// Once in the level: says who it can see and how it's doing, and the joiner goes over to the host, shoots it, and leaves. Called each frame with the player's status.
void Hudson_NetInput(void *playerStatus);
void Hudson_PlayInput(void *playerStatus);

// It can't get any further, so the run ends here, before a level has started.
void Hudson_NetStopBecause(const char *why);

// class=: the place on the class menu's slider that a gun= run starts a skirmish as, or 0 if it doesn't
int Hudson_ClassSlider();

// The end of each frame of the level.
void Hudson_EndOfFrame();

// Just before each frame is drawn: puts the camera where the marine is, and hides him, or it would be inside his head. It leaves the camera alone until he's been made, or if he's gone.
void Hudson_PointTheCamera(VECTORCH *place, MATRIXCH *turn);

// Is this something that stays still in a run with a marine? Other people, and whatever makes more of them, don't run, so that nothing happens to him and two runs can be the same.
bool Hudson_KeepsStill(STRATEGYBLOCK *block);

// The game wanted to show a message and wait for someone. In a run, this says so and ends the game, and doesn't come back. In a normal game it does nothing.
void Hudson_StopIfTheGameWantsToAsk(const char *message);

// A failed assert would wait for someone to press a key. In a run, this says so and ends the game with the game's own code for it. In a normal game it does nothing.
void Hudson_StopIfAnAssertFailed(const char *file, int line, const char *condition);

// The very end of the game, when everything has shut down. Gives back the exit code for the game to end with, which in a normal game is always 0.
int Hudson_LastThingBeforeExit();

#endif
