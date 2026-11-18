// Modified 2026-11-06 by Melanikus for the AvP port:
//   a stand-in for what the memory code now tells the game's sounds
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a place on an animation's picture too far out keeps the shape still (#97)
// Modified 2026-11-17 by Melanikus (Frost) for the AvP port:
//   5: a level with no list of pictures
//   6: the level editor's count of missing pictures
// What happens to the pictures a level file names, going through the game's real level loader. No game files are needed: each test builds
// the bytes of a small level file itself.
//
// Three things are checked here, and all come from level files being untrustworthy:
//
//  1. One file can only ask for so many pictures. Each one asked for costs time, even if there's no such picture, so a small file naming a
//     million of them could keep the game busy for as long as it liked.
//
//  2. A picture that can't be used must never end up as a frame of an animation. A picture that's refused leaves -1 in the level's list of
//     pictures. A polygon's animation copies a number from that list for each frame, and later the drawing code would use that -1 as a
//     texture number, reading far outside the list of textures. Every animated polygon in the game's own levels shows just one picture, so
//     if that picture is refused the polygon loses its texture and never animates. Only a crafted file can reach this: a polygon whose own
//     picture is real, with another frame showing one that isn't.
//
//  3. A frame's places on its picture are worked out in 16.16 (texture_animation.h), so one too far out would run over. A shape with one
//     stays still, the same way as for a refused picture. Every one in the game's own levels is 0 to 127 texels.
//
//  4. Not about pictures: this is the one test that runs chunk_load.cpp, so an animated shape's reach is checked here too. Its box and
//     radius have to come out right even at the farthest a place can be. (Added 2026-11-14 by Melanikus for the AvP port.)
//
//  5. A level with no list of pictures at all (no BMPNAMES) has no list of picture numbers, so a textured polygon's picture is looked up in
//     nothing: it has to become a line, as a polygon whose picture can't be had does. (Added 2026-11-17 by Melanikus (Frost) for the AvP port.)
//
//  6. The level editor's count of missing pictures: only the level's own (the load with CCF_ENVIRONMENT) are counted and named in the log,
//     with the polygons made lines for them, and in the game, which never turns it on, nothing is. (Added 2026-11-17 by Melanikus (Frost).)

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
#include "texture_animation.h"

// These are in chunk_load.cpp, which doesn't list them in a header
void SetupAnimatedTextures(Shape_Chunk *sc, SHAPEHEADER *shp, Animation_Chunk *ac, Shape_Merge_Data_Chunk *smdc, int local_max_index, int *local_tex_index_nos);
void setup_tex_conv_array(int &max_indices, int *&conv_array, RIFFHANDLE h, Chunk_With_Children *tmpshp);
void SetupAnimatingShape(Shape_Chunk *sc, SHAPEHEADER *shp, Shape_Merge_Data_Chunk *smdc);

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
	void LoadLevelNames(int howMany, int flags = 0)
	{
		Bytes bytes;
		AddNames(bytes, "BMPNAMES", howMany);

		std::unique_ptr<Environment_Data_Chunk> environment(new Environment_Data_Chunk(&file, bytes.data(), bytes.size()));
		handle.envd = environment.get();
		chunks.push_back(std::move(environment));
		load_rif_bitmaps(&handle, flags);
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

// A polygon's animation, as a level file holds it: one sequence of two frames, each showing the picture with that number. Each corner of
// each frame sits at the picture's own corner, except the very last number, which is lastPlace.
static void AddAnimatedPolygon(Bytes &to, int polygon, int firstFrameShows, int secondFrameShows, int lastPlace = 0)
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
		AddNumber(to, i == 2 * 2 * 3 - 1 ? lastPlace : 0);   // where each corner of each frame sits in the picture
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

// ---- 3. a place on the picture too far out (#97) ----

// Sets up a one-polygon animation of real pictures whose last corner sits at that place on its picture, and says whether it animates
static bool AnimatesWithLastPlace(int lastPlace, TwoPolygons &polygons)
{
	LevelFile level;
	LoadLevelWithARefusedPicture(level);

	Bytes bytes;
	AddNumber(bytes, 1);
	AddAnimatedPolygon(bytes, 0, 0, 1, lastPlace);
	SetUpAnimation(level, polygons, bytes);

	return polygons.Animated(0);
}

static void TestPlacesTooFarOut()
{
	puts("3. a place on the picture too far out");

	{
		TwoPolygons polygons;
		Check(AnimatesWithLastPlace(kTextureAnimationFurthestTexel, polygons), "the furthest place there can be animates");
	}

	{
		TwoPolygons polygons;
		Check(AnimatesWithLastPlace(-kTextureAnimationFurthestTexel, polygons), "and so does the furthest the other way");
	}

	{
		TwoPolygons polygons;
		Check(!AnimatesWithLastPlace(kTextureAnimationFurthestTexel + 1, polygons), "one texel further: the polygon stays still, because in 16.16 it wouldn't fit");
		Check(polygons.KeptItsCoordinates(0), "and keeps the texture it has");
		Check((polygons.shape.shapeflags & ShapeFlag_HasTextureAnimation) == 0, "and its shape doesn't say it has animation");
	}

	{
		TwoPolygons polygons;
		Check(!AnimatesWithLastPlace(-kTextureAnimationFurthestTexel - 1, polygons), "and one texel further the other way");
	}

	{
		LevelFile level;
		LoadLevelWithARefusedPicture(level);

		TwoPolygons polygons;
		Bytes bytes;
		AddNumber(bytes, 2);
		AddAnimatedPolygon(bytes, 0, 0, 1);
		AddAnimatedPolygon(bytes, 1, 0, 1, kTextureAnimationFurthestTexel + 1);
		SetUpAnimation(level, polygons, bytes);

		Check(!polygons.Animated(1) && !polygons.Animated(0) && polygons.KeptItsCoordinates(0), "the last polygon's last place is checked, and keeps the whole shape still");
	}
}

// ---- 4. an animated shape's reach ----

// A shape of 4 points and one triangle, with one animation of one frame, whose 4 points are these, around this centre. If leftOut, the
// triangle is left out of the animation's box, so no point is counted in it. The shape's own points and every normal are zeros, as allowed.
static Bytes AnimatedShape(int const framePoints[4][3], int const centre[3], bool leftOut)
{
	Bytes shape;

	Bytes header(68, 0);
	int const points = 4;
	int const polygons = 1;
	memcpy(&header[24], &points, sizeof(points));
	memcpy(&header[28], &polygons, sizeof(polygons));
	AddPart(shape, "SHPHEAD1", header);
	AddPart(shape, "SHPRAWVT", Bytes(12 * points, 0));

	// A polygon keeps its corners after 16 bytes of other numbers, and -1 ends them
	Bytes triangle(36, 0);
	int const corners[5] = { 0, 1, 2, -1, -1 };
	memcpy(&triangle[16], corners, sizeof(corners));
	AddPart(shape, "SHPPOLYS", triangle);
	AddPart(shape, "SHPVNORM", Bytes(12 * points, 0));
	AddPart(shape, "SHPPNORM", Bytes(12 * polygons, 0));

	// The animation's centre, then its flags and a spare number
	Bytes middle;
	for (int axis = 0; axis < 3; axis++)
	{
		AddNumber(middle, centre[axis]);
	}
	AddNumber(middle, 0);
	AddNumber(middle, 0);
	AddPart(shape, "ANSHCEN2", middle);

	// The polygons left out of the box, then two spare numbers
	if (leftOut)
	{
		Bytes outOfTheBox;
		AddNumber(outOfTheBox, 0);
		AddNumber(outOfTheBox, 0);
		AddNumber(outOfTheBox, 0);
		AddPart(shape, "PNOTINBB", outOfTheBox);
	}

	// The frame: its numbers (the first is its place, the second its flags) and an empty name, its points, and its polygon's normal
	Bytes frame;
	AddPart(frame, "ANIFRADT", Bytes(24, 0));
	Bytes framePointBytes;
	for (int point = 0; point < 4; point++)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			AddNumber(framePointBytes, framePoints[point][axis]);
		}
	}
	AddPart(frame, "SHPRAWVT", framePointBytes);
	AddPart(frame, "SHPPNORM", Bytes(12 * polygons, 0));

	// The sequence: its numbers (the first is its place) and an empty name, then its frame
	Bytes sequence;
	AddPart(sequence, "ANISEQDT", Bytes(24, 0));
	AddPart(sequence, "ANIMFRAM", frame);
	AddPart(shape, "ANIMSEQU", sequence);

	return shape;
}

struct Reach { bool accepted; int radius; int maxX; int minX; };

// Loads that shape as the level reader does, then sets up its animation as the game does, and gives back the sequence's radius and
// the two ends of its box along x, both measured from the centre.
static Reach AnimatedShapeReach(int const framePoints[4][3], int const centre[3], bool leftOut)
{
	LevelFile level;
	Bytes const bytes = AnimatedShape(framePoints, centre, leftOut);

	Shape_Chunk *const shape = new Shape_Chunk(&level.file, bytes.data(), bytes.size());
	level.chunks.push_back(std::unique_ptr<Chunk>(shape));
	shape->post_input_processing();

	Reach reach = { level.file.error_code == 0, 0, 0, 0 };

	if (reach.accepted)
	{
		// A shape's header keeps a list of lists of its points and normals, and setting up its animation points the first of each at the
		// animation's first frame, so the header needs those lists, as a loaded shape has
		int *points[1] = { 0 };
		int *normals[1] = { 0 };
		int *pointNormals[1] = { 0 };

		SHAPEHEADER header;
		memset(&header, 0, sizeof(header));
		header.numitems = 1;
		header.points = points;
		header.sh_normals = normals;
		header.sh_vnormals = pointNormals;
		local_scale = 1;
		SetupAnimatingShape(shape, &header, 0);

		shapeanimationsequence const &made = header.animation_header->anim_sequences[0];
		reach.radius = made.radius;
		reach.maxX = made.max_x;
		reach.minX = made.min_x;
	}

	return reach;
}

static void TestAnimatedShapeReach()
{
	puts("4. an animated shape's reach");

	int const farthest = 1 << 24;   // the farthest a place can be from the level's middle (CheckPlace, in chunk.hpp)

	{
		int const points[4][3] = { { 100, 50, 25 }, { -100, -50, -25 }, { 0, 0, 0 }, { 0, 0, 0 } };
		int const centre[3] = { 0, 0, 0 };
		Reach const reach = AnimatedShapeReach(points, centre, false);
		Check(reach.accepted && reach.radius == 114 && reach.maxX == 100 && reach.minX == -100, "an ordinary animated shape's box and radius are what they always were");
	}

	// Each side of the box is 2^25 from the centre, the most it can be, so the radius is the square root of 3 times 2^25
	{
		int const points[4][3] = { { farthest, farthest, farthest }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
		int const centre[3] = { -farthest, -farthest, -farthest };
		Reach const reach = AnimatedShapeReach(points, centre, false);
		Check(reach.accepted && reach.maxX == 2 * farthest && reach.minX == farthest, "a shape at the farthest corner, round a centre at the opposite one, has its box 2^25 from the centre");
		Check(reach.radius == 58117981, "and its radius is the square root of 3 times 2^25, 58,117,981, worked out without running over");
	}

	// With its one polygon left out of the box, no point is counted, so the box is a single place at the level's middle
	{
		int const points[4][3] = { { farthest, farthest, farthest }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
		int const centre[3] = { farthest, farthest, farthest };
		Reach const reach = AnimatedShapeReach(points, centre, true);
		Check(reach.accepted && reach.maxX == -farthest && reach.minX == -farthest, "a sequence with no point counted in its box has a box of a single place, at the level's middle");
		Check(reach.radius == 29058990, "and its radius is how far that is from the centre, the square root of 3 times 2^24, 29,058,990");
	}
}

// ---- 5. a level with no list of pictures ----

// One triangle with a texture, whose picture number and list of texture coordinates are in its colour, as a level file gives it, made into
// the game's shape (copy_to_shapeheader). Gives back the polygon's kind and colour as the game has them.
static void ConvertOneTexturedTriangle(LevelFile &level, unsigned int colour, int *kind, unsigned int *convertedColour)
{
	ChunkShape triangle;

	triangle.num_verts = 3;
	triangle.v_list = new ChunkVectorInt[3];
	triangle.v_normal_list = new ChunkVectorFloat[3];

	for (int i = 0; i < 3; i++)
	{
		triangle.v_list[i].x = i == 1 ? 100 : 0;
		triangle.v_list[i].y = i == 2 ? 100 : 0;
		triangle.v_list[i].z = 0;
		triangle.v_normal_list[i].x = 0;
		triangle.v_normal_list[i].y = 0;
		triangle.v_normal_list[i].z = 1;
	}

	triangle.num_polys = 1;
	triangle.poly_list = new ChunkPoly[1];
	triangle.p_normal_list = new ChunkVectorFloat[1];
	triangle.p_normal_list[0].x = 0;
	triangle.p_normal_list[0].y = 0;
	triangle.p_normal_list[0].z = 1;

	ChunkPoly &polygon = triangle.poly_list[0];

	polygon.engine_type = I_ZB_Gouraud3dTexturedPolygon;
	polygon.normal_index = 0;
	polygon.flags = 0;
	polygon.colour = colour;
	polygon.num_verts = 3;

	for (int i = 0; i < 4; i++)
	{
		polygon.vert_ind[i] = i < 3 ? i : -1;
	}

	triangle.num_uvs = 1;
	triangle.uv_list = new ChunkUV_List[1];
	triangle.uv_list[0].num_verts = 3;

	for (int i = 0; i < 4; i++)
	{
		triangle.uv_list[0].vert[i].u = 0;
		triangle.uv_list[0].vert[i].v = 0;
	}

	SHAPEHEADER *made = 0;

	local_scale = 1;
	copy_to_shapeheader(&level.handle, triangle, made, &level.file, 0, level.handle.max_index, level.handle.tex_index_nos);

	*kind = made ? made->items[0][0] : -1;
	*convertedColour = made ? (unsigned int)made->items[0][3] : 0;
}

static void TestNoListOfPictures()
{
	puts("5. a level with no list of pictures");

	{
		// A level file without BMPNAMES: the level has no list of picture numbers at all, so picture 0 (and every other) isn't one it has
		LevelFile level;

		load_rif_bitmaps(&level.handle, 0);
		Check(level.handle.tex_index_nos == 0, "a level with no BMPNAMES has no list of picture numbers");

		int kind = 0;
		unsigned int colour = 0;

		ConvertOneTexturedTriangle(level, 0, &kind, &colour);
		Check(kind == I_Polyline && colour == 0xffffffffu, "its polygon of picture 0 becomes a line, as one whose picture isn't there, and no list is read");
	}

	{
		// The same polygon in a level whose list has picture 0 keeps its texture
		LevelFile level;

		level.LoadLevelNames(1);

		int kind = 0;
		unsigned int colour = 0;

		ConvertOneTexturedTriangle(level, 0, &kind, &colour);
		Check(kind == I_ZB_Gouraud3dTexturedPolygon && colour == (unsigned int)level.handle.tex_index_nos[0], "with picture 0 in the list, the polygon keeps its texture");
	}
}

// ---- 6. the level editor's count of missing pictures ----

// R_SetEditorCountMissingPictures (renderer.h); the editor tells the user how many of a level's pictures couldn't be loaded, and how many polygons are lines for it
void R_SetEditorCountMissingPictures(bool on);
void R_EditorMissingPictures(int *picturesMissing, int *polygonsWithoutPicture, bool *noPictureList, int *picturesPastLimit);

static void TestTheEditorsCount()
{
	puts("6. the level editor's count of missing pictures");

	{
		// The level's own pictures load with CCF_ENVIRONMENT: picture 2 of its three is refused, and one polygon shows it
		LevelFile level;

		pictureToRefuse = "picture2.RIM";
		R_SetEditorCountMissingPictures(true);

		Bytes bytes;
		AddNames(bytes, "BMPNAMES", 3);

		std::unique_ptr<Environment_Data_Chunk> environment(new Environment_Data_Chunk(&level.file, bytes.data(), bytes.size()));
		level.handle.envd = environment.get();
		level.chunks.push_back(std::move(environment));
		load_rif_bitmaps(&level.handle, CCF_ENVIRONMENT);

		int kind = 0;
		unsigned int colour = 0;

		ConvertOneTexturedTriangle(level, 2, &kind, &colour);
		ConvertOneTexturedTriangle(level, 1, &kind, &colour);

		int missing = -1, lines = -1, pastLimit = -1;
		bool noList = true;

		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(missing == 1 && lines == 1 && !noList, "the level's one refused picture is counted, with its one polygon drawn as a line, and the level has a list");
		Check(linesInTheLog == 1, "and the log names it");

		// Another RIF's pictures after the level's (a character's, the HUD's) aren't the level's
		LevelFile another;

		pictureToRefuse = "picture0.RIM";
		another.LoadLevelNames(1);
		ConvertOneTexturedTriangle(another, 0, &kind, &colour);
		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(missing == 1 && lines == 1, "a later RIF's refused picture and its polygon aren't counted as the level's");
		R_SetEditorCountMissingPictures(false);
	}

	{
		// A level with no list of pictures at all
		LevelFile level;

		R_SetEditorCountMissingPictures(true);
		load_rif_bitmaps(&level.handle, CCF_ENVIRONMENT);

		int kind = 0;
		unsigned int colour = 0;

		ConvertOneTexturedTriangle(level, 0, &kind, &colour);

		int missing = -1, lines = -1, pastLimit = -1;
		bool noList = false;

		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(noList && missing == 0 && lines == 1, "a level with no list of pictures is said to have none, with its textured polygon counted as a line");
		R_SetEditorCountMissingPictures(false);
	}

	{
		// The game never sets the count: nothing is counted or said
		LevelFile level;

		pictureToRefuse = "picture0.RIM";
		R_SetEditorCountMissingPictures(true);
		R_SetEditorCountMissingPictures(false);

		Bytes bytes;
		AddNames(bytes, "BMPNAMES", 1);

		std::unique_ptr<Environment_Data_Chunk> environment(new Environment_Data_Chunk(&level.file, bytes.data(), bytes.size()));
		level.handle.envd = environment.get();
		level.chunks.push_back(std::move(environment));
		load_rif_bitmaps(&level.handle, CCF_ENVIRONMENT);

		int missing = -1, lines = -1, pastLimit = -1;
		bool noList = true;

		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(missing == 0 && lines == 0 && !noList && linesInTheLog == 0, "in the game nothing is counted, and the log says nothing");
	}

	{
		// A missing picture two outside shapes of the level name (each shape's BMPLSTST; their names are asked for as "<its RIF>/<name>", so with no RIF name as
		// "/picture2.RIM", the same for both): counted and named once (the review, 2026-11-18)
		LevelFile level;

		R_SetEditorCountMissingPictures(true);
		level.LoadLevelNames(3, CCF_ENVIRONMENT);
		pictureToRefuse = "/picture2.RIM";
		level.LoadShapeNames(3);

		int missing = -1, lines = -1, pastLimit = -1;
		bool noList = true;

		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(missing == 1 && linesInTheLog == 1, "the first outside shape's missing picture is counted and named");

		LevelFile secondShape;   // still the level's load (no other RIF's pictures since): only the list is another shape's

		pictureToRefuse = "/picture2.RIM";
		secondShape.LoadShapeNames(3);
		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(picturesAskedFor == 3 && missing == 1 && linesInTheLog == 0, (std::string("a second shape naming the same missing picture asks for it again, but it is not counted or named again (") +
			std::to_string(missing) + " counted, " + std::to_string(linesInTheLog) + " more lines in the log)").c_str());
		R_SetEditorCountMissingPictures(false);
	}

	{
		// Past the limit of 4096 a file may ask for: those are counted apart, not as missing pictures the log names (the review, 2026-11-18)
		LevelFile level;

		R_SetEditorCountMissingPictures(true);
		level.LoadLevelNames(4098, CCF_ENVIRONMENT);

		int missing = -1, lines = -1, pastLimit = -1;
		bool noList = true;

		R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);
		Check(missing == 0 && pastLimit == 2 && linesInTheLog == 1, (std::string("4098 names that all load: none missing, 2 past the limit, and the log says once that there are too many (") +
			std::to_string(missing) + " / " + std::to_string(pastLimit) + " / " + std::to_string(linesInTheLog) + ")").c_str());
		R_SetEditorCountMissingPictures(false);
	}
}

int main()
{
	TestTheLimit();
	TestARefusedPicture();
	TestPlacesTooFarOut();
	TestAnimatedShapeReach();
	TestNoListOfPictures();
	TestTheEditorsCount();

	printf("level_pictures_test: %d passed, %d failed\n", passed, failed);
	return failed ? 1 : 0;
}
