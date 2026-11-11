// The Online page: every server servers.cfg names, each on its own row, and everything the player does about one there: its yes, its
// access request, and where it stands. Written 2026-11-11 by Melanikus for the AvP port (beta.2), in place of the Servers page and the
// one-server Online access page.
//
// WHY ONE PAGE. The owner, 2026-11-11: "They don't PICK a server, all authorized ones show up", "UNIFY THE UI", "SAME CODE, FOR 2 OR MORE
// SERVERS". There's no server "in use" and no pick: Find games shows the games of every server that has approved this PC, and a host
// chooses where to host on the Multiplayer page. So this page is where the player meets each server once: says yes to it, asks its owner
// to let this PC in, and sees whether they have. Every row is drawn and pressed by the same code (online_page_rules.h), whichever line it
// is, the lines the game ships with too: a server is added by adding a line, and nothing else.
//
// WHAT ENTER ON A ROW DOES (OnlineLines_Pressed, online_page_rules.h): a line without the player's yes opens the yes page for that line
// (server_yes_page.h), which comes back here; a line with it sends that line's server an access request (relay_list.h,
// RelayList_AskForAccessOn), at most once a minute for each line; nothing while one is on its way, or once its owner has refused this PC.
// Nothing goes to any server just from opening the page, except to lines this run has already asked: those are asked again, as a player
// coming back to the page wants to know (PO's ruling of 2026-11-09, kept per line).
//
// WHAT IS ONCE ON THE PAGE, NEVER ON A ROW. This PC has one identity, so one access code, the same for every server (R-SRV-P1): it's
// shown once at the top, Copy access code is one row, and Make a new identity is one row, with the words "A new identity starts over on
// every server: each one has to approve you again." under it (R-SRV-P4). A new identity asks first, on a page of its own.
//
// The page is on the Multiplayer page always, so that even a servers.cfg the game can't use at all can be seen, with the game's own words
// for why (as the Servers page did).

#ifndef ONLINE_PAGE_H
#define ONLINE_PAGE_H

#include "menus.h"
#include "online_page_rules.h"
#include "relay_list_text.h"

// The rows of the question before a new identity: No comes first, so an Enter pressed by chance keeps the identity (their MaxValue)
enum
{
	kOnlineNewIdentityNo = 10,
	kOnlineNewIdentityYes = 11
};

// The room the page needs for its rows: one for each line, its own four, and the end
int const kOnlinePageRowRoom = kOnlinePageMostLines + kOnlinePageOwnRows + 1;

// The name sent with an access request: the profile's to start with (RelayAccess_NameFromProfile), which the player may change
extern char OnlinePageName[kRelayAccessNameMostBytes + 1];

// The page has opened: its rows are written into rows, which has room for kOnlinePageRowRoom. False if there's no room, and then
// menus.cpp goes back to the Multiplayer page.
bool OnlineLines_Opened(AVPMENU_ELEMENT *rows, int room);

// The Multiplayer page has opened: every line with the player's yes is asked for this PC's access, as its row's Enter would ask it, at
// most once a minute for each line (approval is known from this run's answers alone). Never a line without its yes. Added 2026-11-11.
void OnlineLines_AskTheAgreedLines(void);

// The question before a new identity has opened
bool OnlineLines_QuestionOpened(void);

// Each frame of the page: the servers' answers that have come, each to its own row. Nothing is sent from here.
void OnlineLines_Frame(void);

// A row was pressed (its MaxValue). True when the menus should go to another page, and then page says which.
bool OnlineLines_Pressed(int row, enum AVPMENU_ID *page);

// The page's words around its rows: the code, the answer to the last press, which lines are shown, and the lines not used. chosen is the
// menus' chosen row.
void OnlineLines_DrawTop(int chosen);

// A line's row, drawn in its place in the screenful shown: true when the row is a line's (drawn, or not shown), false for one of the
// page's own rows, which the menus draw at OnlineLines_OwnRowTopOf
bool OnlineLines_DrawLineRow(const AVPMENU_ELEMENT *row, int brightness, int chosen);

// Where one of the page's own rows goes
int OnlineLines_OwnRowTopOf(const AVPMENU_ELEMENT *row);

// The question's words, above its two rows. Gives back where the first row goes.
int OnlineLines_DrawQuestion(int menuHeight);

// For a Hudson run pressing the rows (hudson_plan.h's access=): how many lines there are, and whether a line's request is on its way
int OnlineLines_LineCount(void);
bool OnlineLines_AskingOn(int place);

// Whether a line still needs the player's yes (for a Hudson run's yespage=)
bool OnlineLines_NeedsTheYes(int place);

#endif
