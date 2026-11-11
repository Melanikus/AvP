// The Servers page: every server servers.cfg names, which one the game uses, and the player's pick of another for the next start (#175 2b,
// PO's ruling A). Written 2026-11-10 by Melanikus for the AvP port.
//
// WHY. servers.cfg can name up to 64 servers. The game uses one a run: the player's pick, or the first good line when there's none
// (relay_list.h, "Every servers.cfg line"). This page shows them all, so the player sees which is in use and can pick another, and it
// shows each line the game can't use by its number and the game's own words, so someone following an instruction can see what to fix.
//
// WHAT IT NEVER DOES. Nothing on it contacts a server: it reads what servers.cfg said when it was read, once a run, through relay_list's
// own calls (never servers.cfg itself: release check rule R). A pick is used from the next start of the game, never halfway through one
// (RelayList_PickLine). A picked server still needs the player's yes before anything goes to it: the yes page comes at its first use, as
// for any line. "Yes given" is shown only from RelayListLineShown.agreed, which only the kept yeses decide, never from the pick.
//
// It's reached by the Multiplayer page's Servers row, which is always there, so that even with no servers.cfg at all the page can say why.

#ifndef SERVERS_PAGE_H
#define SERVERS_PAGE_H

#include "menus.h"
#include "servers_page_rules.h"

// The page has opened: its rows are written into rows, which has room for kServersPageRowRoom (a row for each server, the Back row, the
// end). False if there's no room, and then menus.cpp goes back to the Multiplayer page.
bool ServersPage_Opened(AVPMENU_ELEMENT *rows, int room);

// A server's row was pressed (its MaxValue): the player picks it, for the next start. True when the row was one of the page's servers.
bool ServersPage_Pressed(int row);

// The page's words around its rows: which servers are shown, the lines not used, and the pick's answer. chosen is the menus' chosen row.
void ServersPage_DrawTop(int chosen);

// One of the page's rows, drawn where the page puts it (a server's in its screenful, the Back row above the help line), or not at all
void ServersPage_DrawRow(const AVPMENU_ELEMENT *row, int element, int brightness, int chosen);

#endif
