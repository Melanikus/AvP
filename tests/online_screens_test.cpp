// The test of the online-play screens' sums and words (3dc/avp/win95/frontend/online_screens.cpp). Written 2026-11-09 by Melanikus for the
// AvP port.
//
// WHAT IT'S FOR: the games page shows names that come from other people's computers, so they're hostile (AVP Tao 3). This checks that a name
// can only ever be drawn in the letters the game's fonts have, and never past its column. It checks the spinning tail stays in its box, turns
// the right way at the same speed at any frame rate, and that each of its pieces is a shape the game can draw. And it checks a refresh puts
// the games there now first and shows the ones that went as gone, that the selection goes back to the game it was last on when the list
// comes back after a Refresh, and follows its game as the rows change, that the Search box keeps only the letters the fonts have and finds
// names by them, that the fetching screen names the server the game dialled, and that the waits count down right, even when the clock
// wraps round.
//
// Nothing here uses the network or the game. Where the game draws the screens is checked by reading its source (the last part).

#include "online_screens.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- The tail

// Whether a piece's four corners go round in one direction, as the two triangles the game draws need: every turn from one edge to the
// next bends the same way, and the piece has some size
static bool GoesRound(const OnlineTailPiece &piece)
{
	int sign = 0;

	for (int c = 0; c < 4; c++)
	{
		OnlineTailCorner const &a = piece.corner[c];
		OnlineTailCorner const &b = piece.corner[(c + 1) % 4];
		OnlineTailCorner const &d = piece.corner[(c + 2) % 4];
		long long const turn = (long long)(b.x - a.x) * (d.y - b.y) - (long long)(b.y - a.y) * (d.x - b.x);

		if (turn == 0)
		{
			return false;
		}

		int const thisSign = turn > 0 ? 1 : -1;

		if (sign != 0 && thisSign != sign)
		{
			return false;
		}

		sign = thisSign;
	}

	return true;
}

// The middle of a piece: the average of its corners
static void Middle(const OnlineTailPiece &piece, double *x, double *y)
{
	*x = (piece.corner[0].x + piece.corner[1].x + piece.corner[2].x + piece.corner[3].x) / 4.0;
	*y = (piece.corner[0].y + piece.corner[1].y + piece.corner[2].y + piece.corner[3].y) / 4.0;
}

static void TestTheTail()
{
	puts("The spinning tail");

	OnlineTailPiece pieces[kOnlineTailPieces];
	uint32_t const moments[] = { 0, 1, 374, 375, 749, 1499, 1500, 123456789, 4294967295u };
	int const sizes[] = { -5, 0, 4, 20, 40, 120, 200, 1000 };
	bool inTheBox = true;
	bool drawable = true;

	for (uint32_t ms : moments)
	{
		for (int size : sizes)
		{
			int const s = size < kOnlineTailSmallest ? kOnlineTailSmallest : (size > kOnlineTailBiggest ? kOnlineTailBiggest : size);
			int const reach = (int)ceil(s * 1.2) + 1;

			OnlineTail_Shape(ms, 320, 240, size, pieces);

			for (int i = 0; i < kOnlineTailPieces; i++)
			{
				for (int c = 0; c < 4; c++)
				{
					if (abs(pieces[i].corner[c].x - 320) > reach || abs(pieces[i].corner[c].y - 240) > reach)
					{
						inTheBox = false;
					}
				}

				// At the smallest sizes a piece can round to a line, which draws nothing and harms nothing. From 20 up it must be a shape.
				if (s >= 20 && !GoesRound(pieces[i]))
				{
					drawable = false;
				}
			}
		}
	}

	Check(inTheBox, "every corner of the tail stays within its size, and a fifth more, of its middle, at any moment and any size asked for");
	Check(drawable, "every piece of a tail 20 or more across goes round in one direction, so the game's two triangles draw it whole");

	// The same moment of the turn gives the same tail: it turns by the clock, not by frames
	OnlineTailPiece later[kOnlineTailPieces];
	OnlineTail_Shape(200, 320, 240, 40, pieces);
	OnlineTail_Shape(200 + 3 * kOnlineTailTurnMs, 320, 240, 40, later);
	Check(memcmp(pieces, later, sizeof(pieces)) == 0, "three whole turns later, the tail is exactly where it was");

	// A quarter of a turn moves the blade from the right of the middle to below it: clockwise, since the screen's y runs down
	double x0, y0, x1, y1;
	OnlineTail_Shape(0, 320, 240, 100, pieces);
	Middle(pieces[0], &x0, &y0);
	OnlineTail_Shape(kOnlineTailTurnMs / 4, 320, 240, 100, pieces);
	Middle(pieces[0], &x1, &y1);
	Check(x0 > 360 && fabs(y0 - 240) < 15, "at the start of a turn the blade is to the right of the middle");
	Check(y1 > 280 && fabs(x1 - 320) < 15, "a quarter of a turn later it's below the middle: it turns clockwise");

	// The bones trail behind the blade: the one just behind it is a little anticlockwise of it
	double xBehind, yBehind;
	OnlineTail_Shape(0, 320, 240, 100, pieces);
	Middle(pieces[1], &xBehind, &yBehind);
	Check(yBehind < 240, "the bone behind the blade is on the side the blade has come from");

	// The blade is brightest, and each bone is dimmer than the one before, but none fades away
	bool dimmer = pieces[0].brightness == 255;

	for (int i = 1; i < kOnlineTailPieces; i++)
	{
		dimmer = dimmer && pieces[i].brightness < pieces[i - 1].brightness && pieces[i].brightness >= 60 && pieces[i].brightness <= 255;
	}

	Check(dimmer, "the blade is the brightest, each bone behind it dimmer, and none dimmer than 60");

	// A middle far off the screen is kept near enough that no sum can overflow
	OnlineTail_Shape(1000, 2000000000, -2000000000, 200, pieces);
	bool kept = true;

	for (int i = 0; i < kOnlineTailPieces; i++)
	{
		for (int c = 0; c < 4; c++)
		{
			kept = kept && abs(pieces[i].corner[c].x) <= kOnlineTailFarthest + 300 && abs(pieces[i].corner[c].y) <= kOnlineTailFarthest + 300;
		}
	}

	Check(kept, "a middle two thousand million pixels away is brought back to 10,000, so no corner can overflow");
}

// ---- Names from the network

static void TestSafeNames()
{
	puts("Names from the network");

	char to[64];

	OnlineScreens_SafeName("Ripley", 40, to, sizeof(to));
	Check(strcmp(to, "Ripley") == 0, "a plain name is kept as it is");

	OnlineScreens_SafeName(" !~%s{}", 40, to, sizeof(to));
	Check(strcmp(to, " !~%s{}") == 0, "the space, the tilde and everything printable between them are kept");

	char const hostile[] = { 'A', 1, 9, 10, 13, 27, 127, (char)128, (char)200, (char)255, 'Z', 0 };
	OnlineScreens_SafeName(hostile, sizeof(hostile), to, sizeof(to));
	Check(strcmp(to, "A?????????Z") == 0, "control letters, 127 and every byte from 128 up each become a question mark");

	char const noEnd[9] = { 'H', 'u', 'd', 's', 'o', 'n', 'X', 'Y', 0 };
	OnlineScreens_SafeName(noEnd, 6, to, sizeof(to));
	Check(strcmp(to, "Hudson") == 0, "a name with no 0 is read no further than the bytes it's given");

	OnlineScreens_SafeName("Bishop", 40, to, 4);
	Check(strcmp(to, "Bis") == 0, "a name longer than its room is cut, and ends with a 0");

	OnlineScreens_SafeName("Bishop", 40, to, 1);
	Check(to[0] == '\0', "a room of one byte gets only the 0, for a name");

	to[0] = 'Q';
	OnlineScreens_SafeName("Bishop", 40, to, 0);
	Check(to[0] == 'Q', "a room of no bytes isn't written at all, for a name");

	OnlineScreens_SafeName(NULL, 40, to, sizeof(to));
	Check(to[0] == '\0', "no name at all gives an empty one");
}

// ---- The level column. Added 2026-11-10 by Melanikus for the AvP port (PO's ruling): a co-op game on one of the 32 campaign places is
// named by the game, never by its host. Where the page works out which game that is, is read from its source (TestWhereTheyAreDrawn).

static void TestTheLevelColumn()
{
	puts("The level column");

	char words[64];

	OnlineScreens_LevelWords("Derelict", "Stranded", 40, words, sizeof(words));
	Check(strcmp(words, "Derelict") == 0, "a campaign level's own name is shown, and the name its host sent isn't");

	char const hostile[] = { 'E', 'v', 'i', 'l', 10, 27, '%', 's', 0 };
	OnlineScreens_LevelWords("Derelict", hostile, sizeof(hostile), words, sizeof(words));
	Check(strcmp(words, "Derelict") == 0, "whatever the host sent, letters a screen can't show included");

	OnlineScreens_LevelWords("Derelict", "", 40, words, sizeof(words));
	Check(strcmp(words, "Derelict") == 0, "and when it sent nothing, as an honest host does for a campaign level, there's no \"-\"");

	// The game's own words come from language.txt, a file a player can change, so they're made safe like any name
	OnlineScreens_LevelWords("Der\x1b" "elict", "", 40, words, sizeof(words));
	Check(strcmp(words, "Der?elict") == 0, "a campaign name is made safe too");

	OnlineScreens_LevelWords("", "Stranded", 40, words, sizeof(words));
	Check(strcmp(words, "-") == 0, "and a campaign place with no name of its own shows \"-\", still never the host's");

	OnlineScreens_LevelWords(NULL, hostile, sizeof(hostile), words, sizeof(words));
	Check(strcmp(words, "Evil??%s") == 0, "any other game shows the name its host sent, made safe");

	OnlineScreens_LevelWords(NULL, "", 40, words, sizeof(words));
	Check(strcmp(words, "-") == 0, "and \"-\" when the host sent none");

	OnlineScreens_LevelWords(NULL, NULL, 40, words, sizeof(words));
	Check(strcmp(words, "-") == 0, "or no name at all");

	char const noEnd[9] = { 'H', 'a', 'd', 'l', 'e', 'y', 'X', 'Y', 0 };
	OnlineScreens_LevelWords(NULL, noEnd, 6, words, sizeof(words));
	Check(strcmp(words, "Hadley") == 0, "a host's name with no 0 is read no further than the bytes it's given");

	OnlineScreens_LevelWords("Derelict", NULL, 0, words, 4);
	Check(strcmp(words, "Der") == 0, "a name longer than its room is cut, and ends with a 0");

	words[0] = 'Q';
	OnlineScreens_LevelWords("Derelict", "Stranded", 40, words, 0);
	Check(words[0] == 'Q', "a room of no bytes isn't written at all");
}

// Every letter 10 wide and a dot 4, and a note of the lowest letter number asked about
static int lowestLetterAsked = 1000;

static int TenWide(int letter)
{
	if (letter < lowestLetterAsked)
	{
		lowestLetterAsked = letter;
	}

	return letter == '.' ? 4 : 10;
}

static int Hostile(int letter)
{
	return letter == 'a' ? -2000000000 : (letter == 'b' ? 2000000000 : 10);
}

static int WidthOf(const char *text, int (*widthOf)(int))
{
	int width = 0;

	for (; *text; text++)
	{
		int const w = widthOf((unsigned char)*text);
		width += w < 0 ? 0 : (w > 1000 ? 1000 : w);
	}

	return width;
}

static void TestFitting()
{
	puts("Names fitted to their column");

	char text[64];

	strcpy(text, "abc");
	OnlineScreens_FitToWidth(text, 30, TenWide);
	Check(strcmp(text, "abc") == 0, "a name that fits is left as it is");

	strcpy(text, "abcdefgh");
	OnlineScreens_FitToWidth(text, 50, TenWide);
	Check(strcmp(text, "abcd..") == 0, "a name too wide keeps the letters that fit with two dots after them");

	strcpy(text, "abc");
	OnlineScreens_FitToWidth(text, 29, TenWide);
	Check(strcmp(text, "a..") == 0, "the dots take the place of letters, so a shortened name never grows past its room");

	strcpy(text, "abcdef");
	OnlineScreens_FitToWidth(text, 5, TenWide);
	Check(text[0] == '\0', "a column too narrow even for the dots gets nothing");

	strcpy(text, "W");
	OnlineScreens_FitToWidth(text, 9, TenWide);
	Check(text[0] == '\0', "one letter too wide gets nothing, even where the dots would fit");

	// Every length and width: never wider than the column, never longer than it was
	bool fits = true;

	for (int length = 0; length < 40; length++)
	{
		for (int most = 0; most < 420; most += 7)
		{
			// Copied with the room it has, so a longer name than text holds would be cut short, never written past its end
			std::string const original(length, 'm');
			snprintf(text, sizeof(text), "%s", original.c_str());
			OnlineScreens_FitToWidth(text, most, TenWide);
			fits = fits && strlen(text) <= (size_t)length && WidthOf(text, TenWide) <= most;
		}
	}

	Check(fits, "for every length and column width, the result is never wider than the column nor longer than it was");

	strcpy(text, "abababab");
	OnlineScreens_FitToWidth(text, 40, Hostile);
	Check(strlen(text) <= 8 && WidthOf(text, Hostile) <= 40, "a font that gives a letter a huge or negative width can't make the sums overflow");

	char high[] = { (char)200, (char)255, 'x', 0 };
	lowestLetterAsked = 1000;
	OnlineScreens_FitToWidth(high, 100, TenWide);
	Check(lowestLetterAsked >= 0, "a letter from 128 up is asked about as 128 to 255, never as a negative number");
}

static void TestSplitting()
{
	puts("A game's name and its players");

	char name[40], players[16];

	OnlineScreens_SplitName("Hadley (2/4)", 40, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, "Hadley") == 0 && strcmp(players, "2/4") == 0, "\"Hadley (2/4)\" is the name Hadley, and 2/4 playing");

	OnlineScreens_SplitName("Hadley (V 1.13)", 40, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, "Hadley") == 0 && strcmp(players, "V 1.13") == 0, "another version's game keeps its version in the second part");

	OnlineScreens_SplitName("Sulaco", 40, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, "Sulaco") == 0 && players[0] == '\0', "a name with no brackets is all name");

	OnlineScreens_SplitName("A (b) (3/8)", 40, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, "A (b)") == 0 && strcmp(players, "3/8") == 0, "only the last brackets are the players");

	OnlineScreens_SplitName("()", 40, name, sizeof(name), players, sizeof(players));
	Check(name[0] == '\0' && players[0] == '\0', "empty brackets alone give an empty name and nothing playing");

	OnlineScreens_SplitName(")", 40, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, ")") == 0 && players[0] == '\0', "a closing bracket alone is a name");

	OnlineScreens_SplitName("Hadley (2/4)", 9, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, "Hadley (2") == 0 && players[0] == '\0', "a name cut short of its closing bracket has no players part");

	char const bad[] = { 'X', (char)200, ' ', '(', 1, '/', '4', ')', 0 };
	OnlineScreens_SplitName(bad, 40, name, sizeof(name), players, sizeof(players));
	Check(strcmp(name, "X?") == 0 && strcmp(players, "?/4") == 0, "both parts are made safe");

	OnlineScreens_SplitName(NULL, 40, name, sizeof(name), players, sizeof(players));
	Check(name[0] == '\0' && players[0] == '\0', "no name at all gives two empty parts");
}

// ---- The rows

static OnlineRow Row(bool relay, const char *code, uint32_t host, uint16_t port, int session)
{
	OnlineRow row = {};
	row.key.throughRelay = relay;

	if (code)
	{
		memcpy(row.key.code, code, kRelayCodeLetters);
	}

	row.key.host = host;
	row.key.port = port;
	row.session = session;
	row.canJoin = true;
	return row;
}

static void TestRows()
{
	puts("The rows kept steady");

	OnlineRow const a = Row(true, "AAAAAAAA", 0, 0, 0);
	OnlineRow const b = Row(true, "BBBBBBBB", 0, 0, 1);
	OnlineRow const c = Row(false, NULL, 0x0100007f, 23513, 2);
	OnlineRow const d = Row(false, NULL, 0x0100007f, 23515, 3);
	OnlineRow const lanWithACode = Row(false, "AAAAAAAA", 0, 0, 4);
	OnlineRow const nearlyA = Row(true, "AAAAAAAB", 0, 0, 5);

	Check(OnlineScreens_SameGame(a.key, a.key) && !OnlineScreens_SameGame(a.key, b.key), "two relay games are the same game only with the same code");
	Check(!OnlineScreens_SameGame(a.key, nearlyA.key), "all eight letters of a code count, the last too");
	Check(!OnlineScreens_SameGame(c.key, d.key), "two games on this network differ by their port");
	Check(!OnlineScreens_SameGame(a.key, lanWithACode.key), "a relay game and a game on this network are never the same");

	OnlineRow before[3] = { a, b, c };
	OnlineRow now[2] = { b, d };
	OnlineRow merged[kOnlineMostRows];
	int const count = OnlineScreens_MergeRows(before, 3, now, 2, merged, kOnlineMostRows);

	Check(count == 4, "after a refresh: the two games there now, then the two that went");
	Check(count == 4 && OnlineScreens_SameGame(merged[0].key, b.key) && OnlineScreens_SameGame(merged[1].key, d.key) && !merged[0].gone && !merged[1].gone,
		"the games there now come first, in their order, not gone");
	Check(count == 4 && OnlineScreens_SameGame(merged[2].key, a.key) && merged[2].gone && !merged[2].canJoin && merged[2].session == -1,
		"a game that went is shown gone, and can't be joined");
	Check(count == 4 && OnlineScreens_SameGame(merged[3].key, c.key) && merged[3].gone, "so is the other");

	OnlineRow again[kOnlineMostRows];
	OnlineRow onlyD[1] = { d };
	int const second = OnlineScreens_MergeRows(merged, count, onlyD, 1, again, kOnlineMostRows);
	Check(second == 2 && OnlineScreens_SameGame(again[0].key, d.key) && OnlineScreens_SameGame(again[1].key, b.key) && again[1].gone,
		"a refresh later, the games gone before are gone for good, and the one that's just gone is shown gone");

	int const capped = OnlineScreens_MergeRows(before, 3, now, 2, merged, 3);
	Check(capped == 3 && !merged[0].gone && !merged[1].gone && merged[2].gone, "no more rows are written than there's room for, the games there now first");

	Check(OnlineScreens_MergeRows(NULL, 0, before, 3, merged, 2) == 2, "no more rows are written than there's room for, even of the games there now");
	Check(OnlineScreens_MergeRows(NULL, 0, NULL, 0, merged, kOnlineMostRows) == 0, "no games before or now gives no rows");

	// Where the selection goes when the rows change: while the list is shown, and when it comes back after a Refresh, for the game the
	// selection was last on before the player went up to the Refresh row (OnlinePage_Frame)
	OnlineRow rows[3] = { d, b, a };
	Check(OnlineScreens_SelectAgain(b.key, 0, rows, 3) == 1, "the selection follows its game to its new row");
	Check(OnlineScreens_SelectAgain(c.key, 7, rows, 3) == 2, "when its game has gone, it stays on the same row number, kept inside the rows");
	Check(OnlineScreens_SelectAgain(c.key, -4, rows, 3) == 0, "and never above the first row");
	Check(OnlineScreens_SelectAgain(a.key, 0, rows, 0) == -1, "with no rows, nothing is selected");
	Check(OnlineScreens_SelectAgain(a.key, 0, NULL, 3) == -1, "and nothing is read from rows that aren't there");

	// A Refresh, as the page has it: the player was on b (row 1 of a, b, c), went up to Refresh, and the list came back merged
	OnlineRow beforeRefresh[3] = { a, b, c };
	OnlineRow afterRefresh[kOnlineMostRows];
	OnlineRow nowThere[2] = { d, b };
	int const afterCount = OnlineScreens_MergeRows(beforeRefresh, 3, nowThere, 2, afterRefresh, kOnlineMostRows);
	Check(OnlineScreens_SelectAgain(b.key, 1, afterRefresh, afterCount) == 1 && OnlineScreens_SameGame(afterRefresh[1].key, b.key),
		"after a Refresh, the selection goes back to the game it was on, at its new row");
	Check(OnlineScreens_SelectAgain(a.key, 0, afterRefresh, afterCount) == 2 && afterRefresh[2].gone,
		"and a game that went is still found, shown gone, so the player sees what happened to it");
}

// ---- The Search box and the fetching screen

static void TestSearch()
{
	puts("The Search box");

	char safe[64];

	OnlineScreens_SearchText("Hicks", 40, safe, sizeof(safe));
	Check(strcmp(safe, "Hicks") == 0, "what's typed is kept as it is when it's plain letters");

	OnlineScreens_SearchText("abcdefghijklmnopqrstuvwxyz", 40, safe, sizeof(safe));
	Check(strlen(safe) == kOnlineSearchLetters && strncmp(safe, "abcdefghijklmnopqrst", kOnlineSearchLetters) == 0, "a search is cut to its 20 letters");

	char const hostile[] = { 'H', 1, (char)200, 'x', 0 };
	OnlineScreens_SearchText(hostile, sizeof(hostile), safe, sizeof(safe));
	Check(strcmp(safe, "H??x") == 0, "a letter the fonts don't have becomes a question mark, as in a name");

	OnlineScreens_SearchText("Hicks", 3, safe, sizeof(safe));
	Check(strcmp(safe, "Hic") == 0, "and no more of it is read than it's given");

	OnlineScreens_SearchText("Hicks", 40, safe, 3);
	Check(strcmp(safe, "Hi") == 0, "a room smaller than a search gets as much as fits, and its 0");

	safe[0] = 'Q';
	OnlineScreens_SearchText("Hicks", 40, safe, 0);
	Check(safe[0] == 'Q', "a room of no bytes isn't written at all, for a search");

	Check(OnlineScreens_NameMatches("Hicks's game", "hicks"), "a search finds its letters in a name, whatever the capitals");
	Check(OnlineScreens_NameMatches("Hicks's game", "GAME"), "anywhere in the name, the end too");
	Check(!OnlineScreens_NameMatches("Hicks's game", "Hudson"), "and not a name without them");
	Check(OnlineScreens_NameMatches("Hicks's game", "") && OnlineScreens_NameMatches("Hicks's game", NULL), "an empty search takes every game");
	Check(!OnlineScreens_NameMatches("Hi", "Hicks"), "a search longer than the name can't be in it");
	Check(!OnlineScreens_NameMatches(NULL, "Hicks"), "no name has nothing in it");
	Check(!OnlineScreens_NameMatches("a[c", "A{C"), "only A to Z count as capitals: [ and { stay different");

	puts("The fetching screen");

	char words[160];

	OnlineScreens_FetchingWords("localhost", words, sizeof(words));
	Check(strcmp(words, "Fetching localhost's open sessions") == 0, "while it fetches, it names the server the game dialled");

	OnlineScreens_ConnectedWords(3, words, sizeof(words));
	Check(strcmp(words, "Connected: 3 sessions open") == 0, "once connected, how many sessions are open");

	OnlineScreens_ConnectedWords(1, words, sizeof(words));
	Check(strcmp(words, "Connected: 1 session open") == 0, "one session is a session");

	OnlineScreens_ConnectedWords(0, words, sizeof(words));
	Check(strcmp(words, "Connected: no sessions open") == 0, "and none is no sessions");

	OnlineScreens_ConnectedWords(2000000000, words, sizeof(words));
	Check(strcmp(words, "Connected: 999 sessions open") == 0, "a count past 999 is shown as 999");

	OnlineScreens_CantConnectWords("virginia.relay.avpx.test", words, sizeof(words));
	Check(strcmp(words, "Couldn't connect to virginia.relay.avpx.test") == 0, "a server that couldn't be reached is named in the failure");

	OnlineScreens_CantFetchWords("localhost", words, sizeof(words));
	Check(strcmp(words, "Connected to localhost, but couldn't fetch its open sessions") == 0, "and one reached that didn't give its sessions says so");

	std::string const longServer(100, 's');
	OnlineScreens_FetchingWords(longServer.c_str(), words, sizeof(words));
	Check(strlen(words) == strlen("Fetching 's open sessions") + kRelayListHostLetters, "a server's name is cut to a host name's 63 letters");

	OnlineScreens_FetchingWords(NULL, words, sizeof(words));
	Check(strcmp(words, "Fetching 's open sessions") == 0, "no name at all is written as nothing");

	words[0] = 'Q';
	OnlineScreens_CantFetchWords("localhost", words, 0);
	Check(words[0] == 'Q', "a room of no bytes isn't written at all, for the fetching screen");
}

// ---- Ping, waits and words

static void TestPingAndWaits()
{
	puts("The ping column and the waits");

	char words[32];

	OnlineScreens_PingWords(false, 40, 30, words, sizeof(words));
	Check(strcmp(words, "-") == 0, "a game on this network has no relay to time");

	OnlineScreens_PingWords(true, -1, 30, words, sizeof(words));
	Check(strcmp(words, "?") == 0, "until the joiner has timed the relay, the ping is a question mark");
	// Added 2026-11-10 by Melanikus for the AvP port (PO's ruling): while the games page is timing the relay, three dots
	OnlineScreens_PingWords(true, kOnlineJoinerMsMeasuring, 30, words, sizeof(words));
	Check(strcmp(words, "...") == 0, "while the games page is timing the relay, the ping is three dots");
	OnlineScreens_PingWords(true, -3, 30, words, sizeof(words));
	Check(strcmp(words, "?") == 0, "and any other time below 0 is still a question mark");

	OnlineScreens_PingWords(true, 40, 30, words, sizeof(words));
	Check(strcmp(words, "about 70 ms") == 0, "a relay game's ping is the joiner's time to the relay and the host's, added");

	OnlineScreens_PingWords(true, 40, -5, words, sizeof(words));
	Check(strcmp(words, "about 40 ms") == 0, "a host's time below 0 counts as not measured");

	OnlineScreens_PingWords(true, 2000000000, 2000000000, words, sizeof(words));
	Check(strcmp(words, "about 19998 ms") == 0, "huge times are taken as 9999 each, as the list caps them, so the sum can't overflow");

	Check(OnlineScreens_SecondsLeft(0, 0, kOnlineAnswerMs) == 30, "a 30 s wait starts with 30 seconds left");
	Check(OnlineScreens_SecondsLeft(0, 29001, kOnlineAnswerMs) == 1 && OnlineScreens_SecondsLeft(0, 29999, kOnlineAnswerMs) == 1,
		"its last second shows as 1, not 0, until it's over");
	Check(OnlineScreens_SecondsLeft(0, 30000, kOnlineAnswerMs) == 0 && OnlineScreens_SecondsLeft(0, 40000, kOnlineAnswerMs) == 0, "and then 0, once the 30 s are over");
	Check(OnlineScreens_SecondsLeft(0xFFFFFF00u, 0x00000100u, kOnlineAnswerMs) == 30, "a wait across the clock's wrap counts only the half second gone");
}

static void TestWords()
{
	puts("The screens' words");

	bool allThere = true;

	for (int w = 0; w < kOnlineWordsCount; w++)
	{
		const char *const words = OnlineScreens_Words((OnlineWords)w);
		allThere = allThere && words[0] != '\0';

		for (const char *letter = words; *letter; letter++)
		{
			allThere = allThere && (unsigned char)*letter >= 32 && (unsigned char)*letter <= 126;
		}
	}

	Check(allThere, "every screen has its words, all in letters the fonts have");
	Check(OnlineScreens_Words(kOnlineWordsCount)[0] == '\0' && OnlineScreens_Words((OnlineWords)-1)[0] == '\0', "a number that isn't one of them gives none");

	char words[96];
	OnlineScreens_WordsWithSeconds(kOnlineWordsAsking, 25, words, sizeof(words));
	Check(strcmp(words, "Asking the host to let you in (25 s)") == 0, "a wait's words end with its seconds left");

	OnlineScreens_AskingWords("Ripley", 25, words, sizeof(words));
	Check(strcmp(words, "Ripley asks to join (25 s)") == 0, "the host is told who asks, and how long is left to answer");

	OnlineScreens_LetInWords("Ripley", words, sizeof(words));
	Check(strcmp(words, "Let Ripley in") == 0, "the yes row names who it lets in");

	OnlineScreens_SayNoWords("Ripley", words, sizeof(words));
	Check(strcmp(words, "Say no to Ripley") == 0, "the no row names who it says no to");

	std::string const longName(60, 'n');
	OnlineScreens_LetInWords(longName.c_str(), words, sizeof(words));
	Check(strlen(words) == strlen("Let  in") + kOnlineRowLetters - 1, "a name longer than a row is cut to a row's letters");

	OnlineScreens_AskingWords("%s%n%x", 3, words, sizeof(words));
	Check(strcmp(words, "%s%n%x asks to join (3 s)") == 0, "a name made of format letters is written as letters, never read as a pattern");

	words[0] = 'Q';
	OnlineScreens_SayNoWords("Ripley", words, 0);
	Check(words[0] == 'Q', "a room of no bytes isn't written at all, for a line");

	OnlineScreens_AskingWords(NULL, 3, words, sizeof(words));
	Check(strcmp(words, " asks to join (3 s)") == 0, "no name at all is written as nothing");

	// H2, the relay's news on the host's screen
	OnlineScreens_ListedWords("saopaulo", 41, words, sizeof(words));
	Check(strcmp(words, "Your game is on the list, through saopaulo, 41 ms") == 0, "the host is told its game is on the list, through which relay, and how far it is");

	OnlineScreens_ListedWords("", 41, words, sizeof(words));
	Check(strcmp(words, "Your game is on the list") == 0, "with no region, only that the game is on the list");

	OnlineScreens_ListedWords("paris", 2000000000, words, sizeof(words));
	Check(strcmp(words, "Your game is on the list, through paris, 9999 ms") == 0, "a round trip past 9999 ms is shown as 9999, as the list caps it");

	std::string const longRegion(40, 'r');
	OnlineScreens_ListedWords(longRegion.c_str(), 5, words, sizeof(words));
	Check(strlen(words) == strlen("Your game is on the list, through , 5 ms") + kRelayListRegionLetters, "a region longer than a region can be is cut to 16 letters");

	OnlineScreens_RoomGoneWords("the relay didn't answer", words, sizeof(words));
	Check(strcmp(words, "Your game's room on the relay has gone: the relay didn't answer") == 0, "a room that's gone says why, in the networking code's words");

	OnlineScreens_RoomGoneWords(NULL, words, sizeof(words));
	Check(strcmp(words, "Your game's room on the relay has gone") == 0, "and with no reason, only that it's gone");

	// The line kept up in play, on both copies of a game through a relay, in the owner's words, each time saying what it measures
	char line[160];

	OnlineScreens_PlayLineWords(true, "nyc3", 41, -1, line, sizeof(line));
	Check(strcmp(line, "HOST, Relay: nyc3, 41 ms (round trip, host to server)") == 0, "in play, the host's line names its relay and its round trip");

	OnlineScreens_PlayLineWords(false, "nyc3", 63, -1, line, sizeof(line));
	Check(strcmp(line, "CLIENT, Relay: nyc3, 63 ms (round trip, client to host through server)") == 0,
		"and a joiner's names the game's relay and its round trip to the host");

	OnlineScreens_PlayLineWords(false, "nyc3", 290, 146, line, sizeof(line));
	Check(strcmp(line, "CLIENT, Relay: nyc3, 290 ms (round trip, client to host through server), 146 ms round trip to server") == 0,
		"a joiner's own round trip to the server follows, when the games page measured one");

	OnlineScreens_PlayLineWords(true, "nyc3", 41, 146, line, sizeof(line));
	Check(strcmp(line, "HOST, Relay: nyc3, 41 ms (round trip, host to server)") == 0, "but a host has no such time, so none is said");

	OnlineScreens_PlayLineWords(false, "nyc3", -1, 146, line, sizeof(line));
	Check(strcmp(line, "CLIENT, Relay: nyc3") == 0, "until a round trip is measured, the line leaves the times out");

	OnlineScreens_PlayLineWords(true, "paris", 2000000000, -1, line, sizeof(line));
	Check(strcmp(line, "HOST, Relay: paris, 9999 ms (round trip, host to server)") == 0, "a round trip past 9999 ms is shown as 9999, in play too");

	OnlineScreens_PlayLineWords(false, "paris", 12, 2000000000, line, sizeof(line));
	Check(strcmp(line, "CLIENT, Relay: paris, 12 ms (round trip, client to host through server), 9999 ms round trip to server") == 0,
		"and so is a joiner's time to the server");

	// Added 2026-11-09 by Melanikus for the AvP port: a joiner's own round trip to the host is capped too, with its time to the server and
	// without (take-out row 39 wasn't noticed: only the host's and the server's times were tried past 9999)
	OnlineScreens_PlayLineWords(false, "paris", 2000000000, 146, line, sizeof(line));
	Check(strcmp(line, "CLIENT, Relay: paris, 9999 ms (round trip, client to host through server), 146 ms round trip to server") == 0,
		"and a joiner's round trip to the host, beside its time to the server");
	OnlineScreens_PlayLineWords(false, "paris", 2000000000, -1, line, sizeof(line));
	Check(strcmp(line, "CLIENT, Relay: paris, 9999 ms (round trip, client to host through server)") == 0,
		"and without a time to the server");

	OnlineScreens_PlayLineWords(true, "", 12, -1, line, sizeof(line));
	Check(strcmp(line, "HOST, Relay: a relay, 12 ms (round trip, host to server)") == 0, "a region that isn't known is said as a relay");

	OnlineScreens_PlayLineWords(true, "NY Mel's Server", 145, -1, line, sizeof(line));
	Check(strcmp(line, "HOST, Relay: NY Mel's Server, 145 ms (round trip, host to server)") == 0,
		"the server's shown name goes where the region would, spaces and all");

	OnlineScreens_PlayLineWords(false, longRegion.c_str(), 7, -1, line, sizeof(line));
	Check(strlen(line) == strlen("CLIENT, Relay: , 7 ms (round trip, client to host through server)") + kRelayListShownNameLetters,
		"in play too, a name is cut to a shown name's 32 letters");

	char small[20];
	OnlineScreens_PlayLineWords(false, "nyc3", 290, 146, small, sizeof(small));
	Check(small[sizeof(small) - 1] == '\0' && strlen(small) == sizeof(small) - 1, "a line too long for its room is cut, and still ends with a 0");
}

// ---- Where the game draws them (read, not run: the drawing needs the whole game)

// A source file, as text, from the top of the repository, with its CRs taken out. The names are this file's own, never from outside.
static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why.c_str());
}

static bool Has(const std::string &text, const char *piece)
{
	return text.find(piece) != std::string::npos;
}

static void TestWhereTheyAreDrawn()
{
	puts("Where the game draws them, by reading");

	std::string const page = ReadSource("3dc/avp/win95/frontend/online_screens_page.cpp");
	std::string const render = ReadSource("3dc/d3d_render.cpp");
	Check(page.size() > 20000 && render.size() > 100000, "the page and the renderer were found and read, from the top of the repository");

	// The relay's line in play is at the top right, and so is the -dev tag. The owner saw the tag drawn over the line (the first footage),
	// so the line starts under the most the tag can ever reach: they never overlap, whatever the tag says and whatever the window's size.
	// Both halves of a line split in two start from there, and play's line is the one drawn this way.
	std::string const playLine = SourceSlice(page, "static void RelayLineInPlay(void)", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	std::string const topRight = SourceSlice(page, "static void PlayLineAtTheTopRight(char *line)", "\n}\n", kSliceEndAfterTheStart,
		FailedSlice);
	Check(Has(playLine, "PlayLineAtTheTopRight(line);") && Has(topRight, "int const top = ShadowStatus_Bottom() + margin;") &&
		Has(topRight, "LineAtTheTopRight(line, top, margin);") && Has(topRight, "LineAtTheTopRight(bracket + 1, top + lineHeight, margin);"),
		"the relay's line in play is drawn under the -dev tag, both its halves when it's split");

	std::string const bottom = SourceSlice(render, "int ShadowStatus_Bottom()", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(bottom, "if (!DevMode || !ShadowStatusOn)") &&
		Has(bottom, "return sizes.margin / 2 + sizes.lineHeight * kShadowStatusMostLines + sizes.margin;"),
		"the tag reaches as far down as its backing does with every line it can have, whenever it can be shown");

	// (from its body: the renderer names it once before, to say it's there)
	std::string const tag = SourceSlice(render, "static void ShadowStatus_Draw()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(tag, "if (!DevMode || !ShadowStatusOn || !viewRan)") && Has(tag, "enum { kMostLines = kShadowStatusMostLines,") &&
		Has(tag, "ShadowStatusSizes const sizes = ShadowStatus_Sizes();") &&
		Has(tag, "DrawQuad(rightX - widest - margin, margin / 2, widest + margin * 2 - margin / 2, lineHeight * numLines + margin, ") &&
		Has(tag, "DrawStatusLine(lines[i], rightX, margin + i * lineHeight, scale);"),
		"and the tag itself has no more lines than that, at the same sizes, from the same top");

	// The selection across a Refresh (added 2026-11-09). The page keeps the game the selection was last on while the list was shown, goes back
	// to it with OnlineScreens_SelectAgain when the list comes back, and uses the same function while the list is shown. A page just opened
	// forgets it, and starts on its first game.
	std::string const frame = SourceSlice(page, "bool OnlinePage_Frame(bool listChanged, int *selectedElement)\n{", "\n}\n", kSliceEndAfterTheStart,
		FailedSlice);
	Check(Has(frame, "lastGame = selected;\n\t\tlastGameRow = oldElement - kFirstGameElement;\n\t\tlastGameKept = true;"),
		"the page keeps the game the selection is on, and its row, while the list is shown");
	Check(Has(frame, "int const again = lastGameKept ? OnlineScreens_SelectAgain(lastGame, lastGameRow, visible, visibleCount) : (visibleCount > 0 ? 0 : -1);"),
		"when the list comes back, the selection goes back to that game by OnlineScreens_SelectAgain, or to the first game on a page just opened");
	Check(Has(frame, "int const again = OnlineScreens_SelectAgain(selected, oldElement - kFirstGameElement, visible, visibleCount);"),
		"while the list is shown, the selection follows its game by the same function");
	Check(Has(frame, "visible[i] = shownRows[visibleRows[i]];") && Has(frame, "newElement = again >= 0 ? kFirstGameElement + again : 1;"),
		"over the games the search lets through, going to Refresh when there are none");
	std::string const opened = SourceSlice(page, "void OnlinePage_Opened(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(opened, "lastGameKept = false;"), "a page just opened forgets the game last selected");

	// The level column (added 2026-11-10 by Melanikus for the AvP port, PO's ruling). A co-op game is on a campaign place when its number
	// is one of the 32 on the co-op list, and the name comes from the co-op pages' own list for that place. Nothing else writes the column.
	std::string const rows = SourceSlice(page, "static int LiveRows(OnlineRow *rows, int most)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(rows, "bool const onACampaign = session.gameStyle == NGT_Coop &&\n\t\t\tLevelList_CampaignAt(session.levelIndex, "
		"MAX_NO_OF_COOPERATIVE_EPISODES, &campaign, &campaignLevel);"),
		"a game is on a campaign place only when it's a co-op game and its number is one of the 32 on the co-op list");
	Check(Has(rows, "OnlineScreens_LevelWords(onACampaign ? CoopMenu_LevelName(campaign, campaignLevel) : NULL, session.levelName, "
		"sizeof(session.levelName),\n\t\t\trow.level, sizeof(row.level));"),
		"and then the column shows the co-op pages' own name for that place, and the host's name for any other game");
	// (on the rows' code itself: an empty slice would have nothing in it to find, and pass)
	Check(rows.size() > 500 && !Has(rows, "OnlineScreens_SafeName(session.levelName"), "the host's level name reaches the column by no other way");
}

// ---- The check code (protocol version 2, CRYPTO.md section 8). Added 2026-11-09 by Melanikus for the AvP port. It reads the source with
// ReadSource and Has, above.

// The text from the first start up to the end after it, or "" if either isn't there
static std::string Between(const std::string &text, const char *start, const char *end)
{
	size_t const from = text.find(start);
	size_t const to = from == std::string::npos ? std::string::npos : text.find(end, from + strlen(start));
	return to == std::string::npos ? std::string() : text.substr(from, to - from);
}

static void TestTheCheckCode()
{
	puts("The check code");

	char words[64];
	OnlineScreens_CheckCodeWords("123456", words, sizeof(words));
	Check(strcmp(words, "Check code: 123 456") == 0, "6 digits are shown in two threes, easy to read out");
	OnlineScreens_CheckCodeWords("000000", words, sizeof(words));
	Check(strcmp(words, "Check code: 000 000") == 0, "and 0s are digits like any other");

	struct NotACode
	{
		const char *digits;
		const char *what;
	};

	NotACode const notCodes[] = {
		{ "12345", "5 digits" },
		{ "1234567", "7 digits" },
		{ "12a456", "a letter among them" },
		{ "12 456", "a space among them" },
		{ "", "nothing" },
		{ "-12345", "a sign" }
	};

	bool noneShown = true;

	for (size_t i = 0; i < sizeof(notCodes) / sizeof(notCodes[0]); i++)
	{
		memset(words, 'x', sizeof(words));
		OnlineScreens_CheckCodeWords(notCodes[i].digits, words, sizeof(words));
		noneShown = noneShown && words[0] == '\0';
	}

	memset(words, 'x', sizeof(words));
	OnlineScreens_CheckCodeWords(NULL, words, sizeof(words));
	Check(noneShown && words[0] == '\0', "anything but exactly 6 digits shows no code at all: 5 or 7 digits, a letter, a space, a sign, or nothing");

	// Looked at a byte at a time, never as a string, so a line left unended can't be read past its room
	char small[8];
	memset(small, 'x', sizeof(small));
	OnlineScreens_CheckCodeWords("123456", small, sizeof(small));
	Check(memcmp(small, "Check c", 7) == 0 && small[7] == '\0', "a line with too little room is cut, and always ended");

	// Where the screens show it: the network's half (networking.h, Fifth's) and the drawing need the game, so the source is read
	std::string const page = ReadSource("3dc/avp/win95/frontend/online_screens_page.cpp");
	std::string const menus = ReadSource("3dc/avp/win95/frontend/menus.cpp");
	Check(!page.empty() && !menus.empty(), "the screens' sources were found and read");

	std::string const joiner = Between(page, "static void DrawTheCheckCode(void)", "\n}\n");
	Check(Has(joiner, "if (!Net_RelayCheckCodeNow(digits))") && Has(joiner, "OnlineScreens_CheckCodeWords(digits, line, sizeof(line));") &&
		Has(joiner, "Words(OnlineScreens_Words(kOnlineWordsCheckCodeMeans)"), "a joiner's own code is shown, with what it's for, once its handshake has made one");

	std::string const asking = Between(page, "bool OnlinePage_DrawAsking(void)", "\n}\n");
	size_t const firstDraw = asking.find("DrawTheCheckCode();");
	Check(firstDraw != std::string::npos && asking.find("DrawTheCheckCode();", firstDraw + 1) != std::string::npos,
		"on the screen asking the host, and on the one saying the host let them in");

	std::string const host = Between(page, "bool OnlineHost_AskCheckCode(char *line, size_t size)", "\n}\n");
	Check(Has(host, "if (!hostIsAsked || !Net_JoinRequestCheckCode(askingWho, digits))") && Has(host, "OnlineScreens_CheckCodeWords(digits, line, size);"),
		"the host's code is the one for the joiner whose request it's answering");
	Check(Has(menus, "OnlineHost_AskCheckCode(checkCodeLine, sizeof(checkCodeLine));") &&
		Has(menus, "if (checkCodeLine[0])\n\t\t{\n\t\t\tRenderSmallMenuText(checkCodeLine, MENU_CENTREX, 58,") &&
		Has(menus, "OnlineScreens_Words(kOnlineWordsCheckCodeMeansHost)"), "and it's shown under the asking line, with what it's for");

	// And nothing covers it. The relay's news on the co-op host page waits while a join request's rows are up, its 8 s starting again, and
	// is shown once they've gone, where the asking line was (Secondary's second dry run of the weekend, R1-4: it was drawn at y 56, over
	// the code at 58). Added 2026-11-09 by Melanikus for the AvP port.
	std::string const news = Between(page, "bool OnlineHost_NewsLine(char *line, size_t size, bool held)", "\n}\n");
	size_t const heldAt = news.find("\tif (held)\n\t{\n\t\tnewsAt = Now();\n\t\treturn false;\n\t}");
	size_t const shownAt = news.find("Put(line, size, newsLine);");
	Check(heldAt != std::string::npos && shownAt != std::string::npos && heldAt < shownAt,
		"the relay's news waits while a join request is answered, its 8 s starting again, before anything is shown");
	Check(Has(menus, "bool const answering = answerRowsShown && AvPMenus.MenuElements == answerRows;") &&
		Has(menus, "OnlineHost_NewsLine(relayNews, sizeof(relayNews), answering))\n\t{\n\t\tRenderSmallMenuText(relayNews, MENU_CENTREX, 40,"),
		"so the co-op host page holds it back while it answers, and never draws it over the check code");

	// (Removed 2026-11-10 by Melanikus for the AvP port, #175 PR 3a: the answer for a host that's another test build of the v2 draft went
	// with the draft's same-commit rule.)
}

int main()
{
	TestTheTail();
	TestSafeNames();
	TestTheLevelColumn();
	TestFitting();
	TestSplitting();
	TestRows();
	TestSearch();
	TestPingAndWaits();
	TestWords();
	TestTheCheckCode();
	TestWhereTheyAreDrawn();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
