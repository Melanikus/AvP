// Modified 2026-11-06 by Melanikus for the AvP port:
//   the difficulty a host says a campaign is at is checked
// Checks on what arrives from other players' machines. Written 2026-11-06 by Melanikus for the AvP port.
//
// Anyone on the network can send anything, so what's in a message is only something someone said until it's been checked. These are kept apart from the game's network code, and need none of its headers, so that a test can run them on their own.

#ifndef NET_CHECKS_H
#define NET_CHECKS_H

#include <stddef.h>
#include <string.h>

// Can a press message from another machine press this kind of thing? Only the two kinds of switch can be pressed. A switch with a link group is a different kind of object from a plain one, and the
// machine that sends the message sends it for both. The caller says which numbers those kinds have, so that this file needs nothing from the game.
inline bool NetPressMessageCanPress(int kind, int plainSwitchKind, int linkSwitchKind)
{
	return kind == plainSwitchKind || kind == linkSwitchKind;
}

// Has everyone else in the player list said they're ready? Each place in the list holds a player or it's empty. Your own place doesn't count, because you know about yourself.
inline bool NetEveryoneElseIsReady(const bool *placeHasPlayer, const bool *placeIsReady, int places, int myPlace)
{
	for (int place = 0; place < places; place++)
	{
		if (place != myPlace && placeHasPlayer[place] && !placeIsReady[place])
		{
			return false;
		}
	}

	return true;
}

// Is this number a place in a table with this many places? The first place is 0.
inline bool NetNumberFitsTable(int number, int places)
{
	return number >= 0 && number < places;
}

// For a log line that any peer or LAN machine can cause once per packet or datagram: counts the event in count and
// says whether to log this one. The first kDropsLoggedInFull are logged, then only the 8th, 16th, 32nd, ... (each
// power of two), so a flood adds a few dozen lines to avp_log.txt at most, each with the running total.
static const unsigned long long kDropsLoggedInFull = 5;

// HOW MANY TIMES EACH REASON HAS STOPPED A MESSAGE. Each reason counts by itself, so that a flood of one bad message can't use up the log and hide a different one. The reasons are fixed text inside this program, so the list can't grow without limit, and anything past its end shares the last place.
static const int kMostRefusalReasons = 40;
static const unsigned long long kRefusalsLoggedInFull = 20;

struct NetRefusalCounts
{
	const char *reasons[kMostRefusalReasons];
	unsigned long long counts[kMostRefusalReasons];
};

// Counts one more of this reason, and answers how many there have been
inline unsigned long long NetCountRefusal(NetRefusalCounts &all, const char *why)
{
	for (int i = 0; i < kMostRefusalReasons; i++)
	{
		if (all.reasons[i] == 0)
		{
			all.reasons[i] = why;
		}

		if (i == kMostRefusalReasons - 1 || strcmp(all.reasons[i], why) == 0)
		{
			return ++all.counts[i];
		}
	}

	return 0;
}

// The first few of a reason go in the log, then only the 32nd, the 64th and so on, because anyone can cause these as fast as they like
inline bool NetLogThisRefusal(unsigned long long count)
{
	return count <= kRefusalsLoggedInFull || (count & (count - 1)) == 0;
}

// A level number the host gave, against the length of the list for the game type the host gave. They come in two different messages.
inline bool NetLevelIsOnItsList(int level, int listLength)
{
	return level >= 0 && level < listLength;
}

inline bool LogThisDrop(unsigned long long &count)
{
	count++;
	return (count <= kDropsLoggedInFull) || ((count & (count - 1)) == 0);
}

// TROUBLE THAT GOES ON ALL LEVEL. Some trouble, once it starts, comes back with every copy of the same message: the host's switches that can't
// be matched with this machine's come ten times a second. The log hears of the first one in a level, and of how many there were once the
// level is over, so a tester's log says what happened without filling up.
struct NetLevelCount
{
	unsigned long long count;
};

// Counts one more, and says whether it's the first in this level, the one the log hears of
inline bool NetLevelCount_One(NetLevelCount &level)
{
	return ++level.count == 1;
}

// The level is over: gives back how many there were, and starts the count again for the next level
inline unsigned long long NetLevelCount_LevelIsOver(NetLevelCount &level)
{
	unsigned long long const howMany = level.count;

	level.count = 0;
	return howMany;
}

// WHAT A GAME MESSAGE IS MADE OF. One message holds any number of parts, one after another. Each starts with a byte saying what kind it is, and each kind has its own length, so a part can only be found by getting all the ones before it right. A message
// has to be checked from end to end before any of it is believed, and if any part is wrong all of it goes, because nothing after a wrong part can be trusted to be where it seems.
struct NetPartRule
{
	unsigned char kind;       // which kind this is, so that the compiler can check the table is in the right order
	unsigned short bytes;     // how long it is after that first byte. For one whose length varies, the least it can be
	bool lengthVaries;        // if so, it's asked how long this one is
	bool onlyFromTheHost;     // the host's word on how things are, like the scores. Anyone else saying these would be believed by whoever hears it
	unsigned short mostEachSecond; // how many of these a client may send in a second, for a kind that costs every machine something. 0 means there's no limit.
};

enum NetPartsAnswer
{
	kNetPartsGood,
	kNetPartsUnknownKind,
	kNetPartsOnlyFromTheHost,
	kNetPartsTooShort
};

// How long this part really is, for a kind whose length varies. It's told how many bytes there are, and it has at least the rule's bytes to look at. 0 means it doesn't fit.
typedef size_t (*NetPartLength)(unsigned char kind, const unsigned char *part, size_t bytesLeft);

inline NetPartsAnswer NetCheckParts(const unsigned char *parts, size_t size, const NetPartRule *rules, size_t ruleCount, bool fromTheHost, NetPartLength lengthOf)
{
	size_t at = 0;

	while (at < size)
	{
		unsigned char const kind = parts[at];

		at++;

		if (kind >= ruleCount)
		{
			return kNetPartsUnknownKind;
		}

		NetPartRule const &rule = rules[kind];

		if (rule.onlyFromTheHost && !fromTheHost)
		{
			return kNetPartsOnlyFromTheHost;
		}

		size_t const bytesLeft = size - at;
		size_t length = rule.bytes;

		if (length > bytesLeft)
		{
			return kNetPartsTooShort;
		}

		if (rule.lengthVaries)
		{
			length = lengthOf(kind, parts + at, bytesLeft);

			if (length == 0 || length > bytesLeft)
			{
				return kNetPartsTooShort;
			}
		}

		at += length;
	}

	return kNetPartsGood;
}

// WHAT A CLIENT MAY SPEND. Some kinds of part make every machine in the game do something costly, and one message can hold hundreds of them. Each client has its own count. A message that would go over is refused whole, and spends nothing. This is only asked about a message that has already passed NetCheckParts.
struct NetPartsSpent
{
	unsigned int secondStarted;
	int used[256];
};

inline bool NetPartsFitTheBudget(const unsigned char *parts, size_t size, const NetPartRule *rules, size_t ruleCount, NetPartLength lengthOf, NetPartsSpent &spent, unsigned int now)
{
	if (now - spent.secondStarted >= 1000)
	{
		spent.secondStarted = now;
		memset(spent.used, 0, sizeof(spent.used));
	}

	int wanted[256];
	size_t at = 0;

	memset(wanted, 0, sizeof(wanted));

	while (at < size)
	{
		unsigned char const kind = parts[at];

		at++;

		// None of these can happen after NetCheckParts, and they keep this safe if that ever changes
		if (kind >= ruleCount)
		{
			return false;
		}

		NetPartRule const &rule = rules[kind];
		size_t const bytesLeft = size - at;
		size_t length = rule.bytes;

		if (length > bytesLeft)
		{
			return false;
		}

		if (rule.lengthVaries)
		{
			length = lengthOf(kind, parts + at, bytesLeft);

			if (length == 0 || length > bytesLeft)
			{
				return false;
			}
		}

		at += length;
		wanted[kind]++;

		if (rule.mostEachSecond != 0 && spent.used[kind] + wanted[kind] > rule.mostEachSecond)
		{
			return false;
		}
	}

	for (size_t i = 0; i < ruleCount; i++)
	{
		spent.used[i] += wanted[i];
	}

	return true;
}

// The game has a fixed number of blocks. Whether fewer than the ones kept back are left.
inline bool NetPoolIsShort(int inUse, int total, int keptBack)
{
	return inUse >= total - keptBack;
}

// WHO HAS ASKED LATELY. For the question anyone can send: one address gets only so many answers a second, under the limit for everyone. The table is small, and the oldest place is taken for an address it hasn't heard of, so flooding from many addresses only pushes each other out.
struct NetAddressCount
{
	unsigned int address;
	unsigned int secondStarted;
	int used;
};

inline bool NetAddressHasRoom(NetAddressCount *table, int tableSize, unsigned int address, unsigned int now, int most)
{
	NetAddressCount *place = &table[0];

	for (int i = 0; i < tableSize; i++)
	{
		if (table[i].used != 0 && table[i].address == address)
		{
			place = &table[i];
			break;
		}

		if (table[i].used == 0 || now - table[i].secondStarted > now - place->secondStarted)
		{
			place = &table[i];
		}
	}

	if (place->used == 0 || place->address != address || now - place->secondStarted >= 1000)
	{
		place->address = address;
		place->secondStarted = now;
		place->used = 0;
	}

	if (place->used >= most)
	{
		return false;
	}

	place->used++;
	return true;
}

// The same as NetThisSecondHasRoom below, for something counted in amounts, like bytes. What's refused isn't counted.
inline bool NetThisSecondHasRoomFor(unsigned int now, unsigned int *secondStarted, int *used, int most, int amount)
{
	if (now - *secondStarted >= 1000)
	{
		*secondStarted = now;
		*used = 0;
	}

	if (amount > most - *used)
	{
		return false;
	}

	*used += amount;
	return true;
}

// For something anyone can make this game do for free, like answering a question from an address it has never heard of. At most this many in any one second. Not per address, because an address in a question can be made up.
inline bool NetThisSecondHasRoom(unsigned int now, unsigned int *secondStarted, int *used, int most)
{
	if (now - *secondStarted >= 1000)
	{
		*secondStarted = now;
		*used = 0;
	}

	if (*used >= most)
	{
		return false;
	}

	(*used)++;
	return true;
}

// A typed address is whatever someone typed. Only four numbers of up to three digits, each up to 255, with dots between them, go anywhere near the network. No names, so nothing is ever looked up.
// It reads from anything that answers to [], not only plain letters, so that a test can hand it text that counts every look past the end of its room. A look too far gives the same answer as no look, so the answer alone can't show one.
template <typename Text>
inline bool NetTypedAddressIsGoodWithin(const Text &text, size_t most)
{
	int parts = 0;
	size_t at = 0;

	while (parts < 4)
	{
		int digits = 0;
		int value = 0;

		while (at < most && text[at] >= '0' && text[at] <= '9')
		{
			value = value * 10 + (text[at] - '0');
			digits++;
			at++;

			if (digits > 3 || value > 255)
			{
				return false;
			}
		}

		if (digits == 0)
		{
			return false;
		}

		parts++;

		if (parts < 4)
		{
			if (at >= most || text[at] != '.')
			{
				return false;
			}

			at++;
		}
	}

	return at < most && text[at] == '\0';
}

inline bool NetTypedAddressIsGood(const char *text, size_t most)
{
	return NetTypedAddressIsGoodWithin(text, most);
}

// How many times over one hit may count, in the game's fixed point. The damage sums go round a loop this many times, and a negative number would go round about four billion. A real hit is well under this: 65536 is once.
static const int kNetMostDamageMultiple = 65536 * 1000;

inline bool NetDamageMultipleIsGood(int multiple)
{
	return multiple >= 0 && multiple <= kNetMostDamageMultiple;
}

// A chat line: a byte saying who it's for, then up to this many letters, then a zero. How long that is, or 0 if there's no zero in time. It has to end in the message, because the game reads along it until the zero.
static const size_t kNetMostChatLetters = 254;

inline size_t NetChatPartLength(const unsigned char *part, size_t bytesLeft)
{
	for (size_t i = 1; i < bytesLeft && i <= kNetMostChatLetters + 1; i++)
	{
		if (part[i] == 0)
		{
			return i + 1;
		}
	}

	return 0;
}

// Text that came from another machine and goes on the screen. Anything that isn't a character to draw becomes a space, so it can't break a line in two, or pretend to be a line of its own.
inline void NetCleanText(char *text, size_t size)
{
	for (size_t i = 0; i < size && text[i] != 0; i++)
	{
		unsigned char const letter = (unsigned char)text[i];

		if (letter < 32 || letter == 127)
		{
			text[i] = ' ';
		}
	}
}

// A player's name, from one fixed place to another. Nothing says the one that arrived ends, and the game treats a name as ending, so this makes sure of it.
inline void NetCopyPlayerName(char *to, const char *from, size_t size)
{
	memcpy(to, from, size);
	to[size - 1] = '\0';
	NetCleanText(to, size);
}

// A name the host is about to give a new player, when someone already has it. Two players with one name could speak for each other. This puts a number on the end, cutting the name if it has to, and takes numbers from 2 up. used says whether a name is taken, and most is the most numbers to try.
template <typename Used>
inline bool NetMakeNameUnique(char *name, size_t size, Used used, int most)
{
	if (!used(name))
	{
		return true;
	}

	char original[32];

	if (size > sizeof(original))
	{
		return false;
	}

	memcpy(original, name, size);

	for (int number = 2; number <= most; number++)
	{
		char ending[12];
		size_t endingLength = 0;
		int value = number;
		char digits[12];
		size_t digitCount = 0;

		do
		{
			digits[digitCount++] = (char)('0' + value % 10);
			value /= 10;
		} while (value > 0);

		while (digitCount > 0)
		{
			ending[endingLength++] = digits[--digitCount];
		}

		if (endingLength >= size)
		{
			return false;
		}

		size_t kept = 0;

		while (kept < size - 1 - endingLength && original[kept] != 0)
		{
			name[kept] = original[kept];
			kept++;
		}

		memcpy(name + kept, ending, endingLength);
		name[kept + endingLength] = 0;

		if (!used(name))
		{
			return true;
		}
	}

	return false;
}

// The host saying which level of the campaign comes next. A campaign only goes forward, and the place just past its last level means it's over. Anything else is refused, because the place is used to look up a level in a list.
inline bool NetCampaignMoveIsGood(int campaign, int place, int campaignNow, int placeNow, int levelsInCampaign)
{
	return campaignNow != 0 && campaign == campaignNow && place > placeNow && place <= levelsInCampaign;
}

// What happens to a hit on a breakable track object. Single player: it counts. A network game outside the mode that plays as
// single player does: it's ignored, as the game always did. In that mode: on the host it counts, and on another machine it goes
// to the host, unless it came from the host in the first place.
enum NetBreakableHit { kNetHitIsIgnored, kNetHitGoesToTheHost, kNetHitCounts };

inline NetBreakableHit NetBreakableHitGoes(bool inNetworkGame, bool sameAsSinglePlayer, bool isAClient, bool cameFromTheHost)
{
	if (!inNetworkGame)
	{
		return kNetHitCounts;
	}

	if (!sameAsSinglePlayer)
	{
		return kNetHitIsIgnored;
	}

	return isAClient && !cameFromTheHost ? kNetHitGoesToTheHost : kNetHitCounts;
}

// With friendly fire off, can the weapons of the other kind of player be a friend's? In a co-op game the game says yes, because a
// marine and a Predator can be on one side there, so neither's weapons hurt the other. In the mode that plays as single player
// does, everyone plays the same species. So the other species' weapons can only be in an NPC's hands, and they have to hurt, or
// turning friendly fire off would take some of the level's dangers away. Only weapons the filter names are affected: most of an
// NPC marine's guns have ammunition of their own, which it doesn't name, and those always hurt.
inline bool NetOtherSpeciesCanBeAFriend(bool coopGame, bool sameAsSinglePlayer)
{
	return coopGame && !sameAsSinglePlayer;
}

// How near the middle of a platform lift's far end someone has to be, for a network game to send the lift to them. The game
// always said 5 m. But five lifts in the campaigns are wider than that, so someone standing beside one is further than 5 m from
// its middle and could never call it. So in the mode that plays as single player does, it's 5 m from the lift's edge. The size
// comes from the level's file, so one below nothing counts as nothing, and one past any real lift counts as that limit, which
// also means the sum can't overflow.
inline int NetLiftCallReach(int liftRadius, bool sameAsSinglePlayer)
{
	int const fromTheMiddle = 5000;
	int const widestRealLift = 100000;

	if (!sameAsSinglePlayer || liftRadius <= 0)
	{
		return fromTheMiddle;
	}

	return fromTheMiddle + (liftRadius < widestRealLift ? liftRadius : widestRealLift);
}

// A lift's sync message, in the mode that plays as single player (NETMESSAGE_LIFTSTATE): which end it names. 0 is the top, the
// lift's upHeight, which is the smaller height; 1 is the bottom, its downHeight. Anything else is refused.
inline bool NetLiftEndIsKnown(int end)
{
	return end == 0 || end == 1;
}

// Whether a height a lift's sync message gives is on that lift's own track, between its two ends, give or take the margin. It
// comes from another machine, and a car put far off its track could carry a player out of the level.
inline bool NetLiftHeightIsOnItsTrack(int height, int topHeight, int bottomHeight, int margin)
{
	return (long long)height >= (long long)topHeight - margin && (long long)height <= (long long)bottomHeight + margin;
}

// Where the host's car should be by now. Its message said the car's height, whether it was moving, and to which end. A moving car
// goes towards that end at speed, in mm a second, and stops there.
inline int NetLiftHostHeightNow(int heightSent, bool moving, int endHeight, int speed, int msSinceSent)
{
	if (!moving || msSinceSent <= 0)
	{
		return heightSent;
	}

	long long const gone = (long long)speed * msSinceSent / 1000;

	if (endHeight < heightSent)
	{
		long long const at = heightSent - gone;
		return (int)(at < endHeight ? endHeight : at);
	}

	long long const at = heightSent + gone;
	return (int)(at > endHeight ? endHeight : at);
}

// One frame of a joiner's car following the host's: the car's new height. With nobody on it and more than a step away, it goes
// straight there. Otherwise it moves towards it by at most mostThisFrame, so someone riding it is carried, never dropped or passed
// through, however far it has to go.
inline int NetLiftFollowStep(int ownHeight, int hostHeight, int mostThisFrame, bool someoneOnIt, int stepHeight)
{
	long long const gap = (long long)hostHeight - ownHeight;
	long long const size = gap < 0 ? -gap : gap;

	if ((!someoneOnIt && size > stepHeight) || size <= mostThisFrame)
	{
		return hostHeight;
	}

	return gap > 0 ? ownHeight + mostThisFrame : ownHeight - mostThisFrame;
}

// The difficulty a host says a campaign is at: 0, 1 or 2, the three the host's page offers. It picks a row in
// the game's tables of damage and health, so anything else from another machine can't be let through.
inline bool NetCampaignDifficultyIsGood(int difficulty)
{
	return difficulty >= 0 && difficulty <= 2;
}

// The same place when a joiner is told where the game has got to. It has to be a level, and "no campaign" comes with place 0.
inline bool NetCampaignPlaceIsGood(int campaign, int place, int levelsInCampaign)
{
	return campaign == 0 ? place == 0 : (place >= 0 && place < levelsInCampaign);
}

#endif
