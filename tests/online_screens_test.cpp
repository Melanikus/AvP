// The test of the online-play screens' sums and words (3dc/avp/win95/frontend/online_screens.cpp). Written 2026-11-09 by Melanikus for the
// AvP port.
//
// WHAT IT'S FOR: the games page shows names that come from other people's computers, so they're hostile (AVP Tao 3). This checks that a name
// can only ever be drawn in the letters the game's fonts have, and never past its column. It checks the spinning tail stays in its box, turns
// the right way at the same speed at any frame rate, and that each of its pieces is a shape the game can draw. And it checks the page keeps
// the selected game while the list refreshes, that the search gives up after its tries, and that the waits count down right, even when the
// clock wraps round.
//
// Nothing here uses the network or the game.

#include "online_screens.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

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
			std::string const original(length, 'm');
			strcpy(text, original.c_str());
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

	OnlineRow rows[3] = { d, b, a };
	Check(OnlineScreens_SelectAgain(b.key, 0, rows, 3) == 1, "the selection follows its game to its new row");
	Check(OnlineScreens_SelectAgain(c.key, 7, rows, 3) == 2, "when its game has gone, it stays on the same row number, kept inside the rows");
	Check(OnlineScreens_SelectAgain(c.key, -4, rows, 3) == 0, "and never above the first row");
	Check(OnlineScreens_SelectAgain(a.key, 0, rows, 0) == -1, "with no rows, nothing is selected");
}

// ---- The search

static void TestSearch()
{
	puts("Looking for games");

	OnlineSearch search;
	OnlineSearch_Start(&search, 1000);
	Check(search.tries == 1 && !search.givenUp, "the first ask is when the page opens");
	Check(OnlineSearch_Frame(&search, 1000, false) == kOnlineSearchNothing && OnlineSearch_Frame(&search, 5999, false) == kOnlineSearchNothing,
		"nothing more is asked for 5 seconds");
	Check(OnlineSearch_Frame(&search, 6000, false) == kOnlineSearchAskAgain && search.tries == 2, "at 5 seconds it asks again");

	uint32_t now = 6000;
	int asks = 2;
	OnlineSearchNews news = kOnlineSearchNothing;

	while (news != kOnlineSearchGivenUp && now < 1000000)
	{
		now += 100;
		news = OnlineSearch_Frame(&search, now, false);
		asks += news == kOnlineSearchAskAgain ? 1 : 0;
	}

	Check(news == kOnlineSearchGivenUp && asks == kOnlineSearchMostTries, "with nothing found it asks 12 times in all, then gives up");
	Check(now >= 1000 + (uint32_t)kOnlineSearchMostTries * kOnlineSearchEveryMs, "the 12 asks take a minute: never faster than one every 5 s");
	Check(OnlineSearch_Frame(&search, now + 60000, false) == kOnlineSearchNothing, "once it's given up, it asks nothing more until Enter starts it again");

	OnlineSearch_Start(&search, 0);
	int kept = 0;

	for (uint32_t t = 0; t <= 600000; t += 50)
	{
		kept += OnlineSearch_Frame(&search, t, true) == kOnlineSearchAskAgain ? 1 : 0;
	}

	Check(kept == 120 && !search.givenUp, "while games are shown, the list is kept fresh every 5 s, for as long as the page is open");

	OnlineSearch_Start(&search, 0);
	OnlineSearch_Frame(&search, 5000, false);
	OnlineSearch_Frame(&search, 10000, false);
	OnlineSearch_Frame(&search, 15000, true);
	Check(search.tries == 1, "when games come after a few empty asks, the count of tries starts again, so a page they leave gets all 12 again");

	OnlineSearch_Start(&search, 0xFFFFF000u);
	Check(OnlineSearch_Frame(&search, 0xFFFFFFFFu, false) == kOnlineSearchNothing, "near the clock's end, the ask isn't due before its 5 s");
	Check(OnlineSearch_Frame(&search, 0xFFFFF000u + 5000u, false) == kOnlineSearchAskAgain, "and is due after them, once the clock has wrapped round to 0");

	char words[96];
	OnlineSearch_Start(&search, 1000);
	OnlineSearch_Words(&search, 1000, 0, words, sizeof(words));
	Check(strcmp(words, "Looking for games on this network") == 0, "the first line says it's looking on this network");
	OnlineSearch_Words(&search, 1000, 1, words, sizeof(words));
	Check(strcmp(words, "Asking the list of games") == 0, "then that it's asking the list");

	OnlineSearch_Frame(&search, 6000, false);
	OnlineSearch_Frame(&search, 11000, false);
	OnlineSearch_Words(&search, 13000, 1, words, sizeof(words));
	Check(strcmp(words, "No games yet. Asking again in 3 s (3 of 12)") == 0, "between asks it says when it asks again, and which try that is");

	search.givenUp = true;
	OnlineSearch_Words(&search, 13000, 1, words, sizeof(words));
	Check(strcmp(words, "No games found. Enter looks again") == 0, "after the last try it says nothing was found, and how to look again");

	OnlineSearch_Words(&search, 13000, 1, words, 1);
	Check(words[0] == '\0', "a room of one byte gets only the 0, for the search's line");
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

	OnlineScreens_PlayLineWords(true, "", 12, -1, line, sizeof(line));
	Check(strcmp(line, "HOST, Relay: a relay, 12 ms (round trip, host to server)") == 0, "a region that isn't known is said as a relay");

	OnlineScreens_PlayLineWords(false, longRegion.c_str(), 7, -1, line, sizeof(line));
	Check(strlen(line) == strlen("CLIENT, Relay: , 7 ms (round trip, client to host through server)") + kRelayListRegionLetters,
		"in play too, a region is cut to 16 letters");

	char small[20];
	OnlineScreens_PlayLineWords(false, "nyc3", 290, 146, small, sizeof(small));
	Check(small[sizeof(small) - 1] == '\0' && strlen(small) == sizeof(small) - 1, "a line too long for its room is cut, and still ends with a 0");
}

int main()
{
	TestTheTail();
	TestSafeNames();
	TestFitting();
	TestSplitting();
	TestRows();
	TestSearch();
	TestPingAndWaits();
	TestWords();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
