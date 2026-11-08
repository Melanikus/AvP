// Stand-in for OpenAL's header, for tests/audio_stream_test.cpp. Written 2026-11-08 by Melanikus for the AvP port.
//
// Only what the sound stream's code (3dc/avp/win95/openal_stream.cpp and openal_shared.h) uses. The numbers are OpenAL's own. The
// functions are stand-ins in the test: a pretend sound source that plays every buffer the moment it's queued.
#ifndef _audio_stream_test_al_h
#define _audio_stream_test_al_h 1

typedef unsigned int ALuint;
typedef int ALint;
typedef int ALenum;
typedef int ALsizei;
typedef float ALfloat;
typedef void ALvoid;

#define AL_NO_ERROR          0
#define AL_INVALID_VALUE     0xA003
#define AL_BUFFER            0x1009
#define AL_GAIN              0x100A
#define AL_BUFFERS_PROCESSED 0x1016
#define AL_FORMAT_MONO8      0x1100
#define AL_FORMAT_MONO16     0x1101
#define AL_FORMAT_STEREO8    0x1102
#define AL_FORMAT_STEREO16   0x1103

ALenum alGetError(void);
void alGenBuffers(ALsizei n, ALuint * buffers);
void alDeleteBuffers(ALsizei n, const ALuint * buffers);
void alBufferData(ALuint buffer, ALenum format, const ALvoid * data, ALsizei size, ALsizei frequency);
void alSourceQueueBuffers(ALuint source, ALsizei n, const ALuint * buffers);
void alSourceUnqueueBuffers(ALuint source, ALsizei n, ALuint * buffers);
void alGetSourcei(ALuint source, ALenum parameter, ALint * value);
void alSourcePlay(ALuint source);
void alSourceStop(ALuint source);
void alSourcef(ALuint source, ALenum parameter, ALfloat value);
void alSourcei(ALuint source, ALenum parameter, ALint value);

#endif
