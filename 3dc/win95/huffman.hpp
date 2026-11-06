// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   declares the checked Huffman decoder (see huffman.cpp)
#ifndef _huffman_hpp_included
#define _huffman_hpp_included 1

#define MAX_DEPTH 11

// How many bytes of zeros come after decoded data, and after a level read from a file. Most chunks read a fixed set of numbers, or a name, with no
// look at their own size. A chunk can be anywhere in a file, and the reading only stops at a zero byte, or when the numbers run out, so
// these bytes mean none of that can leave the block of memory. The longest fixed read is 32768 bytes (Coloured_Polygons_Lookup_Chunk).
#define HUFFMAN_SPARE_BYTES 65536
typedef struct
{
	char			Identifier[8];
    int				CompressedDataSize;
    int				UncompressedDataSize;
    int				CodelengthCount[MAX_DEPTH];
    unsigned char	ByteAssignment[256];
} HuffmanPackage;

/* KJL 17:16:03 17/09/98 - Compression */
extern HuffmanPackage *HuffmanCompression(unsigned char *sourcePtr, int length);

/* KJL 16:53:53 19/09/98 - Decompression */
/* packageSize = bytes available from inpackage on; NULL if not safely
   and completely decodable */
extern char *HuffmanDecompressChecked(const HuffmanPackage *inpackage, int packageSize);

#define COMPRESSED_RIF_IDENTIFIER "REBCRIF1"

#endif