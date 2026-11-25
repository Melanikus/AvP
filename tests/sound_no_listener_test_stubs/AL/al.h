// Stand-in for OpenAL's header, for tests/sound_no_listener_test.cpp. Written 2026-11-25 by Melanikus (Vasquez) for the AvP port.
//
// Only what the game's sound code (3dc/avp/win95/openal_audio.cpp, openal_shared.h and psndplat.h) uses. The numbers are OpenAL's own
// (OpenAL Soft's include/AL/al.h, which is in the public domain). The functions are stand-ins in the test: a pretend sound card that
// hands out sources and buffers and remembers whether each source is playing. No sound is played.
#ifndef _sound_no_listener_test_al_h
#define _sound_no_listener_test_al_h 1

typedef char ALboolean;
typedef char ALchar;
typedef int ALint;
typedef unsigned int ALuint;
typedef int ALsizei;
typedef int ALenum;
typedef float ALfloat;
typedef void ALvoid;

#define AL_NONE                                  0
#define AL_FALSE                                 0
#define AL_TRUE                                  1
#define AL_NO_ERROR                              0
#define AL_INVALID_NAME                          0xA001
#define AL_INVALID_ENUM                          0xA002
#define AL_INVALID_OPERATION                     0xA004
#define AL_OUT_OF_MEMORY                         0xA005
#define AL_SOURCE_RELATIVE                       0x202
#define AL_PITCH                                 0x1003
#define AL_POSITION                              0x1004
#define AL_DIRECTION                             0x1005
#define AL_VELOCITY                              0x1006
#define AL_LOOPING                               0x1007
#define AL_BUFFER                                0x1009
#define AL_GAIN                                  0x100A
#define AL_ORIENTATION                           0x100F
#define AL_SOURCE_STATE                          0x1010
#define AL_PLAYING                               0x1012
#define AL_PAUSED                                0x1013
#define AL_STOPPED                               0x1014
#define AL_REFERENCE_DISTANCE                    0x1020
#define AL_ROLLOFF_FACTOR                        0x1021
#define AL_BYTE_OFFSET                           0x1026
#define AL_FORMAT_MONO8                          0x1100
#define AL_FORMAT_MONO16                         0x1101
#define AL_FORMAT_STEREO8                        0x1102
#define AL_FORMAT_STEREO16                       0x1103

ALenum alGetError(void);
void *alGetProcAddress(const ALchar *fname);
ALenum alGetEnumValue(const ALchar *ename);
void alDistanceModel(ALenum distanceModel);
void alListenerf(ALenum param, ALfloat value);
void alListenerfv(ALenum param, const ALfloat *values);
void alGenSources(ALsizei n, ALuint *sources);
void alDeleteSources(ALsizei n, const ALuint *sources);
void alSourcef(ALuint source, ALenum param, ALfloat value);
void alSource3f(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3);
void alSourcefv(ALuint source, ALenum param, const ALfloat *values);
void alSourcei(ALuint source, ALenum param, ALint value);
void alSource3i(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3);
void alGetSourcei(ALuint source, ALenum param, ALint *value);
void alGetSourceiv(ALuint source, ALenum param, ALint *values);
void alSourcePlay(ALuint source);
void alSourceStop(ALuint source);
void alSourcePause(ALuint source);
void alGenBuffers(ALsizei n, ALuint *buffers);
void alDeleteBuffers(ALsizei n, const ALuint *buffers);
void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei samplerate);

#endif
