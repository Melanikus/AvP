// The Online access pages (server #24): asking the list's owner to let this PC play online. Written 2026-11-09 by Melanikus for the AvP
// port.
//
// ONLY WITH A SERVER. Modified 2026-11-10 by Melanikus for the AvP port (beta.2, PO's go): the pages were -dev only (the owner's ruling of
// 2026-11-09, "NOTHING that can be actually accessed by people with the release", for beta.1). Beta.2 plays online, so they're there for
// every player whose servers.cfg names a server, and they ask that server: the Multiplayer page's row is put there only then (menu_data.cpp,
// MenuData_ShowOnlineAccessRow), and the pages send anyone else straight back. -dev decides nothing here. Before anything goes to the
// server, the player says yes to it (server_yes_page.h; the row's press asks), and relay_list.cpp sends a request only to the server a
// servers.cfg line names, after that yes. access_page_test checks each.
//
// WHAT THE PLAYER SEES. The name to send (the profile's, which they may change), and above it what Request access does, in plain words.
// Nothing is sent until they press Request access. Then their access code, to give the owner themselves (Copy access code puts it on the
// clipboard), and what the server says: waiting, approved, not approved. Make a new identity asks first, on a page of its own.
//
// access_page_rules.h has the page's decisions, game-free, and relay_list_text.h what's sent and read back. This is the part that needs the
// game: the rows' words, the drawing, the clipboard, and this run's identity (networking.h).

#ifndef ACCESS_PAGE_H
#define ACCESS_PAGE_H

#include "menus.h"
#include "relay_list_text.h"

// What each AVPMENU_ELEMENT_ONLINE_ACCESS row does: its MaxValue
enum
{
	kAccessRowRequest,   // Request access, which says Check again once a request has been sent
	kAccessRowCopy,      // Copy access code
	kAccessRowNewNo,     // the question's No, which comes first, so an Enter pressed by chance changes nothing
	kAccessRowNewYes     // and its Yes
};

// The name row's text: the profile's name to start with (RelayAccess_NameFromProfile), which the player may change
extern char AccessPageName[kRelayAccessNameMostBytes + 1];

// Puts the Online access row on the Multiplayer page, or takes it off: on only when aServerIsNamed (servers.cfg names a server,
// Relay_ServerName). menu_data.cpp has the page, and menus.cpp asks each time the Multiplayer page opens.
void MenuData_ShowOnlineAccessRow(bool aServerIsNamed);

// The Online access page has opened, with these rows. False when servers.cfg names no server, and then menus.cpp goes back to the
// Multiplayer page.
bool AccessPage_Opened(AVPMENU_ELEMENT *rows);

// The question before a new identity has opened. False without -dev, as above.
bool AccessPage_QuestionOpened(void);

// Each frame of either page: what the list has said, and the rows' words. The Request access row says Check again, with its own help,
// once a request has been sent. Gives back the row the page chooses for the player, Copy access code (access_page_rules.h,
// AccessPage_ChoosesCopy), or -1 to leave the player's row as it is.
int AccessPage_Frame(AVPMENU_ELEMENT *rows);

// A row was pressed. True when the menus should go to another page, and then page says which.
bool AccessPage_Pressed(int row, enum AVPMENU_ID *page);

// The drawing above the rows, and the lines under them: what Request access does before anything is sent, then the code and what the
// server said. menuHeight is the menus' MenuHeight, and rows how many rows the page has: they're drawn kAccessPageRowsApart further
// apart than the menus' own. Gives back where the first row goes (access_page_rules.h, "Where the page's parts go"; modified 2026-11-10
// by Melanikus for the AvP port).
int AccessPage_DrawTop(int menuHeight, int rows);

// The question's words, above its two rows. Gives back where the first row goes, as AccessPage_DrawTop does.
int AccessPage_DrawQuestion(int menuHeight, int rows);

// Whether a call to the list is on its way, its answer not come yet: for a Hudson run pressing the page's rows (hudson_plan.h's access=),
// which pictures the page once the answer is on it. Added 2026-11-09 by Melanikus for the AvP port.
bool AccessPage_CallOnItsWay(void);

#endif
