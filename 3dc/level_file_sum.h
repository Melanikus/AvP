// Which level file a machine loaded: its size and its MD5 sum, for the log, and the sizes a level file may be. Written 2026-11-08 by
// Melanikus for the AvP port (#89).
//
// WHY: in a network game every machine loads its own copy of the level from its own avp_rifs folder. The host only says which level,
// never sends it. If two copies differ (a mod, a damaged file, another edition's copy), the game doesn't match between the machines and
// nothing says why. So each machine writes one line in its log as a level loads, and testers' logs can be compared:
//
//     level: avp_rifs/Derelict.rif is 2654321 bytes, MD5 0123456789abcdef0123456789abcdef
//
// WHERE THE SUM COMES FROM: the level loader works it out (File_Chunk, mishchnk.cpp), from the very reads that load the level, as they
// happen. So it is the sum of the file that was opened and loaded, never of another file found later under the same name.
//
// WHAT IT PROVES: the MD5 sum tells two honest copies apart. It proves nothing about a file made on purpose to have the same sum as
// another, and doesn't need to: the loader's own checks are what stand against a file made to do harm.
//
// THE SIZES: a level file is hostile input, because anyone can put a file in the folder. The loader refuses a file of the wrong size
// before it sets aside any memory for it, using LevelFile_SizeIsUsable below.

#pragma once

#include <stdio.h>
#include <string>

// The smallest a level file may be: its heading, which is 8 letters saying what kind of file it is and then, in a file that isn't
// packed, a 4-byte number giving the file's size. (A packed file's heading is longer, and the unpacker checks that for itself.)
static const long long kLevelFileLeastBytes = 12;

// The largest a level file may be. The largest of the game's own 93 files is 2.6 MB (Vaults, measured 2026-11-08 in the Steam copy),
// so 32 MB leaves plenty of room for a real map, and none for a file made to fill the computer's memory. The custom maps list uses
// the same number (custom_maps.cpp), so it never lists a map the loader would refuse.
static const long long kLevelFileMostBytes = 32LL << 20;

// Whether a file of this many bytes may be read as a level. The size is a 64-bit number, so that a file of 4 GB or more can't pass for
// a small one, as it would if only the lower 32 bits of its size were looked at.
static inline bool LevelFile_SizeIsUsable(long long bytes)
{
	return bytes >= kLevelFileLeastBytes && bytes <= kLevelFileMostBytes;
}

// The line for the log when the loader refuses a file for its size. The game then stops with a message giving only the file's name
// (load_rif), so this says why. bytes is -1 when the file's size couldn't be found at all.
static inline std::string LevelFile_SizeRefusalLine(const char *path, long long bytes)
{
	char size[32];

	snprintf(size, sizeof(size), "%lld", bytes);

	std::string const start = std::string("level: ") + (path ? path : "") + " can't be read: ";

	if (bytes < 0)
	{
		return start + "its size can't be found";
	}

	if (bytes < kLevelFileLeastBytes)
	{
		return start + "it's " + size + " bytes, too short to be a level file";
	}

	return start + "it's " + size + " bytes, larger than any level the game can use";
}

// What the loader worked out about the file it read. Both are 0 until the whole file has been read, so a file that was refused, or
// a level made in memory rather than read from a file, has no size or sum.
struct LevelFileSum
{
	unsigned int bytes = 0;            // the file's size, as it is on disk (packed, if it's packed)
	unsigned char md5[16] = {};        // the MD5 sum of all those bytes, in the order they are in the file
};

// The line for the log, as in the example at the top. path is the path the loader opened, as the game gave it (from the game's own
// folder, so it never names the player's own folders).
static inline std::string LevelFileSum_Line(const char *path, const LevelFileSum &sum)
{
	static const char kHexDigits[] = "0123456789abcdef";
	char size[16];

	snprintf(size, sizeof(size), "%u", sum.bytes);

	std::string line = std::string("level: ") + (path ? path : "") + " is " + size + " bytes, MD5 ";

	// Each byte of the sum as two hex digits, the higher half first, as md5sum and every other MD5 tool writes it
	for (int i = 0; i < 16; i++)
	{
		line += kHexDigits[sum.md5[i] >> 4];
		line += kHexDigits[sum.md5[i] & 15];
	}

	return line;
}
