// Chunk library

// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed a group of helper macros that nothing used
//   added ChildChunkFits, which checks that a child chunk's size from the file can be real
//   added CountFitsChunk and NameLengthInChunk, which check a count and a name from the file against the room left in a chunk

#ifndef _chunk_hpp
#define _chunk_hpp

	#include <string.h>
	#include "3dc.h"
	#include "mem3dc.h" // for debug new and delete
	#include "inline.h"

#if SupportModules
	#include "module.h"
#endif
	#include "list_tem.hpp"

#if objedit || sprite_edit || ANIMEXP
#include "stdafx.h"
#include "list_tem.hpp"
#endif

#if shpedit
#include "stdafx.h"
#include "list_tem.hpp"
#endif

#define CHUNK_FAILED_ON_LOAD -1
#define CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED -2
#define CHUNK_FAILED_ON_WRITE -3
#define CHECK_FAILED_NOT_OPEN -4

#define DisableLock 1

#define GENERAL_FLAG_LOCKED 0x0000001

// Non class functions ( only one as yet )

// The function will return a list of file pointers
// for the starting position of each chunk within a chunk
// File pointers are offset from the start of the file.

// we start at the header of the chunk we are in
// so that we can stop at the end of the chunk

#ifndef RIFF_OPTIMIZE // define this to get compiler errors where you are calling the old slow functions
extern List<int> list_chunks_in_file (HANDLE &, const char * chunk_id);
#endif
extern void list_chunks_in_file (List<int> * pList, HANDLE, const char * chunk_id);

// Structures for interfacing with the outside application
// these are going to be basically very C-ee so that C
// functions can also read them



// The basic chunk class structure is as follows
//
//											Base_Chunk
//											|				|
//		Chunk_With_Children				Data Chunks
// 			|
// 	File_Chunk
//

// Most chunk classes are either derived from the Chunk
// class or the Chunk_With_Children class.

// A chunk with data is derived from the Chunk class as follows

//	class Sample_Data_Chunk : public Chunk
//	{
// 	public:
//
//	// Constructors are placed either here or may be private
//	// The constructors for most shape data chunks are private
//	// as only the Shape_Chunk can call them.  This is because
//	// the shape chunk deals with the data.
//	//
//	// Any constuctor should initialise class chunk with the
//	// correct identifier for the current chunk.
//	//
//	// There are normally two constructors - one for constructing
//	// from interface data one for constructing from raw data.
//	//
//	// A destructor can also be placed here or may not be needed
//	//
//	// Any variables that are made available for the user should also
//	// be placed here.
//	//
//	// The next three functions are vital for the io functions
//	//
//	virtual BOOL output_chunk (HANDLE &hand);
//	// This function will write the chunk data to the file specified
//	// by the windows file handle - hand.
//	//
//	virtual size_t size_chunk ();
//	// This function will return the size of the chunk (including the header).
//	// IT MUST SET the variable chunk_size to the size it returns
//	//
//	virtual fill_data_block (char * data_start);
//	// This function will fill a data block with data.  The data will be
//	// the same as the file data.  The data is assumed to be pre-allocated
//	// with the size given by the size_chunk function.
//	//
//	}

// A chunk with children is derived from the Chunk_With_Children class
// as follows
//
//	class Sample_Chunk_With_Children : public Chunk
//	{
//	public:
//
//	// Constructors may be used to construct child chunks from
//	// interface data. A parsing constructor should be used for
//	// constructing from raw data (look at Shape_Chunk).
//	//
//	// Any constructor should initialise class Chunk_With_Children with
//	// the correct identifier for the current chunk.
//	//
//	// The Destructor does not need to destroy child chunks as the
//	// Chunk_With_Children destructor will automatically do this.
//	//
//	// The three functions (size_chunk, output_chunk and fill_data_block)
//	// are not needed as Chunk_With_Children can deal with these - but may
//	// be put in.
//	//
//	}

//
//


// The logic behind the locking is as follows.

// There are two functions for locking and unlocking chunks
// these are currently only in the shape and object chunks

// lock_chunk(File_Chunk &)
// will lock a chunk, this must only be called once
// and will return false if it tries to lock a chunk
// that has already been locked by anyone.

// unlock_chunk (File_Chunk &, BOOL updatedyn)
// will unlock the chunk locally and in the file if it is not to be updated
// If it is to be updated, it will set the updated flag and
// the chunk can only be locked again once it is written to a file.

// The user may call File_Chunk::update_file() whenever
// (either after every chunk update, or once a minute)
// Note that this fully unlocks locally locked chunks
// that are being written to the file.

// Another function File_Chunk::update_chunks_from_file()
// will reload those chunks that have been externally upadted
// This must be done with care!!

// The objects are associated with shapes primarily
// When a shape is being edited, a list of objects will
// be with that shape in the header.  These objects will be locked
// also.



///////////////////////////////////////////////

class Chunk
{
public:

	//destructor
	virtual ~Chunk ();

	// constructors
	Chunk (class Chunk_With_Children * parent, const char * ident);

	virtual size_t size_chunk () = 0;

	virtual BOOL output_chunk (HANDLE &);

	virtual void fill_data_block (char * data_start) = 0;

	// this function is virtual, but will probably not be used elsewhere
	virtual char * make_data_block_from_chunk ();


	// Selective output functions, these are similar to the normal ones
	// and will normally call the equivalents.
	// special chunks will have there own functions which will only respond
	// if they have a flag set

	virtual char * make_data_block_for_process();

	virtual size_t size_chunk_for_process();

	virtual void fill_data_block_for_process(char * data_start);

	// these functions are virtual, but will only be used sparingly

	virtual void prepare_for_output()
	{}

	virtual void post_input_processing()
	{}

	const char * identifier;

	int error_code;

	// this allows chunks with children to trap miscellaneous chunks
	// that shouldn't be miscellaneous !!!
	virtual BOOL r_u_miscellaneous()
	{ return FALSE; }

	static void Register(const char* idChunk,const char* idParent ,Chunk * (* pfnCreate) (Chunk_With_Children* parent,const char* data) );

private:

	// copy - private to stop chunks
	// from being copied
	Chunk (const Chunk &);
	// ditto
	void operator=(const Chunk &);

	// pointers to siblings

	friend class Chunk_With_Children;
	friend class Sprite_Header_Chunk; // sprite updating needs to scan through all child chunks
	friend class File_Chunk;
	friend class RIF_File_Chunk;
	friend class Shape_Chunk;

	Chunk * next;
	Chunk * previous;

	// identifier store

	char identifier_store[9];

protected:

	size_t chunk_size;

	// pointer to parent
	class Chunk_With_Children * parent;

public :

	class Chunk_With_Children * GetRootChunk(void);
	class Chunk_With_Children const * GetRootChunk(void) const;

};


///////////////////////////////////////////////

class Miscellaneous_Chunk : public Chunk
{
public:

	Miscellaneous_Chunk (Chunk_With_Children * parent, const char * identifier,
									 const char * _data, size_t _data_size);


	virtual ~Miscellaneous_Chunk ();

	virtual size_t size_chunk ()
	{
		return (chunk_size = (data_size + 12));
	}

	virtual void fill_data_block (char * data_start);

	const size_t data_size;
	const char * const data;

	// this allows chunks with children to trap miscellaneous chunks
	// that shouldn't be miscellaneous !!!
	virtual BOOL r_u_miscellaneous()
	{ return TRUE; }

private:

	char * data_store;

};


///////////////////////////////////////////////

class Chunk_With_Children : public Chunk
{

public:

	virtual ~Chunk_With_Children();

	Chunk_With_Children (Chunk_With_Children * parent, const char * identifier)
		: Chunk (parent, identifier), children (NULL) {}

	virtual size_t size_chunk ();

	virtual BOOL output_chunk (HANDLE &);

	virtual void fill_data_block (char * data_start);

	virtual void prepare_for_output();

	virtual void post_input_processing();

	// look for child chunk(s)
	#ifndef RIFF_OPTIMIZE // define this to get compiler errors where you are calling the old slow functions
	List<Chunk *> lookup_child (const char *) const;
	#endif
	void lookup_child (const char *,List<Chunk*>&) const;
	Chunk* lookup_single_child(const char*) const;
	unsigned count_children(char const *) const;

	// Selective output functions, these are similar to the normal ones
	// and will normally call the equivalents.
	// special chunks will have there own functions which will only respond
	// if they have a flag set

	virtual size_t size_chunk_for_process();

	virtual void fill_data_block_for_process(char * data_start);


	Chunk* Chunk_With_Children::DynCreate(const char* data);

protected:

	friend class Chunk;
	friend class File_Chunk;

	// points to a doubly linked list
	Chunk * children;

};

/////////////////////////////////////////////
// the file chunk being loaded, for the loaders' error codes

extern Chunk * Parent_File;

// A chunk's children sit one after another inside it. Each child starts with a 12-byte header, an 8-byte name and then
// its own size, and that size counts the header. All of this comes from the file, so before a child is used the
// loaders ask this function whether it can be real: there must be room left for a header, the size must at least hold
// the header, and the child must end inside its parent. A size below 12 would make the loaders walk backwards or spin
// on the same spot, and would hand the child a "size minus 12" that wraps round to about 4 GB.
// child is where the child starts, start is where the parent's data starts, total is the size of the parent's data.
inline bool ChildChunkFits(char const * child, char const * start, signed int total)
{
	signed int const bytes_left = total - (signed int)(child - start);

	if (bytes_left < 12)
	{
		return false;
	}

	signed int const child_size = *(int const *)(child + 8);

	return child_size >= 12 && child_size <= bytes_left;
}

// Is there room in bytes_left for count items of item_bytes each? A count comes from the file, so it can be negative, and it can be so big
// that the sum wraps round in a 32-bit number and looks small. A negative count is refused, and this sum is done in numbers wide enough
// not to wrap. Ask this before making a list of that size: asking for too much memory ends the game, and a chunk's own size can't be
// too big for the file it came from.
inline bool CountFitsChunk(int count, size_t item_bytes, size_t bytes_left)
{
	return count >= 0 && (unsigned long long) count * item_bytes <= bytes_left;
}

// How long is the name that starts here? A name in a level file ends at a zero byte, and strlen reads until it finds one, so a name with no
// zero byte would run off the end of the chunk. This only looks at the bytes_left bytes it's given, and says -1 if there's no zero in them.
inline int NameLengthInChunk(char const * name, size_t bytes_left)
{
	char const * const zero = (char const *) memchr(name, 0, bytes_left);

	return zero ? (int)(zero - name) : -1;
}

///////////////////////////////////////////////
//macros for use in chunk construction from buffer, assume buffer is called data

//read variable of type 'type'
#define CHUNK_EXTRACT(var,type) \
	var=*(type*)data; \
	data+=sizeof(type);

//read 4 byte aligned string
#define CHUNK_EXTRACT_STRING(var) { \
	int __length=strlen(data); \
	if(__length) \
	{ \
		var=new char[__length+1]; \
		strcpy(var,data); \
	} \
	else var=0; \
	data+=(__length+4) &~3 ;}

//read array
//length is an int (filled in by macro)
//pointer is a pointer of type 'type'
#define CHUNK_EXTRACT_ARRAY(length,pointer,type){\
	CHUNK_EXTRACT(length,int) \
	if(length) \
	{ \
		pointer=new type[length]; \
		for(int __i=0;__i<length;__i++) \
		{ \
			CHUNK_EXTRACT(pointer[__i],type)	\
		} \
	} \
	else pointer=0;}


//macros for use in fill_data_block

#define CHUNK_FILL_START \
	strncpy (data, identifier, 8); \
	data += 8; \
	*(int *) data = chunk_size; \
	data += 4;


//write variable of type 'type'
#define CHUNK_FILL(var,type) \
	*(type*)data=var; \
	data+=sizeof(type);

//write 4 byte aligned string
#define CHUNK_FILL_STRING(var) \
	if(var) strcpy(data,var); \
	else *data=0; \
	data+=(strlen(data)+4)&~3;

#define CHUNK_FILL_ARRAY(length,pointer,type){\
	CHUNK_FILL(length,int) \
	for(int __i=0;__i<length;__i++)	\
	{ \
		CHUNK_FILL(pointer[__i],type); \
	}	\
}

//macros for use in size_chunk

#define CHUNK_SIZE_START \
	chunk_size = 12


//size of variable of type 'type'
#define CHUNK_SIZE(var,type) \
	+ sizeof(type)

//size of 4 byte aligned string
#define CHUNK_SIZE_STRING(string) \
	+ (string ? (strlen(string)+4)&~3 : 4)

#define CHUNK_SIZE_ARRAY(length,pointer,type) \
	+ sizeof(int) \
	+ (length * sizeof(type))

#define CHUNK_SIZE_END ;

///////////////////////////////////////////////

class GodFather_Chunk : public Chunk_With_Children
{
public:

	GodFather_Chunk ()
	: Chunk_With_Children (NULL, "REBINFF2")
	{}

	GodFather_Chunk (char * buffer, size_t size);
};


///////////////////////////////////////////////

/*
Macro for adding chunk class to list of that can be created using DynCreate
Chunks registered this way can be created as a child of any chunk
*/

#define RIF_IMPLEMENT_DYNCREATE(idChunk,tokenClassName) _RIF_IMPLEMENT_DYNCREATE_LINE_EX(idChunk,tokenClassName,__LINE__)
#define _RIF_IMPLEMENT_DYNCREATE_LINE_EX(idChunk,tokenClassName,nLine) _RIF_IMPLEMENT_DYNCREATE_LINE(idChunk,tokenClassName,nLine)

#define _RIF_IMPLEMENT_DYNCREATE_LINE(idChunk,tokenClassName,nLine) \
static Chunk * RifCreateClassObject ## tokenClassName ##_## nLine (Chunk_With_Children* parent,const char* data) { \
	Chunk * pChunk = new tokenClassName(parent,data+12,(*(int *) (data + 8))-12); \
	return pChunk; \
} \
 class RegisterRifChunkClass ## tokenClassName ##_## nLine { \
	public: RegisterRifChunkClass ## tokenClassName ##_## nLine () { \
		Chunk::Register(idChunk ,0,RifCreateClassObject ## tokenClassName ##_## nLine); \
	} \
} rifcc ## tokenClassName ##_## nLine;

/*
Macro for adding chunk class to list of that can be created using DynCreate.
Chunks registered this way will only be created as children of the parent named
*/


#define RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT(idChunk,tokenClassName,idParent,parentClassName) _RIF_IMPLEMENT_DYNCREATE_PARENT_LINE_EX(idChunk,tokenClassName,idParent,parentClassName,__LINE__)
#define _RIF_IMPLEMENT_DYNCREATE_PARENT_LINE_EX(idChunk,tokenClassName,idParent,parentClassName,nLine) _RIF_IMPLEMENT_DYNCREATE_PARENT_LINE(idChunk,tokenClassName,idParent,parentClassName,nLine)

#define _RIF_IMPLEMENT_DYNCREATE_PARENT_LINE(idChunk,tokenClassName,idParent,parentClassName,nLine) \
static Chunk * RifCreateClassObject ## tokenClassName ##_## nLine (Chunk_With_Children* parent,const char* data) { \
	Chunk * pChunk = new tokenClassName((parentClassName*)parent,data+12,(*(int *) (data + 8))-12); \
	return pChunk; \
} \
 class RegisterRifChunkClass ## tokenClassName ##_## nLine { \
	public: RegisterRifChunkClass ## tokenClassName ##_## nLine () { \
		Chunk::Register(idChunk ,idParent,RifCreateClassObject ## tokenClassName ##_## nLine); \
	} \
} rifcc ## tokenClassName ##_## nLine;


/*
Load from buffer function for standard Chunk_With_Children
*/
#define CHUNK_WITH_CHILDREN_LOADER(id,chunk_class) \
chunk_class::chunk_class(Chunk_With_Children * const parent, char const * __data, size_t const __size)\
:Chunk_With_Children(parent,id)\
{\
	const char * __buffer_ptr = __data;\
	while ((__data-__buffer_ptr)< (signed) __size) {\
		if (!ChildChunkFits(__data, __buffer_ptr, (signed) __size)) {\
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;\
			break;\
		}\
		\
		DynCreate(__data);\
		__data += *(int *)(__data + 8);\
	}\
}
/*
Load from buffer function for standard Lockable_Chunk_With_Children
*/

#define LOCKABLE_CHUNK_WITH_CHILDREN_LOADER(id,chunk_class) \
chunk_class::chunk_class(Chunk_With_Children * const parent,char const * __data, size_t const __size)\
:Lockable_Chunk_With_Children(parent,id)\
{\
	const char * __buffer_ptr = __data;\
	while ((__data-__buffer_ptr)< (signed) __size) {\
		if (!ChildChunkFits(__data, __buffer_ptr, (signed) __size)) {\
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;\
			break;\
		}\
		\
		DynCreate(__data);\
		__data += *(int *)(__data + 8);\
	}\
}


//macros for forcing inclusion of chunk files from a library
#define FORCE_CHUNK_INCLUDE_START //not needed anymore

#define FORCE_CHUNK_INCLUDE(filename)\
	extern int __Chunk_Include_##filename;\
	int* p__Chunk_Include_##filename =& __Chunk_Include_##filename;

#define FORCE_CHUNK_INCLUDE_END //not needed anymore


#define FORCE_CHUNK_INCLUDE_IMPLEMENT(filename) int __Chunk_Include_##filename;

/*
//eg.
FORCE_CHUNK_INCLUDE_START
FORCE_CHUNK_INCLUDE(mishchnk)
FORCE_CHUNK_INCLUDE(shpchunk)
FORCE_CHUNK_INCLUDE(obchunk)
FORCE_CHUNK_INCLUDE(envchunk)
FORCE_CHUNK_INCLUDE(animchnk)
FORCE_CHUNK_INCLUDE(hierchnk)
FORCE_CHUNK_INCLUDE(animobs)
FORCE_CHUNK_INCLUDE(sndchunk)
FORCE_CHUNK_INCLUDE(avpchunk)
FORCE_CHUNK_INCLUDE(bmpnames)
FORCE_CHUNK_INCLUDE(chunkpal)
FORCE_CHUNK_INCLUDE(dummyobjectchunk)
FORCE_CHUNK_INCLUDE(enumchnk)
FORCE_CHUNK_INCLUDE(enumsch)
FORCE_CHUNK_INCLUDE(fragchnk)
FORCE_CHUNK_INCLUDE(gsprchnk)
FORCE_CHUNK_INCLUDE(hierplace)
FORCE_CHUNK_INCLUDE(ltchunk)
FORCE_CHUNK_INCLUDE(oechunk)
FORCE_CHUNK_INCLUDE(pathchnk)
FORCE_CHUNK_INCLUDE(sprchunk)
FORCE_CHUNK_INCLUDE(strachnk)
FORCE_CHUNK_INCLUDE(toolchnk)
FORCE_CHUNK_INCLUDE(wpchunk)
FORCE_CHUNK_INCLUDE_END
*/

#endif // !included

