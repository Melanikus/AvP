// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed code only the level editor used: the game itself never runs it
//   a name is only read as far as the end of its chunk
//   a chunk too small for what its kind always reads is kept as plain bytes, and the level is refused

#include "chunk_dummy.hpp"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(chunk_dummy)


RIF_IMPLEMENT_DYNCREATE("DUMMYOBJ",Dummy_Object_Chunk)

CHUNK_WITH_CHILDREN_LOADER("DUMMYOBJ",Dummy_Object_Chunk)
/*
Children for Dummy_Object_Chunk :

"DUMOBJDT"	Dummy_Object_Data_Chunk
"DUMOBJTX"	Dummy_Object_Text_Chunk
*/



Dummy_Object_Data_Chunk* Dummy_Object_Chunk::get_data_chunk()
{
	return (Dummy_Object_Data_Chunk*) lookup_single_child("DUMOBJDT");
}


const char* Dummy_Object_Chunk::get_text()
{
	//find the text
	Dummy_Object_Text_Chunk* text_chunk = (Dummy_Object_Text_Chunk*) lookup_single_child("DUMOBJTX");
	if(!text_chunk)
	{
		//no text
		return NULL;
	}
	
	return text_chunk->get_text();
}


//////////////////////////////Dummy_Object_Data_Chunk////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("DUMOBJDT",Dummy_Object_Data_Chunk,52)


Dummy_Object_Data_Chunk::Dummy_Object_Data_Chunk(Chunk_With_Children* parent,const char* data,size_t const chunk_bytes)
:Chunk(parent,"DUMOBJDT")
{
	char const * const chunk_start = data;
	CHUNK_EXTRACT(location,ChunkVectorInt)
	CHUNK_EXTRACT(min_extents,ChunkVectorInt)
	CHUNK_EXTRACT(max_extents,ChunkVectorInt)
	CHUNK_EXTRACT(orientation,ChunkQuat)
	CHUNK_EXTRACT_STRING(name,chunk_start,chunk_bytes)
}


Dummy_Object_Data_Chunk::~Dummy_Object_Data_Chunk()
{
	delete[] name;
}


void Dummy_Object_Data_Chunk::fill_data_block(char* data)
{
	CHUNK_FILL_START
	
	CHUNK_FILL(location,ChunkVectorInt)
	CHUNK_FILL(min_extents,ChunkVectorInt)
	CHUNK_FILL(max_extents,ChunkVectorInt)
	CHUNK_FILL(orientation,ChunkQuat)
	CHUNK_FILL_STRING(name)

}


size_t Dummy_Object_Data_Chunk::size_chunk()
{
	chunk_size=12+3*sizeof(ChunkVectorInt)+sizeof(ChunkQuat);
	chunk_size+=(strlen(name)+4)&~3;
	return chunk_size;
}

/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////Dummy_Object_Text_Chunk////////////////////////////////

RIF_IMPLEMENT_DYNCREATE("DUMOBJTX",Dummy_Object_Text_Chunk)

Dummy_Object_Text_Chunk::Dummy_Object_Text_Chunk(Chunk_With_Children* parent,const char* data,size_t const chunk_bytes)
:Chunk(parent,"DUMOBJTX")
{
	CHUNK_EXTRACT_STRING(text,data,chunk_bytes)
}


Dummy_Object_Text_Chunk::~Dummy_Object_Text_Chunk()
{
	delete[] text;
}

void Dummy_Object_Text_Chunk::fill_data_block(char* data)
{
	CHUNK_FILL_START
	
	CHUNK_FILL_STRING(text)
}

void Dummy_Object_Text_Chunk::set_text(const char* _text)
{
	char* new_text = NULL;
	if(_text)
	{
		new_text = new char[strlen(_text)+1];
		strcpy(new_text,_text);
	}

	delete[] text;
	text = new_text;
}


size_t Dummy_Object_Text_Chunk::size_chunk()
{
	chunk_size=12;
	if(text)
		chunk_size+=(strlen(text)+4)&~3;
	else
		chunk_size+=4;
	return chunk_size;
}
