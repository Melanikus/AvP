// The online-play screens, drawn. Written 2026-11-09 by Melanikus for the AvP port. online_screens_page.h says what each part is for.
//
// Everything here is in the menus' own 640 by 480 (the drawing code scales it to the screen), in the game's small font, with the tail
// lit by the menus' own highlight glow (Menus/glowy_middle.RIM), drawn the way the HUD draws the motion tracker's turning disc.
//
// Modified 2026-11-09 by Melanikus for the AvP port: the check code on the joiner's asking screen and for the host's waiting request
// (CRYPTO.md section 8). (The words for a host that's another v2 test build, JBLD, went on 2026-11-10 with the v2 draft's same-commit
// rule, #175 PR 3a.)

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "menus.h"
#include "menu_gfx.hpp"
#include "networking.h"
#include "relay_list.h"
#include "language.h"
#include "renderStates.h"
#include "d3d_hud.h"
#include "hud_layout.h"
#include "d3d_render.h"
#include "mark_key.h"
#include "logString.h"
#include "online_screens.h"
#include "online_screens_page.h"
#include "level_lists.h"
#include "coop_menu.h"

#include <stdio.h>
#include <string.h>

extern char AAFontWidths[256];
extern unsigned char DebouncedKeyboardInput[];
extern bool bRunning;
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;   // the screen's size in play, for the line kept at its top right
extern void DrawMainMenusBackdrop(void);

// The page shows no more rows than the menu has rows for (menu_data.cpp's AvPMenu_MultiplayerSelectSession)
static const int kMostShown = MAX_NO_OF_SESSIONS;
static_assert(MAX_NO_OF_SESSIONS <= kOnlineMostRows, "the page's rows must fit online_screens.h's");

// How long after an ask the page waits before it marks a game that didn't answer as gone: the LAN answers within a second, and the list's
// games stay on it from its last answer until a new one comes
static const uint32_t kSettleMs = 1500;

// How long a failed join's line stays up
static const uint32_t kFailedLineMs = 8000;

// Where things go on the page, in the menus' 640 by 480: the Search box and the Refresh row under the title, then the table
static const int kSearchTop = 100;
static const int kRefreshTop = 130;
static const int kHeadingsTop = 170;
static const int kFirstRowTop = 190;
static const int kRowStep = 18;
static const int kTailX = 320, kTailY = 215, kTailSize = 34;
static const int kWordsTop = 270;

struct Column
{
	int x;
	int width;
	OnlineWords heading;
};

static const Column kColumns[5] =
{
	{ 35, 195, kOnlineWordsColumnGame },
	{ 235, 125, kOnlineWordsColumnLevel },
	{ 365, 60, kOnlineWordsColumnPlayers },
	{ 430, 100, kOnlineWordsColumnRegion },
	{ 535, 75, kOnlineWordsColumnPing }
};

// ---- The page's state

static OnlineRow shownRows[kOnlineMostRows];
static int shownCount = 0;

// What the page is showing (the user's design): the tail while the open sessions are fetched, the count once connected, a failure that
// stays until Enter tries again, or the list itself, with its Search box and Refresh row
enum PageState
{
	kPageFetching,
	kPageConnected,
	kPageCantConnect,
	kPageCantFetch,
	kPageList
};

static PageState pageState = kPageFetching;
static uint32_t pageStateSince = 0;
static bool askNow = false;
static bool listAnswered = false;
static bool listCantConnect = false;
static int connectedSessions = 0;

// "Connected: N sessions open" stays this long before the list comes
static const uint32_t kConnectedShownMs = 1500;

// The Search box's own buffer, which the menus' text field types into: 20 letters and its 0. What it holds is made safe before it's used.
static char searchBox[kOnlineSearchLetters + 1];

// The rows of shownRows the search lets through, in their order. On the list, the menu's rows are the Search box, the Refresh row, then these.
static int visibleRows[kOnlineMostRows];
static int visibleCount = 0;
static const int kFirstGameElement = 2;

static void TwoLines(const char *first, const char *second, bool tail);

// Each relay game's hostMs, by its code, from the list's last answer
struct RelayTime
{
	char code[kRelayCodeLetters];
	int hostMs;
};

static RelayTime relayTimes[kRelayListMostGames];
static int relayTimesKept = 0;

static char failedLine[96];
static uint32_t failedAt = 0;
static bool failedShown = false;

// The list's last answer failed, and why, in the list code's own words: shown, dim, under the failure
static char listFailedLine[96];
static bool listFailed = false;

// The game last joined, to select again when the page opens after the join ends
static OnlineGameKey joinedKey;
static bool joinedKeyKept = false;

// The game the selection was last on while the list was shown, and its row among the games shown then. To press Refresh the player moves
// the selection up onto the Refresh row, so this is what it goes back to when the list comes back (OnlineScreens_SelectAgain). A page
// just opened has none, and starts on its first game.
// Added 2026-11-09 by Melanikus for the AvP port.
static OnlineGameKey lastGame;
static int lastGameRow = 0;
static bool lastGameKept = false;

static uint32_t Now(void)
{
	return (uint32_t)GetTickCount();
}

// Whether the clock has reached a moment, the clock's wrap allowed for (as online_screens.cpp does)
static bool HasCome(uint32_t now, uint32_t moment)
{
	return (uint32_t)(now - moment) < 0x80000000u;
}

// Copies words into a buffer, always ending it with a 0
static void Put(char *to, size_t size, const char *from)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(to, size - 1, "%s", from ? from : "");
	to[size - 1] = '\0';
}

// Whether this game has a list of games to ask: until the relay server's name is built in, only a -dev tester's settings give one
// (Relay_ServerName). Without one, the page is about this network alone, and says nothing of a list.
static bool ThereIsAList(void)
{
	return Relay_ServerName()[0] != '\0';
}

// The small font's width for a letter, never more than its widest (menu_gfx.cpp's SmallLetterWidth, which is that file's own)
static int SmallWidth(int letter)
{
	int const width = AAFontWidths[letter & 255];
	return width < 0 ? 0 : (width > 17 ? 17 : width);
}

// ---- Making the rows

static OnlineGameKey KeyOf(const SESSION_DESC &session)
{
	OnlineGameKey key;
	memset(&key, 0, sizeof(key));
	key.throughRelay = session.throughRelay;

	if (session.throughRelay)
	{
		memcpy(key.code, session.relayCode, sizeof(key.code));
	}
	else
	{
		key.host = session.host;
		key.port = session.port;
	}

	return key;
}

static int HostMsFor(const char code[kRelayCodeLetters])
{
	for (int i = 0; i < relayTimesKept; i++)
	{
		if (memcmp(relayTimes[i].code, code, kRelayCodeLetters) == 0)
		{
			return relayTimes[i].hostMs;
		}
	}

	return 0;
}

// The players column: "2/4", "full", or another version's "V 1.13", which the networking code puts in the name's brackets
static void PlayersWords(const SESSION_DESC &session, const char *bracket, char *words, size_t size)
{
	if (bracket[0] == 'V')
	{
		Put(words, size, bracket);
		return;
	}

	int const players = session.players;
	int const most = session.maxPlayers;

	// The host said these, so they're checked before they're believed. Numbers that can't be right show what the name's brackets said.
	if (most < 1 || most > kRelayMostPlayers || players > most)
	{
		Put(words, size, bracket);
		return;
	}

	if (players >= most)
	{
		Put(words, size, OnlineScreens_Words(kOnlineWordsFull));
		return;
	}

	_snprintf(words, size - 1, "%d/%d", players, most);
	words[size - 1] = '\0';
}

// The rows as the networking code's list has them now
static int LiveRows(OnlineRow *rows, int most)
{
	uint32_t const found = NumberOfSessionsFound < (uint32_t)MAX_NO_OF_SESSIONS ? NumberOfSessionsFound : (uint32_t)MAX_NO_OF_SESSIONS;
	int count = 0;

	for (uint32_t i = 0; i < found && count < most; i++)
	{
		SESSION_DESC const &session = SessionData[i];
		OnlineRow &row = rows[count++];
		char bracket[16];

		memset(&row, 0, sizeof(row));
		row.key = KeyOf(session);
		row.session = (int)i;
		row.canJoin = session.AllowedToJoin;

		OnlineScreens_SplitName(session.Name, sizeof(session.Name), row.name, sizeof(row.name), bracket, sizeof(bracket));

		// Modified 2026-11-10 by Melanikus for the AvP port (PO's ruling): a co-op game on one of the 32 campaign places is named by its
		// place, in the game's own words, never by the name its host sent. An honest host sends no name for a campaign level (mp_config.cpp's
		// GetCustomMultiplayerLevelName), so the column said "-", and a dishonest one could send any. Every other game keeps the host's name,
		// made safe.
		int campaign = kCampaignNone;
		int campaignLevel = 0;
		bool const onACampaign = session.gameStyle == NGT_Coop &&
			LevelList_CampaignAt(session.levelIndex, MAX_NO_OF_COOPERATIVE_EPISODES, &campaign, &campaignLevel);
		OnlineScreens_LevelWords(onACampaign ? CoopMenu_LevelName(campaign, campaignLevel) : NULL, session.levelName, sizeof(session.levelName),
			row.level, sizeof(row.level));

		PlayersWords(session, bracket, row.players, sizeof(row.players));

		if (session.throughRelay)
		{
			OnlineScreens_SafeName(session.relayRegion, sizeof(session.relayRegion), row.region, sizeof(row.region));
			// "..." while the page is still timing that relay (added 2026-11-10 by Melanikus for the AvP port)
			int const joinerMs = Net_JoinerMsToRelay(row.region);
			OnlineScreens_PingWords(true, joinerMs < 0 && Net_RelayBeingTimed(row.region) ? kOnlineJoinerMsMeasuring : joinerMs,
				HostMsFor(session.relayCode), row.ping, sizeof(row.ping));
		}
		else
		{
			Put(row.region, sizeof(row.region), OnlineScreens_Words(kOnlineWordsThisNetwork));
			OnlineScreens_PingWords(false, -1, 0, row.ping, sizeof(row.ping));
		}
	}

	return count;
}

static bool SameRow(const OnlineRow &a, const OnlineRow &b)
{
	return OnlineScreens_SameGame(a.key, b.key) && a.gone == b.gone && a.canJoin == b.canJoin && strcmp(a.name, b.name) == 0 &&
		strcmp(a.level, b.level) == 0 && strcmp(a.players, b.players) == 0 && strcmp(a.region, b.region) == 0 && strcmp(a.ping, b.ping) == 0;
}

// Between asks: each game there now is brought up to date where it's shown, and a new one is added at the end. Nothing is marked gone
// here, because a new search empties the list for a moment.
static void UpdateAndAdd(const OnlineRow *live, int liveCount)
{
	for (int i = 0; i < liveCount; i++)
	{
		bool shown = false;

		for (int j = 0; j < shownCount && !shown; j++)
		{
			if (OnlineScreens_SameGame(live[i].key, shownRows[j].key))
			{
				shownRows[j] = live[i];
				shown = true;
			}
		}

		if (!shown && shownCount < kMostShown)
		{
			shownRows[shownCount++] = live[i];
		}
	}
}

// ---- The games page

// The server's name, as this game dialled it: the list's host name from the game's own settings (Relay_ServerName), or "this network"
// when there's no list. Never a name a server sent (AVP Tao 3).
static void ServerName(char *name, size_t size)
{
	if (ThereIsAList())
	{
		OnlineScreens_SafeName(Relay_ServerName(), kRelayListHostLetters, name, size);
	}
	else
	{
		Put(name, size, OnlineScreens_Words(kOnlineWordsThisNetwork));
	}
}

static void StartFetching(void)
{
	pageState = kPageFetching;
	pageStateSince = Now();
	listAnswered = false;
	listFailed = false;
	listCantConnect = false;
}

void OnlinePage_Opened(void)
{
	// A failed join's line stays: the menus come back here after a join that ended, and the line says why. It goes by itself in a few
	// seconds. So does the game last joined, which the selection goes back to once it's found again.
	shownCount = 0;
	visibleCount = 0;
	askNow = false;
	searchBox[0] = '\0';
	lastGameKept = false;
	StartFetching();

	// The server's relay is timed afresh each time the page opens, for the ping column (networking.h; added 2026-11-10 by Melanikus for the
	// AvP port)
	Net_StopTimingTheRelay();
}

void OnlinePage_RelayGamesCame(const RelayListGame *games, int count, const char *why)
{
	listAnswered = true;
	listFailed = why != NULL;

	// Which failure: no connection to the list's server, or one that didn't give its games. Told by the list code itself, from its own words.
	listCantConnect = why != NULL && RelayList_WhyIsNoConnection(why);

	// The list code's own fixed words for why, made safe all the same, as anything drawn here is
	OnlineScreens_SafeName(why ? why : "", sizeof(listFailedLine), listFailedLine, sizeof(listFailedLine));

	relayTimesKept = 0;

	for (int i = 0; games && i < count && i < kRelayListMostGames; i++)
	{
		memcpy(relayTimes[relayTimesKept].code, games[i].code, kRelayCodeLetters);
		relayTimes[relayTimesKept].hostMs = games[i].hostMs;
		relayTimesKept++;
	}
}

bool OnlinePage_TimeToAskAgain(void)
{
	if (!askNow)
	{
		return false;
	}

	askNow = false;
	StartFetching();
	return true;
}

void OnlinePage_FindAgain(void)
{
	// The Refresh row on the list, or Enter on a failure. Not while a fetch is going: one at a time.
	if (pageState == kPageList || pageState == kPageCantConnect || pageState == kPageCantFetch)
	{
		askNow = true;
	}
}

bool OnlinePage_ShowsTheList(void)
{
	return pageState == kPageList;
}

char *OnlinePage_SearchBox(void)
{
	return searchBox;
}

int OnlinePage_SearchLetters(void)
{
	return kOnlineSearchLetters;
}

// The rows the Search box lets through, in their order
static void Filter(void)
{
	char search[kOnlineSearchLetters + 1];
	OnlineScreens_SearchText(searchBox, sizeof(searchBox), search, sizeof(search));

	visibleCount = 0;

	for (int row = 0; row < shownCount && visibleCount < kOnlineMostRows; row++)
	{
		if (OnlineScreens_NameMatches(shownRows[row].name, search))
		{
			visibleRows[visibleCount++] = row;
		}
	}
}

static int VisibleIndexOf(const OnlineGameKey &key)
{
	for (int i = 0; i < visibleCount; i++)
	{
		if (OnlineScreens_SameGame(shownRows[visibleRows[i]].key, key))
		{
			return i;
		}
	}

	return -1;
}

bool OnlinePage_Frame(bool listChanged, int *selectedElement)
{
	(void)listChanged;   // the rows are read from the list every frame: there are at most ten, and that keeps them right whatever changed

	// The page's own round trip to the server's relay, for the ping column. It starts by itself once the list names the relay, which is
	// only after the player's yes to its line (networking.h; added 2026-11-10 by Melanikus for the AvP port).
	Net_TimeTheRelayForThePage();

	PageState const stateBefore = pageState;
	OnlineRow before[kOnlineMostRows];
	int const beforeCount = shownCount;
	memcpy(before, shownRows, sizeof(OnlineRow) * (size_t)shownCount);

	OnlineRow visibleBefore[kOnlineMostRows];
	int const visibleCountBefore = visibleCount;

	for (int i = 0; i < visibleCount; i++)
	{
		visibleBefore[i] = shownRows[visibleRows[i]];
	}

	// The game the selection is on, when it's on one
	int const oldElement = *selectedElement;
	OnlineGameKey selected;
	memset(&selected, 0, sizeof(selected));
	bool const onAGame = stateBefore == kPageList && oldElement >= kFirstGameElement && oldElement - kFirstGameElement < visibleCount;

	if (onAGame)
	{
		selected = shownRows[visibleRows[oldElement - kFirstGameElement]].key;

		// Kept for when the player goes up to Refresh, and the list comes back
		lastGame = selected;
		lastGameRow = oldElement - kFirstGameElement;
		lastGameKept = true;
	}

	OnlineRow live[kOnlineMostRows];
	int const liveCount = LiveRows(live, kMostShown);
	uint32_t const now = Now();

	switch (pageState)
	{
		case kPageFetching:
			UpdateAndAdd(live, liveCount);

			// This network's answers have had their time, and the list has answered (when there's a list)
			if (HasCome(now, pageStateSince + kSettleMs) && (!ThereIsAList() || listAnswered))
			{
				if (ThereIsAList() && listFailed)
				{
					pageState = listCantConnect ? kPageCantConnect : kPageCantFetch;
				}
				else
				{
					// A game that hasn't answered is marked gone, for once
					shownCount = OnlineScreens_MergeRows(before, beforeCount, live, liveCount, shownRows, kMostShown);
					connectedSessions = 0;

					for (int row = 0; row < shownCount; row++)
					{
						connectedSessions += shownRows[row].gone ? 0 : 1;
					}

					pageState = kPageConnected;
				}

				pageStateSince = now;
			}
			break;

		case kPageConnected:
			UpdateAndAdd(live, liveCount);

			if (HasCome(now, pageStateSince + kConnectedShownMs))
			{
				pageState = kPageList;
				pageStateSince = now;
			}
			break;

		case kPageList:
			UpdateAndAdd(live, liveCount);
			break;

		default:
			break;
	}

	Filter();

	// The games the search lets through, in their order, for OnlineScreens_SelectAgain
	OnlineRow visible[kOnlineMostRows];

	for (int i = 0; i < visibleCount; i++)
	{
		visible[i] = shownRows[visibleRows[i]];
	}

	// Where the selection goes. On the fetching screen: its one row. Coming to the list: the game last joined, if it's there; after a
	// Refresh, the game the selection was last on, wherever it moved, or its row number if it went; on a page just opened, the first game.
	// Refresh when there are none. On the list: its game wherever it moved, or where it was, kept inside the rows. SelectAgain gives -1
	// when there are no games, and the selection goes to Refresh.
	int newElement = 0;

	if (pageState == kPageList)
	{
		int const joined = joinedKeyKept ? VisibleIndexOf(joinedKey) : -1;

		if (joined >= 0)
		{
			newElement = kFirstGameElement + joined;
			joinedKeyKept = false;
		}
		else if (stateBefore != kPageList)
		{
			int const again = lastGameKept ? OnlineScreens_SelectAgain(lastGame, lastGameRow, visible, visibleCount) : (visibleCount > 0 ? 0 : -1);
			newElement = again >= 0 ? kFirstGameElement + again : 1;
			joinedKeyKept = false;
		}
		else if (onAGame)
		{
			int const again = OnlineScreens_SelectAgain(selected, oldElement - kFirstGameElement, visible, visibleCount);
			newElement = again >= 0 ? kFirstGameElement + again : 1;
		}
		else
		{
			newElement = oldElement < 0 ? 0 : (oldElement > 1 ? 1 : oldElement);
		}
	}

	*selectedElement = newElement;

	bool changed = pageState != stateBefore || visibleCount != visibleCountBefore || newElement != oldElement;

	for (int i = 0; i < visibleCount && !changed; i++)
	{
		changed = !SameRow(visibleBefore[i], shownRows[visibleRows[i]]);
	}

	return changed;
}

int OnlinePage_RowCount(void)
{
	return pageState == kPageList ? visibleCount : 0;
}

char *OnlinePage_RowName(int row)
{
	static char none[1] = { 0 };
	return (row >= 0 && row < visibleCount) ? shownRows[visibleRows[row]].name : none;
}

// The game on one of the menu's rows: its place in shownRows, or -1 for a row that isn't a game's
static int ShownRowOf(int menuRow)
{
	int const r = menuRow - kFirstGameElement;
	return (pageState == kPageList && r >= 0 && r < visibleCount) ? visibleRows[r] : -1;
}

int OnlinePage_SessionForRow(int menuRow)
{
	int const row = ShownRowOf(menuRow);

	if (row < 0 || shownRows[row].gone)
	{
		return -1;
	}

	uint32_t const found = NumberOfSessionsFound < (uint32_t)MAX_NO_OF_SESSIONS ? NumberOfSessionsFound : (uint32_t)MAX_NO_OF_SESSIONS;

	for (uint32_t i = 0; i < found; i++)
	{
		if (OnlineScreens_SameGame(KeyOf(SessionData[i]), shownRows[row].key))
		{
			return (int)i;
		}
	}

	return -1;
}

void OnlinePage_JoinFailed(const char *why)
{
	char safe[64];

	OnlineScreens_SafeName(why, sizeof(safe), safe, sizeof(safe));
	_snprintf(failedLine, sizeof(failedLine) - 1, "%s: %s", OnlineScreens_Words(kOnlineWordsCouldNotJoin), safe);
	failedLine[sizeof(failedLine) - 1] = '\0';
	failedAt = Now();
	failedShown = true;
}

void OnlinePage_JoinGivenUp(void)
{
	Put(failedLine, sizeof(failedLine), OnlineScreens_Words(kOnlineWordsGaveUp));
	failedAt = Now();
	failedShown = true;
}

// ---- Drawing

// Words in the small font, in a buffer of the page's own, as the font's functions take them
static void Words(const char *text, int x, int y, int alpha, enum AVPMENUFORMAT_ID format)
{
	char line[128];
	Put(line, sizeof(line), text);
	RenderSmallMenuText(line, x, y, alpha, format);
}

// The tail, lit by the menu's highlight glow. alpha is 0 to ONE_FIXED, as the menus' brightness is.
static void DrawTail(int x, int y, int size, int alpha)
{
	uint32_t width = 0, height = 0;
	Tex_GetDimensions(AVPMENUGFX_GLOWY_MIDDLE, width, height);

	// Without its picture there's nothing to light it with, and the HUD's quad would divide by its size
	if (width == 0 || height == 0)
	{
		return;
	}

	OnlineTailPiece pieces[kOnlineTailPieces];
	OnlineTail_Shape(Now(), x, y, size, pieces);

	// The picture's corners, in the same order as a piece's: behind and inside first
	static const int kU[4] = { 0, 1, 1, 0 };
	static const int kV[4] = { 0, 0, 1, 1 };
	int const light = alpha < 0 ? 0 : (alpha > ONE_FIXED ? ONE_FIXED : alpha);

	for (int p = 0; p < kOnlineTailPieces; p++)
	{
		VertexTag corners[4];

		for (int c = 0; c < 4; c++)
		{
			corners[c].X = pieces[p].corner[c].x;
			corners[c].Y = pieces[p].corner[c].y;
			corners[c].U = kU[c] * (int)width;
			corners[c].V = kV[c] * (int)height;
		}

		// The glow adds light, so how bright a piece is is how much of the picture's light it adds
		int const shade = (int)(((long long)pieces[p].brightness * light) / ONE_FIXED);
		D3D_HUDQuad_Output(AVPMENUGFX_GLOWY_MIDDLE, corners, RCOLOR_ARGB(255, shade, shade, shade), FILTERING_BILINEAR_ON);
	}
}

static void DrawCell(const char *text, int column, int y, int alpha)
{
	char fitted[kOnlineRowLetters];

	Put(fitted, sizeof(fitted), text);
	OnlineScreens_FitToWidth(fitted, kColumns[column].width - 6, SmallWidth);
	RenderSmallMenuText(fitted, kColumns[column].x, y, alpha, AVPMENUFORMAT_LEFTJUSTIFIED);
}

static void DrawHeadings(void)
{
	for (int c = 0; c < 5; c++)
	{
		DrawCell(OnlineScreens_Words(kColumns[c].heading), c, kHeadingsTop, ONE_FIXED);
	}
}

static void DrawOneRow(const OnlineRow &row, int y, int brightness)
{
	int const alpha = row.gone ? brightness / 2 : brightness;

	DrawCell(row.name, 0, y, alpha);
	DrawCell(row.level, 1, y, alpha);
	DrawCell(row.gone ? OnlineScreens_Words(kOnlineWordsGone) : row.players, 2, y, alpha);
	DrawCell(row.region, 3, y, alpha);
	DrawCell(row.gone ? "" : row.ping, 4, y, alpha);
}

// J1, the fetching screen: the tail while it fetches, the count once connected, or a failure with how to try again
static void DrawFetching(void)
{
	char server[kRelayListHostLetters + 1];
	char line[160];

	ServerName(server, sizeof(server));

	switch (pageState)
	{
		case kPageFetching:
			OnlineScreens_FetchingWords(server, line, sizeof(line));
			TwoLines(line, OnlineScreens_Words(kOnlineWordsEscBack), true);
			break;

		case kPageConnected:
			OnlineScreens_ConnectedWords(connectedSessions, line, sizeof(line));
			TwoLines(line, "", true);
			break;

		case kPageCantConnect:
		case kPageCantFetch:
			if (pageState == kPageCantConnect)
			{
				OnlineScreens_CantConnectWords(server, line, sizeof(line));
			}
			else
			{
				OnlineScreens_CantFetchWords(server, line, sizeof(line));
			}

			TwoLines(line, OnlineScreens_Words(kOnlineWordsTryAgain), false);

			// The list code's own reason, dim, under it
			Words(listFailedLine, MENU_CENTREX, kWordsTop + 44, ONE_FIXED / 3, AVPMENUFORMAT_CENTREJUSTIFIED);
			break;

		default:
			break;
	}
}

void OnlinePage_DrawTop(void)
{
	if (pageState != kPageList)
	{
		DrawFetching();
		return;
	}

	DrawHeadings();

	if (visibleCount == 0)
	{
		Words(OnlineScreens_Words(shownCount == 0 ? kOnlineWordsNoSessions : kOnlineWordsNoneMatch), MENU_CENTREX, kFirstRowTop, ONE_FIXED / 2,
			AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	// A failed join's line, under the rows, for a few seconds
	if (failedShown && !HasCome(Now(), failedAt + kFailedLineMs))
	{
		int const rows = visibleCount > 0 ? visibleCount : 1;
		Words(failedLine, MENU_CENTREX, kFirstRowTop + rows * kRowStep + 14, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
}

int OnlinePage_ControlTop(int element)
{
	return element == 0 ? kSearchTop : kRefreshTop;
}

void OnlinePage_DrawRow(int element, int brightness)
{
	int const row = ShownRowOf(element);

	if (row >= 0)
	{
		DrawOneRow(shownRows[row], kFirstRowTop + (element - kFirstGameElement) * kRowStep, brightness);
	}
}

// J3's middle: the tail, the step, and Esc. title is the page's title in the big font.
static void DrawJoiningBody(const char *line)
{
	RenderMenuText(GetTextString(TEXTSTRING_MULTIPLAYER_JOININGGAME), MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	DrawTail(kTailX, kTailY, kTailSize, ONE_FIXED);
	Words(line, MENU_CENTREX, kWordsTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	Words(OnlineScreens_Words(kOnlineWordsEscGivesUp), MENU_CENTREX, kWordsTop + 20, ONE_FIXED / 2, AVPMENUFORMAT_CENTREJUSTIFIED);
}

static void JoiningLine(int step, const char *region, char *line, size_t size)
{
	if (step == kNetRelayJoinStepIntroducing)
	{
		Put(line, size, OnlineScreens_Words(kOnlineWordsRelayIntroducing));
		return;
	}

	char safe[kRelayListRegionLetters + 1];
	OnlineScreens_SafeName(region, sizeof(safe) - 1, safe, sizeof(safe));

	if (safe[0] == '\0')
	{
		Put(line, size, OnlineScreens_Words(kOnlineWordsReachingRelay));
		return;
	}

	_snprintf(line, size - 1, "%s in %s", OnlineScreens_Words(kOnlineWordsReachingRelay), safe);
	line[size - 1] = '\0';
}

// Whether the join's wait has drawn frames of its own (OnlinePage_DrawJoining), so the menus' loop needs a new frame opened for it once the
// wait is over (OnlinePage_JoinWaitOver)
static bool joinDrewFrames = false;

void OnlinePage_DrawJoining(const char *region)
{
	char line[96];
	JoiningLine(Net_RelayJoinStep(), region, line, sizeof(line));

	// The join's wait is called from inside the menus' own frame: the menus loop opened it, drew the page, and the player's Enter on a game
	// started the join. A frame can't be opened inside another (Direct3D refuses, with D3DERR_INVALIDCALL: the first footage's log), so the
	// menus' frame is closed first, once, and each frame of the wait is then a whole frame of its own.
	if (!joinDrewFrames)
	{
		ThisFramesRenderingHasFinished();
		joinDrewFrames = true;
	}

	ThisFramesRenderingHasBegun();
	DrawMainMenusBackdrop();
	DrawJoiningBody(line);
	ThisFramesRenderingHasFinished();
	FlipBuffers();
}

void OnlinePage_JoinWaitOver(void)
{
	// The menus' loop goes on to finish its frame and show it, but the wait closed that frame. So a new one is opened, with the menus'
	// backdrop on it, for the loop to finish: otherwise what it shows is a picture nothing was drawn on.
	if (joinDrewFrames)
	{
		joinDrewFrames = false;
		ThisFramesRenderingHasBegun();
		DrawMainMenusBackdrop();
	}
}

// Two lines in the middle of the page, as every waiting and answering screen has them, with the tail above while something is waited for
static void TwoLines(const char *first, const char *second, bool tail)
{
	if (tail)
	{
		DrawTail(kTailX, kTailY, kTailSize, ONE_FIXED);
	}

	Words(first, MENU_CENTREX, kWordsTop, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	Words(second, MENU_CENTREX, kWordsTop + 20, ONE_FIXED / 2, AVPMENUFORMAT_CENTREJUSTIFIED);
}

// The failed join's line on the games page, in the page's own fixed words
static void SayOnThePage(const char *words)
{
	Put(failedLine, sizeof(failedLine), words);
	failedAt = Now();
	failedShown = true;
}

// ---- Asking the host: J4 and J5

static uint32_t askedAt = 0;          // when this joiner's request began to wait, for its seconds
static bool askTimed = false;

void OnlinePage_Joining(int menuRow)
{
	// the menu's row, as the menus number them (the Search box and Refresh first), to the game the search showed on it
	int const row = ShownRowOf(menuRow);

	if (row >= 0)
	{
		joinedKey = shownRows[row].key;
		joinedKeyKept = true;
	}

	askTimed = false;
	joinDrewFrames = false;
}

// This joiner's check code, under the two lines, once its handshake with the host is done (CRYPTO.md section 8: always shown on the join
// screen). Nothing is drawn before then, or in a game on this network, which has no handshake. Added 2026-11-09 by Melanikus for the AvP
// port.
static void DrawTheCheckCode(void)
{
	char digits[kNetRelayCheckCodeBytes];
	char line[48];

	if (!Net_RelayCheckCodeNow(digits))
	{
		return;
	}

	OnlineScreens_CheckCodeWords(digits, line, sizeof(line));

	if (line[0])
	{
		Words(line, MENU_CENTREX, kWordsTop + 50, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
		Words(OnlineScreens_Words(kOnlineWordsCheckCodeMeans), MENU_CENTREX, kWordsTop + 70, ONE_FIXED / 2, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
}

bool OnlinePage_DrawAsking(void)
{
	int const answer = Net_JoinAnswerNow();

	if (answer == kNetJoinAnswerWaiting)
	{
		if (!askTimed)
		{
			askTimed = true;
			askedAt = Now();
		}

		char line[96];
		OnlineScreens_WordsWithSeconds(kOnlineWordsAsking, OnlineScreens_SecondsLeft(askedAt, Now(), kNetJoinAnswerWithinMs), line, sizeof(line));
		TwoLines(line, OnlineScreens_Words(kOnlineWordsEscGivesUp), true);
		DrawTheCheckCode();
		return true;
	}

	if (answer == kNetJoinAnswerYes)
	{
		TwoLines(OnlineScreens_Words(kOnlineWordsLetIn), "", false);
		DrawTheCheckCode();
		return true;
	}

	return false;
}

bool OnlinePage_JoinEnded(void)
{
	askTimed = false;

	switch (Net_JoinAnswerNow())
	{
		case kNetJoinAnswerNo:
			SayOnThePage(OnlineScreens_Words(kOnlineWordsSaidNo));
			return true;
		case kNetJoinAnswerBusy:
			SayOnThePage(OnlineScreens_Words(kOnlineWordsHostBusy));
			return true;
		case kNetJoinAnswerNoAnswer:
			SayOnThePage(OnlineScreens_Words(kOnlineWordsNoAnswer));
			return true;
		case kNetJoinAnswerWaiting:
			// The host's no, busy and no answer all come as answers (networking.h), so a request still waiting when the join ends was Esc
			SayOnThePage(OnlineScreens_Words(kOnlineWordsGaveUp));
			return true;
		default:
			return false;
	}
}

// ---- The host's side: H1

static NetJoinWho askingWho;
static bool hostIsAsked = false;
static uint32_t hostAskedAt = 0;
static uint32_t nextInPlayLineAt = 0;

bool OnlineHost_AskWaiting(char *safeName, size_t size, int *secondsLeft)
{
	char raw[64];
	NetJoinWho who;

	if (!Net_JoinRequestWaiting(raw, sizeof(raw), &who))
	{
		hostIsAsked = false;
		return false;
	}

	// A new request, or another joiner's: its seconds start now
	if (!hostIsAsked || who.connection != askingWho.connection || who.connectID != askingWho.connectID)
	{
		hostIsAsked = true;
		askingWho = who;
		hostAskedAt = Now();
		nextInPlayLineAt = hostAskedAt;
		LogString("online screens: a player asks to join this game");
	}

	// The name is the joiner's own words: only the letters the font has are drawn
	OnlineScreens_SafeName(raw, sizeof(raw), safeName, size);
	*secondsLeft = OnlineScreens_SecondsLeft(hostAskedAt, Now(), kNetJoinAnswerWithinMs);
	return true;
}

// Added 2026-11-09 by Melanikus for the AvP port: see online_screens_page.h
bool OnlineHost_AskCheckCode(char *line, size_t size)
{
	if (size == 0)
	{
		return false;
	}

	line[0] = '\0';
	char digits[kNetRelayCheckCodeBytes];

	if (!hostIsAsked || !Net_JoinRequestCheckCode(askingWho, digits))
	{
		return false;
	}

	OnlineScreens_CheckCodeWords(digits, line, size);
	return line[0] != '\0';
}

void OnlineHost_Answer(bool yes)
{
	if (!hostIsAsked)
	{
		return;
	}

	Net_AnswerJoinRequest(askingWho, yes);
	hostIsAsked = false;

	// No name in the log: who asked is the player's business, and the log is made to be sent to us
	LogString(yes ? "online screens: the host let the player in" : "online screens: the host said no to the player");
}

extern void NewOnScreenMessage(char *messagePtr);
extern void D3D_RenderHUDString_Centred(char *stringPtr, uint32_t centreX, uint32_t y, uint32_t colour);

// H1 in play: who asks, every 5 s while they wait
static void AskingLineInPlay(void)
{
	char name[kOnlineRowLetters];
	int seconds = 0;

	if (!OnlineHost_AskWaiting(name, sizeof(name), &seconds) || !HasCome(Now(), nextInPlayLineAt))
	{
		return;
	}

	nextInPlayLineAt = Now() + 5000;

	// The screen keeps the words it's given, so they're kept here, for as long as the game runs
	static char shown[128];
	char asking[96];

	OnlineScreens_AskingWords(name, seconds, asking, sizeof(asking));
	_snprintf(shown, sizeof(shown) - 1, "%s. %s", asking, OnlineScreens_Words(kOnlineWordsEscToAnswer));
	shown[sizeof(shown) - 1] = '\0';
	NewOnScreenMessage(shown);
}

// ---- H2: the relay's news, on the host

static unsigned int newsSeen = 0;          // the relay state's count of changes, when the page last looked
static char newsLine[128];                 // the latest news, kept for as long as the game runs, as the screen keeps what it's given
static uint32_t newsAt = 0;
static bool newsCame = false;

// Looks at the host's relay state (Net_RelayHostStateNow, which changes nothing by being asked), and makes the words when it has changed.
// True when there's new news this time.
static bool NewsHasCome(void)
{
	NetRelayHostState state;
	Net_RelayHostStateNow(&state);

	if (state.changes == newsSeen)
	{
		return false;
	}

	newsSeen = state.changes;

	char region[kRelayListRegionLetters + 1];
	char why[64];

	switch (state.what)
	{
		case kNetRelayHostFinding:
			Put(newsLine, sizeof(newsLine), OnlineScreens_Words(kOnlineWordsRelayFinding));
			break;
		case kNetRelayHostListed:
			OnlineScreens_SafeName(state.region, sizeof(state.region), region, sizeof(region));
			OnlineScreens_ListedWords(region, state.ms, newsLine, sizeof(newsLine));
			break;
		case kNetRelayHostNoRelay:
			Put(newsLine, sizeof(newsLine), OnlineScreens_Words(kOnlineWordsRelayNone));
			break;
		case kNetRelayHostRoomGone:
			OnlineScreens_SafeName(state.why, sizeof(why) - 1, why, sizeof(why));
			OnlineScreens_RoomGoneWords(why, newsLine, sizeof(newsLine));
			break;
		default:
			return false;
	}

	newsAt = Now();
	newsCame = true;
	return true;
}

bool OnlineHost_NewsLine(char *line, size_t size, bool held)
{
	NewsHasCome();

	if (!newsCame || HasCome(Now(), newsAt + kFailedLineMs))
	{
		return false;
	}

	// Modified 2026-11-09 by Melanikus for the AvP port: while a join request's rows are up, the news waits, its 8 s starting again each
	// frame, so it never covers the check code (online_screens_page.h)
	if (held)
	{
		newsAt = Now();
		return false;
	}

	Put(line, size, newsLine);
	return true;
}

// ---- The line kept up in play, on the host and on a joiner

// How wide words are on the screen at the HUD's own size. They're only letters the font has (made safe), so each one's width is in the
// font's own table.
static int WidthAtTheHUDsSize(const char *words)
{
	int width = 0;

	for (const char *letter = words; *letter; letter++)
	{
		width += AAFontWidths[(unsigned char)*letter];
	}

	return MUL_FIXED(HUDScaleFactor, width);
}

// One line at the top right, at the HUD's own size (the size of its ammo and health numbers, which D3D_RenderHUDString_Centred draws), its
// right end a margin in from the edge. A line wider than the picture starts at its left edge instead.
static void LineAtTheTopRight(char *words, int y, int margin)
{
	int const width = WidthAtTheHUDsSize(words);
	int centreX = (int)ScreenDescriptorBlock.SDB_Width - margin - width / 2;

	if (centreX < width / 2)
	{
		centreX = width / 2;
	}

	D3D_RenderHUDString_Centred(words, (uint32_t)centreX, (uint32_t)y, (uint32_t)RCOLOR_ARGB(255, 220, 220, 220));
}

// The line kept up in play, large, at the HUD's own size, so it can be read in a film, even with two films side by side (the owner,
// 2026-11-09: "Larger text"). Before, it was drawn at the font's own size whatever the screen's, which is tiny on a big one. A line too
// wide for the picture goes on two, split before its first bracket, which is where its explaining starts. line is cut there to do that.
// Modified 2026-11-09 by Melanikus for the AvP port: the HUD's size, the split, and the start under the -dev tag.
static void PlayLineAtTheTopRight(char *line)
{
	int const margin = MUL_FIXED(HUDScaleFactor, 12);
	int const lineHeight = MUL_FIXED(HUDScaleFactor, HUD_FONT_HEIGHT + 4);
	char *const bracket = strstr(line, " (");

	// With -dev, the developers' tag is at the top right too (d3d_render.cpp), so the line starts under the most the tag can ever reach,
	// whatever it says and whatever the window's size: the two never overlap (the owner saw the tag drawn over this line, in the first
	// footage), and the line doesn't jump about as the tag's lines change. While the tag is off, that's 0, and the line is at the top.
	int const top = ShadowStatus_Bottom() + margin;

	if (bracket && WidthAtTheHUDsSize(line) + 2 * margin > (int)ScreenDescriptorBlock.SDB_Width)
	{
		*bracket = '\0';
		LineAtTheTopRight(line, top, margin);
		LineAtTheTopRight(bracket + 1, top + lineHeight, margin);
	}
	else
	{
		LineAtTheTopRight(line, top, margin);
	}
}

static void RelayLineInPlay(void)
{
	NetRelayPlayState state;
	Net_RelayPlayStateNow(&state);

	if (!state.throughRelay)
	{
		return;
	}

	// The server by the name people see for it (its servers.cfg line's name: modified 2026-11-10 by Melanikus for the AvP port, #175) when it
	// has one, otherwise by the relay's region, made safe either way, as anything drawn here is
	char place[kRelayListShownNameLetters + 1];
	char line[160];
	const char *const shownName = Relay_ServerName();

	if (shownName[0] != '\0')
	{
		OnlineScreens_SafeName(shownName, kRelayListShownNameLetters, place, sizeof(place));
	}
	else
	{
		OnlineScreens_SafeName(state.region, sizeof(state.region), place, sizeof(place));
	}

	// A joiner's own time to the server and back, as the games page measured it before the join, or -1 when it has none (a host has none)
	int const serverMs = state.isHost ? -1 : Net_JoinerMsToRelay(state.region);
	OnlineScreens_PlayLineWords(state.isHost, place, state.ms, serverMs, line, sizeof(line));
	PlayLineAtTheTopRight(line);
}

void OnlinePlay_Frame(void)
{
	AskingLineInPlay();

	if (NewsHasCome())
	{
		NewOnScreenMessage(newsLine);
	}

	RelayLineInPlay();
}

// ---- -dev -findgames

static bool findGamesAtStart = false;

void OnlinePage_FindGamesAtStart(void)
{
	findGamesAtStart = true;
}

bool OnlinePage_GoStraightToFindGames(void)
{
	bool const go = findGamesAtStart;

	// Once: back in the menus after a game, they open as usual
	findGamesAtStart = false;
	return go;
}

// ---- -dev -onlinescreens

static bool demoIsOn = false;

void OnlineDemo_TurnOn(void)
{
	demoIsOn = true;
}

// Every screen, in order. Each name is fixed words, for the log and the mark's line.
static const char *const kDemoSteps[] =
{
	"J1 fetching the open sessions",
	"J1 connected",
	"J1 could not connect",
	"J1 connected but could not fetch",
	"J2 the list, Search and Refresh",
	"J2 the list searched",
	"J3 reaching the relay",
	"J3 the relay is introducing you",
	"J4 asking the host",
	"J5 the host let you in",
	"J5 back on the list: you gave up joining",
	"J5 back on the list: the host said no",
	"J5 back on the list: the host was busy",
	"J5 back on the list: the host did not answer",
	"J5 back on the list: could not join",
	"H1 someone asks to join",
	"H2 finding the nearest relay",
	"H2 your game is on the list",
	"H2 no relay answered",
	"H2 the room on the relay has gone",
	"In play: the host's relay and round trip",
	"In play: a joiner's relay and round trip"
};

// Where the run's screens fall in that list. The four H2 screens, then the two lines kept up in play, are the last.
static const int kDemoFirstJoining = 6, kDemoFirstBackOnTheList = 10, kDemoAsked = 15, kDemoFirstNews = 16, kDemoFirstPlayLine = 20;
static_assert(sizeof(kDemoSteps) / sizeof(kDemoSteps[0]) == kDemoFirstPlayLine + 2, "kDemoSteps and the places of its screens must agree");

static const int kDemoStepCount = (int)(sizeof(kDemoSteps) / sizeof(kDemoSteps[0]));

// Made-up games: one of each kind the page has to show. Each name goes through the same making safe a real one does.
struct DemoGame
{
	const char *name;
	const char *level;
	const char *players;
	const char *region;
	int joinerMs, hostMs;
	bool relay, canJoin, gone;
};

static const DemoGame kDemoGames[] =
{
	{ "Hicks's game", "Derelict", "2/4", "saopaulo", 41, 29, true, true, false },
	{ "The longest name a game on this list could ever be given", "Sulaco", "1/8", "virginia", 120, 35, true, true, false },
	{ "Bishop\x01\xE9s den", "Statue", "3/6", "paris", -1, 40, true, true, false },
	{ "Full house", "Waterfall", "full", "paris", 18, 22, true, false, false },
	{ "Old friends", "Leadworks", "V 1.13", "this network", 0, 0, false, false, false },
	{ "Next door", "Hive", "1/4", "this network", 0, 0, false, true, false },
	{ "Vasquez", "Trapped", "2/4", "virginia", 133, 51, true, false, true }
};

static const int kDemoGameCount = (int)(sizeof(kDemoGames) / sizeof(kDemoGames[0]));

static void DemoRow(const DemoGame &game, OnlineRow *row)
{
	memset(row, 0, sizeof(*row));
	OnlineScreens_SafeName(game.name, strlen(game.name), row->name, sizeof(row->name));
	OnlineScreens_SafeName(game.level, strlen(game.level), row->level, sizeof(row->level));
	Put(row->players, sizeof(row->players), game.players);
	Put(row->region, sizeof(row->region), game.region);
	OnlineScreens_PingWords(game.relay, game.joinerMs, game.hostMs, row->ping, sizeof(row->ping));
	row->canJoin = game.canJoin;
	row->gone = game.gone;
	row->session = -1;
}

// The made-up games as the list shows them, one of them selected, and a line under them as a join that ended leaves it
static void DrawDemoList(bool showGone, int selectedRow, const char *line)
{
	DrawHeadings();

	for (int g = 0; g < kDemoGameCount; g++)
	{
		OnlineRow row;
		DemoRow(kDemoGames[g], &row);
		row.gone = row.gone && showGone;
		DrawOneRow(row, kFirstRowTop + g * kRowStep, g == selectedRow ? BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT : BRIGHTNESS_OF_DARKENED_ELEMENT);
	}

	if (line)
	{
		Words(line, MENU_CENTREX, kFirstRowTop + kDemoGameCount * kRowStep + 14, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
}

// The Search box and the Refresh row, drawn as the menus draw a text field and a row: the label to the left of the middle, what's typed
// to the right
static void DrawDemoControls(const char *typed)
{
	char text[kOnlineSearchLetters + 1];
	Put(text, sizeof(text), typed);

	RenderMenuText(GetTextString(TEXTSTRING_MULTIPLAYER_SEARCH), MENU_CENTREX - 9, kSearchTop, BRIGHTNESS_OF_DARKENED_ELEMENT, AVPMENUFORMAT_RIGHTJUSTIFIED);
	RenderMenuText(text, MENU_CENTREX + 9, kSearchTop, BRIGHTNESS_OF_DARKENED_ELEMENT, AVPMENUFORMAT_LEFTJUSTIFIED);
	RenderMenuText(GetTextString(TEXTSTRING_MULTIPLAYER_REFRESH), MENU_CENTREX, kRefreshTop, BRIGHTNESS_OF_DARKENED_ELEMENT, AVPMENUFORMAT_CENTREJUSTIFIED);
}

static void DrawDemoStep(int step)
{
	char line[160];
	char *const gamesTitle = GetTextString(TEXTSTRING_MULTIPLAYER_SELECTSESSION);
	char *const joiningTitle = GetTextString(TEXTSTRING_MULTIPLAYER_JOININGGAME);

	// The page each screen is on: the games page, the joining page, or play (H1 and H2), which has no title
	if (step < kDemoFirstJoining || (step >= kDemoFirstBackOnTheList && step < kDemoAsked))
	{
		RenderMenuText(gamesTitle, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}
	else if (step < kDemoFirstBackOnTheList)
	{
		RenderMenuText(joiningTitle, MENU_CENTREX, 70, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
	}

	switch (step)
	{
		case 0:
			OnlineScreens_FetchingWords("localhost", line, sizeof(line));
			TwoLines(line, OnlineScreens_Words(kOnlineWordsEscBack), true);
			break;
		case 1:
			OnlineScreens_ConnectedWords(kDemoGameCount - 1, line, sizeof(line));
			TwoLines(line, "", true);
			break;
		case 2:
			OnlineScreens_CantConnectWords("localhost", line, sizeof(line));
			TwoLines(line, OnlineScreens_Words(kOnlineWordsTryAgain), false);
			Words("the relay server couldn't be reached", MENU_CENTREX, kWordsTop + 44, ONE_FIXED / 3, AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		case 3:
			OnlineScreens_CantFetchWords("localhost", line, sizeof(line));
			TwoLines(line, OnlineScreens_Words(kOnlineWordsTryAgain), false);
			Words("the relay server said no", MENU_CENTREX, kWordsTop + 44, ONE_FIXED / 3, AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		case 4:
			// The list: the Search box and Refresh row as the menus draw them, then the games, one of them gone
			DrawDemoControls("");
			DrawDemoList(true, 1, NULL);
			break;
		case 5:
		{
			// The same list searched for "hi": only the games with it in their name
			DrawDemoControls("hi");
			DrawHeadings();
			int shown = 0;

			for (int g = 0; g < kDemoGameCount; g++)
			{
				OnlineRow row;
				DemoRow(kDemoGames[g], &row);

				if (OnlineScreens_NameMatches(row.name, "hi"))
				{
					DrawOneRow(row, kFirstRowTop + shown * kRowStep, shown == 0 ? BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT : BRIGHTNESS_OF_DARKENED_ELEMENT);
					shown++;
				}
			}

			break;
		}
		case 6:
			JoiningLine(kNetRelayJoinStepReaching, "saopaulo", line, sizeof(line));
			DrawJoiningBody(line);
			break;
		case 7:
			JoiningLine(kNetRelayJoinStepIntroducing, "saopaulo", line, sizeof(line));
			DrawJoiningBody(line);
			break;
		case 8:
			OnlineScreens_WordsWithSeconds(kOnlineWordsAsking, 25, line, sizeof(line));
			TwoLines(line, OnlineScreens_Words(kOnlineWordsEscGivesUp), true);
			break;
		case 9:
			TwoLines(OnlineScreens_Words(kOnlineWordsLetIn), "", false);
			break;
		case 10:
			DrawDemoList(false, 0, OnlineScreens_Words(kOnlineWordsGaveUp));
			break;
		case 11:
			DrawDemoList(false, 0, OnlineScreens_Words(kOnlineWordsSaidNo));
			break;
		case 12:
			DrawDemoList(false, 0, OnlineScreens_Words(kOnlineWordsHostBusy));
			break;
		case 13:
			DrawDemoList(false, 0, OnlineScreens_Words(kOnlineWordsNoAnswer));
			break;
		case 14:
			_snprintf(line, sizeof(line) - 1, "%s: %s", OnlineScreens_Words(kOnlineWordsCouldNotJoin), "that game is full");
			line[sizeof(line) - 1] = '\0';
			DrawDemoList(false, 3, line);
			break;
		case kDemoAsked:
		{
			// As the host sees it with the in-game menu open: who asks at the top, and the two rows in front of the menu's own. The
			// selection stays on the row it was on, so a key pressed by chance can't let anyone in.
			char letIn[96], sayNo[96];
			OnlineScreens_AskingWords("Ripley", 25, line, sizeof(line));
			OnlineScreens_LetInWords("Ripley", letIn, sizeof(letIn));
			OnlineScreens_SayNoWords("Ripley", sayNo, sizeof(sayNo));
			Words(line, MENU_CENTREX, 40, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
			RenderMenuText(letIn, MENU_CENTREX, 170, BRIGHTNESS_OF_DARKENED_ELEMENT, AVPMENUFORMAT_CENTREJUSTIFIED);
			RenderMenuText(sayNo, MENU_CENTREX, 200, BRIGHTNESS_OF_DARKENED_ELEMENT, AVPMENUFORMAT_CENTREJUSTIFIED);
			RenderMenuText(GetTextString(TEXTSTRING_INGAMEMENU_RESUMEGAME), MENU_CENTREX, 230, BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT, AVPMENUFORMAT_CENTREJUSTIFIED);
			break;
		}
		case kDemoFirstPlayLine:
		case kDemoFirstPlayLine + 1:
		{
			// The line kept at the top right in play, on the host and on a joiner, drawn as play draws it: the HUD's font at the HUD's size
			char playLine[160];
			bool const host = step == kDemoFirstPlayLine;
			OnlineScreens_PlayLineWords(host, "nyc3", host ? 145 : 290, host ? -1 : 146, playLine, sizeof(playLine));
			PlayLineAtTheTopRight(playLine);
			break;
		}
		default:
		{
			// H2: the relay's news, as a line at the top of the screen in play, made the way the game makes it
			int const news = step - kDemoFirstNews;

			if (news == 1)
			{
				OnlineScreens_ListedWords("virginia", 41, line, sizeof(line));
			}
			else if (news == 3)
			{
				OnlineScreens_RoomGoneWords("the relay didn't answer", line, sizeof(line));
			}
			else
			{
				Put(line, sizeof(line), OnlineScreens_Words(news == 2 ? kOnlineWordsRelayNone : kOnlineWordsRelayFinding));
			}

			Words(line, 30, 40, ONE_FIXED, AVPMENUFORMAT_LEFTJUSTIFIED);
			break;
		}
	}

	// The screen's name in the corner, so a picture says which it is
	_snprintf(line, sizeof(line) - 1, "%d of %d: %s", step + 1, kDemoStepCount, kDemoSteps[step]);
	line[sizeof(line) - 1] = '\0';
	Words(line, 30, 450, ONE_FIXED / 3, AVPMENUFORMAT_LEFTJUSTIFIED);
}

void OnlineDemo_Run(void)
{
	if (!demoIsOn)
	{
		return;
	}

	// Once a run: back in the menus after a level, it doesn't start again
	demoIsOn = false;

	LogString("online screens: -onlinescreens shows every screen, with made-up games and no network. F2 saves a picture and goes on, Esc ends");

	int step = 0;
	LogString(std::string("online screens: showing ") + kDemoSteps[step]);

	while (bRunning)
	{
		CheckForWindowsMessages();
		ThisFramesRenderingHasBegun();
		DrawMainMenusBackdrop();
		ReadUserInput();

		if (DebouncedKeyboardInput[KEY_ESCAPE])
		{
			ThisFramesRenderingHasFinished();
			FlipBuffers();
			break;
		}

		DrawDemoStep(step);

		// The picture is of this frame, taken as it finishes, before the next screen is drawn. A mark refused (within a second of the last)
		// stays on this screen, so no screen goes by without its picture.
		bool const next = DebouncedKeyboardInput[KEY_F2] != 0 && Mark_ThisPage(kDemoSteps[step]);

		ThisFramesRenderingHasFinished();
		FlipBuffers();

		if (next)
		{
			step = (step + 1) % kDemoStepCount;
			LogString(std::string("online screens: showing ") + kDemoSteps[step]);
		}

		Sleep(16);
	}

	LogString("online screens: the made-up screens are over");
}
