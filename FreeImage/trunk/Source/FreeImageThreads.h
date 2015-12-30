#ifndef FREEIMAGE_THREADS_H
#define FREEIMAGE_THREADS_H

namespace FreeImage {

/**
A Mutex (mutual exclusion) is a synchronization mechanism used to control access 
to a shared resource in a concurrent (multithreaded) scenario.
Mutexes are recursive, that is, the same mutex can be locked multiple times by the same thread (but, of course, not by other threads).
*/
class Mutex {
private:
	void *_mutex;

public:
	//! Create the mutex
	Mutex();
	//! Destroy the mutex
	~Mutex();
	//! Locks the mutex. Blocks if the mutex is held by another thread.
	void lock();
	//! Unlocks the mutex so that it can be acquired by other threads.
	void unlock();
};

/**
A class that simplifies thread synchronization with a mutex.
The constructor accepts a Mutex and locks it.
The destructor unlocks the mutex.
*/
class ScopedMutex {
private:
	Mutex& _mutex;

public:
	ScopedMutex(Mutex& mutex) : _mutex(mutex) {
		_mutex.lock();
	}

	~ScopedMutex() {
		_mutex.unlock();
	}

private:
	ScopedMutex();
	ScopedMutex(const ScopedMutex&);
	ScopedMutex& operator = (const ScopedMutex&);
};

} // namespace FreeImage

#endif // FREEIMAGE_THREADS_H
