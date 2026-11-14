// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed code only the level editor used: the game itself never runs it;
//   text is copied using its exact measured length, so it always fits
//   counted lists are measured against their chunks before they're made
//   a level's paths each need a number of their own
//   a lift in another level file is no longer one of its children
//   a name is only read as far as the end of its chunk
//   the code that locked chunks inside a level file on disk is gone: nothing could reach it, and it read names onto the stack with no limit
//   a chunk too small for what its kind always reads is kept as plain bytes, and the level is refused
//   what comes after a name or a list is only read while it's still inside the chunk
//   a level's scale has to be from 0.5 to 1
//   a level's amount of echo has to be a real number
//   a set of random textures has to have a name

#include "chunk.hpp"
#include "environment_chunk.hpp"
#include "real_numbers.h"
#include "path_chunk.hpp"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(environment_chunk)

// Class Environment_Data_Chunk functions
RIF_IMPLEMENT_DYNCREATE("REBENVDT",Environment_Data_Chunk)

// constructor from buffer
LOCKABLE_CHUNK_WITH_CHILDREN_LOADER("REBENVDT",Environment_Data_Chunk)
/*
Children for Enviornment_Data_Chunk :

"ENVSDSCL"		Environment_Scale_Chunk
"GAMEMODE"		Environment_Game_Mode_Chunk
"ENVPALET"		Environment_Palette_Chunk
"ENVTXLIT"		Environment_TLT_Chunk
"TLTCONFG"		TLT_Config_Chunk
"CLRLOOKP"		Coloured_Polygons_Lookup_Chunk
"MATCHIMG"		Matching_Images_Chunk
"BMPNAMES"		Global_BMP_Name_Chunk
"BMNAMVER"		BMP_Names_Version_Chunk
"BMNAMEXT"		BMP_Names_ExtraData_Chunk
"RIFFNAME"		RIF_Name_Chunk
"ENDTHEAD"		Environment_Data_Header_Chunk
"LIGHTSET"		Light_Set_Chunk
"PRSETPAL"		Preset_Palette_Chunk
"SPECLOBJ"		Special_Objects_Chunk
"BMPMD5ID"		Bitmap_MD5_Chunk
"GLOGENDC"		Global_Generator_Data_Chunk
"FRAGTYPE"		Fragment_Type_Chunk
"ENVACOUS"		Environment_Acoustics_Chunk
"AVPENVIR"		AVP_Environment_Settings_Chunk
"SOUNDDIR"		Sound_Directory_Chunk
"RANTEXID"		Random_Texture_ID_Chunk
*/




	

Environment_Data_Header_Chunk * Environment_Data_Chunk::get_header()
{
	return (Environment_Data_Header_Chunk *) this->lookup_single_child ("ENDTHEAD");
}	

	

void Environment_Data_Chunk::set_lock_user (char * user)
{
	Environment_Data_Header_Chunk * hdptr = get_header();

	if (!hdptr) return;

	strncpy (hdptr->lock_user, user,16);

	hdptr->lock_user[16] = 0;
}
	
void Environment_Data_Chunk::post_input_processing()
{
	if (get_header())
		if (get_header()->flags & GENERAL_FLAG_LOCKED)
			external_lock = TRUE;

	Chunk_With_Children::post_input_processing();
}

///////////////////////////////////////

// Class Environment_Data_Header_Chunk functions
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("ENDTHEAD",Environment_Data_Header_Chunk,24)

// from buffer
Environment_Data_Header_Chunk::Environment_Data_Header_Chunk (Chunk_With_Children * parent, const char * hdata, size_t /*hsize*/)
	: Chunk (parent, "ENDTHEAD"),
	flags (0), version_no (0)
{
	flags = *((int *) hdata);

	strncpy (lock_user, (hdata + 4), 16);
	lock_user[16] = '\0';

	version_no = *((int *) (hdata + 20));
}	

BOOL Environment_Data_Header_Chunk::output_chunk (HANDLE & hand)
{
	unsigned long junk;
	BOOL ok;
	char * data_block;

	data_block = make_data_block_from_chunk();

	ok = WriteFile (hand, (long *) data_block, (unsigned long) chunk_size, &junk, 0);

	delete[] data_block;

	if (!ok) return FALSE;

	return TRUE;
}

void Environment_Data_Header_Chunk::fill_data_block ( char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = flags;
	strncpy ((data_start + 4), lock_user, 16);

	*((int *) (data_start+20)) = version_no;
}

void Environment_Data_Header_Chunk::prepare_for_output()
{
	version_no ++;
}

///////////////////////////////////////

// Class Environment_Scale_Chunk functions
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("ENVSDSCL",Environment_Scale_Chunk,8)

// A level's scale is a number from the file, with a fraction, that the game multiplies every place, size and distance in the level by as it
// loads it, turning each answer back into a whole number (projload.cpp's set_local_scale, then object_setup.cpp and chunk_load.cpp).
// It has to be from 0.5 to 1, or the level is refused:
// - at 1 or less, any whole number the game multiplies by it is still a whole number it can hold;
// - the game's own levels that have a scale use 0.8 or 0.9, and the rest have none, which is 1, so half leaves room below them;
// - a number that isn't a number (NaN), or is endless, is refused too.
// (Added 2026-11-14 by Melanikus for the AvP port.)
Environment_Scale_Chunk::Environment_Scale_Chunk (Chunk_With_Children * parent, const char * sdata, size_t /*ssize*/)
: Chunk (parent, "ENVSDSCL"),
scale ( *((double *) sdata) )
{
	double const smallest_scale = 0.5;
	double const largest_scale = 1.0;

	// The 11 bits after a double's sign are its power of two, and all 11 set means it's NaN or endless. They're looked at as bits, because the
	// game is built with the compiler's fast rules for fractions (/fp:fast), under which a comparison isn't promised to treat NaN as it should.
	unsigned long long bits = 0;
	memcpy(&bits, &scale, sizeof(bits));
	bool const not_a_number_or_endless = ((bits >> 52) & 0x7FF) == 0x7FF;

	if (not_a_number_or_endless || scale < smallest_scale || scale > largest_scale)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}
}

void Environment_Scale_Chunk::fill_data_block ( char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((double *) data_start) = scale;
}

///////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("ENVACOUS",Environment_Acoustics_Chunk,12)

Environment_Acoustics_Chunk::Environment_Acoustics_Chunk(Environment_Data_Chunk* parent)
:Chunk(parent,"ENVACOUS")
{
	env_index=0; 
	reverb=2;
	spare=0;
}

Environment_Acoustics_Chunk::Environment_Acoustics_Chunk(Chunk_With_Children* parent,const char* data,size_t)
:Chunk(parent,"ENVACOUS")
{
	env_index=*(int*)data;
	data+=4;
	reverb=*(float*)data;
	data+=4;
	spare=*(int*)data;

	// The level's amount of echo has to be a real number: the game compares it to choose what it means, so each answer has to be one
	// the comparison can give. Told from its bits (real_numbers.h). (Added 2026-11-14 by Melanikus for the AvP port.)
	if (!IsRealNumber(reverb))
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}
}

void Environment_Acoustics_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=env_index;
	data_start+=4;
	*(float*)data_start=reverb;
	data_start+=4;
	*(int*)data_start=spare;
}
///////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE("SOUNDDIR",Sound_Directory_Chunk)

Sound_Directory_Chunk::Sound_Directory_Chunk(Environment_Data_Chunk* parent,const char* dir)
:Chunk(parent,"SOUNDDIR")
{
	directory=new char[strlen(dir)+1];
	strcpy(directory,dir);
}

Sound_Directory_Chunk::Sound_Directory_Chunk(Chunk_With_Children * const parent, const char* data, size_t const chunk_bytes)
:Chunk(parent,"SOUNDDIR")
{
	directory=CopyNameFromChunk(data,chunk_bytes,data);
}

Sound_Directory_Chunk::~Sound_Directory_Chunk()
{
	delete[] directory;
}

void Sound_Directory_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;

	memcpy(data_start,directory,strlen(directory)+1);
}


///////////////////////////////////////
/////////////////////Available shape set collections////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("RANTEXID",Random_Texture_ID_Chunk)

Random_Texture_ID_Chunk::Random_Texture_ID_Chunk(Chunk_With_Children* parent,const char* _name)
:Chunk(parent,"RANTEXID")
{
	name=new char[strlen(_name)+1];
	strcpy(name,_name);
	spare1=spare2=0;
}


Random_Texture_ID_Chunk::Random_Texture_ID_Chunk(Chunk_With_Children* parent,const char* data,size_t size)
:Chunk(parent,"RANTEXID")
{
	char const * const start = data;
	CHUNK_EXTRACT_STRING(name,start,size);

	// A set of random textures is found by its name, the kind of NPC it's for, so one with no name is refused. Every one in the game's own
	// levels has a name. (Added 2026-11-14 by Melanikus for the AvP port.)
	if (!name)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}
	
	int type;
	int const num_types=ReadListCount(start,data,size,sizeof(int));
	for(int i=0;i<num_types;i++)
	{
		CHUNK_EXTRACT(type,int);
		random_types.add_entry(type);
	}
	spare1=ReadFromChunk<int>(start,size,data);
	spare2=ReadFromChunk<int>(start,size,data);
}

Random_Texture_ID_Chunk::~Random_Texture_ID_Chunk()
{
	delete[] name;
}

void Random_Texture_ID_Chunk::fill_data_block(char* data)
{
	CHUNK_FILL_START

	CHUNK_FILL_STRING(name)

	CHUNK_FILL(random_types.size(),int);
	for(LIF<int> rlif(&random_types);!rlif.done();rlif.next())
	{
		CHUNK_FILL(rlif(),int)
	}

	CHUNK_FILL(spare1,int)
	CHUNK_FILL(spare2,int)
}

size_t Random_Texture_ID_Chunk::size_chunk()
{
	chunk_size=12+12+random_types.size()*4;
	chunk_size+=(strlen(name)+4)&~3;
	return chunk_size;
}
///////////////////////////////////////




//Class Special_Objects_Chunk :
RIF_IMPLEMENT_DYNCREATE("SPECLOBJ",Special_Objects_Chunk)

CHUNK_WITH_CHILDREN_LOADER("SPECLOBJ",Special_Objects_Chunk)

// A path's number is its place in the game's list of paths. Two paths with one number would share that place, and the second one's shorter list would then be
// filled to the length of the first. So each number can only be used once. A number outside the list has refused the level already: see AVP_Path_Chunk.
void Special_Objects_Chunk::post_input_processing()
{
	bool number_taken[MostPathNumbers] = { false };

	List<Chunk *> paths;
	lookup_child ("AVPPATH2", paths);

	for (LIF<Chunk *> path (&paths); !path.done(); path.next())
	{
		int const number = path()->r_u_miscellaneous() ? -1 : ((AVP_Path_Chunk *) path())->PathID;

		if (number < 0 || number >= MostPathNumbers)
		{
			continue;
		}

		if (number_taken[number])
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		}

		number_taken[number] = true;
	}

	Chunk_With_Children::post_input_processing();
}
/*
Children for Special_Objects_Chunk :
"AVPGENER"		AVP_Generator_Chunk
"SOUNDOB2"		Sound_Object_Chunk
"VIOBJECT"		Virtual_Object_Chunk
"AVPCABLE"		AVP_Power_Cable_Chunk
"AVPSTART"		AVP_Player_Start_Chunk
"AVPPATH2"		AVP_Path_Chunk
"AVPGENEX"		AVP_Generator_Extra_Data_Chunk
"SOUNDEXD"		Sound_Object_Extra_Data_Chunk
"PARGENER"		AVP_Particle_Generator_Chunk
"PLACHIER"		Placed_Hierarchy_Chunk
"CAMORIGN"		Camera_Origin_Chunk
"AVPDECAL"		AVP_Decal_Chunk
"R6WAYPNT"		R6_Waypoint_Chunk

*/


