// The save test harness in the game (SAVE-HOSTILE-1, step 3). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY. The game's loader checks what it reads from a save (SAVE-2), but a check only proves itself on a real load. This loads one save, a
// broken copy made by tools/save_mutate, through the game's own path, plays a few seconds with the player standing still, writes one line,
// and closes the game. Run under the address sanitizer, a load that wrote or read where it shouldn't stops it at once and says where.
//
//   -dev -savetest <name>                 loads SaveTest\<name>.sav
//   -dev -savetest-make <species>,<episode>   starts that level as a new game, plays a few seconds, and saves it as
//                                          SaveTest\seed_<species>_<episode>.sav, a save of our own to break
//
// SAFE FOR THE PLAYER. The rules are in save_test_rules.h: only with -dev, never in a network game, and only a file in SaveTest named as
// the custom map name rule allows. And before anything else, on either word alone, the game's folder for its log, settings, profiles
// and saves is made in SaveTest, in the folder the game copy runs from (SaveTest\My Games\Aliens versus Predator\, as it would be in
// Documents), and never in the player's Documents. So even a run the rules refuse never touches the player's own log, settings or
// saves. The harness uses a profile of its own there, called SaveTest. The saves it loads, and the seeds it makes, are in SaveTest
// itself.
//
// WHAT'S CALLED WHERE:
//   - winmain.cpp, before the log is cleared: SaveTest_TakeCommandLine;
//   - winmain.cpp, once -dev and the network switches are read: SaveTest_Start, and if it says no, the game closes;
//   - the menus' frame (menus.cpp): SaveTest_MenusAsk says what to do on the first frame, and the menus do it, as a player's choices would;
//   - the loading screen (progress_bar.cpp): SaveTest_Running counts as a key pressed;
//   - the game's frame (game.cpp), after a save or a load has been done: SaveTest_GameFrame.
#ifndef SAVE_TEST_H
#define SAVE_TEST_H

#include <stddef.h>

int const kSaveTestFramesInLevel = 600;   // how long it plays after the level starts (or the load), before it saves or closes
int const kSaveTestRefusedExit = 0x5354;  // the game's exit code when a save test is refused ("ST")

// Before the log is cleared: reads -savetest or -savetest-make, and if either is there, points the game's folder at SaveTest. Says whether
// either was there.
bool SaveTest_TakeCommandLine(const char *commandLine);

// Once -dev and the network switches are read: true if the game goes on, false if the harness was asked for and refuses, having said why
// in the log; then the game closes.
bool SaveTest_Start(bool dev, bool networkGame);

bool SaveTest_Running();

// What the menus are to do for the harness, asked once a frame. kSaveTestMenusNothing once it's been answered.
enum SaveTestMenusAsk
{
	kSaveTestMenusNothing,
	kSaveTestMenusLoad,      // pick the SaveTest profile, put the save in its first slot, and load that slot
	kSaveTestMenusNewGame    // pick the SaveTest profile and start SaveTest_Species()'s SaveTest_Episode() on medium
};

SaveTestMenusAsk SaveTest_MenusAsk();
int SaveTest_Species();
int SaveTest_Episode();

// The save to load, as a full path: SaveTest\<name>.sav in the folder the game copy runs from
const char *SaveTest_SavePath();

// The menus say what they did: false if they couldn't (no profile, no slot), and the harness closes the game with the reason
void SaveTest_MenusDid(bool done, const char *why);

// The game's frame, after its save or load for the frame is done
void SaveTest_GameFrame();

#endif
