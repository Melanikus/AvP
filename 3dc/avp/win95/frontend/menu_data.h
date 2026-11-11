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

// Writes the Multiplayer page's rows, each time it opens: the player's name; the Servers row; Start game, Co-op campaign and Join game,
// as in 1999; and the Online access row when aServerIsNamed (access_page.h). Added 2026-11-10 by Melanikus for the AvP port, in place of
// MenuData_ShowOnlineAccessRow; modified the same day (the owner's ruling for beta.2: "Everything over nyc3"): the Host row, Online or At home, is gone, as every game is hosted
// online.
void MenuData_SetMultiplayerRows(bool aServerIsNamed);

#endif