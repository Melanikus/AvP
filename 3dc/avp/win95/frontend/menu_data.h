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

// Where a host's game is hosted: the Multiplayer page's Host row. Added 2026-11-10 by Melanikus for the AvP port (PO's ruling). Online
// goes through the server servers.cfg names (networking.h, Net_HostGame); At home is a plain game for the home network alone
// (Net_HostAtHome), which the game itself keeps deaf to every other address. The row is there only when servers.cfg names a server; with
// none, every game is a home game. Never saved: every start of the game begins on Online, so it's in no file a player shares or edits
// (release check rule C).
enum { kHostWhereOnline = 0, kHostWhereAtHome = 1 };
extern int MP_HostWhere;

// Writes the Multiplayer page's rows, each time it opens: the player's name; the Host row when aServerIsNamed, its two choices
// hostWhereNames (online, then at home); Start game, Co-op campaign and Join game, as in 1999; and the Online access row when
// aServerIsNamed (access_page.h). Added 2026-11-10 by Melanikus for the AvP port, in place of MenuData_ShowOnlineAccessRow.
void MenuData_SetMultiplayerRows(bool aServerIsNamed, char **hostWhereNames);

#endif