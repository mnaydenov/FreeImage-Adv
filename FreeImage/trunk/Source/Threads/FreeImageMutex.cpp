#include <stdlib.h>
#include "FreeImageThreads.h"

#if (defined(_WIN32) || defined(_WIN64)) || defined(__MINGW32__)
#include <windows.h>
#define HAVE_WINTHREAD
#elif defined(_POSIX_THREADS)
#include <pthread.h>
#define HAVE_PTHREAD
#endif // OS defines

namespace FreeImage {

#ifdef HAVE_WINTHREAD

class MutexImpl {
private:
	//! Win32 mutex
	CRITICAL_SECTION _mutex;
public:
	MutexImpl() {
		InitializeCriticalSection(&_mutex);
	}
	~MutexImpl() {
		DeleteCriticalSection(&_mutex);
	}
	void lock() {
		EnterCriticalSection(&_mutex);
	}
	void unlock() {
		LeaveCriticalSection(&_mutex);
	}
};

#elif defined(HAVE_PTHREAD) // POSIX thread

class MutexImpl {
private:
	//! POSIX mutex
	pthread_mutex_t _mutex;
public:
	MutexImpl() {
		pthread_mutex_init(&_mutex, 0);
	}
	~MutexImpl() {
		pthread_mutex_destroy(&_mutex);
	}
	void lock() {
		pthread_mutex_lock(&_mutex);
	}
	void unlock() {
		pthread_mutex_unlock(&_mutex);
	}
};

#else

class MutexImpl {
private:
public:
	MutexImpl() {
	}
	~MutexImpl() {
	}
	void lock() {
	}
	void unlock() {
	}
};

#endif // HAVE_WINTHREAD

// --------------------------------------------------------------------------


Mutex::Mutex() : _mutex(NULL) {
	MutexImpl *mutex = new MutexImpl();
	_mutex = mutex;
}

Mutex::~Mutex() {
	MutexImpl *mutex = (MutexImpl*)_mutex;
	delete mutex;
}

void Mutex::lock() {
	MutexImpl *mutex = (MutexImpl*)_mutex;
	if (mutex) {
		mutex->lock();
	}
}

void Mutex::unlock() {
	MutexImpl *mutex = (MutexImpl*)_mutex;
	if (mutex) {
		mutex->unlock();
	}
}

} // namespace FreeImage

