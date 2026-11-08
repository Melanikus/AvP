// What a level file is made for: single player or multiplayer. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen
// (the custom single-player maps follow Bobblen's avp-forever, rewritten).
//
// WHY: the single-player menu lists the custom maps in avp_rifs/Custom, but only those made for single player. Most of what's in that
// folder is made for multiplayer.
//
// HOW IT TELLS: by the level's multiplayer starts. A multiplayer start is a generator (an AVPGENER chunk) of type 0 with the
// multiplayer-start flag; object_setup.cpp reads them the same way. Measured on all 71 stock levels, 2026-11-07: each of the 32
// single-player campaign levels has none, and each of the 26 multiplayer and co-op levels has between 2 and 17. The game's own
// single-player start (an AVPSTART chunk) doesn't tell them apart: five campaign levels have none, and six multiplayer levels have one.
//
// THE FILE IS HOSTILE INPUT. It comes from a folder anyone can put files in. So every size is checked against what's left before
// anything is read, nothing is ever read past the end, and a file that doesn't add up is refused with a reason saying why. Only
// three levels of the file are walked: the file's heading, the level's environment (REBENVDT), and its special objects (SPECLOBJ).
//
// A level file is a row of "chunks". Each one starts with a 12-byte heading: an 8-letter name, then its size in bytes, heading
// included, as a 4-byte number. Some chunks hold a row of smaller chunks inside them, after their heading.
//
// This reads the file once it's unpacked: most level files are packed, and custom_maps.cpp unpacks them first, with the checked unpacker.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum LevelKind
{
	kLevelKindUnreadable,     // the file doesn't add up, so it's left out
	kLevelKindSinglePlayer,   // it has no multiplayer starts
	kLevelKindMultiplayer,    // it has at least one
};

// Why a file was refused, or kLevelRefusedNot when it wasn't. Each has its own number so that a test can check for exactly this one.
enum LevelKindRefusal
{
	kLevelRefusedNot,
	kLevelRefusedNoData,           // nothing was given
	kLevelRefusedTooShort,         // shorter than the file's own heading
	kLevelRefusedNotALevel,        // the heading isn't "REBINFF2"
	kLevelRefusedWrongSize,        // the heading's size isn't the file's size
	kLevelRefusedChunkTooSmall,    // a chunk says it's smaller than its own heading
	kLevelRefusedChunkPastTheEnd,  // a chunk says it goes on past the end of what holds it
	kLevelRefusedNoEnvironment,    // the file holds no level environment (REBENVDT), so it isn't a level at all
	kLevelRefusedShortGenerator,   // a generator is too short to hold its type and flags. The game would read past it.
};

// The generator's type and flags come after its place (three 4-byte numbers) and its facing (one), so they start 16 bytes into it.
static const size_t kLevelKindGeneratorTypeAt = 16;
static const size_t kLevelKindGeneratorFlagsAt = 20;
static const size_t kLevelKindGeneratorLeastBody = 24;
static const uint32_t kLevelKindMultiplayerStartFlag = 0x40000000; // AVPGENFLAG_MULTIPLAYERSTART in avp_chunk.hpp
static const size_t kLevelKindHeading = 12;

// A 4-byte number, lowest byte first, as the file stores it. Copied a byte at a time, so it needn't sit at any particular place.
static inline uint32_t LevelKind_Number(const unsigned char *at)
{
	return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

// Walks the row of chunks between start and end, calling see(name, body, bodySize) for each. Gives back kLevelRefusedNot if the
// whole row adds up, or why it doesn't. The row must fill the space exactly: anything left over that's too small to be a chunk is refused.
template <typename See>
static inline LevelKindRefusal LevelKind_WalkRow(const unsigned char *data, size_t start, size_t end, See see)
{
	size_t at = start;

	while (at < end)
	{
		if (end - at < kLevelKindHeading)
		{
			return kLevelRefusedChunkPastTheEnd;
		}

		uint32_t const size = LevelKind_Number(data + at + 8);

		if (size < kLevelKindHeading)
		{
			return kLevelRefusedChunkTooSmall;
		}

		if (size > end - at)
		{
			return kLevelRefusedChunkPastTheEnd;
		}

		LevelKindRefusal const inside = see((const char *)(data + at), at + kLevelKindHeading, (size_t)size - kLevelKindHeading);

		if (inside != kLevelRefusedNot)
		{
			return inside;
		}

		at += size;
	}

	return kLevelRefusedNot;
}

static inline bool LevelKind_Named(const char *name, const char *wanted)
{
	return memcmp(name, wanted, 8) == 0;
}

// What the unpacked level file in data (size bytes) is made for. why, if given, is told the reason for a refusal. multiplayerStarts,
// if given, is told how many it found.
static inline LevelKind LevelKind_Of(const unsigned char *data, size_t size, LevelKindRefusal *why = 0, int *multiplayerStarts = 0)
{
	LevelKindRefusal refusal = kLevelRefusedNot;
	int starts = 0;
	bool sawEnvironment = false;

	if (!data)
	{
		refusal = kLevelRefusedNoData;
	}
	else if (size < kLevelKindHeading)
	{
		refusal = kLevelRefusedTooShort;
	}
	else if (memcmp(data, "REBINFF2", 8) != 0)
	{
		refusal = kLevelRefusedNotALevel;
	}
	else if (LevelKind_Number(data + 8) != size)
	{
		refusal = kLevelRefusedWrongSize;
	}
	else
	{
		// The file's own chunks, then the environment's, then its special objects', and nothing deeper
		refusal = LevelKind_WalkRow(data, kLevelKindHeading, size, [&](const char *name, size_t body, size_t bodySize) {
			if (!LevelKind_Named(name, "REBENVDT"))
			{
				return kLevelRefusedNot;
			}

			sawEnvironment = true;

			return LevelKind_WalkRow(data, body, body + bodySize, [&](const char *name, size_t body, size_t bodySize) {
				if (!LevelKind_Named(name, "SPECLOBJ"))
				{
					return kLevelRefusedNot;
				}

				return LevelKind_WalkRow(data, body, body + bodySize, [&](const char *name, size_t body, size_t bodySize) {
					if (!LevelKind_Named(name, "AVPGENER"))
					{
						return kLevelRefusedNot;
					}

					if (bodySize < kLevelKindGeneratorLeastBody)
					{
						return kLevelRefusedShortGenerator;
					}

					uint32_t const type = LevelKind_Number(data + body + kLevelKindGeneratorTypeAt);
					uint32_t const flags = LevelKind_Number(data + body + kLevelKindGeneratorFlagsAt);

					if (type == 0 && (flags & kLevelKindMultiplayerStartFlag))
					{
						starts++;
					}

					return kLevelRefusedNot;
				});
			});
		});

		if (refusal == kLevelRefusedNot && !sawEnvironment)
		{
			refusal = kLevelRefusedNoEnvironment;
		}
	}

	if (why)
	{
		*why = refusal;
	}

	if (multiplayerStarts)
	{
		*multiplayerStarts = refusal == kLevelRefusedNot ? starts : 0;
	}

	if (refusal != kLevelRefusedNot)
	{
		return kLevelKindUnreadable;
	}

	return starts == 0 ? kLevelKindSinglePlayer : kLevelKindMultiplayer;
}
