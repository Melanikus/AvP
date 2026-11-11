// The host page: where a host chooses the server their game is hosted on (beta.2). Written 2026-11-11 by Melanikus for the AvP port.
//
// WHY. servers.cfg can name many servers, and a player hosts for friends near them, on their friends' server. The owner, 2026-11-11:
// "Their friends will make servers and they will choose their friends' server", "Not 'least'" [ping], "No auto choice, they choose", and
// "Yes 'host' <ny - ping>, <eu - ping>, <we - ping> on host screen". So Start game and Co-op campaign open this page every time, with
// nothing chosen. It lists every line, each with its round trip, its games, their players and its relay's region, and only Enter on a line
// that can be hosted on chooses it.
//
// WHAT IT NEVER DOES. It never chooses for the player: the choice is cleared each time the page opens (RelayList_HostOn(-1)), the cursor
// starts on the page's own question, which chooses nothing, and the choice is never kept past the run. It never sorts by ping. Its rows
// only read relay_list's own answers (RelayList_LineNow); whatever a server or a host sent (a region, a count of players) is made safe or
// checked before it's drawn. The release check (tools/check-online-play.ps1, rule H) refuses a choice of line made anywhere but
// HostPage_PressedRow and a -dev test run's own (relay_auto.cpp).
//
// host_page_rules.h has the page's layout and rules, with nothing of the game's in them, for host_page_test.

#ifndef HOST_PAGE_H
#define HOST_PAGE_H

#include "menus.h"
#include "host_page_rules.h"

// The page has opened: the choice is cleared, every line is asked again (RelayList_FindGames), and the page's rows are written into rows,
// which has room for kHostPageRowRoom: the question, then a row for each line, then the end. False if there's no room, and then menus.cpp
// goes back to the Multiplayer page.
bool HostPage_Opened(AVPMENU_ELEMENT *rows, int room);

// Each frame of the page: when the find's games have come, each line's players are added up from them
void HostPage_Frame(void);

// A row was pressed (its MaxValue). True only when a line was chosen to host on (RelayList_HostOn): the menus then go on to where the host
// was going. Otherwise nothing is chosen, and the page says why under its rows.
bool HostPage_PressedRow(int row);

// The page's own words around its rows (the headings, which lines are shown, the answer to the last Enter), and one of its rows, drawn
// where the page puts it. chosenElement is the menus' chosen row: 0 is the question, 1 the first line.
void HostPage_DrawTop(int chosenElement);
void HostPage_DrawRow(const AVPMENU_ELEMENT *row, int brightness, int chosenElement);

// For a Hudson run (hoston=, hudson_plan.h): the row (its MaxValue) of the line whose servers.cfg name is exactly name, byte for byte, or
// -1; and whether that row can be chosen now. Neither chooses anything: the run presses the row as a player does.
int HostPage_RowNamed(const char *name);
bool HostPage_RowCanBeChosen(int row);

#endif
