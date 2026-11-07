// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   object headers in level files are read without going past their end, and names and other text are
//   copied using their exact measured length, so they always fit; code only the level editor used was removed
//   a child chunk whose size cannot be real is refused
//   the copy of an object's name moved to chunk_object_name.hpp
//   a module's lists and an object's track and alternate places are measured against their chunks before they're made
//   an object's header is only made inside an object, because it writes through its parent
//   a jump in a module's list of what it can see has to go forward
//   a track's sound has to be its middle, start or end sound
//   a track's times have to be ones the game can work through
//   a name is only read as far as the end of its chunk
//   the code that locked chunks inside a level file on disk is gone: nothing could reach it, and it read names onto the stack with no limit
//   a chunk too small for what its kind always reads is kept as plain bytes, and the level is refused
//   what comes after a name or a list is only read while it's still inside the chunk

#include <string>
#include "chunk.hpp"
#include "chunk_type.hpp"
#include "mishchnk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "environment_chunk.hpp"
#include "md5.h"
// Class Object_Chunk functions

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(chunk_object)

RIF_IMPLEMENT_DYNCREATE("RBOBJECT",Object_Chunk)

/*
Children for Object_Chunk :

"OBJHEAD1"		Object_Header_Chunk
"OBINTDT"		Object_Interface_Data_Chunk
"OBJPRJDT"		Object_Project_Data_Chunk
"MODULEDT"		Object_Module_Data_Chunk
"SHPVTINT"		Shape_Vertex_Intensities_Chunk
"OBJTRAK2"		Object_Track_Chunk2
"TRAKSOUN"		Object_Track_Sound_Chunk
"OBANSEQS"		Object_Animation_Sequences_Chunk
"PLOBJLIT"		Placed_Object_Light_Chunk
"ALTLOCAT"		Object_Alternate_Locations_Chunk
*/





// from buffer
Object_Chunk::Object_Chunk(Chunk_With_Children * parent, const char *data, size_t size)
: Lockable_Chunk_With_Children (parent, "RBOBJECT"),
 object_data ()
{
	const char * buffer_ptr = data;

	object_data_store = (ChunkObject *) &object_data;
	
	while ((data-buffer_ptr)< (signed) size) {

		if (!ChildChunkFits(data, buffer_ptr, (signed) size)) {
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(data);
		data += *(int *)(data + 8);

	}

	program_object_index=-1;

}

Object_Chunk::~Object_Chunk()
{
}

Object_Header_Chunk * Object_Chunk::get_header()
{
	
	return (Object_Header_Chunk *) this->lookup_single_child ("OBJHEAD1");


}

Shape_Chunk * Object_Chunk::get_assoc_shape()
{
	if (!get_header()) return 0;

	return get_header()->associated_shape;

}

BOOL Object_Chunk::assoc_with_shape (Shape_Chunk * shpch)
{
	Object_Header_Chunk * obhead = get_header();
	
	if (!obhead) return 0;

	// check that we are not already associated with this shape

	if (obhead->associated_shape == shpch) return 1;

	Shape_Header_Chunk * shphead = shpch->get_header();

	if (!shphead) return 0;

	// deassociate with the old shape

	if (obhead->associated_shape)
		deassoc_with_shape (obhead->associated_shape);

	// associate with the new

	obhead->associated_shape = shpch;

	if (!shphead->associated_objects_store.contains(this))
		shphead->associated_objects_store.add_entry(this);

	return 1;

}

BOOL Object_Chunk::deassoc_with_shape (Shape_Chunk * shpch)
{
	Shape_Header_Chunk * shphead = shpch->get_header();

	if (!shphead) return 0;

	if (shphead->associated_objects_store.contains(this))
		shphead->associated_objects_store.delete_entry(this);
	
 	Object_Header_Chunk * obhead = get_header();

	obhead->associated_shape = 0;
	
	return 1;

}

	

	

void Object_Chunk::set_lock_user (char * user)
{
	Object_Header_Chunk * hdptr = get_header();
	
	if (!hdptr) return;

	strncpy (hdptr->lock_user, user,16);

	hdptr->lock_user[16] = 0;
}	


BOOL Object_Chunk::inc_v_no ()
{
	Object_Header_Chunk * hdptr = get_header();

	if (!hdptr) return (FALSE);

	hdptr->version_no++;

	return (TRUE);
}

BOOL Object_Chunk::same_and_updated(Object_Chunk & obj)
{
	
	Object_Header_Chunk * hd1ptr = get_header();

	if (!hd1ptr) return (0);

	Object_Header_Chunk * hd2ptr = obj.get_header();

	if (!hd2ptr) return (0);

	return (hd1ptr->version_no < hd2ptr->version_no && 
		(!strcmp(obj.object_data.o_name, object_data.o_name)) );

}

BOOL Object_Chunk::assoc_with_shape_no(File_Chunk *fc)
{
	Object_Header_Chunk * hdptr = get_header();
	Shape_Chunk * shp = NULL;
	Shape_Header_Chunk * shphd;
	
	if (!hdptr) return (FALSE);

	List<Chunk *> chlst;
	fc->lookup_child("REBSHAPE",chlst);
	
	LIF<Chunk *> l(&chlst);
	for (; !l.done(); l.next())
	{
		shp = (Shape_Chunk *)l();
		shphd = shp->get_header();
		if (shphd)
		{
			if (hdptr->shape_id_no == shphd->file_id_num)
				break;
		}
	}
	if (!l.done())
	{
		assoc_with_shape(shp);
	}
	else
	{
		return(FALSE);
	}	

	return(TRUE);
	
	
}




void Object_Chunk::post_input_processing()
{
	CalculateID();	
	
	if (get_header())
		if (get_header()->flags & GENERAL_FLAG_LOCKED)
			external_lock = TRUE;

	Chunk_With_Children::post_input_processing();

}


VModule_Array_Chunk * Object_Chunk::get_vmod_chunk()
{
	Object_Module_Data_Chunk * omdc = 0;
	
 	omdc = (Object_Module_Data_Chunk *) lookup_single_child ("MODULEDT");
	
	
	if (omdc)
	{
		VModule_Array_Chunk * vmac = 0;
		vmac = (VModule_Array_Chunk *)omdc->lookup_single_child ("VMDARRAY") ;
		
		if (vmac) return(vmac);
	}
	
	return(0);
	
	
}

void Object_Chunk::destroy(File_Chunk * fc)
{
	List<Chunk *> cl;
	fc->lookup_child("RBOBJECT",cl);
	
	for (LIF<Chunk *> cli(&cl); !cli.done(); cli.next())
	{
		Object_Chunk * ob = (Object_Chunk *)cli();
		assert(ob);

		if (ob == this) continue;
		
		VModule_Array_Chunk * vmac = ob->get_vmod_chunk();
		
		BOOL in_object_va = FALSE;
		
		if (vmac)
		{
			int pos=0;
			for(int i=0;i<vmac->num_array_items;i++)
			{
								
				if (vmac->vmod_array[i].object_index==object_data.index_num)
				{
					in_object_va=TRUE;
					//update branch indeces for branches that go beyond this point
					for(int j=0;j<vmac->num_array_items;j++)
					{
						if(vmac->vmod_array[j].branch_no> pos)
							vmac->vmod_array[j].branch_no--;
					}		
				}
				else
				{
					if(pos!=i)
					{
						vmac->vmod_array[pos]=vmac->vmod_array[i];
					}
					pos++;
				}					
			}

			if(in_object_va)
			{
				if(pos==0)
				{
					delete vmac;
				}
				else
				{
					//excess entries should be properly deleted when the array is deleted
					vmac->num_array_items=pos;
				}
			}
		}

		//now remove this object from any adjacency data

		Object_Module_Data_Chunk* omdc=(Object_Module_Data_Chunk*)ob->lookup_single_child("MODULEDT");
		if(omdc)
		{
			Adjacent_Module_Entry_Points_Chunk* amepc=(Adjacent_Module_Entry_Points_Chunk*)omdc->lookup_single_child("ADJMDLEP");
		
			if(amepc)
			{
				for(LIF<Adjacent_Module> ad_lif(&amepc->adjacent_modules_list);!ad_lif.done();)
				{
					if(ad_lif().object_index==object_data.index_num)
						ad_lif.delete_current();
					else
						ad_lif.next();
										
				}
			}
		}
		
		
	}
	
	Shape_Chunk * shp = get_assoc_shape();
	
	deassoc_with_shape (shp);

	lock_chunk(*fc);
	
	deleted = TRUE;

	unlock_chunk(*fc,TRUE);
	
}


void Object_Chunk::update_my_chunkobject(ChunkObject & cob)
{
	//store the object's index , so that it doesn't get lost
	int object_index=object_data_store->index_num;
	*object_data_store = cob;
	//replace the object index
	object_data_store->index_num=object_index;
	//recalculate the object's id in case the name has changed
	CalculateID();	
}

ObjectID Object_Chunk::CalculateID()
{
	ObjectID retval={0,0};
	List<Chunk*> chlist;
	parent->lookup_child("REBENVDT",chlist);
	if(!chlist.size()) return retval;
	((Environment_Data_Chunk*)chlist.first_entry())->lookup_child("RIFFNAME",chlist);
	if(!chlist.size()) return retval;
	// rif name + object name, hashed; both come from the .rif and have no length limit
	std::string Name(((RIF_Name_Chunk*)chlist.first_entry())->rif_name);

	Name += object_data.o_name;
	char buffer[16];
	md5_buffer(Name.c_str(),(unsigned int)Name.size(),&buffer[0]);
	buffer[7]=0;
	object_data_store->ID=*(ObjectID*)&buffer[0];
	return 	object_data_store->ID;
	
}

/////////////////////////////////////////

// Class Object_Header_Chunk functions
// This constructor writes through its parent, so it can only be made under the object it belongs to. Under anything else it's a chunk
// nothing reads. Every one of the 23068 of these in the game's own levels is inside an object.
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("OBJHEAD1",Object_Header_Chunk,"RBOBJECT",Object_Chunk)

// from buffer
Object_Header_Chunk::Object_Header_Chunk(Chunk_With_Children * parent, const char * hdata, size_t hsize)
: Chunk (parent, "OBJHEAD1"), object_data (((Object_Chunk*)parent)->object_data_store),
flags(0), version_no (0), associated_shape (0)
{
	Object_Chunk* parent_object = (Object_Chunk*) parent;

	// the 60-byte fixed part below is read from a copy that is never longer than the chunk: a chunk shorter than 60
	// bytes (only a damaged or crafted one) reads as if padded with zeros; the name after it is read from the chunk
	char const * const chunk_data = hdata;
	alignas(int) char fixed_part[60] = { 0 };
	memcpy (fixed_part, hdata, hsize < sizeof(fixed_part) ? hsize : sizeof(fixed_part));
	hdata = fixed_part;

	flags = *((int *) hdata);

	if (flags & OBJECT_FLAG_BASE_OBJECT)
		parent_object->object_data_store->is_base_object = TRUE;
	else
	{
		parent_object->object_data_store->is_base_object = FALSE;
	}

	// the lock-user name is the 16 bytes after the flags: copy it up to its first zero byte (or all 16 bytes if it
	// has none), and fill the rest of lock_user with zeros, so it always ends with a zero byte
	size_t lock_user_length = 0;
	while (lock_user_length < 16 && hdata[4 + lock_user_length])
	{
		lock_user_length++;
	}
	memset (lock_user, 0, sizeof(lock_user));
	memcpy (lock_user, hdata + 4, lock_user_length);

	hdata+=20;

	parent_object->object_data_store->location = *(ChunkVectorInt*) hdata;
	hdata+=sizeof(ChunkVectorInt);

	parent_object->object_data_store->orientation = *((ChunkQuat *) hdata);
	hdata+=sizeof(ChunkQuat);

	parent_object->object_data_store->index_num = *(int *) hdata;
	hdata+=4;

	version_no = *((int *) hdata);
	hdata+=4;

	shape_id_no = *((int *) hdata);
	hdata+=4;

	// the name fills the rest of the chunk (after the 60 bytes above): copy what fits in o_name, and never read
	// past the chunk
	CopyObjectName (parent_object->object_data_store->o_name, chunk_data, hsize);
	parent_object->object_data_store->ID.id1=0;
	parent_object->object_data_store->ID.id2=0;
}


size_t Object_Header_Chunk::size_chunk()
{
	size_t length = 72;

	length += (strlen(object_data->o_name)+1);

	length += (4-length%4)%4;

	chunk_size = length;

	return length;
}

BOOL Object_Header_Chunk::output_chunk (HANDLE & hand)
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

void Object_Header_Chunk::fill_data_block ( char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = flags;
	strncpy ((data_start + 4), lock_user, 16);
	data_start+=20;

	*((ChunkVectorInt *) (data_start)) = object_data->location;
	data_start+=sizeof(ChunkVectorInt);
		
	*((ChunkQuat *) (data_start)) = object_data->orientation;
	data_start+=sizeof(ChunkQuat);

	*(int*) data_start=object_data->index_num;
	data_start+=4;

	*((int *) data_start) = version_no;
	data_start+=4;
	
	*((int *) data_start) = shape_id_no;
	data_start+=4;

	memcpy (data_start, object_data->o_name, strlen(object_data->o_name) + 1);
}

void Object_Header_Chunk::prepare_for_output()
{
	version_no ++;
}

/////////////////////////////////////////

// Class Object_Interface_Data_Chunk functions
RIF_IMPLEMENT_DYNCREATE("OBINTDT",Object_Interface_Data_Chunk)

/*
Children for Object_Interface_Data_Chunk :

"OBJNOTES"		Object_Notes_Chunk
*/

// from buffer
Object_Interface_Data_Chunk::Object_Interface_Data_Chunk(Chunk_With_Children * parent, const char *data, size_t size)
: Chunk_With_Children (parent, "OBINTDT")
{
	const char * buffer_ptr = data;

	while ((data-buffer_ptr)< (signed) size) {

		if (!ChildChunkFits(data, buffer_ptr, (signed) size)) {
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(data);
		data += *(int *)(data + 8);
	}
}


///////////////////////////////////////

// Class Object_Notes_Chunk functions
RIF_IMPLEMENT_DYNCREATE("OBJNOTES",Object_Notes_Chunk)

Object_Notes_Chunk::Object_Notes_Chunk (Chunk_With_Children * parent,
 const char * _data, size_t _data_size) 
: Chunk(parent, "OBJNOTES"), data_size(_data_size), data(NULL)
{
	data_store = new char [data_size];

	*((char **) &data) = data_store;

	for (int i = 0; i<(signed) data_size; i++)
		data_store[i] = _data[i];

}

void Object_Notes_Chunk::fill_data_block (char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	for (int i = 0; i<((signed) chunk_size-12); i++)
		data_start[i] = data[i];
}

Object_Notes_Chunk::~Object_Notes_Chunk ()
{
	delete[] data_store;
}

BOOL Object_Notes_Chunk::output_chunk (HANDLE &hand)
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


///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("MODULEDT",Object_Module_Data_Chunk)

/*
Children For Object_Module_Data_Chunk :

"VMDARRAY"		VModule_Array_Chunk
"ADJMDLEP"		Adjacent_Module_Entry_Points_Chunk
"MODFLAGS"		Module_Flag_Chunk
"WAYPOINT"		Module_Waypoint_Chunk
"MODACOUS"		Module_Acoustics_Chunk
"AIMODMAS"		AI_Module_Master_Chunk
"AIMODSLA"		AI_Module_Slave_Chunk

*/

Object_Module_Data_Chunk::Object_Module_Data_Chunk (Chunk_With_Children * parent,const char * mdata, size_t msize)
: Chunk_With_Children (parent, "MODULEDT")
{
	const char * buffer_ptr = mdata;

	while ((mdata-buffer_ptr)< (signed) msize) {

		if (!ChildChunkFits(mdata, buffer_ptr, (signed) msize)) {
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}
		DynCreate(mdata);
		mdata += *(int *)(mdata + 8);

	}
	
}

///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("VMDARRAY",VModule_Array_Chunk)

VModule_Array_Chunk::VModule_Array_Chunk(Object_Module_Data_Chunk * parent, VMod_Arr_Item * vma, int num_in_vma)
: Chunk (parent, "VMDARRAY")
{
	int i;
	
	num_array_items = num_in_vma;
	
	vmod_array = new VMod_Arr_Item [num_in_vma];
	
	for (i=0; i<num_in_vma; i++)
	{
		vmod_array[i] = vma[i];
	}
	
}


void VModule_Array_Chunk::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = num_array_items;

	data_start += 4;
	
	for (int i=0; i<num_array_items; i++)
	{
		*((int *) data_start) = vmod_array[i].object_index;
		data_start += 4;
			
		*((int *) data_start) = vmod_array[i].branch_no;
		data_start += 4;
		
		*((int *) data_start) = vmod_array[i].flags;
		data_start += 4;

		*((int *) data_start) = vmod_array[i].spare;
		data_start += 4;
		
	}
	
}


size_t VModule_Array_Chunk::size_chunk()
{
	chunk_size =16+16*num_array_items;
	return(chunk_size);
		
}

VModule_Array_Chunk::VModule_Array_Chunk (Chunk_With_Children * parent, const char * vmdata, size_t vmsize)
: Chunk (parent, "VMDARRAY")
{
	char const * const start = vmdata;
	size_t const item_bytes = 4 * sizeof(int);

	num_array_items = ReadListCount (start, vmdata, vmsize, item_bytes);

	vmod_array = new VMod_Arr_Item [num_array_items];

	for (int i=0; i<num_array_items; i++)
	{

		vmod_array[i].object_index = *((int *) vmdata);
		vmdata += 4;

		vmod_array[i].branch_no = *((int *) vmdata);
		vmdata += 4;

		// A closed door makes the game jump along this list to the place named here, and it walks the list until the end. A jump back would never reach the
		// end, so the game would hang. So a jump goes forward, as far as the end of the list and no further.
		if (vmod_array[i].branch_no != 0 && (vmod_array[i].branch_no <= i || vmod_array[i].branch_no > num_array_items))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		}

		vmod_array[i].flags = *((int *) vmdata);
		vmdata += 4;

		vmod_array[i].spare = *((int *) vmdata);
		vmdata += 4;
			
	}

}

VModule_Array_Chunk::~VModule_Array_Chunk ()
{
	delete[] vmod_array;
}

///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("ADJMDLEP",Adjacent_Module_Entry_Points_Chunk)

void Adjacent_Module_Entry_Points_Chunk::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = adjacent_modules_list.size();

	data_start += 4;
	
	for (LIF<Adjacent_Module> ami(&adjacent_modules_list); !ami.done(); ami.next())
	{
	
		*((int *) data_start) = ami().object_index;
		data_start += 4;
		
		*((int *) data_start) = ami().flags;
		data_start += 4;

		*((ChunkVectorInt *) data_start) = ami().entry_point;
		data_start += sizeof(ChunkVectorInt);

		
	}
	
}


size_t Adjacent_Module_Entry_Points_Chunk::size_chunk()
{
	chunk_size = 16+adjacent_modules_list.size()*20;
	return(chunk_size);
}

Adjacent_Module_Entry_Points_Chunk::Adjacent_Module_Entry_Points_Chunk (Chunk_With_Children * parent, const char * data, size_t size)
: Chunk (parent, "ADJMDLEP")
{
	char const * const start = data;
	size_t const item_bytes = 2 * sizeof(int) + sizeof(ChunkVectorInt);
	int num_array_items = ReadListCount (start, data, size, item_bytes);

	for (int i=0; i<num_array_items; i++)
	{
		Adjacent_Module am;

		am.object_index = *((int *) data);
		data += 4;
		
		am.flags = *((int *) data);
		data += 4;

		am.entry_point = *((ChunkVectorInt *) data);
		data += sizeof(ChunkVectorInt);

		
		adjacent_modules_list.add_entry(am);	
			
	}

}

///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("MODFLAGS",Module_Flag_Chunk,8)

Module_Flag_Chunk::Module_Flag_Chunk(Object_Module_Data_Chunk* parent)
:Chunk(parent,"MODFLAGS")
{
	Flags=spare=0;
}

Module_Flag_Chunk::Module_Flag_Chunk(Chunk_With_Children* parent,const char* data,size_t)
:Chunk(parent,"MODFLAGS")
{
	Flags=*(int*)data;
	data+=4;
	spare=*(int*)data;
}

void Module_Flag_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=Flags;
	data_start+=4;
	*(int*)data_start=spare;
	
}
///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("MODZONE",Module_Zone_Chunk,8)

Module_Zone_Chunk::Module_Zone_Chunk(Object_Module_Data_Chunk* parent)
:Chunk(parent,"MODZONE")
{
	Zone=spare=0;
}

Module_Zone_Chunk::Module_Zone_Chunk(Chunk_With_Children* parent,const char* data,size_t)
:Chunk(parent,"MODZONE")
{
	Zone=*(int*)data;
	data+=4;
	spare=*(int*)data;
}

void Module_Zone_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=Zone;
	data_start+=4;
	*(int*)data_start=spare;
	
}

///////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("MODACOUS",Module_Acoustics_Chunk,12)

Module_Acoustics_Chunk::Module_Acoustics_Chunk(Object_Module_Data_Chunk* parent)
:Chunk(parent,"MODACOUS")
{
	env_index=-1; //negative means default environment acoustics
	reverb=-1.0;
	spare=0;
}

Module_Acoustics_Chunk::Module_Acoustics_Chunk(Chunk_With_Children* parent,const char* data,size_t)
:Chunk(parent,"MODACOUS")
{
	env_index=*(int*)data;
	data+=4;
	reverb=*(float*)data;
	data+=4;
	spare=*(int*)data;
}

void Module_Acoustics_Chunk::fill_data_block(char* data_start)
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


RIF_IMPLEMENT_DYNCREATE("OBJTRAK2",Object_Track_Chunk2)

Object_Track_Chunk2::Object_Track_Chunk2(Object_Chunk * parent)
: Chunk (parent, "OBJTRAK2")
{
	num_sections=0;
	sections=0;
	flags=timer_start=0;
}

Object_Track_Chunk2::~Object_Track_Chunk2()
{
	delete[] sections;
}

void Object_Track_Chunk2::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;
	
	*(int*)data_start=num_sections;
	data_start+=4;

	for(int i=0;i<num_sections;i++)
	{
		*(ChunkTrackSection*)data_start=sections[i];
		data_start+=sizeof(ChunkTrackSection);
	}

	*(int*)data_start=flags;
	data_start+=4;
	
	*(int*)data_start=timer_start;

	
	
}

size_t Object_Track_Chunk2::size_chunk ()
{
	chunk_size = 12 + 12 +num_sections*sizeof(ChunkTrackSection) ;
	return(chunk_size);
}

Object_Track_Chunk2::Object_Track_Chunk2 (Chunk_With_Children * parent,const char * data, size_t size)
: Chunk (parent, "OBJTRAK2")
{
	int i;
	char const * const start = data;

	num_sections=ReadListCount(start,data,size,sizeof(ChunkTrackSection));

	if(num_sections)
		sections=new ChunkTrackSection[num_sections];
	else
		sections=0;

	for(i=0;i<num_sections;i++)
	{
		sections[i]=*(ChunkTrackSection*)data;
		data+=sizeof(ChunkTrackSection);
	}

	flags=ReadFromChunk<int>(start,size,data);
	timer_start=ReadFromChunk<int>(start,size,data);

	// The game works through a track's timer a section at a time, until what's left fits inside a section. So a section has to take some time, or the timer
	// would never be used up, and the game would hang. 65 is about a thousandth of a second, and the shortest in the game's own levels is 3,276. A start
	// time that's negative or huge could keep that going as well, and the game's own go up to 524,288.
	int const shortest_section=65;
	int const latest_start=1<<24;

	for(i=0;i<num_sections;i++)
	{
		if(sections[i].time_for_section<shortest_section)
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		}
	}

	if(timer_start<0 || timer_start>latest_start)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}

	if(!(flags & TrackFlag_QuatProblemSorted))
	{
		for(i=0;i<num_sections;i++)
		{
			ChunkQuat temp=sections[i].quat_start;
			sections[i].quat_start.x=-temp.w;
			sections[i].quat_start.y=temp.x;
			sections[i].quat_start.z=temp.y;
			sections[i].quat_start.w=-temp.z;

			temp=sections[i].quat_end;
			sections[i].quat_end.x=-temp.w;
			sections[i].quat_end.y=temp.x;
			sections[i].quat_end.z=temp.y;
			sections[i].quat_end.w=-temp.z;
		}
		flags|=TrackFlag_QuatProblemSorted;
	}
	
}

///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("TRAKSOUN",Object_Track_Sound_Chunk,24)

Object_Track_Sound_Chunk::Object_Track_Sound_Chunk(Chunk_With_Children* parent)
:Chunk(parent,"TRAKSOUN")
{
	wav_name=0;
	inner_range=5000;
	outer_range=40000;
	max_volume=127;
	pitch=0;
	flags=0;
	index=0;
}

Object_Track_Sound_Chunk::Object_Track_Sound_Chunk(Chunk_With_Children* const parent,const char* data,size_t const chunk_bytes)
:Chunk(parent,"TRAKSOUN")
{
	char const * const chunk_start = data;
	inner_range=*(unsigned long*)data;
	data+=4;
	outer_range=*(unsigned long*)data;
	data+=4;

	max_volume=*(int*)data;
	data+=4;
	pitch=*(int*)data;
	data+=4;
	flags=*(int*)data;
	data+=4;
	index=*(int*)data;
	data+=4;

	// A track has three sounds, for the middle, the start and the end of its movement, and this says which one this is.
	if(index<0 || index>2)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}

	wav_name=CopyNameFromChunk(chunk_start,chunk_bytes,data);

}

Object_Track_Sound_Chunk::~Object_Track_Sound_Chunk()
{
	delete[] wav_name;
}											 

void Object_Track_Sound_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;
	
	*(unsigned long*)data_start=inner_range;
	data_start+=4;
	*(unsigned long*)data_start=outer_range;
	data_start+=4;

	*(int*)data_start=max_volume;
	data_start+=4;
	*(int*)data_start=pitch;
	data_start+=4;
	*(int*)data_start=flags;
	data_start+=4;
	*(int*)data_start=index;
	data_start+=4;

	memcpy(data_start,wav_name,strlen(wav_name)+1);

}

size_t Object_Track_Sound_Chunk::size_chunk()
{
	chunk_size=12+24;
	chunk_size+=(strlen(wav_name)+4)&~3;
	return chunk_size;
}
///////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("ALTLOCAT",Object_Alternate_Locations_Chunk,8)

Object_Alternate_Locations_Chunk::Object_Alternate_Locations_Chunk(Chunk_With_Children* parent,const char* data,size_t size)
:Chunk(parent,"ALTLOCAT")
{
	char const * const start = data;

	group=*(int*) data;
	data+=4;
	spare2=*(int*) data;
	data+=4;

	num_locations=ReadListCount(start,data,size,sizeof(AltObjectLocation));

	if(num_locations)
		locations=new AltObjectLocation[num_locations];
	else
		locations=0;

	for(int i=0;i<num_locations;i++)
	{
		locations[i] = *(AltObjectLocation*) data;
		data+=sizeof(AltObjectLocation);
	}
}

Object_Alternate_Locations_Chunk::~Object_Alternate_Locations_Chunk()
{
	delete[] locations;
}

void Object_Alternate_Locations_Chunk::fill_data_block(char* data)
{
	strncpy (data, identifier, 8);
	data += 8;
	*((int *) data) = chunk_size;
	data += 4;

	*(int*)data=group;
	data+=4;
	
	*(int*)data=spare2;
	data+=4;
	
	
	*(int*) data = num_locations;
	data+=4;	
	
	for(int i=0;i<num_locations;i++)
	{
		*(AltObjectLocation*)data=locations[i];
		data+=sizeof(AltObjectLocation);
	}

}

size_t Object_Alternate_Locations_Chunk::size_chunk()
{
	chunk_size=24+num_locations*sizeof(AltObjectLocation);
	return chunk_size;
}


/////////////////////////////////////////
//class Object_Project_Data_Chunk

RIF_IMPLEMENT_DYNCREATE("OBJPRJDT",Object_Project_Data_Chunk)

CHUNK_WITH_CHILDREN_LOADER("OBJPRJDT",Object_Project_Data_Chunk)

/*
Children for Object_Project_Data_Chunk :

"MAPBLOCK"	Map_Block_Chunk
"STRATEGY"	Strategy_Chunk
"AVPSTRAT"	AVP_Strategy_Chunk
*/
