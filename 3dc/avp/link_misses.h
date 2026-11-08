// What the log says about a link in a level that finds nothing (#130). Written 2026-11-08 by Melanikus for the AvP port.
//
// WHY: everything in a level aims at other things by name: a switch at the alien it wakes, a track at the door it opens, an NPC's death at
// the switch it sets off. A name that found nothing has always done nothing, without a word, so a broken link looked just like one that
// hadn't been used yet. Third's survey of the stock levels found 59 links broken since 1999 (AvP-docs, level-links-broken.md), and in the
// user's co-op game a facehugger stayed asleep because a switch's name found nothing (#126). Now the log says so, in two places:
//
//     links: a switch (id 1a2b3c4d5e6f7000) at (12.3, -4.0, 56.7) aims at 0a0b0c0d0e0f1000, and nothing in this level has that name
//     links: a switch (id 1a2b3c4d5e6f7000) at (12.3, -4.0, 56.7) went on or off, and its target 0a0b0c0d0e0f1000 isn't in this level
//
// The first is written when a level's links are made, at its start and at each restart (AssignAllSBNames in bh_types.cpp): that's the check
// that every link finds its target. The second is written when a switch goes on or off with a target missing. An id is the name the level
// gives, as 16 hex digits; AvP-docs' level-ids-where.js finds the object with that id. Each name gets each line once a level, and at most
// kLinkMissesMostNames names do, so a level with many broken links can't fill the log.
//
// A link to a placed NPC that this machine hasn't made finds nothing, as it should: a machine that joined a network game makes none (the
// host does, and its switches do the waking), and some NPCs are only made at another difficulty. Those aren't written one by one: one line
// counts them, each time the level's links are made.
//
// This file knows nothing about the game, so tests/link_misses_test.cpp can try all of it.

#ifndef LINK_MISSES_H
#define LINK_MISSES_H

#include <stdio.h>
#include <string.h>

#include <string>

enum
{
	kLinkNameLength = 8,         // the same as SB_NAME_LENGTH. bh_types.cpp checks that.
	kLinkMissesMostNames = 64    // names written in one level, for each of the two lines
};

// The names already written in this level, for one of the two lines
struct LinkMisses
{
	int count;
	bool saidItIsFull;
	char names[kLinkMissesMostNames][kLinkNameLength];
};

static inline void LinkMisses_Forget(LinkMisses *misses)
{
	misses->count = 0;
	misses->saidItIsFull = false;
}

// A name of all zeros is no name: a link that isn't there, so nothing is missing
static inline bool LinkMisses_IsNoName(char const *name)
{
	for (int i = 0; i < kLinkNameLength; i++)
	{
		if (name[i])
		{
			return false;
		}
	}

	return true;
}

enum LinkMissAnswer
{
	kLinkMissNoName,          // nothing to say
	kLinkMissWrite,           // the first time in this level: write its line
	kLinkMissAlreadyWritten,  // said already in this level
	kLinkMissFirstOneTooMany, // the list has just filled: write that, once
	kLinkMissTooMany          // the list is full, and that's been said
};

// What to do about a name that found nothing
static inline LinkMissAnswer LinkMisses_Note(LinkMisses *misses, char const *name)
{
	if (LinkMisses_IsNoName(name))
	{
		return kLinkMissNoName;
	}

	for (int i = 0; i < misses->count; i++)
	{
		if (memcmp(misses->names[i], name, kLinkNameLength) == 0)
		{
			return kLinkMissAlreadyWritten;
		}
	}

	if (misses->count >= kLinkMissesMostNames)
	{
		bool const firstTime = !misses->saidItIsFull;

		misses->saidItIsFull = true;
		return firstTime ? kLinkMissFirstOneTooMany : kLinkMissTooMany;
	}

	memcpy(misses->names[misses->count], name, kLinkNameLength);
	misses->count++;
	return kLinkMissWrite;
}

// A name as 16 hex digits, its bytes in the order they're kept
static inline std::string LinkMisses_NameAsText(char const *name)
{
	char text[kLinkNameLength * 2 + 1];

	for (int i = 0; i < kLinkNameLength; i++)
	{
		snprintf(text + i * 2, 3, "%02x", static_cast<unsigned char>(name[i]));
	}

	return text;
}

// "a switch (id 1a2b3c4d5e6f7000) at (12.3, -4.0, 56.7)", or without the place for something that has none. what is the kind, and a place is in mm.
static inline std::string LinkMisses_Source(char const *what, char const *sourceName, bool hasPlace, double x, double y, double z)
{
	std::string text = std::string(what) + " (id " + LinkMisses_NameAsText(sourceName) + ")";

	if (hasPlace)
	{
		char place[96];

		snprintf(place, sizeof(place), " at (%.1f, %.1f, %.1f)", x / 1000.0, y / 1000.0, z / 1000.0);
		text += place;
	}

	return text;
}

// The line when a level's links are made
static inline std::string LinkMisses_AtLoadLine(std::string const &source, char const *targetName)
{
	return "links: " + source + " aims at " + LinkMisses_NameAsText(targetName) + ", and nothing in this level has that name";
}

// The line when a switch goes on or off
static inline std::string LinkMisses_OnRequestLine(std::string const &source, char const *targetName)
{
	return "links: " + source + " went on or off, and its target " + LinkMisses_NameAsText(targetName) + " isn't in this level";
}

// The line once a level has had kLinkMissesMostNames names for one of the two lines
static inline std::string LinkMisses_TooManyLine(bool onRequest)
{
	char text[160];

	snprintf(text, sizeof(text), "links: more than %d names in this level find nothing%s, so no more are written until the next level", kLinkMissesMostNames,
		onRequest ? " when a switch goes on or off" : "");
	return text;
}

// The one line for the links to placed NPCs this machine hasn't made
static inline std::string LinkMisses_NpcsNotMadeHereLine(int links)
{
	char text[256];

	snprintf(text, sizeof(text), "links: %d %s at placed NPCs this machine hasn't made, so %s nothing, as %s: a machine that joined a network game makes "
		"none, and some are only made at another difficulty", links, links == 1 ? "link aims" : "links aim", links == 1 ? "it finds" : "they find",
		links == 1 ? "it should" : "they should");
	return text;
}

#endif
