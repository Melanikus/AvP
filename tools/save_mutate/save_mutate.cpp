// save_mutate: makes broken copies of one saved game, each with one thing wrong and named by what, for the game's loader to be tried on
// (SAVE-HOSTILE-1). What it changes, and why, is in save_mutate.h. Written 2026-11-08 by Melanikus for the AvP port.
//
//   save_mutate <a copy of a save> [seed] [numbers per kind of block] [single bytes] [episodes to move it to, as 1,2,3]
//
// The copies go into a folder called save_mutate_copies in the folder it's run from, which it makes. If that folder is there already it
// stops, so it never writes over or mixes in anything. Give it a copy of a save, never the player's own.
//
// It's built by tests/save_mutate_test.args's sources plus this file; tests/save_mutate_test.cpp checks the same code in the automatic
// checks, so it can't quietly stop building.
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "huffman.hpp"
#include "save_mutate.h"

// The compressor asks for its memory through AllocMem, the game's own request. The real one brings most of the game with it, so this
// plain stand-in takes its place; save_mutate.h gives it back with free.
extern "C" void *AllocMem(size_t size)
{
	void *memory = malloc(size);

	if (!memory)
	{
		printf("ran out of memory\n");
		exit(1);
	}

	return memory;
}

static char const *const kOutFolder = "save_mutate_copies";

static bool ReadWholeFile(char const *path, std::vector<uint8_t> *bytes)
{
	FILE *file = fopen(path, "rb");

	if (!file)
	{
		return false;
	}

	bytes->clear();
	uint8_t chunk[65536];
	size_t got;

	while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0)
	{
		if (bytes->size() + got > kSaveMutateMostFile)
		{
			fclose(file);
			return false;
		}

		bytes->insert(bytes->end(), chunk, chunk + got);
	}

	bool const ok = !ferror(file);
	fclose(file);
	return ok;
}

static bool WriteWholeFile(std::string const &path, std::vector<uint8_t> const &bytes)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const ok = bytes.empty() || fwrite(&bytes[0], 1, bytes.size(), file) == bytes.size();
	return fclose(file) == 0 && ok;
}

// A whole number from the command line, within limits, or the default when it isn't given
static bool Number(int argc, char **argv, int at, long fallback, long low, long high, long *out)
{
	if (at >= argc)
	{
		*out = fallback;
		return true;
	}

	char *end = nullptr;
	errno = 0;
	long const value = strtol(argv[at], &end, 10);

	if (errno != 0 || end == argv[at] || *end != '\0' || value < low || value > high)
	{
		return false;
	}

	*out = value;
	return true;
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 6)
	{
		printf("save_mutate <a copy of a save> [seed] [numbers per kind of block] [single bytes] [episodes to move it to, as 1,2,3]\n");
		return 2;
	}

	long seed = 145, words = 16, bytes = 32;

	if (!Number(argc, argv, 2, 145, 1, 2147483647L, &seed) || !Number(argc, argv, 3, 16, 0, 1000, &words) || !Number(argc, argv, 4, 32, 0, 10000, &bytes))
	{
		printf("the seed is from 1 up, the numbers per kind from 0 to 1000, and the single bytes from 0 to 10000\n");
		return 2;
	}

	// The episodes: numbers from 0 to 255, split by commas
	std::vector<int> moveTo;

	if (argc > 5)
	{
		std::string const list = argv[5];
		size_t from = 0;

		while (from <= list.size())
		{
			size_t const comma = list.find(',', from);
			std::string const one = list.substr(from, comma == std::string::npos ? std::string::npos : comma - from);
			char *end = nullptr;
			long const episode = strtol(one.c_str(), &end, 10);

			if (one.empty() || *end != '\0' || episode < 0 || episode > 255 || moveTo.size() >= 256)
			{
				printf("the episodes are numbers from 0 to 255, split by commas: %s\n", argv[5]);
				return 2;
			}

			moveTo.push_back(static_cast<int>(episode));

			if (comma == std::string::npos)
			{
				break;
			}

			from = comma + 1;
		}
	}

	std::vector<uint8_t> file;

	if (!ReadWholeFile(argv[1], &file))
	{
		printf("can't read the save, or it's bigger than any save: %s\n", argv[1]);
		return 1;
	}

	SaveMutateSave save;
	const char *why = SaveMutate_Open(file, &save);

	if (why)
	{
		printf("not a save this can open: %s\n", why);
		return 1;
	}

	struct stat there;

	if (stat(kOutFolder, &there) == 0)
	{
		printf("%s is there already: move it away first, so nothing is written over\n", kOutFolder);
		return 1;
	}

	if (_mkdir(kOutFolder) != 0)
	{
		printf("can't make the folder %s\n", kOutFolder);
		return 1;
	}

	std::vector<SaveMutateChange> const changes = SaveMutate_Changes(save, static_cast<uint32_t>(seed), static_cast<size_t>(words), static_cast<size_t>(bytes), moveTo);
	size_t written = 0, skipped = 0;
	std::vector<std::string> names;

	for (size_t k = 0; k < changes.size(); k++)
	{
		std::vector<uint8_t> copy;
		std::string name = SaveMutate_Name(save, changes[k]);

		// Two random picks can land on the same change: the second gets its number on the end
		for (size_t n = 0; n < names.size(); n++)
		{
			if (names[n] == name)
			{
				name += "_" + std::to_string(k);

				if (name.size() > kSaveMutateMostName)
				{
					name = "change" + std::to_string(k);
				}

				break;
			}
		}

		if (!SaveMutate_Make(save, changes[k], &copy))
		{
			skipped++;
			continue;
		}

		if (!WriteWholeFile(std::string(kOutFolder) + "/" + name + ".sav", copy))
		{
			printf("can't write %s\n", name.c_str());
			return 1;
		}

		names.push_back(name);
		written++;
	}

	printf("%u blocks; %u copies written to %s, %u changes that didn't fit this save\n", static_cast<unsigned>(save.list.size()),
		static_cast<unsigned>(written), kOutFolder, static_cast<unsigned>(skipped));
	return 0;
}
