// Modified 2026-11-06 by Melanikus for the AvP port:
//   a stand-in for what the memory code now tells the game's sounds
// What happens to the pictures a level file names, going through the game's real level loader. No game files are needed: each test builds
// the bytes of a small level file itself.
//
// Two things are checked here, and both come from level files being untrustworthy:
//
//  1. One file can only ask for so many pictures. Each one asked for costs time, even if there's no such picture, so a small file naming a
//     million of them could keep the game busy for as long as it liked.
//
//  2. A picture that can't be used must never end up as a frame of an animation. A picture that's refused leaves -1 in the level's list of
//     pictures. A polygon's animation copies a number from that list for each frame, and later the drawing code would use that -1 as a
//     texture number, reading far outside the list of textures. Every animated polygon in the game's own levels shows just one picture, so
//     if that picture is refused the polygon loses its texture and never animates. Only a crafted file can reach this: a polygon whose own
//     picture is real, with another frame showing one that isn't.

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "chunk_load.hpp"
#include "chunk_textures.h"
#include "animation_chunk.hpp"
#include "bitmap_names.hpp"
#include "environment_chunk.hpp"
#include "mempool.h"
#include "mishchnk.hpp"
#include "module.h"

// These are in chunk_load.cpp, which doesn't list them in a header
void SetupAnimatedTextures(Shape_Chunk *sc, SHAPEHEADER *shp, Animation_Chunk *ac, Shape_Merge_Data_Chunk *smdc, int local_max_index, int *local_tex_index_nos);
void setup_tex_conv_array(int &max_indices, int *&conv_array, RIFFHANDLE h, Chunk_With_Children *tmpshp);

// ---- stand-ins for the parts of the game the loader calls ----

static int picturesAskedFor = 0;
static int linesInTheLog = 0;
static std::string pictureToRefuse;

texID_t MISSING_TEXTURE = 0;

// Real pictures get a number from 1000 up, and the one chosen is refused, the way the game answers when it can't use a picture
texID_t load_rif_bitmap(char const *name, BMPN_Flags)
{
	picturesAskedFor++;
	return pictureToRefuse == name ? GEI_NOTLOADED : (texID_t)(1000 + picturesAskedFor);
}

void LogErrorStringC(const char *)
{
	linesInTheLog++;
}

HANDLE avp_CreateFile(const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
{
	return INVALID_HANDLE_VALUE;
}

DWORD avp_GetFileAttributes(const char *)
{
	return INVALID_FILE_ATTRIBUTES;
}

void avp_MessageBox(const char *message, int)
{
	puts(message);
}

void avp_exit(int code)
{
	exit(code);
}

// The memory code tells the game's sounds about memory that's about to go. There are no sounds in this test.
void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// The loader can do far more than these tests ask of it: it copies whole shapes and levels, and plays sounds. All of that needs these to exist,
// and none of it runs here.
char const *cl_pszGameMode = "marine";
MAPHEADER Map[1];
MODULEMAPBLOCK Empty_Module_Map;
SCENEMODULE MainScene;
SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
SHAPEHEADER **mainshapelist = 0;
unsigned char *TextureLightingTable = 0;

int GetMSLPos(void)
{
	return 0;
}

void FreeMSLPos(int)
{
}

void ChangeWindowsSize(uint32_t, uint32_t)
{
}

void DeallocateLoadedShapeheader(SHAPEHEADER *)
{
}

const loaded_sound *GetSoundForMainRif(const char *)
{
	return 0;
}

void LoseSound(const loaded_sound *)
{
}

void post_process_shape(SHAPEHEADER *)
{
}

void pre_process_shape(RIFFHANDLE, ChunkShape &, Chunk_With_Children *, int)
{
}

// The loader gets its memory for an animation from here. A level's memory all goes at once when the level ends, so nothing asks for it back.
void *PoolAllocateMem(size_t amount)
{
	static char pool[256 * 1024];
	static size_t used = 0;

	size_t const rounded = (amount + 7) / 8 * 8;

	if (rounded > sizeof(pool) - used)
	{
		puts("the test's memory ran out");
		exit(1);
	}

	void *const memory = pool + used;
	used += rounded;
	return memory;
}

// ---- reporting ----

static int passed = 0;
static int failed = 0;

static void Check(bool ok, char const *what)
{
	printf("  %s %s\n", ok ? "ok:" : "FAILED:", what);
	(ok ? passed : failed)++;
}

// ---- building the bytes of a level file ----

typedef std::vector<char> Bytes;

static void AddNumber(Bytes &to, int number)
{
	char const *const bytes = (char const *)&number;
	to.insert(to.end(), bytes, bytes + sizeof(number));
}

// A part of a level file: its kind in 8 letters, then its size counting these 12 bytes, then what it holds
static void AddPart(Bytes &to, char const *kind, Bytes const &holds)
{
	to.insert(to.end(), kind, kind + 8);
	AddNumber(to, (int)(12 + holds.size()));
	to.insert(to.end(), holds.begin(), holds.end());
}

// A list of names. The name at each place is pictureN.RIM, where N is its place, and that's also its number in the level's list of pictures.
static void AddNames(Bytes &to, char const *kind, int howMany)
{
	Bytes holds;
	AddNumber(holds, howMany);

	for (int i = 0; i < howMany; i++)
	{
		AddNumber(holds, 0);       // no flags
		AddNumber(holds, i);       // its number in the list of pictures
		AddNumber(holds, 0);
		AddNumber(holds, 0);
		AddNumber(holds, 0);

		std::string const name = "picture" + std::to_string(i) + ".RIM";
		holds.insert(holds.end(), name.begin(), name.end());
		holds.insert(holds.end(), 4 - name.size() % 4, 0);   // an end marker, then enough to reach a multiple of 4 bytes
	}

	AddPart(to, kind, holds);
}

// ---- one level file, as far as the loader sees it ----

struct LevelFile
{
	File_Chunk file;
	_RifHandle handle;

	// What this level is made of. They come after the file, so they go before it, each lifting itself out of the file as it does.
	std::vector<std::unique_ptr<Chunk> > chunks;

	LevelFile()
	{
		Parent_File = &file;
		handle.fc = &file;
		picturesAskedFor = 0;
		linesInTheLog = 0;
		pictureToRefuse.clear();
	}

	~LevelFile()
	{
		delete[] handle.tex_index_nos;
		Env_Chunk = 0;
		Parent_File = 0;
	}

	// The level's own list of pictures, which is what load_rif_bitmaps goes through
	void LoadLevelNames(int howMany)
	{
		Bytes bytes;
		AddNames(bytes, "BMPNAMES", howMany);

		std::unique_ptr<Environment_Data_Chunk> environment(new Environment_Data_Chunk(&file, bytes.data(), bytes.size()));
		handle.envd = environment.get();
		chunks.push_back(std::move(environment));
		load_rif_bitmaps(&handle, 0);
	}

	// An outside shape's own list of pictures, which is the other way a picture gets asked for. This hands back a copy of that shape's list of picture numbers.
	std::vector<int> LoadShapeNames(int howMany)
	{
		Bytes bytes;
		AddNames(bytes, "BMPLSTST", howMany);

		Chunk_With_Children *const shape = &file;
		chunks.push_back(std::unique_ptr<Chunk>(new Shape_External_File_Chunk(shape, bytes.data(), bytes.size())));

		int most = 0;
		int *numbers = 0;
		setup_tex_conv_array(most, numbers, &handle, shape);

		std::vector<int> copy;

		if (numbers)
		{
			copy.assign(numbers, numbers + most + 1);
		}

		// With no list of its own, a shape uses the level's, which isn't this one's to give back
		if (numbers != handle.tex_index_nos)
		{
			delete[] numbers;
		}

		return copy;
	}
};

// ---- 1. the limit ----

static void TestTheLimit()
{
	puts("1. how many pictures one file can ask for");

	{
		LevelFile level;
		level.LoadLevelNames(4096);
		Check(picturesAskedFor == 4096, "a file naming 4,096 pictures gets them all asked for");
		Check(linesInTheLog == 0, "and nothing is said about it");
		Check(level.handle.tex_index_nos[4095] != -1, "and the last of them has its place in the list");
	}

	{
		LevelFile level;
		level.LoadLevelNames(4097);
		Check(picturesAskedFor == 4096, "one more isn't asked for");
		Check(level.handle.tex_index_nos[4095] != -1, "the last one allowed still has its place");
		Check(level.handle.tex_index_nos[4096] == -1, "and the one past the limit is left as a picture that isn't there");
		Check(linesInTheLog == 1, "and the log says so");
	}

	{
		LevelFile level;
		level.LoadLevelNames(65535);
		Check(picturesAskedFor == 4096, "the most one list can hold still stops at the limit");
		Check(linesInTheLog == 1, "and the log says so once, not once for every name");
	}

	{
		LevelFile level;
		std::vector<int> const numbers = level.LoadShapeNames(4097);
		Check(picturesAskedFor == 4096, "an outside shape's list has the same limit");
		Check(numbers.size() == 4097 && numbers[4095] != -1 && numbers[4096] == -1, "and the one past it is left as a picture that isn't there");
	}

	{
		LevelFile level;
		level.LoadLevelNames(4095);
		std::vector<int> const numbers = level.LoadShapeNames(2);
		Check(picturesAskedFor == 4096, "all of a file's lists count together, so splitting them up gets no more");
		Check(numbers.size() == 2 && numbers[0] != -1 && numbers[1] == -1, "and the name that goes past the limit is the one left out");
	}

	{
		LevelFile first;
		first.LoadLevelNames(4097);
	}

	{
		LevelFile second;
		second.LoadLevelNames(10);
		Check(picturesAskedFor == 10, "the next file starts with its own count");
		Check(linesInTheLog == 0, "and has nothing to say");
	}
}

// ---- 2. a refused picture and an animation ----

// A shape with two polygons, each with its own texture coordinates, which the animation replaces if it goes ahead
struct TwoPolygons
{
	int polygon[2][8];
	int *items[2];
	int coordinates[2][8];
	int *textures[2];
	SHAPEHEADER shape;

	TwoPolygons() : polygon(), items(), coordinates(), textures(), shape()
	{
		for (int i = 0; i < 2; i++)
		{
			polygon[i][0] = I_2dTexturedPolygon;
			polygon[i][3] = i << 16;   // which texture coordinates it uses
			items[i] = polygon[i];
			textures[i] = coordinates[i];
		}

		shape.numitems = 2;
		shape.items = items;
		shape.sh_textures = textures;
	}

	bool Animated(int which) const
	{
		return (polygon[which][2] & iflag_txanim) != 0;
	}

	bool KeptItsCoordinates(int which) const
	{
		return textures[which] == coordinates[which] && polygon[which][3] == which << 16;
	}

	// What the drawing code would pick up. Each sequence has one more frame than the file gave, which goes back to the first.
	int FrameShows(int which, int frame) const
	{
		txanimheader *const *const sequences = (txanimheader *const *)textures[which];
		return sequences[1]->txa_framedata[frame].txf_image;
	}
};

// A polygon's animation, as a level file holds it: one sequence of two frames, each showing the picture with that number
static void AddAnimatedPolygon(Bytes &to, int polygon, int firstFrameShows, int secondFrameShows)
{
	AddNumber(to, polygon);
	AddNumber(to, 0);
	AddNumber(to, 1);       // one sequence
	AddNumber(to, 3);       // of a polygon with three corners
	AddNumber(to, 0);
	AddNumber(to, 0);

	AddNumber(to, 65536);   // its speed
	AddNumber(to, 0);
	AddNumber(to, 2);       // two frames
	AddNumber(to, 0);
	AddNumber(to, 0);

	AddNumber(to, firstFrameShows);
	AddNumber(to, secondFrameShows);

	for (int i = 0; i < 2 * 2 * 3; i++)
	{
		AddNumber(to, 0);   // where each corner of each frame sits in the picture
	}
}

// The level names pictures 0, 1 and 2, and picture 2 is refused
static void LoadLevelWithARefusedPicture(LevelFile &level)
{
	pictureToRefuse = "picture2.RIM";
	level.LoadLevelNames(3);
}

static void SetUpAnimation(LevelFile &level, TwoPolygons &polygons, Bytes const &bytes)
{
	Animation_Chunk animation(&level.file, bytes.data(), bytes.size());
	SetupAnimatedTextures(0, &polygons.shape, &animation, 0, level.handle.max_index, level.handle.tex_index_nos);
}

static void TestARefusedPicture()
{
	puts("2. a refused picture and an animation");

	{
		LevelFile level;
		LoadLevelWithARefusedPicture(level);
		Check(level.handle.tex_index_nos[0] != -1 && level.handle.tex_index_nos[1] != -1, "a picture that loads gets its place in the list");
		Check(level.handle.tex_index_nos[2] == -1, "and a refused one leaves -1 there, which is where the danger starts");
	}

	{
		// Without this one, every test below could pass because animations never work at all
		LevelFile level;
		LoadLevelWithARefusedPicture(level);

		TwoPolygons polygons;
		Bytes bytes;
		AddNumber(bytes, 1);
		AddAnimatedPolygon(bytes, 0, 0, 1);
		SetUpAnimation(level, polygons, bytes);

		Check(polygons.Animated(0), "with real pictures, a polygon animates");
		Check((polygons.shape.shapeflags & ShapeFlag_HasTextureAnimation) != 0, "and its shape says it has animation");
		Check(polygons.FrameShows(0, 0) == level.handle.tex_index_nos[0], "and its first frame shows the first picture");
		Check(polygons.FrameShows(0, 1) == level.handle.tex_index_nos[1], "and its second frame shows the second");
	}

	{
		LevelFile level;
		LoadLevelWithARefusedPicture(level);

		TwoPolygons polygons;
		Bytes bytes;
		AddNumber(bytes, 1);
		AddAnimatedPolygon(bytes, 0, 0, 2);
		SetUpAnimation(level, polygons, bytes);

		Check(!polygons.Animated(0), "a frame showing a refused picture: the polygon stays still");
		Check(polygons.KeptItsCoordinates(0), "and keeps the texture it has, so no -1 becomes a frame");
		Check((polygons.shape.shapeflags & ShapeFlag_HasTextureAnimation) == 0, "and its shape doesn't say it has animation");
	}

	{
		LevelFile level;
		LoadLevelWithARefusedPicture(level);

		TwoPolygons polygons;
		Bytes bytes;
		AddNumber(bytes, 2);
		AddAnimatedPolygon(bytes, 0, 0, 1);
		AddAnimatedPolygon(bytes, 1, 2, 1);
		SetUpAnimation(level, polygons, bytes);

		Check(!polygons.Animated(1), "the last polygon of a shape is checked too");
		Check(!polygons.Animated(0) && polygons.KeptItsCoordinates(0), "and one bad frame keeps the whole shape still");
	}

	{
		LevelFile level;
		LoadLevelWithARefusedPicture(level);

		TwoPolygons polygons;
		Bytes bytes;
		AddNumber(bytes, 1);
		AddAnimatedPolygon(bytes, 0, 0, 3);
		SetUpAnimation(level, polygons, bytes);

		Check(!polygons.Animated(0) && polygons.KeptItsCoordinates(0), "a frame showing a picture past the end of the list: the polygon stays still");
	}
}

int main()
{
	TestTheLimit();
	TestARefusedPicture();

	printf("level_pictures_test: %d passed, %d failed\n", passed, failed);
	return failed ? 1 : 0;
}
