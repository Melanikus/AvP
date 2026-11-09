// The online-play screens' own sums and words: the spinning alien tail, the names from the network made safe to draw, the rows of the
// games page kept steady while it refreshes, and the fixed words every screen says. Written 2026-11-09 by Melanikus for the AvP port.
//
// Nothing in here draws, reads a file or touches the network. It only works things out, so tests/online_screens_test.cpp can check all of it
// without the game. online_screens_page.cpp is the part that draws, with the game's own fonts and pictures.
//
// The screens, by the short names the code and its comments use for them (docs/online-play.md says what a player sees on each):
//   J1 looking for games, J2 the list of games, J3 joining through a relay, J4 asking the host, J5 the host's answer: the joiner's.
//   H1 someone asks to join, H2 the relay's news: the host's.
//
// Everything that comes from the network (a game's name, a player's name) is hostile until it's been through OnlineScreens_SafeName.

#ifndef ONLINE_SCREENS_H
#define ONLINE_SCREENS_H

#include <stddef.h>
#include <stdint.h>

#include "relay_list_text.h"

// ---- The spinning tail

enum
{
	kOnlineTailPieces = 16,        // piece 0 is the blade at the tip, and 1 to 15 are the bones behind it, from the tip back
	kOnlineTailTurnMs = 1500,      // one whole turn takes this long, by the clock, so it turns as fast at any frame rate
	kOnlineTailSmallest = 4,       // the tail's size, in the menus' 640 by 480: a size outside these is taken as the nearest of them
	kOnlineTailBiggest = 200,
	kOnlineTailFarthest = 10000    // and its middle is kept this close to the screen, so no sum can overflow
};

// One corner, where the screen's pixel grid has it. Whole numbers, because that's what the game's HUD quads take (VertexTag).
struct OnlineTailCorner
{
	int x, y;
};

// One piece of the tail: four corners, in order round its edge, so the two triangles the game splits a quad into (corners 0, 1 and 2, and
// 2, 1 and 3, as the HUD's quad hands them over) cover it whole. And how bright it is, 0 to 255: the tip is brightest, the base dimmest.
struct OnlineTailPiece
{
	OnlineTailCorner corner[4];
	int brightness;
};

// The tail as it is ms milliseconds into its turning, curled round (centreX, centreY), size pixels out from it. It's an alien's tail curled
// into a ring, the blade leading and the bones trailing behind it round about five sixths of the circle, so a turning ring with a gap
// reads as "waiting", the way a loading circle does. ms can be any value: only where it is in the turn matters.
void OnlineTail_Shape(uint32_t ms, int centreX, int centreY, int size, OnlineTailPiece pieces[kOnlineTailPieces]);

// ---- Names from the network

// Copies a name from the network into to (which holds toSize bytes), reading at most mostBytes of from, and stopping at its first 0.
// Only the letters the game's fonts have are kept, the printable ones from the space (32) to the tilde (126); every other byte becomes a
// question mark, so a name can't draw a letter from outside the font's picture, or move the text with a control character. to always ends
// with a 0. A toSize of 0 writes nothing.
void OnlineScreens_SafeName(const char *from, size_t mostBytes, char *to, size_t toSize);

// Shortens text, where it is, until it's no wider than mostWidth, by the game's own letter widths (widthOf). Text that had to be shortened
// ends with two dots, so nobody takes a shortened name for the whole one. widthOf gets each letter as a number from 0 to 255.
void OnlineScreens_FitToWidth(char *text, int mostWidth, int (*widthOf)(int letter));

// A game's name as the LAN's search gives it is "Name (2/4)", or "Name (V 1.13)" for another version. This splits it into the name and the
// part in brackets, without the brackets. A name with no brackets at the end has an empty second part. Both are made safe as above.
void OnlineScreens_SplitName(const char *sessionName, size_t mostBytes, char *name, size_t nameSize, char *players, size_t playersSize);

// ---- The rows of the games page

// What tells one game from another across refreshes: its relay code, for a game from the list, or its address and port, for one on this
// network. The address is only compared here, never shown or written down.
struct OnlineGameKey
{
	bool throughRelay;
	char code[kRelayCodeLetters];   // the 8 letters, with no 0 after them
	uint32_t host;
	uint16_t port;
};

bool OnlineScreens_SameGame(const OnlineGameKey &a, const OnlineGameKey &b);

// One row of the page, as it's drawn
enum { kOnlineRowLetters = 40 };

struct OnlineRow
{
	OnlineGameKey key;
	int session;                            // its place in the networking code's list (SessionData), or -1 for a game that's gone
	bool gone;                              // it was there on the last refresh, and isn't now: shown once more, and can't be joined
	bool canJoin;                           // not full, not another version, not gone
	char name[kOnlineRowLetters];
	char level[kOnlineRowLetters];
	char players[16];
	char region[kRelayListRegionLetters + 1];
	char ping[16];
};

enum { kOnlineMostRows = 16 };

// The page's rows after a refresh: every game now, in the order the networking code has them, then each game that was there before and
// isn't now, marked gone. A game that was gone already doesn't come back, so a gone game is shown for one refresh only. At most mostOut
// rows are written, the games there now first. Gives back how many there are.
int OnlineScreens_MergeRows(const OnlineRow *before, int beforeCount, const OnlineRow *now, int nowCount, OnlineRow *out, int mostOut);

// Where the selected game is after a refresh: the row with the same game, or, when it's gone, the same row number as before, kept inside
// the rows there are. -1 when there are no rows.
int OnlineScreens_SelectAgain(const OnlineGameKey &selected, int selectedRow, const OnlineRow *rows, int count);

// ---- Looking for games (J1), and keeping the list fresh (J2)

enum
{
	kOnlineSearchEveryMs = 5000,   // the list is asked again this often: it allows one ask a second from an address (PROTOCOL.md section 5)
	kOnlineSearchMostTries = 12    // and while nothing is found, only this many times, a minute, before the page stops and says so
};

enum OnlineSearchNews
{
	kOnlineSearchNothing,          // nothing to do this frame
	kOnlineSearchAskAgain,         // time to ask the LAN and the list again
	kOnlineSearchGivenUp           // nothing was found in all the tries: the page says so, and waits for Enter to start again
};

struct OnlineSearch
{
	int tries;                     // asks so far, counting the first
	uint32_t nextAskAt;            // the clock (GetTickCount) at the next ask
	bool givenUp;
};

// The first ask, when the page opens, or when Enter starts the search again
void OnlineSearch_Start(OnlineSearch *search, uint32_t now);

// Each frame of the page. gamesShown says whether the page has any game to show. While it has, the list is asked again every 5 s for as
// long as the page is open; while it hasn't, only up to the most tries. The clock wrapping round, after 49 days, is handled.
OnlineSearchNews OnlineSearch_Frame(OnlineSearch *search, uint32_t now, bool gamesShown);

// The J1 line for the search as it is, in words of size characters, always ended with a 0. stage says what's being asked first: 0 this
// network, 1 the list of games.
void OnlineSearch_Words(const OnlineSearch *search, uint32_t now, int stage, char *words, size_t size);

// ---- The ping column

// "about N ms" for a game through a relay: the joiner's own time to that game's relay, and the host's (hostMs, 0 when the host didn't
// measure it), because every packet goes host, relay, joiner (PROTOCOL.md, "How the game uses it"). "?" while the joiner's time isn't
// known yet (joinerMs below 0). "-" for a game on this network. Times above 9999 ms are taken as 9999, as the list caps them.
void OnlineScreens_PingWords(bool throughRelay, int joinerMs, int hostMs, char *words, size_t size);

// ---- Joining, and being asked (J3, J4, J5, H1)

enum { kOnlineAnswerMs = 30000 };  // a request to join that the host hasn't answered in 30 s counts as no, on both sides

// How many whole seconds are left of a wait that began at startedAt and lasts mostMs: 0 once it's over. The clock wrapping is handled.
int OnlineScreens_SecondsLeft(uint32_t startedAt, uint32_t now, uint32_t mostMs);

// The words of each screen, fixed, in the game's own voice. Nothing from the network is ever part of these: a name goes in only through
// the functions below, after OnlineScreens_SafeName.
enum OnlineWords
{
	kOnlineWordsLookingHere,          // J1: the first ask, of this network
	kOnlineWordsLookingList,          //     and of the list of games
	kOnlineWordsNothingYet,           //     the start of the line between asks (OnlineSearch_Words adds the rest)
	kOnlineWordsNothingFound,         //     after all the tries
	kOnlineWordsEscBack,              //     and the line under it
	kOnlineWordsColumnGame,           // J2's column headings
	kOnlineWordsColumnLevel,
	kOnlineWordsColumnPlayers,
	kOnlineWordsColumnRegion,
	kOnlineWordsColumnPing,
	kOnlineWordsGone,                 //     in the players column of a game that's gone
	kOnlineWordsFull,                 //     and of a full one
	kOnlineWordsThisNetwork,          //     in the region column of a game on this network
	kOnlineWordsReachingRelay,        // J3
	kOnlineWordsRelayIntroducing,
	kOnlineWordsEscGivesUp,
	kOnlineWordsGaveUp,
	kOnlineWordsAsking,               // J4
	kOnlineWordsLetIn,                // J5
	kOnlineWordsSaidNo,
	kOnlineWordsNoAnswer,
	kOnlineWordsBackToList,
	kOnlineWordsCouldNotJoin,         //     the start of a failed join's line, before the networking code's own words
	kOnlineWordsGameHasGone,          //     why a game that's gone can't be joined
	kOnlineWordsListFailed,           // J1 and J2: the start of the line when the list of games didn't answer, before the list code's words
	kOnlineWordsHostBusy,             // J5: the host was answering someone else
	kOnlineWordsEscToAnswer,          // H1: in play, after the asking line, how the host gets to the answer
	kOnlineWordsHostingThrough,       // the line kept up in play: a host's start, "HOST, Relay:"
	kOnlineWordsJoinedThrough,        //     and a joiner's, "CLIENT, Relay:"
	kOnlineWordsARelay,               //     in place of a region that isn't known
	kOnlineWordsRelayFinding,         // H2
	kOnlineWordsRelayListed,
	kOnlineWordsRelayNone,
	kOnlineWordsRelayGone,
	kOnlineWordsCount
};

// The words, or "" for a number that isn't one of them
const char *OnlineScreens_Words(OnlineWords which);

// One of the words above with the seconds left after it, as "Asking the host to let you in (25 s)". words always ends with a 0.
void OnlineScreens_WordsWithSeconds(OnlineWords which, int secondsLeft, char *words, size_t size);

// "NAME asks to join (25 s)", "Let NAME in" and "Say no to NAME", with a name that has already been made safe. A name longer than a row's
// is cut at kOnlineRowLetters - 1 letters. words always ends with a 0.
void OnlineScreens_AskingWords(const char *safeName, int secondsLeft, char *words, size_t size);
void OnlineScreens_LetInWords(const char *safeName, char *words, size_t size);
void OnlineScreens_SayNoWords(const char *safeName, char *words, size_t size);

// H2's two lines with more than fixed words: "Your game is on the list, through saopaulo, 41 ms" (the relay's region, made safe already, and
// the host's round trip to it, taken as 0 to 9999; no region gives the line without it), and "Your game's room on the relay has gone: the
// relay didn't answer" (the networking code's own words for why, made safe already; none gives the line without them). words always
// ends with a 0.
void OnlineScreens_ListedWords(const char *safeRegion, int ms, char *words, size_t size);
void OnlineScreens_RoomGoneWords(const char *safeWhy, char *words, size_t size);

// The line kept on the screen for as long as a game is played through a relay, in the owner's words (2026-11-09), each time saying what
// it measures:
//   the host:   "HOST, Relay: nyc3, 145 ms (round trip, host to server)", ms being its keep's time to the relay server and back;
//   a joiner:   "CLIENT, Relay: nyc3, 290 ms (round trip, client to host through server)", ms being ENet's time to the host and back,
//               through the server, then ", 146 ms round trip to server" when serverMs isn't below 0: its own time to the server and
//               back, as the games page measured it before the join.
// safeRegion is the name people see for the server ("NY Mel's Server", Relay_ServerShownName) when it has one, otherwise the relay's
// region. It comes made safe already, and is cut to a shown name's 32 letters; with none it says "a relay". ms below 0 (nothing measured
// yet) leaves the times out, and a time above 9999 is 9999. words always ends with a 0, cut short if it has to be.
// Modified 2026-11-09 by Melanikus for the AvP port: the owner's words, serverMs, and the server's shown name.
void OnlineScreens_PlayLineWords(bool isHost, const char *safeRegion, int ms, int serverMs, char *words, size_t size);

#endif
