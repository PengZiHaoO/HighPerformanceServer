#ifndef __LOCKER_H__
#define __LOCKER_H__

/*
*
*   RALL Locker 
*
*/

#include <semaphore.h>

/* Semaphore Class */
class Semaphore {
public:
	Semaphore();
	~Semaphore();

	/* P */
	bool wait();

	/* V */
	bool post();

private:
	sem_t _sem;
};

/* Mutex Class */
class Locker {
public:
	Locker();
	~Locker();

	bool lock();
	bool unlock();

	pthread_mutex_t *get_mutex();

private:
	pthread_mutex_t _mutex;
};

/* Condition var class */
class Condition {
public:
	Condition();
	~Condition();

	bool wait();
	bool signal();

private:
	Locker		   _mutex;
	pthread_cond_t _cond;
};

#endif