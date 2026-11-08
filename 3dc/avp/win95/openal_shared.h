// What the game's OpenAL sound code (openal_audio.cpp) and its streams (openal_stream.cpp) share. Written 2026-11-08 by Melanikus for
// the AvP port.
//
// A stream is sound that comes in pieces while it plays: the level's music, and a film's sound. Its code was in openal_audio.cpp, and
// moved to a file of its own so that a test can build it with a stand-in for OpenAL and two real threads (tests/audio_stream_test.cpp).
// Everything here needs only OpenAL's own header, so the test needs nothing else of the game's.

#ifndef OPENAL_SHARED_H
#define OPENAL_SHARED_H

#include <stdint.h>
#include <assert.h>
#include <AL/al.h>

// One of OpenAL's sound sources: a place a sound plays from. openal_audio.cpp keeps a list of them, and hands one out to whoever asks.
struct sndSource
{
	bool _isFree;
	ALuint _alSource;

	// constructor
	sndSource()
	{
		_isFree = true;
	}
};

// OpenAL's name for sound with this many bits a sample and this many channels: 1 or 2 channels of 8 or 16 bits
inline ALenum GetALFormat(uint16_t bitsPerSample, uint16_t nChannels)
{
	ALenum alFormat = 0;

	if (bitsPerSample == 8)
	{
		if (nChannels == 1) {
			alFormat = AL_FORMAT_MONO8;
		}
		else if (nChannels == 2) {
			alFormat = AL_FORMAT_STEREO8;
		}
	}
	else if (bitsPerSample == 16)
	{
		if (nChannels == 1) {
			alFormat = AL_FORMAT_MONO16;
		}
		else if (nChannels == 2) {
			alFormat = AL_FORMAT_STEREO16;
		}
	}
	else {
		assert(1==0);
	}
	return alFormat;
}

// In openal_audio.cpp, which keeps the sound system's own state: whether sound started, its list of sources, and its table of volumes
bool OpenAL_SoundIsOn();
sndSource *OpenAL_FindFreeSource();
float OpenAL_StreamGain(uint32_t volume);   // a stream's volume, 0 to 127, as OpenAL's gain; anything more is 127
ALenum AlCheckError();                      // takes OpenAL's last error, and says on the console what it was

#endif
