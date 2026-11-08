// What a player is told during a co-op campaign. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the co-op
// campaign system follows Bobblen's avp-forever, rewritten).
//
// The game already knows who is alive, who it's waiting for, and why a game ended. It just doesn't say so. This file works out what to say,
// and nothing else. It touches nothing in the game, so tests/coop_in_game_test.cpp can try every case.
//
// One thing here comes from another machine: the host's list of who everyone is waiting for. A host could lie about it, so it's treated as
// a list of places to look at and nothing more. A place with nobody in it is passed over, and all a lie can do is name the wrong players on
// a line of text. Nothing is decided by it.

#ifndef _coop_in_game_h_
#define _coop_in_game_h_ 1

#include <stddef.h>

// The most players in a game. pldnet.cpp stops the build if this and NET_MAXPLAYERS ever differ, because the list below has one bit for each.
enum { kCoopMostPlayers = 8 };

// ---- Who's in and alive

enum CoopPlayerState
{
	kCoopPlayerJoining,  // on the list, and still on their way into the level
	kCoopPlayerAlive,
	kCoopPlayerOut       // dead, and out until the next level, which is the single-player rule
};

// A player is in the level once they've pressed into it. The host never presses: it starts the game everyone else joins, so it's always in.
inline CoopPlayerState CoopPlayer_State(bool hasPressedIn, bool isTheHost, bool isAlive)
{
	if (!hasPressedIn && !isTheHost)
	{
		return kCoopPlayerJoining;
	}

	return isAlive ? kCoopPlayerAlive : kCoopPlayerOut;
}

// ---- Who the loading screen is waiting for

// The host's list: one bit for each place on the player list, set if that player hasn't pressed a key yet. The host's own key counts the same
// way, because a joiner who has pressed is waiting for the host too.
inline unsigned char CoopWaiting_Places(const bool *placeHasPlayer, const bool *placeIsReady, int places, int hostPlace, bool hostHasPressed)
{
	unsigned int list = 0;

	for (int place = 0; place < places && place < kCoopMostPlayers; place++)
	{
		bool const isReady = place == hostPlace ? hostHasPressed : placeIsReady[place];

		if (placeHasPlayer[place] && !isReady)
		{
			list |= 1u << place;
		}
	}

	return (unsigned char)list;
}

// Copies as much of a piece of text as there's room for, up to its most letters. Text ends with a zero byte, but a name from another machine
// might not have one, so the count stops it anyway. The room's last byte is always kept for the end.
inline void CoopLine_Add(char *line, size_t room, size_t *used, const char *piece, size_t mostLetters)
{
	for (size_t i = 0; i < mostLetters && piece[i] != 0 && *used + 1 < room; i++)
	{
		line[(*used)++] = piece[i];
	}
}

// Writes "Waiting for: Hicks, Vasquez" into the line. False, with an empty line, if there's nobody to name: a player never waits for
// themselves, and a place with nobody in it has no name (a null pointer in names). names has kCoopMostPlayers places.
inline bool CoopWaiting_Line(char *line, size_t room, const char *words, unsigned char places, const char *const *names, size_t nameBytes, int myPlace)
{
	if (room == 0)
	{
		return false;
	}

	size_t used = 0;
	bool named = false;

	CoopLine_Add(line, room, &used, words, room);

	for (int place = 0; place < kCoopMostPlayers; place++)
	{
		if ((places & (1u << place)) == 0 || place == myPlace || names[place] == NULL)
		{
			continue;
		}

		CoopLine_Add(line, room, &used, named ? ", " : " ", 2);
		CoopLine_Add(line, room, &used, names[place], nameBytes);
		named = true;
	}

	line[named ? used : 0] = 0;
	return named;
}

// ---- Why a game ended, for the page after it

enum CoopLastWord
{
	kCoopLastWordNone,              // the player left by themselves, so there's nothing to tell them
	kCoopLastWordLeftBehind,        // the game started without them, because they don't have the map
	kCoopLastWordRemoved,           // the host removed them
	kCoopLastWordHostLeft,          // the host left, which ends the game for everyone
	kCoopLastWordCampaignComplete,
	kCoopLastWordMapComplete
};

// The order matters. Being removed closes the connection to the host, which looks the same as the host leaving, so that's asked first. And a
// finished campaign stays finished if the host's machine leaves straight after it.
inline CoopLastWord CoopLastWord_AtTheEnd(bool wasInACampaign, bool isACustomMap, bool theLastLevelHasBeenFinished, bool wasRemoved, bool hostHasGone)
{
	if (!wasInACampaign)
	{
		return kCoopLastWordNone;
	}

	if (wasRemoved)
	{
		return kCoopLastWordRemoved;
	}

	if (theLastLevelHasBeenFinished)
	{
		return isACustomMap ? kCoopLastWordMapComplete : kCoopLastWordCampaignComplete;
	}

	return hostHasGone ? kCoopLastWordHostLeft : kCoopLastWordNone;
}

#endif
