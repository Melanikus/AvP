// The rules of the co-op campaign lobby, the page where a host chooses a campaign while others wait. Written 2026-11-07 by Melanikus
// for the AvP port. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten).
//
// WHAT THE LOBBY IS
//
// In every other kind of game the host is already loading a level when anyone joins. In the lobby the host sits on a menu page with
// strangers connected and no level loaded, and most of the network code expects a level. So the lobby has rules of its own:
// - only a short list of kinds of message is taken at all (pldnet.cpp's two lists), and anything else is refused before it's read;
// - the host's page is the truth: a joiner's copy of the rows comes only from the host's choice message, each number checked here;
// - the choices are numbered, so an old one can never replace a newer one, and nothing is taken after the host has started.
//
// WHAT THE TWO MESSAGES SAY
//
// The host says what's chosen, whenever a row changes, and again every second for anyone who has just arrived. A joiner answers
// whether it has the map. For a custom map the host's words are a name, a size and a sum of the file:
// - the name is only ever looked up among the maps this machine has already listed, and never opened as a path;
// - the size and the sum are only compared, and never used to size anything;
// - no map file is ever sent. The file a joiner plays is its own, and goes through the game's hardened loader like any other.
//
// The sum is the file's MD5, which tells two honest copies apart. It proves nothing about a file somebody made to match, and it
// doesn't have to: that's what the loader's checks are for.
//
// Nothing here touches the game, so tests/coop_lobby_test.cpp can try every case.

#ifndef _coop_lobby_h_
#define _coop_lobby_h_ 1

#include <string.h>

#include "custom_map_name.h"

enum
{
	kCoopChoiceFirstCampaign = 1,   // the Marine's, then the Predator's, then the Alien's
	kCoopChoiceCustomCampaign = 4,  // one custom map. pldnet.cpp stops the build if these don't match env_info.h
	kCoopChoiceMostSpecies = 2,     // 0 Marine, 1 Predator, 2 Alien
	kCoopChoiceMostDifficulty = 2,
	kCoopMapNameBytes = 40,
	kCoopMapSumBytes = 16
};

static_assert(kCoopMapNameBytes == kCustomMapNameMost + 1, "the message has to hold exactly the longest name a custom map can have");

// Where this machine stands with the lobby. The same three numbers go in the description of a game that others find: see Net_AddSession.
enum CoopLobbyState
{
	kCoopLobbyNone = 0,       // no lobby: every kind of message goes where it always has
	kCoopLobbyOpen = 1,       // the host is on the page, and joiners are waiting
	kCoopLobbyUnderWay = 2    // the host has started, and a joiner who's just arrived is on the page until it hears so
};

// A game's description comes from anyone who answers a search, so this is a number to check like any other
inline bool CoopLobby_StateIsReal(int state)
{
	return state >= kCoopLobbyNone && state <= kCoopLobbyUnderWay;
}

// The three answers a joiner can give
enum
{
	kCoopMapIsHere = 0,
	kCoopMapIsMissing = 1,
	kCoopMapIsDifferent = 2,
	kCoopMostMapAnswer = 2
};

#pragma pack(push, 1)

// From the host to everyone: what's chosen. This is what a joiner's page shows.
typedef struct netmessage_coopcampaignchoice
{
	unsigned int number;             // goes up by one with each change of a row
	unsigned char campaign;
	unsigned char place;             // which of the campaign's levels to start at
	unsigned char species;           // who plays: for a stock campaign, always that campaign's own
	unsigned char difficulty;
	unsigned char keepKit;           // 1 to keep what's carried between levels. See campaign_kit.h
	unsigned char friendlyFireOff;
	unsigned char started;           // 1 once the host has pressed Start: everyone who has the map goes
	int rollNumber;                  // the one number for the whole game. See shared_roll.h. Any number at all is safe
	char mapName[kCoopMapNameBytes]; // for a custom map. Nothing at all for a stock campaign
	unsigned int mapSize;
	unsigned char mapSum[kCoopMapSumBytes];
} NETMESSAGE_COOPCAMPAIGNCHOICE;

// From a joiner to the host: which choice this answers, and whether this machine has the map
typedef struct netmessage_coopmapanswer
{
	unsigned int number;
	unsigned char answer;
} NETMESSAGE_COOPMAPANSWER;

#pragma pack(pop)

// THE PLAYERS A HOST HAS REMOVED. A lobby stays open to anyone who can reach it, so a host needs a way to get rid of someone. Only the host's
// own page can do it: no message asks for it, so no joiner can cause it. A removed player's address can't come back into this game, and the
// list is kept until the game closes, so that nobody removed can walk in after Start. It's small, and when it's full the oldest goes.
// Two players behind one address go together. A player whose address changes can come back, and the host can remove them again.
enum
{
	kCoopMostRemoved = 32
};

// The word that goes with the disconnection, so a removed player's page can say why. It's only a word from the host, and all it can do is say that.
static const unsigned int kCoopRemovedByTheHost = 0x524D5644;

struct CoopRemovedList
{
	unsigned int addresses[kCoopMostRemoved];
	int count;
	int next;
};

inline bool CoopRemoved_Has(CoopRemovedList const &list, unsigned int address)
{
	for (int i = 0; i < list.count; i++)
	{
		if (list.addresses[i] == address)
		{
			return true;
		}
	}

	return false;
}

inline void CoopRemoved_Add(CoopRemovedList &list, unsigned int address)
{
	if (CoopRemoved_Has(list, address))
	{
		return;
	}

	list.addresses[list.next] = address;
	list.next = (list.next + 1) % kCoopMostRemoved;

	if (list.count < kCoopMostRemoved)
	{
		list.count++;
	}
}

inline void CoopRemoved_Forget(CoopRemovedList &list)
{
	list.count = 0;
	list.next = 0;
}

// Why a message is refused. Each reason has words of its own, so a test can tell exactly which check refused a message.
static const char *const kCoopRefusedCampaign = "a campaign choice named a campaign that isn't one";
static const char *const kCoopRefusedPlace = "a campaign choice named a level its campaign hasn't got";
static const char *const kCoopRefusedSpecies = "a campaign choice named a species that isn't one";
static const char *const kCoopRefusedCampaignSpecies = "a campaign choice named a species its campaign doesn't play";
static const char *const kCoopRefusedDifficulty = "a campaign choice had a difficulty that isn't one";
static const char *const kCoopRefusedKeepKit = "a campaign choice had a kit rule that isn't one";
static const char *const kCoopRefusedFriendlyFire = "a campaign choice had a friendly fire setting that isn't one";
static const char *const kCoopRefusedStarted = "a campaign choice had a start flag that isn't one";
static const char *const kCoopRefusedNameHasNoEnd = "a campaign choice named a custom map with no end to its name";
static const char *const kCoopRefusedName = "a campaign choice named a custom map with a name that breaks the rule";
static const char *const kCoopRefusedStockHasAMap = "a campaign choice for a stock campaign carried a custom map";
static const char *const kCoopRefusedOlder = "a campaign choice was older than the one already taken";
static const char *const kCoopRefusedAfterStart = "a campaign choice came after the host had started";
static const char *const kCoopRefusedAnswerNoLobby = "a map answer came with no lobby open";
static const char *const kCoopRefusedAnswerStranger = "a map answer came from someone who isn't in the game";
static const char *const kCoopRefusedAnswerKind = "a map answer that isn't one of the three";
static const char *const kCoopRefusedAnswerFuture = "a map answer to a choice the host hasn't made";
static const char *const kCoopRefusedRemoved = "a player the host removed tried to come back";
static const char *const kCoopRefusedPartInLobby = "a part of a kind the lobby doesn't take";
static const char *const kCoopRefusedPartBeforeJoining = "a part of a game under way, which this machine hasn't joined yet";

// Is this byte anywhere in the field?
inline bool CoopField_Has(const char *field, size_t size, char wanted)
{
	for (size_t i = 0; i < size; i++)
	{
		if (field[i] == wanted)
		{
			return true;
		}
	}

	return false;
}

// Everything in a field nothing?
inline bool CoopField_IsEmpty(const void *field, size_t size)
{
	const unsigned char *const bytes = (const unsigned char *)field;

	for (size_t i = 0; i < size; i++)
	{
		if (bytes[i] != 0)
		{
			return false;
		}
	}

	return true;
}

// Why this choice can't be taken, or 0 if every number in it is good. levelsInCampaign is how many levels its campaign really has.
inline const char *CoopChoice_WhyRefused(NETMESSAGE_COOPCAMPAIGNCHOICE const &choice, int levelsInCampaign)
{
	if (choice.campaign < kCoopChoiceFirstCampaign || choice.campaign > kCoopChoiceCustomCampaign)
	{
		return kCoopRefusedCampaign;
	}

	if ((int)choice.place >= levelsInCampaign)
	{
		return kCoopRefusedPlace;
	}

	if (choice.species > kCoopChoiceMostSpecies)
	{
		return kCoopRefusedSpecies;
	}

	if (choice.campaign != kCoopChoiceCustomCampaign && choice.species != choice.campaign - kCoopChoiceFirstCampaign)
	{
		return kCoopRefusedCampaignSpecies;
	}

	if (choice.difficulty > kCoopChoiceMostDifficulty)
	{
		return kCoopRefusedDifficulty;
	}

	if (choice.keepKit > 1)
	{
		return kCoopRefusedKeepKit;
	}

	if (choice.friendlyFireOff > 1)
	{
		return kCoopRefusedFriendlyFire;
	}

	if (choice.started > 1)
	{
		return kCoopRefusedStarted;
	}

	if (choice.campaign == kCoopChoiceCustomCampaign)
	{
		if (!CoopField_Has(choice.mapName, sizeof(choice.mapName), 0))
		{
			return kCoopRefusedNameHasNoEnd;
		}

		if (!CustomMapName_IsSafe(choice.mapName, sizeof(choice.mapName)))
		{
			return kCoopRefusedName;
		}
	}
	else if (!CoopField_IsEmpty(choice.mapName, sizeof(choice.mapName)) || choice.mapSize != 0 || !CoopField_IsEmpty(choice.mapSum, sizeof(choice.mapSum)))
	{
		return kCoopRefusedStockHasAMap;
	}

	return 0;
}

// What to do with a choice's number. The first after joining is always taken, and a repeat is how a joiner who has just arrived hears
// the choice at all, so it costs nothing. A choice only goes forward, and once the host has started that's the game: one join, one game.
enum CoopChoiceStep
{
	kCoopChoiceTakeIt,
	kCoopChoiceIsARepeat,
	kCoopChoiceIsOlder,
	kCoopChoiceIsAfterTheStart
};

inline CoopChoiceStep CoopChoice_Step(unsigned int number, bool anyTaken, unsigned int numberTaken, bool startedIsTaken)
{
	if (anyTaken && number == numberTaken)
	{
		return kCoopChoiceIsARepeat;
	}

	if (startedIsTaken)
	{
		return kCoopChoiceIsAfterTheStart;
	}

	if (anyTaken && number < numberTaken)
	{
		return kCoopChoiceIsOlder;
	}

	return kCoopChoiceTakeIt;
}

// A joiner's answer about the map. It's only given sizes and sums to compare, and it does nothing else with them.
inline int CoopMap_Answer(bool thisMachineHasIt, unsigned int mySize, const unsigned char *mySum, unsigned int theirSize, const unsigned char *theirSum)
{
	if (!thisMachineHasIt)
	{
		return kCoopMapIsMissing;
	}

	if (mySize != theirSize || memcmp(mySum, theirSum, kCoopMapSumBytes) != 0)
	{
		return kCoopMapIsDifferent;
	}

	return kCoopMapIsHere;
}

// Why the host won't take this answer, or 0 if it's good. An answer to an earlier choice is good, and honest: the joiner hadn't heard yet.
// The host just doesn't count it. isOpen and isKnown are the host's own knowledge: is the lobby open, and is this sender in the game?
inline const char *CoopAnswer_WhyRefused(NETMESSAGE_COOPMAPANSWER const &answer, bool isOpen, bool isKnown, unsigned int numberNow)
{
	if (!isOpen)
	{
		return kCoopRefusedAnswerNoLobby;
	}

	if (!isKnown)
	{
		return kCoopRefusedAnswerStranger;
	}

	if (answer.answer > kCoopMostMapAnswer)
	{
		return kCoopRefusedAnswerKind;
	}

	if (answer.number > numberNow)
	{
		return kCoopRefusedAnswerFuture;
	}

	return 0;
}

// A ROW OF NAMES. A row's words start in the middle of the screen, so there's room for about 54 letters before its edge, and seven names of 12
// letters don't fit. So the list stops in time to say how many more there are: "Ripley, Hicks and 5 more".
static const size_t kCoopRowMostLetters = 44;

// How long a name is, counting no further than its room: a name from another machine might have no end
inline size_t CoopName_Letters(const char *name, size_t nameBytes)
{
	size_t letters = 0;

	while (letters < nameBytes && name[letters] != 0)
	{
		letters++;
	}

	return letters;
}

// Puts letters on the end of words, as many as there's room for, and counts every one either way, so the caller can tell how
// long the words would have been
inline void CoopNames_Put(char *words, size_t room, size_t *wanted, const char *letters, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (*wanted + 1 < room)
		{
			words[*wanted] = letters[i];
		}

		(*wanted)++;
	}
}

// "and 5 more", with a space before it, for this many names left out. andMore is the game's own words, from language.txt, with
// one %d where the number goes. Anyone can change that file, so the words are never handed to printf as its format, where a %s
// or %n would crash the game: the number is written on its own and put in place of the words' one %d. Words without exactly one
// %d are shown as they are, with the number after them. Gives back how many letters the words take, even if they didn't fit.
inline size_t CoopNames_MoreWords(char *words, size_t room, const char *andMore, int more)
{
	if (more <= 0 || room == 0)
	{
		return 0;
	}

	// The number's digits, written backwards and then read the right way round
	char backwards[12];
	char number[12];
	size_t digits = 0;

	for (int left = more; left > 0 && digits < sizeof(backwards); left /= 10)
	{
		backwards[digits++] = (char)('0' + left % 10);
	}

	for (size_t i = 0; i < digits; i++)
	{
		number[i] = backwards[digits - 1 - i];
	}

	const char *const text = andMore ? andMore : "";
	size_t const textLetters = strlen(text);

	// Where the words' %d is, and how many there are
	size_t markAt = 0;
	int marks = 0;

	for (size_t i = 0; i + 1 < textLetters; i++)
	{
		if (text[i] == '%' && text[i + 1] == 'd')
		{
			markAt = i;
			marks++;
		}
	}

	size_t wanted = 0;

	CoopNames_Put(words, room, &wanted, " ", 1);

	// What comes after the %d starts two letters past it. The search above only finds a %d that ends inside the words, and the
	// check is made again here, where the length after it is worked out, so that length can never come out below nothing.
	size_t const afterMark = markAt + 2;

	if (marks == 1 && afterMark <= textLetters)
	{
		CoopNames_Put(words, room, &wanted, text, markAt);
		CoopNames_Put(words, room, &wanted, number, digits);
		CoopNames_Put(words, room, &wanted, text + afterMark, textLetters - afterMark);
	}
	else
	{
		CoopNames_Put(words, room, &wanted, text, textLetters);
		CoopNames_Put(words, room, &wanted, " ", 1);
		CoopNames_Put(words, room, &wanted, number, digits);
	}

	words[wanted < room ? wanted : room - 1] = 0;
	return wanted;
}

// The names, in order, with commas between, and "and 5 more" for any that would take the row past mostLetters. Always ends within its room.
inline void CoopNames_Line(char *line, size_t room, const char *const *names, int nameCount, size_t nameBytes, const char *andMore, size_t mostLetters)
{
	if (room == 0)
	{
		return;
	}

	char more[64];

	// The most names that fit, with the words for any left out after them. Every count is tried, because one more name can mean no words
	// for the rest at all.
	int shown = 0;
	size_t letters = 0;

	for (int i = 0; i < nameCount; i++)
	{
		letters += (i > 0 ? 2 : 0) + CoopName_Letters(names[i], nameBytes);

		if (letters + CoopNames_MoreWords(more, sizeof(more), andMore, nameCount - i - 1) <= mostLetters)
		{
			shown = i + 1;
		}
	}

	size_t used = 0;

	for (int i = 0; i < shown; i++)
	{
		for (size_t c = 0; c < (i > 0 ? 2u : 0u) && used + 1 < room; c++)
		{
			line[used++] = ", "[c];
		}

		for (size_t c = 0; c < CoopName_Letters(names[i], nameBytes) && used + 1 < room; c++)
		{
			line[used++] = names[i][c];
		}
	}

	size_t const moreLetters = CoopNames_MoreWords(more, sizeof(more), andMore, nameCount - shown);

	// With no names before it, it needs no space at its start
	for (size_t c = shown > 0 ? 0 : 1; c < moreLetters && c < sizeof(more) && more[c] != 0 && used + 1 < room; c++)
	{
		line[used++] = more[c];
	}

	line[used] = 0;
}

// WHERE THE ROWS GO ON A CO-OP PAGE. A page has a picture at the top and a help line at the bottom, and the host's page grows by a row for each
// player who joins, so a full game has more rows than fit between the two. Being able to read the rows comes first. They sit in the middle of
// the page, as on any other, unless that would put them on the picture. Then they move down under it, and if that would put them on the help
// line, the picture isn't shown. All of this is in the menu's own units, where 480 is the bottom of the screen.
static const int kCoopRowsMostLow = 440;     // the last row has to end by here, above the help line
static const int kCoopRowsUnderPicture = 2;  // the gap left under a picture

struct CoopPageLayout
{
	int rowsTop;
	bool showPicture;
};

// pictureBottom is where the picture would end, or 0 if there's no picture
inline CoopPageLayout CoopPage_Layout(int rowsHeight, int pageCentre, int pictureBottom)
{
	CoopPageLayout layout;

	layout.rowsTop = pageCentre - rowsHeight / 2;
	layout.showPicture = pictureBottom > 0;

	int const underThePicture = pictureBottom + kCoopRowsUnderPicture;

	if (layout.showPicture && layout.rowsTop < underThePicture)
	{
		if (underThePicture + rowsHeight <= kCoopRowsMostLow)
		{
			layout.rowsTop = underThePicture;
		}
		else
		{
			layout.showPicture = false;
		}
	}

	return layout;
}

#endif
