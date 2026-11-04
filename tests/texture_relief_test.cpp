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

#include <stdio.h>
#include <string.h>
#include <vector>
#include <windows.h>

#include "texture_relief.h"

static int checksFailed = 0;

static void Check(bool good, const char *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);

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

void LogString(const std::string &) {}

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
		texID_t const pictures[3] = { Added::Picture(pixels), Added::Picture(pixels), Added::Picture(pixels) };

		relief.Build(pictures[0]);
		relief.Build(pictures[1]);
		int const madeBefore = madeCount;
		relief.Build(pictures[2]);

		Check(relief.PicturesHeld() == 2 && relief.BytesHeld() == size * size * 4 * 2, "pictures that exactly fill the limit are all kept");
		Check(madeCount == madeBefore, "the next one goes without, and nothing is made for it");

		relief.ReleaseAll();
		relief.Build(pictures[2]);
		Check(relief.PicturesHeld() == 1, "the next level starts with the whole limit again");
	}

	// A number too high is refused before it's looked up, so the list doesn't grow. There's no texture there, and it would stop the test if it were read.
	{
		StartAgain();
		TextureRelief relief;

		relief.Build(65536);
		relief.Build(0);
		relief.Build(1);

		Check(madeCount == 0 && lockCount == 0, "a number too high and the two special textures get none");
	}
}

int main()
{
	CheckTheCalculation();
	CheckHostileInput();
	CheckTheGameSide();

	printf("%d failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
