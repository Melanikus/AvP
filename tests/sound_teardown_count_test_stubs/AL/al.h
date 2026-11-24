// Stand-in for OpenAL's header, for tests/sound_teardown_count_test.cpp. Written 2026-11-24 by Melanikus (Vasquez) for the AvP port.
//
// Only the names the game's sound headers (3dc/avp/win95/psndplat.h and openal_shared.h) need for jsndsup.cpp to build: the test plays no
// sound and calls no OpenAL function. The numbers are OpenAL's own.
#ifndef _sound_teardown_count_test_al_h
#define _sound_teardown_count_test_al_h 1

typedef unsigned int ALuint;
typedef int ALenum;

#define AL_FORMAT_MONO8      0x1100
#define AL_FORMAT_MONO16     0x1101
#define AL_FORMAT_STEREO8    0x1102
#define AL_FORMAT_STEREO16   0x1103

#endif
