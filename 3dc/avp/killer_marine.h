// The rules for the marine who can kill Olde (#210): when "Kill Olde mode" is on, and how long he waits before he may act on what he
// sees. Written 2026-11-10 by Melanikus for the AvP port.
//
// WHAT THE CARD ASKS. The owner wants a bot in multiplayer that can beat Olde, a strong player, and beat him fairly. The bot is Hudson,
// the team's test marine, playing in a game the team hosts on one of its own PCs; Olde joins it as an ordinary player. Fair means: a
// player's health, armour, speed and weapons; shots only through the game's normal weapon and damage code; aim no better than the gun's
// own spread; no shots through walls; and he reads nothing that a player's game doesn't have. Most of that is about which code he goes
// through, and is checked where that code is. The two rules here are sums, so they live on their own, where tests/killer_marine_test.cpp
// can try every case without the game. Nothing in this file moves, aims or fires anything: the marine's behaviour asks it.
//
// RULE 1, KILL OLDE MODE. The mode is on only while a player whose identity is on the list below is in the game. The host knows each
// joiner by his identity's fingerprint (net_identity.h): the short name of the public half of his keys, like a name badge anyone may see.
// Nothing secret is in it. While the mode is on, the bot's eyes glow red, so anyone in the game can tell. That's why Olde's real
// fingerprint goes on the list only once he has said yes: with it there, everyone in a game with him can tell he's in it. Until then the
// list holds a stand-in that no real identity has. A real fingerprint is the first 16 bytes of a SHA-384 sum, and for one to come out
// as these 16 letters would take about 2 to the power 128 tries.
//   - A fingerprint is compared whole, all 16 bytes, every time: never a part of it, and never stopping at the first byte that differs.
//   - A fingerprint of all zeros is what a joiner's place holds before his handshake is done (net_peer_v2.h, JoinerFingerprint). It's
//     nobody, so it never matches, whatever the list holds.
//   - The log says which entry of the list is in the game, by the entry's name, and never writes the fingerprint.
//
// RULE 2, THE REACTION TIME. He may act only after he has seen his target for at least 500 ms without a break. Knowing where someone is
// through a wall is allowed, as the game's marines always have; acting on it isn't. Once he may act, where he aims comes from what he saw
// at least 500 ms ago, carried forward at the speed he saw then. So when the target turns, his aim keeps going the old way for 500 ms, as
// a person's would.
//   - The clock is the machine's own, in milliseconds (timeGetTime), and not the game's. The game's clock can be sped up (TimeScale), and
//     a person's half second is real time.
//   - That clock goes back to 0 every 49.7 days. Taking one reading from another as unsigned numbers stays right across that.
//   - If nobody has told him what he sees for more than 250 ms, he can't know he kept seeing it all that time, so the wait starts again.
//     A clock that went backwards gives a huge number of milliseconds since he was last told, so it starts the wait again too: a mistake
//     makes him wait, and never makes him quicker.

#ifndef KILLER_MARINE_H
#define KILLER_MARINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ----- Rule 1: Kill Olde mode

size_t const kKillerMarineFingerprintBytes = 16;   // net_identity.h's kNetIdentityFingerprintBytes: the test checks they agree
size_t const kKillerMarineMostPlayers = 8;          // pldnet.h's NET_MAXPLAYERS: the test checks they agree

struct KillerMarineListed
{
	const char *name;                                       // what the log says: never the bytes
	uint8_t fingerprint[kKillerMarineFingerprintBytes];     // the identity's fingerprint, as net_identity.h makes it
};

// THE LIST. One entry for now: a stand-in, clearly named, whose 16 bytes are the letters NOT-OLDE-STANDIN, which no real identity's
// fingerprint will ever be (above). Olde's real one replaces it only after he says yes.
static const KillerMarineListed kKillerMarineList[] = {
	{ "the stand-in (not Olde)", { 'N', 'O', 'T', '-', 'O', 'L', 'D', 'E', '-', 'S', 'T', 'A', 'N', 'D', 'I', 'N' } },
};
size_t const kKillerMarineListCount = sizeof(kKillerMarineList) / sizeof(kKillerMarineList[0]);

// Whether two fingerprints are the same. Every one of the 16 bytes is looked at, every time, and the differences are gathered up before
// the answer is given, so the answer never depends on where the first difference is.
inline bool KillerMarine_SameFingerprint(const uint8_t a[kKillerMarineFingerprintBytes], const uint8_t b[kKillerMarineFingerprintBytes])
{
	uint8_t differences = 0;

	for (size_t i = 0; i < kKillerMarineFingerprintBytes; i++)
	{
		differences |= static_cast<uint8_t>(a[i] ^ b[i]);
	}

	return differences == 0;
}

// Whether a fingerprint is all zeros: a joiner's place before his handshake is done, which is nobody
inline bool KillerMarine_IsNobody(const uint8_t fingerprint[kKillerMarineFingerprintBytes])
{
	uint8_t any = 0;

	for (size_t i = 0; i < kKillerMarineFingerprintBytes; i++)
	{
		any |= fingerprint[i];
	}

	return any == 0;
}

// Which entry of a list is in the game, or NULL for none. inTheGame holds the fingerprints of the players in the game, count of them.
// Nobody (all zeros) never matches, even an entry of zeros. More players than a game can have is a mistake somewhere, so then the answer
// is none, and the mode stays off.
inline const KillerMarineListed *KillerMarine_ListedPlayerIn(const uint8_t (*inTheGame)[kKillerMarineFingerprintBytes], size_t count,
	const KillerMarineListed *list, size_t listCount)
{
	if (inTheGame == NULL || list == NULL || count > kKillerMarineMostPlayers)
	{
		return NULL;
	}

	for (size_t player = 0; player < count; player++)
	{
		if (KillerMarine_IsNobody(inTheGame[player]))
		{
			continue;
		}

		for (size_t entry = 0; entry < listCount; entry++)
		{
			if (KillerMarine_SameFingerprint(inTheGame[player], list[entry].fingerprint))
			{
				return &list[entry];
			}
		}
	}

	return NULL;
}

// The same, against the list above: the one the game asks. Kill Olde mode is on while this isn't NULL.
inline const KillerMarineListed *KillerMarine_WhoIsIn(const uint8_t (*inTheGame)[kKillerMarineFingerprintBytes], size_t count)
{
	return KillerMarine_ListedPlayerIn(inTheGame, count, kKillerMarineList, kKillerMarineListCount);
}

// The log's line for the mode: who is in the game, by the entry's name, and never the fingerprint. who is what KillerMarine_WhoIsIn
// said. out gets at most outBytes, ended with a 0 (snprintf always ends it, cutting the line short if it has to).
inline void KillerMarine_ModeLine(const KillerMarineListed *who, char *out, size_t outBytes)
{
	if (out == NULL)
	{
		return;
	}

	if (who != NULL)
	{
		snprintf(out, outBytes, "killer marine: Kill Olde mode is on, because %s is in the game", who->name);
	}
	else
	{
		snprintf(out, outBytes, "killer marine: Kill Olde mode is off, because nobody on the list is in the game");
	}
}

// ----- Rule 2: the reaction time

uint32_t const kKillerMarineReactionMs = 500;    // the owner's: from seeing to acting, at least this long
uint32_t const kKillerMarineStaleMs = 250;       // told nothing for longer than this, he can't know he kept seeing: the wait starts again
uint32_t const kKillerMarineEveryMs = 16;        // a glimpse is kept at most this often...
size_t const kKillerMarineGlimpses = 64;         // ...and this many are kept, so the oldest is more than a second back
uint32_t const kKillerMarineSpeedOverMs = 100;   // the speed he sees is measured over at least this long, so one jumpy reading can't throw his aim

// The glimpses he keeps have to reach back past the reaction time and the time a speed is measured over, or there'd be none old enough
static_assert((kKillerMarineGlimpses - 1) * kKillerMarineEveryMs >= kKillerMarineReactionMs + kKillerMarineSpeedOverMs,
	"the kept glimpses reach back far enough");

// Where he saw his target, and when, by the machine's clock: millimetres, as the game's positions are
struct KillerMarineGlimpse
{
	uint32_t at;
	int32_t x;
	int32_t y;
	int32_t z;
};

// What one marine has seen of his target. A new target, or none, starts it again with KillerMarine_EyesShut.
struct KillerMarineEyes
{
	bool seeing;                                       // whether he saw the target the last time he was told
	uint32_t seeingSince;                              // when he started seeing it, without a break
	uint32_t lastLook;                                 // when he was last told what he sees
	KillerMarineGlimpse kept[kKillerMarineGlimpses];   // a ring: the newest at `newest`, older ones before it
	size_t count;                                      // how many of them are kept
	size_t newest;
};

// Nothing seen yet
inline void KillerMarine_EyesShut(KillerMarineEyes *eyes)
{
	memset(eyes, 0, sizeof(*eyes));
}

// How many ms from then to now, by the machine's clock, right across its going back to 0
inline uint32_t KillerMarine_MsFrom(uint32_t then, uint32_t now)
{
	return now - then;
}

// What he sees now: whether the target is in his sight, by the marine's own sight test (nothing in the way), and where it is. Asked
// each frame. Losing sight, even for one look, starts the wait again; so does being told nothing for too long.
inline void KillerMarine_Look(KillerMarineEyes *eyes, uint32_t now, bool canSee, int32_t x, int32_t y, int32_t z)
{
	bool const unbroken = eyes->seeing && KillerMarine_MsFrom(eyes->lastLook, now) <= kKillerMarineStaleMs;

	eyes->lastLook = now;

	if (!canSee)
	{
		eyes->seeing = false;
		return;
	}

	if (!unbroken)
	{
		eyes->seeing = true;
		eyes->seeingSince = now;
		eyes->count = 0;
	}

	if (eyes->count == 0 || KillerMarine_MsFrom(eyes->kept[eyes->newest].at, now) >= kKillerMarineEveryMs)
	{
		size_t const place = eyes->count == 0 ? 0 : (eyes->newest + 1) % kKillerMarineGlimpses;

		eyes->kept[place].at = now;
		eyes->kept[place].x = x;
		eyes->kept[place].y = y;
		eyes->kept[place].z = z;
		eyes->newest = place;

		if (eyes->count < kKillerMarineGlimpses)
		{
			eyes->count++;
		}
	}
}

// Whether he may act now: he has seen the target for at least the reaction time without a break, and was told so recently
inline bool KillerMarine_MayAct(const KillerMarineEyes *eyes, uint32_t now)
{
	return eyes->seeing && KillerMarine_MsFrom(eyes->lastLook, now) <= kKillerMarineStaleMs
		&& KillerMarine_MsFrom(eyes->seeingSince, now) >= kKillerMarineReactionMs;
}

// A number of millimetres kept inside what the game's positions can hold
inline int32_t KillerMarine_Millimetres(double value)
{
	if (value >= 2147483647.0)
	{
		return INT32_MAX;
	}

	if (value <= -2147483648.0)
	{
		return INT32_MIN;
	}

	return static_cast<int32_t>(value);
}

// One of the three numbers of where he aims: where he saw it, carried forward by what it moved between the two glimpses, scaled up from
// the time between them to the time since. Multiplied before it's divided, so a whole number of millimetres comes out whole.
inline int32_t KillerMarine_CarriedForward(int32_t seen, int32_t before, double msBetween, double msSince)
{
	return KillerMarine_Millimetres(seen + (static_cast<double>(seen) - before) * msSince / msBetween);
}

// Where he aims now, if he may act: where he saw the target at least the reaction time ago, carried forward to now at the speed he saw
// it going then. The speed is from two glimpses at least kKillerMarineSpeedOverMs apart; for the first part of what he saw, with no two
// that far apart, he can't tell a speed and aims where he saw it. False, and nothing written, if he may not act.
//
// While he may act there's always a glimpse old enough: the first one kept is from the moment he started seeing, at least 500 ms ago,
// and once the ring is full the oldest kept is at least 63 times 16 ms back (the static_assert above). So the "none old enough" answer
// below can't happen while those hold. It stays so that if a later change breaks them, he doesn't aim at all, rather than aim at
// something he saw too recently.
inline bool KillerMarine_AimPoint(const KillerMarineEyes *eyes, uint32_t now, int32_t *x, int32_t *y, int32_t *z)
{
	if (!KillerMarine_MayAct(eyes, now))
	{
		return false;
	}

	// The newest glimpse that's at least the reaction time old
	size_t seenBack = eyes->count;

	for (size_t back = 0; back < eyes->count; back++)
	{
		size_t const place = (eyes->newest + kKillerMarineGlimpses - back) % kKillerMarineGlimpses;

		if (KillerMarine_MsFrom(eyes->kept[place].at, now) >= kKillerMarineReactionMs)
		{
			seenBack = back;
			break;
		}
	}

	if (seenBack == eyes->count)
	{
		return false;
	}

	KillerMarineGlimpse const &seen = eyes->kept[(eyes->newest + kKillerMarineGlimpses - seenBack) % kKillerMarineGlimpses];
	double const msSince = KillerMarine_MsFrom(seen.at, now);

	// The newest glimpse before that one that's far enough before it to tell a speed by
	for (size_t back = seenBack + 1; back < eyes->count; back++)
	{
		KillerMarineGlimpse const &before = eyes->kept[(eyes->newest + kKillerMarineGlimpses - back) % kKillerMarineGlimpses];
		uint32_t const msBetween = KillerMarine_MsFrom(before.at, seen.at);

		if (msBetween >= kKillerMarineSpeedOverMs)
		{
			*x = KillerMarine_CarriedForward(seen.x, before.x, msBetween, msSince);
			*y = KillerMarine_CarriedForward(seen.y, before.y, msBetween, msSince);
			*z = KillerMarine_CarriedForward(seen.z, before.z, msBetween, msSince);
			return true;
		}
	}

	*x = seen.x;
	*y = seen.y;
	*z = seen.z;
	return true;
}

#endif
