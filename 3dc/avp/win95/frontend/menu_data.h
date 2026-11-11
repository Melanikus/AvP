// Modified 2026-11-06 by Melanikus for the AvP port:
//   the sidearm a player chose
#ifndef _AvP_MenuData_h_
#define _AvP_MenuData_h_

#include "pldnet.h"

#define MAX_MULTIPLAYER_NAME_LENGTH 10

extern int MP_Species;
extern int MP_Sidearm;
extern int MP_GameStyle;
extern int MP_LevelNumber;

extern char MP_Config_Description[202];
extern char MP_PlayerName[NET_PLAYERNAMELENGTH+3];
extern char MP_SessionName[MAX_MULTIPLAYER_NAME_LENGTH+23];

// Writes the Multiplayer page's rows, each time it opens: the player's name; the Servers row; the Host on row when servers.cfg has a line;
// Start game, Co-op campaign and Join game, as in 1999; and the Online access row when servers.cfg has a line (access_page.h). Added
// 2026-11-10 by Melanikus for the AvP port, in place of MenuData_ShowOnlineAccessRow; modified the same day (the owner's ruling for
// beta.2: every game through a server's relay): the Host row, Online or At home, is gone; modified 2026-11-11 (the owner's rulings:
// "they will choose their friends' server", "No auto choice, they choose"): the Host on row, whose choices are hostOnNames, hostOnCount of
// them after its first, "choose a server" (menus.cpp, HostOnChoices).
void MenuData_SetMultiplayerRows(bool aServerIsNamed, char **hostOnNames, int hostOnCount);

// The Host on row's choices again, as approvals and round trips change while the page is open (each frame, menus.cpp). A choice past the
// new count goes back to "choose a server".
void MenuData_SetHostOnChoices(char **hostOnNames, int hostOnCount);

// The Host on row's choice: 0, "choose a server", until the player moves it; then the n-th of its approved lines. It's 0 at every start
// of the game and never saved, so nothing is ever chosen for the player (added 2026-11-11 by Melanikus for the AvP port).
extern int MP_HostOnChoice;

#endif