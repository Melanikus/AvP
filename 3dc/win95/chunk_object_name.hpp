// Written 2026-11-03 by Melanikus for the AvP port:
//   copies an object's name out of a level file safely. tests/rebeccas_test.cpp runs it through the real
//   chunk constructors.

#ifndef _chunk_object_name_hpp
#define _chunk_object_name_hpp 1

#include <stddef.h>
#include <string.h>

// How many bytes an object's name can use, counting the zero byte that marks where it ends. That leaves 63 characters.
// The longest name in the stock levels is 53 characters. tests/rebeccas_test.cpp shows what happens at each edge.
#define OBJECT_NAME_BYTES 64

// In a level file, an object's header chunk starts with this many bytes of fixed data. The name fills the rest.
#define OBJECT_HEADER_FIXED_BYTES 60

// Copies the name out of an object's header chunk and into dest.
//
// chunk_data is where the chunk's data starts, and chunk_bytes is how many bytes of it there are. All of this comes
// from a file, so nothing about the name can be trusted: there may be no room left for a name at all, it may be too
// long for dest, and it may not have the zero byte that marks its end. Whatever the file holds, this function:
//  - never reads past the end of the chunk,
//  - never writes past the end of dest (a name too long for dest is cut short),
//  - always leaves dest with a zero byte after the name.
//
// The compiler works out dest_bytes from the real size of the array passed in, so the two can never disagree.
template <size_t dest_bytes>
inline void CopyObjectName(char (&dest)[dest_bytes], char const * chunk_data, size_t chunk_bytes)
{
	size_t const name_room = chunk_bytes > OBJECT_HEADER_FIXED_BYTES ? chunk_bytes - OBJECT_HEADER_FIXED_BYTES : 0;
	char const * const name = name_room ? chunk_data + OBJECT_HEADER_FIXED_BYTES : chunk_data;

	size_t name_length = 0;
	while (name_length < name_room && name_length < dest_bytes - 1 && name[name_length])
	{
		name_length++;
	}

	memcpy(dest, name, name_length);
	dest[name_length] = '\0';
}

#endif
