// The test for 3dc/texture_relief.cpp, which works out a relief picture for each of a level's textures. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The calculation itself, on made-up pictures whose right answers can be worked out by hand: flat, sloping, repeating and with holes. Then what a hostile
// file could do: sizes that are too big or nonsense, and memory that ends exactly at the end of a picture. Then the part that talks to the game, against
// stand-ins for the game's texture list that can be told to fail: every failure has to leave no relief picture behind and no crash, and turned off, it
// has to make nothing at all.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the real texture_relief.cpp, and the program gives back a non-zero result if any check failed.

#include <direct.h>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <windows.h>

#include "texture_relief.h"
#include "relief_map_file.h"
#include "FastFile.h"
#include "utilities.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- stand-ins for the game's list of textures

struct StandInTexture
{
	Texture details;
	std::vector<uint8_t> pixels;
	uint32_t pitch;
	bool alive;
};

static std::vector<StandInTexture> textureList;

texID_t NO_TEXTURE = 0;        // the same starting numbers as the game's
texID_t MISSING_TEXTURE = 1;

static int createsLeft = 1000000;    // how many more makes may work
static int createsToFail = 0;        // how many makes fail first, before that count starts
static int locksLeft = 1000000;      // how many more locks may work
static int madeCount = 0;
static int lockCount = 0;
static int unlockCount = 0;
static bool cardShrinksPictures = false;

static std::vector<std::string> logLines;

void LogString(const std::string &text)
{
	logLines.push_back(text);
}

static texID_t AddTexture(const std::string &name, uint32_t width, uint32_t height, uint32_t bits, TextureUsage usage)
{
	StandInTexture texture;
	texture.details.name = name;
	texture.details.usage = usage;
	texture.details.realWidth = (uint16_t)width;
	texture.details.realHeight = (uint16_t)height;
	texture.details.bitsPerPixel = (uint8_t)bits;
	texture.pitch = width * 4;
	texture.pixels.resize((size_t)texture.pitch * height);
	texture.alive = true;
	textureList.push_back(texture);
	return textureList.size() - 1;
}

texID_t Tex_Create(const std::string &textureName, uint32_t width, uint32_t height, uint32_t bitsPerPixel, enum TextureUsage usageType)
{
	if (createsToFail > 0)
	{
		createsToFail--;
		return MISSING_TEXTURE;
	}

	if (createsLeft-- <= 0)
	{
		return MISSING_TEXTURE;
	}

	madeCount++;

	texID_t const made = AddTexture(textureName, width, height, bitsPerPixel, usageType);

	// the memory stays the size asked for, so that a test of the check that catches this can't damage anything if the check is missing
	if (cardShrinksPictures)
	{
		textureList[made].details.realHeight /= 2;
	}

	return made;
}

const Texture &Tex_GetTextureDetails(texID_t textureID)
{
	return textureList.at(textureID).details;
}

bool Tex_Lock(texID_t textureID, uint8_t **data, uint32_t *pitch, enum TextureLock)
{
	if (locksLeft-- <= 0)
	{
		return false;
	}

	lockCount++;
	*data = &textureList.at(textureID).pixels[0];
	*pitch = textureList.at(textureID).pitch;
	return true;
}

bool Tex_Unlock(texID_t)
{
	unlockCount++;
	return true;
}

void Tex_Release(texID_t textureID)
{
	textureList.at(textureID).alive = false;
}

// A hand-made map: the stand-in keeps each copy as a picture of its own, and a card that won't make a texture this big is a count like the others.
struct StandInCopies
{
	texID_t id;
	std::vector<std::vector<uint8_t> > levels;
};

static std::vector<StandInCopies> copiesList;
static uint32_t largestSideCardMakes = 16384;

static StandInCopies *CopiesOf(texID_t id)
{
	for (size_t i = 0; i < copiesList.size(); i++)
	{
		if (copiesList[i].id == id)
		{
			return &copiesList[i];
		}
	}

	return 0;
}

texID_t Tex_CreateWithCopies(const std::string &textureName, uint32_t side, uint32_t levels)
{
	if (createsToFail > 0)
	{
		createsToFail--;
		return MISSING_TEXTURE;
	}

	if (createsLeft-- <= 0 || side > largestSideCardMakes)
	{
		return MISSING_TEXTURE;
	}

	madeCount++;

	StandInCopies copies;
	copies.id = AddTexture(textureName, 1, 1, 32, TextureUsage_Normal);

	for (uint32_t level = 0; level < levels; level++)
	{
		uint32_t const levelSide = ReliefMap_LevelSide(side, level);
		copies.levels.push_back(std::vector<uint8_t>((size_t)levelSide * levelSide * 4 * 2, 0xEE));    // twice the room, with a pitch to match, to show rows are found from the pitch
	}

	copiesList.push_back(copies);
	return copies.id;
}

bool Tex_LockCopy(texID_t textureID, uint32_t level, uint8_t **data, uint32_t *pitch)
{
	if (locksLeft-- <= 0)
	{
		return false;
	}

	lockCount++;
	StandInCopies *copies = CopiesOf(textureID);
	*data = &copies->levels.at(level)[0];

	uint32_t levelSide = 1;

	while ((size_t)levelSide * levelSide * 4 * 2 < copies->levels.at(level).size())
	{
		levelSide *= 2;
	}

	*pitch = levelSide * 4 * 2;
	return true;
}

bool Tex_UnlockCopy(texID_t, uint32_t)
{
	unlockCount++;
	return true;
}

static void StartAgain()
{
	textureList.clear();
	AddTexture("first", 1, 1, 32, TextureUsage_Normal);    // the game's two special textures have the first two numbers
	AddTexture("second", 1, 1, 32, TextureUsage_Normal);
	createsLeft = 1000000;
	createsToFail = 0;
	locksLeft = 1000000;
	madeCount = 0;
	lockCount = 0;
	unlockCount = 0;
	cardShrinksPictures = false;
	logLines.clear();
	copiesList.clear();
	largestSideCardMakes = 16384;
}

static int TexturesAlive()
{
	int alive = 0;

	for (size_t i = 2; i < textureList.size(); i++)
	{
		alive += textureList[i].alive ? 1 : 0;
	}

	return alive;
}

// ---- made-up pictures, four bytes a pixel: blue, green, red, alpha

static void SetPixel(std::vector<uint8_t> &picture, uint32_t width, uint32_t x, uint32_t y, int blue, int green, int red, int alpha)
{
	uint8_t *pixel = &picture[(size_t)(y * width + x) * 4];
	pixel[0] = (uint8_t)blue;
	pixel[1] = (uint8_t)green;
	pixel[2] = (uint8_t)red;
	pixel[3] = (uint8_t)alpha;
}

// Grey the same in all three colours comes back as exactly that number: 29 + 150 + 77 is 256.
static std::vector<uint8_t> GreyPicture(uint32_t width, uint32_t height, int grey)
{
	std::vector<uint8_t> picture((size_t)width * height * 4);

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			SetPixel(picture, width, x, y, grey, grey, grey, 255);
		}
	}

	return picture;
}

static std::vector<uint8_t> ReliefOf(std::vector<uint8_t> const &picture, uint32_t width, uint32_t height)
{
	std::vector<uint8_t> relief((size_t)width * height * 4);
	bool const worked = TextureRelief::Calculate(&picture[0], width * 4, width, height, &relief[0], width * 4);

	if (!worked)
	{
		relief.clear();
		Check(false, "a good picture was refused");
	}

	return relief;
}

static bool PixelIs(std::vector<uint8_t> const &relief, uint32_t width, uint32_t x, uint32_t y, int blue, int green, int red, int alpha)
{
	uint8_t const *pixel = &relief[(size_t)(y * width + x) * 4];
	return pixel[0] == blue && pixel[1] == green && pixel[2] == red && pixel[3] == alpha;
}

static void CheckTheCalculation()
{
	// A flat picture has no slopes, whatever its brightness.
	{
		uint32_t const size = 8;
		std::vector<uint8_t> const relief = ReliefOf(GreyPicture(size, size, 77), size, size);
		bool allFlat = true;

		for (uint32_t i = 0; i < size * size; i++)
		{
			allFlat = allFlat && PixelIs(relief, size, i % size, i / size, 255, 128, 128, 77);
		}

		Check(allFlat, "a flat picture comes out exactly flat, with its brightness as its height");
	}

	// Which colour counts for how much, and that the bytes are read in the right order.
	{
		uint32_t const size = 6;
		std::vector<uint8_t> blue((size_t)size * size * 4), green(blue), red(blue);

		for (uint32_t i = 0; i < size * size; i++)
		{
			SetPixel(blue, size, i % size, i / size, 200, 0, 0, 255);
			SetPixel(green, size, i % size, i / size, 0, 200, 0, 255);
			SetPixel(red, size, i % size, i / size, 0, 0, 200, 255);
		}

		Check(PixelIs(ReliefOf(blue, size, size), size, 2, 2, 255, 128, 128, 22), "blue counts for 29 of 256");
		Check(PixelIs(ReliefOf(green, size, size), size, 2, 2, 255, 128, 128, 117), "green counts for 150 of 256");
		Check(PixelIs(ReliefOf(red, size, size), size, 2, 2, 255, 128, 128, 60), "red counts for 77 of 256");
	}

	// A slope worked by hand. Brightness rises 10 a pixel, so depth rises 10 too, a step lower because of the crack. 10 over 32 leans the arrow 0.3125 back
	// against 1 out, and an arrow 1 long is -0.2983 and 0.9545, which are stored as 90 and 249. Only pixels with the whole slope under the mask are read.
	{
		uint32_t const size = 24;
		std::vector<uint8_t> across((size_t)size * size * 4), down(across);

		for (uint32_t y = 0; y < size; y++)
		{
			for (uint32_t x = 0; x < size; x++)
			{
				SetPixel(across, size, x, y, 10 * x, 10 * x, 10 * x, 255);
				SetPixel(down, size, x, y, 10 * y, 10 * y, 10 * y, 255);
			}
		}

		std::vector<uint8_t> const reliefAcross = ReliefOf(across, size, size);
		std::vector<uint8_t> const reliefDown = ReliefOf(down, size, size);

		Check(PixelIs(reliefAcross, size, 12, 12, 249, 128, 90, 110), "a surface rising to the right leans to the left, by the worked amount");
		Check(PixelIs(reliefDown, size, 12, 12, 249, 90, 128, 110), "a surface rising to the bottom leans to the top, by the worked amount");
	}

	// Pictures repeat, so one bright column at the very left has to show at the very right too, and rows the same.
	{
		uint32_t const size = 16;
		std::vector<uint8_t> column = GreyPicture(size, size, 50);
		std::vector<uint8_t> row = column;

		for (uint32_t i = 0; i < size; i++)
		{
			SetPixel(column, size, 0, i, 150, 150, 150, 255);
			SetPixel(row, size, i, 0, 150, 150, 150, 255);
		}

		std::vector<uint8_t> const reliefColumn = ReliefOf(column, size, size);
		std::vector<uint8_t> const reliefRow = ReliefOf(row, size, size);

		int const rightOfIt = reliefColumn[(size * 4 + 1) * 4 + 2] - 128;      // red, one pixel to the right of the column
		int const leftOfIt = reliefColumn[(size * 4 + size - 1) * 4 + 2] - 128;   // red at the far right, which is one pixel to the left of it
		int const belowIt = reliefRow[(size + 4) * 4 + 1] - 128;               // green, one row below
		int const aboveIt = reliefRow[((size - 1) * size + 4) * 4 + 1] - 128;  // green on the last row, which is one row above it

		Check(rightOfIt != 0 && leftOfIt == -rightOfIt, "the mask carries on round the left and right edges");
		Check(belowIt != 0 && aboveIt == -belowIt, "the mask carries on round the top and bottom edges");
	}

	// A hole's own colour must leave no mark: this one is white, in a grey picture.
	{
		uint32_t const size = 16;
		std::vector<uint8_t> picture = GreyPicture(size, size, 100);

		for (uint32_t y = 6; y < 10; y++)
		{
			for (uint32_t x = 6; x < 10; x++)
			{
				SetPixel(picture, size, x, y, 255, 255, 255, 0);
			}
		}

		std::vector<uint8_t> const relief = ReliefOf(picture, size, size);
		bool allFlat = true;

		for (uint32_t i = 0; i < size * size; i++)
		{
			allFlat = allFlat && PixelIs(relief, size, i % size, i / size, 255, 128, 128, 100);
		}

		Check(allFlat, "a hole in a flat picture leaves it flat, with no rim");
	}

	// Bright pixels beside black ones: pressing the black side in would go below zero, and has to stop at zero.
	{
		uint32_t const size = 16;
		std::vector<uint8_t> picture = GreyPicture(size, size, 0);

		for (uint32_t i = 0; i < size; i++)
		{
			SetPixel(picture, size, 8, i, 255, 255, 255, 255);
		}

		Check(ReliefOf(picture, size, size)[(4 * size + 7) * 4 + 3] == 0, "a crack can't go deeper than the deepest there is");
	}

	// The only solid pixel is the very last, so the first pass can't reach the holes before it. Only the second can.
	{
		uint32_t const size = 8;
		std::vector<uint8_t> picture = GreyPicture(size, size, 255);

		for (uint32_t i = 0; i < size * size; i++)
		{
			SetPixel(picture, size, i % size, i / size, 255, 255, 255, 0);
		}

		SetPixel(picture, size, size - 1, size - 1, 60, 60, 60, 255);

		Check(PixelIs(ReliefOf(picture, size, size), size, 0, 0, 255, 128, 128, 60), "one solid pixel fills every hole, however far away");
	}

	// tiny pictures, where the mask wraps round more than once
	{
		std::vector<uint8_t> const one = ReliefOf(GreyPicture(1, 1, 9), 1, 1);
		std::vector<uint8_t> const tall = ReliefOf(GreyPicture(1, 7, 9), 1, 7);
		std::vector<uint8_t> const wide = ReliefOf(GreyPicture(7, 1, 9), 7, 1);
		std::vector<uint8_t> const two = ReliefOf(GreyPicture(2, 2, 9), 2, 2);

		Check(PixelIs(one, 1, 0, 0, 255, 128, 128, 9), "one pixel");
		Check(PixelIs(tall, 1, 0, 6, 255, 128, 128, 9), "one pixel wide");
		Check(PixelIs(wide, 7, 6, 0, 255, 128, 128, 9), "one pixel high");
		Check(PixelIs(two, 2, 1, 1, 255, 128, 128, 9), "two pixels each way");
	}
}

// Memory that stops dead: reading or writing a byte past it (or before it) stops the program at once, so nothing can get away with it by luck.
static uint8_t *MemoryAgainstAWall(size_t bytes, bool wallAfter)
{
	size_t const page = 4096;
	size_t const pages = (bytes + page - 1) / page;
	uint8_t *const block = (uint8_t *)VirtualAlloc(0, (pages + 1) * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	DWORD old = 0;

	if (!block)
	{
		return 0;
	}

	if (wallAfter)
	{
		VirtualProtect(block + pages * page, page, PAGE_NOACCESS, &old);
		return block + pages * page - bytes;
	}

	VirtualProtect(block, page, PAGE_NOACCESS, &old);
	return block + page;
}

static void CheckHostileInput()
{
	struct SizeRow { uint32_t width, height; bool fine; char const *what; };

	SizeRow const sizeRows[] =
	{
		{ 0,          8,          false, "no width" },
		{ 8,          0,          false, "no height" },
		{ 1025,       8,          false, "one too wide" },
		{ 8,          1025,       false, "one too high" },
		{ 65535,      65535,      false, "the largest a file can say" },
		{ 0x40000000, 4,          false, "a width that overflows when multiplied by four" },
		{ 0xFFFFFFFF, 0xFFFFFFFF, false, "the largest numbers there are" },
		{ 1024,       1024,       true,  "the largest allowed" },
		{ 1,          1,          true,  "the smallest allowed" },
	};

	for (size_t i = 0; i < sizeof(sizeRows) / sizeof(sizeRows[0]); i++)
	{
		Check(TextureRelief::SizeIsFine(sizeRows[i].width, sizeRows[i].height) == sizeRows[i].fine, sizeRows[i].what);
	}

	// A size that's refused has to stop the calculation before it touches anything: these pictures are far too small for what they claim.
	{
		uint8_t picture[16] = { 0 };
		uint8_t relief[16] = { 0 };

		Check(!TextureRelief::Calculate(picture, 4, 0x40000000, 4, relief, 4), "a huge width stops the calculation");
		Check(!TextureRelief::Calculate(picture, 16, 1025, 1, relief, 16), "one too wide stops the calculation");
		Check(!TextureRelief::Calculate(0, 16, 4, 1, relief, 16), "no picture");
		Check(!TextureRelief::Calculate(picture, 16, 4, 1, 0, 16), "nowhere to put the answer");
	}

	// pitches
	{
		uint32_t const width = 4, height = 4;
		std::vector<uint8_t> const tight = GreyPicture(width, height, 66);
		std::vector<uint8_t> relief(16 * 4 + 64, 0xAB);

		Check(!TextureRelief::Calculate(&tight[0], width * 4 - 1, width, height, &relief[0], width * 4), "a picture's rows shorter than its width");
		Check(!TextureRelief::Calculate(&tight[0], width * 4, width, height, &relief[0], width * 4 - 1), "a relief picture's rows shorter than its width");
		Check(relief[0] == 0xAB, "and neither one touched the answer");

		// rows with spare bytes after them: bright rubbish in the picture's spare bytes means nothing, and the relief picture's spare bytes are left alone
		std::vector<uint8_t> roomy((width * 4 + 8) * height, 255);

		for (uint32_t y = 0; y < height; y++)
		{
			memcpy(&roomy[y * (width * 4 + 8)], &tight[y * width * 4], width * 4);
		}

		bool const worked = TextureRelief::Calculate(&roomy[0], width * 4 + 8, width, height, &relief[0], width * 4 + 8);

		Check(worked && relief[0] == 255 && relief[1] == 128 && relief[2] == 128 && relief[3] == 66, "rows with spare bytes are read and written properly");
		Check(relief[width * 4] == 0xAB && relief[(width * 4 + 8) * 3 + width * 4] == 0xAB, "the spare bytes of the answer's rows aren't touched");
	}

	// up against the wall, both ways, at every size that wraps differently
	uint32_t const sizes[] = { 1, 2, 3, 5, 16, 128 };

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		uint32_t const size = sizes[i];
		size_t const bytes = (size_t)size * size * 4;

		for (int wall = 0; wall < 2; wall++)
		{
			uint8_t *const picture = MemoryAgainstAWall(bytes, wall == 0);
			uint8_t *const relief = MemoryAgainstAWall(bytes, wall == 0);

			if (!picture || !relief)
			{
				Check(false, "memory for the wall test");
				continue;
			}

			// half the pixels are holes, so the filling-in runs at the walls too
			for (size_t place = 0; place < bytes; place++)
			{
				picture[place] = (uint8_t)((place & 7) ? place * 37 : 0);
			}

			bool const worked = TextureRelief::Calculate(picture, size * 4, size, size, relief, size * 4);

			Check(worked, (wall == 0) ? "nothing reads or writes past the end of a picture" : "nothing reads or writes before the start of a picture");
		}
	}
}

static void CheckTheGameSide()
{
	uint32_t const size = 8;
	std::vector<uint8_t> const pixels = GreyPicture(size, size, 90);

	struct Added
	{
		static texID_t Picture(std::vector<uint8_t> const &from)
		{
			texID_t const id = AddTexture("graphics/wall.rim", 8, 8, 32, TextureUsage_Normal);
			memcpy(&textureList[id].pixels[0], &from[0], from.size());
			return id;
		}
	};

	// Turned off, nothing at all happens. This is what keeps -norelief the same as today.
	{
		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);
		texID_t const picture = Added::Picture(pixels);

		relief.TurnOff();
		relief.Build(picture);

		Check(!relief.IsOn() && !relief.IsReady(), "turned off, it says so");
		Check(madeCount == 0 && lockCount == 0, "turned off, it makes nothing and reads nothing");
		Check(relief.PicturesHeld() == 0 && relief.BytesHeld() == 0, "turned off, it holds nothing");
	}

	// The real thing. The game gives its special textures their numbers after this object is made, so this uses them the other way round from the start.
	{
		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);
		MISSING_TEXTURE = 0;
		NO_TEXTURE = 1;
		texID_t const picture = Added::Picture(pixels);
		texID_t const other = Added::Picture(pixels);

		Check(!relief.IsReady(), "with no level loaded, it isn't ready");

		relief.Build(picture);
		relief.Build(picture);
		texID_t const made = relief.PictureFor(picture);

		Check(relief.IsReady(), "after a texture, it's ready");
		Check(madeCount == 2, "the same texture twice makes one relief picture, and one flat one");
		Check(relief.PicturesHeld() == 1 && relief.BytesHeld() == size * size * 4, "it counts what it holds");
		Check(PixelIs(textureList[made].pixels, size, 3, 3, 255, 128, 128, 90), "the relief picture holds the calculation's answer");
		Check(lockCount == unlockCount, "everything locked is unlocked");

		texID_t const flat = relief.PictureFor(other);

		Check(flat != made && PixelIs(textureList[flat].pixels, 1, 0, 0, 255, 128, 128, 128), "a texture with none gets the flat picture");
		Check(relief.PictureFor(1000) == flat, "a number nobody has heard of gets the flat picture");

		relief.ReleaseAll();

		Check(TexturesAlive() == 2, "giving everything back leaves only the two textures of the level");
		Check(!relief.IsReady() && relief.PicturesHeld() == 0 && relief.BytesHeld() == 0, "and it holds nothing");

		relief.Build(picture);
		Check(relief.IsReady() && relief.PicturesHeld() == 1, "the next level starts clean, even with the same texture number");

		relief.TurnOff();
		Check(!relief.IsReady(), "turned off, it isn't ready, even holding pictures");

		relief.ReleaseAll();
		MISSING_TEXTURE = 1;
		NO_TEXTURE = 0;
	}

	// Textures that must go without, and things that go wrong. All end the same way: no relief picture, nothing left behind.
	struct FailureRow { int createsToFail, createsLeft, locksLeft; int made; TextureUsage usage; uint32_t bits, width; bool flatStays; char const *what; };

	FailureRow const failureRows[] =
	{
		{ 0, 1000, 1000, 1, TextureUsage_Dynamic, 32, 8,    true,  "a film gets none, and nothing is made for it" },
		{ 0, 1000, 1000, 1, TextureUsage_Normal,  16, 8,    true,  "a picture in any other format gets none, and nothing is made for it" },
		{ 0, 1000, 1000, 1, TextureUsage_Normal,  32, 1025, true,  "a picture too large gets none, and nothing is made for it" },
		{ 1, 1000, 1000, 0, TextureUsage_Normal,  32, 8,    false, "the flat picture can't be made" },
		{ 0, 1000, 0,    1, TextureUsage_Normal,  32, 8,    false, "the flat picture can't be locked" },
		{ 0, 1,    1000, 1, TextureUsage_Normal,  32, 8,    true,  "the relief picture can't be made" },
		{ 0, 1000, 1,    2, TextureUsage_Normal,  32, 8,    true,  "the texture can't be locked" },
		{ 0, 1000, 2,    2, TextureUsage_Normal,  32, 8,    true,  "the relief picture can't be locked" },
	};

	for (size_t i = 0; i < sizeof(failureRows) / sizeof(failureRows[0]); i++)
	{
		FailureRow const &row = failureRows[i];

		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);

		texID_t const picture = AddTexture("graphics/wall.rim", row.width, 8, row.bits, row.usage);
		createsToFail = row.createsToFail;
		createsLeft = row.createsLeft;
		locksLeft = row.locksLeft;

		relief.Build(picture);

		int const expected = row.flatStays ? 2 : 1;   // the texture, and the flat picture if that worked

		Check(relief.PicturesHeld() == 0 && madeCount == row.made && TexturesAlive() == expected && lockCount == unlockCount, row.what);
		Check(relief.IsReady() == row.flatStays, row.flatStays ? "  and everything else still works" : "  and the surface pass stays off");

		relief.ReleaseAll();
	}

	// The card makes the relief picture shorter than the texture, which leaves its rows the right length, so only the check on the size can catch it.
	{
		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);
		texID_t const first = Added::Picture(pixels);
		texID_t const second = Added::Picture(pixels);

		relief.Build(first);   // makes the flat picture, so that only the relief picture comes out wrong

		int const aliveBefore = TexturesAlive();
		int const locksBefore = lockCount;

		cardShrinksPictures = true;
		relief.Build(second);

		Check(relief.PicturesHeld() == 1 && TexturesAlive() == aliveBefore, "a relief picture the card made a different size is given back");
		Check(lockCount == locksBefore, "and it isn't written to");
	}

	// The most a level's relief pictures may add up to, with room for exactly two: the third goes without, and the limit doesn't stop anything else.
	{
		StartAgain();
		TextureRelief relief(size * size * 4 * 2);
		relief.SetAutoGenerate(true);
		texID_t const pictures[3] = { Added::Picture(pixels), Added::Picture(pixels), Added::Picture(pixels) };

		relief.Build(pictures[0]);
		relief.Build(pictures[1]);
		int const madeBefore = madeCount;
		relief.Build(pictures[2]);

		Check(relief.PicturesHeld() == 2 && relief.BytesHeld() == size * size * 4 * 2, "pictures that exactly fill the limit are all kept");
		Check(madeCount == madeBefore, "the next one goes without, and nothing is made for it");
		Check(logLines.size() == 1 && logLines[0].find("limit") != std::string::npos, "and the log says so");

		relief.Build(Added::Picture(pixels));
		Check(logLines.size() == 1, "once, however many more go without");

		relief.ReleaseAll();
		relief.Build(pictures[2]);
		Check(relief.PicturesHeld() == 1, "the next level starts with the whole limit again");

		relief.Build(Added::Picture(pixels));
		relief.Build(Added::Picture(pixels));
		Check(logLines.size() == 2, "and the log can say so again");

		relief.ReleaseAll();
	}

	// What the log shows when a level finishes loading: it's the only way to tell from a log that relief ran.
	{
		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);

		relief.Build(Added::Picture(pixels));
		relief.LogWhatIsHeld();

		Check(logLines.size() == 2 && logLines[0] == "texture relief: 1 pictures held, 256 bytes", "the log says what's held");
		Check(logLines.size() == 2 && logLines[1] == "texture relief: this level has 0 hand-made, 1 worked out and 0 flat. Hand-made maps hold 0 bytes", "and what this level has of each kind");

		relief.TurnOff();
		logLines.clear();
		relief.LogWhatIsHeld();

		Check(logLines.size() == 1 && logLines[0] == "texture relief: off", "and turned off, it says that");
	}

	// A number too high is refused before it's looked up, so the list doesn't grow. There's no texture there, and it would stop the test if it were read.
	{
		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);

		relief.Build(65536);
		relief.Build(0);
		relief.Build(1);

		Check(madeCount == 0 && lockCount == 0, "a number too high and the two special textures get none");
	}
}

// ---- Hand-made maps, as real files, through the game's real file reader

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

static std::string MapFolder()
{
	static std::string folder;

	if (folder.empty())
	{
		char *name = _tempnam(NULL, "reliefmap");
		folder = name ? name : "reliefmaptest";
		free(name);
		_mkdir(folder.c_str());

		for (size_t i = 0; i < folder.size(); i++)
		{
			folder[i] = (folder[i] == '\\') ? '/' : folder[i];
		}
	}

	return folder;
}

typedef std::vector<uint8_t> Bytes;

static void Add32(Bytes &to, uint32_t value)
{
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		to.push_back((uint8_t)(value >> shift));
	}
}

static void AddWords(Bytes &to, char const *words)
{
	to.insert(to.end(), words, words + strlen(words));
}

// A deep .rim with every pixel the same, and planes of its own if asked
static Bytes DeepMap(uint32_t side, uint8_t x, uint8_t y, uint8_t z, uint32_t planes = 24)
{
	Bytes file;
	uint32_t const rowBytes = (side + 7) / 8;

	AddWords(file, "FORM");
	Add32(file, 0);
	AddWords(file, "ILBMBMHD");
	Add32(file, 20);
	file.push_back((uint8_t)(side >> 8));
	file.push_back((uint8_t)side);
	file.push_back((uint8_t)(side >> 8));
	file.push_back((uint8_t)side);
	Add32(file, 0);
	file.push_back((uint8_t)planes);
	file.push_back(0);
	file.push_back(0);
	file.push_back(0);
	Add32(file, 0);
	Add32(file, 0);
	AddWords(file, "BODY");
	Add32(file, rowBytes * planes * side);

	uint8_t const values[3] = { x, y, z };

	for (uint32_t row = 0; row < side; row++)
	{
		for (uint32_t plane = 0; plane < planes; plane++)
		{
			file.insert(file.end(), rowBytes, ((values[(plane / 8) % 3] >> (plane % 8)) & 1) ? 0xFF : 0x00);
		}
	}

	return file;
}

// A deep .rim leaning one way on its left half and the other way on its right half, so that each smaller copy can be told from the one above
static Bytes LeftAndRightMap(uint32_t side)
{
	Bytes file = DeepMap(side, 0, 128, 180);
	size_t const body = std::string((char const *)&file[0], file.size()).find("BODY") + 8;
	uint32_t const rowBytes = (side + 7) / 8;

	// The red planes are the first 8 of each row. Left is 200 and right is 56. This only works for a whole number of bytes a row, and each half is a whole number of bytes.
	for (uint32_t row = 0; row < side; row++)
	{
		for (uint32_t plane = 0; plane < 8; plane++)
		{
			for (uint32_t across = 0; across < rowBytes; across++)
			{
				uint8_t const value = (across < rowBytes / 2) ? 200 : 56;
				file[body + (size_t)row * 24 * rowBytes + (size_t)plane * rowBytes + across] = ((value >> plane) & 1) ? 0xFF : 0x00;
			}
		}
	}

	return file;
}

static void WriteMapFile(std::string const &name, Bytes const &bytes)
{
	std::string const path = MapFolder() + "/" + name;
	FILE *file = fopen(path.c_str(), "wb");

	if (file)
	{
		if (!bytes.empty())
		{
			fwrite(&bytes[0], 1, bytes.size(), file);
		}

		fclose(file);
	}
}

static void RemoveMapFile(std::string const &name)
{
	remove((MapFolder() + "/" + name).c_str());
}

static bool LogHas(std::string const &words)
{
	for (size_t i = 0; i < logLines.size(); i++)
	{
		if (logLines[i].find(words) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

static int LogCount(std::string const &words)
{
	int count = 0;

	for (size_t i = 0; i < logLines.size(); i++)
	{
		count += (logLines[i].find(words) != std::string::npos) ? 1 : 0;
	}

	return count;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

static void CheckHandMadeMaps()
{
	printf("Hand-made maps:\n");

	std::string const wall = MapFolder() + "/wall.rim";
	std::string const mapFile = "wall_Normal.rim";
	std::string const mapPath = MapFolder() + "/" + mapFile;

	RemoveMapFile(mapFile);

	// Nothing there, and working one out not asked for: this is what most textures are now
	{
		StartAgain();
		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		Check(!relief.AutoGenerates(), "working maps out is off to start with");

		relief.Build(picture);

		Check(madeCount == 1 && relief.PicturesHeld() == 0 && relief.PictureFor(picture) != picture, "with no file and it not asked for, there's only the flat picture");
		Check(lockCount == 1, "and the texture itself isn't read");

		relief.LogWhatIsHeld();
		Check(LogHas("this level has 0 hand-made, 0 worked out and 1 flat"), "the log counts it as flat");
	}

	// A map, larger than its picture
	{
		StartAgain();
		WriteMapFile(mapFile, DeepMap(16, 200, 90, 180));

		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);
		relief.Build(picture);

		texID_t const map = relief.PictureFor(picture);
		StandInCopies const *copies = CopiesOf(map);

		Check(copies != 0, "a map beside its picture is used, and it can be larger than the picture");
		Check(madeCount == 2, "once, however many times it's asked for, with one flat picture");
		Check(relief.PicturesHeld() == 1 && relief.BytesHeld() == 0 && relief.HandMadeBytesHeld() == ReliefMap_PlainBytes(16), "it's counted as a hand-made map, at what it really holds");
		Check(textureList[map].details.name == mapPath, "it's named after its file");

		if (copies)
		{
			Check(copies->levels.size() == 5, "it has its smaller copies, all the way down");

			std::vector<uint8_t> const &top = copies->levels[0];
			Check(top[0] == 180 && top[1] == 90 && top[2] == 200 && top[3] == 200, "each pixel is blue z, green y, red x, and alpha x");

			uint32_t const pitch = 16 * 4 * 2;
			Check(top[pitch] == 180 && top[pitch + 2] == 200 && top[16 * 4] == 0xEE, "rows are found from the card's pitch, and nothing is written past a row");

			std::vector<uint8_t> const &last = copies->levels[4];
			Check(last[2] > 128 && last[1] < 128 && last[0] > 128 && last[3] == last[2], "the last copy, 1 x 1, still leans the same way");
			Check(last[4] == 0xEE, "and it's only one pixel");
		}

		Check(lockCount == unlockCount, "everything locked is unlocked");
		Check(LogCount("texture relief: using " + mapPath + ", 16 x 16, with 5 copies and " + std::to_string(ReliefMap_PlainBytes(16)) + " bytes, for a picture of 8 x 8") == 1, "the log says which file and what size, once");

		relief.LogWhatIsHeld();
		Check(LogHas("this level has 1 hand-made, 0 worked out and 0 flat. Hand-made maps hold " + std::to_string(ReliefMap_PlainBytes(16)) + " bytes"), "and counts it as hand-made");

		// The next level has the same picture
		logLines.clear();
		relief.Build(picture);
		relief.LogWhatIsHeld();
		Check(LogHas("this level has 1 hand-made, 0 worked out and 0 flat"), "the next level counts it afresh");
		Check(!LogHas("using"), "without reading the file again");

		// Hand-made stays whatever the switch says
		relief.SetAutoGenerate(true);
		Check(relief.PictureFor(picture) == map, "with working maps out on, a hand-made one stays");
		relief.SetAutoGenerate(false);
		Check(relief.PictureFor(picture) == map, "and with it off");

		relief.ReleaseAll();
		Check(relief.HandMadeBytesHeld() == 0 && !textureList[map].alive, "giving everything back gives it back too");

		RemoveMapFile(mapFile);
	}

	// A map is what's used even when working one out is on, and nothing's worked out for it
	{
		StartAgain();
		WriteMapFile(mapFile, DeepMap(8, 200, 90, 180));

		TextureRelief relief;
		relief.SetAutoGenerate(true);
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);

		Check(CopiesOf(relief.PictureFor(picture)) != 0 && relief.BytesHeld() == 0 && madeCount == 2, "with working maps out on, a hand-made map still comes first, and nothing is worked out");

		RemoveMapFile(mapFile);
	}

	// Each smaller copy is really that copy
	{
		StartAgain();
		WriteMapFile(mapFile, LeftAndRightMap(16));

		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);

		StandInCopies const *copies = CopiesOf(relief.PictureFor(picture));
		Check(copies != 0, "a map that's different on each side is used");

		if (copies)
		{
			// the next copy down is 8 x 8, with a pitch of 8 * 4 * 2, so its first row's middle is where left turns to right
			std::vector<uint8_t> const &half = copies->levels[1];
			Check(half[2] > 128 && half[(4 - 1) * 4 + 2] > 128 && half[4 * 4 + 2] < 128 && half[7 * 4 + 2] < 128, "the next copy down is the whole map at half the size, not its top corner");

			std::vector<uint8_t> const &quarter = copies->levels[2];
			Check(quarter[2] > 128 && quarter[2 * 4 + 2] < 128, "and so is the one below that");
		}

		RemoveMapFile(mapFile);
	}

	// Two levels, each with a picture of its own
	{
		StartAgain();
		TextureRelief relief;
		texID_t const first = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);
		texID_t const second = AddTexture(MapFolder() + "/other.rim", 8, 8, 32, TextureUsage_Normal);

		relief.Build(first);
		relief.LogWhatIsHeld();
		logLines.clear();

		relief.Build(second);
		relief.LogWhatIsHeld();
		Check(LogHas("this level has 0 hand-made, 0 worked out and 1 flat"), "the next level counts only what it asked for");
	}

	// What a level is said to have is what it would draw
	{
		StartAgain();
		TextureRelief relief;
		relief.SetAutoGenerate(true);
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);
		relief.SetAutoGenerate(false);
		relief.LogWhatIsHeld();
		Check(LogHas("this level has 0 hand-made, 0 worked out and 1 flat"), "one worked out counts as flat while the switch says not to draw it");
	}

	// The switch
	{
		StartAgain();
		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);
		memset(&textureList[picture].pixels[0], 90, textureList[picture].pixels.size());

		relief.Build(picture);
		texID_t const flat = relief.PictureFor(picture);

		relief.SetAutoGenerate(true);
		Check(relief.PictureFor(picture) == flat, "turned on, nothing changes until a level asks");

		relief.Build(picture);
		texID_t const workedOut = relief.PictureFor(picture);
		Check(workedOut != flat && relief.PicturesHeld() == 1, "and when one does, the texture gets one worked out");

		relief.LogWhatIsHeld();
		Check(LogHas("this level has 0 hand-made, 1 worked out and 0 flat"), "which the log counts as that");

		relief.SetAutoGenerate(false);
		Check(relief.PictureFor(picture) == flat, "turned off, it's flat again at once");

		relief.SetAutoGenerate(true);
		Check(relief.PictureFor(picture) == workedOut, "and turned on again, the one already made comes back");
	}

	// Every reason a map isn't used. Each says why, once, and the texture goes on as if the file weren't there.
	struct RefusedRow { char const *what; Bytes file; ReliefMapProblem problem; };

	Bytes cutShort = DeepMap(8, 200, 90, 180);
	cutShort.pop_back();

	Bytes notSquare = DeepMap(8, 200, 90, 180);
	notSquare[std::string((char const *)&notSquare[0], notSquare.size()).find("BMHD") + 11] = 16;

	RefusedRow const refused[] =
	{
		{ "an empty file", Bytes(), kMapNoHeading },
		{ "a file of something else", Bytes(100, 'x'), kMapPartsWrong },
		{ "a map that's cut short", cutShort, kMapCutShort },
		{ "a map that isn't square", notSquare, kMapNotSquare },
		{ "a map that isn't a power of two", DeepMap(7, 200, 90, 180), kMapSideNotAPowerOfTwo },
		{ "a map of too few planes to be deep and too many for a table", DeepMap(8, 200, 90, 180, 23), kMapPlanesWrong },
		{ "a map of planes with no table", DeepMap(8, 200, 90, 180, 8), kMapColourTableWrong },
		{ "a map as large as a side can say", DeepMap(4096, 200, 90, 180, 1), kMapSideTooLarge },
	};

	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		StartAgain();
		WriteMapFile(mapFile, refused[i].file);

		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);
		relief.Build(picture);

		std::string const reason = std::string("texture relief: ") + mapPath + " isn't used, because " + ReliefMap_ProblemWords(refused[i].problem);

		Check(LogCount(reason) == 1 && logLines.size() == 1, std::string(refused[i].what) + " says why, once");
		Check(madeCount == 1 && relief.PicturesHeld() == 0 && copiesList.empty(), std::string(refused[i].what) + " makes nothing, and leaves the flat picture");
		Check(relief.IsReady(), "  and everything else still works");
	}

	// With working maps out on, a refused map gives way to that
	{
		StartAgain();
		WriteMapFile(mapFile, Bytes(100, 'x'));

		TextureRelief relief;
		relief.SetAutoGenerate(true);
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);
		relief.LogWhatIsHeld();
		Check(LogHas("isn't used") && LogHas("this level has 0 hand-made, 1 worked out and 0 flat"), "a refused map gives way to one worked out, if that's asked for");
	}

	// The limit, with room for exactly one
	{
		StartAgain();
		WriteMapFile(mapFile, DeepMap(8, 200, 90, 180));
		WriteMapFile("second_Normal.rim", DeepMap(8, 200, 90, 180));

		TextureRelief relief(64 * 1024 * 1024, (size_t)ReliefMap_PlainBytes(8));
		texID_t const first = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);
		texID_t const second = AddTexture(MapFolder() + "/second.rim", 8, 8, 32, TextureUsage_Normal);

		relief.Build(first);
		int const madeBefore = madeCount;
		relief.Build(second);

		Check(relief.HandMadeBytesHeld() == ReliefMap_PlainBytes(8) && CopiesOf(relief.PictureFor(first)) != 0, "a map that exactly fills the limit is kept");
		Check(madeCount == madeBefore && !CopiesOf(relief.PictureFor(second)), "the next one goes without, and nothing is made for it");
		Check(LogHas("second_Normal.rim isn't used, because hand-made maps have reached their limit of " + std::to_string(ReliefMap_PlainBytes(8)) + " bytes"), "and the log says so");

		RemoveMapFile("second_Normal.rim");
	}

	// The card says no
	{
		StartAgain();
		WriteMapFile(mapFile, DeepMap(16, 200, 90, 180));
		largestSideCardMakes = 8;

		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);
		Check(LogHas("isn't used, because the graphics card can't make a texture 16 on a side") && relief.HandMadeBytesHeld() == 0 && relief.PicturesHeld() == 0, "a map larger than the card can make goes without, and says why");
	}

	// It can't be written
	for (int locksThatWork = 1; locksThatWork <= 2; locksThatWork++)
	{
		StartAgain();
		WriteMapFile(mapFile, DeepMap(8, 200, 90, 180));

		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(AddTexture(MapFolder() + "/none.rim", 8, 8, 32, TextureUsage_Normal));   // makes the flat picture first, which takes a lock of its own
		locksLeft = locksThatWork - 1;

		int const before = TexturesAlive();
		relief.Build(picture);

		Check(LogHas("isn't used, because the graphics card wouldn't let it be written") && relief.HandMadeBytesHeld() == 0 && TexturesAlive() == before, "a copy that can't be written gives the whole map back");
		Check(lockCount == unlockCount, "and leaves nothing locked");
	}

	// What can't have a map
	{
		StartAgain();
		WriteMapFile(mapFile, DeepMap(8, 200, 90, 180));

		TextureRelief relief;
		relief.Build(AddTexture(wall, 8, 8, 32, TextureUsage_Dynamic));
		Check(relief.PicturesHeld() == 0 && copiesList.empty(), "a film gets no map, even with a file there");

		TextureRelief off;
		off.TurnOff();
		off.Build(AddTexture(wall, 8, 8, 32, TextureUsage_Normal));
		Check(off.PicturesHeld() == 0 && copiesList.empty(), "with relief turned off at the start, the file isn't looked at");

		WriteMapFile("nameless_Normal.rim", DeepMap(8, 200, 90, 180));
		TextureRelief unnamed;
		unnamed.Build(AddTexture(MapFolder() + "/nameless", 8, 8, 32, TextureUsage_Normal));
		Check(unnamed.PicturesHeld() == 0 && copiesList.empty(), "a picture not named .rim has no map");
		RemoveMapFile("nameless_Normal.rim");

		RemoveMapFile(mapFile);
	}

	// A depth map is noticed, not read: what's in it doesn't matter, and it doesn't stop anything
	{
		StartAgain();
		WriteMapFile("wall_Depth.rim", Bytes(100, 'x'));
		WriteMapFile("wall_Depth.dds", Bytes(100, 'x'));

		TextureRelief relief;
		texID_t const picture = AddTexture(wall, 8, 8, 32, TextureUsage_Normal);

		relief.Build(picture);
		relief.Build(picture);

		Check(LogCount("wall_Depth.rim, which isn't used, because nothing draws from depth yet") == 1, "a depth map is noticed, once, whatever it holds");
		Check(LogCount("wall_Depth.dds, which isn't used, because nothing draws from depth yet") == 1, "with either ending");
		Check(madeCount == 1 && relief.PicturesHeld() == 0, "and it isn't read, and alone it leaves the flat picture");

		RemoveMapFile("wall_Depth.rim");
		RemoveMapFile("wall_Depth.dds");
	}

	// The parts that only the real game has, and so only reading can check
	std::string const pictures = ReadSourceFile("3dc/surface_pictures.cpp");
	std::string const card = ReadSourceFile("win32/d3_func.cpp");
	std::string const reader = ReadSourceFile("3dc/texture_relief.cpp");

	// the check further on says the same words, so a file past the limit couldn't show this: only that it isn't read first
	Check(reader.find("if (fileBytes > kReliefMapMostFileBytes)") != std::string::npos && reader.find("if (fileBytes > kReliefMapMostFileBytes)") < reader.find("file.ReadBytes(bytes.get()"), "a file's length is asked before any of it is read");

	Check(pictures.find("device->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);") != std::string::npos, "the surface pass blends between a map's smaller copies");
	Check(card.find("side > caps.MaxTextureWidth || side > caps.MaxTextureHeight") != std::string::npos, "the card is asked before a map is made");
	Check(card.find("made->GetLevelCount() != levels") != std::string::npos && card.find("description.Width != side || description.Height != side") != std::string::npos, "and what it made is looked at afterwards");

	_rmdir(MapFolder().c_str());
}

int main()
{
	// so that if a row stops the program, what it says before that still gets out
	setvbuf(stdout, 0, _IONBF, 0);

	CheckTheCalculation();
	CheckHostileInput();
	CheckTheGameSide();
	CheckHandMadeMaps();

	printf("%d failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
