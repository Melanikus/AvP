// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed a large amount of code only the level editor used: the game itself never runs it;
//   text is copied using its exact measured length, so it always fits
//   a child chunk whose size cannot be real is refused
//   added a check on how many points a shape claims
//   a polygon's corners are limited to the 4 places it has for them, and each must be one of the shape's points;
//   sub-shapes get the same checks, and a morphing shape must fit the morph list and match its frames
//   removed the class that read a shape stored in a level as a raw copy of memory, because nothing could check it
//   a polygon needs at least 3 corners; a shape can only have one list of points and one of polygons, and a shape with no
//   points chunk can't claim any; merge data and animations are checked against the shape; a sub-shape that fails stops
//   lists of texture coordinates are read within their chunk and their 4 places, and each textured polygon's list is checked
//   a shape must have one normal for each point and one for each polygon, each normal's numbers must be sensible, and a polygon's
//   own normal must be one of those in its list
//   the two polygons of a merge pair must be triangles that share exactly one edge
//   a chunk of morph frames must be exactly the size its count says, and hold no more frames than the game can count
//   a textured polygon can't have the bit set that makes the game skip its texture coordinates
//   a module's points have to fit the box stored with its shape
//   a shape's centre is only made inside a shape, because it writes into the shape
//   a chunk of texture animation must fit the polygons it names, and only a shape can have one
//   a shape's lists of names and polygon changes are measured against their chunks before they're made
//   a polygon can't bring the flag that says its texture list is a table of pointers
//   removed the shapes made for the PlayStation and Saturn: nothing made or read one, and a console type read past the end of its chunk
//   a name is only read as far as the end of its chunk
//   the code that locked chunks inside a level file on disk is gone: nothing could reach it, and it read names onto the stack with no limit

#include <math.h>
#include <vector>

#include "chunk.hpp"
#include "chunk_type.hpp"
#include "chunk_shape.hpp"
#include "chunk_object.hpp"
#include "animation_chunk.hpp"


//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(chunk_shape)

RIF_IMPLEMENT_DYNCREATE("REBSHAPE",Shape_Chunk)

int Shape_Chunk::max_id = 0;
BOOL Shape_External_File_Chunk::UpdatingExternalShape=FALSE;

// Class Shape_Chunk functions

/*
Children for Shape_Chunk :

"SHPRAWVT"		Shape_Vertex_Chunk
"SHPPOLYS"		Shape_Polygon_Chunk
"SHPHEAD1"		Shape_Header_Chunk
"SHPVNORM"		Shape_Vertex_Normal_Chunk
"SHPPNORM"		Shape_Polygon_Normal_Chunk
"SHPTEXFN"		Shape_Texture_Filenames_Chunk
"SHPUVCRD"		Shape_UV_Coord_Chunk
"SHPMRGDT"		Shape_Merge_Data_Chunk
"SHPCENTR"		Shape_Centre_Chunk
"SHPMORPH"		Shape_Morphing_Data_Chunk
"SHPEXTFL"		Shape_External_File_Chunk
"SHPPCINF"		Shape_Poly_Change_Info_Chunk
"TEXTANIM"		Animation_Chunk
"SHPFRAGS"		Shape_Fragments_Chunk
"ANIMSEQU"		Anim_Shape_Sequence_Chunk
"PNOTINBB"		Poly_Not_In_Bounding_Box_Chunk
"ANSHCEN2"		Anim_Shape_Centre_Chunk
"ASALTTEX"		Anim_Shape_Alternate_Texturing_Chunk
*/

Shape_Chunk::Shape_Chunk(Chunk_With_Children * parent, const char *data, size_t size)
: Lockable_Chunk_With_Children (parent, "REBSHAPE"), shape_data ()
{
	const char * buffer_ptr = data;

	shape_data_store = (ChunkShape *) &shape_data;
	
	while ((data-buffer_ptr)< (signed)size)
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

/*--------------------------------------------------------------------**
** N.B. all changes to shape formats should be made to the sub shapes **
**--------------------------------------------------------------------*/

/*--------------------------------------------------------------------------**
** And of course the external file post input processing function should be **
** changed so that any new chunks will be copied over                       **
**--------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------**
** Oy READ THE NOTES, also destroy_auxiliary_chunks should be changed as well **
**----------------------------------------------------------------------------*/

		DynCreate(data);
		data += *(int *)(data + 8);

/*--------------------------------------------------------------------**
** N.B. all changes to shape formats should be made to the sub shapes **
**--------------------------------------------------------------------*/

/*--------------------------------------------------------------------------**
** And of course the external file post input processing function should be **
** changed so that any new chunks will be copied over                       **
**--------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------**
** Oy READ THE NOTES, also destroy_auxiliary_chunks should be changed as well **
**----------------------------------------------------------------------------*/

	}
}

Shape_Chunk::~Shape_Chunk ()
{
}

Shape_Header_Chunk * Shape_Chunk::get_header()
{
	return (Shape_Header_Chunk *) this->lookup_single_child ("SHPHEAD1");
}

// One number from a normal. A normal is a direction, so its numbers are small: the longest of all the 2.57 million normals in the
// game's own levels is 1.09 long. Anything much bigger means nothing, and the game can't turn it into its whole-number form safely,
// and neither can it turn infinity or "not a number". A normal of all zeros is allowed: the game's own levels have 8619 of them.
// This reads the float's bits instead of comparing the float itself, because the Release build's fast floating point setting lets
// the compiler assume every float is a real number, and it could drop a comparison meant to catch one that isn't. With the sign
// taken off, a float's bits count upwards in the same order as its size. 0x40000000 is 2, and infinity and "not a number" come
// after every real number, so this one test catches all of them.
static bool normal_number_is_valid (float number)
{
	unsigned int bits;
	memcpy (&bits, &number, sizeof(bits));

	return (bits & 0x7fffffff) <= 0x40000000;
}

// A shape's normals are in two lists: one for its points and one for its polygons, each read from a chunk with a count of its own.
// The game reads both using the shape's counts, not theirs, so a list that's short, or isn't there at all, would be read past its
// end. So its length has to be exactly what the shape needs. All 30673 shapes in the game's own levels have both lists, at
// exactly the right length.
static bool normals_are_valid (ChunkVectorFloat const * list, int list_count, int needed)
{
	if (list_count != needed)
	{
		return false;
	}

	for (int i = 0; i < list_count; i++)
	{
		if (!normal_number_is_valid (list[i].x) || !normal_number_is_valid (list[i].y) || !normal_number_is_valid (list[i].z))
		{
			return false;
		}
	}

	return true;
}

// Merge data pairs up triangles that the game joins into four-cornered polygons. There's one entry for each polygon: -1 if it
// stands alone, or else the place of the polygon it joins. The game uses that place to reach into its lists, and goes through
// every pair from its first polygon only, so an entry that doesn't lead back would leave its polygon with no place in the
// merged list. Every one of the 12528 chunks of merge data in the game's own levels has one entry per polygon, and all of
// their 89320 pairs lead back to each other.
//
// The game joins a pair by walking round the two triangles and crossing at the edge they share, so a pair has to be just that. To
// find the corner it's adding, it looks for a corner of the second triangle that the first doesn't have, and it trusts there is one.
// Two polygons with all their corners in common would send it to a place holding nothing. Every one of those 89320 pairs is two
// triangles with exactly two corners in common, seen from either side.
static int corners_also_in (ChunkPoly const & triangle, ChunkPoly const & other)
{
	int const corners_in_a_triangle = 3;
	int shared = 0;

	for (int i = 0; i < corners_in_a_triangle; i++)
	{
		for (int j = 0; j < corners_in_a_triangle; j++)
		{
			if (triangle.vert_ind[i] == other.vert_ind[j])
			{
				shared++;
				break;
			}
		}
	}

	return shared;
}

static bool merge_data_is_valid (Shape_Merge_Data_Chunk const * merge, ChunkPoly const * polygons, int polygon_count)
{
	if (merge->num_polys != polygon_count)
	{
		return false;
	}

	for (int i = 0; i < polygon_count; i++)
	{
		int const partner = merge->merge_data[i];

		if (partner == -1)
		{
			continue;
		}

		if (partner < 0 || partner >= polygon_count)
		{
			return false;
		}

		if (merge->merge_data[partner] != i)
		{
			return false;
		}

		// This runs from each polygon of the pair in turn, so it only has to look one way. A polygon that names itself as its partner
		// fails here too, because it has all its corners in common with itself.
		if (polygons[i].num_verts != 3 || corners_also_in (polygons[i], polygons[partner]) != 2)
		{
			return false;
		}
	}

	return true;
}

// A shape's animation number picks its place in a list made to fit the biggest number, so a negative one would write before
// that list, and a huge one would make the list's size wrap round to something small. This is far more than a shape needs.
static int const most_animation_sequences = 1024;

// In an animated shape, each frame has a full set of points and of polygon normals. To show a frame, the game points the shape
// at that frame's lists and goes on using the shape's own counts, so every frame must have exactly as many as the shape has.
// The game also goes straight to the first frame it can find, so there has to be one. A frame can be marked as one the game
// worked out between two others: it no longer makes those, and throws them away as it loads, so a file that marked its frames
// could leave none. No level that comes with the game has an animated shape, so these rules come from how the code uses the
// numbers, not from measuring levels.
static bool animation_matches_shape (Chunk_With_Children const * shape_chunk, int point_count, int polygon_count)
{
	List<Chunk *> sequences;
	shape_chunk->lookup_child ("ANIMSEQU", sequences);

	int frames_found = 0;

	for (LIF<Chunk *> sequence(&sequences); !sequence.done(); sequence.next())
	{
		ChunkAnimSequence const & animation = ((Anim_Shape_Sequence_Chunk *) sequence())->sequence_data;

		if (animation.SequenceNum < 0 || animation.SequenceNum >= most_animation_sequences)
		{
			return false;
		}

		for (int i = 0; i < animation.NumFrames; i++)
		{
			ChunkAnimFrame const * frame = animation.Frames[i];

			if (frame->num_verts != point_count || frame->num_polys != polygon_count)
			{
				return false;
			}

			if (frame->flags & animframeflag_interpolated_frame)
			{
				return false;
			}
		}

		frames_found += animation.NumFrames;
	}

	return sequences.size() == 0 || frames_found > 0;
}

// Two animated polygons of a merge pair become one, and the game reads the second's sequences and frames using the first's counts.
static bool animations_have_the_same_frames (TEXANIM const * first, TEXANIM const * second)
{
	if (first->NumSeq != second->NumSeq)
	{
		return false;
	}

	for (int i = 0; i < first->NumSeq; i++)
	{
		if (first->Seq[i]->NumFrames != second->Seq[i]->NumFrames)
		{
			return false;
		}
	}

	return true;
}

// A textured polygon keeps the place of its list of texture coordinates in the top half of its colour. The game uses that
// place to write into lists of its own, so it has to be a real list. The game reads the colour both as a signed number and
// as an unsigned one, so reading it as signed here means a number with its top bit set fails, which is safe for both. A list
// with fewer corners than its polygon would be read past its end when the polygon is drawn. An empty list is allowed: the game
// shows that polygon as a plain line. Every one of the 951593 textured polygons in the game's own levels has a real list with
// exactly as many corners as it has. A polygon with no texture uses its colour for something else, so it isn't checked: 316 of
// the others in those levels would fail.
static bool texture_numbers_are_valid (ChunkShape const & shape, ChunkPoly const & polygon)
{
	if (!is_textured (polygon.engine_type))
	{
		return true;
	}

	// This bit makes the game skip the step that copies a polygon's texture coordinates, and the polygon stays textured, so it would
	// be drawn from a list that was never filled in. None of the 951593 textured polygons in the game's own levels has it.
	if (polygon.colour & 0x8000)
	{
		return false;
	}

	int const list = (int) polygon.colour >> 16;

	if (list < 0 || list >= shape.num_uvs)
	{
		return false;
	}

	int const list_corners = shape.uv_list[list].num_verts;

	return list_corners == 0 || list_corners >= polygon.num_verts;
}

// A chunk of texture animation names its polygons by number, and the game goes straight to that polygon in its lists, then writes there. These
// numbers are places in the shape's own list, before any pairs are merged, which is the list the game looks them up in: with merge data it
// turns each one into a merged place itself, and with none the two lists are the same. It's all been measured: all 3552 animated polygons
// in the game's own levels pass.
// - The polygon has to be real, and a triangle with three different corners. To join a pair the game finds each corner in the merged polygon
//   and adds up where they were to work out the fourth, so two the same would send that sum, and a write, outside the list.
// - All 1353 pairs that are both animated match, frame for frame. See animations_have_the_same_frames.
// - An animation replaces a polygon's list of texture coordinates with a list of its own, of another kind and much shorter. So a polygon that
//   isn't animated can't share that list: it would read the new list as coordinates, and run past its end.
static bool texture_animation_fits_shape (ChunkShape const & shape, Chunk_With_Children const * shape_chunk)
{
	Animation_Chunk const * animation = (Animation_Chunk const *) shape_chunk->lookup_single_child ("TEXTANIM");

	if (!animation)
	{
		return true;
	}

	int const polygon_count = shape.num_polys;
	std::vector<TEXANIM const *> animation_of_polygon (polygon_count, (TEXANIM const *) 0);
	std::vector<bool> list_is_animated (shape.num_uvs, false);

	for (int i = 0; i < animation->NumPolys; i++)
	{
		TEXANIM const * entry = animation->AnimList[i];

		if (entry->poly < 0 || entry->poly >= polygon_count)
		{
			return false;
		}

		ChunkPoly const & polygon = shape.poly_list[entry->poly];
		int const * corners = polygon.vert_ind;

		if (polygon.num_verts != 3 || corners[0] == corners[1] || corners[1] == corners[2] || corners[0] == corners[2])
		{
			return false;
		}

		animation_of_polygon[entry->poly] = entry;

		if (is_textured (polygon.engine_type))
		{
			list_is_animated[(int) polygon.colour >> 16] = true;
		}
	}

	Shape_Merge_Data_Chunk const * merging = (Shape_Merge_Data_Chunk const *) shape_chunk->lookup_single_child ("SHPMRGDT");

	for (int i = 0; i < polygon_count; i++)
	{
		TEXANIM const * entry = animation_of_polygon[i];

		if (!entry)
		{
			if (is_textured (shape.poly_list[i].engine_type) && list_is_animated[(int) shape.poly_list[i].colour >> 16])
			{
				return false;
			}

			continue;
		}

		int const partner = merging ? merging->merge_data[i] : -1;

		if (partner != -1 && animation_of_polygon[partner] && !animations_have_the_same_frames (entry, animation_of_polygon[partner]))
		{
			return false;
		}
	}

	return true;
}

// Checks the numbers that say how big a shape is, all of which come from the level file.
// - Each time a shape is drawn, its points are copied into RotatedPts, a list of a fixed size, with no other check on how
//   many there are, so a shape with too many would be written past the end of that list. The biggest shape in the game's
//   own levels has 2421 points.
// - The header gives counts of its own, which have to agree with the points and polygons really in the file. If they
//   didn't, the code that loops on those counts would run past the end of the shape's lists.
// - Each corner of a polygon is a place in the shape's list of points, and in the lists built from it. A corner past the
//   end of those lists would read whatever memory comes next. Every corner of all 952940 polygons in the game's own
//   levels is in range, and all the counts agree.
// - A chunk of points or polygons replaces the shape's list as it's read, but these checks look at the first one. With two,
//   they would pass a list the shape no longer has. None of the 30673 shapes in the game's own levels has two.
static bool shape_numbers_are_valid (ChunkShape const & shape, Chunk_With_Children const * shape_chunk)
{
	if (shape_chunk->count_children ("SHPRAWVT") > 1 || shape_chunk->count_children ("SHPPOLYS") > 1)
	{
		return false;
	}

	int const point_count = shape.num_verts;

	// There's no need to test for a count below 0: the next step only passes a count that a real chunk of points agrees with,
	// or a count of 0.
	if (point_count > maxrotpts)
	{
		return false;
	}

	Shape_Vertex_Chunk * points = (Shape_Vertex_Chunk *) shape_chunk->lookup_single_child ("SHPRAWVT");

	// With no chunk of points there's no list of points at all, so the shape can't claim to have any.
	if (points ? points->num_verts != point_count : point_count != 0)
	{
		return false;
	}

	Shape_Polygon_Chunk * polygons = (Shape_Polygon_Chunk *) shape_chunk->lookup_single_child ("SHPPOLYS");
	int const polygon_count = polygons ? polygons->num_polys : 0;

	if (shape.num_polys != polygon_count)
	{
		return false;
	}

	// A second chunk of normals would replace the list, just as a second chunk of points would. None of the game's shapes has one.
	if (shape_chunk->count_children ("SHPVNORM") > 1 || shape_chunk->count_children ("SHPPNORM") > 1)
	{
		return false;
	}

	Shape_Vertex_Normal_Chunk * point_normals = (Shape_Vertex_Normal_Chunk *) shape_chunk->lookup_single_child ("SHPVNORM");
	Shape_Polygon_Normal_Chunk * polygon_normals = (Shape_Polygon_Normal_Chunk *) shape_chunk->lookup_single_child ("SHPPNORM");

	if (!normals_are_valid (shape.v_normal_list, point_normals ? point_normals->num_verts : 0, point_count))
	{
		return false;
	}

	if (!normals_are_valid (shape.p_normal_list, polygon_normals ? polygon_normals->num_polys : 0, polygon_count))
	{
		return false;
	}

	for (int i = 0; i < polygon_count; i++)
	{
		// A polygon says where in the list its own normal is, and the game goes straight there. In the game's own levels each of
		// the 952940 polygons points at its own place in the list.
		if (shape.poly_list[i].normal_index < 0 || shape.poly_list[i].normal_index >= polygon_count)
		{
			return false;
		}

		for (int j = 0; j < shape.poly_list[i].num_verts; j++)
		{
			int const corner = shape.poly_list[i].vert_ind[j];

			if (corner < 0 || corner >= point_count)
			{
				return false;
			}
		}

		if (!texture_numbers_are_valid (shape, shape.poly_list[i]))
		{
			return false;
		}
	}

	// A shape that morphs blends its points with those of its morph frames, and writes the result into MorphedPts, another
	// list of a fixed size and a smaller one. The blend goes by the shape's count, so every frame needs that many points.
	// All 860 morph frames in the game's own levels match their shape, and the biggest has 808 points. Sub-shapes anywhere
	// else don't have to match: fragments of a shape are shapes of their own, and nearly all of them are smaller.
	Chunk_With_Children * morphing = (Chunk_With_Children *) shape_chunk->lookup_single_child ("SHPMORPH");

	if (morphing)
	{
		if (point_count > maxmorphPts)
		{
			return false;
		}

		List<Chunk *> frames;
		morphing->lookup_child ("SUBSHAPE", frames);

		for (LIF<Chunk *> frame(&frames); !frame.done(); frame.next())
		{
			if (((Shape_Sub_Shape_Chunk *) frame())->shape_data.num_verts != point_count)
			{
				return false;
			}
		}
	}

	Shape_Merge_Data_Chunk * merging = (Shape_Merge_Data_Chunk *) shape_chunk->lookup_single_child ("SHPMRGDT");

	if (merging && !merge_data_is_valid (merging, shape.poly_list, polygon_count))
	{
		return false;
	}

	return animation_matches_shape (shape_chunk, point_count, polygon_count) && texture_animation_fits_shape (shape, shape_chunk);
}

// A module's extents, and the ball round it that the shadow pass trusts, come from the box stored with its shape, not from its points. In all
// 12813 module shapes in the game's own levels the box is exact. Other shapes can't be held to this: 655 others have points outside
// their box, by up to 5845, so only a module's shape is asked.
bool Shape_Chunk::points_fit_box()
{
	for (int i = 0; i < shape_data.num_verts; i++)
	{
		ChunkVectorInt const & point = shape_data.v_list[i];

		if (point.x < shape_data.min.x || point.x > shape_data.max.x
			|| point.y < shape_data.min.y || point.y > shape_data.max.y
			|| point.z < shape_data.min.z || point.z > shape_data.max.z)
		{
			return false;
		}
	}

	return true;
}

bool Shape_Chunk::numbers_are_valid()
{
	return shape_numbers_are_valid (shape_data, this);
}

List<Object_Chunk *> const & Shape_Chunk::list_assoc_objs()
{
	if (!get_header())
	{
		static List<Object_Chunk *> empty_list;
		return empty_list;
	}

	return get_header()->associated_objects_store;
}

BOOL Shape_Chunk::assoc_with_object (Object_Chunk *obch)
{
	return obch->assoc_with_shape(this);
}

BOOL Shape_Chunk::deassoc_with_object (Object_Chunk *obch)
{
	return obch->deassoc_with_shape(this);
}

void Shape_Chunk::post_input_processing()
{
	if (get_header())
		if (get_header()->flags & GENERAL_FLAG_LOCKED)
			external_lock = TRUE;

	Chunk_With_Children::post_input_processing();
}

void Shape_Chunk::destroy_auxiliary_chunks()
{
	//split up into different blocks to stop compiler crashing when optimizations are turned on
	List<Chunk *> chlst;

	lookup_child("SHPZSPDT",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("SHPMRGDT",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("SHPMORPH",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("TEXTANIM",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("SHPPCINF",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("SHPFRAGS",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("ANIMSEQU",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("PNOTINBB",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("ANSHCEN2",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}

	lookup_child("ASALTTEX",chlst);
	while (chlst.size())
	{
		delete chlst.first_entry();
		chlst.delete_first_entry();
	}
}

void Shape_Chunk::set_lock_user (char * user)
{
	Shape_Header_Chunk * hdptr = get_header();

	if (!hdptr) return;

	strncpy (hdptr->lock_user, user,16);

	hdptr->lock_user[16] = 0;
}

BOOL Shape_Chunk::inc_v_no ()
{
	Shape_Header_Chunk * hdptr = get_header();

	if (!hdptr) return (FALSE);

	hdptr->version_no++;

	return (TRUE);
}

BOOL Shape_Chunk::same_and_updated(Shape_Chunk & shp)
{
	Shape_Header_Chunk * hd1ptr = get_header();

	if (!hd1ptr) return (0);

	Shape_Header_Chunk * hd2ptr = shp.get_header();

	if (!hd2ptr) return (0);

	return (hd1ptr->version_no < hd2ptr->version_no && hd1ptr->file_id_num == hd2ptr->file_id_num);
}

BOOL Shape_Chunk::assoc_with_object_list(File_Chunk *fc)
{
	Shape_Header_Chunk * hdptr = get_header();
	Object_Chunk * ob = NULL;

	if (!hdptr) return (FALSE);

	List<Chunk *> chlst;
	fc->lookup_child("RBOBJECT",chlst);

	for (LIF<char *> n(&(hdptr->object_names_store)); !n.done(); n.next())
	{
		LIF<Chunk *> l(&chlst);
		for (; !l.done(); l.next())
		{
			ob = (Object_Chunk *)l();
			if ( !strcmp(ob->object_data.o_name, n()) )
				break;
		}
		if (!l.done())
			assoc_with_object(ob);
		else
		{
			return(FALSE);
		}
	}
	return(TRUE);
}

/////////////////////////////////////////

// Class Shape_Vertex_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created

// from buffer

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPRAWVT",Shape_Vertex_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPRAWVT",Shape_Vertex_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPRAWVT",Shape_Vertex_Chunk,"ANIMFRAM",Anim_Shape_Frame_Chunk)

Shape_Vertex_Chunk::Shape_Vertex_Chunk(Shape_Chunk * parent, const char * vtdata , size_t vtsize)
: Chunk (parent, "SHPRAWVT"), vert_data (NULL),
	num_verts (vtsize / 12) // 12 bytes per vertex
{
	int i;

	parent->shape_data_store->v_list = new ChunkVectorInt[num_verts];
	parent->shape_data_store->num_verts = num_verts;

	*((ChunkVectorInt**) &vert_data) = parent->shape_data_store->v_list;

	for (i=0;i<num_verts;i++)
	{
		parent->shape_data_store->v_list[i] = *((ChunkVectorInt *) vtdata );
		vtdata+=sizeof(ChunkVectorInt);
	}
}


Shape_Vertex_Chunk::Shape_Vertex_Chunk(Shape_Sub_Shape_Chunk * parent, const char * vtdata , size_t vtsize)
: Chunk (parent, "SHPRAWVT"), vert_data (NULL),
	num_verts (vtsize / 12) // 12 bytes per vertex
{
	int i;

	parent->shape_data_store->v_list = new ChunkVectorInt[num_verts];
	parent->shape_data_store->num_verts = num_verts;

	*((ChunkVectorInt**) &vert_data) = parent->shape_data_store->v_list;

	for (i=0;i<num_verts;i++)
	{
		parent->shape_data_store->v_list[i] = *((ChunkVectorInt *) vtdata );
		vtdata+=sizeof(ChunkVectorInt);
	}
}


Shape_Vertex_Chunk::Shape_Vertex_Chunk(Anim_Shape_Frame_Chunk * parent, const char * vtdata , size_t vtsize)
: Chunk (parent, "SHPRAWVT"), vert_data (NULL),
	num_verts (vtsize / 12) // 12 bytes per vertex
{
	int i;

	ChunkVectorInt* v_list = new ChunkVectorInt[num_verts];
	*(ChunkVectorInt**)&vert_data=v_list;

	for (i=0;i<num_verts;i++)
	{
		v_list[i] = *((ChunkVectorInt *) vtdata );
		vtdata+=sizeof(ChunkVectorInt);
	}
}

BOOL Shape_Vertex_Chunk::output_chunk (HANDLE &hand)
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

void Shape_Vertex_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	for (int i=0;i<num_verts;i++)
	{
		*(ChunkVectorInt *) data_start  = vert_data[i];
		data_start+=sizeof(ChunkVectorInt);
	}
}

/////////////////////////////////////////

// Class Shape_Vertex_Normal_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPVNORM",Shape_Vertex_Normal_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPVNORM",Shape_Vertex_Normal_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

// from buffer
Shape_Vertex_Normal_Chunk::Shape_Vertex_Normal_Chunk(Shape_Chunk * parent, const char * vtdata , size_t vtsize)
: Chunk (parent, "SHPVNORM"), vert_norm_data (NULL),
	num_verts (vtsize / sizeof(ChunkVectorInt)) 
{
	int i;

	parent->shape_data_store->v_normal_list = new ChunkVectorFloat[num_verts];
	*((ChunkVectorFloat**) &vert_norm_data) = parent->shape_data_store->v_normal_list;
	
	for (i=0;i<num_verts;i++)
	{
		parent->shape_data_store->v_normal_list[i] = *((ChunkVectorFloat *) vtdata);
		vtdata+=sizeof(ChunkVectorFloat);
	}
}

Shape_Vertex_Normal_Chunk::Shape_Vertex_Normal_Chunk(Shape_Sub_Shape_Chunk * parent, const char * vtdata , size_t vtsize)
: Chunk (parent, "SHPVNORM"), vert_norm_data (NULL),
	num_verts (vtsize / sizeof(ChunkVectorInt)) 
{
	int i;

	parent->shape_data_store->v_normal_list = new ChunkVectorFloat[num_verts];
	*((ChunkVectorFloat**) &vert_norm_data) = parent->shape_data_store->v_normal_list;
	
	for (i=0;i<num_verts;i++)
	{
		parent->shape_data_store->v_normal_list[i] = *((ChunkVectorFloat *) vtdata );
		vtdata+=sizeof(ChunkVectorFloat);
	}
}

BOOL Shape_Vertex_Normal_Chunk::output_chunk (HANDLE &hand)
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

void Shape_Vertex_Normal_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	for (int i=0;i<num_verts;i++)
	{
		*((ChunkVectorFloat *) data_start ) = vert_norm_data[i];
		data_start+=sizeof(ChunkVectorFloat);
	}
}
/////////////////////////////////////////

// Class Shape_Polygon_Normal_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPPNORM",Shape_Polygon_Normal_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPPNORM",Shape_Polygon_Normal_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPPNORM",Shape_Polygon_Normal_Chunk,"ANIMFRAM",Anim_Shape_Frame_Chunk)

// from buffer
Shape_Polygon_Normal_Chunk::Shape_Polygon_Normal_Chunk(Shape_Chunk * parent, const char * pndata , size_t pnsize)
: Chunk (parent, "SHPPNORM"), poly_norm_data (NULL),
	num_polys (pnsize / sizeof(ChunkVectorFloat)) 
{
	int i;

	parent->shape_data_store->p_normal_list = new ChunkVectorFloat[num_polys];
	*((ChunkVectorFloat**) &poly_norm_data) = parent->shape_data_store->p_normal_list;

	for (i=0;i<num_polys;i++)
	{
		parent->shape_data_store->p_normal_list[i] = *((ChunkVectorFloat *) pndata);
		pndata+=sizeof(ChunkVectorFloat);
	}
}

Shape_Polygon_Normal_Chunk::Shape_Polygon_Normal_Chunk(Shape_Sub_Shape_Chunk * parent, const char * pndata , size_t pnsize)
: Chunk (parent, "SHPPNORM"), poly_norm_data (NULL),
	num_polys (pnsize / sizeof(ChunkVectorFloat)) 
{
	int i;

	parent->shape_data_store->p_normal_list = new ChunkVectorFloat[num_polys];
	*((ChunkVectorFloat**) &poly_norm_data) = parent->shape_data_store->p_normal_list;

	for (i=0;i<num_polys;i++)
	{
		parent->shape_data_store->p_normal_list[i] = *((ChunkVectorFloat *) pndata  );
		pndata+=sizeof(ChunkVectorFloat);
	}
}

Shape_Polygon_Normal_Chunk::Shape_Polygon_Normal_Chunk(Anim_Shape_Frame_Chunk * parent, const char * pndata , size_t pnsize)
: Chunk (parent, "SHPPNORM"), poly_norm_data (NULL),
	num_polys (pnsize / sizeof(ChunkVectorFloat)) 
{
	int i;
	
	ChunkVectorFloat* p_normal_list = new ChunkVectorFloat[num_polys];
	*((ChunkVectorFloat**) &poly_norm_data) = p_normal_list;
	
	for (i=0;i<num_polys;i++)
	{
		p_normal_list[i] = *((ChunkVectorFloat *) pndata);
		pndata+=sizeof(ChunkVectorFloat);
	}
}

BOOL Shape_Polygon_Normal_Chunk::output_chunk (HANDLE &hand)
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

void Shape_Polygon_Normal_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	for (int i=0;i<num_polys;i++)
	{
		*(ChunkVectorFloat *) data_start  = poly_norm_data[i];
		data_start+=sizeof(ChunkVectorFloat);
	}
}

/////////////////////////////////////////

// Class Shape_Polygon_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPPOLYS",Shape_Polygon_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPPOLYS",Shape_Polygon_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

// Reads one polygon from a level file. Each takes 36 bytes: four numbers, then five places for its corners, which end
// with a -1. vert_ind holds 4 corners, so the reading stops there, whatever the file says: going by the -1 alone, a
// polygon that left it out would be written past the end of vert_ind. A polygon with no -1 can't be real, so the level
// is refused. Nor can one with fewer than 3 corners: the code that draws a polygon counts down its corners and stops at
// the last, so with none it would never stop. Every one of the 952940 polygons in the game's own levels has 3 or 4 corners
// and its -1.
static void read_polygon (ChunkPoly & poly, const char * record)
{
	int const * const corners = (int const *) (record + 16);
	int const most_corners = sizeof(poly.vert_ind) / sizeof(poly.vert_ind[0]);
	int const fewest_corners = 3;

	poly.engine_type = *((int *) record);
	poly.normal_index = *((int *) (record + 4));
	poly.flags = *((int *) (record + 8));
	poly.colour = *((int *) (record + 12));

	poly.num_verts = 0;

	while (poly.num_verts < most_corners && corners[poly.num_verts] != -1)
	{
		poly.vert_ind[poly.num_verts] = corners[poly.num_verts];
		poly.num_verts++;
	}

	if (poly.num_verts < fewest_corners || corners[poly.num_verts] != -1)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}

	// The loader sets this flag itself on a polygon with texture animation (iflag_txanim), and the game then reads the polygon's list of texture coordinates
	// as a table of pointers. From a file, it would turn numbers in the file into pointers. None of the 952,940 polygons in the game's own levels has it.
	int const flag_only_the_loader_sets = 0x40000000;

	if (poly.flags & flag_only_the_loader_sets)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
	}
}

// from buffer
Shape_Polygon_Chunk::Shape_Polygon_Chunk (Shape_Chunk * parent, const char * pdata, size_t psize)
: Chunk (parent, "SHPPOLYS"),
	poly_data (parent->shape_data_store->poly_list), num_polys (psize / 36) // 9 * 4 bytes per polygon
{
	int i;

	parent->shape_data_store->poly_list = new ChunkPoly [num_polys];
	parent->shape_data_store->num_polys = num_polys;

	*((ChunkPoly **) &poly_data) = parent->shape_data_store->poly_list;

	for (i=0; i<num_polys; i++)
	{
		read_polygon (parent->shape_data_store->poly_list[i], pdata + (36*i));
	}
}

Shape_Polygon_Chunk::Shape_Polygon_Chunk (Shape_Sub_Shape_Chunk * parent, const char * pdata, size_t psize)
: Chunk (parent, "SHPPOLYS"),
	poly_data (parent->shape_data_store->poly_list), num_polys (psize / 36) // 9 * 4 bytes per polygon
{
	int i;

	parent->shape_data_store->poly_list = new ChunkPoly [num_polys];
	parent->shape_data_store->num_polys = num_polys;

	*((ChunkPoly **) &poly_data) = parent->shape_data_store->poly_list;

	for (i=0; i<num_polys; i++)
	{
		read_polygon (parent->shape_data_store->poly_list[i], pdata + (36*i));
	}
}

BOOL Shape_Polygon_Chunk::output_chunk (HANDLE &hand)
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

void Shape_Polygon_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	int j;
	for (int i=0;i<num_polys;i++) {
		*((int *) (data_start + i*36)) = poly_data[i].engine_type;
		*((int *) (data_start + i*36 + 4)) = poly_data[i].normal_index;
		*((int *) (data_start + i*36 + 8)) = poly_data[i].flags;
		*((int *) (data_start + i*36 + 12)) = poly_data[i].colour;
		for (j = 0; j<poly_data[i].num_verts; j++)
			*((int *) (data_start + i*36 + 16 + j*4)) = poly_data[i].vert_ind[j];
		for (; j<5; j++)
			*((int *) (data_start + i*36 + 16 + j*4)) = -1;
	}
}
/////////////////////////////////////////
//Class Shape_Centre_Chunk :

// This constructor writes into its parent's shape, so it can only be made under a shape. All 28193 of these in the game's own levels are.
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPCENTR",Shape_Centre_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPCENTR",Shape_Centre_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

Shape_Centre_Chunk::Shape_Centre_Chunk(Chunk_With_Children* parent,const char* data, size_t datasize)
:Chunk (parent,"SHPCENTR")
{
	assert(datasize==16);

	//find parent's chunkshape
	ChunkShape* cs=0;
	if(!strcmp(parent->identifier,"REBSHAPE"))
	{
		cs=((Shape_Chunk*)parent)->shape_data_store;
	}
	else if(!strcmp(parent->identifier,"SUBSHAPE"))
	{
		cs=((Shape_Sub_Shape_Chunk*)parent)->shape_data_store;
	}
	assert(cs);

	//fill in the appropriate entries
	cs->centre=*(ChunkVectorInt*)data;
	data+=sizeof(ChunkVectorInt);
	
	cs->radius_about_centre=*(float*)data;
}

void Shape_Centre_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;

	//find parent's chunkshape
	ChunkShape* cs=0;
	if(!strcmp(parent->identifier,"REBSHAPE"))
	{
		cs=((Shape_Chunk*)parent)->shape_data_store;
	}
	else if(!strcmp(parent->identifier,"SUBSHAPE"))
	{
		cs=((Shape_Sub_Shape_Chunk*)parent)->shape_data_store;
	}
	assert(cs);

	*(ChunkVectorInt*)data_start=cs->centre;
	data_start+=sizeof(ChunkVectorInt);

	*(float*)data_start=cs->radius;
}


/////////////////////////////////////////

// Class Shape_UV_Coord_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPUVCRD",Shape_UV_Coord_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPUVCRD",Shape_UV_Coord_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

// How many lists of texture coordinates a chunk says it holds, which sizes the list the game makes. Each list takes at least 4
// bytes, so a count that couldn't fit in the chunk can't be real, and the level is refused. Turned into an unsigned number, a
// negative count becomes a huge one, so the same test refuses those too.
static int count_uv_lists (const char * uvdata, size_t uvsize)
{
	size_t const count_bytes = sizeof(int);
	size_t const smallest_list = sizeof(int);

	if (uvsize < count_bytes)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return 0;
	}

	int const count = *((int *) uvdata);

	if ((size_t) count > (uvsize - count_bytes) / smallest_list)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return 0;
	}

	return count;
}

// Reads the lists of texture coordinates from a level file. Each is a number of corners, then two numbers for each corner.
// A list has 4 places, so going by the file's number alone, a list with more would be written past the end of its places. So
// that number has to fit the places, and everything read has to be inside the chunk, or the level is refused. All 951735 lists
// in the game's own levels have 3 or 4 corners or none, and all 30671 chunks hold exactly what they say.
static void read_uv_lists (ChunkUV_List * lists, int count, const char * uvdata, size_t uvsize)
{
	size_t const most_corners = sizeof(lists[0].vert) / sizeof(lists[0].vert[0]);
	const char * const end = uvdata + uvsize;

	// a list that's never reached stays empty
	for (int i = 0; i < count; i++)
	{
		lists[i].num_verts = 0;
	}

	// Step past the count. With no lists there is nothing after it to read, and the chunk may end right here.
	if (count)
	{
		uvdata += sizeof(int);
	}

	for (int i = 0; i < count; i++)
	{
		if ((size_t) (end - uvdata) < sizeof(int))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			return;
		}

		int const corners = *((int *) uvdata);
		uvdata += sizeof(int);

		// Turned into an unsigned number, a negative count of corners becomes a huge one, so this refuses those too.
		if ((size_t) corners > most_corners || (size_t) (end - uvdata) < corners * sizeof(ChunkUV))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			return;
		}

		lists[i].num_verts = corners;

		for (int j = 0; j < corners; j++)
		{
			lists[i].vert[j] = *((ChunkUV *) uvdata);
			uvdata += sizeof(ChunkUV);
		}
	}
}

// from buffer
Shape_UV_Coord_Chunk::Shape_UV_Coord_Chunk (Shape_Chunk * parent, const char * uvdata, size_t uvsize)
: Chunk (parent, "SHPUVCRD"),
uv_data (NULL), num_uvs (count_uv_lists (uvdata, uvsize))
{
	parent->shape_data_store->uv_list = num_uvs ? new ChunkUV_List[num_uvs] : 0;
	*((ChunkUV_List**) &uv_data) = parent->shape_data_store->uv_list;

	parent->shape_data_store->num_uvs = num_uvs;

	read_uv_lists (parent->shape_data_store->uv_list, num_uvs, uvdata, uvsize);
}

Shape_UV_Coord_Chunk::Shape_UV_Coord_Chunk (Shape_Sub_Shape_Chunk * parent, const char * uvdata, size_t uvsize)
: Chunk (parent, "SHPUVCRD"),
uv_data (NULL), num_uvs (count_uv_lists (uvdata, uvsize))
{
	parent->shape_data_store->uv_list = num_uvs ? new ChunkUV_List[num_uvs] : 0;
	*((ChunkUV_List**) &uv_data) = parent->shape_data_store->uv_list;

	parent->shape_data_store->num_uvs = num_uvs;

	read_uv_lists (parent->shape_data_store->uv_list, num_uvs, uvdata, uvsize);
}

BOOL Shape_UV_Coord_Chunk::output_chunk (HANDLE &hand)
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

void Shape_UV_Coord_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = num_uvs;

	data_start += 4;

	for (int i=0;i<num_uvs;i++)
	{
		*((int *)data_start) = uv_data[i].num_verts;
		data_start += 4;
		for (int j = 0; j< uv_data[i].num_verts; j++)
		{
			*((ChunkUV *) data_start) = uv_data[i].vert[j];
			data_start += sizeof(ChunkUV);
		}
	}
}

size_t Shape_UV_Coord_Chunk::size_chunk ()
{
	chunk_size = 12 + 4;
	for (int i=0; i<num_uvs; i++) 
		chunk_size += (4+(sizeof(ChunkUV)*uv_data[i].num_verts));

	return chunk_size;
}

/////////////////////////////////////////

// Class Shape_Texture_Filenames_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPTEXFN",Shape_Texture_Filenames_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPTEXFN",Shape_Texture_Filenames_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)
// from buffer
Shape_Texture_Filenames_Chunk::Shape_Texture_Filenames_Chunk (Shape_Chunk * parent, const char * tfndata, size_t tfnsize)
: Chunk (parent, "SHPTEXFN"),
tex_fns (), num_tex_fns (CountIfItFits (*((int *) tfndata), 1, BytesLeftAfter (tfnsize, sizeof(int))))
{
	int i;
	char const * const start = tfndata;

	// the () makes every place start empty: if the list stops early, the ones it didn't reach are still freed
	parent->shape_data_store->texture_fns = new char * [num_tex_fns] ();
	*((char***) &tex_fns) = parent->shape_data_store->texture_fns;

	parent->shape_data_store->num_texfiles = num_tex_fns;

	tfndata += 4;

	for (i=0; i<num_tex_fns; i++)
	{
		// A name can take more than the one byte the count was measured with. The shape then only counts the names it really has, because a copy of it
		// reads every name it counts.
		if ((size_t)(tfndata - start) + 1 > tfnsize)
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			parent->shape_data_store->num_texfiles = i;
			break;
		}

		size_t length = 0;
		parent->shape_data_store->texture_fns[i] = CopyNameFromChunk(start, tfnsize, tfndata, &length);
		tfndata += length + 1;
	}
}

Shape_Texture_Filenames_Chunk::Shape_Texture_Filenames_Chunk (Shape_Sub_Shape_Chunk * parent, const char * tfndata, size_t tfnsize)
: Chunk (parent, "SHPTEXFN"),
tex_fns (), num_tex_fns (CountIfItFits (*((int *) tfndata), 1, BytesLeftAfter (tfnsize, sizeof(int))))
{
	int i;
	char const * const start = tfndata;

	// the () makes every place start empty: if the list stops early, the ones it didn't reach are still freed
	parent->shape_data_store->texture_fns = new char * [num_tex_fns] ();
	*((char***) &tex_fns) = parent->shape_data_store->texture_fns;

	parent->shape_data_store->num_texfiles = num_tex_fns;

	tfndata += 4;

	for (i=0; i<num_tex_fns; i++)
	{
		// A name can take more than the one byte the count was measured with. The shape then only counts the names it really has, because a copy of it
		// reads every name it counts.
		if ((size_t)(tfndata - start) + 1 > tfnsize)
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			parent->shape_data_store->num_texfiles = i;
			break;
		}

		size_t length = 0;
		parent->shape_data_store->texture_fns[i] = CopyNameFromChunk(start, tfnsize, tfndata, &length);
		tfndata += length + 1;
	}
}

BOOL Shape_Texture_Filenames_Chunk::output_chunk (HANDLE &hand)
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


void Shape_Texture_Filenames_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = num_tex_fns;

	data_start += 4;

	for (int i=0;i<num_tex_fns;i++) {
		sprintf(data_start, "%s", tex_fns[i]);
		data_start += (strlen (tex_fns[i]) + 1);
	}
}

size_t Shape_Texture_Filenames_Chunk::size_chunk()
{
	chunk_size = 16;

	for (int i=0;i<num_tex_fns;i++)
		chunk_size += (strlen (tex_fns[i]) + 1);

	chunk_size += (4-chunk_size%4)%4;

	return chunk_size;
}


/////////////////////////////////////////

// Class Shape_Header_Chunk functions

// These can only be children of Shape_Chunks
// the shape chunks data will automatically be updated by it is created
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPHEAD1",Shape_Header_Chunk,"REBSHAPE",Shape_Chunk)


Shape_Header_Chunk::Shape_Header_Chunk (Shape_Chunk * parent, const char * hdata, size_t hsize)
	: Chunk (parent, "SHPHEAD1"),
	shape_data (parent->shape_data_store)
{
	char const * const start = hdata;
	int num_as_obj;

	flags = *((int *) hdata);

	strncpy (lock_user, (hdata + 4), 16);
	lock_user[16] = '\0';
	hdata+=20;

	file_id_num = *((int *) hdata );
	hdata+=4;

	parent->shape_data_store->num_verts = *((int *) hdata );
	hdata+=4;
	parent->shape_data_store->num_polys = *((int *) hdata );
	hdata+=4;

	parent->shape_data_store->radius = *((float *) hdata);
	hdata+=4;
	
	parent->shape_data_store->max.x = *((int *) hdata);
	hdata+=4;
	parent->shape_data_store->min.x = *((int *) hdata);
	hdata+=4;

	parent->shape_data_store->max.y = *((int *) hdata);
	hdata+=4;
	parent->shape_data_store->min.y = *((int *) hdata);
	hdata+=4;

	parent->shape_data_store->max.z = *((int *) hdata);
	hdata+=4;
	parent->shape_data_store->min.z = *((int *) hdata);
	hdata+=4;

	version_no = *((int *) hdata);
	hdata+=4;

	num_as_obj = ReadListCount (start, hdata, hsize, 1);

	char * obj_store;

	for (int i = 0; i< num_as_obj; i++)
	{
		// a name can take more than the one byte the count was measured with
		if ((size_t)(hdata - start) + 1 > hsize)
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		size_t length = 0;
		obj_store = CopyNameFromChunk(start, hsize, hdata, &length);
		object_names_store.add_entry(obj_store);
		hdata += length + 1;
	}
}

Shape_Header_Chunk::~Shape_Header_Chunk()
{
	for (LIF<char *> aon(&object_names_store);
			!aon.done(); aon.next() )
		delete[] aon();
}

size_t Shape_Header_Chunk::size_chunk()
{
	size_t length = 80;

	for (LIF<char *> aon(&object_names_store);
			!aon.done(); aon.next() )
		length += (strlen (aon()) + 1);

	length += (4-length%4)%4;

	chunk_size = length;

	return length;
}

BOOL Shape_Header_Chunk::output_chunk(HANDLE & hand)
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

void Shape_Header_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = flags;
	strncpy ((data_start + 4), lock_user, 16);
	*((int *) (data_start+20)) = file_id_num;
	data_start+=24;

	*((int *) data_start) = shape_data->num_verts;
	data_start+=4;
	*((int *) data_start) = shape_data->num_polys;
	data_start+=4;

	*((float *) data_start) = shape_data->radius;
	data_start+=4;

	*((int *) data_start) = shape_data->max.x;
	data_start+=4;
	*((int *) data_start) = shape_data->min.x;
	data_start+=4;

	*((int *) data_start) = shape_data->max.y;
	data_start+=4;
	*((int *) data_start) = shape_data->min.y;
	data_start+=4;

	*((int *) data_start) = shape_data->max.z;
	data_start+=4;
	*((int *) data_start) = shape_data->min.z;
	data_start+=4;

	*((int *) data_start) = version_no;
	data_start+=4;

	*((int *) data_start) = object_names_store.size();
	data_start+=4;

	for (LIF<char *> ons(&object_names_store); !ons.done(); ons.next())
	{
		memcpy (data_start, ons(), strlen(ons()) + 1);
		data_start += (strlen(ons())+1);
	}
}


void Shape_Header_Chunk::prepare_for_output()
{
// this will also set the object chunks numbers as well,
// so that it is done in the right order

	char * str;

	if (file_id_num == -1) file_id_num = ++(Shape_Chunk::max_id);

	while (object_names_store.size()) 
	{
		delete[] object_names_store.first_entry();
		object_names_store.delete_first_entry();
	}

	for (LIF<Object_Chunk *> aosl(&associated_objects_store);
		!aosl.done(); aosl.next())
	{
		if (aosl()->get_header())
			aosl()->get_header()->shape_id_no = file_id_num;

		size_t const name_size = strlen(aosl()->object_data.o_name) + 1;
		str = new char [name_size];
		memcpy(str, aosl()->object_data.o_name, name_size);
		object_names_store.add_entry(str);
	}

	Shape_Chunk::max_id = max(Shape_Chunk::max_id, file_id_num);

// this should always be the last thing

	version_no ++;
}



/////////////////////////////////////////

// Class Shape_Merge_Data_Chunk functions

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPMRGDT",Shape_Merge_Data_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPMRGDT",Shape_Merge_Data_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

Shape_Merge_Data_Chunk::Shape_Merge_Data_Chunk(Shape_Chunk * parent, int * m_dt, int n_ps)
: Chunk (parent, "SHPMRGDT"), num_polys (n_ps)
{
	merge_data = new int [n_ps];
	for (int i=0; i<n_ps; i++)
	{
		merge_data[i] = m_dt[i];
	}
}

Shape_Merge_Data_Chunk::Shape_Merge_Data_Chunk(Shape_Sub_Shape_Chunk * parent, int * m_dt, int n_ps)
: Chunk (parent, "SHPMRGDT"), num_polys (n_ps)
{
	merge_data = new int [n_ps];
	for (int i=0; i<n_ps; i++)
	{
		merge_data[i] = m_dt[i];
	}
}


Shape_Merge_Data_Chunk::Shape_Merge_Data_Chunk (Shape_Chunk * parent, const char *md, size_t ms)
: Chunk (parent, "SHPMRGDT"), num_polys (ms/4)
{
	merge_data = new int [num_polys];
	for (int i=0; i<num_polys; i++)
	{
		merge_data[i] = *((int *)(md+4*i));
	}
}

Shape_Merge_Data_Chunk::Shape_Merge_Data_Chunk (Shape_Sub_Shape_Chunk * parent, const char *md, size_t ms)
: Chunk (parent, "SHPMRGDT"), num_polys (ms/4)
{
	merge_data = new int [num_polys];
	for (int i=0; i<num_polys; i++)
	{
		merge_data[i] = *((int *)(md+4*i));
	}
}


Shape_Merge_Data_Chunk::~Shape_Merge_Data_Chunk()
{
	delete[] merge_data;
}

void Shape_Merge_Data_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	for (int i=0; i<num_polys; i++)
	{
		*((int *) (data_start+i*4) ) = merge_data[i];
	}
}


/////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("SHPEXTFL",Shape_External_File_Chunk)

/*
Children for Shape_External_File_Chunk :

"SHPEXTFN"		Shape_External_Filename_Chunk)
"BMPLSTST"		Bitmap_List_Store_Chunk)
"BMNAMVER"		BMP_Names_Version_Chunk)
"BMNAMEXT"		BMP_Names_ExtraData_Chunk)
"RIFFNAME"		RIF_Name_Chunk)
"BMPMD5ID"		Bitmap_MD5_Chunk)
"EXTOBJNM"		Shape_External_Object_Name_Chunk)
*/

CHUNK_WITH_CHILDREN_LOADER("SHPEXTFL",Shape_External_File_Chunk)


void Shape_External_File_Chunk::post_input_processing()
{
	Chunk_With_Children::post_input_processing();
}

const char* Shape_External_File_Chunk::get_shape_name()
{
	Shape_External_Object_Name_Chunk* seonc=(Shape_External_Object_Name_Chunk*)lookup_single_child("EXTOBJNM");
	if(seonc)
	{
		return seonc->shape_name;
	}

	RIF_Name_Chunk* rnc=(RIF_Name_Chunk*)lookup_single_child("RIFFNAME");
	if(rnc)
	{
		return rnc->rif_name;
	}

	return 0;
}

/////////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("SHPEXTFN",Shape_External_Filename_Chunk)

Shape_External_Filename_Chunk::Shape_External_Filename_Chunk (Chunk_With_Children * parent, const char *fdata, size_t const chunk_bytes)
: Chunk (parent, "SHPEXTFN")
{
	char const * const chunk_start = fdata;
	rescale = *((double *) fdata);
	fdata += 8;
	version_no = *((int *) fdata);
	fdata += 4;
	file_name = CopyNameFromChunk(chunk_start, chunk_bytes, fdata);
}

Shape_External_Filename_Chunk::~Shape_External_Filename_Chunk()
{
	delete[] file_name;
}

void Shape_External_Filename_Chunk::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((double *) data_start) = rescale;

	data_start += 8;

	*((int *) data_start) = version_no;

	data_start += 4;

	memcpy (data_start, file_name, strlen(file_name) + 1);
}

///////////////////////////////////////
RIF_IMPLEMENT_DYNCREATE("EXTOBJNM",Shape_External_Object_Name_Chunk)

Shape_External_Object_Name_Chunk::Shape_External_Object_Name_Chunk(Chunk_With_Children * parent, const char * oname)
: Chunk (parent, "EXTOBJNM")
{
	object_name = new char [strlen(oname)+1];
	shape_name=0;
	strcpy (object_name, oname);
	pad=0;

	RIF_Name_Chunk* rnc=(RIF_Name_Chunk*)parent->lookup_single_child("RIFFNAME");
	if(rnc)
	{
		shape_name=new char[strlen(object_name)+strlen(rnc->rif_name)+2];
		sprintf(shape_name,"%s@%s",object_name,rnc->rif_name);
	}
	else
	{
		shape_name=new char[strlen(object_name)+10+2];
		sprintf(shape_name,"%s@*NotFound*",object_name);
	}
}

Shape_External_Object_Name_Chunk::Shape_External_Object_Name_Chunk (Chunk_With_Children * parent, const char *data, size_t const chunk_bytes)
: Chunk (parent, "EXTOBJNM")
{
	char const * const chunk_start = data;
	pad=*(int*)data;
	data+=4;
	object_name = CopyNameFromChunk(chunk_start, chunk_bytes, data);
	shape_name=0;
}

Shape_External_Object_Name_Chunk::~Shape_External_Object_Name_Chunk()
{
	delete[] object_name;
	delete[] shape_name;
}

void  Shape_External_Object_Name_Chunk::post_input_processing()
{
	RIF_Name_Chunk* rnc=(RIF_Name_Chunk*)parent->lookup_single_child("RIFFNAME");
	if(rnc)
	{
		shape_name=new char[strlen(object_name)+strlen(rnc->rif_name)+2];
		sprintf(shape_name,"%s@%s",object_name,rnc->rif_name);
	}
	else
	{
		shape_name=new char[strlen(object_name)+10+2];
		sprintf(shape_name,"%s@*NotFound*",object_name);
	}
}

void Shape_External_Object_Name_Chunk::fill_data_block (char *data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*(int*)data_start =pad;

	data_start+=4;

	memcpy (data_start, object_name, strlen(object_name) + 1);
}

///////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPMORPH",Shape_Morphing_Data_Chunk,"REBSHAPE",Shape_Chunk)
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SHPMORPH",Shape_Morphing_Data_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

/*
Children for Shape_Morphing_Data_Chunk :

"SUBSHAPE"		Shape_Sub_Shape_Chunk
"FRMMORPH"		Shape_Morphing_Frame_Data_Chunk
*/


Shape_Morphing_Data_Chunk::Shape_Morphing_Data_Chunk (Shape_Chunk * parent, const char *data, size_t size)
: Chunk_With_Children (parent, "SHPMORPH"), parent_shape (parent), parent_sub_shape (0)
{
	const char * buffer_ptr = data;

	while ((data-buffer_ptr)< (signed)size)
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(data);
		data += *(int *)(data + 8);
	}
}

Shape_Morphing_Data_Chunk::Shape_Morphing_Data_Chunk (Shape_Sub_Shape_Chunk * parent, const char *data, size_t size)
: Chunk_With_Children (parent, "SHPMORPH"), parent_shape (0), parent_sub_shape (parent)
{
	const char * buffer_ptr = data;

	while ((data-buffer_ptr)< (signed)size)
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}
		DynCreate(data);
		data += *(int *)(data + 8);
	}
}

void Shape_Morphing_Data_Chunk::prepare_for_output()
{
	int max_id = 0;

	List<Chunk *> cl;
	lookup_child("SUBSHAPE",cl);

	LIF<Chunk *> cli(&cl);
	for (; !cli.done(); cli.next())
	{
		max_id = max (max_id, ((Shape_Sub_Shape_Chunk *)cli())->get_header()->file_id_num);
	}

	for (cli.restart(); !cli.done(); cli.next())
	{
		if (((Shape_Sub_Shape_Chunk *)cli())->get_header()->file_id_num == -1)
		{
			((Shape_Sub_Shape_Chunk *)cli())->get_header()->file_id_num = ++max_id;
		}
	}
	Chunk_With_Children::prepare_for_output();
}

///////////////////////////////////////

#ifdef new
#pragma message("new defined")
#endif

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("FRMMORPH",Shape_Morphing_Frame_Data_Chunk,"SHPMORPH",Shape_Morphing_Data_Chunk)

// A chunk of morph frames holds three numbers, the last a count of frames, and then three numbers for each frame. The count comes
// from the level file, and this goes on to make a list of that size and read the file into it. A count too big for the chunk would
// read past its end, and one big enough would wrap round when it's multiplied, leaving a tiny list that the loop below writes far
// past the end of. So the chunk has to be exactly the size its count says. All 761 of these in the game's own levels are.
Shape_Morphing_Frame_Data_Chunk::Shape_Morphing_Frame_Data_Chunk (Shape_Morphing_Data_Chunk * parent,const char *data, size_t size)
: Chunk (parent, "FRMMORPH")
{
	size_t const numbers_bytes = 12;
	unsigned long long const bytes_for_each_frame = 12;

	// The game keeps a count of frames in the top half of a number (mph_maxframes, in chunk_load.cpp), so it can't count more than
	// this. The most in any chunk in the game's own levels is 2.
	int const most_frames = 32767;

	a_flags = 0;
	a_speed = 0;
	num_frames = 0;
	frame_store = 0;

	if (size < numbers_bytes)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	a_flags = *((int *)data);
	data +=4;
	a_speed = *((int *)data);
	data +=4;
	int const count = *((int *)data);
	data +=4;

	// These numbers are wide enough that the sum can't wrap round, and a negative count turns into a huge number, so it's refused too.
	if (count > most_frames || (unsigned long long) count * bytes_for_each_frame != size - numbers_bytes)
	{
		Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	num_frames = count;

	if (num_frames)
		frame_store = new int [num_frames * 3];

	for (int i=0; i<num_frames; i++)
	{
		frame_store[i*3] = *((int *)data);
		data +=4;
		frame_store[i*3+1] = *((int *)data);
		data +=4;
		frame_store[i*3+2] = *((int *)data);
		data +=4;
	}
}

void Shape_Morphing_Frame_Data_Chunk::fill_data_block ( char * data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;
	*((int *) data_start) = a_flags;
	data_start += 4;
	*((int *) data_start) = a_speed;
	data_start += 4;
	*((int *) data_start) = num_frames;
	data_start += 4;
	for (int i=0; i<num_frames; i++)
	{
		*((int *) data_start) = frame_store[i*3];
		data_start += 4;
		*((int *) data_start) = frame_store[i*3 + 1];
		data_start += 4;
		*((int *) data_start) = frame_store[i*3 + 2];
		data_start += 4;
	}
}

Shape_Morphing_Frame_Data_Chunk::~Shape_Morphing_Frame_Data_Chunk()
{
	delete[] frame_store;
	while (anim_frames.size())
	{
		delete anim_frames.first_entry();
		anim_frames.delete_first_entry();
	}
}

void Shape_Morphing_Frame_Data_Chunk::prepare_for_output()
{
	// get the number of each shape in each frame

	// N.B. this relies on the fact that the shapes are numbered
	// but they will be by the parent class which will have it's prepare
	// for output called first
	
	delete[] frame_store;

	if (anim_frames.size())
	{
		frame_store = new int [ anim_frames.size() * 3 ];
	}
	else
	{
		frame_store = 0;
	}

	int num_f = 0;

	for (LIF<a_frame *> afi(&anim_frames); !afi.done(); afi.next())
	{
		int s1no, s2no;
		if (afi()->shape1a)
			s1no = afi()->shape1a->get_header()->file_id_num;
		else
			s1no = -1;
		if (afi()->shape2a)
			s2no = afi()->shape2a->get_header()->file_id_num;
		else
			s2no = -1;

		frame_store[num_f] = s1no;
		frame_store[num_f+1] = s2no;
		frame_store[num_f+2] = afi()->spare;
		num_f ++;
	}
	num_frames = num_f;
}


void Shape_Morphing_Frame_Data_Chunk::post_input_processing()
{
	List<Shape_Sub_Shape_Chunk *> shplist;
	List<Chunk *> child_lists;

	Shape_Morphing_Data_Chunk * pchnk = (Shape_Morphing_Data_Chunk *) parent;

	pchnk->lookup_child("SUBSHAPE",child_lists);

	while (child_lists.size()) {
		shplist.add_entry((Shape_Sub_Shape_Chunk *)child_lists.first_entry());
		child_lists.delete_first_entry();
	}

	LIF<Shape_Sub_Shape_Chunk *> sl(&shplist);

	for (int i = 0; i<num_frames; i++)
	{
		Shape_Chunk * sh1b = 0, *sh2b = 0;
		Shape_Sub_Shape_Chunk * sh1a = 0, *sh2a = 0;
		a_frame * fr;
		
		if (frame_store[i*2] == -1)
		{
			sh1b = pchnk->parent_shape;
		}
		else
		{
			for (; !sl.done(); sl.next()) {
				if (sl()->get_header())
					if (sl()->get_header()->file_id_num == frame_store[i])
						break;
			}
			if (!sl.done())
			{
				sh1a = sl();
			}
		}

		if (frame_store[i*2+1] == -1)
		{
			sh2b = pchnk->parent_shape;
		}
		else
		{
			for (sl.restart(); !sl.done(); sl.next()) {
				if (sl()->get_header())
					if (sl()->get_header()->file_id_num == frame_store[i+1])
						break;
			}
			if (!sl.done())
			{
				sh2a = sl();
			}
		}
		if ((sh1a || sh1b) && (sh2a || sh2b))
		{
			fr = new a_frame;
			if (sh1a)
				fr->shape1a = sh1a;
			else if (sh1b)
				fr->shape1b = sh1b;

			if (sh2a)
				fr->shape2a = sh2a;
			else if (sh2b)
				fr->shape2b = sh2b;

			fr->spare = frame_store[i+2];
			anim_frames.add_entry(fr);
		}
	}
}

/////////////////////////////////////////

// Class Shape_Sub_Shape_Chunk functions


RIF_IMPLEMENT_DYNCREATE("SUBSHAPE",Shape_Sub_Shape_Chunk)
/*
Children for Shape_Sub_Shape_Chunk :

"SHPRAWVT"		Shape_Vertex_Chunk
"SHPPOLYS"		Shape_Polygon_Chunk
"SUBSHPHD"		Shape_Sub_Shape_Header_Chunk
"SHPVNORM"		Shape_Vertex_Normal_Chunk
"SHPPNORM"		Shape_Polygon_Normal_Chunk
"SHPTEXFN"		Shape_Texture_Filenames_Chunk
"SHPUVCRD"		Shape_UV_Coord_Chunk
"SHPMRGDT"		Shape_Merge_Data_Chunk
"SHPCENTR"		Shape_Centre_Chunk
"SHPMORPH"		Shape_Morphing_Data_Chunk
"SHPEXTFL"		Shape_External_File_Chunk
"SHPPCINF"		Shape_Poly_Change_Info_Chunk
"TEXTANIM"		Animation_Chunk
"SHPFRAGS"		Shape_Fragments_Chunk
"ANIMSEQU"		Anim_Shape_Sequence_Chunk
"PNOTINBB"		Poly_Not_In_Bounding_Box_Chunk
"ANSHCEN2"		Anim_Shape_Centre_Chunk
"ASALTTEX"		Anim_Shape_Alternate_Texturing_Chunk


"SHPFNAME"		Shape_Name_Chunk
"FRAGDATA"		Shape_Fragments_Data_Chunk
"FRAGLOCN"		Shape_Fragment_Location_Chunk
*/

Shape_Sub_Shape_Chunk::Shape_Sub_Shape_Chunk(Chunk_With_Children * parent, const char *data, size_t size)
: Chunk_With_Children (parent, "SUBSHAPE"), shape_data ()
{
	const char * buffer_ptr = data;

	shape_data_store = (ChunkShape *) &shape_data;

	while ((data-buffer_ptr)< (signed)size) 
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size)) 
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(data);
		data += *(int *)(data + 8);
	}
}

Shape_Sub_Shape_Chunk::~Shape_Sub_Shape_Chunk ()
{
}

// A sub-shape becomes a shape in the game like any other, so it has to pass the same checks. It can sit in several
// different places inside a shape, and this step reaches every one of them. load_rif sees the error and stops the level.
// A sub-shape that fails stops here, because the chunks inside it use its numbers as they stand, and one of them writes.
//
// A sub-shape can't have texture animation: the game treats whatever holds that chunk as a Shape_Chunk, which a sub-shape isn't, and none of the
// 418 of them in the game's own levels is in a sub-shape.
void Shape_Sub_Shape_Chunk::post_input_processing()
{
	if (!shape_numbers_are_valid (shape_data, this) || count_children ("TEXTANIM"))
	{
		GetRootChunk()->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
		return;
	}

	Chunk_With_Children::post_input_processing();
}

Shape_Sub_Shape_Header_Chunk * Shape_Sub_Shape_Chunk::get_header()
{
	return (Shape_Sub_Shape_Header_Chunk *) this->lookup_single_child ("SUBSHPHD");
}


const char * Shape_Sub_Shape_Chunk::get_shape_name()
{
	Shape_Name_Chunk* snc=(Shape_Name_Chunk*)lookup_single_child("SHPFNAME");
	if (snc)
	{
		return snc->shape_name;
	}
	else
	{
		return(0);
	}
}

/////////////////////////////////////////

// Class Shape_Sub_Shape_Header_Chunk functions

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("SUBSHPHD",Shape_Sub_Shape_Header_Chunk,"SUBSHAPE",Shape_Sub_Shape_Chunk)

// These can only be children of Shape_Sub_Shape_Chunks
// the shape chunks data will automatically be updated by it is created

Shape_Sub_Shape_Header_Chunk::Shape_Sub_Shape_Header_Chunk (Shape_Sub_Shape_Chunk * parent, const char * hdata, size_t /*hsize*/)
	: Chunk (parent, "SUBSHPHD"),
	shape_data (parent->shape_data_store)
{
	flags = *((int *) hdata);
	hdata += 4;

	file_id_num = *((int *) (hdata));
	hdata += 4;

	parent->shape_data_store->num_verts = *((int *) (hdata));
	hdata += 4;
	parent->shape_data_store->num_polys = *((int *) (hdata));
	hdata += 4;

	parent->shape_data_store->radius = *((float *) (hdata));
	hdata += 4;

	parent->shape_data_store->max.x = *((int *) (hdata));
	hdata += 4;
	parent->shape_data_store->min.x = *((int *) (hdata));
	hdata += 4;

	parent->shape_data_store->max.y = *((int *) (hdata));
	hdata += 4;
	parent->shape_data_store->min.y = *((int *) (hdata));
	hdata += 4;

	parent->shape_data_store->max.z = *((int *) (hdata));
	hdata += 4;
	parent->shape_data_store->min.z = *((int *) (hdata));
}

Shape_Sub_Shape_Header_Chunk::~Shape_Sub_Shape_Header_Chunk()
{
}

size_t Shape_Sub_Shape_Header_Chunk::size_chunk()
{
	return chunk_size = 44 + 12;
}

void Shape_Sub_Shape_Header_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = flags;
	data_start += 4;

	*((int *) (data_start)) = file_id_num;
	data_start += 4;

	*((int *) (data_start)) = shape_data->num_verts;
	data_start += 4;
	*((int *) (data_start)) = shape_data->num_polys;
	data_start += 4;

	*((float *) (data_start)) = shape_data->radius;
	data_start += 4;

	*((int *) (data_start)) = shape_data->max.x;
	data_start += 4;
	*((int *) (data_start)) = shape_data->min.x;
	data_start += 4;

	*((int *) (data_start)) = shape_data->max.y;
	data_start += 4;
	*((int *) (data_start)) = shape_data->min.y;
	data_start += 4;

	*((int *) (data_start)) = shape_data->max.z;
	data_start += 4;
	*((int *) (data_start)) = shape_data->min.z;
}

/////////////////////////////////////////


// Class Shape_Poly_Change_Info_Chunk functions

RIF_IMPLEMENT_DYNCREATE("SHPPCINF",Shape_Poly_Change_Info_Chunk)

void Shape_Poly_Change_Info_Chunk::fill_data_block(char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = original_num_verts;

	data_start += 4;

	*((int *) data_start) = change_list.size();

	data_start += 4;

	for (LIF<poly_change_info> pcii(&change_list); !pcii.done(); pcii.next())
	{
		*((int *) data_start) = pcii().poly_num;

		data_start += 4;

		*((int *) data_start) = pcii().vert_num_before;

		data_start += 4;

		*((int *) data_start) = pcii().vert_num_after;

		data_start += 4;
	}
}

Shape_Poly_Change_Info_Chunk::Shape_Poly_Change_Info_Chunk (Chunk_With_Children * parent,const char * data, size_t size)
: Chunk (parent, "SHPPCINF")
{
	char const * const start = data;

	original_num_verts = *((int *) data);

	data += 4;

	int n_entries = ReadListCount (start, data, size, 3 * sizeof(int));

	for (int i=0; i<n_entries; i++)
	{
		poly_change_info pci;

		pci.poly_num = *((int *) data);
		data += 4;
		pci.vert_num_before = *((int *) data);
		data += 4;
		pci.vert_num_after = *((int *) data);
		data += 4;

		change_list.add_entry(pci);
	}
}


/////////////////////////////////////////

// Class Shape_Name_Chunk functions

RIF_IMPLEMENT_DYNCREATE("SHPFNAME",Shape_Name_Chunk)

Shape_Name_Chunk::Shape_Name_Chunk (Chunk_With_Children * parent, const char * sname)
: Chunk (parent, "SHPFNAME")
{
	shape_name = new char [strlen(sname)+1];
	strcpy (shape_name, sname);
}

Shape_Name_Chunk::Shape_Name_Chunk (Chunk_With_Children * parent, const char * sndata, size_t const chunk_bytes)
: Chunk (parent, "SHPFNAME")
{
	shape_name = CopyNameFromChunk(sndata, chunk_bytes, sndata);
}

Shape_Name_Chunk::~Shape_Name_Chunk ()
{
	delete[] shape_name;
}

void Shape_Name_Chunk::fill_data_block (char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	memcpy (data_start, shape_name, strlen(shape_name) + 1);
}

/////////////////////////////////////////

// Class Shape_Fragments_Chunk functions

RIF_IMPLEMENT_DYNCREATE("SHPFRAGS",Shape_Fragments_Chunk)

CHUNK_WITH_CHILDREN_LOADER("SHPFRAGS",Shape_Fragments_Chunk)
/*
Children for Shape_Fragments_Chunk :

"SUBSHAPE"		Shape_Sub_Shape_Chunk
"SHPFRGTP"		Shape_Fragment_Type_Chunk
"FRGSOUND"		Fragment_Type_Sound_Chunk
*/


/////////////////////////////////////////

// Class Shape_Fragments_Data_Chunk functions
RIF_IMPLEMENT_DYNCREATE("FRAGDATA",Shape_Fragments_Data_Chunk)

void Shape_Fragments_Data_Chunk::fill_data_block (char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((int *) data_start) = num_fragments;

	data_start += 4;

	*((int *) data_start) = pad1;

	data_start += 4;

	*((int *) data_start) = pad2;

	data_start += 4;

	*((int *) data_start) = pad3;
}

Shape_Fragments_Data_Chunk::Shape_Fragments_Data_Chunk (Chunk_With_Children * parent, const char * sdata, size_t /*ssize*/)
: Chunk (parent, "FRAGDATA")
{
	num_fragments = *((int *) sdata);

	sdata += 4;

	pad1 = *((int *) sdata);

	sdata += 4;

	pad2 = *((int *) sdata);

	sdata += 4;

	pad3 = *((int *) sdata);

	sdata += 4;
}


/////////////////////////////////////////

// Class Shape_Fragments_Location_Chunk functions
RIF_IMPLEMENT_DYNCREATE("FRAGLOCN",Shape_Fragment_Location_Chunk)

void Shape_Fragment_Location_Chunk::fill_data_block (char * data_start)
{
	strncpy (data_start, identifier, 8);

	data_start += 8;

	*((int *) data_start) = chunk_size;

	data_start += 4;

	*((ChunkVectorInt *) data_start) = frag_loc;

	data_start += sizeof(ChunkVectorInt);

	*((int *) data_start) = pad1;

	data_start += 4;

	*((int *) data_start) = pad2;

	data_start += 4;

	*((int *) data_start) = pad3;

	data_start += 4;

	*((int *) data_start) = pad4;
}

Shape_Fragment_Location_Chunk::Shape_Fragment_Location_Chunk (Chunk_With_Children * parent, const char * sdata, size_t /*ssize*/)
: Chunk (parent, "FRAGLOCN")
{
	frag_loc = *((ChunkVectorInt *) sdata);

	sdata += sizeof(ChunkVectorInt);

	pad1 = *((int *) sdata);

	sdata += 4;

	pad2 = *((int *) sdata);

	sdata += 4;

	pad3 = *((int *) sdata);

	sdata += 4;

	pad4 = *((int *) sdata);
	
}

/////////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE("SHPFRGTP",Shape_Fragment_Type_Chunk)
Shape_Fragment_Type_Chunk::Shape_Fragment_Type_Chunk(Chunk_With_Children* parent,const char* name)
:Chunk(parent,"SHPFRGTP")
{
	frag_type_name=new char[strlen(name)+1];
	strcpy(frag_type_name,name);
	pad1=pad2-0;
}

Shape_Fragment_Type_Chunk::Shape_Fragment_Type_Chunk(Chunk_With_Children* const parent,const char* data,size_t const chunk_bytes)
:Chunk(parent,"SHPFRGTP")
{
	size_t length=0;
	frag_type_name=CopyNameFromChunk(data,chunk_bytes,data,&length);
	data+=(length+4)&~3;

	pad1=*(int*)data;
	data+=4;
	pad2=*(int*)data;
	data+=4;
}

Shape_Fragment_Type_Chunk::~Shape_Fragment_Type_Chunk()
{
	delete[] frag_type_name;
}

void Shape_Fragment_Type_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;

	strcpy(data_start,frag_type_name);
	data_start+=(strlen(frag_type_name)+4)&~3;

	*(int*)data_start=pad1;
	data_start+=4;
	*(int*)data_start=pad2;
	data_start+=4;
}

size_t Shape_Fragment_Type_Chunk::size_chunk()
{
	chunk_size=12+8;
	chunk_size+=(strlen(frag_type_name)+4)&~3;
	return chunk_size;
}


/////////////////////////////////////////

// Class Anim_Shape_Sequence_Chunk functions

RIF_IMPLEMENT_DYNCREATE("ANIMSEQU",Anim_Shape_Sequence_Chunk)

/*
Children for Anim_Shape_Sequence_Chunk :
"ANISEQDT"		Anim_Shape_Sequence_Data_Chunk
"ANIMFRAM"		Anim_Shape_Frame_Chunk
*/

Anim_Shape_Sequence_Chunk::Anim_Shape_Sequence_Chunk(Chunk_With_Children * const parent, const char *data, size_t const size)
: Chunk_With_Children (parent, "ANIMSEQU")
{
	sequence_data_store=(ChunkAnimSequence*)&sequence_data;

	const char * buffer_ptr = data;

	while ((data-buffer_ptr)< (signed)size)
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}

		DynCreate(data);
		data += *(int *)(data + 8);
	}
	if(!ConstructSequenceDataFromChildren())
	{
		//this sequence is no longer valid
		delete this;
	}
}

int Anim_Shape_Sequence_Chunk::ConstructSequenceDataFromChildren()
{
	Anim_Shape_Sequence_Data_Chunk* assdc=(Anim_Shape_Sequence_Data_Chunk*)lookup_single_child("ANISEQDT");
	if(!assdc)
	{
		return 0;
	}
	sequence_data_store->name=assdc->name;
	sequence_data_store->SequenceNum=assdc->SequenceNum;
	sequence_data_store->flags=assdc->flags;
	sequence_data_store->pad2=assdc->pad2;
	sequence_data_store->pad3=assdc->pad3;
	sequence_data_store->pad4=assdc->pad4;

	List<Chunk*> chlist;
	lookup_child("ANIMFRAM",chlist);
	sequence_data_store->NumFrames=chlist.size();

	if(sequence_data_store->NumFrames)
	{
		sequence_data_store->Frames=new ChunkAnimFrame*[sequence_data_store->NumFrames];
		for(int i=0;i<sequence_data_store->NumFrames;i++)
		{
			sequence_data_store->Frames[i]=0;
		}
	}

	for(LIF<Chunk*> chlif(& chlist);!chlif.done();chlif.next())
	{
		Anim_Shape_Frame_Chunk* asfc=(Anim_Shape_Frame_Chunk*)chlif();

		Anim_Shape_Frame_Data_Chunk* asfdc=(Anim_Shape_Frame_Data_Chunk*)asfc->lookup_single_child("ANIFRADT");
		if(!asfdc) return 0;
		if(asfdc->FrameNum<0 || asfdc->FrameNum>=sequence_data_store->NumFrames) return 0;

		if(sequence_data_store->Frames[asfdc->FrameNum])return 0;
		sequence_data_store->Frames[asfdc->FrameNum]=new ChunkAnimFrame;
		ChunkAnimFrame* caf=sequence_data_store->Frames[asfdc->FrameNum];

		Shape_Vertex_Chunk* svc=(Shape_Vertex_Chunk*) asfc->lookup_single_child("SHPRAWVT");
		if(!svc)return 0;

		Shape_Polygon_Normal_Chunk* spnc=(Shape_Polygon_Normal_Chunk*)asfc->lookup_single_child("SHPPNORM");
		if(!spnc) return 0;

		caf->name=asfdc->name;
		caf->flags=asfdc->flags;
		caf->num_interp_frames=asfdc->num_interp_frames;
		caf->pad3=asfdc->pad3;
		caf->pad4=asfdc->pad4;

		caf->num_verts=svc->num_verts;
		caf->v_list=(ChunkVectorInt*)svc->vert_data;

		caf->num_polys=spnc->num_polys;
		caf->p_normal_list=(ChunkVectorFloat*)spnc->poly_norm_data;
	}
	return 1;
}

void Anim_Shape_Sequence_Chunk::post_input_processing()
{
	List<int>* poly_not_in_bb=0;
	List<Chunk*> chlist;
	parent->lookup_child("PNOTINBB",chlist);
	if(chlist.size())
		poly_not_in_bb=&((Poly_Not_In_Bounding_Box_Chunk*)chlist.first_entry())->poly_no;
	if(!strcmp(parent->identifier,"REBSHAPE"))
	{
		sequence_data_store->UpdateNormalsAndExtents(&((Shape_Chunk*)parent)->shape_data,poly_not_in_bb);
	}
	else if(!strcmp(parent->identifier,"SUBSHAPE"))
	{
		sequence_data_store->UpdateNormalsAndExtents(&((Shape_Sub_Shape_Chunk*)parent)->shape_data,poly_not_in_bb);
	}

	Chunk_With_Children::post_input_processing();
}


void Anim_Shape_Sequence_Chunk::set_sequence_flags(int flags)
{
	sequence_data_store->flags=flags;
}
void Anim_Shape_Sequence_Chunk::set_frame_flags(int frameno,int flags)
{
	if(frameno< sequence_data_store->NumFrames)
	{
		if(sequence_data_store->Frames[frameno])
		{
			sequence_data_store->Frames[frameno]->flags=flags;
		}
	}
}

void Anim_Shape_Sequence_Chunk::GenerateInterpolatedFrames()
{
	if(!strcmp(parent->identifier,"REBSHAPE"))
	{
		sequence_data_store->GenerateInterpolatedFrames(&((Shape_Chunk*)parent)->shape_data);
	}
	else if(!strcmp(parent->identifier,"SUBSHAPE"))
	{
		sequence_data_store->GenerateInterpolatedFrames(&((Shape_Sub_Shape_Chunk*)parent)->shape_data);
	}
}
/////////////////////////////////////////

// Class Anim_Shape_Frame_Chunk functions
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("ANIMFRAM",Anim_Shape_Frame_Chunk,"ANIMSEQU",Anim_Shape_Sequence_Chunk)

/*
Children for Anim_Shape_Frame_Chunk :

"ANIFRADT"		Anim_Shape_Frame_Data_Chunk
"SHPPNORM"		Shape_Polygon_Normal_Chunk
"SHPRAWVT"		Shape_Vertex_Chunk
*/

Anim_Shape_Frame_Chunk::Anim_Shape_Frame_Chunk(Anim_Shape_Sequence_Chunk *const parent, const char *data, size_t const size)
: Chunk_With_Children (parent, "ANIMFRAM")
{
	const char * buffer_ptr = data;

	while ((data-buffer_ptr)< (signed)size)
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}
		DynCreate(data);
		data += *(int *)(data + 8);
	}
}

/////////////////////////////////////////

// Class Anim_Shape_Sequence_Data_Chunk functions

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("ANISEQDT",Anim_Shape_Sequence_Data_Chunk,"ANIMSEQU",Anim_Shape_Sequence_Chunk)

Anim_Shape_Sequence_Data_Chunk::Anim_Shape_Sequence_Data_Chunk(Anim_Shape_Sequence_Chunk* const parent,const char* data,size_t const chunk_bytes)
: Chunk(parent, "ANISEQDT")
{
	char const * const chunk_start = data;
	SequenceNum=*(int*)data;
	data+=4;
	flags=*(int*)data;
	data+=4;
	pad2=*(int*)data;
	data+=4;
	pad3=*(int*)data;
	data+=4;
	pad4=*(int*)data;
	data+=4;
	name=CopyNameFromChunk(chunk_start,chunk_bytes,data);
}

void Anim_Shape_Sequence_Data_Chunk::fill_data_block(char* datastart)
{
	strncpy (datastart, identifier, 8);
	datastart += 8;
	*((int *) datastart) = chunk_size;
	datastart += 4;

	*(int*)datastart=SequenceNum;
	datastart+=4;
	*(int*)datastart=flags;
	datastart+=4;
	*(int*)datastart=pad2;
	datastart+=4;
	*(int*)datastart=pad3;
	datastart+=4;
	*(int*)datastart=pad4;
	datastart+=4;

	strcpy(datastart,name ? name : "");
}

size_t Anim_Shape_Sequence_Data_Chunk::size_chunk()
{
	return chunk_size =	12+20
		+(name ? strlen(name) : 0)
		+3 +3&~3;
}


/////////////////////////////////////////

// Class Anim_Shape_Frame_Data_Chunk functions
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("ANIFRADT",Anim_Shape_Frame_Data_Chunk,"ANIMFRAM",Anim_Shape_Frame_Chunk)

Anim_Shape_Frame_Data_Chunk::Anim_Shape_Frame_Data_Chunk(Anim_Shape_Frame_Chunk* const parent,const char* data,size_t const chunk_bytes)
: Chunk(parent, "ANIFRADT")
{
	char const * const chunk_start = data;
	FrameNum=*(int*)data;
	data+=4;
	flags=*(int*)data;
	data+=4;
	num_interp_frames=*(int*)data;
	data+=4;
	pad3=*(int*)data;
	data+=4;
	pad4=*(int*)data;
	data+=4;
	name=CopyNameFromChunk(chunk_start,chunk_bytes,data);
}


void Anim_Shape_Frame_Data_Chunk::fill_data_block(char* datastart)
{
	strncpy (datastart, identifier, 8);
	datastart += 8;
	*((int *) datastart) = chunk_size;
	datastart += 4;

	*(int*)datastart=FrameNum;
	datastart+=4;
	*(int*)datastart=flags;
	datastart+=4;
	*(int*)datastart=num_interp_frames;
	datastart+=4;
	*(int*)datastart=pad3;
	datastart+=4;
	*(int*)datastart=pad4;
	datastart+=4;

	strcpy(datastart,name ? name : "");
}

size_t Anim_Shape_Frame_Data_Chunk::size_chunk()
{
	return chunk_size =	12+20
		+(name ? strlen(name) : 0)
		+3 +3&~3;
}

/////////////////////////////////////////

// Class Anim_Shape_Alternate_Texturing_Chunk functions
RIF_IMPLEMENT_DYNCREATE("ASALTTEX",Anim_Shape_Alternate_Texturing_Chunk)

/*
Children for Anim_Shape_Alternate_Texturing_Chunk :
"SUBSHAPE"		Shape_Sub_Shape_Chunk
*/

Anim_Shape_Alternate_Texturing_Chunk::Anim_Shape_Alternate_Texturing_Chunk(Chunk_With_Children *const parent, const char *data, size_t const size)
: Chunk_With_Children (parent, "ASALTTEX")
{
	const char * buffer_ptr = data;

	while ((data-buffer_ptr)< (signed)size)
	{
		if (!ChildChunkFits(data, buffer_ptr, (signed)size))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			break;
		}
		DynCreate(data);
		data += *(int *)(data + 8);
	}
}

/////////////////////////////////////////

// Class Poly_Not_In_Bounding_Box_Chunk functions
RIF_IMPLEMENT_DYNCREATE("PNOTINBB",Poly_Not_In_Bounding_Box_Chunk)

Poly_Not_In_Bounding_Box_Chunk::Poly_Not_In_Bounding_Box_Chunk(Chunk_With_Children* const parent,const char* data,size_t const datasize)
:Chunk(parent,"PNOTINBB")
{
	for(int i=0;i<( ( (signed)datasize ) /4)-2;i++)
	{
		poly_no.add_entry(*(int*)data);
		data+=4;
	}
	pad1=*(int*)data;
	data+=4;
	pad2=*(int*)data;
}

Poly_Not_In_Bounding_Box_Chunk::Poly_Not_In_Bounding_Box_Chunk(Chunk_With_Children* const parent)
:Chunk(parent,"PNOTINBB")
{
	pad1=pad2=0;
}

size_t Poly_Not_In_Bounding_Box_Chunk::size_chunk()
{
	return chunk_size=(20+4*poly_no.size());
}

void Poly_Not_In_Bounding_Box_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;

	for(LIF<int> plif(&poly_no);!plif.done();plif.next())
	{
		*(int*)data_start=plif();
		data_start+=4;
	}
	*(int*)data_start=pad1;
	data_start+=4;
	*(int*)data_start=pad2;
}


/////////////////////////////////////////

// Class Anim_Shape_Centre_Chunk functions

RIF_IMPLEMENT_DYNCREATE("ANSHCEN2",Anim_Shape_Centre_Chunk)

Anim_Shape_Centre_Chunk::Anim_Shape_Centre_Chunk(Chunk_With_Children* const parent,const char* data,size_t const /*datasize*/)
:Chunk(parent,"ANSHCEN2")
{
	Centre=*(ChunkVectorInt*)data;
	data+=sizeof(ChunkVectorInt);
	flags=*(int*)data;
	data+=4;
	pad2=*(int*)data;
}

Anim_Shape_Centre_Chunk::Anim_Shape_Centre_Chunk(Chunk_With_Children* const parent)
:Chunk(parent,"ANSHCEN2")
{
	Centre.x=0;
	Centre.y=0;
	Centre.z=0;
	flags=pad2=0;
}

size_t Anim_Shape_Centre_Chunk::size_chunk()
{
	return chunk_size=(20+sizeof(ChunkVectorInt));
}

void Anim_Shape_Centre_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;

	*(ChunkVectorInt*)data_start=Centre;
	data_start+=sizeof(ChunkVectorInt);
	*(int*)data_start=flags;
	data_start+=4;
	*(int*)data_start=pad2;
}
