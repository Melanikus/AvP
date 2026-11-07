// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   the level file's name is kept in text that grows as needed, instead of a fixed 256-byte box, and file
//   names and other text are copied using their exact measured length; level-editor code was removed
//   a child chunk whose size cannot be real is refused
//   a compressed level goes through the checked decoder, which knows the size of the file
//   a module's points have to fit the box stored with its shape
//   a level read from a file has zero bytes after it, so that no chunk's fixed reads or names can leave the buffer
//   a child chunk whose size cannot be real is refused; the object list is asked for by count, so a number from
//   a level file cannot make the size sum wrap round
//   a level whose objects and shapes don't join up is refused, and a failed load stops before the steps that
//   rely on a whole level; object numbers are limited to what the object list can hold; so is a level with a
//   shape that has more points than the game's list of points can hold
//   a name is only read as far as the end of its chunk
//   the code that locked chunks inside a level file on disk is gone: nothing could reach it, and it read names onto the stack with no limit
//   a chunk too small for what its kind always reads is kept as plain bytes, and the level is refused
//   a hierarchy the game couldn't build refuses the level

#include <string>
#include "chunk.hpp"
#include "chunk_type.hpp"
#include "mishchnk.hpp"
#include "chunk_shape.hpp"
#include "chunk_object.hpp"
#include "hierarchy_chunk.hpp"
#include "huffman.hpp"
#include "mbstring.h"

// Class Lockable_Chunk_With_Children functions

extern char * users_name;

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(mishchnk)

BOOL Lockable_Chunk_With_Children::lock_chunk(File_Chunk & fchunk)
{
	if (!fchunk.filename) return FALSE;

	if (local_lock) return FALSE; // you can't lock a chunk twice

	if(external_lock) return FALSE;
	local_lock = TRUE;
	set_lock_user (users_name);
	return TRUE;
}	

BOOL Lockable_Chunk_With_Children::unlock_chunk (File_Chunk & fchunk, BOOL updateyn)
{
	if (updateyn)
	{
		updated = TRUE;
	}
	local_lock = FALSE;
	(void)fchunk;

	return TRUE;
}


size_t Lockable_Chunk_With_Children::size_chunk_for_process()
{
	if (output_chunk_for_process)
		return size_chunk();
	return(chunk_size = 0);
}


void Lockable_Chunk_With_Children::fill_data_block_for_process(char * data_start)
{
	if (output_chunk_for_process)
	{
		fill_data_block(data_start);
		output_chunk_for_process = FALSE;
	}
}




///////////////////////////////////////

// Class File_Chunk functions

/*
Children for File_Chunk :
"REBSHAPE"		Shape_Chunk
"RSPRITES"		AllSprites_Chunk
"RBOBJECT"		Object_Chunk
"RIFVERIN"		RIF_Version_Num_Chunk
"REBENVDT"		Environment_Data_Chunk
"REBENUMS"		Enum_Chunk
"OBJCHIER"		Object_Hierarchy_Chunk
"OBHALTSH"		Object_Hierarchy_Alternate_Shape_Set_Chunk
"HIDEGDIS"		Hierarchy_Degradation_Distance_Chunk
"INDSOUND"		Indexed_Sound_Chunk
"HSETCOLL"		Hierarchy_Shape_Set_Collection_Chunk
"DUMMYOBJ"		Dummy_Object_Chunk

*/

File_Chunk::File_Chunk(const char * file_name)
: Chunk_With_Children (NULL, "REBINFF2"), version_chunk (0)
{
// Load in whole chunk and traverse
	char rifIsCompressed = FALSE;
	char *uncompressedData = NULL;
	HANDLE rif_file;
	DWORD file_size;
	DWORD file_size_from_file;
	unsigned long bytes_read;
	char * buffer;
	char * buffer_ptr;
	char id_buffer[9];

	Parent_File = this;

	error_code = 0;
	object_array_size=0;
	object_array=0;

	filename = new char [strlen(file_name) + 1];

	strcpy (filename, file_name);

	rif_file = avp_CreateFile (file_name, GENERIC_READ, 0, 0, OPEN_EXISTING, 
					FILE_FLAG_RANDOM_ACCESS, 0);

	if (rif_file == INVALID_HANDLE_VALUE)
	{
		error_code = CHUNK_FAILED_ON_LOAD;
		return;
	}

	file_size = GetFileSize(rif_file, NULL);

	if (!ReadFile(rif_file, id_buffer, 8, &bytes_read, 0))
	{
		error_code = CHUNK_FAILED_ON_LOAD;
		CloseHandle (rif_file);
		return;
	}

	/* KJL 16:46:14 19/09/98 - check for a compressed rif */
	if (!strncmp (id_buffer, COMPRESSED_RIF_IDENTIFIER, 8))
	{
		rifIsCompressed = TRUE;
	}
	else if (strncmp (id_buffer, "REBINFF2", 8))
	{
		error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		CloseHandle (rif_file);
		return;
	}

	buffer = new char [file_size + HUFFMAN_SPARE_BYTES];

	/* KJL 17:57:44 19/09/98 - if the rif is compressed, we must load the whole
	file in and then pass it to the decompression routine, which will return a
	pointer to the original data. */
	if (rifIsCompressed)
	{
		if (!ReadFile(rif_file, buffer+8, (file_size-8), &bytes_read, 0))
		{
			error_code = CHUNK_FAILED_ON_LOAD;
			CloseHandle (rif_file);
			return;
		}

		// Every level the game comes with is compressed, so this runs before anything else looks at the file. A decoder that trusted what the
		// file says could be made to write past the end of its table, or of the memory for the result. This one knows the file's size and checks.
		uncompressedData = HuffmanDecompressChecked((HuffmanPackage*)buffer, (int)file_size);

		if (!uncompressedData)
		{
			delete[] buffer;
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			CloseHandle (rif_file);
			return;
		}

		file_size = ((HuffmanPackage*)buffer)->UncompressedDataSize;

		delete[] buffer; // kill the buffer the compressed file was loaded into

		buffer_ptr = buffer = uncompressedData+12; // skip header data
	}
	else // the normal uncompressed approach:
	{
		if (!ReadFile(rif_file, &file_size_from_file, 4, &bytes_read, 0))
		{
			error_code = CHUNK_FAILED_ON_LOAD;
			CloseHandle (rif_file);
			return;
		}

		if (file_size != file_size_from_file)
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			CloseHandle (rif_file);
			return;
		}

		memset (buffer, 0, file_size + HUFFMAN_SPARE_BYTES);

		if (!ReadFile(rif_file, buffer, (file_size-12), &bytes_read, 0))
		{
			error_code = CHUNK_FAILED_ON_LOAD;
			CloseHandle (rif_file);
			return;
		}
		buffer_ptr = buffer;
	}

	// Process the RIF
	// The start of the first chunk

	while ((buffer_ptr-buffer)< ((signed) file_size-12) && !error_code)
	{
		if (!ChildChunkFits(buffer_ptr, buffer, ((signed)file_size-12)))
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(buffer_ptr);
		buffer_ptr += *(int *)(buffer_ptr + 8);
	}

	/* KJL 17:59:42 19/09/98 - release the memory holding the rif */
	if (rifIsCompressed)
	{
		free(uncompressedData);
	}
	else
	{
		delete[] buffer;
	}

	CloseHandle (rif_file);

	// After a failed load the tree is only part of a level, and this step relies on all of it being there. load_rif
	// sees error_code and stops the level with its error message.
	if (!error_code)
	{
		post_input_processing();
	}
}

File_Chunk::File_Chunk()
: Chunk_With_Children (NULL, "REBINFF2")
{
// Empty File chunk
	// It joins this chunk's children as it's made, and ~Chunk_With_Children deletes it with the rest
	version_chunk = new RIF_Version_Num_Chunk (this);
	filename = 0;

	object_array_size=0;
	object_array=0;
}


File_Chunk::~File_Chunk()
{
	delete[] filename;

	if (object_array)
		DeallocateMem(object_array);
}

#define SAVE_USING_COMPRESSION 1

BOOL File_Chunk::write_file (const char * fname)
{
	if(!fname) return FALSE;
	//if a read_only file exists with this filename , then abort attempt to save
	DWORD attributes = avp_GetFileAttributes(fname);
	if (0xffffffff!=attributes)
	{
		if (attributes & FILE_ATTRIBUTE_READONLY)
		{
			return FALSE;
		}
	}

	HANDLE rif_file;

	delete[] filename;

	filename = new char [strlen(fname) + 1];
	strcpy (filename, fname);
		
	//save under a temporary name in case a crash occurs during save;
	int filename_start_pos=0;
	int pos=0;
	while(fname[pos])
	{
		if(fname[pos]=='/' || fname[pos]==':')
		{
			filename_start_pos=pos+1;
		}
		//go to next MBCS character in string
		pos+=_mbclen((unsigned const char*)&fname[pos]);
	}
	if(!fname[filename_start_pos]) return FALSE;
	
	// temp_name = the directory part of fname + "~temp~" + the file name part + NUL
	size_t const fname_length = strlen(fname);
	char* temp_name=new char[fname_length+7];
	memcpy(temp_name,fname,filename_start_pos);
	memcpy(&temp_name[filename_start_pos],"~temp~",6);
	memcpy(&temp_name[filename_start_pos+6],&fname[filename_start_pos],fname_length-filename_start_pos+1);

	prepare_for_output();

	#if SAVE_USING_COMPRESSION
	//create a block containing the uncompressed rif file
	unsigned char* uncompressedData = (unsigned char*) make_data_block_from_chunk();
	if(!uncompressedData) return FALSE;

	//do the compression thing
	HuffmanPackage *outPackage = HuffmanCompression(uncompressedData,chunk_size);
	delete[] uncompressedData;
	if(!outPackage) return FALSE;

	//and now try to write the file
	rif_file = avp_CreateFile (temp_name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 
					FILE_FLAG_RANDOM_ACCESS, 0);

	if (rif_file == INVALID_HANDLE_VALUE)
	{
		delete[] temp_name;
		free (outPackage);
		return FALSE;
	}

	unsigned long junk;
	BOOL ok;

	ok = WriteFile (rif_file, (long *) outPackage,outPackage->CompressedDataSize+sizeof(HuffmanPackage), &junk, 0);

	CloseHandle (rif_file);

	free (outPackage);

	if(!ok) return FALSE;

	#else
	size_chunk();
	
	rif_file = CreateFileA (temp_name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 
					FILE_FLAG_RANDOM_ACCESS, 0);

	if (rif_file == INVALID_HANDLE_VALUE) {
		delete[] temp_name;
		return FALSE;
	}

	if (!(this->output_chunk(rif_file)))
	{
		CloseHandle (rif_file);
		delete[] temp_name;
		return FALSE;
	}

	CloseHandle (rif_file);
	#endif

	//Delete the old file with this name (if it exists) , and rename the temprary file
	DeleteFileA(fname);
	MoveFileA(temp_name,fname);

	delete[] temp_name;

	return TRUE;
}

// the file_chunk must link all of its shapes & objects together

void File_Chunk::post_input_processing()
{
	List<Shape_Chunk *> shplist;
	List<Object_Chunk *> objlist;

	List<Chunk *> child_lists;

	lookup_child("REBSHAPE",child_lists);

	while (child_lists.size())
	{
		shplist.add_entry((Shape_Chunk *)child_lists.first_entry());
		child_lists.delete_first_entry();
	}

	lookup_child("RBOBJECT",child_lists);

	while (child_lists.size())
	{
		objlist.add_entry((Object_Chunk *)child_lists.first_entry());
		child_lists.delete_first_entry();
	}

	// Next, each object is joined to its shape, by the number it gives. All of these numbers come from the level file, so
	// they are checked first. In all 86 of the game's own levels every shape and object has its header, and every object
	// names a shape in its own file. Anything else means the file is damaged, so the whole level is refused: error_code
	// makes load_rif stop with its error message. Leaving out just the bad object wouldn't be safe, because the code that
	// builds the level relies on every object having a shape.
	int highest_shape_number = 0;

	LIF<Shape_Chunk *> sli(&shplist);
	for (; !sli.done(); sli.next())
	{
		Shape_Header_Chunk * shape_header = sli()->get_header();

		if (!shape_header || !shape_header->file_id_num_is_valid() || !sli()->numbers_are_valid())
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			return;
		}

		highest_shape_number = max (highest_shape_number,shape_header->file_id_num);
		Shape_Chunk::max_id = max (Shape_Chunk::max_id,shape_header->file_id_num);
	}

	// a list with a place for every number up to the highest; the () makes every place start empty, so a number
	// that no shape uses reads as "no shape", not as whatever was in that memory before
	Shape_Chunk** shape_array=new Shape_Chunk*[highest_shape_number+1]();

	for(sli.restart();!sli.done();sli.next())
	{
		shape_array[sli()->get_header()->file_id_num]=sli();
	}

	for (LIF<Object_Chunk *> ol(&objlist); !ol.done(); ol.next())
	{
		Object_Header_Chunk * object_header = ol()->get_header();

		// Only an object number too big for the object list is refused. A negative one is allowed: the original tools
		// could leave -1 on an object that had no number yet, and build_object_array leaves those out of its list.
		if (!object_header || ol()->object_data.index_num > Object_Chunk::Max_Index_Num || object_header->shape_id_no < 0 ||
			object_header->shape_id_no > highest_shape_number || !shape_array[object_header->shape_id_no])
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			delete[] shape_array;
			return;
		}

		if (ol()->lookup_single_child ("MODULEDT") && !shape_array[object_header->shape_id_no]->points_fit_box())
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			delete[] shape_array;
			return;
		}

		ol()->assoc_with_shape(shape_array[object_header->shape_id_no]);
	}

	delete[] shape_array;

	Chunk_With_Children::post_input_processing();

	// Last, because this relies on what the chunks above have just checked for themselves
	if (!error_code)
	{
		List<Chunk *> hierarchies;
		lookup_child ("OBJCHIER",hierarchies);

		for (LIF<Chunk *> hierarchy(&hierarchies); !hierarchy.done(); hierarchy.next())
		{
			if (!((Object_Hierarchy_Chunk *)hierarchy())->can_be_built())
			{
				error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
				return;
			}
		}
	}
}


BOOL File_Chunk::update_file()
{
	if (!filename) return FALSE;

	// a copy, because write_file frees filename before it uses its argument; a level path can be 259 characters
	std::string tempname (filename);

	List<Shape_Chunk *> slist;
	list_shapes(&slist);
	List<Object_Chunk *> olist;
	list_objects(&olist);

	for (LIF<Shape_Chunk *> sli(&slist); !sli.done(); sli.next())
	{
		if (sli()->deleted)
		{
			delete sli();
		}
	}

	for (LIF<Object_Chunk *> oli(&olist); !oli.done(); oli.next())
	{
		if (oli()->deleted)
		{
			delete oli();
		}
	}

	return(write_file(tempname.c_str()));

}

void File_Chunk::list_objects(List<Object_Chunk *> * pList)
{
	Chunk * child_ptr = children;

	while (pList->size())
		pList->delete_first_entry();

	if (children)
	{
		while (child_ptr != NULL) 
		{
			if (strncmp ("RBOBJECT", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				pList->add_entry((Object_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}
}

void File_Chunk::list_shapes(List<Shape_Chunk *> * pList)
{
	Chunk * child_ptr = children;

	while (pList->size())
		pList->delete_first_entry();

	if (children)
	{
		while (child_ptr != NULL)
		{
			if (strncmp ("REBSHAPE", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				pList->add_entry((Shape_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}
}

void File_Chunk::list_dummy_objects(List<Dummy_Object_Chunk *> * pList)
{
	Chunk * child_ptr = children;
	
	while (pList->size())
	{
		pList->delete_first_entry();
	}

	if (children)
	{
		while (child_ptr != NULL)
		{
			if (strncmp ("DUMMYOBJ", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				pList->add_entry((Dummy_Object_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}
}

Environment_Data_Chunk * File_Chunk::get_env_data()
{
	List<Environment_Data_Chunk *> e_list;
	Chunk * child_ptr = children;

	if (children)
	{
		while (child_ptr != NULL)
		{
			if (strncmp ("REBENVDT", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				e_list.add_entry((Environment_Data_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}

	// There can be only ONE.
	assert (e_list.size() < 2);
	
	if (e_list.size())
		return e_list.first_entry();
	return 0;
}

void File_Chunk::build_object_array()
{
	List<Object_Chunk*> oblist;
	list_objects(&oblist);

	if (object_array)
	{
		DeallocateMem(object_array);
		object_array=0;
	}
	object_array_size=0;

	LIF<Object_Chunk*> oblif(&oblist);

	// find the highest object index. The numbers come from the level file, and a huge one would make the size sum below
	// wrap round to a tiny list, which the loop after it would then write far past. See index_num_is_valid.
	for (oblif.restart();!oblif.done();oblif.next())
	{
		if(oblif()->index_num_is_valid())
		{
			object_array_size=max(object_array_size,oblif()->object_data.index_num+1);
		}
	}

	if (object_array_size<=0) return;

	object_array = (Object_Chunk**) AllocArrayMem(object_array_size,sizeof(Object_Chunk*));
	for(int i=0;i<object_array_size;i++)
	{
		object_array[i]=0;
	}

	//now fill in object array

	for(oblif.restart();!oblif.done();oblif.next())
	{
		if(oblif()->index_num_is_valid())
		{
			object_array[oblif()->object_data.index_num]=oblif();
		}
	}
}

Object_Chunk* File_Chunk::get_object_by_index(int index)
{
	if (!object_array) build_object_array();
	if (index<0 || index>=object_array_size)return 0;
	return object_array[index];
}

/////////////////////////////////////////

// Class GodFather_Chunk functions

/*
Children for GodFather_Chunk :
"REBSHAPE"		Shape_Chunk
"RSPRITES"		AllSprites_Chunk
"RBOBJECT"		Object_Chunk
"RIFVERIN"		RIF_Version_Num_Chunk
"REBENVDT"		Environment_Data_Chunk
"REBENUMS"		Enum_Chunk
"OBJCHIER"		Object_Hierarchy_Chunk
"OBHALTSH"		Object_Hierarchy_Alternate_Shape_Set_Chunk

*/

GodFather_Chunk::GodFather_Chunk(char * buffer, size_t size)
: Chunk_With_Children (NULL, "REBINFF2")
{
	Parent_File = this;

	char * buffer_ptr = buffer;

	// The start of the first chunk

	while ((buffer_ptr-buffer)< ((signed)size-12) && !error_code) 
	{
		if (!ChildChunkFits(buffer_ptr, buffer, ((signed)size-12))) 
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(buffer_ptr);
		buffer_ptr += *(int *)(buffer_ptr + 8);
	}
}

/////////////////////////////////////////

// Class RIF_Version_Num_Chunk functions

RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST("RIFVERIN",RIF_Version_Num_Chunk,4)

void RIF_Version_Num_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = file_version_no;
}

/////////////////////////////////////////

// Class RIF_Name_Chunk functions
RIF_IMPLEMENT_DYNCREATE("RIFFNAME",RIF_Name_Chunk)

RIF_Name_Chunk::RIF_Name_Chunk (Chunk_With_Children * parent, const char * rname)
: Chunk (parent, "RIFFNAME")
{
	rif_name = new char [strlen(rname)+1];
	strcpy (rif_name, rname);
}

RIF_Name_Chunk::RIF_Name_Chunk (Chunk_With_Children * parent, const char * rdata, size_t const chunk_bytes)
: Chunk (parent, "RIFFNAME")
{
	rif_name = CopyNameFromChunk(rdata, chunk_bytes, rdata);
}

RIF_Name_Chunk::~RIF_Name_Chunk ()
{
	delete[] rif_name;
}


void RIF_Name_Chunk::fill_data_block (char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	memcpy (data_start, rif_name, strlen(rif_name) + 1);
}


///////////////////////////////////////

/*
Children for RIF_File_Chunk :
"REBSHAPE"		Shape_Chunk
"RSPRITES"		AllSprites_Chunk
"RBOBJECT"		Object_Chunk
"RIFVERIN"		RIF_Version_Num_Chunk
"REBENVDT"		Environment_Data_Chunk
"OBJCHIER"		Object_Hierarchy_Chunk
"OBHALTSH"		Object_Hierarchy_Alternate_Shape_Set_Chunk

*/


RIF_File_Chunk::RIF_File_Chunk (Chunk_With_Children * parent, const char * file_name)
: Chunk_With_Children (parent, "SUBRIFFL")
{
	char rifIsCompressed = FALSE;
	char *uncompressedData = NULL;
	HANDLE rif_file;
	DWORD file_size;
	DWORD file_size_from_file;
	unsigned long bytes_read;
	char * buffer;
	char * buffer_ptr;
	char id_buffer[9];


	Chunk * ParentFileStore = Parent_File;
	
	Parent_File = this;
	
	error_code = 0;
	
	rif_file = avp_CreateFile (file_name, GENERIC_READ, 0, 0, OPEN_EXISTING,
					FILE_FLAG_RANDOM_ACCESS, 0);

	if (rif_file == INVALID_HANDLE_VALUE)
	{
		error_code = CHUNK_FAILED_ON_LOAD;
		Parent_File = ParentFileStore;
		return;
	}

	file_size = GetFileSize(rif_file, NULL);

	if (!ReadFile(rif_file, id_buffer, 8, &bytes_read, 0))
	{
		error_code = CHUNK_FAILED_ON_LOAD;
		CloseHandle (rif_file);
		Parent_File = ParentFileStore;
		return;
	}

	//check for compressed rif
	if (!strncmp (id_buffer, COMPRESSED_RIF_IDENTIFIER, 8))
	{
		rifIsCompressed = TRUE;
	}
	else if (strncmp (id_buffer, "REBINFF2", 8))
	{
		error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		CloseHandle (rif_file);
		Parent_File = ParentFileStore;
		return;
	}

	buffer = new char [file_size + HUFFMAN_SPARE_BYTES];
	
	/* KJL 17:57:44 19/09/98 - if the rif is compressed, we must load the whole
	file in and then pass it to the decompression routine, which will return a
	pointer to the original data. */
	if (rifIsCompressed)
	{
		if (!ReadFile(rif_file, buffer+8, (file_size-8), &bytes_read, 0))
		{
			error_code = CHUNK_FAILED_ON_LOAD;
			CloseHandle (rif_file);
			Parent_File = ParentFileStore;
			delete[] buffer;
			return;
		}

		// the same as in File_Chunk: nothing else has looked at this file yet, so the decoder has to check it
		uncompressedData = HuffmanDecompressChecked((HuffmanPackage*)buffer, (int)file_size);

		if (!uncompressedData)
		{
			delete[] buffer;
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			CloseHandle (rif_file);
			Parent_File = ParentFileStore;
			return;
		}

		file_size = ((HuffmanPackage*)buffer)->UncompressedDataSize;

		delete[] buffer; // kill the buffer the compressed file was loaded into

		buffer_ptr = buffer = uncompressedData+12; // skip header data
	}
	else // the normal uncompressed approach:
	{
		//get the file size stored in the rif file
		if (!ReadFile(rif_file, &file_size_from_file, 4, &bytes_read, 0))
		{
			error_code = CHUNK_FAILED_ON_LOAD;
			CloseHandle (rif_file);
			Parent_File = ParentFileStore;
			delete[] buffer;
			return;
		}

		//and compare with the actual file size
		if (file_size != file_size_from_file)
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			CloseHandle (rif_file);
			Parent_File = ParentFileStore;
			delete[] buffer;
			return;
		}

		//read the rest of the file into the buffer
		memset (buffer, 0, file_size + HUFFMAN_SPARE_BYTES);

		if (!ReadFile(rif_file, buffer, (file_size-12), &bytes_read, 0))
		{
			error_code = CHUNK_FAILED_ON_LOAD;
			CloseHandle (rif_file);
			Parent_File = ParentFileStore;
			delete[] buffer;
			return;
		}
		buffer_ptr = buffer;
	}

	// Process the RIF

	// The start of the first chunk

	while ((buffer_ptr-buffer)< ((signed) file_size-12) && !error_code)
	{
		if (!ChildChunkFits(buffer_ptr, buffer, ((signed)file_size-12)))
		{
			error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(buffer_ptr);
		buffer_ptr += *(int *)(buffer_ptr + 8);
	}

	/* KJL 17:59:42 19/09/98 - release the memory holding the rif */
	if (rifIsCompressed)
	{
		free(uncompressedData);
	}
	else
	{
		delete[] buffer;
	}

	CloseHandle (rif_file);

	// after a failed load the tree is only part of a level, and this step relies on all of it being there
	if (!error_code)
	{
		post_input_processing();
	}

	Parent_File = ParentFileStore;
}

void RIF_File_Chunk::post_input_processing()
{
	List<Shape_Chunk *> shplist;
	List<Object_Chunk *> objlist;

	List<Chunk *> child_lists;

	lookup_child("REBSHAPE",child_lists);

	while (child_lists.size())
	{
		shplist.add_entry((Shape_Chunk *)child_lists.first_entry());
		child_lists.delete_first_entry();
	}

	lookup_child("RBOBJECT",child_lists);

	while (child_lists.size())
	{
		objlist.add_entry((Object_Chunk *)child_lists.first_entry());
		child_lists.delete_first_entry();
	}

	for (LIF<Object_Chunk *> ol(&objlist); !ol.done(); ol.next())
	{
		if (ol()->get_header())
		{
			
			for (LIF<Shape_Chunk *> sl(&shplist); !sl.done(); sl.next())
			{
				if (sl()->get_header())
				{
					if (sl()->get_header()->file_id_num == ol()->get_header()->shape_id_no)
					{
						ol()->assoc_with_shape(sl());
						break;
					}
				}
			}
		}
	}

	for (LIF<Shape_Chunk *> sli(&shplist); !sli.done(); sli.next())
	{
		// a level file can leave a shape's header out
		if (sli()->get_header())
		{
			Shape_Chunk::max_id = max (Shape_Chunk::max_id,sli()->get_header()->file_id_num);
		}
	}

	Chunk_With_Children::post_input_processing();
}


void RIF_File_Chunk::list_objects(List<Object_Chunk *> * pList)
{
	Chunk * child_ptr = children;
	
	while (pList->size())
	{
		pList->delete_first_entry();
	}

	if (children)
	{
		while (child_ptr != NULL)
		{
			if (strncmp ("RBOBJECT", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				pList->add_entry((Object_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}
}

void RIF_File_Chunk::list_shapes(List<Shape_Chunk *> * pList)
{
	Chunk * child_ptr = children;

	while (pList->size())
	{
		pList->delete_first_entry();
	}

	if (children)
	{
		while (child_ptr != NULL) 
		{
			if (strncmp ("REBSHAPE", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				pList->add_entry((Shape_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}
}

Environment_Data_Chunk * RIF_File_Chunk::get_env_data()
{
	List<Environment_Data_Chunk *> e_list;
	Chunk * child_ptr = children;

	if (children)
	{
		while (child_ptr != NULL) 
		{
			if (strncmp ("REBENVDT", child_ptr->identifier, 8) == 0)
			{
				assert (!child_ptr->r_u_miscellaneous());
				e_list.add_entry((Environment_Data_Chunk *)child_ptr);
			}
			child_ptr = child_ptr->next;
		}
	}

	// There can be only ONE.
	assert (e_list.size() < 2);
	
	if (e_list.size())
	{
		return e_list.first_entry();
	}

	return 0;
}
