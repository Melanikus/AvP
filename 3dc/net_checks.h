// Modified 2026-11-10 by Melanikus for the AvP port:
//   whether an address is on the player's home network (NetAddress_IsThisNetwork, #188), and who has asked about games lately
//   (NetAddressHasRoom): both went the same day with the plain game (the owner's ruling for beta.2, "Everything over nyc3": every game
//   through a server's relay)
//   in the mode that plays as single player, someone waits at a lift's far end only on its half of the shaft and near it, never
//   by the size of the car's whole shape (NetLiftIsWaitingAtTheFarEnd, #189)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   how long ago a joiner heard about a lift car is counted by the machine's own clock (NetLiftMsSince, #90)
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the table of refusal reasons has a place for each reason there is
//   a message's parts can be held up against a short list of kinds, for the co-op lobby. Credit: Melanikus and Bobblen (the co-op
//   campaign system follows Bobblen's avp-forever, rewritten)
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
// Each reason needs a place of its own, or a stranger who sends other refused things first can choose which reasons share the last place, and
// hide one flood behind another. tests/coop_lobby_test.cpp counts the reasons in the source, and fails if they get near this.
static const int kMostRefusalReasons = 128;
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

// How many parts a message has, and how many of those are of a kind on this list. The co-op lobby takes only a few kinds, and one part
// of any other kind refuses the whole message, so every part has to be looked at. It's only asked about a message NetCheckParts has
// passed, so the false answer can't happen, and it keeps this safe if that ever changes.
inline bool NetCountPartsOnTheList(const unsigned char *parts, size_t size, const NetPartRule *rules, size_t ruleCount, NetPartLength lengthOf, const unsigned char *list, size_t listCount, size_t *partCount, size_t *onTheList)
{
	size_t at = 0;

	*partCount = 0;
	*onTheList = 0;

	while (at < size)
	{
		unsigned char const kind = parts[at];

		at++;

		// Walked the same way as NetCheckParts, so a part means the same thing to both
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
		(*partCount)++;

		for (size_t i = 0; i < listCount; i++)
		{
			if (list[i] == kind)
			{
				(*onTheList)++;
				break;
			}
		}
	}

	return true;
}

// The game has a fixed number of blocks. Whether fewer than the ones kept back are left.
inline bool NetPoolIsShort(int inUse, int total, int keptBack)
{
	return inUse >= total - keptBack;
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

// How far a platform lift's call reaches in a network game: the game's own 5 m, from 1999
int const kNetLiftCallReach = 5000;

// The most a lift's car counts as reaching out from its middle, along x or along z, when seen from above. No real lift comes near
// 100 m (the widest in the stock levels reaches 6.6 m from its middle), so a bigger number can only come from a broken or hostile
// level file.
int const kNetLiftWidestHalf = 100000;

// A platform lift's car seen from above: the radius of the circle round its footprint, from its shape's x and z extents in the
// level's file. The circle stays the same whichever way the lift is turned. It's the distance from the middle to the car's
// furthest corner, rounded up to a whole mm.
inline int NetLiftFootprintRadius(int minX, int maxX, int minZ, int maxZ)
{
	// How far the car reaches out from its middle along each line, on whichever side is further, at most kNetLiftWidestHalf
	long long const leftX = minX < 0 ? -(long long)minX : (long long)minX;
	long long const rightX = maxX < 0 ? -(long long)maxX : (long long)maxX;
	long long const leftZ = minZ < 0 ? -(long long)minZ : (long long)minZ;
	long long const rightZ = maxZ < 0 ? -(long long)maxZ : (long long)maxZ;
	long long halfX = leftX > rightX ? leftX : rightX;
	long long halfZ = leftZ > rightZ ? leftZ : rightZ;

	halfX = halfX < kNetLiftWidestHalf ? halfX : kNetLiftWidestHalf;
	halfZ = halfZ < kNetLiftWidestHalf ? halfZ : kNetLiftWidestHalf;

	// The corner's distance is the square root of halfX squared plus halfZ squared (Pythagoras). This finds the smallest whole number
	// whose square is at least that, by halving the range it can be in until one number is left. Whole numbers only, so every
	// machine gets the same answer.
	long long const squared = halfX * halfX + halfZ * halfZ;
	long long low = 0;
	long long high = halfX + halfZ;

	while (low < high)
	{
		long long const middle = (low + high) / 2;

		if (middle * middle < squared)
		{
			low = middle + 1;
		}
		else
		{
			high = middle;
		}
	}

	return (int)low;
}

// Whether someone counts as waiting at a platform lift's far end, so that the host of a network game sends the car to him
// (NetPlayerAtOtherTerminal, bh_platform_lift.cpp). Places are the game's, in mm, and a bigger y is lower down. The car is at rest
// at its near end, at the height nearEndY, and its far end is at farEndY. carX and carZ are the middle of the car seen from above.
//
// Outside the mode that plays as single player, it's the game's own rule from 1999: nearer than 5 m to the middle of the far end.
// distanceToTheFarEnd is that distance, as the game measures it (VectorDistance), and nothing else is looked at.
//
// In the mode, three things have to be true:
// - he's on the far end's half of the shaft, as in single player's own rule (PlayerIsNearOtherTerminal, bh_platform_lift.cpp);
// - he's within 5 m of the far end's height;
// - seen from above, he's within 5 m of the edge of the car (footprintRadius, from NetLiftFootprintRadius), so someone standing
//   beside the far end of a wide lift can still call it, which is what the mode's wider reach was for.
// Until #189 the mode took 5 m plus the radius of the car's whole shape. But some cars' shapes are frames as tall as their shafts
// (Derelict's lift down into the hive is 3.9 m wide and 36.9 m tall), so standing at the car's own end counted as waiting at the
// other one. The car went away from him and left the shaft open, and came back, and went again.
inline bool NetLiftIsWaitingAtTheFarEnd(int distanceToTheFarEnd, int playerX, int playerY, int playerZ, int carX, int carZ, int nearEndY,
	int farEndY, int footprintRadius, bool sameAsSinglePlayer)
{
	if (!sameAsSinglePlayer)
	{
		return distanceToTheFarEnd < kNetLiftCallReach;
	}

	// A car with nowhere to go has no far end
	if (farEndY == nearEndY)
	{
		return false;
	}

	// On the far end's half of the shaft. The middle is worked out as single player's rule does, in bigger numbers so it can't overflow.
	long long const middleY = ((long long)nearEndY + (long long)farEndY) / 2;
	bool const farEndIsBelow = farEndY > nearEndY;
	bool const onTheFarHalf = farEndIsBelow ? playerY > middleY : playerY < middleY;

	if (!onTheFarHalf)
	{
		return false;
	}

	// Within 5 m of the far end's height
	long long const belowTheFarEnd = (long long)playerY - (long long)farEndY;

	if (belowTheFarEnd >= kNetLiftCallReach || -belowTheFarEnd >= kNetLiftCallReach)
	{
		return false;
	}

	// Within 5 m of the car's edge, seen from above. The footprint is held to what NetLiftFootprintRadius can give, and each distance
	// is checked on its own first, so the squares below stay small enough to add up.
	long long const widestFootprint = 141422;
	long long const footprint = footprintRadius <= 0 ? 0 : footprintRadius < widestFootprint ? footprintRadius : widestFootprint;
	long long const flatReach = kNetLiftCallReach + footprint;
	long long const awayX = (long long)playerX - (long long)carX;
	long long const awayZ = (long long)playerZ - (long long)carZ;

	if (awayX >= flatReach || -awayX >= flatReach || awayZ >= flatReach || -awayZ >= flatReach)
	{
		return false;
	}

	return awayX * awayX + awayZ * awayZ < flatReach * flatReach;
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

// The longest ago a host's lift message is ever counted as heard, in ms. A minute is longer than any car takes to cross its track, and
// NetLiftHostHeightNow stops a moving car at its end anyway, so this cuts off nothing that matters: it only keeps the sums small.
static const int kNetLiftLongestMsSince = 60000;

// How long ago this machine heard the host's last message about a lift car, in ms, by its own clock (timeGetTime) from the moment it
// arrived (#90). It used to be counted by adding up the game's frame times, which leave time out: a frame is counted as at most a
// quarter of a second however long it took, and after the game has stood still (its window not active, a load, a save) the next
// frame is counted as an ordinary one. All that time the host's car kept moving, so the joiner's car stayed behind it, by 2 m or more,
// until the host's next message. The clock goes back to 0 every 49.7 days; taking one unsigned reading from the other stays right
// across that.
inline int NetLiftMsSince(unsigned int now, unsigned int heardAt)
{
	unsigned int const since = now - heardAt;

	return since > (unsigned int)kNetLiftLongestMsSince ? kNetLiftLongestMsSince : (int)since;
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
