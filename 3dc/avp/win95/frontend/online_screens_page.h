// The online-play screens, drawn with the game's own fonts and its menu glow, and the games page's rows kept steady while it refreshes.
// Written 2026-11-09 by Melanikus for the AvP port.
//
// online_screens.h has the sums and the fixed words, and its test checks them. This file is the part that needs the game: it draws them,
// and it reads the networking code's list of games found (SessionData). menus.cpp calls it, from the games page and the join's wait.
//
// The screens, by online_screens.h's short names: J1 looking for games, J2 the list, J3 joining through a relay, J4 and J5 asking the host
// and its answer, the host's H1, and the made-up run of every screen that -dev -onlinescreens shows for pictures. H2, the relay's news on
// the host's screen in play, is in that run only for now: the game itself says it in the log (the "net:" lines).

#ifndef ONLINE_SCREENS_PAGE_H
#define ONLINE_SCREENS_PAGE_H

#include "relay_list_text.h"

// ---- The games page: J1 while there's nothing to show, J2 once there is

// The page has just been opened (not set up again): the rows start empty, the search starts, and the relays are timed again
void OnlinePage_Opened(void);

// The relay server's list has answered with these games. The page keeps each one's hostMs, for its ping column. why is NULL for a good
// answer, or the list code's own fixed words for one that failed (RelayList_FoundGames), which the page shows until a good one comes.
void OnlinePage_RelayGamesCame(const RelayListGame *games, int count, const char *why);

// Each frame of the page, before OnlinePage_Frame: whether it's time to ask the LAN and the list again (every 5 s, and 12 times at most
// while nothing is found). menus.cpp does the asking, as it does when the page opens.
bool OnlinePage_TimeToAskAgain(void);

// Each frame of the page. listChanged says the networking code's list of games may have changed. selectedRow is the menu's selected row
// on the way in, and where the same game is on the way out. True when the rows have changed, so the menu's rows must be made again.
bool OnlinePage_Frame(bool listChanged, int *selectedRow);

// The rows the page shows, at most MAX_NO_OF_SESSIONS, and each one's name for the menu's own row (made safe already)
int OnlinePage_RowCount(void);
char *OnlinePage_RowName(int row);

// Where the game on a row is in the networking code's list right now, found by its relay code or its address, never by its row number:
// the list is emptied and filled again by each search. -1 when the game has gone, or the row isn't one.
int OnlinePage_SessionForRow(int row);

// Enter on the page while it has no games: after the search has given up, it starts again. True when the page has taken the key.
bool OnlinePage_EnterWithNoGames(void);

// A join failed, for these words (the networking code's own fixed words, or the page's), or was given up with Esc. The page shows a
// line saying so for a few seconds.
void OnlinePage_JoinFailed(const char *why);
void OnlinePage_JoinGivenUp(void);

// The page's drawing, each frame: the column headings, or J1's tail and words while there are no rows, and the failed join's line.
// Then each row, as bright as the menu has it (BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT for the selected one).
void OnlinePage_DrawTop(void);
void OnlinePage_DrawRow(int row, int brightness);

// ---- Joining through a relay: J3

// One whole frame of the joining screen, drawn from inside the join's wait (menus.cpp's KeepTheWindowAliveWhileJoining, every 50 ms).
// region is the game's relay region from SessionData, made safe here.
void OnlinePage_DrawJoining(const char *region);

// ---- Asking the host: J4 and J5 (the request to join: networking.h, Fifth's)

// The joiner is about to join the game on this row. The page remembers which game it was, so that when the join ends and the page opens
// again, the selection goes back to that game.
void OnlinePage_Joining(int row);

// The joining page, each frame: J4 while the request waits (the tail, its seconds and Esc), and "The host let you in" once the host has
// said yes and the game is on its way. False when there's no request, and the page shows its own row as before.
bool OnlinePage_DrawAsking(void);

// The joining page's connection has ended before the game was joined. True when that was the request's own end: the host said no, was
// answering someone else, didn't answer in time, or the player gave up with Esc while it waited. The games page then says which, and the
// menus go back to it.
bool OnlinePage_JoinEnded(void);

// ---- The host's side: H1

// The request waiting at this host, if there is one: its joiner's name made safe to draw, and how many seconds are left to answer it.
bool OnlineHost_AskWaiting(char *safeName, size_t size, int *secondsLeft);

// The host's answer, from the menu's row. An answer for a request that's gone does nothing (Net_AnswerJoinRequest).
void OnlineHost_Answer(bool yes);

// ---- In play, on the host and on a joiner

// Each frame of play, from the game's loop, once the HUD is drawn:
// - on the host, while a player waits to be let in, a line every 5 s says who asks and how to answer (H1);
// - on the host, the relay's news as it comes (H2): finding the nearest relay, on the list through which relay, no relay, the room gone;
// - on both, for as long as the game is played through a relay, a line kept at the top right, in the HUD's font: "Hosting through
//   virginia, 41 ms" or "Joined through virginia, 63 ms", its time refreshed from the measured round trip (the user: "I want the Hudsons
//   showing server name and ping").
void OnlinePlay_Frame(void);

// The relay's latest news for the co-op lobby page, where the host waits before play: true for 8 s after it came, with its words
bool OnlineHost_NewsLine(char *line, size_t size);

// ---- -dev -onlinescreens: every screen, with made-up games and no network, for pictures

// winmain.cpp turns it on, only with -dev
void OnlineDemo_TurnOn(void);
bool OnlineDemo_IsOn(void);

// The run itself, in frames of its own, from the main menus' start. F2 saves a picture of the screen (a mark, mark_key.h) and goes on to
// the next. Esc ends it, and the menus go on as usual. It runs once.
void OnlineDemo_Run(void);

#endif
