// Modified 2026-11-04 by Melanikus for the AvP port:
//   a chunk of texture animation has to fit its own size and the game's limits before any of it is believed, and an animation can
//   say whether every picture it shows is a real one
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   the polygon list is asked for by count, so a number from a level file cannot make the size sum wrap round
#include "animation_chunk.hpp"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(animation_chunk)

RIF_IMPLEMENT_DYNCREATE("TEXTANIM",Animation_Chunk)

Animation_Chunk::Animation_Chunk(Chunk_With_Children* parent)
: Chunk(parent, "TEXTANIM")
{
	NumPolys=0;
	AnimList=0;
}
// How a chunk of texture animation is laid out. It's lists inside lists: a count of polygons, then each polygon's six numbers and its
// sequences, then each sequence's five numbers and its frames. A frame is one texture number, then two numbers for each corner.
static size_t const animation_polygon_bytes = 24;
static size_t const animation_sequence_bytes = 20;

// chunk_load.cpp goes straight to the six numbers of each frame, so every animated polygon has to be a triangle. Each of the 3552 in the
// game's own levels is.
static int const animation_corners = 3;
static size_t const animation_frame_bytes = sizeof(int) + 2 * sizeof(int) * animation_corners;

// The game keeps a count of frames in the top half of a number (txa_maxframe, in chunk_load.cpp), so it can't count more than this. The most
// in any sequence in the game's own levels is 6.
static int const most_animation_frames = 32767;

// Walks the whole chunk the way the constructor does, believing nothing until it has checked it, so the constructor can read it without
// looking. Every number here comes from the level file. All 418 of these chunks in the game's own levels end exactly at their end.
// - A count of polygons is checked as the walk reaches each one, so it only needs to be not negative. A big one soon runs out of room.
// - A sequence with no frames would have the game read the first frame of an empty list, and a polygon with no sequences would leave the
//   game a sequence that's only an empty place, which it goes on to read while the level runs.
// - The game normally adds a frame that leads back to the first, and this flag leaves it out. With only one frame that leaves nothing to move
//   through, and the game would work out frame number -1 (kshape.cpp, where it clamps the frame).
static bool animation_fits_chunk (char const * data, size_t bytes_left)
{
	if (bytes_left < sizeof(int))
	{
		return false;
	}

	int const polygons = *(int const *) data;
	data += sizeof(int);
	bytes_left -= sizeof(int);

	if (polygons < 0)
	{
		return false;
	}

	for (int i = 0; i < polygons; i++)
	{
		if (bytes_left < animation_polygon_bytes)
		{
			return false;
		}

		int const sequences = *(int const *) (data + 8);
		int const corners = *(int const *) (data + 12);
		data += animation_polygon_bytes;
		bytes_left -= animation_polygon_bytes;

		if (corners != animation_corners || sequences < 1)
		{
			return false;
		}

		for (int j = 0; j < sequences; j++)
		{
			if (bytes_left < animation_sequence_bytes)
			{
				return false;
			}

			int const flags = *(int const *) (data + 4);
			int const frames = *(int const *) (data + 8);
			data += animation_sequence_bytes;
			bytes_left -= animation_sequence_bytes;

			if (frames < 1 || frames > most_animation_frames || !CountFitsChunk (frames, animation_frame_bytes, bytes_left))
			{
				return false;
			}

			if (frames == 1 && (flags & txa_flag_nointerptofirst))
			{
				return false;
			}

			data += frames * animation_frame_bytes;
			bytes_left -= frames * animation_frame_bytes;
		}
	}

	return bytes_left == 0;
}

Animation_Chunk::Animation_Chunk(Chunk_With_Children* parent,const char* data,size_t datasize)
: Chunk(parent, "TEXTANIM")
{
	NumPolys=0;
	AnimList=0;

	if (!animation_fits_chunk (data, datasize))
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	NumPolys=*((int*)data);
	data+=4;
	if(NumPolys)
	{
		AnimList=(TEXANIM**)AllocArrayMem(NumPolys,sizeof(TEXANIM*));
		for(int i=0;i<NumPolys;i++)
		{
			AnimList[i]=new TEXANIM;
			TEXANIM* ta=AnimList[i];
			ta->poly=*((int*)data);
			data+=4;
			ta->ID=*((int*)data);
			data+=4;
			ta->NumSeq=*((int*)data);
			data+=4;
			ta->NumVerts=*((int*)data);
			data+=4;
			ta->AnimFlags=*((int*)data);
			data+=4;
			ta->Identifier=*((int*)data);
			data+=4;
			ta->CurSeq=0;
			ta->Seq=new FrameList*[ta->NumSeq];
			for(int j=0;j<ta->NumSeq;j++)
			{
				ta->Seq[j]=new FrameList(ta);
				FrameList* fl=ta->Seq[j];
				fl->Speed=*((int*)data);
				data+=4;
				fl->Flags=*((int*)data);
				data+=4;
	
				fl->NumFrames=*((int*)data);
				data+=4;
				fl->spare1=*((int*)data);
				data+=4;
				fl->spare2=*((int*)data);
				data+=4;
				fl->CurFrame=0;
				
				fl->Textures=new int[fl->NumFrames];
				fl->UVCoords=new int[(2*ta->NumVerts)*fl->NumFrames];
				for(int k=0;k<fl->NumFrames;k++)
				{
					fl->Textures[k]=*((int*)data);
					data+=4;
				}
				for(int k=0;k<(2*ta->NumVerts)*fl->NumFrames;k++)
				{
					fl->UVCoords[k]=*((int*)data);
					data+=4;
				}
			}

		}	
	}
	else
		AnimList=0;
	
}

Animation_Chunk::~Animation_Chunk()
{
	for(int i=0;i<NumPolys;i++)
	{
		delete AnimList[i];
	}
	if(AnimList) DeallocateMem(AnimList);
}


size_t Animation_Chunk::size_chunk()
{
	chunk_size=12+4;
	for(int i=0;i<NumPolys;i++)
	{
		chunk_size+=24;
		TEXANIM* ta=AnimList[i];
		for(int j=0;j<ta->NumSeq;j++)
		{
			chunk_size+=20;
			chunk_size+=4*(1+2*ta->NumVerts)*ta->Seq[j]->NumFrames;
		}
	}
	return chunk_size;
}
	
BOOL Animation_Chunk::output_chunk (HANDLE &hand)
{
	unsigned long junk;
	BOOL ok;
	char * data_block;

	data_block = this->make_data_block_from_chunk();

	ok = WriteFile (hand, (long *) data_block, (unsigned long) chunk_size, &junk, 0);

	delete[] data_block;

	if (!ok) return FALSE;

	return TRUE;
}

void Animation_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start=NumPolys;
	data_start+=4;
	for(int i=0;i<NumPolys;i++)
	{
		TEXANIM* ta=AnimList[i];
		*(int*)data_start=ta->poly;		
		data_start+=4;
		*(int*)data_start=ta->ID;		
		data_start+=4;
		*(int*)data_start=ta->NumSeq;		
		data_start+=4;
		*(int*)data_start=ta->NumVerts;		
		data_start+=4;
		*(int*)data_start=ta->AnimFlags;		
		data_start+=4;
		*(int*)data_start=ta->Identifier;		
		data_start+=4;

		for(int j=0;j<ta->NumSeq;j++)
		{
			FrameList* fl=ta->Seq[j];
			*(int*)data_start=fl->Speed;		
			data_start+=4;
			*(int*)data_start=fl->Flags;		
			data_start+=4;
			*(int*)data_start=fl->NumFrames;		
			data_start+=4;
			*(int*)data_start=fl->spare1;		
			data_start+=4;
			*(int*)data_start=fl->spare2;		
			data_start+=4;
			for(int k=0;k<fl->NumFrames;k++)
			{
				*(int*)data_start=fl->Textures[k];		
				data_start+=4;
			}
			for(int k=0;k<(2*ta->NumVerts)*fl->NumFrames;k++)
			{
				*(int*)data_start=fl->UVCoords[k];		
				data_start+=4;
			}
		}
	}
}

FrameList::FrameList(TEXANIM* p)
{
	Speed=65536;
	Flags=0;
	NumFrames=0;
	CurFrame=-1;
	parent=p;
	Textures=0;
	UVCoords=0;
	spare1=spare2=0;
}
FrameList::FrameList(TEXANIM* p,FrameList* fl,int* conv)
{
	Speed=fl->Speed;
	Flags=fl->Flags;
	NumFrames=fl->NumFrames;
	parent=p;
	Textures=new int[NumFrames];
	UVCoords=new int[NumFrames*2*p->NumVerts];
	spare1=fl->spare1;
	spare2=fl->spare2;
	if(conv)
	{
		for(int i=0;i<NumFrames;i++)
		{
			Textures[i]=conv[fl->Textures[i]];
		}
	}
	else
	{
		for(int i=0;i<NumFrames;i++)
		{
			Textures[i]=fl->Textures[i];
		}
	}
	for(int i=0;i<NumFrames*2*p->NumVerts;i++)
	{
		UVCoords[i]=fl->UVCoords[i];
	}
	CurFrame=0;
}
FrameList::~FrameList()
{
	delete[] Textures;
	delete[] UVCoords;
}

TEXANIM::TEXANIM()
{
	shape=0;
	NumSeq=0;
	CurSeq=-1;
	Seq=0;
	NumVerts=3;
	AnimFlags=Identifier=0;
}
TEXANIM::TEXANIM(TEXANIM* ta)
{
	shape=0;
	NumSeq=0;
	CurSeq=-1;
	Seq=0;
	NumVerts=3;
	AnimFlags=Identifier=0;
	CopyAnimData(ta,0);
}

TEXANIM::~TEXANIM()
{
	for(int i=0;i<NumSeq;i++)
	{
		delete Seq[i];
	}
	delete[] Seq;	
}

// Are the pictures all real? image_numbers has a place for each texture number up to max_index, holding its picture, or -1 if there's none.
bool TEXANIM::ImagesAreKnown(int max_index, int const * image_numbers) const
{
	if (!image_numbers)
	{
		return false;
	}

	for (int i = 0; i < NumSeq; i++)
	{
		FrameList const * sequence = Seq[i];

		for (int j = 0; j < sequence->NumFrames; j++)
		{
			int const texture = sequence->Textures[j];

			if (texture < 0 || texture > max_index || image_numbers[texture] == -1)
			{
				return false;
			}
		}
	}

	return true;
}

void TEXANIM::CopyAnimData(TEXANIM* ta,int*conv)
{
	shape=ta->shape;
	poly=ta->poly;
	ID=ta->ID;
	NumSeq=ta->NumSeq;
	CurSeq=0;
	Seq=new FrameList*[NumSeq];
	NumVerts=ta->NumVerts;
	AnimFlags=ta->AnimFlags;
	Identifier=ta->Identifier;
	for(int i=0;i<NumSeq;i++)
	{
		Seq[i]=new FrameList(this,ta->Seq[i],conv);	
	}

}

