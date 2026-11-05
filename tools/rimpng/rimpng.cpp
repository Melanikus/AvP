// Copies the game's pictures out of its fastfiles, writes them as PNGs so that someone can paint or generate maps from them, and turns those maps into files the game reads.
// Written 2026-11-05 by Melanikus for the AvP port.
//
//   rimpng extract <folder of copied .ffl files> <folder for the pictures>
//   rimpng topng   <picture.rim> <picture.png>
//   rimpng topng   <folder of pictures> <folder for the PNGs>
//   rimpng tomap   <map.png> <name_Normal.dds or name_Normal.rim> [--flip-green] [--flip-both] [--side N]
//   rimpng tomap   <depth.png> <name_Depth.rim>
//
// README.md, beside this file, says how to use it. It only ever reads what it's told to read, so give it copies. It never needs the game's own folders.
//
// Pictures are read by the game's own RimLoader, so what comes out is what the game sees. Files could come from anywhere, so nothing in them is trusted: every number is checked before it's used.

#include <direct.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <corecrt_io.h>

#include <new>
#include <string>
#include <vector>

#include "RimLoader.h"
#include "FastFile.h"
#include "utilities.h"
#include "relief_map_file.h"
#include "png_read.h"
#include "map_maker.h"

// ---- What the game's file reader asks of the rest of the game. These files are ordinary files, never inside a fastfile.

FastFileHandle *FF_Find(const std::string &) { return 0; }

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string const none;
	return none;
}

namespace Util
{
	void FtoBslash(std::string &text)
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			text[i] = (text[i] == '/') ? '\\' : text[i];
		}
	}
}

// ---- Small helpers

// A picture can say it's 65535 x 65535, which is far more memory than this could get. No real picture is close to this.
static uint32_t const kLargestSide = 8192;
static uint64_t const kMostPixels = 64 * 1024 * 1024;

static bool ReadWholeFile(std::string const &path, std::vector<uint8_t> &bytes)
{
	FILE *file = fopen(path.c_str(), "rb");

	if (!file)
	{
		return false;
	}

	bytes.clear();
	uint8_t chunk[65536];
	size_t got;

	while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0)
	{
		bytes.insert(bytes.end(), chunk, chunk + got);
	}

	bool const worked = ferror(file) == 0;
	fclose(file);
	return worked;
}

static bool WriteWholeFile(std::string const &path, void const *bytes, size_t size)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const worked = fwrite(bytes, 1, size, file) == size;
	return (fclose(file) == 0) && worked;
}

// Makes every folder on the way to this file
static void MakeFoldersFor(std::string const &path)
{
	for (size_t i = 1; i < path.size(); i++)
	{
		if ((path[i] == '/' || path[i] == '\\') && path[i - 1] != ':')
		{
			_mkdir(path.substr(0, i).c_str());
		}
	}
}

static bool EndsWith(std::string const &text, char const *ending)
{
	size_t const size = strlen(ending);
	return text.size() >= size && _stricmp(text.c_str() + text.size() - size, ending) == 0;
}

// Whether a name found inside a file is safe to write under a folder of ours. It might say ..\..\something, or a whole path of its own.
static bool NameStaysInsideFolder(std::string const &name)
{
	if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos)
	{
		return false;
	}

	size_t start = 0;

	while (start <= name.size())
	{
		size_t end = name.find_first_of("/\\", start);
		end = (end == std::string::npos) ? name.size() : end;

		if (name.compare(start, end - start, "..") == 0)
		{
			return false;
		}

		start = end + 1;
	}

	return true;
}

static uint32_t ReadLittle32(uint8_t const *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

// ---- Fastfiles
//
// A fastfile is a folder of files joined into one. It starts with "RFFL", then a version, how many files there are and how many bytes their list takes up, then one more number.
// Each entry of the list, from byte 20, says where its file starts in what follows the list, how long it is, and then its name, ended by a zero and padded to a multiple of four bytes.

static bool IsAFastFile(std::vector<uint8_t> const &bytes)
{
	return bytes.size() >= 20 && memcmp(&bytes[0], "RFFL", 4) == 0;
}

static bool ExtractPictures(std::vector<uint8_t> const &fastFile, std::string const &intoFolder, int *written, int *refused)
{
	if (!IsAFastFile(fastFile))
	{
		return false;
	}

	uint32_t const count = ReadLittle32(&fastFile[8]);
	uint64_t const listBytes = ReadLittle32(&fastFile[12]);
	uint64_t const dataStart = 20 + listBytes;

	if (dataStart > fastFile.size())
	{
		return false;
	}

	size_t place = 20;

	for (uint32_t i = 0; i < count; i++)
	{
		if ((uint64_t)place + 8 >= dataStart)
		{
			return false;
		}

		uint64_t const offset = ReadLittle32(&fastFile[place]);
		uint64_t const length = ReadLittle32(&fastFile[place + 4]);
		place += 8;

		size_t end = place;

		while (end < dataStart && fastFile[end] != 0)
		{
			end++;
		}

		if (end >= dataStart)
		{
			return false;
		}

		std::string const name((char const *)&fastFile[place], end - place);
		place += (end - place + 4) & ~(size_t)3;

		if (!EndsWith(name, ".rim"))
		{
			continue;
		}

		if (!NameStaysInsideFolder(name) || dataStart + offset + length > fastFile.size())
		{
			printf("  left out, because its entry can't be right: %s\n", name.c_str());
			(*refused)++;
			continue;
		}

		std::string const path = intoFolder + "/" + name;
		MakeFoldersFor(path);

		if (WriteWholeFile(path, fastFile.data() + dataStart + offset, (size_t)length))
		{
			(*written)++;
		}
		else
		{
			printf("  couldn't write %s\n", path.c_str());
			(*refused)++;
		}
	}

	return true;
}

// ---- PNG
//
// The simplest PNG there is: four bytes a pixel and nothing squeezed, because the deflate format that PNG uses allows blocks stored as they are. Image programs can all read it, and
// this needs no compression code. The numbers that each part ends with, CRC and Adler, are how a reader knows the file isn't damaged.

static uint32_t Crc(uint8_t const *bytes, size_t size, uint32_t crc)
{
	static uint32_t table[256];
	static bool tableMade = false;

	if (!tableMade)
	{
		for (uint32_t n = 0; n < 256; n++)
		{
			uint32_t c = n;

			for (int bit = 0; bit < 8; bit++)
			{
				c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			}

			table[n] = c;
		}

		tableMade = true;
	}

	for (size_t i = 0; i < size; i++)
	{
		crc = table[(crc ^ bytes[i]) & 255] ^ (crc >> 8);
	}

	return crc;
}

static void AddBig32(std::vector<uint8_t> &to, uint32_t value)
{
	to.push_back((uint8_t)(value >> 24));
	to.push_back((uint8_t)(value >> 16));
	to.push_back((uint8_t)(value >> 8));
	to.push_back((uint8_t)value);
}

static void AddPngPart(std::vector<uint8_t> &to, char const *type, std::vector<uint8_t> const &body)
{
	AddBig32(to, (uint32_t)body.size());

	size_t const start = to.size();
	to.insert(to.end(), type, type + 4);
	to.insert(to.end(), body.begin(), body.end());

	AddBig32(to, Crc(&to[start], to.size() - start, 0xFFFFFFFFu) ^ 0xFFFFFFFFu);
}

// pixels are four bytes each, in the order red, green, blue, alpha
static std::vector<uint8_t> MakePng(uint8_t const *pixels, uint32_t width, uint32_t height)
{
	std::vector<uint8_t> png;
	static uint8_t const signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	png.insert(png.end(), signature, signature + 8);

	std::vector<uint8_t> header;
	AddBig32(header, width);
	AddBig32(header, height);
	header.push_back(8);    // eight bits for each number
	header.push_back(6);    // red, green, blue, alpha
	header.push_back(0);    // the only way of packing PNG has
	header.push_back(0);    // the only way of filtering it has
	header.push_back(0);    // not interlaced
	AddPngPart(png, "IHDR", header);

	// each row starts with a byte saying how it was filtered: 0 is not at all
	std::vector<uint8_t> rows;
	rows.reserve(((size_t)width * 4 + 1) * height);

	for (uint32_t y = 0; y < height; y++)
	{
		rows.push_back(0);
		rows.insert(rows.end(), pixels + (size_t)y * width * 4, pixels + ((size_t)y + 1) * width * 4);
	}

	// stored blocks: each has a byte saying whether it's the last, its length, and that length with every bit turned over
	std::vector<uint8_t> packed;
	packed.push_back(0x78);
	packed.push_back(0x01);

	size_t done = 0;

	do
	{
		size_t const size = (rows.size() - done < 65535) ? rows.size() - done : 65535;
		bool const last = done + size == rows.size();

		packed.push_back(last ? 1 : 0);
		packed.push_back((uint8_t)size);
		packed.push_back((uint8_t)(size >> 8));
		packed.push_back((uint8_t)~size);
		packed.push_back((uint8_t)(~size >> 8));
		packed.insert(packed.end(), rows.begin() + done, rows.begin() + done + size);
		done += size;
	}
	while (done < rows.size());

	uint32_t a = 1, b = 0;

	for (size_t i = 0; i < rows.size(); i++)
	{
		a = (a + rows[i]) % 65521;
		b = (b + a) % 65521;
	}

	AddBig32(packed, (b << 16) | a);
	AddPngPart(png, "IDAT", packed);
	AddPngPart(png, "IEND", std::vector<uint8_t>());

	return png;
}

// ---- A picture to a PNG

static bool PictureToPng(std::string const &picturePath, std::string const &pngPath, uint32_t *width, uint32_t *height)
{
	RimLoader picture;

	// This reads one picture after another, and a limit meant for a level means nothing to it
	RimLoader_NewLevel();

	if (!picture.Open(picturePath))
	{
		printf("  can't be read as a picture: %s\n", picturePath.c_str());
		return false;
	}

	picture.GetDimensions(*width, *height);

	if (*width == 0 || *height == 0 || *width > kLargestSide || *height > kLargestSide || (uint64_t)*width * *height > kMostPixels)
	{
		printf("  says it's %u x %u, which is left out: %s\n", *width, *height, picturePath.c_str());
		return false;
	}

	std::vector<uint8_t> pixels((size_t)*width * *height * 4);

	if (!picture.Decode(&pixels[0], *width * 4))
	{
		printf("  couldn't be decoded: %s\n", picturePath.c_str());
		return false;
	}

	// the game keeps blue, green, red, alpha, and PNG wants red first
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		uint8_t const blue = pixels[i];
		pixels[i] = pixels[i + 2];
		pixels[i + 2] = blue;
	}

	std::vector<uint8_t> const png = MakePng(&pixels[0], *width, *height);
	MakeFoldersFor(pngPath);

	if (!WriteWholeFile(pngPath, &png[0], png.size()))
	{
		printf("  couldn't write %s\n", pngPath.c_str());
		return false;
	}

	return true;
}

// ---- Every picture under a folder

struct Written
{
	std::string name;
	uint32_t width, height;
};

// relative is where this folder is, under the folder we started from
static void AllPicturesToPng(std::string const &fromFolder, std::string const &toFolder, std::string const &relative, std::vector<Written> &done, int *failed)
{
	_finddata_t found;
	intptr_t const search = _findfirst((fromFolder + "/" + relative + "*").c_str(), &found);

	if (search == -1)
	{
		return;
	}

	do
	{
		std::string const name = found.name;

		if (name == "." || name == "..")
		{
			continue;
		}

		if (found.attrib & _A_SUBDIR)
		{
			AllPicturesToPng(fromFolder, toFolder, relative + name + "/", done, failed);
		}
		else if (EndsWith(name, ".rim"))
		{
			Written picture;
			picture.name = relative + name.substr(0, name.size() - 4) + ".png";

			if (PictureToPng(fromFolder + "/" + relative + name, toFolder + "/" + picture.name, &picture.width, &picture.height))
			{
				done.push_back(picture);
			}
			else
			{
				(*failed)++;
			}
		}
	}
	while (_findnext(search, &found) == 0);

	_findclose(search);
}

// ---- The commands

static int Extract(std::string const &fromFolder, std::string const &toFolder)
{
	_finddata_t found;
	intptr_t const search = _findfirst((fromFolder + "/*.ffl").c_str(), &found);

	if (search == -1)
	{
		printf("There are no .ffl files in %s\n", fromFolder.c_str());
		return 1;
	}

	int written = 0, refused = 0, unreadable = 0;

	do
	{
		std::vector<uint8_t> bytes;

		if (!ReadWholeFile(fromFolder + "/" + found.name, bytes))
		{
			printf("  couldn't read %s\n", found.name);
			unreadable++;
		}
		else if (!IsAFastFile(bytes))
		{
			// common.ffl has that ending and holds the game's sounds in a form of its own, with no pictures
			printf("  no pictures in %s, because it isn't that kind of fastfile\n", found.name);
		}
		else if (!ExtractPictures(bytes, toFolder, &written, &refused))
		{
			printf("  its list of files doesn't add up: %s\n", found.name);
			unreadable++;
		}
	}
	while (_findnext(search, &found) == 0);

	_findclose(search);

	printf("%d pictures copied, %d left out, %d fastfiles couldn't be read\n", written, refused, unreadable);
	return (refused || unreadable) ? 1 : 0;
}

static int ToPng(std::string const &from, std::string const &to)
{
	if (EndsWith(from, ".rim"))
	{
		uint32_t width = 0, height = 0;

		if (!PictureToPng(from, to, &width, &height))
		{
			return 1;
		}

		printf("%u x %u\n", width, height);
		return 0;
	}

	std::vector<Written> done;
	int failed = 0;

	AllPicturesToPng(from, to, "", done, &failed);

	std::string index = "picture,width,height\n";

	for (size_t i = 0; i < done.size(); i++)
	{
		index += done[i].name + "," + std::to_string(done[i].width) + "," + std::to_string(done[i].height) + "\n";
	}

	std::string const indexPath = to + "/index.csv";
	MakeFoldersFor(indexPath);

	if (!WriteWholeFile(indexPath, index.data(), index.size()))
	{
		printf("  couldn't write %s\n", indexPath.c_str());
		failed++;
	}

	printf("%d PNGs written, %d couldn't be\n", (int)done.size(), failed);
	return failed ? 1 : 0;
}

// A map from a PNG, read back afterwards with the same checks the game uses
static int ToMap(std::string const &from, std::string const &to, MapChoices const &choices)
{
	bool const asDds = EndsWith(to, ".dds");

	if (!asDds && !EndsWith(to, ".rim"))
	{
		printf("The map's name has to end in .dds or .rim: %s\n", to.c_str());
		return 1;
	}

	std::vector<uint8_t> bytes;

	if (!ReadWholeFile(from, bytes))
	{
		printf("Couldn't read %s\n", from.c_str());
		return 1;
	}

	if (bytes.empty())
	{
		printf("%s is empty\n", from.c_str());
		return 1;
	}

	PngPicture image;
	char const *problem = Png_Read(&bytes[0], bytes.size(), &image);

	if (problem)
	{
		printf("%s can't be used, because %s\n", from.c_str(), problem);
		return 1;
	}

	uint32_t side = 0;
	MapBytes top;
	MapReport report = { 0, 0 };
	bool const isDepth = EndsWith(to, "_depth.rim") || EndsWith(to, "_depth.dds");

	if (isDepth && asDds)
	{
		printf("A depth map can only be a .rim for now, because the game doesn't read a .dds one yet: %s\n", to.c_str());
		return 1;
	}

	problem = isDepth ? Map_MakeDepthTop(image, &side, &top) : Map_MakeTop(image, choices, &side, &top, &report);

	if (problem)
	{
		printf("%s is %u x %u and can't be used as it is, because %s. --side N makes it N x N: 256, 512, 1024 or 2048, say.\n", from.c_str(), image.width, image.height, problem);
		return 1;
	}

	uint32_t largestError = 0;
	MapBytes file;

	if (asDds)
	{
		file = Map_MakeDds(Map_MakeAllCopies(top, side), side);
		problem = Map_CheckDds(file, top, side, &largestError);
	}
	else
	{
		file = Map_MakeRim(top, side);
		problem = Map_CheckRim(file, top, side);
	}

	if (problem)
	{
		printf("What was made didn't pass the game's checks, so nothing was written, because %s\n", problem);
		return 1;
	}

	MakeFoldersFor(to);

	if (!WriteWholeFile(to, &file[0], file.size()))
	{
		printf("Couldn't write %s\n", to.c_str());
		return 1;
	}

	printf("%s: %u x %u, %u bytes on disk, %u bytes in memory\n", to.c_str(), side, side, (unsigned)file.size(), (unsigned)(asDds ? ReliefMap_DxtBytes(side) : ReliefMap_PlainBytes(side)));

	if (asDds)
	{
		printf("  packing moved x and y by at most %u out of 255\n", largestError);
	}

	if (report.pixelsMadeFlat)
	{
		printf("  %u pixels are too short to say which way they face, such as a grey frame, and were made flat\n", report.pixelsMadeFlat);
	}

	if (report.pixelsFacingBack)
	{
		printf("  %u pixels pointed into the surface, and now lie along it\n", report.pixelsFacingBack);
	}

	return 0;
}

static int RunTool(int argc, char **argv)
{
	std::string const command = (argc > 1) ? argv[1] : "";

	if (argc >= 4 && command == "tomap")
	{
		MapChoices choices = { false, false, 0 };

		for (int i = 4; i < argc; i++)
		{
			std::string const option = argv[i];

			if (option == "--flip-green")
			{
				choices.flipGreen = true;
			}
			else if (option == "--flip-both")
			{
				choices.flipGreen = true;
				choices.flipRed = true;
			}
			else if (option == "--side" && i + 1 < argc)
			{
				choices.side = (uint32_t)strtoul(argv[++i], 0, 10);

				if (choices.side == 0)
				{
					printf("--side needs a number: 256, 512, 1024 or 2048, say\n");
					return 1;
				}
			}
			else
			{
				printf("I don't know %s\n", option.c_str());
				return 1;
			}
		}

		return ToMap(argv[2], argv[3], choices);
	}

	if (argc == 4 && command == "extract")
	{
		return Extract(argv[2], argv[3]);
	}

	if (argc == 4 && command == "topng")
	{
		return ToPng(argv[2], argv[3]);
	}

	printf("rimpng extract <folder of copied .ffl files> <folder for the pictures>\n");
	printf("rimpng topng   <picture.rim> <picture.png>\n");
	printf("rimpng topng   <folder of pictures> <folder for the PNGs>\n");
	printf("rimpng tomap   <map.png> <name_Normal.dds or name_Normal.rim> [--flip-green] [--flip-both] [--side N]\n");
	printf("rimpng tomap   <depth.png> <name_Depth.rim>\n");
	return 1;
}

// A picture can ask for a lot of memory, and asking for more than there is throws, which would stop the program with no word about why
static int RunWithMemoryCheck(int (*run)(int, char **), int argc, char **argv)
{
	try
	{
		return run(argc, argv);
	}
	catch (std::bad_alloc const &)
	{
		printf("There isn't the memory for that. Nothing more was written.\n");
		return 1;
	}
}

#ifndef RIMPNG_TEST
int main(int argc, char **argv)
{
	return RunWithMemoryCheck(RunTool, argc, argv);
}
#endif
