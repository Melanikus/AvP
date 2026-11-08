// The test of finding the custom maps for single player. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and Bobblen (the
// custom single-player maps follow Bobblen's avp-forever, rewritten).
//
// WHAT IT TESTS
//
// custom_maps.cpp, the game's own file, with real files. The test writes a small avp_rifs/Custom folder beside itself and runs the
// game's code on it. In the folder:
// - two good single-player maps, one plain and one packed the way the game packs its levels;
// - a multiplayer map;
// - a file that isn't a level;
// - a file too short to be one;
// - a file larger than any level;
// - a packed file that claims it unpacks to more than any level;
// - a name that breaks the rule;
// - a folder with a level's name.
// Each must be listed, or left out with the right words in the log, once however often the list is made. The pictures come from a
// stand-in for the game's picture reader, so one can be too big without making a real picture.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with tests/custom_maps_test.args, and the program gives back a non-zero result if any check
// failed. It needs no game files: it makes its own. tests/takeouts/custom_maps_test.json proves each check can fail.

#include <direct.h>
#include <corecrt_io.h> // the C library's file listing. Its usual name, io.h, is also the name of a game header, which comes first
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <string>
#include <vector>

#include "custom_maps.h"
#include "huffman.hpp"
#include "md5.h"

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game custom_maps.cpp uses

char LevelName[MAX_PATH] = "unchanged";
texID_t MISSING_TEXTURE = 0;

static std::vector<std::string> logLines;
static std::string folder; // the test's own game folder, beside the program, ending in a backslash. The test works from inside it.

void LogString(const std::string &line)
{
	logLines.push_back(line);
}

extern "C" void *AllocMem(size_t size)
{
	void *memory = malloc(size);

	if (!memory)
	{
		printf("  FAILED  the test ran out of memory\n");
		exit(1);
	}

	return memory;
}

// The game opens its files by a path from its own folder. The test works from inside its own folder, so this is the plain fopen.
FILE *avp_fopen(const char *name, const char *mode)
{
	return fopen(name, mode);
}

struct Picture
{
	texID_t id;
	uint32_t width, height;
};

static std::map<std::string, Picture> pictures;
static std::map<texID_t, int> releases;

texID_t Tex_CreateFromRIM(const std::string &name)
{
	std::map<std::string, Picture>::const_iterator found = pictures.find(name);

	return found == pictures.end() ? MISSING_TEXTURE : found->second.id;
}

void Tex_GetDimensions(texID_t id, uint32_t &width, uint32_t &height)
{
	width = height = 0;

	for (std::map<std::string, Picture>::const_iterator p = pictures.begin(); p != pictures.end(); ++p)
	{
		if (p->second.id == id)
		{
			width = p->second.width;
			height = p->second.height;
		}
	}
}

void Tex_Release(texID_t id)
{
	releases[id]++;
}

// Puts a picture where the stand-in picture reader will find it, by name
static void AddPicture(const std::string &name, texID_t id, uint32_t width, uint32_t height)
{
	Picture picture;

	picture.id = id;
	picture.width = width;
	picture.height = height;
	pictures[name] = picture;
}

// ---- Making the folder

typedef std::vector<unsigned char> Bytes;

static void PutNumber(Bytes &out, uint32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		out.push_back((unsigned char)(value >> (8 * i)));
	}
}

static Bytes Chunk(const char *name, const Bytes &body)
{
	Bytes out(name, name + 8);

	PutNumber(out, (uint32_t)(12 + body.size()));
	out.insert(out.end(), body.begin(), body.end());

	return out;
}

// A level file with one generator, which is a multiplayer start or not
static Bytes Level(bool multiplayerStart)
{
	Bytes generator;

	for (int i = 0; i < 4; i++)
	{
		PutNumber(generator, 0);
	}

	PutNumber(generator, 0);
	PutNumber(generator, multiplayerStart ? 0x40000800 : 0x800);
	PutNumber(generator, 0);

	return Chunk("REBINFF2", Chunk("REBENVDT", Chunk("SPECLOBJ", Chunk("AVPGENER", generator))));
}

static void Write(const std::string &name, const void *data, size_t size)
{
	FILE *file = fopen(name.c_str(), "wb");

	if (!file)
	{
		printf("  FAILED  the test couldn't write %s\n", name.c_str());
		exit(1);
	}

	fwrite(data, 1, size, file);
	fclose(file);
}

static void Write(const std::string &name, const Bytes &data)
{
	Write(name, data.data(), data.size());
}

// The level, packed the way the game packs its levels, as one file: the package and what follows it
static Bytes Packed(const Bytes &level)
{
	Bytes copy(level);
	HuffmanPackage *const package = HuffmanCompression(copy.data(), (int)copy.size());
	unsigned char const *const start = (unsigned char const *)package;
	Bytes out(start, start + sizeof(HuffmanPackage) + package->CompressedDataSize);

	free(package);

	return out;
}

static const char *const kFiles[] = { "Ab.rif", "A_b.rif", "Gamma.rif", "Delta.rif", "Short.rif", "Huge.rif", "Bigpack.rif", "a.b.rif" };

// The folder is made beside the program, wherever that is (the program's own path is the first thing it's given), and the test
// works from inside it, as the game works from inside its own folder.
static void MakeTheFolder(const char *program)
{
	std::string here = program ? program : "";
	size_t const slash = here.find_last_of("\\/");
	here = (slash == std::string::npos ? std::string(".\\") : here.substr(0, slash + 1)) + "custom_maps_test_files\\";
	folder = here;

	_mkdir(folder.c_str());

	if (_chdir(folder.c_str()) != 0)
	{
		printf("  FAILED  the test can't work from its own folder %s\n", folder.c_str());
		exit(1);
	}

	_mkdir("avp_rifs");
	_mkdir("avp_rifs\\Custom");
	_mkdir("avp_rifs\\Custom\\Dir.rif");

	for (size_t i = 0; i < sizeof(kFiles) / sizeof(kFiles[0]); i++)
	{
		remove((std::string("avp_rifs\\Custom\\") + kFiles[i]).c_str());
	}

	std::string const custom = "avp_rifs\\Custom\\";

	Write(custom + "Ab.rif", Level(false));
	Write(custom + "A_b.rif", Packed(Level(false)));
	Write(custom + "Gamma.rif", Level(true));
	Write(custom + "Delta.rif", "this is not a level file at all", 31);
	Write(custom + "Short.rif", "REBIN", 5);
	Write(custom + "a.b.rif", Level(false));

	// A packed level whose package claims it unpacks to 100 MB
	Bytes bigPack = Packed(Level(false));
	((HuffmanPackage *)bigPack.data())->UncompressedDataSize = 100 << 20;
	Write(custom + "Bigpack.rif", bigPack);

	// A file one byte past the 32 MB a level may be, all zeros: an empty file, made that long
	FILE *const huge = fopen((custom + "Huge.rif").c_str(), "wb");

	if (!huge || _chsize_s(_fileno(huge), (32LL << 20) + 1) != 0)
	{
		printf("  FAILED  the test couldn't make its large file\n");
		exit(1);
	}

	fclose(huge);
}

static bool LogSays(const std::string &what, const std::string &words)
{
	for (size_t i = 0; i < logLines.size(); i++)
	{
		if (logLines[i].find(what) != std::string::npos && logLines[i].find(words) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

static bool LogMentions(const std::string &what)
{
	return LogSays(what, "");
}

static void TestTheList()
{
	puts("The maps listed for single player:");

	CustomMaps_Find();

	Check(CustomMaps_Count() == 2, "two maps: the plain one and the packed one");
	Check(CustomMaps_Name(0) && std::string(CustomMaps_Name(0)) == "A_b" && CustomMaps_Name(1) && std::string(CustomMaps_Name(1)) == "Ab",
	      "sorted by name, in any case, whatever order Windows finds them in");
	Check(CustomMaps_Name(2) == NULL && CustomMaps_Name(-1) == NULL, "and no name for a number that isn't one of them");

	puts("The files left out, and what the log says:");

	Check(!LogMentions("Gamma"), "a multiplayer map is left out without a word, as it isn't broken");
	Check(LogSays("Delta", "isn't a level file"), "a file that isn't a level is left out, and the log says so");
	Check(LogSays("Short", "too small"), "a file too short to be a level");
	Check(LogSays("Huge", "larger than any level"), "a file larger than any level, which is never read");
	Check(LogSays("Bigpack", "would unpack to more"), "a packed file claiming more than any level, which is never unpacked");
	Check(LogSays("a.b.rif", "breaks the rule"), "a name that breaks the rule");
	Check(!LogMentions("Dir"), "a folder with a level's name is passed over");

	size_t const linesBefore = logLines.size();
	CustomMaps_Find();
	Check(logLines.size() == linesBefore && CustomMaps_Count() == 2, "making the list again says nothing more, and finds the same two");
}

static void TestNamesAndLoading()
{
	puts("A map named, and loaded:");

	Check(CustomMaps_Named("ab", 3) == 1 && CustomMaps_Named("A_B", 4) == 0, "a map is found by its name in any case, as Windows finds the file");
	Check(CustomMaps_Named("Gamma", 6) == -1, "a multiplayer map isn't one of them");
	Check(CustomMaps_Named("..\\Ab", 6) == -1 && CustomMaps_Named("Ab.rif", 7) == -1, "a name that breaks the rule is never one of them");

	Check(CustomMaps_SetLevelToLoad(1) && std::string(LevelName) == "Custom/Ab", "loading one puts Custom/<name> where the game loads from");

	strcpy(LevelName, "unchanged");
	Check(!CustomMaps_SetLevelToLoad(2) && !CustomMaps_SetLevelToLoad(-1) && std::string(LevelName) == "unchanged",
	      "and a number that isn't one of them changes nothing");
}

static void TestSizeAndSum()
{
	puts("A map's size and sum, for comparing with another player's:");

	Bytes const level = Level(false);
	char expected[16];
	unsigned char sum[16];
	unsigned int size = 0;

	md5_buffer((char const *)level.data(), (unsigned int)level.size(), expected);

	Check(CustomMaps_SizeAndSum(1, &size, sum), "a map on the list has them");
	Check(size == level.size(), "its size is the size of its file");
	Check(memcmp(sum, expected, 16) == 0, "and its sum is the MD5 of everything in the file");

	unsigned char packedSum[16];
	unsigned int packedSize = 0;

	Check(CustomMaps_SizeAndSum(0, &packedSize, packedSum) && memcmp(sum, packedSum, 16) != 0, "another file has another sum");

	// Changing the file proves the second answer didn't read it again
	Write("avp_rifs\\Custom\\Ab.rif", "changed", 7);

	Check(CustomMaps_SizeAndSum(1, &size, sum) && size == level.size() && memcmp(sum, expected, 16) == 0, "a map's file is only read once");

	Write("avp_rifs\\Custom\\Ab.rif", level);

	Check(!CustomMaps_SizeAndSum(2, &size, sum), "a number past the end of the list has none");
	Check(!CustomMaps_SizeAndSum(-1, &size, sum), "and nor does a number before it");

	CustomMaps_Find();
	remove("avp_rifs\\Custom\\Ab.rif");

	Check(!CustomMaps_SizeAndSum(1, &size, sum), "a file that has gone since the list was made has none");

	Write("avp_rifs\\Custom\\Ab.rif", level);

	FILE *const huge = fopen("avp_rifs\\Custom\\Ab.rif", "wb");

	if (!huge || _chsize_s(_fileno(huge), (32LL << 20) + 1) != 0)
	{
		printf("  FAILED  the test couldn't make its large file\n");
		exit(1);
	}

	fclose(huge);

	Check(!CustomMaps_SizeAndSum(1, &size, sum), "and nor does one that has grown past the most a level may be");

	Write("avp_rifs\\Custom\\Ab.rif", level);
	CustomMaps_Find();
}

static void TestPictures()
{
	puts("A map's own picture:");

	AddPicture("avp_rifs/Custom/Ab.rim", 11, 200, 100);
	AddPicture("graphics/Menus/A_b_briefing.rim", 12, 201, 100);

	CustomMaps_Find();

	Check(CustomMaps_Picture(1) == 11, "a picture beside the map is shown, up to 200 by 100");
	Check(CustomMaps_Picture(0) == MISSING_TEXTURE, "one where Bobblen's avp-forever keeps them is looked for too, and one too wide isn't shown");
	Check(releases[12] == 1, "and the one too wide is let go at once");
	Check(LogSays("A_b_briefing.rim", "bigger than 200 by 100"), "and the log says why it isn't shown");
	Check(CustomMaps_Picture(5) == MISSING_TEXTURE, "a number that isn't one of the maps has no picture");

	CustomMaps_ForgetPictures();
	Check(releases[11] == 1 && releases[12] == 1, "forgetting the pictures lets go of each shown one, once");
}

int main(int argc, char **argv)
{
	(void)argc;

	MakeTheFolder(argv[0]);

	TestTheList();
	TestNamesAndLoading();
	TestSizeAndSum();
	TestPictures();

	Check(!CustomMaps_Playing(), "nothing is being played until a map is started");
	CustomMaps_SetPlaying(true);
	Check(CustomMaps_Playing(), "and then a custom map is");

	// The large file is 32 MB, so nothing is left behind
	for (size_t i = 0; i < sizeof(kFiles) / sizeof(kFiles[0]); i++)
	{
		remove((std::string("avp_rifs\\Custom\\") + kFiles[i]).c_str());
	}

	puts(checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
