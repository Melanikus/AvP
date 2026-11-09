// Hudson playing a level on foot: play=yes. Written 2026-11-06 by Melanikus for the AvP port.
//
// It's the real player, driven through the same fields the keyboard fills in, in a level running as it does for anyone. Nothing is set off except by what a player does there. If he
// can't get on he cheats, and each cheat is logged and counted, so a level finished that way doesn't show a player can. hudson_play.cpp says how.

#ifndef HUDSON_PLAY_H
#define HUDSON_PLAY_H

#include <vector>

struct HudsonPlayRibbonCorner;

// Reads the level's list of what to do. False means it isn't there or was refused, and the log says why.
bool HudsonPlay_Start(const char *levelName);

// For the second of two Hudsons in a game: he has no steps to do. He follows the first, and fights what he sees. The level's list is read only so that his machine can say what becomes of each step.
void HudsonPlay_StartFollowing(const char *levelName);

// Once a frame, straight after the game has read the keyboard: decides what he does this frame. otherPlayer is the other Hudson's body, a STRATEGYBLOCK, or null if there's none: the second
// follows it, and the first waits for it.
void HudsonPlay_Input(void *playerStatus, const void *otherPlayer);

// Once a frame at its end: has this come to an end? Gives back 0 to carry on, or else one of the run's exit codes, with what to say about it.
int HudsonPlay_EndOfFrame(const char **howItEnded);

// The ribbon, which shows where he means to walk and where he's been, on every run he plays. The drawing code asks the first once a frame, and the second makes the frame's shapes.
bool HudsonPlay_RibbonIsOn();
void HudsonPlay_RibbonCorners(std::vector<HudsonPlayRibbonCorner> &corners);

// Once a frame, from the moment the level's loaded, even while he waits for the other Hudson: says what's become of each step's switch and of each lift, what's near a lift, and any fall.
// None of it changes what he does. theOtherPlayer is as for HudsonPlay_Input.
void HudsonPlay_WatchTheLevel(const void *theOtherPlayer);

// While a Hudson in a pair waits for the other, he may not fight, so something that finds him then could kill him. He's kept alive the same way as while he plays, and each time is a cheat.
void HudsonPlay_KeepHimAliveWhileHeWaits(void *playerStatus);

// playgun=: the slot of the gun he's given and holds, from 1 to 14. Asked for before he starts.
void HudsonPlay_HoldGun(int slot);

// stepseconds=: how long one step may take before he's moved to it, a logged cheat, from 10 to 600 (the plan reader checks it). Without it, 180.
void HudsonPlay_SetStepSeconds(int seconds);

// One line for the run's result: how many steps there were, how many worked, and how many cheats it took
void HudsonPlay_ForTheResult(char *line, int size);

#endif
