// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed code and declarations only the level editor used: the game itself never runs them
//   a sequence starts with no frames, so one a broken list never reached is safe to free
//   a sequence checks that each of its frames has a place of its own
//   a sequence can say whether the game can play it

#ifndef _animobs_hpp
#define _animobs_hpp 1

#include "chunk.hpp"
#include "chunk_type.hpp"

class Object_Animation_Sequence_Header_Chunk;
class Object_Animation_Sequence_Frame_Chunk;
class Object_Animation_Sequence_Chunk;
class Hierarchy_Bounding_Box_Chunk;

class Object_Animation_Sequences_Chunk : public Chunk_With_Children 
{
public:

	Object_Animation_Sequences_Chunk (Chunk_With_Children * parent)
	: Chunk_With_Children (parent, "OBANSEQS")
	{}
	Object_Animation_Sequences_Chunk (Chunk_With_Children * const parent,const char *, const size_t);

	void list_sequences(List <Object_Animation_Sequence_Chunk *> * pList);
	
	Object_Animation_Sequence_Chunk * get_sequence (int num, int sub_num);

private:

	friend class Object_Chunk;
	friend class Object_Hierarchy_Chunk;


};




class Object_Animation_Sequence_Chunk : public Chunk_With_Children 
{
public:

	Object_Animation_Sequence_Chunk (Object_Animation_Sequences_Chunk * parent)
	: Chunk_With_Children (parent, "OBANSEQC")
	{}
	Object_Animation_Sequence_Chunk (Chunk_With_Children * const parent,const char *, const size_t);

	Object_Animation_Sequence_Header_Chunk * get_header();
	
	void get_frames(List <Object_Animation_Sequence_Frame_Chunk	*>* );

	// Refuses the level unless every frame has a place of its own. See the .cpp.
	virtual void post_input_processing();

	// Can the game play this sequence? Only asked once post_input_processing has checked every frame's place. See the .cpp.
	bool can_be_played();

	int get_sequence_time(); //gets time in ms
	int get_sequence_speed(); //gets movement speed in mm/second
	
	//get (normalised) direction of movement for sequence
	//if no direction has been set then returns false , and sets direction to a forward vector
	BOOL get_sequence_vector(ChunkVectorFloat* direction);

	//getting flags for this sequence
	int get_sequence_flags();

	Hierarchy_Bounding_Box_Chunk* get_bounding_box(); //gets the chunk with the bounding box for the sequence
private:

	friend class Object_Animation_Sequences_Chunk;

		
};



#define HierarchyFrameFlag_DeltaFrame 0x80000000
#define HierarchyFrame_SoundIndexMask 0x7f000000
#define HierarchyFrame_FlagMask 0x00ffffff

class Object_Animation_Sequence_Frame_Chunk : public Chunk 
{
public:

	Object_Animation_Sequence_Frame_Chunk (Chunk_With_Children * parent,const char *, size_t);

	~Object_Animation_Sequence_Frame_Chunk(){ delete[] extra_data; }
	
	ChunkQuat orientation;
	ChunkVectorInt transform;
	
	int32_t at_frame_no;  //frame start time (0-65535)
	
	signed long frame_ref_no; //frame index number
	 
	int flags;
	
	int num_extra_data;
	int* extra_data;

	virtual void fill_data_block (char *);
	virtual size_t size_chunk ()
	{
		return(chunk_size = 12 + 4*4 + 3*4 + 4 + 4 + 8+4*num_extra_data);
	}

	void set_sound_index(int ind);
	int get_sound_index(){return ((flags & HierarchyFrame_SoundIndexMask )>>24);}

private:

	friend class Object_Animation_Sequence_Chunk;


};




class Object_Animation_Sequence_Header_Chunk : public Chunk 
{
public:

	Object_Animation_Sequence_Header_Chunk (Chunk_With_Children * parent,const char *, size_t);
	
	~Object_Animation_Sequence_Header_Chunk();
	
	unsigned long num_frames;
	
	signed long sequence_number;
	signed long sub_sequence_number;
	
	int num_extra_data;
	int* extra_data;
	
	char * sequence_name;

	virtual void fill_data_block (char *);
	virtual size_t size_chunk ()
	{
		if (sequence_name)
		{
			size_t sequence_name_length = strlen(sequence_name);
			return(chunk_size = 12 + 16 + 4*num_extra_data + sequence_name_length + 4 - sequence_name_length%4);
		}
		else
		{
			return(chunk_size = 12 + 16 + 4*num_extra_data + 4);
		}
	}

private:

	friend class Object_Animation_Sequence_Chunk;


};

class Object_Animation_Sequence_Time_Chunk : public Chunk
{
public :
	Object_Animation_Sequence_Time_Chunk(Chunk_With_Children* parent)
	: Chunk (parent,"OBASEQTM") , sequence_time(0)
	{
	}
	Object_Animation_Sequence_Time_Chunk (Chunk_With_Children * parent,const char *, size_t);

	unsigned int sequence_time; //in milliseconds
	
	virtual void fill_data_block (char *);
	virtual size_t size_chunk (){ return chunk_size=16;}

private:

	friend class Object_Animation_Sequence_Chunk;


};


class Object_Animation_Sequence_Speed_Chunk : public Chunk
{
public :
	Object_Animation_Sequence_Speed_Chunk(Chunk_With_Children* parent)
	: Chunk (parent,"OBASEQSP") , sequence_speed(0) ,angle(0),spare(0)
	{
	}
	Object_Animation_Sequence_Speed_Chunk (Chunk_With_Children * parent,const char *, size_t);

	int sequence_speed; //in mm/second
	int angle; //in degrees (mainly for tools use)
	int spare;
	
	virtual void fill_data_block (char *);
	virtual size_t size_chunk (){ return chunk_size=24;}

private:

	friend class Object_Animation_Sequence_Chunk;


};

#define MummySequenceFlag_UpperSequence		0x00000001
#define MummySequenceFlag_LowerSequence		0x00000002
#define SequenceFlag_Loops					0x00000004
#define SequenceFlag_NoLoop					0x00000008
#define SequenceFlag_NoInterpolation		0x00000010
#define SequenceFlag_HalfFrameRate			0x00000020

class Object_Animation_Sequence_Flags_Chunk : public Chunk
{
public :
	Object_Animation_Sequence_Flags_Chunk (Chunk_With_Children * parent,const char *, size_t);

	int flags;
	
	virtual void fill_data_block (char *);
	virtual size_t size_chunk (){ return chunk_size=16;}
	
};


struct Object_Animation_Frame
{
	ChunkQuat orientation;
	ChunkVectorInt transform;
	signed long at_frame_no;  //frame start time (0-65535)
	int flags;

	int get_sound_index(){return ((flags & HierarchyFrame_SoundIndexMask )>>24);}

};

struct Object_Animation_Sequence
{
	// A file's list can stop early, and the sequences it didn't reach still get freed, so each starts with nothing to free.
	Object_Animation_Sequence() : num_frames(0), frames(0) {}
	~Object_Animation_Sequence() {delete[] frames;}

	unsigned long num_frames;
	signed long sequence_number;
	signed long sub_sequence_number;
	unsigned int sequence_time; //in milliseconds
	Object_Animation_Frame* frames;
};

#define Get_Object_Animation_All_Sequence_Chunk(parent) (Object_Animation_All_Sequence_Chunk*)(parent)->lookup_single_child("OBANALLS")

//a more compact version of the sequence and frame data
//this format isn't recognized by any of the tools however.
class Object_Animation_All_Sequence_Chunk : public Chunk
{
public:

	Object_Animation_All_Sequence_Chunk (Chunk_With_Children * parent);
	Object_Animation_All_Sequence_Chunk (Chunk_With_Children * const parent,const char *, const size_t);
	~Object_Animation_All_Sequence_Chunk () { delete[] sequences;}

	virtual void fill_data_block (char *);
	virtual size_t size_chunk ();
	
	int num_sequences;
	Object_Animation_Sequence* sequences;

	// Can the game play all of these? See the .cpp.
	bool sequences_can_be_played();

};
#endif