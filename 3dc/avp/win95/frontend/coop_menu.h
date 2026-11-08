// The pages of the co-op campaign: the host's, where a game is chosen while others gather, the joiner's, which shows the same rows and
// can't change them, and a plain one for a joiner who is out of the game. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the co-op campaign system
// follows Bobblen's avp-forever, rewritten).
//
// coop_lobby.h has the rules, and pldnet.cpp the messages. This is only what the pages do with them.

#ifndef _coop_menu_h_
#define _coop_menu_h_ 1

#include "3dc.h"

// What the rows on the two pages show (menu_data.cpp)
extern int CoopRowCampaign;
extern int CoopRowPlace;
extern int CoopRowSpecies;

enum
{
	kCoopRowTextBytes = 120
};

extern char CoopRowPlayers[kCoopRowTextBytes];
extern char CoopRowNoMap[kCoopRowTextBytes];
extern char CoopRowMapLine[kCoopRowTextBytes];

// What menus.cpp has to do after a frame of one of the pages
enum CoopPageAnswer
{
	kCoopPageStays,
	kCoopPageGoesToTheLoad,  // the host has started, and this joiner has the map
	kCoopPageRowsChanged,    // someone has come or gone, so the rows on the page have changed
	kCoopPageHostHasGone,
	kCoopPageJoinerIsOut     // the game has gone on without this joiner, or the host has removed them
};

// The host's page opens the game. False if the game couldn't be opened.
bool CoopMenu_HostPageOpens(void);
void CoopMenu_JoinPageOpens(void);

// The page for a player the co-op game has ended for. It works out what to say from how that happened. False if nothing did, and there's
// nothing to say.
bool CoopMenu_OutPageOpens(void);

// winmain.cpp, as a network game ends and before anything is wiped: notes why it ended, for the page after it
void CoopMenu_TheGameIsEnding(void);

// Is there something to tell the player about the game that just ended? AvP_MainMenus goes to that page first.
bool CoopMenu_HasALastWord(void);

// The pause menu in a campaign: makes Quit a question on the host's machine
void CoopMenu_InGameMenuOpens(void);

// Each frame of the Players page: fills in its rows. True if the number of rows changed.
bool CoopMenu_PlayersPageFrame(void);

// One player's line, "Hicks: alive". False if nobody is at that place on the list.
bool CoopMenu_PlayerLine(int place, char *line, size_t room);

// A level's name in a campaign, as the Start at row shows it
char *CoopMenu_LevelName(int campaign, int place);

CoopPageAnswer CoopMenu_EveryFrame(bool isTheHostsPage);

// The host has pressed Start. False if there's nothing to start: a custom map whose file has gone.
bool CoopMenu_HostStarts(void);

// The host has pressed a player's Remove row. The number is their place on the list of players.
void CoopMenu_HostRemoves(int place);

// Either page is being left, by the player or because the host has gone. This closes or leaves the game.
void CoopMenu_PageLeft(void);

// A picture for the chosen level: the episode's own, or the custom map's. MISSING_TEXTURE if it has none.
texID_t CoopMenu_Picture(void);

// The page to go back to when a custom map ends, which depends on how it was started. It's an AVPMENU_ID.
int CoopMenu_PageAfterACustomMap(void);

#endif
