#ifndef _huffman_hpp_included
#define _huffman_hpp_included 1

#define MAX_DEPTH 11
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
extern char *HuffmanDecompress(const HuffmanPackage *inpackage);
/* for untrusted data (save games, text files): packageSize = bytes available from inpackage on; NULL if not safely
   and completely decodable */
extern char *HuffmanDecompressChecked(const HuffmanPackage *inpackage, int packageSize);

#define COMPRESSED_RIF_IDENTIFIER "REBCRIF1"

#endif