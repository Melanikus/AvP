// The test of the level record in a saved game (SAVE-HOSTILE-1, part c; 3dc/avp/save_level_record.h). Written 2026-11-10 by Melanikus
// for the AvP port.
//
// WHAT IT TESTS
//
// - save_level_record.h on its own: which names and file sizes a record may hold, the record made for a level, a record read from a save
//   as hostile input (its size, its name, what's after the name, its file size), the comparison with the level that's loaded (the same
//   level, another level, another copy of this one by its size or by its sum), and the log's words for each.
// - By reading the game's source: every new save writes the record first in its body; a load reads it before any block can change the
//   level, and loads nothing when it's another level or another copy (PO's ruling, 2026-11-10); a save with no record loads as before; a
//   record anywhere but first stops the load; the block's number is the last, so older saves keep theirs; and system.cpp notes each
//   level as it loads, from the loader's own size and sum.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/save_level_record_test.args, and the program gives back a non-zero result if any check failed.
// It needs no game files. tests/takeouts/save_level_record_test.json proves each check can fail.

#include <stdio.h>
#include <string.h>

#include <string>

#include "save_level_record.h"
#include "level_file_sum.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static void Check(bool passed, const std::string &what)
{
	Check(passed, what.c_str());
}

// A sum of the test's own: every byte the same number, written a byte at a time
static void MadeUpSum(uint8_t sum[kSaveLevelSumBytes], uint8_t value)
{
	for (size_t i = 0; i < kSaveLevelSumBytes; i++)
	{
		sum[i] = value;
	}
}

// ---- Making a record

static void TestMaking()
{
	puts("1. the record made for the level that's loaded");

	uint8_t sum[kSaveLevelSumBytes];
	MadeUpSum(sum, 0x5e);
	SaveLevelRecord record;

	Check(SaveLevel_Make("derelict", 2654321, sum, &record) && strcmp(record.name, "derelict") == 0 && SaveLevel_FileBytes(record) == 2654321u &&
		memcmp(record.sum, sum, sizeof(sum)) == 0, "a level's name, its file's size and its sum make its record");
	Check(record.fileBytes[0] == (2654321u & 0xFF) && record.fileBytes[3] == 0, "the size is kept with its lowest byte first, the same on every machine");

	bool zerosAfter = true;

	for (size_t i = strlen("derelict"); i < kSaveLevelNameBytes; i++)
	{
		zerosAfter = zerosAfter && record.name[i] == 0;
	}

	Check(zerosAfter, "and only 0s after the name");
	Check(SaveLevel_Make("hadleyshope_coop", 12, sum, &record) && SaveLevel_Make("als-dm-coop", kSaveLevelMostFileBytes, sum, &record),
		"the game's longest name, a dash, and the smallest and largest file sizes are all a record's");
	Check(SaveLevel_Make(std::string(31, 'a').c_str(), 12, sum, &record), "and so is a name of 31 letters");

	struct NotAName
	{
		const char *name;
		const char *what;
	};

	NotAName const notNames[] = {
		{ "", "an empty name" },
		{ "Custom\\mymap", "a custom map's path, which can't be saved anyway" },
		{ "der elict", "a space" },
		{ "der/elict", "a slash" },
		{ "%s%s%n", "a format" },
		{ "d\xc3\xa9relict", "an accent" },
	};

	for (size_t i = 0; i < sizeof(notNames) / sizeof(notNames[0]); i++)
	{
		memset(&record, 'x', sizeof(record));
		bool const made = SaveLevel_Make(notNames[i].name, 12, sum, &record);
		Check(!made && record.name[0] == 0 && SaveLevel_FileBytes(record) == 0, std::string("no record for a name with ") + notNames[i].what);
	}

	Check(!SaveLevel_Make(std::string(32, 'a').c_str(), 12, sum, &record), "nor for a name of 32 letters, which leaves no room for its end");
	Check(!SaveLevel_Make(NULL, 12, sum, &record), "nor for no name");
	Check(!SaveLevel_Make("derelict", 0, sum, &record) && !SaveLevel_Make("derelict", 11, sum, &record) &&
		!SaveLevel_Make("derelict", kSaveLevelMostFileBytes + 1, sum, &record),
		"nor for a level with no file (size 0), or a size no level file may be");
	Check(!SaveLevel_Make("derelict", 12, NULL, &record) && !SaveLevel_Make("derelict", 12, sum, NULL), "nor with no sum, or nowhere to put it");

	// The sizes are level_file_sum.h's, repeated so the record's rules need nothing of the game's
	Check(kSaveLevelLeastFileBytes == kLevelFileLeastBytes && kSaveLevelMostFileBytes == kLevelFileMostBytes,
		"a record's file sizes are exactly those a level file may be (level_file_sum.h)");
	Check(sizeof(SaveLevelRecord) == 52, "a record is 52 bytes");
}

// ---- Reading a record from a save, as hostile input

static SaveLevelRefusal ReadBytes(const SaveLevelRecord &bytes, SaveLevelRecord *out)
{
	return SaveLevel_Read(&bytes, sizeof(bytes), out);
}

static void TestReading()
{
	puts("2. a record read from a save is refused unless it's one the game writes");

	uint8_t sum[kSaveLevelSumBytes];
	MadeUpSum(sum, 0x17);
	SaveLevelRecord good;
	SaveLevel_Make("temple", 1000000, sum, &good);
	SaveLevelRecord read;

	Check(ReadBytes(good, &read) == kSaveLevelRead && memcmp(&read, &good, sizeof(good)) == 0, "a record the game wrote reads back as it was");

	unsigned char longer[sizeof(SaveLevelRecord) + 1] = {};
	memcpy(longer, &good, sizeof(good));
	memset(&read, 'x', sizeof(read));
	Check(SaveLevel_Read(longer, sizeof(longer), &read) == kSaveLevelWrongSize && read.name[0] == 0, "a block a byte longer is refused, and nothing is taken");
	Check(SaveLevel_Read(&good, sizeof(good) - 1, &read) == kSaveLevelWrongSize, "and so is one a byte shorter");
	Check(SaveLevel_Read(NULL, sizeof(good), &read) == kSaveLevelWrongSize && SaveLevel_Read(&good, sizeof(good), NULL) == kSaveLevelWrongSize,
		"and nothing to read, or nowhere to put it");

	SaveLevelRecord bad = good;
	memset(bad.name, 'a', sizeof(bad.name));
	memset(&read, 'x', sizeof(read));
	Check(ReadBytes(bad, &read) == kSaveLevelBadName && read.name[0] == 0, "a name with no end in its room is refused, and nothing is taken");

	bad = good;
	bad.name[0] = 0;
	Check(ReadBytes(bad, &read) == kSaveLevelBadName, "and so is an empty name");

	bad = good;
	bad.name[2] = '\n';
	Check(ReadBytes(bad, &read) == kSaveLevelBadName, "and a letter no level name has, so the log never gets one");

	bad = good;
	bad.name[20] = 'x';
	Check(ReadBytes(bad, &read) == kSaveLevelBadName, "and anything after the name's end");

	bad = good;
	bad.fileBytes[0] = 0;
	bad.fileBytes[1] = 0;
	bad.fileBytes[2] = 0;
	bad.fileBytes[3] = 0;
	Check(ReadBytes(bad, &read) == kSaveLevelBadFileSize, "a file size of 0 is refused");

	bad = good;
	bad.fileBytes[3] = 0xFF;
	Check(ReadBytes(bad, &read) == kSaveLevelBadFileSize, "and one no level file may be");
}

// ---- Comparing a save's record with the level that's loaded

static void TestComparing()
{
	puts("3. a save's record against the level that's loaded");

	uint8_t sum[kSaveLevelSumBytes];
	MadeUpSum(sum, 0x5e);
	SaveLevelRecord loaded;
	SaveLevel_Make("derelict", 2654321, sum, &loaded);

	SaveLevelRecord saved = loaded;
	Check(SaveLevel_Compare(saved, loaded) == kSaveLevelSameLevel, "the same level, and the same copy of it, is the same");

	SaveLevel_Make("sulaco", 2654321, sum, &saved);
	Check(SaveLevel_Compare(saved, loaded) == kSaveLevelOtherLevel, "another name is another level, whatever its size and sum");

	SaveLevel_Make("derelict", 2654322, sum, &saved);
	Check(SaveLevel_Compare(saved, loaded) == kSaveLevelOtherCopy, "the same name with another size is another copy of the level");

	saved = loaded;
	saved.sum[15] ^= 1;
	Check(SaveLevel_Compare(saved, loaded) == kSaveLevelOtherCopy, "and so is the same name and size with a sum one bit different");

	SaveLevel_Make("Derelict", 2654321, sum, &saved);
	Check(SaveLevel_Compare(saved, loaded) == kSaveLevelOtherLevel, "names are compared exactly, as the game's list has them");

	// The words for the log: the game's own, and only names a record has
	SaveLevelRecord other;
	SaveLevel_Make("sulaco", 1000, sum, &other);
	std::string const otherLevel = SaveLevel_LoadLine(kSaveLevelOtherLevel, other, loaded);
	std::string const otherCopy = SaveLevel_LoadLine(kSaveLevelOtherCopy, loaded, loaded);
	std::string const same = SaveLevel_LoadLine(kSaveLevelSameLevel, loaded, loaded);
	Check(otherLevel == "save: not loaded: the save is for level sulaco, and this is level derelict", "another level's line names both, and says it's not loaded");
	Check(otherCopy == "save: not loaded: the save was made with another copy of level derelict (its file's size or MD5 sum is different)",
		"another copy's line says which level, and why");
	Check(same == "save: the save is for this level, derelict, and this copy of it", "and the same level's line says so");

	SaveLevelRefusal const refusals[] = { kSaveLevelRead, kSaveLevelWrongSize, kSaveLevelBadName, kSaveLevelBadFileSize };
	bool allDiffer = true;

	for (size_t i = 0; i < sizeof(refusals) / sizeof(refusals[0]); i++)
	{
		for (size_t j = 0; j < i; j++)
		{
			allDiffer = allDiffer && strcmp(SaveLevel_RefusalWords(refusals[i]), SaveLevel_RefusalWords(refusals[j])) != 0;
		}
	}

	Check(allDiffer && SaveLevel_RefusalWords(static_cast<SaveLevelRefusal>(99)) != NULL, "every refusal has words of its own, and a value that isn't one still has words");
}

// ---- Where the game writes and reads it (read, not run: a save needs the whole game)

static std::string ReadSource(const char *name)
{
	FILE *file = fopen(name, "rb");
	std::string text;

	if (!file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		for (size_t i = 0; i < got; i++)
		{
			if (block[i] != '\r')
			{
				text += block[i];
			}
		}
	}

	fclose(file);
	return text;
}

static void FailedSlice(const std::string &why)
{
	Check(false, why);
}

static bool Has(const std::string &text, const char *piece)
{
	return text.find(piece) != std::string::npos;
}

static void TestTheGame()
{
	puts("4. where the game writes the record and holds a load to it (by reading the source)");

	std::string const savegame = ReadSource("3dc/avp/savegame.cpp");
	std::string const header = ReadSource("3dc/avp/savegame.h");
	std::string const system = ReadSource("3dc/avp/win95/system.cpp");
	Check(savegame.size() > 20000 && !header.empty() && system.size() > 10000, "savegame.cpp, savegame.h and system.cpp were found and read");

	// The block's number is the last, so every block an older save holds keeps its number
	Check(Has(header, "\tSaveBlock_GunOwnState, // one gun's own state, straight after the player. See weapon_hooks.h\n") &&
		Has(header, "\tSaveBlock_LevelIdentity,\n}SAVE_BLOCK_TYPE;"), "the record's block is the last kind, after every kind an older save can hold");

	// Every new save writes it first in its body
	std::string const saving = SourceSlice(savegame, "void SaveGame()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(saving, "headerLength = SaveInfo.BufferSpaceUsed;\n\n\t// The level record first,") &&
		saving.find("SaveLevelIdentity();") < saving.find("SaveDeadStrategies();"),
		"a new save writes the record first in its body, before any other block");
	std::string const writing = SourceSlice(savegame, "static void SaveLevelIdentity(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(writing, "block->header.type = SaveBlock_LevelIdentity;") && Has(writing, "block->header.size = sizeof(*block);") &&
		Has(writing, "block->record = levelLoaded;"), "and it's the level that's loaded, in a block of exactly its size");

	// A load reads it before any block can change the level, and goes on only for this level and this copy of it
	std::string const loading = SourceSlice(savegame, "void LoadSavedGame()\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	size_t const unpacked = loading.find("LoadInfo.BufferPos = LoadInfo.BufferStart;");
	size_t const held = loading.find("if (!terminal_error && !TheSaveIsForThisLevel())\n\t{\n\t\tterminal_error = TRUE;\n\t}");
	size_t const blocks = loading.find("while(LoadInfo.BufferSpaceLeft && !terminal_error)");
	Check(unpacked != std::string::npos && held != std::string::npos && blocks != std::string::npos && unpacked < held && held < blocks,
		"a load is held to the record once the body is unpacked, and before any of its blocks are loaded");
	Check(Has(loading, "case SaveBlock_LevelIdentity :\n\t\t\t\tLogString(\"save: not loaded: its level record isn't first, where the game writes it\");\n\t\t\t\tterminal_error = TRUE;"),
		"a record anywhere but first stops the load");

	std::string const holding = SourceSlice(savegame, "static bool TheSaveIsForThisLevel(void)\n{", "\n}\n", kSliceEndAfterTheStart, FailedSlice);
	Check(Has(holding, "header->type != SaveBlock_LevelIdentity)\n\t{\n\t\tLogString(\"save: this save has no level record, as saves made before it don't, so it's loaded as it always was\");\n\t\treturn true;"),
		"a save with no record, as older saves are, loads as it always did, and the log says so");
	Check(Has(holding, "if (read != kSaveLevelRead)\n\t{\n\t\tLogString(std::string(\"save: not loaded: \") + SaveLevel_RefusalWords(read));\n\t\treturn false;"),
		"a record refused as it's read stops the load, in the game's own words");
	Check(Has(holding, "if (!levelLoadedHasRecord)\n\t{\n\t\tLogString(std::string(\"save: not loaded: the save is for level \") + saved.name") &&
		Has(holding, "\", and this level has no record to hold it to\");\n\t\treturn false;"),
		"a record on a level with no record of its own stops the load");
	size_t const compared = holding.find("SaveLevelMatch const match = SaveLevel_Compare(saved, levelLoaded);");
	size_t const refused = holding.find("if (match != kSaveLevelSameLevel)\n\t{\n\t\treturn false;\n\t}");
	size_t const usedUp = holding.find("LoadInfo.BufferPos += header->size;");
	Check(compared != std::string::npos && refused != std::string::npos && usedUp != std::string::npos && compared < refused && refused < usedUp,
		"another level or another copy stops the load, and a record is used up only once it's the same level and copy");

	// Each level is noted as it loads, from the loader's own size and sum
	size_t const sumLine = system.find("LogString(LevelFileSum_Line(file_and_path, avp_rif_file_sum(env_rif)));");
	size_t const noted = system.find("LevelFileSum const levelSum = avp_rif_file_sum(env_rif);\n\t\tSaveGame_NoteTheLevel(Env_List[AvP.CurrentEnv]->main, levelSum.bytes, levelSum.md5);");
	Check(sumLine != std::string::npos && noted != std::string::npos && sumLine < noted && noted < system.find("copy_rif_data(env_rif, CCF_ENVIRONMENT"),
		"each level is noted as it loads: its name from the game's list, and the loader's own size and sum, with no second sum");
	std::string const noting = SourceSlice(savegame, "void SaveGame_NoteTheLevel(const char *name, unsigned int fileBytes, const unsigned char sum[16])\n{", "\n}\n",
		kSliceEndAfterTheStart, FailedSlice);
	Check(Has(noting, "levelLoadedHasRecord = SaveLevel_Make(name, fileBytes, sum, &levelLoaded);"), "and the record it makes is what saves record and loads are held to");
}

int main()
{
	TestMaking();
	TestReading();
	TestComparing();
	TestTheGame();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
