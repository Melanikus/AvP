// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed the declarations of code only the level editor used: the game itself never runs it; added a
//   check on the sound numbers a level file gives
//   a chunk for collections of sounds, which was only ever a comment, is gone

#ifndef _sndchunk_hpp
#define _sndchunk_hpp 1
#include "chunk_type.hpp"

#define SoundObjectFlag_NotPlayingAtStart 0x00000001
#define SoundObjectFlag_NoLoop			  0x00000002
	 
class Object_Alternate_Locations_Chunk;
class Sound_Object_Extra_Data_Chunk;

class Sound_Object_Chunk : public Chunk
{
public:
	
	Sound_Object_Chunk (Chunk_With_Children * parent, 
											ChunkVectorInt & pos,
											const char * name
											);
	// constructor from buffer
	Sound_Object_Chunk (Chunk_With_Children * parent, const char * sdata, size_t /*ssize*/);

	~Sound_Object_Chunk ();
	
	ChunkVectorInt position;

	unsigned long inner_range; //millimetres
	unsigned long outer_range; //millimetres
	int max_volume; //from 0 to 127
	int	pitch;	   //pitch shift in 1/1536ths of an actave
	
	int flags;
	int probability;
	int pad3;
	
	char * snd_name;
	char * wav_name;
	
	size_t size_chunk ()
	{
		chunk_size = 12 + 3*4 + 7*4 + strlen(snd_name) + 1;
		if (wav_name)
		{
			chunk_size += strlen (wav_name);
		}
		else
		{
			chunk_size += 1;
		}
		
		chunk_size +=  4 - chunk_size%4;
		return (chunk_size);
	}
	
	void fill_data_block (char *);
	
	ObjectID CalculateID();

	Sound_Object_Extra_Data_Chunk* get_extra_data_chunk();

	Object_Alternate_Locations_Chunk* get_alternate_locations_chunk();

private:

	friend class Special_Objects_Chunk;


	
};

//For attaching extra data to the sound objects
class Sound_Object_Extra_Data_Chunk : public Chunk_With_Children
{
public:
	Sound_Object_Extra_Data_Chunk (Chunk_With_Children * const parent,const char *, size_t const);
	
	Sound_Object_Chunk* get_sound_chunk();
};
	
//Needed so I can match the extra data chunk with the appropriate sound chunk
class Sound_Object_Extra_Name_Chunk : public Chunk
{
public :	
	Sound_Object_Extra_Name_Chunk(Chunk_With_Children* parent,const char*,size_t);
	~Sound_Object_Extra_Name_Chunk();
	
	char* name;

	
	virtual size_t size_chunk();
	virtual void fill_data_block(char* data_start);
};



#define IndexedSoundFlag_Loop 0x00000001
class Indexed_Sound_Chunk : public Chunk
{
public :

	Indexed_Sound_Chunk(Chunk_With_Children* parent,const char*,const size_t);
	Indexed_Sound_Chunk(Chunk_With_Children* parent);
	~Indexed_Sound_Chunk();

	size_t size_chunk();
	void fill_data_block(char*);

	int index;
	char* wav_name;
	unsigned long inner_range; //millimetres
	unsigned long outer_range; //millimetres
	int max_volume; //from 0 to 127
	int	pitch; //pitch shift in 1/1536ths of an actave
	int flags;
	
	int num_extra_data;
	int* extra_data;

	// The game keeps a list of sounds, just long enough for the highest number it finds, and uses each sound's number
	// as its place in that list. The number comes straight from the level file, so it could be anything. A negative
	// number would point in front of the list, and a huge one would need more memory than the size sum can count,
	// which leaves a list far too short. Numbers in the game's own files run from 0 to 39, so this limit leaves plenty
	// of room. Anything using index as a place in a list must skip a sound that fails this check.
	static const int Max_Sound_Number = 4095;
	bool index_is_valid() const { return index >= 0 && index <= Max_Sound_Number; }
};

#endif
