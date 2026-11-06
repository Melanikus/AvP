// The chunk room test: does any kind of chunk read past the end of its own data?
// Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A level file is a tree of chunks, and each chunk starts by saying how many bytes it holds. A file can say anything, so a chunk that says it holds 4 bytes must not read a
// fifth, whatever it was expecting to find. Most of the time a read like that lands in the next chunk and nothing shows, so a test can't see it just by looking at what came out.
//
// So here, each chunk ends exactly where readable memory ends. The next page of memory is marked so that touching it stops the program, which means that reading even one byte
// too many is caught, every time, with no luck in it. This test catches that stop, notes which chunk did it and how far past its end it went, and carries on with the next.
//
// It tries every kind of chunk the game can make from a level file, at every size from nothing up to LargestSizeTried bytes, filled with each of a few patterns of bytes. The
// patterns matter because what a chunk reads can depend on what it finds: a count of nothing reads no list at all, and a count of one reads one entry.
//
// THE LIST OF KINDS
//
// Each kind is in the table below, with the chunks it has to be inside, because some kinds are only made inside a named parent. The game's own list of kinds is only filled in
// while it runs and is private to chunk.cpp, so the table is a copy of it. A row whose kind never got made proves nothing, so that counts as a failure: a misspelt id can't pass.
//
// HOW FAR THIS REACHES
//
// It only sees reads that these fill patterns lead to, and it only looks at a chunk being made. What the game later does with what the chunk read is for other tests.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "oechunk.h"

// The chunk code can open files, through these two functions from another part of the game. This test only gives it bytes in memory, so they answer that there is no such file.
HANDLE avp_CreateFile(const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
{
	return INVALID_HANDLE_VALUE;
}

DWORD avp_GetFileAttributes(const char *)
{
	return INVALID_FILE_ATTRIBUTES;
}

// The memory code stops the game through these if the computer runs out of memory. Here that stops the test, and what it says is the last line of the output.
void avp_MessageBox(const char * message, int)
{
	puts(message);
}

void avp_exit(int code)
{
	exit(code);
}

// Big enough for the largest fixed set of fields any kind reads (204 bytes, in a map block), with room to spare for a list after them.
static size_t const LargestSizeTried = 320;

static size_t const ChunkHeaderBytes = 12;
static size_t const MostParents = 3;

struct Kind
{
	char const * id;
	char const * inside[MostParents];   // the chunks it has to be inside, from the outside in, and 0 for no more
	char const * className;
};

static Kind const kinds[] =
{
	{ "ADJMDLEP", { 0, 0, 0 }, "Adjacent_Module_Entry_Points_Chunk" },
	{ "AIMODMAS", { "MODULEDT", 0, 0 }, "AI_Module_Master_Chunk" },
	{ "AIMODSLA", { "MODULEDT", 0, 0 }, "AI_Module_Slave_Chunk" },
	{ "ALTLOCAT", { 0, 0, 0 }, "Object_Alternate_Locations_Chunk" },
	{ "AMBIENCE", { 0, 0, 0 }, "Lighting_Ambience_Chunk" },
	{ "ANIFRADT", { "ANIMSEQU", "ANIMFRAM", 0 }, "Anim_Shape_Frame_Data_Chunk" },
	{ "ANIMFRAM", { "ANIMSEQU", 0, 0 }, "Anim_Shape_Frame_Chunk" },
	{ "ANIMSEQU", { 0, 0, 0 }, "Anim_Shape_Sequence_Chunk" },
	{ "ANISEQDT", { "ANIMSEQU", 0, 0 }, "Anim_Shape_Sequence_Data_Chunk" },
	{ "ANSHCEN2", { 0, 0, 0 }, "Anim_Shape_Centre_Chunk" },
	{ "ASALTTEX", { 0, 0, 0 }, "Anim_Shape_Alternate_Texturing_Chunk" },
	{ "ASPRHEAD", { 0, 0, 0 }, "AllSprites_Header_Chunk" },
	{ "AVPCABLE", { 0, 0, 0 }, "AVP_Power_Cable_Chunk" },
	{ "AVPDECAL", { 0, 0, 0 }, "AVP_Decal_Chunk" },
	{ "AVPENVIR", { 0, 0, 0 }, "AVP_Environment_Settings_Chunk" },
	{ "AVPGENER", { "SPECLOBJ", 0, 0 }, "AVP_Generator_Chunk" },
	{ "AVPGENEX", { 0, 0, 0 }, "AVP_Generator_Extra_Data_Chunk" },
	{ "AVPGENNM", { 0, 0, 0 }, "AVP_Generator_Extra_Name_Chunk" },
	{ "AVPPATH2", { 0, 0, 0 }, "AVP_Path_Chunk" },
	{ "AVPSTART", { 0, 0, 0 }, "AVP_Player_Start_Chunk" },
	{ "AVPSTRAT", { 0, 0, 0 }, "AVP_Strategy_Chunk" },
	{ "BMNAMEXT", { 0, 0, 0 }, "BMP_Names_ExtraData_Chunk" },
	{ "BMNAMVER", { 0, 0, 0 }, "BMP_Names_Version_Chunk" },
	{ "BMPENUMS", { 0, 0, 0 }, "BMP_Enums_Chunk" },
	{ "BMPLSTST", { 0, 0, 0 }, "Bitmap_List_Store_Chunk" },
	{ "BMPMD5ID", { 0, 0, 0 }, "Bitmap_MD5_Chunk" },
	{ "BMPNAMES", { 0, 0, 0 }, "Global_BMP_Name_Chunk" },
	{ "CLRLOOKP", { 0, 0, 0 }, "Coloured_Polygons_Lookup_Chunk" },
	{ "DUMMYOBJ", { 0, 0, 0 }, "Dummy_Object_Chunk" },
	{ "DUMOBJDT", { 0, 0, 0 }, "Dummy_Object_Data_Chunk" },
	{ "DUMOBJTX", { 0, 0, 0 }, "Dummy_Object_Text_Chunk" },
	{ "ENDTHEAD", { 0, 0, 0 }, "Environment_Data_Header_Chunk" },
	{ "ENUMHEAD", { 0, 0, 0 }, "Enum_Header_Chunk" },
	{ "ENVACOUS", { 0, 0, 0 }, "Environment_Acoustics_Chunk" },
	{ "ENVPALET", { 0, 0, 0 }, "Environment_Palette_Chunk" },
	{ "ENVSDSCL", { 0, 0, 0 }, "Environment_Scale_Chunk" },
	{ "ENVTXLIT", { 0, 0, 0 }, "Environment_TLT_Chunk" },
	{ "EXTOBJNM", { 0, 0, 0 }, "Shape_External_Object_Name_Chunk" },
	{ "FRAGDATA", { 0, 0, 0 }, "Shape_Fragments_Data_Chunk" },
	{ "FRAGLOCN", { 0, 0, 0 }, "Shape_Fragment_Location_Chunk" },
	{ "FRAGTYPE", { 0, 0, 0 }, "Fragment_Type_Chunk" },
	{ "FRGSOUND", { 0, 0, 0 }, "Fragment_Type_Sound_Chunk" },
	{ "FRGTYPDC", { 0, 0, 0 }, "Fragment_Type_Data_Chunk" },
	{ "FRGTYPSC", { 0, 0, 0 }, "Fragment_Type_Shape_Chunk" },
	{ "FRMMORPH", { "REBSHAPE", "SHPMORPH", 0 }, "Shape_Morphing_Frame_Data_Chunk" },
	{ "GAMEMODE", { "REBENVDT", 0, 0 }, "Environment_Game_Mode_Chunk" },
	{ "GENEXSET", { 0, 0, 0 }, "AVP_Generator_Extended_Settings_Chunk" },
	{ "GLOGENDC", { 0, 0, 0 }, "Global_Generator_Data_Chunk" },
	{ "GMODHEAD", { "REBENVDT", "GAMEMODE", 0 }, "Environment_Game_Mode_Header_Chunk" },
	{ "HIDEGDIS", { 0, 0, 0 }, "Hierarchy_Degradation_Distance_Chunk" },
	{ "HIERBBOX", { 0, 0, 0 }, "Hierarchy_Bounding_Box_Chunk" },
	{ "HSETCOLL", { 0, 0, 0 }, "Hierarchy_Shape_Set_Collection_Chunk" },
	{ "INDSOUND", { 0, 0, 0 }, "Indexed_Sound_Chunk" },
	{ "LIGHTSET", { 0, 0, 0 }, "Light_Set_Chunk" },
	{ "LITSCALE", { 0, 0, 0 }, "Light_Scale_Chunk" },
	{ "LTSETHDR", { 0, 0, 0 }, "Light_Set_Header_Chunk" },
	{ "MAPBLOCK", { 0, 0, 0 }, "Map_Block_Chunk" },
	{ "MATCHIMG", { 0, 0, 0 }, "Matching_Images_Chunk" },
	{ "MODACOUS", { 0, 0, 0 }, "Module_Acoustics_Chunk" },
	{ "MODFLAGS", { 0, 0, 0 }, "Module_Flag_Chunk" },
	{ "MODULEDT", { 0, 0, 0 }, "Object_Module_Data_Chunk" },
	{ "MODZONE", { 0, 0, 0 }, "Module_Zone_Chunk" },
	{ "OBANALLS", { 0, 0, 0 }, "Object_Animation_All_Sequence_Chunk" },
	{ "OBANSEQC", { 0, 0, 0 }, "Object_Animation_Sequence_Chunk" },
	{ "OBANSEQS", { 0, 0, 0 }, "Object_Animation_Sequences_Chunk" },
	{ "OBASEQFL", { 0, 0, 0 }, "Object_Animation_Sequence_Flags_Chunk" },
	{ "OBASEQFR", { 0, 0, 0 }, "Object_Animation_Sequence_Frame_Chunk" },
	{ "OBASEQHD", { 0, 0, 0 }, "Object_Animation_Sequence_Header_Chunk" },
	{ "OBASEQSP", { 0, 0, 0 }, "Object_Animation_Sequence_Speed_Chunk" },
	{ "OBASEQTM", { 0, 0, 0 }, "Object_Animation_Sequence_Time_Chunk" },
	{ "OBHALTSH", { 0, 0, 0 }, "Object_Hierarchy_Alternate_Shape_Set_Chunk" },
	{ "OBHIERNM", { 0, 0, 0 }, "Object_Hierarchy_Name_Chunk" },
	{ "OBINTDT", { 0, 0, 0 }, "Object_Interface_Data_Chunk" },
	{ "OBJCHIER", { 0, 0, 0 }, "Object_Hierarchy_Chunk" },
	{ "OBJHEAD1", { "RBOBJECT", 0, 0 }, "Object_Header_Chunk" },
	{ "OBJHIERD", { 0, 0, 0 }, "Object_Hierarchy_Data_Chunk" },
	{ "OBJNOTES", { 0, 0, 0 }, "Object_Notes_Chunk" },
	{ "OBJPRJDT", { 0, 0, 0 }, "Object_Project_Data_Chunk" },
	{ "OBJTRAK2", { 0, 0, 0 }, "Object_Track_Chunk2" },
	{ "PARGENDA", { 0, 0, 0 }, "AVP_Particle_Generator_Data_Chunk" },
	{ "PARGENER", { 0, 0, 0 }, "AVP_Particle_Generator_Chunk" },
	{ "PLACHIDT", { 0, 0, 0 }, "Placed_Hierarchy_Data_Chunk" },
	{ "PLACHIER", { 0, 0, 0 }, "Placed_Hierarchy_Chunk" },
	{ "PLHISEQU", { 0, 0, 0 }, "Placed_Hierarchy_Sequence_Chunk" },
	{ "PLOBJLIT", { 0, 0, 0 }, "Placed_Object_Light_Chunk" },
	{ "PNOTINBB", { 0, 0, 0 }, "Poly_Not_In_Bounding_Box_Chunk" },
	{ "PRSETPAL", { 0, 0, 0 }, "Preset_Palette_Chunk" },
	{ "R6GENDAT", { 0, 0, 0 }, "Rainbow6_Generator_Extra_Data_Chunk" },
	{ "RANTEXID", { 0, 0, 0 }, "Random_Texture_ID_Chunk" },
	{ "RBOBJECT", { 0, 0, 0 }, "Object_Chunk" },
	{ "REBENUMS", { 0, 0, 0 }, "Enum_Chunk" },
	{ "REBENVDT", { 0, 0, 0 }, "Environment_Data_Chunk" },
	{ "REBSHAPE", { 0, 0, 0 }, "Shape_Chunk" },
	{ "RIFCHILD", { "REBENVDT", "GAMEMODE", 0 }, "RIF_Child_Chunk" },
	{ "RIFFNAME", { 0, 0, 0 }, "RIF_Name_Chunk" },
	{ "RIFVERIN", { 0, 0, 0 }, "RIF_Version_Num_Chunk" },
	{ "RSPRITES", { 0, 0, 0 }, "AllSprites_Chunk" },
	{ "SETPALST", { 0, 0, 0 }, "Preset_Palette_Store_Chunk" },
	{ "SHBMPNAM", { 0, 0, 0 }, "External_Shape_BMPs_Store_Chunk" },
	{ "SHPCENTR", { "REBSHAPE", 0, 0 }, "Shape_Centre_Chunk" },
	{ "SHPCENTR", { "SUBSHAPE", 0, 0 }, "Shape_Centre_Chunk" },
	{ "SHPEXTFL", { 0, 0, 0 }, "Shape_External_File_Chunk" },
	{ "SHPEXTFN", { 0, 0, 0 }, "Shape_External_Filename_Chunk" },
	{ "SHPFNAME", { 0, 0, 0 }, "Shape_Name_Chunk" },
	{ "SHPFRAGS", { 0, 0, 0 }, "Shape_Fragments_Chunk" },
	{ "SHPFRGTP", { 0, 0, 0 }, "Shape_Fragment_Type_Chunk" },
	{ "SHPHEAD1", { "REBSHAPE", 0, 0 }, "Shape_Header_Chunk" },
	{ "SHPMORPH", { "REBSHAPE", 0, 0 }, "Shape_Morphing_Data_Chunk" },
	{ "SHPMORPH", { "SUBSHAPE", 0, 0 }, "Shape_Morphing_Data_Chunk" },
	{ "SHPMRGDT", { "REBSHAPE", 0, 0 }, "Shape_Merge_Data_Chunk" },
	{ "SHPMRGDT", { "SUBSHAPE", 0, 0 }, "Shape_Merge_Data_Chunk" },
	{ "SHPPCINF", { 0, 0, 0 }, "Shape_Poly_Change_Info_Chunk" },
	{ "SHPPNORM", { "ANIMSEQU", "ANIMFRAM", 0 }, "Shape_Polygon_Normal_Chunk" },
	{ "SHPPNORM", { "REBSHAPE", 0, 0 }, "Shape_Polygon_Normal_Chunk" },
	{ "SHPPNORM", { "SUBSHAPE", 0, 0 }, "Shape_Polygon_Normal_Chunk" },
	{ "SHPPOLYS", { "REBSHAPE", 0, 0 }, "Shape_Polygon_Chunk" },
	{ "SHPPOLYS", { "SUBSHAPE", 0, 0 }, "Shape_Polygon_Chunk" },
	{ "SHPRAWVT", { "ANIMSEQU", "ANIMFRAM", 0 }, "Shape_Vertex_Chunk" },
	{ "SHPRAWVT", { "REBSHAPE", 0, 0 }, "Shape_Vertex_Chunk" },
	{ "SHPRAWVT", { "SUBSHAPE", 0, 0 }, "Shape_Vertex_Chunk" },
	{ "SHPTEXFN", { "REBSHAPE", 0, 0 }, "Shape_Texture_Filenames_Chunk" },
	{ "SHPTEXFN", { "SUBSHAPE", 0, 0 }, "Shape_Texture_Filenames_Chunk" },
	{ "SHPUVCRD", { "REBSHAPE", 0, 0 }, "Shape_UV_Coord_Chunk" },
	{ "SHPUVCRD", { "SUBSHAPE", 0, 0 }, "Shape_UV_Coord_Chunk" },
	{ "SHPVNORM", { "REBSHAPE", 0, 0 }, "Shape_Vertex_Normal_Chunk" },
	{ "SHPVNORM", { "SUBSHAPE", 0, 0 }, "Shape_Vertex_Normal_Chunk" },
	{ "SHPVTINT", { 0, 0, 0 }, "Shape_Vertex_Intensities_Chunk" },
	{ "SOUNDDIR", { 0, 0, 0 }, "Sound_Directory_Chunk" },
	{ "SOUNDEXD", { 0, 0, 0 }, "Sound_Object_Extra_Data_Chunk" },
	{ "SOUNDNAM", { 0, 0, 0 }, "Sound_Object_Extra_Name_Chunk" },
	{ "SOUNDOB2", { 0, 0, 0 }, "Sound_Object_Chunk" },
	{ "SPECLOBJ", { 0, 0, 0 }, "Special_Objects_Chunk" },
	{ "SPRACTIO", { 0, 0, 0 }, "Sprite_Action_Chunk" },
	{ "SPRBMPCE", { 0, 0, 0 }, "Sprite_Bitmap_Centre_Chunk" },
	{ "SPRBMPSC", { 0, 0, 0 }, "Sprite_Bitmap_Scale_Chunk" },
	{ "SPREXTEN", { 0, 0, 0 }, "Sprite_Extent_Chunk" },
	{ "SPRIHEAD", { 0, 0, 0 }, "Sprite_Header_Chunk" },
	{ "SPRISIZE", { 0, 0, 0 }, "Sprite_Size_Chunk" },
	{ "SPRITEPC", { "SPRIHEAD", 0, 0 }, "PC_Sprite_Chunk" },
	{ "SPRITEPS", { "SPRIHEAD", 0, 0 }, "Playstation_Sprite_Chunk" },
	{ "SPRITESA", { "SPRIHEAD", 0, 0 }, "Saturn_Sprite_Chunk" },
	{ "SPRITVER", { 0, 0, 0 }, "Sprite_Version_Number_Chunk" },
	{ "STDLIGHT", { 0, 0, 0 }, "Light_Chunk" },
	{ "STRATEGY", { 0, 0, 0 }, "Strategy_Chunk" },
	{ "SUBSHAPE", { 0, 0, 0 }, "Shape_Sub_Shape_Chunk" },
	{ "SUBSHPHD", { "SUBSHAPE", 0, 0 }, "Shape_Sub_Shape_Header_Chunk" },
	{ "TEXTANIM", { 0, 0, 0 }, "Animation_Chunk" },
	{ "TLTCONFG", { 0, 0, 0 }, "TLT_Config_Chunk" },
	{ "TRAKSOUN", { 0, 0, 0 }, "Object_Track_Sound_Chunk" },
	{ "VIOBJECT", { 0, 0, 0 }, "Virtual_Object_Chunk" },
	{ "VMDARRAY", { 0, 0, 0 }, "VModule_Array_Chunk" },
	{ "VOBJPROP", { 0, 0, 0 }, "Virtual_Object_Properties_Chunk" },
	{ "WAYPOINT", { 0, 0, 0 }, "Module_Waypoint_Chunk" },
};

// What fills a chunk's bytes, all of them. The plain patterns come first. After them come the name patterns, one for every mix of how many numbers come first and what those numbers
// are. Those numbers can be counts, and after them everything is letters, up to a zero byte that's the chunk's very last. So a name takes up all the room that's left and still ends
// inside the chunk, which leaves anything the chunk reads after a name with nowhere to read from.
enum { Zeros, Letters, NumbersOfOne, NumbersOfTwo, AllOnes, HowManyPlainPatterns };
static char const * const plainPatternNames[HowManyPlainPatterns] = { "zero bytes", "letters", "numbers of 1", "numbers of 2", "bytes of 0xff" };

static int const MostNumbersBeforeAName = 12;
static int const LargestNumberBeforeAName = 2;
static int const HowManyPatterns = HowManyPlainPatterns + LargestNumberBeforeAName * (MostNumbersBeforeAName + 1);

// These are slower to get through, there being so many of them, and no kind has both a name and more than this many bytes of numbers.
static size_t const LargestSizeTriedWithAName = 160;

static size_t LargestSizeFor(int pattern)
{
	return (pattern < HowManyPlainPatterns) ? LargestSizeTried : LargestSizeTriedWithAName;
}

static int NumbersBeforeTheName(int pattern)
{
	return (pattern - HowManyPlainPatterns) % (MostNumbersBeforeAName + 1);
}

static int ValueOfThoseNumbers(int pattern)
{
	return (pattern - HowManyPlainPatterns) / (MostNumbersBeforeAName + 1) + 1;
}

static void SayPattern(int pattern)
{
	if (pattern < HowManyPlainPatterns)
	{
		fputs(plainPatternNames[pattern], stdout);
	}
	else
	{
		printf("%d numbers of %d and a name", NumbersBeforeTheName(pattern), ValueOfThoseNumbers(pattern));
	}
}

static void FillWithPattern(char * bytes, size_t size, int pattern)
{
	if (pattern >= HowManyPlainPatterns)
	{
		memset(bytes, 'x', size);

		size_t const numbersBytes = (size_t)NumbersBeforeTheName(pattern) * 4;

		for (size_t at = 0; at < numbersBytes && at < size; at++)
		{
			bytes[at] = (at % 4 == 0) ? (char)ValueOfThoseNumbers(pattern) : 0;
		}

		if (size)
		{
			bytes[size - 1] = 0;
		}

		return;
	}

	switch (pattern)
	{
	case Letters:
		memset(bytes, 'x', size);
		break;
	case AllOnes:
		memset(bytes, 0xff, size);
		break;
	case NumbersOfOne:
	case NumbersOfTwo:
		memset(bytes, 0, size);
		for (size_t at = 0; at < size; at += 4)
		{
			bytes[at] = (pattern == NumbersOfOne) ? 1 : 2;
		}
		break;
	default:
		memset(bytes, 0, size);
		break;
	}
}

// Memory that ends at a wall: readable up to wall, and the next page stops the program when it's touched. Nothing here is ever run as code.
static char * wall = 0;
static size_t pageBytes = 0;

static void MakeTheWall()
{
	SYSTEM_INFO system;
	GetSystemInfo(&system);
	pageBytes = system.dwPageSize;

	size_t const readable = 16 * pageBytes;
	DWORD before = 0;
	char * const block = (char *)VirtualAlloc(NULL, readable + pageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (!block || !VirtualProtect(block + readable, pageBytes, PAGE_NOACCESS, &before))
	{
		puts("Couldn't make memory that ends at a wall");
		exit(1);
	}

	wall = block + readable;
}

static void PutChunkHeader(char * at, char const * id, size_t dataBytes)
{
	int const wholeChunk = (int)(dataBytes + ChunkHeaderBytes);

	memset(at, 0, 8);
	memcpy(at, id, strlen(id));
	memcpy(at + 8, &wholeChunk, sizeof(wholeChunk));
}

// A sequence (ANIMSEQU) gives itself back when it has no data chunk, and takes everything inside it with it, so wherever one is a parent it gets one of these first.
static size_t const SequenceDataBytes = 24;

// Lays out a chunk of this kind, with its parents around it, so that its last byte is the last readable one. Gives back where the outermost chunk starts.
static char * PutChunkAtTheWall(Kind const & kind, size_t size, int pattern)
{
	size_t parents = 0;
	size_t extra = 0;

	while (parents < MostParents && kind.inside[parents])
	{
		if (!strcmp(kind.inside[parents], "ANIMSEQU"))
		{
			extra += ChunkHeaderBytes + SequenceDataBytes;
		}

		parents++;
	}

	char * const start = wall - size - extra - ChunkHeaderBytes * (parents + 1);
	char * at = start;

	for (size_t i = 0; i < parents; i++)
	{
		PutChunkHeader(at, kind.inside[i], (size_t)(wall - at) - ChunkHeaderBytes);
		at += ChunkHeaderBytes;

		if (!strcmp(kind.inside[i], "ANIMSEQU"))
		{
			PutChunkHeader(at, "ANISEQDT", SequenceDataBytes);
			memset(at + ChunkHeaderBytes, 0, SequenceDataBytes);
			at += ChunkHeaderBytes + SequenceDataBytes;
		}
	}

	PutChunkHeader(at, kind.id, size);
	FillWithPattern(at + ChunkHeaderBytes, size, pattern);

	return start;
}

// Was a chunk of this kind really made, and not just kept as plain bytes? The file owns everything made in it, so this looks there, and not at what was handed back: a few kinds
// give themselves back when they find they aren't valid.
static bool KindWasMade(File_Chunk const * file, Kind const & kind)
{
	Chunk_With_Children const * at = file;

	for (size_t i = 0; i < MostParents && kind.inside[i]; i++)
	{
		at = dynamic_cast<Chunk_With_Children const *>(at->lookup_single_child(kind.inside[i]));

		if (!at)
		{
			return false;
		}
	}

	Chunk * const found = at->lookup_single_child(kind.id);

	return found && !dynamic_cast<Miscellaneous_Chunk *>(found);
}

// A sequence with any part missing gives itself back, and everything inside it goes too. A sequence can't be whole here, because the chunk being tried is the only part it has. So
// these kinds are made and gone again before anyone can look, and whether they were made has to be shown another way: taking out one of their "holding at least" numbers stops this test.
static bool GoneBeforeAnyoneCanLook(Kind const & kind)
{
	if (!strcmp(kind.id, "ANIMSEQU"))
	{
		return true;
	}

	for (size_t i = 0; i < MostParents && kind.inside[i]; i++)
	{
		if (!strcmp(kind.inside[i], "ANIMSEQU"))
		{
			return true;
		}
	}

	return false;
}

// What stopped the program, if anything did.
struct Stop
{
	DWORD code;
	char const * address;
};

static Stop lastStop;

static int NoteTheStop(EXCEPTION_POINTERS const * what)
{
	lastStop.code = what->ExceptionRecord->ExceptionCode;
	lastStop.address = (what->ExceptionRecord->NumberParameters >= 2) ? (char const *)what->ExceptionRecord->ExceptionInformation[1] : 0;

	return EXCEPTION_EXECUTE_HANDLER;
}

// These two are on their own because a function that catches a stop like this can't hold anything that has to be given back on the way out.
static bool MakeWithoutStopping(File_Chunk * file, char const * start, Kind const & kind, bool * made)
{
	__try
	{
		file->DynCreate(start);
		*made = KindWasMade(file, kind);
		return true;
	}
	__except (NoteTheStop(GetExceptionInformation()))
	{
		return false;
	}
}

static bool GiveBackWithoutStopping(File_Chunk * file)
{
	__try
	{
		delete file;
		return true;
	}
	__except (NoteTheStop(GetExceptionInformation()))
	{
		return false;
	}
}

// Says what stopped the program, in words, for one kind's first stop with each pattern.
static void SayWhatStopped(Kind const & kind, size_t size, int pattern, char const * when)
{
	printf("  FAILED  %s", kind.id);

	for (size_t i = 0; i < MostParents && kind.inside[i]; i++)
	{
		printf(" in %s", kind.inside[i]);
	}

	printf(" (%s): holding %u bytes of ", kind.className, (unsigned)size);
	SayPattern(pattern);
	printf(", %s: ", when);

	if (lastStop.code == EXCEPTION_ACCESS_VIOLATION && lastStop.address >= wall && lastStop.address < wall + pageBytes)
	{
		printf("read %u bytes past its end\n", (unsigned)(lastStop.address - wall) + 1);
	}
	else
	{
		printf("stopped with code %08lx at %p, which isn't the wall\n", (unsigned long)lastStop.code, (void const *)lastStop.address);
	}
}

// Tries one kind with one pattern, at every size, from the largest down, so that the first stop is at the largest size that still reads too far. Says whether it got through with
// no stop at all, and sets everMade if a chunk of this kind really was made.
static bool TryOnePattern(Kind const & kind, int pattern, bool * everMade)
{
	for (int size = (int)LargestSizeFor(pattern); size >= 0; size--)
	{
		char const * const start = PutChunkAtTheWall(kind, size, pattern);

		File_Chunk * const file = new File_Chunk;
		Parent_File = file;

		bool made = false;

		// After a stop, the half-made chunks in the file can't safely be given back, so they're left. The test is about to end anyway.
		if (!MakeWithoutStopping(file, start, kind, &made))
		{
			SayWhatStopped(kind, size, pattern, "while being made");
			return false;
		}

		*everMade = *everMade || made;

		if (!GiveBackWithoutStopping(file))
		{
			SayWhatStopped(kind, size, pattern, "while being given back");
			return false;
		}

		Parent_File = 0;
	}

	return true;
}

// A map block keeps its "interior type" 156 bytes in, which is where the code that writes one puts it. It stayed inside its chunk when it read from the wrong place, so nothing about
// walls could show this. It gets a look of its own.
static bool MapBlockFindsItsInteriorType()
{
	char bytes[204] = { 0 };
	int const interiorType = 7;
	memcpy(bytes + 156, &interiorType, sizeof(interiorType));

	File_Chunk * const file = new File_Chunk;
	Parent_File = file;

	Map_Block_Chunk const * const block = new Map_Block_Chunk(file, bytes, sizeof(bytes));
	bool const found = (block->map_data.MapInteriorType == interiorType);

	delete file;
	Parent_File = 0;

	return found;
}

static bool Says(bool good, char const * what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what);
	return good;
}

// What became of one chunk, made by itself at the wall.
struct WhatHappened
{
	bool made;
	int errorCode;
};

static WhatHappened MakeOne(Kind const & kind, size_t size)
{
	char const * const start = PutChunkAtTheWall(kind, size, Zeros);

	File_Chunk * const file = new File_Chunk;
	Parent_File = file;

	file->DynCreate(start);

	WhatHappened const what = { KindWasMade(file, kind), file->error_code };

	delete file;
	Parent_File = 0;

	return what;
}

// A chunk refused for being too small has to say why, and one holding just enough has to get through. Reading no further than the wall shows neither, because a level that's quietly
// let through with half its fields missing doesn't touch the wall, and neither does one refused for no good reason.
static bool RefusalsSayWhy()
{
	Kind const mapBlock = { "MAPBLOCK", { 0, 0, 0 }, "Map_Block_Chunk" };
	Kind const colours = { "CLRLOOKP", { 0, 0, 0 }, "Coloured_Polygons_Lookup_Chunk" };

	size_t const mapBlockBytes = 204;
	size_t const colourBytes = 32 + (1 << 15);   // its numbers, and then its table

	WhatHappened const blockOneShort = MakeOne(mapBlock, mapBlockBytes - 1);
	WhatHappened const wholeBlock = MakeOne(mapBlock, mapBlockBytes);
	WhatHappened const tableOneShort = MakeOne(colours, colourBytes - 1);
	WhatHappened const wholeTable = MakeOne(colours, colourBytes);

	bool ok = true;
	ok = Says(!blockOneShort.made && blockOneShort.errorCode == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a chunk one byte too small for what its kind reads is refused, and the file says why") && ok;
	ok = Says(wholeBlock.made && wholeBlock.errorCode == 0, "a chunk holding exactly what its kind reads is made, with nothing to complain about") && ok;
	ok = Says(tableOneShort.errorCode == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a colour table one byte short is refused, and the file says why") && ok;
	ok = Says(wholeTable.made && wholeTable.errorCode == 0, "a whole colour table is made, with nothing to complain about") && ok;

	return ok;
}

// A waypoint's module is a name and then a number, and it comes after 14 numbers, which is more than the name patterns above have. So this row lays one out by hand: one waypoint, with no
// links, and one module whose name ends at the chunk's very last byte. That leaves nowhere to read the module's number from.
static bool WaypointModuleNameTakesAllTheRoom()
{
	Kind const waypoint = { "WAYPOINT", { 0, 0, 0 }, "Module_Waypoint_Chunk" };

	size_t const modulesAt = sizeof(int) + 44 + sizeof(int);   // the chunk's count, a waypoint's own numbers, and its count of links
	size_t const nameAt = modulesAt + sizeof(int);
	size_t const nameBytes = 16;
	int const one = 1;

	char * const start = PutChunkAtTheWall(waypoint, nameAt + nameBytes, Zeros);
	char * const data = start + ChunkHeaderBytes;

	memcpy(data, &one, sizeof(one));
	memcpy(data + modulesAt, &one, sizeof(one));
	memset(data + nameAt, 'x', nameBytes - 1);

	File_Chunk * const file = new File_Chunk;
	Parent_File = file;

	file->DynCreate(start);

	int const errorCode = file->error_code;

	delete file;
	Parent_File = 0;

	return Says(errorCode == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a waypoint's module name that takes all the room leaves its number unread, and the file says why");
}

// The one read everything after a name or a list goes through. The chunk here owns the first 8 bytes, and what's after them isn't its own.
static bool ReadingStaysInside()
{
	File_Chunk * const file = new File_Chunk;
	Parent_File = file;

	int const numbers[4] = { 0, 9, 123, 123 };
	char const * const start = (char const *)numbers;
	size_t const size = 8;
	char const * at = start + 4;
	bool ok = true;

	int const fits = ReadFromChunk<int>(start, size, at);
	ok = Says(fits == 9 && at == start + 8 && file->error_code == 0, "a number with room for it is read, and the place moves on past it") && ok;

	at = start + 5;
	int const tooFarAlong = ReadFromChunk<int>(start, size, at);
	ok = Says(tooFarAlong == 0 && at == start + 5 && file->error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a number one byte too far along isn't read, and the file says why") && ok;

	// The padding after a name can leave the place past the end already, by up to 3 bytes.
	file->error_code = 0;
	at = start + 9;
	int const pastTheEnd = ReadFromChunk<int>(start, size, at);
	ok = Says(pastTheEnd == 0 && at == start + 9 && file->error_code == CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED, "a place already past the end isn't read from, and the file says why") && ok;

	delete file;
	Parent_File = 0;

	return ok;
}

int main(int argc, char ** argv)
{
	// A stop being caught here shouldn't put a box on the screen.
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

	MakeTheWall();

	int const kindCount = (int)(sizeof(kinds) / sizeof(kinds[0]));

	// Given a number from the table and a number from the list of patterns, it tries only that. It's for finding every stop, and not just the first: run it once for each.
	if (argc == 3)
	{
		int const kind = atoi(argv[1]);
		int const pattern = atoi(argv[2]);

		if (kind < 0 || kind >= kindCount || pattern < 0 || pattern >= HowManyPatterns)
		{
			puts("There's no such kind or pattern");
			return 2;
		}

		bool made = false;

		if (!TryOnePattern(kinds[kind], pattern, &made))
		{
			return 1;
		}

		printf("  ok      %s (%s) with ", kinds[kind].id, kinds[kind].className);
		SayPattern(pattern);
		puts(made ? "" : ", though it was never made with these");
		return 0;
	}

	if (!MapBlockFindsItsInteriorType())
	{
		puts("  FAILED  a map block didn't find its interior type where it's written");
		puts("The chunk room test FAILED");
		return 1;
	}

	puts("  ok      a map block finds its interior type where it's written");

	if (!RefusalsSayWhy() || !ReadingStaysInside() || !WaypointModuleNameTakesAllTheRoom())
	{
		puts("The chunk room test FAILED");
		return 1;
	}

	for (int i = 0; i < kindCount; i++)
	{
		bool everMade = false;

		for (int pattern = 0; pattern < HowManyPatterns; pattern++)
		{
			// The first stop ends the test. Stopping halfway through making a chunk leaves the game's count of chunks being made too high, and past its limit it stops making
			// real chunks, so nothing tried after a stop could be believed.
			if (!TryOnePattern(kinds[i], pattern, &everMade))
			{
				puts("The chunk room test FAILED");
				return 1;
			}
		}

		if (!everMade && !GoneBeforeAnyoneCanLook(kinds[i]))
		{
			printf("  FAILED  %s (%s) was never made, at any size, so this proves nothing about it\n", kinds[i].id, kinds[i].className);
			puts("The chunk room test FAILED");
			return 1;
		}

		printf("  ok      %s (%s) stayed inside itself\n", kinds[i].id, kinds[i].className);
		fflush(stdout);
	}

	printf("The chunk room test passed: %d kinds tried\n", kindCount);
	return 0;
}
