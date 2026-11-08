// Stand-in for pthreads' header, for tests/audio_stream_test.cpp. Written 2026-11-08 by Melanikus for the AvP port.
//
// These are real threads and real locks, made with C++'s own (std::thread and std::mutex, in the test), so the sound stream's two
// threads really run at the same time. Only what the stream's code uses is here.
#ifndef _audio_stream_test_pthread_h
#define _audio_stream_test_pthread_h 1

typedef void *pthread_t;         // in the test, a std::thread
typedef void *pthread_mutex_t;   // in the test, a std::mutex

int pthread_create(pthread_t * thread, void * attributes, void * (*start)(void *), void * argument);
int pthread_join(pthread_t thread, void ** result);
int pthread_mutex_init(pthread_mutex_t * mutex, void * attributes);
int pthread_mutex_lock(pthread_mutex_t * mutex);
int pthread_mutex_unlock(pthread_mutex_t * mutex);
int pthread_mutex_destroy(pthread_mutex_t * mutex);

#endif
