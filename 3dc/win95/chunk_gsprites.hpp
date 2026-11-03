// Modified 2026-11-03 by Melanikus for the AvP port:
//   deleted the AllSprites_Header_Chunk constructor that builds the chunk from its parent (level-editor code: the game itself never calls it)
//   deleted the declaration of unused RIF-authoring code

#ifndef _included_gsprchnk_hpp_
#define _included_gsprchnk_hpp_

#include "chunk.hpp"
#include "mishchnk.hpp"

class AllSprites_Header_Chunk;

class AllSprites_Chunk : public Lockable_Chunk_With_Children
{
public:

	// constructor from buffer
	AllSprites_Chunk (Chunk_With_Children * const parent,const char *, size_t const);

	AllSprites_Header_Chunk * get_header();

	// functions for the locking functionality

	BOOL file_equals (HANDLE &);
	const char * get_head_id();
	void set_lock_user(char *);
	
	void post_input_processing();

private:

	friend class File_Chunk;
	friend class GodFather_Chunk;




};

///////////////////////////////////////////////

class AllSprites_Header_Chunk : public Chunk
{
public:
	// constructor from buffer
	AllSprites_Header_Chunk (Chunk_With_Children * parent, const char * pdata, size_t psize);

	virtual size_t size_chunk ()
	{
		chunk_size = 36;
		return chunk_size;
	}

	virtual BOOL output_chunk (HANDLE &);

	virtual void fill_data_block (char * data_start);

	void prepare_for_output();

private:

	friend class AllSprites_Chunk;
	friend class File_Chunk;

	int flags;

	int version_no;

	char lock_user[17];
	

	
	
};
	



#endif // _included_gsprchnk_hpp_