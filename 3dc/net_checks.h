// Checks on what arrives from other players' machines. Written 2026-11-06 by Melanikus for the AvP port.
//
// Anyone on the network can send anything, so what's in a message is only something someone said until it's been checked. These are kept apart from the game's network code, and need none of its headers, so that a test can run them on their own.

#ifndef NET_CHECKS_H
#define NET_CHECKS_H

#include <stddef.h>
#include <string.h>

// Is this number a place in a table with this many places? The first place is 0.
inline bool NetNumberFitsTable(int number, int places)
{
	return number >= 0 && number < places;
}

// For a log line that any peer or LAN machine can cause once per packet or datagram: counts the event in count and
// says whether to log this one. The first kDropsLoggedInFull are logged, then only the 8th, 16th, 32nd, ... (each
// power of two), so a flood adds a few dozen lines to avp_log.txt at most, each with the running total.
static const unsigned long long kDropsLoggedInFull = 5;

inline bool LogThisDrop(unsigned long long &count)
{
	count++;
	return (count <= kDropsLoggedInFull) || ((count & (count - 1)) == 0);
}

// WHAT A GAME MESSAGE IS MADE OF. One message holds any number of parts, one after another. Each starts with a byte saying what kind it is, and each kind has its own length, so a part can only be found by getting all the ones before it right. A message
// has to be checked from end to end before any of it is believed, and if any part is wrong all of it goes, because nothing after a wrong part can be trusted to be where it seems.
struct NetPartRule
{
	unsigned char kind;       // which kind this is, so that the compiler can check the table is in the right order
	unsigned short bytes;     // how long it is after that first byte. For one whose length varies, the least it can be
	bool lengthVaries;        // if so, it's asked how long this one is
	bool onlyFromTheHost;     // the host's word on how things are, like the scores. Anyone else saying these would be believed by whoever hears it
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
inline bool NetTypedAddressIsGood(const char *text, size_t most)
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

// A player's name, from one fixed place to another. Nothing says the one that arrived ends, and the game treats a name as ending, so this makes sure of it.
inline void NetCopyPlayerName(char *to, const char *from, size_t size)
{
	memcpy(to, from, size);
	to[size - 1] = '\0';
}

#endif
