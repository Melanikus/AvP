// Modified 2026-11-05 by Melanikus for the AvP port:
//   says how big a film's ring of sound has to be, so a film with very little sound can't freeze
#ifndef _FmvPlayback_h_
#define _FmvPlayback_h_

#include <stdint.h>


#include "RingBuffer.h"
#include "AudioStreaming.h"
#include "TextureManager.h"

enum fmvErrors
{
	FMV_OK,
	FMV_INVALID_FILE,
	FMV_ERROR
};

/* structure holds pointers to y, u, v channels */
typedef struct _OggPlayYUVChannels 
{
	unsigned char * ptry;
	unsigned char * ptru;
	unsigned char * ptrv;
	int				y_width;
	int				y_height;
	int				y_pitch;
	int				uv_width;
	int				uv_height;
	int				uv_pitch;
} OggPlayYUVChannels;

/* structure holds pointers to y, u, v channels */
typedef struct _OggPlayRGBChannels 
{
	unsigned char * ptro;
	int				rgb_width;
	int				rgb_height;
	int				rgb_pitch;
} OggPlayRGBChannels;

void oggplay_yuv2rgb(OggPlayYUVChannels * yuv, OggPlayRGBChannels * rgb);




// How big a film's ring of sound has to be. One thread puts a frame's sound in, and has to wait until there's room for it. Another takes it out to play, and has to wait until there's
// a whole stream buffer in there. If the ring holds less than one of each, there are times when neither can go on, and the film freezes with both waiting. It's usually three frames' worth,
// which is plenty for the game's own films, but a crafted film can say its frames have hardly any sound. The largest sizes the decoders accept are well short of overflowing a uint32_t.
inline uint32_t FmvPlayback_RingSize(uint32_t frameSoundSize, uint32_t streamBufferSize, uint32_t usualFramesOfSound)
{
	uint32_t const usual = frameSoundSize * usualFramesOfSound;
	uint32_t const least = frameSoundSize + streamBufferSize;
	return (usual < least) ? least : usual;
}

#endif
