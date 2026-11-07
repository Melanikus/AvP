// A stand-in for the threads library's header, for the video test only. Written 2026-11-04 by Melanikus for the AvP port.
//
// The video players start threads to decode with. This test feeds them by hand instead, one call at a time, so that a failure
// can be repeated exactly. So starting a thread here does nothing, and neither does taking a lock.

#ifndef fmv_test_pthread_h
#define fmv_test_pthread_h

typedef int pthread_t;
typedef int pthread_mutex_t;

// Set by a check, to find out what happens when a thread can't be started
extern bool pthreadCreateFails;

// Set by a check, to let some threads start and the next one fail. Below zero means there's no limit.
extern int pthreadCreatesLeft;

// How many times something waited for a thread to finish
extern int pthreadJoins;

inline int pthread_create(pthread_t *, void *, void *(*)(void *), void *)
{
	if (pthreadCreateFails || pthreadCreatesLeft == 0)
	{
		return 1;
	}

	if (pthreadCreatesLeft > 0)
	{
		pthreadCreatesLeft--;
	}

	return 0;
}

inline int pthread_join(pthread_t, void **) { pthreadJoins++; return 0; }
inline int pthread_mutex_init(pthread_mutex_t *, void *) { return 0; }
inline int pthread_mutex_destroy(pthread_mutex_t *) { return 0; }
inline int pthread_mutex_lock(pthread_mutex_t *) { return 0; }
inline int pthread_mutex_unlock(pthread_mutex_t *) { return 0; }

#endif
