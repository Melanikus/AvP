// Stand-in for pthreads' header, for tests/vorbis_test.cpp: only what the game's music and sound headers use.
#ifndef _vorbis_test_pthread_h
#define _vorbis_test_pthread_h 1

typedef int pthread_t;
typedef int pthread_mutex_t;

int pthread_create(pthread_t * thread, void * attributes, void * (*start)(void *), void * argument);
int pthread_join(pthread_t thread, void ** result);

#endif
