// Modified 2026-11-03 by Melanikus for the AvP port:
//   deleted the declaration of the Fragment_Type_Data_Chunk constructor that builds the chunk from a name (level-editor code: the game itself never calls it)
//   deleted unused RIF-authoring code (the Fragment_Type_Chunk name constructor)

#ifndef _fragchnk_hpp_
#define _fragchnk_hpp_

#include "chunk.hpp"
#include "chunk_type.hpp"


class Fragment_Type_Data_Chunk : public Chunk
{
public :
	
	Fragment_Type_Data_Chunk (Chunk_With_Children * const parent,const char *, size_t const);
	~Fragment_Type_Data_Chunk();
	
	size_t size_chunk ();
	void fill_data_block (char * data_start);
	

	char* frag_type_name;
	int pad1,pad2,pad3;

};

class Fragment_Type_Shape_Chunk : public Chunk
{
public:	
	
	Fragment_Type_Shape_Chunk(Chunk_With_Children* parent,const char* _name,int number,ChunkVectorInt _location);
	Fragment_Type_Shape_Chunk (Chunk_With_Children * const parent,const char *, size_t const);
	~Fragment_Type_Shape_Chunk();
	
	size_t size_chunk ();
	void fill_data_block (char * data_start);
	
	
	int num_fragments;
	ChunkVectorInt location;
	char* name;
	
	int pad1,pad2,pad3;	

};
class Fragment_Type_Sound_Chunk : public Chunk
{
public:	
	
	Fragment_Type_Sound_Chunk(Chunk_With_Children* parent);
	Fragment_Type_Sound_Chunk (Chunk_With_Children * const parent,const char *, size_t const);
	~Fragment_Type_Sound_Chunk();
	
	size_t size_chunk ();
	void fill_data_block (char * data_start);
	
	char* wav_name;
	unsigned long inner_range;
	unsigned long outer_range;
	int max_volume;
	int	pitch;
	int pad;	

};

class Fragment_Type_Chunk : public Chunk_With_Children
{
public :
	Fragment_Type_Chunk (Chunk_With_Children * const parent,const char *, size_t const);

	const char* get_name();

};






#endif