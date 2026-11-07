// The custom maps you can play in single player: see custom_maps.h. Written 2026-11-07 for the AvP port. Credit: Melanikus and
// Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten).

#include "3dc.h"
#include "custom_maps.h"
#include "custom_map_name.h"
#include "level_kind.h"
#include "huffman.hpp"
#include "utilities.h"
#include "logString.h"

#include <algorithm>
#include <string>
#include <vector>

extern char LevelName[MAX_PATH];
extern texID_t MISSING_TEXTURE;

// The largest custom level file that's read, and the largest it may unpack to. The largest stock level is 2.6 MB packed and 5.1 MB
// unpacked (Vaults, measured 2026-11-07), so these leave plenty of room for a real map and none for a file made to fill memory.
static const DWORD kCustomMapMostFileBytes = 32u << 20;
static const int kCustomMapMostLevelBytes = 64 << 20;

// The largest picture shown with a map. It goes between the page's title and its rows, which leaves room for this much
static const uint32_t kCustomMapPictureMostWide = 200;
static const uint32_t kCustomMapPictureMostHigh = 100;

// The list has a name for each map. The names are kept, never the files' own names, so nothing found in the folder is used unchecked.
struct CustomMapEntry
{
	char name[kCustomMapNameMost + 1];
	texID_t picture;
	bool pictureTried;
};

static std::vector<CustomMapEntry> customMaps;
static bool playingACustomMap = false;

// Each map left out is said once, so a map that's broken stays one line in the log however often the menu opens
static std::vector<std::string> mapsAlreadySaidLeftOut;

static void SayLeftOut(const char *fileName, const char *why)
{
	std::string const line = std::string("custom maps: ") + fileName + " isn't listed for single player: " + why;

	if (std::find(mapsAlreadySaidLeftOut.begin(), mapsAlreadySaidLeftOut.end(), line) == mapsAlreadySaidLeftOut.end())
	{
		mapsAlreadySaidLeftOut.push_back(line);
		LogString(line);
	}
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
	HANDLE const file = avp_CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		*why = "it can't be opened";
		return false;
	}

	LARGE_INTEGER size;
	bool const sized = GetFileSizeEx(file, &size) != 0;

	if (!sized || size.QuadPart < 12 || size.QuadPart > (LONGLONG)kCustomMapMostFileBytes)
	{
		CloseHandle(file);
		*why = sized && size.QuadPart >= 12 ? "it's larger than any level the game can use" : "it's too small to be a level";
		return false;
	}

	// Zeros after what's read, as the game's own level loader has (huffman.hpp)
	std::vector<unsigned char> bytes((size_t)size.QuadPart + HUFFMAN_SPARE_BYTES, 0);
	DWORD read = 0;
	bool const good = ReadFile(file, bytes.data(), (DWORD)size.QuadPart, &read, 0) && read == (DWORD)size.QuadPart;

	CloseHandle(file);

	if (!good)
	{
		*why = "it can't be read to the end";
		return false;
	}

	int const fileBytes = (int)size.QuadPart;

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

	WIN32_FIND_DATA found;
	HANDLE const search = avp_FindFirstFile("avp_rifs/Custom/*.rif", &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN))
		{
			continue;
		}

		CustomMapEntry entry;

		// The file name ends within its own room in the found data, as Windows promises, and is checked against the rule anyway
		if (!CustomMapName_FromFileName(found.cFileName, sizeof(found.cFileName), entry.name, sizeof(entry.name)))
		{
			found.cFileName[sizeof(found.cFileName) - 1] = 0;
			SayLeftOut(found.cFileName, "its name breaks the rule for a map's name (see custom_map_name.h)");
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
		customMaps.push_back(entry);
	}
	while (FindNextFile(search, &found));

	FindClose(search);

	std::sort(customMaps.begin(), customMaps.end(), ByName);
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

	for (size_t i = 0; i < customMaps.size(); i++)
	{
		if (strcmp(customMaps[i].name, name) == 0)
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
					SayLeftOut(places[p].c_str(), "the picture is bigger than 200 by 100, so it isn't shown");
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
