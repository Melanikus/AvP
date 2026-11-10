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

// Copies words into a buffer of size characters, always ending it with a 0
static void Put(char *words, size_t size, const char *what)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "%s", what ? what : "");
	words[size - 1] = '\0';
}

// ---- The list page's Search box

void OnlineScreens_SearchText(const char *typed, size_t typedBytes, char *safe, size_t size)
{
	if (size == 0)
	{
		return;
	}

	// Made safe into room for the search's letters, then into the caller's room, which may be smaller still
	char letters[kOnlineSearchLetters + 1];
	OnlineScreens_SafeName(typed, typedBytes, letters, sizeof(letters));
	Put(safe, size, letters);
}

// A letter as a small one, for comparing without caring about capitals: only A to Z change, so nothing else can be made to match
static char Small(char letter)
{
	return (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : letter;
}

bool OnlineScreens_NameMatches(const char *safeName, const char *safeSearch)
{
	if (!safeSearch || safeSearch[0] == '\0')
	{
		return true;
	}

	if (!safeName)
	{
		return false;
	}

	// Both are short (a row's 39 letters, a search's 20), so trying each place in the name is quick
	for (size_t start = 0; safeName[start] != '\0'; start++)
	{
		size_t i = 0;

		while (safeSearch[i] != '\0' && safeName[start + i] != '\0' && Small(safeName[start + i]) == Small(safeSearch[i]))
		{
			i++;
		}

		if (safeSearch[i] == '\0')
		{
			return true;
		}
	}

	return false;
}

// ---- The fetching screen

// The server's name as it goes into a line: never more than a host name's letters, and "" for none
struct ServerName
{
	char letters[kRelayListHostLetters + 1];

	explicit ServerName(const char *safeServer)
	{
		Put(letters, sizeof(letters), safeServer);
	}
};

void OnlineScreens_FetchingWords(const char *safeServer, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "Fetching %s's open sessions", ServerName(safeServer).letters);
	words[size - 1] = '\0';
}

void OnlineScreens_ConnectedWords(int sessions, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	if (sessions <= 0)
	{
		Put(words, size, "Connected: no sessions open");
	}
	else if (sessions == 1)
	{
		Put(words, size, "Connected: 1 session open");
	}
	else
	{
		_snprintf(words, size - 1, "Connected: %d sessions open", Between(sessions, 2, 999));
		words[size - 1] = '\0';
	}
}

void OnlineScreens_CantConnectWords(const char *safeServer, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "Couldn't connect to %s", ServerName(safeServer).letters);
	words[size - 1] = '\0';
}

void OnlineScreens_CantFetchWords(const char *safeServer, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	_snprintf(words, size - 1, "Connected to %s, but couldn't fetch its open sessions", ServerName(safeServer).letters);
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
	"Esc goes back",                                              // kOnlineWordsEscBack
	"Enter tries again, Esc goes back",                           // kOnlineWordsTryAgain
	"No open sessions",                                           // kOnlineWordsNoSessions
	"No open sessions with that in their name",                   // kOnlineWordsNoneMatch
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
	"The host is answering someone else. Try again in a moment",  // kOnlineWordsHostBusy
	"Esc to answer",                                              // kOnlineWordsEscToAnswer
	"HOST, Relay:",                                               // kOnlineWordsHostingThrough
	"CLIENT, Relay:",                                             // kOnlineWordsJoinedThrough
	"a relay",                                                    // kOnlineWordsARelay
	"Finding the nearest relay",                                  // kOnlineWordsRelayFinding
	"Your game is on the list",                                   // kOnlineWordsRelayListed
	"No relay answered: your game is open on this network only",  // kOnlineWordsRelayNone
	"Your game's room on the relay has gone",                     // kOnlineWordsRelayGone
	"The host runs another test build of the v2 draft",           // kOnlineWordsOtherBuild
	// The check code, for friends and for strangers (the owner's ruling, 2026-11-09: always shown on both screens; friends compare it
	// outside the game; a stranger may go on without, told what that means; the game's own chat is never the way to compare, since it runs
	// over the very connection the code checks)
	"Friends compare it by voice or Discord, never in the game's chat: the same 6 digits means nobody is in between",  // kOnlineWordsCheckCodeMeans
	"Ask a friend for theirs by voice or Discord, never in the game's chat: the same 6 digits means nobody is in between",   // kOnlineWordsCheckCodeMeansHost
	"Not comparing is your choice: the game is still encrypted, but someone in the middle can't be ruled out"   // kOnlineWordsCheckCodeSkipped
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

void OnlineScreens_ListedWords(const char *safeRegion, int ms, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	if (!safeRegion || safeRegion[0] == '\0')
	{
		Put(words, size, OnlineScreens_Words(kOnlineWordsRelayListed));
		return;
	}

	char region[kRelayListRegionLetters + 1];
	Put(region, sizeof(region), safeRegion);

	_snprintf(words, size - 1, "%s, through %s, %d ms", OnlineScreens_Words(kOnlineWordsRelayListed), region, Between(ms, 0, kRelayListMostHostMs));
	words[size - 1] = '\0';
}

void OnlineScreens_PlayLineWords(bool isHost, const char *safeRegion, int ms, int serverMs, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	// The server's shown name or its region: room for the longer, a shown name's 32 letters
	char place[kRelayListShownNameLetters + 1];
	Put(place, sizeof(place), (safeRegion && safeRegion[0] != '\0') ? safeRegion : OnlineScreens_Words(kOnlineWordsARelay));

	OnlineWords const start = isHost ? kOnlineWordsHostingThrough : kOnlineWordsJoinedThrough;

	// Each time says what it measures (the owner, 2026-11-09): the host's is to the relay server and back; a joiner's goes to the host and
	// back, through the server, and its own time to the server and back, measured on the games page, follows when it has one
	if (ms < 0)
	{
		_snprintf(words, size - 1, "%s %s", OnlineScreens_Words(start), place);
	}
	else if (isHost)
	{
		_snprintf(words, size - 1, "%s %s, %d ms (round trip, host to server)", OnlineScreens_Words(start), place,
			Between(ms, 0, kRelayListMostHostMs));
	}
	else if (serverMs < 0)
	{
		_snprintf(words, size - 1, "%s %s, %d ms (round trip, client to host through server)", OnlineScreens_Words(start), place,
			Between(ms, 0, kRelayListMostHostMs));
	}
	else
	{
		_snprintf(words, size - 1, "%s %s, %d ms (round trip, client to host through server), %d ms round trip to server",
			OnlineScreens_Words(start), place, Between(ms, 0, kRelayListMostHostMs), Between(serverMs, 0, kRelayListMostHostMs));
	}

	words[size - 1] = '\0';
}

void OnlineScreens_RoomGoneWords(const char *safeWhy, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	if (!safeWhy || safeWhy[0] == '\0')
	{
		Put(words, size, OnlineScreens_Words(kOnlineWordsRelayGone));
		return;
	}

	char why[64];
	Put(why, sizeof(why), safeWhy);

	_snprintf(words, size - 1, "%s: %s", OnlineScreens_Words(kOnlineWordsRelayGone), why);
	words[size - 1] = '\0';
}

// Added 2026-11-09 by Melanikus for the AvP port: see online_screens.h
void OnlineScreens_CheckCodeWords(const char *digits, char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	words[0] = '\0';

	// Exactly 6 digits, and the 0 after them: anything else is no code at all
	for (int i = 0; i < 6; i++)
	{
		if (!digits || digits[i] < '0' || digits[i] > '9')
		{
			return;
		}
	}

	if (digits[6] != '\0')
	{
		return;
	}

	_snprintf(words, size - 1, "Check code: %.3s %.3s", digits, digits + 3);
	words[size - 1] = '\0';
}
