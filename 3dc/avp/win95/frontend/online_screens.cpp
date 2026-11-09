// The online-play screens' own sums and words. Written 2026-11-09 by Melanikus for the AvP port. online_screens.h says what each one is for.
//
// Nothing here draws or touches the network, so tests/online_screens_test.cpp builds this file alone and checks every function in it.

#include "online_screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- The spinning tail

// A number kept between lowest and highest
static int Between(int value, int lowest, int highest)
{
	return value < lowest ? lowest : (value > highest ? highest : value);
}

// A position rounded to the nearest whole pixel
static int Pixel(double value)
{
	return (int)floor(value + 0.5);
}

void OnlineTail_Shape(uint32_t ms, int centreX, int centreY, int size, OnlineTailPiece pieces[kOnlineTailPieces])
{
	const double kWholeTurn = 6.283185307179586;   // a whole circle, in the radians sin and cos take

	double const s = Between(size, kOnlineTailSmallest, kOnlineTailBiggest);
	double const middleX = Between(centreX, -kOnlineTailFarthest, kOnlineTailFarthest);
	double const middleY = Between(centreY, -kOnlineTailFarthest, kOnlineTailFarthest);

	// Where the blade is in its turn. The screen's y runs down, so a growing angle goes round clockwise, and the blade leads that way.
	double const lead = kWholeTurn * (double)(ms % kOnlineTailTurnMs) / kOnlineTailTurnMs;

	// The tail fills five sixths of the ring, so there's a gap behind its base: a full ring wouldn't look as if it were turning
	double const step = kWholeTurn * 5.0 / 6.0 / kOnlineTailPieces;

	for (int i = 0; i < kOnlineTailPieces; i++)
	{
		// 0 at the tip, 1 at the base. The tail curls in towards its tip, and grows thicker towards its base, as an alien's does.
		double const towardsBase = (double)i / (kOnlineTailPieces - 1);
		double const angle = lead - i * step;
		double const radius = s * (0.80 + 0.20 * towardsBase);
		double const outX = cos(angle), outY = sin(angle);   // the way out from the middle
		double const aheadX = -outY, aheadY = outX;          // the way the tail is turning, square to that
		double const atX = middleX + radius * outX;
		double const atY = middleY + radius * outY;

		OnlineTailPiece &piece = pieces[i];

		if (i == 0)
		{
			// The blade: a point ahead, a barb to each side, and a short heel behind. Its corners go round in the same order as a bone's:
			// behind, inside, ahead, outside.
			double const length = s * 0.55;
			double const width = s * 0.36;

			piece.corner[0].x = Pixel(atX - aheadX * length * 0.40);
			piece.corner[0].y = Pixel(atY - aheadY * length * 0.40);
			piece.corner[1].x = Pixel(atX - aheadX * length * 0.10 - outX * width * 0.5);
			piece.corner[1].y = Pixel(atY - aheadY * length * 0.10 - outY * width * 0.5);
			piece.corner[2].x = Pixel(atX + aheadX * length * 0.60);
			piece.corner[2].y = Pixel(atY + aheadY * length * 0.60);
			piece.corner[3].x = Pixel(atX - aheadX * length * 0.10 + outX * width * 0.5);
			piece.corner[3].y = Pixel(atY - aheadY * length * 0.10 + outY * width * 0.5);
			piece.brightness = 255;
			continue;
		}

		// A bone: a short block along the curl, three quarters of the gap to the next, so the bones are seen apart
		double const halfLong = radius * step * 0.75 * 0.5;
		double const halfWide = s * (0.07 + 0.11 * towardsBase) * 0.5;

		piece.corner[0].x = Pixel(atX - aheadX * halfLong - outX * halfWide);   // behind, inside
		piece.corner[0].y = Pixel(atY - aheadY * halfLong - outY * halfWide);
		piece.corner[1].x = Pixel(atX + aheadX * halfLong - outX * halfWide);   // ahead, inside
		piece.corner[1].y = Pixel(atY + aheadY * halfLong - outY * halfWide);
		piece.corner[2].x = Pixel(atX + aheadX * halfLong + outX * halfWide);   // ahead, outside
		piece.corner[2].y = Pixel(atY + aheadY * halfLong + outY * halfWide);
		piece.corner[3].x = Pixel(atX - aheadX * halfLong + outX * halfWide);   // behind, outside
		piece.corner[3].y = Pixel(atY - aheadY * halfLong + outY * halfWide);

		// Bright near the blade, fading towards the base, like the trail of something moving fast
		piece.brightness = 230 - (i - 1) * 11;
	}
}

// ---- Names from the network

void OnlineScreens_SafeName(const char *from, size_t mostBytes, char *to, size_t toSize)
{
	if (toSize == 0)
	{
		return;
	}

	size_t used = 0;

	for (size_t i = 0; from && i < mostBytes && used + 1 < toSize; i++)
	{
		// Read as a number from 0 to 255, so it's the same whether the compiler's char has a sign or not (the game is built with /J)
		unsigned char const letter = (unsigned char)from[i];

		if (letter == 0)
		{
			break;
		}

		to[used++] = (letter >= 32 && letter <= 126) ? (char)letter : '?';
	}

	to[used] = '\0';
}

// A letter's width as the font gives it, never below 0, and never so big that adding a row of them could overflow
static int LetterWidth(int (*widthOf)(int letter), char letter)
{
	return Between(widthOf((unsigned char)letter), 0, 1000);
}

void OnlineScreens_FitToWidth(char *text, int mostWidth, int (*widthOf)(int letter))
{
	if (!text || !widthOf)
	{
		return;
	}

	size_t const length = strlen(text);
	int width = 0;

	// strlen of a name the game made is small, but a long text can't make this overflow: each letter adds at most 1000
	for (size_t i = 0; i < length && width <= mostWidth; i++)
	{
		width += LetterWidth(widthOf, text[i]);
	}

	if (width <= mostWidth)
	{
		return;
	}

	// It's too wide: keep as many letters as fit with two dots after them. The dots take the place of letters that were there, so the
	// text never grows past the room it had.
	if (length < 2)
	{
		text[0] = '\0';
		return;
	}

	int const dots = 2 * LetterWidth(widthOf, '.');
	size_t keep = 0;
	int kept = 0;

	while (keep < length - 2)
	{
		int const next = LetterWidth(widthOf, text[keep]);

		if (kept + next + dots > mostWidth)
		{
			break;
		}

		kept += next;
		keep++;
	}

	if (dots > mostWidth)
	{
		text[0] = '\0';
		return;
	}

	text[keep] = '.';
	text[keep + 1] = '.';
	text[keep + 2] = '\0';
}

void OnlineScreens_SplitName(const char *sessionName, size_t mostBytes, char *name, size_t nameSize, char *players, size_t playersSize)
{
	size_t length = 0;

	while (sessionName && length < mostBytes && sessionName[length] != 0)
	{
		length++;
	}

	// The part in brackets is the last "(" to a ")" that ends the name
	size_t open = length;

	if (length >= 2 && sessionName[length - 1] == ')')
	{
		for (size_t i = length - 1; i > 0; i--)
		{
			if (sessionName[i - 1] == '(')
			{
				open = i - 1;
				break;
			}
		}
	}

	if (open == length)
	{
		OnlineScreens_SafeName(sessionName, length, name, nameSize);
		OnlineScreens_SafeName("", 0, players, playersSize);
		return;
	}

	// The name, without the spaces before its bracket
	size_t nameEnd = open;

	while (nameEnd > 0 && sessionName[nameEnd - 1] == ' ')
	{
		nameEnd--;
	}

	OnlineScreens_SafeName(sessionName, nameEnd, name, nameSize);
	OnlineScreens_SafeName(sessionName + open + 1, length - open - 2, players, playersSize);
}

// ---- The rows of the games page

bool OnlineScreens_SameGame(const OnlineGameKey &a, const OnlineGameKey &b)
{
	if (a.throughRelay != b.throughRelay)
	{
		return false;
	}

	if (a.throughRelay)
	{
		return memcmp(a.code, b.code, sizeof(a.code)) == 0;
	}

	return a.host == b.host && a.port == b.port;
}

int OnlineScreens_MergeRows(const OnlineRow *before, int beforeCount, const OnlineRow *now, int nowCount, OnlineRow *out, int mostOut)
{
	int made = 0;

	for (int i = 0; now && i < nowCount && made < mostOut; i++)
	{
		out[made++] = now[i];
	}

	for (int j = 0; before && j < beforeCount && made < mostOut; j++)
	{
		if (before[j].gone)
		{
			continue;
		}

		bool stillThere = false;

		for (int i = 0; now && i < nowCount && !stillThere; i++)
		{
			stillThere = OnlineScreens_SameGame(before[j].key, now[i].key);
		}

		if (!stillThere)
		{
			out[made] = before[j];
			out[made].gone = true;
			out[made].canJoin = false;
			out[made].session = -1;
			made++;
		}
	}

	return made;
}

int OnlineScreens_SelectAgain(const OnlineGameKey &selected, int selectedRow, const OnlineRow *rows, int count)
{
	if (!rows || count <= 0)
	{
		return -1;
	}

	for (int i = 0; i < count; i++)
	{
		if (OnlineScreens_SameGame(selected, rows[i].key))
		{
			return i;
		}
	}

	return Between(selectedRow, 0, count - 1);
}

// ---- Looking for games, and keeping the list fresh

// Whether the clock has reached a moment. The clock wraps round to 0 after about 49 days, so this looks at how far apart the two are,
// which is right as long as they're less than 24 days apart.
static bool HasCome(uint32_t now, uint32_t moment)
{
	return (uint32_t)(now - moment) < 0x80000000u;
}

void OnlineSearch_Start(OnlineSearch *search, uint32_t now)
{
	search->tries = 1;
	search->nextAskAt = now + kOnlineSearchEveryMs;
	search->givenUp = false;
}

OnlineSearchNews OnlineSearch_Frame(OnlineSearch *search, uint32_t now, bool gamesShown)
{
	if (search->givenUp || !HasCome(now, search->nextAskAt))
	{
		return kOnlineSearchNothing;
	}

	if (gamesShown)
	{
		// Games are on the page: keep them fresh for as long as the page is open, and if they all go, start counting the tries again
		search->tries = 1;
	}
	else if (search->tries >= kOnlineSearchMostTries)
	{
		search->givenUp = true;
		return kOnlineSearchGivenUp;
	}
	else
	{
		search->tries++;
	}

	search->nextAskAt = now + kOnlineSearchEveryMs;
	return kOnlineSearchAskAgain;
}

// Copies words into a buffer of size characters, always ending it with a 0
static void Put(char *words, size_t size, const char *what)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "%s", what);
	words[size - 1] = '\0';
}

void OnlineSearch_Words(const OnlineSearch *search, uint32_t now, int stage, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	if (search->givenUp)
	{
		Put(words, size, OnlineScreens_Words(kOnlineWordsNothingFound));
		return;
	}

	if (search->tries <= 1)
	{
		Put(words, size, OnlineScreens_Words(stage == 0 ? kOnlineWordsLookingHere : kOnlineWordsLookingList));
		return;
	}

	int const seconds = OnlineScreens_SecondsLeft(search->nextAskAt - kOnlineSearchEveryMs, now, kOnlineSearchEveryMs);

	_snprintf(words, size - 1, "%s. Asking again in %d s (%d of %d)", OnlineScreens_Words(kOnlineWordsNothingYet), seconds, search->tries,
		(int)kOnlineSearchMostTries);
	words[size - 1] = '\0';
}

// ---- The ping column

void OnlineScreens_PingWords(bool throughRelay, int joinerMs, int hostMs, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	if (!throughRelay)
	{
		Put(words, size, "-");
		return;
	}

	if (joinerMs < 0)
	{
		Put(words, size, "?");
		return;
	}

	int const total = Between(joinerMs, 0, kRelayListMostHostMs) + Between(hostMs, 0, kRelayListMostHostMs);

	_snprintf(words, size - 1, "about %d ms", total);
	words[size - 1] = '\0';
}

// ---- Joining, and being asked

int OnlineScreens_SecondsLeft(uint32_t startedAt, uint32_t now, uint32_t mostMs)
{
	uint32_t const gone = now - startedAt;

	if (gone >= mostMs)
	{
		return 0;
	}

	// Rounded up, so the last second shows as 1, never 0, until it's really over
	return (int)((mostMs - gone + 999) / 1000);
}

// One line for each of OnlineWords, in its order. The list's size is counted from its lines, so a line missing or extra stops the build
// (the static_assert below) instead of leaving a word empty.
static const char *const kWords[] =
{
	"Looking for games on this network",                          // kOnlineWordsLookingHere
	"Asking the list of games",                                   // kOnlineWordsLookingList
	"No games yet",                                               // kOnlineWordsNothingYet
	"No games found. Enter looks again",                          // kOnlineWordsNothingFound
	"Esc goes back",                                              // kOnlineWordsEscBack
	"Game",                                                       // kOnlineWordsColumnGame
	"Level",                                                      // kOnlineWordsColumnLevel
	"Players",                                                    // kOnlineWordsColumnPlayers
	"Region",                                                     // kOnlineWordsColumnRegion
	"Ping",                                                       // kOnlineWordsColumnPing
	"gone",                                                       // kOnlineWordsGone
	"full",                                                       // kOnlineWordsFull
	"this network",                                               // kOnlineWordsThisNetwork
	"Reaching the relay",                                         // kOnlineWordsReachingRelay
	"The relay is introducing you to the host",                   // kOnlineWordsRelayIntroducing
	"Esc gives up",                                               // kOnlineWordsEscGivesUp
	"You gave up joining",                                        // kOnlineWordsGaveUp
	"Asking the host to let you in",                              // kOnlineWordsAsking
	"The host let you in",                                        // kOnlineWordsLetIn
	"The host said no",                                           // kOnlineWordsSaidNo
	"The host didn't answer in time",                             // kOnlineWordsNoAnswer
	"Enter or Esc goes back to the list",                         // kOnlineWordsBackToList
	"Couldn't join",                                              // kOnlineWordsCouldNotJoin
	"that game has gone",                                         // kOnlineWordsGameHasGone
	"No games from the list",                                     // kOnlineWordsListFailed
	"Finding the nearest relay",                                  // kOnlineWordsRelayFinding
	"Your game is on the list",                                   // kOnlineWordsRelayListed
	"No relay answered: your game is open on this network only",  // kOnlineWordsRelayNone
	"Your game's room on the relay has gone"                      // kOnlineWordsRelayGone
};

static_assert(sizeof(kWords) / sizeof(kWords[0]) == kOnlineWordsCount, "kWords needs one line for each of OnlineWords, in its order");

const char *OnlineScreens_Words(OnlineWords which)
{
	if ((int)which < 0 || (int)which >= kOnlineWordsCount)
	{
		return "";
	}

	return kWords[which];
}

void OnlineScreens_WordsWithSeconds(OnlineWords which, int secondsLeft, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "%s (%d s)", OnlineScreens_Words(which), Between(secondsLeft, 0, 999));
	words[size - 1] = '\0';
}

// The name as it goes into a line: never more than a row's letters, and "" for none. Each line below has its own fixed pattern, and the
// name only ever goes in as a value, never as the pattern.
struct RowName
{
	char letters[kOnlineRowLetters];

	explicit RowName(const char *safeName)
	{
		Put(letters, sizeof(letters), safeName ? safeName : "");
	}
};

void OnlineScreens_AskingWords(const char *safeName, int secondsLeft, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "%s asks to join (%d s)", RowName(safeName).letters, Between(secondsLeft, 0, 999));
	words[size - 1] = '\0';
}

void OnlineScreens_LetInWords(const char *safeName, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "Let %s in", RowName(safeName).letters);
	words[size - 1] = '\0';
}

void OnlineScreens_SayNoWords(const char *safeName, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "Say no to %s", RowName(safeName).letters);
	words[size - 1] = '\0';
}
