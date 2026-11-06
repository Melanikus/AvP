// Modified 2026-11-04 by Melanikus for the AvP port:
//   says how many numbers a path can have

#ifndef pathchnk_hpp
#define pathchnk_hpp 1

#include "chunk.hpp"
#include "chunk_type.hpp"
#include "chunk_object.hpp"

#define PathPointFlag_InterpolatedPoint 0x00000001
#define PathPointFlag_LocationValid     0x00000002

struct ChunkPathPoint
{
	int module_index;
	ChunkVectorInt location; //relative to world
	int flags,spare2;
};


#define PathFlag_LoopPath 0x00000001
#define PathFlag_BackAndForth 0x00000002

// A path's number is its place in the game's list of paths, and a switch can only name a path up to 255.
enum { MostPathNumbers = 256 };
 
class AVP_Path_Chunk : public Chunk
{
	public :
	AVP_Path_Chunk(Chunk_With_Children*,const char *,size_t);
	AVP_Path_Chunk(Chunk_With_Children*);
	~AVP_Path_Chunk();

	virtual size_t size_chunk();
	virtual void fill_data_block(char* data_start);

	int PathLength;
	ChunkPathPoint* Path;
	
	char* PathName;
	int PathID;
	int flags,spare2;	


};

#endif