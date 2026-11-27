// Stand-in for pthreads' header, for tests/sound_no_listener_test.cpp. Written 2026-11-25 by Melanikus (Vasquez) for the AvP port.
//
// openal_audio.cpp reads audioStreaming.h, whose sound stream names these two types. The test plays no stream, so only the names are here.
#ifndef _sound_no_listener_test_pthread_h
#define _sound_no_listener_test_pthread_h 1

typedef void *pthread_t;
typedef void *pthread_mutex_t;

#endif
