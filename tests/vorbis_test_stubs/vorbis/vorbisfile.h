// Stand-in for libvorbis's header, for tests/vorbis_test.cpp: only what VorbisPlayer.cpp uses. The numbers are libvorbis's own.
#ifndef _vorbis_test_vorbisfile_h
#define _vorbis_test_vorbisfile_h 1

#define OV_HOLE -3
#define OV_EBADLINK -137

struct vorbis_info
{
	int channels;
	long rate;
};

struct OggVorbis_File
{
	int unused;
};

struct ov_callbacks
{
	int unused;
};

extern ov_callbacks OV_CALLBACKS_DEFAULT;

int ov_open_callbacks(void * datasource, OggVorbis_File * file, char const * initial, long initialBytes, ov_callbacks callbacks);
vorbis_info * ov_info(OggVorbis_File * file, int link);
long ov_read(OggVorbis_File * file, char * buffer, int length, int bigEndian, int wordSize, int isSigned, int * bitstream);
int ov_raw_seek(OggVorbis_File * file, long long position);
int ov_clear(OggVorbis_File * file);

#endif
