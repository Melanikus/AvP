// The yes page: the player's yes to the server in servers.cfg, before anything at all goes to it (#175's last part). Written 2026-11-10 by
// Melanikus for the AvP port.
//
// WHY. servers.cfg is a file anyone can be told to replace ("put this line in your servers.cfg"), so before the game sends a byte to a line's
// server, the player sees where it goes and says yes (the owner's ruling, #175). relay_list.cpp holds the list to that: until the yes,
// it isn't started, so nothing goes to the server or its relay (relay_list.h, RelayList_LineToAgree). This page is where the player
// answers. The yes is kept (servers_trust.h), so a line is asked about once on a PC; a new address, port or key is a new line, asked about
// again. Deleting servers-trusted.cfg in %LOCALAPPDATA%\AVPx asks again about every line.
//
// WHAT IT SHOWS, in PO's words (2026-11-10; the owner may reword them once he's seen the page in a Hudson picture):
//   Play online through this server?
//   New York
//   104.131.35.41, on the internet           (or "on this PC", or "on your own network")
//   key bb0e2958 ... f28d663e
//   Its owner approves their own players, and sees your name, your key and your address while you play.
// with two rows under it, "No, not now" first, then "Yes, use this server".
//
// THE WAYS IN are the three that would use the line: Find games, Host online and the Online access page. Each asks ServerYesPage_Needed
// before it goes on (Primary hooks them into his screens; PO, 2026-11-10). A Hudson run never sees the page: relay_list.cpp needs no yes
// in one, as nobody is there to answer.

#ifndef SERVER_YES_PAGE_H
#define SERVER_YES_PAGE_H

#include "menus.h"
#include "server_yes_rules.h"

// Before a way online: true when the line needs the player's yes first. Then the caller opens AVPMENU_SERVER_YES in its place, and after a
// yes the page goes on to goTo, after a no back to backTo. False when there's nothing to ask (no line, a yes already given, or a Hudson
// run): then the caller goes on as it was.
bool ServerYesPage_Needed(enum AVPMENU_ID goTo, enum AVPMENU_ID backTo);

// The page has opened. False when there's no line waiting for a yes after all, and then menus.cpp goes back to the Multiplayer page.
bool ServerYesPage_Opened(void);

// A row was pressed. True, with page set, when the menus should go to another page: goTo after the yes (kept, RelayList_AgreeToTheLine),
// backTo after the no, with nothing sent.
bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page);

// The page's words, above its rows
void ServerYesPage_Draw(void);

#endif
