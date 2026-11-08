// The custom maps you can play in single player: see custom_maps.h. Written 2026-11-07 by Melanikus for the AvP port. Credit: Melanikus and
// Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten).

#include "3dc.h"
#include "custom_maps.h"
#include "custom_map_name.h"
#include "level_kind.h"
#include "huffman.hpp"
#include "utilities.h"
#include "logString.h"
#include "md5.h"

#include <corecrt_io.h> // the C library's file listing. Its usual name, io.h, is also the name of a game header, which comes first
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

extern char LevelName[MAX_PATH];
extern texID_t MISSING_TEXTURE;

// The largest custom level file that's read, and the largest it may unpack to. The largest stock level is 2.6 MB packed and 5.1 MB
// unpacked (Vaults, measured 2026-11-07), so these leave plenty of room for a real map and none for a file made to fill memory.
static const long long kCustomMapMostFileBytes = 32LL << 20;
static const int kCustomMapMostLevelBytes = 64 << 20;

// The largest picture shown with a map. It goes between the page's title and its rows, which leaves room for this much
static const uint32_t kCustomMapPictureMostWide = 200;
static const uint32_t kCustomMapPictureMostHigh = 100;

static const int kCustomMapSumBytes = 16;

// The list has a name for each map. The names are kept, never the files' own names, so nothing found in the folder is used unchecked.
struct CustomMapEntry
{
	char name[kCustomMapNameMost + 1];
	texID_t picture;
	bool pictureTried;

	// The size of its file and its sum, once something has asked for them
	bool sumTried;
	bool sumIsGood;
	unsigned int fileBytes;
	unsigned char sum[kCustomMapSumBytes];
};

static std::vector<CustomMapEntry> customMaps;
static bool playingACustomMap = false;

// Each thing is said once, so a map that's broken stays one line in the log however often the menu opens
static std::vector<std::string> mapsAlreadySaidLeftOut;

static void SayOnce(std::string const &line)
{
	if (std::find(mapsAlreadySaidLeftOut.begin(), mapsAlreadySaidLeftOut.end(), line) == mapsAlreadySaidLeftOut.end())
	{
		mapsAlreadySaidLeftOut.push_back(line);
		LogString(line);
	}
}

static void SayLeftOut(const char *fileName, const char *why)
{
	SayOnce(std::string("custom maps: ") + fileName + " isn't listed for single player: " + why);
}

static const char *RefusalWords(LevelKindRefusal why)
{
	switch (why)
	{
		case kLevelRefusedTooShort: return "it's shorter than a level file's heading";
		case kLevelRefusedNotALevel: return "it isn't a level file";
		case kLevelRefusedWrongSize: return "its heading gives the wrong size";
		case kLevelRefusedChunkTooSmall: return "a part of it is smaller than its own heading";
		case kLevelRefusedChunkPastTheEnd: return "a part of it goes on past the end";
		case kLevelRefusedNoEnvironment: return "it holds no level";
		case kLevelRefusedShortGenerator: return "a generator in it is too short";
		default: return "it can't be read";
	}
}

// Reads the whole level file at path into level, unpacked. why is told what's wrong when it gives back false.
static bool ReadLevel(const std::string &path, std::vector<unsigned char> &level, const char **why)
{
	FILE *const file = avp_fopen(path.c_str(), "rb");

	if (!file)
	{
		*why = "it can't be opened";
		return false;
	}

	// Its size, from the end. A size the C library can't give is -1, which is refused with the files too small.
	long long size = -1;

	if (_fseeki64(file, 0, SEEK_END) == 0)
	{
		size = _ftelli64(file);
	}

	if (size < 12 || size > kCustomMapMostFileBytes || _fseeki64(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		*why = size > kCustomMapMostFileBytes ? "it's larger than any level the game can use" : "it's too small to be a level";
		return false;
	}

	// Zeros after what's read, as the game's own level loader has (huffman.hpp)
	std::vector<unsigned char> bytes((size_t)size + HUFFMAN_SPARE_BYTES, 0);
	bool const good = fread(bytes.data(), 1, (size_t)size, file) == (size_t)size;

	fclose(file);

	if (!good)
	{
		*why = "it can't be read to the end";
		return false;
	}

	int const fileBytes = (int)size;

	if (memcmp(bytes.data(), COMPRESSED_RIF_IDENTIFIER, 8) == 0)
	{
		if ((size_t)fileBytes < sizeof(HuffmanPackage))
		{
			*why = "it's too short to be a packed level";
			return false;
		}

		HuffmanPackage const *const package = (HuffmanPackage const *)bytes.data();

		if (package->UncompressedDataSize < 12 || package->UncompressedDataSize > kCustomMapMostLevelBytes)
		{
			*why = "it would unpack to more than any level the game can use";
			return false;
		}

		// The checked unpacker knows the file's size, and refuses a package that doesn't add up
		char *const unpacked = HuffmanDecompressChecked(package, fileBytes);

		if (!unpacked)
		{
			*why = "it can't be unpacked";
			return false;
		}

		level.assign((unsigned char *)unpacked, (unsigned char *)unpacked + package->UncompressedDataSize);
		free(unpacked);
	}
	else
	{
		bytes.resize((size_t)fileBytes);
		level.swap(bytes);
	}

	return true;
}

static bool ByName(const CustomMapEntry &a, const CustomMapEntry &b)
{
	return _stricmp(a.name, b.name) < 0;
}

void CustomMaps_Find(void)
{
	CustomMaps_ForgetPictures();
	customMaps.clear();

	struct __finddata64_t found;
	intptr_t const search = _findfirst64("avp_rifs/Custom/*.rif", &found);

	if (search == -1)
	{
		return;
	}

	do
	{
		if (found.attrib & (_A_SUBDIR | _A_SYSTEM | _A_HIDDEN))
		{
			continue;
		}

		CustomMapEntry entry;

		// The file name ends within its own room in the found data, as the C library promises, and is checked against the rule anyway
		if (!CustomMapName_FromFileName(found.name, sizeof(found.name), entry.name, sizeof(entry.name)))
		{
			found.name[sizeof(found.name) - 1] = 0;
			SayLeftOut(found.name, "its name breaks the rule for a map's name (see custom_map_name.h)");
			continue;
		}

		std::vector<unsigned char> level;
		const char *why = "";

		if (!ReadLevel(std::string("avp_rifs/Custom/") + entry.name + ".rif", level, &why))
		{
			SayLeftOut(entry.name, why);
			continue;
		}

		LevelKindRefusal refusal = kLevelRefusedNot;
		LevelKind const kind = LevelKind_Of(level.data(), level.size(), &refusal);

		if (kind == kLevelKindUnreadable)
		{
			SayLeftOut(entry.name, RefusalWords(refusal));
			continue;
		}

		if (kind == kLevelKindMultiplayer)
		{
			continue; // a multiplayer map, which is listed where multiplayer maps are, and isn't worth a line here
		}

		entry.picture = MISSING_TEXTURE;
		entry.pictureTried = false;
		entry.sumTried = false;
		entry.sumIsGood = false;
		customMaps.push_back(entry);
	}
	while (_findnext64(search, &found) == 0);

	_findclose(search);

	std::sort(customMaps.begin(), customMaps.end(), ByName);
}

// Reads the whole file a piece at a time, and stops at the most a level may be: a file can have grown since the list was made.
static bool SumTheFile(const char *name, unsigned int *fileBytes, unsigned char *sum)
{
	FILE *const file = avp_fopen((std::string("avp_rifs/Custom/") + name + ".rif").c_str(), "rb");

	if (!file)
	{
		return false;
	}

	std::vector<unsigned char> piece(64 * 1024);
	struct MD5Context context;
	long long total = 0;
	size_t got;

	MD5Init(&context);

	while (total <= kCustomMapMostFileBytes && (got = fread(piece.data(), 1, piece.size(), file)) > 0)
	{
		MD5Update(&context, piece.data(), (unsigned int)got);
		total += (long long)got;
	}

	bool const readItAll = !ferror(file) && total <= kCustomMapMostFileBytes;

	fclose(file);
	MD5Final(sum, &context);
	*fileBytes = (unsigned int)total;

	return readItAll;
}

bool CustomMaps_SizeAndSum(int which, unsigned int *fileBytes, unsigned char sum[kCustomMapSumBytes])
{
	if (which < 0 || which >= (int)customMaps.size())
	{
		return false;
	}

	CustomMapEntry &entry = customMaps[which];

	// Only once for each map, so a host changing its mind over and over costs a joiner nothing more
	if (!entry.sumTried)
	{
		entry.sumTried = true;
		entry.sumIsGood = SumTheFile(entry.name, &entry.fileBytes, entry.sum);
	}

	if (!entry.sumIsGood)
	{
		return false;
	}

	*fileBytes = entry.fileBytes;
	memcpy(sum, entry.sum, sizeof(entry.sum));

	return true;
}

int CustomMaps_Count(void)
{
	return (int)customMaps.size();
}

const char *CustomMaps_Name(int which)
{
	if (which < 0 || which >= (int)customMaps.size())
	{
		return NULL;
	}

	return customMaps[which].name;
}

int CustomMaps_Named(const char *name, size_t room)
{
	if (!CustomMapName_IsSafe(name, room))
	{
		return -1;
	}

	// In any case, because Windows takes Map.rif and map.rif for the same file
	for (size_t i = 0; i < customMaps.size(); i++)
	{
		if (_stricmp(customMaps[i].name, name) == 0)
		{
			return (int)i;
		}
	}

	return -1;
}

bool CustomMaps_SetLevelToLoad(int which)
{
	const char *const name = CustomMaps_Name(which);

	if (!name)
	{
		return false;
	}

	// The name is at most 39 letters, so "Custom/" and it always fit LevelName's 260 bytes
	_snprintf(LevelName, MAX_PATH - 1, "Custom/%s", name);
	LevelName[MAX_PATH - 1] = 0;

	return true;
}

texID_t CustomMaps_Picture(int which)
{
	if (which < 0 || which >= (int)customMaps.size())
	{
		return MISSING_TEXTURE;
	}

	CustomMapEntry &entry = customMaps[which];

	if (!entry.pictureTried)
	{
		entry.pictureTried = true;

		// Beside the map first, then where Bobblen's avp-forever keeps a map's briefing picture. A picture that isn't there is
		// nothing to say; one that's there and can't be read is said by the picture reader.
		std::string const places[2] = {
			std::string("avp_rifs/Custom/") + entry.name + ".rim",
			std::string("graphics/Menus/") + entry.name + "_briefing.rim",
		};

		for (size_t p = 0; p < 2 && entry.picture == MISSING_TEXTURE; p++)
		{
			entry.picture = Tex_CreateFromRIM(places[p]);

			// The menu draws a picture at its own size, so a bigger one would cover the rows
			uint32_t width = 0, height = 0;

			if (entry.picture != MISSING_TEXTURE)
			{
				Tex_GetDimensions(entry.picture, width, height);

				if (width > kCustomMapPictureMostWide || height > kCustomMapPictureMostHigh)
				{
					SayOnce("custom maps: " + places[p] + " is bigger than 200 by 100, so it isn't shown. The map is still listed.");
					Tex_Release(entry.picture);
					entry.picture = MISSING_TEXTURE;
				}
			}
		}
	}

	return entry.picture;
}

void CustomMaps_ForgetPictures(void)
{
	for (size_t i = 0; i < customMaps.size(); i++)
	{
		if (customMaps[i].pictureTried && customMaps[i].picture != MISSING_TEXTURE)
		{
			Tex_Release(customMaps[i].picture);
		}

		customMaps[i].picture = MISSING_TEXTURE;
		customMaps[i].pictureTried = false;
	}
}

void CustomMaps_SetPlaying(bool playing)
{
	playingACustomMap = playing;
}

bool CustomMaps_Playing(void)
{
	return playingACustomMap;
}
