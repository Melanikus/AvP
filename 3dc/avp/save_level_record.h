// The level record in a saved game: which level the save was made on, so a save is never loaded onto another one (SAVE-HOSTILE-1, part
// c). Written 2026-11-10 by Melanikus for the AvP port. This file has only the rules, with nothing of the game's in them, so a test can try
// every case (save_level_record_test). savegame.cpp writes the record and holds a load to it.
//
// WHY. A save names its level by its species and episode numbers alone, and anyone can edit a save. An edited episode, or a save carried
// over to a changed level file, would have the loader pour one map's saved objects into another map. The loader is made to be safe for any
// save (SAVE-2), but that's being proved (SAVE-HOSTILE-1's runs), and the game shouldn't even try. So each new save carries a record of the
// level it was made on, and a save whose record isn't the level that's loaded isn't loaded at all (PO's ruling, 2026-11-10: a Tao 1 call).
//
// WHAT'S IN IT. The level's name, as the game's own list of levels has it (Env_List, such as "derelict"), and its file's size and MD5 sum,
// as the level loader worked them out from its own reads of the file (level_file_sum.h, avp_rif_file_sum). Another name is another level.
// The same name with another size or sum is another copy of that level: a changed, damaged or modded file. Both are refused.
//
// WHAT IT CAN'T DO. Whoever edits a save can edit its record too, so it's no lock against a save made to do harm: the loader's own checks
// stand against that. It stops the honest mistakes, and makes an edit take two changes that agree instead of one.
//
// OLD SAVES. A save made before the record has none, and loads as it always did, with a line in the log saying so (the user's answer B).
//
// READ AS HOSTILE (AVP Tao 3). A record is exactly its size. Its name is 1 to 31 of the letters the game's level names use (letters, digits,
// _ and -), ended with a 0, with only 0s after it. Its size is one a level file may be (level_file_sum.h's sizes). Anything else refuses the
// whole save. So a name in the log, read from a save, is only ever those letters.

#ifndef SAVE_LEVEL_RECORD_H
#define SAVE_LEVEL_RECORD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>

size_t const kSaveLevelNameBytes = 32;   // the longest level name the game has is 16 letters ("hadleyshope_coop")
size_t const kSaveLevelSumBytes = 16;    // MD5

// The record, exactly as it's stored in the save after its block's header. Only bytes, so it's the same on every machine and compiler.
struct SaveLevelRecord
{
	char name[kSaveLevelNameBytes];      // the level's name, ended with a 0, with only 0s after it
	uint8_t fileBytes[4];                // the level file's size, its lowest byte first
	uint8_t sum[kSaveLevelSumBytes];     // the level file's MD5 sum
};

static_assert(sizeof(SaveLevelRecord) == 52, "a level record is 52 bytes, with nothing between its parts");

// The level file sizes a record may hold: those a level file may be (level_file_sum.h's kLevelFileLeastBytes and kLevelFileMostBytes,
// repeated here so this file needs nothing of the game's; save_level_record_test checks they're the same)
uint32_t const kSaveLevelLeastFileBytes = 12;
uint32_t const kSaveLevelMostFileBytes = 32u << 20;

inline bool SaveLevel_NameLetterIsGood(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

// Whether name is one the record may hold: 1 to 31 good letters, ended with a 0 within its room
inline bool SaveLevel_NameIsGood(const char *name, size_t room)
{
	if (!name)
	{
		return false;
	}

	size_t length = 0;

	while (length < room && name[length])
	{
		if (!SaveLevel_NameLetterIsGood(name[length]))
		{
			return false;
		}

		length++;
	}

	return length > 0 && length < room && length < kSaveLevelNameBytes;
}

inline uint32_t SaveLevel_FileBytes(const SaveLevelRecord &record)
{
	return static_cast<uint32_t>(record.fileBytes[0]) | (static_cast<uint32_t>(record.fileBytes[1]) << 8) |
		(static_cast<uint32_t>(record.fileBytes[2]) << 16) | (static_cast<uint32_t>(record.fileBytes[3]) << 24);
}

// The record for the level that's loaded: its name from the game's list, and the loader's size and sum for its file. False, with out all
// 0, when there's nothing to record: a name the rule above refuses, or a level that wasn't read from a file (no size). Then the save gets
// no record, and loads as an old save does.
inline bool SaveLevel_Make(const char *name, uint32_t fileBytes, const uint8_t sum[kSaveLevelSumBytes], SaveLevelRecord *out)
{
	if (!out)
	{
		return false;
	}

	memset(out, 0, sizeof(*out));

	if (!sum || !SaveLevel_NameIsGood(name, kSaveLevelNameBytes) || fileBytes < kSaveLevelLeastFileBytes || fileBytes > kSaveLevelMostFileBytes)
	{
		return false;
	}

	memcpy(out->name, name, strlen(name));
	out->fileBytes[0] = static_cast<uint8_t>(fileBytes);
	out->fileBytes[1] = static_cast<uint8_t>(fileBytes >> 8);
	out->fileBytes[2] = static_cast<uint8_t>(fileBytes >> 16);
	out->fileBytes[3] = static_cast<uint8_t>(fileBytes >> 24);
	memcpy(out->sum, sum, kSaveLevelSumBytes);
	return true;
}

// Why a record read from a save was refused, or that it wasn't
enum SaveLevelRefusal
{
	kSaveLevelRead,
	kSaveLevelWrongSize,   // the block isn't exactly a record's size
	kSaveLevelBadName,     // no name, a letter a level name never has, no 0 in its room, or something after the 0
	kSaveLevelBadFileSize  // a size no level file may be
};

// Reads a record from a save: bytes of it, the block after its header. out is the record only for kSaveLevelRead, and all 0 otherwise.
inline SaveLevelRefusal SaveLevel_Read(const void *bytes, size_t size, SaveLevelRecord *out)
{
	SaveLevelRecord read;
	memset(&read, 0, sizeof(read));

	if (out)
	{
		memset(out, 0, sizeof(*out));
	}

	if (!bytes || !out || size != sizeof(SaveLevelRecord))
	{
		return kSaveLevelWrongSize;
	}

	memcpy(&read, bytes, sizeof(read));

	if (!SaveLevel_NameIsGood(read.name, kSaveLevelNameBytes))
	{
		return kSaveLevelBadName;
	}

	// Only 0s after the name's end, as the game writes it, so the name can't hide anything after it
	for (size_t i = strlen(read.name); i < kSaveLevelNameBytes; i++)
	{
		if (read.name[i] != 0)
		{
			return kSaveLevelBadName;
		}
	}

	uint32_t const fileBytes = SaveLevel_FileBytes(read);

	if (fileBytes < kSaveLevelLeastFileBytes || fileBytes > kSaveLevelMostFileBytes)
	{
		return kSaveLevelBadFileSize;
	}

	*out = read;
	return kSaveLevelRead;
}

// Whether a save's record is the level that's loaded
enum SaveLevelMatch
{
	kSaveLevelSameLevel,
	kSaveLevelOtherLevel,   // another name: the save is for another level
	kSaveLevelOtherCopy     // the same name, another size or sum: the save was made with another copy of this level
};

inline SaveLevelMatch SaveLevel_Compare(const SaveLevelRecord &saved, const SaveLevelRecord &loaded)
{
	if (strncmp(saved.name, loaded.name, kSaveLevelNameBytes) != 0)
	{
		return kSaveLevelOtherLevel;
	}

	if (memcmp(saved.fileBytes, loaded.fileBytes, sizeof(saved.fileBytes)) != 0 || memcmp(saved.sum, loaded.sum, kSaveLevelSumBytes) != 0)
	{
		return kSaveLevelOtherCopy;
	}

	return kSaveLevelSameLevel;
}

// The log's line for a load the record stopped, or let go on. Both names are records the rules above have read or made, so they're only
// ever a level name's letters.
inline std::string SaveLevel_LoadLine(SaveLevelMatch match, const SaveLevelRecord &saved, const SaveLevelRecord &loaded)
{
	switch (match)
	{
	case kSaveLevelSameLevel:
		return std::string("save: the save is for this level, ") + loaded.name + ", and this copy of it";
	case kSaveLevelOtherLevel:
		return std::string("save: not loaded: the save is for level ") + saved.name + ", and this is level " + loaded.name;
	case kSaveLevelOtherCopy:
		return std::string("save: not loaded: the save was made with another copy of level ") + loaded.name +
			" (its file's size or MD5 sum is different)";
	}

	return "save: not loaded: its level record can't be compared";
}

// The log's words for a record refused as it was read: the game's own, never anything from the save
inline const char *SaveLevel_RefusalWords(SaveLevelRefusal why)
{
	switch (why)
	{
	case kSaveLevelRead:
		return "it was read";
	case kSaveLevelWrongSize:
		return "its level record isn't the size the game writes";
	case kSaveLevelBadName:
		return "its level record's name isn't one a level can have";
	case kSaveLevelBadFileSize:
		return "its level record's file size isn't one a level file can be";
	}

	return "its level record was refused for a reason this game has no words for";
}

#endif
