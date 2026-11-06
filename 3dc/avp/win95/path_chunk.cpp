// AvP port by Barry Duncan.
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a path's list of points is measured against its chunk before it's made
//   a path's number has to be a place in the list of paths
//   a name is only read as far as the end of its chunk
//   what comes after a name or a list is only read while it's still inside the chunk

#include "path_chunk.hpp"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(path_chunk)

RIF_IMPLEMENT_DYNCREATE("AVPPATH2",AVP_Path_Chunk)

AVP_Path_Chunk::AVP_Path_Chunk(Chunk_With_Children* parent)
:Chunk(parent,"AVPPATH2")
{
	PathLength=0;
	Path=0;
	PathID=0;
	PathName=0;
	flags=spare2=0;
}

AVP_Path_Chunk::AVP_Path_Chunk(Chunk_With_Children* parent,const char* data,size_t datasize)
:Chunk(parent,"AVPPATH2")
{
	char const * const start = data;
	size_t name_length=0;
	PathName=CopyNameFromChunk(start,datasize,data,&name_length);
	data+=(name_length+4)&~3;
	
	PathID=ReadFromChunk<int>(start,datasize,data);

	// This is used as a place in the list of paths, so a negative one would write in front of the list, and a bigger one would only ask for a bigger list.
	if(PathID<0 || PathID>=MostPathNumbers)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}

	flags=ReadFromChunk<int>(start,datasize,data);
	spare2=ReadFromChunk<int>(start,datasize,data);
	PathLength=ReadListCount(start,data,datasize,sizeof(ChunkPathPoint));

	if(PathLength) Path=new ChunkPathPoint[PathLength];
	else Path=0;
	
	for(int i=0;i<PathLength;i++)
	{
		Path[i]=*(ChunkPathPoint*)data;
		data+=sizeof(ChunkPathPoint);
	}		
}


AVP_Path_Chunk::~AVP_Path_Chunk()
{
	delete[] PathName;
	delete[] Path;
}

void AVP_Path_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;
		
	strcpy(data_start,PathName);
	data_start+=(strlen(PathName)+4)&~3;
	
	*(int*)data_start=PathID;
	data_start+=4;
	*(int*)data_start=flags;
	data_start+=4;
	*(int*)data_start=spare2;
	data_start+=4;
	*(int*)data_start=PathLength;
	data_start+=4;

	for(int i=0;i<PathLength;i++)
	{
		*(ChunkPathPoint*)data_start=Path[i];
		data_start+=sizeof(ChunkPathPoint);
	}
}

size_t AVP_Path_Chunk::size_chunk()
{
	chunk_size=12+16;
	chunk_size+=(strlen(PathName)+4)&~3;
	chunk_size+=sizeof(ChunkPathPoint)*PathLength;
	return chunk_size;
}
