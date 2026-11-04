// Modified 2026-11-04 by Melanikus for the AvP port:
//   a path's list of points is measured against its chunk before it's made

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
	int name_length=strlen(data);
	PathName=new char[name_length+1];
	strcpy(PathName,data);
	data+=(name_length+4)&~3;
	
	PathID=*(int*)data;
	data+=4;
	flags=*(int*)data;
	data+=4;
	spare2=*(int*)data;
	data+=4;
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
