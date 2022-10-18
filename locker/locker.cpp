#include "locker.h"
#include <exception>
#include <pthread.h>

Semaphore::Semaphore() {
	if (sem_init(&_sem, 0, 0) != 0) {
		throw std::exception();
	}
}

Semaphore::~Semaphore() {
	sem_destroy(&_sem);
}

/* P */
bool Semaphore::wait() {
	return sem_wait(&_sem) == 0;
}

/* V */
bool Semaphore::post() {
	return sem_post(&_sem) == 0;
}

Locker::Locker() {
	if (pthread_mutex_init(&_mutex, nullptr) != 0) {
		throw std::exception();
	}
}

Locker::~Locker() {
	pthread_mutex_destroy(&_mutex);
}

bool Locker::lock() {
	return pthread_mutex_lock(&_mutex) == 0;
}

bool Locker::unlock() {
	return pthread_mutex_unlock(&_mutex) == 0;
}

pthread_mutex_t *Locker::get_mutex() {
	return &_mutex;
}

Condition::Condition() {
	if (pthread_cond_init(&_cond, nullptr) != 0) {
		throw std::exception();
	}
}

Condition::~Condition() {
	pthread_cond_destroy(&_cond);
}

bool Condition::wait() {
	_mutex.lock();
	int ret = pthread_cond_wait(&_cond, _mutex.get_mutex());
	_mutex.unlock();

	return ret == 0;
}

bool Condition::signal() {
	return pthread_cond_signal(&_cond) == 0;
}
