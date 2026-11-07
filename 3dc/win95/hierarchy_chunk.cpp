// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   text is copied using its exact measured length, so it always fits; a level with a hierarchy section that
//   has no name is refused
//   counted lists are measured against their chunks before they're made
//   a name is only read as far as the end of its chunk
//   a chunk too small for what its kind always reads is kept as plain bytes, and the level is refused
//   what comes after a name or a list is only read while it's still inside the chunk

#include "hierarchy_chunk.hpp"
#include "chunk_object.hpp"
//#include "animation_objects.hpp"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(hierarchy_chunk)

/////////////Object_Hierarchy_Chunk////////////////////
RIF_IMPLEMENT_DYNCREATE("OBJCHIER",Object_Hierarchy_Chunk)

//loader for Object_Hierarchy_Chunk
CHUNK_WITH_CHILDREN_LOADER("OBJCHIER",Object_Hierarchy_Chunk)

/*
children for Object_Hierarchy_Chunk:

"OBJCHIER"		Object_Hierarchy_Chunk)
"OBJHIERD"		Object_Hierarchy_Data_Chunk)
"OBHIERNM"		Object_Hierarchy_Name_Chunk)
"HIERBBOX"		Hierarchy_Bounding_Box_Chunk)
"OBANSEQS"		Object_Animation_Sequences_Chunk)
"OBANALLS"		Object_Animation_All_Sequence_Chunk)

*/

List <Object_Hierarchy_Chunk *> Object_Hierarchy_Chunk::list_h_children()
{
	List <Chunk *> cl;
	lookup_child ("OBJCHIER",cl);
	List <Object_Hierarchy_Chunk *> h_l;
	
	for (LIF<Chunk *> cli(&cl); !cli.done(); cli.next())
	{
		h_l.add_entry((Object_Hierarchy_Chunk *)cli());
	}
	return(h_l);
	
}

Object_Hierarchy_Data_Chunk * Object_Hierarchy_Chunk::get_data ()
{
  	return((Object_Hierarchy_Data_Chunk *)lookup_single_child ("OBJHIERD"));
	
}

Object_Hierarchy_Name_Chunk * Object_Hierarchy_Chunk::get_name ()
{
 	return((Object_Hierarchy_Name_Chunk *)lookup_single_child ("OBHIERNM"));
}
//////////////////////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE("OBJHIERD",Object_Hierarchy_Data_Chunk)

Object_Hierarchy_Data_Chunk::Object_Hierarchy_Data_Chunk (Object_Hierarchy_Chunk * parent, const char * obname)
: Chunk (parent, "OBJHIERD"), object(0), ob_name (0)
{
	num_extra_data=0;
	extra_data=0;

	if (obname)
	{
		ob_name = new char [strlen(obname)+1];
		strcpy (ob_name, obname);

		Chunk_With_Children * root = GetRootChunk();
		
		List<Chunk *> oblist;
		root->lookup_child ("RBOBJECT",oblist);
		
		for (LIF<Chunk *> oli(&oblist); !oli.done(); oli.next())
		{
			Object_Chunk * ob = (Object_Chunk *)oli();
			
			if (!strcmp (ob->object_data.o_name, ob_name))
			{
				*((Object_Chunk **)&object) = ob;
				break;
			}
		}
	}

}

Object_Hierarchy_Data_Chunk::~Object_Hierarchy_Data_Chunk()
{
	delete[] ob_name;
	delete[] extra_data;
}


size_t Object_Hierarchy_Data_Chunk::size_chunk ()
{
	if (ob_name)
	{
		size_t ob_name_length = strlen(ob_name);
		chunk_size = 12 + 4+4*num_extra_data + ob_name_length + 4 - ob_name_length%4;
	}
	else
	{
		chunk_size = 12 + 4+4*num_extra_data + 4;
	}
	return chunk_size;
}


void Object_Hierarchy_Data_Chunk::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;
	
	*(int*) data_start=num_extra_data;
	data_start+=4;
	
	for (int i=0; i<num_extra_data; i++)
	{
		*((int *) data_start) = extra_data[i];
		data_start += 4;
	}
	
	if (ob_name)
	{
		sprintf (data_start, "%s", ob_name);
	}
	else
	{
		*data_start = 0;
	}
}

Object_Hierarchy_Data_Chunk::Object_Hierarchy_Data_Chunk (Chunk_With_Children * parent, const char * data_start, size_t size)
: Chunk (parent, "OBJHIERD"), object(0), ob_name (0)
{
	char const * const start = data_start;
	num_extra_data=ReadListCount(start,data_start,size,sizeof(int));

	if (num_extra_data)
		extra_data = new int[num_extra_data];
	else
		extra_data = 0;

	for (int i = 0; i < num_extra_data; i++)
	{
		extra_data[i] =	*((int *) data_start);
		data_start += 4;
	}
	
	ob_name = CopyOptionalNameFromChunk(start, size, data_start);
}

void Object_Hierarchy_Data_Chunk::post_input_processing ()
{
	find_object_for_this_section();
}

void Object_Hierarchy_Data_Chunk::find_object_for_this_section ()
{
	*((Object_Chunk **)&object) = NULL;

	Chunk_With_Children * root = GetRootChunk();

	// A level file can leave a section's name blank, and the chunk then holds no name at all. This search needs the name,
	// and so does the code that later builds the section, which uses it with no check, so skipping the search would only
	// move the crash. The level is refused instead: load_rif sees error_code and stops with its error message. All 2006
	// sections in the game's own files have a name.
	if (!ob_name)
	{
		root->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	List<Chunk *> oblist;
	root->lookup_child ("RBOBJECT",oblist);

	for (LIF<Chunk *> oli(&oblist); !oli.done(); oli.next())
	{
		Object_Chunk * ob = (Object_Chunk *)oli();

		if (!strcmp (ob->object_data.o_name, ob_name))
		{
			*((Object_Chunk **)&object) = ob;
			break;
		}
	}
}
//////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE("OBHIERNM",Object_Hierarchy_Name_Chunk)

Object_Hierarchy_Name_Chunk::Object_Hierarchy_Name_Chunk (Object_Hierarchy_Chunk * parent, const char * hname)
: Chunk (parent, "OBHIERNM")
{
	if (hname)
	{
		hierarchy_name = new char [strlen(hname)+1];
		strcpy (hierarchy_name, hname);
	}
	else
	{
		hierarchy_name = new char [1];
		*hierarchy_name = 0;
	}
}

Object_Hierarchy_Name_Chunk::~Object_Hierarchy_Name_Chunk()
{
	delete[] hierarchy_name;
}

void Object_Hierarchy_Name_Chunk::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;
	
	memcpy (data_start, hierarchy_name, strlen(hierarchy_name) + 1);
	
}

Object_Hierarchy_Name_Chunk::Object_Hierarchy_Name_Chunk (Chunk_With_Children * parent, const char * data, size_t const chunk_bytes)
: Chunk (parent, "OBHIERNM")
{
	hierarchy_name = CopyNameFromChunk(data, chunk_bytes, data);
}


///////////////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("OBHALTSH",Object_Hierarchy_Alternate_Shape_Set_Chunk,4)

Object_Hierarchy_Alternate_Shape_Set_Chunk::Object_Hierarchy_Alternate_Shape_Set_Chunk(Chunk_With_Children* parent,int num,const char* name)
:Chunk(parent,"OBHALTSH")
{
	Shape_Set_Name=new char[strlen(name)+1];
	Shape_Set_Num=num;
	strcpy(Shape_Set_Name,name);
	flags=0;
	spare[0]=spare[1]=spare[2]=0;
}
Object_Hierarchy_Alternate_Shape_Set_Chunk::Object_Hierarchy_Alternate_Shape_Set_Chunk(Chunk_With_Children* parent,const char* name)
:Chunk(parent,"OBHALTSH")
{
	Shape_Set_Name=new char[strlen(name)+1];
	strcpy(Shape_Set_Name,name);
	flags=0;
	spare[0]=spare[1]=spare[2]=0;
	Shape_Set_Num=-1;
	
	//find next available shape set number
	List<Chunk*> chlist;
	parent->lookup_child("OBHALTSH",chlist);
	int num=0;
	for(LIF<Chunk*> chlif(&chlist);!chlif.done();)
	{
		Object_Hierarchy_Alternate_Shape_Set_Chunk* ohassc=(Object_Hierarchy_Alternate_Shape_Set_Chunk*) chlif();
		if(ohassc->Shape_Set_Num==num)
		{
			num++;
			chlif.restart();
		} 
		else
		{
			chlif.next();
		}
	}

	Shape_Set_Num=num;

}

Object_Hierarchy_Alternate_Shape_Set_Chunk::Object_Hierarchy_Alternate_Shape_Set_Chunk(Chunk_With_Children* parent,const char* data,size_t size)
:Chunk(parent,"OBHALTSH")
{
	char const * const start = data;
		
	Shape_Set_Num=*(int*)data;
	data+=4;
	
	size_t name_length=0;
	Shape_Set_Name=CopyNameFromChunk(start,size,data,&name_length);
	data+=(name_length+4)&~3;

	// two names, which take at least 4 bytes each
	size_t const smallest_entry=8;
	int num_shapes=ReadListCount(start,data,size,smallest_entry);

	// Long names can use up the room that the count was measured against. Each read here looks for itself, so a list that runs out is refused, and the count stops it going on for long.
	for(int i=0;i<num_shapes;i++)
	{
		Replaced_Shape_Details* rsd=new Replaced_Shape_Details;
		
		rsd->old_object_name=CopyNameFromChunk(start,size,data,&name_length);
		data+=(name_length+4)&~3;

		rsd->new_object_name=CopyNameFromChunk(start,size,data,&name_length);
		data+=(name_length+4)&~3;

		Replaced_Shape_List.add_entry(rsd);	
	}

   	flags=ReadFromChunk<int>(start,size,data);

	for(int i=0;i<3;i++)
	{
		spare[i]=ReadFromChunk<int>(start,size,data);
	}
	
}

Object_Hierarchy_Alternate_Shape_Set_Chunk::~Object_Hierarchy_Alternate_Shape_Set_Chunk()
{
	delete[] Shape_Set_Name;

	while(Replaced_Shape_List.size())
	{
		delete Replaced_Shape_List.first_entry();
		Replaced_Shape_List.delete_first_entry();
	}

}

void Object_Hierarchy_Alternate_Shape_Set_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=Shape_Set_Num;
	data_start+=4;

	strcpy(data_start,Shape_Set_Name);
	data_start+=(strlen(Shape_Set_Name)+4)&~3;

	*(int*)data_start=Replaced_Shape_List.size();
	data_start+=4;

	for(LIF<Replaced_Shape_Details*> rlif(&Replaced_Shape_List);!rlif.done();rlif.next())
	{
		memcpy(data_start,rlif()->old_object_name,strlen(rlif()->old_object_name)+1);
		data_start+=(strlen(rlif()->old_object_name)+4)&~3;

		memcpy(data_start,rlif()->new_object_name,strlen(rlif()->new_object_name)+1);
		data_start+=(strlen(rlif()->new_object_name)+4)&~3;
	}

	*(int*)data_start=flags;
	data_start+=4;

	for(int i=0;i<3;i++)
	{
		*(int*)data_start=spare[i];
		data_start+=4;
	}

}

size_t Object_Hierarchy_Alternate_Shape_Set_Chunk::size_chunk()
{
	chunk_size=36;
	chunk_size+=(strlen(Shape_Set_Name)+4)&~3;
	for(LIF<Replaced_Shape_Details*> rlif(&Replaced_Shape_List);!rlif.done();rlif.next())
	{
		chunk_size+=(strlen(rlif()->old_object_name)+4)&~3;
		chunk_size+=(strlen(rlif()->new_object_name)+4)&~3;
	}
	return chunk_size;

}


///////////////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("HSETCOLL",Hierarchy_Shape_Set_Collection_Chunk,4)

Hierarchy_Shape_Set_Collection_Chunk::Hierarchy_Shape_Set_Collection_Chunk(Chunk_With_Children* parent,int num,const char* name)
:Chunk(parent,"HSETCOLL")
{
	Set_Collection_Name=new char[strlen(name)+1];
	Set_Collection_Num=num;
	strcpy(Set_Collection_Name,name);
	TypeIndex=flags=0;
}


Hierarchy_Shape_Set_Collection_Chunk::Hierarchy_Shape_Set_Collection_Chunk(Chunk_With_Children* parent,const char* data,size_t size)
:Chunk(parent,"HSETCOLL")
{
	char const * const start = data;
		
	Set_Collection_Num=*(int*)data;
	data+=4;
	
	size_t name_length=0;
	Set_Collection_Name=CopyNameFromChunk(start,size,data,&name_length);
	data+=(name_length+4)&~3;

	int list_length=ReadListCount(start,data,size,sizeof(int));

	for(int i=0;i<list_length;i++)
	{
		Index_List.add_entry(*(int*)data);		
		data+=4;
	}

	TypeIndex=ReadFromChunk<int>(start,size,data);
	flags=ReadFromChunk<int>(start,size,data);
}

Hierarchy_Shape_Set_Collection_Chunk::~Hierarchy_Shape_Set_Collection_Chunk()
{
	delete[] Set_Collection_Name;
}

void Hierarchy_Shape_Set_Collection_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=Set_Collection_Num;
	data_start+=4;

	strcpy(data_start,Set_Collection_Name);
	data_start+=(strlen(Set_Collection_Name)+4)&~3;

	*(int*)data_start=Index_List.size();
	data_start+=4;

	for(LIF<int> slif(&Index_List);!slif.done();slif.next())
	{
		*(int*)data_start=slif();
		data_start+=4;
	}

	*(int*)data_start=TypeIndex;
	data_start+=4;
	*(int*)data_start=flags;
	data_start+=4;
	
}

size_t Hierarchy_Shape_Set_Collection_Chunk::size_chunk()
{
	chunk_size=12+16;
	chunk_size+=(strlen(Set_Collection_Name)+4)&~3;
	chunk_size+=Index_List.size()*4;
	return chunk_size;

}

///////////////////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE("HIDEGDIS",Hierarchy_Degradation_Distance_Chunk)

Hierarchy_Degradation_Distance_Chunk::Hierarchy_Degradation_Distance_Chunk(Chunk_With_Children* parent,int _num_detail_levels)
:Chunk(parent,"HIDEGDIS")
{
	num_detail_levels=_num_detail_levels;
	if(num_detail_levels)
	{
		distance_array=new int[num_detail_levels];
	}
	else
	{
		distance_array=0;
	}
	//fill in some arbitrary distance values
	for(int i=0;i<num_detail_levels;i++)
	{
		distance_array[i]=i*10000;
	}

}

Hierarchy_Degradation_Distance_Chunk::Hierarchy_Degradation_Distance_Chunk(Chunk_With_Children* parent,const char* data,size_t size)
:Chunk(parent,"HIDEGDIS")
{
	char const * const start = data;
	num_detail_levels=ReadListCount(start,data,size,sizeof(int));
	
	if(num_detail_levels)
	{
		distance_array=new int[num_detail_levels];
	}
	else
	{
		distance_array=0;
	}
	
	for(int i=0;i<num_detail_levels;i++)
	{
		distance_array[i]=*(int*)data;
		data+=sizeof(int);
	}		
	
}

Hierarchy_Degradation_Distance_Chunk::~Hierarchy_Degradation_Distance_Chunk()
{
	delete[] distance_array;
}

void Hierarchy_Degradation_Distance_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=num_detail_levels;
	data_start+=4;

	for(int i=0;i<num_detail_levels;i++)
	{
		*(int*)data_start=distance_array[i];
		data_start+=4;
	}
}

size_t Hierarchy_Degradation_Distance_Chunk::size_chunk()
{
	chunk_size=16+num_detail_levels*4;
	return chunk_size;

}


///////////////////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("HIERBBOX",Hierarchy_Bounding_Box_Chunk,24)

Hierarchy_Bounding_Box_Chunk::Hierarchy_Bounding_Box_Chunk(Chunk_With_Children* parent,const char* data,size_t datasize)
:Chunk(parent,"HIERBBOX")
{
	assert(datasize==2*sizeof(ChunkVectorInt));

	min=*(ChunkVectorInt*)data;
	data+=sizeof(ChunkVectorInt);
	max=*(ChunkVectorInt*)data;
}

void Hierarchy_Bounding_Box_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;

	*(ChunkVectorInt*)data_start=min;
	data_start+=sizeof(ChunkVectorInt);
	*(ChunkVectorInt*)data_start=max;
}
///////////////////////////////////////////////////
