// The yes page: the player's yes to one servers.cfg line, before anything at all goes to its server (#175's last part). Written 2026-11-10
// by Melanikus for the AvP port.
//
// WHY. servers.cfg is a file anyone can be told to replace ("put this line in your servers.cfg"), so before the game sends a byte to a line's
// server, the player sees where it goes and says yes (the owner's ruling, #175). relay_list.cpp holds every line to that: nothing goes to
// a line's server or its relay until that line's yes (relay_list.h, ONE MODEL FOR EVERY LINE). This page is where the player answers. The
// yes is kept (servers_trust.h), so a line is asked about once on a PC; a new address, port or key is a new line, asked about again.
// Deleting servers-trusted.cfg in %LOCALAPPDATA%\AVPx asks again about every line.
//
// WHAT IT SHOWS, in PO's words (2026-11-10; the owner may reword them once he's seen the page in a Hudson picture):
//   Play online through this server?
//   New York
//   104.131.35.41, on the internet           (or "on this PC", or "on your own network")
//   key bb0e2958 ... f28d663e
//   Its owner approves their own players, and sees your name, your key and your address while you play.
// with two rows under it, "No, not now" first, then "Yes, use this server".
//
// THE ONE WAY IN. Modified 2026-11-11 by Melanikus for the AvP port (one model for every line; the owner: "UNIFY THE UI"): the page opens
// only from a line's row on the Online page (online_page.h), for that line, and goes back there after a yes or a no. There's no line "in
// use" any more, so no other page asks for "the" line's yes: Find games and the host page show a line without its yes as just that, and
// send it nothing. A Hudson run doesn't see the page unless it was asked for it with yespage= (hudson_plan.h): it sees it as a player
// does, for each line, and presses one of its rows; that yes holds for the run alone and is never kept (relay_list.h, RelayList_AgreeToTheLineAt).

#ifndef SERVER_YES_PAGE_H
#define SERVER_YES_PAGE_H

#include "menus.h"
#include "server_yes_rules.h"

// A line's row on the Online page was pressed and the line has no yes: true, and the page is ready to open for that line (its place among
// servers.cfg's good lines). False when there's nothing to ask (no such line, or its yes is already given).
bool ServerYesPage_OpenFor(int place);

// The page has opened. False when its line has no yes waiting after all, and then menus.cpp goes back to the Online page.
bool ServerYesPage_Opened(void);

// A row was pressed. True, with page set, when the menus should go to another page: the Online page, after the yes (kept,
// RelayList_AgreeToTheLineAt) or after the no, with nothing sent.
bool ServerYesPage_Pressed(int row, enum AVPMENU_ID *page);

// The page's words, above its rows, which start at rowsTop (server_yes_rules.h, THE PAGE'S LAYOUT). Modified 2026-11-10 by Melanikus for
// the AvP port: the rows' top is given, so nothing is drawn into them.
void ServerYesPage_Draw(int rowsTop);

#endif
