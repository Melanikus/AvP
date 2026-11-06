// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   names saved into level data are copied, and stepped past, by the length measured from the name
//   itself, and unused code for making new level files was removed;
//   a placed hierarchy's name and lists are checked against the size of their chunk before they're read
//   a placed hierarchy checks what kind of strategy it holds
//   a placed hierarchy's name is copied by the length measured from the chunk, and the way of making one in code, which nothing used, is gone
#include "hierplace.hpp"
#include "strategy_chunk.hpp"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(hierplace)

////////////////////////////////////////////////
//Class Placed_Hiearchy_Chunk


RIF_IMPLEMENT_DYNCREATE("PLACHIER",Placed_Hierarchy_Chunk)

CHUNK_WITH_CHILDREN_LOADER("PLACHIER",Placed_Hierarchy_Chunk)

// The game reads a placed hierarchy's strategy as the hierarchy kind, whatever it says it is. Any other kind has different numbers in those places, and
// they'd be used as a list of pointers, straight from the file. So any other kind refuses the level.
void Placed_Hierarchy_Chunk::post_input_processing()
{
	Chunk * const strategy = lookup_single_child ("AVPSTRAT");

	if (strategy && !strategy->r_u_miscellaneous())
	{
		AvpStrat const * const kept = ((AVP_Strategy_Chunk *) strategy)->Strategy;

		if (kept && kept->StrategyType != StratHierarchy)
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		}
	}

	Chunk_With_Children::post_input_processing();
}
/*
Children for Placed_Hiearchy_Chunk :

"PLACHIDT"		Placed_Hierarchy_Data_Chunk
"PLHISEQU"		Placed_Hierarchy_Sequence_Chunk
"INDSOUND"		Indexed_Sound_Chunk
"AVPSTRAT"		AVP_Strategy_Chunk
*/



///////////////////////////////////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("PLACHIDT",Placed_Hierarchy_Data_Chunk)

// A name, some numbers, then a list of extra numbers with its own count. The count comes from the file and sizes a list, so the whole chunk is
// measured before any of it is read: asking for more memory than there is ends the game, and a count too big reads past the chunk. All 3 of
// these in the game's own levels end exactly at the end of their chunk, so that's the rule.
Placed_Hierarchy_Data_Chunk::Placed_Hierarchy_Data_Chunk(Chunk_With_Children* parent, const char* data, const size_t chunk_bytes)
:Chunk(parent,"PLACHIDT")
{
	size_t const numbers_bytes = sizeof(int) + sizeof(ChunkVectorInt) + sizeof(ChunkQuat) + sizeof(ObjectID) + sizeof(int);

	// whatever happens next, these are safe for the destructor
	name = 0;
	num_extra_data = 0;
	extra_data = 0;

	int const name_length = NameLengthInChunk(data, chunk_bytes);

	if (name_length < 0)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	size_t const length = (size_t)name_length;
	size_t const name_bytes = (length + 4) & ~(size_t)3;

	if (chunk_bytes < name_bytes + numbers_bytes)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	int const extra_count = *(int *)(data + name_bytes + numbers_bytes - sizeof(int));
	size_t const bytes_left = chunk_bytes - name_bytes - numbers_bytes;

	if (!CountFitsChunk(extra_count, sizeof(int), bytes_left) || (size_t)extra_count * sizeof(int) != bytes_left)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	if (length)
	{
		// NameLengthInChunk above found this name's end marker inside the chunk, so this many bytes are there to copy, and the new list has room for them
		name = new char[length+1];
		memcpy(name, data, length);
		name[length] = 0;
	}
	else
	{
		name = 0;
	}
	data += (length+4)&~3;

	hierarchy_index = *(int*)data;
	data += 4;

	location = *(ChunkVectorInt*)data;
	data += sizeof(ChunkVectorInt);

	orientation = *(ChunkQuat*)data;
	data += sizeof(ChunkQuat);

	id = *(ObjectID*)data;
	data += sizeof(ObjectID);

	num_extra_data = extra_count;
	data += 4;

	if (num_extra_data)
	{
		extra_data = new int[num_extra_data];
		for (int i = 0; i < num_extra_data; i++)
		{
			extra_data[i] = *(int*)data;
			data += 4;
		}
	}
	else
	{
		extra_data = 0;
	}
}

Placed_Hierarchy_Data_Chunk::~Placed_Hierarchy_Data_Chunk()
{
	delete[] extra_data;
	delete[] name;
}

void Placed_Hierarchy_Data_Chunk::fill_data_block(char* data)
{
	strncpy (data, identifier, 8);
	data += 8;
	*((int *) data) = chunk_size;
	data += 4;

	// The name is stored followed by its end marker (the zero byte that marks
	// where a text stops), then the write position moves on by a whole number
	// of 4-byte steps. With no name, 4 zero bytes are stored. The name is
	// measured once, here, and that length is used both to copy it and to step
	// past it, instead of measuring again what was just written.
	size_t const nameLength = name ? strlen(name) : 0;
	if(name)
	{
		memcpy(data,name,nameLength+1);
	}
	else
	{
		*(int*)data=0;
	}
	data+=(nameLength+4)&~3;

	*(int*)data=hierarchy_index;
	data+=4;

	*(ChunkVectorInt*)data=location;
	data+=sizeof(ChunkVectorInt);

	*(ChunkQuat*) data=orientation;
	data+=sizeof(ChunkQuat);

	*(ObjectID*) data=id;
	data+=sizeof(ObjectID);

	*(int*)data=num_extra_data;
	data+=4;

	for(int i=0;i<num_extra_data;i++)
	{
		*(int*) data=extra_data[i];
		data+=4;
	}

}

size_t Placed_Hierarchy_Data_Chunk::size_chunk()
{
	chunk_size=12+4+sizeof(ChunkVectorInt)+sizeof(ChunkQuat)+sizeof(ObjectID);
	chunk_size+=4+4*num_extra_data;
	if(name)
		chunk_size+=(strlen(name)+4)&~3;
	else
		chunk_size+=4;

	return chunk_size;
}

/////////////////////////////////////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("PLHISEQU",Placed_Hierarchy_Sequence_Chunk)

// Five numbers, a list of sounds, then a list of extra numbers, each list with its own count. Both counts come from the file and size a list,
// so they're measured against the chunk before anything is made. All 3 of these in the game's own levels end exactly at the end of their
// chunk, so that's the rule.
Placed_Hierarchy_Sequence_Chunk::Placed_Hierarchy_Sequence_Chunk(Chunk_With_Children* parent,const char* data, const size_t chunk_bytes)
:Chunk(parent,"PLHISEQU")
{
	size_t const numbers_bytes = 5 * sizeof(int);
	size_t const count_bytes = sizeof(int);

	// whatever happens next, these are safe for the destructor
	sound_list_size = 0;
	sound_list = 0;
	num_extra_data = 0;
	extra_data = 0;

	if (chunk_bytes < numbers_bytes + 2 * count_bytes)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	int const sound_count = *(int *)(data + numbers_bytes);
	size_t const room_for_lists = chunk_bytes - numbers_bytes - 2 * count_bytes;

	if (!CountFitsChunk(sound_count, sizeof(Timed_Sound_Description), room_for_lists))
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	size_t const sound_bytes = (size_t)sound_count * sizeof(Timed_Sound_Description);
	int const extra_count = *(int *)(data + numbers_bytes + count_bytes + sound_bytes);
	size_t const bytes_left = room_for_lists - sound_bytes;

	if (!CountFitsChunk(extra_count, sizeof(int), bytes_left) || (size_t)extra_count * sizeof(int) != bytes_left)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	index=*(int*) data;
	data+=4;
	
	sequence=*(int*) data;
	data+=4;
	
	sub_sequence=*(int*)data;
	data+=4;
	
	time=*(int*)data ;
	data+=4;
	
	flags=*(int*)data ;
	data+=4;

	sound_list_size=sound_count;
	data+=4;

	if(sound_list_size)
	{
		sound_list=new Timed_Sound_Description[sound_list_size];
		for(int i=0;i<sound_list_size;i++)
		{
			sound_list[i]=*(Timed_Sound_Description*)data;
			data+=sizeof(Timed_Sound_Description);
		}
	}
	else
	{
		sound_list=0;
	}
		
	num_extra_data=extra_count;
	data+=4;

	if(num_extra_data)
	{
		extra_data=new int[num_extra_data];
		for(int i=0;i<num_extra_data;i++)
		{
			extra_data[i]=*(int*)data;
			data+=4;
		}
	}
	else
	{
		extra_data=0;
	}
	
}

Placed_Hierarchy_Sequence_Chunk::Placed_Hierarchy_Sequence_Chunk(Chunk_With_Children* parent,int _index)
:Chunk(parent,"PLHISEQU")
{
	index=_index;
	sequence=0;
	sub_sequence=0;
	time=1000;
	sound_list_size=0;
	sound_list=0;
	extra_data=0;
	num_extra_data=0;
	flags=0;


}

Placed_Hierarchy_Sequence_Chunk::~Placed_Hierarchy_Sequence_Chunk()
{
	delete[] extra_data;
	delete[] sound_list;	
}

void Placed_Hierarchy_Sequence_Chunk::fill_data_block(char* data)
{
	strncpy (data, identifier, 8);
	data += 8;
	*((int *) data) = chunk_size;
	data += 4;

	*(int*)data=index;
	data+=4;

	*(int*)data=sequence;
	data+=4;

	*(int*)data=sub_sequence;
	data+=4;

	*(int*)data=time;
	data+=4;

	*(int*)data=flags;
	data+=4;

	*(int*)data=sound_list_size;
	data+=4;

	for(int i=0;i<sound_list_size;i++)
	{
		*(Timed_Sound_Description*) data=sound_list[i];
		data+=sizeof(Timed_Sound_Description);
	}

	*(int*)data=num_extra_data;
	data+=4;

	for(int i=0;i<num_extra_data;i++)
	{
		*(int*) data=extra_data[i];
		data+=4;
	}
}

size_t Placed_Hierarchy_Sequence_Chunk::size_chunk()
{
	chunk_size=12+24;
	chunk_size+=sound_list_size*sizeof(Timed_Sound_Description);
	chunk_size+=4+4*num_extra_data;
	return chunk_size;
}

