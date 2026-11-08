// Modified 2026-11-07 by Melanikus for the AvP port:
//   a state message that sets a flag this version doesn't know is refused, where it used to be quietly ignored
// The kinds of NPC the host runs and tells the other machines about, apart from aliens, which have always had messages of their own. Written 2026-11-06 by Melanikus for the AvP port.
//
// These numbers go over the network, so they mean only what this file says, and don't depend on how the game numbers its behaviours. A number that arrives is treated as hostile:
// the receiver asks these before using one for anything.
//
// This file knows nothing about the game, so tests/npc_kinds_test.cpp can try all of it.

#ifndef NPC_KINDS_H
#define NPC_KINDS_H

enum
{
	kNpcKindMarine = 0,
	kNpcKindPredator,
	kNpcKindXenoborg,
	kNpcKindQueen,
	kNpcKindFacehugger,
	kNpcKindSentry,
	kNpcKindCount
};

// What the flags in a state message mean
enum
{
	kNpcFlagFiring = 1,
	kNpcFlagOnFire = 2,
	kNpcFlagSequenceLoops = 4,
	kNpcFlagsKnown = kNpcFlagFiring | kNpcFlagOnFire | kNpcFlagSequenceLoops   // 8 was an NPC marine's lamp on one test branch: each machine decides that now
};

// Whether every flag a state message sets is one this version knows. A flag from another build's meaning would otherwise be quietly ignored.
static inline bool NpcFlags_AreKnown(int flags)
{
	return (flags & ~kNpcFlagsKnown) == 0;
}

static inline bool NpcKind_IsKnown(int kind)
{
	return kind >= 0 && kind < kNpcKindCount;
}

// The game's own names for things in a network message are positive. -1 means a player, and anything else below nothing has never been a name.
static inline bool NpcGuid_IsGood(int guid)
{
	return guid >= 0;
}

// A sequence with no length has nothing to play, and the game's animation code divides by it
static inline bool NpcSequenceLength_IsGood(unsigned int length)
{
	return length > 0;
}

// How cloaked a predator is goes over the network in one byte. The game counts it from 0, not cloaked at all, up to 65536,
// and it draws a predator by that number, so a ghost has to have about the same one or it looks different on each machine.
// Anything cloaked at all stays above 0, because 0 is what turns the cloak off.
static inline unsigned char NpcCloak_ToByte(int effectiveness)
{
	if (effectiveness <= 0)
	{
		return 0;
	}

	int const byte = effectiveness >> 8;

	return (unsigned char)(byte > 255 ? 255 : (byte < 1 ? 1 : byte));
}

static inline int NpcCloak_FromByte(unsigned char byte)
{
	return (int)byte << 8;
}

// Whose voice an NPC's sound is in. Aliens have an older message of their own.
enum
{
	kNpcSoundMarine = 0,
	kNpcSoundPredator,
	kNpcSoundQueen,
	kNpcSoundKinds
};

// How far a sound may be shifted from its usual pitch. The game shifts one by far less than this, and this isn't to say what a machine
// should play, only that a number from nowhere doesn't get as far as the sound card.
static inline bool NpcSoundPitch_IsGood(int pitch)
{
	return pitch >= -32768 && pitch <= 32767;
}

// How long a corpse lasts before it melts away, where 65536 is a second. It comes from another machine, and it's counted
// down frame by frame, so a number from nowhere could leave a corpse there for hours or have none at all. The longest the
// game gives one is 16 seconds, a predator's and a marine's.
enum
{
	kNpcLongestDeathTime = 16 * 65536
};

static inline bool NpcDeathTime_IsGood(int deathTime)
{
	return deathTime > 0 && deathTime <= kNpcLongestDeathTime;
}

// A xenoborg has five beams: three thin lines that show where his head and arms are aiming, and the two gun beams that do
// the damage. One bit for each, so one byte says which are on.
enum
{
	kNpcXenoBeamHead = 1,
	kNpcXenoBeamLeftArm = 2,
	kNpcXenoBeamRightArm = 4,
	kNpcXenoBeamLeftGun = 8,
	kNpcXenoBeamRightGun = 16,
	kNpcXenoBeamsThatExist = 31
};

// Any other bit isn't a beam of his, so this message didn't come from a game like this one
static inline bool NpcXenoBeams_AreGood(unsigned char beams)
{
	return (beams & ~kNpcXenoBeamsThatExist) == 0;
}

// A bone's position in a delta controller goes over the network in one byte, and it's kept under 65536
static inline unsigned char NpcDelta_ToByte(int timer)
{
	if (timer < 0)
	{
		return 0;
	}

	return (unsigned char)((timer > 65535 ? 65535 : timer) >> 8);
}

static inline int NpcDelta_FromByte(unsigned char byte)
{
	return (int)byte << 8;
}

#endif
