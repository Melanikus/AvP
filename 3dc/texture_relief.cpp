// Gives a level's flat pictures a feeling of depth. Written 2026-11-04 by Melanikus for the AvP port.
//
// A wall in this game is a flat polygon with a picture on it. The rivets and cracks are only painted on, so light falls on them as if they were
// flat. This looks at each picture while a level loads, guesses the shape the painting shows, and keeps that guess as a second picture beside it,
// the relief picture. The surface pass reads it to tilt each pixel's normal. It's worked out once for each picture, and nothing here runs in play.
//
// HOW THE SHAPE IS GUESSED: in three steps, for each picture.
//   1. Grey. Colour says little about shape, so each pixel becomes one brightness number.
//   2. Depth. In a painted surface, the dark is usually deep and the light is usually raised. Where brightness changes sharply there's usually a crack
//      or a seam, so those places are pressed in a little further. The 5x5 Sobel mask below is what finds them.
//   3. Normals. The same mask, run over the depth, says which way the surface slopes at each pixel, and that gives the arrow for that pixel.
// It's a guess, and a painted shadow will come out as a dent. That's fine: this is a stand-in for pictures made by hand, which it only does if it's asked to. texture_relief.h says how
// those arrive.
//
// THE EDGES OF A PICTURE: the mask reads two pixels each way, so at an edge it carries on from the opposite side. Pictures repeat: in the stock levels
// 81,080 of 913,052 polygons run past the edge of their picture, so stopping there would draw a seam across them.
//
// HOLES: a pixel with an alpha of 0 is a hole, and its colour means nothing. If that colour went into the mask, every hole would get a false rim. So
// holes are filled in from the solid pixels beside them first.
//
// ANIMATED TEXTURES need nothing special. Each frame is a picture with a texture number of its own, so it has a relief picture of its own too.

#include <math.h>
#include <memory>
#include <new>
#include <stdlib.h>
#include <string>

#include "texture_relief.h"
#include "relief_map_file.h"
#include "FileStream.h"
#include "d3_func.h"
#include "logString.h"

TextureRelief textureRelief;

static uint32_t const kBytesPerPixel = 4;

// The largest side that's worked on. The stock levels' pictures are all 128. This keeps a file from asking for more memory and time than a level can
// spare: a bigger picture just goes without.
static uint32_t const kLargestSide = 1024;

// No game has this many textures. It keeps the list by texture number from growing without end.
static texID_t const kMostTextures = 65536;

// The mask, as two steps across and two down each way from the pixel in the middle. It's negative on the left and positive on the right, so on flat
// ground the two sides cancel out and the answer is zero. On a slope they don't, and the answer says how steep it is. Rows nearer the middle count for
// more. The same mask turned on its side, by swapping row and column, measures the slope downwards.
static int const kMaskSize = 5;
static int const kMaskReach = 2;
static int const kMask[kMaskSize][kMaskSize] =
{
	{ -1,  -2, 0,  2, 1 },
	{ -4,  -8, 0,  8, 4 },
	{ -6, -12, 0, 12, 6 },
	{ -4,  -8, 0,  8, 4 },
	{ -1,  -2, 0,  2, 1 },
};

// A slope of 1 for each pixel comes out of the mask as this, so dividing by it gives the slope itself.
static int const kMaskWeight = 128;

// How much further in a sharp change of brightness is pressed: this many for each 1 of brightness that changes from one pixel to the next.
static int const kCrackDepth = 1;

// How strong the effect is. This much rise in depth over one pixel makes a 45 degree slope, so a smaller number gives a stronger relief.
static float const kRiseForFortyFiveDegrees = 32.0f;

// Carries on from the opposite side, when a place is off the edge of a picture. It's never more than kMaskReach off, however small the picture.
static int Wrapped(int place, int size)
{
	while (place < 0)
	{
		place += size;
	}

	while (place >= size)
	{
		place -= size;
	}

	return place;
}

// Lays the mask over one pixel of a picture of single numbers. across and down come back as how much the numbers rise to the right and to the bottom.
static void MeasureSlope(uint8_t const *numbers, int width, int height, int x, int y, int &across, int &down)
{
	int columns[kMaskSize];
	int rows[kMaskSize];

	for (int i = 0; i < kMaskSize; i++)
	{
		columns[i] = Wrapped(x + i - kMaskReach, width);
		rows[i] = Wrapped(y + i - kMaskReach, height);
	}

	int sumAcross = 0;
	int sumDown = 0;

	for (int row = 0; row < kMaskSize; row++)
	{
		uint8_t const *line = numbers + rows[row] * width;

		for (int column = 0; column < kMaskSize; column++)
		{
			int const number = line[columns[column]];

			sumAcross += kMask[row][column] * number;
			sumDown += kMask[column][row] * number;
		}
	}

	across = sumAcross;
	down = sumDown;
}

// Step 1. The weights are how bright the eye finds blue, green and red, out of 256. Gives back whether the picture has any holes.
static bool MakeGrey(uint8_t const *picture, uint32_t pitch, int width, int height, uint8_t *grey, uint8_t *solid)
{
	bool holes = false;

	for (int y = 0; y < height; y++)
	{
		uint8_t const *pixel = picture + y * pitch;

		for (int x = 0; x < width; x++, pixel += kBytesPerPixel)
		{
			int const place = y * width + x;

			grey[place] = (uint8_t)((29 * pixel[0] + 150 * pixel[1] + 77 * pixel[2]) >> 8);
			solid[place] = (pixel[3] != 0) ? 1 : 0;

			if (!solid[place])
			{
				holes = true;
			}
		}
	}

	return holes;
}

// Takes a hole's number from a solid neighbour, if it has one. A hole filled this way counts as solid for the next, so the numbers spread inwards.
static void FillFromNeighbour(uint8_t *grey, uint8_t *solid, int width, int height, int x, int y)
{
	int const place = y * width + x;

	if (solid[place])
	{
		return;
	}

	int const neighbours[4] =
	{
		y * width + Wrapped(x - 1, width),
		y * width + Wrapped(x + 1, width),
		Wrapped(y - 1, height) * width + x,
		Wrapped(y + 1, height) * width + x,
	};

	for (int i = 0; i < 4; i++)
	{
		if (solid[neighbours[i]])
		{
			grey[place] = grey[neighbours[i]];
			solid[place] = 1;
			return;
		}
	}
}

// Fills the holes. Forwards through the picture reaches everything after the first solid pixel, because each pixel looks left and up. Backwards reaches
// the rest. So it takes two passes whatever the file holds, and a picture that's all hole is never drawn, so what's in it doesn't matter.
static void FillHoles(uint8_t *grey, uint8_t *solid, int width, int height)
{
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			FillFromNeighbour(grey, solid, width, height, x, y);
		}
	}

	for (int y = height - 1; y >= 0; y--)
	{
		for (int x = width - 1; x >= 0; x--)
		{
			FillFromNeighbour(grey, solid, width, height, x, y);
		}
	}
}

// Step 2. Bright is high, and a sharp change of brightness is pressed in.
static void MakeDepth(uint8_t const *grey, int width, int height, uint8_t *depth)
{
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int across, down;
			MeasureSlope(grey, width, height, x, y, across, down);

			int const place = y * width + x;
			int const crack = (abs(across) + abs(down)) * kCrackDepth / kMaskWeight;
			int const heightHere = (int)grey[place] - crack;

			depth[place] = (uint8_t)((heightHere < 0) ? 0 : heightHere);
		}
	}
}

// A number from -1 to 1, as it's stored: 128 is exactly zero, so a flat pixel comes back as exactly flat.
static uint8_t StoredNumber(float number)
{
	return (uint8_t)(128.5f + 127.0f * number);
}

// Step 3. A surface rising to the right leans to the left, which is why both slopes change sign.
static void MakeNormals(uint8_t const *depth, int width, int height, uint8_t *relief, uint32_t pitch)
{
	float const slopeScale = 1.0f / ((float)kMaskWeight * kRiseForFortyFiveDegrees);

	for (int y = 0; y < height; y++)
	{
		uint8_t *pixel = relief + y * pitch;

		for (int x = 0; x < width; x++, pixel += kBytesPerPixel)
		{
			int across, down;
			MeasureSlope(depth, width, height, x, y, across, down);

			float const leanRight = -(float)across * slopeScale;
			float const leanDown = -(float)down * slopeScale;
			float const length = sqrtf(leanRight * leanRight + leanDown * leanDown + 1.0f);

			pixel[0] = StoredNumber(1.0f / length);
			pixel[1] = StoredNumber(leanDown / length);
			pixel[2] = StoredNumber(leanRight / length);
			pixel[3] = depth[y * width + x];
		}
	}
}

bool TextureRelief::SizeIsFine(uint32_t width, uint32_t height)
{
	return width >= 1 && height >= 1 && width <= kLargestSide && height <= kLargestSide;
}

bool TextureRelief::Calculate(uint8_t const *picture, uint32_t picturePitch, uint32_t width, uint32_t height, uint8_t *relief, uint32_t reliefPitch)
{
	// SizeIsFine comes first, because it's what makes the sums after it safe
	if (!picture || !relief || !SizeIsFine(width, height))
	{
		return false;
	}

	if (picturePitch < width * kBytesPerPixel || reliefPitch < width * kBytesPerPixel)
	{
		return false;
	}

	size_t const pixels = (size_t)width * height;

	// three pictures of one number a pixel, for the time this takes
	uint8_t *const work = new (std::nothrow) uint8_t[pixels * 3];

	if (!work)
	{
		return false;
	}

	uint8_t *const grey = work;
	uint8_t *const solid = work + pixels;
	uint8_t *const depth = work + pixels * 2;

	if (MakeGrey(picture, picturePitch, (int)width, (int)height, grey, solid))
	{
		FillHoles(grey, solid, (int)width, (int)height);
	}

	MakeDepth(grey, (int)width, (int)height, depth);
	MakeNormals(depth, (int)width, (int)height, relief, reliefPitch);

	delete[] work;
	return true;
}

TextureRelief::TextureRelief(size_t mostBytes, size_t mostHandMadeBytes, size_t mostTries)
: mostBytesHeld(mostBytes), mostHandMadeBytesHeld(mostHandMadeBytes), limitWasLogged(false), on(true), autoGenerate(false), flatPictureMade(false), flatPicture(0),
  picturesHeld(0), bytesHeld(0), handMadeBytesHeld(0), mostHandMadeTries(mostTries), handMadeTries(0), handMadeBytesTried(0), stoppedTryingWasLogged(false), levelNumber(1)
{
}

void TextureRelief::SetAutoGenerate(bool wanted)
{
	autoGenerate = wanted;
}

bool TextureRelief::AutoGenerates() const
{
	return autoGenerate;
}

size_t TextureRelief::HandMadeTriesThisLevel() const
{
	return handMadeTries;
}

uint64_t TextureRelief::HandMadeBytesTriedThisLevel() const
{
	return handMadeBytesTried;
}

size_t TextureRelief::HandMadeBytesHeld() const
{
	return handMadeBytesHeld;
}

void TextureRelief::TurnOff()
{
	on = false;
}

bool TextureRelief::IsOn() const
{
	return on;
}

bool TextureRelief::IsReady() const
{
	return on && flatPictureMade;
}

size_t TextureRelief::PicturesHeld() const
{
	return picturesHeld;
}

size_t TextureRelief::BytesHeld() const
{
	return bytesHeld;
}

// The picture for every texture without a relief picture of its own: one pixel, flat, and at the middle height.
bool TextureRelief::MakeFlatPicture()
{
	if (flatPictureMade)
	{
		return true;
	}

	texID_t const flat = Tex_Create("relief_flat", 1, 1, 32, TextureUsage_Normal);

	if (flat == MISSING_TEXTURE)
	{
		return false;
	}

	uint8_t *pixel = 0;
	uint32_t pitch = 0;

	if (!Tex_Lock(flat, &pixel, &pitch))
	{
		Tex_Release(flat);
		return false;
	}

	// Flat: a normal straight out of the picture. Its alpha is what says "no depth here" when it stands in for a depth map: see reliefMarch.fxh.
	pixel[0] = 255;
	pixel[1] = 128;
	pixel[2] = 128;
	pixel[3] = 0;

	Tex_Unlock(flat);

	flatPicture = flat;
	flatPictureMade = true;
	return true;
}

// Works out a relief picture from the texture's own pixels. Gives back MISSING_TEXTURE if there can't be one.
texID_t TextureRelief::CalculatedPictureFor(texID_t picture)
{
	// Copied out, because making a texture can move the list these numbers are in.
	Texture const &details = Tex_GetTextureDetails(picture);
	uint32_t const width = details.realWidth;
	uint32_t const height = details.realHeight;
	std::string const name = details.name + "_relief";

	// A film is changed while it plays, so a relief picture would be wrong at once. And only this one format is understood.
	if (details.usage != TextureUsage_Normal || details.bitsPerPixel != kBytesPerPixel * 8 || !SizeIsFine(width, height))
	{
		return MISSING_TEXTURE;
	}

	// the last check before anything is made, and bytesHeld can't be more than mostBytesHeld, so this can't go below zero
	size_t const bytes = (size_t)width * height * kBytesPerPixel;

	if (bytes > mostBytesHeld - bytesHeld)
	{
		if (!limitWasLogged)
		{
			LogString("texture relief: the limit of " + std::to_string(mostBytesHeld) + " bytes is reached, so textures after this one have no relief");
			limitWasLogged = true;
		}

		return MISSING_TEXTURE;
	}

	texID_t const relief = Tex_Create(name, width, height, kBytesPerPixel * 8, TextureUsage_Normal);

	if (relief == MISSING_TEXTURE)
	{
		return MISSING_TEXTURE;
	}

	// the card can change the size it was asked for, and the two pictures have to match
	Texture const &made = Tex_GetTextureDetails(relief);
	bool worked = false;

	if (made.realWidth == width && made.realHeight == height)
	{
		uint8_t *from = 0;
		uint32_t fromPitch = 0;

		if (Tex_Lock(picture, &from, &fromPitch))
		{
			uint8_t *to = 0;
			uint32_t toPitch = 0;

			if (Tex_Lock(relief, &to, &toPitch))
			{
				worked = Calculate(from, fromPitch, width, height, to, toPitch);
				Tex_Unlock(relief);
			}

			Tex_Unlock(picture);
		}
	}

	if (!worked)
	{
		Tex_Release(relief);
		return MISSING_TEXTURE;
	}

	bytesHeld += bytes;
	return relief;
}

// THE ONE PLACE a texture gets its relief picture. Later: "use somerimname_Normal.rim if it exists, otherwise calculate" goes here. See texture_relief.h.
// Says once for each if a depth map is there, because someone made it for a reason, and it would be easy to think it was doing something
void TextureRelief::NoteDepthFile(std::string const &pictureName)
{
	std::string depthName;

	if (ReliefMap_FileName(pictureName, "_Depth.dds", &depthName))
	{
		FileStream file;
		file.Open(depthName, FileStream::FileRead);

		if (file.IsGood())
		{
			LogString("texture relief: found " + depthName + ", which isn't read yet, because only a .rim depth map is");
		}
	}
}

// Whether another map file may be opened. See texture_relief.h for why every try counts. The last try before the bytes run out can go a little over, by one map at most, and that's fine:
// what this limits is how long a level can spend, and one more map is a fraction of a second.
bool TextureRelief::MayTryAnotherMap()
{
	std::string why;

	if (handMadeBytesHeld >= mostHandMadeBytesHeld)
	{
		why = "they've reached their limit of " + std::to_string(mostHandMadeBytesHeld) + " bytes";
	}
	else if (handMadeTries >= mostHandMadeTries)
	{
		why = "this level has tried " + std::to_string(mostHandMadeTries) + " of their files";
	}
	else if (handMadeBytesTried >= (uint64_t)mostHandMadeBytesHeld * 3)
	{
		why = "this level has spent " + std::to_string(handMadeBytesTried) + " bytes reading and unpacking them";
	}
	else
	{
		return true;
	}

	if (!stoppedTryingWasLogged)
	{
		stoppedTryingWasLogged = true;
		LogString("texture relief: no more hand-made maps are read for now, because " + why);
	}

	return false;
}

// The texture's hand-made map of either kind, or MISSING_TEXTURE if it hasn't one that can be used. The file is checked all the way through before the card is asked for anything. Both kinds come
// through here so that each has the same limits and pays from the same allowance, and only what's done with the pixels differs.
texID_t TextureRelief::HandMadePictureFor(texID_t picture, MapKind kind)
{
	// Copied out, because making a texture can move the list these numbers are in.
	Texture const &details = Tex_GetTextureDetails(picture);
	std::string const pictureName = details.name;
	uint32_t const pictureWidth = details.realWidth;
	uint32_t const pictureHeight = details.realHeight;

	// A film is changed while it plays, so a map of it would mean nothing.
	if (details.usage != TextureUsage_Normal)
	{
		return MISSING_TEXTURE;
	}

	if (kind == kDepthMap)
	{
		NoteDepthFile(pictureName);
	}

	std::string mapName;

	if (!ReliefMap_FileName(pictureName, (kind == kDepthMap) ? "_Depth.rim" : "_Normal.rim", &mapName))
	{
		return MISSING_TEXTURE;
	}

	FileStream file;
	file.Open(mapName, FileStream::FileRead);

	if (!file.IsGood())
	{
		return MISSING_TEXTURE;
	}

	handMadeTries++;

	std::string const notUsed = "texture relief: " + mapName + " isn't used, because ";
	uint64_t const fileBytes = file.GetFileSize();

	if (fileBytes > kReliefMapMostFileBytes)
	{
		LogString(notUsed + ReliefMap_ProblemWords(kMapFileTooLong));
		return MISSING_TEXTURE;
	}

	handMadeBytesTried += fileBytes;

	std::unique_ptr<uint8_t[]> const bytes(new(std::nothrow) uint8_t[(size_t)fileBytes + 1]);

	if (!bytes)
	{
		LogString(notUsed + "there isn't the memory to read it");
		return MISSING_TEXTURE;
	}

	if (fileBytes > 0 && file.ReadBytes(bytes.get(), (size_t)fileBytes) != fileBytes)
	{
		LogString(notUsed + "it couldn't be read");
		return MISSING_TEXTURE;
	}

	ReliefMapRim rim;
	ReliefMapProblem problem = ReliefMap_FindRimParts(bytes.get(), fileBytes, &rim);

	if (problem != kMapFine)
	{
		LogString(notUsed + ReliefMap_ProblemWords(problem));
		return MISSING_TEXTURE;
	}

	uint64_t const allBytes = ReliefMap_PlainBytes(rim.side);

	// bytes held can't be more than the most allowed, so this can't go below zero
	if (allBytes > mostHandMadeBytesHeld - handMadeBytesHeld)
	{
		LogString(notUsed + "hand-made maps have reached their limit of " + std::to_string(mostHandMadeBytesHeld) + " bytes");
		return MISSING_TEXTURE;
	}

	std::unique_ptr<uint8_t[]> const pixels(new(std::nothrow) uint8_t[(size_t)allBytes]);

	if (!pixels)
	{
		LogString(notUsed + "there isn't the memory to read it");
		return MISSING_TEXTURE;
	}

	// Charged now, because a map can be wrong at its very last pixel: all the work has been done by the time that shows.
	handMadeBytesTried += allBytes;

	problem = ReliefMap_DecodeRim(rim, pixels.get());

	if (problem != kMapFine)
	{
		LogString(notUsed + ReliefMap_ProblemWords(problem));
		return MISSING_TEXTURE;
	}

	if (kind == kDepthMap)
	{
		size_t const pixelCount = (size_t)rim.side * rim.side;

		for (size_t i = 0; i < pixelCount; i++)
		{
			ReliefMap_MakeHeightPixel(pixels.get() + i * 4);
		}
	}

	// Each smaller copy goes straight after the one it's made from
	uint32_t const levels = ReliefMap_Levels(rim.side);
	uint8_t *copy = pixels.get();

	for (uint32_t level = 0; level + 1 < levels; level++)
	{
		uint32_t const levelSide = ReliefMap_LevelSide(rim.side, level);
		uint8_t *const next = copy + (size_t)levelSide * levelSide * 4;

		if (kind == kDepthMap)
		{
			ReliefMap_MakeSmallerHeightCopy(copy, levelSide, next);
		}
		else
		{
			ReliefMap_MakeSmallerCopy(copy, levelSide, next);
		}

		copy = next;
	}

	texID_t const map = Tex_CreateWithCopies(mapName, rim.side, levels);

	if (map == MISSING_TEXTURE)
	{
		LogString(notUsed + "the graphics card can't make a texture " + std::to_string(rim.side) + " on a side with smaller copies");
		return MISSING_TEXTURE;
	}

	copy = pixels.get();

	for (uint32_t level = 0; level < levels; level++)
	{
		uint32_t const levelSide = ReliefMap_LevelSide(rim.side, level);
		uint8_t *to = 0;
		uint32_t pitch = 0;

		if (!Tex_LockCopy(map, level, &to, &pitch))
		{
			Tex_Release(map);
			LogString(notUsed + "the graphics card wouldn't let it be written");
			return MISSING_TEXTURE;
		}

		for (uint32_t y = 0; y < levelSide; y++)
		{
			memcpy(to + (size_t)y * pitch, copy + (size_t)y * levelSide * 4, (size_t)levelSide * 4);
		}

		Tex_UnlockCopy(map, level);
		copy += (size_t)levelSide * levelSide * 4;
	}

	handMadeBytesHeld += (size_t)allBytes;

	LogString("texture relief: using " + mapName + ", " + std::to_string(rim.side) + " x " + std::to_string(rim.side) + ", with " + std::to_string(levels) + " copies and " + std::to_string(allBytes)
		+ " bytes, for a picture of " + std::to_string(pictureWidth) + " x " + std::to_string(pictureHeight));

	return map;
}

// The one place a texture gets its relief picture. See texture_relief.h for the order.
void TextureRelief::ReliefPictureFor(texID_t picture, Entry &entry)
{
	texID_t relief = MISSING_TEXTURE;
	bool workedOut = false;

	if (!entry.lookedForFile && MayTryAnotherMap())
	{
		entry.lookedForFile = true;
		relief = HandMadePictureFor(picture, kNormalMap);
	}

	if (relief == MISSING_TEXTURE && autoGenerate)
	{
		relief = CalculatedPictureFor(picture);
		workedOut = true;
	}

	if (relief != MISSING_TEXTURE)
	{
		entry.relief = relief;
		entry.workedOut = workedOut;
		picturesHeld++;
	}
}

// The one place a texture gets its depth map. Whether it has a relief picture or not makes no difference to this.
void TextureRelief::DepthPictureFor(texID_t picture, Entry &entry)
{
	if (entry.lookedForDepth || !MayTryAnotherMap())
	{
		return;
	}

	entry.lookedForDepth = true;

	texID_t const height = HandMadePictureFor(picture, kDepthMap);

	if (height != MISSING_TEXTURE)
	{
		entry.height = height;
		picturesHeld++;
	}
}

void TextureRelief::Build(texID_t picture)
{
	if (!on || picture == NO_TEXTURE || picture == MISSING_TEXTURE || picture >= kMostTextures)
	{
		return;
	}

	if (!MakeFlatPicture())
	{
		return;
	}

	if (picture >= reliefOf.size())
	{
		Entry const none = { NO_TEXTURE, false, false, 0, NO_TEXTURE, false };
		reliefOf.resize(picture + 1, none);
	}

	// a picture used by two shapes loads once, and gets here twice
	if (reliefOf[picture].relief == NO_TEXTURE)
	{
		ReliefPictureFor(picture, reliefOf[picture]);
	}

	// Reached even when the texture already has its relief picture, because the depth map may not have had its turn yet
	DepthPictureFor(picture, reliefOf[picture]);

	reliefOf[picture].levelCounted = levelNumber;
}

texID_t TextureRelief::HeightFor(texID_t picture) const
{
	if (picture < reliefOf.size() && reliefOf[picture].height != NO_TEXTURE)
	{
		return reliefOf[picture].height;
	}

	return flatPicture;
}

texID_t TextureRelief::PictureFor(texID_t picture) const
{
	if (picture < reliefOf.size() && reliefOf[picture].relief != NO_TEXTURE && (autoGenerate || !reliefOf[picture].workedOut))
	{
		return reliefOf[picture].relief;
	}

	return flatPicture;
}

void TextureRelief::LogWhatIsHeld()
{
	if (on)
	{
		LogString("texture relief: " + std::to_string(picturesHeld) + " pictures held, " + std::to_string(bytesHeld) + " bytes");

		// What this level asked for, as each would be drawn right now
		size_t handMade = 0, workedOut = 0, flat = 0, depth = 0;

		for (size_t i = 0; i < reliefOf.size(); i++)
		{
			Entry const &entry = reliefOf[i];

			if (entry.levelCounted == levelNumber)
			{
				if (entry.height != NO_TEXTURE)
				{
					depth++;
				}

				if (entry.relief == NO_TEXTURE || (entry.workedOut && !autoGenerate))
				{
					flat++;
				}
				else if (entry.workedOut)
				{
					workedOut++;
				}
				else
				{
					handMade++;
				}
			}
		}

		LogString("texture relief: this level has " + std::to_string(handMade) + " hand-made, " + std::to_string(workedOut) + " worked out and " + std::to_string(flat)
			+ " flat. Hand-made maps hold " + std::to_string(handMadeBytesHeld) + " bytes. It has " + std::to_string(depth) + " depth maps");
		levelNumber++;

		// The next level has its own allowance
		handMadeTries = 0;
		handMadeBytesTried = 0;
		stoppedTryingWasLogged = false;
	}
	else
	{
		LogString("texture relief: off");
	}
}

void TextureRelief::ReleaseAll()
{
	for (size_t i = 0; i < reliefOf.size(); i++)
	{
		Tex_Release(reliefOf[i].relief);
		Tex_Release(reliefOf[i].height);
	}

	if (flatPictureMade)
	{
		Tex_Release(flatPicture);
	}

	reliefOf.clear();
	flatPictureMade = false;
	limitWasLogged = false;
	picturesHeld = 0;
	bytesHeld = 0;
	handMadeBytesHeld = 0;
	handMadeTries = 0;
	handMadeBytesTried = 0;
	stoppedTryingWasLogged = false;
}
